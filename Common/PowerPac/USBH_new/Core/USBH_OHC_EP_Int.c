/*********************************************************************
*  
*   IAR PowerPac
*
*   (c) Copyright IAR Systems 2010.  All rights reserved.
*
**********************************************************************
----------------------------------------------------------------------
File        : USBH_OHC_EP_Int.c
Purpose     : USB host implementation
---------------------------END-OF-HEADER------------------------------
*/

/*********************************************************************
*
*       #include Section
*
**********************************************************************
*/

#include <stdlib.h>
#include "USBH_Int.h"

/*********************************************************************
*
*       _GetDummyInt
*
*  Function description
*    Returns an dummy interrupt endpoint from pool.
*/
static USBH_OHCI_DUMMY_INT_EP * _GetDummyInt(USBH_HCM_POOL * EpPool) {
  USBH_OHCI_DUMMY_INT_EP * item;
  item             = (USBH_OHCI_DUMMY_INT_EP *)USBH_HCM_GetItem(EpPool);
  if (NULL == item) {
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: _GetDummyInt: no resources!"));
  }
  USBH_ASSERT(USBH_IS_ALIGNED(item->ItemHeader.PhyAddr, OH_ED_ALIGNMENT));
  return item;
}


/*********************************************************************
*
*       _InsertIntoDummyIntList
*
*  Function description
*    Links two dummy endpoints
*/
static void _InsertIntoDummyIntList(USBH_OHCI_DUMMY_INT_EP * Ep, USBH_OHCI_DUMMY_INT_EP * NextEp) {
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: INFO _InsertIntoDummyIntList: interval: %u ms next: %u ms!", Ep->IntervalTime, NextEp->IntervalTime));
  Ep->pNextDummyEp = NextEp;
  Ep->State       = OH_EP_LINK;
  USBH_OHCI_EpGlobLinkEds(&Ep->ItemHeader, &NextEp->ItemHeader);
}

/*********************************************************************
*
*       _InitDummyInt
*
*  Function description
*    Initialize an interrupt dummy endpoint
*/
static void _InitDummyInt(USBH_OHCI_DUMMY_INT_EP * Ep, USBH_OHCI_DEVICE * Dev, U16 IntervalTime) {
  USBH_HCM_ITEM_HEADER * item;
  USBH_BOOL            SkipFlag;
  USBH_OCHI_IS_DEV_VALID(Dev);
  Ep->EndpointType = USB_EP_TYPE_INT;
  Ep->pDev          = Dev;
  USBH_DLIST_Init(&Ep->ActiveList);
  Ep->pNextDummyEp  = NULL;
  Ep->State        = OH_EP_IDLE;
  Ep->Mask         = OH_DUMMY_ED_EPFLAG;
  Ep->IntervalTime = IntervalTime;
  Ep->Bandwidth    = 0;
  SkipFlag         = TRUE; // Skip the endpoint no TD is inserted
  item             = &Ep->ItemHeader;
  // Init DWORD 0..DWORD 3 in the ED no TD is linked do this Ep
  USBH_OHCI_EpGlobInitED(item, OH_DEFAULT_DEV_ADDR, OH_DEFAULT_EP_ADDR, OH_DEFAULT_MAX_PKT_SIZE, FALSE, SkipFlag, OH_DEFAULT_SPEED);
}

