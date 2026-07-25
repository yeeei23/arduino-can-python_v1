import serial
import time

# 본인의 아두이노 포트 번호로 수정 (예: Windows -> 'COM3', Linux/Mac -> '/dev/ttyACM0')
PORT = 'COM3' 
BAUD_RATE = 115200

def main():
    try:
        py_serial = serial.Serial(PORT, BAUD_RATE, timeout=1)
        print(f"=== [Uno A Pedal ECU] 시리얼 모니터링 시작 ({PORT}) ===")
        time.sleep(2) # 시리얼 리셋 대기

        while True:
            if py_serial.in_waiting > 0:
                # 아두이노에서 보낸 한 줄 읽기
                line = py_serial.readline().decode('utf-8', errors='ignore').strip()
                
                # [PEDAL],targetSpeed,aliveCounter 형식 검증
                if line.startswith("[PEDAL]"):
                    parts = line.split(',')
                    if len(parts) == 3:
                        tag, speed, counter = parts
                        print(f"[수신 성공] 목표 속도: {speed:>3} km/h  |  Alive Counter: {counter:>3}")
                    else:
                        print(f"[원문 데이터]: {line}")
                else:
                    print(f"[RAW]: {line}")

    except serial.SerialException as e:
        print(f"❌ 포트 연결 실패: {e}")
        print("Tip: 아두이노 IDE의 '시리얼 모니터' 창이 켜져있다면 끄고 다시 실행하세요!")
    except KeyboardInterrupt:
        print("\n=== 모니터링 종료 ===")
        py_serial.close()

if __name__ == '__main__':
    main()