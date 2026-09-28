#include "app_motor.h"
#include "pid.h"
#include "tim.h"
#include "task.h"
#include "app_encoder.h"
#include "app_pwm.h"
#include "app_bat.h"
#include "app_control.h"
#include "pid.h"

static pid_typedef pid_motor_l;//左电机调速系统的PID控制器
static pid_typedef pid_motor_r;//右电机调速系统的PID控制器

//初始化左右电机的PID控制器
void App_Motor_Init(void) {
    pid_init(&pid_motor_l, 0.5f, 7.0f, 0.0f);
    pid_init(&pid_motor_r, 0.5f, 7.0f, 0.0f);

    // 启动左右电机PWM通道输出（TIM1/TIM4计数器+比较输出）
    // 初始CCR为0（占空比0%），实际是否驱动电机由STBY(PA1)决定，由按钮控制
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);
    HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_1);
}

void App_Motor_Process(void) {
    PERIODIC(1); //每1ms执行一次
    
    //1.获取左右电机的速度反馈值
    float omega_speed_L, omega_speed_R;
    App_Encoder_Get_Speed(&omega_speed_L, &omega_speed_R); //获取左右电机的速度反馈值

    //2.用当前电池电压作为PID的输出限幅
    //PID输出的是期望电枢电压，而实际能加到电机上的电压上限就是当前电池电压。
    //让限幅点和执行器真正的饱和点重合，积分项才不会累积到无法兑现的区域（抗饱和）。
    //另外分子分母用同一个vbat，下面算出的占空比在数学上必落在±100内
    float vbat = APP_Get_Bat_Voltage(); // 获取电池电压

    //ADC首次注入转换完成前bat_v为0，此时不驱动电机，同时避免下面除零
    if (vbat < 1.0f) {
        App_PWM_Set_L(0.0f);
        App_PWM_Set_R(0.0f);
        return;
    }

    pid_limit_config(&pid_motor_l, -vbat, vbat);
    pid_limit_config(&pid_motor_r, -vbat, vbat);

    //3.计算PID输出并转换为PWM占空比
    float Ua_l = PID_Compute(&pid_motor_l, omega_speed_L);
    float Ua_r = PID_Compute(&pid_motor_r, omega_speed_R);

    float duty_l = Ua_l / vbat*100.0f; // 将PID输出值转换为PWM占空比
    float duty_r = Ua_r / vbat*100.0f; // 将PID输出值转换为PWM占空比

    //4.设置PWM占空比
    App_PWM_Set_L(duty_l);//设置左电机PWM占空比
    App_PWM_Set_R(duty_r);//设置右电机PWM占空比
}

//设置左右电机的目标转速 单位为弧度每秒
void App_Motor_Set_Speed_L(float omega_speed_L) {
    pid_changesp(&pid_motor_l, omega_speed_L); //设置左电机的目标速度
}

//设置右电机的目标转速 单位为弧度每秒
void App_Motor_Set_Speed_R(float omega_speed_R) {
    pid_changesp(&pid_motor_r, omega_speed_R); //设置右电机的目标速度
}

void App_Motor_Cmd(uint8_t on) {
    //启动PWM输出
    App_PWM_Cmd(on);
    pid_reset(&pid_motor_l);
    pid_reset(&pid_motor_r);
}

