/*********************************************************************
*  
*   IAR PowerPac
*
*   (c) Copyright IAR Systems 2010.  All rights reserved.
*
**********************************************************************
----------------------------------------------------------------------
File        : USBH_OHC_EP_BulkInt.c
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
*       OhBulkIntCompleteUrb
*
*  Function description
*    Completes the current bulk URB. Is called if all TDs of
*    an URB are done or the URB is aborted
*
*  Parameters:
*    Urb: Condition code of the transfer or an driver status
*/
static void OhBulkIntCompleteUrb(OHD_BULK_INT_EP * Ep, URB * Urb, USBH_STATUS Status, int NewUrbFlag) {
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: OhBulkIntCompleteUrb Ep 0x%x length: %lu!", (int)Ep->EndpointAddress, Urb->Request.BulkIntRequest.Length));
  if (Urb != NULL) {
    Urb->Header.Status = Status;
    if (Urb == Ep->PendingUrb) {         // Current completed URB
#if (USBH_DEBUG > 1)
      if (Ep->TdCounter > 1) {           // More than the reserved TD is active
        USBH_LOG((USBH_MTYPE_OHCI, "OHCI: INFO OhBulkIntCompleteUrb: Pending TDs!!!"));
      }
#endif
      if (NULL != Ep->CopyBuffer) {      // Release the copy buffer resource
        Urb->Request.BulkIntRequest.Length = Ep->CopyBuffer->Transferred;
        HcmPutItem(&Ep->CopyBuffer->ItemHeader);
        Ep->CopyBuffer = NULL;
      }
      Ep->PendingUrb = NULL;             // Delete the current pending URB
    }
    T_ASSERT(Ep->UrbCount);
    Ep->UrbCount--;
    if (NewUrbFlag) {
      OhBulkIntSubmitUrbsFromList(Ep);   // Submit next packet before the previous one is completed. All neded previous transfer resources are released!
    }
    Urb->Header.InternalCompletion(Urb); // Call the USB Bus driver completion routine
  }
}

/*********************************************************************
*
*       OhBulkIntCompletePendingUrbs
*
*  Function description
*
*  Parameters:
*    HcFlagMask: URBS where one of the flag mask bit is set where completed (mask bits are or'ed)
*                0: all URBs are completed.
*/
static void OhBulkIntCompletePendingUrbs(OHD_BULK_INT_EP * Ep, U32 HcFlagMask) {
  PDLIST   listHead, entry, abortEntry;
  DLIST    abortList;
  URB    * urb;
  OH_BULKINT_VALID(Ep);
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: OhBulkIntCompletePendingUrbs!"));
  DlistInit(&abortList);
  listHead = &Ep->UrbList;
  entry    = DlistGetNext(listHead);
  while (entry != listHead) {
    urb        = (URB *)GET_URB_HEADER_FROM_ENTRY(entry);
    abortEntry = entry;
    entry      = DlistGetNext(entry);
    if (0 == HcFlagMask || (HcFlagMask &urb->Header.HcFlags)) { // No mask bit set or the same mask bit is also in the URB set
      DlistRemoveEntry(abortEntry);
      DlistInsertHead(&abortList, abortEntry);
    }
  }
  while (!DlistEmpty(&abortList)) {
    DlistRemoveTail(&abortList, &entry);
    urb = (URB * )GET_URB_HEADER_FROM_ENTRY(entry);
    urb->Request.BulkIntRequest.Length = 0;                     // Set length to zero
    OhBulkIntCompleteUrb(Ep, urb, USBH_STATUS_CANCELED, FALSE);
  }
}

/*********************************************************************
*
*       OhBulkUnlink
*
*  Function description
*    Before this function is called the HC list must be disabled! The
*    endpoint is removed from the Oist bulk list and from the device
*    object bulk list. If the function returns the endpoint is still valid!
*/
static void OhBulkUnlink(OHD_BULK_INT_EP * Ep) {
  HC_DEVICE       * dev;
  PDLIST            previousEntry;
  OHD_BULK_INT_EP * previousEp;
  U32               nextPhyAddr;
  OH_BULKINT_VALID(Ep);
  dev             = Ep->Dev;
  OH_DEV_VALID(dev);
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: OhBulkUnlink: Ep: 0x%x!",Ep->EndpointAddress));
  T_ASSERT( NULL == Ep->PendingUrb);
  T_ASSERT( 0==Ep->UrbCount);
  T_ASSERT( dev->BulkEpCount);
  T_ASSERT( Ep->State != OH_EP_IDLE);
  // HC list must be disabled
  T_ASSERT( 0==OhHalTestReg(dev->RegBase, OH_REG_CONTROL, HC_CONTROL_BLE));
  OhHalWriteReg(dev->RegBase,OH_REG_BULKCURRENTED, 0);
  T_ASSERT(!DlistEmpty(&dev->BulkEpList));
  previousEntry = DlistGetPrev(&Ep->ListEntry); // Get the list tail
  if ( previousEntry == &dev->BulkEpList ) {
    USBH_LOG((USBH_MTYPE_OHCI, "OHCI: INFO OhBulkUnlink: remove the first ED in the  ED list!"));
    nextPhyAddr = OhEpGlobUnlinkEd(NULL, &Ep->ItemHeader);
    OhHalWriteReg(dev->RegBase, OH_REG_BULKHEADED, nextPhyAddr);
  } else {
    previousEp = GET_BULKINT_EP_FROM_ENTRY(previousEntry);
    OH_BULKINT_VALID(previousEp);
    OhEpGlobUnlinkEd(&previousEp->ItemHeader, &Ep->ItemHeader);
  }
  DlistRemoveEntry(&Ep->ListEntry);
  Ep->State = OH_EP_IDLE;
  if (0==dev->BulkEpCount) {
    USBH_LOG((USBH_MTYPE_OHCI, "OHCI: INFO OhBulkUnlink: ED list now empty!"));
  }
  T_ASSERT(dev->BulkEpCount);
  dev->BulkEpCount--;
}

