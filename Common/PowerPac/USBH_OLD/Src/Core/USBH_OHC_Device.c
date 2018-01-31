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
*       OhdInitHccaInterruptTable
*
*  Function description
*    Build an interrupt endpoint tree with dummy endpoints. The HCCA
*    interrupt table is initialized but not activated !!!
*/
static USBH_STATUS OhdInitHccaInterruptTable(HC_DEVICE * Dev) {
  USBH_STATUS   status;
  status      = OhInitAllocDummyInterruptEndpoints(Dev);        // 1. Allocate all needed dummy interrupts
  if (status) {
    goto exit;
  }
  OhIntBuildDummyEpTree(Dev);                                   // 2. Links all dummy Eps in the devices DummyInterruptEpArr list.
  OhHccaSetInterruptTable(Dev->Hcca, Dev->DummyInterruptEpArr); // 3. Initialize the HCCA interrupt table entries

exit:
  if (status) {
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: OhdInitHccaInterruptTable"));
  }
  return status;
}

/*********************************************************************
*
*       OhdIsTimeOver 
*
*  Function description
*    Returns TRUE if the time is over.
*
*  Parameters:
*    Waittime:  in microseconds
*    StartTime: in microseconds
*/
T_BOOL OhdIsTimeOver(U32 Waittime, U32 StartTime) {
  if (((U32)((USBH_OS_GetTime32() * 1000) - StartTime)) >= Waittime) {
    return TRUE;
  } else {
    return FALSE;
  }
}

/*********************************************************************
*
*       OhdEnableInterrupt
*
*  Function description
*    Global interrupt enable
*/
static void OhdEnableInterrupt(HC_DEVICE * dev) {
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: Enable master INT!"));
  OhHalWriteReg(dev->RegBase, OH_REG_INTERRUPTENABLE, HC_INT_MIE); // Set the new state
}

/*********************************************************************
*
*       OhdDisableInterrupt
*
*  Function description
*    Global interrupt disable
*/
static void OhdDisableInterrupt(HC_DEVICE * dev) {
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: Disable master INT!"));
  OhHalWriteReg(dev->RegBase, OH_REG_INTERRUPTDISABLE, HC_INT_MIE); // Set the new state
}

/*********************************************************************
*
*       OhdSetInterrupts
*
*  Function description
*    Enables interrupt bits
*/
static void OhdSetInterrupts(HC_DEVICE * dev, U32 IntMask) {
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: OhdEnableInterruptBits: Mask: 0x%lx", IntMask));
  OhHalWriteReg(dev->RegBase, OH_REG_INTERRUPTENABLE, IntMask);
}

/*********************************************************************
*
*       OhdInitIntervalReg
*
*  Function description
*/
static void OhdInitIntervalReg(HC_DEVICE * dev) {
  U32 temp;
  OH_DEV_VALID(dev);
  dev->FmIntervalReg = OH_DEV_DEFAULT_FMINTERVAL; // Check if exist an interval register
  temp = OhHalReadReg(dev->RegBase, OH_REG_FMINTERVAL);
  // Toggle the Frame interval bit and write the new frame bit times
  temp ^= HC_FM_INTERVAL_FIT;
  temp &= HC_FM_INTERVAL_FIT;
  temp |= dev->FmIntervalReg;
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: OhdInitIntervalReg: FmIntervalReg: 0x%lx", temp));
  OhHalWriteReg(dev->RegBase, OH_REG_FMINTERVAL, temp);
  // Calculate the periodic start bit time from frame interval
  temp = ((9 * (dev->FmIntervalReg &HC_FM_INTERVAL_FI_MASK)) / 10) & 0x3fff;
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: OhdInitIntervalReg: OH_REG_PERIODICSTART: 0x%lu", temp));
  OhHalWriteReg(dev->RegBase, OH_REG_PERIODICSTART, temp);
  OhHalWriteReg(dev->RegBase, OH_REG_LSTHRESHOLD, OH_DEV_LOW_SPEED_THRESHOLD);
}

/*********************************************************************
*
*       OhdSetHcFuncState
*
*  Function description
*/
static USBH_STATUS OhdSetHcFuncState(HC_DEVICE * dev, U32 HcFuncState) {
  USBH_STATUS status;
  U32         control;
  status   = USBH_STATUS_SUCCESS;
  control  = OhHalReadReg(dev->RegBase, OH_REG_CONTROL);
  control &= ~HC_CONTROL_HCFS;
  switch (HcFuncState) {
    case HC_USB_RESET:
      dev->state = OH_DEV_HALT;
      USBH_LOG((USBH_MTYPE_OHCI, "OHCI: OhdSetHcFuncState: Reset!"));
      control |= HcFuncState;
      break;
    case HC_USB_RESUME:
      dev->state = OH_DEV_RESUME;
      if (dev->state != OH_DEV_SUSPEND) {
        USBH_WARN((USBH_MTYPE_OHCI, "OHCI: OhdSetHcFuncState: not suspend!"));
        status = USBH_STATUS_ERROR;
        break;
      }
      control |= HcFuncState;
      USBH_LOG((USBH_MTYPE_OHCI, "OHCI: OhdSetHcFuncState: Resume!"));
      break;
    case HC_USB_OPERATIONAL:
      dev->state = OH_DEV_RUNNING;
      USBH_LOG((USBH_MTYPE_OHCI, "OHCI: OhdSetHcFuncState: new State: Operational"));
      control |= HcFuncState;
      break;
    case HC_USB_SUSPEND:
      dev->state = OH_DEV_SUSPEND;
      if (dev->state != OH_DEV_RUNNING) {
        USBH_WARN((USBH_MTYPE_OHCI, "OHCI: OhdSetHcFuncState: not running!"));
        status = USBH_STATUS_ERROR;
        break;
      }
      USBH_LOG((USBH_MTYPE_OHCI, "OHCI: OhdSetHcFuncState: Suspend"));
      control |= HcFuncState;
    default: // On suspend do nothing
      break;
  }
  OhHalWriteReg(dev->RegBase, OH_REG_CONTROL, control); // Set the new state
  return status;
}

/*********************************************************************
*
*       OhdSoftReset
*
*  Function description
*    OhdReset resets the host controller register. The host is then in suspend state
*/
static USBH_STATUS OhdSoftReset(HC_DEVICE * Dev) {
  U32         command, time, i, reg;
  USBH_STATUS status;
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: OhdReset"));
  OH_DEV_VALID(Dev);
  reg = OhHalReadReg(Dev->RegBase, OH_REG_FMINTERVAL);                      // Store the HcFmInterval
  OhdSetHcFuncState(Dev, HC_USB_RESET);                                     // Additional USB bus Reset
  time = USBH_OS_GetTime32();
  while (!OhdIsTimeOver(OH_RESET_STATE_TIMEOUT, time));                     // Busy wait
  OhHalWriteReg(Dev->RegBase, OH_REG_COMMANDSTATUS, HC_COMMAND_STATUS_HCR); // Set sotware reset
  status = USBH_STATUS_ERROR;
  i = OH_TIME_SOFTWARE_RESET;
  time = USBH_OS_GetTime32();
  do {
    while (!OhdIsTimeOver(1, time));                                        // Wait 1us
    time = USBH_OS_GetTime32();
    command = OhHalReadReg(Dev->RegBase, OH_REG_COMMANDSTATUS);
    if (0 == (command &HC_COMMAND_STATUS_HCR)) {
      status = USBH_STATUS_SUCCESS;                                         // This bit is cleared by HC upon the completion of the reset operation.
      break;
    }
  } while (--i);

#if (USBH_DEBUG > 1)

  if (status) {
    USBH_WARN((USBH_MTYPE_OHCI, " OhdSoftReset!"));
  }

#endif

  OhHalWriteReg(Dev->RegBase, OH_REG_FMINTERVAL, reg);
  return status;
}

