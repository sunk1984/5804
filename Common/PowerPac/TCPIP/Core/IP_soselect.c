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
File    : IP_soselect.c
Purpose : Sockets library select() support. This will probably
          need to be modified on a per-port basis. If your sockets code does
          not rely on select() calls, then you can omit this file form the
          build.
--------  END-OF-HEADER  ---------------------------------------------
*/

/* Additional Copyrights: */
/* Copyright 1997- 2000 By InterNiche Technologies Inc. All rights reserved */

#include "IP_Int.h"
#include "IP_sockvar.h"

#ifdef INCLUDE_TCP  /* include/exclude whole file at compile time */
#ifdef SO_SELECT  /* whole file can be ifdeffed out */

#include "IP_protosw.h"
#include "IP_TCP_pcb.h"


#define  SOREAD      1
#define  SOWRITE     2

/*********************************************************************
*
*       sock_select()
*
*  Function description
*    sock_select - handle the select logic for a single event on a
*    single socket. This is called multiple times from sock_selscan
*    above. Retuns 1 if socket is ready for event, 0 if not.
*/
static int sock_select(long hSock, int flag) {
  struct socket *   so;
  int   ready =  0;

  so = IP_SOCKET_h2p(hSock);
  if (so == NULL) {
    return SOCKET_ERROR;     // Socket has not been opened or has already been closed
  }

   switch (flag)
   {
   case SOREAD:
      /* can we read something from so? */
      if (so->so_rcv.NumBytes)
      {
         ready = 1;
         break;
      }
      if (so->so_state & SS_CANTRCVMORE)
      {  ready = 1;
         break;
      }
      if (so->so_qlen)  /* attach is ready */
      {
         ready = 1;
         break;
      }

#ifdef TCP_ZEROCOPY
      /* zerocopy sockets with a callback set should wbe awakened
      if there is pending data which has been upcalled */
      if ((so->rx_upcall) && (so->so_state & SS_UPCALLED))
      {
         so->so_state &= ~SS_UPCALLED; /* clear flag */
         {
            ready = 1;
            break;
         }
      }
#endif   /* TCP_ZEROCOPY */

      /* fall to here if so is not ready to reead */
      so->so_rcv.sb_flags |= SB_SEL;   /* set flag for select wakeup */
      break;

   case SOWRITE:
      if ((sbspace(&(so)->so_snd) > 0) &&
          ((((so)->so_state&SS_ISCONNECTED) ||
            ((so)->so_proto->pr_flags&PR_CONNREQUIRED)==0) ||
           ((so)->so_state & SS_CANTSENDMORE)))
      {
         ready = 1;
         break;
      }
      sbselqueue (&so->so_snd);
      break;

   case 0:
      if (so->so_state & SS_RCVATMARK) {
         ready = 1;
         break;
      }
      if (so->so_error &&
          (so->so_error != EINPROGRESS) &&
          (so->so_error != EWOULDBLOCK))
      {
         ready = 1;
         break;
      }
      sbselqueue(&so->so_rcv);
      break;
   }

   return ready;
}

/*********************************************************************
*
*       sock_selscan()
*
*  Function description
*    sock_selscan() - internal non-blocking routine under t_select().
*    This scans the two FD lists passed for activity by calling the
*    subroutine sock_select() on a per-socket basis.
*/
static int sock_selscan(IP_fd_set * ibits, IP_fd_set * obits) {
  IP_fd_set *in, *out;
  int   which;
  int   sock;
  int   flag  =  0;
  int   num_sel  =  0;

  for (which = 0; which < 3; which++) {
    switch (which) {
    case 0:
       flag = SOREAD; break;

    case 1:
       flag = SOWRITE; break;

    case 2:
       flag = 0; break;
    }
    in = &ibits [which];
    out = &obits [which];
    for (sock = 0; sock < (int)in->fd_count; sock++) {
       if (sock_select (in->fd_array[sock], flag)) {
          IP_FD_SET(in->fd_array[sock], out);
          num_sel++;
       }
    }
  }
  return num_sel;
}

