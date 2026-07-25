#include "CDD_MCP2515.h"
#include <SPI.h>


#define CAN_CS_PIN 10

MCP2515 mcp2515(CAN_CS_PIN);
volatile bool g_canCanRxFlag = false;

// ⚡ ISR: MCP2515가 메시지 수신 시 D2 핀을 LOW로 떨어뜨려 즉시 실행
void MCP2515_ISR(void) {
    g_canCanRxFlag = true;
}

void CDD_MCP2515_Init(void) {
    mcp2515.reset();
    mcp2515.setBitrate(CAN_500KBPS, MCP_8MHZ);
    mcp2515.setNormalMode();

    // IoHwAb에 정의된 CAN_INT_PIN(D2) 바인딩
    pinMode(CAN_INT_PIN, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(CAN_INT_PIN), MCP2515_ISR, FALLING);
}

bool CDD_MCP2515_ReadMessage(Can_PduType* pPdu) {
    if (pPdu == NULL) return false;

    // 📌 [핵심 수정 1] 인터럽트 플래그 또는 버퍼 수신 여부 확인
    if (g_canCanRxFlag || (digitalRead(CAN_INT_PIN) == LOW)) {
        struct can_frame frame;
        
        if (mcp2515.readMessage(&frame) == MCP2515::ERROR_OK) {
            pPdu->can_id  = frame.can_id;
            pPdu->can_dlc = frame.can_dlc;
            for (int i = 0; i < frame.can_dlc; i++) {
                pPdu->data[i] = frame.data[i];
            }

            // 📌 [핵심 수정 2] 수신 완료 후 플래그 리셋 (다음 인터럽트 대기)
            g_canCanRxFlag = false;
            return true;
        }
    }

    g_canCanRxFlag = false; // 수신 데이터가 없거나 실패 시 플래그 초기화
    return false;
}

/*bool CDD_MCP2515_ReadMessage(Can_PduType* pPdu) {
    struct can_frame frame;
    if (mcp2515.readMessage(&frame) == MCP2515::ERROR_OK) {
        pPdu->can_id = frame.can_id;
        pPdu->can_dlc = frame.can_dlc;
        for (int i = 0; i < frame.can_dlc; i++) {
            pPdu->data[i] = frame.data[i];
        }
        return true;
    }
    return false;
}*/