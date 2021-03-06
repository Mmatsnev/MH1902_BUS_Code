/****************************************Copyright (c)**********************************
========================================================================================
                    文件描述

========================================================================================
编写: cj  tel:15767637491  qq ：773208906
日期：2019-06-29 09点54分
***************************************************************************************/
#include "psam_bsp.h"
#include "delay.h"
#include "psam.h"
#include "stdio.h"
#include "mhscpu.h"
#include "delay.h"
#include "timer.h"

volatile uint32_t iso7816_etu_flg = 0;
volatile uint32_t iso7816_baud;//波特率
volatile unsigned char selPsam;//选择PSAM卡槽
/***************************************************************************************
函数名称: void psam_init(void)
函数功能: psam卡 相关io脚初始化， 波特率定时器
          定时器pwm  提供时钟
输入参数:
返 回 值:
***************************************************************************************/
void psam_init(unsigned char psam)
{
    GPIO_InitTypeDef GPIO_InitStruct;
    //======================================================================================
    // psam 复位管教
    // psam1 rst PC13       psam2 rst PC10
    // psam1 clk PA2       psam2 clk PA3
    // psam1 io  PC12      psam2  io  PC9
    //======================================================================================
    if(psam== 1)
    {
        GPIO_InitStruct.GPIO_Pin = GPIO_Pin_10;
        GPIO_InitStruct.GPIO_Mode = GPIO_Mode_Out_PP;
        GPIO_InitStruct.GPIO_Remap = GPIO_Remap_1;
        GPIO_Init(GPIOC, &GPIO_InitStruct);//PC10  ========> PSAM2 RST

        GPIO_InitStruct.GPIO_Pin = GPIO_Pin_9;
        GPIO_InitStruct.GPIO_Mode = GPIO_Mode_Out_OD_PU;
        GPIO_InitStruct.GPIO_Remap = GPIO_Remap_1;
        GPIO_Init(GPIOC, &GPIO_InitStruct);//PC9  ========> PSAM2 IO

        psam_data_set(psam, 1); // PSAM卡data脚设置为 输入
        pasm_rst_write(psam, 0);//PSAM卡槽1  RST脚拉低
    }
    else if(psam == 2)
    {

        GPIO_InitStruct.GPIO_Pin = GPIO_Pin_13;
        GPIO_InitStruct.GPIO_Mode = GPIO_Mode_Out_PP;
        GPIO_InitStruct.GPIO_Remap = GPIO_Remap_1;
        GPIO_Init(GPIOC, &GPIO_InitStruct);//PC13  ========> CPU RST

        GPIO_InitStruct.GPIO_Pin = GPIO_Pin_12;
        GPIO_InitStruct.GPIO_Mode = GPIO_Mode_IN_FLOATING;
        GPIO_InitStruct.GPIO_Remap = GPIO_Remap_1;
        GPIO_Init(GPIOC, &GPIO_InitStruct);//PC12  ========> CPU IO

        psam_data_set(psam, 1);
        pasm_rst_write(psam, 0);//PSAM卡槽2 RST脚  拉低

    }
    else if(psam ==3)
    {


    }
    GPIO_InitStruct.GPIO_Pin = GPIO_Pin_13;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_IPU ;
    GPIO_InitStruct.GPIO_Remap = GPIO_Remap_1;
    GPIO_Init(GPIOA, &GPIO_InitStruct);//PA13  ========> 接触式卡插入感应 引脚
    //======================================================================================
    //PSAM卡 CLK初始化
    //======================================================================================
//    iso7816_init_baud(0, 5376);
//    iso7816_init_baud(1, 5376);//PSAM卡 波特率初始化
    tim_pwm_init();//定时器输出PWM给PSAM
    psam_power_on();//PSAM卡上电
}
/***************************************************************************************
函数名称: void psam_iso7816_send_data(uint8_t psam, uint8_t byte)
函数功能: 7816协议发送数据
输入参数:
返 回 值:
***************************************************************************************/
void iso7816_send_byte(uint8_t psam, uint8_t byte)
{
    uint8_t even_bit = 0, i;
    //uint32_t wait_etu = TICK_1US / iso7816_baud;;

    //======================================================================================
    // 设置psam  data线为输出
    //======================================================================================
    psam_data_set(psam, 0);

    //======================================================================================
    // 发送起始位
    //======================================================================================
    psam_data_write(psam, 0);
    delayus(etu_time);

    //======================================================================================
    // 发送数据位8位
    //======================================================================================
    for (i = 0; i < 8; i++)
    {
        if (byte & (0x01 << i))
        {
            psam_data_write(psam, 1);
            even_bit++;
        }
        else
        {
            psam_data_write(psam, 0);
        }
        delayus(etu_time);
    }

    //======================================================================================
    // 发送校验
    //======================================================================================
    if (even_bit & 0x01)
        psam_data_write(psam,1);
    else
        psam_data_write(psam,0);

    delayus(etu_time);


    //======================================================================================
    // 发送结束 data脚 拉高
    //======================================================================================
    psam_data_set(psam, 1); // 拉高
    delayus(etu_time);
    delayus(etu_time>>1);

}

