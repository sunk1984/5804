/*********************************************************************
*  
*   IAR PowerPac
*
*   (c) Copyright IAR Systems 2010.  All rights reserved.
*
**********************************************************************
----------------------------------------------------------------------
File        : USBH_OHC_Device.c
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
*       Devices helper functions
*
**********************************************************************
*/

/*********************************************************************
*
*       _InitHccaInterruptTable
*
*  Function description
*    Build an interrupt endpoint tree with dummy endpoints. The HCCA
*    interrupt table is initialized but not activated !!!
*/
static USBH_STATUS _InitHccaInterruptTable(USBH_OHCI_DEVICE * pDev) {
  USBH_STATUS   status;
  status      = USBH_OHCI_INT_InitAllocDummyIntEps(pDev);        // 1. Allocate all needed dummy interrupts
  if (status) {
    goto exit;
  }
  USBH_OHCI_INT_BuildDummyEpTree(pDev);                                   // 2. Links all dummy Eps in the devices DummyInterruptEpArr list.
  USBH_OHCI_HccaSetInterruptTable(pDev->pHcca, pDev->DummyInterruptEpArr); // 3. Initialize the HCCA interrupt table entries

exit:
  if (status) {
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: _InitHccaInterruptTable"));
  }
  return status;
}

/*********************************************************************
*
*       _EnableInterrupt
*
*  Function description
*    Global interrupt enable
*/
static void _EnableInterrupt(USBH_OHCI_DEVICE * pDev) {
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: Enable master INT!"));
  OhHalWriteReg(pDev->pRegBase, OH_REG_INTERRUPTENABLE, HC_INT_MIE); // Set the new state
}

/*********************************************************************
*
*       _DisableInterrupt
*
*  Function description
*    Global interrupt disable
*/
static void _DisableInterrupt(USBH_OHCI_DEVICE * pDev) {
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: Disable master INT!"));
  OhHalWriteReg(pDev->pRegBase, OH_REG_INTERRUPTDISABLE, HC_INT_MIE); // Set the new state
}

/*********************************************************************
*
*       _SetInterrupts
*
*  Function description
*    Enables interrupt bits
*/
static void _SetInterrupts(USBH_OHCI_DEVICE * pDev, U32 IntMask) {
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: OhdEnableInterruptBits: Mask: 0x%lx", IntMask));
  OhHalWriteReg(pDev->pRegBase, OH_REG_INTERRUPTENABLE, IntMask);
}

/*********************************************************************
*
*       _InitIntervalReg
*
*  Function description
*/
static void _InitIntervalReg(USBH_OHCI_DEVICE * pDev) {
  U32 temp;
  USBH_OCHI_IS_DEV_VALID(pDev);
  pDev->FmIntervalReg = OH_DEV_DEFAULT_FMINTERVAL; // Check if exist an interval register
  temp = OhHalReadReg(pDev->pRegBase, OH_REG_FMINTERVAL);
  // Toggle the Frame interval bit and write the new frame bit times
  temp ^= HC_FM_INTERVAL_FIT;
  temp &= HC_FM_INTERVAL_FIT;
  temp |= pDev->FmIntervalReg;
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: _InitIntervalReg: FmIntervalReg: 0x%lx", temp));
  OhHalWriteReg(pDev->pRegBase, OH_REG_FMINTERVAL, temp);
  // Calculate the periodic start bit time from frame interval
  temp = ((9 * (pDev->FmIntervalReg &HC_FM_INTERVAL_FI_MASK)) / 10) & 0x3fff;
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: _InitIntervalReg: OH_REG_PERIODICSTART: 0x%lu", temp));
  OhHalWriteReg(pDev->pRegBase, OH_REG_PERIODICSTART, temp);
  OhHalWriteReg(pDev->pRegBase, OH_REG_LSTHRESHOLD, OH_DEV_LOW_SPEED_THRESHOLD);
}

/*********************************************************************
*
*       _SetHcFuncState
*
*  Function description
*/
static USBH_STATUS _SetHcFuncState(USBH_OHCI_DEVICE * pDev, U32 HcFuncState) {
  USBH_STATUS status;
  U32         control;
  status   = USBH_STATUS_SUCCESS;
  control  = OhHalReadReg(pDev->pRegBase, OH_REG_CONTROL);
  control &= ~HC_CONTROL_HCFS;
  switch (HcFuncState) {
  case HC_USB_RESET:
    pDev->State = USBH_OHCI_DEV_STATE_HALT;
    USBH_LOG((USBH_MTYPE_OHCI, "OHCI: _SetHcFuncState: Reset!"));
    control |= HcFuncState;
    break;
  case HC_USB_RESUME:
    pDev->State = USBH_OHCI_DEV_STATE_RESUME;
    if (pDev->State != USBH_OHCI_DEV_STATE_SUSPEND) {
      USBH_WARN((USBH_MTYPE_OHCI, "OHCI: _SetHcFuncState: not suspend!"));
      status = USBH_STATUS_ERROR;
      break;
    }
    control |= HcFuncState;
    USBH_LOG((USBH_MTYPE_OHCI, "OHCI: _SetHcFuncState: Resume!"));
    break;
  case HC_USB_OPERATIONAL:
    pDev->State = USBH_OHCI_DEV_STATE_RUNNING;
    USBH_LOG((USBH_MTYPE_OHCI, "OHCI: _SetHcFuncState: new State: Operational"));
    control |= HcFuncState;
    break;
  case HC_USB_SUSPEND:
    pDev->State = USBH_OHCI_DEV_STATE_SUSPEND;
    if (pDev->State != USBH_OHCI_DEV_STATE_RUNNING) {
      USBH_WARN((USBH_MTYPE_OHCI, "OHCI: _SetHcFuncState: not running!"));
      status = USBH_STATUS_ERROR;
      break;
    }
    USBH_LOG((USBH_MTYPE_OHCI, "OHCI: _SetHcFuncState: Suspend"));
    control |= HcFuncState;
  default: // On suspend do nothing
    break;
  }
  OhHalWriteReg(pDev->pRegBase, OH_REG_CONTROL, control); // Set the new state
  return status;
}

/*********************************************************************
*
*       _SoftReset
*
*  Function description
*    OhdReset resets the host controller register. The host is then in suspend state
*/
static USBH_STATUS _SoftReset(USBH_OHCI_DEVICE * pDev) {
  U32         command, time, i, reg;
  USBH_STATUS status;
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: OhdReset"));
  USBH_OCHI_IS_DEV_VALID(pDev);
  reg = OhHalReadReg(pDev->pRegBase, OH_REG_FMINTERVAL);                      // Store the HcFmInterval
  _SetHcFuncState(pDev, HC_USB_RESET);                                     // Additional USB bus Reset
  time = USBH_OS_GetTime32();
  while (!USBH_IsTimeOver(OH_RESET_STATE_TIMEOUT, time));                     // Busy wait
  OhHalWriteReg(pDev->pRegBase, OH_REG_COMMANDSTATUS, HC_COMMAND_STATUS_HCR); // Set sotware reset
  status = USBH_STATUS_ERROR;
  i = OH_TIME_SOFTWARE_RESET;
  time = USBH_OS_GetTime32();
  do {
    while (!USBH_IsTimeOver(1, time));                                        // Wait 1us
    time = USBH_OS_GetTime32();
    command = OhHalReadReg(pDev->pRegBase, OH_REG_COMMANDSTATUS);
    if (0 == (command &HC_COMMAND_STATUS_HCR)) {
      status = USBH_STATUS_SUCCESS;                                         // This bit is cleared by HC upon the completion of the reset operation.
      break;
    }
  } while (--i);

#if (USBH_DEBUG > 1)

  if (status) {
    USBH_WARN((USBH_MTYPE_OHCI, " _SoftReset!"));
  }

#endif

  OhHalWriteReg(pDev->pRegBase, OH_REG_FMINTERVAL, reg);
  return status;
}

/*********************************************************************
*
*       _CheckRevision
*
*  Function description
*/
static USBH_STATUS _CheckRevision(USBH_OHCI_DEVICE * pDev) {
  U32   rev;
  rev = OhHalReadReg(pDev->pRegBase, OH_REG_REVISION);
  if ((rev &OH_REVISION_MASK) != OH_REVISION) {
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: _CheckRevision: Wrong OHCI revision exp.:0x%x rcv:0x%x", OH_REVISION, rev & OH_REVISION_MASK));
    return USBH_STATUS_ERROR;
  } else {
    USBH_LOG((USBH_MTYPE_OHCI, "OHCI: INFO _CheckRevision: OH_REG_REVISION content: 0x%x", rev)); // Prints out all values
    return USBH_STATUS_SUCCESS;
  }
}

