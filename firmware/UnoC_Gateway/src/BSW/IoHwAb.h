#ifndef IOHWAB_H
#define IOHWAB_H

#include <Arduino.h>
#include <stdbool.h>
#include <stdint.h>

// 하드웨어 핀 맵 정의
#define CAN_INT_PIN      2  // D2: MCP2515 CAN 외부 인터럽트 0
#define LED_UNOB_DTC_PIN 4  // D4: 주황색 LED (안전벨트 경고등 / Uno B DTC)
#define LED_UNOA_DTC_PIN 5  // D5: 빨간색 LED (엔진 경고등 / Uno A DTC)

#define MAX_DTC_SLOTS      10   // 10슬롯 링버퍼

// DTC 정의 (SAE J2012)
#define DTC_LOST_COMM_UNOA 0xC100  // U0100: Lost Comm with Uno A
#define DTC_LOST_COMM_UNOB 0xC151  // U0151: Lost Comm with Uno B

typedef struct {
    uint16_t dtcCode;       // DTC (2B)
    uint16_t timestampSec;  // 발생 시각(초) (2B)
    uint8_t  lastSpeed;     // 당시 차속 (1B)
    uint8_t  seatbeltState; // 당시 벨트 상태 (1B)
    uint8_t  faultSource;   // 0x01: Uno A Loss, 0x02: Uno B Loss (1B)
    uint8_t  reserved;      // 패딩 (1B)
} FreezeFrame_t;

#ifdef __cplusplus
extern "C" {
#endif

void IoHwAb_Init(void);
void IoHwAb_SetDtcLeds(bool unoA_NodeOut, bool unoB_NodeOut);

void IoHwAb_SaveFreezeFrame(const FreezeFrame_t* frame);
uint8_t IoHwAb_GetStoredCount(void);
uint8_t IoHwAb_ReadFreezeFrameBySlot(uint8_t slotIdx, FreezeFrame_t* frame);
void IoHwAb_ClearDTC(void);
void IoHwAb_PrintStoredDtc(void);

#ifdef __cplusplus
}
#endif

#endif