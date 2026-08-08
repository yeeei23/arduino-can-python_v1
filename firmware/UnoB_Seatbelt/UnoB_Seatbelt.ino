
#include "src/BSW/IoHwAb.h"
#include "src/BSW/CDD_MCP2515.h"
#include "src/RTE/RTE_Seatbelt.h"
#include "src/ASW/SeatbeltControl_SWC.h"

// ASW 제어 구조체 및 전역 스케줄링 변수
static Seatbelt_SWC_Type g_seatbeltSwc;
static uint32_t g_lastTxTick = 0;

// Uno B (안전벨트 ECU): 50ms 고정 주기 적용
const uint32_t TX_INTERVAL_MS = 50;

void setup() {
    Serial.begin(115200);

    IoHwAb_Init();
    CDD_MCP2515_Init();
    RTE_Seatbelt_Init(&g_seatbeltSwc);

}

void loop() {
    // 1. [Async Rx] CAN 수신 인터럽트 루틴
    RTE_Seatbelt_ProcessCanRx(&g_seatbeltSwc);

    // 2. [Sync Tx] 50ms 고정 주기 스케줄링
    uint32_t currentMs = millis();
    if (currentMs - g_lastTxTick >= TX_INTERVAL_MS) {
        g_lastTxTick = currentMs;

        // ASW 로직 실행 (스위치 상태, Alive Counter 연산 등)
        Runnable_SeatbeltLogic_50ms(&g_seatbeltSwc);

        // CAN 메시지(0x160) 송신
        bool isTxOk = RTE_Seatbelt_TransmitCanTx(&g_seatbeltSwc);


        // 3. 0.5초(500ms) Heartbeat LED 제어 (50ms * 10회 = 500ms)
        static uint8_t txSuccessCounter = 0;

        if (isTxOk) {
            txSuccessCounter++;
            if (txSuccessCounter >= 10) { // 50ms * 10 = 500ms (0.5초)
                txSuccessCounter = 0;
                Rte_Call_NOP_ToggleLed();
            }
        } else {
            txSuccessCounter = 0;
            Rte_Call_NOP_SetLed(0); // 통신 실패 시 LED 즉시 OFF
            Serial.println(F("[ERROR] UnoB CAN Tx FAIL"));
        }
    }
   
}
