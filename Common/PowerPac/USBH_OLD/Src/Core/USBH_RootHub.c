/*********************************************************************
*  
*   IAR PowerPac
*
*   (c) Copyright IAR Systems 2010.  All rights reserved.
*
**********************************************************************
----------------------------------------------------------------------
File        : USBH_Roothub.c
Purpose     : USB Bus Driver Core
              root hub object
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

#ifndef    SIM_RH_PORT_RESET_RESTART
  #define  SIM_RH_PORT_RESET_RESTART      0
#endif

#ifndef    SIM_RH_PORT_RESET_FATAL
  #define  SIM_RH_PORT_RESET_FATAL        0
#endif

#if SIM_RH_PORT_RESET_RESTART || SIM_RH_PORT_RESET_FATAL
  static int gSimCt = 0;
#endif

static void RootHubProcessPortResetSetAddress(void * rootHub);
static void RootHubInitPortsCompletion       (void * rootHub);

/*********************************************************************
*
*       UbdInitRootHub
*
*  Function description:
*/
USBH_STATUS UbdInitRootHub(struct tag_HOST_CONTROLLER * HostController) {
  USBH_STATUS   status;
  ROOT_HUB    * RootHub = &HostController->RootHub;
#if (USBH_DEBUG > 1)
  RootHub->Magic          = ROOT_HUB_MAGIC;
#endif
  RootHub->HostController = HostController;
  DlistInit(&RootHub->PortList);
  status = UrbSubStateInit(&RootHub->SubState, HostController, &RootHub->EnumEpHandle, RootHubProcessPortResetSetAddress, RootHub);
  if (status) {
    USBH_WARN((USBH_MTYPE_HUB, "Roothub: UbdInitRootHub: UrbSubStateInit st: %08x",status));
    return status;
  }
  status = UrbSubStateInit(&RootHub->InitHubPortSubState, HostController, NULL, RootHubInitPortsCompletion, RootHub);
  if (status) {
    USBH_WARN((USBH_MTYPE_HUB, "Roothub: UbdInitRootHub: UrbSubStateInit st: %08x",status));
  }
  return status;
}

/*********************************************************************
*
*       RootHubRemoveAllPorts
*
*  Function description:
*/
static void RootHubRemoveAllPorts(ROOT_HUB * hub) {
  PDLIST     entry;
  HUB_PORT * hub_port;
  USBH_LOG((USBH_MTYPE_HUB, "Roothub: RootHubRemoveAllPorts!"));
  while (!DlistEmpty(&hub->PortList)) {
    DlistRemoveHead(&hub->PortList, &entry);
    hub_port = HUB_PORT_PTR(entry);
    UbdDeleteHubPort(hub_port);
  }
}

/*********************************************************************
*
*       UbdReleaseRootHub
*
*  Function description:
*/
void UbdReleaseRootHub(ROOT_HUB * RootHub) {
  UrbSubStateExit(&RootHub->SubState);
  UrbSubStateExit(&RootHub->InitHubPortSubState);
  RootHubRemoveAllPorts(RootHub);                 // Release all root hub ports
}