/***************************************************************************************
函数名称: unsigned char psam_iso7816_rcv_data(uint8_t psam, uint8_t *buf, uint32_t timeover)
函数功能: iso7816协议接受数据
           地位在前，一位开始位 + 8位数据位 + 1位校验(偶校验)
输入参数:
返 回 值:
***************************************************************************************/
unsigned char iso7816_rcv_byte(uint8_t psam, uint8_t *buf, uint32_t timeover)
{
    uint8_t i, data = 0;
    uint32_t cnt = 0;
    //======================================================================================
    // 设置psam  data线为输入
    //======================================================================================
    psam_data_set(psam, 1);
    //======================================================================================
    // 等待起始位
    //======================================================================================
    while (1)
    {
        cnt++;
        if (psam_data_read(psam) == 0)
            break;
        // atr响应失败
        if (cnt >= timeover)
        {
            return 1;
        }
        delayus(etu_time>>4);
    }
    //======================================================================================
    // 读取数据地位在前
    //======================================================================================

    delayus(etu_time>>1);
    delayus(etu_time);

    for (i = 0; i < 8; i++)
    {
        data >>= 1;
        if (psam_data_read(psam) != 0)
            data |= 0x80;
        delayus(etu_time);
    }
    *buf = data;

    delayus(etu_time>>1);
    delayus(etu_time);
    return 0;
}
/***************************************************************************************
函数名称: void psam1_data_set(unsigned char in_or_out)
函数功能: 配置psam1 为输入或输出模式
输入参数: 1 -- 输入
          0 -- 输出
返 回 值:
***************************************************************************************/
void psam_data_set(uint8_t psam, uint8_t in_or_out)
{
    GPIO_InitTypeDef         GPIO_InitStructure;
    //======================================================================================
    // psam 卡槽选择
    //======================================================================================
    if (psam == 1)
    {
        GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_9; //PSAM1 IO ========>PC9
    }
    else if(psam == 2)
    {
        GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_12;//CPU IO ==========>PC12
    }
    else if(psam == 3)
    {


    }
    //======================================================================================
    // psam1 io  PC9       psam  io  PC12
    //======================================================================================
    if (in_or_out == 0)//输出
    {
//        GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_Out_PP;
//        GPIO_InitStructure.GPIO_Remap = GPIO_Remap_1;
//        GPIO_Init(GPIOC, &GPIO_InitStructure);

        GPIOC->OEN  &= ~GPIO_InitStructure.GPIO_Pin;		//Set Init Status

    }
    else//输入
    {
//        GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_IN_FLOATING;
//        GPIO_InitStructure.GPIO_Remap = GPIO_Remap_1;
//        GPIO_Init(GPIOC, &GPIO_InitStructure);
//
        GPIOC->OEN |= GPIO_InitStructure.GPIO_Pin;		//Input Mode

    }
}
/***************************************************************************************
函数名称: uint8_t psam1_data_read(void)
函数功能: 读取psam卡数据线的状态
输入参数:
返 回 值:
***************************************************************************************/
uint8_t psam_data_read(uint8_t psam)
{
    if (psam == 1)
    {
        //GPIO_ReadInputDataBit(GPIOC, GPIO_Pin_9)
        if (GPIOC->IODR & (1<<25))//PSAM1 IO========>PC9
            return 1;
        else
            return 0;
    }
    else if(psam == 2)
    {
        // GPIO_ReadInputDataBit(GPIOC, GPIO_Pin_12)
        if (GPIOC->IODR &(1<<28))//CPU IO=========>PC12
            return 1;
        else
            return 0;
    }
    else if(psam == 3)
    {

    }



}

