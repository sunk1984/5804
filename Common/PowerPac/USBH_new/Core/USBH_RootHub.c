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
              root pHub object
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

static void _ProcessPortResetSetAddress(void * rootHub);
static void _InitPortsCompletion       (void * rootHub);



/*********************************************************************
*
*       Static code
*
**********************************************************************
*/

/*********************************************************************
*
*       _RemoveAllPorts
*
*  Function description:
*/
static void _RemoveAllPorts(ROOT_HUB * pHub) {
  USBH_DLIST    * pEntry;
  USBH_HUB_PORT * pHubPort;
  USBH_LOG((USBH_MTYPE_HUB, "Roothub: _RemoveAllPorts!"));
  while (!USBH_DLIST_IsEmpty(&pHub->PortList)) {
    USBH_DLIST_RemoveHead(&pHub->PortList, &pEntry);
    pHubPort = GET_HUB_PORT_PTR(pEntry);
    USBH_BD_DeleteHubPort(pHubPort);
  }
}

/*********************************************************************
*
*       _PortResetSetIdleServicePorts
*
*  Function description:
*/
static void _PortResetSetIdleServicePorts(ROOT_HUB * pRootHub) {
  USBH_ASSERT_MAGIC(pRootHub, ROOT_HUB);
  pRootHub->PortResetEnumState = RH_PORTRESET_IDLE;
  // Allow starting an port reset on another port
  USBH_HC_ClrActivePortReset(pRootHub->pHostController, pRootHub->pEnumPort);
  pRootHub->pEnumDevice = NULL;
  pRootHub->pEnumPort   = NULL;
  // Service all ports
  USBH_HC_ServicePorts(pRootHub->pHostController);
  HC_DEC_REF       (pRootHub->pHostController);
}

/*********************************************************************
*
*       _PortResetSetIdleServicePorts
*
*  Function description:
*    Called if an port is not connected or the root pHub is removed. The
*    port state is set to PORT_REMOVED and the state machine to idle
*/
static void _PortResetRemove(ROOT_HUB * pRootHub) {
  USBH_HOST_DRIVER * pDriver;

  USBH_ASSERT_MAGIC(pRootHub, ROOT_HUB);
  USBH_ASSERT_MAGIC(pRootHub->pEnumPort, USBH_HUB_PORT);
  USBH_LOG((USBH_MTYPE_HUB, "Roothub: _PortResetRemove"));
  pDriver = pRootHub->pHostController->pDriver;
  USBH_BD_SetPortState(pRootHub->pEnumPort, PORT_REMOVED); // Set port to remove and disable the port
  pDriver->pfDisablePort(pRootHub->pHostController->hHostController, pRootHub->pEnumPort->HubPortNumber);
  if (NULL != pRootHub->pEnumDevice) {
    // Delete the device, this is the initial reference on default
    DEC_REF(pRootHub->pEnumDevice);
  }
  _PortResetSetIdleServicePorts(pRootHub);
}

/*********************************************************************
*
*       _PortResetRestart
*
*  Function description:
*/
static void _PortResetRestart(ROOT_HUB * pRootHub, USBH_STATUS Status) {
  USBH_HOST_DRIVER * pDriver;

  USBH_ASSERT_MAGIC(pRootHub, ROOT_HUB);
  USBH_ASSERT_MAGIC(pRootHub->pEnumPort, USBH_HUB_PORT);
  USBH_LOG((USBH_MTYPE_HUB, "Roothub: _PortResetRestart: %s", USBH_RhPortResetState2Str(pRootHub->PortResetEnumState)));
  pDriver = pRootHub->pHostController->pDriver;
  pDriver->pfDisablePort(pRootHub->pHostController->hHostController, pRootHub->pEnumPort->HubPortNumber);
  USBH_BD_SetPortState(pRootHub->pEnumPort, PORT_RESTART);
  UbdSetEnumErrorNotificationRootPortReset(pRootHub->pEnumPort, pRootHub->PortResetEnumState, Status);
  if (NULL != pRootHub->pEnumDevice) {
    DEC_REF(pRootHub->pEnumDevice);    // Delete the device, this is the initial reference on default
  }
  _PortResetSetIdleServicePorts(pRootHub);
}

