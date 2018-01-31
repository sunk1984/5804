/*********************************************************************
*  
*   IAR PowerPac
*
*   (c) Copyright IAR Systems 2010.  All rights reserved.
*
**********************************************************************
----------------------------------------------------------------------
File    : IP_Ping.c
Purpose : Ping program for embOS & TCP/IP
--------- END-OF-HEADER ----------------------------------------------
*/
#include "includes.h"

/*********************************************************************
*
*       Local defines, configurable
*
**********************************************************************
*/
#define USE_RX_TASK       0                 // 0: Packets are read in ISR, 1: Packets are read in a task of its own.


/*********************************************************************
*
*       static data
*/
static OS_STACKPTR int IP_Stack[512]; // Task stacks and Task-control-blocks
static OS_TASK         IP_TCB;
static OS_STACKPTR int _IPStack[768/sizeof(int)];       // Define the stack of the IP_Task to 768 bytes
static OS_TASK         _TCB;                            // Task-control-block

OS_CSEMA  PingReply_Sem;

/*********************************************************************
*
*       _pfOnRxICMP
*/
static int _pfOnRxICMP(IP_PACKET * pPacket)
{
	const char * pData;

	pData = IP_GetIPPacketInfo(pPacket);
//	if(*pData == 0x08)
//	{
//		JLINKDCC_Printf("ICMP echo request!\n");
//	}
	if(*pData == 0x00)
	{
//		JLINKDCC_Printf("ICMP echo reply!\n");
		OS_SetCSemaValue(&PingReply_Sem, TRUE);		//ping pass.
	}
	return 0;
}

/*********************************************************************
*
*       IP_Ping_Task
*/

static void IP_Ping_Task(void)
{
	IP_Init();
	OS_SetPriority(OS_GetTaskID(), 255);                         // This task has highest prio for real-time application
	OS_CREATECSEMA(&PingReply_Sem);
	OS_CREATETASK(&_TCB, "IP_Task", IP_Task  , 150, _IPStack);   // Start the IP_Task
	IP_ICMP_SetRxHook(_pfOnRxICMP);
	while(1)
	{
		OS_Delay(500);
	}
}

void IP_Ping_Init(void)
{
	OS_CREATETASK(&IP_TCB, "IP_TestTask", IP_Ping_Task, 100, IP_Stack);
}

U32 IP_Ping_Test(U32 ip_addr)
{
	OS_SetCSemaValue(&PingReply_Sem, FALSE);		//ready to ping.
	
    IP_SendPing(htonl(ip_addr), "ICMP echo request!", strlen("ICMP echo request!"), 1111);

	return((U32)(OS_WaitCSemaTimed(&PingReply_Sem, 1000)));
}



