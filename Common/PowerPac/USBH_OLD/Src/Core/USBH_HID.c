/*********************************************************************
*  
*   IAR PowerPac
*
*   (c) Copyright IAR Systems 2010.  All rights reserved.
*
**********************************************************************
----------------------------------------------------------------------
File        : USBH_HID.c
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
#define NUM_DEVICES          3

/*********************************************************************
*
*       Define non-configurable
*
**********************************************************************
*/

#define DEC_HID_REF()      pInst->RefCnt--; T_ASSERT((int)pInst->RefCnt >= 0)
#define INC_HID_REF()      pInst->RefCnt++
#define HID_MOUSE          1
#define HID_KEYBOARD       2

/*********************************************************************
*
*       Data structures
*
**********************************************************************
*/

typedef enum _USBH_HID_STATE {
  StateIdle,  // Initial state, set if UsbhHid_InitDemoApp is called
  StateStop,  // Device is removed or an user break
  StateError, // Application error
  StateInit,  // Set during device initialization
  StateRunning
} USBH_HID_STATE;

typedef struct {
  U8                          IsUsed;
  USBH_HID_STATE              RunningState;
  USBH_INTERFACE_ID           InterfaceID;
  USBH_INTERFACE_HANDLE       hInterface;
  U8                          IntEp;
  U8                          OutEp;
  int                         MaxPktSize; // Maximum packet size, important read only with this size from the device
  URB                         OutUrb;
  URB                         InUrb;
  URB                         AbortUrb;
  URB                         ControlUrb;
  int                         ReadErrorCount;
  U32                         RefCnt;
  U8                          aReportBufferDesc[100];
  U8                          aIntBuffer[64];
  U8                          aOldState[8];
  U8                          DeviceType;
  USBH_HID_ON_KEYBOARD_FUNC * pfOnKeyStateChange;
  USBH_HID_ON_MOUSE_FUNC    * pfOnMouseStateChange;
  USBH_HID_ON_SCANGUN_FUNC  * pfOnScanGunStateChange;
} USBH_HID_INST;

/*********************************************************************
*
*       Static data
*
**********************************************************************
*/

//static unsigned                 _Len;
//static U8                       _acBuffer[200];
static USBH_HID_INST            _aInst[NUM_DEVICES];
static U8                       _NumDevices;
static USBH_NOTIFICATION_HANDLE _hMouseDevNotification;
static USBH_NOTIFICATION_HANDLE _hKeyboardDevNotification;
static U8                       _LedState;

/*********************************************************************
*
*       Static prototypes
*
**********************************************************************
*/
static void        _OnResetReadEndpointCompletion(URB * Urb);
static USBH_STATUS _SubmitBuffer(USBH_HID_INST * pInst);

/*********************************************************************
*
*       Static code
*
**********************************************************************
*/

#if 0

/*********************************************************************
*
*       _NibbleToChar
*/
static char _NibbleToChar(unsigned int n) {
  n = n & 0xF;
  return (char)(n <= 9 ? ('0' + n) : ('A' + (n - 10)));
}

/*********************************************************************
*
*       _StoreChar
*/
static void _StoreChar(char c) {
  _acBuffer[_Len++] = c;
}