/*********************************************************************
*
*       UbdRootHubNotification
*
*  Function description:
*    Called from the Host controller driver if an root hub event occures
*    bit0 indicates a status change of the HUB, bit 1 of port 1 of the hub and so on.
*
*  Parameters:
*    RootHubContext    - 
*    Notification    - 
*  
*  Return value:
*    void       - 
*/
void UbdRootHubNotification(void * RootHubContext, U32 Notification) {
  ROOT_HUB       * RootHub   = (ROOT_HUB * )RootHubContext;
  HUB_PORT       * HubPort;
  USB_HOST_ENTRY * HostEntry = &RootHub->HostController->HostEntry;
  U32              HubStatus;
  U8               i;
  USBH_LOG((USBH_MTYPE_HUB, "Roothub: ROOT_HUB_NOTIFY: 0x%x!",Notification));
  T_ASSERT_MAGIC(RootHub, ROOT_HUB);
  if (RootHub->HostController->State < HC_WORKING) {
    USBH_WARN((USBH_MTYPE_HUB, "Roothub: UbdRootHubNotification: got Notification: 0x%x while HC State is %s",Notification,UbdHcStateStr));
    RootHub->PortResetEnumState = RH_PORTRESET_REMOVED;       // This prevents the state machine to call the HC and it stops it, free resources
  }
  if (Notification & 0x01) {
    HubStatus = HostEntry->GetHubStatus(HostEntry->HcHandle); // Hub status change (port 0 is the hub self)
    USBH_LOG((USBH_MTYPE_HUB, "Roothub: ROOT_HUB_NOTIFY: HubStatus %08x",HubStatus));
    // Clear the change bits
    if (HubStatus & HUB_STATUS_LOCAL_POWER) {  // The root hub is always self powered, this should not happen
      HostEntry->ClearHubStatus(HostEntry->HcHandle, HDC_SELECTOR_C_HUB_LOCAL_POWER);
    }
    if (HubStatus & HUB_STATUS_OVER_CURRENT) { // Hub reports an overcurrent on the global scope
      HostEntry->ClearHubStatus(HostEntry->HcHandle, HDC_SELECTOR_C_HUB_OVER_CURRENT);
    }
  }
  // Check device notifications
  for (i = 1; i <= RootHub->PortCount; i++) {
    U32 notification;
    notification = Notification &(0x00000001 << i);
    if (notification != 0) {
      HubPort = UbdGetRootHubPortByNumber(RootHub, i);
      if (HubPort == NULL) {
        USBH_WARN((USBH_MTYPE_HUB, "Roothub: ROOT_HUB_NOTIFY: HC returns invalid notifications %08x",Notification));
      } else {
        // Get the current port status, it is processed later with UbdServiceRootHubPorts
        HubPort->PortStatus       = HostEntry->GetPortStatus(HostEntry->HcHandle, HubPort->HubPortNumber);
        HubPort->PortStatusShadow = HubPort->PortStatus; // Shadow register used from enumeration for reset and compare
        // This port needs service, the current port status is in HubPort->PortStatus
        // Clear the change bits in the host
        if (HubPort->PortStatus & PORT_C_STATUS_CONNECT) {
          HubPort->PortStatus &= ~PORT_C_STATUS_CONNECT;
          HostEntry->ClearPortStatus(HostEntry->HcHandle, HubPort->HubPortNumber, HDC_SELECTOR_C_PORT_CONNECTION);
        }
        if (HubPort->PortStatus & PORT_C_STATUS_ENABLE) {
          HubPort->PortStatus &= ~PORT_C_STATUS_ENABLE;
          HostEntry->ClearPortStatus(HostEntry->HcHandle, HubPort->HubPortNumber, HDC_SELECTOR_C_PORT_ENABLE);
        }
        if (HubPort->PortStatus & PORT_C_STATUS_SUSPEND) {
          HubPort->PortStatus &= ~PORT_C_STATUS_SUSPEND;
          HostEntry->ClearPortStatus(HostEntry->HcHandle, HubPort->HubPortNumber, HDC_SELECTOR_C_PORT_SUSPEND);
        }
        if (HubPort->PortStatus & PORT_C_STATUS_OVER_CURRENT) {
          HubPort->PortStatus &= ~PORT_C_STATUS_OVER_CURRENT;
          HostEntry->ClearPortStatus(HostEntry->HcHandle, HubPort->HubPortNumber, HDC_SELECTOR_C_PORT_OVER_CURRENT);
        }
        if (HubPort->PortStatus & PORT_C_STATUS_RESET) {
          HubPort->PortStatus &= ~PORT_C_STATUS_RESET;
          HostEntry->ClearPortStatus(HostEntry->HcHandle, HubPort->HubPortNumber, HDC_SELECTOR_C_PORT_RESET);
        }
        // Process port status information if the port if the port is not enumerated
        if (HubPort != RootHub->EnumPort) {                                      // This port is not enumeratated now
          if (HubPort->PortStatus & PORT_STATUS_OVER_CURRENT) {                  // Check overcurrent
            USBH_LOG((USBH_MTYPE_HUB, "Roothub: ROOT_HUB_NOTIFY: PORT_STATUS_OVER_CURRENT Port:%d Status:%08x",HubPort->HubPortNumber,HubPort->PortStatus));
            // The device uses too much current, remove it
            if (HubPort->Device != NULL) {
              UbdUdevMarkParentAndChildDevicesAsRemoved(HubPort->Device);
            }
            HostEntry->DisablePort(HostEntry->HcHandle, HubPort->HubPortNumber); // Disable the port to avoid fire -)
            UbdSetPortState(HubPort, PORT_RESTART);                              // Should we restart a device over current? Its better than forget it!
          }
          // New connection
          if ((HubPort->PortStatus &PORT_STATUS_CONNECT) && !(HubPort->PortStatus &PORT_STATUS_ENABLED)) {
            // This device must be enumerated
            if (HubPort->PortState >= PORT_ENABLED && HubPort->Device != NULL) {
              // Remove the old connected device first
              USBH_LOG((USBH_MTYPE_HUB, "Roothub: ROOT_HUB_NOTIFY: delete dev., port connected but not enabled Port:%d Status:%08x", HubPort->HubPortNumber,HubPort->PortStatus));
              UbdUdevMarkParentAndChildDevicesAsRemoved(HubPort->Device);
            }
            HubPort->RetryCounter = 0;
            UbdSetPortState(HubPort, PORT_CONNECTED);
          }
          // Device removed
          if (!(HubPort->PortStatus &PORT_STATUS_CONNECT)) {
            if (HubPort->Device != NULL) { // This device is removed
              // Remove the old connected device first
              USBH_LOG((USBH_MTYPE_HUB, "Roothub: ROOT_HUB_NOTIFY: port not connected, delete dev., Port:%d Status:%08x", HubPort->HubPortNumber, HubPort->PortStatus));
              UbdUdevMarkParentAndChildDevicesAsRemoved(HubPort->Device);
            }
            USBH_LOG((USBH_MTYPE_HUB, "Roothub: ROOT_HUB_NOTIFY: port removed!"));
            UbdSetPortState(HubPort, PORT_REMOVED);
            HostEntry->DisablePort(HostEntry->HcHandle, HubPort->HubPortNumber); // Disable the port
          }
        }
      }
    }
  }
  UbdHcServicePorts(RootHub->HostController); // Service all ports
}

