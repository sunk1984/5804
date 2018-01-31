/*********************************************************************
*  
*   IAR PowerPac
*
*   (c) Copyright IAR Systems 2010.  All rights reserved.
*
**********************************************************************
----------------------------------------------------------------------
File        : USBH_PrinterClass.c
Purpose     : API of the USB host stack
---------------------------END-OF-HEADER------------------------------
*/

#include "USBH_Int.h"

/*********************************************************************
*
*       Defines configurable
*
**********************************************************************
*/


#define MAX_TRANSFERS_ERRORS 3 // If the device returns more than 3 errors the application stops reading from the device and waits for removing the device
#define NUM_DEVICES          2 // NOTE: Limited by the number of bits in DevIndexUsedMask which by now is 32

#ifndef    USBH_PRINTER_DEFAULT_TIMEOUT
  #define  USBH_PRINTER_DEFAULT_TIMEOUT      5000
#endif

/*********************************************************************
*
*       Define non-configurable
*
**********************************************************************
*/
typedef enum _USBH_PRINTER_STATE {
  StateIdle,  // Initial state, set if UsbhHid_InitDemoApp is called
  StateStop,  // Device is removed or an user break
  StateError, // Application error
  StateInit,  // Set during device initialization
  StateRunning
} USBH_PRINTER_STATE;


typedef struct USBH_PRINTER_INST USBH_PRINTER_INST;

typedef struct EP_DATA {
  U8   EPAddr;
  U16  MaxPacketSize;
  USBH_URB  Urb;
  USBH_OS_EVENT_OBJ * pEvent;
  unsigned            RefCount;
} EP_DATA;

struct USBH_PRINTER_INST {
  union {
    USBH_DLIST_ITEM             Link;
    struct USBH_PRINTER_INST *  pInst;
  } Next;
  USBH_PRINTER_STATE            RunningState;
  U8                            IsUsed;
  USBH_INTERFACE_ID             InterfaceID;
  U8                            DevInterfaceID;
  USBH_INTERFACE_HANDLE         hInterface;
  USBH_URB                           AbortUrb;
  EP_DATA                       Control;
  EP_DATA                       CmdIn;
  EP_DATA                       CmdOut;
  U8                            IsOpened;
  USBH_OS_EVENT_OBJ           * pAbortTransactionEvent;
  USBH_PRINTER_HANDLE           Handle;
  char                          acName[8];
  U8                            DevIndex;
};

typedef struct {
  union {
    USBH_PRINTER_INST       * pFirst;
    USBH_DLIST_HEAD           Head;
  } List;
  U8                          NumDevices;
  USBH_NOTIFICATION_HANDLE    hDevNotification;
  int                         NextHandle;
  USBH_NOTIFICATION_FUNC    * pfOnUserNotification;
  void                      * pfUserNotificationContext;
  U32                         DefaultTimeOut;
  U32                         DevIndexUsedMask;
} USBH_PRINTER_GLOBAL;

/*********************************************************************
*
*       Static data
*
**********************************************************************
*/
static USBH_PRINTER_GLOBAL           USBH_PRINTER_Global;

/*********************************************************************
*
*       Static prototypes
*
**********************************************************************
*/

/*********************************************************************
*
*       Static code
*
**********************************************************************
*/

/*********************************************************************
*
*       _AllocateDevIndex()
*
*   Function description
*     Searches for an available device index which is the index
*     of the first cleared bit in the DevIndexUsedMask.
*
*   Return value
*     A device index or NUM_DEVICES in case all device indexes are allocated.
*/
static U8 _AlocateDevIndex(void) {
  U8 i;
  U32 Mask;

  Mask = 1;
  for (i = 0; i < NUM_DEVICES; ++i) {
    if (!(USBH_PRINTER_Global.DevIndexUsedMask & Mask)) {
      USBH_PRINTER_Global.DevIndexUsedMask |= Mask;
      break;
    }
    Mask <<= 1;
  }
  return i;
}

/*********************************************************************
*
*       _FreeDevIndex()
*
*   Function description
*     Marks a device index as free by clearing the corresponding bit
*     in the DevIndexUsedMask.
*
*   Parameter
*     DevIndex
*/
static void _FreeDevIndex(U8 DevIndex) {
  U32 Mask;

  if (DevIndex < NUM_DEVICES) {
    Mask = 1 << DevIndex;
    USBH_PRINTER_Global.DevIndexUsedMask &= ~Mask;
  }
}

