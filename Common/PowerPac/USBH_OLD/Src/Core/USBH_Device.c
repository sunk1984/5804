/*********************************************************************
*  
*   IAR PowerPac
*
*   (c) Copyright IAR Systems 2010.  All rights reserved.
*
**********************************************************************
----------------------------------------------------------------------
File        : USBH_Device.c
Purpose     : USB host bus driver core
              USB device object.
              Represents a physical device
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

#define SIM_DEV_ENUM_RESTART   0
#define SIM_DEV_ENUM_FATAL     0

#if SIM_DEV_ENUM_RESTART || SIM_DEV_ENUM_FATAL
static int gSimCt = 0;

#endif

/*********************************************************************
*
*       TConvDeviceDesc
*
*  Function description
*    TConvDeviceDesc convert a received byte aligned buffer to
*    a machine independent struct USB_DEVICE_DESCRIPTOR
*
*  pDevDesc: IN: Pointer to a empty struct --- OUT: Device descriptor
*/

static void TConvDeviceDesc(const U8 * pBuffer, USB_DEVICE_DESCRIPTOR * pDevDesc) {
  pDevDesc->bLength            = pBuffer[0];         //Index  0 bLength
  pDevDesc->bDescriptorType    = pBuffer[1];         //Index  1 bDescriptorType
  pDevDesc->bcdUSB             = pBuffer[3];
  pDevDesc->bcdUSB           <<= 8;
  pDevDesc->bcdUSB             = (U16)(pDevDesc->bcdUSB    + pBuffer[2]);
  pDevDesc->bDeviceClass       = pBuffer[4];         //Index  4 bDeviceClass
  pDevDesc->bDeviceSubClass    = pBuffer[5];         //Index  6 bDeviceSubClass
  pDevDesc->bDeviceProtocol    = pBuffer[6];         //Index  7 bDeviceProtocol
  pDevDesc->bMaxPacketSize0    = pBuffer[7];         //Index  8 bMaxPacketSize0
  pDevDesc->idVendor           = pBuffer[9];
  pDevDesc->idVendor         <<= 8;
  pDevDesc->idVendor           = (U16)(pDevDesc->idVendor  + pBuffer[8]);
  pDevDesc->idProduct          = pBuffer[11];
  pDevDesc->idProduct        <<= 8;
  pDevDesc->idProduct          = (U16)(pDevDesc->idProduct + pBuffer[10]);
  pDevDesc->bcdDevice          = pBuffer[13];
  pDevDesc->bcdDevice        <<= 8;
  pDevDesc->bcdDevice          = (U16)(pDevDesc->bcdDevice + pBuffer[12]);
  pDevDesc->iManufacturer      = pBuffer[14];         //Index 14 iManufacturer
  pDevDesc->iProduct           = pBuffer[15];         //Index 15 iProduct
  pDevDesc->iSerialNumber      = pBuffer[16];         //Index 16 iSerialNumber
  pDevDesc->bNumConfigurations = pBuffer[17];         //Index 17 bNumConfigurations
}

/*********************************************************************
*
*       UbdNewUsbDevice
*
*  Function description
*    Allocates device object and makes an basic initialization. Set the reference counter to one. Set the HostController pointer.
*    Initialize all dlists and needed IDs. In the default endpoint the URB list is initialized and a pointer to this object is set.
*/
USB_DEVICE * UbdNewUsbDevice(HOST_CONTROLLER * pHostController) {
  USB_DEVICE * pDev;

  T_ASSERT_MAGIC(pHostController, HOST_CONTROLLER);
  USBH_LOG((USBH_MTYPE_DEVICE, "Device Notification:  UbdNewUsbDevice!"));
  pDev = USBH_Malloc(sizeof(USB_DEVICE));
  if (NULL == pDev) {
    USBH_WARN((USBH_MTYPE_DEVICE, "Device Notification Error:  UbdNewUsbDevice: USBH_malloc!"));
    return NULL;
  }
  ZERO_MEMORY(pDev, sizeof(USB_DEVICE));
#if (USBH_DEBUG > 1)
  pDev->Magic = USB_DEVICE_MAGIC;
#endif
  pDev->HostController = pHostController;
  DlistInit(&pDev->UsbInterfaceList);
  pDev->DeviceID       = UbdGetNextDeviceID();
  HC_INC_REF(pHostController); // Add a reference to the host controller
  INC_REF(pDev);               // Initial refcount
  // The sub state machine increments the reference count of the device before submitting the request
  if (UrbSubStateInit(&pDev->SubState, pHostController, &pDev->DefaultEp.EpHandle, UbdProcessEnum, pDev)) {
    // On error
    USBH_WARN((USBH_MTYPE_DEVICE, "Device Notification Error:  UbdNewUsbDevice: UrbSubStateInit failed!"));
    DEC_REF(pDev); // Device is deleted
    return NULL;
  }
  pDev->EnumState = DEV_ENUM_IDLE; // default basic initialization
  // Init by zero memory
  //pDev->EnumUrb                = NULL;
  //pDev->CtrlTransferBuffer     = NULL;
  //pDev->CtrlTransferBufferSize = 0;
  //pDev->MaxFifoSize            = 0;
  pDev->DefaultEp.UsbDevice = pDev;
  return pDev;
}

/*********************************************************************
*
*       _AbortDeviceEndpoints
*
*  Function description
*    Abort URB's on all related endpoints
*/
static void _AbortDeviceEndpoints(USB_DEVICE * pDev) {
  DLIST          * pInterface, * ep;
  USB_HOST_ENTRY * pHostEntry;
  USB_INTERFACE  * pUsbInterface;
  USB_ENDPOINT   * pUsbEndpoint;

  T_ASSERT_MAGIC(pDev, USB_DEVICE);
  pHostEntry = &pDev->HostController->HostEntry;
  pInterface      = DlistGetNext(&pDev->UsbInterfaceList); // For each interface

  while (pInterface != &pDev->UsbInterfaceList) {
    pUsbInterface = GET_USB_INTERFACE_FROM_ENTRY(pInterface);
    T_ASSERT_MAGIC(pUsbInterface, USB_INTERFACE);
    pInterface = DlistGetNext(pInterface);
    ep    = DlistGetNext(&pUsbInterface->UsbEndpointList); // For each endpoint

    while (ep != &pUsbInterface->UsbEndpointList) {
      pUsbEndpoint = GET_USB_ENDPOINT_FROM_ENTRY(ep);
      T_ASSERT_MAGIC(pUsbEndpoint, USB_ENDPOINT);
      ep = DlistGetNext(ep);
      if (pUsbEndpoint->UrbCount > 0) {
        pHostEntry->AbortEndpoint(pUsbEndpoint->EpHandle);
      }
    }
  }
}

/*********************************************************************
*
*       UbdUdevMarkDeviceAsRemoved
*
*  Function description
*/
void UbdUdevMarkDeviceAsRemoved(USB_DEVICE * pDev) {
  USB_HOST_ENTRY * pHostEntry;
  T_ASSERT_MAGIC(pDev, USB_DEVICE);
  USBH_LOG((USBH_MTYPE_DEVICE, "Device Notification:  MarkDeviceAsRemoved pDev-addr: %u!", pDev->UsbAddress));
  if (pDev->State == DEV_STATE_REMOVED) {
    USBH_WARN((USBH_MTYPE_DEVICE, "Device Notification Warning:  UbdUdevMarkDeviceAsRemoved pDev-addr: %u already removed!", pDev->UsbAddress));
    return;
  }
  pDev->State = DEV_STATE_REMOVED;
  pHostEntry  = &pDev->HostController->HostEntry;
  UbdProcessDevicePnpNotifications(pDev, USBH_RemoveDevice);
  // URB's on default endpoint
  pHostEntry->AbortEndpoint(pDev->DefaultEp.EpHandle);
  _AbortDeviceEndpoints(pDev);
  UbdHcRemoveDeviceFromList(pDev); // Remove from the list in the host controller, it is not found during enumerations
  if (pDev->ParentPort != NULL) {  // Delete the link between the hub port and the device in both directions
    pDev->ParentPort->Device = NULL;
  }
  pDev->ParentPort = NULL;
  DEC_REF(pDev); // delete the initial reference
}

