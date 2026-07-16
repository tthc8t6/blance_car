#include "mpu6050_test.h"
#include "app_mpu6050.h"
#include "usart.h"
#include <stdio.h>
#include <string.h>
#include "task.h"

void MPU6050_Test(void)
{
    char buf[64]={0};
    App_MPU6050_Init();
    
    while (1)
    {
        App_MPU6050_Update();
        
        float ax = App_MPU6050_Get_Accel_X();
        float ay = App_MPU6050_Get_Accel_Y();
        float az = App_MPU6050_Get_Accel_Z();
        float temp = App_MPU6050_Get_Temperature();
        float gx = App_MPU6050_Get_Gyro_X();
        float gy = App_MPU6050_Get_Gyro_Y();
        float gz = App_MPU6050_Get_Gyro_Z();
        
        sprintf(buf,"%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f\n",ax,ay,az,temp,gx,gy,gz);
        
        HAL_UART_Transmit(&huart2, (uint8_t *)buf, strlen(buf), 100);
        
        HAL_Delay(10);
    }
}

static void USART2_Proc(void)
{
  PERIODIC(10) // 每10ms发送一次数据
  float ax = App_MPU6050_Get_Accel_X();
  float ay = App_MPU6050_Get_Accel_Y();
  float az = App_MPU6050_Get_Accel_Z();

  float  temp = App_MPU6050_Get_Temperature();

  float gx = App_MPU6050_Get_Gyro_X();
  float gy = App_MPU6050_Get_Gyro_Y();
  float gz = App_MPU6050_Get_Gyro_Z();

  float yaw = App_MPU6050_Get_Yaw();
  float pitch = App_MPU6050_Get_Pitch();
  float roll = App_MPU6050_Get_Roll();

  char buf[200]={0};  // 10个float用%.2f格式需要~160字节，必须足够大
  sprintf(buf, "%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f\n", ax, ay, az, temp, gx, gy, gz, yaw, pitch, roll);
  HAL_UART_Transmit(&huart2, (uint8_t *)buf, strlen(buf), 100);
}

//测试欧拉角
void MPU6050_Euler_Test(void)
{
    App_MPU6050_Init();
    
    while (1)
    {
        if (App_MPU6050_Update() == HAL_OK)
        {
            APP_MPU6050_Proc(); // 更新欧拉角计算
        }
        USART2_Proc(); // 通过串口发送数据
    }
}