/*********************************************************************
*
*       _AllocTimers
*
*  Function description
*/
static USBH_STATUS _AllocTimers(USBH_OHCI_DEVICE * pDev) {
  pDev->ControlEpRemoveTimer      = USBH_AllocTimer(USBH_OHCI_EP0_OnReleaseEpTimer, pDev); // Allocate all device object specific timer
  if (pDev->ControlEpRemoveTimer == NULL) {
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: _InitDLists: USBH_AllocTimer!"));
    return USBH_STATUS_RESOURCES;
  }
  pDev->ControlEpAbortTimer       = USBH_AllocTimer(USBH_OHCI_EP0_OnAbortUrbTimer, pDev);
  if (pDev->ControlEpAbortTimer  == NULL) {
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: _InitDLists: USBH_AllocTimer!"));
    return USBH_STATUS_RESOURCES;
  }
  pDev->hBulkEpRemoveTimer         = USBH_AllocTimer(USBH_OHCI_BULK_INT_OnRemoveEpTimer, pDev);
  if (pDev->hBulkEpRemoveTimer    == NULL) {
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: _InitDLists: USBH_AllocTimer!"));
    return USBH_STATUS_RESOURCES;
  }
  pDev->hBulkEpAbortTimer          = USBH_AllocTimer(USBH_OHCI_BULK_OnAbortUrbTimer, pDev);
  if (pDev->hBulkEpAbortTimer     == NULL) {
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: _InitDLists: USBH_AllocTimer!"));
    return USBH_STATUS_RESOURCES;
  }
  pDev->hIntEpRemoveTimer          = USBH_AllocTimer(USBH_OHCI_INT_OnReleaseEpTimer, pDev);
  if (pDev->hIntEpRemoveTimer     == NULL) {
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: _InitDLists: USBH_AllocTimer!"));
    return USBH_STATUS_RESOURCES;
  }
  pDev->hIntEpAbortTimer           = USBH_AllocTimer(USBH_OHCI_INT_OnAbortUrbTimer, pDev);
  if (pDev->hIntEpAbortTimer      == NULL) {
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: _InitDLists: USBH_AllocTimer!"));
    return USBH_STATUS_RESOURCES;
  }
#if USBH_SUPPORT_ISO_TRANSFER
  pDev->hIsoEpRemoveTimer         = USBH_AllocTimer(USBH_OHCI_ISO_OnRemoveEpTimer, pDev);
  if (pDev->hIsoEpRemoveTimer    == NULL) {
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: _InitDLists: USBH_AllocTimer!"));
    return USBH_STATUS_RESOURCES;
  }
  pDev->hIsoEpAbortTimer          = USBH_AllocTimer(USBH_OHCI_ISO_OnAbortUrbTimer, pDev);
  if (pDev->hIsoEpAbortTimer     == NULL) {
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: _InitDLists: USBH_AllocTimer!"));
    return USBH_STATUS_RESOURCES;
  }
#endif
  return USBH_STATUS_SUCCESS;
}

/*********************************************************************
*
*       _CancelTimers
*
*  Function description
*/
static void _CancelTimers(USBH_OHCI_DEVICE * pDev) {
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: _CancelTimers!"));
  // Ep0 remove and abort timers
  if (pDev->ControlEpRemoveTimer) {
    pDev->ControlEpRemoveTimerCancelFlag = TRUE;
    USBH_CancelTimer(pDev->ControlEpRemoveTimer);
  }
  if (pDev->ControlEpAbortTimer) {
    pDev->ControlEpAbortTimerCancelFlag = TRUE;
    USBH_CancelTimer(pDev->ControlEpAbortTimer);
  }
  // Bulk remove and abort timers
  if (pDev->hBulkEpRemoveTimer) {
    pDev->BulkEpRemoveTimerCancelFlag = TRUE;
    USBH_CancelTimer(pDev->hBulkEpRemoveTimer);
  }
  if (pDev->hBulkEpAbortTimer) {
    pDev->BulkEpAbortTimerCancelFlag = TRUE;
    USBH_CancelTimer(pDev->hBulkEpAbortTimer);
  }
  // Int remove and abort timers
  if (pDev->hIntEpRemoveTimer) {
    pDev->IntEpRemoveTimerCancelFlag = TRUE;
    USBH_CancelTimer(pDev->hIntEpRemoveTimer);
  }
  if (pDev->hIntEpAbortTimer) {
    pDev->IntEpAbortTimerCancelFlag = TRUE;
    USBH_CancelTimer(pDev->hIntEpAbortTimer);
  }
  // Iso remove and abort timers
  if (pDev->hIsoEpRemoveTimer) {
    pDev->IsoEpRemoveTimerCancelFlag = TRUE;
    USBH_CancelTimer(pDev->hIsoEpRemoveTimer);
  }
  if (pDev->hIsoEpAbortTimer) {
    pDev->IsoEpAbortTimerCancelFlag = TRUE;
    USBH_CancelTimer(pDev->hIsoEpAbortTimer);
  }
}

/*********************************************************************
*
*       _FreeTimers
*
*  Function description
*/
static void _FreeTimers(USBH_OHCI_DEVICE * pDev) {
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: _FreeTimers!"));
  // Ep0 remove and abort timers
  if (pDev->ControlEpRemoveTimer) {
    USBH_FreeTimer(pDev->ControlEpRemoveTimer);
    pDev->ControlEpRemoveTimer = NULL;
  }
  if (pDev->ControlEpAbortTimer) {
    USBH_FreeTimer(pDev->ControlEpAbortTimer);
    pDev->ControlEpAbortTimer = NULL;
  }
  // Bulk remove and abort timers
  if (pDev->hBulkEpRemoveTimer) {
    USBH_FreeTimer(pDev->hBulkEpRemoveTimer);
    pDev->hBulkEpRemoveTimer = NULL;
  }
  if (pDev->hBulkEpAbortTimer) {
    USBH_FreeTimer(pDev->hBulkEpAbortTimer);
    pDev->hBulkEpAbortTimer = NULL;
  }
  // Int remove and abort timers
  if (pDev->hIntEpRemoveTimer) {
    USBH_FreeTimer(pDev->hIntEpRemoveTimer);
    pDev->hIntEpRemoveTimer = NULL;
  }
  if (pDev->hIntEpAbortTimer) {
    USBH_FreeTimer(pDev->hIntEpAbortTimer);
    pDev->hIntEpAbortTimer = NULL;
  }
  // Iso remove and abort timers
  if (pDev->hIsoEpRemoveTimer) {
    USBH_FreeTimer(pDev->hIsoEpRemoveTimer);
    pDev->hIsoEpRemoveTimer = NULL;
  }
  if (pDev->hIsoEpAbortTimer) {
    USBH_FreeTimer(pDev->hIsoEpAbortTimer);
    pDev->hIsoEpAbortTimer = NULL;
  }
}

/*********************************************************************
*
*       _InitDLists
*
*  Function description
*/
static void _InitDLists(USBH_OHCI_DEVICE * pDev) {
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: _InitDLists!"));
  // Init all devices dlists
  USBH_DLIST_Init(&pDev->ControlEpList);
  pDev->ControlEpCount = 0;
  USBH_DLIST_Init(&pDev->BulkEpList);
  pDev->BulkEpCount    = 0;
  pDev->IntEpCount     = 0;
  USBH_DLIST_Init(&pDev->IsoEpList);
  pDev->IsoEpCount     = 0;
}

/*********************************************************************
*
*       _ValidDListsOnExit
*
*  Function description
*/
static void _ValidDListsOnExit(USBH_OHCI_DEVICE * pDev) {
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: _ValidDListsOnExit!"));
  USBH_OCHI_IS_DEV_VALID(pDev);
  if (!USBH_DLIST_IsEmpty(&pDev->ControlEpList)) {
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: OhdCheckDLists: ControlEpList not empty!"));
  }
  if (pDev->ControlEpCount) {
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: OhdCheckDLists: ControlEpCount not zero!"));
  }
  if (!USBH_DLIST_IsEmpty(&pDev->BulkEpList)) {
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: OhdCheckDLists: BulkEpList not empty!"));
  }
  if (pDev->BulkEpCount) {
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: OhdCheckDLists: BulkEpCount not zero!"));
  }
  if (pDev->IntEpCount) {
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: OhdCheckDLists: IntEpCount not zero!"));
  }
  if (!USBH_DLIST_IsEmpty(&pDev->IsoEpList)) {
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: OhdCheckDLists: IsoEpList not empty!"));
  }
  if (pDev->IntEpCount) {
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: OhdCheckDLists: IsoEpCount not zero!"));
  }
}