/*********************************************************************
*
*       OhdCheckRevision
*
*  Function description
*/
static USBH_STATUS OhdCheckRevision(HC_DEVICE * dev) {
  U32   rev;
  rev = OhHalReadReg(dev->RegBase, OH_REG_REVISION);
  if ((rev &OH_REVISION_MASK) != OH_REVISION) {
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: OhdCheckRevision: Wrong OHCI revision exp.:0x%x rcv:0x%x", OH_REVISION, rev & OH_REVISION_MASK));
    return USBH_STATUS_ERROR;
  } else {
    USBH_LOG((USBH_MTYPE_OHCI, "OHCI: INFO OhdCheckRevision: OH_REG_REVISION content: 0x%x", rev)); // Prints out all values
    return USBH_STATUS_SUCCESS;
  }
}

/*********************************************************************
*
*       OhdAllocTimers
*
*  Function description
*/
static USBH_STATUS OhdAllocTimers(HC_DEVICE * dev) {
  dev->ControlEpRemoveTimer      = USBH_AllocTimer(OhEp0ReleaseEp_TimerCallback, dev); // Allocate all device object specific timer
  if (dev->ControlEpRemoveTimer == NULL) {
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: OhdInitDLists: USBH_AllocTimer!"));
    return USBH_STATUS_RESOURCES;
  }
  dev->ControlEpAbortTimer       = USBH_AllocTimer(OhEp0AbortUrb_TimerCallback, dev);
  if (dev->ControlEpAbortTimer  == NULL) {
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: OhdInitDLists: USBH_AllocTimer!"));
    return USBH_STATUS_RESOURCES;
  }
  dev->BulkEpRemoveTimer         = USBH_AllocTimer(OhBulkRemoveEp_TimerCallback, dev);
  if (dev->BulkEpRemoveTimer    == NULL) {
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: OhdInitDLists: USBH_AllocTimer!"));
    return USBH_STATUS_RESOURCES;
  }
  dev->BulkEpAbortTimer          = USBH_AllocTimer(OhBulkAbortUrb_TimerCallback, dev);
  if (dev->BulkEpAbortTimer     == NULL) {
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: OhdInitDLists: USBH_AllocTimer!"));
    return USBH_STATUS_RESOURCES;
  }
  dev->IntEpRemoveTimer          = USBH_AllocTimer(OhIntReleaseEp_TimerCallback, dev);
  if (dev->IntEpRemoveTimer     == NULL) {
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: OhdInitDLists: USBH_AllocTimer!"));
    return USBH_STATUS_RESOURCES;
  }
  dev->IntEpAbortTimer           = USBH_AllocTimer(OhIntAbortUrb_TimerCallback, dev);
  if (dev->IntEpAbortTimer      == NULL) {
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: OhdInitDLists: USBH_AllocTimer!"));
    return USBH_STATUS_RESOURCES;
  }
  return USBH_STATUS_SUCCESS;
}

/*********************************************************************
*
*       OhdCancelTimers
*
*  Function description
*/
static void OhdCancelTimers(HC_DEVICE * dev) {
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: OhdCancelTimers!"));
  // Ep0 remove and abort timers
  if (dev->ControlEpRemoveTimer) {
    dev->ControlEpRemoveTimerCancelFlag = TRUE;
    USBH_CancelTimer(dev->ControlEpRemoveTimer);
  }
  if (dev->ControlEpAbortTimer) {
    dev->ControlEpAbortTimerCancelFlag = TRUE;
    USBH_CancelTimer(dev->ControlEpAbortTimer);
  }
  // Bulk remove and abort timers
  if (dev->BulkEpRemoveTimer) {
    dev->BulkEpRemoveTimerCancelFlag = TRUE;
    USBH_CancelTimer(dev->BulkEpRemoveTimer);
  }
  if (dev->BulkEpAbortTimer) {
    dev->BulkEpAbortTimerCancelFlag = TRUE;
    USBH_CancelTimer(dev->BulkEpAbortTimer);
  }
  // Int remove and abort timers
  if (dev->IntEpRemoveTimer) {
    dev->IntEpRemoveTimerCancelFlag = TRUE;
    USBH_CancelTimer(dev->IntEpRemoveTimer);
  }
  if (dev->IntEpAbortTimer) {
    dev->IntEpAbortTimerCancelFlag = TRUE;
    USBH_CancelTimer(dev->IntEpAbortTimer);
  }
}

/*********************************************************************
*
*       OhdFreeTimers
*
*  Function description
*/
static void OhdFreeTimers(HC_DEVICE * dev) {
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: OhdFreeTimers!"));
  // Ep0 remove and abort timers
  if (dev->ControlEpRemoveTimer) {
    USBH_FreeTimer(dev->ControlEpRemoveTimer);
    dev->ControlEpRemoveTimer = NULL;
  }
  if (dev->ControlEpAbortTimer) {
    USBH_FreeTimer(dev->ControlEpAbortTimer);
    dev->ControlEpAbortTimer = NULL;
  }
  // Bulk remove and abort timers
  if (dev->BulkEpRemoveTimer) {
    USBH_FreeTimer(dev->BulkEpRemoveTimer);
    dev->BulkEpRemoveTimer = NULL;
  }
  if (dev->BulkEpAbortTimer) {
    USBH_FreeTimer(dev->BulkEpAbortTimer);
    dev->BulkEpAbortTimer = NULL;
  }
  // Int remove and abort timers
  if (dev->IntEpRemoveTimer) {
    USBH_FreeTimer(dev->IntEpRemoveTimer);
    dev->IntEpRemoveTimer = NULL;
  }
  if (dev->IntEpAbortTimer) {
    USBH_FreeTimer(dev->IntEpAbortTimer);
    dev->IntEpAbortTimer = NULL;
  }
}

/*********************************************************************
*
*       OhdInitDLists
*
*  Function description
*/
static void OhdInitDLists(HC_DEVICE * dev) {
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: OhdInitDLists!"));
  // Init all devices dlists
  DlistInit(&dev->ControlEpList);
  dev->ControlEpCount = 0;
  DlistInit(&dev->BulkEpList);
  dev->BulkEpCount    = 0;
  dev->IntEpCount     = 0;
  DlistInit(&dev->IsoEpList);
  dev->IsoEpCount     = 0;
}

/*********************************************************************
*
*       OhdValidDListsOnExit
*
*  Function description
*/
static void OhdValidDListsOnExit(HC_DEVICE * dev) {
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: OhdValidDListsOnExit!"));
  OH_DEV_VALID(dev);
  if (!DlistEmpty(&dev->ControlEpList)) {
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: OhdCheckDLists: ControlEpList not empty!"));
  }
  if (dev->ControlEpCount) {
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: OhdCheckDLists: ControlEpCount not zero!"));
  }
  if (!DlistEmpty(&dev->BulkEpList)) {
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: OhdCheckDLists: BulkEpList not empty!"));
  }
  if (dev->BulkEpCount) {
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: OhdCheckDLists: BulkEpCount not zero!"));
  }
  if (dev->IntEpCount) {
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: OhdCheckDLists: IntEpCount not zero!"));
  }
  if (!DlistEmpty(&dev->IsoEpList)) {
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: OhdCheckDLists: IsoEpList not empty!"));
  }
  if (dev->IntEpCount) {
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: OhdCheckDLists: IsoEpCount not zero!"));
  }
}