/*********************************************************************
*
*       OhBulkIntSubmitTransfer
*
*  Function description
*/
static USBH_STATUS OhBulkIntSubmitTransfer(OHD_BULK_INT_EP * Ep, void * Buffer, U32 Length) {
  USBH_STATUS   status;
  OHD_GTD     * td;
  OHD_TD_PID    pid;
  U32           startAddr, endAddr;
  U32           TDword0Mask;
  OH_DEV_VALID(Ep->Dev);
  T_ASSERT( 0==Ep->AbortMask);
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: OhBulkIntSubmitTransfer Ep: 0x%x Length: %lu!",(int)Ep->EndpointAddress,Length ));
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: Start transfer: Length: %lu ",Length ));
  Ep->UrbTotalTdNumber =0;
  Ep->UrbDoneTdNumber  =0;
  Ep->UpDownTDCounter  =0;
  Ep->Flags           &=~OH_SHORT_PKT_EPFLAG;
  TDword0Mask          =0;
  if (Ep->EndpointAddress & USB_IN_DIRECTION) {
    pid = OH_IN_PID;
  } else {
    pid = OH_OUT_PID;
  }
  // Build the TDs
  //   Get the MDL list (page address table)
  //   Init the dummy TD until the address table is submitted
  //   Allocate a new TD
  //   Link the new TD, set TailP from ED
  status = USBH_STATUS_SUCCESS;
    for (; ;) {
      // Get the Tail TD
      td = OhEpGlobGetLastTDFromED(&Ep->ItemHeader);
      if (td==NULL) {
        USBH_WARN((USBH_MTYPE_OHCI, "OHCI: FATAL OhBulkIntSubmitTransfer: OhEpGlobGetLastTDFromED!"));
        status = USBH_STATUS_ERROR;
        break;
      }
      // Append all needed TDs
      if ( Length==0 ) { // Zero length IN packet
        if ( Ep->EndpointAddress &  USB_IN_DIRECTION) {
           TDword0Mask |=OHCI_TD_R;
        }
        USBH_LOG((USBH_MTYPE_OHCI, "OHCI: OhTdInit: ep: 0x%x ",Ep->EndpointAddress)); // Init the TD
        // Set the previous TD and allocate and insert a new one
        OhTdInit(td,Ep,Ep->EndpointType,pid,0,0,TDword0Mask);
        td = OhTdGet(&Ep->Dev->GTDPool);
        if (td == NULL) {
          USBH_WARN((USBH_MTYPE_OHCI, "OHCI: FATAL OhBulkIntAllocAndFillTD: OhTdGet!"));
          status = USBH_STATUS_MEMORY;
          break;
        }
        // Start the TD, set the EP Tail pointer
        Ep->UrbTotalTdNumber++;
        OhEpGlobInsertTD(&Ep->ItemHeader, &td->ItemHeader, &Ep->TdCounter);
        break;
      } else { // Write all transfer descriptors
        // Setup all needed TDs if the data length unequal zero!
        // Set the buffer rounding bit only on the last page on IN token!
        // If a zero length IN packet is received no on the last page a USB data underrun
        // is generated form the HC and the DONE routine removes all other TDs and restart this endpoint!
        startAddr = (U32)Buffer;
        endAddr   = startAddr + Length - 1;
        if ( Ep->EndpointAddress &  USB_IN_DIRECTION) { // On the last transfer descriptor of all submitted TDs allow short packets
           TDword0Mask |=OHCI_TD_R;
        }
        USBH_LOG((USBH_MTYPE_OHCI, "OHCI: OhTdInit: ep: 0x%x ",Ep->EndpointAddress)); // Init the TD
        OhTdInit(td, Ep, Ep->EndpointType, pid, startAddr, endAddr, TDword0Mask);     // Set the previous TD and allocate and insert an new one
        td=OhTdGet(&Ep->Dev->GTDPool);                                                // Get an new TD
        if (td == NULL) {
          USBH_WARN((USBH_MTYPE_OHCI, "OHCI: FATAL OhBulkIntSubmitTransfer: OhTdGet!"));
          status = USBH_STATUS_MEMORY;
          break;
        }
        // Start the TD, set the EP Tail pointer
        Ep->UrbTotalTdNumber++;
        OhEpGlobInsertTD(&Ep->ItemHeader, &td->ItemHeader, &Ep->TdCounter);
        break;
      }
    }
  // As last restart the endpoint list
  if ( !Ep->Dev->BulkEpRemoveTimerRunFlag ) {
    // No other endpoint is removed
    OhdEndpointListEnable(Ep->Dev, Ep->EndpointType, TRUE, TRUE);
  }
  return status;
}

/*********************************************************************
*
*       OhBulkIntSubmitUrb
*
*  Function description
*    Submits an bulk URB. One URB is submitted at the time. More than
*    one transfer descriptor can be submitted.
*
*  Returns:
*    On success: USBH_STATUS_PENDING: 
*    On error:   Other vlaues 
*/
static USBH_STATUS OhBulkIntSubmitUrb(OHD_BULK_INT_EP * Ep, URB * urb) {
  USBH_STATUS             status;
  USBH_BULK_INT_REQUEST * bulkIntRequest;
  HC_DEVICE             * dev;
  U32                     length;
  U8                    * buffer;
  OH_BULKINT_VALID(Ep);
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: OhBulkIntSubmitUrb: Ep: 0x%x!",Ep->EndpointAddress));

  // The PendingURB is submitted
  T_ASSERT(urb != NULL);
  T_ASSERT(Ep->PendingUrb == NULL);
  T_ASSERT(0 == (urb->Header.HcFlags & URB_CANCEL_PENDING_MASK) );
  // Assert only the default TD is in the ED TD list
  T_ASSERT(Ep->TdCounter == 1 );
  T_ASSERT(!OhEpGlobIsHalt(&Ep->ItemHeader) );
  dev = Ep->Dev;
  OH_DEV_VALID(dev);
  bulkIntRequest = &urb->Request.BulkIntRequest;
  if (!USBH_IS_VALID_TRANSFER_MEM(bulkIntRequest->Buffer)) {
    // No valid transfer memory transfer data in pieces!
    //   Allocate and initialize the new transfer buffer on error complete the request, only the SETUP packet is transferered!
    //   Get new pointer and length
    //   If this is an OUT packet copy data to the new transfer buffer
    T_ASSERT(NULL == Ep->CopyBuffer);
    Ep->CopyBuffer = OhGetInitializedCopyTransferBuffer(&dev->TransferBufferPool, bulkIntRequest->Buffer, bulkIntRequest->Length);
    if (NULL == Ep->CopyBuffer) {
      USBH_WARN((USBH_MTYPE_OHCI, "OHCI: OhBulkSubmitUrb!"));
      status = USBH_STATUS_RESOURCES;
      goto exit;
    } else { // On success
      if ( !( Ep->EndpointAddress & USB_IN_DIRECTION) ) {                        // OUT packet
        OhFillCopyTransferBuffer(Ep->CopyBuffer);
      }
      buffer = OhGetBufferLengthFromCopyTransferBuffer(Ep->CopyBuffer, &length); // Buffer and length are updated
    }
  } else {
    buffer = bulkIntRequest->Buffer;
    length = bulkIntRequest->Length;
  }
  bulkIntRequest->Length = 0;                                                    // Clear URB length and submit buffer
  status                 = OhBulkIntSubmitTransfer(Ep,buffer,length);
  if (status) {
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: OhBulkSubmitUrb!"));
    if (NULL != Ep->CopyBuffer) {
      HcmPutItem(&Ep->CopyBuffer->ItemHeader);
      Ep->CopyBuffer = NULL;
    }
  } else { // On success set pending URB
    Ep->PendingUrb = urb;
    status         = USBH_STATUS_PENDING;
  }
exit:
  return status;
}

