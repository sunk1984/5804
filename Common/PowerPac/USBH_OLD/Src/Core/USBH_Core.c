/*********************************************************************
*  
*   IAR PowerPac
*
*   (c) Copyright IAR Systems 2010.  All rights reserved.
*
**********************************************************************
----------------------------------------------------------------------
File        : USBH_Core.c
Purpose     : USB host implementation
---------------------------END-OF-HEADER------------------------------
*/

/*********************************************************************
*
*       #include Section
*
**********************************************************************
*/
#define USBHCORE_C
#include <stdlib.h>
#include "USBH_Int.h"

/*********************************************************************
*
*       #define constants
*
**********************************************************************
*/
#define MAX_TIMERS 10

/*********************************************************************
*
*       Local data types
*
**********************************************************************
*/
typedef struct TIMER {
  struct TIMER          * pNext;
  struct TIMER          * pPrev;
  USBH_TIMER_CB_ROUTINE * pfHandler;
  void                  * pContext;
  I32                     TimeOfExpiration;
  U8                      IsActive;
  U32                     Timeout;
} TIMER;

/*********************************************************************
*
*       Static data
*
**********************************************************************
*/
static TIMER * _pFirstTimer;
static I32     _NextTimeout;
static U8      _TimerActive;

/*********************************************************************
*
*       Public data
*
**********************************************************************
*/

/*********************************************************************
*
*       #define function replacement
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
*       _RemoveFromList
*
*  Function Description
*    Removes a given work block from list of work blocks.
*/
static void _RemoveFromList(TIMER * pTimer, TIMER * * ppFirst) {
  if (pTimer == *ppFirst) {
    * ppFirst = pTimer->pNext;
  } else {
    pTimer->pPrev->pNext = pTimer->pNext;
    if (pTimer->pNext) {
      pTimer->pNext->pPrev = pTimer->pPrev;
    }
  }
}

/*********************************************************************
*
*       _AddToList
*
*  Function Description
*    Adds a given work block to the list of work blocks.
*/
static void _AddToList(TIMER * pTimer, TIMER * * ppFirst) {
  TIMER * pPrevFirst;
  pPrevFirst    = * ppFirst;
  pTimer->pPrev = NULL; // First entry
  pTimer->pNext = pPrevFirst;
  if (pPrevFirst) {
    pPrevFirst->pPrev = pTimer;
  }
  * ppFirst = pTimer;
}

/*********************************************************************
*
*       USBH_StartTimer
*
*  Function description
*    Starts a timer. The timer is restarted again if it is running.
*/
static void _StartTimer(USBH_TIMER_HANDLE hTimer, U32 ms) {
  TIMER * pTimer;
  I32     t;
  I32     t0;
  I32     t1;

  USBH_LOG((USBH_MTYPE_TIMER, "INIT: Starting timer 0x%x with timeout = %d ms", hTimer, ms));
  t                        = USBH_OS_GetTime32();
  pTimer                   = (TIMER *)hTimer;
  pTimer->IsActive         = 1;
  pTimer->TimeOfExpiration = t + ms;
  pTimer->Timeout          = ms;

  //
  // Check if this affects the expiration time of the next timer
  //
  if (_TimerActive == 0) {
    _NextTimeout = pTimer->TimeOfExpiration;
    USBH_OS_SignalNetEvent();   // Timeout change means we need to wake the task to make sure it does not sleep too long
  } else {
    t0 = pTimer->TimeOfExpiration - t;
    t1 = _NextTimeout - t;
    if (t0 < t1) {
      _NextTimeout = pTimer->TimeOfExpiration;
      USBH_OS_SignalNetEvent(); // Timeout change means we need to wake the task to make sure it does not sleep too long
    }
  }
  _TimerActive = 1;
}

/*********************************************************************
*
*       Helper functions
*
**********************************************************************
*/

