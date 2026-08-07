#ifndef PEDALCONTROL_SWC_H
#define PEDALCONTROL_SWC_H

#include <Arduino.h>
#include <stdint.h>

// C언어는 클래스가 없으므로 데이터 구조체(struct) 사용
typedef struct {
    uint8_t aliveCounter;
} PedalControl_SWC_Type;

#ifdef __cplusplus
extern "C" {
#endif

void PedalControl_SWC_Init(PedalControl_SWC_Type* me);
void Runnable_PedalLogic_20ms(PedalControl_SWC_Type* me, uint8_t* outTargetSpeed, uint8_t* outAliveCounter);

#ifdef __cplusplus
}
#endif

#endif // PEDALCONTROL_SWC_H