#include "GatewayControl_SWC.h"

#define TIMEOUT_THRESHOLD_MS 100 // 100ms 타임아웃 판정
#define RECOVERY_STABLE_CNT  5  // 5회(약 100ms) 연속 수신 시 소생

void GatewayControl_SWC_Init(GatewayControl_SWC_Type* pSwc) {
    pSwc->unoA_Speed = 0;
    pSwc->unoA_Alive = 0;
    pSwc->unoA_LastRxTime = millis();
    pSwc->unoA_NodeOut = false;

    pSwc->unoB_BeltStatus = 0;
    pSwc->unoB_Alive = 0;
    pSwc->unoB_LastRxTime = millis();
    pSwc->unoB_NodeOut = false;
}

void Runnable_GatewayLogic_20ms(GatewayControl_SWC_Type* pSwc) {
    uint32_t currentMs = millis();

    // 1. Uno A 타임아웃 (Node Out -> DTC 발생)
    if (currentMs - pSwc->unoA_LastRxTime > TIMEOUT_THRESHOLD_MS) {
        pSwc->unoA_NodeOut = true;
        pSwc->unoA_Speed = 60; // Fail-Safe 속도 60 고정
        pSwc->unoA_SuccessCount = 0;    // 단선 즉시 카운터 리셋
    }
    else {
        // 2. 패킷 수신 중인 상태
        if (pSwc->unoA_NodeOut) {
            // NodeOut 상태였다면, 연속 수신 카운터가 5(지정 횟수)에 도달해야만 소생 인정
            if (pSwc->unoA_SuccessCount >= RECOVERY_STABLE_CNT) {
                pSwc->unoA_NodeOut = false;  //  정상 복구
                pSwc->unoA_SuccessCount = 0; // 카운터 리셋
            }
        }
    }

    // 1. Uno B 타임아웃 (Node Out -> DTC 발생)
    if (currentMs - pSwc->unoB_LastRxTime > TIMEOUT_THRESHOLD_MS) {
        pSwc->unoB_NodeOut = true;
        pSwc->unoB_SuccessCount = 0; // 단선 즉시 카운터 리셋
    }
    else {
        //  2. 패킷 수신 중인 상태
        if (pSwc->unoB_NodeOut) {
            // NodeOut 상태였다면, 연속 수신 카운터가 5(지정 횟수)에 도달해야만 소생 인정
            if (pSwc->unoB_SuccessCount >= RECOVERY_STABLE_CNT) {
                pSwc->unoB_NodeOut = false;  // 정상 복구
                pSwc->unoB_SuccessCount = 0; // 카운터 리셋
            }
        }
    }
}