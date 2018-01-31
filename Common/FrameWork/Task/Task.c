

#include "includes.h"

//#define DEBUG_CYCLE_TEST  //zjm

#define TASKPRIO_SCAN_DUT	(160)
#define TASKPRIO_SCANGUN	(155)
#define TASKPRIO_TEST		(140)
#define TASKPRIO_COMMTEST	(130)

#define DUTSTATE_IDLE       0
#define DUTSTATE_PULLUP     1
#define DUTSTATE_PUSHDOWN   2

#define GPIO_KEY 	    AT91C_BASE_PIOC
#define PIN_DUTPROB 	AT91C_PIO_PC31

OS_STACKPTR int Stack_ScanDut[256]; /* Task stacks */
OS_TASK TCB_ScanDut;                        /* Task-control-blocks */

OS_STACKPTR int Stack_Test[2048]; 	/* Task stacks */
OS_TASK TCB_TEST;                        /* Task-control-blocks */

OS_STACKPTR int Stack_CommTest[768]; 	/* Task stacks */
OS_TASK TCB_COMM_TEST;                        /* Task-control-blocks */

//USBH HID
static OS_STACKPTR int  Stack_USBH_HID[1024];  
static OS_TASK          TCB_USBH_HID;

OS_CSEMA  DutReady_Sem;
OS_CSEMA  CommTest_Sem;

static void Prob_Init(void)
{
    AT91C_BASE_PMC->PMC_PCER |= 1 << AT91C_ID_PIOC;

    GPIO_KEY->PIO_PER   = PIN_DUTPROB;
    GPIO_KEY->PIO_ODR   = PIN_DUTPROB;
    GPIO_KEY->PIO_PPUER = PIN_DUTPROB;
}

static void SysRst(void)
{
    MERAK_ResetALL();
#ifdef DEBUG
	AT91C_BASE_RSTC->RSTC_RCR = (0xA5 << 24) | 0x01;    //Debug
#else
	AT91C_BASE_RSTC->RSTC_RCR = (0xA5 << 24) | 0x05;    //Release
#endif
}

static U32 PushDownDut(void)
{
    if((GPIO_KEY->PIO_PDSR & PIN_DUTPROB) == 0)
    {
        return(TRUE);
    }
    else
    {
        return(FALSE);
    }
}


static void CommTest(void)
{
    U8 ch;
    
    OS_WaitCSema(&CommTest_Sem);
    
    while(1)
    {
        if(UsartGetChar(AUX_COMM_PORT, &ch) == RECV_OK)
        {
            if(ch == 0xaa)
            {
                UsartPutChar(AUX_COMM_PORT, 0x55);
            }
        }
        OS_Delay(5);
    }
}

static void ScanDut_Task(void)
{
	static U8 DutState = DUTSTATE_IDLE;

    Prob_Init();
    
	while(1)
	{
		if(DutState == DUTSTATE_IDLE)		//NO DUT.
		{
			if(PushDownDut() == FALSE)	//pull up
			{
				DutState = DUTSTATE_PULLUP;
			}
		}
		if(DutState == DUTSTATE_PULLUP)		//NO DUT.
		{
			if(PushDownDut())
			{
				OS_Delay(500);
				if(PushDownDut())
				{
					DutState = DUTSTATE_PUSHDOWN;	//DUT OK!
	                OS_SetCSemaValue(&DutReady_Sem, TRUE);
				}
			}
		}
		if(DutState == DUTSTATE_PUSHDOWN)
		{
			if(PushDownDut() == FALSE)	//pull up
			{
				PWR_TurnOffDut();
				SysRst();
			}
        }
		OS_Delay(10);
	}
}

static void Test_Task(void)
{
	INITFILE_Proc();
	CFGFILE_Proc();
#ifdef DEBUG_CYCLE_TEST
	OS_Delay(3000);
	SysRst();
#endif
	while(1)
	{
		OS_Delay(500);
	}
}

void TASK_Init(void)
{
    FS_Init();
    
    OS_CREATECSEMA(&DutReady_Sem);
    OS_CREATECSEMA(&CommTest_Sem);

	OS_CREATETASK(&TCB_ScanDut,  "ScanDut Task",   ScanDut_Task,  TASKPRIO_SCAN_DUT, Stack_ScanDut);
	OS_CREATETASK(&TCB_TEST, 	 "Test Task", 	   Test_Task,  	  TASKPRIO_TEST, 	 Stack_Test);
    OS_CREATETASK(&TCB_USBH_HID, "USBH_HID_Task",  USBH_HID_Task, TASKPRIO_SCANGUN,  Stack_USBH_HID);
    //OS_CREATETASK(&TCB_COMM_TEST,"COMM_TEST_TASK", CommTest,      TASKPRIO_COMMTEST, Stack_CommTest);
}



