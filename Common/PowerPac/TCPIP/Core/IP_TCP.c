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
File    : IP_TCP.c
Purpose : TCP layer's emulation of BSD routines called by the TCP code.
--------  END-OF-HEADER  ---------------------------------------------
*/

/* Additional copyrights */
/* Copyright 1997- 2000 By InterNiche Technologies Inc. All rights reserved */


#include "IP_Int.h"
#include "IP_sockvar.h"
#include "IP_TCP_pcb.h"
#include "IP_ICMP.h"
#include "IP_protosw.h"
#include "IP_TCP.h"
#include "IP_mbuf.h"       /* BSD-ish Sockets includes */


struct mbstats {
   U32   allocs;
   U32   frees;
}  mbstat;


U16  select_wait;      /* select wait bits */

struct TcpMib  tcpmib;  /* mib stats for TCP layer */
struct inpcb IP_tcb;                // head of TCP control blocks

U8 default_wind_scale = 0;   /* default value for window scaling option */

/* protocol switch table entry, for TCP: */
struct protosw tcp_protosw =  {
   SOCK_STREAM,
   IP_PROT_TCP,
   PR_CONNREQUIRED|PR_WANTRCVD,
#ifdef CTL_INPUT
   tcp_ctlinput,
#endif   /* CTL_INPUT */
#ifdef CTL_OUTPUT
   tcp_ctloutput,
#endif
   tcp_usrreq,
   tcp_init
};

#ifdef UDP_SOCKETS
struct protosw udp_protosw =  {
   SOCK_DGRAM,          /* type - datagram */
   IP_PROT_UDP,         /* number (UDP over IP) */
   PR_ATOMIC|PR_ADDR,   /* flags */
#ifdef CTL_INPUT
   NULL,
#endif   /* CTL_INPUT */
#ifdef CTL_OUTPUT
   NULL,
#endif
   udp_usrreq, /* this is where all the real work happens */
   NULL    /* state-less UDP needs no init call */
};
#endif   /* UDP_SOCKETS */

/*********************************************************************
*
*       IP_TCP_CalcChecksum()
*
* The IP header passed is assumed to be
* prepended to the rest of a TCP packet, i.e. followed by TCP header
* and data to be checksummed. Returns the correct checksum for the
* packet.
*  The checksum field of the data is modified.
*  This is not reversed, since since routine is called to compute
*  the checksum on an outgoing packet (in which case the checksum is filled in later)
*  or an incoming packet, in which case it can be be read before the computation.
*
*  Notes
*    (1) Optimizations used
//
  // The TCP checksum spec. says to Init TCP header cksum field to 0 before
  // summing. We also need to inclue in the sum a protocol type (always 6
  // for TCP). the Ip length, and the IP addresses. Nominally this is done
  // by constructing a "pseudo header" and prepending it to the tcp header
  // and data to be summed. Instead, we add the type & length (they
  // cannot overflow a 16 bit field) and put them in the cksum field.
  // We include the IP addresses by passing them to the lower level
  // fast sum routine. This results in their values being factored into
  // the sum and the cksum field contributes zero.
  //
  //
  // Pass a pointer to the beginning of the IP address area into the IP header
  // the the low level sum routine. Add the size of these two IP addresses to
  // the length.
  //
*/
unsigned IP_TCP_CalcChecksum(struct ip * pip) {
  U16  iphlen;  /* length of IP header, usually 20 */
  unsigned  tcplen;  /* length of TCP header and data */
  unsigned newsum;
  struct tcphdr * tp;

  iphlen = (U16)ip_hlen(pip);
  tcplen = htons(pip->ip_len) - iphlen;
  tp = (struct tcphdr*)ip_data(pip);      /* get TCP header */

  tp->th_sum = htons(tcplen + 6);
  newsum = IP_CalcChecksum_Byte(((U8*)tp) - 8, tcplen + 8);

  return (U16)~newsum;
}



/*********************************************************************
*
*       pffindtype()
*
*/
struct protosw * pffindtype(int domain, int type) {
  /* check that the passed domain is vaid for the build */
  if (domain != AF_INET) {
    IP_WARN((IP_MTYPE_CORE, "CORE: Domain must be AF_INET"));
    return NULL;
  }

  if (type == SOCK_STREAM) {
    return &tcp_protosw;
  }
#ifdef UDP_SOCKETS
  else if(type == SOCK_DGRAM) {
    return &udp_protosw;
  }
#endif   /* UDP_SOCKETS */
  return NULL;
}



/*********************************************************************
*
*       pffindproto()
*
*/
struct protosw * pffindproto(int domain, int protocol, int type) {
  switch (protocol) {
#ifdef BSD_SOCKETS
  case IP_PROT_TCP:
    if (type == SOCK_STREAM) {
      break;
    }
    IP_WARN((IP_MTYPE_TCP, "TCP: IP_PROT_TCP protocol on non-SOCK_STREAM type socket"));
    return NULL;
  case IP_PROT_UDP:
    if (type == SOCK_DGRAM) {
      break;
    }
    IP_WARN((IP_MTYPE_TCP, "TCP: IP_PROT_UDP protocol on non-SOCK_DGRAM type socket"));
    return NULL;
#endif /* BSD_SOCKETS */
  case 0:
    /* let protocol default based on socket type */
    break;
  default:
    IP_WARN((IP_MTYPE_TCP, "TCP: unknown/unsupported protocol on socket"));
    return NULL;
  }
  return(pffindtype(domain, type));   /* map to findtype */
}




