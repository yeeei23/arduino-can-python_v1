#ifndef IOHWAB_H
#define IOHWAB_H

#include <Arduino.h>

// 하드웨어 핀 맵 정의
#define CAN_INT_PIN      2  // D2: MCP2515 CAN 외부 인터럽트 0
#define LED_UNOB_DTC_PIN 4  // D4: 주황색 LED (안전벨트 경고등 / Uno B DTC)
#define LED_UNOA_DTC_PIN 5  // D5: 빨간색 LED (엔진 경고등 / Uno A DTC)

void IoHwAb_Init(void);
void IoHwAb_SetDtcLeds(bool unoA_NodeOut, bool unoB_NodeOut);

#endif