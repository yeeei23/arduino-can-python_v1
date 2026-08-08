#ifndef RTE_GATEWAY_H
#define RTE_GATEWAY_H

#include <SPI.h>

#include "../BSW/CDD_MCP2515.h"
#include "../BSW/IoHwAb.h"
#include "../ASW/GatewayControl_SWC.h"

void RTE_Gateway_Init(GatewayControl_SWC_Type* pSwc);
void RTE_Gateway_ProcessCanRx(GatewayControl_SWC_Type* pSwc);
void RTE_Gateway_UpdateFeedback(const GatewayControl_SWC_Type* pSwc);

#endif