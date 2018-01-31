//

#include "includes.h"

//#define DEBUG_CYCLE_TEST  //zjm

U8 TestItemArray[CH_PERCFG_MAX] = 
"ITEM,COMMAND,RESPONSE CMD PASS,RESPONSE CMD FAIL,LOWER(V),UPPER(V),ID,LCD PRINT,IO RLY CHANNEL,PARAM\r\n"
"0101:Bar code,,,,,,BAR_SCA,,1,8\r\n"     //scan barcode
"0102:Wait DUT,,,,,,WAITDUT,,,\r\n"       //wait dut
"0103:Power On,,,,,,RLY_CTL,,1,1\r\n"     //open GND
"0103:Power On,,,,,,PWR_ON,,,\r\n"        //power on
"0103:Power On,,,,,,RLY_CTL,,14,1\r\n"    //Open UART GND
"0103:Power On,,,,,,RLY_CTL,,13,1\r\n"    //Open UART power
"0104:RF VDD,,,,3.0,3.6,VOLG_T,,4,\r\n"   //Test VDD RF voltage
"0105:Load FCT,,,,,,RLY_CTL,,9,1\r\n"     //open VCC of E1
"0105:Load FCT,,,,,,RLY_CTL,,10,1\r\n"    //open data of E1
"0105:Load FCT,,,,,,RLY_CTL,,11,1\r\n"    //open reset of E1
"0105:Load FCT,,,,,,RLY_CTL,,12,1\r\n"    //open GND of E1
"0105:Load FCT,,,,,,MAN_T,,,1\r\n"        //load  FCT program
"0105:Load FCT,,,,,,RLY_CTL,,9,0\r\n"     //close VCC of E1
"0105:Load FCT,,,,,,RLY_CTL,,10,0\r\n"    //close data of E1
"0105:Load FCT,,,,,,RLY_CTL,,11,0\r\n"    //close reset of E1
"0105:Load FCT,,,,,,RLY_CTL,,12,0\r\n"    //close GND of E1
"0106:DUT Start,,,,,,DELAY,,,1\r\n"                  // delay to wait DUT run       
"0106:DUT Start,FCT+MCUSTART,OK,ERROR,,,CMD,,,\r\n"  //Notify DUT to enter FCT mode

"0107:Reed,FCT+REED?,REED:1,,,,CMD,,,\r\n"        //test ReedSwitch status
"0107:Reed,,,,,,RLY_CTL,,5,1\r\n"                 //open Electromagnet
"0107:Reed,FCT+REED?,REED:0,,,,CMD,,,\r\n"        //test ReedSwitch status
"0107:Reed,,,,,,RLY_CTL,,5,0\r\n"                 //close Electromagnet

"0108:Loop1,FCT+LOOP1?,LOOP1:1,,,,CMD,,,\r\n"     //test Loop1 status
"0108:Loop1,,,,,,RLY_CTL,,8,1\r\n"                //open Loop1
"0108:Loop1,FCT+LOOP1?,LOOP1:0,,,,CMD,,,\r\n"     //test Loop1 status
"0108:Loop1,,,,,,RLY_CTL,,8,0\r\n"                //close Loop1

"0109:Tamper1,,,,,,RLY_CTL,,6,1\r\n"                  //close Tamper2
"0109:Tamper1,FCT+TAMPER?,TAMPER:1,,,,CMD,,,\r\n"     //test Tamper1 status
//"0109:Tamper1,,,,,,MAN_T,,,1\r\n"                      //press Tamper1
"0109:Tamper1,FCT+TAMPER?,TAMPER:0,,,,TAMP_T,,,1\r\n"     //test Tamper1 status
"0109:Tamper1,,,,,,RLY_CTL,,6,0\r\n"                  //open Tamper2

"0110:Tamper2,,,,,,RLY_CTL,,7,1\r\n"                  //close Tamper1
"0110:Tamper2,FCT+TAMPER?,TAMPER:1,,,,CMD,,,\r\n"     //test Tamper2 status
//"0110:Tamper2,,,,,,MAN_T,,,1\r\n"                      //press Tamper2
"0110:Tamper2,FCT+TAMPER?,TAMPER:0,,,,TAMP_T,,,1\r\n"     //test Tamper2 status
"0110:Tamper2,,,,,,RLY_CTL,,7,0\r\n"                  //open Tamper1

