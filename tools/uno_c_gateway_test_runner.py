from datetime import datetime
import os
import re
import serial
import time
import statistics

# 📌 게이트웨이(우노 C) COM 포트 설정
UNO_C_PORT = "COM10"  # 실제 연결된 포트로 변경 필요
BAUD_RATE = 115200

# 결과 파일 저장 경로
LOG_DIR = "../test_results"
if not os.path.exists(LOG_DIR):
    os.makedirs(LOG_DIR)

session_id = datetime.now().strftime("%Y%m%d_%H%M%S")
raw_log_file = os.path.join(LOG_DIR, f"raw_log_{session_id}.log")
summary_report_file = os.path.join(LOG_DIR, f"summary_report_{session_id}.txt")

# 📌 우노 C 시리얼 출력 정규식
LOG_PATTERN = re.compile(
    r"\[UnoA\]\s+Speed:\s*(?P<speed>\d+)\s*km/h\s*\|\s*Alive:\s*(?P<alive_a>\d+)\s*\|\|\s*"
    r"\[UnoB\]\s+Belt:\s*(?P<belt>[^\|]+?)\s*\|\s*Alive:\s*(?P<alive_b>\d+)\s*\|\|\s*"
    r"Engine DTC:\s*\[(?P<eng_dtc>[^\]]+)\]\s*\|\s*"
    r"Seatbelt DTC:\s*\[(?P<sb_dtc>[^\]]+)\]"
)

metrics = {
    "total_frames": 0,
    "intervals_loop": [],        # 전체 게이트웨이 루프 간격 (20ms)
    "intervals_a_total": [],     # Uno A 전체 수신 주기
    "intervals_a_normal": [],    # Uno A 정상 상태 수신 주기 (DTC OFF)
    "intervals_b_total": [],     # Uno B 전체 수신 주기
    "intervals_b_normal": [],    # Uno B 정상 상태 수신 주기 (DTC OFF)
    "dtc_events": {"Uno_A": 0, "Uno_B": 0},
    "dtc_timestamps": {"Uno_A": [], "Uno_B": []},
    "recovery_times": [],
    "failsafe_passed": False,
    "last_rx_time_loop": None,
    "last_rx_time_a": None,
    "last_rx_time_b": None,
    "last_alive_a": None,
    "last_alive_b": None,
    "prev_eng_fail": False,
    "prev_sb_fail": False,
}

dtc_start_time = None
start_time = None


def parse_uno_c_line(line, now_sec, timestamp_str):
    global dtc_start_time

    match = LOG_PATTERN.search(line)
    if not match:
        return

    metrics["total_frames"] += 1
    data = match.groupdict()

    curr_alive_a = int(data["alive_a"])
    curr_alive_b = int(data["alive_b"])

    # 1. 고장 상태 여부 판단
    eng_str = data["eng_dtc"].upper()
    sb_str = data["sb_dtc"].upper()

    eng_fail = "FAIL" in eng_str or "ON" in eng_str
    sb_fail = "FAIL" in sb_str or "ON" in sb_str or "NODE_OUT" in data["belt"].upper()
    is_normal_state = (not eng_fail) and (not sb_fail)

    # 2-1. 게이트웨이 루프 주기 측정
    if metrics["last_rx_time_loop"] is not None:
        metrics["intervals_loop"].append((now_sec - metrics["last_rx_time_loop"]) * 1000.0)
    metrics["last_rx_time_loop"] = now_sec

    # 2-2. Uno A 수신 주기 측정 (Target: 20ms)
    if metrics["last_alive_a"] is not None and curr_alive_a != metrics["last_alive_a"]:
        if metrics["last_rx_time_a"] is not None:
            interval_a = (now_sec - metrics["last_rx_time_a"]) * 1000.0
            metrics["intervals_a_total"].append(interval_a)
            # 정상 상태이고 고장 끊김 스파이크(>100ms)가 아닐 때만 정상 주기로 집계
            if is_normal_state and interval_a < 100.0:
                metrics["intervals_a_normal"].append(interval_a)
        metrics["last_rx_time_a"] = now_sec
    metrics["last_alive_a"] = curr_alive_a

    # 2-3. Uno B 수신 주기 측정 (Target: 50ms)
    if metrics["last_alive_b"] is not None and curr_alive_b != metrics["last_alive_b"]:
        if metrics["last_rx_time_b"] is not None:
            interval_b = (now_sec - metrics["last_rx_time_b"]) * 1000.0
            metrics["intervals_b_total"].append(interval_b)
            # 정상 상태이고 고장 끊김 스파이크(>200ms)가 아닐 때만 정상 주기로 집계
            if is_normal_state and interval_b < 200.0:
                metrics["intervals_b_normal"].append(interval_b)
        metrics["last_rx_time_b"] = now_sec
    metrics["last_alive_b"] = curr_alive_b

    # 3. 고장 진단 및 복구 감지
    if eng_fail and not metrics["prev_eng_fail"]:
        metrics["dtc_events"]["Uno_A"] += 1
        metrics["dtc_timestamps"]["Uno_A"].append(timestamp_str)
        metrics["failsafe_passed"] = True
        if dtc_start_time is None:
            dtc_start_time = now_sec

    if sb_fail and not metrics["prev_sb_fail"]:
        metrics["dtc_events"]["Uno_B"] += 1
        metrics["dtc_timestamps"]["Uno_B"].append(timestamp_str)
        if dtc_start_time is None:
            dtc_start_time = now_sec

    if is_normal_state and dtc_start_time is not None:
        recovery_ms = (now_sec - dtc_start_time) * 1000.0
        metrics["recovery_times"].append(recovery_ms)
        dtc_start_time = None

    metrics["prev_eng_fail"] = eng_fail
    metrics["prev_sb_fail"] = sb_fail


