/*****************************************************************************
    i2c_api.h
    API functions

    Copyright(C) 2009, Honeywell Integrated Technology (China) Co.,Ltd.
    Security FCT team
    All rights reserved.

    History
    2009.02.25  ver.1.00    First release
******************************************************************************/

#ifndef _I2C_API_H_
#define _I2C_API_H_

#include "includes.h"

#if defined(__cplusplus)
extern "C"
{
#endif

/*******************************************************************************
    Definitions
*******************************************************************************/
enum
{
	I2C_ER_OK = 0,
	I2C_ER_NG = -1
};

/*******************************************************************************
    API functions
*******************************************************************************/
extern void i2c_init(void);
extern int  i2c_write8574(unsigned char slave_add, unsigned char data);
extern int  i2c_read8574(unsigned char slave_add, unsigned char *data);

extern void i2c_delay(void);
extern void i2c_SDA_Output(int data);
extern unsigned char i2c_SDA_Input(void);
extern void i2c_SCL_Output(int data);
extern void i2c_Start(void);
extern void i2c_Stop(void);
extern void i2c_SendByte(unsigned char ch);
extern unsigned char i2c_ReceiveByte(void);
extern int  i2c_WaitAck(void);
extern void i2c_SendAck(void);
extern void i2c_SendNotAck(void);


/* for porting */
#define I2C1_GPIO_BASE      AT91C_BASE_PIOA
#define I2C1_GPIO_SDA       AT91C_PIO_PA23
#define I2C1_GPIO_SCL       AT91C_PIO_PA24
#define I2C1_TC_BASE        AT91C_BASE_TC5
#define I2C1_TC_ID          AT91C_ID_TC5


#if defined(__cplusplus)
}
#endif

#endif	/* _I2C_API_H_ */

/*********************************** EOF **************************************/
