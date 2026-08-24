#ifndef RTE_PEDALECU_H
#define RTE_PEDALECU_H

#include <Arduino.h>  
#include <stdint.h>

#include "../BSW/IoHwAb.h"
#include "../BSW/CDD_MCP2515.h"



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


static inline void Rte_Call_RP_Diagnostic_SaveFreezeFrame(const FreezeFrame_t* frame) {
    IoHwAb_SaveFreezeFrame(frame);
}

// [추가] CAN PDU 조립 및 송신 래퍼 API (ID: 0x150)
static inline bool Rte_Write_PP_PedalStatus_Transmit(uint8_t targetSpeedKmh, uint8_t faultFlag, uint8_t aliveCounter) {
    Can_PduType canPdu;
    canPdu.can_id  = 0x150;
    canPdu.can_dlc = 3;                  // Speed(1B) + Alive(1B) + Fault(1B)
    canPdu.data[0] = targetSpeedKmh;     // [Data 0] 목표 속도 (km/h)
    canPdu.data[1] = aliveCounter;       // [Data 1] Alive Counter
    canPdu.data[2] = faultFlag;          // [Data 2] 고장 상태 플래그 (0:정상, 1:센서결함, 2:BusOff)

    return CDD_MCP2515_WriteMessage(&canPdu);
}



#endif // RTE_PEDALECU_H