/***************************************************************************************
函数名称: psam_data_write(uint8_t psam, uint8_t enable)
函数功能: 写psam1数据线的状态
输入参数:
返 回 值:
***************************************************************************************/
void psam_data_write(uint8_t psam, uint8_t enable)
{
    if (psam == 1)
    {
        if (enable == 0)
            //GPIO_ResetBits(GPIOC, GPIO_Pin_9);//PSAM IO=========>PC9
            GPIOC->BSRR |=1<<25;
        else
            //GPIO_SetBits(GPIOC, GPIO_Pin_9);

            GPIOC->BSRR |=1<<9;
    }
    else if(psam == 2)
    {
        if (enable == 0)
            //GPIO_ResetBits(GPIOC, GPIO_Pin_12);//CPU IO=========>PC12
            GPIOC->BSRR |=1<<28;

        else
            // GPIO_SetBits(GPIOC, GPIO_Pin_12);
            GPIOC->BSRR |=1<<12;
    }
    else if(psam == 3)
    {



    }
}

/***************************************************************************************
函数名称: void psam_power_on(void)
函数功能:  psam卡上电
输入参数:
返 回 值:
***************************************************************************************/
void psam_power_on(void)
{
    //GPIO_ResetBits(GPIOA, GPIO_Pin_1);
}
/***************************************************************************************
函数名称: void psam_power_off(void)
函数功能: psam卡掉电
输入参数:
返 回 值:
***************************************************************************************/
void psam_power_off(void)
{
    //GPIO_SetBits(GPIOA, GPIO_Pin_1);
}
/***************************************************************************************
函数名称: void pasm_rst_write(uint8_t psam, uint8_t enable)
函数功能: psam1 rst PC10       psam2 rst PC13
输入参数:
返 回 值:
***************************************************************************************/
void pasm_rst_write(uint8_t psam, uint8_t enable)
{
    if (psam == 1)
    {
        if (enable == 0)
            //GPIO_ResetBits(GPIOC, GPIO_Pin_10);
            GPIOC->BSRR |=1<<26;
        else
            // GPIO_SetBits(GPIOC, GPIO_Pin_10);// PC10 =========>PSAM1 RST
            GPIOC->BSRR |=1<<10;
    }
    else if(psam == 2)
    {
        if (enable == 0)
            //GPIO_ResetBits(GPIOC, GPIO_Pin_13);
            GPIOC->BSRR |=1<<29;

        else
            // GPIO_SetBits(GPIOC, GPIO_Pin_13);//PC13 ============>CPU1 RST
            GPIOC->BSRR |=1<<13;
    }
    else if(psam == 3)
    {

    }
}

