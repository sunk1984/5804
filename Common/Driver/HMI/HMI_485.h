/*******************************************************************************
    Merak.c
    Board support packets of Merak main board

    Copyright(C) 2010, Honeywell Integrated Technology (China) Co.,Ltd.
    Security FCT team
    All rights reserved.

    IDE:    IAR EWARM V5.5  5501-1908
    ICE:    J-Link V8
    BOARD:  Merak V1.0

    History
    2010.07.14  ver.1.00    First release by Taoist
*******************************************************************************/
#ifndef _HMI_H_
#define _HMI_H_

#include "includes.h"

extern BOOL HMI_PassBuzz(void);
extern BOOL HMI_FailBuzz(void);
extern BOOL HMI_LongWarnBuzz(void);
extern BOOL HMI_LongWarnBuzzOff(void);
extern BOOL HMI_ShortWarnBuzz(void);

extern BOOL HMI_OnRunLed(void);
extern BOOL HMI_OffRunLed(void);
extern BOOL HMI_FlashRunLed(void);

extern BOOL HMI_OnFailLed(void);
extern BOOL HMI_OffFailLed(void);

extern BOOL HMI_OnPassLed(void);
extern BOOL HMI_OffPassLed(void);

extern BOOL HMI_PressYesKey(void);
extern BOOL HMI_PressNoKey(void);
extern BOOL HMI_PressFuncKey(void);

extern void HMI_ShowPass(void);
extern void HMI_ShowFail(void);

#endif//_HMI_H_