"0111:RFM Start,FCT+MCUSTART,OK,ERROR,,,AUXCMD,,2,\r\n"   //Notify RFM to enter FCT mode.
"0112:CW Start,FCT+CW=1,CW:1,ERROR,,,CMD,,,\r\n"         //Notify DUT to enter TX carrier status.
"0113:RSSI Test,FCT+RSSI?,RSSI:,,55,68,RSSI_T,,2,2\r\n"  //Get RSSI of RFM, Calculate the average and variance.      
"0114:CW End,FCT+CW=0,CW:0,ERROR,,,CMD,,,\r\n"           //Notify DUT to exit TX carrier status.
"0115:RF Mode,FCT+DATAMODE=0,OK,ERROR,,,AUXCMD,,2,\r\n"  //Notify RFM to enter FCT data Mode.
"0115:RF Data,FCT+DATA=1,OK,ERROR,,,AUXCMD,,2,\r\n"      //Notify RFM to enter receive data.
"0115:RF Test,FCT+DATA?,DATA:,ERROR,,,RFDA_T,,2,3\r\n"   //RF Data test. From DUT to RFM.
"0115:RF Data,FCT+DATA=0,OK,ERROR,,,AUXCMD,,2,\r\n"      //Notify RFM to enter receive data.
"0115:RF Mode,FCT+DATAMODE=1,OK,ERROR,,,AUXCMD,,2,\r\n"  //Notify RFM to enter Normal data mode

"0117:Write SN,FCT+SN=,SN:,ERROR,,,BAR_WR,,1,\r\n"        //write serial number
"0118:Read SN,FCT+SN?,SN:,,,,BAR_RD,,1,\r\n"              //veriry serial number

"0119:LOW PWR,FCT+LOWPWR,OK,ERROR,,,CMD,,,\r\n"           //Notify DUT to enter low power mode
"0120:Current,,,,,,RLY_CTL,,13,0\r\n"                     //close UART power
"0120:Current,,,,,,RLY_CTL,,14,0\r\n"                     //close UART GND
"0120:Current,,,,,,RLY_CTL,,2,1\r\n"                      //open sample resistance
"0120:Current,,,,,,RLY_CTL,,1,0\r\n"                      //change GND mode
"0120:Current,,,,0.3,2,CUR_T,,3,\r\n"                     //test current
"0120:Current,,,,,,RLY_CTL,,1,1\r\n"                      //change GND mode
"0120:Current,,,,,,RLY_CTL,,2,0\r\n"                      //close sample resistance
"0120:Current,,,,,,RLY_CTL,,14,1\r\n"                     //Open UART GND
"0120:Current,,,,,,RLY_CTL,,13,1\r\n"                     //Open UART power

"0122:Load APP,,,,,,RLY_CTL,,9,1\r\n"     //open VCC 
"0122:Load APP,,,,,,RLY_CTL,,10,1\r\n"    //open data 
"0122:Load APP,,,,,,RLY_CTL,,11,1\r\n"    //open reset
"0122:Load APP,,,,,,RLY_CTL,,12,1\r\n"    //open GND
"0122:Load APP,,,,,,MAN_T,,,1\r\n"        //load  APP program
"0122:Load APP,,,,,,RLY_CTL,,9,0\r\n"     //close VCC
"0122:Load APP,,,,,,RLY_CTL,,10,0\r\n"    //close data
"0122:Load APP,,,,,,RLY_CTL,,11,0\r\n"    //close reset
"0122:Load APP,,,,,,RLY_CTL,,12,0\r\n"    //close GND
"0122:Load APP,,,,,,RLY_CTL,,12,0\r\n"    //close GND
"0123:RF Data,FCT+DATA=1,OK,ERROR,,,AUXCMD,,2,\r\n"  //
"0123:SN Test,FCT+DATA?,DATA:,ERROR,,,SN_T,,1,\r\n" //
",,,,,,,,,\r\n"
;

