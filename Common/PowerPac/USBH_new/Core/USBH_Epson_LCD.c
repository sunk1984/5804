/*********************************************************************
*  
*   IAR PowerPac
*
*   (c) Copyright IAR Systems 2010.  All rights reserved.
*
**********************************************************************
----------------------------------------------------------------------
File        : USBH_Epson_LCD.c
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
#define NUM_DEVICES          2
/*********************************************************************
*
*       Define non-configurable
*
**********************************************************************
*/
typedef enum _USBH_LCD_STATE {
  StateIdle,  // Initial state, set if UsbhHid_InitDemoApp is called
  StateStop,  // Device is removed or an user break
  StateError, // Application error
  StateInit,  // Set during device initialization
  StateRunning
} USBH_LCD_STATE;


typedef struct USBH_LCD_INST USBH_LCD_INST;

typedef struct EP_DATA {
  U8   EPAddr;
  U16  MaxPacketSize;
  USBH_URB            Urb;
  USBH_OS_EVENT_OBJ * pEvent;
  unsigned            RefCount;
} EP_DATA;

struct USBH_LCD_INST {
  union {
    USBH_DLIST_ITEM             Link;
    struct USBH_LCD_INST *      pInst;
  } Next;
  USBH_LCD_STATE                RunningState;
  U8                            IsUsed;
  USBH_INTERFACE_ID             InterfaceID;
  U8                            DevInterfaceID;
  USBH_INTERFACE_HANDLE         hInterface;
  USBH_URB                      AbortUrb;
  USBH_URB                      ControlUrb;
  EP_DATA                       CmdIn;
  EP_DATA                       CmdOut;
  EP_DATA                       EventStatus;
  EP_DATA                       DispTransfer;
  U8                            IsOpened;
  USBH_OS_EVENT_OBJ           * pAbortTransactionEvent;
  USBH_LCD_HANDLE               Handle;
  char                          acName[8];
};

typedef struct {
  union {
    USBH_LCD_INST           * pFirst;
    USBH_DLIST_HEAD           Head;
  } List;
  U8                          NumDevices;
  USBH_NOTIFICATION_HANDLE    hDevNotification;
  int                         NextHandle;
} USBH_LCD_GLOBAL;

typedef void (USBH_LCD_USER_FUNC)(void * pContextData);

/*********************************************************************
*
*       Static data
*
**********************************************************************
*/
static USBH_LCD_GLOBAL           USBH_LCD_Global;

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
*       _h2p()
*/
static USBH_LCD_INST * _h2p(USBH_LCD_HANDLE Handle) {
  USBH_LCD_INST * pInst;

  if (Handle == 0) {
    return NULL;
  }

  //
  // Check if the first instance is a match (which is so in most cases)
  //
  pInst = (USBH_LCD_INST *)(USBH_LCD_Global.List.pFirst);                // First instance
  if (pInst == NULL) {
    USBH_WARN((USBH_MTYPE_HID, "Instance list is empty"));
    return NULL;
  }
  if (pInst->Handle == Handle) {                                        // Match ?
    return pInst;
  }
  //
  // Iterate over linked list to find a socket with matching handle. Return if found.
  //
  do {
    pInst = (USBH_LCD_INST *)pInst->Next.Link.pNext;
    if (pInst == NULL) {
      break;
    }
    if (pInst->Handle == Handle) {                                        // Match ?
      //
      // If it is not the first in the list, make it the first
      //
      if (pInst != (USBH_LCD_INST *)(USBH_LCD_Global.List.Head.pFirst)) {     // First socket ?
        USBH_DLIST_Remove(&USBH_LCD_Global.List.Head, &pInst->Next.Link);
        USBH_DLIST_Add(&USBH_LCD_Global.List.Head, &pInst->Next.Link);
      }
      return pInst;
    }
  } while(1);

  //
  // Error handling: Socket handle not found in list.
  //
  USBH_WARN((USBH_MTYPE_HID, "HANDLE: handle %d not in instance list", Handle));
  return NULL;
}

