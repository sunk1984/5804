
#include "includes.h"

//#define DEBUG_CYCLE_TEST  //zjm

U8 barCode_List[5][20] = {0};

U32 DUT_CMD(P_ITEM_T pitem)  //////////////////////////////////////////////////
{
    return(Cmd_Proc(pitem));
}

/******************************************************************************
    Routine Name    : TEST_BarcodeScan
    Parameters      : pitem
    Return value    : none
    Description     : 扫描条码, 得到条码数据
******************************************************************************/
void TEST_BarcodeScan(P_ITEM_T pitem)
{
#ifdef DEBUG_CYCLE_TEST
    strncpy((char * )barCode_List[pitem->Channel], (char *)"12345678901234567890", pitem->Param);
#else

	HMI_PassBuzz();
	
    while(1)
    {
	    //LCD_DisplayALine(LCD_LINE2, (U8 *)"Please scan the barcode");
	    LCD_DisplayALine(LCD_LINE2, (U8 *)"scan  barcode");
        SCANGUN_GetBarCode(barCode_List[pitem->Channel], pitem->Param);
        
        if(strncmp((char *)barCode_List[pitem->Channel], (char *)pitem->RspCmdPass, strlen((char *)pitem->RspCmdPass)) == 0)
        {
            if(strncmp((char *)barCode_List[pitem->Channel], (char *)barCode_List[pitem->Channel - 1], pitem->Param))
            {
        	    break;
            }
            else
            {
	            LCD_DisplayALine(LCD_LINE2, (U8 *)"Barcode error! Repeat!");
            }
        }
        else
        {
	        LCD_DisplayALine(LCD_LINE2, (U8 *)"Barcode error! Char err!");
	    }
    }
#endif
	pitem->retResult = PASS;
}
/******************************************************************************
    Routine Name    : TEST_BarcodeWr
    Parameters      : pitem
    Return value    : none
    Description     : 将得到的条码数据通过命令写入DUT
******************************************************************************/
void TEST_BarcodeWr(P_ITEM_T pitem)
{
    U8 wrStr[30]={0};

    sprintf((char *)wrStr, "%s%s", (char * )pitem->TestCmd, (char * )barCode_List[pitem->Channel]);
    strcpy((char * )pitem->TestCmd, (char *)wrStr);
	memset(wrStr , 0, 30);
    sprintf((char *)wrStr, "%s%s", (char * )pitem->RspCmdPass, (char * )barCode_List[pitem->Channel]);
    strcpy((char * )pitem->RspCmdPass, (char *)wrStr);
    pitem->retResult = DUT_CMD(pitem);
}
/******************************************************************************
    Routine Name    : TEST_BarcodeRd
    Parameters      : pitem
    Return value    : none
    Description     : 通过命令读取条码数据,并校验
******************************************************************************/
void TEST_BarcodeRd(P_ITEM_T pitem)
{
	U8 rdStr[30];
	
    sprintf((char *)rdStr, "%s%s", (char * )pitem->RspCmdPass, (char * )barCode_List[pitem->Channel]);
    strcpy((char * )pitem->RspCmdPass, (char *)rdStr);
	pitem->retResult = DUT_CMD(pitem);
}
/******************************************************************************
    Routine Name    : TEST_WaitDUT
    Parameters      : pitem
    Return value    : none
    Description     : 等待放好DUT,按下手柄,开始启动测试
******************************************************************************/
void TEST_WaitDUT(P_ITEM_T pitem)
{
	LCD_DisplayALine(LCD_LINE2, (U8 *)"put on a DUT");
#ifndef DEBUG_CYCLE_TEST
    OS_WaitCSema(&DutReady_Sem);
#endif
	OS_SetCSemaValue(&DutReady_Sem, TRUE);
	HMI_PassBuzz();
	HMI_FlashRunLed();
    
	pitem->retResult = PASS;
}
/******************************************************************************
    Routine Name    : TEST_WaitKey
    Parameters      : pitem
    Return value    : none
    Description     : 等待按钮启动测试,有的测试台没有压板检测,而是通过按钮的方式启动测试
                      按下FUNC键,启动测试
******************************************************************************/
void TEST_WaitKey(P_ITEM_T pitem)
{
	HMI_ShortWarnBuzz();
#ifndef DEBUG_CYCLE_TEST
	while(HMI_PressFuncKey() == FALSE){;}
#endif
	OS_SetCSemaValue(&DutReady_Sem, TRUE);
	HMI_FlashRunLed();
    pitem->retResult = PASS;
}
/******************************************************************************
    Routine Name    : TEST_PowerOn
    Parameters      : pitem
    Return value    : none
    Description     : 打开DUT电源
******************************************************************************/
void TEST_PowerOn(P_ITEM_T pitem)
{
	pitem->retResult = (U32)PWR_TurnOnDut();
    OS_Delay(100);
}
/******************************************************************************
    Routine Name    : TEST_PowerADJ
    Parameters      : pitem
    Return value    : none
    Description     : 调节DUT电源
******************************************************************************/
void TEST_PowerADJ(P_ITEM_T pitem)
{
    U8 i;
	U32 volt;

    pitem->retResult = FAIL;
    
	RLY_ON((U32)pitem->Channel);
    OS_Delay(50);

    volt = (U32)AD_MeasureAutoRange(pitem->upper);

    for(i = 0; i< 80; i++)      // step: 0.1V, 0.1*80 = 8V
    {
        volt = (U32)AD_MeasureAutoRange(pitem->upper);
        Dprintf((char *)"%2d.%03dV", volt/1000, volt%1000);

        if(volt >= pitem->upper)               // The real voltage is more than expected voltage.
        {
            if((volt - pitem->upper) < 100)    // The real voltage is approximate with expected voltage.
            {
                pitem->retResult = PASS;
                break;
            }
            else
            {
            	PWR_SubDutVolt(2);           // Continue to adjust the voltage, Decrease it 0.2v.
            }
        }
        else                                    // The real voltage is less than expected voltage.
        {
            if((pitem->upper - volt) < 100)    // The real voltage is approximate with expected voltage.
            {
                pitem->retResult = PASS;
                break;
            }
            else
            {
            	PWR_AddDutVolt(2);           // Continue to adjust the voltage, Increase it 0.2v.
            }
        }
    }
    
	RLY_OFF((U32)pitem->Channel);
    OS_Delay(100);
}
/******************************************************************************
    Routine Name    : TEST_PowerOff
    Parameters      : pitem
    Return value    : none
    Description     : 关闭DUT电源
******************************************************************************/
void TEST_PowerOff(P_ITEM_T pitem)
{
	pitem->retResult = (U32)PWR_TurnOffDut();
}
/******************************************************************************
    Routine Name    : TEST_PowerSet
    Parameters      : pitem
    Return value    : none
    Description     : 设置DUT电压,并打开
******************************************************************************/
void TEST_PowerSet(P_ITEM_T pitem)
{
	PWR_TurnOffDut();   //关闭DUT电源
	PWR_SetDutVolt(pitem->upper); //设置DUT电源
    OS_Delay(20);
	pitem->retResult = (U32)PWR_TurnOnDut();//打开DUT电源
    OS_Delay(100);
}
/******************************************************************************
    Routine Name    : TEST_PowerOnAux
    Parameters      : pitem
    Return value    : none
    Description     : 打开备份电源
******************************************************************************/
void TEST_PowerOnAux(P_ITEM_T pitem)
{
	pitem->retResult = (U32)PWR_TurnOnAux();
    OS_Delay(100);
}
/******************************************************************************
    Routine Name    : TEST_PowerOnAux
    Parameters      : pitem
    Return value    : none
    Description     : 关闭备份电源
******************************************************************************/
void TEST_PowerOffAux(P_ITEM_T pitem)
{
	pitem->retResult = (U32)PWR_TurnOffAux();
}
/******************************************************************************
    Routine Name    : TEST_Delay
    Parameters      : pitem
    Return value    : none
    Description     : 延时函数,单位为s
******************************************************************************/
void TEST_Delay(P_ITEM_T pitem)
{
    if(pitem->Param > 1)
    {
	    Dprintf((char * )"Delay %d s...\r\n", pitem->Param);
    }
    
    OS_Delay(pitem->Param * 1000);
	pitem->retResult = PASS;
}
/******************************************************************************
    Routine Name    : TEST_Command
    Parameters      : pitem
    Return value    : none
    Description     : DUT 命令
******************************************************************************/
void TEST_Command(P_ITEM_T pitem)
{
	pitem->retResult = DUT_CMD(pitem);
}

