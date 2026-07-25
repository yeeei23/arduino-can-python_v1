#ifndef RTE_PEDALECU_H
#define RTE_PEDALECU_H

#include <Arduino.h>   // NULL 매크로 및 타입 정의 포함
#include <stdint.h>
#include "../BSW/IoHwAb.h"

#ifdef __cplusplus
extern "C" {
#endif

// ASW가 BSW 함수를 직접 부르지 않고 RTE 매핑 함수를 호출하도록 함
static inline void Rte_Read_RP_AdcRaw_DeAdcValue(uint16_t* data) {
    if (data != NULL) {
        *data = IoHwAb_ReadAdcRaw();
    }
}

static inline void Rte_Call_NOP_ToggleLed(void) {
    IoHwAb_ToggleLed();
}

#ifdef __cplusplus
}
#endif

#endif // RTE_PEDALECU_H