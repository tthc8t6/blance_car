#include "app_mpu6500.h"
#include "i2c.h"
#include "task.h"
#include "quick_math.h"

#define SAMPLE_PERIOD 0.005f //采样周期 单位s（与PERIODIC(5)对应）
#define COMP_ALPHA 0.95238f //互补滤波系数：陀螺仪权重
#define RAD_TO_DEG(x) ((x) * 180.0f / 3.1415927f) //弧度转度
#define ACCEL_SENSITIVITY 16384.0f //加速度计灵敏度 LSB/g（±2g）
#define GYRO_SENSITIVITY 16.4f //陀螺仪灵敏度 LSB/(°/s)（±2000°/s）
#define TEMP_SENSITIVITY 333.87f //温度灵敏度 LSB/℃
#define TEMP_OFFSET 21.0f //温度偏移 ℃


static MPU6500_Data_t mpu6500_data; // 传感器数据缓存, 供 Get 函数使用

//
// @简介：恢复I2C总线：SDA被从设备拉低卡住时，手动翻转SCL发9个时钟脉冲迫使从设备释放SDA，再重新初始化I2C
// @参数：无
//
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

//
// @简介：向MPU6500指定寄存器写入一个字节，超时100ms
// @参数：reg - 寄存器地址
// @参数：data - 要写入的数据
// @返回值：HAL状态
//
static HAL_StatusTypeDef App_MPU6500_WriteByte(uint8_t reg, uint8_t data)
{
    return HAL_I2C_Mem_Write(&hi2c1,
                             MPU6500_ADDR,
                             reg,
                             I2C_MEMADD_SIZE_8BIT,
                             &data,
                             1,
                             100);
}

//
// @简介：从MPU6500指定寄存器连续读取多个字节，超时2ms
// @参数：reg - 起始寄存器地址
// @参数：buf - 数据缓冲区指针
// @参数：len - 要读取的字节数
// @返回值：HAL状态
//
static HAL_StatusTypeDef App_MPU6500_ReadBytes(uint8_t reg, uint8_t *buf, uint16_t len)
{
    return HAL_I2C_Mem_Read(&hi2c1,
                            MPU6500_ADDR,
                            reg,
                            I2C_MEMADD_SIZE_8BIT,
                            buf,
                            len,
                            2);   /* 400kHz 下 14 字节突发仅需 0.39ms, 2ms 是 5 倍余量 */
}

//
// @简介：初始化MPU6500：复位、选时钟源、采样率1kHz、陀螺仪与加速度计低通92Hz、量程±2000dps/±2g
// @参数：无
// @返回值：无
//
void App_MPU6500_Init(void)
{

    /* 第一步: 复位 MPU6500 (PWR_MGMT_1 bit7 = DEVICE_RESET) */
    App_MPU6500_WriteByte(MPU6500_REG_PWR_MGMT_1, 0x80);

    /* 等待复位完成 (约 100ms) */
    HAL_Delay(100);

    /* 第二步: 唤醒, CLKSEL=1 自动选择最佳时钟源 */
    App_MPU6500_WriteByte(MPU6500_REG_PWR_MGMT_1, 0x01);
 
    /* 第三步: 输出数据率 ODR = 1kHz/(1+0) = 1kHz */
    App_MPU6500_WriteByte(MPU6500_REG_SMPLRT_DIV, 0x00);

    /* 第四步: 陀螺仪数字低通滤波器 92Hz, 延迟 3.9ms */
    App_MPU6500_WriteByte(MPU6500_REG_CONFIG, MPU6500_DLPF_92HZ);
    
    /* 第五步: 加速度计量程 ±2g, 灵敏度 16384 LSB/g */
    App_MPU6500_WriteByte(MPU6500_REG_ACCEL_CONFIG, MPU6500_ACCEL_FS_2G);

    /* 第六步: 陀螺仪量程 ±2000°/s, 灵敏度 16.4 LSB/(°/s) */
    App_MPU6500_WriteByte(MPU6500_REG_GYRO_CONFIG, MPU6500_GYRO_FS_2000);

    /* 第七步: 加速度计数字低通滤波器 92Hz, 延迟 7.8ms (MPU6500 独有寄存器) */
    App_MPU6500_WriteByte(MPU6500_REG_ACCEL_CONFIG2, MPU6500_ACCEL_DLPF_92HZ);

}