/*********************************************************************
*
*       UbdUdevMarkParentAndChildDevicesAsRemoved
*
*  Function description
*    Marks the device and all child devices if the device is an hub
*    as removed. If an device already removed then nothing is done.
*/
void UbdUdevMarkParentAndChildDevicesAsRemoved(USB_DEVICE * usbDev) {
  T_ASSERT_MAGIC(usbDev, USB_DEVICE);
  USBH_LOG((USBH_MTYPE_DEVICE, "Device Notification:  UbdUdevMarkParentAndChildDevicesAsRemoved pDev-addr: %u!", usbDev->UsbAddress));
  if (usbDev->State == DEV_STATE_REMOVED) {
    // Device already removed
    USBH_WARN((USBH_MTYPE_DEVICE, "Device Notification Warning:  UbdUdevMarkParentAndChildDevicesAsRemoved pDev-addr: %u already removed!", usbDev->UsbAddress));
    return;
  }

#if USBH_EXTHUB_SUPPORT
  if (NULL != usbDev->UsbHub) { // Device is a hub
    DLIST        child_dev_list;
    int          ct;
    USB_DEVICE * pDev;
    PDLIST       dev_entry;
    DlistInit(&child_dev_list);
    ct = UbdHubBuildChildDeviceList(usbDev, &child_dev_list);

    if (ct) {
      for (; ;) { // Remove all devices, start with the tail of the port tree list
        dev_entry = DlistGetPrev(&child_dev_list);
        if (dev_entry != &child_dev_list) {
          pDev = GET_USB_DEVICE_FROM_TEMP_ENTRY(dev_entry);
          T_ASSERT_MAGIC(pDev, USB_DEVICE);
          // Remove from the this temporary list before the device is deleted
          DlistRemoveEntry(dev_entry);
          UbdUdevMarkDeviceAsRemoved(pDev);
          ct--;
        } else { // List is empty
          break;
        }
      }
      T_ASSERT(0 == ct && DlistEmpty(&child_dev_list));
    }

#if (USBH_DEBUG > 1)
    else {
      USBH_WARN((USBH_MTYPE_DEVICE, "Device Notification Error:  UbdUdevMarkParentAndChildDevicesAsRemoved UbdHubBuildChildDeviceList!"));
    }
#endif

  } else {
    UbdUdevMarkDeviceAsRemoved(usbDev);
  }
#else
  UbdUdevMarkDeviceAsRemoved(usbDev);
#endif // USBH_EXTHUB_SUPPORT
}

/*********************************************************************
*
*       UbdDeleteDevice
*
*  Function description
*/
void UbdDeleteDevice(USB_DEVICE * pDev) {
  USBH_LOG((USBH_MTYPE_DEVICE, "Device Notification:  UbdDeleteDevice pDev-addr: %u!", pDev->UsbAddress));
  UrbSubStateExit(&pDev->SubState);

#if USBH_EXTHUB_SUPPORT
  if (NULL != pDev->UsbHub) {
    UbdDeleteHub(pDev->UsbHub);
    pDev->UsbHub = NULL;
  }
#endif
  UbdDeleteInterfaces(pDev);                   // Delete all interfaces, endpoints and notify the application of a remove event
  UbdReleaseDefaultEndpoint(&pDev->DefaultEp); // Release the default endpoint if any
  if (pDev->CtrlTransferBuffer != NULL) {
    UrbBufferFreeTransferBuffer(pDev->CtrlTransferBuffer);
  }
  if (pDev->ConfigDescriptor != NULL) {
    USBH_Free(pDev->ConfigDescriptor);
  }
  if (pDev->SerialNumber != NULL) {
    USBH_Free(pDev->SerialNumber);
  }
  HC_DEC_REF(pDev->HostController); // Release the reference of the ost controller
#if (USBH_DEBUG > 1)
  pDev->Magic = 0;
#endif
  USBH_Free(pDev);
}

/*********************************************************************
*
*       Implementation
*
**********************************************************************
*/

/*********************************************************************
*
*       UbdCheckCtrlTransferBuffer
*
*  Function description
*/
int UbdCheckCtrlTransferBuffer(USB_DEVICE * pDev, U16 RequestLength) {
  if (pDev->CtrlTransferBufferSize < RequestLength) {
    if (pDev->CtrlTransferBuffer != NULL) {
      UrbBufferFreeTransferBuffer(pDev->CtrlTransferBuffer);
      pDev->CtrlTransferBuffer = NULL;
    }
    // Allocate a new buffer
    pDev->CtrlTransferBufferSize = (unsigned int)USBH_MAX(DEFAULT_TRANSFERBUFFER_SIZE, RequestLength);
    pDev->CtrlTransferBuffer     = UrbBufferAllocateTransferBuffer(pDev->CtrlTransferBufferSize);

    //pDev->CtrlTransferBuffer     = UbdAllocateTransferBuffer      (pDev->CtrlTransferBufferSize);
    if (pDev->CtrlTransferBuffer == NULL) {
      USBH_WARN((USBH_MTYPE_DEVICE, "Device Notification Error:  UbdCheckCtrlTransferBuffer: UbdAllocateTransferBuffer failed"));
      DEC_REF(pDev); // Delete the initial reference
      return 0;
    }
  }
  return 1;
}

/*********************************************************************
*
*       EnumPrepareGetDescReq
*
*  Function description
*/
static void EnumPrepareGetDescReq(USB_DEVICE * pDev, U8 DescType, U8 DescIndex, U16 LanguageID, U16 RequestLength, void * pBuffer) {
  URB * Urb = &pDev->EnumUrb;
  ZERO_MEMORY(Urb, sizeof(URB));
  Urb->Header.Function                      = USBH_FUNCTION_CONTROL_REQUEST;
  Urb->Request.ControlRequest.Setup.Type    = 0x80; // STD, IN, device
  Urb->Request.ControlRequest.Setup.Request = USB_REQ_GET_DESCRIPTOR;
  Urb->Request.ControlRequest.Setup.Value   = (U16)((DescType << 8) | DescIndex);
  Urb->Request.ControlRequest.Setup.Index   = LanguageID;
  Urb->Request.ControlRequest.Setup.Length  = RequestLength;
  Urb->Request.ControlRequest.Buffer        = pBuffer;
  Urb->Request.ControlRequest.Length        = RequestLength;
}

/*********************************************************************
*
*       EnumPrepareSubmitSetConfiguration
*
*  Function description
*/
static void EnumPrepareSubmitSetConfiguration(USB_DEVICE * pDev) {
  USBH_STATUS   Status;
  URB         * Urb = &pDev->EnumUrb;

  ZERO_MEMORY(Urb, sizeof(URB));
  Urb->Header.Function                      = USBH_FUNCTION_CONTROL_REQUEST;
  Urb->Request.ControlRequest.Setup.Type    = 0x00; // STD, OUT, device
  Urb->Request.ControlRequest.Setup.Request = USB_REQ_SET_CONFIGURATION;
  Urb->Request.ControlRequest.Setup.Value   = UbdGetUcharFromDesc(pDev->ConfigDescriptor, 5);
  pDev->EnumState                            = DEV_ENUM_SET_CONFIGURATION;
  Status                                    = UrbSubStateSubmitRequest(&pDev->SubState, Urb, DEFAULT_SETUP_TIMEOUT, pDev);
  if (Status != USBH_STATUS_PENDING) {
    USBH_WARN((USBH_MTYPE_DEVICE, "Device Notification Error:  EnumPrepareSubmitSetConfiguration:UrbSubStateSubmitRequest failed %08x", Status));
    DEC_REF(pDev); // delete the initial reference
  }
}

