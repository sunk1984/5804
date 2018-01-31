/*********************************************************************
*  
*   IAR PowerPac
*
*   (c) Copyright IAR Systems 2010.  All rights reserved.
*
**********************************************************************
----------------------------------------------------------------------
File        : USBH_OHC_EP_Iso.c
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
*       _ISO_CompleteUrb
*
*  Function description
*    Completes the current ISO USBH_URB. Is called if all TDs of
*    an USBH_URB are done or the USBH_URB is aborted
*
*  Parameters:
*    pUrb: Condition code of the transfer or an driver Status
*/
static void _ISO_CompleteUrb(USBH_OHCI_ISO_EP * pIsoEP, USBH_URB * pUrb, USBH_STATUS Status, int NewUrbFlag) {
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: _ISO_CompleteUrb pIsoEP 0x%x Length: %lu!", (int)pIsoEP->EndpointAddress, pUrb->Request.IsoRequest.Length));
  if (pUrb != NULL) {
    pUrb->Header.Status = Status;
    if (pUrb == pIsoEP->pPendingUrb) {         // Current completed USBH_URB
#if (USBH_DEBUG > 1)
      if (pIsoEP->TdCounter > 1) {           // More than the reserved TD is active
        USBH_LOG((USBH_MTYPE_OHCI, "OHCI: INFO _ISO_CompleteUrb: Pending TDs!!!"));
      }
#endif
      if (NULL != pIsoEP->pCopyBuffer) {      // Release the copy pBuffer resource
        pUrb->Request.IsoRequest.Length = pIsoEP->pCopyBuffer->Transferred;
        USBH_HCM_PutItem(&pIsoEP->pCopyBuffer->ItemHeader);
        pIsoEP->pCopyBuffer = NULL;
      }
      pIsoEP->pPendingUrb = NULL;             // Delete the current pending USBH_URB
    }
    USBH_ASSERT(pIsoEP->UrbCount);
    pIsoEP->UrbCount--;
    if (NewUrbFlag) {
      USBH_OHCI_ISO_SubmitUrbsFromList(pIsoEP);   // Submit next packet before the previous one is completed. All neded previous transfer resources are released!
    }
    pUrb->Header.pfOnInternalCompletion(pUrb); // Call the USB Bus driver completion routine
  }
}

/*********************************************************************
*
*       _ISO_CompletePendingUrbs
*
*  Function description
*
*  Parameters:
*    HcFlagMask: URBS where one of the flag mask bit is set where completed (mask bits are or'ed)
*                0: all URBs are completed.
*/
static void _ISO_CompletePendingUrbs(USBH_OHCI_ISO_EP * pIsoEp, U32 HcFlagMask) {
  USBH_DLIST  * pListHead;
  USBH_DLIST  * pEntry;
  USBH_DLIST  * pAbortEntry;
  USBH_DLIST    AbortList;
  USBH_URB    * pUrb;
  OH_ISO_VALID(pIsoEp);
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: _ISO_CompletePendingUrbs!"));
  USBH_DLIST_Init(&AbortList);
  pListHead = &pIsoEp->UrbList;
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
    pUrb->Request.IsoRequest.Length = 0;                     // Set Length to zero
    _ISO_CompleteUrb(pIsoEp, pUrb, USBH_STATUS_CANCELED, FALSE);
  }
}

/*********************************************************************
*
*       _ISO_Unlink
*
*  Function description
*    Before this function is called the HC list must be disabled! The
*    endpoint is removed from the O-ist ISO list and from the device
*    object ISO list. If the function returns the endpoint is still valid!
*/
static void _ISO_Unlink(USBH_OHCI_ISO_EP * pIsoEp) {
  USBH_OHCI_DEVICE  * pDev;
  USBH_DLIST        * pPreviousEntry;
  USBH_OHCI_ISO_EP  * pPreviousIsoEP;
  U32                 NextPhyAddr;

  OH_ISO_VALID(pIsoEp);
  pDev             = pIsoEp->pDev;
  USBH_OCHI_IS_DEV_VALID(pDev);
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: _ISO_Unlink: pIsoEp: 0x%x!",pIsoEp->EndpointAddress));
  USBH_ASSERT( NULL == pIsoEp->pPendingUrb);
  USBH_ASSERT( 0==pIsoEp->UrbCount);
  USBH_ASSERT( pDev->IsoEpCount);
  USBH_ASSERT( pIsoEp->State != OH_EP_IDLE);
  // HC list must be disabled
  USBH_ASSERT( 0==OhHalTestReg(pDev->pRegBase, OH_REG_CONTROL, HC_CONTROL_IE));
  USBH_ASSERT(!USBH_DLIST_IsEmpty(&pDev->IsoEpList));
  pPreviousEntry = USBH_DLIST_GetPrev(&pIsoEp->ListEntry); // Get the list tail
  if ( pPreviousEntry == &pDev->IsoEpList ) {
    USBH_LOG((USBH_MTYPE_OHCI, "OHCI: INFO _ISO_Unlink: remove the first ED in the  ED list!"));
    NextPhyAddr = USBH_OHCI_EpGlobUnlinkEd(NULL, &pIsoEp->ItemHeader);
    OhHalWriteReg(pDev->pRegBase, OH_REG_BULKHEADED, NextPhyAddr);
  } else {
    pPreviousIsoEP = GET_ISO_EP_FROM_ENTRY(pPreviousEntry);
    OH_ISO_VALID(pPreviousIsoEP);
    USBH_OHCI_EpGlobUnlinkEd(&pPreviousIsoEP->ItemHeader, &pIsoEp->ItemHeader);
  }
  USBH_DLIST_RemoveEntry(&pIsoEp->ListEntry);
  pIsoEp->State = OH_EP_IDLE;
  if (0==pDev->IsoEpCount) {
    USBH_LOG((USBH_MTYPE_OHCI, "OHCI: INFO _ISO_Unlink: ED list now empty!"));
  }
  USBH_ASSERT(pDev->IsoEpCount);
  pDev->IsoEpCount--;
}

