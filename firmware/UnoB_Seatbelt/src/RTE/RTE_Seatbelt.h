#ifndef RTE_SEATBELT_H
#define RTE_SEATBELT_H

#include <Arduino.h>
#include <stdint.h>
#include <stdbool.h>

#include "../BSW/CDD_MCP2515.h"
#include "../BSW/IoHwAb.h"
#include "../ASW/SeatbeltControl_SWC.h"

#ifdef __cplusplus
extern "C" {
#endif

// RTE Init: BSW 드라이버 및 ASW 상태 초기화
void RTE_Seatbelt_Init(Seatbelt_SWC_Type* pSwc);

// BSW CAN 수신 -> ASW 데이터 최신화 (0x150 속도 패킷 디멀티플렉싱)
void RTE_Seatbelt_ProcessCanRx(Seatbelt_SWC_Type* pSwc);

// ASW 계산 결과 -> BSW CAN 전송 (0x160 안전벨트 패킷)
bool RTE_Seatbelt_TransmitCanTx(const Seatbelt_SWC_Type* pSwc);

// 1. DIP 스위치 읽기 매핑 (ASW -> RTE -> BSW)
static inline void Rte_Read_RP_DipSwitch_DeState(bool* state) {
    if (state != NULL) {
        *state = IoHwAb_ReadDipSwitch();
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

#endif