/*********************************************************************
*
*       _AddControlBulkDummyEndpoint
*
*  Function description
*    Adds a control or an bulk dummy endpoint. An empty Transfer Descriptor is also added.
*/
static USBH_STATUS _AddControlBulkDummyEndpoint(USBH_OHCI_DEVICE * pDev, U8 EpType) {
  USBH_OHCI_EP0         * pEP0;
  USBH_OHCI_BULK_INT_EP * pBulkEP;
  USBH_OHCI_ISO_EP      * pIsoEP = NULL;
  USBH_STATUS             Status;

  USBH_OCHI_IS_DEV_VALID(pDev);
  Status = USBH_STATUS_SUCCESS;
  switch (EpType) {
  case USB_EP_TYPE_CONTROL:
    pEP0 = USBH_OHCI_EP0_Get(&pDev->ControlEPPool, &pDev->SetupPacketPool); // Get memory from pools
    if (pEP0 == NULL) {
      USBH_WARN((USBH_MTYPE_OHCI, "OHCI: OhdAddDummyEndpoint: USBH_OHCI_EP0_Get!"));
      break;
    }
    Status = USBH_OHCI_EP0_Init(pEP0, pDev, OH_DUMMY_ED_EPFLAG, OH_DEFAULT_DEV_ADDR, OH_DEFAULT_EP_ADDR, OH_DEFAULT_MAX_PKT_SIZE, OH_DEFAULT_SPEED);
    if (Status) {                                               // On error
      USBH_WARN((USBH_MTYPE_OHCI, "OHCI: OhdAddDummyEndpoint: USBH_OHCI_EP0_Init!"));
      USBH_OHCI_EP0_Put(pEP0);
      break;
    }
    USBH_OHCI_EP0_Insert(pEP0);
    USBH_OHCI_EndpointListEnable(pDev, USB_EP_TYPE_CONTROL, TRUE, TRUE);
    break;
  case USB_EP_TYPE_BULK:
    pBulkEP = USBH_OHCI_BULK_INT_GetEp(&pDev->BulkEPPool);
    if (pBulkEP == NULL) {
      USBH_WARN((USBH_MTYPE_OHCI, "OHCI: OhdAddDummyEndpoint: USBH_OHCI_BULK_INT_GetEp!"));
      break;
    }
    pBulkEP = USBH_OHCI_BULK_INT_InitEp(pBulkEP, pDev, EpType, OH_DEFAULT_DEV_ADDR, OH_DEFAULT_EP_ADDR, OH_DEFAULT_MAX_PKT_SIZE, 0, USBH_FULL_SPEED, OH_DUMMY_ED_EPFLAG);
    if (pBulkEP == NULL) {
      USBH_WARN((USBH_MTYPE_OHCI, "OHCI: OhdAddDummyEndpoint: USBH_OHCI_BULK_INT_InitEp!"));
      Status = USBH_STATUS_RESOURCES;
      break;
    }
    USBH_OHCI_BULK_InsertEp(pBulkEP);
    break;
  case USB_EP_TYPE_ISO:
#if USBH_SUPPORT_ISO_TRANSFER
    pIsoEP = USBH_OHCI_ISO_GetEp(&pDev->IsoEPPool);
    if (pIsoEP == NULL) {
      USBH_WARN((USBH_MTYPE_OHCI, "OHCI: OhdAddDummyEndpoint: USBH_OHCI_BULK_INT_GetEp!"));
      break;
    }
    pIsoEP = USBH_OHCI_ISO_InitEp(pIsoEP, pDev, EpType, OH_DEFAULT_DEV_ADDR, OH_DEFAULT_EP_ADDR, OH_DEFAULT_MAX_PKT_SIZE, 0, USBH_FULL_SPEED, OH_DUMMY_ED_EPFLAG);
    if (pIsoEP== NULL) {
      USBH_WARN((USBH_MTYPE_OHCI, "OHCI: OhdAddDummyEndpoint: USBH_OHCI_ISO_InitEp!"));
      Status = USBH_STATUS_RESOURCES;
      break;
    }
    USBH_OHCI_ISO_InsertEp(pIsoEP);
    break;
#else
    (void)(pIsoEP);
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: ISO EP not enabled"));
    break;
#endif
    default:
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: OhdAddDummyEndpoint:invalid EP type!"));
      break;
  }
  return Status;
}

/*********************************************************************
*
*       _SetupOHCI
*
*  Function description
*    Sets up the host controller. The host controller must be in one
*    of the following state before this functions is called:
*    - operational state
*    - reset state
*    - resume state
*    Initializes all needed host register and sets the reset state!
*/
static USBH_STATUS _SetupOHCI(USBH_OHCI_DEVICE * pDev) {
  USBH_STATUS status;
  _DisableInterrupt(pDev); // Disables all USB host interrupts
  // After software reset adds an dummy endpoint to each transfer type and link this ep with the host phy. address register
  // This functions writes the phy. start addresses of lists in register.
  status = _AddControlBulkDummyEndpoint(pDev, USB_EP_TYPE_CONTROL);
  if (status) {
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: _SetupOHCI: OhdAddDummyEndpoint: Ep0!"));
    return status;
  }
  status = _AddControlBulkDummyEndpoint(pDev, USB_EP_TYPE_BULK);

  if (status) {
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: _SetupOHCI: OhdAddDummyEndpoint: Bulk!"));
    return status;
  }
  status = _InitHccaInterruptTable(pDev); // Adds all interrupt endpoints
  if (status) {
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: _SetupOHCI: _InitHccaInterruptTable!"));
    return status;
  }
  OhHalWriteReg(pDev->pRegBase, OH_REG_HCCA, pDev->pHcca->ItemHeader.PhyAddr); // Set the HCCA phy. address
  _InitIntervalReg(pDev);                                                 // Set frame interval
  USBH_OHCI_EndpointListEnable(pDev, USB_EP_TYPE_INT, TRUE, TRUE);                 // Enable periodic list
  _SetInterrupts(pDev, OH_ENABLED_INTERRUPTS);                            // Enable interrupts DOEN,unrecoverable error ,root hub status change
  _EnableInterrupt(pDev);                                                 // Enable master interrupts
  return status;
}

/*********************************************************************
*
*       _UnlinkHostResources
*
*  Function description
*/
static void _UnlinkHostResources(USBH_OHCI_DEVICE * pDev) {
  U32 time;
  U32 intStatus;
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: _UnlinkHostResources!"));
  if (pDev == NULL) {
    USBH_LOG((USBH_MTYPE_OHCI, "OHCI: INFO _UnlinkHostResources: parameter dev is NULL, return!"));
    return;
  }
  USBH_OCHI_IS_DEV_VALID(pDev);
  // First stop all list processing
  USBH_OHCI_EndpointListEnable(pDev, USB_EP_TYPE_CONTROL, FALSE, FALSE);
  USBH_OHCI_EndpointListEnable(pDev, USB_EP_TYPE_BULK,    FALSE, FALSE);
  USBH_OHCI_EndpointListEnable(pDev, USB_EP_TYPE_INT,     FALSE, FALSE);
#if USBH_SUPPORT_ISO_TRANSFER
  USBH_OHCI_EndpointListEnable(pDev, USB_EP_TYPE_ISO,     FALSE, FALSE);
#endif
  _CancelTimers(pDev);                                    // Stop all timer
  USBH_OHCI_INT_RemoveAllUserEDFromPhysicalLink(pDev);              // Remove physical links on all physical linked endpoints
  time = USBH_OS_GetTime32();
  while (!USBH_IsTimeOver(OH_STOP_DELAY_TIME * 1000, time)); // *1000 -> in us
  // Disable the host controller interrupt and clear all interrupt Status bits!
  _DisableInterrupt(pDev);
  intStatus = OhHalReadReg(pDev->pRegBase, OH_REG_INTERRUPTSTATUS);
  OhHalWriteReg(pDev->pRegBase, OH_REG_INTERRUPTSTATUS, intStatus & OH_ENABLED_INTERRUPTS);
  if (pDev->ControlEpCount > 1) {
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: USBH_HostExit: open control endpoints: %u !", pDev->BulkEpCount));
  }
  USBH_OHCI_Ep0RemoveEndpoints(pDev, TRUE);                         // Used endpoint and the dummy endpoint are released
  if (pDev->BulkEpCount > 1) {
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: USBH_HostExit: open bulk endpoints: %u !", pDev->BulkEpCount));
  }
  USBH_OHCI_BULK_RemoveEps(pDev, TRUE);                        // Used endpoint and the dummy endpoint are released
  if (pDev->IntEpCount) {
  // User interrupt endpoints must be zero because there dummy endpoints are not counted
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: USBH_HostExit: open interrupt endpoints: %u != 0!", pDev->IntEpCount));
  }
  USBH_OHCI_INT_RemoveEDFromLogicalListAndFree(pDev, TRUE);          // Remove all endpoints with the UNLINK state
  USBH_OHCI_INT_PutAllDummyEp(pDev);
  if (pDev->IsoEpCount) {
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: USBH_HostExit: IsoEpCount: %u !", pDev->IsoEpCount));
  }
#if USBH_SUPPORT_ISO_TRANSFER
  USBH_OHCI_ISO_RemoveEps(pDev, TRUE);
#endif
  _ValidDListsOnExit(pDev);                               // Check that all dlists and endpoint counter are zero
}