/*********************************************************************
*
*       IP_MBUF_Get()
*
* The sole mbuf get routine for this port. These no
* version with a wait option. This gets a netport style nbuf to hold
* the data & maps it into a structure designed to fool the BSD TCP
* code into thinking it's an mbuf.
*
*/
#if IP_DEBUG
struct mbuf * IP_MBUF_Get(int type, int len)
#else
struct mbuf * IP_MBUF_Get(          int len)
#endif
{
  struct mbuf *  m;
  IP_PACKET * pkt = NULL;

#if IP_DEBUG
  if (type < MT_RXDATA || type > MT_IFADDR) {
    IP_PANIC("Wrong M-Type");
  }
#endif
  //
  // If caller has data (len >= 0), we need to allocate
  // a packet buffer; else all we need is the mbuf
  //
  if (len != 0) {
    pkt = IP_PACKET_Alloc(len);
    if (!pkt) {
      return NULL;
    }
  }

  m = (struct mbuf *)IP_Q_TryGetRemoveFirst(&IP_Global.MBufFreeQ);
  if (!m) {
    if (pkt) {
      pk_free(pkt);
    }
    return NULL;
  }
#if IP_DEBUG
   m->m_type = type;
#endif
   m->m_len  = 0;
   m->m_next = NULL;
   m->m_act  = NULL;
   if (len == 0) {
      m->pkt     = NULL;
   } else {
      m->pkt = pkt;
      /* set m_data to the part where tcp data should go */
      m->m_data = pkt->pData = pkt->pBuffer;
   }
   IP_STAT_INC(mbstat.allocs);        /* maintain local statistics */
   IP_Q_Add(&IP_Global.MBufInUseQ, m);
   return m;
}


/*********************************************************************
*
*       m_free()
*
* m_free() frees mbufs allocated above. Retuns pointer to "next"
* mbuf.
*
*/
struct mbuf * m_free(struct mbuf * m) {
  struct mbuf *  nextptr;

  //
  // Some checks (debug version only)
  //
  if (IP_Global.MBufInUseQ.q_head == NULL) {
    IP_PANIC("mfree: q_len");
  }
#if IP_DEBUG
  if (m->m_type < MT_RXDATA || m->m_type > MT_IFADDR) {
    if (m->m_type == MT_FREE) {
      IP_PANIC("m_free: Double free of Buffer");
    } else {
      IP_PANIC("m_free: Illegal type");
    }
  }
#endif
  nextptr = m->m_next;    /* remember value to return */

  IP_Q_RemoveItem(&IP_Global.MBufInUseQ, m);
#if IP_DEBUG
  m->m_type = MT_FREE;    /* this may seem silly, but helps error checking */
#endif
  if (m->pkt) {
    pk_free(m->pkt);     /* free up the netport buffer */
  }
  IP_STAT_INC(mbstat.frees);
  IP_Q_Add(&IP_Global.MBufFreeQ, m);
  return nextptr;
}



/*********************************************************************
*
*       m_freem()
*
*  m_freem() frees a list of mbufs
*
*/
void m_freem(struct mbuf * m) {
  while (m != NULL) {
    m = m_free(m);
  }
}



/*********************************************************************
*
*       m_copy()
*
* Make a copy of an mbuf chain starting "off" bytes from
* the beginning, continuing for "len" bytes. If len is M_COPYALL,
* copy to end of mbuf. Returns pointer to the copy.
*
*/
struct mbuf * m_copy(struct mbuf * m, int off, int len, unsigned Prepend) {
   struct mbuf *  nb, * head, *  tail;
   int   tocopy;

   if (len == 0) { /* nothing to do */
      return NULL;
   }

#if IP_DEBUG > 0
   /* sanity test parms */
   if (off < 0 || (len < 0 && len != M_COPYALL)) {
     IP_WARN((IP_MTYPE_TCP, "CORE: MBuf operation illegal"));
     return NULL;
   }
#endif

   /* move forward through mbuf q to "off" point */
   while (off > 0) {
      if (!m) {
        IP_WARN((IP_MTYPE_TCP, "CORE: MBuf pointer NULL"));
        return NULL;
      }
      if (off < (int)m->m_len) {
        break;
      }
      off -= m->m_len;
      m = m->m_next;
   }

   head = tail = NULL;

   while (len > 0) {
     if (m == NULL) { /* at end of queue? */
       IP_PANIC("m_copy: bad len");
       return NULL;
     }
     tocopy = (int)MIN(len, (int)(m->m_len - off));

     //
     // mbuf data is expected to be word aligned.
     // So if the offset isn't aligned, we must
     // copy the buffer instead of cloning it.
     // Also, don't permit multiple clones; they sometimes
     // lead to corrupted data.
     //
     if (((off & 3) == 0) && (m->pkt->UseCnt == 1)) {
       /* Rather than memcpy every mbuf's data, "clone" the data by
       * making a duplicate of the mbufs involved and bumping the
       * UseCnt of the actual packet structs
       */
       nb = MBUF_GET(m->m_type);
       if (nb == NULL) {
         goto nospace;
       }

       m->pkt->UseCnt++;     /* bump pkt use count to clone it */

       /* set up new mbuf with pointers to cloned packet */
       nb->pkt = m->pkt;
       nb->m_data = m->m_data + off;
       nb->m_len = tocopy;

       IP_STAT_INC(IP_TCP_Stat.mclones);
       IP_STAT_ADD(IP_TCP_Stat.mclonedbytes, tocopy);
     } else {
       nb = MBUF_GET_WITH_DATA (m->m_type, tocopy + Prepend);
       if (nb == NULL) {
         goto nospace;
       }
       nb->m_data += Prepend;
       IP_MEMCPY(nb->m_data, m->m_data+off, tocopy);
       nb->m_len = tocopy;  /* set length of data we just moved into new mbuf */

       IP_STAT_INC(IP_TCP_Stat.mcopies);
       IP_STAT_ADD(IP_TCP_Stat.mcopiedbytes, tocopy);
     }

     len -= tocopy;
     off = 0;
     if (tail) {     /* head & tail are set by first pass thru loop */
       tail->m_next = nb;
     } else {
       head = nb;
     }
     tail = nb;     /* always make new mbuf the tail */
     m = m->m_next;
   }
   return head;

nospace:
   m_freem (head);
   return NULL;
}