/*********************************************************************
*
*       _h2p()
*/
static USBH_PRINTER_INST * _h2p(USBH_PRINTER_HANDLE Handle) {
  USBH_PRINTER_INST * pInst;

  if (Handle == 0) {
    return NULL;
  }

  //
  // Check if the first instance is a match (which is so in most cases)
  //
  pInst = (USBH_PRINTER_INST *)(USBH_PRINTER_Global.List.pFirst);                // First instance
  if (pInst == NULL) {
    USBH_WARN((USBH_MTYPE_PRINTER_CLASS, "Instance list is empty"));
    return NULL;
  }
  if (pInst->Handle == Handle) {                                        // Match ?
    return pInst;
  }
  //
  // Iterate over linked list to find a socket with matching handle. Return if found.
  //
  do {
    pInst = (USBH_PRINTER_INST *)pInst->Next.Link.pNext;
    if (pInst == NULL) {
      break;
    }
    if (pInst->Handle == Handle) {                                        // Match ?
      //
      // If it is not the first in the list, make it the first
      //
      if (pInst != (USBH_PRINTER_INST *)(USBH_PRINTER_Global.List.Head.pFirst)) {     // First socket ?
        USBH_DLIST_Remove(&USBH_PRINTER_Global.List.Head, &pInst->Next.Link);
        USBH_DLIST_Add(&USBH_PRINTER_Global.List.Head, &pInst->Next.Link);
      }
      return pInst;
    }
  } while(1);

  //
  // Error handling: Socket handle not found in list.
  //
  USBH_WARN((USBH_MTYPE_PRINTER_CLASS, "HANDLE: handle %d not in instance list", Handle));
  return NULL;
}

/*********************************************************************
*
*       _RemoveDevInstance
*
*/
static void _RemoveDevInstance(USBH_PRINTER_INST * pInst) {
  if (pInst) {
    //
    //  Free all associated EP buffers
    //
//    if (pInst->pInBuffer) {
//      USBH_Free(pInst->pInBuffer);
//    }
//    if (pInst->pOutBuffer) {
//      USBH_Free(pInst->pOutBuffer);
//    }
    //
    // Remove instance from list
    //
    USBH_DLIST_Remove(&USBH_PRINTER_Global.List.Head, (USBH_DLIST_ITEM *)pInst);
    //
    // Free the memory that is used by the instance
    //
    USBH_Free(pInst);
    pInst = (USBH_PRINTER_INST *)NULL;
  }
}


/*********************************************************************
*
*       _CheckStateAndCloseInterface
*
*  Function description:
*    Close the handle interface when it is not referenced from any application.
*
*  Return value:
*    TRUE   - Handle is closed
*    FALSE  - Error
*/
static USBH_BOOL _CheckStateAndCloseInterface(USBH_PRINTER_INST * pInst) {
  USBH_BOOL r = FALSE;

  if (pInst->RunningState == StateStop || pInst->RunningState == StateError) {
    if (pInst->hInterface != NULL) {
      USBH_CloseInterface(pInst->hInterface);
      _FreeDevIndex(pInst->DevIndex);
      USBH_PRINTER_Global.NumDevices--;
      pInst->hInterface = NULL;
      pInst->IsUsed     = 0;
      _RemoveDevInstance(pInst);
    }
  }
  if (NULL == pInst->hInterface) {
    r = TRUE;
  }
  return r;
}


/*********************************************************************
*
*       _RemoveAllInstances
*
*/
static USBH_BOOL _RemoveAllInstances(void) {
  USBH_PRINTER_INST * pInst;
  USBH_BOOL       r;

  r = FALSE;
  for (pInst = (USBH_PRINTER_INST *)(USBH_PRINTER_Global.List.Head.pFirst); pInst; pInst = (USBH_PRINTER_INST *)pInst->Next.Link.pNext) {   // Iterate over all instances
     r |= _CheckStateAndCloseInterface(pInst);
  }
  return r;
}

/*********************************************************************
*
*       _CreateDevInstance
*
*/
static USBH_PRINTER_INST * _CreateDevInstance(void) {
  USBH_PRINTER_INST * pInst;
  int i;

  //
  // Check if max. number of devices allowed is exceeded
  //
  if ((USBH_PRINTER_Global.NumDevices + 1) >= NUM_DEVICES) {
    USBH_PANIC("No instance available for creating a new printer device");
  }
  //
  // Use next available device handle.
  // Valid handles are positive integers; handles are assigned in increasing order from 1.
  // When the handle reaches a certain limit, we restart at 1.
  // Wrap around if necessary and make sure device handle is not yet in use
  //
  i = USBH_PRINTER_Global.NextHandle + 1;
SearchDuplicate:
  if (i >= 0xFFFF) {
    i = 1;
  }
  for (pInst = (struct USBH_PRINTER_INST *)(USBH_PRINTER_Global.List.Head.pFirst); pInst; pInst = (struct USBH_PRINTER_INST *)pInst->Next.Link.pNext) {   // Iterate over all instances
    if (i == pInst->Handle) {
      i++;
      goto SearchDuplicate;
    }
  }
  USBH_PRINTER_Global.NextHandle = i;
  //
  // We found a valid new handle!
  // Perform the actual allocation
  //
  pInst = (USBH_PRINTER_INST *)USBH_MallocZeroed(sizeof(USBH_PRINTER_INST));
  if (pInst) {
    USBH_DLIST_Add(&USBH_PRINTER_Global.List.Head, (USBH_DLIST_ITEM *)pInst);
    pInst->Handle = i;
  }
  pInst->hInterface     = NULL;
  pInst->InterfaceID    = 0;
  pInst->IsUsed         = 1;
  pInst->DevIndex       = _AlocateDevIndex();
  sprintf(pInst->acName, "prt%.3d", pInst->DevIndex);
  USBH_PRINTER_Global.NumDevices++;
  return pInst;
}

