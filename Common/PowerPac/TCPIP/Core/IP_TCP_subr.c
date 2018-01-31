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
File    : UP_TCP_subr.c
Purpose :
--------  END-OF-HEADER  ---------------------------------------------
*/

/* Additional Copyrights: */
/* Copyright 1997 - 2000 By InterNiche Technologies Inc. All rights reserved */
/* Copyright (c) 1982, 1986 Regents of the University of California. */
/* All rights reserved.
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



#include "IP_Int.h"
#include "IP_sockvar.h"

#ifdef INCLUDE_TCP  /* include/exclude whole file at compile time */

#include "IP_protosw.h"
#include "IP_TCP_pcb.h"
#include "IP_TCP.h"
#include "IP_mbuf.h"       /* BSD-ish Sockets includes */

IP_TCP_STAT IP_TCP_Stat;

QUEUE IP_FreeTCPCBQ;
QUEUE IP_FreeTCPIPHDRQ;

#ifdef TCP_BIGCWND      /* Support for Large initial congestion window */
int      use_default_cwnd = 1;         /* Flag to use this on all sockets */
U32      default_cwnd = (2*TCP_MSS);  /* initial cwnd value to use */
#endif   /* TCP_BIGCWND */

/*********************************************************************
*
*       tcp_init()
*
* Tcp initialization
*/
void tcp_init() {
  tcp_iss = 1;      /* wrong */
  IP_tcb.inp_next = IP_tcb.inp_prev = &IP_tcb;
}

/*********************************************************************
*
*       _TryAllocTCPCB
*
*  Function description
*    Allocate storage space for a TCP control block.
*    It is either fetched from the free queue or allocated.
*/
static struct tcpcb * _TryAllocTCPCB(void) {
  return (struct tcpcb*)IP_TryAllocWithQ(&IP_FreeTCPCBQ, sizeof(struct tcpcb));
}

/*********************************************************************
*
*       _FreeTCPCB
*
*  Function description
*    Frees storage space for a TCPIPHDR by adding it to the free queue.
*/
static void _FreeTCPCB(struct tcpcb * p) {
  IP_Q_Add(&IP_FreeTCPCBQ, p);
}

/*********************************************************************
*
*       _TryAllocTCPIPHDR
*
*  Function description
*    Allocate storage space for a TCPIP header
*    It is either fetched from the free queue or allocated.
*/
static struct tcpiphdr * _TryAllocTCPIPHDR(void) {
  return (struct tcpiphdr*)IP_TryAllocWithQ(&IP_FreeTCPIPHDRQ, sizeof(struct tcpiphdr));
}
/*********************************************************************
*
*       IP_FreeTCPIPHDR
*
*  Function description
*    Frees storage space for a TCPIPHDR by adding it to the free queue.
*/
static void _FreeTCPIPHDR(struct tcpiphdr * p) {
  IP_Q_Add(&IP_FreeTCPIPHDRQ, p);
}

/*********************************************************************
*
*       tcp_template()
*
*  Function description
*    Create template to be used to send tcp packets on a connection.
*    Call after host entry created, allocates an mbuf and fills
*    in a skeletal tcp/ip header, minimizing the amount of work
*    necessary when the connection is used.
*/
struct tcpiphdr * tcp_template(struct tcpcb * tp) {
  struct inpcb * inp   =  tp->t_inpcb;
  struct tcpiphdr * n;

  n = tp->t_template;
  if (n == 0) {
    n = (struct tcpiphdr *)_TryAllocTCPIPHDR();
    if (n == NULL) {
      return NULL;
    }
  }
  IP_MEMSET(n, 0, sizeof(struct tcpiphdr));
  n->ti_len = htons(sizeof (struct tcpiphdr) - sizeof (struct ip));
  n->ti_src = inp->inp_laddr;
  n->ti_dst = inp->inp_faddr;
  n->ti_sport = inp->inp_lport;
  n->ti_dport = inp->inp_fport;
  n->ti_t.th_doff = (5 << 4);   /* NetPort */
  return n;
}


/*********************************************************************
*
*       tcp_respond()
*
* Send a single message to the TCP at address specified by
* the given TCP/IP header.  If flags==0, then we make a copy
* of the tcpiphdr at ti and send directly to the addressed host.
* This is used to force keep alive messages out using the TCP
* template for a connection tp->t_template.  If flags are given
* then we send a message back to the TCP which originated the
* segment ti, and discard the mbuf containing it and any other
* attached mbufs.
*
* In any case the ack and sequence number of the transmitted
* segment are as specified by the parameters.
*
*/
#define  EXCHANGE(a,b,type) {  type  t; t=a;  a=b;  b=t;  }

