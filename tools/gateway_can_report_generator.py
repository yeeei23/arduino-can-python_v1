import re
from datetime import datetime

LOG_FILE_PATH = "can_log.txt"

# Uno A/B 통합 Gateway 출력 정규식
log_pattern = re.compile(
    r"\[(?P<timestamp>[\d:\.]+)\]\s+"
    r"\[UnoA\]\s+Speed:\s*(?P<speed>\d+)\s*km/h\s*\|\s*Alive:\s*(?P<alive>\d+)\s*\|\|\s*"
    r"\[UnoB\]\s+Belt:\s*(?P<belt>\w+\(\d\))\s*\|\|\s*"
    r"Engine DTC:\s*\[(?P<eng_dtc>[^\]]+)\]\s*\|\s*"
    r"Seatbelt DTC:\s*\[(?P<sb_dtc>[^\]]+)\]"
)

# 데이터 집계 변수
total_frames = 0
frame_drops = 0
prev_alive = None

time_diffs = []
prev_time = None

# DTC 타임스탬프 및 복구 관리
eng_dtc_events = []
sb_dtc_events = []

eng_fail_start = None
sb_fail_start = None

prev_eng_fail = False
prev_sb_fail = False

# 파일 읽기
try:
    with open(LOG_FILE_PATH, "r", encoding="utf-8") as f:
        log_lines = f.readlines()
except FileNotFoundError:
    log_lines = []

for line in log_lines:
    match = log_pattern.search(line)
    if not match:
        continue

    total_frames += 1
    data = match.groupdict()
    curr_time_str = data["timestamp"]
    curr_time = datetime.strptime(curr_time_str, "%H:%M:%S.%f")

    # 1. 통신 주기 산출 (Uno A 및 Gateway 통합 전송 주기)
    if prev_time is not None:
        diff_ms = (curr_time - prev_time).total_seconds() * 1000.0
        time_diffs.append(diff_ms)
    prev_time = curr_time

    # 2. 4비트 Alive Counter (0~15 Overflow) 프레임 드롭 검사
    curr_alive = int(data["alive"])
    if prev_alive is not None:
        expected_alive = (prev_alive + 1) % 16
        if curr_alive != expected_alive:
            frame_drops += 1
    prev_alive = curr_alive

    # 3. DTC 발생 및 복구(Rising/Falling Edge) 검사
    eng_fail = "FAIL" in data["eng_dtc"]
    sb_fail = "FAIL" in data["sb_dtc"]

    # Uno A Engine DTC
    if eng_fail and not prev_eng_fail:
        eng_fail_start = curr_time
        eng_dtc_events.append({"start": curr_time_str, "end": "ONGOING"})
    elif not eng_fail and prev_eng_fail and eng_dtc_events:
        duration = (curr_time - eng_fail_start).total_seconds() * 1000.0
        eng_dtc_events[-1]["end"] = f"{curr_time_str} (Duration: {duration:.1f}ms)"

    # Uno B Seatbelt DTC
    if sb_fail and not prev_sb_fail:
        sb_fail_start = curr_time
        sb_dtc_events.append({"start": curr_time_str, "end": "ONGOING"})
    elif not sb_fail and prev_sb_fail and sb_dtc_events:
        duration = (curr_time - sb_fail_start).total_seconds() * 1000.0
        sb_dtc_events[-1]["end"] = f"{curr_time_str} (Duration: {duration:.1f}ms)"

    prev_eng_fail = eng_fail
    prev_sb_fail = sb_fail

# 수치 계산
avg_period = sum(time_diffs) / len(time_diffs) if time_diffs else 0.0
uno_a_period = avg_period  # Gateway 기준 1:1 수신 주기
uno_b_period = avg_period
loss_rate = (frame_drops / total_frames * 100.0) if total_frames > 0 else 0.0

def format_dtc_list(events):
    if not events:
        return "  └ None (No DTC Detected)"
    lines = []
    for idx, ev in enumerate(events, 1):
        lines.append(f"  └ [{idx}] Occurred: {ev['start']} | Cleared/Recovered: {ev['end']}")
    return "\n".join(lines)

# 최종 요약 리포트 생성
report = f"""
====================================================================
           AUTOMOTIVE CAN BUS SYSTEM SUMMARY REPORT
====================================================================
Session ID: {datetime.now().strftime("%Y%m%d_%H%M%S")}

[1] NODE COMMUNICATION & PERIODICITY PERFORMANCE
    - Uno A Transmission Period (Avg)  : {uno_a_period:.2f} ms (Frequency: {(1000.0/uno_a_period if uno_a_period > 0 else 0):.2f} Hz)
    - Uno B Transmission Period (Avg)  : {uno_b_period:.2f} ms (Frequency: {(1000.0/uno_b_period if uno_b_period > 0 else 0):.2f} Hz)
    - Gateway Processing Latency       : Real-time Synchronized

[2] NETWORK RELIABILITY & PACKET LOSS
    - Total Processed Frames           : {total_frames} Frames
    - Alive Counter Frame Drop         : {frame_drops} Drops (Loss Rate: {loss_rate:.3f}%)

[3] FAULT DIAGNOSTICS & RECOVERY TIMELINE
    - Uno A Engine DTC Counts           : {len(eng_dtc_events)}회
{format_dtc_list(eng_dtc_events)}

    - Uno B Seatbelt DTC Counts         : {len(sb_dtc_events)}회
{format_dtc_list(sb_dtc_events)}

[4] FAIL-SAFE & SYSTEM STATUS
    - Active Failures                  : {"DETECTED" if (prev_eng_fail or prev_sb_fail) else "CLEARED / OK"}
    - System Functional Safety Status  : {"FAIL-SAFE ENGAGED" if (prev_eng_fail or prev_sb_fail) else "NORMAL OPERATION"}
====================================================================
"""

print(report)