/*********************************************************************
*
*       _DeleteDevice
*
*  Function description
*/
static void _DeleteDevice(USBH_OHCI_DEVICE * pDev) {
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: _DeleteDevice!"));
  if (pDev == NULL) {
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: _DeleteDevice: param. dev is NULL!"));
    return;
  }
  USBH_OCHI_IS_DEV_VALID(pDev);
  _FreeTimers(pDev); // _FreeTimers can called twice
  // Frees Control, Bulk, Interrupt dummy interrupt, transfer descriptor and HCCA pool
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: Frees Control EP and setup packet Pool!"));
  USBH_HCM_FreePool(&pDev->ControlEPPool);
  USBH_HCM_FreePool(&pDev->SetupPacketPool);
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: Frees BulkEPPool!"));
  USBH_HCM_FreePool(&pDev->BulkEPPool);
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: Frees IntEPPool!"));
  USBH_HCM_FreePool(&pDev->IntEPPool);
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: Frees DummyIntEPPool!"));
  USBH_HCM_FreePool(&pDev->DummyIntEPPool);
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: Frees IsoEPPool!"));
  USBH_HCM_FreePool(&pDev->IsoEPPool);
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: Frees GTDPool!"));
  USBH_HCM_FreePool(&pDev->GTDPool);
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: [Frees HccaPool!"));
  USBH_OHCI_HccaRelease(pDev->pHcca);
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: ]Frees HccaPool!"));
  USBH_HCM_FreePool(&pDev->TransferBufferPool);
  USBH_Free(pDev);
}

/*********************************************************************
*
*       Bus driver callback functions
*
**********************************************************************
*/

/*********************************************************************
*
*       _AbortEndpoint
*
*  Function description
*/
static USBH_STATUS _AbortEndpoint(USBH_HC_EP_HANDLE EpHandle) {
  USBH_OHCI_EP0   * pEp0;
  USBH_OHCI_BULK_INT_EP * pBulkEp;
  USBH_OHCI_ISO_EP      * pIsoEp = NULL;
  USBH_STATUS       Status;

  pEp0   = (USBH_OHCI_EP0 *)EpHandle; // Field type is the first field after the item header field
  Status = USBH_STATUS_INVALID_PARAM;
  switch (pEp0->EndpointType) {       // The element EndpointType has in all endpoint structs the same offset in the struct
  case USB_EP_TYPE_CONTROL:
    Status = USBH_OHCI_EP0_AbortEp(pEp0);
    break;
  case USB_EP_TYPE_INT:
  case USB_EP_TYPE_BULK:
    pBulkEp = (USBH_OHCI_BULK_INT_EP *)EpHandle;
    Status = USBH_OHCI_BULK_INT_AbortEp(pBulkEp);
    if (Status) {
      USBH_WARN((USBH_MTYPE_OHCI, "OHCI: _AbortEndpoint: USBH_OHCI_BULK_INT_AbortEp Status: 0x%x!", Status));
    }
    break;
  case USB_EP_TYPE_ISO:
#if USBH_SUPPORT_ISO_TRANSFER
    pIsoEp = (USBH_OHCI_ISO_EP *)EpHandle;
    Status = USBH_OHCI_ISO_AbortEp(pIsoEp);
    if (Status) {
      USBH_WARN((USBH_MTYPE_OHCI, "OHCI: _AbortEndpoint: USBH_OHCI_ISO_AbortEp Status: 0x%x!", Status));
    }
#else
    (void)(pIsoEp);
#endif
    break;
  default:
    USBH_LOG((USBH_MTYPE_OHCI, "OHCI: _AbortEndpoint: invalid endpoint type: %u!", pEp0->EndpointType));
    break;
  }
  return Status;
}

/*********************************************************************
*
*       _ResetEndpoint
*
*  Function description
*    Resets the data toggle bit to 0. The bus driver takes care that
*    this function is called only if no pending URB is scheduled.
*/
static USBH_STATUS _ResetEndpoint(USBH_HC_EP_HANDLE EpHandle) {
  USBH_OHCI_BULK_INT_EP * ep;
  USBH_STATUS       status;
  ep              = (USBH_OHCI_BULK_INT_EP *)EpHandle; // Field type is the first field after the item header field
  status          = USBH_STATUS_INVALID_PARAM;
  switch (ep->EndpointType) {
  case USB_EP_TYPE_BULK:
  case USB_EP_TYPE_INT:
    USBH_LOG((USBH_MTYPE_OHCI, "OHCI: _ResetEndpoint: DevAddr.:%u Ep: 0x%x !", ep->DeviceAddress, ep->EndpointAddress));
    if (ep->State != OH_EP_LINK) {
      USBH_WARN((USBH_MTYPE_OHCI, "OHCI: _ResetEndpoint: Ep state is not linked!"));
      status = USBH_STATUS_ERROR;
      break;
    }
    if (ep->UrbCount) {
      USBH_WARN((USBH_MTYPE_OHCI, "OHCI: _ResetEndpoint: Pending URBs!"));
    }
    USBH_OHCI_EpClearToggle(&ep->ItemHeader);
    USBH_OHCI_EpGlobClearHalt(&ep->ItemHeader);
    ep->HaltFlag = FALSE;                      // Rest with _ResetEndpoint
    USBH_OHCI_BULK_INT_SubmitUrbsFromList(ep);           // Try to submit the next URB from list
    status = USBH_STATUS_SUCCESS;
    break;
  default:
    ;
    USBH_LOG((USBH_MTYPE_OHCI, "OHCI: _ResetEndpoint: invalid endpoint type: %u!", ep->EndpointType));
  }
  return status;
}

/*********************************************************************
*
*       _AllocCopyTransferBufferPool
*
*  Function description
*/
static USBH_STATUS _AllocCopyTransferBufferPool(USBH_HCM_POOL * bufferPool, U32 bufferSize, U32 numberOfBuffers) {
  USBH_STATUS status = USBH_STATUS_SUCCESS;
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: OhAllocTransferBufferPool: bufferSize: %d numberOfBuffers:%d!", numberOfBuffers, bufferSize));
  status             = USBH_HCM_AllocPool(bufferPool, numberOfBuffers, bufferSize, sizeof(USBH_OHCI_TRANSFER_BUFFER), USBH_TRANSFER_BUFFER_ALIGNMENT);
  if (status) { // On error
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: OhAllocTransferBufferPool: USBH_HCM_AllocPool!"));
  }
  return status;
}

/*********************************************************************
*
*       Public code
*
**********************************************************************
*/

/*********************************************************************
*
*       USBH_IsTimeOver
*
*  Function description
*    Returns TRUE if the time is over.
*
*  Parameters:
*    Waittime:  in microseconds
*    StartTime: in microseconds
*/
USBH_BOOL USBH_IsTimeOver(U32 Waittime, U32 StartTime) {
  if (((U32)((USBH_OS_GetTime32() * 1000) - StartTime)) >= Waittime) {
    return TRUE;
  } else {
    return FALSE;
  }
}

/*********************************************************************
*
*       USBH_OHCI_GetCopyTransferBuffer
*
*  Function description
*/
USBH_OHCI_TRANSFER_BUFFER * USBH_OHCI_GetCopyTransferBuffer(USBH_HCM_POOL * transferBufferPool) {
  USBH_OHCI_TRANSFER_BUFFER * item;
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: OhGetTransferBuffer!"));
  item = (USBH_OHCI_TRANSFER_BUFFER *)USBH_HCM_GetItem(transferBufferPool);
  if (NULL == item) {
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: OhGetTransferBuffer: no resources!"));
  }
  return item;
}

/*********************************************************************
*
*       USBH_OHCI_GetInitializedCopyTransferBuffer
*
*  Function description
*/
USBH_OHCI_TRANSFER_BUFFER * USBH_OHCI_GetInitializedCopyTransferBuffer(USBH_HCM_POOL * transferBufferPool, U8 * urbBuffer, U32 urbBufferLength) {
  USBH_OHCI_TRANSFER_BUFFER * item;
  item               = USBH_OHCI_GetCopyTransferBuffer(transferBufferPool);
  if (NULL == item) {
    return NULL;
  }
  item->pUrbBuffer       = urbBuffer;
  item->RemainingLength = urbBufferLength;
  item->Transferred     = 0;
  return item;
}

/*********************************************************************
*
*       USBH_OHCI_GetBufferLengthFromCopyTransferBuffer
*
*  Function description
*    Return the remaining transfer length and an virt. pointer to the
*    physical transfer buffer
*/
U8 * USBH_OHCI_GetBufferLengthFromCopyTransferBuffer(USBH_OHCI_TRANSFER_BUFFER * transferBuffer, U32 * length) {
  U8       * buffer;
  * length = USBH_MIN(transferBuffer->RemainingLength, USBH_Global.Config.TransferBufferSize);
  buffer   = (U8 *)transferBuffer->ItemHeader.PhyAddr;
  return buffer;
}

