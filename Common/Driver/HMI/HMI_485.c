/*********************************************************************
*  
*   IAR PowerPac
*
*   (c) Copyright IAR Systems 2010.  All rights reserved.
*
**********************************************************************
----------------------------------------------------------------------
File    : HMI.c
Purpose : HMI for AT91SAM9260-EK and AT91SAM9XE-EK
--------  END-OF-HEADER  ---------------------------------------------
*/

#include "includes.h"

#define HMINUM_ONLY_1 	    1

#define HMIFUNC_SET_LED 	0x71
#define HMIFUNC_SET_BUZZ 	0x72
#define HMIFUNC_GET_KEY 	0x73

#define HMIREG_SET_BUZZ	    0

#define HMIREG_LED_RUN 	    1
#define HMIREG_LED_PASS 	2
#define HMIREG_LED_FAIL 	3

#define HMIREG_KEY_FUNC 	1
#define HMIREG_KEY_YES 	    2
#define HMIREG_KEY_NO 	    3

const U8 BuzzPassData[] = "01";
const U8 BuzzFailData[] = "02";
const U8 BuzzShortWarnData[] = "03";
const U8 BuzzLongWarnData[] = "04";
const U8 BuzzLongWarnDataOff[] = "05";

const U8 LedOffData[] = "00";
const U8 LedOnData[] =  "01";
const U8 LedFlashData[] = "02";

const U8 KeyActiveData[] = "01";
const U8 KeyIdleData[] = "00";
extern U8 McuVerStr[20];
BOOL HMI_PassBuzz(void);
BOOL HMI_OnRunLed(void);
BOOL HMI_FlashRunLed(void);
BOOL HMI_PressYesKey(void);
BOOL HMI_PressNoKey(void);
BOOL HMI_PressFuncKey(void);
void HMI_ShowPass(void);
void HMI_ShowFail(void);
void HMI_Init(void);

BOOL HMI_WriteCmd(U8 func, U8 reg, U8 * WriteStr)
{
    return(MERAK_WriteCmd("HMI", HMINUM_ONLY_1, func, reg, WriteStr));
}

BOOL HMI_ReadCmd(U8 func, U8 reg, U8 * ReadStr)
{
    return(MERAK_ReadCmd("HMI", HMINUM_ONLY_1, func, reg, ReadStr));
}

BOOL HMI_ShortWarnBuzz(void)
{
    return(HMI_WriteCmd(HMIFUNC_SET_BUZZ, HMIREG_SET_BUZZ, (U8 * )BuzzShortWarnData));
}

BOOL HMI_LongWarnBuzz(void)
{
    return(HMI_WriteCmd(HMIFUNC_SET_BUZZ, HMIREG_SET_BUZZ, (U8 * )BuzzLongWarnData));
}

BOOL HMI_LongWarnBuzzOff(void)
{
    return(HMI_WriteCmd(HMIFUNC_SET_BUZZ, HMIREG_SET_BUZZ, (U8 * )BuzzLongWarnDataOff));
}

BOOL HMI_PassBuzz(void)
{
    return(HMI_WriteCmd(HMIFUNC_SET_BUZZ, HMIREG_SET_BUZZ, (U8 * )BuzzPassData));
}

BOOL HMI_FailBuzz(void)
{
    return(HMI_WriteCmd(HMIFUNC_SET_BUZZ, HMIREG_SET_BUZZ, (U8 * )BuzzFailData));
}

BOOL HMI_OnPassLed(void)
{
    return(HMI_WriteCmd(HMIFUNC_SET_LED, HMIREG_LED_PASS, (U8 * )LedOnData));
}

BOOL HMI_OffPassLed(void)
{
    return(HMI_WriteCmd(HMIFUNC_SET_LED, HMIREG_LED_PASS, (U8 * )LedOffData));
}

BOOL HMI_OnFailLed(void)
{
    return(HMI_WriteCmd(HMIFUNC_SET_LED, HMIREG_LED_FAIL, (U8 * )LedOnData));
}

BOOL HMI_OffFailLed(void)
{
    return(HMI_WriteCmd(HMIFUNC_SET_LED, HMIREG_LED_FAIL, (U8 * )LedOffData));
}

BOOL HMI_OnRunLed(void)
{
    return(HMI_WriteCmd(HMIFUNC_SET_LED, HMIREG_LED_RUN, (U8 * )LedOnData));
}
BOOL HMI_OffRunLed(void)
{
    return(HMI_WriteCmd(HMIFUNC_SET_LED, HMIREG_LED_RUN, (U8 * )LedOffData));
}
BOOL HMI_FlashRunLed(void)
{
    return(HMI_WriteCmd(HMIFUNC_SET_LED, HMIREG_LED_RUN, (U8 * )LedFlashData));
}

BOOL HMI_KeyPress(U8 key_num)
{
    U8 ReadKeyData[3];

    if(HMI_ReadCmd(HMIFUNC_GET_KEY, key_num, ReadKeyData))
    {
        if(strncmp((char * )ReadKeyData, (char * )KeyActiveData, 2) == 0)
        {
            return(TRUE);
        }
    }
    return(FALSE);
}

BOOL HMI_PressFuncKey(void)
{
    return(HMI_KeyPress(HMIREG_KEY_FUNC));
}

BOOL HMI_PressYesKey(void)
{
    return(HMI_KeyPress(HMIREG_KEY_YES));
}

BOOL HMI_PressNoKey(void)
{
    return(HMI_KeyPress(HMIREG_KEY_NO));
}

void HMI_ShowPass(void)
{
    LCD_Clear(LCD_ALL_LINE);
	LCD_DisplayALine(LCD_LINE2, (U8 *)"PASS!!!");
	
	OS_SetCSemaValue(&DutReady_Sem, FALSE);

	HMI_OnPassLed();
	HMI_OnRunLed();
    HMI_PassBuzz();
}


void HMI_ShowFail(void)
{
    LCD_Clear(LCD_LINE2);
	LCD_DisplayALine(LCD_LINE2, (U8 *)"FAIL!!!");
	
	OS_SetCSemaValue(&DutReady_Sem, FALSE);

	HMI_OnFailLed();
	HMI_OnRunLed();
    HMI_FailBuzz();
}




