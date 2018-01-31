/*********************************************************************
*  
*   IAR PowerPac
*
*   (c) Copyright IAR Systems 2010.  All rights reserved.
*
**********************************************************************
----------------------------------------------------------------------
File        : USBH_OHC_td.c
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
*       _EP0_CompleteUrb
*
*  Function description
*    Condition code of the transfer or an driver status
*/
static void _EP0_CompleteUrb(USBH_OHCI_EP0 * Ep, USBH_URB * Urb, USBH_STATUS Status) {
  OH_EP0_VALID(Ep);
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: INFO _EP0_CompleteUrb: USBH_URB status: %08x!", Status));
  if (Urb != NULL) {             // valid USBH_URB
    if (NULL != Ep->pDataPhaseCopyBuffer) {
      Urb->Request.ControlRequest.Length = Ep->pDataPhaseCopyBuffer->Transferred;
      USBH_HCM_PutItem(&Ep->pDataPhaseCopyBuffer->ItemHeader);
      Ep->pDataPhaseCopyBuffer = NULL;
    }
    Urb->Header.Status = Status;
    if (Urb == Ep->pPendingUrb) { // If an USBH_URB is canceled an this is the pending USBH_URB
#if (USBH_DEBUG > 1)
      if (Ep->TdCounter > 1) {   // Active Transfer descriptors, this excludes the default TD
        USBH_WARN((USBH_MTYPE_OHCI, "OHCI: _EP0_CompleteUrb: complete the current USBH_URB and more than the default TD on this EP are not released!"));
      }
#endif

      Ep->pPendingUrb = NULL;
    }
    USBH_ASSERT(Ep->UrbCount);
    Ep->UrbCount--;
    Urb->Header.pfOnInternalCompletion(Urb); // Call the completion routine
  }
}

/*********************************************************************
*
*       _EP0_CancelUrbList
*
*  Function description
*
*  Parameters:
*    AbortFlag: TRUE:  Only aborted URBs are completed.
*               FALSE: All URBs are completed.
*/
static void _EP0_CancelUrbList(USBH_OHCI_EP0 * Ep, U32 HcFlagMask) {
  USBH_DLIST  * pListHead;
  USBH_DLIST  * pEntry;
  USBH_DLIST  * pAbortEntry;
  USBH_DLIST    AbortList;
  USBH_URB    * pUrb;
  OH_EP0_VALID(Ep);
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: _EP0_CancelUrbList!"));
  USBH_DLIST_Init(&AbortList);
  pListHead = &Ep->UrbList;
  pEntry    = USBH_DLIST_GetNext(pListHead);

  while (pEntry != pListHead) {
    pUrb        = (USBH_URB *)GET_URB_HEADER_FROM_ENTRY(pEntry);
    pAbortEntry = pEntry;
    pEntry      = USBH_DLIST_GetNext(pEntry);
    if (0 == HcFlagMask || (HcFlagMask &pUrb->Header.HcFlags)) { // If parameter HcFlagMask is set then remove only URBs where the USBH_URB flag is set
      USBH_DLIST_RemoveEntry(pAbortEntry);
      USBH_DLIST_InsertHead(&AbortList, pAbortEntry);
    }
  }
  while (!USBH_DLIST_IsEmpty(&AbortList)) {
    USBH_DLIST_RemoveTail(&AbortList, &pEntry);
    pUrb = (USBH_URB * )GET_URB_HEADER_FROM_ENTRY(pEntry);
    _EP0_CompleteUrb(Ep, pUrb, USBH_STATUS_CANCELED);
  }
}

/*********************************************************************
*
*       _EP0_Unlink
*
*  Function description
*    The entry is removed only from the devices control endpoint list,
*    not from the remove pending list!
*/
static void _EP0_Unlink(USBH_OHCI_EP0 * Ep) {
  USBH_OHCI_DEVICE * pDev;
  USBH_DLIST       * pPreviousEntry;
  USBH_OHCI_EP0    * pPreviousEp;
  U32                nextPhyAddr;

  OH_EP0_VALID(Ep);
  pDev = Ep->pDev;
  USBH_OCHI_IS_DEV_VALID(pDev);
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: _EP0_Unlink pDev.Addr: %u!", Ep->DeviceAddress));
  USBH_ASSERT(pDev->ControlEpCount);
  USBH_ASSERT(Ep->pPendingUrb == NULL);
  // HC list must be disabled
  USBH_ASSERT(0 == OhHalTestReg(pDev->pRegBase, OH_REG_CONTROL, HC_CONTROL_CLE));
  USBH_ASSERT(!USBH_DLIST_IsEmpty(&pDev->ControlEpList));
  pPreviousEntry = USBH_DLIST_GetPrev(&Ep->ListEntry); // Get the list tail
  if (pPreviousEntry == &pDev->ControlEpList) {
    USBH_LOG((USBH_MTYPE_OHCI, "OHCI: INFO _EP0_Unlink: remove the first ED in the  ED list!"));
    nextPhyAddr = USBH_OHCI_EpGlobUnlinkEd(NULL, &Ep->ItemHeader);
    OhHalWriteReg(pDev->pRegBase, OH_REG_CONTROLHEADED, nextPhyAddr);
  } else {
    pPreviousEp = GET_CONTROL_EP_FROM_ENTRY(pPreviousEntry);
    OH_EP0_VALID(pPreviousEp);
    USBH_OHCI_EpGlobUnlinkEd(&pPreviousEp->ItemHeader, &Ep->ItemHeader);
  }
  // Remove ep from logical list and set endpoint in Idle state
  USBH_DLIST_RemoveEntry(&Ep->ListEntry);
  pDev->ControlEpCount--;
  Ep->State = OH_EP_IDLE;
  if (0 == pDev->ControlEpCount) {
    USBH_LOG((USBH_MTYPE_OHCI, "OHCI: INFO _EP0_Unlink: ED list now empty!"));
  }
}

