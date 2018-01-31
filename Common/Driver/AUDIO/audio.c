
/*******************************************************************************
    Audio.c
    Communication with Merak Audio board throgh I2C interface

    Copyright(C) 2012, Honeywell Integrated Technology (China) Co.,Ltd.
    Security FCT team
    All rights reserved.

    IDE:    IAR EWARM V6.4
    ICE:    J-Link
    BOARD:  Merak Main board

*******************************************************************************/
#include "includes.h"

#define AUDNUM_ONLY_1 	    1

#define AUDIO_FREQ_TOLERANCE 3
#define BUZZ_FREQ_TOLERANCE 50
#define TEST_AUDIO_1KHZ     1000
#define TEST_AUDIO_300MV    300
#define DISC_LIMIT          3
#define SNR_LIMIT           40

#define AUDFUNC_ENTER_MODE  0x51 
#define AUDFUNC_AUD_GEN     0x52 
#define AUDFUNC_CONFIG_SET  0x53 
#define AUDFUNC_READ_SIGNAL 0x54 

#define AUDREG_ENTER_MODE   0
#define AUDREG_CFG_DTMF     1
#define AUDREG_CFG_TONE     2

#define AUDREG_GEN_DTMF     1
#define AUDREG_GEN_TONE     3
#define AUDREG_GEN_IDLE     4

#define AUDREG_DECODE_SIGN  0

const U8 ModeDTMF[] = "01";
const U8 ModeSimp[] = "04";

const U8 GenIdle[] = "00";

static BOOL Audio_WriteCmd(U8 func, U8 reg, U8 * write_str)
{
    return(MERAK_WriteCmd("AUD", AUDNUM_ONLY_1, func, reg, write_str));
}

static BOOL Audio_ReadCmd(U8 func, U8 reg, U8 * read_str)
{
    return(MERAK_ReadCmd("AUD", AUDNUM_ONLY_1, func, reg, read_str));
}

static BOOL Audio_SetMode(U8 * mode_str)
{
    return(Audio_WriteCmd(AUDFUNC_ENTER_MODE, AUDREG_ENTER_MODE, mode_str));
}

static BOOL Audio_ConfigTone(U16 amp_min)
{
    U8 amp_str[3];
    
    Hex2Str(amp_str, amp_min);
    return(Audio_WriteCmd(AUDFUNC_CONFIG_SET, AUDREG_CFG_TONE, amp_str));
}

static BOOL Audio_ConfigDTMF(U16 sig_time, U16 spac_time)
{
    U8 time_str[9];
    
    sprintf((char * )time_str, "%d,%d", sig_time, spac_time);

    return(Audio_WriteCmd(AUDFUNC_CONFIG_SET, AUDREG_CFG_DTMF, time_str));
}


/******************************************************************************
    Routine Name    : Audio_GenSimpTone
    Form            : static BOOL Audio_GenerateSinTone(U16 freq, U16 amp)
    Parameters      : freq, amp
    Return value    : TRUE/FALSE
    Description     : Generate the Audio Tone signal .
******************************************************************************/
static BOOL Audio_GenSimpTone(U16 freq, U16 amp)
{
    U8 tone_str[9];
    
    sprintf((char * )tone_str, "%d,%d", freq, amp);

    return(Audio_WriteCmd(AUDFUNC_AUD_GEN, AUDREG_GEN_TONE, tone_str));
}

static BOOL Audio_GenDTMF(U8 * DTMF_numb)
{
    return(Audio_WriteCmd(AUDFUNC_AUD_GEN, AUDREG_GEN_DTMF, DTMF_numb));
}

/******************************************************************************
    Routine Name    : Audio_StopSignal
    Form            : static void Audio_StopSignal()
    Parameters      : void
    Return value    : TRUE/FALSE
    Description     : End the signal transmitting .
******************************************************************************/
static BOOL Audio_StopSignal(void)
{
    return(Audio_WriteCmd(AUDFUNC_AUD_GEN, AUDREG_GEN_IDLE, (U8 * )GenIdle));
}