/*********************************************************************
*
*       _UpdateTimeout
*
*  Function description
*    Compute timeout of next exiring timer
*    System lock is active, so we can simply iterate over the timers in the list
*/
static void _UpdateTimeout(void) {
  TIMER * pTimer;
  I32     Timeout;
  U8      IsActive;
  I32     t;
  I32     v;
  //
  // Compute next time out relative to current time
  //
  Timeout  = 0x7FFFFFFF;
  IsActive = 0;
  t        = USBH_OS_GetTime32();
  pTimer   = _pFirstTimer;
  while (pTimer) {
    if (pTimer->IsActive) {
      v = pTimer->TimeOfExpiration - t;
      if (v < Timeout) {
        Timeout  = v;
        IsActive = 1;
      }
    }
    pTimer = pTimer->pNext;
  }
  //
  // Convert Timeout to absolut value and store in in variables
  //
  if (Timeout == 0x7FFFFFFF) {
    t = 0;
  }
  _NextTimeout = Timeout + t;
  _TimerActive = IsActive;
}

/*********************************************************************
*
*       USBH_AllocTimer
*
*  Function description
*    Allocates memory for & initializes timer object
*/
USBH_TIMER_HANDLE USBH_AllocTimer(USBH_TIMER_CB_ROUTINE * pfHandler, void * pContext) {
  TIMER * pTimer;
  pTimer = USBH_MallocZeroed(sizeof(TIMER));
  if (pTimer == NULL) {
    return pTimer;
  }
  USBH_LOG((USBH_MTYPE_TIMER, "INIT: Allocating timer 0x%x", pTimer));
  pTimer->pfHandler = pfHandler;
  pTimer->pContext = pContext;
  //
  // Add timer to linked list
  //
  _AddToList(pTimer, &_pFirstTimer);
  return (USBH_TIMER_HANDLE)pTimer;
}

/*********************************************************************
*
*       USBH_FreeTimer
*
*  Function description
*    Frees a timer object via timer handle.
*/
void USBH_FreeTimer(USBH_TIMER_HANDLE hTimer) {
  TIMER * pTimer;
  USBH_CancelTimer(hTimer); // Should not be necessary, but to be on the safe side
  USBH_LOG((USBH_MTYPE_TIMER, "INIT: Freeing timer 0x%x", hTimer));
  USBH_OS_LockSys();
  //
  // Unlink
  //
  pTimer = (TIMER * )hTimer;
  _RemoveFromList(pTimer, &_pFirstTimer);
  USBH_OS_UnlockSys();
  USBH_Free(pTimer);
}

/*********************************************************************
*
*       USBH_StartTimer
*
*  Function description
*    Starts a timer. The timer is restarted again if it is running.
*/
void USBH_StartTimer(USBH_TIMER_HANDLE hTimer, U32 ms) {
  USBH_OS_LockSys();
  _StartTimer(hTimer, ms);
  USBH_OS_UnlockSys();
}

/*********************************************************************
*
*       USBH_CancelTimer
*
*  Function description
*    Cancels an timer if running, the completion routine is not called.
*/
void USBH_CancelTimer(USBH_TIMER_HANDLE hTimer) {
  TIMER * pTimer;
  USBH_LOG((USBH_MTYPE_TIMER, "INIT: Cancel timer 0x%x", hTimer));
  USBH_OS_LockSys();
  pTimer           = (TIMER * )hTimer;
  pTimer->IsActive = 0;
  USBH_OS_UnlockSys();
}

/*********************************************************************
*
*       USBH_Task
*
*  Function description
*/
void USBH_Task(void) {
  TIMER * pTimer;
  I32     t;
  USBH_LOG((USBH_MTYPE_INIT, "INIT: USBH_Task started"));
  while (1) {
    t = -1;

#if (defined (USBH_TRIAL))
  if (USBH_OS_GetTime32() < 15 * 60 * 1000)
#endif

    {
      //
      // If timeout is expired, call all expired timers & compute next timeout
      //
      USBH_OS_LockSys();
      pTimer = _pFirstTimer;
      if (_TimerActive && USBH_IsExpired(_NextTimeout)) {
        do {
          if (pTimer == NULL) {
            break;
          }
          if (pTimer->IsActive) {
            t = pTimer->TimeOfExpiration;
            if (USBH_IsExpired(t)) {
              pTimer->IsActive = 0;
              USBH_OS_UnlockSys();
              pTimer->pfHandler(pTimer->pContext);
              USBH_OS_LockSys();
            }
          }
          pTimer = pTimer->pNext;
        } while (1);
        _UpdateTimeout();
      }
      //
      // Pause as for as long as no timer expires
      //
      USBH_OS_UnlockSys();
      t = _NextTimeout - USBH_OS_GetTime32();
      if (t < 0) {
        t = 0;
      }
      USBH_OS_WaitNetEvent(t);
    }
  }
}

