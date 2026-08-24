
#include "src/BSW/IoHwAb.h"
#include "src/BSW/CDD_MCP2515.h"  // Can_PduType 및 CDD API 선언 포함
#include "src/ASW/PedalControl_SWC.h"
#include "src/RTE/Rte_PedalECU.h"


PedalControl_SWC_Type pedalSwc;

unsigned long lastTxTime = 0;

const unsigned long TX_INTERVAL = 20; // 20ms 주기


void printStoredDtc(void) {
    uint8_t count = IoHwAb_GetStoredCount();
    Serial.println(F("\n--- [EEPROM DTC History] ---"));
    Serial.print(F("Total Stored Records: "));
    Serial.println(count);

    if (count == 0) {
        Serial.println(F("No DTC Records Found."));
    } else {
        FreezeFrame_t frame;
        for (uint8_t i = 0; i < count; i++) {
            if (IoHwAb_ReadFreezeFrameBySlot(i, &frame)) {
                Serial.print(F("[Slot "));
                Serial.print(i);
                Serial.print(F("] Code: 0x"));
                Serial.print(frame.dtcCode, HEX);
                Serial.print(F(" | Time: "));
                Serial.print(frame.timestampSec);
                Serial.print(F("s | ADC: "));
                Serial.print(frame.rawAdc);
                Serial.print(F(" | Speed: "));
                Serial.print(frame.vehicleSpeed);
                Serial.print(F("km/h | Flag: 0x"));
                Serial.println(frame.faultFlag, HEX);
            }
        }
    }
    Serial.println(F("----------------------------\n"));
}
// EEPROM 저장된 고장 로그 시리얼 출력 함수 - DTC 1개 버전
/*void printStoredDtc(void) {
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

void setup() {
    // 시리얼 통신 초기화 (파이썬 모니터링 연동용)
    Serial.begin(115200);

    // BSW 및 ASW SWC 초기화
    IoHwAb_Init();
    PedalControl_SWC_Init(&pedalSwc);

    // CDD MCP2515 드라이버 초기화
    CDD_MCP2515_Init();

    Serial.println(F("=== Uno A (Pedal ECU) Booted ==="));
    printStoredDtc(); // 수정: IoHwAb_PrintStoredDtc -> printStoredDtc
}

void loop() {
    unsigned long currentTime = millis();

    // 20ms 주기 실행
    if (currentTime - lastTxTime >= TX_INTERVAL) {
        lastTxTime = currentTime;

        uint8_t targetSpeedKmh = 0;
        uint8_t faultFlag = 0;
        uint8_t aliveCounter = 0;

        // 1. ASW 실행 (센서 판독, DTC 결함 판단, FreezeFrame 보고, 속도 계산)
        Runnable_PedalLogic_20ms(&pedalSwc, currentTime, &targetSpeedKmh, &faultFlag, &aliveCounter);

        // 2. RTE 래퍼를 통한 CAN PDU 송신 (ID: 0x150)
        bool isTxOk = Rte_Write_PP_PedalStatus_Transmit(targetSpeedKmh, faultFlag, aliveCounter);

        // 3. Heartbeat LED 및 시리얼 모니터링
        static uint8_t txSuccessCounter = 0;

        if (isTxOk) {
            txSuccessCounter++;
            Serial.print(F("[Tx 0x150] Spd: "));
            Serial.print(targetSpeedKmh);
            Serial.print(F(" | Alive: "));
            Serial.print(aliveCounter);
            Serial.print(F(" | Fault: "));
            Serial.println(faultFlag);

            if (txSuccessCounter >= 25) { // 500ms 주기 LED 토글
                txSuccessCounter = 0;
                Rte_Call_NOP_ToggleLed();
            }
        } else {
            txSuccessCounter = 0;
            Rte_Call_NOP_SetLed(0);
            Serial.println(F("[ERROR] CAN Tx FAIL (Check Bus-Off)"));
        }
    }

    // 시리얼 커맨드로 EEPROM 초기화 및 조회 테스트 ('c': 삭제, 'r': 조회)
    if (Serial.available()) {
        char cmd = Serial.read();
        if (cmd == 'c' || cmd == 'C') {
            IoHwAb_ClearDTC();
            Serial.println(F("[EEPROM] DTC Cleared!"));
        } else if (cmd == 'r' || cmd == 'R') {
            printStoredDtc(); // 수정: IoHwAb_PrintStoredDtc -> printStoredDtc
        }
    }
}

/*void printStoredDtc(void) {
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
}

void setup() {

    // 시리얼 통신 초기화 (파이썬 모니터링 연동용)
    Serial.begin(115200);

    // BSW 및 ASW SWC 초기화
    IoHwAb_Init();
    PedalControl_SWC_Init(&pedalSwc);

    // CDD MCP2515 드라이버 초기화 (내부에서 spi, bitrate 설정 처리)
    CDD_MCP2515_Init();

    Serial.println(F("=== Uno A (Pedal ECU) Booted ==="));
    IoHwAb_PrintStoredDtc(); // 부팅 시 EEPROM에 저장된 이전 고장 로그 출력
}

void loop() {
    unsigned long currentTime = millis();

    // 20ms 주기 실행
    if (currentTime - lastTxTime >= TX_INTERVAL) {
        lastTxTime = currentTime;

        uint8_t targetSpeedKmh = 0;
        uint8_t faultFlag = 0;
        uint8_t aliveCounter = 0;

        // 1. ASW 실행 (센서 판독, DTC 결함 판단, FreezeFrame 보고, 속도 계산)
        Runnable_PedalLogic_20ms(&pedalSwc, currentTime, &targetSpeedKmh, &faultFlag, &aliveCounter);

        // 2. RTE 래퍼를 통한 CAN PDU 송신 (ID: 0x150)
        bool isTxOk = Rte_Write_PP_PedalStatus_Transmit(targetSpeedKmh, faultFlag, aliveCounter);

        // 3. Heartbeat LED 및 시리얼 모니터링
        static uint8_t txSuccessCounter = 0;

        if (isTxOk) {
            txSuccessCounter++;
            Serial.print(F("[Tx 0x150] Spd: "));
            Serial.print(targetSpeedKmh);
            Serial.print(F(" | Alive: "));
            Serial.print(aliveCounter);
            Serial.print(F(" | Fault: "));
            Serial.println(faultFlag);

            if (txSuccessCounter >= 25) { // 500ms 주기 LED 토글
                txSuccessCounter = 0;
                Rte_Call_NOP_ToggleLed();
            }
        } else {
            txSuccessCounter = 0;
            Rte_Call_NOP_SetLed(0);
            Serial.println(F("[ERROR] CAN Tx FAIL (Check Bus-Off)"));
        }
    }

    // 시리얼 커맨드로 EEPROM 초기화 테스트 가능 ('c' 입력 시 DTC 삭제)
    if (Serial.available()) {
        char cmd = Serial.read();
        if (cmd == 'c' || cmd == 'C') {
            IoHwAb_ClearDTC();
            Serial.println(F("[EEPROM] DTC Cleared!"));
        } else if (cmd == 'r' || cmd == 'R') {
            IoHwAb_PrintStoredDtc();
        }
    }
}*/

