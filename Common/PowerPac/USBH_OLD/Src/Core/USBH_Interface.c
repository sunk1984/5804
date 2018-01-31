/*********************************************************************
*  
*   IAR PowerPac
*
*   (c) Copyright IAR Systems 2010.  All rights reserved.
*
**********************************************************************
----------------------------------------------------------------------
File        : USBH_Interface.c
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
*       UbdResetPipeCompletion
*
*  Function description:
*    Is used for URB requests where the default completion routine of
*    the default endpoint (DEFAULT_EP) object or from the USB endpoint
*    object (USB_ENDPOINT) object can not be used. The URBs internal
*    context and the Urb UbdContext contains additional information!
*/
static void UbdResetPipeCompletion(URB * Urb) {
  URB            * originalUrb;
  DEFAULT_EP     * defaultEndpoint = (DEFAULT_EP * )Urb->Header.InternalContext;
  USB_ENDPOINT   * usbEndpoint;
  USBH_STATUS       status;
  USB_DEVICE     * Device          = defaultEndpoint->UsbDevice;
  USB_HOST_ENTRY * HostEntry       = &Device->HostController->HostEntry;

  T_ASSERT(Urb->Header.Completion == NULL);         // The helpers URBs completion routine should always NULL
  defaultEndpoint->UrbCount--;                      // Decrement the count
  USBH_LOG((USBH_MTYPE_UBD, "UBD: UbdResetPipeCompletion: urbcount: %u",defaultEndpoint->UrbCount));
  originalUrb = (URB * )Urb->Header.UbdContext;     // Decrement the reference of the URB
  T_ASSERT(originalUrb->Header.UbdContext != NULL); // Get the endpoint
  usbEndpoint                = (USB_ENDPOINT * )originalUrb->Header.UbdContext;
  status                     = Urb->Header.Status;  // Transfer the status
  originalUrb->Header.Status = status;
  if (status == USBH_STATUS_SUCCESS) {
    status = HostEntry->ResetEndpoint(usbEndpoint->EpHandle);
  }
  originalUrb->Header.Status = status;
  if (originalUrb->Header.Completion != NULL) {
    originalUrb->Header.Completion(originalUrb);
  }
  USBH_Free(Urb);                                   // Delete the helper URB
  DEC_REF(Device);
}

/*********************************************************************
*
*       UbdResetEndpoint
*
*  Function description:
*    First submits an ClearFeatureEndpointStall control request with
*    a new created URB. The control request URB UbdContext points to
*    the original URB. In the original the HCContext is set to the URB
*    function USBH_FUNCTION_RESET_ENDPOINT. In the default endpoint
*    completion routine the control request URB is destroyed.
*
*  Parameters:
*    UsbEndpoint: Endpoint number with direction bit
*            Urb: Original URB
*
*  Return value:
*    USBH_STATUS_PENDING on success
*    other values on error
*/
static USBH_STATUS UbdResetEndpoint(USB_ENDPOINT * UsbEndpoint, URB * Urb) {
  USB_DEVICE  * usbDevice;
  URB         * ep0Urb;
  USBH_STATUS   status;
  if (UsbEndpoint == NULL) {
    Urb->Header.Status = USBH_STATUS_INVALID_PARAM;
    return USBH_STATUS_INVALID_PARAM;
  }
  usbDevice = UsbEndpoint->UsbInterface->Device;

  // The host controller layer stops submitting of new packets if an errors occurres on the bulk or interrupt endpoint.
  // New URBs are returned with USBH_STATUS_ENDPOINT_HALTED. Pending URBs are submitted at the end of the reset endpoint.

  //if (UsbEndpoint->UrbCount > 0 ) {
  //  Urb->Header.Status = USBH_STATUS_BUSY;
  //  return USBH_STATUS_BUSY;
  //}
  Urb->Header.UbdContext = UsbEndpoint;              // Store the UsbEndpoint pointer in the original URB
  ep0Urb                 = USBH_Malloc(sizeof(URB)); // The URB must be allocated because of the asynchronous request
  if (!ep0Urb) {
    USBH_WARN((USBH_MTYPE_UBD, "UBD: UbdResetEndpoint: USBH_malloc!"));
    return USBH_STATUS_MEMORY;
  }
  // Prepare and submit the URB, the control endpoint is never in Halt!
  status = UbdSubmitClearFeatureEndpointStall(&usbDevice->DefaultEp, ep0Urb, Urb->Request.EndpointRequest.Endpoint, UbdResetPipeCompletion, Urb);
  if (status != USBH_STATUS_PENDING) {
    USBH_WARN((USBH_MTYPE_UBD, "UBD: UbdResetEndpoint: status: 0x%lx!",status));
    Urb->Header.Status = status;
    USBH_Free(ep0Urb);
  }
  return status;
}

