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
*       _GetItemFromPhyAddress
*
*  Function description
*/
static USBH_HCM_ITEM_HEADER * _GetItemFromPhyAddress(USBH_OHCI_DEVICE * pDev, U32 PhyAddr, USBH_BOOL * pIsoFlag) {
  USBH_HCM_ITEM_HEADER * pItem;
#if USBH_SUPPORT_ISO_TRANSFER
  if (USBH_HCM_IsPhysAddrInPool(&pDev->IsoTDPool, PhyAddr)) {
    // Iso TD
    pItem     = USBH_HCM_GetItemFromPhyAddr(&pDev->IsoTDPool, PhyAddr);
    *pIsoFlag = TRUE;
  } else {
    pItem     = USBH_HCM_GetItemFromPhyAddr(&pDev->GTDPool, PhyAddr);
    *pIsoFlag = FALSE;
  }
#else
  *pIsoFlag   = FALSE;
  pItem       = USBH_HCM_GetItemFromPhyAddr(&pDev->GTDPool, PhyAddr);
#endif
  if (pItem == NULL) {
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: OhTDProcessInterruptDone: USBH_HCM_GetItemFromPhyAddr!"));
  }
  return pItem;
}

/*********************************************************************
*
*       USBH_OHCI_TRANSFER_SubmitRequest
*
*  Function description
*    Submit a request to the HC. If USBH_STATUS_PENDING is returned the
*    request is in the queue and the completion routine is called later.
*/
USBH_STATUS USBH_OHCI_TRANSFER_SubmitRequest(USBH_HC_EP_HANDLE hEndPoint, USBH_URB * pUrb) {
  USBH_STATUS                 Status;
  USBH_OHCI_EP0             * pEP0;
  USBH_OHCI_BULK_INT_EP     * pBulkIntEp;
  USBH_OHCI_ISO_EP          * pIsoEp = NULL;

  pUrb->Header.HcFlags = 0;
  pUrb->Header.Status  = USBH_STATUS_PENDING;
  USBH_ASSERT(hEndPoint != NULL);
  Status              = USBH_STATUS_ERROR;
  switch (pUrb->Header.Function) {
  case USBH_FUNCTION_CONTROL_REQUEST:
    pEP0 = (USBH_OHCI_EP0 *)hEndPoint;
    USBH_LOG((USBH_MTYPE_OHCI, "OHCI: INFO USBH_OHCI_TRANSFER_SubmitRequest: control request!"));
    // Get the endpoint and add this request
    Status = USBH_OHCI_EP0_AddUrb(pEP0, pUrb);
    if (Status != USBH_STATUS_PENDING) {
      USBH_WARN((USBH_MTYPE_OHCI, "OHCI: USBH_OHCI_TRANSFER_SubmitRequest: USBH_OHCI_EP0_AddUrb %08x!",Status));
    }
    break;
  case USBH_FUNCTION_BULK_REQUEST:
  case USBH_FUNCTION_INT_REQUEST:
    pBulkIntEp = (USBH_OHCI_BULK_INT_EP *)hEndPoint;
    USBH_LOG((USBH_MTYPE_OHCI, "OHCI: INFO USBH_OHCI_TRANSFER_SubmitRequest: EP: 0x%x length: %lu!", pBulkIntEp->EndpointAddress, pUrb->Request.BulkIntRequest.Length));
    Status = USBH_OHCI_BULK_INT_AddUrb(pBulkIntEp, pUrb);
    if (Status != USBH_STATUS_PENDING) {
      USBH_WARN((USBH_MTYPE_OHCI, "OHCI: USBH_OHCI_TRANSFER_SubmitRequest: USBH_OHCI_BULK_INT_AddUrb!"));
    }
    break;
  case USBH_FUNCTION_ISO_REQUEST:
#if USBH_SUPPORT_ISO_TRANSFER
    pIsoEp = (USBH_OHCI_ISO_EP *)hEndPoint;
    USBH_LOG((USBH_MTYPE_OHCI, "OHCI: INFO USBH_OHCI_TRANSFER_SubmitRequest: EP: 0x%x length: %lu!", pIsoEp->EndpointAddress, pUrb->Request.IsoRequest.Length));
    Status = USBH_OHCI_ISO_AddUrb(pIsoEp, pUrb);
    if (Status != USBH_STATUS_PENDING) {
      USBH_WARN((USBH_MTYPE_OHCI, "OHCI: USBH_OHCI_TRANSFER_SubmitRequest: USBH_OHCI_ISO_AddUrb!"));
    }
#else
    (void)pIsoEp;
#endif
    break;
  default:
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: USBH_OHCI_TRANSFER_SubmitRequest: invalid USBH_URB function type!"));
  }
  return Status;
}

