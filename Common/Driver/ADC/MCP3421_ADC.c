/******************************************************************************
    MCP3421_ADC.c
    MCP3421_ADC functions

    Copyright(C) 2014, Honeywell Integrated Technology (China) Co.,Ltd.
    Security FCT team
    All rights reserved.

    History
    2014.09.09  ver.1.00    First release
******************************************************************************/
#include "includes.h"

/* for porting */
#define I2C_ADC_BASE      AT91C_BASE_PIOC
#define I2C_ADC_SDA       AT91C_PIO_PC13
#define I2C_ADC_SCL       AT91C_PIO_PC11
#define I2C_ADC_TC_BASE   AT91C_BASE_TC5
#define I2C_ADC_TC_ID     AT91C_ID_TC5

#define TC_CLKS_MCK2             0x0
#define TC_CLKS_MCK8             0x1
#define TC_CLKS_MCK32            0x2
#define TC_CLKS_MCK128           0x3

#define TIMER_PERIOD_5US         100    // TC_CLKS_MCK2, 100MHz

#define ADC_GPIO_ID         AT91C_ID_PIOC
#define ADC_GPIO_RANG10     AT91C_PIO_PC6
#define ADC_GPIO_RANG100    AT91C_PIO_PC9

#define ADC3421_ADDR_R     0xD1
#define ADC3421_ADDR_W     0xD0

#define  INITIATE_TRANSITION   1

#define SRS_12BIT       (5)
#define SRS_14BIT       (17)
#define SRS_16BIT       (67)
#define SRS_18BIT       (300)

#define LIMIT_12BIT       (0xFFF)
#define LIMIT_14BIT       (0x3FFF)
#define LIMIT_16BIT       (0xFFFF)
#define LIMIT_18BIT       (0x3FFFF)

static float  verify_coef_range_1to1  ;   // 无分压矫正系数
static float  verify_coef_range_10to1 ;   // 10:1 分压矫正系数
static float  verify_coef_range_100to1;   // 100:1 分压矫正系数

static void i2c_ADC_delay(void);
static void i2c_ADC_SDA_Output(int data);
static unsigned char i2c_ADC_SDA_Input(void);
static void i2c_ADC_SCL_Output(int data);
static void i2c_ADC_Start(void);
static void i2c_ADC_Stop(void);
static void i2c_ADC_SendByte(unsigned char ch);
static unsigned char i2c_ADC_ReceiveByte(void);
static int  i2c_ADC_WaitAck(void);
static void i2c_ADC_SendAck(void);
static void i2c_ADC_SendNotAck(void);

static void ADC_VoltIn_1to1_ENABLE();
static void ADC_VoltIn_10to1_ENABLE();
static void ADC_VoltIn_100to1_ENABLE();

static INT8U i2c_ADC3421_ConfigADC(unsigned char configValue);
static INT8U i2c_ADC3421_readVoltage(INT8U * pVoltage, INT8U length);
static INT32U ADC_value_no_verify(INT32U val_range, INT32U val_precision);

/******************************************************************************
    Routine Name    : i2c_init
    Form            : void i2c_init(void)
    Parameters      : none
    Return value    : none
    Description     : The initialize function of the I2C.
******************************************************************************/
static void ADC_init(void)
{
    volatile INT32U adReg;

    // init spi port
    AT91C_BASE_PMC->PMC_PCER    = 1 << ADC_GPIO_ID;

    AT91C_BASE_PMC->PMC_PCER |= 1 << AT91C_ID_PIOC;

    I2C_ADC_BASE->PIO_PER = ADC_GPIO_RANG10;
    I2C_ADC_BASE->PIO_OER = ADC_GPIO_RANG10;
    
    I2C_ADC_BASE->PIO_PER = ADC_GPIO_RANG100;
    I2C_ADC_BASE->PIO_OER = ADC_GPIO_RANG100;
}

