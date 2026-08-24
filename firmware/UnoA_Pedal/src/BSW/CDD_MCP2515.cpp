#include "CDD_MCP2515.h"
#include <SPI.h>
#include <mcp2515.h> // 기본 하드웨어 SPI 제어 라이브러리 랩핑

#define CS_PIN 10

// CDD 내부 MCP2515 객체 캡슐화 (외부 ASW/RTE에 노출 안 됨)
static MCP2515 mcp2515(CS_PIN);

// C++ 컴파일러가 함수 이름을 바꾸지 않도록 C 링크 규칙 적용
#ifdef __cplusplus
extern "C" {
#endif

void CDD_MCP2515_Init(void) {
    mcp2515.reset();
    // 8MHz 크리스탈 모듈 기준 500Kbps 설정 (보유 모듈 크리스탈 주파수 확인)
    mcp2515.setBitrate(CAN_500KBPS, MCP_8MHZ); 
    mcp2515.setNormalMode();
}

bool CDD_MCP2515_WriteMessage(const Can_PduType* pdu) {
    if (pdu == NULL) return false;

    struct can_frame frame;
    frame.can_id  = pdu->can_id;
    frame.can_dlc = pdu->can_dlc;

    for (uint8_t i = 0; i < pdu->can_dlc; i++) {
        frame.data[i] = pdu->data[i];
    }

    // MCP2515 SPI 버스로 CAN 패킷 전송
    return (mcp2515.sendMessage(&frame) == MCP2515::ERROR_OK);
}



uint8_t CDD_MCP2515_GetErrorFlags(void) {
    return mcp2515.checkError(); 
}

/*bool CDD_MCP2515_ReadMessage(Can_PduType* pdu) {
    if (pdu == NULL) return false;

    struct can_frame frame;
    if (mcp2515.readMessage(&frame) == MCP2515::ERROR_OK) {
        pdu->can_id  = frame.can_id;
        pdu->can_dlc = frame.can_dlc;
        for (uint8_t i = 0; i < frame.can_dlc; i++) {
            pdu->data[i] = frame.data[i];
        }
        return true;
    }
    return false; // 수신된 패킷 없음
}*/

#ifdef __cplusplus
}
#endif
