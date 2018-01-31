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
File    : IP_TCP_out.c
Purpose :
--------  END-OF-HEADER  ---------------------------------------------
*/

/* Additional Copyrights: */
/* Copyright 1997- 2000 By InterNiche Technologies Inc. All rights reserved */
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



#define     TCPOUTFLAGS

#include "IP_Int.h"
#include "IP_sockvar.h"

#ifdef INCLUDE_TCP  /* include/exclude whole file at compile time */

#include "IP_TCP_pcb.h"
//#include "pmtu.h"
#include "IP_TCP.h"
#include "IP_mbuf.h"       /* BSD-ish Sockets includes */

/*
 * Initial options. We always Use TCP MSS. In cases where we use other
 * options we make the options buffer large enough for full sized option
 * data header so that we can use it for option building later. We start
 * by defining all the header sizes.
 */

#define  MSSOPT_LEN     4

#ifdef TCP_SACK
#define  SACKOPT_LEN    ((SACK_BLOCKS * 8) + 2)
#else
#define  SACKOPT_LEN    0
#endif

#if TCP_WIN_SCALE
#define  SWINOPT_LEN    4  /* actually 3, but round upwards */
#else
#define  SWINOPT_LEN    0
#endif

#if TCP_TIMESTAMP
#define  STAMPOPT_LEN   10
#else
#define  STAMPOPT_LEN   0
#endif


/* Add up all the option sizes for the worst case option buffer size */
#define  OPTBUF_LEN ((MSSOPT_LEN + SACKOPT_LEN + SWINOPT_LEN + STAMPOPT_LEN + 3) & ~ 3)

/*
 * Flags used when sending segments in tcp_output.
 * Basic flags (TH_RST,TH_ACK,TH_SYN,TH_FIN) are totally
 * determined by state, with the proviso that TH_FIN is sent only
 * if all data queued for output is included in the segment.
 */

const U8   tcp_outflags[TCP_NSTATES]  = {
   TH_RST|TH_ACK, 0, TH_SYN, TH_SYN|TH_ACK,
   TH_ACK, TH_ACK,
   TH_FIN|TH_ACK, TH_FIN|TH_ACK, TH_FIN|TH_ACK, TH_ACK, TH_ACK,
};



/*
 * TCP Headers and IPv6
 *
 *   The tcp_output() routine gets it's send data from an mbuf
 * queue in the socket structure. These mbufs contain only the
 * data to send, no headers. The tcp_output() code prepends an
 * IP/TCP header to this data for sending. In the old v4 code
 * this header was placed in a space in the front of the PACKET
 * containing the data mbuf. This  space had previously been
 * reserved for this purpose by getnbuf(). If there is no mbuf
 * data to send, then an mbuf large enough for the TCP/IP header
 * structure "ti" is allocated.
 *
 *   "ti" is a BSD invention. It contains an "overlay" IP header followed by
 * a real TCP header. The overlay IP header has IP addresses at the end of
 * structure where a real IP header would, however the beginning of the
 * structure contains pointers which are used by the tcp_input() socket
 * code to link the received TCP reassembly queue. When we are sending
 * packets (below) the v4 IP header is constructed in the "ti" structure,
 * and the complete IP/TCP header is passed down to the IP layer in a single
 * contiguous buffer.
 *
 *   For IPv6, the IP header is 40 bytes instead of 20, and IPv6 is far more
 * likely than IPv4 to insert a field between the IP and TCP headers. For
 * these reasons, it's not as beneficial to keep the IP and TCP headers in a
 * contiguous buffer. So we don't. Instead we require that the system support
 * a scatter/gather version of the PACKET structures on which the mbufs are
 * mapped. The IPv6 code allocates a seperate mbuf for the IP header, and
 * builds the IP header seperatly from the TCP header.
 *
 *    Since we also want to support a dual-mode stack, we still maintain
 * the old "ti" strucuture for use by the the v4 packets, as well as
 * seperate IPv6 header mbuf used by the v6 code path. Thus the mbufs
 * allocated for TCP headers are still (about) 40 bytes in size.
 */



