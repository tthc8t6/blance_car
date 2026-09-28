#ifndef APP_MPU6500_H
#define APP_MPU6500_H

#include "main.h"
#include <stdint.h>

/* ======================== MPU6500 寄存器地址定义 ======================== */

/* MPU6500 7位 I2C 地址 = 0b1101000 (0x68), AD0 接 GND 时为 0x68 */
#define MPU6500_ADDR             (0x68 << 1)  // HAL 库需要左移1位 → 0xD0

/* 寄存器地址 (仅保留实际使用的) */
#define MPU6500_REG_SMPLRT_DIV   0x19  // 采样率分频
#define MPU6500_REG_CONFIG       0x1A  // 配置寄存器 (DLPF)
#define MPU6500_REG_GYRO_CONFIG  0x1B  // 陀螺仪配置
#define MPU6500_REG_ACCEL_CONFIG 0x1C  // 加速度计配置
#define MPU6500_REG_ACCEL_CONFIG2 0x1D // 加速度计配置2 (MPU6500 独有, MPU6050 无此寄存器)
#define MPU6500_REG_ACCEL_XOUT_H 0x3B  // 加速度计数据起始地址 (连续读取14字节)
#define MPU6500_REG_PWR_MGMT_1   0x6B  // 电源管理1
#define MPU6500_REG_WHO_AM_I     0x75  // 设备ID (MPU6500 返回 0x70)

/* 数字低通滤波器 (CONFIG bit2:0 / ACCEL_CONFIG2 bit2:0) */
#define MPU6500_DLPF_92HZ        0x02  // 陀螺仪 92Hz  / 延迟 3.9ms
#define MPU6500_ACCEL_DLPF_92HZ  0x02  // 加速度计 92Hz / 延迟 7.8ms

/* 陀螺仪量程 (仅保留实际使用的) */
#define MPU6500_GYRO_FS_2000     0x18  // ±2000°/s, 灵敏度 16.4 LSB/°/s

/* 加速度计量程 (仅保留实际使用的) */
#define MPU6500_ACCEL_FS_2G      0x00  // ±2g, 灵敏度 16384 LSB/g

/* ======================== 数据结构体 ======================== */

/**
 * @brief MPU6500 原始数据结构体
 */
typedef struct {
    int16_t accel_x;  // 加速度计 X 轴原始值
    int16_t accel_y;  // 加速度计 Y 轴原始值
    int16_t accel_z;  // 加速度计 Z 轴原始值
    int16_t temp;     // 温度原始值
    int16_t gyro_x;   // 陀螺仪 X 轴原始值
    int16_t gyro_y;   // 陀螺仪 Y 轴原始值
    int16_t gyro_z;   // 陀螺仪 Z 轴原始值
} MPU6500_RawData_t;

/**
 * @brief MPU6500 物理量数据结构体 (转换后的实际值)
 */
typedef struct {
    float accel_x;    // X轴加速度 (g)
    float accel_y;    // Y轴加速度 (g)
    float accel_z;    // Z轴加速度 (g)
    float temp;       // 温度 (°C)
    float gyro_x;     // X轴角速度 (°/s)
    float gyro_y;     // Y轴角速度 (°/s)
    float gyro_z;     // Z轴角速度 (°/s)
} MPU6500_Data_t;

/* ======================== 函数声明 ======================== */

/**
 * @brief  向 MPU6500 写入一个字节
 * @param  reg: 寄存器地址
 * @param  data: 要写入的数据
 * @retval HAL_StatusTypeDef: HAL 状态
 */
HAL_StatusTypeDef App_MPU6500_WriteByte(uint8_t reg, uint8_t data);

/**
 * @brief  从 MPU6500 连续读取多个字节
 * @param  reg: 起始寄存器地址
 * @param  buf: 数据缓冲区指针
 * @param  len: 读取字节数
 * @retval HAL_StatusTypeDef: HAL 状态
 */
HAL_StatusTypeDef App_MPU6500_ReadBytes(uint8_t reg, uint8_t *buf, uint16_t len);

/**
 * @brief  初始化 MPU6500
 * @retval HAL_StatusTypeDef: HAL_OK 表示成功
 */
HAL_StatusTypeDef App_MPU6500_Init(void);

/**
 * @brief  读取 MPU6500 原始数据 (全部7个寄存器, 共14字节)
 * @param  data: 原始数据结构体指针
 * @retval HAL_StatusTypeDef: HAL 状态
 */
HAL_StatusTypeDef App_MPU6500_ReadRaw(MPU6500_RawData_t *data);

/**
 * @brief  读取 MPU6500 数据并转换为物理量
 * @param  data: 物理量数据结构体指针
 * @retval HAL_StatusTypeDef: HAL 状态
 */
static HAL_StatusTypeDef App_MPU6500_ReadData(MPU6500_Data_t *data);

/**
 * @brief  更新内部数据缓存 (无参数版本，供外部调用)
 * @retval HAL_StatusTypeDef: HAL_OK 表示成功
 * @note   调用后可通过 Get_xxx 函数获取最新数据
 */
HAL_StatusTypeDef App_MPU6500_Update(void);

float App_MPU6500_Get_Temperature(void);
float App_MPU6500_Get_Accel_X(void);
float App_MPU6500_Get_Accel_Y(void);
float App_MPU6500_Get_Accel_Z(void);
float App_MPU6500_Get_Gyro_X(void);
float App_MPU6500_Get_Gyro_Y(void);
float App_MPU6500_Get_Gyro_Z(void);

//6500的进程函数 
void App_MPU6500_Process(void);

float App_MPU6500_Get_Yaw(void);
float App_MPU6500_Get_Pitch(void);
float App_MPU6500_Get_Roll(void);

#endif // APP_MPU6500_H
