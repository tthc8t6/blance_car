#include "mpu6500_test.h"
#include "app_mpu6500.h"
#include "usart.h"
#include <stdio.h>
#include <string.h>
#include "task.h"

//
// @简介：MPU6500数据打印测试：初始化后每10ms通过串口打印加速度、温度、角速度
// @参数：无
//
void MPU6500_Test(void)
{
    char buf[64]={0};
    App_MPU6500_Init();

    while (1)
    {
        App_MPU6500_Update();

        float ax = App_MPU6500_Get_Accel_X(); //加速度 单位g
        float ay = App_MPU6500_Get_Accel_Y();
        float az = App_MPU6500_Get_Accel_Z();
        float temp = App_MPU6500_Get_Temperature(); //温度 单位℃
        float gx = App_MPU6500_Get_Gyro_X(); //角速度 单位°/s
        float gy = App_MPU6500_Get_Gyro_Y();
        float gz = App_MPU6500_Get_Gyro_Z();

        sprintf(buf,"%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f\n",ax,ay,az,temp,gx,gy,gz);

        HAL_UART_Transmit(&huart2, (uint8_t *)buf, strlen(buf), 100);

        HAL_Delay(10);
    }
}

//
// @简介：每10ms通过串口发送加速度、温度、角速度和欧拉角
// @参数：无
//
static void USART2_Proc(void)
{
  PERIODIC(10)
  float ax = App_MPU6500_Get_Accel_X(); //加速度 单位g
  float ay = App_MPU6500_Get_Accel_Y();
  float az = App_MPU6500_Get_Accel_Z();

  float  temp = App_MPU6500_Get_Temperature(); //温度 单位℃

  float gx = App_MPU6500_Get_Gyro_X(); //角速度 单位°/s
  float gy = App_MPU6500_Get_Gyro_Y();
  float gz = App_MPU6500_Get_Gyro_Z();

  float yaw = App_MPU6500_Get_Yaw(); //欧拉角 单位度
  float pitch = App_MPU6500_Get_Pitch();
  float roll = App_MPU6500_Get_Roll();

  char buf[200]={0};  // 10个float用%.2f格式需要约160字节
  sprintf(buf, "%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f\n", ax, ay, az, temp, gx, gy, gz, yaw, pitch, roll);
  HAL_UART_Transmit(&huart2, (uint8_t *)buf, strlen(buf), 100);
}

//
// @简介：欧拉角测试：循环解算姿态并通过串口发送
// @参数：无
//
void MPU6500_Euler_Test(void)
{
    App_MPU6500_Init();

    while (1)
    {
        App_MPU6500_Process(); //自己限速5ms，内部已包含Update
        USART2_Proc();
    }
}