/*********************************************************************
*
*       UbdAbortEndpoint
*
*  Function description:
*/
static USBH_STATUS UbdAbortEndpoint(USB_ENDPOINT * UsbEndpoint, URB * Urb) {
  USBH_STATUS      status;
  USB_HOST_ENTRY * HostEntry;

#if (USBH_DEBUG > 1)
  Urb = Urb;
#endif

  if (UsbEndpoint == NULL) {
    Urb->Header.Status = USBH_STATUS_INVALID_PARAM;
    return USBH_STATUS_INVALID_PARAM;
  }
  HostEntry = &UsbEndpoint->UsbInterface->Device->HostController->HostEntry;
  status    = HostEntry->AbortEndpoint(UsbEndpoint->EpHandle);
  T_ASSERT(status != USBH_STATUS_PENDING);      // Do not return status pending and we do not call the completion routine
  return status;
}

/*********************************************************************
*
*       UbdSetInterfaceCompletion
*
*  Function description:
*/
static void UbdSetInterfaceCompletion(URB * Urb) {
  USB_INTERFACE * UsbInterface = (USB_INTERFACE *)Urb->Header.InternalContext;
  USB_DEVICE    * Device       = UsbInterface->Device;
  USBH_STATUS     status;
  URB           * originalUrb  = (URB *)Urb->Header.UbdContext;

  Device->DefaultEp.UrbCount--;                  // Decrement the count
  USBH_LOG((USBH_MTYPE_UBD, "UBD: UbdSetInterfaceCompletion: urbcount: %u", Device->DefaultEp.UrbCount));
  originalUrb = (URB * )Urb->Header.UbdContext;  // decrement the reference of the URB
  status      = Urb->Header.Status;
  if (status == USBH_STATUS_SUCCESS) {           // On error the old endpoint structure is valid
    UbdRemoveEndpoints(UsbInterface);            // Delete all endpoints
    UsbInterface->AlternateSettingDescriptor = UsbInterface->NewAlternateSettingDescriptor; // store new alternate setting
    UsbInterface->CurrentAlternateSetting = UsbInterface->NewAlternateSetting;
    status = UbdCreateEndpoints(UsbInterface);   // Add new endpoints
  }
  originalUrb->Header.Status = status;           // Update the status
  if (originalUrb->Header.Completion != NULL) {  // Call the completion
    originalUrb->Header.Completion(originalUrb);
  }
  USBH_Free(Urb);                                // Delete the helper URB
  DEC_REF(Device);
}

/*********************************************************************
*
*       UbdSetInterface
*
*  Function description:
*    Sets a new interface in the device. All endpoint handles associated with
*    the interface will be unbound and all pending requests will be cancelled.
*    If this request returns with success, new endpoint objects are available.
*
*  Parameters:
*    0. Check if the same interface is active
*    1. Check if the alternate setting available
*    2. Abort all data endpoint requests
*    3. Delete all interfaces
*    4. Create and add all interfaces
*
*  Return:
*    USBH_STATUS_PENDING on success
*    other values on error
*/
static USBH_STATUS UbdSetInterface(USB_INTERFACE * usbInterface, URB * Urb) {
  USBH_STATUS    status;
  U8             alternateSetting = Urb->Request.SetInterface.AlternateSetting;
  U8             Interface        = usbInterface->InterfaceDescriptor[2];
  unsigned int   PendingUrbs;
  USB_DEVICE   * Device           = usbInterface->Device;
  U8           * InterfaceDescriptor;

  if (alternateSetting == usbInterface->CurrentAlternateSetting) {                          // On the same alternate setting do nothing
    Urb->Header.Status = USBH_STATUS_SUCCESS;
    return USBH_STATUS_SUCCESS;
  }
  PendingUrbs = UbdGetPendingUrbCount(usbInterface);
  if (PendingUrbs > 0) {                                                                    // Check pending count
    Urb->Header.Status = USBH_STATUS_BUSY;
    return USBH_STATUS_BUSY;
  }
  InterfaceDescriptor = UbdGetNextInterfaceDesc(Device, NULL, Interface, alternateSetting); // Check if the new alternate setting is available
  if (InterfaceDescriptor == NULL) {
    Urb->Header.Status = USBH_STATUS_INVALID_PARAM;
    return USBH_STATUS_INVALID_PARAM;
  }
  usbInterface->NewAlternateSettingDescriptor = InterfaceDescriptor;
  usbInterface->NewAlternateSetting           = alternateSetting;
  // Prepare and submit the URB, the control endpoint is never in Halt!
  status                                      = UbdSubmitSetInterface(usbInterface, Interface, alternateSetting, UbdSetInterfaceCompletion, Urb);
  if (status != USBH_STATUS_PENDING) {
    Urb->Header.Status = status;
    USBH_LOG((USBH_MTYPE_UBD, "UBD: UbdSetInterfaceCompletion: urbcount: %u", Device->DefaultEp.UrbCount));
  }
  return status;
}

