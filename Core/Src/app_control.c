#include "app_control.h"
#include "pid.h"
#include "app_mpu6500.h"
#include "task.h"
#include "quick_math.h"
#include "app_encoder.h"
#include "app_motor.h"

#define DEG2RAD 0.0174533f //度转弧度 PI/180
#define OMEGA_REF_MAX 40.0f //电机转速环设定值限幅 单位rad/s

static pid_typedef pid_theta; //角度环PID控制器
static pid_typedef pid_theta_dot; //角加速度环PID控制器
static pid_typedef pid_velocity; //速度环PID控制器
static pid_typedef pid_turn; //转向环PID控制器
static const float g = 9.81f; //重力加速度 m/s^2
static const float lp = 0.062f; //力臂 m
static const float Rw = 0.03f; //轮半径 m
static const float theta_max = 1.3963f; //倾角保护阈值 80° = 80*PI/180 rad
static uint64_t last_time = 0; //上次积分时间
static float omega_ref = 0.0f; //电机转速环设定值 单位rad/s

//
// @简介：初始化速度环、转向环、角度环、角加速度环四个PID的参数与输出限幅，并复位控制器
// @参数：无
//
void App_Control_Init(void)
{
    pid_init(&pid_velocity, 10.0f, 1.0f, 0.0f);
    pid_limit_config(&pid_velocity, -0.5f*g, 0.5f*g); //输出限幅 +-0.5g

    pid_init(&pid_turn, 1.0f, 0.0f, 0.0f);
    pid_limit_config(&pid_turn, -10.0f, 10.0f ); //输出限幅 +-10 rad/s

    pid_init(&pid_theta, 4.0f, 0.0f, 0.0f);
    pid_limit_config(&pid_theta, -12.57f, 12.57f ); //输出限幅 +-4PI rad/s

    pid_init(&pid_theta_dot, 10.0f, 10.0f, 0.0f);
    pid_limit_config(&pid_theta_dot, -125.7f, 125.7f); //输出限幅 +-40PI rad/s^2

    App_Control_Reset();
}

//
// @简介：复位控制器全部状态：清四个PID的积分与历史、清纯积分器omega_ref、电机设定值归零
// @参数：无
// @注意：按键按下和倾角保护触发时调用；pid_reset不清设定值，所以电机设定值在这里显式归零
//
void App_Control_Reset(void)
{
    pid_reset(&pid_velocity);
    pid_reset(&pid_theta);
    pid_reset(&pid_theta_dot);
    pid_reset(&pid_turn);

    omega_ref = 0.0f; //第7步的纯积分器

    App_Motor_Set_Speed_L(0.0f);
    App_Motor_Set_Speed_R(0.0f);

    last_time = 0;
}

//
// @简介：平衡控制主流程，每5ms执行一次：速度环->角度环->角加速度环->逆解算->积分得电机转速->叠加转向环，写入左右电机设定值
// @参数：无
// @注意：倾角超过80度时复位控制器并直接返回
//
void App_Control_Process(void)
{
    PERIODIC(5) //每5ms执行一次 200Hz

    uint64_t now = App_GetMicroseconds(); //当前时间 单位us
    float deltaT = (now - last_time) * 1e-6f; //时间差 单位s

    //-2.读取编码器值
    float omega_l, omega_r; //左右轮角速度 单位rad/s
    App_Encoder_Get_Speed(&omega_l, &omega_r);
    float omega=(omega_l + omega_r)*0.5f;
    float theta = App_MPU6500_Get_Pitch() * DEG2RAD; //当前角度 单位rad 
    float theta_dot = App_MPU6500_Get_Gyro_X() * DEG2RAD; //当前角速度 单位rad/s

    //-2.计算速度环PID的返回值x_dot
    float omega2 =-theta_dot * (lp+Rw) / Rw;
    float omega1 =omega - omega2;
    float x_dot = omega1 * Rw;

    //-3.计算速度环PID+逆解算
    float theta_ref=qatan(pid_compute(&pid_velocity,x_dot) / g);

    //1.设定角度环设定值
    pid_changesp(&pid_theta, theta_ref);

    //2.5 倾角保护: 倾角过大时cos(theta)趋近0会除零，复位后车被扶起从静止开始
    if (theta > theta_max || theta < -theta_max)
    {
        App_Control_Reset();
        return;
    }

    //3.计算角度环PID输出
    float theta_dot_ref = pid_compute(&pid_theta, theta); //输出为角加速度环设定值

    //4.设定角加速度环目标值
    pid_changesp(&pid_theta_dot, theta_dot_ref);

    //5.计算角加速度环PID输出
    float theta_dot_dot_ref = pid_compute(&pid_theta_dot, theta_dot); //输出为逆解算输入

    //6.逆解算角加速度为线加速度
    float x_dot_dot_ref = (g * qsin(theta)- theta_dot_dot_ref * lp) / qcos(theta);

    //7.计算电机转速
    if(last_time != 0)
    {
       omega_ref = omega_ref + 1.0f / Rw * x_dot_dot_ref * deltaT;
    }

    //8.设定电机转速环设定值为omega_ref
    if (omega_ref > OMEGA_REF_MAX) {
        omega_ref = OMEGA_REF_MAX; //限幅 +-40 rad/s
    }
    if (omega_ref < -OMEGA_REF_MAX) {
        omega_ref = -OMEGA_REF_MAX;
    }

    float gz = App_MPU6500_Get_Gyro_Z() * DEG2RAD; //Z轴角速度 单位rad/s
    float omega_diff = pid_compute(&pid_turn, gz); //转向环输出

    App_Motor_Set_Speed_L(omega_ref + omega_diff);
    App_Motor_Set_Speed_R(omega_ref - omega_diff);

    //9.更新上次积分时间
    last_time = now;
}

//
// @简介：改变平衡车移动的速度（设置速度环设定值）
// @参数：MoveSpeed - 速度，单位m/s，遥控指令映射范围为±0.7m/s
//
void App_Control_SetMoveSpeed(float MoveSpeed)
{
   pid_changesp(&pid_velocity, MoveSpeed);
}

//
// @简介：改变平衡车转向的速度（设置转向环设定值）
// @参数：TurnSpeed - 转向速度，单位rad/s，遥控指令映射范围为±15rad/s
//
void App_Control_SetTurnSpeed(float TurnSpeed)
{
   pid_changesp(&pid_turn, TurnSpeed);
}
