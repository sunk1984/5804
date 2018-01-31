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
File    : IP_ConfDefaults.h
Purpose :
--------  END-OF-HEADER  ---------------------------------------------
*/

#ifndef IP_CONFDEFAULTS_H
#define IP_CONFDEFAULTS_H

#include "IP_Conf.h"


#ifndef   IP_DEBUG
  #define IP_DEBUG                  (0)
#endif

#ifndef   IP_MEMCPY
  #define IP_MEMCPY  memcpy
#endif

#ifndef   IP_MEMSET
  #define IP_MEMSET  memset
#endif

#ifndef   IP_MEMMOVE
  #define IP_MEMMOVE memmove
#endif

#ifndef   IP_MEMCMP
  #define IP_MEMCMP  memcmp
#endif

#ifndef   IP_CKSUM
  #define IP_CKSUM(p, NumHWords) IP_cksum(p, NumHWords)
#endif

#ifndef   IP_OPTIMIZE
  #define IP_OPTIMIZE
#endif

#ifndef   IP_IS_BIG_ENDIAN
  #define IP_IS_BIG_ENDIAN 0      // Little endian is default
#endif

#ifndef   IP_USE_PARA
  #define IP_USE_PARA(Para) (void)Para
#endif

#ifndef   IP_INCLUDE_STAT         // Allow override in IP_Conf.h
  #if IP_DEBUG > 0
    #define IP_INCLUDE_STAT 1     // Can be set to 0 to disable statistics for extremly small release builds
  #else
    #define IP_INCLUDE_STAT 0
  #endif
#endif

#ifndef IP_DEBUG_Q                // Allow override in IP_Conf.h
  #if IP_DEBUG
    #define IP_DEBUG_Q 1
  #else
    #define IP_DEBUG_Q 0
  #endif
#endif
//
// TCP retransmission range defaults
//
#ifndef   IP_TCP_RETRANS_MIN
  #define IP_TCP_RETRANS_MIN   200   // Min. delay for retransmit. Real delay is computed, this minimum applies only if computed delay is shorter.
#endif

#ifndef   IP_TCP_RETRANS_MAX
  #define IP_TCP_RETRANS_MAX  5000   // Max. delay for retransmit. Real delay is computed, this maximum applies only if computed delay is longer.
#endif

//
// TCP keep-alive defaults
//
#ifndef   IP_TCP_KEEPALIVE_INIT
  #define IP_TCP_KEEPALIVE_INIT     20000       // Initial connect keep alive, 20 sec.
#endif

#ifndef   IP_TCP_KEEPALIVE_IDLE
  #define IP_TCP_KEEPALIVE_IDLE     60000       // Default time before probing
#endif

#ifndef   IP_TCP_KEEPALIVE_PERIOD
  #define IP_TCP_KEEPALIVE_PERIOD   10000       // Default probe interval
#endif

#ifndef   IP_TCP_KEEPALIVE_MAX_REPS
  #define IP_TCP_KEEPALIVE_MAX_REPS     8       // Max probes before drop
#endif

#ifndef   IP_TCP_MSL
  #define IP_TCP_MSL                 2000       // Max segment lifetime
#endif


#define IP_TCP_DACK_PERIOD    10   // Time base for delayed acknowledges
#define IP_TCP_SLOW_PERIOD    10


#define INCLUDE_ARP          1   // Include Ethernet ARP ?
#define INCLUDE_ICMP         1   // Include ICMP || ping only
#define INCLUDE_TCP          1
#define INCLUDE_UDP          1
#define TCP_ZEROCOPY         1  // Enable zero-copy Socket extension
#define TCP_TIMESTAMP        1  // Are we using RFC-1323 TCP timestamp feature to compute RTT ?
#ifndef   IP_SUPPORT_MULTICAST
  #define IP_SUPPORT_MULTICAST  0   // Experimental
#endif

#ifndef IP_MAX_DNS_SERVERS
  #define IP_MAX_DNS_SERVERS  2
#endif

#ifndef IP_PANIC
  #if   IP_DEBUG
    #define IP_PANIC(s)     IP_Panic(s)
  #else
    #define IP_PANIC(s)
  #endif
#endif

#ifndef   IP_SUPPORT_LOG
  #if   IP_DEBUG > 1
    #define IP_SUPPORT_LOG  1
  #else
    #define IP_SUPPORT_LOG  0
  #endif
#endif

#ifndef   IP_SUPPORT_WARN
  #if   IP_DEBUG > 1
    #define IP_SUPPORT_WARN  1
  #else
    #define IP_SUPPORT_WARN  0
  #endif
#endif



#if   IP_INCLUDE_STAT
  #define IP_STAT_DEC(Cnt)     (Cnt)--
  #define IP_STAT_INC(Cnt)     (Cnt)++
  #define IP_STAT_ADD(Cnt, v) { Cnt += v; }
#else
  #define IP_STAT_DEC(Cnt)
  #define IP_STAT_INC(Cnt)
  #define IP_STAT_ADD(Cnt, v)
#endif

#define TPS        1000    /* cticks per second */

#define IP_TTL        64 /* define IP hop count for this port */
#define IP_TCP_DELAY_ACK_DEFAULT 200       // [ms]

#define DO_DELAY_ACKS  (1)   // Defining enables delayed acks
//#define TCP_SACK       (1)
#define TCP_WIN_SCALE  (0)     // We do not want to use it per default, since it requires a sufficiently big buffer in the hardware.
#define IP_PTR_OP_IS_ATOMIC       (1)

#ifndef   IP_MAX_IFACES
  #define IP_MAX_IFACES       (1)  /* max ifaces to support at one time */
#endif

#endif // Avoid multiple inclusion

/*************************** End of file ****************************/



