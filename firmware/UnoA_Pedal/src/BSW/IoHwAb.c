// BSW/IoHwAb.c
#include "IoHwAb.h"

void IoHwAb_Init(void) {
// 하드웨어 핀 입출력 방향 명시적 초기화 (BSW 역할)
    pinMode(PIN_PEDAL_ADC, INPUT);   // 가속 페달 슬라이드 저항 (A0)
    pinMode(PIN_BLUE_LED, OUTPUT);
}

uint16_t IoHwAb_ReadAdcRaw(void) {
    return (uint16_t)analogRead(PIN_PEDAL_ADC);
}

void IoHwAb_ToggleLed(void) {
    digitalWrite(PIN_BLUE_LED, !digitalRead(PIN_BLUE_LED));
}