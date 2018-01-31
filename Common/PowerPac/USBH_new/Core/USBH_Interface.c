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
*       _ResetPipeCompletion
*
*  Function description:
*    Is used for URB requests where the default completion routine of
*    the default endpoint (USBH_DEFAULT_EP) object or from the USB endpoint
*    object (USB_ENDPOINT) object can not be used. The URBs internal
*    context and the Urb UbdContext contains additional information!
*/
static void _ResetPipeCompletion(USBH_URB * Urb) {
  USBH_URB         * originalUrb;
  USBH_DEFAULT_EP  * defaultEndpoint = (USBH_DEFAULT_EP * )Urb->Header.pInternalContext;
  USB_ENDPOINT     * usbEndpoint;
  USBH_STATUS        status;
  USB_DEVICE       * Device          = defaultEndpoint->pUsbDevice;
  USBH_HOST_DRIVER * HostEntry       = Device->pHostController->pDriver;

  USBH_ASSERT(Urb->Header.pfOnCompletion == NULL);         // The helpers URBs completion routine should always NULL
  defaultEndpoint->UrbCount--;                      // Decrement the count
  USBH_LOG((USBH_MTYPE_UBD, "UBD: _ResetPipeCompletion: urbcount: %u",defaultEndpoint->UrbCount));
  originalUrb = (USBH_URB * )Urb->Header.pUbdContext;     // Decrement the reference of the URB
  USBH_ASSERT(originalUrb->Header.pUbdContext != NULL); // Get the endpoint
  usbEndpoint                = (USB_ENDPOINT * )originalUrb->Header.pUbdContext;
  status                     = Urb->Header.Status;  // Transfer the status
  originalUrb->Header.Status = status;
  if (status == USBH_STATUS_SUCCESS) {
    status = HostEntry->pfResetEndpoint(usbEndpoint->hEP);
  }
  originalUrb->Header.Status = status;
  if (originalUrb->Header.pfOnCompletion != NULL) {
    originalUrb->Header.pfOnCompletion(originalUrb);
  }
  USBH_Free(Urb);                                   // Delete the helper URB
  DEC_REF(Device);
}

/*********************************************************************
*
*       _ResetEndpoint
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
static USBH_STATUS _ResetEndpoint(USB_ENDPOINT * UsbEndpoint, USBH_URB * Urb) {
  USB_DEVICE  * usbDevice;
  USBH_URB    * ep0Urb;
  USBH_STATUS   status;

  if (UsbEndpoint == NULL) {
    Urb->Header.Status = USBH_STATUS_INVALID_PARAM;
    return USBH_STATUS_INVALID_PARAM;
  }
  usbDevice = UsbEndpoint->pUsbInterface->pDevice;

  // The host controller layer stops submitting of new packets if an errors occurres on the bulk or interrupt endpoint.
  // New URBs are returned with USBH_STATUS_ENDPOINT_HALTED. Pending URBs are submitted at the end of the reset endpoint.

  //if (UsbEndpoint->UrbCount > 0 ) {
  //  Urb->Header.Status = USBH_STATUS_BUSY;
  //  return USBH_STATUS_BUSY;
  //}
  Urb->Header.pUbdContext = UsbEndpoint;              // Store the UsbEndpoint pointer in the original URB
  ep0Urb                 = (USBH_URB *)USBH_Malloc(sizeof(USBH_URB)); // The URB must be allocated because of the asynchronous request
  if (!ep0Urb) {
    USBH_WARN((USBH_MTYPE_UBD, "UBD: _ResetEndpoint: USBH_malloc!"));
    return USBH_STATUS_MEMORY;
  }
  // Prepare and submit the URB, the control endpoint is never in Halt!
  status = USBH_BD_SubmitClearFeatureEndpointStall(&usbDevice->DefaultEp, ep0Urb, Urb->Request.EndpointRequest.Endpoint, _ResetPipeCompletion, Urb);
  if (status != USBH_STATUS_PENDING) {
    USBH_WARN((USBH_MTYPE_UBD, "UBD: _ResetEndpoint: status: 0x%lx!",status));
    Urb->Header.Status = status;
    USBH_Free(ep0Urb);
  }
  return status;
}

/*********************************************************************
*
*       _AbortEP0
*
*  Function description:
*/
static USBH_STATUS _AbortEP0(USBH_DEFAULT_EP * UsbEndpoint, USBH_URB * Urb) {
  USBH_STATUS        status;
  USBH_HOST_DRIVER * pDriver;

#if (USBH_DEBUG > 1)
  Urb = Urb;
#endif

  if (UsbEndpoint == NULL) {
    Urb->Header.Status = USBH_STATUS_INVALID_PARAM;
    return USBH_STATUS_INVALID_PARAM;
  }
  pDriver = UsbEndpoint->pUsbDevice->pHostController->pDriver;
  status  = pDriver->pfAbortEndpoint(UsbEndpoint->hEP);
  USBH_ASSERT(status != USBH_STATUS_PENDING);      // Do not return status pending and we do not call the completion routine
  return status;
}