/*********************************************************************
*
*       _OnSubmitUrbCompletion
*
*  Function description
*/
static void _OnSubmitUrbCompletion(USBH_URB * pUrb) {
  EP_DATA  * pEPData;

  pEPData       = (EP_DATA  *)pUrb->Header.pContext;
  USBH_LOG((USBH_MTYPE_PRINTER_CLASS, "Printer: _OnSubmitUrbCompletion USBH_URB st: %s",USBH_GetStatusStr(pUrb->Header.Status)));
  USBH_OS_SetEvent(pEPData->pEvent);
}

/*********************************************************************
*
*       _SubmitUrbAndWait
*
*  Function description
*    Submits an USBH_URB to the USB bus driver synchronous, it uses the
*    TAL event functions. On successful completion the USBH_URB Status is returned!
*/
static USBH_STATUS _SubmitUrbAndWait(USBH_PRINTER_INST * pInst, EP_DATA * pEPData, U32 Timeout) {
  USBH_STATUS Status;
  int         EventStatus;
  USBH_URB       * pUrb;


  USBH_ASSERT(NULL != pInst->hInterface);
  USBH_ASSERT_PTR(pEPData->pEvent);
  pUrb = &pEPData->Urb;
  USBH_LOG((USBH_MTYPE_PRINTER_CLASS, "Printer: _SubmitUrbAndWait"));
  pUrb->Header.pfOnCompletion = _OnSubmitUrbCompletion;
  pUrb->Header.pContext       = pEPData;
  USBH_OS_ResetEvent(pEPData->pEvent);
  pEPData->RefCount++;
  Status = USBH_SubmitUrb(pInst->hInterface, pUrb);
  if (Status != USBH_STATUS_PENDING) {
    USBH_LOG((USBH_MTYPE_PRINTER_CLASS, "Printer: _SubmitUrbAndWait: USBH_SubmitUrb st: 0x%08x", USBH_GetStatusStr(Status)));
    Status = USBH_STATUS_ERROR;
  } else {                                // Pending USBH_URB
    Status       = USBH_STATUS_SUCCESS;
    EventStatus = USBH_OS_WaitEventTimed(pEPData->pEvent, Timeout);
    pEPData->RefCount--;
    if (EventStatus != USBH_OS_EVENT_SIGNALED) {
      USBH_BOOL Abort    = TRUE;
      USBH_URB * pAbortUrb = &pInst->AbortUrb;

      USBH_LOG((USBH_MTYPE_MSD, "Printer: _SubmitUrbAndWait: Timeout Status: 0x%08x, now Abort the USBH_URB!",EventStatus));
      USBH_ZERO_MEMORY(pAbortUrb, sizeof(USBH_URB));
      switch (pUrb->Header.Function) {     // Not signaled Abort and wait infinite
      case USBH_FUNCTION_BULK_REQUEST:
      case USBH_FUNCTION_INT_REQUEST:
        pAbortUrb->Request.EndpointRequest.Endpoint = pUrb->Request.BulkIntRequest.Endpoint;
        break;
      case USBH_FUNCTION_CONTROL_REQUEST:
        pAbortUrb->Request.EndpointRequest.Endpoint = 0;
      default:
        Abort = FALSE;
        USBH_LOG((USBH_MTYPE_PRINTER_CLASS, "Printer: _SubmitUrbAndWait: invalid USBH_URB function: %d",pUrb->Header.Function));
        break;
      }
      if (Abort) {
        USBH_WARN((USBH_MTYPE_PRINTER_CLASS, "Printer: _SubmitUrbAndWait: Abort Ep: 0x%x", pUrb->Request.EndpointRequest.Endpoint));
        pAbortUrb->Header.Function = USBH_FUNCTION_ABORT_ENDPOINT;
        USBH_OS_ResetEvent(pEPData->pEvent);
        pAbortUrb->Header.pfOnCompletion = _OnSubmitUrbCompletion;
        pAbortUrb->Header.pContext       = pInst;
        pEPData->RefCount++;
        Status = USBH_SubmitUrb(pInst->hInterface, pAbortUrb);
        if (Status) {
          USBH_LOG((USBH_MTYPE_PRINTER_CLASS, "Printer: _SubmitUrbAndWait: USBH_FUNCTION_ABORT_ENDPOINT st: 0x%08x",Status));
        }
        USBH_OS_WaitEvent(pEPData->pEvent); // Wait infinite for completion
        pEPData->RefCount--;
      }
    }
    if (!Status) {
      Status = pUrb->Header.Status;       // USBH_URB completed return the pBuffer Status
      if (Status) {
        USBH_LOG((USBH_MTYPE_PRINTER_CLASS, "Printer: _SubmitUrbAndWait: USBH_URB Status: %s", USBH_GetStatusStr(Status)));
      }
    }
  }
  return Status;
}

