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
File    : IP_TCP_pcb.c
Purpose : Handles protocol control block (for incoming TCP packets)
--------  END-OF-HEADER  ---------------------------------------------
*/

/* Additional Copyrights: */
/*
* Copyright 1997- 2000 By InterNiche Technologies Inc. All rights reserved
* Copyright (c) 1982, 1986 Regents of the University of California.
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
#include "IP_mbuf.h"       /* BSD-ish Sockets includes */


/* 2/9/00 - modified in_pcblookup() put the matched pcb at the head
 * of the list to speed future searches. The stats below are to gauge
 * the success of this.
 */
long     inpcb_cachehits   =  0;
long     inpcb_cachemiss   =  0;

QUEUE IP_FreeInPCBQ;


/*********************************************************************
*
*       _TryAlloc
*
*  Function description
*    Allocate storage space.
*    It is either fetched from the free queue or allocated.
*/
static struct inpcb * _TryAlloc(void) {
  return (struct inpcb*)IP_TryAllocWithQ(&IP_FreeInPCBQ, sizeof(struct inpcb));
}

/*********************************************************************
*
*       _Free
*
*  Function description
*    Frees storage space for a TCPIPHDR by adding it to the free queue.
*/
static void _Free(struct inpcb * p) {
  IP_Q_Add(&IP_FreeInPCBQ, p);
}



/* FUNCTION: in_pcballoc()
 *
 * Allocate a protocol control block and prepare it for use.
 *
 * PARAM1: struct socket *so
 * PARAM2: struct inpcb *head
 *
 * RETURNS: PCB if OK, NULL if out of memory.
 */

int IP_TCP_PCB_alloc(struct socket * so, struct inpcb * head) {
   struct inpcb * inp;

   inp = _TryAlloc();
   if (inp == NULL) {
     return ENOMEM;
   }
   inp->inp_head = head;
   inp->inp_socket = so;

   inp->ifp      = &IP_aIFace[0];
   insque(inp, head);
   so->so_pcb = inp;
   return 0;
}


/*********************************************************************
*
*       IP_TCP_PCB_detach()
*/
void IP_TCP_PCB_detach(struct inpcb * inp) {
  struct socket *   so =  inp->inp_socket;

  IP_LOG((IP_MTYPE_SOCKET_STATE, "SOCKET: free %x", so));
  so->so_pcb = 0;
  sofree(so);
  remque(inp);
  _Free (inp);
}




/* FUNCTION: IP_TCP_PCB_bind()
 *
 * Guts of socket "bind" call for TCP.
 *
 * PARAM1: struct inpcb *inp
 * PARAM2: struct mbuf *nam
 *
 * RETURNS: 0 if OK, else one of the socket error codes
 */


int IP_TCP_PCB_bind(struct inpcb * inp, struct mbuf *  nam) {
   struct socket *   so =  inp->inp_socket;
   struct inpcb * head  =  inp->inp_head;
   struct sockaddr_in * sin;
   U16  lport =  0;

   if (inp->inp_lport || inp->inp_laddr.s_addr != INADDR_ANY)
      return (EINVAL);
   if (nam == 0)
      goto noname;
   sin = MTOD(nam, struct sockaddr_in *);

   /*
    * removed test here for "if (nam->m_len != sizeof (*sin))"
    * since it really complicatges supporting dual IPv4/v6, and
    * the 2.0 stack now checks this in t_bind(). -JB-
    */
   if (sin->sin_addr.s_addr != INADDR_ANY) {
     if (ip_mymach(sin->sin_addr.s_addr) != sin->sin_addr.s_addr) {
       return (EADDRNOTAVAIL);
     }
   }
   lport = sin->sin_port;
   if (lport)  {
      int   wild  =  0;

      /* even GROSSER, but this is the Internet */
      if ((so->so_options & SO_REUSEADDR) == 0 &&
          ((so->so_proto->pr_flags & PR_CONNREQUIRED) == 0 ||
          (so->so_options & SO_ACCEPTCONN) == 0))
      {
         wild = INPLOOKUP_WILDCARD;
      }
      if (IP_TCP_PCB_lookup(head,
          0L, 0, sin->sin_addr.s_addr, lport, wild))
      {
         return (EADDRINUSE);
      }
   }
   inp->inp_laddr = sin->sin_addr;
noname:
   if (lport == 0) {
      do {
        U16 inp_lport;
        inp_lport = head->inp_lport++;
         if (inp_lport < IPPORT_RESERVED || inp_lport > IPPORT_USERRESERVED)  {
           head->inp_lport = IPPORT_RESERVED;
         }
         lport = htons(head->inp_lport);
      } while(IP_TCP_PCB_lookup(head, 0L, 0, inp->inp_laddr.s_addr, lport, 0));
   }
   inp->inp_lport = lport;
   return (0);
}