/*********************************************************************
*
*       _EP0_MarkUrbsWithCancelPendingFlag
*
*  Function description
*/
static void _EP0_MarkUrbsWithCancelPendingFlag(USBH_OHCI_EP0 * Ep) {
  USBH_DLIST  * pListHead;
  USBH_DLIST  * pEntry;
  USBH_URB    * pUrb;
  OH_EP0_VALID(Ep);
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: USBH_OHCI_Ep0UrbListSetAbortFlag!"));
  pListHead = &Ep->UrbList;
  pEntry    = USBH_DLIST_GetNext(pListHead);
  while (pEntry != pListHead) {
    pUrb = (USBH_URB *)GET_URB_HEADER_FROM_ENTRY(pEntry); // The USBH_URB header is the first element in the USBH_URB, cast it
#if (USBH_DEBUG > 1)
    if (pUrb->Header.HcFlags & URB_CANCEL_PENDING_MASK) {
      USBH_WARN((USBH_MTYPE_OHCI, "OHCI: USBH_OHCI_Ep0UrbListSetAbortFlag: URB_CANCEL_PENDING_MASK already set!"));
    }
#endif
    pUrb->Header.HcFlags |= URB_CANCEL_PENDING_MASK;
    pEntry                = USBH_DLIST_GetNext(pEntry);
  }
}

/*********************************************************************
*
*       _EP0_SubmitPacket
*
*  Return value
*    USBH_STATUS_PENDING on success.
*    other values: errors
*/
static USBH_STATUS _EP0_SubmitPacket(USBH_OHCI_EP0 * Ep, U8 * Buffer, U32 Length, U32 TDword0Mask, USBH_OHCI_TD_PID Pid) {
  USBH_STATUS   status;
  USBH_OHCI_INFO_GENERAL_TRANS_DESC     * td;
  U32           startAddr, endAddr;

  OH_EP0_VALID(Ep);
  USBH_ASSERT(0 == Ep->AbortMask);
  status = USBH_STATUS_SUCCESS;
#if (USBH_DEBUG > 1)
  if (Length) {
    USBH_ASSERT_PTR(Buffer);
  }
#endif
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: _EP0_SubmitPacket Length: %lu!", Length));
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: Start transfer: Length: %lu ",   Length));
  for (; ;) {
    // Get the not used TD
    td = USBH_OHCI_EpGlobGetLastTDFromED(&Ep->ItemHeader);
    if (td == NULL) {
      USBH_WARN((USBH_MTYPE_OHCI, "OHCI: _EP0_SubmitPacket: USBH_OHCI_EpGlobGetLastTDFromED!"));
      status = USBH_STATUS_ERROR;
      break;
    }
    // Get phys. addresses for the control endpoint
    startAddr = (U32)Buffer;
    endAddr   = startAddr + Length - 1;
    // Init the not used TD in ED
    USBH_LOG((USBH_MTYPE_OHCI, "OHCI: USBH_OHCI_TdInit: ep: 0x%x ", Ep->EndpointAddress));
    USBH_OHCI_TdInit(td, Ep, USB_EP_TYPE_CONTROL, Pid, startAddr, endAddr, TDword0Mask);
    // Get an new TD
    td = USBH_OHCI_GetTransDesc(&Ep->pDev->GTDPool);
    if (td == NULL) {
      USBH_WARN((USBH_MTYPE_OHCI, "OHCI: USBH_OHCI_EP0_DoneCheckForCompletion: USBH_OHCI_GetTransDesc!"));
      status = USBH_STATUS_MEMORY;
      break;
    }
    // Add the new TD to the TD list
    USBH_OHCI_EpGlobInsertTD(&Ep->ItemHeader, &td->ItemHeader, &Ep->TdCounter);
    // Set always the control list filled bit, this ensure that the OHCI scans the control endpoint list again!
    if (!Ep->pDev->ControlEpRemoveTimerRunFlag) {
      // Update the list control register
      USBH_OHCI_EndpointListEnable(Ep->pDev, USB_EP_TYPE_CONTROL, TRUE, TRUE);
    }
    break;
  }
  if (!status) { // On success
    status = USBH_STATUS_PENDING;
  }
  return status;
}

/*********************************************************************
*
*       _EP_SubmitUrb
*
*  Function description
*    _EP_SubmitUrb submits always Ep->pPendingUrb, starts with the SETUP phase
*
*  Parameters
*    Ep valid pointer to an endpoint!
*
*  Returns:
*    URB_STATUS_PENDING: USBH_URB successfully submitted.
*    other errors:       Error on submitting the USBH_URB.
*/
static USBH_STATUS _EP_SubmitUrb(USBH_OHCI_EP0 * Ep, USBH_URB * urb) {
  USBH_STATUS            status;
  USBH_CONTROL_REQUEST * urbRequest;
  OH_EP0_VALID(Ep);
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: _EP_SubmitUrb!"));
  // The PendingURB is submitted
  USBH_ASSERT(urb != NULL);
  USBH_ASSERT(Ep->pPendingUrb == NULL);
  USBH_ASSERT(0 == (urb->Header.HcFlags &URB_CANCEL_PENDING_MASK));
  // Set the control endpoint phase
  Ep->Ep0Phase = ES_SETUP;
  urbRequest   = &urb->Request.ControlRequest;
  USBH__ConvSetupPacketToBuffer(&urbRequest->Setup, Ep->pSetup);
  status       = _EP0_SubmitPacket(Ep, Ep->pSetup, USB_SETUP_PACKET_LEN, OHCI_TD_FORCE_DATA0, OH_SETUP_PID);
  if (status != USBH_STATUS_PENDING) {
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: _EP_SubmitUrb!"));
  } else { // On success
    Ep->pPendingUrb = urb;
  }
  return status;
}

