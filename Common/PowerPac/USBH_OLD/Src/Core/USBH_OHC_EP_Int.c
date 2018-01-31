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
*       OhIntDummyGet
*
*  Function description
*    Returns an dummy interrupt endpoint from pool.
*/
static OHD_DUMMY_INT_EP * OhIntDummyGet(HCM_POOL * EpPool) {
  OHD_DUMMY_INT_EP * item;
  item             = (OHD_DUMMY_INT_EP *)HcmGetItem(EpPool);
  if (NULL == item) {
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: OhIntDummyGet: no resources!"));
  }
  T_ASSERT(TB_IS_ALIGNED(item->ItemHeader.PhyAddr, OH_ED_ALIGNMENT));
  return item;
}

/*********************************************************************
*
*       OhIntPutAllDummyEp
*
*  Function description
*    Releases all dummy interrupts from the devices array
*/
void OhIntPutAllDummyEp(HC_DEVICE * Dev) {
  int                i;
  OHD_DUMMY_INT_EP * dummyEp;
  /* 1. Get an dummy endpoint from pool and build a list */
  for (i = 0; i < OHD_DUMMY_INT_NUMBER; i++) {
    if (Dev->DummyInterruptEpArr[i] == NULL) {
      continue;
    }
    dummyEp                     = Dev->DummyInterruptEpArr[i];
    Dev->DummyInterruptEpArr[i] = NULL;
    HcmPutItem(&dummyEp->ItemHeader);
  }
}

/*********************************************************************
*
*       OhDummyIntInsertList
*
*  Function description
*    Links two dummy endpoints
*/
static void OhDummyIntInsertList(OHD_DUMMY_INT_EP * Ep, OHD_DUMMY_INT_EP * NextEp) {
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: INFO OhDummyIntInsertList: interval: %u ms next: %u ms!", Ep->IntervalTime, NextEp->IntervalTime));
  Ep->NextDummyEp = NextEp;
  Ep->State       = OH_EP_LINK;
  OhEpGlobLinkEds(&Ep->ItemHeader, &NextEp->ItemHeader);
}

/*********************************************************************
*
*       OhDummyIntInit
*
*  Function description
*    Initialize an interrupt dummy endpoint
*/
static void OhDummyIntInit(OHD_DUMMY_INT_EP * Ep, struct T_HC_DEVICE * Dev, U16 IntervalTime) {
  HCM_ITEM_HEADER * item;
  T_BOOL            SkipFlag;
  OH_DEV_VALID(Dev);
  Ep->EndpointType = USB_EP_TYPE_INT;
  Ep->Dev          = Dev;
  DlistInit(&Ep->ActiveList);
  Ep->NextDummyEp  = NULL;
  Ep->State        = OH_EP_IDLE;
  Ep->Mask         = OH_DUMMY_ED_EPFLAG;
  Ep->IntervalTime = IntervalTime;
  Ep->Bandwidth    = 0;
  SkipFlag         = TRUE; // Skip the endpoint no TD is inserted
  item             = &Ep->ItemHeader;
  // Init DWORD 0..DWORD 3 in the ED no TD is linked do this Ep
  OhEpGlobInitED(item, OH_DEFAULT_DEV_ADDR, OH_DEFAULT_EP_ADDR, OH_DEFAULT_MAX_PKT_SIZE, FALSE, SkipFlag, OH_DEFAULT_SPEED);
}

