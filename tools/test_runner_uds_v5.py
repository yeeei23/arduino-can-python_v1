from datetime import datetime
import os
import re
import serial
import threading
import time
import statistics

# ==============================================================================
# 📌 1. 환경 설정 및 경로
# ==============================================================================
UNO_C_PORT = "COM3"  # 게이트웨이(Uno C) 실제 포트
BAUD_RATE = 115200

LOG_DIR = "../test_results"
if not os.path.exists(LOG_DIR):
    os.makedirs(LOG_DIR)

session_id = datetime.now().strftime("%Y%m%d_%H%M%S")
raw_log_file = os.path.join(LOG_DIR, f"raw_log_{session_id}.log")
summary_report_file = os.path.join(LOG_DIR, f"summary_report_{session_id}.txt")

# 👇 [추가] 파싱된 예쁜 로그를 저장할 파일과 색상 제거기
parsed_log_file = os.path.join(LOG_DIR, f"parsed_log_{session_id}.txt")
ANSI_ESCAPE = re.compile(r'\x1B(?:[@-Z\\-_]|\[[0-?]*[ -/]*[@-~])')
# ==============================================================================
# 📌 2. 정규식 패턴 (테이블 텔레메트리 & UDS 응답)
# ==============================================================================
LOG_PATTERN = re.compile(
    r"\|\s*(?P<speed>\d+)\s*km/h\s*\|\s*#(?P<alive_a>\d+)\s*\|\s*\[\s*(?P<comm_a>[^\]]+)\s*\]\s*\|\s*"
    r"\[\s*(?P<belt>[^\]]+)\s*\]\s*\|\s*#(?P<alive_b>\d+)\s*\|\s*\[\s*(?P<comm_b>[^\]]+)\s*\]\s*\|\s*"
    r"\[\s*(?P<dtc_a>[^\]]+)\s*\]\s*\|\s*\[\s*(?P<dtc_b>[^\]]+)\s*\]"
)

UDS_RESP_PATTERN = re.compile(r"<RESP,(?P<can_id>[0-9A-Fa-f]{3}),(?P<payload>[0-9A-Fa-f]{16})>")

# ==============================================================================
# 📌 3. 메트릭 데이터 구조 (1차 프로젝트 스타일)
# ==============================================================================
metrics = {
    "total_frames": 0,
    "intervals_loop_normal": [],  # 게이트웨이 루프 정상 주기 (Target 20ms)
    "intervals_a_normal": [],     # Uno A 정상 주기 (Target 20ms)
    "intervals_b_normal": [],     # Uno B 정상 주기 (Target 50ms)
    "dtc_events": {"Uno_A": 0, "Uno_B": 0},
    "dtc_timestamps": {"Uno_A": [], "Uno_B": []},
    "failsafe_a_passed": False,
    "failsafe_b_passed": False,
    "last_rx_time_loop": None,
    "last_rx_time_a": None,
    "last_rx_time_b": None,
    "last_alive_a": None,
    "last_alive_b": None,
    "prev_eng_fail": False,
    "prev_sb_fail": False,
    "uds_counts": {"0x22": 0, "0x19": 0, "0x14": 0},
    "boot_reset_count": 0,
}

start_time = None
is_running = True

# ==============================================================================
# 📌 4. 파싱 함수
# ==============================================================================
# 👇 [추가] 화면에도 띄우고 텍스트 파일에도 깔끔하게 저장하는 헬퍼 함수
def log_and_print(msg):
    print(msg)  # 화면엔 색상 그대로 출력
    clean_msg = ANSI_ESCAPE.sub('', msg)  # 텍스트 파일용으론 색상 코드만 쏙 제거
    with open(parsed_log_file, "a", encoding="utf-8") as f:
        f.write(clean_msg + "\n")


def parse_uds_line(payload_hex):
    try:
        sid_resp = int(payload_hex[2:4], 16)
        if sid_resp == 0x62:
            metrics["uds_counts"]["0x22"] += 1
            did = payload_hex[4:8]
            val = int(payload_hex[8:10], 16)
            return f"[UDS 0x22 RESP] DID: 0x{did} -> Value: {val} (0x{val:02X})"
        elif sid_resp == 0x59:
            metrics["uds_counts"]["0x19"] += 1
            dtc = payload_hex[6:10]
            spd = int(payload_hex[10:12], 16)
            belt = int(payload_hex[12:14], 16)
            if dtc == "0000":
                return "[UDS 0x19 RESP] No Stored DTC Records."
            return f"[UDS 0x19 RESP] DTC: 0x{dtc} | FreezeFrame Speed: {spd} km/h | Belt: {belt}"
        elif sid_resp == 0x54:
            metrics["uds_counts"]["0x14"] += 1
            return "[UDS 0x14 RESP] ClearDiagnosticInformation OK (0x54)"
    except Exception:
        pass
    return None