/*********************************************************************
*
*       m_adj()
*
* slice "length" data from the head of an mbuf
*
*/
void m_adj(struct mbuf * mp, int len) {
  struct mbuf *  m;
  int   count;

  if ((m = mp) == NULL) {
    return;
  }

  if (len >= 0) {
    while (m != NULL && len > 0) {
      if (m->m_len <= (unsigned)len) {
        len -= m->m_len;
        m->m_len = 0;
        m = m->m_next;
      } else {
        m->m_len -= len;
        m->m_data += len;
        break;
      }
    }
  } else {
    /*
     * Trim from tail.  Scan the mbuf chain,
     * calculating its length and finding the last mbuf.
     * If the adjustment only affects this mbuf, then just
     * adjust and return.  Otherwise, rescan and truncate
     * after the remaining size.
     */
    len = -len;
    count = 0;
    for (;;) {
      count += m->m_len;
      if (m->m_next == (struct mbuf *)0) {
        break;
      }
      m = m->m_next;
    }
    if (m->m_len >= (unsigned)len) {
       m->m_len -= len;
       return;
    }
    count -= len;
    /*
     * Correct length for chain is "count".
     * Find the mbuf with last data, adjust its length,
     * and toss data from remaining mbufs on chain.
     */
    for (m = mp; m; m = m->m_next) {
      if (m->m_len >= (unsigned)count) {
        m->m_len = count;
        break;
      }
      count -= m->m_len;
    }
    while ((m = m->m_next) != (struct mbuf *)NULL) {
      m->m_len = 0;
    }
  }
}


/*********************************************************************
*
*       mbuf_len ()
*
*   mbuf_len() - Compute the length of a chain of m_bufs.
*/
int mbuf_len (struct mbuf * m) {
  int   len   =  0;

  while (m) {
    len += m->m_len;
    m = m->m_next;
  }
  return len;
}



/*********************************************************************
*
*       dtom()
*
* Given a data pointer, return the mbuf it's in.
*/
struct mbuf * dtom(void * data) {
  QUEUE_ITEM* qptr;
  struct mbuf *  m;

  for (qptr = IP_Global.MBufInUseQ.q_head; qptr; qptr = qptr->qe_next) {
    m = (struct mbuf *)qptr;
    if (IN_RANGE(m->pkt->pBuffer, m->pkt->BufferSize, (char*)data)) {
      return (struct mbuf *)qptr;
    }
  }
  IP_PANIC("dtom");    /* data not found in any "in use" mbuf */
  return NULL;
}


/*********************************************************************
*
*       remque ()
*
* remque() - Used for removing overlapping tcp segments from input
* queue. Adapted from BSD/Mango, but "struct queue" conflicted with
* NetPort struct; so it uses the a contrived BSDq structure.
*/

struct bsdq {
   struct bsdq *  next; /* for overlaying on ipovly, et.al. */
   struct bsdq *  prev;
};


void remque (void * arg) {
  struct bsdq *  old;

  old = (struct bsdq *)arg;
  if (!old->prev) {
    return;
  }
  old->prev->next = old->next;
  if (old->next) {
    old->next->prev = old->prev;
  }
}

/*********************************************************************
*
*       insque()
*
*/

void insque(void * n, void * p) {
  struct bsdq *  newe, *  prev;

  newe = (struct bsdq *)n;
  prev = (struct bsdq *)p;
  newe->next = prev->next;
  newe->prev = prev;
  prev->next = newe;
  if (newe->next) {
    newe->next->prev = newe;
  }
}


/*********************************************************************
*
*       IP_MBUF_Init()
*
*/
void IP_MBUF_Init(void) {
  int   i;
  /* total netport buffers:
  * Get twice the number of packet buffers plus 3, to help
  * support UDP. if you are receiving UDP packets faster
  * than your UDP server can process them, its possible for
  * all of your packet buffers to end up in the UDP socket's
  * receive queue. each of these packet buffers has associated with
  * it 2 mbufs, one for the packet buffer itself and one for the
  * address of the remote host that the packet came from. in order
  * for this socket queue to get emptied and the receive buffers to
  * get freed, the socket has to be read and what's the first thing
  * that soreceive() does to try to do this? it tries to allocate
  * an mbuf, so if you only have twice as many mbufs as packet
  * buffers, soreceive() can't complete and the packet buffers stay
  * on the queue, so we allocate 3 extra mbufs in the hope that
  * this will allow soreceive() to complete and free up the packet
  * buffers. yes, its kind of an ugly hack and 3 is a wild guess.
  */
  unsigned bufcount = (IP_Global.aBufferConfigNum[0] + IP_Global.aBufferConfigNum[1]) * 2 + 3;
  struct mbuf *  m; /* scratch mbuf for IP_Global.MBufFreeQ init */

  for (i = 0; i < (int)bufcount; i++)  {
    m = (struct  mbuf *)IP_AllocZeroed(sizeof(struct mbuf));    // Panic if no memory
    IP_Q_Add(&IP_Global.MBufFreeQ, m);
  }
#if IP_DEBUG_Q
   IP_Global.MBufFreeQ.q_min = (int)bufcount;
#endif

}



