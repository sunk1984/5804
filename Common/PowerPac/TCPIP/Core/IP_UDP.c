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
File    : IP_UDP.c
Purpose :
--------  END-OF-HEADER  ---------------------------------------------
*/

/* Additional Copyrights: */
/* Copyright 1997- 2000 By InterNiche Technologies Inc. All rights reserved */
/* Portions Copyright 1990, 1993 by NetPort Software. */
/* Portions Copyright 1986 by Carnegie Mellon  */
/* Portions Copyright 1984 by the Massachusetts Institute of Technology  */


#include "IP_Int.h"
#include "IP_Q.h"
#include "IP_ICMP.h"

/* Some reserved UDP ports */
#define  RIP_PORT    520
#define  DNS_PORT    53
#define  SNMP_PORT   161


struct ph {        /* pseudo header for checksumming */
  ip_addr  ph_src;  /* source address */
  ip_addr  ph_dest; /* dest address */
  char     ph_zero; /* zero (reserved) */
  char     ph_prot; /* protocol */
  U16  ph_len;  /* udp length */
};

QUEUE IP_FreeUDPQ;
U8 IP_UDP_RxChecksumEnable = 1;
U8 IP_UDP_TxChecksumEnable = 1;

/*********************************************************************
*
*       _TryAlloc
*
*  Function description
*    Allocate storage space for a UDB connnect block.
*    It is either fetched from the free queue or allocated.
*/
static struct udp_conn * _TryAlloc(void) {
  return (struct udp_conn*) IP_TryAllocWithQ(&IP_FreeUDPQ, sizeof(struct udp_conn));
}

/*********************************************************************
*
*       _Free
*
*  Function description
*    Frees storage space for a TCPIPHDR by adding it to the free queue.
*/
static void _Free(struct udp_conn * p) {
  IP_Q_Add(&IP_FreeUDPQ, p);
}

/*********************************************************************
*
*       udpswap()
*
*  Function description
*    Swap the fields in a udp header for transmission on the net
*    Little endian only, no action taken for BE targets (typically routine is optimized away)
*/
static void udpswap(struct udp *pup) {
  pup->ud_srcp  = htons(pup->ud_srcp);
  pup->ud_dstp  = htons(pup->ud_dstp);
  pup->ud_len   = htons(pup->ud_len);
  pup->ud_cksum = htons(pup->ud_cksum);
}


/*********************************************************************
*
*       IP_UDP_GetLPort()
*
*  Function description
*    Extracts local port information from a UDP packet.
*/
U16 IP_UDP_GetLPort(const IP_PACKET *pPacket) {
  struct udp *   pup;  /* UDP header of this layer */
  pup = ((struct udp *)pPacket->pData) - 1;
  return pup->ud_dstp;
}

/*********************************************************************
*
*       IP_UDP_GetDataPtr()
*
*  Function description
*    Returns pointer to data contained in the received UDP packet
*/
void * IP_UDP_GetDataPtr(const IP_PACKET *pPacket) {
  return pPacket->pData;
}

/*********************************************************************
*
*       IP_UDP_GetFAddr()
*
*  Function description
*    Retrieves the IP-addr of the sender of the given UDP Packet.
*/
void IP_UDP_GetSrcAddr(const IP_PACKET *pPacket, void * pSrcAddr, int AddrLen) {
  IP_UDP_HEADER * pUDP;
  IP_IP_HEADER  * pIP;

  pUDP = ((IP_UDP_HEADER *)pPacket->pData) - 1;
  pIP =  ((IP_IP_HEADER  *)pUDP) - 1;
  *(U32*)pSrcAddr = pIP->SrcAddr;
}



