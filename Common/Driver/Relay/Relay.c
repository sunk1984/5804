/******************************************************************************
    Relay_LPC1114.c
    API functions

    Copyright(C) 2011, Honeywell Integrated Technology (China) Co.,Ltd.
    Security FCT team
    All rights reserved.

    History
    2011.12.16  ver.0.1.00   by Roger
******************************************************************************/
/******************************************************************************
    Note : 继电器板号从1号开始，channel号也 从 1号开始 
******************************************************************************/
#include "includes.h"

//每个板子的最大channel 数
#define CHANNEL_NUMBER_PER_BOARD    24
#define RLY_BOARD_MAX               8
#define CHANNEL_NUMBER_ALL_BOARD    (RLY_BOARD_MAX * CHANNEL_NUMBER_PER_BOARD)

#define RLYFUNC_ON_OFF_CHAN  0x21
#define RLYFUNC_OFF_ALLCHAN  0x22
#define RLYFUNC_SCAN_CHAN    0x23
#define RLYFUNC_SET_MODE     0x24

#define RLYREG_SET_BOARD     0

const U8 RlyOnData[] = "01";
const U8 RlyOffData[] = "00";

const U8 ModeCommonlData[] = "01";
const U8 ModeAdData[] = "00";

static BOOL RLY_Chan_Total2Board(U8 * board_num, U8 * board_chan, U8 TotalChan)
{
	if(TotalChan > CHANNEL_NUMBER_ALL_BOARD)
	{
        return(FALSE);
    }
    if(TotalChan == 0)
    {
        return(FALSE);
	}
	
    * board_num = ((TotalChan-1) / CHANNEL_NUMBER_PER_BOARD) + 1;
    * board_chan = TotalChan % CHANNEL_NUMBER_PER_BOARD;
    
    if(* board_chan == 0)
    {
        * board_chan = CHANNEL_NUMBER_PER_BOARD;
    }
    
    return(TRUE);
}


static BOOL RLY_WriteCmd(U8 board_num, U8 func, U8 reg, U8 * WriteStr)
{
    return(MERAK_WriteCmd("RLY", board_num, func, reg, WriteStr));
}


BOOL RLY_ON(U32 TotalChan)
{
    U8 board_num;
    U8 board_chan;

	if(RLY_Chan_Total2Board(&board_num, &board_chan, TotalChan) == FALSE)
    {
        return(FALSE);
	}
    return(RLY_WriteCmd(board_num, RLYFUNC_ON_OFF_CHAN, board_chan, (U8 * )RlyOnData));
}
/*********************************************************************************
function:    RLY_OFF

description: 此函数用于关闭对应的relay通道

parameters:  Channelnum，通道号       
           
return: TRUE/FALSE
*********************************************************************************/
BOOL RLY_OFF(U32 TotalChan)
{
    U8 board_num;
    U8 board_chan;

	if(RLY_Chan_Total2Board(&board_num, &board_chan, TotalChan) == FALSE)
    {
        return(FALSE);
	}
    return(RLY_WriteCmd(board_num, RLYFUNC_ON_OFF_CHAN, board_chan, (U8 * )RlyOffData));
}

/*********************************************************************************
function:    RLY_OffAll

description: 此函数用于关闭某一个relay的所有通道

parameters:  board_num，板号       
           
return: TRUE/FALSE
*********************************************************************************/
BOOL RLY_OffAll(U8 board_num)
{
    return(RLY_WriteCmd(board_num, RLYFUNC_OFF_ALLCHAN, RLYREG_SET_BOARD, ""));
}

/*********************************************************************************
function:    RLY_Scan

description: 此函数用于扫描某一个relay板的所有通道

parameters:  board_num，板号       
           
return: TRUE/FALSE
*********************************************************************************/
BOOL RLY_Scan(U8 board_num)
{
    return(RLY_WriteCmd(board_num, RLYFUNC_SCAN_CHAN, RLYREG_SET_BOARD, ""));
}

/*********************************************************************************
function:    RLY_SetAdMode

description: 此函数用于设置relay板的模式为AD模式

parameters:  board_num，板号       
           
return: TRUE/FALSE
*********************************************************************************/
BOOL RLY_SetAdMode(U8 board_num)
{
    return(RLY_WriteCmd(board_num, RLYFUNC_SET_MODE, RLYREG_SET_BOARD, (U8 * )ModeAdData));
}

/*********************************************************************************
function:    RLY_SetCommonMode

description: 此函数用于设置relay板的模式为普通模式

parameters:  board_num，板号       
           
return: TRUE/FALSE
*********************************************************************************/
BOOL RLY_SetCommonMode(U8 board_num)
{
    return(RLY_WriteCmd(board_num, RLYFUNC_SET_MODE, RLYREG_SET_BOARD, (U8 * )ModeCommonlData));
}




