#include "RTE_Gateway.h"

#define RECOVERY_STABLE_CNT  3  // 3회 연속 수신 시 소생

static char s_serialBuf[64];
static uint8_t s_bufIdx = 0;

static uint8_t HexStringToBytes(const char* hexStr, uint8_t* outBytes, uint8_t maxLen) {
    uint8_t count = 0;
    while (*hexStr && *(hexStr + 1) && count < maxLen) {
        char high = *hexStr++;
        char low  = *hexStr++;
        auto hexVal = [](char c) -> uint8_t {
            if (c >= '0' && c <= '9') return c - '0';
            if (c >= 'A' && c <= 'F') return c - 'A' + 10;
            if (c >= 'a' && c <= 'f') return c - 'a' + 10;
            return 0;
        };
        outBytes[count++] = (hexVal(high) << 4) | hexVal(low);
    }
    return count;
}

void RTE_Gateway_Init(GatewayControl_SWC_Type* pSwc) {
    IoHwAb_Init();
    CDD_MCP2515_Init();
    GatewayControl_SWC_Init(pSwc);
}



void RTE_Gateway_ProcessCanRx(GatewayControl_SWC_Type* pSwc) {
    //if (pSwc == NULL) return;

    Can_PduType rxPdu;

    while (CDD_MCP2515_ReadMessage(&rxPdu)) {
        uint32_t currentMs = millis();

      
        // [1] Uno A (Engine / Speed) 수신
        if (rxPdu.can_id == 0x150) {
            // [추가] 순수 하드웨어 수신 간격(Delta) 측정 (최초 수신 시 0 방지)
            if (pSwc->unoA_LastRxTime > 0) {
                pSwc->unoA_DeltaTime = currentMs - pSwc->unoA_LastRxTime;
            }
            pSwc->unoA_Alive = rxPdu.data[1];
            pSwc->unoA_LastRxTime = currentMs; 

            // [수정] 패킷이 들어올 때마다 소생 카운터 증가
            if (pSwc->unoA_NodeOut) {
                pSwc->unoA_SuccessCount++;
                // [수정] 5회 연속 정상 수신 즉시 NodeOut 해제 (소생!)
                if (pSwc->unoA_SuccessCount >= RECOVERY_STABLE_CNT) {
                    pSwc->unoA_NodeOut = false; // 소생 완료!
                    pSwc->unoA_SuccessCount = 0;
                    pSwc->unoA_Speed = rxPdu.data[0]; // 실시간 센서 속도로 복귀
                }
            } else {
                // 정상 상태: 카운터 0 유지 및 실시간 속도 반영
                pSwc->unoA_SuccessCount = 0;
                pSwc->unoA_Speed = rxPdu.data[0];
            }
        }
     
        // [2] Uno B (Seatbelt) 수신
        else if (rxPdu.can_id == 0x160) {
            // [추가] 순수 하드웨어 수신 간격(Delta) 측정
            if (pSwc->unoB_LastRxTime > 0) {
                pSwc->unoB_DeltaTime = currentMs - pSwc->unoB_LastRxTime;
            }
            pSwc->unoB_Alive = rxPdu.data[1];
            pSwc->unoB_LastRxTime = currentMs; 

            // [수정] 패킷이 들어올 때마다 소생 카운터 증가
            if (pSwc->unoB_NodeOut) {
                pSwc->unoB_SuccessCount++;
                // [수정] 5회 연속 정상 수신 즉시 NodeOut 해제 (소생!)
                if (pSwc->unoB_SuccessCount >= RECOVERY_STABLE_CNT) {
                    pSwc->unoB_NodeOut = false; // 소생 완료!
                    pSwc->unoB_SuccessCount = 0;
                    pSwc->unoB_BeltStatus = rxPdu.data[0]; // 실시간 벨트 상태 복귀
                }
            } else {
                pSwc->unoB_SuccessCount = 0;
                pSwc->unoB_BeltStatus = rxPdu.data[0];
            }
        }
    }
}


