/*******************************************************************************
    Copyright(C) 2012, Honeywell Integrated Technology (China) Co.,Ltd.
    Security FCT team
    All rights reserved.
    File name: Comm_485.c
    Function: FCT  test function with dut
    IDE:    IAR EWARM V6.21 
    ICE:    J-Link 
    BOARD:  Merak board V1.0
    History
    2012.02.28  ver.1.00                                   
*******************************************************************************/

#include <ctype.h>

#include "includes.h"

#define MERAK_FRAME_HEAD_LEN (14)   //from start to data.len

#define MERAK_FRAME_MIN (18)
#define MERAK_FRAME_MAX (48)            //收到子板数据的最大长度


const U8 WriteStr_EmptData[] = "";

/**************************************************************************** 
函数名称: BcdStr2Hex 
函数功能: BCD字符串 转十六进制
输入参数: str 字符串 hex_buf 十六进制 len 十六进制字符串的长度。 
输出参数: 无 
*****************************************************************************/   
static U8 BcdStr2Hex(U8 * str)
{
    return((str[0] - '0') * 10 + str[1] - '0');
} 


/**************************************************************************** 
函数名称: Bcd2Str 
函数功能: 十六进制BCD码转字符串 
输入参数: str 字符串 bcd_data 十六进制 BCD码
输出参数: 无 
*****************************************************************************/   
static void Bcd2Str(U8 * str, U8 bcd_data)
{
    str[0] = (bcd_data/10) + '0';
    str[1] = (bcd_data%10) + '0';
} 

/**************************************************************************** 
函数名称: Hex2Str 
函数功能: 十六进制转字符串 
输入参数: str 字符串 hex_data 十六进制 
输出参数: 无 
*****************************************************************************/   
void Hex2Str(U8 * str, U8 hex_data)
{
    sprintf((char * )str, "%02x", hex_data);
    str[0] = toupper(str[0]);
    str[1] = toupper(str[1]);
} 

/*********************************************************************************
function:    MERAK_SetBuff

description: 此函数用于赋值

parameters:  U8 * buff, U8 * str       
           
return: void
*********************************************************************************/
void MERAK_SetBuff(U8 * buff, U8 * str)
{
	strncpy((char * )buff, (char * )str, OS_STRLEN((char * )str));
}


/*********************************************************************************
function:    MERAK_GetChk

description: 此函数用于计算并添加帧的CHK部分。

parameters:  p_tx_frame， 发送的字符串结构             
           
return: U8
*********************************************************************************/
static U8 MERAK_GetChk(MERAK_FRAME * p_frame)
{
    U8 i;
    U8 * p;
    U8 chk_len;
    U8 datalen_str[3] = {0,0,0};
    U8 chk_hex = 0;
    
	OS_MEMCPY((char * )datalen_str, (char *)p_frame->data_region.len, 2);
    chk_len = BcdStr2Hex(datalen_str) + MERAK_FRAME_HEAD_LEN-1;

    p = p_frame->id;
    for(i = 0; i < chk_len; i++)
    {
        chk_hex = (U8)(chk_hex + * (p+i));
    }

    return(chk_hex);
}

/*********************************************************************************
function:    MERAK_VerifyChk

description: 此函数用于计算对比CHK。

parameters:  p_tx_frame， 发送的字符串结构             
           
return: BOOL
*********************************************************************************/
static BOOL MERAK_VerifyChk(MERAK_FRAME * p_rx_frame)
{
    U8 chk_calc;
    U8 chk_recv;

    chk_calc = MERAK_GetChk(p_rx_frame);
    p_rx_frame->end[0] = 0;
    chk_recv = (U8)strtol((char *)p_rx_frame->chk, NULL, 16);
    
    if(chk_calc == chk_recv)
    {
		return(TRUE);
    }
    else
    {
	    return(FALSE);
    }
}

