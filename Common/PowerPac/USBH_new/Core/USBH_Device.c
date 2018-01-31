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

/*********************************************************************
*
*       _ConvDeviceDesc
*
*  Function description
*    _ConvDeviceDesc convert a received byte aligned buffer to
*    a machine independent struct USB_DEVICE_DESCRIPTOR
*
*  pDevDesc: IN: Pointer to a empty struct --- OUT: Device descriptor
*/

static void _ConvDeviceDesc(const U8 * pBuffer, USB_DEVICE_DESCRIPTOR * pDevDesc) {
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
*       _AbortDeviceEndpoints
*
*  Function description
*    Abort URB's on all related endpoints
*/
static void _AbortDeviceEndpoints(USB_DEVICE * pDev) {
  USBH_DLIST            * pInterface;
  USBH_DLIST            * pEPList;
  USBH_HOST_DRIVER * pDriver;
  USB_INTERFACE    * pUsbInterface;
  USB_ENDPOINT     * pUsbEndpoint;

  USBH_ASSERT_MAGIC(pDev, USB_DEVICE);
  pDriver    = pDev->pHostController->pDriver;
  pInterface = USBH_DLIST_GetNext(&pDev->UsbInterfaceList); // For each interface

  while (pInterface != &pDev->UsbInterfaceList) {
    pUsbInterface = GET_USB_INTERFACE_FROM_ENTRY(pInterface);
    USBH_ASSERT_MAGIC(pUsbInterface, USB_INTERFACE);
    pInterface = USBH_DLIST_GetNext(pInterface);
    pEPList    = USBH_DLIST_GetNext(&pUsbInterface->UsbEndpointList); // For each endpoint

    while (pEPList != &pUsbInterface->UsbEndpointList) {
      pUsbEndpoint = GET_USB_ENDPOINT_FROM_ENTRY(pEPList);
      USBH_ASSERT_MAGIC(pUsbEndpoint, USB_ENDPOINT);
      pEPList = USBH_DLIST_GetNext(pEPList);
      if (pUsbEndpoint->UrbCount > 0) {
        pDriver->pfAbortEndpoint(pUsbEndpoint->hEP);
      }
    }
  }
}

/*********************************************************************
*
*       _EnumPrepareGetDescReq
*
*  Function description
*/
static void _EnumPrepareGetDescReq(USB_DEVICE * pDev, U8 DescType, U8 DescIndex, U16 LanguageID, U16 RequestLength, void * pBuffer) {
  USBH_URB * pUrb = &pDev->EnumUrb;

  USBH_ZERO_MEMORY(pUrb, sizeof(USBH_URB));
  pUrb->Header.Function                      = USBH_FUNCTION_CONTROL_REQUEST;
  pUrb->Request.ControlRequest.Setup.Type    = 0x80; // STD, IN, device
  pUrb->Request.ControlRequest.Setup.Request = USB_REQ_GET_DESCRIPTOR;
  pUrb->Request.ControlRequest.Setup.Value   = (U16)((DescType << 8) | DescIndex);
  pUrb->Request.ControlRequest.Setup.Index   = LanguageID;
  pUrb->Request.ControlRequest.Setup.Length  = RequestLength;
  pUrb->Request.ControlRequest.pBuffer       = pBuffer;
  pUrb->Request.ControlRequest.Length        = RequestLength;
}

/*********************************************************************
*
*       _EnumPrepareSubmitSetConfiguration
*
*  Function description
*/
static void _EnumPrepareSubmitSetConfiguration(USB_DEVICE * pDev) {
  USBH_STATUS   Status;
  USBH_URB    * pUrb = &pDev->EnumUrb;

  USBH_ZERO_MEMORY(pUrb, sizeof(USBH_URB));
  pUrb->Header.Function                      = USBH_FUNCTION_CONTROL_REQUEST;
  pUrb->Request.ControlRequest.Setup.Type    = 0x00; // STD, OUT, device
  pUrb->Request.ControlRequest.Setup.Request = USB_REQ_SET_CONFIGURATION;
  pUrb->Request.ControlRequest.Setup.Value   = USBH_GetUcharFromDesc(pDev->pConfigDescriptor, 5);
  pDev->EnumState                            = DEV_ENUM_SET_CONFIGURATION;
  Status                                    = USBH_URB_SubStateSubmitRequest(&pDev->SubState, pUrb, DEFAULT_SETUP_TIMEOUT, pDev);
  if (Status != USBH_STATUS_PENDING) {
    USBH_WARN((USBH_MTYPE_DEVICE, "Device Notification Error:  _EnumPrepareSubmitSetConfiguration:USBH_URB_SubStateSubmitRequest failed %08x", Status));
    DEC_REF(pDev); // delete the initial reference
  }
}

/*********************************************************************
*
*       _ProcessEnumDisableParentHubPortCompletion
*
*  Function description
*/
static void _ProcessEnumDisableParentHubPortCompletion(void * usbDevice) {
  USB_DEVICE * pDev;
  USBH_WARN((USBH_MTYPE_DEVICE, "Device Notification Warning:  EnumPortErrorCompletion")); // Port error info
  pDev = (USB_DEVICE * )usbDevice;
  USBH_ASSERT_MAGIC(pDev, USB_DEVICE);
  if (pDev->EnumUrb.Header.Status != USBH_STATUS_SUCCESS) {
    USBH_WARN((USBH_MTYPE_DEVICE, "Device Notification Error:  EnumPortErrorCompletion %08x", pDev->EnumUrb.Header.Status));
  }
  DEC_REF(pDev); // Release sub state reference
  DEC_REF(pDev); // Delete the initial reference
}

/*********************************************************************
*
*       _CheckParentPortPower
*
*  Function description
*    Returns TRUE if the power supply of the USB device ok.
*    Returns also true if the device is low powered on an low powered parent port.
*    During the hub enumeration this case is checked an second time!
*/
static USBH_BOOL _CheckParentPortPower(USB_DEVICE * pDev) {
  int    power;
  USBH_BOOL HighPower;
  USBH_ASSERT_MAGIC(pDev->pParentPort, USBH_HUB_PORT);

  power = pDev->pConfigDescriptor[USB_CONFIGURATION_DESCRIPTOR_POWER_INDEX] << 1; // Bus powered device
  if (power >= 500) {
    HighPower = TRUE;
  } else {
    HighPower = FALSE;
  }
  if (pDev->pParentPort->HighPowerFlag) { // High powered port always allowed
    return TRUE;
  } else {                              // On low powered port
    if (HighPower) {
      return FALSE;
    } else {
      return TRUE; // Low power device on low power port
    }
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
*       USBH_CreateNewUsbDevice
*
*  Function description
*    Allocates device object and makes an basic initialization. Set the reference counter to one. Set the pHostController pointer.
*    Initialize all dlists and needed IDs. In the default endpoint the URB list is initialized and a pointer to this object is set.
*/
USB_DEVICE * USBH_CreateNewUsbDevice(USBH_HOST_CONTROLLER * pHostController) {
  USB_DEVICE * pDev;

  USBH_ASSERT_MAGIC(pHostController, USBH_HOST_CONTROLLER);
  USBH_LOG((USBH_MTYPE_DEVICE, "Device Notification:  USBH_CreateNewUsbDevice!"));
  pDev = (USB_DEVICE *)USBH_Malloc(sizeof(USB_DEVICE));
  if (NULL == pDev) {
    USBH_WARN((USBH_MTYPE_DEVICE, "Device Notification Error:  USBH_CreateNewUsbDevice: USBH_malloc!"));
    return NULL;
  }
  USBH_ZERO_MEMORY(pDev, sizeof(USB_DEVICE));
  IFDBG(pDev->Magic = USB_DEVICE_MAGIC);
  pDev->pHostController = pHostController;
  USBH_DLIST_Init(&pDev->UsbInterfaceList);
  pDev->DeviceId       = USBH_BD_GetNextDeviceId();
  HC_INC_REF(pHostController); // Add a reference to the host controller
  INC_REF(pDev);               // Initial refcount
  // The sub state machine increments the reference count of the device before submitting the request
  if (USBH_URB_SubStateInit(&pDev->SubState, pHostController, &pDev->DefaultEp.hEP, USBH_BD_ProcessEnum, pDev)) {
    // On error
    USBH_WARN((USBH_MTYPE_DEVICE, "Device Notification Error:  USBH_CreateNewUsbDevice: USBH_URB_SubStateInit failed!"));
    DEC_REF(pDev); // Device is deleted
    return NULL;
  }
  pDev->EnumState = DEV_ENUM_IDLE; // default basic initialization
  // Init by zero memory
  //pDev->EnumUrb                = NULL;
  //pDev->pCtrlTransferBuffer     = NULL;
  //pDev->CtrlTransferBufferSize = 0;
  //pDev->MaxFifoSize            = 0;
  pDev->DefaultEp.pUsbDevice = pDev;
  return pDev;
}

/*********************************************************************
*
*       USBH_MarkDeviceAsRemoved
*
*  Function description
*/
void USBH_MarkDeviceAsRemoved(USB_DEVICE * pDev) {
  USBH_HOST_DRIVER * pDriver;
  USBH_ASSERT_MAGIC(pDev, USB_DEVICE);
  USBH_LOG((USBH_MTYPE_DEVICE, "Device Notification:  MarkDeviceAsRemoved pDev-addr: %u!", pDev->UsbAddress));
  if (pDev->State == DEV_STATE_REMOVED) {
    USBH_WARN((USBH_MTYPE_DEVICE, "Device Notification Warning:  USBH_MarkDeviceAsRemoved pDev-addr: %u already removed!", pDev->UsbAddress));
    return;
  }
  pDev->State = DEV_STATE_REMOVED;
  pDriver     = pDev->pHostController->pDriver;
  USBH_BD_ProcessDevicePnpNotifications(pDev, USBH_REMOVE_DEVICE);
  // URB's on default endpoint
  pDriver->pfAbortEndpoint(pDev->DefaultEp.hEP);
  _AbortDeviceEndpoints(pDev);
  USBH_HC_RemoveDeviceFromList(pDev); // Remove from the list in the host controller, it is not found during enumerations
  if (pDev->pParentPort != NULL) {  // Delete the link between the hub port and the device in both directions
    pDev->pParentPort->Device = NULL;
  }
  pDev->pParentPort = NULL;
  DEC_REF(pDev); // delete the initial reference
}

/*********************************************************************
*
*       USBH_MarkParentAndChildDevicesAsRemoved
*
*  Function description
*    Marks the device and all child devices if the device is an hub
*    as removed. If an device already removed then nothing is done.
*/
void USBH_MarkParentAndChildDevicesAsRemoved(USB_DEVICE * usbDev) {
  USBH_ASSERT_MAGIC(usbDev, USB_DEVICE);
  USBH_LOG((USBH_MTYPE_DEVICE, "Device Notification:  USBH_MarkParentAndChildDevicesAsRemoved pDev-addr: %u!", usbDev->UsbAddress));
  if (usbDev->State == DEV_STATE_REMOVED) {
    // Device already removed
    USBH_WARN((USBH_MTYPE_DEVICE, "Device Notification Warning:  USBH_MarkParentAndChildDevicesAsRemoved pDev-addr: %u already removed!", usbDev->UsbAddress));
    return;
  }
  if (USBH_Global.Config.SupportExternalHubs) {
    if (NULL != usbDev->pUsbHub) { // Device is a hub
      USBH_DLIST        child_dev_list;
      int          ct;
      USB_DEVICE * pDev;
      USBH_DLIST *       dev_entry;
      USBH_DLIST_Init(&child_dev_list);
      ct = USBH_BD_HubBuildChildDeviceList(usbDev, &child_dev_list);
      if (ct) {
        for (; ;) { // Remove all devices, start with the tail of the port tree list
          dev_entry = USBH_DLIST_GetPrev(&child_dev_list);
          if (dev_entry != &child_dev_list) {
            pDev = GET_USB_DEVICE_FROM_TEMP_ENTRY(dev_entry);
            USBH_ASSERT_MAGIC(pDev, USB_DEVICE);
            // Remove from the this temporary list before the device is deleted
            USBH_DLIST_RemoveEntry(dev_entry);
            USBH_MarkDeviceAsRemoved(pDev);
            ct--;
          } else { // List is empty
            break;
          }
        }
        USBH_ASSERT(0 == ct && USBH_DLIST_IsEmpty(&child_dev_list));
      } else {
        USBH_WARN((USBH_MTYPE_DEVICE, "Device Notification Error:  USBH_MarkParentAndChildDevicesAsRemoved USBH_BD_HubBuildChildDeviceList!"));
      }
    } else {
      USBH_MarkDeviceAsRemoved(usbDev);
    }
  } else {
    USBH_MarkDeviceAsRemoved(usbDev);
  }

}

/*********************************************************************
*
*       USBH_DeleteDevice
*
*  Function description
*/
void USBH_DeleteDevice(USB_DEVICE * pDev) {
  USBH_LOG((USBH_MTYPE_DEVICE, "Device Notification:  USBH_DeleteDevice pDev-addr: %u!", pDev->UsbAddress));
  USBH_URB_SubStateExit(&pDev->SubState);

  if (USBH_Global.Config.SupportExternalHubs) {
    if (NULL != pDev->pUsbHub) {
      USBH_BD_DeleteHub(pDev->pUsbHub);
      pDev->pUsbHub = NULL;
    }
  }
  USBH_DeleteInterfaces(pDev);                   // Delete all interfaces, endpoints and notify the application of a remove event
  USBH_BD_ReleaseDefaultEndpoint(&pDev->DefaultEp); // Release the default endpoint if any
  if (pDev->pCtrlTransferBuffer != NULL) {
    USBH_URB_BufferFreeTransferBuffer(pDev->pCtrlTransferBuffer);
  }
  if (pDev->pConfigDescriptor != NULL) {
    USBH_Free(pDev->pConfigDescriptor);
  }
  if (pDev->pSerialNumber != NULL) {
    USBH_Free(pDev->pSerialNumber);
  }
  HC_DEC_REF(pDev->pHostController); // Release the reference of the ost controller
  IFDBG(pDev->Magic = 0);
  USBH_Free(pDev);
}

/*********************************************************************
*
*       USBH_CheckCtrlTransferBuffer
*
*  Function description
*/
int USBH_CheckCtrlTransferBuffer(USB_DEVICE * pDev, U16 RequestLength) {
  if (pDev->CtrlTransferBufferSize < RequestLength) {
    if (pDev->pCtrlTransferBuffer != NULL) {
      USBH_URB_BufferFreeTransferBuffer(pDev->pCtrlTransferBuffer);
      pDev->pCtrlTransferBuffer = NULL;
    }
    // Allocate a new buffer
    pDev->CtrlTransferBufferSize = (unsigned int)USBH_MAX(DEFAULT_TRANSFERBUFFER_SIZE, RequestLength);
    pDev->pCtrlTransferBuffer     = USBH_URB_BufferAllocateTransferBuffer(pDev->CtrlTransferBufferSize);

    //pDev->pCtrlTransferBuffer     = UbdAllocateTransferBuffer      (pDev->CtrlTransferBufferSize);
    if (pDev->pCtrlTransferBuffer == NULL) {
      USBH_WARN((USBH_MTYPE_DEVICE, "Device Notification Error:  USBH_CheckCtrlTransferBuffer: UbdAllocateTransferBuffer failed"));
      DEC_REF(pDev); // Delete the initial reference
      return 0;
    }
  }
  return 1;
}

/*********************************************************************
*
*       USBH_EnumParentPortRestart
*
*  Function description
*    Sets the parent port to PORT_RESTART, decrement the reference count of the USB
*    device and service all hubs. Can only be called during device enumeration.
*
*/
void USBH_EnumParentPortRestart(USB_DEVICE * pDev, USBH_STATUS Status) {
  USBH_HUB_PORT * pParentPort;
  USBH_ASSERT_MAGIC(pDev, USB_DEVICE);

  if (pDev->pParentPort != NULL) { // Parent port is available
    pParentPort = pDev->pParentPort;
    USBH_ASSERT_MAGIC(pParentPort, USBH_HUB_PORT);
    USBH_LOG((USBH_MTYPE_DEVICE, "Device Notification:  USBH_EnumParentPortRestart: ref.ct.: %ld portnumber: %d portstate: %s", pDev->RefCount, (int)pParentPort->HubPortNumber, USBH_PortState2Str(pParentPort->PortState)));
    USBH_BD_SetPortState(pParentPort, PORT_RESTART); // Try to restart that port
    UbdSetEnumErrorNotificationProcessDeviceEnum(pParentPort, pDev->EnumState, Status, pDev->pUsbHub ? TRUE : FALSE);
  } else {
    USBH_WARN((USBH_MTYPE_DEVICE, "Device Notification Warning:  FATAL USBH_EnumParentPortRestart: pParentPort is NULL"));
  }
  DEC_REF(pDev);                           // Delete the initial reference
  USBH_HC_ServicePorts(pDev->pHostController); // Service all ports
}

/*********************************************************************
*
*       USBH_ProcessEnumPortError
*
*  Function description
*    On error during enumeration the parent port is disabled before the enumeration device is deleted.
*/
void USBH_ProcessEnumPortError(USB_DEVICE * pDev, USBH_STATUS EnumStatus) {
  USBH_HUB_PORT       * pParentPort;
  DEV_ENUM_STATE   EnumState;
  int              HubFlag;
  USBH_ASSERT_MAGIC(pDev, USB_DEVICE);
  EnumState = pDev->EnumState; // Save device flags before the device is deleted
  if (NULL != pDev->pUsbHub) {
    HubFlag = TRUE;
  } else {
    HubFlag = FALSE;
  }
  if (pDev->pParentPort != NULL) { // Parent port is available
    pParentPort = pDev->pParentPort;
    USBH_ASSERT_MAGIC(pParentPort, USBH_HUB_PORT);
    USBH_WARN((USBH_MTYPE_DEVICE, "Device Notification Warning:  UbdEnumPortError: ref.ct.: %ld portnumber: %d portstate: %s", pDev->RefCount, (int)pParentPort->HubPortNumber, USBH_PortState2Str(pParentPort->PortState)));
    USBH_BD_SetPortState(pParentPort, PORT_ERROR);
    if (pParentPort->PortStatus & PORT_STATUS_ENABLED) {
      if (NULL != pParentPort->RootHub) { // On error disable the parent port
        USBH_HOST_DRIVER * pDriver;
        pDriver= pDev->pHostController->pDriver;
        pDriver->pfDisablePort(pDev->pHostController->hHostController, pParentPort->HubPortNumber); // Disable parent port: synchronous request delete the enum device object
        DEC_REF(pDev);                                                             // Delete the initial reference
      } else { 
        if (USBH_Global.Config.SupportExternalHubs)  {  // Parent hub port is an external port
          USBH_STATUS Status;
          USBH_ASSERT_MAGIC(pParentPort->ExtHub, USB_HUB);
          USBH_BD_HubPrepareClrFeatureReq(&pDev->EnumUrb, HDC_SELECTOR_PORT_ENABLE, pParentPort->HubPortNumber);
          // Use the enum device sub state field to install the EnumPortErrorCompletion routine
          USBH_URB_SubStateExit(&pDev->SubState);
          USBH_URB_SubStateInit(&pDev->SubState, pDev->pHostController, &pParentPort->ExtHub->pHubDevice->DefaultEp.hEP, // Parent hub ep0 handle
          _ProcessEnumDisableParentHubPortCompletion, pDev);
          Status = USBH_URB_SubStateSubmitRequest(&pDev->SubState, &pDev->EnumUrb, DEFAULT_SETUP_TIMEOUT, pDev);
          if (Status != USBH_STATUS_PENDING) {
            USBH_WARN((USBH_MTYPE_DEVICE, "Device Notification Error:  UbdEnumPortError: USBH_URB_SubStateSubmitRequest failed %08x", Status));
            DEC_REF(pDev); // Delete the previous sub state reference
            DEC_REF(pDev); // Delete the initial reference
          }
        }
      }
    } else {
      DEC_REF(pDev);                                                                             // Delete the initial reference
    }
    UbdSetEnumErrorNotificationProcessDeviceEnum(pParentPort, EnumState, EnumStatus, HubFlag); // Parent port is set to PORT_ERROR, notify the user
  } else {
    USBH_WARN((USBH_MTYPE_DEVICE, "Device Notification Warning:  FATAL: parent port during device enumeraton is NULL"));
    DEC_REF(pDev); // delete the initial reference
  }
}

/*********************************************************************
*
*       USBH_BD_ProcessEnum
*
*  Function description
*    Is called with an unlinked device object, this means this device is not in the host controllers device list.
*    The hub port enumDevice element is also NULL, because the device has  an unique USB address so another port
*    reset state machine can run during this device enumeration! If enumeration fails this state machine must
*    delete the device object. Stops on error and disables the parent port.
*/
void USBH_BD_ProcessEnum(void * pUsbDevice) {
  U16           RequestLength;
  USBH_STATUS   Status;
  U8            SerialNumberIndex;
  USBH_URB    * pUrb;
  USB_DEVICE  * pEnumDev;
  USBH_HUB_PORT    * pParentPort;

  INC_RECURSIVE_CT(USBH_BD_ProcessEnum);
  pEnumDev = (USB_DEVICE * )pUsbDevice;
  USBH_ASSERT_MAGIC(pEnumDev, USB_DEVICE);
  pParentPort = pEnumDev->pParentPort;
  USBH_ASSERT_MAGIC(pParentPort, USBH_HUB_PORT);
  // Set the devices enumeration state to DEV_ENUM_REMOVED if host is removed, the port not enabled or the hub does not work
  if (pEnumDev->pHostController->State == HC_REMOVED) { // Root hub removed
    USBH_LOG((USBH_MTYPE_DEVICE, "Device Notification:  INFO USBH_BD_ProcessEnum: host removed: Set enum state to DEV_ENUM_REMOVED"));
    pEnumDev->EnumState = DEV_ENUM_REMOVED;
  } else {
    if (pParentPort->PortState != PORT_ENABLED) {
      USBH_LOG((USBH_MTYPE_DEVICE, "Device Notification:  INFO USBH_BD_ProcessEnum: parent port not enabled: Set enum state to DEV_ENUM_REMOVED"));
      pEnumDev->EnumState = DEV_ENUM_REMOVED;
    } else {
      if (USBH_Global.Config.SupportExternalHubs) {
        if (NULL != pParentPort->ExtHub) {
          if (NULL != pParentPort->ExtHub->pHubDevice) {
            if (pParentPort->ExtHub->pHubDevice->State < DEV_STATE_WORKING) {
              USBH_LOG((USBH_MTYPE_DEVICE, "Device Notification:  INFO USBH_BD_ProcessEnum: hub does not work: Set enum state to DEV_ENUM_REMOVED"));
              pEnumDev->EnumState = DEV_ENUM_REMOVED;
            }
          }
        }
      }
    }
  }
  USBH_LOG((USBH_MTYPE_DEVICE, "Device Notification:  USBH_BD_ProcessEnum %s Dev.ref.ct: %ld", USBH_EnumState2Str(pEnumDev->EnumState), pEnumDev->RefCount));
  pUrb = &pEnumDev->EnumUrb;
  switch (pEnumDev->EnumState) {
  case DEV_ENUM_RESTART:
    pEnumDev->EnumState = DEV_ENUM_START; // Timeout elapsed after restart
  // Fall trough
  case DEV_ENUM_START:
    switch (pEnumDev->DeviceSpeed) {                    // Calculate the max FIFO size and next state
    case USBH_LOW_SPEED:
      pEnumDev->EnumState   = DEV_ENUM_GET_DEVICE_DESC; // Must be always 8 bytes
      pEnumDev->MaxFifoSize = 8;
      RequestLength        = USB_DEVICE_DESCRIPTOR_LENGTH;  // sizeof(DEVICE_DESCIPTOR)
      break;
    case USBH_FULL_SPEED:
      pEnumDev->EnumState   = DEV_ENUM_GET_DEVICE_DESC_PART; // Can be 8, 16, 32, or 64 bytes, start with 8 bytes and retry with the real size
      pEnumDev->MaxFifoSize = 8;
      RequestLength        = 8;                                  // Size of expected FIFO
      break;
    case USBH_HIGH_SPEED:
      pEnumDev->EnumState   = DEV_ENUM_GET_DEVICE_DESC;
      pEnumDev->MaxFifoSize = 64;
      RequestLength        = USB_DEVICE_DESCRIPTOR_LENGTH; // sizeof(DEVICE_DESCIPTOR)
      break;
    default:
      RequestLength = 0;
      USBH_ASSERT0;
    }
    if (pEnumDev->DefaultEp.hEP == NULL) {
      Status = USBH_BD_InitDefaultEndpoint(pEnumDev);
      if (Status != USBH_STATUS_SUCCESS) {
        USBH_WARN((USBH_MTYPE_DEVICE, "Device Notification Error:  USBH_BD_ProcessEnum:InitDefaultEndpoint failed"));
        USBH_ProcessEnumPortError(pEnumDev, Status);
        goto exit;
      }
    }
    if (!USBH_CheckCtrlTransferBuffer(pEnumDev, RequestLength)) {
      goto exit;
    }
    // Prepare an URB
    _EnumPrepareGetDescReq(pEnumDev, USB_DEVICE_DESCRIPTOR_TYPE, 0, 0, RequestLength, pEnumDev->pCtrlTransferBuffer);
    Status = USBH_URB_SubStateSubmitRequest(&pEnumDev->SubState, pUrb, DEFAULT_SETUP_TIMEOUT, pEnumDev);
    if (Status != USBH_STATUS_PENDING) {
      USBH_WARN((USBH_MTYPE_DEVICE, "Device Notification Error:  USBH_BD_ProcessEnum: DEV_ENUM_START USBH_URB_SubStateSubmitRequest failed %08x", Status));
      USBH_ProcessEnumPortError(pEnumDev, Status);
    }
    break;
  case DEV_ENUM_GET_DEVICE_DESC_PART:
    if (pUrb->Header.Status != USBH_STATUS_SUCCESS) {
      USBH_WARN((USBH_MTYPE_DEVICE, "Device Notification Error:  USBH_BD_ProcessEnum:DEV_ENUM_GET_DEVICE_DESC_PART failed %08x", pUrb->Header.Status));
      USBH_EnumParentPortRestart(pEnumDev, pUrb->Header.Status);
    } else {                                                                // Status success
      pEnumDev->MaxFifoSize = * (((char *)pEnumDev->pCtrlTransferBuffer) + 7); // Extract the EP0 FIFO size
      RequestLength        = USB_DEVICE_DESCRIPTOR_LENGTH;                  // sizeof(DEVICE_DESCIPTOR) / Get the full device descriptor
      pEnumDev->EnumState   = DEV_ENUM_GET_DEVICE_DESC;                      // Set next state
      if (pEnumDev->MaxFifoSize != 8) {                                      // Set the new max FIFO size, setup a new endpoint
        USBH_BD_ReleaseDefaultEndpoint(&pEnumDev->DefaultEp);                     // Update the new FIFO size
        Status = USBH_BD_InitDefaultEndpoint(pEnumDev);
        if (Status != USBH_STATUS_SUCCESS) {
          USBH_WARN((USBH_MTYPE_DEVICE, "Device Notification Error:  USBH_BD_ProcessEnum:AddEndpoint failed"));
          USBH_ProcessEnumPortError(pEnumDev, Status);
          goto exit;
        }
      }
      // Prepare an URB
      _EnumPrepareGetDescReq(pEnumDev, USB_DEVICE_DESCRIPTOR_TYPE, 0, 0, RequestLength, pEnumDev->pCtrlTransferBuffer);
      Status = USBH_URB_SubStateSubmitRequest(&pEnumDev->SubState, pUrb, DEFAULT_SETUP_TIMEOUT, pEnumDev);
      if (Status != USBH_STATUS_PENDING) {
        USBH_WARN((USBH_MTYPE_DEVICE, "Device Notification Error:   USBH_BD_ProcessEnum: DEV_ENUM_GET_DEVICE_DESC_PART USBH_URB_SubStateSubmitRequest failed %08x", Status));
        USBH_ProcessEnumPortError(pEnumDev, Status);
        goto exit;
      }
    }
    break;
  case DEV_ENUM_GET_DEVICE_DESC:
    if (pUrb->Header.Status != USBH_STATUS_SUCCESS || USB_DEVICE_DESCRIPTOR_LENGTH != pUrb->Request.ControlRequest.Length) {
      USBH_WARN((USBH_MTYPE_DEVICE, "Device Notification Error:  USBH_BD_ProcessEnum:DEV_ENUM_GET_DEVICE_DESC failed st:%08x, len:%d ", pUrb->Header.Status, pUrb->Request.ControlRequest.Length));
      USBH_EnumParentPortRestart(pEnumDev, USBH_STATUS_INVALID_DESCRIPTOR);
    } else {                                                                                                   // Status success
      _ConvDeviceDesc((U8 * )pEnumDev->pCtrlTransferBuffer, &pEnumDev->DeviceDescriptor);                         // Store the device descriptor in a typed format
      USBH_MEMCPY(pEnumDev->aDeviceDescriptorBuffer, pEnumDev->pCtrlTransferBuffer, USB_DEVICE_DESCRIPTOR_LENGTH); // Store the device descriptor in a raw format
      // Prepare an URB
      _EnumPrepareGetDescReq(pEnumDev, USB_CONFIGURATION_DESCRIPTOR_TYPE, pEnumDev->ConfigurationIndex, 0, USB_CONFIGURATION_DESCRIPTOR_LENGTH, pEnumDev->pCtrlTransferBuffer);
      pEnumDev->EnumState = DEV_ENUM_GET_CONFIG_DESC_PART;
      Status = USBH_URB_SubStateSubmitRequest(&pEnumDev->SubState, pUrb, DEFAULT_SETUP_TIMEOUT, pEnumDev);
      if (Status != USBH_STATUS_PENDING) {
        USBH_WARN((USBH_MTYPE_DEVICE, "Device Notification Error:   USBH_BD_ProcessEnum: DEV_ENUM_GET_DEVICE_DESC USBH_URB_SubStateSubmitRequest failed %08x", Status));
        USBH_ProcessEnumPortError(pEnumDev, Status);
        goto exit;
      }
    }
    break;
  case DEV_ENUM_GET_CONFIG_DESC_PART:
    if (pUrb->Header.Status != USBH_STATUS_SUCCESS || pUrb->Request.ControlRequest.Length != USB_CONFIGURATION_DESCRIPTOR_LENGTH) {
      USBH_WARN((USBH_MTYPE_DEVICE, "Device Notification Error:  USBH_BD_ProcessEnum:DEV_ENUM_GET_CONFIG_DESC_PART failed st:%08x, len:%d ", pUrb->Header.Status, pUrb->Request.ControlRequest.Length));
      USBH_EnumParentPortRestart(pEnumDev, USBH_STATUS_INVALID_DESCRIPTOR);
    } else { // Status success
      RequestLength = USBH_GetUshortFromDesc(pEnumDev->pCtrlTransferBuffer, 2);
      if (!USBH_CheckCtrlTransferBuffer(pEnumDev, RequestLength)) {
        goto exit;
      }
      USBH_ASSERT_PTR(pEnumDev->pCtrlTransferBuffer);
      // Prepare an URB
      _EnumPrepareGetDescReq(pEnumDev, USB_CONFIGURATION_DESCRIPTOR_TYPE, pEnumDev->ConfigurationIndex, 0, RequestLength, pEnumDev->pCtrlTransferBuffer);
      pEnumDev->ConfigDescriptorSize = RequestLength;
      pEnumDev->EnumState = DEV_ENUM_GET_CONFIG_DESC;
      Status = USBH_URB_SubStateSubmitRequest(&pEnumDev->SubState, pUrb, DEFAULT_SETUP_TIMEOUT, pEnumDev);
      if (Status != USBH_STATUS_PENDING) {
        USBH_WARN((USBH_MTYPE_DEVICE, "Device Notification Error:   USBH_BD_ProcessEnum: DEV_ENUM_GET_CONFIG_DESC_PART SubmitRequest failed 0x%08x", Status));
        USBH_ProcessEnumPortError(pEnumDev, Status);
        goto exit;
      }
    }
    break;
  case DEV_ENUM_GET_CONFIG_DESC:
    if (pUrb->Header.Status != USBH_STATUS_SUCCESS || pUrb->Request.ControlRequest.Length != pEnumDev->ConfigDescriptorSize) {
      USBH_WARN((USBH_MTYPE_DEVICE, "Device Notification Error:   USBH_BD_ProcessEnum:DEV_ENUM_GET_CONFIG_DESC failed st:%08x, exp.length:%d rcv.length:%d ", pUrb->Header.Status, pEnumDev->ConfigDescriptorSize, pUrb->Request.ControlRequest.Length));
      USBH_EnumParentPortRestart(pEnumDev, USBH_STATUS_INVALID_DESCRIPTOR);
    } else {                                   // Status success
      if (pEnumDev->pConfigDescriptor != NULL) { // Save the configuration descriptor
        USBH_Free(pEnumDev->pConfigDescriptor);
      }
      pEnumDev->pConfigDescriptor = (U8 *)USBH_Malloc(pEnumDev->ConfigDescriptorSize);
      if (pEnumDev->pConfigDescriptor == NULL) {
        USBH_WARN((USBH_MTYPE_DEVICE, "Device Notification Error:  USBH_BD_ProcessEnum:DEV_ENUM_GET_CONFIG_DESC USBH_malloc %d failed ", pEnumDev->ConfigDescriptorSize));
        USBH_ProcessEnumPortError(pEnumDev, USBH_STATUS_MEMORY);
        goto exit;
      }
      USBH_MEMCPY(pEnumDev->pConfigDescriptor, pEnumDev->pCtrlTransferBuffer, pUrb->Request.ControlRequest.Length);
      if (!_CheckParentPortPower(pEnumDev)) { // Check power consumption
        USBH_WARN((USBH_MTYPE_DEVICE, "Device Notification Error:  USBH_BD_ProcessEnum: _CheckParentPortPower: Parent port power exceeded!"));
        USBH_ProcessEnumPortError(pEnumDev, USBH_STATUS_PORT);
        goto exit;
      }
      SerialNumberIndex = pEnumDev->DeviceDescriptor.iSerialNumber;
      if (SerialNumberIndex == 0) {
        _EnumPrepareSubmitSetConfiguration(pEnumDev);
      } else {
        // Prepare an URB for the language ID
        _EnumPrepareGetDescReq(pEnumDev, USB_STRING_DESCRIPTOR_TYPE, 0, 0, 255, pEnumDev->pCtrlTransferBuffer);
        pEnumDev->EnumState = DEV_ENUM_GET_LANG_ID;
        Status = USBH_URB_SubStateSubmitRequest(&pEnumDev->SubState, pUrb, DEFAULT_SETUP_TIMEOUT, pEnumDev);

        if (Status != USBH_STATUS_PENDING) {
          USBH_WARN((USBH_MTYPE_DEVICE, "Device Notification Error:   USBH_BD_ProcessEnum:DEV_ENUM_GET_CONFIG_DESC_PART USBH_URB_SubStateSubmitRequest failed %08x", Status));
          USBH_ProcessEnumPortError(pEnumDev, Status);
          goto exit;
        }
      }
    }
    break;
  case DEV_ENUM_GET_LANG_ID:
    if (pUrb->Header.Status != USBH_STATUS_SUCCESS || pUrb->Request.ControlRequest.Length < 4) {
      USBH_WARN((USBH_MTYPE_DEVICE, "Device Notification Error:  USBH_BD_ProcessEnum:DEV_ENUM_GET_LANG_ID failed st:%08x, len:%d ", pUrb->Header.Status, pUrb->Request.ControlRequest.Length));
    } else { // Status success
      pEnumDev->LanguageId = USBH_GetUshortFromDesc(pEnumDev->pCtrlTransferBuffer, 2);
    }
    SerialNumberIndex = pEnumDev->DeviceDescriptor.iSerialNumber; // The language ID is now 0 or the first ID reported by the device
    // Prepare an URB
    _EnumPrepareGetDescReq(pEnumDev, USB_STRING_DESCRIPTOR_TYPE, SerialNumberIndex, pEnumDev->LanguageId, 255, pEnumDev->pCtrlTransferBuffer);
    pEnumDev->EnumState = DEV_ENUM_GET_SERIAL_DESC;
    Status = USBH_URB_SubStateSubmitRequest(&pEnumDev->SubState, pUrb, DEFAULT_SETUP_TIMEOUT, pEnumDev);
    if (Status != USBH_STATUS_PENDING) {
      USBH_WARN((USBH_MTYPE_DEVICE, "Device Notification Error: USBH_BD_ProcessEnum: DEV_ENUM_GET_LANG_ID USBH_URB_SubStateSubmitRequest failed %08x", Status));
      USBH_ProcessEnumPortError(pEnumDev, Status);
      goto exit;
    }
    break;
  case DEV_ENUM_GET_SERIAL_DESC:
    if (pUrb->Header.Status != USBH_STATUS_SUCCESS || pUrb->Request.ControlRequest.Length <= 2) {
      USBH_WARN((USBH_MTYPE_DEVICE, "Device Notification Error:  USBH_BD_ProcessEnum:DEV_ENUM_GET_SERIAL_DESC failed st:%08x, len:%d ", pUrb->Header.Status, pUrb->Request.ControlRequest.Length));
    } else {                                // Status success
      if (pEnumDev->pSerialNumber != NULL) { // Copy the serial number descriptor
        USBH_Free(pEnumDev->pSerialNumber);
      }
      pEnumDev->SerialNumberSize = pUrb->Request.ControlRequest.Length - 2; // Don't copy the header
      pEnumDev->pSerialNumber     = (U8 *)USBH_Malloc(pEnumDev->SerialNumberSize);
      if (pEnumDev->pSerialNumber == NULL) {
        USBH_WARN((USBH_MTYPE_DEVICE, "Device Notification Error:  USBH_BD_ProcessEnum:USBH_malloc %d failed", pEnumDev->SerialNumberSize));
        USBH_ProcessEnumPortError(pEnumDev, USBH_STATUS_MEMORY);
        goto exit;
      }
      USBH_MEMCPY(pEnumDev->pSerialNumber, ((U8 * )(pEnumDev->pCtrlTransferBuffer)) + 2, pEnumDev->SerialNumberSize);
    }
    _EnumPrepareSubmitSetConfiguration(pEnumDev);
    break;
  case DEV_ENUM_SET_CONFIGURATION:
    if (pUrb->Header.Status != USBH_STATUS_SUCCESS) {
      USBH_WARN((USBH_MTYPE_DEVICE, "Device Notification Error:  USBH_BD_ProcessEnum:DEV_ENUM_SET_CONFIGURATION failed st:%08x", pUrb->Header.Status));
      USBH_EnumParentPortRestart(pEnumDev, pUrb->Header.Status);
    } else {
      if ((USBH_Global.Config.SupportExternalHubs) && pEnumDev->DeviceDescriptor.bDeviceClass == USB_DEVICE_CLASS_HUB) {
        USB_HUB * hub;
        // Device is an hub device, start the hub enumeration routine
        USBH_ASSERT(NULL == pEnumDev->pUsbHub); // Hub object is always unlinked
        hub = USBH_BD_AllocInitUsbHub(pEnumDev);
        if (NULL == hub) {
          USBH_WARN((USBH_MTYPE_DEVICE, "Device Notification Error:  USBH_BD_ProcessEnum: llocInitUsbHub failed"));
          USBH_ProcessEnumPortError(pEnumDev, USBH_STATUS_RESOURCES);
          goto exit;
        }
        // Link the hub to the device and start the hub initialization
        pEnumDev->pUsbHub = hub;
        pEnumDev->EnumState = DEV_ENUM_INIT_HUB;
        USBH_BD_StartHub(hub, pEnumDev->pfPostEnumFunction, pEnumDev->pPostEnumerationContext);
      } else {
        pEnumDev->EnumState = DEV_ENUM_IDLE; // Device on a root hub port
        if (pEnumDev->pfPostEnumFunction != NULL) {
          pEnumDev->pfPostEnumFunction(pEnumDev->pPostEnumerationContext);
        }
      }
    }
    break;
  case DEV_ENUM_INIT_HUB:
    USBH_ASSERT0; // Not allowed to call Process Enum when an USB hub is initialized
    break;
  case DEV_ENUM_REMOVED:
    USBH_ProcessEnumPortError(pEnumDev, USBH_STATUS_ERROR); // Invalid parent port state during device enumeration
    break;
  default:
    USBH_ASSERT0;
  } // Switch
  exit:
  ;
  DEC_RECURSIVE_CT(USBH_BD_ProcessEnum); // Only for testing
}

/*********************************************************************
*
*       USBH_StartEnumeration
*/
void USBH_StartEnumeration(USB_DEVICE * pDev, POST_ENUM_FUNC * PostEnumFunction, void * pContext) {
  USBH_ASSERT(pDev->EnumState == DEV_ENUM_IDLE);
  USBH_LOG((USBH_MTYPE_DEVICE, "Device Notification:  USBH_CreateNewUsbDevice!"));
  pDev->pfPostEnumFunction       = PostEnumFunction;
  pDev->pPostEnumerationContext = pContext;
  // Device is now enumerating
  pDev->EnumState              = DEV_ENUM_START;
  pDev->State                  = DEV_STATE_ENUMERATE;
  USBH_BD_ProcessEnum(pDev);
}

/*********************************************************************
*
*       USBH_CreateInterfaces
*
*  Function description
*    Create all interfaces and endpoints, create PnP notification
*/
void USBH_CreateInterfaces(void * pContext) {
  USB_DEVICE    * pDev     = (USB_DEVICE * )pContext;
  USB_INTERFACE * pUsbInterface;
  U8            * pDesc    = NULL;
  USBH_STATUS     Status;

  while ((pDesc = USBH_GetNextInterfaceDesc(pDev, pDesc, 0xff, 0)) != NULL) {
    pUsbInterface = USBH_BD_NewUsbInterface(pDev);
    if (pUsbInterface == NULL) {
      USBH_WARN((USBH_MTYPE_DEVICE, "Device Notification Error:  USBH_CreateInterfaces, USBH_BD_NewUsbInterface failed"));
      break;
    }
    pUsbInterface->pInterfaceDescriptor        = pDesc;
    pUsbInterface->pAlternateSettingDescriptor = pDesc;                             // Set the alternate setting descriptor to the same descriptor
    Status                                   = USBH_BD_CreateEndpoints(pUsbInterface); // Create the endpoints
    if (Status) {                                                                // On error
      USBH_BD_DeleteUsbInterface(pUsbInterface);
      continue;
    } else {
      USBH_BD_AddUsbInterface(pUsbInterface);                                          // Add the interfaces to the list
    }
  }
  USBH_AddUsbDevice(pDev);
  USBH_BD_ProcessDevicePnpNotifications(pDev, USBH_ADD_DEVICE);
}

/*********************************************************************
*
*       USBH_DeleteInterfaces
*
*  Function description
*/
void USBH_DeleteInterfaces(USB_DEVICE * pDev) {
  USB_INTERFACE * pUsbInterface;
  USBH_DLIST         * e;
  e = USBH_DLIST_GetNext(&pDev->UsbInterfaceList);
  while (e != &pDev->UsbInterfaceList) {
    pUsbInterface = GET_USB_INTERFACE_FROM_ENTRY(e);
    USBH_ASSERT_MAGIC(pUsbInterface, USB_INTERFACE);
    e = USBH_DLIST_GetNext(e);
    USBH_BD_RemoveUsbInterface(pUsbInterface);
    USBH_BD_DeleteUsbInterface(pUsbInterface);
  }
}

/*********************************************************************
*
*       USBH_GetNextInterfaceDesc
*
*  Function description
*    Returns NULL or the pointer of the descriptor
*
*  Parameters:
*    pStart:            Can be NULL
*    InterfaceNumber:  0xff for don't care
*    AlternateSetting: 0xff for don't care
*/
U8 * USBH_GetNextInterfaceDesc(USB_DEVICE * pDev, U8 * pStart, U8 InterfaceNumber, unsigned int AlternateSetting) {
  U8 * pDesc;
  U8 * pLimit = pDev->pConfigDescriptor + pDev->ConfigDescriptorSize;
  U8 * p      = NULL;

  if (pStart == NULL) {
    pDesc = pDev->pConfigDescriptor;
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
*       USBH_GetNextEndpointDesc
*
*  Function description
*    Returns NULL or the pointer of the descriptor
*
*  Parameters:
*    pStart:    // Must be a pointer to an interface descriptor
*    Endpoint: // Endpoint address with direction bit 0xff for don't care
*/
U8 * USBH_GetNextEndpointDesc(USB_DEVICE * pDev, U8 * pStart, U8 Endpoint) {
  U8 * pDesc;
  U8 * pLimit = pDev->pConfigDescriptor + pDev->ConfigDescriptorSize;
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
*       USBH_GetUshortFromDesc
*
*  Function description
*/
U16 USBH_GetUshortFromDesc(void * pBuffer, U16 Offset) {
  U8  * p = (U8 *)pBuffer;
  U16   v = (U16)((*(p + Offset)) | ((*(p + Offset + 1)) << 8));
  return v;
}

/*********************************************************************
*
*       USBH_GetUcharFromDesc
*
*  Function description
*/
U8 USBH_GetUcharFromDesc(void * pBuffer, U16 Offset) {
  U8 * p = (U8 *)pBuffer;
  return *(p + Offset);
}

/*********************************************************************
*
*       _OnSubmitUrbCompletion
*
*  Function description
*/
static void _OnSubmitUrbCompletion(USBH_URB * pUrb) {
  USBH_OS_EVENT_OBJ * pEvent;

  pEvent     = (USBH_OS_EVENT_OBJ *)pUrb->Header.pContext;
  USBH_LOG((USBH_MTYPE_DEVICE, "Device: _OnSubmitUrbCompletion URB st: 0x%08x",pUrb->Header.Status));
  USBH_OS_SetEvent(pEvent);
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
USBH_STATUS USBH_GetDeviceDescriptor(USBH_INTERFACE_HANDLE Handle, U8 * pDescriptor, unsigned * pBufferSize) {
  USB_INTERFACE * pUsbInterface;
  USB_DEVICE    * pDev;
  USBH_STATUS     Status = USBH_STATUS_SUCCESS;

  USBH_LOG((USBH_MTYPE_UBD, "UBD: USBH_GetDeviceDescriptor"));
  pUsbInterface = (USB_INTERFACE * )Handle;
  USBH_ASSERT_MAGIC(pUsbInterface, USB_INTERFACE);
  pDev          = pUsbInterface->pDevice;
  USBH_ASSERT_MAGIC(pDev, USB_DEVICE);
  if (pDev->State < DEV_STATE_WORKING) {
    return USBH_STATUS_DEVICE_REMOVED;
  }
  *pBufferSize = USBH_MIN(*pBufferSize, USB_DEVICE_DESCRIPTOR_LENGTH);
  USBH_MEMCPY(pDescriptor, pDev->aDeviceDescriptorBuffer, *pBufferSize);
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
USBH_STATUS USBH_GetCurrentConfigurationDescriptor(USBH_INTERFACE_HANDLE Handle, U8 * pDescriptor, unsigned * pBufferSize) {
  USB_INTERFACE * pUsbInterface;
  USB_DEVICE    * pDev;
  USBH_STATUS     Status = USBH_STATUS_SUCCESS;

  USBH_LOG((USBH_MTYPE_UBD, "UBD: USBH_GetCurrentConfigurationDescriptor"));
  pUsbInterface = (USB_INTERFACE *)Handle;
  USBH_ASSERT_MAGIC(pUsbInterface, USB_INTERFACE);
  pDev          = pUsbInterface->pDevice;
  USBH_ASSERT_MAGIC(pDev, USB_DEVICE);
  if (pDev->State < DEV_STATE_WORKING) {
    return USBH_STATUS_DEVICE_REMOVED;
  }
  *pBufferSize = USBH_MIN(*pBufferSize, pDev->ConfigDescriptorSize);
  USBH_MEMCPY(pDescriptor, pDev->pConfigDescriptor, *pBufferSize);
  return Status;
}

/*********************************************************************
*
*       USBH_GetInterfaceDescriptor
*
*  Function description
*    Returns the Interface descriptor for a given alternate setting.
*/
USBH_STATUS USBH_GetInterfaceDescriptor(USBH_INTERFACE_HANDLE Handle, U8 AlternateSetting, U8 * pBuffer, unsigned * pBufferSize) {
  USB_INTERFACE            * pUsbInterface;
  USB_DEVICE               * pDev;
  USBH_STATUS                Status = USBH_STATUS_SUCCESS;
  USB_INTERFACE_DESCRIPTOR * pInterfaceDescriptor;
  U8                       * pTemp;
  USBH_LOG((USBH_MTYPE_UBD, "UBD: USBH_GetInterfaceDescriptor: Alt Setting:%d", AlternateSetting));
  pUsbInterface = (USB_INTERFACE * )Handle;
  USBH_ASSERT_MAGIC(pUsbInterface, USB_INTERFACE);
  pDev          = pUsbInterface->pDevice;
  USBH_ASSERT_MAGIC(pDev, USB_DEVICE);
  if (pDev->State < DEV_STATE_WORKING) {
    return USBH_STATUS_DEVICE_REMOVED;
  }
  pInterfaceDescriptor = (USB_INTERFACE_DESCRIPTOR * )pUsbInterface->pInterfaceDescriptor;
  pTemp                = USBH_GetNextInterfaceDesc(pDev, NULL, pInterfaceDescriptor->bInterfaceNumber, AlternateSetting);
  if (NULL == pTemp) {
    USBH_WARN((USBH_MTYPE_DEVICE, "Device Notification Error: USBH_GetInterfaceDescriptor: USBH_GetNextInterfaceDesc!"));
    return USBH_STATUS_INVALID_PARAM;
  }
  *pBufferSize = USBH_MIN(*pBufferSize, USB_INTERFACE_DESCRIPTOR_LENGTH);
  USBH_MEMCPY(pBuffer, pTemp, *pBufferSize);
  return Status;
}

/*********************************************************************
*
*       USBH_SearchUsbInterface
*
*  Function description
*    Searches in the interface list of the device an interface that matches with iMask.
*
*  Return values:
*    On success: Pointer to the interface descriptor!
*    Else:       Error
*/
USBH_STATUS USBH_SearchUsbInterface(USB_DEVICE * pDev, USBH_INTERFACE_MASK * iMask, USB_INTERFACE ** ppUsbInterface) {
  USB_INTERFACE * pInterface;
  USBH_STATUS     Status = USBH_STATUS_INVALID_PARAM;
  USBH_DLIST         * pEntry;

  USBH_ASSERT_MAGIC(pDev, USB_DEVICE);
  pEntry = USBH_DLIST_GetNext(&pDev->UsbInterfaceList);
  while (pEntry != &pDev->UsbInterfaceList) {
    pInterface = GET_USB_INTERFACE_FROM_ENTRY(pEntry); // Search in all device interfaces and notify every interface
    USBH_ASSERT_MAGIC(pInterface, USB_INTERFACE);
    Status = USBH_BD_CompareUsbInterface(pInterface, iMask, TRUE);
    if (USBH_STATUS_SUCCESS == Status) {
      *ppUsbInterface = pInterface;
      break;
    }
    pEntry = USBH_DLIST_GetNext(pEntry);
  }
  return Status;
}

/*********************************************************************
*
*       USBH_GetEndpointDescriptorFromInterface
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
USBH_STATUS USBH_GetEndpointDescriptorFromInterface(USB_INTERFACE * pUsbInterface, U8 alternateSetting, const USBH_EP_MASK * pMask, U8 ** ppDescriptor) {
  USBH_STATUS                Status = USBH_STATUS_SUCCESS;
  USB_INTERFACE_DESCRIPTOR * pInterfaceDescriptor;
  U8                       * pBuffer;
  U8                       * pEndpointDescriptor;
  const U8                 * pNextInterface;
  USB_DEVICE               * pDev;
  int                        Length;
  unsigned int               Index = 0;

  USBH_LOG((USBH_MTYPE_DEVICE, "Device Notification:  GetEndpointDescriptorFromInterfaceDesc: Alt Setting:%d pMask: 0x%x", alternateSetting, pMask->Mask));
  USBH_ASSERT_MAGIC(pUsbInterface, USB_INTERFACE);
  pDev = pUsbInterface->pDevice;
  USBH_ASSERT_MAGIC(pDev, USB_DEVICE);
  if (pDev->State < DEV_STATE_WORKING) {
    return USBH_STATUS_DEVICE_REMOVED;
  }
  // First get the interface descriptor from the configuration descriptor with the alternate setting.
  pInterfaceDescriptor = (USB_INTERFACE_DESCRIPTOR * )pUsbInterface->pInterfaceDescriptor;
  pBuffer = USBH_GetNextInterfaceDesc(pDev, NULL, pInterfaceDescriptor->bInterfaceNumber, alternateSetting);
  if (NULL == pBuffer) {
    USBH_WARN((USBH_MTYPE_DEVICE, "Device Notification Error:  USBH_GetEndpointDescriptor: USBH_GetNextInterfaceDesc!"));
    return USBH_STATUS_INVALID_PARAM;
  }
  Length             = (int)(pDev->pConfigDescriptor + pDev->ConfigDescriptorSize - pBuffer);
  pNextInterface      = (U8 *)USBH_SearchNextDescriptor(pBuffer, &Length, USB_INTERFACE_DESCRIPTOR_TYPE);
  pEndpointDescriptor = pBuffer; // Get the endpoint from the interface
  for (; ;) {
    pEndpointDescriptor = USBH_GetNextEndpointDesc(pDev, pEndpointDescriptor, 0xff);
    if (NULL == pEndpointDescriptor || (pNextInterface != NULL && pEndpointDescriptor > pNextInterface)) {
      USBH_LOG((USBH_MTYPE_DEVICE, "Device Notification warning: No endpoint descriptor found with set mask!"));
      Status = USBH_STATUS_INVALID_PARAM;
      break;
    } else { // Check the mask
      if ((((pMask->Mask & USBH_EP_MASK_INDEX)     == 0) || (Index >= pMask->Index)) &&
          (((pMask->Mask & USBH_EP_MASK_ADDRESS)   == 0) || (pEndpointDescriptor[USB_EP_DESC_ADDRESS_OFS]                            == pMask->Address)) &&
          (((pMask->Mask & USBH_EP_MASK_TYPE)      == 0) || (pEndpointDescriptor[USB_EP_DESC_ATTRIB_OFS]  & USB_EP_DESC_ATTRIB_MASK) == pMask->Type)     &&
          (((pMask->Mask & USBH_EP_MASK_DIRECTION) == 0) || (pEndpointDescriptor[USB_EP_DESC_ADDRESS_OFS] & USB_EP_DESC_DIR_MASK)    == pMask->Direction))
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
*       USBH_GetDescriptorFromInterface
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
USBH_STATUS USBH_GetDescriptorFromInterface(USB_INTERFACE * pUsbInterface, U8 alternateSetting, U8 Type, U8 ** ppDescriptor) {
  USBH_STATUS                Status = USBH_STATUS_SUCCESS;
  USB_INTERFACE_DESCRIPTOR * pInterfaceDescriptor;
  U8                       * pBuffer;
  U8                       * pDescriptor;
  USB_DEVICE               * pDev;
  int                        Length;

  USBH_LOG((USBH_MTYPE_DEVICE, "Device Notification:  USBH_GetDescriptorFromInterface: Alt Setting:%d Type: 0x%x", alternateSetting, Type));
  USBH_ASSERT_MAGIC(pUsbInterface, USB_INTERFACE);
  pDev = pUsbInterface->pDevice;
  USBH_ASSERT_MAGIC(pDev, USB_DEVICE);
  if (pDev->State < DEV_STATE_WORKING) {
    return USBH_STATUS_DEVICE_REMOVED;
  }
  // First get the interface descriptor from the configuration descriptor with the alternate setting.
  pInterfaceDescriptor = (USB_INTERFACE_DESCRIPTOR * )pUsbInterface->pInterfaceDescriptor;
  pBuffer = USBH_GetNextInterfaceDesc(pDev, NULL, pInterfaceDescriptor->bInterfaceNumber, alternateSetting);
  if (NULL == pBuffer) {
    USBH_WARN((USBH_MTYPE_DEVICE, "Device Notification Error:  USBH_GetEndpointDescriptor: USBH_GetNextInterfaceDesc!"));
    return USBH_STATUS_INVALID_PARAM;
  }
  Length      = (int)(pDev->pConfigDescriptor + pDev->ConfigDescriptorSize - pBuffer);
  pDescriptor = (U8 *)USBH_SearchNextDescriptor(pBuffer, &Length, Type);
  if (pDescriptor) {
    *ppDescriptor = pDescriptor;
  } else {
    Status = USBH_STATUS_INVALID_DESCRIPTOR;
  }
  return Status;
}


/*********************************************************************
*
*       USBH_GetEndpointDescriptor
*
*  Function description
*/
USBH_STATUS USBH_GetEndpointDescriptor(USBH_INTERFACE_HANDLE Handle, U8 AlternateSetting, const USBH_EP_MASK * pMask, U8 * pBuffer, unsigned * pBufferSize) {
  USB_INTERFACE * pUsbInterface;
  USBH_STATUS     Status = USBH_STATUS_SUCCESS;
  U8            * pTemp;
  USBH_LOG((USBH_MTYPE_UBD, "UBD: USBH_GetEndpointDescriptor: Alt Setting:%d pMask: 0x%x", AlternateSetting, pMask->Mask));
  pUsbInterface = (USB_INTERFACE * )Handle;
  USBH_ASSERT_MAGIC(pUsbInterface, USB_INTERFACE);
  Status = USBH_GetEndpointDescriptorFromInterface(pUsbInterface, AlternateSetting, pMask, &pTemp);
  if (Status == USBH_STATUS_SUCCESS) {
    *pBufferSize= USBH_MIN(*pBufferSize, USB_ENDPOINT_DESCRIPTOR_LENGTH);
    USBH_MEMCPY(pBuffer, pTemp, *pBufferSize);
  }
  return Status;
}

/*********************************************************************
*
*       USBH_GetDescriptor
*
*  Function description
*/
USBH_STATUS USBH_GetDescriptor(USBH_INTERFACE_HANDLE Handle, U8 AlternateSetting, U8 Type, U8 * pBuffer, unsigned * pBufferSize) {
  USB_INTERFACE * pUsbInterface;
  USB_DEVICE    * pDev;
  USBH_STATUS     Status = USBH_STATUS_SUCCESS;
  U8            * pTemp;
  unsigned        DescLength;

  USBH_LOG((USBH_MTYPE_UBD, "UBD: USBH_GetEndpointDescriptor: Alt Seting:%d Type: 0x%x", AlternateSetting, Type));
  pUsbInterface = (USB_INTERFACE * )Handle;
  pDev          = pUsbInterface->pDevice;
  USBH_ASSERT_MAGIC(pUsbInterface, USB_INTERFACE);
  Status = USBH_GetDescriptorFromInterface(pUsbInterface, AlternateSetting, Type, &pTemp);
  if (Status == USBH_STATUS_SUCCESS) {
    DescLength = *pTemp;
    *pBufferSize = USBH_MIN(*pBufferSize, DescLength);
    USBH_MEMCPY(pBuffer, pTemp, *pBufferSize);
  } else {
    // Prepare an URB
    _EnumPrepareGetDescReq(pDev, Type, 0, 0, 255, pDev->pCtrlTransferBuffer);
    Status = USBH_URB_SubStateSubmitRequest(&pDev->SubState, &pDev->EnumUrb, DEFAULT_SETUP_TIMEOUT, pDev);
    if (Status != USBH_STATUS_PENDING) {
      USBH_WARN((USBH_MTYPE_DEVICE, "Device Notification Error:  USBH_GetDescriptor: DEV_ENUM_START USBH_URB_SubStateSubmitRequest failed %08x", Status));
    }

  }
  return Status;
}


/*********************************************************************
*
*       USBH_GetDescriptorEx
*
*  Function description
*/
USBH_STATUS USBH_GetDescriptorEx(USBH_INTERFACE_HANDLE Handle, U8 Type, U8 DescIndex, U16 LangID, U8 * pBuffer, unsigned * pBufferSize) {
  USB_INTERFACE * pUsbInterface;
  USB_DEVICE    * pDev;
  USBH_STATUS     Status = USBH_STATUS_SUCCESS;
  USBH_OS_EVENT_OBJ * pEvent;

  pEvent = USBH_OS_AllocEvent();
  pUsbInterface = (USB_INTERFACE * )Handle;
  pDev          = pUsbInterface->pDevice;
  USBH_ASSERT_MAGIC(pUsbInterface, USB_INTERFACE);
  // Prepare an URB
  _EnumPrepareGetDescReq(pDev, Type, DescIndex, LangID, 255, pDev->pCtrlTransferBuffer);
  pDev->EnumUrb.Header.pfOnCompletion = _OnSubmitUrbCompletion;
  pDev->EnumUrb.Header.pContext = pEvent;
  Status = USBH_SubmitUrb(Handle, &pDev->EnumUrb);
  if (Status != USBH_STATUS_PENDING) {
    USBH_WARN((USBH_MTYPE_DEVICE, "Device Notification Error:  USBH_GetDescriptor: DEV_ENUM_START USBH_URB_SubStateSubmitRequest failed %08x", Status));
  }
  if (USBH_OS_WaitEventTimed(pEvent, DEFAULT_SETUP_TIMEOUT) != USBH_OS_EVENT_SIGNALED) {
    USBH_URB  Urb;

    USBH_MEMSET(&Urb, 0, sizeof(Urb));
    Urb.Header.Function = USBH_FUNCTION_ABORT_ENDPOINT;
    Urb.Request.EndpointRequest.Endpoint = 0x00;
    Urb.Header.pfOnCompletion = _OnSubmitUrbCompletion;
    Urb.Header.pContext       = pEvent;
    USBH_SubmitUrb(Handle, &Urb);
    USBH_OS_WaitEvent(pEvent);
    Status = USBH_STATUS_CANCELED;
  } else {
    Status = pDev->EnumUrb.Header.Status;
    *pBufferSize = USBH_MIN(*pBufferSize, pDev->CtrlTransferBufferSize);
    USBH_MEMCPY(pBuffer, pDev->pCtrlTransferBuffer, *pBufferSize);
  }
  USBH_OS_FreeEvent(pEvent);
  return Status;
}

/*********************************************************************
*
*       USBH_GetSerialNumber
*
*  Function description
*    It returns the serial number as a UNICODE string in USB little endian format. pBufferSize returns the number of valid bytes.
*    The string is not zero terminated.
*/
USBH_STATUS USBH_GetSerialNumber(USBH_INTERFACE_HANDLE Handle, U8 * pBuffer, unsigned * pBufferSize) {
  USB_INTERFACE * pUsbInterface;
  USB_DEVICE    * pDev;
  USBH_STATUS     Status = USBH_STATUS_SUCCESS;

  USBH_LOG((USBH_MTYPE_UBD, "UBD: USBH_GetSerialNumber"));
  pUsbInterface = (USB_INTERFACE * )Handle;
  USBH_ASSERT_MAGIC(pUsbInterface, USB_INTERFACE);
  pDev           = pUsbInterface->pDevice;
  if (pDev->State < DEV_STATE_WORKING) {
    return USBH_STATUS_DEVICE_REMOVED;
  }
  *pBufferSize = USBH_MIN(*pBufferSize, pDev->SerialNumberSize);
  if (*pBufferSize == 0) {
    return Status;
  }
  USBH_MEMCPY(pBuffer, pDev->pSerialNumber, *pBufferSize); // Returns a little endian unicode string
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
  USBH_ASSERT_MAGIC(pUsbInterface, USB_INTERFACE);
  pDev          = pUsbInterface->pDevice;
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
  USB_INTERFACE        * pUsbInterface;
  USBH_HOST_CONTROLLER * pHostController;
  USB_DEVICE           * pDev;

  pUsbInterface = (USB_INTERFACE * )Handle;
  USBH_ASSERT_MAGIC(pUsbInterface, USB_INTERFACE);
  pDev          = pUsbInterface->pDevice;
  if (pDev->State < DEV_STATE_WORKING) {
    USBH_WARN((USBH_MTYPE_DEVICE, "Device Notification Error:  USBH_GetFrameNumber: invalid device state!"));
    return USBH_STATUS_DEVICE_REMOVED;
  }
  pHostController = pDev->pHostController;
  *pFrameNumber   = pHostController->pDriver->pfGetFrameNumber(pHostController->hHostController);
  USBH_LOG((USBH_MTYPE_UBD, "UBD: USBH_GetFrameNumber: %d", *pFrameNumber));
  return USBH_STATUS_SUCCESS;
}

/*********************************************************************
*
*       USBH_GetPendingUrbCount
*
*  Function description
*/
unsigned int USBH_GetPendingUrbCount(USB_DEVICE * pDev) {
  USBH_DLIST         * pEntry;
  USB_INTERFACE * pUsbInterface;
  unsigned int    Count = 0;

  pEntry = USBH_DLIST_GetNext(&pDev->UsbInterfaceList);
  while (pEntry != &pDev->UsbInterfaceList) {
    pUsbInterface = GET_USB_INTERFACE_FROM_ENTRY(pEntry);
    USBH_ASSERT_MAGIC(pUsbInterface, USB_INTERFACE);
    pEntry       = USBH_DLIST_GetNext(pEntry);
    Count       += USBH_BD_GetPendingUrbCount(pUsbInterface);
  }
  Count += pDev->DefaultEp.UrbCount;
  return Count;
}

/********************************* EOF ******************************/