void TEST_AuxCmd(P_ITEM_T pitem)
{
	pitem->retResult = Cmd_Aux(pitem);
}


/******************************************************************************
    Routine Name    : TEST_CtrlMcuIO
    Parameters      : pitem
    Return value    : none
    Description     : IO控制
******************************************************************************/
void TEST_CtrlMcuIO(P_ITEM_T pitem)
{
	U32 MCU_PoutP7[POUT_CH_MAX] = {DUMMY, DUMMY, DUMMY, MCU_POUT_P7_3, MCU_POUT_P7_4, 
	MCU_POUT_P7_5, MCU_POUT_P7_6, MCU_POUT_P7_7, MCU_POUT_P7_8, MCU_POUT_P7_9};

    AT91C_BASE_PMC->PMC_PCER = 1 << AT91C_ID_PIOC;
	if(pitem->Param == 1)
	{
        MCU_GPIO_OUT->PIO_PER  = MCU_PoutP7[pitem->Channel];
		MCU_GPIO_OUT->PIO_SODR = MCU_PoutP7[pitem->Channel];
        MCU_GPIO_OUT->PIO_OER  = MCU_PoutP7[pitem->Channel];
	}
	else
	{
        MCU_GPIO_OUT->PIO_PER  = MCU_PoutP7[pitem->Channel];
		MCU_GPIO_OUT->PIO_CODR = MCU_PoutP7[pitem->Channel];
        MCU_GPIO_OUT->PIO_OER  = MCU_PoutP7[pitem->Channel];
	}
	pitem->retResult = PASS;
}