#define RF_DATA_SAMPLE		50
#define RF_DATA_TIMES_MAX   20
#define RF_DUT_COMM_PORT    USART1
#define RF_MODULE_COMM_PORT USART2
extern U8 barCode_List[5][20];
/******************************************************************************
    Routine Name    : TEST_APP_RssiTest
    Parameters      : pitem
    Return value    : PASS/FAIL
    Description     : 测试RF RSSI
******************************************************************************/
void TEST_APP_RssiTest(P_ITEM_T pitem)
{
	U32 i;
	U32 getRssi;
	U8 rssiSamp[RF_DATA_SAMPLE];
	U32 diff;
	U32 Average  = 0;
	U32 Variance = 0;
	U32 VarLmtMax;

	VarLmtMax = pitem->Param;

	Cmd_ReadData(pitem->Channel,&getRssi, pitem);	// Is a dummy operation
	OS_Delay(10);
	
	//Get Samples
	for(i=0; i<RF_DATA_SAMPLE; i++){
		if(Cmd_ReadData(pitem->Channel,&getRssi, pitem)){
		    rssiSamp[i] = getRssi;   //dBm Conver
            Dprintf("-%d,", rssiSamp[i]);
		}
        else{
            break;
		}
		OS_Delay(10);
	}
	if(i < RF_DATA_SAMPLE){
	    pitem->retResult = FAIL;
	    return;
	}

	//Get Average
	for(i=0; i<RF_DATA_SAMPLE; i++){
		Average += rssiSamp[i];
	}
	Average /= RF_DATA_SAMPLE;

	//Get Variance
	for(i=0; i<RF_DATA_SAMPLE; i++){
	    if(rssiSamp[i] >= Average){
            diff = rssiSamp[i] - Average;
	    }
	    else{
            diff = Average - rssiSamp[i];
	    }
	    diff = diff * diff;
		Variance += diff;
	}
	Variance /= RF_DATA_SAMPLE;
    Dprintf("\r\nAverage = %d, Variance = %d\r\n", Average, Variance);
    
    //check 
    if(Variance <= VarLmtMax){
        pitem->lower /= 1000;
        pitem->upper /= 1000;

    	if(Average >= pitem->lower && Average <= pitem->upper){
            pitem->retResult = PASS;
            return;
    	}
    }
    
	pitem->retResult = FAIL;
}

/******************************************************************************
    Routine Name    : TEST_APP_RFDA_Test
    Parameters      : pitem
    Return value    : PASS/FAIL
    Description     : 测试RF 发送
******************************************************************************/
void TEST_APP_RFDA_Test(P_ITEM_T pitem)
{
    U32 i;
    U32 fail_cnt = 0;
    U32 chk;

    U8 TestDataTx[12+1]="313233343536"; //RF模块需要发送的数据
    U8 TestDataRx[12+1]="313233343536"; //从DUT接收到的数据
    U8 tx_str[24];
    U8 exp_str[24]; //
    U32 TestFailMax;//最大失败次数
    
    TestFailMax = pitem->Param;

    for(i=0; i<RF_DATA_TIMES_MAX; i++)   // Odd frames can not be received successfully?
    {
        TestDataTx[11] = (i<<1)%10+'0';  // Format string to "12345X", X:even
        TestDataRx[11] = (i<<1)%10+'0';
        sprintf((char * )tx_str, "%s%s", "FCT+RFDATA=", TestDataTx);
        chk = FAIL;
        if(Cmd_Ack(RF_DUT_COMM_PORT, tx_str, "OK", "ERROR"))//RF模块发送数据
        {
            memset(exp_str, 0 ,24);
            sprintf((char * )exp_str, "DATA:%s", TestDataRx);
            chk = Cmd_Listen(RF_MODULE_COMM_PORT, (U8 *)exp_str);
            //OS_Delay(100);
            //chk=Cmd_Ack(RF_MODULE_COMM_PORT,"FCT+DATA?",exp_str,pitem->RspCmdFail);//被测板读取RF数据,并校验数据
        }
        
        if(chk == FALSE)
        {
            Dprintf("Package%d failed!\r\n", i);
            
            if(fail_cnt++ > TestFailMax)//
            {
                break;
            }
        }
    }

    if(fail_cnt <= TestFailMax){
	    pitem->retResult = PASS;
	}
	else{
	    pitem->retResult = FAIL;
	}
    OS_Delay(300);  // For next step
}