//
// @简介：一次性连续读取MPU6500全部原始数据（14字节，从ACCEL_XOUT_H 0x3B开始）
// @参数：data - 原始数据结构体指针
// @返回值：HAL状态
//
static HAL_StatusTypeDef App_MPU6500_ReadRaw(MPU6500_RawData_t *data)
{
    uint8_t buf[14];
    HAL_StatusTypeDef status;

    status = App_MPU6500_ReadBytes(MPU6500_REG_ACCEL_XOUT_H, buf, 14);
    if (status != HAL_OK)
    {
        return status;
    }

    /* 数据为大端序, 合并高低字节为16位有符号数 */
    data->accel_x = (int16_t)((buf[0]  << 8) | buf[1]);   // 0x3B, 0x3C
    data->accel_y = (int16_t)((buf[2]  << 8) | buf[3]);   // 0x3D, 0x3E
    data->accel_z = (int16_t)((buf[4]  << 8) | buf[5]);   // 0x3F, 0x40
    data->temp    = (int16_t)((buf[6]  << 8) | buf[7]);   // 0x41, 0x42
    data->gyro_x  = (int16_t)((buf[8]  << 8) | buf[9]);   // 0x43, 0x44
    data->gyro_y  = (int16_t)((buf[10] << 8) | buf[11]);  // 0x45, 0x46
    data->gyro_z  = (int16_t)((buf[12] << 8) | buf[13]);  // 0x47, 0x48

    return HAL_OK;
}

//
// @简介：读取MPU6500并换算为物理量（加速度g、温度℃、角速度°/s）
// @参数：data - 物理量数据结构体指针
// @返回值：HAL状态
//
static HAL_StatusTypeDef App_MPU6500_ReadData(MPU6500_Data_t *data)
{
    MPU6500_RawData_t raw;
    HAL_StatusTypeDef status;

    status = App_MPU6500_ReadRaw(&raw);
    if (status != HAL_OK)
    {
        return status;
    }
    data->accel_x = (float)raw.accel_x / ACCEL_SENSITIVITY;  // ±2g 灵敏度 单位g
    data->accel_y = (float)raw.accel_y / ACCEL_SENSITIVITY;
    data->accel_z = (float)raw.accel_z / ACCEL_SENSITIVITY;

    /* MPU6500 温度公式: T = raw/333.87 + 21.0 单位℃ */
    data->temp = (float)raw.temp / TEMP_SENSITIVITY + TEMP_OFFSET;

    data->gyro_x = (float)raw.gyro_x / GYRO_SENSITIVITY;       // ±2000°/s 灵敏度 单位°/s
    data->gyro_y = (float)raw.gyro_y / GYRO_SENSITIVITY;
    data->gyro_z = (float)raw.gyro_z / GYRO_SENSITIVITY;

    return HAL_OK;
}


//
// @简介：读取一次传感器数据并更新内部缓存，I2C失败时自动恢复总线并重读一次
// @参数：无
// @返回值：HAL_OK表示读取成功
// @注意：只允许App_MPU6500_Process调用，其他地方用Get函数
//
HAL_StatusTypeDef App_MPU6500_Update(void)
{
    HAL_StatusTypeDef status = App_MPU6500_ReadData(&mpu6500_data);

    if (status != HAL_OK)
    {
        App_I2C_Recovery();
        status = App_MPU6500_ReadData(&mpu6500_data);
    }

    return status;
}

//
// @简介：检查MPU6500是否丢失配置，丢失则重新配置
// @参数：无
// @注意：MPU6500掉电复位后寄存器回到默认值（陀螺仪±250dps、低通关闭），而STM32并不知道，
//        仍按±2000dps换算，角速度读数会大8倍。只允许App_MPU6500_Process调用
//
static void App_MPU6500_CheckConfig(void)
{
    uint8_t gyro_config;

    // 读回陀螺仪量程寄存器，读失败说明总线有问题，交给下一拍的Update去恢复
    if (App_MPU6500_ReadBytes(MPU6500_REG_GYRO_CONFIG, &gyro_config, 1) != HAL_OK)
    {
        return;
    }
    // 如果陀螺仪量程不是 ±2000°/s，则重新配置 MPU6500
    if (gyro_config != MPU6500_GYRO_FS_2000)
    {
       //唤醒, CLKSEL=1 自动选择最佳时钟源
       App_MPU6500_WriteByte(MPU6500_REG_PWR_MGMT_1, 0x01);
       // 输出数据率 ODR = 1kHz/(1+0) = 1kHz
       App_MPU6500_WriteByte(MPU6500_REG_SMPLRT_DIV, 0x00);
       // 陀螺仪数字低通滤波器 92Hz, 延迟 3.9ms
       App_MPU6500_WriteByte(MPU6500_REG_CONFIG, MPU6500_DLPF_92HZ);
       // 加速度计量程 ±2g, 灵敏度 16384 LSB/g
       App_MPU6500_WriteByte(MPU6500_REG_ACCEL_CONFIG, MPU6500_ACCEL_FS_2G);
       // 陀螺仪量程 ±2000°/s, 灵敏度 16.4 LSB/(°/s)
       App_MPU6500_WriteByte(MPU6500_REG_GYRO_CONFIG, MPU6500_GYRO_FS_2000);
       // 加速度计数字低通滤波器 92Hz, 延迟 7.8ms (MPU6500 独有寄存器)
       App_MPU6500_WriteByte(MPU6500_REG_ACCEL_CONFIG2, MPU6500_ACCEL_DLPF_92HZ);
    }
}

