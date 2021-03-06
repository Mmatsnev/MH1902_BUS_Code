/****************************************Copyright (c)**********************************
========================================================================================
                    文件描述

========================================================================================
编写: cj  tel:15767637491  qq ：773208906
日期：2019-06-29 09点54分
***************************************************************************************/
#include "psam.h"
#include "delay.h"
#include "psam_bsp.h"
#include "string.h"
#include "stdio.h"
#include "mhscpu.h"
#include "emv_core.h"

uint8_t psam_atr_tab[40];
unsigned char psam_atr_len = 0;//复位卡返回的ATR长度
uint8_t psam_err_arq_data = 0;
ISO7816_ATR psam_rest_atr;
struct emv_atr res_atr[4];

// F ? D
const uint32_t f_tab[16] = {0, 372, 558, 744, 1116, 1488, 1860, 0, 0, 512, 768, 1024, 1536, 2048, 0, 0};
const uint32_t d_tab[8] = {0, 1, 2, 4, 8, 16, 0, 0};
uint8_t ATR_init_iso7816_flag = 0;

/***************************************************************************************
函数名称: uint32_t psam1_reset(uint8_t *response)
函数功能: psam1卡复位应答
输入参数:
返 回 值: 0: 复位失败  1：复位成功
***************************************************************************************/
uint8_t psam_reset(uint8_t psam)
{
    uint8_t i, j, data, err;
    // uint8_t protocol;
    ATR_init_iso7816_flag = 0;
    //==================================================================================
    // 初始化7816
    //==================================================================================
    psam_init(psam);// 初始化7816
    delayms(10);
    //==================================================================================
    // 清全局变量参数
    //==================================================================================
//    psam_rest_atr.TS      = 0;
//    psam_rest_atr.T0      = 0;
//    psam_rest_atr.Tlength = 0;
//    psam_rest_atr.Hlength = 0;
//    memset(psam_atr_tab, 0 , 40);
//    memset(psam_rest_atr.T, 0, SETUP_LENGTH);
//    memset(psam_rest_atr.H, 0, HIST_LENGTH);
    //==================================================================================
    // 复位线拉高, 准备接受ATR
    //==================================================================================
    pasm_rst_write(psam, 1);
    //==================================================================================
    // 准备接受ATR
    //==================================================================================
    for (i = 0, j = 0; i < 40; i++)
    {
        err = iso7816_rcv_byte(psam, &data, ISO7816_RCV_TIMEOUT);
        //printf("rev:%d\r\n",err);
        if (!err)
        {
            if ((j == 0 && (data == 0x3b || data == 0x3f)) || j > 0)
                psam_atr_tab[j++] = data;
        }
        else if (j > 0)
        {
            break;
        }
    }
//		printf("ATR:");
//		for(i=0;i<18;i++)
//		{
//		 printf("%02X",psam_atr_tab[i]);
//		}
//		printf("\r\n");
    //==================================================================================
    // 判断是否应答成功
    //==================================================================================
    if (psam_atr_tab[0] == 0x00 || psam_atr_tab[0] == 0XFF)
    {
        //printf("psam reset err!!\r\n");
        return 0;
    }
    else
    {

        //printf("psam reset sucess!!\r\n");
        // 解码复位应答数据
        //iso7816_decode_atr(&psam_atr_tab[0], &psam_rest_atr); // == protocol
        psam_atr_len = j;//获取ATR长度
        iso7816_analy_atr(psam_atr_tab, &res_atr[psam]);
        // 根据解码数据重新初始化iso7816,多次初始化减小误差
        //for(i = 0; i < 100; i++)
        {
            //err = atr_init_psam_iso7816(psam);
            //if (!err)
            {
                ATR_init_iso7816_flag = 1;
                //printf("ATR init iso7816 sucess!!\r\n");
                //break;
            }
        }
        // 等待卡稳定
        //delay_ms(400);
        return 1;
    }
}

