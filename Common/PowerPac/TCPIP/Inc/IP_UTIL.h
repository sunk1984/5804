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
*       TCP/IP stack for embedded applications                       *
*                                                                    *
**********************************************************************
----------------------------------------------------------------------
File        : IP_UTIL.h
Purpose     : UTIL API
---------------------------END-OF-HEADER------------------------------
*/

#ifndef IP_UTIL_H
#define IP_UTIL_H

#if defined(__cplusplus)
extern "C" {     /* Make sure we have C-declarations in C++ programs */
#endif

int IP_UTIL_BASE64_Encode(const unsigned char * pSrc, int SrcLen, unsigned char *pDest, int * pDestLen);
int IP_UTIL_BASE64_Decode(const unsigned char * pSrc, int SrcLen, unsigned char *pDest, int * pDestLen);


#if defined(__cplusplus)
  }
#endif

#endif   // Avoid multiple inclusion



