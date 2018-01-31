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
*       _BULK_INT_OnCompleteUrb
*
*  Function description
*    Completes the current bulk USBH_URB. Is called if all TDs of
*    an USBH_URB are done or the USBH_URB is aborted
*
*  Parameters:
*    Urb: Condition code of the transfer or an driver Status
*/
static void _BULK_INT_OnCompleteUrb(USBH_OHCI_BULK_INT_EP * Ep, USBH_URB * Urb, USBH_STATUS Status, int NewUrbFlag) {
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: _BULK_INT_OnCompleteUrb Ep 0x%x length: %lu!", (int)Ep->EndpointAddress, Urb->Request.BulkIntRequest.Length));
  if (Urb != NULL) {
    Urb->Header.Status = Status;
    if (Urb == Ep->pPendingUrb) {         // Current completed USBH_URB
#if (USBH_DEBUG > 1)
      if (Ep->TdCounter > 1) {           // More than the reserved TD is active
        USBH_LOG((USBH_MTYPE_OHCI, "OHCI: INFO _BULK_INT_OnCompleteUrb: Pending TDs!!!"));
      }
#endif
      if (NULL != Ep->pCopyBuffer) {      // Release the copy buffer resource
        Urb->Request.BulkIntRequest.Length = Ep->pCopyBuffer->Transferred;
        USBH_HCM_PutItem(&Ep->pCopyBuffer->ItemHeader);
        Ep->pCopyBuffer = NULL;
      }
      Ep->pPendingUrb = NULL;             // Delete the current pending USBH_URB
    }
    USBH_ASSERT(Ep->UrbCount);
    Ep->UrbCount--;
    if (NewUrbFlag) {
      USBH_OHCI_BULK_INT_SubmitUrbsFromList(Ep);   // Submit next packet before the previous one is completed. All neded previous transfer resources are released!
    }
    Urb->Header.pfOnInternalCompletion(Urb); // Call the USB Bus driver completion routine
  }
}

/*********************************************************************
*
*       _BULK_INT_CompletePendingUrbs
*
*  Function description
*
*  Parameters:
*    HcFlagMask: URBS where one of the flag mask bit is set where completed (mask bits are or'ed)
*                0: all URBs are completed.
*/
static void _BULK_INT_CompletePendingUrbs(USBH_OHCI_BULK_INT_EP * Ep, U32 HcFlagMask) {
  USBH_DLIST  * pListHead;
  USBH_DLIST  * pEntry;
  USBH_DLIST  * pAbortEntry;
  USBH_DLIST    AbortList;
  USBH_URB    * pUrb;
  OH_BULKINT_VALID(Ep);
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: _BULK_INT_CompletePendingUrbs!"));
  USBH_DLIST_Init(&AbortList);
  pListHead = &Ep->UrbList;
  pEntry    = USBH_DLIST_GetNext(pListHead);
  while (pEntry != pListHead) {
    pUrb        = (USBH_URB *)GET_URB_HEADER_FROM_ENTRY(pEntry);
    pAbortEntry = pEntry;
    pEntry      = USBH_DLIST_GetNext(pEntry);
    if (0 == HcFlagMask || (HcFlagMask &pUrb->Header.HcFlags)) { // No mask bit set or the same mask bit is also in the USBH_URB set
      USBH_DLIST_RemoveEntry(pAbortEntry);
      USBH_DLIST_InsertHead(&AbortList, pAbortEntry);
    }
  }
  while (!USBH_DLIST_IsEmpty(&AbortList)) {
    USBH_DLIST_RemoveTail(&AbortList, &pEntry);
    pUrb = (USBH_URB * )GET_URB_HEADER_FROM_ENTRY(pEntry);
    pUrb->Request.BulkIntRequest.Length = 0;                     // Set length to zero
    _BULK_INT_OnCompleteUrb(Ep, pUrb, USBH_STATUS_CANCELED, FALSE);
  }
}

/*********************************************************************
*
*       _BULK_INT_Unlink
*
*  Function description
*    Before this function is called the HC list must be disabled! The
*    endpoint is removed from the Oist bulk list and from the device
*    object bulk list. If the function returns the endpoint is still valid!
*/
static void _BULK_INT_Unlink(USBH_OHCI_BULK_INT_EP * Ep) {
  USBH_OHCI_DEVICE  * pDev;
  USBH_DLIST           * pPreviousEntry;
  USBH_OHCI_BULK_INT_EP * previousEp;
  U32               nextPhyAddr;

  OH_BULKINT_VALID(Ep);
  pDev             = Ep->pDev;
  USBH_OCHI_IS_DEV_VALID(pDev);
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: _BULK_INT_Unlink: Ep: 0x%x!",Ep->EndpointAddress));
  USBH_ASSERT( NULL == Ep->pPendingUrb);
  USBH_ASSERT( 0==Ep->UrbCount);
  USBH_ASSERT( pDev->BulkEpCount);
  USBH_ASSERT( Ep->State != OH_EP_IDLE);
  // HC list must be disabled
  USBH_ASSERT( 0==OhHalTestReg(pDev->pRegBase, OH_REG_CONTROL, HC_CONTROL_BLE));
  OhHalWriteReg(pDev->pRegBase,OH_REG_BULKCURRENTED, 0);
  USBH_ASSERT(!USBH_DLIST_IsEmpty(&pDev->BulkEpList));
  pPreviousEntry = USBH_DLIST_GetPrev(&Ep->ListEntry); // Get the list tail
  if ( pPreviousEntry == &pDev->BulkEpList ) {
    USBH_LOG((USBH_MTYPE_OHCI, "OHCI: INFO _BULK_INT_Unlink: remove the first ED in the  ED list!"));
    nextPhyAddr = USBH_OHCI_EpGlobUnlinkEd(NULL, &Ep->ItemHeader);
    OhHalWriteReg(pDev->pRegBase, OH_REG_BULKHEADED, nextPhyAddr);
  } else {
    previousEp = GET_BULKINT_EP_FROM_ENTRY(pPreviousEntry);
    OH_BULKINT_VALID(previousEp);
    USBH_OHCI_EpGlobUnlinkEd(&previousEp->ItemHeader, &Ep->ItemHeader);
  }
  USBH_DLIST_RemoveEntry(&Ep->ListEntry);
  Ep->State = OH_EP_IDLE;
  if (0==pDev->BulkEpCount) {
    USBH_LOG((USBH_MTYPE_OHCI, "OHCI: INFO _BULK_INT_Unlink: ED list now empty!"));
  }
  USBH_ASSERT(pDev->BulkEpCount);
  pDev->BulkEpCount--;
}

