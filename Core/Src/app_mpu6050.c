#include "app_mpu6050.h"
#include "i2c.h"
#include "usart.h"
#include <string.h>
#include "task.h"
#include "quick_math.h"


/* ======================== 私有辅助函数 ======================== */
/** @brief MPU6050 数据缓存, 供 getter 函数使用 */
static MPU6050_Data_t mpu6050_data;

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
 * @brief  向 MPU6050 指定寄存器写入一个字节
 * @param  reg:  寄存器地址
 * @param  data: 要写入的数据
 * @retval HAL_StatusTypeDef
 * @note   I2C 传输时序: [START][ADDR+W][REG][DATA][STOP]
 *         超时时间 100ms
 */
static HAL_StatusTypeDef App_MPU6050_WriteByte(uint8_t reg, uint8_t data)
{
    return HAL_I2C_Mem_Write(&hi2c1,            // I2C 句柄
                             MPU6050_ADDR,       // 设备地址 (7位左移后)
                             reg,                // 寄存器地址
                             I2C_MEMADD_SIZE_8BIT, // 寄存器地址宽度 8 位
                             &data,              // 数据指针
                             1,                  // 写入 1 个字节
                             100);               // 超时 100ms
}

/**
 * @brief  从 MPU6050 指定寄存器读取一个字节
 * @param  reg:  寄存器地址
 * @param  data: 读取数据的存放指针
 * @retval HAL_StatusTypeDef
 * @note   I2C 传输时序: [START][ADDR+W][REG][RESTART][ADDR+R][DATA][STOP]
 */
static HAL_StatusTypeDef App_MPU6050_ReadByte(uint8_t reg, uint8_t *data)
{
    return HAL_I2C_Mem_Read(&hi2c1,
                            MPU6050_ADDR,
                            reg,
                            I2C_MEMADD_SIZE_8BIT,
                            data,
                            1,
                            100);
}

/**
 * @brief  从 MPU6050 指定寄存器连续读取多个字节
 * @param  reg: 起始寄存器地址
 * @param  buf: 数据缓冲区指针
 * @param  len: 要读取的字节数
 * @retval HAL_StatusTypeDef
 * @note   MPU6050 支持地址自增的连续读取, 读取多个寄存器时效率更高
 */
static HAL_StatusTypeDef App_MPU6050_ReadBytes(uint8_t reg, uint8_t *buf, uint16_t len)
{
    return HAL_I2C_Mem_Read(&hi2c1,
                            MPU6050_ADDR,
                            reg,
                            I2C_MEMADD_SIZE_8BIT,
                            buf,
                            len,
                            100);
}

/* ======================== MPU6050 设备检测与初始化 ======================== */

/**
 * @brief  检测 MPU6050 是否在线
 * @retval HAL_OK 表示设备在线, 其他表示通信失败
 * @note   读取 WHO_AM_I 寄存器 (0x75), 正确值应为 0x68
 */
HAL_StatusTypeDef App_MPU6050_Check(void)
{
    uint8_t id = 0;
    HAL_StatusTypeDef status;

    status = App_MPU6050_ReadByte(MPU6050_REG_WHO_AM_I, &id);
    if (status != HAL_OK)
    {
        return status;
    }

    /* MPU6050 的 WHO_AM_I 应返回 0x68 (0b1101000) */
    if (id == 0x68)
    {
        return HAL_OK;
    }

    return HAL_ERROR;
}

/**
 * @brief  初始化 MPU6050
 * @retval HAL_OK: 初始化成功
 * @note   初始化流程:
 *         1. 复位设备 (PWR_MGMT_1 bit7)
 *         2. 唤醒, 选择时钟源为 PLL with X-axis gyroscope
 *         3. 配置陀螺仪量程 ±2000°/s
 *         4. 配置加速度计量程 ±2g
 *         5. 采样率 = 1kHz / (1+9) = 100Hz
 */
HAL_StatusTypeDef App_MPU6050_Init(void)
{
    HAL_StatusTypeDef status;
    uint8_t check;

    /* 第一步: 检测设备是否在线 */
    /* MPU6050_REG_WHO_AM_I = 0x75, 只读, MPU6050固定返回0x68(0b1101000) */
    status = App_MPU6050_Check();
    if (status != HAL_OK)
    {
        return status;
    }

    /* 第二步: 复位 MPU6050 */
    /* MPU6050_REG_PWR_MGMT_1 = 0x6B (电源管理1), bit7=DEVICE_RESET(复位), bit2:0=CLKSEL(时钟源) */
    status = App_MPU6050_WriteByte(MPU6050_REG_PWR_MGMT_1, 0x80);
    if (status != HAL_OK) return status;

    /* 等待复位完成 (约 100ms) */
    HAL_Delay(100);

    /* 第三步: 唤醒, 时钟源选择 X 轴陀螺仪 PLL */
    /* PWR_MGMT_1 写入 0x01: bit7=0(不清复位), bit2:0=001(X轴PLL时钟) */
    status = App_MPU6050_WriteByte(MPU6050_REG_PWR_MGMT_1, 0x01);
    if (status != HAL_OK) return status;

    /* 第四步: 配置采样率 200Hz */
    /* MPU6050_REG_SMPLRT_DIV = 0x19, 采样率 = 1kHz / (1+9) = 200Hz */
    status = App_MPU6050_WriteByte(MPU6050_REG_SMPLRT_DIV, 0x04);
    if (status != HAL_OK) return status;

    /* 第五步: 配置数字低通滤波器 */
    /* MPU6050_REG_CONFIG = 0x1A, 值0x03 → DLPF带宽约44Hz, 延迟约4.9ms */
    status = App_MPU6050_WriteByte(MPU6050_REG_CONFIG, 0x03);
    if (status != HAL_OK) return status;

        /* 第六步: 加速度计量程 ±2g */
    /* MPU6050_REG_ACCEL_CONFIG = 0x1C, bit4:3=AFS_SEL, FS_2G=0x00(bit4:3=00), 灵敏度16384 LSB/g */
    status = App_MPU6050_WriteByte(MPU6050_REG_ACCEL_CONFIG, MPU6050_ACCEL_FS_2G);
    if (status != HAL_OK) return status;

    /* 第七步: 陀螺仪量程 ±2000°/s */
    /* MPU6050_REG_GYRO_CONFIG = 0x1B, bit4:3=FS_SEL, FS_2000=0x18(bit4:3=11), 灵敏度16.4 LSB/°/s */
    status = App_MPU6050_WriteByte(MPU6050_REG_GYRO_CONFIG, MPU6050_GYRO_FS_2000);
    if (status != HAL_OK) return status;

    /* 验证初始化: 再次读取 WHO_AM_I */
    status = App_MPU6050_ReadByte(MPU6050_REG_WHO_AM_I, &check);
    if (status == HAL_OK && check == 0x68)  // 0b1101000
    {
        return HAL_OK;
    }
    else
    {
        return HAL_ERROR;
    }
}

