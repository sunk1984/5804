/*********************************************************************
*  
*   IAR PowerPac
*
*   (c) Copyright IAR Systems 2010.  All rights reserved.
*
**********************************************************************
----------------------------------------------------------------------
File        : USBH_Hub.c
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


#define HC_CLEAR_ACTIVE_PORTRESET(hostPtr, enumPortPtr) {                                          \
  T_ASSERT_MAGIC((hostPtr), HOST_CONTROLLER);                                                      \
  if (NULL == (hostPtr)->ActivePortReset) {                                                        \
    USBH_WARN((USBH_MTYPE_HUB, "HcClearActivePortReset: ActivePortReset already NULL!\n"));        \
  } else {                                                                                         \
    T_ASSERT_MAGIC( (enumPortPtr), HUB_PORT);                                                      \
    if ( (hostPtr)->ActivePortReset != (enumPortPtr) ) {                                           \
      USBH_WARN((USBH_MTYPE_HUB, "HcClearActivePortReset: not the same port as at the start!\n")); \
    }                                                                                              \
    (hostPtr)->ActivePortReset=NULL;                                                               \
  }                                                                                                \
}

#define HC_SET_ACTIVE_PORTRESET(hostPtr, enumPortPtr) {                                  \
  T_ASSERT_MAGIC((hostPtr), HOST_CONTROLLER);                                            \
  if (NULL != (hostPtr)->ActivePortReset) {                                              \
    USBH_WARN((USBH_MTYPE_HUB, "HcSetActivePortReset: ActivePortReset is not NULL!\n")); \
  } else {                                                                               \
    T_ASSERT_MAGIC((enumPortPtr), HUB_PORT);                                             \
    (hostPtr)->ActivePortReset=(enumPortPtr);                                            \
  }                                                                                      \
}

#if USBH_EXTHUB_SUPPORT

/*********************************************************************
*
*       HubSetNotifyState
*/
static void HubSetNotifyState(USB_HUB * hub, HUB_NOTIFY_STATE state) {
  T_ASSERT_MAGIC(hub, USB_HUB);
  USBH_LOG((USBH_MTYPE_HUB, "HUB_NOTIFY: old: %s new: %s", UbdHubNotificationStateStr(hub->NotifyState), UbdHubNotificationStateStr(state)));
  hub->OldNotifyState = hub->NotifyState;
  hub->NotifyState    = state;
}

/*********************************************************************
*
*       HubPrepareGetPortStatus
*/
static void HubPrepareGetPortStatus(URB * urb, U16 selector, void * buffer, U16 bufferLength) {
  T_ASSERT(HCD_GET_STATUS_LENGTH <= bufferLength);
  USBH_LOG((USBH_MTYPE_HUB, "HubPrepareGetPortStatus: port: %d!",(int)selector));
  ZERO_MEMORY(urb, sizeof(URB));

  bufferLength = bufferLength; // unused

	urb->Header.Function = USBH_FUNCTION_CONTROL_REQUEST;
  if (selector) {
    urb->Request.ControlRequest.Setup.Type = USB_TO_HOST | USB_REQTYPE_CLASS | USB_OTHER_RECIPIENT;  // Device
  } else {
    urb->Request.ControlRequest.Setup.Type = USB_TO_HOST | USB_REQTYPE_CLASS | USB_DEVICE_RECIPIENT; // Port
  }
	urb->Request.ControlRequest.Setup.Request = HDC_REQTYPE_GET_STATUS;
	urb->Request.ControlRequest.Setup.Value   = 0;
	urb->Request.ControlRequest.Setup.Index   = selector;
	urb->Request.ControlRequest.Setup.Length  = HCD_GET_STATUS_LENGTH;
  urb->Request.ControlRequest.Buffer        = buffer;
	urb->Request.ControlRequest.Length        = HCD_GET_STATUS_LENGTH;
}

/*********************************************************************
*
*       HubPrepareStandardOutRequest
*/
static void HubPrepareStandardOutRequest(URB * urb,U8 request, U16 value, U16 index) {
  USBH_LOG((USBH_MTYPE_HUB, "HubPrepareStandardOutRequest: requst: %d!",(int)request));
  ZERO_MEMORY(urb,sizeof(URB));
	urb->Header.Function                      = USBH_FUNCTION_CONTROL_REQUEST;
  urb->Request.ControlRequest.Setup.Type    = 0x00; // STD, OUT, device
	urb->Request.ControlRequest.Setup.Request = request;
	urb->Request.ControlRequest.Setup.Value   = value;
  urb->Request.ControlRequest.Setup.Index   = index;
}

/*********************************************************************
*
*       HubPrepareGetHubDesc
*/
static void HubPrepareGetHubDesc(URB * urb, void * buffer, U16 reqLength) {
  U16 length;
	USBH_LOG((USBH_MTYPE_HUB, "HubPrepareGetDescClassReq: length:%d!",(int)reqLength));
  length = USBH_MIN(reqLength, HDC_MAX_HUB_DESCRIPTOR_LENGTH);
  ZERO_MEMORY(urb,sizeof(URB));
	urb->Header.Function                      = USBH_FUNCTION_CONTROL_REQUEST;
	urb->Request.ControlRequest.Setup.Type    = USB_TO_HOST | USB_REQTYPE_CLASS; // class request, IN, device
	urb->Request.ControlRequest.Setup.Request = USB_REQ_GET_DESCRIPTOR;
	urb->Request.ControlRequest.Setup.Value   = (U16)(USB_HUB_DESCRIPTOR_TYPE << 8);
	urb->Request.ControlRequest.Setup.Length  = length;
	urb->Request.ControlRequest.Buffer        = buffer;
	urb->Request.ControlRequest.Length        = length;
}

/*********************************************************************
*
*       HubPrepareSetFeatureReq
*/
static void HubPrepareSetFeatureReq(URB * urb, U16 featureSelector, U16 selector) {
 	USBH_LOG((USBH_MTYPE_HUB, "HubPrepareSetFeatureReq: featureSelector: %d seletor: %d!", (int)featureSelector, (int)selector));
  ZERO_MEMORY(urb, sizeof(URB));
	urb->Header.Function                      = USBH_FUNCTION_CONTROL_REQUEST;
	urb->Request.ControlRequest.Setup.Type    = USB_REQTYPE_CLASS | USB_OTHER_RECIPIENT; // class request, IN, device
	urb->Request.ControlRequest.Setup.Request = USB_REQ_SET_FEATURE;
	urb->Request.ControlRequest.Setup.Value   = featureSelector;
	urb->Request.ControlRequest.Setup.Index   = selector;
	urb->Request.ControlRequest.Setup.Length  = 0;
}

/*********************************************************************
*
*       UbdHubPrepareClrFeatureReq
*/
void UbdHubPrepareClrFeatureReq(URB * urb, U16 feature, U16 selector) {
  USBH_LOG((USBH_MTYPE_HUB, "UbdHubPrepareClrFeatureReq: feature: %d port: %d!", (int)feature, (int)selector));
 	ZERO_MEMORY(urb, sizeof(URB));
	urb->Header.Function = USBH_FUNCTION_CONTROL_REQUEST;
  if (selector) {
    urb->Request.ControlRequest.Setup.Type = USB_REQTYPE_CLASS | USB_OTHER_RECIPIENT;  // class request, IN, device
  } else {
    urb->Request.ControlRequest.Setup.Type = USB_REQTYPE_CLASS | USB_DEVICE_RECIPIENT; // class request, IN, device
  }
	urb->Request.ControlRequest.Setup.Request = USB_REQ_CLEAR_FEATURE;
	urb->Request.ControlRequest.Setup.Value   = feature;
	urb->Request.ControlRequest.Setup.Index   = selector ;
}

/*********************************************************************
*
*       ParseHubDescriptor
*/
static USBH_STATUS ParseHubDescriptor(USB_HUB * hub, U8 * buffer, U32 length) {
  T_ASSERT_MAGIC(hub, USB_HUB);
  T_ASSERT_PTR(buffer);

  if (length < HDC_DESC_MIN_LENGTH) {
    USBH_LOG((USBH_MTYPE_HUB, " ParseHubDescriptor: desc.-length: %lu ", length));
    return USBH_STATUS_INVALID_DESCRIPTOR;
  }
  hub->PortCount       = UbdGetUcharFromDesc( buffer,HDC_DESC_PORT_NUMBER_OFS);
  hub->Characteristics = UbdGetUshortFromDesc(buffer,HDC_DESC_CHARACTERISTICS_LOW_OFS);
  hub->PowerGoodTime   = UbdGetUcharFromDesc(buffer,HDC_DESC_POWER_GOOD_TIME_OFS);
  hub->PowerGoodTime   = hub->PowerGoodTime << 1;
  USBH_LOG((USBH_MTYPE_HUB, "ParseHubDescriptor: Ports: %d Character.: 0x%x powergoodtime: %d!", hub->PortCount, hub->Characteristics,hub->PowerGoodTime));
  return USBH_STATUS_SUCCESS;
}

/*********************************************************************
*
*       HubNotifyRestart
*/
static void HubNotifyRestart(USB_HUB* hub) {
  HUB_PORT * parentPort;
  T_ASSERT_MAGIC(hub,            USB_HUB);
  T_ASSERT_MAGIC(hub->HubDevice, USB_DEVICE);
  HubSetNotifyState(hub, HUB_NOTIFY_ERROR);
  parentPort = hub->HubDevice->ParentPort;
  if (parentPort != NULL) {
    T_ASSERT_MAGIC(parentPort, HUB_PORT);
    USBH_WARN((USBH_MTYPE_HUB, "HubNotifyRestart: hub ref.ct %ld portnumber: %d portstate: %s", hub->HubDevice->RefCount,
              (int)parentPort->HubPortNumber, UbdPortStateStr(parentPort->PortState)));
    UbdSetPortState(parentPort, PORT_RESTART);
  }
  UbdUdevMarkParentAndChildDevicesAsRemoved(hub->HubDevice);
  DEC_REF(hub->HubDevice); // Delete notify state machine reference
  // Service all ports
  UbdHcServicePorts(hub->HubDevice->HostController);
}