void TEST_CtrlExtIO(P_ITEM_T pitem)
{
    EXTIO_ConfigureBitDirction(pitem->Channel, IO_OUTPUT);
    pitem->retResult = EXTIO_WriteBit(pitem->Channel, pitem->Param);
}

void TEST_GetExtIO(P_ITEM_T pitem)
{
    U8 readData;

    EXTIO_ConfigureBitDirction(pitem->Channel, IO_INPUT);

    if(EXTIO_ReadBit(pitem->Channel, &readData))
    {
        if(readData = pitem->Param)
        {
            pitem->retResult = PASS;
            return;
        }
    }
    
    pitem->retResult = FAIL;
}
/******************************************************************************
    Routine Name    : TEST_CtrlRly
    Parameters      : pitem
    Return value    : none
    Description     : Relay控制,只是Relay板上的relay
******************************************************************************/
void TEST_CtrlRly(P_ITEM_T pitem)
{
    if(pitem->Param == 1)
    {
	    pitem->retResult = (U32)RLY_ON((U32)pitem->Channel);
	}
    else
    {
	    pitem->retResult = (U32)RLY_OFF((U32)pitem->Channel);
	}
    OS_Delay(100);
}

/******************************************************************************
    Routine Name    : TEST_VoltageTest
    Parameters      : pitem
    Return value    : none
    Description     : 电压测试
******************************************************************************/
void TEST_VoltageTest(P_ITEM_T pitem)
{
	U32 volt;
    U8 str[10];

	RLY_ON((U32)pitem->Channel);//打开RELAY测试通道
    OS_Delay(50);

    volt = (U32)AD_MeasureAutoRange(pitem->upper);//读取电压值

	RLY_OFF((U32)pitem->Channel);//关闭RELAY测试通道

    sprintf((char *)str, "%2d.%03dV", volt/1000, volt%1000);
	LCD_DisplayALine(LCD_LINE2, (U8 *)str);
	
    OS_Delay(50);

	if(volt >= pitem->lower && volt < pitem->upper)//电压值检测
	{
		pitem->retResult = PASS;
	}
	else
	{
		pitem->retResult = FAIL;
	}
}

