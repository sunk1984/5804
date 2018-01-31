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
File    : IP_TCP_timr.c
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



#include "IP_Int.h"
#include "IP_sockvar.h"

#include "IP_protosw.h"
#include "IP_TCP_pcb.h"
#include "IP_TCP.h"


/*********************************************************************
*
*       Public data
*
**********************************************************************
*/
U32 IP_TCP_Msl             = IP_TCP_MSL;
U32 IP_TCP_RetransDelayMin = IP_TCP_RETRANS_MIN;
U32 IP_TCP_RetransDelayMax = IP_TCP_RETRANS_MAX;
U32 IP_TCP_KeepInit        = IP_TCP_KEEPALIVE_INIT;
U32 IP_TCP_KeepIdle        = IP_TCP_KEEPALIVE_IDLE;
U32 IP_TCP_KeepPeriod      = IP_TCP_KEEPALIVE_PERIOD;
U32 IP_TCP_KeepMaxReps     = IP_TCP_KEEPALIVE_MAX_REPS;
U32 tcp_iss;


#ifdef DO_DELAY_ACKS

/*********************************************************************
*
*       IP_TCP_FastTimer()
*
* Fast timeout routine for processing delayed acks.
* This should be called every IP_TCP_FAST_TIMEOUT ms
* to get the delayed ACK timing correct.
*/
void IP_TCP_FastTimer(void) {
  struct inpcb * inp;
  struct tcpcb * tp;
  inp = IP_tcb.inp_next;
  if (inp) {
    for (; inp != &IP_tcb; inp = inp->inp_next) {
      tp = (struct tcpcb *)inp->inp_ppcb;
      if (tp) {
        if (tp->t_flags & TF_DELACK) {
          int v;
          v = tp->DelayAckRem;
          v -= IP_TCP_DACK_PERIOD;
          if (v < 0) {
            v = 0;
            tp->t_flags &= ~TF_DELACK;
            tp->t_flags |= TF_ACKNOW;
            IP_STAT_INC(IP_TCP_Stat.delack);
            tcp_output(tp);
          }
          tp->DelayAckRem = v;
        }
      }
    }
  }
}
#endif   /* DO_DELAY_ACKS */

/*********************************************************************
*
*       IP_TCP_SlowTimer()
*
* Tcp protocol timeout routine called periodically.
* Updates the timers in all active tcb's and
* causes finite state machine actions if timers expire.
*/
void IP_TCP_SlowTimer(void) {
   struct inpcb * ip, * ipnxt;
   struct tcpcb * tp;
   int   i;
   struct socket * so, * sonext;
   struct sockbuf *  sb;

   /* search through open sockets */
   for (so = (struct socket *)IP_Global.SocketInUseQ.q_head; so != NULL; so = sonext) {
      sonext = so->next;

      /* for SOCK_STREAM (TCP) sockets, we must do slow-timeout
       * processing and (optionally) processing of pending
       * zero-copy socket upcalls.
       */
      if (so->so_type == SOCK_STREAM) {
         ip = so->so_pcb;
         if (!ip) {
            continue;
         }
         ipnxt = ip->inp_next;

         tp = so->so_pcb->inp_ppcb;
         if (!tp) {
            continue;
         }

         for (i = 0; i < TCPT_NTIMERS; i++) {
           if (tp->t_timer[i]) {
             if (--tp->t_timer[i] == 0) {
               /* call usrreq to do actual work */
               so->so_req = PRU_SLOWTIMO;
               tcp_usrreq(so, (struct mbuf *)0, LONG2MBUF((long)i));

               /* If ip disappeared on us, handle it */
               if (ipnxt->inp_prev != ip) {
                 goto tpgone;
               }
             }
           }
         }

#ifdef TCP_ZEROCOPY
         /* also nudge sockets which may have missed an upcall. We have
          * to test for a variety of problems, as ending up here usually
          * means the system doesn't have enough buffers to be doing this
          * well.
          */
         if (so->rx_upcall) {         /* If call back set... */
            U32 ready = so->so_rcv.NumBytes;
            /* If socket has data, try to deliver it to app */
            if (ready > 0) {
              IP_TCP_DataUpcall(so);
              if (ready != so->so_rcv.NumBytes) {  /* did app accept any data? */
                tcp_output(tp);   /* may need to push out a Window update */
              }
            } else {           // else, no current data...
               /* if the connection is shutting down, but the application
                * hasn't been informed, do so now
                */
               if ((tp->t_state > TCPS_ESTABLISHED) && ((so->so_state & SS_UPCFIN) == 0)) {
                  so->rx_upcall(IP_SOCKET_p2h(so), NULL, ESHUTDOWN);
                  so->so_state |= SS_UPCFIN;
               }
            }
         }
#endif   /* TCP_ZEROCOPY */

         tp->IdleCnt++;
      }

      /* wake up anyone sleeping in a select() involving this socket */
      sb = &so->so_rcv;
      if (sb->sb_flags & SB_SEL) {
         select_wait = 0;
         IP_OS_SignalItem ((char *)&select_wait);
         sb->sb_flags &= ~SB_SEL;
      }
      sb = &so->so_snd;
      if (sb->sb_flags & SB_SEL) {
         select_wait = 0;
         IP_OS_SignalItem ((char *)&select_wait);
         sb->sb_flags &= ~SB_SEL;
      }

      /* wake any thread with a timer going for a connection state change */
      IP_OS_SignalItem((char*)&so->so_timeo);

tpgone:
      ;
   }

  tcp_iss += (unsigned)(TCP_ISSINCR/PR_SLOWHZ);      /* increment iss */
  if (tcp_iss & 0xff000000) {
    tcp_iss = 0L;
  }
}