/*********************************************************************************
function:    MERAK_GetFrame

description: 此函数用于接收subboard发过来的字符串并进行分析处理，取出12字节

parameters:  rx_data， 接收的数据部分

return:      PASS or FAIL
*********************************************************************************/
static BOOL MERAK_GetFrame(MERAK_FRAME * p_rx_frame)
{
    U8 i;
    U8 rcnt;
    U8 frame_len;
    U8 rbuf[MERAK_FRAME_MAX];
    static U8 rlen = 0;
    static U8 tempbuf[MERAK_FRAME_MAX];

    if(UsartGetFrame_by_2BytesEnd(MERAK_COMM_PORT, 0x0d, 0x0a, rbuf, sizeof(rbuf),(INT32U * )&rcnt))	//Received a frame by "0d0a".
    {
        return(FALSE);
    }
    
	OS_MEMCPY(&tempbuf[rlen], rbuf, rcnt);		//Copy to temp.

	rlen += rcnt;								//Update the buffer size.

	if(rlen > MERAK_FRAME_MAX)				//Buffer received error, it has been too long.
	{
		rlen = 0;
		return(FALSE);
	}

	if(rlen < MERAK_FRAME_MIN)				//A part of frame?
	{
		return(FALSE);
	}
	
	for(i = 0; i < rlen; i++)
	{
        if(tempbuf[i] == '^')
        {
            frame_len = rlen - i;
            break;
        }
	}
	if(i == rlen)
	{
		return(FALSE);
	}
	
	rlen = 0;
	
	OS_MEMCPY(p_rx_frame, &tempbuf[i], frame_len-4);
	OS_MEMCPY(p_rx_frame->chk, &tempbuf[i+frame_len-4], 2);
	
    return(TRUE);
}

   
/*********************************************************************************
function:    MERAK_GetAck

description: 此函数用于接收子板发过来的字符串并进行分析处理，取出数据部分

parameters:  p_tx_frame， 接收的数据部分

return:      TRUE/FALSE
*********************************************************************************/
static BOOL MERAK_GetAck(MERAK_FRAME * p_tx_frame)
{
    U32 data_len;
    
    MERAK_FRAME MERAK_RxFrame;
    MERAK_FRAME * p_rx_frame;

    p_rx_frame = &MERAK_RxFrame;

    if(MERAK_GetFrame(p_rx_frame) == FALSE)
    {
        return(FALSE);
    }
    
    if(MERAK_VerifyChk(p_rx_frame) == FALSE)
    {
        return(FALSE);                        // zjm
    }
    
    if(strncmp((char * )p_rx_frame->id, (char * )p_tx_frame->id, 3) != 0)
    {
        return(FALSE);
    }

    if(strncmp((char * )p_rx_frame->func, (char * )p_tx_frame->func, 2) == 0)  //收发功能码一样的情况
    {
        data_len = BcdStr2Hex(p_rx_frame->data_region.len);
        data_len += 2;
    	OS_MEMCPY((char * )p_tx_frame->data_region.len, (char * )p_rx_frame->data_region.len, data_len);
        return(TRUE);
    }
    
    if(p_rx_frame->func[0] == p_tx_frame->func[0] && p_rx_frame->func[1] == '0')//收的应答 是 x0的情况
    {
        data_len = BcdStr2Hex(p_rx_frame->data_region.len);
        data_len += 2;
    	OS_MEMCPY((char * )p_tx_frame->data_region.len, (char * )p_rx_frame->data_region.len, data_len);
        return(TRUE);
    }

    return(FALSE);
}


/*********************************************************************************
function:    MERAK_SendFrame

description: 此函数用于向子板发送帧。包括增加CHK，剔除帧的多余字节

parameters:  p_tx_frame， 发送的字符串结构             
           
return: void
*********************************************************************************/
static void MERAK_SendFrame(MERAK_FRAME * p_tx_frame)
{
    U8 chk;
    U8 len_head2data;
    U8 datalen_str[3] = {0};
    U8 tx_frame[MERAK_FRAME_MAX];

    chk = MERAK_GetChk(p_tx_frame);
    Hex2Str(p_tx_frame->chk, chk);
    
	OS_MEMCPY((char * )datalen_str, (char *)p_tx_frame->data_region.len, 2);
    len_head2data = BcdStr2Hex(datalen_str) + MERAK_FRAME_HEAD_LEN;
    
	OS_MEMCPY(tx_frame, p_tx_frame, len_head2data);        // Copy the strings to tx buffer from start to data 
	OS_MEMCPY(tx_frame+len_head2data, p_tx_frame->chk, 2);
	OS_MEMCPY(tx_frame+len_head2data+2, "\r\n", 2);        // Copy chk string and end string.

	UsartPutFrame(MERAK_COMM_PORT, tx_frame, len_head2data+4);
}