/*********************************************************************
*
*       USBH_OHCI_UpdateCopyTransferBuffer
*
*  Function description
*/
U32 USBH_OHCI_UpdateCopyTransferBuffer(USBH_OHCI_TRANSFER_BUFFER * transferBuffer, U32 transferred) {
  USBH_ASSERT_PTR(transferBuffer->pUrbBuffer);
  if (transferBuffer->RemainingLength < transferred) {
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: OhUpdateTransferBuffer:RemainingLength: %d < transferLength: %d ", transferBuffer->RemainingLength, transferred));
    transferred = transferBuffer->RemainingLength;
  }
  // Update buffer pointer and length
  transferBuffer->pUrbBuffer += transferred;
  transferBuffer->Transferred += transferred;
  transferBuffer->RemainingLength -= transferred;
  return transferBuffer->RemainingLength;
}

/*********************************************************************
*
*       USBH_OHCI_FillCopyTransferBuffer
*
*  Function description
*
*  Parameters:
*    transferBuffer: pointer to the transfer buffer
*    transferLength: IN:  Transferred length or zero at the first time
*                    OUT: length to send to the USB device
*  Return value:
*    Pointer to the beginning of the transfer buffer
*/
U32 USBH_OHCI_FillCopyTransferBuffer(USBH_OHCI_TRANSFER_BUFFER * transferBuffer) {
  U8  * phy_buffer;
  U32   length;
  USBH_ASSERT_PTR(transferBuffer->pUrbBuffer);
  phy_buffer = (U8 *)transferBuffer->ItemHeader.PhyAddr;
  length = USBH_MIN(transferBuffer->RemainingLength, USBH_Global.Config.TransferBufferSize);
  if (length) {
    USBH_MEMCPY(phy_buffer, transferBuffer->pUrbBuffer, length);
  }
  return length;
}

/*********************************************************************
*
*       USBH_OHCI_CopyToUrbBufferUpdateTransferBuffer
*
*  Function description
*
*  Parameters:
*    pTransferBuffer: pointer to the transfer buffer
*    transferLength: IN:  Transferred length
*                    OUT: length to send to the USB device
*  Return value:     Pointer to the beginning of the transfer buffer
*/
U32 USBH_OHCI_CopyToUrbBufferUpdateTransferBuffer(USBH_OHCI_TRANSFER_BUFFER * pTransferBuffer, U32 Transferred) {
  U8  * phy_buffer;
  U32   length;
  USBH_ASSERT_PTR(pTransferBuffer->pUrbBuffer);
  if (pTransferBuffer->RemainingLength < Transferred) {
    length = pTransferBuffer->RemainingLength;
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: OhUpdateTransferBuffer:RemainingLength: %d < transferLength: %d ", pTransferBuffer->RemainingLength, Transferred));
  } else {
    length = Transferred;
  }
  phy_buffer = (U8 *)pTransferBuffer->ItemHeader.PhyAddr;
  if (length) {
    USBH_MEMCPY(pTransferBuffer->pUrbBuffer, phy_buffer, length);
  }
  return USBH_OHCI_UpdateCopyTransferBuffer(pTransferBuffer, length); // Update lengths and pointers
}

/***************************************************************

        Global Host device driver interface functions

***************************************************************/


/*********************************************************************
*
*       USBH_OHCI_CreateController
*
*  Function description
*    Allocates all needed resources for a host controller device object and calls
*    USBH_AddHostController to link this driver to the next upper driver object
*
*  Return value: a valid Handle or NULL on error.
*/
USBH_HC_HANDLE USBH_OHCI_CreateController(void * pBaseAddress) {
  USBH_OHCI_DEVICE   * pHostControllerDev;
  USBH_STATUS   Status;

  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: [USBH_OHCI_CreateController! BaseAddress: 0x%lx ", pBaseAddress));
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: USBH_OHCI_CreateController: OH_TOTAL_CONTIGUOUS_MEMORY: %d", OH_OHCI_MEMORY_SIZE + USBH_Global.Config.TransferBufferSize));
  USBH_ASSERT(pBaseAddress != NULL);
  pHostControllerDev = (USBH_OHCI_DEVICE *)USBH_Malloc(sizeof(USBH_OHCI_DEVICE));
  if (pHostControllerDev == NULL) {
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: USBH_OHCI_CreateController: malloc HC_DEVICE!"));
    Status = USBH_STATUS_MEMORY;
    goto AllocErr;
  }
  USBH_ZERO_MEMORY(pHostControllerDev, sizeof(USBH_OHCI_DEVICE));
  IFDBG(pHostControllerDev->Magic = USBH_OHCI_DEVICE_MAGIC);
  pHostControllerDev->pRegBase = (U8 *)pBaseAddress; // todo: set base address
  _InitDLists(pHostControllerDev);
  Status = _AllocTimers(pHostControllerDev);
  if (Status) {
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: USBH_OHCI_CreateController: _AllocTimers!"));
    goto AllocErr;
  }
  // Allocate all resources for endpoint and transfer descriptors
  pHostControllerDev->pHcca = USBH_OHCI_HccaAlloc(&pHostControllerDev->HccaPool);
  if (NULL == pHostControllerDev->pHcca) {
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: USBH_OHCI_CreateController: OhHccaInit!"));
    goto AllocErr;
  }
  Status = USBH_OHCI_EP0_Alloc(&pHostControllerDev->ControlEPPool, &pHostControllerDev->SetupPacketPool, USBH_OHCI_MAX_CONTROL_EP);
  if (Status) { // On error
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: USBH_OHCI_CreateController:  USBH_OHCI_EP0_Alloc!"));
    goto AllocErr;
  }
  Status = USBH_OHCI_BULK_INT_AllocPool(&pHostControllerDev->BulkEPPool, USBH_OHCI_MAX_BULK_EP);
  if (Status) { // On error
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: USBH_OHCI_CreateController:  USBH_OHCI_BULK_INT_AllocPool!"));
    goto AllocErr;
  }
  Status = USBH_OHCI_BULK_INT_AllocPool(&pHostControllerDev->IntEPPool, USBH_OHCI_MAX_INT_EP);
  if (Status) { // On error
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: USBH_OHCI_CreateController: USBH_OHCI_BULK_INT_AllocPool!"));
    goto AllocErr;
  }
  Status = USBH_OHCI_INT_DummyEpAllocPool(&pHostControllerDev->DummyIntEPPool);
  if (Status) { // on error
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: USBH_OHCI_CreateController: OhDummyIntEpAlloc!"));
    goto AllocErr;
  }
  Status = USBH_OHCI_TdAlloc(&pHostControllerDev->GTDPool, OH_TOTAL_GTD, OH_GTD_ALIGNMENT);
  if (Status) { // On error
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: USBH_OHCI_CreateController: USBH_OHCI_TdAlloc!"));
    goto AllocErr;
  }
#if USBH_SUPPORT_ISO_TRANSFER
  if (USBH_Global.Config.NumIsoEndpoints) {
    Status = USBH_OHCI_TdAlloc(&pHostControllerDev->IsoTDPool, OH_DEV_MAX_ISO_TD, OH_ISO_TD_ALIGNMENT);
    if (Status) { // On error
      USBH_WARN((USBH_MTYPE_OHCI, "OHCI: USBH_OHCI_CreateController: USBH_OHCI_TdAlloc!"));
      goto AllocErr;
    }
    Status = USBH_OHCI_ISO_AllocPool(&pHostControllerDev->IsoEPPool, USBH_OHCI_MAX_ISO_EP);
    if (Status) { // On error
      USBH_WARN((USBH_MTYPE_OHCI, "OHCI: USBH_OHCI_CreateController: USBH_OHCI_BULK_INT_AllocPool!"));
      goto AllocErr;
    }
  } else {
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: Number of ISO EP is 0 although ISO transfer is allowed!"));
  }
#endif
  Status = _AllocCopyTransferBufferPool(&pHostControllerDev->TransferBufferPool, USBH_Global.Config.TransferBufferSize, 4);
  if (Status) { // On error
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: USBH_OHCI_CreateController:  _AllocCopyTransferBufferPool!"));
    goto AllocErr;
  }

  pHostControllerDev->pOhHcca = (USBH_OHCI_HCCA_REG *)pHostControllerDev->pHcca->ItemHeader.PhyAddr; // Set an additional virtual pointer to struct USBH_OHCI_HCCA_REG
AllocErr:
  if (Status) { // On error
    _DeleteDevice(pHostControllerDev);
    pHostControllerDev = NULL;
  }
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: ]USBH_OHCI_CreateController!"));
  return (USBH_HC_HANDLE)pHostControllerDev;
}


/*********************************************************************
*
*       USBH_OHCI_DeleteController
*
*  Function description
*    Deletes a host controller in the memory. The driver does never
*    delete the host controller. If the host controller is added to
*    the USB bus driver this function has to be called after removing
*    the host with OHC_RemoveController().
*    This may happen in the REMOVE_HC_COMPLETION_FUNC routine or at a later time.
*
*    The completion is called after all requests has been (what?) and
*    the USB driver can be removed.
*/
void USBH_OHCI_DeleteController(USBH_HC_HANDLE hHostController) {
  USBH_OHCI_DEVICE * pDev;
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: OHC_DeleteController!"));
  if (NULL == hHostController) {
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: OHC_DeleteController: invalid hHostController!"));
    return;
  }
  USBH_OHCI_HANDLE_TO_PTR(pDev, hHostController);
  USBH_OCHI_IS_DEV_VALID(pDev);
  _DeleteDevice(pDev);
}

