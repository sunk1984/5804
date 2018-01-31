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
File    : IP_NI_LAN91C111.h
Purpose : Driver specific header file for the SMSC LAN91C111
--------  END-OF-HEADER  ---------------------------------------------
*/

#ifndef IP_NI_LAN9118_H
#define IP_NI_LAN9118_H

#if defined(__cplusplus)
extern "C" {  // Make sure we have C-declarations in C++ programs
#endif

/*********************************************************************
*
*       Public data
*
**********************************************************************
*/
extern const IP_HW_DRIVER IP_Driver_LAN91C111;

/*********************************************************************
*
*       Public code
*
**********************************************************************
*/
void IP_NI_LAN91C111_ISR_Handler(unsigned Unit);
void IP_NI_LAN91C111_ConfigAddr (unsigned Unit, void* pBase);


#if defined(__cplusplus)
  }           // Make sure we have C-declarations in C++ programs
#endif

/********************************************************************/

#endif        // Avoid multiple inclusion

/*************************** End of file ****************************/



