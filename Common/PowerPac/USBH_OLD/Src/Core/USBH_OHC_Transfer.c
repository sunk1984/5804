/*********************************************************************
*  
*   IAR PowerPac
*
*   (c) Copyright IAR Systems 2010.  All rights reserved.
*
**********************************************************************
----------------------------------------------------------------------
File        : USBH_OHC_Transfer.c
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
*       OhTGetItemFromPhyAddress
*
*  Function description
*/
static HCM_ITEM_HEADER * OhTGetItemFromPhyAddress(HC_DEVICE * Dev, U32 PhyAddr, T_BOOL * IsoFlag) {
  HCM_ITEM_HEADER * item;
#if OH_ISO_ENABLE
  if (HcmIsPhysAddrInPool(&Dev->IsoTDPool, PhyAddr)) {
    // Iso TD
    item      = HcmGetItemFromPhyAddr(&Dev->IsoTDPool, PhyAddr);
    * IsoFlag = TRUE;
  } else {
    item      = HcmGetItemFromPhyAddr(&Dev->GTDPool, PhyAddr);
    * IsoFlag = FALSE;
  }
#else
  * IsoFlag   = FALSE;
  item        = HcmGetItemFromPhyAddr(&Dev->GTDPool, PhyAddr);
#endif
  if (item == NULL) {
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: OhTDProcessInterruptDone: HcmGetItemFromPhyAddr!"));
  }
  return item;
}

/*********************************************************************
*
*       OhT_SubmitRequest
*
*  Function description
*    Submit a request to the HC. If USBH_STATUS_PENDING is returned the
*    request is in the queue and the completion routine is called later.
*/
USBH_STATUS OhT_SubmitRequest(USBH_HC_EP_HANDLE EpHandle, URB * Urb) {
  USBH_STATUS           status;
  OHD_EP0             * ep0;
  OHD_BULK_INT_EP     * bulkIntEp;
  Urb->Header.HcFlags = 0;
  Urb->Header.Status  = USBH_STATUS_PENDING;
  T_ASSERT(EpHandle != NULL);
  status              = USBH_STATUS_ERROR;
  switch (Urb->Header.Function) {
    case USBH_FUNCTION_CONTROL_REQUEST:
      ep0 = (OHD_EP0 *)EpHandle;
      USBH_LOG((USBH_MTYPE_OHCI, "OHCI: INFO OhT_SubmitRequest: control request!"));
      // Get the endpoint and add this request
      status = OhEp0AddUrb(ep0, Urb);
      if (status != USBH_STATUS_PENDING) {
        USBH_WARN((USBH_MTYPE_OHCI, "OHCI: OhT_SubmitRequest: OhEp0AddUrb %08x!",status));
      }
      break;
    case USBH_FUNCTION_BULK_REQUEST:
    case USBH_FUNCTION_INT_REQUEST:
      bulkIntEp = (OHD_BULK_INT_EP *)EpHandle;
      USBH_LOG((USBH_MTYPE_OHCI, "OHCI: INFO OhT_SubmitRequest: ep: 0x%x length: %lu!", bulkIntEp->EndpointAddress, Urb->Request.BulkIntRequest.Length));
      status = OhBulkIntAddUrb(bulkIntEp, Urb);
      if (status != USBH_STATUS_PENDING) {
        USBH_WARN((USBH_MTYPE_OHCI, "OHCI: OhT_SubmitRequest: OhBulkIntAddUrb!"));
      }
      break;
    default:
      USBH_WARN((USBH_MTYPE_OHCI, "OHCI: OhT_SubmitRequest: invalid URB function type!"));
  }
  return status;
}

/*********************************************************************
*
*       OhT_SubmitRequest
*
*  Function description
*    Process the hosts DONE interrupt
*/
void OhTProcessDoneInterrupt(HC_DEVICE * dev) {
  OHD_GTD         * gtd;
  U32               doneHead, nextAddr;
  T_BOOL isoFlag  = FALSE;
  HCM_ITEM_HEADER * item;
  int               i;
  OH_DEV_VALID(dev);
  T_ASSERT(dev->OhHcca != NULL);
  doneHead = dev->OhHcca->DoneHead &( ~OH_DONE_HEAD_INT_MASK);
  if (doneHead == 0) {
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: OhTDProcessInterruptDone: DoneHead ptr. is null!"));
    return;
  }
  dev->OhHcca->DoneHead = 0;
  // Marks all transfer descriptors as complete. Check the status!
  for (i = 0; i < 2; i++) {
    // In the first loop update the endpoint length field and read the status,
    // in the second loop check if an endpoint can be completed (error on status or all bytes transferred!
    nextAddr = doneHead;
    for (; ;) {
      if (nextAddr == 0) {
        break;
      }
      item = OhTGetItemFromPhyAddress(dev, nextAddr, &isoFlag);
      if (item == NULL) {
        USBH_WARN((USBH_MTYPE_OHCI, "OHCI: FATAL OhTDProcessInterruptDone: OhTGetItemFromPhyAddress, stop process done list!"));
        goto OHT_ERR;
      }
      // Important: if an endpoint is deleted this can contains an invalid address
      nextAddr = OhTdGetNextTd(item->VirtAddr);
#if OH_ISO_ENABLE
      // Update ISO endpoints
      if (isoFlag) {
        tdi = (OHD_TDI *)item;
        if (i == 0) {
          OhIsoUpdateDone(tdi);
        } else {
          OhIsoCheckForCompletion(tdi);
        }
      } else
#endif
      // Update none ISO endpoints
      {
        gtd = (OHD_GTD * )item;
        // Update td field and the endpoint
        switch (gtd->EndpointType) {
          case USB_EP_TYPE_CONTROL:
            if (i == 1) {
              // Because only one TD for the control endpoint is used in the second pass the TD of the control endpoint is removed
              OhEp0DoneCheckForCompletion(gtd);
            }
            break;
          case USB_EP_TYPE_BULK:
          case USB_EP_TYPE_INT:
            if (i == 0) {
              OhBulkIntUpdateTDLengthStatus(gtd);
            } else {
              OhBulkIntCheckForCompletion(gtd);
            }
            break;
          default:
            USBH_WARN((USBH_MTYPE_OHCI, "OHCI: FATAL OhTDProcessInterruptDone: invalid Ep type!"));
            break;
        }
      }
    }
  }
  // If URBs available and the endpoint state is valid for transfer submit new URbs
  OHT_ERR:
  ;
  // Activate the DONE list again
}

/******************************* EOF ********************************/
