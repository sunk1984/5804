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
File    : IP_Q.h
Purpose : Include file for Queues.
--------  END-OF-HEADER  ---------------------------------------------
*/

/* Additional Copyrights: */
/* Copyright  2000 By InterNiche Technologies Inc. All rights reserved */
/* Portions Copyright 1990-1994 by NetPort 10/13/90
 * Portions Copyright 1986 by Carnegie Mellon
 * Portions Copyright 1983 by the Massachusetts Institute of
 * Technology
 */

#ifndef _Q_H_
#define  _Q_H_ 1

#if defined(__cplusplus)
extern "C" {     /* Make sure we have C-declarations in C++ programs */
#endif

typedef struct q_elt {     /* queue element: cast to right type */
   struct   q_elt   *   qe_next; /* it's just a pointer to next elt */
} QUEUE_ITEM;

typedef struct   queue {        /* queue header */
   QUEUE_ITEM * q_head;        /* first element in queue */
   QUEUE_ITEM * q_tail;        /* last element in queue */
   int          q_len;         /* number of elements in queue */
#if IP_DEBUG_Q
   int          q_max;         /* maximum length */
   int          q_min;         /* minimum length */
#endif
} QUEUE;

void     IP_Q_Add              (QUEUE*, void*);    // Add item
void *   IP_Q_GetRemoveFirst   (QUEUE*);           // Get and remove first item
void *   IP_Q_GetFirst         (QUEUE*);           // Get first item. Does NOT remove it from queue.
void     IP_Q_RemoveItem       (QUEUE * q, void * elt);
void *   IP_Q_TryGetRemoveFirst(QUEUE * q);

/*********************************************************************
*
*       MACROs for higher performance
*/
#if IP_DEBUG || (IP_PTR_OP_IS_ATOMIC == 0)
  #define IP_Q_GET_FIRST(pQ) IP_Q_GetFirst(pQ)
#else
  #define IP_Q_GET_FIRST(pQ) (pQ)->q_head
#endif

#if defined(__cplusplus)
  }
#endif

#endif   /* _Q_H_ */


