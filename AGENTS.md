# 平衡车项目 - AI 助手指引

## 项目概述

STM32F103C8T6 两轮自平衡小车项目，使用 HAL 库开发，Keil MDK-ARM 构建。

## 硬件架构

| 组件 | 型号/引脚 | 说明 |
|------|----------|------|
| MCU | STM32F103C8T6 | Cortex-M3, 72MHz, 64KB Flash, 20KB SRAM |
| 左电机 | TIM1_CH1 (PA8) | PWM 输出，PA9/PA10 控制方向 |
| 右电机 | TIM4_CH1 (PB6) | PWM 输出，PB5/PB7 控制方向 |
| 左编码器 | PB14 (EXTI) | 外部中断捕获 A 相，PB15 为 B 相 |
| 右编码器 | PB3 (EXTI) | 外部中断捕获 A 相，PB4 为 B 相 |
| 电池 ADC | ADC1_IN8 (PB0) | 注入组，TIM2 触发 |
| 串口 | USART2 (PA2/PA3) | 调试输出 |
| 按钮 | PA11 | 上拉输入，控制电机驱动使能 |
| LED | PA4, PA5, PA6 | 电量指示 |

## 代码结构

```
blance_car/
├── Core/
│   ├── Inc/          # 头文件
│   │   ├── main.h
│   │   ├── app_bat.h       # 电池电压监测
│   │   ├── app_button.h    # 按钮处理
│   │   ├── app_encoder.h   # 编码器测速
│   │   └── app_pwm.h       # 电机 PWM 控制
│   └── Src/          # 源文件
│       ├── main.c          # 主程序入口
│       ├── app_bat.c
│       ├── app_button.c
│       ├── app_encoder.c
│       └── app_pwm.c
├── test/             # 测试代码
│   ├── encoder_test.h/c    # 编码器测试
│   └── PWM_test.h/c        # PWM 测试
├── Drivers/          # ST HAL 库 (v1.1.10)
└── MDK-ARM/          # Keil 工程文件
```

## 编码约定

### 命名规则
- **应用层文件**: `app_xxx.h/c` 前缀
- **应用层函数**: `App_` 前缀 + PascalCase（如 `App_PWM_Set_L`）
- **测试文件**: `xxx_test.h/c`
- **测试函数**: 无前缀 PascalCase（如 `PWM_Test`）
- **全局变量**: `volatile` 修饰中断共享变量

### CubeMX 代码区域
代码必须写在 `USER CODE BEGIN/END` 标记之间，否则重新生成代码时会被覆盖：
```c
/* USER CODE BEGIN 1 */
// 你的代码放这里
/* USER CODE END 1 */
```

### 注释风格
- 使用 Doxygen 格式 `@brief`、`@param`、`@note`
- 内联注释使用中文

## 构建与调试

### 构建命令
使用 Keil MDK-ARM 打开 `MDK-ARM/blance_car.uvprojx` 进行编译。

### 调试方法
1. **串口调试**: 通过 USART2 (115200 baud) 输出数据
2. **测试函数**: 在 `main.c` 中调用对应测试函数（阻塞式 `while(1)`）
3. **ST-Link**: SWD 接口 (PA13/PA14) 烧录调试

### 测试函数使用
在 `main.c` 的 `USER CODE BEGIN 2` 区域取消注释对应测试调用：
```c
// PWM_Test();           // PWM 扫描测试
// Encoder_Test();       // 编码器计数测试
// Encoder_T_Method_Test(); // T 法测速测试
Encoder_T_Method_Test();   // 当前激活的测试
```

## 关键 API 速查

### 电机控制
```c
App_PWM_Set_L(float Duty);  // 左电机 -100 ~ +100
App_PWM_Set_R(float Duty);  // 右电机 -100 ~ +100
```

### 编码器读取
```c
App_Encoder_Get_Count(&count_L, &count_R);  // 累计脉冲
App_Encoder_Get_Speed(&speed_L, &speed_R);  // 角速度
```

### 电池监测
```c
extern volatile float bat_v;  // 当前电压 (6.0V ~ 8.4V)
```

## 注意事项

1. **不要修改** `Drivers/` 目录下的 HAL 库文件
2. **不要修改** CubeMX 自动生成的初始化代码（`USER CODE` 区域外）
3. 测试函数为阻塞式，切换测试需修改 `main.c` 后重新编译
4. 编码器使用外部中断而非硬件编码器模式，需注意中断优先级
