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
#ifndef _BSP_H_
#define _BSP_H_

#include "includes.h"

#define USBH_ID             AT91C_ID_UHP

#define MATRIX_BASE_ADDR    (0xFFFFEE00)    // Matrix configuration
#define MATRIX_SCFG3        (*(volatile unsigned int*) (MATRIX_BASE_ADDR + 0x4C)) // Slave configuration register 3
#define MATRIX_PRAS3        (*(volatile unsigned int*) (MATRIX_BASE_ADDR + 0x98)) // Priority Register A for Slave 3

extern void BSP_ETH_Init(unsigned Unit);
extern void BSP_CACHE_InvalidateRange(void * p, unsigned NumBytes);
extern void BSP_USBH_Init(void);
extern void BSP_USBH_InstallISR(void (*pfISR)(void));

#endif//_BSP_H_