/*********************************************************************
*
*       _StopDevice
*
*  Function description:
*    Is used to stop the HID class filter.
*    It is called if the user wants to stop the class filter.
*/
static void _StopDevice(USBH_PRINTER_INST * pInst) {
  USBH_STATUS  Status;
  unsigned     i;
  EP_DATA    * aEPData[3];

  
  aEPData[0] = &pInst->Control;
  aEPData[1] = &pInst->CmdIn;
  aEPData[2] = &pInst->CmdOut;
  if (StateStop == pInst->RunningState || StateError == pInst->RunningState) {
    USBH_LOG((USBH_MTYPE_PRINTER_CLASS, "Printer: _StopDevice: app. already stopped state: %d!", pInst->RunningState));
    return;
  }
  // Stops submitting of new URBs from the application
  pInst->RunningState = StateStop;
  if (NULL == pInst->hInterface) {
    USBH_LOG((USBH_MTYPE_PRINTER_CLASS, "Printer: _StopDevice: interface handle is null, nothing to do!"));
    return;
  }
  for (i = 0; i < USBH_COUNTOF(aEPData); i++) {
    if (aEPData[i]->RefCount) {
      USBH_URB * pAbortUrb = &pInst->AbortUrb;

      pAbortUrb->Header.Function = USBH_FUNCTION_ABORT_ENDPOINT;
      pAbortUrb->Request.EndpointRequest.Endpoint = aEPData[i]->EPAddr;
      USBH_OS_ResetEvent(aEPData[i]->pEvent);
      pAbortUrb->Header.pfOnCompletion = _OnSubmitUrbCompletion;
      pAbortUrb->Header.pContext       = pInst;
      Status = USBH_SubmitUrb(pInst->hInterface, pAbortUrb);
      if (Status) {
        USBH_LOG((USBH_MTYPE_PRINTER_CLASS, "Printer: _SubmitUrbAndWait: USBH_FUNCTION_ABORT_ENDPOINT st: 0x%08x",Status));
      }
      USBH_OS_WaitEvent(aEPData[i]->pEvent); // Wait infinite for completion
      aEPData[i]->RefCount = 0;

    }
  }
}

/*********************************************************************
*
*       _StartDevice
*
*  Function description:
*   Starts the application and is called if a USB device is connected.
*   The function uses the first interface of the device.
*
*  Parameters:
*    InterfaceID    -
*
*  Return value:
*    USBH_STATUS       -
*/
static USBH_STATUS _StartDevice(USBH_PRINTER_INST * pInst) {
  USBH_STATUS  Status;
  USBH_EP_MASK EPMask;
  unsigned int Length;
  U8           aEpDesc[USB_ENDPOINT_DESCRIPTOR_LENGTH];

  // Open the hid interface
  Status = USBH_OpenInterface(pInst->InterfaceID, TRUE, &pInst->hInterface);
  if (USBH_STATUS_SUCCESS != Status) {
    USBH_WARN((USBH_MTYPE_PRINTER_CLASS, "Printer: _StartDevice: USBH_OpenInterface failed 0x%08x!", Status));
    return Status;
  }
  pInst->Control.pEvent = USBH_OS_AllocEvent();
  //
  // Get first the EP in descriptor
  //
  USBH_MEMSET(&EPMask,  0, sizeof(USBH_EP_MASK));
  EPMask.Mask      = USBH_EP_MASK_TYPE | USBH_EP_MASK_DIRECTION;
  EPMask.Direction = USB_OUT_DIRECTION;
  EPMask.Type      = USB_EP_TYPE_BULK;
  Length           = sizeof(aEpDesc);
  Status           = USBH_GetEndpointDescriptor(pInst->hInterface, 0, &EPMask, aEpDesc, &Length);
  if (Status) {
    USBH_WARN((USBH_MTYPE_PRINTER_CLASS, "Printer: _StartDevice: USBH_GetEndpointDescriptor failed st: %08x", Status));
    goto Err;
  } else {
    pInst->CmdOut.MaxPacketSize = (int)(aEpDesc[USB_EP_DESC_PACKET_SIZE_OFS] + (aEpDesc[USB_EP_DESC_PACKET_SIZE_OFS + 1] << 8));
    pInst->CmdOut.EPAddr        = aEpDesc[USB_EP_DESC_ADDRESS_OFS];
    pInst->CmdOut.pEvent        = USBH_OS_AllocEvent();
    USBH_LOG((USBH_MTYPE_PRINTER_CLASS, "Address   MaxPacketSize"));
    USBH_LOG((USBH_MTYPE_PRINTER_CLASS, "0x%02X      %5d      ", pInst->CmdOut.EPAddr, pInst->CmdOut.MaxPacketSize));
  }
  //
  // Now try to get the EP Out descriptor
  //
  USBH_MEMSET(&EPMask,  0, sizeof(USBH_EP_MASK));
  EPMask.Mask      = USBH_EP_MASK_TYPE | USBH_EP_MASK_DIRECTION;
  EPMask.Direction = USB_IN_DIRECTION;
  EPMask.Type      = USB_EP_TYPE_BULK;
  Length           = sizeof(aEpDesc);
  Status           = USBH_GetEndpointDescriptor(pInst->hInterface, 0, &EPMask, aEpDesc, &Length);
  if (Status == USBH_STATUS_SUCCESS) {
    pInst->CmdIn.EPAddr        = aEpDesc[USB_EP_DESC_ADDRESS_OFS];
    pInst->CmdIn.MaxPacketSize = (int)(aEpDesc[USB_EP_DESC_PACKET_SIZE_OFS] + (aEpDesc[USB_EP_DESC_PACKET_SIZE_OFS + 1] << 8));
    pInst->CmdIn.pEvent        = USBH_OS_AllocEvent();
    USBH_LOG((USBH_MTYPE_PRINTER_CLASS, "Address   MaxPacketSize"));
    USBH_LOG((USBH_MTYPE_PRINTER_CLASS, "0x%02X      %5d      ", pInst->CmdIn.EPAddr, pInst->CmdIn.MaxPacketSize));
  }
  return USBH_STATUS_SUCCESS;
Err: // on error
  USBH_CloseInterface(pInst->hInterface);
  pInst->InterfaceID = 0;
  pInst->hInterface  = NULL;
  return USBH_STATUS_ERROR;
}