/*********************************************************************
*
*       _BULK_INT_SubmitTransfer
*
*  Function description
*/
static USBH_STATUS _BULK_INT_SubmitTransfer(USBH_OHCI_BULK_INT_EP * Ep, void * Buffer, U32 Length) {
  USBH_STATUS   Status;
  USBH_OHCI_INFO_GENERAL_TRANS_DESC     * td;
  USBH_OHCI_TD_PID    pid;
  U32           StartAddr, EndAddr;
  U32           TDword0Mask;
  USBH_OCHI_IS_DEV_VALID(Ep->pDev);
  USBH_ASSERT( 0==Ep->AbortMask);
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: _BULK_INT_SubmitTransfer Ep: 0x%x Length: %lu!",(int)Ep->EndpointAddress,Length ));
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
  Status = USBH_STATUS_SUCCESS;
  for (; ;) {
    // Get the Tail TD
    td = USBH_OHCI_EpGlobGetLastTDFromED(&Ep->ItemHeader);
    if (td==NULL) {
      USBH_WARN((USBH_MTYPE_OHCI, "OHCI: FATAL _BULK_INT_SubmitTransfer: USBH_OHCI_EpGlobGetLastTDFromED!"));
      Status = USBH_STATUS_ERROR;
      break;
    }
    // Append all needed TDs
    if ( Length==0 ) { // Zero length IN packet
      if ( Ep->EndpointAddress &  USB_IN_DIRECTION) {
         TDword0Mask |=OHCI_TD_R;
      }
      USBH_LOG((USBH_MTYPE_OHCI, "OHCI: USBH_OHCI_TdInit: pEP: 0x%x ",Ep->EndpointAddress)); // Init the TD
      // Set the previous TD and allocate and insert a new one
      USBH_OHCI_TdInit(td,Ep,Ep->EndpointType,pid,0,0,TDword0Mask);
      td = USBH_OHCI_GetTransDesc(&Ep->pDev->GTDPool);
      if (td == NULL) {
        USBH_WARN((USBH_MTYPE_OHCI, "OHCI: FATAL USBH_OHCI_BulkIntAllocAndFillTD: USBH_OHCI_GetTransDesc!"));
        Status = USBH_STATUS_MEMORY;
        break;
      }
      // Start the TD, set the EP Tail pointer
      Ep->UrbTotalTdNumber++;
      USBH_OHCI_EpGlobInsertTD(&Ep->ItemHeader, &td->ItemHeader, &Ep->TdCounter);
      break;
    } else { // Write all transfer descriptors
      // Setup all needed TDs if the data length unequal zero!
      // Set the buffer rounding bit only on the last page on IN token!
      // If a zero length IN packet is received no on the last page a USB data underrun
      // is generated form the HC and the DONE routine removes all other TDs and restart this endpoint!
      StartAddr = (U32)Buffer;
      EndAddr   = StartAddr + Length - 1;
      if ( Ep->EndpointAddress &  USB_IN_DIRECTION) { // On the last transfer descriptor of all submitted TDs allow short packets
         TDword0Mask |=OHCI_TD_R;
      }
      USBH_LOG((USBH_MTYPE_OHCI, "OHCI: USBH_OHCI_TdInit: pEP: 0x%x ",Ep->EndpointAddress)); // Init the TD
      USBH_OHCI_TdInit(td, Ep, Ep->EndpointType, pid, StartAddr, EndAddr, TDword0Mask);     // Set the previous TD and allocate and insert an new one
      td=USBH_OHCI_GetTransDesc(&Ep->pDev->GTDPool);                                                // Get an new TD
      if (td == NULL) {
        USBH_WARN((USBH_MTYPE_OHCI, "OHCI: FATAL _BULK_INT_SubmitTransfer: USBH_OHCI_GetTransDesc!"));
        Status = USBH_STATUS_MEMORY;
        break;
      }
      // Start the TD, set the EP Tail pointer
      Ep->UrbTotalTdNumber++;
      USBH_OHCI_EpGlobInsertTD(&Ep->ItemHeader, &td->ItemHeader, &Ep->TdCounter);
      break;
    }
  }
  // As last restart the endpoint list
  if ( !Ep->pDev->BulkEpRemoveTimerRunFlag ) {
    // No other endpoint is removed
    USBH_OHCI_EndpointListEnable(Ep->pDev, Ep->EndpointType, TRUE, TRUE);
  }
  return Status;
}

/*********************************************************************
*
*       _EP,_BULK_INT_SubmitUrb
*
*  Function description
*    Submits an bulk USBH_URB. One USBH_URB is submitted at the time. More than
*    one transfer descriptor can be submitted.
*
*  Returns:
*    On success: USBH_STATUS_PENDING:
*    On error:   Other values
*/
static USBH_STATUS _BULK_INT_SubmitUrb(USBH_OHCI_BULK_INT_EP * Ep, USBH_URB * urb) {
  USBH_STATUS             status;
  USBH_BULK_INT_REQUEST * bulkIntRequest;
  USBH_OHCI_DEVICE        * pDev;
  U32                     length;
  U8                    * buffer;
  OH_BULKINT_VALID(Ep);
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: _EP,_BULK_INT_SubmitUrb: Ep: 0x%x!",Ep->EndpointAddress));

  // The PendingURB is submitted
  USBH_ASSERT(urb != NULL);
  USBH_ASSERT(Ep->pPendingUrb == NULL);
  USBH_ASSERT(0 == (urb->Header.HcFlags & URB_CANCEL_PENDING_MASK) );
  // Assert only the default TD is in the ED TD list
  USBH_ASSERT(Ep->TdCounter == 1 );
  USBH_ASSERT(!USBH_OHCI_EpGlobIsHalt(&Ep->ItemHeader) );
  pDev = Ep->pDev;
  USBH_OCHI_IS_DEV_VALID(pDev);
  bulkIntRequest = &urb->Request.BulkIntRequest;
  if (!USBH_IS_VALID_TRANSFER_MEM(bulkIntRequest->pBuffer)) {
    // No valid transfer memory transfer data in pieces!
    //   Allocate and initialize the new transfer buffer on error complete the request, only the SETUP packet is transferered!
    //   Get new pointer and length
    //   If this is an OUT packet copy data to the new transfer buffer
    USBH_ASSERT(NULL == Ep->pCopyBuffer);
    Ep->pCopyBuffer = USBH_OHCI_GetInitializedCopyTransferBuffer(&pDev->TransferBufferPool, (U8 *)bulkIntRequest->pBuffer, bulkIntRequest->Length);
    if (NULL == Ep->pCopyBuffer) {
      USBH_WARN((USBH_MTYPE_OHCI, "OHCI: USBH_OHCI_BulkSubmitUrb!"));
      status = USBH_STATUS_RESOURCES;
      goto exit;
    } else { // On success
      if ( !( Ep->EndpointAddress & USB_IN_DIRECTION) ) {                        // OUT packet
        USBH_OHCI_FillCopyTransferBuffer(Ep->pCopyBuffer);
      }
      buffer = USBH_OHCI_GetBufferLengthFromCopyTransferBuffer(Ep->pCopyBuffer, &length); // pBuffer and length are updated
    }
  } else {
    buffer = (U8 *)bulkIntRequest->pBuffer;
    length = bulkIntRequest->Length;
  }
  bulkIntRequest->Length = 0;                                                    // Clear USBH_URB length and submit buffer
  status                 = _BULK_INT_SubmitTransfer(Ep,buffer,length);
  if (status) {
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: _BULK_INT_SubmitTransfer!"));
    if (NULL != Ep->pCopyBuffer) {
      USBH_HCM_PutItem(&Ep->pCopyBuffer->ItemHeader);
      Ep->pCopyBuffer = NULL;
    }
  } else { // On success set pending USBH_URB
    Ep->pPendingUrb = urb;
    status         = USBH_STATUS_PENDING;
  }
exit:
  return status;
}