/*********************************************************************
*
*       _ISO_SubmitTransfer
*
*  Function description
*/
static USBH_STATUS _ISO_SubmitTransfer(USBH_OHCI_ISO_EP * pIsoEp, USBH_ISO_REQUEST * pIsoRequest) {
  USBH_STATUS                         Status;
  USBH_OHCI_INFO_GENERAL_TRANS_DESC * pTransDesc;
  U16                                 StartFrame;
  unsigned                            i;
  U16                                 Frame;

  USBH_OCHI_IS_DEV_VALID(pIsoEp->pDev);
  USBH_ASSERT(0 == pIsoEp->AbortMask);
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: _ISO_SubmitTransfer pIsoEp: 0x%x Length: %lu!",(int)pIsoEp->EndpointAddress, pIsoRequest->Length));
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: Start transfer: Length: %lu ",pIsoRequest->Length ));
  StartFrame = pIsoEp->pDev->pOhHcca->FrameNumber;
  /* delay a few frames before the first TD */
  StartFrame += USBH_MAX(8, pIsoEp->IntervalTime);
  StartFrame &= ~(pIsoEp->IntervalTime - 1);
  pIsoRequest->StartFrame = StartFrame;
  pIsoEp->UrbTotalTdNumber = 0;
  pIsoEp->UrbDoneTdNumber  = 0;
  pIsoEp->UpDownTDCounter  = 0;
  pIsoEp->Flags           &= ~OH_SHORT_PKT_EPFLAG;
  // Build the TDs
  //   Get the MDL list (page address table)
  //   Init the dummy TD until the address table is submitted
  //   Allocate a new TD
  //   Link the new TD, set TailP from ED
  Status = USBH_STATUS_SUCCESS;
  Frame = StartFrame;
  for (i = 0; i < pIsoRequest->NumPackets; i++) {

    Frame += pIsoEp->IntervalTime;
    Frame &= 0xffff;

    // Get the Tail TD
    pTransDesc = USBH_OHCI_EpGlobGetLastTDFromED(&pIsoEp->ItemHeader);
    if (pTransDesc == NULL) {
      USBH_WARN((USBH_MTYPE_OHCI, "OHCI: FATAL _ISO_SubmitTransfer: USBH_OHCI_EpGlobGetLastTDFromED!"));
      Status = USBH_STATUS_ERROR;
      break;
    }
    // Write all transfer descriptors
    // Setup all needed TDs if the data Length unequal zero!
    // Set the pBuffer rounding bit only on the last page on IN token!
    // If a zero Length IN packet is received no on the last page a USB data underrun
    // is generated form the HC and the DONE routine removes all other TDs and restart this endpoint!
    USBH_LOG((USBH_MTYPE_OHCI, "OHCI: USBH_OHCI_TdInit: pIsoEP: 0x%x ",pIsoEp->EndpointAddress)); // Init the TD
    if (i == (pIsoRequest->NumPackets -1)) {
      USBH_OHCI_IsoTdInit(pTransDesc, pIsoEp, pIsoRequest, (6 << 21) | OHCI_TD_CC | Frame, (U32)pIsoRequest->pBuffer + pIsoRequest->aIsoPacket[i].Offset, pIsoRequest->aIsoPacket[i].Length, i);     // Set the previous TD and allocate and insert an new one
    } else {
      USBH_OHCI_IsoTdInit(pTransDesc, pIsoEp, pIsoRequest, OHCI_TD_DI | OHCI_TD_CC | Frame, (U32)pIsoRequest->pBuffer + pIsoRequest->aIsoPacket[i].Offset, pIsoRequest->aIsoPacket[i].Length, i);     // Set the previous TD and allocate and insert an new one
    }
    pTransDesc = USBH_OHCI_GetTransDesc(&pIsoEp->pDev->IsoTDPool);                                                // Get an new TD
    if (pTransDesc == NULL) {
      USBH_WARN((USBH_MTYPE_OHCI, "OHCI: FATAL _ISO_SubmitTransfer: USBH_OHCI_GetTransDesc!"));
      Status = USBH_STATUS_MEMORY;
      break;
    }
    // Start the TD, set the EP Tail pointer
    pIsoEp->UrbTotalTdNumber++;
    USBH_OHCI_EpGlobInsertTD(&pIsoEp->ItemHeader, &pTransDesc->ItemHeader, &pIsoEp->TdCounter);
  }
  // As last restart the endpoint list
  if (!pIsoEp->pDev->IsoEpRemoveTimerRunFlag) {
    // No other endpoint is removed
    USBH_OHCI_EndpointListEnable(pIsoEp->pDev, USB_EP_TYPE_ISO, TRUE, TRUE);
  }
  return Status;

}

/*********************************************************************
*
*       _ISO_SubmitUrb
*
*  Function description
*    Submits an ISO USBH_URB. One USBH_URB is submitted at the time. More than
*    one transfer descriptor can be submitted.
*
*  Returns:
*    On success: USBH_STATUS_PENDING:
*    On error:   Other values
*/
static USBH_STATUS _ISO_SubmitUrb(USBH_OHCI_ISO_EP * pIsoEP, USBH_URB * pUrb) {
  USBH_STATUS             Status;
  USBH_ISO_REQUEST      * pIsoRequest;
  USBH_OHCI_DEVICE      * pDev;

  OH_ISO_VALID(pIsoEP);
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: _ISO_SubmitUrb: pIsoEP: 0x%x!",pIsoEP->EndpointAddress));
  // The PendingURB is submitted
  USBH_ASSERT(pUrb != NULL);
  USBH_ASSERT(pIsoEP->pPendingUrb == NULL);
  USBH_ASSERT(0 == (pUrb->Header.HcFlags & URB_CANCEL_PENDING_MASK) );
  // Assert only the default TD is in the ED TD list
  USBH_ASSERT(pIsoEP->TdCounter == 1 );
  USBH_ASSERT(!USBH_OHCI_EpGlobIsHalt(&pIsoEP->ItemHeader) );
  pDev = pIsoEP->pDev;
  USBH_OCHI_IS_DEV_VALID(pDev);
  pIsoRequest = &pUrb->Request.IsoRequest;
//  pIsoRequest->Length = 0;                                                    // Clear USBH_URB Length and submit pBuffer
  Status  = _ISO_SubmitTransfer(pIsoEP, pIsoRequest);
  if (Status) {
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: _ISO_SubmitUrb!"));
    if (NULL != pIsoEP->pCopyBuffer) {
      USBH_HCM_PutItem(&pIsoEP->pCopyBuffer->ItemHeader);
      pIsoEP->pCopyBuffer = NULL;
    }
  } else { // On success set pending USBH_URB
    pIsoEP->pPendingUrb = pUrb;
    Status         = USBH_STATUS_PENDING;
  }
  return Status;
}