void tcp_respond(struct tcpcb * tp, struct tcpiphdr * ti, U32  ack, U32  seq, int   flags, struct mbuf *  ti_mbuf) {
  int      tlen;       /* tcp data len - 0 or 1 */
  int      win = 0;    /* window to use in sent packet */
  struct mbuf *  m;    /* mbuf to send */
  struct tcpiphdr * tmp_thdr;   /* scratch */
  struct ip * pip;

  if (tp) {
    win = (int)sbspace(&tp->t_inpcb->inp_socket->so_rcv);
  }

  //
  // Figure out of we can recycle the passed buffer or if we need a
  // new one. Construct the easy parts of the the TCP and IP headers.
  //
  if (flags == 0) {  /* sending keepalive from timer */
    /* no flags == need a new buffer */
    m = MBUF_GET_WITH_DATA(MT_HEADER, IP_TCP_HEADER_SIZE + 4);  // Enough for a full packet incl. all headers + 1 byte of data (and padding)
    if (m == NULL) {
      return;
    }
    m->m_data += tp->t_inpcb->ifp->n_lnh;
    tlen = 1;   /* Keepalives have one byte of data */
    m->m_len = TCPIPHDRSZ + tlen;
    /*
     * Copy template contents into the mbuf and set ti to point
     * to the header structure in the mbuf.
     */
    tmp_thdr = (struct tcpiphdr *)((char *)m->m_data + sizeof(struct ip) - sizeof(struct ipovly));
    if ((char *)tmp_thdr < m->pkt->pBuffer) {
      IP_PANIC("tcp_respond- packet ptr underflow");
    }
    IP_MEMCPY(tmp_thdr, ti, sizeof(struct tcpiphdr));
    ti = tmp_thdr;
    flags = TH_ACK;
  } else {  /* Flag was passed (e.g. reset); recycle passed mbuf */
    m = ti_mbuf;   /*dtom(ti);*/
    m_freem(m->m_next);
    m->m_next = 0;
    tlen = 0;         /* NO data */
    m->m_len = TCPIPHDRSZ;
    EXCHANGE(ti->ti_dport, ti->ti_sport, U16);
    EXCHANGE(ti->ti_dst.s_addr, ti->ti_src.s_addr, U32);
    if (flags & TH_RST) { /* count resets in MIB */
      IP_STAT_INC(tcpmib.tcpOutRsts);   /* keep MIB stats */
    }
  }

  //
  // Finish constructing the TCP header
  //
  ti->ti_seq = IP_HTONL_FAST(seq);
  ti->ti_ack = IP_HTONL_FAST(ack);
  ti->ti_t.th_doff = 0x50;      /* NetPort: init data offset bits */
  ti->ti_flags = (U8)flags;
  ti->ti_win = htons((U16)win);
  ti->ti_urp = 0;

   /* Finish constructing IP header and send, based on IP type in use */

  pip = (struct ip *)((char*)ti+sizeof(struct ipovly)-sizeof(struct ip));
  pip->ip_len = (U16)(TCPIPHDRSZ + tlen);

  /* If our system's max. MAC header size is geater than the size
  * of the MAC header in the received packet then we need to
  * adjust the IP header offset to allow for this. Since the packets
  * are only headers they should always fit.
  */
  if (pip >= (struct ip *)(m->pkt->pBuffer + IP_aIFace[0].n_lnh)) {
    m->m_data = (char*)pip; /* headers will fit, just set pointer */
  } else {     /* MAC may not fit, adjust pointer and move headers back */
    m->m_data = m->pkt->pData = m->pkt->pBuffer + IP_aIFace[0].n_lnh;  /* new ptr */
    IP_MEMMOVE(m->m_data, pip, TCPIPHDRSZ);  /* move back tcp/ip headers */
  }
  ip_output(m);
}

/*********************************************************************
*
*       tcp_newtcpcb()
*
* Create a new TCP control block, making an
* empty reassembly queue and hooking it to the argument
* protocol control block.
*
*/
struct tcpcb * tcp_newtcpcb(struct inpcb * inp) {
   struct tcpcb * tp;
   short t_time;

   tp = _TryAllocTCPCB();
   IP_LOG((IP_MTYPE_TCP_OPEN, "TCP: New TCP connection: %x", tp));
   if (tp == NULL) {
     return tp;
   }

