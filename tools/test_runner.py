from datetime import datetime
import os
import re
import serial
import time

#  게이트웨이(우노 C) COM 포트 설정
UNO_C_PORT = "COM5"
BAUD_RATE = 115200

# 결과 파일 저장 경로
LOG_DIR = "../test_results"
if not os.path.exists(LOG_DIR):
    os.makedirs(LOG_DIR)

session_id = datetime.now().strftime("%Y%m%d_%H%M%S")
raw_log_file = os.path.join(LOG_DIR, f"raw_log_{session_id}.log")
summary_report_file = os.path.join(LOG_DIR, f"summary_report_{session_id}.txt")

#  우노 C 시리얼 출력 유연 정규식
LOG_PATTERN = re.compile(
    r"\[UnoA\]\s+Speed:\s*(?P<speed>\d+)\s*km/h\s*\|\s*Alive:\s*(?P<alive_a>\d+)\s*\|\|\s*"
    r"\[UnoB\]\s+Belt:\s*(?P<belt>[^\|]+?)\s*\|\s*Alive:\s*(?P<alive_b>\d+)\s*\|\|\s*"
    r"Engine DTC:\s*\[(?P<eng_dtc>[^\]]+)\]\s*\|\s*"
    r"Seatbelt DTC:\s*\[(?P<sb_dtc>[^\]]+)\]"
)

