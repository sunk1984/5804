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
*       _EpGlobBuildEDword0
*
*  Function description
*    Builds DWORD 0 of the OHCI endpoint descriptor
*/
static U32 _EpGlobBuildEDword0(U8 DeviceAddr, U8 EpWithDirection, U32 MaxPktSize, USBH_BOOL SkipFlag, USBH_BOOL IsoFlag, USBH_SPEED Speed) {
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
*       USBH_OHCI_EpGlobInitED
*
*  Function description
*    Changes all ED fields. Initializes ED Dword0. This call is only
*    allowed if the ED is not in the list or has been stopped!
*/
void USBH_OHCI_EpGlobInitED(USBH_HCM_ITEM_HEADER * Header, U8 DeviceAddr, U8 EpWithDirection, U32 MaxPktSize, USBH_BOOL IsoFlag, USBH_BOOL SkipFlag, USBH_SPEED Speed) {
  USBH_OHCI_ED    * ed;
  ed         = (USBH_OHCI_ED *)Header->PhyAddr;
  ed->Dword0 = _EpGlobBuildEDword0(DeviceAddr, EpWithDirection, MaxPktSize, SkipFlag, IsoFlag, Speed);
  ed->HeadP  = ed->NextED = ed->TailP = 0; // Set next ED pointer and Pointers to Transfer descriptors to NULL
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: USBH_OHCI_EpGlobInitED: device addr: %d  Ep: %d  MaxPktSize: %lu dword0: 0x%lx", DeviceAddr, EpWithDirection, MaxPktSize, ed->Dword0));
}

/*********************************************************************
*
*       USBH_OHCI_EpGlobInsertTD
*
*  Function description
*    Insert an new TD in the ED TD list and inserts this TD also in
*    the logical link list. Adds an new TD to the endpoints TD list head.
*/
void USBH_OHCI_EpGlobInsertTD(USBH_HCM_ITEM_HEADER * EpHeader, USBH_HCM_ITEM_HEADER * NewTdHeader, U16 * pTdCounter) {
  USBH_OHCI_ED         * ed;
  USBH_OHCI_TRANSFER_DESC         * td;
  USBH_HCM_ITEM_HEADER * prevTdHeader;
  USBH_DLIST           * pEntry;

  ed              = (USBH_OHCI_ED *)EpHeader->PhyAddr;
  if (USBH_DLIST_IsEmpty(&EpHeader->Link.ListEntry)) { // Physical link
    // Insert the first TD
    USBH_LOG((USBH_MTYPE_OHCI, "OHCI: USBH_OHCI_EpGlobInsertTD: First phy. TD Addr: 0x%lx!",NewTdHeader->PhyAddr));
    ed->TailP = NewTdHeader->PhyAddr;
    ed->HeadP = NewTdHeader->PhyAddr; // Clears indirect the HALT condition and the toggle carry bit
  } else { // Insert other TDs
    pEntry = USBH_DLIST_GetPrev(&EpHeader->Link.ListEntry);
    prevTdHeader = GET_HCMITEM_FROM_ENTRY(pEntry);
    USBH_LOG((USBH_MTYPE_OHCI, "OHCI: USBH_OHCI_EpGlobInsertTD: phy TD Addr: 0x%lx!",NewTdHeader->PhyAddr));
    // Set next pointer
    td = (USBH_OHCI_TRANSFER_DESC *)prevTdHeader->PhyAddr;
    td->NextTD = NewTdHeader->PhyAddr;
    // Set the ED tail pointer
    ed->TailP = NewTdHeader->PhyAddr;
  }
  // Logical link
  *pTdCounter = (*pTdCounter) + 1;
  USBH_DLIST_InsertTail(&EpHeader->Link.ListEntry, &NewTdHeader->Link.ListEntry);
}

/*********************************************************************
*
*       USBH_OHCI_EpGlobDeleteAllPendingTD
*
*  Function description
*    Clears all TDs. The TD is deleted. ED Tailp = EdHeadP stops from
*    TD processing if the skip flag is reset. This saves also the last
*    goggle flag in HeadP.
*/
void USBH_OHCI_EpGlobDeleteAllPendingTD(USBH_HCM_ITEM_HEADER * EpHeader, U16 * pTdCounter) {
  U32                 v;
  USBH_DLIST             * pEntry;
  USBH_DLIST             * pHead;
  USBH_HCM_ITEM_HEADER   * pItem;
  USBH_OHCI_ED           * Ed;
  U16                      TDCounter;
  TDCounter = *pTdCounter;
  if (TDCounter > 1) {
#if (USBH_DEBUG > 1)
    // Active Transfer descriptors, this excludes the default TD
    if (!(USBH_OHCI_EpGlobIsSkipped(EpHeader) || USBH_OHCI_EpGlobIsHalt(EpHeader))) {
      USBH_WARN((USBH_MTYPE_OHCI, "OHCI: USBH_OHCI_EpGlobDeleteAllPendingTD: ED not halted or skipped!"));
    // If the endpoint list the disabled then this is not an error
    }
#endif
    // Do not delete the default TD
    Ed = (USBH_OHCI_ED *)EpHeader->PhyAddr;
    // Stop TD processing by setting HeadP = TailP, save always toggle carry bit and halt bit
    v          = Ed->TailP;
    v         |= Ed->HeadP & 0xf;
    Ed->HeadP  = v;
    // Remove from head, the default TD remains the same
    USBH_LOG((USBH_MTYPE_OHCI, "OHCI: USBH_OHCI_EpGlobInsertTD: tailp=headp phy Addr: %lu!",Ed->HeadP & 0x0fffffff0));
    pHead = &EpHeader->Link.ListEntry;
    while (TDCounter > 1) {
      USBH_ASSERT(!USBH_DLIST_IsEmpty(pHead));
      USBH_DLIST_RemoveHead(pHead, &pEntry);
      pItem = GET_HCM_ITEM_HEADER_FROM_ENTRY(pEntry);
      USBH_HCM_PutItem(pItem);
      TDCounter--;
    }
    USBH_ASSERT(!USBH_DLIST_IsEmpty(pHead)); // The default TD is always available
    *pTdCounter = TDCounter;
  }
}

/*********************************************************************
*
*       USBH_OHCI_EpGlobLinkEds
*
*  Function description
*    Inserts a new endpoint to the endpoint list.
*    The next field of the last endpoint must be valid!!!
*/
void USBH_OHCI_EpGlobLinkEds(USBH_HCM_ITEM_HEADER * pLast, USBH_HCM_ITEM_HEADER * pNew) {
  USBH_OHCI_ED        * pLastED;
  USBH_OHCI_ED        * pNewED;

  USBH_ASSERT(pLast != NULL);
  USBH_ASSERT(pNew  != NULL);
  pLastED         = (USBH_OHCI_ED *)pLast->PhyAddr;
  pNewED          = (USBH_OHCI_ED *)pNew->PhyAddr;
  pNewED->NextED  = pLastED->NextED;
  pLastED->NextED = pNew->PhyAddr;
  {
    U32 LastAddr = (U32)pLast->PhyAddr;
    USBH_LOG((USBH_MTYPE_OHCI, "OHCI: USBH_OHCI_EpGlobLinkEds: last Ed phy Addr: 0x%lx new Ed phy Addr: 0x%lx!", LastAddr, pNew->PhyAddr ));
  }
}

/*********************************************************************
*
*       USBH_OHCI_EpGlobUnlinkEd
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
U32 USBH_OHCI_EpGlobUnlinkEd(USBH_HCM_ITEM_HEADER * Prev, USBH_HCM_ITEM_HEADER * Remove) {
  USBH_OHCI_ED  * PrevEd;
  USBH_OHCI_ED  * RemoveEd;

  RemoveEd = (USBH_OHCI_ED *)Remove->PhyAddr;
  if (Prev == NULL) {
    USBH_HCM_ASSERT_ITEM_HEADER(Remove);
    USBH_LOG((USBH_MTYPE_OHCI, "OHCI: USBH_OHCI_EpGlobUnlinkEd: Prev:NULL next: phy Addr: %lu!", RemoveEd->NextED));
    return RemoveEd->NextED;
  } else { // Unlink Ed
    USBH_HCM_ASSERT_ITEM_HEADER(Remove);
    USBH_HCM_ASSERT_ITEM_HEADER(Prev);
    PrevEd         = (USBH_OHCI_ED *)Prev->PhyAddr;
    // Only this operation is possible to remove an endpoint from an list if the
    // list processing is not stopped, the endpoint must be valid until the next frame is over
    PrevEd->NextED = RemoveEd->NextED;
    return RemoveEd->NextED;
  }
}

/*********************************************************************
*
*       USBH_OHCI_EpGlobRemoveAllTDtoPool
*
*  Function description
*    Remove all TDs (also the dummy TD) from list and put them back to the pool
*/
void USBH_OHCI_EpGlobRemoveAllTDtoPool(USBH_HCM_ITEM_HEADER * pEpHeader, U16 * pTdCounter) {
  USBH_DLIST           * pEntry;
  USBH_DLIST           * pHead;
  USBH_HCM_ITEM_HEADER * item;

  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: USBH_OHCI_EpGlobRemoveAllTDtoPool!"));
  USBH_HCM_ASSERT_ITEM_HEADER(pEpHeader);
  pHead = &pEpHeader->Link.ListEntry;
  while (!USBH_DLIST_IsEmpty(pHead)) {
    USBH_DLIST_RemoveTail(pHead, &pEntry); // Remove the dlist element from pool and put it back to the pool
    item = GET_HCM_ITEM_HEADER_FROM_ENTRY(pEntry);
    USBH_HCM_PutItem(item);
  }
  *pTdCounter = 0;                 // Set ref.couter to zero
}

/*********************************************************************
*
*       USBH_OHCI_EpGlobIsTDActive
*
*  Function description
*    Return TRUE if the TD list active
*/
int USBH_OHCI_EpGlobIsTDActive(USBH_HCM_ITEM_HEADER * EpHeader) {
  U32        v;
  USBH_OHCI_ED  * ed;
  ed       = (USBH_OHCI_ED *)EpHeader->PhyAddr;
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
*       USBH_OHCI_EpGlobGetLastTDFromED
*
*  Function description
*    Returns the last Td fro the dlist this is the dummy TD
*/
USBH_OHCI_INFO_GENERAL_TRANS_DESC * USBH_OHCI_EpGlobGetLastTDFromED(USBH_HCM_ITEM_HEADER * EpHeader) {
  USBH_DLIST           * pEntry;
  USBH_HCM_ITEM_HEADER * pItem;
  if (USBH_DLIST_IsEmpty(&EpHeader->Link.ListEntry)) {
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: USBH_OHCI_EpGlobGetLastTDFromED: DList is empty!"));
    return NULL;
  }
  pEntry = USBH_DLIST_GetPrev(&EpHeader->Link.ListEntry);
  pItem  = GET_HCM_ITEM_HEADER_FROM_ENTRY(pEntry);
  USBH_HCM_ASSERT_ITEM_HEADER(pItem);
  // Cast the start address of the TD
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: USBH_OHCI_EpGlobGetLastTDFromED phy TD addr: 0x%lx!", pItem->PhyAddr));
  return (USBH_OHCI_INFO_GENERAL_TRANS_DESC *)pItem;
}

/*********************************************************************
*
*       USBH_OHCI_EpGlobGetFirstTDFromED
*
*  Function description
*    Returns the last submitted TD
*/
USBH_OHCI_INFO_GENERAL_TRANS_DESC * USBH_OHCI_EpGlobGetFirstTDFromED(USBH_HCM_ITEM_HEADER * EpHeader) {
  USBH_DLIST           * pEntry;
  USBH_HCM_ITEM_HEADER * pItem;
  if (USBH_DLIST_IsEmpty(&EpHeader->Link.ListEntry)) {
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: USBH_OHCI_EpGlobGetFirstTDFromED: DList is empty!"));
    return NULL;
  }
  pEntry = USBH_DLIST_GetNext(&EpHeader->Link.ListEntry);
  pItem  = GET_HCM_ITEM_HEADER_FROM_ENTRY(pEntry);
  // Cast the start address of the TD
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: USBH_OHCI_EpGlobGetFirstTDFromED phy TD addr: 0x%lx!",pItem->PhyAddr));
  return (USBH_OHCI_INFO_GENERAL_TRANS_DESC * )pItem;
}

/*********************************************************************
*
*       USBH_OHCI_EpGlobDeleteDoneTD
*
*  Function description
*    Removes an TD from the Ep list
*/
void USBH_OHCI_EpGlobDeleteDoneTD(USBH_HCM_ITEM_HEADER * pTdItem, U16 * pTdCounter) {
  USBH_ASSERT(!USBH_DLIST_IsEmpty(&pTdItem->Link.ListEntry));
  USBH_ASSERT((*pTdCounter) != 0);
  USBH_DLIST_RemoveEntry(&pTdItem->Link.ListEntry);
  *pTdCounter = (* pTdCounter)-1; // dec. Refcounter
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: USBH_OHCI_EpGlobDeleteDoneTD phy TD addr: 0x%lx !",pTdItem->PhyAddr));
  USBH_HCM_PutItem(pTdItem);
}

/*********************************************************************
*
*       USBH_OHCI_EpGlobClearSkip
*
*  Function description
*    Clear the Skip Bit
*/
void USBH_OHCI_EpGlobClearSkip(USBH_HCM_ITEM_HEADER * EpHeader) {
  USBH_OHCI_ED * Ed;
  U32       dword0;
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: USBH_OHCI_EpGlobClearSkip!"));
  Ed      = (USBH_OHCI_ED *)EpHeader->PhyAddr;
  dword0  = Ed->Dword0;
  if (dword0 & OHCI_ED_K) {
    dword0     &= ~OHCI_ED_K;
    Ed->Dword0  = dword0;
  }
}

/*********************************************************************
*
*       USBH_OHCI_EpGlobSetSkip
*
*  Function description
*    Sets the skip bit of any CPU
*/
void USBH_OHCI_EpGlobSetSkip(USBH_HCM_ITEM_HEADER * EpHeader) {
  USBH_OHCI_ED * Ed;
  U32       dword0;
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: USBH_OHCI_EpGlobSetSkip!"));
  Ed          = (USBH_OHCI_ED *)EpHeader->PhyAddr;
  dword0      = Ed->Dword0;
  dword0     |= OHCI_ED_K;
  Ed->Dword0  = dword0;
}

/*********************************************************************
*
*       USBH_OHCI_EpGlobSetSkip
*
*  Function description
*    Returns TRUE if the EP skip bit is on
*/
int USBH_OHCI_EpGlobIsSkipped(USBH_HCM_ITEM_HEADER * EpHeader) {
  USBH_OHCI_ED * Ed;
  Ed      = (USBH_OHCI_ED *)EpHeader->PhyAddr;
  if (Ed->Dword0 & OHCI_ED_K) {
    return TRUE;
  } else {
    return FALSE;
  }
}

/*********************************************************************
*
*       USBH_OHCI_EpGlobClearHalt
*
*  Function description
*/
void USBH_OHCI_EpGlobClearHalt(USBH_HCM_ITEM_HEADER * EpHeader) {
  USBH_OHCI_ED * Ed;
  U32       val;

  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: USBH_OHCI_EpGlobClearHalt!"));
  Ed      = (USBH_OHCI_ED *)EpHeader->PhyAddr;
  val     = Ed->HeadP;
  if (val & OHCI_ED_H) {
    Ed->HeadP = val &( ~OHCI_ED_H);
  }
}

/*********************************************************************
*
*       USBH_OHCI_EpGlobIsHalt
*
*  Function description
*    Returns true if the host has halted the endpoint. This function
*    does not check if the endpoint is halted!
*/
int USBH_OHCI_EpGlobIsHalt(USBH_HCM_ITEM_HEADER * EpHeader) {
  USBH_OHCI_ED * Ed;

  Ed      = (USBH_OHCI_ED *)EpHeader->PhyAddr;
  if (Ed->HeadP & OHCI_ED_H) {
    return TRUE;
  } else {
    return FALSE;
  }
}

/*********************************************************************
*
*       USBH_OHCI_EpClearToggle
*
*  Function description
*    Clear the endpoint toggleCarry bit
*/
void USBH_OHCI_EpClearToggle(USBH_HCM_ITEM_HEADER * EpHeader) {
  USBH_OHCI_ED * Ed;
  U32       val;

  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: USBH_OHCI_EpClearToggle!"));
  Ed      = (USBH_OHCI_ED *)EpHeader->PhyAddr;
  val     = Ed->HeadP;
  if (val & OHCI_ED_C) {
    val       &= ~OHCI_ED_C;
    Ed->HeadP  = val;
  }
}

/*********************************************************************
*
*       USBH_OHCI_EpSetToggle
*
*  Function description
*/
void USBH_OHCI_EpSetToggle(USBH_HCM_ITEM_HEADER * EpHeader, USBH_BOOL Toggle) {
  USBH_OHCI_ED * Ed;
  U32       val;

  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: USBH_OHCI_EpClearToggle!"));
  Ed  = (USBH_OHCI_ED *)EpHeader->PhyAddr;
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
*       USBH_OHCI_EpGetToggle
*
*  Function description
*/
USBH_BOOL USBH_OHCI_EpGetToggle(USBH_HCM_ITEM_HEADER * EpHeader) {
  USBH_OHCI_ED * Ed;
  U32       val;
  USBH_BOOL    toggle;

  Ed      = (USBH_OHCI_ED *)EpHeader->PhyAddr;
  val     = Ed->HeadP;
  if (val & OHCI_ED_C) {
    toggle = TRUE;
  } else {
    toggle = FALSE;
  }
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: USBH_OHCI_EpGetToggle toggle:%d!",toggle));
  return toggle;
}

/*********************************************************************
*
*       USBH_OHCI_EpGlobGetTdCount
*
*  Function description
*    Returns the number of TD of an ED. The endpoint must be halted!
*/
U32 USBH_OHCI_EpGlobGetTdCount(USBH_HCM_ITEM_HEADER * EpHeader, USBH_HCM_POOL * TdPool) {
  USBH_OHCI_ED         * ed;
  USBH_OHCI_TRANSFER_DESC         * td;
  USBH_HCM_ITEM_HEADER * item;
  U32                        tailp;
  U32                        headp;
  int               ct = 0;

  ed              = (USBH_OHCI_ED *)EpHeader->PhyAddr;
  tailp           = ed->TailP & 0x0fffffff0;
  headp           = ed->HeadP & 0x0fffffff0;
  if (tailp != headp) {
    do { // Walk through the single linked list, stop if the last element is equal the tailp pointer
      ct++;
      item = USBH_HCM_GetItemFromPhyAddr(TdPool, headp);
      if (NULL == item) { // On error
        break;
      }
      td    = (USBH_OHCI_TRANSFER_DESC *)item->PhyAddr;
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
