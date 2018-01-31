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
File    : IP_ICMP.c
Purpose : ICMP code
--------  END-OF-HEADER  ---------------------------------------------
*/

/* Additional Copyrights: */
/* Copyright  2000 By InterNiche Technologies Inc. All rights reserved */
/* Portions Copyright 1990-1994 by NetPort Software. */
/* Portions Copyright 1986 by Carnegie Mellon  */
/* Portions Copyright 1984 by the Massachusetts Institute of Technology  */


#include "IP_Int.h"
#include "IP_ICMP.h"

IP_ICMP_MIB         IP_ICMP_Mib;   // Storage for MIB statistics
static IP_RX_HOOK * _pfOnRx;

/*********************************************************************
*
*       IP_ICMP_SetRxHook
*
*  Function description
*    Allows to set a hook function which will be called
*    if target receives a ping packet.
*/
void IP_ICMP_SetRxHook(IP_RX_HOOK * pf) {
  _pfOnRx = pf;
}

/*********************************************************************
*
*       IP_ICMP_OnRx
*
* Function description
*   ICMP received packet upcall handler.
*/
void IP_ICMP_OnRx(IP_PACKET * pPacket) {
  unsigned len;        /* packet length, minus IP & MAC headers */
  ip_addr host;        /* the foreign host */
  struct ping *  e;
  struct ip * pip;
  int r;

  len = pPacket->NumBytes;    /* adjusted for us by IP layer */
  host = pPacket->fhost;  /* filled in by IP layer */
  pip = ip_head(pPacket);    /* find IP header */
  e = (struct ping *)ip_data(pip); /* ...and icmp header */
  IP_LOG((IP_MTYPE_ICMP, "ICMP: p[%u] from %i", len, host));
  IP_STAT_INC(IP_ICMP_Mib.InMsgs);        /* received one more icmp */
  //
  // Call hook function to give application a chance to intercept the packet
  //
  if (_pfOnRx) {
    r = (_pfOnRx)(pPacket);
    if (r) {
      pk_free(pPacket);
      return;
    }
  }
  //
  // Handle echo requests by sending back the ECHO REPLY
  //
  if (e->ptype == IP_ICMP_ECHO_REQ) {  /* got ping request, send reply */
    len -= ip_hlen(pip);    /* strip IP header from reply length */
    /* reset data pointer to IP header since we use p for reply */
    pPacket->pData = (char*)e;
    IP_STAT_INC(IP_ICMP_Mib.InEchos);
    IP_LOG((IP_MTYPE_ICMP, "ICMP: echo reply to %i", host));
    e->ptype = IP_ICMP_ECHO_REP;
    //
    // Calc checksum if driver can not do this automatically.
    //
    e->pchksum = 0;
    if ((pPacket->pNet->Caps & IP_NI_CAPS_WRITE_ICMP_CHKSUM) == 0) {
      e->pchksum = (U16)~IP_CalcChecksum_Byte(e, len);
    }
    pip->ip_src = pip->ip_dest;
    pip->ip_dest = host;
    IP_STAT_INC(IP_ICMP_Mib.OutEchoReps);
    IP_STAT_INC(IP_ICMP_Mib.OutMsgs);
    pPacket->fhost = host;
    pPacket->NumBytes = len;
    if (IP_Write(IP_PROT_ICMP, pPacket)) {
      IP_LOG((IP_MTYPE_ICMP, "ICMP: reply failed"));
    }
    /* reused pPacket will be freed by net->xxx_send() */
    return;
  }
  //
  // Handle echo replys
  //
  if (e->ptype == IP_ICMP_ECHO_REP) {   // Got ping reply
    IP_STAT_INC(IP_ICMP_Mib.InEchoReps);
  }
  pk_free(pPacket);
}

/********************************************************************
*
*       FUNCTION: icmp_destun()
*
* Function description
*   Send an ICMP destination unreachable packet.
*
* Parameters
*   host:     host to complain to
*   ip:       IP header of offending packet
*   typecode: type & code of DU to send (PROT, PORT, HOST)
*   net:      Interface that this packet came in on
*
*   If the type field is 0, then type is assumed to be DESTIN.
*/
void icmp_destun(ip_addr host,  /* host to complain to */
   struct ip * ip,   /* IP header of offending packet */
   unsigned typecode,    /* type & code of DU to send (PROT, PORT, HOST) */
   NET *  net)        /* interface that this packet came in on */
{
  IP_PACKET * pPacket;
  struct destun *   d;
  int   i;

  IP_LOG((IP_MTYPE_ICMP, "ICMP: sending dest unreachable to %i", host));

  pPacket = IP_PACKET_Alloc(512 + IPHSIZ);   /* get packet to send icmp dest unreachable */

  if (pPacket == NULL) {
    IP_LOG((IP_MTYPE_ICMP, "ICMP: can't alloc pkt"));
    IP_STAT_INC(IP_ICMP_Mib.OutErrors);
    return;
  }

  /* allow space for icmp header */
  pPacket->pData += sizeof(struct ip);
  pPacket->NumBytes -= sizeof(struct ip);
  pPacket->pNet = net;     /* Put in the interface that this packet came in on */

  d = (struct destun *)pPacket->pData;

  if (typecode & 0xFF00) {               /* if the type was sent */
    d->dtype = (char)(typecode >>8);   /* then use it */
  } else {                                  /* else use default */
    d->dtype = IP_ICMP_DEST_UN;
  }
  d->dcode = (char)(typecode & 0xFF);
  IP_MEMCPY(&d->dip, ip, (sizeof(struct ip) + ICMPDUDATA));

  //
  // Compute checksum.
  // Since this is an error packet which is not send in normal operation, things are not time critical and
  // we do not use hardware to compute the checksum
  //
  d->dchksum = 0;
  d->dchksum = (U16)~IP_CKSUM(d, sizeof(struct destun) >> 1);
  pPacket->NumBytes =  sizeof(struct destun);
  pPacket->fhost = host;
  i = IP_Write(IP_PROT_ICMP, pPacket);
  if (i) {
    IP_STAT_INC(IP_ICMP_Mib.OutErrors);
    IP_LOG((IP_MTYPE_ICMP, "ICMP: Can't send dest unreachable."));
    return;
  }
  IP_STAT_INC(IP_ICMP_Mib.OutMsgs);
  IP_STAT_INC(IP_ICMP_Mib.OutDestUnreachs);
}


/*********************************************************************
*
*       IP_ShowICMP
*
*  Printf info about icmp MIB statistics.
*/
int IP_ShowICMP(void (*pfSendf)(void * pContext, const char * sFormat, ...), void * pContext) {
#if IP_INCLUDE_STAT
  pfSendf(pContext, "ICMP layer stats:\n");
  pfSendf(pContext, "InMsgs %lu    InErrors %lu, echoReqs %lu, echoReps %lu, unhandledTypes: %lu\n",  IP_ICMP_Mib.InMsgs, IP_ICMP_Mib.InErrors, IP_ICMP_Mib.InEchos, IP_ICMP_Mib.InEchoReps);
  pfSendf(pContext, "OutMsgs %lu    icmpOutErrors %lu, \n", IP_ICMP_Mib.OutMsgs, IP_ICMP_Mib.OutErrors, IP_ICMP_Mib.OutEchoReps, IP_ICMP_Mib.OutRedirects);
#else
  pfSendf(pContext, "Statistics unavailable in this build.\n");
#endif
  return 0;
}

/*************************** End of file ****************************/