/*********************************************************************
*
*       _ISO_MarkUrbsWithCancelPendingFlag
*
*  Function description
*/
static void _ISO_MarkUrbsWithCancelPendingFlag(USBH_OHCI_ISO_EP * pIsoEP) {
  USBH_DLIST  * pListHead;
  USBH_DLIST  * pEntry;
  USBH_URB    * pUrb;

  OH_ISO_VALID(pIsoEP);
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: _ISO_MarkUrbsWithCancelPendingFlag!"));
  pListHead = &pIsoEP->UrbList;
  pEntry    = USBH_DLIST_GetNext(pListHead);
  while (pEntry != pListHead) {
    pUrb = (USBH_URB *)GET_URB_HEADER_FROM_ENTRY(pEntry); // The USBH_URB header is the first element in the USBH_URB, cast it
#if (USBH_DEBUG > 1)
    if (pUrb->Header.HcFlags & URB_CANCEL_PENDING_MASK) {
      USBH_WARN((USBH_MTYPE_OHCI, "OHCI: _ISO_MarkUrbsWithCancelPendingFlag: URB_CANCEL_PENDING_MASK already set!"));
    }
#endif
    pUrb->Header.HcFlags |= URB_CANCEL_PENDING_MASK;
    pEntry = USBH_DLIST_GetNext(pEntry);
  }
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
static void _RemoveUserEDFromPhysicalLink(USBH_OHCI_DEVICE * pDev, USBH_OHCI_ISO_EP * pIsoEp) {
  USBH_OHCI_ISO_EP       * pPreviousEp;
  USBH_OHCI_DUMMY_INT_EP * pDummyEp;
  USBH_DLIST             * pPreviousEntry;

  USBH_ASSERT(pDev->IsoEpCount);
  USBH_ASSERT(pIsoEp->State != OH_EP_IDLE);
  pDummyEp = pIsoEp->pDummyIntEp;
  USBH_ASSERT_MAGIC(&pDummyEp->ItemHeader, USBH_HCM_ITEM_HEADER);
  USBH_ASSERT(!USBH_DLIST_IsEmpty(&pDummyEp->ActiveList));
  USBH_USE_PARA(pDev);
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: INFO _RemoveUserEDFromPhysicalLink: remove pIsoEp 0x%x pDev: %d!", (int)pIsoEp->EndpointAddress, (int)pIsoEp->DeviceAddress));
  // 1. Mark the endpoint as removed with OH_EP_UNLINK
  // 2. Skip ED from list processing
  // 3. Remove from the physical list: set the next pointer for the previous ED to the next pointer of endpoint that must be deleted,
  //    do not remove the endpoint for the logical dummy endpoint active list, because a logical remove list does not exist!
  USBH_OHCI_EpGlobSetSkip(&pIsoEp->ItemHeader);             // Skip endpoint from list processing
  pPreviousEntry = USBH_DLIST_GetPrev(&pIsoEp->ListEntry);
  if (pPreviousEntry == &pDummyEp->ActiveList) {  // Previous link is a dummy EP
    USBH_OHCI_EpGlobUnlinkEd(&pDummyEp->ItemHeader, &pIsoEp->ItemHeader);
  } else {                                      // Previous EP is not an dummy EP
    pPreviousEp = GET_ISO_EP_FROM_ENTRY(pPreviousEntry);
    OH_ISO_VALID(pPreviousEp);
    USBH_OHCI_EpGlobUnlinkEd(&pPreviousEp->ItemHeader, &pIsoEp->ItemHeader);
  }
#if (USBH_DEBUG > 1)
  if (pDev->IntRemoveFlag) { // IntRemoveFlag only for debug
    USBH_LOG((USBH_MTYPE_OHCI, "INFO _RemoveUserEDFromPhysicalLink: More as one endpoint deleted within one frame time!"));
  }
  pDev->IsoRemoveFlag = TRUE;
#endif
}


/*********************************************************************
*
*       Global functions
*
**********************************************************************
*/

/*********************************************************************
*
*       USBH_OHCI_ISO_SubmitUrbsFromList
*
*  Function description
*    Submit the next USBH_URB from an ISO endpoint
*/
void USBH_OHCI_ISO_SubmitUrbsFromList(USBH_OHCI_ISO_EP * pIsoEp) {
  USBH_URB    * pUrb;
  USBH_STATUS   Status;
  USBH_DLIST  * pEntry;
  int           EmptyFlag;

  OH_ISO_VALID(pIsoEp);
  if (pIsoEp->State     != OH_EP_LINK) {
    USBH_LOG((USBH_MTYPE_OHCI, "OHCI: USBH_OHCI_ISO_SubmitUrbsFromList: EP 0x%x is unlinked!", (unsigned int)pIsoEp->EndpointAddress));
    return;
  }
  if (pIsoEp->AbortMask != 0){
    USBH_LOG((USBH_MTYPE_OHCI, "OHCI: USBH_OHCI_ISO_SubmitUrbsFromList: pUrb on EP 0x%x is aborted!", (unsigned int)pIsoEp->EndpointAddress));
    return;
  }
  if (USBH_OHCI_EpGlobIsHalt(&pIsoEp->ItemHeader)) {
    USBH_LOG((USBH_MTYPE_OHCI, "OHCI: USBH_OHCI_ISO_SubmitUrbsFromList: EP 0x%x is halted!", (unsigned int)pIsoEp->EndpointAddress));
    return;
  }
  if (NULL          != pIsoEp->pPendingUrb) {
    return;
  }
  EmptyFlag = USBH_DLIST_IsEmpty(&pIsoEp->UrbList);
  while(!EmptyFlag) { // Submit the next USBH_URB
    USBH_DLIST_RemoveHead(&pIsoEp->UrbList, &pEntry);
    EmptyFlag = USBH_DLIST_IsEmpty(&pIsoEp->UrbList);
    pUrb       = (USBH_URB*)GET_URB_HEADER_FROM_ENTRY(pEntry);
    Status    = _ISO_SubmitUrb(pIsoEp, pUrb);
    if (Status == USBH_STATUS_PENDING) { // On success stop
      break;
    } else { // pUrb can not be submitted
      if (pIsoEp->UrbCount==1) {
        _ISO_CompleteUrb(pIsoEp, pIsoEp->pPendingUrb, Status, FALSE); // Endpoint can be invalid
        break;
      } else {
        _ISO_CompleteUrb(pIsoEp, pIsoEp->pPendingUrb, Status, FALSE);
      }
    }
  }
}

/*********************************************************************
*
*       USBH_OHCI_ISO_CheckAndCancelAbortedUrbs
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
USBH_STATUS USBH_OHCI_ISO_CheckAndCancelAbortedUrbs(USBH_OHCI_ISO_EP * pIsoEP, int TDDoneFlag) {
  USBH_OHCI_INFO_GENERAL_TRANS_DESC * pGlobalTransDesc;
  U32                                 Length;
  U32                                 TDCounter;
  USBH_BOOL                           TDInDoneCacheQueueFlag = FALSE;
  USBH_BOOL                           ShortPkt               = FALSE;
  USBH_BOOL                           CompleteFlag           = FALSE;

  USBH_ASSERT(pIsoEP->AbortMask);
  if (NULL != pIsoEP->pPendingUrb) {                                                  // An URB is pending
    if ( !TDDoneFlag ) {                                                              // Called from the DONE routine
      TDCounter = USBH_OHCI_EpGlobGetTdCount(&pIsoEP->ItemHeader, &pIsoEP->pDev->IsoTDPool);
      USBH_ASSERT( TDCounter >= 1 );                                                  // Compare sum of TDs
      if ((TDCounter-1) + pIsoEP->UrbDoneTdNumber != pIsoEP->UrbTotalTdNumber) {      // TD lost, -1 do not use the dummy TD, waits for DONE interrupt!
        USBH_LOG((USBH_MTYPE_OHCI, "OHCI: INFO USBH_OHCI_ISO_CheckAndCancelAbortedUrbs: lost TD, wait for DONE!"));
        TDInDoneCacheQueueFlag = TRUE;
      } else {                                                                        // No TD lost, IsCompleted pending USBH_URB with the Length of the last pending TD
        pGlobalTransDesc = USBH_OHCI_EpGlobGetFirstTDFromED(&pIsoEP->ItemHeader);
        USBH_OHCI_TdGetStatusAndLength(pGlobalTransDesc, &Length, &ShortPkt);         // Only the Length is checked of the returned values!
        pIsoEP->pPendingUrb->Request.IsoRequest.Length += Length;                     // Update the Length of the canceled USBH_URB by the last active TD
        USBH_OHCI_EpGlobDeleteAllPendingTD(&pIsoEP->ItemHeader, &pIsoEP->TdCounter);
        CompleteFlag = TRUE;
      }
    } else {
      CompleteFlag = TRUE;
    }
    if (CompleteFlag) {
      USBH_OHCI_EpGlobClearSkip(&pIsoEP->ItemHeader);
      pIsoEP->AbortMask = 0;
      _ISO_CompleteUrb(pIsoEP, pIsoEP->pPendingUrb, USBH_STATUS_CANCELED, FALSE);
    }
  } else { // No pending USBH_URB
    pIsoEP->AbortMask = 0;
    USBH_OHCI_EpGlobClearSkip(&pIsoEP->ItemHeader);
  }
  if (!TDInDoneCacheQueueFlag) {
    _ISO_CompletePendingUrbs(pIsoEP, URB_CANCEL_PENDING_MASK);
    USBH_OHCI_ISO_SubmitUrbsFromList(pIsoEP); // Submit the next canceled USBH_URB if available
    return USBH_STATUS_SUCCESS;
  } else {                                    // Pending TD in Host cache
    return USBH_STATUS_ERROR;
  }
}

/*********************************************************************
*
*       USBH_OHCI_ISO_RemoveEps
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
void USBH_OHCI_ISO_RemoveEps(USBH_OHCI_DEVICE * pDev, USBH_BOOL AllEndpointFlag) {
  USBH_OHCI_ISO_EP                * pEP;
  USBH_DLIST                      * pEntry;
  USBH_DLIST                        RemoveList;
  USBH_RELEASE_EP_COMPLETION_FUNC * pfCompletion;
  void                            * pContext;

  USBH_OCHI_IS_DEV_VALID(pDev);
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: USBH_OHCI_ISO_RemoveEps!"));
#if (USBH_DEBUG > 1)
  if ( OhHalTestReg(pDev->pRegBase, OH_REG_CONTROL, HC_CONTROL_BLE)) {
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: FATAL USBH_OHCI_ISO_RemoveEps: ISO list not disabled!"));
  }
#endif
  // Remove needed endpoints from the list
  USBH_DLIST_Init(&RemoveList);
  pEntry = USBH_DLIST_GetPrev(&pDev->IsoEpList);
  while (pEntry != &pDev->IsoEpList)  {
    USBH_ASSERT(pDev->IsoEpCount);
    pEP    = GET_ISO_EP_FROM_ENTRY(pEntry);
    OH_ISO_VALID(pEP);
    pEntry = USBH_DLIST_GetPrev(pEntry);
    if (AllEndpointFlag) {
      _ISO_Unlink(pEP);
      USBH_DLIST_InsertHead(&RemoveList, &pEP->ListEntry);
    } else {
      if ( pEP->State == OH_EP_UNLINK ) { // Remove only endpoints where the OH_EP_UNLINK flag is set
        _ISO_Unlink(pEP);
        USBH_DLIST_InsertHead(&RemoveList, &pEP->ListEntry);
      }
    }
  }
  // After unlinking the endpoint restart the HC list processing
  if (pDev->IsoEpCount > 1) { // On active endpoints
    USBH_OHCI_EndpointListEnable(pDev, USB_EP_TYPE_ISO, TRUE, TRUE);
  }
#if (USBH_DEBUG > 1)
  if(pDev->IsoEpCount == 0) {
    USBH_LOG((USBH_MTYPE_OHCI, "OHCI: INFO USBH_OHCI_ISO_RemoveEps: all ISO endpoints removed!"));
  }
#endif
  // Call the release completion functions of all removed endpoints
  while (!USBH_DLIST_IsEmpty(&RemoveList)) {
    USBH_DLIST_RemoveHead(&RemoveList, &pEntry);
    pEP = GET_ISO_EP_FROM_ENTRY(pEntry);
    OH_ISO_VALID(pEP);
    pfCompletion = pEP->pfOnReleaseCompletion;
    pContext     = pEP->pReleaseContext;
    USBH_OHCI_ISO_PutEp(pEP);
    if(pfCompletion) {  // Call the completion routine, attention: runs in this context
      pfCompletion((void *)pContext);
    }
  }
}

/*********************************************************************
*
*       USBH_OHCI_ISO_OnRemoveEpTimer
*
*  Function description
*    Called if one or more ISO endpoints has been removed
*/
void USBH_OHCI_ISO_OnRemoveEpTimer(void * pContext) {
  USBH_OHCI_DEVICE * pDev;

  pDev       = (USBH_OHCI_DEVICE *)pContext;
  USBH_OCHI_IS_DEV_VALID(pDev);
  if (pDev->IsoEpRemoveTimerCancelFlag) {
    pDev->IsoEpRemoveTimerCancelFlag = FALSE;
    return;
  }
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: USBH_OHCI_ISO_OnRemoveEpTimer!"));
  pDev->IsoEpRemoveTimerRunFlag = FALSE;                              // Timer is stopped
  USBH_OHCI_ISO_RemoveEps(pDev, FALSE);
}

/*********************************************************************
*
*       USBH_OHCI_ISO_OnAbortUrbTimer
*
*  Function description
*    The timer routine runs if an ISO endpoint is aborted and an USBH_URB
*    of the endpoint at the time where the endpoint is aborted is submitted
*    and pending. The endpoint is always skipped if the endpoint is aborted.
*    This routine calls the abort request completion routine!
*    Cancel all URBs on skipped EDs, this function is always called after a
*    timeout of more than one frame after the ED SKIP bit is set
*
*  Parameters
*    pContext: pDev Ptr.
*/
void USBH_OHCI_ISO_OnAbortUrbTimer(void * pContext) {
  USBH_OHCI_DEVICE  * pDev;
  USBH_BOOL           RestartFlag;
  USBH_DLIST        * pEntry;
  USBH_OHCI_ISO_EP  * pEP;
  USBH_STATUS         Status;
  USBH_BOOL           StartTimer = FALSE;

  USBH_ASSERT(pContext != NULL);
  pDev = (USBH_OHCI_DEVICE *)pContext;
  USBH_OCHI_IS_DEV_VALID(pDev);
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: USBH_OHCI_ISO_OnAbortUrbTimer!"));

  if (pDev->IsoEpAbortTimerCancelFlag) {
    pDev->IsoEpAbortTimerCancelFlag = FALSE;
    return;
  }
  pDev->IsoEpAbortTimerRunFlag = FALSE;

  // Mark all aborted endpoints with EP_ABORT_SKIP_TIME_OVER_MASK mask
  pEntry=USBH_DLIST_GetNext(&pDev->IsoEpList);
  while (pEntry != &pDev->IsoEpList) {
    pEP = GET_ISO_EP_FROM_ENTRY(pEntry);
    OH_ISO_VALID(pEP);
    if (pEP->AbortMask & EP_ABORT_MASK) {
      pEP->AbortMask |= EP_ABORT_SKIP_TIME_OVER_MASK | EP_ABORT_PROCESS_FLAG_MASK;
    } else if (pEP->AbortMask & EP_ABORT_START_TIMER_MASK) { // Start timer again for aborted endpoints where the timer already runs
      StartTimer = TRUE;
    }
    pEntry = USBH_DLIST_GetNext(pEntry);
  }
  if (StartTimer) {
    pDev->IsoEpAbortTimerRunFlag = TRUE;
  }
  RestartFlag = TRUE;
  while(RestartFlag) {
    RestartFlag = FALSE;
    pEntry        = USBH_DLIST_GetNext(&pDev->IsoEpList);
    while (pEntry != &pDev->IsoEpList) {
      pEP    = GET_ISO_EP_FROM_ENTRY(pEntry);
      pEntry = USBH_DLIST_GetNext(pEntry);
      OH_ISO_VALID(pEP);
      if ((pEP->AbortMask & EP_ABORT_PROCESS_FLAG_MASK)) { // Endpoint aborted
        pEP->AbortMask &= ~EP_ABORT_PROCESS_FLAG_MASK;
    #if (USBH_DEBUG > 1)
        if (pEP->pPendingUrb) {
         USBH_ASSERT(USBH_OHCI_EpGlobIsHalt(&pEP->ItemHeader) || USBH_OHCI_EpGlobIsSkipped(&pEP->ItemHeader));
        }
    #endif
        Status = USBH_OHCI_ISO_CheckAndCancelAbortedUrbs(pEP, FALSE);
        if (!Status) {
          // On success it is possible that the completion routine is called and the IsoEpList can be changed
          RestartFlag = TRUE;
          break;
        }
      }
    }
  }
  // Start timer at the end of the timer routine
  if (StartTimer) {
    pDev->IsoEpAbortTimerCancelFlag = FALSE;
    USBH_StartTimer(pDev->hIsoEpAbortTimer, OH_STOP_DELAY_TIME);
  }
}

/*********************************************************************
*
*       USBH_OHCI_ISO_AbortEp
*
*  Function description
*    Aborts all pending URBs. It is allowed to abort more than endpoints
*    at the time. Only after a frame timeout of the last aborted endpoint
*    the timer callback routine is called! Because the ISO  endpoints
*    have different lists different timer callback routines are used.
*
*/
USBH_STATUS USBH_OHCI_ISO_AbortEp(USBH_OHCI_ISO_EP * pIsoEP) {
  USBH_OHCI_DEVICE * pDev;
  USBH_STATUS        Status;

  OH_ISO_VALID(pIsoEP);
  pDev         = pIsoEP->pDev;
  USBH_OCHI_IS_DEV_VALID(pDev);
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: USBH_OHCI_ISO_AbortEp: pIsoEP:0x%x!", pIsoEP->EndpointAddress));
  Status      = USBH_STATUS_SUCCESS;
  //
  // If an TD is pending the Skip bit is set and an timer is scheduled
  // If in the timer routine all pending TDs are removed.
  // Not pending URBs with the abort Flag are removed.
  // As last the Skip bit is reset.
  //
  if (pIsoEP->AbortMask & (EP_ABORT_MASK | EP_ABORT_START_TIMER_MASK)) { // Already aborted
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: USBH_OHCI_ISO_AbortEp: Endpoint 0x%x already aborted!",pIsoEP->EndpointAddress));
    _ISO_MarkUrbsWithCancelPendingFlag(pIsoEP);
    return USBH_STATUS_SUCCESS;
  }
  if (NULL == pIsoEP->pPendingUrb) {
    _ISO_CompletePendingUrbs(pIsoEP, 0); // Cancel the USBH_URB list without skipping of the endpoint
  } else {
    // Skip the endpoint from list processing, wait a frame time until the TD can removed from the endpoint.
    // Because one timer is used for all control endpoints restart the timer in the timer routine if the timer already started.
    USBH_OHCI_EpGlobSetSkip(&pIsoEP->ItemHeader);
    pIsoEP->pPendingUrb->Header.HcFlags |=URB_CANCEL_PENDING_MASK;
    pIsoEP->AbortMask |= EP_ABORT_MASK;
    _ISO_MarkUrbsWithCancelPendingFlag(pIsoEP);
    if (!pDev->IsoEpAbortTimerRunFlag) {
      pDev->IsoEpAbortTimerRunFlag    = TRUE;
      pDev->IsoEpAbortTimerCancelFlag = FALSE;
      USBH_StartTimer(pDev->hIsoEpAbortTimer, OH_STOP_DELAY_TIME);
    } else {
      pIsoEP->AbortMask |= EP_ABORT_START_TIMER_MASK;
    }
  }

  return Status;
}

/*********************************************************************
*
*       USBH_OHCI_ISO_AllocPool
*
*  Function description
*    Allocate all ISO endpoints from a specified pool
*
*/
USBH_STATUS USBH_OHCI_ISO_AllocPool(USBH_HCM_POOL * pEpPool, unsigned int MaxEps) {
  USBH_STATUS Status;

  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: USBH_OHCI_ISO_AllocPool: Eps:%d!",MaxEps));
  if (!MaxEps) {
  return USBH_STATUS_SUCCESS;
  }
  Status = USBH_HCM_AllocPool(pEpPool, MaxEps, OH_ED_SIZE, sizeof(USBH_OHCI_ISO_EP), OH_ED_ALIGNMENT);
  if ( Status ) { // On error
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: USBH_OHCI_ISO_AllocPool: USBH_HCM_AllocPool!"));
  }
  return Status;
}

/*********************************************************************
*
*       USBH_OHCI_ISO_GetEp
*
*  Function description
*    Get an ISO endpoint from a pool
*
*/
USBH_OHCI_ISO_EP * USBH_OHCI_ISO_GetEp(USBH_HCM_POOL * pEpPool) {
  USBH_OHCI_ISO_EP * pItem;
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: USBH_OHCI_ISO_GetEp!"));
  pItem = (USBH_OHCI_ISO_EP*)USBH_HCM_GetItem(pEpPool);
  if (NULL==pItem) {
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: USBH_OHCI_ISO_GetEp: no resources!"));
  } else {
    USBH_ASSERT(USBH_IS_ALIGNED(pItem->ItemHeader.PhyAddr, OH_ED_ALIGNMENT));
    USBH_DLIST_Init(&pItem->ItemHeader.Link.ListEntry); // Init the pItem link list for later deallocating TDs
  }
  return pItem;
}

/*********************************************************************
*
*       USBH_OHCI_ISO_PutEp
*
*  Function description
*    Puts an ISO endpoint back to the pool
*/
void USBH_OHCI_ISO_PutEp(USBH_OHCI_ISO_EP * pEP) {
  OH_ISO_VALID(pEP);
  // Put all TD items back to the TD pool before put back the Ep0 object
  USBH_OHCI_EpGlobRemoveAllTDtoPool(&pEP->ItemHeader, &pEP->TdCounter);
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: USBH_OHCI_ISO_PutEp!"));
  if (NULL != pEP->pCopyBuffer) {
    USBH_HCM_PutItem(&pEP->pCopyBuffer->ItemHeader);
    pEP->pCopyBuffer = NULL;
  }
  USBH_HCM_PutItem(&pEP->ItemHeader);
}

/*********************************************************************
*
*       USBH_OHCI_ISO_InitEp
*
*  Function description
*    Initialize an ISO endpoint
*/
USBH_OHCI_ISO_EP * USBH_OHCI_ISO_InitEp(USBH_OHCI_ISO_EP * pEP, USBH_OHCI_DEVICE * pDev,
                                        U8  EndpointType, U8  DeviceAddress, U8 EndpointAddress,
                                        U16 MaxFifoSize,  U16 IntervalTime,  USBH_SPEED Speed, U32 Flags) {
  USBH_HCM_ITEM_HEADER              * pItem;
  USBH_OHCI_INFO_GENERAL_TRANS_DESC * pTransDesc;
  USBH_BOOL                           SkipFlag;

  USBH_USE_PARA(EndpointType);
  OH_ISO_VALID(pEP);        // The itemheader must be valid
  USBH_OCHI_IS_DEV_VALID(pDev);
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: USBH_OHCI_ISO_InitEp: pDev.Addr: %d EpAddr.: 0x%x max.pktsize: %d interval: %d", (int)DeviceAddress, (int)EndpointAddress, (int)MaxFifoSize, (int)IntervalTime));
  // Recalculate the interval time
  pEP->pCopyBuffer       = NULL;
  pEP->pDev              = pDev;
  pEP->State             = OH_EP_IDLE;
  USBH_DLIST_Init(&pEP->ListEntry);
  USBH_DLIST_Init(&pEP->UrbList);
  pEP->UrbCount               = 0;
  pEP->pPendingUrb            = NULL;
  pEP->UrbTotalTdNumber       = 0;
  pEP->UrbDoneTdNumber        = 0;
  pEP->UpDownTDCounter        = 0;
  pEP->TdCounter              = 0;
  pEP->AbortMask              = 0;
  pEP->CancelPendingFlag      = FALSE;
  // Parameter
  pEP->pfOnReleaseCompletion  = NULL;
  pEP->pReleaseContext        = NULL;
  pEP->Flags                  = Flags;
  pEP->DeviceAddress          = DeviceAddress;
  pEP->EndpointAddress        = EndpointAddress;
  pEP->MaxPacketSize          = MaxFifoSize;
  pEP->Speed                  = Speed;
  pEP->IntervalTime           = IntervalTime;
  pEP->HaltFlag               = FALSE;
  // Allocate a timer
  if (Flags & OH_DUMMY_ED_EPFLAG) {
    SkipFlag = TRUE;
  } else {
    SkipFlag = FALSE;
  }
  pItem = &pEP->ItemHeader;
  USBH_OHCI_EpGlobInitED(pItem, DeviceAddress, EndpointAddress, MaxFifoSize, TRUE, SkipFlag, Speed); // Init DWORD 0..DWORD 3 in the ED
  pTransDesc = USBH_OHCI_GetTransDesc(&pDev->IsoTDPool);                                               // Get an TD pItem from pool
  if (pTransDesc == NULL) {
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: USBH_OHCI_IsoEpInit: USBH_OHCI_GetTransDesc!"));
    USBH_OHCI_ISO_PutEp(pEP);                                                                 // Release the endpoint
    pEP = NULL;
    goto exit;
  }
  USBH_OHCI_EpGlobInsertTD(pItem,&pTransDesc->ItemHeader, &pEP->TdCounter);                   // Link the new TD to the EP TD list, set the ISO list filled bit
exit:
  return pEP;                                                                                 // On error an NULL pointer is returned
}

/*********************************************************************
*
*       USBH_OHCI_ISO_InsertEp
*/
/* Insert an endpoint in the devices endpoint list and in the HC Ed list */
USBH_STATUS USBH_OHCI_ISO_InsertEp(USBH_OHCI_ISO_EP * pEP) {
  USBH_OHCI_DEVICE   * pDev;
  USBH_OHCI_DUMMY_INT_EP * pMinBandwidthEp;

  USBH_ASSERT(pEP != NULL);
  pDev = pEP->pDev;
  USBH_OCHI_IS_DEV_VALID(pDev);
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: INFO USBH_OHCI_INT_InsertEp pIsoEp:0x%x pDev:%d!", pEP->EndpointAddress, (int)pEP->DeviceAddress));
  USBH_OHCI_INT_GetBandwidth(pDev, pEP->IntervalTime, &pMinBandwidthEp);
  if (pMinBandwidthEp == NULL) {
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: USBH_OHCI_INT_InsertEp: USBH_OHCI_INT_GetBandwidth: not endpoint found!"));
    return USBH_STATUS_ERROR;
  }
  // Insert pIsoEp of the returned dummy endpoint user endpoint list
  pEP->State = OH_EP_LINK;
  USBH_OHCI_EpGlobLinkEds(&pMinBandwidthEp->ItemHeader, &pEP->ItemHeader);
  USBH_DLIST_InsertHead(&pMinBandwidthEp->ActiveList, &pEP->ListEntry);
  // Link the new user endpoint to an dummy endpoint that has the minimum bandwidth
  pEP->pDummyIntEp = pMinBandwidthEp;
  // Adds the packet size for this interval
  pMinBandwidthEp->Bandwidth += pEP->MaxPacketSize;
  pDev->IsoEpCount++;
  return USBH_STATUS_SUCCESS;
}

/*********************************************************************
*
*       USBH_OHCI_ISO_ReleaseEndpoint
*
*  Function description
*    Releases an interrupt endpoint without stopping the interrupt list.
*    0. No request should be pending
*    1. Set the Skip bit
*    2. The pointer from the previous endpoint is set to the next endpoint
*    3. An timer is started for two milliseconds
*    4. If the timer is processed the endpoint is put back to the pool
*/
void USBH_OHCI_ISO_ReleaseEndpoint(USBH_OHCI_ISO_EP * pEp, USBH_RELEASE_EP_COMPLETION_FUNC * pfReleaseEpCompletion, void * pContext) {
  USBH_OHCI_DEVICE * pDev;

  OH_ISO_VALID(pEp);
  pDev = pEp->pDev;
  USBH_OCHI_IS_DEV_VALID(pDev);
  USBH_ASSERT(USBH_DLIST_IsEmpty(&pEp->UrbList) );
  USBH_ASSERT(pEp->pPendingUrb == NULL);
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: INFO USBH_OHCI_ISO_ReleaseEndpoint pEp: 0x%x pDev: %d!", pEp->EndpointAddress, pEp->DeviceAddress));
  if (pEp->State == OH_EP_UNLINK || pEp->State == OH_EP_UNLINK_TIMER) {
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

/*********************************************************************
*
*       USBH_OHCI_ISO_AddUrb
*
*  Function description
*    Adds an ISO endpoint request
*/
USBH_STATUS USBH_OHCI_ISO_AddUrb(USBH_OHCI_ISO_EP * pEP, USBH_URB * pUrb) {
  USBH_STATUS Status;

  OH_ISO_VALID(pEP);
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: INFO USBH_OHCI_ISO_AddUrb pEP: 0x%x!", pEP->EndpointAddress));
  USBH_OCHI_IS_DEV_VALID(pEP->pDev);
  USBH_ASSERT(pUrb != NULL);
  if (pEP->State != OH_EP_LINK) {
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI:  USBH_OHCI_ISO_AddUrb: pEP 0x%x not linked!", pEP->EndpointAddress));
    pUrb->Header.Status = USBH_STATUS_ENDPOINT_HALTED;
    return USBH_STATUS_ENDPOINT_HALTED;
  }
  if ( pEP->HaltFlag) {
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI:  USBH_OHCI_ISO_AddUrb: pEP in 0x%x halted!", pEP->EndpointAddress));
    return USBH_STATUS_ENDPOINT_HALTED;
  }
  if (pEP->pPendingUrb == NULL && pEP->AbortMask==0) {
    pEP->UrbCount++;
    if (!USBH_DLIST_IsEmpty(&pEP->UrbList)) {
      USBH_DLIST_InsertTail(&pEP->UrbList,&pUrb->Header.ListEntry);
      Status = USBH_STATUS_PENDING;
      USBH_OHCI_ISO_SubmitUrbsFromList(pEP);       // Submit next USBH_URB from list
    } else {
      Status=_ISO_SubmitUrb(pEP, pUrb);
      if ( Status != USBH_STATUS_PENDING ) { // On error
        USBH_WARN((USBH_MTYPE_OHCI, "OHCI:  USBH_OHCI_ISO_AddUrb: USBH_OHCI_Ep0SubmitUrb: %08x!", Status));
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
*       USBH_OHCI_ISO_UpdateTDLengthStatus
*
*  Function description
*    Called during the first enumeration of the DONE TD list.
*    Updates the EP Length and Status field.
*    Works without any order of completed TDs.
*/
void USBH_OHCI_ISO_UpdateTDLengthStatus(USBH_OHCI_INFO_GENERAL_TRANS_DESC * pIsoTD) {
  USBH_OHCI_ISO_EP * pIsoEp;
  USBH_URB         * pUrb;
  USBH_STATUS        TDStatus;
  U32                Length;
  USBH_BOOL          shortPkt;

  pIsoEp              = (USBH_OHCI_ISO_EP *)pIsoTD->pEp;
  OH_ISO_VALID(pIsoEp);
  USBH_ASSERT(pIsoEp->pPendingUrb != NULL);
  pIsoEp->UrbDoneTdNumber++;                            // Total TDs of the USBH_URB
  pIsoEp->UpDownTDCounter++;                            // Help count: Number of TDs found in the current DONE list
  pUrb = pIsoEp->pPendingUrb;
  // Get TD Status and Length, shortPkt not used because on ISO endpoint
  // only on the last TD the buffer rounding bit is set, on all other TD the ED goes in HALT!
  TDStatus = USBH_OHCI_ISO_TdGetStatusAndLength(pIsoTD, &Length, &shortPkt);
  if ( TDStatus != USBH_STATUS_SUCCESS) {           // Save the last error
    pUrb->Header.Status = TDStatus;
#if (USBH_DEBUG > 1)
    if (TDStatus != USBH_STATUS_DATA_UNDERRUN) {
      USBH_LOG((USBH_MTYPE_OHCI, "OHCI: OHCI ISO transfer error condition code: %u pEp: 0x%x!", TDStatus, (int)pIsoEp->EndpointAddress));
    }
#endif
  }
  if (shortPkt) {
    pIsoEp->Flags |= OH_SHORT_PKT_EPFLAG;
  }
  if (NULL ==pIsoEp->pCopyBuffer) {                      // USBH_URB buffer is used
    pUrb->Request.IsoRequest.Length += Length;
  }  else if( Length ) {                            // Bytes transferred, update buffer and pointer
    if ( pIsoEp->EndpointAddress & USB_IN_DIRECTION ) { // It is needed to update the copy buffer before it read
      USBH_OHCI_CopyToUrbBufferUpdateTransferBuffer(pIsoEp->pCopyBuffer, Length);
    } else {
      USBH_OHCI_UpdateCopyTransferBuffer           (pIsoEp->pCopyBuffer, Length);
    }
  }
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: INFO USBH_OHCI_ISO_UpdateTDLengthStatus pEp 0x%x transferred: %u !", (int)pIsoEp->EndpointAddress, Length));
}

/*********************************************************************
*
*       USBH_OHCI_ISO_CheckForCompletion
*
*  Function description
*    USBH_OHCI_ISO_CheckForCompletion is called during the second enumeration
*    of the DONE TD list. If the pipe is halted (on transfer error) all
*    other pending TDs are removed and the USBH_URB is completed. If all TDs
*    are done the USBH_URB is also completed. Then the next one is submitted!
*/
void USBH_OHCI_ISO_CheckForCompletion(USBH_OHCI_INFO_GENERAL_TRANS_DESC * pIsoTD) {
  USBH_OHCI_ISO_EP * pEp;
  USBH_URB         * pUrb;
  USBH_STATUS        UrbStatus;
  int                IsCompleted;
  USBH_STATUS        Status = USBH_STATUS_SUCCESS;

  pEp = (USBH_OHCI_ISO_EP *)pIsoTD->pEp;
  OH_ISO_VALID(pEp);
  USBH_ASSERT_PTR(pEp->pPendingUrb);
  // Delete TD
  USBH_OHCI_EpGlobDeleteDoneTD(&pIsoTD->ItemHeader, &pEp->TdCounter);
  pEp->UpDownTDCounter--;
  if (pEp->UpDownTDCounter) {
    return;
  }
  pUrb        = pEp->pPendingUrb;
  UrbStatus = pUrb->Header.Status;
  IsCompleted   = FALSE;
  if (NULL == pEp->pCopyBuffer) {
    if (pEp->TdCounter == 1 || USBH_STATUS_PENDING != UrbStatus) {
      IsCompleted = TRUE;
    }
  } else if ((pEp->TdCounter == 1 && 0 == pEp->pCopyBuffer->RemainingLength) || USBH_STATUS_PENDING != UrbStatus || (pEp->Flags & OH_SHORT_PKT_EPFLAG)) {
    IsCompleted = TRUE;
  }
  if (!IsCompleted) {
    if (pEp->AbortMask & EP_ABORT_SKIP_TIME_OVER_MASK) {          // Endpoint is aborted and the host list processing is stopped
      USBH_ASSERT((pUrb->Header.HcFlags & URB_CANCEL_PENDING_MASK)); // For testing
      if ( pUrb->Header.HcFlags & URB_CANCEL_PENDING_MASK ) {     // USBH_URB must be canceled
        IsCompleted = TRUE;
      }
    }
  }
//  {
//    U32   length;
//    U8  * buffer;
//
//    if (!IsCompleted) {
//      if (NULL != pEp->pCopyBuffer) {
//        // Copy new OUT buffer / Get buffer and length / Submit the next packet and return on success!
//        if (!(pEp->EndpointAddress & USB_IN_DIRECTION)) {
//          USBH_OHCI_FillCopyTransferBuffer(pEp->pCopyBuffer);
//        }
//        buffer = USBH_OHCI_GetBufferLengthFromCopyTransferBuffer(pEp->pCopyBuffer, &length);
//        if (length) {
//          Status = _ISO_SubmitTransfer(pEp, &pUrb->Request.IsoRequest);
//          if (Status) {
//            USBH_WARN((USBH_MTYPE_OHCI, "OHCI: USBH_OHCI_ISO_CheckForCompletion:_ISO_SubmitTransfer!"));
//          }
//        }
//        if ( !Status ) { // On success
//          return;
//        }
//      }
//    }
//  }
  // USBH_URB IsCompleted (If all TDs transferred or not all and the HC has stopped the endpoint because of an error or abort)
  // Update USBH_URB pBuffer and Length
  if (NULL != pEp->pCopyBuffer) {
    if (Status) {
      UrbStatus = Status;
    }
    pUrb->Request.IsoRequest.Length = pEp->pCopyBuffer->Transferred;
  }
#if (USBH_DEBUG > 1) // Infos
  if (pEp->TdCounter != 1) {
    USBH_LOG((USBH_MTYPE_OHCI, "OHCI: INFO USBH_OHCI_ISO_CheckForCompletion: Complete USBH_URB: active TDS!"));
  }
  if (USBH_OHCI_EpGlobIsSkipped(&pEp->ItemHeader)) {
    USBH_LOG((USBH_MTYPE_OHCI, "OHCI: INFO USBH_OHCI_ISO_CheckForCompletion: Complete USBH_URB: ED is skipped!"));
  }
  if(USBH_OHCI_EpGlobIsHalt(&pEp->ItemHeader)) {
    USBH_LOG((USBH_MTYPE_OHCI, "OHCI: INFO USBH_OHCI_ISO_CheckForCompletion: Complete USBH_URB: ED halted!"));
  }
  // USBH_URB IsCompleted or aborted
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: INFO DONE: ISO IsCompleted or aborted:pIsoEP: 0x%x transferred: %u Status %s:",
                              (unsigned int)pEp->EndpointAddress, pUrb->Request.IsoRequest.Length, USBH_GetStatusStr(UrbStatus)));
#endif

  if (pEp->TdCounter > 1) {
   USBH_OHCI_EpGlobDeleteAllPendingTD(&pEp->ItemHeader,&pEp->TdCounter); // Delete all not processed TDs
  }
  if (UrbStatus == USBH_STATUS_PENDING) {
    UrbStatus = USBH_STATUS_SUCCESS;
  } else if (UrbStatus == USBH_STATUS_DATA_UNDERRUN) {
    if (USBH_OHCI_EpGlobIsHalt(&pEp->ItemHeader)) {
      USBH_OHCI_EpGlobClearHalt(&pEp->ItemHeader);
    }
    UrbStatus = USBH_STATUS_SUCCESS;
  }
  if (USBH_OHCI_EpGlobIsHalt(&pEp->ItemHeader)) {
    pEp->HaltFlag=TRUE; // Always halt set flag
  }
  if (pEp->AbortMask) {
                       // Endpoint is aborted: Complete pending USBH_URB / Clear the skip bit / Complete aborted URBs resubmit new added URBS
    USBH_OHCI_ISO_CheckAndCancelAbortedUrbs(pEp, TRUE);
  } else {             // TRUE flag: submit an new USBH_URB before the old one is completed
    _ISO_CompleteUrb(pEp, pEp->pPendingUrb, UrbStatus, TRUE);
  }
}

/******************************* EOF ********************************/
