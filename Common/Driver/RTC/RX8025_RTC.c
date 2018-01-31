/******************************************************************************
    RX8025_RTC.c
    RX8025_RTC functions

    Copyright(C) 2014, Honeywell Integrated Technology (China) Co.,Ltd.
    Security FCT team
    All rights reserved.

    History
    2014.09.09  ver.1.00    First release
******************************************************************************/
#include "includes.h"

/* for porting */
#define I2C_RTC_BASE      AT91C_BASE_PIOB
#define I2C_RTC_SDA       AT91C_PIO_PB29
#define I2C_RTC_SCL       AT91C_PIO_PB27
#define I2C_RTC_TC_BASE   AT91C_BASE_TC4
#define I2C_RTC_TC_ID     AT91C_ID_TC4

#define RTC_8025_ADDR_R     0x65
#define RTC_8025_ADDR_W     0x64

#define TIMER_PERIOD_5US         50    // TC_CLKS_MCK2, 100MHz

#define TC_CLKS_MCK2             0x0
#define TC_CLKS_MCK8             0x1
#define TC_CLKS_MCK32            0x2
#define TC_CLKS_MCK128           0x3

/* --- Prototype ----------------------------------------------------------- */
static void i2c_RTC_delay(void);
static void i2c_RTC_SDA_Output(int data);
static unsigned char i2c_RTC_SDA_Input(void);
static void i2c_RTC_SCL_Output(int data);
static void i2c_RTC_Start(void);
static void i2c_RTC_Stop(void);
static void i2c_RTC_SendByte(unsigned char ch);
static unsigned char i2c_RTC_ReceiveByte(void);
static int  i2c_RTC_WaitAck(void);
static void i2c_RTC_SendAck(void);
static void i2c_RTC_SendNotAck(void);
static INT8U HEX2BCD(INT8U bcd_data);
static INT8U BCD2HEX(INT8U bcd_data);

