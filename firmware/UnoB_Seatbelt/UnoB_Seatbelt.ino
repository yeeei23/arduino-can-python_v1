#include <SPI.h>
#include "src/BSW/IoHwAb.h"
#include "src/BSW/CDD_MCP2515.h"
#include "src/RTE/RTE_Seatbelt.h"
#include "src/ASW/SeatbeltControl_SWC.h"

// ASW 제어 구조체 및 전역 스케줄링 변수
static Seatbelt_SWC_Type g_seatbeltSwc;
static uint32_t g_lastTxTick = 0;

void setup() {
    Serial.begin(115200);

    // 1. [BSW Layer] 하드웨어 핀 및 CAN 복합 드라이버(ISR 인터럽트 바인딩) 초기화
    IoHwAb_Init();
    CDD_MCP2515_Init();

    // 2. [RTE & ASW Layer] 데이터 구조체 및 ASW 상태 초기화
    RTE_Seatbelt_Init(&g_seatbeltSwc);

    Serial.println(F("[OK] Uno B (Seatbelt & Heartbeat Node) Initialized."));
}

void loop() {
    // 1. [Async Rx] CAN 수신 인터럽트 루틴
    //    Uno A의 0x150 속도 패킷을 상시 감지하여 메모리 덮어쓰기 & 소생 카운터 업
    RTE_Seatbelt_ProcessCanRx(&g_seatbeltSwc);

    // 2. [Dynamic Periodic Task] ASW가 속도/Fail-Safe 상태에 따라 결정한 가변 주기에 의한 동기 실행
    //    - 정차 시: 100ms (10Hz)
    //    - 주행 중 or Node-Out 비상 시: 20ms (50Hz)
    uint32_t currentMs = millis();
    if (currentMs - g_lastTxTick >= g_seatbeltSwc.targetTxIntervalMs) {
        g_lastTxTick = currentMs;

        // ASW 메인 로직 실행 (스위치 디바운스, Node-Out/소생 판단, Fail-Safe 속도 결정, 가변 주기 갱신, White LED 하트비트)
        Runnable_SeatbeltLogic(&g_seatbeltSwc);

        // 3. [Sync Tx] 결정된 안전벨트 상태 및 Alive 카운터를 담아 0x160 CAN 패킷 전송
        RTE_Seatbelt_TransmitCanTx(&g_seatbeltSwc);
    }
}