/* FUNCTION: IP_TCP_PCB_connect()
 *
 * Connect from a socket to a specified address.
 * Both address and port must be specified in argument sin.
 * If don't have a local address for this socket yet,
 * then pick one.
 *
 * PARAM1: struct inpcb *inp
 * PARAM2: struct mbuf *nam
 *
 * RETURNS:
 */

int IP_TCP_PCB_connect(struct inpcb * inp, struct mbuf *  nam) {
  unsigned long ifaddr;
  struct sockaddr_in * sin;

  sin =  MTOD(nam,   struct sockaddr_in *);
  if (nam->m_len < sizeof (*sin)) {
    return EINVAL;
  }
  if (sin->sin_family != AF_INET) {
    return EAFNOSUPPORT;
  }
  if (sin->sin_port == 0) {
    return EADDRNOTAVAIL;
  }
  /*
  * If the destination address is INADDR_ANY,
  * use the primary local address.
  * If the supplied address is INADDR_BROADCAST,
  * and the primary interface supports broadcast,
  * choose the broadcast address for that interface.
  */
  if (sin->sin_addr.s_addr == INADDR_ANY) {
   if (inp && inp->ifp) {
     sin->sin_addr.s_addr = inp->ifp->n_ipaddr;
   } else {
     return EADDRNOTAVAIL;
   }
  } else if (sin->sin_addr.s_addr == INADDR_BROADCAST) {
    return (EADDRNOTAVAIL);
  }

  if (inp->inp_laddr.s_addr == INADDR_ANY) {
#ifdef MULTI_HOMED
      ip_addr hop1;     /* dummy for pass to iproute() */
      NET npnet;     /* the netport iface we can send on */
      /* call netport stack's IP routing */
      npnet = iproute(sin->sin_addr.s_addr, &hop1);
      if (!npnet) {
        return EADDRNOTAVAIL;
      }
      ifaddr = npnet->n_ipaddr;  /* local address for this host */
#else    /* not netport MULTI_HOMED, use 0th (only) iface */
      ifaddr = IP_aIFace[0].n_ipaddr;
#endif   /* MULTI_HOMED */
   } else {  /* inp->inp_laddr.s_addr != INADDR_ANY */
     ifaddr = inp->inp_laddr.s_addr;  /* use address passed */
   }

   if (IP_TCP_PCB_lookup(inp->inp_head, sin->sin_addr.s_addr, sin->sin_port, ifaddr, inp->inp_lport, 0)) {
     return (EADDRINUSE);
   }
   if (inp->inp_laddr.s_addr == INADDR_ANY) {
     if (inp->inp_lport == 0)  {
       (void)IP_TCP_PCB_bind(inp, (struct mbuf *)0);
     }
     inp->inp_laddr.s_addr = ifaddr;
   }
   inp->inp_faddr = sin->sin_addr;
   inp->inp_fport = sin->sin_port;
   return 0;
}