/*********************************************************************
*
*       _PortResetSetPortError
*
*  Function description:
*    Parent port is disabled an port state is set to PORT_ERROR. This
*    port is never reenumerated or the user tries again to restart this
*    device!
*/
static void _PortResetSetPortError(ROOT_HUB * pRootHub, USBH_STATUS Status) {
  USBH_HOST_DRIVER * pDriver;

  USBH_ASSERT_MAGIC(pRootHub, ROOT_HUB);
  USBH_ASSERT_MAGIC(pRootHub->pEnumPort, USBH_HUB_PORT);
  USBH_LOG((USBH_MTYPE_HUB, "Roothub: _PortResetSetPortError: %s", USBH_RhPortResetState2Str(pRootHub->PortResetEnumState)));
  pDriver = pRootHub->pHostController->pDriver;
  pDriver->pfDisablePort(pRootHub->pHostController->hHostController, pRootHub->pEnumPort->HubPortNumber);
  USBH_BD_SetPortState(pRootHub->pEnumPort, PORT_ERROR);
  UbdSetEnumErrorNotificationRootPortReset(pRootHub->pEnumPort, pRootHub->PortResetEnumState, Status); // Notify user from port enumeration error
  if (NULL != pRootHub->pEnumDevice) {
    DEC_REF(pRootHub->pEnumDevice);    // Delete the device, this is the initial reference on default
  }
  _PortResetSetIdleServicePorts(pRootHub);
}

