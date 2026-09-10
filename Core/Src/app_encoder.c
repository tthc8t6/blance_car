#include <stdint.h>
#include "stm32f1xx_hal.h"
#include "app_encoder.h"

static volatile int64_t encoder_count_L = 0; // 左侧编码器计数器
static volatile int64_t encoder_count_R = 0; // 右侧编码器计数器
static volatile int8_t direction_L = 0; // 左侧编码器方向，1为正转，-1为反转
static volatile int8_t direction_R = 0; // 右侧编码器方向，1为正转，-1为反转
static volatile int64_t t0_L = 0, t0_R = 0; // 左侧和右侧编码器当前计数时间
static volatile int64_t t1_L = 0, t1_R = 0; // 左侧和右侧编码器上次计数时间

//
// SysTick 的 COUNTFLAG 读一次即自动清零，它是「毫秒进位」的唯一真相来源：
// SysTick 中断和 App_GetMicroseconds() 谁先读到这个标志，谁就负责把进位写回
// 共享的 uwTick，写完标志位就没了，另一方自然不会重复加。
// 所以必须覆盖 HAL 默认的 HAL_IncTick()（它是 __weak 的），让中断也走同一套协议。
// 否则 HAL 在中断里无条件累加 uwTick，而 COUNTFLAG 仍挂着等人来读，
// 该毫秒内第一个调用 App_GetMicroseconds() 的人会误判成「进位还没算」，
// 在已经正确的 uwTick 上再加 1ms，使时间戳凭空大出 1000us。
//
void HAL_IncTick(void)
{
    if (SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk)
    {
        uwTick += (uint32_t)uwTickFreq;
    }
}

//
// 初始化编码器所依赖的微秒时基。
// 把 SysTick 提到和编码器 EXTI 相同的最高优先级(0)，避免编码器中断密集时
// SysTick 被推迟，造成毫秒进位滞后。必须在 HAL_Init/SystemClock_Config 之后调用，
// 因为它们会把 SysTick 优先级重置回 TICK_INT_PRIORITY(15)。
//
void App_Encoder_Init(void)
{
    HAL_NVIC_SetPriority(SysTick_IRQn, 0, 0);
}

uint64_t App_GetMicroseconds(void)
{
    uint32_t primask = __get_PRIMASK(); // 保存中断开关状态，允许在临界区内嵌套调用
    uint32_t tick_ms;
    uint32_t systick_val;

    __disable_irq();

    // 循环直到读到一组没有跨越毫秒边界的 (uwTick, VAL)：
    // 读到 COUNTFLAG 说明刚翻转过，先把进位补到 uwTick 上，再重读一次
    while (1)
    {
        tick_ms = uwTick;
        systick_val = SysTick->VAL;

        if (SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk)
        {
            uwTick += (uint32_t)uwTickFreq; // 进位写回共享计数器，而不是局部 +1
        }
        else
        {
            break;
        }
    }

    __set_PRIMASK(primask); // 原来关着就保持关着，不会误开中断

    uint32_t systick_load = SysTick->LOAD + 1UL;
    // (load - val) 最大为 72000，乘 1000 后仍在 uint32 范围内，
    // 用 32 位除法代替原来的 64 位除法（Cortex-M3 无 64 位硬件除法，中断里开销可观）
    return (uint64_t)tick_ms * 1000ULL + ((systick_load - systick_val) * 1000UL) / systick_load;
}