/*********************************************************************
*
*       UbdEnumParentPortRestart
*
*  Function description
*    Sets the parent port to PORT_RESTART, decrement the reference count of the USB
*    device and service all hubs. Can only be called during device enumeration.
*
*/
void UbdEnumParentPortRestart(USB_DEVICE * pDev, USBH_STATUS Status) {
  HUB_PORT * pParentPort;
  T_ASSERT_MAGIC(pDev, USB_DEVICE);

  if (pDev->ParentPort != NULL) { // Parent port is available
    pParentPort = pDev->ParentPort;
    T_ASSERT_MAGIC(pParentPort, HUB_PORT);
    USBH_LOG((USBH_MTYPE_DEVICE, "Device Notification:  UbdEnumParentPortRestart: ref.ct.: %ld portnumber: %d portstate: %s", pDev->RefCount, (int)pParentPort->HubPortNumber, UbdPortStateStr(pParentPort->PortState)));
    UbdSetPortState(pParentPort, PORT_RESTART); // Try to restart that port
    UbdSetEnumErrorNotificationProcessDeviceEnum(pParentPort, pDev->EnumState, Status, pDev->UsbHub ? TRUE : FALSE);
  } else {
    USBH_WARN((USBH_MTYPE_DEVICE, "Device Notification Warning:  FATAL UbdEnumParentPortRestart: ParentPort is NULL"));
  }
  DEC_REF(pDev);                           // Delete the initial reference
  UbdHcServicePorts(pDev->HostController); // Service all ports
}

#if USBH_EXTHUB_SUPPORT

/*********************************************************************
*
*       ProcessEnumDisableParentHubPortCompletion
*
*  Function description
*/
static void ProcessEnumDisableParentHubPortCompletion(void * usbDevice) {
  USB_DEVICE * pDev;
  USBH_WARN((USBH_MTYPE_DEVICE, "Device Notification Warning:  EnumPortErrorCompletion")); // Port error info
  pDev = (USB_DEVICE * )usbDevice;
  T_ASSERT_MAGIC(pDev, USB_DEVICE);
  if (pDev->EnumUrb.Header.Status != USBH_STATUS_SUCCESS) {
    USBH_WARN((USBH_MTYPE_DEVICE, "Device Notification Error:  EnumPortErrorCompletion %08x", pDev->EnumUrb.Header.Status));
  }
  DEC_REF(pDev); // Release sub state reference
  DEC_REF(pDev); // Delete the initial reference
}

#endif

/*********************************************************************
*
*       UbdProcessEnumPortError
*
*  Function description
*    On error during enumeration the parent port is disabled before the enumeration device is deleted.
*/
void UbdProcessEnumPortError(USB_DEVICE * pDev, USBH_STATUS enumStatus) {
  HUB_PORT       * pParentPort;
  DEV_ENUM_STATE   enum_state;
  int              hub_flag;
  T_ASSERT_MAGIC(pDev, USB_DEVICE);
  enum_state = pDev->EnumState; // Save device flags before the device is deleted
  if (NULL != pDev->UsbHub) {
    hub_flag = TRUE;
  } else {
    hub_flag = FALSE;
  }
  if (pDev->ParentPort != NULL) { // Parent port is available
    pParentPort = pDev->ParentPort;
    T_ASSERT_MAGIC(pParentPort, HUB_PORT);
    USBH_WARN((USBH_MTYPE_DEVICE, "Device Notification Warning:  UbdEnumPortError: ref.ct.: %ld portnumber: %d portstate: %s", pDev->RefCount, (int)pParentPort->HubPortNumber, UbdPortStateStr(pParentPort->PortState)));
    UbdSetPortState(pParentPort, PORT_ERROR);
    if (pParentPort->PortStatus & PORT_STATUS_ENABLED) {
      if (NULL != pParentPort->RootHub) { // On error disable the parent port
        USB_HOST_ENTRY * pHostEntry;
        pHostEntry = &pDev->HostController->HostEntry;
        pHostEntry->DisablePort(pHostEntry->HcHandle, pParentPort->HubPortNumber); // Disable parent port: synchronous request delete the enum device object
        DEC_REF(pDev);                                                             // Delete the initial reference
      }

#if USBH_EXTHUB_SUPPORT
      else { // Parent hub port is an external port
        USBH_STATUS Status;
        T_ASSERT_MAGIC(pParentPort->ExtHub, USB_HUB);
        UbdHubPrepareClrFeatureReq(&pDev->EnumUrb, HDC_SELECTOR_PORT_ENABLE, pParentPort->HubPortNumber);
        // Use the enum device substate field to install the EnumPortErrorCompletion routine
        UrbSubStateExit(&pDev->SubState);
        UrbSubStateInit(&pDev->SubState, pDev->HostController, &pParentPort->ExtHub->HubDevice->DefaultEp.EpHandle, // Parent hub ep0 handle
        ProcessEnumDisableParentHubPortCompletion, pDev                                                           // Process enum context is the enum device
        );
        Status = UrbSubStateSubmitRequest(&pDev->SubState, &pDev->EnumUrb, DEFAULT_SETUP_TIMEOUT, pDev);
        if (Status != USBH_STATUS_PENDING) {
          USBH_WARN((USBH_MTYPE_DEVICE, "Device Notification Error:  UbdEnumPortError: UrbSubStateSubmitRequest failed %08x", Status));
          DEC_REF(pDev); // Delete the previous substate reference
          DEC_REF(pDev); // Delete the initial reference
        }
      }
#endif

    } else {
      DEC_REF(pDev);                                                                             // Delete the initial reference
    }
    UbdSetEnumErrorNotificationProcessDeviceEnum(pParentPort, enum_state, enumStatus, hub_flag); // Parent port is set to PORT_ERROR, notify the user
  } else {
    USBH_WARN((USBH_MTYPE_DEVICE, "Device Notification Warning:  FATAL: parent port during device enumeraton is NULL"));
    DEC_REF(pDev); // delete the initial reference
  }
}

/*********************************************************************
*
*       UdevCheckParentPortPower
*
*  Function description
*    Returns TRUE if the power supply of the USB device ok.
*    Returns also true if the device is low powered on an low powered parent port.
*    During the hub enumeration this case is checked an second time!
*/
static T_BOOL UdevCheckParentPortPower(USB_DEVICE * pDev) {
  int    power;
  T_BOOL high_power;
  T_ASSERT_MAGIC(pDev->ParentPort, HUB_PORT);

  power = pDev->ConfigDescriptor[USB_CONFIGURATION_DESCRIPTOR_POWER_INDEX] << 1; // Bus powered device

  if (power >= 500) {
    high_power = TRUE;
  } else {
    high_power = FALSE;
  }

  if (pDev->ParentPort->HighPowerFlag) { // High powered port always allowed
    return TRUE;
  } else {                              // On low powered port
    if (high_power) {
      return FALSE;
    } else {
      return TRUE; // Low power device on low power port
    }
  }
}

