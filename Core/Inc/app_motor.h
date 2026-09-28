#ifndef APP_MOTOR_H
#define APP_MOTOR_H

#include "stm32f1xx_hal.h"

void App_Motor_Init(void);
void App_Motor_Process(void);
void App_Motor_Set_Speed_L(float omega_speed_L);
void App_Motor_Set_Speed_R(float omega_speed_R);
void App_Motor_Cmd(uint8_t on);
#endif // APP_MOTOR_H