/*********************************************************************
*
*       OhBulkIntSubmitUrbsFromList
*
*  Function description
*    Submit the next URB from an bulk or interrupt endpoint
*/
void OhBulkIntSubmitUrbsFromList(OHD_BULK_INT_EP * Ep) {
  URB         * urb;
  USBH_STATUS   status;
  PDLIST        entry;
  int           emptyFlag;
  OH_BULKINT_VALID(Ep);
  if (Ep->State     != OH_EP_LINK) {
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: OhBulkIntSubmitUrbsFromList: ep 0x%x is unlinked!",       (unsigned int)Ep->EndpointAddress));
    return;
  }
  if (Ep->AbortMask != 0){
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: OhBulkIntSubmitUrbsFromList: urb on ep 0x%x is aborted!", (unsigned int)Ep->EndpointAddress));
    return;
  }
  if (OhEpGlobIsHalt(&Ep->ItemHeader)) {
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: OhBulkIntSubmitUrbsFromList: ep 0x%x is halted!",         (unsigned int)Ep->EndpointAddress));
    return;
  }
  if (NULL          != Ep->PendingUrb) {
    return;
  }
  emptyFlag = DlistEmpty(&Ep->UrbList);
  while(!emptyFlag) { // Submit the next URB
    DlistRemoveHead(&Ep->UrbList, &entry);
    emptyFlag = DlistEmpty(&Ep->UrbList);
    urb       = (URB*)GET_URB_HEADER_FROM_ENTRY(entry);
    status    = OhBulkIntSubmitUrb(Ep, urb);
    if (status == USBH_STATUS_PENDING) { // On success stop
      break;
    } else { // Urb can not be submitted
      if (Ep->UrbCount==1) {
        OhBulkIntCompleteUrb(Ep, Ep->PendingUrb, status, FALSE); // Endpoint can be invalid
        break;
      } else {
        OhBulkIntCompleteUrb(Ep, Ep->PendingUrb, status, FALSE);
      }
    }
  }
}

/*********************************************************************
*
*       OhBulkIntCheckAndCancelAbortedUrbs
*
*  Function description
*    Completes all aborted URbs clears the endpoint abort mask and
*    resubmit new not aborted URBs of the same endpoint.
*
*  Parameters
*    TDDoneFlag:           TRUE if called in the DONE TD routine
*
*  Return value
*    USBH_STATUS_SUCCESS:  1. All URBs are canceled and new are submitted
*                          2. The next not aborted URB in the URB list is submitted
*    USBH_STATUS_ERROR:    The pending URB cannot completed because an TD is lost (in the OHCI cache)!
*/
USBH_STATUS OhBulkIntCheckAndCancelAbortedUrbs(OHD_BULK_INT_EP * Ep, int TDDoneFlag) {
  T_BOOL    td_in_donecache_queue_flag = FALSE;
  OHD_GTD * gtd;
  T_BOOL    shortPkt                   = FALSE;
  U32       length, tdCounter;
  T_BOOL    complete_flag              = FALSE;
  T_ASSERT(Ep->AbortMask);
  if (NULL != Ep->PendingUrb) {                                          // An URB is pending
    if ( !TDDoneFlag ) {                                                 // Called from the DONE routine
      tdCounter = OhEpGlobGetTdCount(&Ep->ItemHeader, &Ep->Dev->GTDPool);
      T_ASSERT( tdCounter >= 1 );                                        // Compare sum of TDs
      if ((tdCounter-1) + Ep->UrbDoneTdNumber != Ep->UrbTotalTdNumber) { // TD lost, -1 do not use the dummy TD, waits for DONE interrupt!
        USBH_LOG((USBH_MTYPE_OHCI, "OHCI: INFO OhBulkIntCheckAndCancelAbortedUrbs: lost TD, wait for DONE!"));
        td_in_donecache_queue_flag = TRUE;
      } else {                                                           // No TD lost, complete pending URB with the length of the last pending TD
        gtd = OhEpGlobGetFirstTDFromED(&Ep->ItemHeader);
        OhTdGetStatusAndLength(gtd, &length, &shortPkt);                 // Only the length is checked of the returned values!
        Ep->PendingUrb->Request.BulkIntRequest.Length += length;         // Update the length of the canceled URB by the last active TD
        OhEpGlobDeleteAllPendingTD(&Ep->ItemHeader, &Ep->TdCounter);
        complete_flag = TRUE;
      }
    } else {
      complete_flag = TRUE;
    }
    if (complete_flag) {
      OhEpGlobClearSkip(&Ep->ItemHeader);
      Ep->AbortMask = 0;
      OhBulkIntCompleteUrb(Ep, Ep->PendingUrb, USBH_STATUS_CANCELED, FALSE);
    }
  } else { // No pending URB
    Ep->AbortMask = 0;
    OhEpGlobClearSkip(&Ep->ItemHeader);
  }
  if (!td_in_donecache_queue_flag) {
    OhBulkIntCompletePendingUrbs(Ep, URB_CANCEL_PENDING_MASK);
    OhBulkIntSubmitUrbsFromList(Ep); // Submit the next canceled URB if available
    return USBH_STATUS_SUCCESS;
  } else { // Pending TD in Host cache
    return USBH_STATUS_ERROR;
  }
}

