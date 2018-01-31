/*********************************************************************
*  
*   IAR PowerPac
*
*   (c) Copyright IAR Systems 2010.  All rights reserved.
*
**********************************************************************
----------------------------------------------------------------------
File    : Config_SAM9260.c
Purpose : Config file for Atmel SAM9260.
--------  END-OF-HEADER  ---------------------------------------------
*/

#include "USB.h"

/*********************************************************************
*
*       Defines
*
**********************************************************************
*/
#define PID_USB          (10)                                    //  USB Identifier
#define USB_TXVC       *(volatile unsigned long *) (0xFFFA4074)  // Transceiver control register

/*********************************************************************
*
*       Public code
*
**********************************************************************
*/


/*********************************************************************
*
*       USB_X_HWAttach
*/
void USB_X_HWAttach(void) {
  //
  //  Enable USB Pull-up pad 
  //
  USB_TXVC |= (1 << 9);
}


/*********************************************************************
*
*       Setup which target USB driver shall be used
*/


/*********************************************************************
*
*       USB_X_AddDriver
*/
void USB_X_AddDriver(void) {
  USB_AddDriver(&USB_Driver_AtmelSAM9260);
}


/*********************************************************************
*
*       USB_X_EnableISR
*/
void USB_X_EnableISR(USB_ISR_HANDLER * pfISRHandler) {
#if 0
  OS_ARM_InstallISRHandler(PID_USB, pfISRHandler);
  OS_ARM_EnableISR(PID_USB);
#else
  *(U32*)(0xFFFFF080 + 4 * PID_USB) = (U32)pfISRHandler;    // Set interrupt vector
  *(U32*)(0xFFFFF128)                = (1 << PID_USB);      // Clear pending interrupt
  *(U32*)(0xFFFFF120)                = (1 << PID_USB);      // Enable Interrupt
#endif
}

/**************************** end of file ***************************/