/*********************************************************************
*
*       Global functions
*
**********************************************************************
*/

/*********************************************************************
*
*       USBH_OHCI_BULK_INT_SubmitUrbsFromList
*
*  Function description
*    Submit the next USBH_URB from an bulk or interrupt endpoint
*/
void USBH_OHCI_BULK_INT_SubmitUrbsFromList(USBH_OHCI_BULK_INT_EP * Ep) {
  USBH_URB         * urb;
  USBH_STATUS   status;
  USBH_DLIST       * pEntry;
  int           emptyFlag;
  OH_BULKINT_VALID(Ep);
  if (Ep->State     != OH_EP_LINK) {
    USBH_LOG((USBH_MTYPE_OHCI, "OHCI: USBH_OHCI_BULK_INT_SubmitUrbsFromList: EP 0x%x is unlinked!",       (unsigned int)Ep->EndpointAddress));
    return;
  }
  if (Ep->AbortMask != 0){
    USBH_LOG((USBH_MTYPE_OHCI, "OHCI: USBH_OHCI_BULK_INT_SubmitUrbsFromList: urb on EP 0x%x is aborted!", (unsigned int)Ep->EndpointAddress));
    return;
  }
  if (USBH_OHCI_EpGlobIsHalt(&Ep->ItemHeader)) {
    USBH_LOG((USBH_MTYPE_OHCI, "OHCI: USBH_OHCI_BULK_INT_SubmitUrbsFromList: EP 0x%x is halted!",         (unsigned int)Ep->EndpointAddress));
    return;
  }
  if (NULL          != Ep->pPendingUrb) {
    return;
  }
  emptyFlag = USBH_DLIST_IsEmpty(&Ep->UrbList);
  while(!emptyFlag) { // Submit the next USBH_URB
    USBH_DLIST_RemoveHead(&Ep->UrbList, &pEntry);
    emptyFlag = USBH_DLIST_IsEmpty(&Ep->UrbList);
    urb       = (USBH_URB*)GET_URB_HEADER_FROM_ENTRY(pEntry);
    status    = _BULK_INT_SubmitUrb(Ep, urb);
    if (status == USBH_STATUS_PENDING) { // On success stop
      break;
    } else { // pUrb can not be submitted
      if (Ep->UrbCount==1) {
        _BULK_INT_OnCompleteUrb(Ep, Ep->pPendingUrb, status, FALSE); // Endpoint can be invalid
        break;
      } else {
        _BULK_INT_OnCompleteUrb(Ep, Ep->pPendingUrb, status, FALSE);
      }
    }
  }
}

/*********************************************************************
*
*       USBH_OHCI_BULK_INT_CheckAndCancelAbortedUrbs
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
*                          2. The next not aborted USBH_URB in the USBH_URB list is submitted
*    USBH_STATUS_ERROR:    The pending USBH_URB cannot completed because an TD is lost (in the OHCI cache)!
*/
USBH_STATUS USBH_OHCI_BULK_INT_CheckAndCancelAbortedUrbs(USBH_OHCI_BULK_INT_EP * Ep, int TDDoneFlag) {
  USBH_BOOL td_in_donecache_queue_flag = FALSE;
  USBH_OHCI_INFO_GENERAL_TRANS_DESC * gtd;
  USBH_BOOL shortPkt                   = FALSE;
  U32       length, tdCounter;
  USBH_BOOL complete_flag              = FALSE;

  USBH_ASSERT(Ep->AbortMask);
  if (NULL != Ep->pPendingUrb) {                                          // An USBH_URB is pending
    if ( !TDDoneFlag ) {                                                 // Called from the DONE routine
      tdCounter = USBH_OHCI_EpGlobGetTdCount(&Ep->ItemHeader, &Ep->pDev->GTDPool);
      USBH_ASSERT( tdCounter >= 1 );                                        // Compare sum of TDs
      if ((tdCounter-1) + Ep->UrbDoneTdNumber != Ep->UrbTotalTdNumber) { // TD lost, -1 do not use the dummy TD, waits for DONE interrupt!
        USBH_LOG((USBH_MTYPE_OHCI, "OHCI: INFO USBH_OHCI_BULK_INT_CheckAndCancelAbortedUrbs: lost TD, wait for DONE!"));
        td_in_donecache_queue_flag = TRUE;
      } else {                                                           // No TD lost, complete pending USBH_URB with the length of the last pending TD
        gtd = USBH_OHCI_EpGlobGetFirstTDFromED(&Ep->ItemHeader);
        USBH_OHCI_TdGetStatusAndLength(gtd, &length, &shortPkt);                 // Only the length is checked of the returned values!
        Ep->pPendingUrb->Request.BulkIntRequest.Length += length;         // Update the length of the canceled USBH_URB by the last active TD
        USBH_OHCI_EpGlobDeleteAllPendingTD(&Ep->ItemHeader, &Ep->TdCounter);
        complete_flag = TRUE;
      }
    } else {
      complete_flag = TRUE;
    }
    if (complete_flag) {
      USBH_OHCI_EpGlobClearSkip(&Ep->ItemHeader);
      Ep->AbortMask = 0;
      _BULK_INT_OnCompleteUrb(Ep, Ep->pPendingUrb, USBH_STATUS_CANCELED, FALSE);
    }
  } else { // No pending USBH_URB
    Ep->AbortMask = 0;
    USBH_OHCI_EpGlobClearSkip(&Ep->ItemHeader);
  }
  if (!td_in_donecache_queue_flag) {
    _BULK_INT_CompletePendingUrbs(Ep, URB_CANCEL_PENDING_MASK);
    USBH_OHCI_BULK_INT_SubmitUrbsFromList(Ep); // Submit the next canceled USBH_URB if available
    return USBH_STATUS_SUCCESS;
  } else { // Pending TD in Host cache
    return USBH_STATUS_ERROR;
  }
}

