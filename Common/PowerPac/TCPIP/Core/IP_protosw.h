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
File    : IP_protosw.h
Purpose : Protocol switch table.
          Each protocol has a handle initializing one of these structures,
          which is used for protocol-protocol and system-protocol communication.
          A protocol is called through the pr_init entry before any other.
          Thereafter it is called every 200ms through the pr_fasttimo entry and
          every 500ms through the pr_slowtimo for timer based actions.
          The system will call the pr_drain entry if it is low on space and
          this should throw away any non-critical data.
          Protocols pass data between themselves as chains of mbufs using
          the pr_input and pr_output hooks.  Pr_input passes data up (towards
          UNIX) and pr_output passes it down (towards the imps); control
          information passes up and down on pr_ctlinput and pr_ctloutput.
          The protocol is responsible for the space occupied by any the
          arguments to these entries and must dispose it.
--------  END-OF-HEADER  ---------------------------------------------
*/

/* Additional Copyrights: */
/* Copyright 1997 - 2000 By InterNiche Technologies Inc. All rights reserved */
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



#ifndef PROTOSW_H
#define  PROTOSW_H   1

#if defined(__cplusplus)
extern "C" {     /* Make sure we have C-declarations in C++ programs */
#endif

struct ifnet;  /* pre-declaration to supress warnings */

struct protosw {
  short    pr_type;             /* socket type used for */
  short    pr_protocol;         /* protocol number */
  short    pr_flags;            /* see below */
  /* protocol-protocol hooks */
#ifdef CTL_INPUT
  void     (*pr_ctlinput)(int, struct sockaddr *);             /* input to protocol (from below) */
#endif
#ifdef CTL_OUTPUT
  int      (*pr_ctloutput) (int, struct socket *, int, int, void *); /* control output (from above) */
#endif
  int      (*pr_usrreq) (struct socket *, struct mbuf *, struct mbuf *); /* user-protocol hook */
  /* user request: see list below */
  /* utility hooks */
  void  (*pr_init)(void);  /* initialization hook */
};

/*
 * Values for pr_flags
 */
#define     PR_ATOMIC   0x01     /* exchange atomic messages only */
#define     PR_ADDR        0x02  /* addresses given with messages */
/* in the current implementation, PR_ADDR needs PR_ATOMIC to work */
#define     PR_CONNREQUIRED   0x04     /* connection required by protocol */
#define     PR_WANTRCVD    0x08     /* want PRU_RCVD calls */
#define     PR_RIGHTS   0x10     /* passes capabilities */

/*
 * The arguments to usrreq are:
 *   (*protosw[].pr_usrreq)(up, req, m, nam, opt);
 * where up is a (struct socket *), req is one of these requests,
 * m is a optional mbuf chain containing a message,
 * nam is an optional mbuf chain containing an address,
 * and opt is a pointer to a socketopt structure or nil.
 * The protocol is responsible for disposal of the mbuf chain m,
 * the caller is responsible for any space held by nam and opt.
 * A non-zero return from usrreq gives an
 * UNIX error number which should be passed to higher level software.
 */
#define     PRU_ATTACH        0  /* attach protocol to up */
#define     PRU_DETACH        1  /* detach protocol from up */
#define     PRU_BIND          2  /* bind socket to address */
#define     PRU_LISTEN        3  /* listen for connection */
#define     PRU_CONNECT       4  /* establish connection to peer */
#define     PRU_ACCEPT        5  /* accept connection from peer */
#define     PRU_DISCONNECT    6  /* disconnect from peer */
#define     PRU_SHUTDOWN      7  /* won't send any more data */
#define     PRU_RCVD          8  /* have taken data; more room now */
#define     PRU_SEND          9  /* send this data */
#define     PRU_ABORT        10 /* abort (fast DISCONNECT, DETATCH) */
#define     PRU_CONTROL      11 /* control operations on protocol */
#define     PRU_SENSE        12    /* return status into m */
#define     PRU_SOCKADDR     13 /* fetch socket's address */
#define     PRU_PEERADDR     14 /* fetch peer's address */
#define     PRU_CONNECT2     15 /* connect two sockets */

/* begin for protocols internal use */
#define     PRU_SLOWTIMO      16 /* 500ms timeout */


struct protosw *  pffindtype  (int, int);
struct protosw *  pffindproto (int, int, int);

#if defined(__cplusplus)
  }
#endif

#endif   /* PROTOSW_H */

/* end of file protosw.h */