/* FUNCTION: tcp_canceltimers()
 *
 * Cancel all timers for TCP tp.
 *
 * PARAM1: struct tcpcb *tp
 *
 * RETURNS:
 */

void tcp_canceltimers(struct tcpcb * tp) {
  int   i;

  for (i = 0; i < TCPT_NTIMERS; i++) {
    tp->t_timer[i] = 0;
  }
}

const unsigned char tcp_backoff [TCP_MAXRXTSHIFT + 1] = { 1, 2, 4, 8, 16, 32, 64, 64, 64, 64, 64, 64, 64 };







/*********************************************************************
*
*       _On2MSLTimer()
*
*  Function description
*    2MSL timer is expired.
*    Handle timeout conditions for TCP states.
*    RFC793 specifies a timeout of 2msl for the final transition from TIME_WAIT to CLOSED only,
*    but in reality, we also need to have timeouts for FIN_WAIT1 and FIN_WAIT2
*/

static void _On2MSLTimer(struct tcpcb * tp) {
  int State;

  State = tp->t_state;
  switch (State) {
  case TCPS_TIME_WAIT:
  case TCPS_FIN_WAIT_1:
  case TCPS_FIN_WAIT_2:
    tp = tcp_close(tp);
    break;
  default:
    ;                       // No action in other states
  }
}



