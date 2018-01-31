/*********************************************************************
*  
*   IAR PowerPac
*
*   (c) Copyright IAR Systems 2010.  All rights reserved.
*
**********************************************************************
--------  END-OF-HEADER  ---------------------------------------------
*/

#ifndef _IP_CONF_H_
#define _IP_CONF_H_ 1

//
// Define IP_DEBUG: Debug level for IP stack
//                  0: No checks, no statistics      (Smallest and fastest code)
//                  1: Statistics & "Panic" checks
//                  2: Statistics, warn, log, panic  (Seriously bigger code)
//
#ifndef DEBUG
  #define DEBUG 0
#endif
#if DEBUG
  #ifndef   IP_DEBUG
    #define IP_DEBUG      2      // Default for debug builds
  #endif
#else
  #ifndef   IP_DEBUG
    #define IP_DEBUG      0      // Default for release builds
  #endif
#endif

//
// Inline OS function calls for higher performance in release builds
//
#if !IP_DEBUG
  #include "RTOS.h"
  extern OS_RSEMA IP_OS_RSema;
  #define IP_OS_DISABLE_INTERRUPT()   OS_DI()
  #define IP_OS_ENABLE_INTERRUPT()    OS_RestoreI()
  #define IP_OS_GET_TIME()            OS_GetTime32()
  #define IP_OS_LOCK()                OS_Use(&IP_OS_RSema)
  #define IP_OS_UNLOCK()              OS_Unuse(&IP_OS_RSema)
  #define IP_OS_WAIT_NET_EVENT(ms)    OS_EVENT_WaitTimed(&IP_OS_EventNet, ms);
#endif

//
// IAR ARM compiler related macros
//
#ifdef __ICCARM__
  #if ((__TID__ >> 4) & 0x0F) < 6   // For any ARM CPU core < v7, we will use optimized routines
    #include "SEGGER.h"
    U16 ARM_IP_cksum(void * ptr, unsigned NumHWords);
    #define IP_CKSUM(p, NumItems)                   ARM_IP_cksum((p), (NumItems))
    #define IP_MEMCPY(pDest, pSrc, NumBytes) SEGGER_ARM_memcpy(pDest, pSrc, NumBytes)    // Speed optimization: Our memcpy is much faster!
  #endif
#endif

//
// Default value is little endian
//
#ifndef   IP_IS_BIG_ENDIAN
  #define IP_IS_BIG_ENDIAN 0
#endif

#endif     // Avoid multiple inclusion