/*********************************************************************
*
*       UbdServiceRootHubPorts
*
*  Function description:
*    Called after a notification or if the enumeration of a device has finished
*/
T_BOOL UbdServiceRootHubPorts(ROOT_HUB * RootHub) {
  HUB_PORT * HubPort;
  DLIST    * e;
  if (RootHub->PortResetEnumState != RH_PORTRESET_IDLE || RootHub->HostController->State < HC_WORKING) {
    // Enumeration is active or the root hub is not working, do nothing now, we are called later
    return TRUE;
  }
  // Run a second time over all ports, to see if a port needs a reset
  e = DlistGetNext(&RootHub->PortList);
  while (e != &RootHub->PortList) {
    HubPort = HUB_PORT_PTR(e);
    T_ASSERT_MAGIC(HubPort, HUB_PORT);
    e       = DlistGetNext(e);
    if (HubPort->PortState == PORT_RESTART || HubPort->PortState == PORT_CONNECTED) {
      HubPort->RetryCounter++;
      if (HubPort->RetryCounter > RESET_RETRY_COUNTER) {
        USBH_WARN((USBH_MTYPE_HUB, "Roothub: UbdServiceRootHubPorts: max. port retries -> PORT_ERROR!"));
#if SIM_RH_PORT_RESET_RESTART
        gSimCt = 0;                                 // Set counter to zero for restart error simulation
#endif
        UbdSetPortState                         (HubPort, PORT_ERROR);
        UbdSetEnumErrorNotificationRootPortReset(HubPort, RootHub->PortResetEnumState, USBH_STATUS_ERROR);
      } else {
        if (HubPort->PortState == PORT_RESTART) {   // PORT_CONNECTED or PORT_RESTART
          RootHub->PortResetEnumState = RH_PORTRESET_RESTART;
        } else {
          RootHub->PortResetEnumState = RH_PORTRESET_START;
        }
        HC_INC_REF(RootHub->HostController);
        RootHub->EnumPort = HubPort;
        USBH_HC_SetActivePortReset       (RootHub->HostController, HubPort);
        RootHubProcessPortResetSetAddress(RootHub); // Start the port reset
        return TRUE;
      }
    }
  }
  return FALSE;
}

