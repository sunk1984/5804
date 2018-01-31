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
File    : IP_sockvar.h
Purpose : Kernel structure per socket.
          Contains send and receive buffer queues,
          handle on protocol and pointer to protocol
          private data and error information.
--------  END-OF-HEADER  ---------------------------------------------
*/

/* Additional Copyrights: */
/* Copyright 1997-2003 by InterNiche Technologies Inc. All rights reserved. */
/* Copyright (c) 1982, 1986 Regents of the University of California.
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions
* are met:
* 1. Redistributions of source code must retain the above copyright
*    notice, this list of conditions and the following disclaimer.
* 2. Redistributions in binary form must reproduce the above copyright
*    notice, this list of conditions and the following disclaimer in the
*    documentation and/or other materials provided with the distribution.
* 3. [rescinded 22 July 1999]
* 4. Neither the name of the University nor the names of its contributors
*    may be used to endorse or promote products derived from this software
*    without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
* ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
* ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
* FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
* DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
* OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
* HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
* LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
* OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
* SUCH DAMAGE.
*/



#ifndef SOCKVAR_H
#define  SOCKVAR_H   1


#if defined(__cplusplus)
extern "C" {     /* Make sure we have C-declarations in C++ programs */
#endif

/*
 * Variables for socket buffering.
 */
struct   sockbuf {
   U32      NumBytes;        /* actual chars in buffer */
   U32      Limit;        /* max actual char count */
   unsigned Timeout;   /* Snd/Rcv timeout */
   short    sb_flags;   /* flags, see below */
   struct mbuf * sb_mb; /* the   mbuf  chain */
};

struct socket {
   struct   socket * next; /* ptr to next socket */
   struct   inpcb * so_pcb;      /* protocol control block */
   struct   protosw * so_proto;  /* protocol handle */
#ifdef TCP_ZEROCOPY
   int   (*rx_upcall)(long s, void * pkt, int error);
#endif
   long     Handle;
   U32      so_options;    /* from socket call, see socket.h*/
   int      so_domain;     /* AF_INET or AF_INET6 */
   int      so_error;      /* error affecting connection */
   int      so_req;        /* request for pass to tcp_usrreq() */
   short    so_linger;     /* time to linger while closing */
   short    so_state;      /* internal state flags SS_*, below */
   unsigned    so_timeo;      /* connection timeout */
   char     so_type;       /* generic type, see socket.h   */
   char     so_hopcnt;     /* Number of hops to dst   */

   /*
    * Variables for socket buffering.
    */
   struct   sockbuf  so_rcv;
   struct   sockbuf  so_snd;
   /*
    * Variables for connection queueing.
    * Socket where accepts occur is so_head in all subsidiary sockets.
    * If so_head is 0, socket is not related to an accept.
    * For head socket so_q0 queues partially completed connections,
    * while so_q is a queue of connections ready to be accepted.
    * If a connection is aborted and it has so_head set, then
    * it has to be pulled out of either so_q0 or so_q.
    * We allow connections to queue up based on current queue lengths
    * and limit on number of queued connections for this socket.
    */
   struct   socket * so_head; /* back pointer to accept socket */
   struct   socket * so_q0;   /* queue of partial connections */
   struct   socket * so_q;    /* queue of incoming connections */
   char     so_q0len;         /* partials on so_q0 */
   char     so_qlen;          /* number of connections on so_q */
   char     so_qlimit;        /* max number queued connections */
   struct   ip_socopts * so_optsPack; /* pointer to options, to be given to PACKET */
};

/* sockbuf defines */
#define  SB_MAX      (16*1024)   /* max chars in sockbuf */
#define  SB_LOCK     0x01     /* lock on data queue (so_rcv only) */
#define  SB_WANT     0x02     /* someone is waiting to lock */
#define  SB_WAIT     0x04     /* someone is waiting for data/space */
#define  SB_SEL      0x08     /* buffer is selected */
#define  SB_COLL     0x10     /* collision selecting */
#define  SB_NOINTR   0x40     /* operations not interruptible */
#define  SB_MBCOMP   0x80     /* can compress mbufs */

/*
 * Socket state bits.
 */
#define  SS_NOFDREF           0x0001   /* no file table ref any more */
#define  SS_ISCONNECTED       0x0002   /* socket connected to a peer */
#define  SS_ISCONNECTING      0x0004   /* in process of connecting to peer */
#define  SS_ISDISCONNECTING   0x0008   /*  in process  of disconnecting */
#define  SS_CANTSENDMORE      0x0010   /* can't send more data to peer */
#define  SS_CANTRCVMORE       0x0020   /* can't receive more data from peer */
#define  SS_RCVATMARK         0x0040   /* at mark on input */
#define  SS_PRIV              0x0080   /* privileged for broadcast, raw... */
#define  SS_NBIO              0x0100   /* non-blocking ops */
#define  SS_ASYNC             0x0200   /* async i/o notify */
#define  SS_UPCALLED          0x0400   /* zerocopy data has been upcalled (for select) */
#define  SS_INUPCALL          0x0800   /* inside zerocopy upcall (reentry guard) */
#define  SS_UPCFIN            0x1000   /* inside zerocopy upcall (reentry guard) */
#define  SS_WASCONNECTING     0x2000   /* SS_ISCONNECTING w/possible pending error */

/*
 * Macros for sockets and socket buffering.
 */

int sbspace(struct   sockbuf * pSockBuf);

/* adjust counters in sb reflecting freeing of m */
#define   sbfree(sb, m) { \
   (sb)->NumBytes -= (m)->m_len; \
}

#define  sorwakeup(so)     sbwakeup(&(so)->so_rcv)
#define  sowwakeup(so)     sbwakeup(&(so)->so_snd)

void  soisconnecting (struct socket *);
void  soisconnected  (struct socket *);
void  soisdisconnecting (struct socket *);
void  soisdisconnected  (struct socket *);
struct socket *   sonewconn   (struct socket *);
void  soqinsque     (struct socket *, struct socket *, int);
int   soqremque     (struct socket *, int);
void  socantsendmore(struct socket *);
void  socantrcvmore (struct socket *);
void  sbreserve     (struct sockbuf *, unsigned long);
void  sbwakeup      (struct sockbuf *);
void  sbrelease     (struct sockbuf *);
void  sbselqueue    (struct sockbuf *);
void  sbappend      (struct sockbuf * sb, struct mbuf *);
void  sbappendrecord(struct sockbuf * sb, struct mbuf *);
int   sbappendaddr  (struct sockbuf * sb, struct sockaddr *, struct mbuf *);
void  sbflush       (struct sockbuf *);
void  sbdrop        (struct sockbuf *, int);
void  sbdropend     (struct sockbuf *, struct mbuf *);
void  sbdroprecord  (struct sockbuf *);
void  sbwait        (struct sockbuf *, I32 Timeout);
void  insque        (void *, void *);
void  remque        (void *);

int              soabort       (struct socket *);
void             sofree        (struct socket *);
void             sohasoutofband(struct socket *);

int   soreserve (struct socket * so, U32 sndcc, U32 rcvcc);
int   soreceive (struct socket * so, struct mbuf ** nam, char * data, int * datalen, int   flags);
void  sbcompress(struct sockbuf *, struct mbuf *, struct mbuf *);

#if defined(__cplusplus)
  }
#endif

#endif   /* SOCKVAR_H */

/* end of file sockvar.h */


