#ifndef CDD_MCP2515_H
#define CDD_MCP2515_H

#include <Arduino.h>
#include <mcp2515.h>

#include "IoHwAb.h"

typedef struct {
    uint32_t can_id;
    uint8_t  can_dlc;
    uint8_t  data[8];
} Can_PduType;

// D2 외부 인터럽트 발생 시 세워지는 전역 플래그
extern volatile bool g_canCanRxFlag;

void CDD_MCP2515_Init(void);
bool CDD_MCP2515_ReadMessage(Can_PduType* pPdu);

bool CDD_MCP2515_WriteMessage(uint32_t id, const uint8_t* data, uint8_t dlc);

#endif