/*********************************************************************
*
*       USBH_OHCI_BULK_RemoveEps
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
void USBH_OHCI_BULK_RemoveEps(USBH_OHCI_DEVICE * pDev, USBH_BOOL AllEndpointFlag) {
  USBH_OHCI_BULK_INT_EP                 * pEP;
  USBH_DLIST                           * pEntry;
  USBH_DLIST                             RemoveList;
  USBH_RELEASE_EP_COMPLETION_FUNC * pfCompletion;
  void                            * pContext;
  USBH_OCHI_IS_DEV_VALID(pDev);
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: USBH_OHCI_BULK_RemoveEps!"));
#if (USBH_DEBUG > 1)
  if ( OhHalTestReg(pDev->pRegBase, OH_REG_CONTROL, HC_CONTROL_BLE)) {
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: FATAL USBH_OHCI_BULK_RemoveEps: bulk list not disabled!"));
  }
#endif

  // Remove needed endpoints from the list
  USBH_DLIST_Init(&RemoveList);
  pEntry = USBH_DLIST_GetPrev(&pDev->BulkEpList);
  while (pEntry != &pDev->BulkEpList)  {
    USBH_ASSERT(pDev->BulkEpCount);
    pEP    = GET_BULKINT_EP_FROM_ENTRY(pEntry);
    OH_BULKINT_VALID(pEP);
    pEntry = USBH_DLIST_GetPrev(pEntry);
    if (AllEndpointFlag) {
      _BULK_INT_Unlink(pEP);
      USBH_DLIST_InsertHead(&RemoveList, &pEP->ListEntry);
    } else {
      if ( pEP->State == OH_EP_UNLINK ) { // Remove only endpoints where the OH_EP_UNLINK flag is set
        _BULK_INT_Unlink(pEP);
        USBH_DLIST_InsertHead(&RemoveList, &pEP->ListEntry);
      }
    }
  }
  // After unlinking the endpoint restart the HC list processing
  if (pDev->BulkEpCount > 1) { // On active endpoints
    USBH_OHCI_EndpointListEnable(pDev, USB_EP_TYPE_BULK, TRUE, TRUE);
  }
#if (USBH_DEBUG > 1)
  if(pDev->BulkEpCount == 0) {
    USBH_LOG((USBH_MTYPE_OHCI, "OHCI: INFO USBH_OHCI_Ep0RemoveEndpoints: all control endpoints removed!"));
  }
#endif
  // Call the release completion functions of all removed endpoints
  while (!USBH_DLIST_IsEmpty(&RemoveList)) {
    USBH_DLIST_RemoveHead(&RemoveList, &pEntry);
    pEP = GET_BULKINT_EP_FROM_ENTRY(pEntry);
    OH_BULKINT_VALID(pEP);
    pfCompletion = pEP->pfOnReleaseCompletion;
    pContext     = pEP->pReleaseContext;
    USBH_OHCI_BULK_INT_PutEp(pEP);
    if(pfCompletion) {  // Call the completion routine, attention: runs in this context
      pfCompletion((void *)pContext);
    }
  }
}

/*********************************************************************
*
*       USBH_OHCI_BULK_INT_OnRemoveEpTimer
*
*  Function description
*    Called if one or more bulk endpoints has been removed
*/
void USBH_OHCI_BULK_INT_OnRemoveEpTimer(void * pContext) {
  USBH_OHCI_DEVICE * pDev;

  pDev       = (USBH_OHCI_DEVICE *)pContext;
  USBH_OCHI_IS_DEV_VALID(pDev);
  if (pDev->BulkEpRemoveTimerCancelFlag) {
    pDev->BulkEpRemoveTimerCancelFlag = FALSE;
    return;
  }
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: USBH_OHCI_BULK_INT_OnRemoveEpTimer!"));
  pDev->BulkEpRemoveTimerRunFlag = FALSE;                              // Timer is stopped
  USBH_OHCI_BULK_RemoveEps(pDev, FALSE);
}

/*********************************************************************
*
*       USBH_OHCI_BULK_INT_MarkUrbsWithCancelPendingFlag
*
*  Function description
*/
static void USBH_OHCI_BULK_INT_MarkUrbsWithCancelPendingFlag(USBH_OHCI_BULK_INT_EP * pEP) {
  USBH_DLIST  * pListHead;
  USBH_DLIST  * pEntry;
  USBH_URB    * pUrb;
  OH_BULKINT_VALID(pEP);
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: USBH_OHCI_BULK_INT_MarkUrbsWithCancelPendingFlag!"));
  pListHead = &pEP->UrbList;
  pEntry    = USBH_DLIST_GetNext(pListHead);
  while (pEntry != pListHead) {
    pUrb = (USBH_URB *)GET_URB_HEADER_FROM_ENTRY(pEntry); // The USBH_URB header is the first element in the USBH_URB, cast it
#if (USBH_DEBUG > 1)
    if (pUrb->Header.HcFlags & URB_CANCEL_PENDING_MASK) {
      USBH_WARN((USBH_MTYPE_OHCI, "OHCI: USBH_OHCI_BULK_INT_MarkUrbsWithCancelPendingFlag: URB_CANCEL_PENDING_MASK already set!"));
    }
#endif
    pUrb->Header.HcFlags |= URB_CANCEL_PENDING_MASK;
    pEntry = USBH_DLIST_GetNext(pEntry);
  }
}

