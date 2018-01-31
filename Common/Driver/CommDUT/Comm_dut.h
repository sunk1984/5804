/*******************************************************************************
    Copyright(C) 2012, Honeywell Integrated Technology (China) Co.,Ltd.
    Security FCT team
    All rights reserved.
    File name:  Test_comm_dut.h
    Function: FCT communication test with DUT head file
    IDE:    IAR EWARM V6.21 
    ICE:    J-Link 
    BOARD:  Merak board V1.0
    History
    2012.02.28  ver.1.00    First released by Roger                                 
*******************************************************************************/
#ifndef _HONEYWELL_FCT_COMM_DUT_H_
#define _HONEYWELL_FCT_COMM_DUT_H_

#include "includes.h"

extern U32 Cmd_Proc(P_ITEM_T pitem);
extern U32 Cmd_Aux(P_ITEM_T pitem);
extern U32 Cmd_Ack(U32 usart, U8 * testCmd, U8 * rspPass, U8 * rspFail);
extern U32 Cmd_ReadData(U32 usart,U32 * sq, P_ITEM_T pitem);
extern U32 Cmd_Listen(U32 usart, U8 *rspPass);

#endif
