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
File    : IP_TCP_pcb.h
Purpose : Handles protocol control block (for incoming TCP packets)
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



#ifndef _IN_PCB_H
#define  _IN_PCB_H   1

#if defined(__cplusplus)
extern "C" {     /* Make sure we have C-declarations in C++ programs */
#endif

/*
 * Common structure pcb for internet protocol implementation.
 * Here are stored pointers to local and foreign host table
 * entries, local and foreign socket numbers, and pointers
 * up (to a socket structure) and down (to a protocol-specific)
 * control block.
 */

typedef struct inpcb {
   struct inpcb *  inp_next;
   struct inpcb *  inp_prev;   /* list links */
   struct inpcb *  inp_head;      /* chain of inpcb's for this protocol */

   struct   in_addr  inp_faddr;  /* foreign host table entry */
   struct   in_addr  inp_laddr;  /* local host table entry */
   U16      inp_fport;           /* foreign port */
   U16      inp_lport;           /* local port */
   struct   socket * inp_socket; /* back pointer to socket */
   struct tcpcb * inp_ppcb;            /* pointer to per-protocol (TCP, UDP) pcb */
   NET     *ifp;                 /* interface if connected */
} IP_INPCB;

#define     INPLOOKUP_WILDCARD   1
#define     INPLOOKUP_SETLOCAL   2

int   IP_TCP_PCB_alloc          (struct socket *, IP_INPCB *);
int   IP_TCP_PCB_bind           (IP_INPCB *, struct mbuf *);
int   IP_TCP_PCB_connect        (IP_INPCB *, struct mbuf *);
void  IP_TCP_PCB_disconnect     (IP_INPCB *);
void  IP_TCP_PCB_detach         (IP_INPCB *);
void  in_setsockaddr       (IP_INPCB *, struct mbuf *);
void  in_setpeeraddr       (IP_INPCB *, struct mbuf *);
IP_INPCB * IP_TCP_PCB_lookup    (IP_INPCB *, U32,  U16, U32,  U16, int);

#if defined(__cplusplus)
  }
#endif

#endif // Avoid multiple inclusion

/*************************** End of file ****************************/


