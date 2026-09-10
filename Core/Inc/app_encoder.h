#ifndef APP_ENCODER_H
#define APP_ENCODER_H
#include <stdint.h>
#include "stm32f1xx_hal.h"

void App_Encoder_Init(void);
uint64_t App_GetMicroseconds(void);
void App_Encoder_Get_Count(volatile float* count_L, volatile float* count_R);
void App_Encoder_Get_Speed(volatile float* speed_L, volatile float* speed_R);

#endif // APP_ENCODER_H