static void MERAK_BuildFrame(MERAK_FRAME * p_tx_frame, U8 * board_id, U8 board_num, U8 func, U8 reg, U8 * data_str)
{
    U8 boardnum_str[4];
    U8 func_str[3];
    U8 reg_str[3];
    U8 datalen_str[3];

    Hex2Str(boardnum_str, board_num);
    Hex2Str(func_str, func);
    Bcd2Str(reg_str, reg);
    Bcd2Str(datalen_str, OS_STRLEN((char * )data_str));

    MERAK_SetBuff(p_tx_frame->start, "~");
    MERAK_SetBuff(p_tx_frame->id, board_id);
    MERAK_SetBuff(p_tx_frame->num, boardnum_str);
    MERAK_SetBuff(p_tx_frame->func, func_str);
    MERAK_SetBuff(p_tx_frame->reg, reg_str);
    MERAK_SetBuff(p_tx_frame->data_region.prompt, "**");
    MERAK_SetBuff(p_tx_frame->data_region.len, datalen_str);
    MERAK_SetBuff(p_tx_frame->data_region.data, data_str);
}


/*********************************************************************************                        
function: MERAK_CMD

description: MERAK通信的分析执行函数

parameters: void

return: void
*********************************************************************************/
static BOOL MERAK_CMD(U8 * board_id, U8 board_num, U8 func, U8 reg, U8 * tx_data, U8 * rx_data)
{
    U16 i;
    U32 data_len;
    MERAK_FRAME MERAK_TxFrame;
    MERAK_FRAME * p_tx_frame;
    
    p_tx_frame = &MERAK_TxFrame;
    
    UsartRecvReset(MERAK_COMM_PORT);

    MERAK_BuildFrame(p_tx_frame, board_id, board_num, func, reg, tx_data);
    MERAK_SendFrame(p_tx_frame);
    
    for(i = 0; i < 500; i++)
    {
        if(MERAK_GetAck(p_tx_frame))
        {
            break;
        }
        OS_Delay(1);
    }

    if(i < 500)
    {
        data_len = BcdStr2Hex(p_tx_frame->data_region.len);
    	OS_MEMCPY((char * )rx_data, (char * )p_tx_frame->data_region.data, data_len);
        return(TRUE);
    }

    return(FALSE);
}

/*********************************************************************************                        
function: MERAK_WriteCmd

description: MERAK通信的写命令

parameters: void

return: void
*********************************************************************************/
BOOL MERAK_WriteCmd(U8 * board_id, U8 board_num, U8 func, U8 reg, U8 * write_str)
{
    U8 read_data[10] ={0};
    
    if(MERAK_CMD(board_id, board_num, func, reg, write_str, read_data) == FALSE)
    {
        if(MERAK_CMD(board_id, board_num, func, reg, write_str, read_data) == FALSE)
        {
            return(FALSE);
        }
    }
    
    if(strncmp((char * )read_data , "OK", 2) == 0)
    {
        return(TRUE);
    }
    if(strncmp((char * )read_data , "PASS", 4) == 0)
    {
        return(TRUE);
    }
    
    return(FALSE);
}

/*********************************************************************************                        
function: MERAK_ReadCmd

description: MERAK通信的读命令

parameters: BOOL

return: TRUE/FALSE
*********************************************************************************/
BOOL MERAK_ReadCmd(U8 * board_id, U8 board_num, U8 func, U8 reg, U8 * read_str)
{
    if(MERAK_CMD(board_id, board_num, func, reg, (U8 * )WriteStr_EmptData, read_str) == TRUE)
    {
        return(TRUE);
    }
    if(MERAK_CMD(board_id, board_num, func, reg, (U8 * )WriteStr_EmptData, read_str) == TRUE)
    {
        return(TRUE);
    }
    return(FALSE);
}

void RS485_Test();

