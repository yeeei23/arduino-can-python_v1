#ifndef SEATBELT_CONTROL_SWC_H
#define SEATBELT_CONTROL_SWC_H

#include <Arduino.h>

typedef struct {
    // [Rx Input] Uno A 수신 데이터 및 타임스탬프
    uint8_t  unoA_Speed;
    uint32_t unoA_LastRxTime;
    
    // [Fault Diagnostic & Recovery State]
    bool     unoA_NodeOut;          // Uno A 통신 단선 여부 (DTC)
    uint8_t  unoA_SuccessCount;     // 소생 디바운스 카운터 (5회 연속)
    
    // [Internal System State]
    bool     seatbeltBuckled;       // Debounce 적용 완료된 벨트 상태
    uint8_t  aliveCounter;          // 0~15 순환 카운터
    uint16_t targetTxIntervalMs;    // 가변 CAN 송신 주기 (20ms / 100ms)
} Seatbelt_SWC_Type;

void SeatbeltControl_SWC_Init(Seatbelt_SWC_Type* pSwc);
void Runnable_SeatbeltLogic(Seatbelt_SWC_Type* pSwc);

#endif