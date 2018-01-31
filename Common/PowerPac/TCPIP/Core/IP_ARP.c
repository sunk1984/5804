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
File    : IP_arp.c
Purpose : ARP handling.
          See rfc 1156 for the Address Translation Tables.
Literature
  [1] RFC 826 - Ethernet Address Resolution Protocol: Or Converting Network Protocol Addresses to 48.bit Ethernet Address for Transmission on Ethernet Hardware
--------  END-OF-HEADER  ---------------------------------------------
*/

/* Additional copyrights */
/* Copyright  2000 By InterNiche Technologies Inc. All rights reserved */

#include "IP_Int.h"

/*********************************************************************
*
*       Defines, configurable
*
**********************************************************************
*/
#define  MAXARPS        4  // maximum mumber of arp table entries

/*********************************************************************
*
*       Defines, fixed
*
**********************************************************************
*/


#define ET_DSTOFF       (0)    // ET_DSTOFF - offset of destination address within Ethernet header
#define ET_SRCOFF       (6)    // ET_SRCOFF - offset of source address within Ethernet header
#define ET_TYPEOFF      (12)   // ET_TYPEOFF - offset of Ethernet type within Ethernet header

#define ETH_TYPE_IP    htons(0x0800)  // Ethernet type "IP"
#define ETH_TYPE_ARP   htons(0x0806)  // Ethernet type "ARP"