/*********************************************************************
*
*       HubNotifyError
*
*  Function description
*  Called on fatal errors (errors returned from URB submit routines) in the hub
*  notification routine. The parent port is set to state PORT_ERROR. The hub device
*  and all conntected child devices are deleted. At last the local reference is deleted.
*/
static void HubNotifyError(USB_HUB * hub) {
  HUB_PORT * parentPort;
  T_ASSERT_MAGIC(hub,            USB_HUB);
  T_ASSERT_MAGIC(hub->HubDevice, USB_DEVICE);
  HubSetNotifyState(hub,HUB_NOTIFY_ERROR);
  parentPort=hub->HubDevice->ParentPort;
  if (parentPort != NULL) {
    T_ASSERT_MAGIC(parentPort, HUB_PORT);
    USBH_WARN((USBH_MTYPE_HUB, "HubNotifyError: hub ref.ct %ld portnumber: %d portstate: %s", hub->HubDevice->RefCount, (int)parentPort->HubPortNumber,
               UbdPortStateStr(parentPort->PortState)));
    UbdSetPortState(parentPort, PORT_ERROR);
  }
  UbdUdevMarkParentAndChildDevicesAsRemoved(hub->HubDevice);
  DEC_REF(hub->HubDevice); // Delete notify state machine reference
  // Service all ports
  UbdHcServicePorts(hub->HubDevice->HostController);
}

/*********************************************************************
*
*       EnumPrepareGetDeviceStatus
*/
static void EnumPrepareGetDeviceStatus(USB_DEVICE * Dev, void * Buffer, U16 reqLength) {
  URB * urb;
  U16   length;
  length                                    = USBH_MIN(reqLength, USB_STATUS_LENGTH);
  urb                                       = &Dev->EnumUrb;
	ZERO_MEMORY(urb, sizeof(URB));
	urb->Header.Function                      = USBH_FUNCTION_CONTROL_REQUEST;
	urb->Request.ControlRequest.Setup.Type    = USB_STATUS_DEVICE; // STD, IN, device
	urb->Request.ControlRequest.Setup.Request = USB_REQ_GET_STATUS;
	urb->Request.ControlRequest.Setup.Length  = length;
  urb->Request.ControlRequest.Buffer        = Buffer;
	urb->Request.ControlRequest.Length        = length;
}

/*********************************************************************
*
*       HubEnumParentPortRestart
*/
static void HubEnumParentPortRestart(USB_HUB * hub, USBH_STATUS enum_status) {
  USBH_LOG((USBH_MTYPE_HUB, "HubEnumParentPortRestart!"));
  T_ASSERT_MAGIC(hub, USB_HUB);
  UbdEnumParentPortRestart(hub->HubDevice,enum_status); // Parent port restart
}

/*********************************************************************
*
*       HubEnumParentPortError
*/
static void HubEnumParentPortError(USB_HUB* hub, USBH_STATUS enumStatus) {
  USBH_LOG((USBH_MTYPE_HUB, "HubEnumParentPortError!"));
  T_ASSERT_MAGIC(hub, USB_HUB);
  UbdProcessEnumPortError(hub->HubDevice,enumStatus); // Parent port restart
}

