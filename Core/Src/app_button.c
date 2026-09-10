#include "app_button.h"
#include "app_motor.h"

static uint8_t last_reading = 1;          // 上一次读取到的原始电平
static uint8_t stable_state = 1;          // 消抖后的稳定状态
static uint32_t last_change_time = 0;     // 上一次电平变化的时间
static uint8_t output_state = 0;          // TB6612双H桥的STBY 输出状态 (0=关, 1=开)
const uint32_t debounce_delay = 20;       // 消抖延时，单位毫秒

void APP_Button_Process(void)
{
    // PA11 上拉输入：1=按键断开，0=按键按下
    uint8_t reading = HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_11);
    uint32_t now = HAL_GetTick();

    // 电平一变化就重置消抖计时器
    if (reading != last_reading) {
        last_change_time = now;
        last_reading = reading;
    }

    // 电平稳定超过 debounce_delay 毫秒后才承认
    if ((now - last_change_time) > debounce_delay) {
        if (reading != stable_state) {
            stable_state = reading;

					// 下降沿（按下瞬间）翻转输出 stable_state为0表示按钮闭合过 为1表示未闭合过
            if (stable_state == 0) {
                output_state = !output_state;
                App_motor_cmd(output_state); // 根据 output_state 控制电机
            }
        }
    }
}
