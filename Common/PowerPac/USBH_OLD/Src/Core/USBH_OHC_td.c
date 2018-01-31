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
static U32 OhTdGetRemainingLength(U32 CBPAddr, U32 BEAddr)  {
  U32 r;
  r = ((BEAddr ^ CBPAddr)  & 0xFFFFF000 ) ? 0x00001000 : 0;
  r += (BEAddr & 0x00000FFF) - (CBPAddr & 0x00000FFF) + 1;
  return r;
}





static U32 OhTdGetDword0(OHD_TD_PID TransferType,
/* the type of the endpoint, one of USB_EP_TYPE_CONTROL, ...*/
U8 EpType, U32 Dword0Mask) {
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


/* init the struct */
USBH_STATUS OhTdAlloc(HCM_POOL * GeneralTd, U32 GeneralTdNumbers) {
  USBH_STATUS status;

  /* initialize all memory pools */
  status = HcmAllocPool(GeneralTd, GeneralTdNumbers, OH_GTD_SIZE, sizeof(OHD_GTD), OH_GTD_ALIGNMENT); /* size of Open host driver TD object*/

  if (status) {
    /* on error */
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: OhGeneralTdInit: HcmAllocPool!"));
  }
  return status;
}

// Returns an uninitialized transfer descriptor from the pool. The once initialized field status has the value OH_TD_EMPTY!
OHD_GTD * OhTdGet(HCM_POOL * Pool) {
  OHD_GTD * item;
  T_ASSERT(PTRVALID(Pool, HCM_POOL));

  item = (OHD_GTD * )HcmGetItem(Pool);

  if (item == NULL) {
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: OhTdGet: HcmGetItem!"));
    return NULL;
  }
  item->Status = OH_TD_EMPTY;
  T_ASSERT(TB_IS_ALIGNED(item->ItemHeader.PhyAddr, OH_GTD_ALIGNMENT));
  return item;
}

// Initialize an general transfer descriptor object and write all needed values to the phys. TD item.
// The TD is not added to any endpoint at this point!
void OhTdInit(OHD_GTD * gtd, void * Ep, U8 EndpointType, OHD_TD_PID Pid, U32 StartAddr, U32 EndAddr, U32 Dword0Mask) {
// Dword0Mask: Masks of type OHCI_TD_DATA0...defined in ohci.h these bits are additional to other parameter
  OHCI_TD * td;
  T_ASSERT(gtd != NULL);
  T_ASSERT(Ep != NULL);
  // Set the TD extension
  gtd->CancelPendingFlag = FALSE;
  if (StartAddr != 0) {
    // Full length
    gtd->Size = OhTdGetRemainingLength(StartAddr, EndAddr);
  } else {
    gtd->Size = 0;
  }
  gtd->EndpointType = EndpointType;
  gtd->Ep = Ep;
  gtd->Status = OH_TD_PENDING;

  // Set DWORD 0 start and end addres
  td = gtd->ItemHeader.VirtAddr;
  // Error Count and Condition code are zero
  td->Dword0 = OhTdGetDword0(Pid, EndpointType, Dword0Mask);
  td->CBP = StartAddr;
  td->BE = EndAddr;
  td->NextTD = 0;
//   USBH_LOG((USBH_MTYPE_OHCI, "OHCI: TD-Word0: 0x%lx R: %d DP:%d DI:%d T:%d  start:0x%lx  end:  0x%lx  size: %lu!",
//       td->Dword0,
//       (td->Dword0 & OHCI_TD_R) ? 1:0,
//       (td->Dword0 >> OHCI_TD_PID_BIT) & 0x3,
//       (td->Dword0 >> OHCI_TD_DI_BIT) & 0x7,
//       (td->Dword0 >> OHCI_TD_T_BIT) & 0x3,
//       td->CBP,
//       td->BE,
//       gtd->Size));
}

// Returnes the physical start address of the next iso or general TD
U32 OhTdGetNextTd(void * VirtTdAddr) {
  OHCI_TD * td;
  td      = (OHCI_TD*)VirtTdAddr;
  return  td->NextTD;
}

// OhTdGetStatusAndLength returns the transferred length in bytes.
// The TD status is set to OH_TD_COMPLETED and the condition code is read from the TD.
USBH_STATUS OhTdGetStatusAndLength(OHD_GTD * Gtd, U32 * Transferred, T_BOOL * shortPkt) {
  OHCI_TD       * td;
  U32   remaining;
  USBH_STATUS      status;
  U32             v;
  * Transferred = 0;
  * shortPkt    = FALSE;
  if (Gtd == NULL) {
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: OhTdGetStatusAndLength: Gtd NULL!"));
    return USBH_STATUS_ERROR;
  }
  td     = Gtd->ItemHeader.VirtAddr;
  status = (USBH_STATUS)(((td->Dword0 &OHCI_TD_CC) >> OHCI_TD_CC_BIT));
  if (status == USBH_STATUS_NOT_ACCESSED) {
    /* not needed only for testing DBGOUT(DBG_WARN,DbgPrint(DBGPFX"INFO OhTdGetStatusAndLength: TD not accessed from host!"));*/
    return status;
  }
  if (td->CBP == 0) {
    /* zero length packet or all bytes transferred */
    * Transferred = Gtd->Size;

    if (*Transferred == 0) {
      * shortPkt = TRUE;
    }
  } else {
    v= td->CBP;
    remaining = OhTdGetRemainingLength(v, td->BE);
#if (USBH_DEBUG > 1)
    if (remaining > Gtd->Size) {
      USBH_PANIC("FATAL OhTdGetStatusAndLength: remaining > Gtd->Counter!");
      remaining = Gtd->Size;
    }
#endif
    // Return the remaining from the total size and the remaining remaining
    * Transferred = Gtd->Size - remaining;
    if (*Transferred != Gtd->Size) {
      * shortPkt = TRUE;
    }
  }
  if (status == USBH_STATUS_DATA_UNDERRUN) {
    * shortPkt = TRUE;  // this bit only set if this TD is not the last TD in the list, the HC halts the ED!
  }
  return status;
}

/********************************* EOF****************************************/
