#include "GatewayControl_SWC.h"
#include "../RTE/RTE_Gateway.h"

#define TIMEOUT_THRESHOLD_MS 100 // 100ms 타임아웃 판정
#define RECOVERY_STABLE_CNT  5  // 5회(약 100ms) 연속 수신 시 소생

static bool s_prevUnoA_NodeOut = false;
static bool s_prevUnoB_NodeOut = false;


void GatewayControl_SWC_Init(GatewayControl_SWC_Type* pSwc) {
    uint32_t now = millis();

    pSwc->unoA_Speed = 0;
    pSwc->unoA_Alive = 0;
    pSwc->unoA_LastRxTime = now;
    pSwc->unoA_NodeOut = false;
    pSwc->unoA_SuccessCount = 0;

    pSwc->unoB_BeltStatus = 0;
    pSwc->unoB_Alive = 0;
    pSwc->unoB_LastRxTime = now;
    pSwc->unoB_NodeOut = false;
    pSwc->unoB_SuccessCount = 0;
}

void Runnable_GatewayLogic_20ms(GatewayControl_SWC_Type* pSwc) {
    uint32_t currentMs = millis();

 
    // 1. Uno A (Engine / Speed) 타임아웃 감시
    if (currentMs - pSwc->unoA_LastRxTime > TIMEOUT_THRESHOLD_MS) {
        pSwc->unoA_NodeOut = true;

        // [수정] 단선 발생 순간 1회만 직전 속도 스냅샷 EEPROM 기록
        if (!s_prevUnoA_NodeOut) {
            FreezeFrame_t frame;
            frame.dtcCode = DTC_LOST_COMM_UNOA; // 0xC100 (U0100)
            frame.timestampSec = (uint16_t)(currentMs / 1000UL);
            frame.lastSpeed = pSwc->unoA_Speed; // 단선 직전 마지막 실제 속도
            frame.seatbeltState = pSwc->unoB_BeltStatus;
            frame.faultSource = 0x01; 
            frame.reserved = 0x00;

            RTE_Gateway_Call_RP_Diagnostic_SaveFreezeFrame(&frame);
        }

        pSwc->unoA_Speed = 60; // 페일세이프 기본값
    }
    s_prevUnoA_NodeOut = pSwc->unoA_NodeOut;

    
    // 2. Uno B (Seatbelt) 타임아웃 감시
    if (currentMs - pSwc->unoB_LastRxTime > TIMEOUT_THRESHOLD_MS) {
        pSwc->unoB_NodeOut = true;

        // [수정] 단선 발생 순간 1회만 직전 상태 스냅샷 EEPROM 기록
        if (!s_prevUnoB_NodeOut) {
            FreezeFrame_t frame;
            frame.dtcCode = DTC_LOST_COMM_UNOB; // 0xC151 (U0151)
            frame.timestampSec = (uint16_t)(currentMs / 1000UL);
            frame.lastSpeed = pSwc->unoA_Speed;
            frame.seatbeltState = pSwc->unoB_BeltStatus;
            frame.faultSource = 0x02; 
            frame.reserved = 0x00;

            RTE_Gateway_Call_RP_Diagnostic_SaveFreezeFrame(&frame);
        }
    }
    s_prevUnoB_NodeOut = pSwc->unoB_NodeOut;
}




