/*********************************************************************
*
*       OhdValidDListsOnExit
*
*  Function description
*    Adds a control or an bulk dummy endpoint. An empty Transfer Descriptor is also added.
*/
static USBH_STATUS OhdAddControlBulkDummyEndpoint(HC_DEVICE * Dev, U8 EpType) {
  OHD_EP0         * ep0;
  OHD_BULK_INT_EP * bulkEp;
  USBH_STATUS       status;
  OH_DEV_VALID(Dev);
  status = USBH_STATUS_SUCCESS;
  switch (EpType) {
    case USB_EP_TYPE_CONTROL:
      ep0 = OhEp0Get(&Dev->ControlEPPool, &Dev->SetupPacketPool); // Get memory from pools
      if (ep0 == NULL) {
        USBH_WARN((USBH_MTYPE_OHCI, "OHCI: OhdAddDummyEndpoint: OhEp0Get!"));
        break;
      }
      status = OhEp0Init(ep0, Dev, OH_DUMMY_ED_EPFLAG,            // Dummy ED
      OH_DEFAULT_DEV_ADDR, OH_DEFAULT_EP_ADDR, OH_DEFAULT_MAX_PKT_SIZE, OH_DEFAULT_SPEED);
      if (status) {                                               // On error
        USBH_WARN((USBH_MTYPE_OHCI, "OHCI: OhdAddDummyEndpoint: OhEp0Init!"));
        OhEp0Put(ep0);
        break;
      }
      OhEp0Insert(ep0);
      OhdEndpointListEnable(Dev, USB_EP_TYPE_CONTROL, TRUE, TRUE);
      break;
    case USB_EP_TYPE_BULK:
      bulkEp = OhBulkIntEpGet(&Dev->BulkEPPool);

      if (bulkEp == NULL) {
        USBH_WARN((USBH_MTYPE_OHCI, "OHCI: OhdAddDummyEndpoint: OhBulkIntEpGet!"));
        break;
      }
      bulkEp = OhBulkIntInitEp(bulkEp,                                                  // Endpoint
      Dev, EpType, OH_DEFAULT_DEV_ADDR, OH_DEFAULT_EP_ADDR, OH_DEFAULT_MAX_PKT_SIZE, 0, // Interval is zero
      USBH_FULL_SPEED,                                                                  // Bulk on low speed not available
      OH_DUMMY_ED_EPFLAG                                                                // Endpoint mask
      );
      if (bulkEp == NULL) {
        USBH_WARN((USBH_MTYPE_OHCI, "OHCI: OhdAddDummyEndpoint: OhBulkIntInitEp!"));
        status = USBH_STATUS_RESOURCES;
        break;
      }
      OhBulkInsertEndpoint(bulkEp);
      break;
#if OH_ISO_ENABLE

    case USB_EP_TYPE_ISO:
      break;

#endif

    default:
      USBH_WARN((USBH_MTYPE_OHCI, "OHCI: OhdAddDummyEndpoint:invalid ep-type!"));
      break;
  }
  return status;
}

/*********************************************************************
*
*       OhdSetupOHCI
*
*  Function description
*    Sets up the host controller. The host controller must be in one
*    of the following state before this functions is called:
*    - operational state
*    - reset state
*    - resume state
*    Initializes all needed host register and sets the reset state!
*/
static USBH_STATUS OhdSetupOHCI(HC_DEVICE * Dev) {
  USBH_STATUS status;
  OhdDisableInterrupt(Dev); // Disables all USB host interrupts
  // After software reset adds an dummy endpoint to each transfer type and link this ep with the host phy. address register
  // This functions writes the phy. start addresses of lists in register.
  status = OhdAddControlBulkDummyEndpoint(Dev, USB_EP_TYPE_CONTROL);
  if (status) {
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: OhdSetupOHCI: OhdAddDummyEndpoint: Ep0!"));
    return status;
  }
  status = OhdAddControlBulkDummyEndpoint(Dev, USB_EP_TYPE_BULK);

  if (status) {
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: OhdSetupOHCI: OhdAddDummyEndpoint: Bulk!"));
    return status;
  }
  status = OhdInitHccaInterruptTable(Dev); // Adds all interrupt endpoints
  if (status) {
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: OhdSetupOHCI: OhdInitHccaInterruptTable!"));
    return status;
  }
  OhHalWriteReg(Dev->RegBase, OH_REG_HCCA, Dev->Hcca->ItemHeader.PhyAddr); // Set the HCCA phy. address
  OhdInitIntervalReg(Dev);                                                 // Set frame interval
  OhdEndpointListEnable(Dev, USB_EP_TYPE_INT, TRUE, TRUE);                 // Enable periodic list
  OhdSetInterrupts(Dev, OH_ENABLED_INTERRUPTS);                            // Enable interrupts DOEN,unrecoverable error ,root hub status change
  OhdEnableInterrupt(Dev);                                                 // Enable master interrupts
  return status;
}

/*********************************************************************
*
*       OhdUnlinkHostResources
*
*  Function description
*/
static void OhdUnlinkHostResources(HC_DEVICE * dev) {
  U32 time;
  U32 intStatus;
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: OhdUnlinkHostResources!"));
  if (dev == NULL) {
    USBH_LOG((USBH_MTYPE_OHCI, "OHCI: INFO OhdUnlinkHostResources: parameter dev is NULL, return!"));
    return;
  }
  OH_DEV_VALID(dev);
  // First stop all list processing
  OhdEndpointListEnable(dev, USB_EP_TYPE_CONTROL, FALSE, FALSE);
  OhdEndpointListEnable(dev, USB_EP_TYPE_BULK,    FALSE, FALSE);
  OhdEndpointListEnable(dev, USB_EP_TYPE_INT,     FALSE, FALSE);
  OhdEndpointListEnable(dev, USB_EP_TYPE_ISO,     FALSE, FALSE);
  OhdCancelTimers(dev);                                    // Stop all timer
  OhInt_RemoveAllUserEDFromPhysicalLink(dev);              // Remove physical links on all physical linked endpoints
  time = USBH_OS_GetTime32();
  while (!OhdIsTimeOver(OH_STOP_DELAY_TIME * 1000, time)); // *1000 -> in us
  // Disable the host controller interrupt and clear all interrupt status bits!
  OhdDisableInterrupt(dev);
  intStatus = OhHalReadReg(dev->RegBase, OH_REG_INTERRUPTSTATUS);
  OhHalWriteReg(dev->RegBase, OH_REG_INTERRUPTSTATUS, intStatus & OH_ENABLED_INTERRUPTS);
  if (dev->ControlEpCount > 1) {
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: OHC_HostExit: open control endpoints: %u !", dev->BulkEpCount));
  }
  OhEp0RemoveEndpoints(dev, TRUE);                         // Used endpoint and the dummy endpoint are released
  if (dev->BulkEpCount > 1) {
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: OHC_HostExit: open bulk endpoints: %u !", dev->BulkEpCount));
  }
  OhBulkRemoveEndpoints(dev, TRUE);                        // Used endpoint and the dummy endpoint are released
  if (dev->IntEpCount) {
  // User interrupt endpoints must be zero because there dummy endpoints are not counted
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: OHC_HostExit: open interrupt endpoints: %u != 0!", dev->IntEpCount)); 
  }
  OhIntRemoveEDFromLogicalListAndFree(dev, TRUE);          // Remove all endpoints with the UNLINK state
  OhIntPutAllDummyEp(dev);
  if (dev->IsoEpCount) {
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: OHC_HostExit: IsoEpCount: %u !", dev->IsoEpCount));
  }
  OhdValidDListsOnExit(dev);                               // Check that all dlists and endpoint counter are zero
}

