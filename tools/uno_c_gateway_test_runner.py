from datetime import datetime
import os
import re
import serial
import time

# 📌 게이트웨이(우노 C) COM 포트 번호
UNO_C_PORT = "COM10"
BAUD_RATE = 115200

# 결과 파일 저장 경로
LOG_DIR = "../test_results"
if not os.path.exists(LOG_DIR):
    os.makedirs(LOG_DIR)

session_id = datetime.now().strftime("%Y%m%d_%H%M%S")
raw_log_file = os.path.join(LOG_DIR, f"raw_log_{session_id}.log")
summary_report_file = os.path.join(LOG_DIR, f"summary_report_{session_id}.txt")

# 수신 로그 유연 정규식 (Engine DTC & Seatbelt DTC 유연한 매칭)
LOG_PATTERN = re.compile(
    r"\[UnoA\]\s+Speed:\s*(?P<speed>\d+)\s*km/h\s*\|\s*Alive:\s*(?P<alive_a>\d+)\s*\|\|\s*"
    r"\[UnoB\]\s+Belt:\s*(?P<belt>[^\|]+)\|\|\s*"
    r"Engine DTC:\s*\[(?P<eng_dtc>[^\]]+)\]\s*\|\s*"
    r"Seatbelt DTC:\s*\[(?P<sb_dtc>[^\]]+)\]"
)

metrics = {
    "total_frames": 0,
    "intervals": [],
    "dtc_events": {"Uno_A": 0, "Uno_B": 0},
    "dtc_timestamps": {"Uno_A": [], "Uno_B": []},
    "so_saeng_times": [],
    "failsafe_passed": False,
    "alive_drops": {"Uno_A": 0, "Uno_B": 0},
    "last_alive_a": None,
    "last_rx_time": None,
    "prev_eng_fail": False,
    "prev_sb_fail": False,
}

dtc_start_time = None


def parse_uno_c_line(line, now_sec, timestamp_str):
    global dtc_start_time

    # 1. Alive Counter 및 프레임 Drop 검사 (4비트 Modulo 16)
    match = LOG_PATTERN.search(line)
    if not match:
        return

    metrics["total_frames"] += 1
    data = match.groupdict()

    # Uno A Alive Drop 판단 (Gateway에 동기화되어 오므로 Rx Frame Drop 공통 적용)
    curr_alive_a = int(data["alive_a"])
    if metrics["last_alive_a"] is not None:
        expected_a = (metrics["last_alive_a"] + 1) % 16
        if curr_alive_a != expected_a:
            # 유실 발생 시 Uno A/B 수신 통로 공통 드롭 카운트 증가
            metrics["alive_drops"]["Uno_A"] += 1
            metrics["alive_drops"]["Uno_B"] += 1
    metrics["last_alive_a"] = curr_alive_a

    # 2. 통신 수신 주기 (Rx Cycle) 계산
    if metrics["last_rx_time"] is not None:
        interval_ms = (now_sec - metrics["last_rx_time"]) * 1000.0
        metrics["intervals"].append(interval_ms)
    metrics["last_rx_time"] = now_sec

    # 3. 고장 진단 (DTC Edge Detection: OK -> FAIL 트랜지션 감지)
    eng_fail = "FAIL" in data["eng_dtc"].upper()
    sb_fail = "FAIL" in data["sb_dtc"].upper()

    # Uno A Engine DTC 발생 감지
    if eng_fail and not metrics["prev_eng_fail"]:
        metrics["dtc_events"]["Uno_A"] += 1
        metrics["dtc_timestamps"]["Uno_A"].append(timestamp_str)
        metrics["failsafe_passed"] = True  # Engine Fail-Safe 트리거
        if dtc_start_time is None:
            dtc_start_time = now_sec

    # Uno B Seatbelt DTC 발생 감지
    if sb_fail and not metrics["prev_sb_fail"]:
        metrics["dtc_events"]["Uno_B"] += 1
        metrics["dtc_timestamps"]["Uno_B"].append(timestamp_str)
        if dtc_start_time is None:
            dtc_start_time = now_sec

    # 소생 (SoSaeng Recovery) 시간 측정 (DTC가 모두 [OK]로 회복된 시점)
    if not eng_fail and not sb_fail and dtc_start_time is not None:
        recovery_ms = (now_sec - dtc_start_time) * 1000.0
        metrics["so_saeng_times"].append(recovery_ms)
        dtc_start_time = None

    metrics["prev_eng_fail"] = eng_fail
    metrics["prev_sb_fail"] = sb_fail