/*********************************************************************
*
*       _RemoveUserEDFromPhysicalLink
*
*  Function description
*    Set the Skip bit and removes the ED from physical link. The ED remains
*    in the logical list to get access in the timer routine to delete them.
*    During the time of one frame it can be that the HC has access to that endpoint!
*/
static void _RemoveUserEDFromPhysicalLink(USBH_OHCI_DEVICE * pDev, USBH_OHCI_BULK_INT_EP * Ep) {
  USBH_OHCI_BULK_INT_EP  * pPreviousEp;
  USBH_OHCI_DUMMY_INT_EP * pDummyEp;
  USBH_DLIST            * pPreviousEntry;

  USBH_ASSERT(pDev->IntEpCount);
  USBH_ASSERT(Ep->State != OH_EP_IDLE);
  pDummyEp = Ep->pDummyIntEp;
  USBH_ASSERT_MAGIC(&pDummyEp->ItemHeader, USBH_HCM_ITEM_HEADER);
  USBH_ASSERT(!USBH_DLIST_IsEmpty(&pDummyEp->ActiveList));
  USBH_USE_PARA(pDev);
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: INFO _RemoveUserEDFromPhysicalLink: remove Ep 0x%x pDev: %d!", (int)Ep->EndpointAddress, (int)Ep->DeviceAddress));
  // 1. Mark the endpoint as removed with OH_EP_UNLINK
  // 2. Skip ED from list processing
  // 3. Remove from the physical list: set the next pointer for the previous ED to the next pointer of endpoint that must be deleted,
  //    do not remove the endpoint for the logical dummy endpoint active list, because a logical remove list does not exist!
  USBH_OHCI_EpGlobSetSkip(&Ep->ItemHeader);             // Skip endpoint from list processing
  pPreviousEntry = USBH_DLIST_GetPrev(&Ep->ListEntry);
  if (pPreviousEntry == &pDummyEp->ActiveList) {  // Previous link is a dummy EP
    USBH_OHCI_EpGlobUnlinkEd(&pDummyEp->ItemHeader, &Ep->ItemHeader);
  } else {                                      // Previous EP is not an dummy EP
    pPreviousEp = GET_BULKINT_EP_FROM_ENTRY(pPreviousEntry);
    OH_BULKINT_VALID(pPreviousEp);
    USBH_OHCI_EpGlobUnlinkEd(&pPreviousEp->ItemHeader, &Ep->ItemHeader);
  }
#if (USBH_DEBUG > 1)
  if (pDev->IntRemoveFlag) { // IntRemoveFlag only for debug
    USBH_LOG((USBH_MTYPE_OHCI, "INFO _RemoveUserEDFromPhysicalLink: More as one endpoint deleted within one frame time!"));
  }
  pDev->IntRemoveFlag = TRUE;
#endif
}

/*********************************************************************
*
*       Globals
*
**********************************************************************
*/


/*********************************************************************
*
*       USBH_OHCI_INT_PutAllDummyEp
*
*  Function description
*    Releases all dummy interrupts from the devices array
*/
void USBH_OHCI_INT_PutAllDummyEp(USBH_OHCI_DEVICE * pDev) {
  int                i;
  USBH_OHCI_DUMMY_INT_EP * dummyEp;
  /* 1. Get an dummy endpoint from pool and build a list */
  for (i = 0; i < OHD_DUMMY_INT_NUMBER; i++) {
    if (pDev->DummyInterruptEpArr[i] == NULL) {
      continue;
    }
    dummyEp                     = pDev->DummyInterruptEpArr[i];
    pDev->DummyInterruptEpArr[i] = NULL;
    USBH_HCM_PutItem(&dummyEp->ItemHeader);
  }
}


