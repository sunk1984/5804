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
File    : IP_Packet.c
Purpose : Code to manage the queues of free packet buffers.
--------  END-OF-HEADER  ---------------------------------------------
*/

/* Additional Copyrights: */
/* Copyright  2000 By InterNiche Technologies Inc. All rights reserved */
/* Portions Copyright 1990-1994 by NetPort Software  */
/* Portions Copyright 1986 by Carnegie Mellon  */
/* Portions Copyright 1984, 1985 by the Massachusetts Institute of Technology  */

#include "IP_Int.h"
#include <stdio.h>

#if IP_DEBUG
QUEUE IP_MiscPacket;      // Keep track of packets which are not in any queue (debug only!)

/*********************************************************************
*
*       IP_PACKET_Q_Add()
*
*  Function description
*    Adds an item to tail of queue.
*/
void IP_PACKET_Q_Add(QUEUE * pQ, IP_PACKET * pPacket) {
  QUEUE_ITEM * pItem;

  pItem = (QUEUE_ITEM*)pPacket;
  IP_Q_RemoveItem(&IP_MiscPacket, pItem);
  IP_Q_Add(pQ, pPacket);
}

/*********************************************************************
*
*       IP_PACKET_Q_TryGetRemoveFirst()
*
*  Function description
*    Get and remove first item from queue
*/
IP_PACKET *  IP_PACKET_Q_TryGetRemoveFirst(QUEUE * pQ) {
  IP_PACKET * pPacket;

  pPacket = (IP_PACKET *)IP_Q_TryGetRemoveFirst(pQ);
  if (pPacket) {
    IP_Q_Add(&IP_MiscPacket, pPacket);
  }
  return pPacket;
}

/*********************************************************************
*
*       IP_PACKET_Q_GetRemoveFirst()
*
*  Function description
*    Get and remove first item from queue
*/
IP_PACKET *  IP_PACKET_Q_GetRemoveFirst(QUEUE * pQ) {
  IP_PACKET * pPacket;

  pPacket = (IP_PACKET *)IP_Q_GetRemoveFirst(pQ);
  if (pPacket) {
    IP_Q_Add(&IP_MiscPacket, pPacket);
  }
  return pPacket;
}
#endif


/*********************************************************************
*
*       IP_PACKET_Alloc
*
*  Function description
*    Returns a pointer to a netbuf structure (IP_PACKET *) with a buffer
*    big enough for len bytes, or NULL if none is available.
*    Interrupts are disabled during execution to make the function thread-safe.
*/
IP_PACKET * IP_PACKET_Alloc(unsigned len) {
  IP_PACKET * p;
  int i;

  for (i = 0;;) {
    if (len <= IP_Global.aBufferConfigSize[i]) {      // Packet size big enough in this pool ?
      p = (IP_PACKET *)IP_Q_TryGetRemoveFirst(&IP_Global.aFreeBufferQ[i]);
      if (p) {     // Any packet available in this queue ?
        break;
      }
    }
    if (++i >= COUNTOF(IP_Global.aBufferConfigSize)) {
      IP_WARN((IP_MTYPE_NET_IN, "NET: No packet buffer for packet of %d bytes --- Discarded", len));
      p = NULL;
      goto Done;       // No buffer !
    }
  }
  //
  // O.K., we managed to get a packet buffer.
  //
  p->pData = p->pBuffer + IP_aIFace[0].n_lnh;   // Point past biggest mac header
  p->NumBytes = 0;   /* no protocol data there yet */
  p->pNet = NULL;
  p->UseCnt = 1;  /* initially buffer in use by 1 user */
#if IP_DEBUG
  IP_Q_Add(&IP_MiscPacket, p);
#endif

Done:
  return p;
}


/*********************************************************************
*
*       pk_free
*
*  Function description
*    Return a packet to the free queue.
*    Interrupts are disabled during execution to make the function thread-safe.
*
*  Context
*    Can be called from task or ISR
*/
void pk_free(IP_PACKET * pPacket) {
  int Cnt;
  int i;

  Cnt = --pPacket->UseCnt;
  //
  // Perform sanity checks (debug build)
  //
  if (Cnt < 0) {
    IP_PANIC("Negative packet count. Packet has been freed too often.");
  }
  //
  // Find pool for packet
  //
  if (Cnt == 0) {  /* more than 1 owner? */
#if IP_DEBUG
    IP_Q_RemoveItem(&IP_MiscPacket, pPacket);
#endif
    for (i = 0;;) {
      if (pPacket->BufferSize == IP_Global.aBufferConfigSize[i]) {
        break;
      }
      i++;
      if (i >= COUNTOF(IP_Global.aBufferConfigSize)) {
        IP_PANIC("Illegal packet size!");
      }
    }
  //
  // Perform sanity checks (debug build) and add packet to free queue
  //
#if IP_DEBUG
      {
        char * p;
        p = (char*)pPacket->pBuffer;
        p -= 4;
        if (*(U32*)p != 0x44444444) {
          IP_PANIC("Start of packet overwritten!");
        }
        p += 4 + pPacket->BufferSize;
        if (*(U32*)p != 0x45454545) {
          IP_PANIC("End of packet overwritten!");
        }
      }
#endif
    IP_Q_Add(&IP_Global.aFreeBufferQ[i], (QUEUE_ITEM*)pPacket);
  }
}


/*********************************************************************
*
*       pk_init
*
*  Function description
*    Init the free queue (or queues) for use by IP_PACKET_Alloc() and pk_free() above.
*
*  RETURNS: Returns 0 if OK, else negative error code.
*/
int pk_init() {
  IP_PACKET * packet;
  unsigned iPool;
  unsigned NumBuffers;
  unsigned BufferSize;
  unsigned iBuffer;

  for (iPool = 0; iPool < COUNTOF(IP_Global.aBufferConfigSize); iPool++) {
    NumBuffers = IP_Global.aBufferConfigNum[iPool];
    BufferSize = IP_Global.aBufferConfigSize[iPool];
    for (iBuffer = 0; iBuffer < NumBuffers; iBuffer++) {
      packet = (IP_PACKET *) IP_Alloc(sizeof(IP_PACKET));      // Panic if no memory
#if IP_DEBUG_Q
    IP_Global.aFreeBufferQ[iPool].q_min = IP_Global.aBufferConfigNum[iPool];
#endif
#if IP_DEBUG
      {
        char * p;
        p = (char *)IP_Alloc(BufferSize + 8);                  // Panic if no memory
        *(U32*)p = 0x44444444;
        p += 4;
        packet->pBuffer = p;
        p += BufferSize;
        *(U32*)p = 0x45454545;
      }
#else
      packet->pBuffer = (char *)IP_Alloc(BufferSize);           // Panic if no memory
#endif
      packet->BufferSize = BufferSize;
      IP_Q_Add(&IP_Global.aFreeBufferQ[iPool], packet);        /* save it in big pkt free queue */
    }
  }
  return 0;
}

/*************************** End of file ****************************/