/*********************************************************************
*
*       _ProcessPortResetSetAddress
*
*  Function description:
*/
static void _ProcessPortResetSetAddress(void * pRoot_Hub) {
  USBH_HOST_DRIVER * pDriver;
  USBH_HUB_PORT       * pEnumPort;
  ROOT_HUB       * pRootHub;
  USBH_URB            * pUrb;
  USBH_STATUS      Status;

  pRootHub       = (ROOT_HUB *)pRoot_Hub;
  USBH_ASSERT_MAGIC(pRootHub, ROOT_HUB);
  pDriver     = pRootHub->pHostController->pDriver;
  pEnumPort   =  pRootHub->pEnumPort;
#if (USBH_DEBUG > 1)
  if (pRootHub->PortResetEnumState >= RH_PORTRESET_START) {
    USBH_ASSERT_MAGIC(pEnumPort, USBH_HUB_PORT);
  }
#endif
  if (pRootHub->pHostController->State < HC_WORKING) {
    pRootHub->PortResetEnumState = RH_PORTRESET_REMOVED;
  }
  if (pRootHub->PortResetEnumState >= RH_PORTRESET_WAIT_RESTART) {
    if (pEnumPort->PortStatus & PORT_STATUS_CONNECT) {               // Port is connected
      if (pRootHub->PortResetEnumState >= RH_PORTRESET_WAIT_RESET) { // All states after port reset
        if (!(pEnumPort->PortStatus & PORT_STATUS_ENABLED)) {         // Port is not enabled
          USBH_WARN((USBH_MTYPE_HUB, "Roothub: _ProcessPortResetSetAddress: Port disabled during port reset"));
          _PortResetRestart(pRootHub, USBH_STATUS_PORT);
          return;
        }
      }
    } else { // Port not connected after port reset state
      pRootHub->PortResetEnumState = RH_PORTRESET_REMOVED;
    }
  }
  USBH_LOG((USBH_MTYPE_HUB, "Roothub: ROOT_HUB_PORT_RESET: %s", USBH_RhPortResetState2Str(pRootHub->PortResetEnumState)));
  switch (pRootHub->PortResetEnumState) {
  case RH_PORTRESET_REMOVED:                // Mark port as removed and set enumeration state to idle check other root pHub ports
    _PortResetRemove(pRootHub);
    break;
  case RH_PORTRESET_START:                  // Normal port reset: wait 100ms before reseting the port
    USBH_BD_SetPortState(pEnumPort, PORT_RESET);
    pRootHub->PortResetEnumState = RH_PORTRESET_WAIT_RESTART;
    USBH_URB_SubStateWait(&pRootHub->SubState, WAIT_AFTER_CONNECT, NULL);
    break;
  case RH_PORTRESET_RESTART:                // Delayed port reset: wait about one second
    USBH_BD_SetPortState(pEnumPort, PORT_RESET);
    pRootHub->PortResetEnumState = RH_PORTRESET_WAIT_RESTART;
    USBH_URB_SubStateWait(&pRootHub->SubState, DELAY_FOR_REENUM, NULL);
    break;
  case RH_PORTRESET_WAIT_RESTART:
    pRootHub->PortResetEnumState = RH_PORTRESET_RES;
    // Set to zero, expect that a state notification is returned, if the reset is complete
    pEnumPort->PortStatusShadow  = 0;
    // Port reset, if after the timeout the port state is not connected then the next state is RH_PORTRESET_REMOVED
    pDriver->pfResetPort(pRootHub->pHostController->hHostController, pEnumPort->HubPortNumber);
    USBH_URB_SubStateWait(&pRootHub->SubState, DEFAULT_RESET_TIMEOUT, NULL);
    break;
  case RH_PORTRESET_RES:                    // Ok, port is enabled
    USBH_BD_SetPortState(pEnumPort, PORT_ENABLED);
    pEnumPort->PortSpeed = USBH_FULL_SPEED; // Get the speed
    if (pEnumPort->PortStatus & PORT_STATUS_LOW_SPEED) {
      pEnumPort->PortSpeed = USBH_LOW_SPEED;
    }
    if (pEnumPort->PortStatus & PORT_STATUS_HIGH_SPEED) {
      pEnumPort->PortSpeed = USBH_HIGH_SPEED;
    }
    // Wait some time that the device can recover, this time is not defined by the specification
    // but windows makes a gap, so we will do it in the same way
    pRootHub->PortResetEnumState = RH_PORTRESET_WAIT_RESET;
    USBH_LOG((USBH_MTYPE_HUB, "Roothub: ROOT_HUB_PORT_RESET: TAL_StartTimer(WAIT_AFTER_RESET)"));
    USBH_URB_SubStateWait(&pRootHub->SubState, WAIT_AFTER_RESET, NULL);
    break;
  case RH_PORTRESET_WAIT_RESET:
    pRootHub->pEnumDevice = USBH_CreateNewUsbDevice(pRootHub->pHostController); // Now create a new device
    if (pRootHub->pEnumDevice == NULL) { // On error abort the port enumeration
      USBH_WARN((USBH_MTYPE_HUB, "Roothub: ROOT_HUB_PORT_RESET: USBH_CreateNewUsbDevice fails, no memory, no retry!"));
      _PortResetSetPortError(pRootHub, USBH_STATUS_RESOURCES);
      break;
    }
    // Init the device structure
    pRootHub->pEnumDevice->DeviceSpeed         = pEnumPort->PortSpeed;
    pRootHub->pEnumDevice->UsbAddress          = USBH_BD_GetUsbAddress(pRootHub->pHostController);
    // Backward pointer to the pHub port, the ports device pointer is set after complete enumeration of this device.
    // The state machine of the later device enumeration checks the port state and delete the device if the state is removed.
    pRootHub->pEnumDevice->pParentPort          = pEnumPort;
    pRootHub->pEnumDevice->ConfigurationIndex  = pEnumPort->ConfigurationIndex;
    // Prepare the set address request
    pUrb = &pRootHub->EnumUrb;
    USBH_ZERO_MEMORY(pUrb, sizeof(USBH_URB));
    pUrb->Header.Function                      = USBH_FUNCTION_CONTROL_REQUEST;
    pUrb->Request.ControlRequest.Setup.Type    = 0x00; // STD, OUT, device
    pUrb->Request.ControlRequest.Setup.Request = USB_REQ_SET_ADDRESS;
    pUrb->Request.ControlRequest.Setup.Value   = pRootHub->pEnumDevice->UsbAddress;
    // Select one of the preallocated endpoints
    switch (pEnumPort->PortSpeed) {
      case USBH_LOW_SPEED:
        pRootHub->hEnumEP = pRootHub->pHostController->LowSpeedEndpoint;
        break;
      case USBH_FULL_SPEED:
        pRootHub->hEnumEP = pRootHub->pHostController->FullSpeedEndpoint;
        break;
      case USBH_HIGH_SPEED:
        pRootHub->hEnumEP = pRootHub->pHostController->HighSpeedEndpoint;
        break;
      default:
        USBH_ASSERT0;
    }
    // Set a new  state
    pRootHub->PortResetEnumState = RH_PORTRESET_SET_ADDRESS;
    // Setup a timer if the device does not answer
    // Submit the request
    Status = USBH_URB_SubStateSubmitRequest(&pRootHub->SubState, &pRootHub->EnumUrb, DEFAULT_SETUP_TIMEOUT, pRootHub->pEnumDevice);
    if (Status != USBH_STATUS_PENDING) { // Error on submitting: set port to PORT_ERROR
      USBH_WARN((USBH_MTYPE_HUB, "Roothub: ROOT_HUB_PORT_RESET: USBH_URB_SubStateSubmitRequest failed %08x",Status));
      _PortResetSetPortError(pRootHub, Status);
    }
    break;
  case RH_PORTRESET_SET_ADDRESS:
    if (pRootHub->EnumUrb.Header.Status != USBH_STATUS_SUCCESS) {
      USBH_WARN((USBH_MTYPE_HUB, "Roothub: ROOT_HUB_PORT_RESET:RH_PORTRESET_SET_ADDRESS failed st:%08x",pRootHub->
        EnumUrb.Header.Status));
      _PortResetRestart(pRootHub, pRootHub->EnumUrb.Header.Status);
      break;
    }
    // Ok, now the device is addressed, wait some ms to let the device switch to the new address
    pRootHub->PortResetEnumState = RH_PORTRESET_WAIT_ADDRESS;
    USBH_URB_SubStateWait(&pRootHub->SubState, WAIT_AFTER_SETADDRESS, NULL);
    break;
  case RH_PORTRESET_WAIT_ADDRESS: {
    USB_DEVICE * dev;
    // 1. The device that is connected to the port is added after successfully enumeration (Port->Device = Device)
    // 2. start the device enumeration process
    // 3. release this port enumeration and wait for connecting other ports! at this point the port state is PORT_ENABLED!
    pEnumPort->ConfigurationIndex = 0;
    dev                           = pRootHub->pEnumDevice;
    // Prevent access to the enum device after starting the enumeration process
    pRootHub->pEnumDevice          = NULL;
    USBH_StartEnumeration(dev, USBH_CreateInterfaces, dev);
    _PortResetSetIdleServicePorts(pRootHub);
  }
    break;
  default:
    USBH_ASSERT0;
  }
}