/*********************************************************************
*
*       IP_UDP_OnRx()
*
* This routine handles incoming UDP packets. They're handed to it by
* the internet layer. It demultiplexes the incoming packet based on
* the local port and upcalls the appropriate routine.
*
* RETURNS: 0 if OK or ENP error code
*/
int IP_UDP_OnRx(IP_PACKET * pPacket) {
  struct ip * pip;  /* IP header below this layer */
  struct udp *   pup;  /* UDP header of this layer */
  struct ph   php;     /* pseudo header for checksumming */
  UDPCONN con;      /* UDP connection for table lookup */
  unsigned short osum, xsum; /* scratch checksum holders */
  unsigned plen; /* packet length */
  int   e;    /* general error holder */

  //
  // Verify packet len
  //
  pip = ip_head(pPacket);       /* we'll need IP header info */
  pup = (struct udp*)ip_data(pip);    /*  also need UDP header */
  plen = htons(pup->ud_len);

  if (plen > pPacket->NumBytes) {
    IP_WARN((IP_MTYPE_UDP_IN, "UDP: bad len pkt: rcvd: %u, hdr: %u.", pPacket->NumBytes, plen + sizeof(struct  udp)));
    IP_STAT_INC(udp_mib.udpInErrors);
    pk_free(pPacket);
    return IP_ERR_BAD_HEADER;
  }
  //
  // Verify checksum
  //
  if ((pPacket->pNet->Caps & IP_NI_CAPS_CHECK_UDP_CHKSUM) == 0) {
    if (IP_UDP_RxChecksumEnable) {
      osum = pup->ud_cksum;
      //
      // Did other guy use checksumming ?
      //
      if (osum) {
        php.ph_src = pPacket->fhost;
        php.ph_dest = pip->ip_dest;
        php.ph_zero = 0;
        php.ph_prot = IP_PROT_UDP;
        php.ph_len  = pup->ud_len;
        pup->ud_cksum = (U16)IP_CKSUM(&php, sizeof(struct ph)>>1);
        xsum = (U16)~IP_CalcChecksum_Byte(pup, plen);
        if (!xsum) {
          xsum = 0xffff;
        }
        pup->ud_cksum = osum;
        if (xsum != osum) {
          IP_WARN((IP_MTYPE_UDP_IN, "UDP: bad xsum %04x right %04x from %i",  osum, xsum, pPacket->fhost));
          IP_STAT_INC(udp_mib.udpInErrors);
          pk_free(pPacket);
          return IP_ERR_BAD_HEADER;
        }
      }
    }
  }
  //
  // Packet has been verified.
  // Prepare packet for further processing by adjusting prot fields by size of IP and UDP headers.
  //
  e = (sizeof(struct udp) + sizeof(struct ip));
  pPacket->NumBytes -= e;
  pPacket->pData += e;
  udpswap(pup);
  IP_LOG((IP_MTYPE_UDP_IN, "UDP: pkt[%u] from %i:%d to %d", plen, pPacket->fhost, pup->ud_srcp, pup->ud_dstp));
  //
  // Check for special cases - we have a built-in snmp agent.
  // We do SNMP before trying the demux table so
  // external application programs can examine SNMP packets that
  //
#ifdef INCLUDE_SNMPV3      /* If V3 is present, call SNMPV3 upcall */
  if (pup->ud_dstp == SNMP_PORT) {
    IP_STAT_INC(udp_mib.udpInDatagrams);
    UNLOCK_NET();
    e = v3_udp_recv(pPacket, pup->ud_srcp);      /* upcall imbedded snmp agent */
    LOCK_NET();
    if (e != IP_ERR_NOT_MINE) {
      return(e);
    }
    /* else SNMP pkt was not for imbedded agent, fall to try demux table */
  }
#else                   /* Else call V1's upcall, if V1 is present */
#ifdef PREBIND_AGENT
  if (pup->ud_dstp == SNMP_PORT) {
    udp_mib.udpInDatagrams++;
    UNLOCK_NET();
    e = snmp_upc(pPacket, pup->ud_srcp);      /* upcall imbedded snmp agent */
    LOCK_NET();
    if (e != IP_ERR_NOT_MINE) {
      return(e);
    }
    /* else SNMP pkt was not for imbedded agent, fall to try demux table */
  }
#endif   /* PREBIND_AGENT */
#endif   /* INCLUDE_SNMPV3 */

  //
  // Run through the demux table and try to upcall it
  //
  for (con = firstudp; con; con = con->u_next) {
    //
    // Enforce all three aspects of tuple matching. Old code
    // assumed lport was unique, which is not always so.
    //
    if (con->u_lport && (con->u_lport != pup->ud_dstp)) {
      continue;
    }
    if (con->u_fport && (con->u_fport != pup->ud_srcp)) {
      continue;
    }
    if (con->u_fhost && (con->u_fhost != pPacket->fhost)) {
      continue;
    }
    //
    // If this endpoint has been bound to a local interface address,
    // make sure the packet was received on that interface address
    //
    if (con->u_lhost && (con->u_lhost != pPacket->pNet->n_ipaddr)) {
      continue;
    }

    //
    // Fall to here if we found it
    //
    if ((void*)con->u_rcv == NULL) {        /* if upcall address is set... */
      IP_PANIC("Invalid callback");
    }
    IP_STAT_INC(udp_mib.udpInDatagrams);
    UNLOCK_NET();
    e = ((*con->u_rcv)(pPacket, con->u_data));   /* upcall it */
    LOCK_NET();
    //
    // If error occurred in upcall or there was no upcall hander
    // its up to this routine to free the packet buffer
    //
    if (e < 0) {
      IP_STAT_INC(udp_mib.udpInErrors);
    }
    if (e != IP_OK_KEEP_PACKET) {
      pk_free(pPacket);
    }
    return e;
  }
  //
  // Fall to here if packet is not for us. Check if the packet was
  // sent to an ip broadcast address. If it was, don't send a
  // destination unreachable.
  //
  if ((pip->ip_dest == 0xffffffffL) ||      /* Physical cable broadcast addr*/
     (pip->ip_dest == pPacket->pNet->n_netbr))    /* subnet broadcast */
  {
   IP_LOG((IP_MTYPE_UDP_IN, "UDP: Ignoring ip broadcast"));
   IP_STAT_INC(udp_mib.udpInErrors);
   pk_free(pPacket);
   return IP_ERR_NOT_MINE;
  }

  IP_LOG((IP_MTYPE_UDP_IN, "UDP: Unexpected port 0x%04x", pup->ud_dstp));
  /* send destination unreachable.  Swap back all the swapped information */
  /* so that the destun packet format is correct */
  udpswap(pup);
  icmp_destun(pPacket->fhost, ip_head(pPacket), IP_ICMP_DU_PORT, pPacket->pNet);
  IP_STAT_INC(udp_mib.udpNoPorts);
  pk_free(pPacket);
  return IP_ERR_NOT_MINE;
}