void TEST_CalcCurrTest(P_ITEM_T pitem)
{
	U32 volt_former;
	U32 volt_latter;
	U32 volt_diff;
	U32 resis;  //resis range: 0.1 OHm to 99 OHm
	U32 curr;
    U8 str[10];

	RLY_ON((U32)pitem->Channel);
    OS_Delay(50);
    volt_former = (U32)AD_MeasureAutoRange(12000);  //The range is seleted to 2V-20V, apply to most of condition
	RLY_OFF((U32)pitem->Channel);
    sprintf((char * )str, "former=%2d.%03dV", volt_former/1000, volt_former%1000);
	Dprintf((char * )str);
	Dprintf((char * )"\r\n");
    OS_Delay(50);

	RLY_ON((U32)(pitem->Channel+1));
    OS_Delay(50);
    volt_latter = (U32)AD_MeasureAutoRange(12000);
	RLY_OFF((U32)pitem->Channel+1);
    sprintf((char * )str, "latter=%2d.%03dV", volt_latter/1000, volt_latter%1000);
	Dprintf((char * )str);
	Dprintf((char * )"\r\n");

    resis = pitem->Param; // resis = 0.1 Ohm * Param, For example : Param = 8, resis is 0.8 Ohm
    sprintf((char * )str, "resis=%1d.%01d Ohm", resis/10, resis%10);
	Dprintf((char * )str);
	Dprintf((char * )"\r\n");

    volt_diff = volt_former - volt_latter;
    curr = volt_diff/resis*10;
    sprintf((char * )str, "curr=%2d.%03dA", curr/1000, curr%1000);
	LCD_DisplayALine(LCD_LINE2, (U8 *)str);

	if(curr >= pitem->lower && curr < pitem->upper)
	{
		pitem->retResult = PASS;
	}
	else
	{
		pitem->retResult = FAIL;
	}
}
/******************************************************************************
    Routine Name    : TEST_ReadCurrTest
    Parameters      : pitem
    Return value    : none
    Description     : DUT电流测试
******************************************************************************/
void TEST_ReadCurrTest(P_ITEM_T pitem)
{
    U32 DutCur;
    U8 str[10];
    
    if(PWR_GetDUTCur((U32 * )&DutCur) == FALSE) //读取Power板DUT电流
    {
        pitem->retResult = FAIL;
        return;
    }
    
    sprintf((char * )str, "DutCurr=%2d.%03dA", DutCur/1000, DutCur%1000);
	LCD_DisplayALine(LCD_LINE2, (U8 *)str);

	if(DutCur >= pitem->lower && DutCur < pitem->upper)//电流值检测
	{
		pitem->retResult = PASS;
	}
	else
	{
		pitem->retResult = FAIL;
	}
}

/******************************************************************************
    Routine Name    : TEST_ManualTest
    Parameters      : pitem
    Return value    : none
    Description     : 手动测试,用户根据测试结果手动选择,按YES键PASS，按NO键Fail
******************************************************************************/
void TEST_ManualTest(P_ITEM_T pitem)
{
    if(DUT_CMD(pitem) == FALSE)
    {
        pitem->retResult = FAIL;
        return;
    }
	
	LCD_DisplayALine(LCD_LINE2, (U8 *)pitem->lcdPrt);
	
#ifdef DEBUG_CYCLE_TEST
    pitem->retResult = PASS;
	for(int i=0;i<10;i++)
#else
    if(pitem->Param == 1)
    {
	    HMI_ShortWarnBuzz();
    }

	while(1)
#endif	
	{
		if(HMI_PressYesKey())
		{
			pitem->retResult = PASS;
			break;
		}
		if(HMI_PressNoKey())
		{
			pitem->retResult = FAIL;
			break;
		}
		OS_Delay(500);
	}
    //LCD_Clear(LCD_LINE3);
	//LCD_Clear(LCD_LINE4);
}

void TEST_GpioInTest(P_ITEM_T pitem)
{
	U8 pos;
	U8 lev;

    pos = strlen((char *)pitem->RspCmdPass) - 1;    //PCA9555INT:X
    lev = pitem->RspCmdPass[pos] - '0';

    EXTIO_ConfigureBitDirction(pitem->Channel, IO_OUTPUT);
    EXTIO_WriteBit(pitem->Channel, lev);

	pitem->retResult = DUT_CMD(pitem);
}

