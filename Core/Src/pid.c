#include "pid.h"

//
// @简介：初始化PID控制器：设置Kp、Ki、Kd，清零状态，输出限幅默认为不限制
// @参数：pid - PID控制器指针
// @参数：kp - 比例系数
// @参数：ki - 积分系数
// @参数：kd - 微分系数
//
void pid_init(pid_typedef *pid, float kp, float ki, float kd) {
    pid->kp = kp;
    pid->ki = ki;
    pid->kd = kd;
    pid->sp = 0.0f;

    pid->last_time = HAL_GetTick();
    pid->last_error = 0.0f;
    pid->integral_term = 0.0f;

    // 输出上下限默认为float极值，可用pid_limit_config设置
    pid->upper_limit = 3.4e+38f;
    pid->lower_limit = -3.4e+38f;

}

//
// @简介：修改PID控制器的设定值
// @参数：pid - PID控制器指针
// @参数：new_sp - 新的设定值
//
void pid_changesp(pid_typedef *pid, float new_sp) {
    pid->sp = new_sp;
}

//
// @简介：计算一次PID输出（积分项已乘Ki并限幅，输出也被限幅）
// @参数：pid - PID控制器指针
// @参数：feedback - 被控量的测量值（误差 = 设定值 - 测量值）
// @返回值：PID输出
//
float pid_compute(pid_typedef *pid, float feedback) {
    float error = pid->sp-feedback;
    float Cop =error * pid->kp; //比例项

    uint64_t current_time = HAL_GetTick();//当前时间 单位ms
    float delta_time = (current_time - pid->last_time) / 1000.0f; //时间差 单位s
    float Cod = 0.0f; //微分项

    // 首次运行积分项和微分项为0
    if(pid->last_time != 0) {
        // kd为0时不做除法，避免delta_time为0时得到NaN
        if(pid->kd != 0.0f) {
            Cod = pid->kd * (error - pid->last_error) / delta_time; //误差微分
        }

        if(pid->ki != 0.0f) {
            //积分项累加时已乘ki，单位与输出一致，便于直接用输出限幅抗饱和
            pid->integral_term = pid->integral_term + pid->ki * (error + pid->last_error) * delta_time * 0.5f;

            if (pid->integral_term > pid->upper_limit) {
                pid->integral_term = pid->upper_limit;
            }
            if (pid->integral_term < pid->lower_limit) {
                pid->integral_term = pid->lower_limit;
            }
        }
    }

    float Co = Cop + pid->integral_term + Cod; //PID输出

    pid->last_time = current_time;
    pid->last_error = error;

    if (Co > pid->upper_limit) {
        Co = pid->upper_limit;
    }
    if (Co < pid->lower_limit) {
        Co = pid->lower_limit;
    }
    return Co;
}

//
// @简介：设置PID输出的上下限（同时也是积分项的上下限）
// @参数：pid - PID控制器指针
// @参数：lower_limit - 输出下限
// @参数：upper_limit - 输出上限
//
void pid_limit_config(pid_typedef *pid, float lower_limit, float upper_limit)
{
    pid->lower_limit = lower_limit;
    pid->upper_limit = upper_limit;
}

//
// @简介：重置PID状态：清零积分项和上次误差，刷新时间戳
// @参数：pid - PID控制器指针
// @注意：不会清除设定值sp
//
void pid_reset(pid_typedef *pid)
{
    pid->last_time = HAL_GetTick();
    pid->last_error = 0.0f;
    pid->integral_term = 0.0f;
}

