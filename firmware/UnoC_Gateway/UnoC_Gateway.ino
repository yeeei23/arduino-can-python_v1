#include "src/BSW/IoHwAb.h"
#include "src/BSW/CDD_MCP2515.h"
#include "src/RTE/RTE_Gateway.h"
#include "src/ASW/GatewayControl_SWC.h"

GatewayControl_SWC_Type gatewaySwc;


unsigned long lastTaskTime = 0;
static uint32_t lastLogTime = 0;


const unsigned long TASK_INTERVAL = 10; // 10ms 주기 
const uint32_t LOG_INTERVAL  = 10; // 로그 주기

// 숫자 2자리 패딩 출력 헬퍼 (01, 09, 15)
static void PrintDigits2(uint8_t val) {
    if (val < 10) Serial.print('0');
    Serial.print(val);
}

// 속도 3자리 패딩 출력 헬퍼 (  0,  60, 120)
static void PrintSpeed3(uint8_t speed) {
    if (speed < 10) Serial.print(F("  "));
    else if (speed < 100) Serial.print(F(" "));
    Serial.print(speed);
}

void setup() {
    Serial.begin(115200);

    RTE_Gateway_Init(&gatewaySwc);

    
    Serial.println(F("\n=========================================================================================="));
    Serial.println(F("                       [ Uno C Central Gateway Telemetry Monitor ]                        "));
    Serial.println(F("=========================================================================================="));
    Serial.println(F(" | Node A (Engine/Pedal) | Node B (Seatbelt/Restraint) | Central Gateway DTC Status      |"));
    Serial.println(F(" | Speed   | Alive | Comm| Status    | Alive | Comm    | Engine(U0100) | Seatbelt(U0151) |"));
    Serial.println(F("------------------------------------------------------------------------------------------"));
    
}

void loop() {
    // 1. D2 CAN 인터럽트 수신 (MCP2515 -> RTE -> ASW)
    RTE_Gateway_ProcessCanRx(&gatewaySwc);

    // 2. PC UDS 진단 명령 파싱 (>REQ,... 수신 처리 및 로컬 CLI 'r','c')
    RTE_Gateway_ProcessSerialRx(&gatewaySwc);

    unsigned long currentTime = millis();

    // 3. 10ms 주기 ASW 로직 (타임아웃 감시 & DTC EEPROM 자동 기록 & LED)
    if (currentTime - lastTaskTime >= TASK_INTERVAL) {
        lastTaskTime = currentTime;
        Runnable_GatewayLogic_10ms(&gatewaySwc);
        RTE_Gateway_UpdateFeedback(&gatewaySwc);
    }

    // 4. 10ms 주기 대시보드 출력
    if (currentTime - lastLogTime >= LOG_INTERVAL) {
        lastLogTime = currentTime;

        // 포맷: $DATA,<Time>,<Speed>,<Delta_A>,<Belt>,<Delta_B>,<NodeA_Err>,<NodeB_Err>
        Serial.print(F("$DATA,"));
        Serial.print(currentTime);
        Serial.print(F(","));
        Serial.print(gatewaySwc.unoA_Speed);
        Serial.print(F(","));
        Serial.print(gatewaySwc.unoA_DeltaTime);  // <== Alive Counter 대신 DeltaTime 전송
        Serial.print(F(","));
        Serial.print(gatewaySwc.unoB_BeltStatus);
        Serial.print(F(","));
        Serial.print(gatewaySwc.unoB_DeltaTime);  // <== Alive Counter 대신 DeltaTime 전송
        Serial.print(F(","));
        Serial.print(gatewaySwc.unoA_NodeOut);
        Serial.print(F(","));
        Serial.println(gatewaySwc.unoB_NodeOut);
        /*// [1] Node A (Engine / Pedal) 출력
        Serial.print(F(" | "));
        PrintSpeed3(gatewaySwc.unoA_Speed);
        Serial.print(F(" km/h |  #"));
        PrintDigits2(gatewaySwc.unoA_Alive);
        Serial.print(F("  | "));
        if (gatewaySwc.unoA_NodeOut) {
            Serial.print(F("[FAIL]"));
        } else {
            Serial.print(F("[ OK ]"));
        }

  
        // [2] Node B (Seatbelt) 출력
        Serial.print(F(" | "));
        if (gatewaySwc.unoB_NodeOut) {
            Serial.print(F("[  LOST   ]"));
        } else {
            if (gatewaySwc.unoB_BeltStatus == 0x01) {
                Serial.print(F("[ BUCKLED ]"));
            } else {
                Serial.print(F("[UNBUCKLED]"));
            }
        }
        Serial.print(F(" |  #"));
        PrintDigits2(gatewaySwc.unoB_Alive);
        Serial.print(F("  | "));
        if (gatewaySwc.unoB_NodeOut) {
            Serial.print(F("[FAIL] "));
        } else {
            Serial.print(F("[ OK ] "));
        }


        // [3] Gateway DTC 판정 상태 출력
        Serial.print(F("  | "));
        if (gatewaySwc.unoA_NodeOut) {
            Serial.print(F("[ ACTIVE(FAIL) ]"));
        } else {
            Serial.print(F("[ NORMAL(OK)   ]"));
        }

        Serial.print(F(" | "));
        if (gatewaySwc.unoB_NodeOut) {
            Serial.println(F("[ ACTIVE(FAIL) ] |"));
        } else {
            Serial.println(F("[ NORMAL(OK)   ] |"));
        }*/
    }
}

