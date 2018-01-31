/*********************************************************************
*               SEGGER MICROCONTROLLER SYSTEME GmbH                  *
*       Solutions for real time microcontroller applications         *
**********************************************************************
*                                                                    *
*       (C) 2003 - 2007   SEGGER Microcontroller Systeme GmbH        *
*                                                                    *
*       www.segger.com     Support: support@segger.com               *
*                                                                    *
**********************************************************************
*                                                                    *
*       USB device stack for embedded applications                   *
*                                                                    *
**********************************************************************
----------------------------------------------------------------------
File    : USB_Conf.h
Purpose : Config file. Modify to reflect your configuration
--------  END-OF-HEADER  ---------------------------------------------
*/


#ifndef USB_CONF_H           /* Avoid multiple inclusion */
#define USB_CONF_H

#ifndef NULL
  #define NULL                   0
#endif


#ifdef USB_IS_HIGH_SPEED
  #define USB_MAX_PACKET_SIZE     512
#endif

#ifdef DEBUG
  #if DEBUG
    #define USB_DEBUG_LEVEL        0   // Debug level: 1: Support "Panic" checks, 2: Support warn & log
  #endif
#endif

#endif     /* Avoid multiple inclusion */
