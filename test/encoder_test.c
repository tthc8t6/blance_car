#include "main.h"
#include "usart.h"
#include "app_encoder.h"
#include <stdio.h>
#include <string.h>

volatile float count_L = 0;
volatile float count_R = 0;
char uart_buf[64];

static volatile float last_pos_L = 0.0f;
static volatile float last_pos_R = 0.0f;

void Encoder_Test(void)
{
    while (1) {
        HAL_Delay(50); // 每50ms更新一次
        App_Encoder_Get_Count(&count_L, &count_R);
        sprintf(uart_buf, "count_L: %f, count_R: %f\r\n", count_L, count_R);
        HAL_UART_Transmit(&huart2, (uint8_t *)uart_buf, strlen(uart_buf), HAL_MAX_DELAY);
    }
}

void Encoder_M_Method_Test(void)
{
    volatile float pos_L = 0.0f;
    volatile float pos_R = 0.0f;

    while (1) {
        HAL_Delay(1); // 每1ms更新一次
        App_Encoder_Get_Count(&pos_L, &pos_R);
        float M_l = pos_L - last_pos_L;
        float M_r = pos_R - last_pos_R;
        float jiaosudu_L = M_l / 0.001f; // 计算角速度，单位为度/s
        float jiaosudu_R = M_r / 0.001f; // 计算角速度，单位为度/s
        last_pos_L = pos_L;
        last_pos_R = pos_R;
        sprintf(uart_buf, "pos_L: %f, pos_R: %f, jiaosudu_L: %f, jiaosudu_R: %f\r\n", pos_L, pos_R, jiaosudu_L, jiaosudu_R);
        HAL_UART_Transmit(&huart2, (uint8_t *)uart_buf, strlen(uart_buf), HAL_MAX_DELAY);
    }
}

void Encoder_T_Method_Test(void)
{
    volatile float omega_speed_L = 0.0f;
    volatile float omega_speed_R = 0.0f;

    while (1) {
        HAL_Delay(1); // 每1ms更新一次
        App_Encoder_Get_Speed(&omega_speed_L, &omega_speed_R);
        sprintf(uart_buf, "omega_speed_L: %f, omega_speed_R: %f\r\n", omega_speed_L, omega_speed_R);
        HAL_UART_Transmit(&huart2, (uint8_t *)uart_buf, strlen(uart_buf), HAL_MAX_DELAY);
    }
}

