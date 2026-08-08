#include "RTE_Gateway.h"

#define RECOVERY_STABLE_CNT  5  // 5회(약 100ms) 연속 수신 시 소생

void RTE_Gateway_Init(GatewayControl_SWC_Type* pSwc) {
    IoHwAb_Init();
    CDD_MCP2515_Init();
    GatewayControl_SWC_Init(pSwc);
}


// BSW MCP2515 수신 메시지를 가져와 CAN ID별 디멀티플렉싱
void RTE_Gateway_ProcessCanRx(GatewayControl_SWC_Type* pSwc) {
    Can_PduType rxPdu;

    //  CDD_MCP2515_ReadMessage가 true를 반환하는 동안 계속 읽어옴
    while (CDD_MCP2515_ReadMessage(&rxPdu)) {
        uint32_t currentMs = millis();

        if (rxPdu.can_id == 0x150) { // Uno A (Pedal)
            pSwc->unoA_Speed = rxPdu.data[0];
            pSwc->unoA_Alive = rxPdu.data[1];
            pSwc->unoA_LastRxTime = currentMs;
         
            // 단선(NodeOut == true) 상태일 때만 소생을 위한 연속 수신 카운터 올림
            if (pSwc->unoA_NodeOut) {
                if (pSwc->unoA_SuccessCount < RECOVERY_STABLE_CNT) {
                    pSwc->unoA_SuccessCount++;
                }
            } else {
                pSwc->unoA_SuccessCount = 0; // 이미 정상 통신 중이면 0 유지
            }
        }
        else if (rxPdu.can_id == 0x160) { // Uno B (Seatbelt)
            pSwc->unoB_BeltStatus = rxPdu.data[0];
            pSwc->unoB_Alive = rxPdu.data[1];
            pSwc->unoB_LastRxTime = currentMs;
            
            // 단선(NodeOut == true) 상태일 때만 소생을 위한 연속 수신 카운터 올림
            if (pSwc->unoB_NodeOut) {
                if (pSwc->unoB_SuccessCount < RECOVERY_STABLE_CNT) {
                    pSwc->unoB_SuccessCount++;
                }
            } else {
                pSwc->unoB_SuccessCount = 0; // 이미 정상 통신 중이면 0 유지
            }
        }
    }
}


// ASW의 DTC 상태를 BSW IoHwAb(경고등 LED)로 전달
void RTE_Gateway_UpdateFeedback(const GatewayControl_SWC_Type* pSwc) {
    if (pSwc == NULL) return;
    IoHwAb_SetDtcLeds(pSwc->unoA_NodeOut, pSwc->unoB_NodeOut);
}