/*********************************************************************
*
*       USBH_OHCI_BULK_OnAbortUrbTimer
*
*  Function description
*    The timer routine runs if an bulk endpoint is aborted and an USBH_URB
*    of the endpoint at the time where the endpoint is aborted is submitted
*    and pending. The endpoint is always skipped if the endpoint is aborted.
*    This routine calls the abort request completion routine!
*    Cancel all URBs on skipped EDs, this function is always called after a
*    timeout of more than one frame after the ED SKIP bit is set
*
*  Parameters
*    pContext: pDev Ptr.
*/
void USBH_OHCI_BULK_OnAbortUrbTimer(void * pContext) {
  USBH_OHCI_DEVICE  * pDev;
  USBH_BOOL         restart_flag;
  USBH_DLIST           * pEntry;
  USBH_OHCI_BULK_INT_EP * pEP;
  USBH_STATUS       Status;
  USBH_BOOL         StartTimer = FALSE;

  USBH_ASSERT(pContext != NULL);
  pDev = (USBH_OHCI_DEVICE*)pContext;
  USBH_OCHI_IS_DEV_VALID(pDev);
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: USBH_OHCI_BULK_OnAbortUrbTimer!"));

  if (pDev->BulkEpAbortTimerCancelFlag) {
    pDev->BulkEpAbortTimerCancelFlag = FALSE;
    return;
  }
  pDev->BulkEpAbortTimerRunFlag = FALSE;

  // Mark all aborted endpoints with EP_ABORT_SKIP_TIME_OVER_MASK mask
  pEntry=USBH_DLIST_GetNext(&pDev->BulkEpList);
  while (pEntry != &pDev->BulkEpList) {
    pEP = GET_BULKINT_EP_FROM_ENTRY(pEntry);
    OH_BULKINT_VALID(pEP);
    if (pEP->AbortMask & EP_ABORT_MASK) {
      pEP->AbortMask |= EP_ABORT_SKIP_TIME_OVER_MASK | EP_ABORT_PROCESS_FLAG_MASK;
    } else if (pEP->AbortMask & EP_ABORT_START_TIMER_MASK) { // Start timer again for aborted endpoints where the timer already runs
      StartTimer = TRUE;
    }
    pEntry = USBH_DLIST_GetNext(pEntry);
  }
  if (StartTimer) {
    pDev->BulkEpAbortTimerRunFlag = TRUE;
  }
  restart_flag = TRUE;
  while(restart_flag) {
    restart_flag = FALSE;
    pEntry        = USBH_DLIST_GetNext(&pDev->BulkEpList);
    while (pEntry != &pDev->BulkEpList) {
      pEP    = GET_BULKINT_EP_FROM_ENTRY(pEntry);
      pEntry = USBH_DLIST_GetNext(pEntry);
      OH_BULKINT_VALID(pEP);
      if ((pEP->AbortMask & EP_ABORT_PROCESS_FLAG_MASK)) { // Endpoint aborted
        pEP->AbortMask &= ~EP_ABORT_PROCESS_FLAG_MASK;
    #if (USBH_DEBUG > 1)
        if (pEP->pPendingUrb) {
         USBH_ASSERT(USBH_OHCI_EpGlobIsHalt(&pEP->ItemHeader) || USBH_OHCI_EpGlobIsSkipped(&pEP->ItemHeader));
        }
    #endif
        Status = USBH_OHCI_BULK_INT_CheckAndCancelAbortedUrbs(pEP, FALSE);
        if (!Status) {
          // On success it is possible that the completion routine is called and the BulkEpList can be changed
          restart_flag = TRUE;
          break;
        }
      }
    }
  }
  // Start timer at the end of the timer routine
  if (StartTimer) {
    pDev->BulkEpAbortTimerCancelFlag = FALSE;
    USBH_StartTimer(pDev->hBulkEpAbortTimer, OH_STOP_DELAY_TIME);
  }
}

/*********************************************************************
*
*       USBH_OHCI_BULK_INT_AbortEp
*
*  Function description
*    Aborts all pending URBs. It is allowed to abort more than endpoints
*    at the time. Only after a frame timeout of the last aborted endpoint
*    the timer callback routine is called! Because the bulk and interrupt
*    endpoints have different lists different timer callback routines are used.
*/
USBH_STATUS USBH_OHCI_BULK_INT_AbortEp(USBH_OHCI_BULK_INT_EP * pEP) {
  USBH_OHCI_DEVICE * pDev;
  USBH_STATUS      Status;

  OH_BULKINT_VALID(pEP);
  pDev         = pEP->pDev;
  USBH_OCHI_IS_DEV_VALID(pDev);
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: USBH_OHCI_BULK_INT_AbortEp: pEP:0x%x!", pEP->EndpointAddress));
  Status      = USBH_STATUS_SUCCESS;
  // If an TD is pending the Skip bit is set and an timer is scheduled
  // If in the timer routine all pending TDs are removed.
  // Not pending URBs with the abort Flag are removed.
  // As last the Skip bit is reset.
  if (pEP->AbortMask & (EP_ABORT_MASK | EP_ABORT_START_TIMER_MASK)) { // Already aborted
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: USBH_OHCI_BULK_INT_AbortEp: Endpoint 0x%x already aborted!",pEP->EndpointAddress));
    USBH_OHCI_BULK_INT_MarkUrbsWithCancelPendingFlag(pEP);
    return USBH_STATUS_SUCCESS;
  }
  if (NULL == pEP->pPendingUrb) {
    _BULK_INT_CompletePendingUrbs(pEP,0); // Cancel the USBH_URB list without skipping of the endpoint
  } else {
    // Skip the endpoint from list processing, wait a frame time until the TD can removed from the endpoint.
    // Because one timer is used for all control endpoints restart the timer in the timer routine if the timer already started.
    USBH_OHCI_EpGlobSetSkip(&pEP->ItemHeader);
    pEP->pPendingUrb->Header.HcFlags |=URB_CANCEL_PENDING_MASK;
    pEP->AbortMask |= EP_ABORT_MASK;
    USBH_OHCI_BULK_INT_MarkUrbsWithCancelPendingFlag(pEP);
    if (pEP->EndpointType == USB_EP_TYPE_INT) { // Interrupt endpoint
      if (!pDev->IntEpAbortTimerRunFlag) {
        pDev->IntEpAbortTimerRunFlag    = TRUE;
        pDev->IntEpAbortTimerCancelFlag = FALSE;
        USBH_StartTimer(pDev->hIntEpAbortTimer, OH_STOP_DELAY_TIME);
      } else {
        pEP->AbortMask |= EP_ABORT_START_TIMER_MASK;
      }
    } else { // Bulk endpoint
      if (!pDev->BulkEpAbortTimerRunFlag) {
        pDev->BulkEpAbortTimerRunFlag    = TRUE;
        pDev->BulkEpAbortTimerCancelFlag = FALSE;
        USBH_StartTimer(pDev->hBulkEpAbortTimer, OH_STOP_DELAY_TIME);
      } else {
        pEP->AbortMask |= EP_ABORT_START_TIMER_MASK;
      }
    }
  }
  return Status;
}

/*********************************************************************
*
*       USBH_OHCI_BULK_INT_AllocPool
*
*  Function description
*    Allocate all bulk or interrupt endpoints from an specified pool
*/
USBH_STATUS USBH_OHCI_BULK_INT_AllocPool(USBH_HCM_POOL * pEpPool, unsigned int MaxEps) {
  USBH_STATUS Status;
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: USBH_OHCI_BULK_INT_AllocPool: Eps:%d!",MaxEps));
  if (!MaxEps) {
  return USBH_STATUS_SUCCESS;
  }
  Status = USBH_HCM_AllocPool(pEpPool, MaxEps, OH_ED_SIZE, sizeof(USBH_OHCI_BULK_INT_EP), OH_ED_ALIGNMENT);
  if ( Status ) { // On error
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: USBH_OHCI_BULK_INT_AllocPool: USBH_HCM_AllocPool!"));
  }
  return Status;
}