/*********************************************************************
*
*       USBH_OHCI_INT_RemoveEDFromLogicalListAndFree
*
*  Function description
*   Removes endpoints where the state is OH_EP_UNLINK.
*
*  Parameters
*    all:     Remove also endpoints where only the OH_EP_UNLINK_TIMER flag is set
*
*  Return value:
*    FALSE:   -
*    TRUE:    Any endpoint has the state OH_EP_UNLINK_TIMER.
*
*  If the function is called in the timer routine context and true is
*  returned the hIntEpRemoveTimer timer must be started.
*/
USBH_BOOL USBH_OHCI_INT_RemoveEDFromLogicalListAndFree(USBH_OHCI_DEVICE * pDev, USBH_BOOL all) {
  USBH_DLIST                           * pEntry;
  int                               i;
  USBH_OHCI_BULK_INT_EP                 * remove_ep;
  USBH_OHCI_DUMMY_INT_EP                * dummy_ep;
  USBH_DLIST                             removeList;
  USBH_RELEASE_EP_COMPLETION_FUNC * pfCompletion;
  void                            * pContext;
  USBH_BOOL                         start_timer = FALSE;

  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: USBH_OHCI_INT_RemoveEDFromLogicalListAndFree!"));
  USBH_DLIST_Init(&removeList); // Remove all Eps and put it to removeList
  for (i = 0; i < OHD_DUMMY_INT_NUMBER; i++) {
    dummy_ep = pDev->DummyInterruptEpArr[i];
    if (dummy_ep == NULL) {
      continue;
    }
    pEntry = USBH_DLIST_GetNext(&dummy_ep->ActiveList);
    // Clear only endpoints where the UNLINK flag is set
    while (pEntry != &dummy_ep->ActiveList) {
      remove_ep = GET_BULKINT_EP_FROM_ENTRY(pEntry);
      OH_BULKINT_VALID(remove_ep);
      pEntry = USBH_DLIST_GetNext(pEntry);
      if (remove_ep->State == OH_EP_UNLINK || (all && remove_ep->State == OH_EP_UNLINK_TIMER)) {
        dummy_ep = remove_ep->pDummyIntEp;
        USBH_ASSERT(!USBH_DLIST_IsEmpty(&dummy_ep->ActiveList));
        USBH_ASSERT(dummy_ep->Bandwidth >= remove_ep->MaxPacketSize);
        USBH_ASSERT(pDev->IntEpCount);
        USBH_DLIST_RemoveEntry(&remove_ep->ListEntry);             // Release the endpoint from the logical list
        remove_ep->State     = OH_EP_IDLE;                   // OH_EP_IDLE marks the endpoint as removed
        dummy_ep->Bandwidth -= remove_ep->MaxPacketSize;
        // Decrement common counter
        pDev->IntEpCount--;
        if (0==pDev->IntEpCount) {
          USBH_LOG((USBH_MTYPE_OHCI, "OHCI: INFO USBH_OHCI_INT_RemoveEDFromLogicalListAndFree: All user interrupt endpoints are removed!"));
        }
        USBH_DLIST_InsertHead(&removeList,&remove_ep->ListEntry);  // Insert to the remove list before freed
      } else if (remove_ep->State == OH_EP_UNLINK_TIMER) {
        remove_ep->State = OH_EP_UNLINK;
        start_timer      = TRUE;
      }
    }
  }
#if (USBH_DEBUG > 1)
  pDev->IntRemoveFlag = FALSE;                                // Only for debug
#endif
  if ( start_timer ) {
    pDev->IntEpRemoveTimerRunFlag=TRUE;
  }
  // Free the endpoints
  while (!USBH_DLIST_IsEmpty(&removeList)) {
    USBH_DLIST_RemoveHead(&removeList, &pEntry);
    remove_ep = GET_BULKINT_EP_FROM_ENTRY(pEntry);
    OH_BULKINT_VALID(remove_ep);
    pfCompletion = remove_ep->pfOnReleaseCompletion;
    pContext     = remove_ep->pReleaseContext;
    // Put it back to the pool
    USBH_LOG((USBH_MTYPE_OHCI, "OHCI: INFO USBH_OHCI_INT_RemoveEDFromLogicalListAndFree: Put Ep 0x%x back to the pool!", remove_ep->EndpointAddress));
    USBH_OHCI_BULK_INT_PutEp(remove_ep);
    // If the last endpoint is deleted and the bus driver host reference counter is zero
    // then USBH_HostExit() is called and and the device is invalid
    if (pfCompletion) {
      pfCompletion(pContext);
    }
  }
  return start_timer;
}

