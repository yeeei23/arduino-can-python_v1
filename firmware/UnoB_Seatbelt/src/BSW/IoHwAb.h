#ifndef IOHWAB_H
#define IOHWAB_H

#include <Arduino.h>

#define CAN_INT_PIN      2  // D2: MCP2515 CAN 외부 인터럽트 0
#define PIN_DIP_SW       6   // DIP Switch Input (Internal Pull-up)
#define PIN_HEARTBEAT    4   // Heartbeat White LED

void IoHwAb_Init(void);
bool IoHwAb_ReadDipSwitch(void);
void IoHwAb_ToggleHeartbeatLed(void);

#endif