/***************************************************************************************
函数名称: void iso7816_psam_usart_init(uint8_t psam, uint32_t baud)
函数功能: 设置数据线为输入模式，并根据波特兰设置etu时钟的重装载值
输入参数:
返 回 值:
***************************************************************************************/
void iso7816_init_baud(uint8_t psam, uint32_t baud)
{
    iso7816_baud = baud;
    psam_data_set(psam, 1);
}
/***************************************************************************************
函数名称: void tim4_init(void)
函数功能: 此定时器用于etu延时
输入参数:
返 回 值:
***************************************************************************************/
void tim4_init(void)
{

    //==================================================================================
    // PCLK 计时1ms TIM_Period =0xBB80=48000  可推出 1us=48   186us = 48*186 = 8928
    //==================================================================================
    TIM_InitTypeDef TIM_TimeBaseStructure;
    NVIC_InitTypeDef nvic;

    TIM_TimeBaseStructure.TIMx = TIM_4;
    TIM_TimeBaseStructure.TIM_Period = 8928;//186us
    TIM_DeInit(TIMM0);
    TIM_Init(TIMM0, &TIM_TimeBaseStructure);
    TIM_ITConfig(TIMM0, TIM_4, ENABLE);
    TIM_Cmd(TIMM0, TIM_4, ENABLE);

    nvic.NVIC_IRQChannel = TIM0_4_IRQn;
    nvic.NVIC_IRQChannelCmd = ENABLE;
    nvic.NVIC_IRQChannelPreemptionPriority = 2;
    nvic.NVIC_IRQChannelSubPriority = 2;
    NVIC_Init(&nvic);
}
/***************************************************************************************
函数名称: void tim_pwm_init(void)
函数功能:  定时器pwm2/3 提供时钟
输入参数:
返 回 值:
***************************************************************************************/
void tim_pwm_init(void)
{

    //======================================================================================
    // psam CLK管脚
    // psam1 rst PC13       psam2 rst PC10
    // psam1 clk PA2       psam2 clk PA3
    // psam1 io  PC12      psam2  io  PC9
    //======================================================================================
    GPIO_InitTypeDef GPIO_InitStruct;
    GPIO_InitStruct.GPIO_Pin = GPIO_Pin_3;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStruct.GPIO_Remap = GPIO_Remap_2;
    GPIO_Init(GPIOA, &GPIO_InitStruct);//PA3  ========> PSAM2 CLK

    GPIO_InitStruct.GPIO_Pin = GPIO_Pin_2;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStruct.GPIO_Remap = GPIO_Remap_2;
    GPIO_Init(GPIOA, &GPIO_InitStruct);//PA2  ========> CPU CLK


    TimerPWMSetStructInit();
    TIMER_Configuration();// 定时器输出PWM初始化

    TIM_Cmd(TIMM0, TIM_2, ENABLE);
    TIM_Cmd(TIMM0, TIM_3, ENABLE);

}
/***************************************************************************************
函数名称: void iso7816_wait_etu(uint32_t etu)
函数功能: 7816协议等带etu  此延时必须准确
输入参数:
返 回 值:
***************************************************************************************/
void iso7816_wait_etu(uint32_t etu)
{
    //TIM_Cmd(TIM2, DISABLE);
    //TIM_SetCounter(TIM2, 0);
    //TIM_SetAutoreload(TIM2, etu);
    //iso7816_etu_flg = 1;
    //TIM_Cmd(TIM2, ENABLE);
    iso7816_etu_flg = 1;
    TIM_Cmd(TIMM0, TIM_4, DISABLE);
    //TIMM0->TIM[4].LoadCount = 0;
    TIMM0->TIM[4].LoadCount = etu;

    TIM_Cmd(TIMM0, TIM_4, ENABLE);
    while (iso7816_etu_flg);
}
/***************************************************************************************
函数名称: void TIM0_4_IRQHandler(void)
函数功能: 此定时器用于etu延时
输入参数:
返 回 值:
***************************************************************************************/
void TIM0_4_IRQHandler(void)
{
    TIM_ClearITPendingBit(TIMM0, TIM_4);
    TIM_Cmd(TIMM0, TIM_4, DISABLE);
    iso7816_etu_flg = 0;
}

/***************************************************************************************
    end of file
***************************************************************************************/
