/*********************************************************************
 File    : PWR.c 
Communication with Merak Power board throgh RS485 interface

    Copyright(C) 2014, Honeywell Integrated Technology (China) Co.,Ltd.
    Security FCT team
    All rights reserved.

    IDE:    IAR EWARM V6.4
    ICE:    J-Link
    BOARD:  Merak Main board(2014,5,22 rs485 interface)

    History
    2014.8.27  ver.1.00    ZJM first release  

Purpose : PWR for AT91SAM9260-EK and AT91SAM9XE-EK
--------  END-OF-HEADER  ---------------------------------------------
*/
#include "includes.h"

#define PWRFUNC_SET_VOLT	    0x11//设置可调节电源电压命令
#define PWRFUNC_ON_OFF_DUT 	    0x12//电源的输出开关命令
#define PWRFUNC_ON_OFF_AUX 	    0x13//后备电源开关命令
#define PWRFUNC_RD_CURRENT 	    0x14//读电流命令
#define PWRFUNC_VOL_ADD 	    0x15//可调节电源电压增加命令
#define PWRFUNC_VOL_SUB 	    0x16//可调节电源电压下降命令

#define PWRCHAN_DUT	            0x01//DUT电源通道
#define PWRCHAN_MEARK	        0x02//meark电源通道
#define PWRCHAN_AUX 	        0x01

#define PWRNUM_ONLY_1       1
#define MAX_VOL_DUT        24000    //DUT最大电压值
const U8 TurnOnData[]  = "01";
const U8 TurnOffData[] = "00";

/**************************************************************************** 
函数名称: Hex2Str32 
函数功能: 十六进制转字符串 
输入参数: str 字符串 hex_data 十六进制 
输出参数: 无 
*****************************************************************************/   
static void Hex2Str32(U8 * str, U32 hex_data)
{
    sprintf((char * )str, "%d", hex_data);
} 
/******************************************************************************
函数名称  : PWR_WriteCmd
输入参数  : func 功能码 reg寄存器 data_str数据
返回值    : TRUE/FALSE
功能描述  : 发送命令
******************************************************************************/
static BOOL PWR_WriteCmd(U8 func, U8 reg, U8 * data_str)
{
    return(MERAK_WriteCmd("PWR", PWRNUM_ONLY_1, func, reg, data_str));
}
/******************************************************************************
函数名称  : PWR_ReadCmd
输入参数  : func 功能码 reg寄存器 ReadStr
返回值    : TRUE/FALSE
功能描述  : 读命令
******************************************************************************/
static BOOL PWR_ReadCmd(U8 func, U8 reg, U8 * ReadStr)
{
    return(MERAK_ReadCmd("PWR", PWRNUM_ONLY_1, func, reg, ReadStr));
}
/******************************************************************************
函数名    : PWR_TurnOnDut
参数      : void
返回值    : TRUE/FALSE
功能描述  : 使能输出DUT电源
******************************************************************************/
BOOL PWR_TurnOnDut(void)
{
    return(PWR_WriteCmd(PWRFUNC_ON_OFF_DUT, PWRCHAN_DUT, (U8 * )TurnOnData));
}
/******************************************************************************
函数名    : PWR_TurnOffDut
参数      : void
返回值    : TRUE/FALSE
功能描述  : 关闭输出DUT电源
******************************************************************************/
BOOL PWR_TurnOffDut(void)
{
    return(PWR_WriteCmd(PWRFUNC_ON_OFF_DUT, PWRCHAN_DUT, (U8 * )TurnOffData));
}
/******************************************************************************
函数名    : PWR_TurnOnAux
参数      : void
返回值    : TRUE/FALSE
功能描述  : 使能备份电源
******************************************************************************/
BOOL PWR_TurnOnAux(void)
{
    return(PWR_WriteCmd(PWRFUNC_ON_OFF_AUX, PWRCHAN_AUX, (U8 * )TurnOnData));
}
/******************************************************************************
函数名    : PWR_TurnOnAux
参数      : void
返回值    : TRUE/FALSE
功能描述  : 关闭备份电源
******************************************************************************/
BOOL PWR_TurnOffAux(void)
{
    return(PWR_WriteCmd(PWRFUNC_ON_OFF_AUX, PWRCHAN_AUX, (U8 * )TurnOffData));
}
/******************************************************************************
函数名    : PWR_SetDutVolt
参数      : volt 设置的电压值 单位mv 例如要设置12v volt=12000 
返回值    : TRUE/FALSE
功能描述  :设置DUT板电压值 最大只能输出24V
******************************************************************************/
BOOL PWR_SetDutVolt(U32 volt)
{
    U8 VoltStr[6];
    //如果设置电压超出允许最大值,按最大值设置
    if(volt > MAX_VOL_DUT) 
    {
        volt = MAX_VOL_DUT;
    }
    Hex2Str32(VoltStr, volt);//数字转换成ASCII
 
    return(PWR_WriteCmd(PWRFUNC_SET_VOLT, PWRCHAN_DUT, (U8 * )VoltStr));//发送设置电压命令
}

BOOL PWR_SetAuxVolt(U32 volt)
{
    U8 VoltStr[6];

    Hex2Str32(VoltStr, volt);
    
    return(PWR_WriteCmd(PWRFUNC_SET_VOLT, PWRCHAN_AUX, (U8 * )VoltStr));
}
/******************************************************************************
函数名    : PWR_RdDUTCurrent
参数      : pCur 
返回值    : TRUE/FALSE
功能描述  : 读取DUT负载电流 单位ma
******************************************************************************/
BOOL PWR_GetDUTCur(U32 *pCur)
{
    U8 VoltStr[6];
    if(PWR_ReadCmd(PWRFUNC_RD_CURRENT, PWRCHAN_DUT, (U8 * )VoltStr)==TRUE)//发送读电流命令
    {
        *pCur=atoi((char *)VoltStr);//ASCII转换成int型数字
         return TRUE;
    }
    else
    {
        *pCur=0;
        return FALSE;
    }
}
/******************************************************************************
函数名    : PWR_AddDutVolt
参数      : stepval 步进值   1个步进电压值0.09884v
返回值    : TRUE/FALSE
功能描述  : 在当前电压的基础上增加(0.09884*stepval)V电压  
例如当前电压5v  执行PWR_AddDutVolt(10)后DUT电压值=5+10*0.098=5.98v
******************************************************************************/
BOOL PWR_AddDutVolt(U32 Stepval)
{
    U8 VoltStr[6];

    Hex2Str32(VoltStr, Stepval);//数字转换成ASCII
    return(PWR_WriteCmd(PWRFUNC_VOL_ADD, PWRCHAN_DUT, (U8 * )VoltStr));//发送调节电源电压上升命令
}
/******************************************************************************
函数名    : PWR_SubDutVolt
参数      : stepval 步进值
返回值    : TRUE/FALSE 1个步进电压值0.09884v
功能描述  : 在当前电压的基础上减去(0.09884*stepval)V电压
例如当前电压5v  执行PWR_AddDutVolt(10)后DUT电压值=5-10*0.098=4.02v
******************************************************************************/
BOOL PWR_SubDutVolt(U32 Stepval)
{
    U8 VoltStr[6];

    Hex2Str32(VoltStr, Stepval);//数字转换成ASCII
    return(PWR_WriteCmd(PWRFUNC_VOL_SUB, PWRCHAN_DUT, (U8 * )VoltStr));//发送调节电源电压下降命令
}

