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
File    : IP_Ping.c
Purpose :
--------  END-OF-HEADER  ---------------------------------------------
*/

/* Additional Copyrights: */
/* Portions Copyright 1996- 2000 By InterNiche Technologies Inc. All rights reserved */
/* Portions Copyright 1994 by NetPort software. */

#include "IP_Int.h"
#include "IP_ICMP.h"

char *   pingdata =  "Ping from target IP stack";  /* default ping data */



/*********************************************************************
*
*       IP_SendPing()
*
* icmpEcho() - ICMP Echo Request (the guts of "ping") Callable from
* Applications. Sends a single "ping" (ICMP echo request) to the
* specified host. Replys are upcalled via Queue event so we can send
* the application's window a Message as we get a reply or timeout.
*
*
* RETURNS: Returns 0 if started OK, else negative error message.
*/
int IP_SendPing(ip_addr host,  /* host to ping - 32 bit, network-endian */
                char *   data,       /* ping data, NULL if don't care */
                unsigned datalen,     /* length of data to attach to ping request */
                U16  pingseq)    /* ping sequence number */
{
   IP_PACKET *   pPacket;
   int      ip_err;
   struct ping *  e;
   struct ip *    pip;

   pPacket = IP_PACKET_Alloc(PINGHDRSLEN + datalen);
   if (!pPacket) {
     IP_WARN((IP_MTYPE_ICMP, "ICMP: can't alloc packet"));
     return IP_ERR_NOBUFFER;
   }

   pPacket->pData = pPacket->pBuffer + PINGHDRSLEN;
   pPacket->NumBytes = datalen;
   pPacket->fhost = host;
   if(host == 0xFFFFFFFF) { /* broadcast? */
     pPacket->pNet = &IP_aIFace[0];    /* then use first iface */
   }
   /* copy in data field */
   if (data) {
     IP_MEMCPY(pPacket->pData, data, datalen);
   } else {  /* caller didn't specify data */
      unsigned   donedata;
      strcpy(pPacket->pData, pingdata);
      donedata = (unsigned)strlen(pingdata);
      while (donedata < datalen) {
        *(pPacket->pData + donedata) = (char)((donedata) & 0x00FF);
        donedata++;
      }
   }
   /* adjust packet pointers to icmp ping header */
   pPacket->pData -= sizeof(struct ping);
   pPacket->NumBytes += sizeof(struct ping);
   /* fill in icmp ping header */
   e = (struct ping *)pPacket->pData;
   e->ptype = IP_ICMP_ECHO_REQ;
   e->pcode = 0;
   e->pid = 0;
   e->pseq = pingseq;
   //
   // Calculate the checksum
   //
   e->pchksum = 0;
   e->pchksum = (U16)~IP_CalcChecksum_Byte(e, ICMPSIZE+datalen);
   //
   // need to fill in IP addresses at this layer too
   //
   pip = (struct ip *)(pPacket->pData - sizeof(struct ip));
   pip->ip_src = ip_mymach(host);
   pip->ip_dest = host;

   LOCK_NET();
   ip_err = IP_Write(IP_PROT_ICMP, pPacket);    /* send down to IP layer */
   UNLOCK_NET();

   /* Errors are negative. A zero means send was OK. a positive number
    * usually means we had to ARP. Assume this will work and count a send.
    */
   if (ip_err < 0) {
     IP_WARN((IP_MTYPE_ICMP | IP_MTYPE_NET_OUT, "IMCP: can't send echo request"));
     // rfc 1156 seems to say not to count these. (pg 48)
     return ip_err;
   }
   // Fall to here if we sent echo request OK
   IP_STAT_INC(IP_ICMP_Mib.OutMsgs);
   IP_STAT_INC(IP_ICMP_Mib.OutEchos);
   return 0;
}


/*************************** End of file ****************************/
