/*********************************************************************
*  
*   IAR PowerPac
*
*   (c) Copyright IAR Systems 2010.  All rights reserved.
*
**********************************************************************
----------------------------------------------------------------------
File        : USBH_OHC_EP.c
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
*       OhEpGlobBuildEDword0
*
*  Function description
*    Builds DWORD 0 of the OHCI endpoint descriptor
*/
static U32 OhEpGlobBuildEDword0(U8 DeviceAddr, U8 EpWithDirection, U32 MaxPktSize, T_BOOL SkipFlag, T_BOOL IsoFlag, USBH_SPEED Speed) {
  U32       dword0;
  dword0  = DeviceAddr;
  dword0 |= (U32)((((U32)EpWithDirection) & 0xf) << OHCI_ED_EN_BIT);
  if (0 != (EpWithDirection & 0x0f)) { // On not control endpoint
    if (EpWithDirection & 0x80) {      // On IN endpoint
      dword0 |= (OHCI_ED_IN_DIR << OHCI_ED_DIR_BIT);
    } else {
      dword0 |= (OHCI_ED_OUT_DIR << OHCI_ED_DIR_BIT);
    }
  }
  if (Speed == USBH_LOW_SPEED) {
    dword0 |= OHCI_ED_S;
  }
  if (IsoFlag) {
    dword0 |= OHCI_ED_F;
  }
  if (SkipFlag) {
    dword0 |= OHCI_ED_K;
  }
  dword0 |= MaxPktSize << 16;
  return dword0;
}

/*********************************************************************
*
*       OhEpGlobInitED
*
*  Function description
*    Changes all ED fields. Initializes ED Dword0. This call is only
*    allowed if the ED is not in the list or has been stopped!
*/
void OhEpGlobInitED(HCM_ITEM_HEADER * Header, U8 DeviceAddr, U8 EpWithDirection, U32 MaxPktSize, T_BOOL IsoFlag, T_BOOL SkipFlag, USBH_SPEED Speed) {
  OHCI_ED    * ed;
  ed         = Header->VirtAddr;
  ed->Dword0 = OhEpGlobBuildEDword0(DeviceAddr, EpWithDirection, MaxPktSize, SkipFlag, IsoFlag, Speed);
  ed->HeadP  = ed->NextED = ed->TailP = 0; // Set next ED pointer and Pointers to Transfer descriptors to NULL
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: OhEpGlobInitED: device addr: %d  Ep: %d  MaxPktSize: %lu dword0: 0x%lx", DeviceAddr, EpWithDirection, MaxPktSize, ed->Dword0));
}

/*********************************************************************
*
*       OhEpGlobInsertTD
*
*  Function description
*    Insert an new TD in the ED TD list and inserts this TD also in
*    the logical link list. Adds an new TD to the endpoints TD list head.
*/
void OhEpGlobInsertTD(HCM_ITEM_HEADER * EpHeader, HCM_ITEM_HEADER * NewTdHeader, U32 * TdCounter) {
  OHCI_ED         * ed;
  OHCI_TD         * td;
  HCM_ITEM_HEADER * prevTdHeader;
  PDLIST            entry;
  ed              = EpHeader->VirtAddr;
  if (DlistEmpty(&EpHeader->Link.ListEntry)) { // Physical link
    // Insert the first TD
    USBH_LOG((USBH_MTYPE_OHCI, "OHCI: OhEpGlobInsertTD: First phy. TD Addr: 0x%lx!",NewTdHeader->PhyAddr));
    ed->TailP = NewTdHeader->PhyAddr;
    ed->HeadP = NewTdHeader->PhyAddr; // Clears indirect the HALT confition and the toggle carry bit
  } else { // Insert other TDs
    entry = DlistGetPrev(&EpHeader->Link.ListEntry);
    prevTdHeader = GET_HCMITEM_FROM_ENTRY(entry);
    USBH_LOG((USBH_MTYPE_OHCI, "OHCI: OhEpGlobInsertTD: phy TD Addr: 0x%lx!",NewTdHeader->PhyAddr));
    // Set next pointer
    td = prevTdHeader->VirtAddr;
    td->NextTD = NewTdHeader->PhyAddr;
    // Set the ED tail pointer
    ed->TailP = NewTdHeader->PhyAddr;
  }
  // Logical link
  * TdCounter = (* TdCounter)+1;
  DlistInsertTail(&EpHeader->Link.ListEntry, &NewTdHeader->Link.ListEntry);
}

