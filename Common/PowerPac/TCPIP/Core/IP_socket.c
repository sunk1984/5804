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
File    : IP_socket.c
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
#include "IP_mbuf.h"

U16 IP_SOCKET_DefaultOptions = SO_KEEPALIVE; // = SO_TIMESTAMP;  // Default so_options value for new sockets in the system. Can be set using IP_SOCKET_SetDefaultOptions()

unsigned IP_SOCKET_Limit;
unsigned IP_SOCKET_Max;
unsigned IP_SOCKET_Cnt;

QUEUE IP_FreeSockOptQ;

/*********************************************************************
*
*       Defines
*
**********************************************************************
*/
// bitmask for connection state bits which determine if send/recv is OK
#define  SO_IO_OK (SS_ISCONNECTED|SS_ISCONNECTING|SS_ISDISCONNECTING)

/*********************************************************************
*
*       Static code
*
**********************************************************************
*/

/*********************************************************************
*
*       _sblock()
*
*  Function description
*    Set lock on sockbuf sb
*/
static void _sblock(struct   sockbuf * pSockBuf) {
  while (pSockBuf->sb_flags & SB_LOCK) {
    IP_OS_WaitItem ((char *)&pSockBuf->sb_flags);
  }
  pSockBuf->sb_flags |= SB_LOCK;
}

/*********************************************************************
*
*       _sbunlock()
*
*  Function description
*    Release lock on sockbuf sb
*/
static void _sbunlock(struct   sockbuf * pSockBuf) {
  pSockBuf->sb_flags &= ~SB_LOCK;
  IP_OS_SignalItem((char *)&pSockBuf->sb_flags);
}

/*********************************************************************
*
*       _TryAlloc
*
*  Function description
*    Allocate storage space for a UDB connnect block.
*    It is either fetched from the free queue or allocated.
*/
static struct ip_socopts * _TryAlloc(void) {
  struct ip_socopts * p;

  p = (struct ip_socopts *)IP_TryAllocWithQ(&IP_FreeSockOptQ, sizeof(struct ip_socopts));
  return p;
}

/*********************************************************************
*
*       _sockargs ()
*
*  Function description
*    Allocates MBUF and copies data into it.
*/
static struct mbuf * _sockargs (void * pData, int NumBytes) {
  struct mbuf *  m;

  m = MBUF_GET_WITH_DATA (MT_SONAME, NumBytes);
  if (m) {
    m->m_len = NumBytes;
    IP_MEMCPY(MTOD (m, char *), pData, NumBytes);
  }
  return m;
}
/*********************************************************************
*
*       _sorflush()
*/
static void _sorflush(struct socket * so) {
   struct sockbuf *  sb =  &so->so_rcv;
   int   s;

   _sblock(sb);
   socantrcvmore (so);
   _sbunlock (sb);
   sbrelease (sb);
   IP_MEMSET ((char *)sb, 0, sizeof (*sb));
   s = so->so_error;
   so->so_error = ESHUTDOWN;
   sorwakeup (so);
   so->so_error = s;
}

/*********************************************************************
*
*       IP_FreeSockOpt
*
*  Function description
*    Frees storage space by adding it to the free queue.
*/
void IP_FreeSockOpt(struct ip_socopts * p) {
  IP_Q_Add(&IP_FreeSockOptQ, p);
}

/*********************************************************************
*
*       sofree
*/
void sofree(struct socket * so) {
  IP_LOG((IP_MTYPE_SOCKET_STATE, "SOCKET: sofree(so = %x)", so));

   if (so->so_pcb || (so->so_state & SS_NOFDREF) == 0) {
     return;
   }
   if (so->so_head) {
     if (!soqremque(so, 0) && !soqremque(so, 1)) {
       IP_PANIC("sofree");
     }
     so->so_head = 0;
   }
   sbrelease(&so->so_snd);
   _sorflush(so);
#ifdef SAVE_SOCK_ENDPOINTS
   if (so->so_endpoint) {
     _socket_free_entry (so);
   }
#endif   /* SAVE_SOCK_ENDPOINTS */

   /* IP_TOS opts? */
   if (so->so_optsPack) {
     IP_FreeSockOpt(so->so_optsPack);
   }
	
   IP_Q_RemoveItem(&IP_Global.SocketInUseQ, so);   /* Delete the socket entry from the queue */
   IP_SOCKET_Free(so);
}

/*********************************************************************
*
*       _socreate()
*
*  Function description
*    Create socket of given Type & Proto
*/
static struct socket * _socreate(int dom, int   type, int   proto) {
  struct protosw *  prp;
  struct socket *   so;
  int   error;

  if (proto) {
    prp = pffindproto(dom, proto, type);
  } else {
    prp = pffindtype(dom, type);
  }
  if (prp == 0) {
    return NULL;
  }
  if (prp->pr_type != type) {
    return NULL;
  }

  so = IP_SOCKET_Alloc();
  if (so == NULL) {
    return NULL;
  }
  so->next = NULL;
  IP_Q_Add(&IP_Global.SocketInUseQ,(QUEUE_ITEM*)so);

  so->so_options = IP_SOCKET_DefaultOptions;
  so->so_domain = dom;
  so->so_state = 0;
  so->so_type = (char)type;
  so->so_proto = prp;

  so->so_req = PRU_ATTACH;
  error = (*prp->pr_usrreq)(so,(struct mbuf *)0, LONG2MBUF((long)proto));
  if (error) {
    so->so_state |= SS_NOFDREF;
    sofree (so);
    return NULL;
  }

  return so;
}

/*********************************************************************
*
*       _socket()
*/
static struct socket * _socket(int family, int   type, int   proto) {
  struct socket *   so;

  IP_LOG((IP_MTYPE_SOCKET, "SOCKET: family %d, typ %d, proto %d",  family, type, proto));
  so = _socreate (family, type, proto);
  if (so) {
    so->so_error = 0;
  }
  return so;
}

/*********************************************************************
*
*       t_socket()
*/
long t_socket(int family, int   type, int   proto) {
  struct socket * pSock;
  long r;

  LOCK_NET();
  pSock = _socket(family, type, proto);
  r = IP_SOCKET_p2h(pSock);
  UNLOCK_NET();
  return r;
}

/*********************************************************************
*
*       sobind()
*
* RETURNS: 0 if OK, else one of the socket error codes
*/
static int sobind(struct socket * so, struct mbuf *  nam) {
  int   error;

  so->so_req = PRU_BIND;
  error = (*so->so_proto->pr_usrreq)(so, (struct mbuf *)0, nam);
  return (error);
}

/*********************************************************************
*
*        _bind()
*/
static int _bind (struct socket *   so, struct sockaddr * pSockAddr, int addrlen) {
  struct mbuf *     nam;
  struct sockaddr   sa;
  int               err;

  so->so_error = 0;
  if ((void*)pSockAddr == NULL) {
    IP_MEMSET ((void *)&sa, 0, sizeof(sa));
    addrlen = sizeof(sa);
    sa.sa_family = so->so_domain;
    pSockAddr = &sa;
  }

  nam = _sockargs (pSockAddr, addrlen);
  if (nam == NULL) {
    so->so_error = ENOMEM;
    return SOCKET_ERROR;
  }
  err = sobind (so, nam);
  m_freem(nam);
  if (err) {
    so->so_error = err;
    return SOCKET_ERROR;
  }
  return 0;
}

/*********************************************************************
*
*       t_bind()
*
* RETURNS: 0 if OK, else one of the socket error codes
*/
int t_bind (long hSock, struct sockaddr * pSockAddr, int addrlen) {
  struct socket *   so;
  int r;

  LOCK_NET();
  so = IP_SOCKET_h2p(hSock);
  if (so) {
    r = _bind (so, pSockAddr, addrlen);
  } else {
    r = ENOTSOCK;     // Socket has not been opened or has already been closed
  }
  UNLOCK_NET();
  return r;
}

