#include "app_pwm.h"
#include "math.h"
#include "tim.h"
/**
 * @brief 设置左电机PWM占空比
 * @param Duty 占空比，范围-100到100，-100表示反转全速，0表示停止，100表示正转全速
 * @note AN1（PA9）高电压正转，AN2（PA10）低电压反转
 * //顺时针为正转 逆时针为反转
 */
void App_PWM_Set_L(float Duty)
{
    int8_t sign = (Duty >= 0) ? 1 : -1;  // 符号位：1表示正转，-1表示反转
    Duty = fabsf(Duty);  // 取绝对值
    uint32_t ccr = (uint32_t)(Duty / 100.0f * 1000);  // 将占空比转换为定时器的脉冲值，ARR=999
    //由于左右电机相对所以修改sign使一边正传时一边反转 统一行进方向
    if (sign < 0) {
        // 正转：PWM控制AN1高电平，AN2低电平 逆时针
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_9, GPIO_PIN_SET);   // AN1为高电平
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_10, GPIO_PIN_RESET); // AN2为低电平
    } else {
        // 反转：PWM控制AN2高电平，AN1低电平 顺时针
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_10, GPIO_PIN_SET);   // AN2为高电平
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_9, GPIO_PIN_RESET);  // AN1为低电平
    }
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, ccr);
}
/**
 * @brief 设置右电机PWM占空比
 * @param Duty 占空比，范围-100到100，-100表示反转全速，0表示停止，100表示正转全速
 * @note BN1（PB5）高电压正转，BN2（PB7）低电压反转
 */
void App_PWM_Set_R(float Duty)
{
    int8_t sign = (Duty >= 0) ? 1 : -1;  // 符号位：1表示正转，-1表示反转
    Duty = fabsf(Duty);  // 取绝对值
    uint32_t ccr = (uint32_t)(Duty / 100.0f * 1000);  // 将占空比转换为定时器的脉冲值，ARR=999

    if (sign > 0) {
        // 正转：PWM控制BN1高电平，BN2低电平 逆时针
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, GPIO_PIN_SET);   // BN1为高电平
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7, GPIO_PIN_RESET); // BN2为低电平
    } else {
        // 反转：PWM控制BN2高电平，BN1低电平 顺时针
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7, GPIO_PIN_SET);   // BN2为高电平
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, GPIO_PIN_RESET); // BN1为低电平
    }
    __HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_1, ccr);
}