/***************************************************************************************
函数名称: uint8_t SC_decode_Answer2reset(uint8_t *card)
函数功能:  解码复位应答数据
输入参数:
返 回 值:  卡的协议
***************************************************************************************/
uint8_t iso7816_decode_atr(uint8_t *atr_tab, ISO7816_ATR *atr)
{
    uint32_t i = 0, flag = 0, buf = 0, protocol = 0;
    //==================================================================================
    //                  ATR结构
    // |      数据元       |      说明    |
    // |       TS          |   起始字符   |
    // |       TO          |   格式字符   |
    // |TA1, TB1...TD1,... |   接口字符   |
    // |   T1,T2, ...TK    |   历史字符   |
    // |       TCK         |   校验字符   |
    // 测试ATR数据
    // 3b 9c 94 80 1f 47 80 31 e0 73 fe 21 1b 54 53 04 71 6b 2f
    // TS   0X3B
    // T0   0X9C    1001 1100 TA1, TD1 存在,
    // TA1  0X94    1001 0100 高半字节代表FI, 低半字节代表DI, FI = 9, F为512 DI = 4
    // TD1  0X80    1000 0000 低4位为0, 代表T0协议, TD2存在

    //1etu = F/D * f = 372 / f
    // baud = f / 372
    //==================================================================================
    atr->TS = atr_tab[0];  // 0x3b   起始字符, 正向约定
    atr->T0 = atr_tab[1];  // 0x9c   格式字符, 1001 1100  TA1, TD1存在,

    atr->Hlength = atr->T0 & (uint8_t)0x0F;   // 0X9C & 0X0f  = 12 卡片自定义信息的历史字节数
    if ((atr->T0 & (uint8_t)0x80) == 0x80)
    {
        flag = 1;    // TD1 存在标志
    }
    // 求T(A~D)1接口字符的长度
    for (i = 0; i < 4; i++)
    {
        atr->Tlength = atr->Tlength + (((atr->T0 & (uint8_t)0xF0) >> (4 + i)) & (uint8_t)0x1);
    }
    // 把 T(A~D)1 copy打SC_A2R.T中
    for (i = 0; i < atr->Tlength; i++)
    {
        atr->T[i] = atr_tab[i + 2];
    }
    protocol = atr->T[atr->Tlength - 1] & (uint8_t)0x0F;      // TD1 & 0X0F = 0X00  T0协议
    while (flag)
    {
        if ((atr->T[atr->Tlength - 1] & (uint8_t)0x80) == 0x80)
        {
            flag = 1;    // TD2 是否存在
        }
        else
        {
            flag = 0;
        }

        buf = atr->Tlength;
        atr->Tlength = 0;

        for (i = 0; i < 4; i++)
        {
            atr->Tlength = atr->Tlength + (((atr->T[buf - 1] & (uint8_t)0xF0) >> (4 + i)) & (uint8_t)0x1);
        }

        for (i = 0; i < atr->Tlength; i++)
        {
            atr->T[buf + i] = atr_tab[i + 2 + buf];
        }
        atr->Tlength += (uint8_t)buf;
    }
    for (i = 0; i < atr->Hlength; i++)
    {
        atr->H[i] = atr_tab[i + 2 + atr->Tlength];
    }

    return (uint8_t)protocol;
}
/***************************************************************************************
函数名称: uint8_t ATR_init_iso7816(void)
函数功能: 获取到复位应答的处理
输入参数:
返 回 值:
***************************************************************************************/
uint8_t atr_init_psam_iso7816(uint8_t psam)
{
    // RCC_ClocksTypeDef RCC_ClocksStatus;
    uint32_t workingbaudrate = 0, apbclock = 0;
    uint8_t byte = 0, PTSConfirmStatus = 1, err;
    //USART_TypeDef *uart;
    //==================================================================================
    // 获取iso7816 CLK频率

    apbclock = 2 * TICK_1US;

    // 终端回送TA1
    if ((psam_rest_atr.T0 & (uint8_t)0x10) == 0x10)  // 判断TA1存在
    {
        if (psam_rest_atr.T[0] != 0x11)  // PA1 = 0x11 F/D  372
        {
            //USART_DMACmd(uart, USART_DMAReq_Rx, ENABLE);
            //==================================================================================
            // 终端发送报文, 卡响应与终端一样的报文
            //==================================================================================
            psam_err_arq_data = 0xFF;
            iso7816_send_byte(psam, psam_err_arq_data);

            psam_err_arq_data = 0x10;
            iso7816_send_byte(psam, psam_err_arq_data);

            psam_err_arq_data = psam_rest_atr.T[0];
            iso7816_send_byte(psam, psam_err_arq_data);

            psam_err_arq_data = (uint8_t)0xFF^(uint8_t)0x10^(uint8_t)psam_rest_atr.T[0];
            iso7816_send_byte(psam, psam_err_arq_data);

            //USART_DMACmd(uart, USART_DMAReq_Rx, DISABLE);

            err = iso7816_rcv_byte(psam, &byte, ISO7816_RCV_TIMEOUT);
            if (err || byte != 0xff)
            {
                PTSConfirmStatus = 0x00;
            }

            err = iso7816_rcv_byte(psam, &byte, ISO7816_RCV_TIMEOUT);
            if (err || byte != 0x10)
            {
                PTSConfirmStatus = 0x00;
            }

            err = iso7816_rcv_byte(psam, &byte, ISO7816_RCV_TIMEOUT);
            if (err || byte != psam_rest_atr.T[0])
            {
                PTSConfirmStatus = 0x00;
            }

            err = iso7816_rcv_byte(psam, &byte, ISO7816_RCV_TIMEOUT);
            if (err || byte != ((uint8_t)0xFF^(uint8_t)0x10^(uint8_t)psam_rest_atr.T[0]))
            {
                PTSConfirmStatus = 0x00;
            }
            else
            {
                PTSConfirmStatus = 0x00;
            }
            //==================================================================================
            // 重新初始化iso7816 波特率
            //==================================================================================
            //if(PTSConfirmStatus == 0x01)            // 此句可以省略
            {
                workingbaudrate = apbclock * d_tab[(psam_rest_atr.T[0] & (uint8_t)0x0F)];
                workingbaudrate /= f_tab[((psam_rest_atr.T[0] >> 4) & (uint8_t)0x0F)];
                printf("workingbaudrate = %d\r\n", workingbaudrate);
                iso7816_init_baud(psam, workingbaudrate);
                if (PTSConfirmStatus == 0x01)
                    return 0;
            }
        }
    }
    return 1;
}
/***************************************************************************************
函数名称: void iso7816_send_ADPU(SC_ADPU_Commands *SC_ADPU, SC_ADPU_Responce *SC_ResponceStatus)
函数功能: 发送ADPU
输入参数:
返 回 值:
***************************************************************************************/
void psam_iso7816_send_ADPU(uint8_t psam, ISO7816_ADPU_Commands *SC_ADPU, ISO7816_ADPU_Responce *SC_ResponceStatus)
{
    uint32_t i = 0;
    uint8_t byte = 0;
    uint8_t err;
    //==================================================================================
    // 清空返回报文缓存
    //==================================================================================
    for (i = 0; i < LCmax; i++)
    {
        SC_ResponceStatus->Data[i] = 0;
    }

    SC_ResponceStatus->SW1 = 0;
    SC_ResponceStatus->SW2 = 0;

    //printf("send===\r\n");
    //USART_DMACmd(uart, USART_DMAReq_Rx, ENABLE);
    //==================================================================================
    // 发送ADPU 报文, 先发送CLA,INS.P1,P2,LC(LE), 等5byte
    //==================================================================================
    psam_err_arq_data = SC_ADPU->Header.CLA;
    iso7816_send_byte(psam, psam_err_arq_data);
    delayus(etu_time);

    if(res_atr[psam].tc_flag) {

        // printf("enter\r\n");
        delayus(etu_time*res_atr[psam].tc[0]);

    }

    //printf("send 1 over===\r\n");

    psam_err_arq_data = SC_ADPU->Header.INS;
    iso7816_send_byte(psam, psam_err_arq_data);
    delayus(etu_time);
    if(res_atr[psam].tc_flag)
        delayus(etu_time*res_atr[psam].tc[0]);

    psam_err_arq_data = SC_ADPU->Header.P1;
    iso7816_send_byte(psam, psam_err_arq_data);
    delayus(etu_time);
    if(res_atr[psam].tc_flag)
        delayus(etu_time*res_atr[psam].tc[0]);

    psam_err_arq_data = SC_ADPU->Header.P2;
    iso7816_send_byte(psam, psam_err_arq_data);
    delayus(etu_time);
    if(res_atr[psam].tc_flag)
        delayus(etu_time*res_atr[psam].tc[0]);

    if (SC_ADPU->Body.LC)
    {
        psam_err_arq_data = SC_ADPU->Body.LC;
        iso7816_send_byte(psam, psam_err_arq_data);
        if(res_atr[psam].tc_flag)
            delayus(etu_time*res_atr[psam].tc[0]);
    }
//    else if(SC_ADPU->Body.LE)
    else
    {
        psam_err_arq_data = SC_ADPU->Body.LE;
        iso7816_send_byte(psam, psam_err_arq_data);
        if(res_atr[psam].tc_flag)
            delayus(etu_time*res_atr[psam].tc[0]);
    }
    // 清空USART1缓存
    //(void)USART_ReceiveData(uart);


    //printf("send over\r\n");
    //==================================================================================
    // 等待卡响应, 3中类型
    // 1 - ACK
    // 2 - NULL 0x60
    // 3 - SW1; SW2
    //==================================================================================
    // 多次等待响应
    for (i = 0; i < 20; i++)
    {
        err = iso7816_rcv_byte(psam, &byte, ISO7816_RCV_TIMEOUT);
        if(!err)
            break;
    }
    //==================================================================================
    // 卡响应成功
    //==================================================================================

    // printf("err = %02X\r\n",err);
    if (!err)
    {
        #if  0  
        // printf("byte = %02X\r\n",byte);
        if (((byte & (uint8_t)0xF0) == 0x60) || ((byte & (uint8_t)0xF0) == 0x90))
        {
            // SW1
            SC_ResponceStatus->SW1 = byte;
            err = iso7816_rcv_byte(psam, &byte, ISO7816_RCV_TIMEOUT);
            if (!err)
            {
                /// SW2
                SC_ResponceStatus->SW2 = byte;
            }

        }
        else if (((byte & (uint8_t)0xFE) == (((uint8_t)~(SC_ADPU->Header.INS)) & (uint8_t)0xFE))
                 ||((byte & (uint8_t)0xFE) == (SC_ADPU->Header.INS & (uint8_t)0xFE)))
        {
            // ACK
            SC_ResponceStatus->Data[0] = byte;
            if(res_atr[psam].tc_flag)
                delayus(etu_time*res_atr[psam].tc[0]);
        }
				#endif
				while(byte==0x60)
				{
					err = iso7816_rcv_byte(psam, &byte, ISO7816_RCV_TIMEOUT);
				}
				if(!err)
				{
					if (((byte & (uint8_t)0xF0) == 0x60) || ((byte & (uint8_t)0xF0) == 0x90))
					{
							// SW1
							SC_ResponceStatus->SW1 = byte;

							err = iso7816_rcv_byte(psam, &byte, ISO7816_RCV_TIMEOUT);
							if (!err)
							{
									/// SW2 
									SC_ResponceStatus->SW2 = byte;
							}

					}
					else if (((byte & (uint8_t)0xFE) == (((uint8_t)~(SC_ADPU->Header.INS)) & (uint8_t)0xFE))
									 ||((byte & (uint8_t)0xFE) == (SC_ADPU->Header.INS & (uint8_t)0xFE)))
					{
							// ACK
							SC_ResponceStatus->Data[0] = byte;
							if(res_atr[psam].tc_flag)
									delayus(etu_time*res_atr[psam].tc[0]);
					}
				}
    }
    //==================================================================================
    // 表示没有接受到状态字节，此时接着发送数据，或接收数据
    //==================================================================================
    if (SC_ResponceStatus->SW1 == 0x00)
    {
        //==================================================================================
        // 发送LC 数据
        //==================================================================================
        if (SC_ADPU->Body.LC)
        {

            //printf("send lc\r\n");
            delayus(etu_time*2);
            for (i = 0; i < SC_ADPU->Body.LC-1; i++)
            {
                psam_err_arq_data = SC_ADPU->Body.Data[i];

                iso7816_send_byte(psam, psam_err_arq_data);
                delayus(etu_time);
                if(res_atr[psam].tc_flag)
                    delayus(etu_time*res_atr[psam].tc[0]);
            }
            psam_err_arq_data = SC_ADPU->Body.Data[i];
            iso7816_send_byte(psam, psam_err_arq_data);
            if(res_atr[psam].tc_flag)
                delayus(etu_time*res_atr[psam].tc[0]);

            // 清空USAT1缓存
            //(void)USART_ReceiveData(uart);
            // USART_DMACmd(uart, USART_DMAReq_Rx, DISABLE);
        }
        //==================================================================================
        // 从卡接收LE长度数据
        //==================================================================================
        else if (SC_ADPU->Body.LE)
        {
            for (i = 0; i < SC_ADPU->Body.LE; i++)
            {
                err = iso7816_rcv_byte(psam, &byte, ISO7816_RCV_TIMEOUT);
                if(!err)
                {
                    SC_ResponceStatus->Data[i] = byte;
                }
            }
        }
        //==================================================================================
        // 等待SW1,SW2响应
        //==================================================================================
        for (i = 0; i < 10; i++)
        {
            err = iso7816_rcv_byte(psam, &byte, ISO7816_RCV_TIMEOUT);
					
					  #if 0
            if (!err)
            {
                SC_ResponceStatus->SW1 = byte;
                break;
            }
					  #endif
					  if (!err)
            {
							
							while(byte == 0x60){
								err = iso7816_rcv_byte(psam, &byte, ISO7816_RCV_TIMEOUT);
							}
							if(!err){
                SC_ResponceStatus->SW1 = byte;
                break;
							}
            }

					
					
        }
        for (i = 0; i < 10; i++)
        {
            err = iso7816_rcv_byte(psam, &byte, ISO7816_RCV_TIMEOUT);
            if (!err)
            {
                SC_ResponceStatus->SW2 = byte;
                break;
            }
        }
        //==================================================================================
        //==================================================================================

    }
}
/***************************************************************************************
函数名称: void iso7816_send_ADPU(SC_ADPU_Commands *SC_ADPU, SC_ADPU_Responce *SC_ResponceStatus)
函数功能: 发送ADPU
输入参数:
返 回 值:
***************************************************************************************/
void iso7816_send_ADPU(uint8_t psam, uint8_t *datain, uint8_t *dataout,unsigned short datain_len,unsigned short *dataout_len)
{
    uint32_t i = 0;
    uint8_t byte = 0;
    uint8_t err;
    uint8_t sw1,sw2;

    ISO7816_ADPU_Commands SC_ADPU;
    ISO7816_ADPU_Responce SC_ResponceStatus;

    if(datain_len<4)
    {
        dataout[0]=0x67;
        dataout[1]=0x00;
        *dataout_len=2;
        return;
    }
    else if(datain_len==4)
    {
        SC_ADPU.Header.CLA=datain[0];
        SC_ADPU.Header.INS=datain[1];
        SC_ADPU.Header.P1=datain[2];
        SC_ADPU.Header.P2=datain[3];
        SC_ADPU.Body.LC=0x00;
        SC_ADPU.Body.LE=0x00;
    }
    else if(datain_len==5)
    {
        SC_ADPU.Header.CLA=datain[0];
        SC_ADPU.Header.INS=datain[1];
        SC_ADPU.Header.P1=datain[2];
        SC_ADPU.Header.P2=datain[3];
        SC_ADPU.Body.LC=0x00;
        SC_ADPU.Body.LE=datain[4];
    }
    else
    {
        SC_ADPU.Header.CLA=datain[0];
        SC_ADPU.Header.INS=datain[1];
        SC_ADPU.Header.P1=datain[2];
        SC_ADPU.Header.P2=datain[3];
        SC_ADPU.Body.LC=datain[4];
        for(i=0; i<SC_ADPU.Body.LC; i++)
        {
            SC_ADPU.Body.Data[i]=datain[5+i];
        }
        if(SC_ADPU.Body.LC+5==datain_len)
            SC_ADPU.Body.LE=0x00;
        else
            SC_ADPU.Body.LE=datain[5+i];

    }

    psam_iso7816_send_ADPU(psam, &SC_ADPU, &SC_ResponceStatus);
    if((SC_ResponceStatus.SW1==0x61)&&(SC_ResponceStatus.SW2))
    {
        SC_ADPU.Header.CLA=0x00;
        SC_ADPU.Header.INS=0xc0;
        SC_ADPU.Header.P1=0x00;
        SC_ADPU.Header.P2=0x00;
        SC_ADPU.Body.LC=0x00;
        SC_ADPU.Body.LE=SC_ResponceStatus.SW2;
        delayus(etu_time*3);
        psam_iso7816_send_ADPU(psam, &SC_ADPU, &SC_ResponceStatus);
    }
    if((SC_ResponceStatus.SW1==0x6C)&&(SC_ResponceStatus.SW2))
    {
        SC_ADPU.Body.LC=0x00;
        SC_ADPU.Body.LE=SC_ResponceStatus.SW2;
        delayus(etu_time*3);
        psam_iso7816_send_ADPU(psam, &SC_ADPU, &SC_ResponceStatus);
    }

    if((SC_ResponceStatus.SW1==0x90)&&(SC_ResponceStatus.SW2==0x00)&&SC_ADPU.Body.LE)
    {
        memcpy(dataout,SC_ResponceStatus.Data,SC_ADPU.Body.LE);
    }
    else
    {
        SC_ADPU.Body.LE=0;
    }
    dataout[SC_ADPU.Body.LE]=SC_ResponceStatus.SW1;
    dataout[SC_ADPU.Body.LE+1]=SC_ResponceStatus.SW2;
    *dataout_len=SC_ADPU.Body.LE+2;

}




