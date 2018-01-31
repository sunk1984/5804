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
File    : IP_TCP_in.c
Purpose :
--------  END-OF-HEADER  ---------------------------------------------
*/

/* Additional Copyrights: */
/* Copyright 1997- 2000 By InterNiche Technologies Inc. All rights reserved */
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

#include "IP_TCP_pcb.h"
#include "IP_TCP.h"
#include "IP_mbuf.h"       /* BSD-ish Sockets includes */


#define  TRACE_RESET 0
#if      TRACE_RESET  /* this can be usefull for tracing tcp_input() logic: */
  int      dropline;
  #define  GOTO_DROP   {  dropline=__LINE__;   goto  drop; }
  #define  GOTO_DROPWITHRESET   {  dropline=__LINE__;   goto  dropwithreset; }
#else /* the production flavor: */
  #define  GOTO_DROP   {  goto  drop; }
  #define  GOTO_DROPWITHRESET   {  goto  dropwithreset; }
#endif

struct   tcpiphdr tcp_saveti;

/*********************************************************************
*
*       _UpdateRTT()
*
*  Function description
*    Updates the RTT (round trip time) and SRTT (Smoothed round trip time) values
*    These are used to compute retransmission times
*
*    Add. notes
*      (1) RFC793 specifies:
*        Retransmission Timeout
*          Because of the variability of the networks that compose an
*          internetwork system and the wide range of uses of TCP connections the
*          retransmission timeout must be dynamically determined.  One procedure
*          for determining a retransmission time out is given here as an illustration.
*
*        An Example Retransmission Timeout Procedure
*          Measure the elapsed time between sending a data octet with a
*          particular sequence number and receiving an acknowledgment that
*          covers that sequence number (segments sent do not have to match
*          segments received).  This measured elapsed time is the Round Trip Time (RTT).
*          Next compute a Smoothed Round Trip Time (SRTT) as:
*            SRTT = ( ALPHA * SRTT ) + ((1-ALPHA) * RTT)
*          and based on this, compute the retransmission timeout (RTO) as:
*          RTO = min[UBOUND,max[LBOUND,(BETA*SRTT)]]
*          where UBOUND is an upper bound on the timeout (e.g., 1 minute),
*          LBOUND is a lower bound on the timeout (e.g., 1 second), ALPHA is
*          a smoothing factor (e.g., .8 to .9), and BETA is a delay variance factor (e.g., 1.3 to 2.0).
*/
static void _UpdateRTT(struct tcpcb * tp) {
   int Delta;
   int rtt;
   I32 TimeStamp;
   I32 v;

   TimeStamp = tp->t_rttick;
   if (TimeStamp == 0) {
     IP_WARN((IP_MTYPE_TCP_RTT, "TCP_RTT: Tick = 0"));
     return;
   }
   IP_STAT_INC(IP_TCP_Stat.rttupdated);

   //
   // get  this rtt.
   //
   Delta = IP_OS_GetTime32() - TimeStamp;
   IP_LOG((IP_MTYPE_TCP_RTT, "TCP_RTT: %dms", Delta));

   rtt = Delta;
   if (tp->t_srtt != 0) {
     if (rtt == 0) {     /* fast path for small round trip */
       /* if either the rtt or varience is over 1, reduce it. */
       if (tp->t_srtt > 1) {
         tp->t_srtt--;
       }
       if (tp->t_rttvar > 1) {
         tp->t_rttvar--;
       }
     } else {
      /*
       * srtt is stored as fixed point with 3 bits
       * after the binary point (i.e., scaled by 8).
       * The following magic is equivalent
       * to the smoothing algorithm in rfc793
       * with an alpha of .875
       * (srtt = rtt/8 + srtt*7/8 in fixed point).
       */
      Delta = ((rtt - 1) << 2) - (int)(tp->t_srtt >> 3);
      if ((tp->t_srtt += Delta) <= 0) {
        tp->t_srtt = 1;
      }
      /*
       * We accumulate a smoothed rtt variance (actually, a smoothed mean difference),
       * then set the retransmit timer to smoothed rtt + 2 times the smoothed variance.
       * rttvar is stored as fixed point with 2 bits after the binary point
       * (scaled by 4).  The following is equivalent to rfc793 smoothing with an alpha of .75
       * (rttvar = rttvar*3/4 + |Delta| / 4).
       * This replaces rfc793's wired-in beta.
       */
      if (Delta < 0) {
        Delta = -Delta;
      }
      Delta -= (short)(tp->t_rttvar >> 1);
      if ((tp->t_rttvar += Delta) <= 0)
        tp->t_rttvar = 1;
      }
   } else {  /* srtt is zero, nedd to initialize it */
      /*
       * No rtt measurement yet - use the
       * unsmoothed rtt.  Set the variance
       * to half the rtt (so our first
       * retransmit happens at 2*rtt)
       */
     if (rtt < 1) {
       rtt = 1;
     }
     tp->t_srtt = rtt << 3;
     tp->t_rttvar = rtt << 1;
   }
   IP_LOG((IP_MTYPE_TCP_RTT, "TCP_SRTT: %dms", tp->t_srtt / 8));
   tp->t_rttick = 0;       /* clear RT timer */
   tp->t_rxtshift = 0;
   v = ((tp->t_srtt >> 2) + tp->t_rttvar) >> 1;
   IP_TCP_SetRetransDelay(tp, v);
}


