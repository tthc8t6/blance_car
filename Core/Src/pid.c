#include "pid.h"

//对PID控制器的Kp,Ki,Kd进行初始化
void pid_init(pid_typedef *pid, float kp, float ki, float kd) {
    // 初始化PID参数
    pid->kp = kp;
    pid->ki = ki;
    pid->kd = kd;
    pid->sp = 0.0f;

    // 初始化PID状态
    pid->last_time = HAL_GetTick();
    pid->last_error = 0.0f;
    pid->integral_term = 0.0f;

    // 初始化PID输出上下限为无穷大，可使用pid_limit_config函数进行设置
    pid->upper_limit = 3.4e+38f; // 设置为float类型的最大值
    pid->lower_limit = -3.4e+38f; // 设置为float类型最小值

}

void pid_changesp(pid_typedef *pid, float new_sp) {
    pid->sp = new_sp;
}

//计算PID输出
float PID_Compute(pid_typedef *pid, float feedback) {
    float error = pid->sp-feedback;
    float Cop =error * pid->kp; //比例项

    uint64_t current_time = HAL_GetTick();//获取当前时间 单位为毫秒
    float delta_time = (current_time - pid->last_time) / 1000.0f; //转化为秒
    float Cod = 0.0f; //微分项

    // 首次运行积分项和微分项为0
    if(pid->last_time != 0) {
        //仅在启用微分时才做这个除法：kd为0时若 delta_time 也为0，
        //(0误差变化)/(0时间差) 会得到NaN，而NaN乘0仍是NaN，会污染整个输出
        if(pid->kd != 0.0f) {
            Cod = pid->kd * (error - pid->last_error) / delta_time; //误差微分
        }

        if(pid->ki != 0.0f) {
            //积分项以「已乘过ki」的形式累加，单位与PID输出一致(V)，
            //这样紧接着才能直接拿输出上下限做抗饱和限幅。
            //若累加的是未乘ki的原始误差积分，再用输出量纲的上下限去截，
            //实际限到的是 limit*ki（ki=7时就是7倍），等于没限
            pid->integral_term = pid->integral_term + pid->ki * (error + pid->last_error) * delta_time * 0.5f;

            //限制积分项，避免积分饱和
            if (pid->integral_term > pid->upper_limit) {
                pid->integral_term = pid->upper_limit;
            }
            if (pid->integral_term < pid->lower_limit) {
                pid->integral_term = pid->lower_limit;
            }
        }
    }

    float Co = Cop + pid->integral_term + Cod; //PID输出

    //更新PID状态
    pid->last_time = current_time;
    pid->last_error = error;

    // 限制PID上下限输出
    if (Co > pid->upper_limit) {
        Co = pid->upper_limit;
    }
    if (Co < pid->lower_limit) {
        Co = pid->lower_limit;
    }
    return Co;
}

//设置PID输出上下限
void pid_limit_config(pid_typedef *pid, float lower_limit, float upper_limit)
{
    pid->lower_limit = lower_limit;
    pid->upper_limit = upper_limit;
}

//重置PID状态
void pid_reset(pid_typedef *pid)
{
    pid->last_time = HAL_GetTick();
    pid->last_error = 0.0f;
    pid->integral_term = 0.0f;
}