/*********************************************************************
*
*       ip_output()
*
*  Function description
*    Called by tcp_output() to send a tcp segment on the
*    IP layer. We convert the mbufs passed into a IP_PACKET * and pass it on down.
*
*  Return value
*    0        OK
*    != 0     Error
*
*/
int ip_output(struct mbuf * m1) { /* mbuf chain with data to send */
  struct ip * bip;
  struct tcphdr *   tcpp;
  IP_PACKET * pPacket;
  struct mbuf * m2;
  char * pData;
  int   Space;
  int   e;                 // Error holder
  int   total;
  U16 Checksum;
  int HeaderSize;
  NET       * pNet;            // Interface for which this entry is valid

  pNet = &IP_aIFace[0];
  /* reassemble mbufs into a contiguous packet. Do this with as
  * little copying as possible. Typically the mbufs will be either
  * 1) a single mbuf with iptcp header info only (e.g.tcp ACK
  * packet), or 2) iptcp header with data mbuf chained to it, or 3)
  * #2) with a tiny option data mbuf between header and data.
  */
  do {
    m2 = m1->m_next;
    if (m2 == NULL) {
      break;
    }
    //
    // M1 = M1 + M2: Append M2 to M1, free M2.
    // This is typically used to copy TCP-options past TCP-header
    //
    if (m2->m_len < 16) {
      pPacket = m1->pkt;
      if ((pPacket->pBuffer + pPacket->BufferSize) > /* make sure m2 will fit in m1 */
         (m1->m_data + m1->m_len + m2->m_len))  {
        IP_MEMCPY((m1->m_data + m1->m_len), m2->m_data, m2->m_len);
        m1->m_len += m2->m_len;
        m1->m_next = m2->m_next;
        m_free(m2);
        IP_STAT_INC(IP_TCP_Stat.oappends);
        continue;
      }
    }

    //
    // M2 = M1 + M2:
    //
    // If we still have two or more buffers, more copying:
    // try prepending m1 to m2, first see if it fits:
    // This is typically used to copy TCP-header & options BEFORE data
    //
    Space = m2->m_data - m2->pkt->pBuffer;  // Space before actual data
    Space -= IP_aIFace[0].n_lnh;            // Subtract space needed by LAN (For ethernet, this is 14 + 2 bytes)
    if (((int)m1->m_len <= Space)
        && ((m1->m_len & 3) == 0)
        && (((unsigned)m2->m_data & 3) == 0)    // Data can be misaligned in case we resend a packet behind a small packet and data has been re-organized
    ) {  /* and stay aligned */
      IP_MEMCPY((m2->m_data - m1->m_len), m1->m_data, m1->m_len);
      m2->m_data -= m1->m_len;   /* fix target to reflect prepend */
      m2->m_len += m1->m_len;
      m_free(m1);    /* free head (copied) mbuf */
      m1 = m2;   /* move other mbufs up the chain */
      IP_STAT_INC(IP_TCP_Stat.oprepends);
      continue;
    }

    //
    //
    // M3 = M1 + M2:
    // This is typically used to copy multiple chunks of data into a single packet
    //
    // Compute number of bytes first
    //
    total = 0;
    for (m2 = m1; m2; m2 = m2->m_next) {
      total += m2->m_len;
    }
    //
    // Alloc new packet as destination for all membufs
    //
    HeaderSize = pNet->n_lnh;
    pPacket = IP_PACKET_Alloc(total + HeaderSize);
    if (!pPacket) {
      do {
        m2 = m1->m_next;
        m_free(m1);
        m1 = m2;
      } while (m1);
      return ENOBUFS;
    }
    pData      = pPacket->pBuffer + HeaderSize;
    pPacket->pData = pData;
    //
    // Copy all membufs into new packet. Free membufs except for the first one
    //
    m2 = m1;
    do {
      int NumBytes;
      NumBytes = m2->m_len;
      IP_MEMCPY(pData, m2->m_data, NumBytes);
      pPacket->NumBytes += NumBytes;
      pData         += NumBytes;
      if (m2 != m1) {  /* save original head */
        m_free(m2);
      }
      m2 = m2->m_next;
      IP_STAT_INC(IP_TCP_Stat.ocopies);
    } while (m2);

    /* release the original mbufs packet install the new one */
    pk_free(m1->pkt);
    m1->pkt = pPacket;
    m1->m_len = pPacket->NumBytes;
    m1->m_next = NULL;
    m1->m_data = pPacket->pData;
    m1->m_len = total;

    //
    // Sanity check (debug build only)
    //
    if ((m1->m_data < (m1->pkt->pBuffer + HeaderSize))) {
      IP_PANIC("ip_output: overflow");
    }
    break;
  } while (1);

  pPacket = m1->pkt;
  /* fill in dest host for IP layer */
  bip = (struct ip *)m1->m_data;
  pPacket->fhost = bip->ip_dest;

  if (pNet->Caps & IP_NI_CAPS_WRITE_TCP_CHKSUM) {
    //
    // Driver will calculate checksum
    //
    Checksum = 0;
  } else {
    //
    // Calculate and fill in TCP Checksum
    //
    bip->ip_ver_ihl = 0x45;
    bip->ip_len = htons(bip->ip_len);   /* make net endian for calculation */
    //tcpp = (struct tcphdr *)ip_data(bip);
    Checksum = IP_TCP_CalcChecksum(bip);
  }
  tcpp = (struct tcphdr *)ip_data(bip);
  tcpp->th_sum = Checksum;

  pPacket->pData = (char*)(bip + 1);    /* point past IP header */
  pPacket->NumBytes = m1->m_len - sizeof(struct ip);
  e = IP_Write(IP_PROT_TCP, pPacket);

  m1->pkt = NULL;          // IP_Write() is now responsable for pPacket, so...
  m_freem(m1);

  if (e < 0) {
    if (e != SEND_DROPPED) {      // Don't report dropped sends, it causes socket applications to bail when a TCP retry will fix the problem
      return e;
    }
  }
  return 0;
}



