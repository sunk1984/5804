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
File    : IP_IP.c
Purpose : Contains the base IP code.
--------  END-OF-HEADER  ---------------------------------------------
*/

/* Additional Copyrights: */
/* Copyright 1997- 2000 By InterNiche Technologies Inc. All rights reserved */
/* Portions Copyright 1990 by NetPort Software. */
/* Portions Copyright 1986 by Carnegie Mellon  */
/* Portions Copyright 1984 by the Massachusetts Institute of Technology  */

#include "IP_Int.h"
#include "IP_ICMP.h"


/*********************************************************************
*
*       Public data
*
**********************************************************************
*/


int ipf_filter(IP_PACKET * p, int dir);

/*********************************************************************
*
*       Public code
*
**********************************************************************
*/

/********************************************************************
*
*       IP_Write
*
* Function description
*   Fill in the internet header in the packet p and send
*   the packet through the appropriate net interface. This will
*   involve using routing. Call p->fhost set to target IP address,
*   and with IP_PACKET * p->NumBytes & p->pData fields set to start and
*   length of upper layer, ie UDP. This is in contrast to the IP->UDP
*   upcall, which leaves pData at the IP header.
*
* Parameters
*   prot: U8 prot
*   p:    IP_PACKET * p


                       !PP          Eth     !  IP               !  Upper layer
                       !--------------------!-------------------!--------------------------
                       !          16             20             !
                                                                ^pData

*
* Return value
*   If sent O.K.:       0
*   If waiting for ARP: IP_ERR_SEND_PENDING (1)
*   On Error:           Negative error code
*/
int IP_Write(U8 prot, IP_PACKET * pPacket) {
  ip_addr firsthop;
  struct ip * pip;
  U32         NetMask;
  U32         FHost;
  U32         LHost;
  NET       * pNet;            // Interface for which this entry is valid
  U32         v;

  pNet = &IP_aIFace[0];
  IP_STAT_INC(ip_mib.ipOutRequests);
  //
  // Prepend IP header to packet data
  //
  pPacket->pData    -= sizeof(struct ip);       /* this assumes no send options! */
  pPacket->NumBytes += sizeof(struct ip);
  pPacket->pNet      = pNet;     /* set send net for compatability w/drivers */
  pip = (struct ip*)pPacket->pData;
  pip->ip_src        = pNet->n_ipaddr;
  pip->ip_prot = prot;          // Install protocol ID (TCP, UDP, etc)
  pip->ip_ver_ihl = 0x45;       /* 2 nibbles; VER:4, IHL:5. */
  pip->ip_flgs_foff = 0;        /* fragment flags and offset */
  pip->ip_time = IP_Global.TTL;
  v = IP_Global.PacketId++;
  pip->ip_id   = htons(v);      /* IP datagram ID */
  pip->ip_dest = pPacket->fhost;
  pip->ip_len  = htons(pPacket->NumBytes);
  pip->ip_tos  = 0;
  //
  // Compute IP header checksum.
  // Note that we always use software to compute the checksum since the header consists of only 10 half words.
  // The overhead to use the hardware to do this is usually higher, so it does not make
  // sense to use hardware in this case.
  //
  pip->ip_chksum = 0;           // Clear checksum field for summing
  if ((pPacket->pNet->Caps & IP_NI_CAPS_WRITE_IP_CHKSUM) == 0) {
    pip->ip_chksum = (U16)~IP_CKSUM(pip, 10);
  }
  //
  // Lossy I/O:
  // Allow dropping a certain number of packets to test stability of the stack and certain sub-components,
  // as well as performance drop in such an environment.
  //
#if IP_DEBUG >= 0
  if (IP_Debug.TxDropRate) {
    if (++IP_Debug.TxDropCnt >= IP_Debug.TxDropRate) {
      pk_free(pPacket);                 /* punt packet */
      IP_Debug.TxDropCnt = 0;
      return 0;                     /* act as if we sent OK */
    }
  }
#endif
#if (defined (IP_TRIAL))
  if (IP_OS_GetTime32() > 15 * 60 * 1000) {
    pk_free(pPacket);                 /* punt packet */
    return 0;
  }
#endif
#if IP_DEBUG > 0
  if (IP_Debug.PacketDelay) {
    IP_OS_Delay(IP_Debug.PacketDelay);
  }
#endif
  //
  // Decide if firsthop is local host or default gateway
  //
  NetMask = IP_aIFace[0].snmask;
  FHost   = pPacket->fhost;
  LHost   = IP_aIFace[0].n_ipaddr;
  if (((LHost ^ FHost) & NetMask) == 0) {      // FHost is in local subnet ?
    if ((FHost & ~NetMask) == ~NetMask) {
      return IP_ETH_SendBroadcast(pPacket);
    }
    firsthop = FHost;                         // We can send directly to peer.
  } else if (FHost == 0xFFFFFFFF) {                   // Is it a broadcast for this net ? e.g.
    return IP_ETH_SendBroadcast(pPacket);
  } else if ((*(U8*)&pPacket->fhost & 0xF0) == 0xE0) {                  // Multicast ?
    return IP_ETH_SendMulticast(pPacket, pPacket->fhost);
  } else if (IP_aIFace[0].n_defgw) {                              // Do we have a default gateway ?
    firsthop = IP_aIFace[0].n_defgw;
  } else {
    IP_STAT_INC(ip_mib.ipOutNoRoutes);
    pk_free(pPacket);
    return IP_ERR_NO_ROUTE;
  }
  //
  // Send packet to MAC layer. This will try to resolve MAC layer addressing
  // and send packet. This can return SUCCESS, PENDING, or error codes
  //
  return IP_ARP_Send(pPacket, firsthop);
}


