#include "app_bat.h"
#include "adc.h"
#include "tim.h"

static volatile float bat_v = 0.0f; //电池电压 单位V
static uint8_t stage = 0; //低电量闪烁状态 0=下次点亮 1=下次熄灭
static uint32_t last_tick = 0; //上次翻转LED的时间 单位ms

//
// @简介：获取电池电压
// @参数：无
// @返回值：电池电压，单位V，范围被限制在6.0V~8.4V
//
float App_Get_Bat_Voltage(void) {
    return bat_v;
}

//
// @简介：ADC1注入组转换完成回调（由TIM2每100ms触发）：换算电池电压并按电量点亮LED，电量过低时闪烁
// @参数：hadc - ADC句柄
//
void HAL_ADCEx_InjectedConvCpltCallback(ADC_HandleTypeDef* hadc)
{
    if (hadc->Instance == ADC1)
    {
        uint32_t adc_value = HAL_ADCEx_InjectedGetValue(&hadc1, ADC_INJECTED_RANK_1);
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
                    case 0:
                        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_6, GPIO_PIN_SET);
                        stage = 1;
                        break;
                    case 1:
                        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_6, GPIO_PIN_RESET);
                        stage = 0;
                        break;
                }
            }
        }
        // HAL 在 IRQHandler 中会关闭 JEOC 中断，需重新使能以响应下一次 TIM2 触发
        __HAL_ADC_ENABLE_IT(hadc, ADC_IT_JEOC);
    }
}

//
// @简介：启动电池电压监测：开启TIM2触发定时和ADC1注入组中断转换
// @参数：无
//
void App_Bat_Start(void)
{
  HAL_TIM_Base_Start(&htim2);
  HAL_ADCEx_InjectedStart_IT(&hadc1);
}