/*********************************************************************
*
*       solisten
*
* PARAM1: struct socket *so
* PARAM2: int backlog
*
* RETURNS: 0 if OK, else one of the socket error codes
*/
static int solisten(struct socket * so, int   backlog) {
  int   error;

  so->so_req = PRU_LISTEN;
  error = (*so->so_proto->pr_usrreq)(so, (struct mbuf *)0, (struct mbuf *)0);
  if (error) {
    return (error);
  }
  if (so->so_q == 0) {
    so->so_q = so;
    so->so_q0 = so;
    so->so_options |= SO_ACCEPTCONN;
  }
  if (backlog < 0) {
    backlog = 0;
  }
  so->so_qlimit = (char)MIN(backlog, SOMAXCONN);
  return 0;
}

/*********************************************************************
*
*        _listen()
*/
static int _listen(struct socket *   so, int   backlog) {
  int   err;

  so->so_error = 0;
  IP_LOG((IP_MTYPE_SOCKET, "SOCKET: listen:qlen %d", backlog));

  err = solisten (so, backlog);
  if (err != 0) {
    so->so_error = err;
    return SOCKET_ERROR;
  }
  return 0;
}

/*********************************************************************
*
*       t_listen()
*/
int t_listen(long hSock, int   backlog) {
  struct socket *   so;
  int r;

  LOCK_NET();
  so = IP_SOCKET_h2p(hSock);
  if (so) {
    r = _listen (so, backlog);
  } else {
    r = ENOTSOCK;     // Socket has not been opened or has already been closed
  }
  UNLOCK_NET();
  return r;
}

/*********************************************************************
*
*       _sodisconnect()
*/
static int _sodisconnect(struct socket * so) {
  int   error;

  ASSERT_LOCK();
  if ((so->so_state & SS_ISCONNECTED) == 0) {
    error = ENOTCONN;
    goto bad;
  }
  if (so->so_state & SS_ISDISCONNECTING) {
    error = EALREADY;
    goto bad;
  }
  so->so_req = PRU_DISCONNECT;
  error = (*so->so_proto->pr_usrreq)(so, (struct mbuf *)0, (struct mbuf *)0);

bad:
  return (error);
}

/*********************************************************************
*
*       soabort()
*
* Must be called at splnet...
*
*/
int soabort(struct socket * so) {
  ASSERT_LOCK();
  so->so_req = PRU_ABORT;
  return(*so->so_proto->pr_usrreq)(so, (struct mbuf *)0, (struct mbuf *)0);
}

/*********************************************************************
*
*       _soclose()
*
*  Function description
*    Close a socket on last file table reference removal.
*    Initiate disconnect if connected.
*    Free socket when disconnect complete.
*/
static int _soclose(struct socket * so) {
   int   error =  0;
   struct socket *   tmpso;
   unsigned long endtime;

   //
   // Check whether the closing socket is in the socket queue.  If it is
   // not, return a EINVAL error code to the caller.
   //
   for ((tmpso=(struct socket *)IP_Global.SocketInUseQ.q_head); ; tmpso=tmpso->next) {
     if (tmpso == NULL) {
       return EINVAL;
     }
     if (so == tmpso) {
       break;
     }
   }
   IP_LOG((IP_MTYPE_SOCKET_STATE, "SOCKET: close(so = %x)",  so));
   if (so->so_options & SO_ACCEPTCONN) {
     while (so->so_q0 != so) {
       soabort(so->so_q0);
     }
     while (so->so_q != so) {
       soabort(so->so_q);
     }
   }
   /* for datagram-oriented sockets, dispense with further tests */
   if (so->so_type != SOCK_STREAM) {
      so->so_req = PRU_DETACH;
      error = (*so->so_proto->pr_usrreq)(so, (struct mbuf *)0, (struct mbuf *)0);
      goto discard;
   }

   if (so->so_pcb == 0) {
      goto discard;
   }
   if (so->so_state & SS_ISCONNECTED) {
     if ((so->so_state & SS_ISDISCONNECTING) == 0) {
       error = _sodisconnect(so);
       if (error) {
         goto drop;
       }
     }
     if (so->so_options & SO_LINGER) {
       if ((so->so_state & SS_ISDISCONNECTING) && (so->so_state & SS_NBIO)) {
         goto drop;
       }
       endtime = IP_OS_GetTime32() + (unsigned long)so->so_linger * TPS;
       while (so->so_state & SS_ISCONNECTED) {
         I32 t;
         t = endtime - IP_OS_GetTime32();    // Compute remaining time
         if (t <= 0) {
           break;        // Timeout expired
         }
         IP_OS_WaitItem(&so->so_timeo);
       }
     } else { /* Linger option not set */
       /* If socket still has send data just return now, leaving the
       * socket intact so the data can be sent. Socket should be cleaned
       * up later by timers.
       */
       if(so->so_snd.NumBytes) {
         so->so_state |= SS_NOFDREF;   /* mark as OK to close */
         return 0;
       }
     }
   }
drop:
  if (so->so_pcb) {
    int   error2;
    so->so_req = PRU_DETACH;
    error2 = (*so->so_proto->pr_usrreq)(so, (struct mbuf *)0, (struct mbuf *)0);
    if (error == 0) {
       error = error2;
    }
  }
discard:
  if (so->so_state & SS_NOFDREF) {
    IP_WARN((IP_MTYPE_SOCKET, "SOCKET: _soclose"));
  }
  so->so_state |= SS_NOFDREF;
  sofree(so);
  return (error);
}

/*********************************************************************
*
*       soaccept()
*/
static int soaccept(struct socket * so, struct sockaddr_in *  pSockAddr) {
  int   error;

  if ((so->so_state & SS_NOFDREF) == 0) {
    IP_PANIC("soaccept");
  }
  so->so_state &= ~SS_NOFDREF;
  so->so_req = PRU_ACCEPT;
  error = (*so->so_proto->pr_usrreq)(so, (struct mbuf *)0, (struct mbuf *)pSockAddr);

  return error;
}

/*********************************************************************
*
*        _accept()
*/
static long _accept(long hSock, struct sockaddr * addr, int * addrlen) {
  struct socket *  so;
  struct socket * aso;     // Child socket
  struct sockaddr_in SockAddr;

  so = IP_SOCKET_h2p(hSock);
  if (so == NULL) {
    return SOCKET_ERROR;     // Socket has not been opened or has already been closed
  }
  so->so_error = 0;

  //
  // Check various error conditions
  //
  if ((so->so_options & SO_ACCEPTCONN) == 0) {
    so->so_error = EINVAL;
    return SOCKET_ERROR;
  }
  if ((so->so_state & SS_NBIO) && so->so_qlen == 0) {
    so->so_error = EWOULDBLOCK;
    return SOCKET_ERROR;
  }
  //
  // Wait for a state change: Either connection or error
  //
  while (so->so_qlen == 0 && so->so_error == 0) {
    if (so->so_state & SS_CANTRCVMORE) {
      so->so_error = ECONNABORTED;
      return SOCKET_ERROR;
    }
    IP_OS_WaitItem ((char *)&so->so_timeo);
  }
  //
  // Handle state change. Check error conditions first.
  //
  if (so->so_error) {
    IP_LOG((IP_MTYPE_SOCKET_STATE, "SOCKET: accept(s = %x): Failed", hSock));
    return SOCKET_ERROR;
  }

  aso = so->so_q;
  if (soqremque (aso, 1) == 0) {
    IP_PANIC("accept");
  }
  soaccept (aso, &SockAddr);

  //
  // Log output in debug builds
  //
  IP_LOG((IP_MTYPE_SOCKET_STATE, "SOCKET: accept(s = %x, ...): Connect: %i:%d on socket %x",
                                 hSock,
                                 SockAddr.sin_addr.s_addr,
                                 ntohs(SockAddr.sin_port),
                                 aso));
  //
  // Return the addressing info in the passed structure
  //
  if (addr != NULL) {
    IP_MEMCPY(addr, &SockAddr, *addrlen);
  }

  return IP_SOCKET_p2h(aso);
}

/*********************************************************************
*
*       t_accept()
*/
long t_accept(long hSock, struct sockaddr * addr, int * addrlen) {
  int r;

  IP_LOG((IP_MTYPE_SOCKET_STATE, "SOCKET: accept(s = %x, ...)", hSock));
  LOCK_NET();
  r = _accept (hSock, addr, addrlen);
  UNLOCK_NET();
  return r;
}