/*********************************************************************
*
*       _OnDevNotification
*
*  Function description:
*    TBD
*/
static void _OnDevNotification(USBH_PRINTER_INST * pInst, USBH_PNP_EVENT Event, USBH_INTERFACE_ID InterfaceID) {
  USBH_STATUS Status;
  switch (Event) {
    case USBH_ADD_DEVICE:
      USBH_LOG((USBH_MTYPE_PRINTER_CLASS, "_OnDeviceNotification: USB Printer device detected interface ID: %u !", InterfaceID));
      pInst->RunningState = StateInit;
      if (pInst->hInterface == NULL) {
        // Only one device is handled from the application at the same time
        pInst->InterfaceID = InterfaceID;
        Status             = _StartDevice(pInst);
        if (Status) { // On error
          pInst->RunningState = StateError;
        } else {
          pInst->RunningState = StateRunning;
        }
      }
      break;
    case USBH_REMOVE_DEVICE:
      if (pInst->hInterface == NULL || pInst->InterfaceID != InterfaceID) {
        // Only one device is handled from the application at the same time
        return;
      }
      USBH_LOG((USBH_MTYPE_PRINTER_CLASS, "_OnDeviceNotification: USB Printer device removed interface  ID: %u !", InterfaceID));
      _StopDevice(pInst);
      _CheckStateAndCloseInterface(pInst);
      break;
    default:
      USBH_WARN((USBH_MTYPE_PRINTER_CLASS, "_OnDeviceNotification: invalid Event: %d !", Event));
      break;
  }
}

/*********************************************************************
*
*       _OnDeviceNotification
*/
static void _OnDeviceNotification(void * pContext, USBH_PNP_EVENT Event, USBH_INTERFACE_ID InterfaceID) {
  USBH_PRINTER_INST * pInst;
  USBH_DEVICE_EVENT   DeviceEvent;

  USBH_USE_PARA(pContext);
  DeviceEvent = USBH_DEVICE_EVENT_ADD;
  if (Event == USBH_ADD_DEVICE) {
    pInst = _CreateDevInstance();
    DeviceEvent = USBH_DEVICE_EVENT_ADD;
  } else {
    for (pInst = (struct USBH_PRINTER_INST *)(USBH_PRINTER_Global.List.Head.pFirst); pInst; pInst = (struct USBH_PRINTER_INST *)pInst->Next.Link.pNext) {   // Iterate over all instances
      if (pInst->InterfaceID == InterfaceID) {
        DeviceEvent = USBH_DEVICE_EVENT_REMOVE;
        break;
      }
    }
  }
  _OnDevNotification(pInst, Event, InterfaceID);
  if (USBH_PRINTER_Global.pfOnUserNotification) {
    USBH_PRINTER_Global.pfOnUserNotification(USBH_PRINTER_Global.pfUserNotificationContext, pInst->DevIndex, DeviceEvent);
  }
}

#if 0
/*********************************************************************
*
*       _OnAbortCompletion
*/
static void _OnAbortCompletion(URB * pUrb) {
  USBH_PRINTER_INST * pInst;

  USBH_ASSERT(pUrb != NULL);
  pInst = (USBH_PRINTER_INST * )pUrb->Header.pContext;
  if (pInst->pAbortTransactionEvent) {
    USBH_OS_SetEvent(pInst->pAbortTransactionEvent);
  }
}

