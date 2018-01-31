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
*       OhHccaAlloc
*
*  Function description
*    Allocates memory for the 256 byte aligned HCCA and writes the
*    physical base address of the HCCA memory to the host register HcHCCA.
*
*  Return:
*    Pointer to the pool item
*    NULL: no memory
*/
OHD_HCCA * OhHccaAlloc(HCM_POOL * pool) {
  USBH_STATUS       status;
  HCM_ITEM_HEADER * itemHeader;
  OHD_HCCA        * hcdHCCA;
  T_ASSERT(pool != NULL);
  status = HcmAllocPool(pool, 1, OH_HCCA_LENGTH, sizeof(OHD_HCCA), OH_HCCA_ALIGNMENT);
  if (status) {             // On error
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: OhHccaInit: HcmAllocPool!"));
    return NULL;
  }
  itemHeader = HcmGetItem(pool);
  if (NULL == itemHeader) { // On error
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: OhHccaInit: HcmGetItem!"));
    hcdHCCA = NULL;
  } else {
    T_ASSERT(TB_IS_ALIGNED(itemHeader->PhyAddr, OH_HCCA_ALIGNMENT));
    // Clear all item bytes
    HcmFillPhyMemory(itemHeader, 0);
    hcdHCCA = (OHD_HCCA *)itemHeader;
  }
  return hcdHCCA;
}

/*********************************************************************
*
*       OhHccaRelease
*
*  Function description
*/
void OhHccaRelease(OHD_HCCA * OhdHcca) {
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: OhHccaRelease!"));
  if (OhdHcca == NULL) {
    return;
  }
  OH_HCCA_VALID(OhdHcca);
  HcmPutItem  (&OhdHcca->ItemHeader);
  HcmFreePool  (OhdHcca->ItemHeader.OwningPool);
}

/*********************************************************************
*
*       OhHccaRelease
*
*  Function description
*    Initializes the HCCA interrupt list to all used 32ms interrupt
*    endpoints. HCCA list will not be enabled!
*/
void OhHccaSetInterruptTable(OHD_HCCA * OhdHcca, OHD_DUMMY_INT_EP * dummyInterruptEndpointList[]) {
  int                i, tableIdx;
  volatile U32     * intTable;
  OHD_DUMMY_INT_EP * dummyEp;
  intTable = ((OHCI_HCCA *)OhdHcca->ItemHeader.VirtAddr)->InterruptTable;
  for (i = 0, tableIdx = 0; i < 32; i++) {
    // gHccaIntFrameBalance determines interval. The second value of the interval is calculated from the previous divide by two
    if (i & 1) {
      tableIdx += 0x10;
    } else {
      T_ASSERT((i >> 1) < ARRAY_ELEMENTS(gHccaIntFrameBalance));
      tableIdx = gHccaIntFrameBalance[i >> 1];
    }
    dummyEp = dummyInterruptEndpointList[31 + i];
    HCM_ASSERT_ITEM_HEADER(&dummyEp->ItemHeader);
    USBH_LOG((USBH_MTYPE_OHCI, "OHCI: OhHccaSetInterruptTable: Frame number:%02x ED interval: %02d ED phyAddr. 0x%x!",
              tableIdx, dummyEp->IntervalTime, dummyEp->ItemHeader.PhyAddr));
    T_ASSERT(tableIdx < ARRAY_ELEMENTS(((OHCI_HCCA * )OhdHcca->ItemHeader.VirtAddr)->InterruptTable));
    intTable[tableIdx] = dummyEp->ItemHeader.PhyAddr;
  }
}


/******************************* EOF ********************************/