/*********************************************************************
*
*       _BuildOptions()
*
* Build tcp options for tp passed in buffer passed. No length checking
* is done on the buffer.
*
* RETURNS: length of option data added to buffer
*/

static int _BuildOptions(struct tcpcb * tp, U8 * p, int flags, struct socket * so) {
  int      len;
  U16  mss;

  len = 0;
  //
  // MSS option: Always put MSS option on SYN packets
  //
  if (flags & TH_SYN) {
    mss = tp->Mss;
    if (mss == 0) {
      mss = tp->t_inpcb->ifp->n_mtu - 40;     // Use interface MTU - 40 as defined by RFC 879
    }
    /* always send MSS option on SYN, fill in MSS parm */
    *(p + 0) = TCPOPT_MAXSEG;
    *(p + 1) = MSSOPT_LEN;               /* length byte */
    *(p + 2)  = (U8) ((mss & 0xff00) >> 8);
    *(p + 3)  = (U8) (mss & 0xff);
    len = 4;
    p += 4;
  }

#ifdef TCP_SACK
 /* on SYN and SYN/ACK pkts attach SACK OK option */
  if ((flags & TH_SYN) &&  (so->so_options & SO_TCPSACK))  {
    *p++ = TCPOPT_SACKOK;
    *p++ = 2;  /* length */
    len += 2;
  }

 /* If we should be sending a SACK data option packet, prepare it */
  if ((tp->t_flags & (TF_SACKNOW|TF_SACKOK)) == (TF_SACKNOW|TF_SACKOK))  {
    int tmp;
    tmp = tcp_buildsack(tp, p);
    len += tmp;
    p += tmp;
  }
#endif /* TCP_SACK */

#if TCP_WIN_SCALE
  /* TCP Window Scale options only go on packets with SYN bit set */
  if ((flags & TH_SYN) && (so->so_options & SO_WINSCALE))  {
    if (flags & TH_ACK) {   // Sending a SYN/ACK
      if (tp->t_flags & TF_WINSCALE) {   // Did he offer to scale ?
        /* On SYN/ACKs, reply to scale offer */
        *p++ = TCPOPT_WINSCALE ;  /* window scale option */
        *p++ = 3;                 /* option length */
        *p++ = tp->rcv_wind_scale;
        len += 3;
      }
    } else {    /* sending a SYN */
      /* send scale offer with plain SYNs */
      *p++ = TCPOPT_WINSCALE ;  /* window scale option */
      *p++ = 3;                 /* option length */
      *p++ = tp->rcv_wind_scale;
      *p++ = TCPOPT_NOP;        /* Pad out to 4 bytes. */
      len += 4;
    }
  }
#endif   /* TCP_WIN_SCALE */

  //
  // Only add timestamp (RTTM) option IF option flag is set AND it's a SYN packet
  // or we've already agreed to use RTTM.
  //
#if TCP_TIMESTAMP
  if ((so->so_options & SO_TIMESTAMP) && (((flags & (TH_SYN|TH_ACK)) == TH_SYN)|| (tp->t_flags & TF_TIMESTAMP))) {
    *p = TCPOPT_RTT;     /* window scale option */
    *(p + 1) = 10;       /* option length */
    IP_StoreU32BE(p + 2, IP_OS_GetTime32());       // Set our send time for echo
    IP_StoreU32BE(p + 6, tp->ts_recent);           // Echo the last time we got from peer
    p += 10;
    len += 10;
  }
#endif   /* TCP_TIMESTAMP */

  //
  //  Extend options to aligned address
  //
  while (len & 3) {
    *p++ = TCPOPT_EOL;
    len++;
  }
  return len;
}



