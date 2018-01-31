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
File    : IP_UDPSock.c
Purpose : Glue layer lightweight UDP API and UDP under sockets.
          This maps the sockets calls - sendto(), recvfrom(), etc - to the
          lightweight API for sockets purists.
--------  END-OF-HEADER  ---------------------------------------------
*/

/* Additional Copyrights: */
/* Copyright  2000 By InterNiche Technologies Inc. All rights reserved */
/* Portions Copyright 1997 by InterNiche Technologies Inc. All rights reserved. */

#include "IP_Int.h"

#ifdef INCLUDE_TCP  /* include/exclude whole file at compile time */
#ifdef UDP_SOCKETS   /* can ifdef away whole file */

#include "IP_mbuf.h"    /* sockets/BSDish includes */
#include "IP_socket.h"
#include "IP_sockvar.h"
#include "IP_protosw.h"

/* list of actice UDP "connections" */
extern   UDPCONN  firstudp;

#ifdef IP_V4
int udp4_sockbind(struct socket *so, struct mbuf *nam, int req);
int udp4_socksend(struct socket *so, struct mbuf *m, struct mbuf *nam);
int udp4_sockaddr(struct socket *so, struct mbuf *nam , int req);
#endif



/* FUNCTION: udp_lookup()
 *
 * udp_lookup() - Return socket's matching UDP connection in
 * ..\inet\udp's internal list. Return NULL if not found.
 *
 * PARAM1: struct socket * so
 *
 * RETURNS:
 */

static UDPCONN udp_lookup(struct socket * so) {
   UDPCONN tmp;

   for (tmp = firstudp; tmp; tmp = tmp->u_next)
      if (tmp->u_data == (void*)so)
      return (tmp);

   return NULL;   /* didn't find it */
}

/*********************************************************************
*
*       udp_soinput()
*
* UDP upcall handler for UDP connections mapped to sockets. Return 0
* if OK, else nonzero error code.
*
* RETURNS:
*/
static int _udp_soinput(IP_PACKET * pkt, void * so_ptr) {
   struct mbuf *  m_in;    /* packet/data mbuf */
   struct socket *   so =  (struct  socket *)so_ptr;
   struct sockaddr_in   sin;
   struct udp *   udpp;

   LOCK_NET();

   /* make sure we're not flooding input buffers */
   if ((so->so_rcv.NumBytes + pkt->NumBytes) >= so->so_rcv.Limit) {
      UNLOCK_NET();
      return ENOBUFS;
   }

   /* alloc mbuf for received data */
   m_in = MBUF_GET(MT_RXDATA);
   if (!m_in) {
      UNLOCK_NET();
      return ENOBUFS;
   }

   /* set data mbuf to point to start of UDP data */
   m_in->pkt = pkt;
   m_in->m_data  = pkt->pData;
   m_in->m_len   = pkt->NumBytes;

   /* fill in net address info for pass to socket append()ers */
   sin.sin_addr.s_addr = pkt->fhost;
   udpp = (struct udp *)(pkt->pData - sizeof(struct udp));
   sin.sin_port = htons(udpp->ud_srcp);
   sin.sin_family = AF_INET;

   /* attempt to append address information to mbuf */
   if (!sbappendaddr(&so->so_rcv, (struct sockaddr *)&sin, m_in)) {
      /* set the pkt field in the mbuf to NULL so m_free() below wont
       * free the packet buffer, because that is left to the
       */
      m_in->pkt = NULL;
      /* free only the mbuf itself */
      m_free(m_in);
      /* return error condition so caller can free the packet buffer */
      UNLOCK_NET();
      return ENOBUFS;
   }

   IP_OS_SignalItem(&so->so_rcv);   /* wake anyone waiting for this */

   sorwakeup(so);    /* wake up selects too */

   UNLOCK_NET();
   return 0;
}


/*********************************************************************
*
*       udp_soinput()
*
* UDP upcall handler for UDP connections mapped to sockets.
*
* RETURNS:
*   IP_OK_KEEP_PACKET if packet is in socket queue
*   < 0               Error
*/
static int udp_soinput(IP_PACKET * pkt, void * so_ptr) {
  int r;
  r = _udp_soinput(pkt, so_ptr);
  if (r == 0) {
    r = IP_OK_KEEP_PACKET;
  }
  return r;
}