/*********************************************************************
*
*       UbdProcessEnum
*
*  Function description
*    Is called with an unlinked device object, this means this device is not in the host controllers device list.
*    The hub port enumDevice element is also NULL, because the device has  an unique USB address so another port
*    reset state machine can run during this device enumeration! If enumeration fails this state machine must
*    delete the device object. Stops on error and disables the parent port.
*/
void UbdProcessEnum(void * usbDevice) {
  U16           RequestLength;
  USBH_STATUS   Status;
  U8            SerialNumberIndex;
  URB         * Urb;
  USB_DEVICE  * EnumDev;
  HUB_PORT    * parent_port;

  INC_RECURSIVE_CT(UbdProcessEnum);
  EnumDev = (USB_DEVICE * )usbDevice;
  T_ASSERT_MAGIC(EnumDev, USB_DEVICE);
  parent_port = EnumDev->ParentPort;
  T_ASSERT_MAGIC(parent_port, HUB_PORT);
  // Set the devices enumeration state to DEV_ENUM_REMOVED if host is removed, the port not enabled or the hub does not work
  if (EnumDev->HostController->State == HC_REMOVED) { // Root hub removed
    USBH_LOG((USBH_MTYPE_DEVICE, "Device Notification:  INFO UbdProcessEnum: host removed: Set enum state to DEV_ENUM_REMOVED"));
    EnumDev->EnumState = DEV_ENUM_REMOVED;
  } else {
    if (parent_port->PortState != PORT_ENABLED) {
      USBH_LOG((USBH_MTYPE_DEVICE, "Device Notification:  INFO UbdProcessEnum: parent port not enabled: Set enum state to DEV_ENUM_REMOVED"));
      EnumDev->EnumState = DEV_ENUM_REMOVED;
    }

#if USBH_EXTHUB_SUPPORT
    else {
      if (NULL != parent_port->ExtHub) {
        if (NULL != parent_port->ExtHub->HubDevice) {
          if (parent_port->ExtHub->HubDevice->State < DEV_STATE_WORKING) {
            USBH_LOG((USBH_MTYPE_DEVICE, "Device Notification:  INFO UbdProcessEnum: hub does not work: Set enum state to DEV_ENUM_REMOVED"));
            EnumDev->EnumState = DEV_ENUM_REMOVED;
          }
        }
      }
    }
#endif

  }
  USBH_LOG((USBH_MTYPE_DEVICE, "Device Notification:  UbdProcessEnum %s Dev.ref.ct: %ld", UbdEnumStateStr(EnumDev->EnumState), EnumDev->RefCount));
  Urb = &EnumDev->EnumUrb;
  switch (EnumDev->EnumState) {
    case DEV_ENUM_RESTART:
      EnumDev->EnumState = DEV_ENUM_START; // Timeout elapsed after restart
    // Fall trough
    case DEV_ENUM_START:
      switch (EnumDev->DeviceSpeed) {                    // Calculate the max FIFO size and next state
        case USBH_LOW_SPEED:
          EnumDev->EnumState   = DEV_ENUM_GET_DEVICE_DESC; // Must be always 8 bytes
          EnumDev->MaxFifoSize = 8;
          RequestLength        = USB_DEVICE_DESCRIPTOR_LENGTH;  // sizeof(DEVICE_DESCIPTOR)
          break;
        case USBH_FULL_SPEED:
          EnumDev->EnumState   = DEV_ENUM_GET_DEVICE_DESC_PART; // Can be 8, 16, 32, or 64 bytes, start with 8 bytes and retry with the real size
          EnumDev->MaxFifoSize = 8;
          RequestLength        = 8;                                  // Size of expected FIFO
          break;
        case USBH_HIGH_SPEED:
          EnumDev->EnumState   = DEV_ENUM_GET_DEVICE_DESC;
          EnumDev->MaxFifoSize = 64;
          RequestLength        = USB_DEVICE_DESCRIPTOR_LENGTH; // sizeof(DEVICE_DESCIPTOR)
          break;
        default:
          RequestLength = 0;
          T_ASSERT0;
      }
      if (EnumDev->DefaultEp.EpHandle == NULL) {
        Status = UbdInitDefaultEndpoint(EnumDev);
        if (Status != USBH_STATUS_SUCCESS) {
          USBH_WARN((USBH_MTYPE_DEVICE, "Device Notification Error:  UbdProcessEnum:InitDefaultEndpoint failed"));
          UbdProcessEnumPortError(EnumDev, Status);
          goto exit;
        }
      }
      if (!UbdCheckCtrlTransferBuffer(EnumDev, RequestLength)) {
        goto exit;
      }
      // Prepare an URB
      EnumPrepareGetDescReq(EnumDev, USB_DEVICE_DESCRIPTOR_TYPE, 0, 0, RequestLength, EnumDev->CtrlTransferBuffer);
      Status = UrbSubStateSubmitRequest(&EnumDev->SubState, Urb, DEFAULT_SETUP_TIMEOUT, EnumDev);
      if (Status != USBH_STATUS_PENDING) {
        USBH_WARN((USBH_MTYPE_DEVICE, "Device Notification Error:  UbdProcessEnum: DEV_ENUM_START UrbSubStateSubmitRequest failed %08x", Status));
        UbdProcessEnumPortError(EnumDev, Status);
      }
      break;
    case DEV_ENUM_GET_DEVICE_DESC_PART:
      if (Urb->Header.Status != USBH_STATUS_SUCCESS) {
        USBH_WARN((USBH_MTYPE_DEVICE, "Device Notification Error:  UbdProcessEnum:DEV_ENUM_GET_DEVICE_DESC_PART failed %08x", Urb->Header.Status));
        UbdEnumParentPortRestart(EnumDev, Urb->Header.Status);
      } else {                                                                // Status success
        EnumDev->MaxFifoSize = * (((char *)EnumDev->CtrlTransferBuffer) + 7); // Extract the EP0 FIFO size
        RequestLength        = USB_DEVICE_DESCRIPTOR_LENGTH;                  // sizeof(DEVICE_DESCIPTOR) / Get the full device descriptor
        EnumDev->EnumState   = DEV_ENUM_GET_DEVICE_DESC;                      // Set next state
        if (EnumDev->MaxFifoSize != 8) {                                      // Set the new max FIFO size, setup a new endpoint
          UbdReleaseDefaultEndpoint(&EnumDev->DefaultEp);                     // Update the new FIFO size
          Status = UbdInitDefaultEndpoint(EnumDev);
          if (Status != USBH_STATUS_SUCCESS) {
            USBH_WARN((USBH_MTYPE_DEVICE, "Device Notification Error:  UbdProcessEnum:AddEndpoint failed"));
            UbdProcessEnumPortError(EnumDev, Status);
            goto exit;
          }
        }
        // Prepare an URB
        EnumPrepareGetDescReq(EnumDev, USB_DEVICE_DESCRIPTOR_TYPE, 0, 0, RequestLength, EnumDev->CtrlTransferBuffer);
        Status = UrbSubStateSubmitRequest(&EnumDev->SubState, Urb, DEFAULT_SETUP_TIMEOUT, EnumDev);
        if (Status != USBH_STATUS_PENDING) {
          USBH_WARN((USBH_MTYPE_DEVICE, "Device Notification Error:   UbdProcessEnum: DEV_ENUM_GET_DEVICE_DESC_PART UrbSubStateSubmitRequest failed %08x", Status));
          UbdProcessEnumPortError(EnumDev, Status);
          goto exit;
        }
      }
      break;
    case DEV_ENUM_GET_DEVICE_DESC:
      if (Urb->Header.Status != USBH_STATUS_SUCCESS || USB_DEVICE_DESCRIPTOR_LENGTH != Urb->Request.ControlRequest.Length) {
        USBH_WARN((USBH_MTYPE_DEVICE, "Device Notification Error:  UbdProcessEnum:DEV_ENUM_GET_DEVICE_DESC failed st:%08x, len:%d ", Urb->Header.Status, Urb->Request.ControlRequest.Length));
        UbdEnumParentPortRestart(EnumDev, USBH_STATUS_INVALID_DESCRIPTOR);
      } else {                                                                                                   // Status success
        TConvDeviceDesc((U8 * )EnumDev->CtrlTransferBuffer, &EnumDev->DeviceDescriptor);                         // Store the device descriptor in a typed format
        USBH_MEMCPY(EnumDev->DeviceDescriptorBuffer, EnumDev->CtrlTransferBuffer, USB_DEVICE_DESCRIPTOR_LENGTH); // Store the device descriptor in a raw format
        // Prepare an URB
        EnumPrepareGetDescReq(EnumDev, USB_CONFIGURATION_DESCRIPTOR_TYPE, EnumDev->ConfigurationIndex, 0, USB_CONFIGURATION_DESCRIPTOR_LENGTH, EnumDev->CtrlTransferBuffer);
        EnumDev->EnumState = DEV_ENUM_GET_CONFIG_DESC_PART;
        Status = UrbSubStateSubmitRequest(&EnumDev->SubState, Urb, DEFAULT_SETUP_TIMEOUT, EnumDev);
        if (Status != USBH_STATUS_PENDING) {
          USBH_WARN((USBH_MTYPE_DEVICE, "Device Notification Error:   UbdProcessEnum: DEV_ENUM_GET_DEVICE_DESC UrbSubStateSubmitRequest failed %08x", Status));
          UbdProcessEnumPortError(EnumDev, Status);
          goto exit;
        }
      }
      break;
    case DEV_ENUM_GET_CONFIG_DESC_PART:
      if (Urb->Header.Status != USBH_STATUS_SUCCESS || Urb->Request.ControlRequest.Length != USB_CONFIGURATION_DESCRIPTOR_LENGTH) {
        USBH_WARN((USBH_MTYPE_DEVICE, "Device Notification Error:  UbdProcessEnum:DEV_ENUM_GET_CONFIG_DESC_PART failed st:%08x, len:%d ", Urb->Header.Status, Urb->Request.ControlRequest.Length));
        UbdEnumParentPortRestart(EnumDev, USBH_STATUS_INVALID_DESCRIPTOR);
      } else { // Status success
        RequestLength = UbdGetUshortFromDesc(EnumDev->CtrlTransferBuffer, 2);
        if (!UbdCheckCtrlTransferBuffer(EnumDev, RequestLength)) {
          goto exit;
        }
        T_ASSERT_PTR(EnumDev->CtrlTransferBuffer);
        // Prepare an URB
        EnumPrepareGetDescReq(EnumDev, USB_CONFIGURATION_DESCRIPTOR_TYPE, EnumDev->ConfigurationIndex, 0, RequestLength, EnumDev->CtrlTransferBuffer);
        EnumDev->ConfigDescriptorSize = RequestLength;
        EnumDev->EnumState = DEV_ENUM_GET_CONFIG_DESC;
        Status = UrbSubStateSubmitRequest(&EnumDev->SubState, Urb, DEFAULT_SETUP_TIMEOUT, EnumDev);
        if (Status != USBH_STATUS_PENDING) {
          USBH_WARN((USBH_MTYPE_DEVICE, "Device Notification Error:   UbdProcessEnum: DEV_ENUM_GET_CONFIG_DESC_PART SubmitRequest failed 0x%08x", Status));
          UbdProcessEnumPortError(EnumDev, Status);
          goto exit;
        }
      }
      break;
    case DEV_ENUM_GET_CONFIG_DESC:
      if (Urb->Header.Status != USBH_STATUS_SUCCESS || Urb->Request.ControlRequest.Length != EnumDev->ConfigDescriptorSize) {
        USBH_WARN((USBH_MTYPE_DEVICE, "Device Notification Error:   UbdProcessEnum:DEV_ENUM_GET_CONFIG_DESC failed st:%08x, exp.length:%d rcv.length:%d ", Urb->Header.Status, EnumDev->ConfigDescriptorSize, Urb->Request.ControlRequest.Length));
        UbdEnumParentPortRestart(EnumDev, USBH_STATUS_INVALID_DESCRIPTOR);
      } else {                                   // Status success
        if (EnumDev->ConfigDescriptor != NULL) { // Save the configuration descriptor
          USBH_Free(EnumDev->ConfigDescriptor);
        }
        EnumDev->ConfigDescriptor = USBH_Malloc(EnumDev->ConfigDescriptorSize);
        if (EnumDev->ConfigDescriptor == NULL) {
          USBH_WARN((USBH_MTYPE_DEVICE, "Device Notification Error:  UbdProcessEnum:DEV_ENUM_GET_CONFIG_DESC USBH_malloc %d failed ", EnumDev->ConfigDescriptorSize));
          UbdProcessEnumPortError(EnumDev, USBH_STATUS_MEMORY);
          goto exit;
        }
        USBH_MEMCPY(EnumDev->ConfigDescriptor, EnumDev->CtrlTransferBuffer, Urb->Request.ControlRequest.Length);
        if (!UdevCheckParentPortPower(EnumDev)) { // Check power consumption
          USBH_WARN((USBH_MTYPE_DEVICE, "Device Notification Error:  UbdProcessEnum: UdevCheckParentPortPower: Parent port power exceeded!"));
          UbdProcessEnumPortError(EnumDev, USBH_STATUS_PORT);
          goto exit;
        }
        SerialNumberIndex = EnumDev->DeviceDescriptor.iSerialNumber;
        if (SerialNumberIndex == 0) {
          EnumPrepareSubmitSetConfiguration(EnumDev);
        } else {
          // Prepare an URB for the language ID
          EnumPrepareGetDescReq(EnumDev, USB_STRING_DESCRIPTOR_TYPE, 0, 0, 255, EnumDev->CtrlTransferBuffer);
          EnumDev->EnumState = DEV_ENUM_GET_LANG_ID;
          Status = UrbSubStateSubmitRequest(&EnumDev->SubState, Urb, DEFAULT_SETUP_TIMEOUT, EnumDev);

          if (Status != USBH_STATUS_PENDING) {
            USBH_WARN((USBH_MTYPE_DEVICE, "Device Notification Error:   UbdProcessEnum:DEV_ENUM_GET_CONFIG_DESC_PART UrbSubStateSubmitRequest failed %08x", Status));
            UbdProcessEnumPortError(EnumDev, Status);
            goto exit;
          }
        }
      }
      break;
    case DEV_ENUM_GET_LANG_ID:
      if (Urb->Header.Status != USBH_STATUS_SUCCESS || Urb->Request.ControlRequest.Length < 4) {
        USBH_WARN((USBH_MTYPE_DEVICE, "Device Notification Error:  UbdProcessEnum:DEV_ENUM_GET_LANG_ID failed st:%08x, len:%d ", Urb->Header.Status, Urb->Request.ControlRequest.Length));
      } else { // Status success
        EnumDev->LanguageID = UbdGetUshortFromDesc(EnumDev->CtrlTransferBuffer, 2);
      }
      SerialNumberIndex = EnumDev->DeviceDescriptor.iSerialNumber; // The language ID is now 0 or the first ID reported by the device
      // Prepare an URB
      EnumPrepareGetDescReq(EnumDev, USB_STRING_DESCRIPTOR_TYPE, SerialNumberIndex, EnumDev->LanguageID, 255, EnumDev->CtrlTransferBuffer);
      EnumDev->EnumState = DEV_ENUM_GET_SERIAL_DESC;
      Status = UrbSubStateSubmitRequest(&EnumDev->SubState, Urb, DEFAULT_SETUP_TIMEOUT, EnumDev);
      if (Status != USBH_STATUS_PENDING) {
        USBH_WARN((USBH_MTYPE_DEVICE, "Device Notification Error: UbdProcessEnum: DEV_ENUM_GET_LANG_ID UrbSubStateSubmitRequest failed %08x", Status));
        UbdProcessEnumPortError(EnumDev, Status);
        goto exit;
      }
      break;
    case DEV_ENUM_GET_SERIAL_DESC:
      if (Urb->Header.Status != USBH_STATUS_SUCCESS || Urb->Request.ControlRequest.Length <= 2) {
        USBH_WARN((USBH_MTYPE_DEVICE, "Device Notification Error:  UbdProcessEnum:DEV_ENUM_GET_SERIAL_DESC failed st:%08x, len:%d ", Urb->Header.Status, Urb->Request.ControlRequest.Length));
      } else {                               // Status success
        if (EnumDev->SerialNumber != NULL) { // Copy the serial number descriptor
          USBH_Free(EnumDev->SerialNumber);
        }
        EnumDev->SerialNumberSize = Urb->Request.ControlRequest.Length - 2; // Don't copy the header
        EnumDev->SerialNumber = USBH_Malloc(EnumDev->SerialNumberSize);
        if (EnumDev->SerialNumber == NULL) {
          USBH_WARN((USBH_MTYPE_DEVICE, "Device Notification Error:  UbdProcessEnum:USBH_malloc %d failed", EnumDev->SerialNumberSize));
          UbdProcessEnumPortError(EnumDev, USBH_STATUS_MEMORY);
          goto exit;
        }
        USBH_MEMCPY(EnumDev->SerialNumber, ((U8 * )(EnumDev->CtrlTransferBuffer)) + 2, EnumDev->SerialNumberSize);
      }
      EnumPrepareSubmitSetConfiguration(EnumDev);
      break;
    case DEV_ENUM_SET_CONFIGURATION:
      if (Urb->Header.Status != USBH_STATUS_SUCCESS) {
        USBH_WARN((USBH_MTYPE_DEVICE, "Device Notification Error:  UbdProcessEnum:DEV_ENUM_SET_CONFIGURATION failed st:%08x", Urb->Header.Status));
        UbdEnumParentPortRestart(EnumDev, Urb->Header.Status);
      } else {
#if USBH_EXTHUB_SUPPORT
        if (EnumDev->DeviceDescriptor.bDeviceClass == USB_DEVICE_CLASS_HUB) {
          USB_HUB * hub;
          // Device is an hub device, start the hub enumeration routine
          T_ASSERT(NULL == EnumDev->UsbHub); // Hub object is always unlinked
          hub = UbdAllocInitUsbHub(EnumDev);
          if (NULL == hub) {
            USBH_WARN((USBH_MTYPE_DEVICE, "Device Notification Error:  UbdProcessEnum: llocInitUsbHub failed"));
            UbdProcessEnumPortError(EnumDev, USBH_STATUS_RESOURCES);
            goto exit;
          }
          // Link the hub to the device and start the hub initialization
          EnumDev->UsbHub = hub;
          EnumDev->EnumState = DEV_ENUM_INIT_HUB;
          UbdStartHub(hub, EnumDev->PostEnumFunction, EnumDev->PostEnumerationContext);
        } else
#endif
        {
          EnumDev->EnumState = DEV_ENUM_IDLE; // Device on a root hub port
          if (EnumDev->PostEnumFunction != NULL) {
            EnumDev->PostEnumFunction(EnumDev->PostEnumerationContext);
          }
        }
      }
      break;
#if USBH_EXTHUB_SUPPORT
    case DEV_ENUM_INIT_HUB:
      T_ASSERT0; // Not allowed to call Process Enum when an USB hub is initialized
      break;
#endif
    case DEV_ENUM_REMOVED:
      UbdProcessEnumPortError(EnumDev, USBH_STATUS_ERROR); // Invalid parent port state during device enumeration
      break;
    default:
      T_ASSERT0;
  } // Switch
  exit:
  ;
  DEC_RECURSIVE_CT(UbdProcessEnum); // Only for testing
}