/* FUNCTION: in_broadcast()
 *
 * Determine if the passed IP address is a braodcast or not.
 * Currently no support for subnet broadcasts.
 *
 * PARAM1: U32 ipaddr
 *
 * RETURNS: TRUE if broadcast, else FALSE
 */

int in_broadcast(U32 ipaddr) {  /* passed in net endian */
  if (ipaddr == 0xffffffff) {
    return 1;
  }
  return 0;
}

/*********************************************************************
*
*       IP_ShowTCP
*
* The next few routines pretty-print stats.
* They support many ancient (but sometimes still usefull) BSD counters.
*
*/
int IP_ShowTCP(void (*pfSendf)(void * pContext, const char * sFormat, ...), void * pContext) {
#if IP_DEBUG > 0
   /* MIB-2 tcp group counters */
   pfSendf(pContext,"tcpRtoAlgorithm %lu,\ttcpRtoMin %lu\n",      tcpmib.tcpRtoAlgorithm, tcpmib.tcpRtoMin);
   pfSendf(pContext,"tcpRtoMax %lu,\ttcpMaxConn %lu\n",           tcpmib.tcpRtoMax, tcpmib.tcpMaxConn);
   pfSendf(pContext,"tcpActiveOpens %lu,\ttcpPassiveOpens %lu\n", tcpmib.tcpActiveOpens, tcpmib.tcpPassiveOpens);
   pfSendf(pContext,"tcpAttemptFails %lu,\ttcpEstabResets %lu\n", tcpmib.tcpAttemptFails, tcpmib.tcpEstabResets);
   pfSendf(pContext,"tcpCurrEstab %lu,\ttcpInSegs %lu\n",         tcpmib.tcpCurrEstab, tcpmib.tcpInSegs);
   pfSendf(pContext,"tcpOutSegs %lu,\ttcpRetransSegs %lu\n",      tcpmib.tcpOutSegs, tcpmib.tcpRetransSegs);
   pfSendf(pContext,"tcpInErrs %lu,\ttcpOutRsts %lu\n",           tcpmib.tcpInErrs, tcpmib.tcpOutRsts);
#else
  pfSendf(pContext,"TCP statistics not available in release build.");
#endif
   return 0;
}



/*********************************************************************
*
*        IP_ShowBSDConn
*
* BSD connection statistics
*/
int IP_ShowBSDConn(void (*pfSendf)(void * pContext, const char * sFormat, ...), void * pContext) {
#if IP_DEBUG > 0
   pfSendf(pContext,"connections initiated: %lu,\tconnections accepted: %lu\n",  IP_TCP_Stat.connattempt, IP_TCP_Stat.accepts);
   pfSendf(pContext,"connections established: %lu,\tconnections dropped: %lu\n", IP_TCP_Stat.connects, IP_TCP_Stat.drops);
   pfSendf(pContext,"embryonic connections dropped: %lu,\tconn. closed (includes drops): %lu\n",  IP_TCP_Stat.conndrops, IP_TCP_Stat.closed);
   pfSendf(pContext,"segs where we tried to get rtt: %lu,\ttimes we succeeded: %lu\n",            IP_TCP_Stat.segstimed, IP_TCP_Stat.rttupdated);
   pfSendf(pContext,"delayed acks sent: %lu,\tconn. dropped in rxmt timeout: %lu\n",              IP_TCP_Stat.delack, IP_TCP_Stat.timeoutdrop);
   pfSendf(pContext,"retransmit timeouts: %lu,\tpersist timeouts: %lu\n",                         IP_TCP_Stat.rexmttimeo, IP_TCP_Stat.persisttimeo);
   pfSendf(pContext,"keepalive timeouts: %lu,\tkeepalive probes sent: %lu\n",                     IP_TCP_Stat.keeptimeo, IP_TCP_Stat.keepprobe);
#ifdef TCP_SACK
   pfSendf(pContext,"sack connections: %lu,\tconns dropped in keepalive: %lu\n",                  IP_TCP_Stat.sackconn, IP_TCP_Stat.keepdrops);
#else
   pfSendf(pContext,"conns dropped in keepalive: %lu\n",       IP_TCP_Stat.keepdrops);
#endif   /* TCP_SACK */
#else
  pfSendf(pContext,"BSD connection statistics not available in release build.");
#endif
   return 0;
}




/* FUNCTION: tcp_bsdsendstat()
 *
 * BSD TCP send stats
 *
 * PARAM1: void * pContext
 *
 * RETURNS: 0
 */

int IP_ShowBSDSend(void (*pfSendf)(void * pContext, const char * sFormat, ...), void * pContext) {
#if IP_DEBUG > 0
  pfSendf(pContext,"total packets sent: %lu,\tdata packets sent: %lu\n",  IP_TCP_Stat.sndtotal, IP_TCP_Stat.sndpack);
  pfSendf(pContext,"data bytes sent: %lu,\tdata packets retransmitted: %lu\n",  IP_TCP_Stat.sndbyte, IP_TCP_Stat.sndrexmitpack);
  pfSendf(pContext,"data bytes retransmitted: %lu,\tack-only packets sent: %lu\n", IP_TCP_Stat.sndrexmitbyte, IP_TCP_Stat.sndacks);
  pfSendf(pContext,"window probes sent: %lu,\tpackets sent with URG only: %lu\n",  IP_TCP_Stat.sndprobe, IP_TCP_Stat.sndurg);
  pfSendf(pContext,"window update-only packets sent: %lu,\tcontrol (SYN|FIN|RST) packets sent: %lu\n",   IP_TCP_Stat.sndwinup, IP_TCP_Stat.sndctrl);
#ifdef TCP_SACK
  pfSendf(pContext,"sack hdrs sent: %lu,\tsack based data resends: %lu\n",   IP_TCP_Stat.sacksent, IP_TCP_Stat.sackresend);
#endif   /* TCP_SACK */
#else
  pfSendf(pContext,"BSD send statistics not available in release build.");
#endif
  return 0;
}



