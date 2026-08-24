#ifndef IOHWAB_H
#define IOHWAB_H


#include <stdint.h>
#include <stdbool.h>

#define PIN_PEDAL_ADC  A0
#define PIN_BLUE_LED   4

/* ========================================================================== */
/*               SAE J2012 / ISO 14229 표준 DTC 정의                          */
/* ========================================================================== */
// P2138: Powertrain 계열 가속 페달 센서 회로 결함 (ADC Out-of-Range)
#define DTC_PEDAL_SENSOR_FAULT   0x2138  


#define MAX_DTC_SLOTS            4       // 최대 링 버퍼 슬롯 수

// B0070: Body 계열 시트벨트 스위치 결함 (Uno B 적용용)
//#define DTC_SEATBELT_SW_FAULT    0x8070  

// 8바이트 Snapshot(Freeze Frame) 구조체
typedef struct {
    uint16_t dtcCode;       // DTC 코드 (2B) - 예: 0x2138
    uint16_t timestampSec;  // 발생 시각(초) (2B)
    uint16_t rawAdc;        // 당시 센서 원시 ADC (2B)
    uint8_t  vehicleSpeed;  // 당시 제어 차속 (1B)
    uint8_t  faultFlag;     // 결함 플래그 (1B, 0x01: Open, 0x02: Short)
} FreezeFrame_t;


#ifdef __cplusplus
extern "C" {
#endif

void IoHwAb_Init(void);
uint16_t IoHwAb_ReadAdcRaw(void);
void IoHwAb_ToggleLed(void);
void IoHwAb_SetLed(uint8_t state);


//void IoHwAb_SaveFreezeFrame(const FreezeFrame_t* frame);
//uint8_t IoHwAb_ReadFreezeFrame(FreezeFrame_t* frame);

// 링 버퍼 기반 DTC 관리 API
void IoHwAb_SaveFreezeFrame(const FreezeFrame_t* frame);
uint8_t IoHwAb_GetStoredCount(void);
uint8_t IoHwAb_ReadFreezeFrameBySlot(uint8_t slotIdx, FreezeFrame_t* frame);
void IoHwAb_ClearDTC(void);

void IoHwAb_PrintStoredDtc(void); // 시리얼 모니터 확인용 디버그 출력

#ifdef __cplusplus
}
#endif

#endif // IOHWAB_H