/*********************************************************************
*
*       _DumpData
*
*  Function description:
*    Dumps the pData that are stored in the buffer.
*
*/
static void _DumpData(const void * pData, unsigned int length) {
  const char   * p;
  const char   * s;
  unsigned int   adr, l;
  char           i;

  if (length == 0) {
    USBH_LOG((USBH_MTYPE_HID, "_DumpData: 0"));
  } else {
    s   = (char *)pData;
    adr = 0;
    for (; ;) {
      //
      // Print a line with 16 values
      //
      // print address
      _StoreChar(_NibbleToChar(adr >> 12));
      _StoreChar(_NibbleToChar(adr >> 8));
      _StoreChar(_NibbleToChar(adr >> 4));
      _StoreChar(_NibbleToChar(adr));
      _StoreChar(':');
      _StoreChar(' ');
      //
      // Print hex values
      //
      l    = (int)(length - adr);
      adr += 16;
      p    = s;
      if (l > 16) {
        i = 16;
      } else {
        i = (char)l;
      }
      for (; i > 0; i--) {
        _StoreChar(_NibbleToChar(*p >> 4));
        _StoreChar(_NibbleToChar(*p));
        p++;
        _StoreChar(' ');
      }
      //
      // Print spaces
      //
      if (l < 16) {
        i = (char)(3 * (16 - l));
        for (; i > 0; i--)
          _StoreChar(' ');
      }
      //
      // Print ASCII values
      //
      if (l > 16) {
        i = 16;
      } else {
        i = (char)l;
      }
      p = s;
      for (; i > 0; i--) {
        if (* p >= ' ' && * p < 127) {
          _StoreChar(* p);
        } else {
          _StoreChar('.');
        }
        p++;
      }
      if (l <= 16) {
        USBH_LOG((USBH_MTYPE_HID, "%s", _acBuffer));
        _Len = 0;
        break;
      }
      s += 16;
    }
  }
}
#endif

