#ifndef CDD_MCP2515_H
#define CDD_MCP2515_H

#include <Arduino.h>
#include <stdint.h>
#include <stdbool.h>

// CAN 메시지 구조체 (BSW/CDD 표준 데이터 타입)
typedef struct {
    uint32_t can_id;  // CAN ID (예: 0x150)
    uint8_t  can_dlc; // Data Length Code (데이터 바이트 길이, 예: 2)
    uint8_t  data[8]; // 실제 전달 데이터 버퍼
} Can_PduType;

#ifdef __cplusplus
extern "C" {
#endif

// CDD MCP2515 API 명세
void CDD_MCP2515_Init(void);
bool CDD_MCP2515_WriteMessage(const Can_PduType* pdu);
bool CDD_MCP2515_ReadMessage(Can_PduType* pdu);

#ifdef __cplusplus
}
#endif

#endif // CDD_MCP2515_H