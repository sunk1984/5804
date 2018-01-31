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
*       Define configurable
*
**********************************************************************
*/
/*
 *  This define contains the interrupt delay that is used for all Transfer descriptors.
 *  If a USB transfer request is complete the host controller may wait for HC_TRANSFER_INTERRUPT_RATE
 *  frames before generating an interrupt. Higher values as 0 decrease the transfer rate.
 */
#define HC_TRANSFER_INTERRUPT_RATE  0



/*********************************************************************
*
*       OhTdGetRemainingLength
*
*  Function description
*    Returns the remaining length of the current transfer. The addresses
*    can be in different pages, CBPAddr must be unequal zero.
*    TransferredLength = TotalLegth - RemainingLength.
*  Parameters:
*    CBPAddr:physical address of memory that will be accessed for the next transfer
*/
static U32 _GetRemainingLength(U32 CBPAddr, U32 BEAddr)  {
  U32 r;
  r = ((BEAddr ^ CBPAddr)  & 0xFFFFF000 ) ? 0x00001000 : 0;
  r += (BEAddr & 0x00000FFF) - (CBPAddr & 0x00000FFF) + 1;
  return r;
}

/*********************************************************************
*
*       USBH_OHCI_TdGetDword0
*
*/
static U32 _GetDword0(USBH_OHCI_TD_PID TransferType, U8 EpType, U32 Dword0Mask) {
  U32 dword0;

  /* buffer rounding is on accept always short packets */
  EpType = EpType;
  dword0 = 0;
  switch (TransferType) {
  case OH_SETUP_PID:
    /* DATA 1 PID */
    break;
  case OH_IN_PID:
    dword0 |= OH_IN_PID << OHCI_TD_PID_BIT;
    break;
  case OH_OUT_PID:
    dword0 |= OH_OUT_PID << OHCI_TD_PID_BIT;
    break;
  default:
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: OhTdInitdword0: invalid TransferType!"));
    break;
  }
  /* This is the interrupt delay time */
  dword0 |= ((HC_TRANSFER_INTERRUPT_RATE << OHCI_TD_DI_BIT) & OHCI_TD_DI);
  dword0 |= Dword0Mask;
  /* Set the condition to not accessed */
  dword0 |= (U32)USBH_STATUS_NOT_ACCESSED << OHCI_TD_CC_BIT;
  return dword0;
}

/*********************************************************************
*
*       USBH_OHCI_TdAlloc
*
*  Function description:
*    init the struct
*
*/
USBH_STATUS USBH_OHCI_TdAlloc(USBH_HCM_POOL * GeneralTd, U32 GeneralTdNumbers, unsigned Alignment) {
  USBH_STATUS status;

  /* initialize all memory pools */
  status = USBH_HCM_AllocPool(GeneralTd, GeneralTdNumbers, OH_GTD_SIZE, sizeof(USBH_OHCI_INFO_GENERAL_TRANS_DESC), Alignment); /* size of Open host driver TD object*/

  if (status) {
    /* on error */
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: OhGeneralTdInit: USBH_HCM_AllocPool!"));
  }
  return status;
}

/*********************************************************************
*
*       USBH_OHCI_GetTransDesc
*
*  Function description:
*    Returns an uninitialized transfer descriptor from the pool.
*    The once initialized field status has the value OH_TD_EMPTY!
*
*/
USBH_OHCI_INFO_GENERAL_TRANS_DESC * USBH_OHCI_GetTransDesc(USBH_HCM_POOL * pPool) {
  USBH_OHCI_INFO_GENERAL_TRANS_DESC * pItem;

  USBH_ASSERT(USBH_IS_PTR_VALID(pPool, USBH_HCM_POOL));

  pItem = (USBH_OHCI_INFO_GENERAL_TRANS_DESC * )USBH_HCM_GetItem(pPool);

  if (pItem == NULL) {
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: USBH_OHCI_GetTransDesc: USBH_HCM_GetItem!"));
    return NULL;
  }
  pItem->Status = OH_TD_EMPTY;
  USBH_ASSERT(USBH_IS_ALIGNED(pItem->ItemHeader.PhyAddr, OH_GTD_ALIGNMENT));
  return pItem;
}