/*********************************************************************
*
*       _AbortEndpoint
*
*  Function description:
*/
static USBH_STATUS _AbortEndpoint(USB_ENDPOINT * UsbEndpoint, USBH_URB * Urb) {
  USBH_STATUS        status;
  USBH_HOST_DRIVER * pDriver;

#if (USBH_DEBUG > 1)
  Urb = Urb;
#endif

  if (UsbEndpoint == NULL) {
    Urb->Header.Status = USBH_STATUS_INVALID_PARAM;
    return USBH_STATUS_INVALID_PARAM;
  }
  pDriver = UsbEndpoint->pUsbInterface->pDevice->pHostController->pDriver;
  status  = pDriver->pfAbortEndpoint(UsbEndpoint->hEP);
  USBH_ASSERT(status != USBH_STATUS_PENDING);      // Do not return status pending and we do not call the completion routine
  return status;
}

/*********************************************************************
*
*       _SetInterfaceCompletion
*
*  Function description:
*/
static void _SetInterfaceCompletion(USBH_URB * Urb) {
  USB_INTERFACE * UsbInterface = (USB_INTERFACE *)Urb->Header.pInternalContext;
  USB_DEVICE    * Device       = UsbInterface->pDevice;
  USBH_STATUS     status;
  USBH_URB      * originalUrb  = (USBH_URB *)Urb->Header.pUbdContext;

  Device->DefaultEp.UrbCount--;                  // Decrement the count
  USBH_LOG((USBH_MTYPE_UBD, "UBD: _SetInterfaceCompletion: urbcount: %u", Device->DefaultEp.UrbCount));
  originalUrb = (USBH_URB * )Urb->Header.pUbdContext;  // decrement the reference of the URB
  status      = Urb->Header.Status;
  if (status == USBH_STATUS_SUCCESS) {           // On error the old endpoint structure is valid
    USBH_BD_RemoveEndpoints(UsbInterface);            // Delete all endpoints
    UsbInterface->pAlternateSettingDescriptor = UsbInterface->pNewAlternateSettingDescriptor; // store new alternate setting
    UsbInterface->CurrentAlternateSetting = UsbInterface->NewAlternateSetting;
    status = USBH_BD_CreateEndpoints(UsbInterface);   // Add new endpoints
  }
  originalUrb->Header.Status = status;           // Update the status
  if (originalUrb->Header.pfOnCompletion != NULL) {  // Call the completion
    originalUrb->Header.pfOnCompletion(originalUrb);
  }
  USBH_Free(Urb);                                // Delete the helper URB
  DEC_REF(Device);
}

