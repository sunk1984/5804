/*********************************************************************
*  
*   IAR PowerPac
*
*   (c) Copyright IAR Systems 2010.  All rights reserved.
*
**********************************************************************
----------------------------------------------------------------------
File        : USBH_ConfigIO.c
Purpose     : 
---------------------------END-OF-HEADER------------------------------
*/

/*********************************************************************
*
*       #include Section
*
**********************************************************************
*/
#include <stdlib.h>
#include "USBH_Int.h"
#include "RTOS.h"

/*********************************************************************
*
*       Defines, configurable
*
*       This section is normaly the only section which requires changes on most embedded systems
*/
#define USE_DCC        0
#define SHOW_TIME      1
#define SHOW_TASK      1

#if USE_DCC
  #include "JLINKDCC.h"
#endif

/*********************************************************************
*
*       _puts
*
*  Function description
*    Local (static) replacement for puts.
*    The reason why puts is not used is that puts always appends a NL character, which screws up our formatting.
*/
static void _puts(const char * s) {

    //UART_WriteFrame(s,strlen(s));
    //UART_WriteFrame("\r",1);
#if 0  
  char c;
  for (;;) {
    c = *s++;
    if (c == 0) {
      break;
    }
#if USE_DCC
    JLINKDCC_SendChar(c);
#else
    putchar(c);
#endif
  }
  
#endif
}

/*********************************************************************
*
*       _WriteUnsigned
*
*  Function description
*/
static char * _WriteUnsigned(char * s, U32 v, int NumDigits) {
  unsigned   Base;
  unsigned   Div;
  U32        Digit;
  Digit    = 1;
  Base     = 10;
  //
  // Count how many digits are required
  //
  while (((v / Digit) >= Base) | (NumDigits-- > 1)) {
    Digit *= Base;
  }
  //
  // Output digits
  //
  do {
    Div = v / Digit;
    v  -= Div * Digit;
    *s++ = (char)('0' + Div);
    Digit /= Base;
  } while (Digit);
  *s = 0;
  return s;
}

/*********************************************************************
*
*       _ShowStamp
*
*  Function description
*/
static void _ShowStamp(void) {
#if SHOW_TIME
  I32    Time;
  char   ac[20];
  char * sBuffer = &ac[0];
  Time           = OS_GetTime32();
  sBuffer        = _WriteUnsigned(sBuffer, Time / 1000, 0);
  *sBuffer++     = ':';
  sBuffer        = _WriteUnsigned(sBuffer, Time % 1000, 3);
  *sBuffer++     = ' ';
  *sBuffer++     = 0; 
  _puts(ac);
#endif

#if SHOW_TASK
{
  const char * s;
  s = OS_GetTaskName(OS_pCurrentTask);
  if (s) {
    _puts(s);
    _puts(" - ");
  }
}
#endif
}

/*********************************************************************
*
*       USBH_Panic
*
*  Function description
*    Is called if the stack encounters a critical situation.
*    In a release build, this function may not be linked in.
*/
void USBH_Panic(const char * s) {
  USBH_OS_DisableInterrupt();
#if USBH_DEBUG > 1
  _puts("*** Fatal error, System halted: ");
  _puts(s);
  _puts("\n");
#endif
  while (1);
}

/*********************************************************************
*
*       USBH_Log
*
*  Function description
*    This function is called by the stack in debug builds with log output.
*    In a release build, this function may not be linked in.
*
*  Notes
*    (1)  Interrupts and task switches
*         printf() has a reentrance problem on  alot of systems if interrupts are not disabled.
*                  which would scramble strings if printf() called from an ISR interrupts an other printf.
*         In order to avoid this problem, interrupts are disabled.
*/
void USBH_Log(const char * s) {
  USBH_OS_DisableInterrupt();
  _ShowStamp();
  _puts(s);
  _puts("\n");
  USBH_OS_EnableInterrupt();
}

/*********************************************************************
*
*       USBH_Warn
*
*  Function description
*    This function is called by the stack in debug builds with log output.
*    In a release build, this function may not be linked in.
*
*  Notes
*    (1)  Interrupts and task switches
*         See USBH_Log()
*/
void USBH_Warn(const char * s) {
  USBH_OS_DisableInterrupt();
  _ShowStamp();
  _puts("*** Warning *** ");
  _puts(s);
  _puts("\n");
  USBH_OS_EnableInterrupt();
}

/********************************* EOF ******************************/