/*********************************************************************
*
*       USBH_OHCI_INT_OnReleaseEpTimer
*
*  Function description
*    Timer callback routine releases all user interrupt endpoints
*/
void USBH_OHCI_INT_OnReleaseEpTimer(void * pContext) {
  USBH_OHCI_DEVICE         * pDev;
  USBH_ASSERT(pContext != NULL);
  pDev               = (USBH_OHCI_DEVICE *)pContext;
  USBH_OCHI_IS_DEV_VALID(pDev);

  if (pDev->IntEpRemoveTimerCancelFlag) {
    pDev->IntEpRemoveTimerCancelFlag = FALSE;
    return;
  }
  pDev->IntEpRemoveTimerRunFlag = FALSE;
  if (USBH_OHCI_INT_RemoveEDFromLogicalListAndFree(pDev, FALSE)){
    // The timer run flag must set earlier in USBH_OHCI_INT_RemoveEDFromLogicalListAndFree
    pDev->IntEpRemoveTimerCancelFlag = FALSE;
    USBH_StartTimer(pDev->hIntEpRemoveTimer, OH_STOP_DELAY_TIME);
  }
}

/*********************************************************************
*
*       USBH_OHCI_INT_OnAbortUrbTimer
*
*  Function description
*    Timer callback routine cancel all URBs
*/
void USBH_OHCI_INT_OnAbortUrbTimer(void * pContext) {
  USBH_OHCI_DEVICE   * pDev;
  int                i;
  USBH_OHCI_DUMMY_INT_EP * ep;
  USBH_BOOL             restart_flag;
  USBH_DLIST            * pEntry;
  USBH_OHCI_BULK_INT_EP  * activeEp;
  USBH_STATUS        status;
  USBH_BOOL             start_timer = FALSE;
  USBH_ASSERT(pContext != NULL);
  pDev = (USBH_OHCI_DEVICE *)pContext;
  USBH_OCHI_IS_DEV_VALID(pDev);
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: USBH_OHCI_INT_OnAbortUrbTimer!"));
  if ( pDev->IntEpAbortTimerCancelFlag ) {
    pDev->IntEpAbortTimerCancelFlag = FALSE;
    return;
  }
  pDev->IntEpAbortTimerRunFlag = FALSE;
  // Walk through all dummy endpoints
  for (i = 0; i < OHD_DUMMY_INT_NUMBER; i++) {
    // Get active interrupt endpoints
    ep = pDev->DummyInterruptEpArr[i];
    if (ep == NULL) {
      USBH_WARN((USBH_MTYPE_OHCI, "OHCI: WARNIUNG USBH_OHCI_INT_OnAbortUrbTimer: invalid DummyInterruptEpArr index: %d!",i));
      continue;
    }
    // Mark all aborted endpoints with EP_ABORT_SKIP_TIME_OVER_MASK mask
    pEntry = USBH_DLIST_GetNext(&ep->ActiveList);
    while (pEntry != &ep->ActiveList) {
      activeEp = GET_BULKINT_EP_FROM_ENTRY(pEntry);
      OH_BULKINT_VALID(activeEp);
      if (activeEp->AbortMask & EP_ABORT_MASK) {
        activeEp->AbortMask |= EP_ABORT_SKIP_TIME_OVER_MASK | EP_ABORT_PROCESS_FLAG_MASK;
      } else if (activeEp->AbortMask & EP_ABORT_START_TIMER_MASK) {
        activeEp->AbortMask |= EP_ABORT_MASK;
        start_timer = TRUE;
      }
      pEntry = USBH_DLIST_GetNext(pEntry);
    }
    if (start_timer) {
      // The timer run flag prevents an start of this timer if an control endpoint in the
      // abort completion routines is aborted. The timer is started at the end of this routine.
      pDev->IntEpAbortTimerRunFlag = TRUE;
    }
    // Cancel all URBs
    restart_flag = TRUE;
    while (restart_flag) {
      // Search always from the beginning of the list endpoints where the EP_ABORT_SKIP_TIME_OVER_MASK mask is set
      restart_flag = FALSE;
      pEntry        = USBH_DLIST_GetNext(&ep->ActiveList);
      while (pEntry != &ep->ActiveList) {
        activeEp = GET_BULKINT_EP_FROM_ENTRY(pEntry);
        pEntry    = USBH_DLIST_GetNext(pEntry);
        OH_BULKINT_VALID(activeEp);
        if ((activeEp->AbortMask & EP_ABORT_PROCESS_FLAG_MASK)) { // Aborted endpoint
          activeEp->AbortMask &=~EP_ABORT_PROCESS_FLAG_MASK;
#if (USBH_DEBUG > 1)
          if (activeEp->pPendingUrb) {
            // Because the interrupt list is not stopped the endpoint must be halted or the ski bit must be on
            USBH_ASSERT(USBH_OHCI_EpGlobIsHalt(&activeEp->ItemHeader) ||  USBH_OHCI_EpGlobIsSkipped(&activeEp->ItemHeader)) ;
          }
#endif
          status = USBH_OHCI_BULK_INT_CheckAndCancelAbortedUrbs(activeEp, FALSE);
          // If the Urb is completed start with the beginning of the endpoint list, because
          // the completion routine can added an new endpoint. (removing of an endpoint has always an duration)
          if (!status) { // On success
            restart_flag = TRUE;
            break;
          }
        }
      }
    }
  }
  if (start_timer) {
    pDev->IntEpAbortTimerCancelFlag = FALSE;
    USBH_StartTimer(pDev->hIntEpAbortTimer, OH_STOP_DELAY_TIME);
  }
}