/*********************************************************************
*
*       _SetInterface
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
static USBH_STATUS _SetInterface(USB_INTERFACE * usbInterface, USBH_URB * Urb) {
  USBH_STATUS    status;
  U8             alternateSetting = Urb->Request.SetInterface.AlternateSetting;
  U8             Interface        = usbInterface->pInterfaceDescriptor[2];
  unsigned int   PendingUrbs;
  USB_DEVICE   * Device           = usbInterface->pDevice;
  U8           * InterfaceDescriptor;

  if (alternateSetting == usbInterface->CurrentAlternateSetting) {                          // On the same alternate setting do nothing
    Urb->Header.Status = USBH_STATUS_SUCCESS;
    return USBH_STATUS_SUCCESS;
  }
  PendingUrbs = USBH_BD_GetPendingUrbCount(usbInterface);
  if (PendingUrbs > 0) {                                                                    // Check pending count
    Urb->Header.Status = USBH_STATUS_BUSY;
    return USBH_STATUS_BUSY;
  }
  InterfaceDescriptor = USBH_GetNextInterfaceDesc(Device, NULL, Interface, alternateSetting); // Check if the new alternate setting is available
  if (InterfaceDescriptor == NULL) {
    Urb->Header.Status = USBH_STATUS_INVALID_PARAM;
    return USBH_STATUS_INVALID_PARAM;
  }
  usbInterface->pNewAlternateSettingDescriptor = InterfaceDescriptor;
  usbInterface->NewAlternateSetting           = alternateSetting;
  // Prepare and submit the URB, the control endpoint is never in Halt!
  status                                      = USBH_BD_SubmitSetInterface(usbInterface, Interface, alternateSetting, _SetInterfaceCompletion, Urb);
  if (status != USBH_STATUS_PENDING) {
    Urb->Header.Status = status;
    USBH_LOG((USBH_MTYPE_UBD, "UBD: _SetInterfaceCompletion: urbcount: %u", Device->DefaultEp.UrbCount));
  }
  return status;
}

/*********************************************************************
*
*       _SetPowerState
*
*  Function description:
*/
static USBH_STATUS _SetPowerState(USB_INTERFACE * usbInterface, USBH_URB * Urb) {
  USBH_POWER_STATE   PowerState;
  USBH_STATUS        Status      = USBH_STATUS_INVALID_PARAM;
  USB_DEVICE       * pUsbDevice  = usbInterface->pDevice;
  USBH_HUB_PORT         * pHubPort    = pUsbDevice->pParentPort;
  USBH_HOST_DRIVER * pDriver     = pUsbDevice->pHostController->pDriver;

  PowerState                     = Urb->Request.SetPowerState.PowerState;
  if (pHubPort->RootHub != NULL) { // This is a root hub
    switch (PowerState) {
      case USBH_NORMAL_POWER:
        pDriver->pfSetPortSuspend(pUsbDevice->pHostController->hHostController, pHubPort->HubPortNumber, USBH_PORT_POWER_RUNNING);
        Status = USBH_STATUS_SUCCESS;
        break;
      case USBH_SUSPEND:
        pDriver->pfSetPortSuspend(pUsbDevice->pHostController->hHostController, pHubPort->HubPortNumber, USBH_PORT_POWER_SUSPEND);
        Status = USBH_STATUS_SUCCESS;
        break;
      default:
        USBH_WARN((USBH_MTYPE_UBD, "UBD: _SetPowerState: invalid param"));
    }
  }
  if (pHubPort->ExtHub != NULL) { // This is an external hub
    // ToDo: implement state machine
  }
  return Status;
}

/*********************************************************************
*
*       _SetConfiguration
*
*  Function description:
*    Set configuration
*/
static USBH_STATUS _SetConfiguration(USB_DEVICE * Dev, USBH_URB * Urb) {
  USBH_STATUS   Status = USBH_STATUS_SUCCESS;
  USBH_HUB_PORT    * HubPort;
  if (Urb->Request.SetConfiguration.ConfigurationDescriptorIndex == Dev->ConfigurationIndex) {
    Urb->Header.Status = USBH_STATUS_SUCCESS;              // This configuration is already set, do nothing, return success
  } else {                                                 // The configuration must be changed
    Dev->pParentPort->ConfigurationIndex = Urb->Request.SetConfiguration.ConfigurationDescriptorIndex;
    HubPort                             = Dev->pParentPort; // Make a local copy of the parent port, the link is cleared with USBH_MarkParentAndChildDevicesAsRemoved
    USBH_MarkParentAndChildDevicesAsRemoved(Dev);        // Delete the old instance of the device completely
    USBH_BD_SetPortState(HubPort, PORT_CONNECTED);              // The state connected causes the HUB to reset the device
    USBH_HC_ServicePorts(Dev->pHostController);                // Service all ports
  }
  return Status;
}