def parse_uno_c_line(line, now_sec, timestamp_str):
    # 1. 보드 리셋 및 EEPROM 감지
    if any(k in line for k in ["[SYSTEM_RESET]", "Booted", "EEPROM DTC History", "Gateway DTCs Cleared"]):
        metrics["boot_reset_count"] += 1
        return f"🚨 \033[93m[SYSTEM REBOOT / EEPROM DETECTED] {line}\033[0m"

    # 2. UDS 진단 통신 응답 감지
    uds_match = UDS_RESP_PATTERN.search(line)
    if uds_match:
        return parse_uds_line(uds_match.group("payload"))

    # 3. 실시간 텔레메트리 파싱 (CSV 분리)
    if line.startswith("$DATA,"):
        metrics["total_frames"] += 1
        try:
            parts = line.split(",")
            # 규격: $DATA, Time, Speed, Delta_A, Belt, Delta_B, NodeA_Err, NodeB_Err
            gw_time = int(parts[1])
            speed = int(parts[2])
            delta_a = int(parts[3])
            belt_val = int(parts[4])
            delta_b = int(parts[5])
            
            eng_fail = (parts[6] == "1")
            sb_fail = (parts[7] == "1")
            
            belt_str = "BUCKLED" if belt_val == 1 else "UNBUCKLED"
            is_normal_state = (not eng_fail) and (not sb_fail)

            # --- 3-1. 게이트웨이 루프 주기 (파이썬 UART 수신 기준 Jitter) ---
            if metrics["last_rx_time_loop"] is not None:
                dt_loop = (now_sec - metrics["last_rx_time_loop"]) * 1000.0
                if is_normal_state and dt_loop < 100.0:
                    metrics["intervals_loop_normal"].append(dt_loop)
            metrics["last_rx_time_loop"] = now_sec

            # --- 3-2. 통계 및 지터 수집 (아두이노 측정 순수 CAN Delta 반영) ---
            if is_normal_state:
                if 0 < delta_a < 100:  # 노이즈/단선 스파이크 필터링 (Target 20ms)
                    metrics["intervals_a_normal"].append(delta_a)
                if 0 < delta_b < 200:  # Target 100ms
                    metrics["intervals_b_normal"].append(delta_b)

            # --- 3-3. 결함 감지 및 Fail-Safe 검증 ---
            if eng_fail:
                if not metrics["prev_eng_fail"]:
                    metrics["dtc_events"]["Uno_A"] += 1
                    metrics["dtc_timestamps"]["Uno_A"].append(timestamp_str)
                if speed == 60:
                    metrics["failsafe_a_passed"] = True

            if sb_fail:
                if not metrics["prev_sb_fail"]:
                    metrics["dtc_events"]["Uno_B"] += 1
                    metrics["dtc_timestamps"]["Uno_B"].append(timestamp_str)
                if belt_val == 0: # 0이 UNBUCKLED
                    metrics["failsafe_b_passed"] = True

            metrics["prev_eng_fail"] = eng_fail
            metrics["prev_sb_fail"] = sb_fail

            # --- 3-4. 콘솔 출력용 UI 렌더링 ---
            if eng_fail or sb_fail:
                return f"🚨 \033[91m[FAULT ] Speed: {speed:3d} km/h (dt:{delta_a:3d}ms) | Belt: {belt_str:9s} (dt:{delta_b:3d}ms) | Err: A={int(eng_fail)}, B={int(sb_fail)}\033[0m"
            else:
                return f"   \033[92m[NORMAL] Speed: {speed:3d} km/h (dt:{delta_a:3d}ms) | Belt: {belt_str:9s} (dt:{delta_b:3d}ms) | Err: A=0, B=0\033[0m"
                
        except Exception as e:
            pass # 파싱 실패 시 무시

    return line # 정형화되지 않은 일반 프린트문은 그대로 반환

# ==============================================================================
# 📌 5. 통계 계산 및 요약 리포트 생성
# ==============================================================================
def calc_jitter_stats(intervals):
    if not intervals:
        return 0.0, 0.0, 0.0, 0.0
    
    avg_val = statistics.mean(intervals)
    min_val = min(intervals)
    max_val = max(intervals)
    std_val = statistics.stdev(intervals) if len(intervals) > 1 else 0.0
    
    return avg_val, min_val, max_val, std_val