/*********************************************************************
*
*       USBH_OHCI_RemoveController
*
*  Function description
*    Removes an OHCI controller from the USB bus driver. The OHCI
*    controller remains in memory until it is deleted with
*    OHC_DeleteController. If REMOVE_HC_COMPLETION_FUNC is called then the
*    host controller is removed from the USB bus driver.
*/
USBH_STATUS USBH_OHCI_RemoveController(USBH_HC_HANDLE hHostController, REMOVE_HC_COMPLETION_FUNC * Completion, void * Context) {
  USBH_OHCI_DEVICE   * pDev;
  USBH_STATUS   status = USBH_STATUS_SUCCESS;
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: [OHC_RemoveController!"));
  if (NULL == hHostController) {
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: OHC_RemoveController: nothing to do! hHostController is NULL!"));
    status = USBH_STATUS_ERROR;
    goto exit;
  }
  USBH_OHCI_HANDLE_TO_PTR(pDev, hHostController);
  USBH_OCHI_IS_DEV_VALID      (pDev);
  if (NULL == pDev->hBusDriver) {
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: OHC_RemoveController: nothing to do: bus driver is not attached!"));
    status = USBH_STATUS_ERROR;
    goto exit;
  }
  // Wait until the bus driver has called the completion routine
  pDev->pfRemoveCompletion = Completion;
  pDev->pRemoveContext    = Context;
  USBH_RemoveHostController(pDev->hBusDriver);
exit:
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: ]OHC_RemoveController!"));
  return status;
}

/*********************************************************************
*
*       USBH_OHCI_HostInit
*
*  Function description
*    Setup all host controller registers and enables the OHCI interrupt.
*    If the function returns the OHCI controller is always in state RESET.
*    Then it set the root hub notification and the context. Before this
*    function is called the interrupt service routine.
*/
USBH_STATUS USBH_OHCI_HostInit(USBH_HC_HANDLE hHostController, USBH_ROOT_HUB_NOTIFICATION_FUNC * pfUbdRootHubNotification, void * pRootHubNotificationContext) {
  USBH_STATUS        status;
  USBH_OHCI_DEVICE   * pDev;
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: [USBH_HostInit!"));
  USBH_ASSERT(hHostController != NULL);
  USBH_OHCI_HANDLE_TO_PTR(pDev, hHostController);
  USBH_OCHI_IS_DEV_VALID      (pDev);
  pDev->RootHub.pfUbdRootHubNotification     = pfUbdRootHubNotification;
  pDev->RootHub.pRootHubNotificationContext  = pRootHubNotificationContext;
  status = _CheckRevision(pDev);  // Check the host revision register
  if (status) {
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: USBH_HostInit: _CheckRevision!"));
    goto exit;
  }
  // Reset the OHCI controller, set the operational state to Reset
  status = _SoftReset(pDev);
  if (status) {
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: USBH_HostInit: _SoftReset!"));
    goto exit;
  }
  // Initialize the root hub descriptors the port power is off!
  status = USBH_OHCI_ROOTHUB_Init(pDev, USBH_ROOTHUB_OnNotification, pRootHubNotificationContext);
  if (status) {
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: USBH_HostInit: USBH_OHCI_ROOTHUB_Init!"));
    goto exit;
  }
  status = _SetupOHCI(pDev); // Setup other register
  if (status) {
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: USBH_HostInit: _SetupOHCI!"));
    goto exit;
  }
  // Do not use all list
  //USBH_OHCI_EndpointListEnable(dev,USB_EP_TYPE_CONTROL,FALSE,FALSE);
  //USBH_OHCI_EndpointListEnable(dev,USB_EP_TYPE_INT,FALSE,FALSE);
exit:
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: ]USBH_HostInit!"));
  if (status) {
    // On error remove added endpoints and disable all endpoint lists. The OHCI device is deleted in
    // USBH_AddHostController at later if USBH_OHCI_CreateController returns this status.
    _UnlinkHostResources(pDev);
  }
  return status;
}

/*********************************************************************
*
*       USBH_OHCI_HostExit
*
*  Function description
*    Unlinks all resources from the host controller. If the remove completion
*    is set then the pfRemoveCompletion routine is called with an user context!
*    Is called if the USB bus driver has removed the host object.
*/
USBH_STATUS USBH_OHCI_HostExit(USBH_HC_HANDLE hHostController) {
  USBH_OHCI_DEVICE       * pDev;
  REMOVE_HC_COMPLETION_FUNC * removeCompletion;
  void                 * removeContext;
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: [USBH_HostExit!"));
  USBH_OHCI_HANDLE_TO_PTR    (pDev, hHostController);
  USBH_OCHI_IS_DEV_VALID          (pDev);
  _UnlinkHostResources(pDev); // Unlink resources from the host
  removeCompletion = pDev->pfRemoveCompletion;
  removeContext    = pDev->pRemoveContext;
  if (NULL != removeCompletion) {
    removeCompletion(removeContext);
  }
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: ]USBH_HostExit!"));
  return USBH_STATUS_SUCCESS;
}

/*********************************************************************
*
*       USBH_OHCI_AddEndpoint
*
*  Function description
*    Returns an endpoint Handle for the added endpoint
*/
USBH_HC_EP_HANDLE USBH_OHCI_AddEndpoint(USBH_HC_HANDLE hHostController, U8 EndpointType, U8 DeviceAddress, U8 EndpointAddress, U16 MaxFifoSize, U16 IntervalTime, USBH_SPEED Speed) {
  USBH_HC_EP_HANDLE         Handle;
  USBH_OHCI_DEVICE    * pDev;
  USBH_OHCI_EP0           * pEP0;
  USBH_OHCI_BULK_INT_EP   * pBulkIntEp;
  USBH_OHCI_ISO_EP        * pIsoEp = NULL;
  USBH_STATUS               Status;

  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: USBH_OHCI_AddEndpoint: Dev.Addr: %d, EpAddr: 0x%x max.Fifo size: %d!",
           (int)DeviceAddress, (int)EndpointAddress, (int)MaxFifoSize));
  Handle            = NULL;
  USBH_OHCI_HANDLE_TO_PTR(pDev,hHostController);
  USBH_OCHI_IS_DEV_VALID      (pDev);
  switch (EndpointType) {
  case USB_EP_TYPE_CONTROL:
    USBH_LOG((USBH_MTYPE_OHCI, "OHCI: Add Control Ep"));
    pEP0=USBH_OHCI_EP0_Get(&pDev->ControlEPPool, &pDev->SetupPacketPool);
    if ( pEP0 == NULL ) {
     USBH_WARN((USBH_MTYPE_OHCI, "OHCI: USBH_OHCI_AddEndpoint: USBH_OHCI_EP0_Get!"));
     break;
    }
    Status=USBH_OHCI_EP0_Init(pEP0, pDev, 0, DeviceAddress, EndpointAddress, MaxFifoSize, Speed);
    if (Status) { // On error
      USBH_WARN((USBH_MTYPE_OHCI, "OHCI: USBH_OHCI_AddEndpoint: USBH_OHCI_EP0_Init!"));
      USBH_OHCI_EP0_Put(pEP0); // Put the endpoint back to the pool
      break;
    }
    USBH_OHCI_EP0_Insert(pEP0);
    USBH_LOG((USBH_MTYPE_OHCI, "OHCI: Control Ep successful added! "));
    Handle = pEP0;
    break;
  case USB_EP_TYPE_BULK:
    USBH_LOG((USBH_MTYPE_OHCI, "OHCI: Add Bulk Ep"));
    pBulkIntEp=USBH_OHCI_BULK_INT_GetEp(&pDev->BulkEPPool);
    if (pBulkIntEp == NULL) {
     USBH_WARN((USBH_MTYPE_OHCI, "OHCI: USBH_OHCI_AddEndpoint: USBH_OHCI_BULK_INT_GetEp!"));
     break;
    }
    pBulkIntEp=USBH_OHCI_BULK_INT_InitEp(pBulkIntEp, pDev, EndpointType, DeviceAddress, EndpointAddress, MaxFifoSize, IntervalTime, Speed, 0); // Init the bulk endpoint
    if (pBulkIntEp == NULL) {
      USBH_WARN((USBH_MTYPE_OHCI, "OHCI: USBH_OHCI_AddEndpoint: USBH_OHCI_BULK_INT_InitEp!"));
      break;
    }
    USBH_OHCI_BULK_InsertEp(pBulkIntEp); // Add the Ed to the HC list
    Handle = pBulkIntEp;
    break;
  case USB_EP_TYPE_INT:
    USBH_LOG((USBH_MTYPE_OHCI, "OHCI: Add Int Ep"));
    pBulkIntEp=USBH_OHCI_BULK_INT_GetEp(&pDev->IntEPPool);       // Get the interrupt endpoint always from another pool
    if (pBulkIntEp == NULL) {
     USBH_WARN((USBH_MTYPE_OHCI, "OHCI: USBH_OHCI_AddEndpoint: USBH_OHCI_BULK_INT_GetEp!"));
     break;
    }
    pBulkIntEp=USBH_OHCI_BULK_INT_InitEp(pBulkIntEp, pDev, EndpointType, DeviceAddress, EndpointAddress, MaxFifoSize, IntervalTime, Speed, 0); // Init the interrupt endpoint
    if (pBulkIntEp==NULL) {
      USBH_WARN((USBH_MTYPE_OHCI, "OHCI: USBH_OHCI_AddEndpoint: USBH_OHCI_BULK_INT_InitEp!"));
      break;
    }
    Status=USBH_OHCI_INT_InsertEp(pBulkIntEp); // Add the interrupt endpoint on the correct place
    if ( Status ) {
      USBH_WARN((USBH_MTYPE_OHCI, "OHCI: USBH_OHCI_AddEndpoint: USBH_OHCI_INT_InsertEp!"));
      USBH_OHCI_BULK_INT_PutEp(pBulkIntEp);// Release the endpoint with the dummy TD
      break;
    }
    Handle = pBulkIntEp;
    break;
  case USB_EP_TYPE_ISO:
#if USBH_SUPPORT_ISO_TRANSFER
    USBH_LOG((USBH_MTYPE_OHCI, "OHCI: Add Iso Ep"));
    pIsoEp = USBH_OHCI_ISO_GetEp(&pDev->IsoEPPool);
    if (pIsoEp == NULL) {
     USBH_WARN((USBH_MTYPE_OHCI, "OHCI: USBH_OHCI_AddEndpoint: USBH_OHCI_ISO_GetEp!"));
    break;
    }
    pIsoEp = USBH_OHCI_ISO_InitEp(pIsoEp, pDev, EndpointType, DeviceAddress, EndpointAddress, MaxFifoSize, IntervalTime, Speed, 0); // Init the bulk endpoint
    if (pIsoEp == NULL) {
      USBH_WARN((USBH_MTYPE_OHCI, "OHCI: USBH_OHCI_AddEndpoint: USBH_OHCI_ISO_InitEp!"));
      break;
    }
    USBH_OHCI_ISO_InsertEp(pIsoEp); // Add the Ed to the HC list
    Handle = pIsoEp;
#else
    (void)(pIsoEp);
    USBH_WARN((USBH_MTYPE_OHCI, "ISO transfer not enabled!"));
#endif
    break;
  default:
    USBH_LOG((USBH_MTYPE_OHCI, "OHCI: USBH_OHCI_AddEndpoint: invalid endpoint type: %u!",EndpointType));
    break;
  }
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: ]USBH_OHCI_AddEndpoint!"));
  return Handle;
}

