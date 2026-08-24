// BSW/IoHwAb.c
#include "IoHwAb.h"


#include <Arduino.h>
#include <avr/eeprom.h>  // AVR 표준 C용 EEPROM 헤더

//#define EEPROM_ADDR_DTC_FLAG  0x00
//#define EEPROM_ADDR_FRAME     0x01

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
// GPIO 초기화 (핀 입출력 설정)
    //pinMode(PIN_PEDAL_ADC, INPUT);  
    pinMode(PIN_BLUE_LED, OUTPUT);

    // EEPROM 메타데이터 유효성 검증 및 초기화
    uint8_t head = eeprom_read_byte((const uint8_t*)ADDR_HEAD_INDEX);
    uint8_t count = eeprom_read_byte((const uint8_t*)ADDR_STORED_COUNT);

    if (head >= MAX_DTC_SLOTS || count > MAX_DTC_SLOTS) {
        eeprom_update_byte((uint8_t*)ADDR_HEAD_INDEX, 0);
        eeprom_update_byte((uint8_t*)ADDR_STORED_COUNT, 0);
    }

   /*eeprom_read_byte: EEPROM.read 대체
   eeprom_update_byte: EEPROM.update 대체
   단일 dtc 저장 
    if (eeprom_read_byte((const uint8_t*)EEPROM_ADDR_DTC_FLAG) > 1) {
        eeprom_update_byte((uint8_t*)EEPROM_ADDR_DTC_FLAG, 0);
    }*/
}

uint16_t IoHwAb_ReadAdcRaw(void) {
    return (uint16_t)analogRead(PIN_PEDAL_ADC);
}

void IoHwAb_ToggleLed(void) {
    digitalWrite(PIN_BLUE_LED, !digitalRead(PIN_BLUE_LED));
}

void IoHwAb_SetLed(uint8_t state) {
    digitalWrite(PIN_BLUE_LED, state ? HIGH : LOW);   
}



/*void IoHwAb_SaveFreezeFrame(const FreezeFrame_t* frame) {
    if (frame == NULL) return;

    // 20ms마다 동일한 결함이 지속될 경우 EEPROM 수명 보호를 위해 연속 쓰기 방지
    if (s_lastSavedDtc == frame->dtcCode) {
        return;
    }

    uint8_t head = eeprom_read_byte((const uint8_t*)ADDR_HEAD_INDEX);
    uint8_t count = eeprom_read_byte((const uint8_t*)ADDR_STORED_COUNT);

    if (head >= MAX_DTC_SLOTS) head = 0;

    // 슬롯 주소 계산: 0x10 + (head * 8)
    uint16_t slotAddr = ADDR_SLOT_BASE + (head * SLOT_SIZE);

    eeprom_update_byte((uint8_t*)(slotAddr + 0), (frame->dtcCode >> 8) & 0xFF);
    eeprom_update_byte((uint8_t*)(slotAddr + 1), frame->dtcCode & 0xFF);
    eeprom_update_byte((uint8_t*)(slotAddr + 2), (frame->timestampSec >> 8) & 0xFF);
    eeprom_update_byte((uint8_t*)(slotAddr + 3), frame->timestampSec & 0xFF);
    eeprom_update_byte((uint8_t*)(slotAddr + 4), (frame->rawAdc >> 8) & 0xFF);
    eeprom_update_byte((uint8_t*)(slotAddr + 5), frame->rawAdc & 0xFF);
    eeprom_update_byte((uint8_t*)(slotAddr + 6), frame->vehicleSpeed);
    eeprom_update_byte((uint8_t*)(slotAddr + 7), frame->faultFlag);

    // 링 버퍼 헤드 및 카운트 갱신
    head = (head + 1) % MAX_DTC_SLOTS;
    if (count < MAX_DTC_SLOTS) {
        count++;
    }

    eeprom_update_byte((uint8_t*)ADDR_HEAD_INDEX, head);
    eeprom_update_byte((uint8_t*)ADDR_STORED_COUNT, count);

    s_lastSavedDtc = frame->dtcCode; // 최근 저장 DTC 캐시 갱신
}*/
void IoHwAb_SaveFreezeFrame(const FreezeFrame_t* frame) {
    if (frame == NULL) return;

    uint8_t head = eeprom_read_byte((const uint8_t*)ADDR_HEAD_INDEX);
    uint8_t count = eeprom_read_byte((const uint8_t*)ADDR_STORED_COUNT);

    if (head >= MAX_DTC_SLOTS) head = 0;

    // 슬롯 주소: 0x10 + (head * 8)
    uint16_t slotAddr = ADDR_SLOT_BASE + (head * SLOT_SIZE);

    eeprom_update_byte((uint8_t*)(slotAddr + 0), (frame->dtcCode >> 8) & 0xFF);
    eeprom_update_byte((uint8_t*)(slotAddr + 1), frame->dtcCode & 0xFF);
    eeprom_update_byte((uint8_t*)(slotAddr + 2), (frame->timestampSec >> 8) & 0xFF);
    eeprom_update_byte((uint8_t*)(slotAddr + 3), frame->timestampSec & 0xFF);
    eeprom_update_byte((uint8_t*)(slotAddr + 4), (frame->rawAdc >> 8) & 0xFF);
    eeprom_update_byte((uint8_t*)(slotAddr + 5), frame->rawAdc & 0xFF);
    eeprom_update_byte((uint8_t*)(slotAddr + 6), frame->vehicleSpeed);
    eeprom_update_byte((uint8_t*)(slotAddr + 7), frame->faultFlag);

    // 링 버퍼 헤드(0~3) 및 카운트(최대 4) 갱신
    head = (head + 1) % MAX_DTC_SLOTS;
    if (count < MAX_DTC_SLOTS) {
        count++;
    }

    eeprom_update_byte((uint8_t*)ADDR_HEAD_INDEX, head);
    eeprom_update_byte((uint8_t*)ADDR_STORED_COUNT, count);
}

