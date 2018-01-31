

#include "includes.h"

#define CALIBRATE_RANGE_12V     12000
#define CALIBRATE_RANGE_3V      3000

static void Update_CalibrationValue(void)
{
    char title[] = "AD CALIBRATE S,AD CALIBRATE M,AD CALIBRATE L,\r\n";
    char str[20];
    U32 fraction_s, fraction_m, fraction_l;
    char * p;
    
	FS_FILE *fb;
    
	if(fb = FS_FOpen(INIT_FILE,"r")) // Read the file.
	{
        memset(InitArray, 0, CH_INITFILE_MAX);  // Clear the file.
		FS_FRead(InitArray,1,CH_INITFILE_MAX,fb);
		FS_FClose(fb);
	}
	
    p = strstr((char * )InitArray, title);

    if(p)
    {
        fraction_s = (U32)((verify_coef_range_s-(int)verify_coef_range_s)*10000000);
        fraction_m = (U32)((verify_coef_range_m-(int)verify_coef_range_m)*10000000);
        fraction_l = (U32)((verify_coef_range_l-(int)verify_coef_range_l)*10000000);
        
        sprintf(str, "%1d.%07d,%1d.%07d,%1d.%07d", 
                (int)verify_coef_range_s, fraction_s, 
                (int)verify_coef_range_m, fraction_m,
                (int)verify_coef_range_l, fraction_l);
        strncpy(p + strlen(title), str, strlen(str));
        
    	if(fb = FS_FOpen(INIT_FILE,"w")) // Update the file.
    	{
    	    FS_FWrite(InitArray, 1, strlen((char * )InitArray), fb);
    	    FS_SetEndOfFile(fb);
            FS_FClose(fb);
        }
    }
}


static void Display_Voltage(U32 volt)
{
    U8 str[10];

    sprintf((char *)str, "%2d.%03dV", volt/1000, volt%1000);
	LCD_DisplayALine(LCD_LINE2, str);
}

static U8 Calibrate_Proc(U32 volt_range)
{
    U8 res = FALSE;
	U32 volt;

    if(volt_range == CALIBRATE_RANGE_12V)
    {
        volt = (U32)AD_MeasureAutoRange(volt_range);
        Display_Voltage(volt);

        if(volt > 9600 && volt < 14400)
        {
            verify_coef_range_l *= (float)(12000.0/volt);
            res = TRUE;
            
            volt = (U32)AD_MeasureAutoRange(volt_range);
            Display_Voltage(volt);
            
	        LCD_DisplayALine(LCD_LINE3, (U8 *)"Voltage Calibration OK!");
	        OS_Delay(1000);
        }
        else
        {
	        LCD_DisplayALine(LCD_LINE3, (U8 *)"Voltage Value is ERR!");
	        LCD_Clear(LCD_LINE4);
	        OS_Delay(1000);
        }
    }
    
	if(volt_range == CALIBRATE_RANGE_3V)
	{
        volt = (U32)AD_MeasureAutoRange(volt_range-1000);
        Display_Voltage(volt);

        if(volt > 2400 && volt < 3600)
        {
            verify_coef_range_s *= (float)(3000.0/volt);
            res |= TRUE;
        
            volt = (U32)AD_MeasureAutoRange(volt_range-1000);
            Display_Voltage(volt);
            
	        LCD_DisplayALine(LCD_LINE3, (U8 *)"Voltage Calibration OK!");
	        OS_Delay(1000);
        }
        else
        {
	        LCD_DisplayALine(LCD_LINE3, (U8 *)"Voltage Value is ERR!");
	        LCD_Clear(LCD_LINE4);
	        OS_Delay(1000);
        }
    }
    
    return(res);
}

void Volt_Calibration(void)
{
    U8 res = FALSE;
    
	if(HMI_PressYesKey() == FALSE)
	{
		return;
	}
	if(HMI_PressNoKey() == FALSE)
	{
		return;
	}
	
	HMI_PassBuzz();
	LCD_Clear(LCD_LINE1);
	LCD_DisplayALine(LCD_LINE2, (U8 *)"Voltage Calibration...");
	LCD_DisplayALine(LCD_LINE3, (U8 *)"Please release the key");
	LCD_Clear(LCD_LINE4);

	while(HMI_PressYesKey() == TRUE)
	{}
	while(HMI_PressNoKey() == TRUE)
	{}
	
	HMI_PassBuzz();
	LCD_DisplayALine(LCD_LINE1, (U8 *)"Voltage Calibration 12V");
	LCD_DisplayALine(LCD_LINE2, (U8 *)"Please connect DC 12V");
	LCD_DisplayALine(LCD_LINE3, (U8 *)"Press Yes key to start");
	LCD_DisplayALine(LCD_LINE4, (U8 *)"Press No key to exit");
	
	while(1)
	{
	    if(HMI_PressYesKey() == TRUE)
	    {
	        res = Calibrate_Proc(CALIBRATE_RANGE_12V);
            break;
	    }
	    if(HMI_PressNoKey() == TRUE)
	    {
            break;
	    }
	}
	
	HMI_PassBuzz();
	LCD_DisplayALine(LCD_LINE1, (U8 *)"Voltage Calibration 3V");
	LCD_DisplayALine(LCD_LINE2, (U8 *)"Please connect DC 3V");
	LCD_DisplayALine(LCD_LINE3, (U8 *)"Press Yes key to start");
	LCD_DisplayALine(LCD_LINE4, (U8 *)"Press No key to exit");
	
	while(1)
	{
	    if(HMI_PressYesKey() == TRUE)
	    {
	        res |= Calibrate_Proc(CALIBRATE_RANGE_3V);
            break;
	    }
	    if(HMI_PressNoKey() == TRUE)
	    {
            break;
	    }
	}
	
	if(res == FALSE)
	{
	    LCD_DisplayALine(LCD_LINE3, (U8 *)"Voltage Calibration fail");
    }
	else
	{
        Update_CalibrationValue();
	    LCD_DisplayALine(LCD_LINE3, (U8 *)"Voltage Calibration OK!!");
    }
	LCD_Clear(LCD_LINE1);
	LCD_Clear(LCD_LINE2);
	LCD_Clear(LCD_LINE4);
	HMI_PassBuzz();
    
	OS_Delay(2000);
}