/*********************************************************************
*
*       USBH_OHCI_INT_InitAllocDummyIntEps
*
*  Function description
*    Allocates needed dummy interrupt items form the pool and writes this
*    to the devices dummy interrupt array. Later this allocated endpoints
*    are initialized with _InitDummyInt()
*/
USBH_STATUS USBH_OHCI_INT_InitAllocDummyIntEps(USBH_OHCI_DEVICE * pDev) {
  int                i;
  USBH_OHCI_DUMMY_INT_EP * dummyEp;
  USBH_STATUS        status;
  status           = USBH_STATUS_SUCCESS;
  USBH_ZERO_ARRAY(pDev->DummyInterruptEpArr);  // Writes NULL pointer in the array
  // Get a dummy endpoint from pool and build a list
  for (i = 0; i < OHD_DUMMY_INT_NUMBER; i++) {
    dummyEp = _GetDummyInt(&pDev->DummyIntEPPool);
    if (dummyEp == NULL) {
      USBH_WARN((USBH_MTYPE_OHCI, "OHCI: OhdBuildAndSetInterruptTable: No resources!"));
      // Release all allocated pool elements
      USBH_OHCI_INT_PutAllDummyEp(pDev);
      status = USBH_STATUS_RESOURCES;
      break;
    }
    pDev->DummyInterruptEpArr[i] = dummyEp;
  }
  return status;
}

/*********************************************************************
*
*       USBH_OHCI_INT_DummyEpAllocPool
*
*  Function description
*/
USBH_STATUS USBH_OHCI_INT_DummyEpAllocPool(USBH_HCM_POOL * EpPool) {
  USBH_STATUS status;
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: OhDummyIntEpAlloc: Eps:%d!",OHD_DUMMY_INT_NUMBER));
  status = USBH_HCM_AllocPool(EpPool, OHD_DUMMY_INT_NUMBER, OH_ED_SIZE, sizeof(USBH_OHCI_DUMMY_INT_EP), OH_ED_ALIGNMENT);
  if ( status ) { // On error
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: OhDummyIntEpAllocPool: USBH_HCM_AllocPool!"));
  }
  return status;
}