/*********************************************************************
*
*       IP_TCP_PCB_disconnect()
*/
void IP_TCP_PCB_disconnect(struct inpcb * inp) {
  inp->inp_faddr.s_addr = INADDR_ANY;
  inp->inp_fport = 0;
  if (inp->inp_socket->so_state & SS_NOFDREF) {
    IP_TCP_PCB_detach (inp);
  }
}

/*********************************************************************
*
*       in_setsockaddr()
*/
void in_setsockaddr(struct inpcb * inp, struct mbuf *  nam) {
  struct sockaddr_in * sin;

  nam->m_len = sizeof (*sin);
  sin = MTOD(nam, struct sockaddr_in *);
  IP_MEMSET(sin, 0, sizeof (*sin));
  sin->sin_family = AF_INET;
  sin->sin_port = inp->inp_lport;
  sin->sin_addr = inp->inp_laddr;
}



/*********************************************************************
*
*       in_setpeeraddr()
*
*/
void in_setpeeraddr(struct inpcb * inp, struct mbuf *  nam) {
  struct sockaddr_in * sin;

  nam->m_len = sizeof (*sin);
  sin = MTOD(nam, struct sockaddr_in *);
  IP_MEMSET(sin, 0, sizeof (*sin));
  sin->sin_family = AF_INET;
  sin->sin_port = inp->inp_fport;
  sin->sin_addr = inp->inp_faddr;
}

/*********************************************************************
*
*       IP_TCP_PCB_lookup()
*
* Find a TCP connection in the passed list which matches the
* parameters passed.
*
*
* RETURNS: NULL if not found, else entry in inpcb list.
*/
struct inpcb * IP_TCP_PCB_lookup(struct inpcb * head, U32   faddr, U16  fport, U32   laddr, U16  lport, int   flags) {
  struct inpcb * inp;
  struct inpcb * pMatch;
  int   matchwild   =  3;
  int   wildcard;

  pMatch = NULL;
  //
  // Search thru list of connections to find a match
  //
  for (inp = head->inp_next; inp != head; inp = inp->inp_next) {
    if (inp->inp_lport != lport) {
      continue;
    }

    /* Skip non IPv4 sockets */
    if (inp->inp_socket->so_domain != AF_INET) {
      continue;
    }

    wildcard = 0;
    if (inp->inp_laddr.s_addr != INADDR_ANY) {
      if (laddr == INADDR_ANY) {
        wildcard++;
      } else if (inp->inp_laddr.s_addr != laddr) {
        continue;
      }
    } else {
      if (laddr != INADDR_ANY) {
        wildcard++;
      }
    }
    if (inp->inp_faddr.s_addr != INADDR_ANY) {
      if (faddr == INADDR_ANY) {
        wildcard++;
      } else if (inp->inp_faddr.s_addr != faddr || inp->inp_fport != fport) {
        continue;
      }
    } else {
      if (faddr != INADDR_ANY) {
        wildcard++;
      }
    }
    if (wildcard && (flags & INPLOOKUP_WILDCARD) == 0) {
       continue;
    }
    if (wildcard < matchwild) {
      pMatch = inp;
      matchwild = wildcard;
      if (matchwild == 0) {
        break;
      }
    }
  }

  //
  // Bring found entry to beginning of the list so next time it is first
  //
  if (pMatch) {
    if (head->inp_next == pMatch) { /* got cache hit? */
      IP_STAT_INC(inpcb_cachehits);
    } else {
      IP_STAT_INC(inpcb_cachemiss);
      /* "cache" the match to be first checked next time. */
      pMatch->inp_next->inp_prev = pMatch->inp_prev; /*unlink match */
      pMatch->inp_prev->inp_next = pMatch->inp_next;

      /* relink pMatch as head->inp_next */
      pMatch->inp_next = head->inp_next;
      head->inp_next = pMatch;
      pMatch->inp_prev = head;
      pMatch->inp_next->inp_prev = pMatch;
    }
  }
  return pMatch;
}


/*************************** End of file ****************************/