/*********************************************************************
*
*       OhdDeleteDevice
*
*  Function description
*/
static void OhdDeleteDevice(HC_DEVICE * dev) {
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: OhdDeleteDevice!"));
  if (dev == NULL) {
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: OhdDeleteDevice: param. dev is NULL!"));
    return;
  }
  OH_DEV_VALID(dev);
  OhdFreeTimers(dev); // OhdFreeTimers can called twice
  // Frees Control, Bulk, Interrupt dummy interrupt, transfer descriptor and HCCA pool
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: Frees Control EP and setup packet Pool!"));
  HcmFreePool(&dev->ControlEPPool);
  HcmFreePool(&dev->SetupPacketPool);
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: Frees BulkEPPool!"));
  HcmFreePool(&dev->BulkEPPool);
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: Frees IntEPPool!"));
  HcmFreePool(&dev->IntEPPool);
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: Frees DummyIntEPPool!"));
  HcmFreePool(&dev->DummyIntEPPool);
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: Frees GTDPool!"));
  HcmFreePool(&dev->GTDPool);
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: [Frees HccaPool!"));
  OhHccaRelease(dev->Hcca);
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: ]Frees HccaPool!"));
  HcmFreePool(&dev->TransferBufferPool);
  USBH_Free(dev);
}


/*********************************************************************
*
*       Bus driver callback functions
*
**********************************************************************
*/

/*********************************************************************
*
*       Ohd_AbortEndpoint
*
*  Function description
*/
static USBH_STATUS Ohd_AbortEndpoint(USBH_HC_EP_HANDLE EpHandle) {
  OHD_EP0     * ep0;
  USBH_STATUS   status;
  ep0         = (OHD_EP0 *)EpHandle; // Field type is the first field after the item header field
  status      = USBH_STATUS_INVALID_PARAM;
  switch (ep0->EndpointType) {       // The element EndpointType has in all endpoint structs the same offset in the struct
    case USB_EP_TYPE_CONTROL:
      status = OhEp0AbortEndpoint(ep0);
      break;
    case USB_EP_TYPE_INT:
    case USB_EP_TYPE_BULK: {
      OHD_BULK_INT_EP * bulkEp;
      bulkEp = (OHD_BULK_INT_EP *)EpHandle;
      status = OhBulkIntAbortEndpoint(bulkEp);
      if (status) {
        USBH_WARN((USBH_MTYPE_OHCI, "OHCI: Ohd_AbortEndpoint: OhBulkIntAbortEndpoint status: 0x%x!", status));
      }
    }
      break;
    case USB_EP_TYPE_ISO:
      break;
    default:
      USBH_LOG((USBH_MTYPE_OHCI, "OHCI: Ohd_AbortEndpoint: invalid endpoint type: %u!", ep0->EndpointType));
      ;
  }
  return status;
}

/*********************************************************************
*
*       Ohd_ResetEndpoint
*
*  Function description
*    Resets the data toggle bit to 0. The bus driver takes care that
*    this function is called only if no pending URB is scheduled.
*/
static USBH_STATUS Ohd_ResetEndpoint(USBH_HC_EP_HANDLE EpHandle) {
  OHD_BULK_INT_EP * ep;
  USBH_STATUS       status;
  ep              = (OHD_BULK_INT_EP *)EpHandle; // Field type iisthe first field after the item header field
  status          = USBH_STATUS_INVALID_PARAM;
  switch (ep->EndpointType) {
    case USB_EP_TYPE_BULK:
    case USB_EP_TYPE_INT:
      USBH_LOG((USBH_MTYPE_OHCI, "OHCI: Ohd_ResetEndpoint: DevAddr.:%u Ep: 0x%x !", ep->DeviceAddress, ep->EndpointAddress));
      if (ep->State != OH_EP_LINK) {
        USBH_WARN((USBH_MTYPE_OHCI, "OHCI: Ohd_ResetEndpoint: Ep state is not linked!"));
        status = USBH_STATUS_ERROR;
        break;
      }
      if (ep->UrbCount) {
        USBH_WARN((USBH_MTYPE_OHCI, "OHCI: Ohd_ResetEndpoint: Pending URBs!"));
      }
      OhEpClearToggle(&ep->ItemHeader);
      OhEpGlobClearHalt(&ep->ItemHeader);
      ep->HaltFlag = FALSE;                      // Rest with Ohd_ResetEndpoint
      OhBulkIntSubmitUrbsFromList(ep);           // Try to submit the next URB from list
      status = USBH_STATUS_SUCCESS;
      break;
    default:
      ;
      USBH_LOG((USBH_MTYPE_OHCI, "OHCI: Ohd_ResetEndpoint: invalid endpoint type: %u!", ep->EndpointType));
  }
  return status;
}

/*********************************************************************
*
*       OhAllocCopyTransferBufferPool
*
*  Function description
*/
static USBH_STATUS OhAllocCopyTransferBufferPool(HCM_POOL * bufferPool, U32 bufferSize, U32 numberOfBuffers) {
  USBH_STATUS status = USBH_STATUS_SUCCESS;
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: OhAllocTransferBufferPool: bufferSize: %d numberOfBuffers:%d!", numberOfBuffers, bufferSize));
  status             = HcmAllocPool(bufferPool, numberOfBuffers, bufferSize, sizeof(OH_TRANSFER_BUFFER), USBH_TRANSFER_BUFFER_ALIGNMENT);
  if (status) { // On error
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: OhAllocTransferBufferPool: HcmAllocPool!"));
  }
  return status;
}

/*********************************************************************
*
*       OhGetCopyTransferBuffer
*
*  Function description
*/
OH_TRANSFER_BUFFER * OhGetCopyTransferBuffer(HCM_POOL * transferBufferPool) {
  OH_TRANSFER_BUFFER * item;
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: OhGetTransferBuffer!"));
  item = (OH_TRANSFER_BUFFER *)HcmGetItem(transferBufferPool);
  if (NULL == item) {
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: OhGetTransferBuffer: no resources!"));
  }
  return item;
}

/*********************************************************************
*
*       OhGetInitializedCopyTransferBuffer
*
*  Function description
*/
OH_TRANSFER_BUFFER * OhGetInitializedCopyTransferBuffer(HCM_POOL * transferBufferPool, U8 * urbBuffer, U32 urbBufferLength) {
  OH_TRANSFER_BUFFER * item;
  item               = OhGetCopyTransferBuffer(transferBufferPool);
  if (NULL == item) {
    return NULL;
  }
  item->UrbBuffer       = urbBuffer;
  item->RemainingLength = urbBufferLength;
  item->Transferred     = 0;
  return item;
}

/*********************************************************************
*
*       OhGetBufferLengthFromCopyTransferBuffer
*
*  Function description
*    Return the remaining transfer length and an virt. pointer to the
*    physical transfer buffer
*/
U8 * OhGetBufferLengthFromCopyTransferBuffer(OH_TRANSFER_BUFFER * transferBuffer, U32 * length) {
  U8       * buffer;
  * length = USBH_MIN(transferBuffer->RemainingLength, USBH_Global.Config.TransferBufferSize);
  buffer   = (U8 *)transferBuffer->ItemHeader.VirtAddr;
  return buffer;
}

/*********************************************************************
*
*       OhUpdateCopyTransferBuffer
*
*  Function description
*/
U32 OhUpdateCopyTransferBuffer(OH_TRANSFER_BUFFER * transferBuffer, U32 transferred) {
  T_ASSERT_PTR(transferBuffer->UrbBuffer);
  if (transferBuffer->RemainingLength < transferred) {
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: OhUpdateTransferBuffer:RemainingLength: %d < transferLength: %d ", transferBuffer->RemainingLength, transferred));
    transferred = transferBuffer->RemainingLength;
  }
  // Update buffer pointer and length
  transferBuffer->UrbBuffer += transferred;
  transferBuffer->Transferred += transferred;
  transferBuffer->RemainingLength -= transferred;
  return transferBuffer->RemainingLength;
}