/*********************************************************************
*
*       UbdStartEnumeration
*/
void UbdStartEnumeration(USB_DEVICE * pDev, POST_ENUM_FUNCTION * PostEnumFunction, void * pContext) {
  T_ASSERT(pDev->EnumState == DEV_ENUM_IDLE);
  USBH_LOG((USBH_MTYPE_DEVICE, "Device Notification:  UbdNewUsbDevice!"));
  pDev->PostEnumFunction       = PostEnumFunction;
  pDev->PostEnumerationContext = pContext;
  // Device is now enumerating
  pDev->EnumState              = DEV_ENUM_START;
  pDev->State                  = DEV_STATE_ENUMERATE;
  UbdProcessEnum(pDev);
}

/*********************************************************************
*
*       UbdCreateInterfaces
*
*  Function description
*    Create all interfaces and endpoints, create PnP notification
*/
void UbdCreateInterfaces(void * pContext) {
  USB_DEVICE    * pDev     = (USB_DEVICE * )pContext;
  USB_INTERFACE * pUsbInterface;
  U8            * pDesc    = NULL;
  USBH_STATUS     Status;

  while ((pDesc = UbdGetNextInterfaceDesc(pDev, pDesc, 0xff, 0)) != NULL) {
    pUsbInterface = UbdNewUsbInterface(pDev);
    if (pUsbInterface == NULL) {
      USBH_WARN((USBH_MTYPE_DEVICE, "Device Notification Error:  UbdCreateInterfaces, UbdNewUsbInterface failed"));
      break;
    }
    pUsbInterface->InterfaceDescriptor        = pDesc;
    pUsbInterface->AlternateSettingDescriptor = pDesc;                             // Set the alternate setting descriptor to the same descriptor
    Status                                   = UbdCreateEndpoints(pUsbInterface); // Create the endpoints
    if (Status) {                                                                // On error
      UbdDeleteUsbInterface(pUsbInterface);
      continue;
    } else {
      UbdAddUsbInterface(pUsbInterface);                                          // Add the interfaces to the list
    }
  }
  UbdAddUsbDevice(pDev);
  UbdProcessDevicePnpNotifications(pDev, USBH_AddDevice);
}

