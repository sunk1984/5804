/*********************************************************************
*               SEGGER MICROCONTROLLER SYSTEME GmbH                  *
*       Solutions for real time microcontroller applications         *
**********************************************************************
*                                                                    *
*       (C) 2003 - 2010   SEGGER Microcontroller Systeme GmbH        *
*                                                                    *
*       www.segger.com     Support: support@segger.com               *
*                                                                    *
**********************************************************************
*                                                                    *
*       TCP/IP stack for embedded applications                       *
*                                                                    *
**********************************************************************
----------------------------------------------------------------------
File    : IP_NI_AT91SAM7X.h
Purpose : Driver specific header file for the ATMEL AT91SAM7X
--------  END-OF-HEADER  ---------------------------------------------
*/

#ifndef IP_NI_AT91SAM7X_H
#define IP_NI_AT91SAM7X_H

#if defined(__cplusplus)
extern "C" {  // Make sure we have C-declarations in C++ programs
#endif

/*********************************************************************
*
*       Public data
*
**********************************************************************
*/
extern const IP_HW_DRIVER IP_Driver_SAM7X;

/*********************************************************************
*
*       Public code
*
**********************************************************************
*/
void IP_NI_SAM7X_ConfigNumRxBuffers     (U16 NumRxBuffers);

#if defined(__cplusplus)
  }           // Make sure we have C-declarations in C++ programs
#endif

/********************************************************************/

#endif        // Avoid multiple inclusion

/*************************** End of file ****************************/



