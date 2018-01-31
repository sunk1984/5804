/*******************************************************************************
    ExtIO_drv_PCA9505.h
    header function file of ExtIO driver (CHIP:PCA9505)

    Copyright(C) 2012, Honeywell Integrated Technology (China) Co.,Ltd.
    Security FCT team
    All rights reserved.

    IDE:    IAR EWARM V6.10.5 6105-2331  
    ICE:    J-Link V8
    BOARD:  Merak EXTIO DRIVER board V1.0

    History
    2012.12.17  ver.1.00    First release by Roger gu   
    2013.2.22   ver.1.10    2nd release by Roger gu  
                delete  orginal table init method 
                add functions EXTIO_ConfigureByteDirction() and EXTIO_ConfigureBankDirction()
    2013.2.27   ver.1.20    3nd release by Roger gu
                revives EXTIO_ReadBank() bug
*******************************************************************************/


/*****************************************************************************
使用说明:

1.slave_add 指 两块ExtIO board的地址，有电阻R22决定是哪一块，也可以共存。

2.pin用16进制表示PCA9505的引脚。
  例如 pin-->0x15 代表的是 IO 1.5，
  pin-->0x23 代表的是IO 2.3.

3.bank 中，0表示bank 0, 1 表示bank 1，以此类推

4.direction有两种表示 IO_OUTPUT，IO_INPUT。

5.value 有两种表示0,1.若不用，请填0。


******************************************************************************/

#ifndef     __EXTIO_485_H__
#define     __EXTIO_485_H__

//定义IO结构中的方向
#define IO_OUTPUT 0
#define IO_INPUT  1

#define IO_CHANNEL_TOTAL  24

//EXTIO board的结构定义
struct IO_INFO
{ 
    INT32U Channel;     //引脚号
    INT32U Direction; //方向
    INT32U Value;     //output方向时的值
};


//配置bit方向
extern BOOL EXTIO_ConfigureBitDirction(U8 TotalChan, U8 BitDirData);

//配置byte的方向
extern BOOL EXTIO_ConfigureByteDirction(U8 TotalChan, U8 ByteDirData);

//读写一个byte的函数
extern BOOL EXTIO_WriteByte(U8 TotalChan, U8 ByteData);
extern BOOL EXTIO_ReadByte(U8 TotalChan, U8 * ReadByte);

//读写某一个bit的函数
extern BOOL EXTIO_WriteBit(U8 TotalChan, U8 BitData);
extern BOOL EXTIO_ReadBit(U8 TotalChan, U8 * ReadBit);

//
extern INT32U EXTIO_Init(struct IO_INFO io_info[]);

#endif



