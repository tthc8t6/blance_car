#include "app_mpu6500.h"
#include "i2c.h"
#include "usart.h"
#include <string.h>
#include "task.h"
#include "quick_math.h"


/* ======================== 私有辅助函数 ======================== */
/** @brief MPU6500 数据缓存, 供 getter 函数使用 */
static MPU6500_Data_t mpu6500_data;

/* ======================== I2C 总线恢复函数 ======================== */

/**
 * @brief  恢复 I2C 总线 (当 SDA 被卡住时)
 * @note   通过手动翻转 SCL 时钟线释放 SDA
 *         快速旋转/振动可能导致 I2C 信号完整性问题，SDA 被从设备拉低卡住
 *         此函数通过 GPIO 手动模拟 9 个时钟脉冲，迫使从设备释放 SDA
 */
static void App_I2C_Recovery(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = GPIO_PIN_8;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    for (int i = 0; i < 9; i++)
    {
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_8, GPIO_PIN_RESET);
        for (volatile int d = 0; d < 50; d++);
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_8, GPIO_PIN_SET);
        for (volatile int d = 0; d < 50; d++);
    }

    HAL_I2C_DeInit(&hi2c1);
    MX_I2C1_Init();
}

/* ======================== I2C 基础读写函数 ======================== */

/**
 * @brief  向 MPU6500 指定寄存器写入一个字节
 * @param  reg:  寄存器地址
 * @param  data: 要写入的数据
 * @retval HAL_StatusTypeDef
 * @note   I2C 传输时序: [START][ADDR+W][REG][DATA][STOP]
 *         超时时间 100ms
 */
static HAL_StatusTypeDef App_MPU6500_WriteByte(uint8_t reg, uint8_t data)
{
    return HAL_I2C_Mem_Write(&hi2c1,            // I2C 句柄
                             MPU6500_ADDR,       // 设备地址 (7位左移后)
                             reg,                // 寄存器地址
                             I2C_MEMADD_SIZE_8BIT, // 寄存器地址宽度 8 位
                             &data,              // 数据指针
                             1,                  // 写入 1 个字节
                             100);               // 超时 100ms
}

/**
 * @brief  从 MPU6500 指定寄存器连续读取多个字节
 * @param  reg: 起始寄存器地址
 * @param  buf: 数据缓冲区指针
 * @param  len: 要读取的字节数
 * @retval HAL_StatusTypeDef
 * @note   MPU6500 支持地址自增的连续读取, 读取多个寄存器时效率更高
 */
static HAL_StatusTypeDef App_MPU6500_ReadBytes(uint8_t reg, uint8_t *buf, uint16_t len)
{
    return HAL_I2C_Mem_Read(&hi2c1,
                            MPU6500_ADDR,
                            reg,
                            I2C_MEMADD_SIZE_8BIT,
                            buf,
                            len,
                            2);   /* 400kHz 下 14 字节突发仅需 0.39ms, 2ms 是 5 倍余量。
                                     原来的 100ms 会让失败路径(超时+恢复+重试)阻塞约 200ms */
}

/* ======================== MPU6500 设备检测与初始化 ======================== */

/**
 * @brief  初始化 MPU6500
 * @retval HAL_OK: 初始化成功
 * @note   初始化流程:
 *         1. 复位设备 (PWR_MGMT_1 bit7)
 *         2. 唤醒, 选择时钟源为 PLL with X-axis gyroscope
 *         3. 配置陀螺仪量程 ±2000°/s
 *         4. 配置加速度计量程 ±2g
 *         5. 采样率 = 1kHz / (1+0) = 1kHz
 */