/*********************************************************************
*
*       soconnect()
*/
static int soconnect(struct socket * so, struct mbuf *  nam) {
  int   error;

  if (so->so_options & SO_ACCEPTCONN) {
    return EOPNOTSUPP;
  }
  /*
  * If protocol is connection-based, can only connect once.
  * Otherwise, if connected, try to disconnect first.
  * This allows user to disconnect by connecting to, e.g.,
  * a null address.
  */
  if (so->so_state & (SS_ISCONNECTED | SS_ISCONNECTING)) {
    if (so->so_proto->pr_flags & PR_CONNREQUIRED) {
      return EISCONN;     // TCP. Return error rather than automatic disconnect
    }
    IP_LOG((IP_MTYPE_SOCKET_STATE, "SOCKET: disconnecting because of new connect(so = %x)", so));
    if (_sodisconnect(so) != 0) {
      return EISCONN;
    }
  }
  //
  // Not connected. Send connect request to protocol
  //
  so->so_req = PRU_CONNECT;
  error = (*so->so_proto->pr_usrreq)(so, (struct mbuf *)0, nam);
  return error;
}

/*********************************************************************
*
*       _connect()
*/
static int _connect(long hSock, struct sockaddr * addr, int   addrlen) {
  struct socket *   so;
  struct mbuf *  nam;

  so = IP_SOCKET_h2p(hSock);
  if (so == NULL) {
    return SOCKET_ERROR;     // Socket has not been opened or has already been closed
  }

  /* need to test non blocking connect bits in case this is a
    poll of a previous request */
  if (so->so_state & SS_NBIO) {
    if (so->so_state & SS_ISCONNECTING) { // Still trying
      so->so_error = EINPROGRESS;
      return SOCKET_ERROR;
    }
    if (so->so_state & SS_ISCONNECTED) {  // connected OK
      so->so_error = 0;
      return 0;
    }
    if (so->so_state & SS_WASCONNECTING) {
      so->so_state &= ~SS_WASCONNECTING;
      if (so->so_error) {         // connect error - maybe timeout
        return SOCKET_ERROR;
      }
    }
  }
  so->so_error = 0;
  nam = _sockargs (addr, addrlen);
  if (nam == NULL) {
    so->so_error = ENOMEM;
    return SOCKET_ERROR;
  }

#ifdef TRACE_DEBUG
   {
     struct sockaddr_in *sin = (struct sockaddr_in *)uap->sap;
     IP_LOG((IP_MTYPE_SOCKET_STATE, "SOCKET:  connect, port %d addr %lx",  sin->sin_port, sin->sin_addr.s_addr));
   }
#endif   /* TRACE_DEBUG */

  if ((so->so_error = soconnect (so, nam)) != 0) {
    goto bad;
  }

  /* need to test non blocking connect bits after soconnect() call */
  if ((so->so_state & SS_NBIO)&& (so->so_state & SS_ISCONNECTING)) {
    so->so_error = EINPROGRESS;
    goto bad;
  }
  IP_LOG((IP_MTYPE_SOCKET_STATE, "SOCKET: connect(), so %x so_state %x so_error %d", so, so->so_state, so->so_error));
  while ((so->so_state & SS_ISCONNECTING) && so->so_error == 0) {
    IP_OS_WaitItem ((char *)&so->so_timeo);
  }
bad:
  if (so->so_error != EINPROGRESS) {
    so->so_state &= ~(SS_ISCONNECTING|SS_WASCONNECTING);
  }
  m_freem (nam);

  if (so->so_error) {
    return SOCKET_ERROR;
  }
  return 0;
}

/*********************************************************************
*
*       t_connect()
*/
int t_connect(long hSock, struct sockaddr * addr, int   addrlen) {
  int r;

  LOCK_NET();
  r = _connect(hSock, addr, addrlen);
  UNLOCK_NET();
  return r;
}

/*********************************************************************
*
*       _CalcHeaderSize()
*
*  Function description
*    Compute Header size required for the given socket
*/
static int _CalcHeaderSize(struct socket * so) {
  int v;
  struct inpcb * inp;
  struct tcpcb * tp;

  //
  // If we have a valid TCP socket, it contains a MaxSeg value which we can use.
  //
  inp = (struct inpcb *)so->so_pcb;         /* Map socket to IP cb */
  tp = (struct tcpcb *)inp->inp_ppcb;       /* Map IP to TCP cb */
  v = 40 + tp->OptLen + inp->ifp->n_lnh;    // 40 bytes for IP and TCP header, plus connection dependent Option len + typ. 14+2 bytes for Ethernet
  return v;
}



/*********************************************************************
*
*       _sosend
*
*  Function description
*    Send on a socket.
*    If send must go all at once and message is larger than
*    send buffering, then hard error.
*    Lock against other senders.
*    If must go all at once and not enough room now, then
*    inform user that this would block and do nothing.
*    Otherwise, if nonblocking, send as much as possible.
*
*  RETURNS: 0 if OK or BSD socket error code
*/
static int _sosend(struct socket * so, struct mbuf *  nam,     /* sockaddr, if UDP socket, NULL if TCP */
   const char  * data,           /* data to send */
   int   * data_length,    /* IN/OUT  length of (remaining) data */
   int   flags)
{
   struct mbuf *  head  =  NULL;
   struct mbuf *  m;
   int   space;
   int   NumBytesRemaining;
   int   len;
   int   error =  0;
   int   dontroute;
   int   first =  1;

   NumBytesRemaining = *data_length;


   IP_LOG((IP_MTYPE_SOCKET_WRITE, "SOCKET: _sosend: so %lx NumBytesRemaining %d Limit %d so_state %x", so, NumBytesRemaining, so->so_snd.Limit, so->so_state));
   /*
    * In theory NumBytesRemaining should be unsigned.
    * However, space must be signed, as it might be less than 0
    * if we over-committed, and we must use a signed comparison
    * of space and NumBytesRemaining.  On the other hand, a negative NumBytesRemaining
    * causes us to loop sending 0-length segments to the protocol.
    */
   if (NumBytesRemaining < 0) {
     IP_WARN((IP_MTYPE_SOCKET_WRITE, "SOCKET: Negative number of byted to send."));
     return EINVAL;
   }
   if ((so->so_proto->pr_flags & PR_ATOMIC) && (NumBytesRemaining > (int)so->so_snd.Limit)) {
     IP_WARN((IP_MTYPE_SOCKET_WRITE, "SOCKET: Can not send packet at once or store in buffer."));
     return EMSGSIZE;
   }
   dontroute = (flags & MSG_DONTROUTE) && (so->so_options & SO_DONTROUTE) == 0 && (so->so_proto->pr_flags & PR_ATOMIC);
restart:
   _sblock(&so->so_snd);
   do {
      if (so->so_error) {
         error = so->so_error;
         so->so_error = 0;          /* ??? */
         goto release;
      }
      if (so->so_state & SS_CANTSENDMORE) {
        error = EPIPE;
        goto release;
      }
      if ((so->so_state & SS_ISCONNECTED) == 0) {
        if (so->so_proto->pr_flags & PR_CONNREQUIRED) {
          error = ENOTCONN;
          goto release;
        }
        if (nam == 0) {
          error = EDESTADDRREQ;
          goto release;
        }
      }
      //
      // Check if we must block
      //
      {
        int MustBlock = 0;
        space = (int)sbspace(&so->so_snd);
        if ((so->so_proto->pr_flags & PR_ATOMIC) && (space < NumBytesRemaining)) {
          MustBlock = 1;
        } else {
          if (NumBytesRemaining >= CLBYTES) {
            if (space < CLBYTES) {
              if (so->so_snd.NumBytes >= CLBYTES) {
                if ((so->so_state & SS_NBIO) == 0) {
                  MustBlock = 1;
                }
              }
            }
          }
        }
        //
        // Handle blocking situation:
        // For non-blocking send, return error
        // For blocking sockets, wait
        //
        if (MustBlock) {
          if (so->so_state & SS_NBIO) {
            if (first) {
              error = EWOULDBLOCK;
            }
            goto release;
          }
          _sbunlock(&so->so_snd);
          sbwait(&so->so_snd, so->so_snd.Timeout);
          goto restart;
        }
      }
      if ( space <= 0 ) {
        /* no space in socket send buffer - see if we can wait */
        if (so->so_state & SS_NBIO) {   /* check non-blocking IO bit */
          if (first) {     /* report first error */
            error = EWOULDBLOCK;
          }
          goto release;
        }
        /* If blocking socket, let someone else run */
        sbwait(&so->so_snd, so->so_snd.Timeout);
      }
      while (space > 0) {
        len = NumBytesRemaining;
         if ( so->so_type == SOCK_STREAM ) {
           int HeaderSize;
           HeaderSize = _CalcHeaderSize(so);
           m = MBUF_GET_WITH_DATA(MT_TXDATA, len + HeaderSize);
           if (!m) {
             error = ENOBUFS;
             goto release;
           }
           m->m_data += HeaderSize;      // Reserve enough so we can send it in a packet without having to copy
           IP_MEMCPY(m->m_data, data, len);
         } else {
           m = MBUF_GET(MT_TXDATA);
           if (!m) {
             error = ENOBUFS;
             goto release;
           }
           m->m_data = (char *)data;
         }
         NumBytesRemaining -= len;
         data += len;
         m->m_len = len;
         if (head == NULL) {
           head = m;
         }
         if (error) {
           goto release;
         }
         if (NumBytesRemaining <= 0) {
           break;
         }
      }
      if (dontroute) {
        so->so_options |= SO_DONTROUTE;
      }
      so->so_req = PRU_SEND;
      error = (*so->so_proto->pr_usrreq)(so, head, nam);         // Send data

      if (dontroute) {
        so->so_options &= ~SO_DONTROUTE;
      }

      head = 0;
      first = 0;
      if (error) {
        break;
      }
  } while (NumBytesRemaining);

release:
  _sbunlock(&so->so_snd);
  if (head) {
     m_freem (head);
  }
  *data_length = NumBytesRemaining;
  return error;
}



