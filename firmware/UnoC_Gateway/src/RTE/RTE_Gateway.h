#ifndef RTE_GATEWAY_H
#define RTE_GATEWAY_H

#include <SPI.h>

#include "../BSW/CDD_MCP2515.h"
#include "../BSW/IoHwAb.h"
#include "../ASW/GatewayControl_SWC.h"

#ifdef __cplusplus
extern "C" {
#endif

void RTE_Gateway_Init(GatewayControl_SWC_Type* pSwc);
void RTE_Gateway_ProcessCanRx(GatewayControl_SWC_Type* pSwc);
void RTE_Gateway_UpdateFeedback(const GatewayControl_SWC_Type* pSwc);


// PC 시리얼 UDS 진단 명령어('>REQ,...' / 'r' / 'c') 수신 및 응답 처리
void RTE_Gateway_ProcessSerialRx(GatewayControl_SWC_Type* pSwc);

// ASW -> BSW 진단 Freeze Frame 저장 인터페이스 (인라인 래퍼)
static inline void RTE_Gateway_Call_RP_Diagnostic_SaveFreezeFrame(const FreezeFrame_t* frame) {
    IoHwAb_SaveFreezeFrame(frame);
}


#ifdef __cplusplus
}
#endif


#endif