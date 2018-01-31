

/*******************************************************************************
    Initfile.c
    Read the initializtion file, Process the lines, and get the initializtion setting. and init the usart and AD calibration.

    Copyright(C) 2012, Honeywell Integrated Technology (China) Co.,Ltd.
    Security FCT team
    All rights reserved.

    IDE:    IAR EWARM V6.4
    ICE:    J-Link
    BOARD:  Merak Main board

*******************************************************************************/

#include "includes.h"

//#define UPDATE_INITFILE  //zjm
#define REDUCE_TIME_COST
#define OS_MCK 100000000

#define SET_DUMMY   0xff

U8 InitArray[CH_INITFILE_MAX] = 
"COM,BAUD,STOP,VERIFY\r\n"
"DUT_USART1,115200,1,NONE\r\n"    //和DUT交互
"AUX_USART2,2400,1,US_EVEN\r\n"    //通常用于RS485测试
//"AUX_USART2,115200,1,NONE\r\n"    //
"MERAK_USART0,115200,1,NONE\r\n"  //MERAK 通常用于RS485测试
"AD CALIBRATE S,AD CALIBRATE M,CALIBRATE L,\r\n"
"1.0000000,1.0000000,1.0000000,\r\n"
//"RLY_SUM,RLY_ADMODE,RLY_CMMODE,\r\n"
//"2,1,2,\r\n"
"MAC ADDRESS,IP ADDRESS,SUBNET MASK,GATEWAY ADDR\r\n"
"00-1F-55-01-02-03,192.168.1.100,255.255.255.0,192.168.1.1\r\n"
;
USART_CONFIG DBGU_COMM_setting = {DBGU_COMM_PORT,  US_RS232, CHRL_8, US_NONE, STOP_1, 115200}; //调试
USART_CONFIG MerakCOMM_setting = {MERAK_COMM_PORT, US_RS485, CHRL_8, US_NONE, STOP_1, 115200};  //MERAK
USART_CONFIG DutCOMM_setting   = {DUT_COMM_PORT,   US_RS232, CHRL_8, US_NONE, STOP_1, 115200}; //和DUT交互
USART_CONFIG AuxCOMM_setting   = {AUX_COMM_PORT,   US_RS232, CHRL_8, US_EVEN, STOP_1, 2400}; //通常用于测试

float  verify_coef_range_s = 1.0000000;// 无分压矫正系数
float  verify_coef_range_m = 1.0000000;// 10:1 分压矫正系数
float  verify_coef_range_l = 1.0000000;// 100:1 分压矫正系数

typedef void (*SETTING_FUNC)(U8 * initStr);

static BOOL GetString(U8 * line, U8 * str)
{
	U8 i;

    memset(str, 0, CH_INITSTR_MAX);  // Clear the string.
    
	for(i=0;i<CH_PERSTR_MAX;i++)
	{
		if(line[i] == ',' || line[i] == '\r')  // Look for a comma or a LF char.
		{
			strncpy((char * )str, (char * )line, i);	//Got a string.
			break;
		}
	}
	return(i + 1);
}

static BOOL GetComParam(U8 * initStr, USART_CONFIG * setting)
{
    U8 pos;
	U8 str[CH_INITSTR_MAX];
	
	if((pos = GetString(initStr, str)) < CH_PERSTR_MAX)	    //Get a string. 
	{
		initStr += pos;   // Skip this item, it is a title.
	}
	if((pos = GetString(initStr, str)) < CH_PERSTR_MAX)	    //Get a string. 
	{
    	setting->baudrate = (INT32U)strtod((char * )str, NULL);    // Get the baud.
		initStr += pos;
	}
	if((pos = GetString(initStr, str)) < CH_PERSTR_MAX)	    //Get a string. 
	{
    	if(strcmp("2", (char * )str) == 0)      // Get the stop bit.
    	{
    	    setting->stopbit = STOP_2;
    	}
    	else if(strcmp("1", (char * )str) == 0)
    	{
    	    setting->stopbit = STOP_1;
    	}
    	else
    	{
    	    setting->stopbit = SET_DUMMY;
    	}
		initStr += pos;
	}
	if((GetString(initStr, str)) < CH_PERSTR_MAX)	    //Get a string. 
	{
    	if(strcmp("EVEN", (char * )str) == 0)      // Get the parity bit.
    	{
    	    setting->parity = US_EVEN;
    	}
    	else if(strcmp("ODD", (char * )str) == 0)
    	{
    	    setting->parity = US_ODD;
    	}
    	else if(strcmp("NO", (char * )str) == 0)
    	{
    	    setting->parity = US_NONE;
    	}
    	else                //"NONE"
    	{
    	    setting->parity = SET_DUMMY;
    	}
	}

	return(TRUE);
}

static void CommSetting(USART_CONFIG * pSetting, U8 * initStr)
{
    USART_CONFIG new_setting;
    
    GetComParam(initStr, &new_setting);
    
    if(new_setting.baudrate)
    {
        pSetting->baudrate =  new_setting.baudrate;
    }
    if(new_setting.stopbit != SET_DUMMY)
    {
        pSetting->stopbit =  new_setting.stopbit;
    }
    if(new_setting.parity != SET_DUMMY)
    {
        pSetting->parity =  new_setting.parity;
    }
}

static void DutComSetting(U8 * initStr)
{
    CommSetting((USART_CONFIG * )&DutCOMM_setting.usartport, initStr);
}