void TEST_GpioInRlyTest(P_ITEM_T pitem)
{
	U8 pos;
	U8 lev;

    pos = strlen((char *)pitem->RspCmdPass) - 1;    //PCA9555INT:X
    lev = pitem->RspCmdPass[pos] - '0';

    if(lev == 0)
    {
	    RLY_ON((U32)pitem->Channel);
	}
    else
    {
	    RLY_OFF((U32)pitem->Channel);
	}

	pitem->retResult = DUT_CMD(pitem);
}

void TEST_GpioOutTest(P_ITEM_T pitem)
{
	U8 readData;
	U8 pos;
	U8 cmdLev;

    pos = strlen((char *)pitem->TestCmd) - 1;
    cmdLev = pitem->TestCmd[pos] - '0';

    EXTIO_ConfigureBitDirction(pitem->Channel, IO_INPUT);
    
    if(DUT_CMD(pitem) == FALSE)
    {
        pitem->retResult = FAIL;
        return;
    }

    OS_Delay(20);
    EXTIO_ReadBit(pitem->Channel, &readData); 

	if(readData == cmdLev)
	{
		pitem->retResult = PASS;
	}
    else
    {
	    pitem->retResult = FAIL;
	}
}

void TEST_GpioIn16Test(P_ITEM_T pitem)
{
	U8 * p;
    U8 start_pin;
	U16 out_data;

    start_pin = pitem->Channel;
    p = pitem->RspCmdPass + (strlen((char *)pitem->RspCmdPass) - 4);    //ALARMIN:XXXX
    out_data = (U32)strtol((char *)p, NULL, 16);

    EXTIO_ConfigureByteDirction(start_pin, IO_OUTPUT);
    EXTIO_ConfigureByteDirction(start_pin+8, IO_OUTPUT);

    EXTIO_WriteByte(start_pin, (U32)out_data);
    EXTIO_WriteByte(start_pin+8, (U32)(out_data>>8));

	pitem->retResult = DUT_CMD(pitem);
}

void TEST_In4pinTest(P_ITEM_T pitem)
{
    U8 i;
	U8 * p;

    p = pitem->RspCmdPass + (strlen((char *)pitem->RspCmdPass) - 4);    //SET:XXXX

    for(i = 0; i < 4; i ++)
    {
        if(*(p+i) == '1')
        {
    	    RLY_OFF((U32)(pitem->Channel + i));
    	}
    	else
    	{
    	    RLY_ON((U32)(pitem->Channel + i));
    	}
    }
    OS_Delay(50);

	pitem->retResult = DUT_CMD(pitem);
}

/*********************************************************************************                        
function: TEST_AudioTest

description: Audio测试

parameters: pitem

return: TRUE/FALSE
*********************************************************************************/
void TEST_AudioTest(P_ITEM_T pitem)
{
    U16 tx_amp;//audio幅度

    tx_amp = pitem->Param * 10; // Param is only assigned from 0 to 255, so amp is Param *10 , 80 means 800mv
    
    if(pitem->Channel)
    {
        RLY_ON(pitem->Channel);
    }
    
	OS_Delay(500);
    
    if(DUT_CMD(pitem) == TRUE)
	{
        pitem->retResult = (U32)Audio_LoopTest(tx_amp, pitem->lower, pitem->upper);//audio 回环测试
    }
    else
    {
        pitem->retResult = FAIL;
    }
    
    if(pitem->Channel)
    {
        RLY_OFF(pitem->Channel);
    }
}

void TEST_GenAudio(P_ITEM_T pitem)
{
    U16 freq;
    U16 amp;

    freq = pitem->lower;
    amp = pitem->upper;
    
    if(pitem->Channel)
    {
        RLY_ON(pitem->Channel);
    }
    
	OS_Delay(300);
    
    if(DUT_CMD(pitem) == TRUE)
	{
        pitem->retResult = (U32)Audio_GenTone(freq, amp);
    }
    else
    {
        pitem->retResult = FAIL;
    }
    
    if(pitem->Channel)
    {
        RLY_OFF(pitem->Channel);
    }
}

void TEST_StopAudio(P_ITEM_T pitem)
{
    pitem->retResult = Audio_StopTone();
}

