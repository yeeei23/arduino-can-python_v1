#include "CDD_MCP2515.h"
#include <SPI.h>
#include <mcp2515.h>  // MCP2515 CAN 컨트롤러 라이브러리

// CS 핀 번호는 외부 노출 없이 CDD 내부에서만 사용
#define CAN_CS_PIN 10

MCP2515 mcp2515(CAN_CS_PIN);
volatile bool g_canCanRxFlag = false;

//  ISR: MCP2515가 메시지 수신 시 D2 핀을 LOW로 떨어뜨려 즉시 실행
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

    //  1. 레지스터 상태 확인
    uint8_t irq = mcp2515.getInterrupts();
    uint8_t eflg = mcp2515.getErrorFlags();

    //  2. ERRIF 또는 버퍼 오버플로우 발생 시 라이브러리 정식 함수(clearRXnOVR)로 클리어
    if ((irq & MCP2515::CANINTF_ERRIF) || (eflg & 0xC0) || (eflg & 0x20)) {
        mcp2515.clearInterrupts();
        mcp2515.clearRXnOVR(); // [수정] RX0, RX1 오버플로우 플래그 일괄 클리어
    }

    //  3. 수신 버퍼에서 메시지 읽기
    struct can_frame frame;
    if (mcp2515.readMessage(&frame) == MCP2515::ERROR_OK) {
        pPdu->can_id  = frame.can_id;
        pPdu->can_dlc = frame.can_dlc;
        for (int i = 0; i < frame.can_dlc; i++) {
            pPdu->data[i] = frame.data[i];
        }

        g_canCanRxFlag = false;
        return true;
    }

    // D2 핀 정상 시 인터럽트 플래그 리셋
    if (digitalRead(CAN_INT_PIN) == HIGH) {
        g_canCanRxFlag = false;
    }

    return false;
}

bool CDD_MCP2515_WriteMessage(uint32_t id, const uint8_t* data, uint8_t dlc) {
    struct can_frame frame;
    frame.can_id = id;
    frame.can_dlc = dlc;
    for (uint8_t i = 0; i < dlc; i++) {
        frame.data[i] = data[i];
    }
    return (mcp2515.sendMessage(&frame) == MCP2515::ERROR_OK);
}

uint8_t CDD_MCP2515_GetErrorFlags(void) {
    return mcp2515.checkError(); 
}