/*********************************************************************
*
*       RootHubPortResetSetIdleServicePorts
*
*  Function description:
*/
static void RootHubPortResetSetIdleServicePorts(ROOT_HUB * rootHub) {
  T_ASSERT_MAGIC(rootHub, ROOT_HUB);
  rootHub->PortResetEnumState = RH_PORTRESET_IDLE;
  // Allow starting an port reset on another port
  USBH_HC_ClrActivePortReset(rootHub->HostController, rootHub->EnumPort);
  rootHub->EnumDevice = NULL;
  rootHub->EnumPort   = NULL;
  // Service all ports
  UbdHcServicePorts(rootHub->HostController);
  HC_DEC_REF       (rootHub->HostController);
}

/*********************************************************************
*
*       RootHubPortResetSetIdleServicePorts
*
*  Function description:
*    Called if an port is not connected or the root hub is removed. The
*    port state is set to PORT_REMOVED and the state machine to idle
*/
static void RootHubPortResetRemove(ROOT_HUB * rootHub) {
  USB_HOST_ENTRY * host_entry;
  T_ASSERT_MAGIC(rootHub, ROOT_HUB);
  T_ASSERT_MAGIC(rootHub->EnumPort, HUB_PORT);
  USBH_LOG((USBH_MTYPE_HUB, "Roothub: RootHubPortResetRemove"));
  host_entry = &rootHub->HostController->HostEntry;
  UbdSetPortState(rootHub->EnumPort, PORT_REMOVED); // Set port to remove and disable the port
  host_entry->DisablePort(host_entry->HcHandle, rootHub->EnumPort->HubPortNumber);
  if (NULL != rootHub->EnumDevice) {
    // Delete the device, this is the initial reference on default
    DEC_REF(rootHub->EnumDevice);
  }
  RootHubPortResetSetIdleServicePorts(rootHub);
}

/*********************************************************************
*
*       RootHubPortResetRestart
*
*  Function description:
*/
static void RootHubPortResetRestart(ROOT_HUB * rootHub, USBH_STATUS status) {
  USB_HOST_ENTRY * host_entry;
  T_ASSERT_MAGIC(rootHub, ROOT_HUB);
  T_ASSERT_MAGIC(rootHub->EnumPort, HUB_PORT);
  USBH_LOG((USBH_MTYPE_HUB, "Roothub: RootHubPortResetRestart: %s", UbdRhPortResetStateStr(rootHub->PortResetEnumState)));
  host_entry = &rootHub->HostController->HostEntry;
  host_entry->DisablePort(host_entry->HcHandle, rootHub->EnumPort->HubPortNumber);
  UbdSetPortState                              (rootHub->EnumPort, PORT_RESTART);
  UbdSetEnumErrorNotificationRootPortReset     (rootHub->EnumPort, rootHub->PortResetEnumState, status);
  if (NULL != rootHub->EnumDevice) {
    DEC_REF(rootHub->EnumDevice);    // Delete the device, this is the initial reference on default
  }
  RootHubPortResetSetIdleServicePorts(rootHub);
}