/*********************************************************************
*
*       soreceive()
*
* Implement receive operations on a socket.
* We depend on the way that records are added to the sockbuf
* by sbappend*.  In particular, each record (mbufs linked through m_next)
* must begin with an address if the protocol so specifies,
* followed by an optional mbuf containing access rights if supported
* by the protocol, and then zero or more mbufs of data.
* In order to avoid blocking network interrupts for the entire time here,
* we splx() while doing the actual copy to user space.
* Although the sockbuf is locked, new data may still be appended,
* and thus we must maintain consistency of the sockbuf during that time.
*
*  Parameters
*    pNumBytes   I/O      points to the number of bytes remaining to be read. Note: in and out parameter!
*/
int soreceive(struct socket * so, struct mbuf **aname, char * data, int * pNumBytes, int   flags) {
  struct mbuf *  m;
  int   len;
  int   error =  0;
  struct protosw *  pr =  so->so_proto;
  struct mbuf *  nextrecord;
  int   moff;
  I32 t;
  I32   t0;
  int   NumBytes;

  if (aname) {
    *aname = 0;
  }
  NumBytes = *pNumBytes;
  //
  // Wait while (!Timedout && NoData && NoError).
  //
  t0 = IP_OS_GetTime32();
  IP_LOG((IP_MTYPE_SOCKET_READ, "SOCKET: soreceive(), sbcc %d soerror %d so_state %d NumBytes %d", so->so_rcv.NumBytes, so->so_error, so->so_state, NumBytes));

  do {
    _sblock (&so->so_rcv);
    // If no data is ready, see if we should wait or return
    if (so->so_rcv.NumBytes) {
      break;
    }
    if (so->so_error) {
      error = so->so_error;
      so->so_error = 0;
      goto release;
    }
    if (so->so_state & SS_CANTRCVMORE) {
      goto release;
    }
    if ((so->so_state & SS_ISCONNECTED) == 0 && (so->so_proto->pr_flags & PR_CONNREQUIRED)) {
      error = ENOTCONN;
      goto release;
    }
    if (NumBytes == 0) {
      goto release;
    }
    if ((so->so_state & SS_NBIO) || (flags & MSG_DONTWAIT)) {
      error = EWOULDBLOCK;
      goto release;
    }
    //
    // For blocking call with timeout, check if the timeout has expired.
    // If it has, then do NOT wait
    //
    t = 0;
    if (so->so_rcv.Timeout) {
      t = t0 + so->so_rcv.Timeout;
      t -= IP_OS_GetTime32();         // Compute how much time is left
      if (t <= 0) {
        error = ETIMEDOUT;
        goto release;
      }
    }
    _sbunlock(&so->so_rcv);
    sbwait(&so->so_rcv, t);
  } while (1);
  //
  // There is data in the receive buffer. Copy it to the application and return.
  //
  m = so->so_rcv.sb_mb;
  if (m == 0) {
    IP_PANIC("sorecv 1");
  }
  nextrecord = m->m_act;
  if (pr->pr_flags & PR_ADDR) {
#if IP_DEBUG
    if (m->m_type != MT_SONAME) {
       IP_PANIC("sorecv 2");
    }
#endif
    if (flags & MSG_PEEK) {
      if (aname) {
          *aname = m_copy(m, 0, m->m_len, 0);
      }
      m = m->m_next;
    } else {
       sbfree (&so->so_rcv, m);
       if (aname) {
          *aname = m;
          m = m->m_next;
          (*aname)->m_next = 0;
          so->so_rcv.sb_mb = m;
       } else {
          MFREE(m, so->so_rcv.sb_mb);
          m = so->so_rcv.sb_mb;
       }
       if (m) {
         m->m_act = nextrecord;
       }
    }
  }
  moff = 0;
  while (m && (NumBytes > 0) && (error == 0)) {
#if IP_DEBUG
    if (m->m_type != MT_RXDATA && m->m_type != MT_HEADER) {
      IP_PANIC("sorecv 3");
    }
#endif
    len = NumBytes;
    so->so_state &= ~SS_RCVATMARK;
    if (len > (int)(m->m_len - moff)) {
       len = m->m_len - moff;
    }

    IP_LOG((IP_MTYPE_SOCKET_READ, "SOCKET:  soreceive(), so %lx %d bytes, flags %x",  so, len, flags));

    /*
     * Copy mbufs info user buffer, then free them.
     * Sockbuf must be consistent here (points to current mbuf,
     * it points to next record) when we drop priority;
     * we must note any additions to the sockbuf when we
     * block interrupts again.
     */

    IP_MEMCPY(data, (MTOD(m, char *) + moff), len);
    data += len;
    NumBytes -= len;

    if (len == (int)(m->m_len - moff)) {
       if (flags & MSG_PEEK) {
          m = m->m_next;
          moff = 0;
       } else {
          nextrecord = m->m_act;
          sbfree(&so->so_rcv, m);
          MFREE(m, so->so_rcv.sb_mb);
          m = so->so_rcv.sb_mb;
          if (m) {
             m->m_act = nextrecord;
          }
       }
    } else {
      if (flags & MSG_PEEK) {
        moff += len;
      } else {
        m->m_data += len;
        m->m_len -= len;
        so->so_rcv.NumBytes -= len;
      }
    }
  }

  if ((flags & MSG_PEEK) == 0) {
    if (m == 0) {
       so->so_rcv.sb_mb = nextrecord;
    } else if (pr->pr_flags & PR_ATOMIC) {
      sbdroprecord(&so->so_rcv);
    }
    if (pr->pr_flags & PR_WANTRCVD && so->so_pcb) {
      so->so_req = PRU_RCVD;
      (*pr->pr_usrreq)(so, (struct mbuf *)0, (struct mbuf *)0);
    }
  }
release:
  _sbunlock(&so->so_rcv);
  *pNumBytes = NumBytes;
  return error;
}

