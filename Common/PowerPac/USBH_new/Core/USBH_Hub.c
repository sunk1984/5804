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
#include "usbh.h"


#define HC_CLEAR_ACTIVE_PORTRESET(hostPtr, enumPortPtr) {                                          \
  USBH_ASSERT_MAGIC((hostPtr), USBH_HOST_CONTROLLER);                                                      \
  if (NULL == (hostPtr)->pActivePortReset) {                                                        \
    USBH_WARN((USBH_MTYPE_HUB, "HcClearActivePortReset: pActivePortReset already NULL!\n"));        \
  } else {                                                                                         \
    USBH_ASSERT_MAGIC( (enumPortPtr), USBH_HUB_PORT);                                                      \
    if ( (hostPtr)->pActivePortReset != (enumPortPtr) ) {                                           \
      USBH_WARN((USBH_MTYPE_HUB, "HcClearActivePortReset: not the same port as at the start!\n")); \
    }                                                                                              \
    (hostPtr)->pActivePortReset=NULL;                                                               \
  }                                                                                                \
}

#define HC_SET_ACTIVE_PORTRESET(hostPtr, enumPortPtr) {                                  \
  USBH_ASSERT_MAGIC((hostPtr), USBH_HOST_CONTROLLER);                                            \
  if (NULL != (hostPtr)->pActivePortReset) {                                              \
    USBH_WARN((USBH_MTYPE_HUB, "HcSetActivePortReset: pActivePortReset is not NULL!\n")); \
  } else {                                                                               \
    USBH_ASSERT_MAGIC((enumPortPtr), USBH_HUB_PORT);                                             \
    (hostPtr)->pActivePortReset=(enumPortPtr);                                            \
  }                                                                                      \
}

/*********************************************************************
*
*       _HubSetNotifyState
*/
static void _HubSetNotifyState(USB_HUB * hub, USBH_HUB_NOTIFY_STATE state) {
  USBH_ASSERT_MAGIC(hub, USB_HUB);
  USBH_LOG((USBH_MTYPE_HUB, "HUB_NOTIFY: old: %s new: %s", USBH_HubNotificationState2Str(hub->NotifyState), USBH_HubNotificationState2Str(state)));
  hub->OldNotifyState = hub->NotifyState;
  hub->NotifyState    = state;
}

/*********************************************************************
*
*       _HubPrepareGetPortStatus
*/
static void _HubPrepareGetPortStatus(USBH_URB * urb, U16 selector, void * buffer, U16 bufferLength) {
  USBH_ASSERT(HCD_GET_STATUS_LENGTH <= bufferLength);
  USBH_LOG((USBH_MTYPE_HUB, "_HubPrepareGetPortStatus: port: %d!",(int)selector));
  USBH_ZERO_MEMORY(urb, sizeof(USBH_URB));

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
  urb->Request.ControlRequest.pBuffer       = buffer;
  urb->Request.ControlRequest.Length        = HCD_GET_STATUS_LENGTH;
}

/*********************************************************************
*
*       _HubPrepareStandardOutRequest
*/
static void _HubPrepareStandardOutRequest(USBH_URB * urb,U8 request, U16 value, U16 index) {
  USBH_LOG((USBH_MTYPE_HUB, "_HubPrepareStandardOutRequest: requst: %d!",(int)request));
  USBH_ZERO_MEMORY(urb,sizeof(USBH_URB));
  urb->Header.Function                      = USBH_FUNCTION_CONTROL_REQUEST;
  urb->Request.ControlRequest.Setup.Type    = 0x00; // STD, OUT, device
  urb->Request.ControlRequest.Setup.Request = request;
  urb->Request.ControlRequest.Setup.Value   = value;
  urb->Request.ControlRequest.Setup.Index   = index;
}

/*********************************************************************
*
*       _HubPrepareGetHubDesc
*/
static void _HubPrepareGetHubDesc(USBH_URB * urb, void * buffer, U16 reqLength) {
  U16 length;

  USBH_LOG((USBH_MTYPE_HUB, "HubPrepareGetDescClassReq: length:%d!",(int)reqLength));
  length = USBH_MIN(reqLength, HDC_MAX_HUB_DESCRIPTOR_LENGTH);
  USBH_ZERO_MEMORY(urb,sizeof(USBH_URB));
  urb->Header.Function                      = USBH_FUNCTION_CONTROL_REQUEST;
  urb->Request.ControlRequest.Setup.Type    = USB_TO_HOST | USB_REQTYPE_CLASS; // class request, IN, device
  urb->Request.ControlRequest.Setup.Request = USB_REQ_GET_DESCRIPTOR;
  urb->Request.ControlRequest.Setup.Value   = (U16)(USB_HUB_DESCRIPTOR_TYPE << 8);
  urb->Request.ControlRequest.Setup.Length  = length;
  urb->Request.ControlRequest.pBuffer       = buffer;
  urb->Request.ControlRequest.Length        = length;
}

/*********************************************************************
*
*       _HubPrepareSetFeatureReq
*/
static void _HubPrepareSetFeatureReq(USBH_URB * urb, U16 featureSelector, U16 selector) {
 	USBH_LOG((USBH_MTYPE_HUB, "_HubPrepareSetFeatureReq: featureSelector: %d seletor: %d!", (int)featureSelector, (int)selector));
  USBH_ZERO_MEMORY(urb, sizeof(USBH_URB));
  urb->Header.Function                      = USBH_FUNCTION_CONTROL_REQUEST;
  urb->Request.ControlRequest.Setup.Type    = USB_REQTYPE_CLASS | USB_OTHER_RECIPIENT; // class request, IN, device
  urb->Request.ControlRequest.Setup.Request = USB_REQ_SET_FEATURE;
  urb->Request.ControlRequest.Setup.Value   = featureSelector;
  urb->Request.ControlRequest.Setup.Index   = selector;
  urb->Request.ControlRequest.Setup.Length  = 0;
}

