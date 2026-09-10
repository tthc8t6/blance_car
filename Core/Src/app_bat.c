#include "app_bat.h"
#include "adc.h"
#include "tim.h"

volatile float bat_v = 0.0f;
static uint8_t stage = 0; 
static uint32_t last_tick = 0;
extern volatile float bat_v; // 电池电压变量

float APP_Get_Bat_Voltage(void) {
    return bat_v;
}

// 调用注入序列ADC转换完成回调函数
void HAL_ADCEx_InjectedConvCpltCallback(ADC_HandleTypeDef* hadc)
{
    if (hadc->Instance == ADC1)
    {
        // 注意：读取注入组数据要用 GetInjectedValue
        uint32_t adc_value = HAL_ADCEx_InjectedGetValue(&hadc1, ADC_INJECTED_RANK_1);
        // bat_v=adc_value*8.4f/3.3*3.3/4095.0f;
        bat_v = adc_value*8.4f/4095.0f;

        if (bat_v > 8.4f) {
            bat_v = 8.4f; // 限制最大电压为8.4V
        }
        if (bat_v < 6.0f) {
            bat_v = 6.0f; // 限制最小电压为6.0V
        }
        if (bat_v > 7.9f) {
            //满电 点亮3颗LED
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_SET); 
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_SET); 
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_SET); 
        }
        else if (bat_v > 7.4f) {
            //75%电 点亮2颗LED
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_SET); 
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_SET); 
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_RESET); 
        }
        else if (bat_v > 7.0f) {
            //50%电 点亮1颗LED
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_SET); 
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET); 
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_RESET); 
        }
        else if (bat_v > 6.5f) {
            //25%电 熄灭3颗LED
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_RESET); 
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET); 
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_RESET); 
        }
        else {
            //LED闪烁表示电量过低
            uint32_t current_tick = HAL_GetTick();
            if (current_tick - last_tick >= 100) {
                last_tick = current_tick;
            switch(stage) {
                    case 0: //当前熄灭状态
                        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_6, GPIO_PIN_SET);
                        stage = 1;
                        break;
                    case 1: //当前点亮状态
                        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_6, GPIO_PIN_RESET);
                        stage = 0;
                        break;
                }
            }
        }
        // HAL 在 IRQHandler 中会自动关闭 JEOC 中断，需在此重新使能以响应下一次 TIM2 触发
        __HAL_ADC_ENABLE_IT(hadc, ADC_IT_JEOC);
    }
}

void app_bat_start(void) 
{
  HAL_TIM_Base_Start(&htim2);      // 开启定时器产生ADC采集触发信号
  HAL_ADCEx_InjectedStart_IT(&hadc1);  // 启动ADC注入组中断
}