/*********************************************************************
*
*       _ResetDevice
*
*  Function description:
*    On reset we mark this device as removed and create a new device. The reason is,
*    that under some circumstances the device may change the descriptors and the interface.
*    E.g. the DFU class requires this. So we have to enumerate a new device to handle this.
*/
static USBH_STATUS _ResetDevice(USB_DEVICE * Dev) {
  USBH_HUB_PORT * HubPort;
  Dev->pParentPort->ConfigurationIndex = Dev->ConfigurationIndex;
  HubPort                             = Dev->pParentPort; // Make a local copy of the parent port, the link is cleared with USBH_MarkParentAndChildDevicesAsRemoved
  USBH_MarkParentAndChildDevicesAsRemoved(Dev);        // Delete the old instance of the device completely
  USBH_BD_SetPortState(HubPort, PORT_CONNECTED);              // The state connected causes the HUB to reset the device
  USBH_HC_ServicePorts(Dev->pHostController);                // Service all ports
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
USBH_STATUS USBH_SubmitUrb(USBH_INTERFACE_HANDLE Handle, USBH_URB * Urb) {
  USB_INTERFACE * usbInterface;
  USB_ENDPOINT  * usbEndpoint;
  USB_DEVICE    * usbDevice;

  USBH_STATUS     status = USBH_STATUS_INVALID_PARAM;
  USBH_LOG((USBH_MTYPE_URB, "URB: USBH_SubmitUrb: %s",USBH_UrbFunction2Str(Urb->Header.Function)));
  usbInterface = (USB_INTERFACE *)Handle;
  USBH_ASSERT_MAGIC(usbInterface, USB_INTERFACE);
  USBH_ASSERT_PTR(Urb);
  Urb->Header.Status = USBH_STATUS_PENDING; // Default status
  usbDevice          = usbInterface->pDevice;
  if (usbDevice->State < DEV_STATE_WORKING) {
    return USBH_STATUS_DEVICE_REMOVED;
  }
  switch (Urb->Header.Function) {
    case USBH_FUNCTION_CONTROL_REQUEST:
      Urb->Header.pInternalContext    = &usbDevice->DefaultEp;
      Urb->Header.pfOnInternalCompletion = USBH_BD_DefaultEpUrbCompletion;
      status                         = USBH_BD_DefaultEpSubmitUrb(usbInterface->pDevice, Urb);
      break;
    case USBH_FUNCTION_BULK_REQUEST:        // Fall trough
    case USBH_FUNCTION_INT_REQUEST:
      usbEndpoint                    = GET_EP_FROM_ADDRESS(usbInterface, Urb->Request.BulkIntRequest.Endpoint);
      Urb->Header.pInternalContext    = usbEndpoint;
      Urb->Header.pfOnInternalCompletion = USBH_BD_EpUrbCompletion;
      status                         = USBH_BD_EpSubmitUrb(usbEndpoint, Urb);

#if (USBH_DEBUG > 1)
      if (status != USBH_STATUS_SUCCESS && status != USBH_STATUS_PENDING) {
        USBH_LOG((USBH_MTYPE_URB, "USBH_SubmitUrb: Error Ep:0x%x ", Urb->Request.BulkIntRequest.Endpoint));
      }
#endif

      break;
    case USBH_FUNCTION_ISO_REQUEST:
      usbEndpoint                    = GET_EP_FROM_ADDRESS(usbInterface, Urb->Request.IsoRequest.Endpoint);
      Urb->Header.pInternalContext    = usbEndpoint;
      Urb->Header.pfOnInternalCompletion = USBH_BD_EpUrbCompletion;
      status                         = USBH_BD_EpSubmitUrb(usbEndpoint, Urb);
      break;
    case USBH_FUNCTION_RESET_ENDPOINT:
      usbEndpoint = GET_EP_FROM_ADDRESS(usbInterface, Urb->Request.EndpointRequest.Endpoint);
      status      = _ResetEndpoint(usbEndpoint, Urb);
      break;
    case USBH_FUNCTION_ABORT_ENDPOINT:
      if (Urb->Request.EndpointRequest.Endpoint == 0) {
        status = _AbortEP0(&usbDevice->DefaultEp, Urb);
      } else {
        usbEndpoint = GET_EP_FROM_ADDRESS(usbInterface, Urb->Request.EndpointRequest.Endpoint);
        status      = _AbortEndpoint(usbEndpoint, Urb);
      }
      break;
    case USBH_FUNCTION_SET_INTERFACE:
      status = _SetInterface(usbInterface, Urb);
      break;
    case USBH_FUNCTION_SET_POWER_STATE:
      status = _SetPowerState(usbInterface, Urb);
      break;
    case USBH_FUNCTION_SET_CONFIGURATION:
      status = _SetConfiguration(usbInterface->pDevice, Urb);
      break;
    case USBH_FUNCTION_RESET_DEVICE:
      status = _ResetDevice(usbInterface->pDevice);
      break;
    default:
      USBH_WARN((USBH_MTYPE_URB, "URB: USBH_SubmitUrb: invalid URB function: %d!",Urb->Header.Function));
      status = USBH_STATUS_INVALID_PARAM;
      break;
  }

#if (USBH_DEBUG > 1)
  if (status != USBH_STATUS_SUCCESS && status != USBH_STATUS_PENDING) {
    USBH_LOG((USBH_MTYPE_URB, "USBH_SubmitUrb: %s status:%s ",USBH_UrbFunction2Str(Urb->Header.Function),USBH_GetStatusStr(status)));
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
USBH_STATUS USBH_GetInterfaceInfo(USBH_INTERFACE_ID InterfaceID, USBH_INTERFACE_INFO * pInterfaceInfo) {
  USB_INTERFACE * UsbInterface;
  USB_DEVICE    * UsbDevice;
  USBH_LOG((USBH_MTYPE_DEVICE, "Device: USBH_GetInterfaceInfo: InterfaceID: %u!", InterfaceID));
  UsbInterface = USBH_BD_GetInterfaceById(InterfaceID);
  if (UsbInterface == NULL) {
    USBH_LOG((USBH_MTYPE_DEVICE, "Device: USBH_GetInterfaceInfo: USBH_BD_GetInterfaceById ID: %u!", InterfaceID));
    return USBH_STATUS_DEVICE_REMOVED;
  }
  UsbDevice                       = UsbInterface->pDevice;
  // Fill in the information
  pInterfaceInfo->InterfaceId       = InterfaceID;
  pInterfaceInfo->DeviceId          = UsbDevice->DeviceId;
  pInterfaceInfo->VendorId          = UsbDevice->DeviceDescriptor.idVendor;
  pInterfaceInfo->ProductId         = UsbDevice->DeviceDescriptor.idProduct;
  pInterfaceInfo->bcdDevice         = UsbDevice->DeviceDescriptor.bcdDevice;
  pInterfaceInfo->Interface         = UsbInterface->pInterfaceDescriptor[2];
  pInterfaceInfo->Class             = UsbInterface->pInterfaceDescriptor[5];
  pInterfaceInfo->SubClass          = UsbInterface->pInterfaceDescriptor[6];
  pInterfaceInfo->Protocol          = UsbInterface->pInterfaceDescriptor[7];
  pInterfaceInfo->OpenCount         = UsbInterface->OpenCount;
  pInterfaceInfo->ExclusiveUsed     = UsbInterface->ExclusiveUsed;
  pInterfaceInfo->Speed             = UsbDevice->DeviceSpeed;
  pInterfaceInfo->NumConfigurations = UsbDevice->DeviceDescriptor.bNumConfigurations;
  pInterfaceInfo->SerialNumberSize  = (U8)UsbDevice->SerialNumberSize;
  if (UsbDevice->SerialNumberSize > 0) {
    USBH_MEMCPY(pInterfaceInfo->acSerialNumber, UsbDevice->pSerialNumber, UsbDevice->SerialNumberSize);
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
  iface = USBH_BD_GetInterfaceById(InterfaceID);
  if (iface == NULL) {
    USBH_LOG((USBH_MTYPE_DEVICE, "Device: USBH_OpenInterface: USBH_BD_GetInterfaceById iface-ID: %u!", InterfaceID));
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
      INC_REF(iface->pDevice);
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
  USBH_ASSERT_MAGIC(iface,             USB_INTERFACE);
  USBH_ASSERT_MAGIC(iface->pDevice,     USB_DEVICE);
  USBH_ASSERT      (iface->OpenCount > 0); // Always unequal zero also if opened exclusive
  USBH_LOG((USBH_MTYPE_DEVICE, "Device: USBH_CloseInterface: InterfaceId: %u!", iface->InterfaceId));
  iface->ExclusiveUsed = FALSE;
  iface->OpenCount--;
  DEC_REF(iface->pDevice);               // The caller is responsible to cancel all pending URB before closing the interface
}

/*********************************************************************
*
*       USBH_GetInterfaceIdByHandle
*
*  Function description:
*    Get the interface ID for a given index. A returned value of zero indicates an error.
*/
USBH_STATUS USBH_GetInterfaceIdByHandle(USBH_INTERFACE_HANDLE Handle, USBH_INTERFACE_ID * pInterfaceId) {
  USB_INTERFACE * pUSBInterface;
  if (NULL == Handle) {
    return USBH_STATUS_INVALID_PARAM;
  }
  pUSBInterface = (USB_INTERFACE * )Handle;
  USBH_ASSERT_MAGIC(pUSBInterface, USB_INTERFACE);
  USBH_LOG((USBH_MTYPE_DEVICE, "Device: USBH_GetInterfaceIdByHandle: InterfaceId: %u!", pUSBInterface->InterfaceId));
  * pInterfaceId = pUSBInterface->InterfaceId;
  USBH_ASSERT(pUSBInterface->InterfaceId != 0);
  return USBH_STATUS_SUCCESS;
}

/*********************************************************************
*
*       Implementation of USB_INTERFACE
*
**********************************************************************
*/

/*********************************************************************
*
*       USBH_BD_NewUsbInterface
*
*  Function description:
*    Allocate a USB interface and makes an basic initialization
*/
USB_INTERFACE * USBH_BD_NewUsbInterface(USB_DEVICE * Device) {
  USB_INTERFACE * iface;
  USBH_LOG((USBH_MTYPE_DEVICE, "Device: USBH_BD_NewUsbInterface!"));
  USBH_ASSERT_MAGIC(Device, USB_DEVICE);
  iface = (USB_INTERFACE *)USBH_Malloc(sizeof(USB_INTERFACE));
  if (!iface) {
    USBH_WARN((USBH_MTYPE_UBD, "UBD: USBH_BD_NewUsbInterface: USBH_malloc!"));
    return NULL;
  }
  USBH_ZERO_MEMORY(iface, sizeof(USB_INTERFACE));
  IFDBG(iface->Magic = USB_INTERFACE_MAGIC);
  iface->pDevice      = Device;
  USBH_DLIST_Init(&iface->UsbEndpointList);
  iface->InterfaceId = USBH_BD_GetNextInterfaceId(); // Get a new unique interface ID
  return iface;
}

/*********************************************************************
*
*       USBH_BD_DeleteUsbInterface
*
*  Function description:
*/
void USBH_BD_DeleteUsbInterface(USB_INTERFACE * UsbInterface) {
  USBH_ASSERT(USBH_BD_GetPendingUrbCount(UsbInterface) == 0);
  USBH_BD_RemoveEndpoints(UsbInterface);
  USBH_Free         (UsbInterface);
}

/*********************************************************************
*
*       USBH_BD_CreateEndpoints
*
*  Function description:
*
*  Return value:
*    TRUE on success
*/
USBH_STATUS USBH_BD_CreateEndpoints(USB_INTERFACE * UsbInterface) {
  U8           * EndpointDescriptor = UsbInterface->pAlternateSettingDescriptor;
  U8             i;
  USB_ENDPOINT * UsbEndpoint;

  for (i = 0; i < UsbInterface->pAlternateSettingDescriptor[4]; i++) { // For each endpoint of this interface
    EndpointDescriptor = USBH_GetNextEndpointDesc(UsbInterface->pDevice, EndpointDescriptor, 0xff);
    if (EndpointDescriptor == NULL) {
      USBH_WARN((USBH_MTYPE_DEVICE, "Device: USBH_BD_CreateEndpoints: invalid configuration descriptor!"));
      return USBH_STATUS_INVALID_DESCRIPTOR;
    } else {
      UsbEndpoint = USBH_BD_NewEndpoint(UsbInterface, EndpointDescriptor);
      if (UsbEndpoint == NULL) {
        USBH_WARN((USBH_MTYPE_DEVICE, "Device: USBH_BD_CreateEndpoints: NewEndpoint failed!"));
        return USBH_STATUS_RESOURCES;
      } else {
        USBH_BD_AddUsbEndpoint(UsbEndpoint);
      }
    }
  }
  return USBH_STATUS_SUCCESS;
}

/*********************************************************************
*
*       USBH_BD_CreateEndpoints
*
*  Function description:
*    Removes all endpoints from the interface and from the host controller.
*    Before this function can be called all URBs to this endpoint should be completed.
*/
void USBH_BD_RemoveEndpoints(USB_INTERFACE * UsbInterface) {
  USB_ENDPOINT * UsbEndpoint;
  USBH_DLIST        * e;
  e            = USBH_DLIST_GetNext(&UsbInterface->UsbEndpointList);
  while (e != &UsbInterface->UsbEndpointList) {
    UsbEndpoint = GET_USB_ENDPOINT_FROM_ENTRY(e);
    USBH_ASSERT_MAGIC        (UsbEndpoint, USB_ENDPOINT);
    e = USBH_DLIST_GetNext(e);
    USBH_RemoveUsbEndpoint(UsbEndpoint);
    USBH_BD_DeleteEndpoint     (UsbEndpoint);
  }
}

/*********************************************************************
*
*       USBH_BD_AddUsbEndpoint
*
*  Function description:
*    Adds the endpoint to the list in the interface and increments the count
*    The endpoint object interface pointer must be valid.
*/
void USBH_BD_AddUsbEndpoint(USB_ENDPOINT * UsbEndpoint) {
  USB_INTERFACE * iface;
  USBH_LOG((USBH_MTYPE_DEVICE, "Device: USBH_BD_AddUsbEndpoint!"));
  USBH_ASSERT_MAGIC(UsbEndpoint, USB_ENDPOINT);
  iface = UsbEndpoint->pUsbInterface;
  USBH_ASSERT_MAGIC(iface, USB_INTERFACE);
  USBH_LOG((USBH_MTYPE_DEVICE, "Device: USBH_BD_AddUsbEndpoint Ep: 0x%x bInterface: %u!", UsbEndpoint->pEndpointDescriptor[2], iface->pInterfaceDescriptor[2]));
  USBH_DLIST_InsertTail(&iface->UsbEndpointList, &UsbEndpoint->ListEntry);
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
  USBH_ASSERT_MAGIC(UsbEndpoint, USB_ENDPOINT);
  iface = UsbEndpoint->pUsbInterface;
  USBH_ASSERT_MAGIC(iface, USB_INTERFACE);
  USBH_LOG((USBH_MTYPE_DEVICE, "Device: USBH_RemoveUsbEndpoint Ep: 0x%x bInterface: %u!", UsbEndpoint->pEndpointDescriptor[2], iface->pInterfaceDescriptor[2]));
  USBH_DLIST_RemoveEntry(&UsbEndpoint->ListEntry);
  USBH_ASSERT(iface->EndpointCount);
  iface->EndpointCount--;
}

/*********************************************************************
*
*       USBH_BD_CompareUsbInterface
*
*  Function description:
*    CheckInterface returns TRUE if the InterfaceMaks matches with the
*    current interface settings.
*
*  Return value:
*    USBH_STATUS_SUCCESS interface matches
*    other values on error
*/
USBH_STATUS USBH_BD_CompareUsbInterface(USB_INTERFACE * pInterface, USBH_INTERFACE_MASK * pInterfaceMask, USBH_BOOL EnableHubInterfaces) {
  U16          mask;
  USBH_STATUS  status;
  U8         * idesc;
  USB_DEVICE * dev;

  USBH_ASSERT_MAGIC(pInterface, USB_INTERFACE);
  if (NULL != pInterfaceMask) {
    mask = pInterfaceMask->Mask;
  } else {
    mask = 0;
  }
  idesc = pInterface->pInterfaceDescriptor;
  dev   = pInterface->pDevice;
  if (!EnableHubInterfaces) {
    if (dev->DeviceDescriptor.bDeviceClass == USB_DEVICE_CLASS_HUB || idesc[USB_INTERFACE_DESC_CLASS_OFS] == USB_DEVICE_CLASS_HUB) {
      USBH_ASSERT(NULL != pInterface->pDevice->pUsbHub); // On error: the interface is an hub class interface
      return USBH_STATUS_ERROR;
    }
  }
  if (0 == mask) {
    return USBH_STATUS_SUCCESS;
  }
  status = USBH_STATUS_ERROR;
  for (; ; ) {
    if (mask & USBH_INFO_MASK_VID) {
      if (dev->DeviceDescriptor.idVendor != pInterfaceMask->VendorId) {
        USBH_LOG((USBH_MTYPE_PNP, "PNP: USBH_BD_CompareUsbInterface invalid VendorId: 0x%x ",      (int)dev->DeviceDescriptor.idVendor));
        break;
      }
    }
    if (mask & USBH_INFO_MASK_PID) {
      if (dev->DeviceDescriptor.idProduct != pInterfaceMask->ProductId) {
        USBH_LOG((USBH_MTYPE_PNP, "PNP: USBH_BD_CompareUsbInterface invalid ProductId: 0x%x ",      (int)dev->DeviceDescriptor.idProduct));
        break;
      }
    }
    if (mask & USBH_INFO_MASK_DEVICE) {
      if (dev->DeviceDescriptor.bcdDevice != pInterfaceMask->bcdDevice) {
        USBH_LOG((USBH_MTYPE_PNP, "PNP: USBH_BD_CompareUsbInterface invalid bcdDevice: 0x%x ",(int)dev->DeviceDescriptor.bcdDevice));
        break;
      }
    }
    if (mask & USBH_INFO_MASK_INTERFACE) { // Check the interface number
      if (idesc[USB_INTERFACE_DESC_NUMBER_OFS] != pInterfaceMask->Interface) {
        USBH_LOG((USBH_MTYPE_PNP, "PNP: USBH_BD_CompareUsbInterface invalid interface: %d ",  (int)idesc[2]));
        break;
      }
    }
    if (mask & USBH_INFO_MASK_CLASS) {     // Check class subclass and protocol
      if (idesc[USB_INTERFACE_DESC_CLASS_OFS] != pInterfaceMask->Class) {
        USBH_LOG((USBH_MTYPE_PNP, "PNP: USBH_BD_CompareUsbInterface invalid class: %d ",      (int)idesc[5]));
        break;
      }
    }
    if (mask & USBH_INFO_MASK_SUBCLASS) {
      if (idesc[USB_INTERFACE_DESC_SUBCLASS_OFS] != pInterfaceMask->SubClass) {
        USBH_LOG((USBH_MTYPE_PNP, "PNP: USBH_BD_CompareUsbInterface invalid sub class: %d ",  (int)idesc[6]));
        break;
      }
    }
    if (mask & USBH_INFO_MASK_PROTOCOL) {
      if (idesc[USB_INTERFACE_DESC_PROTOCOL_OFS] != pInterfaceMask->Protocol) {
        USBH_LOG((USBH_MTYPE_PNP, "PNP: USBH_BD_CompareUsbInterface invalid protocol: %d ",   (int)idesc[7]));
        break;
      }
    }
    // On success
    USBH_LOG((USBH_MTYPE_PNP, "PNP: USBH_BD_CompareUsbInterface: success: VendorId: 0x%x ProductId: 0x%x Class: 0%d Interface: %d !", (int)pInterfaceMask->VendorId,
                                     (int)pInterfaceMask->ProductId, (int)pInterfaceMask->Class, (int)pInterfaceMask->Interface ));
    status = USBH_STATUS_SUCCESS;
    break;
  }
  return status;
}

/*********************************************************************
*
*       USBH_BD_GetPendingUrbCount
*
*  Function description:
*/
unsigned int USBH_BD_GetPendingUrbCount(USB_INTERFACE * Interface) {
  USBH_DLIST        * e;
  USB_ENDPOINT * UsbEndpoint;
  unsigned int   UrbCount = 0;
  e = USBH_DLIST_GetNext(&Interface->UsbEndpointList);
  while (e != &Interface->UsbEndpointList) {
    UsbEndpoint = GET_USB_ENDPOINT_FROM_ENTRY(e);
    USBH_ASSERT_MAGIC(UsbEndpoint, USB_ENDPOINT);
    e           = USBH_DLIST_GetNext(e);
    UrbCount   += UsbEndpoint->UrbCount;
  }
  return UrbCount;
}

/*********************************************************************
*
*       USBH_BD_SearchUsbEndpointInInterface
*
*  Function description:
*    Returns a pointer to USB_ENDPOINT if the parameter mask matches
*    with one of the endpoints of the interface!
*/
USB_ENDPOINT * USBH_BD_SearchUsbEndpointInInterface(USB_INTERFACE * Interface, const USBH_EP_MASK * mask) {
  USBH_DLIST        * e;
  USB_ENDPOINT * usb_endpoint = NULL;
  U8           * ep_desc;
  unsigned int   index        = 0;
  e = USBH_DLIST_GetNext(&Interface->UsbEndpointList);
  while (e != &Interface->UsbEndpointList) {
    usb_endpoint = GET_USB_ENDPOINT_FROM_ENTRY(e);
    USBH_ASSERT_MAGIC(usb_endpoint, USB_ENDPOINT);
    ep_desc      = usb_endpoint->pEndpointDescriptor;
    e            = USBH_DLIST_GetNext(e);
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
*       USBH_BD_AddUsbInterface
*
*  Function description:
*    Adds an interface to the devices list
*/
void USBH_BD_AddUsbInterface(USB_INTERFACE * UsbInterface) {
  USB_DEVICE * dev;
  USBH_ASSERT_MAGIC(UsbInterface, USB_INTERFACE);
  USBH_LOG((USBH_MTYPE_DEVICE, "Device: USBH_BD_AddUsbInterface dev-addr: %u!", UsbInterface->pDevice->UsbAddress));
  dev = UsbInterface->pDevice;
  USBH_DLIST_InsertTail(&dev->UsbInterfaceList, &UsbInterface->ListEntry);
  dev->InterfaceCount++;
}

/*********************************************************************
*
*       USBH_BD_RemoveUsbInterface
*
*  Function description:
*    Removes an interface from the devices list.
*/
void USBH_BD_RemoveUsbInterface(USB_INTERFACE * UsbInterface) {
  USB_DEVICE * dev;
  USBH_ASSERT_MAGIC(UsbInterface, USB_INTERFACE);
  USBH_LOG((USBH_MTYPE_DEVICE, "Device: USBH_BD_RemoveUsbInterface dev-addr: %u!", UsbInterface->pDevice->UsbAddress));
  dev = UsbInterface->pDevice;
  USBH_DLIST_RemoveEntry(&UsbInterface->ListEntry);
  USBH_ASSERT(dev->InterfaceCount);
  dev->InterfaceCount--;
}

/********************************* EOF ******************************/