/*********************************************************************
*
*       UbdSetPowerState
*
*  Function description:
*/
static USBH_STATUS UbdSetPowerState(USB_INTERFACE * usbInterface, URB * Urb) {
  USBH_STATUS        Status      = USBH_STATUS_INVALID_PARAM;
  USB_DEVICE       * UsbDevice   = usbInterface->Device;
  HUB_PORT         * HubPort     = UsbDevice->ParentPort;
  USBH_POWER_STATE   PowerState;
  USB_HOST_ENTRY   * HostEntry   = &UsbDevice->HostController->HostEntry;
  PowerState                     = Urb->Request.SetPowerState.PowerState;
  if (HubPort->RootHub != NULL) { // This is a root hub
    switch (PowerState) {
      case USBH_NORMAL_POWER:
        HostEntry->SetPortSuspend(HostEntry->HcHandle, HubPort->HubPortNumber, USBH_PortPowerRunning);
        Status = USBH_STATUS_SUCCESS;
        break;
      case USBH_SUSPEND:
        HostEntry->SetPortSuspend(HostEntry->HcHandle, HubPort->HubPortNumber, USBH_PortPowerSuspend);
        Status = USBH_STATUS_SUCCESS;
        break;
      default:
        USBH_WARN((USBH_MTYPE_UBD, "UBD: UbdSetPowerState: invalid param"));
    }
  }
  if (HubPort->ExtHub != NULL) { // This is an external hub
    // ToDo: implement state machine
  }
  return Status;
}

/*********************************************************************
*
*       UbdSetConfiguration
*
*  Function description:
*    Set configuration
*/
static USBH_STATUS UbdSetConfiguration(USB_DEVICE * Dev, URB * Urb) {
  USBH_STATUS   Status = USBH_STATUS_SUCCESS;
  HUB_PORT    * HubPort;
  if (Urb->Request.SetConfiguration.ConfigurationDescriptorIndex == Dev->ConfigurationIndex) {
    Urb->Header.Status = USBH_STATUS_SUCCESS;              // This configuration is already set, do nothing, return success
  } else {                                                 // The configuration must be changed
    Dev->ParentPort->ConfigurationIndex = Urb->Request.SetConfiguration.ConfigurationDescriptorIndex;
    HubPort                             = Dev->ParentPort; // Make a local copy of the parent port, the link is cleared with UbdUdevMarkParentAndChildDevicesAsRemoved
    UbdUdevMarkParentAndChildDevicesAsRemoved(Dev);        // Delete the old instance of the device completely
    UbdSetPortState(HubPort, PORT_CONNECTED);              // The state connected causes the HUB to reset the device
    UbdHcServicePorts(Dev->HostController);                // Service all ports
  }
  return Status;
}

/*********************************************************************
*
*       UbdResetDevice
*
*  Function description:
*    On reset we mark this device as removed and create a new device. The reason is,
*    that under some circumstances the device may change the descriptors and the interface.
*    E.g. the DFU class requires this. So we have to enumerate a new device to handle this.
*/
static USBH_STATUS UbdResetDevice(USB_DEVICE * Dev) {
  HUB_PORT * HubPort;
  Dev->ParentPort->ConfigurationIndex = Dev->ConfigurationIndex;
  HubPort                             = Dev->ParentPort; // Make a local copy of the parent port, the link is cleared with UbdUdevMarkParentAndChildDevicesAsRemoved
  UbdUdevMarkParentAndChildDevicesAsRemoved(Dev);        // Delete the old instance of the device completely
  UbdSetPortState(HubPort, PORT_CONNECTED);              // The state connected causes the HUB to reset the device
  UbdHcServicePorts(Dev->HostController);                // Service all ports
  return USBH_STATUS_SUCCESS;
}

