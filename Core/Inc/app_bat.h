#ifndef APP_BAT_H
#define APP_BAT_H
#include "stm32f1xx_hal.h"

void App_Bat_Start(void);
float App_Get_Bat_Voltage(void);

#endif // APP_BAT_H