/*********************************************************************
*
*       RootHubPortResetSetPortError
*
*  Function description:
*    Parent port is disabled an port state is set to PORT_ERROR. This
*    port is never reenumerated or the user tries agina to restart this
*    device!
*/
static void RootHubPortResetSetPortError(ROOT_HUB * rootHub, USBH_STATUS status) {
  USB_HOST_ENTRY * host_entry;
  T_ASSERT_MAGIC(rootHub, ROOT_HUB);
  T_ASSERT_MAGIC(rootHub->EnumPort, HUB_PORT);
  USBH_LOG((USBH_MTYPE_HUB, "Roothub: RootHubPortResetSetPortError: %s", UbdRhPortResetStateStr(rootHub->PortResetEnumState)));
  host_entry = &rootHub->HostController->HostEntry;
  host_entry->DisablePort(host_entry->HcHandle, rootHub->EnumPort->HubPortNumber);
  UbdSetPortState(rootHub->EnumPort, PORT_ERROR);
  UbdSetEnumErrorNotificationRootPortReset(rootHub->EnumPort, rootHub->PortResetEnumState, status); // Notify user from port enumeration error
  if (NULL != rootHub->EnumDevice) {
    DEC_REF(rootHub->EnumDevice);    // Delete the device, this is the initial reference on default
  }
  RootHubPortResetSetIdleServicePorts(rootHub);
}