/*********************************************************************
*
*       _RemoveDevInstance
*
*/
static void _RemoveDevInstance(USBH_LCD_INST * pInst) {
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
    USBH_DLIST_Remove(&USBH_LCD_Global.List.Head, (USBH_DLIST_ITEM *)pInst);
    //
    // Free the memory that is used by the instance
    //
    USBH_Free(pInst);
    pInst = (USBH_LCD_INST *)NULL;
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
static USBH_BOOL _CheckStateAndCloseInterface(USBH_LCD_INST * pInst) {
  USBH_BOOL r = FALSE;

  if (pInst->RunningState == StateStop || pInst->RunningState == StateError) {
    if (pInst->hInterface != NULL) {
      USBH_CloseInterface(pInst->hInterface);
      USBH_LCD_Global.NumDevices--;
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
  USBH_LCD_INST * pInst;
  USBH_BOOL       r;

  r = FALSE;
  for (pInst = (USBH_LCD_INST *)(USBH_LCD_Global.List.Head.pFirst); pInst; pInst = (USBH_LCD_INST *)pInst->Next.Link.pNext) {   // Iterate over all instances
     r |= _CheckStateAndCloseInterface(pInst);
  }
  return r;
}

/*********************************************************************
*
*       _CreateDevInstance
*
*/
static USBH_LCD_INST * _CreateDevInstance(void) {
  USBH_LCD_INST * pInst;
  int i;

  //
  // Check if max. number of sockets allowed is exceeded
  //
  if ((USBH_LCD_Global.NumDevices + 1) >= NUM_DEVICES) {
    USBH_PANIC("No instance available for creating a new Epson LCD device");
  }
  //
  // Use next available socket handle.
  // Valid handles are positive integers; handles are assigned in increasing order from 1.
  // When the handle reaches a certain limit, we restart at 1.
  // Wrap around if necessary and make sure socket handle is not yet in use
  //
  i = USBH_LCD_Global.NextHandle + 1;
SearchDuplicate:
  if (i >= 0xFFFF) {
    i = 1;
  }
  for (pInst = (struct USBH_LCD_INST *)(USBH_LCD_Global.List.Head.pFirst); pInst; pInst = (struct USBH_LCD_INST *)pInst->Next.Link.pNext) {   // Iterate over all instances
    if (i == pInst->Handle) {
      i++;
      goto SearchDuplicate;
    }
  }
  USBH_LCD_Global.NextHandle = i;
  //
  // We found a valid new handle!
  // Perform the actual allocation
  //
  pInst = (USBH_LCD_INST *)USBH_MallocZeroed(sizeof(USBH_LCD_INST));
  if (pInst) {
    USBH_DLIST_Add(&USBH_LCD_Global.List.Head, (USBH_DLIST_ITEM *)pInst);
    pInst->Handle = i;
  }
  pInst->hInterface     = NULL;
  pInst->InterfaceID    = 0;
  pInst->IsUsed         = 1;
  sprintf(pInst->acName, "lcd%.3d", USBH_LCD_Global.NumDevices);
  USBH_LCD_Global.NumDevices++;
  return pInst;
}

/*********************************************************************
*
*       _OnGeneralCompletion
*
*/
#if 0
static void _OnGeneralCompletion(USBH_URB * pUrb) {
  USBH_USE_PARA(pUrb);
  USBH_OS_SetEvent((USBH_OS_EVENT_OBJ *)pUrb->Header.pContext);
}

/*********************************************************************
*
*       _OnIntInCompletion
*
*  Function description:
*    Is called when an URB is completed.
*/
static void _OnIntInCompletion(USBH_URB * pUrb) {
  USBH_LCD_INST * pInst;

  T_ASSERT(pUrb != NULL);
  pInst = (USBH_LCD_INST * )pUrb->Header.pContext;
  if (_CheckStateAndCloseInterface(pInst)) {
    return;
  }
  if (pInst->RunningState == StateStop || pInst->RunningState == StateError) {
    USBH_WARN((USBH_MTYPE_HID, "Error:  _OnIntInCompletion: App. has an error or is stopped!"));
    goto End;
  }
  if (pUrb->Header.Status != USBH_STATUS_SUCCESS) {
    USBH_WARN((USBH_MTYPE_HID, "UsbhHidReadCompletion: URB completed with Status 0x%08X ", pUrb->Header.Status));
  }
End:
  if (pInst->EventStatus.pEvent) {
    USBH_OS_SetEvent(pInst->EventStatus.pEvent);
  }
  if (pUrb->Header.pfOnUserCompletion) {
    (pUrb->Header.pfOnUserCompletion)(pUrb->Header.pUserContext);
  }
}
#endif

/*********************************************************************
*
*       _OnCmdOutCompletion
*
*  Function description:
*    Is called when an URB is completed.
*/
static void _OnCmdOutCompletion(USBH_URB * pUrb) {
  USBH_LCD_INST * pInst;

  USBH_ASSERT(pUrb != NULL);
  pInst = (USBH_LCD_INST * )pUrb->Header.pContext;


  if (pInst->CmdOut.pEvent) {
    USBH_OS_SetEvent(pInst->CmdOut.pEvent);
  }
  if (pUrb->Header.pfOnUserCompletion) {
    (pUrb->Header.pfOnUserCompletion)(pUrb->Header.pUserContext);
  }
}

/*********************************************************************
*
*       _OnCmdInCompletion
*
*  Function description:
*    Is called when an URB is completed.
*/
static void _OnCmdInCompletion(USBH_URB * pUrb) {
  USBH_LCD_INST * pInst;

  USBH_ASSERT(pUrb != NULL);
  pInst = (USBH_LCD_INST * )pUrb->Header.pContext;


  if (pInst->CmdIn.pEvent) {
    USBH_OS_SetEvent(pInst->CmdIn.pEvent);
  }
  if (pUrb->Header.pfOnUserCompletion) {
    (pUrb->Header.pfOnUserCompletion)(pUrb->Header.pUserContext);
  }
}
/*********************************************************************
*
*       _OnCmdOutCompletion
*
*  Function description:
*    Is called when an URB is completed.
*/
static void _OnDisplayOutCompletion(USBH_URB * pUrb) {
  USBH_LCD_INST * pInst;

  USBH_ASSERT(pUrb != NULL);
  pInst = (USBH_LCD_INST * )pUrb->Header.pContext;


  if (pInst->DispTransfer.pEvent) {
    USBH_OS_SetEvent(pInst->DispTransfer.pEvent);
  }
  if (pUrb->Header.pfOnUserCompletion) {
    (pUrb->Header.pfOnUserCompletion)(pUrb->Header.pUserContext);
  }
}

/**********************************************************************
*
*       _SubmitEventStatus
*
*/
#if 0
static USBH_STATUS _SubmitEventStatus(USBH_LCD_INST * pInst, U8 * pBuffer, U32 NumBytes, USBH_LCD_USER_FUNC * pfUser, void * pUserContext) {
  USBH_STATUS Status;
  pInst->EventStatus.Urb.Header.pContext                 = pInst;
  pInst->EventStatus.Urb.Header.pfOnCompletion           = _OnIntInCompletion;
  pInst->EventStatus.Urb.Header.Function                 = USBH_FUNCTION_INT_REQUEST;
  pInst->EventStatus.Urb.Header.pfOnUserCompletion       = pfUser;
  pInst->EventStatus.Urb.Header.pUserContext             = pUserContext;
  pInst->EventStatus.Urb.Request.BulkIntRequest.Endpoint = pInst->EventStatus.EPAddr;
  pInst->EventStatus.Urb.Request.BulkIntRequest.Buffer   = pBuffer;
  pInst->EventStatus.Urb.Request.BulkIntRequest.Length   = NumBytes;
  Status = USBH_SubmitUrb(pInst->hInterface, &pInst->EventStatus.Urb);
  if (Status != USBH_STATUS_PENDING) {
    USBH_WARN((USBH_MTYPE_HID, "_SubmitBuffer: USBH_SubmitUrb (0x%08x)", Status));
  } else {
    Status = USBH_STATUS_SUCCESS;
  }
  return Status;
}
#endif
/**********************************************************************
*
*       _SubmitCmdStatus
*
*/
static USBH_STATUS _SubmitCmdStatus(USBH_LCD_INST * pInst, U8 * pBuffer, U32 NumBytes, USBH_LCD_USER_FUNC * pfUser, void * pUserContext) {
  USBH_STATUS Status;
  pInst->CmdIn.Urb.Header.pContext                 = pInst;
  pInst->CmdIn.Urb.Header.pfOnCompletion           = _OnCmdInCompletion;
  pInst->CmdIn.Urb.Header.Function                 = USBH_FUNCTION_BULK_REQUEST;
  pInst->CmdIn.Urb.Header.pfOnUserCompletion       = pfUser;
  pInst->CmdIn.Urb.Header.pUserContext             = pUserContext;
  pInst->CmdIn.Urb.Request.BulkIntRequest.Endpoint = pInst->CmdIn.EPAddr;
  pInst->CmdIn.Urb.Request.BulkIntRequest.pBuffer   = pBuffer;
  pInst->CmdIn.Urb.Request.BulkIntRequest.Length   = NumBytes;
  Status = USBH_SubmitUrb(pInst->hInterface, &pInst->CmdIn.Urb);
  if (Status != USBH_STATUS_PENDING) {
    USBH_WARN((USBH_MTYPE_HID, "_SubmitBuffer: USBH_SubmitUrb (0x%08x)", Status));
  } else {
    Status = USBH_STATUS_SUCCESS;
  }
  return Status;
}


/**********************************************************************
*
*       _SubmitCmd
*
*/
static USBH_STATUS _SubmitCmd(USBH_LCD_INST * pInst, U8 * pBuffer, U32 NumBytes, USBH_LCD_USER_FUNC * pfUser, void * pUserContext) {
  USBH_STATUS Status;
  pInst->CmdOut.Urb.Header.pContext                 = pInst;
  pInst->CmdOut.Urb.Header.pfOnCompletion           = _OnCmdOutCompletion;
  pInst->CmdOut.Urb.Header.Function                 = USBH_FUNCTION_BULK_REQUEST;
  pInst->CmdOut.Urb.Header.pfOnUserCompletion       = pfUser;
  pInst->CmdOut.Urb.Header.pUserContext             = pUserContext;
  pInst->CmdOut.Urb.Request.BulkIntRequest.Endpoint = pInst->CmdOut.EPAddr;
  pInst->CmdOut.Urb.Request.BulkIntRequest.pBuffer   = pBuffer;
  pInst->CmdOut.Urb.Request.BulkIntRequest.Length   = NumBytes;
  Status = USBH_SubmitUrb(pInst->hInterface, &pInst->CmdOut.Urb);
  if (Status != USBH_STATUS_PENDING) {
    USBH_WARN((USBH_MTYPE_HID, "_SubmitOutBuffer: USBH_SubmitUrb returned (0x%08x)", Status));
  } else {
    Status = USBH_STATUS_SUCCESS;
  }
  return Status;
}

/**********************************************************************
*
*       _SubmitDisplayData
*
*/
static USBH_STATUS _SubmitDisplayData(USBH_LCD_INST * pInst, U8 * pBuffer, U32 NumBytes, USBH_LCD_USER_FUNC * pfUser, void * pUserContext) {
  USBH_STATUS Status;
  pInst->DispTransfer.Urb.Header.pContext                 = pInst;
  pInst->DispTransfer.Urb.Header.pfOnCompletion           = _OnDisplayOutCompletion;
  pInst->DispTransfer.Urb.Header.Function                 = USBH_FUNCTION_BULK_REQUEST;
  pInst->DispTransfer.Urb.Header.pfOnUserCompletion       = pfUser;
  pInst->DispTransfer.Urb.Header.pUserContext             = pUserContext;
  pInst->DispTransfer.Urb.Request.BulkIntRequest.Endpoint = pInst->DispTransfer.EPAddr;
  pInst->DispTransfer.Urb.Request.BulkIntRequest.pBuffer   = pBuffer;
  pInst->DispTransfer.Urb.Request.BulkIntRequest.Length   = NumBytes;
  Status = USBH_SubmitUrb(pInst->hInterface, &pInst->DispTransfer.Urb);
  if (Status != USBH_STATUS_PENDING) {
    USBH_WARN((USBH_MTYPE_HID, "_SubmitOutBuffer: USBH_SubmitUrb returned (0x%08x)", Status));
  } else {
    Status = USBH_STATUS_SUCCESS;
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
static void _StopDevice(USBH_LCD_INST * pInst) {
  USBH_USE_PARA(pInst);
#if 0
  USBH_STATUS Status;

  if (StateStop == pInst->RunningState || StateError == pInst->RunningState) {
    USBH_LOG((USBH_MTYPE_HID, "USBH_LCD_Stop: app. already stopped state: %d!", pInst->RunningState));
    return;
  }
  // Stops submitting of new URBs from the application
  pInst->RunningState = StateStop;
  if (NULL == pInst->hInterface) {
    USBH_LOG((USBH_MTYPE_HID, "USBH_LCD_Stop: interface handle is null, nothing to do!"));
    return;
  }
  if (pInst->RefCnt) {
    // Pending URBS
    pInst->AbortUrb.Header.Function                  = USBH_FUNCTION_ABORT_ENDPOINT;
    pInst->AbortUrb.Header.pfOnCompletion            = NULL;
    pInst->AbortUrb.Request.EndpointRequest.Endpoint = pInst->IntEp;
    Status                                           = USBH_SubmitUrb(pInst->hInterface, &pInst->AbortUrb);
    if (Status) {
      USBH_WARN((USBH_MTYPE_HID, "USBH_LCD_Stop: USBH_AbortEndpoint st:0x%08x!", Status));
    }
  }
#endif
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
static USBH_STATUS _StartDevice(USBH_LCD_INST * pInst) {
  USBH_STATUS  Status;
  unsigned int Length;
  U8           aDescriptorBuffer[255];

  // Open the hid interface
  Status = USBH_OpenInterface(pInst->InterfaceID, TRUE, &pInst->hInterface);
  if (USBH_STATUS_SUCCESS != Status) {
    USBH_WARN((USBH_MTYPE_HID, "USBH_LCD_Start: USBH_OpenInterface failed 0x%08x!", Status));
    return Status;
  }
  //
  //  Get current configuration descriptor
  //
  Length = sizeof(aDescriptorBuffer);
  USBH_GetCurrentConfigurationDescriptor(pInst->hInterface, aDescriptorBuffer, &Length);
  pInst->CmdOut.EPAddr = 0x01;
  pInst->CmdOut.MaxPacketSize = 0x40;
  pInst->CmdOut.pEvent = USBH_OS_AllocEvent();

  pInst->CmdIn.EPAddr = 0x82;
  pInst->CmdIn.MaxPacketSize = 0x40;
  pInst->CmdIn.pEvent = USBH_OS_AllocEvent();

  pInst->EventStatus.EPAddr = 0x83;
  pInst->EventStatus.MaxPacketSize = 0x40;
  pInst->EventStatus.pEvent = USBH_OS_AllocEvent();

  pInst->DispTransfer.EPAddr = 0x04;
  pInst->DispTransfer.MaxPacketSize = 0x40;
  pInst->DispTransfer.pEvent = USBH_OS_AllocEvent();

  return Status;
}

/*********************************************************************
*
*       _OnDevNotification
*
*  Function description:
*    TBD
*/
static void _OnDevNotification(USBH_LCD_INST * pInst, USBH_PNP_EVENT Event, USBH_INTERFACE_ID InterfaceID) {
  USBH_STATUS Status;
  switch (Event) {
    case USBH_ADD_DEVICE:
      USBH_LOG((USBH_MTYPE_HID, "_OnDeviceNotification: USB Epson LCD device detected interface ID: %u !", InterfaceID));
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
      USBH_LOG((USBH_MTYPE_HID, "_OnDeviceNotification: USB Epson LCD device removed interface  ID: %u !", InterfaceID));
      _StopDevice(pInst);
      _CheckStateAndCloseInterface(pInst);
      break;
    default:
      USBH_WARN((USBH_MTYPE_HID, "_OnDeviceNotification: invalid Event: %d !", Event));
      break;
  }
}

/*********************************************************************
*
*       _OnDeviceNotification
*/
static void _OnDeviceNotification(void * pContext, USBH_PNP_EVENT Event, USBH_INTERFACE_ID InterfaceID) {
  USBH_LCD_INST * pInst;

  USBH_USE_PARA(pContext);
  if (Event == USBH_ADD_DEVICE) {
    pInst = _CreateDevInstance();
  } else {
    for (pInst = (struct USBH_LCD_INST *)(USBH_LCD_Global.List.Head.pFirst); pInst; pInst = (struct USBH_LCD_INST *)pInst->Next.Link.pNext) {   // Iterate over all instances
      if (pInst->InterfaceID == InterfaceID) {
        break;
      }
    }
  }
  _OnDevNotification(pInst, Event, InterfaceID);
}

#if 0
/*********************************************************************
*
*       _OnAbortCompletion
*/
static void _OnAbortCompletion(USBH_URB * pUrb) {
  USBH_LCD_INST * pInst;

  T_ASSERT(pUrb != NULL);
  pInst = (USBH_LCD_INST * )pUrb->Header.pContext;
  if (pInst->pAbortTransactionEvent) {
    USBH_OS_SetEvent(pInst->pAbortTransactionEvent);
  }
}


/*********************************************************************
*
*       _CancelIO
*/
static int _CancelIO(USBH_LCD_INST * pInst) {
  pInst->pAbortTransactionEvent = USBH_OS_AllocEvent();
  if (pInst->EventStatus.Urb.Header.Status != USBH_STATUS_SUCCESS) {
    pInst->AbortUrb.Header.Function                  = USBH_FUNCTION_ABORT_ENDPOINT;
    pInst->AbortUrb.Header.pfOnCompletion            = _OnAbortCompletion;
    pInst->AbortUrb.Header.pContext                  = pInst;
    pInst->AbortUrb.Request.EndpointRequest.Endpoint = pInst->EventStatus.EPAddr;
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
*       USBH_LCD_Init
*
*  Function description:
*    Initialize the LCD device filter.
*
*  Return value:
*    TRUE   - Success
*/
USBH_BOOL USBH_LCD_Init(void) {
  USBH_PNP_NOTIFICATION   PnpNotify;
  USBH_INTERFACE_MASK   * pInterfaceMask;

  // Add an plug an play notification routine
  pInterfaceMask            = &PnpNotify.InterfaceMask;
  pInterfaceMask->Mask      = USBH_INFO_MASK_VID | USBH_INFO_MASK_PID;
  pInterfaceMask->VendorId       = 0x04b8;     // Epson
  pInterfaceMask->ProductId       = 0x052f;     // SED
  PnpNotify.pContext         = NULL;
  PnpNotify.pfPnpNotification = _OnDeviceNotification;
  USBH_LCD_Global.hDevNotification = USBH_RegisterPnPNotification(&PnpNotify); // Register the  PNP notification
  if (NULL == USBH_LCD_Global.hDevNotification) {
    USBH_WARN((USBH_MTYPE_HID, "USBH_LCD_Init: USBH_RegisterPnPNotification"));
    return FALSE;
  }
  return TRUE;
}

/*********************************************************************
*
*       USBH_LCD_Exit
*
*  Function description:
*    Exit from application. Application has to wait until that all URB requests
*    completed before this function is called!
*/
void USBH_LCD_Exit(void) {
  USBH_LCD_INST * pInst;

  USBH_LOG((USBH_MTYPE_HID, "USBH_LCD_Exit"));
  for (pInst = (struct USBH_LCD_INST *)(USBH_LCD_Global.List.Head.pFirst); pInst; pInst = (struct USBH_LCD_INST *)pInst->Next.Link.pNext) {   // Iterate over all instances
    if (pInst->hInterface != NULL) {
      USBH_CloseInterface(pInst->hInterface);
      pInst->hInterface = NULL;
    }
    USBH_LCD_Global.NumDevices--;
  }
  if (USBH_LCD_Global.hDevNotification != NULL) {
    USBH_UnregisterPnPNotification(USBH_LCD_Global.hDevNotification);
    USBH_LCD_Global.hDevNotification = NULL;
  }
  _RemoveAllInstances();
}

/*********************************************************************
*
*       USBH_LCD_Open
*
*  Function description:
*
*
*  Parameters:
*    sName    - Pointer to a name of the device eg. hid001 for device 0.
*
*  Return value:
*    != 0     - Handle to a LCD device
*    == 0     - Device not available or
*
*/
USBH_LCD_HANDLE USBH_LCD_Open(const char * sName) {
  USBH_LCD_INST * pInst;
  USBH_LCD_HANDLE Handle;

  Handle = 0;
  pInst = USBH_LCD_Global.List.pFirst;
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
*       USBH_LCD_WriteCmd
*
*  Function description:
*
*
*  Parameters:
*    hDevice    -
*    pCmdBuffer    -
*    NumBytesCmd    -
*    pStatusBuffer    -
*    NumBytesStatus    -
*
*  Return value:
*    USBH_STATUS       -
*
*/
USBH_STATUS USBH_LCD_WriteCmd(USBH_LCD_HANDLE hDevice, U8 * pCmdBuffer, unsigned NumBytesCmd, U8 * pStatusBuffer, unsigned NumBytesStatus) {
  USBH_LCD_INST * pInst;

  pInst = _h2p(hDevice);
  if (pInst) {
    if (pInst->IsOpened) {
      _SubmitCmd(pInst, pCmdBuffer, NumBytesCmd, NULL, NULL);
      _SubmitCmdStatus(pInst, pStatusBuffer, NumBytesStatus, NULL, NULL);
      USBH_OS_WaitEvent(pInst->CmdIn.pEvent);
      if (pInst->CmdIn.Urb.Header.Status == USBH_STATUS_SUCCESS) {
        return USBH_STATUS_SUCCESS;
      } else {
        USBH_WARN((USBH_MTYPE_APPLICATION, "Cmd was not successfully passed to device."));
      }
    }
  }
  return USBH_STATUS_ERROR;
}



/*********************************************************************
*
*       USBH_LCD_WriteDisplayData
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
USBH_STATUS USBH_LCD_WriteDisplayData(USBH_LCD_HANDLE hDevice, U8 * pData, unsigned NumBytes) {
  USBH_LCD_INST * pInst;

  pInst = _h2p(hDevice);
  if (pInst) {
    if (pInst->IsOpened) {
      _SubmitDisplayData(pInst, pData, NumBytes, NULL, NULL);
      USBH_OS_WaitEvent(pInst->DispTransfer.pEvent);
      if (pInst->DispTransfer.Urb.Header.Status == USBH_STATUS_SUCCESS) {
        return USBH_STATUS_SUCCESS;
      } else {
        USBH_WARN((USBH_MTYPE_APPLICATION, "Cmd was not successfully passed to device."));
      }
    }
  }
  return USBH_STATUS_ERROR;
}

/*********************************************************************
*
*       USBH_LCD_Close
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
USBH_STATUS USBH_LCD_Close(USBH_LCD_HANDLE hDevice) {
  USBH_LCD_INST * pInst;

  pInst = _h2p(hDevice);
  if (pInst) {
    pInst->IsOpened = 0;
    return USBH_STATUS_SUCCESS;
  }
  return USBH_STATUS_ERROR;
}

/*********************************************************************
*
*       USBH_LCD_GetNumDevices
*
*  Function description:
*     Returns the number of available devices.
*
*  Return value:
*    Number of devices available
*
*/
int USBH_LCD_GetNumDevices(void) {
  return USBH_LCD_Global.NumDevices;
}


/*************************** EOF ************************************/