/*********************************************************************
*
*       UbdDeleteInterfaces
*
*  Function description
*/
void UbdDeleteInterfaces(USB_DEVICE * pDev) {
  USB_INTERFACE * pUsbInterface;
  DLIST         * e;
  e = DlistGetNext(&pDev->UsbInterfaceList);
  while (e != &pDev->UsbInterfaceList) {
    pUsbInterface = GET_USB_INTERFACE_FROM_ENTRY(e);
    T_ASSERT_MAGIC(pUsbInterface, USB_INTERFACE);
    e = DlistGetNext(e);
    UbdRemoveUsbInterface(pUsbInterface);
    UbdDeleteUsbInterface(pUsbInterface);
  }
}

/*********************************************************************
*
*       UbdGetNextInterfaceDesc
*
*  Function description
*    Returns NULL or the pointer of the descriptor
*
*  Parameters:
*    pStart:            Can be NULL
*    InterfaceNumber:  0xff for don't care
*    AlternateSetting: 0xff for don't care
*/
U8 * UbdGetNextInterfaceDesc(USB_DEVICE * pDev, U8 * pStart, U8 InterfaceNumber, unsigned int AlternateSetting) {
  U8 * pDesc;
  U8 * pLimit = pDev->ConfigDescriptor + pDev->ConfigDescriptorSize;
  U8 * p      = NULL;

  if (pStart == NULL) {
    pDesc = pDev->ConfigDescriptor;
  } else {
    pDesc = pStart;
  }
  if (pDesc + 1 < pLimit) {
    pDesc += *pDesc; // Next descriptor
  }
  while (pDesc + USB_INTERFACE_DESCRIPTOR_LENGTH <= pLimit && *pDesc > 0) {
    if (pDesc[1] == USB_INTERFACE_DESCRIPTOR_TYPE && (pDesc[2] == InterfaceNumber || InterfaceNumber == 0xff) && (pDesc[3] == AlternateSetting || AlternateSetting == 0xff)) {
      p = pDesc;
      break;
    }
    pDesc += *pDesc;
  }
  return p;
}

/*********************************************************************
*
*       UbdGetNextEndpointDesc
*
*  Function description
*    Returns NULL or the pointer of the descriptor
*
*  Parameters:
*    pStart:    // Must be a pointer to an interface descriptor
*    Endpoint: // Endpoint address with direction bit 0xff for don't care
*/
U8 * UbdGetNextEndpointDesc(USB_DEVICE * pDev, U8 * pStart, U8 Endpoint) {
  U8 * pDesc;
  U8 * pLimit = pDev->ConfigDescriptor + pDev->ConfigDescriptorSize;
  U8 * p   = NULL;

  pDesc       = pStart;
  if (pDesc + 1 < pLimit) {
    pDesc += *pDesc; // Next descriptor
  }
  while (((pDesc + USB_ENDPOINT_DESCRIPTOR_LENGTH) <= pLimit) && (*pDesc > 0)) {
    if (pDesc[1] == USB_ENDPOINT_DESCRIPTOR_TYPE && (pDesc[2] == Endpoint || Endpoint == 0xff)) {
      p = pDesc;
      break;
    }
    pDesc += *pDesc;
  }
  return p;
}

/*********************************************************************
*
*       UbdGetUshortFromDesc
*
*  Function description
*/
U16 UbdGetUshortFromDesc(void * pBuffer, U16 Offset) {
  U8  * p = (U8 *)pBuffer;
  U16   v = (U16)((*(p + Offset)) | ((*(p + Offset + 1)) << 8));
  return v;
}