/*********************************************************************
*
*       USBH_SubmitUrb
*
*  Function description:
*    Interface function for all asynchronous requests. If the function returns
*    USBH_STATUS_PENDING the completion routine is called. On each other status code
*    the completion routine is never called. The storage of the URB must be provided
*    by the caller and must be permanent until the URB is returned by the completion
*    routine.
*/
USBH_STATUS USBH_SubmitUrb(USBH_INTERFACE_HANDLE Handle, URB * Urb) {
  USB_INTERFACE * usbInterface;
  USB_ENDPOINT  * usbEndpoint;
  USB_DEVICE    * usbDevice;
 
  USBH_STATUS     status = USBH_STATUS_INVALID_PARAM;
  USBH_LOG((USBH_MTYPE_URB, "URB: USBH_SubmitUrb: %s",UbdUrbFunctionStr(Urb->Header.Function)));
  usbInterface = (USB_INTERFACE *)Handle;
  T_ASSERT_MAGIC(usbInterface, USB_INTERFACE);
  T_ASSERT_PTR(Urb);
  Urb->Header.Status = USBH_STATUS_PENDING; // Default status
  usbDevice          = usbInterface->Device;
  if (usbDevice->State < DEV_STATE_WORKING) {
    return USBH_STATUS_DEVICE_REMOVED;
  }
  switch (Urb->Header.Function) {
    case USBH_FUNCTION_CONTROL_REQUEST:
      Urb->Header.InternalContext    = &usbDevice->DefaultEp;
      Urb->Header.InternalCompletion = UbdDefaultEpUrbCompletion;
      status                         = UbdDefaultEpSubmitUrb(usbInterface->Device, Urb);
      break;
    case USBH_FUNCTION_BULK_REQUEST:        // Fall trough
    case USBH_FUNCTION_INT_REQUEST:
      usbEndpoint                    = GET_EP_FROM_ADDRESS(usbInterface, Urb->Request.BulkIntRequest.Endpoint);
      Urb->Header.InternalContext    = usbEndpoint;
      Urb->Header.InternalCompletion = UbdEpUrbCompletion;
      status                         = UbdEpSubmitUrb(usbEndpoint, Urb);

#if (USBH_DEBUG > 1)
      if (status != USBH_STATUS_SUCCESS && status != USBH_STATUS_PENDING) {
        USBH_LOG((USBH_MTYPE_URB, "USBH_SubmitUrb: Error Ep:0x%x ", Urb->Request.BulkIntRequest.Endpoint));
      }
#endif

      break;
    case USBH_FUNCTION_ISO_REQUEST:
      usbEndpoint                    = GET_EP_FROM_ADDRESS(usbInterface, Urb->Request.IsoRequest.Endpoint);
      Urb->Header.InternalContext    = usbEndpoint;
      Urb->Header.InternalCompletion = UbdEpUrbCompletion;
      status                         = UbdEpSubmitUrb(usbEndpoint, Urb);
      break;
    case USBH_FUNCTION_RESET_ENDPOINT:
      usbEndpoint = GET_EP_FROM_ADDRESS(usbInterface, Urb->Request.EndpointRequest.Endpoint);
      status      = UbdResetEndpoint(usbEndpoint, Urb);
      break;
    case USBH_FUNCTION_ABORT_ENDPOINT:
      usbEndpoint = GET_EP_FROM_ADDRESS(usbInterface, Urb->Request.EndpointRequest.Endpoint);
      status      = UbdAbortEndpoint(usbEndpoint, Urb);
      break;
    case USBH_FUNCTION_SET_INTERFACE:
      status = UbdSetInterface(usbInterface, Urb);
      break;
    case USBH_FUNCTION_SET_POWER_STATE:
      status = UbdSetPowerState(usbInterface, Urb);
      break;
    case USBH_FUNCTION_SET_CONFIGURATION:
      status = UbdSetConfiguration(usbInterface->Device, Urb);
      break;
    case USBH_FUNCTION_RESET_DEVICE:
      status = UbdResetDevice(usbInterface->Device);
      break;
    default:
      USBH_WARN((USBH_MTYPE_URB, "URB: USBH_SubmitUrb: invalid URB function: %d!",Urb->Header.Function));
      status = USBH_STATUS_INVALID_PARAM;
      break;
  }

#if (USBH_DEBUG > 1)
  if (status != USBH_STATUS_SUCCESS && status != USBH_STATUS_PENDING) {
    USBH_LOG((USBH_MTYPE_URB, "USBH_SubmitUrb: %s status:%s ",UbdUrbFunctionStr(Urb->Header.Function),USBH_GetStatusStr(status)));
  }
#endif

  return status;
}

/*********************************************************************
*
*       USBH_GetInterfaceInfo
*
*  Function description:
*   Get information about a USB interface. This function may return USBH_STATUS_DEVICE_REMOVED
*/
USBH_STATUS USBH_GetInterfaceInfo(USBH_INTERFACE_ID InterfaceID, USBH_INTERFACE_INFO * InterfaceInfo) {
  USB_INTERFACE * UsbInterface;
  USB_DEVICE    * UsbDevice;
  USBH_LOG((USBH_MTYPE_DEVICE, "Device: USBH_GetInterfaceInfo: InterfaceID: %u!", InterfaceID));
  UsbInterface = GetInterfaceByID(InterfaceID);
  if (UsbInterface == NULL) {
    USBH_LOG((USBH_MTYPE_DEVICE, "Device: USBH_GetInterfaceInfo: GetInterfaceByID ID: %u!", InterfaceID));
    return USBH_STATUS_DEVICE_REMOVED;
  }
  UsbDevice                       = UsbInterface->Device;
  // Fill in the information
  InterfaceInfo->InterfaceID      = InterfaceID;
  InterfaceInfo->DeviceID         = UsbDevice->DeviceID;
  InterfaceInfo->VID              = UsbDevice->DeviceDescriptor.idVendor;
  InterfaceInfo->PID              = UsbDevice->DeviceDescriptor.idProduct;
  InterfaceInfo->bcdDevice        = UsbDevice->DeviceDescriptor.bcdDevice;
  InterfaceInfo->Interface        = UsbInterface->InterfaceDescriptor[2];
  InterfaceInfo->Class            = UsbInterface->InterfaceDescriptor[5];
  InterfaceInfo->SubClass         = UsbInterface->InterfaceDescriptor[6];
  InterfaceInfo->Protocol         = UsbInterface->InterfaceDescriptor[7];
  InterfaceInfo->OpenCount        = UsbInterface->OpenCount;
  InterfaceInfo->ExclusiveUsed    = UsbInterface->ExclusiveUsed;
  InterfaceInfo->Speed            = UsbDevice->DeviceSpeed;
  InterfaceInfo->SerialNumberSize = (U8)UsbDevice->SerialNumberSize;
  if (UsbDevice->SerialNumberSize > 0) {
    USBH_MEMCPY(InterfaceInfo->SerialNumber, UsbDevice->SerialNumber, UsbDevice->SerialNumberSize);
  }
  return USBH_STATUS_SUCCESS;
}