void TEST_DecAudioFreq(P_ITEM_T pitem)
{
    if(DUT_CMD(pitem) == TRUE)
	{
    	OS_Delay(200);
        pitem->retResult = Audio_DecToneFreq(pitem->lower, pitem->upper);
    }
    else
    {
        pitem->retResult = FAIL;
    }
}

void TEST_CompAudioAmp(P_ITEM_T pitem)
{
    if(DUT_CMD(pitem) == TRUE)
	{
    	OS_Delay(200);
        pitem->retResult = Audio_CompToneAmp(pitem->lower, pitem->upper);
    }
    else
    {
        pitem->retResult = FAIL;
    }
}

void TEST_CommTest(P_ITEM_T pitem)
{
	OS_SetCSemaValue(&CommTest_Sem, TRUE);
    UsartRecvStart(AUX_COMM_PORT);
	pitem->retResult = DUT_CMD(pitem);
}


static U32 Get_IP_Address(U8 * ip_str)
{
	U32 DestIpAddr = 0xC0A8010A;  //Default IP address is 192.168.1.10;   
	char * C_addr = "192.168.";
    U32 Ip12 = 0xC0A80000;    //192.168.0.0
    U8 Ip3, Ip4;
    U8 * sta4;

    if(strncmp((char * )ip_str, C_addr, strlen(C_addr)) == 0)
    {
        sta4 = (U8 *)(strrchr((char *)(ip_str + 9), '.') + 1);
        Ip3 = (U8)strtod((char *)(ip_str + 8), NULL);
        Ip4 = (U8)strtod((char *)sta4, NULL);
        
	    DestIpAddr = Ip12 + (U16)(Ip3<<8) + Ip4;
    }
    return(DestIpAddr);
}

void TEST_EthernetTest(P_ITEM_T pitem)
{
    U8 i;
	U32 DestIpAddr;
	
    DestIpAddr = Get_IP_Address(pitem->RspCmdFail);
	pitem->retResult = IP_Ping_Test(DestIpAddr);

////////////////////////
    for(i=0;i<50;i++)
    {
        if(IP_Ping_Test(DestIpAddr))
        {
            i++;
        }
        OS_Delay(50);
    }
    Dprintf("IP test ok %d times of 50 times.\r\n",i);
//////////////////////    
}

void TEST_NetLedTest(P_ITEM_T pitem)
{
    U8 i;
    U8 cnt = 0;
	U8 readData;
	U32 TestLedIpAddr;

	TestLedIpAddr = Get_IP_Address(pitem->RspCmdFail);

    EXTIO_ConfigureBitDirction(pitem->Channel, IO_INPUT);

    for(i=0;i<255;i++)
    {
        if((i % 10) == 0)
        {
            IP_SendPing(htonl(TestLedIpAddr), "ICMP echo request!", strlen("ICMP echo request!"), i);
        }
        EXTIO_ReadBit(pitem->Channel, &readData); 
        OS_Delay(5);

    	if(readData == 0)
    	{
    	    cnt++;
    	}
    }
        
    Dprintf("0: %d\r\n",cnt);
    if(cnt > 5 && cnt < 160)
    {
        pitem->retResult = PASS;
    }
    else
    {
        pitem->retResult = FAIL;
    }
}

//"0704: Buzzer test,NVRFCT BUZZER=1,OK,ERROR,0.02,0.06,BUZZ_T,,21,27\r\n"
//",NVRFCT BUZZER=0,OK,ERROR,,,CMD,,,\r\n"
void TEST_BuzzTest(P_ITEM_T pitem)
{
    pitem->retResult = FAIL;
    
    if(pitem->Channel)
    {
        RLY_ON(pitem->Channel);
    }
    
	OS_Delay(100);
    
    if(Audio_TestBuzz(pitem->Param*100, pitem->lower, pitem->upper) == FAIL)
    {
        if(DUT_CMD(pitem))
    	{
	        OS_Delay(1000);
            pitem->retResult = (U32)Audio_LoopTest(pitem->Param*100, pitem->lower, pitem->upper);
        }
    }
    
    if(pitem->Channel)
    {
        RLY_OFF(pitem->Channel);
    }
}