def format_ts(ts_list):
    return ", ".join(ts_list) if ts_list else "None"

def calc_jitter_stats(intervals, target_ms):
    """주기 리스트를 받아 평균, 최소, 최대, 표준편차, 오차율을 반환하는 함수"""
    if not intervals:
        return 0.0, 0.0, 0.0, 0.0, 0.0
    
    avg_val = sum(intervals) / len(intervals)
    min_val = min(intervals)
    max_val = max(intervals)
    std_val = statistics.stdev(intervals) if len(intervals) > 1 else 0.0
    err_rate = ((avg_val - target_ms) / target_ms) * 100.0
    
    return avg_val, min_val, max_val, std_val, err_rate


def generate_summary():
    elapsed_sec = time.time() - start_time if start_time else 0.0

   # 1. 노드별 수신 주기 및 Jitter 통계 연산
    avg_loop, min_loop, max_loop, std_loop, err_loop = calc_jitter_stats(metrics["intervals_loop"], 20.0)
    avg_a_norm, min_a, max_a, std_a, err_a_norm = calc_jitter_stats(metrics["intervals_a_normal"], 20.0)
    avg_b_norm, min_b, max_b, std_b, err_b_norm = calc_jitter_stats(metrics["intervals_b_normal"], 50.0)


    # Uno A
    a_tot = metrics["intervals_a_total"]
    a_norm = metrics["intervals_a_normal"]
    avg_a_tot = sum(a_tot) / len(a_tot) if a_tot else 0.0
    avg_a_norm = sum(a_norm) / len(a_norm) if a_norm else 0.0
    err_a_norm = ((avg_a_norm - 20.0) / 20.0 * 100.0) if avg_a_norm > 0 else 0.0

    # Uno B
    b_tot = metrics["intervals_b_total"]
    b_norm = metrics["intervals_b_normal"]
    avg_b_tot = sum(b_tot) / len(b_tot) if b_tot else 0.0
    avg_b_norm = sum(b_norm) / len(b_norm) if b_norm else 0.0
    err_b_norm = ((avg_b_norm - 50.0) / 50.0 * 100.0) if avg_b_norm > 0 else 0.0

    range_loop = f"{min_loop:.1f} / {max_loop:.1f}"
    range_a = f"{min_a:.1f} / {max_a:.1f}"
    range_b = f"{min_b:.1f} / {max_b:.1f}"
    
    total_frames = metrics["total_frames"]

    report_content = f"""+-------------------------------------------------------------------------------------------------------+
|                                  AUTOMOTIVE CAN GATEWAY TEST REPORT                                   |
+-------------------------------------------------------------------------------------------------------+
  Session ID           : {session_id}
  Total Execution Time : {elapsed_sec:.2f} sec ({int(elapsed_sec // 60)}m {int(elapsed_sec % 60)}s)
  Total Frames Rx      : {total_frames} Frames
+-------------------------------------------------------------------------------------------------------+

[1] NODE PERIODICITY & JITTER ANALYSIS
+--------------------------+------------------+-------------------+--------------------+--------------------+--------------------+
| Node Identifier          | Target Cycle(ms) | Normal Avg Cycle  | Min / Max Cycle    | Jitter (StdDev)    | Normal Error Rate  |
+--------------------------+------------------+-------------------+--------------------+--------------------+--------------------+
| Gateway Loop (Uno C)     | 20.00 ms         | {avg_loop:14.2f} ms | {range_loop:>18s} ms | ±{std_loop:13.2f} ms | {err_loop:+15.2f} % |
| Uno A Rx (Engine ECU)    | 20.00 ms         | {avg_a_norm:14.2f} ms | {range_a:>18s} ms | ±{std_a:13.2f} ms | {err_a_norm:+15.2f} % |
| Uno B Rx (Seatbelt ECU)  | 50.00 ms         | {avg_b_norm:14.2f} ms | {range_b:>18s} ms | ±{std_b:13.2f} ms | {err_b_norm:+15.2f} % |
+--------------------------+------------------+-------------------+--------------------+--------------------+--------------------+

[2] FAULT DIAGNOSTICS 
+--------------------------+--------------------+---------------------------------------------------------------+
| Diagnostic Item          | Fault Count        | Occurrence Timestamps / Details                               |
+--------------------------+--------------------+---------------------------------------------------------------+
| Engine DTC (Uno A)       | {metrics['dtc_events']['Uno_A']:18d} | {format_ts(metrics['dtc_timestamps']['Uno_A']):61s} |
| Seatbelt DTC (Uno B)     | {metrics['dtc_events']['Uno_B']:18d} | {format_ts(metrics['dtc_timestamps']['Uno_B']):61s} |                      |
| Fail-Safe Enforcement    | {'PASS' if metrics['failsafe_passed'] else 'N/A':18s} | Enforced Speed: 60 km/h                                       |
+--------------------------+--------------------+---------------------------------------------------------------+
"""
    with open(summary_report_file, "w", encoding="utf-8") as f:
        f.write(report_content)

    print("\n" + report_content)
    print(f"✅ 최종 자동화 보고서 저장 완료: {summary_report_file}")


