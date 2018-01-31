/*******************************************************************************
    Copyright(C) 2012, Honeywell Integrated Technology (China) Co.,Ltd.
    Security FCT team
    All rights reserved.
    File name:  Test_comm_dut.h
    Function: FCT communication test with PC head file
    IDE:    IAR EWARM V6.21 
    ICE:    J-Link 
    BOARD:  Merak board V1.0
    History
    2012.02.28  ver.1.00    First released by Roger                                 
*******************************************************************************/
#ifndef _HONEYWELL_FCT_COMM_485_H_
#define _HONEYWELL_FCT_COMM_485_H_


#include "includes.h"

#pragma pack(1)

typedef struct
{
	U8 prompt[2];
	U8 len[2];
	U8 data[26];

} MERAK_DATA_T, * P_MERAK_DATA_T;

typedef struct
{
    U8 	start[1];      //起始符
    U8 	id[3];      //子板ID
    U8 	num[2];      //子板序号
    U8 	func[2];     //功能码
    U8 	reg[2];     //寄存器
    MERAK_DATA_T data_region;   //数据
    U8 	chk[2];     //校验
    U8 	end[2];     //结束符
  
} MERAK_FRAME;

#pragma pack()


extern void Hex2Str(U8 * str, U8 hex_data);
extern BOOL MERAK_WriteCmd(U8 * board_id, U8 board_num, U8 func, U8 reg, U8 * write_str);
extern BOOL MERAK_ReadCmd(U8 * board_id, U8 board_num, U8 func, U8 reg, U8 * read_str);
extern void MERAK_ResetALL(void);

#endif


