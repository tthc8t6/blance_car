#ifndef MPU6500_TEST_H
#define MPU6500_TEST_H

/**
 * @brief  MPU6500 数据打印测试
 * @note   通过串口打印加速度、温度、陀螺仪数据
 *         调用前需确保 MPU6500 已初始化
 */
void MPU6500_Test(void);

void MPU6500_Euler_Test(void);

#endif // MPU6500_TEST_H