/*********************************************************************
*
*       Global functions
*
**********************************************************************
*/

/*********************************************************************
*
*       OhBulkRemoveEndpoints
*
*  Function description
*    Before an endpoint can be removed for the HC list, the endpoints
*    unlink flag is set for an minimum of one ms. If an second endpoint
*    must removed during and the first is not removed but the time runs,
*    the timer is restarted again so the HC can finish all operations
*    on that second endpoint before it removed.
*
*  Parameters:
*    AllEndpointFlag: If true all control endpoints in the linked list are
*                     deleted, else only endpoints where the UNLINK Flag is on
*/
void OhBulkRemoveEndpoints(HC_DEVICE * dev, T_BOOL AllEndpointFlag) {
  OHD_BULK_INT_EP                 * ep;
  PDLIST                            entry;
  DLIST                             removeList;
  USBH_RELEASE_EP_COMPLETION_FUNC * pfCompletion;
  void                            * pContext;
  OH_DEV_VALID(dev);
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: OhBulkRemoveEndpoints!"));
#if (USBH_DEBUG > 1)
  if ( OhHalTestReg(dev->RegBase, OH_REG_CONTROL, HC_CONTROL_BLE)) {
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: FATAL OhBulkRemoveEndpoints: bulk list not disabled!"));
  }
#endif

  // Remove needed endpoints from the list
  DlistInit(&removeList);
  entry = DlistGetPrev(&dev->BulkEpList);
  while (entry != &dev->BulkEpList)  {
    T_ASSERT(dev->BulkEpCount);
    ep    = GET_BULKINT_EP_FROM_ENTRY(entry);
    OH_BULKINT_VALID(ep);
    entry = DlistGetPrev(entry);
    if (AllEndpointFlag) {
      OhBulkUnlink(ep);
      DlistInsertHead(&removeList, &ep->ListEntry);
    } else {
      if ( ep->State == OH_EP_UNLINK ) { // Remove only endpoints where the OH_EP_UNLINK flag is set
        OhBulkUnlink(ep);
        DlistInsertHead(&removeList, &ep->ListEntry);
      }
    }
  }
  // After unlinking the endpoint restart the HC list processing
  if (dev->BulkEpCount > 1) { // On active endpoints
    OhdEndpointListEnable(dev, USB_EP_TYPE_BULK, TRUE, TRUE);
  }
#if (USBH_DEBUG > 1)
  if(dev->BulkEpCount == 0) {
    USBH_LOG((USBH_MTYPE_OHCI, "OHCI: INFO OhEp0RemoveEndpoints: all control endpoints removed!"));
  }
#endif
  // Call the release completion functions of all removed endpoints
 while (!DlistEmpty(&removeList)) {
    DlistRemoveHead(&removeList, &entry);
    ep = GET_BULKINT_EP_FROM_ENTRY(entry);
    OH_BULKINT_VALID(ep);
    pfCompletion = ep->ReleaseCompletion;
    pContext = ep->ReleaseContext;
    OhBulkIntEpPut(ep);
    if (pfCompletion) { // Call the completion routine, attention: runs in this context
      pfCompletion(pContext);
    }
  }
}

/*********************************************************************
*
*       OhBulkRemoveEp_TimerCallback
*
*  Function description
*    Called if one or more bulk endpoints has been removed
*/
void OhBulkRemoveEp_TimerCallback(void * Context) {
  HC_DEVICE * dev;
  dev       = (HC_DEVICE *)Context;
  OH_DEV_VALID(dev);
  if (dev->BulkEpRemoveTimerCancelFlag) {
    dev->BulkEpRemoveTimerCancelFlag = FALSE;
    return;
  }
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: OhBulkRemoveEp_TimerCallback!"));
  dev->BulkEpRemoveTimerRunFlag = FALSE;                              // Timer is stopped
  OhBulkRemoveEndpoints(dev, FALSE);
}

/*********************************************************************
*
*       OhBulkIntMarkUrbsWithCancelPendingFlag
*
*  Function description
*/
static void OhBulkIntMarkUrbsWithCancelPendingFlag(OHD_BULK_INT_EP * Ep) {
  PDLIST   listHead, entry;
  URB    * urb;
  OH_BULKINT_VALID(Ep);
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: OhBulkIntMarkUrbsWithCancelPendingFlag!"));
  listHead = &Ep->UrbList;
  entry    = DlistGetNext(listHead);
  while (entry != listHead) {
    urb = (URB *)GET_URB_HEADER_FROM_ENTRY(entry); // The URB header is the first element in the URB, cast it
#if (USBH_DEBUG > 1)
    if (urb->Header.HcFlags & URB_CANCEL_PENDING_MASK) {
      USBH_WARN((USBH_MTYPE_OHCI, "OHCI: OhBulkIntMarkUrbsWithCancelPendingFlag: URB_CANCEL_PENDING_MASK already set!"));
    }
#endif
    urb->Header.HcFlags |= URB_CANCEL_PENDING_MASK;
    entry = DlistGetNext(entry);
  }
}