/* FUNCTION: udp_usrreq()
 *
 * proto_tab's usr_req call function for UDP protocol. Maps the
 * sockets requests into NetPort lightweight UDP interface. Returns 0
 * if no error, negative "NP" error if lower layers fail, postive
 * error from nptcp.h if TCP/Sockets fails.
 *
 *
 * PARAM1: struct socket * so
 * PARAM2: struct mbuf * m
 * PARAM3: struct mbuf * nam
 *
 * RETURNS:
 */

int udp_usrreq(struct socket * so, struct mbuf *  m,  struct mbuf *  nam) {
   UDPCONN udpconn = (UDPCONN)NULL;
   int   req;

   req = so->so_req;    /* get request from socket struct */

   switch (req) {
   case PRU_ATTACH:
      /* fake small windows so sockets asks us to move data */
      so->so_rcv.Limit = so->so_snd.Limit = udp_maxalloc();

#ifdef IP_V4
      if (so->so_domain  == AF_INET){
        udpconn = IP_UDP_Open(0L, 0, IP_UDP_FindFreePort(), udp_soinput, so);
      }
#endif

      if (!udpconn)
         return(EINVAL);
      return 0;
   case PRU_DETACH:
      /* delete the NetPort UDP connection */
      udpconn = udp_lookup(so);
      if (!udpconn) {
         return(EINVAL);
      }
      IP_UDP_Close(udpconn);
      return 0;
   case PRU_CONNECT:
      /* Install foreign port for UDP, making a virtual connection */
      /* fall to shared bind logic */
   case PRU_BIND:
      /* do bind parameters lookups and tests */
      if (nam == NULL)
         return(EINVAL);
#ifdef IP_V4
      if (so->so_domain == AF_INET){
        return udp4_sockbind(so, nam, req );
      }
#endif

      IP_WARN((IP_MTYPE_UDP, "UDP: Expected AF_INET on bind"));
      return EINVAL;
   case PRU_SEND:
      /* do parameter lookups and tests */
      if (!m)  /* no data passed? */
         return(EINVAL);
#ifdef IP_V4
      if (so->so_domain == AF_INET){
        return udp4_socksend(so, m, nam );
      }
#endif

      IP_WARN((IP_MTYPE_UDP, "UDP: Expected AF_INET on send"));
      return EINVAL;

   case PRU_SOCKADDR:
      /* fall through to share PRU_PEERADDR prefix */
   case PRU_PEERADDR:
      if (nam == NULL)
         return(EINVAL);
#ifdef IP_V4
      if (so->so_domain == AF_INET){
        return udp4_sockaddr(so, nam, req );
      }
#endif

      IP_WARN((IP_MTYPE_UDP, "UDP: Expected AF_INET on peeraddr"));
      return EINVAL;

   case PRU_DISCONNECT:
   case PRU_RCVD:
      IP_WARN((IP_MTYPE_UDP, "UDP: Illegal command"));
      return 0;
   case PRU_LISTEN:     /* don't support these for UDP */
   case PRU_ACCEPT:
   default:
      return EOPNOTSUPP;
   }
}

