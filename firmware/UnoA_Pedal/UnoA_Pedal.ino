#include <SPI.h>

#include <mcp2515.h>



// AUTOSAR 레이어 헤더 파일 포함 (src/ 상대 경로 적용)

#include "src/BSW/IoHwAb.h"

#include "src/BSW/CDD_MCP2515.h"  // Can_PduType 및 CDD API 선언 포함

#include "src/ASW/PedalControl_SWC.h"


// 🟢 [추가] Test Task 함수 선언 (loop()보다 위에 위치해야 함)
void Test_UnoA_Pedal_Task(PedalControl_SWC_Type* pSwc);

#define CS_PIN 10



MCP2515 mcp2515(CS_PIN);

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



        uint8_t targetSpeedKmh = 0; // 0 ~ 240 km/h 목표 시속

        uint8_t aliveCounter = 0;



        // 1. ASW Runnable 실행 (가속 페달 개도량 -> 목표 속도 연산)

        Runnable_PedalLogic_20ms(&pedalSwc, &targetSpeedKmh, &aliveCounter);
        // 수정: BSW Self-Test Task가 내부에서 ADC 읽기, ASW 계산, 유효성 검증, 시리얼/LED 출력을 한번에 처리!
        Test_UnoA_Pedal_Task(&pedalSwc);


        // 2. CAN 메시지 생성 및 BSW/CDD 송신 (ID: 0x150)

        Can_PduType canPdu;

        canPdu.can_id  = 0x150;

        canPdu.can_dlc = 2;

        canPdu.data[0] = targetSpeedKmh; // [Data 0] 목표 시속 (km/h)

        canPdu.data[1] = aliveCounter;   // [Data 1] Alive Counter



        CDD_MCP2515_WriteMessage(&canPdu);


        //Serial.println(targetSpeedKmh);
        // ---------------------------------------------------------
        // 📊 [시리얼 플로터 전용 3선 멀티 그래프 출력]
        // ⚠️ 각 항목을 콤마(,)로 구분하고 맨 마지막만 println으로 개행합니다.
        // ---------------------------------------------------------
        //Serial.print(F("CAN_ID_0x150:"));
        Serial.print(canPdu.can_id);        // 336 상단 기준선
        Serial.print(F(","));

        //Serial.print(F("Speed_km_h:"));
        Serial.print(targetSpeedKmh);      // 0 ~ 240 속도 곡선
        Serial.print(F(","));

        //Serial.print(F("Alive_Counter:"));
        Serial.println(aliveCounter);      // 0 ~ 15 톱니바퀴 파형
        
 

    }

}