/*********************************************************************
*
*       USBH_ISRTask
*
*  Function description
*    Main thread for starting the net. After startup, it settles into
*    a loop handling received packets. This loop sleeps until a packet
*    has been queued in USBH_Global.RxPacketQ; at which time it should be awakend by the
*    driver which queued the packet.
*/
void USBH_ISRTask(void) {
  USBH_LOG((USBH_MTYPE_INIT, "INIT: USBH_ISRTask started"));
  while (1) {
    USBH_OS_WaitISR();
    USBH_OHC_ProcessInterrupt(USBH_Global.hHC);
  }
}


/*********************************************************************
*
*       USBH_OnISREvent
*
*  Function description
*    Typically called from within ISR
*/
void USBH_OnISREvent(void) {
  USBH_OS_SignalISR();
}

/*********************************************************************
*
*       USBH_IsExpired()
*
*  Notes
*    (1) How to compare
*        Important: We need to check the sign of the difference.
*        Simple comparison will fail when 0x7fffffff and 0x80000000 is compared.
*/
char USBH_IsExpired(I32 Time) {
  I32 t;
  t = (I32)USBH_OS_GetTime32();
  if ((t - Time) >= 0) { // (Note 1)
    return 1;
  }
  return 0;
}


/*********************************************************************
*
*       USBH_GetVersion
*
*  Function description
*    Returns the version of the stack.
*    Format: Mmmrr. Sample 10201 is 1.02a
*/
int USBH_GetVersion(void) {
  return USBH_VERSION;
}

/*********************************************************************
*
*       USBH_Init
*
*  Function description
*/
void USBH_Init(void) {
  USBH_LOG((USBH_MTYPE_INIT, "INIT: Init started. Version %d.%2d.%2d", USBH_VERSION / 10000, (USBH_VERSION / 100) % 100, USBH_VERSION % 100));

#if USBH_DEBUG > 0
  if (USBH_GetVersion() != USBH_VERSION) {
    USBH_PANIC("Version stamps in Code and header does not match");
  }
#endif

  USBH_OS_Init();
  USBH_UBD_PreInit();
  USBH_Global.Config.TransferBufferSize = 256;
  USBH_X_Config();
  USBH_LOG((USBH_MTYPE_INIT, "INIT: Initialize USB Bus Driver..."));
  USBH_UBD_Init();

#if USBH_DEBUG > 0
  USBH_Global.ConfigCompleted = 1;
#endif

  USBH_Global.InitCompleted = 1;
  USBH_LOG((USBH_MTYPE_INIT, "INIT: Init completed"));
  USBH_LOG((USBH_MTYPE_INIT, "INIT: Allow enumeration of devices"));
  USBH_EnumerateDevices(USBH_Global.hHCBD);
  USBH_LOG((USBH_MTYPE_INIT, "INIT: Enumeration of devices enabled"));
}

/*********************************************************************
*
*       Configuration descriptor enumeration functions
*
*  The configuration descriptor consists of the following descriptors:
*  1. Configuration descriptor and then the Interface descriptor
*  After the interface descriptor follow none, one or more endpoint descriptors
*  The configuration can have more than one interface
*
**********************************************************************
*/

/*********************************************************************
*
*       TGetNextDescriptor
*
*  Function description
*    Returns a pointer to the next descriptor or NULL if the length is to small
*
*  Parameters:
*    Desc:   Pointer to a descriptor
*    length: IN:  Remaining bytes from Desc
*            OUT: Remaining bytes from the returned descriptor
*/
static const void * TGetNextDescriptor(const void * Desc, int * length) {
  U8 descLen;
  USBH_LOG((USBH_MTYPE_CORE, "TGetNextDescriptor"));
  descLen = * (U8 *)Desc;
  if (descLen > * length) {
    return NULL;
  }
  * length -= descLen;
  if (* length <= 0) {
    // Not more descriptors
    USBH_LOG((USBH_MTYPE_CORE, "TGetNextDescriptor:end of descriptors!"));
    return NULL;
  }
  return ((U8 *)Desc) + descLen; // Returns the next descriptor
}