/*********************************************************************
*
*       IP_TCP_OnTimer()
*
*  Function description
*    TCP timer processing.
*    One of the 4 TCP timers is expired, we need to take the appropriate action.
*/
void IP_TCP_OnTimer(struct tcpcb * tp, int timer) {
  int   rexmt;
  switch (timer) {

  case TCPT_2MSL:
    _On2MSLTimer(tp);
    break;

  //
  // Retransmission timer went off.  Message has not
  // been acked within retransmit interval.  Back off
  // to a longer retransmit interval and retransmit one segment.
  //
  case TCPT_REXMT:
      IP_STAT_INC(tcpmib.tcpRetransSegs);     /* keep MIB stats */
      if (++tp->t_rxtshift > TCP_MAXRXTSHIFT) {
         tp->t_rxtshift = TCP_MAXRXTSHIFT;
         IP_STAT_INC(IP_TCP_Stat.timeoutdrop);
         tp = tcp_drop(tp, ETIMEDOUT);
         break;
      }
      IP_STAT_INC(IP_TCP_Stat.rexmttimeo);
      rexmt = ((tp->t_srtt >> 2) + tp->t_rttvar) >> 1;
      rexmt *= tcp_backoff[tp->t_rxtshift];
      IP_TCP_SetRetransDelay(tp, rexmt);
      IP_TCP_StartREXMTTimer(tp);
      /*
       * If losing, let the lower level know and try for
       * a better route.  Also, if we backed off this far,
       * our srtt estimate is probably bogus.  Clobber it
       * so we'll take the next rtt measurement as our srtt;
       * move the current srtt into rttvar to keep the current
       * retransmit times until then. Don't clobber with rtt
       * if we got it from a timestamp option.
       */
      if ((tp->t_rxtshift > TCP_MAXRXTSHIFT / 4) && ((tp->t_flags & TF_TIMESTAMP) == 0)) {
         tp->t_rttvar += (tp->t_srtt >> 2);
         tp->t_srtt = 0;
      }
      tp->snd_nxt = tp->snd_una;
      /*
       * If timing a segment in this window, stop the timer.
       */
      tp->t_rttick = 0;
      /*
       * Close the congestion window down to one segment
       * (we'll open it by one segment for each ack we get).
       * Since we probably have a window's worth of unacked
       * data accumulated, this "slow start" keeps us from
       * dumping all that data as back-to-back packets (which
       * might overwhelm an intermediate gateway).
       *
       * There are two phases to the opening: Initially we
       * open by one mss on each ack.  This makes the window
       * size increase exponentially with time.  If the
       * window is larger than the path can handle, this
       * exponential growth results in dropped packet(s)
       * almost immediately.  To get more time between
       * drops but still "push" the network to take advantage
       * of improving conditions, we switch from exponential
       * to linear window opening at some threshhold size.
       * For a threshhold, we use half the current window
       * size, truncated to a multiple of the mss.
       *
       * (the minimum cwnd that will give us exponential
       * growth is 2 mss.  We don't allow the threshhold
       * to go below this.)
       *
       * Vers 1.9 - Skip slow start if the SO_NOSLOWSTART socket option
       * is set.
       */
      if ((tp->t_inpcb->inp_socket->so_options & SO_NOSLOWSTART) == 0) {
        U32 win = MIN(tp->snd_wnd, tp->snd_cwnd);
        if (tp->TrueMSS == 0) {
          win = win / 2;
        } else {
          win = win / 2 / tp->TrueMSS;	// Truncate to MSS
        }
        if (win < 2) {
          win = 2;
        }
        tp->snd_cwnd = tp->TrueMSS;
        tp->snd_ssthresh = (U16)win * tp->TrueMSS;
      }
      tcp_output(tp);
      break;

   /*
    * Persistance timer into zero window.
    * Force a byte to be output, if possible.
    */
   case TCPT_PERSIST:
      IP_STAT_INC(IP_TCP_Stat.persisttimeo);
      tcp_setpersist(tp);
      tp->t_force = 1;
      tcp_output(tp);
      tp->t_force = 0;
      break;

   /*
    * Keep-alive timer went off; send something
    * or drop connection if idle for too long.
    */
    case TCPT_KEEP:
      IP_STAT_INC(IP_TCP_Stat.keeptimeo);
      if (tp->t_state < TCPS_ESTABLISHED) {
         goto dropit;
      }
      if ((tp->t_inpcb->inp_socket->so_options & SO_KEEPALIVE) && (tp->t_state <= TCPS_CLOSE_WAIT)) {
        if (tp->IdleCnt * IP_TCP_SLOW_PERIOD >= (I32)(IP_TCP_KeepIdle + IP_TCP_KeepPeriod * IP_TCP_KeepMaxReps)) {
          goto dropit;
        }
        //
        // Send a packet designed to force a response if the peer is up and reachable:
        // either an ACK if the connection is still alive, or an RST if the peer has closed the connection
        // due to timeout or reboot.
        // Using sequence number tp->snd_una-1 causes the transmitted zero-length segment
        // to lie outside the receive window; by the protocol spec, this requires the
        // correspondent TCP to respond.
        //
        IP_STAT_INC(IP_TCP_Stat.keepprobe);
        //
        // The keepalive packet must have nonzero length to get a 4.2 host to respond.
        //
        tcp_respond(tp, tp->t_template, tp->rcv_nxt - 1,  tp->snd_una - 1, 0, (struct mbuf *)NULL);
        IP_TCP_StartKEEPTimer(tp, IP_TCP_KeepPeriod);
      } else {
        IP_TCP_StartKEEPTimer(tp, IP_TCP_KeepIdle);
      }
      break;
dropit:
      IP_STAT_INC(IP_TCP_Stat.keepdrops);
      tcp_drop (tp, ETIMEDOUT);
      break;
   }
}

