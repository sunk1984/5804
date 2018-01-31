
#include "includes.h"

#define RF_DATA_SAMPLE		100

static BOOL RFM_GetRSSI(U8 * rssi, P_ITEM_T pitem)
{
    U8 recvbyte;
    U8 recvbuf[30];
    U8 txCmd[30];
    U8 i;
    U8 len;

    len = strlen((char * )pitem->RspCmdPass);

    sprintf((char *)txCmd, "%s\r\n", (char * )pitem->TestCmd);
    UsartPutStr(pitem->Channel, txCmd);
    
    for(i=0;i<100;i++)
    {
//	    strcpy((char * )recvbuf, (char * )"RSSI:-  4\r\n");
        if(UsartGetFrame_by_2BytesEnd(pitem->Channel, '\r','\n', recvbuf, sizeof(recvbuf),(INT32U * )&recvbyte) == 0)
        {
		    if(strncmp((char * )pitem->RspCmdPass, (char * )recvbuf, len) == 0)
		    {
                * rssi = (U8)strtod((char *)(recvbuf+len), NULL);
                break;
            }
        }
        OS_Delay(10);
    }
    if(i < 100)
    {
        return(TRUE);
    }
    else
    {
        return(FALSE);
    }
}

BOOL RFM_RssiTest(P_ITEM_T pitem)
{
	U8 i;
	U8 getRssi;
	U8 rssiSamp[RF_DATA_SAMPLE];
	U8 diff;
	U16 Average = 0;
	U32 Variance = 0;

    UsartRecvReset(pitem->Channel);

	//Get Samples
	for(i=0; i<RF_DATA_SAMPLE; i++)
	{
		if(RFM_GetRSSI(&getRssi, pitem))
		{
		    rssiSamp[i] = getRssi;   //dBm Conver
            Dprintf("-%d,", rssiSamp[i]);
		}
	}

	//Get Average
	for(i=0; i<RF_DATA_SAMPLE; i++)
	{
		Average += rssiSamp[i];
	}
	Average /= RF_DATA_SAMPLE;

	//Get Variance
	for(i=0; i<RF_DATA_SAMPLE; i++)
	{
	    if(rssiSamp[i] >= Average)
	    {
            diff = rssiSamp[i] - Average;
	    }
	    else
	    {
            diff = Average - rssiSamp[i];
	    }
	    diff = diff * diff;
		Variance += diff;
	}
	Variance /= RF_DATA_SAMPLE;
    Dprintf("\r\nAverage = %d, Variance = %d\r\n", Average, Variance);
    
    if(Variance > pitem->Param)
    {
        return(FALSE);
    }
    
    pitem->lower /= 1000;
    pitem->upper /= 1000;

	if(Average < pitem->lower || Average > pitem->upper)
	{
        return(FALSE);
	}
	return(TRUE);
}