/*********************************************************************
*
*       _CancelIO
*/
static int _CancelIO(USBH_PRINTER_INST * pInst) {
  pInst->pAbortTransactionEvent = USBH_OS_AllocEvent();
  if (pInst->CmdIn.Urb.Header.Status != USBH_STATUS_SUCCESS) {
    pInst->AbortUrb.Header.Function                  = USBH_FUNCTION_ABORT_ENDPOINT;
    pInst->AbortUrb.Header.pfOnCompletion            = _OnAbortCompletion;
    pInst->AbortUrb.Header.pContext                  = pInst;
    pInst->AbortUrb.Request.EndpointRequest.Endpoint = pInst->CmdIn.EPAddr;
    USBH_SubmitUrb(pInst->hInterface, &pInst->AbortUrb);
    USBH_OS_WaitEvent(pInst->pAbortTransactionEvent);
  }
  if (pInst->CmdOut.Urb.Header.Status != USBH_STATUS_SUCCESS) {
    pInst->AbortUrb.Header.Function                  = USBH_FUNCTION_ABORT_ENDPOINT;
    pInst->AbortUrb.Header.pfOnCompletion            = _OnAbortCompletion;
    pInst->AbortUrb.Header.pContext                  = pInst;
    pInst->AbortUrb.Request.EndpointRequest.Endpoint = pInst->CmdOut.EPAddr;
    USBH_SubmitUrb(pInst->hInterface, &pInst->AbortUrb);
    USBH_OS_WaitEvent(pInst->pAbortTransactionEvent);
  }
  USBH_OS_FreeEvent(pInst->pAbortTransactionEvent);
  pInst->pAbortTransactionEvent = NULL;
  return 0;
}
#endif

/*********************************************************************
*
*       Public code
*
**********************************************************************
*/

/*********************************************************************
*
*       USBH_PRINTER_Init
*
*  Function description:
*    Initialize the Printer device class driver.
*
*  Return value:
*    TRUE   - Success
*    FALSE  - Could not register class device driver
*
*/
USBH_BOOL USBH_PRINTER_Init(void) {
  USBH_PNP_NOTIFICATION   PnpNotify;
  USBH_INTERFACE_MASK   * pInterfaceMask;

  USBH_PRINTER_Global.DefaultTimeOut = USBH_PRINTER_DEFAULT_TIMEOUT;
  // Add an plug an play notification routine
  pInterfaceMask            = &PnpNotify.InterfaceMask;
  pInterfaceMask->Mask      = USBH_INFO_MASK_CLASS | USBH_INFO_MASK_SUBCLASS;
  pInterfaceMask->Class     = USB_DEVICE_CLASS_PRINTER;
  pInterfaceMask->SubClass  = 0x01;                    // Printer
  PnpNotify.pContext         = NULL;
  PnpNotify.pfPnpNotification = _OnDeviceNotification;
  USBH_PRINTER_Global.hDevNotification = USBH_RegisterPnPNotification(&PnpNotify); // Register the  PNP notification
  if (NULL == USBH_PRINTER_Global.hDevNotification) {
    USBH_WARN((USBH_MTYPE_PRINTER_CLASS, "USBH_PRINTER_Init: USBH_RegisterPnPNotification"));
    return FALSE;
  }
  return TRUE;
}

/*********************************************************************
*
*       USBH_PRINTER_Exit
*
*  Function description:
*    Exit from application. Application has to wait until that all USBH_URB requests
*    completed before this function is called!
*
*/
void USBH_PRINTER_Exit(void) {
  USBH_PRINTER_INST * pInst;

  USBH_LOG((USBH_MTYPE_PRINTER_CLASS, "USBH_PRINTER_Exit"));
  for (pInst = (struct USBH_PRINTER_INST *)(USBH_PRINTER_Global.List.Head.pFirst); pInst; pInst = (struct USBH_PRINTER_INST *)pInst->Next.Link.pNext) {   // Iterate over all instances
    if (pInst->hInterface != NULL) {
      USBH_CloseInterface(pInst->hInterface);
      pInst->hInterface = NULL;
    }
    USBH_PRINTER_Global.NumDevices--;
  }
  if (USBH_PRINTER_Global.hDevNotification != NULL) {
    USBH_UnregisterPnPNotification(USBH_PRINTER_Global.hDevNotification);
    USBH_PRINTER_Global.hDevNotification = NULL;
  }
  _RemoveAllInstances();
}

