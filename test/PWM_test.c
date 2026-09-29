#include "main.h"
#include "tim.h"
#include "app_pwm.h"

//
// @简介：PWM测试：使能STBY，占空比从-100%到100%每2s步进20%，同时驱动左右电机，结束后关闭PWM
// @参数：无
// @注意：测试时轮子会转动，请先架空小车
//
void PWM_Test(void)
{
  HAL_GPIO_WritePin(GPIOA,GPIO_PIN_1,GPIO_PIN_SET);
  HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);  // 左轮 TIM1_CH1
  HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_1);  // 右轮 TIM4_CH1
    for (float duty = -100.0f; duty <= 100.0f; duty += 20.0f) {
        App_PWM_Set_L(duty);
        App_PWM_Set_R(duty);
        HAL_Delay(2000);
    }
  HAL_TIM_PWM_Stop(&htim1, TIM_CHANNEL_1);
  HAL_TIM_PWM_Stop(&htim4, TIM_CHANNEL_1);
}
