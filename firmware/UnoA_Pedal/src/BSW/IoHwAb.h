#ifndef IOHWAB_H
#define IOHWAB_H

#include <Arduino.h>

#define PIN_PEDAL_ADC  A0
#define PIN_BLUE_LED   4

#ifdef __cplusplus
extern "C" {
#endif

void IoHwAb_Init(void);
uint16_t IoHwAb_ReadAdcRaw(void);
void IoHwAb_ToggleLed(void);

#ifdef __cplusplus
}
#endif

#endif // IOHWAB_H