/*********************************************************************
*
*       USBH_BD_HubPrepareClrFeatureReq
*/
void USBH_BD_HubPrepareClrFeatureReq(USBH_URB * urb, U16 feature, U16 selector) {
  USBH_LOG((USBH_MTYPE_HUB, "USBH_BD_HubPrepareClrFeatureReq: feature: %d port: %d!", (int)feature, (int)selector));
  USBH_ZERO_MEMORY(urb, sizeof(USBH_URB));
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
*       _ParseHubDescriptor
*/
static USBH_STATUS _ParseHubDescriptor(USB_HUB * hub, U8 * buffer, U32 length) {
  USBH_ASSERT_MAGIC(hub, USB_HUB);
  USBH_ASSERT_PTR(buffer);

  if (length < HDC_DESC_MIN_LENGTH) {
    USBH_LOG((USBH_MTYPE_HUB, " _ParseHubDescriptor: desc.-length: %lu ", length));
    return USBH_STATUS_INVALID_DESCRIPTOR;
  }
  hub->PortCount       = USBH_GetUcharFromDesc( buffer,HDC_DESC_PORT_NUMBER_OFS);
  hub->Characteristics = USBH_GetUshortFromDesc(buffer,HDC_DESC_CHARACTERISTICS_LOW_OFS);
  hub->PowerGoodTime   = USBH_GetUcharFromDesc(buffer,HDC_DESC_POWER_GOOD_TIME_OFS);
  hub->PowerGoodTime   = hub->PowerGoodTime << 1;
  USBH_LOG((USBH_MTYPE_HUB, "_ParseHubDescriptor: Ports: %d Character.: 0x%x powergoodtime: %d!", hub->PortCount, hub->Characteristics,hub->PowerGoodTime));
  return USBH_STATUS_SUCCESS;
}

/*********************************************************************
*
*       _HubNotifyRestart
*/
static void _HubNotifyRestart(USB_HUB* hub) {
  USBH_HUB_PORT * parentPort;
  USBH_ASSERT_MAGIC(hub,            USB_HUB);
  USBH_ASSERT_MAGIC(hub->pHubDevice, USB_DEVICE);
  _HubSetNotifyState(hub, USBH_HUB_NOTIFY_ERROR);
  parentPort = hub->pHubDevice->pParentPort;
  if (parentPort != NULL) {
    USBH_ASSERT_MAGIC(parentPort, USBH_HUB_PORT);
    USBH_WARN((USBH_MTYPE_HUB, "_HubNotifyRestart: pHub ref.ct %ld portnumber: %d portstate: %s", hub->pHubDevice->RefCount,
              (int)parentPort->HubPortNumber, USBH_PortState2Str(parentPort->PortState)));
    USBH_BD_SetPortState(parentPort, PORT_RESTART);
  }
  USBH_MarkParentAndChildDevicesAsRemoved(hub->pHubDevice);
  DEC_REF(hub->pHubDevice); // Delete notify State machine reference
  // Service all ports
  USBH_HC_ServicePorts(hub->pHubDevice->pHostController);
}

/*********************************************************************
*
*       _HubNotifyError
*
*  Function description
*  Called on fatal errors (errors returned from URB submit routines) in the pHub
*  notification routine. The parent port is set to State PORT_ERROR. The pHub device
*  and all connected child devices are deleted. At last the local reference is deleted.
*/
static void _HubNotifyError(USB_HUB * hub) {
  USBH_HUB_PORT * parentPort;
  USBH_ASSERT_MAGIC(hub,            USB_HUB);
  USBH_ASSERT_MAGIC(hub->pHubDevice, USB_DEVICE);
  _HubSetNotifyState(hub,USBH_HUB_NOTIFY_ERROR);
  parentPort=hub->pHubDevice->pParentPort;
  if (parentPort != NULL) {
    USBH_ASSERT_MAGIC(parentPort, USBH_HUB_PORT);
    USBH_WARN((USBH_MTYPE_HUB, "_HubNotifyError: pHub ref.ct %ld portnumber: %d portstate: %s", hub->pHubDevice->RefCount, (int)parentPort->HubPortNumber,
               USBH_PortState2Str(parentPort->PortState)));
    USBH_BD_SetPortState(parentPort, PORT_ERROR);
  }
  USBH_MarkParentAndChildDevicesAsRemoved(hub->pHubDevice);
  DEC_REF(hub->pHubDevice); // Delete notify State machine reference
  // Service all ports
  USBH_HC_ServicePorts(hub->pHubDevice->pHostController);
}

/*********************************************************************
*
*       _EnumPrepareGetDeviceStatus
*/
static void _EnumPrepareGetDeviceStatus(USB_DEVICE * Dev, void * Buffer, U16 reqLength) {
  USBH_URB * urb;
  U16   length;
  length                                    = USBH_MIN(reqLength, USB_STATUS_LENGTH);
  urb                                       = &Dev->EnumUrb;
  USBH_ZERO_MEMORY(urb, sizeof(USBH_URB));
  urb->Header.Function                      = USBH_FUNCTION_CONTROL_REQUEST;
  urb->Request.ControlRequest.Setup.Type    = USB_STATUS_DEVICE; // STD, IN, device
  urb->Request.ControlRequest.Setup.Request = USB_REQ_GET_STATUS;
  urb->Request.ControlRequest.Setup.Length  = length;
  urb->Request.ControlRequest.pBuffer        = Buffer;
  urb->Request.ControlRequest.Length        = length;
}

/*********************************************************************
*
*       _HubEnumParentPortRestart
*/
static void _HubEnumParentPortRestart(USB_HUB * hub, USBH_STATUS enum_status) {
  USBH_LOG((USBH_MTYPE_HUB, "_HubEnumParentPortRestart!"));
  USBH_ASSERT_MAGIC(hub, USB_HUB);
  USBH_EnumParentPortRestart(hub->pHubDevice,enum_status); // Parent port restart
}

/*********************************************************************
*
*       _HubEnumParentPortError
*/
static void _HubEnumParentPortError(USB_HUB* hub, USBH_STATUS enumStatus) {
  USBH_LOG((USBH_MTYPE_HUB, "_HubEnumParentPortError!"));
  USBH_ASSERT_MAGIC(hub, USB_HUB);
  USBH_ProcessEnumPortError(hub->pHubDevice,enumStatus); // Parent port restart
}

/*********************************************************************
*
*       _ProcessHubNotification
*
*  Function description:
*    Is called in the pContext of the completion routine of the pHub
*    interrupt IN endpoint (pHub status bitmap) or in the pContext in a
*    URB completion routine or in the pContext of an timer routine if
*    waiting for an event. If the pHub device State is not working the
*    pHub device is removed!
*/
static void _ProcessHubNotification(void * usbHub) {
  USB_HUB         * pHub;
  USBH_HUB_PORT   * port;
  USB_DEVICE      * hub_dev;
  USBH_URB        * urb;
  USBH_STATUS       status;
  U32               notify;
  U32               mask;
  int               i;
  unsigned int      notify_length;
  USBH_HOST_CONTROLLER * pHostController;
  USBH_BOOL            hub_submit_error_flag;
  USBH_BOOL            restart_flag;

  hub_submit_error_flag = FALSE;
  restart_flag          = FALSE;

  pHub             = (USB_HUB *)usbHub;
  USBH_ASSERT_MAGIC(pHub,     USB_HUB);
  hub_dev         = pHub->pHubDevice;
  USBH_ASSERT_MAGIC(hub_dev, USB_DEVICE);
  urb             = &pHub->NotifyUrb;
  pHostController = hub_dev->pHostController;
  USBH_ASSERT_MAGIC(pHostController, USBH_HOST_CONTROLLER);
  USBH_LOG((USBH_MTYPE_HUB, "HUB: HUB_NOTIFY: %s ref.ct: %ld", USBH_HubNotificationState2Str(pHub->NotifyState), hub_dev->RefCount));
  if (hub_dev->State < DEV_STATE_WORKING) { // Check hubs device State
    USBH_LOG((USBH_MTYPE_HUB, "HUB: INFO HUB_NOTIFY: pHub device is less than < DEV_STATE_WORKING"));
    _HubSetNotifyState(pHub, USBH_HUB_NOTIFY_REMOVED);
    hub_submit_error_flag = TRUE;           // Indirect call of _HubNotifyError
  }
  switch (pHub->NotifyState) {
    case USBH_HUB_NOTIFY_START:
      notify_length = pHub->interruptUrb.Request.BulkIntRequest.Length;
      if (0 == notify_length) {             // Zero length packet recevied
        USBH_WARN((USBH_MTYPE_HUB, "HUB:  HUB_NOTIFY zero length pkt. on interrupt IN Ep!"));
        _HubSetNotifyState(pHub, USBH_HUB_NOTIFY_IDLE);
        break;
      }
      pHub->Notification = USBH_LoadU32LE((const U8 *)pHub->InterruptTransferBuffer);
      // Get buffer for all get status requests
      USBH_LOG((USBH_MTYPE_HUB, "HUB: USBH_HUB_NOTIFY_START: USB addr: %d portstatus: 0x%x nb.of rcv. bytes: %d!", (int)pHub->pHubDevice->UsbAddress, pHub->Notification, notify_length));
      if (!USBH_CheckCtrlTransferBuffer(hub_dev, HCD_GET_STATUS_LENGTH)) {      // Prepare the transfer buffer to get the port status
        USBH_WARN((USBH_MTYPE_HUB, "HUB: FATAL HUB_NOTIFY USBH_HUB_NOTIFY_START:USBH_CheckCtrlTransferBuffer!"));
        return;
      }
      if (pHub->Notification & 0x01) {
        USBH_LOG((USBH_MTYPE_HUB, "HUB: HUB_NOTIFY: pHub status changed!"));
        _HubPrepareGetPortStatus(urb, 0, hub_dev->pCtrlTransferBuffer, (U16)hub_dev->CtrlTransferBufferSize); // Get pHub status selector is zero!
        _HubSetNotifyState(pHub, USBH_HUB_NOTIFY_GET_HUB_STATUS);                                                             // Get pHub status, clear pHub status
        status = USBH_URB_SubStateSubmitRequest(&pHub->NotifySubState, urb, DEFAULT_SETUP_TIMEOUT, NULL);
        if (status != USBH_STATUS_PENDING) {
          USBH_WARN((USBH_MTYPE_HUB, "HUB:  HUB_NOTIFY USBH_HUB_NOTIFY_START: USBH_URB_SubStateSubmitRequest: st:%08x", urb->Header.Status));
          hub_submit_error_flag = TRUE;
          break;
        } else {
          return; // Wait for completion
        }
      } else {
        pHub->NotifyPortCt = pHub->PortCount; // No pHub notification available, start port status
        USBH_LOG((USBH_MTYPE_HUB, "HUB: HUB_NOTIFY: start get port status! ports: %d!",pHub->PortCount));
        _HubSetNotifyState(pHub, USBH_HUB_NOTIFY_GET_PORT_STATUS);
        pHub->SubmitFlag = FALSE;            // Prevent checking of URB in the new notify_state
        _ProcessHubNotification(pHub);        // Recursive
        return;
      }
    case USBH_HUB_NOTIFY_GET_HUB_STATUS:
      if (urb->Header.Status != USBH_STATUS_SUCCESS) {
        USBH_WARN((USBH_MTYPE_HUB, "HUB:  HUB_NOTIFY USBH_HUB_NOTIFY_GET_HUB_STATUS urb  st:%08x", urb->Header.Status)); // On error this can be also an timeout
        restart_flag = TRUE;                                                                                        // Try again with USBH_HUB_NOTIFY_START
        break;
      }
      // Get pHub status
      USBH_ASSERT(urb->Request.ControlRequest.Length >= 4);
      pHub->Status     = USBH_LoadU32LE((const U8 *)hub_dev->pCtrlTransferBuffer);
      pHub->NotifyTemp = pHub->Status;
      // Prevent checking of URB in the new notify_state!
      pHub->SubmitFlag = FALSE;
      USBH_LOG((USBH_MTYPE_HUB, "HUB: HUB_NOTIFY: HubStatus %08x",pHub->Status));
      _HubSetNotifyState(pHub, USBH_HUB_NOTIFY_CLEAR_HUB_STATUS);
    case USBH_HUB_NOTIFY_CLEAR_HUB_STATUS: // Fall trough
      if (pHub->SubmitFlag) {
        pHub->SubmitFlag = FALSE;
        // Clear change status submitted
        if (urb->Header.Status != USBH_STATUS_SUCCESS) { // On error
          USBH_WARN((USBH_MTYPE_HUB, "HUB:  HUB_NOTIFY USBH_HUB_NOTIFY_CLEAR_HUB_STATUS urb st:%08x", urb->Header.Status));
          restart_flag = TRUE;                          // Try again
          break;
        }
      }
      // Check pHub change bits
      pHub->SubmitFlag = FALSE;
      if (pHub->NotifyTemp & HUB_STATUS_C_LOCAL_POWER) {
        pHub->NotifyTemp &= ~HUB_STATUS_C_LOCAL_POWER;
        USBH_LOG((USBH_MTYPE_HUB, "HUB: HUB_NOTIFY: HUB_STATUS_C_LOCAL_POWER is cleared"));
        USBH_BD_HubPrepareClrFeatureReq(urb, HDC_SELECTOR_C_HUB_LOCAL_POWER, 0);
        pHub->SubmitFlag = TRUE;
      } else {
        if (pHub->NotifyTemp & HUB_STATUS_C_OVER_CURRENT) {
          pHub->NotifyTemp &= ~HUB_STATUS_C_OVER_CURRENT;
          USBH_BD_HubPrepareClrFeatureReq(urb, HDC_SELECTOR_C_HUB_OVER_CURRENT, 0);
          USBH_LOG((USBH_MTYPE_HUB, "HUB: HUB_NOTIFY: HUB_STATUS_C_OVER_CURRENT is cleared"));
          pHub->SubmitFlag = TRUE;
        }
      }
      if (pHub->SubmitFlag) {
        status = USBH_URB_SubStateSubmitRequest(&pHub->NotifySubState, urb, DEFAULT_SETUP_TIMEOUT, NULL);
        if (status != USBH_STATUS_PENDING) {
          USBH_WARN((USBH_MTYPE_HUB, "HUB:  HUB_NOTIFY USBH_URB_SubStateSubmitRequest: Clear pHub status st:%08x", urb->Header.Status));
          hub_submit_error_flag = TRUE;
          break;
        } else {
          return; // Wait for completion
        }
      } else { // All submitted
        pHub->NotifyPortCt = pHub->PortCount;
        _HubSetNotifyState(pHub, USBH_HUB_NOTIFY_GET_PORT_STATUS);
        pHub->SubmitFlag   = FALSE;
      }
    case USBH_HUB_NOTIFY_GET_PORT_STATUS: // Fall trough
      // 1. no URB is submitted the first get port status URB is submitted
      // 2. After the first port completion the status change bit is cleared.
      // 3. After status change is cleared the connect, remove an overcurrent conditions are checked.
      // 4. Until not all ports processed the State USBH_HUB_NOTIFY_GET_PORT_STATUS is started again
      // 5. If all ports processed the State is Idle and the next interrupt IN status request is submitted before USBH_HC_ServicePorts is called
      if (pHub->SubmitFlag) { // URB completed
        pHub->SubmitFlag = FALSE;
        if (urb->Header.Status != USBH_STATUS_SUCCESS) { // On error
          USBH_WARN((USBH_MTYPE_HUB, "HUB:  HUB_NOTIFY USBH_HUB_NOTIFY_GET_PORT_STATUS urb st:%08x", urb->Header.Status));
          restart_flag = TRUE;                          // Try again
          break;
        }
        USBH_ASSERT(urb->Request.ControlRequest.Length >= 4);
        USBH_ASSERT_PTR(pHub->NotifyPort);
        // Set the port status
        pHub->NotifyTemp                   = USBH_LoadU32LE((const U8 *)hub_dev->pCtrlTransferBuffer);
        pHub->NotifyPort->PortStatus       = pHub->NotifyTemp;
        pHub->NotifyPort->PortStatusShadow = pHub->NotifyTemp;
        USBH_LOG((USBH_MTYPE_HUB, "HUB: HUB_NOTIFY: port: %d port status: 0x%lx", pHub->NotifyPort->HubPortNumber,pHub->NotifyTemp));
        if (pHub->NotifyTemp &(PORT_C_STATUS_CONNECT | PORT_C_STATUS_ENABLE | PORT_C_STATUS_SUSPEND | PORT_C_STATUS_OVER_CURRENT | PORT_C_STATUS_RESET)) {
          // Release this notify_state and delete all change bits in pHub->NotifyTemp, return until all port states are cleared
          _HubSetNotifyState(pHub, USBH_HUB_NOTIFY_CLR_PORT_STATUS);
          pHub->SubmitFlag = FALSE;
          _ProcessHubNotification(pHub); // Recursive call
          return;
        }
      }
      notify = 0;
      while (pHub->NotifyPortCt) {
        notify = pHub->Notification &(0x01 << pHub->NotifyPortCt);
        pHub->NotifyPortCt--;
        if (notify) {
          break;
        }
      }
      if (notify) {
        pHub->NotifyPort = USBH_BD_HubGetPortByNumber(pHub, (U8)(pHub->NotifyPortCt + 1));
        _HubPrepareGetPortStatus(urb, (U16)(pHub->NotifyPortCt + 1),              // Port number
        hub_dev->pCtrlTransferBuffer, (U16)hub_dev->CtrlTransferBufferSize);
        pHub->SubmitFlag = TRUE;
        status = USBH_URB_SubStateSubmitRequest(&pHub->NotifySubState, urb, DEFAULT_SETUP_TIMEOUT, NULL);
        if (status != USBH_STATUS_PENDING) {
          USBH_WARN((USBH_MTYPE_HUB, "HUB:  HUB_NOTIFY USBH_URB_SubStateSubmitRequest: Get port status st:%08x", urb->Header.Status));
          hub_submit_error_flag = TRUE;
        } else {
          return; // Wait for completion
        }
      } else {
        _HubSetNotifyState(pHub, USBH_HUB_NOTIFY_IDLE); // All get status requests done, set idle and submit an new pHub interrupt IN status URB
      }
      break;
    case USBH_HUB_NOTIFY_CLR_PORT_STATUS:
      port = pHub->NotifyPort;
      USBH_ASSERT_MAGIC(port, USBH_HUB_PORT);
      // Clears all port status bits
      if (pHub->SubmitFlag) {
        pHub->SubmitFlag = FALSE;
        if (urb->Header.Status != USBH_STATUS_SUCCESS) { // On error
          USBH_WARN((USBH_MTYPE_HUB, "HUB:  HUB_NOTIFY USBH_HUB_NOTIFY_CLR_PORT_STATUS urb st:%08x", urb->Header.Status));
          // Try again
          restart_flag = TRUE;
          break;
        }
      }
      mask = PORT_C_STATUS_CONNECT;
      for (i = 0; i < 5; i++) {     // Check all five port change bits
        if (pHub->NotifyTemp & mask) {
          pHub->NotifyTemp &= ~mask; // clear change bit before submit the request
          // Submit clear prot status
          pHub->SubmitFlag = TRUE;
          USBH_BD_HubPrepareClrFeatureReq(urb, (U16)(i + HDC_SELECTOR_C_PORT_CONNECTION), (U16)pHub->NotifyPort->HubPortNumber);
          status = USBH_URB_SubStateSubmitRequest(&pHub->NotifySubState, urb, DEFAULT_SETUP_TIMEOUT, NULL);
          if (status != USBH_STATUS_PENDING) {
            USBH_WARN((USBH_MTYPE_HUB, "HUB:  HUB_NOTIFY USBH_URB_SubStateSubmitRequest: Clear pHub status st:%08x", urb->Header.Status));
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
      // All change bits from the current port number are cleared! Update the State of an device if no device enumeration is running
      // and update the port State if no port reset enumeration is runnung!
      if (port != pHub->EnumPort) {                             // On overcurrent mark the device as removed
        _HubSetNotifyState(pHub, USBH_HUB_NOTIFY_CHECK_OVER_CURRENT); // Port is not enumerated, update port State and device State in the next states
      } else {                                                 // Fall trough
        _HubSetNotifyState(pHub, USBH_HUB_NOTIFY_GET_PORT_STATUS);
        _ProcessHubNotification(pHub);                           // Recursive call
        return;
      }
    case USBH_HUB_NOTIFY_CHECK_OVER_CURRENT:
      port = pHub->NotifyPort;
      USBH_ASSERT_MAGIC(port, USBH_HUB_PORT);
      if (pHub->SubmitFlag) {                                   // Disable port request complete
        if (urb->Header.Status != USBH_STATUS_SUCCESS) {        // On error
          USBH_WARN((USBH_MTYPE_HUB, "HUB:  HUB_NOTIFY USBH_HUB_NOTIFY_CHECK_OVER_CURRENT urb st:%08x", urb->Header.Status));
          restart_flag = TRUE;
          break;
        }
      } else {
        if (port->PortStatus & PORT_C_STATUS_OVER_CURRENT) {
          port->PortStatus &= ~PORT_C_STATUS_OVER_CURRENT;
          if ((port->PortStatus &PORT_STATUS_OVER_CURRENT)) {  // Overcurrent
            USBH_LOG((USBH_MTYPE_HUB, "HUB: HUB_NOTIFY: over current Port:%d Status:%08x", port->HubPortNumber,port->PortStatus));
            if (port->Device != NULL) {
              USBH_MarkParentAndChildDevicesAsRemoved(port->Device);
            }
            USBH_BD_SetPortState(port, PORT_RESTART);               // Should we restart a device over current? Its better than forget it!
            _HubSetNotifyState(pHub, USBH_HUB_NOTIFY_DISABLE_PORT);
            _ProcessHubNotification(pHub);                       // Recursive call
            return;
          }

#if (USBH_DEBUG > 1)
          else {                                               // Overcurrent gone
            USBH_LOG((USBH_MTYPE_HUB, "HUB: HUB_NOTIFY: over current gone! Port:%d Status:%08x", port->HubPortNumber,port->PortStatus));
          }
#endif
        }
      }
      _HubSetNotifyState(pHub, USBH_HUB_NOTIFY_CHECK_CONNECT);        // Checking the port connect bit
    case USBH_HUB_NOTIFY_CHECK_CONNECT:                             // Fall trough
      port = pHub->NotifyPort;
      USBH_ASSERT_MAGIC(port, USBH_HUB_PORT);

      if (port->PortStatus & PORT_C_STATUS_CONNECT) {
        if ((port->PortStatus &PORT_STATUS_CONNECT) && !(port->PortStatus &PORT_STATUS_ENABLED)) {
          USBH_LOG((USBH_MTYPE_HUB, "HUB: HUB_NOTIFY: Port:%d must be enumerated", (int)port->HubPortNumber));
          if (port->PortState >= PORT_ENABLED && port->Device != NULL) { // This device must be enumerated
            USBH_MarkParentAndChildDevicesAsRemoved(port->Device);     // Remove the old connected device first
          }
          USBH_BD_SetPortState(port, PORT_CONNECTED);
          port->RetryCounter = 0;
        }
      }
      _HubSetNotifyState(pHub, USBH_HUB_NOTIFY_CHECK_REMOVE); // Go to USBH_HUB_NOTIFY_CHECK_REMOVE
    case USBH_HUB_NOTIFY_CHECK_REMOVE: // Fall trough
      port = pHub->NotifyPort;
      USBH_ASSERT_MAGIC(port, USBH_HUB_PORT);
      if (pHub->SubmitFlag) {
        pHub->SubmitFlag = FALSE;
        if (urb->Header.Status != USBH_STATUS_SUCCESS) {
          USBH_WARN((USBH_MTYPE_HUB, "HUB:  HUB_NOTIFY USBH_HUB_NOTIFY_CHECK_REMOVE urb st:%08x", urb->Header.Status)); // On error
          restart_flag = TRUE; // Try again
          break;
        }
      } else {
        if (port->PortStatus & PORT_C_STATUS_CONNECT) {
          port->PortStatus &= ~PORT_C_STATUS_CONNECT;          // PORT_C_STATUS_CONNECT no more used on this port, clear it
          if (!(port->PortStatus &PORT_STATUS_CONNECT)) {
            if (port->Device != NULL) {                        // Port is removed
              USBH_MarkParentAndChildDevicesAsRemoved(port->Device);
              USBH_LOG((USBH_MTYPE_HUB, "HUB: USBH_HUB_NOTIFY_CHECK_REMOVE: port is removed"));
              USBH_BD_SetPortState(port, PORT_REMOVED);
              port->RetryCounter = 0;
              _HubSetNotifyState(pHub, USBH_HUB_NOTIFY_DISABLE_PORT); // Disable the port
              _ProcessHubNotification(pHub);                     // Recursive
              return;
            }
          }
        }
      }
      _HubSetNotifyState(pHub, USBH_HUB_NOTIFY_GET_PORT_STATUS);
      _ProcessHubNotification(pHub);                             // Recursive
      return;
    //  Helper notify_state returns to OldNotifyState or delete the device on error!
    case USBH_HUB_NOTIFY_DISABLE_PORT:
      port = pHub->NotifyPort;
      USBH_ASSERT_MAGIC(port, USBH_HUB_PORT);
      if (pHub->SubmitFlag) {
        pHub->SubmitFlag = FALSE;
        if (urb->Header.Status != USBH_STATUS_SUCCESS) {
          USBH_WARN((USBH_MTYPE_HUB, "HUB:  HUB_NOTIFY USBH_HUB_NOTIFY_DISABLE_PORT urb st:%08x", urb->Header.Status)); // On error
          restart_flag = TRUE;                  // Try again
          break;
        }
        pHub->NotifyState = pHub->OldNotifyState; // Go back to the previous State
        _ProcessHubNotification(pHub);            // Recursive
        return;
      } else {
        USBH_LOG((USBH_MTYPE_HUB, "HUB: HUB_NOTIFY: Port:%d is disabled", (int)port->HubPortNumber)); // Start submitting an port disable
        pHub->SubmitFlag = TRUE;                                                                       // Disable the port to avoid fire -)
        USBH_BD_HubPrepareClrFeatureReq(urb, HDC_SELECTOR_PORT_ENABLE, port->HubPortNumber);               // Disable the port
        status = USBH_URB_SubStateSubmitRequest(&pHub->NotifySubState, urb, DEFAULT_SETUP_TIMEOUT, NULL);
        if (status != USBH_STATUS_PENDING) {
          USBH_WARN((USBH_MTYPE_HUB, "HUB:  _ProcessEnumHub: USBH_HUB_NOTIFY_DISABLE_PORT:" "USBH_URB_SubStateSubmitRequest st:%08x", urb->Header.Status));
          hub_submit_error_flag = TRUE; // submit error
        } else {
          return;                       // Wait for completion
        }
      }
      break;
    case USBH_HUB_NOTIFY_REMOVED:
      break; // Stop processing
    case USBH_HUB_NOTIFY_ERROR:
      break; // Stop processing
    case USBH_HUB_NOTIFY_IDLE:
      // At the end of this function an new HUB status request is submitted in this State. This can not made here because some states set the Idle State.
      break;
    default:
      USBH_ASSERT0;
      USBH_WARN((USBH_MTYPE_HUB, "HUB:  HUB_NOTIFY invalid  pHub->NotifyState: %d", pHub->NotifyState));
  }
  // Check error flags
  if (restart_flag) {
    _HubNotifyRestart(pHub);
  } else if (hub_submit_error_flag) {
    _HubNotifyError(pHub);
  } else {
    if (USBH_HUB_NOTIFY_IDLE == pHub->NotifyState && DEV_STATE_WORKING == hub_dev->State) {     // On success
      pHub->interruptUrb.Request.BulkIntRequest.Length = pHub->InterruptTransferBufferSize; // ready pHub device and notify State idle
      status = USBH_BD_EpSubmitUrb(pHub->InterruptEp, &pHub->interruptUrb);
      if (status != USBH_STATUS_PENDING) {
        USBH_WARN((USBH_MTYPE_HUB, "HUB:  HUB_NOTIFY USBH_BD_EpSubmitUrb on interrupt IN: st:%08x", urb->Header.Status));
        _HubNotifyError(pHub);
        return;
      }
    }
    USBH_HC_ServicePorts(pHostController); // Service all ports
  }
} // _ProcessHubNotification

/*********************************************************************
*
*       _HubNotifyClearEndpointStallCompletion
*
*  Function description:
*    Hub clear pipe completion
*/
static void _HubNotifyClearEndpointStallCompletion(USBH_URB * urb) {
  USBH_DEFAULT_EP * defaultEndpoint;
	USB_DEVICE * Device;
  USB_HUB    * hub;

	defaultEndpoint = (USBH_DEFAULT_EP *)urb->Header.pInternalContext;
  Device          = defaultEndpoint->pUsbDevice;
  USBH_ASSERT_MAGIC(Device, USB_DEVICE);
  hub             = Device->pUsbHub;
  USBH_ASSERT_MAGIC(hub, USB_HUB);
  // Decrement the count
  defaultEndpoint->UrbCount--;
  USBH_LOG((USBH_MTYPE_HUB, "_HubNotifyClearEndpointStallCompletion: urbcount: %u",defaultEndpoint->UrbCount));
  // Check the URB Status
  if (urb->Header.Status) {
    USBH_WARN((USBH_MTYPE_HUB, "_HubNotifyClearEndpointStallCompletion: st:%08x",urb->Header.Status));
    // On error try to restart the hub device
    DEC_REF(Device);// Clear local URB reference
    _HubNotifyError(hub);
    return;
  } else {
    // On success do nothing and submit an new hub Status request in the _ProcessHubNotification()
    _HubSetNotifyState(hub,USBH_HUB_NOTIFY_IDLE);
  }
  _ProcessHubNotification(hub);
  DEC_REF(Device);
}

/*********************************************************************
*
*       _HubAddAllPorts
*/
static USBH_STATUS _HubAddAllPorts(USB_HUB* hub) {
  unsigned int   i;
  USBH_HUB_PORT     * HubPort;
  USB_DEVICE   * dev;
  USBH_ASSERT_MAGIC( hub, USB_HUB );
  USBH_LOG((USBH_MTYPE_HUB, "_HubAddAllPorts Ports: %d!",hub->PortCount));
  dev = hub->pHubDevice;
  if (0 == (dev->DevStatus & USB_STATUS_SELF_POWERED)
    &&  !dev->pParentPort->HighPowerFlag ) {
    USBH_WARN((USBH_MTYPE_HUB, "HubCreateAllPorts: self powered hub on an low powered parent port!"));
    return USBH_STATUS_INVALID_PARAM;
  };
  if ( 0==hub->PortCount ) {
    USBH_WARN((USBH_MTYPE_HUB, "HubCreateAllPorts: no ports!"));
    return USBH_STATUS_INVALID_PARAM;
  }
  if ( !USBH_DLIST_IsEmpty( &hub->PortList ) ) {
    USBH_WARN((USBH_MTYPE_HUB, "HubCreateAllPorts: PortList not empty!",                             hub->PortCount));
    return USBH_STATUS_INVALID_PARAM;
  }
  for (i=1;i<=hub->PortCount;i++) {
    // Initialize and create the hub ports
    HubPort = USBH_BD_NewHubPort();
		if (HubPort == NULL) {
		  USBH_WARN((USBH_MTYPE_HUB, "HubCreateAllPorts: USBH_BD_NewHubPort failed"));
			return USBH_STATUS_RESOURCES;
		}
    if ( dev->DevStatus & USB_STATUS_SELF_POWERED ) {
      HubPort->HighPowerFlag=TRUE;
    }
    HubPort->HubPortNumber = (unsigned char)i;
    HubPort->ExtHub        = hub;
    USBH_DLIST_InsertTail(&hub->PortList,&HubPort->ListEntry);
  }
  return USBH_STATUS_SUCCESS;
}

/*********************************************************************
*
*       _HubStatusRequestCompletion
*
*  Function description:
*/
static void _HubStatusRequestCompletion(USBH_URB * pUrb) {
  USB_HUB      * hub = (USB_HUB *)pUrb->Header.pInternalContext;
  USB_ENDPOINT * usb_ep;
  USBH_STATUS    Status;

  USBH_ASSERT_MAGIC(hub, USB_HUB);
  USBH_LOG((USBH_MTYPE_HUB, "HUB: [_HubStatusRequestCompletion Ref.ct: %ld",hub->pHubDevice->RefCount));
  usb_ep = hub->InterruptEp;
  usb_ep->UrbCount--;
  USBH_ASSERT(hub->NotifyState == USBH_HUB_NOTIFY_IDLE);                              // Status URB only in USBH_HUB_NOTIFY_IDLE State allowed
  if (USBH_STATUS_SUCCESS != pUrb->Header.Status) {                            // Check Status
    USBH_WARN((USBH_MTYPE_HUB, "HUB:  _HubStatusRequestCompletion: st:%08x",pUrb->Header.Status));
    if (hub->pHubDevice->State == DEV_STATE_WORKING) {                         // Hub device ready
      Status = USBH_BD_SubmitClearFeatureEndpointStall(&hub->pHubDevice->DefaultEp, &hub->NotifyUrb,
                 hub->InterruptEp->pEndpointDescriptor[USB_EP_DESC_ADDRESS_OFS], _HubNotifyClearEndpointStallCompletion, NULL);
      if (Status != USBH_STATUS_PENDING) {
        USBH_WARN((USBH_MTYPE_HUB, "HUB:  _HubStatusRequestCompletion: SubmitClearFeatureEndpointStall Status: 0x%lx!", Status));
        hub->NotifyUrb.Header.Status = Status;
        DEC_REF(hub->pHubDevice);                                              // Clear the local reference
        _HubNotifyError(hub);
        return;
      }
      goto exit;
    }
  } else {
    _HubSetNotifyState(hub, USBH_HUB_NOTIFY_START);                                 // Successful URB Status
  }
  _ProcessHubNotification(hub);
  exit:
  DEC_REF(hub->pHubDevice);                                                    // Clear the local reference
  USBH_LOG((USBH_MTYPE_HUB, "HUB: ]_HubStatusRequestCompletion"));
} // _HubStatusRequestCompletion

/*********************************************************************
*
*       _HubInstallPeriodicStatusTransfer
*/
static USBH_STATUS _HubInstallPeriodicStatusTransfer(USB_HUB * pHub) {
  USB_DEVICE          * pDev;
  USB_INTERFACE       * iface;
  USBH_INTERFACE_MASK   iMask;
  USBH_STATUS           Status;
  USBH_EP_MASK          epMask;
  USB_ENDPOINT        * ep;
  USBH_URB            * urb;

  USBH_LOG((USBH_MTYPE_HUB, "_HubInstallPeriodicStatusTransfer !"));
  USBH_ASSERT_MAGIC(pHub,            USB_HUB);
  USBH_ASSERT_MAGIC(pHub->pHubDevice, USB_DEVICE);
  pDev = pHub->pHubDevice;
  // Get the first interface
  iMask.Mask      = USBH_INFO_MASK_INTERFACE | USBH_INFO_MASK_CLASS;
  iMask.Interface = USBHUB_DEFAULT_INTERFACE;
  iMask.Class     = USB_DEVICE_CLASS_HUB;

  Status=USBH_SearchUsbInterface(pDev,&iMask,&iface);
  if (Status) {
    // On error
    USBH_WARN((USBH_MTYPE_HUB, "_HubInstallPeriodicStatusTransfer: USBH_SearchUsbInterface!\n"));
    return Status;
  }
  // Get an interrupt in endpoint
  epMask.Mask      = USBH_EP_MASK_TYPE | USBH_EP_MASK_DIRECTION;
  epMask.Direction = USB_IN_DIRECTION;
  epMask.Type      = USB_EP_TYPE_INT;
  ep               = USBH_BD_SearchUsbEndpointInInterface(iface,&epMask);
  if( NULL == ep ) { // On error
    USBH_WARN((USBH_MTYPE_HUB, "_HubInstallPeriodicStatusTransfer: UbdGetEndpointDescriptorFromInterface!\n"));
    return USBH_STATUS_INVALID_PARAM;
  }
  pHub->InterruptEp = ep;
  // Initialize the pUrb
  pHub->InterruptTransferBufferSize = USBH_GetUshortFromDesc(ep->pEndpointDescriptor, USB_EP_DESC_PACKET_SIZE_OFS);
  // Allocate an URB buffer
  USBH_ASSERT(NULL == pHub->InterruptTransferBuffer);
  pHub->InterruptTransferBuffer = USBH_URB_BufferAllocateTransferBuffer(pHub->InterruptTransferBufferSize);
//  pHub->InterruptTransferBuffer = UbdAllocateTransferBuffer(pHub->InterruptTransferBufferSize);
	if (pHub->InterruptTransferBuffer == NULL) {
		USBH_WARN((USBH_MTYPE_HUB, "_HubInstallPeriodicStatusTransfer:TAL_AllocatePhysicalMemory failed\n"));
		return USBH_STATUS_ERROR;
  }
  urb = &pHub->interruptUrb;
  USBH_ZERO_MEMORY(urb, sizeof(USBH_URB));
  urb->Header.pfOnInternalCompletion     = _HubStatusRequestCompletion;
  urb->Header.pInternalContext        = pHub;
  urb->Header.Function               = USBH_FUNCTION_INT_REQUEST;
  urb->Request.BulkIntRequest.pBuffer = pHub->InterruptTransferBuffer;
  urb->Request.BulkIntRequest.Length = pHub->InterruptTransferBufferSize;
  // Initial reference for the pHub notification= 2. This reference is decremented at the State HUB_NOTIFY_REOVED or USBH_HUB_NOTIFY_ERROR
  INC_REF(pHub->pHubDevice);
  Status = USBH_BD_EpSubmitUrb(ep, urb);
  return Status;
}

/*********************************************************************
*
*       _HubPortResetSetIdleServicePorts
*
*  Function description:
*    Called if the device on the port is addressed and the device enumeration process
*    has been started. Also called from routines that are called if _HubProcessPortResetSetAddress() stops.
*/
static void _HubPortResetSetIdleServicePorts(USB_HUB * pHub) {
  USBH_ASSERT_MAGIC(pHub, USB_HUB);
  pHub->PortResetEnumState = USBH_HUB_PORTRESET_IDLE;
  HC_CLEAR_ACTIVE_PORTRESET(pHub->pHubDevice->pHostController, pHub->EnumPort); // Allow starting an port reset on other port
  pHub->EnumDevice         = NULL;
  pHub->EnumPort           = NULL;
  USBH_HC_ServicePorts(pHub->pHubDevice->pHostController);                        // Service all ports
  DEC_REF(pHub->pHubDevice);                                                  // This pHub port reset State machine is released here!
}

/*********************************************************************
*
*       _HubPortResetError
*
*  Function description:
*/
static void _HubPortResetError(USB_HUB * pHub, USBH_STATUS Status) {
  USBH_ASSERT_MAGIC(pHub, USB_HUB);
  USBH_LOG((USBH_MTYPE_HUB, "HUB: _HubPortResetError: %s hubs ref.ct: %ld", USBH_HubPortResetState2Str(pHub->PortResetEnumState), pHub->pHubDevice->RefCount));
  if (NULL != pHub->EnumPort) {
    USBH_BD_SetPortState(pHub->EnumPort, PORT_ERROR);
  }
  if (NULL != pHub->EnumDevice) {
    DEC_REF(pHub->EnumDevice);
  }
  UbdSetEnumErrorNotificationHubPortReset(pHub->EnumPort, pHub->PortResetEnumState, Status); // Notify user
  _HubPortResetSetIdleServicePorts(pHub);
}

/*********************************************************************
*
*       _HubPortResetDisablePort
*/
static void _HubPortResetDisablePort(USB_HUB * pHub) {
  USBH_STATUS status;
  USBH_LOG((USBH_MTYPE_HUB, "HubPortResetDisablePortAndStop: %s hubs ref.ct: %ld\n", USBH_HubPortResetState2Str(pHub->PortResetEnumState),pHub->pHubDevice->RefCount));
  if (pHub->pHubDevice->State < DEV_STATE_WORKING) {
    // On an request error set port State to HUB_ENUM_PORT_ERROR
    USBH_WARN((USBH_MTYPE_HUB, "HubPortResetDisablePortAndStop: pHub device State is not working, stop\n"));
    _HubPortResetError(pHub,USBH_STATUS_PORT);
    return;
  }
  // Submit the disable port request
  USBH_BD_HubPrepareClrFeatureReq(&pHub->EnumUrb,  HDC_SELECTOR_PORT_ENABLE,pHub->EnumPort->HubPortNumber);
  // Next port reset State
  pHub->PortResetEnumState = USBH_HUB_PORTRESET_DISABLE_PORT;
  status=USBH_URB_SubStateSubmitRequest(&pHub->PortResetSubState, &pHub->EnumUrb, DEFAULT_SETUP_TIMEOUT, pHub->pHubDevice);
  if (status != USBH_STATUS_PENDING) {
    // On request error stop State machine
    USBH_WARN((USBH_MTYPE_HUB, "HubDisablePortAndRestartDeviceReset: USBH_URB_SubStateSubmitRequest: USBH_HUB_PORTRESET_IS_ENABLED disable port: st: %08x\n", pHub->EnumUrb.Header.Status));
    // On a request error set port State to HUB_ENUM_PORT_ERROR
    _HubPortResetError(pHub, status);
  }
}

/*********************************************************************
*
*       _ProcessEnumHub
*
*  Function description:
*/
static void _ProcessEnumHub(void * p) {
  USB_DEVICE  * pHubDev;
  USBH_STATUS   Status;
  USB_HUB     * pHub;
  USBH_URB    * pUrb;

  pHub     = (USB_HUB *)p;
  USBH_ASSERT_MAGIC(pHub, USB_HUB);
  USBH_ASSERT_MAGIC(pHub->pHubDevice, USB_DEVICE);
  pHubDev  = pHub->pHubDevice;
  pUrb         = &pHubDev->EnumUrb;             // During device enumeration the URB from the device is used!
  USBH_LOG((USBH_MTYPE_HUB, "HUB: _ProcessEnumHub %s Dev.ref.ct: %ld", USBH_HubEnumState2Str(pHub->EnumState),pHubDev->RefCount));
  if (pHubDev->pHostController->State == HC_REMOVED) {
    pHub->EnumState = USBH_HUB_ENUM_REMOVED;        // Valid for all child devices
  }
  if (NULL != pHubDev->pParentPort->ExtHub) {
    USB_HUB * parentHub;                          // The parent port is an external hub
    parentHub = pHubDev->pParentPort->ExtHub;
    if (parentHub->pHubDevice->State < DEV_STATE_WORKING) {
      pHub->EnumState = USBH_HUB_ENUM_REMOVED;      // Parent hub does not work
    }
  }
  switch (pHub->EnumState) {
  case USBH_HUB_ENUM_START:
    if (!USBH_CheckCtrlTransferBuffer(pHubDev, HDC_MAX_HUB_DESCRIPTOR_LENGTH)) {
      USBH_WARN((USBH_MTYPE_HUB, "HUB: FATAL _ProcessEnumHub: USBH_HUB_ENUM_START:USBH_CheckCtrlTransferBuffer!"));
      return;
    }
    _EnumPrepareGetDeviceStatus(pHubDev, pHubDev->pCtrlTransferBuffer, HDC_MAX_HUB_DESCRIPTOR_LENGTH);
    pHub->EnumState = USBH_HUB_ENUM_GET_STATUS;                                                                      // Set the new State
    Status             = USBH_URB_SubStateSubmitRequest(&pHub->EnumSubState, pUrb, DEFAULT_SETUP_TIMEOUT, pHubDev); // Start the request
    if (Status != USBH_STATUS_PENDING) {
      USBH_WARN((USBH_MTYPE_HUB, "HUB:  _ProcessEnumHub: USBH_HUB_ENUM_START submit pUrb: st:%08x", pUrb->Header.Status));
      _HubEnumParentPortError(pHub, Status);
    }
    break;
  case USBH_HUB_ENUM_GET_STATUS:
    if (pUrb->Header.Status != USBH_STATUS_SUCCESS) {                                                                   // On error
      USBH_WARN((USBH_MTYPE_HUB, "HUB:  _ProcessEnumHub: get device Status pUrb st:%08x", pUrb->Header.Status));
      _HubEnumParentPortRestart(pHub, USBH_STATUS_INVALID_DESCRIPTOR);
      return;
    }
    pHubDev->DevStatus = USBH_LoadU16LE((U8 *)pUrb->Request.ControlRequest.pBuffer);                                  // Copy the device Status
    _HubPrepareGetHubDesc(pUrb, pHubDev->pCtrlTransferBuffer, HDC_MAX_HUB_DESCRIPTOR_LENGTH);                          // Get the hub descriptor
    pHub->EnumState    = USBH_HUB_ENUM_HUB_DESC;                                                                         // Set the new State
    Status                = USBH_URB_SubStateSubmitRequest(&pHub->EnumSubState, pUrb, DEFAULT_SETUP_TIMEOUT, pHubDev);  // Start the request
    if (Status != USBH_STATUS_PENDING) {
      USBH_WARN((USBH_MTYPE_HUB, "HUB:  _ProcessEnumHub: Get pHub descriptor st:%08x", pUrb->Header.Status));
      _HubEnumParentPortError(pHub, Status);
    }
    break;
  case USBH_HUB_ENUM_HUB_DESC:
    if (pUrb->Header.Status != USBH_STATUS_SUCCESS) {                                                    // On error this can be also a timeout
      USBH_WARN((USBH_MTYPE_HUB, "HUB:  _ProcessEnumHub: Get pHub descriptor pUrb st:%08x", pUrb->Header.Status));
      _HubEnumParentPortRestart(pHub, pUrb->Header.Status);
      return;
    }
    if (_ParseHubDescriptor(pHub, (U8 *)pUrb->Request.ControlRequest.pBuffer, pUrb->Request.ControlRequest.Length)) {
      USBH_WARN((USBH_MTYPE_HUB, "HUB:  _ProcessEnumHub: _ParseHubDescriptor failed"));                   // On error
      _HubEnumParentPortRestart(pHub, USBH_STATUS_INVALID_DESCRIPTOR);
      return;
    }
    Status = _HubAddAllPorts(pHub);                                                                   // Add all ports to the pHub
    if (Status) {                                                                                       // On error
      USBH_WARN((USBH_MTYPE_HUB, "HUB:  _ProcessEnumHub: _HubAddAllPorts failed st:%08x", Status));
      _HubEnumParentPortError(pHub, Status);
      return;
    }
    pHub->EnumState         = USBH_HUB_ENUM_SET_POWER;                                                    // Set port power for all ports
    pHub->NotifyPortCt      = pHub->PortCount;
    pHubDev->EnumSubmitFlag = FALSE;
  case USBH_HUB_ENUM_SET_POWER:                                                                              // Fall through
    if (pHubDev->EnumSubmitFlag) {                                                                   // pUrb is completed
      pHubDev->EnumSubmitFlag = FALSE;
      if (pUrb->Header.Status != USBH_STATUS_SUCCESS) {                                                  // On error
        USBH_WARN((USBH_MTYPE_HUB, "HUB:  _ProcessEnumHub: Set port power pUrb st:%08x", pUrb->Header.Status));
        _HubEnumParentPortRestart(pHub, pUrb->Header.Status);
      } else {
        pHub->NotifyPortCt--;
        USBH_URB_SubStateWait(&pHub->EnumSubState, pHub->PowerGoodTime, pHubDev); // Wait for power good time, enter again to USBH_HUB_ENUM_SET_POWER
      }
      break;
    }
    if (pHub->NotifyPortCt) {                                                         // Start next set power URB or release USBH_HUB_ENUM_SET_POWER
      _HubPrepareSetFeatureReq(pUrb, HDC_SELECTOR_PORT_POWER, (U16)pHub->NotifyPortCt); // Not all ports powered
      Status = USBH_URB_SubStateSubmitRequest(&pHub->EnumSubState, pUrb, DEFAULT_SETUP_TIMEOUT, pHubDev);
      if (Status != USBH_STATUS_PENDING) {
        USBH_WARN((USBH_MTYPE_HUB, "HUB:  _ProcessEnumHub: USBH_URB_SubStateSubmitRequest: Set port power st:%08x", pUrb->Header.Status));
        _HubEnumParentPortError(pHub, Status);
      } else {
        pHubDev->EnumSubmitFlag = TRUE;
      }
      break;
    } else {
      if (!USBH_CheckCtrlTransferBuffer(pHubDev, HCD_GET_STATUS_LENGTH)) {              // All ports are powered, get port Status
        USBH_WARN((USBH_MTYPE_HUB, "HUB: FATAL _ProcessEnumHub: USBH_HUB_ENUM_SET_POWER: USBH_CheckCtrlTransferBuffer!"));
        return;
      }
      pHub->EnumState    = USBH_HUB_ENUM_PORT_STATE;
      pHub->NotifyPortCt = pHub->PortCount;                                        // Load number of ports
    }
  case USBH_HUB_ENUM_PORT_STATE: // Fall trough
    if (pHubDev->EnumSubmitFlag) {
      pHubDev->EnumSubmitFlag = FALSE;
      if (pUrb->Header.Status != USBH_STATUS_SUCCESS) { // On error
        USBH_WARN((USBH_MTYPE_HUB, "HUB:  _ProcessEnumHub: Get port Status pUrb st:%08x", pUrb->Header.Status));
        _HubEnumParentPortRestart(pHub, pUrb->Header.Status);
        break;
      } else {
        USBH_HUB_PORT * port;
        port = USBH_BD_HubGetPortByNumber(pHub, (U8)pHub->NotifyPortCt); // On success save port Status
        if (NULL == port) {
          USBH_WARN((USBH_MTYPE_HUB, "HUB:  USBH_BD_HubGetPortByNumber: port number: %d", pHub->NotifyPortCt));
          _HubEnumParentPortRestart(pHub, USBH_STATUS_INVALID_PARAM);
          break;
        }
        USBH_ASSERT(pUrb->Request.ControlRequest.Length >= 4);
        port->PortStatus       = USBH_LoadU32LE((const U8 *)pHubDev->pCtrlTransferBuffer);
        port->PortStatusShadow = port->PortStatus;
        if (port->PortStatus & PORT_STATUS_CONNECT) {
          USBH_BD_SetPortState(port, PORT_CONNECTED);                       // Device is connected, mark service required
        }
        pHub->NotifyPortCt--;
      }
    }
    if (pHub->NotifyPortCt) { // Not all ports processed
      _HubPrepareGetPortStatus(pUrb, (U16)pHub->NotifyPortCt, pHubDev->pCtrlTransferBuffer, (U16)pHubDev->CtrlTransferBufferSize);
      Status = USBH_URB_SubStateSubmitRequest(&pHub->EnumSubState, pUrb, DEFAULT_SETUP_TIMEOUT, pHubDev);
      if (Status != USBH_STATUS_PENDING) {
        USBH_WARN((USBH_MTYPE_HUB, "HUB:  _ProcessEnumHub: USBH_URB_SubStateSubmitRequest: Get port Status st:%08x", pUrb->Header.Status));
        _HubEnumParentPortError(pHub, Status);
      } else {
        pHubDev->EnumSubmitFlag = TRUE;
      }
      break;
    } else {
      pHub->EnumState = USBH_HUB_ENUM_ADD_DEVICE;         // All ports processed, new State
    }
  case USBH_HUB_ENUM_ADD_DEVICE:                             // Fall through
    pHub->EnumState    = USBH_HUB_ENUM_IDLE;              // device enumeration now complete
    pHubDev->EnumState = DEV_ENUM_IDLE;
    if (pHub->PostEnumFunction != NULL) {
      pHub->PostEnumFunction(pHub->PostEnumContext);
    }
    Status = _HubInstallPeriodicStatusTransfer(pHub); // Now the added hub interface is valid.
    if (Status != USBH_STATUS_PENDING) {
      USBH_WARN((USBH_MTYPE_HUB, "HUB:  _ProcessEnumHub: _HubInstallPeriodicStatusTransfer st:%08x", pUrb->Header.Status));
      _HubEnumParentPortError(pHub, Status);
    }
    pHub->pHubDevice->pParentPort->RetryCounter = 0;   // Reset the parent port retry counter
    break;
  case USBH_HUB_ENUM_REMOVED:
    DEC_REF(pHubDev);                                // Delete the device
    break;
  case USBH_HUB_ENUM_IDLE:
    break;
  default:
    USBH_ASSERT0;
  }                                                       // Switch
}   // _ProcessEnumHub

/*********************************************************************
*
*       _HubDeviceResetRemove
*
*  Function description:
*/
static void _HubDeviceResetRemove(USB_HUB * hub) {
  USBH_ASSERT_MAGIC(hub, USB_HUB);
  USBH_LOG((USBH_MTYPE_HUB, "HUB: _HubDeviceResetRemove"));
  if (NULL != hub->EnumPort) { // Try a restart
    USBH_BD_SetPortState(hub->EnumPort, PORT_REMOVED);
  }
  if (NULL != hub->EnumDevice) {
    DEC_REF(hub->EnumDevice);  // Delete the device
  }
  UbdSetEnumErrorNotificationHubPortReset(hub->EnumPort, hub->PortResetEnumState, USBH_STATUS_ERROR); // Notify user
  _HubPortResetSetIdleServicePorts(hub);
}

/*********************************************************************
*
*       _HubPortResetRestart
*
*  Function description:
*/
static void _HubPortResetRestart(USB_HUB * hub, USBH_STATUS status) {
  USBH_ASSERT_MAGIC(hub, USB_HUB);
  if (NULL != hub->EnumPort) {  // Try a restart
    USBH_BD_SetPortState(hub->EnumPort, PORT_RESTART);
  }
  if (NULL != hub->EnumDevice) {
    DEC_REF(hub->EnumDevice);   // Delete the device
  }
  UbdSetEnumErrorNotificationHubPortReset(hub->EnumPort, hub->PortResetEnumState, status); // Notify user
  _HubPortResetSetIdleServicePorts(hub);
}

/*********************************************************************
*
*       _HubProcessPortResetSetAddress
*
*  Function description:
*   An addressed USB device is enumerated on a hub port. If a parent device
*   is removed (this can be an root hub or hub) the State is always
*   USBH_HUB_PORTRESET_REMOVED. An already allocated device on this port is deleted.
*/
static void _HubProcessPortResetSetAddress(void * usbHub) {
  USBH_HUB_PORT   * enum_port;
  USB_HUB         * hub;
  USBH_URB        * urb;
  USBH_STATUS       status;
  USBH_HOST_CONTROLLER * host;

  hub       = (USB_HUB *)usbHub;
  USBH_ASSERT_MAGIC(hub, USB_HUB);
  host      =  hub->pHubDevice->pHostController;
  USBH_ASSERT_MAGIC(host, USBH_HOST_CONTROLLER);
  enum_port =  hub->EnumPort;
  USBH_ASSERT_PTR(enum_port);
  urb       = &hub->EnumUrb;
  // 1. Check hub device State
  // 2. Check port State during reset
  if (hub->pHubDevice->State < DEV_STATE_WORKING) {
    USBH_WARN((USBH_MTYPE_HUB, "HUB:  _HubProcessPortResetSetAddress: hub device State is not working, stop"));
    hub->PortResetEnumState = USBH_HUB_PORTRESET_REMOVED;
  } else {
    if (hub->PortResetEnumState >= USBH_HUB_PORTRESET_START) {           // Check physically port status
      if (enum_port->PortStatus & PORT_STATUS_CONNECT) {
        if (hub->PortResetEnumState >= USBH_HUB_PORTRESET_SET_ADDRESS) {
          if (!(enum_port->PortStatus &PORT_STATUS_ENABLED)) {
            USBH_BD_SetPortState(enum_port, PORT_RESTART);               // Port disabled after State port reset
            // Tricky: No port enumeration notification because _HubPortResetDisablePort calls _HubProcessPortResetSetAddress
            //         again with new enum State USBH_HUB_PORTRESET_DISABLE_PORT the port State is not changed!
            _HubPortResetDisablePort(hub);
            return;
          }
        }
      } else {
        // Port is not connected, stop port enumeration
        USBH_WARN((USBH_MTYPE_HUB, "HUB: _HubProcessPortResetSetAddress: port State not connect during port reset!"));
        _HubPortResetError(hub, USBH_STATUS_PORT);
        return;
      }
    }
  }
  USBH_LOG((USBH_MTYPE_HUB, "HUB: _HubProcessPortResetSetAddress: %s hubs ref.ct: %ld", USBH_HubPortResetState2Str(hub->PortResetEnumState), hub->pHubDevice->RefCount));
  switch (hub->PortResetEnumState) {
  case USBH_HUB_PORTRESET_START:
    hub->PortResetEnumState = USBH_HUB_PORTRESET_WAIT_RESTART;                  // Wait after connect
    USBH_URB_SubStateWait(&hub->PortResetSubState, WAIT_AFTER_CONNECT, NULL);
    break;
  case USBH_HUB_PORTRESET_RESTART:
    hub->PortResetEnumState = USBH_HUB_PORTRESET_WAIT_RESTART;
    USBH_URB_SubStateWait(&hub->PortResetSubState, DELAY_FOR_REENUM, NULL);
    break;
  case USBH_HUB_PORTRESET_WAIT_RESTART:
    USBH_BD_SetPortState(enum_port, PORT_RESET);                                // Reset the port
    _HubPrepareSetFeatureReq(urb, HDC_SELECTOR_PORT_RESET, enum_port->HubPortNumber);
    enum_port->PortStatusShadow = 0;
    hub->PortResetEnumState     = USBH_HUB_PORTRESET_RES;                       // Next State
    status                      = USBH_URB_SubStateSubmitRequest(&hub->PortResetSubState, urb, DEFAULT_SETUP_TIMEOUT, NULL);
    if (status != USBH_STATUS_PENDING) {
      USBH_WARN((USBH_MTYPE_HUB, "HUB:  Ext.Hub Port Reset:: USBH_URB_SubStateSubmitRequest: USBH_HUB_PORTRESET_WAIT_RESTART Set port reset:%08x", urb->Header.Status));
      _HubPortResetError(hub, status);                                      // Error submitting, disable the port is not available
    }
    break;
  case USBH_HUB_PORTRESET_RES:
    if (urb->Header.Status != USBH_STATUS_SUCCESS) {                       // Check urb
      USBH_WARN((USBH_MTYPE_HUB, "HUB:  Ext.Hub Port Reset:: USBH_HUB_PORTRESET_RES urb st:%08x", urb->Header.Status)); // Hub request error
      _HubPortResetRestart(hub, urb->Header.Status);                        // Restart port
      break;
    }
    hub->PortResetEnumState = USBH_HUB_PORTRESET_IS_ENABLED;
    USBH_URB_SubStateWait(&hub->PortResetSubState, DEFAULT_RESET_TIMEOUT, NULL); // Wait after reset the port
    break;
  case USBH_HUB_PORTRESET_IS_ENABLED:
    if ((enum_port->PortStatusShadow &PORT_STATUS_ENABLED) && (enum_port->PortStatusShadow &PORT_STATUS_CONNECT)) {
      USBH_BD_SetPortState(enum_port, PORT_ENABLED);                            // Port is connected and enabled
      enum_port->PortSpeed = USBH_FULL_SPEED;
      if (enum_port->PortStatus & PORT_STATUS_LOW_SPEED) {
        enum_port->PortSpeed = USBH_LOW_SPEED;
      }
      if (enum_port->PortStatus & PORT_STATUS_HIGH_SPEED) {
        enum_port->PortSpeed = USBH_HIGH_SPEED;
      }
      hub->PortResetEnumState = USBH_HUB_PORTRESET_WAIT_RESET;                 // New State
    } else {
      USBH_WARN((USBH_MTYPE_HUB, "HUB: Ext.Hub Port Reset:: device disappears during reset"));
      USBH_BD_SetPortState(enum_port, PORT_RESTART);
      _HubPortResetDisablePort(hub);                                       // Tricky: _HubProcessPortResetSetAddress is called again with USBH_HUB_PORTRESET_DISABLE_PORT
      break;
    }
  case USBH_HUB_PORTRESET_WAIT_RESET: // Fall trough
    hub->PortResetEnumState = USBH_HUB_PORTRESET_SET_ADDRESS;                  // New State
    USBH_URB_SubStateWait(&hub->PortResetSubState, WAIT_AFTER_RESET, NULL);     // Wait after reset the port
    break;
  case USBH_HUB_PORTRESET_SET_ADDRESS:
    {
      USB_DEVICE * enum_device;
      enum_device = USBH_CreateNewUsbDevice(host);
      if (enum_device == NULL) {                                            // Set enum device pointer
        USBH_WARN((USBH_MTYPE_HUB, "HUB: Ext.Hub Port Reset:: USBH_CreateNewUsbDevice fails -> port error!"));
        USBH_BD_SetPortState(enum_port, PORT_ERROR);
        _HubPortResetDisablePort(hub);                                       // Tricky: _HubProcessPortResetSetAddress is called again with USBH_HUB_PORTRESET_DISABLE_PORT
      } else {
        hub->EnumDevice                 = enum_device;
        enum_device->DeviceSpeed        = enum_port->PortSpeed;
        enum_device->UsbAddress         = USBH_BD_GetUsbAddress(host);
        enum_device->pParentPort         = enum_port;
        enum_device->ConfigurationIndex = enum_port->ConfigurationIndex;
        _HubPrepareStandardOutRequest(urb, USB_REQ_SET_ADDRESS, enum_device->UsbAddress, 0); // prepare the set address request
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
          USBH_ASSERT0;
        }
        hub->PortResetEnumState = USBH_HUB_PORTRESET_WAIT_SET_ADDRESS;           // new State
        status                  = USBH_URB_SubStateSubmitRequest(&hub->PortResetControlUrbSubState, urb, DEFAULT_SETUP_TIMEOUT, NULL);
        if (status != USBH_STATUS_PENDING) {
          USBH_WARN((USBH_MTYPE_HUB, "HUB: FATAL Ext.Hub Port Reset:: USBH_URB_SubStateSubmitRequest: failed: st: %08x", urb->Header.Status));
          USBH_BD_SetPortState(enum_port, PORT_ERROR);
          _HubPortResetDisablePort(hub); // Tricky: _HubProcessPortResetSetAddress is called again with USBH_HUB_PORTRESET_DISABLE_PORT
        }
      }
    }
    break;
  case USBH_HUB_PORTRESET_WAIT_SET_ADDRESS:
    USBH_ASSERT_MAGIC(hub->EnumDevice, USB_DEVICE);
    if (urb->Header.Status != USBH_STATUS_SUCCESS) {
      USBH_WARN((USBH_MTYPE_HUB, "HUB:  Ext.Hub Port Reset:: USBH_HUB_PORTRESET_WAIT_SET_ADDRESS urb st:%08x", urb->Header.Status));
      USBH_BD_SetPortState(enum_port, PORT_RESTART);
      _HubPortResetDisablePort(hub); // Tricky: _HubProcessPortResetSetAddress is called again with USBH_HUB_PORTRESET_DISABLE_PORT
      break;
    }
    hub->PortResetEnumState = USBH_HUB_PORTRESET_START_DEVICE_ENUM;             // New State
    USBH_URB_SubStateWait(&hub->PortResetSubState, WAIT_AFTER_SETADDRESS, NULL);
    break;
  case USBH_HUB_PORTRESET_START_DEVICE_ENUM:
    {
      USB_DEVICE * dev;
      USBH_ASSERT_MAGIC(hub->EnumDevice, USB_DEVICE);
      dev                           = hub->EnumDevice;          // Save and release enum device pointer
      hub->EnumDevice               = NULL;
      enum_port->ConfigurationIndex = 0;
      // 1.The Port device pointer pints to the connected device
      // 2.Start the device enumeration process
      // 3.Release the port enumeration and wait for connecting other ports!
      //   at this point the port reset enumeration State is PORT_ENABLED!
      USBH_StartEnumeration(dev, USBH_CreateInterfaces, dev);
      _HubPortResetSetIdleServicePorts(hub);
    }
    break;
  case USBH_HUB_PORTRESET_DISABLE_PORT:  // The port is disabled restart or stop port reset enumeration

#if (USBH_DEBUG > 1)
    if (urb->Header.Status != USBH_STATUS_SUCCESS) {
      USBH_WARN((USBH_MTYPE_HUB, "HUB:  Ext.Hub Port Reset:: USBH_HUB_PORTRESET_DISABLE_PORT urb st:%08x", urb->Header.Status));
    }
#endif

    if (enum_port->PortState == PORT_RESTART) {
      _HubPortResetRestart(hub, USBH_STATUS_PORT);
    } else {
      _HubPortResetError(hub, USBH_STATUS_PORT);
    }
    break;
  case USBH_HUB_PORTRESET_REMOVED:
    _HubDeviceResetRemove(hub);
    break;
  default:
    USBH_ASSERT0;
  }
}


/*********************************************************************
*
*       _InitHub
*
*  Function description:
*    Initializes a hub object. The link pointer in the hub and the device object are set!
*  Parameters:
*    pHub: Pointer to an uninitialized hub object
*/
static USBH_STATUS _InitHub(USB_HUB * pHub, USB_DEVICE * pDev) {
  USBH_STATUS Status;
  USBH_LOG((USBH_MTYPE_HUB, "HUB: _InitHub!"));
  USBH_ASSERT_PTR(pHub);
  USBH_ASSERT_PTR(pDev);
  USBH_ZERO_MEMORY(pHub, sizeof(USB_HUB));

  IFDBG(pHub->Magic = USB_HUB_MAGIC);
  USBH_DLIST_Init(&pHub->PortList);
  pHub->pHubDevice = pDev;
  Status          = USBH_URB_SubStateInit(&pHub->EnumSubState, pDev->pHostController, &pDev->DefaultEp.hEP, _ProcessEnumHub, pHub);
  if (Status) {
    USBH_WARN((USBH_MTYPE_HUB, "HUB:  _InitHub: USBH_URB_SubStateInit pHub->EnumSubState !"));
    goto Exit;
  }
  Status          = USBH_URB_SubStateInit(&pHub->NotifySubState, pDev->pHostController, &pDev->DefaultEp.hEP, _ProcessHubNotification, pHub);
  if (Status) {
    USBH_WARN((USBH_MTYPE_HUB, "HUB:  _InitHub: USBH_URB_SubStateInit pHub->NotifySubState!"));
    goto Exit;
  }
  Status          = USBH_URB_SubStateInit(&pHub->PortResetSubState, pDev->pHostController, &pDev->DefaultEp.hEP, _HubProcessPortResetSetAddress, pHub);
  if (Status) {
    USBH_WARN((USBH_MTYPE_HUB, "HUB:  _InitHub: USBH_URB_SubStateInit PortResetSubState!"));
    goto Exit;
  }
  Status          = USBH_URB_SubStateInit(&pHub->PortResetControlUrbSubState, pDev->pHostController, &pHub->PortResetEp0Handle, _HubProcessPortResetSetAddress, pHub);
  if (Status) {
    USBH_WARN((USBH_MTYPE_HUB, "HUB:  _InitHub: USBH_URB_SubStateInit PortResetEnumState!"));
    goto Exit;
  }
Exit:
  return Status;
}

/*********************************************************************
*
*       USBH_BD_AllocInitUsbHub
*
*  Function description:
*    Hub device allocation and initialization.
*    The link pointer in the hub and the device object are set!
*/
USB_HUB * USBH_BD_AllocInitUsbHub(USB_DEVICE * pDev) {
  USB_HUB * hub;

  USBH_ASSERT_MAGIC(pDev, USB_DEVICE);
  USBH_LOG((USBH_MTYPE_HUB, "USBH_BD_AllocInitUsbHub!"));
  hub = (USB_HUB *)USBH_Malloc(sizeof(USB_HUB));
  if (NULL == hub) {
    USBH_WARN((USBH_MTYPE_HUB, "HUB:  USBH_BD_AllocInitUsbHub: USBH_malloc!"));
    goto exit;
  }
  if (USBH_STATUS_SUCCESS != _InitHub(hub, pDev)) {
    USBH_WARN((USBH_MTYPE_HUB, "HUB:  USBH_BD_AllocInitUsbHub: _InitHub!"));
    USBH_Free(hub);
    hub = NULL;
  }
  exit:
  return hub;
}

/*********************************************************************
*
*       _HubRemoveAllPorts
*
*  Function description:
*/
static void _HubRemoveAllPorts(USB_HUB * pHub) {
  USBH_DLIST    * pEntry;
  USBH_HUB_PORT * pHubPort;
  USBH_LOG((USBH_MTYPE_HUB, "HUB: _HubRemoveAllPorts!"));
  while (!USBH_DLIST_IsEmpty(&pHub->PortList)) {
    USBH_DLIST_RemoveHead(&pHub->PortList, &pEntry);
    pHubPort = GET_HUB_PORT_PTR(pEntry);
    USBH_BD_DeleteHubPort(pHubPort);
  }
}

/*********************************************************************
*
*       _ServiceHubPorts
*
*  Function description:
*/
static USBH_BOOL _ServiceHubPorts(USB_HUB * pHub) {
  USBH_HUB_PORT * pHubPort;
  USBH_DLIST    * pList;
  USBH_ASSERT_MAGIC(pHub, USB_HUB);
  if (pHub->PortResetEnumState != USBH_HUB_PORTRESET_IDLE || pHub->pHubDevice->State < DEV_STATE_WORKING) {
    return FALSE;                                          // Enumeration is active or device is not in working State, do nothing now, we are called later
  }
  pList = USBH_DLIST_GetNext(&pHub->PortList);                        // Run over all ports, to see if a port needs a reset
  while (pList != &pHub->PortList) {
    pHubPort = GET_HUB_PORT_PTR(pList);
    USBH_ASSERT_MAGIC(pHubPort, USBH_HUB_PORT);
    pList        = USBH_DLIST_GetNext(pList);
    if (pHubPort->PortState == PORT_RESTART || pHubPort->PortState == PORT_CONNECTED) {
      pHubPort->RetryCounter++;
      if (pHubPort->RetryCounter > RESET_RETRY_COUNTER) {
        USBH_WARN((USBH_MTYPE_HUB, "HUB: _ServiceHubPorts: max. port retries -> PORT_ERROR!"));
        USBH_BD_SetPortState(pHubPort, PORT_ERROR);
        UbdSetEnumErrorNotificationHubPortReset(pHubPort, pHub->PortResetEnumState, USBH_STATUS_ERROR);
      } else {
        if (pHubPort->PortState == PORT_RESTART) {         // On success and not active root pHub enumeration State machine
          pHub->PortResetEnumState = USBH_HUB_PORTRESET_RESTART; // Schedule a delayed restart
        } else {
          pHub->PortResetEnumState = USBH_HUB_PORTRESET_START;
        }
        INC_REF(pHub->pHubDevice);                           // Ref.Ct=2
        pHub->EnumPort = pHubPort;
        HC_SET_ACTIVE_PORTRESET(pHub->pHubDevice->pHostController, pHubPort);
        _HubProcessPortResetSetAddress(pHub);
        return TRUE;
      }
    }
  }
  return FALSE;
}


/*********************************************************************
*
*       USBH_BD_DeleteHub
*
*  Function description:
*/
void USBH_BD_DeleteHub(USB_HUB * pHub) {
  USBH_LOG((USBH_MTYPE_HUB, "USBH_BD_DeleteHub!"));
  USBH_ASSERT_PTR(pHub);
  if (pHub->InterruptTransferBuffer != NULL) {
    USBH_URB_BufferFreeTransferBuffer(pHub->InterruptTransferBuffer);
    pHub->InterruptTransferBuffer = NULL;
  }
  _HubRemoveAllPorts(pHub);
  // Releases resources of USBH_URB_SubStateInit
  USBH_URB_SubStateExit(&pHub->EnumSubState);
  USBH_URB_SubStateExit(&pHub->NotifySubState);
  USBH_URB_SubStateExit(&pHub->PortResetSubState);
  USBH_URB_SubStateExit(&pHub->PortResetControlUrbSubState);
  IFDBG(pHub->Magic = 0);
  USBH_Free(pHub);
}

/*********************************************************************
*
*       _HubPrepareClearFeatureEndpointStall
*
*  Function description:
*/
void _HubPrepareClearFeatureEndpointStall(USBH_URB * pUrb, U8 Endpoint);
void _HubPrepareClearFeatureEndpointStall(USBH_URB * pUrb, U8 Endpoint) {
  USBH_ZERO_MEMORY(pUrb, sizeof(USBH_URB));
  pUrb->Header.Function                      = USBH_FUNCTION_CONTROL_REQUEST; // Set clear feature endpoint stall request
  pUrb->Request.ControlRequest.Setup.Type    = USB_ENDPOINT_RECIPIENT;       // STD, OUT, endpoint
  pUrb->Request.ControlRequest.Setup.Request = USB_REQ_CLEAR_FEATURE;
  pUrb->Request.ControlRequest.Setup.Value   = USB_FEATURE_STALL;
  pUrb->Request.ControlRequest.Setup.Index   = (U16)Endpoint;
}

/*********************************************************************
*
*       USBH_BD_StartHub
*
*  Function description:
*/
void USBH_BD_StartHub(USB_HUB * pHub, POST_ENUM_FUNC * pfPostEnumFunc, void * pContext) {
  USBH_LOG((USBH_MTYPE_HUB, "HUB: USBH_BD_StartHub!"));
  USBH_ASSERT_MAGIC(pHub, USB_HUB);
  USBH_ASSERT(USBH_HUB_ENUM_IDLE     == pHub->EnumState);
  USBH_ASSERT(DEV_ENUM_INIT_HUB == pHub->pHubDevice->EnumState);
  USBH_DLIST_Init(&pHub->PortList);
  pHub->PortCount        = 0;
  pHub->PostEnumFunction = pfPostEnumFunc;
  pHub->PostEnumContext  = pContext;
  pHub->EnumState        = USBH_HUB_ENUM_START;
  _ProcessEnumHub(pHub);
}

/*********************************************************************
*
*       USBH_BD_HubBuildChildDeviceList
*
*  Function description:
*    Builds an device list of all devices that are connected to a parent
*    device inclusive the parent device. The first device in the list
*    is the parent device. The list ends if no pHub device on a port is found!
*
*  Return value:
*    Number of devices in the list inclusive the rootHubDevice!
*    0: rootHubDevice is no pHub device!
*/
int USBH_BD_HubBuildChildDeviceList(USB_DEVICE * pHubDevice, USBH_DLIST * pDevList) {
  int          ct;
  USB_DEVICE * pDev;
  USBH_DLIST      * pDevEntry;
  USBH_DLIST      * pPortEntry;
  USBH_HUB_PORT   * pPort;
  USBH_BOOL       Succ;
  USBH_BOOL       DevListChangeFlag;
  USB_HUB    * pHub;

  pDev = pHubDevice;
  USBH_ASSERT_MAGIC(pDev, USB_DEVICE);
  USBH_LOG((USBH_MTYPE_HUB, "USBH_BD_HubBuildChildDeviceList!"));
  if (NULL == pDev->pUsbHub) {
    USBH_WARN((USBH_MTYPE_HUB, "HUB:  USBH_BD_HubBuildChildDeviceList: param. hubDevice is not an pHub device!"));
    return 0;
  }
  pDev->TempFlag = FALSE; // Add the root device to the list
  ct            = 1;
  USBH_DLIST_InsertTail(pDevList, &pDev->TempEntry);
  for (; ;) {
    // Search until an pHub device is found where the tempFlag is FALSE end insert all child devices of this pHub,
    // then break and search again until no pHub device with tempFlag=FALSE is in the list
    Succ              = TRUE;
    DevListChangeFlag = FALSE;
    pDevEntry         = USBH_DLIST_GetNext(pDevList);
    while (pDevEntry != pDevList) {
      pDev = GET_USB_DEVICE_FROM_TEMP_ENTRY(pDevEntry);
      USBH_ASSERT_MAGIC(pDev, USB_DEVICE);
      if (NULL != pDev->pUsbHub) {
        pHub = pDev->pUsbHub;                           // Device is an pHub
        USBH_ASSERT_MAGIC(pHub, USB_HUB);
        if (!pDev->TempFlag) {
          pDev->TempFlag = TRUE;
          // No processed pHub device found!
          pPortEntry    = USBH_DLIST_GetNext(&pHub->PortList); // Add all devices of all pHub ports
          while (pPortEntry != &pHub->PortList) {
            pPort       = GET_HUB_PORT_PTR(pPortEntry);
            USBH_ASSERT_MAGIC(pPort, USBH_HUB_PORT);
            pPortEntry = USBH_DLIST_GetNext(pPortEntry);
            if (pPort->Device != NULL) {
              if (NULL != pPort->Device->pUsbHub) {    // A device is connected to this port
                pPort->Device->TempFlag = FALSE;      // USB pHub device must be checked
                Succ                   = FALSE;
              } else {
                pPort->Device->TempFlag = TRUE;
              }
              ct++;
              USBH_DLIST_InsertTail(pDevList, &pPort->Device->TempEntry);
              DevListChangeFlag = TRUE;
            }
          }
        }
      }
      if (DevListChangeFlag) {                       // Start again if the devList is chnaged
        break;
      }
      pDevEntry = USBH_DLIST_GetNext(pDevEntry);
    }
    if (Succ) {
      break;
    }
  }
  return ct;
}

/*********************************************************************
*
*       USBH_BD_ServiceAllHubs
*
*  Function description:
*/
void USBH_BD_ServiceAllHubs(USBH_HOST_CONTROLLER * pHostController) {
  USB_DEVICE * pDev;
  USBH_DLIST      * pDevEntry;

  USBH_ASSERT_MAGIC(pHostController, USBH_HOST_CONTROLLER);
  pDevEntry  = USBH_DLIST_GetNext(&pHostController->DeviceList);
  while (pDevEntry != &pHostController->DeviceList) {
    pDev = GET_USB_DEVICE_FROM_ENTRY(pDevEntry);
    USBH_ASSERT_MAGIC(pDev, USB_DEVICE);
    pDevEntry = USBH_DLIST_GetNext(pDevEntry);
    if (NULL != pDev->pUsbHub && pDev->State == DEV_STATE_WORKING) {
      if (_ServiceHubPorts(pDev->pUsbHub)) { // Device is an pHub device
        break;                            // If HubProcessDeviceReset() is called from _ServiceHubPorts
      }
    }
  }
}

/*********************************************************************
*
*       USBH_BD_NewHubPort
*
*  Function description:
*
*  Return value:
*    Return null on error
*/
USBH_HUB_PORT * USBH_BD_NewHubPort(void) {
  USBH_HUB_PORT * pHubPort;

  USBH_LOG((USBH_MTYPE_HUB, "HUB: USBH_BD_NewHubPort!"));
  pHubPort = (USBH_HUB_PORT *)USBH_Malloc(sizeof(USBH_HUB_PORT));
  if (pHubPort == NULL) {
    USBH_WARN((USBH_MTYPE_HUB, "HUB: USBH_BD_NewHubPort: USBH_malloc"));
    return NULL;
  }
  USBH_ZERO_MEMORY(pHubPort,sizeof(USBH_HUB_PORT));
  IFDBG(pHubPort->Magic = USBH_HUB_PORT_MAGIC);
  USBH_BD_SetPortState(pHubPort, PORT_REMOVED); // Assume the port is removed
  return pHubPort;
}

/*********************************************************************
*
*       USBH_BD_DeleteHubPort
*
*  Function description:
*/
void USBH_BD_DeleteHubPort(USBH_HUB_PORT * HubPort) {
  USBH_Free(HubPort);
}

/*********************************************************************
*
*       USBH_BD_SetPortState
*
*  Function description:
*/
void USBH_BD_SetPortState(USBH_HUB_PORT * pHubPort, PORT_STATE State) {
  USBH_ASSERT_MAGIC(pHubPort, USBH_HUB_PORT);
  USBH_LOG((USBH_MTYPE_HUB, "HUB: SET_PORT_STATE: ext.pHub.ptr: %p portnb: %d old: %s new: %s", pHubPort->ExtHub,(int)pHubPort->HubPortNumber,
                            USBH_PortState2Str(pHubPort->PortState), USBH_PortState2Str(State)));
  pHubPort->PortState = State;
}

/*********************************************************************
*
*       USBH_BD_HubGetPortByNumber
*
*  Function description:
*/
USBH_HUB_PORT * USBH_BD_HubGetPortByNumber(USB_HUB * Hub, U8 Number) {
  USBH_HUB_PORT * HubPort;
  USBH_DLIST    * e;
  USBH_ASSERT_MAGIC(Hub, USB_HUB);
  e = USBH_DLIST_GetNext(&Hub->PortList);
  while (e != &Hub->PortList) {
    HubPort = GET_HUB_PORT_PTR(e);
    USBH_ASSERT_MAGIC(HubPort, USBH_HUB_PORT);
    e = USBH_DLIST_GetNext(e);
    if (HubPort->HubPortNumber == Number) {
      return HubPort;
    }
  }
  return NULL;
}

/********************************* EOF ******************************/
