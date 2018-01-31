/*****************************************************************************
    MCP3421_ADC.h
    MCP3421_ADCAPI functions

    Copyright(C) 2014, Honeywell Integrated Technology (China) Co.,Ltd.
    Security FCT team
    All rights reserved.

    History
    2014.09.09  ver.1.00    First release
******************************************************************************/

#ifndef _MCP3421_ADC_H_
#define _MCP3421_ADC_H_

#include "includes.h"

/*******************************************************************************
    Definitions
*******************************************************************************/
//设置AD 精度
#define PRECISION_12BIT       (0)
#define PRECISION_14BIT       (1)
#define PRECISION_16BIT       (2)
#define PRECISION_18BIT       (3)

//设置AD 增益
#define PGA_1VV         (0)
#define PGA_2VV         (1)
#define PGA_4VV         (2)
#define PGA_8VV         (3)

//设置AD 量程
#define     _RANGE_0_2V         1 // 0~2V
#define     _RANGE_0_20V        2 // 0 ~20v
#define     _RANGE_0_200V       3 // 0 ~200v

/*******************************************************************************
    API functions
*******************************************************************************/
extern void i2c_ADC_init(float coef_range_1to1, float coef_range_10to1, float coef_range_100to1);
extern INT32U ADC_cal_value(INT32U val_range, INT32U val_precision);
extern INT32U ADC_Test(void);
extern INT32U AD_MeasureAutoRange(INT32U VoltMax);

#endif	/* _I2C_API_H_ */

/*********************************** EOF **************************************/