/******************************************************************************
    Routine Name    : Audio_DecSimpTone
    Form            : static U8 Audio_DecSimpTone(U16 * freq, U16 * amp)
    Parameters      : freq, amp, disc, sn
    Return value    : TRUE/FALSE
    Description     : Decode the Audio signal, Get the value of the audio signal.
******************************************************************************/
static BOOL Audio_DecSimpTone(U16 * freq, U16 * amp)
{
    U8 i;
    U8 DecStr[10];
    
    if(Audio_ReadCmd(AUDFUNC_READ_SIGNAL, AUDREG_DECODE_SIGN, DecStr))
    {
        for(i = 0; i < 10; i++)
        {
            if(DecStr[i] == ',')
            {
                break;
            }
        }
        if(i < 10)
        {
            * freq = (U16)strtod((char * )DecStr, NULL);
            * amp = (U16)strtod((char * )(DecStr+i+1), NULL);
            return(TRUE);
        }
    }
    return(FALSE);
}

/******************************************************************************
    Routine Name    : Audio_TestSimpTone
    Form            : static BOOL Audio_TestSimpTone(U16 exp_freq, U8 freq_tol, U16 amp_lower, U16 amp_upper)
    Parameters      : exp_freq, amp_lower, amp_upper
    Return value    : PASS/FAIL
    Description     : Test the Audio signal, Compare signal value with limit, Get the PASS/FAIL conclusion.
******************************************************************************/
static BOOL Audio_TestSimpTone(U16 exp_freq, U8 freq_tol, U16 amp_lower, U16 amp_upper)
{
    volatile U8 i;
    BOOL ret = PASS; //返回值
    U16 rx_freq;    //信号的频率
    U16 rx_amp;     //信号的幅值
    OS_Delay(2000);
    for(i=0;i<5;i++)
    {
    	OS_Delay(500);
    	
        rx_freq = 0;
        rx_amp = 0;

        Audio_DecSimpTone(&rx_freq, &rx_amp);//读取信号的频率和
        Dprintf("Audio%dHz,%dmv\r\n", rx_freq, rx_amp);
        
        //检测读取的频率和幅值是否在范围内
        if((rx_amp < amp_upper)&&(rx_amp > amp_lower))//检测幅值
        {
            if((rx_freq <=(exp_freq+freq_tol))&&(rx_freq >=(exp_freq-freq_tol)))//检测频率
            {
                break;
            }
        }
        
        /*if(rx_freq >= exp_freq)
        {
            if((rx_freq - exp_freq) > freq_tol)
            {
                ret = FAIL;  
            }
        }
        if(rx_freq < exp_freq)
        {
            if((exp_freq - rx_freq) > freq_tol)
            {
                ret = FAIL;  
            }
        }
        
        if(rx_amp > amp_upper)
        {
            ret = FAIL;  
        }
        if(rx_amp < amp_lower)
        {
            ret = FAIL;  
        }*/
    }
    //如果一次没通过则PassCount仍为0,则判定fail，只要通过一次则判定pass
    if(i == 5)
    {
        ret = FAIL;
    }
    else
    {
        ret = PASS;
    }
    return(ret);
}


/******************************************************************************
    Routine Name    : Audio_TestBuzz
    Form            : BOOL Audio_TestBuzz(U16 tx_amp, U16 amp_lower, U16 amp_upper)
    Parameters      : buzz_freq, amp_lower, amp_upper
    Return value    : PASS/FAIL
    Description     : Testing procedure for Buzzer. 
******************************************************************************/
BOOL Audio_TestBuzz(U16 buzz_freq, U16 amp_lower, U16 amp_upper)
{
    Audio_SetMode((U8 * )ModeSimp);
    return(Audio_TestSimpTone(buzz_freq, BUZZ_FREQ_TOLERANCE, amp_lower, amp_upper));
}

