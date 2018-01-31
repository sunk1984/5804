/*******************************************************************************
    Copyright(C) 2012, Honeywell Integrated Technology (China) Co.,Ltd.
    Security FCT team
    All rights reserved.
    File name:  Test_comm_dut.c
    Function: FCT  test function with DUT
    IDE:    IAR EWARM V6.21 
    ICE:    J-Link 
    BOARD:  Merak board V1.0
    History
    2012.02.28  ver.1.00    First released by Roger                                 
*******************************************************************************/
//#define LYNX_AP_MAIN

#include "includes.h"

#define DEFAULT_TIMEOUT_MS          (1000)
#define DEFAULT_CMD_REPEAD_TIMES    (10)
#define RECEIVE_BUFF_SIZE           (250) 
U32 Cmd_ListenSn(U32 usart, U8 *rspPass)
{
    U32 recvbyte;
    U32 recvflag = FALSE;
    U8 recvbuf[RECEIVE_BUFF_SIZE];
    U32 i; 

    for(i = 0; i < 1000; i++)
    {
        if(UsartGetFrame_by_2BytesEnd(usart, '\r', '\n', recvbuf, sizeof(recvbuf),(INT32U *)&recvbyte) == 0)
        {
            recvbuf[recvbyte - 2] = 0;   //Remove "\r\n"
            if(strncmp((char * )recvbuf, (char *)rspPass, strlen(rspPass)) == 0)
            {
                recvflag = TRUE;
                break;
            }
        }
        OS_Delay(3);
    }
    return(recvflag);
}

U32 Cmd_Listen(U32 usart, U8 *rspPass)
{
    U32 recvbyte;
    U32 recvflag = FALSE;
    U8 recvbuf[RECEIVE_BUFF_SIZE];
    U8 i; 

    for(i = 0; i < 100; i++)
    {
        if(UsartGetFrame_by_2BytesEnd(usart, '\r', '\n', recvbuf, sizeof(recvbuf),(INT32U *)&recvbyte) == 0)
        {
            recvbuf[recvbyte - 2] = 0;   //Remove "\r\n"
            if(strcmp((char * )recvbuf, (char * )rspPass) == 0)
            {
                recvflag = TRUE;
                break;
            }
        }
        OS_Delay(3);
    }
    return(recvflag);
}
/******************************************************************************
*   Routine Name    : Cmd_Ack
*   Parameters      : usart:串口号 testCmd:需要发送的命令 rspPass:Pass标志  rspFail:fial标志
*   Return value    : PASS或者Fail
*   Description     : 命令应答 先发送命令,再根据参数判断返回值是Pass还是Fail
*                     如果返回值不是rspPass和rspFail,这个函数要等待10S,做的不是很好
******************************************************************************/
U32 Cmd_Ack(U32 usart, U8 *testCmd, U8 *rspPass, U8 * rspFail)
{
    U32 i; 
    U32 recvbyte;        //接收的字节数
    U32 recvflag = FALSE;//返回标志
    U8 txCmd[40];        //发送buffer
    U8 recvbuf[RECEIVE_BUFF_SIZE];     //接收buffer 

    if( *testCmd )//如果有命令,发送命令
    {
        UsartRecvReset(usart); //复位串口
        sprintf((char *)txCmd, "%s\r\n", (char * )testCmd);
        UsartPutStr(usart, txCmd); //发送命令
    }
	
    if( *rspPass == 0 )
    {
        return(TRUE);
    }

    for(i = 0; i < DEFAULT_TIMEOUT_MS; i++)
    {
        if(UsartGetFrame_by_2BytesEnd(usart, '\r', '\n', recvbuf, sizeof(recvbuf),(INT32U *)&recvbyte) == 0)//得到返回数据
        {
            recvbuf[recvbyte - 2] = 0;   //Remove "\r\n"
            if(strcmp((char * )recvbuf, (char * )rspPass) == 0)//判断Pass标志
            {
                recvflag = TRUE; 
                break;
            }
            if(* rspFail)
            {
                if(strcmp((char * )recvbuf, (char * )rspFail) == 0)//判断Fail标志
                {
                    break;
                }
            }
        }
        OS_Delay(1);
    }

    return(recvflag);
}
/******************************************************************************
*   Routine Name    : Cmd_ReadData
*   Parameters      : usart:串口号 sq:返回的数据 pitem
*   Return value    : PASS或者Fail
*   Description     : 读取数据,只能返回数字,不可返回字符串
******************************************************************************/
U32 Cmd_ReadData(U32 usart,U32 *sq, P_ITEM_T pitem)
{
    U8 recvbyte;
    U8 recvbuf[30];
    U8 txCmd[30];
    U8 i;
    U8 len;
    int data;
    len = strlen((char * )pitem->RspCmdPass);
    memset(recvbuf, 0 ,30);
    UsartRecvReset(usart); //复位串口
    sprintf((char *)txCmd, "%s\r\n", (char * )pitem->TestCmd);
    UsartPutStr(usart, txCmd);//发送命令
    
    for(i=0;i<100;i++)
    {
        if(UsartGetFrame_by_2BytesEnd(usart, '\r','\n', recvbuf, sizeof(recvbuf),(INT32U * )&recvbyte) == 0)//得到返回数据
        {
		    if(strncmp((char * )pitem->RspCmdPass, (char * )recvbuf, len) == 0)//判断返回数据的头
		    {
                //* sq = (U8)strtod((char *)(recvbuf+len), NULL); //取数据
                data = strtod((char *)(recvbuf+len), NULL); //取数据
                *sq = abs(data);
                break;
            }
        }
        OS_Delay(10);
    }
    if(i < 100)
    {
        return(TRUE);
    }
    else
    {
        return(FALSE);
    }
}
#ifdef LYNX_AP_MAIN

