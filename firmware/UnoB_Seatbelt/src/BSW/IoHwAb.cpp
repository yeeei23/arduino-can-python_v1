#include "IoHwAb.h"

#define PIN_DIP_SW       6   // DIP Switch Input (Internal Pull-up)
#define PIN_HEARTBEAT    4   // Heartbeat White LED

void IoHwAb_Init(void) {
    pinMode(PIN_DIP_SW, INPUT_PULLUP);
    pinMode(PIN_HEARTBEAT, OUTPUT);
    digitalWrite(PIN_HEARTBEAT, LOW);
}

// DIP Switch 읽기 (ON 시 LOW 반환 -> true)
bool IoHwAb_ReadDipSwitch(void) {
    return (digitalRead(PIN_DIP_SW) == LOW);
}

// White LED 하트비트 토글
void IoHwAb_ToggleHeartbeatLed(void) {
    static bool state = false;
    state = !state;
    digitalWrite(PIN_HEARTBEAT, state ? HIGH : LOW);
}