/*********************************************************************
*
*       USBH_OHCI_BULK_INT_GetEp
*
*  Function description
*    Get an bulk or interrupt endpoint from an pool
*/
USBH_OHCI_BULK_INT_EP * USBH_OHCI_BULK_INT_GetEp(USBH_HCM_POOL * pEpPool) {
  USBH_OHCI_BULK_INT_EP * pItem;
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: USBH_OHCI_BULK_INT_GetEp!"));
  pItem = (USBH_OHCI_BULK_INT_EP*)USBH_HCM_GetItem(pEpPool);
  if (NULL==pItem) {
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: USBH_OHCI_BulkEpGet: no resources!"));
  } else {
    USBH_ASSERT(USBH_IS_ALIGNED(pItem->ItemHeader.PhyAddr, OH_ED_ALIGNMENT));
    USBH_DLIST_Init(&pItem->ItemHeader.Link.ListEntry); // Init the pItem link list for later deallocating TDs
  }
  return pItem;
}

/*********************************************************************
*
*       USBH_OHCI_BULK_INT_PutEp
*
*  Function description
*    Puts an bulk or interrupt endpoint back to the pool
*/
void USBH_OHCI_BULK_INT_PutEp(USBH_OHCI_BULK_INT_EP * pEP) {
  OH_BULKINT_VALID(pEP);
  // Put all TD items back to the TD pool before put back the Ep0 object
  USBH_OHCI_EpGlobRemoveAllTDtoPool(&pEP->ItemHeader, &pEP->TdCounter);
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: USBH_OHCI_BULK_INT_PutEp!"));
  if (NULL != pEP->pCopyBuffer) {
    USBH_HCM_PutItem(&pEP->pCopyBuffer->ItemHeader);
    pEP->pCopyBuffer = NULL;
  }
  USBH_HCM_PutItem(&pEP->ItemHeader);
}

/*********************************************************************
*
*       USBH_OHCI_BULK_INT_InitEp
*
*  Function description
*    Initialize an bulk or interrupt endpoint
*/
USBH_OHCI_BULK_INT_EP * USBH_OHCI_BULK_INT_InitEp(USBH_OHCI_BULK_INT_EP * pEP, USBH_OHCI_DEVICE * pDev, 
                                                U8  EndpointType, U8  DeviceAddress, U8 EndpointAddress, 
                                                U16 MaxFifoSize,  U16 IntervalTime,  USBH_SPEED Speed, U32 Flags) {
  USBH_HCM_ITEM_HEADER * pItem;
  USBH_OHCI_INFO_GENERAL_TRANS_DESC         * td;
  USBH_BOOL             SkipFlag;

  OH_BULKINT_VALID(pEP);        // The itemheader must be valid
  USBH_OCHI_IS_DEV_VALID(pDev);
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: USBH_OHCI_BULK_INT_InitEp: pDev.Addr: %d EpAddr.: 0x%x max.pktsize: %d interval: %d", (int)DeviceAddress, (int)EndpointAddress, (int)MaxFifoSize, (int)IntervalTime));
  // Recalculate the interval time
  pEP->pCopyBuffer        = NULL;
  pEP->EndpointType      = EndpointType;
  pEP->pDev               = pDev;
  pEP->State             = OH_EP_IDLE;
  USBH_DLIST_Init(&pEP->ListEntry);
  USBH_DLIST_Init(&pEP->UrbList);
  pEP->UrbCount          = 0;
  pEP->pPendingUrb       = NULL;
  pEP->UrbTotalTdNumber  = 0;
  pEP->UrbDoneTdNumber   = 0;
  pEP->UpDownTDCounter   = 0;
  pEP->TdCounter         = 0;
  pEP->AbortMask         = 0;
  pEP->CancelPendingFlag = FALSE;
  // Parameter
  pEP->pDummyIntEp        = NULL;
  pEP->pfOnReleaseCompletion = NULL;
  pEP->pReleaseContext   = NULL;
  pEP->Flags             = Flags;
  pEP->DeviceAddress     = DeviceAddress;
  pEP->EndpointAddress   = EndpointAddress;
  pEP->MaxPacketSize     = MaxFifoSize;
  pEP->Speed             = Speed;
  if (EndpointType == USB_EP_TYPE_INT) { // Calculate the interval time
    if (IntervalTime > 31) {
      pEP->IntervalTime  = 32;
    } else {
      if (IntervalTime > 15) {
        pEP->IntervalTime  = 16;
      } else {
        if (IntervalTime > 7) {
          pEP->IntervalTime  = 8;
        } else {
          if (IntervalTime > 3) {
            pEP->IntervalTime  = 4;
          } else {
            if (IntervalTime > 1) {
              pEP->IntervalTime  = 2;
            } else {
              pEP->IntervalTime  = 1;
            }
          }
        }
      }
    }
  } else {
    pEP->IntervalTime    = IntervalTime;
  }
  pEP->HaltFlag = FALSE;
  // Allocate a timer
  if ( Flags & OH_DUMMY_ED_EPFLAG ) {
    SkipFlag = TRUE;
  } else {
    SkipFlag = FALSE;
  }
  pItem = &pEP->ItemHeader;
  USBH_OHCI_EpGlobInitED(pItem, DeviceAddress, EndpointAddress, MaxFifoSize, FALSE, SkipFlag, Speed); // Init DWORD 0..DWORD 3 in the ED
  td = USBH_OHCI_GetTransDesc(&pDev->GTDPool);                                                               // Get an TD pItem from pool
  if (td == NULL) {
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: USBH_OHCI_BulkEpInit: USBH_OHCI_GetTransDesc!"));
    USBH_OHCI_BULK_INT_PutEp(pEP);                                                                      // Release the endpoint
    pEP = NULL;
    goto exit;
  }
  USBH_OHCI_EpGlobInsertTD(pItem,&td->ItemHeader,&pEP->TdCounter);                                     // Link the new TD to the EP TD list, set the bulk list filled bit
exit:
  return pEP;                                                                                 // On error an NULL pointer is returned
}

/*********************************************************************
*
*       USBH_OHCI_BULK_InsertEp
*/
/* Insert an endpoint in the devices endpoint list and in the HC Ed list */
void USBH_OHCI_BULK_InsertEp(USBH_OHCI_BULK_INT_EP * pEP) {
  USBH_OHCI_DEVICE       * pDev;
  U8              * pBase;
  USBH_OHCI_BULK_INT_EP * pTailEP;
  USBH_DLIST           * pEntry;

  OH_BULKINT_VALID(pEP);
  pDev = pEP->pDev;
  USBH_OCHI_IS_DEV_VALID(pDev);
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: USBH_OHCI_BULK_InsertEp: pEP: 0x%x!", pEP->EndpointAddress));
  pBase = pEP->pDev->pRegBase;
  if (0 == pEP->pDev->BulkEpCount) { // Empty bulk ED list
    USBH_ASSERT(OhHalReadReg(pBase, OH_REG_BULKHEADED) == 0);
    OhHalWriteReg(pBase, OH_REG_BULKHEADED, pEP->ItemHeader.PhyAddr);
  } else {
    USBH_ASSERT (!USBH_DLIST_IsEmpty(&pDev->BulkEpList));
    pEntry  = USBH_DLIST_GetPrev(&pDev->BulkEpList);
    pTailEP = GET_BULKINT_EP_FROM_ENTRY(pEntry);
    OH_BULKINT_VALID(pTailEP);
    USBH_OHCI_EpGlobLinkEds(&pTailEP->ItemHeader, &pEP->ItemHeader);
  }
  pEP->State = OH_EP_LINK;
  USBH_DLIST_InsertTail(&pDev->BulkEpList,&pEP->ListEntry); // Logical link
  pDev->BulkEpCount++;
}