/*********************************************************************
*
*       t_select
*
*  Function description
*    Implement a UNIX-like socket select call. Causes the
*    calling process to block waiting for activity on any of a list of
*    sockets. Arrays of socket descriptions are passed for read, write,
*    and exception event. Any of these may be NULL of the event is not
*    of interest. A timeout is also passed, which given in cticks (TPS
*    ticks per second). Returns number of sockets which had an event
*    occur.
*/
int t_select(IP_fd_set * in, IP_fd_set * out,  IP_fd_set * ex, long  tv) {
  IP_fd_set obits[3], ibits [3];
  U32   tmo;
  int   retval   =  0;

  IP_MEMSET(&obits, 0, sizeof(obits));
  IP_MEMSET(&ibits, 0, sizeof(ibits));

  if (in) {
    IP_MEMCPY(&ibits[0], in, sizeof(IP_fd_set));
  }
  if (out) {
    IP_MEMCPY(&ibits[1], out, sizeof(IP_fd_set));
  }
  if (ex) {
    IP_MEMCPY(&ibits[2], ex, sizeof(IP_fd_set));
  }
  tmo = IP_OS_GetTime32() + tv;

  /* if all the IP_fd_set are empty, just block;  else do a real select() */
  if ((ibits[0].fd_count == 0) && (ibits[1].fd_count == 0) && (ibits[2].fd_count == 0)) {
   if (tv > 0) {     /* make sure we don't block on nothing forever */
     IP_OS_Delay(tv);
   }
  } else {
#ifdef SOC_CHECK_ALWAYS
      int i, j;
      struct socket * so;

      for (i = 0; i < 2; i++) {
         for (j = 0; j < ibits[i].fd_count; j++) {
            long hSock;
            hSock = ibits[i].fd_array[j];
            so = IP_SOCKET_h2p(hSock);
            if (so == NULL) {
              return SOCKET_ERROR;     // Socket has not been opened or has already been closed
            }
         }
      }
#endif /* SOC_CHECK_ALWAYS */

      /* Lock the net semaphore before going into selscan. Upon
       * return we will either call tcp_sleep(), which unlocks the
       * semaphore, or fall into the unlock statement.
       */
      LOCK_NET();
      while ((retval = sock_selscan(ibits, obits)) == 0) {
         if (tv != -1L)  {
           if (tmo <= IP_OS_GetTime32()) {
             break;
           }
         }
         select_wait = 1;
         IP_OS_WaitItem (&select_wait);
      }
      UNLOCK_NET();

   }

  if (retval >= 0) {
    if (in) {
      IP_MEMCPY(in, &obits[0], sizeof(IP_fd_set));
    }
    if (out) {
      IP_MEMCPY(out, &obits[1], sizeof(IP_fd_set));
    }
    if (ex) {
      IP_MEMCPY(ex, &obits[2], sizeof(IP_fd_set));
    }
  }
  return retval;
}

/* The next three routines are derived from macros, which is how they
  are traditionally implemented. */

/*********************************************************************
*
*       IP_FD_CLR()
*/
void IP_FD_CLR(long sock, IP_fd_set * set) {
   unsigned i;

   for (i = 0; i < set->fd_count ; i++)  {
      if (set->fd_array[i] == sock)  {
         while (i + 1 < set->fd_count) {
            set->fd_array[i] = set->fd_array[i + 1];
            i++;
         }
         set->fd_count--;
         return;
      }
   }
  IP_PANIC("SOCKET: IP_FD_CLR(): socket not found in array");
}


/*********************************************************************
*
*       IP_FD_SET()
*/
void IP_FD_SET(long sock, IP_fd_set * set) {
   if (set->fd_count < FD_SETSIZE)
      set->fd_array[set->fd_count++] = sock;
}


/*********************************************************************
*
*       IP_FD_ISSET()
*/
int IP_FD_ISSET(long sock, IP_fd_set * set) {
  int   i;

  for (i = 0; i < (int)set->fd_count ; i++) {
    if (set->fd_array[i] == sock) {
      return 1;
    }
  }
  return 0;
}

#endif   /* SO_SELECT */
#endif /* INCLUDE_TCP */

/*************************** End of file ****************************/