U32 comm = DUT_COMM_PORT;

void Get_LynxComm(U8 * item)
{
    if(item[0] == '1')
    {
        comm = AUX_COMM_PORT;
    }
    else if(item[0] == '0')
    {
        comm = DUT_COMM_PORT;
    }
}
#endif
/******************************************************************************
    Routine Name    : Cmd_Proc
    Parameters      : pitem
    Return value    : none
    Description     :  命令处理
******************************************************************************/
U32 Cmd_Proc(P_ITEM_T pitem)
{
    U8 i;
    U8 Cmd_CntMax = DEFAULT_CMD_REPEAD_TIMES;
    
    if(strcmp((char * )pitem->id, "CMD") == 0)  //For "CMD" command, Param indicate maximum repeat time
    {
        if(pitem->Param)
        {
            Cmd_CntMax = pitem->Param;
        }
    }
    
	for(i=0; i<Cmd_CntMax; i++)
	{
#ifdef LYNX_AP_MAIN
	    Get_LynxComm(pitem->item);
    	if(Cmd_Ack(comm, pitem->TestCmd, pitem->RspCmdPass, pitem->RspCmdFail))
#else
    	if(Cmd_Ack(DUT_COMM_PORT, pitem->TestCmd, pitem->RspCmdPass, pitem->RspCmdFail))
#endif
    	{
    	    break;
    	}
	}
	
    if(i < Cmd_CntMax)
    {
        if(i)
        {
            Dprintf("Cmd repeat times is %d\r\n", i);
        }
        return(TRUE);
    }
    else
    {
        return(FALSE);
    }
}

U32 Cmd_Aux(P_ITEM_T pitem)
{
	/*if(Cmd_Ack(pitem->Channel, pitem->TestCmd, pitem->RspCmdPass, pitem->RspCmdFail))
	{
        return(TRUE);
	}
	
    Dprintf("Repeat the CMD!\r\n");

	if(Cmd_Ack(pitem->Channel, pitem->TestCmd, pitem->RspCmdPass, pitem->RspCmdFail))
	{
        return(TRUE);
	}*/
    U32 i;
    for(i=0; i<10; i++)
    {
        if(Cmd_Ack(pitem->Channel, pitem->TestCmd, pitem->RspCmdPass, pitem->RspCmdFail))
        {
            return(TRUE);
        }
        Dprintf("Repeat the CMD!\r\n");
    }
    
    return(FALSE);
}