/***************************************************************************************
函数名称: void psam_cmd_deal(void)
函数功能: psam卡指令处理
输出参数:
返 回 值:
***************************************************************************************/
void psam_cmd_deal(void)
{
    /*ISO7816_ADPU_Commands SC_ADPU;
    ISO7816_ADPU_Responce SC_Responce;
    unsigned char  pram1 = 0, pram2 = 0;
    unsigned int len;
    unsigned char reset_len;
    unsigned char wr_buf[64];
    queue_out(cmd_queue_buf, &pram1, 1,500);
    queue_out(cmd_queue_buf, &pram2, 1, 500);
    if ((pram1 == 0x01 || pram1 == '1') || (pram1 == 0x02 || pram1 == '2'))
    {
        //==============================================================================
        // 复位PSAM卡, 发送复位数据
        //==============================================================================
        if (pram2 == 0x00)
        {
            reset_len = psam_reset(pram1 % 0x30 - 1);

    //            if (reset_len == 0)
    //                reset_len = psam_reset(pram1 % 0x30 - 1);
            if (reset_len > 0)
                uart_send(psam_atr_tab, reset_len);
            else
                uart_send((uint8_t *)"psam is fail\r\n", strlen((char *)"psam is fail\r\n"));
        }
        //==============================================================================
        // 放ADPU, 复位成功才能发送
        //==============================================================================
        else if (pram2 > 0)
        {

            len = queue_out(cmd_queue_buf, wr_buf, pram2, 500);
            //==============================================================================
            // ADPU报文
            // 传送的ADPU报文必须符合规范
            // 检测参数是否正确
            //==============================================================================
            if (len > 5)
            {
                SC_ADPU.Header.CLA = wr_buf[0];
                SC_ADPU.Header.INS = wr_buf[1];
                SC_ADPU.Header.P1  = wr_buf[2];
                SC_ADPU.Header.P2  = wr_buf[3];
                SC_ADPU.Body.LC    = wr_buf[4];
                if (SC_ADPU.Body.LC && (SC_ADPU.Body.LC < LCmax)
                   && (SC_ADPU.Body.LC == len - 6))
                {
                    memcpy(SC_ADPU.Body.Data, &wr_buf[5], SC_ADPU.Body.LC);
                    SC_ADPU.Body.LE = wr_buf[5 + SC_ADPU.Body.LC];
                }
                else
                {
                    SC_ADPU.Body.LE = wr_buf[5];
                }
            }
            psam_iso7816_send_ADPU(pram1 % 0x30 - 1, &SC_ADPU, &SC_Responce);
            //==============================================================================
            // 获取卡应答数据
            //==============================================================================
            memcpy(wr_buf, SC_Responce.Data, SC_ADPU.Body.LE);
            wr_buf[SC_ADPU.Body.LE + 0] = SC_Responce.SW1;
            wr_buf[SC_ADPU.Body.LE + 1] = SC_Responce.SW2;
            uart_send(wr_buf, SC_ADPU.Body.LE + 2);
        }
    }*/
}
void iso7816_analy_atr(uint8_t *atr_tab, struct emv_atr *atr)
{
    unsigned char i;

    i=0;
    memset(atr,0,sizeof(*atr));
    atr->ts=atr_tab[i++];
    atr->t0=atr_tab[i++];
    if(atr->t0 & 0x10)
    {
        atr->ta_flag|=0x01;
        atr->ta[0]=atr_tab[i++];
    }
    else
    {
        atr->ta_flag&=~0x01;
    }
    if(atr->t0 & 0x20)
    {
        atr->tb_flag|=0x01;
        atr->tb[0]=atr_tab[i++];
    }
    else
    {
        atr->tb_flag&=~0x01;
    }

    if(atr->t0 & 0x40)
    {
        atr->tc_flag|=0x01;
        atr->tc[0]=atr_tab[i++];
    }
    else {
        atr->tc_flag&=~0x01;
    }
    if(atr->t0 & 0x80)
    {
        atr->td_flag|=0x01;
        atr->td[0]=atr_tab[i++];

        if(atr->td[0] & 0x10) {
            atr->ta_flag|=0x02;
            atr->ta[1]=atr_tab[i++];
        }
        else {
            atr->ta_flag&=~0x02;
        }
        if(atr->td[0] & 0x20) {
            atr->tb_flag|=0x02;
            atr->tb[1]=atr_tab[i++];
        }
        else {
            atr->tb_flag&=~0x02;
        }

        if(atr->td[0] & 0x40) {
            atr->tc_flag|=0x02;
            atr->tc[1]=atr_tab[i++];
        }
        else {
            atr->tc_flag&=~0x02;
        }
    }
    else {
        atr->td_flag&=~0x01;
    }
}




/***************************************************************************************
    end of file
***************************************************************************************/
