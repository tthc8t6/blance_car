#ifndef APP_PWM_H
#define APP_PWM_H

#include "stm32f1xx_hal.h"

void App_PWM_Set_L(float Duty);

void App_PWM_Set_R(float Duty);

void App_PWM_Cmd(uint8_t on);

void PWM_Test(void);

#endif // APP_PWM_H