//gaoxi add 检测是否有一个脉冲或电平跳变

#define     MAX_COLLECT_OBJ     200

void TEST_HDLedTest(P_ITEM_T pitem)
{
	U32 volt[MAX_COLLECT_OBJ],i;

    RLY_ON((U32)pitem->Channel);
    OS_Delay(50);
    
    for(i = 0; i < MAX_COLLECT_OBJ; i++)
    {
//        volt[i] = (U32)Fast_AD_cal_value(_RANGE_L_0_40V);
    }
    
    RLY_OFF((U32)pitem->Channel);
    OS_Delay(50);
    
    if(VolFlashJudge(volt, pitem->Param, pitem->lower, pitem->upper) == TRUE)
	{
		pitem->retResult = PASS;
	}
    else
    {
	    pitem->retResult = FAIL;
	}
}

void TEST_LedTest(P_ITEM_T pitem)
{
	U32 volt;
    U8 str[10];
	
    if(DUT_CMD(pitem) == FALSE)
    {
        pitem->retResult = FAIL;
        return;
    }

	RLY_ON((U32)pitem->Channel);
    OS_Delay(200);

    volt = (U32)AD_MeasureAutoRange(pitem->upper);

	RLY_OFF((U32)pitem->Channel);
    OS_Delay(20);

    sprintf((char *)str, "%2d.%03dV", volt/1000, volt%1000);
    LCD_DisplayALine(LCD_LINE2, (U8 *)str);

	if(volt >= pitem->lower && volt < pitem->upper)
	{
		pitem->retResult = PASS;
	}
	else
	{
		pitem->retResult = FAIL;
	}
}


void TEST_PortTxTest(P_ITEM_T pitem)
{
	pitem->retResult = Cmd_Ack(pitem->Channel, "", pitem->RspCmdPass, "");
}

void TEST_PortRxTest(P_ITEM_T pitem)
{
    U8 txCmd[40];

    sprintf((char *)txCmd, "%s\r\n", (char * )pitem->TestCmd);
    UsartPutStr(pitem->Channel, txCmd);

	pitem->retResult = PASS;
}

extern int  USB_Enum_Ok;

void TEST_USB_DevTest(P_ITEM_T pitem)
{
    pitem->retResult = USB_Enum_Ok;
}

void TEST_ADinTest(P_ITEM_T pitem)
{
    U32 adc=0;
    U8 str[10];

    if(Cmd_ReadData(DUT_COMM_PORT,&adc, pitem) == FALSE)
    {
        pitem->retResult = FAIL;
        return;
    }

    sprintf((char *)str, "DATA: %d", adc);
    LCD_DisplayALine(LCD_LINE2, (U8 *)str);
    
    // It means the data is SQ, lower and upper is XX(25,33,...),  Else the data is voltage, lower and upper is XXXXmv(3300,...)
    if(pitem->Param == 1)   // SQ
    {
        adc *= 1000;
    }
    
	if(adc >= pitem->lower && adc < pitem->upper)
	{
		pitem->retResult = PASS;
	}
	else
	{
		pitem->retResult = FAIL;
	}
}