/*********************************************************************
*
*       OhEpGlobDeleteAllPendingTD
*
*  Function description
*    Clears all TDs. The TD is deleted. ED Tailp = EdHeadP stops from
*    TD processing if the skip flag is reset. This saves also the last
*    goggle flag in HeadP.
*/
void OhEpGlobDeleteAllPendingTD(HCM_ITEM_HEADER * EpHeader, U32 * TdCounter) {
  U32                 v;
  PDLIST              entry, head;
  HCM_ITEM_HEADER   * item;
  OHCI_ED           * Ed;
  U32 td_counter  = * TdCounter;
  if (td_counter > 1) {
#if (USBH_DEBUG > 1)
    // Active Transfer descriptors, this excludes the default TD
    if (!(OhEpGlobIsSkipped(EpHeader) || OhEpGlobIsHalt(EpHeader))) {
      USBH_WARN((USBH_MTYPE_OHCI, "OHCI: OhEpGlobDeleteAllPendingTD: ED not halted or skipped!"));
    // If the endpoint list the disabled then this is not an error
    }
#endif
    // Do not delete the default TD
    Ed = EpHeader->VirtAddr;
    // Stop TD processing by setting HeadP = TailP, save always toggle carry bit and halt bit
    v          = Ed->TailP;
    v         |= Ed->HeadP & 0xf;
    Ed->HeadP  = v;
    // Remove from head, the default TD remains the same
    USBH_LOG((USBH_MTYPE_OHCI, "OHCI: OhEpGlobInsertTD: tailp=headp phy Addr: %lu!",Ed->HeadP & 0x0fffffff0));
    head = &EpHeader->Link.ListEntry;
    while (td_counter > 1) {
      T_ASSERT(!DlistEmpty(head));
      DlistRemoveHead(head, &entry);
      item = GET_HCM_ITEM_HEADER_FROM_ENTRY(entry);
      HcmPutItem(item);
      td_counter--;
    }
    T_ASSERT(!DlistEmpty(head)); // The default TD is always available
    * TdCounter = td_counter;
  }
}

/*********************************************************************
*
*       OhEpGlobLinkEds
*
*  Function description
*    Inserts an new endpoint to the endpoint list.
*    The next field of the last endpoint must be valid!!!
*/
void OhEpGlobLinkEds(HCM_ITEM_HEADER * Last, HCM_ITEM_HEADER * New) {
  OHCI_ED        * LastTD;
  OHCI_ED        * NewTD;
  T_ASSERT(Last != NULL);
  T_ASSERT(New  != NULL);
  LastTD         = Last->VirtAddr;
  NewTD          = New->VirtAddr;
  NewTD->NextED  = LastTD->NextED;
  LastTD->NextED = New->PhyAddr;
  {
    U32 LastAddr = (U32)Last->PhyAddr;    
    USBH_LOG((USBH_MTYPE_OHCI, "OHCI: OhEpGlobLinkEds: last Ed phy Addr: 0x%lx new Ed phy Addr: 0x%lx!", LastAddr, New->PhyAddr ));
  }
}

/*********************************************************************
*
*       OhEpGlobUnlinkEd
*
*  Function description
*    Removes an endpoint from the HC endpoint list if not the first
*    endpoint in the list.
*  Parameters:
*    Prev:   Itemheader to the previous endpoint, if the removed endpoint
*            is the fist endpoint in the list then Prev can be NULL.
*    Remove: Endpoint to be removed must always unequal zero
*
*  Return value:
*    phy.Address of the ED NEXT field.
*/
U32 OhEpGlobUnlinkEd(HCM_ITEM_HEADER * Prev, HCM_ITEM_HEADER * Remove) {
  OHCI_ED  * PrevEd;
  OHCI_ED  * RemoveEd;
  RemoveEd = Remove->VirtAddr;
  if (Prev == NULL) {
    HCM_ASSERT_ITEM_HEADER(Remove);
    USBH_LOG((USBH_MTYPE_OHCI, "OHCI: OhEpGlobUnlinkEd: Prev:NULL next: phy Addr: %lu!", RemoveEd->NextED));
    return RemoveEd->NextED;
  } else { // Unlink Ed
    HCM_ASSERT_ITEM_HEADER(Remove);
    HCM_ASSERT_ITEM_HEADER(Prev);
    PrevEd         = Prev->VirtAddr;
    // Only this operation is possible to remove an endpoint from an list if the
    // list processing is not stopped, the endpoint must be valid until the next frame is over
    PrevEd->NextED = RemoveEd->NextED;
    return RemoveEd->NextED;
  }
}