/*********************************************************************
*
*       IP_UDP_Send()
*
*  Function description
*    Prepend a udp header on a packet, checksum it and
*    pass it to IP for sending. IP_PACKET * p should have fhost set to
*    target IP address, pData pointing to udpdata, & NumBytes set
*    before call. If you expect to get any response to this packet you
*    should have opened a UDPCONN with udp_open() prior to calling this.
*
*  Return value
*    Returns 0 if sent OK, else non-zero error code if error detected.
*
*  Add. information
*    (1) Freeing the packet
*    If no error is reported, the packet is freed by the stack.
*    In case of error, this is repsonsibility of the application.
*/
int IP_UDP_Send(int IFace, IP_ADDR FHost, U16 fport, U16 lport, IP_PACKET * p) {
  struct udp* pup;
  struct ph   php;
  int         udplen;
  int         e;

  IP_LOG((IP_MTYPE_UDP_OUT, "UDP: pkt [%u] %04x -> %i:%04x", p->NumBytes, lport, p->fhost, fport));
  LOCK_NET();

  p->fhost = FHost;
  p->pNet   = &IP_aIFace[IFace];

  /* prepend UDP header to upper layer's data */
  p->pData -= sizeof(struct udp);
  pup = (struct udp*)p->pData;
  udplen = p->NumBytes + sizeof(struct udp);
  p->NumBytes = udplen;

  pup->ud_len = (U16)udplen;   /* fill in the UDP header */
  pup->ud_srcp = lport;
  pup->ud_dstp = fport;
  udpswap(pup);

  if (((p->pNet->Caps & IP_NI_CAPS_WRITE_UDP_CHKSUM) == 0) && IP_UDP_TxChecksumEnable) {
    php.ph_src = IP_aIFace[0].n_ipaddr;
    php.ph_dest = p->fhost;
    php.ph_zero = 0;
    php.ph_prot = IP_PROT_UDP;
    php.ph_len = pup->ud_len;
    pup->ud_cksum = IP_CKSUM(&php, sizeof(struct ph)>>1);
    pup->ud_cksum = ~IP_CalcChecksum_Byte(pup, udplen);
    if (pup->ud_cksum == 0) {
      pup->ud_cksum = 0xffff;
    }
  } else {
    pup->ud_cksum = 0;
  }
  IP_STAT_INC(udp_mib.udpOutDatagrams);
  p->NumBytes = udplen;       /* pData was adjusted above */
  e = IP_Write(IP_PROT_UDP, p);
  UNLOCK_NET();
  return e;
}

/*********************************************************************
*
*       IP_UDP_SendAndFree()
*
*  Function description
*    Same as IP_UDP_Send(), but always frees the packet, even in case of error.
*
*  Return value
*    Same as IP_UDP_Send()
*/
int IP_UDP_SendAndFree(int IFace, IP_ADDR FHost, U16 fport, U16 lport, IP_PACKET * p) {
  int e;
  e = IP_UDP_Send(IFace, FHost, fport, lport, p);
  if ((e < 0) && (e != IP_ERR_NO_ROUTE)) {   // if e == IP_ERR_NO_ROUTE, packet has been freed by IP_Write()
    IP_UDP_Free(p);
  }
  return e;
}