/*void loop() {

    unsigned long currentTime = millis();

    // 20ms 주기 스케줄링

    if (currentTime - lastTxTime >= TX_INTERVAL) {

        lastTxTime = currentTime;
        uint8_t targetSpeedKmh = 0; 
        uint8_t aliveCounter = 0;


        // 1. ASW Runnable 실행 (ADC 값 기반 목표 속도 및 Alive Counter 연산)
        Runnable_PedalLogic_20ms(&pedalSwc, &targetSpeedKmh, &aliveCounter);
   

        // 2. CAN 메시지 생성 및 BSW/CDD 송신 (ID: 0x150)
        Can_PduType canPdu;

        canPdu.can_id  = 0x150;
        canPdu.can_dlc = 2;
        canPdu.data[0] = targetSpeedKmh; // [Data 0] 속도 (km/h)
        canPdu.data[1] = aliveCounter;   // [Data 1] Alive Counter

        bool isTxOk = CDD_MCP2515_WriteMessage(&canPdu);

        // 4. CAN 통신 상태 연동 Heartbeat LED 제어 (0.5초 점멸 / 실패 시 OFF)
        static uint8_t txSuccessCounter = 0;

        if (isTxOk) {
            txSuccessCounter++;
            Serial.print(F("[Tx 0x"));
            Serial.print(canPdu.can_id, HEX); // ID를 Hex로 출력 (0x150)
            Serial.print(F("] "));

            // Data[0] (Target Speed) Hex 출력
            if (canPdu.data[0] < 0x10) Serial.print('0');
            Serial.print(canPdu.data[0], HEX);
            Serial.print(' ');

            // Data[1] (Alive Counter) Hex 출력
            if (canPdu.data[1] < 0x10) Serial.print('0');
            Serial.println(canPdu.data[1], HEX);

            // 20ms * 25회 = 500ms (0.5초)마다 LED 토글
            if (txSuccessCounter >= 25) {
                txSuccessCounter = 0;
                Rte_Call_NOP_ToggleLed();
            }
        } else {
            // CAN 송신 실패 시: 카운터 초기화 및 LED 강제 OFF, 송신 실패(FAIL) 시에만 에러 출력
            txSuccessCounter = 0;
            Rte_Call_NOP_SetLed(0);
            Serial.println(F("[ERROR] CAN Tx FAIL"));
        }
    }
    
}*/