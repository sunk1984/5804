/*********************************************************************
*  
*   IAR PowerPac
*
*   (c) Copyright IAR Systems 2010.  All rights reserved.
*
**********************************************************************
----------------------------------------------------------------------
File        : USBH_Enum.c
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
*       _CreateEnumErrorNotification
*
*  Function description
*    Notification callback function
*/
ENUM_ERROR_NOTIFICATION * _CreateEnumErrorNotification(void * Context, USBH_ON_ENUM_ERROR_FUNC * EnumErrorCallback);
ENUM_ERROR_NOTIFICATION * _CreateEnumErrorNotification(void * Context, USBH_ON_ENUM_ERROR_FUNC * EnumErrorCallback) {
  ENUM_ERROR_NOTIFICATION * pNotification;

  pNotification = USBH_MallocZeroed(sizeof(ENUM_ERROR_NOTIFICATION));
  if (NULL == pNotification) {
    USBH_WARN((USBH_MTYPE_PNP, "PNP notification: _CreateEnumErrorNotification: USBH_malloc!"));
    return pNotification;
  }
#if (USBH_DEBUG > 1)
  pNotification->Magic             = ENUM_ERROR_NOTIFICATION_MAGIC;
#endif
  pNotification->Context           = Context;
  pNotification->EnumErrorCallback = EnumErrorCallback;
  return pNotification;
}

/*********************************************************************
*
*       USBH_RegisterEnumErrorNotification
*
*  Function description
*    Register the port enumeration error notification function,
*    if an valid handle is returned, the function USBH_UnregisterEnumErrorNotification
*    must be called to release the notification.
*/
USBH_ENUM_ERROR_HANDLE USBH_RegisterEnumErrorNotification(void * Context, USBH_ON_ENUM_ERROR_FUNC * EnumErrorCallback) {
  ENUM_ERROR_NOTIFICATION * pNotification;

  USBH_LOG((USBH_MTYPE_PNP, "PNP notification: USBH_RegisterEnumErrorNotification context: 0%p",Context));
  T_ASSERT_MAGIC(&gUsbDriver, USB_DRIVER);
  T_ASSERT_PTR(EnumErrorCallback);
  //
  // Create new notification
  //
  pNotification = USBH_MallocZeroed(sizeof(ENUM_ERROR_NOTIFICATION));
  if (NULL == pNotification) {
    USBH_WARN((USBH_MTYPE_PNP, "PNP notification: USBH_RegisterEnumErrorNotification(): USBH_malloc!"));
    return pNotification;
  }
#if (USBH_DEBUG > 1)
  pNotification->Magic             = ENUM_ERROR_NOTIFICATION_MAGIC;
#endif
  pNotification->Context           = Context;
  pNotification->EnumErrorCallback = EnumErrorCallback;

  DlistInsertTail(&gUsbDriver.EnumErrorNotificationList, &pNotification->ListEntry);
  gUsbDriver.EnumErrorNotificationCount++;
  // Always USBH_AddDevice is sent after the notification function is added if an interface is available
  //UbdProcessEnumErrorNotification(notification);
  return pNotification;
}

/*********************************************************************
*
*       _ReleaseEnumErrorNotification
*
*  Function description
*/
static void _ReleaseEnumErrorNotification(ENUM_ERROR_NOTIFICATION * notification) {
  if (NULL == notification) {
    USBH_WARN((USBH_MTYPE_PNP, "PNP notification: _ReleaseEnumErrorNotification: Notification is NULL!"));
    return;
  }
  T_ASSERT_MAGIC(notification, ENUM_ERROR_NOTIFICATION);
  USBH_Free     (notification);
}

/*********************************************************************
*
*       USBH_UnregisterEnumErrorNotification
*
*  Function description
*    Unregister the notification function from the notification list.
*    Release the notification element!
*/
void USBH_UnregisterEnumErrorNotification(USBH_ENUM_ERROR_HANDLE Handle) {
  ENUM_ERROR_NOTIFICATION * notification;

  USBH_LOG((USBH_MTYPE_PNP, "PNP notification: USBH_UnregisterEnumErrorNotification!"));
  notification =  (ENUM_ERROR_NOTIFICATION *)Handle;
  T_ASSERT_MAGIC  ( notification, ENUM_ERROR_NOTIFICATION);
  DlistRemoveEntry(&notification->ListEntry);
  T_ASSERT(gUsbDriver.EnumErrorNotificationCount);
  gUsbDriver.EnumErrorNotificationCount--;
  _ReleaseEnumErrorNotification(notification);
}