/*********************************************************************
*
*       _InitPortsCompletion
*
*  Function description:
*    Called in the context of an timer routine after elapses the power good time
*    of the root pHub. This routine is called once after the pHub objects created.
*/
static void _InitPortsCompletion(void * pRoot_Hub) {
  USBH_DLIST          * pList;
  USBH_HUB_PORT       * pEnumPort;
  ROOT_HUB       * pRootHub;
  USBH_HOST_DRIVER * pHostEntry;

  pRootHub = (ROOT_HUB *)pRoot_Hub;
  USBH_ASSERT_MAGIC(pRootHub, ROOT_HUB);
  pHostEntry = pRootHub->pHostController->pDriver;
  pList      = USBH_DLIST_GetNext(&pRootHub->PortList);
  // Read the port Status from the field of all root pHub ports before any root pHub port is serviced
  while (pList != &pRootHub->PortList) {
    pEnumPort = GET_HUB_PORT_PTR(pList);
    USBH_ASSERT_MAGIC(pEnumPort, USBH_HUB_PORT);
    pList         = USBH_DLIST_GetNext(pList);
    pEnumPort->PortStatus       = pHostEntry->pfGetPortStatus(pRootHub->pHostController->hHostController, pEnumPort->HubPortNumber);
    pEnumPort->PortStatusShadow = pEnumPort->PortStatus;
    if (pEnumPort->PortStatus & PORT_STATUS_CONNECT) {
      // Device is connected, mark service required
      USBH_BD_SetPortState(pEnumPort, PORT_CONNECTED);
    }
  }
  pRootHub->PortResetEnumState = RH_PORTRESET_IDLE;
  USBH_HC_ServicePorts(pRootHub->pHostController);
  HC_DEC_REF(pRootHub->pHostController);
}