void RTE_Gateway_ProcessSerialRx(GatewayControl_SWC_Type* pSwc) {
    static char s_serialBuf[64];
    static uint8_t s_bufIdx = 0;

    while (Serial.available() > 0) {
        char c = (char)Serial.read();

        // [1] 개행 수신 시 완성된 한 줄 파싱
        if (c == '\n' || c == '\r') {
            if (s_bufIdx > 0) {
                s_serialBuf[s_bufIdx] = '\0'; // 문자열 종단

                // 1-1. UDS 요청 (>REQ,ID,DATA) 파싱
                if (strncmp(s_serialBuf, ">REQ,", 5) == 0) {
                    char* pId = s_serialBuf + 5;
                    char* pData = strchr(pId, ',');
                    if (pData != NULL) {
                        *pData = '\0';
                        pData++;

                        uint8_t payload[8] = {0};
                        HexStringToBytes(pData, payload, 8);
                        uint8_t sid = payload[1];

                        // SID 0x22: ReadDataByIdentifier
                        if (sid == 0x22) {
                            uint16_t did = ((uint16_t)payload[2] << 8) | payload[3];
                            if (did == 0x0100) { // Speed DID
                                Serial.print(F("<RESP,7EA,04620100"));
                                if (pSwc->unoA_Speed < 0x10) Serial.print('0');
                                Serial.print(pSwc->unoA_Speed, HEX);
                                Serial.println(F("000000>"));
                            } else if (did == 0x0101) { // Belt DID
                                Serial.print(F("<RESP,7EA,04620101"));
                                if (pSwc->unoB_BeltStatus < 0x10) Serial.print('0');
                                Serial.print(pSwc->unoB_BeltStatus, HEX);
                                Serial.println(F("000000>"));
                            }
                        }
                        // SID 0x19: ReadDTCInformation
                        else if (sid == 0x19 && payload[2] == 0x02) {
                            uint8_t count = IoHwAb_GetStoredCount();
                            FreezeFrame_t frame;
                            if (count > 0 && IoHwAb_ReadFreezeFrameBySlot(count - 1, &frame)) {
                                Serial.print(F("<RESP,7EA,065902"));
                                if (((frame.dtcCode >> 8) & 0xFF) < 0x10) Serial.print('0');
                                Serial.print((frame.dtcCode >> 8) & 0xFF, HEX);
                                if ((frame.dtcCode & 0xFF) < 0x10) Serial.print('0');
                                Serial.print(frame.dtcCode & 0xFF, HEX);
                                if (frame.lastSpeed < 0x10) Serial.print('0');
                                Serial.print(frame.lastSpeed, HEX);
                                if (frame.seatbeltState < 0x10) Serial.print('0');
                                Serial.print(frame.seatbeltState, HEX);
                                Serial.println(F("00>"));
                            } else {
                                Serial.println(F("<RESP,7EA,0359020000000000>"));
                            }
                        }
                        // SID 0x14: ClearDiagnosticInformation
                        else if (sid == 0x14) {
                            IoHwAb_ClearDTC();
                            Serial.println(F("<RESP,7EA,0154000000000000>"));

                            // [핵심] EEPROM 삭제 지연 시간 보정 (허위 타임아웃 방지)
                            uint32_t now = millis();
                            pSwc->unoA_LastRxTime = now;
                            pSwc->unoB_LastRxTime = now;
                            
                                                }
                    }
                }
                // 1-2. 단축키 ('r', 'c') - 정확히 1글자 단독 입력일 때만 실행!
                else if (s_bufIdx == 1) {
                    if (s_serialBuf[0] == 'r' || s_serialBuf[0] == 'R') {
                        IoHwAb_PrintStoredDtc();
                    } else if (s_serialBuf[0] == 'c' || s_serialBuf[0] == 'C') {
                        IoHwAb_ClearDTC();

                        // [핵심] EEPROM 삭제 지연 시간 보정
                        uint32_t now = millis();
                        pSwc->unoA_LastRxTime = now;
                        pSwc->unoB_LastRxTime = now;
                        Serial.println(F("[EEPROM] Gateway DTCs Cleared."));
                    }
                }

                s_bufIdx = 0; // 버퍼 리셋
            }
        } 
        // [2] 문자 버퍼링
        else {
            if (s_bufIdx < sizeof(s_serialBuf) - 1) {
                s_serialBuf[s_bufIdx++] = c;
            }
        }
    }
}

// ASW의 DTC 상태를 BSW IoHwAb(경고등 LED)로 전달
void RTE_Gateway_UpdateFeedback(const GatewayControl_SWC_Type* pSwc) {
    if (pSwc == NULL) return;
    IoHwAb_SetDtcLeds(pSwc->unoA_NodeOut, pSwc->unoB_NodeOut);
}

