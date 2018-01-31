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
File    : IP_TCP_usr.c
Purpose :
--------  END-OF-HEADER  ---------------------------------------------
*/

/* Additional Copyrights: */
/* Copyright 1997- 2000 By InterNiche Technologies Inc. All rights reserved. */
/* Copyright (c) 1982, 1986 Regents of the University of California. */
/** All rights reserved.
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

/*********************************************************************
*
*       Debug vars (used in higher debug levels only)
*
**********************************************************************
*/

struct tcpcb * IP_LastTP;

/* FUNCTION: tcp_attach()
 *
 * Attach TCP protocol to socket, allocating
 * internet protocol control block, tcp control block,
 * bufer space, and entering LISTEN state if to accept connections.
 *
 * PARAM1: struct socket *so
 *
 * RETURNS: 0 if OK, or nonzero error code.
 */

static int tcp_attach(struct socket * so) {
  struct tcpcb * tp;
  struct inpcb * inp;
  int   error;

  ASSERT_LOCK();
  if (so->so_snd.Limit == 0 || so->so_rcv.Limit == 0) {
    error = soreserve(so, IP_Global.TCP_TxWindowSize, IP_Global.TCP_RxWindowSize);
    if (error) {
      return (error);
    }
  }
  error = IP_TCP_PCB_alloc(so, &IP_tcb);
  if (error) {
    return (error);
  }
  inp = so->so_pcb;
  tp = tcp_newtcpcb(inp);
  if (tp == 0) {
    int   nofd  =  so->so_state   &  SS_NOFDREF; /* XXX */

    so->so_state &= ~SS_NOFDREF;     /* don't free the socket yet */
    IP_TCP_PCB_detach(inp);
    so->so_state |= nofd;
    return (ENOBUFS);
  }
  tp->t_state = TCPS_CLOSED;
  return (0);
}



/* FUNCTION: tcp_usrclosed()
 *
 * User issued close, and wish to trail through shutdown states:
 * if never received SYN, just forget it.  If got a SYN from peer,
 * but haven't sent FIN, then go to FIN_WAIT_1 state to send peer a FIN.
 * If already got a FIN from peer, then almost done; go to LAST_ACK
 * state.  In all other cases, have already sent FIN to peer (e.g.
 * after PRU_SHUTDOWN), and just have to play tedious game waiting
 * for peer to send FIN or not respond to keep-alives, etc.
 * We can let the user exit from the close as soon as the FIN is acked.
 *
 * PARAM1: struct tcpcb *tp
 *
 * RETURNS:
 */

static struct tcpcb * tcp_usrclosed(struct tcpcb * tp) {

  ASSERT_LOCK();
  switch (tp->t_state) {
  case TCPS_CLOSED:
  case TCPS_LISTEN:
  case TCPS_SYN_SENT:
    tp->t_state = TCPS_CLOSED;
    tp = tcp_close(tp);
    break;

  case TCPS_SYN_RECEIVED:
  case TCPS_ESTABLISHED:
    tp->t_state = TCPS_FIN_WAIT_1;
    break;

  case TCPS_CLOSE_WAIT:
    tp->t_state = TCPS_LAST_ACK;
    break;
  }
  if (tp && tp->t_state >= TCPS_FIN_WAIT_2)
    soisdisconnected(tp->t_inpcb->inp_socket);
  return (tp);
}

/* FUNCTION: tcp_disconnect()
 *
 * Initiate (or continue) disconnect.
 * If embryonic state, just send reset (once).
 * If in ``let data drain'' option and linger null, just drop.
 * Otherwise (hard), mark socket disconnecting and drop
 * current input data; switch states based on user close, and
 * send segment to peer (with FIN).
 *
 *
 * PARAM1: struct tcpcb *tp
 *
 * RETURNS:
 */

static struct tcpcb * tcp_disconnect(struct tcpcb * tp) {
  struct socket *   so =  tp->t_inpcb->inp_socket;

