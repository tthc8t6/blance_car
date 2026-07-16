#include "main.h"
#include "tim.h"
#include "app_pwm.h"

void PWM_Test(void)
{
  HAL_GPIO_WritePin(GPIOA,GPIO_PIN_1,GPIO_PIN_SET);
  HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);  // 开启左轮TIM1通道1的PWM输出
  HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_1);  // 开启右轮TIM4通道1的PWM输出
    for (float duty = -100.0f; duty <= 100.0f; duty += 20.0f) {
        App_PWM_Set_L(duty);
        App_PWM_Set_R(duty);
        HAL_Delay(2000);
    }
  HAL_TIM_PWM_Stop(&htim1, TIM_CHANNEL_1);  // 关闭PWM输出
  HAL_TIM_PWM_Stop(&htim4, TIM_CHANNEL_1);  
}