   tp->seg_next = tp->seg_prev = (struct tcpiphdr *)tp;
   tp->t_flags = TF_NODELAY;        /* sends options! */
   tp->t_inpcb = inp;
   tp->OptLen  = 20;                       // Use a safe (max.) value. After Options are negotiated, this will be set to the correct (smaller) value.
   tp->Mss     = IP_aIFace[0].n_mtu - 40;  // Set initial value for the stack internal used maximum segment size. The initial "true" MSS is: MSS - 40
   /*
    * Init srtt to TCPTV_SRTTBASE (0), so we can tell that we have no
    * rtt estimate.  Set rttvar so that srtt + 2 * rttvar gives
    * reasonable initial retransmit time.
    */
   tp->t_srtt = TCPTV_SRTTBASE;
   tp->t_rttvar = TCPTV_SRTTDFLT << 2;

   t_time = ((TCPTV_SRTTBASE >> 2) + (TCPTV_SRTTDFLT << 2)) >> 1;
   IP_TCP_SetRetransDelay(tp, t_time);
   /* Set initial congestion window - RFC-2581, pg 4. */
   tp->snd_cwnd = 2 * tp->Mss;
   tp->TrueMSS  = tp->Mss - 40;            // Set initial value for the stack internal used maximum segment size. The initial "true" MSS is: MSS - 40
#ifdef TCP_BIGCWND
   /* See if we should set a large initial congestion window (RFC-2518) */
   if((use_default_cwnd) || (inp->inp_socket->so_options & SO_BIGCWND)) {
     tp->snd_cwnd = default_cwnd;
   }
#endif
#if TCP_WIN_SCALE
   /* set TCP window scaling factor to outgoing window. The window we are
    * sending is what we are silling to receive, so rfc1323 calls this the
    * "rcv.wind.scale". We will try to negotiate this on the syn/ack
    * handshake and back off to zero if it fails.
    */
   tp->rcv_wind_scale = default_wind_scale;
#endif   /* TCP_WIN_SCALE */

#ifdef DO_DELAY_ACKS
   // Init delay for delayed acknowledges. Make sure that the value is at least 1.
   tp->DelayAckPeriod = IP_TCP_DELAY_ACK_DEFAULT;
#endif   /* DO_DELAY_ACKS */

   tp->snd_ssthresh = 65535;  /* Start with high slow-start threshold */
   inp->inp_ppcb = tp;
   return tp;
}


/*********************************************************************
*
*       tcp_drop()
*
* Drop a TCP connection, reporting
* the specified error.  If connection is synchronized,
* then send a RST to peer.
*
*/
struct tcpcb * tcp_drop(struct tcpcb * tp, int err) {
  struct socket *   so =  tp->t_inpcb->inp_socket;

  if (TCPS_HAVERCVDSYN(tp->t_state)) {
    tp->t_state = TCPS_CLOSED;
    tcp_output(tp);
    IP_STAT_INC(IP_TCP_Stat.drops);
  } else {
   IP_STAT_INC(IP_TCP_Stat.conndrops);
  }
  so->so_error = err;
#ifdef TCP_ZEROCOPY
  if (so->rx_upcall) {
    so->rx_upcall(IP_SOCKET_p2h(so), NULL, err);
  }
#endif   /* TCP_ZEROCOPY */
  return tcp_close(tp);
}


/*********************************************************************
*
*       tcp_close()
*
* Close a TCP control block:
*   discard all space held by the tcp
*   discard internet protocol block
*   wake up any sleepers
*/

struct tcpcb * tcp_close(struct tcpcb * tp) {
  struct tcpiphdr * t;
  struct inpcb * inp   =  tp->t_inpcb;
  struct socket *   so =  inp->inp_socket;
  struct mbuf *  m;

  IP_LOG((IP_MTYPE_TCP_CLOSE, "TCP: TCP control block closed: %x", tp));
  t = tp->seg_next;
  while (t != (struct tcpiphdr *)tp) {
    t = (struct tcpiphdr *)t->ti_next;
    m = dtom(t->ti_prev);
    remque(t->ti_prev);
    m_freem (m);
  }
  if (tp->t_template) {
    _FreeTCPIPHDR(tp->t_template);
  }
  _FreeTCPCB(tp);
  inp->inp_ppcb = 0;
  soisdisconnected(so);
  IP_TCP_PCB_detach(inp);
  IP_STAT_INC(IP_TCP_Stat.closed);
  return ((struct tcpcb *)0);
}


/*********************************************************************
*
*       tcp_quench()
*
* When a source quench is received, close congestion window
* to one segment.  We will gradually open it again as we proceed.
*
*
* PARAM1: struct inpcb *inp
*
* RETURNS:
*/
void tcp_quench(struct inpcb * inp) {
  struct tcpcb * tp;

  tp = inp->inp_ppcb;

  if (tp) {
    tp->snd_cwnd = tp->Mss;
  }
}


#endif /* INCLUDE_TCP */

/*************************** End of file ****************************/