/* FUNCTION: tcp_bsdrcvstat()
 *
 *  BSD TCP receive stats
 *
 *
 * PARAM1: void * pContext
 *
 * RETURNS: 0
 */

//int tcp_bsdrcvstat(void * pContext) {
int IP_ShowBSDRcv(void (*pfSendf)(void * pContext, const char * sFormat, ...), void * pContext) {
#if IP_DEBUG > 0
  pfSendf(pContext,"total packets received: %lu,\tpackets received in sequence: %lu\n", IP_TCP_Stat.rcvtotal, IP_TCP_Stat.rcvpack);
  pfSendf(pContext,"bytes received in sequence: %lu,\tpackets received with ccksum errs: %lu\n", IP_TCP_Stat.rcvbyte, IP_TCP_Stat.rcvbadsum);
  pfSendf(pContext,"packets received with bad offset: %lu,\tpackets received too short: %lu\n",  IP_TCP_Stat.rcvbadoff, IP_TCP_Stat.rcvshort);
  pfSendf(pContext,"duplicate-only packets received: %lu,\tduplicate-only bytes received: %lu\n",  IP_TCP_Stat.rcvduppack, IP_TCP_Stat.rcvdupbyte);
  pfSendf(pContext,"packets with some duplicate data: %lu,\tdup. bytes in part-dup. packets: %lu\n",  IP_TCP_Stat.rcvpartduppack, IP_TCP_Stat.rcvpartdupbyte);
  pfSendf(pContext,"out-of-order packets received: %lu,\tout-of-order bytes received: %lu\n",   IP_TCP_Stat.rcvoopack, IP_TCP_Stat.rcvoobyte);
  pfSendf(pContext,"packets with data after window: %lu,\tbytes rcvd after window: %lu\n",      IP_TCP_Stat.rcvpackafterwin, IP_TCP_Stat.rcvbyteafterwin);
  pfSendf(pContext,"packets rcvd after close: %lu,\trcvd window probe packets: %lu\n",          IP_TCP_Stat.rcvafterclose, IP_TCP_Stat.rcvwinprobe);
  pfSendf(pContext,"rcvd duplicate acks: %lu,\trcvd acks for unsent data: %lu\n",               IP_TCP_Stat.rcvdupack, IP_TCP_Stat.rcvacktoomuch);
  pfSendf(pContext,"rcvd ack packets: %lu,\tbytes acked by rcvd acks: %lu\n",                   IP_TCP_Stat.rcvackpack, IP_TCP_Stat.rcvackbyte);
  pfSendf(pContext,"rcvd window update packets: %lu\n", IP_TCP_Stat.rcvwinupd);
  pfSendf(pContext,"rcvd predictive header hits; acks:%lu, data:%lu\n",    IP_TCP_Stat.predack, IP_TCP_Stat.preddat);
#ifdef TCP_SACK
  pfSendf(pContext,"sack hdrs recv: %lu,\tmax sack blocks in hdr: %lu\n", IP_TCP_Stat.sackrcvd, IP_TCP_Stat.sackmaxblk);
#endif   /* TCP_SACK */
#else
  pfSendf(pContext,"BSD send statistics not available in release build.");
#endif
  return 0;
}





/* FUNCTION: mbuf_list()
 *
 * PARAM1: void * pContext
 *
 * RETURNS:
 */

int IP_ShowMBufList(void (*pfSendf)(void * pContext, const char * sFormat, ...), void * pContext) {
#if IP_DEBUG > 0
   struct mbuf *  mb;

   pfSendf(pContext,"mbufs in use: \n");
   for (mb = (struct mbuf *)IP_Global.MBufInUseQ.q_head; mb; mb = mb->next) {
     pfSendf(pContext,"type %d, pkt:%lx, data:%lx, len:%d\n",  mb->m_type, mb->pkt, mb->m_data, mb->m_len);
   }
#else
  pfSendf(pContext,"Memory buffer list not available in release build.");
#endif
   return 0;
}

/* FUNCTION: mbuf_stat()
 *
 * PARAM1: void * pContext
 *
 * RETURNS: 0
 */

int IP_ShowMBuf(void (*pfSendf)(void * pContext, const char * sFormat, ...), void * pContext) {
#if IP_DEBUG
  #if IP_DEBUG_Q
    pfSendf(pContext,"mfreeq: head:%p, tail:%p, len:%d, min:%d, max:%d\n",  (void*)IP_Global.MBufFreeQ.q_head, (void*)IP_Global.MBufFreeQ.q_tail,  IP_Global.MBufFreeQ.q_len, IP_Global.MBufFreeQ.q_min, IP_Global.MBufFreeQ.q_max);
    pfSendf(pContext,"IP_Global.mbufq: head:%p, tail:%p, len:%d, min:%d, max:%d\n",   (void*)IP_Global.MBufInUseQ.q_head, (void*)IP_Global.MBufInUseQ.q_tail, IP_Global.MBufInUseQ.q_len, IP_Global.MBufInUseQ.q_min, IP_Global.MBufInUseQ.q_max);
  #endif
  pfSendf(pContext,"mbuf allocs: %lu, frees: %lu\n", mbstat.allocs, mbstat.frees);
  pfSendf(pContext,"m_copy copies: %lu, copied bytes: %lu\n",  IP_TCP_Stat.mcopies, IP_TCP_Stat.mcopiedbytes);
  pfSendf(pContext,"m_copy clones: %lu, cloned bytes: %lu\n",  IP_TCP_Stat.mclones, IP_TCP_Stat.mclonedbytes);
  pfSendf(pContext,"ip_output appends: %lu, prepends: %lu, copies: %lu\n",  IP_TCP_Stat.oappends, IP_TCP_Stat.oprepends,  IP_TCP_Stat.ocopies);
#else
  pfSendf(pContext,"Memory buffer statistics not available in release build.");
#endif
  return 0;
}