/*********************************************************************
*
*       ProcessHubNotification
*
*  Function description:
*    Is called in the context of the completion routine of the hub
*    interrupt IN endpoint (hub status bitmap) or in the context in a
*    URB completion routine or in the context of an timerroutine if
*    waiting for an event. If the hub device state is not working the
*    hub device is removed!
*/
static void ProcessHubNotification(void * usbHub) {
  USB_HUB         * hub;
  HUB_PORT        * port;
  USB_DEVICE      * hub_dev;
  URB             * urb;
  USBH_STATUS       status;
  U32               notify;
  U32               mask;
  int               i;
  unsigned int      notify_length;
  HOST_CONTROLLER * host_controller;
  T_BOOL            hub_submit_error_flag;
  T_BOOL            restart_flag;

  hub_submit_error_flag = FALSE;
  restart_flag          = FALSE;

  hub             = (USB_HUB *)usbHub;
  T_ASSERT_MAGIC(hub,     USB_HUB);
  hub_dev         = hub->HubDevice;
  T_ASSERT_MAGIC(hub_dev, USB_DEVICE);
  urb             = &hub->NotifyUrb;
  host_controller = hub_dev->HostController;
  T_ASSERT_MAGIC(host_controller, HOST_CONTROLLER);
  USBH_LOG((USBH_MTYPE_HUB, "HUB: HUB_NOTIFY: %s ref.ct: %ld", UbdHubNotificationStateStr(hub->NotifyState), hub_dev->RefCount));
  if (hub_dev->State < DEV_STATE_WORKING) { // Check hubs device state
    USBH_LOG((USBH_MTYPE_HUB, "HUB: INFO HUB_NOTIFY: hub device is less than < DEV_STATE_WORKING"));
    HubSetNotifyState(hub, HUB_NOTIFY_REMOVED);
    hub_submit_error_flag = TRUE;           // Indirect call of HubNotifyError
  }
  switch (hub->NotifyState) {
    case HUB_NOTIFY_START:
      notify_length = hub->interruptUrb.Request.BulkIntRequest.Length;
      if (0 == notify_length) {             // Zero length packet recevied
        USBH_WARN((USBH_MTYPE_HUB, "HUB:  HUB_NOTIFY zero length pkt. on interrupt IN Ep!"));
        HubSetNotifyState(hub, HUB_NOTIFY_IDLE);
        break;
      }
      hub->Notification = USBH_LoadU32LE(hub->InterruptTransferBuffer);
      // Get buffer for all get status requests
      USBH_LOG((USBH_MTYPE_HUB, "HUB: HUB_NOTIFY_START: USB addr: %d portstatus: 0x%x nb.of rcv. bytes: %d!", (int)hub->HubDevice->UsbAddress, hub->Notification, notify_length));
      if (!UbdCheckCtrlTransferBuffer(hub_dev, HCD_GET_STATUS_LENGTH)) {      // Prepare the transfer buffer to get the port status
        USBH_WARN((USBH_MTYPE_HUB, "HUB: FATAL HUB_NOTIFY HUB_NOTIFY_START:UbdCheckCtrlTransferBuffer!"));
        return;
      }
      if (hub->Notification & 0x01) {
        USBH_LOG((USBH_MTYPE_HUB, "HUB: HUB_NOTIFY: hub status changed!"));
        HubPrepareGetPortStatus(urb, 0, hub_dev->CtrlTransferBuffer, (U16)hub_dev->CtrlTransferBufferSize); // Get hub status selector is zero!
        HubSetNotifyState(hub, HUB_NOTIFY_GET_HUB_STATUS);                                                             // Get hub status, clear hub status
        status = UrbSubStateSubmitRequest(&hub->NotifySubState, urb, DEFAULT_SETUP_TIMEOUT, NULL);
        if (status != USBH_STATUS_PENDING) {
          USBH_WARN((USBH_MTYPE_HUB, "HUB:  HUB_NOTIFY HUB_NOTIFY_START: UrbSubStateSubmitRequest: st:%08x", urb->Header.Status));
          hub_submit_error_flag = TRUE;
          break;
        } else {
          return; // Wait for completion
        }
      } else {
        hub->NotifyPortCt = hub->PortCount; // No hub notification available, start port status
        USBH_LOG((USBH_MTYPE_HUB, "HUB: HUB_NOTIFY: start get port status! ports: %d!",hub->PortCount));
        HubSetNotifyState(hub, HUB_NOTIFY_GET_PORT_STATUS);
        hub->SubmitFlag = FALSE;            // Prevent checking of URB in the new notify_state
        ProcessHubNotification(hub);        // Recursive
        return;
      }
    case HUB_NOTIFY_GET_HUB_STATUS:
      if (urb->Header.Status != USBH_STATUS_SUCCESS) {
        USBH_WARN((USBH_MTYPE_HUB, "HUB:  HUB_NOTIFY HUB_NOTIFY_GET_HUB_STATUS urb  st:%08x", urb->Header.Status)); // On error this can be also an timeout
        restart_flag = TRUE;                                                                                        // Try again with HUB_NOTIFY_START
        break;
      }
      // Get hub status
      T_ASSERT(urb->Request.ControlRequest.Length >= 4);
      hub->Status     = USBH_LoadU32LE(hub_dev->CtrlTransferBuffer);
      hub->NotifyTemp = hub->Status;
      // Prevent checking of URB in the new notify_state!
      hub->SubmitFlag = FALSE;
      USBH_LOG((USBH_MTYPE_HUB, "HUB: HUB_NOTIFY: HubStatus %08x",hub->Status));
      HubSetNotifyState(hub, HUB_NOTIFY_CLEAR_HUB_STATUS);
    case HUB_NOTIFY_CLEAR_HUB_STATUS: // Fall trough
      if (hub->SubmitFlag) {
        hub->SubmitFlag = FALSE;
        // Clear change status submitted
        if (urb->Header.Status != USBH_STATUS_SUCCESS) { // On error
          USBH_WARN((USBH_MTYPE_HUB, "HUB:  HUB_NOTIFY HUB_NOTIFY_CLEAR_HUB_STATUS urb st:%08x", urb->Header.Status));
          restart_flag = TRUE;                          // Try again
          break;
        }
      }
      // Check hub change bits
      hub->SubmitFlag = FALSE;
      if (hub->NotifyTemp & HUB_STATUS_C_LOCAL_POWER) {
        hub->NotifyTemp &= ~HUB_STATUS_C_LOCAL_POWER;
        USBH_LOG((USBH_MTYPE_HUB, "HUB: HUB_NOTIFY: HUB_STATUS_C_LOCAL_POWER is cleared"));
        UbdHubPrepareClrFeatureReq(urb, HDC_SELECTOR_C_HUB_LOCAL_POWER, 0);
        hub->SubmitFlag = TRUE;
      } else {
        if (hub->NotifyTemp & HUB_STATUS_C_OVER_CURRENT) {
          hub->NotifyTemp &= ~HUB_STATUS_C_OVER_CURRENT;
          UbdHubPrepareClrFeatureReq(urb, HDC_SELECTOR_C_HUB_OVER_CURRENT, 0);
          USBH_LOG((USBH_MTYPE_HUB, "HUB: HUB_NOTIFY: HUB_STATUS_C_OVER_CURRENT is cleared"));
          hub->SubmitFlag = TRUE;
        }
      }
      if (hub->SubmitFlag) {
        status = UrbSubStateSubmitRequest(&hub->NotifySubState, urb, DEFAULT_SETUP_TIMEOUT, NULL);
        if (status != USBH_STATUS_PENDING) {
          USBH_WARN((USBH_MTYPE_HUB, "HUB:  HUB_NOTIFY UrbSubStateSubmitRequest: Clear hub status st:%08x", urb->Header.Status));
          hub_submit_error_flag = TRUE;
          break;
        } else {
          return; // Wait for completion
        }
      } else { // All submitted
        hub->NotifyPortCt = hub->PortCount;
        HubSetNotifyState(hub, HUB_NOTIFY_GET_PORT_STATUS);
        hub->SubmitFlag   = FALSE;
      }
    case HUB_NOTIFY_GET_PORT_STATUS: // Fall trough
      // 1. no URB is submitted the first get port status URB is submitted
      // 2. After the first port completion the status change bit is cleared.
      // 3. After status change is cleared the connect, remove an dovercurrent conditions are checked.
      // 4. Until not all ports processed the state HUB_NOTIFY_GET_PORT_STATUS is started again
      // 5. If all ports processed the state is Idle and the next interrupt IN status request is submitted before UbdHcServicePorts is called
      if (hub->SubmitFlag) { // URB completed
        hub->SubmitFlag = FALSE;
        if (urb->Header.Status != USBH_STATUS_SUCCESS) { // On error
          USBH_WARN((USBH_MTYPE_HUB, "HUB:  HUB_NOTIFY HUB_NOTIFY_GET_PORT_STATUS urb st:%08x", urb->Header.Status));
          restart_flag = TRUE;                          // Try again
          break;
        }
        T_ASSERT(urb->Request.ControlRequest.Length >= 4);
        T_ASSERT_PTR(hub->NotifyPort);
        // Set the port status
        hub->NotifyTemp                   = USBH_LoadU32LE(hub_dev->CtrlTransferBuffer);
        hub->NotifyPort->PortStatus       = hub->NotifyTemp;
        hub->NotifyPort->PortStatusShadow = hub->NotifyTemp;
        USBH_LOG((USBH_MTYPE_HUB, "HUB: HUB_NOTIFY: port: %d port status: 0x%lx", hub->NotifyPort->HubPortNumber,hub->NotifyTemp));
        if (hub->NotifyTemp &(PORT_C_STATUS_CONNECT | PORT_C_STATUS_ENABLE | PORT_C_STATUS_SUSPEND | PORT_C_STATUS_OVER_CURRENT | PORT_C_STATUS_RESET)) {
          // Release this notify_state and delete all change bits in hub->NotifyTemp, return until all port states are cleared
          HubSetNotifyState(hub, HUB_NOTIFY_CLR_PORT_STATUS);
          hub->SubmitFlag = FALSE;
          ProcessHubNotification(hub); // Recursive call
          return;
        }
      }
      notify = 0;
      while (hub->NotifyPortCt) {
        notify = hub->Notification &(0x01 << hub->NotifyPortCt);
        hub->NotifyPortCt--;
        if (notify) {
          break;
        }
      }
      if (notify) {
        hub->NotifyPort = HubGetPortByNumber(hub, (U8)(hub->NotifyPortCt + 1));
        HubPrepareGetPortStatus(urb, (U16)(hub->NotifyPortCt + 1),              // Port number
        hub_dev->CtrlTransferBuffer, (U16)hub_dev->CtrlTransferBufferSize);
        hub->SubmitFlag = TRUE;
        status = UrbSubStateSubmitRequest(&hub->NotifySubState, urb, DEFAULT_SETUP_TIMEOUT, NULL);
        if (status != USBH_STATUS_PENDING) {
          USBH_WARN((USBH_MTYPE_HUB, "HUB:  HUB_NOTIFY UrbSubStateSubmitRequest: Get port status st:%08x", urb->Header.Status));
          hub_submit_error_flag = TRUE;
        } else {
          return; // Wait for completion
        }
      } else {
        HubSetNotifyState(hub, HUB_NOTIFY_IDLE); // All get status requests done, set idle and submit an new hub interrupt IN status URB
      }
      break;
    case HUB_NOTIFY_CLR_PORT_STATUS:
      port = hub->NotifyPort;
      T_ASSERT_MAGIC(port, HUB_PORT);
      // Clears all port status bits
      if (hub->SubmitFlag) {
        hub->SubmitFlag = FALSE;
        if (urb->Header.Status != USBH_STATUS_SUCCESS) { // On error
          USBH_WARN((USBH_MTYPE_HUB, "HUB:  HUB_NOTIFY HUB_NOTIFY_CLR_PORT_STATUS urb st:%08x", urb->Header.Status));
          // Try again
          restart_flag = TRUE;
          break;
        }
      }
      mask = PORT_C_STATUS_CONNECT;
      for (i = 0; i < 5; i++) {     // Check all five port change bits
        if (hub->NotifyTemp & mask) {
          hub->NotifyTemp &= ~mask; // clear change bit before submit the request
          // Submit clear prot status
          hub->SubmitFlag = TRUE;
          UbdHubPrepareClrFeatureReq(urb, (U16)(i + HDC_SELECTOR_C_PORT_CONNECTION), (U16)hub->NotifyPort->HubPortNumber);
          status = UrbSubStateSubmitRequest(&hub->NotifySubState, urb, DEFAULT_SETUP_TIMEOUT, NULL);
          if (status != USBH_STATUS_PENDING) {
            USBH_WARN((USBH_MTYPE_HUB, "HUB:  HUB_NOTIFY UrbSubStateSubmitRequest: Clear hub status st:%08x", urb->Header.Status));
            hub_submit_error_flag = TRUE;
            break;  // In for loop
          } else {
            return; // Wait for completion
          }
        }
        mask = mask << 1;
      }
      if (hub_submit_error_flag) {
        break;
      }
      // All change bits from the current port number are cleared! Update the state of an device if no device enumeration is running
      // and update the port state if no port reset enumeration is runnung!
      if (port != hub->EnumPort) {                             // On overcurrent mark the device as removed
        HubSetNotifyState(hub, HUB_NOTIFY_CHECK_OVER_CURRENT); // Port is not enumerated, update port state and device state in the next states
      } else {                                                 // Fall trough
        HubSetNotifyState(hub, HUB_NOTIFY_GET_PORT_STATUS);
        ProcessHubNotification(hub);                           // Recursive call
        return;
      }
    case HUB_NOTIFY_CHECK_OVER_CURRENT:
      port = hub->NotifyPort;
      T_ASSERT_MAGIC(port, HUB_PORT);
      if (hub->SubmitFlag) {                                   // Disable port request complete
        if (urb->Header.Status != USBH_STATUS_SUCCESS) {        // On error
          USBH_WARN((USBH_MTYPE_HUB, "HUB:  HUB_NOTIFY HUB_NOTIFY_CHECK_OVER_CURRENT urb st:%08x", urb->Header.Status));
          restart_flag = TRUE;
          break;
        }
      } else {
        if (port->PortStatus & PORT_C_STATUS_OVER_CURRENT) {
          port->PortStatus &= ~PORT_C_STATUS_OVER_CURRENT;
          if ((port->PortStatus &PORT_STATUS_OVER_CURRENT)) {  // Overcurrent
            USBH_LOG((USBH_MTYPE_HUB, "HUB: HUB_NOTIFY: over current Port:%d Status:%08x", port->HubPortNumber,port->PortStatus));
            if (port->Device != NULL) {
              UbdUdevMarkParentAndChildDevicesAsRemoved(port->Device);
            }
            UbdSetPortState(port, PORT_RESTART);               // Should we restart a device over current? Its better than forget it!
            HubSetNotifyState(hub, HUB_NOTIFY_DISABLE_PORT);
            ProcessHubNotification(hub);                       // Recursive call
            return;
          }

#if (USBH_DEBUG > 1)
          else {                                               // Overcurrent gone
            USBH_LOG((USBH_MTYPE_HUB, "HUB: HUB_NOTIFY: over current gone! Port:%d Status:%08x", port->HubPortNumber,port->PortStatus));
          }
#endif

        }
      }
      HubSetNotifyState(hub, HUB_NOTIFY_CHECK_CONNECT);        // Checking the port connect bit
    case HUB_NOTIFY_CHECK_CONNECT:                             // Fall trough
      port = hub->NotifyPort;
      T_ASSERT_MAGIC(port, HUB_PORT);

      if (port->PortStatus & PORT_C_STATUS_CONNECT) {
        if ((port->PortStatus &PORT_STATUS_CONNECT) && !(port->PortStatus &PORT_STATUS_ENABLED)) {
          USBH_LOG((USBH_MTYPE_HUB, "HUB: HUB_NOTIFY: Port:%d must be enumerated", (int)port->HubPortNumber));
          if (port->PortState >= PORT_ENABLED && port->Device != NULL) { // This device must be enumerated
            UbdUdevMarkParentAndChildDevicesAsRemoved(port->Device);     // Remove the old connected device first
          }
          UbdSetPortState(port, PORT_CONNECTED);
          port->RetryCounter = 0;
        }
      }
      HubSetNotifyState(hub, HUB_NOTIFY_CHECK_REMOVE); // Go to HUB_NOTIFY_CHECK_REMOVE
    case HUB_NOTIFY_CHECK_REMOVE: // Fall trough
      port = hub->NotifyPort;
      T_ASSERT_MAGIC(port, HUB_PORT);
      if (hub->SubmitFlag) {
        hub->SubmitFlag = FALSE;
        if (urb->Header.Status != USBH_STATUS_SUCCESS) {
          USBH_WARN((USBH_MTYPE_HUB, "HUB:  HUB_NOTIFY HUB_NOTIFY_CHECK_REMOVE urb st:%08x", urb->Header.Status)); // On error
          restart_flag = TRUE; // Try again
          break;
        }
      } else {
        if (port->PortStatus & PORT_C_STATUS_CONNECT) {
          port->PortStatus &= ~PORT_C_STATUS_CONNECT;          // PORT_C_STATUS_CONNECT no more used on this port, clear it
          if (!(port->PortStatus &PORT_STATUS_CONNECT)) {
            if (port->Device != NULL) {                        // Port is removed
              UbdUdevMarkParentAndChildDevicesAsRemoved(port->Device);
              USBH_LOG((USBH_MTYPE_HUB, "HUB: HUB_NOTIFY_CHECK_REMOVE: port is removed"));
              UbdSetPortState(port, PORT_REMOVED);
              port->RetryCounter = 0;
              HubSetNotifyState(hub, HUB_NOTIFY_DISABLE_PORT); // Disable the port
              ProcessHubNotification(hub);                     // Recursive
              return;
            }
          }
        }
      }
      HubSetNotifyState(hub, HUB_NOTIFY_GET_PORT_STATUS);
      ProcessHubNotification(hub);                             // Recursive
      return;
    //  Helper notify_state returns to OldNotifyState or delete the device on error!
    case HUB_NOTIFY_DISABLE_PORT:
      port = hub->NotifyPort;
      T_ASSERT_MAGIC(port, HUB_PORT);
      if (hub->SubmitFlag) {
        hub->SubmitFlag = FALSE;
        if (urb->Header.Status != USBH_STATUS_SUCCESS) {
          USBH_WARN((USBH_MTYPE_HUB, "HUB:  HUB_NOTIFY HUB_NOTIFY_DISABLE_PORT urb st:%08x", urb->Header.Status)); // On error
          restart_flag = TRUE;                  // Try again
          break;
        }
        hub->NotifyState = hub->OldNotifyState; // Go back to the previous state
        ProcessHubNotification(hub);            // Recursive
        return;
      } else {
        USBH_LOG((USBH_MTYPE_HUB, "HUB: HUB_NOTIFY: Port:%d is disabled", (int)port->HubPortNumber)); // Start submitting an port disable
        hub->SubmitFlag = TRUE;                                                                       // Disable the port to avoid fire -)
        UbdHubPrepareClrFeatureReq(urb, HDC_SELECTOR_PORT_ENABLE, port->HubPortNumber);               // Disable the port
        status = UrbSubStateSubmitRequest(&hub->NotifySubState, urb, DEFAULT_SETUP_TIMEOUT, NULL);
        if (status != USBH_STATUS_PENDING) {
          USBH_WARN((USBH_MTYPE_HUB, "HUB:  ProcessEnumHub: HUB_NOTIFY_DISABLE_PORT:" "UrbSubStateSubmitRequest st:%08x", urb->Header.Status));
          hub_submit_error_flag = TRUE; // submit error
        } else {
          return;                       // Wait for completion
        }
      }
      break;
    case HUB_NOTIFY_REMOVED:
      break; // Stop processing
    case HUB_NOTIFY_ERROR:
      break; // Stop processing
    case HUB_NOTIFY_IDLE:
      // At the end of this function an new HUB status request is submitted in this state. This can not made here because some states set the Idle state.
      break;
    default:
      T_ASSERT0;
      USBH_WARN((USBH_MTYPE_HUB, "HUB:  HUB_NOTIFY invalid  hub->NotifyState: %d", hub->NotifyState));
  }
  // Check error flags
  if (restart_flag) {
    HubNotifyRestart(hub);
  } else if (hub_submit_error_flag) {
    HubNotifyError(hub);
  } else {
    if (HUB_NOTIFY_IDLE == hub->NotifyState && DEV_STATE_WORKING == hub_dev->State) {     // On success
      hub->interruptUrb.Request.BulkIntRequest.Length = hub->InterruptTransferBufferSize; // ready hub device and notify state idle
      status = UbdEpSubmitUrb(hub->InterruptEp, &hub->interruptUrb);
      if (status != USBH_STATUS_PENDING) {
        USBH_WARN((USBH_MTYPE_HUB, "HUB:  HUB_NOTIFY UbdEpSubmitUrb on interrupt IN: st:%08x", urb->Header.Status));
        HubNotifyError(hub);
        return;
      }
    }
    UbdHcServicePorts(host_controller); // Service all ports
  }
} // ProcessHubNotification