/*********************************************************************
*
*       TSearchNextDescriptor
*
*  Function description
*    Returns the pointer to the beginning of the descriptor or NULL if not Desc. is found
*
*  Parameters:
*    PrevDesc: Pointer to a descriptor
*    Length:   IN: remaining bytes from Desc.
*              OUT: if the descriptor is found then that is the remaining length from the beginning of the returned descriptor
*    DescType: Descriptor type, see USB spec.h
*/
const void * TSearchNextDescriptor(const void * PrevDesc, int * Length, int DescType) {
  int          len  = * Length;
  const void * Desc =   PrevDesc;

  for (; ;) {
    Desc = TGetNextDescriptor(Desc, &len);
    if (Desc == NULL) {
      break;
    }
    // Check type
    if (* ((U8 *)Desc + 1) == DescType) {
      * Length = len;
      break;
    }
  }
  return Desc;
}

/*********************************************************************
*
*       UbdInitDefaultEndpoint
*
*  Function description
*    Initializes the embedded default endpoint object in the device
*    and creates an new default endpoint in the host controller driver.
*/
USBH_STATUS UbdInitDefaultEndpoint(USB_DEVICE * UsbDevice) {
  DEFAULT_EP           * ep;
  HOST_CONTROLLER      * HostController;
  USBH_STATUS status =   USBH_STATUS_SUCCESS;

  T_ASSERT_MAGIC(UsbDevice, USB_DEVICE);
  ep = &UsbDevice->DefaultEp;
  // After allocation the device is set with zero values
  T_ASSERT(ep->EpHandle == NULL);

#if (USBH_DEBUG > 1)
  ep->Magic = DEFAULT_EP_MAGIC;
#endif

  ep->UsbDevice  = UsbDevice;
  ep->UrbCount   = 0;
  HostController = UsbDevice->HostController;
  ep->EpHandle   = HostController->HostEntry.AddEndpoint(HostController->HostEntry.HcHandle, USB_EP_TYPE_CONTROL, UsbDevice->UsbAddress, 0,
                                                         UsbDevice->MaxFifoSize,                               0, UsbDevice->DeviceSpeed);
  if (ep->EpHandle == NULL) {
    USBH_WARN((USBH_MTYPE_UBD, "UBD Error: UbdInitDefaultEndpoint: AddEndpoint failed"));
    status = USBH_STATUS_ERROR;
  }
  return status;
}

/*********************************************************************
*
*       UbdReleaseDefaultEndpoint
*
*  Function description
*    Removes the default endpoint for the host controller
*/
void UbdReleaseDefaultEndpoint(DEFAULT_EP * UsbEndpoint) {
  HOST_CONTROLLER * HostController = UsbEndpoint->UsbDevice->HostController;
  T_ASSERT_MAGIC(HostController, HOST_CONTROLLER);
  // An URB must have a reference and the device must not be deleted if the URB has the reference
  T_ASSERT(UsbEndpoint->UrbCount == 0);
  USBH_LOG((USBH_MTYPE_UBD, "UBD: UbdReleaseDefaultEndpoint: urbcount: %u", UsbEndpoint->UrbCount));
  if (UsbEndpoint->EpHandle != NULL) {
    HC_INC_REF(HostController);
    HostController->HostEntry.ReleaseEndpoint(UsbEndpoint->EpHandle, UbdDefaultReleaseEpCompletion, HostController);
  }
  UsbEndpoint->EpHandle = NULL;
}