/*********************************************************************
*
*       Static code
*
**********************************************************************
*/

/*********************************************************************
*
*       USBH_ROOTHUB_Init
*
*  Function description:
*/
USBH_STATUS USBH_ROOTHUB_Init(USBH_HOST_CONTROLLER * pHostController) {
  USBH_STATUS   status;
  ROOT_HUB    * pRootHub = &pHostController->RootHub;

  IFDBG(pRootHub->Magic     = ROOT_HUB_MAGIC);
  pRootHub->pHostController = pHostController;
  USBH_DLIST_Init(&pRootHub->PortList);
  status = USBH_URB_SubStateInit(&pRootHub->SubState, pHostController, &pRootHub->hEnumEP, _ProcessPortResetSetAddress, pRootHub);
  if (status) {
    USBH_WARN((USBH_MTYPE_HUB, "Roothub: USBH_ROOTHUB_Init: USBH_URB_SubStateInit st: %08x",status));
    return status;
  }
  status = USBH_URB_SubStateInit(&pRootHub->InitHubPortSubState, pHostController, NULL, _InitPortsCompletion, pRootHub);
  if (status) {
    USBH_WARN((USBH_MTYPE_HUB, "Roothub: USBH_ROOTHUB_Init: USBH_URB_SubStateInit st: %08x",status));
  }
  return status;
}


/*********************************************************************
*
*       USBH_ROOTHUB_Release
*
*  Function description:
*/
void USBH_ROOTHUB_Release(ROOT_HUB * pRootHub) {
  USBH_URB_SubStateExit(&pRootHub->SubState);
  USBH_URB_SubStateExit(&pRootHub->InitHubPortSubState);
  _RemoveAllPorts(pRootHub);                 // Release all root pHub ports
}