/*********************************************************************
*
*       IP_UDP_FindFreePort()
*
* Select a port number at random, but avoid reserved
* ports from 0 thru 1024. Also leave range from 1025 thru 1199
* available for explicit application use.
*
* RETURNS:
*   Returns a useable port number in local endian
*/
#define  MINSOCKET   1200
static U16 usocket = 0;   /* next socket to grab */

U16 IP_UDP_FindFreePort(void) {
  UDPCONN tmp;

  if (usocket < MINSOCKET) {
    /* logic for for init and after wraps */
    usocket = (U16)(IP_OS_GetTime32() & 0x7fff);
    if (usocket < MINSOCKET) {
      usocket += MINSOCKET;
    }
  }
  /* scan existing connections, making sure socket isn't in use */
  for (tmp = firstudp; tmp; tmp = tmp->u_next) {
    if (tmp->u_lport == usocket) {
       usocket++;     /* bump socket number */
       tmp = firstudp;   /* restart scan */
       continue;
    }
  }
  return usocket++;
}



/*********************************************************************
*
*       IP_UDP_Alloc()
*
* Returns a pointer to a packet buffer big enough
* for the specified sizes.
*
* RETURNS:  Returns buffer, or NULL in no buffer was available.
*/
IP_PACKET * IP_UDP_Alloc(int datalen) {
  int   len;
  IP_PACKET * p;

  len = datalen + sizeof(struct udp);      // Add size of  UDP header
  len = (len + 1) & ~1;                    // Round to multiple of 2 bytes
  p = IP_PACKET_Alloc(len + IPHSIZ + IP_aIFace[0].n_lnh);
  if (!p) {
    return NULL;
  }

  /* set prot pointers past end of UDP header  */
  len = sizeof(struct ip) + sizeof(struct udp);
  p->pData += len;
  p->NumBytes  = datalen;
  return(p);
}

/*********************************************************************
*
*       IP_UDP_Free()
*
* Frees an allocated packet buffer
*/
void IP_UDP_Free(IP_PACKET * p) {
  pk_free(p);
}

/*********************************************************************
*
*       udp_maxalloc()
*
*  Returns the largest allocation possible
*                  using udp_alloc()
*
* RETURNS: an int indicating the largest allocation possible
*          using udp_alloc(); i.e. if the sum of udp_alloc()
*          arguments datalen + optlen is greater than the
*          returned value, the allocation will fail
*/
int udp_maxalloc(void) {
  return (IP_Global.aBufferConfigSize[1] - (IPHSIZ + IP_aIFace[0].n_lnh));
}




/*********************************************************************
*
*       udp_open()
*
* Create a UDP connection and enter it in the demux table.
*
* RETURNS:
*   Returns a pointer to the connections structure for use a a
*   handle if Success, else returns NULL.
*/
UDPCONN IP_UDP_Open(
   ip_addr  fhost,      /* foreign host, 0L for any */
   U16  fsock,      /* foreign socket, 0 for any */
   U16  lsock,      /* local socket */
   int (*handler)(IP_PACKET *, void*),   /* rcv upcall */
   void *   data)       /* random data, returned on upcalls to aid demuxing */
{
  UDPCONN con;
  UDPCONN ocon;

  IP_LOG((IP_MTYPE_UDP, "UDP: host %i, lsock %u, fsock %u, foo %04x\n", fhost,lsock, fsock, data));

  LOCK_NET();
  ocon = NULL;
  for (con = firstudp; con; con = con->u_next) {
    if (con->u_lport == lsock && con->u_fport == fsock && con->u_lhost == 0 && con->u_fhost == fhost) {
      IP_LOG((IP_MTYPE_UDP, "UDP: Connection already exists."));
      UNLOCK_NET();
      return(NULL);
    }
    ocon = con;       /* remember last con in list */
  }

  con = _TryAlloc();
  if (con == 0) {
    IP_LOG((IP_MTYPE_UDP, "UDP: Couldn't allocate conn storage."));
    UNLOCK_NET();
    return(NULL);
  }

  if (ocon) {  /* ocon is end of list */
    ocon->u_next = con;  /* add new connection to end */
  } else { /* no list, start one */
    firstudp = con;
  }
  con->u_next = 0;
  con->u_lport = lsock;      /* fill in connection info */
  con->u_fport = fsock;
  con->u_lhost = 0;
  con->u_fhost = fhost;
  con->u_rcv   = handler;
  con->u_data  = data;
  con->u_flags = UDPCF_V4;

  UNLOCK_NET();
  return con;
}