/*********************************************************************
*
*       IP_TCP_StartREXMTTimer()
*
*  Function description
*    Start the retransmission timer.
*/
void IP_TCP_StartREXMTTimer(struct tcpcb * tp) {
  tp->t_timer[TCPT_REXMT] = (U16)(tp->RetransDelay / IP_TCP_SLOW_PERIOD);
}

/*********************************************************************
*
*       IP_TCP_StartKEEPTimer()
*
*  Function description
*    Start the keep-alive timer.
*/
void IP_TCP_StartKEEPTimer(struct tcpcb * tp, U32 TimeOut) {
  tp->t_timer[TCPT_KEEP] = (U16) (TimeOut / IP_TCP_SLOW_PERIOD);
}

/*********************************************************************
*
*       IP_TCP_Start2MSLTimer()
*
*  Function description
*    Start the 2MSL timer.
*    The term 2MSL means twice the MSL, maximum segment lifetime.
*/
void IP_TCP_Start2MSLTimer(struct tcpcb * tp) {
  U32 Timeout;

  Timeout = 2 * IP_TCP_Msl;            // Time in ms
  Timeout /= IP_TCP_SLOW_PERIOD;       // Convert into number of calls
  tp->t_timer[TCPT_2MSL] = (U16)Timeout;
}

/*********************************************************************
*
*       IP_TCP_SetRetransDelay()
*
*  Function description
*    Set retransmission delay.
*/
void IP_TCP_SetRetransDelay(struct tcpcb * tp, unsigned Delay) {
  if (Delay < IP_TCP_RetransDelayMin) {
    Delay = IP_TCP_RetransDelayMin;
  }
  if (Delay > IP_TCP_RetransDelayMax) {
    Delay = IP_TCP_RetransDelayMax;
  }
  tp->RetransDelay = Delay;
}

/*********************************************************************
*
*       IP_TCP_SetRetransDelayRange()
*
*  Function description
*    Sets retransmission delay range.
*/
void IP_TCP_SetRetransDelayRange(unsigned RetransDelayMin, unsigned RetransDelayMax) {
  IP_TCP_RetransDelayMin = RetransDelayMin;
  IP_TCP_RetransDelayMax = RetransDelayMax;
}

/*********************************************************************
*
*       IP_TCP_SetConnKeepaliveOpt()
*
*  Function description
*    Sets the keepalive options
*/
void IP_TCP_SetConnKeepaliveOpt(U32 Init, U32 Idle, U32 Period, U32 MaxRep) {
  IP_TCP_KeepInit        = Init;        // Max. time for TCP-connection open (response to SYN) [ms]
  IP_TCP_KeepIdle        = Idle;        // Time of TCP-inactivity before first keep alive is sent [ms]
  IP_TCP_KeepPeriod      = Period;      // Time of TCP-inactivity between keep alive packages [ms]
  IP_TCP_KeepMaxReps     = MaxRep;      // Number of keep alives before we give up and close the connection
}



/*************************** End of file ****************************/