void TEST_KeyTest(P_ITEM_T pitem)
{
	LCD_Display2Line(LCD_LINE3, (U8 *)pitem->lcdPrt);
	HMI_LongWarnBuzz();

	while(1)
	{
        if(DUT_CMD(pitem) == TRUE)
        {
            pitem->retResult = PASS;
            break;
        }
		if(HMI_PressNoKey())
		{
			pitem->retResult = FAIL;
			break;
		}
    	
    	OS_Delay(200);
    }
    
	HMI_LongWarnBuzzOff();
	LCD_Clear(LCD_LINE3);
	LCD_Clear(LCD_LINE4);
}
/*********************************************************************************                        
function: TEST_ReadData
parameters: pitem
return: TRUE/FALSE
description: DUT通过命令读GSM信号质量
*********************************************************************************/
void TEST_ReadData(P_ITEM_T pitem)
{
    U32 sq;       //GSM信号质量
    U32 sq1;      //GSM信号质量
    U8 str[10];  

    if(Cmd_ReadData(DUT_COMM_PORT,&sq, pitem) == FALSE)//发送读查询信号质量命令
    {
        pitem->retResult = FAIL;
        return;
    }
    //显示到LCD并保存到日志
    sprintf((char *)str, "SQ: %2d dBm", sq);
    LCD_DisplayALine(LCD_LINE2, (U8 *)str);
    //
    
    if(Cmd_ReadData(AUX_COMM_PORT,&sq1, pitem) == FALSE)//发送读查询信号质量命令
    {
        pitem->retResult = FAIL;
        return;
    }
    //显示到LCD并保存到日志
    sprintf((char *)str, "SQ: %2d dBm", sq1);
    LCD_DisplayALine(LCD_LINE3, (U8 *)str);
    //
    sq *= 1000;
    //检查GSM信号质量是否在合适的范围内
	if(sq >= pitem->lower && sq < pitem->upper)
	{
		pitem->retResult = PASS;
	}
	else
	{
		pitem->retResult = FAIL;
	}
}
//BAR_SCA,BAR_WR,BAR_RD,WAITDUT,WAITKEY,DELAY,CMD,IO_CTL,EIO_CTL,RLY_CTL,PWR_ON,PWR_OFF,COMM_T,
//IN16_T,GPIN_T,GPOUT_T,CURR_T,VOLG_T,ADC_T,RLY_T,AUDIO_T,NET_T,LED_T,BUZZ_T,NTLED_T,KEY_T,MAN_T,

const TEST_ID TestIdTab[] = 
{
	{"BAR_SCA",  TEST_BarcodeScan},
	{"BAR_WR",   TEST_BarcodeWr},
	{"BAR_RD",   TEST_BarcodeRd},
	{"WAITDUT",  TEST_WaitDUT},
	{"WAITKEY",  TEST_WaitKey},
	{"DELAY",    TEST_Delay},
	{"CMD",      TEST_Command},
	{"AUXCMD",   TEST_AuxCmd},
	{"IO_CTL",   TEST_CtrlMcuIO},
	{"EIO_CTL",  TEST_CtrlExtIO},
	{"EIO_GET",  TEST_GetExtIO},
	{"RLY_CTL",  TEST_CtrlRly},
	{"PWR_ON",   TEST_PowerOn},
	{"PWR_ADJ",  TEST_PowerADJ},
	{"PWR_OFF",  TEST_PowerOff},
	{"PWR_SET",  TEST_PowerSet},
	{"PWB_ON",   TEST_PowerOnAux},
	{"PWB_OFF",  TEST_PowerOffAux},
	{"COMM_T",   TEST_CommTest},
	{"GPINR_T",  TEST_GpioInRlyTest},
	{"GPIN_T",   TEST_GpioInTest},
	{"IN16_T",   TEST_GpioIn16Test},
	{"GPOUT_T",  TEST_GpioOutTest},
	{"VOLG_T",   TEST_VoltageTest},
	{"RCURR_T",  TEST_ReadCurrTest},
	{"CCURR_T",  TEST_CalcCurrTest},
	{"AUDIO_T",  TEST_AudioTest},
	{"AUDGEN",   TEST_GenAudio},
	{"AUDEND",   TEST_StopAudio},
	{"AUDFREQ",  TEST_DecAudioFreq},
	{"AUDAMP",   TEST_CompAudioAmp},
	{"NET_T",    TEST_EthernetTest},
	{"BUZZ_T",   TEST_BuzzTest},
	{"NTLED_T",  TEST_NetLedTest},
    {"HDLED_T",  TEST_HDLedTest},
	{"MAN_T",    TEST_ManualTest},
	{"LED_T",    TEST_LedTest},
	{"IN4P_T",   TEST_In4pinTest},
	{"PORTT_T",  TEST_PortTxTest},
	{"PORTR_T",  TEST_PortRxTest},
	{"USBDV_T",  TEST_USB_DevTest},
	{"KEY_T",    TEST_KeyTest},
	{"ADC_T",    TEST_ADinTest},
    {"RDDA_T",   TEST_ReadData},
};

U8 Get_IdSum(void)
{
    return(sizeof(TestIdTab)/sizeof(TEST_ID));
}




