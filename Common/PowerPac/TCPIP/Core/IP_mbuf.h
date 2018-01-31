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
File    : IP_mbuf.h
Purpose : This is the definitions file for NetPort mbufs. These
          are sytacticly used like BSD mbufs in the BSD derived C code,
          however the actual buffer space managed is mapped inthe a NetPort
          style IP_PACKET * structure. Concepts and Portions of this file are
          adapted from the BSD sources.
--------  END-OF-HEADER  ---------------------------------------------
*/

/* Additional Copyrights: */
/* Copyright 1997 - 2000 By InterNiche Technologies Inc. All rights reserved */
/* Portions Copyright 1996 by NetPort Software. All rights reserved. */


#ifndef _MBUF_H
#define  _MBUF_H  1

#if defined(__cplusplus)
extern "C" {     /* Make sure we have C-declarations in C++ programs */
#endif

/* mbuf struct - just a wrapper for pBuffer */

typedef struct mbuf {
   struct mbuf *  next;    /* queue link */
   IP_PACKET *   pkt;      /* the pBuffer, w/actual contiguous buffer */
   char *   m_data;        /* pointer to next data */
   struct mbuf *  m_next;  /* next mbuf in record (TCP) */
   struct mbuf *  m_act;   /* start of next record (UDP) */
   U16      m_len;         /* length of m_data */
#if IP_DEBUG
   U16      m_type;        /* as in UNIX; 0==free */
#endif
} MBUF;

/* mbuf types */
#define  MT_FREE     0     /* should be on free list */
#define  MT_RXDATA   1     /* dynamic (data) allocation */
#define  MT_TXDATA   2     /* dynamic (data) allocation */
#define  MT_HEADER   3     /* packet header */
#define  MT_SOCKET   4     /* socket structure */
#define  MT_PCB      5     /* protocol control block */
#define  MT_RTABLE   6     /* routing tables */
#define  MT_HTABLE   7     /* IMP host tables */
#define  MT_ATABLE   8     /* address resolution tables */
#define  MT_SONAME   9     /* socket name */
#define  MT_SOOPTS   10    /* socket options */
#define  MT_FTABLE   11    /* fragment reassembly header */
#define  MT_RIGHTS   12    /* access rights */
#define  MT_IFADDR   13    /* interface address */

#define  CLBYTES     1400
#define  M_COPYALL   -1


void     m_adj(struct mbuf * mp, int len);
int      mbuf_len (struct mbuf * m);

#if IP_DEBUG
  struct mbuf * IP_MBUF_Get (int type, int len);
  #define MBUF_GET(Type)                      IP_MBUF_Get(Type, 0)
  #define MBUF_GET_WITH_DATA(Type, NumBytes)  IP_MBUF_Get(Type, NumBytes)
#else
  struct mbuf * IP_MBUF_Get (int len);
  #define MBUF_GET(Type)                      IP_MBUF_Get(0)
  #define MBUF_GET_WITH_DATA(Type, NumBytes)  IP_MBUF_Get(NumBytes)
#endif


#define  MFREE(m, n)    ((n)  =  m_free(m))
#define  MTOD(mb, cast)    (cast)(mb->m_data)

struct mbuf *  m_copy  (struct mbuf * pMBuf, int off, int len, unsigned Prepend);
struct mbuf *  m_free  (struct mbuf * pMBuf);
void           m_freem (struct mbuf * pMBuf);
struct mbuf *  dtom  (void *);

#if defined(__cplusplus)
  }
#endif

#endif   /* _MBUF_H */

/* end of file mbuf.h */



