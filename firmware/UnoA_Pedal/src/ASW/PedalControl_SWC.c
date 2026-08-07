// ASW/PedalControl_SWC.c
#include "PedalControl_SWC.h"
#include "../RTE/Rte_PedalECU.h"

// 최대 속도 (240 km/h)
#define MAX_CLUSTER_SPEED_KMH 240UL


void PedalControl_SWC_Init(PedalControl_SWC_Type* me) {
    if (me != NULL) {
        me->aliveCounter = 0;
    }
}

void Runnable_PedalLogic_20ms(PedalControl_SWC_Type* me, uint8_t* outTargetSpeed, uint8_t* outAliveCounter) {
    uint16_t rawAdc = 0;

    // 1. RTE API를 통한 슬라이드 저항 ADC 읽기 (0 ~ 1023)
    Rte_Read_RP_AdcRaw_DeAdcValue(&rawAdc);

    // 2. 가속 페달 개도량을 속도 (0 ~ 240 km/h)으로 직접 변환
    if (outTargetSpeed != NULL) {
        *outTargetSpeed = (uint8_t)((rawAdc * MAX_CLUSTER_SPEED_KMH) / 1023UL);
    }

    // 3. Alive Counter (0 ~ 15)
    me->aliveCounter = (me->aliveCounter + 1) & 0x0F;
    if (outAliveCounter != NULL) {
        *outAliveCounter = me->aliveCounter;
    }

}