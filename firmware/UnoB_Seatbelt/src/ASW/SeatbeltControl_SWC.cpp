#include "SeatbeltControl_SWC.h"
#include "../RTE/RTE_Seatbelt.h"

#define DEBOUNCE_STABLE_CNT 5   // 5회 연속 신호 유지 시 정식 반영
#define UNOA_TIMEOUT_MS     100 // 100ms 동안 패킷 미수신 시 Node-Out 판단
#define FAILSAFE_DEFAULT_SPEED 60 // 속도 수신 불가 시 가상 주행 속도 (60km/h)

void SeatbeltControl_SWC_Init(Seatbelt_SWC_Type* pSwc) {
    if (pSwc == NULL) return;

    pSwc->unoA_Speed = 0;
    pSwc->unoA_LastRxTime = 0;
    pSwc->unoA_NodeOut = true;     // 초기 상태는 통신 미연결(Node-Out)로 시작
    pSwc->unoA_SuccessCount = 0;
    pSwc->seatbeltBuckled = false;
    pSwc->aliveCounter = 0;
}

void Runnable_SeatbeltLogic_50ms(Seatbelt_SWC_Type* pSwc) {
    if (pSwc == NULL) return;

    uint32_t currentMs = millis();

    
    // 1. Uno A Node-Out 감지 및 소생(Recovery) 디바운스 로직
   
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

   
    // 2. Fail-Safe 속도 결정
   
    uint8_t effectiveSpeed = 0;
    if (pSwc->unoA_NodeOut) {
        // 차량 SW 표준: 속도 수신 불능 시 주행 중(60km/h)으로 간주하여 안전벨트 경고 유지
        effectiveSpeed = FAILSAFE_DEFAULT_SPEED; 
    } else {
        effectiveSpeed = pSwc->unoA_Speed;
    }

  
    // 3. DIP 스위치 디바운스 (RTE API 경유)

    static uint8_t sampleCount = 0;
    static bool lastRawState = false;
    
    bool currentRawState = false;
    // BSW 직접 호출(IoHwAb_ReadDipSwitch) 대신 RTE API 경유 호출
    Rte_Read_RP_DipSwitch_DeState(&currentRawState); 

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

    // 4. Alive Counter 갱신 (0~15 순환)
    pSwc->aliveCounter = (pSwc->aliveCounter + 1) & 0x0F;
}