//
// @简介：获取温度
// @参数：无
// @返回值：温度，单位摄氏度
//
float App_MPU6500_Get_Temperature(void)
{
    return mpu6500_data.temp;
}

//
// @简介：获取X轴加速度
// @参数：无
// @返回值：X轴加速度，单位g
//
float App_MPU6500_Get_Accel_X(void)
{
    return mpu6500_data.accel_x;
}

//
// @简介：获取Y轴加速度
// @参数：无
// @返回值：Y轴加速度，单位g
//
float App_MPU6500_Get_Accel_Y(void)
{
    return mpu6500_data.accel_y;
}

//
// @简介：获取Z轴加速度
// @参数：无
// @返回值：Z轴加速度，单位g
//
float App_MPU6500_Get_Accel_Z(void)
{
    return mpu6500_data.accel_z;
}

//
// @简介：获取X轴角速度（俯仰方向）
// @参数：无
// @返回值：X轴角速度，单位度/秒
//
float App_MPU6500_Get_Gyro_X(void)
{
    return mpu6500_data.gyro_x;
}

//
// @简介：获取Y轴角速度（横滚方向）
// @参数：无
// @返回值：Y轴角速度，单位度/秒
//
float App_MPU6500_Get_Gyro_Y(void)
{
    return mpu6500_data.gyro_y;
}

//
// @简介：获取Z轴角速度（偏航方向）
// @参数：无
// @返回值：Z轴角速度，单位度/秒
//
float App_MPU6500_Get_Gyro_Z(void)
{
    return mpu6500_data.gyro_z;
}

static float yaw, pitch, roll=0.0f; // 欧拉角 单位度

//
// @简介：姿态解算，每5ms执行一次：读传感器，陀螺仪积分与加速度计角度做互补滤波，得到偏航/俯仰/横滚角
// @参数：无
// @注意：偏航角只有陀螺仪积分，会漂移
//
void App_MPU6500_Process(void)
{
  PERIODIC(5) // 每5ms执行一次 200Hz

  // 每20拍(100ms)检查一次MPU6500配置是否丢失
  static uint8_t check_count = 0;
  if (++check_count >= 20)
  {
    check_count = 0;
    App_MPU6500_CheckConfig();
  }

  // 先刷新传感器缓存, 否则 Get 读到的是上一拍的旧值
  if (App_MPU6500_Update() != HAL_OK)
  {
    return; // 本拍数据不可信, 跳过积分
  }

  //通过陀螺仪解算欧拉角 角度增量 = 角速度 * 采样周期0.005s
  float yaw_g = yaw+App_MPU6500_Get_Gyro_Z()*SAMPLE_PERIOD;
  float pitch_g = pitch+App_MPU6500_Get_Gyro_X()*SAMPLE_PERIOD;
  float roll_g = roll-App_MPU6500_Get_Gyro_Y()*SAMPLE_PERIOD;

  // 通过加速度计解算欧拉角 单位度
  float pitch_a = RAD_TO_DEG(qatan2(App_MPU6500_Get_Accel_Y(), App_MPU6500_Get_Accel_Z()));
  float roll_a = RAD_TO_DEG(qatan2(App_MPU6500_Get_Accel_X(), App_MPU6500_Get_Accel_Z()));

    // 互补滤波融合陀螺仪和加速度计的欧拉角
    yaw = yaw_g; // 偏航角只用陀螺仪
    pitch = COMP_ALPHA * pitch_g + (1 - COMP_ALPHA) * pitch_a; // 陀螺仪占95.238%，加速度计占4.762%
    roll = COMP_ALPHA * roll_g + (1 - COMP_ALPHA) * roll_a;

}

//
// @简介：获取偏航角
// @参数：无
// @返回值：偏航角，单位度
//
float App_MPU6500_Get_Yaw(void)
{
    return yaw;
}

//
// @简介：获取俯仰角（平衡控制使用的倾角）
// @参数：无
// @返回值：俯仰角，单位度
//
float App_MPU6500_Get_Pitch(void)
{
    return pitch;
}

//
// @简介：获取横滚角
// @参数：无
// @返回值：横滚角，单位度
//
float App_MPU6500_Get_Roll(void)
{
    return roll;
}
