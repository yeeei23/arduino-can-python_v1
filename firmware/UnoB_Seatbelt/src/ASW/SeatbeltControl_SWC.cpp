#include "SeatbeltControl_SWC.h"
#include "../BSW/IoHwAb.h"

#define DEBOUNCE_STABLE_CNT 5   // 5회 연속 신호 유지 시 정식 반영
#define UNOA_TIMEOUT_MS     100 // 100ms 동안 패킷 미수신 시 Node-Out 판단
#define FAILSAFE_DEFAULT_SPEED 60 // 속도 수신 불가 시 보수적 안전을 위한 가상 주행 속도 (60km/h)

void SeatbeltControl_SWC_Init(Seatbelt_SWC_Type* pSwc) {
    if (pSwc == NULL) return;

    pSwc->unoA_Speed = 0;
    pSwc->unoA_LastRxTime = 0;
    pSwc->unoA_NodeOut = true;     // 초기 상태는 통신 미연결(Node-Out)로 시작
    pSwc->unoA_SuccessCount = 0;
    pSwc->seatbeltBuckled = false;
    pSwc->aliveCounter = 0;
    pSwc->targetTxIntervalMs = 20;  // 비상/초기 상태 20ms 지정
}

void Runnable_SeatbeltLogic(Seatbelt_SWC_Type* pSwc) {
    if (pSwc == NULL) return;

    uint32_t currentMs = millis();

    // -------------------------------------------------------------
    // 1. Uno A Node-Out 감지 및 소생(Recovery) 디바운스 로직
    // -------------------------------------------------------------
    if (currentMs - pSwc->unoA_LastRxTime > UNOA_TIMEOUT_MS) {
        // [Fault] 100ms 동안 패킷이 없으면 Node-Out 진입 & 소생 카운터 리셋
        pSwc->unoA_NodeOut = true;
        pSwc->unoA_SuccessCount = 0;
    } else {
        // [Recovery Check] 패킷이 수신 중일 때
        if (pSwc->unoA_NodeOut) {
            // RTE에서 패킷 수신 시 unoA_SuccessCount를 올려주는 구조와 연동되거나,
            // 5회 이상 수신 성공 시 소생(Recovery) 확정
            if (pSwc->unoA_SuccessCount >= DEBOUNCE_STABLE_CNT) {
                pSwc->unoA_NodeOut = false; // 소생 완료!
                pSwc->unoA_SuccessCount = 0;
            }
        }
    }

    // -------------------------------------------------------------
    // 2. Fail-Safe 속도 결정
    // -------------------------------------------------------------
    uint8_t effectiveSpeed = 0;
    if (pSwc->unoA_NodeOut) {
        // 차량 SW 표준: 속도 수신 불능 시 주행 중(60km/h)으로 간주하여 안전벨트 경고 유지
        effectiveSpeed = FAILSAFE_DEFAULT_SPEED; 
    } else {
        effectiveSpeed = pSwc->unoA_Speed;
    }

    // -------------------------------------------------------------
    // 3. DIP 스위치 소프트웨어 디바운스 (5회)
    // -------------------------------------------------------------
    static uint8_t sampleCount = 0;
    static bool lastRawState = false;
    bool currentRawState = IoHwAb_ReadDipSwitch(); // D6 읽기

    if (currentRawState == lastRawState) {
        if (sampleCount < DEBOUNCE_STABLE_CNT) {
            sampleCount++;
            if (sampleCount == DEBOUNCE_STABLE_CNT) {
                pSwc->seatbeltBuckled = currentRawState;
            }
        }
    } else {
        sampleCount = 0;
        lastRawState = currentRawState;
    }

    // -------------------------------------------------------------
    // 4. Dynamic CAN Tx Task Interval 결정
    // -------------------------------------------------------------
    // Node-Out(비상) 상태이거나 주행 중(effectiveSpeed > 0)일 때는 빠른 20ms 주기 사용
    if (pSwc->unoA_NodeOut || effectiveSpeed > 0) {
        pSwc->targetTxIntervalMs = 20;
    } else {
        pSwc->targetTxIntervalMs = 100; // 정차 시 버스 점유율 아낌
    }

    // -------------------------------------------------------------
    // 5. White LED 하트비트 토글 (D4, 500ms)
    // -------------------------------------------------------------
    static uint32_t lastHeartbeatMs = 0;
    if (currentMs - lastHeartbeatMs >= 500) {
        lastHeartbeatMs = currentMs;
        IoHwAb_ToggleHeartbeatLed();
    }

    // 6. Alive Counter 갱신
    pSwc->aliveCounter = (pSwc->aliveCounter + 1) & 0x0F;
}