/*********************************************************************
*
*       OhBulkAbortUrb_TimerCallback
*
*  Function description
*    The timer routine runs if an bulk endpoint is aborted and an URB
*    of the endpoint at the time where the endpoint is aborted is submitted
*    and pending. The endpoint is always skipped if the endpoint is aborted.
*    This routine calls the abort request completion routine!
*    Cancel all URBs on skipped EDs, this function is always called after a
*    timeout of more than one frame after the ED SKIP bit is set
*
*  Parameters
*    Context: Dev Ptr.
*/
void OhBulkAbortUrb_TimerCallback(void * Context) {
  HC_DEVICE       * dev;
  T_BOOL            restart_flag;
  PDLIST            entry;
  OHD_BULK_INT_EP * ep;
  USBH_STATUS       status;
  T_BOOL            start_timer = FALSE;
  T_ASSERT(Context != NULL);
  dev = (HC_DEVICE*)Context;
  OH_DEV_VALID(dev);
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: OhBulkAbortUrb_TimerCallback!"));

  if (dev->BulkEpAbortTimerCancelFlag) {
    dev->BulkEpAbortTimerCancelFlag = FALSE;
    return;
  }
  dev->BulkEpAbortTimerRunFlag = FALSE;

  // Mark all aborted endpoints with EP_ABORT_SKIP_TIME_OVER_MASK mask
  entry=DlistGetNext(&dev->BulkEpList);
  while (entry != &dev->BulkEpList) {
    ep = GET_BULKINT_EP_FROM_ENTRY(entry);
    OH_BULKINT_VALID(ep);
    if (ep->AbortMask & EP_ABORT_MASK) {
      ep->AbortMask |= EP_ABORT_SKIP_TIME_OVER_MASK | EP_ABORT_PROCESS_FLAG_MASK;
    } else if (ep->AbortMask & EP_ABORT_START_TIMER_MASK) { // Start timer again for aborted endpoints where the timer already runs
      start_timer = TRUE;
    }
    entry = DlistGetNext(entry);
  }
  if (start_timer) {
    dev->BulkEpAbortTimerRunFlag = TRUE;
  }
  restart_flag = TRUE;
  while(restart_flag) {
    restart_flag = FALSE;
    entry        = DlistGetNext(&dev->BulkEpList);
    while (entry != &dev->BulkEpList) {
      ep    = GET_BULKINT_EP_FROM_ENTRY(entry);
      entry = DlistGetNext(entry);
      OH_BULKINT_VALID(ep);
      if ((ep->AbortMask & EP_ABORT_PROCESS_FLAG_MASK)) { // Endpoint aborted
        ep->AbortMask &= ~EP_ABORT_PROCESS_FLAG_MASK;
    #if (USBH_DEBUG > 1)
        if (ep->PendingUrb) {
         T_ASSERT(OhEpGlobIsHalt(&ep->ItemHeader) || OhEpGlobIsSkipped(&ep->ItemHeader));
        }
    #endif
        status = OhBulkIntCheckAndCancelAbortedUrbs(ep, FALSE);
        if (!status) {
          // On success it is possible that the completion routine is called and the BulkEpList can be changed
          restart_flag = TRUE;
          break;
        }
      }
    }
  }
  // Start timer at the end of the timer routine
  if (start_timer) {
    dev->BulkEpAbortTimerCancelFlag = FALSE;
    USBH_StartTimer(dev->BulkEpAbortTimer, OH_STOP_DELAY_TIME);
  }
}

/*********************************************************************
*
*       OhBulkIntAbortEndpoint
*
*  Function description
*    Aborts all pending URBs. It is allowed to abort more than endpoints
*    at the time. Only after a frame timeout of the last aborted endpoint
*    the timer callback routine is called! Because the bulk and interrupt
*    endpoints have different lists different timer callback routines are used.
*/
USBH_STATUS OhBulkIntAbortEndpoint(OHD_BULK_INT_EP * Ep) {
  HC_DEVICE   * dev;
  USBH_STATUS   status;
  OH_BULKINT_VALID(Ep);
  dev         = Ep->Dev;
  OH_DEV_VALID(dev);
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: OhBulkIntAbortEndpoint: Ep:0x%x!", Ep->EndpointAddress));
  status      = USBH_STATUS_SUCCESS;
  // If an TD is pending the Skip bit is set and an timer is scheduled
  // If in the timer routine allpending TDs are removed.
  // Not pending URBs with the abort Flag are removed.
  // As last the Skip bit is reset.
  if (Ep->AbortMask & (EP_ABORT_MASK | EP_ABORT_START_TIMER_MASK)) { // Already aborted
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: OhBulkIntAbortEndpoint: Endpoint 0x%x already aborted!",Ep->EndpointAddress));
    OhBulkIntMarkUrbsWithCancelPendingFlag(Ep);
    return USBH_STATUS_SUCCESS;
  }
  if (NULL == Ep->PendingUrb) {
    OhBulkIntCompletePendingUrbs(Ep,0); // Cancel the URB list without skipping of the endpoint
  } else {
    // Skip the endpoint from list processing, wait a frame time until the TD can removed from the endpoint.
    // Because one timer is used for all control endpoints restart the timer in the timer routine if the timer already started.
    OhEpGlobSetSkip(&Ep->ItemHeader);
    Ep->PendingUrb->Header.HcFlags |=URB_CANCEL_PENDING_MASK;
    Ep->AbortMask |= EP_ABORT_MASK;
    OhBulkIntMarkUrbsWithCancelPendingFlag(Ep);
    if (Ep->EndpointType == USB_EP_TYPE_INT) { // Interrupt endpoint
      if (!dev->IntEpAbortTimerRunFlag) {
        dev->IntEpAbortTimerRunFlag    = TRUE;
        dev->IntEpAbortTimerCancelFlag = FALSE;
        USBH_StartTimer(dev->IntEpAbortTimer, OH_STOP_DELAY_TIME);
      } else {
        Ep->AbortMask |= EP_ABORT_START_TIMER_MASK;
      }
    } else { // Bulk endpoint
      if (!dev->BulkEpAbortTimerRunFlag) {
        dev->BulkEpAbortTimerRunFlag    = TRUE;
        dev->BulkEpAbortTimerCancelFlag = FALSE;
        USBH_StartTimer(dev->BulkEpAbortTimer, OH_STOP_DELAY_TIME);
      } else {
        Ep->AbortMask |= EP_ABORT_START_TIMER_MASK;
      }
    }
  }
  return status;
}

/*********************************************************************
*
*       OhBulkIntAllocPool
*
*  Function description
*    Allocate all bulk or interrupt endpoints from an specified pool
*/
USBH_STATUS OhBulkIntAllocPool(HCM_POOL * EpPool, unsigned int MaxEps) {
  USBH_STATUS status;
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: OhBulkIntAllocPool: Eps:%d!",MaxEps));
  if (!MaxEps) {
  return USBH_STATUS_SUCCESS;
  }
  status = HcmAllocPool(EpPool, MaxEps, OH_ED_SIZE, sizeof(OHD_BULK_INT_EP), OH_ED_ALIGNMENT);
  if ( status ) { // On error
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: OhBulkIntAllocPool: HcmAllocPool!"));
  }
  return status;
}