/*********************************************************************
*
*       UbdFireEnumErrorNotification
*
*  Function description
*    Called from any device enumeration state machine if an error occurs
*/
void UbdFireEnumErrorNotification(USBH_ENUM_ERROR * enumError) {
  PDLIST                    entry, NotifyList;
  ENUM_ERROR_NOTIFICATION * enumErrorNotify;

  USBH_LOG((USBH_MTYPE_PNP, "PNP notification: UbdFireEnumErrorNotification!"));
  T_ASSERT_MAGIC(&gUsbDriver, USB_DRIVER);

  // Walk trough the driver enum error notify list and notify user from enum error!
  NotifyList = &gUsbDriver.EnumErrorNotificationList;
  entry      = DlistGetNext(NotifyList);

  while (entry != NotifyList) {
    enumErrorNotify = GET_ENUM_ERROR_NOTIFICATION_FROM_ENTRY(entry);
    T_ASSERT_MAGIC(enumErrorNotify, ENUM_ERROR_NOTIFICATION);
    enumErrorNotify->EnumErrorCallback(enumErrorNotify->Context, enumError);
    entry = DlistGetNext(entry);   // Next
  }
}

/*********************************************************************
*
*       UbdEnumErrorUpdateNotifyFlag
*
*  Function description
*    Update the notify flag
*/
static int UbdEnumErrorUpdateNotifyFlag(int flag, PORT_STATE state, U32 portStatus) {
  switch (state) {
    case PORT_ERROR:
    case PORT_REMOVED:
      flag |= USBH_ENUM_ERROR_STOP_ENUM_FLAG;
      break;
    case PORT_RESTART:
      flag |= USBH_ENUM_ERROR_RETRY_FLAG;
      break;
    default:
      USBH_WARN((USBH_MTYPE_PNP, "PNP notification: UbdEnumErrorUpdateNotifyFlag: unexpected port state: %d!", state));
      break;
  }
  if (portStatus & PORT_STATUS_CONNECT) {
    flag |= USBH_ENUM_ERROR_DISCONNECT_FLAG;
  }
  return flag;
}

/*********************************************************************
*
*       UbdSetEnumErrorNotificationRootPortReset
*
*  Function description
*/
void UbdSetEnumErrorNotificationRootPortReset(HUB_PORT * port, RH_PORTRESET_STATE state, USBH_STATUS status) {
  USBH_ENUM_ERROR enum_error;
  T_ASSERT_PTR(port);
  ZERO_STRUCT(enum_error);
  enum_error.Flags                    = USBH_ENUM_ERROR_ROOT_PORT_RESET;
  enum_error.ExtendedErrorInformation = state;
  if (!status) {
    enum_error.Status                 = USBH_STATUS_ERROR; // status not specified
  } else {
    enum_error.Status                 = status;
  }
  enum_error.PortNumber               = port->HubPortNumber;
  enum_error.Flags                    = UbdEnumErrorUpdateNotifyFlag(enum_error.Flags, port->PortState, port->PortStatus);
  UbdFireEnumErrorNotification(&enum_error);
}

/*********************************************************************
*
*       UbdSetEnumErrorNotificationProcessDeviceEnum
*
*  Function description
*/
void UbdSetEnumErrorNotificationProcessDeviceEnum(HUB_PORT * port, DEV_ENUM_STATE state, USBH_STATUS status, int hub_flag) {
  USBH_ENUM_ERROR enum_error;

  ZERO_STRUCT(enum_error);
  if (hub_flag) {
    enum_error.Flags = UDB_ENUM_ERROR_INIT_HUB;
  } else {
    enum_error.Flags = UDB_ENUM_ERROR_INIT_DEVICE;
  }
  enum_error.ExtendedErrorInformation = state;
  if (!status) {
    enum_error.Status = USBH_STATUS_ERROR;               // Status not specified
  } else {
    enum_error.Status = status;
  }
  enum_error.PortNumber = port->HubPortNumber;
  if (NULL == port->RootHub) {
    enum_error.Flags |= UDB_ENUM_ERROR_EXTHUBPORT_FLAG; // Port is not connected to the root hub
  }
  enum_error.Flags = UbdEnumErrorUpdateNotifyFlag(enum_error.Flags, port->PortState, port->PortStatus);
  UbdFireEnumErrorNotification(&enum_error);
}

