#include "RTE_Gateway.h"

void RTE_Gateway_Init(GatewayControl_SWC_Type* pSwc) {
    IoHwAb_Init();
    CDD_MCP2515_Init();
    GatewayControl_SWC_Init(pSwc);
}

// BSW MCP2515 인터럽트 신호를 수신하여 CAN ID별 디멀티플렉싱
/*void RTE_Gateway_ProcessCanRx(GatewayControl_SWC_Type* pSwc) {
    if (g_canCanRxFlag) {
        g_canCanRxFlag = false;

        Can_PduType rxPdu;
        while (CDD_MCP2515_ReadMessage(&rxPdu)) {
            uint32_t currentMs = millis();

            if (rxPdu.can_id == 0x150) { // Uno A (Pedal)
                pSwc->unoA_Speed = rxPdu.data[0];
                pSwc->unoA_Alive = rxPdu.data[1];
                pSwc->unoA_LastRxTime = currentMs;
                pSwc->unoA_NodeOut = false;
            } 
            else if (rxPdu.can_id == 0x160) { // Uno B (Sensor 예정)
                pSwc->unoB_Data = rxPdu.data[0];
                pSwc->unoB_Alive = rxPdu.data[1];
                pSwc->unoB_LastRxTime = currentMs;
                pSwc->unoB_NodeOut = false;
            }
        }
    }
}*/
// BSW MCP2515 수신 메시지를 가져와 CAN ID별 디멀티플렉싱
void RTE_Gateway_ProcessCanRx(GatewayControl_SWC_Type* pSwc) {
    Can_PduType rxPdu;

    // ⚡ g_canCanRxFlag 조건문을 제거하고, CDD_MCP2515_ReadMessage가 true를 반환하는 동안 계속 읽어옴
    while (CDD_MCP2515_ReadMessage(&rxPdu)) {
        uint32_t currentMs = millis();

        if (rxPdu.can_id == 0x150) { // Uno A (Pedal)
            pSwc->unoA_Speed = rxPdu.data[0];
            pSwc->unoA_Alive = rxPdu.data[1];
            pSwc->unoA_LastRxTime = currentMs;
            pSwc->unoA_NodeOut = false; // 수신 성공 시 NodeOut 해제 (DTC 정상)
        } 
        else if (rxPdu.can_id == 0x160) { // Uno B (Sensor)
            pSwc->unoB_Data = rxPdu.data[0];
            pSwc->unoB_Alive = rxPdu.data[1];
            pSwc->unoB_LastRxTime = currentMs;
            pSwc->unoB_NodeOut = false;
        }
    }
}


// ASW의 DTC 상태를 BSW IoHwAb(경고등 LED)로 전달
void RTE_Gateway_UpdateFeedback(const GatewayControl_SWC_Type* pSwc) {
    IoHwAb_SetDtcLeds(pSwc->unoA_NodeOut, pSwc->unoB_NodeOut);
}