/*********************************************************************
*
*       soshutdown()
*
*  Function description
*    soshutdown(socket, how) how values: 0 - shutdown read operation 1
*    - shutdown write operation 2 - shutdown both read and write
*    operation
*/
static int soshutdown(struct socket * so, int how) {
  how++;   /* convert 0,1,2 into 1,2,3 */
  if (how & 1) {  /* caller wanted READ or BOTH */
    _sorflush(so);
  }

  if (how & 2) {   /* caller wanted WRITE or BOTH */
    so->so_req = PRU_SHUTDOWN;
    return (*so->so_proto->pr_usrreq) (so, (struct mbuf *)0, (struct mbuf *)0);
  }
  return 0;
}

/*********************************************************************
*
*       sosetopt()
*/
static int sosetopt(struct socket * so, int   optname, void *   arg) {
  int   error =  0;
  int v;

  switch (optname) {
  case SO_LINGER:
    so->so_linger = (short) ((struct linger *)arg)->l_linger;
    arg = &((struct linger *)arg)->l_onoff;
    // Fall through
  case SO_KEEPALIVE:
  case SO_DONTROUTE:
  case SO_BROADCAST:
  case SO_REUSEADDR:
  case SO_OOBINLINE:
  case SO_TCPSACK:
  case SO_NOSLOWSTART:
#ifdef SUPPORT_SO_FULLMSS
  case SO_FULLMSS:
#endif
    if (*(int *)arg) {
      so->so_options |= optname;
    } else {
      so->so_options &= ~optname;
    }
    break;

#ifdef TCP_BIGCWND
   case SO_BIGCWND:
      /* Set Large initial congestion window on this socket. This should
       * only be done on TCP sockets, and only before they are opened.
       */
      if(so->so_type != SOCK_STREAM)
         return EINVAL;
      if (*(int *)arg)
      {
         so->so_options |= optname;
         default_cwnd = TCP_MSS * (*(int *)arg);
      }
      else
         so->so_options &= ~optname;
      break;
#endif /* TCP_BIGCWND */

   case SO_SNDBUF:
   case SO_RCVBUF:
     v = * (int *)arg;
     if (optname == SO_SNDBUF) {
       sbreserve(&so->so_snd, v);
     } else {
       sbreserve(&so->so_rcv, v);
     }
     break;

   case SO_SNDTIMEO:
      so->so_snd.Timeout = *(unsigned *)arg;
      break;

   case SO_RCVTIMEO:
      so->so_rcv.Timeout = *(unsigned *)arg;
      break;

   case SO_NBIO:     /* set socket into NON-blocking mode */
      so->so_state |= SS_NBIO;
      break;

   case SO_BIO:   /* set socket into blocking mode */
      so->so_state &= ~SS_NBIO;
      break;

   case SO_NONBLOCK:    /* set blocking mode according to arg */
      /* sanity check the arg parameter */
      if (!arg)
      {
         error = IP_ERR_PARAM;
         break;
      }
      /* if contents of integer addressed by arg are non-zero */
      if (*(int *) arg)
         so->so_state |= SS_NBIO;   /* set non-blocking mode */
      else
         so->so_state &= ~SS_NBIO;  /* set blocking mode */
      break;

#ifdef IP_RAW

   case IP_HDRINCL:
      /* try to make sure that the argument pointer is valid */
      if (arg == NULL) {
        error = IP_ERR_PARAM;
        break;
      }
      /* set the socket option flag based on the pointed-to argument */
      if (*(int *)arg) {
        so->so_options |= SO_HDRINCL;
      } else {
        so->so_options &= ~SO_HDRINCL;
      }
      break;

#endif   /* IP_RAW */

#ifdef DO_DELAY_ACKS
   case TCP_ACKDELAYTIME:
   case TCP_NOACKDELAY:
   {
      struct inpcb * inp;
      struct tcpcb * tp;

      if(so->so_type != SOCK_STREAM) {
         error = EINVAL;
         break;
      }
      inp = (struct inpcb *)(so->so_pcb);
      tp = inp->inp_ppcb;
      if (!tp)  {
        error = ENOTCONN;
        break;
      }
      if (optname == TCP_ACKDELAYTIME) {
        int v;
        v = *(int*)arg;
        tp->DelayAckPeriod = v;
      } else {
        tp->DelayAckPeriod = 0;
      }

      break;
   }
#endif   /* DO_DELAY_ACKS */

   case TCP_NODELAY:
   {
      struct inpcb * inp;
      struct tcpcb * tp;

      if(so->so_type != SOCK_STREAM)
      {
         error = EINVAL;
         break;
      }
      inp = (struct inpcb *)(so->so_pcb);
      tp = inp->inp_ppcb;
      if(!tp)
      {
         error = ENOTCONN;
         break;
      }
      /* try to make sure that the argument pointer is valid */
      if (arg == NULL)
      {
         error = IP_ERR_PARAM;
         break;
      }
      /* if contents of integer addressed by arg are non-zero */
      if (*(int *) arg)
         tp->t_flags |= TF_NODELAY;   /* Disable Nagle Algorithm */
      else
         tp->t_flags &= ~TF_NODELAY;  /* Enable Nagle Algorithm */

      break;
   }

#ifdef TCP_ZEROCOPY
   case SO_CALLBACK:    /* install ZERO_COPY callback routine */
      /*
       * This generates warnings on many compilers, even when it's
       * doing the right thing. Just make sure the "void * arg" data
       * pointer converts properly to a function pointer.
       */
      so->rx_upcall = (int (*)(long sock, void*, int))arg;
      break;
#endif   /* TCP_ZEROCOPY */

   case SO_MAXMSG:
   case TCP_MAXSEG:
   {
      struct inpcb * inp;
      struct tcpcb * tp;
      int v;

      if(so->so_type != SOCK_STREAM)
      {
         error = EINVAL;
         break;
      }
      inp = (struct inpcb *)(so->so_pcb);
      tp = inp->inp_ppcb;
      if(!tp)
      {
         error = ENOTCONN;
         break;
      }
      v = *(int*)(arg);
      tp->Mss     = v;                /* set TCP MSS */
      tp->TrueMSS = v - tp->OptLen;
      break;
   }
   default:
      error = ENOPROTOOPT;
      break;
   }
   return (error);
}