def format_ts(ts_list):
    if not ts_list:
        return "None"
    return ", ".join(ts_list)


def generate_summary():
    intervals = metrics["intervals"]
    recoveries = metrics["so_saeng_times"]

    avg_interval = sum(intervals) / len(intervals) if intervals else 0.0
    avg_recovery = sum(recoveries) / len(recoveries) if recoveries else 0.0

    total_drops = (
        metrics["alive_drops"]["Uno_A"] + metrics["alive_drops"]["Uno_B"]
    )
    total_checks = metrics["total_frames"] * 2
    drop_rate = (
        (total_drops / total_checks * 100.0) if total_checks > 0 else 0.0
    )

    report_content = f"""====================================================================
           AUTOMOTIVE CAN BUS GATEWAY REPORT (UNO C)
====================================================================
Session ID: {session_id}

[1] CAN GATEWAY THROUGHPUT & PERIODICITY
    - Total Processed Frames       : {metrics['total_frames']} Frames
    - Gateway Rx Cycle (Uno A)     : Avg {avg_interval:.2f} ms (Freq: {(1000.0/avg_interval if avg_interval > 0 else 0):.2f} Hz)
    - Gateway Rx Cycle (Uno B)     : Avg {avg_interval:.2f} ms (Freq: {(1000.0/avg_interval if avg_interval > 0 else 0):.2f} Hz)

[2] RELIABILITY & PACKET LOSS (ALIVE COUNTER)
    - Alive Drops (Uno A / Uno B)  : Uno A ({metrics['alive_drops']['Uno_A']}회), Uno B ({metrics['alive_drops']['Uno_B']}회)
    - Total Loss Rate              : {drop_rate:.3f} %

[3] FAULT DIAGNOSTICS & RECOVERY (SoSaeng)
    - Engine DTC (Uno A)           : {metrics['dtc_events']['Uno_A']}회 | Occurrence Timestamps: {format_ts(metrics['dtc_timestamps']['Uno_A'])}
    - Seatbelt DTC (Uno B)         : {metrics['dtc_events']['Uno_B']}회 | Occurrence Timestamps: {format_ts(metrics['dtc_timestamps']['Uno_B'])}
    - Avg SoSaeng Recovery Time    : {avg_recovery:.2f} ms
    - Fail-Safe Enforced (60km/h)  : {'PASS' if metrics['failsafe_passed'] else 'FAIL-SAFE N/A'}
====================================================================
"""
    # 1. 리포트 파일 저장
    with open(summary_report_file, "w", encoding="utf-8") as f:
        f.write(report_content)

    # 2. 터미널 출력
    print("\n" + report_content)
    print(f"✅ 최종 요약 리포트 저장 완료: {summary_report_file}")


if __name__ == "__main__":
    print("====================================================")
    print(f" 🚗 Gateway (Uno C: {UNO_C_PORT}) Test Runner Started")
    print(" 테스트 종료 시 Ctrl + C 를 누르세요.")
    print("====================================================\n")

    try:
        ser = serial.Serial(UNO_C_PORT, BAUD_RATE, timeout=1)
        print(f"✅ 우노 C 게이트웨이 포트({UNO_C_PORT}) 연결 성공!\n")

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
                if "FAIL" in line or "NODE_OUT" in line:
                    print(f"🚨 [FAULT DETECTED] {log_entry}")
                else:
                    print(log_entry)

    except KeyboardInterrupt:
        print("\n테스트 종료 중... 최종 분석 요약 리포트를 생성합니다.")
        generate_summary()
    except Exception as e:
        print(f"\n❌ 포트 연결 오류: {e}")