/*********************************************************************
*
*       USBH_OpenInterface
*
*  Function description:
*    Open an interface. If the interface is available it returns a
*    valid handle, otherwise NULL. The interface handle must be closed
*    with USBH_CloseInterface. If the flag Exclusive is TRUE the handle
*    is returned if no other application has an open handle to this
*    interface. If the interface is allocated exclusive no other
*    application can open it.
*/
USBH_STATUS USBH_OpenInterface(USBH_INTERFACE_ID InterfaceID, U8 Exclusive, USBH_INTERFACE_HANDLE * InterfaceHandle) {
  USB_INTERFACE * iface;
  USBH_STATUS     status;
  USBH_LOG((USBH_MTYPE_DEVICE, "Device: USBH_OpenInterface: InterfaceID: %u!", InterfaceID));
  iface = GetInterfaceByID(InterfaceID);
  if (iface == NULL) {
    USBH_LOG((USBH_MTYPE_DEVICE, "Device: USBH_OpenInterface: GetInterfaceByID iface-ID: %u!", InterfaceID));
    * InterfaceHandle = NULL;
    status            = USBH_STATUS_DEVICE_REMOVED;
  } else {
    status = USBH_STATUS_ERROR;                                 // Check exclusive usage
    if (Exclusive) {
      if (iface->ExclusiveUsed == 0 && iface->OpenCount == 0) { // On exclusive
        iface->ExclusiveUsed = 1;
        status = USBH_STATUS_SUCCESS;
      }
    } else {                                                    // On not exclusive
      if (iface->ExclusiveUsed == 0) {
        status = USBH_STATUS_SUCCESS;
      }
    }
    if (status == USBH_STATUS_SUCCESS) {                        // On success
      iface->OpenCount++;
      * InterfaceHandle = (USBH_INTERFACE_HANDLE)iface;
      INC_REF(iface->Device);
    } else {                                                    // On error
      * InterfaceHandle = NULL;
      USBH_WARN((USBH_MTYPE_DEVICE, "Device: USBH_OpenInterface IfaceID: %u!", InterfaceID));
    }
  }
  return status;
}

/*********************************************************************
*
*       USBH_CloseInterface
*
*  Function description:
*    Close the interface handle that was opened with USBH_OpenInterface.
*/
void USBH_CloseInterface(USBH_INTERFACE_HANDLE Handle) {
  USB_INTERFACE * iface;
  iface =       (USB_INTERFACE *)Handle;
  T_ASSERT_MAGIC(iface,             USB_INTERFACE);
  T_ASSERT_MAGIC(iface->Device,     USB_DEVICE);
  T_ASSERT      (iface->OpenCount > 0); // Always unequal zero also if opened exclusive
  USBH_LOG((USBH_MTYPE_DEVICE, "Device: USBH_CloseInterface: InterfaceID: %u!", iface->InterfaceID));
  iface->ExclusiveUsed = FALSE;
  iface->OpenCount--;
  DEC_REF(iface->Device);               // The caller is responsible to cancel all pending URB before closing the interface
}

/*********************************************************************
*
*       USBH_GetInterfaceIDByHandle
*
*  Function description:
*    Get the interface ID for a given index. A returned value of zero indicates an error.
*/
USBH_STATUS USBH_GetInterfaceIDByHandle(USBH_INTERFACE_HANDLE Handle, USBH_INTERFACE_ID * InterfaceID) {
  USB_INTERFACE * usbInterface;
  if (NULL == Handle) {
    return USBH_STATUS_INVALID_PARAM;
  }
  usbInterface = (USB_INTERFACE * )Handle;
  T_ASSERT_MAGIC(usbInterface, USB_INTERFACE);
  USBH_LOG((USBH_MTYPE_DEVICE, "Device: USBH_GetInterfaceIDByHandle: InterfaceID: %u!", usbInterface->InterfaceID));
  * InterfaceID = usbInterface->InterfaceID;
  T_ASSERT(usbInterface->InterfaceID != 0);
  return USBH_STATUS_SUCCESS;
}