/*********************************************************************
*
*       USBH_OHCI_TRANSFER_ProcessDoneInterrupt
*
*  Function description
*    Process the hosts DONE interrupt
*/
void USBH_OHCI_TRANSFER_ProcessDoneInterrupt(USBH_OHCI_DEVICE * pDev) {
  USBH_OHCI_INFO_GENERAL_TRANS_DESC         * gtd;
  U32               doneHead;
  U32               nextAddr;
  USBH_BOOL         isoFlag  = FALSE;
  USBH_HCM_ITEM_HEADER * pItem;
  int               i;

  USBH_OCHI_IS_DEV_VALID(pDev);
  USBH_ASSERT(pDev->pOhHcca != NULL);
  doneHead = pDev->pOhHcca->DoneHead & (~OH_DONE_HEAD_INT_MASK);
  if (doneHead == 0) {
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: OhTDProcessInterruptDone: DoneHead ptr. is null!"));
    return;
  }
  pDev->pOhHcca->DoneHead = 0;
  // Marks all transfer descriptors as complete. Check the Status!
  for (i = 0; i < 2; i++) {
    // In the first loop update the endpoint length field and read the Status,
    // in the second loop check if an endpoint can be completed (error on Status or all bytes transferred!
    nextAddr = doneHead;
    for (; ;) {
      if (nextAddr == 0) {
        break;
      }
      pItem = _GetItemFromPhyAddress(pDev, nextAddr, &isoFlag);
      if (pItem == NULL) {
        USBH_WARN((USBH_MTYPE_OHCI, "OHCI: FATAL OhTDProcessInterruptDone: _GetItemFromPhyAddress, stop process done list!"));
        goto OHT_ERR;
      }
      // Important: if an endpoint is deleted this can contains an invalid address
      nextAddr = USBH_OHCI_TdGetNextTd(pItem->PhyAddr);
#if USBH_SUPPORT_ISO_TRANSFER
      // Update ISO endpoints
      if (isoFlag) {
        USBH_OHCI_INFO_GENERAL_TRANS_DESC * pTdi;

        pTdi = (USBH_OHCI_INFO_GENERAL_TRANS_DESC *)pItem;
        if (i == 0) {
          USBH_OHCI_ISO_UpdateTDLengthStatus(pTdi);
        } else {
          USBH_OHCI_ISO_CheckForCompletion(pTdi);
        }
      } else
#endif
      // Update none ISO endpoints
      {
        gtd = (USBH_OHCI_INFO_GENERAL_TRANS_DESC * )pItem;
        // Update td field and the endpoint
        switch (gtd->EndpointType) {
        case USB_EP_TYPE_CONTROL:
          if (i == 1) {
            // Because only one TD for the control endpoint is used in the second pass the TD of the control endpoint is removed
            USBH_OHCI_EP0_DoneCheckForCompletion(gtd);
          }
          break;
        case USB_EP_TYPE_BULK:
        case USB_EP_TYPE_INT:
          if (i == 0) {
            USBH_OHCI_BULK_INT_UpdateTDLengthStatus(gtd);
          } else {
            USBH_OHCI_BULK_INT_CheckForCompletion(gtd);
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
  return;
  // Activate the DONE list again
}

/******************************* EOF ********************************/