/*********************************************************************
*
*       USBH_OHCI_ReleaseEndpoint
*
*  Function description
*/
void USBH_OHCI_ReleaseEndpoint(USBH_HC_EP_HANDLE EpHandle, USBH_RELEASE_EP_COMPLETION_FUNC * pfReleaseEpCompletion, void* pContext) {
  USBH_OHCI_EP0         * pEp0;
  USBH_OHCI_BULK_INT_EP * pBulkIntEp;
  USBH_OHCI_ISO_EP      * pIsoEp = NULL;
  // The struct elements until Type are the same for all endpoint types
  if (NULL == EpHandle) {
    USBH_LOG((USBH_MTYPE_OHCI, "OHCI: USBH_OHCI_ReleaseEndpoint: invalid EpHandle!"));
    return;
  }
  pEp0=(USBH_OHCI_EP0 *)EpHandle;
  pBulkIntEp = (USBH_OHCI_BULK_INT_EP *)EpHandle;
  pIsoEp = (USBH_OHCI_ISO_EP *)EpHandle;
  switch (pEp0->EndpointType) {
  // The element EndpointType has in all endpoint structs the same offset in the struct
  case USB_EP_TYPE_CONTROL:
    USBH_OHCI_EP0_ReleaseEp(pEp0, pfReleaseEpCompletion, pContext);
    break;
  case USB_EP_TYPE_BULK:
    USBH_OHCI_BULK_ReleaseEp(pBulkIntEp, pfReleaseEpCompletion, pContext);
    break;
  case USB_EP_TYPE_INT:
    USBH_OHCI_INT_ReleaseEp (pBulkIntEp, pfReleaseEpCompletion, pContext);
    break;
  case USB_EP_TYPE_ISO:
#if USBH_SUPPORT_ISO_TRANSFER
    USBH_OHCI_ISO_ReleaseEndpoint(pIsoEp, pfReleaseEpCompletion, pContext);
    break;
#else
    (void)pIsoEp;
#endif
  default:;
    USBH_LOG((USBH_MTYPE_OHCI, "OHCI: USBH_OHCI_ReleaseEndpoint: invalid endpoint type:%u!",pEp0->EndpointType));
  }
}

/*********************************************************************
*
*       USBH_OHCI_SetHcState
*
*  Function description
*    Set the state of the HC
*/
USBH_STATUS USBH_OHCI_SetHcState(USBH_HC_HANDLE hHostController, USBH_HOST_STATE HostState) {
  USBH_OHCI_DEVICE  * pDev;
  USBH_STATUS       status;

  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: USBH_OHCI_SetHcState HostState:%d!",HostState));
  USBH_OHCI_HANDLE_TO_PTR(pDev,hHostController);
  USBH_OCHI_IS_DEV_VALID      (pDev);
  status      = USBH_STATUS_SUCCESS;
  switch (HostState) {
  case USBH_HOST_RESET:
    status = _SetHcFuncState(pDev, HC_USB_RESET);
    // Wait for 10ms if switch to USBH_HOST_RUNNING
    break;
  case USBH_HOST_RUNNING:
    status = _SetHcFuncState(pDev, HC_USB_OPERATIONAL);
    break;
  case USBH_HOST_SUSPEND:
    status = _SetHcFuncState(pDev, HC_USB_SUSPEND);
    break;
  default:
    status = USBH_STATUS_ERROR;
  }
#if (USBH_DEBUG > 1)
  if (status) {
   USBH_WARN((USBH_MTYPE_OHCI, "OHCI: USBH_OHCI_SetHcState!"));
  }
#endif
  return status;
}

/*********************************************************************
*
*       USBH_OHCI_UpdateUpperFrameCounter
*
*  Function description
*    Must be called once every 64 seconds. This is done by calling this
*    routine in the interrupt if an frame overflow happens! This occurs every 32 ms!
*/
void USBH_OHCI_UpdateUpperFrameCounter(USBH_OHCI_DEVICE * pDev) {
  U16     frame;
  frame = pDev->pOhHcca->FrameNumber;
  if (frame < pDev->LastFrameCounter) {
    pDev->UpperFrameCounter++;
  }
  pDev->LastFrameCounter=frame;
}

/*********************************************************************
*
*       USBH_OHCI_GetFrameNumber
*
*  Function description
*    This code accounts for the fact that HccaFrameNumber is updated by
*    the HC before the HCD gets an interrupt that will adjust FrameHighPart.
*    If the HC frame counter is run to zero and OhdFrameOverflowInterrupt
*    is not called then the returned 32 bit frame number is correct!
*    No SOF interrupt is needed!
*/
U32 USBH_OHCI_GetFrameNumber(USBH_HC_HANDLE hHostController) {
  U32              v;
  USBH_OHCI_DEVICE * pDev;

  USBH_OHCI_HANDLE_TO_PTR(pDev,hHostController);
  USBH_OCHI_IS_DEV_VALID      (pDev);
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: USBH_OHCI_GetFrameNumber!"));
  USBH_OHCI_UpdateUpperFrameCounter(pDev);
  v  = pDev->pOhHcca->FrameNumber;
  v |= (U32)pDev->UpperFrameCounter << 16;
  return v;
}

