#include "stm32f1xx_hal.h"
#include <stdio.h>
#include "usart.h"
#include "string.h"
#include "app_motor.h"
#include "app_encoder.h"
#include "pid.h"
#include "task.h"


static float target_speed = 0.0f; // 目标速度 单位rad/s
static char uart_buf[64];


//
// @简介：每10ms通过串口发送目标速度和左右轮实际速度，供VOFA+画图
// @参数：无
// @注意：Firewater协议：逗号分隔的十进制数，\n结尾
//
static void USART2_Process(void)
{
  PERIODIC(10)
  float speed_L, speed_R; //单位rad/s
  App_Encoder_Get_Speed(&speed_L, &speed_R);
  sprintf(uart_buf, "%.3f,%.3f,%.3f\n", target_speed, speed_L, speed_R);
  HAL_UART_Transmit(&huart2, (uint8_t*)uart_buf, strlen(uart_buf), 100);
}

//
// @简介：电机转速环测试：目标速度每秒增加2rad/s，10秒后回到0，同时绘图观察跟踪效果
// @参数：无
// @注意：临时测试期间避免轮子直接转动！！！
//
void PID_Motor_Test(void)
{
  target_speed = (HAL_GetTick() / 1000) % 10 * 2.0f;
  App_Motor_Set_Speed_L(target_speed);
  App_Motor_Set_Speed_R(target_speed);

  USART2_Process();
}