/*********************************************************************
*
*       OhEpGlobRemoveAllTDtoPool
*
*  Function description
*    Remove all TDs (also the dummy TD) from list anputt them back to the pool
*/
void OhEpGlobRemoveAllTDtoPool(HCM_ITEM_HEADER * EpHeader, U32 * TdCounter) {
  PDLIST            entry;
  PDLIST            head;
  HCM_ITEM_HEADER * item;
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: OhEpGlobRemoveAllTDtoPool!"));
  HCM_ASSERT_ITEM_HEADER(EpHeader);
  head = &EpHeader->Link.ListEntry;
  while (!DlistEmpty(head)) {
    DlistRemoveTail(head, &entry); // Remove the dlist element from pool and put it back to the pool
    item = GET_HCM_ITEM_HEADER_FROM_ENTRY(entry);
    HcmPutItem(item);
  }
  * TdCounter = 0;                 // Set ref.couter to zero
}

/*********************************************************************
*
*       OhEpGlobIsTDActive
*
*  Function description
*    Return TRUE if the TD list active
*/
int OhEpGlobIsTDActive(HCM_ITEM_HEADER * EpHeader) {
  U32        v;
  OHCI_ED  * ed;
  ed       = EpHeader->VirtAddr;
  v        = ed->TailP;
  v       ^= ed->HeadP;
  v       &= 0x0fffffff0;
  if (v) {
    return TRUE;
  }
  return FALSE;
}

/*********************************************************************
*
*       OhEpGlobGetLastTDFromED
*
*  Function description
*    Returns the last Td fro the dlist this is the dummy TD
*/
OHD_GTD * OhEpGlobGetLastTDFromED(HCM_ITEM_HEADER * EpHeader) {
  PDLIST            entry;
  HCM_ITEM_HEADER * item;
  if (DlistEmpty(&EpHeader->Link.ListEntry)) {
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: OhEpGlobGetLastTDFromED: DList is empty!"));
    return NULL;
  }
  entry = DlistGetPrev(&EpHeader->Link.ListEntry);
  item  = GET_HCM_ITEM_HEADER_FROM_ENTRY(entry);
  HCM_ASSERT_ITEM_HEADER(item);
  // Cast the start address of the TD
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: OhEpGlobGetLastTDFromED phy TD addr: 0x%lx!", item->PhyAddr));
  return (OHD_GTD *)item;
}

/*********************************************************************
*
*       OhEpGlobGetFirstTDFromED
*
*  Function description
*    Returns the last submitted TD
*/
OHD_GTD * OhEpGlobGetFirstTDFromED(HCM_ITEM_HEADER * EpHeader) {
  PDLIST            entry;
  HCM_ITEM_HEADER * item;
  if (DlistEmpty(&EpHeader->Link.ListEntry)) {
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: OhEpGlobGetFirstTDFromED: DList is empty!"));
    return NULL;
  }
  entry = DlistGetNext(&EpHeader->Link.ListEntry);
  item  = GET_HCM_ITEM_HEADER_FROM_ENTRY(entry);
  // Cast the start address of the TD
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: OhEpGlobGetFirstTDFromED phy TD addr: 0x%lx!",item->PhyAddr));
  return (OHD_GTD * )item;
}

/*********************************************************************
*
*       OhEpGlobDeleteDoneTD
*
*  Function description
*    Removes an TD from the Ep list
*/
void OhEpGlobDeleteDoneTD(HCM_ITEM_HEADER * TdItem, U32 * TdCounter) {
  T_ASSERT(!DlistEmpty(&TdItem->Link.ListEntry));
  T_ASSERT((* TdCounter) != 0);
  DlistRemoveEntry(&TdItem->Link.ListEntry);
  * TdCounter = (* TdCounter)-1; // dec. Refcounter
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: OhEpGlobDeleteDoneTD phy TD addr: 0x%lx !",TdItem->PhyAddr));
  HcmPutItem(TdItem);
}

/*********************************************************************
*
*       OhEpGlobClearSkip
*
*  Function description
*    Clear the Skip Bit
*/
void OhEpGlobClearSkip(HCM_ITEM_HEADER * EpHeader) {
  OHCI_ED * Ed;
  U32       dword0;
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: OhEpGlobClearSkip!"));
  Ed      = EpHeader->VirtAddr;
  dword0  = Ed->Dword0;
  if (dword0 & OHCI_ED_K) {
    dword0     &= ~OHCI_ED_K;
    Ed->Dword0  = dword0;
  }
}

