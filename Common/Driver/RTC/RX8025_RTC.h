/*****************************************************************************
    RX8025_RTC.h
    RTC API functions

    Copyright(C) 2014, Honeywell Integrated Technology (China) Co.,Ltd.
    Security FCT team
    All rights reserved.

    History
    2014.09.09  ver.1.00    First release
******************************************************************************/

#ifndef _RX8024_RTC_H_
#define _RX8024_RTC_H_

#include "includes.h"

/******************************************************************************
Note:在调试 RX8024时，I2C 不能设断点，否则会失败。
******************************************************************************/

/*******************************************************************************
    Definitions
*******************************************************************************/
typedef struct
{ 
    //0:SEC; 1:MIN; 2:HOU; 3:WEEK; 4:DAY; 5:MONTH; 6:YEAR
    INT8U sec;
    INT8U min;
    INT8U hour;
    INT8U week;
    INT8U day;
    INT8U month;
    INT8U year; 
}RTC_Time;

/*******************************************************************************
    API functions
*******************************************************************************/
extern void i2c_RTC_init(void);
extern INT32U i2c_RTC8025_configTime(RTC_Time rtcTime);
extern INT32U i2c_RTC8025_readTime(RTC_Time * rtcTime);
extern INT32U i2c_RTC_InitTime();
extern void RTC_test();

#endif	/* _I2C_RTC_H_ */

/*********************************** EOF **************************************/
