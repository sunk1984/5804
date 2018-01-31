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
  USBH_TIMER_FUNC * pfHandler;
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
  // Convert Timeout to absolute value and store in in variables
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
USBH_TIMER_HANDLE USBH_AllocTimer(USBH_TIMER_FUNC * pfHandler, void * pContext) {
  TIMER * pTimer;
  pTimer = (TIMER *)USBH_MallocZeroed(sizeof(TIMER));
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
    USBH_ProcessISR(0);
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
  USBH_BD_PreInit();
  USBH_Global.Config.TransferBufferSize = 512;
  //
  //  ROOT HUB configuration
  //
#define HC_ROOTHUB_PORTS_ALWAYS_POWERED 0       // If set, ports are always powered on  when the Host Controller is powered on. The default value is 0.
                                                // If this define is set each port is powered individually. If this define is not set all ports powered on at the same time.
  USBH_Global.Config.RootHubPortsAlwaysPowered = HC_ROOTHUB_PORTS_ALWAYS_POWERED;
#define HC_ROOTHUB_PER_PORT_POWERED     1       // Not all host controller supports individually port switching. Because of this the default value is 0.
  USBH_Global.Config.RootHubPerPortPowered     = HC_ROOTHUB_PER_PORT_POWERED;
#define HC_ROOTHUB_OVERCURRENT          1       // This define can set to 1 if the hardware on the USB port detects an over current condition on the Vbus line.
                                                // If this define is set to 1 and the port status has an over current condition the port is automatically disabled.
  USBH_Global.Config.RootHubSupportOvercurrent = HC_ROOTHUB_OVERCURRENT;

// Validates root hub port defines.
#if HC_ROOTHUB_PORTS_ALWAYS_POWERED && HC_ROOTHUB_PER_PORT_POWERED
  #error HC_ROOTHUB_PORTS_ALWAYS_POWERED or HC_ROOTHUB_PER_PORT_POWERED is allowed
#endif


  //
  // USB host controller driver endpoint resources.
  // That are all endpoints that can be used at the same time.
  // The number of control endpoint is calculated from the number
  // of the USB devices and additional control endpoints that are
  // needed for the USB device enumeration. The following defines determine indirect
  // also additional bus master memory.

#define HC_DEVICE_MAX_USB_DEVICES       2 // Number connected USB devices
  USBH_Global.Config.NumUSBDevices    = HC_DEVICE_MAX_USB_DEVICES;
#define HC_DEVICE_INTERRUPT_ENDPOINTS   2 // Number of interrupt endpoints
  USBH_Global.Config.NumIntEndpoints  = HC_DEVICE_INTERRUPT_ENDPOINTS;
#define HC_DEVICE_BULK_ENDPOINTS        2 // Numbers of bulk endpoints
  USBH_Global.Config.NumBulkEndpoints = HC_DEVICE_BULK_ENDPOINTS;
#if USBH_SUPPORT_ISO_TRANSFER
  #define HC_DEVICE_ISO_ENDPOINTS         1 // Numbers of bulk endpoints
#else
  #define HC_DEVICE_ISO_ENDPOINTS         0 // Numbers of bulk endpoints
#endif
  USBH_Global.Config.NumIsoEndpoints = HC_DEVICE_ISO_ENDPOINTS;
#define USBH_EXTHUB_SUPPORT          1   // Set USBH_EXTHUB_SUPPORT to 1 if the UBD driver should support external hubs
  USBH_Global.Config.SupportExternalHubs = USBH_EXTHUB_SUPPORT;
  USBH_X_Config();
  USBH_LOG((USBH_MTYPE_INIT, "INIT: Initialize USB Bus Driver..."));
  USBH_BD_Init();

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
*       USBH_SearchNextDescriptor
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
const void * USBH_SearchNextDescriptor(const void * PrevDesc, int * Length, int DescType) {
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
*       USBH_BD_InitDefaultEndpoint
*
*  Function description
*    Initializes the embedded default endpoint object in the device
*    and creates an new default endpoint in the host controller driver.
*/
USBH_STATUS USBH_BD_InitDefaultEndpoint(USB_DEVICE * UsbDevice) {
  USBH_DEFAULT_EP           * ep;
  USBH_HOST_CONTROLLER      * pHostController;
  USBH_STATUS status =   USBH_STATUS_SUCCESS;

  USBH_ASSERT_MAGIC(UsbDevice, USB_DEVICE);
  ep = &UsbDevice->DefaultEp;
  // After allocation the device is set with zero values
  USBH_ASSERT(ep->hEP == NULL);
  IFDBG(ep->Magic = DEFAULT_EP_MAGIC);
  ep->pUsbDevice  = UsbDevice;
  ep->UrbCount   = 0;
  pHostController = UsbDevice->pHostController;
  ep->hEP   = pHostController->pDriver->pfAddEndpoint(pHostController->hHostController, USB_EP_TYPE_CONTROL, UsbDevice->UsbAddress, 0,
                                                         UsbDevice->MaxFifoSize,                               0, UsbDevice->DeviceSpeed);
  if (ep->hEP == NULL) {
    USBH_WARN((USBH_MTYPE_UBD, "UBD Error: USBH_BD_InitDefaultEndpoint: pfAddEndpoint failed"));
    status = USBH_STATUS_ERROR;
  }
  return status;
}

/*********************************************************************
*
*       USBH_BD_ReleaseDefaultEndpoint
*
*  Function description
*    Removes the default endpoint for the host controller
*/
void USBH_BD_ReleaseDefaultEndpoint(USBH_DEFAULT_EP * UsbEndpoint) {
  USBH_HOST_CONTROLLER * HostController = UsbEndpoint->pUsbDevice->pHostController;

  USBH_ASSERT_MAGIC(HostController, USBH_HOST_CONTROLLER);
  // An URB must have a reference and the device must not be deleted if the URB has the reference
  USBH_ASSERT(UsbEndpoint->UrbCount == 0);
  USBH_LOG((USBH_MTYPE_UBD, "UBD: USBH_BD_ReleaseDefaultEndpoint: urbcount: %u", UsbEndpoint->UrbCount));
  if (UsbEndpoint->hEP != NULL) {
    HC_INC_REF(HostController);
    HostController->pDriver->pfReleaseEndpoint(UsbEndpoint->hEP, USBH_DefaultReleaseEpCompletion, HostController);
  }
  UsbEndpoint->hEP = NULL;
}

/*********************************************************************
*
*       USBH_BD_DefaultEpUrbCompletion
*
*  Function description
*    URBs internal default ep completion routine
*/
void USBH_BD_DefaultEpUrbCompletion(USBH_URB * Urb) {
  USBH_DEFAULT_EP * UsbEndpoint = (USBH_DEFAULT_EP * )Urb->Header.pInternalContext;
  UsbEndpoint->UrbCount--;
  USBH_LOG((USBH_MTYPE_UBD, "UBD: USBH_BD_DefaultEpUrbCompletion: urbcount: %u", UsbEndpoint->UrbCount));
  if (Urb->Header.pfOnCompletion != NULL) {
    Urb->Header.pfOnCompletion(Urb); // Complete the URB
  }
  DEC_REF(UsbEndpoint->pUsbDevice);
}

/*********************************************************************
*
*       USBH_BD_DefaultEpSubmitUrb
*
*  Function description
*    DefaultEpSubmitUrb does normal submit the URB direct to the Host driver.
*    If the function returns USBH_STATUS_PENDING the completion routine is called.
*    On each other status code the completion routine is never called.
*/
USBH_STATUS USBH_BD_DefaultEpSubmitUrb(USB_DEVICE * Dev, USBH_URB * Urb) {
  USBH_STATUS       Status;
  USBH_DEFAULT_EP      * DefaultEndpoint;
  USBH_HOST_CONTROLLER * HostController;
  if (Dev == NULL) {
    return USBH_STATUS_INVALID_PARAM;
  }
  USBH_ASSERT_MAGIC(Dev, USB_DEVICE);
  DefaultEndpoint = &Dev->DefaultEp;
  HostController  = Dev->pHostController;
  DefaultEndpoint->UrbCount++;
  INC_REF(Dev);
  Status = HostController->pDriver->pfSubmitRequest(DefaultEndpoint->hEP, Urb);

  if (Status != USBH_STATUS_PENDING) {
    // Completion routine is never called in this case */
    USBH_WARN((USBH_MTYPE_UBD, "UBD Error: USBH_BD_DefaultEpSubmitUrb: %08x!", Status));
    Urb->Header.Status = Status;
    DefaultEndpoint->UrbCount--;
    USBH_LOG((USBH_MTYPE_UBD, "UBD: USBH_BD_DefaultEpSubmitUrb: urbcount: %u", DefaultEndpoint->UrbCount));
    DEC_REF(Dev);
  }
  return Status;
}

/*********************************************************************
*
*       USBH_BD_SubmitClearFeatureEndpointStall
*
*  Function description
*    The Urb is submitted if the function returns USBH_STATUS_PENDING
*/
USBH_STATUS USBH_BD_SubmitClearFeatureEndpointStall(USBH_DEFAULT_EP * DefaultEp, USBH_URB * Urb, U8 Endpoint, USBH_ON_COMPLETION_FUNC * InternalCompletion, void * UbdContext) {
  USB_DEVICE  * dev = DefaultEp->pUsbDevice;
  USBH_STATUS   status;

  USBH_ZERO_MEMORY(Urb, sizeof(USBH_URB));
  Urb->Header.pfOnInternalCompletion = InternalCompletion;
  Urb->Header.pInternalContext    = DefaultEp;
  Urb->Header.pUbdContext         = UbdContext;

  // Set clear feature endpoint stall request
  Urb->Header.Function                      = USBH_FUNCTION_CONTROL_REQUEST;
  Urb->Request.ControlRequest.Setup.Type    = USB_ENDPOINT_RECIPIENT; // STD, OUT, endpoint
  Urb->Request.ControlRequest.Setup.Request = USB_REQ_CLEAR_FEATURE;
  Urb->Request.ControlRequest.Setup.Value   = USB_FEATURE_STALL;
  Urb->Request.ControlRequest.Setup.Index   = (U16)Endpoint;
  DefaultEp->UrbCount++;
  INC_REF(dev);
  status = dev->pHostController->pDriver->pfSubmitRequest(DefaultEp->hEP, Urb);

  if (status != USBH_STATUS_PENDING) {
    DefaultEp->UrbCount--;
    USBH_WARN((USBH_MTYPE_UBD, "UBD Error: UbdProcessEnum:USBH_BD_SubmitClearFeatureEndpointStall failed %08x", status));
    DEC_REF(dev); // Delete the device
  }
  return status;
}

/*********************************************************************
*
*       USBH_BD_SubmitSetInterface
*
*  Function description
*    The Urb is submitted if the function returns USBH_STATUS_PENDING
*/
USBH_STATUS USBH_BD_SubmitSetInterface(USB_INTERFACE * UsbInterface, U16 Interface, U16 AlternateSetting, USBH_ON_COMPLETION_FUNC * Completion, USBH_URB * OriginalUrb) {
  USBH_STATUS   status;
  USBH_URB    * Urb;
  // Prepare a request for new interface
  Urb = (USBH_URB *)USBH_Malloc(sizeof(USBH_URB)); // URB must be allocated because of the asynchronous request

  if (!Urb) {
    USBH_WARN((USBH_MTYPE_UBD, "UBD Error: USBH_BD_SubmitSetInterface: USBH_Malloc!"));
    return USBH_STATUS_MEMORY;
  }
  USBH_ZERO_MEMORY(Urb, sizeof(USBH_URB));
  Urb->Header.pfOnInternalCompletion = Completion;
  Urb->Header.pInternalContext    = UsbInterface;
  Urb->Header.pUbdContext         = OriginalUrb;

  // Set clear feature endpoint stall request
  Urb->Header.Function                      = USBH_FUNCTION_CONTROL_REQUEST;
  Urb->Request.ControlRequest.Setup.Type    = USB_INTERFACE_RECIPIENT; // STD, OUT, interface
  Urb->Request.ControlRequest.Setup.Request = USB_REQ_SET_INTERFACE;
  Urb->Request.ControlRequest.Setup.Value   = AlternateSetting;
  Urb->Request.ControlRequest.Setup.Index   = Interface;

  UsbInterface->pDevice->DefaultEp.UrbCount++;
  INC_REF(UsbInterface->pDevice);
  status = UsbInterface->pDevice->pHostController->pDriver->pfSubmitRequest(UsbInterface->pDevice->DefaultEp.hEP, Urb);

  if (status != USBH_STATUS_PENDING) {
    USBH_WARN((USBH_MTYPE_UBD, "UBD Error: UbdProcessEnum:USBH_BD_SubmitSetInterface failed %08x", status));
    // Delete the device
    UsbInterface->pDevice->DefaultEp.UrbCount--;
    USBH_LOG((USBH_MTYPE_UBD, "UBD: USBH_BD_SubmitSetInterface: urbcount: %u", UsbInterface->pDevice->DefaultEp.UrbCount));
    DEC_REF(UsbInterface->pDevice);
  }
  return status;
}

/*********************************************************************
*
*       USBH_ResetEndpoint
*
*  Function description
*/
USBH_STATUS USBH_ResetEndpoint(USBH_INTERFACE_HANDLE IfaceHandle, USBH_URB * urb, U8 Endpoint, USBH_ON_COMPLETION_FUNC Completion, void * Context) {
  USBH_STATUS status;
  // Allocate an URB
  USBH_ASSERT_PTR(Completion); // Completion routine is always needed
  urb->Header.pContext                   = Context;
  urb->Header.Function                  = USBH_FUNCTION_RESET_ENDPOINT;
  urb->Header.pfOnCompletion                = Completion;
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

/*********************************************************************
*
*       USBH_ConfigRootHub
*
*  Function description:
*
*
*  Parameters:
*    SupportOvercurrent    -
*    PortsAlwaysPowered    -
*    PerPortPowered    -
*
*/
void USBH_ConfigRootHub(U8 SupportOvercurrent, U8 PortsAlwaysPowered, U8 PerPortPowered) {
  if (PortsAlwaysPowered && PerPortPowered) {
    USBH_Panic("Setting PortsAlwaysPowered and PerPortPowered simultaneously is not allowed");
  }
  USBH_Global.Config.RootHubSupportOvercurrent = SupportOvercurrent;
  USBH_Global.Config.RootHubPortsAlwaysPowered = PortsAlwaysPowered;
  USBH_Global.Config.RootHubPerPortPowered     = PerPortPowered;
}

/*********************************************************************
*
*       USBH_ConfigMaxUSBDevices
*
*  Function description:
*    Configures how many individual endpoints shall be reserved.
*    In normal cases this is only necessary for controllers which
*    a limited access CPU memory range and store the USB transfer/endpoint information
*    in this shared memory.
*    In this case we need to configure the number of endpoints accordingly.
*
*  Parameters:
*    NumDevices    -
*
*/
void USBH_ConfigMaxUSBDevices(U8 NumDevices) {
  USBH_Global.Config.NumUSBDevices = NumDevices;
}

/*********************************************************************
*
*       USBH_ConfigMaxNumEndpoints
*
*  Function description:
*    Configures how many individual endpoints shall be reserved.
*    In normal cases this is only necessary for controllers which
*    a limited access CPU memory range and store the USB transfer/endpoint information
*    in this shared memory.
*    In this case we need to configure the number of endpoints accordingly.
*
*  Parameters:
*    MaxNumBulkEndpoints    -
*    MaxNumIntEndpoints    -
*    MaxNumIsoEndpoints    -
*
*/
void USBH_ConfigMaxNumEndpoints(U8 MaxNumBulkEndpoints, U8 MaxNumIntEndpoints, U8 MaxNumIsoEndpoints) {
  USBH_Global.Config.NumBulkEndpoints = MaxNumBulkEndpoints;
  USBH_Global.Config.NumIntEndpoints  = MaxNumIntEndpoints;
  USBH_Global.Config.NumIsoEndpoints  = MaxNumIsoEndpoints;
}


/*********************************************************************
*
*       USBH_ConfigSupportExternalHubs
*
*  Function description:
*    Shall external USB hubs be supported
*
*  Parameters:
*    OnOff    -
*
*/
void USBH_ConfigSupportExternalHubs(U8 OnOff) {
  USBH_Global.Config.SupportExternalHubs = OnOff;
}

/*********************************************************************
*
*       USBH_DLIST_Init
*
*  Function description:
*    Initializes a USBH_DLIST element. The link pointers
*    points to the structure itself. This element represents
*    an empty USBH_DLIST.
*    Each list head has to be initialized by this function.
*
*  Parameters:
*    ListHead    - Pointer to a structure of type USBH_DLIST.
*
*  Return value:
*    void       -
*
*/
void USBH_DLIST_Init(USBH_DLIST * ListHead) {
  (ListHead)->pNext = (ListHead)->pPrev = (ListHead);
}

/*********************************************************************
*
*       USBH_DLIST_IsEmpty
*
*  Function description:
*    Checks whether the list is empty.
*
*  Parameters:
*    ListHead    - Pointer to the list head.
*
*  Return value:
*    1 (TRUE)  - if the list is empty
*    0 (FALSE) - otherwise.
*
*/
int USBH_DLIST_IsEmpty(USBH_DLIST * ListHead) {
  return ((ListHead)->pNext == (ListHead));
}

/*********************************************************************
*
*       USBH_DLIST_GetNext
*
*  Function description:
*    Returns a pointer to the successor.
*
*  Parameters:
*    Entry    - Pointer to a list entry.
*
*  Return value:
*    Pointer to the successor of Entry.
*
*/
USBH_DLIST * USBH_DLIST_GetNext(USBH_DLIST * Entry) {
  return ((Entry)->pNext);
}

// USBH_DLIST * USBH_DLIST_GetPrev(USBH_DLIST * Entry);

// @func USBH_DLIST *<spc>| USBH_DLIST_GetPrev |
//   <f USBH_DLIST_GetPrev>
// @parm IN USBH_DLIST *<spc>| Entry |
//
// @rdesc
//   .
/*********************************************************************
*
*       USBH_DLIST_GetPrev
*
*  Function description:
*    Returns a pointer to the predecessor.
*
*  Parameters:
*    Entry    - Pointer to a list entry.
*
*  Return value:
*    Pointer to the predecessor of Entry.
*
*/
USBH_DLIST * USBH_DLIST_GetPrev(USBH_DLIST * Entry) {
  return ((Entry)->pPrev);
}

/*********************************************************************
*
*       USBH_DLIST_RemoveEntry
*
*  Function description:
*    Detaches one element from the list.
*    Calling this function on an empty list results in undefined behavior.
*
*  Parameters:
*    Entry    - Pointer to the element to be detached.
*
*/
void USBH_DLIST_RemoveEntry(USBH_DLIST * Entry) {
  USBH_DLIST * dlist_m_Entry = (Entry);

  dlist_m_Entry->pPrev->pNext = dlist_m_Entry->pNext;
  dlist_m_Entry->pNext->pPrev = dlist_m_Entry->pPrev;
  dlist_m_Entry->pNext        = dlist_m_Entry->pPrev = dlist_m_Entry;
}


/*********************************************************************
*
*       USBH_DLIST_RemoveHead
*
*  Function description:
*    Detaches the first element from the list
*    Calling this function on an empty list results in undefined behavior.
*
*  Parameters:
*    ListHead - Pointer to the list head.
*    Entry    - Address of a pointer to the detached element.
*
*  Return value:
*    void       -
*
*/
void USBH_DLIST_RemoveHead(USBH_DLIST * ListHead, USBH_DLIST ** Entry) {
  *(Entry) = (ListHead)->pNext;
  USBH_DLIST_RemoveEntry(*(Entry));
}

/*********************************************************************
*
*       USBH_DLIST_RemoveTail
*
*  Function description:
*    Detaches the last element from the list.
*    Calling this function on an empty list results in undefined behavior.
*
*  Parameters:
*    ListHead - Pointer to the list head.
*    Entry    - Address of a pointer to the detached element.
*
*  Return value:
*    void       -
*
*/
void USBH_DLIST_RemoveTail(USBH_DLIST * ListHead, USBH_DLIST ** Entry) {
  *(Entry) = (ListHead)->pPrev;
  USBH_DLIST_RemoveEntry(*(Entry));
}

/*********************************************************************
*
*       USBH_DLIST_InsertEntry
*
*  Function description:
*    Inserts an element into a list.
*    NewEntry is inserted after Entry,
*    i. e. NewEntry becomes the successor of Entry.
*
*  Parameters:
*    Entry    - Pointer to the element after which the new entry is to be inserted.
*    NewEntry - Pointer to the element to be inserted.
*
*/
void USBH_DLIST_InsertEntry(USBH_DLIST * Entry, USBH_DLIST * NewEntry) {
  USBH_DLIST * dlist_m_Entry               = (Entry);
  USBH_DLIST * dlist_m_NewEntry            = (NewEntry);

  dlist_m_NewEntry->pNext     = dlist_m_Entry->pNext;
  dlist_m_NewEntry->pPrev     = dlist_m_Entry;
  dlist_m_Entry->pNext->pPrev = dlist_m_NewEntry;
  dlist_m_Entry->pNext        = dlist_m_NewEntry;
}

/*********************************************************************
*
*       USBH_DLIST_InsertHead
*
*  Function description:
*    Inserts an element at the beginning of a list.
*    Entry becomes the first list entry.
*
*  Parameters:
*    ListHead - Pointer to the list head.
*    Entry    - Pointer to the element to be inserted.
*
*/
void USBH_DLIST_InsertHead(USBH_DLIST * ListHead, USBH_DLIST * Entry) {
  USBH_DLIST_InsertEntry(ListHead,Entry);
}

/*********************************************************************
*
*       USBH_DLIST_InsertTail
*
*  Function description:
*    Inserts an element at the end of a list.
*    Entry becomes the last list entry.
*
*  Parameters:
*    ListHead - Pointer to the list head.
*    Entry    - Pointer to the element to be inserted.
*
*/
void USBH_DLIST_InsertTail(USBH_DLIST * ListHead, USBH_DLIST * Entry) {
  USBH_DLIST_InsertEntry((ListHead)->pPrev,Entry);
}

/*********************************************************************
*
*       USBH_DLIST_Append
*
*  Function description:
*    Concatenates two lists.
*    The first element of List becomes the successor
*    of the last element of ListHead.
*
*  Parameters:
*    ListHead - Pointer to the list head of the first list.
*    List     - Pointer to the list head of the second list.
*
*/
void USBH_DLIST_Append(USBH_DLIST * ListHead, USBH_DLIST * List) {
  USBH_DLIST * dlist_m_List       = (List);
  USBH_DLIST * dlist_m_Tail       = (ListHead)->pPrev;
  dlist_m_Tail->pNext        = dlist_m_List;
  dlist_m_List->pPrev->pNext = (ListHead);
  (ListHead)->pPrev          = dlist_m_List->pPrev;
  dlist_m_List->pPrev        = dlist_m_Tail;
}


/*********************************************************************
*
*       USBH_DLIST_Remove
*
*  Function Description
*    Removes a USBH_DLIST element from the list
*/
void USBH_DLIST_Remove(USBH_DLIST_HEAD * pHead, USBH_DLIST_ITEM * pItem) {
  //
  // Unlink Front: From head or previous block
  //
  if (pItem == pHead->pFirst) {           // This item first in list ?
    pHead->pFirst = pItem->pNext;
  } else {
    pItem->pPrev->pNext = pItem->pNext;
  }
  //
  // Unlink next if pNext is valid
  //
  if (pItem->pNext) {
    pItem->pNext->pPrev = pItem->pPrev;
  }
}

/*********************************************************************
*
*       USBH_DLIST_Add
*
*  Function Description
*    Adds a USBH_DLIST element to the front of the list, making it the first element
*/
void USBH_DLIST_Add(USBH_DLIST_HEAD * pHead, USBH_DLIST_ITEM * pNew) {
  //
  // Prepare new item to be first
  //
  pNew->pPrev = NULL;    // First entry
  pNew->pNext = pHead->pFirst;
  //
  // If there was already an item in the list, modify it by making its pPrev point to the new item
  //
  if (pHead->pFirst) {
    pHead->pFirst->pPrev = pNew;
  }
  //
  // Make head pointer point to new item
  //
  pHead->pFirst = pNew;
}

/*********************************************************************
*
*       USBH_ServiceISR
*
*  Function description
*/
void USBH_ServiceISR(unsigned Index) {
  int succ;

  USBH_USE_PARA(Index);
  USBH_ASSERT_PTR(USBH_Global.pDriver);
  USBH_ASSERT_PTR(USBH_Global.pDriver->pfCheckIsr);
  succ = (USBH_Global.pDriver->pfCheckIsr)(USBH_Global.hHC);
  if(succ){
    USBH_OnISREvent();
  }
}

/*********************************************************************
*
*       USBH_ServiceISR
*
*  Function description
*/
void USBH_ProcessISR(unsigned Index) {
  USBH_USE_PARA(Index);
  USBH_ASSERT_PTR(USBH_Global.pDriver);
  USBH_ASSERT_PTR(USBH_Global.pDriver->pfCheckIsr);
  (USBH_Global.pDriver->pfIsr)(USBH_Global.hHC);
}

/*********************************************************************
*
*       USBH__ConvSetupPacketToBuffer
*
*  Function description
*    Converts the struct USBH_SETUP_PACKET to a byte buffer.
*    IN: Pointer to a empty struct - OUT: Setup
*    points to a byte buffer with a length of 8 bytes
*/
void USBH__ConvSetupPacketToBuffer(const USBH_SETUP_PACKET * Setup, U8 * pBuffer) {
  *pBuffer++ = Setup->Type;
  *pBuffer++ = Setup->Request;
  *pBuffer++ = (U8) Setup->Value;        //LSB
  *pBuffer++ = (U8)(Setup->Value  >> 8); //MSB
  *pBuffer++ = (U8) Setup->Index;        //LSB
  *pBuffer++ = (U8)(Setup->Index  >> 8); //MSB
  *pBuffer++ = (U8) Setup->Length;       //LSB
  *pBuffer++ = (U8)(Setup->Length >> 8); //MSB
}

/*********************************************************************
*
*       USBH_AllocIsoUrb
*
*  Function description
*    Creates an Iso-URB that is used in order to send ISO requests to
*    the device.
*/
USBH_URB * USBH_AllocIsoUrb(unsigned NumIsoPackets, unsigned NumBytesForBuffer) {
  USBH_URB * pUrb;
  unsigned   UrbSize;

  UrbSize = USBH_URB_GET_ISO_URB_SIZE(NumIsoPackets);
  pUrb = (USBH_URB *)USBH_MallocZeroed(UrbSize);
  pUrb->Request.IsoRequest.pBuffer = USBH_AllocTransferMemory(NumBytesForBuffer, 1);
  return pUrb;
}

/*********************************************************************
*
*       USBH_FreeIsoUrb
*
*  Function description
*    Frees an URB.
*/
void USBH_FreeIsoUrb(USBH_URB * pUrb) {
  USBH_Free(pUrb->Request.IsoRequest.pBuffer);
  USBH_Free(pUrb);
}


/********************************* EOF ******************************/