/*********************************************************************
*
*       OhInt_RemoveUserEDFromPhysicalLink
*
*  Function description
*    Set the Skip bit and removes the ED from physical link. The ED remains
*    in the logical list to get access in the timer routine to delete them.
*    During the time of one frame it can be that the HC has access to that endpoint!
*/
static void OhInt_RemoveUserEDFromPhysicalLink(HC_DEVICE * dev, OHD_BULK_INT_EP * Ep) {
  OHD_BULK_INT_EP  * previousEp;
  OHD_DUMMY_INT_EP * dummyEp;
  PDLIST             previousEntry;
  T_ASSERT(dev->IntEpCount);
  T_ASSERT(Ep->State != OH_EP_IDLE);
  dummyEp = Ep->DummyIntEp;
  T_ASSERT_MAGIC(&dummyEp->ItemHeader, HCM_ITEM_HEADER);
  T_ASSERT(!DlistEmpty(&dummyEp->ActiveList));
  UNUSED_PARAM(dev);
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: INFO OhInt_RemoveUserEDFromPhysicalLink: remove Ep 0x%x Dev: %d!", (int)Ep->EndpointAddress, (int)Ep->DeviceAddress));
  // 1. Mark the endpoint as removed with OH_EP_UNLINK
  // 2. Skip ED from list processing
  // 3. Remove from the physical list: set the next pointer for the prevoius ED to the next pointer of endpoint that must be deleted,
  //    do not remove the endpoint for the logical dummy endpoint active list, because a logical remove list does not exist!
  OhEpGlobSetSkip(&Ep->ItemHeader);             // Skip endpoint from list processing
  previousEntry = DlistGetPrev(&Ep->ListEntry);
  if (previousEntry == &dummyEp->ActiveList) {  // Previous link is a dummy EP
    OhEpGlobUnlinkEd(&dummyEp->ItemHeader, &Ep->ItemHeader);
  } else {                                      // Previous EP is not an dummy EP
    previousEp = GET_BULKINT_EP_FROM_ENTRY(previousEntry);
    OH_BULKINT_VALID(previousEp);
    OhEpGlobUnlinkEd(&previousEp->ItemHeader, &Ep->ItemHeader);
  }
#if (USBH_DEBUG > 1)
  if (dev->IntRemoveFlag) { // IntRemoveFlag only for debug
    USBH_LOG((USBH_MTYPE_OHCI, "INFO OhInt_RemoveUserEDFromPhysicalLink: More as one endpoint deleted within one frame time!"));
  }
  dev->IntRemoveFlag = TRUE;
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
*       OhIntRemoveEDFromLogicalListAndFree
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
*  returned the IntEpRemoveTimer timer must be started.
*/
T_BOOL OhIntRemoveEDFromLogicalListAndFree(HC_DEVICE * dev, T_BOOL all) {
  PDLIST                            entry;
  int                               i;
  OHD_BULK_INT_EP                 * remove_ep;
  OHD_DUMMY_INT_EP                * dummy_ep;
  DLIST                             removeList;
  USBH_RELEASE_EP_COMPLETION_FUNC * pfCompletion;
  void                            * pContext;
  T_BOOL                            start_timer = FALSE;

  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: OhIntRemoveEDFromLogicalListAndFree!"));
  DlistInit(&removeList); // Remove all Eps and put it to removeList
  for (i = 0; i < OHD_DUMMY_INT_NUMBER; i++) {
    dummy_ep = dev->DummyInterruptEpArr[i];
    if (dummy_ep == NULL) {
      continue;
    }
    entry = DlistGetNext(&dummy_ep->ActiveList);
    // Clear only endpoints where the UNLINK flag is set
    while (entry != &dummy_ep->ActiveList) {
      remove_ep = GET_BULKINT_EP_FROM_ENTRY(entry);
      OH_BULKINT_VALID(remove_ep);
      entry = DlistGetNext(entry);
      if (remove_ep->State == OH_EP_UNLINK || (all && remove_ep->State == OH_EP_UNLINK_TIMER)) {
        dummy_ep = remove_ep->DummyIntEp;
        T_ASSERT(!DlistEmpty(&dummy_ep->ActiveList));
        T_ASSERT(dummy_ep->Bandwidth >= remove_ep->MaxPktSize);
        T_ASSERT(dev->IntEpCount);
        DlistRemoveEntry(&remove_ep->ListEntry);             // Release the endpoint from the logical list
        remove_ep->State     = OH_EP_IDLE;                   // OH_EP_IDLE marks the endpoint as removed
        dummy_ep->Bandwidth -= remove_ep->MaxPktSize;
        // Decrement common counter
        dev->IntEpCount--;
        if (0==dev->IntEpCount) {
          USBH_LOG((USBH_MTYPE_OHCI, "OHCI: INFO OhIntRemoveEDFromLogicalListAndFree: All user interrupt endpoints are removed!"));
        }
        DlistInsertHead(&removeList,&remove_ep->ListEntry);  // Insert to the remove list before freed
      } else if (remove_ep->State == OH_EP_UNLINK_TIMER) {
        remove_ep->State = OH_EP_UNLINK;
        start_timer      = TRUE;
      }
    }
  }
#if (USBH_DEBUG > 1)
  dev->IntRemoveFlag = FALSE;                                // Only for debug
#endif
  if ( start_timer ) {
    dev->IntEpRemoveTimerRunFlag=TRUE;
  }
  // Free the endpoints
  while (!DlistEmpty(&removeList)) {
    DlistRemoveHead(&removeList, &entry);
    remove_ep = GET_BULKINT_EP_FROM_ENTRY(entry);
    OH_BULKINT_VALID(remove_ep);
    pfCompletion = remove_ep->ReleaseCompletion;
    pContext     = remove_ep->ReleaseContext;
    // Put it back to the pool
    USBH_LOG((USBH_MTYPE_OHCI, "OHCI: INFO OhIntRemoveEDFromLogicalListAndFree: Put Ep 0x%x back to the pool!", remove_ep->EndpointAddress));
    OhBulkIntEpPut(remove_ep);
    // If the last endpoint is deleted and the bus driver host reference counter is zero
    // then USBH_HostExit() is called and and the device is invalid
    if (pfCompletion ) {
      pfCompletion(pContext);
    }
  }
  return start_timer;
}

/*********************************************************************
*
*       OhIntReleaseEp_TimerCallback
*
*  Function description
*    Timer callback routine releases all user interrupt endpoints
*/
void OhIntReleaseEp_TimerCallback(void * Context) {
  HC_DEVICE         * dev;
  T_ASSERT(Context != NULL);
  dev               = (HC_DEVICE *)Context;
  OH_DEV_VALID(dev);

  if (dev->IntEpRemoveTimerCancelFlag) {
    dev->IntEpRemoveTimerCancelFlag = FALSE;
    return;
  }
  dev->IntEpRemoveTimerRunFlag = FALSE;
  if (OhIntRemoveEDFromLogicalListAndFree(dev, FALSE)){
    // The timer run flag must set earlier in OhIntRemoveEDFromLogicalListAndFree
    dev->IntEpRemoveTimerCancelFlag = FALSE;
    USBH_StartTimer(dev->IntEpRemoveTimer, OH_STOP_DELAY_TIME);
  }
}

/*********************************************************************
*
*       OhIntAbortUrb_TimerCallback
*
*  Function description
*    Timer callback routine cancel all URBs
*/
void OhIntAbortUrb_TimerCallback(void * Context) {
  HC_DEVICE        * dev;
  int                i;
  OHD_DUMMY_INT_EP * ep;
  T_BOOL             restart_flag;
  PDLIST             entry;
  OHD_BULK_INT_EP  * activeEp;
  USBH_STATUS        status;
  T_BOOL             start_timer = FALSE;
  T_ASSERT(Context != NULL);
  dev = (HC_DEVICE *)Context;
  OH_DEV_VALID(dev);
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: OhIntAbortUrb_TimerCallback!"));
  if ( dev->IntEpAbortTimerCancelFlag ) {
    dev->IntEpAbortTimerCancelFlag = FALSE;
    return;
  }
  dev->IntEpAbortTimerRunFlag = FALSE;
  // Walk through all dummy endpoints
  for (i = 0; i < OHD_DUMMY_INT_NUMBER; i++) {
    // Get active interrupt endpoints
    ep = dev->DummyInterruptEpArr[i];
    if (ep == NULL) {
      USBH_WARN((USBH_MTYPE_OHCI, "OHCI: WARNIUNG OhIntAbortUrb_TimerCallback: invalid DummyInterruptEpArr index: %d!",i));
      continue;
    }
    // Mark all aborted endpoints with EP_ABORT_SKIP_TIME_OVER_MASK mask
    entry = DlistGetNext(&ep->ActiveList);
    while (entry != &ep->ActiveList) {
      activeEp = GET_BULKINT_EP_FROM_ENTRY(entry);
      OH_BULKINT_VALID(activeEp);
      if (activeEp->AbortMask & EP_ABORT_MASK) {
        activeEp->AbortMask |= EP_ABORT_SKIP_TIME_OVER_MASK | EP_ABORT_PROCESS_FLAG_MASK;
      } else if (activeEp->AbortMask & EP_ABORT_START_TIMER_MASK) {
        activeEp->AbortMask |= EP_ABORT_MASK;
        start_timer = TRUE;
      }
      entry = DlistGetNext(entry);
    }
    if (start_timer) {
      // The timer run flag prevents an start of this timer if an control endpoint in the
      // abort completion routines is aborted. The timer is started at the end of this routine.
      dev->IntEpAbortTimerRunFlag = TRUE;
    }
    // Cancel all URBs
    restart_flag = TRUE;
    while (restart_flag) {
      // Search always from the beginning of the list endpoints where the EP_ABORT_SKIP_TIME_OVER_MASK mask is set
      restart_flag = FALSE;
      entry        = DlistGetNext(&ep->ActiveList);
      while (entry != &ep->ActiveList) {
        activeEp = GET_BULKINT_EP_FROM_ENTRY(entry);
        entry    = DlistGetNext(entry);
        OH_BULKINT_VALID(activeEp);
        if ((activeEp->AbortMask & EP_ABORT_PROCESS_FLAG_MASK)) { // Aborted endpoint
          activeEp->AbortMask &=~EP_ABORT_PROCESS_FLAG_MASK;
#if (USBH_DEBUG > 1)
          if (activeEp->PendingUrb) {
            // Because the interrupt list is not stopped the endpoint must be halted or the ski bit must be on
            T_ASSERT(OhEpGlobIsHalt(&activeEp->ItemHeader) ||  OhEpGlobIsSkipped(&activeEp->ItemHeader)) ;
          }
#endif
          status = OhBulkIntCheckAndCancelAbortedUrbs(activeEp, FALSE);
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
    dev->IntEpAbortTimerCancelFlag = FALSE;
    USBH_StartTimer(dev->IntEpAbortTimer, OH_STOP_DELAY_TIME);
  }
}

/*********************************************************************
*
*       OhInitAllocDummyInterruptEndpoints
*
*  Function description
*    Allocates needed dummy interrupt items form the pool and writes this
*    to the devices dummy interrupt array. Later this allocated endpoints
*    are initialized with OhDummyIntInit()
*/
USBH_STATUS OhInitAllocDummyInterruptEndpoints(HC_DEVICE * Dev) {
  int                i;
  OHD_DUMMY_INT_EP * dummyEp;
  USBH_STATUS        status;
  status           = USBH_STATUS_SUCCESS;
  ZERO_ARRAY(Dev->DummyInterruptEpArr);  // Writes NULL pointer in the array
  // Get a dummy endpoint from pool and build a list
  for (i = 0; i < OHD_DUMMY_INT_NUMBER; i++) {
    dummyEp = OhIntDummyGet(&Dev->DummyIntEPPool);
    if (dummyEp == NULL) {
      USBH_WARN((USBH_MTYPE_OHCI, "OHCI: OhdBuildAndSetInterruptTable: No resources!"));
      // Release all allocated pool elements
      OhIntPutAllDummyEp(Dev);
      status = USBH_STATUS_RESOURCES;
      break;
    }
    Dev->DummyInterruptEpArr[i] = dummyEp;
  }
  return status;
}

/*********************************************************************
*
*       OhIntDummyEpAllocPool
*
*  Function description
*/
USBH_STATUS OhIntDummyEpAllocPool(HCM_POOL * EpPool) {
  USBH_STATUS status;
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: OhDummyIntEpAlloc: Eps:%d!",OHD_DUMMY_INT_NUMBER));
  status = HcmAllocPool(EpPool, OHD_DUMMY_INT_NUMBER, OH_ED_SIZE, sizeof(OHD_DUMMY_INT_EP), OH_ED_ALIGNMENT);
  if ( status ) { // On error
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: OhDummyIntEpAllocPool: HcmAllocPool!"));
  }
  return status;
}

/*********************************************************************
*
*       OhIntBuildDummyEpTree
*
*  Function description
*    Links all dummy endpoint with the next dummy endpoint.
*    Start with 32 ms endpoints up to the 1ms last endpoint.
*/
void OhIntBuildDummyEpTree(HC_DEVICE * Dev) {
  // The initialization starts with interval 32ms, epListIdxNextInterval
  // contains the index for 16ms and so on
  int                epInterval, epNextIntervalIdx;
  int                i, j, numberEps;
  OHD_DUMMY_INT_EP * ep;
  OHD_DUMMY_INT_EP * nextIntervalEp;
  numberEps        = 0;
  // Start with the 32 ms level, breaks the loop if have the 1ms interval
  for (epInterval = 32; epInterval > 0; epInterval /= 2) {
    if (1 == epInterval) { // Last 1ms endpoint
      epNextIntervalIdx = 0;
      nextIntervalEp = NULL;
    } else {
      epNextIntervalIdx = ((epInterval) / 2) - 1;
      T_ASSERT(epNextIntervalIdx < ARRAY_ELEMENTS(Dev->DummyInterruptEpArr));
      nextIntervalEp = Dev->DummyInterruptEpArr[epNextIntervalIdx];
    }
    USBH_LOG((USBH_MTYPE_OHCI, "OHCI: OhIntBuildDummyEpTree: current interval: %d next interval: %d!", epInterval, epNextIntervalIdx+1));
    // Initialize all dummy endpoints for that interval. The amoung of intervals is identical to the
    // interval (32ms = 32 dummy endpoints, nextIntervalEp points to the first endpoint in the next interval
    for (j = 0, i = 0; i < epInterval; i++) {
      if (numberEps >= OHD_DUMMY_INT_NUMBER) {
        USBH_WARN((USBH_MTYPE_OHCI, "OHCI: OhdBuildInterruptTree: too many endpoints!"));
        goto exit;
      }
      T_ASSERT((epInterval-1+i) < ARRAY_ELEMENTS(Dev->DummyInterruptEpArr));
      ep = Dev->DummyInterruptEpArr[epInterval - 1 + i];
      if (ep == NULL) {
        USBH_WARN((USBH_MTYPE_OHCI, "OHCI: OhdBuildInterruptTree: dummy endpoint is NULL!"));
        return;
      }
      HCM_ASSERT_ITEM_HEADER(&ep->ItemHeader);
      OhDummyIntInit(ep, Dev, (U16)(epInterval)); // Init the dummy endpoint
      if (NULL == nextIntervalEp) {
        break;
      }
      OhDummyIntInsertList(ep, nextIntervalEp);

      if ( i & 0x01 ) { // Increment ep in current level
        j++;            // Get next intervalEp on every second endpoint
        T_ASSERT((epNextIntervalIdx+j) < ARRAY_ELEMENTS(Dev->DummyInterruptEpArr));
        nextIntervalEp = Dev->DummyInterruptEpArr[epNextIntervalIdx + j];
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
*       OhIntGetBandwidth
*
*  Function description
*    OhIntGetBandwidth determine which dummy endpoint with an certain interval
*    has the minimum bandwidth.
*
*  Parameters:
*    Dev:          Valid device
*    intervalTime: in ms or 0
*    Ep:           valid ptr or NULL : OUT: Pointer to the dummy endpoint
*  Return value:
*    Maximum bytes that has to be transfer in one frame.
*    This is the maximum interrupt bandwidth used on the USB bus!
*/
static U32 OhIntGetBandwidth(HC_DEVICE * Dev, U16 intervalTime, OHD_DUMMY_INT_EP ** Ep) {
  int                       frameNb;
  U32                       bandWidth;
  U32                       minBandWidth;
  U32                       maxBandWidth = 0;
  OHD_DUMMY_INT_EP        * dummyEp;
  OHD_DUMMY_INT_EP        * intervalEp;
  OHD_DUMMY_INT_EP        * minBandwidthEp;
  int minBandWidthFrameNb = 0;
  int maxBandWidthFrameNb = 0;
  T_ASSERT(Ep != NULL);
  OH_DEV_VALID(Dev);
  USBH_USE_PARA(minBandWidthFrameNb);
  USBH_USE_PARA(maxBandWidthFrameNb);
  // First search an dummy endpoint where the sum of all endpoints has a minimum.
  minBandwidthEp = NULL;
  minBandWidth   = ~((U32)0);
  for ( frameNb=0;frameNb<32;frameNb++) {
    // Search for all frame numbers line by line in the interrupt endpoint tree, Start with the 32ms interval
    T_ASSERT((31+frameNb) < ARRAY_ELEMENTS(Dev->DummyInterruptEpArr));
    dummyEp    = Dev->DummyInterruptEpArr[31+frameNb];
    intervalEp = NULL;
    bandWidth  = 0;
    // Walk through all dummy endpoints and adds the bandwidth of the endpoints
    while (dummyEp != NULL) {
      HCM_ASSERT_ITEM_HEADER(&dummyEp->ItemHeader);
      bandWidth += dummyEp->Bandwidth;
      if (dummyEp->IntervalTime == intervalTime) {
        intervalEp = dummyEp;
      }
      dummyEp = dummyEp->NextDummyEp;
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
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: OhIntGetBandwidth min: frameNb: %d bandwitdh: %d  max: framenb: %d bandwidth: %d !",
                          minBandWidthFrameNb, minBandWidth, maxBandWidthFrameNb, maxBandWidth));
  return maxBandWidth;
}

/*********************************************************************
*
*       OhIntInsertEndpoint
*
*  Function description
*    Insert an interrupt endpoint in the HC link list and in the device
*    link list. All active endpoints are appended to an dummy endpoint
*    that has a specified frame interval. Every dummy endpoint counts
*    the number of inserted active endpoints. An new endpoint is inserted
*    on such dummy endpoints where the number of active endpoints in the
*    tree has a minimum.
*/
USBH_STATUS OhIntInsertEndpoint(OHD_BULK_INT_EP * Ep) {
  HC_DEVICE        * dev;
  OHD_DUMMY_INT_EP * minBandwidthEp;

  T_ASSERT(Ep != NULL);
  dev = Ep->Dev;
  OH_DEV_VALID(dev);
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: INFO OhIntInsertEndpoint Ep:0x%x Dev:%d!", Ep->EndpointAddress, (int)Ep->DeviceAddress));
  OhIntGetBandwidth(dev, Ep->IntervalTime, &minBandwidthEp);
  if (minBandwidthEp == NULL) {
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: OhIntInsertEndpoint: OhIntGetBandwidth: not endpoint found!"));
    return USBH_STATUS_ERROR;
  }
  // Insert Ep of the returned dummy endpoint user endpoint list
  Ep->State = OH_EP_LINK;
  OhEpGlobLinkEds(&minBandwidthEp->ItemHeader, &Ep->ItemHeader);
  DlistInsertHead(&minBandwidthEp->ActiveList, &Ep->ListEntry);
  // Link the new user endpoint to an dummy endpoint that has the minimum bandwidth
  Ep->DummyIntEp = minBandwidthEp;
  // Adds the packet size for this interval
  minBandwidthEp->Bandwidth += Ep->MaxPktSize;
  dev->IntEpCount++;
  return USBH_STATUS_SUCCESS;
}

/*********************************************************************
*
*       OhInt_RemoveAllUserEDFromPhysicalLink
*
*  Function description
*/
void OhInt_RemoveAllUserEDFromPhysicalLink(HC_DEVICE * dev) {
  PDLIST             entry;
  int                i;
  OHD_BULK_INT_EP  * remove_ep;
  OHD_DUMMY_INT_EP * dummy_ep;
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: OhInt_RemoveAllUserEDFromPhysicalLink!"));
  for (i = 0; i < OHD_DUMMY_INT_NUMBER; i++) {
    dummy_ep = dev->DummyInterruptEpArr[i];
    if (dummy_ep == NULL) {
      continue;
    }
    entry = DlistGetNext(&dummy_ep->ActiveList);
    // Remove all endpoints where the UNLINK flag is not set
    while (entry != &dummy_ep->ActiveList) {
      remove_ep = GET_BULKINT_EP_FROM_ENTRY(entry);
      OH_BULKINT_VALID(remove_ep);
      entry = DlistGetNext(entry);
      if (remove_ep->State != OH_EP_UNLINK && remove_ep->State != OH_EP_UNLINK_TIMER) {
        OhInt_RemoveUserEDFromPhysicalLink(dev, remove_ep); // Unlink all not phys. unlinked endpoints
      }
    }
  }
}

/*********************************************************************
*
*       OhInt_RemoveAllUserEDFromPhysicalLink
*
*  Function description
*    Releases an interrupt endpoint without stopping the interrupt list.
*    0. No request should be pending
*    1. Set the Skip bit
*    2. The pointer from the previous endpoint is set to the next endpoint
*    3. An timer is started for two milliseconds
*    4. If the timer is processed the endpoint is put back to the pool
*/
void OhInt_ReleaseEndpoint(OHD_BULK_INT_EP * Ep, USBH_RELEASE_EP_COMPLETION_FUNC * pfReleaseEpCompletion, void * pContext) {
  HC_DEVICE * dev;
  OH_BULKINT_VALID(Ep);
  dev = Ep->Dev;
  OH_DEV_VALID(dev);
  T_ASSERT(DlistEmpty(&Ep->UrbList) );
  T_ASSERT(Ep->PendingUrb == NULL);
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: INFO OhInt_ReleaseEndpoint Ep: 0x%x Dev: %d!", Ep->EndpointAddress, Ep->DeviceAddress));
  if (Ep->State == OH_EP_UNLINK || Ep->State==OH_EP_UNLINK_TIMER) {
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: OhEp0_ReleaseEndpoint: Endpoint already unlinked, return!"));
    return;
  }
  Ep->ReleaseContext    = pContext;
  Ep->ReleaseCompletion = pfReleaseEpCompletion;
  OhInt_RemoveUserEDFromPhysicalLink(dev, Ep); // The endpoint has only logical no physical link
  if(!dev->IntEpRemoveTimerRunFlag){
    dev->IntEpRemoveTimerRunFlag    = TRUE;
    dev->IntEpRemoveTimerCancelFlag = FALSE;
    Ep->State                       = OH_EP_UNLINK;
    USBH_StartTimer(dev->IntEpRemoveTimer, OH_STOP_DELAY_TIME);
  } else {
    Ep->State = OH_EP_UNLINK_TIMER;
  }
}

/******************************* EOF ********************************/
