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
#ifndef _PWR_H_
#define _PWR_H_

#include "includes.h"

extern BOOL PWR_TurnOnDut(void);
extern BOOL PWR_TurnOffDut(void);
extern BOOL PWR_TurnOnAux(void);
extern BOOL PWR_TurnOffAux(void);
extern BOOL PWR_SetDutVolt(U32 volt);
extern BOOL PWR_SetAuxVolt(U32 volt);
extern BOOL PWR_GetDUTCur(U32 *pCur);
extern BOOL PWR_AddDutVolt(U32 Stepval);
extern BOOL PWR_SubDutVolt(U32 Stepval);
#endif//_PWR_H_