/*********************************************************************
*
*       OhFillCopyTransferBuffer
*
*  Function description
*
*  Parameters:
*    transferBuffer: pointer to the transfer buffer
*    transferLength: IN:  transferred length or zero at the first time
*                    OUT: length to send to the USB device
*  Return value:
*    Pointer to the beginning of the transfer buffer
*/
U32 OhFillCopyTransferBuffer(OH_TRANSFER_BUFFER * transferBuffer) {
  U8  * phy_buffer;
  U32   length;
  T_ASSERT_PTR(transferBuffer->UrbBuffer);
  phy_buffer = transferBuffer->ItemHeader.VirtAddr;
  length = USBH_MIN(transferBuffer->RemainingLength, USBH_Global.Config.TransferBufferSize);
  if (length) {
    USBH_MEMCPY(phy_buffer, transferBuffer->UrbBuffer, length);
  }
  return length;
}

/*********************************************************************
*
*       OhCopyToUrbBufferUpdateTransferBuffer
*
*  Function description
*
*  Parameters:
*    transferBuffer: pointer to the transfer buffer
*    transferLength: IN:  transferred length
*                    OUT: length to send to the USB device
*  Return value:     Pointer to the beginning of the transfer buffer
*/
U32 OhCopyToUrbBufferUpdateTransferBuffer(OH_TRANSFER_BUFFER * transferBuffer, U32 transferred) {
  U8  * phy_buffer;
  U32   length;
  T_ASSERT_PTR(transferBuffer->UrbBuffer);
  if (transferBuffer->RemainingLength < transferred) {
    length = transferBuffer->RemainingLength;
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: OhUpdateTransferBuffer:RemainingLength: %d < transferLength: %d ", transferBuffer->RemainingLength, transferred));
  } else {
    length = transferred;
  }
  phy_buffer = transferBuffer->ItemHeader.VirtAddr;
  if (length) {
    USBH_MEMCPY(transferBuffer->UrbBuffer, phy_buffer, length);
  }
  return OhUpdateCopyTransferBuffer(transferBuffer, length); // Update lengths and pointers
}

/***************************************************************

        Global Host device driver interface functions

***************************************************************/


/*********************************************************************
*
*       USBH_OHC_CreateController
*
*  Function description
*    Allocates all needed resources for a host controller device object and calls
*    USBH_AddHostController to link this driver to the next upper driver object
*
*  Return value: a valid handle or NULL on error.
*/
USBH_HC_HANDLE USBH_OHC_CreateController(void * BaseAddress) {
  HC_DEVICE   * dev;
  USBH_STATUS   status;

  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: [OHC_CreateController! BaseAddress: 0x%lx ", BaseAddress));
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: OHC_CreateController: OH_TOTAL_CONTIGUOUS_MEMORY: %d", OH_OHCI_MEMORY_SIZE + USBH_Global.Config.TransferBufferSize));
  T_ASSERT(BaseAddress != NULL);
  dev = USBH_Malloc(sizeof(HC_DEVICE));
  if (dev == NULL) {
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: OHC_CreateController: malloc HC_DEVICE!"));
    status = USBH_STATUS_MEMORY;
    goto alloc_err;
  }
  ZERO_MEMORY(dev, sizeof(HC_DEVICE));
#if (USBH_DEBUG > 1)

  dev->Magic = HC_DEVICE_MAGIC;

#endif

  dev->RegBase = BaseAddress; // todo: set base address
  OhdInitDLists(dev);
  status       = OhdAllocTimers(dev);
  if (status) {
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: OHC_CreateController: OhdAllocTimers!"));
    goto alloc_err;
  }
  // Allocate all resources for endpoint and transfer descriptors
  dev->Hcca = OhHccaAlloc(&dev->HccaPool);
  if (NULL == dev->Hcca) {
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: OHC_CreateController: OhHccaInit!"));
    goto alloc_err;
  }
  status = OhAllocCopyTransferBufferPool(&dev->TransferBufferPool, USBH_Global.Config.TransferBufferSize, 4);
  if (status) { // On error
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: OHC_CreateController:  OhAllocCopyTransferBufferPool!"));
    goto alloc_err;
  }
  status = OhEp0Alloc(&dev->ControlEPPool, &dev->SetupPacketPool, OH_DEV_MAX_CONTROL_EP);
  if (status) { // On error
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: OHC_CreateController:  OhEp0Alloc!"));
    goto alloc_err;
  }
  status = OhBulkIntAllocPool(&dev->BulkEPPool, OH_DEV_MAX_BULK_EP);
  if (status) { // On error
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: OHC_CreateController:  OhBulkIntAllocPool!"));
    goto alloc_err;
  }
  status = OhBulkIntAllocPool(&dev->IntEPPool, HC_DEVICE_INTERRUPT_ENDPOINTS);

  if (status) { // On error
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: OHC_CreateController: OhBulkIntAllocPool!"));
    goto alloc_err;
  }
  status = OhIntDummyEpAllocPool(&dev->DummyIntEPPool);
  if (status) { // on error
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: OHC_CreateController: OhDummyIntEpAlloc!"));
    goto alloc_err;
  }
  status = OhTdAlloc(&dev->GTDPool, OH_TOTAL_GTD);
  if (status) { // On error
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: OHC_CreateController: OhTdAlloc!"));
    goto alloc_err;
  }
  dev->OhHcca = dev->Hcca->ItemHeader.VirtAddr; // Set an additional virtual pointer to struct OHCI_HCCA
  alloc_err:
  if (status) { // On error
    OhdDeleteDevice(dev);
    dev = NULL;
  }
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: ]OHC_CreateController!"));
  return (USBH_HC_HANDLE)dev;
}


/*********************************************************************
*
*       USBH_OHC_DeleteController
*
*  Function description
*    Deletes a host controller in the memory. The driver does never
*    delete the host controller. If the host controller is added to
*    the USB bus driver this function has to be called after removing
*    the host with OHC_RemoveController().
*    This may happen in the REMOVE_HC_COMPLETION routine or at a later time.
*
*    The completion is called after all requests has been (what?) and
*    the USB driver can be removed.
*/
void USBH_OHC_DeleteController(USBH_HC_HANDLE HcHandle) {
  HC_DEVICE * dev;
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: OHC_DeleteController!"));
  if (NULL == HcHandle) {
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: OHC_DeleteController: invalid HcHandle!"));
    return;
  }
  OH_DEV_FROM_HANDLE(dev, HcHandle);
  OH_DEV_VALID      (dev);
  OhdDeleteDevice   (dev);
}

/*********************************************************************
*
*       USBH_OHC_AddController
*
*  Function description
*    Attach of an OHCI controller to the USB bus driver.
*    After attaching the host controller initializations routine is called.
*/
USBH_STATUS USBH_OHC_AddController(USBH_HC_HANDLE HcHandle, USBH_HC_BD_HANDLE * HcBdHandle) {
  USB_HOST_ENTRY   hostEntry;
  USBH_STATUS      status = USBH_STATUS_SUCCESS;
  HC_DEVICE      * dev;
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: [OHC_AddController!"));
  T_ASSERT               (HcHandle != NULL);
  OH_DEV_FROM_HANDLE(dev, HcHandle);
  OH_DEV_VALID      (dev);
  hostEntry.AbortEndpoint    = Ohd_AbortEndpoint;
  hostEntry.AddEndpoint      = Ohd_AddEndpoint;
  hostEntry.DisablePort      = OhRh_DisablePort;
  hostEntry.GetFrameNumber   = Ohd_GetFrameNumber;
  hostEntry.GetHubStatus     = OhRh_GetHubStatus;
  hostEntry.GetPortCount     = OhRh_GetPortCount;
  hostEntry.GetPortStatus    = OhRh_GetPortStatus;
  hostEntry.ClearPortStatus  = OhRh_ClearPortStatus;
  hostEntry.ClearHubStatus   = OhRh_ClearHubStatus;
  hostEntry.GetPowerGoodTime = OhRh_GetPowerGoodTime;
  hostEntry.HcHandle         = dev;
  hostEntry.HostInit         = OHC_HostInit;
  hostEntry.HostExit         = OHC_HostExit;
  hostEntry.ReleaseEndpoint  = Ohd_ReleaseEndpoint;
  hostEntry.ResetEndpoint    = Ohd_ResetEndpoint;
  hostEntry.ResetPort        = OhRh_ResetPort;
  hostEntry.SetHcState       = Ohd_SetHcState;
  hostEntry.SetPortPower     = OhRh_SetPortPower;
  hostEntry.SetPortSuspend   = OhRh_SetPortSuspend;
  hostEntry.SubmitRequest    = OhT_SubmitRequest;
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: OHC_AddController: USBH_AddHostController!"));
  dev->ubdHandle             = USBH_AddHostController(&hostEntry);
  * HcBdHandle               = dev->ubdHandle;
  if (dev->ubdHandle == NULL) { // The OHCI controller can be attached to the USB Bus driver
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: OHC_AddController: BD_AddHostController!"));
    status = USBH_STATUS_ERROR;
  }
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: ]OHC_AddController!"));
  return status;
}

