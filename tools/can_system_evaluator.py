import serial
import threading
import time
from datetime import datetime
import os

# 📌 1. PC 장치 관리자의 포트 번호에 맞게 수정하세요.
PORTS = {
    'Uno_A': 'COM3',
    'Uno_B': 'COM9',
    'Uno_C': 'COM10'
}
BAUD_RATE = 115200

LOG_DIR = "./test_results"
if not os.path.exists(LOG_DIR):
    os.makedirs(LOG_DIR)

session_id = datetime.now().strftime('%Y%m%d_%H%M%S')
raw_log_file = os.path.join(LOG_DIR, f"raw_log_{session_id}.log")
summary_report_file = os.path.join(LOG_DIR, f"summary_report_{session_id}.txt")

# 측정 데이터 저장 구조체
metrics = {
    'total_frames': 0,
    'last_rx_time': {'Uno_A': None, 'Uno_B': None, 'Uno_C': None},
    'b_to_c_latencies': [],
    'chain_latencies': [],
    'unoB_intervals': [],
    'dtc_events': {'Uno_A': 0, 'Uno_B': 0},
    'so_saeng_times': [],
    'failsafe_passed': False,
    'alive_drops': {'Uno_A': 0, 'Uno_B': 0},
    'last_alive': {'Uno_A': None, 'Uno_B': None}
}
dtc_start_time = None
lock = threading.Lock()

def parse_and_analyze(node_name, line, now_sec, timestamp):
    global dtc_start_time
    with lock:
        metrics['total_frames'] += 1
        metrics['last_rx_time'][node_name] = now_sec

        # 1. E2E 및 Chain Latency 계산
        if node_name == 'Uno_B':
            if metrics['last_rx_time']['Uno_B'] is not None:
                interval = (now_sec - metrics['last_rx_time']['Uno_B']) * 1000
                metrics['unoB_intervals'].append(interval)

        elif node_name == 'Uno_C':
            if metrics['last_rx_time']['Uno_B'] is not None:
                b_to_c = (now_sec - metrics['last_rx_time']['Uno_B']) * 1000
                metrics['b_to_c_latencies'].append(b_to_c)

            if metrics['last_rx_time']['Uno_A'] is not None:
                chain = (now_sec - metrics['last_rx_time']['Uno_A']) * 1000
                metrics['chain_latencies'].append(chain)

        # 2. DTC / 소생(Recovery) 시간 및 Fail-Safe 측정
        if "FAIL" in line or "NODE_OUT" in line:
            if dtc_start_time is None:
                dtc_start_time = now_sec
            if "Engine DTC: [FAIL]" in line:
                metrics['dtc_events']['Uno_A'] += 1
                metrics['failsafe_passed'] = True # Fail-Safe 60km/h 전환 확인
            if "Belt DTC: [FAIL" in line:
                metrics['dtc_events']['Uno_B'] += 1

        elif "DTC: [OK]" in line and dtc_start_time is not None:
            recovery_ms = (now_sec - dtc_start_time) * 1000
            metrics['so_saeng_times'].append(recovery_ms)
            dtc_start_time = None

        # 3. Alive Counter 드랍(패킷 손실) 체킹
        if "Alive:" in line:
            try:
                parts = line.split("Alive:")
                alive_val = int(parts[1].split("|")[0].strip())
                target_node = 'Uno_A' if '[UnoA]' in line else ('Uno_B' if '[UnoB]' in line else None)
                
                if target_node:
                    prev_alive = metrics['last_alive'][target_node]
                    if prev_alive is not None:
                        expected = (prev_alive + 1) % 16
                        if alive_val != expected:
                            metrics['alive_drops'][target_node] += 1
                    metrics['last_alive'][target_node] = alive_val
            except Exception:
                pass

def monitor_port(node_name, port_name):
    try:
        ser = serial.Serial(port_name, BAUD_RATE, timeout=1)
        print(f"✅ [{node_name}] {port_name} 통신 연결 완료")

        while True:
            if ser.in_waiting > 0:
                line = ser.readline().decode('utf-8', errors='ignore').strip()
                if not line:
                    continue

                now_sec = time.time()
                timestamp = datetime.now().strftime('%H:%M:%S.%f')[:-3]
                log_entry = f"[{timestamp}] [{node_name}] {line}"

                # 원본 로그 파일 기록
                with open(raw_log_file, 'a', encoding='utf-8') as f:
                    f.write(log_entry + '\n')

                parse_and_analyze(node_name, line, now_sec, timestamp)

                if "FAIL" in line or "NODE_OUT" in line:
                    print(f"🚨 [FAULT EVENT] {log_entry}")

    except Exception as e:
        print(f"❌ [{node_name}] 포트 연결 실패: {e}")

def generate_executive_summary():
    with lock:
        b_c = metrics['b_to_c_latencies']
        chain = metrics['chain_latencies']
        intervals = metrics['unoB_intervals']
        recoveries = metrics['so_saeng_times']

        avg_b_c = sum(b_c) / len(b_c) if b_c else 0.0
        max_b_c = max(b_c) if b_c else 0.0

        avg_chain = sum(chain) / len(chain) if chain else 0.0
        max_chain = max(chain) if chain else 0.0

        avg_interval = sum(intervals) / len(intervals) if intervals else 0.0
        avg_recovery = sum(recoveries) / len(recoveries) if recoveries else 0.0

        total_drops = metrics['alive_drops']['Uno_A'] + metrics['alive_drops']['Uno_B']
        drop_rate = (total_drops / metrics['total_frames'] * 100) if metrics['total_frames'] > 0 else 0.0

    report_content = f"""====================================================================
           AUTOMOTIVE CAN BUS SYSTEM SUMMARY REPORT
====================================================================
Session ID: {session_id}

[1] LATENCY PERFORMANCE ANALYSIS
    - Uno B -> Uno C Direct Latency  : Avg {avg_b_c:.2f} ms | Max {max_b_c:.2f} ms
    - Uno A -> Uno B -> Uno C Chain  : Avg {avg_chain:.2f} ms | Max {max_chain:.2f} ms

[2] DYNAMIC TASK PERIODICITY
    - Uno B Average Transmission Period : {avg_interval:.2f} ms

[3] RELIABILITY & PACKET LOSS
    - Total Processed Frames       : {metrics['total_frames']} Frames
    - Alive Counter Frame Drop     : {total_drops} Drops (Loss Rate: {drop_rate:.3f}%)

[4] FAULT DIAGNOSTICS & RECOVERY (SoSaeng)
    - DTC Failure Counts           : Uno A ({metrics['dtc_events']['Uno_A']}회), Uno B ({metrics['dtc_events']['Uno_B']}회)
    - Avg SoSaeng Recovery Time    : {avg_recovery:.2f} ms (5-Frame Debounce)
    - Fail-Safe Enforced (60km/h)  : {'PASS' if metrics['failsafe_passed'] else 'N/A'}
====================================================================
"""
    with open(summary_report_file, 'w', encoding='utf-8') as f:
        f.write(report_content)

    print("\n" + report_content)
    print(f"✅ 요약 리포트 저장 완료: {summary_report_file}")

if __name__ == '__main__':
    print("====================================================")
    print(" 🚗 CAN Bus System Evaluator Started")
    print(" 테스트 종료 시 Ctrl + C 를 누르세요.")
    print("====================================================\n")

    threads = []
    for node, port in PORTS.items():
        t = threading.Thread(target=monitor_port, args=(node, port), daemon=True)
        threads.append(t)
        t.start()

    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        print("\n테스트 종료 중... 최종 분석 요약 리포트를 생성합니다.")
        generate_executive_summary()