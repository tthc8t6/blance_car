#include "main.h"
#include "usart.h"
#include "app_encoder.h"
#include <stdio.h>
#include <string.h>

static volatile float count_L = 0; //左轮累计角度 单位度
static volatile float count_R = 0; //右轮累计角度 单位度
static char uart_buf[64];

static volatile float last_pos_L = 0.0f; //上次左轮位置 单位度
static volatile float last_pos_R = 0.0f; //上次右轮位置 单位度

//
// @简介：编码器计数测试：每50ms通过串口打印左右轮累计角度
// @参数：无
//
void Encoder_Test(void)
{
    while (1) {
        HAL_Delay(50);
        App_Encoder_Get_Count(&count_L, &count_R);
        sprintf(uart_buf, "count_L: %f, count_R: %f\r\n", count_L, count_R);
        HAL_UART_Transmit(&huart2, (uint8_t *)uart_buf, strlen(uart_buf), HAL_MAX_DELAY);
    }
}

//
// @简介：M法测速测试：每1ms用位置差除以时间得角速度，并通过串口打印
// @参数：无
//
void Encoder_M_Method_Test(void)
{
    volatile float pos_L = 0.0f;
    volatile float pos_R = 0.0f;

    while (1) {
        HAL_Delay(1);
        App_Encoder_Get_Count(&pos_L, &pos_R);
        float M_l = pos_L - last_pos_L;
        float M_r = pos_R - last_pos_R;
        float omega_deg_L = M_l / 0.001f; //角速度 单位度/s
        float omega_deg_R = M_r / 0.001f; //角速度 单位度/s
        last_pos_L = pos_L;
        last_pos_R = pos_R;
        sprintf(uart_buf, "pos_L: %f, pos_R: %f, omega_deg_L: %f, omega_deg_R: %f\r\n", pos_L, pos_R, omega_deg_L, omega_deg_R);
        HAL_UART_Transmit(&huart2, (uint8_t *)uart_buf, strlen(uart_buf), HAL_MAX_DELAY);
    }
}

//
// @简介：T法测速测试：每1ms读取左右轮角速度，并通过串口打印
// @参数：无
//
void Encoder_T_Method_Test(void)
{
    volatile float omega_speed_L = 0.0f; //单位rad/s
    volatile float omega_speed_R = 0.0f; //单位rad/s

    while (1) {
        HAL_Delay(1);
        App_Encoder_Get_Speed(&omega_speed_L, &omega_speed_R);
        sprintf(uart_buf, "omega_speed_L: %f, omega_speed_R: %f\r\n", omega_speed_L, omega_speed_R);
        HAL_UART_Transmit(&huart2, (uint8_t *)uart_buf, strlen(uart_buf), HAL_MAX_DELAY);
    }
}