#ifdef IP_V4
int udp4_sockbind(struct socket *so, struct mbuf *nam, int req )
{
  struct sockaddr_in * sin;
  UDPCONN udpconn;
  UDPCONN udptmp;
  U16  fport;   /* foreign port (local byte order) */
  U16  lport;   /* local port (local byte order) */
  ip_addr fhost; /* host to send to/recv from (network byte order) */
  ip_addr lhost; /* local IP address to bind to (network byte order) */
  NET * ifp;

  sin = MTOD(nam, struct sockaddr_in *);
  if (sin == NULL)
    return(EINVAL);
  if (nam->m_len != sizeof (*sin))
    return(EINVAL);
  udpconn = udp_lookup(so);
  if (!udpconn)
     return(EINVAL);
  if (req == PRU_BIND) {
    /* bind the socket to a local UDP port,
     * and optionally a local interface IP address.
     *
     * if the caller-supplied port is 0, try to get
     * the port from the UDP endpoint, or pick a new
     * unique port; else, use the caller-supplied
     * port
     */
    if (sin->sin_port == 0) {
      if (udpconn->u_lport != 0) {
        lport = udpconn->u_lport;
      } else {
        lport = IP_UDP_FindFreePort();
      }
    } else {
      lport = ntohs(sin->sin_port);
    }
    /* if the caller-supplied address is INADDR_ANY,
     * don't bind to a specific address; else,
     * make sure the caller-supplied address is
     * an interface IP address and if so, bind to that
     */
    if (sin->sin_addr.s_addr == INADDR_ANY) {
      lhost = 0L;
    } else {
      lhost = sin->sin_addr.s_addr;
      /* verify that lhost is a local interface address */
#if 0  // Code for multiple interfaces
      for (ifp = (NET*)(netlist.q_head); ifp; ifp = ifp->n_next) {
        if (ifp->n_ipaddr == lhost) {
          break;
        }
      }
      if (ifp == NULL) {
         return EADDRNOTAVAIL;
      }
#else
      ifp = &IP_aIFace[0];
      if (ifp->n_ipaddr != lhost) {
        return(EADDRNOTAVAIL);
      }
#endif
    }

    /* make sure we're not about to collide with an
     * existing binding
     */
    if (!(so->so_options & SO_REUSEADDR))
      for (udptmp = firstudp; udptmp; udptmp = udptmp->u_next)
        if ((udptmp->u_lport == lport) && (udptmp != udpconn))
          return(EADDRINUSE);
    /* bind the UDP endpoint */
    udpconn->u_lport = lport;
    udpconn->u_lhost = lhost;
  } else {  /* PRU_CONNECT */
    /* connect the socket to a remote IP address and
     * UDP port.
     */
    fport = ntohs(sin->sin_port);
    /* if the caller-supplied address is INADDR_ANY,
     * use the wildcard address; else, use the caller-
     * supplied address
     */
    if (sin->sin_addr.s_addr == INADDR_ANY)
      fhost = 0L;
    else
      fhost = sin->sin_addr.s_addr;
    /* prepare to bind the socket to the appropriate
     * local interface address for the to-be-connected
     * peer
     */
    lhost = ip_mymach(fhost);
    if (lhost == 0)
      return(ENETUNREACH);
    /* if the socket hasn't been bound to a local
     * port yet, do so now
     */
    lport = udpconn->u_lport;
    if (lport == 0)
      lport = IP_UDP_FindFreePort();
    /* bind and connect the UDP endpoint */
    udpconn->u_lhost = lhost;
    udpconn->u_lport = lport;
    udpconn->u_fhost = fhost;
    udpconn->u_fport = fport;
    /* mark the socket as connected */
    so->so_state &= ~(SS_ISCONNECTING|SS_ISDISCONNECTING);
    so->so_state |= SS_ISCONNECTED;
    /* since socket was in listen state, packets may be queued */
    sbflush(&so->so_rcv);   /* dump these now */
  }
  return 0;
}

