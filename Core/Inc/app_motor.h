#ifndef APP_MOTOR_H
#define APP_MOTOR_H

#include "stm32f1xx_hal.h"

void App_motor_init(void);
void App_motor_process(void);
void App_motor_set_speed_L(float speed_L);
void App_motor_set_speed_R(float speed_R);
void App_motor_cmd(uint8_t on);
#endif // APP_MOTOR_H