/*********************************************************************
*
*       USBH_PRINTER_Open
*
*  Function description:
*
*
*  Parameters:
*    sName    - Pointer to a name of the device eg. prt001 for device 0.
*
*  Return value:
*    != 0     - Handle to a printing device
*    == 0     - Device not available or
*
*/
USBH_PRINTER_HANDLE USBH_PRINTER_Open(const char * sName) {
  USBH_PRINTER_INST * pInst;
  USBH_PRINTER_HANDLE Handle;

  Handle = 0;
  pInst = USBH_PRINTER_Global.List.pFirst;
  do {
    if (strcmp(sName, pInst->acName) == 0) {
      //
      // Device found
      //
      Handle = pInst->Handle;
      pInst->IsOpened = 1;
      break;
    }
    pInst = pInst->Next.pInst;
  } while (pInst);
  return Handle;
}

/*********************************************************************
*
*       USBH_PRINTER_Write
*
*  Function description:
*    Sends a number of bytes to a printer.
*
*  Parameters:
*    hDevice    - Handle to printer
*    pData      - Pointer to data to be sent
*    NumBytes   - Number of bytes to send
*
*  Return value:
*    USBH_STATUS
*/
USBH_STATUS USBH_PRINTER_Write(USBH_PRINTER_HANDLE hDevice, const U8 * pData, unsigned NumBytes) {
  USBH_PRINTER_INST * pInst;

  pInst = _h2p(hDevice);
  if (pInst) {
    if (pInst->IsOpened) {
      EP_DATA * pEPData;

      pEPData = &pInst->CmdOut;
      pEPData->Urb.Header.pContext                 = pInst;
      pEPData->Urb.Header.Function                 = USBH_FUNCTION_BULK_REQUEST;
      pEPData->Urb.Request.BulkIntRequest.Endpoint = pEPData->EPAddr;
      pEPData->Urb.Request.BulkIntRequest.pBuffer  = (U8 *)pData;
      pEPData->Urb.Request.BulkIntRequest.Length   = NumBytes;
      return _SubmitUrbAndWait(pInst, pEPData, USBH_PRINTER_Global.DefaultTimeOut);
    }
  }
  return USBH_STATUS_ERROR;
}

/*********************************************************************
*
*       USBH_PRINTER_Read
*
*  Function description:
*
*
*  Parameters:
*    hDevice    -
*    pData      -
*    NumBytes   -
*
*  Return value:
*    USBH_STATUS       -
*
*/
USBH_STATUS USBH_PRINTER_Read(USBH_PRINTER_HANDLE hDevice, U8 * pData, unsigned NumBytes) {
  USBH_PRINTER_INST * pInst;

  pInst = _h2p(hDevice);
  if (pInst) {
    if (pInst->IsOpened) {
      EP_DATA * pEPData;

      pEPData = &pInst->CmdIn;
      pEPData->Urb.Header.Function                 = USBH_FUNCTION_BULK_REQUEST;
      pEPData->Urb.Request.BulkIntRequest.Endpoint = pEPData->EPAddr;
      pEPData->Urb.Request.BulkIntRequest.pBuffer  = pData;
      pEPData->Urb.Request.BulkIntRequest.Length   = NumBytes;
      return _SubmitUrbAndWait(pInst, pEPData, USBH_PRINTER_Global.DefaultTimeOut);
    }
  }
  return USBH_STATUS_ERROR;
}

/*********************************************************************
*
*       USBH_PRINTER_Close
*
*  Function description:
*    Closes a handle to an opened device.
*
*  Parameters:
*    hDevice    -  Handle to the opened device
*
*  Return value:
*    == 0          - Success
*    != 0          - Error
*
*/
USBH_STATUS USBH_PRINTER_Close(USBH_PRINTER_HANDLE hDevice) {
  USBH_PRINTER_INST * pInst;

  pInst = _h2p(hDevice);
  if (pInst) {
    pInst->IsOpened = 0;
    return USBH_STATUS_SUCCESS;
  }
  return USBH_STATUS_ERROR;
}

/*********************************************************************
*
*       USBH_PRINTER_GetNumDevices
*
*  Function description:
*     Returns the number of available devices.
*
*  Return value:
*    Number of devices available
*
*/
int USBH_PRINTER_GetNumDevices(void) {
  return USBH_PRINTER_Global.NumDevices;
}

/*********************************************************************
*
*       USBH_PRINTER_RegisterNotification
*
*  Function description:
*
*
*  Parameters:
*    pfNotification    -
*    pContext    -
*
*  Return value:
*    int       -
*
*/
void USBH_PRINTER_RegisterNotification(USBH_NOTIFICATION_FUNC * pfNotification, void * pContext) {
  USBH_PRINTER_Global.pfOnUserNotification = pfNotification;
  USBH_PRINTER_Global.pfUserNotificationContext = pContext;

}

