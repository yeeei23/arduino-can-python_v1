
#include "src/BSW/IoHwAb.h"
#include "src/BSW/CDD_MCP2515.h"
#include "src/RTE/RTE_Seatbelt.h"
#include "src/ASW/SeatbeltControl_SWC.h"

// ASW 제어 구조체 및 전역 스케줄링 변수
static Seatbelt_SWC_Type g_seatbeltSwc;
static uint32_t g_lastTxTick = 0;
static uint32_t g_lastDebugTick = 0;

// Uno B (안전벨트 ECU): 100ms 고정 주기 적용
const uint32_t TX_INTERVAL_MS = 100;

// EEPROM DTC 전체 출력 헬퍼 함수
static void PrintDtcHistory(void) {
    uint8_t count = IoHwAb_GetStoredCount();
    Serial.println(F("\n--- [EEPROM DTC History (Uno B)] ---"));
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
    Serial.println(F("------------------------------------\n"));
}

void setup() {
    Serial.begin(115200);

    IoHwAb_Init();
    CDD_MCP2515_Init();
    RTE_Seatbelt_Init(&g_seatbeltSwc);

    Serial.println(F("[INFO] Uno B (Seatbelt ECU) Initialized."));
    Serial.println(F("[INFO] Commands: 'r' = Read DTC History, 'c' = Clear EEPROM DTCs"));

}

void loop() {
    // 1. [Async Rx] CAN 수신 인터럽트 루틴
    RTE_Seatbelt_ProcessCanRx(&g_seatbeltSwc);

    // 2. [Local Diag] 시리얼 명령어 처리 ('r', 'c')
    if (Serial.available() > 0) {
        char cmd = (char)Serial.read();
        if (cmd == 'r' || cmd == 'R') {
            PrintDtcHistory();
        } else if (cmd == 'c' || cmd == 'C') {
            IoHwAb_ClearDTC();
            Serial.println(F("[EEPROM] All DTCs Cleared successfully."));
        }
    }

    // 3. [Sync Tx] 100ms 고정 주기 스케줄링
    uint32_t currentMs = millis();
    if (currentMs - g_lastTxTick >= TX_INTERVAL_MS) {
        g_lastTxTick = currentMs;

        // ASW 로직 실행 (스위치 상태, Alive Counter 연산 등)
        Runnable_SeatbeltLogic_100ms(&g_seatbeltSwc);

        // CAN 메시지(0x160) 송신
        bool isTxOk = RTE_Seatbelt_TransmitCanTx(&g_seatbeltSwc);


        // 4. Heartbeat LED 제어 (100ms * 5회 = 500ms)
        static uint8_t txSuccessCounter = 0;

        if (isTxOk) {
            txSuccessCounter++;
            if (txSuccessCounter >= 5) { // 100ms * 5 = 500ms (0.5초)
                txSuccessCounter = 0;
                Rte_Call_NOP_ToggleLed();
            }
        } else {
            txSuccessCounter = 0;
            Rte_Call_NOP_SetLed(0); // 통신 실패 시 LED 즉시 OFF
            Serial.println(F("[ERROR] UnoB CAN Tx FAIL"));
        }
    }
    // 5. [Debug Telemetry] 0.5초(500ms) 주기 상태 출력
    if (currentMs - g_lastDebugTick >= 500) {
        g_lastDebugTick = currentMs;
        
        Serial.print(F("[Uno B Status] RxSpeed: "));
        Serial.print(g_seatbeltSwc.unoA_Speed);
        Serial.print(F("km/h | Comm: "));
        // NodeOut == false -> 통신 정상(ON), NodeOut == true -> 통신 끊김(OFF)
        Serial.print(g_seatbeltSwc.unoA_NodeOut ? F("OFF") : F("ON"));
        Serial.print(F(" | Belt: "));
        Serial.print(g_seatbeltSwc.seatbeltBuckled ? F("BUCKLED") : F("UNBUCKLED"));
        Serial.print(F(" | Alive: "));
        Serial.println(g_seatbeltSwc.aliveCounter);
    }
   
}