//
/*********************************************************************
*
*       USBH_OHCI_TdInit
*
*  Function description:
*    Initializes a general transfer descriptor object and write all needed values to the phys. TD item.
*    The TD is not added to any endpoint at this point!
*
*/
void USBH_OHCI_TdInit(USBH_OHCI_INFO_GENERAL_TRANS_DESC * pGlobalTransDesc, void * Ep, U8 EndpointType, USBH_OHCI_TD_PID Pid, U32 StartAddr, U32 EndAddr, U32 Dword0Mask) {
// Dword0Mask: Masks of type OHCI_TD_DATA0...defined in ohci.h these bits are additional to other parameter
  USBH_OHCI_TRANSFER_DESC * pTransferDesc;
  USBH_ASSERT(pGlobalTransDesc != NULL);
  USBH_ASSERT(Ep != NULL);
  // Set the TD extension
  pGlobalTransDesc->CancelPendingFlag = FALSE;
  if (StartAddr != 0) {
    // Full length
    pGlobalTransDesc->Size = _GetRemainingLength(StartAddr, EndAddr);
  } else {
    pGlobalTransDesc->Size = 0;
  }
  pGlobalTransDesc->EndpointType = EndpointType;
  pGlobalTransDesc->pEp = Ep;
  pGlobalTransDesc->Status = OH_TD_PENDING;

  // Set DWORD 0 start and end address
  pTransferDesc = (USBH_OHCI_TRANSFER_DESC *)pGlobalTransDesc->ItemHeader.PhyAddr;
  // Error Count and Condition code are zero
  pTransferDesc->Dword0 = _GetDword0(Pid, EndpointType, Dword0Mask);
  pTransferDesc->CBP = StartAddr;
  pTransferDesc->BE = EndAddr;
  pTransferDesc->NextTD = 0;
//   USBH_LOG((USBH_MTYPE_OHCI, "OHCI: TD-Word0: 0x%lx R: %d DP:%d DI:%d T:%d  start:0x%lx  end:  0x%lx  size: %lu!",
//       td->Dword0,
//       (td->Dword0 & OHCI_TD_R) ? 1:0,
//       (td->Dword0 >> OHCI_TD_PID_BIT) & 0x3,
//       (td->Dword0 >> OHCI_TD_DI_BIT) & 0x7,
//       (td->Dword0 >> OHCI_TD_T_BIT) & 0x3,
//       td->CBP,
//       td->BE,
//       pGlobalTransDesc->Size));
}

/*********************************************************************
*
*       USBH_OHCI_IsoTdInit
*
*  Function description:
*    Initializes a general transfer descriptor object and write all needed values to the phys. TD pItem.
*    The TD is not added to any endpoint at this point!
*
*/
void USBH_OHCI_IsoTdInit(USBH_OHCI_INFO_GENERAL_TRANS_DESC * pGlobalTransDesc, USBH_OHCI_ISO_EP * pIsoEp, USBH_ISO_REQUEST * pIsoRequest, U32 DWord0, U32 StartAddr, int NumBytes, int Index) {
  USBH_OHCI_ISO_TRANS_DESC * pTransferDesc;

  (void)pIsoRequest;
  (void)Index;
  USBH_ASSERT(pGlobalTransDesc != NULL);
  USBH_ASSERT(pIsoEp != NULL);
  // Set the TD extension
  pGlobalTransDesc->CancelPendingFlag = FALSE;
  if (StartAddr) {
    // Full length
    pGlobalTransDesc->Size = _GetRemainingLength(StartAddr, StartAddr + NumBytes - 1);
  } else {
    pGlobalTransDesc->Size = 0;
  }
  pGlobalTransDesc->EndpointType = USB_EP_TYPE_ISO;
  pGlobalTransDesc->pEp = pIsoEp;
  pGlobalTransDesc->Status = OH_TD_PENDING;

  // Set DWORD 0 start and end address
  pTransferDesc = (USBH_OHCI_ISO_TRANS_DESC *)pGlobalTransDesc->ItemHeader.PhyAddr;
  // Error Count and Condition code are zero
  pTransferDesc->Dword0 = DWord0;
  pTransferDesc->Dword1 = StartAddr & 0xFFFFF000;
  pTransferDesc->OfsPsw[0] = (U16)((StartAddr & 0x0FFF) | 0xE000);
  pTransferDesc->BE = StartAddr + NumBytes - 1;


}


/*********************************************************************
*
*       USBH_OHCI_TdGetNextTd
*
*  Function description:
*    Returns the physical start address of the next iso or general TD
*
*/
U32 USBH_OHCI_TdGetNextTd(U32 TdAddress) {
  USBH_OHCI_TRANSFER_DESC * td;
  td      = (USBH_OHCI_TRANSFER_DESC *)TdAddress;
  return  td->NextTD;
}

