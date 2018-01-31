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
File    : IP_ICMP.h
Purpose : ICMP message handling
--------  END-OF-HEADER  ---------------------------------------------
*/

/* Additional Copyrights: */
/* Copyright  2000 By InterNiche Technologies Inc. All rights reserved */
/* Portions Copyright 1990,1993 by NetPort Software. */
/* Portions Copyright 1986 by Carnegie Mellon */
/* Portions Copyright 1983 by the Massachusetts Institute of Technology */

#ifndef _ICMP_H_
#define  _ICMP_H_ 1

#if defined(__cplusplus)
extern "C" {     /* Make sure we have C-declarations in C++ programs */
#endif

/* Define some ICMP messages */

/* ICMP dest unreachable types */
#define  IP_ICMP_DU_NET      0
#define  IP_ICMP_DU_HOST     1
#define  IP_ICMP_DU_PROT     2
#define  IP_ICMP_DU_PORT     3
#define  IP_ICMP_DU_FRAG     4
#define  IP_ICMP_DU_SRC      5


#define  IP_ICMP_ECHO_REP    0     /* ICMP Echo reply */
#define  IP_ICMP_DEST_UN     3     /* Destination Unreachable */
#define  IP_ICMP_SOURCEQ     4     /* Source quench */
#define  IP_ICMP_REDIR       5     /* Redirect */
#define  IP_ICMP_ECHO_REQ    8     /* ICMP Echo request */
#define  IP_ICMP_TIMEX       11    /* Time exceeded */
#define  IP_ICMP_PARAM       12    /* Parameter problem */
#define  IP_ICMP_TIME_REQ    13    /* Timestamp request */
#define  IP_ICMP_TIME_REP    14
#define  IP_ICMP_INFO        15    /* Information request */


#define  ICMPSIZE    sizeof(struct  ping) /* default size for ICMP packet */

struct ping {         /* ICMP Echo request/reply header */
  char     ptype;
  char     pcode;
  U16      pchksum;
  U16      pid;
  U16      pseq;
};

/* structure of an icmp destination unreachable packet */

#define  ICMPDUDATA     8  /* size of extra data */

struct destun {
   char     dtype;
   char     dcode;
   U16      dchksum;
   U16      dno1;
   U16      dno2;
   struct ip   dip;    /* the offending IP packet */
   char     ddata[ICMPDUDATA];
};

/*********************************************************************
*
*       IP_ICMP_MIB
*
* The snmp icmp variables
* No comments for self explanitory elements
*/
typedef struct {
  U32   InMsgs;          // 1 - Received icmp packets, including errors
  U32   InErrors;        // 2 - Bad sums, bad len, etc.
  U32   InDestUnreachs;  // 3
  U32   InTimeExcds;     // 4
  U32   InParmProbs;     /* 5 */
  U32   InSrcQuenchs;    /* 6 */
  U32   InRedirects;     /* 7 */
  U32   InEchos;         /* 8 */
  U32   InEchoReps;      /* 9 */
  U32   InTimestamps;    /* 10 */
  U32   InTimestampReps; /* 11 */
  U32   InAddrMasks;     /* 12 */
  U32   InAddrMaskReps;  /* 13 */
  U32   OutMsgs;         /* 14 - total sent s, including errors */
  U32   OutErrors;       /* 15 - ICMP Layer errors ONLY (see rfc #1156) */
  U32   OutDestUnreachs; /* 16 */
  U32   OutTimeExcds;    /* 17 */
  U32   OutParmProbs;    /* 18 */
  U32   OutSrcQuenchs;   /* 19 */
  U32   OutRedirects;    /* 20 */
  U32   OutEchos;        /* 21 */
  U32   OutEchoReps;     /* 22 */
  U32   OutTimestamps;   /* 23 */
  U32   OutTimestampReps;/* 24 */
  U32   OutAddrMasks;    /* 25 */
  U32   OutAddrMaskReps; /* 26 */
} IP_ICMP_MIB;

extern IP_ICMP_MIB IP_ICMP_Mib;

void  IP_ICMP_OnRx(IP_PACKET *);
void  icmp_destun(ip_addr, struct ip *, unsigned, NET *);
void  icmp_du(IP_PACKET * p, struct destun * pdp);

/* Figure out a minimum value */
#define PINGHDRSMINLEN (34 + 8 + ETHHDR_BIAS)
#define  PINGHDRSLEN  ((PINGHDRSMINLEN + 3) & ~ 3)    // Adjust to 4 bytes

#if defined(__cplusplus)
  }
#endif

#endif   /* _ICMP_H_ */