/* ======================== 数据读取函数 ======================== */

/**
 * @brief  读取 MPU6050 全部原始数据 (一次性读取 14 字节)
 * @param  data: 原始数据结构体指针
 * @retval HAL_StatusTypeDef
 * @note   从 ACCEL_XOUT_H (0x3B) 开始连续读取 14 字节:
 *         [AX_H][AX_L][AY_H][AY_L][AZ_H][AZ_L][T_H][T_L]
 *         [GX_H][GX_L][GY_H][GY_L][GZ_H][GZ_L]
 *         数据为大端序 (高字节在前), 需要合并为 16 位有符号数
 */
static HAL_StatusTypeDef App_MPU6050_ReadRaw(MPU6050_RawData_t *data)
{
    uint8_t buf[14];
    HAL_StatusTypeDef status;

    /* 从 0x3B 开始连续读取 14 字节 */
    status = App_MPU6050_ReadBytes(MPU6050_REG_ACCEL_XOUT_H, buf, 14);
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

static HAL_StatusTypeDef App_MPU6050_ReadData(MPU6050_Data_t *data)
{
    MPU6050_RawData_t raw;
    HAL_StatusTypeDef status;

    /* 先读取原始数据 */
    status = App_MPU6050_ReadRaw(&raw);
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
HAL_StatusTypeDef App_MPU6050_Update(void)
{
    HAL_StatusTypeDef status = App_MPU6050_ReadData(&mpu6050_data);
    
    /* I2C 通信失败时，尝试恢复总线 */
    if (status != HAL_OK)
    {
        App_I2C_Recovery();
        /* 恢复后重新读取一次 */
        status = App_MPU6050_ReadData(&mpu6050_data);
    }
    
    return status;
}

/* ======================== 获取函数 ======================== */

float App_MPU6050_Get_Temperature(void)
{
    return mpu6050_data.temp;
}

float App_MPU6050_Get_Accel_X(void)
{
    return mpu6050_data.accel_x;
}

float App_MPU6050_Get_Accel_Y(void)
{
    return mpu6050_data.accel_y;
}

float App_MPU6050_Get_Accel_Z(void)
{
    return mpu6050_data.accel_z;
}

float App_MPU6050_Get_Gyro_X(void)
{
    return mpu6050_data.gyro_x;
}

float App_MPU6050_Get_Gyro_Y(void)
{
    return mpu6050_data.gyro_y;
}

float App_MPU6050_Get_Gyro_Z(void)
{
    return mpu6050_data.gyro_z;
}

float yaw, pitch, roll=0.0f; // 全局变量存储欧拉角
// MPU6050的进程函数 解算欧拉角
void APP_MPU6050_Proc(void)
{
  PERIODIC(5) // 每5ms更新一次数据 采样率为200Hz
  //通过陀螺仪解算欧拉角
  float yaw_g = yaw+App_MPU6050_Get_Gyro_Z()*0.005;   // Z轴角速度即为偏航角速度 采集频率为200Hz, 采样周期为0.005s, 角度增量 = 角速度 * 采样周期
  float pitch_g = pitch+App_MPU6050_Get_Gyro_X()*0.005; // X轴角速度即为俯仰角速度 
  float roll_g = roll-App_MPU6050_Get_Gyro_Y()*0.005;  // Y轴角速度即为横滚角速度

  // 通过加速度计解算欧拉角
  float pitch_a = qatan2(App_MPU6050_Get_Accel_Y(), App_MPU6050_Get_Accel_Z()) * 180.0f / 3.1415927f; // 俯仰角 转换为度
  float roll_a = qatan2(App_MPU6050_Get_Accel_X(), App_MPU6050_Get_Accel_Z()) * 180.0f / 3.1415927f; // 横滚角 转换为度
  
    // 互补滤波融合陀螺仪和加速度计的欧拉角
    yaw = yaw_g; // 偏航角主要依赖陀螺仪，直接使用陀螺仪解算的结果
    pitch = 0.95238f * pitch_g + (1 - 0.95238f) * pitch_a; // 俯仰角陀螺仪占95.238%，加速度计占4.762%
    roll = 0.95238f * roll_g + (1 - 0.95238f) * roll_a;    // 横滚角陀螺仪占95.238%，加速度计占4.762%   

  nxt=HAL_GetTick()+5; // 设置下次更新时间
}

float App_MPU6050_Get_Yaw(void)
{
    return yaw;
}

float App_MPU6050_Get_Pitch(void)
{
    return pitch;
}

float App_MPU6050_Get_Roll(void)
{
    return roll;
}