/* FUNCTION: sock_list()
 *
 * Display information about all the existing TCP and UDP sockets.
 *
 * PARAM1: void * pContext
 *
 * RETURNS:  0 if successfull, else error code.
 */

int IP_ShowSocketList(void (*pfSendf)(void * pContext, const char * sFormat, ...), void * pContext) {
#if IP_DEBUG > 0
   struct inpcb * inp;
   struct socket *   so;
   struct tcpcb * tp;

   if (IP_tcb.inp_next == NULL) {
      pfSendf(pContext,"No TCP sockets\n");
      return 0;
   }

   pfSendf(pContext, "TCP sock, fhost,     ports,    opts, rxbytes, txbytes, snd_una, snd_nxt, state:\n");
   for (inp = IP_tcb.inp_next; inp != &IP_tcb; inp = inp->inp_next) {
      tp = inp->inp_ppcb;
      so = inp->inp_socket;
      if (!so) {
         pfSendf(pContext,"No socket\n");
         continue;
      }
      if(so->so_domain == AF_INET) {
        pfSendf(pContext,"%lx,  %i", so, inp->inp_faddr.s_addr);
      }
      pfSendf(pContext,", %u->%u, ", htons(inp->inp_lport), htons(inp->inp_fport));
      pfSendf(pContext,"0x%x, %u, %u, %ld, %ld, ", (unsigned)so->so_options, (unsigned)so->so_rcv.NumBytes,(unsigned)so->so_snd.NumBytes, tp->snd_una, tp->snd_nxt);
      pfSendf(pContext, "%s\n", IP_INFO_ConnectionState2String(tp->t_state));
   }
#endif
   return IP_ShowUDPSockets(pfSendf, pContext);
}



/*********************************************************************
*
*       so_icmpdu()
*
* Called from NetPort UDP layer whenever it gets a
* ICMP destination unreachable packet (and is linked with this
* sockets layer) Tries to find the offending socket, notify caller,
* and shut it down.
*
*/
void so_icmpdu(IP_PACKET * p, struct destun * pdp);
void so_icmpdu(IP_PACKET * p, struct destun * pdp) {
  ip_addr lhost;    /* IP address of originator (our iface) */
  ip_addr fhost;    /* IP address we sent to */
  U16  fport;   /* TCP/UDP port we sent to */
  U16  lport;   /* TCP/UDP port we sent from */
  struct inpcb * inp;
  struct socket *   so;
  struct tcpcb * tp;

  /* extract information about packet which generated DU */
  fhost = htonl(pdp->dip.ip_dest);
  lhost = htonl(pdp->dip.ip_src);
  lport = htons(*(U16*)(&pdp->ddata[0]));
  fport = htons(*(U16*)(&pdp->ddata[2]));

#ifndef IP_PMTU
   /* if it's a datagram-too-big message, ignore it -- As the
    * build isn't using PMTU Discovery this packet is most
    * probably a Denial of Service Attack.
    */
    if(pdp->dcode == IP_ICMP_DU_FRAG) {
       goto done;
    }
#endif   /* IP_PMTU */

   /* if it's a TCP connection, clean it up */
   if (pdp->dip.ip_prot == TCPTP) {
      /* find associated data structs and socket */
      inp = IP_TCP_PCB_lookup(&IP_tcb, fhost, fport, lhost, lport, INPLOOKUP_WILDCARD);
      if (inp == 0)
         goto done;
      so = inp->inp_socket;
      if (so == 0) {
        goto done;
      }
      tp = inp->inp_ppcb;
      if (tp) {
         if (tp->t_state <= TCPS_LISTEN) {
            goto done;
         }

#ifdef ICMP_TCP_DOS
         {
           struct ip * pip;
           struct tcpiphdr * ti;

           pip = ip_head(p);  /* find IP header */
           ti = (struct tcpiphdr *)p->pData;

           if (!((tp->snd_una <=  ti->ti_seq) && (ti->ti_seq <= tp->snd_nxt))) {
              goto done;
           }

           /* If we get an ICMP Type 3 (Destination Unreachable) - Code 2
            * (Protocol Unreachable) message and during the life of a TCP
            * connection, then its most probably a Denial of Service Attack.
            * As the only other interpretation would be that the support for
            * the transport protocol has been removed from the host sending
            * the error message during the life of the corresponding
            * connection. As in common practice this is higly unlikely in most
            * cases, we will treat this message as a DOS attack.
            */
           if (pdp->dcode == DSTPROT) {
             if ((tp->t_state >= TCPS_ESTABLISHED) && (tp->t_state <= TCPS_TIME_WAIT)) {
                 goto done;
             }
           }

          /* Note some ICMP error messages generated by intermediate routers,
           * include more than the recommended 64 bits of the IP Data. If the
           * TCP ACK number happens to be present then use it in detecting a
           * Denial of Service attack.
           *
           * This way we can ensure that the TCP Acknowledgement number should
           * correspond to data that have already been acknowledged. This way
           * we can further reduce the possiblity of considering a spoofed ICMP
           * packet by a factor of 2.
           */
          if (pip->ip_len >= 32) {
            if (!(ti->ti_seq <= tp->rcv_nxt)) {
              goto done;
            }
          }
        }
#endif
        tcp_close(tp);
      }
      so->so_error = ECONNREFUSED;  /* set error for socket owner */
   }
#ifdef UDP_SOCKETS   /* this sockets layer supports UDP too */
   else if(pdp->dip.ip_prot == IP_PROT_UDP) {
      UDPCONN tmp;
      /* search udp table (which keeps hosts in net endian) */
      for (tmp = firstudp; tmp; tmp = tmp->u_next)
         if ((tmp->u_fport == fport || tmp->u_fport == 0) &&
             (tmp->u_fhost == htonl(fhost)) &&
             (tmp->u_lport == lport))
         {
            break;   /* found our UDP table entry */
         }
      if (!tmp)
         goto done;
      so = (struct socket *)tmp->u_data;
      /* May be non-socket (lightweight) UDP connection. */
      if (so->so_type != SOCK_DGRAM)
         goto done;
      so->so_error = ECONNREFUSED;  /* set error for socket owner */
      /* do a select() notify on socket here */
      sorwakeup(so);
      sowwakeup(so);
   }
#endif   /* UDP_SOCKETS */
   else {
      goto done;
   }

done:
   pk_free(p); /* done with original packet */
   return;
}