/*********************************************************************
*
*       UbdGetUcharFromDesc
*
*  Function description
*/
U8 UbdGetUcharFromDesc(void * pBuffer, U16 Offset) {
  U8 * p = (U8 *)pBuffer;
  return *(p + Offset);
}

/*********************************************************************
*
*       UBD interface
*
**********************************************************************
*/

/*********************************************************************
*
*       USBH_GetDeviceDescriptor
*
*  Function description
*    Returns a copy of the device descriptor to the user provided buffer. It does not call the device. If the handle is invalid or
*    the device is removed the function fails. It is possible to get parts of the descriptor. This function can be used to identify
*    a device during enumeration.
*/
USBH_STATUS USBH_GetDeviceDescriptor(USBH_INTERFACE_HANDLE Handle, U8 * pDescriptor, unsigned int Size, unsigned * pCount) {
  USB_INTERFACE * pUsbInterface;
  USB_DEVICE    * pDev;
  USBH_STATUS     Status = USBH_STATUS_SUCCESS;

  USBH_LOG((USBH_MTYPE_UBD, "UBD: USBH_GetDeviceDescriptor"));
  pUsbInterface = (USB_INTERFACE * )Handle;
  T_ASSERT_MAGIC(pUsbInterface, USB_INTERFACE);
  pDev          = pUsbInterface->Device;
  T_ASSERT_MAGIC(pDev, USB_DEVICE);
  if (pDev->State < DEV_STATE_WORKING) {
    return USBH_STATUS_DEVICE_REMOVED;
  }
  *pCount = USBH_MIN(Size, USB_DEVICE_DESCRIPTOR_LENGTH);
  USBH_MEMCPY(pDescriptor, pDev->DeviceDescriptorBuffer, *pCount);
  return Status;
}

/*********************************************************************
*
*       USBH_GetCurrentConfigurationDescriptor
*
*  Function description
*    It returns the current configuration descriptor. The device is not requested. It is a copy of the bus driver. The bus driver
*    selects the configuration at index 0 after the enumeration. It is the complete descriptor including all class, interface and
*    endpoint descriptors. The descriptor can be requested in parts. The caller provides the memory.
*/
USBH_STATUS USBH_GetCurrentConfigurationDescriptor(USBH_INTERFACE_HANDLE Handle, U8 * pDescriptor, unsigned int Size, unsigned * pCount) {
  USB_INTERFACE * pUsbInterface;
  USB_DEVICE    * pDev;
  USBH_STATUS     Status = USBH_STATUS_SUCCESS;

  USBH_LOG((USBH_MTYPE_UBD, "UBD: USBH_GetCurrentConfigurationDescriptor"));
  pUsbInterface = (USB_INTERFACE *)Handle;
  T_ASSERT_MAGIC(pUsbInterface, USB_INTERFACE);
  pDev          = pUsbInterface->Device;
  T_ASSERT_MAGIC(pDev, USB_DEVICE);
  if (pDev->State < DEV_STATE_WORKING) {
    return USBH_STATUS_DEVICE_REMOVED;
  }
  *pCount = USBH_MIN(Size, pDev->ConfigDescriptorSize);
  USBH_MEMCPY(pDescriptor, pDev->ConfigDescriptor, *pCount);
  return Status;
}

/*********************************************************************
*
*       USBH_GetInterfaceDescriptor
*
*  Function description
*    Returns the Interface descriptor for a given alternate setting.
*/
USBH_STATUS USBH_GetInterfaceDescriptor(USBH_INTERFACE_HANDLE Handle, U8 AlternateSetting, U8 * pDescriptor, unsigned int Size, unsigned * pCount) {
  USB_INTERFACE            * pUsbInterface;
  USB_DEVICE               * pDev;
  USBH_STATUS                Status = USBH_STATUS_SUCCESS;
  USB_INTERFACE_DESCRIPTOR * pInterfaceDescriptor;
  U8                       * pBuffer;
  USBH_LOG((USBH_MTYPE_UBD, "UBD: USBH_GetInterfaceDescriptor: Alt Seting:%d", AlternateSetting));
  pUsbInterface = (USB_INTERFACE * )Handle;
  T_ASSERT_MAGIC(pUsbInterface, USB_INTERFACE);
  pDev          = pUsbInterface->Device;
  T_ASSERT_MAGIC(pDev, USB_DEVICE);
  if (pDev->State < DEV_STATE_WORKING) {
    return USBH_STATUS_DEVICE_REMOVED;
  }
  pInterfaceDescriptor = (USB_INTERFACE_DESCRIPTOR * )pUsbInterface->InterfaceDescriptor;
  pBuffer              = UbdGetNextInterfaceDesc(pDev, NULL, pInterfaceDescriptor->bInterfaceNumber, AlternateSetting);
  if (NULL == pBuffer) {
    USBH_WARN((USBH_MTYPE_DEVICE, "Device Notification Error: USBH_GetInterfaceDescriptor: UbdGetNextInterfaceDesc!"));
    return USBH_STATUS_INVALID_PARAM;
  }
  *pCount = USBH_MIN(Size, USB_INTERFACE_DESCRIPTOR_LENGTH);
  USBH_MEMCPY(pDescriptor, pBuffer, *pCount);
  return Status;
}

/*********************************************************************
*
*       UbdSearchUsbInterface
*
*  Function description
*    Searches in the interface list of the device an interface that matches with iMask.
*
*  Return values:
*    On success: Pointer to the interface descriptor!
*    Else:       Error
*/
USBH_STATUS UbdSearchUsbInterface(USB_DEVICE * pDev, USBH_INTERFACE_MASK * iMask, USB_INTERFACE ** ppUsbInterface) {
  USB_INTERFACE * pInterface;
  USBH_STATUS     Status = USBH_STATUS_INVALID_PARAM;
  PDLIST          entry;
  
  T_ASSERT_MAGIC(pDev, USB_DEVICE);
  entry = DlistGetNext(&pDev->UsbInterfaceList);
  while (entry != &pDev->UsbInterfaceList) {
    pInterface = GET_USB_INTERFACE_FROM_ENTRY(entry); // Search in all device interfaces and notify every interface
    T_ASSERT_MAGIC(pInterface, USB_INTERFACE);
    Status = UbdCompareUsbInterface(pInterface, iMask, TRUE);
    if (USBH_STATUS_SUCCESS == Status) {
      *ppUsbInterface = pInterface;
      break;
    }
    entry = DlistGetNext(entry);
  }
  return Status;
}

