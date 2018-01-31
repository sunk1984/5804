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

const char * EnumEp0StateStr(Ep0_Phase x);
const char * EnumEp0StateStr(Ep0_Phase x) {
  return (
    TB_ENUMSTR(ES_IDLE) :
    TB_ENUMSTR(ES_SETUP) :
    TB_ENUMSTR(ES_COPY_DATA) :
    TB_ENUMSTR(ES_DATA) :
    TB_ENUMSTR(ES_PROVIDE_HANDSHAKE) :
    TB_ENUMSTR(ES_HANDSHAKE) :
    TB_ENUMSTR(ES_ERROR) :
      "unknown enum state!"
    );
}

/*********************************************************************
*
*       TConvSetupPacketToBuffer
*
*  Function description
*    Converts the struct USBH_SETUP_PACKET to a byte buffer.
*    IN: Pointer to a empty struct - OUT: Setup
*    points to a byte buffer with a length of 8 bytes
*/
static void TConvSetupPacketToBuffer(const USBH_SETUP_PACKET * Setup, U8 Buffer[8]) {
  U8     * ptr;
  ptr    = Buffer;
  *ptr++ = Setup->Type;
  *ptr++ = Setup->Request;
  *ptr++ = (U8) Setup->Value;        //LSB
  *ptr++ = (U8)(Setup->Value  >> 8); //MSB
  *ptr++ = (U8) Setup->Index;        //LSB
  *ptr++ = (U8)(Setup->Index  >> 8); //MSB
  *ptr++ = (U8) Setup->Length;       //LSB
  *ptr++ = (U8)(Setup->Length >> 8); //MSB
}

/*********************************************************************
*
*       OhEp0CompleteUrb
*
*  Function description
*    Condition code of the transfer or an driver status
*/
static void OhEp0CompleteUrb(OHD_EP0 * Ep, URB * Urb, USBH_STATUS Status) {
  OH_EP0_VALID(Ep);
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: INFO OhEp0CompleteUrb: URB status: %08x!", Status));
  if (Urb != NULL) {             // valid URB
    if (NULL != Ep->DataPhaseCopyBuffer) {
      Urb->Request.ControlRequest.Length = Ep->DataPhaseCopyBuffer->Transferred;
      HcmPutItem(&Ep->DataPhaseCopyBuffer->ItemHeader);
      Ep->DataPhaseCopyBuffer = NULL;
    }
    Urb->Header.Status = Status;
    if (Urb == Ep->PendingUrb) { // If an URB is canceled an this is the pending URB
#if (USBH_DEBUG > 1)
      if (Ep->TdCounter > 1) {   // Active Transfer descriptors, this excludes the default TD
        USBH_WARN((USBH_MTYPE_OHCI, "OHCI: OhEp0CompleteUrb: complete the current URB and more than the default TD on this EP are not released!"));
      }
#endif

      Ep->PendingUrb = NULL;
    }
    T_ASSERT(Ep->UrbCount);
    Ep->UrbCount--;
    Urb->Header.InternalCompletion(Urb); // Call the completion routine
  }
}

/*********************************************************************
*
*       OhEp0CancelUrbList
*
*  Function description
*
*  Parameters:
*    AbortFlag: TRUE:  Only aborted URBs are completed.
*               FALSE: All URBs are completed.
*/
static void OhEp0CancelUrbList(OHD_EP0 * Ep, U32 HcFlagMask) {
  PDLIST   listHead, entry, abortEntry;
  DLIST    abortList;
  URB    * urb;
  OH_EP0_VALID(Ep);
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: OhEp0CancelUrbList!"));
  DlistInit(&abortList);
  listHead = &Ep->UrbList;
  entry    = DlistGetNext(listHead);

  while (entry != listHead) {
    urb        = (URB *)GET_URB_HEADER_FROM_ENTRY(entry);
    abortEntry = entry;
    entry      = DlistGetNext(entry);
    if (0 == HcFlagMask || (HcFlagMask &urb->Header.HcFlags)) { // If parameter HcFlagMask is set then remove only URBs where the URB flag is set
      DlistRemoveEntry(abortEntry);
      DlistInsertHead(&abortList, abortEntry)
    }
  }
  while (!DlistEmpty(&abortList)) {
    DlistRemoveTail(&abortList, &entry);
    urb = (URB * )GET_URB_HEADER_FROM_ENTRY(entry);
    OhEp0CompleteUrb(Ep, urb, USBH_STATUS_CANCELED);
  }
}

/*********************************************************************
*
*       OhEp0Unlink
*
*  Function description
*    The entry is removed only from the devices control endpoint list,
*    not from the remove pending list!
*/
static void OhEp0Unlink(OHD_EP0 * Ep) {
  HC_DEVICE * dev;
  PDLIST      previousEntry;
  OHD_EP0   * previousEp;
  U32         nextPhyAddr;
  OH_EP0_VALID(Ep);
  dev = Ep->Dev;
  OH_DEV_VALID(dev);
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: OhEp0Unlink Dev.Addr: %u!", Ep->DeviceAddress));
  T_ASSERT(dev->ControlEpCount);
  T_ASSERT(Ep->PendingUrb == NULL);
  // HC list must be disabled
  T_ASSERT(0 == OhHalTestReg(dev->RegBase, OH_REG_CONTROL, HC_CONTROL_CLE));
  T_ASSERT(!DlistEmpty(&dev->ControlEpList));
  previousEntry = DlistGetPrev(&Ep->ListEntry); // Get the list tail
  if (previousEntry == &dev->ControlEpList) {
    USBH_LOG((USBH_MTYPE_OHCI, "OHCI: INFO OhEp0Unlink: remove the first ED in the  ED list!"));
    nextPhyAddr = OhEpGlobUnlinkEd(NULL, &Ep->ItemHeader);
    OhHalWriteReg(dev->RegBase, OH_REG_CONTROLHEADED, nextPhyAddr);
  } else {
    previousEp = GET_CONTROL_EP_FROM_ENTRY(previousEntry);
    OH_EP0_VALID(previousEp);
    OhEpGlobUnlinkEd(&previousEp->ItemHeader, &Ep->ItemHeader);
  }
  // Remove ep from logical list and set endpoint in Idle state
  DlistRemoveEntry(&Ep->ListEntry);
  dev->ControlEpCount--;
  Ep->State = OH_EP_IDLE;
  if (0 == dev->ControlEpCount) {
    USBH_LOG((USBH_MTYPE_OHCI, "OHCI: INFO OhEp0Unlink: ED list now empty!"));
  }
}