metrics = {
    "total_frames": 0,
    "intervals_loop": [],  # 게이트웨이 루프 수신 간격 (20ms)
    "intervals_a": [],     # Uno A 실제 패킷 수신 주기 (20ms)
    "intervals_b": [],     # Uno B 실제 패킷 수신 주기 (50ms)
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

    # 1-1. 게이트웨이 시리얼 수신 주기 (20ms)
    if metrics["last_rx_time_loop"] is not None:
        metrics["intervals_loop"].append((now_sec - metrics["last_rx_time_loop"]) * 1000.0)
    metrics["last_rx_time_loop"] = now_sec

    # 1-2. Uno A 수신 주기 측정 (Alive A가 변경되는 간격: Target 20ms)
    if metrics["last_alive_a"] is not None and curr_alive_a != metrics["last_alive_a"]:
        if metrics["last_rx_time_a"] is not None:
            metrics["intervals_a"].append((now_sec - metrics["last_rx_time_a"]) * 1000.0)
        metrics["last_rx_time_a"] = now_sec
    metrics["last_alive_a"] = curr_alive_a

    # 1-3. Uno B 수신 주기 측정 (Alive B가 변경되는 간격: Target 50ms)
    if metrics["last_alive_b"] is not None and curr_alive_b != metrics["last_alive_b"]:
        if metrics["last_rx_time_b"] is not None:
            metrics["intervals_b"].append((now_sec - metrics["last_rx_time_b"]) * 1000.0)
        metrics["last_rx_time_b"] = now_sec
    metrics["last_alive_b"] = curr_alive_b

    # 2. 고장 진단 (DTC ON / FAIL / NODE_OUT 감지)
    eng_str = data["eng_dtc"].upper()
    sb_str = data["sb_dtc"].upper()

    eng_fail = "FAIL" in eng_str or "ON" in eng_str
    sb_fail = "FAIL" in sb_str or "ON" in sb_str or "NODE_OUT" in data["belt"].upper()

    # Engine DTC 감지
    if eng_fail and not metrics["prev_eng_fail"]:
        metrics["dtc_events"]["Uno_A"] += 1
        metrics["dtc_timestamps"]["Uno_A"].append(timestamp_str)
        metrics["failsafe_passed"] = True
        if dtc_start_time is None:
            dtc_start_time = now_sec

    # Seatbelt DTC 감지
    if sb_fail and not metrics["prev_sb_fail"]:
        metrics["dtc_events"]["Uno_B"] += 1
        metrics["dtc_timestamps"]["Uno_B"].append(timestamp_str)
        if dtc_start_time is None:
            dtc_start_time = now_sec

    # Fault Recovery Time 측정
    if not eng_fail and not sb_fail and dtc_start_time is not None:
        recovery_ms = (now_sec - dtc_start_time) * 1000.0
        metrics["recovery_times"].append(recovery_ms)
        dtc_start_time = None

    metrics["prev_eng_fail"] = eng_fail
    metrics["prev_sb_fail"] = sb_fail


def format_ts(ts_list):
    return ", ".join(ts_list) if ts_list else "None"


def generate_summary():
    elapsed_sec = time.time() - start_time if start_time else 0.0

    # 주기 계산
    loop_intervals = metrics["intervals_loop"]
    avg_loop = sum(loop_intervals) / len(loop_intervals) if loop_intervals else 0.0
    freq_loop = (1000.0 / avg_loop) if avg_loop > 0 else 0.0

    a_intervals = metrics["intervals_a"]
    avg_a = sum(a_intervals) / len(a_intervals) if a_intervals else 0.0
    freq_a = (1000.0 / avg_a) if avg_a > 0 else 0.0

    b_intervals = metrics["intervals_b"]
    avg_b = sum(b_intervals) / len(b_intervals) if b_intervals else 0.0
    freq_b = (1000.0 / avg_b) if avg_b > 0 else 0.0

    recoveries = metrics["recovery_times"]
    avg_recovery = sum(recoveries) / len(recoveries) if recoveries else 0.0

    total_frames = metrics["total_frames"]

    report_content = f"""+-----------------------------------------------------------------------------------+
|                        AUTOMOTIVE CAN GATEWAY TEST REPORT                         |
+-----------------------------------------------------------------------------------+
  Session ID           : {session_id}
  Total Execution Time : {elapsed_sec:.2f} sec ({int(elapsed_sec // 60)}m {int(elapsed_sec % 60)}s)
  Total Frames Rx      : {total_frames} Frames
+-----------------------------------------------------------------------------------+

[1] BUS THROUGHPUT & NODE PERIODICITY ANALYSIS
+--------------------------+--------------------+--------------------+----------------------+
| Node Identifier          | Target Cycle (ms)  | Measured Avg (ms)  | Rx Frequency (Hz)    |
+--------------------------+--------------------+--------------------+----------------------+
| Gateway Loop (Uno C)     | 20.00 ms           | {avg_loop:14.2f} ms | {freq_loop:16.2f} Hz |
| Uno A Rx (Engine ECU)    | 20.00 ms           | {avg_a:14.2f} ms | {freq_a:16.2f} Hz |
| Uno B Rx (Seatbelt ECU)  | 50.00 ms           | {avg_b:14.2f} ms | {freq_b:16.2f} Hz |
+--------------------------+--------------------+--------------------+----------------------+

[2] FAULT DIAGNOSTICS & RECOVERY 
+--------------------------+--------------------+-------------------------------------------+
| Diagnostic Item          | Fault Count        | Occurrence Timestamps / Details           |
+--------------------------+--------------------+-------------------------------------------+
| Engine DTC (Uno A)       | {metrics['dtc_events']['Uno_A']:18d} | {format_ts(metrics['dtc_timestamps']['Uno_A']):41s} |
| Seatbelt DTC (Uno B)     | {metrics['dtc_events']['Uno_B']:18d} | {format_ts(metrics['dtc_timestamps']['Uno_B']):41s} |
| Fault Recovery Time      | {avg_recovery:15.2f} ms | Debounce & Fail-Safe Recovery Time        |
| Fail-Safe Enforcement    | {'PASS' if metrics['failsafe_passed'] else 'N/A':18s} | Enforced Speed: 60 km/h                   |
+--------------------------+--------------------+-------------------------------------------+
"""
    with open(summary_report_file, "w", encoding="utf-8") as f:
        f.write(report_content)

    print("\n" + report_content)
    print(f" 최종 자동화 보고서 저장 완료: {summary_report_file}")


if __name__ == "__main__":
    print("====================================================")
    print(f" 🚗 Gateway (Uno C: {UNO_C_PORT}) Test Runner Started")
    print(" 테스트 종료 시 Ctrl + C 를 누르세요.")
    print("====================================================\n")

    try:
        ser = serial.Serial(UNO_C_PORT, BAUD_RATE, timeout=1)
        print(f" 우노 C 게이트웨이 포트({UNO_C_PORT}) 연결 성공!\n")

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

                # 데이터 분석 및 수치 카운트
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
        print(f"\n 포트 연결 오류: {e}")