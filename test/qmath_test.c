#include "qmath_test.h"
#include "quick_math.h"
#include "usart.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

/**
 * @brief 数学运算测速函数
 * @note 用于测量各种数学运算的速度，MPU6050的欧拉角解算中需要大量调用三角函数
 *       防止影响MPU6050的切片运行速度
 */
void QMath_Test(void)
{
    char buf[64]={0};
    uint32_t t1, t2;
    volatile float result_f = 0;
    volatile int result_i = 0;
    uint32_t loop_cnt = 1000000;

    sprintf(buf, "\r\n===== 数学运算测速 =====\r\n");
    HAL_UART_Transmit(&huart2, (uint8_t *)buf, strlen(buf), 100);

    // ==================== 整数四则运算 ====================

    // 整数加法测试
    sprintf(buf, "\r\n--- 整数加法测试 ---\r\n");
    HAL_UART_Transmit(&huart2, (uint8_t *)buf, strlen(buf), 100);
    t1 = HAL_GetTick();
    for(uint32_t i = 0; i < loop_cnt; i++)
    {
        result_i = (int)i + 1;
    }
    (void)result_i;
    t2 = HAL_GetTick();
    sprintf(buf, "耗时: %lu ms, 每次: %f us\r\n", (unsigned long)(t2-t1), (t2-t1)/(float)loop_cnt*1000.0f);
    HAL_UART_Transmit(&huart2, (uint8_t *)buf, strlen(buf), 100);

    // 整数减法测试
    sprintf(buf, "--- 整数减法测试 ---\r\n");
    HAL_UART_Transmit(&huart2, (uint8_t *)buf, strlen(buf), 100);
    t1 = HAL_GetTick();
    for(uint32_t i = 0; i < loop_cnt; i++)
    {
        result_i = (int)i - 1;
    }
    (void)result_i;
    t2 = HAL_GetTick();
    sprintf(buf, "耗时: %lu ms, 每次: %f us\r\n", (unsigned long)(t2-t1), (t2-t1)/(float)loop_cnt*1000.0f);
    HAL_UART_Transmit(&huart2, (uint8_t *)buf, strlen(buf), 100);

    // 整数乘法测试
    sprintf(buf, "--- 整数乘法测试 ---\r\n");
    HAL_UART_Transmit(&huart2, (uint8_t *)buf, strlen(buf), 100);
    t1 = HAL_GetTick();
    for(uint32_t i = 0; i < loop_cnt; i++)
    {
        result_i = (int)i * 3;
    }
    (void)result_i;
    t2 = HAL_GetTick();
    sprintf(buf, "耗时: %lu ms, 每次: %f us\r\n", (unsigned long)(t2-t1), (t2-t1)/(float)loop_cnt*1000.0f);
    HAL_UART_Transmit(&huart2, (uint8_t *)buf, strlen(buf), 100);

    // 整数除法测试
    sprintf(buf, "--- 整数除法测试 ---\r\n");
    HAL_UART_Transmit(&huart2, (uint8_t *)buf, strlen(buf), 100);
    t1 = HAL_GetTick();
    for(uint32_t i = 1; i <= loop_cnt; i++)
    {
        result_i = (int)(i * 100) / 3;
    }
    (void)result_i;
    t2 = HAL_GetTick();
    sprintf(buf, "耗时: %lu ms, 每次: %f us\r\n", (unsigned long)(t2-t1), (t2-t1)/(float)loop_cnt*1000.0f);
    HAL_UART_Transmit(&huart2, (uint8_t *)buf, strlen(buf), 100);

    // ==================== 单精度浮点四则运算 ====================

    // 浮点加法测试
    sprintf(buf, "\r\n--- 浮点加法测试 ---\r\n");
    HAL_UART_Transmit(&huart2, (uint8_t *)buf, strlen(buf), 100);
    t1 = HAL_GetTick();
    for(uint32_t i = 0; i < loop_cnt; i++)
    {
        result_f = (float)i + 1.23f;
    }
    (void)result_f;
    t2 = HAL_GetTick();
    sprintf(buf, "耗时: %lu ms, 每次: %f us\r\n", (unsigned long)(t2-t1), (t2-t1)/(float)loop_cnt*1000.0f);
    HAL_UART_Transmit(&huart2, (uint8_t *)buf, strlen(buf), 100);

    // 浮点减法测试
    sprintf(buf, "--- 浮点减法测试 ---\r\n");
    HAL_UART_Transmit(&huart2, (uint8_t *)buf, strlen(buf), 100);
    t1 = HAL_GetTick();
    for(uint32_t i = 0; i < loop_cnt; i++)
    {
        result_f = (float)i - 1.23f;
    }
    (void)result_f;
    t2 = HAL_GetTick();
    sprintf(buf, "耗时: %lu ms, 每次: %f us\r\n", (unsigned long)(t2-t1), (t2-t1)/(float)loop_cnt*1000.0f);
    HAL_UART_Transmit(&huart2, (uint8_t *)buf, strlen(buf), 100);

    // 浮点乘法测试
    sprintf(buf, "--- 浮点乘法测试 ---\r\n");
    HAL_UART_Transmit(&huart2, (uint8_t *)buf, strlen(buf), 100);
    t1 = HAL_GetTick();
    for(uint32_t i = 0; i < loop_cnt; i++)
    {
        result_f = (float)i * 3.14f;
    }
    (void)result_f;
    t2 = HAL_GetTick();
    sprintf(buf, "耗时: %lu ms, 每次: %f us\r\n", (unsigned long)(t2-t1), (t2-t1)/(float)loop_cnt*1000.0f);
    HAL_UART_Transmit(&huart2, (uint8_t *)buf, strlen(buf), 100);

    // 浮点除法测试
    sprintf(buf, "--- 浮点除法测试 ---\r\n");
    HAL_UART_Transmit(&huart2, (uint8_t *)buf, strlen(buf), 100);
    t1 = HAL_GetTick();
    for(uint32_t i = 1; i <= loop_cnt; i++)
    {
        result_f = (float)i / 3.14f;
    }
    (void)result_f;
    t2 = HAL_GetTick();
    sprintf(buf, "耗时: %lu ms, 每次: %f us\r\n", (unsigned long)(t2-t1), (t2-t1)/(float)loop_cnt*1000.0f);
    HAL_UART_Transmit(&huart2, (uint8_t *)buf, strlen(buf), 100);

    // ==================== 三角函数 / 反三角函数 ====================

    // sinf 测试
    sprintf(buf, "\r\n--- sinf 测试 ---\r\n");
    HAL_UART_Transmit(&huart2, (uint8_t *)buf, strlen(buf), 100);
    t1 = HAL_GetTick();
    for(uint32_t i = 0; i < loop_cnt; i++)
    {
        result_f = sinf((float)i * 0.001f);
    }
    (void)result_f;
    t2 = HAL_GetTick();
    sprintf(buf, "耗时: %lu ms, 每次: %f us\r\n", (unsigned long)(t2-t1), (t2-t1)/(float)loop_cnt*1000.0f);
    HAL_UART_Transmit(&huart2, (uint8_t *)buf, strlen(buf), 100);

    // cosf 测试
    sprintf(buf, "--- cosf 测试 ---\r\n");
    HAL_UART_Transmit(&huart2, (uint8_t *)buf, strlen(buf), 100);
    t1 = HAL_GetTick();
    for(uint32_t i = 0; i < loop_cnt; i++)
    {
        result_f = cosf((float)i * 0.001f);
    }
    (void)result_f;
    t2 = HAL_GetTick();
    sprintf(buf, "耗时: %lu ms, 每次: %f us\r\n", (unsigned long)(t2-t1), (t2-t1)/(float)loop_cnt*1000.0f);
    HAL_UART_Transmit(&huart2, (uint8_t *)buf, strlen(buf), 100);

    // tanf 测试
    sprintf(buf, "--- tanf 测试 ---\r\n");
    HAL_UART_Transmit(&huart2, (uint8_t *)buf, strlen(buf), 100);
    t1 = HAL_GetTick();
    for(uint32_t i = 0; i < loop_cnt; i++)
    {
        result_f = tanf((float)i * 0.001f);
    }
    (void)result_f;
    t2 = HAL_GetTick();
    sprintf(buf, "耗时: %lu ms, 每次: %f us\r\n", (unsigned long)(t2-t1), (t2-t1)/(float)loop_cnt*1000.0f);
    HAL_UART_Transmit(&huart2, (uint8_t *)buf, strlen(buf), 100);

    // atanf 测试
    sprintf(buf, "--- atanf 测试 ---\r\n");
    HAL_UART_Transmit(&huart2, (uint8_t *)buf, strlen(buf), 100);
    t1 = HAL_GetTick();
    for(uint32_t i = 0; i < loop_cnt; i++)
    {
        result_f = atanf((float)i * 0.001f);
    }
    (void)result_f;
    t2 = HAL_GetTick();
    sprintf(buf, "耗时: %lu ms, 每次: %f us\r\n", (unsigned long)(t2-t1), (t2-t1)/(float)loop_cnt*1000.0f);
    HAL_UART_Transmit(&huart2, (uint8_t *)buf, strlen(buf), 100);

    // atan2f 测试
    sprintf(buf, "--- atan2f 测试 ---\r\n");
    HAL_UART_Transmit(&huart2, (uint8_t *)buf, strlen(buf), 100);
    t1 = HAL_GetTick();
    for(uint32_t i = 1; i <= loop_cnt; i++)
    {
        result_f = atan2f((float)i, (float)(i + 1));
    }
    (void)result_f;
    t2 = HAL_GetTick();
    sprintf(buf, "耗时: %lu ms, 每次: %f us\r\n", (unsigned long)(t2-t1), (t2-t1)/(float)loop_cnt*1000.0f);
    HAL_UART_Transmit(&huart2, (uint8_t *)buf, strlen(buf), 100);

    // ==================== 测试完成 ====================
    sprintf(buf, "\r\n===== 测试完成 =====\r\n");
    HAL_UART_Transmit(&huart2, (uint8_t *)buf, strlen(buf), 100);

    // while(1);
}

