#include <Arduino.h>

#include "../BSW/IoHwAb.h"
#include "../ASW/PedalControl_SWC.h"


// ---------------------------------------------------------
// 테스트 결과 구조체 정의
// ---------------------------------------------------------
typedef struct {
    uint16_t adcRaw;          // A0 ADC 읽기 값 (0 ~ 1023)
    uint8_t  targetSpeed;     // ASW 계산 속도 (0 ~ 240 km/h)
    uint8_t  aliveCounter;    // Alive Counter (0 ~ 15)
    bool     isAdcValid;      // ADC 범위 적합성 (0 <= ADC <= 1023)
    bool     isSpeedValid;    // 속도 연산 적합성 (0 <= Speed <= 240)
    bool     isAliveValid;    // Alive Counter 적합성 (0 <= Alive <= 15)
    bool     isSystemPass;    // 전체 시스템 종합 Pass/Fail
} UnoA_TestResult_Type;

static uint8_t prevAliveCounter = 0;

// ---------------------------------------------------------
// 우노 A 단독 검증 Task
// ---------------------------------------------------------
void Test_UnoA_Pedal_Task(PedalControl_SWC_Type* pSwc) {
    UnoA_TestResult_Type testRes;

    // 1. BSW & ASW 실행
    testRes.adcRaw = IoHwAb_ReadAdcRaw();
	
    Runnable_PedalLogic_20ms(pSwc, &testRes.targetSpeed, &testRes.aliveCounter);

    // 2. 시스템 요구사항 유효성 검증 (Validation Checks)
    testRes.isAdcValid   = (testRes.adcRaw <= 1023);
    testRes.isSpeedValid = (testRes.targetSpeed <= 240);
    testRes.isAliveValid = (testRes.aliveCounter <= 15);

    // 종합 PASS 조건: 모든 범위 판정이 TRUE
    testRes.isSystemPass = (testRes.isAdcValid && testRes.isSpeedValid && testRes.isAliveValid);

    // 3. 검증 피드백 출력 (LED & Serial Output)
    if (testRes.isSystemPass) {
        digitalWrite(4, HIGH); // D4 Blue LED 점등 (PASS 시각화)

		
    } else {
        digitalWrite(4, LOW);  // 이상 발생 시 D4 LED 소등 (FAIL 시각화)

      
    }
	
	

    prevAliveCounter = testRes.aliveCounter;
}