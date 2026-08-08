#ifndef IOHWAB_H
#define IOHWAB_H

#include <Arduino.h>

#define CAN_INT_PIN      2  // D2: MCP2515 CAN 외부 인터럽트 0
#define PIN_DIP_SW       6   // DIP Switch Input 
#define PIN_GREEN_LED    4   // Heartbeat Green LED

#ifdef __cplusplus
extern "C" {
#endif

void IoHwAb_Init(void);
bool IoHwAb_ReadDipSwitch(void);
void IoHwAb_ToggleLed(void);
void IoHwAb_SetLed(uint8_t state);
#ifdef __cplusplus
}
#endif

#endif