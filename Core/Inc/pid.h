#ifndef PID_H
#define PID_H

#include "stm32f1xx_hal.h"

// PID controller implementation
typedef struct {
    float kp;//比列系数
    float ki;//积分系数
    float kd;//微分系数
    float sp; // Setpoint

    uint64_t last_time; //（k-1）次PID计算时间
    float last_error; //（k-1）次误差
    float integral_term; //积分项，累加时已乘过ki，单位与输出一致，便于直接用输出限幅做抗饱和

    float upper_limit; // PID输出上限
    float lower_limit; // PID输出下限
} pid_typedef;

void pid_init(pid_typedef *pid, float kp, float ki, float kd);
void pid_changesp(pid_typedef *pid, float new_sp);
float PID_Compute(pid_typedef *pid, float feedback);
void pid_limit_config(pid_typedef *pid, float lower_limit, float upper_limit);
void pid_reset(pid_typedef *pid);

#endif // PID_H