/*********************************************************************
*
*       USBH_OHCI_INT_BuildDummyEpTree
*
*  Function description
*    Links all dummy endpoint with the next dummy endpoint.
*    Start with 32 ms endpoints up to the 1ms last endpoint.
*/
void USBH_OHCI_INT_BuildDummyEpTree(USBH_OHCI_DEVICE * pDev) {
  // The initialization starts with interval 32ms, epListIdxNextInterval
  // contains the index for 16ms and so on
  int                epInterval, epNextIntervalIdx;
  int                i, j, numberEps;
  USBH_OHCI_DUMMY_INT_EP * ep;
  USBH_OHCI_DUMMY_INT_EP * nextIntervalEp;
  numberEps        = 0;
  // Start with the 32 ms level, breaks the loop if have the 1ms interval
  for (epInterval = 32; epInterval > 0; epInterval /= 2) {
    if (1 == epInterval) { // Last 1ms endpoint
      epNextIntervalIdx = 0;
      nextIntervalEp = NULL;
    } else {
      epNextIntervalIdx = ((epInterval) / 2) - 1;
      USBH_ASSERT(epNextIntervalIdx < USBH_ARRAY_ELEMENTS(pDev->DummyInterruptEpArr));
      nextIntervalEp = pDev->DummyInterruptEpArr[epNextIntervalIdx];
    }
    USBH_LOG((USBH_MTYPE_OHCI, "OHCI: USBH_OHCI_INT_BuildDummyEpTree: current interval: %d next interval: %d!", epInterval, epNextIntervalIdx+1));
    // Initialize all dummy endpoints for that interval. The amoung of intervals is identical to the
    // interval (32ms = 32 dummy endpoints, nextIntervalEp points to the first endpoint in the next interval
    for (j = 0, i = 0; i < epInterval; i++) {
      if (numberEps >= OHD_DUMMY_INT_NUMBER) {
        USBH_WARN((USBH_MTYPE_OHCI, "OHCI: OhdBuildInterruptTree: too many endpoints!"));
        goto exit;
      }
      USBH_ASSERT((epInterval-1+i) < USBH_ARRAY_ELEMENTS(pDev->DummyInterruptEpArr));
      ep = pDev->DummyInterruptEpArr[epInterval - 1 + i];
      if (ep == NULL) {
        USBH_WARN((USBH_MTYPE_OHCI, "OHCI: OhdBuildInterruptTree: dummy endpoint is NULL!"));
        return;
      }
      USBH_HCM_ASSERT_ITEM_HEADER(&ep->ItemHeader);
      _InitDummyInt(ep, pDev, (U16)(epInterval)); // Init the dummy endpoint
      if (NULL == nextIntervalEp) {
        break;
      }
      _InsertIntoDummyIntList(ep, nextIntervalEp);

      if ( i & 0x01 ) { // Increment ep in current level
        j++;            // Get next intervalEp on every second endpoint
        USBH_ASSERT((epNextIntervalIdx+j) < USBH_ARRAY_ELEMENTS(pDev->DummyInterruptEpArr));
        nextIntervalEp = pDev->DummyInterruptEpArr[epNextIntervalIdx + j];
        if (nextIntervalEp == NULL) {
          USBH_WARN((USBH_MTYPE_OHCI, "OHCI: OhdBuildInterruptTree: nextIntervalEp is NULL!"));
          return;
        }
      }
      numberEps++;
    }
  }
exit:;
}

