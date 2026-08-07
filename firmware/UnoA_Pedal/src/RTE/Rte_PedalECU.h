#ifndef RTE_PEDALECU_H
#define RTE_PEDALECU_H

#include <Arduino.h>  
#include <stdint.h>
#include "../BSW/IoHwAb.h"

#ifdef __cplusplus
extern "C" {
#endif

// ASW가 BSW 함수를 직접 부르지 않고 RTE 매핑 함수를 호출
static inline void Rte_Read_RP_AdcRaw_DeAdcValue(uint16_t* data) {
    if (data != NULL) {
        *data = IoHwAb_ReadAdcRaw();
    }
}

static inline void Rte_Call_NOP_ToggleLed(void) {
    IoHwAb_ToggleLed();
}

// LED 상태 직접 제어 (통신 실패 시 OFF 제어용: 1=ON, 0=OFF)
static inline void Rte_Call_NOP_SetLed(uint8_t state) {
    IoHwAb_SetLed(state);
}

#ifdef __cplusplus
}
#endif

#endif // RTE_PEDALECU_H