/*********************************************************************
*
*       RootHubProcessPortResetSetAddress
*
*  Function description:
*/
static void RootHubProcessPortResetSetAddress(void * rootHub) {
  USB_HOST_ENTRY * host_entry;
  HUB_PORT       * enum_port;
  ROOT_HUB       * root_hub;
  URB            * urb;
  USBH_STATUS      status;
  root_hub       = (ROOT_HUB *)rootHub;
  T_ASSERT_MAGIC(root_hub, ROOT_HUB);
  host_entry     = &root_hub->HostController->HostEntry;
  enum_port      =  root_hub->EnumPort;
#if (USBH_DEBUG > 1)
  if (root_hub->PortResetEnumState >= RH_PORTRESET_START) {
    T_ASSERT_MAGIC(enum_port, HUB_PORT);
  }
#endif
  if (root_hub->HostController->State < HC_WORKING) {
    root_hub->PortResetEnumState = RH_PORTRESET_REMOVED;
  }
  if (root_hub->PortResetEnumState >= RH_PORTRESET_WAIT_RESTART) {
    if (enum_port->PortStatus & PORT_STATUS_CONNECT) {               // Port is connected
      if (root_hub->PortResetEnumState >= RH_PORTRESET_WAIT_RESET) { // All states after port reset
        if (!(enum_port->PortStatus &PORT_STATUS_ENABLED)) {         // Port is not enabled
          USBH_WARN((USBH_MTYPE_HUB, "Roothub: RootHubProcessPortResetSetAddress: Port disabled during port reset"));
          RootHubPortResetRestart(root_hub, USBH_STATUS_PORT);
          return;
        }
      }
    } else { // Port not connected after port reset state
      root_hub->PortResetEnumState = RH_PORTRESET_REMOVED;
    }
  }
  USBH_LOG((USBH_MTYPE_HUB, "Roothub: ROOT_HUB_PORT_RESET: %s", UbdRhPortResetStateStr(root_hub->PortResetEnumState)));
  switch (root_hub->PortResetEnumState) {
    case RH_PORTRESET_REMOVED:                // Mark port as removed and set enumeration state to idle check other root hub ports
      RootHubPortResetRemove(root_hub);
      break;
    case RH_PORTRESET_START:                  // Normal port reset: wait 100ms before resetet the port
      UbdSetPortState(enum_port, PORT_RESET);
      root_hub->PortResetEnumState = RH_PORTRESET_WAIT_RESTART;
      UrbSubStateWait(&root_hub->SubState, WAIT_AFTER_CONNECT, NULL);
      break;
    case RH_PORTRESET_RESTART:                // Delayed port reset: wait about one second
      UbdSetPortState(enum_port, PORT_RESET);
      root_hub->PortResetEnumState = RH_PORTRESET_WAIT_RESTART;
      UrbSubStateWait(&root_hub->SubState, DELAY_FOR_REENUM, NULL);
      break;
    case RH_PORTRESET_WAIT_RESTART:
      root_hub->PortResetEnumState = RH_PORTRESET_RES;
      // Set to zero, expect that a state notification is returned, if the reset is complete
      enum_port->PortStatusShadow  = 0;
      // Port reset, if after the timeout the port state is not connected then the next state is RH_PORTRESET_REMOVED
      host_entry->ResetPort(host_entry->HcHandle, enum_port->HubPortNumber);
      UrbSubStateWait(&root_hub->SubState, DEFAULT_RESET_TIMEOUT, NULL);
      break;
    case RH_PORTRESET_RES:                    // Ok, port is enabled
      UbdSetPortState(enum_port, PORT_ENABLED);
      enum_port->PortSpeed = USBH_FULL_SPEED; // Get the speed
      if (enum_port->PortStatus & PORT_STATUS_LOW_SPEED) {
        enum_port->PortSpeed = USBH_LOW_SPEED;
      }
      if (enum_port->PortStatus & PORT_STATUS_HIGH_SPEED) {
        enum_port->PortSpeed = USBH_HIGH_SPEED;
      }
      // Wait some time that the device can recover, this time is not defined by the specification
      // but windows makes a gap, so we will do it in the same way
      root_hub->PortResetEnumState = RH_PORTRESET_WAIT_RESET;
      USBH_LOG((USBH_MTYPE_HUB, "Roothub: ROOT_HUB_PORT_RESET: TAL_StartTimer(WAIT_AFTER_RESET)"));
      UrbSubStateWait(&root_hub->SubState, WAIT_AFTER_RESET, NULL);
      break;
    case RH_PORTRESET_WAIT_RESET:
      root_hub->EnumDevice = UbdNewUsbDevice(root_hub->HostController); // Now create a new device
      if (root_hub->EnumDevice == NULL) { // On error abort the port enumeration
        USBH_WARN((USBH_MTYPE_HUB, "Roothub: ROOT_HUB_PORT_RESET: UbdNewUsbDevice fails, no memory, no retry!"));
        RootHubPortResetSetPortError(root_hub, USBH_STATUS_RESOURCES);
        break;
      }
      // Init the device structure
      root_hub->EnumDevice->DeviceSpeed         = enum_port->PortSpeed;
      root_hub->EnumDevice->UsbAddress          = UbdGetUsbAddress(root_hub->HostController);
      // Backward pointer to the hub port, the ports device pointer is set after complete enumeration of this device.
      // The state machine of the later device enumeration checks the port state and delete the device if the state is removed.
      root_hub->EnumDevice->ParentPort          = enum_port;
      root_hub->EnumDevice->ConfigurationIndex  = enum_port->ConfigurationIndex;
      // Prepare the set address request
      urb = &root_hub->EnumUrb;
      ZERO_MEMORY(urb, sizeof(URB));
      urb->Header.Function                      = USBH_FUNCTION_CONTROL_REQUEST;
      urb->Request.ControlRequest.Setup.Type    = 0x00; // STD, OUT, device
      urb->Request.ControlRequest.Setup.Request = USB_REQ_SET_ADDRESS;
      urb->Request.ControlRequest.Setup.Value   = root_hub->EnumDevice->UsbAddress;
      // Select one of the preallocated endpoints
      switch (enum_port->PortSpeed) {
        case USBH_LOW_SPEED:
          root_hub->EnumEpHandle = root_hub->HostController->LowSpeedEndpoint;
          break;
        case USBH_FULL_SPEED:
          root_hub->EnumEpHandle = root_hub->HostController->FullSpeedEndpoint;
          break;
        case USBH_HIGH_SPEED:
          root_hub->EnumEpHandle = root_hub->HostController->HighSpeedEndpoint;
          break;
        default:
          T_ASSERT0;
      }
      // Set a new  state
      root_hub->PortResetEnumState = RH_PORTRESET_SET_ADDRESS;
      // Setup a timer if the device does not answer
      // Submit the request
#if SIM_RH_PORT_RESET_FATAL
      gSimCt++;
      if (gSimCt % SIM_RH_ERROR_RETRY == 0) {
        status = USBH_STATUS_ERROR;
      } else {
        status = UrbSubStateSubmitRequest(&root_hub->SubState, &root_hub->EnumUrb, DEFAULT_SETUP_TIMEOUT, root_hub->EnumDevice);
      }
#else
      status = UrbSubStateSubmitRequest(&root_hub->SubState, &root_hub->EnumUrb, DEFAULT_SETUP_TIMEOUT, root_hub->EnumDevice);
#endif
      if (status != USBH_STATUS_PENDING) { // Error on submitting: set port to PORT_ERROR
        USBH_WARN((USBH_MTYPE_HUB, "Roothub: ROOT_HUB_PORT_RESET: UrbSubStateSubmitRequest failed %08x",status));
        RootHubPortResetSetPortError(root_hub, status);
      }
      break;
    case RH_PORTRESET_SET_ADDRESS:
#if SIM_RH_PORT_RESET_RESTART
      gSimCt++;
      if (gSimCt > SIM_RH_ERROR_RETRY) { // Generates errors until the port is stopped
        root_hub->EnumUrb.Header.Status = USBH_STATUS_ERROR;
      }
#endif
      if (root_hub->EnumUrb.Header.Status != USBH_STATUS_SUCCESS) {
        USBH_WARN((USBH_MTYPE_HUB, "Roothub: ROOT_HUB_PORT_RESET:RH_PORTRESET_SET_ADDRESS failed st:%08x",root_hub->
          EnumUrb.Header.Status));
        RootHubPortResetRestart(root_hub, root_hub->EnumUrb.Header.Status);
        break;
      }
      // Ok, now the device is addressed, wait some ms to let the device switch to the new address
      root_hub->PortResetEnumState = RH_PORTRESET_WAIT_ADDRESS;
      UrbSubStateWait(&root_hub->SubState, WAIT_AFTER_SETADDRESS, NULL);
      break;
    case RH_PORTRESET_WAIT_ADDRESS: {
      USB_DEVICE * dev;
      // 1. The device that is connected to the port is added after successfully enumeration (Port->Device = Device)
      // 2. start the device enumeration process
      // 3. release this port enumeration and wait for connecting other ports! at this point the port state is PORT_ENABLED!
      enum_port->ConfigurationIndex = 0;
      dev                           = root_hub->EnumDevice;
      // Prevent access to the enum device after starting the enumeration process
      root_hub->EnumDevice          = NULL;
      UbdStartEnumeration(dev, UbdCreateInterfaces, dev);
      RootHubPortResetSetIdleServicePorts(root_hub);
    }
      break;
    default:
      T_ASSERT0;
  }
}

