/******************************************************************************
    i2c_api.h
    API functions

    Copyright(C) 2009, Honeywell Integrated Technology (China) Co.,Ltd.
    Security FCT team
    All rights reserved.

    History
    2010.02.25  ver.1.00    First release
******************************************************************************/
/******************************************************************************
    Note : Use GPIO simulate i2c function.
******************************************************************************/

#include "includes.h"

/* --- Prototype ----------------------------------------------------------- */
void i2c_delay(void);
void i2c_SDA_Output(int data);
unsigned char i2c_SDA_Input(void);
void i2c_SCL_Output(int data);
void i2c_Start(void);
void i2c_Stop(void);
void i2c_SendByte(unsigned char ch);
unsigned char i2c_ReceiveByte(void);
int  i2c_WaitAck(void);
void i2c_SendAck(void);
void i2c_SendNotAck(void);

#define TC_CLKS_MCK2             0x0
#define TC_CLKS_MCK8             0x1
#define TC_CLKS_MCK32            0x2
#define TC_CLKS_MCK128           0x3

#define TIMER_PERIOD_5US         1000    // TC_CLKS_MCK2, 100MHz


/******************************************************************************
    Routine Name    : i2c_init
    Form            : void i2c_init(void)
    Parameters      : none
    Return value    : none
    Description     : The initialize function of the I2C.
******************************************************************************/
void i2c_init(void)
{
    I2C1_GPIO_BASE->PIO_PER     = I2C1_GPIO_SDA | I2C1_GPIO_SCL;
    I2C1_GPIO_BASE->PIO_SODR    = I2C1_GPIO_SDA | I2C1_GPIO_SCL;
    I2C1_GPIO_BASE->PIO_OER     = I2C1_GPIO_SDA | I2C1_GPIO_SCL;

	// Open timer5
    AT91C_BASE_PMC->PMC_PCER    = 1 << I2C1_TC_ID;

    //* Disable the clock and the interrupts
	I2C1_TC_BASE->TC_CCR        = AT91C_TC_CLKDIS ;
	I2C1_TC_BASE->TC_IDR        = 0xFFFFFFFF ;

    //* Clear status bit
	I2C1_TC_BASE->TC_SR;
    //* Set the Mode of the Timer Counter
	I2C1_TC_BASE->TC_CMR        = TC_CLKS_MCK2 | AT91C_TC_CPCTRG ;

    //* Enable the clock
	I2C1_TC_BASE->TC_CCR        = AT91C_TC_CLKEN;

	I2C1_TC_BASE->TC_SR;

	// Set compare value.
	I2C1_TC_BASE->TC_RC         = TIMER_PERIOD_5US;

//    i2c_Stop();
}


/******************************************************************************
    Routine Name    : i2c_read8574
    Form            : int i2c_read8574(unsigned char slave_add, unsigned char *data)
    Parameters      : unsigned char slave_add
                      unsigned char reg
                      unsigned char data
    Return value    : error code
    Description     : Writes the data to the device.
******************************************************************************/
int i2c_read8574(unsigned char slave_add, unsigned char *data)
{
	i2c_Start();
	i2c_SendByte(slave_add | 0x01);
	if(i2c_WaitAck() == FALSE)
	{
		i2c_Stop();
		return FALSE;
	}
	*data = i2c_ReceiveByte();
	i2c_SendAck();
	i2c_Stop();

	return TRUE;
}


/******************************************************************************
    Routine Name    : i2c_write8574
    Form            : int i2c_write8574(unsigned char slave_add, unsigned char data)
    Parameters      : unsigned char  slave_add
                      unsigned short reg
                      unsigned char  data
    Return value    : error code
    Description     : Writes the data to the device.
******************************************************************************/
int i2c_write8574(unsigned char slave_add, unsigned char data)
{

	i2c_Start();
	i2c_SendByte(slave_add & 0xFE);
	if(i2c_WaitAck() == FALSE)
	{
		i2c_Stop();
		return FALSE;
	}
	i2c_SendByte(data);
	if(i2c_WaitAck() == FALSE)
	{
		i2c_Stop();
		return FALSE;
	}
	i2c_Stop();

	return TRUE;
}


/******************************************************************************
    Routine Name    : i2c_delay
    Form            : void i2c_delay(void)
    Parameters      : none
    Return value    : none
    Description     : Delay Function for I2C.
******************************************************************************/
void i2c_delay(void)
{
	I2C1_TC_BASE->TC_SR;

	// Start timer5
	I2C1_TC_BASE->TC_CCR = AT91C_TC_SWTRG;

	I2C1_TC_BASE->TC_SR;

    while((I2C1_TC_BASE->TC_SR & AT91C_TC_CPCS) == 0)
    {
    	;
    }
}


/******************************************************************************
    Routine Name    : i2c_SDA_Output
    Form            : void i2c_SDA_Output(int data)
    Parameters      : int data
    Return value    : none
    Description     : Send SDA signal.
******************************************************************************/
void i2c_SDA_Output(int data)
{
	if(data == 0)
	{
        I2C1_GPIO_BASE->PIO_CODR = I2C1_GPIO_SDA;
	}
	else
	{
        I2C1_GPIO_BASE->PIO_SODR = I2C1_GPIO_SDA;
	}
}


