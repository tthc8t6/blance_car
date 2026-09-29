#include "app_control.h"
#include "pid.h"
#include "app_mpu6500.h"
#include "task.h"
#include "quick_math.h"
#include "app_encoder.h"
#include "app_motor.h"

pid_typedef pid_theta; //角度环PID控制器
pid_typedef pid_theta_dot; //角加速度环PID控制器
pid_typedef pid_velocity; //速度环PID控制器
pid_typedef pid_turn; //转向环PID控制器
float const g = 9.81f; //重力加速度 m/s^2
float const lp = 0.062f; //力臂 m
float const Rw = 0.03f; //轮半径 m
float const theta_max = 1.3963f; //倾角保护阈值 80° = 80*PI/180 rad
uint64_t last_time = 0; //上次积分时间
float omega_ref = 0.0f; //电机转速环设定值 单位rad/s


void App_Control_Init(void)
{
    pid_init(&pid_velocity, 10.0f, 1.0f, 0.0f); //速度环PID参数
    pid_limit_config(&pid_velocity, -0.5*g, 0.5*g); //速度环PID输出限幅 限制在+-0.5g 之间

    pid_init(&pid_turn, 1.0f, 0.0f, 0.0f); //转向环PID参数
    pid_limit_config(&pid_turn, -10.0f, 10.0f ); //转向环PID输出限幅 限制在+-10 rad/s 之间

    pid_init(&pid_theta, 4.0f, 0.0f, 0.0f); //角度环PID参数
    pid_limit_config(&pid_theta, -12.57f, 12.57f ); //角度环PID输出限幅 限制在+-4PI rad/s 之间

    pid_init(&pid_theta_dot, 10.0f, 10.0f, 0.0f); //角加速度环PID参数 限制在+-40PI rad/s^2 之间
    pid_limit_config(&pid_theta_dot, -125.7f, 125.7f); //角加速度环PID输出限幅

    App_Control_Reset(); //清空积分状态, 并把 last_time 设为当前时刻
}

//复位控制器的全部积分状态。在电机使能瞬间调用, 保证每次都从静止开始
void App_Control_Reset(void)
{
    pid_reset(&pid_velocity); //清速度环的积分项与微分历史
    pid_reset(&pid_theta);     //清角度环的积分项与微分历史
    pid_reset(&pid_theta_dot); //清角加速度环的积分项与微分历史
    pid_reset(&pid_turn); //清转向环的积分项与微分历史

    omega_ref = 0.0f; //第7步那个纯积分器自己也要清

    //电机环的设定值(sp)不在 pid_reset 的清理范围内, 必须显式归零。
    //否则从使能到下一次 App_Control_Process() 之间(最多5ms),
    //电机环仍按上一次的 omega_ref(可能是±40)全力驱动
    App_Motor_Set_Speed_L(0.0f);
    App_Motor_Set_Speed_R(0.0f);

    last_time = 0;
}

void App_Control_Process(void)
{
    PERIODIC(5) //每5ms更新一次数据 采样率为200Hz为最长耗时

    uint64_t now = App_GetMicroseconds(); //获取当前时间 单位us
    float deltaT = (now - last_time) * 1e-6f; //计算时间差 单位s

    //-2.读取编码器值
    float omega_l, omega_r; //左右轮角速度 单位rad/s
    App_Encoder_Get_Speed(&omega_l, &omega_r); //获取左右轮角速度
    float omega=(omega_l + omega_r)*0.5f;
    float theta = App_MPU6500_Get_Pitch() * 0.0174533f; //获取当前角度 单位rad 180/PI=0.0174533
    float theta_dot = App_MPU6500_Get_Gyro_X() * 0.0174533f; //获取当前角加速度 单位rad/s

    //-2.计算速度环PID的返回值x_dot
    float omega2 =-theta_dot * (lp+Rw) / Rw;
    float omega1 =omega - omega2;
    float x_dot = omega1 * Rw; 

    //-3.计算速度环PID+逆解算
    float theta_ref=qatan(PID_Compute(&pid_velocity,x_dot) / g);

    //1.设定角度环设定值
    pid_changesp(&pid_theta, theta_ref); //角度环设定值为theta_ref

   

    //2.5 倾角保护: 车已经摔倒, 无法再挽回。
    //  第6步的分母是 qcos(theta), theta 接近 ±90° 时它趋近 0,
    //  除法结果先被急剧放大, 到 90° 整直接除零得到 inf,
    //  inf 一旦进了第7步的积分器就再也出不来(inf/NaN 会一直传染下去)。
    //  这里顺带复位控制器, 使得车被重新扶起时从静止状态开始, 不会有冲击。
    if (theta > theta_max || theta < -theta_max)
    {
        App_Control_Reset();
        return;
    }

    //3.计算角度环PID输出
    float theta_dot_ref = PID_Compute(&pid_theta, theta); //角度环PID计算 输出为角加速度环设定值即theta_dot_ref

    //4.设定角加速度环目标值
    pid_changesp(&pid_theta_dot, theta_dot_ref); //角加速度环设定值为角度环PID后的输出即theta_dot_ref

    //5.计算角加速度环PID输出
    float theta_dot_dot_ref = PID_Compute(&pid_theta_dot, theta_dot); //角加速度环PID计算 输出为逆解算设定值即theta_dot_dot_ref

    //6.逆解算角加速度为线加速度
    float x_dot_dot_ref = (g * qsin(theta)- theta_dot_dot_ref * lp) / qcos(theta); //线加速度环设定值为角加速度环输出即theta_dot_dot_ref

    //7.计算电机转速
    if(last_time != 0)
    {
       omega_ref = omega_ref + 1.0f / Rw * x_dot_dot_ref * deltaT; //电机转速环设定值为线加速度环输出即x_dot_dot_ref
    }

    //8.设定电机转速环设定值为omega_ref
    if (omega_ref > 40.0f) {
        omega_ref = 40.0f; //限制电机转速环设定值在40 rad/s 之间
    }
    if (omega_ref < -40.0f) {
        omega_ref = -40.0f; //限制电机转速环设定值在-40 rad/s 之间
    }
   
    float gz = App_MPU6500_Get_Gyro_Z() * 0.0174533f; //获取Z轴角速度 单位rad/s
    float omega_diff = PID_Compute(&pid_turn, gz); //计算转向环PID输出
    
    App_Motor_Set_Speed_L(omega_ref + omega_diff); //设置左电机转速环设定值为omega_ref+omega_diff
    App_Motor_Set_Speed_R(omega_ref - omega_diff); //设置右电机转速环设定值为omega_ref-omega_diff

    //9.更新上次积分时间
    last_time = now; //更新积分时间
}

void App_Control_SetMoveSpeed(float MoveSpeed)
{
   pid_changesp(&pid_velocity, MoveSpeed); //设置速度环设定值为MoveSpeed
}

void App_Control_SetTurnSpeed(float TurnSpeed)
{
   pid_changesp(&pid_turn, TurnSpeed); //设置转向环设定值为TurnSpeed
}