/*********************************************************************
*
*       _IsValueInArray
*
*  Function description:
*    Checks whether a value is in the specified array.
*/
static int _IsValueInArray(const U8 * p, U8 Val, unsigned NumItems) {
  unsigned i;
  for (i = 0; i < NumItems; i++) {
    if (* p == Val) {
      return 1;
    }
  }
  return 0;
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
static T_BOOL _CheckStateAndCloseInterface(USBH_HID_INST * pInst) {
  T_BOOL r = FALSE;
  if (pInst->RunningState == StateStop || pInst->RunningState == StateError) {
    if (!pInst->RefCnt) {
      if (pInst->hInterface != NULL) {
        USBH_CloseInterface(pInst->hInterface);
        _NumDevices--;
        pInst->hInterface = NULL;
        pInst->IsUsed     = 0;
      }
    }
  }
  if (NULL == pInst->hInterface) {
    r = TRUE;
  }
  return r;
}

/*********************************************************************
*
*       _CreateDevInstance
*
*/
static USBH_HID_INST * _CreateDevInstance(void) {
  USBH_HID_INST * pInst;
  if ((_NumDevices + 1) >= NUM_DEVICES) {
    USBH_PANIC("No instance available for creating a new HID device");
  }
  pInst                 = &_aInst[_NumDevices++];
  pInst->hInterface     = NULL;
  pInst->RefCnt         = 0;
  pInst->ReadErrorCount = 0;
  pInst->InterfaceID    = 0;
  pInst->IsUsed         = 1;
  return pInst;
}

/*********************************************************************
*
*       _OnReportDescReceived
*
*  Function description:
*
*  Parameters:
*    Urb    -
*
*  Return value:
*    void   -
*/
static void _OnReportDescReceived(URB * Urb) { 
  USBH_USE_PARA(Urb);
}

/*********************************************************************
*
*       _UpdateLEDState
*
*  Function description:
*/
static void _UpdateLEDState(USBH_HID_INST * pInst) {
  if (pInst->OutEp) {
    pInst->OutUrb.Header.Context                  = NULL;
    pInst->OutUrb.Header.Completion               = NULL;
    pInst->OutUrb.Header.Function                 = USBH_FUNCTION_INT_REQUEST;
    pInst->OutUrb.Request.BulkIntRequest.Endpoint = pInst->OutEp;
    pInst->OutUrb.Request.BulkIntRequest.Buffer   = &_LedState;
    pInst->OutUrb.Request.BulkIntRequest.Length   = 1;
    USBH_SubmitUrb(pInst->hInterface, &pInst->OutUrb);
  } else {
    pInst->ControlUrb.Request.ControlRequest.Setup.Type    = 0x21; // STD, IN, device
    pInst->ControlUrb.Request.ControlRequest.Setup.Request = 0x09;
    pInst->ControlUrb.Request.ControlRequest.Setup.Value   = 0x0200;
    pInst->ControlUrb.Request.ControlRequest.Setup.Index   = 0;
    pInst->ControlUrb.Request.ControlRequest.Setup.Length  = 1;
    pInst->ControlUrb.Request.ControlRequest.Buffer        = &_LedState;
    pInst->ControlUrb.Request.ControlRequest.Length        = 1;
    USBH_SubmitUrb(pInst->hInterface, &pInst->ControlUrb);
  }
}

/*********************************************************************
*
*       _UpdateKeyState
*
*  Function description:
*/
static void _UpdateKeyState(USBH_HID_INST * pInst, unsigned Code, int Value) {
  USBH_HID_KEYBOARD_DATA   KeyData;
  KeyData.Code           = Code;
  KeyData.Value          = Value;
  if (pInst->pfOnKeyStateChange) {
    pInst->pfOnKeyStateChange(&KeyData);
  }
}

extern SCANCODE_TO_CH _aScanCode2ChTable[] ;
extern char  _ScanCode2Ch(unsigned Code);
/*********************************************************************
*
*       _ParseKeyboardData
*
*  Function description:
*/
static void _ParseKeyboardData(USBH_HID_INST * pInst, U8 * pNewState) {
  unsigned i;
  
#if 1 
/* 
  for(i=0;i<8;i++){   
        Dprintf("%2x ",*(pNewState+i));
  }
  Dprintf("\n\r");
*/  
  U8 tempData = * (pNewState + 2);
  USBH_HID_SCANGUN_DATA scanGunData ;
  if(( (tempData >= 0x1E) && (tempData <= 0x27) )|| ((tempData>= 0x4) && (tempData <= 0x9) ) ||(tempData==0x2D) ){
      scanGunData.data = _ScanCode2Ch(tempData);
      if (pInst->pfOnScanGunStateChange) {
        pInst->pfOnScanGunStateChange(&scanGunData);
      }
  }
  
#else
  U8       LedState = _LedState;
  for (i = 0; i < 8; i++) {
    _UpdateKeyState(pInst, 0xe0 + i, (pNewState[0] >> i) & 1);
  }
  for (i = 2; i < 8; i++) {
    if (pInst->aOldState[i] > 3 && (_IsValueInArray(pNewState + 2, pInst->aOldState[i], 6) == 0)) {
      if (pInst->aOldState[i]) {
        _UpdateKeyState(pInst, pInst->aOldState[i], 0);
      } else {
        USBH_WARN((USBH_MTYPE_HID, "Unknown key (HID scancode %#x) released.\n", pInst->aOldState[i]));
      }
    }
    if (pNewState[i] > 3 && _IsValueInArray(pInst->aOldState + 2, pNewState[i], 6) == 0) {
      if (pNewState[i]) {
        _UpdateKeyState(pInst, pNewState[i], 1);
        //  Update
        if (pNewState[i] == 0x39) {
          LedState ^= (1 << 1);
        }
        if (pNewState[i] == 0x47) {
          LedState ^= (1 << 2);
        }
        if (pNewState[i] == 0x53) {
          LedState ^= (1 << 0);
        }
      } else {
        USBH_WARN((USBH_MTYPE_HID, "Unknown key (HID scancode %#x) released.\n", pNewState[i]));
      }
    }
  }
  if (_LedState != LedState) {
    _LedState = LedState;
    _UpdateLEDState(pInst);
  }
  
#endif  
}

/*********************************************************************
*
*       _ParseMouseData
*/
static void _ParseMouseData(USBH_HID_INST * pInst, U8 * pNewState) {
  USBH_HID_MOUSE_DATA MouseData;
  MouseData.ButtonState = (I8)*pNewState;
  MouseData.xChange     = (I8)*(pNewState + 1);
  MouseData.yChange     = (I8)*(pNewState + 2);
  MouseData.WheelChange = (I8)*(pNewState + 3);
  if (pInst->pfOnMouseStateChange) {
    pInst->pfOnMouseStateChange(&MouseData);
  }
}

/*********************************************************************
*
*       _OnIntInCompletion
*
*  Function description:
*
*  Parameters:
*    pUrb    -
*
*  Return value:
*    void       -
*/
static void _OnIntInCompletion(URB * pUrb) {
  USBH_STATUS     Status;
  U32             Length;
  T_BOOL          DoResetEP;
  USBH_HID_INST * pInst;
  T_ASSERT(pUrb != NULL);
  pInst = (USBH_HID_INST * )pUrb->Header.Context;
  DEC_HID_REF(); // To get information about pending transfer requests
  if (_CheckStateAndCloseInterface(pInst)) {
    return;
  }
  if (pInst->RunningState == StateStop || pInst->RunningState == StateError) {
    USBH_WARN((USBH_MTYPE_HID, "HID Error:  _OnIntInCompletion: App. has an error or is stopped!"));
    return;
  }
  DoResetEP = FALSE;
  Length    = pUrb->Request.BulkIntRequest.Length;
  if (pUrb->Header.Status == USBH_STATUS_SUCCESS) {
    // Dump hid pData
    if (0 == Length) {
      USBH_LOG((USBH_MTYPE_HID, "_OnIntInCompletion(): zero Length packet received"));
    } else {
//      USBH_LOG((USBH_MTYPE_HID, "HID interrupt IN pData:"));
//      _DumpData(pUrb->Request.BulkIntRequest.Buffer, Length);
    }
    if (pInst->DeviceType == HID_KEYBOARD) {
      _ParseKeyboardData(pInst, pUrb->Request.BulkIntRequest.Buffer);
    } else {
      _ParseMouseData(pInst, pUrb->Request.BulkIntRequest.Buffer);
    }
    pInst->ReadErrorCount = 0; // On success clear error count
  } else {
    USBH_WARN((USBH_MTYPE_HID, "UsbhHidReadCompletion: URB completed with Status 0x%08X ", pUrb->Header.Status));
    pInst->ReadErrorCount++;
    if (MAX_TRANSFERS_ERRORS <= pInst->ReadErrorCount) {
      USBH_WARN((USBH_MTYPE_HID, "UsbhHidReadCompletion: Max error count: %d, read stopped", pInst->ReadErrorCount));
      pInst->RunningState = StateError;
    } else {
      // Reset the endpoint and resubmit an new buffer in the completion routine of the request use the same URB
      DoResetEP = TRUE;
      INC_HID_REF();
      Status = USBH_ResetEndpoint(pInst->hInterface, &pInst->InUrb, pInst->IntEp, _OnResetReadEndpointCompletion, (void * )pInst);
      if (Status != USBH_STATUS_PENDING) {
        DEC_HID_REF();
        USBH_WARN((USBH_MTYPE_HID, "_OnIntInCompletion: ResetEndpoint: (%08x), read stopped!", Status));
        pInst->RunningState = StateError;
      }
    }
  }
  if (DoResetEP) {
    return;
  }
  if (pInst->RunningState == StateInit || pInst->RunningState == StateRunning) {
    // Resubmit an transfer request
    Status = _SubmitBuffer(pInst);
    if (Status) {
      USBH_WARN((USBH_MTYPE_HID, "UsbhHidWriteCompletion: ResetEndpoint: (%08x)", Status));
    }
  }
}

/*********************************************************************
*
*       _GetReportDescriptor
*
*  Function description:
*/
static USBH_STATUS _GetReportDescriptor(USBH_HID_INST * pInst) {
  USBH_STATUS   Status;
  URB         * pURB;

  pURB                                       = &pInst->ControlUrb;
  pURB->Header.Context                       = NULL;
  pURB->Header.Completion                    = _OnReportDescReceived;
  pURB->Header.Function                      = USBH_FUNCTION_CONTROL_REQUEST;
  pURB->Request.ControlRequest.Setup.Type    = 0x81; // STD, IN, device
  pURB->Request.ControlRequest.Setup.Request = USB_REQ_GET_DESCRIPTOR;
  pURB->Request.ControlRequest.Setup.Value   = 0x2200;
  pURB->Request.ControlRequest.Setup.Index   = 0;
  pURB->Request.ControlRequest.Setup.Length  = 0x0076;
  pURB->Request.ControlRequest.Buffer        = pInst->aReportBufferDesc;
  pURB->Request.ControlRequest.Length        = 0x0076;
  Status                                     = USBH_SubmitUrb(pInst->hInterface, pURB);
  if (Status != USBH_STATUS_PENDING) {
    USBH_WARN((USBH_MTYPE_HID, "_GetReportDescriptor: USBH_SubmitUrb (0x%08x)", Status));
    return USBH_STATUS_ERROR;
  }
  return USBH_STATUS_SUCCESS;
}

/***************************s******************************************
*
*       _SubmitBuffer
*
*  Function description:
*    TBD
*/
static USBH_STATUS _SubmitBuffer(USBH_HID_INST * pInst) {
  USBH_STATUS Status;
  pInst->InUrb.Header.Context                  = pInst;
  pInst->InUrb.Header.Completion               = _OnIntInCompletion;
  pInst->InUrb.Header.Function                 = USBH_FUNCTION_INT_REQUEST;
  pInst->InUrb.Request.BulkIntRequest.Endpoint = pInst->IntEp;
  pInst->InUrb.Request.BulkIntRequest.Buffer   = pInst->aIntBuffer;
  pInst->InUrb.Request.BulkIntRequest.Length   = pInst->MaxPktSize;
  INC_HID_REF(); // Only for testing, counts the number of submitted URBs
  Status = USBH_SubmitUrb(pInst->hInterface, &pInst->InUrb);
  if (Status != USBH_STATUS_PENDING) {
    USBH_WARN((USBH_MTYPE_HID, "_SubmitBuffer: USBH_SubmitUrb (0x%08x)", Status));
    DEC_HID_REF();
  } else {
    Status = USBH_STATUS_SUCCESS;
  }
  return Status;
}

/*********************************************************************
*
*       _OnResetReadEndpointCompletion
*
*  Function description:
*    Endpoint reset is complete. It submits an new URB if possible!
*/
static void _OnResetReadEndpointCompletion(URB * pUrb) {
  USBH_HID_INST * pInst;
  T_ASSERT(pUrb != NULL);
  pInst = (USBH_HID_INST * )pUrb->Header.Context;
  DEC_HID_REF();
  if (_CheckStateAndCloseInterface(pInst)) {
    return;
  }
  if (pInst->RunningState == StateInit || pInst->RunningState == StateRunning) {
    // Resubmit an transfer request
    if (USBH_STATUS_SUCCESS != pUrb->Header.Status) {
      USBH_WARN((USBH_MTYPE_HID, "_OnResetReadEndpointCompletion: URB Status: 0x%08x!", pUrb->Header.Status));
      pInst->RunningState = StateError;
    } else {
      _SubmitBuffer(pInst);
    }
  }
}

/*********************************************************************
*
*       _StopDevice
*
*  Function description:
*    Is used to stop the HID class filter.
*    It is called if the user wants to stop the class filter.
*/
static void _StopDevice(USBH_HID_INST * pInst) {
  USBH_STATUS Status;
  if (StateStop == pInst->RunningState || StateError == pInst->RunningState) {
    USBH_LOG((USBH_MTYPE_HID, "USBH_HID_Stop: app. already stopped state: %d!", pInst->RunningState));
    return;
  }
  // Stops submitting of new URBs from the application
  pInst->RunningState = StateStop;
  if (NULL == pInst->hInterface) {
    USBH_LOG((USBH_MTYPE_HID, "USBH_HID_Stop: interface handle is null, nothing to do!"));
    return;
  }
  if (pInst->RefCnt) {
    // Pending URBS
    pInst->AbortUrb.Header.Function                  = USBH_FUNCTION_ABORT_ENDPOINT;
    pInst->AbortUrb.Header.Completion                = NULL;
    pInst->AbortUrb.Request.EndpointRequest.Endpoint = pInst->IntEp;
    Status                                           = USBH_SubmitUrb(pInst->hInterface, &pInst->AbortUrb);
    if (Status) {
      USBH_WARN((USBH_MTYPE_HID, "USBH_HID_Stop: USBH_AbortEndpoint st:0x%08x!", Status));
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
static USBH_STATUS _StartDevice(USBH_HID_INST * pInst) {
  USBH_STATUS  Status;
  USBH_EP_MASK EPMask;
  unsigned int Length;
  U8           aEpDesc[USB_ENDPOINT_DESCRIPTOR_LENGTH];
  U8           aConfDesc[255];
  // Open the hid interface
  Status = USBH_OpenInterface(pInst->InterfaceID, TRUE, &pInst->hInterface);
  if (USBH_STATUS_SUCCESS != Status) {
    USBH_WARN((USBH_MTYPE_HID, "USBH_HID_Start: USBH_OpenInterface failed 0x%08x!", Status));
    return Status;
  }
  // Get an interrupt in endpoint from the hid interface
  USBH_MEMSET(&EPMask, 0, sizeof(USBH_EP_MASK));
  EPMask.Mask      = USBH_EP_MASK_TYPE | USBH_EP_MASK_DIRECTION;
  EPMask.Direction = USB_IN_DIRECTION;
  EPMask.Type      = USB_EP_TYPE_INT;
  USBH_GetCurrentConfigurationDescriptor(pInst->hInterface, aConfDesc, sizeof(aConfDesc), &Length);
  Status           = USBH_GetEndpointDescriptor(pInst->hInterface, 0, &EPMask, aEpDesc, sizeof(aEpDesc), &Length);
  if (Status) {
    USBH_WARN((USBH_MTYPE_HID, "USBH_HID_Start: USBH_GetEndpointDescriptor failed st: %08x", Status));
    goto Err;
  } else {
    pInst->MaxPktSize = (int)(aEpDesc[USB_EP_DESC_PACKET_SIZE_OFS] + (aEpDesc[USB_EP_DESC_PACKET_SIZE_OFS + 1] << 8));
    USBH_LOG((USBH_MTYPE_HID, "Address   Attrib.   MaxPacketSize   Interval"));
    USBH_LOG((USBH_MTYPE_HID, "0x%02X      0x%02X      %5d             %d", (int)aEpDesc[USB_EP_DESC_ADDRESS_OFS], (int)aEpDesc[USB_EP_DESC_ATTRIB_OFS], pInst->MaxPktSize, (int)aEpDesc[USB_EP_DESC_INTERVAL_OFS]));
  }
  // Setup global var.
  pInst->IntEp = aEpDesc[USB_EP_DESC_ADDRESS_OFS];
  _GetReportDescriptor(pInst);
  // Submit URB
  Status = _SubmitBuffer(pInst);
  if (Status) { // On error
    USBH_WARN((USBH_MTYPE_HID, "USBH_HID_Start: _SubmitBuffer failed st: %08x", Status));
    goto Err;
  }
  return Status;
  Err: // on error
  USBH_CloseInterface(pInst->hInterface);
  pInst->InterfaceID = 0;
  pInst->hInterface  = NULL;
  return USBH_STATUS_ERROR;
}

/*********************************************************************
*
*       _OnDeviceNotification
*
*  Function description:
*    TBD
*/
static void _OnDeviceNotification(USBH_HID_INST * pInst, USBH_PNP_EVENT Event, USBH_INTERFACE_ID InterfaceID) {
  USBH_STATUS Status;
  switch (Event) {
    case USBH_AddDevice:
      USBH_LOG((USBH_MTYPE_HID, "_OnDeviceNotification: USB HID device detected interface ID: %u !", InterfaceID));
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
    case USBH_RemoveDevice:
      if (pInst->hInterface == NULL || pInst->InterfaceID != InterfaceID) {
        // Only one device is handled from the application at the same time
        return;
      }
      USBH_LOG((USBH_MTYPE_HID, "_OnDeviceNotification: USB HID device removed interface  ID: %u !", InterfaceID));
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
*       _OnMouseDeviceNotification
*/
static void _OnMouseDeviceNotification(void * pContext, USBH_PNP_EVENT Event, USBH_INTERFACE_ID InterfaceID) {
  USBH_HID_INST * pInst;
  
  USBH_USE_PARA(pContext);
  if (Event == USBH_AddDevice) {
    pInst = _CreateDevInstance();
    pInst->DeviceType = HID_MOUSE;
  } else {
    unsigned i;
    for (i = 0; i < USBH_COUNTOF(_aInst); i++) {
      pInst = &_aInst[i];
      if (pInst->InterfaceID == InterfaceID) {
        break;
      }
    }
  }
  _OnDeviceNotification(pInst, Event, InterfaceID);
}

/*********************************************************************
*
*       _OnKeyboardDeviceNotification
*/
static void _OnKeyboardDeviceNotification(void * pContext, USBH_PNP_EVENT Event, USBH_INTERFACE_ID InterfaceID) {
  USBH_HID_INST * pInst;

  USBH_USE_PARA(pContext);
  if (Event == USBH_AddDevice) {
    pInst = _CreateDevInstance();
    pInst->DeviceType = HID_KEYBOARD;
  } else {
    unsigned i;
    for (i = 0; i < USBH_COUNTOF(_aInst); i++) {
      pInst = &_aInst[i];
      if (pInst->InterfaceID == InterfaceID) {
        break;
      }
    }
  }
  _OnDeviceNotification(pInst, Event, InterfaceID);
}

int  USB_Enum_Ok = FALSE;

/*********************************************************************
*
*       _OnCompliantHIDDeviceNotification
*/

static void _OnCompliantHIDDeviceNotification(void * pContext, USBH_PNP_EVENT Event, USBH_INTERFACE_ID InterfaceID) {
  USBH_USE_PARA(pContext);
  if (Event == USBH_AddDevice) {
	USB_Enum_Ok = TRUE;
  } 
}

/*********************************************************************
*
*       Public code
*
**********************************************************************
*/

/*********************************************************************
*
*       USBH_HID_Init
*
*  Function description:
*    Initialize the HID sub class filter.
*
*  Return value:
*    TRUE   - Success
*/
#define HID_DEVICE_COMPLIANT_PROTOCOL                  0x00

T_BOOL USBH_HID_Init(void) {
  USBH_PNP_NOTIFICATION   PnpNotify;
  USBH_INTERFACE_MASK   * pInterfaceMask;

  // Add an plug an play notification routine
  pInterfaceMask            = &PnpNotify.InterfaceMask;
  pInterfaceMask->Mask      = USBH_INFO_MASK_CLASS | USBH_INFO_MASK_PROTOCOL;
  pInterfaceMask->Class     = USB_DEVICE_CLASS_HUMAN_INTERFACE;
  pInterfaceMask->Protocol  = HID_DEVICE_MOUSE_PROTOCOL;
  PnpNotify.Context         = NULL;
  // Register the PnP notification routine _OnDeviceNotification() for mouse and keyboard HID devices.
  // Because protocols are different call USBH_RegisterPnPNotification twice!
  PnpNotify.PnpNotification = _OnMouseDeviceNotification;
  _hMouseDevNotification    = USBH_RegisterPnPNotification(&PnpNotify); /* register HID mouse devices */
  if (NULL == _hMouseDevNotification) {
    USBH_WARN((USBH_MTYPE_HID, "USBH_HID_Init: USBH_RegisterPnPNotification"));
    return FALSE;
  }
  pInterfaceMask->Protocol  = HID_DEVICE_KEYBOARD_PROTOCOL;
  PnpNotify.PnpNotification = _OnKeyboardDeviceNotification;
  _hKeyboardDevNotification = USBH_RegisterPnPNotification(&PnpNotify); /* register HID keyboard devices */
  if (NULL == _hKeyboardDevNotification) {
    USBH_WARN((USBH_MTYPE_HID, "USBH_HID_Init: USBH_RegisterPnPNotification"));
    USBH_UnregisterPnPNotification(_hMouseDevNotification);
    _hMouseDevNotification  = NULL;
    return FALSE;
  }

  pInterfaceMask->Protocol  = HID_DEVICE_COMPLIANT_PROTOCOL;
  PnpNotify.PnpNotification = _OnCompliantHIDDeviceNotification;
  USBH_RegisterPnPNotification(&PnpNotify);

  return TRUE;
}

/*********************************************************************
*
*       USBH_HID_Exit
*
*  Function description:
*    Exit from application. Application has to wait until that all URB requests
*    completed before this function is called!
*/
void USBH_HID_Exit(void) {
  USBH_HID_INST * pInst;
  unsigned        i;
  USBH_LOG((USBH_MTYPE_HID, "USBH_HID_Exit"));
  for (i = 0; i < _NumDevices; i++) {
    T_ASSERT(0 == pInst->RefCnt); // No pending requests
    pInst = &_aInst[i];
    if (pInst->hInterface != NULL) {
      USBH_CloseInterface(pInst->hInterface);
      pInst->hInterface = NULL;
    }
  }
  if (_hMouseDevNotification != NULL) {
    USBH_UnregisterPnPNotification(_hMouseDevNotification);
    _hMouseDevNotification = NULL;
  }
  if (_hKeyboardDevNotification != NULL) {
    USBH_UnregisterPnPNotification(_hKeyboardDevNotification);
    _hKeyboardDevNotification = NULL;
  }
}

/*********************************************************************
*
*       USBH_HID_SetOnMouseStateChange
*
*  Function description:
*
*  Parameters:
*    pfOnChange -
*
*  Return value:
*    void       -
*/
void USBH_HID_SetOnMouseStateChange(USBH_HID_ON_MOUSE_FUNC * pfOnChange) {
  USBH_HID_INST * pInst;
  unsigned        i;
  for (i = 0; i < USBH_COUNTOF(_aInst); i++) {
    pInst = &_aInst[i];
    pInst->pfOnMouseStateChange = pfOnChange;
  }
}

void USBH_HID_SetOnScanGunStateChange(USBH_HID_ON_SCANGUN_FUNC * pfOnChange) {
  USBH_HID_INST * pInst;
  unsigned        i;
  for (i = 0; i < USBH_COUNTOF(_aInst); i++) {
    pInst = &_aInst[i];
    pInst->pfOnScanGunStateChange = pfOnChange;
  }
}

/*********************************************************************
*
*       USBH_HID_SetOnKeyboardStateChange
*
*  Function description:
*
*  Parameters:
*    pfOnChange    -
*
*  Return value:
*    void       -
*
*/
void USBH_HID_SetOnKeyboardStateChange(USBH_HID_ON_KEYBOARD_FUNC * pfOnChange) {
  USBH_HID_INST * pInst;
  unsigned i;

  for (i = 0; i < USBH_COUNTOF(_aInst); i++) {
    pInst = &_aInst[i];
    pInst->pfOnKeyStateChange = pfOnChange;
  }
}

/*************************** EOF ************************************/