/******************************************************************************
    Routine Name    : TEST_APP_Current
    Parameters      : pitem
    Return value    : PASS/FAIL
    Description     : 测试低功耗时的电流 I=V/R,采样电阻1K 电流单位uA
******************************************************************************/
void TEST_APP_Current(P_ITEM_T pitem) 
{
    INT32U volt; 
    INT32U curr;
    RLY_ON((U32)pitem->Channel);//打开RELAY测试通道
    OS_Delay(50);
   
    volt=getADCValue();

    RLY_OFF((U32)pitem->Channel);//关闭RELAY测试通道
    OS_Delay(50);
    curr=volt/1000;
    Dprintf("current:%dnA\r\n", curr);
    if((curr > pitem->lower) && (curr < pitem->upper)){
        pitem->retResult = PASS;
    }
    else{
        pitem->retResult = FAIL;
    }
}

/******************************************************************************
    Routine Name    : TEST_APP_WriteSN
    Parameters      : pitem
    Return value    : PASS/FAIL
    Description     : 测试低功耗时的电流 I=V/R,采样电阻1K 电流单位uA
******************************************************************************/
void TEST_APP_WriteSN( P_ITEM_T pitem ) 
{
    U8 tx_str[24]={0};
    sprintf((char * )tx_str, "%s%d", "FCT+SN=", 1234567);
    if(Cmd_Ack(RF_DUT_COMM_PORT, tx_str, pitem->RspCmdPass,pitem->RspCmdFail)){
        pitem->retResult = PASS;
    }
    else{
        pitem->retResult = FAIL;
    }
}
static U32 SwitchCharToHexSN(U8 *pCharSN)
{
    uint32_t sn32 = 0;

    sn32  = (pCharSN[0] - '0') * 1000000uL;
    sn32 += (pCharSN[1] - '0') * 100000uL;
    sn32 += (pCharSN[2] - '0') * 10000uL;
    // '-'
    sn32 += (pCharSN[4] - '0') * 1000uL;
    sn32 += (pCharSN[5] - '0') * 100uL;
    sn32 += (pCharSN[6] - '0') * 10uL;
    sn32 += (pCharSN[7] - '0') * 1uL;

    return sn32;

}//end of SwitchCharToHexSN()

void SnFunc(U8 *pCharSN, U8 *pCpySn)
{
    U32 sn32;
    U8 sn8[3];
   
    sn32 = SwitchCharToHexSN(pCharSN);
    sn8[0] = ((sn32 & 0x00FF0000) >> 16) | 0x80;
    sn8[1] = ((sn32 & 0x0000FF00) >>  8);
    sn8[2] = ((sn32 & 0x000000FF) >>  0);
    
    sprintf((char *)pCpySn, "%02x", sn8[0]);
    sprintf((char *)pCpySn+2, "%02x", sn8[1]);
    sprintf((char *)pCpySn+4, "%02x", sn8[2]);
}
void TEST_APP_CheckSN( P_ITEM_T pitem )
{
    U8  SnOriStr[10]={0};
    U8  SnCpyStr[8]={0};
    U8  exp_str[24]={0}; //
    //sprintf((char *)SnOriStr, "%s%s", (char *)pitem->TestCmd, (char *)barCode_List[pitem->Channel]);
    strcpy((char *)SnOriStr, (char *)barCode_List[pitem->Channel]);
    SnFunc(SnOriStr, SnCpyStr);
    sprintf((char *)exp_str, "%s%s", (char *)pitem->RspCmdPass, SnCpyStr);
    if (Cmd_ListenSn(RF_MODULE_COMM_PORT, exp_str)) {
        pitem->retResult = PASS;
    }
    else {
        pitem->retResult = FAIL;
    }
    
}

void TEST_APP_Tamper(P_ITEM_T pitem)
{
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
      if(Cmd_Ack(RF_DUT_COMM_PORT, pitem->TestCmd, pitem->RspCmdPass, pitem->RspCmdFail)) {
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
}
const TEST_ID TestAppIdTab[] = 
{
	{"RSSI_T", TEST_APP_RssiTest},
	{"RFDA_T", TEST_APP_RFDA_Test},
    {"CUR_T",  TEST_APP_Current},
    {"SN_T",   TEST_APP_CheckSN},    
    {"TAMP_T", TEST_APP_Tamper},
};




U8 Get_App_IdSum(void)
{
    return(sizeof(TestAppIdTab)/sizeof(TEST_ID));
}



