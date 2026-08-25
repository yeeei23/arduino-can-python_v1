#include "IoHwAb.h"
#include <avr/eeprom.h>  // AVR 표준 C용 EEPROM 헤더

/* ========================================================================== */
/*                             EEPROM 링 버퍼 매크로                           */
/* ========================================================================== */
#define ADDR_HEAD_INDEX       0x00
#define ADDR_STORED_COUNT     0x01
#define ADDR_SLOT_BASE        0x10
#define SLOT_SIZE             8

// 결함 지속 시 연속 쓰기를 방지하기 위한 정적 변수
static uint16_t s_lastSavedDtc = 0x0000;


void IoHwAb_Init(void) {
    pinMode(PIN_DIP_SW, INPUT_PULLUP);
    pinMode(PIN_GREEN_LED, OUTPUT);
    digitalWrite(PIN_GREEN_LED, LOW);

    // EEPROM 메타데이터 유효성 검증 및 초기화
    uint8_t head = eeprom_read_byte((const uint8_t*)ADDR_HEAD_INDEX);
    uint8_t count = eeprom_read_byte((const uint8_t*)ADDR_STORED_COUNT);

    if (head >= MAX_DTC_SLOTS || count > MAX_DTC_SLOTS) {
        eeprom_update_byte((uint8_t*)ADDR_HEAD_INDEX, 0);
        eeprom_update_byte((uint8_t*)ADDR_STORED_COUNT, 0);
    }

}

// DIP Switch 읽기 (ON 시 LOW 반환 -> true)
bool IoHwAb_ReadDipSwitch(void) {
    return (digitalRead(PIN_DIP_SW) == LOW);
}

// Green LED 토글
void IoHwAb_ToggleLed(void) {
    bool currentState = digitalRead(PIN_GREEN_LED);
    digitalWrite(PIN_GREEN_LED, !currentState);
}

// Green LED 상태 설정
void IoHwAb_SetLed(uint8_t state) {
    digitalWrite(PIN_GREEN_LED, state ? HIGH : LOW);
}

void IoHwAb_SaveFreezeFrame(const FreezeFrame_t* frame) {
    if (frame == NULL) return;

    uint8_t head = eeprom_read_byte((const uint8_t*)ADDR_HEAD_INDEX);
    uint8_t count = eeprom_read_byte((const uint8_t*)ADDR_STORED_COUNT);

    if (head >= MAX_DTC_SLOTS) head = 0;

    uint16_t slotAddr = ADDR_SLOT_BASE + (head * SLOT_SIZE);

    // [Uno B 전용 8바이트 매핑]
    // Byte 0~1: DTC Code (0xC100 / 0x8070)
    eeprom_update_byte((uint8_t*)(slotAddr + 0), (frame->dtcCode >> 8) & 0xFF);
    eeprom_update_byte((uint8_t*)(slotAddr + 1), frame->dtcCode & 0xFF);
    // Byte 2~3: Timestamp (sec)
    eeprom_update_byte((uint8_t*)(slotAddr + 2), (frame->timestampSec >> 8) & 0xFF);
    eeprom_update_byte((uint8_t*)(slotAddr + 3), frame->timestampSec & 0xFF);
    // Byte 4: 당시 차속
    eeprom_update_byte((uint8_t*)(slotAddr + 4), frame->lastSpeed);
    // Byte 5: 시트벨트 상태 (0/1)
    eeprom_update_byte((uint8_t*)(slotAddr + 5), frame->seatbeltState);
    // Byte 6: 결함 종류 (0x01: Comm Fail, 0x02: Switch Fault)
    eeprom_update_byte((uint8_t*)(slotAddr + 6), frame->faultSource);
    // Byte 7: 패딩
    eeprom_update_byte((uint8_t*)(slotAddr + 7), 0x00);

    head = (head + 1) % MAX_DTC_SLOTS;
    if (count < MAX_DTC_SLOTS) count++;

    eeprom_update_byte((uint8_t*)ADDR_HEAD_INDEX, head);
    eeprom_update_byte((uint8_t*)ADDR_STORED_COUNT, count);
}

uint8_t IoHwAb_GetStoredCount(void) {
    uint8_t count = eeprom_read_byte((const uint8_t*)ADDR_STORED_COUNT);
    return (count > MAX_DTC_SLOTS) ? 0 : count;
}

uint8_t IoHwAb_ReadFreezeFrameBySlot(uint8_t slotIdx, FreezeFrame_t* frame) {
    if (slotIdx >= MAX_DTC_SLOTS || frame == NULL) return 0;

    uint16_t slotAddr = ADDR_SLOT_BASE + (slotIdx * SLOT_SIZE);

    // [Uno B 전용 8바이트 복원]
    frame->dtcCode       = ((uint16_t)eeprom_read_byte((const uint8_t*)(slotAddr + 0)) << 8) | 
                            (uint16_t)eeprom_read_byte((const uint8_t*)(slotAddr + 1));
    frame->timestampSec  = ((uint16_t)eeprom_read_byte((const uint8_t*)(slotAddr + 2)) << 8) | 
                            (uint16_t)eeprom_read_byte((const uint8_t*)(slotAddr + 3));
    frame->lastSpeed     = eeprom_read_byte((const uint8_t*)(slotAddr + 4));
    frame->seatbeltState = eeprom_read_byte((const uint8_t*)(slotAddr + 5));
    frame->faultSource   = eeprom_read_byte((const uint8_t*)(slotAddr + 6));
    frame->reserved      = eeprom_read_byte((const uint8_t*)(slotAddr + 7));

    return 1;
}

void IoHwAb_ClearDTC(void) {
    eeprom_update_byte((uint8_t*)ADDR_HEAD_INDEX, 0);
    eeprom_update_byte((uint8_t*)ADDR_STORED_COUNT, 0);

    for (uint16_t i = 0; i < (MAX_DTC_SLOTS * SLOT_SIZE); i++) {
        eeprom_update_byte((uint8_t*)(ADDR_SLOT_BASE + i), 0x00);
    }
    s_lastSavedDtc = 0x0000;
}