/*void GatewayControl_SWC_Init(GatewayControl_SWC_Type* pSwc) {
    pSwc->unoA_Speed = 0;
    pSwc->unoA_Alive = 0;
    pSwc->unoA_LastRxTime = millis();
    pSwc->unoA_NodeOut = false;
    pSwc->unoA_SuccessCount = 0;

    pSwc->unoB_BeltStatus = 0;
    pSwc->unoB_Alive = 0;
    pSwc->unoB_LastRxTime = millis();
    pSwc->unoB_NodeOut = false;
    pSwc->unoB_SuccessCount = 0;
}
void Runnable_GatewayLogic_20ms(GatewayControl_SWC_Type* pSwc) {
    uint32_t currentMs = millis();

    
    // 1. Uno A (Engine/Pedal) 타임아웃 감시 & DTC
    if (currentMs - pSwc->unoA_LastRxTime > TIMEOUT_THRESHOLD_MS) {
        pSwc->unoA_NodeOut = true;
        pSwc->unoA_Speed = 60; // Fail-Safe 속도 60 고정
        pSwc->unoA_SuccessCount = 0; // 단선 즉시 카운터 리셋

        
        // [추가] Rising Edge: 1회만 EEPROM 기록
        if (!s_prevUnoA_NodeOut) {
            FreezeFrame_t frame;
            frame.dtcCode = DTC_LOST_COMM_UNOA; // 0xC100 (U0100)
            frame.timestampSec = (uint16_t)(currentMs / 1000UL);
            frame.lastSpeed = pSwc->unoA_Speed;
            frame.seatbeltState = pSwc->unoB_BeltStatus;
            frame.faultSource = 0x01; // Fault Source: Uno A Loss
            frame.reserved = 0x00;

            RTE_Gateway_Call_RP_Diagnostic_SaveFreezeFrame(&frame);
    }
    else {
        // 패킷 수신 중인 상태
        if (pSwc->unoA_NodeOut) {
            // NodeOut 상태였다면, 연속 수신 카운터가 5에 도달해야만 소생 인정
            if (pSwc->unoA_SuccessCount >= RECOVERY_STABLE_CNT) {
                pSwc->unoA_NodeOut = false;  // 정상 복구
                pSwc->unoA_SuccessCount = 0; // 카운터 리셋
            }
        }
    }
    // [추가] Uno A 이전 상태 갱신
    s_prevUnoA_NodeOut = pSwc->unoA_NodeOut;


    
    // 2. Uno B (Seatbelt) 타임아웃 감시 & DTC
    if (currentMs - pSwc->unoB_LastRxTime > TIMEOUT_THRESHOLD_MS) {
        pSwc->unoB_NodeOut = true;
        pSwc->unoB_SuccessCount = 0; // 단선 즉시 카운터 리셋

       
        // [추가] Rising Edge: 1회만 EEPROM 기록
        
        if (!s_prevUnoB_NodeOut) {
            FreezeFrame_t frame;
            frame.dtcCode = DTC_LOST_COMM_UNOB; // 0xC151 (U0151)
            frame.timestampSec = (uint16_t)(currentMs / 1000UL);
            frame.lastSpeed = pSwc->unoA_Speed;
            frame.seatbeltState = pSwc->unoB_BeltStatus;
            frame.faultSource = 0x02; // Fault Source: Uno B Loss
            frame.reserved = 0x00;

            RTE_Gateway_Call_RP_Diagnostic_SaveFreezeFrame(&frame);
        }
    }
    else {
        // 패킷 수신 중인 상태
        if (pSwc->unoB_NodeOut) {
            // NodeOut 상태였다면, 연속 수신 카운터가 5에 도달해야만 소생 인정
            if (pSwc->unoB_SuccessCount >= RECOVERY_STABLE_CNT) {
                pSwc->unoB_NodeOut = false;  // 정상 복구
                pSwc->unoB_SuccessCount = 0; // 카운터 리셋
            }
        }
    }
    // [추가] Uno B 이전 상태 갱신
    s_prevUnoB_NodeOut = pSwc->unoB_NodeOut;
    }
}*/

/*void Runnable_GatewayLogic_20ms(GatewayControl_SWC_Type* pSwc) {
    uint32_t currentMs = millis();

    // 1. Uno A 타임아웃 감시 및 DTC 저장
    if (currentMs - pSwc->unoA_LastRxTime > TIMEOUT_THRESHOLD_MS) {
        pSwc->unoA_NodeOut = true;
        pSwc->unoA_Speed = 60; // Fail-Safe 속도 60 고정
        pSwc->unoA_SuccessCount = 0;    // 단선 즉시 카운터 리셋
    }
    else {
        // 2. 패킷 수신 중인 상태
        if (pSwc->unoA_NodeOut) {
            // NodeOut 상태였다면, 연속 수신 카운터가 5(지정 횟수)에 도달해야만 소생 인정
            if (pSwc->unoA_SuccessCount >= RECOVERY_STABLE_CNT) {
                pSwc->unoA_NodeOut = false;  //  정상 복구
                pSwc->unoA_SuccessCount = 0; // 카운터 리셋
            }
        }
    }

    // 1. Uno B 타임아웃 (Node Out -> DTC 발생)
    if (currentMs - pSwc->unoB_LastRxTime > TIMEOUT_THRESHOLD_MS) {
        pSwc->unoB_NodeOut = true;
        pSwc->unoB_SuccessCount = 0; // 단선 즉시 카운터 리셋
    }
    else {
        //  2. 패킷 수신 중인 상태
        if (pSwc->unoB_NodeOut) {
            // NodeOut 상태였다면, 연속 수신 카운터가 5(지정 횟수)에 도달해야만 소생 인정
            if (pSwc->unoB_SuccessCount >= RECOVERY_STABLE_CNT) {
                pSwc->unoB_NodeOut = false;  // 정상 복구
                pSwc->unoB_SuccessCount = 0; // 카운터 리셋
            }
        }
    }
}*/