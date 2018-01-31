/*********************************************************************
*  
*   IAR PowerPac
*
*   (c) Copyright IAR Systems 2010.  All rights reserved.
*
**********************************************************************
----------------------------------------------------------------------
File    : USB_PrinterClass.c
Purpose : Sample implementation of USB printer device class
----------Literature--------------------------------------------------
Universal Serial Bus Device Class Definition for Printing Devices
Version 1.1 January 2000
--------  END-OF-HEADER  ---------------------------------------------
*/
#ifndef USB_PRINTERCLASS_H__
#define USB_PRINTERCLASS_H__

#include "USB.h"

typedef struct {
  const char * (*pfGetDeviceIdString)(void);
  int          (*pfOnDataReceived)(const U8 * pData, unsigned NumBytes);
  U8           (*pfGetHasNoError)(void);
  U8           (*pfGetIsSelected)(void);
  U8           (*pfGetIsPaperEmpty)(void);
  void         (*pfOnReset)(void);
} USB_PRINTER_API;

void         USB_PRINTER_Init(USB_PRINTER_API * pAPI);
void         USB_PRINTER_Task(void);
#endif

/**************************** end of file ***************************/

