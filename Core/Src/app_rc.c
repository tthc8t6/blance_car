#include "app_rc.h"
#include "usart.h"
#include "stdio.h"
#include <string.h>
#include "app_control.h"

#define CMD_MAX_LEN 64

static char intBuf[CMD_MAX_LEN]; //专门用于中断程序缓冲区
static char transBuf[CMD_MAX_LEN]; //用于数据转运的缓冲区
static char procBuf[CMD_MAX_LEN]; //用于进程函数的缓冲区
static uint8_t volatile LinReceivedFlag = 0;//一行字符串接收完成标志
static uint16_t intBufCursor = 0;//中断缓冲区光标位置
static uint8_t rxData; //用于接收单个字节的数据缓冲区

void App_RC_Init(void)
{
    HAL_UART_Receive_IT(&huart3, &rxData, 1);
}

void App_RC_Clear(void)
{
    intBufCursor = 0;
    LinReceivedFlag = 0;
    memset(intBuf, 0, CMD_MAX_LEN);
    memset(transBuf, 0, CMD_MAX_LEN);
    memset(procBuf, 0, CMD_MAX_LEN);
    App_Control_SetMoveSpeed(0); 
    App_Control_SetTurnSpeed(0);
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if(huart->Instance == USART3)
    {
        HAL_UART_Receive_IT(&huart3, &rxData, 1);   // 出错后重新预约
    }
}


void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if(huart->Instance == USART3)
    {
       HAL_UART_Receive_IT(&huart3, &rxData, 1);
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
           LinReceivedFlag = 1;
       }
    }

}


void App_RC_Process(void)
{
    if(LinReceivedFlag)
    {
        __disable_irq();
       strcpy(procBuf, transBuf); //将转运缓冲区的内容复制到进程缓冲区
       __enable_irq();
       LinReceivedFlag = 0; //清除接收标志

       if(strncasecmp(procBuf, "move ", 5) == 0)
       {
           int turnSpeed, moveSpeed;
           if(sscanf(procBuf, "move %d %d", &turnSpeed, &moveSpeed) == 2)
           {
             App_Control_SetMoveSpeed(-moveSpeed*0.01f*0.7f); //设置移动速度
             App_Control_SetTurnSpeed(-turnSpeed*0.01f*15.0f); //设置转向速度
           }
       }
    }
}