/*********************************************************************
*
*       USBH_OHC_RemoveController
*
*  Function description
*    Removes an OHCI controller from the USB bus driver. The OHCI
*    controller remains in memory until it is deleted with
*    OHC_DeleteController. If REMOVE_HC_COMPLETION is called then the
*    host controller is removed from the USB bus driver.
*/
USBH_STATUS USBH_OHC_RemoveController(USBH_HC_HANDLE HcHandle, REMOVE_HC_COMPLETION * Completion, void * Context) {
  HC_DEVICE   * dev;
  USBH_STATUS   status = USBH_STATUS_SUCCESS;
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: [OHC_RemoveController!"));
  if (NULL == HcHandle) {
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: OHC_RemoveController: nothing to do! HcHandle is NULL!"));
    status = USBH_STATUS_ERROR;
    goto exit;
  }
  OH_DEV_FROM_HANDLE(dev, HcHandle);
  OH_DEV_VALID      (dev);
  if (NULL == dev->ubdHandle) {
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: OHC_RemoveController: nothing to do: bus driver is not attached!"));
    status = USBH_STATUS_ERROR;
    goto exit;
  }
  // Wait until the bus driver has called the completion routine
  dev->RemoveCompletion = Completion;
  dev->RemoveContext    = Context;
  USBH_RemoveHostController(dev->ubdHandle);
  exit:
  ;
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: ]OHC_RemoveController!"));
  return status;
}

/*********************************************************************
*
*       OHC_HostInit
*
*  Function description
*    Setup all host controller registers and enables the OHCI interrupt.
*    If the function returns the OHCI controller is always in state RESET.
*    Then it set the root hub notification and the context. Before this
*    function is called the interrupt service routine.
*/
USBH_STATUS OHC_HostInit(USBH_HC_HANDLE HcHandle, USBH_RootHubNotification UbdRootHubNotification, void * RootHubNotificationContext) {
  USBH_STATUS   status;
  HC_DEVICE   * dev;
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: [OHC_HostInit!"));
  T_ASSERT(HcHandle != NULL);
  OH_DEV_FROM_HANDLE(dev, HcHandle);
  OH_DEV_VALID      (dev);
  dev->RootHub.UbdRootHubNotification     = UbdRootHubNotification;
  dev->RootHub.RootHubNotificationContext = RootHubNotificationContext;
  status = OhdCheckRevision(dev);  // Check the host revision register
  if (status) {
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: OHC_HostInit: OhdCheckRevision!"));
    goto exit;
  }
  // Reset the OHCI controller, set the operational state to Reset
  status = OhdSoftReset(dev);
  if (status) {
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: OHC_HostInit: OhdSoftReset!"));
    goto exit;
  }
  // Initialize the root hub descriptors the port power is off!
  status = OhRhInit(dev, UbdRootHubNotification, RootHubNotificationContext);
  if (status) {
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: OHC_HostInit: OhRhInit!"));
    goto exit;
  }
  status = OhdSetupOHCI(dev); // Setup other register
  if (status) {
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: OHC_HostInit: OhdSetupOHCI!"));
    goto exit;
  }
  // Do not use all list
  //OhdEndpointListEnable(dev,USB_EP_TYPE_CONTROL,FALSE,FALSE);
  //OhdEndpointListEnable(dev,USB_EP_TYPE_INT,FALSE,FALSE);
  exit:
  ;
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: ]OHC_HostInit!"));
  if (status) {
    // On error remove added endpoints and disable all endpoint lists. The OHCI device is deleted in
    // USBH_AddHostController at later if OHC_CreateController returns this status.
    OhdUnlinkHostResources(dev);
  }
  return status;
}

/*********************************************************************
*
*       OHC_HostExit
*
*  Function description
*    Unlinks all resources from the host controller. If the remove completion
*    is set then the RemoveCompletion routine is called with an user context!
*    Is called if the USB bus driver has removed the host object.
*/
USBH_STATUS OHC_HostExit(USBH_HC_HANDLE HcHandle) {
  HC_DEVICE            * dev;
  REMOVE_HC_COMPLETION * removeCompletion;
  void                 * removeContext;
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: [OHC_HostExit!"));
  OH_DEV_FROM_HANDLE    (dev, HcHandle);
  OH_DEV_VALID          (dev);
  OhdUnlinkHostResources(dev); // Unlink resources from the host
  removeCompletion = dev->RemoveCompletion;
  removeContext    = dev->RemoveContext;
  if (NULL != removeCompletion) {
    removeCompletion(removeContext);
  }
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: ]OHC_HostExit!"));
  return USBH_STATUS_SUCCESS;
}