/*********************************************************************
*
*       _HandleOptions()
*
*  Function description
*    Parse received TCP options appended to the TCP header
*/
static void _HandleOptions(struct tcpcb * tp, struct mbuf *  om, struct tcpiphdr * ti) {
  U8 * cp;   /* pointer into option buffer */
  int   opt;     /* current option code */
  int   optlen;  /* length of current option */
  int   cnt;     /* byte count left in header */
  struct socket * so = tp->t_inpcb->inp_socket;
  #if TCP_TIMESTAMP
  int   gotstamp = 0;    /* TRUE if we got a timestamp */
  #endif   /* TCP_TIMESTAMP */

  cp = MTOD(om, U8 *);
  cnt = om->m_len;
  for (; cnt > 0; cnt -= optlen, cp += optlen) {
    opt = cp[0];
    if (opt == TCPOPT_EOL) {
      break;
    }
    if (opt == TCPOPT_NOP) {
      optlen = 1;
    } else {
      optlen = cp[1];
      if (optlen <= 0) {
        break;
      }
    }

    switch (opt) {
    case TCPOPT_MAXSEG:
    {
      U16 mssval;
      U16 OurMss;
      if (optlen != 4) {
        continue;
      }
      if ((ti->ti_flags & TH_SYN) == 0) {   /* MSS only on SYN */
        continue;
      }
      mssval = (U16)IP_LoadU16BE(cp + 2);
      OurMss = tp->Mss;
      if (OurMss == 0) {
        OurMss = tp->t_inpcb->ifp->n_mtu - 40;     // Use interface MTU - 40 as defined by RFC 879
      }
      if (mssval > OurMss) {
        mssval = OurMss;
      }
      tp->Mss = mssval;
      break;
    }
  #ifdef TCP_SACK
    case TCPOPT_SACKOK:
    {
      if(optlen != 2)
        continue;

      if (!(ti->ti_flags & TH_SYN))    /* only legal on SYN, SYN/ACK */
        continue;

      /* If SACK is OK with our socket and the other side then
      * set the t_flags bit which will allow it.
      */
      if (so->so_options & SO_TCPSACK) {
       tp->t_flags |= TF_SACKOK;
       IP_STAT_INC(IP_TCP_Stat.sackconn);
      }
      break;
    }
    case TCPOPT_SACKDATA:   /* got a SACK info packet */
      if (tp->t_flags & TF_SACKOK) {
        tcp_resendsack(cp, tp);
      }
      break;
  #endif   /* TCP_SACK */

  #if TCP_WIN_SCALE
    case TCPOPT_WINSCALE: {  /* window scale offer */
      if (optlen != 3) {
        continue;
      }
      if (!(ti->ti_flags & TH_SYN)) {    /* only legal on SYN, SYN/ACK */
        continue;
      }
      tp->snd_wind_scale = *(cp + 2);
      tp->t_flags |= TF_WINSCALE;
       /* Here's a a trick that's not in the literature. If the other side has
        * agreed to let us do scaled windows but sent set his scale as 0,
        * then it means he's not ever going to get his window over 64K, and
        * in practice. probably no where near that high. Some TCPs (like
        * Microsoft's Win 2K) get really confused by disparate window sizes.
        * If we put a lid on our size too, things work a lot better.
        */
      if (tp->snd_wind_scale == 0) {
        so->so_rcv.Limit = (32 * 1024);
      }
      break;
    }
  #endif /* TCP_WIN_SCALE */

#if TCP_TIMESTAMP
    case TCPOPT_RTT:     /* round trip time option */
      if (optlen != 10) {
        continue;
      }
      if ((so->so_options & SO_TIMESTAMP) == 0) {
        continue;
      }

      gotstamp = 1;     /* set flag indicating we found timestamp */
      //
      // Timestamp options are only legal on SYN packets or on connections
      // where we got one on the SYN packet
      //
      /* If it's on a SYN packet, mark the connection as doing timestamps */
      if (ti->ti_flags & TH_SYN) {
         tp->t_flags |= TF_TIMESTAMP;
         break;
      }
      if ((ti->ti_flags & TH_SYN) || (tp->t_flags & TF_TIMESTAMP)) {

        /* See if we should save his timestamp for our replys. RFC-1323
         * goes into detail about how to do this in section 3.4.
         */
        if((ti->ti_seq <= tp->last_ack) && (tp->last_ack < (ti->ti_seq + ti->ti_len))) {
           tp->ts_recent = IP_LoadU32BE(cp + 2);
        }
        /* If the received segment acknowledges new data then update our
         * round trip time. Again, from RFC-1323: (3.3)
         */
        if (ti->ti_t.th_ack > tp->snd_una) {
          int v;
          v = IP_LoadU32BE(cp + 6);  /* extract echoed tick count */
          tp->t_rttick = v;
          _UpdateRTT(tp);     /*  pass to routine */
        }
      }
       break;
  #endif   /* TCP_TIMESTAMP */

    default:
       break;
    }
  }
  m_free(om);

  #if TCP_TIMESTAMP
  /*
  * On syn/ack packets see if we should be timestamping or not. The
  * tp->t_flags TF_TIMESTAMP bit was already set above if we sent a
  * SYN w/RTTM and peer acked it in kind, but if we sent RTTM in
  * a SYN and this syn/ack did NOT echo it then the peer is not
  * doing RTTM and we should not pester it with options it probably
  * doesn't understand.
  */
  if((ti->ti_flags & (TH_SYN|TH_ACK)) == (TH_SYN|TH_ACK)) {
    if(!gotstamp) {
      tp->t_flags &= ~TF_TIMESTAMP;
    }
  }
  #endif /* TCP_TIMESTAMP */

  //
  // Compute True MSS based on mss and Option lne
  //
  if (ti->ti_flags & TH_SYN) {   /* MSS only on SYN */
    int OptLen = 0;
    if (tp->t_flags &= TF_TIMESTAMP) {
      OptLen += 12;
    }
    tp->OptLen  = OptLen;
    tp->TrueMSS = tp->Mss - OptLen;
    tp->snd_cwnd = 0xFFFF;   // Use max. value initially
  }
  return;
}



/* FUNCTION: tcp_reass()
 *
 * Insert segment ti into reassembly queue of tcp with
 * control block tp.  tcp_input() does the common case inline
 * (segment is the next to be received on an established connection,
 * and the queue is empty), avoiding linkage into and removal
 * from the queue and repetition of various conversions.
 * Set DELACK for segments received in order, but ack immediately
 * when segments are out of order (so fast retransmit can work).
 *
 * PARAM1: struct tcpcb *tp
 * PARAM2: struct tcpiphdr *ti
 * PARAM3: struct mbuf *ti_mbuf
 *
 * RETURNS:  Return TH_FIN if reassembly now includes a segment with FIN.
 */

static int tcp_reass(struct tcpcb * tp, struct tcpiphdr * ti, struct mbuf *  ti_mbuf) {
   struct tcpiphdr * q;
   struct socket *   so =  tp->t_inpcb->inp_socket;
   struct mbuf *  m;
   int   flags;

   /*
    * Call with ti==0 after become established to
    * force pre-ESTABLISHED data up to user socket.
    */
   if (ti == 0)
      goto present;

   /*
    * Find a segment which begins after this one does.
    */
   for (q = tp->seg_next; q != (struct tcpiphdr *)tp; q = (struct tcpiphdr *)q->ti_next) {
     if (SEQ_GT(q->ti_seq, ti->ti_seq))
     break;
   }

  /*
  * If there is a preceding segment, it may provide some of
  * our data already.  If so, drop the data from the incoming
  * segment.  If it provides all of our data, drop us.
  */
  if ((struct tcpiphdr *)q->ti_prev != (struct tcpiphdr *)tp) {
    long  i;
    q = (struct tcpiphdr *)q->ti_prev;
    /* conversion to int (in i) handles seq wraparound */
    i = q->ti_seq + q->ti_len - ti->ti_seq;
    if (i > 0) {
       if (i >= (long)ti->ti_len) {
          IP_STAT_INC(IP_TCP_Stat.rcvduppack);
          IP_STAT_ADD(IP_TCP_Stat.rcvdupbyte, ti->ti_len);
          GOTO_DROP;
       }
       m_adj (ti_mbuf, (int)i);
       ti->ti_len -= (short)i;
       ti->ti_seq += (U32)i;
    }
    q = (struct tcpiphdr *)(q->ti_next);
  }
  IP_STAT_INC(IP_TCP_Stat.rcvoopack);
  IP_STAT_ADD(IP_TCP_Stat.rcvoobyte, ti->ti_len);

  /*
  * While we overlap succeeding segments trim them or,
  * if they are completely covered, dequeue them.
  */
  while (q != (struct tcpiphdr *)tp) {
    int  i;
    i =  (int)((ti->ti_seq +  ti->ti_len) -  q->ti_seq);
    if (i <= 0) {
      break;
    }
    if (i < (int)(q->ti_len)) {
      q->ti_seq += i;
      q->ti_len -= (U16)i;
      m_adj (dtom(q), (int)i);
      break;
    }
    q = (struct tcpiphdr *)q->ti_next;
    m = dtom(q->ti_prev);
    remque (q->ti_prev);
    m_freem (m);
  }
  //
  // Stick new segment in its place.
  //
  insque(ti, q->ti_prev);

present:
  //
  // Present data to user, advancing rcv_nxt through
  // completed sequence space.
  //
  if (TCPS_HAVERCVDSYN (tp->t_state) == 0) {
    return 0;
  }
  ti = tp->seg_next;
  if (ti == (struct tcpiphdr *)tp || ti->ti_seq != tp->rcv_nxt) {
    return 0;
  }
  if (tp->t_state == TCPS_SYN_RECEIVED && ti->ti_len) {
    return 0;
  }
  do {
    tp->rcv_nxt += ti->ti_len;
    flags = ti->ti_flags & TH_FIN;
    remque(ti);
    m = dtom(ti);
    ti = (struct tcpiphdr *)ti->ti_next;
    if (so->so_state & SS_CANTRCVMORE) {
      m_freem (m);
    } else {
      sbappend (&so->so_rcv, m);
    }
  } while (ti != (struct tcpiphdr *)tp && ti->ti_seq == tp->rcv_nxt);
  sorwakeup(so);
  return (flags);
drop:
  /**m_freem (dtom(ti));**/
  m_freem (ti_mbuf);
  return (0);
}

/*********************************************************************
*
*       _StripOptions()
*/

static void _StripOptions(struct ip * ti, struct mbuf * m) {
   int   ihlen;

   /* get the IP header length in octets */
   ihlen = (ti->ip_ver_ihl & 0x0f) << 2;

   /* if it's <= 20 octets, there are no IP header options to strip */
   if (ihlen <= 20)
      return;

   /* figure out how much to strip: we want to keep the 20-octet IP header */
   ihlen -= 20;

   /* remove the stripped options from the IP datagram length */
   ti->ip_len -= ihlen;

   /* and from the IP header length (which will be 5*4 octets long) */
   ti->ip_ver_ihl = (ti->ip_ver_ihl & 0xf0) | 5;

   /* move the 20-octet IP header up against the IP payload */
   IP_MEMMOVE( ((char*)ti) + ihlen, ti, 20);
   m->m_len -= ihlen;
   m->m_data += ihlen;
}