/*********************************************************************
*
*       USBH_ROOTHUB_OnNotification
*
*  Function description:
*    Called from the Host controller driver if an root hub event occurs
*    bit0 indicates a Status change of the HUB, bit 1 of port 1 of the Hub and so on.
*
*  Parameters:
*    pRootHubContext    -
*    Notification    -
*
*  Return value:
*    void       -
*/
void USBH_ROOTHUB_OnNotification(void * pRootHubContext, U32 Notification) {
  USBH_HUB_PORT       * pHubPort;
  USBH_HOST_DRIVER * pDriver;
  ROOT_HUB       * pRootHub;
  U32              HubStatus;
  U8               i;

  pRootHub   = (ROOT_HUB * )pRootHubContext;
  pDriver = pRootHub->pHostController->pDriver;
  USBH_LOG((USBH_MTYPE_HUB, "Roothub: ROOT_HUB_NOTIFY: 0x%x!",Notification));
  USBH_ASSERT_MAGIC(pRootHub, ROOT_HUB);
  if (pRootHub->pHostController->State < HC_WORKING) {
    USBH_WARN((USBH_MTYPE_HUB, "Roothub: USBH_ROOTHUB_OnNotification: got Notification: 0x%x while HC State is %s",Notification,USBH_HcState2Str));
    pRootHub->PortResetEnumState = RH_PORTRESET_REMOVED;       // This prevents the state machine to call the HC and it stops it, frepList resources
  }
  if (Notification & 0x01) {
    HubStatus = pDriver->pfGetHubStatus(pRootHub->pHostController->hHostController); // Hub Status change (port 0 is the hub self)
    USBH_LOG((USBH_MTYPE_HUB, "Roothub: ROOT_HUB_NOTIFY: HubStatus %08x",HubStatus));
    // Clear the change bits
    if (HubStatus & HUB_STATUS_LOCAL_POWER) {  // The root pHub is always self powered, this should not happen
      pDriver->pfClearHubStatus(pRootHub->pHostController->hHostController, HDC_SELECTOR_C_HUB_LOCAL_POWER);
    }
    if (HubStatus & HUB_STATUS_OVER_CURRENT) { // Hub reports an over-current on the global scope
      pDriver->pfClearHubStatus(pRootHub->pHostController->hHostController, HDC_SELECTOR_C_HUB_OVER_CURRENT);
    }
  }
  // Check device notifications
  for (i = 1; i <= pRootHub->PortCount; i++) {
    U32 notification;
    notification = Notification &(0x00000001 << i);
    if (notification != 0) {
      pHubPort = USBH_ROOTHUB_GetPortByNumber(pRootHub, i);
      if (pHubPort == NULL) {
        USBH_WARN((USBH_MTYPE_HUB, "Roothub: ROOT_HUB_NOTIFY: HC returns invalid notifications %08x",Notification));
      } else {
        // Get the current port Status, it is processed later with USBH_ROOTHUB_ServicePorts
        pHubPort->PortStatus       = pDriver->pfGetPortStatus(pRootHub->pHostController->hHostController, pHubPort->HubPortNumber);
        pHubPort->PortStatusShadow = pHubPort->PortStatus; // Shadow register used from enumeration for reset and compare
        // This port needs service, the current port Status is in pHubPort->PortStatus
        // Clear the change bits in the host
        if (pHubPort->PortStatus & PORT_C_STATUS_CONNECT) {
          pHubPort->PortStatus &= ~PORT_C_STATUS_CONNECT;
          pDriver->pfClearPortStatus(pRootHub->pHostController->hHostController, pHubPort->HubPortNumber, HDC_SELECTOR_C_PORT_CONNECTION);
        }
        if (pHubPort->PortStatus & PORT_C_STATUS_ENABLE) {
          pHubPort->PortStatus &= ~PORT_C_STATUS_ENABLE;
          pDriver->pfClearPortStatus(pRootHub->pHostController->hHostController, pHubPort->HubPortNumber, HDC_SELECTOR_C_PORT_ENABLE);
        }
        if (pHubPort->PortStatus & PORT_C_STATUS_SUSPEND) {
          pHubPort->PortStatus &= ~PORT_C_STATUS_SUSPEND;
          pDriver->pfClearPortStatus(pRootHub->pHostController->hHostController, pHubPort->HubPortNumber, HDC_SELECTOR_C_PORT_SUSPEND);
        }
        if (pHubPort->PortStatus & PORT_C_STATUS_OVER_CURRENT) {
          pHubPort->PortStatus &= ~PORT_C_STATUS_OVER_CURRENT;
          pDriver->pfClearPortStatus(pRootHub->pHostController->hHostController, pHubPort->HubPortNumber, HDC_SELECTOR_C_PORT_OVER_CURRENT);
        }
        if (pHubPort->PortStatus & PORT_C_STATUS_RESET) {
          pHubPort->PortStatus &= ~PORT_C_STATUS_RESET;
          pDriver->pfClearPortStatus(pRootHub->pHostController->hHostController, pHubPort->HubPortNumber, HDC_SELECTOR_C_PORT_RESET);
        }
        // Process port Status information if the port if the port is not enumerated
        if (pHubPort != pRootHub->pEnumPort) {                                      // This port is not enumerated now
          if (pHubPort->PortStatus & PORT_STATUS_OVER_CURRENT) {                   // Check over current
            USBH_LOG((USBH_MTYPE_HUB, "Roothub: ROOT_HUB_NOTIFY: PORT_STATUS_OVER_CURRENT Port:%d Status:%08x",pHubPort->HubPortNumber,pHubPort->PortStatus));
            // The device uses too much current, remove it
            if (pHubPort->Device != NULL) {
              USBH_MarkParentAndChildDevicesAsRemoved(pHubPort->Device);
            }
            pDriver->pfDisablePort(pRootHub->pHostController->hHostController, pHubPort->HubPortNumber); // Disable the port to avoid fire -)
            USBH_BD_SetPortState(pHubPort, PORT_RESTART);                              // Should we restart a device over current? Its better than forget it!
          }
          // New connection
          if ((pHubPort->PortStatus &PORT_STATUS_CONNECT) && !(pHubPort->PortStatus &PORT_STATUS_ENABLED)) {
            // This device must be enumerated
            if (pHubPort->PortState >= PORT_ENABLED && pHubPort->Device != NULL) {
              // Remove the old connected device first
              USBH_LOG((USBH_MTYPE_HUB, "Roothub: ROOT_HUB_NOTIFY: delete dev., port connected but not enabled Port:%d Status:%08x", pHubPort->HubPortNumber,pHubPort->PortStatus));
              USBH_MarkParentAndChildDevicesAsRemoved(pHubPort->Device);
            }
            pHubPort->RetryCounter = 0;
            USBH_BD_SetPortState(pHubPort, PORT_CONNECTED);
          }
          // Device removed
          if (!(pHubPort->PortStatus &PORT_STATUS_CONNECT)) {
            if (pHubPort->Device != NULL) { // This device is removed
              // Remove the old connected device first
              USBH_LOG((USBH_MTYPE_HUB, "Roothub: ROOT_HUB_NOTIFY: port not connected, delete dev., Port:%d Status:%08x", pHubPort->HubPortNumber, pHubPort->PortStatus));
              USBH_MarkParentAndChildDevicesAsRemoved(pHubPort->Device);
            }
            USBH_LOG((USBH_MTYPE_HUB, "Roothub: ROOT_HUB_NOTIFY: port removed!"));
            USBH_BD_SetPortState(pHubPort, PORT_REMOVED);
            pDriver->pfDisablePort(pRootHub->pHostController->hHostController, pHubPort->HubPortNumber); // Disable the port
          }
        }
      }
    }
  }
  USBH_HC_ServicePorts(pRootHub->pHostController); // Service all ports
}

