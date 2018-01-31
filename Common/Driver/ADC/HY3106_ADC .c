/******************************************************************************
    HY3016_ADC.c
    API functions

    Copyright(C) 2016, Honeywell Integrated Technology (China) Co.,Ltd.
    Security FCT team
    All rights reserved.

    Document
    80TS8000333AC AD module functions

    History
    2016.5.10  ver.1.00    sun ke first release
******************************************************************************/

#include "includes.h"

//float  verify_coef_range_s;// 无分压矫正系数
//float  verify_coef_range_l; // 10:1 分压矫正系数

void ADC_init(void)
{
    volatile INT32U i;

    // init spi port
    AT91C_BASE_PMC->PMC_PCER    = 1 << ADC_GPIO_ID;

    ADC_GPIO_BASE->PIO_SODR     = ADC_GPIO_CS;
    ADC_GPIO_BASE->PIO_SODR     = ADC_GPIO_SPCK;
    ADC_GPIO_BASE->PIO_SODR     = ADC_GPIO_MOSI;
    ADC_GPIO_BASE->PIO_SODR     = ADC_GPIO_RANG;

    ADC_GPIO_BASE->PIO_PER      = ADC_GPIO_CS;
    ADC_GPIO_BASE->PIO_PER      = ADC_GPIO_SPCK;
    ADC_GPIO_BASE->PIO_PER      = ADC_GPIO_MOSI;
    ADC_GPIO_BASE->PIO_PER      = ADC_GPIO_MISO;
    ADC_GPIO_BASE->PIO_PER      = ADC_GPIO_RANG;

    ADC_GPIO_BASE->PIO_OER      = ADC_GPIO_CS;
    ADC_GPIO_BASE->PIO_OER      = ADC_GPIO_SPCK;
    ADC_GPIO_BASE->PIO_OER      = ADC_GPIO_MOSI;
    ADC_GPIO_BASE->PIO_ODR      = ADC_GPIO_MISO;       // input
    ADC_GPIO_BASE->PIO_OER      = ADC_GPIO_RANG;

    //test
    //end test
    // ad range relay
    _RANGE_LARGE();

    // init ad
    _CSAD_1;
    _SCLK_0;
    _SDOUT_0;

    // init verify coefficent
    //verify_coef_range_l = 1.0376815;//1.0094178;//1.0913140;
    //verify_coef_range_s = 1.0064044;
    OS_Delay(100);
}

//  一次采集的电压,低12bit 有效, 不分正负
static INT32U ADConvert(INT32U ChanelSel)
{
    INT32U          i;
    volatile INT32U j;
    INT32U          ReadData;

    _CSAD_0;
    _SCLK_0;

    ReadData  = 0;

    for(i=0;i<8;i++)
    {
        _SCLK_0;
        for(j=0;j<500;j++);
        if(ChanelSel&0x80)
            _SDOUT_1;
        else
            _SDOUT_0;
        ChanelSel <<= 1;
        for(j=0;j<500;j++);
        _SCLK_1;
        for(j=0;j<1000;j++);
    }
    _CSAD_1;

    _SCLK_0;
    _SDOUT_0;

    for(j=0;j<1000;j++);

    _CSAD_0;
    for(i=0;i<16;i++)
    {
        ReadData = ReadData << 1;
        _SCLK_1;                            // 下降沿数据有效
        for(j=0;j<500;j++);
        if(_SDIN)
            ReadData = ReadData | 0x00000001;
        else
            ReadData = ReadData & 0xfffffffe;

        for(j=0;j<500;j++);
        _SCLK_0;
        for(j=0;j<1000;j++);
    }

    ReadData >>= 3;
    ReadData &= 0x0FFF;
    /* if(ReadData & 0x00000800)
        ReadData |= 0xFFFFF000;*/

    _CSAD_1;
    return ReadData;
}

INT32S AD_value_no_verify(INT32U val_range, INT32U channel)
{
    INT32U i, j, control_byte;
    INT32S AD_DataTemp[16];
    INT32S IDataTemp;

    switch(val_range)
    {
        case _RANGE_S_0_4V:
            // unipolar, small range
            _RANGE_SMALL();
            control_byte = 0x8e | (channel <<4); // unipolar
            break;

        case _RANGE_L_0_40V:
            // unipolar, big range
            _RANGE_LARGE();
            control_byte = 0x8e| (channel <<4); // unipolar
            break;

        case _RANGE_S_N2_P2:
            // bipolar, small range
            _RANGE_SMALL();
            control_byte = 0x86| (channel <<4); // unipolar
            break;

        case _RANGE_L_N20_P20:
            // bipolar, big range
            _RANGE_LARGE();
            control_byte = 0x86| (channel <<4); // unipolar
            break;

        default:
            return 0;
    }

    // waiting for some time after change the relay status
    //Delay_ms(200);

    for(i=0;i<16;i++)
    {
        AD_DataTemp[i] = ADConvert(control_byte);
        if((val_range == _RANGE_S_N2_P2) || (val_range == _RANGE_L_N20_P20))
        {
            if(AD_DataTemp[i] & 0x800) // 负电压,高位补1
                AD_DataTemp[i] |= 0xFFFFF000;
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
    for(i=4;i<12;i++)
        IDataTemp += AD_DataTemp[i];

    IDataTemp = IDataTemp/8;

    if((val_range == _RANGE_S_0_4V) || (val_range == _RANGE_S_N2_P2))
    {
        return IDataTemp;
    }
    else
        return IDataTemp * 10;
}


// Voltage :  _RANGE_S_0_4V, _RANGE_S_N2_P2  RANGE_SMALL config
//_Voltage :  RANGE_0_40V, _RANGE_N20_P20 , RANGE_LARGE config
void AD_verify_coef(INT32U input_val_mv, INT32U config, INT32U channel) //
{
    INT32U ad_value;
    // cau k
    if(config == RANGE_S)
    {
        ad_value = (INT32U)AD_value_no_verify(_RANGE_S_0_4V, channel);
        verify_coef_range_s = (float)(input_val_mv ) / (float)(ad_value);
    }
    else
    {
        ad_value = (INT32U)AD_value_no_verify(_RANGE_L_0_40V, channel);
        verify_coef_range_l = (float)(input_val_mv) / (float)(ad_value);
    }

}


INT32S AD_cal_value(INT32U val_range, INT32U channel)
{
    switch(val_range)
    {
        case _RANGE_S_0_4V:
        case _RANGE_S_N2_P2:
            return (INT32S)(AD_value_no_verify(val_range, channel) * verify_coef_range_s);
            //break;

        case _RANGE_L_0_40V:
        case _RANGE_L_N20_P20:
            return (INT32S)(AD_value_no_verify(val_range, channel) * verify_coef_range_l);
        //break;

        default:
            return 0;
    }
}




INT32U AD_MeasureAutoRange(INT32U VoltMax)
{
    INT32U volt;
    float verify_coef_factor = 0;
    
	if(VoltMax > 4000)
	{
        volt = AD_cal_value(_RANGE_L_0_40V, AD_CHANNEL0);
	}
	else
	{
        volt = AD_cal_value(_RANGE_S_0_4V, AD_CHANNEL0);
	}
	
	return((INT32U)(volt ));
}

INT32U ADC_Test(void)
{  
    INT32U volt;
    
    ADC_init();  
    
    while(1)
    {
        volt = AD_cal_value(_RANGE_L_0_40V, AD_CHANNEL1);
        Dprintf("voltage is %d\n\r", volt);
        OS_Delay(300);     
    
    }      
}