/******************************************************************************
    Routine Name    : i2c_init
    Form            : void i2c_init(void)
    Parameters      : none
    Return value    : none
    Description     : The initialize function of the I2C.
******************************************************************************/
void i2c_RTC_init(void)
{
    AT91C_BASE_PMC->PMC_PCER    = 1 << AT91C_ID_PIOB;
    I2C_RTC_BASE->PIO_PER     = I2C_RTC_SDA | I2C_RTC_SCL;
    I2C_RTC_BASE->PIO_SODR    = I2C_RTC_SDA | I2C_RTC_SCL;
    I2C_RTC_BASE->PIO_OER     = I2C_RTC_SDA | I2C_RTC_SCL;

	// Open timer5
    AT91C_BASE_PMC->PMC_PCER    = 1 << I2C_RTC_TC_ID;

    //* Disable the clock and the interrupts
	I2C_RTC_TC_BASE->TC_CCR        = AT91C_TC_CLKDIS ;
	I2C_RTC_TC_BASE->TC_IDR        = 0xFFFFFFFF ;

    //* Clear status bit
	I2C_RTC_TC_BASE->TC_SR;
    //* Set the Mode of the Timer Counter
	I2C_RTC_TC_BASE->TC_CMR        = TC_CLKS_MCK2 | AT91C_TC_CPCTRG ;

    //* Enable the clock
	I2C_RTC_TC_BASE->TC_CCR        = AT91C_TC_CLKEN;

	I2C_RTC_TC_BASE->TC_SR;

	// Set compare value.
	I2C_RTC_TC_BASE->TC_RC         = TIMER_PERIOD_5US;

    i2c_RTC_Stop();
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
int i2c_RTC_write8574(unsigned char slave_add, unsigned char data)
{

	i2c_RTC_Start();
	i2c_RTC_SendByte(slave_add & 0xFE);
	if(i2c_RTC_WaitAck() == FALSE)
	{
		i2c_RTC_Stop();
		return FALSE;
	}
	i2c_RTC_SendByte(data);
	if(i2c_RTC_WaitAck() == FALSE)
	{
		i2c_RTC_Stop();
		return FALSE;
	}
	i2c_RTC_Stop();

	return TRUE;
}


/******************************************************************************
    Routine Name    : i2c_delay
    Form            : void i2c_delay(void)
    Parameters      : none
    Return value    : none
    Description     : Delay Function for I2C.
******************************************************************************/
static void i2c_RTC_delay(void)
{
	I2C_RTC_TC_BASE->TC_SR;

	// Start timer5
	I2C_RTC_TC_BASE->TC_CCR = AT91C_TC_SWTRG;

	I2C_RTC_TC_BASE->TC_SR;

#ifndef DEBUG_IRNORE_I2C
    while((I2C_RTC_TC_BASE->TC_SR & AT91C_TC_CPCS) == 0)
    {
    	;
    }
#endif
}


/******************************************************************************
    Routine Name    : i2c_SDA_Output
    Form            : void i2c_SDA_Output(int data)
    Parameters      : int data
    Return value    : none
    Description     : Send SDA signal.
******************************************************************************/
static void i2c_RTC_SDA_Output(int data)
{
	if(data == 0)
	{
        I2C_RTC_BASE->PIO_CODR = I2C_RTC_SDA;
	}
	else
	{
        I2C_RTC_BASE->PIO_SODR = I2C_RTC_SDA;
	}
}


/******************************************************************************
    Routine Name    : i2c_SDA_Input
    Form            : unsigned char i2c_SDA_Input(void)
    Parameters      : none
    Return value    : Value of SDA
    Description     : Get SDA signal.
******************************************************************************/
static unsigned char i2c_RTC_SDA_Input(void)
{
    if ( (I2C_RTC_BASE->PIO_PDSR & I2C_RTC_SDA ) == I2C_RTC_SDA )
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
static void i2c_RTC_SCL_Output(int data)
{
	if(data == 0)
	{
	    I2C_RTC_BASE -> PIO_CODR = I2C_RTC_SCL;
	}
	else
	{
		I2C_RTC_BASE -> PIO_SODR = I2C_RTC_SCL;
	}
}

/******************************************************************************
    Routine Name    : i2c_Start
    Form            : void i2c_SCL_Output(void)
    Parameters      : none
    Return value    : none
    Description     : Send start signal.
******************************************************************************/
static void i2c_RTC_Start(void)
{
	i2c_RTC_SDA_Output(1);
	i2c_RTC_SCL_Output(1);
	i2c_RTC_delay();
	i2c_RTC_SDA_Output(0);
	i2c_RTC_delay();
	i2c_RTC_SCL_Output(0);
    i2c_RTC_delay();
}

/******************************************************************************
    Routine Name    : i2c_Stop
    Form            : void i2c_Stop(void)
    Parameters      : none
    Return value    : none
    Description     : Send stop signal.
******************************************************************************/
static void i2c_RTC_Stop(void)
{
	i2c_RTC_SDA_Output(0);
	i2c_RTC_delay();
	i2c_RTC_SCL_Output(1);
	i2c_RTC_delay();
	i2c_RTC_SDA_Output(1);
	i2c_RTC_delay();
}

/******************************************************************************
    Routine Name    : i2c_SendByte
    Form            : void i2c_SendByte(unsigned char ch)
    Parameters      : unsigned char ch
    Return value    : none
    Description     : Send data(one byte).
******************************************************************************/
static void i2c_RTC_SendByte(unsigned char ch)
{
	unsigned char i;

	for(i = 0; i < 8; i++)
	{
		i2c_RTC_SDA_Output(ch & 0x80);
		ch <<= 1;
		i2c_RTC_delay();
		i2c_RTC_SCL_Output(1);
		i2c_RTC_delay();
        i2c_RTC_SCL_Output(0);
        i2c_RTC_delay();
	}
}

/******************************************************************************
    Routine Name    : i2c_ReceiveByte
    Form            : void i2c_SendByte(unsigned char)
    Parameters      : none
    Return value    : Recived data
    Description     : Get data(one byte).
******************************************************************************/
static unsigned char i2c_RTC_ReceiveByte(void)
{
	unsigned char i;
	unsigned char data;

	data = 0;

	I2C_RTC_BASE->PIO_ODR = I2C_RTC_SDA;
	for(i = 0; i < 8; i++)
	{
        data <<= 1;
		i2c_RTC_SCL_Output(1);
        i2c_RTC_delay();
		data |= i2c_RTC_SDA_Input();
        i2c_RTC_delay();
		i2c_RTC_SCL_Output(0);
        i2c_RTC_delay();
	}
	I2C_RTC_BASE->PIO_OER = I2C_RTC_SDA;
	return data;
}

/******************************************************************************
    Routine Name    : i2c_WaitAck
    Form            : int i2c_WaitAck(void)
    Parameters      : none
    Return value    : errer code
    Description     : Wait for ACK.
******************************************************************************/
static int i2c_RTC_WaitAck(void)
{
	unsigned int errtime;

#ifdef DEBUG_IRNORE_I2C
	errtime = 3;
#else
	errtime = 300000;
#endif

	I2C_RTC_BASE->PIO_ODR =  I2C_RTC_SDA;
	i2c_RTC_delay();
    i2c_RTC_SCL_Output(1);
    i2c_RTC_delay();
	while ( (I2C_RTC_BASE->PIO_PDSR & I2C_RTC_SDA ) == I2C_RTC_SDA )
	{
		errtime--;
		if (!errtime)
		{
			I2C_RTC_BASE->PIO_OER = I2C_RTC_SDA;
            i2c_RTC_SDA_Output(1);
            i2c_RTC_SCL_Output(0);
			return FALSE;
		}
	}
	i2c_RTC_SCL_Output(0);
	I2C_RTC_BASE->PIO_OER = I2C_RTC_SDA;
    i2c_RTC_delay();
	return TRUE;
}

/******************************************************************************
    Routine Name    : i2c_SendAck
    Form            : void i2c_SendAck(void)
    Parameters      : none
    Return value    : none
    Description     : Send ACK.
******************************************************************************/
static void i2c_RTC_SendAck(void)
{
	i2c_RTC_SDA_Output(0);
	i2c_RTC_delay();
	i2c_RTC_SCL_Output(1);
	i2c_RTC_delay();
	i2c_RTC_delay();
	i2c_RTC_SCL_Output(0);
	i2c_RTC_delay();
    i2c_RTC_SDA_Output(0);
}

/******************************************************************************
    Routine Name    : i2c_SendNotAck
    Form            : void i2c_SendNotAck(void)
    Parameters      : none
    Return value    : none
    Description     : Send Not ACK.
******************************************************************************/
static void i2c_RTC_SendNotAck(void)
{
	i2c_RTC_SDA_Output(1);
	i2c_RTC_delay();
	i2c_RTC_SCL_Output(1);
	i2c_RTC_delay();
	i2c_RTC_SCL_Output(0);
	i2c_RTC_delay();
    i2c_RTC_SDA_Output(0);
}

/*********************************** EOF **************************************/
INT32U i2c_RTC8025_configTime(RTC_Time rtcTime)
{
	INT8U i;
    INT32U slave_add = 0;
    //传送时需要按照 8421传送
    RTC_Time rtcHexTime = { BCD2HEX(rtcTime.sec),  BCD2HEX(rtcTime.min), BCD2HEX(rtcTime.hour),  BCD2HEX(rtcTime.week), 
                            BCD2HEX(rtcTime.day), BCD2HEX(rtcTime.month), BCD2HEX(rtcTime.year) };
    
    INT8U *temp = (INT8U*)&rtcHexTime;
    
    i2c_RTC_Start();
	i2c_RTC_SendByte( RTC_8025_ADDR_W ); 
	if(i2c_RTC_WaitAck() != TRUE)
	{
		i2c_RTC_Stop();
		return FALSE;
	}

    i2c_RTC_SendByte(slave_add);		
    if(i2c_RTC_WaitAck() != TRUE)
	{
		i2c_RTC_Stop();
		return FALSE;
	}
    
    //写入相关 时间参数值
    for(i = 0; i < sizeof(RTC_Time)/ sizeof(INT8U); i++)
    {
        i2c_RTC_SendByte(temp[i]);		
        if(i2c_RTC_WaitAck() != TRUE)
        {
            i2c_RTC_Stop();
            return FALSE;
        }
    }
    
    //写入ALARM
    for(i = (sizeof(RTC_Time)/ sizeof(INT8U)); i < 14; i++)
    {
        i2c_RTC_SendByte(0);		
        if(i2c_RTC_WaitAck() != TRUE)
        {
            i2c_RTC_Stop();
            return FALSE;
        }
    }
    
    //写入配置control 1
    i2c_RTC_SendByte(0x20);  //24 小时模式		
    if(i2c_RTC_WaitAck() != TRUE)
    {
        i2c_RTC_Stop();
        return FALSE;
    }
    
    //写入配置control 2
    i2c_RTC_SendByte(0);		
    if(i2c_RTC_WaitAck() != TRUE)
    {
        i2c_RTC_Stop();
        return FALSE;
    }
    
	i2c_RTC_Stop();

	return TRUE;
}

INT32U i2c_RTC8025_readTime(RTC_Time * rtcTime)
{
    INT8U i;
    RTC_Time rtcBCDTime = {0};      
    INT8U *temp = (INT8U*)&rtcBCDTime;

	i2c_RTC_Start();
	i2c_RTC_SendByte( RTC_8025_ADDR_W ); 
	if(i2c_RTC_WaitAck() != TRUE)
	{
		i2c_RTC_Stop();
		return FALSE;
	}
    // write address, 0:SEC; 1:MIN; 2:HOU; 3:WEEK; 4:DAY; 5:MONTH; 6:YEAR
    i2c_RTC_SendByte(0x00);	

    if(i2c_RTC_WaitAck() != TRUE)
	{
		i2c_RTC_Stop();
		return FALSE;
	}

    i2c_RTC_Start();
    
	i2c_RTC_SendByte(RTC_8025_ADDR_R); 		// read cmd

    if(i2c_RTC_WaitAck() != TRUE)
	{
		i2c_RTC_Stop();
		return FALSE;
	}

	for ( i = 0; i <= 5; i++ )
    {
        temp[i] = i2c_RTC_ReceiveByte();		//read data
        i2c_RTC_SendAck();
    }
	
    temp[6] = i2c_RTC_ReceiveByte();			//read data    
    i2c_RTC_SendNotAck();
	i2c_RTC_Stop();    
    
    //人可读的是 10进制数，转之
    RTC_Time rtcTimeTemp = { HEX2BCD(rtcBCDTime.sec), HEX2BCD(rtcBCDTime.min), HEX2BCD(rtcBCDTime.hour),  HEX2BCD(rtcBCDTime.week), 
                HEX2BCD(rtcBCDTime.day), HEX2BCD(rtcBCDTime.month), HEX2BCD(rtcBCDTime.year)};
    
    *rtcTime = rtcTimeTemp;
    
	return TRUE;
}

//16进制转bcd码
static INT8U HEX2BCD(INT8U bcd_data)    //hex转为bcd子程序 
{
    INT8U temp;
    temp=(bcd_data/16*10+bcd_data%16);
    return temp;
}

//BCD转16进制
static INT8U BCD2HEX(INT8U hex_data)    //BCD转为HEX子程序 
{
    INT8U temp;
    temp=(hex_data/10*16+hex_data%10);
    return temp;
}

INT32U i2c_RTC_InitTime()
{
    INT32U status = 0;
    //sec，min, hour, week, day, month, year 2014.7.21 13:20:10 MON
    RTC_Time RtcTimeData = { NOW_SECOND, NOW_MINUTE, NOW_HOUR, NOW_WEEK, NOW_DAY, NOW_MONTH, NOW_YEAR}; 
    RTC_Time retTime;
    
    i2c_RTC_init();
    status = i2c_RTC8025_configTime(RtcTimeData);
    if(status != TRUE)
        return FALSE;
    return TRUE;
}

void RTC_test()
{
    INT32U status =0;
    //sec，min, hour, week, day, month, year 2014.7.21 13:20:10 MON
    RTC_Time RtcTimeData = { 10, 20, 13, 1, 21, 7, 14}; 
    RTC_Time retTime;
    
    i2c_RTC_init();
    status = i2c_RTC8025_configTime(RtcTimeData);
    if(status != TRUE)
        return;
        
    OS_Delay(3000);
    
    status = i2c_RTC8025_readTime(&retTime);
    if(status != TRUE)
        return;
    
    if ((retTime.sec - RtcTimeData.sec ) == 10)
    {
        OS_Delay(1000);
        //display pass
    }
    else
    {
         OS_Delay(1000);
         //display fail
    }
    
#ifdef FILE_RTC_TEST
    FS_FILE *fb;
    if(fb = FS_FOpen("test.txt","a+"))
    {
        FS_FWrite("abc", 1, strlen((char * )"abc"), fb);
        
          INT32U TimeStamp;
          FS_FILETIME FileTime;
          FileTime.Year   = (INT16U)(retTime.year + 2000);
          FileTime.Month  = (INT16U)retTime.month;
          FileTime.Day    = (INT16U)retTime.day;
          FileTime.Hour   = (INT16U)retTime.hour;
          FileTime.Minute = (INT16U)retTime.min;
          FileTime.Second = (INT16U)retTime.sec;
          
          FS_FileTimeToTimeStamp (&FileTime, &TimeStamp);
          FS_SetFileTime("test.txt", TimeStamp);
    }
    FS_FClose(fb);
#endif    
}