/*********************************************************************
*
*       USBH_OHCI_INT_GetBandwidth
*
*  Function description
*    USBH_OHCI_INT_GetBandwidth determine which dummy endpoint with an certain interval
*    has the minimum bandwidth.
*
*  Parameters:
*    pDev:          Valid device
*    intervalTime: in ms or 0
*    Ep:           valid ptr or NULL : OUT: Pointer to the dummy endpoint
*  Return value:
*    Maximum bytes that has to be transfer in one frame.
*    This is the maximum interrupt bandwidth used on the USB bus!
*/
U32 USBH_OHCI_INT_GetBandwidth(USBH_OHCI_DEVICE * pDev, U16 intervalTime, USBH_OHCI_DUMMY_INT_EP ** Ep) {
  int                 frameNb;
  U32                 bandWidth;
  U32                 minBandWidth;
  U32                 maxBandWidth = 0;
  USBH_OHCI_DUMMY_INT_EP  * dummyEp;
  USBH_OHCI_DUMMY_INT_EP  * intervalEp;
  USBH_OHCI_DUMMY_INT_EP  * minBandwidthEp;
  int                 minBandWidthFrameNb = 0;
  int                 maxBandWidthFrameNb = 0;

  USBH_ASSERT(Ep != NULL);
  USBH_OCHI_IS_DEV_VALID(pDev);
  USBH_USE_PARA(minBandWidthFrameNb);
  USBH_USE_PARA(maxBandWidthFrameNb);
  // First search an dummy endpoint where the sum of all endpoints has a minimum.
  minBandwidthEp = NULL;
  minBandWidth   = ~((U32)0);
  for ( frameNb=0;frameNb<32;frameNb++) {
    // Search for all frame numbers line by line in the interrupt endpoint tree, Start with the 32ms interval
    USBH_ASSERT((31+frameNb) < USBH_ARRAY_ELEMENTS(pDev->DummyInterruptEpArr));
    dummyEp    = pDev->DummyInterruptEpArr[31+frameNb];
    intervalEp = NULL;
    bandWidth  = 0;
    // Walk through all dummy endpoints and adds the bandwidth of the endpoints
    while (dummyEp != NULL) {
      USBH_HCM_ASSERT_ITEM_HEADER(&dummyEp->ItemHeader);
      bandWidth += dummyEp->Bandwidth;
      if (dummyEp->IntervalTime == intervalTime) {
        intervalEp = dummyEp;
      }
      dummyEp = (USBH_OHCI_DUMMY_INT_EP *)dummyEp->pNextDummyEp;
    }
    if (bandWidth < minBandWidth) {
      minBandWidth        = bandWidth;
      minBandwidthEp      = intervalEp;
      minBandWidthFrameNb = frameNb;
    }
    if (maxBandWidth < bandWidth) {
      maxBandWidth        = bandWidth;
      maxBandWidthFrameNb = frameNb;
    }
  }
  if (Ep != NULL) {
    *Ep = minBandwidthEp;
  }  else {
    *Ep = NULL;
  }
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: USBH_OHCI_INT_GetBandwidth min: frameNb: %d bandwitdh: %d  max: framenb: %d bandwidth: %d !",
                          minBandWidthFrameNb, minBandWidth, maxBandWidthFrameNb, maxBandWidth));
  return maxBandWidth;
}

/*********************************************************************
*
*       USBH_OHCI_INT_InsertEp
*
*  Function description
*    Insert an interrupt endpoint in the HC link list and in the device
*    link list. All active endpoints are appended to an dummy endpoint
*    that has a specified frame interval. Every dummy endpoint counts
*    the number of inserted active endpoints. An new endpoint is inserted
*    on such dummy endpoints where the number of active endpoints in the
*    tree has a minimum.
*/
USBH_STATUS USBH_OHCI_INT_InsertEp(USBH_OHCI_BULK_INT_EP * Ep) {
  USBH_OHCI_DEVICE   * pDev;
  USBH_OHCI_DUMMY_INT_EP * pMinBandwidthEp;

  USBH_ASSERT(Ep != NULL);
  pDev = Ep->pDev;
  USBH_OCHI_IS_DEV_VALID(pDev);
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: INFO USBH_OHCI_INT_InsertEp Ep:0x%x pDev:%d!", Ep->EndpointAddress, (int)Ep->DeviceAddress));
  USBH_OHCI_INT_GetBandwidth(pDev, Ep->IntervalTime, &pMinBandwidthEp);
  if (pMinBandwidthEp == NULL) {
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: USBH_OHCI_INT_InsertEp: USBH_OHCI_INT_GetBandwidth: not endpoint found!"));
    return USBH_STATUS_ERROR;
  }
  // Insert Ep of the returned dummy endpoint user endpoint list
  Ep->State = OH_EP_LINK;
  USBH_OHCI_EpGlobLinkEds(&pMinBandwidthEp->ItemHeader, &Ep->ItemHeader);
  USBH_DLIST_InsertHead(&pMinBandwidthEp->ActiveList, &Ep->ListEntry);
  // Link the new user endpoint to an dummy endpoint that has the minimum bandwidth
  Ep->pDummyIntEp = pMinBandwidthEp;
  // Adds the packet size for this interval
  pMinBandwidthEp->Bandwidth += Ep->MaxPacketSize;
  pDev->IntEpCount++;
  return USBH_STATUS_SUCCESS;
}