/*********************************************************************
*
*       Ohd_AddEndpoint
*
*  Function description
*    Returns an endpoint handle for the added endpoint
*/
USBH_HC_EP_HANDLE Ohd_AddEndpoint(USBH_HC_HANDLE HcHandle, U8 EndpointType, U8 DeviceAddress, U8 EndpointAddress, U16 MaxFifoSize, U16 IntervalTime, USBH_SPEED Speed) {
  USBH_HC_EP_HANDLE   handle;
  HC_DEVICE         * dev;
  OHD_EP0           * ep0;
  OHD_BULK_INT_EP   * bulkIntEp;
  USBH_STATUS         status;
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: Ohd_AddEndpoint: Dev.Addr: %d, EpAddr: 0x%x max.Fifo size: %d!",
           (int)DeviceAddress, (int)EndpointAddress, (int)MaxFifoSize));
  handle            = NULL;
  OH_DEV_FROM_HANDLE(dev,HcHandle);
  OH_DEV_VALID      (dev);
  switch (EndpointType) {
  case USB_EP_TYPE_CONTROL:
    USBH_LOG((USBH_MTYPE_OHCI, "OHCI: Add Control Ep"));
    ep0=OhEp0Get(&dev->ControlEPPool, &dev->SetupPacketPool);
    if ( ep0 == NULL ) {
     USBH_WARN((USBH_MTYPE_OHCI, "OHCI: Ohd_AddEndpoint: OhEp0Get!"));
     break;
    }
    status=OhEp0Init(ep0, dev, 0, DeviceAddress, EndpointAddress, MaxFifoSize, Speed);
    if (status) { // On error
      USBH_WARN((USBH_MTYPE_OHCI, "OHCI: Ohd_AddEndpoint: OhEp0Init!"));
      OhEp0Put(ep0); // Put the endpoint back to the pool
      break;
    }
    OhEp0Insert(ep0);
    USBH_LOG((USBH_MTYPE_OHCI, "OHCI: Control Ep successful added! "));
    handle = ep0;
    break;
  case USB_EP_TYPE_BULK:
    USBH_LOG((USBH_MTYPE_OHCI, "OHCI: Add Bulk Ep"));
    bulkIntEp=OhBulkIntEpGet(&dev->BulkEPPool);
    if (bulkIntEp == NULL) {
     USBH_WARN((USBH_MTYPE_OHCI, "OHCI: Ohd_AddEndpoint: OhBulkIntEpGet!"));
     break;
    }
    bulkIntEp=OhBulkIntInitEp(bulkIntEp, dev, EndpointType, DeviceAddress, EndpointAddress, MaxFifoSize, IntervalTime, Speed, 0); // Init the bulk endpoint
    if (bulkIntEp == NULL) {
      USBH_WARN((USBH_MTYPE_OHCI, "OHCI: Ohd_AddEndpoint: OhBulkIntInitEp!"));
      break;
    }
    OhBulkInsertEndpoint(bulkIntEp); // Add the Ed to the HC list
    handle = bulkIntEp;
    break;
  case USB_EP_TYPE_INT:
    USBH_LOG((USBH_MTYPE_OHCI, "OHCI: Add Int Ep"));
    bulkIntEp=OhBulkIntEpGet(&dev->IntEPPool);       // Get the interrupt endpoint always from another pool
    if (bulkIntEp == NULL) {
     USBH_WARN((USBH_MTYPE_OHCI, "OHCI: Ohd_AddEndpoint: OhBulkIntEpGet!"));
     break;
    }
    bulkIntEp=OhBulkIntInitEp(bulkIntEp, dev, EndpointType, DeviceAddress, EndpointAddress, MaxFifoSize, IntervalTime, Speed, 0); // Init the interrupt endpoint
    if (bulkIntEp==NULL) {
      USBH_WARN((USBH_MTYPE_OHCI, "OHCI: Ohd_AddEndpoint: OhBulkIntInitEp!"));
      break;
    }
    status=OhIntInsertEndpoint(bulkIntEp); // Add the interrupt endpoint on the correct place
    if ( status ) {
      USBH_WARN((USBH_MTYPE_OHCI, "OHCI: Ohd_AddEndpoint: OhIntInsertEndpoint!"));
      OhBulkIntEpPut(bulkIntEp);// Release the endpoint with the dummy TD
      break;
    }
    handle = bulkIntEp;
    break;

#if OH_ISO_ENABLE

  case USB_EP_TYPE_ISO:
    USBH_LOG((USBH_MTYPE_OHCI, "OHCI: Add Iso Ep")v;
    break;

#endif

  default:
    USBH_LOG((USBH_MTYPE_OHCI, "OHCI: Ohd_AddEndpoint: invalid endpoint type: %u!",EndpointType));
    break;
  }
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: ]Ohd_AddEndpoint!"));
  return handle;
}

/*********************************************************************
*
*       Ohd_ReleaseEndpoint
*
*  Function description
*/
void Ohd_ReleaseEndpoint(USBH_HC_EP_HANDLE EpHandle, USBH_RELEASE_EP_COMPLETION_FUNC pfReleaseEpCompletion, void* pContext){
  OHD_EP0         * ep0;
  OHD_BULK_INT_EP * ep;
  // The struct elements until Type are the same for all endpoint types
  if (NULL == EpHandle) {
    USBH_LOG((USBH_MTYPE_OHCI, "OHCI: Ohd_ReleaseEndpoint: invalid EpHandle!"));
    return;
  }
  ep0=(OHD_EP0 *)EpHandle;
  switch (ep0->EndpointType) {
  // The element EndpointType has in all endpoint structs the same offset in the struct
  case USB_EP_TYPE_CONTROL:
    OhEp0_ReleaseEndpoint(ep0, pfReleaseEpCompletion, pContext);
    break;
  case USB_EP_TYPE_BULK:
    ep = (OHD_BULK_INT_EP *)EpHandle;
    OhBulk_ReleaseEndpoint(ep, pfReleaseEpCompletion, pContext);
    break;
  case USB_EP_TYPE_INT:
    ep = (OHD_BULK_INT_EP *)EpHandle;
    OhInt_ReleaseEndpoint (ep, pfReleaseEpCompletion, pContext);
    break;
  case USB_EP_TYPE_ISO:
    break;
  default:;
    USBH_LOG((USBH_MTYPE_OHCI, "OHCI: Ohd_ReleaseEndpoint: invalid endpoint type:%u!",ep0->EndpointType));
  }
}

/*********************************************************************
*
*       Ohd_SetHcState
*
*  Function description
*    Set the state of the HC
*/
USBH_STATUS Ohd_SetHcState(USBH_HC_HANDLE HcHandle, USBH_HostState HostState) {
  HC_DEVICE   * dev;
  USBH_STATUS   status;
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: Ohd_SetHcState HostState:%d!",HostState));
  OH_DEV_FROM_HANDLE(dev,HcHandle);
  OH_DEV_VALID      (dev);
  status      = USBH_STATUS_SUCCESS;

  switch (HostState) {
  case USBH_HOST_RESET:
    status = OhdSetHcFuncState(dev, HC_USB_RESET);
    // Wait for 10ms if switch to USBH_HOST_RUNNING
    break;
  case USBH_HOST_RUNNING:
    status = OhdSetHcFuncState(dev, HC_USB_OPERATIONAL);
    break;
  case USBH_HOST_SUSPEND:
    status = OhdSetHcFuncState(dev, HC_USB_SUSPEND);
    break;
  default:
    status = USBH_STATUS_ERROR;
  }
#if (USBH_DEBUG > 1)
  if (status) {
   USBH_WARN((USBH_MTYPE_OHCI, "OHCI: Ohd_SetHcState!"));
  }
#endif
  return status;
}

/*********************************************************************
*
*       OhdUpdateUpperFrameCounter
*
*  Function description
*    Must be called once every 64 seconds. This is done by calling this
*    routine in the interrupt if an frame overflow happens! This occurs every 32 ms!
*/
void OhdUpdateUpperFrameCounter(HC_DEVICE * dev) {
  U16     frame;
  frame = dev->OhHcca->FrameNumber;
  if (frame < dev->LastFrameCounter) {
    dev->UpperFrameCounter++;
  }
  dev->LastFrameCounter=frame;
}

/*********************************************************************
*
*       Ohd_GetFrameNumber
*
*  Function description
*    This code accounts for the fact that HccaFrameNumber is updated by
*    the HC before the HCD gets an interrupt that will adjust FrameHighPart.
*    If the HC frame counter is run to zero and OhdFrameOverflowInterrupt
*    is not called then the returned 32 bit frame number is correct!
*    No SOF interrupt is needed!
*/
U32 Ohd_GetFrameNumber(USBH_HC_HANDLE HcHandle) {
  U32         v;
  HC_DEVICE * dev;
  OH_DEV_FROM_HANDLE(dev,HcHandle);
  OH_DEV_VALID      (dev);
  USBH_LOG((USBH_MTYPE_OHCI, "OHCI: Ohd_GetFrameNumber!"));
  OhdUpdateUpperFrameCounter(dev);
  v  = dev->OhHcca->FrameNumber;
  v |= (U32)dev->UpperFrameCounter << 16;
  return v;
}

