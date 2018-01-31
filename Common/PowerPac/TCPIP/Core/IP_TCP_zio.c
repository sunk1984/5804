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
File    : IP_TCP_zio.c
Purpose : Socket zero-copy extensions.
--------  END-OF-HEADER  ---------------------------------------------
*/


/* Additional Copyrights: */
/* Copyright 1999- 2000 By InterNiche Technologies Inc. All rights reserved */

#include "IP_Int.h"
#include "IP_sockvar.h"


#include "IP_protosw.h"
#include "IP_TCP_pcb.h"
#include "IP_TCP.h"
#include "IP_mbuf.h"       /* BSD-ish Sockets includes */

/* bitmask for connection state bits which determine if send/recv is OK */
#define  SO_IO_OK (SS_ISCONNECTED|SS_ISCONNECTING|SS_ISDISCONNECTING)



/*********************************************************************
*
*       IP_TCP_Send()
*
*  Function description
*    Send a packet (previously allocated by a call to IP_TCP_Alloc())
*    on a socket. Packet is freed by IP_TCP_Send() if
*    OK, not freed if there's an error. Note: pkt->n_prot points to
*    data to be sent. The TCP/IP layers will prepend headers to this
*    data. Returns 0 if OK or one of the IP_ERR error codes. If the error
*    code is IP_ERR_SEND_PENDING then the socket errno is set with a BSD
*    error code, but the packet is queued on the socket's send buffer
*    and so must not be freed by the application.
*
*  Return value
*
*
*/
int IP_TCP_Send(long hSock, IP_PACKET * pkt) { /* socket & packet to send */
  struct socket *   so;
  struct mbuf *  m;
  int   err;

  so = IP_SOCKET_h2p(hSock);
  if (so == NULL) {
    return SOCKET_ERROR;     // Socket has not been opened or has already been closed
  }

  if (!(so->so_state & SS_INUPCALL)) {
    LOCK_NET();
  }

  if ((so->so_state & SO_IO_OK) != SS_ISCONNECTED) {
    err = IP_ERR_BAD_STATE;   /* socket is not connected! */
    goto errexit;
  }

  /* make sure space in sockbuf is available */
  if ((so->so_snd.NumBytes + pkt->NumBytes) > so->so_snd.Limit) {
    err = IP_ERR_RESOURCE;     /* not allowed, use t_send() */
    goto errexit;
  }

  m = MBUF_GET(MT_TXDATA);
  m->pkt = pkt;
  m->m_data = pkt->pData;
  m->m_len = pkt->NumBytes;

  sbappend(&so->so_snd, m);
  err =  tcp_output( (struct tcpcb *) (so->so_pcb->inp_ppcb) );

  if (!(so->so_state & SS_INUPCALL)) {
    UNLOCK_NET();
  }

  if (err) {
    so->so_error =  err;
    return IP_ERR_SEND_PENDING;
  }
  return 0;

errexit:
  /* return error (as return, not as socket error) */
  if (!(so->so_state & SS_INUPCALL)) {
    UNLOCK_NET();
  }
  return err;
}

/*********************************************************************
*
*       IP_TCP_SendAndFree()
*
*  Function description
*    Same as IP_TCP_Send(), but always frees the packet, even in case of error.
*
*  Return value
*    Same as IP_TCP_Send()
*
*/
int IP_TCP_SendAndFree(long s, IP_PACKET * pkt) { /* socket & packet to send */
  int e;
  e = IP_TCP_Send(s, pkt);
  if (e < 0) {
    IP_TCP_Free(pkt);
  }
  return e;
}

/*********************************************************************
*
*       IP_TCP_Alloc()
*
*   Request a data buffer for sending tcp
*   data to IP_TCP_Send(). Generally datasize must be smaller than the
*   tcp data size of smallest device, usually about 1440 bytes.
*   Packets allocated with this should be freed either implicitly by a
*   successful send call to IP_TCP_Send(), or explicitly by a call to
*   IP_TCP_Free(). Returns a IP_PACKET * structure if OK, else NULL.
*
*/
IP_PACKET * IP_TCP_Alloc(int datasize) {
  IP_PACKET * pkt;

  pkt = IP_PACKET_Alloc(datasize + IP_TCP_HEADER_SIZE);
  if (pkt) {
    /* save space for tcp/ip/MAC headers */
    pkt->pData = pkt->pBuffer + IP_TCP_HEADER_SIZE;
    pkt->NumBytes = datasize; //pkt->nb_blen;
  }
  return pkt;
}

// RS: TBD: We should also have a function which takes the socket and required data size
// This would allow us to alloc packets of the required size, rather than simply always adding
// the worst-case header size
//IP_PACKET * IP_TCP_AllocEx(long sock, int *pDatasize) {


/*********************************************************************
*
*       IP_TCP_Free()
*
* Free a packet allocated by (presumably) IP_TCP_Alloc().
* This is a simple wrapper around pk_free() to lock
* and unlock the free-queue resource.
*/
void IP_TCP_Free(IP_PACKET * p) {
  pk_free(p);
}


/*********************************************************************
*
*       IP_TCP_DataUpcall()
*
* Called by tcp_input() when data is received
* for a socket with an upcall handler set. The upcall handler is a
* socket structure member, set by the app via a call to
* setsockopt(SO_CALLBACK, ptr). The upcall routine description is as
* follows: int rx_upcall(struct socket so, IP_PACKET * pkt, int error);
* where: so - socket which got data. pkt - pkt containing recieved
* data, or NULL if error. error - 0 if good pkt, else BSD socket
* error The upcall() returns 0 if it accepted data, non-zero if not.
* End of file is signaled to the upcall by ESHUTDOWN eror code. If
* LOCK_NET_RESOURCE() is used, the resource is already locked when
* the upcall is called. The upcall will NOT be called from inside a
* CRIT_SECTION macro pairing.
*
*/
void IP_TCP_DataUpcall(struct socket * so) {
   struct mbuf *  m;
   IP_PACKET * pkt;
   int   e;

   /* don't upcall application if there's no data */
   if (so->so_rcv.NumBytes == 0) {
     return;
   }

   /* don't re-enter the upcall routine */
   if (so->so_state & SS_INUPCALL) {
     return;
   }

   /* Set flags. SS_UPCALLED is used by select() logic to wake sockets blocked
   on receive, SS_INUPCALL is the re-entry guard. */
   so->so_state |= (SS_UPCALLED|SS_INUPCALL);

   m = so->so_rcv.sb_mb;
   while (m) {
      /* TCP code left pkt pointers at IP header, need to fix: */
      pkt = m->pkt;        /* get pkt from mbuf */
      pkt->pData = m->m_data;
      pkt->NumBytes = m->m_len;

      /* pass buffer to the app */
      e = so->rx_upcall(IP_SOCKET_p2h(so), pkt, 0);

      if (e == IP_OK) {
        IP_TCP_Free(pkt);
      } else if (e != IP_OK_KEEP_PACKET) { /* if app returned error, quit */
         break;
      }

      /* dequeue the mbuf data the application just accepted */
      m->pkt = NULL;    /* pkt is now the appp's responsability */
      sbfree(&so->so_rcv, m);
      MFREE(m, so->so_rcv.sb_mb);
      m = so->so_rcv.sb_mb;
   }
   so->so_state &= ~SS_INUPCALL;    /* clear re-entry flag */
   return;
}




/*************************** End of file ****************************/