  ASSERT_LOCK();
  if (tp->t_state < TCPS_ESTABLISHED) {
    tp = tcp_close(tp);
  } else if ((so->so_options & SO_LINGER) && so->so_linger == 0) {
    tp = tcp_drop(tp, 0);
  } else {
    soisdisconnecting(so);
    sbflush(&so->so_rcv);
    tp = tcp_usrclosed(tp);
    if (tp) {
      tcp_output(tp);
    }
  }
  return tp;
}


/* FUNCTION: tcp_usrreq()
 *
 * Process a TCP user request for TCP tb.  If this is a send request
 * then m is the mbuf chain of send data.  If this is a timer expiration
 * (called from the software clock routine), then timertype tells which timer.
 *
 * PARAM1: struct socket *so
 * PARAM2: struct mbuf *m
 * PARAM3: struct mbuf *nam
 *
 * RETURNS: 0 if OK, or nonzero socket error code.
 */

int tcp_usrreq(struct socket * so, struct mbuf *  m, struct mbuf *  nam) {
   struct inpcb * inp;
   struct tcpcb * tp;
   int   error =  0;
   int   req;

#ifdef DO_TCPTRACE
   int   ostate;
#endif

  ASSERT_LOCK();
   req = so->so_req;    /* get request from socket struct */
   inp = so->so_pcb;
   /*
    * When a TCP is attached to a socket, then there will be
    * a (struct inpcb) pointed at by the socket, and this
    * structure will point at a subsidary (struct tcpcb).
    */
   if ((inp == 0) && (req != PRU_ATTACH)) {
     return EINVAL;
   }

   tp = NULL;  /* stifle compiler warnings about using unassigned tp*/
   if (inp) {
     tp = inp->inp_ppcb;
   }

   switch (req) {
   /*
    * TCP attaches to socket via PRU_ATTACH, reserving space,
    * and an internet control block.
    */
   case PRU_ATTACH:
      if (inp) {
         error = EISCONN;
         break;
      }
      error = tcp_attach(so);
      if (error) {
        break;
      }
      if ((so->so_options & SO_LINGER) && so->so_linger == 0) {
        so->so_linger = TCP_LINGERTIME;
      }
#ifdef   DO_TCPTRACE
      SETTP(tp, sototcpcb(so));
#endif
      break;

   /*
    * PRU_DETACH detaches the TCP protocol from the socket.
    * If the protocol state is non-embryonic, then can't
    * do this directly: have to initiate a PRU_DISCONNECT,
    * which may finish later; embryonic TCB's can just
    * be discarded here.
    */
   case PRU_DETACH:
      if (tp->t_state > TCPS_LISTEN)
         SETTP(tp, tcp_disconnect(tp));
      else
         SETTP(tp, tcp_close(tp));
      break;

   /*
    * Give the socket an address.
    */
   case PRU_BIND:

      /* bind is quite different for IPv4 and v6, so we use two
       * seperate pcbbind routines. so_domain was checked for
       * validity way up in t_bind()
       */
#ifdef IP_V4
      if(inp->inp_socket->so_domain == AF_INET) {
        error = IP_TCP_PCB_bind(inp, nam);
        break;
      }
#endif /* IP_V4 */

      IP_WARN((IP_MTYPE_TCP, "TCP: Illegal domain in tcp_usrreq()"));
      error = EINVAL;
      break;
   /*
    * Prepare to accept connections.
    */
   case PRU_LISTEN:
      if (inp->inp_lport == 0) {
        error = IP_TCP_PCB_bind(inp, (struct mbuf *)0);
      }
      if (error == 0) {
        tp->t_state = TCPS_LISTEN;
      }
      break;

   /*
    * Initiate connection to peer.
    * Create a template for use in transmissions on this connection.
    * Enter SYN_SENT state, and mark socket as connecting.
    * Start keep-alive timer, and seed output sequence space.
    * Send initial segment on connection.
    */
   case PRU_CONNECT:
      if (inp->inp_lport == 0) {

#ifdef IP_V4
      error = IP_TCP_PCB_bind(inp, (struct mbuf *)0);
#endif   /* end v6 only */

         if (error)
            break;
      }

#ifdef IP_V4
      error = IP_TCP_PCB_connect(inp, nam);
#endif   /* end v6 only */

      if (error)
         break;
      tp->t_template = tcp_template(tp);
      if (tp->t_template == 0)
      {

#ifdef IP_V4
         IP_TCP_PCB_disconnect(inp);
#endif   /* end v6 only */

         error = ENOBUFS;
         break;
      }

      soisconnecting(so);
      IP_STAT_INC(IP_TCP_Stat.connattempt);
      tp->t_state = TCPS_SYN_SENT;
      IP_TCP_StartKEEPTimer(tp, IP_TCP_KeepInit);
      tp->iss = tcp_iss;
      tcp_iss += (TCP_ISSINCR/2);
      tp->snd_una = tp->iss;
      tp->snd_nxt = tp->iss;
      tp->snd_max = tp->iss;
      error = tcp_output(tp);
      if (!error)
         IP_STAT_INC(tcpmib.tcpActiveOpens);     /* keep MIB stats */
      break;

   /*
    * Create a TCP connection between two sockets.
    */
   case PRU_CONNECT2:
      error = EOPNOTSUPP;
      break;

   /*
    * Initiate disconnect from peer.
    * If connection never passed embryonic stage, just drop;
    * else if don't need to let data drain, then can just drop anyways,
    * else have to begin TCP shutdown process: mark socket disconnecting,
    * drain unread data, state switch to reflect user close, and
    * send segment (e.g. FIN) to peer.  Socket will be really disconnected
    * when peer sends FIN and acks ours.
    *
    * SHOULD IMPLEMENT LATER PRU_CONNECT VIA REALLOC TCPCB.
    */
   case PRU_DISCONNECT:
      SETTP(tp, tcp_disconnect(tp));
      break;

  //
  // Accept a connection.
  // Essentially all the work is done at higher levels; just return the address
  // of the peer, storing through addr.
  //
  case PRU_ACCEPT:
    {
      struct sockaddr_in * sin   =  (struct sockaddr_in *) nam;    // Parameter passed is really a pointer in this case (called by accept only)

      sin->sin_family = AF_INET;
      sin->sin_port   = inp->inp_fport;
      sin->sin_addr   = inp->inp_faddr;
      IP_STAT_INC(tcpmib.tcpPassiveOpens);    /* keep MIB stats */
      break;
    }

   //
   // Mark the connection as being incapable of further output.
   //
   case PRU_SHUTDOWN:
      socantsendmore(so);
      tp = tcp_usrclosed(tp);
      if (tp) {
        error = tcp_output(tp);
      }
      break;

   /*
    * After a receive, possibly send window update to peer.
    */
   case PRU_RCVD:
     tcp_output(tp);
     break;

   /*
    * Do a send by putting data in output queue and updating urgent
    * marker if URG set.  Possibly send more data.
    */
   case PRU_SEND:


#if IP_DEBUG
  IP_LastTP = tp;
#endif


     if (so->so_pcb == NULL) {                    /* Return EPIPE error if socket is not connected */
       error = EPIPE;
       break;
     }
     sbappend(&so->so_snd, m);
     error = tcp_output(tp);
     if (error == ENOBUFS) {
       sbdropend(&so->so_snd,m);  /* Remove data from socket buffer */
     }
     break;

   //
   // Abort the TCP.
   //
   case PRU_ABORT:
      SETTP(tp, tcp_drop(tp, ECONNABORTED));
      break;

   case PRU_SENSE:
      /*      ((struct stat *) m)->st_blksize = so->so_snd.Limit; */
      IP_WARN((IP_MTYPE_TCP, "TCP: Internal: PRU_SENSE"));
      return (0);

   case PRU_SOCKADDR:

   /* sockaddr and peeraddr have to switch based on IP type */
#ifdef IP_V4
      in_setsockaddr(inp, nam);
#endif
      break;

   case PRU_PEERADDR:
#ifdef IP_V4
      in_setpeeraddr(inp, nam);
#endif
      break;

   case PRU_SLOWTIMO:
      IP_TCP_OnTimer(tp, (int)MBUF2LONG(nam));
#if IP_DEBUG > 0
      req |= (long)nam << 8;        /* for debug's sake */
#endif
      break;

  default:
    IP_PANIC("tcp_usrreq");
  }
  return error;
}





#endif /* INCLUDE_TCP */

/*************************** End of file ****************************/
