/*********************************************************************
*               SEGGER MICROCONTROLLER SYSTEME GmbH                  *
*       Solutions for real time microcontroller applications         *
**********************************************************************
*                                                                    *
*       (C) 2003 - 2009   SEGGER Microcontroller Systeme GmbH        *
*                                                                    *
*       www.segger.com     Support: support@segger.com               *
*                                                                    *
**********************************************************************
*                                                                    *
*       TCP/IP stack for embedded applications                       *
*                                                                    *
**********************************************************************
----------------------------------------------------------------------
File    : IP_NI_STM32F107.h
Purpose : Driver specific header file for the ST STM32F107 (Barracuda)
--------  END-OF-HEADER  ---------------------------------------------
*/

#ifndef IP_NI_STM32F107_H
#define IP_NI_STM32F107_H

#if defined(__cplusplus)
extern "C" {     /* Make sure we have C-declarations in C++ programs */
#endif

/*********************************************************************
*
*       Public data
*
**********************************************************************
*/
extern const IP_HW_DRIVER IP_Driver_STM32F107;

/*********************************************************************
*
*       Public code
*
**********************************************************************
*/
void IP_NI_STM32F107_ConfigNumRxBuffers(U16 NumRxBuffers);

#if defined(__cplusplus)
  }    // Make sure we have C-declarations in C++ programs
#endif

/********************************************************************/

#endif // Avoid multiple inclusion

/*************************** End of file ****************************/