#if USBH_EXTHUB_SUPPORT

  /*********************************************************************
  *
  *       UbdSetEnumErrorNotificationHubPortReset
  *
  *  Function description
  */
  void UbdSetEnumErrorNotificationHubPortReset(HUB_PORT * port, HUB_PORTRESET_STATE state, USBH_STATUS status) {
    USBH_ENUM_ERROR enum_error;
    T_ASSERT_PTR(port);
    ZERO_STRUCT(enum_error);

    enum_error.Flags                    = USBH_ENUM_ERROR_HUB_PORT_RESET | UDB_ENUM_ERROR_EXTHUBPORT_FLAG;
    enum_error.ExtendedErrorInformation = state;

    if (!status) {
      enum_error.Status = USBH_STATUS_ERROR; // Status not specified
    } else {
      enum_error.Status = status;
    }
    enum_error.PortNumber = port->HubPortNumber;
    enum_error.Flags      = UbdEnumErrorUpdateNotifyFlag(enum_error.Flags, port->PortState, port->PortStatus);
    UbdFireEnumErrorNotification(&enum_error);
  }

#endif //USBH_EXTHUB_SUPPORT

/*********************************************************************
*
*       USBH_RestartEnumError
*
*  Function description
*/
void USBH_RestartEnumError(void) {
  // First checks all root hub ports
  HUB_PORT        * port;
  PDLIST            hc_entry;
  PDLIST            port_entry;
  HOST_CONTROLLER * hc;
  int               succ;

  USBH_LOG((USBH_MTYPE_PNP, "PNP notification: INFO USBH_RestartEnumError!"));
  hc_entry       = DlistGetNext(&gUsbDriver.HostControllerList);           // For all hosts checks all ports
  while (hc_entry != &gUsbDriver.HostControllerList) {
    hc           = GET_HOST_CONTROLLER_FROM_ENTRY(hc_entry);
    T_ASSERT_MAGIC(hc, HOST_CONTROLLER);
    port_entry   = DlistGetNext(&hc->RootHub.PortList);                // Check all root hub ports
    succ         = FALSE;
    while (port_entry != &hc->RootHub.PortList) {
      port       = HUB_PORT_PTR(port_entry);
      T_ASSERT_MAGIC(port, HUB_PORT);
      port_entry = DlistGetNext(port_entry);
      if (port->PortStatus & PORT_STATUS_CONNECT && port->PortState == PORT_ERROR) {
        port->RetryCounter = 0;
        UbdSetPortState(port, PORT_RESTART);
        succ     = TRUE;
      }
    }
#if USBH_EXTHUB_SUPPORT // Checks all ports from all hubs
  {
    USB_DEVICE * dev;
    PDLIST       dev_entry;
    USB_HUB    * hub;

    dev_entry  = DlistGetNext(&hc->DeviceList); // Searches in all host controller devices
    while (dev_entry != &hc->DeviceList) {
      dev = GET_USB_DEVICE_FROM_ENTRY(dev_entry);
      T_ASSERT_MAGIC(dev, USB_DEVICE);
      dev_entry = DlistGetNext(dev_entry);
      if (NULL != dev->UsbHub && dev->State == DEV_STATE_WORKING) {
        hub = dev->UsbHub;                         // device is a hub device
        T_ASSERT_MAGIC(hub, USB_HUB);
        port_entry = DlistGetNext(&hub->PortList);
        while (port_entry != &hub->PortList) {
          port = HUB_PORT_PTR(port_entry);
          T_ASSERT_MAGIC(port, HUB_PORT);
          port_entry = DlistGetNext(port_entry);
          if (port->PortStatus & PORT_STATUS_CONNECT && port->PortState == PORT_ERROR) {
            port->RetryCounter = 0;
            UbdSetPortState(port, PORT_RESTART);
            succ = TRUE;
          }
        }
      }
    }
  }
#endif
    if (succ) {
      UbdHcServicePorts(hc); // Services all host controller ports
    }
    hc_entry = DlistGetNext(hc_entry);
  }
}

/********************************* EOF ******************************/