/*********************************************************************
*
*       UbdDefaultEpUrbCompletion
*
*  Function description
*    URBs internal default ep completion routine
*/
void UbdDefaultEpUrbCompletion(URB * Urb) {
  DEFAULT_EP * UsbEndpoint = (DEFAULT_EP * )Urb->Header.InternalContext;
  UsbEndpoint->UrbCount--;
  USBH_LOG((USBH_MTYPE_UBD, "UBD: UbdDefaultEpUrbCompletion: urbcount: %u", UsbEndpoint->UrbCount));
  if (Urb->Header.Completion != NULL) {
    Urb->Header.Completion(Urb); // Complete the URB
  }
  DEC_REF(UsbEndpoint->UsbDevice);
}

/*********************************************************************
*
*       UbdDefaultEpSubmitUrb
*
*  Function description
*    DefaultEpSubmitUrb does normal submit the URB direct to the Host driver.
*    If the function returns USBH_STATUS_PENDING the completion routine is called.
*    On each other status code the completion routine is never called.
*/
USBH_STATUS UbdDefaultEpSubmitUrb(USB_DEVICE * Dev, URB * Urb) {
  USBH_STATUS       Status;
  DEFAULT_EP      * DefaultEndpoint;
  HOST_CONTROLLER * HostController;
  if (Dev == NULL) {
    return USBH_STATUS_INVALID_PARAM;
  }
  T_ASSERT_MAGIC(Dev, USB_DEVICE);
  DefaultEndpoint = &Dev->DefaultEp;
  HostController  = Dev->HostController;
  DefaultEndpoint->UrbCount++;
  INC_REF(Dev);
  Status = HostController->HostEntry.SubmitRequest(DefaultEndpoint->EpHandle, Urb);

  if (Status != USBH_STATUS_PENDING) {
    // Completion routine is never called in this case */
    USBH_WARN((USBH_MTYPE_UBD, "UBD Error: UbdDefaultEpSubmitUrb: %08x!", Status));
    Urb->Header.Status = Status;
    DefaultEndpoint->UrbCount--;
    USBH_LOG((USBH_MTYPE_UBD, "UBD: UbdDefaultEpSubmitUrb: urbcount: %u", DefaultEndpoint->UrbCount));
    DEC_REF(Dev);
  }
  return Status;
}

/*********************************************************************
*
*       UbdSubmitClearFeatureEndpointStall
*
*  Function description
*    The Urb is submitted if the function returns USBH_STATUS_PENDING
*/
USBH_STATUS UbdSubmitClearFeatureEndpointStall(DEFAULT_EP * DefaultEp, URB * Urb, U8 Endpoint, USBH_ON_COMPLETION_FUNC * InternalCompletion, void * UbdContext) {
  USB_DEVICE  * dev = DefaultEp->UsbDevice;
  USBH_STATUS   status;

  ZERO_MEMORY(Urb, sizeof(URB));
  Urb->Header.InternalCompletion = InternalCompletion;
  Urb->Header.InternalContext    = DefaultEp;
  Urb->Header.UbdContext         = UbdContext;

  // Set clear feature endpoint stall request
  Urb->Header.Function                      = USBH_FUNCTION_CONTROL_REQUEST;
  Urb->Request.ControlRequest.Setup.Type    = USB_ENDPOINT_RECIPIENT; // STD, OUT, endpoint
  Urb->Request.ControlRequest.Setup.Request = USB_REQ_CLEAR_FEATURE;
  Urb->Request.ControlRequest.Setup.Value   = USB_FEATURE_STALL;
  Urb->Request.ControlRequest.Setup.Index   = (U16)Endpoint;
  DefaultEp->UrbCount++;
  INC_REF(dev);
  status = dev->HostController->HostEntry.SubmitRequest(DefaultEp->EpHandle, Urb);

  if (status != USBH_STATUS_PENDING) {
    DefaultEp->UrbCount--;
    USBH_WARN((USBH_MTYPE_UBD, "UBD Error: UbdProcessEnum:UbdSubmitClearFeatureEndpointStall failed %08x", status));
    DEC_REF(dev); // Delete the device
  }
  return status;
}

