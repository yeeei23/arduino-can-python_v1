#include "RTE_Seatbelt.h"

void RTE_Seatbelt_Init(Seatbelt_SWC_Type* pSwc) {
    IoHwAb_Init();
    CDD_MCP2515_Init();
    SeatbeltControl_SWC_Init(pSwc);
}

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
bool RTE_Seatbelt_TransmitCanTx(const Seatbelt_SWC_Type* pSwc) {
    if (pSwc == NULL) return;

    uint8_t txData[2];
    txData[0] = pSwc->seatbeltBuckled ? 0x01 : 0x00; // 1: Buckled, 0: Unbuckled
    txData[1] = pSwc->aliveCounter;

    // 디버깅 로그 출력 ([Tx 0x160] XX YY 형식)
    Serial.print(F("[Tx 0x160] "));

    // Data[0] 16진수 2자리 출력
    if (txData[0] < 0x10) Serial.print('0');
    Serial.print(txData[0], HEX);
    Serial.print(' ');

    // Data[1] 16진수 2자리 출력
    if (txData[1] < 0x10) Serial.print('0');
    Serial.println(txData[1], HEX);

    return CDD_MCP2515_WriteMessage(0x160, txData, 2);
}