//T法测角速度的中断回调函数
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{ 
    //定义：顺时针为正转 逆时针为反转 顺时针count加1 逆时针count减1
    if(GPIO_Pin == GPIO_PIN_14) { // 左侧编码器EXTI中断
  
  t1_L = t0_L; // 将上次计数时间赋值给t1_L
  t0_L = App_GetMicroseconds(); // 获取当前时间，单位为us

   //左侧编码器相位电平 
   uint8_t A_L = HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_14);//A相上下升降沿都触发中断
   uint8_t B_L = HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_15);//B相电平
   // 简化判断：A相和B相电平不同则反转，相同则正转
   if((A_L == GPIO_PIN_SET && B_L == GPIO_PIN_RESET) || (A_L == GPIO_PIN_RESET && B_L == GPIO_PIN_SET))//现在反转
   {
     encoder_count_L--;
     if(direction_L > 0) direction_L = -2; // 之前正转现在反转，方向变为-2表示从正转变为反转
     else// 之前已经是反转了，继续保持反转
     direction_L = -1;
   }
   else//现在正转
   {
     encoder_count_L++;
     if(direction_L < 0) direction_L = 2; // 之前反转现在正转，方向变为2表示从反转变为正转
     else// 之前已经是正转了，继续保持正转
     direction_L = 1;
   }
}
 if(GPIO_Pin == GPIO_PIN_3) { // 右侧编码器EXTI中断
    t1_R = t0_R; // 将上次计数时间赋值给t1_R
    t0_R = App_GetMicroseconds(); // 获取当前时间，单位为us

    //右侧编码器相位电平 
    uint8_t A_R = HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_3);//A相上下升降沿都触发中断
    uint8_t B_R = HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_4);//B相电平
    // 简化判断：右轮与左轮方向相反，A相和B相电平不同则正转，相同则反转
    if((A_R == GPIO_PIN_SET && B_R == GPIO_PIN_RESET) || (A_R == GPIO_PIN_RESET && B_R == GPIO_PIN_SET))//现在正转
    {
      encoder_count_R++;
      if(direction_R < 0) direction_R = 2; // 之前反转现在正转，方向变为2表示从反转变为正转
      else
      direction_R = 1;// 之前已经是正转了，继续保持正转
    }
    else//现在反转
    {
      encoder_count_R--;
      if(direction_R > 0) direction_R = -2; // 之前正转现在反转，方向变为-2表示从正转变为反转
      else
      direction_R = -1;// 之前已经是反转了，继续保持反转
    }
}
}

void App_Encoder_Get_Count(volatile float* count_L, volatile float* count_R)
{    
    *count_L = encoder_count_L /22.0f/(30613.0f/1500.0f)*360.0f; // 将编码器计数转换为弧度，22为编码器每转的脉冲数，3061/1500为减速比，360为每转的弧度
    *count_R = encoder_count_R /22.0f/(30613.0f/1500.0f)*360.0f; // 将编码器计数转换为弧度，22为编码器每转的脉冲数，3061/1500为减速比，360为每转的弧度
}

void App_Encoder_Get_Speed(volatile float* speed_L, volatile float* speed_R)
{
  // 原子读取时间戳，防止中断修改导致数据不一致 导致异常值
  // 注意：App_GetMicroseconds() 必须放在临界区之外调用。它内部会开关中断，
  // 若在临界区内调用，其结尾的开中断会提前解除这里的保护，
  // 下面几个变量的读取就不再原子——高速时 t0/t1 可能取到不同边沿而撕裂。
  uint32_t primask = __get_PRIMASK();
  __disable_irq();// 关闭中断保护临界区
  int64_t t0_l = t0_L, t1_l = t1_L;
  int64_t t0_r = t0_R, t1_r = t1_R;
  int8_t dir_l = direction_L, dir_r = direction_R;
  __set_PRIMASK(primask);// 恢复原来的中断状态

  // 在快照之后再取当前时间，保证 now >= t0，(now - t0) 不会为负
  int64_t now = App_GetMicroseconds();

  float T_L, T_R;

  // 计算时间间隔（秒）
  float dt_L_period = (t0_l - t1_l) * 1.0e-6f;
  float dt_L_elapsed = (now - t0_l) * 1.0e-6f;
  T_L = (dt_L_period > dt_L_elapsed) ? dt_L_period : dt_L_elapsed;

  float dt_R_period = (t0_r - t1_r) * 1.0e-6f;
  float dt_R_elapsed = (now - t0_r) * 1.0e-6f;
  T_R = (dt_R_period > dt_R_elapsed) ? dt_R_period : dt_R_elapsed;

 
  // 角速度(度/秒) = 每脉冲角度 / T。换算因子须与 App_Encoder_Get_Count 完全一致：
  // 每脉冲角度 = 360 / (22 * 减速比) ≈ 0.8°，所以是 /22.0f/(...) 而非 *22.0f，减速比=(30613.0f / 1500.0f)
  // 发生转向时速度为0，因为转向时T会变得非常小，导致计算出的速度异常大，所以直接将速度置0
  if(dir_l==2||dir_l==-2) 
		*speed_L = 0;
  else
  *speed_L = (dir_l / T_L) / 22.0f / (30613.0f / 1500.0f) * 6.2831853f;// 角速度(弧度/秒) = 每脉冲角度 / T。换算因子须与 App_Encoder_Get_Count 完全一致，使用弧度制计算角速度，方便后续使用
  if(dir_r==2||dir_r==-2) 
		*speed_R = 0;
  else
  *speed_R = (dir_r / T_R) / 22.0f / (30613.0f / 1500.0f) * 6.2831853f;// 角速度(弧度/秒) = 每脉冲角度 / T。换算因子须与 App_Encoder_Get_Count 完全一致，使用弧度制计算角速度，方便后续使用
}


