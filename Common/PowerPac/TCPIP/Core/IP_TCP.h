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
File    : IP_TCP.h
Purpose :
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



#ifndef _TCPIP_H
#define  _TCPIP_H 1

#if defined(__cplusplus)
extern "C" {     /* Make sure we have C-declarations in C++ programs */
#endif

/*
 * TCP header.
 * Per RFC 793, September, 1981.
 */

struct tcphdr {
   U16  th_sport;      /* source port */
   U16  th_dport;      /* destination port */
   U32  th_seq;        /* sequence number */
   U32  th_ack;        /* acknowledgement number */
   U8   th_doff;       /* data offset: high 4 bits only */
   U8   th_flags;
   U16     th_win;     /* window */
   U16     th_sum;     /* checksum */
   U16     th_urp;     /* urgent pointer */
};

/*
 * Tcp/Ip-like header, after ip options removed. First part of IPv4
 * header if overwritten with double linked list links.
 */
struct ipovly {
  struct ipovly *   ih_next;    /* list link */
  struct ipovly *   ih_prev;    /* list link */
  U16  padword;             /* unused - for compilers that don't pad structures */
  U16  ih_len;              /* protocol length */
  struct   in_addr  ih_src;     /* source internet address */
  struct   in_addr  ih_dst;     /* destination internet address */
};




struct tcpiphdr {
  struct  ipovly ti_i; /* overlaid ip structure */
  struct tcphdr  ti_t; /* tcp header */
};

/* The following definition is somewhat mis-named, since it really
 * indicates the size of the overlay structure, not the IP head.
 * on 32 bit IPv4 systems this is usually the same, but on IPv6 or
 * systems with poiunters which are not 32 bits these sizes may
 * NOT be the same.
 */

#define TCPIPHDRSZ 40

#define  ti_next  ti_i.ih_next
#define  ti_prev  ti_i.ih_prev
#define  ti_len   ti_i.ih_len
#define  ti_src   ti_i.ih_src
#define  ti_dst   ti_i.ih_dst
#define  ti_sport ti_t.th_sport
#define  ti_dport ti_t.th_dport
#define  ti_seq   ti_t.th_seq
#define  ti_ack   ti_t.th_ack


#define  ti_flags ti_t.th_flags
#define  ti_win   ti_t.th_win
#define  ti_sum   ti_t.th_sum
#define  ti_urp   ti_t.th_urp


#ifdef IPCORE_C
  #define EXTERN
#else
  #define EXTERN extern
#endif


extern U32 IP_TCP_RetransDelayMin;
extern U32 IP_TCP_RetransDelayMax;
extern U32 IP_TCP_KeepInit;
extern U32 IP_TCP_KeepIdle;
extern U32 IP_TCP_KeepPeriod;
extern U32 IP_TCP_KeepMaxReps;      // Max. number of repetitions of "Keep alive" packets
extern U32 IP_TCP_Msl;

#ifndef  IP_MAXPACKET
#define  IP_MAXPACKET   (24<<10)    /* maximum packet size */
#endif

#ifndef  SACK_BLOCKS    /* allow ipport.h to override */
#define  SACK_BLOCKS    3
#endif   /*  SACK_BLOCKS */

extern   U8   default_wind_scale;  /* default scaling value */

//
// Definitions of the TCP timers.  These timers are counted
// down PR_SLOWHZ times a second.
//

#define     TCPT_REXMT     0     /* retransmit */
#define     TCPT_PERSIST   1     /* retransmit persistance */
#define     TCPT_KEEP      2     /* keep alive */
#define     TCPT_2MSL      3     /* 2*msl quiet time timer */
#define     TCPT_NTIMERS   4







#define     TCP_NSTATES    11

#define     TCPS_CLOSED          0  /* closed */
#define     TCPS_LISTEN          1  /* listening for connection */
#define     TCPS_SYN_SENT        2  /* active, have sent syn */
#define     TCPS_SYN_RECEIVED    3  /* have send and received syn */
/* states < TCPS_ESTABLISHED are those where connections not established */
#define     TCPS_ESTABLISHED     4  /* established */
#define     TCPS_CLOSE_WAIT      5  /* rcvd fin, waiting for close */
/* states > TCPS_CLOSE_WAIT are those where user has closed */
#define     TCPS_FIN_WAIT_1      6  /* have closed, sent fin */
#define     TCPS_CLOSING      7  /* closed xchd FIN; await FIN ACK */
#define     TCPS_LAST_ACK        8  /* had fin and close; await FIN ACK */
/* states > TCPS_CLOSE_WAIT && < TCPS_FIN_WAIT_2 await ACK of FIN */
#define     TCPS_FIN_WAIT_2      9  /* have closed, fin is acked */
#define     TCPS_TIME_WAIT       10 /* in 2*msl quiet wait after close */

#define     TCPS_HAVERCVDSYN(s)     ((s)  >= TCPS_SYN_RECEIVED)
#define     TCPS_HAVERCVDFIN(s)     ((s)  >= TCPS_TIME_WAIT)

#define     TH_FIN   0x01
#define     TH_SYN   0x02
#define     TH_RST   0x04
#define     TH_PUSH     0x08
#define     TH_ACK   0x10
#define     TH_URG   0x20

#define     TCPOOB_HAVEDATA   0x01
#define     TCPOOB_HADDATA    0x02


/*
 * The TCPT_REXMT timer is used to force retransmissions.
 * The TCP has the TCPT_REXMT timer set whenever segments
 * have been sent for which ACKs are expected but not yet
 * received.  If an ACK is received which advances tp->snd_una,
 * then the retransmit timer is cleared (if there are no more
 * outstanding segments) or reset to the base value (if there
 * are more ACKs expected).  Whenever the retransmit timer goes off,
 * we retransmit one unacknowledged segment, and do a backoff
 * on the retransmit timer.
 *
 * The TCPT_PERSIST timer is used to keep window size information
 * flowing even if the window goes shut.  If all previous transmissions
 * have been acknowledged (so that there are no retransmissions in progress),
 * and the window is too small to bother sending anything, then we start
 * the TCPT_PERSIST timer.  When it expires, if the window is nonzero,
 * we go to transmit state.  Otherwise, at intervals send a single byte
 * into the peer's window to force him to update our window information.
 * We do this at most as often as TCPT_PERSMIN time intervals,
 * but no more frequently than the current estimate of round-trip
 * packet time.  The TCPT_PERSIST timer is cleared whenever we receive
 * a window update from the peer.
 *
 * The TCPT_KEEP timer is used to keep connections alive.  If an
 * connection is idle (no segments received) for IP_TCP_KEEPALIVE_INIT amount of time,
 * but not yet established, then we drop the connection.  Once the connection
 * is established, if the connection is idle for IP_TCP_KEEPALIVE_KEEP_IDLE time
 * (and keepalives have been enabled on the socket), we begin to probe
 * the connection.  We force the peer to send us a segment by sending:
 *   <SEQ=SND.UNA-1><ACK=RCV.NXT><CTL=ACK>
 * This segment is (deliberately) outside the window, and should elicit
 * an ack segment in response from the peer.  If, despite the TCPT_KEEP
 * initiated segments we cannot elicit a response from a peer in TCPT_MAXIDLE
 * amount of time probing, then we drop the connection.
 */

   /* these macros get/set the raw value, usually 5 */
#define  GET_TH_OFF(th) (th.th_doff >> 4)


#define  TCPOPT_EOL        0
#define  TCPOPT_NOP        1
#define  TCPOPT_MAXSEG     2
#define  TCPOPT_WINSCALE   3
#define  TCPOPT_SACKOK     4
#define  TCPOPT_SACKDATA   5
#define  TCPOPT_RTT        8

/*********************************************************************
*
*       Time constants.
*/
#define     TCPTV_SRTTBASE    0     /* base roundtrip time; if 0, no idea yet */

#define  TCPTV_SRTTDFLT    (3*PR_SLOWHZ)        /* assumed RTT if no info */
#define  TCPTV_PERSMIN     (5*PR_SLOWHZ)        /* retransmit persistance */
#define  TCPTV_PERSMAX     (60*PR_SLOWHZ)       /* maximum persist interval */
#define  TCP_LINGERTIME    120                  /* linger at most 2 minutes */
#define  TCP_MAXRXTSHIFT   12                   /* maximum retransmits */

extern   const unsigned char  tcp_backoff[TCP_MAXRXTSHIFT   +  1];



/*
 * Tcp control block, one per tcp; fields:
 */
typedef struct tcpcb {
   struct   tcpiphdr *  seg_next;   /* sequencing queue */
   struct   tcpiphdr *  seg_prev;
   U8       t_state;             /* state of this connection */
   U8       t_rxtshift;          /* log(2) of rexmt exp. backoff */
   U8       t_dupacks;           /* consecutive dup acks recd */
   U8       t_force;             /* 1 if forcing out a byte */
   U16      t_timer[TCPT_NTIMERS];  /* tcp timers */
   U16      Mss;                 // Maximum segment size. This is always MTU - 40, acc. to RFC 879: THE TCP MAXIMUM SEGMENT SIZE IS THE IP MAXIMUM DATAGRAM SIZE MINUS FORTY.
   U16      OptLen;              // Option len. typically 0 for no options, or 12 with  RTTM
   U16      TrueMSS;             // True MSS: Mss - OptLen
   U16      t_flags;             /* mask of the TF_ state bits below */
   struct tcpiphdr * t_template; /* skeletal packet for transmit */
   struct inpcb * t_inpcb;       /* back pointer to internet pcb */
   U32      RetransDelay;        // Retransmission delay in ms
   /*
    * The following fields are used as in the protocol specification.
    * See RFC783, Dec. 1981, page 21.
    */
   /* send sequence variables */
   U32     snd_una;    /* send unacknowledged */
   U32     snd_nxt;    /* send next */
   U32     snd_wl1;    /* window update seg seq number */
   U32     snd_wl2;    /* window update seg ack number */
   U32     iss;        /* initial send sequence number */
   U32     snd_wnd;    /* send window */
   /* receive sequence variables */
   U32     rcv_adv;    /* advertised window */
   U32     rcv_wnd;    /* receive window */
   U32     rcv_nxt;    /* receive next */
   U32     irs;        /* initial receive sequence number */
   /* retransmit variables */
   /* highest sequence number sent used to recognize retransmits */
   U32   snd_max;
   /* congestion control (for slow start, source quench, retransmit after loss) */
   U32  snd_cwnd;      /* congestion-controlled window */
   U32  snd_ssthresh;   // snd_cwnd size threshhold for for slow start exponential to linear switch
   /*
    * transmit timing stuff.
    * srtt and rttvar are stored as fixed point; for convenience in smoothing,
    * srtt has 3 bits to the right of the binary point, rttvar has 2.
    * "Variance" is actually smoothed difference.
    */
   int      IdleCnt;       // Inactivity time counter. Incremented with every "slow tick"
   U32      t_rttick;      /* cticks if timing RTT, else 0 */
   U32      t_rtseq;       /* sequence number being timed */
   int      t_srtt;        /* smoothed round-trip time */
   int      t_rttvar;      /* variance in round-trip time */
   U32      max_rcvd;      /* most peer has sent into window */
   U32      max_sndwnd;    /* largest window peer has offered */
#if TCP_TIMESTAMP
   U32  ts_recent;           /* RFC-1323 TS.Recent (peer's timestamp) */
   U32  last_ack;            /* RFC-1323 Last.ACK (last ack to peer) */
#endif   /* TCP_TIMESTAMP */

#ifdef DO_DELAY_ACKS
   U16     DelayAckPeriod;        // Period for delaying ACK [ms]. 0 means no delayed acknowledges.
   U16     DelayAckRem;           // Time left for delaying this ACK [ms]
#endif   /* DO_DELAY_ACKS */

#ifdef TCP_SACK
   /* seq numbers in last SACK option sent. sack_seq[0] is last segment
    * which triggered a SACK.
    */
   U32  sack_seq[SACK_BLOCKS];
   int      sack_seqct;    /* number of valid sack_seq[] entries */

   /* start & end of current SACK hole(s) to fill with a send. If these are
    * both zero then there is no SACK hole specified for the send. These
    * are updated whever a sack header is received and cleared when the
    * missing segments are send by the rexmit timer.
    */
   U32  sack_hole_start[SACK_BLOCKS];
   U32  sack_hole_end[SACK_BLOCKS];

   /* The seq number in our send stream which triggered the last sack
    * option header from the other side.
    */
   U32  sack_trigger;
#endif /* TCP_SACK */

#if TCP_WIN_SCALE
   /* Scales factors if TF_TIMESTAMP bit is set */
   U8   snd_wind_scale;      /* RFC-1323 Snd.Wind.Scale (S) */
   U8   rcv_wind_scale;      /* RFC-1323 Rcv.Wind.Scale (R) */
#endif   /* TCP_WIN_SCALE */
} TCPCB;

extern struct inpcb IP_tcb;                // head of TCP control blocks

/* bit values for tcpcb.t_flags field */
#define     TF_ACKNOW      0x0001     /* ack peer immediately */
#define     TF_DELACK      0x0002     /* ack, but try to delay it */
#define     TF_NODELAY     0x0004     /* don't delay packets to coalesce */
#define     TF_SENTFIN     0x0010     /* have sent FIN */
#define     TF_SACKOK      0x0020     /* negotiated SACK on this conn.*/
#define     TF_SACKNOW     0x0040     /* send SACK option packet now */
#define     TF_WINSCALE    0x0080     /* negotiated a scaled window */
#define     TF_TIMESTAMP   0x0100     /* negotiated timestamp option */
#define     TF_SACKREPLY   0x0200     /* send reply to peer's sack options */


int      tcp_buildsack(struct tcpcb * tp, U8 * opt);
void     tcp_resendsack(U8 * opt, struct tcpcb * tp);
void     tcp_setsackinfo(struct tcpcb * tp, struct tcpiphdr * ti);

#define     sototcpcb(so)     (so->so_pcb->inp_ppcb)

/*
 * TCP statistics.
 * Many of these should be kept per connection,
 * but that's inconvenient at the moment.
 */
typedef struct {
   U32   connattempt;    /* connections initiated */
   U32   accepts;        /* connections accepted */
   U32   connects;       /* connections established */
   U32   drops;          /* connections dropped */
   U32   conndrops;      /* embryonic connections dropped */
   U32   closed;         /* conn. closed (includes drops) */
   U32   segstimed;      /* segs where we tried to get rtt */
   U32   rttupdated;     /* times we succeeded */
   U32   delack;         /* delayed acks sent */
   U32   timeoutdrop;    /* conn. dropped in rxmt timeout */
   U32   rexmttimeo;     /* retransmit timeouts */
   U32   persisttimeo;   /* persist timeouts */
   U32   keeptimeo;      /* keepalive timeouts */
   U32   keepprobe;      /* keepalive probes sent */
   U32   keepdrops;      /* connections dropped in keepalive */

   U32   sndtotal;       /* total packets sent */
   U32   sndpack;        /* data packets sent */
   U32   sndbyte;        /* data bytes sent */
   U32   sndrexmitpack;  /* data packets retransmitted */
   U32   sndrexmitbyte;  /* data bytes retransmitted */
   U32   sndacks;        /* ack-only packets sent */
   U32   sndprobe;       /* window probes sent */
   U32   sndurg;         /* packets sent with URG only */
   U32   sndwinup;       /* window update-only packets sent */
   U32   sndctrl;        /* control (SYN|FIN|RST) packets sent */

   U32   rcvtotal;       /* total packets received */
   U32   rcvpack;        /* packets received in sequence */
   U32   rcvbyte;        /* bytes received in sequence */
   U32   rcvbadsum;      /* packets received with ccksum errs */
   U32   rcvbadoff;      /* packets received with bad offset */
   U32   rcvshort;       /* packets received too short */
   U32   rcvduppack;     /* duplicate-only packets received */
   U32   rcvdupbyte;     /* duplicate-only bytes received */
   U32   rcvpartduppack; /* packets with some duplicate data */
   U32   rcvpartdupbyte; /* dup. bytes in part-dup. packets */
   U32   rcvoopack;      /* out-of-order packets received */
   U32   rcvoobyte;      /* out-of-order bytes received */
   U32   rcvpackafterwin;   /* packets with data after window */
   U32   rcvbyteafterwin;   /* bytes rcvd after window */
   U32   rcvafterclose;  /* packets rcvd after "close" */
   U32   rcvwinprobe;    /* rcvd window probe packets */
   U32   rcvdupack;      /* rcvd duplicate acks */
   U32   rcvacktoomuch;  /* rcvd acks for unsent data */
   U32   rcvackpack;     /* rcvd ack packets */
   U32   rcvackbyte;     /* bytes acked by rcvd acks */
   U32   rcvwinupd;      /* rcvd window update packets */

   U32   mcopies;        /* m_copy() actual copies */
   U32   mclones;        /* m_copy() clones */
   U32   mcopiedbytes;   /* m_copy() # bytes copied */
   U32   mclonedbytes;   /* m_copy() # bytes cloned */

   U32   oprepends;      /* ip_output() prepends of header to data */
   U32   oappends;       /* ip_output() appends of data to header */
   U32   ocopies;        /* ip_output() copies */
   U32   predack;        /* VJ predicted-header acks */
   U32   preddat;        /* VJ predicted-header data packets */
   U32   zioacks;        /* acks recvd during zio sends */
#ifdef TCP_SACK
   U32   sackconn;       /* connections which negotiated SACK */
   U32   sacksent;       /* SACK option headers sent */
   U32   sackresend;     /* segs resent because of recv SACKs */
   U32   sackrcvd;       /* SACK option headers received */
   U32   sackmaxblk;     /* most SACK blocks in a received option field */
#endif /* TCP_SACK */
} IP_TCP_STAT;

extern   IP_TCP_STAT IP_TCP_Stat; /* tcp statistics */

void              IP_TCP_OnTimer(struct tcpcb *, int);
struct tcpcb *    tcp_close (struct tcpcb *);
struct tcpcb *    tcp_drop  (struct tcpcb *, int);
struct tcpcb *    tcp_newtcpcb (struct inpcb *);

int   tcp_usrreq    (struct socket *, struct mbuf *, struct mbuf *);
int   tcp_output    (struct tcpcb *);
int   tcp_ctloutput (int, struct socket *, int, int, void *);
void  tcp_input     (struct mbuf *, NET * ifp);

unsigned IP_TCP_CalcChecksum(struct ip * pip);

void  IP_TCP_FastTimer(void);
void  IP_TCP_SlowTimer(void);
void  IP_TCP_StartREXMTTimer(struct tcpcb * tp);
void  IP_TCP_StartKEEPTimer(struct tcpcb * tp, U32 Start);
void  IP_TCP_Start2MSLTimer(struct tcpcb * tp);
void  IP_TCP_SetRetransDelay(struct tcpcb * tp, unsigned Delay);

void  tcp_canceltimers(struct tcpcb *);
void  tcp_init (void);
void  tcp_setpersist (struct tcpcb *);
void  tcp_notify (struct inpcb *);
void  tcp_quench (struct inpcb *);
void  tcp_respond (struct tcpcb *, struct tcpiphdr *, U32, U32, int, struct mbuf *);
struct tcpiphdr * tcp_template (struct tcpcb *);

/*
 * TCP sequence numbers are 32 bit integers operated
 * on with modular arithmetic.  These macros can be
 * used to compare such integers.
 */
#define     SEQ_LT(a,b)    ((long)((a)-(b))  <  0)
#define     SEQ_LEQ(a,b)   ((long)((a)-(b))  <= 0)
#define     SEQ_GT(a,b)    ((long)((a)-(b))  >  0)
#define     SEQ_GEQ(a,b)   ((long)((a)-(b))  >= 0)

/*
 * Macros to initialize tcp sequence numbers for
 * send and receive from initial send and receive
 * sequence numbers.
 */
#define   tcp_rcvseqinit(tp)  (tp)->rcv_adv = (tp)->rcv_nxt = (tp)->irs + 1

#define     TCP_ISSINCR    (long)(0x0001F4FF)   /* increment for tcp_iss each second */
extern   U32     tcp_iss;    /* tcp initial send seq # */

#if defined(__cplusplus)
  }
#endif


#endif // Avoid multiple inclusion

/*************************** End of file ****************************/