/*********************************************************************
*
*       USBH_OHCI_BULK_ReleaseEp
*
*  Function description
*    Starts removing of an endpoint from the HC list.
*/
void USBH_OHCI_BULK_ReleaseEp(USBH_OHCI_BULK_INT_EP * pEP, USBH_RELEASE_EP_COMPLETION_FUNC * pfReleaseEpCompletion, void * pContext) {
  USBH_OHCI_DEVICE * pDev;
  OH_BULKINT_VALID(pEP);
  pDev       = pEP->pDev;
  USBH_OCHI_IS_DEV_VALID(pDev);
  USBH_ASSERT(USBH_DLIST_IsEmpty(&pEP->UrbList) );
  USBH_ASSERT(pEP->pPendingUrb == NULL);

  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: INFO USBH_OHCI_BULK_ReleaseEp 0x%x!", pEP->EndpointAddress));
  pEP->pReleaseContext    = pContext;
  pEP->pfOnReleaseCompletion = pfReleaseEpCompletion;
  if (pEP->State == OH_EP_UNLINK) {
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: USBH_OHCI_BULK_ReleaseEp: Endpoint already unlinked, return!"));
    return;
  }
  pEP->State = OH_EP_UNLINK;
  USBH_OHCI_EpGlobSetSkip(&pEP->ItemHeader);
  if (!pDev->BulkEpRemoveTimerRunFlag) { // If this is the first endpoint that must be deleted in the control endpoint list of the HC
    USBH_OHCI_EndpointListEnable(pDev, pEP->EndpointType, FALSE, FALSE);
    pDev->BulkEpRemoveTimerRunFlag    = TRUE;
    pDev->BulkEpRemoveTimerCancelFlag = FALSE;
    USBH_StartTimer(pDev->hBulkEpRemoveTimer, OH_STOP_DELAY_TIME);
  }
}

/*********************************************************************
*
*       USBH_OHCI_BULK_INT_AddUrb
*
*  Function description
*    Adds an bulk or interrupt endpoint request
*/
USBH_STATUS USBH_OHCI_BULK_INT_AddUrb(USBH_OHCI_BULK_INT_EP * pEP, USBH_URB * pUrb) {
  USBH_STATUS Status;

  OH_BULKINT_VALID(pEP);
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: INFO USBH_OHCI_BULK_INT_AddUrb pEP: 0x%x!", pEP->EndpointAddress));
  USBH_OCHI_IS_DEV_VALID(pEP->pDev);
  USBH_ASSERT(pUrb != NULL);
  if (pEP->State != OH_EP_LINK) {
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI:  USBH_OHCI_BULK_INT_AddUrb: pEP 0x%x not linked!", pEP->EndpointAddress));
    pUrb->Header.Status = USBH_STATUS_ENDPOINT_HALTED;
    return USBH_STATUS_ENDPOINT_HALTED;
  }
  if ( pEP->HaltFlag) {
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI:  USBH_OHCI_BULK_INT_AddUrb: pEP in 0x%x halted!", pEP->EndpointAddress));
    return USBH_STATUS_ENDPOINT_HALTED;
  }
  if (pEP->pPendingUrb == NULL && pEP->AbortMask==0) {
    pEP->UrbCount++;
    if (!USBH_DLIST_IsEmpty(&pEP->UrbList)) {
      USBH_DLIST_InsertTail(&pEP->UrbList,&pUrb->Header.ListEntry);
      Status = USBH_STATUS_PENDING;
      USBH_OHCI_BULK_INT_SubmitUrbsFromList(pEP);       // Submit next USBH_URB from list
    } else {
      Status=_BULK_INT_SubmitUrb(pEP,pUrb);
      if ( Status != USBH_STATUS_PENDING ) { // On error
        USBH_WARN((USBH_MTYPE_OHCI, "OHCI:  USBH_OHCI_BULK_INT_AddUrb: USBH_OHCI_Ep0SubmitUrb: %08x!", Status));
        pEP->UrbCount--;
      }
      return Status;
    }
  } else {                                   // USBH_URB is pending, add it to the USBH_URB list and return pending
    Status = USBH_STATUS_PENDING;
    pEP->UrbCount++;
    USBH_DLIST_InsertTail(&pEP->UrbList,&pUrb->Header.ListEntry);
  }
  return Status;
}

/*********************************************************************
*
*       USBH_OHCI_BULK_INT_UpdateTDLengthStatus
*
*  Function description
*    Called during the first enumeration of the DONE TD list.
*    Updates the EP length and status field.
*    Works without any order of completed TDs.
*/
void USBH_OHCI_BULK_INT_UpdateTDLengthStatus(USBH_OHCI_INFO_GENERAL_TRANS_DESC * pGtd) {
  USBH_OHCI_BULK_INT_EP * ep;
  USBH_STATUS       tdStatus;
  U32               length;
  USBH_BOOL            shortPkt;
  USBH_URB             * urb;
  ep              = (USBH_OHCI_BULK_INT_EP*)pGtd->pEp;
  OH_BULKINT_VALID(ep);
  USBH_ASSERT(ep->pPendingUrb != NULL);
  ep->UrbDoneTdNumber++;                            // Total TDs of the USBH_URB
  ep->UpDownTDCounter++;                            // Help count: Number of TDs found in the current DONE list
  urb = ep->pPendingUrb;
  // Get TD status and length, shortPkt not used because on bulk and interrupt endpoints
  // only on the last TD the buffer rounding bit is set, on all other TD the ED goes in HALT!
  tdStatus = USBH_OHCI_TdGetStatusAndLength(pGtd, &length, &shortPkt);
  if ( tdStatus != USBH_STATUS_SUCCESS) {           // Save the last error
    urb->Header.Status = tdStatus;
#if (USBH_DEBUG > 1)
    if (tdStatus != USBH_STATUS_DATA_UNDERRUN) {
      USBH_LOG((USBH_MTYPE_OHCI, "OHCI: OHCI bulk int transfer error condition code: %u ep: 0x%x!", tdStatus, (int)ep->EndpointAddress));
    }
#endif
  }
  if (shortPkt) {
    ep->Flags |= OH_SHORT_PKT_EPFLAG;
  }
  if (NULL ==ep->pCopyBuffer) {                      // USBH_URB buffer is used
    urb->Request.BulkIntRequest.Length+=length;
  }  else if( length ) {                            // Bytes transferred, update buffe and pointer
    if ( ep->EndpointAddress & USB_IN_DIRECTION ) { // It is needed to update the copy buffer before it read
      USBH_OHCI_CopyToUrbBufferUpdateTransferBuffer(ep->pCopyBuffer, length);
    } else {
      USBH_OHCI_UpdateCopyTransferBuffer           (ep->pCopyBuffer, length);
    }
  }
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: INFO USBH_OHCI_BULK_INT_UpdateTDLengthStatus pEp 0x%x transferred: %u !", (int)ep->EndpointAddress, length));
}