/*********************************************************************
*
*       HubNotifyClearEndpointStallCompletion
*
*  Function description:
*    Hub clear pipe completion
*/
static void HubNotifyClearEndpointStallCompletion(URB *urb) {
  DEFAULT_EP * defaultEndpoint;
	USB_DEVICE * Device;
  USB_HUB    * hub;

	defaultEndpoint = (DEFAULT_EP *)urb->Header.InternalContext;
  Device          = defaultEndpoint->UsbDevice;
  T_ASSERT_MAGIC(Device, USB_DEVICE);
  hub             = Device->UsbHub;
  T_ASSERT_MAGIC(hub, USB_HUB);
  // Decrement the count
  defaultEndpoint->UrbCount--;
  USBH_LOG((USBH_MTYPE_HUB, "HubNotifyClearEndpointStallCompletion: urbcount: %u",defaultEndpoint->UrbCount));
  // Check the URB status
  if (urb->Header.Status) {
    USBH_WARN((USBH_MTYPE_HUB, "HubNotifyClearEndpointStallCompletion: st:%08x",urb->Header.Status));
    // On error try to restart the hub device
    DEC_REF(Device);// Clear local URB reference
    HubNotifyError(hub);
    return;
  } else {
    // On success do nothing and submit an new hub status request in the ProcessHubNotification()
    HubSetNotifyState(hub,HUB_NOTIFY_IDLE);
  }
  ProcessHubNotification(hub);
  DEC_REF(Device);
}

/*********************************************************************
*
*       HubAddAllPorts
*/
static USBH_STATUS HubAddAllPorts(USB_HUB* hub) {
  unsigned int   i;
  HUB_PORT     * HubPort;
  USB_DEVICE   * dev;
  T_ASSERT_MAGIC( hub, USB_HUB );
  USBH_LOG((USBH_MTYPE_HUB, "HubAddAllPorts Ports: %d!",hub->PortCount));
  dev = hub->HubDevice;
  if (0 == (dev->DevStatus & USB_STATUS_SELF_POWERED)
    &&  !dev->ParentPort->HighPowerFlag ) {
    USBH_WARN((USBH_MTYPE_HUB, "HubCreateAllPorts: self powered hub on an low powered parent port!"));
    return USBH_STATUS_INVALID_PARAM;
  };
  if ( 0==hub->PortCount ) {
    USBH_WARN((USBH_MTYPE_HUB, "HubCreateAllPorts: no ports!"));
    return USBH_STATUS_INVALID_PARAM;
  }
  if ( !DlistEmpty( &hub->PortList ) ) {
    USBH_WARN((USBH_MTYPE_HUB, "HubCreateAllPorts: PortList not empty!",                             hub->PortCount));
    return USBH_STATUS_INVALID_PARAM;
  }
  for (i=1;i<=hub->PortCount;i++) {
    // Initialize and create the hub ports
    HubPort = UbdNewHubPort();
		if (HubPort == NULL) {
		  USBH_WARN((USBH_MTYPE_HUB, "HubCreateAllPorts: UbdNewHubPort failed"));
			return USBH_STATUS_RESOURCES;
		}
    if ( dev->DevStatus & USB_STATUS_SELF_POWERED ) {
      HubPort->HighPowerFlag=TRUE;
    }
    HubPort->HubPortNumber = (unsigned char)i;
    HubPort->ExtHub        = hub;
    DlistInsertTail(&hub->PortList,&HubPort->ListEntry);
  }
  return USBH_STATUS_SUCCESS;
}

/*********************************************************************
*
*       HubStatusRequestCompletion
*
*  Function description:
*/
static void HubStatusRequestCompletion(URB * urb) {
  USB_HUB      * hub = (USB_HUB *)urb->Header.InternalContext;
  USB_ENDPOINT * usb_ep;
  USBH_STATUS    status;

  T_ASSERT_MAGIC(hub, USB_HUB);
  USBH_LOG((USBH_MTYPE_HUB, "HUB: [HubStatusRequestCompletion Ref.ct: %ld",hub->HubDevice->RefCount));
  usb_ep = hub->InterruptEp;
  usb_ep->UrbCount--;
  T_ASSERT(hub->NotifyState == HUB_NOTIFY_IDLE);                              // Status URB only in HUB_NOTIFY_IDLE state allowed
  if (USBH_STATUS_SUCCESS != urb->Header.Status) {                            // Check status
    USBH_WARN((USBH_MTYPE_HUB, "HUB:  HubStatusRequestCompletion: st:%08x",urb->Header.Status));
    if (hub->HubDevice->State == DEV_STATE_WORKING) {                         // Hub device ready
      status = UbdSubmitClearFeatureEndpointStall(&hub->HubDevice->DefaultEp, &hub->NotifyUrb,
                 hub->InterruptEp->EndpointDescriptor[USB_EP_DESC_ADDRESS_OFS], HubNotifyClearEndpointStallCompletion, NULL);
      if (status != USBH_STATUS_PENDING) {
        USBH_WARN((USBH_MTYPE_HUB, "HUB:  HubStatusRequestCompletion: SubmitClearFeatureEndpointStall status: 0x%lx!", status));
        hub->NotifyUrb.Header.Status = status;
        DEC_REF(hub->HubDevice);                                              // Clear the local reference
        HubNotifyError(hub);
        return;
      }
      goto exit;
    }
  } else {
    HubSetNotifyState(hub, HUB_NOTIFY_START);                                 // Successful URB status
  }
  ProcessHubNotification(hub);
  exit:
  DEC_REF(hub->HubDevice);                                                    // Clear the local reference
  USBH_LOG((USBH_MTYPE_HUB, "HUB: ]HubStatusRequestCompletion"));
} // HubStatusRequestCompletion

/*********************************************************************
*
*       HubInstallPeriodicStatusTransfer
*/
static USBH_STATUS HubInstallPeriodicStatusTransfer(USB_HUB* hub) {
  USB_DEVICE          * dev;
  USB_INTERFACE       * iface;
  USBH_INTERFACE_MASK   iMask;
  USBH_STATUS           status;
  USBH_EP_MASK          epMask;
  USB_ENDPOINT        * ep;
  URB                 * urb;

  USBH_LOG((USBH_MTYPE_HUB, "HubInstallPeriodicStatusTransfer !"));
  T_ASSERT_MAGIC(hub,            USB_HUB);
  T_ASSERT_MAGIC(hub->HubDevice, USB_DEVICE);
  dev = hub->HubDevice;
  // Get the first interface
  iMask.Mask      = USBH_INFO_MASK_INTERFACE | USBH_INFO_MASK_CLASS;
  iMask.Interface = USBHUB_DEFAULT_INTERFACE;
  iMask.Class     = USB_DEVICE_CLASS_HUB;

  status=UbdSearchUsbInterface(dev,&iMask,&iface);
  if (status) {
    // On error
    USBH_WARN((USBH_MTYPE_HUB, "HubInstallPeriodicStatusTransfer: UbdSearchUsbInterface!\n"));
    return status;
  }
  // Get an interrupt in endpoint
  epMask.Mask      = USBH_EP_MASK_TYPE | USBH_EP_MASK_DIRECTION;
  epMask.Direction = USB_IN_DIRECTION;
  epMask.Type      = USB_EP_TYPE_INT;
  ep               = UbdSearchUsbEndpointInInterface(iface,&epMask);
  if( NULL == ep ) { // On error
    USBH_WARN((USBH_MTYPE_HUB, "HubInstallPeriodicStatusTransfer: UbdGetEndpointDescriptorFromInterface!\n"));
    return USBH_STATUS_INVALID_PARAM;
  }
  hub->InterruptEp = ep;
  // Initialize the urb
  hub->InterruptTransferBufferSize = UbdGetUshortFromDesc(ep->EndpointDescriptor, USB_EP_DESC_PACKET_SIZE_OFS);
  // Allocate an URB buffer
  T_ASSERT(NULL == hub->InterruptTransferBuffer);
  hub->InterruptTransferBuffer = UrbBufferAllocateTransferBuffer(hub->InterruptTransferBufferSize);
//  hub->InterruptTransferBuffer = UbdAllocateTransferBuffer(hub->InterruptTransferBufferSize);
	if (hub->InterruptTransferBuffer == NULL) {
		USBH_WARN((USBH_MTYPE_HUB, "HubInstallPeriodicStatusTransfer:TAL_AllocatePhysicalMemory failed\n"));
		return USBH_STATUS_ERROR;
  }
  urb = &hub->interruptUrb;
  ZERO_MEMORY(urb, sizeof(URB));
  urb->Header.InternalCompletion     = HubStatusRequestCompletion;
  urb->Header.InternalContext        = hub;
  urb->Header.Function               = USBH_FUNCTION_INT_REQUEST;
  urb->Request.BulkIntRequest.Buffer = hub->InterruptTransferBuffer;
  urb->Request.BulkIntRequest.Length = hub->InterruptTransferBufferSize;
  // Initial reference for the hub notification= 2. This reference is decremented at the state HUB_NOTIFY_REOVED or HUB_NOTIFY_ERROR
  INC_REF(hub->HubDevice);
  status = UbdEpSubmitUrb(ep, urb);
  return status;
}