/*********************************************************************
*
*       _EP0_SubmitUrbsFromList
*
*  Function description
*/
static void _EP0_SubmitUrbsFromList(USBH_OHCI_EP0 * Ep) {
  USBH_URB         * urb;
  USBH_STATUS   status;
  USBH_DLIST       * pEntry;
  int           emptyFlag;
  OH_EP0_VALID(Ep);
  if (Ep->State != OH_EP_LINK) {
    USBH_LOG((USBH_MTYPE_OHCI, "OHCI: _EP0_SubmitUrbsFromList: Ep0 on pDev.address %d not in LINK state!", (int)Ep->DeviceAddress));
    return;
  }
  if (Ep->AbortMask != 0) {
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: _EP0_SubmitUrbsFromList: Ep0 on pDev.address %d is aborted!", (int)Ep->DeviceAddress));
    return;
  }
  if (NULL != Ep->pPendingUrb) {
    return;
  }
  emptyFlag = USBH_DLIST_IsEmpty(&Ep->UrbList);
  while (!emptyFlag) {
    // Submit the next USBH_URB
    USBH_DLIST_RemoveHead(&Ep->UrbList, &pEntry);
    emptyFlag = USBH_DLIST_IsEmpty(&Ep->UrbList);
    urb       = (USBH_URB *)GET_URB_HEADER_FROM_ENTRY(pEntry);
    status    = _EP_SubmitUrb(Ep, urb);
    if (status == USBH_STATUS_PENDING) { // On success stop
      break;
    } else { // On error
      if (Ep->UrbCount == 1) {
        _EP0_CompleteUrb(Ep, Ep->pPendingUrb, status); // Endpoint can be invalid
        break;
      } else {
        _EP0_CompleteUrb(Ep, Ep->pPendingUrb, status);
      }
    }
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
*       USBH_OHCI_Ep0RemoveEndpoints
*
*  Function description
*    AllEndpointFlag - all endpoints are removed regardless of the
*    endpoint state, no need to check the state
*/
void USBH_OHCI_Ep0RemoveEndpoints(USBH_OHCI_DEVICE * pDev, USBH_BOOL AllEndpointFlag) {
  USBH_OHCI_EP0                         * ep;
  USBH_DLIST                      * pEntry;
  USBH_DLIST                        removeList;
  USBH_RELEASE_EP_COMPLETION_FUNC * pfCompletion;
  void                            * pContext;

  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: !"));
  USBH_OCHI_IS_DEV_VALID(pDev);

#if (USBH_DEBUG > 1)
  if (OhHalTestReg(pDev->pRegBase, OH_REG_CONTROL, HC_CONTROL_CLE)) {
    USBH_PANIC("FATAL :USBH_OHCI_Ep0RemoveEndpoints control endpoint list is not disabled!");
  }
#endif
  USBH_DLIST_Init(&removeList);
  // Build the removeList
  pEntry = USBH_DLIST_GetPrev(&pDev->ControlEpList);
  while (pEntry != &pDev->ControlEpList) {
    ep    = GET_CONTROL_EP_FROM_ENTRY(pEntry);
    OH_EP0_VALID(ep);
    pEntry = USBH_DLIST_GetPrev(pEntry);
    if (AllEndpointFlag) {
      _EP0_Unlink(ep);
      USBH_DLIST_InsertHead(&removeList, &ep->ListEntry);
    } else {
      if (ep->State == OH_EP_UNLINK) {
        _EP0_Unlink(ep);
        USBH_DLIST_InsertHead(&removeList, &ep->ListEntry);
      }
    }
  }
  // Enable the endpoint list processing
  if (pDev->ControlEpCount > 1) {
    // More than dummy endpoints available
    USBH_OHCI_EndpointListEnable(pDev, USB_EP_TYPE_CONTROL, TRUE, TRUE);
  }
#if (USBH_DEBUG > 1)
  if (pDev->ControlEpCount == 0) {
    USBH_LOG((USBH_MTYPE_OHCI, "OHCI: USBH_OHCI_Ep0RemoveEndpoints INFO : empty endpoint list !"));
  }
#endif
  while (!USBH_DLIST_IsEmpty(&removeList)) { // Delete the endpoint, call the completion routine
    USBH_DLIST_RemoveHead(&removeList, &pEntry);
    ep           = GET_CONTROL_EP_FROM_ENTRY(pEntry);
    OH_EP0_VALID(ep);
    pfCompletion = ep->pfReleaseCompletion;
    pContext     = ep->pReleaseContext;
    USBH_OHCI_EP0_Put    (ep);
    if (pfCompletion) {
      pfCompletion(pContext); // Call the completion routine, attention: runs in this context
    }
  }
}

/*********************************************************************
*
*       USBH_OHCI_EP0_OnReleaseEpTimer
*
*  Function description
*    Called if one or more endpoints has been removed!
*
*  Parameters:
*    Context: pDev Ptr.
*/
void USBH_OHCI_EP0_OnReleaseEpTimer(void * pContext) {
  USBH_OHCI_DEVICE * pDev;
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: USBH_OHCI_EP0_OnReleaseEpTimer!"));
  USBH_ASSERT(pContext != NULL);
  pDev = (USBH_OHCI_DEVICE *)pContext;
  USBH_OCHI_IS_DEV_VALID(pDev);
  if (pDev->ControlEpRemoveTimerCancelFlag) {
    pDev->ControlEpRemoveTimerCancelFlag = FALSE;
    return;
  }
  pDev->ControlEpRemoveTimerRunFlag = FALSE; // Timer is stopped
  USBH_OHCI_Ep0RemoveEndpoints(pDev, FALSE);         // FALSE: release only unlinked state endpoints
}

/*********************************************************************
*
*       USBH_OHCI_EP0_Alloc
*
*  Function description
*/
USBH_STATUS USBH_OHCI_EP0_Alloc(USBH_HCM_POOL * EpPool, USBH_HCM_POOL * pSetupPacketPool, U32 Numbers) {
  USBH_STATUS status;
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: USBH_OHCI_EP0_Alloc!"));
  status = USBH_HCM_AllocPool(EpPool, Numbers, OH_ED_SIZE, sizeof(USBH_OHCI_EP0), OH_ED_ALIGNMENT);
  if (status) {          // On error
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: USBH_OHCI_EP0_Alloc: USBH_HCM_AllocPool!"));
  }
  status = USBH_HCM_AllocPool(pSetupPacketPool, Numbers, USB_SETUP_PACKET_LEN, sizeof(SETUP_BUFFER), USBH_TRANSFER_BUFFER_ALIGNMENT);
  if (status) {          // On error
    USBH_HCM_FreePool(EpPool); // Frees the control endpoint pool
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: USBH_OHCI_EP0_Alloc: pSetup packets!"));
  }
  return status;
}

/*********************************************************************
*
*       USBH_OHCI_EP0_Get
*
*  Function description
*/
USBH_OHCI_EP0 * USBH_OHCI_EP0_Get(USBH_HCM_POOL * EpPool, USBH_HCM_POOL * pSetupPacketPool) {
  USBH_OHCI_EP0      * pItem;
  SETUP_BUFFER * pSetupPacket;
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: USBH_OHCI_EP0_Get!"));
  pItem = (USBH_OHCI_EP0 * )USBH_HCM_GetItem(EpPool);
  if (NULL == pItem) {
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: USBH_OHCI_EP0_Get: no resources!"));
  } else {                           // Init the item link list for TDs
    USBH_DLIST_Init(&pItem->ItemHeader.Link.ListEntry);
    pSetupPacket = (SETUP_BUFFER *)USBH_HCM_GetItem(pSetupPacketPool);
    if (NULL == pSetupPacket) {
      USBH_WARN((USBH_MTYPE_OHCI, "OHCI: USBH_OHCI_EP0_Get: no pSetup packet resources!"));
      USBH_HCM_PutItem(&pItem->ItemHeader); // Release also the ED memory
      pItem = NULL;
    } else {                         // On success
      pItem->pSetupPacket = pSetupPacket;
      pItem->pSetup       = (U8 *)pSetupPacket->ItemHeader.PhyAddr;
    }
    USBH_ASSERT(USBH_IS_ALIGNED(pItem->ItemHeader.PhyAddr, OH_ED_ALIGNMENT));
  }
  return pItem;
}

/*********************************************************************
*
*       USBH_OHCI_EP0_Put
*
*  Function description
*/
void USBH_OHCI_EP0_Put(USBH_OHCI_EP0 * Ep) {
  USBH_OHCI_EpGlobRemoveAllTDtoPool(&Ep->ItemHeader, &Ep->TdCounter); // Put all TD items back to the TD pool before put back the Ep0 object
  if (Ep->pSetup != NULL) {
    USBH_HCM_PutItem(&Ep->pSetupPacket->ItemHeader);
    Ep->pSetup = NULL;
  }
  if (NULL != Ep->pDataPhaseCopyBuffer) {
    USBH_HCM_PutItem(&Ep->pDataPhaseCopyBuffer->ItemHeader);
    Ep->pDataPhaseCopyBuffer = NULL;
  }
  USBH_HCM_PutItem(&Ep->ItemHeader);
}

/*********************************************************************
*
*       USBH_OHCI_EP0_Init
*
*  Function description
*    Initializes the control endpoint object, allocates an empty transfer
*    descriptor and link it to the endpoint.
*  Return value:
*    Pointer to the endpoint object
*    == NULL: Error during initialization, the endpoint is not deallocated!
*/
USBH_STATUS USBH_OHCI_EP0_Init(USBH_OHCI_EP0 * Ep, USBH_OHCI_DEVICE * pDev, U32 Mask, U8 DeviceAddress, U8 EndpointAddress, U16 MaxFifoSize, USBH_SPEED Speed) {
  USBH_STATUS       status;
  USBH_HCM_ITEM_HEADER * item;
  USBH_OHCI_INFO_GENERAL_TRANS_DESC         * td;
  USBH_BOOL         SkipFlag;

  USBH_OCHI_IS_DEV_VALID(pDev);
  // Do not clear the Ep0 struct because it contains a valid item header and a valid Setup packet pointer
  OH_EP0_VALID(Ep);
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: USBH_OHCI_EP0_Init!"));
  status                  = USBH_STATUS_SUCCESS;
  Ep->pDataPhaseCopyBuffer = NULL;
  Ep->EndpointType        = USB_EP_TYPE_CONTROL;
  Ep->pDev                 = pDev;        // Backward pointer to the device
  Ep->State               = OH_EP_IDLE; // Endpoint is not linked
  USBH_DLIST_Init(&Ep->ListEntry);
  USBH_DLIST_Init(&Ep->UrbList);
  Ep->UrbCount            = 0;
  Ep->pPendingUrb          = NULL;
  Ep->TdCounter           = 0;
  Ep->AbortMask           = 0;

  Ep->Ep0Phase            = ES_IDLE;    // No request
  Ep->pfReleaseCompletion   = NULL;       // Parameter
  Ep->Mask                = Mask;
  Ep->DeviceAddress       = DeviceAddress;
  Ep->EndpointAddress     = EndpointAddress;
  Ep->MaxPacketSize          = MaxFifoSize;
  Ep->Speed               = Speed;

  if (Mask & OH_DUMMY_ED_EPFLAG) {
    SkipFlag = TRUE;
  } else {
    SkipFlag = FALSE;
  }
  item = &Ep->ItemHeader;
  USBH_OHCI_EpGlobInitED(item, DeviceAddress, EndpointAddress, MaxFifoSize, FALSE, SkipFlag, Speed); // Init DWORD 0..DWORD 3 in the ED
  td = USBH_OHCI_GetTransDesc(&pDev->GTDPool);                                                               // Get a TD item from pool
  if (td == NULL) {
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: USBH_OHCI_EP0_Init: USBH_OHCI_GetTransDesc!"));
    status = USBH_STATUS_MEMORY;
    goto Exit;
  }
  // The added TD must not be initialized because Head=TailPointer=address of first added TD
  // Link the new TD to the EP TD list!
  USBH_OHCI_EpGlobInsertTD(item, &td->ItemHeader, &Ep->TdCounter);
Exit:
  return status; // On error an NULL pointer is returned
}

/*********************************************************************
*
*       USBH_OHCI_EP0_AddUrb
*
*  Function description
*    Adds a control endpoint request.
*
*  Return value:
*    USBH_STATUS_PENDING on success
*    other values are errors
*/
USBH_STATUS USBH_OHCI_EP0_AddUrb(USBH_OHCI_EP0 * Ep, USBH_URB * Urb) {
  USBH_CONTROL_REQUEST * urbRequest;
  USBH_STATUS            status;
  OH_EP0_VALID(Ep);
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: USBH_OHCI_EP0_AddUrb!"));
  /* pDev checks the endpoint object */
  USBH_OCHI_IS_DEV_VALID(Ep->pDev);
  USBH_ASSERT(Urb != NULL);
  urbRequest         = &Urb->Request.ControlRequest;
  urbRequest->Length = 0;
  if (Ep->State != OH_EP_LINK) {
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: USBH_OHCI_EP0_AddUrb: State != OH_EP_LINK, USBH_URB is canceled!"));
    Urb->Header.Status = USBH_STATUS_CANCELED;
    return USBH_STATUS_CANCELED;
  }
  if (Ep->UrbCount == 0 && Ep->AbortMask == 0) {
    // If no pending USBH_URB is available the URBlist can contain aborted
    // URBs and this USBH_URB is submitted during in the completion routine
    Ep->UrbCount++;
    status = _EP_SubmitUrb(Ep, Urb);
    if (status != USBH_STATUS_PENDING) { // On error
      USBH_WARN((USBH_MTYPE_OHCI, "OHCI: USBH_OHCI_EP0_AddUrb: _EP_SubmitUrb: %08x!", status));
      Ep->UrbCount--;
    }
    return status;
  } else { // USBH_URB is pending, add it to the USBH_URB list and return pending
    Ep->UrbCount++;
    USBH_DLIST_InsertTail(&Ep->UrbList, &Urb->Header.ListEntry);
    status = USBH_STATUS_PENDING;
  }
  return status;
}

/*********************************************************************
*
*       USBH_OHCI_EP0_Insert
*
*  Function description
*    Adds an endpoint to the devices control endpoint list. The endpoint
*    must be initialized with one TD! First the next pointer is set and
*    then the next pointer of the previous endpoint!
*/
void USBH_OHCI_EP0_Insert(USBH_OHCI_EP0 * pEp) {
  USBH_OHCI_DEVICE * pDev;
  U8             * pBase;
  USBH_OHCI_EP0        * pTailEp;
  USBH_DLIST          * pEntry;

  OH_EP0_VALID(pEp);
  pDev       = pEp->pDev;
  USBH_ASSERT(pEp != NULL);
  USBH_OCHI_IS_DEV_VALID(pDev);
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: USBH_OHCI_EP0_Insert!"));
  pBase      = pEp->pDev->pRegBase;
  if (0 == pDev->ControlEpCount) { // Empty control ED list
    OhHalWriteReg(pBase, OH_REG_CONTROLHEADED, pEp->ItemHeader.PhyAddr);
  } else {
    USBH_ASSERT(!USBH_DLIST_IsEmpty  (&pDev->ControlEpList));
    pEntry  =  USBH_DLIST_GetPrev(&pDev->ControlEpList);
    pTailEp = GET_CONTROL_EP_FROM_ENTRY(pEntry);
    OH_EP0_VALID    (pTailEp);
    USBH_OHCI_EpGlobLinkEds(&pTailEp->ItemHeader, &pEp->ItemHeader);
  }
  pEp->State = OH_EP_LINK;
  // Logical link
  USBH_DLIST_InsertTail(&pDev->ControlEpList, &pEp->ListEntry);
  pDev->ControlEpCount++;
}

/*********************************************************************
*
*       USBH_OHCI_EP0_ReleaseEp
*
*  Function description
*    Releases that endpoint. This function returns immediately.
*    If the Completion function is called the endpoint is removed.
*/
void USBH_OHCI_EP0_ReleaseEp(USBH_OHCI_EP0 * Ep, USBH_RELEASE_EP_COMPLETION_FUNC * pfReleaseEpCompletion, void * pContext) {
  USBH_OHCI_DEVICE * pDev;

  OH_EP0_VALID(Ep);
  pDev = Ep->pDev;
  USBH_OCHI_IS_DEV_VALID(pDev);
  USBH_ASSERT(USBH_DLIST_IsEmpty(&Ep->UrbList));
  USBH_ASSERT(Ep->pPendingUrb == NULL);
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: USBH_OHCI_EP0_ReleaseEp!"));
  if (Ep->State == OH_EP_UNLINK) {
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: USBH_OHCI_EP0_ReleaseEp: Endpoint already unlinked, return!"));
    return;
  }
  Ep->pReleaseContext    = pContext;
  Ep->pfReleaseCompletion = pfReleaseEpCompletion;
  Ep->State             = OH_EP_UNLINK;
  USBH_OHCI_EpGlobSetSkip(&Ep->ItemHeader); // Additional set the skip bit
  if (!pDev->ControlEpRemoveTimerRunFlag) {
    // If this is the first endpoint that must be deleted in the control endpoint list of the HC
    USBH_OHCI_EndpointListEnable(pDev, USB_EP_TYPE_CONTROL, FALSE, FALSE);
    pDev->ControlEpRemoveTimerRunFlag    = TRUE;
    pDev->ControlEpRemoveTimerCancelFlag = FALSE;
    USBH_StartTimer(pDev->ControlEpRemoveTimer, OH_STOP_DELAY_TIME);
  }
}

/*********************************************************************
*
*       USBH_OHCI_EP0_CompleteAbortedUrbs
*
*  Function description
*    Completes all aborted URbs clears the endpoint abort mask and resubmit new
*    not aborted URBs of the same endpoint. TRUE if called in the DONE TD routine
*/
static void USBH_OHCI_EP0_CompleteAbortedUrbs(USBH_OHCI_EP0 * pEp0, int TDDoneFlag) {
  int completeFlag = TRUE;
  // Prevents completing of other new aborted endpoints in the completion routine
  if (NULL != pEp0->pPendingUrb) {                                      // USBH_URB is pending
    if (!TDDoneFlag) {                                               // Called from the timer abort routine
      if (USBH_OHCI_EpGlobIsTDActive(&pEp0->ItemHeader)) {                     // Nothing is done, (Ep0 uses maximal one TD per USBH_URB
        USBH_ASSERT(pEp0->TdCounter > 1);
        USBH_OHCI_EpGlobDeleteAllPendingTD(&pEp0->ItemHeader, &pEp0->TdCounter); // Delete all pending TDs of this USBH_URB
        USBH_OHCI_EpGlobClearSkip(&pEp0->ItemHeader);
        pEp0->AbortMask = 0;
        _EP0_CompleteUrb(pEp0, pEp0->pPendingUrb, USBH_STATUS_CANCELED);
      } else {                                                       // Pending USBH_URB without an TD (TD is in the cache)
        USBH_LOG((USBH_MTYPE_OHCI, "OHCI: INFO USBH_OHCI_EP0_OnAbortUrbTimer: wait for DONE!"));
        completeFlag = FALSE;
      }
    } else {                                                         // TD done
      USBH_OHCI_EpGlobClearSkip(&pEp0->ItemHeader);
      pEp0->AbortMask = 0;
      _EP0_CompleteUrb(pEp0, pEp0->pPendingUrb, USBH_STATUS_CANCELED);
    }
  } else {                                                           // Not pending USBH_URB clear ED skip bit
    USBH_OHCI_EpGlobClearSkip(&pEp0->ItemHeader);
    pEp0->AbortMask = 0;
  }
  if (completeFlag) {
    _EP0_CancelUrbList(pEp0, URB_CANCEL_PENDING_MASK);                 // Complete all URBs where URB_CANCEL_PENDING_MASK is set.
    _EP0_SubmitUrbsFromList(pEp0);
  }
}

/*********************************************************************
*
*       USBH_OHCI_EP0_OnAbortUrbTimer
*
*  Function description
*    Abort timer routine for all canceled URBs.
*    1. If no pending URb available complete other aborted URBs.
*    2. If the USBH_URB pending:
*       If the URb is not on the DONE list and not in the EDs TD list wait for DONE interrupt.
*    Others:
*      It is allowed to submit an new USBH_URB on an aborted endpoint and to abort this new submitted endpoint in the
*      context of USBH_OHCI_EP0_OnAbortUrbTimer in the URbs completion routines.
*
*/
void USBH_OHCI_EP0_OnAbortUrbTimer(void * pContext) {
  USBH_OHCI_EP0   * pEp;
  USBH_OHCI_DEVICE * pDev;
  USBH_DLIST     * pEntry;
  int         RestartFlag;
  USBH_BOOL      StartTimer = FALSE;

  USBH_ASSERT(pContext != NULL);
  pDev       = (USBH_OHCI_DEVICE *)pContext;
  USBH_OCHI_IS_DEV_VALID(pDev);
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: USBH_OHCI_EP0_OnAbortUrbTimer!"));
  if (pDev->ControlEpAbortTimerCancelFlag) {
    pDev->ControlEpAbortTimerCancelFlag = FALSE;
    return;
  }
  pDev->ControlEpAbortTimerRunFlag = FALSE;
  pEntry                           = USBH_DLIST_GetNext(&pDev->ControlEpList);
  while (pEntry != &pDev->ControlEpList) {
    pEp = GET_CONTROL_EP_FROM_ENTRY(pEntry);
    OH_EP0_VALID(pEp);
    if (pEp->AbortMask & EP_ABORT_MASK) {
      pEp->AbortMask |= EP_ABORT_SKIP_TIME_OVER_MASK | EP_ABORT_PROCESS_FLAG_MASK; // EP_ABORT_SKIP_TIME_OVER_MASK is an additional mask used in the DONE routine
    } else {
      if (pEp->AbortMask & EP_ABORT_START_TIMER_MASK) {
        pEp->AbortMask |= EP_ABORT_MASK;
        StartTimer = TRUE;
      }
    }
    pEntry = USBH_DLIST_GetNext(pEntry);
  }
  if (StartTimer) {
    // The timer run flag prevents an start of this timer if an control endpoint in the abort
    // completion routines is aborted. The timer is started at the end of this routine.
    pDev->ControlEpAbortTimerRunFlag = TRUE;
  }

  // Search all EP_ABORT_PROCESS_FLAG_MASK and processes URBs of aborted Endpoint
  RestartFlag   = TRUE;
  while (RestartFlag) {
    RestartFlag = FALSE; // if EP_ABORT_PROCESS_FLAG_MASK list processing is stopped
    pEntry        = USBH_DLIST_GetNext(&pDev->ControlEpList);
    while (pEntry != &pDev->ControlEpList) {
      pEp    = GET_CONTROL_EP_FROM_ENTRY(pEntry);
      pEntry = USBH_DLIST_GetNext(pEntry);
      OH_EP0_VALID(pEp);
      if ((pEp->AbortMask &EP_ABORT_PROCESS_FLAG_MASK)) {
        pEp->AbortMask &= ~EP_ABORT_PROCESS_FLAG_MASK;
#if (USBH_DEBUG > 1)
        if (pEp->pPendingUrb) {
          USBH_ASSERT(USBH_OHCI_EpGlobIsHalt(&pEp->ItemHeader) || USBH_OHCI_EpGlobIsSkipped(&pEp->ItemHeader));
        }
#endif
        USBH_OHCI_EP0_CompleteAbortedUrbs(pEp, FALSE); // Call completion routine
        RestartFlag = TRUE;
        break;
      }
    }
  }
  if (StartTimer) {
    pDev->ControlEpAbortTimerCancelFlag = FALSE;
    USBH_StartTimer(pDev->ControlEpAbortTimer, OH_STOP_DELAY_TIME);
  }
}

/*********************************************************************
*
*       USBH_OHCI_EP0_AbortEp
*
*  Function description
*    1. If an TD is pending (on control endpoint, only one TD per request is allowed)
*       then the Skip bit is set and an timer is scheduled
*    2. In the timer completion routine the TD is canceled.
*       The endpoint skip bit is set to zero and the list filled bit is set again!
*    3. All other URBs are complete in the right range.
*/
USBH_STATUS USBH_OHCI_EP0_AbortEp(USBH_OHCI_EP0 * Ep) {
  USBH_OHCI_DEVICE * pDev;
  USBH_STATUS        Status;

  OH_EP0_VALID(Ep);
  pDev         = Ep->pDev;
  USBH_OCHI_IS_DEV_VALID(pDev);
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: USBH_OHCI_EP0_AbortEp!"));
  Status      = USBH_STATUS_SUCCESS;
  if (Ep->AbortMask &(EP_ABORT_MASK | EP_ABORT_START_TIMER_MASK)) { // Already aborted
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: USBH_OHCI_EP0_AbortEp: Endpoint already aborted mark added URBs with the cancel pending flag!"));
    _EP0_MarkUrbsWithCancelPendingFlag(Ep);
    return USBH_STATUS_SUCCESS;
  }
  if (NULL == Ep->pPendingUrb) { // Cancel the USBH_URB list without skipping of the endpoint
    _EP0_CancelUrbList(Ep, 0);
  } else {
    // Skip the endpoint from list processing, wait a frame time until the TD can removed from the endpoint.
    // Because one timer is used for all control endpoints restart the timer in the timer routine if the timer already started.
    USBH_OHCI_EpGlobSetSkip(&Ep->ItemHeader);
    Ep->pPendingUrb->Header.HcFlags |= URB_CANCEL_PENDING_MASK;
    _EP0_MarkUrbsWithCancelPendingFlag(Ep);
    if (!pDev->ControlEpAbortTimerRunFlag) {
      pDev->ControlEpAbortTimerRunFlag     = TRUE;
      pDev->ControlEpAbortTimerCancelFlag  = FALSE;
      Ep->AbortMask                      |= EP_ABORT_MASK;
      USBH_StartTimer(pDev->ControlEpAbortTimer, OH_STOP_DELAY_TIME);
    } else {
      Ep->AbortMask |= EP_ABORT_START_TIMER_MASK;
    }
  }
  return Status;
}

/*********************************************************************
*
*       USBH_OHCI_EP0_UpdateUrbLength
*
*  Function description
*    Is called after an end of an data transfer during the data phase.
*    Is also called with the last transferred bytes after an error in the data phase!
*/
static void USBH_OHCI_EP0_UpdateUrbLength(USBH_OHCI_EP0 * ep, USBH_CONTROL_REQUEST * urbRequest, U32 transferred) {
  if (NULL != ep->pDataPhaseCopyBuffer) {
    urbRequest->Length = ep->pDataPhaseCopyBuffer->Transferred;
  } else {
    urbRequest->Length = transferred;
  }
}

/*********************************************************************
*
*       USBH_OHCI_EP0_DoneCheckForCompletion
*
*  Function description
*    Called during the second enumeration of the DONE TD list. Maximum one
*    TD is active at an time! Checks for errors and  switch to the next pSetup phase.
*/
void USBH_OHCI_EP0_DoneCheckForCompletion(USBH_OHCI_INFO_GENERAL_TRANS_DESC * Gtd) {
  USBH_STATUS            TdStatus;
  USBH_STATUS            Status;
  USBH_OHCI_EP0        * pEp0;
  USBH_CONTROL_REQUEST * pUrbRequest;
  U8                   * pSetup;
  U32                    SetupDataLength;
  U32                    Transferred;
  U8                   * pBuffer;
  U32                    NumBytesInBuffer;
  U32                    TdMask;
  USBH_OHCI_TD_PID             Pid;
  int                    CompleteFlag = FALSE;
  USBH_BOOL              ShortPkt;
  int                    InDirFlag; // True if the data direction points ot the host
  int                    OldState;
  U32                    NumBytesRem = 0;

  USBH_USE_PARA(NumBytesRem);
  pEp0 = (USBH_OHCI_EP0 *)Gtd->pEp;
  OH_EP0_VALID(pEp0);
  USBH_OCHI_IS_DEV_VALID(pEp0->pDev);
  USBH_ASSERT(pEp0->pPendingUrb != NULL);
  USBH_ASSERT(pEp0->State == OH_EP_LINK);
  // It is not allowed to delete endpoints where the USBH_URB list is not empty
  // Get TD Status and length and put the TD back to the pool
  TdStatus = USBH_OHCI_TdGetStatusAndLength(Gtd, &Transferred, &ShortPkt);
  USBH_OHCI_EpGlobDeleteDoneTD(&Gtd->ItemHeader, &pEp0->TdCounter);
  // Update Transferred data phase length and Status
  pUrbRequest  = &pEp0->pPendingUrb->Request.ControlRequest;
  pSetup       = (U8 * ) &pUrbRequest->Setup;
  InDirFlag = (pSetup[USB_SETUP_TYPE_INDEX]&USB_TO_HOST) ? TRUE : FALSE;
  OldState   = pEp0->Ep0Phase;
  if (TdStatus) { // On error
    pEp0->Ep0Phase = ES_ERROR;
  }
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: INFO USBH_OHCI_EP0_DoneCheckForCompletion: ep0-state: %s Transferred: %u pBuffer Status %s:", USBH_Ep0State2Str(pEp0->Ep0Phase), Transferred, USBH_GetStatusStr(TdStatus)));
#if (USBH_DEBUG > 1)
  if (USBH_OHCI_EpGlobIsSkipped(&pEp0->ItemHeader)) {
    USBH_LOG((USBH_MTYPE_OHCI, "OHCI: INFO USBH_OHCI_EP0_DoneCheckForCompletion: ED skipped!"));
  }
#endif
  if (USBH_OHCI_EpGlobIsHalt(&pEp0->ItemHeader)) { // On halt
    USBH_LOG((USBH_MTYPE_OHCI, "OHCI: INFO USBH_OHCI_EP0_DoneCheckForCompletion: ED halt!"));
    USBH_OHCI_EpGlobClearHalt(&pEp0->ItemHeader);  // Clear halt condition
  };
  TdMask            = 0;
  pBuffer            = NULL;
  NumBytesInBuffer     = 0;
  SetupDataLength = (U32)(pSetup[USB_SETUP_LENGTH_INDEX_MSB] << 8) + pSetup[USB_SETUP_LENGTH_INDEX_LSB];
  if (pEp0->Ep0Phase == ES_SETUP && !SetupDataLength) { // Check phases
    pEp0->Ep0Phase = ES_PROVIDE_HANDSHAKE;                // No data goto provide handshake phase
  }
  Pid = OH_IN_PID;                                      // Default value
  switch (pEp0->Ep0Phase) {
  case ES_SETUP:
    // End of pSetup and pSetup length unequal zero! Enter the data phase
    if (!USBH_IS_VALID_TRANSFER_MEM(pUrbRequest->pBuffer)) {
      // No valid transfer memory transfer data in pieces!!!
      // 1. Allocate and initialize the new transfer pBuffer on error complete the request, only the SETUP packet is Transferred!
      // 2. Get new pointer and length
      // 3. If this is an OUT packet copy data to the new transfer pBuffer
      USBH_ASSERT(NULL == pEp0->pDataPhaseCopyBuffer);
      pEp0->pDataPhaseCopyBuffer = USBH_OHCI_GetInitializedCopyTransferBuffer(&pEp0->pDev->TransferBufferPool, (U8 *)pUrbRequest->pBuffer, SetupDataLength);

      if (NULL == pEp0->pDataPhaseCopyBuffer) { // STOP and COMPLETE
        USBH_WARN((USBH_MTYPE_OHCI, "OHCI: USBH_OHCI_EP0_DoneCheckForCompletion: OhGetInitializedTransferBuffer, pSetup ends!"));
        CompleteFlag = TRUE;
        pEp0->Ep0Phase = ES_ERROR;
        break;
      } else {                               // Valid transfer pBuffer update the pBuffer
        if (!InDirFlag) {
          USBH_OHCI_FillCopyTransferBuffer(pEp0->pDataPhaseCopyBuffer);
        }
        pBuffer = USBH_OHCI_GetBufferLengthFromCopyTransferBuffer(pEp0->pDataPhaseCopyBuffer, &NumBytesInBuffer);
        pEp0->Ep0Phase = ES_COPY_DATA;         // Next state after transfer: ES_DATA
      }
    } else {                                 // Use USBH_URB pBuffer as transfer pBuffer
      pBuffer        = (U8 *)pUrbRequest->pBuffer;
      NumBytesInBuffer = SetupDataLength;
      pEp0->Ep0Phase  = ES_DATA;
    }
    TdMask = OHCI_TD_FORCE_DATA1;            // Set TD mask and PID, send the packet
    if (InDirFlag) {
      Pid     = OH_IN_PID;
      TdMask |= OHCI_TD_R;
    } else {
      Pid = OH_OUT_PID;
    }
    break;
  case ES_COPY_DATA:
    // This state is only entered if pDataPhaseCopyBuffer != NULL! If all bytes Transferred ES_COPY_DATA fall through to ES_DATA!
    USBH_ASSERT_PTR(pEp0->pDataPhaseCopyBuffer);
    if (InDirFlag) {
      NumBytesRem = USBH_OHCI_CopyToUrbBufferUpdateTransferBuffer(pEp0->pDataPhaseCopyBuffer, Transferred);
      if (NumBytesRem && !ShortPkt) {
        // Remaining length available the last received packet was not an short packet
        pBuffer  = USBH_OHCI_GetBufferLengthFromCopyTransferBuffer(pEp0->pDataPhaseCopyBuffer, &NumBytesInBuffer);
        Pid     = OH_IN_PID;
        TdMask |= OHCI_TD_R;
        break;
      }
    // Tricky: fall through if needed
    } else {
      if (USBH_OHCI_UpdateCopyTransferBuffer(pEp0->pDataPhaseCopyBuffer, Transferred)) {
        USBH_OHCI_FillCopyTransferBuffer(pEp0->pDataPhaseCopyBuffer);
        Pid    = OH_OUT_PID;
        pBuffer = USBH_OHCI_GetBufferLengthFromCopyTransferBuffer(pEp0->pDataPhaseCopyBuffer, &NumBytesInBuffer);
        break;
      }
    }
  // Tricky: fall through if needed
  case ES_DATA:
    // This state is entered if the data phase ends. If data are copied during the dataphase the state ES_COPY_DATA is the active state
    USBH_OHCI_EP0_UpdateUrbLength(pEp0, pUrbRequest, Transferred); // Update the USBH_URB pBuffer length and enter the handshake phase
  // Tricky: fall through go to the handshake phase
  case ES_PROVIDE_HANDSHAKE:
    TdMask = OHCI_TD_FORCE_DATA1;
    if (!InDirFlag || 0 == SetupDataLength) {
      Pid     = OH_IN_PID;
      TdMask |= OHCI_TD_R;
    } else {
      Pid = OH_OUT_PID;
    }
    pEp0->Ep0Phase = ES_HANDSHAKE;
    break;
  case ES_HANDSHAKE: // End of handshake phase
    CompleteFlag = TRUE;
    pEp0->Ep0Phase = ES_IDLE;
    break;
  case ES_ERROR:
    // 1. Copy data and update transfer and USBH_URB pBuffer
    // 2. Stop from sending any PID and complete the request!
    if (ES_COPY_DATA == OldState) { // Last state was data phase update buffers
      if (InDirFlag) {
        USBH_OHCI_CopyToUrbBufferUpdateTransferBuffer(pEp0->pDataPhaseCopyBuffer, Transferred);
      } else {
        USBH_OHCI_UpdateCopyTransferBuffer(pEp0->pDataPhaseCopyBuffer, Transferred);
      }
      USBH_OHCI_EP0_UpdateUrbLength(pEp0, pUrbRequest, Transferred);
    }
    if (ES_DATA == OldState) { // Last state was data phase update buffers
      USBH_OHCI_EP0_UpdateUrbLength(pEp0, pUrbRequest, Transferred);
    }
    CompleteFlag = TRUE;
    pEp0->Ep0Phase = ES_IDLE;
    break;
  case ES_IDLE:
    break;
  default:
      ;
  }
  if (pEp0->AbortMask) {
    // Endpoint is aborted, complete and resubmit newer added not aborted URBs!
    USBH_OHCI_EP0_CompleteAbortedUrbs(pEp0, TRUE);
    return;
  }
  if (CompleteFlag) {
    _EP0_CompleteUrb(pEp0, pEp0->pPendingUrb, TdStatus);
  } else {
    Status = _EP0_SubmitPacket(pEp0, pBuffer, NumBytesInBuffer, TdMask, Pid);
    if (Status != USBH_STATUS_PENDING) { // On error
      _EP0_CompleteUrb(pEp0, pEp0->pPendingUrb, Status);
    }
  }
  _EP0_SubmitUrbsFromList(pEp0);
}

/*********************************************************************
*
*       USBH_OHCI_Ep0ServiceAllEndpoints
*
*  Function description
*/
/*
static void USBH_OHCI_Ep0ServiceAllEndpoints(HC_DEVICE*   pDev) {
  USBH_OHCI_EP0 * pEp0;
  PDLIST    entry;
  //
  // 1. remove all Eps from ControlEpList and put it to removeList
  //
  USBH_OCHI_IS_DEV_VALID(pDev);
  entry = USBH_DLIST_GetPrev(&pDev->ControlEpList);
  while (entry != &pDev->ControlEpList)  {
    pEp0    = GET_CONTROL_EP_FROM_ENTRY(entry);
    OH_EP0_VALID           (pEp0);
    entry = USBH_DLIST_GetPrev(entry);
    _EP0_SubmitUrbsFromList(pEp0);
  }
}
*/

/******************************* EOF ********************************/