/*********************************************************************
*
*       OhBulkIntEpGet
*
*  Function description
*    Get an bulk or interrupt endpoint from an pool
*/
OHD_BULK_INT_EP * OhBulkIntEpGet(HCM_POOL * EpPool) {
  OHD_BULK_INT_EP * item;
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: OhBulkIntEpGet!"));
  item = (OHD_BULK_INT_EP*)HcmGetItem(EpPool);
  if (NULL==item) {
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: OhBulkEpGet: no resources!"));
  } else {
    T_ASSERT(TB_IS_ALIGNED(item->ItemHeader.PhyAddr, OH_ED_ALIGNMENT));
    DlistInit(&item->ItemHeader.Link.ListEntry); // Init the item link list for later deallocating TDs
  }
  return item;
}

/*********************************************************************
*
*       OhBulkIntEpPut
*
*  Function description
*    Puts an bulk or interrupt endpoint back to the pool
*/
void OhBulkIntEpPut(OHD_BULK_INT_EP * Ep) {
  OH_BULKINT_VALID(Ep);
  // Put all TD items back to the TD pool before put back the Ep0 object
  OhEpGlobRemoveAllTDtoPool(&Ep->ItemHeader, &Ep->TdCounter);
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: OhBulkIntEpPut!"));
  if (NULL != Ep->CopyBuffer) {
    HcmPutItem(&Ep->CopyBuffer->ItemHeader);
    Ep->CopyBuffer = NULL;
  }
  HcmPutItem(&Ep->ItemHeader);
}

/*********************************************************************
*
*       OhBulkIntInitEp
*
*  Function description
*    Initialize an bulk or interrupt endpoint
*/
OHD_BULK_INT_EP * OhBulkIntInitEp(OHD_BULK_INT_EP * Ep,              struct T_HC_DEVICE * Dev,         U8  EndpointType, U8         DeviceAddress,
                                  U8                EndpointAddress, U16                  MaxFifoSize, U16 IntervalTime, USBH_SPEED Speed, U32 Flags) {
  HCM_ITEM_HEADER * item;
  OHD_BULK_INT_EP * ep;
  OHD_GTD         * td;
  T_BOOL             SkipFlag;
  OH_BULKINT_VALID(Ep);        // The itemheader must be valid
  OH_DEV_VALID(Dev);
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: OhBulkIntInitEp: Dev.Addr: %d EpAddr.: 0x%x max.pktsize: %d interval: %d",
                             (int)DeviceAddress, (int)EndpointAddress, (int)MaxFifoSize, (int)IntervalTime));
  ep = Ep;

  // Recalculate the interval time
  ep->CopyBuffer        = NULL;
  Ep->EndpointType      = EndpointType;
  Ep->Dev               = Dev;
  Ep->State             = OH_EP_IDLE;
  DlistInit(&Ep->ListEntry);
  DlistInit(&Ep->UrbList);
  Ep->UrbCount          = 0;
  Ep->PendingUrb        = NULL;
  Ep->UrbTotalTdNumber  = 0;
  Ep->UrbDoneTdNumber   = 0;
  Ep->UpDownTDCounter   = 0;
  Ep->TdCounter         = 0;
  Ep->AbortMask         = 0;
  Ep->CancelPendingFlag = FALSE;

  // Parameter
  Ep->DummyIntEp        = NULL;
  Ep->ReleaseCompletion = NULL;
  Ep->ReleaseContext    = NULL;
  Ep->Flags             = Flags;
  Ep->DeviceAddress     = DeviceAddress;
  Ep->EndpointAddress   = EndpointAddress;
  Ep->MaxPktSize        = MaxFifoSize;
  Ep->Speed             = Speed;
  if (EndpointType == USB_EP_TYPE_INT) { // Calculate the interval time
    if ( IntervalTime > 31 ) {
      Ep->IntervalTime  = 32;
    } else {
    if ( IntervalTime > 15 ) {
      Ep->IntervalTime  = 16;
    } else {
    if ( IntervalTime > 7) {
      Ep->IntervalTime  = 8;
    } else {
    if ( IntervalTime > 3 ) {
      Ep->IntervalTime  = 4;
    } else {
    if ( IntervalTime > 1 ) {
      Ep->IntervalTime  = 2;
    } else {
      Ep->IntervalTime  = 1;
    }}}}}
  } else {
    Ep->IntervalTime    = IntervalTime;
  }
  Ep->HaltFlag = FALSE;
  // Allocate a timer
  if ( Flags & OH_DUMMY_ED_EPFLAG ) {
    SkipFlag = TRUE;
  } else {
    SkipFlag = FALSE;
  }
  item = &Ep->ItemHeader;
  OhEpGlobInitED(item, DeviceAddress, EndpointAddress, MaxFifoSize, FALSE, SkipFlag, Speed); // Init DWORD 0..DWORD 3 in the ED
  td = OhTdGet(&Dev->GTDPool);                                                               // Get an TD item from pool
  if (td == NULL) {
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: OhBulkEpInit: OhTdGet!"));
    OhBulkIntEpPut(Ep);                                                                      // Release the endpoint
    ep = NULL;
    goto exit;
  }
  OhEpGlobInsertTD(item,&td->ItemHeader,&ep->TdCounter);                                     // Link the new TD to the EP TD list, set the bulk list filled bit
exit:
  return ep;                                                                                 // On error an NULL pointer is returned
}