/*********************************************************************
*
*       USBH_OHCI_EndpointListEnable
*
*  Function description
*    Sets the endpoint register to enable or disable a list. The current
*    ED pointer register is set to zero and the list filled bit is set
*    if the list has an disable->enable transition. If the parameter
*    ListFill is set always the list filled bit is rewritten.
*/
void USBH_OHCI_EndpointListEnable(USBH_OHCI_DEVICE * pDev, U8 EpType, USBH_BOOL Enable, USBH_BOOL Rescan) {
  U32   val;
  // USBH_WARN((USBH_MTYPE_OHCI, "OHCI: INFO USBH_OHCI_EndpointListEnable: EpType: %d: enable: %d rescan: %d!", EpType,Enable,Rescan));
  val = OhHalReadReg(pDev->pRegBase,OH_REG_CONTROL);
  switch (EpType) {
  case USB_EP_TYPE_CONTROL:
    if (Enable && !(val & HC_CONTROL_CLE)) { // On restart the list
      OhHalWriteReg(pDev->pRegBase, OH_REG_CONTROLCURRENTED, 0);
    }
    if (Rescan) {
      OhHalWriteReg(pDev->pRegBase, OH_REG_COMMANDSTATUS, HC_COMMAND_STATUS_CLF);
    }
    if (Enable) {
      OhHalWriteReg(pDev->pRegBase, OH_REG_CONTROL, val | HC_CONTROL_CLE);
    } else {
      val &= ~HC_CONTROL_CLE;
      OhHalWriteReg(pDev->pRegBase, OH_REG_CONTROL, val);
    }
    break;
  case USB_EP_TYPE_BULK:
    if (Enable && !(val & HC_CONTROL_BLE)) {
      OhHalWriteReg(pDev->pRegBase, OH_REG_BULKCURRENTED, 0);
    }
    if (Enable) {
      OhHalWriteReg(pDev->pRegBase, OH_REG_CONTROL, val | HC_CONTROL_BLE);
    } else {
      val &= ~HC_CONTROL_BLE;
      OhHalWriteReg(pDev->pRegBase, OH_REG_CONTROL, val);
    }
    if (Rescan) {
      OhHalWriteReg(pDev->pRegBase, OH_REG_COMMANDSTATUS, HC_COMMAND_STATUS_BLF);
    }
    break;
  case USB_EP_TYPE_INT:
    if (Enable && !(val & HC_CONTROL_PLE)) {
      OhHalWriteReg(pDev->pRegBase, OH_REG_CONTROL,val | HC_CONTROL_PLE);
    }
    if (!Enable && (val & HC_CONTROL_PLE)) {
      val &=~HC_CONTROL_PLE;
      OhHalWriteReg(pDev->pRegBase, OH_REG_CONTROL,val);
    }
    break;
#if USBH_SUPPORT_ISO_TRANSFER
  case USB_EP_TYPE_ISO:
    if (Enable && (!(val & HC_CONTROL_PLE ) || !(val & HC_CONTROL_IE))) { // On isochronous the periodic list must be also enabled
      OhHalWriteReg(pDev->pRegBase, OH_REG_CONTROL, val | HC_CONTROL_PLE | HC_CONTROL_IE);
    } else {
      if (!Enable && (val & HC_CONTROL_IE))  {
        val &=~HC_CONTROL_IE;
        OhHalWriteReg(pDev->pRegBase,OH_REG_CONTROL, val);
      }
    }
#endif
  default:
    break;
  }
}

/*********************************************************************
*
*       USBH_OHCI_ServiceISR
*
*  Function description
*    Disables the interrupt mask for the HC. It returns TRUE if the interrupt
*    was caused by the device. This function depends from the used USB host controller.
*/
int USBH_OHCI_ServiceISR(USBH_HC_HANDLE hHostController) {
  USBH_OHCI_DEVICE * pDev;
  U32         intStatus, intEnable;
  U8          ret;

  USBH_OHCI_HANDLE_TO_PTR(pDev, hHostController);
  USBH_OCHI_IS_DEV_VALID(pDev);
  intEnable = OhHalReadReg(pDev->pRegBase, OH_REG_INTERRUPTENABLE);
  if (0 != (intEnable &HC_INT_MIE)) {                                   // Master interrupt enable bit is on
    intStatus = OhHalReadReg(pDev->pRegBase, OH_REG_INTERRUPTSTATUS);
    // All interrupt Status bits are checked, Bit 31 (HC_INT_STATUS_VALIDATION_BIT) is always zero
    if (0 != (intStatus &(OH_ENABLED_INTERRUPTS)) && 0 == (intStatus &HC_INT_STATUS_VALIDATION_BIT)) {
      OhHalWriteReg(pDev->pRegBase, OH_REG_INTERRUPTDISABLE, HC_INT_MIE); // disable the interrupt
      ret = TRUE;
    } else {
      ret = FALSE;
    }
  } else {
    ret = FALSE;
  }
  return ret;
}

/*********************************************************************
*
*       USBH_OHCI_ProcessInterrupt
*
*  Function description
*    Process of interrupt requests that are needed from the driver.
*    The calling task must be able to wait.
*/
void USBH_OHCI_ProcessInterrupt(USBH_HC_HANDLE hHostController) {
  USBH_OHCI_DEVICE * pDev;
  U32                intStatus;

  USBH_OHCI_HANDLE_TO_PTR(pDev, hHostController);
  USBH_OCHI_IS_DEV_VALID(pDev);
  intStatus = OhHalReadReg(pDev->pRegBase, OH_REG_INTERRUPTSTATUS);
  if (intStatus & HC_INT_SO) {
    USBH_LOG((USBH_MTYPE_OHCI, "OHCI: PI: Sheduling overrun not handled!"));
  }
  if (intStatus & HC_INT_WDH) { // WD bit is true, read the DoneHead Register from HCCA
    USBH_LOG((USBH_MTYPE_OHCI, "OHCI: [PI: HC_INT_WDH!"));
    USBH_OHCI_TRANSFER_ProcessDoneInterrupt(pDev);
    USBH_LOG((USBH_MTYPE_OHCI, "OHCI: ]PI: HC_INT_WDH!"));
  }
  if (intStatus & HC_INT_SF) {
    USBH_LOG((USBH_MTYPE_OHCI, "OHCI: PI: Start of frame not handled!"));
  }
  if (intStatus & HC_INT_RD) {
    USBH_LOG((USBH_MTYPE_OHCI, "OHCI: PI: Resume detected not handled!"));
  }
  if (intStatus & HC_INT_UE) {
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: PI: UnrecoverableError not handled ! "));
    while (1);
  }
  if (intStatus & HC_INT_FNO) {  // Frame number overflow
    USBH_LOG((USBH_MTYPE_OHCI, "OHCI: PI: HC_INT_FNO!"));
    USBH_OHCI_UpdateUpperFrameCounter(pDev);
  }
  if (intStatus & HC_INT_RHSC) { // Root hub Status change
    USBH_LOG((USBH_MTYPE_OHCI, "OHCI: PI: HC_INT_RHSC!"));
    USBH_OHCI_ROOTHUB_ProcessInterrupt(&pDev->RootHub);
  }
  if (intStatus & HC_INT_OC) {
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: PI: OwnershipChange is not handled !"));
  }
  OhHalWriteReg(pDev->pRegBase, OH_REG_INTERRUPTSTATUS, intStatus);  // Clear interrupt Status bits and enable the interrupt
  OhHalWriteReg(pDev->pRegBase, OH_REG_INTERRUPTENABLE, HC_INT_MIE); // Now the ISR detects the next interrupt
}

USBH_HOST_DRIVER USBH_OHCI_Driver = {
  USBH_OHCI_HostInit,
  USBH_OHCI_HostExit,
  USBH_OHCI_SetHcState,
  USBH_OHCI_GetFrameNumber,
  USBH_OHCI_AddEndpoint,
  USBH_OHCI_ReleaseEndpoint,
  _AbortEndpoint,
  _ResetEndpoint,
  USBH_OHCI_TRANSFER_SubmitRequest,
  USBH_OHCI_ROOTHUB_GetPortCount,
  USBH_OHCI_ROOTHUB_GetPowerGoodTime,
  USBH_OHCI_ROOTHUB_GetHubStatus,
  USBH_OHCI_ROOTHUB_ClearHubStatus,
  USBH_OHCI_ROOTHUB_GetPortStatus,
  USBH_OHCI_ROOTHUB_ClearPortStatus,
  USBH_OHCI_ROOTHUB_SetPortPower,
  USBH_OHCI_ROOTHUB_ResetPort,
  USBH_OHCI_ROOTHUB_DisablePort,
  USBH_OHCI_ROOTHUB_SetPortSuspend,
  USBH_OHCI_ServiceISR,
  USBH_OHCI_ProcessInterrupt
};

/*********************************************************************
*
*       USBH_OHCI_Add
*
*  Function description:
*/
void USBH_OHCI_Add(void * pBase) {
  USBH_OHCI_DEVICE * pHostDevice;
  USBH_HC_HANDLE   hHostController;

  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: [USBH_OHCI_Add!"));
  hHostController = USBH_OHCI_CreateController(pBase);

  USBH_ASSERT               (hHostController != NULL);
  USBH_OHCI_HANDLE_TO_PTR(pHostDevice, hHostController);
  USBH_OCHI_IS_DEV_VALID      (pHostDevice);
  pHostDevice->hBusDriver     = USBH_AddHostController(&USBH_OHCI_Driver, hHostController);
  if (pHostDevice->hBusDriver == NULL) { // The OHCI controller can be attached to the USB Bus driver
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: OHC_AddController: BD_AddHostController!"));
  }
  USBH_Global.hHC = hHostController;
  USBH_Global.hHCBD = pHostDevice->hBusDriver;
  USBH_Global.pDriver = &USBH_OHCI_Driver;
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: ]USBH_OHCI_Add!"));
}

/********************************* EOF*******************************/
