#include "app_pwm.h"
#include "math.h"
#include "tim.h"
#include "stm32f1xx_hal.h"

//
// @简介：设置左电机PWM占空比与转向（TIM1_CH1，方向脚PA9/PA10）
// @参数：Duty - 占空比，范围-100~100，负数反转，0停止，超范围自动钳位
//
void App_PWM_Set_L(float Duty)
{
    //钳位到[-100,100]，防止float转uint32溢出
    if (Duty != Duty) {           // NaN与任何值比较都为假，需单独判断
        Duty = 0.0f;
    } else if (Duty > 100.0f) {
        Duty = 100.0f;
    } else if (Duty < -100.0f) {
        Duty = -100.0f;
    }

    int8_t sign = (Duty >= 0) ? 1 : -1;  // 符号位：1表示正转，-1表示反转
    Duty = fabsf(Duty);
    uint32_t ccr = (uint32_t)(Duty / 100.0f * 1000);  // ARR=999
    //左右电机相对安装，左轮方向取反，使正值对应同一行进方向
    if (sign < 0) {
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_9, GPIO_PIN_SET);   // AN1为高电平
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_10, GPIO_PIN_RESET); // AN2为低电平
    } else {
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_10, GPIO_PIN_SET);   // AN2为高电平
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_9, GPIO_PIN_RESET);  // AN1为低电平
    }
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, ccr);
}

//
// @简介：设置右电机PWM占空比与转向（TIM4_CH1，方向脚PB5/PB7）
// @参数：Duty - 占空比，范围-100~100，负数反转，0停止，超范围自动钳位
//
void App_PWM_Set_R(float Duty)
{
    if (Duty != Duty) {           // NaN
        Duty = 0.0f;
    } else if (Duty > 100.0f) {
        Duty = 100.0f;
    } else if (Duty < -100.0f) {
        Duty = -100.0f;
    }

    int8_t sign = (Duty >= 0) ? 1 : -1;  // 符号位：1表示正转，-1表示反转
    Duty = fabsf(Duty);
    uint32_t ccr = (uint32_t)(Duty / 100.0f * 1000);  // ARR=999

    if (sign > 0) {
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, GPIO_PIN_SET);   // BN1为高电平
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7, GPIO_PIN_RESET); // BN2为低电平
    } else {
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7, GPIO_PIN_SET);   // BN2为高电平
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, GPIO_PIN_RESET); // BN1为低电平
    }
    __HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_1, ccr);
}

//
// @简介：控制TB6612驱动芯片的STBY引脚（PA1），决定电机是否能被驱动
// @参数：on - 1表示使能输出，0表示待机
//
void App_PWM_Cmd(uint8_t on)
{
    if (on) {
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, GPIO_PIN_SET);
    } else {
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, GPIO_PIN_RESET);
    }
}
