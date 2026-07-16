#ifndef MPU6050_TEST_H
#define MPU6050_TEST_H

/**
 * @brief  MPU6050 数据打印测试
 * @note   通过串口打印加速度、温度、陀螺仪数据
 *         调用前需确保 MPU6050 已初始化
 */
void MPU6050_Test(void);

void MPU6050_Euler_Test(void);

#endif // MPU6050_TEST_H