void i2c_ADC_init(float coef_range_1to1, float coef_range_10to1, float coef_range_100to1)
{
    I2C_ADC_BASE->PIO_PER     = I2C_ADC_SDA | I2C_ADC_SCL;
    I2C_ADC_BASE->PIO_SODR    = I2C_ADC_SDA | I2C_ADC_SCL;
    I2C_ADC_BASE->PIO_OER     = I2C_ADC_SDA | I2C_ADC_SCL;

	// Open timer5
    AT91C_BASE_PMC->PMC_PCER    = 1 << I2C_ADC_TC_ID;

    //* Disable the clock and the interrupts
	I2C_ADC_TC_BASE->TC_CCR        = AT91C_TC_CLKDIS ;
	I2C_ADC_TC_BASE->TC_IDR        = 0xFFFFFFFF ;

    //* Clear status bit
	I2C_ADC_TC_BASE->TC_SR;
    //* Set the Mode of the Timer Counter
	I2C_ADC_TC_BASE->TC_CMR        = TC_CLKS_MCK2 | AT91C_TC_CPCTRG ;

    //* Enable the clock
	I2C_ADC_TC_BASE->TC_CCR        = AT91C_TC_CLKEN;

	I2C_ADC_TC_BASE->TC_SR;

	// Set compare value.
	I2C_ADC_TC_BASE->TC_RC         = TIMER_PERIOD_5US;
    
    ADC_init();
    
    //增益设置
    verify_coef_range_1to1  = coef_range_1to1;
    verify_coef_range_10to1 = coef_range_10to1;
    verify_coef_range_100to1 = coef_range_100to1;
}

