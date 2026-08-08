#include "src/BSW/IoHwAb.h"
#include "src/BSW/CDD_MCP2515.h"
#include "src/RTE/RTE_Gateway.h"
#include "src/ASW/GatewayControl_SWC.h"

GatewayControl_SWC_Type gatewaySwc;
unsigned long lastTaskTime = 0;
const unsigned long TASK_INTERVAL = 20; // 20ms 주기 (50Hz)

void setup() {
    Serial.begin(115200);

    RTE_Gateway_Init(&gatewaySwc);

    /*
    Serial.println(F("================================================"));
    Serial.println(F("   UnoC_Gateway Serial Monitor Debugger Starting   "));
    Serial.println(F("================================================"));
    Serial.println(F("D4: Orange LED (Seatbelt / UnoB DTC)"));
    Serial.println(F("D5: Red LED    (Engine   / UnoA DTC)"));
    Serial.println(F("------------------------------------------------"));
    */
}

void loop() {
    unsigned long currentTime = millis();

    //  1. D2 외부 인터럽트 기반 CAN 메시지 수신 (MCP2515 -> RTE -> ASW)
    RTE_Gateway_ProcessCanRx(&gatewaySwc);

    // 2. 20ms 주기 스케줄링 (ASW 타임아웃 검증 & 디버그 출력)
    if (currentTime - lastTaskTime >= TASK_INTERVAL) {
        lastTaskTime = currentTime;

        // ASW 100ms Timeout & Fail-Safe 검증
        Runnable_GatewayLogic_20ms(&gatewaySwc);

        // H/W 경고등 제어 (D4: 주황-UnoB / D5: 빨강-UnoA)
        RTE_Gateway_UpdateFeedback(&gatewaySwc);

        
        // 1. [Uno A 노드 데이터 출력]
        Serial.print(F("[UnoA] Speed: "));
        if (gatewaySwc.unoA_Speed < 10) Serial.print(F("  "));
        else if (gatewaySwc.unoA_Speed < 100) Serial.print(F(" "));
        Serial.print(gatewaySwc.unoA_Speed);
        Serial.print(F(" km/h | Alive: "));
        if (gatewaySwc.unoA_Alive < 10) Serial.print(F(" "));
        Serial.print(gatewaySwc.unoA_Alive);

    //  [Uno B 안전벨트 상태 & Alive & DTC 출력]
        Serial.print(F("    || [UnoB] Belt: "));
        if (gatewaySwc.unoB_NodeOut) {
            Serial.print(F("[NODE_OUT]  "));
        } else {
            // unoB_Data: 0x01 (체결) / 0x00 (미체결)
            if (gatewaySwc.unoB_Data == 0x01) {
                Serial.print(F("BUCKLED(1)  "));
            } else {
                Serial.print(F("UNBUCKLED(0)"));
            }
        }

        // Uno B Alive Counter 출력 
        Serial.print(F(" | Alive: "));
        if (gatewaySwc.unoB_Alive < 10) Serial.print(F(" "));
        Serial.print(gatewaySwc.unoB_Alive);

        
        // 2. [DTC 상태 출력 - ON(ACTIVE) / OFF(NORMAL)]
        // Uno A Engine DTC
        if (gatewaySwc.unoA_NodeOut) {
            Serial.print(F("    || Engine DTC: [ON (FAIL)] "));
        } else {
            Serial.print(F("    || Engine DTC: [OFF (OK)]  "));
        }

        // Uno B Seatbelt DTC
        if (gatewaySwc.unoB_NodeOut) {
            Serial.println(F(" | Seatbelt DTC: [ON (FAIL)] "));
        } else {
            Serial.println(F(" | Seatbelt DTC: [OFF (OK)]  "));
        }
        
    }
}