/*********************************************************************
*
*       UbdGetEndpointDescriptorFromInterface
*
*  Function description
*    Searches an endpoint descriptor in the current interface descriptor.
*
*  Return value
*    On success:   The pointer to the endpoint descriptor is returned.
*
*  Parameters:
*    ppDescriptor: Returns a pointer to the descriptor
*/
USBH_STATUS UbdGetEndpointDescriptorFromInterface(USB_INTERFACE * pUsbInterface, U8 alternateSetting, const USBH_EP_MASK * pMask, U8 ** ppDescriptor) {
  USBH_STATUS                Status = USBH_STATUS_SUCCESS;
  USB_INTERFACE_DESCRIPTOR * pInterfaceDescriptor;
  U8                       * pBuffer;
  U8                       * pEndpointDescriptor;
  const U8                 * pNextInterface;
  USB_DEVICE               * pDev;
  int                        Length;
  unsigned int               Index = 0;

  USBH_LOG((USBH_MTYPE_DEVICE, "Device Notification:  GetEndpointDescriptorFromInterfaceDesc: Alt Seting:%d pMask: 0x%x", alternateSetting, pMask->Mask));
  T_ASSERT_MAGIC(pUsbInterface, USB_INTERFACE);
  pDev = pUsbInterface->Device;
  T_ASSERT_MAGIC(pDev, USB_DEVICE);
  if (pDev->State < DEV_STATE_WORKING) {
    return USBH_STATUS_DEVICE_REMOVED;
  }
  // First get the interface descriptor from the configuration descriptor with the alternate setting.
  pInterfaceDescriptor = (USB_INTERFACE_DESCRIPTOR * )pUsbInterface->InterfaceDescriptor;
  pBuffer = UbdGetNextInterfaceDesc(pDev, NULL, pInterfaceDescriptor->bInterfaceNumber, alternateSetting);
  if (NULL == pBuffer) {
    USBH_WARN((USBH_MTYPE_DEVICE, "Device Notification Error:  USBH_GetEndpointDescriptor: UbdGetNextInterfaceDesc!"));
    return USBH_STATUS_INVALID_PARAM;
  }
  Length             = (int)(pDev->ConfigDescriptor + pDev->ConfigDescriptorSize - pBuffer);
  pNextInterface      = TSearchNextDescriptor(pBuffer, &Length, USB_INTERFACE_DESCRIPTOR_TYPE);
  pEndpointDescriptor = pBuffer; // Get the endpoint from the interface
  for (; ;) {
    pEndpointDescriptor = UbdGetNextEndpointDesc(pDev, pEndpointDescriptor, 0xff);
    if (NULL == pEndpointDescriptor || (pNextInterface != NULL && pEndpointDescriptor > pNextInterface)) {
      USBH_WARN((USBH_MTYPE_DEVICE, "Device Notification Error:  USBH_GetEndpointDescriptor: UbdGetNextEndpointDesc!"));
      Status = USBH_STATUS_INVALID_PARAM;
      break;
    } else { // Check the mask
      if ((((pMask->Mask &USBH_EP_MASK_INDEX) == 0) || Index >= pMask->Index) && (((pMask->Mask &USBH_EP_MASK_ADDRESS) == 0) || pEndpointDescriptor[USB_EP_DESC_ADDRESS_OFS] == pMask->Address)
        && (((pMask->Mask &USBH_EP_MASK_TYPE) == 0) || (pEndpointDescriptor[USB_EP_DESC_ATTRIB_OFS]&USB_EP_DESC_ATTRIB_MASK) == pMask->Type)
        && (((pMask->Mask &USBH_EP_MASK_DIRECTION) == 0) || (pEndpointDescriptor[USB_EP_DESC_ADDRESS_OFS]&USB_EP_DESC_DIR_MASK) == pMask->Direction))
        {
        *ppDescriptor = pEndpointDescriptor;
        break;
      }
      Index++;
    }
  }
  return Status;
}

/*********************************************************************
*
*       USBH_GetEndpointDescriptor
*
*  Function description
*/
USBH_STATUS USBH_GetEndpointDescriptor(USBH_INTERFACE_HANDLE Handle, U8 AlternateSetting, const USBH_EP_MASK * pMask, U8 * pDescriptor, unsigned int Size, unsigned * pCount) {
  USB_INTERFACE * pUsbInterface;
  USBH_STATUS     Status = USBH_STATUS_SUCCESS;
  U8            * pBuffer;
  USBH_LOG((USBH_MTYPE_UBD, "UBD: USBH_GetEndpointDescriptor: Alt Seting:%d pMask: 0x%x", AlternateSetting, pMask->Mask));
  pUsbInterface = (USB_INTERFACE * )Handle;
  T_ASSERT_MAGIC(pUsbInterface, USB_INTERFACE);
  *pCount = 0;
  Status = UbdGetEndpointDescriptorFromInterface(pUsbInterface, AlternateSetting, pMask, &pBuffer);

  if (Status == USBH_STATUS_SUCCESS) {
    *pCount = USBH_MIN(Size, USB_ENDPOINT_DESCRIPTOR_LENGTH);
    USBH_MEMCPY(pDescriptor, pBuffer, *pCount);
  }
  return Status;
}

/*********************************************************************
*
*       USBH_GetSerialNumber
*
*  Function description
*    It returns the serial number as a UNICODE string in USB little endian format. pCount returns the number of valid bytes.
*    The string is not zero terminated.
*/
USBH_STATUS USBH_GetSerialNumber(USBH_INTERFACE_HANDLE Handle, U8 * pDescriptor, unsigned int Size, unsigned * pCount) {
  USB_INTERFACE * pUsbInterface;
  USB_DEVICE    * pDev;
  USBH_STATUS     Status = USBH_STATUS_SUCCESS;

  USBH_LOG((USBH_MTYPE_UBD, "UBD: USBH_GetSerialNumber"));
  pUsbInterface = (USB_INTERFACE * )Handle;
  T_ASSERT_MAGIC(pUsbInterface, USB_INTERFACE);
  pDev           = pUsbInterface->Device;
  if (pDev->State < DEV_STATE_WORKING) {
    return USBH_STATUS_DEVICE_REMOVED;
  }
  *pCount = USBH_MIN(Size, pDev->SerialNumberSize);
  if (*pCount == 0) {
    return Status;
  }
  USBH_MEMCPY(pDescriptor, pDev->SerialNumber, *pCount); // Returns a little endian unicode string
  return Status;
}

/*********************************************************************
*
*       USBH_GetSpeed
*
*  Function description
*    Returns the operating speed of the device.
*/
USBH_STATUS USBH_GetSpeed(USBH_INTERFACE_HANDLE Handle, USBH_SPEED * pSpeed) {
  USB_INTERFACE * pUsbInterface;
  USB_DEVICE    * pDev;

  USBH_LOG((USBH_MTYPE_UBD, "UBD: USBH_GetSpeed"));
  pUsbInterface = (USB_INTERFACE *)Handle;
  T_ASSERT_MAGIC(pUsbInterface, USB_INTERFACE);
  pDev          = pUsbInterface->Device;
  if (pDev->State < DEV_STATE_WORKING) {
    USBH_WARN((USBH_MTYPE_DEVICE, "Device Notification Error:  USBH_GetSpeed: invalid device state!"));
    return USBH_STATUS_DEVICE_REMOVED;
  }
  *pSpeed = pDev->DeviceSpeed;
  return USBH_STATUS_SUCCESS;
}

/*********************************************************************
*
*       USBH_GetFrameNumber
*
*  Function description
*/
USBH_STATUS USBH_GetFrameNumber(USBH_INTERFACE_HANDLE Handle, U32 * pFrameNumber) {
  USB_INTERFACE   * pUsbInterface;
  HOST_CONTROLLER * pHostController;
  USB_DEVICE      * pDev;

  pUsbInterface = (USB_INTERFACE * )Handle;
  T_ASSERT_MAGIC(pUsbInterface, USB_INTERFACE);
  pDev          = pUsbInterface->Device;
  if (pDev->State < DEV_STATE_WORKING) {
    USBH_WARN((USBH_MTYPE_DEVICE, "Device Notification Error:  USBH_GetFrameNumber: invalid device state!"));
    return USBH_STATUS_DEVICE_REMOVED;
  }
  pHostController = pDev->HostController;
  *pFrameNumber  = pHostController->HostEntry.GetFrameNumber(pHostController->HostEntry.HcHandle);
  USBH_LOG((USBH_MTYPE_UBD, "UBD: USBH_GetFrameNumber: %d", *pFrameNumber));
  return USBH_STATUS_SUCCESS;
}

/*********************************************************************
*
*       UbdDevGetPendingUrbCount
*
*  Function description
*/
unsigned int UbdDevGetPendingUrbCount(USB_DEVICE * pDev) {
  DLIST         * e;
  USB_INTERFACE * pUsbInterface;
  unsigned int    Count = 0;

  e = DlistGetNext(&pDev->UsbInterfaceList);
  while (e != &pDev->UsbInterfaceList) {
    pUsbInterface = GET_USB_INTERFACE_FROM_ENTRY(e);
    T_ASSERT_MAGIC(pUsbInterface, USB_INTERFACE);
    e            = DlistGetNext(e);
    Count       += UbdGetPendingUrbCount(pUsbInterface);
  }
  Count += pDev->DefaultEp.UrbCount;
  return Count;
}

/********************************* EOF ******************************/
