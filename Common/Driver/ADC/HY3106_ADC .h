#ifndef _NEW_ADC_H_
#define _NEW_ADC_H_

// max186
#define     ADC_GPIO_BASE       AT91C_BASE_PIOC
#define     ADC_GPIO_ID         AT91C_ID_PIOC
#define     ADC_GPIO_CS         AT91C_PIO_PC13
#define     ADC_GPIO_SPCK       AT91C_PIO_PC6
#define     ADC_GPIO_MOSI       AT91C_PIO_PC23
#define     ADC_GPIO_MISO       AT91C_PIO_PC11
#define     ADC_GPIO_RANG       AT91C_PIO_PC9


#define     _CSAD_0             (ADC_GPIO_BASE->PIO_CODR = ADC_GPIO_CS)
#define     _CSAD_1             (ADC_GPIO_BASE->PIO_SODR = ADC_GPIO_CS)

#define     _SCLK_0             (ADC_GPIO_BASE->PIO_CODR = ADC_GPIO_SPCK)
#define     _SCLK_1             (ADC_GPIO_BASE->PIO_SODR = ADC_GPIO_SPCK)

#define     _SDOUT_0            (ADC_GPIO_BASE->PIO_CODR = ADC_GPIO_MOSI)
#define     _SDOUT_1            (ADC_GPIO_BASE->PIO_SODR = ADC_GPIO_MOSI)

#define     _SDIN               ((ADC_GPIO_BASE->PIO_PDSR & ADC_GPIO_MISO) == ADC_GPIO_MISO)

// range
#define     _RANGE_LARGE()      (ADC_GPIO_BASE->PIO_SODR = ADC_GPIO_RANG)   // 10:1
#define     _RANGE_SMALL()      (ADC_GPIO_BASE->PIO_CODR = ADC_GPIO_RANG)   //  1:1

#define     _RANGE_S_0_4V         1 // 0~4V
#define     _RANGE_L_0_40V        2 // 0 ~40v
#define     _RANGE_S_N2_P2        3 // +/-2V
#define     _RANGE_L_N20_P20      4 // +/- 20V

#define     _UNIPOLAR           1
#define     _BIPOLAR            0  //UNIPOLAR:0x8e;BIPOLAR:0x86

#define     RANGE_S             0  // _RANGE_0_4V, _RANGE_N2_P2
#define     RANGE_L             1  // _RANGE_N2_P2, _RANGE_N20_P20

#define AD_CHANNEL0   0
#define AD_CHANNEL1   4
#define AD_CHANNEL2   1
#define AD_CHANNEL3   5
#define AD_CHANNEL4   2
#define AD_CHANNEL5   6
#define AD_CHANNEL6   3
#define AD_CHANNEL7   7

void ADC_init(void);
INT32S AD_value_no_verify(INT32U val_range, INT32U channel);
void AD_verify_coef(INT32U input_val_mv, INT32U config, INT32U channel);
INT32S AD_cal_value(INT32U val_range, INT32U channel);
INT32U ADConvert(INT32U ChanelSel);
extern INT32U AD_MeasureAutoRange(INT32U VoltMax);
extern INT32U ADC_Test(void);

extern float  verify_coef_gain1;
extern float  verify_coef_gain10;

#endif