/*********************************************************************
*
*       OhdEndpointListEnable
*
*  Function description
*    Sets the endpoint register to enable or disable a list. The current
*    ED pointer register is set to zero and the list filled bit is set
*    if the list has an disable->enable transition. If the parameter
*    ListFill is set always the list filled bit is rewritten.
*/
void OhdEndpointListEnable(HC_DEVICE * Dev, U8 EpType, T_BOOL Enable, T_BOOL Rescan) {
  U32   val;
  // USBH_WARN((USBH_MTYPE_OHCI, "OHCI: INFO OhdEndpointListEnable: EpType: %d: enable: %d rescan: %d!", EpType,Enable,Rescan));
  val = OhHalReadReg(Dev->RegBase,OH_REG_CONTROL);
  switch (EpType) {
  case USB_EP_TYPE_CONTROL:
    if (Enable && !(val & HC_CONTROL_CLE)) { // On restart the list
      OhHalWriteReg(Dev->RegBase, OH_REG_CONTROLCURRENTED, 0);
    }
    if (Rescan) {
      OhHalWriteReg(Dev->RegBase, OH_REG_COMMANDSTATUS, HC_COMMAND_STATUS_CLF);
    }
    if (Enable) {
      OhHalWriteReg(Dev->RegBase, OH_REG_CONTROL, val | HC_CONTROL_CLE);
    } else {
      val &= ~HC_CONTROL_CLE;
      OhHalWriteReg(Dev->RegBase, OH_REG_CONTROL, val);
    }
    break;
  case USB_EP_TYPE_BULK:
    if (Enable && !(val & HC_CONTROL_BLE)) {
      OhHalWriteReg(Dev->RegBase, OH_REG_BULKCURRENTED, 0);
    }
    if (Enable) {
      OhHalWriteReg(Dev->RegBase, OH_REG_CONTROL, val | HC_CONTROL_BLE);
    } else {
      val &= ~HC_CONTROL_BLE;
      OhHalWriteReg(Dev->RegBase, OH_REG_CONTROL, val);
    }
    if (Rescan) {
      OhHalWriteReg(Dev->RegBase, OH_REG_COMMANDSTATUS, HC_COMMAND_STATUS_BLF);
    }
    break;
  case USB_EP_TYPE_INT:
    if (Enable && !(val & HC_CONTROL_PLE)) {
      OhHalWriteReg(Dev->RegBase, OH_REG_CONTROL,val | HC_CONTROL_PLE);
    }
    if (!Enable && (val & HC_CONTROL_PLE)) {
      val &=~HC_CONTROL_PLE;
      OhHalWriteReg(Dev->RegBase, OH_REG_CONTROL,val);
    }
    break;
  case USB_EP_TYPE_ISO:
    if (Enable && (!(val & HC_CONTROL_PLE ) || !(val & HC_CONTROL_IE))) { // On isochronous the periodic list must be also enabled
      OhHalWriteReg(Dev->RegBase, OH_REG_CONTROL, val | HC_CONTROL_PLE | HC_CONTROL_IE);
    } else {
      if ( !Enable && ( val & HC_CONTROL_IE) )  {
        val &=~HC_CONTROL_IE;
        OhHalWriteReg(Dev->RegBase,OH_REG_CONTROL,val);
      }
    }
  default:
    break;
  }
}

#if OH_ISO_ENABLE

/*********************************************************************
*
*       OhIsoAlloc
*
*  Function description
*/
USBH_STATUS OhIsoAlloc(HC_DEVICE * Dev, HCM_POOL * EpPool, HCM_POOL * TdPool, U32 MaxIsoTds) {
  USBH_STATUS   status;
  OH_DEV_VALID(Dev);
  status      = HcmAllocPool(EpPool, OH_DEV_MAX_ISO_EP, OH_ED_SIZE, sizeof(OHD_ISO_EP), OH_ED_ALIGNMENT);
  if (status) { // On error
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: OhIsoAlloc: HcmAllocPool: Eps!"));
    goto exit;
  }
  status      = HcmAllocPool(TdPool, OH_DEV_MAX_ISO_TD, OH_ISO_TD_SIZE, sizeof(OHD_GTD), OH_ISO_TD_ALIGNMENT);
  if (status) { // On error
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: OhIsoAlloc: HcmAllocPool:TDs!"));
    goto exit;
  }
  exit:
  return status;
}

void OhIsoEpExit(struct T_HC_DEVICE * Dev, HCM_POOL * EpPool) {
  HcmFreePool(EpPool);
}

#endif /*OH_ISO_ENABLE*/

/*********************************************************************
*
*       USBH_OHC_ServiceISR
*
*  Function description
*    Disables the interrupt mask for the HC. It returns TRUE if the interrupt
*    was caused by the device. This function depends from the used USB host controller.
*/
U8 USBH_OHC_ServiceISR(USBH_HC_HANDLE HcHandle) {
  HC_DEVICE * dev;
  U32         intStatus, intEnable;
  U8          ret;
  OH_DEV_FROM_HANDLE(dev, HcHandle);
  OH_DEV_VALID(dev);
  intEnable = OhHalReadReg(dev->RegBase, OH_REG_INTERRUPTENABLE);
  if (0 != (intEnable &HC_INT_MIE)) {                                   // Master interrupt enable bit is on
    intStatus = OhHalReadReg(dev->RegBase, OH_REG_INTERRUPTSTATUS);
    // All interrupt status bits are checked, Bit 31 (HC_INT_STATUS_VALIDATION_BIT) is always zero
    if (0 != (intStatus &(OH_ENABLED_INTERRUPTS)) && 0 == (intStatus &HC_INT_STATUS_VALIDATION_BIT)) {
      OhHalWriteReg(dev->RegBase, OH_REG_INTERRUPTDISABLE, HC_INT_MIE); // disable the interrupt
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
*       USBH_OHC_ProcessInterrupt
*
*  Function description
*    Process of interrupt requests that are needed from the driver.
*    The calling task must be able to wait.
*/
void USBH_OHC_ProcessInterrupt(USBH_HC_HANDLE HcHandle) {
  HC_DEVICE * dev;
  U32         intStatus;
  OH_DEV_FROM_HANDLE(dev, HcHandle);
  OH_DEV_VALID(dev);
  intStatus = OhHalReadReg(dev->RegBase, OH_REG_INTERRUPTSTATUS);
  if (intStatus & HC_INT_SO) {
    USBH_LOG((USBH_MTYPE_OHCI, "OHCI: PI: Sheduling overrun not handled!"));
  }
  if (intStatus & HC_INT_WDH) { // WD bit is true, read the DoneHead Register from HCCA
    USBH_LOG((USBH_MTYPE_OHCI, "OHCI: [PI: HC_INT_WDH!"));
    OhTProcessDoneInterrupt(dev);
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
    OhdUpdateUpperFrameCounter(dev);
  }
  if (intStatus & HC_INT_RHSC) { // Root hub status change
    USBH_LOG((USBH_MTYPE_OHCI, "OHCI: PI: HC_INT_RHSC!"));
    OhRhProcessInterrupt(&dev->RootHub);
  }
  if (intStatus & HC_INT_OC) {
    USBH_WARN((USBH_MTYPE_OHCI, "OHCI: PI: OwnershipChange is not handled !"));
  }
  OhHalWriteReg(dev->RegBase, OH_REG_INTERRUPTSTATUS, intStatus);  // Clear interrupt status bits and enable the interrupt
  OhHalWriteReg(dev->RegBase, OH_REG_INTERRUPTENABLE, HC_INT_MIE); // Now the ISR detects the next interrupt
}

/*********************************************************************
*
*       USBH_ServiceISR
*
*  Function description
*/
void USBH_ServiceISR(unsigned Index) {
  U8 succ;
  USBH_USE_PARA(Index);
  succ = USBH_OHC_ServiceISR(USBH_Global.hHC);
  if( succ ){
    USBH_OnISREvent();
  }
}

/*********************************************************************
*
*       USBH_OHC_Add
*
*  Function description:
*/
void USBH_OHC_Add(void * pBase) {
  USBH_Global.hHC = USBH_OHC_CreateController(pBase);
  USBH_OHC_AddController(USBH_Global.hHC, &USBH_Global.hHCBD);
}

/********************************* EOF*******************************/