/*********************************************************************
*
*       OhBulkInsertEndpoint
*/
/* Insert an endpoint in the devices endpoint list and in the HC Ed list */
void OhBulkInsertEndpoint(OHD_BULK_INT_EP * Ep) {
  HC_DEVICE       * dev;
  U8              * base;
  OHD_BULK_INT_EP * tailEp;
  PDLIST            entry;
  OH_BULKINT_VALID(Ep);
  dev = Ep->Dev;
  OH_DEV_VALID(dev);
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: OhBulkInsertEndpoint: Ep: 0x%x!", Ep->EndpointAddress));
  base = Ep->Dev->RegBase;
  if (0 == Ep->Dev->BulkEpCount) { // Empty bulk ED list
    T_ASSERT(OhHalReadReg(base, OH_REG_BULKHEADED) == 0);
    OhHalWriteReg(base, OH_REG_BULKHEADED, Ep->ItemHeader.PhyAddr);
  } else {
    T_ASSERT (!DlistEmpty(&dev->BulkEpList));
    entry  = DlistGetPrev(&dev->BulkEpList);
    tailEp = GET_BULKINT_EP_FROM_ENTRY(entry);
    OH_BULKINT_VALID(tailEp);
    OhEpGlobLinkEds(&tailEp->ItemHeader, &Ep->ItemHeader);
  }
  Ep->State = OH_EP_LINK; 
  DlistInsertTail(&dev->BulkEpList,&Ep->ListEntry); // Logical link
  dev->BulkEpCount++;
}

/*********************************************************************
*
*       OhBulk_ReleaseEndpoint
*
*  Function description
*    Starts removing of an endpoint from the HC list.
*/
void OhBulk_ReleaseEndpoint(OHD_BULK_INT_EP * Ep, USBH_RELEASE_EP_COMPLETION_FUNC * pfReleaseEpCompletion, void * pContext) {
  HC_DEVICE * dev;
  OH_BULKINT_VALID(Ep);
  dev       = Ep->Dev;
  OH_DEV_VALID(dev);
  T_ASSERT(DlistEmpty(&Ep->UrbList) );
  T_ASSERT(Ep->PendingUrb == NULL);

  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: INFO OhBulk_ReleaseEndpoint 0x%x!", Ep->EndpointAddress));
  Ep->ReleaseContext    = pContext;
  Ep->ReleaseCompletion = pfReleaseEpCompletion;
  if (Ep->State == OH_EP_UNLINK) {
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: OhBulk_ReleaseEndpoint: Endpoint already unlinked, return!"));
    return;
  }
  Ep->State = OH_EP_UNLINK;
  OhEpGlobSetSkip(&Ep->ItemHeader);
  if (!dev->BulkEpRemoveTimerRunFlag) { // If this is the first endpoint that must be deleted in the control endpoint list of the HC
    OhdEndpointListEnable(dev, Ep->EndpointType, FALSE, FALSE);
    dev->BulkEpRemoveTimerRunFlag    = TRUE;
    dev->BulkEpRemoveTimerCancelFlag = FALSE;
    USBH_StartTimer(dev->BulkEpRemoveTimer, OH_STOP_DELAY_TIME);
  }
}

/*********************************************************************
*
*       OhBulkIntAddUrb
*
*  Function description
*    Adds an bulk or interrupt endpoint request
*/
USBH_STATUS OhBulkIntAddUrb(OHD_BULK_INT_EP * Ep, URB * Urb) {
  USBH_STATUS status;
  OH_BULKINT_VALID(Ep);
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: INFO OhBulkIntAddUrb Ep: 0x%x!", Ep->EndpointAddress));
  OH_DEV_VALID(Ep->Dev);
  T_ASSERT(Urb != NULL);
  if (Ep->State != OH_EP_LINK) {
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI:  OhBulkIntAddUrb: Ep 0x%x not linked!", Ep->EndpointAddress));
    Urb->Header.Status = USBH_STATUS_ENDPOINT_HALTED;
    return USBH_STATUS_ENDPOINT_HALTED;
  }
  if ( Ep->HaltFlag) {
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI:  OhBulkIntAddUrb: Ep in 0x%x halted!", Ep->EndpointAddress));
    return USBH_STATUS_ENDPOINT_HALTED;
  }
  if (Ep->PendingUrb == NULL && Ep->AbortMask==0) {
    Ep->UrbCount++;
    if (!DlistEmpty(&Ep->UrbList)) {
      DlistInsertTail(&Ep->UrbList,&Urb->Header.ListEntry);
      status = USBH_STATUS_PENDING;
      OhBulkIntSubmitUrbsFromList(Ep);       // Submit next URB from list
    } else {
      status=OhBulkIntSubmitUrb(Ep,Urb);
      if ( status != USBH_STATUS_PENDING ) { // On error
        USBH_WARN((USBH_MTYPE_OHCI, "OHCI:  OhBulkIntAddUrb: OhEp0SubmitUrb: %08x!", status));
        Ep->UrbCount--;
      }
      return status;
    }
  } else {                                   // URB is pending, add it to the URB list and return pending
    status = USBH_STATUS_PENDING;
    Ep->UrbCount++;
    DlistInsertTail(&Ep->UrbList,&Urb->Header.ListEntry);
  }
  return status;
}

/*********************************************************************
*
*       OhBulkIntUpdateTDLengthStatus
*
*  Function description
*    Called during the first enumeration of the DONE TD list.
*    Updates the EP length and status field.
*    Works without any order of completed TDs.
*/
void OhBulkIntUpdateTDLengthStatus(OHD_GTD * Gtd) {
  OHD_BULK_INT_EP * ep;
  USBH_STATUS       tdStatus;
  U32               length;
  T_BOOL            shortPkt;
  URB             * urb;
  ep              = (OHD_BULK_INT_EP*)Gtd->Ep;
  OH_BULKINT_VALID(ep);
  T_ASSERT(ep->PendingUrb != NULL);
  ep->UrbDoneTdNumber++;                            // Total TDs of the URB
  ep->UpDownTDCounter++;                            // Help count: Number of TDs found in the current DONE list
  urb = ep->PendingUrb;
  // Get TD status and length, shortPkt not used because on bulk and interrupt endpoints
  // only on the last TD the buffer rounding bit is set, on all other TD the ED goes in HALT!
  tdStatus = OhTdGetStatusAndLength(Gtd, &length, &shortPkt);
  if ( tdStatus != USBH_STATUS_SUCCESS) {           // Save the last error
    urb->Header.Status = tdStatus;
#if (USBH_DEBUG > 1)
    if (tdStatus != USBH_STATUS_DATA_UNDERRUN) {
      USBH_WARN((USBH_MTYPE_OHCI, "OHCI: OHCI bulk int transfer error condition code: %u ep: 0x%x!", tdStatus, (int)ep->EndpointAddress));
    }
#endif
  }
  if (shortPkt) {
    ep->Flags |= OH_SHORT_PKT_EPFLAG;
  }
  if (NULL ==ep->CopyBuffer) {                      // URB buffer is used
    urb->Request.BulkIntRequest.Length+=length;
  }  else if( length ) {                            // Bytes transferred, update buffe and pointer
    if ( ep->EndpointAddress & USB_IN_DIRECTION ) { // It is needed to update the copy buffer before it read
      OhCopyToUrbBufferUpdateTransferBuffer(ep->CopyBuffer, length);
    } else {
      OhUpdateCopyTransferBuffer           (ep->CopyBuffer, length);
    }
  }
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: INFO OhBulkIntUpdateTDLengthStatus Ep 0x%x transferred: %u !", (int)ep->EndpointAddress, length));
}