/*********************************************************************
*
*       Static data, const
*
**********************************************************************
*/
const U8 _abHardwareAddrBroadcast[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

/*********************************************************************
*
*       Statistic counters (used in higher debug levels only)
*
**********************************************************************
*/

int IP_ARP_SendDropCnt;
int IP_ARP_SendCnt;

/*********************************************************************
*
*       Local data types
*
**********************************************************************
*/


/* The ARP table entry structure.
 * It is empty (unused) if t_pro_addr == 0L.
 *
 */

typedef struct {
  U32       t_pro_addr;        // Protocol address
  U8        t_phy_addr[6];     // Physical address
  NET       * pNet;            // Interface for which this entry is valid
  IP_PACKET * pPending;        // Packets waiting for resolution of this arp
  U32       createtime;        // Time entry was created
  U32       lasttime;          // Time entry was last referenced
} ARP;

/* the ARP header as it appears on the wire: */
struct arp_wire   {
   U16         ar_hd;      /* hardware type */
   U16         ar_pro;     /* protcol type */
   char        ar_hln;     /* hardware addr length */
   char        ar_pln;     /* protocol header length */
   U16         ar_op;      /* opcode */
   char        data[20];   /* send IP, send eth, target IP, target eth */
};


/* THE ARP header structure, with special macros around it to help
 * with declaring it "packed".
 */

struct arp_hdr {  /* macro to optionally pack struct */
  U16     ar_hd;      /* hardware type */
  U16     ar_pro;     /* protcol type */
  U8      ar_hln;     /* hardware addr length */
  U8      ar_pln;     /* protocol header length */
  U16     ar_op;      /* opcode */
  U8      ar_sha[6];  /* sender hardware address */
  ip_addr ar_spa;     /* sender protocol address */
  U8      ar_tha[6];  /* target hardware address */
  ip_addr ar_tpa;     /* target protocol address */
};

/* Plummer's internals. All constants are already byte-swapped. */
#define     ARREQ      htons(1)       /* byte swapped request opcode */
#define     ARREP      htons(2)       /* byte swapped reply opcode */

/* offsets to fields in arp_wire->data[] */
#define  AR_SHA   0
#define  AR_SPA   6
#define  AR_THA   10
#define  AR_TPA   16

/*********************************************************************
*
*       Static data
*
**********************************************************************
*/


static I32   arp_ageout  =  600000;        /* APR table refresh age, in seconds */

static ARP _aARP[MAXARPS];           // the actual table

/* arp stats - In addition to MIB-2 */
typedef struct {
  unsigned    ReqsIn;    // requests received
  unsigned    ReqsOut;   // requests sent
  unsigned    RepsIn;    // replys received
  unsigned    RepsOut;   // replys sent
}  IP_ARP_STAT;

IP_ARP_STAT IP_ARP_Stat;


static void (*pfHandleAddrConflict) (int IFace, IP_PACKET * pPacket);  //

/*********************************************************************
*
*       find_oldest_arp
*
* Return LRU or first free entry in arp table - preperatory to
* making a new arp entry out of it. IP address passed is that of new
* entry so we can recycle previous entry for that IP if it exists.
*
* RETURNS: LRU or first free entry in arp table
*/

static ARP * find_oldest_arp(ip_addr dest_ip) {
  ARP *   tp;
  ARP *   oldest;

  /* find lru (or free) entry,  */
  oldest = _aARP;
  for (tp = _aARP; tp <= &_aARP[MAXARPS-1]; tp++)  {
    if (tp->t_pro_addr == dest_ip) {  /* ip addr already has entry */
      tp->lasttime = IP_OS_GetTime32();
      return tp;
    }
    if (!tp->t_pro_addr) {   // entry is unused
       oldest = tp;   /* use free entry as "oldest" */

       /* need to keep scanning in case dest_ip already has an entry */
    } else if (oldest->lasttime > tp->lasttime) {
      oldest = tp;   /* found an older (LRU) entry */
    }
  }
  return oldest;
}

/*********************************************************************
*
*       _FindEntryByIP
*
* Return LRU or first free entry in arp table - preperatory to
* making a new arp entry out of it. IP address passed is that of new
* entry so we can recycle previous entry for that IP if it exists.
*
* RETURNS: LRU or first free entry in arp table
*/
static ARP * _FindEntryByIP(ip_addr dest_ip) {
  ARP * p;
  int i;

#if 0
  p = &_aARP[0];
  for (i = 0; i < MAXARPS; i++)  {
    if (p->t_pro_addr == dest_ip) {  /* we found our entry */
      return p;
    }
    p++;
  }
#else
  p = &_aARP[MAXARPS - 1];
  for (i = 0; i < MAXARPS; i++)  {
    if (p->t_pro_addr == dest_ip) {  /* we found our entry */
      return p;
    }
    p--;
  }
#endif
  return NULL;
}



/*********************************************************************
*
*       make_arp_entry()
*
* Find the first unused (or the oldest) arp table entry.
*
* RETURNS: Returns pointer to arp table entry selected.
*/

static ARP * make_arp_entry(ip_addr dest_ip, NET * net) {
  ARP * pOldest;

  pOldest = find_oldest_arp(dest_ip);       // Find usable (or existing) ARP table entry
  pOldest->createtime = IP_OS_GetTime32();  // Partially fill in arp entry
  //
  // If recycling entry, don't leak packets which may be stuck here
  //
  if (pOldest->pPending) {
    pk_free(pOldest->pPending);
    pOldest->pPending = NULL;
  }

  pOldest->t_pro_addr = dest_ip;
  pOldest->pNet = net;
  IP_MEMSET(pOldest->t_phy_addr, '\0', 6);   /* clear mac address */
  return pOldest;
}


/*********************************************************************
*
*       _SendEthernetPacket()
*
*  Function description
*    Sends an Ethernet packet.
*    Sender's HW address and Ethernet type is filled in.
*
*  Return value:
*     0  if OK
*   !=0  on error (packet can not be sent)
*/
static int _SendEthernetPacket(IP_PACKET * pPacket, U16 Type, const U8 * pDestHWAddr) {
  char * pData;

  //
  // Add 14-byte ethernet header to packet
  //
  pData = pPacket->pData;
  pData -= 14;                     // Ethernet header is 14 bytes
  pPacket->pData = pData;
  pPacket->NumBytes += 14;
  if (pData < pPacket->pBuffer) {  // Sanity check pointer
    IP_PANIC("_SendEthernetPacket: prepend");
  }
  //
  // Fill in type, source & destination addresses
  //
  IP_MEMCPY(pData + ET_DSTOFF, pDestHWAddr, 6);              // MAC Dest
  IP_MEMCPY(pData + ET_SRCOFF,  pPacket->pNet->abHWAddr, 6);  // MAC src
  *(U16*)(pData   + ET_TYPEOFF) = Type;
  return IP__SendPacket(pPacket);   /* send packet to media */
}


/*********************************************************************
*
*       _SendViaARPEntry()
*
*  Function description
*    Fill in outgoing ethernet-bound packet's ethernet
*    header and send it. Header info is in arp entry passed and MIB
*    info.
*
*  Return value:
*     0  if OK
*   !=0  on error (packet can not be sent)
*/
static int _SendViaARPEntry(IP_PACKET * pPacket, ARP * tp) {
  tp->lasttime = IP_OS_GetTime32();
  return _SendEthernetPacket(pPacket, ETH_TYPE_IP, tp->t_phy_addr);
}

/*********************************************************************
*
*       _SendARP()
*
*  Function description
*    Sends an ARP packet. This can be either ARP request or ARP reply.
*
*/
static void _SendARP(IP_PACKET * pPacket, U16 Opcode, U32 * pDestIp, const U8 * pDestHWAddr) {
  char * pData;

  pPacket->NumBytes = 28;             // ARP packet for ethernet is always 42 bytes
  pData = pPacket->pBuffer + 16;      // Leave space for Ethernet header (14 bytes header + 2 bytes padding)
  pPacket->pData = pData;

  //
  // Fill in all fields
  //
  *(U16*)  (pData + 0x00) = htons(1);      // Hardware type: Ethernet in net endian
  *(U16*)  (pData + 0x02) = htons(0x0800); // IP type in net endian
  *        (pData + 0x04) = 6;
  *        (pData + 0x05) = 4;
  *(U16*)  (pData + 0x06) = Opcode;
  IP_MEMCPY(pData + 0x08, pPacket->pNet->abHWAddr, 6);
  IP_MEMCPY(pData + 0x0E, &pPacket->pNet->n_ipaddr, 4);
  IP_MEMCPY(pData + 0x12, pDestHWAddr, 6);
  IP_MEMCPY(pData + 0x18, pDestIp, 4);
  //
  // Send it !
  //
  _SendEthernetPacket(pPacket, ETH_TYPE_ARP, pDestHWAddr);
}

/*********************************************************************
*
*       _SendARPRequest()
*
*  Function description
*    Send an arp for outgoing ethernet packet pPacket, which
*   has no current arp table entry. This means making a partial entry
*   and queuing the packet at the entry. Packet will be send when
*   target IP host reply to the ARP we send herein. If no reply,
*   timeout will eventually free packet.
*
* RETURNS: Returns 0 if OK, or the usual IP_ERR_ errors
*/
static int _SendARPRequest(IP_PACKET * pPacket, ip_addr dest_ip) {
  ARP * oldest;
  IP_PACKET * arppkt;

  /* not broadcasting, so get a packet for an ARP request */
  arppkt = IP_PACKET_Alloc(64);         // Alloc small packet
  if (!arppkt) {
    pk_free(pPacket);
    return IP_ERR_RESOURCE;
  }
  arppkt->pNet = &IP_aIFace[0];
  //
  // Create entry in ARP table
  // Note that we always do this in order to also have an entry in case of a forced ARP Request (by DHCP code)
  //
  oldest = make_arp_entry(dest_ip, &IP_aIFace[0]);
  oldest->pPending = pPacket;           /* packet is "pended", not pk_free()d */
  //
  // Build & send arp request packet
  //
  _SendARP(arppkt, ARREQ, &dest_ip, _abHardwareAddrBroadcast);           // input 'pPacket' will be freed by caller
  IP_STAT_INC(IP_ARP_Stat.ReqsOut);
  return IP_ERR_SEND_PENDING;
}

/*********************************************************************
*
*       _SendARPReply()
*
*  Function description
*    Do arp reply to the passed arp request packet. packet
*    must be freed (or reused) herein.
*
*/
static void _SendARPReply(IP_PACKET * pPacket) {
  IP_PACKET * outpkt;
  struct arp_hdr *  in;

  outpkt = IP_PACKET_Alloc(sizeof(struct arp_hdr));
  if (!outpkt)  {
    IP_WARN((IP_MTYPE_ARP, "Out of mem: Could not send ARP"));
    return;
  }
  outpkt->pNet = pPacket->pNet;    /* send back out the iface it came from */

  in = (struct arp_hdr *)(pPacket->pBuffer + ETHHDR_SIZE);

  /* prepare outgoing arp packet */
  _SendARP(outpkt, ARREP, &in->ar_spa, &in->ar_sha[0]);   // input 'pPacket' will be freed by caller */
  IP_STAT_INC(IP_ARP_Stat.RepsOut);
}

/*********************************************************************
*
*       IP_ARP_OnRx()
*
* Process an incoming arp packet.
*
*  Returns
*   0            if it was for us,
*   IP_ERR_NOT_MINE if the arp packet is not for my IP address, else a negative error code.
*/

int IP_ARP_OnRx(IP_PACKET * pPacket) {
  struct arp_hdr *  arphdr;
  ARP *   tp;

  arphdr = (struct arp_hdr *)pPacket->pData; //(pPacket->pBuffer + ETHHDR_SIZE);

  {
    struct arp_wire * arwp  =  (struct  arp_wire *)arphdr;
    IP_MEMMOVE(&arphdr->ar_tpa, &arwp->data[AR_TPA], 4);
    IP_MEMMOVE(arphdr->ar_tha, &arwp->data[AR_THA], 6);
    IP_MEMMOVE(&arphdr->ar_spa, &arwp->data[AR_SPA], 4);
//    IP_MEMMOVE(arphdr->ar_sha, &arwp->data[AR_SHA], 6);  // Not required since addr. is identical
  }
  //
  // ACD: Check if the IP address of the sender is identical to our IP address.
  //
  if (pfHandleAddrConflict) {
    if (arphdr->ar_spa == pPacket->pNet->n_ipaddr) {
        pfHandleAddrConflict(0, pPacket);
        pk_free(pPacket);            // Address conflict with sender. Handled by ACD module.
        return IP_ERR_ACD;
    }
  }
  /* check ARP's target IP against our net's: */
  if(arphdr->ar_tpa != pPacket->pNet->n_ipaddr) {
    pk_free(pPacket);     /* not for us, dump & ret (proxy here later?) */
    return IP_ERR_NOT_MINE;
  }

  if (arphdr->ar_op == ARREQ) {  /* is it an arp request? */
    IP_STAT_INC(IP_ARP_Stat.ReqsIn);
    _SendARPReply(pPacket);
    pk_free(pPacket);
    return 0;               // Done
  } else {    /* ARP reply, count and fall thru to logic to update table */
    IP_STAT_INC(IP_ARP_Stat.RepsIn);
  }

  /* scan table for matching entry */
  /* check this for default gateway situations later, JB */
  for (tp = _aARP;   tp <= &_aARP[MAXARPS-1]; tp++) {
    if (tp->t_pro_addr == arphdr->ar_spa) {     /* we found IP address, update entry */
      IP_LOG((IP_MTYPE_ARP, "ARP: ARP table entry updated: %i %h", arphdr->ar_spa, &arphdr->ar_sha));
      IP_MEMCPY(tp->t_phy_addr, arphdr->ar_sha, 6);   /* update MAC adddress */
      tp->lasttime = IP_OS_GetTime32();
      if (tp->pPending) {     /* packet waiting for this IP entry? */
        IP_PACKET * outpkt = tp->pPending;
        tp->pPending = NULL;
        _SendViaARPEntry(outpkt, tp);    /* try send again */
      }
      pk_free(pPacket);
      return 0;
    }
  }
  /* fall to here if packet is not in table */
  pk_free(pPacket);
  return IP_ERR_NOT_MINE;
}



/*********************************************************************
*
*       IP_ARP_Send()
*
*  Function description
*    Called when we want to send an IP packet on a medium (i.e. ethernet) which supports ARP.
* Packet is passed, along with target IP address, which may be the packet's dest_ip or a
* gateway/router. We check the ARP cache (and scan arp table if required) for MAC address
* matching the passed dest_ip address. If
* the MAC address is not already known, we broadcast an arp request
* for the missing IP address and attach the packet to the "pPending"
* pointer. The packet will be sent when the ARP reply comes in, or
* freed if we time out. We flush the cache every second to force the
* regular freeing of any "pPending" packets. We flush every entry on
* the ageout interval so bogus ARP addresses won't get permanently
* wedged in the table. This happens when someone replaces a PC's MAC
* adapter but does not change the PC's IP address.
*
* RETURNS: Returns 0 if packet went to mac sender; IP_ERR_SEND_PENDING
* if awaiting arp reply, or SEND_FAILED if error
*/

int IP_ARP_Send(IP_PACKET * pPacket, ip_addr dest_ip) {
  ARP *   p;

  IP_STAT_INC(IP_ARP_SendCnt);
  p = _FindEntryByIP(dest_ip);
  if (p) {
    if (p->pPending) {  /* arp already pPending for this IP? */
      pk_free(pPacket);  /* sorry, we have to dump this one.. */
      IP_STAT_INC(IP_ARP_SendDropCnt);
      return SEND_DROPPED;    /* packet already waiting for this IP entry */
    } else {  /* just send it */
      return _SendViaARPEntry(pPacket, p);
    }
  }
  //
  // Send ARP, queing packet
  //
  return _SendARPRequest(pPacket, dest_ip);
}

/*********************************************************************
*
*       IP_ARP_Timer
*
*  Function description
*    Checks the ARP table for expired entries.
*    Expired entries are removed from the list.
*/
void IP_ARP_Timer(void) {
  int i;
  ARP * p;

  for (i = 0; i < MAXARPS; i++)  {
    p = &_aARP[i];

    /* age out pPending entrys here: */
    if (p->pPending) {
      /* if over a second old.. */
      if (IP_IsExpired(p->createtime + 1000)) {
        IP_LOG((IP_MTYPE_ARP, "ARP: Freeing packet waiting for ARP response: %i %h", p->t_pro_addr, &p->t_phy_addr));
        pk_free(p->pPending);   /* free the blocked IP packet */
        p->pPending = NULL;     /* clear pointer */
        p->t_pro_addr = 0;     /* marks entry as "unused" */
      }
    }
    //
    // Remove aged out entries from table if they have not been used recently
    //
    if (p->t_pro_addr)   {                                // Is entry used ?
      if (IP_IsExpired(p->createtime + arp_ageout)) {  // Aged out ?
        if (IP_IsExpired(p->lasttime + 1000)) {        // Recentently used ?
          IP_LOG((IP_MTYPE_ARP, "ARP: Removing ARP entry [%d] for IP %i: %h", i, p->t_pro_addr, &p->t_phy_addr));
          p->t_pro_addr = 0;
        }
      }
    }
  }
}


/*********************************************************************
*
*       IP_ARP_SendRequest()
*
*  Function description
*    Sends ARP request fo rno particular reason, not associated with any particular packet.
*    Typical usage is from a DHCP client who wants to find out if the given addr. is already in use.
*/
void IP_ARP_SendRequest(ip_addr dest_ip) {
  _SendARPRequest(NULL, dest_ip);
}


/*********************************************************************
*
*       IP_ARP_SetAgeout
*/
void IP_ARP_SetAgeout(U32 Ageout) {
  arp_ageout  =  Ageout;        // APR table refresh age [ms]
}

/*********************************************************************
*
*       IP_ShowARP
*/
int IP_ShowARP(void (*pfSendf)(void * pContext, const char * sFormat, ...), void * pContext) {
#if IP_DEBUG > 0
   ARP *   atp;
   int   i;
   int   arp_entrys  =  0;

   pfSendf(pContext, "arp Requests In: %u,   out: %u\n", IP_ARP_Stat.ReqsIn, IP_ARP_Stat.ReqsOut);
   pfSendf(pContext, "arp Replys   In: %u,   out: %u\n", IP_ARP_Stat.RepsIn, IP_ARP_Stat.RepsOut);

   /* count number of arp entrys in use: */
   for (i = 0; i < MAXARPS; i++) {
     if (_aARP[i].t_pro_addr) {
       arp_entrys++;
     }
   }

   if (arp_entrys) {
      pfSendf(pContext, "X)  MAC Address         iface pend IP           ctime  ltime\n");
      for (i = 0; i < MAXARPS; i++) {
         atp = &_aARP[i];
         if (atp->t_pro_addr) {
            pfSendf(pContext, "%d)  ", i);
            pfSendf(pContext, "%02x-%02x-%02x-%02x-%02x-%02x ", atp->t_phy_addr[0], atp->t_phy_addr[1], atp->t_phy_addr[2], atp->t_phy_addr[3], atp->t_phy_addr[4], atp->t_phy_addr[5]);
            pfSendf(pContext, "  %d     %s    %i   %lu  %lu\n", if_netnumber(atp->pNet)+1, atp->pPending ? "Y":"N",  atp->t_pro_addr, (long)atp->createtime, (long)atp->lasttime);
         }
      }
   } else {
     pfSendf(pContext, "Currently no arp table entrys.\n");
   }
#else
  pfSendf(pContext,"ARP statistics not available in release build.");
#endif
   return 0;
}

/*********************************************************************
*
*       IP_ARP_HasEntry
*
*  Function description
*    Typical usage is from a DHCP client who wants to find out if the given addr. is already in use.
*/
int IP_ARP_HasEntry(ip_addr dest_ip) {
  ARP * pArp;
  U8  * p;
  int i;

  pArp = _FindEntryByIP(dest_ip);

  if (!pArp) {
    return 0;      // No entry
  }
  //
  // Check if MAC Addr is != 00:00:00:00:00:00
  //
  p = pArp->t_phy_addr;
  for (i = 0; i < 6; i++) {
    if (*p++) {
      return 1;             // Valid entry
    }
  }

  return 0;                 // No ARP packet recived (MAC addr unknown)
}

/*********************************************************************
*
*       IP_ETH_SendMulticast()
*
*  Function description
*    Sends a multicast packet.
*
*  Note:
*    The IANA owns an Ethernet block, which in hexadecimal is 00:00:5E.
*    This is the high-order 24 bits of the ethernet address, meaning
*    that this block includes addresses in the range 00:00:5E:00:00:00
*    through 00:00:5E:FF:FF:FF. The IANA allocates half of this block
*    for multicast addresses. Given that the first byte of any Ethernet
*    address must be 0x01 to specify a  multicast address, this means
*    the Ethernet addresses corresponding to IP multicasting are in the
*    range 01:00:5E:00:00:00 through 01:00:5E:1F:FF:FF.
*
*  Return value:
*     0  if OK
*   !=0  on error (packet can not be sent)
*/
int IP_ETH_SendMulticast(IP_PACKET * pPacket, ip_addr dest_ip) {
  U8 abDestAddr[6];
  U8 * pSrc;

  //
  // Convert Multicast IP addr. to multicast MAC addr.
  // The conversion process is simple:
  // The first 25 bits are fixed to 0x01, 0x0, 0x5E, 0 (bit)
  // The last 23 bits are identical to the lower 23 bits of the Multicast IP addr.
  //
  pSrc  = 1 + (U8*)&dest_ip;
  abDestAddr[0] = 0x1;
  abDestAddr[1] = 0x0;
  abDestAddr[2] = 0x5E;
  abDestAddr[3] = (*pSrc++) & 0x7f;
  abDestAddr[4] = *pSrc++;
  abDestAddr[5] = *pSrc;
  return _SendEthernetPacket(pPacket, ETH_TYPE_IP, &abDestAddr[0]);
}

/*********************************************************************
*
*       IP_ETH_SendBroadcast()
*
*  Function description
*    Sends a Broadcast packet.
*
*  Return value:
*     0  if OK
*   !=0  on error (packet can not be sent)
*
*  Note:
*/
int IP_ETH_SendBroadcast(IP_PACKET * pPacket) {
  return _SendEthernetPacket(pPacket, ETH_TYPE_IP, &_abHardwareAddrBroadcast[0]);
}

/*********************************************************************
*
*       IP_ARP_AddACDHandler
*
*  Function description
*    Adds an address conflict handler to the ARP module.
*/
void IP_ARP_AddACDHandler(void (*pf)(int, IP_PACKET*)) {
  pfHandleAddrConflict = pf;
}

/*************************** End of file ****************************/