/******************************************************************************
    Routine Name    : Audio_LoopTest
    Form            : BOOL Audio_LoopTest(U16 tx_amp, U16 amp_lower, U16 amp_upper)
    Parameters      : tx_amp, amp_lower, amp_upper
    Return value    : PASS/FAIL
    Description     : Testing procedure for Audio. 
******************************************************************************/
BOOL Audio_LoopTest(U16 tx_amp, U16 amp_lower, U16 amp_upper)
{
    BOOL ret;

    Audio_SetMode((U8 * )ModeSimp);
    Audio_GenSimpTone(TEST_AUDIO_1KHZ, tx_amp);
    Dprintf("Test%dHz, %dmv\r\n", TEST_AUDIO_1KHZ, tx_amp);
    ret = Audio_TestSimpTone(TEST_AUDIO_1KHZ, AUDIO_FREQ_TOLERANCE, amp_lower, amp_upper);
    Audio_StopSignal();
   
    return(ret);
}

/******************************************************************************
    Routine Name    : Audio_GenTone
    Form            : BOOL Audio_GenTone(U16 tx_fre, U16 tx_amp)
    Parameters      : tx_fre, tx_amp
    Return value    : PASS/FAIL
    Description     : Generate the Audio of specified frequency and voltage. 
******************************************************************************/
BOOL Audio_GenTone(U16 tx_fre, U16 tx_amp)
{
    BOOL ret;

    Audio_SetMode((U8 * )ModeSimp);
    ret = Audio_GenSimpTone(tx_fre, tx_amp);
    Dprintf("Gen %dHz, %dmv\r\n", tx_fre, tx_amp);
   
    return(ret);
}

/******************************************************************************
    Routine Name    : Audio_StopTone
    Form            : BOOL Audio_GenTone(U16 tx_fre, U16 tx_amp)
    Parameters      : tx_fre, tx_amp
    Return value    : PASS/FAIL
    Description     : Generate the Audio of specified frequency and voltage. 
******************************************************************************/
BOOL Audio_StopTone(void)
{
    BOOL ret;

    ret = Audio_StopSignal();
    Dprintf("Stop single tone.\r\n");
   
    return(ret);
}

/******************************************************************************
    Routine Name    : Audio_CompToneAmp
    Form            : BOOL Audio_CompToneAmp(U16 tx_amp, U16 amp_lower, U16 amp_upper)
    Parameters      : tx_amp, amp_lower, amp_upper
    Return value    : PASS/FAIL
    Description     : Testing procedure for Audio. 
******************************************************************************/
BOOL Audio_CompToneAmp(U16 amp_lower, U16 amp_upper)
{
    U16 freq;
    U16 amp;

    if(Audio_DecSimpTone(&freq, &amp))
    {
        Dprintf("GetAmp:%dmv, lower:%dmv, upper:%dmv\r\n", amp, amp_lower, amp_upper);
        if(amp > amp_lower && amp < amp_upper)
        {
            return(TRUE);
        }
    }
    return(FALSE);
}

/******************************************************************************
    Routine Name    : Audio_GenTone
    Form            : BOOL Audio_DecTone(U16 tx_amp, U16 amp_lower, U16 amp_upper)
    Parameters      : tx_amp, amp_lower, amp_upper
    Return value    : PASS/FAIL
    Description     : Testing procedure for Audio. 
******************************************************************************/
BOOL Audio_DecToneFreq(U16 freq_lower, U16 freq_upper)
{
    U16 freq;
    U16 amp;

    if(Audio_DecSimpTone(&freq, &amp))
    {
        Dprintf("DecFreq:%dHz, lower:%dHz, upper:%dHz\r\n", freq, freq_lower, freq_upper);
        if(freq > freq_lower && freq < freq_upper)
        {
            return(TRUE);
        }
    }
    return(FALSE);
}

BOOL Audio_Open(void)
{
    Audio_SetMode((U8 * )ModeSimp);
    Audio_GenSimpTone(TEST_AUDIO_1KHZ, 200);
    return 1;
}
BOOL Aduio_Close(void)
{
  Audio_StopSignal();
   return 1;
}