int udp4_socksend(struct socket *so, struct mbuf *m, struct mbuf *nam ) {
  int e;
  struct sockaddr_in * sin;
  UDPCONN udpconn;
  U16  fport;   /* foreign port (local byte order) */
  ip_addr fhost;
  IP_PACKET * pkt;
#ifdef MULTI_HOMED
  NET ifp;
#endif
  udpconn = udp_lookup(so);
  if (!udpconn) {
    m_free(m);
    /* may be bogus socket, but more likely the connection may
       have closed due to ICMP dest unreachable from other side. */
    return(ECONNREFUSED);
  }

  if (nam == NULL) { /* no sendto() info passed, must be send() */
    if (so->so_state & SS_ISCONNECTED) {
      fport = udpconn->u_fport;
      fhost = udpconn->u_fhost;
    } else {
      return (EINVAL);
    }
  } else if(nam->m_len != sizeof (*sin)) {
    IP_WARN((IP_MTYPE_UDP, "UDP: MBuf error"));
    return (EINVAL);
  } else {
    sin = MTOD(nam, struct sockaddr_in *);
    fhost = sin->sin_addr.s_addr;
    /* use caller's fport if specified, ours may be a wildcard */
    if (sin->sin_port)   /* caller gets to change fport on the fly */
      fport = ntohs(sin->sin_port);
    else  /* use port already set in UDP connection */
    {
      if (udpconn->u_fport == 0) /* don't send to port 0 */
        return (EINVAL);
      fport = udpconn->u_fport;
    }
  }
  /* fhost and fport are now set */

  /* since our pkt->pBuffer size is tied to max packet size, we
   * assume our UDP datagrams are always in one mbuf and that the
   * mbuf
   */
  if (m->m_len > (unsigned)udp_maxalloc()) /* but check anyway:*/
  {
    IP_WARN((IP_MTYPE_UDP, "UDP: MBuf error")); /* should never happen */
    return EMSGSIZE;  /* try to recover */
  }
  pkt = IP_UDP_Alloc(m->m_len);
  if (!pkt) {
    m_free(m);
    return ENOBUFS;   /* report buffer shortages */
  }
  IP_MEMCPY(pkt->pData, m->m_data, m->m_len);
  /* finished with mbuf, free it now */
  m_free(m);

  /* if we're being asked to send to 255.255.255.255 (a local-net
     * broadcast), figure out which interface to send the broadcast
     * on, based on the IP address that the socket is bound to: if
     * it has been bound to an interface address, we should send the
     * broadcast on that interface; else, we look for the first
     * interface that can support broadcasts and is up; if we still
     * don't have an interface we look for the first interface that
     * is up; if (after all that) we don't have an interface then we
     * fail with error EADDRNOTAVAIL; and finally, if we're built
     * for a single-homed configuration where there's only one
     * interface, we might as well use it, so we do.
   */
  if (fhost == 0xffffffff)
  {
#ifdef MULTI_HOMED
    if (udpconn->u_lhost != 0L) {
      for (ifp = (NET)(netlist.q_head); ifp; ifp = ifp->n_next) {
        if (ifp->n_ipaddr == udpconn->u_lhost) {
          break;
        }
      }
    } else {
      for (ifp = (NET)(netlist.q_head); ifp; ifp = ifp->n_next)
        if ((ifp->n_flags & NF_BCAST) &&
          (ifp->n_mib) && (ifp->n_mib->ifAdminStatus == NI_UP))
           break;
    }
    if (ifp == NULL) {
      for (ifp = (NET)(netlist.q_head); ifp; ifp = ifp->n_next)
        if ((ifp->n_mib) && (ifp->n_mib->ifAdminStatus == NI_UP))
          break;
      if (ifp == NULL)
        return(EADDRNOTAVAIL);
    }
    pkt->net = ifp;
#else  /* single-homed */
    pkt->pNet = &IP_aIFace[0];
#endif /* MULTI_HOMED */
  }

#ifdef IP_MULTICAST
  /* If the socket has an IP moptions structure for multicast options,
   * place a pointer to this structure in the IP_PACKET * structure.
   */
  if (so->inp_moptions) {
    pkt->imo = so->inp_moptions;
  }

#endif   /* IP_MULTICAST */

  e = IP_UDP_Send(0, fhost, fport, udpconn->u_lport, pkt);
  if (e < 0) {
    return e;
  }
  return 0;
}

int udp4_sockaddr(struct socket *so, struct mbuf *nam , int req) {
  struct sockaddr_in * sin;
  UDPCONN udpconn;

  sin = MTOD(nam, struct sockaddr_in *);
  if (sin == NULL)
     return(EINVAL);
  udpconn = udp_lookup(so);
  if (!udpconn) {
    return(EINVAL);
  }
  nam->m_len = sizeof(*sin);
  if (req == PRU_SOCKADDR) {
     sin->sin_family = AF_INET;
     sin->sin_port = htons(udpconn->u_lport);
     sin->sin_addr.s_addr = udpconn->u_lhost;
  } else { /* PRU_PEERADDR */
    sin->sin_family = AF_INET;
    sin->sin_port = htons(udpconn->u_fport);
    sin->sin_addr.s_addr = udpconn->u_fhost;
  }
  return 0;
}

#endif      /* IP_V4 */

#endif   /* UDP_SOCKETS */
#endif /* INCLUDE_TCP */


/*************************** End of file ****************************/

