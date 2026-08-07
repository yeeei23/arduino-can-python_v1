// BSW/IoHwAb.c
#include "IoHwAb.h"

void IoHwAb_Init(void) {
// GPIO 초기화 (핀 입출력 설정)
    pinMode(PIN_PEDAL_ADC, INPUT);  
    pinMode(PIN_BLUE_LED, OUTPUT);
}

uint16_t IoHwAb_ReadAdcRaw(void) {
    return (uint16_t)analogRead(PIN_PEDAL_ADC);
}

void IoHwAb_ToggleLed(void) {
    digitalWrite(PIN_BLUE_LED, !digitalRead(PIN_BLUE_LED));
}

void IoHwAb_SetLed(uint8_t state) {
    digitalWrite(PIN_BLUE_LED, state ? HIGH : LOW);   
}