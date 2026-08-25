#ifndef IOHWAB_H
#define IOHWAB_H

#include <Arduino.h>
#include <stdbool.h>

#define CAN_INT_PIN      2  // D2: MCP2515 CAN 외부 인터럽트 0
#define PIN_DIP_SW       6   // DIP Switch Input 
#define PIN_GREEN_LED    4   // Heartbeat Green LED

/* ========================================================================== */
/*               SAE J2012 / ISO 14229 표준 DTC 정의                          */
/* ========================================================================== */
// P2138: Powertrain 계열 가속 페달 센서 회로 결함 (ADC Out-of-Range)
//#define DTC_PEDAL_SENSOR_FAULT   0x2138  


#define MAX_DTC_SLOTS            4       // 최대 링 버퍼 슬롯 수

// DTC 정의 (SAE J2012 / ISO 14229)
#define DTC_SEATBELT_SW_FAULT    0x8070  // B0070: Driver Seatbelt Switch Fault
#define DTC_LOST_COMM_UNOA       0xC100  // U0100: Lost Comm with Uno A

// [수정 포인트] Uno B 전용 8바이트 구조체 정의
typedef struct {
    uint16_t dtcCode;       // DTC (2B) -> 0x8070 or 0xC100
    uint16_t timestampSec;  // 발생 시각(초) (2B)
    uint8_t  lastSpeed;     // 당시 차속 (1B)
    uint8_t  seatbeltState; // 당시 벨트 상태 (1B)
    uint8_t  faultSource;   // 0x01: Comm Fail, 0x02: Switch Fault (1B)
    uint8_t  reserved;      // 8바이트 패딩 (1B)
} FreezeFrame_t;


#ifdef __cplusplus
extern "C" {
#endif

void IoHwAb_Init(void);
bool IoHwAb_ReadDipSwitch(void);
void IoHwAb_ToggleLed(void);
void IoHwAb_SetLed(uint8_t state);

// 링 버퍼 기반 DTC 관리 API
void IoHwAb_SaveFreezeFrame(const FreezeFrame_t* frame);
uint8_t IoHwAb_GetStoredCount(void);
uint8_t IoHwAb_ReadFreezeFrameBySlot(uint8_t slotIdx, FreezeFrame_t* frame);
void IoHwAb_ClearDTC(void);

void IoHwAb_PrintStoredDtc(void); // 시리얼 모니터 확인용 디버그 출력


#ifdef __cplusplus
}
#endif

#endif