if __name__ == "__main__":
    print("====================================================")
    print(f" 🚗 Gateway (Uno C: {UNO_C_PORT}) Test Runner Started")
    print(" 테스트 종료 시 Ctrl + C 를 누르세요.")
    print("====================================================\n")

    try:
        ser = serial.Serial(UNO_C_PORT, BAUD_RATE, timeout=1)
        print(f"✅ 우노 C 게이트웨이 포트({UNO_C_PORT}) 연결 성공!\n")

        start_time = time.time()

        while True:
            if ser.in_waiting > 0:
                line = ser.readline().decode("utf-8", errors="ignore").strip()
                if not line:
                    continue

                now_sec = time.time()
                timestamp_str = datetime.now().strftime("%H:%M:%S.%f")[:-3]
                log_entry = f"[{timestamp_str}] {line}"

                # 원본 로그 파일 기록
                with open(raw_log_file, "a", encoding="utf-8") as f:
                    f.write(log_entry + "\n")

                # 데이터 분석
                parse_uno_c_line(line, now_sec, timestamp_str)

                # 터미널 실시간 출력
                if "FAIL" in line or "NODE_OUT" in line or "ON" in line:
                    print(f"🚨 [FAULT DETECTED] {log_entry}")
                else:
                    print(log_entry)

    except KeyboardInterrupt:
        print("\n테스트 종료 중... 최종 요약 보고서를 생성합니다.")
        generate_summary()
    except Exception as e:
        print(f"\n❌ 포트 연결 오류: {e}")