/*********************************************************************
*
*       HubPortResetSetIdleServicePorts
*
*  Function description:
*    Called if the device on the port is addressed and the device enumeration process
*    has been started. Also called from routines that are called if HubProcessPortResetSetAddress() stops.
*/
static void HubPortResetSetIdleServicePorts(USB_HUB * hub) {
  T_ASSERT_MAGIC(hub, USB_HUB);
  hub->PortResetEnumState = HUB_PORTRESET_IDLE;
  HC_CLEAR_ACTIVE_PORTRESET(hub->HubDevice->HostController, hub->EnumPort); // Allow starting an port reset on other port
  hub->EnumDevice         = NULL;
  hub->EnumPort           = NULL;
  UbdHcServicePorts(hub->HubDevice->HostController);                        // Service all ports
  DEC_REF(hub->HubDevice);                                                  // This hub port reset state machine is released here!
}

/*********************************************************************
*
*       HubPortResetError
*
*  Function description:
*/
static void HubPortResetError(USB_HUB * hub, USBH_STATUS status) {
  T_ASSERT_MAGIC(hub, USB_HUB);
  USBH_LOG((USBH_MTYPE_HUB, "HUB: HubPortResetError: %s hubs ref.ct: %ld", UbdHubPortResetStateStr(hub->PortResetEnumState), hub->HubDevice->RefCount));
  if (NULL != hub->EnumPort) {
    UbdSetPortState(hub->EnumPort, PORT_ERROR);
  }
  if (NULL != hub->EnumDevice) {
    DEC_REF(hub->EnumDevice);
  }
  UbdSetEnumErrorNotificationHubPortReset(hub->EnumPort, hub->PortResetEnumState, status); // Notify user
  HubPortResetSetIdleServicePorts(hub);
}

/*********************************************************************
*
*       HubPortResetDisablePort
*/
static void HubPortResetDisablePort(USB_HUB * hub) {
  USBH_STATUS status;
  USBH_LOG((USBH_MTYPE_HUB, "HubPortResetDisablePortAndStop: %s hubs ref.ct: %ld\n", UbdHubPortResetStateStr(hub->PortResetEnumState),hub->HubDevice->RefCount));
  if (hub->HubDevice->State < DEV_STATE_WORKING) {
    // On an request error set port state to HUB_ENUM_PORT_ERROR
    USBH_WARN((USBH_MTYPE_HUB, "HubPortResetDisablePortAndStop: hub device state is not working, stop\n"));
    HubPortResetError(hub,USBH_STATUS_PORT);
    return;
  }
  // Submit the disable port request
  UbdHubPrepareClrFeatureReq(&hub->EnumUrb,  HDC_SELECTOR_PORT_ENABLE,hub->EnumPort->HubPortNumber);
  // Next port reset state
  hub->PortResetEnumState = HUB_PORTRESET_DISABLE_PORT;
  status=UrbSubStateSubmitRequest(&hub->PortResetSubState, &hub->EnumUrb, DEFAULT_SETUP_TIMEOUT, hub->HubDevice);
  if (status != USBH_STATUS_PENDING) {
    // On request error stop state machine
    USBH_WARN((USBH_MTYPE_HUB, "HubDisablePortAndRestartDeviceReset: UrbSubStateSubmitRequest: HUB_PORTRESET_IS_ENABLED disable port: st: %08x\n", hub->EnumUrb.Header.Status));
    // On a request error set port state to HUB_ENUM_PORT_ERROR
    HubPortResetError(hub, status);
  }
}

/*********************************************************************
*
*       ProcessEnumHub
*
*  Function description:
*/
static void ProcessEnumHub(void * hubDev) {
  USB_DEVICE  * enumHubDev;
  USBH_STATUS   status;
  USB_HUB     * enumHub;
  URB         * urb;

  enumHub     = (USB_HUB *)hubDev;
  T_ASSERT_MAGIC(enumHub, USB_HUB);
  T_ASSERT_MAGIC(enumHub->HubDevice, USB_DEVICE);
  enumHubDev  = enumHub->HubDevice;
  urb         = &enumHubDev->EnumUrb;             // During device enumeration the URB from the device is used!
  USBH_LOG((USBH_MTYPE_HUB, "HUB: ProcessEnumHub %s Dev.ref.ct: %ld", UbdHubEnumStateStr(enumHub->EnumState),enumHubDev->RefCount));
  if (enumHubDev->HostController->State == HC_REMOVED) {
    enumHub->EnumState = HUB_ENUM_REMOVED;        // Valid for all child devices
  }
  if (NULL != enumHubDev->ParentPort->ExtHub) {
    USB_HUB * parentHub;                          // The parent port is an external hub
    parentHub = enumHubDev->ParentPort->ExtHub;
    if (parentHub->HubDevice->State < DEV_STATE_WORKING) {
      enumHub->EnumState = HUB_ENUM_REMOVED;      // Parent hub does not work
    }
  }
  switch (enumHub->EnumState) {
    case HUB_ENUM_START:
      if (!UbdCheckCtrlTransferBuffer(enumHubDev, HDC_MAX_HUB_DESCRIPTOR_LENGTH)) {
        USBH_WARN((USBH_MTYPE_HUB, "HUB: FATAL ProcessEnumHub: HUB_ENUM_START:UbdCheckCtrlTransferBuffer!"));
        return;
      }
      EnumPrepareGetDeviceStatus(enumHubDev, enumHubDev->CtrlTransferBuffer, HDC_MAX_HUB_DESCRIPTOR_LENGTH);
      enumHub->EnumState = HUB_ENUM_GET_STATUS;                                                                      // Set the new state
      status             = UrbSubStateSubmitRequest(&enumHub->EnumSubState, urb, DEFAULT_SETUP_TIMEOUT, enumHubDev); // Start the reequest
      if (status != USBH_STATUS_PENDING) {
        USBH_WARN((USBH_MTYPE_HUB, "HUB:  ProcessEnumHub: HUB_ENUM_START submit urb: st:%08x", urb->Header.Status));
        HubEnumParentPortError(enumHub, status);
      }
      break;
    case HUB_ENUM_GET_STATUS:
      if (urb->Header.Status != USBH_STATUS_SUCCESS) {                                                                   // On error
        USBH_WARN((USBH_MTYPE_HUB, "HUB:  ProcessEnumHub: get device status urb st:%08x", urb->Header.Status));
        HubEnumParentPortRestart(enumHub, USBH_STATUS_INVALID_DESCRIPTOR);
        return;
      }
      enumHubDev->DevStatus = USBH_LoadU16LE((U8 *)urb->Request.ControlRequest.Buffer);                                  // Copy the device status
      HubPrepareGetHubDesc(urb, enumHubDev->CtrlTransferBuffer, HDC_MAX_HUB_DESCRIPTOR_LENGTH);                          // Get the hub descriptor
      enumHub->EnumState    = HUB_ENUM_HUB_DESC;                                                                         // Set the new state
      status                = UrbSubStateSubmitRequest(&enumHub->EnumSubState, urb, DEFAULT_SETUP_TIMEOUT, enumHubDev);  // Start the reequest
      if (status != USBH_STATUS_PENDING) {
        USBH_WARN((USBH_MTYPE_HUB, "HUB:  ProcessEnumHub: Get enumHub descriptor st:%08x", urb->Header.Status));
        HubEnumParentPortError(enumHub, status);
      }
      break;
    case HUB_ENUM_HUB_DESC:
      if (urb->Header.Status != USBH_STATUS_SUCCESS) {                                                    // On error this can be also a timeout
        USBH_WARN((USBH_MTYPE_HUB, "HUB:  ProcessEnumHub: Get enumHub descriptor urb st:%08x", urb->Header.Status));
        HubEnumParentPortRestart(enumHub, urb->Header.Status);
        return;
      }
      if (ParseHubDescriptor(enumHub, urb->Request.ControlRequest.Buffer, urb->Request.ControlRequest.Length)) {
        USBH_WARN((USBH_MTYPE_HUB, "HUB:  ProcessEnumHub: ParseHubDescriptor failed"));                   // On error
        HubEnumParentPortRestart(enumHub, USBH_STATUS_INVALID_DESCRIPTOR);
        return;
      }
      status = HubAddAllPorts(enumHub);                                                                   // Add all ports to the enumHub
      if (status) {                                                                                       // On error
        USBH_WARN((USBH_MTYPE_HUB, "HUB:  ProcessEnumHub: HubAddAllPorts failed st:%08x", status));
        HubEnumParentPortError(enumHub, status);
        return;
      }
      enumHub->EnumState         = HUB_ENUM_SET_POWER;                                                    // Set port power for all ports
      enumHub->NotifyPortCt      = enumHub->PortCount;
      enumHubDev->EnumSubmitFlag = FALSE;
    case HUB_ENUM_SET_POWER:                                                                              // Fall through
      if (enumHubDev->EnumSubmitFlag) {                                                                   // Urb is completed
        enumHubDev->EnumSubmitFlag = FALSE;
        if (urb->Header.Status != USBH_STATUS_SUCCESS) {                                                  // On error
          USBH_WARN((USBH_MTYPE_HUB, "HUB:  ProcessEnumHub: Set port power urb st:%08x", urb->Header.Status));
          HubEnumParentPortRestart(enumHub, urb->Header.Status);
        } else {
          enumHub->NotifyPortCt--;
          UrbSubStateWait(&enumHub->EnumSubState, enumHub->PowerGoodTime, enumHubDev); // Wait for power good time, enter again to HUB_ENUM_SET_POWER
        }
        break;
      }
      if (enumHub->NotifyPortCt) {                                                         // Start next set power URB or release HUB_ENUM_SET_POWER
        HubPrepareSetFeatureReq(urb, HDC_SELECTOR_PORT_POWER, (U16)enumHub->NotifyPortCt); // Not all ports powered
        status = UrbSubStateSubmitRequest(&enumHub->EnumSubState, urb, DEFAULT_SETUP_TIMEOUT, enumHubDev);
        if (status != USBH_STATUS_PENDING) {
          USBH_WARN((USBH_MTYPE_HUB, "HUB:  ProcessEnumHub: UrbSubStateSubmitRequest: Set port power st:%08x", urb->Header.Status));
          HubEnumParentPortError(enumHub, status);
        } else {
          enumHubDev->EnumSubmitFlag = TRUE;
        }
        break;
      } else {
        if (!UbdCheckCtrlTransferBuffer(enumHubDev, HCD_GET_STATUS_LENGTH)) {              // All ports are powered, get port status
          USBH_WARN((USBH_MTYPE_HUB, "HUB: FATAL ProcessEnumHub: HUB_ENUM_SET_POWER: UbdCheckCtrlTransferBuffer!"));
          return;
        }
        enumHub->EnumState    = HUB_ENUM_PORT_STATE;
        enumHub->NotifyPortCt = enumHub->PortCount;                                        // Load number of ports
      }
    case HUB_ENUM_PORT_STATE: // Fall trough
      if (enumHubDev->EnumSubmitFlag) {
        enumHubDev->EnumSubmitFlag = FALSE;
        if (urb->Header.Status != USBH_STATUS_SUCCESS) { // On error
          USBH_WARN((USBH_MTYPE_HUB, "HUB:  ProcessEnumHub: Get port status urb st:%08x", urb->Header.Status));
          HubEnumParentPortRestart(enumHub, urb->Header.Status);
          break;
        } else {
          HUB_PORT * port;
          port = HubGetPortByNumber(enumHub, (U8)enumHub->NotifyPortCt); // On success save port status
          if (NULL == port) {
            USBH_WARN((USBH_MTYPE_HUB, "HUB:  HubGetPortByNumber: port number: %d", enumHub->NotifyPortCt));
            HubEnumParentPortRestart(enumHub, USBH_STATUS_INVALID_PARAM);
            break;
          }
          T_ASSERT(urb->Request.ControlRequest.Length >= 4);
          port->PortStatus       = USBH_LoadU32LE(enumHubDev->CtrlTransferBuffer);
          port->PortStatusShadow = port->PortStatus;
          if (port->PortStatus & PORT_STATUS_CONNECT) {
            UbdSetPortState(port, PORT_CONNECTED);                       // Device is connected, mark service required
          }
          enumHub->NotifyPortCt--;
        }
      }
      if (enumHub->NotifyPortCt) { // Not all ports processed
        HubPrepareGetPortStatus(urb, (U16)enumHub->NotifyPortCt, enumHubDev->CtrlTransferBuffer, (U16)enumHubDev->CtrlTransferBufferSize);
        status = UrbSubStateSubmitRequest(&enumHub->EnumSubState, urb, DEFAULT_SETUP_TIMEOUT, enumHubDev);
        if (status != USBH_STATUS_PENDING) {
          USBH_WARN((USBH_MTYPE_HUB, "HUB:  ProcessEnumHub: UrbSubStateSubmitRequest: Get port status st:%08x", urb->Header.Status));
          HubEnumParentPortError(enumHub, status);
        } else {
          enumHubDev->EnumSubmitFlag = TRUE;
        }
        break;
      } else {
        enumHub->EnumState = HUB_ENUM_ADD_DEVICE;         // All ports processed, new state
      }
    case HUB_ENUM_ADD_DEVICE:                             // Fall through
      enumHub->EnumState    = HUB_ENUM_IDLE;              // device enumeration now complete
      enumHubDev->EnumState = DEV_ENUM_IDLE;
      if (enumHub->PostEnumFunction != NULL) {
        enumHub->PostEnumFunction(enumHub->PostEnumContext);
      }
      status = HubInstallPeriodicStatusTransfer(enumHub); // Now the added hub interface is valid.
      if (status != USBH_STATUS_PENDING) {
        USBH_WARN((USBH_MTYPE_HUB, "HUB:  ProcessEnumHub: HubInstallPeriodicStatusTransfer st:%08x", urb->Header.Status));
        HubEnumParentPortError(enumHub, status);
      }
      enumHub->HubDevice->ParentPort->RetryCounter = 0;   // Reset the parent port retry counter
      break;
    case HUB_ENUM_REMOVED:
      DEC_REF(enumHubDev);                                // Delete the device
      break;
    case HUB_ENUM_IDLE:
      break;
    default:
      T_ASSERT0;
  }                                                       // Switch
}   // ProcessEnumHub