/******************************************************************************
    Routine Name    : i2c_delay
    Form            : void i2c_delay(void)
    Parameters      : none
    Return value    : none
    Description     : Delay Function for I2C.
******************************************************************************/
static void i2c_ADC_delay(void)
{
	I2C_ADC_TC_BASE->TC_SR;

	// Start timer5
	I2C_ADC_TC_BASE->TC_CCR = AT91C_TC_SWTRG;

	I2C_ADC_TC_BASE->TC_SR;

#ifndef DEBUG_IRNORE_I2C
    while((I2C_ADC_TC_BASE->TC_SR & AT91C_TC_CPCS) == 0)
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
static void i2c_ADC_SDA_Output(int data)
{
	if(data == 0)
	{
        I2C_ADC_BASE->PIO_CODR = I2C_ADC_SDA;
	}
	else
	{
        I2C_ADC_BASE->PIO_SODR = I2C_ADC_SDA;
	}
}


/******************************************************************************
    Routine Name    : i2c_SDA_Input
    Form            : unsigned char i2c_SDA_Input(void)
    Parameters      : none
    Return value    : Value of SDA
    Description     : Get SDA signal.
******************************************************************************/
static unsigned char i2c_ADC_SDA_Input(void)
{
    if ( (I2C_ADC_BASE->PIO_PDSR & I2C_ADC_SDA ) == I2C_ADC_SDA )
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
static void i2c_ADC_SCL_Output(int data)
{
	if(data == 0)
	{
	    I2C_ADC_BASE -> PIO_CODR = I2C_ADC_SCL;
	}
	else
	{
		I2C_ADC_BASE -> PIO_SODR = I2C_ADC_SCL;
	}
}

/******************************************************************************
    Routine Name    : i2c_Start
    Form            : void i2c_SCL_Output(void)
    Parameters      : none
    Return value    : none
    Description     : Send start signal.
******************************************************************************/
static void i2c_ADC_Start(void)
{
	i2c_ADC_SDA_Output(1);
	i2c_ADC_SCL_Output(1);
	i2c_ADC_delay();
	i2c_ADC_SDA_Output(0);
	i2c_ADC_delay();
	i2c_ADC_SCL_Output(0);
    i2c_ADC_delay();
}

/******************************************************************************
    Routine Name    : i2c_Stop
    Form            : void i2c_Stop(void)
    Parameters      : none
    Return value    : none
    Description     : Send stop signal.
******************************************************************************/
static void i2c_ADC_Stop(void)
{
	i2c_ADC_SDA_Output(0);
	i2c_ADC_delay();
	i2c_ADC_SCL_Output(1);
	i2c_ADC_delay();
	i2c_ADC_SDA_Output(1);
	i2c_ADC_delay();
}

/******************************************************************************
    Routine Name    : i2c_SendByte
    Form            : void i2c_SendByte(unsigned char ch)
    Parameters      : unsigned char ch
    Return value    : none
    Description     : Send data(one byte).
******************************************************************************/
static void i2c_ADC_SendByte(unsigned char ch)
{
	unsigned char i;

	for(i = 0; i < 8; i++)
	{
		i2c_ADC_SDA_Output(ch & 0x80);
		ch <<= 1;
		i2c_ADC_delay();
		i2c_ADC_SCL_Output(1);
		i2c_ADC_delay();
        i2c_ADC_SCL_Output(0);
        i2c_ADC_delay();
	}
}

/******************************************************************************
    Routine Name    : i2c_ReceiveByte
    Form            : void i2c_SendByte(unsigned char)
    Parameters      : none
    Return value    : Recived data
    Description     : Get data(one byte).
******************************************************************************/
static unsigned char i2c_ADC_ReceiveByte(void)
{
	unsigned char i;
	unsigned char data;

	data = 0;

	I2C_ADC_BASE->PIO_ODR = I2C_ADC_SDA;
	for(i = 0; i < 8; i++)
	{
        data <<= 1;
		i2c_ADC_SCL_Output(1);
        i2c_ADC_delay();
		data |= i2c_ADC_SDA_Input();
        i2c_ADC_delay();
		i2c_ADC_SCL_Output(0);
        i2c_ADC_delay();
	}
	I2C_ADC_BASE->PIO_OER = I2C_ADC_SDA;
	return data;
}

/******************************************************************************
    Routine Name    : i2c_WaitAck
    Form            : int i2c_WaitAck(void)
    Parameters      : none
    Return value    : errer code
    Description     : Wait for ACK.
******************************************************************************/
static int i2c_ADC_WaitAck(void)
{
	unsigned int errtime;

#ifdef DEBUG_IRNORE_I2C
	errtime = 3;
#else
	errtime = 300000;
#endif

	I2C_ADC_BASE->PIO_ODR =  I2C_ADC_SDA;
	i2c_ADC_delay();
    i2c_ADC_SCL_Output(1);
    i2c_ADC_delay();
	while ( (I2C_ADC_BASE->PIO_PDSR & I2C_ADC_SDA ) == I2C_ADC_SDA )
	{
		errtime--;
		if (!errtime)
		{
			I2C_ADC_BASE->PIO_OER = I2C_ADC_SDA;
            i2c_ADC_SDA_Output(1);
            i2c_ADC_SCL_Output(0);
			return FALSE;
		}
	}
	i2c_ADC_SCL_Output(0);
	I2C_ADC_BASE->PIO_OER = I2C_ADC_SDA;
    i2c_ADC_delay();
	return TRUE;
}

/******************************************************************************
    Routine Name    : i2c_SendAck
    Form            : void i2c_SendAck(void)
    Parameters      : none
    Return value    : none
    Description     : Send ACK.
******************************************************************************/
static void i2c_ADC_SendAck(void)
{
	i2c_ADC_SDA_Output(0);
	i2c_ADC_delay();
	i2c_ADC_SCL_Output(1);
	i2c_ADC_delay();
	i2c_ADC_delay();
	i2c_ADC_SCL_Output(0);
	i2c_ADC_delay();
    i2c_ADC_SDA_Output(0);
}

/******************************************************************************
    Routine Name    : i2c_SendNotAck
    Form            : void i2c_SendNotAck(void)
    Parameters      : none
    Return value    : none
    Description     : Send Not ACK.
******************************************************************************/
static void i2c_ADC_SendNotAck(void)
{
	i2c_ADC_SDA_Output(1);
	i2c_ADC_delay();
	i2c_ADC_SCL_Output(1);
	i2c_ADC_delay();
	i2c_ADC_SCL_Output(0);
	i2c_ADC_delay();
    i2c_ADC_SDA_Output(0);
}

//=============================================================================
static void ADC_VoltIn_1to1_ENABLE()
{
    I2C_ADC_BASE->PIO_CODR = ADC_GPIO_RANG10;
    I2C_ADC_BASE->PIO_CODR = ADC_GPIO_RANG100;
}

static void ADC_VoltIn_10to1_ENABLE()
{
    I2C_ADC_BASE->PIO_SODR = ADC_GPIO_RANG10;
    I2C_ADC_BASE->PIO_CODR = ADC_GPIO_RANG100;
}

static void ADC_VoltIn_100to1_ENABLE()
{
    I2C_ADC_BASE->PIO_CODR = ADC_GPIO_RANG10;
    I2C_ADC_BASE->PIO_SODR = ADC_GPIO_RANG100;
}

static INT8U i2c_ADC3421_ConfigADC(unsigned char configValue)
{
	//INT8U i;
    
    i2c_ADC_Start();
	i2c_ADC_SendByte( ADC3421_ADDR_W ); 
	if(i2c_ADC_WaitAck() != TRUE)
	{
		i2c_ADC_Stop();
		return  FALSE;
	}

    i2c_ADC_SendByte(configValue);		
    if(i2c_ADC_WaitAck() != TRUE)
	{
		i2c_ADC_Stop();
		return FALSE;
	}
	i2c_ADC_Stop();
            
	return TRUE;
}

static INT8U i2c_ADC3421_readVoltage(INT8U * pVoltage, INT8U length)
{
    INT8U i;

	i2c_ADC_Start();
	i2c_ADC_SendByte( ADC3421_ADDR_R ); 
	if(i2c_ADC_WaitAck() != TRUE)
	{
		i2c_ADC_Stop();
		return FALSE;
	}
    
	for ( i = 0; i < (length - 1); i++ )
    {
        pVoltage[i] = i2c_ADC_ReceiveByte();		//read data
        i2c_ADC_SendAck();
    }
	
    pVoltage[length - 1] = i2c_ADC_ReceiveByte();			//read data    
    i2c_ADC_SendNotAck();
	i2c_ADC_Stop();

	return TRUE;
}


static INT32U ADC_value_no_verify(INT32U val_range, INT32U val_precision)
{
    INT32U i, j, control_byte;
    INT32U AD_DataTemp[16];
    INT32U IDataTemp, tempVolt;
    INT8U voltArray[4] = {0};
    INT32U status;    
    INT32U waitTimer[4] = { SRS_12BIT, SRS_14BIT, SRS_16BIT, SRS_18BIT };  //不同的精度对应不同的等待时间
    INT32U AD_Limit[4] = { LIMIT_12BIT, LIMIT_14BIT, LIMIT_16BIT, LIMIT_18BIT};
    
    control_byte = (PGA_1VV | (val_precision << 2)| (INITIATE_TRANSITION << 7)); 
    // waiting for some time after change the relay status
    //Delay_ms(200); 
  
    //读数
    for(i=0;i<16;i++)
    { 
        status  = i2c_ADC3421_ConfigADC(control_byte);
        if(status != TRUE)
            return 0;
       
        //延时读 
        OS_Delay(waitTimer[val_precision]);
        status = i2c_ADC3421_readVoltage(voltArray, 4);
        if(status != TRUE)
            return 0;        
        if(val_precision == PRECISION_18BIT)
        {

            AD_DataTemp[i] = (voltArray[0]<<16) | (voltArray[1]<<8)  | (voltArray[2]<<0) ; 
        }
        else
        {
            AD_DataTemp[i] =  (voltArray[0]<<8)  | (voltArray[1]<<0) ; 
        }   

        if(AD_DataTemp[i] > AD_Limit[val_precision])
        {
            AD_DataTemp[i] = 0;
        }
    }    

    for(j=1;j<16;j++)    // 冒泡法排序
    {        
        for(i=0;i<(16-j);i++)
        {
            if(AD_DataTemp[i]<AD_DataTemp[i+1])
            {
                IDataTemp = AD_DataTemp[i];
                AD_DataTemp[i] = AD_DataTemp[i+1];
                AD_DataTemp[i+1] = IDataTemp;
            }
        }
    }

    IDataTemp = 0;
    for(i=4; i<12; i++)
        IDataTemp += AD_DataTemp[i];

    IDataTemp = IDataTemp/8;

    if(val_range == _RANGE_0_2V) 
    {
        tempVolt = IDataTemp;
    }
    else  if(val_range == _RANGE_0_20V) 
    {
        tempVolt =  IDataTemp * 11;
    }
    else if(val_range == _RANGE_0_200V) 
    {
        tempVolt = IDataTemp * 100;
    }
    else 
        return 0;
    
    return  tempVolt * 2048 / ( (1 << (11 + 2 * val_precision) )- 1 );
}


INT32U ADC_cal_value(INT32U val_range, INT32U val_precision)
{
    float verify_coef_factor = 0;
    INT32U voltage = 0;
    switch(val_range)
    {
        case _RANGE_0_2V:
            verify_coef_factor = verify_coef_range_1to1;
            // small range
            ADC_VoltIn_1to1_ENABLE();
            break;

        case _RANGE_0_20V:
            verify_coef_factor = verify_coef_range_10to1;
            // middle range
            ADC_VoltIn_10to1_ENABLE();
            break;   
        
        case _RANGE_0_200V:
            verify_coef_factor = verify_coef_range_100to1; 
            // large range
            ADC_VoltIn_100to1_ENABLE();
            break;             
            
        default:
            return 0;
    }
    
    voltage = (INT32U)(ADC_value_no_verify(val_range, val_precision) * verify_coef_factor); 
    return  voltage; 
}

INT32U AD_MeasureAutoRange(INT32U VoltMax)
{
    INT32U volt;
    float verify_coef_factor = 0;
    
	if(VoltMax > 20000)
	{
        ADC_VoltIn_100to1_ENABLE();
        verify_coef_factor = verify_coef_range_100to1; 
		volt = ADC_value_no_verify(_RANGE_0_200V, PRECISION_12BIT);
	}
	else if(VoltMax > 2000)
	{
        ADC_VoltIn_10to1_ENABLE();
        verify_coef_factor = verify_coef_range_10to1; 
		volt = ADC_value_no_verify(_RANGE_0_20V, PRECISION_12BIT);
	}
	else
	{
        ADC_VoltIn_1to1_ENABLE();
        verify_coef_factor = verify_coef_range_1to1; 
		volt = ADC_value_no_verify(_RANGE_0_2V, PRECISION_12BIT);
	}
//    Dprintf("voltage is %d\n\r", volt);
	
	return((INT32U)(volt * verify_coef_factor));
}

INT32U ADC_value_18Bit(INT32U val_range, INT32U Gain, INT32U val_precision)
{
    INT32U i;
    INT32U j;
    INT32U control_byte=0;
    INT32U AD_DataTemp[16];
    INT32U IDataTemp, tempVolt;
    INT8U voltArray[4] = {0};
    INT32U status;    
    INT32U waitTimer[4] = { SRS_12BIT, SRS_14BIT, SRS_16BIT, SRS_18BIT };  //不同的精度对应不同的等待时间
    INT32U AD_Limit[4] = { LIMIT_12BIT, LIMIT_14BIT, LIMIT_16BIT, LIMIT_18BIT};
    
    control_byte = (Gain | (val_precision << 2)| (INITIATE_TRANSITION << 7)); 
    // waiting for some time after change the relay status
    //Delay_ms(200); 
  
    //读数
    for( i=0; i<16; i++ )
    { 
        status  = i2c_ADC3421_ConfigADC(control_byte);
        if(status != TRUE)
            return 0;
       
        //延时读 
        OS_Delay(waitTimer[val_precision]);
        status = i2c_ADC3421_readVoltage(voltArray, 4);
        if(status != TRUE)
            return 0;        
        if(val_precision == PRECISION_18BIT)
        {

            AD_DataTemp[i] = (voltArray[0]<<16) | (voltArray[1]<<8)  | (voltArray[2]<<0) ; 
        }
        else
        {
            AD_DataTemp[i] =  (voltArray[0]<<8)  | (voltArray[1]<<0) ; 
        }   

        if(AD_DataTemp[i] > AD_Limit[val_precision])
        {
            AD_DataTemp[i] = 0;
        }
    }    

    for( j=1; j<16; j++ )    // 冒泡法排序
    {        
        for(i=0; i<(16-j); i++)
        {
            if(AD_DataTemp[i] < AD_DataTemp[i+1])
            {
                IDataTemp = AD_DataTemp[i];
                AD_DataTemp[i] = AD_DataTemp[i+1];
                AD_DataTemp[i+1] = IDataTemp;
            }
        }
    }

    IDataTemp = 0;
    for(i=4; i<12; i++)
        IDataTemp += AD_DataTemp[i];

    IDataTemp = IDataTemp/8;

    if(val_range == _RANGE_0_2V) 
    {
        tempVolt = IDataTemp;
    }
    else  if(val_range == _RANGE_0_20V) 
    {
        tempVolt =  IDataTemp * 11;
    }
    else if(val_range == _RANGE_0_200V) 
    {
        tempVolt = IDataTemp * 100;
    }
    else 
        return 0;
    
    return  tempVolt*15625;
}

INT32U getADCValue(void)
{
    INT32U  gainVolt=0;
    INT32U  realVolt=0;
    ADC_VoltIn_1to1_ENABLE();
    gainVolt=ADC_value_18Bit(_RANGE_0_2V, PGA_1VV, PRECISION_18BIT);
    //realVolt = gainVolt/8;
    return gainVolt;
}

INT32U ADC_Test(void)
{  
    INT32U volt;
    
    i2c_ADC_init(1.0001, 1.0002, 1.0003);  
    
    while(1)
    {
        volt = ADC_cal_value(_RANGE_0_20V, PRECISION_12BIT);
        Dprintf("voltage is %d\n\r", volt);
        OS_Delay(300);     
    }
}

#define     MAX_COLLECT_OBJ     200

//WHOLEPLUSE:波形▁▁│▔▔│▁▁ or▔▔│▁▁│▔▔
#define     GETWHOLEPLUSE       0

//SIMPLEPLUS:波形▁▁│▔▔│▁     or▔▔│▁▁│▔
#define     GETSIMPLEPLUSE      1

//HOP:波形       ▁▁│▔▔        or▔▔│▁▁
#define     GETHOP              2

#define     STATE_START         0
#define     STATE_LEVEL_X       1
#define     STATE_LEVEL_XTOY    2
#define     STATE_LEVEL_Y       3
#define     STATE_LEVEL_YTOX    4

#define     VOLTLOW             0x00
#define     VOLTHIGH            0x01
#define     VOLTMID             0x55


//gaoxi add to get AD value direct from chip, without edge-based line averaging 
INT32S Fast_AD_cal_value(INT32U val_range)
{
    INT32S IDataTemp;
	INT32U control_byte;
/*    
    IDataTemp = ADConvert(control_byte); // unipolar
    _RANGE_LARGE();
    
    if((val_range == _RANGE_S_0_4V) || (val_range == _RANGE_S_N2_P2))
    {
        return (INT32S)(IDataTemp * verify_coef_range_s);
    }
    else
    {
        return (INT32S)(IDataTemp * verify_coef_range_l * 10);
    }
*/
        return (INT32S)(IDataTemp * verify_coef_range_s);
}

// 从一组电压数据中查找电平跳变或一个完整的脉冲
// 低电平：连续二个电压低于阀值（lowerThreshold）；同理可得高电平
U32 VolFlashJudge(U32 *voltData, U8 judgeType, U32 lowerThreshold, U32 upperThreshold)
{
    U8 normalizVolt[MAX_COLLECT_OBJ],i,tempVolt;
    U8 judgeState = STATE_START;
    U32 result = FALSE;
    
    //电压归一化处理
    for(i = 0; i < MAX_COLLECT_OBJ; i++)
    {
        if (voltData[i] < lowerThreshold)
        {
            normalizVolt[i] = VOLTLOW;
        }
        else if (voltData[i] > upperThreshold)
        {
            normalizVolt[i] = VOLTHIGH;
        }
        else
        {
            normalizVolt[i] = VOLTMID;       //异常数据
        }
    }
    
    i = 1;
    while((i < MAX_COLLECT_OBJ) && (result == FALSE))
    {
        switch(judgeState)
        {
            case STATE_START  :
                while (normalizVolt[i] == VOLTMID)    //去除异常数据
                {
                    i = i + 2;
                    if (i >= MAX_COLLECT_OBJ)
                    {
                        break;
                    }
                }
                //是否有连续2个同样的电压
                if ((normalizVolt[i-1] ^ normalizVolt[i]) == 0)
                {
                    judgeState = STATE_LEVEL_X;   
                }
                i = i + 1;
                break;
                
            case STATE_LEVEL_X :
                //去除异常数据
                tempVolt = normalizVolt[i-1];
                while(normalizVolt[i] == VOLTMID)
                {
                    i = i + 1;
                    if (i >= MAX_COLLECT_OBJ)
                    {
                        break;
                    }
                }
                //是否发生跳变
                if ((tempVolt ^ normalizVolt[i]) == 1)
                {
                    judgeState = STATE_LEVEL_XTOY;
                }
                
                i = i + 1;                
                break;
                
            case STATE_LEVEL_XTOY:
                //是否有连续2个同样的电压
                if ((normalizVolt[i-1] ^ normalizVolt[i]) == 0)
                {
                    judgeState = STATE_LEVEL_Y; 
                }
                else
                {
                    judgeState = STATE_START;
                }
                i = i + 1;    
                break;
                
            case STATE_LEVEL_Y :
                if (judgeType == GETHOP)
                {
                    result = TRUE;
                }
                else  
                {
                    //去除异常数据
                    tempVolt = normalizVolt[i-1];
                    while(normalizVolt[i] == VOLTMID)
                    {
                        i = i + 1;
                        if (i > MAX_COLLECT_OBJ)
                        {
                            break;
                        }
                    }
                    //是否发生跳变
                    if ((tempVolt ^ normalizVolt[i]) == 1)
                    {
                        if(judgeType == GETSIMPLEPLUSE)
                        {
                            result = TRUE;
                        }
                        else
                        {
                            judgeState = STATE_LEVEL_YTOX;
                        }
                    }
                    i = i + 1;
                }
                break;
                
            case STATE_LEVEL_YTOX:
                //是否有连续2个同样的电压
                if ((normalizVolt[i-1] ^ normalizVolt[i]) == 0)
                {
                    result = TRUE;
                }
                else
                {
                    judgeState = STATE_START;
                    i = i + 1;
                }    
                break;

            default:
                judgeState = STATE_START;
                break;                
        }
    }
    return result;
}