HAL_StatusTypeDef App_MPU6500_Init(void)
{
    HAL_StatusTypeDef status;

    /* 第一步: 复位 MPU6500 */
    /* MPU6500_REG_PWR_MGMT_1 = 0x6B (电源管理1), bit7=DEVICE_RESET(复位), bit2:0=CLKSEL(时钟源) */
    status = App_MPU6500_WriteByte(MPU6500_REG_PWR_MGMT_1, 0x80);
    if (status != HAL_OK) return status;

    /* 等待复位完成 (约 100ms) */
    HAL_Delay(100);

    /* 第二步: 唤醒, 选择最佳时钟源 */
    /* PWR_MGMT_1 写入 0x01: bit7=0(不清复位), bit2:0=001
       MPU6500 手册: CLKSEL=1~5 均为"自动选择最佳时钟源, PLL 就绪则用 PLL" */
    status = App_MPU6500_WriteByte(MPU6500_REG_PWR_MGMT_1, 0x01);
    if (status != HAL_OK) return status;

    /* 第三步: 配置输出数据率 (ODR) = 1kHz */
    /* SMPLRT_DIV = 0 → ODR = 1kHz/(1+0) = 1kHz
       注意: ODR 设得比轮询率(200Hz)高不是浪费 —— 寄存器每 1ms 刷新一次,
       Proc 每 5ms 来取时数据最多只陈旧 1ms; 若设成 200Hz 则最多陈旧 5ms。
       生效条件(手册 p.12): FCHOICE_B=00 且 0<DLPF_CFG<7, 下面两步均满足 */
    status = App_MPU6500_WriteByte(MPU6500_REG_SMPLRT_DIV, 0x00);
    if (status != HAL_OK) return status;

    /* 第四步: 陀螺仪数字低通滤波器 */
    /* CONFIG bit2:0 = DLPF_CFG = 2 → 陀螺仪带宽 92Hz, 群延迟 3.9ms (手册 p.14) */
    status = App_MPU6500_WriteByte(MPU6500_REG_CONFIG, MPU6500_DLPF_92HZ);
    if (status != HAL_OK) return status;

        /* 第五步: 加速度计量程 ±2g */
    /* MPU6500_REG_ACCEL_CONFIG = 0x1C, bit4:3=AFS_SEL, FS_2G=0x00(bit4:3=00), 灵敏度16384 LSB/g */
    status = App_MPU6500_WriteByte(MPU6500_REG_ACCEL_CONFIG, MPU6500_ACCEL_FS_2G);
    if (status != HAL_OK) return status;

    /* 第六步: 陀螺仪量程 ±2000°/s */
    /* MPU6500_REG_GYRO_CONFIG = 0x1B, bit4:3=FS_SEL=11(±2000dps), bit1:0=FCHOICE_B=00(启用DLPF)
       灵敏度 16.4 LSB/(°/s) */
    status = App_MPU6500_WriteByte(MPU6500_REG_GYRO_CONFIG, MPU6500_GYRO_FS_2000);
    if (status != HAL_OK) return status;

    /* 第七步: 加速度计数字低通滤波器 (MPU6500 独有, MPU6050 没有这个寄存器) */
    /* ACCEL_CONFIG2 bit3=ACCEL_FCHOICE_B=0, bit2:0=A_DLPF_CFG=2 → 92Hz, 延迟 7.8ms (手册 p.15)
       不写则保持复位值 0x00 = 460Hz 带宽, 电机振动噪声会大量进入互补滤波的加速度计支路 */
    status = App_MPU6500_WriteByte(MPU6500_REG_ACCEL_CONFIG2, MPU6500_ACCEL_DLPF_92HZ);
    if (status != HAL_OK) return status;

    return HAL_OK;
}

/* ======================== 数据读取函数 ======================== */

/**
 * @brief  读取 MPU6500 全部原始数据 (一次性读取 14 字节)
 * @param  data: 原始数据结构体指针
 * @retval HAL_StatusTypeDef
 * @note   从 ACCEL_XOUT_H (0x3B) 开始连续读取 14 字节:
 *         [AX_H][AX_L][AY_H][AY_L][AZ_H][AZ_L][T_H][T_L]
 *         [GX_H][GX_L][GY_H][GY_L][GZ_H][GZ_L]
 *         数据为大端序 (高字节在前), 需要合并为 16 位有符号数
 */
static HAL_StatusTypeDef App_MPU6500_ReadRaw(MPU6500_RawData_t *data)
{
    uint8_t buf[14];
    HAL_StatusTypeDef status;

    /* 从 0x3B 开始连续读取 14 字节 */
    status = App_MPU6500_ReadBytes(MPU6500_REG_ACCEL_XOUT_H, buf, 14);
    if (status != HAL_OK)
    {
        return status;
    }

    /* 合并高低字节 (大端序 → 小端序), 组成 16 位有符号整数 |是或 运算法则是相同为0不同为1*/
    data->accel_x = (int16_t)((buf[0]  << 8) | buf[1]);   // 0x3B, 0x3C
    data->accel_y = (int16_t)((buf[2]  << 8) | buf[3]);   // 0x3D, 0x3E
    data->accel_z = (int16_t)((buf[4]  << 8) | buf[5]);   // 0x3F, 0x40
    data->temp    = (int16_t)((buf[6]  << 8) | buf[7]);   // 0x41, 0x42
    data->gyro_x  = (int16_t)((buf[8]  << 8) | buf[9]);   // 0x43, 0x44
    data->gyro_y  = (int16_t)((buf[10] << 8) | buf[11]);  // 0x45, 0x46
    data->gyro_z  = (int16_t)((buf[12] << 8) | buf[13]);  // 0x47, 0x48

    return HAL_OK;
}