/*********************************************************************************
function:    MERAK_ResetALL

description: 此函数用于复位所有子板

parameters:  void       
           
return: PASS or FAIL
*********************************************************************************/
void MERAK_ResetALL(void)
{
    MERAK_SendFrame((MERAK_FRAME * )"~ALL000100**00xx\r\n");
    OS_Delay(100);
//RS485_Test();
}

void RS485_Test()
{
    INT32U DutCur;
    INT8U Port;
    
   // MERAK_ResetALL();
/*   
    PWR_TurnOffDut();
    PWR_SetDutVolt(8000);
    PWR_TurnOnDut();
*/
/*       
    PWR_TurnOffDut();
    PWR_SetDutVolt(7000);
    PWR_TurnOnDut();
    
    PWR_TurnOffDut();
//    while(1)
    {
        PWR_TurnOnDut();
        PWR_AddDutVolt(1);
        OS_Delay(100);
        PWR_SubDutVolt(1);
        OS_Delay(100);
    }

    PWR_TurnOffDut();
    DutCur=0;
    PWR_GetDUTCur((U32 * )&DutCur);
    PWR_TurnOffDut();
    PWR_TurnOnAux();
    PWR_TurnOffAux();
    */   
#if 0

    while(1)
    {
        for(int i=1;i<=24;i++)
        {
            EXTIO_ConfigureBitDirction(i,0);
           // EXTIO_ReadBit(i,&Port);
            OS_Delay(2);
            EXTIO_WriteBit(i,0);
            OS_Delay(2);
            EXTIO_WriteBit(i,1);
            OS_Delay(2);
        }
        
        for(int i=0;i<3;i++)
        {
            EXTIO_ConfigureByteDirction(1+ i*8,0);         
            EXTIO_WriteByte(1+ i*8,0);
            OS_Delay(2);
            EXTIO_WriteByte(1+ i*8,0xff);
            OS_Delay(2);
        }
       /* EXTIO_ConfigureByteDirction(1,0);
        EXTIO_ConfigureByteDirction(9,0);
        EXTIO_ConfigureByteDirction(17,0);
        EXTIO_WriteByte(1,0);
        EXTIO_WriteByte(9,0);
        EXTIO_WriteByte(17,0);
        EXTIO_ConfigureByteDirction(1,1);
        EXTIO_ConfigureByteDirction(9,1);
        EXTIO_ConfigureByteDirction(17,1); 
        EXTIO_ReadByte(1,&Port);
        EXTIO_ReadByte(9,&Port);
        EXTIO_ReadByte(17,&Port);
        */
    }
#endif    

   /* HMI_PassBuzz();
    HMI_ShortWarnBuzz();
    HMI_LongWarnBuzz();
    HMI_LongWarnBuzzOff();
      
    HMI_FailBuzz();
    
    HMI_OnRunLed();
    HMI_FlashRunLed();
    HMI_OffRunLed();
    HMI_OnRunLed();
    HMI_OnFailLed();
    HMI_OffFailLed();
    HMI_OnPassLed();
    HMI_OffPassLed();
    HMI_PassBuzz();
//    HMI_WarnBuzz();
    HMI_FailBuzz();
    HMI_PressFuncKey();
    HMI_PressYesKey();
    HMI_PressNoKey();  
*/
/*
    RLY_SetAdMode(1);
    RLY_SetAdMode(2);
    while(1)
    {
      for(int i=1;i<=24;i++)
      {
            RLY_ON(i);
            OS_Delay(10);
            RLY_OFF(i);
            OS_Delay(10);
      }
      for(int i=25;i<=48;i++)
      {
            RLY_ON(i);
            OS_Delay(10);
            RLY_OFF(i);
            OS_Delay(10);
      }
      RLY_OffAll(1);
      RLY_OffAll(2);
    }
*/
 
    LCD_DisplayALine(1,"Hello FCT");
    LCD_DisplayALine(2,"Hello FCT");
    LCD_DisplayALine(3,"Hello FCT");
    LCD_DisplayALine(4,"Hello FCT");
    LCD_Clear(1);
    LCD_Clear(2);
    LCD_Clear(3);
    LCD_Clear(4);

}