/*********************************************************************
*
*       OhEp0MarkUrbsWithCancelPendingFlag
*
*  Function description
*/
static void OhEp0MarkUrbsWithCancelPendingFlag(OHD_EP0 * Ep) {
  PDLIST   listHead, entry;
  URB    * urb;
  OH_EP0_VALID(Ep);
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: OhEp0UrbListSetAbortFlag!"));
  listHead = &Ep->UrbList;
  entry    = DlistGetNext(listHead);
  while (entry != listHead) {
    urb = (URB *)GET_URB_HEADER_FROM_ENTRY(entry); // The URB header is the first element in the URB, cast it
#if (USBH_DEBUG > 1)
    if (urb->Header.HcFlags & URB_CANCEL_PENDING_MASK) {
      USBH_WARN((USBH_MTYPE_OHCI, "OHCI: OhEp0UrbListSetAbortFlag: URB_CANCEL_PENDING_MASK already set!"));
    }
#endif
    urb->Header.HcFlags |= URB_CANCEL_PENDING_MASK;
    entry                = DlistGetNext(entry);
  }
}

/*********************************************************************
*
*       OhEp0SubmitPacket
*
*  Return value
*    USBH_STATUS_PENDING on success.
*    other values: errors
*/
static USBH_STATUS OhEp0SubmitPacket(OHD_EP0 * Ep, U8 * Buffer, U32 Length, U32 TDword0Mask, OHD_TD_PID Pid) {
  USBH_STATUS   status;
  OHD_GTD     * td;
  U32           startAddr, endAddr;
  OH_EP0_VALID(Ep);
  T_ASSERT(0 == Ep->AbortMask);
  status = USBH_STATUS_SUCCESS;
#if (USBH_DEBUG > 1)
  if (Length) {
    T_ASSERT_PTR(Buffer);
  }
#endif
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: OhEp0SubmitPacket Length: %lu!", Length));
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: Start transfer: Length: %lu ",   Length));
  for (; ;) {
    // Get the not used TD
    td = OhEpGlobGetLastTDFromED(&Ep->ItemHeader);
    if (td == NULL) {
      USBH_WARN((USBH_MTYPE_OHCI, "OHCI: OhEp0SubmitPacket: OhEpGlobGetLastTDFromED!"));
      status = USBH_STATUS_ERROR;
      break;
    }
    // Get phys. addresses for the control endpoint
    startAddr = (U32)Buffer;
    endAddr   = startAddr + Length - 1;
    // Init the not used TD in ED
    USBH_LOG((USBH_MTYPE_OHCI, "OHCI: OhTdInit: ep: 0x%x ", Ep->EndpointAddress));
    OhTdInit(td, Ep, USB_EP_TYPE_CONTROL, Pid, startAddr, endAddr, TDword0Mask);
    // Get an new TD
    td = OhTdGet(&Ep->Dev->GTDPool);
    if (td == NULL) {
      USBH_WARN((USBH_MTYPE_OHCI, "OHCI: OhEp0DoneCheckForCompletion: OhTdGet!"));
      status = USBH_STATUS_MEMORY;
      break;
    }
    // Add the new TD to the TD list
    OhEpGlobInsertTD(&Ep->ItemHeader, &td->ItemHeader, &Ep->TdCounter);
    // Set always the control list filled bit, this ensure that the OHCI scans the control endpoint list again!
    if (!Ep->Dev->ControlEpRemoveTimerRunFlag) {
      // Update the list control register
      OhdEndpointListEnable(Ep->Dev, USB_EP_TYPE_CONTROL, TRUE, TRUE);
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
*       OhEp0SubmitUrb
*
*  Function description
*    OhEp0SubmitUrb submits always Ep->PendingUrb, starts with the SETUP phase
*
*  Parameters
*    Ep valid pointer to an endpoint!
*
*  Returns:
*    URB_STATUS_PENDING: URB successfully submitted.
*    other errors:       Error on submitting the URB.
*/
static USBH_STATUS OhEp0SubmitUrb(OHD_EP0 * Ep, URB * urb) {
  USBH_STATUS            status;
  USBH_CONTROL_REQUEST * urbRequest;
  OH_EP0_VALID(Ep);
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: OhEp0SubmitUrb!"));
  // The PendingURB is submitted
  T_ASSERT(urb != NULL);
  T_ASSERT(Ep->PendingUrb == NULL);
  T_ASSERT(0 == (urb->Header.HcFlags &URB_CANCEL_PENDING_MASK));
  // Set the control endpoint phase
  Ep->Ep0Phase = ES_SETUP;
  urbRequest   = &urb->Request.ControlRequest;
  TConvSetupPacketToBuffer(&urbRequest->Setup, Ep->Setup);
  status       = OhEp0SubmitPacket(Ep, Ep->Setup, USB_SETUP_PACKET_LEN, OHCI_TD_FORCE_DATA0, OH_SETUP_PID);
  if (status != USBH_STATUS_PENDING) {
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: OhEp0SubmitUrb!"));
  } else { // On success
    Ep->PendingUrb = urb;
  }
  return status;
}

/*********************************************************************
*
*       OhEp0SubmitUrbsFromList
*
*  Function description
*/
static void OhEp0SubmitUrbsFromList(OHD_EP0 * Ep) {
  URB         * urb;
  USBH_STATUS   status;
  PDLIST        entry;
  int           emptyFlag;
  OH_EP0_VALID(Ep);
  if (Ep->State != OH_EP_LINK) {
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: OhEp0SubmitUrbsFromList: Ep0 on dev.address %d not in LINK state!", (int)Ep->DeviceAddress));
    return;
  }
  if (Ep->AbortMask != 0) {
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: OhEp0SubmitUrbsFromList: Ep0 on dev.address %d is aborted!", (int)Ep->DeviceAddress));
    return;
  }
  if (NULL != Ep->PendingUrb) {
    return;
  }
  emptyFlag = DlistEmpty(&Ep->UrbList);
  while (!emptyFlag) {
    // Submit the next URB
    DlistRemoveHead(&Ep->UrbList, &entry);
    emptyFlag = DlistEmpty(&Ep->UrbList);
    urb       = (URB *)GET_URB_HEADER_FROM_ENTRY(entry);
    status    = OhEp0SubmitUrb(Ep, urb);
    if (status == USBH_STATUS_PENDING) { // On success stop
      break;
    } else { // On error
      if (Ep->UrbCount == 1) {
        OhEp0CompleteUrb(Ep, Ep->PendingUrb, status); // Endpoint can be invalid
        break;
      } else {
        OhEp0CompleteUrb(Ep, Ep->PendingUrb, status);
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
*       OhEp0RemoveEndpoints
*
*  Function description
*    AllEndpointFlag - all endpoints are removed regardless of the
*    endpoint state, no need to check the state
*/
void OhEp0RemoveEndpoints(HC_DEVICE * dev, T_BOOL AllEndpointFlag) {
  OHD_EP0                         * ep;
  PDLIST                            entry;
  DLIST                             removeList;
  USBH_RELEASE_EP_COMPLETION_FUNC * pfCompletion;
  void                            * pContext;
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: OhEp0RemoveEndpoints!"));
  OH_DEV_VALID(dev);

#if (USBH_DEBUG > 1)
  if (OhHalTestReg(dev->RegBase, OH_REG_CONTROL, HC_CONTROL_CLE)) {
    USBH_PANIC("FATAL OhEp0RemoveEndpoints: control endpoint list is not disabled!");
  }
#endif
  DlistInit(&removeList);
  // Build the removeList
  entry = DlistGetPrev(&dev->ControlEpList);
  while (entry != &dev->ControlEpList) {
    ep    = GET_CONTROL_EP_FROM_ENTRY(entry);
    OH_EP0_VALID(ep);
    entry = DlistGetPrev(entry);
    if (AllEndpointFlag) {
      OhEp0Unlink(ep);
      DlistInsertHead(&removeList, &ep->ListEntry);
    } else {
      if (ep->State == OH_EP_UNLINK) {
        OhEp0Unlink(ep);
        DlistInsertHead(&removeList, &ep->ListEntry);
      }
    }
  }
  // Enable the endpoint list processing
  if (dev->ControlEpCount > 1) {
    // More than dummy endpoints available
    OhdEndpointListEnable(dev, USB_EP_TYPE_CONTROL, TRUE, TRUE);
  }
#if (USBH_DEBUG > 1)
  if (dev->ControlEpCount == 0) {
    USBH_LOG((USBH_MTYPE_OHCI, "OHCI: INFO OhEp0RemoveEndpoints: empty endpoint list !"));
  }
#endif
  while (!DlistEmpty(&removeList)) { // Delete the endpoint, call the completion routine
    DlistRemoveHead(&removeList, &entry);
    ep           = GET_CONTROL_EP_FROM_ENTRY(entry);
    OH_EP0_VALID(ep);
    pfCompletion = ep->ReleaseCompletion;
    pContext     = ep->ReleaseContext;
    OhEp0Put    (ep);
    if (pfCompletion) {
      pfCompletion(pContext); // Call the completion routine, attention: runs in this context
    }
  }
}

/*********************************************************************
*
*       OhEp0ReleaseEp_TimerCallback
*
*  Function description
*    Called if one or more endpoints has been removed!
*
*  Parameters:
*    Context: Dev Ptr.
*/
void OhEp0ReleaseEp_TimerCallback(void * Context) {
  HC_DEVICE * dev;
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: OhEp0ReleaseEp_TimerCallback!"));
  T_ASSERT(Context != NULL);
  dev = (HC_DEVICE *)Context;
  OH_DEV_VALID(dev);
  if (dev->ControlEpRemoveTimerCancelFlag) {
    dev->ControlEpRemoveTimerCancelFlag = FALSE;
    return;
  }
  dev->ControlEpRemoveTimerRunFlag = FALSE; // Timer is stopped
  OhEp0RemoveEndpoints(dev, FALSE);         // FALSE: release only unlinked state endpoints
}

/*********************************************************************
*
*       OhEp0Alloc
*
*  Function description
*/
USBH_STATUS OhEp0Alloc(HCM_POOL * EpPool, HCM_POOL * SetupPacketPool, U32 Numbers) {
  USBH_STATUS status;
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: OhEp0Alloc!"));
  status = HcmAllocPool(EpPool, Numbers, OH_ED_SIZE, sizeof(OHD_EP0), OH_ED_ALIGNMENT);
  if (status) {          // On error
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: OhEp0Alloc: HcmAllocPool!"));
  }
  status = HcmAllocPool(SetupPacketPool, Numbers, USB_SETUP_PACKET_LEN, sizeof(SETUP_BUFFER), USBH_TRANSFER_BUFFER_ALIGNMENT);
  if (status) {          // On error
    HcmFreePool(EpPool); // Frees the control endpoint pool
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: OhEp0Alloc: Setup packets!"));
  }
  return status;
}

/*********************************************************************
*
*       OhEp0Get
*
*  Function description
*/
OHD_EP0 * OhEp0Get(HCM_POOL * EpPool, HCM_POOL * SetupPacketPool) {
  OHD_EP0      * item;
  SETUP_BUFFER * setup_packet;
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: OhEp0Get!"));
  item = (OHD_EP0 * )HcmGetItem(EpPool);
  if (NULL == item) {
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: OhEp0Get: no resources!"));
  } else {                           // Init the item link list for TDs
    DlistInit(&item->ItemHeader.Link.ListEntry);
    setup_packet = (SETUP_BUFFER *)HcmGetItem(SetupPacketPool);
    if (NULL == setup_packet) {
      USBH_WARN((USBH_MTYPE_OHCI, "OHCI: OhEp0Get: no setup packet resources!"));
      HcmPutItem(&item->ItemHeader); // Release also the ED memory
      item = NULL;
    } else {                         // On success
      item->SetupPacket = setup_packet;
      item->Setup       = setup_packet->ItemHeader.VirtAddr;
    }
    T_ASSERT(TB_IS_ALIGNED(item->ItemHeader.PhyAddr, OH_ED_ALIGNMENT));
  }
  return item;
}

/*********************************************************************
*
*       OhEp0Put
*
*  Function description
*/
void OhEp0Put(OHD_EP0 * Ep) {
  OhEpGlobRemoveAllTDtoPool(&Ep->ItemHeader, &Ep->TdCounter); // Put all TD items back to the TD pool before put back the Ep0 object
  if (Ep->Setup != NULL) {
    HcmPutItem(&Ep->SetupPacket->ItemHeader);
    Ep->Setup = NULL;
  }
  if (NULL != Ep->DataPhaseCopyBuffer) {
    HcmPutItem(&Ep->DataPhaseCopyBuffer->ItemHeader);
    Ep->DataPhaseCopyBuffer = NULL;
  }
  HcmPutItem(&Ep->ItemHeader);
}

/*********************************************************************
*
*       OhEp0Init
*
*  Function description
*    Initializes the control endpoint object, allocates an empty transfer
*    descriptor and link it to the endpoint.
*  Return value:
*    Pointer to the endpoint object
*    == NULL: Error during initialization, the endpoint is not deallocated!
*/
USBH_STATUS OhEp0Init(OHD_EP0 * Ep, HC_DEVICE * Dev, U32 Mask, U8 DeviceAddress, U8 EndpointAddress, U16 MaxFifoSize, USBH_SPEED Speed) {
  USBH_STATUS       status;
  HCM_ITEM_HEADER * item;
  OHD_GTD         * td;
  T_BOOL            SkipFlag;
  OH_DEV_VALID(Dev);
  // Do not clear the Ep0 struct because it contains a valid item header and a valid Setup packet pointer
  OH_EP0_VALID(Ep);
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: OhEp0Init!"));
  status                  = USBH_STATUS_SUCCESS;
  Ep->DataPhaseCopyBuffer = NULL;
  Ep->EndpointType        = USB_EP_TYPE_CONTROL;
  Ep->Dev                 = Dev;        // Backward pointer to the device
  Ep->State               = OH_EP_IDLE; // Endpoint is not linked
  DlistInit(&Ep->ListEntry);
  DlistInit(&Ep->UrbList);
  Ep->UrbCount            = 0;
  Ep->PendingUrb          = NULL;
  Ep->TdCounter           = 0;
  Ep->AbortMask           = 0;

  Ep->Ep0Phase            = ES_IDLE;    // No request
  Ep->ReleaseCompletion   = NULL;       // Parameter
  Ep->Mask                = Mask;
  Ep->DeviceAddress       = DeviceAddress;
  Ep->EndpointAddress     = EndpointAddress;
  Ep->MaxPktSize          = MaxFifoSize;
  Ep->Speed               = Speed;

  if (Mask & OH_DUMMY_ED_EPFLAG) {
    SkipFlag = TRUE;
  } else {
    SkipFlag = FALSE;
  }
  item = &Ep->ItemHeader;
  OhEpGlobInitED(item, DeviceAddress, EndpointAddress, MaxFifoSize, FALSE, SkipFlag, Speed); // Init DWORD 0..DWORD 3 in the ED
  td = OhTdGet(&Dev->GTDPool);                                                               // Get a TD item from pool
  if (td == NULL) {
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: OhEp0Init: OhTdGet!"));
    status = USBH_STATUS_MEMORY;
    goto exit;
  }
  // The added TD must not be initialized because Head=TailPointer=address of first added TD
  // Link the new TD to the EP TD list!
  OhEpGlobInsertTD(item, &td->ItemHeader, &Ep->TdCounter);
  exit:
  return status; // On error an NULL pointer is returned
}

/*********************************************************************
*
*       OhEp0AddUrb
*
*  Function description
*    Adds a control endpoint request.
*
*  Return value:
*    USBH_STATUS_PENDING on success
*    other values are errors
*/
USBH_STATUS OhEp0AddUrb(OHD_EP0 * Ep, URB * Urb) {
  USBH_CONTROL_REQUEST * urbRequest;
  USBH_STATUS            status;
  OH_EP0_VALID(Ep);
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: OhEp0AddUrb!"));
  /* dev checks the endpoint object */
  OH_DEV_VALID(Ep->Dev);
  T_ASSERT(Urb != NULL);
  urbRequest         = &Urb->Request.ControlRequest;
  urbRequest->Length = 0;
  if (Ep->State != OH_EP_LINK) {
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: OhEp0AddUrb: State != OH_EP_LINK, URB is canceled!"));
    Urb->Header.Status = USBH_STATUS_CANCELED;
    return USBH_STATUS_CANCELED;
  }
  if (Ep->UrbCount == 0 && Ep->AbortMask == 0) {
    // If no pending URB is available the URBlist can contain aborted
    // URBs and this URB is submitted during in the completion routine
    Ep->UrbCount++;
    status = OhEp0SubmitUrb(Ep, Urb);
    if (status != USBH_STATUS_PENDING) { // On error
      USBH_WARN((USBH_MTYPE_OHCI, "OHCI: OhEp0AddUrb: OhEp0SubmitUrb: %08x!", status));
      Ep->UrbCount--;
    }
    return status;
  } else { // URB is pending, add it to the URB list and return pending
    Ep->UrbCount++;
    DlistInsertTail(&Ep->UrbList, &Urb->Header.ListEntry);
    status = USBH_STATUS_PENDING;
  }
  return status;
}

/*********************************************************************
*
*       OhEp0Insert
*
*  Function description
*    Adds an endpoint to the devices control endpoint list. The endpoint
*    must be initialized with one TD! First the next pointer is set and
*    then the next pointer of the previous endpoint!
*/
void OhEp0Insert(OHD_EP0 * Ep) {
  HC_DEVICE * dev;
  U8        * base;
  OHD_EP0   * tailEp;
  PDLIST      entry;
  OH_EP0_VALID(Ep);
  dev       = Ep->Dev;
  T_ASSERT(Ep != NULL);
  OH_DEV_VALID(dev);
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: OhEp0Insert!"));
  base      = Ep->Dev->RegBase;
  if (0 == Ep->Dev->ControlEpCount) { // Empty control ED list 
    OhHalWriteReg(base, OH_REG_CONTROLHEADED, Ep->ItemHeader.PhyAddr);
  } else {
    T_ASSERT(!DlistEmpty  (&dev->ControlEpList));
    entry  =  DlistGetPrev(&dev->ControlEpList);
    tailEp = GET_CONTROL_EP_FROM_ENTRY(entry);
    OH_EP0_VALID    (tailEp);
    OhEpGlobLinkEds(&tailEp->ItemHeader, &Ep->ItemHeader);
  }
  Ep->State = OH_EP_LINK;
  // Logical link
  DlistInsertTail(&dev->ControlEpList, &Ep->ListEntry);
  dev->ControlEpCount++;
}

/*********************************************************************
*
*       OhEp0_ReleaseEndpoint
*
*  Function description
*    Releases an endpoint
*/
void OhEp0_ReleaseEndpoint(OHD_EP0 * Ep, USBH_RELEASE_EP_COMPLETION_FUNC * pfReleaseEpCompletion, void * pContext) {
  HC_DEVICE * dev;
  OH_EP0_VALID(Ep);
  dev = Ep->Dev;
  OH_DEV_VALID(dev);
  T_ASSERT(DlistEmpty(&Ep->UrbList));
  T_ASSERT(Ep->PendingUrb == NULL);
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: OhEp0_ReleaseEndpoint!"));
  if (Ep->State == OH_EP_UNLINK) {
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: OhEp0_ReleaseEndpoint: Endpoint already unlinked, return!"));
    return;
  }
  Ep->ReleaseContext    = pContext;
  Ep->ReleaseCompletion = pfReleaseEpCompletion;
  Ep->State             = OH_EP_UNLINK;
  OhEpGlobSetSkip(&Ep->ItemHeader); // Additional set the skip bit
  if (!dev->ControlEpRemoveTimerRunFlag) {
    // If this is the first endpoint that must be deleted in the control endpoint list of the HC
    OhdEndpointListEnable(dev, USB_EP_TYPE_CONTROL, FALSE, FALSE);
    dev->ControlEpRemoveTimerRunFlag    = TRUE;
    dev->ControlEpRemoveTimerCancelFlag = FALSE;
    USBH_StartTimer(dev->ControlEpRemoveTimer, OH_STOP_DELAY_TIME);
  }
}

/*********************************************************************
*
*       OhEp0CompleteAbortedUrbs
*
*  Function description
*    Completes all aborted URbs clears the endpoint abort mask and resubmit new
*    not aborted URBs of the same endpoint. TRUE if called in the DONE TD routine
*/
static void OhEp0CompleteAbortedUrbs(OHD_EP0 * Ep, int TDDoneFlag) {
  int completeFlag = TRUE;
  // Prevents completing of other new aborted eendpoints in the completion routine
  if (NULL != Ep->PendingUrb) {                                      // URB is pending
    if (!TDDoneFlag) {                                               // Called from the timer abort routine
      if (OhEpGlobIsTDActive(&Ep->ItemHeader)) {                     // Nothing is done, (Ep0 uses maximal one TD per URB
        T_ASSERT(Ep->TdCounter > 1);
        OhEpGlobDeleteAllPendingTD(&Ep->ItemHeader, &Ep->TdCounter); // Delete all pending TDs of this URB
        OhEpGlobClearSkip(&Ep->ItemHeader);
        Ep->AbortMask = 0;
        OhEp0CompleteUrb(Ep, Ep->PendingUrb, USBH_STATUS_CANCELED);
      } else {                                                       // Pending URB without an TD (TD is in the cache)
        USBH_LOG((USBH_MTYPE_OHCI, "OHCI: INFO OhEp0AbortUrb_TimerCallback: wait for DONE!"));
        completeFlag = FALSE;
      }
    } else {                                                         // TD done
      OhEpGlobClearSkip(&Ep->ItemHeader);
      Ep->AbortMask = 0;
      OhEp0CompleteUrb(Ep, Ep->PendingUrb, USBH_STATUS_CANCELED);
    }
  } else {                                                           // Not pending URB clear ED skip bit
    OhEpGlobClearSkip(&Ep->ItemHeader);
    Ep->AbortMask = 0;
  }
  if (completeFlag) {
    OhEp0CancelUrbList(Ep, URB_CANCEL_PENDING_MASK);                 // Complete all URBs where URB_CANCEL_PENDING_MASK is set.
    OhEp0SubmitUrbsFromList(Ep);
  }
}

/*********************************************************************
*
*       OhEp0AbortUrb_TimerCallback
*
*  Function description
*/
void OhEp0AbortUrb_TimerCallback(void * Context) {
  OHD_EP0   * ep;
  HC_DEVICE * dev;
  PDLIST      entry;
  int         restart_flag;
  T_BOOL      start_timer = FALSE;
  T_ASSERT(Context != NULL);
  dev       = (HC_DEVICE *)Context;
  OH_DEV_VALID(dev);
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: OhEp0AbortUrb_TimerCallback!"));
  if (dev->ControlEpAbortTimerCancelFlag) {
    dev->ControlEpAbortTimerCancelFlag = FALSE;
    return;
  }
  dev->ControlEpAbortTimerRunFlag = FALSE;
  entry                           = DlistGetNext(&dev->ControlEpList);
  while (entry != &dev->ControlEpList) {
    ep = GET_CONTROL_EP_FROM_ENTRY(entry);
    OH_EP0_VALID(ep);
    if (ep->AbortMask & EP_ABORT_MASK) {
      ep->AbortMask |= EP_ABORT_SKIP_TIME_OVER_MASK | EP_ABORT_PROCESS_FLAG_MASK; // EP_ABORT_SKIP_TIME_OVER_MASK is an additional mask used in the DONE routine
    } else {
      if (ep->AbortMask & EP_ABORT_START_TIMER_MASK) {
        ep->AbortMask |= EP_ABORT_MASK;
        start_timer = TRUE;
      }
    }
    entry = DlistGetNext(entry);
  }
  if (start_timer) {
    // The timer run flag prevents an start of this timer if an control endpoint in the abort
    // completion routines is aborted. The timer is started at the end of this routine.
    dev->ControlEpAbortTimerRunFlag = TRUE;
  }

  // Search all EP_ABORT_PROCESS_FLAG_MASK and processes URBs of aborted Endpoint
  restart_flag   = TRUE;
  while (restart_flag) {
    restart_flag = FALSE; // if EP_ABORT_PROCESS_FLAG_MASK list processing is stopped
    entry        = DlistGetNext(&dev->ControlEpList);
    while (entry != &dev->ControlEpList) {
      ep    = GET_CONTROL_EP_FROM_ENTRY(entry);
      entry = DlistGetNext(entry);
      OH_EP0_VALID(ep);
      if ((ep->AbortMask &EP_ABORT_PROCESS_FLAG_MASK)) {
        ep->AbortMask &= ~EP_ABORT_PROCESS_FLAG_MASK;
#if (USBH_DEBUG > 1)
        if (ep->PendingUrb) {
          T_ASSERT(OhEpGlobIsHalt(&ep->ItemHeader) || OhEpGlobIsSkipped(&ep->ItemHeader));
        }
#endif
        OhEp0CompleteAbortedUrbs(ep, FALSE); // Call completion routine
        restart_flag = TRUE;
        break;
      }
    }
  }
  if (start_timer) {
    dev->ControlEpAbortTimerCancelFlag = FALSE;
    USBH_StartTimer(dev->ControlEpAbortTimer, OH_STOP_DELAY_TIME);
  }
}

/*********************************************************************
*
*       OhEp0AbortEndpoint
*
*  Function description
*    1. If an TD is pending (on control endpoint, only one TD per request is allowed)
*       then the Skip bit is set and an timer is scheduled
*    2. In the timer completion routine the TD is canceled.
*       The endpoint skip bit is set to zero and the list filled bit is set again!
*    3. All other URBs are complete in the right range.
*/
USBH_STATUS OhEp0AbortEndpoint(OHD_EP0 * Ep) {
  HC_DEVICE   * dev;
  USBH_STATUS   status;
  OH_EP0_VALID(Ep);
  dev         = Ep->Dev;
  OH_DEV_VALID(dev);
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: OhEp0AbortEndpoint!"));
  status      = USBH_STATUS_SUCCESS;
  if (Ep->AbortMask &(EP_ABORT_MASK | EP_ABORT_START_TIMER_MASK)) { // Already aborted
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: OhEp0AbortEndpoint: Endpoint already aborted mark added URBs with the cancel pending flag!"));
    OhEp0MarkUrbsWithCancelPendingFlag(Ep);
    return USBH_STATUS_SUCCESS;
  }
  if (NULL == Ep->PendingUrb) { // Cancel the URB list without skipping of the endpoint
    OhEp0CancelUrbList(Ep, 0);
  } else {
    // Skip the endpoint from list processing, wait a frame time until the TD can removed from the endpoint.
    // Because one timer is used for all control endpoints restart the timer in the timer routine if the timer already started.
    OhEpGlobSetSkip(&Ep->ItemHeader);
    Ep->PendingUrb->Header.HcFlags |= URB_CANCEL_PENDING_MASK;
    OhEp0MarkUrbsWithCancelPendingFlag(Ep);
    if (!dev->ControlEpAbortTimerRunFlag) {
      dev->ControlEpAbortTimerRunFlag     = TRUE;
      dev->ControlEpAbortTimerCancelFlag  = FALSE;
      Ep->AbortMask                      |= EP_ABORT_MASK;
      USBH_StartTimer(dev->ControlEpAbortTimer, OH_STOP_DELAY_TIME);
    } else {
      Ep->AbortMask |= EP_ABORT_START_TIMER_MASK;
    }
  }
  return status;
}

/*********************************************************************
*
*       OhEp0UpdateUrbLength
*
*  Function description
*    Is called after an end of an data transfer during the data phase.
*    Is also called with the last transferred bytes after an error in the data phase!
*/
static void OhEp0UpdateUrbLength(OHD_EP0 * ep, USBH_CONTROL_REQUEST * urbRequest, U32 transferred) {
  if (NULL != ep->DataPhaseCopyBuffer) {
    urbRequest->Length = ep->DataPhaseCopyBuffer->Transferred;
  } else {
    urbRequest->Length = transferred;
  }
}

/*********************************************************************
*
*       OhEp0UpdateUrbLength
*
*  Function description
*    Called during the second enumeration of the DONE TD list. Maximum one
*    TD is active at an time! Checks for errors and  switch to the next setup phase.
*/
void OhEp0DoneCheckForCompletion(OHD_GTD * Gtd) {
  USBH_STATUS            tdStatus;
  USBH_STATUS            status;
  OHD_EP0              * ep;
  USBH_CONTROL_REQUEST * urbRequest;
  U8                   * setup;
  U32                    setup_data_length;
  U32                    transferred;
  U8                   * buffer;
  U32                    buffer_length;
  U32                    tdMask;
  OHD_TD_PID             pid;
  int                    completeFlag = FALSE;
  T_BOOL                 shortPkt;
  int                    in_dir_flag; // True if the data direction points ot the host
  int                    old_state;
  U32                    remaining_length = 0;
  USBH_USE_PARA(remaining_length);
  ep = (OHD_EP0 * )Gtd->Ep;
  OH_EP0_VALID(ep);
  OH_DEV_VALID(ep->Dev);
  T_ASSERT(ep->PendingUrb != NULL);
  T_ASSERT(ep->State == OH_EP_LINK);
  // It is not allowed to delete endpoints where the URB list is not empty
  // Get TD status and length and put the TD back to the pool
  tdStatus = OhTdGetStatusAndLength(Gtd, &transferred, &shortPkt);
  OhEpGlobDeleteDoneTD(&Gtd->ItemHeader, &ep->TdCounter);
  // Update transferred data phase length and status
  urbRequest  = &ep->PendingUrb->Request.ControlRequest;
  setup       = (U8 * ) &urbRequest->Setup;
  in_dir_flag = (setup[USB_SETUP_TYPE_INDEX]&USB_TO_HOST) ? TRUE : FALSE;
  old_state   = ep->Ep0Phase;
  if (tdStatus) { // On error
    ep->Ep0Phase = ES_ERROR;
  }
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: INFO OhEp0DoneCheckForCompletion: ep0-state: %s transferred: %u buffer status %s:", EnumEp0StateStr(ep->Ep0Phase), transferred, USBH_GetStatusStr(tdStatus)));
#if (USBH_DEBUG > 1)
  if (OhEpGlobIsSkipped(&ep->ItemHeader)) {
    USBH_LOG((USBH_MTYPE_OHCI, "OHCI: INFO OhEp0DoneCheckForCompletion: ED skipped!"));
  }
#endif
  if (OhEpGlobIsHalt(&ep->ItemHeader)) { // On halt
    USBH_LOG((USBH_MTYPE_OHCI, "OHCI: INFO OhEp0DoneCheckForCompletion: ED halt!"));
    OhEpGlobClearHalt(&ep->ItemHeader);  // Clear halt condition
  };
  tdMask            = 0;
  buffer            = NULL;
  buffer_length     = 0;
  setup_data_length = (U32)(setup[USB_SETUP_LENGTH_INDEX_MSB] << 8) + setup[USB_SETUP_LENGTH_INDEX_LSB];
  if (ep->Ep0Phase == ES_SETUP && !setup_data_length) { // Check phases
    ep->Ep0Phase = ES_PROVIDE_HANDSHAKE;                // No data goto provide handshake phase
  }
  pid = OH_IN_PID;                                      // Default value
  switch (ep->Ep0Phase) {
    case ES_SETUP:
      // End of setup and setup length unequal zero! Enter the data phase
      if (!USBH_IS_VALID_TRANSFER_MEM(urbRequest->Buffer)) {
        // No valid transfer memory transfer data in pieces!!!
        // 1. Allocate and initilaize the new transfer buffer on error complete the request, only the SETUP packet is transferered!
        // 2. Get new pointer and length
        // 3. If this is an OUT packet copy data to the new transfer buffer
        T_ASSERT(NULL == ep->DataPhaseCopyBuffer);
        ep->DataPhaseCopyBuffer = OhGetInitializedCopyTransferBuffer(&ep->Dev->TransferBufferPool, urbRequest->Buffer, setup_data_length);

        if (NULL == ep->DataPhaseCopyBuffer) { // STOP and COMPLETE
          USBH_WARN((USBH_MTYPE_OHCI, "OHCI: OhEp0DoneCheckForCompletion: OhGetInitializedTransferBuffer, setup ends!"));
          completeFlag = TRUE;
          ep->Ep0Phase = ES_ERROR;
          break;
        } else {                               // Valid transfer buffer update the buffer
          if (!in_dir_flag) {
            OhFillCopyTransferBuffer(ep->DataPhaseCopyBuffer);
          }
          buffer = OhGetBufferLengthFromCopyTransferBuffer(ep->DataPhaseCopyBuffer, &buffer_length);
          ep->Ep0Phase = ES_COPY_DATA;         // Next state after transfer: ES_DATA
        }
      } else {                                 // Use URB buffer as transfer buffer
        buffer        = urbRequest->Buffer;
        buffer_length = setup_data_length;
        ep->Ep0Phase  = ES_DATA;
      }
      tdMask = OHCI_TD_FORCE_DATA1;            // Set TD mask and PID, send the packet
      if (in_dir_flag) {
        pid     = OH_IN_PID;
        tdMask |= OHCI_TD_R;
      } else {
        pid = OH_OUT_PID;
      }
      break;
    case ES_COPY_DATA:
      // This state is only entered if DataPhaseCopyBuffer != NULL! If all bytes transferred ES_COPY_DATA fall through to ES_DATA!
      T_ASSERT_PTR(ep->DataPhaseCopyBuffer);
      if (in_dir_flag) {
        remaining_length = OhCopyToUrbBufferUpdateTransferBuffer(ep->DataPhaseCopyBuffer, transferred);
        if (remaining_length && !shortPkt) {
          // Remaining length available the last received packet was not an short packet
          buffer  = OhGetBufferLengthFromCopyTransferBuffer(ep->DataPhaseCopyBuffer, &buffer_length);
          pid     = OH_IN_PID;
          tdMask |= OHCI_TD_R;
          break;
        }
      // Tricky: fall through if needed
      } else {
        if (OhUpdateCopyTransferBuffer(ep->DataPhaseCopyBuffer, transferred)) {
          OhFillCopyTransferBuffer(ep->DataPhaseCopyBuffer);
          pid    = OH_OUT_PID;
          buffer = OhGetBufferLengthFromCopyTransferBuffer(ep->DataPhaseCopyBuffer, &buffer_length);
          break;
        }
      }
    // Tricky: fall through if needed
    case ES_DATA:
      // This state is entered if the data phase ends. If data are copied during the dataphase the state ES_COPY_DATA is the active state
      OhEp0UpdateUrbLength(ep, urbRequest, transferred); // Update the URB buffer length and enter the handshake phase
    // Tricky: fall through go to the handshake phase
    case ES_PROVIDE_HANDSHAKE:
      tdMask = OHCI_TD_FORCE_DATA1;
      if (!in_dir_flag || 0 == setup_data_length) {
        pid     = OH_IN_PID;
        tdMask |= OHCI_TD_R;
      } else {
        pid = OH_OUT_PID;
      }
      ep->Ep0Phase = ES_HANDSHAKE;
      break;
    case ES_HANDSHAKE: // End of handshake phase
      completeFlag = TRUE;
      ep->Ep0Phase = ES_IDLE;
      break;
    case ES_ERROR:
      // 1. Copy data and update transfer and URB buffer
      // 2. Stop from sending any PID and complete the request!
      if (ES_COPY_DATA == old_state) { // Last state was data phase update buffers
        if (in_dir_flag) {
          OhCopyToUrbBufferUpdateTransferBuffer(ep->DataPhaseCopyBuffer, transferred);
        } else {
          OhUpdateCopyTransferBuffer(ep->DataPhaseCopyBuffer, transferred);
        }
        OhEp0UpdateUrbLength(ep, urbRequest, transferred);
      }
      if (ES_DATA == old_state) { // Last state was data phase update buffers
        OhEp0UpdateUrbLength(ep, urbRequest, transferred);
      }
      completeFlag = TRUE;
      ep->Ep0Phase = ES_IDLE;
      break;
    case ES_IDLE:
      break;
    default:
      ;
  }
  if (ep->AbortMask) {
    // Endpoint is aborted, complete and resubmit newer added not aborted URBs!
    OhEp0CompleteAbortedUrbs(ep, TRUE);
    return;
  }
  if (completeFlag) {
    OhEp0CompleteUrb(ep, ep->PendingUrb, tdStatus);
  } else {
    status = OhEp0SubmitPacket(ep, buffer, buffer_length, tdMask, pid);
    if (status != USBH_STATUS_PENDING) { // On error
      OhEp0CompleteUrb(ep, ep->PendingUrb, status);
    }
  }
  OhEp0SubmitUrbsFromList(ep);
}

/*********************************************************************
*
*       OhEp0ServiceAllEndpoints
*
*  Function description
*/
/*
static void OhEp0ServiceAllEndpoints(HC_DEVICE*   dev) {
  OHD_EP0 * ep;
  PDLIST    entry;
  //
  // 1. remove all Eps from ControlEpList and put it to removeList
  //
  OH_DEV_VALID(dev);
  entry = DlistGetPrev(&dev->ControlEpList);
  while (entry != &dev->ControlEpList)  {
    ep    = GET_CONTROL_EP_FROM_ENTRY(entry);
    OH_EP0_VALID           (ep);
    entry = DlistGetPrev(entry);
    OhEp0SubmitUrbsFromList(ep);
  }
}
*/

/******************************* EOF ********************************/
