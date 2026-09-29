#include "app_motor.h"
#include "pid.h"
#include "tim.h"
#include "task.h"
#include "app_encoder.h"
#include "app_pwm.h"
#include "app_bat.h"

static pid_typedef pid_motor_l;//左电机调速系统的PID控制器
static pid_typedef pid_motor_r;//右电机调速系统的PID控制器

//
// @简介：初始化左右电机转速环PID（Kp=0.5, Ki=7），并启动TIM1/TIM4的PWM输出
// @参数：无
//
void App_Motor_Init(void) {
    pid_init(&pid_motor_l, 0.5f, 7.0f, 0.0f);
    pid_init(&pid_motor_r, 0.5f, 7.0f, 0.0f);

    // 初始占空比为0，是否驱动电机由STBY(PA1)决定
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);
    HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_1);
}

//
// @简介：电机转速环，每1ms执行一次：读编码器转速->PID算电枢电压->除以电池电压得占空比->输出PWM
// @参数：无
// @注意：电池电压尚未采到(<1V)时不驱动电机
//
void App_Motor_Process(void) {
    PERIODIC(1) //每1ms执行一次

    //1.获取左右电机的速度反馈值
    float omega_speed_L, omega_speed_R; //单位rad/s
    App_Encoder_Get_Speed(&omega_speed_L, &omega_speed_R);

    //2.用当前电池电压作为PID的输出限幅（电压不可能超过电池电压，抗饱和）
    float vbat = App_Get_Bat_Voltage(); //单位V

    //ADC首次转换完成前vbat为0，不驱动电机，同时避免除零
    if (vbat < 1.0f) {
        App_PWM_Set_L(0.0f);
        App_PWM_Set_R(0.0f);
        return;
    }

    pid_limit_config(&pid_motor_l, -vbat, vbat);
    pid_limit_config(&pid_motor_r, -vbat, vbat);

    //3.计算PID输出（期望电枢电压 单位V）并转换为PWM占空比
    float Ua_l = pid_compute(&pid_motor_l, omega_speed_L);
    float Ua_r = pid_compute(&pid_motor_r, omega_speed_R);

    float duty_l = Ua_l / vbat*100.0f; //占空比 单位%
    float duty_r = Ua_r / vbat*100.0f;

    //4.设置PWM占空比
    App_PWM_Set_L(duty_l);
    App_PWM_Set_R(duty_r);
}

//
// @简介：设置左电机的目标转速
// @参数：omega_speed_L - 目标角速度，单位rad/s
//
void App_Motor_Set_Speed_L(float omega_speed_L) {
    pid_changesp(&pid_motor_l, omega_speed_L);
}

//
// @简介：设置右电机的目标转速
// @参数：omega_speed_R - 目标角速度，单位rad/s
//
void App_Motor_Set_Speed_R(float omega_speed_R) {
    pid_changesp(&pid_motor_r, omega_speed_R);
}

//
// @简介：使能/关闭电机驱动（STBY），并复位左右电机转速环PID
// @参数：on - 1表示使能，0表示待机
//
void App_Motor_Cmd(uint8_t on) {
    App_PWM_Cmd(on);
    pid_reset(&pid_motor_l);
    pid_reset(&pid_motor_r);
}

