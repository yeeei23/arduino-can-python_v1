#include "CDD_MCP2515.h"

#define CAN_CS_PIN 10

MCP2515 mcp2515(CAN_CS_PIN);
volatile bool g_canCanRxFlag = false;

// ISR: MCP2515가 메시지 수신 시 D2 핀을 LOW로 떨어뜨려 즉시 실행
void MCP2515_ISR(void) {
    g_canCanRxFlag = true;
}

void CDD_MCP2515_Init(void) {
    SPI.begin(); // [추가] SPI 버스 초기화 확실화
    mcp2515.reset();
    mcp2515.setBitrate(CAN_500KBPS, MCP_8MHZ);
    mcp2515.setNormalMode();

    // IoHwAb에 정의된 CAN_INT_PIN(D2) 바인딩
    pinMode(CAN_INT_PIN, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(CAN_INT_PIN), MCP2515_ISR, FALLING);
}

bool CDD_MCP2515_ReadMessage(Can_PduType* pPdu) {
    if (pPdu == NULL) return false;

    // [수정] 인터럽트 플래그가 켜졌거나 D2 핀이 LOW(잔여 메시지 존재)일 때 진입
    if (g_canCanRxFlag || digitalRead(CAN_INT_PIN) == LOW) {
        
        // [수정] 수신 버퍼 오버플로우 발생 시에만 안전하게 오버플로우 플래그 클리어 (수신 인터럽트는 건드리지 않음)
        uint8_t eflg = mcp2515.getErrorFlags();
        if (eflg & 0xC0) { // RX0OVR 또는 RX1OVR
            mcp2515.clearRXnOVR();
        }

        struct can_frame frame;
        // [수정] 메시지 수신 (성공 시 라이브러리 내부에서 해당 RXnIF 플래그를 자동으로 클리어함)
        if (mcp2515.readMessage(&frame) == MCP2515::ERROR_OK) {
            pPdu->can_id  = frame.can_id;
            pPdu->can_dlc = frame.can_dlc;
            for (uint8_t i = 0; i < frame.can_dlc; i++) {
                pPdu->data[i] = frame.data[i];
            }

            // [수정] D2 핀이 HIGH로 돌아왔으면(더 이상 남은 수신 패킷이 없으면) 플래그 리셋
            if (digitalRead(CAN_INT_PIN) == HIGH) {
                g_canCanRxFlag = false;
            }
            return true; // 수신 성공
        }
    }

    // [수정] 읽을 메시지가 없거나 INT 라인이 복구되었으면 플래그 리셋
    if (digitalRead(CAN_INT_PIN) == HIGH) {
        g_canCanRxFlag = false;
    }

    return false;
}



/*bool CDD_MCP2515_ReadMessage(Can_PduType* pPdu) {
    if (pPdu == NULL) return false;

    // 1. 레지스터 상태 확인
    uint8_t irq = mcp2515.getInterrupts();
    uint8_t eflg = mcp2515.getErrorFlags();

    // 2. ERRIF 또는 버퍼 오버플로우 발생 시 라이브러리 정식 함수(clearRXnOVR)로 클리어
    if ((irq & MCP2515::CANINTF_ERRIF) || (eflg & 0xC0) || (eflg & 0x20)) {
        mcp2515.clearInterrupts();
        mcp2515.clearRXnOVR(); // [추가] RX0, RX1 오버플로우 플래그 일괄 클리어
    }

    // 3. 수신 버퍼에서 메시지 읽기
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
}*/

