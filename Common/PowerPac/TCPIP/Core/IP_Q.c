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
File    : IP_Q.c
Purpose : Simple queuing system for internal use by the stack.
--------  END-OF-HEADER  ---------------------------------------------
*/

/* Additional Copyrights: */
/* Copyright  2000 By InterNiche Technologies Inc. All rights reserved */
/* Portions Copyright 1993 by NetPort Software  */
/* Copyright 1986 by Carnegie Mellon  */
/* Copyright 1984 by the Massachusetts Institute of Technology  */


#include "IP_Int.h"
#include "IP_Q.h"


/*********************************************************************
*
*       _RemoveFirst()
*
*  Function description
*    Removes the first item from the queue. Locking must be done from caller.
*    First element must be valid (non-NULL)
*/
static void _RemoveFirst(QUEUE * q) {
  QUEUE_ITEM * pFirst;
  QUEUE_ITEM * pSecond;

  pFirst  = q->q_head;
  pSecond = pFirst->qe_next;
  q->q_head = pSecond;           // Unlink
  if (pSecond == NULL) {        /* queue empty? */
    q->q_tail = NULL;          /* yes, update tail pointer too */
  }
  q->q_len--;                /* update queue length */
#if IP_DEBUG_Q
  pFirst->qe_next = NULL;
  if (q->q_len < q->q_min) {
    q->q_min = q->q_len;
  }
#endif
}

/*********************************************************************
*
*       IP_Q_Add()
*
*  Function description
*    Adds an item to tail of queue.
*/
void IP_Q_Add(QUEUE * q, void * elt) {
  QUEUE_ITEM * pItem;


  ENTER_CRIT_SECTION();

  //
  // Debug build: Make sure element is not already in queue
  //
#if IP_DEBUG_Q
  pItem = q->q_head;
  while (pItem) {
    if ((void *)pItem == elt) {
      IP_PANIC("Element already in Q");
    }
    pItem = pItem->qe_next;
  }
#endif

  pItem = (QUEUE_ITEM *) elt;
  pItem->qe_next = NULL;

  //
  // Add item to the end of the linked list
  //
  if (q->q_head == NULL) {           // QUEUE empty ?
    q->q_head = pItem;               // In this case the new item is also first item
  } else {
    q->q_tail->qe_next = pItem;      // Link it to last item
  }
  q->q_tail = pItem;
  q->q_len++;
  //
  // Let's do some statistics in debug build
  //
#if IP_DEBUG_Q
  if (q->q_len > q->q_max) {
    q->q_max = q->q_len;
  }
#endif
  EXIT_CRIT_SECTION();   /* restore int state */
}

/*********************************************************************
*
*       IP_Q_GetFirst()
*
*  Function description
*    Get first item from queue.
*    Does not remove item from queue.
*/
void * IP_Q_GetFirst(QUEUE * q) {
#if IP_PTR_OP_IS_ATOMIC       // For a 32-bit CPU, there is no need to disable interrupts
  return q->q_head;
#else
  q_elt   temp;        /* temp for result */
  IP_OS_DisableInterrupt();
  temp = q->q_head;
  IP_OS_EnableInterrupt();
  return (void*)temp;
#endif
}

/*********************************************************************
*
*       IP_Q_TryGetRemoveFirst()
*
*  Function description
*    Get and remove first item from queue
*/
void * IP_Q_TryGetRemoveFirst(QUEUE * q) {
  QUEUE_ITEM*   temp;        /* temp for result */

  ENTER_CRIT_SECTION();     /* shut off ints, save old state */
  temp = q->q_head;
  if (temp) {  // Queue empty ?
    _RemoveFirst(q);
  }
  EXIT_CRIT_SECTION();   /* restore caller's int state */
  return (void*)temp;
}


/*********************************************************************
*
*       IP_Q_GetRemoveFirst()
*
*  Function description
*    Get and remove first item from queue
*/
void * IP_Q_GetRemoveFirst(QUEUE * q) {
  QUEUE_ITEM*   temp;        /* temp for result */

  ENTER_CRIT_SECTION();     /* shut off ints, save old state */
  temp = q->q_head;
  if (temp == NULL) {  // Queue empty ?
    IP_PANIC("No element in Q");
  }
  _RemoveFirst(q);
  EXIT_CRIT_SECTION();   /* restore caller's int state */
  return (void*)temp;
}


/*********************************************************************
*
*       IP_Q_RemoveItem()
*
*  Function description
*    Delete an item from the midst of a queue.
*/
void IP_Q_RemoveItem(QUEUE * q, void * elt) {
  QUEUE_ITEM* qptr;
  QUEUE_ITEM* qlast;

  /* search queue for element passed */
  ENTER_CRIT_SECTION();
  if (q->q_len == 0) {
    IP_PANIC("Can not remove an item from an empty Queue");
  }
  qptr = q->q_head;
  qlast = NULL;
  while (qptr) {
    if (qptr == (QUEUE_ITEM*)elt) {
      /* found our item; dequeue it */
      if (qlast) {
        qlast->qe_next = qptr->qe_next;
      } else {     /* item was at head of queqe */
        q->q_head = qptr->qe_next;
      }
      /* fix queue tail pointer if needed */
      if (q->q_tail == (QUEUE_ITEM*)elt) {
        q->q_tail = qlast;
      }

      /* fix queue counters */
      q->q_len--;
#if IP_DEBUG_Q
      if (q->q_len < q->q_min) {
        q->q_min = q->q_len;
      }
#endif
      goto Done;
    }
    qlast = qptr;
    qptr = qptr->qe_next;
  }
  IP_PANIC("Element not in Queue!");      // This should never happen.
Done:
  EXIT_CRIT_SECTION();   /* restore int state */
}


/*************************** End of file ****************************/