/*********************************************************************
*
*       USBH_OHCI_BULK_INT_CheckForCompletion
*
*  Function description
*    USBH_OHCI_BULK_INT_CheckForCompletion is called during the second enumeration
*    of the DONE TD list. If the pipe is halted (on transfer error) all
*    other pending TDs are removed and the USBH_URB is completed. If all TDs
*    are done the USBH_URB is also completed. Then the next one is submitted!
*/
void USBH_OHCI_BULK_INT_CheckForCompletion(USBH_OHCI_INFO_GENERAL_TRANS_DESC * Gtd) {
  USBH_OHCI_BULK_INT_EP * ep;
  USBH_URB             * urb;
  USBH_STATUS       urb_status;
  int               complete;
  USBH_STATUS       status = USBH_STATUS_SUCCESS;
  ep = (USBH_OHCI_BULK_INT_EP*)Gtd->pEp;
  OH_BULKINT_VALID(ep);
  USBH_ASSERT_PTR(ep->pPendingUrb);
  // Delete TD
  USBH_OHCI_EpGlobDeleteDoneTD(&Gtd->ItemHeader,&ep->TdCounter);
  ep->UpDownTDCounter--;
  if (ep->UpDownTDCounter) {
    return;
  }
  urb        = ep->pPendingUrb;
  urb_status = urb->Header.Status;
  complete   = FALSE;
  if (NULL == ep->pCopyBuffer) {
    if (ep->TdCounter == 1 || USBH_STATUS_PENDING != urb_status) {
      complete = TRUE;
    }
  } else if ((ep->TdCounter == 1 && 0 == ep->pCopyBuffer->RemainingLength) || USBH_STATUS_PENDING != urb_status || (ep->Flags & OH_SHORT_PKT_EPFLAG)) {
    complete = TRUE;
  }
  if (!complete) {
    if (ep->AbortMask & EP_ABORT_SKIP_TIME_OVER_MASK) {          // Endpoint is aborted and the host list processing is stopped
      USBH_ASSERT((urb->Header.HcFlags & URB_CANCEL_PENDING_MASK)); // For testing
      if ( urb->Header.HcFlags & URB_CANCEL_PENDING_MASK ) {     // USBH_URB must be canceled
        complete = TRUE;
      }
    }
  }
  {
    U32   length;
    U8  * buffer;

    if (!complete) {
      if (NULL != ep->pCopyBuffer) {
        // Copy new OUT buffer / Get buffer and length / Submit the next packet and return on success!
        if (!(ep->EndpointAddress & USB_IN_DIRECTION)) {
          USBH_OHCI_FillCopyTransferBuffer(ep->pCopyBuffer);
        }
        buffer = USBH_OHCI_GetBufferLengthFromCopyTransferBuffer(ep->pCopyBuffer, &length);
        if (length) {
          status = _BULK_INT_SubmitTransfer(ep, buffer, length);
          if (status) {
            USBH_WARN((USBH_MTYPE_OHCI, "OHCI: USBH_OHCI_BULK_INT_CheckForCompletion:_BULK_INT_SubmitTransfer!"));
          }
        }
        if ( !status ) { // On success
          return;
        }
      }
    }
  }
  // USBH_URB complete (If all TDs transferred or not all and the HC has stopped the endpoint because of an error or abort)
  // Update USBH_URB buffer and length
  if (NULL != ep->pCopyBuffer) {
    if (status) {
      urb_status = status;
    }
    urb->Request.BulkIntRequest.Length = ep->pCopyBuffer->Transferred;
  }
#if (USBH_DEBUG > 1) // Infos
  if (ep->TdCounter != 1) {
    USBH_LOG((USBH_MTYPE_OHCI, "OHCI: INFO USBH_OHCI_BULK_INT_CheckForCompletion: Complete USBH_URB: active TDS!"));
  }
  if (USBH_OHCI_EpGlobIsSkipped(&ep->ItemHeader)) {
    USBH_LOG((USBH_MTYPE_OHCI, "OHCI: INFO USBH_OHCI_BULK_INT_CheckForCompletion: Complete USBH_URB: ED is skipped!"));
  }
  if(USBH_OHCI_EpGlobIsHalt(&ep->ItemHeader)) {
    USBH_LOG((USBH_MTYPE_OHCI, "OHCI: INFO USBH_OHCI_BULK_INT_CheckForCompletion: Complete USBH_URB: ED halted!"));
  }
  // USBH_URB complete or aborted
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: INFO DONE: BulkInt complete or aborted:pEP: 0x%x transferred: %u Status %s:",
                              (unsigned int)ep->EndpointAddress, urb->Request.BulkIntRequest.Length, USBH_GetStatusStr(urb_status)));
#endif

  if (ep->TdCounter > 1) {
   USBH_OHCI_EpGlobDeleteAllPendingTD(&ep->ItemHeader,&ep->TdCounter); // Delete all not processed TDs
  }
  if (urb_status == USBH_STATUS_PENDING) {
    urb_status = USBH_STATUS_SUCCESS;
  } else if (urb_status == USBH_STATUS_DATA_UNDERRUN) {
    if (USBH_OHCI_EpGlobIsHalt(&ep->ItemHeader)) {
      USBH_OHCI_EpGlobClearHalt(&ep->ItemHeader);
    }
    urb_status = USBH_STATUS_SUCCESS;
  }
  if (USBH_OHCI_EpGlobIsHalt(&ep->ItemHeader)) {
    ep->HaltFlag=TRUE; // Always halt set flag
  }
  if (ep->AbortMask) {
                       // Endpoint is aborted: Complete pending USBH_URB / Clear the skip bit / Complete aborted URBs resubmit new added URBS
    USBH_OHCI_BULK_INT_CheckAndCancelAbortedUrbs(ep, TRUE);
  } else {             // TRUE flag: submit an new USBH_URB before the old one is completed
    _BULK_INT_OnCompleteUrb(ep, ep->pPendingUrb, urb_status, TRUE);
  }
}

/******************************* EOF ********************************/
