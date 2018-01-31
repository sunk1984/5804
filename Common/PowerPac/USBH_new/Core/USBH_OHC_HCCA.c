/*********************************************************************
*  
*   IAR PowerPac
*
*   (c) Copyright IAR Systems 2010.  All rights reserved.
*
**********************************************************************
----------------------------------------------------------------------
File        : USBH_OHC_HCCA.c
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
*       #define constants
*
**********************************************************************
*/

// This array contains frame bits 0..5 (0..0x1f)  to balance the interrupt tree
const U8 gHccaIntFrameBalance[16] = {
  0x00, 0x08, 0x04, 0x0C, 0x02, 0x0A, 0x06, 0x0E, 0x01, 0x09, 0x05, 0x0D, 0x03, 0x0B, 0x07, 0x0F
};

/*********************************************************************
*
*       USBH_OHCI_HccaAlloc
*
*  Function description
*    Allocates memory for the 256 byte aligned HCCA and writes the
*    physical base address of the HCCA memory to the host register HcHCCA.
*
*  Return:
*    Pointer to the pool item
*    NULL: no memory
*/
USBH_OHCI_HCCA * USBH_OHCI_HccaAlloc(USBH_HCM_POOL * pool) {
  USBH_STATUS       status;
  USBH_HCM_ITEM_HEADER * itemHeader;
  USBH_OHCI_HCCA        * hcdHCCA;
  USBH_ASSERT(pool != NULL);
  status = USBH_HCM_AllocPool(pool, 1, OH_HCCA_LENGTH, sizeof(USBH_OHCI_HCCA), OH_HCCA_ALIGNMENT);
  if (status) {             // On error
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: OhHccaInit: USBH_HCM_AllocPool!"));
    return NULL;
  }
  itemHeader = USBH_HCM_GetItem(pool);
  if (NULL == itemHeader) { // On error
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: OhHccaInit: USBH_HCM_GetItem!"));
    hcdHCCA = NULL;
  } else {
    USBH_ASSERT(USBH_IS_ALIGNED(itemHeader->PhyAddr, OH_HCCA_ALIGNMENT));
    // Clear all item bytes
    USBH_HCM_FillPhyMemory(itemHeader, 0);
    hcdHCCA = (USBH_OHCI_HCCA *)itemHeader;
  }
  return hcdHCCA;
}

/*********************************************************************
*
*       USBH_OHCI_HccaRelease
*
*  Function description
*/
void USBH_OHCI_HccaRelease(USBH_OHCI_HCCA * OhdHcca) {
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: USBH_OHCI_HccaRelease!"));
  if (OhdHcca == NULL) {
    return;
  }
  OH_HCCA_VALID(OhdHcca);
  USBH_HCM_PutItem  (&OhdHcca->ItemHeader);
  USBH_HCM_FreePool  (OhdHcca->ItemHeader.pOwningPool);
}

/*********************************************************************
*
*       USBH_OHCI_HccaSetInterruptTable
*
*  Function description
*    Initializes the HCCA interrupt list to all used 32ms interrupt
*    endpoints. HCCA list will not be enabled!
*/
void USBH_OHCI_HccaSetInterruptTable(USBH_OHCI_HCCA * OhdHcca, USBH_OHCI_DUMMY_INT_EP * dummyInterruptEndpointList[]) {
  int                i, tableIdx;
  volatile U32     * intTable;
  USBH_OHCI_DUMMY_INT_EP * dummyEp;
  intTable = ((USBH_OHCI_HCCA_REG *)OhdHcca->ItemHeader.PhyAddr)->InterruptTable;
  for (i = 0, tableIdx = 0; i < 32; i++) {
    // gHccaIntFrameBalance determines interval. The second value of the interval is calculated from the previous divide by two
    if (i & 1) {
      tableIdx += 0x10;
    } else {
      USBH_ASSERT((i >> 1) < USBH_ARRAY_ELEMENTS(gHccaIntFrameBalance));
      tableIdx = gHccaIntFrameBalance[i >> 1];
    }
    dummyEp = dummyInterruptEndpointList[31 + i];
    USBH_HCM_ASSERT_ITEM_HEADER(&dummyEp->ItemHeader);
    USBH_LOG((USBH_MTYPE_OHCI, "OHCI: USBH_OHCI_HccaSetInterruptTable: Frame number:%02x ED interval: %02d ED phyAddr. 0x%x!",
              tableIdx, dummyEp->IntervalTime, dummyEp->ItemHeader.PhyAddr));
    USBH_ASSERT(tableIdx < USBH_ARRAY_ELEMENTS(((USBH_OHCI_HCCA_REG * )OhdHcca->ItemHeader.PhyAddr)->InterruptTable));
    intTable[tableIdx] = dummyEp->ItemHeader.PhyAddr;
  }
}


/******************************* EOF ********************************/
