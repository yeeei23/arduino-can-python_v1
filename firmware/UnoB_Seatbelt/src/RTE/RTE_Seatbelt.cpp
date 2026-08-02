#include "RTE_Seatbelt.h"

void RTE_Seatbelt_Init(Seatbelt_SWC_Type* pSwc) {
    IoHwAb_Init();
    CDD_MCP2515_Init();
    SeatbeltControl_SWC_Init(pSwc);
}

// BSW MCP2515 수신 메시지를 가져와 CAN ID 디멀티플렉싱
/*void RTE_Seatbelt_ProcessCanRx(Seatbelt_SWC_Type* pSwc) {
    if (pSwc == NULL) return;

    Can_PduType rxPdu;

    // CDD_MCP2515_ReadMessage가 true인 동안 수신 버퍼 비우기 (우노 C와 동일)
    while (CDD_MCP2515_ReadMessage(&rxPdu)) {
        uint32_t currentMs = millis();

        if (rxPdu.can_id == 0x150) { // Uno A (Speed Pedal Node)
            pSwc->unoA_Speed = rxPdu.data[0];
            pSwc->unoA_LastRxTime = currentMs;
        }
    }
}*/
void RTE_Seatbelt_ProcessCanRx(Seatbelt_SWC_Type* pSwc) {
    if (pSwc == NULL) return;

    Can_PduType rxPdu;
    while (CDD_MCP2515_ReadMessage(&rxPdu)) {
        uint32_t currentMs = millis();

        if (rxPdu.can_id == 0x150) { // Uno A 속도 패킷
            pSwc->unoA_Speed = rxPdu.data[0];
            pSwc->unoA_LastRxTime = currentMs;

            // Node-Out 상태일 때만 소생 카운터 증가 (5회 연속 수신 검증용)
            if (pSwc->unoA_NodeOut) {
                if (pSwc->unoA_SuccessCount < 5) {
                    pSwc->unoA_SuccessCount++;
                }
            }
        }
    }
}

// ASW 데이터를 BSW MCP2515(0x160)로 전달하여 CAN 전송
void RTE_Seatbelt_TransmitCanTx(const Seatbelt_SWC_Type* pSwc) {
    if (pSwc == NULL) return;

    uint8_t txData[2];
    txData[0] = pSwc->seatbeltBuckled ? 0x01 : 0x00; // 1: Buckled, 0: Unbuckled
    txData[1] = pSwc->aliveCounter;

    CDD_MCP2515_WriteMessage(0x160, txData, 2);
}