/*********************************************************************
*
*       IP_TCP_Set2MSLDelay
*/
void IP_TCP_Set2MSLDelay(unsigned v) {
  IP_TCP_Msl = v;
}



/*********************************************************************
*
*       IP_INFO_GetConnectionList()
*
*  Function description
*    Retrieves a list of connection handles, one per connection.
*    The connetion handle are typically used to obtain detailed information using IP_INFO_GetConnectionInfo()
*
*  Return value
*    Number of connections.
*/
int IP_INFO_GetConnectionList(IP_CONNECTION_HANDLE *pDest, int MaxItems) {
  struct inpcb * inp;
  int i;

  LOCK_NET();
  i = 0;
  for (inp = IP_tcb.inp_next; inp != &IP_tcb; inp = inp->inp_next) {
    if (i < MaxItems) {
      *pDest++ = inp;
    }
    i++;
  }
  UNLOCK_NET();
  return i;
}


/*********************************************************************
*
*       IP_INFO_GetConnectionInfo()
*
*  Function description
*    Retrieves the connection information for a connection handle.
*    The connetion handle is typically obtained via a call to IP_INFO_GetConnectionList()
*
*  Return value
*    0    OK, Information retrieved
*    1    Error, typically because the connection pointer is no longer in list
*/
int IP_INFO_GetConnectionInfo(IP_CONNECTION_HANDLE h, IP_CONNECTION * p) {
  struct inpcb * inp;
  struct tcpcb * pTCPCB;

  int r;

  r = 1;        // Error if not found below
  IP_MEMSET(p, 0, sizeof(IP_CONNECTION));
  LOCK_NET();
  //
  // Check if handle is still valid
  //
  for (inp = IP_tcb.inp_next; inp != &IP_tcb; inp = inp->inp_next) {
    if ((void*)inp == h) {
      pTCPCB = inp->inp_ppcb;
      p->pSock       = inp->inp_socket;
      p->hSock       = inp->inp_socket->Handle;
      p->ForeignAddr = htonl(inp->inp_faddr.s_addr);
      p->ForeignPort = htons(inp->inp_fport);
      p->LocalAddr   = htonl(inp->inp_laddr.s_addr);
      p->LocalPort   = htons(inp->inp_lport);
      if (pTCPCB) {
        p->Type            = IP_CONNECTION_TYPE_TCP;
        p->TcpState        = pTCPCB->t_state;
        p->TcpMss          = pTCPCB->Mss;
        p->TcpMtu          = pTCPCB->Mss + pTCPCB->OptLen + 40;
        p->TcpRetransDelay = pTCPCB->RetransDelay;
        p->TcpIdleTime     = pTCPCB->IdleCnt * IP_TCP_SLOW_PERIOD;
        p->RxWindowMax     = pTCPCB->rcv_wnd;
        p->RxWindowCur     = pTCPCB->rcv_adv - pTCPCB->rcv_nxt;
        p->TxWindow        = pTCPCB->snd_wnd;
      }

      r = 0;                      // O.K.
      break;
    }
  }
  UNLOCK_NET();
  return r;
}

/*********************************************************************
*
*       IP_INFO_ConnectionType2String()
*
*  Function description
*    Returns a descriptive string for the connection type
*/
const char * IP_INFO_ConnectionType2String(U8 Type) {
  const char * s;

  switch (Type) {
  case IP_CONNECTION_TYPE_TCP:
    s = "TCP";
    break;
  default:
    s = "---";
  }
  return s;
}

/*********************************************************************
*
*       IP_INFO_ConnectionState2String()
*
*  Function description
*    Returns a descriptive string for the connection state
*/
const char * IP_INFO_ConnectionState2String(U8 State) {
  const char * s;

  switch (State) {
  case TCPS_CLOSED:       s = "Closed";       break;
  case TCPS_LISTEN:       s = "Listen";       break;
  case TCPS_SYN_SENT:     s = "SYN sent";     break;
  case TCPS_SYN_RECEIVED: s = "SYN received"; break;
  case TCPS_ESTABLISHED:  s = "Established";  break;
  case TCPS_CLOSE_WAIT:   s = "Close wait";   break;
  case TCPS_FIN_WAIT_1:   s = "Fin wait 1";   break;
  case TCPS_CLOSING:      s = "Closing";      break;
  case TCPS_LAST_ACK:     s = "Last ACK";     break;
  case TCPS_FIN_WAIT_2:   s = "FIN wait 2";   break;
  case TCPS_TIME_WAIT:    s = "Time wait";    break;
  default:
    s = "---";
  }
  return s;
}

/*************************** End of file ****************************/