/********************************************************************
*
*       IP_IP_OnRx
*
* Function description
*   This is the IP receive upcall routine.
*   It handles IP packets received by network ISRs.
*   It verifies their Ip headers and does the
*   upcall to the upper layer that should receive the packet.
*
*/
void IP_IP_OnRx(IP_PACKET * p) {
  struct ip * pip;     /* the internet header */
  unsigned short csum; /* packet checksum */
  unsigned short tempsum;
  NET * nt;
  unsigned len;

  IP_LOG((IP_MTYPE_NET_IN, "NET: Received IP packet, len:%d, if:%d", p->NumBytes, if_netnumber(p->pNet)));

  ASSERT_LOCK();
  //
  // Lossy I/O:
  // Allow dropping a certain number of packets to test stability of the stack and certain sub-components,
  // as well as performance drop in such an environment.
  //
#if IP_DEBUG >= 0
  if (IP_Debug.RxDropRate) {
    if (++IP_Debug.RxDropCnt >= IP_Debug.RxDropRate) {
      pk_free(p);                 /* punt packet */
      IP_Debug.RxDropCnt = 0;
      return;
    }
  }
#endif
  nt = p->pNet;      /* which interface it came in on */
  IP_STAT_INC(ip_mib.ipInReceives);
  pip = ip_head(p);

  //
  // Test received MAC len against IP header len
  //
  if (p->NumBytes < (unsigned)htons(pip->ip_len)) {
    IP_WARN((IP_MTYPE_NET_IN, "NET: IP packet too short: (%d vs. %d) --- Discarded", p->NumBytes, (unsigned)htons(pip->ip_len)));
    IP_STAT_INC(ip_mib.ipInHdrErrors);
    pk_free(p);
    return;
  }
  //
  // Use length from IP header; MAC value may be padded
  //
  len = htons(pip->ip_len);
  p->NumBytes = len;       /* fix pkt len */

  if ( ((pip->ip_ver_ihl & 0xf0) >> 4) != IP_VER) {
    IP_WARN((IP_MTYPE_NET_IN, "NET: IP_IP_OnRx: bad version number --- Discarded"));
    IP_STAT_INC(ip_mib.ipInHdrErrors);
    pk_free(p);
    return;
  }

  #ifdef USE_IPFILTER
  /* Do IP filtering. If packet is accepted, ipf_filter() returns
  * SUCCESS. Discard the packet for all other return values
  */
  if (ipf_filter(p,1)) {  /* 1 - inbound pkt */
    pk_free(p);
    IN_PROFILER(PF_IP, PF_EXIT);
    return IP_ERR_NO_ROUTE;
  }
  #endif

  //
  // Compute & check IP header checksum.
  // Note that we always use software to compute the checksum since the header consists of only 10 half words.
  // The overhead to use the hardware to do this is usually higher, so it does not make
  // sense to use hardware in this case.
  //
  if ((p->pNet->Caps & IP_NI_CAPS_CHECK_IP_CHKSUM) == 0) {
    csum = pip->ip_chksum;
    pip->ip_chksum = 0;
    tempsum = (U16)~IP_CKSUM(pip, ip_hlen(pip) >> 1);
    if (csum != tempsum) {
      pip->ip_chksum = csum;
      IP_WARN((IP_MTYPE_NET_IN, "NET: IP_IP_OnRx: bad xsum --- Discarded"));
      IP_STAT_INC(ip_mib.ipInHdrErrors);
      pk_free(p);
      return;
    }
    pip->ip_chksum = csum;
  }
  //
  // Check IP addr.  Needs to be the IP-addr of the interface or a broadcast.
  //
  if ((pip->ip_dest != nt->n_ipaddr) &&  /* Quick check on our own addr */
     (pip->ip_dest != 0xffffffffL) &&   /* Physical cable broadcast addr*/
     (pip->ip_dest != nt->n_netbr) )    /* All subnet broadcast */
  {
    IP_STAT_INC(ip_mib.ipInAddrErrors);
    pk_free(p);
    return;
  }

  p->fhost = pip->ip_src;

  IP_LOG((IP_MTYPE_IP, "IP: Received packet protocol %d from %i", pip->ip_prot, pip->ip_src));

  switch (pip->ip_prot) {
#ifdef INCLUDE_TCP
  case IP_PROT_TCP:
    IP_STAT_INC(ip_mib.ipInDelivers);
    IP_TCP_OnRx(p);
    return;
#endif   /* INCLUDE_TCP */

#ifdef INCLUDE_UDP
  case IP_PROT_UDP:
    IP_STAT_INC(ip_mib.ipInDelivers);
    IP_UDP_OnRx(p);
    return;
#endif   /* INCLUDE_UDP */
#ifdef INCLUDE_ICMP
  case IP_PROT_ICMP:
    IP_STAT_INC(ip_mib.ipInDelivers);
    IP_ICMP_OnRx(p);
    return;
#endif   /* INCLUDE_ICMP */
  default: /* unknown upper protocol */
    break;
  }

  IP_WARN((IP_MTYPE_IP, "IP: Received IP packet of unsupp. type %d from %i", pip->ip_prot, pip->ip_src));
  IP_STAT_INC(ip_mib.ipUnknownProtos);
  pk_free(p);
}

/*********************************************************************
*
*       IP_ShowStat
*/
int IP_ShowStat(void (*pfSendf)(void * pContext, const char * sFormat, ...), void * pContext) {
#if IP_DEBUG > 0
  pfSendf(pContext,"IP MIB statistics:\n");
  pfSendf(pContext,"Gateway: %s     default TTL: %lu\n",                        (ip_mib.ipForwarding == 1) ? "YES" : "NO", ip_mib.ipDefaultTTL);
  pfSendf(pContext,"rcv:  total: %lu    header err: %lu    address err: %lu\n",  ip_mib.ipInReceives,    ip_mib.ipInHdrErrors, ip_mib.ipInAddrErrors);
  pfSendf(pContext,"rcv:  unknown Protocls: %lu    delivered: %lu\n",            ip_mib.ipUnknownProtos, ip_mib.ipInDelivers);
  pfSendf(pContext,"send: total: %lu    discarded: %lu     No routes: %lu\n",    ip_mib.ipOutRequests,   ip_mib.ipOutDiscards, ip_mib.ipOutNoRoutes);
#else
  pfSendf(pContext,"Ip statistics not available in release build.");
#endif
  return 0;
}

/*************************** End of file ****************************/