/*********************************************************************
*
*       Imlementation of USB_INTERFACE
*
**********************************************************************
*/

/*********************************************************************
*
*       UbdNewUsbInterface
*
*  Function description:
*    Allocate a USB interface and makes an basic initialization
*/
USB_INTERFACE * UbdNewUsbInterface(USB_DEVICE * Device) {
  USB_INTERFACE * iface;
  USBH_LOG((USBH_MTYPE_DEVICE, "Device: UbdNewUsbInterface!"));
  T_ASSERT_MAGIC(Device, USB_DEVICE);
  iface = USBH_Malloc(sizeof(USB_INTERFACE));
  if (!iface) {
    USBH_WARN((USBH_MTYPE_UBD, "UBD: UbdNewUsbInterface: USBH_malloc!"));
    return NULL;
  }
  ZERO_MEMORY(iface, sizeof(USB_INTERFACE));

#if (USBH_DEBUG > 1)
  iface->Magic = USB_INTERFACE_MAGIC;
#endif

  iface->Device      = Device;
  DlistInit(&iface->UsbEndpointList);
  iface->InterfaceID = UbdGetNextInterfaceID(); // Get a new unique interface ID
  return iface;
}

/*********************************************************************
*
*       UbdDeleteUsbInterface
*
*  Function description:
*/
void UbdDeleteUsbInterface(USB_INTERFACE * UsbInterface) {
  T_ASSERT(UbdGetPendingUrbCount(UsbInterface) == 0);
  UbdRemoveEndpoints(UsbInterface);
  USBH_Free         (UsbInterface);
}

/*********************************************************************
*
*       UbdCreateEndpoints
*
*  Function description:
*
*  Return value:
*    TRUE on success
*/
USBH_STATUS UbdCreateEndpoints(USB_INTERFACE * UsbInterface) {
  U8           * EndpointDescriptor = UsbInterface->AlternateSettingDescriptor;
  U8             i;
  USB_ENDPOINT * UsbEndpoint;

  for (i = 0; i < UsbInterface->AlternateSettingDescriptor[4]; i++) { // For each endpoint of this interface
    EndpointDescriptor = UbdGetNextEndpointDesc(UsbInterface->Device, EndpointDescriptor, 0xff);
    if (EndpointDescriptor == NULL) {
      USBH_WARN((USBH_MTYPE_DEVICE, "Device: UbdCreateEndpoints: invalid configuration descriptor!"));
      return USBH_STATUS_INVALID_DESCRIPTOR;
    } else {
      UsbEndpoint = UbdNewEndpoint(UsbInterface, EndpointDescriptor);
      if (UsbEndpoint == NULL) {
        USBH_WARN((USBH_MTYPE_DEVICE, "Device: UbdCreateEndpoints: NewEndpoint failed!"));
        return USBH_STATUS_RESOURCES;
      } else {
        UbdAddUsbEndpoint(UsbEndpoint);
      }
    }
  }
  return USBH_STATUS_SUCCESS;
}

/*********************************************************************
*
*       UbdCreateEndpoints
*
*  Function description:
*    Removes all endpoints from the interface and from the host controller.
*    Before this function can be called all URBs to this endpoint should be completed.
*/
void UbdRemoveEndpoints(USB_INTERFACE * UsbInterface) {
  USB_ENDPOINT * UsbEndpoint;
  DLIST        * e;
  e            = DlistGetNext(&UsbInterface->UsbEndpointList);
  while (e != &UsbInterface->UsbEndpointList) {
    UsbEndpoint = GET_USB_ENDPOINT_FROM_ENTRY(e);
    T_ASSERT_MAGIC        (UsbEndpoint, USB_ENDPOINT);
    e = DlistGetNext(e);
    USBH_RemoveUsbEndpoint(UsbEndpoint);
    UbdDeleteEndpoint     (UsbEndpoint);
  }
}

/*********************************************************************
*
*       UbdAddUsbEndpoint
*
*  Function description:
*    Adds the endpoint to the list in the interface and increments the count
*    The endpoint object interface pointer must be valid.
*/
void UbdAddUsbEndpoint(USB_ENDPOINT * UsbEndpoint) {
  USB_INTERFACE * iface;
  USBH_LOG((USBH_MTYPE_DEVICE, "Device: UbdAddUsbEndpoint!"));
  T_ASSERT_MAGIC(UsbEndpoint, USB_ENDPOINT);
  iface = UsbEndpoint->UsbInterface;
  T_ASSERT_MAGIC(iface, USB_INTERFACE);
  USBH_LOG((USBH_MTYPE_DEVICE, "Device: UbdAddUsbEndpoint Ep: 0x%x bInterface: %u!", UsbEndpoint->EndpointDescriptor[2], iface->InterfaceDescriptor[2]));
  DlistInsertTail(&iface->UsbEndpointList, &UsbEndpoint->ListEntry);
  iface->EndpointCount++;
}