/*********************************************************************
*
*       USBH_ROOTHUB_ServicePorts
*
*  Function description:
*    Called after a notification or if the enumeration of a device has finished
*/
USBH_BOOL USBH_ROOTHUB_ServicePorts(ROOT_HUB * pRootHub) {
  USBH_HUB_PORT * pHubPort;
  USBH_DLIST    * pList;
  if (pRootHub->PortResetEnumState != RH_PORTRESET_IDLE || pRootHub->pHostController->State < HC_WORKING) {
    // Enumeration is active or the root pHub is not working, do nothing now, we are called later
    return TRUE;
  }
  // Run a second time over all ports, to sepList if a port needs a reset
  pList = USBH_DLIST_GetNext(&pRootHub->PortList);
  while (pList != &pRootHub->PortList) {
    pHubPort = GET_HUB_PORT_PTR(pList);
    USBH_ASSERT_MAGIC(pHubPort, USBH_HUB_PORT);
    pList       = USBH_DLIST_GetNext(pList);
    if (pHubPort->PortState == PORT_RESTART || pHubPort->PortState == PORT_CONNECTED) {
      pHubPort->RetryCounter++;
      if (pHubPort->RetryCounter > RESET_RETRY_COUNTER) {
        USBH_WARN((USBH_MTYPE_HUB, "Roothub: USBH_ROOTHUB_ServicePorts: max. port retries -> PORT_ERROR!"));
        USBH_BD_SetPortState                         (pHubPort, PORT_ERROR);
        UbdSetEnumErrorNotificationRootPortReset(pHubPort, pRootHub->PortResetEnumState, USBH_STATUS_ERROR);
      } else {
        if (pHubPort->PortState == PORT_RESTART) {   // PORT_CONNECTED or PORT_RESTART
          pRootHub->PortResetEnumState = RH_PORTRESET_RESTART;
        } else {
          pRootHub->PortResetEnumState = RH_PORTRESET_START;
        }
        HC_INC_REF(pRootHub->pHostController);
        pRootHub->pEnumPort = pHubPort;
        USBH_HC_SetActivePortReset       (pRootHub->pHostController, pHubPort);
        _ProcessPortResetSetAddress(pRootHub); // Start the port reset
        return TRUE;
      }
    }
  }
  return FALSE;
}


