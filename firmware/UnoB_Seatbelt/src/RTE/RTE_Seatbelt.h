#ifndef RTE_SEATBELT_H
#define RTE_SEATBELT_H

#include "../BSW/CDD_MCP2515.h"
#include "../BSW/IoHwAb.h"
#include "../ASW/SeatbeltControl_SWC.h"

// RTE Init: BSW 드라이버 및 ASW 상태 초기화
void RTE_Seatbelt_Init(Seatbelt_SWC_Type* pSwc);

// BSW CAN 수신 -> ASW 데이터 최신화 (0x150 속도 패킷 디멀티플렉싱)
void RTE_Seatbelt_ProcessCanRx(Seatbelt_SWC_Type* pSwc);

// ASW 계산 결과 -> BSW CAN 전송 (0x160 안전벨트 패킷)
void RTE_Seatbelt_TransmitCanTx(const Seatbelt_SWC_Type* pSwc);

#endif