/*********************************************************************
*
*       tcp_output()
*
* Tcp output routine: figure out what should be sent and send it.
*
* RETURNS: 0 if OK, else a sockets error code.
*/
int tcp_output(struct tcpcb * tp) {
  struct socket *   so;
  int   len;
  I32   win;
  int   off,  flags,   error;
  struct mbuf * m;       // MBUF containing start sequence
  struct mbuf * sendm;   /* mbuf which contains data to send */
  struct tcpiphdr * ti;
  unsigned optlen;
  unsigned optlenRem;
  int   idle, sendalot;
  int   bufoff;           /* offset of data in sendm->m_data */
  U8 tcp_optionbuf[OPTBUF_LEN];   // Buffer for TCP-options

  so =  tp->t_inpcb->inp_socket;
#ifdef TCP_SACK
   int   sack_resend;
   int   sack_hole = 0;    /* next sack hole to fill */
   if(tp->t_flags & TF_SACKREPLY) {
      /* we are resending based on a received SACK header */
      sack_resend = 1;
      tp->t_flags &= ~TF_SACKREPLY;    /* clear flag */
   } else {
     sack_resend = 0;
   }
#endif /* TCP_SACK */

   /*
    * Determine length of data that should be transmitted,
    * and flags that will be used.
    * If there is some data or critical controls (SYN, RST)
    * to send, then transmit; otherwise, investigate further.
    */
   idle = (tp->snd_max == tp->snd_una);

again:
  sendalot = 0;
  off = (int)(tp->snd_nxt - tp->snd_una);
  win = tp->snd_wnd;   /* set basic send window */
  if (win > (I32)tp->snd_cwnd) { // See if we need congestion control
    win = tp->snd_cwnd & ~3; /* keep data aligned */
  }

   /*
    * If in persist timeout with window of 0, send 1 byte.
    * Otherwise, if window is small but nonzero
    * and timer expired, we will send what we can
    * and go to transmit state.
    */
   if (tp->t_force) {
     if (win == 0) {
       win = 1;
     } else {
       tp->t_timer[TCPT_PERSIST] = 0;
       tp->t_rxtshift = 0;
     }
   }

#ifdef TCP_SACK
   /* See if we need to adjust the offset for a sack resend */
   if(sack_resend) {
      off = (int)(tp->sack_hole_start[sack_hole] - tp->snd_una);
      /* if this hole's already been acked then punt and move to next hole */
      if(off < 0) {
         /* clear out the acked hole */
         tp->sack_hole_start[sack_hole] = tp->sack_hole_end[sack_hole] = 0;
         /* see if we're done with SACK hole list (2 tests) */
         if(++sack_hole >= SACK_BLOCKS)
            return 0;
         if(tp->sack_hole_start[sack_hole] == tp->sack_hole_end[sack_hole])
            return 0;
         goto again;
      }
      tp->snd_nxt = tp->sack_hole_start[sack_hole];
      len = (int)(tp->sack_hole_end[sack_hole] - tp->sack_hole_start[sack_hole]);
      len = (int)MIN(len, (int)win);
   } else
#endif /* TCP_SACK */
   {
      /* set length of packets which are not sack resends */
      len = (int)MIN(so->so_snd.NumBytes, (unsigned)win) - off;
   }
   flags = tcp_outflags[tp->t_state];

   optlen = _BuildOptions(tp, &tcp_optionbuf[0], flags, so);
   optlenRem = optlen;      // Number of Opt-bytes whcih still need to be copied
   if (len < 0) {
      /*
       * If FIN has been sent but not acked,
       * but we haven't been called to retransmit,
       * len will be -1.  Otherwise, window shrank
       * after we sent into it.  If window shrank to 0,
       * cancel pending retransmit and pull snd_nxt
       * back to (closed) window.  We will enter persist
       * state below.  If the window didn't close completely,
       * just wait for an ACK.
       */
      len = 0;
      if (win == 0) {
         tp->t_timer[TCPT_REXMT] = 0;
         tp->snd_nxt = tp->snd_una;
      }
   }

   if (len > (int)tp->TrueMSS) {
     len = tp->TrueMSS;
     sendalot = 1;
   }


   if (SEQ_LT(tp->snd_nxt + len, tp->snd_una + so->so_snd.NumBytes)) {
     flags &= ~TH_FIN;
   }
   win = sbspace(&so->so_rcv);
  IP_LOG((IP_MTYPE_TCP_RXWIN, "TCP: Rx Window space: %u.", win));

   //
   // If our state indicates that FIN should be sent
   // and we have not yet done so, or we're retransmitting the FIN,
   // then we need to send.
   //
   if (flags & TH_FIN) {
     if (so->so_snd.NumBytes == 0) {
       if ((tp->t_flags & TF_SENTFIN) == 0 || tp->snd_nxt == tp->snd_una) {
         goto send;
       }
     }
   }
   //
   // Send if we owe peer an ACK.
   //
   if (tp->t_flags & TF_ACKNOW) {
     goto send;
   }
   if (flags & (TH_SYN | TH_RST)) {
     goto send;
   }
   //
   // Sender silly window avoidance.  If connection is idle and can send all data, a maximum segment,
   // at least a maximum default-size segment do it, or are forced, do it; otherwise don't bother.
   // If peer's buffer is tiny, then send when window is at least half open.
   // If retransmitting (possibly after persist timer forced us
   // to send into a small window), then must resend.
   //
   if (len) {
     if (len == (int)tp->TrueMSS) {               // Full segment ?   -> Send
       goto send;
     }
     if (idle || (tp->t_flags & TF_NODELAY)) {    // Connection Idle ? -> Send
       if ((len + off) >= (int)so->so_snd.NumBytes) {
         goto send;
       }
     }
     if (tp->t_force) {                           // Forced to send ?  -> Send
       goto send;
     }
     if (tp->snd_wnd >= (tp->max_sndwnd / 2)) {   // Peer's window at least half open ?
       goto send;
     }
     if (SEQ_LT(tp->snd_nxt, tp->snd_max)) {
       goto send;
     }
   }

   //
   // Compare available window to amount of window
   // known to peer (as advertised window less
   // next expected input).  If the difference is at least two
   // max size segments or at least 35% of the maximum possible
   // window, then want to send a window update to peer.
   //
   if (win > 0) {
     I32 AdvWin;
     I32 WinDelta;


     AdvWin = tp->rcv_adv -  tp->rcv_nxt;   // Advertised window
     WinDelta  =  win -  AdvWin;            // Delta can only be positive
     if (WinDelta < 0) {
       IP_WARN((IP_MTYPE_TCP, "Advertised window size shrunk, delta < 0"));
     }
     if ((so->so_rcv.NumBytes == 0) && (WinDelta >= (int)(tp->TrueMSS * 2))) {
       goto send;
     }
     if (100uL * WinDelta  >= 35uL * so->so_rcv.Limit) {
       goto send;
     }
   }

   /*
    * TCP window updates are not reliable, rather a polling protocol
    * using ``persist'' packets is used to insure receipt of window
    * updates.  The three ``states'' for the output side are:
    *   idle         not doing retransmits or persists
    *   persisting      to move a small or zero window
    *   (re)transmitting   and thereby not persisting
    *
    * tp->t_timer[TCPT_PERSIST]
    *   is set when we are in persist state.
    * tp->t_force
    *   is set when we are called to send a persist packet.
    * tp->t_timer[TCPT_REXMT]
    *   is set when we are retransmitting
    * The output side is idle when both timers are zero.
    *
    * If send window is too small, there is data to transmit, and no
    * retransmit or persist is pending, then go to persist state.
    * If nothing happens soon, send when timer expires:
    * if window is nonzero, transmit what we can,
    * otherwise force out a byte.
    */
   if (so->so_snd.NumBytes && tp->t_timer[TCPT_REXMT] == 0 &&  tp->t_timer[TCPT_PERSIST] == 0) {
      tp->t_rxtshift = 0;
      tcp_setpersist(tp);
   }

   /*
    * No reason to send a segment, just return.
    */
   return 0;

send:
  ASSERT_LOCK();

  //
  // Limit send length to the current buffer so as to
  // avoid doing the "mbuf shuffle" in m_copy().
  //
  bufoff = off;
  sendm = so->so_snd.sb_mb;

  if (len) {
    /* find mbuf containing data to send (at "off") */
    while (1) { /* loop through socket send list */
      if (!sendm) {
        IP_PANIC("TCP: Internal: No out buffer");
      }
      if (bufoff < sendm->m_len) {  /* if off is in this buffer, break */
        break;
      }
      bufoff -= sendm->m_len;
      sendm = sendm->m_next;
    }


    /* if socket has multiple unsent mbufs, set flag for send to loop */
    if ((sendm->m_next) && (len > (int)sendm->m_len)) {
     flags &= ~TH_FIN; /* don't FIN on segment prior to last */
     sendalot = 1;     /* set to send more segments */
    }
    if ((flags & TH_FIN) && (so->so_snd.NumBytes > (unsigned)len)) {
     /* This can happen on slow links (PPP) which retry the last
      * segment - the one with the FIN bit attached to data.
      */
      flags &= ~TH_FIN; /* don't FIN on segment prior to last */
    }

    len = IP_MIN(len, (int)sendm->m_len);       // only send the rest of msend

    //
    //  If we are not sending from offset 0, we need to copy the data.
    //
    if (bufoff) {
      len = IP_MIN(len, (int)(sendm->m_len - bufoff));   /* limit len again */
      //
      // If we are low on big buffers, make sure our send fits into a little buffer
      //
      if ((len > (int)(IP_Global.aBufferConfigSize[0] - IP_TCP_HEADER_SIZE)) && (IP_Global.aFreeBufferQ[1].q_len < 2))    {
        len = IP_Global.aBufferConfigSize[0] - IP_TCP_HEADER_SIZE;
      }
    }
  }

  //
  // Create MBuf with packet to send.
  // Ideally (in most cases), we can prepend the packet buffer and increment use count.
  // If this does not work, we need to copy it to a fresh buffer.
  //
  if (len && (bufoff == 0) && (((unsigned)sendm->m_data & 3) == 0)  && (sendm->pkt->UseCnt == 1)) {
    //
    // Send data is sufficiently aligned in packet, prepend TCP/IP header
    // in the space provided.
    //
    m = MBUF_GET(MT_TXDATA);      /* get an empty mbuf to "clone" the data */
    if (!m) {
      return ENOBUFS;
    }
    m->pkt = sendm->pkt; /* copy packet location in new mbuf */
    m->pkt->UseCnt++;     /* bump packet's use count */
    m->m_len  = len +  TCPIPHDRSZ + optlen;  /* adjust clone for header */
    m->m_data = sendm->m_data;
    if (optlen) {
      m->m_data -= optlen;
      IP_MEMCPY(m->m_data, tcp_optionbuf, optlen);
      optlenRem = 0;
    }
    m->m_data -= TCPIPHDRSZ;
  } else {
    //
    // Either no data or data is not front aligned in mbuf
    // Grab a header mbuf, attaching a copy of data to be
    // transmitted, and initialize the header from
    // the template for sends on this connection.
    //
    int NumBytes;
    int SizeLNH;
    SizeLNH   = tp->t_inpcb->ifp->n_lnh;       // Typically 16 bytes for MAC header and padding
    NumBytes  = SizeLNH;
    NumBytes += TCPIPHDRSZ;                    // 40 bytes for IP and TCP headers
    NumBytes += tp->OptLen;                    // Option len depends on negotiated options, typically either 0 or 12 bytes for RTTM
    m = MBUF_GET_WITH_DATA(MT_HEADER, NumBytes);
    if (m ==(struct mbuf *)NULL) {
      return ENOBUFS;
    }

    m->m_len   = TCPIPHDRSZ;
    m->m_data += SizeLNH;         // Leave enough space so we can prepend packet with local header

    if (len) { /* attach any data to send */
      m->m_next = m_copy(so->so_snd.sb_mb, off, (int) len, IP_TCP_HEADER_SIZE);
      if (m->m_next == 0) {
        m_freem(m);
        return ENOBUFS;
      }
    }
  }


  //
  // Update some statistic counters
  //
  if (len) {
   if (tp->t_force && len == 1) {
     IP_STAT_INC(IP_TCP_Stat.sndprobe);
   } else if (SEQ_LT(tp->snd_nxt, tp->snd_max)) {
     IP_STAT_INC(IP_TCP_Stat.sndrexmitpack);
     IP_STAT_ADD(IP_TCP_Stat.sndrexmitbyte, len);
#ifdef TCP_SACK
     if(sack_resend) {
       IP_STAT_INC(IP_TCP_Stat.sackresend);
     }
#endif
    } else {
       IP_STAT_INC(IP_TCP_Stat.sndpack);
       IP_STAT_ADD(IP_TCP_Stat.sndbyte, len);
    }
  } else if (tp->t_flags & TF_ACKNOW) {
   IP_STAT_INC(IP_TCP_Stat.sndacks);
  } else if (flags & (TH_SYN|TH_FIN|TH_RST)) {
   IP_STAT_INC(IP_TCP_Stat.sndctrl);
  } else {
   IP_STAT_INC(IP_TCP_Stat.sndwinup);
  }
  ti = (struct tcpiphdr *)(m->m_data+sizeof(struct ip)-sizeof(struct ipovly));
  if ((char *)ti < m->pkt->pBuffer) {
    IP_PANIC("tcp_out- packet ptr underflow");
  }

  IP_MEMCPY((char*)ti, (char*)tp->t_template, sizeof(struct tcpiphdr));

   /*
    * Fill in fields, remembering maximum advertised
    * window for use in delaying messages about window sizes.
    * If resending a FIN, be sure not to use a new sequence number.
    */
   if (flags & TH_FIN && tp->t_flags & TF_SENTFIN && tp->snd_nxt == tp->snd_max) {
      tp->snd_nxt--;
   }

   ti->ti_seq = IP_HTONL_FAST(tp->snd_nxt);
   ti->ti_ack = IP_HTONL_FAST(tp->rcv_nxt);

   /*
    * If we're sending a SYN, check the IP address of the interface
    * that we will (likely) use to send the IP datagram -- if it's
    * changed from what is in the template (as it might if this is
    * a retransmission, and the original SYN caused PPP to start
    * bringing the interface up, and PPP has got a new IP address
    * via IPCP), update the template and the inpcb with the new
    * address.
    */
   if (flags & TH_SYN) {
      switch(so->so_domain) {
      case AF_INET:
      {

#ifdef INCLUDE_PPP
        struct inpcb * inp;
        inp = (struct inpcb *)so->so_pcb;
        ip_addr src;

        if(((flags & TH_ACK) == 0) && /* SYN only, not SYN/ACK */
            (inp->ifp) &&              /* Make sure we have iface */
            (inp->ifp->mib.ifType == PPP))   /* only PPP type */
        {
          src = ip_mymach(ti->ti_dst.s_addr);
          if (src != ti->ti_src.s_addr) {
             ti->ti_src.s_addr = src;
             tp->t_template->ti_src.s_addr = src;
             tp->t_inpcb->inp_laddr.s_addr = src;
          }
        }
#endif   /* INCLUDE_PPP */

        break;
      }


      default:
        IP_WARN((IP_MTYPE_TCP_OUT, "TCP: bad domain setting"));
      }
   }

   //
   // Fill in options if any are set
   //
   if (optlenRem) {
      struct mbuf * mopt;

      mopt = MBUF_GET_WITH_DATA(MT_TXDATA, optlen);
      if (mopt == NULL) {
         m_freem(m);
         return ENOBUFS;
      }

      /* insert options mbuf after after tmp_mbuf */
      mopt->m_next = m->m_next;
      m->m_next = mopt;

      IP_MEMCPY(MTOD(mopt, char*), tcp_optionbuf, optlen);
      mopt->m_len = optlen;
   }

   ti->ti_t.th_doff = (20 + optlen) << 2;

   ti->ti_flags = (U8)flags;
   //
   // Calculate receive window. Don't shrink window,
   // but avoid silly window syndrome.
   //
   if ((win < (I32)so->so_rcv.Limit / 4) && (win < tp->TrueMSS)) {
     win = 0;
   }
   if (win < (long)(tp->rcv_adv - tp->rcv_nxt)) {
     win = (long)(tp->rcv_adv - tp->rcv_nxt);
   }
   IP_LOG((IP_MTYPE_TCP_RXWIN, "TCP: Rx Window: %u.", win));

#if 0 // RS: No longer required since we free receive buffers if necessary
  if (IP_Global.aFreeBufferQ[1].q_len == 0) {  /* If queue length is 0, set window to 0 */
    win = 0;
  } else {
    U32 BufferSpace;
    BufferSpace = (IP_Global.aFreeBufferQ[1].q_len - 1) * IP_aBufferConfigSize[1];
    if (win > BufferSpace) {
      win = BufferSpace;
    }
  }
#endif


  {
    unsigned Shift = 0;
#if TCP_WIN_SCALE
    if (tp->t_flags & TF_WINSCALE) {
      Shift = tp->rcv_wind_scale;
    }
#endif
    ti->ti_win = (U16)htons(win >> Shift); /* apply scale */
  }

   /*
    * If anything to send and we can send it all, set PUSH.
    * (This will keep happy those implementations which only
    * give data to the user when a buffer fills or a PUSH comes in.)
    */
   if (len && off+len == (int)so->so_snd.NumBytes) {
     ti->ti_flags |= TH_PUSH;
   }

   /*
    * In transmit state, time the transmission and arrange for
    * the retransmit.  In persist state, just set snd_max.
    */
   if ((tp->t_force == 0) || (tp->t_timer[TCPT_PERSIST] == 0)) {
      U32 startseq = tp->snd_nxt;

      /*
       * Advance snd_nxt over sequence space of this segment.
       */
      if (flags & TH_SYN) {
        tp->snd_nxt++;
      }

      if (flags & TH_FIN) {
        tp->snd_nxt++;
        tp->t_flags |= TF_SENTFIN;
      }
      tp->snd_nxt += len;
      if (SEQ_GT(tp->snd_nxt, tp->snd_max)) {
         tp->snd_max = tp->snd_nxt;
         /*
          * Time this transmission if not a retransmission and
          * not currently timing anything.
          */
         if (tp->t_rttick == 0) {
            tp->t_rttick = IP_OS_GetTime32();
            tp->t_rtseq = startseq;
            IP_STAT_INC(IP_TCP_Stat.segstimed);
         }
      }

      /*
       * Set retransmit timer if not currently set,
       * and not doing an ack or a keep-alive probe.
       * Initial value for retransmit timer is smoothed
       * round-trip time + 2 * round-trip time variance.
       * Initialize shift counter which is used for backoff
       * of retransmit time.
       */
      if (tp->t_timer[TCPT_REXMT] == 0 && tp->snd_nxt != tp->snd_una) {
        IP_TCP_StartREXMTTimer(tp);
        if (tp->t_timer[TCPT_PERSIST]) {
          tp->t_timer[TCPT_PERSIST] = 0;
          tp->t_rxtshift = 0;
        }
      }
   } else {
     if (SEQ_GT(tp->snd_nxt + len, tp->snd_max)) {
       tp->snd_max = tp->snd_nxt + len;
     }
   }

   IP_LOG((IP_MTYPE_TCP_OUT, "TCP_OUT: Sending, state %d, tcpcb: %x", tp->t_state, tp ));

   error = 0;    // SHould not be required, just to keep compiler happy

#ifdef IP_V4
   {
      struct ip * pip;
      pip = MTOD(m, struct ip *);
      /* Fill in IP length and send to IP level. */
      pip->ip_len = (U16)(TCPIPHDRSZ + optlen + len);
      error = ip_output(m);
   }
#endif   /* IP_V4 */

   if (error) {
     if (error == ENOBUFS) {  /* ip_output needed a copy buffer it couldn't get */
       tcp_quench(tp->t_inpcb);
     }
     return error;
   }

   //
   // Data sent (as far as we can tell).
   //

   IP_STAT_INC(tcpmib.tcpOutSegs);   /* keep MIB stats */
   IP_STAT_INC(IP_TCP_Stat.sndtotal);

#ifdef TCP_SACK
   /* If we're doing a sack driven resend then update the sack info.*/
   if (sack_resend) {
      /* snd_nxt has been maintined by the above code to reflect
       * the amount of data space covered by the send, so we use
       * it to update sack_start.
       */
      if((tp->sack_hole_start[sack_hole] + len) != tp->snd_nxt) {
        IP_WARN((IP_MTYPE_TCP, "TCP: SACK error"));
      }
      tp->sack_hole_start[sack_hole] = tp->snd_nxt;

      /* If the sack hole is filled, move to next hole */
      if(tp->sack_hole_start[sack_hole] >= tp->sack_hole_end[sack_hole])  {
         /* set next byte to send to end of hole */
         tp->snd_nxt = tp->sack_hole_end[sack_hole];
         /* Done with hole, flag this by clearing out values */
         tp->sack_hole_start[sack_hole] = tp->sack_hole_end[sack_hole] = 0;
         sack_hole++;      /* move on to next hole */

         /* If next hole has no set data then we are done */
         if(tp->sack_hole_start[sack_hole] == tp->sack_hole_end[sack_hole]) {
            sendalot = 0;     /* done with sack-based resent */
         }
      }
   }
#endif /* TCP_SACK */

#if TCP_TIMESTAMP
   /* save last ACK to peer for timestamp features */
   if (tp->t_flags & TF_TIMESTAMP) {
     tp->last_ack = IP_HTONL_FAST(ti->ti_ack);
   }
#endif   /* TCP_TIMESTAMP */

   /*
    * If this advertises a larger window than any other segment,
    * then remember the size of the advertised window.
    * Any pending ACK has now been sent.
    */
  if (win > 0 && SEQ_GT(tp->rcv_nxt+win, tp->rcv_adv)) {
    tp->rcv_adv = tp->rcv_nxt + (unsigned)win;
  }
  tp->t_flags &= ~(TF_ACKNOW|TF_SACKNOW|TF_DELACK);
  if (sendalot){
    goto again;
  }
  return 0;
}



/*********************************************************************
*
*       tcp_setpersist()
*/
void tcp_setpersist(struct tcpcb * tp) {
  int   t;

  t = ((tp->t_srtt >> 2) + tp->t_rttvar) >> 1;

  if (tp->t_timer[TCPT_REXMT]) {
    IP_PANIC("tcp_output REXMT");
  }
  /*
  * Start/restart persistance timer.
  */
  tp->t_timer[TCPT_PERSIST] = (U16)  IP_BringInBounds(t * tcp_backoff[tp->t_rxtshift], TCPTV_PERSMIN, TCPTV_PERSMAX);
  if (tp->t_rxtshift < TCP_MAXRXTSHIFT) {
    tp->t_rxtshift++;
  }
}



#endif /* INCLUDE_TCP */


/*************************** End of file ****************************/

