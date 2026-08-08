#include "IoHwAb.h"

void IoHwAb_Init(void) {
    pinMode(PIN_DIP_SW, INPUT_PULLUP);
    pinMode(PIN_GREEN_LED, OUTPUT);
    digitalWrite(PIN_GREEN_LED, LOW);
}

// DIP Switch 읽기 (ON 시 LOW 반환 -> true)
bool IoHwAb_ReadDipSwitch(void) {
    return (digitalRead(PIN_DIP_SW) == LOW);
}

// Green LED 토글
void IoHwAb_ToggleLed(void) {
    bool currentState = digitalRead(PIN_GREEN_LED);
    digitalWrite(PIN_GREEN_LED, !currentState);
}

// Green LED 상태 설정
void IoHwAb_SetLed(uint8_t state) {
    digitalWrite(PIN_GREEN_LED, state ? HIGH : LOW);
}