/*********************************************************************
*
*       UbdSubmitSetInterface
*
*  Function description
*    The Urb is submitted if the function returns USBH_STATUS_PENDING
*/
USBH_STATUS UbdSubmitSetInterface(USB_INTERFACE * UsbInterface, U16 Interface, U16 AlternateSetting, USBH_ON_COMPLETION_FUNC * Completion, URB * OriginalUrb) {
  USBH_STATUS   status;
  URB         * Urb;
  // Prepare a request for new interface
  Urb = USBH_Malloc(sizeof(URB)); // URB must be allocated because of the asynchronous request

  if (!Urb) {
    USBH_WARN((USBH_MTYPE_UBD, "UBD Error: UbdSubmitSetInterface: USBH_Malloc!"));
    return USBH_STATUS_MEMORY;
  }
  ZERO_MEMORY(Urb, sizeof(URB));
  Urb->Header.InternalCompletion = Completion;
  Urb->Header.InternalContext    = UsbInterface;
  Urb->Header.UbdContext         = OriginalUrb;

  // Set clear feature endpoint stall request
  Urb->Header.Function                      = USBH_FUNCTION_CONTROL_REQUEST;
  Urb->Request.ControlRequest.Setup.Type    = USB_INTERFACE_RECIPIENT; // STD, OUT, interface
  Urb->Request.ControlRequest.Setup.Request = USB_REQ_SET_INTERFACE;
  Urb->Request.ControlRequest.Setup.Value   = AlternateSetting;
  Urb->Request.ControlRequest.Setup.Index   = Interface;

  UsbInterface->Device->DefaultEp.UrbCount++;
  INC_REF(UsbInterface->Device);
  status = UsbInterface->Device->HostController->HostEntry.SubmitRequest(UsbInterface->Device->DefaultEp.EpHandle, Urb);

  if (status != USBH_STATUS_PENDING) {
    USBH_WARN((USBH_MTYPE_UBD, "UBD Error: UbdProcessEnum:UbdSubmitSetInterface failed %08x", status));
    // Delete the device
    UsbInterface->Device->DefaultEp.UrbCount--;
    USBH_LOG((USBH_MTYPE_UBD, "UBD: UbdSubmitSetInterface: urbcount: %u", UsbInterface->Device->DefaultEp.UrbCount));
    DEC_REF(UsbInterface->Device);
  }
  return status;
}

/*********************************************************************
*
*       USBH_ResetEndpoint
*
*  Function description
*/
USBH_STATUS USBH_ResetEndpoint(USBH_INTERFACE_HANDLE IfaceHandle, URB * urb, U8 Endpoint, USBH_ON_COMPLETION_FUNC Completion, void * Context) {
  USBH_STATUS status;
  // Allocate an URB
  T_ASSERT_PTR(Completion); // Completion routine is always needed
  urb->Header.Context                   = Context;
  urb->Header.Function                  = USBH_FUNCTION_RESET_ENDPOINT;
  urb->Header.Completion                = Completion;
  urb->Request.EndpointRequest.Endpoint = Endpoint;
  status                                = USBH_SubmitUrb(IfaceHandle, urb);
  if (status != USBH_STATUS_PENDING) {
    USBH_WARN((USBH_MTYPE_CORE, "ERROR ResetEndpoint: USBH_SubmitUrb(0x%x)!", status)); // On error
  }
  return status;
}

/*********************************************************************
*
*       USBH_ReadReg8
*
*  Function description
*/
U8 USBH_ReadReg8(U8 * pAddr) {
  U8     r;
  r  = *(pAddr);
  return r;
}

/*********************************************************************
*
*       USBH_ReadReg16
*
*  Function description
*/
U16 USBH_ReadReg16(U16 * pAddr) {
  U16     r;
  r   = * pAddr;
  return r;
}

/*********************************************************************
*
*       USBH_WriteReg32
*
*  Function description
*/
void USBH_WriteReg32(U8 * pAddr, U32 Value) {
  * ((U32 *)pAddr) = Value;
}

/*********************************************************************
*
*       USBH_ReadReg32
*
*  Function description
*/
U32 USBH_ReadReg32(U8 * pAddr) {
  U32 r;
  r = * ((U32 *)pAddr);
  return r;
}

/*********************************************************************
*
*       USBH_LoadU32LE
*
*  Function description
*/
U32 USBH_LoadU32LE(const U8 * pData) {
  U32 r;
  r  = * pData++;
  r |= * pData++ << 8;
  r |= (U32) * pData++ << 16;
  r |= (U32) * pData   << 24;
  return r;
}

