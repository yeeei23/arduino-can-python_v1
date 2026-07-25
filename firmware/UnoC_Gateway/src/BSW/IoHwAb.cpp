#include "IoHwAb.h"

void IoHwAb_Init(void) {
    pinMode(LED_UNOB_DTC_PIN, OUTPUT);
    pinMode(LED_UNOA_DTC_PIN, OUTPUT);
    digitalWrite(LED_UNOB_DTC_PIN, LOW);
    digitalWrite(LED_UNOA_DTC_PIN, LOW);
}

void IoHwAb_SetDtcLeds(bool unoA_NodeOut, bool unoB_NodeOut) {
    // Uno A (페달) 통신 끊김 -> 빨간색 엔진 경고등(D5) 점등
    digitalWrite(LED_UNOA_DTC_PIN, unoA_NodeOut ? HIGH : LOW);

    // Uno B (센서) 통신 끊김 -> 주황색 안전벨트 경고등(D4) 점등
    digitalWrite(LED_UNOB_DTC_PIN, unoB_NodeOut ? HIGH : LOW);
}