/*********************************************************************
*
*       HubDeviceResetRemove
*
*  Function description:
*/
static void HubDeviceResetRemove(USB_HUB * hub) {
  T_ASSERT_MAGIC(hub, USB_HUB);
  USBH_LOG((USBH_MTYPE_HUB, "HUB: HubDeviceResetRemove"));
  if (NULL != hub->EnumPort) { // Try a restart
    UbdSetPortState(hub->EnumPort, PORT_REMOVED);
  }
  if (NULL != hub->EnumDevice) {
    DEC_REF(hub->EnumDevice);  // Delete the device
  }
  UbdSetEnumErrorNotificationHubPortReset(hub->EnumPort, hub->PortResetEnumState, USBH_STATUS_ERROR); // Notify user
  HubPortResetSetIdleServicePorts(hub);
}

/*********************************************************************
*
*       HubPortResetRestart
*
*  Function description:
*/
static void HubPortResetRestart(USB_HUB * hub, USBH_STATUS status) {
  T_ASSERT_MAGIC(hub, USB_HUB);
  if (NULL != hub->EnumPort) {  // Try a restart
    UbdSetPortState(hub->EnumPort, PORT_RESTART);
  }
  if (NULL != hub->EnumDevice) {
    DEC_REF(hub->EnumDevice);   // Delete the device
  }
  UbdSetEnumErrorNotificationHubPortReset(hub->EnumPort, hub->PortResetEnumState, status); // Notify user
  HubPortResetSetIdleServicePorts(hub);
}