/*********************************************************************
*
*       OhBulkIntCheckForCompletion
*
*  Function description
*    OhBulkIntCheckForCompletion is called during the second enumeration
*    of the DONE TD list. If the pipe is halted (on transfer error) all
*    other pending TDs are removed and the URB is completed. If all TDs
*    are done the URB is also completed. Then the next one is submitted!
*/
void OhBulkIntCheckForCompletion(OHD_GTD * Gtd) {
  OHD_BULK_INT_EP * ep;
  URB             * urb;
  USBH_STATUS       urb_status;
  int               complete;
  USBH_STATUS       status = USBH_STATUS_SUCCESS;
  ep = (OHD_BULK_INT_EP*)Gtd->Ep;
  OH_BULKINT_VALID(ep);
  T_ASSERT_PTR(ep->PendingUrb);
  // Delete TD
  OhEpGlobDeleteDoneTD(&Gtd->ItemHeader,&ep->TdCounter);
  ep->UpDownTDCounter--;
  if (ep->UpDownTDCounter) {
    return;
  }
  urb        = ep->PendingUrb;
  urb_status = urb->Header.Status;
  complete   = FALSE;
  if (NULL == ep->CopyBuffer) {
    if (ep->TdCounter == 1 || USBH_STATUS_PENDING != urb_status) {
      complete = TRUE;
    }
  } else if ((ep->TdCounter == 1 && 0 == ep->CopyBuffer->RemainingLength) || USBH_STATUS_PENDING != urb_status || (ep->Flags & OH_SHORT_PKT_EPFLAG)) {
    complete = TRUE;
  }
  if (!complete) {
    if (ep->AbortMask & EP_ABORT_SKIP_TIME_OVER_MASK) {          // Endpoint is aborted and the host list processing is stopped
      T_ASSERT((urb->Header.HcFlags & URB_CANCEL_PENDING_MASK)); // For testing
      if ( urb->Header.HcFlags & URB_CANCEL_PENDING_MASK ) {     // URB must be canceled
        complete = TRUE;
      }
    }
  }
  {
    U32   length;
    U8  * buffer;

    if (!complete) {
      if (NULL != ep->CopyBuffer) {
        // Copy new OUT buffer / Get buffer and length / Submit the next packet and return on success!
        if (!(ep->EndpointAddress & USB_IN_DIRECTION)) {
          OhFillCopyTransferBuffer(ep->CopyBuffer);
        }
        buffer = OhGetBufferLengthFromCopyTransferBuffer(ep->CopyBuffer, &length);
        if (length) {
          status = OhBulkIntSubmitTransfer(ep, buffer, length);
          if (status) {
            USBH_WARN((USBH_MTYPE_OHCI, "OHCI: OhBulkIntCheckForCompletion:OhBulkIntSubmitTransfer!"));
          }
        }
        if ( !status ) { // On success
          return;
        }
      }
    }
  }
  // URB complete (If all TDs transferred or not all and the HC has stopped the endpoint because of an error or abort)
  // Update URB buffer and length
  if (NULL != ep->CopyBuffer) {
    if (status) {
      urb_status = status;
    }
    urb->Request.BulkIntRequest.Length = ep->CopyBuffer->Transferred;
  }
#if (USBH_DEBUG > 1) // Infos
  if (ep->TdCounter != 1) {
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: INFO OhBulkIntCheckForCompletion: Complete URB: active TDS!"));
  }
  if (OhEpGlobIsSkipped(&ep->ItemHeader)) {
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: INFO OhBulkIntCheckForCompletion: Complete URB: ED is skipped!"));
  }
  if(OhEpGlobIsHalt(&ep->ItemHeader)) {
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: INFO OhBulkIntCheckForCompletion: Complete URB: ED halted!"));
  }
  // URB complete or aborted
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: INFO DONE: BulkInt complete or aborted:Ep: 0x%x transferred: %u status %s:",
                              (unsigned int)ep->EndpointAddress, urb->Request.BulkIntRequest.Length, USBH_GetStatusStr(urb_status)));
#endif

  if (ep->TdCounter > 1) {
   OhEpGlobDeleteAllPendingTD(&ep->ItemHeader,&ep->TdCounter); // Delete all not processed TDs
  }
  if (urb_status == USBH_STATUS_PENDING) {
    urb_status = USBH_STATUS_SUCCESS;
  } else if (urb_status == USBH_STATUS_DATA_UNDERRUN) {
    if (OhEpGlobIsHalt(&ep->ItemHeader)) {
      OhEpGlobClearHalt(&ep->ItemHeader);
    }
    urb_status = USBH_STATUS_SUCCESS;
  }
  if (OhEpGlobIsHalt(&ep->ItemHeader)) {
    ep->HaltFlag=TRUE; // Always halt set flag
  }
  if (ep->AbortMask) {
                       // Endpoint is aborted: Complete pending URB / Clear the skip bit / Complete aborted URBs resubmit new added URBS
    OhBulkIntCheckAndCancelAbortedUrbs(ep, TRUE);
  } else {             // TRUE flag: submit an new URB before the old one is completed
    OhBulkIntCompleteUrb(ep, ep->PendingUrb, urb_status, TRUE);
  }
}

/******************************* EOF ********************************/
