#include "GatewayControl_SWC.h"

#define TIMEOUT_THRESHOLD_MS 100 // 100ms 타임아웃 판정

void GatewayControl_SWC_Init(GatewayControl_SWC_Type* pSwc) {
    pSwc->unoA_Speed = 0;
    pSwc->unoA_Alive = 0;
    pSwc->unoA_LastRxTime = millis();
    pSwc->unoA_NodeOut = false;

    pSwc->unoB_Data = 0;
    pSwc->unoB_Alive = 0;
    pSwc->unoB_LastRxTime = millis();
    pSwc->unoB_NodeOut = false;
}

void Runnable_GatewayLogic_20ms(GatewayControl_SWC_Type* pSwc) {
    uint32_t currentMs = millis();

    // Uno A 타임아웃 (Node Out -> DTC 발생)
    if (currentMs - pSwc->unoA_LastRxTime > TIMEOUT_THRESHOLD_MS) {
        pSwc->unoA_NodeOut = true;
        pSwc->unoA_Speed = 0; // Fail-Safe 속도 0 고정
    }

    // Uno B 타임아웃 (Node Out -> DTC 발생)
    if (currentMs - pSwc->unoB_LastRxTime > TIMEOUT_THRESHOLD_MS) {
        pSwc->unoB_NodeOut = true;
    }
}