/*********************************************************************
*
*       OhEpGlobSetSkip
*
*  Function description
*    Sets the skip bit of any CPU
*/
void OhEpGlobSetSkip(HCM_ITEM_HEADER * EpHeader) {
  OHCI_ED * Ed;
  U32       dword0;
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: OhEpGlobSetSkip!"));
  Ed          = EpHeader->VirtAddr;
  dword0      = Ed->Dword0;
  dword0     |= OHCI_ED_K;
  Ed->Dword0  = dword0;
}

/*********************************************************************
*
*       OhEpGlobSetSkip
*
*  Function description
*    Returns TRUE if the EP skip bit is on
*/
int OhEpGlobIsSkipped(HCM_ITEM_HEADER * EpHeader) {
  OHCI_ED * Ed;
  Ed      = EpHeader->VirtAddr;
  if (Ed->Dword0 & OHCI_ED_K) {
    return TRUE;
  } else {
    return FALSE;
  }
}

/*********************************************************************
*
*       OhEpGlobClearHalt
*
*  Function description
*/
void OhEpGlobClearHalt(HCM_ITEM_HEADER * EpHeader) {
  OHCI_ED * Ed;
  U32       val;
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: OhEpGlobClearHalt!"));
  Ed      = EpHeader->VirtAddr;
  val     = Ed->HeadP;
  if (val & OHCI_ED_H) {
    Ed->HeadP = val &( ~OHCI_ED_H);
  }
}

/*********************************************************************
*
*       OhEpGlobIsHalt
*
*  Function description
*    Returns true if the host has halted the endpoint. This function
*    does not check if the endpoint is halted!
*/
int OhEpGlobIsHalt(HCM_ITEM_HEADER * EpHeader) {
  OHCI_ED * Ed;
  Ed      = EpHeader->VirtAddr;
  if (Ed->HeadP & OHCI_ED_H) {
    return TRUE;
  } else {
    return FALSE;
  }
}

/*********************************************************************
*
*       OhEpClearToggle
*
*  Function description
*    Clear the endpoint toggleCarry bit
*/
void OhEpClearToggle(HCM_ITEM_HEADER * EpHeader) {
  OHCI_ED * Ed;
  U32       val;
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: OhEpClearToggle!"));
  Ed      = EpHeader->VirtAddr;
  val     = Ed->HeadP;
  if (val & OHCI_ED_C) {
    val       &= ~OHCI_ED_C;
    Ed->HeadP  = val;
  }
}

/*********************************************************************
*
*       OhEpSetToggle
*
*  Function description
*/
void OhEpSetToggle(HCM_ITEM_HEADER * EpHeader, T_BOOL Toggle) {
  OHCI_ED * Ed;
  U32       val;
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: OhEpClearToggle!"));
  Ed  = EpHeader->VirtAddr;
  val = Ed->HeadP;
  if (Toggle) {
    val |= OHCI_ED_C;
  } else {
    val &= ~OHCI_ED_C;
  }
  Ed->HeadP = val;
}

/*********************************************************************
*
*       OhEpGetToggle
*
*  Function description
*/
T_BOOL OhEpGetToggle(HCM_ITEM_HEADER * EpHeader) {
  OHCI_ED * Ed;
  U32       val;
  T_BOOL    toggle;
  Ed      = EpHeader->VirtAddr;
  val     = Ed->HeadP;
  if (val & OHCI_ED_C) {
    toggle = TRUE;
  } else {
    toggle = FALSE;
  }
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: OhEpGetToggle toggle:%d!",toggle));
  return toggle;
}

/*********************************************************************
*
*       OhEpGlobGetTdCount
*
*  Function description
*    Returns the number of TD of an ED. The endpoint must be halted!
*/
U32 OhEpGlobGetTdCount(HCM_ITEM_HEADER * EpHeader, HCM_POOL * TdPool) {
  OHCI_ED         * ed;
  OHCI_TD         * td;
  U32               tailp, headp;
  HCM_ITEM_HEADER * item;
  int               ct = 0;
  ed              = EpHeader->VirtAddr;
  tailp           = ed->TailP & 0x0fffffff0;
  headp           = ed->HeadP & 0x0fffffff0;
  if (tailp != headp) {
    do { // Walk through the single linked list, stop if the last element is equal the tailp pointer
      ct++;
      item = HcmGetItemFromPhyAddr(TdPool, headp);
      if (NULL == item) { // On error
        break;
      }
      td    = item->VirtAddr;
      headp = td->NextTD;
    } while (item->PhyAddr != tailp); // Until the tailp Transfer descriptor is found
    return ct;
  } else {
    if (tailp != 0) {
      return 1;
    } else {
      return 0;
    }
  }
}

/******************************* EOF ********************************/