/*********************************************************************
*
*       USBH_RemoveUsbEndpoint
*
*  Function description:
*    Removes an endpoint to the interface list and decrements the count.
*    The endpoint object interface pointer must be valid.
*/
void USBH_RemoveUsbEndpoint(USB_ENDPOINT * UsbEndpoint) {
  USB_INTERFACE * iface;
  USBH_LOG((USBH_MTYPE_DEVICE, "Device: USBH_RemoveUsbEndpoint!"));
  T_ASSERT_MAGIC(UsbEndpoint, USB_ENDPOINT);
  iface = UsbEndpoint->UsbInterface;
  T_ASSERT_MAGIC(iface, USB_INTERFACE);
  USBH_LOG((USBH_MTYPE_DEVICE, "Device: USBH_RemoveUsbEndpoint Ep: 0x%x bInterface: %u!", UsbEndpoint->EndpointDescriptor[2], iface->InterfaceDescriptor[2]));
  DlistRemoveEntry(&UsbEndpoint->ListEntry);
  T_ASSERT(iface->EndpointCount);
  iface->EndpointCount--;
}

/*********************************************************************
*
*       UbdCompareUsbInterface
*
*  Function description:
*    CheckInterface returns TRUE if the InterfaceMaks matches with the
*    current interface settings.
*
*  Return value:
*    USBH_STATUS_SUCCESS interface matches
*    other values on error
*/
USBH_STATUS UbdCompareUsbInterface(USB_INTERFACE * Interface, USBH_INTERFACE_MASK * InterfaceMask, T_BOOL EnableHubInterfaces) {
  U16          mask;
  USBH_STATUS  status;
  U8         * idesc;
  USB_DEVICE * dev;
  T_ASSERT_MAGIC(Interface, USB_INTERFACE);
  if (NULL != InterfaceMask) {
    mask = InterfaceMask->Mask;
  } else {
    mask = 0;
  }
  idesc = Interface->InterfaceDescriptor;
  dev   = Interface->Device;
  if (!EnableHubInterfaces) {
    if (dev->DeviceDescriptor.bDeviceClass == USB_DEVICE_CLASS_HUB || idesc[USB_INTERFACE_DESC_CLASS_OFS] == USB_DEVICE_CLASS_HUB) {
      T_ASSERT(NULL != Interface->Device->UsbHub); // On error: the interface is an hub class interface
      return USBH_STATUS_ERROR;
    }
  }
  if (0 == mask) {
    return USBH_STATUS_SUCCESS;
  }
  status = USBH_STATUS_ERROR;
  for (; ; ) {
    if (mask & USBH_INFO_MASK_VID) {
      if (dev->DeviceDescriptor.idVendor != InterfaceMask->VID) {
        USBH_LOG((USBH_MTYPE_PNP, "PNP: UbdCompareUsbInterface invalid VID: 0x%x ",      (int)dev->DeviceDescriptor.idVendor));
        break;
      }
    }
    if (mask & USBH_INFO_MASK_PID) {
      if (dev->DeviceDescriptor.idProduct != InterfaceMask->PID) {
        USBH_LOG((USBH_MTYPE_PNP, "PNP: UbdCompareUsbInterface invalid PID: 0x%x ",      (int)dev->DeviceDescriptor.idProduct));
        break;
      }
    }
    if (mask & USBH_INFO_MASK_DEVICE) {
      if (dev->DeviceDescriptor.bcdDevice != InterfaceMask->bcdDevice) {
        USBH_LOG((USBH_MTYPE_PNP, "PNP: UbdCompareUsbInterface invalid bcdDevice: 0x%x ",(int)dev->DeviceDescriptor.bcdDevice));
        break;
      }
    }
    if (mask & USBH_INFO_MASK_INTERFACE) { // Check the interface number
      if (idesc[USB_INTERFACE_DESC_NUMBER_OFS] != InterfaceMask->Interface) {
        USBH_LOG((USBH_MTYPE_PNP, "PNP: UbdCompareUsbInterface invalid interface: %d ",  (int)idesc[2]));
        break;
      }
    }
    if (mask & USBH_INFO_MASK_CLASS) {     // Check class subclass and protocol
      if (idesc[USB_INTERFACE_DESC_CLASS_OFS] != InterfaceMask->Class) {
        USBH_LOG((USBH_MTYPE_PNP, "PNP: UbdCompareUsbInterface invalid class: %d ",      (int)idesc[5]));
        break;
      }
    }
    if (mask & USBH_INFO_MASK_SUBCLASS) {
      if (idesc[USB_INTERFACE_DESC_SUBCLASS_OFS] != InterfaceMask->SubClass) {
        USBH_LOG((USBH_MTYPE_PNP, "PNP: UbdCompareUsbInterface invalid sub class: %d ",  (int)idesc[6]));
        break;
      }
    }
    if (mask & USBH_INFO_MASK_PROTOCOL) {
      if (idesc[USB_INTERFACE_DESC_PROTOCOL_OFS] != InterfaceMask->Protocol) {
        USBH_LOG((USBH_MTYPE_PNP, "PNP: UbdCompareUsbInterface invalid protocol: %d ",   (int)idesc[7]));
        break;
      }
    }
    // On success
    USBH_LOG((USBH_MTYPE_PNP, "PNP: UbdCompareUsbInterface: success: VID: 0x%x PID: 0x%x Class: 0%d Interface: %d !", (int)InterfaceMask->VID,
                                     (int)InterfaceMask->PID, (int)InterfaceMask->Class, (int)InterfaceMask->Interface ));
    status = USBH_STATUS_SUCCESS;
    break;
  }
  return status;
}

