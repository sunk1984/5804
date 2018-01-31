/*******************************************************************************
    Copyright(C) 2012, Honeywell Integrated Technology (China) Co.,Ltd.
    Security FCT team
    All rights reserved.
    File name:  FCT.h
    Function: FCT main function
    IDE:    IAR EWARM V6.21 
    ICE:    J-Link 
    BOARD:  Merak board V1.0
    History
    2012.02.28  ver.1.00    First released by Roger                                 
*******************************************************************************/
#ifndef _HONEYWELL_TASK_H_
#define _HONEYWELL_TASK_H_

#include "includes.h"

extern OS_CSEMA  DutReady_Sem;  
extern OS_CSEMA  CommTest_Sem;

extern void TASK_Init(void);


#endif