uint8_t IoHwAb_GetStoredCount(void) {
    uint8_t count = eeprom_read_byte((const uint8_t*)ADDR_STORED_COUNT);
    return (count > MAX_DTC_SLOTS) ? 0 : count;
}

/*uint8_t IoHwAb_ReadFreezeFrameBySlot(uint8_t slotIdx, FreezeFrame_t* frame) {
    if (slotIdx >= MAX_DTC_SLOTS || frame == NULL) return 0;

    uint16_t slotAddr = ADDR_SLOT_BASE + (slotIdx * SLOT_SIZE);

    frame->dtcCode      = ((uint16_t)eeprom_read_byte((const uint8_t*)(slotAddr + 0)) << 8) | 
                           (uint16_t)eeprom_read_byte((const uint8_t*)(slotAddr + 1));
    frame->timestampSec = ((uint16_t)eeprom_read_byte((const uint8_t*)(slotAddr + 2)) << 8) | 
                           (uint16_t)eeprom_read_byte((const uint8_t*)(slotAddr + 3));
    frame->rawAdc       = ((uint16_t)eeprom_read_byte((const uint8_t*)(slotAddr + 4)) << 8) | 
                           (uint16_t)eeprom_read_byte((const uint8_t*)(slotAddr + 5));
    frame->vehicleSpeed = eeprom_read_byte((const uint8_t*)(slotAddr + 6));
    frame->faultFlag    = eeprom_read_byte((const uint8_t*)(slotAddr + 7));

    return 1;
}*/
uint8_t IoHwAb_ReadFreezeFrameBySlot(uint8_t slotIdx, FreezeFrame_t* frame) {
    if (slotIdx >= MAX_DTC_SLOTS || frame == NULL) return 0;

    uint16_t slotAddr = ADDR_SLOT_BASE + (slotIdx * SLOT_SIZE);

    frame->dtcCode      = ((uint16_t)eeprom_read_byte((const uint8_t*)(slotAddr + 0)) << 8) | 
                           (uint16_t)eeprom_read_byte((const uint8_t*)(slotAddr + 1));
    frame->timestampSec = ((uint16_t)eeprom_read_byte((const uint8_t*)(slotAddr + 2)) << 8) | 
                           (uint16_t)eeprom_read_byte((const uint8_t*)(slotAddr + 3));
    frame->rawAdc       = ((uint16_t)eeprom_read_byte((const uint8_t*)(slotAddr + 4)) << 8) | 
                           (uint16_t)eeprom_read_byte((const uint8_t*)(slotAddr + 5));
    frame->vehicleSpeed = eeprom_read_byte((const uint8_t*)(slotAddr + 6));
    frame->faultFlag    = eeprom_read_byte((const uint8_t*)(slotAddr + 7));

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


/*void IoHwAb_SaveFreezeFrame(const FreezeFrame_t* frame) {
    if (eeprom_read_byte((const uint8_t*)EEPROM_ADDR_DTC_FLAG) == 0) {
        eeprom_update_byte((uint8_t*)(EEPROM_ADDR_FRAME + 0), (frame->dtcCode >> 8) & 0xFF);
        eeprom_update_byte((uint8_t*)(EEPROM_ADDR_FRAME + 1), frame->dtcCode & 0xFF);
        eeprom_update_byte((uint8_t*)(EEPROM_ADDR_FRAME + 2), (frame->timestampSec >> 8) & 0xFF);
        eeprom_update_byte((uint8_t*)(EEPROM_ADDR_FRAME + 3), frame->timestampSec & 0xFF);
        eeprom_update_byte((uint8_t*)(EEPROM_ADDR_FRAME + 4), (frame->rawAdc >> 8) & 0xFF);
        eeprom_update_byte((uint8_t*)(EEPROM_ADDR_FRAME + 5), frame->rawAdc & 0xFF);
        eeprom_update_byte((uint8_t*)(EEPROM_ADDR_FRAME + 6), frame->vehicleSpeed);
        eeprom_update_byte((uint8_t*)(EEPROM_ADDR_FRAME + 7), frame->faultFlag);
        eeprom_update_byte((uint8_t*)EEPROM_ADDR_DTC_FLAG, 1);
    }
}

uint8_t IoHwAb_ReadFreezeFrame(FreezeFrame_t* frame) {
    if (eeprom_read_byte((const uint8_t*)EEPROM_ADDR_DTC_FLAG) == 1) {
        frame->dtcCode      = ((uint16_t)eeprom_read_byte((const uint8_t*)(EEPROM_ADDR_FRAME + 0)) << 8) | 
                               (uint16_t)eeprom_read_byte((const uint8_t*)(EEPROM_ADDR_FRAME + 1));
        frame->timestampSec = ((uint16_t)eeprom_read_byte((const uint8_t*)(EEPROM_ADDR_FRAME + 2)) << 8) | 
                               (uint16_t)eeprom_read_byte((const uint8_t*)(EEPROM_ADDR_FRAME + 3));
        frame->rawAdc       = ((uint16_t)eeprom_read_byte((const uint8_t*)(EEPROM_ADDR_FRAME + 4)) << 8) | 
                               (uint16_t)eeprom_read_byte((const uint8_t*)(EEPROM_ADDR_FRAME + 5));
        frame->vehicleSpeed = eeprom_read_byte((const uint8_t*)(EEPROM_ADDR_FRAME + 6));
        frame->faultFlag    = eeprom_read_byte((const uint8_t*)(EEPROM_ADDR_FRAME + 7));
        return 1;
    }
    return 0;
}

void IoHwAb_ClearDTC(void) {
    eeprom_update_byte((uint8_t*)EEPROM_ADDR_DTC_FLAG, 0);
    for (int i = 0; i < 8; i++) {
        eeprom_update_byte((uint8_t*)(EEPROM_ADDR_FRAME + i), 0x00);
    }
}*/
// 표준 Freeze Frame을 EEPROM에 8바이트 연속 기록
/*void IoHwAb_SaveFreezeFrame(const FreezeFrame_t* frame) {
    // 이미 기록된 결함이 있으면 덮어쓰지 않음 (최초 발생 시점 보존 원칙)
    if (EEPROM.read(EEPROM_ADDR_DTC_FLAG) == 0) {
        EEPROM.update(EEPROM_ADDR_FRAME + 0, (frame->dtcCode >> 8) & 0xFF);
        EEPROM.update(EEPROM_ADDR_FRAME + 1, frame->dtcCode & 0xFF);
        
        EEPROM.update(EEPROM_ADDR_FRAME + 2, (frame->timestampSec >> 8) & 0xFF);
        EEPROM.update(EEPROM_ADDR_FRAME + 3, frame->timestampSec & 0xFF);
        
        EEPROM.update(EEPROM_ADDR_FRAME + 4, (frame->rawAdc >> 8) & 0xFF);
        EEPROM.update(EEPROM_ADDR_FRAME + 5, frame->rawAdc & 0xFF);
        
        EEPROM.update(EEPROM_ADDR_FRAME + 6, frame->vehicleSpeed);
        EEPROM.update(EEPROM_ADDR_FRAME + 7, frame->faultFlag);

        EEPROM.update(EEPROM_ADDR_DTC_FLAG, 1); // 기록 완료 플래그 세팅
    }
}

uint8_t IoHwAb_ReadFreezeFrame(FreezeFrame_t* frame) {
    if (EEPROM.read(EEPROM_ADDR_DTC_FLAG) == 1) {
        frame->dtcCode      = ((uint16_t)EEPROM.read(EEPROM_ADDR_FRAME + 0) << 8) | EEPROM.read(EEPROM_ADDR_FRAME + 1);
        frame->timestampSec = ((uint16_t)EEPROM.read(EEPROM_ADDR_FRAME + 2) << 8) | EEPROM.read(EEPROM_ADDR_FRAME + 3);
        frame->rawAdc       = ((uint16_t)EEPROM.read(EEPROM_ADDR_FRAME + 4) << 8) | EEPROM.read(EEPROM_ADDR_FRAME + 5);
        frame->vehicleSpeed = EEPROM.read(EEPROM_ADDR_FRAME + 6);
        frame->faultFlag    = EEPROM.read(EEPROM_ADDR_FRAME + 7);
        return 1;
    }
    return 0;
}

void IoHwAb_ClearDTC(void) {
    EEPROM.update(EEPROM_ADDR_DTC_FLAG, 0);
    for (int i = 0; i < 8; i++) {
        EEPROM.update(EEPROM_ADDR_FRAME + i, 0x00);
    }
}

void IoHwAb_PrintStoredDtc(void) {
    FreezeFrame_t frame;
    if (IoHwAb_ReadFreezeFrame(&frame)) {
        Serial.print(F("[EEPROM DTC FOUND] Code: 0x"));
        Serial.print(frame.dtcCode, HEX);
        Serial.print(F(" | Time: "));
        Serial.print(frame.timestampSec);
        Serial.print(F("s | Raw ADC: "));
        Serial.print(frame.rawAdc);
        Serial.print(F(" | Speed: "));
        Serial.print(frame.vehicleSpeed);
        Serial.print(F("km/h | Flag: 0x"));
        Serial.println(frame.faultFlag, HEX);
    } else {
        Serial.println(F("[EEPROM] No DTC Stored."));
    }
}*/