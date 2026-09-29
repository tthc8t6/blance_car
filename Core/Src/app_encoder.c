#include <stdint.h>
#include "stm32f1xx_hal.h"
#include "app_encoder.h"

#define ENCODER_PPR 22.0f //编码器每转脉冲数
#define GEAR_RATIO (30613.0f / 1500.0f) //减速比
#define TWO_PI 6.2831853f

static volatile int64_t encoder_count_L = 0; // 左侧编码器计数器
static volatile int64_t encoder_count_R = 0; // 右侧编码器计数器
static volatile int8_t direction_L = 0; // 左侧编码器方向，1为正转，-1为反转，2/-2表示刚换向
static volatile int8_t direction_R = 0; // 右侧编码器方向，1为正转，-1为反转，2/-2表示刚换向
static volatile int64_t t0_L = 0, t0_R = 0; // 左侧和右侧编码器当前计数时间 单位us
static volatile int64_t t1_L = 0, t1_R = 0; // 左侧和右侧编码器上次计数时间 单位us

//
// @简介：重写HAL的毫秒计数函数，让SysTick中断与App_GetMicroseconds共用COUNTFLAG进位协议，避免重复进位
// @参数：无
//
void HAL_IncTick(void)
{
    if (SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk)
    {
        uwTick += (uint32_t)uwTickFreq;
    }
}

//
// @简介：初始化编码器所依赖的微秒时基：把SysTick中断优先级提到最高(0)
// @参数：无
// @注意：必须在HAL_Init/SystemClock_Config之后调用
//
void App_Encoder_Init(void)
{
    HAL_NVIC_SetPriority(SysTick_IRQn, 0, 0);
}

//
// @简介：获取系统运行的微秒数（uwTick毫秒数 + SysTick当前计数换算的微秒数）
// @参数：无
// @返回值：从上电开始的时间，单位us
//
uint64_t App_GetMicroseconds(void)
{
    uint32_t primask = __get_PRIMASK(); // 保存中断状态，允许嵌套调用
    uint32_t tick_ms;
    uint32_t systick_val;

    __disable_irq();

    // 读到COUNTFLAG说明刚跨过毫秒边界，补上进位后重读
    while (1)
    {
        tick_ms = uwTick;
        systick_val = SysTick->VAL;

        if (SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk)
        {
            uwTick += (uint32_t)uwTickFreq;
        }
        else
        {
            break;
        }
    }

    __set_PRIMASK(primask);

    uint32_t systick_load = SysTick->LOAD + 1UL;
    return (uint64_t)tick_ms * 1000ULL + ((systick_load - systick_val) * 1000UL) / systick_load;
}


//
// @简介：编码器A相外部中断回调：判断转向、更新计数，并记录相邻两次边沿的时间（T法测速用）
// @参数：GPIO_Pin - 触发中断的引脚（PB14为左轮，PB3为右轮）
//
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    //定义：顺时针为正转 逆时针为反转 顺时针count加1 逆时针count减1
    if(GPIO_Pin == GPIO_PIN_14) { // 左侧编码器EXTI中断

  t1_L = t0_L;
  t0_L = App_GetMicroseconds(); // 单位us

   uint8_t A_L = HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_14);//A相上下升降沿都触发中断
   uint8_t B_L = HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_15);//B相电平
   // A相和B相电平不同则反转，相同则正转
   if((A_L == GPIO_PIN_SET && B_L == GPIO_PIN_RESET) || (A_L == GPIO_PIN_RESET && B_L == GPIO_PIN_SET))//现在反转
   {
     encoder_count_L--;
     if(direction_L > 0) direction_L = -2; // 由正转变为反转
     else
     direction_L = -1;
   }
   else//现在正转
   {
     encoder_count_L++;
     if(direction_L < 0) direction_L = 2; // 由反转变为正转
     else
     direction_L = 1;
   }
}
 if(GPIO_Pin == GPIO_PIN_3) { // 右侧编码器EXTI中断
    t1_R = t0_R;
    t0_R = App_GetMicroseconds(); // 单位us

    uint8_t A_R = HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_3);//A相上下升降沿都触发中断
    uint8_t B_R = HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_4);//B相电平
    // 右轮与左轮方向相反，A相和B相电平不同则正转，相同则反转
    if((A_R == GPIO_PIN_SET && B_R == GPIO_PIN_RESET) || (A_R == GPIO_PIN_RESET && B_R == GPIO_PIN_SET))//现在正转
    {
      encoder_count_R++;
      if(direction_R < 0) direction_R = 2; // 由反转变为正转
      else
      direction_R = 1;
    }
    else//现在反转
    {
      encoder_count_R--;
      if(direction_R > 0) direction_R = -2; // 由正转变为反转
      else
      direction_R = -1;
    }
}
}

//
// @简介：获取左右轮累计转过的角度
// @参数：count_L - 左轮累计角度，单位度
// @参数：count_R - 右轮累计角度，单位度
//
void App_Encoder_Get_Count(volatile float* count_L, volatile float* count_R)
{
    // 22为编码器每转脉冲数，30613/1500为减速比，360为每转的度数
    *count_L = encoder_count_L /ENCODER_PPR/GEAR_RATIO*360.0f;
    *count_R = encoder_count_R /ENCODER_PPR/GEAR_RATIO*360.0f;
}

//
// @简介：用T法获取左右轮角速度；换向瞬间输出0
// @参数：omega_speed_L - 左轮角速度，单位rad/s
// @参数：omega_speed_R - 右轮角速度，单位rad/s
//
void App_Encoder_Get_Speed(volatile float* omega_speed_L, volatile float* omega_speed_R)
{
  // 关中断原子读取，防止被中断修改导致数据撕裂
  // 注意：App_GetMicroseconds() 必须放在临界区之外，它结尾会开中断
  uint32_t primask = __get_PRIMASK();
  __disable_irq();
  int64_t t0_l = t0_L, t1_l = t1_L;
  int64_t t0_r = t0_R, t1_r = t1_R;
  int8_t dir_l = direction_L, dir_r = direction_R;
  __set_PRIMASK(primask);

  int64_t now = App_GetMicroseconds(); // 在快照之后取，保证 now >= t0

  float T_L, T_R;

  // 时间间隔 单位s：取两次边沿间隔与距上次边沿已过时间的较大者
  float dt_L_period = (t0_l - t1_l) * 1.0e-6f;
  float dt_L_elapsed = (now - t0_l) * 1.0e-6f;
  T_L = (dt_L_period > dt_L_elapsed) ? dt_L_period : dt_L_elapsed;

  float dt_R_period = (t0_r - t1_r) * 1.0e-6f;
  float dt_R_elapsed = (now - t0_r) * 1.0e-6f;
  T_R = (dt_R_period > dt_R_elapsed) ? dt_R_period : dt_R_elapsed;


  // 角速度(rad/s) = 方向 / T / 22 / 减速比 * 2PI，换算因子须与 App_Encoder_Get_Count 一致
  // 换向时T会突变得很小，速度异常大，所以直接置0
  if(dir_l==2||dir_l==-2)
		*omega_speed_L = 0;
  else
  *omega_speed_L = (dir_l / T_L) / ENCODER_PPR / GEAR_RATIO * TWO_PI;
  if(dir_r==2||dir_r==-2)
		*omega_speed_R = 0;
  else
  *omega_speed_R = (dir_r / T_R) / ENCODER_PPR / GEAR_RATIO * TWO_PI;
}


