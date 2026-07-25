#ifndef GATEWAYCONTROL_SWC_H
#define GATEWAYCONTROL_SWC_H

#include <Arduino.h>

typedef struct {
    // Uno A (Pedal ECU / CAN ID: 0x150)
    uint8_t  unoA_Speed;
    uint8_t  unoA_Alive;
    uint32_t unoA_LastRxTime;
    bool     unoA_NodeOut; // 빨간색 엔진 경고등(D5) 조건

    // Uno B (Sensor ECU / CAN ID: 0x160 예정)
    uint8_t  unoB_Data;
    uint8_t  unoB_Alive;
    uint32_t unoB_LastRxTime;
    bool     unoB_NodeOut; // 주황색 안전벨트 경고등(D4) 조건
} GatewayControl_SWC_Type;

void GatewayControl_SWC_Init(GatewayControl_SWC_Type* pSwc);
void Runnable_GatewayLogic_20ms(GatewayControl_SWC_Type* pSwc);

#endif