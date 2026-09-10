#include "stm32f1xx_hal.h"
#include <stdio.h>
#include "usart.h"
#include "string.h"
#include "app_motor.h"
#include "app_encoder.h"
#include "pid.h"
#include "task.h"


static float target_speed = 0.0f; // 目标速度，单位为弧度每秒
static char uart_buf[64];


static void USART2_Process(void)
{
  PERIODIC(10); // 每10ms执行一次，Firewater协议：逗号分隔的十进制数，\n结尾，供VOFA+画图
  float speed_L, speed_R;
  App_Encoder_Get_Speed(&speed_L, &speed_R);
  sprintf(uart_buf, "%.3f,%.3f,%.3f\n", target_speed, speed_L, speed_R);
  HAL_UART_Transmit(&huart2, (uint8_t*)uart_buf, strlen(uart_buf), 100);
}

//注意：临时测试期间避免轮子直接转动！！！
void Pid_motor_Test(void) 
{
  target_speed = (HAL_GetTick() / 1000) % 10 * 2.0f; // 目标速度每秒增加2弧度，10秒后回到0
  App_motor_set_speed_L(target_speed); // 设置左电机目标速度
  App_motor_set_speed_R(target_speed); // 设置右电机目标速度

  USART2_Process(); // 发送速度数据到VOFA+进行绘图
}