/*********************************************************************
*
*       sogetopt()
*/
static int sogetopt(struct socket * so, int   optname, void *   val) {
   int   error =  0;

   /* sanity check the val parameter */
   if (!val)
   {
      return IP_ERR_PARAM;
   }

   switch (optname)
   {
   case SO_MYADDR:
      /* Get my IP address. */
      if (so->so_state & SS_ISCONNECTED)
      {
         *(U32 *)val = so->so_pcb->ifp->n_ipaddr;
      }
      else  /* not connected, use first iface */
         *(U32 *)val = IP_aIFace[0].n_ipaddr;
      break;
   case SO_LINGER:
      {
         struct linger *   l  =  (struct  linger *)val;
         l->l_onoff = so->so_options & SO_LINGER;
         l->l_linger = so->so_linger;
      }
      break;

   case SO_KEEPALIVE:
   case SO_OOBINLINE:
   case SO_DONTROUTE:
   case SO_REUSEADDR:
   case SO_BROADCAST:
   case SO_TCPSACK:
      *(int *)val = so->so_options & optname;
      break;

   case SO_SNDBUF:
      *(int *)val = (int)so->so_snd.Limit;
      break;

   case SO_RCVBUF:
      *(int *)val = (int)so->so_rcv.Limit;
      break;

   case SO_RXDATA:   /* added, JB */
      *(int *)val = (int)so->so_rcv.NumBytes;
      break;

   case SO_TXDATA:   /* added for rel 1.8 */
      *(int *)val = (int)so->so_snd.NumBytes;
      break;

   case SO_TYPE:
      *(int *)val = so->so_type;
      break;

   case SO_ERROR:
      *(int *)val = so->so_error;
      so->so_error = 0;
      break;

   case SO_MAXMSG:
   case TCP_MAXSEG:
   {
      struct inpcb * inp;
      struct tcpcb * tp;

      if(so->so_type != SOCK_STREAM)
      {
         error = EINVAL;
         break;
      }
      inp = (struct inpcb *)(so->so_pcb);
      tp = inp->inp_ppcb;
      if(!tp)
      {
         error = ENOTCONN;
         break;
      }
      *(int *)val = tp->Mss;     /* Fill in TCP MSS for current socket */
      break;
   }

   case SO_SNDTIMEO:
      *(unsigned*)val = so->so_snd.Timeout;
      break;

   case SO_RCVTIMEO:
      *(unsigned*)val = so->so_rcv.Timeout;
      break;

   case SO_HOPCNT:
      *(int *)val = so->so_hopcnt;
      break;

   case SO_NONBLOCK:    /* get blocking mode according to val */
      /* if the non-blocking I/O bit is set in the state */
      if (so->so_state & SS_NBIO)
         *(int *)val = 1;   /* return 1 in val */
      else
         *(int *)val = 0;     /* return 0 in val */
      break;

#ifdef IP_RAW

   case IP_HDRINCL:
      /* indicate based on header-include flag in socket state */
      if (so->so_options & SO_HDRINCL)
         *(int *)val = 1;
      else
         *(int *)val = 0;
      break;

#endif   /* IP_RAW */

#ifdef DO_DELAY_ACKS
   case TCP_ACKDELAYTIME:
   case TCP_NOACKDELAY:
   {
      struct inpcb * inp;
      struct tcpcb * tp;

      if(so->so_type != SOCK_STREAM)
      {
         error = EINVAL;
         break;
      }
      inp = (struct inpcb *)(so->so_pcb);
      tp = inp->inp_ppcb;
      if (!tp)
      {
         error = ENOTCONN;
         break;
      }
      if (optname == TCP_ACKDELAYTIME)  {
         *(int*)(val) = tp->DelayAckPeriod;
      } else {
        if (tp->DelayAckPeriod == 0) {
          *(int *)val = 1;  /* NO ACK DELAY is Enabled */
        } else {
          *(int *)val = 0;  /* NO ACK DELAY is NOT Enabled */		
        }
      }

      break;
   }
#endif   /* DO_DELAY_ACKS */

   case TCP_NODELAY:
   {
      struct inpcb * inp;
      struct tcpcb * tp;

      if(so->so_type != SOCK_STREAM)
      {
         error = EINVAL;
         break;
      }
      inp = (struct inpcb *)(so->so_pcb);
      tp = inp->inp_ppcb;
      if (!tp)
      {
         error = ENOTCONN;
         break;
      }
      /* try to make sure that the argument pointer is valid */
      if (val == NULL)
      {
         error = IP_ERR_PARAM;
         break;
      }
      /* if contents of integer addressed by arg are non-zero */
      if (tp->t_flags & TF_NODELAY)
         *(int *)val = 1;  /* Nagle Algorithm is Enabled */
      else
         *(int *)val = 0;  /* Nagle Algorithm is NOT Enabled */

      break;
   }

   default:
      return ENOPROTOOPT;
   }
   return error;     /* no error */
}



/* FUNCTION: sohasoutofband()
 *
 * PARAM1: struct socket *so
 *
 * RETURNS:
 */

void sohasoutofband(struct socket * so) {
  so->so_error = EHAVEOOB;   /* WILL be picked up by the socket */
  sorwakeup (so);
}

/*********************************************************************
*
*       _getname()
*
* Worker function for getpeername and getsockname.
*
*/
static int _getname(long hSock, struct sockaddr * addr, int * addrlen, int opcode) {
   struct socket *   so;
   struct mbuf *  m;
   int   err;

  so = IP_SOCKET_h2p(hSock);
  if (so == NULL) {
    return SOCKET_ERROR;     // Socket has not been opened or has already been closed
  }
   so->so_error = 0;
   IP_LOG((IP_MTYPE_SOCKET, "SOCKET: get[sock|peer]name so %x", so));
   if((opcode == PRU_PEERADDR) && (so->so_state & SS_ISCONNECTED) == 0) {
      so->so_error = ENOTCONN;
      return SOCKET_ERROR;
   }
   m = MBUF_GET_WITH_DATA (MT_SONAME, sizeof (struct sockaddr));
   if (m == NULL) {
      so->so_error = ENOMEM;
      return SOCKET_ERROR;
   }
   so->so_req = opcode;
   if ((err = (*so->so_proto->pr_usrreq)(so, 0, m)) != 0)
      goto bad;

#ifdef IP_V4
   if (so->so_domain == AF_INET) {
     if (*addrlen < sizeof(struct sockaddr_in)) {
       IP_WARN((IP_MTYPE_SOCKET, "SOCKET: Programming error"));
       m_freem(m);
       return EINVAL;
     }
     IP_MEMCPY(addr, m->m_data, sizeof(struct sockaddr_in));
     *addrlen = sizeof(struct sockaddr_in);
   }
#endif   /* IP_V4 */

bad:
  m_freem(m);
  if (err) {
    so->so_error = err;
    return SOCKET_ERROR;
  }
  return 0;
}

/*********************************************************************
*
*       t_getpeername()
*/
int t_getpeername(long s, struct sockaddr * addr, int * addrlen) {
  int r;

  LOCK_NET();
  r = _getname(s, addr, addrlen, PRU_PEERADDR);
  UNLOCK_NET();
  return r;
}

/*********************************************************************
*
*       t_getsockname()
*/
int t_getsockname(long s, struct sockaddr * addr, int * addrlen) {
  int r;

  LOCK_NET();
  r = _getname(s, addr, addrlen, PRU_SOCKADDR);
  UNLOCK_NET();
  return r;
}

/*********************************************************************
*
*       _setsockopt()
*/
static int _setsockopt(long hSock, int   level, int   name, void * arg, int arglen) {
  struct socket *   so;
  int   err;


  so = IP_SOCKET_h2p(hSock);
  if (so == NULL) {
    return SOCKET_ERROR;     // Socket has not been opened or has already been closed
  }
  so->so_error = 0;
  IP_LOG((IP_MTYPE_SOCKET, "SOCKET: setsockopt: name %d", name));
  /* is it a level IP_OPTIONS call? */
  if (level != IP_OPTIONS) {
    if ((err = sosetopt (so, name, arg)) != 0) {
       so->so_error = err;
       return SOCKET_ERROR;
    }
  } else {
    //
    // yup
    // level 1 options are for the IP packet level.
    // the info is carried in the socket CB, then put into the PACKET.
    //
    if (so->so_optsPack == NULL) {
      so->so_optsPack = _TryAlloc();
    }
    if (so->so_optsPack == NULL) {
      return SOCKET_ERROR;
    }
    if (name == IP_TTL_OPT) {
      so->so_optsPack->ip_tll = (U8)(*(U32 *)arg);
    } else {
      if (name == IP_TOS) {
        so->so_optsPack->ip_tos = (U8)(*(U32 *)arg);
      } else {
        return SOCKET_ERROR;
      }
    }
  }
  return 0;
}

/*********************************************************************
*
*       t_setsockopt()
*/
int t_setsockopt(long hSock, int   level, int   name, void * arg, int arglen) {
  int r;

  LOCK_NET();
  r = _setsockopt(hSock, level, name, arg, arglen);
  UNLOCK_NET();
  return r;
}

/*********************************************************************
*
*       _getsockopt()
*/
static int _getsockopt(long hSock, int level, int name, void * arg, int arglen) {
  struct socket *   so;
  int   err;

  so = IP_SOCKET_h2p(hSock);
  if (so == NULL) {
    return SOCKET_ERROR;     // Socket has not been opened or has already been closed
  }
  IP_LOG((IP_MTYPE_SOCKET, "SOCKET: getsockopt: name %d", name));
   /* is it a level IP_OPTIONS call? */
   if (level != IP_OPTIONS) {
	   /* nope */
      if ((err = sogetopt (so, name, arg)) != 0) {
         so->so_error = err;
         return SOCKET_ERROR;
      }
   } else {
	  /* yup */
	   /* level 1 options are for the IP packet level.
	    * the info is carried in the socket CB, then put
		* into the PACKET.
		*/
	  if (!so->so_optsPack)
         return SOCKET_ERROR;

	  if (name == IP_TTL_OPT)
         *(U32 *)arg = (U32)so->so_optsPack->ip_tll;
      else
	  if (name == IP_TOS)
         *(U32 *)arg = (U32)so->so_optsPack->ip_tos;
	  else
         return SOCKET_ERROR;
    }
   so->so_error = 0;
   return 0;
}