/*********************************************************************
*
*       RootHubInitPortsCompletion
*
*  Function description:
*    Called in the context of an timer routine after elapses the power good time
*    of the root hub. This routine is called once after the hub objects created.
*/
static void RootHubInitPortsCompletion(void * rootHub) {
  DLIST          * e;
  HUB_PORT       * enum_port;
  ROOT_HUB       * root_hub;
  USB_HOST_ENTRY * host_entry;
  root_hub = (ROOT_HUB *)rootHub;
  T_ASSERT_MAGIC(root_hub, ROOT_HUB);
  host_entry = &root_hub->HostController->HostEntry;
  e          = DlistGetNext(&root_hub->PortList);
  // Read the port status from the field of all root hub ports before any root hub port is serviced
  while (e != &root_hub->PortList) {
    enum_port = HUB_PORT_PTR(e);
    T_ASSERT_MAGIC(enum_port, HUB_PORT);
    e         = DlistGetNext(e);
    enum_port->PortStatus       = host_entry->GetPortStatus(host_entry->HcHandle, enum_port->HubPortNumber);
    enum_port->PortStatusShadow = enum_port->PortStatus;
    if (enum_port->PortStatus & PORT_STATUS_CONNECT) {
      // Device is connected, mark service required
      UbdSetPortState(enum_port, PORT_CONNECTED);
    }
  }
  root_hub->PortResetEnumState = RH_PORTRESET_IDLE;
  UbdHcServicePorts(root_hub->HostController);
  HC_DEC_REF(root_hub->HostController);
}