def generate_summary():
    elapsed_sec = time.time() - start_time if start_time else 0.0

    avg_loop, min_loop, max_loop, std_loop = calc_jitter_stats(metrics["intervals_loop_normal"])
    avg_a, min_a, max_a, std_a = calc_jitter_stats(metrics["intervals_a_normal"])
    avg_b, min_b, max_b, std_b = calc_jitter_stats(metrics["intervals_b_normal"])

    range_loop = f"{min_loop:.1f} / {max_loop:.1f}"
    range_a = f"{min_a:.1f} / {max_a:.1f}"
    range_b = f"{min_b:.1f} / {max_b:.1f}"

    report_content = f"""+-------------------------------------------------------------------------------------------------------+
|                                  AUTOMOTIVE CAN GATEWAY TEST REPORT                                   |
+-------------------------------------------------------------------------------------------------------+
  Session ID           : {session_id}
  Total Execution Time : {elapsed_sec:.2f} sec ({int(elapsed_sec // 60)}m {int(elapsed_sec % 60)}s)
  Total Frames Rx      : {metrics['total_frames']} Frames
+-------------------------------------------------------------------------------------------------------+

[1] NODE PERIODICITY & TIMING ANALYSIS
+--------------------------+------------------+-------------------+--------------------+--------------------+
| Node Identifier          | Target Cycle(ms) | Measured Avg Cycle| Min / Max Cycle    | Jitter (StdDev)    |
+--------------------------+------------------+-------------------+--------------------+--------------------+
| Gateway Loop (Uno C)     | 10.00 ms         | {avg_loop:14.2f} ms | {range_loop:>18s} ms | ±{std_loop:15.2f} ms |
| Uno A Rx (Engine ECU)    | 20.00 ms         | {avg_a:14.2f} ms | {range_a:>18s} ms | ±{std_a:15.2f} ms |
| Uno B Rx (Seatbelt ECU)  | 100.00 ms         | {avg_b:14.2f} ms | {range_b:>18s} ms | ±{std_b:15.2f} ms |
+--------------------------+------------------+-------------------+--------------------+--------------------+

[2] FAULT DIAGNOSTICS & FAIL-SAFE EVALUATION
+--------------------------+--------------------+---------------------------------------------------------------+
| Diagnostic Item          | Fault Count        | Status & Occurrence Timestamps                                |
+--------------------------+--------------------+---------------------------------------------------------------+
| Engine DTC (Uno A)       | {metrics['dtc_events']['Uno_A']:18d} | {", ".join(metrics['dtc_timestamps']['Uno_A']) if metrics['dtc_timestamps']['Uno_A'] else 'None'}
| Uno A Fail-Safe Speed    | {('PASS' if metrics['failsafe_a_passed'] else 'N/A'):>18s} | Enforced Default Speed: 60 km/h (Verified)                    |
| Seatbelt DTC (Uno B)     | {metrics['dtc_events']['Uno_B']:18d} | {", ".join(metrics['dtc_timestamps']['Uno_B']) if metrics['dtc_timestamps']['Uno_B'] else 'None'}
| Uno B Fail-Safe State    | {('PASS' if metrics['failsafe_b_passed'] else 'N/A'):>18s} | Enforced State: UNBUCKLED / Safe Default (Verified)           |
+--------------------------+--------------------+---------------------------------------------------------------+

[3] UDS (ISO 14229) DIAGNOSTIC SERVICES
+---------------------------------------------+---------------+-----------------------------------------+
| UDS Service Name                            | Verified Count| Protocol Status                         |
+---------------------------------------------+---------------+-----------------------------------------+
| SID 0x22 (ReadDataByIdentifier)             | {metrics['uds_counts']['0x22']:13d} | Positive Response (0x62) Received       |
| SID 0x19 (ReadDTCInformation / FreezeFrame) | {metrics['uds_counts']['0x19']:13d} | Positive Response (0x59) Received       |
| SID 0x14 (ClearDiagnosticInformation)       | {metrics['uds_counts']['0x14']:13d} | Positive Response (0x54) Received       |
+---------------------------------------------+---------------+-----------------------------------------+
"""
    with open(summary_report_file, "w", encoding="utf-8") as f:
        f.write(report_content)

    print("\n" + report_content)
    print(f"✅ 최종 자동화 보고서 저장 완료: {summary_report_file}")

