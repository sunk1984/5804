/*******************************************************************************
    ExtIO_drv_PCA9505.c
    function file of ExtIO driver (CHIP:PCA9505)

    Copyright(C) 2012, Honeywell Integrated Technology (China) Co.,Ltd.
    Security FCT team
    All rights reserved.

    IDE:    IAR EWARM V6.10.5 6105-2331  
    ICE:    J-Link V8
    BOARD:  Merak EXTIO DRIVER board V1.2

*******************************************************************************/
#include "includes.h"

#define BOARD_NUMBER_MAX        4
#define CHAN_NUMBER_PER_BOARD   24
#define CHAN_NUMBER_ALL_BOARD   (CHAN_NUMBER_PER_BOARD*BOARD_NUMBER_MAX)

#define IOMFUNC_WRITE_BIT       0x31        
#define IOMFUNC_WRITE_BYTE      0x32        
#define IOMFUNC_DIR_BIT         0x33        
#define IOMFUNC_DIR_BYTE        0x34        
#define IOMFUNC_READ_BIT        0x35        
#define IOMFUNC_READ_BYTE       0x36        


static BOOL EXTIO_Chan_Total2Board(U8 * board_num, U8 * board_chan, U8 TotalChan)
{
	if(TotalChan > CHAN_NUMBER_ALL_BOARD)
	{
        return(FALSE);
    }
    if(TotalChan == 0)
    {
        return(FALSE);
	}
	
    * board_num = ((TotalChan-1) / CHAN_NUMBER_PER_BOARD) + 1;
    * board_chan = TotalChan % CHAN_NUMBER_PER_BOARD;
    
    if(* board_chan == 0)
    {
        * board_chan = CHAN_NUMBER_PER_BOARD;
    }
    
    return(TRUE);
}

INT32U EXTIO_Init(struct IO_INFO io_info[])
{
    INT32U status = 0;    
   
    //引脚方向初始化
    INT32U item = 0;
    for(item = 1; item <= IO_CHANNEL_TOTAL; item++)
    {        
        status = EXTIO_ConfigureBitDirction(io_info[item].Channel, io_info[item].Direction);
        if(status != TRUE)
            return FALSE;
    }
    
    //值初始化
    for(item = 1; item <= IO_CHANNEL_TOTAL; item++)
    {                
        status = EXTIO_WriteBit(io_info[item].Channel, io_info[item].Value);
        if(status != TRUE)
            return FALSE;        
    }    
    return TRUE;
}

static BOOL EXTIO_WriteCmd(U8 func, U8 TotalChan, U8 * WriteStr)
{
    U8 board_num;
    U8 board_chan;

	if(EXTIO_Chan_Total2Board(&board_num, &board_chan, TotalChan) == FALSE)
    {
        return(FALSE);
	}
    return(MERAK_WriteCmd("IOM", board_num, func, board_chan, WriteStr));
}


static BOOL EXTIO_ReadCmd(U8 func, U8 TotalChan, U8 * ReadStr)
{
    U8 board_num;
    U8 board_chan;

	if(EXTIO_Chan_Total2Board(&board_num, &board_chan, TotalChan) == FALSE)
    {
        return(FALSE);
	}
    return(MERAK_ReadCmd("IOM", board_num, func, board_chan, ReadStr));
}


BOOL EXTIO_ConfigureBitDirction(U8 TotalChan, U8 BitDirData)
{
    U8 BitDirStr[3] = {0};

	Hex2Str(BitDirStr, BitDirData);
	
    return(EXTIO_WriteCmd(IOMFUNC_DIR_BIT, TotalChan, BitDirStr));
}


BOOL EXTIO_ConfigureByteDirction(U8 TotalChan, U8 ByteDirData)
{
    U8 ByteDirStr[3] = {0};

	Hex2Str(ByteDirStr, ByteDirData);
	
    return(EXTIO_WriteCmd(IOMFUNC_DIR_BYTE, TotalChan, ByteDirStr));
}


BOOL EXTIO_WriteBit(U8 TotalChan, U8 BitData)
{    
    U8 BitDateStr[3] = {0};

	Hex2Str(BitDateStr, BitData);
	
    return(EXTIO_WriteCmd(IOMFUNC_WRITE_BIT, TotalChan, BitDateStr));
}


BOOL EXTIO_WriteByte(U8 TotalChan, U8 ByteData)
{
    U8 ByteDateStr[3] = {0};

	Hex2Str(ByteDateStr, ByteData);
	
    return(EXTIO_WriteCmd(IOMFUNC_WRITE_BYTE, TotalChan, ByteDateStr));
}


BOOL EXTIO_ReadBit(U8 TotalChan, U8 * ReadBit)
{
    U8 BitDataStr[3] = {0};

    if(EXTIO_ReadCmd(IOMFUNC_READ_BIT, TotalChan, BitDataStr) == FALSE)
    {
        return(FALSE);
    }
    
    * ReadBit = (U8)strtol((char *)BitDataStr, NULL, 10);

    return(TRUE);
}


BOOL EXTIO_ReadByte(U8 TotalChan, U8 * ReadByte)
{
    U8 ReadByteStr[3] = {0};

    if(EXTIO_ReadCmd(IOMFUNC_READ_BYTE, TotalChan, ReadByteStr) == FALSE)
    {
        return(FALSE);
    }
    
    * ReadByte = (U8)strtol((char *)ReadByteStr, NULL, 16);

    return(TRUE);
}

//    以下是测试ExtIO 的函数
void ExIO_Test(void)
{
    U8 readData = 0;
    
    //bit 操作
    EXTIO_ConfigureBitDirction(11, IO_OUTPUT);
    
    EXTIO_WriteBit(11, 0);
    EXTIO_ReadBit(11, &readData);    
    
    EXTIO_WriteBit(11, 1);
    EXTIO_ReadBit(11, &readData);    

    //byte 操作
    EXTIO_ConfigureByteDirction(17, 0x00); 
    
    EXTIO_WriteByte(17, 0x55);
    EXTIO_ReadByte(17, &readData);
    
    while(1);    
}



