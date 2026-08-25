#include "IoHwAb.h"

#include <avr/eeprom.h>

#define ADDR_HEAD_INDEX       0x00
#define ADDR_STORED_COUNT     0x01
#define ADDR_SLOT_BASE        0x10
#define SLOT_SIZE             8

void IoHwAb_Init(void) {
    pinMode(LED_UNOB_DTC_PIN, OUTPUT);
    pinMode(LED_UNOA_DTC_PIN, OUTPUT);
    digitalWrite(LED_UNOB_DTC_PIN, LOW);
    digitalWrite(LED_UNOA_DTC_PIN, LOW);

    uint8_t head = eeprom_read_byte((const uint8_t*)ADDR_HEAD_INDEX);
    uint8_t count = eeprom_read_byte((const uint8_t*)ADDR_STORED_COUNT);

    if (head >= MAX_DTC_SLOTS || count > MAX_DTC_SLOTS) {
        eeprom_update_byte((uint8_t*)ADDR_HEAD_INDEX, 0);
        eeprom_update_byte((uint8_t*)ADDR_STORED_COUNT, 0);
    }
}

void IoHwAb_SetDtcLeds(bool unoA_NodeOut, bool unoB_NodeOut) {
    // Uno A (페달) 통신 끊김 -> 빨간색 엔진 경고등(D5) 점등
    digitalWrite(LED_UNOA_DTC_PIN, unoA_NodeOut ? HIGH : LOW);

    // Uno B (안전벨트) 통신 끊김 -> 주황색 안전벨트 경고등(D4) 점등
    digitalWrite(LED_UNOB_DTC_PIN, unoB_NodeOut ? HIGH : LOW);
}

void IoHwAb_SaveFreezeFrame(const FreezeFrame_t* frame) {
    if (frame == NULL) return;

    uint8_t head = eeprom_read_byte((const uint8_t*)ADDR_HEAD_INDEX);
    uint8_t count = eeprom_read_byte((const uint8_t*)ADDR_STORED_COUNT);

    if (head >= MAX_DTC_SLOTS) head = 0;

    uint16_t slotAddr = ADDR_SLOT_BASE + (head * SLOT_SIZE);

    eeprom_update_byte((uint8_t*)(slotAddr + 0), (frame->dtcCode >> 8) & 0xFF);
    eeprom_update_byte((uint8_t*)(slotAddr + 1), frame->dtcCode & 0xFF);
    eeprom_update_byte((uint8_t*)(slotAddr + 2), (frame->timestampSec >> 8) & 0xFF);
    eeprom_update_byte((uint8_t*)(slotAddr + 3), frame->timestampSec & 0xFF);
    eeprom_update_byte((uint8_t*)(slotAddr + 4), frame->lastSpeed);
    eeprom_update_byte((uint8_t*)(slotAddr + 5), frame->seatbeltState);
    eeprom_update_byte((uint8_t*)(slotAddr + 6), frame->faultSource);
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
}

void IoHwAb_PrintStoredDtc(void) {
    uint8_t count = IoHwAb_GetStoredCount();
    Serial.println(F("\n--- [EEPROM DTC History (Uno C Gateway)] ---"));
    Serial.print(F("Total Stored Records: "));
    Serial.println(count);

    for (uint8_t i = 0; i < MAX_DTC_SLOTS; i++) {
        FreezeFrame_t frame;
        if (IoHwAb_ReadFreezeFrameBySlot(i, &frame)) {
            Serial.print(F("[Slot "));
            Serial.print(i);
            Serial.print(F("] Code: 0x"));
            Serial.print(frame.dtcCode, HEX);
            Serial.print(F(" | Time: "));
            Serial.print(frame.timestampSec);
            Serial.print(F("s | Speed: "));
            Serial.print(frame.lastSpeed);
            Serial.print(F("km/h | Belt: "));
            Serial.print(frame.seatbeltState ? F("BUCKLED") : F("UNBUCKLED"));
            Serial.print(F(" | FaultSrc: 0x"));
            Serial.println(frame.faultSource, HEX);
        }
    }
    Serial.println(F("--------------------------------------------\n"));
}