/*********************************************************************
*
*       IP_TCP_OnRx
*
*  Function description
*    Called from IP stack whenever it receives a TCP packet.
*    Data is passed in a NetPort netbuf structure. Returns
*    0 if packet was processed succesfully, IP_ERR_NOT_MINE if not for me,
*    or a negative error code if packet was badly formed. In practice,
*    the return value is usually ignored.
*
*/
void IP_TCP_OnRx(IP_PACKET * pPacket) {     /* NOTE: pPacket has pData pointing to IP header */
  struct mbuf *  m;
  struct tcphdr *   tcpp;
  unsigned CheckSumRx;
  unsigned CheckSumCalc;

   struct ip * pip;
   struct in_addr laddr;

   struct tcpiphdr * ti;
   struct inpcb * inp;
   struct mbuf *  om;
   int   len,  tlen, off;
   struct tcpcb * tp;
   int   tiflags;
   struct socket *   so;
   int   todrop,  acked,   ourfinisacked, needoutput;
   int   dropsocket;
   long  iss;
   U32   rx_win;     /* scaled window from received segment */
   int olen;      /* length of options field */

#if IP_DEBUG > 1
   int   ostate = 0;
#endif


  pip = (struct ip *)pPacket->pData;    /* get ip header */
  len = ntohs(pip->ip_len);  /* get length in local endian */

  //
  // Verify checksum of received packet if driver has not already done so.
  //
  if ((IP_aIFace[0].Caps & IP_NI_CAPS_CHECK_TCP_CHKSUM) == 0) {
    tcpp = (struct tcphdr *)ip_data(pip);
    CheckSumRx   = tcpp->th_sum;                 // This needs to be done BEFORE the checksum computation, since the computation modifies the Checksum (for performance reasons)
    CheckSumCalc = IP_TCP_CalcChecksum(pip);
#if IP_TCP_ACCEPT_CHECKSUM_FFFF       // Some systems send wrong TCP checksums of 0xFFFF. Allow ignoring this. Note: This is not compliant with RFC1071 and RFC1624
    if (CheckSumRx == 0xFFFF) {
      CheckSumRx = CheckSumCalc;
    }
#endif
    if (CheckSumCalc != CheckSumRx) {
      IP_WARN((IP_MTYPE_TCP_IN, "TCP: Packet with incorrect checksum received: %4x computed, received %4x", CheckSumCalc, CheckSumRx));
      IP_STAT_INC(tcpmib.tcpInErrs);
      IP_STAT_INC(IP_TCP_Stat.rcvbadsum);
      pk_free(pPacket);
      return;                              // Bad header
    }
  }
  //
  // Put packet into MBUF
  //
  m = MBUF_GET(MT_RXDATA);
  if (!m) {
    pk_free(pPacket);
    return;
  }

  /* subtract IP header length from total IP packet length */
  len -= ((U16)(pip->ip_ver_ihl & 0x0f) << 2);
  pip->ip_len = len;   /* put TCP length in struct for TCP code to use */

  /* set mbuf to point to start of IP header (not TCP) */
  m->pkt = pPacket;
  m->m_data  = (char*)pip;  //pPacket->pData;
  m->m_len   = pPacket->NumBytes;

/*
*
* TCP input routine, follows pages 65-76 of the
* protocol specification dated September, 1981 very closely.
*
*/
  om = NULL;
  tp = NULL;
  so = NULL;
  dropsocket = 0;
  iss = 0;
  needoutput = 0;

  ASSERT_LOCK();
  IP_STAT_INC(IP_TCP_Stat.rcvtotal);
  IP_STAT_INC(tcpmib.tcpInSegs);    /* keep MIB stats */

  //
  // Get IP and TCP header together in first mbuf.
  // Note: IP leaves IP header in first mbuf.
  //
  if (pip->ip_ver_ihl > 0x45) {  /* IP v4, 5 dword hdr len */
    _StripOptions(pip, m);
    pip = (struct ip *)m->m_data;
  }
  if (m->m_len < ((sizeof (struct ip) + sizeof (struct tcphdr)))) {
    IP_STAT_INC(IP_TCP_Stat.rcvshort);
    m_freem(m);      /* free the received mbuf */
    return;
  }
  tlen = pip->ip_len;     /* this was fudged by IP layer */

  /* The following is needed in the cases where the size of the
   * overlay structure is larger than the size of the ip header.
   * This can happen if the ih_next and ih_prev pointers in the
   * overlay structure are larger than 32 bit pointers.
   */
  ti = (struct tcpiphdr *)(m->m_data + sizeof(struct ip) - sizeof(struct ipovly));
  IP_LOG((IP_MTYPE_TCP_IN, "TCP_IN: Seq: %u", htonl(ti->ti_t.th_seq)));
  if ((char *)ti < m->pkt->pBuffer) {
     IP_PANIC("tcp_input");
  }

   /*
    * Check that TCP offset makes sense,
    * pull out TCP options and adjust length.
    */

  off = GET_TH_OFF(ti->ti_t) << 2;
  if (off < sizeof (struct tcphdr) || off > tlen)  {
    IP_WARN((IP_MTYPE_TCP_IN, "TCP: tcp offset: src %x off %d", ti->ti_src, off));
    IP_STAT_INC(IP_TCP_Stat.rcvbadoff);
    IP_STAT_INC(tcpmib.tcpInErrs);   /* keep MIB stats */
    GOTO_DROP;
  }
  tlen -= (int)off;
  ti->ti_len = (U16)tlen;

  //
  // Handle TCP options.
  // If there are any options, allocate an MBUF, copy options into it
  // and eliminate option bytes from MBUF containing data by adjusting point and len
  //
  olen = off - sizeof (struct tcphdr);   /* get options length */
  if (olen)  {
    om = MBUF_GET_WITH_DATA(MT_RXDATA, olen);  /* get mbuf for opts */
    if (om == 0) {
       GOTO_DROP;
    }
    om->m_len = olen;       /* set mbuf length */
    /* set pointer to options field at end of TCP header */
    IP_MEMCPY(om->m_data, (m->m_data + 40), olen); /* copy to new mbuf */
    //
    // Strip options from data mbuf.
    //
    m->m_data += olen;
    m->m_len  -= olen;
  }
  tiflags = ti->ti_flags;

#if (IP_IS_BIG_ENDIAN == 0)
  /* Convert TCP protocol specific fields to host format. */
  ti->ti_seq = IP_HTONL_FAST(ti->ti_seq);
  ti->ti_ack = IP_HTONL_FAST(ti->ti_ack);
  ti->ti_urp = ntohs(ti->ti_urp);
#endif   /* endian */


   /*
    * Locate pcb for segment.
    */
findpcb:
  /* Drop TCP and IP headers; TCP options were dropped above. */
  m->m_data += 40;
  m->m_len  -= 40;

  inp = IP_TCP_PCB_lookup(&IP_tcb, ti->ti_src.s_addr, ti->ti_sport, ti->ti_dst.s_addr, ti->ti_dport, INPLOOKUP_WILDCARD);
  /*
  * If the state is CLOSED (i.e., TCB does not exist) then
  * all data in the incoming segment is discarded.
  * If the TCB exists but is in CLOSED state, it is embryonic,
  * but should either do a listen or a connect soon.
  */
  if (inp == 0) {
    IP_WARN((IP_MTYPE_TCP_IN, "TCP: Packet drop with Reset: inp == 0"));
    goto  dropwithreset;
  }
  tp = inp->inp_ppcb;
  if (tp == 0) {
    IP_WARN((IP_MTYPE_TCP_IN, "TCP: Packet drop with Reset: tp == 0"));
    goto  dropwithreset;
  }
  if (tp->t_state == TCPS_CLOSED) {
    IP_WARN((IP_MTYPE_TCP_IN, "TCP: TCP-state closed. Packet dropped"));
    goto drop;
  }
  so = inp->inp_socket;
#if IP_DEBUG > 1
  if (so->so_options & SO_DEBUG) {
    ostate = tp->t_state;
    tcp_saveti = *ti;
  }
#endif

   /* figure out the size of the other guy's receive window */
   rx_win = ntohs(ti->ti_win);

#if TCP_WIN_SCALE
   /* Both the WINDSCLAE flag and send_wind_scale should be set if he
    * negtitiated this at connect itme.
    */
   if (tp->t_flags & TF_WINSCALE) {
     rx_win <<= tp->snd_wind_scale;         /* apply scale */
   }
#endif /* TCP_WIN_SCALE */

   if (so->so_options & SO_ACCEPTCONN) {
      so = sonewconn(so);
      if (so == 0) {
         GOTO_DROP;
      }
      /*
       * This is ugly, but ....
       *
       * Mark socket as temporary until we're
       * committed to keeping it.  The code at
       * ``drop'' and ``dropwithreset'' check the
       * flag dropsocket to see if the temporary
       * socket created here should be discarded.
       * We mark the socket as discardable until
       * we're committed to it below in TCPS_LISTEN.
       */
      dropsocket++;

      inp = (struct inpcb *)so->so_pcb;
      inp->ifp = pPacket->pNet;      /* save iface to peer */

      switch(so->so_domain) {
      case AF_INET:
         inp->inp_laddr = ti->ti_dst;
         break;
      }

      inp->inp_lport = ti->ti_dport;
      tp = inp->inp_ppcb;
      tp->t_state = TCPS_LISTEN;
   }

   /*
    * Segment received on connection.
    * Reset idle time and keep-alive timer.
    */
   tp->IdleCnt = 0;
   IP_TCP_StartKEEPTimer(tp, IP_TCP_KeepIdle);

   /*
    * Process options if not in LISTEN state,
    * else do it below (after getting remote address).
    */
   if (om && tp->t_state != TCPS_LISTEN) {
     _HandleOptions(tp, om, ti);
     om = 0;
   }

   acked = (int)(ti->ti_ack - tp->snd_una);

#ifdef TCP_SACK
   /* If we received a TCP SACK header, indicate we need output now */
   if(tp->t_flags & TF_SACKREPLY) {
      needoutput = 1;            /* set tcp_input local flag */
   }
   /* If new data has been acked and we have a sack hole list
    * for the peer then see if the ack covers any sack holes
    */
   if((acked > 0) && (tp->sack_hole_start[0] != 0) ) {
      int i,j;
      for (i = 0; i < SACK_BLOCKS; i++)  {
         /* see if the ack covers this sack hole */
         if (ti->ti_ack >= tp->sack_hole_end[i]) {
            /* if hole is not the last one in the list, move up
             * the trainling holes
             */
            if ((i < (SACK_BLOCKS - 1)) && (tp->sack_hole_start[i + 1] != 0)) {
              for (j = i; j < SACK_BLOCKS - 1; j++) {
                tp->sack_hole_start[j] = tp->sack_hole_start[j + 1];
                tp->sack_hole_end[j] = tp->sack_hole_end[j + 1];
              }
            } else {  /* hole is at end of list */
               /* delete the hole */
               tp->sack_hole_start[i] = tp->sack_hole_end[i] = 0;
            }
         }
      }
   }
#endif /* TCP_SACK */


   /*
    * Calculate amount of space in receive window,
    * and then do TCP input processing.
    * Receive window is amount of space in rcv queue,
    * but not less than advertised window.
    */
   {
     U32 win;

     win = sbspace(&so->so_rcv);
     win = MAX((U32)win, (tp->rcv_adv - tp->rcv_nxt));
     tp->rcv_wnd = win;
   }


   /*
    * Header prediction: check for the two common cases
    * of a uni-directional data xfer.  If the packet has
    * no control flags, is in-sequence, the window didn't
    * change and we're not retransmitting, it's a
    * candidate.  If the length is zero and the ack moved
    * forward, we're the sender side of the xfer.  Just
    * free the data acked & wake any higher level process
    * that was blocked waiting for space.  If the length
    * is non-zero and the ack didn't move, we're the
    * receiver side.  If we're getting packets in-order
    * (the reassembly queue is empty), add the data to
    * the socket buffer and note that we need a delayed ack.
    */
   if ((tp->t_state == TCPS_ESTABLISHED) &&
       ((tiflags & (TH_SYN|TH_FIN|TH_RST|TH_URG|TH_ACK)) == TH_ACK) &&
       (ti->ti_seq == tp->rcv_nxt) &&
        rx_win && (rx_win == tp->snd_wnd) &&
       (tp->snd_nxt == tp->snd_max))
   {
      if (ti->ti_len == 0) {
         if (SEQ_GT(ti->ti_ack, tp->snd_una) && SEQ_LEQ(ti->ti_ack, tp->snd_max) && tp->snd_cwnd >= tp->snd_wnd)  {
            /*
             * this is a pure ack for outstanding data.
             */
            IP_STAT_INC(IP_TCP_Stat.predack);
            if (tp->t_rttick &&
#if TCP_TIMESTAMP
               ((tp->t_flags & TF_TIMESTAMP) == 0) &&
#endif
               (SEQ_GT(ti->ti_ack, tp->t_rtseq))) {
               _UpdateRTT(tp);
            }

            IP_STAT_INC(IP_TCP_Stat.rcvackpack);
            IP_STAT_ADD(IP_TCP_Stat.rcvackbyte, acked);
            sbdrop(&so->so_snd, acked);
            tp->snd_una = ti->ti_ack;
            m_freem(m);

            /*
             * If all outstanding data are acked, stop
             * retransmit timer, otherwise restart timer
             * using current (possibly backed-off) value.
             * If process is waiting for space,
             * wakeup/selwakeup/signal.  If data
             * are ready to send, let tcp_output
             * decide between more output or persist.
             */
            if (tp->snd_una == tp->snd_max) {
              tp->t_timer[TCPT_REXMT] = 0;
            } else if (tp->t_timer[TCPT_PERSIST] == 0) {
              IP_TCP_StartREXMTTimer(tp);
            }

            if (so->so_snd.sb_flags & (SB_WAIT | SB_SEL)) {
              sowwakeup(so);
            }

            /* If there is more data in the send buffer, and some is
             * still unsent, then call tcp_output() to try to send it
             */
            if (so->so_snd.NumBytes > (tp->snd_nxt - tp->snd_una)) {
              tcp_output(tp);
            }
            return;
         }
      } else if (ti->ti_ack == tp->snd_una && tp->seg_next == (struct tcpiphdr *)tp && ti->ti_len <= sbspace(&so->so_rcv)) {
         int   adv;

#ifdef TCP_ZEROCOPY
         //
         // This may be a window probe sent because a zerocopy application
         // has closed the TCP window by sitting on all the bigbufs. If so,
         // we want to drop this byte and send an ACK for the pervious one.
         //
         if (so->rx_upcall && (ti->ti_len == 1) && (IP_Global.aFreeBufferQ[1].q_len < 2)) {
           goto drop_probe;
         }
#endif   /* TCP_ZEROCOPY */

         /* this may also be a garden-variety probe received because
          * the socket sendbuf was full.
          */
         if (tp->rcv_wnd == 0) {
#ifdef TCP_ZEROCOPY
drop_probe:
#endif   /* TCP_ZEROCOPY */
            /* we should probably do some elegant handling of the TCP state
             * info in this seg, but Windows NT 4.0 has a nasty bug where it
             * will hammer us mericilessly with these probes (one customer
             * reports thousands per second) so we just dump it ASAP to
             * save cycles.
             */
            IP_STAT_INC(IP_TCP_Stat.rcvwinprobe);
            m_freem (m);      /* free the received mbuf */
            tcp_output(tp);   /* send the ack now... */
            return;
         }

         /*
          * this is a pure, in-sequence data packet
          * with nothing on the reassembly queue and
          * we have enough buffer space to take it.
          */
         IP_STAT_INC(IP_TCP_Stat.preddat);
         tp->rcv_nxt += ti->ti_len;
         IP_STAT_INC(IP_TCP_Stat.rcvpack);
         IP_STAT_ADD(IP_TCP_Stat.rcvbyte, ti->ti_len);
         //
         // Add data to socket buffer.
         //
         IP_LOG((IP_MTYPE_TCP_IN, "TCP_IN: %u bytes", ti->ti_len));
         sbappend(&so->so_rcv, m);
         sorwakeup(so);
#ifdef TCP_ZEROCOPY
         /* if it's a zerocopy socket, push data up to user */
         if (so->rx_upcall) {
           IP_TCP_DataUpcall(so);
         }
#endif
         /*
          * If this is a short packet, then ACK now - with Nagel
          *   congestion avoidance sender won't send more until
          *   he gets an ACK.
          */
         if (tiflags & TH_PUSH) {
           tp->t_flags |= TF_ACKNOW;
         } else {
           tp->t_flags |= TF_DELACK;
         }
         /* see if we need to send an ack */
         adv = (int)(tp->rcv_wnd - (tp->rcv_adv - tp->rcv_nxt));

         if ((adv >= (int)(tp->Mss * 2)) || (tp->t_flags & TF_ACKNOW)) {
#ifdef DO_DELAY_ACKS
           if (tp->DelayAckPeriod) {   /* doing delayed acks on timer */
             if (tp->DelayAckRem != 0) {  /* ack timeout not set? */
               tp->DelayAckRem = tp->DelayAckPeriod;  /* set it */
             }
             tp->t_flags |= TF_DELACK;
             tp->t_flags &= ~TF_ACKNOW;
             return;
           }
#endif   /* DO_DELAY_ACKS */

           tp->t_flags |= TF_ACKNOW;
           tp->t_flags &= ~TF_DELACK;
           tcp_output(tp);   /* send the ack now... */
         }
         return;
      }
   }

   switch (tp->t_state) {
   /*
    * If the state is LISTEN then ignore segment if it contains an RST.
    * If the segment contains an ACK then it is bad and send a RST.
    * If it does not contain a SYN then it is not interesting; drop it.
    * Don't bother responding if the destination was a broadcast.
    * Otherwise initialize tp->rcv_nxt, and tp->irs, select an initial
    * tp->iss, and send a segment:
    *     <SEQ=ISS><ACK=RCV_NXT><CTL=SYN,ACK>
    * Also initialize tp->snd_nxt to tp->iss+1 and tp->snd_una to tp->iss.
    * Fill in remote peer address fields if not previously specified.
    * Enter SYN_RECEIVED state, and process any other fields of this
    * segment in this state.
    */
   case TCPS_LISTEN:
   {
     struct mbuf *  am;
     if (tiflags & TH_RST)
        GOTO_DROP;
     if (tiflags & TH_ACK)
        GOTO_DROPWITHRESET;
     if ((tiflags & TH_SYN) == 0)
        GOTO_DROP;
     if(in_broadcast(ti->ti_dst.s_addr))
        GOTO_DROP;
     am = MBUF_GET_WITH_DATA(MT_SONAME, sizeof (struct sockaddr));
     if (am == NULL) {
       GOTO_DROP;
     }
#ifdef IP_V4
     if (inp->inp_socket->so_domain == AF_INET) {
       struct sockaddr_in * sin;
       am->m_len = sizeof (struct sockaddr_in);
       sin = MTOD(am, struct sockaddr_in *);
       sin->sin_family = AF_INET;
       sin->sin_addr = ti->ti_src;
       sin->sin_port = ti->ti_sport;
       /* Assuming pcbconnect will work, we put the sender's address in
        * the inp_laddr (after saving a local laddr copy). If the connect
        * fails we restore the inpcb before going to drop:
        */
       laddr = inp->inp_laddr;    /* save tmp laddr */
       if (inp->inp_laddr.s_addr == INADDR_ANY) {
         inp->inp_laddr = ti->ti_dst;
       }
       if (IP_TCP_PCB_connect (inp, am)) {
         inp->inp_laddr = laddr;
         m_free(am);
         GOTO_DROP;
       }
       m_free (am);
     }
#endif   /* end v4 */

     inp->ifp = pPacket->pNet;      /* set interface for conn.*/
     tp->t_template = tcp_template(tp);
     if (tp->t_template == 0) {
       SETTP(tp, tcp_drop(tp, ENOBUFS));
       dropsocket = 0;      /* socket is already gone */
       GOTO_DROP;
     }
     if (om) {
       _HandleOptions(tp, om, ti);
       om = 0;
     }
     if (iss) {
       tp->iss = iss;
     } else {
       tp->iss = tcp_iss;
     }
     tcp_iss += (unsigned)(TCP_ISSINCR/2);
     tp->irs = ti->ti_seq;
     tp->snd_una = tp->iss;
     tp->snd_nxt = tp->iss;
     tp->snd_max = tp->iss;

     tcp_rcvseqinit(tp);
     tp->t_flags |= TF_ACKNOW;
     tp->t_state = TCPS_SYN_RECEIVED;
     IP_TCP_StartKEEPTimer(tp, IP_TCP_KeepInit);
     dropsocket = 0;      /* committed to socket */
     IP_STAT_INC(IP_TCP_Stat.accepts);
     goto trimthenstep6;
   }

   /*
    * If the state is SYN_SENT:
    *   if seg contains an ACK, but not for our SYN, drop the input.
    *   if seg contains a RST, then drop the connection.
    *   if seg does not contain SYN, then drop it.
    * Otherwise this is an acceptable SYN segment
    *   initialize tp->rcv_nxt and tp->irs
    *   if seg contais ack then advance tp->snd_una
    *   if SYN has been acked change to ESTABLISHED else SYN_RCVD state
    *   arrange for segment to be acked (eventually)
    *   continue processing rest of data/controls, beginning with URG
    */
   case TCPS_SYN_SENT:
      inp->ifp = pPacket->pNet;
      if ((tiflags & TH_ACK) && (SEQ_LEQ(ti->ti_ack, tp->iss) || SEQ_GT(ti->ti_ack, tp->snd_max))) {
        GOTO_DROPWITHRESET;
      }
      if (tiflags & TH_RST) {
        if (tiflags & TH_ACK) {
          SETTP(tp, tcp_drop(tp, ECONNREFUSED));
        }
        GOTO_DROP;
      }
      if ((tiflags & TH_SYN) == 0) {
        GOTO_DROP;
      }
      if (tiflags & TH_ACK) {
        tp->snd_una = ti->ti_ack;
        if (SEQ_LT(tp->snd_nxt, tp->snd_una)) {
          tp->snd_nxt = tp->snd_una;
        }
      }
      tp->t_timer[TCPT_REXMT] = 0;
      tp->irs = ti->ti_seq;
      tcp_rcvseqinit(tp);
      if (inp->inp_laddr.s_addr != ti->ti_dst.s_addr) {
         /*
          * the IP interface may have changed address since we sent our SYN
          * (e.g. PPP brings link up as a result of said SYN and gets new
          * address via IPCP); if so we need to update the inpcb and the
          * TCP header template with the new address.
          */
        if ((m->pkt->pNet != NULL)  && (m->pkt->pNet->n_ipaddr == ti->ti_dst.s_addr)) {
      /* send an ack */
            inp->inp_laddr = ti->ti_dst;
            if (tp->t_template != NULL) {
              tp->t_template->ti_src = ti->ti_dst;
            }
         }
      }
      tp->t_flags |= TF_ACKNOW;
      if (tiflags & TH_ACK && SEQ_GT(tp->snd_una, tp->iss)) {
         IP_STAT_INC(IP_TCP_Stat.connects);
         tp->t_state = TCPS_ESTABLISHED;
         soisconnected (so);
         (void) tcp_reass (tp, (struct tcpiphdr *)0, m);
         /*
          * if we didn't have to retransmit the SYN,
          * use its rtt as our initial srtt & rtt var.
          */
         if (tp->t_rttick) {
           _UpdateRTT(tp);
         }
      } else {
         tp->t_state = TCPS_SYN_RECEIVED;
      }

trimthenstep6:
      /*
       * Advance ti->ti_seq to correspond to first data byte.
       * If data, trim to stay within window,
       * dropping FIN if necessary.
       */
      ti->ti_seq++;
      if (ti->ti_len > tp->rcv_wnd) {
         todrop = ti->ti_len - (U16)tp->rcv_wnd;
         /* XXX work around 4.2 m_adj bug */
         if (m->m_len) {
           m_adj(m, -todrop);
         } else {
           /* skip tcp/ip header in first mbuf */
           m_adj(m->m_next, -todrop);
         }
         ti->ti_len = (U16)tp->rcv_wnd;
         tiflags &= ~TH_FIN;
         IP_STAT_INC(IP_TCP_Stat.rcvpackafterwin);
         IP_STAT_ADD(IP_TCP_Stat.rcvbyteafterwin, todrop);
      }
      tp->snd_wl1 = ti->ti_seq - 1;
      goto step6;
   }

   /*
    * States other than LISTEN or SYN_SENT.
    * First check that at least some bytes of segment are within
    * receive window.  If segment begins before rcv_nxt,
    * drop leading data (and SYN); if nothing left, just ack.
    */
   todrop = (int)(tp->rcv_nxt - ti->ti_seq);
   if (todrop > 0) {
      if (tiflags & TH_SYN) {
         tiflags &= ~TH_SYN;
         ti->ti_seq++;
         if (ti->ti_urp > 1) {
           ti->ti_urp--;
         } else {
           tiflags &= ~TH_URG;
         }
         todrop--;
      }
      if ((todrop > (int)ti->ti_len) || ((todrop == (int)ti->ti_len) && ((tiflags & TH_FIN) == 0))) {
         IP_STAT_INC(IP_TCP_Stat.rcvduppack);
         IP_STAT_ADD(IP_TCP_Stat.rcvdupbyte, ti->ti_len);
         /*
          * If segment is just one to the left of the window,
          * check two special cases:
          * 1. Don't toss RST in response to 4.2-style keepalive.
          * 2. If the only thing to drop is a FIN, we can drop
          *    it, but check the ACK or we will get into FIN
          *    wars if our FINs crossed (both CLOSING).
          * In either case, send ACK to resynchronize,
          * but keep on processing for RST or ACK.
          */
         if ((tiflags & TH_FIN && todrop == (int)ti->ti_len + 1) || (tiflags & TH_RST && ti->ti_seq == tp->rcv_nxt - 1)) {
            todrop = ti->ti_len;
            tiflags &= ~TH_FIN;
            tp->t_flags |= TF_ACKNOW;
         } else {
           goto dropafterack;
         }
      } else {
        IP_STAT_INC(IP_TCP_Stat.rcvpartduppack);
        IP_STAT_ADD(IP_TCP_Stat.rcvpartdupbyte, todrop);
      }
      m_adj(m, todrop);
      ti->ti_seq += todrop;
      ti->ti_len -= (U16)todrop;
      if (ti->ti_urp > (U16)todrop) {
        ti->ti_urp -= (U16)todrop;
      } else {
        tiflags &= ~TH_URG;
        ti->ti_urp = 0;
      }
   }

   /*
    * If new data are received on a connection after the
    * user processes are gone, then RST the other end.
    */
   if ((so->so_state & SS_NOFDREF) && tp->t_state > TCPS_CLOSE_WAIT && ti->ti_len) {
     tp = tcp_close(tp);
     IP_STAT_INC(IP_TCP_Stat.rcvafterclose);
     GOTO_DROPWITHRESET;
   }

   /*
    * If segment ends after window, drop trailing data
    * (and PUSH and FIN); if nothing left, just ACK.
    */
   todrop = (int)((ti->ti_seq + (short)ti->ti_len) - (tp->rcv_nxt + tp->rcv_wnd));
   if (todrop > 0) {
      IP_STAT_INC(IP_TCP_Stat.rcvpackafterwin);
      if (todrop >= (int)ti->ti_len) {
         IP_STAT_ADD(IP_TCP_Stat.rcvbyteafterwin, ti->ti_len);
         /*
          * If a new connection request is received
          * while in TIME_WAIT, drop the old connection
          * and start over if the sequence numbers
          * are above the previous ones.
          */
         if (tiflags & TH_SYN && tp->t_state == TCPS_TIME_WAIT && SEQ_GT(ti->ti_seq, tp->rcv_nxt)) {
            iss = (U32)(tp->rcv_nxt + (TCP_ISSINCR));
            if (iss & 0xff000000) {
              iss = 0L;
            }
            (void) tcp_close(tp);
            goto findpcb;
         }
         /*
          * If window is closed can only take segments at
          * window edge, and have to drop data and PUSH from
          * incoming segments.  Continue processing, but
          * remember to ack.  Otherwise, drop segment
          * and ack.
          */
         if ((tp->rcv_wnd == 0) && (ti->ti_seq == tp->rcv_nxt)) {
            tp->t_flags |= TF_ACKNOW;
            IP_STAT_INC(IP_TCP_Stat.rcvwinprobe);
         } else
            goto dropafterack;
      } else
         IP_STAT_ADD(IP_TCP_Stat.rcvbyteafterwin, todrop);
      /* XXX work around m_adj bug */
      if (m->m_len) {
        m_adj(m, -todrop);
      } else {
         /* skip tcp/ip header in first mbuf */
         m_adj(m->m_next, -todrop);
      }
      ti->ti_len -= (U16)todrop;
      tiflags &= ~(TH_PUSH|TH_FIN);
   }


   /*
    * If the RST bit is set examine the state:
    * SYN_RECEIVED STATE:
    *   If passive open, return to LISTEN state.
    *   If active open, inform user that connection was refused.
    * ESTABLISHED, FIN_WAIT_1, FIN_WAIT2, CLOSE_WAIT STATES:
    *   Inform user that connection was reset, and close tcb.
    * CLOSING, LAST_ACK, TIME_WAIT STATES
    *   Close the tcb.
    */

#ifdef DOS_RST
   /* DOS_RST - Fix for "Denial of Service (DOS) using RST"
    * An intruder can send RST packet to break on existing TCP
    * connection. It means that if a RST is received in
    * ESTABLISHED state from an intruder, then the connection gets
    * closed between the original peers. To overcome this
    * vulnerability, it is suggested that we accept RST only when
    * the sequence numbers match. Else we send an ACK.
    */
   if ((tiflags & TH_RST) && (tp->t_state == TCPS_ESTABLISHED) &&  (ti->ti_seq != tp->rcv_nxt)) {
      /* RST received in established state and sequnece numbers
       * don't match.
       */
     IP_LOG((IP_MTYPE_TCP_IN, "TCP: Rcvd RST with seq num mismatch - ignoring RST, sending ACK."));
     tiflags &= ~TH_RST;  /* clear reset flag */
     goto dropafterack;   /* send an ack and drop current packet */
   }
#endif /* DOS_RST */

   if (tiflags & TH_RST) {
      switch (tp->t_state) {
      case TCPS_SYN_RECEIVED:
         so->so_error = ECONNREFUSED;
         goto close;

      case TCPS_ESTABLISHED:
         IP_STAT_INC(tcpmib.tcpEstabResets);     /* keep MIB stats */
         IP_LOG((IP_MTYPE_TCP_CLOSE, "TCP: Connection to %i:%d reset ", ti->ti_src.s_addr, htons(ti->ti_sport)));
      case TCPS_FIN_WAIT_1:
      case TCPS_FIN_WAIT_2:
      case TCPS_CLOSE_WAIT:
         so->so_error = ECONNRESET;
close:
         tp->t_state = TCPS_CLOSED;
         IP_STAT_INC(IP_TCP_Stat.drops);
         SETTP(tp, tcp_close(tp));
#ifdef TCP_ZEROCOPY
         if (so->rx_upcall) {
           so->rx_upcall(IP_SOCKET_p2h(so), NULL, ECONNRESET);
         }
#endif   /* TCP_ZEROCOPY */
         GOTO_DROP;

      case TCPS_CLOSING:
      case TCPS_LAST_ACK:
      case TCPS_TIME_WAIT:
         SETTP(tp, tcp_close(tp));
         GOTO_DROP;
      }
   }

   /*
    * If a SYN is in the window, then this is an
    * error and we send an RST and drop the connection.
    */

   if (tiflags & TH_SYN) {
     tp = tcp_drop(tp, ECONNRESET);
     GOTO_DROPWITHRESET;
   }

   /*
    * If the ACK bit is off we drop the segment and return.
    */
   if ((tiflags & TH_ACK) == 0) {
     GOTO_DROP;
   }

   /*
    * Ack processing.
    */
   switch (tp->t_state) {
   /*
    * In SYN_RECEIVED state if the ack ACKs our SYN then enter
    * ESTABLISHED state and continue processing, otherwise
    * send an RST.
    */
   case TCPS_SYN_RECEIVED:
      if (SEQ_GT(tp->snd_una, ti->ti_ack) ||
          SEQ_GT(ti->ti_ack, tp->snd_max))
      {
         IP_STAT_INC(tcpmib.tcpEstabResets);     /* keep MIB stats */
         GOTO_DROPWITHRESET;
      }
      IP_STAT_INC(IP_TCP_Stat.connects);
      tp->t_state = TCPS_ESTABLISHED;
      soisconnected(so);
      (void) tcp_reass(tp, (struct tcpiphdr *)0, m);
      tp->snd_wl1 = ti->ti_seq - 1;
      /* fall into ... */

   /*
    * In ESTABLISHED state: drop duplicate ACKs; ACK out of range
    * ACKs.  If the ack is in the range
    *   tp->snd_una < ti->ti_ack <= tp->snd_max
    * then advance tp->snd_una to ti->ti_ack and drop
    * data from the retransmission queue.  If this ACK reflects
    * more up to date window information we update our window information.
    */
   case TCPS_ESTABLISHED:
   case TCPS_FIN_WAIT_1:
   case TCPS_FIN_WAIT_2:
   case TCPS_CLOSE_WAIT:
   case TCPS_CLOSING:
   case TCPS_LAST_ACK:
   case TCPS_TIME_WAIT:

      if (SEQ_LEQ(ti->ti_ack, tp->snd_una)) {
         if (ti->ti_len == 0 && rx_win == tp->snd_wnd) {
            IP_STAT_INC(IP_TCP_Stat.rcvdupack);
            /*
             * If we have outstanding data (not a window probe), this is a completely
             * duplicate ack (ie, window info didn't change), the ack is the biggest we've
             * seen and we've seen exactly our rexmt threshhold of them, assume a packet
             * has been dropped and retransmit it.
             * Kludge snd_nxt & the congestion window so we send only this one
             * packet.  If this packet fills the only hole in the receiver's seq.
             * space, the next real ack will fully open our window.  This means we
             * have to do the usual slow-start to not overwhelm an intermediate gateway
             * with a burst of packets.  Leave here with the congestion window set
             * to allow 2 packets on the next real ack and the exp-to-linear thresh
             * set for half the current window size (since we know we're losing at
             * the current window size).
             */
            if (tp->t_timer[TCPT_REXMT] == 0 || ti->ti_ack != tp->snd_una) {
              tp->t_dupacks = 0;
            } else if (++tp->t_dupacks == 3) {         // This value may be changed
               U32 onxt = tp->snd_nxt;
               unsigned  win;
               win = tp->snd_wnd;
               if (win > tp->snd_cwnd) {
                 win = tp->snd_cwnd;
               }
               win /= 2 * tp->TrueMSS;

               if (win < 2) {
                 win = 2;
               }
               tp->snd_ssthresh = (U16)(win * tp->TrueMSS);

               tp->t_timer[TCPT_REXMT] = 0;
               tp->t_rttick = 0;
               tp->snd_nxt = ti->ti_ack;
               tp->snd_cwnd = tp->TrueMSS;
               tcp_output(tp);

               if (SEQ_GT(onxt, tp->snd_nxt)) {
                 tp->snd_nxt = onxt;
               }
               GOTO_DROP;
            }
         } else {
           tp->t_dupacks = 0;
         }
         break;
      }
      tp->t_dupacks = 0;
      if (SEQ_GT(ti->ti_ack, tp->snd_max)) {
        IP_STAT_INC(IP_TCP_Stat.rcvacktoomuch);
        goto dropafterack;
      }
      acked = (int)(ti->ti_ack - tp->snd_una);
      IP_STAT_INC(IP_TCP_Stat.rcvackpack);
      IP_STAT_ADD(IP_TCP_Stat.rcvackbyte, acked);

      /*
       * If transmit timer is running and timed sequence
       * number was acked, update smoothed round trip time.
       * Since we now have an rtt measurement, cancel the
       * timer backoff (cf., Phil Karn's retransmit alg.).
       * Recompute the initial retransmit timer.
       */
      if((tp->t_rttick) &&
#if TCP_TIMESTAMP
         ((tp->t_flags & TF_TIMESTAMP) == 0) &&
#endif /* TCP_TIMESTAMP */
         (SEQ_GT(ti->ti_ack, tp->t_rtseq)))
      {
         _UpdateRTT(tp);
      }
      /*
       * If all outstanding data is acked, stop retransmit
       * timer and remember to restart (more output or persist).
       * If there is more data to be acked, restart retransmit
       * timer, using current (possibly backed-off) value.
       */
      if (ti->ti_ack == tp->snd_max) {
         tp->t_timer[TCPT_REXMT] = 0;
         needoutput = 1;
      } else if (tp->t_timer[TCPT_PERSIST] == 0) {
        IP_TCP_StartREXMTTimer(tp);
      }
      /*
       * When new data is acked, open the congestion window.
       * If the window gives us less than ssthresh packets
       * in flight, open exponentially (maxseg per packet).
       * Otherwise open linearly (maxseg per window,
       * or maxseg^2 / cwnd per packet).
       */
      {
         U32 cw   =  tp->snd_cwnd;
         U16 incr =  tp->TrueMSS;

         if (cw > tp->snd_ssthresh) {
            incr = (U16) (MAX( (incr * incr / cw), 16 ));
         }

         tp->snd_cwnd = MIN(cw + (U16)incr, (IP_MAXPACKET));
      }
      if (acked > (int)so->so_snd.NumBytes) {
        tp->snd_wnd -= (U16)so->so_snd.NumBytes;
        sbdrop(&so->so_snd, (int)so->so_snd.NumBytes);
        ourfinisacked = 1;
      } else {
        sbdrop(&so->so_snd, acked);
        tp->snd_wnd -= (U16)acked;
        ourfinisacked = 0;
      }

      if (so->so_snd.sb_flags & (SB_WAIT | SB_SEL)) {
        sowwakeup(so);
      }

      tp->snd_una = ti->ti_ack;
      if (SEQ_LT(tp->snd_nxt, tp->snd_una)) {
        tp->snd_nxt = tp->snd_una;
      }

      switch (tp->t_state) {

      /*
       * In FIN_WAIT_1 STATE in addition to the processing
       * for the ESTABLISHED state if our FIN is now acknowledged
       * then enter FIN_WAIT_2.
       */
      case TCPS_FIN_WAIT_1:
         if (ourfinisacked) {
            /*
             * If we can't receive any more
             * data, then closing user can proceed.
             * Starting the timer is contrary to the
             * specification, but if we don't get a FIN
             * we'll hang forever.
             */
            if (so->so_state & SS_CANTRCVMORE) {
               soisdisconnected(so);
               IP_TCP_Start2MSLTimer(tp);
            }
            tp->t_state = TCPS_FIN_WAIT_2;
         }
         break;

       /*
       * In CLOSING STATE in addition to the processing for
       * the ESTABLISHED state if the ACK acknowledges our FIN
       * then enter the TIME-WAIT state, otherwise ignore
       * the segment.
       */
      case TCPS_CLOSING:
        if (ourfinisacked) {
          tp->t_state = TCPS_TIME_WAIT;
          tcp_canceltimers(tp);
          IP_TCP_Start2MSLTimer(tp);
          soisdisconnected(so);
        }
        break;

      /*
       * In LAST_ACK, we may still be waiting for data to drain
       * and/or to be acked, as well as for the ack of our FIN.
       * If our FIN is now acknowledged, delete the TCB,
       * enter the closed state and return.
       */
      case TCPS_LAST_ACK:
        if (ourfinisacked) {
          SETTP(tp, tcp_close(tp));
          GOTO_DROP;
        }
        break;

      /*
       * In TIME_WAIT state the only thing that should arrive
       * is a retransmission of the remote FIN.  Acknowledge
       * it and restart the finack timer.
       */
      case TCPS_TIME_WAIT:
         IP_TCP_Start2MSLTimer(tp);
         goto dropafterack;
      }
   }

step6:
   /*
    * Update window information.
    * Don't look at window if no ACK: TAC's send garbage on first SYN.
    */
   if (((tiflags & TH_ACK) && (SEQ_LT(tp->snd_wl1, ti->ti_seq)))
    || ((tp->snd_wl1 == ti->ti_seq) && (SEQ_LT(tp->snd_wl2, ti->ti_ack)))
    || ((tp->snd_wl2 == ti->ti_ack) && (rx_win > tp->snd_wnd)))
   {
      /* keep track of pure window updates */
      if ((ti->ti_len == 0) &&  (tp->snd_wl2 == ti->ti_ack) && (rx_win > tp->snd_wnd)) {
        IP_STAT_INC(IP_TCP_Stat.rcvwinupd);
      }
      tp->snd_wnd = rx_win;
      tp->snd_wl1 = ti->ti_seq;
      tp->snd_wl2 = ti->ti_ack;
      if (tp->snd_wnd > tp->max_sndwnd) {
        tp->max_sndwnd = tp->snd_wnd;
      }
      needoutput = 1;
   }

   /*
    * Process the segment text, merging it into the TCP sequencing queue,
    * and arranging for acknowledgment of receipt if necessary.
    * This process logically involves adjusting tp->rcv_wnd as data
    * is presented to the user (this happens in tcp_usrreq.c,
    * case PRU_RCVD).  If a FIN has already been received on this
    * connection then we just ignore the text.
    */
   if ((ti->ti_len || (tiflags&TH_FIN)) && TCPS_HAVERCVDFIN(tp->t_state) == 0) {
      /* Do the common segment reassembly case inline */
      if((ti->ti_seq == tp->rcv_nxt) && (tp->seg_next == (struct tcpiphdr *)(tp) ) && (tp->t_state == TCPS_ESTABLISHED)) {
#ifdef DO_DELAY_ACKS
         if (tp->DelayAckPeriod) {
           if (tp->DelayAckRem == 0) {   /* need to set ack timer? */
             tp->DelayAckRem = tp->DelayAckPeriod;
           }
           tp->t_flags |= TF_DELACK;
         } else {
           tp->t_flags |= TF_ACKNOW;
         }
#else    /* not DO_DELAY_ACKS */
            tp->t_flags |= TF_ACKNOW;
#endif   /* DO_DELAY_ACKS */

         tp->rcv_nxt += ti->ti_len;
         tiflags = ti->ti_flags & TH_FIN;
         IP_STAT_INC(IP_TCP_Stat.rcvpack);
         IP_STAT_ADD(IP_TCP_Stat.rcvbyte, ti->ti_len);
         sbappend(&so->so_rcv, (m));
         sorwakeup(so);
#ifdef TCP_SACK
         tp->sack_seqct = 0;     /* clear sack valid block count */
#endif /* TCP_SACK */
      } else {    /* received out of sequence segment */
        /* Drop it in the reassmbly queue */
        tiflags = tcp_reass(tp, ti, m);
        tp->t_flags |= TF_ACKNOW;
#ifdef TCP_SACK
        //
        // If doing SACK and there's data, then save sack info. and
        // set flag to send sack header on next ack
        ///
        if ((tp->t_flags & TF_SACKOK) && (m->m_len > 0)) {
          tcp_setsackinfo(tp, ti);
        }
#endif /* TCP_SACK */
      }


      //
      // Note the amount of data that peer has sent into
      // our window, in order to estimate the sender's
      // buffer size.
      //
      len = (int)(so->so_rcv.Limit - (tp->rcv_adv - tp->rcv_nxt));
      if (len > (int)tp->max_rcvd) {
        tp->max_rcvd = (U16)len;
      }
#ifdef TCP_ZEROCOPY
      if (so->rx_upcall) {
         IP_TCP_DataUpcall(so);
         /* if we have FIN and app has all data, do shutdown */
         if ((tiflags & TH_FIN) && (so->so_rcv.NumBytes == 0)) {
            so->rx_upcall(IP_SOCKET_p2h(so), NULL, ESHUTDOWN);
            so->so_state |= SS_UPCFIN;    /* flag that upcall was FINed */
         }
      }
#endif   /* TCP_ZEROCOPY */
   } else {
      m_freem(m);
      tiflags &= ~TH_FIN;
   }

   /*
    * If FIN is received ACK the FIN and let the user know
    * that the connection is closing.
    */
   if (tiflags & TH_FIN) {
      if (TCPS_HAVERCVDFIN(tp->t_state) == 0) {
         socantrcvmore(so);
         tp->t_flags |= TF_ACKNOW;
         tp->rcv_nxt++;
      }
      switch (tp->t_state) {

       /*
       * In SYN_RECEIVED and ESTABLISHED STATES
       * enter the CLOSE_WAIT state.
       */
      case TCPS_SYN_RECEIVED:
      case TCPS_ESTABLISHED:
         tp->t_state = TCPS_CLOSE_WAIT;
         break;

       /*
       * If still in FIN_WAIT_1 STATE FIN has not been acked so
       * enter the CLOSING state.
       */
      case TCPS_FIN_WAIT_1:
         tp->t_state = TCPS_CLOSING;
         break;

       /*
       * In FIN_WAIT_2 state enter the TIME_WAIT state,
       * starting the time-wait timer, turning off the other
       * standard timers.
       */
      case TCPS_FIN_WAIT_2:
         tp->t_state = TCPS_TIME_WAIT;
         tcp_canceltimers(tp);
         IP_TCP_Start2MSLTimer(tp);
         soisdisconnected(so);
         break;

      /*
       * In TIME_WAIT state restart the 2 MSL time_wait timer.
       */
      case TCPS_TIME_WAIT:
         IP_TCP_Start2MSLTimer(tp);
         break;
      }
   }
#if IP_DEBUG > 1
   if (so->so_options & SO_DEBUG) {
     IP_LOG((IP_MTYPE_TCP, "TCP_IN: state: %d, tcpcb: %x saveti: %x", ostate, tp, &tcp_saveti));
   }
#endif
   //
   // Return any desired output.
   //
   if (needoutput || (tp->t_flags & TF_ACKNOW)) {
     tcp_output(tp);
   }
   return;

dropafterack:
   /*
    * Generate an ACK dropping incoming segment if it occupies
    * sequence space, where the ACK reflects our state.
    */
   if (tiflags & TH_RST) {
      GOTO_DROP;
   }
   m_freem (m);
   tp->t_flags |= TF_ACKNOW;
   tcp_output (tp);
   return;

dropwithreset:
   IP_STAT_INC(tcpmib.tcpInErrs);    /* keep MIB stats */
   if (om) {
     m_free(om);
     om = 0;
   }

   /* Don't reset resets */
   if (tiflags & TH_RST) {
     GOTO_DROP;
   }

   /*
    * Generate a RST, dropping incoming segment.
    * Make ACK acceptable to originator of segment.
    * Don't bother to respond if destination was broadcast.
    */
#ifdef IP_V4
   if (in_broadcast(ti->ti_dst.s_addr))
      GOTO_DROP;
#endif

   if (tiflags & TH_ACK) {
      tcp_respond (tp, ti, 0, ti->ti_ack, TH_RST, m);
   } else {
     if (tiflags & TH_SYN) {
       ti->ti_seq++;
     }
     tcp_respond(tp, ti, ti->ti_seq, 0, TH_RST|TH_ACK, m);
   }
   /* destroy temporarily created socket */
   if (dropsocket) {
     soabort(so);
   }
   return;

drop:
  if (om) {
    m_free(om);
  }
  //
  // Drop space held by incoming segment and return.f
  //
  if (tp) {
    if (tp->t_inpcb->inp_socket->so_options & SO_DEBUG) {
      IP_LOG((IP_MTYPE_TCP, "TCP: drop: state %d, tcpcb: %x, saveti: %x", ostate, tp, &tcp_saveti));
    }
  }
  m_freem(m);
  /* destroy temporarily created socket */
  if (dropsocket) {
    soabort(so);
  }
}

/*************************** End of file ****************************/