/**
 * @brief 快速查表法数学运算测速函数
 * @note 用于测量 quick_math.c 中查表法三角函数的速度，
 *       并与标准库 math.h 的三角函数进行对比
 */
void QMath_Tab_Speed_Test(void)
{
    char buf[64]={0};
    uint32_t t1, t2;
    volatile float result_f = 0;
    uint32_t loop_cnt = 1000000;

    sprintf(buf, "\r\n===== 快速查表法运算测速 =====\r\n");
    HAL_UART_Transmit(&huart2, (uint8_t *)buf, strlen(buf), 100);

    // ==================== qsin 测试 ====================
    sprintf(buf, "\r\n--- qsin 测试 ---\r\n");
    HAL_UART_Transmit(&huart2, (uint8_t *)buf, strlen(buf), 100);
    t1 = HAL_GetTick();
    for(uint32_t i = 0; i < loop_cnt; i++)
    {
        result_f = qsin((float)i * 0.001f);
    }
    (void)result_f;
    t2 = HAL_GetTick();
    sprintf(buf, "耗时: %lu ms, 每次: %f us\r\n", (unsigned long)(t2-t1), (t2-t1)/(float)loop_cnt*1000.0f);
    HAL_UART_Transmit(&huart2, (uint8_t *)buf, strlen(buf), 100);

    // ==================== qcos 测试 ====================
    sprintf(buf, "--- qcos 测试 ---\r\n");
    HAL_UART_Transmit(&huart2, (uint8_t *)buf, strlen(buf), 100);
    t1 = HAL_GetTick();
    for(uint32_t i = 0; i < loop_cnt; i++)
    {
        result_f = qcos((float)i * 0.001f);
    }
    (void)result_f;
    t2 = HAL_GetTick();
    sprintf(buf, "耗时: %lu ms, 每次: %f us\r\n", (unsigned long)(t2-t1), (t2-t1)/(float)loop_cnt*1000.0f);
    HAL_UART_Transmit(&huart2, (uint8_t *)buf, strlen(buf), 100);

    // ==================== qtan 测试 ====================
    sprintf(buf, "--- qtan 测试 ---\r\n");
    HAL_UART_Transmit(&huart2, (uint8_t *)buf, strlen(buf), 100);
    t1 = HAL_GetTick();
    for(uint32_t i = 0; i < loop_cnt; i++)
    {
        result_f = qtan((float)i * 0.001f);
    }
    (void)result_f;
    t2 = HAL_GetTick();
    sprintf(buf, "耗时: %lu ms, 每次: %f us\r\n", (unsigned long)(t2-t1), (t2-t1)/(float)loop_cnt*1000.0f);
    HAL_UART_Transmit(&huart2, (uint8_t *)buf, strlen(buf), 100);

    // ==================== qasin 测试 ====================
    sprintf(buf, "--- qasin 测试 ---\r\n");
    HAL_UART_Transmit(&huart2, (uint8_t *)buf, strlen(buf), 100);
    t1 = HAL_GetTick();
    for(uint32_t i = 0; i < loop_cnt; i++)
    {
        // asin输入范围[-1, 1]，映射到 [-0.999, 0.999]
        float x = (float)(i % 2000) / 1000.0f - 1.0f;
        result_f = qasin(x);
    }
    (void)result_f;
    t2 = HAL_GetTick();
    sprintf(buf, "耗时: %lu ms, 每次: %f us\r\n", (unsigned long)(t2-t1), (t2-t1)/(float)loop_cnt*1000.0f);
    HAL_UART_Transmit(&huart2, (uint8_t *)buf, strlen(buf), 100);

    // ==================== qacos 测试 ====================
    sprintf(buf, "--- qacos 测试 ---\r\n");
    HAL_UART_Transmit(&huart2, (uint8_t *)buf, strlen(buf), 100);
    t1 = HAL_GetTick();
    for(uint32_t i = 0; i < loop_cnt; i++)
    {
        float x = (float)(i % 2000) / 1000.0f - 1.0f;
        result_f = qacos(x);
    }
    (void)result_f;
    t2 = HAL_GetTick();
    sprintf(buf, "耗时: %lu ms, 每次: %f us\r\n", (unsigned long)(t2-t1), (t2-t1)/(float)loop_cnt*1000.0f);
    HAL_UART_Transmit(&huart2, (uint8_t *)buf, strlen(buf), 100);

    // ==================== qatan 测试 ====================
    sprintf(buf, "--- qatan 测试 ---\r\n");
    HAL_UART_Transmit(&huart2, (uint8_t *)buf, strlen(buf), 100);
    t1 = HAL_GetTick();
    for(uint32_t i = 0; i < loop_cnt; i++)
    {
        result_f = qatan((float)i * 0.001f);
    }
    (void)result_f;
    t2 = HAL_GetTick();
    sprintf(buf, "耗时: %lu ms, 每次: %f us\r\n", (unsigned long)(t2-t1), (t2-t1)/(float)loop_cnt*1000.0f);
    HAL_UART_Transmit(&huart2, (uint8_t *)buf, strlen(buf), 100);

    // ==================== qatan2 测试 ====================
    sprintf(buf, "--- qatan2 测试 ---\r\n");
    HAL_UART_Transmit(&huart2, (uint8_t *)buf, strlen(buf), 100);
    t1 = HAL_GetTick();
    for(uint32_t i = 1; i <= loop_cnt; i++)
    {
        result_f = qatan2((float)i, (float)(i + 1));
    }
    (void)result_f;
    t2 = HAL_GetTick();
    sprintf(buf, "耗时: %lu ms, 每次: %f us\r\n", (unsigned long)(t2-t1), (t2-t1)/(float)loop_cnt*1000.0f);
    HAL_UART_Transmit(&huart2, (uint8_t *)buf, strlen(buf), 100);

    // ==================== 测试完成 ====================
    sprintf(buf, "\r\n===== 快速查表法测试完成 =====\r\n");
    HAL_UART_Transmit(&huart2, (uint8_t *)buf, strlen(buf), 100);

    while(1);
}