/*********************************************************************
*
*       USBH_ROOTHUB_AddPortsStartPowerGoodTime
*
*  Function description:
*    Create all need root pHub ports and starts an timer routine with the
*    power good time. In the timer routine an routine is called that
*    services all ports for connected devices!
*/
USBH_STATUS USBH_ROOTHUB_AddPortsStartPowerGoodTime(ROOT_HUB * pRootHub) {
  USBH_HUB_PORT        * pHubPort;
  U8                i;
  USBH_HOST_CONTROLLER * pHostController = pRootHub->pHostController;
  USBH_HOST_DRIVER  * pDriver       = pHostController->pDriver;

  USBH_ASSERT_MAGIC(pRootHub, ROOT_HUB);
  pRootHub->PortCount     = pDriver->pfGetPortCount    (pHostController->hHostController);
  pRootHub->PowerGoodTime = pDriver->pfGetPowerGoodTime(pHostController->hHostController);
  for (i = 1; i <= pRootHub->PortCount; i++) {
    pDriver->SetPortPower(pHostController->hHostController, i, 1); // Turn the power on
    pHubPort = USBH_BD_NewHubPort();                          // Create a new pHub port port state is port_removed
    if (pHubPort == NULL) {
      USBH_WARN((USBH_MTYPE_HUB, "Roothub: USBH_ROOTHUB_AddPortsStartPowerGoodTime: USBH_BD_NewHubPort failed"));
      return USBH_STATUS_RESOURCES;
    }
    // Init the pHub port
    pHubPort->HighPowerFlag = TRUE;
    pHubPort->HubPortNumber = i;
    pHubPort->RootHub       = pRootHub;
    USBH_DLIST_InsertTail(&pRootHub->PortList, &pHubPort->ListEntry);
  }
  pRootHub->PortResetEnumState = RH_PORTRESET_INIT; // RH_PORTRESET_INIT prevents starting of root pHub enumeration until power good time is elapsed!
  HC_INC_REF(pHostController);                      // Hold the host controller
  USBH_URB_SubStateWait(&pRootHub->InitHubPortSubState, pRootHub->PowerGoodTime, NULL); // Wait for Power good time
  return USBH_STATUS_SUCCESS;
}

/*********************************************************************
*
*       USBH_ROOTHUB_GetPortByNumber
*
*  Function description:
*/
USBH_HUB_PORT * USBH_ROOTHUB_GetPortByNumber(ROOT_HUB * pRootHub, U8 Port) {
  USBH_HUB_PORT * HubPort;
  USBH_DLIST    * e;
  USBH_ASSERT_MAGIC(pRootHub, ROOT_HUB);
  e = USBH_DLIST_GetNext(&pRootHub->PortList);
  while (e != &pRootHub->PortList) {
    HubPort = GET_HUB_PORT_PTR(e);
    USBH_ASSERT_MAGIC(HubPort, USBH_HUB_PORT);
    e       = USBH_DLIST_GetNext(e);
    if (HubPort->HubPortNumber == Port) {
      return HubPort;
    }
  }
  return NULL;
}

/******************************* EOF ********************************/