static HAL_StatusTypeDef App_MPU6500_ReadData(MPU6500_Data_t *data)
{
    MPU6500_RawData_t raw;
    HAL_StatusTypeDef status;

    /* 先读取原始数据 */
    status = App_MPU6500_ReadRaw(&raw);
    if (status != HAL_OK)
    {
        return status;
    }
    data->accel_x = (float)raw.accel_x / 16384.0f;  // ±2g 灵敏度
    data->accel_y = (float)raw.accel_y / 16384.0f;
    data->accel_z = (float)raw.accel_z / 16384.0f;

    /* 6500转换温度: 公式 T = raw/333.87 + 21.0 */
    data->temp = (float)raw.temp / 333.87f + 21.0f;    /* 转换加速度: raw / sensitivity → 单位 g */
    
    /* 转换角速度: raw / sensitivity → 单位 °/s */
    data->gyro_x = (float)raw.gyro_x / 16.4f;       // ±2000°/s 灵敏度
    data->gyro_y = (float)raw.gyro_y / 16.4f;
    data->gyro_z = (float)raw.gyro_z / 16.4f;

    return HAL_OK;
}


/**
 * @brief  更新内部数据缓存
 * @retval HAL_StatusTypeDef: HAL_OK 表示成功
 * @note   调用后可通过 Get_xxx 函数获取最新数据
 *         如果 I2C 通信失败，会自动尝试恢复总线
 */
HAL_StatusTypeDef App_MPU6500_Update(void)
{
    HAL_StatusTypeDef status = App_MPU6500_ReadData(&mpu6500_data);
    
    /* I2C 通信失败时，尝试恢复总线 */
    if (status != HAL_OK)
    {
        App_I2C_Recovery();
        /* 恢复后重新读取一次 */
        status = App_MPU6500_ReadData(&mpu6500_data);
    }
    
    return status;
}

/* ======================== 获取函数 ======================== */

float App_MPU6500_Get_Temperature(void)
{
    return mpu6500_data.temp;
}

float App_MPU6500_Get_Accel_X(void)
{
    return mpu6500_data.accel_x;
}

float App_MPU6500_Get_Accel_Y(void)
{
    return mpu6500_data.accel_y;
}

float App_MPU6500_Get_Accel_Z(void)
{
    return mpu6500_data.accel_z;
}

float App_MPU6500_Get_Gyro_X(void)
{
    return mpu6500_data.gyro_x;
}

float App_MPU6500_Get_Gyro_Y(void)
{
    return mpu6500_data.gyro_y;
}

float App_MPU6500_Get_Gyro_Z(void)
{
    return mpu6500_data.gyro_z;
}

float yaw, pitch, roll=0.0f; // 全局变量存储欧拉角
// MPU6500的进程函数 解算欧拉角
void App_MPU6500_Process(void)
{
  PERIODIC(5) // 每5ms更新一次数据 采样率为200Hz

  // 先刷新传感器缓存, 否则下面 Get_xxx 读到的是上一拍的陈旧值
  if (App_MPU6500_Update() != HAL_OK)
  {
    return; // 本拍数据不可信, 跳过积分
  }

  //通过陀螺仪解算欧拉角
  float yaw_g = yaw+App_MPU6500_Get_Gyro_Z()*0.005;   // Z轴角速度即为偏航角速度 采集频率为200Hz, 采样周期为0.005s, 角度增量 = 角速度 * 采样周期
  float pitch_g = pitch+App_MPU6500_Get_Gyro_X()*0.005; // X轴角速度即为俯仰角速度
  float roll_g = roll-App_MPU6500_Get_Gyro_Y()*0.005;  // Y轴角速度即为横滚角速度

  // 通过加速度计解算欧拉角
  float pitch_a = qatan2(App_MPU6500_Get_Accel_Y(), App_MPU6500_Get_Accel_Z()) * 180.0f / 3.1415927f; // 俯仰角 转换为度
  float roll_a = qatan2(App_MPU6500_Get_Accel_X(), App_MPU6500_Get_Accel_Z()) * 180.0f / 3.1415927f; // 横滚角 转换为度

    // 互补滤波融合陀螺仪和加速度计的欧拉角
    yaw = yaw_g; // 偏航角主要依赖陀螺仪，直接使用陀螺仪解算的结果
    pitch = 0.95238f * pitch_g + (1 - 0.95238f) * pitch_a; // 俯仰角陀螺仪占95.238%，加速度计占4.762%
    roll = 0.95238f * roll_g + (1 - 0.95238f) * roll_a;    // 横滚角陀螺仪占95.238%，加速度计占4.762%

}

float App_MPU6500_Get_Yaw(void)
{
    return yaw;
}

float App_MPU6500_Get_Pitch(void)
{
    return pitch;
}

float App_MPU6500_Get_Roll(void)
{
    return roll;
}