/*********************************************************************
*
*       USBH_OHCI_TdGetStatusAndLength
*
*  Function description:
*    Returns the transferred length in bytes.
*    The TD status is set to OH_TD_COMPLETED and the condition code is read from the TD.
*
*/
USBH_STATUS USBH_OHCI_TdGetStatusAndLength(USBH_OHCI_INFO_GENERAL_TRANS_DESC * pGlobalTransDesc, U32 * pTransferred, USBH_BOOL * pShortPacket) {
  USBH_OHCI_TRANSFER_DESC     * pTransferDesc;
  U32           remaining;
  USBH_STATUS   status;
  U32           v;

  * pTransferred = 0;
  * pShortPacket    = FALSE;
  if (pGlobalTransDesc == NULL) {
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: USBH_OHCI_TdGetStatusAndLength: pGlobalTransDesc NULL!"));
    return USBH_STATUS_ERROR;
  }
  pTransferDesc     = (USBH_OHCI_TRANSFER_DESC *)pGlobalTransDesc->ItemHeader.PhyAddr;
  status = (USBH_STATUS)(((pTransferDesc->Dword0 &OHCI_TD_CC) >> OHCI_TD_CC_BIT));
  if (status == USBH_STATUS_NOT_ACCESSED) {
    /* not needed only for testing DBGOUT(DBG_WARN,DbgPrint(DBGPFX"INFO USBH_OHCI_TdGetStatusAndLength: TD not accessed from host!"));*/
    return status;
  }
  if (pTransferDesc->CBP == 0) {
    /* zero length packet or all bytes transferred */
    * pTransferred = pGlobalTransDesc->Size;

    if (*pTransferred == 0) {
      * pShortPacket = TRUE;
    }
  } else {
    v= pTransferDesc->CBP;
    remaining = _GetRemainingLength(v, pTransferDesc->BE);
#if (USBH_DEBUG > 1)
    if (remaining > pGlobalTransDesc->Size) {
      USBH_PANIC("FATAL USBH_OHCI_TdGetStatusAndLength: remaining > pGlobalTransDesc->Counter!");
      remaining = pGlobalTransDesc->Size;
    }
#endif
    // Return the remaining from the total size and the remaining remaining
    * pTransferred = pGlobalTransDesc->Size - remaining;
    if (*pTransferred != pGlobalTransDesc->Size) {
      * pShortPacket = TRUE;
    }
  }
  if (status == USBH_STATUS_DATA_UNDERRUN) {
    * pShortPacket = TRUE;  // this bit only set if this TD is not the last TD in the list, the HC halts the ED!
  }
  return status;
}

/*********************************************************************
*
*       USBH_OHCI_ISO_TdGetStatusAndLength
*
*  Function description:
*    Returns the transferred length in bytes.
*    The TD status is set to OH_TD_COMPLETED and the condition code is read from the TD.
*
*/
USBH_STATUS USBH_OHCI_ISO_TdGetStatusAndLength(USBH_OHCI_INFO_GENERAL_TRANS_DESC * pGlobalTransDesc, U32 * pTransferred, USBH_BOOL * pShortPacket) {
  USBH_OHCI_ISO_TRANS_DESC * pTransferDesc;
  U32           remaining;
  USBH_STATUS   status;
  U32           v;

  * pTransferred = 0;
  * pShortPacket    = FALSE;
  if (pGlobalTransDesc == NULL) {
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: USBH_OHCI_TdGetStatusAndLength: pGlobalTransDesc NULL!"));
    return USBH_STATUS_ERROR;
  }
  pTransferDesc     = (USBH_OHCI_ISO_TRANS_DESC *)pGlobalTransDesc->ItemHeader.PhyAddr;
  status = (USBH_STATUS)(((pTransferDesc->Dword0 &OHCI_TD_CC) >> OHCI_TD_CC_BIT));
  if (status == USBH_STATUS_NOT_ACCESSED) {
    /* not needed only for testing DBGOUT(DBG_WARN,DbgPrint(DBGPFX"INFO USBH_OHCI_TdGetStatusAndLength: TD not accessed from host!"));*/
    return status;
  }
  if (pTransferDesc->Dword1 == 0) {
    /* zero length packet or all bytes transferred */
    * pTransferred = pGlobalTransDesc->Size;

    if (*pTransferred == 0) {
      * pShortPacket = TRUE;
    }
  } else {
    v  = pTransferDesc->Dword1;
    v |= pTransferDesc->OfsPsw[0] & 0xFFF;
    remaining = _GetRemainingLength(v, pTransferDesc->BE);
#if (USBH_DEBUG > 1)
//    if (remaining > pGlobalTransDesc->Size) {
//      USBH_PANIC("FATAL USBH_OHCI_TdGetStatusAndLength: remaining > pGlobalTransDesc->Counter!");
//      remaining = pGlobalTransDesc->Size;
//    }
#endif
    // Return the remaining from the total size and the remaining remaining
    *pTransferred = pGlobalTransDesc->Size - remaining;
    if (*pTransferred != pGlobalTransDesc->Size) {
      *pShortPacket = TRUE;
    }
  }
  if (status == USBH_STATUS_DATA_UNDERRUN) {
    * pShortPacket = TRUE;  // this bit only set if this TD is not the last TD in the list, the HC halts the ED!
  }
  return status;
}


/********************************* EOF****************************************/