/*********************************************************************
*
*       HubProcessPortResetSetAddress
*
*  Function description:
*   An addressed USB device is enumerated on a hub port. If a parent device
*   is removed (this can be an root hub or hub) the state is always
*   HUB_PORTRESET_REMOVED. An already allocated device on this port is deleted.
*/
static void HubProcessPortResetSetAddress(void * usbHub) {
  HUB_PORT        * enum_port;
  USB_HUB         * hub;
  URB             * urb;
  USBH_STATUS       status;
  HOST_CONTROLLER * host;

  hub       = (USB_HUB *)usbHub;
  T_ASSERT_MAGIC(hub, USB_HUB);
  host      =  hub->HubDevice->HostController;
  T_ASSERT_MAGIC(host, HOST_CONTROLLER);
  enum_port =  hub->EnumPort;
  T_ASSERT_PTR(enum_port);
  urb       = &hub->EnumUrb;
  // 1. Check hub device state
  // 2. Check port state during reset
  if (hub->HubDevice->State < DEV_STATE_WORKING) {
    USBH_WARN((USBH_MTYPE_HUB, "HUB:  HubProcessPortResetSetAddress: hub device state is not working, stop"));
    hub->PortResetEnumState = HUB_PORTRESET_REMOVED;
  } else {
    if (hub->PortResetEnumState >= HUB_PORTRESET_START) {           // Check physically port status
      if (enum_port->PortStatus & PORT_STATUS_CONNECT) {
        if (hub->PortResetEnumState >= HUB_PORTRESET_SET_ADDRESS) {
          if (!(enum_port->PortStatus &PORT_STATUS_ENABLED)) {
            UbdSetPortState(enum_port, PORT_RESTART);               // Port disabled after state port reset
            // Tricky: No port enumeration notification because HubPortResetDisablePort calls HubProcessPortResetSetAddress
            //         again with new enum state HUB_PORTRESET_DISABLE_PORT the port state is not changed!
            HubPortResetDisablePort(hub);
            return;
          }
        }
      } else {
        // Port is not connected, stop port enumeration
        USBH_WARN((USBH_MTYPE_HUB, "HUB: HubProcessPortResetSetAddress: port state not connect during port reset!"));
        HubPortResetError(hub, USBH_STATUS_PORT);
        return;
      }
    }
  }
  USBH_LOG((USBH_MTYPE_HUB, "HUB: HubProcessPortResetSetAddress: %s hubs ref.ct: %ld", UbdHubPortResetStateStr(hub->PortResetEnumState),
                                                                                       hub->HubDevice->RefCount));
  switch (hub->PortResetEnumState) {
    case HUB_PORTRESET_START:
      hub->PortResetEnumState = HUB_PORTRESET_WAIT_RESTART;                  // Wait after connect
      UrbSubStateWait(&hub->PortResetSubState, WAIT_AFTER_CONNECT, NULL);
      break;
    case HUB_PORTRESET_RESTART:
      hub->PortResetEnumState = HUB_PORTRESET_WAIT_RESTART;
      UrbSubStateWait(&hub->PortResetSubState, DELAY_FOR_REENUM, NULL);
      break;
    case HUB_PORTRESET_WAIT_RESTART:
      UbdSetPortState(enum_port, PORT_RESET);                                // Reset the port
      HubPrepareSetFeatureReq(urb, HDC_SELECTOR_PORT_RESET, enum_port->HubPortNumber);
      enum_port->PortStatusShadow = 0;
      hub->PortResetEnumState     = HUB_PORTRESET_RES;                       // Next state
      status                      = UrbSubStateSubmitRequest(&hub->PortResetSubState, urb, DEFAULT_SETUP_TIMEOUT, NULL);
      if (status != USBH_STATUS_PENDING) {
        USBH_WARN((USBH_MTYPE_HUB, "HUB:  Ext.Hub Port Reset:: UrbSubStateSubmitRequest: HUB_PORTRESET_WAIT_RESTART Set port reset:%08x", urb->Header.Status));
        HubPortResetError(hub, status);                                      // Error submitting, disable the port is not available
      }
      break;
    case HUB_PORTRESET_RES:
      if (urb->Header.Status != USBH_STATUS_SUCCESS) {                       // Check urb
        USBH_WARN((USBH_MTYPE_HUB, "HUB:  Ext.Hub Port Reset:: HUB_PORTRESET_RES urb st:%08x", urb->Header.Status)); // Hub request error
        HubPortResetRestart(hub, urb->Header.Status);                        // Restart port
        break;
      }
      hub->PortResetEnumState = HUB_PORTRESET_IS_ENABLED;
      UrbSubStateWait(&hub->PortResetSubState, DEFAULT_RESET_TIMEOUT, NULL); // Wait afer reset the port
      break;
    case HUB_PORTRESET_IS_ENABLED:
      if ((enum_port->PortStatusShadow &PORT_STATUS_ENABLED) && (enum_port->PortStatusShadow &PORT_STATUS_CONNECT)) {
        UbdSetPortState(enum_port, PORT_ENABLED);                            // Port is connected and enabled
        enum_port->PortSpeed = USBH_FULL_SPEED;
        if (enum_port->PortStatus & PORT_STATUS_LOW_SPEED) {
          enum_port->PortSpeed = USBH_LOW_SPEED;
        }
        if (enum_port->PortStatus & PORT_STATUS_HIGH_SPEED) {
          enum_port->PortSpeed = USBH_HIGH_SPEED;
        }
        hub->PortResetEnumState = HUB_PORTRESET_WAIT_RESET;                 // New state
      } else {
        USBH_WARN((USBH_MTYPE_HUB, "HUB: Ext.Hub Port Reset:: device disappears during reset"));
        UbdSetPortState(enum_port, PORT_RESTART);
        HubPortResetDisablePort(hub);                                       // Tricky: HubProcessPortResetSetAddress is called again with HUB_PORTRESET_DISABLE_PORT
        break;
      }
    case HUB_PORTRESET_WAIT_RESET: // Fall trough
      hub->PortResetEnumState = HUB_PORTRESET_SET_ADDRESS;                  // New state
      UrbSubStateWait(&hub->PortResetSubState, WAIT_AFTER_RESET, NULL);     // Wait afer reset the port
      break;
    case HUB_PORTRESET_SET_ADDRESS: {
      USB_DEVICE * enum_device;
      enum_device = UbdNewUsbDevice(host);
      if (enum_device == NULL) {                                            // Set enum device pointer
        USBH_WARN((USBH_MTYPE_HUB, "HUB: Ext.Hub Port Reset:: UbdNewUsbDevice fails -> port error!"));
        UbdSetPortState(enum_port, PORT_ERROR);
        HubPortResetDisablePort(hub);                                       // Tricky: HubProcessPortResetSetAddress is called again with HUB_PORTRESET_DISABLE_PORT
      } else {
        hub->EnumDevice                 = enum_device;
        enum_device->DeviceSpeed        = enum_port->PortSpeed;
        enum_device->UsbAddress         = UbdGetUsbAddress(host);
        enum_device->ParentPort         = enum_port;
        enum_device->ConfigurationIndex = enum_port->ConfigurationIndex;
        HubPrepareStandardOutRequest(urb, USB_REQ_SET_ADDRESS, enum_device->UsbAddress, 0); // prepare the set address request
        switch (enum_port->PortSpeed) {                                     // Select one of the preallocated endpoints
          case USBH_LOW_SPEED:
            hub->PortResetEp0Handle = host->LowSpeedEndpoint;
            break;
          case USBH_FULL_SPEED:
            hub->PortResetEp0Handle = host->FullSpeedEndpoint;
            break;
          case USBH_HIGH_SPEED:
            hub->PortResetEp0Handle = host->HighSpeedEndpoint;
            break;

          default:
            T_ASSERT0;
        }
        hub->PortResetEnumState = HUB_PORTRESET_WAIT_SET_ADDRESS;           // new state
        status                  = UrbSubStateSubmitRequest(&hub->PortResetControlUrbSubState, urb, DEFAULT_SETUP_TIMEOUT, NULL);
        if (status != USBH_STATUS_PENDING) {
          USBH_WARN((USBH_MTYPE_HUB, "HUB: FATAL Ext.Hub Port Reset:: UrbSubStateSubmitRequest: failed: st: %08x", urb->Header.Status));
          UbdSetPortState(enum_port, PORT_ERROR);
          HubPortResetDisablePort(hub); // Tricky: HubProcessPortResetSetAddress is called again with HUB_PORTRESET_DISABLE_PORT
        }
      }
    }
      break;
    case HUB_PORTRESET_WAIT_SET_ADDRESS:
      T_ASSERT_MAGIC(hub->EnumDevice, USB_DEVICE);
      if (urb->Header.Status != USBH_STATUS_SUCCESS) {
        USBH_WARN((USBH_MTYPE_HUB, "HUB:  Ext.Hub Port Reset:: HUB_PORTRESET_WAIT_SET_ADDRESS urb st:%08x", urb->Header.Status));
        UbdSetPortState(enum_port, PORT_RESTART);
        HubPortResetDisablePort(hub); // Tricky: HubProcessPortResetSetAddress is called again with HUB_PORTRESET_DISABLE_PORT
        break;
      }
      hub->PortResetEnumState = HUB_PORTRESET_START_DEVICE_ENUM;             // New state
      UrbSubStateWait(&hub->PortResetSubState, WAIT_AFTER_SETADDRESS, NULL);
      break;
    case HUB_PORTRESET_START_DEVICE_ENUM: {
      USB_DEVICE * dev;
      T_ASSERT_MAGIC(hub->EnumDevice, USB_DEVICE);
      dev                           = hub->EnumDevice;          // Save and release enum device pointer
      hub->EnumDevice               = NULL;
      enum_port->ConfigurationIndex = 0;
      // 1.The Port device pointer pints to the connected device
      // 2.Start the device enumeration process
      // 3.Release the port enumeration and wait for connecting other ports!
      //   at this point the port reset enumeration state is PORT_ENABLED!
      UbdStartEnumeration(dev, UbdCreateInterfaces, dev);
      HubPortResetSetIdleServicePorts(hub);
    }
      break;
    case HUB_PORTRESET_DISABLE_PORT:  // The port is disabled restart or stop port reset enumeration

#if (USBH_DEBUG > 1)
      if (urb->Header.Status != USBH_STATUS_SUCCESS) {
        USBH_WARN((USBH_MTYPE_HUB, "HUB:  Ext.Hub Port Reset:: HUB_PORTRESET_DISABLE_PORT urb st:%08x", urb->Header.Status));
      }
#endif

      if (enum_port->PortState == PORT_RESTART) {
        HubPortResetRestart(hub, USBH_STATUS_PORT);
      } else {
        HubPortResetError(hub, USBH_STATUS_PORT);
      }
      break;
    case HUB_PORTRESET_REMOVED:
      HubDeviceResetRemove(hub);
      break;
    default:
      T_ASSERT0;
  }
}

/*********************************************************************
*
*       ServiceHubPorts
*
*  Function description:
*/
static T_BOOL ServiceHubPorts(USB_HUB * hub) {
  HUB_PORT * hub_port;
  DLIST    * e;
  T_ASSERT_MAGIC(hub, USB_HUB);
  if (hub->PortResetEnumState != HUB_PORTRESET_IDLE || hub->HubDevice->State < DEV_STATE_WORKING) {
    return FALSE;                                          // Enumeration is active or device is not in working state, do nothing now, we are called later
  }
  e = DlistGetNext(&hub->PortList);                        // Run over all ports, to see if a port needs a reset
  while (e != &hub->PortList) {
    hub_port = HUB_PORT_PTR(e);
    T_ASSERT_MAGIC(hub_port, HUB_PORT);
    e        = DlistGetNext(e);
    if (hub_port->PortState == PORT_RESTART || hub_port->PortState == PORT_CONNECTED) {
      hub_port->RetryCounter++;
      if (hub_port->RetryCounter > RESET_RETRY_COUNTER) {
        USBH_WARN((USBH_MTYPE_HUB, "HUB: ServiceHubPorts: max. port retries -> PORT_ERROR!"));
        UbdSetPortState(hub_port, PORT_ERROR);
        UbdSetEnumErrorNotificationHubPortReset(hub_port, hub->PortResetEnumState, USBH_STATUS_ERROR);
      } else {
        if (hub_port->PortState == PORT_RESTART) {         // On success and not active root hub enumeration state machine
          hub->PortResetEnumState = HUB_PORTRESET_RESTART; // Schedule a delayed restart
        } else {
          hub->PortResetEnumState = HUB_PORTRESET_START;
        }
        INC_REF(hub->HubDevice);                           // Ref.Ct=2
        hub->EnumPort = hub_port;
        HC_SET_ACTIVE_PORTRESET(hub->HubDevice->HostController, hub_port);
        HubProcessPortResetSetAddress(hub);
        return TRUE;
      }
    }
  }
  return FALSE;
}

/*********************************************************************
*
*       InitHub
*
*  Function description:
*    Initializes a hub object. The link pointer in the hub and the device object are set!
*  Parameters:
*    pHub: Pointer to an uninitialized hub object
*/
static USBH_STATUS InitHub(USB_HUB * pHub, USB_DEVICE * dev) {
  USBH_STATUS status;
  USBH_LOG((USBH_MTYPE_HUB, "HUB: InitHub!"));
  T_ASSERT_PTR(pHub);
  T_ASSERT_PTR(dev);
  ZERO_MEMORY(pHub, sizeof(USB_HUB));

#if (USBH_DEBUG > 1)
  pHub->Magic = USB_HUB_MAGIC;
#endif

  DlistInit(&pHub->PortList);
  pHub->HubDevice = dev;
  status          = UrbSubStateInit(&pHub->EnumSubState, dev->HostController, &dev->DefaultEp.EpHandle, ProcessEnumHub, pHub);
  if (status) {
    USBH_WARN((USBH_MTYPE_HUB, "HUB:  InitHub: UrbSubStateInit pHub->EnumSubState !"));
    goto Exit;
  }
  status          = UrbSubStateInit(&pHub->NotifySubState, dev->HostController, &dev->DefaultEp.EpHandle, ProcessHubNotification, pHub);
  if (status) {
    USBH_WARN((USBH_MTYPE_HUB, "HUB:  InitHub: UrbSubStateInit pHub->NotifySubState!"));
    goto Exit;
  }
  status          = UrbSubStateInit(&pHub->PortResetSubState, dev->HostController, &dev->DefaultEp.EpHandle, HubProcessPortResetSetAddress, pHub);
  if (status) {
    USBH_WARN((USBH_MTYPE_HUB, "HUB:  InitHub: UrbSubStateInit PortResetSubState!"));
    goto Exit;
  }
  status          = UrbSubStateInit(&pHub->PortResetControlUrbSubState, dev->HostController, &pHub->PortResetEp0Handle, HubProcessPortResetSetAddress, pHub);
  if (status) {
    USBH_WARN((USBH_MTYPE_HUB, "HUB:  InitHub: UrbSubStateInit PortResetEnumState!"));
    goto Exit;
  }
Exit:
  return status;
}

