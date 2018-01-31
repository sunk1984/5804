

#include "includes.h"

U32 ReadChann(U8 * chan)
{
    U8 i; 
    U32 recvbyte;
    U32 recvflag = FALSE;
    U8 recvbuf[100];

    UsartRecvReset(AUX_COMM_PORT);
    UsartPutStr(AUX_COMM_PORT, "R107701\r\n");

    for(i = 0; i < 255; i++)
    {
        if(UsartGetFrame_by_2BytesEnd(AUX_COMM_PORT, '\r', '\n', recvbuf, sizeof(recvbuf),(INT32U *)&recvbyte) == 0)
        {
            if(strncmp((char * )recvbuf, (char * )"R107701\r", 8) == 0)
            {
                * chan = recvbuf[9] - '0';
                recvflag = TRUE;
                break;
            }
        }
        OS_Delay(2);
    }

    return(recvflag);
}

U32 BarCode_Command(U8 * CmdStr)
{
    U32 chk;
	U8 RspStr[30];

    sprintf((char *)RspStr, "%s%s", (char * )CmdStr, "\r");
    
	chk = Cmd_Ack(AUX_COMM_PORT, CmdStr, RspStr, NULL);

	return(chk);

}


U32 WriteScenes(U8 chan)
{
    U32 chk;

    UsartRecvReset(AUX_COMM_PORT);
    
    chk = BarCode_Command("W10048300AA80");
    if(chk == FALSE)
    {
        return(FALSE);
    }
    
	OS_Delay(200);
    chk = BarCode_Command("W84088300AA800100E400");
    if(chk == FALSE)
    {
        return(FALSE);
    }

	OS_Delay(200);
    if(chan > 1)
    {
        chk = BarCode_Command("W84108300AA800201E400");
        if(chk == FALSE)
        {
            return(FALSE);
        }
    }
    
	OS_Delay(200);
    if(chan > 2)
    {
        chk = BarCode_Command("W84188300AA800302E400");
        if(chk == FALSE)
        {
            return(FALSE);
        }
    }

    return(TRUE);
}

U32 ReadyReadID(U8 chan)
{
    U32 chk;

	chk = Cmd_Ack(AUX_COMM_PORT, "R100404", "R100404\r8300AA80", NULL);
    if(chk == FALSE)
    {
        return(FALSE);
    }
    
	chk = Cmd_Ack(AUX_COMM_PORT, "R840808", "R840808\r8300AA800100E400", NULL);
    if(chk == FALSE)
    {
        return(FALSE);
    }

    if(chan > 1)
    {
	    chk = Cmd_Ack(AUX_COMM_PORT, "R841008", "R841008\r8300AA800201E400", NULL);
        if(chk == FALSE)
        {
            return(FALSE);
        }
    }
    
    if(chan > 2)
    {
	    chk = Cmd_Ack(AUX_COMM_PORT, "R841808", "R841808\r8300AA800302E400", NULL);
        if(chk == FALSE)
        {
            return(FALSE);
        }
    }

    return(TRUE);
}

U32 ReadEEPROM(U8 * eep_str)
{
    U8 i; 
    U32 recvbyte;
    U32 recvflag = FALSE;
    U8 recvbuf[100];

    UsartRecvReset(AUX_COMM_PORT);
    UsartPutStr(AUX_COMM_PORT, "R104004\r\n");

    for(i = 0; i < 255; i++)
    {
        if(UsartGetFrame_by_2BytesEnd(AUX_COMM_PORT, '\r', '\n', recvbuf, sizeof(recvbuf),(INT32U *)&recvbyte) == 0)
        {
            if(strncmp((char * )recvbuf, (char * )"R104004\r", 8) == 0)
            {
                recvbuf[recvbyte - 2] = 0;   //Remove "\r\n"
                strcpy((char * )eep_str, (char * )(recvbuf+8));
                recvflag = TRUE;
                break;
            }
        }
        OS_Delay(2);
    }

    return(recvflag);
}

U32 BarcodeWrRd(char * BarCode_Num)
{
    U32 chk;
    U8 chan;
	U8 str[30];
    
	chk = Cmd_Ack(AUX_COMM_PORT, "s00", "OK!", NULL);
	if(chk == FALSE)
	{
	    return(FALSE);
	}

    sprintf((char *)str, "%s%s", "W1040", BarCode_Num);
    chk = BarCode_Command(str);
	if(chk == FALSE)
	{
	    return(FALSE);
	}
	OS_Delay(500);

    chk = ReadChann(&chan);
	if(chk == FALSE)
	{
	    return(FALSE);
	}
	
    Dprintf("chan=%d\r\n",chan);
    chk = WriteScenes(chan);
	if(chk == FALSE)
	{
	    return(FALSE);
	}

    Dprintf("ReadyReadID\r\n");
    chk = ReadyReadID(chan);
	if(chk == FALSE)
	{
	    return(FALSE);
	}

    Dprintf("ReadEEPROM\r\n");
    chk = ReadEEPROM(str);
	if(chk == FALSE)
	{
	    return(FALSE);
	}

    Dprintf((char *)str);
    Dprintf("\r\n");
    if(strcmp((char *)str, BarCode_Num) == 0)
    {
        Dprintf("PASS!\r\n");
	    return(TRUE);
    }
    else
    {
        Dprintf("FAIL!\r\n");
	    return(FALSE);
    }
}