/*********************************************************************
*
*       t_getsockopt()
*/
int t_getsockopt(long hSock, int   level, int   name, void * arg, int arglen) {
  int r;

  LOCK_NET();
  r = _getsockopt(hSock, level, name, arg, arglen);
  UNLOCK_NET();
  return r;
}

/*********************************************************************
*
*       _recv()
*/
static int _recv (long hSock, char *   buf, int   len, int   flag) {
  struct socket *   so;
  int   err;
  int   sendlen = len;

  so = IP_SOCKET_h2p(hSock);
  if (so == NULL) {
    return SOCKET_ERROR;     // Socket has not been opened or has already been closed
  }
  if ((so->so_state & SO_IO_OK) != SS_ISCONNECTED)  {
    so->so_error = EPIPE;
    return SOCKET_ERROR;
  }
  so->so_error = 0;
  IP_LOG((IP_MTYPE_SOCKET_READ, "SOCKET: recv(), so %x, len %d", so, len));
  err = soreceive(so, NULL, buf, &len, flag);
  if (err) {
    so->so_error = err;
    return SOCKET_ERROR;
  }
  return sendlen - len;
}

/*********************************************************************
*
*       t_recv()
*/
int t_recv (long hSock, char *   buf, int   len, int   flag) {
  int r;

  LOCK_NET();
  r = _recv(hSock, buf, len, flag);
  UNLOCK_NET();
  return r;
}

/*********************************************************************
*
*       _recvfrom()
*/
static int _recvfrom(long hSock, char *   buf, int   len, int   flags, struct sockaddr * pFrom, int * pAddrLen) {
  struct socket *   so;
  struct mbuf *     sender = NULL;
  int   err;
  int   sendlen = len;

  so = IP_SOCKET_h2p(hSock);
  if (so == NULL) {
    return SOCKET_ERROR;     // Socket has not been opened or has already been closed
  }
  so->so_error = 0;
  err = soreceive(so, &sender, buf, &len, flags);
  /* copy sender info from mbuf to sockaddr */
  if (sender) {
    if (pFrom) {
      int AddrLen;
      if (pAddrLen == NULL) {
        IP_PANIC("API usage error.");
      }
      AddrLen = *pAddrLen;
      if ((unsigned)AddrLen > sizeof(struct sockaddr)) {
        AddrLen = sizeof(struct sockaddr);
        IP_WARN((IP_MTYPE_SOCKET_READ, "SOCKET: recvfrom: AddrLen invalid"));
      }
      IP_MEMCPY(pFrom, (MTOD(sender, struct sockaddr *)), AddrLen);
    }
    m_freem (sender);
  }
  if (err) {
    so->so_error = err;
    return SOCKET_ERROR;
  }
  return (sendlen - len); // OK return: amount of data actually sent
}


/*********************************************************************
*
*       t_recvfrom()
*
*/
int t_recvfrom(long hSock, char *   buf, int   len, int   flags, struct sockaddr * pFrom, int * pAddrLen) {
  int r;

  LOCK_NET();
  r = _recvfrom(hSock, buf, len, flags, pFrom, pAddrLen);
  UNLOCK_NET();
  return r;
}


/*********************************************************************
*
*       _sendto
*/
static int _sendto (long hSock, const char *   buf, int   len, int   flags, struct sockaddr * to, int   tolen) {
  struct socket * so;
  int sendlen;
  int err;
  struct mbuf * name;

  so = IP_SOCKET_h2p(hSock);
  if (so == NULL) {
    return SOCKET_ERROR;     // Socket has not been opened or has already been closed
  }
  so->so_error = 0;
  switch (so->so_type) {
  case SOCK_STREAM:
    //
    // This is a stream socket, so pass this request through
    // t_send() for its large-send support.
    //
    return t_send(hSock, buf, len, flags);
    /*NOTREACHED*/
  case SOCK_DGRAM:
    /* datagram (UDP) socket -- prepare to check length */
    sendlen = udp_maxalloc();
    break;
#ifdef IP_RAW
  case SOCK_RAW:
    /* raw socket -- prepare to check length */
    sendlen = ip_raw_maxalloc(so->so_options & SO_HDRINCL);
    break;
#endif /* IP_RAW */
  default:
    /* socket has unknown type */
    IP_WARN((IP_MTYPE_SOCKET, "SOCKET: Unknown type"));
    so->so_error = EFAULT;
    return SOCKET_ERROR;
  }
  //
  // Fall through for non-stream sockets: SOCK_DGRAM (UDP) and
  // SOCK_RAW (raw IP)
  //
  if (len > sendlen) {  // Check length against underlying stack's maximum
    so->so_error = EMSGSIZE;
    return SOCKET_ERROR;
  }
  //
  // If a sockaddr was passed, wrap it in an mbuf and pass it into the
  // bowels of the BSD code; else assume this is a bound UDP socket
  // and this call came from t_send() below.
  //
  if (to) {
    name = _sockargs(to, tolen);
    if (name == NULL) {
      so->so_error = ENOMEM;
      return SOCKET_ERROR;
    }
  } else {    /* hope user called bind() first... */
    name = NULL;
  }
  sendlen = len;
  err = _sosend (so, name, buf, &sendlen, flags);
  if (name) {
    m_freem(name);
  }
  if (err != 0) {
    so->so_error = err;
    return SOCKET_ERROR;
  }
  return (len - sendlen);
}

/*********************************************************************
*
*       t_sendto
*/
int t_sendto (long hSock, const char *   buf, int   len, int   flags, struct sockaddr * to, int   tolen) {
  int r;

  LOCK_NET();
  r = _sendto (hSock, buf, len, flags, to, tolen);
  UNLOCK_NET();
  return r;
}

/*********************************************************************
*
*       _GetMSS()
*
* Find TCP_MSS for socket passed.
*/
static int _GetMSS(struct socket * so) {
  int mss;
  struct inpcb * inp;
  struct tcpcb * tp;

  //
  // If we have a valid TCP socket, it contains a MaxSeg value which we can use.
  //
  inp = (struct inpcb *)so->so_pcb;         /* Map socket to IP cb */
  tp = (struct tcpcb *)inp->inp_ppcb;       /* Map IP to TCP cb */
  mss = tp->TrueMSS;
  return mss;
}


/*********************************************************************
*
*       t_send()
*/
int t_send(long hSock, const char *   buf, int      len, int flags) {
  struct socket *   so;
  int   e;       /* error holder */
  int   total_sent  =  0;
  int   maxpkt;
  int   sendlen;
  int   sent;


  while (len) {
    LOCK_NET();
    so = IP_SOCKET_h2p(hSock);
    if (so == NULL) {
      UNLOCK_NET();
      return SOCKET_ERROR;     // Socket has not been opened or has already been closed
    }
    if ((so->so_state & SO_IO_OK) != SS_ISCONNECTED) {
      IP_LOG((IP_MTYPE_SOCKET_STATE, "SOCKET: Send on socket w/o connection."));
      so->so_error = EPIPE;
      UNLOCK_NET();
      return SOCKET_ERROR;
    }
    so->so_error = 0;
    //
    // If this is not a stream socket, assume it is bound and pass to
    // t_sendto() with a null sockaddr
    //
    if (so->so_type != SOCK_STREAM) {
      UNLOCK_NET();
      return t_sendto(hSock, buf, len, flags, NULL, 0);
    }
    maxpkt = _GetMSS(so);
    if (len > maxpkt) {
      sendlen = maxpkt;  /* take biggest block we can */
    } else {
      sendlen = len;
    }
    sent = sendlen;

    e = _sosend (so, NULL, buf, &sendlen, flags);
    UNLOCK_NET();

    if (e != 0) {  /* sock_sendit failed? */
       /* if we simply ran out of bufs, report back to caller. */
       if ((e == ENOBUFS) || (e == EWOULDBLOCK)) {
          /* if we actually sent something before running out
           * of buffers, report what we sent;
           * else, report the error and let the application
           * retry the call later
           */
          if (total_sent != 0) {
             so->so_error = 0;
             break;      /* break out of while(len) loop */
          }
       }
       so->so_error = e;
       return SOCKET_ERROR;
    }
    /* if we can't send anymore, return now */
    if (sendlen != 0) {
       break;         /* break out of while(len) loop */
    }

    /* adjust numbers & pointers, and go do next send loop */
    sent -= sendlen;        /* subtract anything that didn't get sent */
    buf += sent;
    len -= sent;
    total_sent += sent;
  }

  return total_sent;
}