/*********************************************************************
*
*       UbdAllocInitUsbHub
*
*  Function description:
*    Hub device allocation and initialization.
*    The link pointer in the hub and the device object are set!
*/
USB_HUB * UbdAllocInitUsbHub(USB_DEVICE * dev) {
  USB_HUB * hub;

  T_ASSERT_MAGIC(dev, USB_DEVICE);
  USBH_LOG((USBH_MTYPE_HUB, "UbdAllocInitUsbHub!"));
  hub = USBH_Malloc(sizeof(USB_HUB));
  if (NULL == hub) {
    USBH_WARN((USBH_MTYPE_HUB, "HUB:  UbdAllocInitUsbHub: USBH_malloc!"));
    goto exit;
  }
  if (USBH_STATUS_SUCCESS != InitHub(hub, dev)) {
    USBH_WARN((USBH_MTYPE_HUB, "HUB:  UbdAllocInitUsbHub: InitHub!"));
    USBH_Free(hub);
    hub = NULL;
  }
  exit:
  return hub;
}

/*********************************************************************
*
*       HubRemoveAllPorts
*
*  Function description:
*/
static void HubRemoveAllPorts(USB_HUB * hub) {
  PDLIST     entry;
  HUB_PORT * hub_port;
  USBH_LOG((USBH_MTYPE_HUB, "HUB: HubRemoveAllPorts!"));
  while (!DlistEmpty(&hub->PortList)) {
    DlistRemoveHead(&hub->PortList, &entry);
    hub_port = HUB_PORT_PTR(entry);
    UbdDeleteHubPort(hub_port);
  }
}

/*********************************************************************
*
*       UbdDeleteHub
*
*  Function description:
*/
void UbdDeleteHub(USB_HUB * hub) {
  USBH_LOG((USBH_MTYPE_HUB, "UbdDeleteHub!"));
  T_ASSERT_PTR(hub);
  if (hub->InterruptTransferBuffer != NULL) {
    UrbBufferFreeTransferBuffer(hub->InterruptTransferBuffer);
    hub->InterruptTransferBuffer = NULL;
  }
  HubRemoveAllPorts(hub);
  // Releases resources of UrbSubStateInit
  UrbSubStateExit(&hub->EnumSubState);
  UrbSubStateExit(&hub->NotifySubState);
  UrbSubStateExit(&hub->PortResetSubState);
  UrbSubStateExit(&hub->PortResetControlUrbSubState);

#if (USBH_DEBUG > 1)
  hub->Magic = 0;
#endif

  USBH_Free(hub);
}

/*********************************************************************
*
*       HubPrepareClearFeatureEndpointStall
*
*  Function description:
*/
void HubPrepareClearFeatureEndpointStall(URB * Urb, U8 Endpoint);
void HubPrepareClearFeatureEndpointStall(URB * Urb, U8 Endpoint) {
  ZERO_MEMORY(Urb, sizeof(URB));
  Urb->Header.Function                      = USBH_FUNCTION_CONTROL_REQUEST; // Set clear feature endpoint stall request
  Urb->Request.ControlRequest.Setup.Type    = USB_ENDPOINT_RECIPIENT;       // STD, OUT, endpoint
  Urb->Request.ControlRequest.Setup.Request = USB_REQ_CLEAR_FEATURE;
  Urb->Request.ControlRequest.Setup.Value   = USB_FEATURE_STALL;
  Urb->Request.ControlRequest.Setup.Index   = (U16)Endpoint;
}

/*********************************************************************
*
*       UbdStartHub
*
*  Function description:
*/
void UbdStartHub(USB_HUB * hub, POST_ENUM_FUNCTION * postEnumFunction, void * context) {
  USBH_LOG((USBH_MTYPE_HUB, "HUB: UbdStartHub!"));
  T_ASSERT_MAGIC(hub, USB_HUB);
  T_ASSERT(HUB_ENUM_IDLE     == hub->EnumState);
  T_ASSERT(DEV_ENUM_INIT_HUB == hub->HubDevice->EnumState);
  DlistInit(&hub->PortList);
  hub->PortCount        = 0;
  hub->PostEnumFunction = postEnumFunction;
  hub->PostEnumContext  = context;
  hub->EnumState        = HUB_ENUM_START;
  ProcessEnumHub(hub);
}

/*********************************************************************
*
*       UbdHubBuildChildDeviceList
*
*  Function description:
*    Builds an device list of all devices that are connected to a parent
*    device inclusive the parent device. The first device in the list
*    is the parent device. The list ends if no hub device on a port is found!
*
*  Return value:
*    Number of devices in the list inclusive the rootHubDevice!
*    0: rootHubDevice is no hub device!
*/
int UbdHubBuildChildDeviceList(USB_DEVICE * hubDevice, DLIST * devList) {
  int          ct;
  USB_DEVICE * dev;
  PDLIST       dev_entry;
  PDLIST       port_entry;
  HUB_PORT   * port;
  T_BOOL       succ;
  T_BOOL       devListChangeFlag;
  USB_HUB    * hub;
  dev = hubDevice;
  T_ASSERT_MAGIC(dev, USB_DEVICE);
  USBH_LOG((USBH_MTYPE_HUB, "UbdHubBuildChildDeviceList!"));
  if (NULL == dev->UsbHub) {
    USBH_WARN((USBH_MTYPE_HUB, "HUB:  UbdHubBuildChildDeviceList: param. hubDevice is not an hub device!"));
    return 0;
  }
  dev->TempFlag = FALSE; // Add the root device to the list
  ct            = 1;
  DlistInsertTail(devList, &dev->TempEntry);
  for (; ;) {
    // Search until an hub device is found where the tempFlag is FALSE end insert all child devices of this hub,
    // then break and search again until no hub device with tempFlag=FALSE is in the list
    succ              = TRUE;
    devListChangeFlag = FALSE;
    dev_entry         = DlistGetNext(devList);
    while (dev_entry != devList) {
      dev = GET_USB_DEVICE_FROM_TEMP_ENTRY(dev_entry);
      T_ASSERT_MAGIC(dev, USB_DEVICE);
      if (NULL != dev->UsbHub) {
        hub = dev->UsbHub;                           // Device is an hub
        T_ASSERT_MAGIC(hub, USB_HUB);
        if (!dev->TempFlag) {
          dev->TempFlag = TRUE;
          // No processed hub device found!
          port_entry    = DlistGetNext(&hub->PortList); // Add all devices of all hub ports
          while (port_entry != &hub->PortList) {
            port       = HUB_PORT_PTR(port_entry);
            T_ASSERT_MAGIC(port, HUB_PORT);
            port_entry = DlistGetNext(port_entry);
            if (port->Device != NULL) {
              if (NULL != port->Device->UsbHub) {    // A device is connected to this port
                port->Device->TempFlag = FALSE;      // USB hub device must be checked
                succ                   = FALSE;
              } else {
                port->Device->TempFlag = TRUE;
              }
              ct++;
              DlistInsertTail(devList, &port->Device->TempEntry);
              devListChangeFlag = TRUE;
            }
          }
        }
      }
      if (devListChangeFlag) {                       // Start again if the devList is chnaged
        break;
      }
      dev_entry = DlistGetNext(dev_entry);
    }
    if (succ) {
      break;
    }
  }
  return ct;
}

/*********************************************************************
*
*       UbdServiceAllHubs
*
*  Function description:
*/
void UbdServiceAllHubs(HOST_CONTROLLER * hc) {
  USB_DEVICE * dev;
  PDLIST       dev_entry;
  T_ASSERT_MAGIC(hc, HOST_CONTROLLER);
  dev_entry  = DlistGetNext(&hc->DeviceList);
  while (dev_entry != &hc->DeviceList) {
    dev = GET_USB_DEVICE_FROM_ENTRY(dev_entry);
    T_ASSERT_MAGIC(dev, USB_DEVICE);
    dev_entry = DlistGetNext(dev_entry);
    if (NULL != dev->UsbHub && dev->State == DEV_STATE_WORKING) {
      if (ServiceHubPorts(dev->UsbHub)) { // Device is an hub device
        break;                            // If HubProcessDeviceReset() is called from ServiceHubPorts
      }
    }
  }
}
#endif

/*********************************************************************
*
*       UbdNewHubPort
*
*  Function description:
*
*  Return value:
*    Return null on error
*/
HUB_PORT * UbdNewHubPort() {
  HUB_PORT * port;
  USBH_LOG((USBH_MTYPE_HUB, "HUB: UbdNewHubPort!"));
  port = USBH_Malloc(sizeof(HUB_PORT));
  if (port == NULL) {
    USBH_WARN((USBH_MTYPE_HUB, "HUB: UbdNewHubPort: USBH_malloc"));
    return NULL;
  }
  ZERO_MEMORY(port,sizeof(HUB_PORT));

#if (USBH_DEBUG > 1)
  port->Magic = HUB_PORT_MAGIC;
#endif

  UbdSetPortState(port, PORT_REMOVED); // Assume the port is removed
  return port;
}

/*********************************************************************
*
*       UbdDeleteHubPort
*
*  Function description:
*/
void UbdDeleteHubPort(HUB_PORT * HubPort) {
  USBH_Free(HubPort);
}

/*********************************************************************
*
*       UbdSetPortState
*
*  Function description:
*/
void UbdSetPortState(HUB_PORT * hubPort, PORT_STATE state) {
  T_ASSERT_MAGIC(hubPort, HUB_PORT);
  USBH_LOG((USBH_MTYPE_HUB, "HUB: SET_PORT_STATE: ext.hub.ptr: %p portnb: %d old: %s new: %s", hubPort->ExtHub,(int)hubPort->HubPortNumber,
                            UbdPortStateStr(hubPort->PortState), UbdPortStateStr(state)));
  hubPort->PortState = state;
}

/*********************************************************************
*
*       HubGetPortByNumber
*
*  Function description:
*/
HUB_PORT * HubGetPortByNumber(USB_HUB * Hub, U8 Number) {
  HUB_PORT * HubPort;
  DLIST    * e;
  T_ASSERT_MAGIC(Hub, USB_HUB);
  e = DlistGetNext(&Hub->PortList);
  while (e != &Hub->PortList) {
    HubPort = HUB_PORT_PTR(e);
    T_ASSERT_MAGIC(HubPort, HUB_PORT);
    e = DlistGetNext(e);
    if (HubPort->HubPortNumber == Number) {
      return HubPort;
    }
  }
  return NULL;
}

/********************************* EOF ******************************/
