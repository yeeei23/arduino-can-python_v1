// ASW/PedalControl_SWC.c
#include "PedalControl_SWC.h"
#include "../RTE/Rte_PedalECU.h"

// 최대 속도 (240 km/h)
#define MAX_CLUSTER_SPEED_KMH 240UL

// 센서 정상 동작 구간 (ADC 50 ~ 970: 약 0.25V ~ 4.75V)
#define ADC_VALID_MIN         50UL
#define ADC_VALID_MAX         970UL

// 센서 결함 시 Fail-Safe 안전 속도 (60 km/h)
#define FALLBACK_SPEED_KMH    60

// 이전 주기 결함 상태 기억 변수 (Rising Edge 검출용)
static uint8_t s_prevFaultFlag = 0x00;

void PedalControl_SWC_Init(PedalControl_SWC_Type* me) {
    if (me != NULL) {
        me->aliveCounter = 0;
    }
    s_prevFaultFlag = 0x00;
}


void Runnable_PedalLogic_20ms(PedalControl_SWC_Type* me, 
                              uint32_t currentTimestampMs, 
                              uint8_t* outTargetSpeed, 
                              uint8_t* outFaultFlag, 
                              uint8_t* outAliveCounter) {
    uint16_t rawAdc = 0;
    uint8_t targetSpeed = 0;
    uint8_t faultFlag = 0;

    // 1. RTE API를 통한 센서 ADC 읽기 (0 ~ 1023)
    Rte_Read_RP_AdcRaw_DeAdcValue(&rawAdc);

    // 2. Out-of-Range 판정 (단선: < 50, 단락: > 970)
    if (rawAdc < (uint16_t)ADC_VALID_MIN || rawAdc > (uint16_t)ADC_VALID_MAX) {
        faultFlag = (rawAdc < (uint16_t)ADC_VALID_MIN) ? 0x01 : 0x02; // 0x01: Open/GND, 0x02: Short/VCC
        targetSpeed = FALLBACK_SPEED_KMH; // 안전 속도 60km/h 제한

        // [방법 B 적용] 결함이 새로 진입한 순간(Rising Edge)에만 1회 저장 호출
        if (s_prevFaultFlag != faultFlag) {
            FreezeFrame_t frame;
            frame.dtcCode = DTC_PEDAL_SENSOR_FAULT; // 0x2138
            frame.timestampSec = (uint16_t)(currentTimestampMs / 1000UL);
            frame.rawAdc = rawAdc;
            frame.vehicleSpeed = targetSpeed;
            frame.faultFlag = faultFlag;

            Rte_Call_RP_Diagnostic_SaveFreezeFrame(&frame);
        }
    } 
    else {
        // 정상 구간 (50 ~ 970) -> (0 ~ 240 km/h) 맵핑
        faultFlag = 0x00;
        targetSpeed = (uint8_t)(((uint32_t)(rawAdc - (uint16_t)ADC_VALID_MIN) * MAX_CLUSTER_SPEED_KMH) / 
                                (ADC_VALID_MAX - ADC_VALID_MIN));
    }

    // 다음 주기를 위해 결함 상태 갱신
    s_prevFaultFlag = faultFlag;

    // 3. 출력 인터페이스 갱신
    if (outTargetSpeed != NULL) {
        *outTargetSpeed = targetSpeed;
    }

    if (outFaultFlag != NULL) {
        *outFaultFlag = faultFlag;
    }

    // 4. Alive Counter (0 ~ 15)
    if (me != NULL) {
        me->aliveCounter = (me->aliveCounter + 1) & 0x0F;
        if (outAliveCounter != NULL) {
            *outAliveCounter = me->aliveCounter;
        }
    }
}

/*void Runnable_PedalLogic_20ms(PedalControl_SWC_Type* me, uint8_t* outTargetSpeed, uint8_t* outAliveCounter) {
    uint16_t rawAdc = 0;

    // 1. RTE API를 통한 슬라이드 저항 ADC 읽기 (0 ~ 1023)
    Rte_Read_RP_AdcRaw_DeAdcValue(&rawAdc);

    // 2. 가속 페달 개도량을 속도 (0 ~ 240 km/h)으로 직접 변환
    if (outTargetSpeed != NULL) {
        *outTargetSpeed = (uint8_t)((rawAdc * MAX_CLUSTER_SPEED_KMH) / 1023UL);
    }

    // 3. Alive Counter (0 ~ 15)
    me->aliveCounter = (me->aliveCounter + 1) & 0x0F;
    if (outAliveCounter != NULL) {
        *outAliveCounter = me->aliveCounter;
    }

}*/