/*********************************************************************
*
*       USBH_PRINTER_GetPortStatus
*
*  Function description:
*
*
*  Parameters:
*    hDevice    -
*    pData    -
*    NumBytes    -
*
*  Return value:
*    USBH_STATUS       -
*
*/
USBH_STATUS USBH_PRINTER_GetPortStatus(USBH_PRINTER_HANDLE hDevice, U8 * pStatus) {
  USBH_PRINTER_INST * pInst;

  pInst = _h2p(hDevice);
  if (pInst) {
    if (pInst->IsOpened) {
      EP_DATA * pEPData;

      pEPData = &pInst->Control;
      pEPData->Urb.Header.Function                      = USBH_FUNCTION_CONTROL_REQUEST;
      pEPData->Urb.Request.ControlRequest.Endpoint      = 0;
      pEPData->Urb.Request.ControlRequest.Setup.Type    = 0xA1; // Interface, OUT, Class
      pEPData->Urb.Request.ControlRequest.Setup.Request = 0x01;
      pEPData->Urb.Request.ControlRequest.Setup.Value   = 0x0000;
      pEPData->Urb.Request.ControlRequest.Setup.Index   = pInst->DevInterfaceID;
      pEPData->Urb.Request.ControlRequest.Setup.Length  = 1;
      pEPData->Urb.Request.ControlRequest.pBuffer       = pStatus;
      pEPData->Urb.Request.ControlRequest.Length        = 1;
      return _SubmitUrbAndWait(pInst, pEPData, 1000);
    }
  }
  return USBH_STATUS_ERROR;

}

/*********************************************************************
*
*       USBH_PRINTER_ExecSoftReset
*
*  Function description:
*
*
*  Parameters:
*    hDevice    -
*
*  Return value:
*    USBH_STATUS       -
*
*/
USBH_STATUS USBH_PRINTER_ExecSoftReset(USBH_PRINTER_HANDLE hDevice) {
  USBH_PRINTER_INST * pInst;

  pInst = _h2p(hDevice);
  if (pInst) {
    if (pInst->IsOpened) {
      EP_DATA * pEPData;

      pEPData = &pInst->Control;
      pEPData->Urb.Header.Function                      = USBH_FUNCTION_CONTROL_REQUEST;
      pEPData->Urb.Request.ControlRequest.Endpoint      = 0;
      pEPData->Urb.Request.ControlRequest.Setup.Type    = 0x21; // Interface, OUT, Class
      pEPData->Urb.Request.ControlRequest.Setup.Request = 0x02;
      pEPData->Urb.Request.ControlRequest.Setup.Value   = 0x0000;
      pEPData->Urb.Request.ControlRequest.Setup.Index   = pInst->DevInterfaceID;
      pEPData->Urb.Request.ControlRequest.Setup.Length  = 0;
      pEPData->Urb.Request.ControlRequest.pBuffer       = NULL;
      pEPData->Urb.Request.ControlRequest.Length        = 0;
      return _SubmitUrbAndWait(pInst, pEPData, 1000);
    }
  }
  return USBH_STATUS_ERROR;

}

/*********************************************************************
*
*       USBH_PRINTER_GetDeviceId
*
*  Function description:
*    Ask the USB printer to send the IEEE.1284 ID string.
*
*  Parameters:
*    hDevice    - Handle to the opened printer device
*    pData      - Pointer to a buffer that shall store the IEE1284 string
*    NumBytes   - Number of bytes for that were reserved for the buffer.
*
*  Return value:
*    USBH_STATUS       -
*
*/
USBH_STATUS USBH_PRINTER_GetDeviceId(USBH_PRINTER_HANDLE hDevice, U8 * pData, unsigned NumBytes) {
  USBH_PRINTER_INST * pInst;

  pInst = _h2p(hDevice);
  if (pInst) {
    if (pInst->IsOpened) {
      EP_DATA * pEPData;

      pEPData = &pInst->Control;
      pEPData->Urb.Header.Function                      = USBH_FUNCTION_CONTROL_REQUEST;
      pEPData->Urb.Request.ControlRequest.Endpoint      = 0;
      pEPData->Urb.Request.ControlRequest.Setup.Type    = 0xA1; // Interface, OUT, Class
      pEPData->Urb.Request.ControlRequest.Setup.Request = 0x00;
      pEPData->Urb.Request.ControlRequest.Setup.Value   = 0x0000;
      pEPData->Urb.Request.ControlRequest.Setup.Index   = pInst->DevInterfaceID;
      pEPData->Urb.Request.ControlRequest.Setup.Length  = NumBytes;
      pEPData->Urb.Request.ControlRequest.pBuffer       = pData;
      pEPData->Urb.Request.ControlRequest.Length        = NumBytes;
      return _SubmitUrbAndWait(pInst, pEPData, 1000);
    }
  }
  return USBH_STATUS_ERROR;
}

/*********************************************************************
*
*       USBH_PRINTER_ConfigureTimeout
*
*  Function description:
*     Sets up the default timeout the host waits until the
*     data transfer will be aborted.
*
*  Parameters:
*     Timeout   - Timeout given in ms
*
*/
void USBH_PRINTER_ConfigureTimeout(U32 Timeout) {
  USBH_PRINTER_Global.DefaultTimeOut = Timeout;
}


/*************************** EOF ************************************/