/*********************************************************************
*
*       UbdRootHubAddPortsStartPowerGoodTime
*
*  Function description:
*    Create all need root hub ports and starts an timer routine with the
*    power good time. In the timer routine an routine is called that
*    services all ports for connected devices!
*/
USBH_STATUS UbdRootHubAddPortsStartPowerGoodTime(ROOT_HUB * RootHub) {
  HOST_CONTROLLER * HostController = RootHub->HostController;
  USB_HOST_ENTRY  * HostEntry      = &HostController->HostEntry;
  HUB_PORT        * HubPort;
  U8                i;
  T_ASSERT_MAGIC(RootHub, ROOT_HUB);
  RootHub->PortCount     = HostEntry->GetPortCount    (HostEntry->HcHandle);
  RootHub->PowerGoodTime = HostEntry->GetPowerGoodTime(HostEntry->HcHandle);
  for (i = 1; i <= RootHub->PortCount; i++) {
    HostEntry->SetPortPower(HostEntry->HcHandle, i, 1); // Turn the power on
    HubPort = UbdNewHubPort();                          // Create a new hub port port state is port_removed
    if (HubPort == NULL) {
      USBH_WARN((USBH_MTYPE_HUB, "Roothub: UbdRootHubAddPortsStartPowerGoodTime: UbdNewHubPort failed"));
      return USBH_STATUS_RESOURCES;
    }
    // Init the hub port
    HubPort->HighPowerFlag = TRUE;
    HubPort->HubPortNumber = i;
    HubPort->RootHub       = RootHub;
    DlistInsertTail(&RootHub->PortList, &HubPort->ListEntry);
  }
  RootHub->PortResetEnumState = RH_PORTRESET_INIT; // RH_PORTRESET_INIT prevents starting of root hub enumeration until power good time is elapsed!
  HC_INC_REF(HostController);                      // Hold the host controller
  UrbSubStateWait(&RootHub->InitHubPortSubState, RootHub->PowerGoodTime, NULL); // Wait for Power good time
  return USBH_STATUS_SUCCESS;
}

/*********************************************************************
*
*       UbdGetRootHubPortByNumber
*
*  Function description:
*/
HUB_PORT * UbdGetRootHubPortByNumber(ROOT_HUB * RootHub, U8 port) {
  HUB_PORT * HubPort;
  DLIST    * e;
  T_ASSERT_MAGIC(RootHub, ROOT_HUB);
  e = DlistGetNext(&RootHub->PortList);
  while (e != &RootHub->PortList) {
    HubPort = HUB_PORT_PTR(e);
    T_ASSERT_MAGIC(HubPort, HUB_PORT);
    e       = DlistGetNext(e);
    if (HubPort->HubPortNumber == port) {
      return HubPort;
    }
  }
  return NULL;
}

/******************************* EOF ********************************/