# ==============================================================================
# 📌 6. 대화형 키보드 입력 스레드
# ==============================================================================
def keyboard_input_thread(ser):
    global is_running
    log_and_print("\n[명령어 안내]")
    log_and_print("  '1' : UDS 0x22 (속도 DID 조회)")
    log_and_print("  '2' : UDS 0x22 (안전벨트 DID 조회)")
    log_and_print("  '3' : UDS 0x19 (DTC & FreezeFrame 조회)")
    log_and_print("  '4' : UDS 0x14 (DTC 전체 삭제)")
    log_and_print("  'r' : EEPROM 전체 출력  |  'c' : EEPROM 전체 초기화\n")

    while is_running:
        try:
            cmd = input().strip()
            if not cmd:
                continue

            
            if cmd == '1':
                ser.write(b">REQ,7EA,0322010000000000\n")
                log_and_print(f"[{datetime.now().strftime('%H:%M:%S.%f')[:-3]}] 📤 [UDS TX] SID 0x22 (Speed DID) 송신")
            elif cmd == '2':
                ser.write(b">REQ,7EA,0322010100000000\n")
                log_and_print(f"[{datetime.now().strftime('%H:%M:%S.%f')[:-3]}] 📤 [UDS TX] SID 0x22 (Belt DID) 송신")
            elif cmd == '3':
                ser.write(b">REQ,7EA,0219020000000000\n")
                log_and_print(f"[{datetime.now().strftime('%H:%M:%S.%f')[:-3]}] 📤 [UDS TX] SID 0x19 (Read DTC) 송신")
            elif cmd == '4':
                ser.write(b">REQ,7EA,0114000000000000\n")
                log_and_print(f"[{datetime.now().strftime('%H:%M:%S.%f')[:-3]}] 📤 [UDS TX] SID 0x14 (Clear DTC) 송신")
            elif cmd.lower() == 'r':
                ser.write(b"r\n")
                log_and_print(f"[{datetime.now().strftime('%H:%M:%S.%f')[:-3]}] 📤 [CLI TX] 'r' (Print Stored DTCs) 송신")
            elif cmd.lower() == 'c':
                ser.write(b"c\n")
                log_and_print(f"[{datetime.now().strftime('%H:%M:%S.%f')[:-3]}] 📤 [CLI TX] 'c' (Clear DTCs) 송신")

        except Exception:
            break

# ==============================================================================
# 📌 7. 메인 실행 루프
# ==============================================================================
if __name__ == "__main__":
    log_and_print("====================================================")
    log_and_print(f" 🚗 Gateway (Uno C: {UNO_C_PORT}) Test Runner Started")
    log_and_print(" 테스트 종료 시 Ctrl + C 를 누르세요.")
    log_and_print("====================================================\n")

    try:
        ser = serial.Serial(UNO_C_PORT, BAUD_RATE, timeout=1)
        log_and_print(f"✅ 우노 C 게이트웨이 포트({UNO_C_PORT}) 연결 성공!\n")

        log_and_print("⏳ 부팅 헤더 동기화 대기 중...")

        # 부팅 시 출력되는 테이블 구분선(---)이 나올 때까지 앞의 깨진 로그는 전부 버림
        while True:
            if ser.in_waiting > 0:
                init_line = ser.readline().decode("utf-8", errors="ignore").strip()
                if "------" in init_line or "[ Uno C" in init_line:
                    break  # 게이트웨이 부팅 헤더 감지 완료!

        log_and_print("🚀 게이트웨이 동기화 완료! \n")

        log_and_print("🚀 게이트웨이 동기화 완료! \n")

        start_time = time.time()

        t = threading.Thread(target=keyboard_input_thread, args=(ser,), daemon=True)
        t.start()

        while True:
            if ser.in_waiting > 0:
                line = ser.readline().decode("utf-8", errors="ignore").strip()
                if not line:
                    continue

                now_sec = time.time()
                ts_str = datetime.now().strftime("%H:%M:%S.%f")[:-3]
                log_entry = f"[{ts_str}] {line}"
# 1. 원본 수신 데이터(raw) 파일 저장
                with open(raw_log_file, "a", encoding="utf-8") as f:
                    f.write(log_entry + "\n")

                uds_evt = parse_uno_c_line(line, now_sec, ts_str)
                # 2. 파싱 및 가공된 로그 화면 출력 + parsed_log_file 저장
                if uds_evt:
                    log_and_print(f"🔧 \033[96m{uds_evt}\033[0m")
                elif "FAIL" in line or "NODE_OUT" in line:
                    log_and_print(f"🚨 \033[91m{log_entry}\033[0m")
                elif "NORMAL(OK)" in line:
                    log_and_print(f"   \033[92m{log_entry}\033[0m")
                else:
                    log_and_print(log_entry)

    except KeyboardInterrupt:
        is_running = False
        log_and_print("\n테스트 종료 중... 최종 요약 보고서를 생성합니다.")
        generate_summary()
    except Exception as e:
        log_and_print(f"\n❌ 포트 연결 오류: {e}")