static void AuxComSetting(U8 * initStr)
{
    CommSetting((USART_CONFIG * )&AuxCOMM_setting.usartport, initStr);
}

static void MerakComSetting(U8 * initStr)
{
    CommSetting((USART_CONFIG * )&MerakCOMM_setting.usartport, initStr);
}

static void GetAdParam(U8 * initStr, float * cal_s, float * cal_m, float * cal_l)
{
    U8 pos;
	U8 str[CH_INITSTR_MAX];
	
	if((pos = GetString(initStr, str)) < CH_PERSTR_MAX)	    //Get a string. 
	{
		* cal_s = strtof((char * )str, NULL);   // get the cal data from each string.
	}
	if((GetString(initStr+pos, str)) < CH_PERSTR_MAX)	    //Get a string. 
	{
		* cal_m = strtof((char * )str, NULL);   // get the cal data from each string.
	}
	if((GetString(initStr+pos, str)) < CH_PERSTR_MAX)	    //Get a string. 
	{
		* cal_l = strtof((char * )str, NULL);   // get the cal data from each string.
	}
}

static void ADCalSetting(U8 * initStr)
{
    float cal_s, cal_m, cal_l;
    
    GetAdParam(initStr, (float * )&cal_s, (float * )&cal_m, (float * )&cal_l);
    
    if(cal_s)
    {
        verify_coef_range_s = cal_s;
    }
    if(cal_m)
    {
        verify_coef_range_m = cal_m;
    }
    if(cal_l)
    {
        verify_coef_range_l = cal_l;
    }
}

static void IP_Setting(U8 * initStr)
{
}

static void MenuLine(U8 * initStr)
{
}

const SETTING_FUNC SettingFunc[] = 
{
    MenuLine,
	DutComSetting,
	AuxComSetting,
	MerakComSetting,
	MenuLine,
	ADCalSetting,
	MenuLine,
	IP_Setting,
};

static void DutComInit(void)
{
    UsartInit(DutCOMM_setting, OS_MCK);
    
    OS_ARM_InstallISRHandler(DUT_COMM_ID, &US1_ISR_Handler);  /* OS UART interrupt handler vector */
	OS_ARM_ISRSetPrio(DUT_COMM_ID, 0);              /* Level sensitive, selected priority. */
	OS_ARM_EnableISR(DUT_COMM_ID);                             /* Enable OS usart interrupts       */
	
    UsartRecvStart(DUT_COMM_PORT);
}

static void AuxComInit(void)
{
    UsartInit(AuxCOMM_setting, OS_MCK);
    
    OS_ARM_InstallISRHandler(AUX_COMM_ID, &US2_ISR_Handler);  /* OS UART interrupt handler vector */
	OS_ARM_ISRSetPrio(AUX_COMM_ID, 0);              /* Level sensitive, selected priority. */
	OS_ARM_EnableISR(AUX_COMM_ID);                             /* Enable OS usart interrupts       */
	
    UsartRecvStart(AUX_COMM_PORT);
}

static void MerakComInit(void)
{
    UsartInit(MerakCOMM_setting, OS_MCK);
    
    OS_ARM_InstallISRHandler(MERAK_COMM_ID, &US0_ISR_Handler);  /* OS UART interrupt handler vector */
	OS_ARM_ISRSetPrio(MERAK_COMM_ID, 0);              /* Level sensitive, selected priority. */
	OS_ARM_EnableISR(MERAK_COMM_ID);                             /* Enable OS usart interrupts       */
	
    UsartRecvStart(MERAK_COMM_PORT);
}

static void DbgComInit(void)
{
    UsartInit(DBGU_COMM_setting, OS_MCK);
    UsartRecvStart(DBGU_COMM_PORT);    
}

static void UART_Init(void)
{
    DutComInit();
    AuxComInit();
    MerakComInit();
    DbgComInit();
}


static void FIX_Init(void)
{
    UART_Init();
	MERAK_ResetALL();
    //i2c_init();
	i2c_ADC_init(verify_coef_range_s, verify_coef_range_m, verify_coef_range_l);
    //IP_Ping_Init();
    HMI_OnRunLed();
    RLY_SetCommonMode(1);
}


void INITFILE_Proc(void)
{
	U8 i = 0;
	U8 * line;
    static U8 * ptr;
	
	FS_FILE *fb;
    
#ifdef UPDATE_INITFILE
	if(fb = FS_FOpen(INIT_FILE,"w")) // Update the file.
	{
	    FS_FWrite(InitArray, 1, strlen((char * )InitArray), fb);
	    FS_SetEndOfFile(fb);
        FS_FClose(fb);
    }
#endif

	if(fb = FS_FOpen(INIT_FILE,"r")) // Read the file.
	{
        memset(InitArray, 0, CH_INITFILE_MAX);  // Clear the file.
		FS_FRead(InitArray,1,CH_INITFILE_MAX,fb);
		FS_FClose(fb);
	}

    ptr = InitArray;
    
	while((line = (U8 * )strtok((char * )ptr, "\n")) != NULL)
	{
	    ptr += strlen((char * )line) + 1;
    	SettingFunc[i++](line);
	}

	FIX_Init();

    LCD_Clear(LCD_ALL_LINE);
    LCD_DisplayALine(LCD_LINE1, "FCT Start...");
    Volt_Calibration();  // zjm mcm
}


