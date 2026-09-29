#include "app_rc.h"
#include "usart.h"
#include "stdio.h"
#include <string.h>
#include "app_control.h"

#define CMD_MAX_LEN 64
#define RC_SCALE 0.01f //遥控指令-100~100换算为-1~1
#define MOVE_SPEED_MAX 0.7f //最大移动速度 单位m/s
#define TURN_SPEED_MAX 15.0f //最大转向速度 单位rad/s

static char intBuf[CMD_MAX_LEN]; //中断缓冲区
static char transBuf[CMD_MAX_LEN]; //数据转运缓冲区
static char procBuf[CMD_MAX_LEN]; //进程函数缓冲区
static uint8_t volatile LineReceivedFlag = 0;//一行字符串接收完成标志
static uint16_t intBufCursor = 0;//中断缓冲区光标位置
static uint8_t rxData; //单字节接收缓冲区

//
// @简介：初始化遥控接收：预约USART3的1字节中断接收
// @参数：无
//
void App_RC_Init(void)
{
    HAL_UART_Receive_IT(&huart3, &rxData, 1);
}

//
// @简介：清空遥控接收缓冲区与标志，并把移动速度、转向速度设定值清零
// @参数：无
//
void App_RC_Clear(void)
{
    intBufCursor = 0;
    LineReceivedFlag = 0;
    memset(intBuf, 0, CMD_MAX_LEN);
    memset(transBuf, 0, CMD_MAX_LEN);
    memset(procBuf, 0, CMD_MAX_LEN);
    App_Control_SetMoveSpeed(0);
    App_Control_SetTurnSpeed(0);
}

//
// @简介：串口错误回调（如溢出ORE）：重新预约USART3的1字节接收，否则接收会永久停止
// @参数：huart - 串口句柄
//
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if(huart->Instance == USART3)
    {
        HAL_UART_Receive_IT(&huart3, &rxData, 1);
    }
}


//
// @简介：串口接收完成回调：重新预约接收，按字节拼成一行，收到换行符后置位一行接收完成标志
// @参数：huart - 串口句柄
//
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if(huart->Instance == USART3)
    {
       HAL_UART_Receive_IT(&huart3, &rxData, 1); //Receive_IT只预约一次，每字节都要重新预约
       if(rxData != '\n')
       {
           intBuf[intBufCursor++] = rxData;
           if(intBufCursor >= CMD_MAX_LEN){intBufCursor =0;}
       }
       else
       {
           intBuf[intBufCursor] = '\0';
           intBufCursor = 0;
           strcpy(transBuf, intBuf);
           LineReceivedFlag = 1;
       }
    }

}


//
// @简介：解析一行遥控指令 "move <转向> <速度>"，换算后设置转向环与速度环的设定值
// @参数：无
// @注意：放在主循环中调用，无新指令时不做任何事
//
void App_RC_Process(void)
{
    if(LineReceivedFlag)
    {
        __disable_irq();
       strcpy(procBuf, transBuf);
       __enable_irq();
       LineReceivedFlag = 0;

       if(strncasecmp(procBuf, "move ", 5) == 0)
       {
           int turnSpeed, moveSpeed;
           if(sscanf(procBuf, "move %d %d", &turnSpeed, &moveSpeed) == 2)
           {
             App_Control_SetMoveSpeed(-moveSpeed*RC_SCALE*MOVE_SPEED_MAX); //速度 单位m/s
             App_Control_SetTurnSpeed(-turnSpeed*RC_SCALE*TURN_SPEED_MAX); //转向速度 单位rad/s
           }
       }
    }
}

