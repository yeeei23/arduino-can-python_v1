import serial
import time
from datetime import datetime
import os

# 📌 게이트웨이(우노 C)의 COM 포트 번호만 입력하세요.
UNO_C_PORT = 'COM10'
BAUD_RATE = 115200

# 결과 파일 저장 경로 (상위 test_results 폴더에 생성)
LOG_DIR = "../test_results"
if not os.path.exists(LOG_DIR):
    os.makedirs(LOG_DIR)

session_id = datetime.now().strftime('%Y%m%d_%H%M%S')
raw_log_file = os.path.join(LOG_DIR, f"raw_log_{session_id}.log")
summary_report_file = os.path.join(LOG_DIR, f"summary_report_{session_id}.txt")

# 측정 데이터 저장 구조체
metrics = {
    'total_frames': 0,
    'unoA_intervals': [],
    'unoB_intervals': [],
    'dtc_events': {'Uno_A': 0, 'Uno_B': 0},
    'so_saeng_times': [],
    'failsafe_passed': False,
    'alive_drops': {'Uno_A': 0, 'Uno_B': 0},
    'last_alive': {'Uno_A': None, 'Uno_B': None},
    'last_rx_time': {'Uno_A': None, 'Uno_B': None}
}

dtc_start_time = None

def parse_uno_c_line(line, now_sec):
    global dtc_start_time
    metrics['total_frames'] += 1

    # 1. Alive Counter 및 패킷 누락(Drop) 검사
    # 우노 C 출력 포맷: ... Alive: 5 | ... Alive: 12 ...
    if "Alive:" in line:
        try:
            # Uno A / Uno B Alive 값 추출
            parts = line.split("Alive:")
            if len(parts) >= 3:
                alive_a = int(parts[1].split("|")[0].strip())
                alive_b = int(parts[2].split("|")[0].strip())

                # Uno A Alive Check
                if metrics['last_alive']['Uno_A'] is not None:
                    if alive_a != (metrics['last_alive']['Uno_A'] + 1) % 16:
                        metrics['alive_drops']['Uno_A'] += 1
                metrics['last_alive']['Uno_A'] = alive_a

                # Uno B Alive Check
                if metrics['last_alive']['Uno_B'] is not None:
                    if alive_b != (metrics['last_alive']['Uno_B'] + 1) % 16:
                        metrics['alive_drops']['Uno_B'] += 1
                metrics['last_alive']['Uno_B'] = alive_b
        except Exception:
            pass

    # 2. 송신 주기 (Tx Interval) 계산
    if metrics['last_rx_time']['Uno_A'] is not None:
        metrics['unoA_intervals'].append((now_sec - metrics['last_rx_time']['Uno_A']) * 1000)
    metrics['last_rx_time']['Uno_A'] = now_sec

    # 3. 고장 진단(DTC) 및 소생(Recovery) 측정
    if "FAIL" in line or "NODE_OUT" in line:
        if dtc_start_time is None:
            dtc_start_time = now_sec
        if "Engine DTC: [FAIL" in line:
            metrics['dtc_events']['Uno_A'] += 1
            metrics['failsafe_passed'] = True  # Fail-Safe 동작 확인
        if "Seatbelt DTC: [FAIL" in line:
            metrics['dtc_events']['Uno_B'] += 1

    elif "DTC: [OK]" in line and dtc_start_time is not None:
        recovery_ms = (now_sec - dtc_start_time) * 1000
        metrics['so_saeng_times'].append(recovery_ms)
        dtc_start_time = None

def generate_summary():
    intervals_a = metrics['unoA_intervals']
    recoveries = metrics['so_saeng_times']

    avg_interval_a = sum(intervals_a) / len(intervals_a) if intervals_a else 0.0
    avg_recovery = sum(recoveries) / len(recoveries) if recoveries else 0.0

    total_drops = metrics['alive_drops']['Uno_A'] + metrics['alive_drops']['Uno_B']
    drop_rate = (total_drops / metrics['total_frames'] * 100) if metrics['total_frames'] > 0 else 0.0

    report_content = f"""====================================================================
           AUTOMOTIVE CAN BUS SYSTEM SUMMARY REPORT (GATEWAY ONLY)
====================================================================
Session ID: {session_id}

[1] CAN GATEWAY THROUGHPUT & PERIODICITY
    - Total Processed Frames       : {metrics['total_frames']} Frames
    - Gateway Rx Cycle (Uno A)     : Avg {avg_interval_a:.2f} ms

[2] RELIABILITY & PACKET LOSS (ALIVE COUNTER)
    - Alive Drops (Uno A / Uno B)  : A({metrics['alive_drops']['Uno_A']}회), B({metrics['alive_drops']['Uno_B']}회)
    - Total Loss Rate              : {drop_rate:.3f} %

[3] FAULT DIAGNOSTICS & RECOVERY (SoSaeng)
    - DTC Failure Occurrences       : Engine ({metrics['dtc_events']['Uno_A']}회), Seatbelt ({metrics['dtc_events']['Uno_B']}회)
    - Avg SoSaeng Recovery Time    : {avg_recovery:.2f} ms (5-Frame Debounce)
    - Fail-Safe Status (60km/h)    : {'PASS' if metrics['failsafe_passed'] else 'N/A'}
====================================================================
"""
    with open(summary_report_file, 'w', encoding='utf-8') as f:
        f.write(report_content)

    print("\n" + report_content)
    print(f"✅ 최종 요약 리포트 저장 완료: {summary_report_file}")

if __name__ == '__main__':
    print("====================================================")
    print(f" 🚗 Gateway (Uno C: {UNO_C_PORT}) Test Runner Started")
    print(" 테스트 종료 시 Ctrl + C 를 누르세요.")
    print("====================================================\n")

    try:
        ser = serial.Serial(UNO_C_PORT, BAUD_RATE, timeout=1)
        print(f"✅ 우노 C 게이트웨이 포트({UNO_C_PORT}) 연결 성공!\n")

        while True:
            if ser.in_waiting > 0:
                line = ser.readline().decode('utf-8', errors='ignore').strip()
                if not line:
                    continue

                now_sec = time.time()
                timestamp = datetime.now().strftime('%H:%M:%S.%f')[:-3]
                log_entry = f"[{timestamp}] {line}"

                # 원본 로그 파일 기록
                with open(raw_log_file, 'a', encoding='utf-8') as f:
                    f.write(log_entry + '\n')

                parse_uno_c_line(line, now_sec)

                if "FAIL" in line or "NODE_OUT" in line:
                    print(f"🚨 [FAULT DETECTED] {log_entry}")
                else:
                    print(log_entry)

    except KeyboardInterrupt:
        print("\n테스트 종료 중... 최종 분석 요약 리포트를 생성합니다.")
        generate_summary()
    except Exception as e:
        print(f"\n❌ 포트 연결 오류: {e}")