/*********************************************************************
*
*       USBH_LoadU32BE
*
*  Function description
*/
U32 USBH_LoadU32BE(const U8 * pData) {
  U32 r;
  r = * pData++;
  r = (r << 8) | * pData++;
  r = (r << 8) | * pData++;
  r = (r << 8) | * pData;
  return r;
}

/*********************************************************************
*
*       USBH_LoadU32TE
*
*  Function description
*/
U32 USBH_LoadU32TE(const U8 * pData) {
  U32 v;
  U8 * p2 = (U8 *) &v;
  * p2++ = * pData++;
  * p2++ = * pData++;
  * p2++ = * pData++;
  * p2++ = * pData++;
  return v;
}

/*********************************************************************
*
*       USBH_LoadU16BE
*
*  Function description
*/
unsigned USBH_LoadU16BE(const U8 * pData) {
  unsigned r;
  r = * pData++;
  r = (r << 8) | * pData;
  return r;
}

/*********************************************************************
*
*       USBH_LoadU16LE
*
*  Function description
*/
unsigned USBH_LoadU16LE(const U8 * pData) {
  unsigned r;
  r  =  * pData++;
  r |= (* pData++ << 8);
  return r;
}

/*********************************************************************
*
*       USBH_StoreU32BE
*
*  Function description
*/
void USBH_StoreU32BE(U8 * p, U32 v) {
  *  p      = (U8)((v >> 24) & 255);
  * (p + 1) = (U8)((v >> 16) & 255);
  * (p + 2) = (U8)((v >> 8)  & 255);
  * (p + 3) = (U8)( v        & 255);
}

/*********************************************************************
*
*       USBH_StoreU32LE
*
*  Function description
*/
void USBH_StoreU32LE(U8 * p, U32 v) {
  * p++ = (U8)v;
  v >>= 8;
  * p++ = (U8)v;
  v >>= 8;
  * p++ = (U8)v;
  v >>= 8;
  * p   = (U8)v;
}

/*********************************************************************
*
*       USBH_StoreU16BE
*
*  Function description
*/
void USBH_StoreU16BE(U8 * p, unsigned v) {
  * (p + 0) = (U8)((v >> 8) & 255);
  * (p + 1) = (U8)( v       & 255);
}

/*********************************************************************
*
*       USBH_StoreU16LE
*
*  Function description
*/
void USBH_StoreU16LE(U8 * p, unsigned v) {
  * p++ = (U8)v;
  v >>= 8;
  * p   = (U8)v;
}

/*********************************************************************
*
*       USBH_SwapU32
*
*  Function description
*/
U32 USBH_SwapU32(U32 v) {
  U32 r;
  r =  ((v << 0)  >> 24) << 0;
  r |= ((v << 8)  >> 24) << 8;
  r |= ((v << 16) >> 24) << 16;
  r |= ((v << 24) >> 24) << 24;
  return r;
}

/*********************************************************************
*
*       USBH_ConfigTransferBufferSize
*
*  Function description
*    The size and the count of buffers used for the USBH_COPY_TRANSFER_BUFFER
*    option are defined these parameters. The parameter USBH_COPY_TRANSFER_BUFFER_SIZE
*    is important for the performance of the data transfer. A small buffer causes a
*    bad performance or a slow data rate. If a buffer size of 2048 is used the
*    transfer of one buffer takes approximately 2 milliseconds. The switching to the
*    next buffer takes typically 1 millisecond. In this case 2/3 of the bandwidth is
*    used. If a copy operation is required and all transfer buffer are in use an error
*    is returned. To avoid this make sure that number of
*    USBH_COPY_TRANSFER_BUFFER_BLOCKS are big enough. USBH_COPY_TRANSFER_BUFFER_SIZE
*    must be less than TAL_MAX_LENGTH_MEMORY_DESCRIPTOR_TABLE.
*/
void USBH_ConfigTransferBufferSize(U32 Size) {
  USBH_Global.Config.TransferBufferSize = Size;
}

/********************************* EOF ******************************/