/******************************************************************************
    Routine Name    : i2c_SDA_Input
    Form            : unsigned char i2c_SDA_Input(void)
    Parameters      : none
    Return value    : Value of SDA
    Description     : Get SDA signal.
******************************************************************************/
unsigned char i2c_SDA_Input(void)
{
    if ( (I2C1_GPIO_BASE->PIO_PDSR & I2C1_GPIO_SDA ) == I2C1_GPIO_SDA )
	{
	    return 0x01;
	}
	else
	{
		return 0x00;
	}
}


/******************************************************************************
    Routine Name    : i2c_SCL_Output
    Form            : void i2c_SCL_Output(int data)
    Parameters      : int data
    Return value    : none
    Description     : Send SCL signal.
******************************************************************************/
void i2c_SCL_Output(int data)
{
	if(data == 0)
	{
	    I2C1_GPIO_BASE -> PIO_CODR = I2C1_GPIO_SCL;
	}
	else
	{
		I2C1_GPIO_BASE -> PIO_SODR = I2C1_GPIO_SCL;
	}

}


/******************************************************************************
    Routine Name    : i2c_Start
    Form            : void i2c_SCL_Output(void)
    Parameters      : none
    Return value    : none
    Description     : Send start signal.
******************************************************************************/
void i2c_Start(void)
{
	i2c_SDA_Output(1);
	i2c_SCL_Output(1);
	i2c_delay();
	i2c_SDA_Output(0);
	i2c_delay();
	i2c_SCL_Output(0);
    i2c_delay();
}


/******************************************************************************
    Routine Name    : i2c_Stop
    Form            : void i2c_Stop(void)
    Parameters      : none
    Return value    : none
    Description     : Send stop signal.
******************************************************************************/
void i2c_Stop(void)
{
	i2c_SDA_Output(0);
	i2c_delay();
	i2c_SCL_Output(1);
	i2c_delay();
	i2c_SDA_Output(1);
	i2c_delay();
}


/******************************************************************************
    Routine Name    : i2c_SendByte
    Form            : void i2c_SendByte(unsigned char ch)
    Parameters      : unsigned char ch
    Return value    : none
    Description     : Send data(one byte).
******************************************************************************/
void i2c_SendByte(unsigned char ch)
{
	unsigned char i;

	for(i = 0; i < 8; i++)
	{
		i2c_SDA_Output(ch & 0x80);
		ch <<= 1;
		i2c_delay();
		i2c_SCL_Output(1);
		i2c_delay();
        i2c_SCL_Output(0);
        i2c_delay();
	}
}


/******************************************************************************
    Routine Name    : i2c_ReceiveByte
    Form            : void i2c_SendByte(unsigned char)
    Parameters      : none
    Return value    : Recived data
    Description     : Get data(one byte).
******************************************************************************/
unsigned char i2c_ReceiveByte(void)
{
	unsigned char i;
	unsigned char ddata;

	ddata = 0;

	I2C1_GPIO_BASE->PIO_ODR = I2C1_GPIO_SDA;
	for(i = 0; i < 8; i++)
	{
        ddata <<= 1;
		i2c_SCL_Output(1);
        i2c_delay();
		ddata |= i2c_SDA_Input();
        i2c_delay();
		i2c_SCL_Output(0);
        i2c_delay();
	}
	I2C1_GPIO_BASE->PIO_OER = I2C1_GPIO_SDA;
	return ddata;

}


/******************************************************************************
    Routine Name    : i2c_WaitAck
    Form            : int i2c_WaitAck(void)
    Parameters      : none
    Return value    : errer code
    Description     : Wait for ACK.
******************************************************************************/
int i2c_WaitAck(void)
{
	unsigned int errtime;

	errtime = 300000;

	I2C1_GPIO_BASE->PIO_ODR =  I2C1_GPIO_SDA;
	i2c_delay();
    i2c_SCL_Output(1);
    i2c_delay();
	while ( (I2C1_GPIO_BASE->PIO_PDSR & I2C1_GPIO_SDA ) == I2C1_GPIO_SDA )
	{
		errtime--;
		if (!errtime)
		{
			I2C1_GPIO_BASE->PIO_OER = I2C1_GPIO_SDA;
            i2c_SDA_Output(1);
            i2c_SCL_Output(0);
			return FALSE;
		}
	}
	i2c_SCL_Output(0);
	I2C1_GPIO_BASE->PIO_OER = I2C1_GPIO_SDA;
    i2c_delay();
	return TRUE;
}


/******************************************************************************
    Routine Name    : i2c_SendAck
    Form            : void i2c_SendAck(void)
    Parameters      : none
    Return value    : none
    Description     : Send ACK.
******************************************************************************/
void i2c_SendAck(void)
{
	i2c_SDA_Output(0);
	i2c_delay();
	i2c_SCL_Output(1);
	i2c_delay();
	i2c_delay();
	i2c_SCL_Output(0);
	i2c_delay();
    i2c_SDA_Output(0);
}


/******************************************************************************
    Routine Name    : i2c_SendNotAck
    Form            : void i2c_SendNotAck(void)
    Parameters      : none
    Return value    : none
    Description     : Send Not ACK.
******************************************************************************/
void i2c_SendNotAck(void)
{
	i2c_SDA_Output(1);
	i2c_delay();
	i2c_SCL_Output(1);
	i2c_delay();
	i2c_SCL_Output(0);
	i2c_delay();
    i2c_SDA_Output(0);
}

/*********************************** EOF **************************************/