/*********************************************************************
*
*       UbdGetPendingUrbCount
*
*  Function description:
*/
unsigned int UbdGetPendingUrbCount(USB_INTERFACE * Interface) {
  DLIST        * e;
  USB_ENDPOINT * UsbEndpoint;
  unsigned int   UrbCount = 0;
  e = DlistGetNext(&Interface->UsbEndpointList);
  while (e != &Interface->UsbEndpointList) {
    UsbEndpoint = GET_USB_ENDPOINT_FROM_ENTRY(e);
    T_ASSERT_MAGIC(UsbEndpoint, USB_ENDPOINT);
    e           = DlistGetNext(e);
    UrbCount   += UsbEndpoint->UrbCount;
  }
  return UrbCount;
}

/*********************************************************************
*
*       UbdSearchUsbEndpointInInterface
*
*  Function description:
*    Returns a pointer to USB_ENDPOINT if the parameter mask matches
*    with one of the endpoints of the interface!
*/
USB_ENDPOINT * UbdSearchUsbEndpointInInterface(USB_INTERFACE * Interface, const USBH_EP_MASK * mask) {
  DLIST        * e;
  USB_ENDPOINT * usb_endpoint = NULL;
  U8           * ep_desc;
  unsigned int   index        = 0;
  e = DlistGetNext(&Interface->UsbEndpointList);
  while (e != &Interface->UsbEndpointList) {
    usb_endpoint = GET_USB_ENDPOINT_FROM_ENTRY(e);
    T_ASSERT_MAGIC(usb_endpoint, USB_ENDPOINT);
    ep_desc      = usb_endpoint->EndpointDescriptor;
    e            = DlistGetNext(e);
    if (NULL != ep_desc) {
      if ( (((mask->Mask &USBH_EP_MASK_INDEX)     == 0) || index >= mask->Index) && (((mask->Mask &USBH_EP_MASK_ADDRESS) == 0) || ep_desc[USB_EP_DESC_ADDRESS_OFS] == mask->Address)
        && (((mask->Mask &USBH_EP_MASK_TYPE)      == 0) || (ep_desc[USB_EP_DESC_ATTRIB_OFS]&USB_EP_DESC_ATTRIB_MASK) == mask->Type)
        && (((mask->Mask &USBH_EP_MASK_DIRECTION) == 0) || (ep_desc[USB_EP_DESC_ADDRESS_OFS]&USB_EP_DESC_DIR_MASK)   == mask->Direction)) {
        break;
      }
    }
    index++;
  }
  return usb_endpoint;
}

/*********************************************************************
*
*       UbdAddUsbInterface
*
*  Function description:
*    Adds an interface to the devices list
*/
void UbdAddUsbInterface(USB_INTERFACE * UsbInterface) {
  USB_DEVICE * dev;
  T_ASSERT_MAGIC(UsbInterface, USB_INTERFACE);
  USBH_LOG((USBH_MTYPE_DEVICE, "Device: UbdAddUsbInterface dev-addr: %u!", UsbInterface->Device->UsbAddress));
  dev = UsbInterface->Device;
  DlistInsertTail(&dev->UsbInterfaceList, &UsbInterface->ListEntry);
  dev->InterfaceCount++;
}

/*********************************************************************
*
*       UbdRemoveUsbInterface
*
*  Function description:
*    Removes an interface from the devices list.
*/
void UbdRemoveUsbInterface(USB_INTERFACE * UsbInterface) {
  USB_DEVICE * dev;
  T_ASSERT_MAGIC(UsbInterface, USB_INTERFACE);
  USBH_LOG((USBH_MTYPE_DEVICE, "Device: UbdRemoveUsbInterface dev-addr: %u!", UsbInterface->Device->UsbAddress));
  dev = UsbInterface->Device;
  DlistRemoveEntry(&UsbInterface->ListEntry);
  T_ASSERT(dev->InterfaceCount);
  dev->InterfaceCount--;
}

/********************************* EOF ******************************/