/*********************************************************************
*
*       _shutdown()
*/
static int _shutdown(long hSock, int   how) {
  struct socket *   so;
  int   err;

  so = IP_SOCKET_h2p(hSock);
  if (so == NULL) {
    return SOCKET_ERROR;     // Socket has not been opened or has already been closed
  }
  so->so_error = 0;
  IP_LOG((IP_MTYPE_SOCKET_STATE, "SOCKET: shutdown(), so %x how %d", so, how));
  err = soshutdown(so, how);
  if (err != 0) {
    so->so_error = err;
    return SOCKET_ERROR;
  }
  return 0;
}

/*********************************************************************
*
*       t_shutdown()
*/
int t_shutdown(long hSock, int   how) {
  int r;

  LOCK_NET();
  r = _shutdown(hSock, how);
  UNLOCK_NET();
  return r;
}

/*********************************************************************
*
*       _socketclose()
*/
static int _socketclose(long hSock) {
  struct socket * so;
  int err;

  so = IP_SOCKET_h2p(hSock);
  if (so == NULL) {
    return SOCKET_ERROR;     // Socket has not been opened or has already been closed
  }
  so->so_error = 0;
  err = _soclose(so);
  if (err != 0) {
    //
    // Do not do the following assignment since the socket structure
    // addressed by so has been freed by this point, jharan 12-10-98
    /*      so->so_error = err;   */
    return SOCKET_ERROR;
  }
  return 0;
}

/*********************************************************************
*
*       t_socketclose
*/
int t_socketclose(long hSock) {
  int r;

  LOCK_NET();
  r = _socketclose(hSock);
  UNLOCK_NET();
  return r;
}

/*********************************************************************
*
*       t_errno
*
*  Function description
*    Return last error on a socket. Return value is
*    undefined if the socket has not previously reported an error.
*/
int t_errno(long hSock) {
  struct socket *   so;

  so = IP_SOCKET_h2p(hSock);
  if (so == NULL) {
    return ENOTSOCK;     // Socket has not been opened or has already been closed
  }
  return so->so_error;
}

/*********************************************************************
*
*       IP_SOCKET_Alloc
*
*  Function description
*    Allocate storage space for a socket.
*    It is either fetched from the free queue or allocated.
*/
struct socket * IP_SOCKET_Alloc(void) {
  struct socket * pSock;

  //
  // Check if max. number of sockets allowed is exceeded
  //
  if (IP_SOCKET_Limit) {
    if (IP_SOCKET_Cnt >= IP_SOCKET_Limit) {
      IP_WARN((IP_MTYPE_SOCKET, "SOCKET: Socket limit reached."));    // Use IP_SOCKET_SetLimit() to set a larger limit or decrease number of sockets used.
      return (struct socket *)NULL;
    }
  }
  //
  // Perform the actual allocation
  //
  pSock = (struct socket *)IP_TryAllocWithQ(&IP_Global.SocketFreeQ, sizeof(struct socket));
  if (pSock) {
    IP_SOCKET_Cnt++;
    //
    // Update max value, which is used in debug builds only
    //
#if IP_DEBUG
    if (IP_SOCKET_Cnt > IP_SOCKET_Max) {
      IP_SOCKET_Max = IP_SOCKET_Cnt;
    }
#endif
    pSock->Handle = ++IP_Global.NextSocketHandle;
  }
  return pSock;
}

/*********************************************************************
*
*       IP_SOCKET_Free
*
*  Function description
*    Frees storage space for a socket by adding it to the socket free queue.
*/
void IP_SOCKET_Free(struct socket * pSock) {
  IP_SOCKET_Cnt--;
  IP_Q_Add(&IP_Global.SocketFreeQ, pSock);
}


/*********************************************************************
*
*       IP_SOCKET_p2h()
*/
long IP_SOCKET_p2h(struct socket * pSock) {
#if 0
  if (pSock == NULL) {
    return SOCKET_ERROR;
  }
#endif
  return pSock->Handle;
}

/*********************************************************************
*
*       IP_SOCKET_h2p()
*/
struct socket * IP_SOCKET_h2p(long hSock) {
  struct socket * pSock;
  struct socket ** ppSock;

  pSock = (struct socket *)(IP_Global.SocketInUseQ.q_head);                // First socket
  if (pSock->Handle == hSock) {   // First socket a match ?
    return pSock;
  }
  //
  // Iterate over linked list to find a socket with matching handle.
  //
  do {
    ppSock = &pSock->next;
    pSock  = *ppSock;
    if (pSock == NULL) {
      break;
    }
    if (pSock->Handle == hSock) {                                        // Match ?
/*
      //
      // Make this socket the first in the queue to speed up next search, which is most likely for same socket
      //
      *ppSock = pSock->next;                                             // Unlink
      if (pSock->next == NULL) {
        IP_Global.SocketInUseQ.q_tail = pPrev;
      }
      pSock->next = (struct socket *)(IP_Global.SocketInUseQ.q_head);    // Link second element
      ppSock  = (struct socket **)&(IP_Global.SocketInUseQ.q_head);
      *ppSock = pSock;                                                   // Make it the first element
*/
      return pSock;
    }
  } while(1);

  IP_WARN((IP_MTYPE_SOCKET, "SOCKET: Socket %d not in socket list", hSock));
  return NULL;
}

/*********************************************************************
*
*       IP_SOCKET_SetLimit
*
*  Function description
*    Sets the maximum number of allowed sockets.
*/
void IP_SOCKET_SetLimit(unsigned Limit) {
  IP_SOCKET_Limit = Limit;
}

/*********************************************************************
*
*       IP_SOCKET_SetDefaultOptions
*
*  Function description
*    Sets the maximum number of allowed sockets.
*/
void IP_SOCKET_SetDefaultOptions(U16 v) {
  IP_SOCKET_DefaultOptions = v;
}

char *   so_emessages[] = {
   "",               /*  0 */
   "ENOBUFS",        /*  1 */
   "ETIMEDOUT",      /*  2 */
   "EISCONN",        /*  3 */
   "EOPNOTSUPP",     /*  4 */
   "ECONNABORTED",   /*  5 */
   "EWOULDBLOCK",    /*  6 */
   "ECONNREFUSED",   /*  7 */
   "ECONNRESET",     /*  8 */
   "ENOTCONN",       /*  9 */
   "EALREADY",       /* 10 */
   "EINVAL",         /* 11 */
   "EMSGSIZE",       /* 12 */
   "EPIPE",          /* 13 */
   "EDESTADDRREQ",   /* 14 */
   "ESHUTDOWN",      /* 15 */
   "ENOPROTOOPT",    /* 16 */
   "EHAVEOOB",       /* 17 */
   "ENOMEM",         /* 18 */
   "EADDRNOTAVAIL",  /* 19 */
   "EADDRINUSE",     /* 20 */
   "EAFNOSUPPORT",   /* 21 */
   "EINPROGRESS",    /* 22 */
   "ELOWER",         /* 23  lower layer (IP) error */
};


/* FUNCTION: so_perror()
 *
 * so_perror() - return a pointer to a static string which is a short
 * mnemonic for the socket error message.
 *
 *
 * PARAM1: int ecode
 *
 * RETURNS: error text string
 */

char * so_perror(int ecode) {
  if (ecode < (sizeof(so_emessages)/sizeof(char*))) {
    return(so_emessages[ecode]);
  } else {
    return "";
  }
}












/*************************** End of file ****************************/