/* FUNCTION: udp_close()
 *
 * udp_close(UDPCONN) - close a udp connection - remove the
 * connection from udp's list of connections and deallocate it.
 *
 *
 * PARAM1: UDPCONN con
 *
 * RETURNS: void
 */

void IP_UDP_Close(UDPCONN con) {
  UDPCONN pcon;
  UDPCONN lcon;

#if IP_DEBUG
  if ((con == NULL) || (firstudp == NULL)) {
    IP_WARN((IP_MTYPE_UDP, "UDP: Bad programming"));
    return;
  }
#endif

  LOCK_NET();
  /* find connection in list and unlink it */
  lcon = NULL;   /* clear ptr to last connection */
  for (pcon = firstudp; pcon != con; pcon = pcon->u_next) {
    if (!pcon) {
      IP_WARN((IP_MTYPE_UDP, "UDP: Prog error - connection not in list"));
      UNLOCK_NET();
      return;
    }
    lcon = pcon;   /* remember last connection */
  }


  if (lcon) {   /* in con is not head of list */
    lcon->u_next = con->u_next;   /* unlink */
  } else {
    firstudp = con->u_next; /* remove from head */
  }
  _Free(con);  /* free memory for structure */
  UNLOCK_NET();
}











/*********************************************************************
*
*       IP_ShowUDP
*/
int IP_ShowUDP      (void (*pfSendf)(void * pContext, const char * sFormat, ...), void * pContext) {
#if IP_DEBUG > 0
  pfSendf(pContext,"UDP MIB dump:");
  pfSendf(pContext,"In: Good: %lu    No Port: %lu     Bad: %lu\n",  udp_mib.udpInDatagrams, udp_mib.udpNoPorts, udp_mib.udpInErrors);
  pfSendf(pContext,"Out: %lu\n", udp_mib.udpOutDatagrams);
#else
  pfSendf(pContext,"UDP statistics not available in release build.");
#endif
  return 0;
}

/*********************************************************************
*
*       IP_ShowUDPSockets
*
* Display information about all the existing UDP sockets.
*/
int IP_ShowUDPSockets(void (*pfSendf)(void * pContext, const char * sFormat, ...), void * pContext) {
#if IP_DEBUG > 0
  UDPCONN con;      /* UDP connection for table lookup */

  if (!firstudp) {
    pfSendf(pContext,"No UDP sockets\n");
    return 0;
  }

  pfSendf(pContext, "Here is a list of all UDP sockets (normal/lightweight).\n");
  pfSendf(pContext, "UDP sock, hosts          ,     ports,    IPv6 addrs");

  for (con = firstudp; con; con = con->u_next) {
    if (con->u_flags & UDPCF_V4) {
      pfSendf(pContext,"\n%lx,  %i", con, con->u_fhost);
      pfSendf(pContext,"->%i ", con->u_lhost);
    } else {
      pfSendf(pContext,"\n%lx,  - (v6 socket) -  ", con);
    }
    pfSendf(pContext,", %u->%u, ", con->u_lport, con->u_fport);
  }
  pfSendf(pContext,"\n");
#else
  pfSendf(pContext,"UDP socket statistics not available in release build.");
#endif
  return 0;
}

/*********************************************************************
*
*       IP_UDP_EnableRxChecksum
*
*  Function description
*    Enables checksumming for incoming UDP packets.
*/
void IP_UDP_EnableRxChecksum(void) {
 IP_UDP_RxChecksumEnable = 1;
}

/*********************************************************************
*
*       IP_UDP_DisableRxChecksum
*
*  Function description
*    Disables checksumming for incoming UDP packets.
*/
void IP_UDP_DisableRxChecksum(void) {
 IP_UDP_RxChecksumEnable = 0;
}

/*********************************************************************
*
*       IP_UDP_EnableTxChecksum
*
*  Function description
*    Enables checksumming for incoming UDP packets.
*/
void IP_UDP_EnableTxChecksum(void) {
 IP_UDP_RxChecksumEnable = 1;
}

/*********************************************************************
*
*       IP_UDP_DisableTxChecksum
*
*  Function description
*    Disables checksumming for incoming UDP packets.
*    This is permitted acc. to RFC 768.
*/
void IP_UDP_DisableTxChecksum(void) {
 IP_UDP_RxChecksumEnable = 0;
}

/*************************** End of file ****************************/