/*********************************************************************
*
*       USBH_OHCI_INT_RemoveAllUserEDFromPhysicalLink
*
*  Function description
*/
void USBH_OHCI_INT_RemoveAllUserEDFromPhysicalLink(USBH_OHCI_DEVICE * pDev) {
  USBH_DLIST            * pEntry;
  int                i;
  USBH_OHCI_BULK_INT_EP  * pRemoveEp;
  USBH_OHCI_DUMMY_INT_EP * pDummyEp;
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: USBH_OHCI_INT_RemoveAllUserEDFromPhysicalLink!"));
  for (i = 0; i < OHD_DUMMY_INT_NUMBER; i++) {
    pDummyEp = pDev->DummyInterruptEpArr[i];
    if (pDummyEp == NULL) {
      continue;
    }
    pEntry = USBH_DLIST_GetNext(&pDummyEp->ActiveList);
    // Remove all endpoints where the UNLINK flag is not set
    while (pEntry != &pDummyEp->ActiveList) {
      pRemoveEp = GET_BULKINT_EP_FROM_ENTRY(pEntry);
      OH_BULKINT_VALID(pRemoveEp);
      pEntry = USBH_DLIST_GetNext(pEntry);
      if (pRemoveEp->State != OH_EP_UNLINK && pRemoveEp->State != OH_EP_UNLINK_TIMER) {
        _RemoveUserEDFromPhysicalLink(pDev, pRemoveEp); // Unlink all not phys. unlinked endpoints
      }
    }
  }
}

/*********************************************************************
*
*       USBH_OHCI_INT_ReleaseEp
*
*  Function description
*    Releases an interrupt endpoint without stopping the interrupt list.
*    0. No request should be pending
*    1. Set the Skip bit
*    2. The pointer from the previous endpoint is set to the next endpoint
*    3. An timer is started for two milliseconds
*    4. If the timer is processed the endpoint is put back to the pool
*/
void USBH_OHCI_INT_ReleaseEp(USBH_OHCI_BULK_INT_EP * pEp, USBH_RELEASE_EP_COMPLETION_FUNC * pfReleaseEpCompletion, void * pContext) {
  USBH_OHCI_DEVICE * pDev;

  OH_BULKINT_VALID(pEp);
  pDev = pEp->pDev;
  USBH_OCHI_IS_DEV_VALID(pDev);
  USBH_ASSERT(USBH_DLIST_IsEmpty(&pEp->UrbList) );
  USBH_ASSERT(pEp->pPendingUrb == NULL);
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: INFO USBH_OHCI_INT_ReleaseEp pEp: 0x%x pDev: %d!", pEp->EndpointAddress, pEp->DeviceAddress));
  if (pEp->State == OH_EP_UNLINK || pEp->State==OH_EP_UNLINK_TIMER) {
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: USBH_OHCI_Ep0_ReleaseEndpoint: Endpoint already unlinked, return!"));
    return;
  }
  pEp->pReleaseContext    = pContext;
  pEp->pfOnReleaseCompletion = pfReleaseEpCompletion;
  _RemoveUserEDFromPhysicalLink(pDev, pEp); // The endpoint has only logical no physical link
  if(!pDev->IntEpRemoveTimerRunFlag){
    pDev->IntEpRemoveTimerRunFlag    = TRUE;
    pDev->IntEpRemoveTimerCancelFlag = FALSE;
    pEp->State                       = OH_EP_UNLINK;
    USBH_StartTimer(pDev->hIntEpRemoveTimer, OH_STOP_DELAY_TIME);
  } else {
    pEp->State = OH_EP_UNLINK_TIMER;
  }
}

/******************************* EOF ********************************/
