#ifndef SEATBELT_CONTROL_SWC_H
#define SEATBELT_CONTROL_SWC_H

#include <Arduino.h>
#include <stdbool.h>

typedef struct {
    // [Rx Input] Uno A 수신 데이터 및 타임스탬프
    uint8_t  unoA_Speed;
    uint32_t unoA_LastRxTime;
    
    // [Fault Diagnostic & Recovery State]
    bool     unoA_NodeOut;          // Uno A 통신 단선 여부 (DTC-U0100)
    uint8_t  unoA_SuccessCount;     // 소생 디바운스 카운터 (5회 연속)
    bool     seatbeltSwFault;       // 스위치 채터링/센서 결함 여부 (DTC-B0070)
    
    // [Internal System State]
    bool     seatbeltBuckled;       // Debounce 적용 완료된 벨트 상태
    uint8_t  aliveCounter;          // 0~15 순환 카운터
} Seatbelt_SWC_Type;

void SeatbeltControl_SWC_Init(Seatbelt_SWC_Type* pSwc);
void Runnable_SeatbeltLogic_50ms(Seatbelt_SWC_Type* pSwc);

#endif