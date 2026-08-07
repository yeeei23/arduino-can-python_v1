
#include "src/BSW/IoHwAb.h"
#include "src/BSW/CDD_MCP2515.h"  // Can_PduType 및 CDD API 선언 포함
#include "src/ASW/PedalControl_SWC.h"
#include "src/RTE/RTE_PEDALECU.h"


PedalControl_SWC_Type pedalSwc;

unsigned long lastTxTime = 0;

const unsigned long TX_INTERVAL = 20; // 20ms 주기



void setup() {

    // 시리얼 통신 초기화 (파이썬 모니터링 연동용)
    Serial.begin(115200);

    // BSW 및 ASW SWC 초기화
    IoHwAb_Init();
    PedalControl_SWC_Init(&pedalSwc);

    // CDD MCP2515 드라이버 초기화 (내부에서 spi, bitrate 설정 처리)
    CDD_MCP2515_Init();
}



void loop() {

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
    
}