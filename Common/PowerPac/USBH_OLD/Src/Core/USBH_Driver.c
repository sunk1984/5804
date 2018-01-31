/*********************************************************************
*  
*   IAR PowerPac
*
*   (c) Copyright IAR Systems 2010.  All rights reserved.
*
**********************************************************************
----------------------------------------------------------------------
File        : USBH_Driver.c
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
*       UbdProcessDevicePnpNotifications
*
*  Function description
*    Called if an device is successful added to the device list
*    or before it is removed from the device list.
*    If match an devices interface with one of the notification list
*    the notification function is called.
*
*  Params: Device: valid pointer
*          event:  device event
*/

void UbdProcessDevicePnpNotifications(USB_DEVICE * Device, USBH_PNP_EVENT event) {
  PDLIST             entry;
  PNP_NOTIFICATION * notification;

  // Get the driver object
  T_ASSERT_MAGIC(Device, USB_DEVICE);
  T_ASSERT_MAGIC(Device->HostController, HOST_CONTROLLER);
  T_ASSERT_MAGIC(Device->HostController->Driver, USB_DRIVER);
  entry = DlistGetNext(&gUsbDriver.NotificationList);
  while (entry != &gUsbDriver.NotificationList) { // Check all device interfaces
    notification = GET_PNP_NOTIFICATION_FROM_ENTRY(entry);
    T_ASSERT_MAGIC(notification, PNP_NOTIFICATION);
    UbdProcessDeviceNotifications(notification, Device, event);
    entry = DlistGetNext(entry);
  }
}

/*********************************************************************
*
*       UbdAddHostController
*
*  Function description
*    Adds a host controller to the HostControllerList.
*    The host controller object must be created with  NewHostController().
*/
void UbdAddHostController(HOST_CONTROLLER * HostController) {
  T_ASSERT_MAGIC(HostController, HOST_CONTROLLER);
  DlistInsertTail(&gUsbDriver.HostControllerList, &HostController->ListEntry);
  gUsbDriver.HostControllerCount++;
}

/*********************************************************************
*
*       UbdRemoveHostController
*
*  Function description
*    Removes a host controller from the HostControllerList
*/
void UbdRemoveHostController(HOST_CONTROLLER * HostController) {
  T_ASSERT_MAGIC(&gUsbDriver, USB_DRIVER);
  T_ASSERT(gUsbDriver.HostControllerCount);
  T_ASSERT_MAGIC(HostController, HOST_CONTROLLER);
  DlistRemoveEntry(&HostController->ListEntry);
  gUsbDriver.HostControllerCount--;
}

/*********************************************************************
*
*       UbdAddNotification
*
*  Function description
*    Add device notification, check if an pnp notification does match with this device
*/
void UbdAddNotification(USB_DEVICE * Device) {
  PDLIST             entry;
  PNP_NOTIFICATION * pnpNotification;
  entry            = DlistGetNext(&gUsbDriver.NotificationList);

  while (entry != &gUsbDriver.NotificationList) {
    pnpNotification = GET_PNP_NOTIFICATION_FROM_ENTRY(entry);
    T_ASSERT_MAGIC(pnpNotification, PNP_NOTIFICATION);
    UbdProcessDeviceNotifications(pnpNotification, Device, USBH_AddDevice);
    entry = DlistGetNext(entry);
  }
}

/*********************************************************************
*
*       UbdRemoveNotification
*
*  Function description
*    Remove device notification
*/
void UbdRemoveNotification(USB_DEVICE * Device) {
  PDLIST             entry;
  PNP_NOTIFICATION * pnpNotification;
  entry            = DlistGetNext(&gUsbDriver.NotificationList);

  while (entry != &gUsbDriver.NotificationList) {
    pnpNotification = GET_PNP_NOTIFICATION_FROM_ENTRY(entry);
    T_ASSERT_MAGIC(pnpNotification, PNP_NOTIFICATION);
    UbdProcessDeviceNotifications(pnpNotification, Device, USBH_RemoveDevice);
    entry = DlistGetNext(entry);
  }
}

/*********************************************************************
*
*       UbdGetNextInterfaceID
*
*  Function description
*/
USBH_INTERFACE_ID UbdGetNextInterfaceID(void) {
  USBH_INTERFACE_ID id;
  T_ASSERT_MAGIC(&gUsbDriver, USB_DRIVER);
  gUsbDriver.NextInterfaceID++;
  id = gUsbDriver.NextInterfaceID;
  USBH_LOG((USBH_MTYPE_UBD, "UBD: UbdGetNextInterfaceID: id: %u!", id));
  return id;
}

/*********************************************************************
*
*       UbdGetNextDeviceID
*
*  Function description
*/
USBH_DEVICE_ID UbdGetNextDeviceID(void) {
  USBH_DEVICE_ID id;
  T_ASSERT_MAGIC(&gUsbDriver, USB_DRIVER);
  gUsbDriver.NextDeviceID++;
  id = gUsbDriver.NextDeviceID;
  USBH_LOG((USBH_MTYPE_UBD, "UBD: UbdGetNextDeviceID: id: %u!", id));
  return id;
}

/*********************************************************************
*
*       UbdGetDeviceByID
*
*  Function description
*/
USB_DEVICE * UbdGetDeviceByID(USBH_DEVICE_ID DeviceID) {
  PDLIST            entry, hostList, devEntry;
  HOST_CONTROLLER * host;
  USB_DEVICE      * uDev;

  USBH_LOG((USBH_MTYPE_UBD, "UBD: UbdGetDeviceByID: DeviceID: %u!", DeviceID));
  hostList = &gUsbDriver.HostControllerList;
  entry    = DlistGetNext(hostList);
  while (entry != hostList) { // Search in all host controller
    host = GET_HOST_CONTROLLER_FROM_ENTRY(entry);
    T_ASSERT_MAGIC(host, HOST_CONTROLLER);
    devEntry = DlistGetNext(&host->DeviceList);
    while (devEntry != &host->DeviceList) { // Search in all devices
      uDev = GET_USB_DEVICE_FROM_ENTRY(devEntry);
      T_ASSERT_MAGIC(uDev, USB_DEVICE);
      if (uDev->DeviceID == DeviceID) {
        return uDev;
      }
      devEntry = DlistGetNext(devEntry);
    }
  }
  USBH_LOG((USBH_MTYPE_UBD, "UBD: UbdGetDeviceByID: No device  found!"));
  return NULL;
}

/*********************************************************************
*
*       GetInterfaceByID
*
*  Function description
*/
USB_INTERFACE * GetInterfaceByID(USBH_INTERFACE_ID InterfaceID) {
  PDLIST            entry, hostList, ifaceEntry, devEntry;
  HOST_CONTROLLER * host;
  USB_DEVICE      * uDev;
  USB_INTERFACE   * usbInterface;

  USBH_LOG((USBH_MTYPE_UBD, "UBD: UbdGetDeviceByID: InterfaceID: %u!", InterfaceID));
  hostList = &gUsbDriver.HostControllerList;
  entry    = DlistGetNext(hostList);
  while (entry != hostList) { // Search in all host controller
    host     = GET_HOST_CONTROLLER_FROM_ENTRY(entry);
    T_ASSERT_MAGIC(host, HOST_CONTROLLER);
    devEntry = DlistGetNext(&host->DeviceList);
    while (devEntry != &host->DeviceList) { // Search in all devices
      uDev       = GET_USB_DEVICE_FROM_ENTRY(devEntry);
      T_ASSERT_MAGIC(uDev, USB_DEVICE);
      ifaceEntry = DlistGetNext(&uDev->UsbInterfaceList);
      while (ifaceEntry != &uDev->UsbInterfaceList) { // Search in all interfaces
        usbInterface = GET_USB_INTERFACE_FROM_ENTRY(ifaceEntry);
        T_ASSERT_MAGIC(usbInterface, USB_INTERFACE);
        if (usbInterface->InterfaceID == InterfaceID) { // USB interface does match
          return usbInterface;
        }
        ifaceEntry = DlistGetNext(ifaceEntry);
      }
      devEntry = DlistGetNext(devEntry);
    }
    entry = DlistGetNext(entry); // Next host
  }
  USBH_LOG((USBH_MTYPE_UBD, "UBD: UbdGetDeviceByID: No interface found!"));
  return NULL;
}


/*********************************************************************
*
*       Driver object related bus driver functions
*
**********************************************************************
*/

/*********************************************************************
*
*       USBH_UBD_PreInit
*
*  Function description
*    This function is called before USBH_X_Config is called.
*    Since memory will be configured when USBH_X_Config is called
*    We remove everything that may allocate some memory.
*    Within USBH_X_Config, USBH_...Controller is called which require 
*    this information gUsbDriver initialization.
*    After USBH_X_Config is called, we then call USBH_UBD_Init
*    in order to do the allocation stuff.
*/
USBH_STATUS USBH_UBD_PreInit(void) {
  USBH_LOG((USBH_MTYPE_INIT, "INIT: USBH_PreInit!"));
  ZERO_STRUCT(gUsbDriver);
  DlistInit(&gUsbDriver.HostControllerList);
  DlistInit(&gUsbDriver.NotificationList);
  DlistInit(&gUsbDriver.EnumErrorNotificationList);
  DlistInit(&gUsbDriver.DelayedPnPNotificationList);
  return USBH_STATUS_SUCCESS;
}

/*********************************************************************
*
*       USBH_UBD_Init
*
*  Function description
*    Is called for the basic initialization of the USB Bus Driver.
*    Call this function one time during startup before any other function.
*    The USB Bus Driver initializes or allocates global resources.
*/
USBH_STATUS USBH_UBD_Init(void) {
  gUsbDriver.DelayedPnPNotifyTimer = USBH_AllocTimer(UbdNotifyWrapperCallbackRoutine, NULL);
  if (NULL == gUsbDriver.DelayedPnPNotifyTimer) {
    USBH_WARN((USBH_MTYPE_CORE, "Core: USBH_Init: USBH_AllocTimer: no resources!"));
    return USBH_STATUS_RESOURCES;
  }

#if (USBH_DEBUG > 1)
  gUsbDriver.Magic = USB_DRIVER_MAGIC;
#endif

  return USBH_STATUS_SUCCESS;
}

/*********************************************************************
*
*       USBH_Exit
*
*  Function description
*    Is called on exit of the USB Bus Driver. The USB Bus Driver may free global resources.
*/
void USBH_Exit(void) {
  // All registered notifications must be unregistered. All resources that the user allocates must be deallocate.
  T_ASSERT(0 == gUsbDriver.HostControllerCount);         // Check that USBH_RemoveHostControlle is called
  T_ASSERT(0 == gUsbDriver.NotificationCount);           // Check that USBH_UnregisterPnPNotification is called
  T_ASSERT(0 == gUsbDriver.DelayedPnPNotificationCount); // Check that the PnP notification routine returns
  T_ASSERT(0 == gUsbDriver.EnumErrorNotificationCount);  // Check call of USBH_UnregisterEnumErrorNotification
  USBH_LOG((USBH_MTYPE_INIT, "INIT: USBH_Exit!"));
  if (NULL != gUsbDriver.DelayedPnPNotifyTimer) {
    USBH_FreeTimer(gUsbDriver.DelayedPnPNotifyTimer);
  }
}

/*********************************************************************
*
*       USBH_AddHostController
*
*  Function description
*/
USBH_HC_HANDLE USBH_AddHostController(USB_HOST_ENTRY * HostEntry) {
  void            * context;
  HOST_CONTROLLER * host;
  USBH_STATUS       status;

  host = UbdCreateHostController(HostEntry);
  if (!host) {
    USBH_WARN((USBH_MTYPE_CORE, "Core:  USBH_AddHostController!"));
    return NULL;
  }
  UbdAddHostController(host);
  context = &host->RootHub;
  status  = HostEntry->HostInit(HostEntry->HcHandle, UbdRootHubNotification, context); // Initialize the host and enable all interrupts
  if (status) {
    USBH_WARN((USBH_MTYPE_CORE, "Core:  USBH_AddHostController: HostInit (0x%x) !", status));
    UbdRemoveHostController(host);
    UbdReleaseRootHub(&host->RootHub);
    // Delete the Host
    USBH_Free(host);
    host = NULL;
  }
  return (USBH_HC_HANDLE)host;
}

/*********************************************************************
*
*       USBH_RemoveHostController
*
*  Function description
*/
void USBH_RemoveHostController(USBH_HC_BD_HANDLE HcBdHandle) {
  HOST_CONTROLLER * Host       = (HOST_CONTROLLER *)HcBdHandle;
  USB_HOST_ENTRY  * HostEntry  = &Host->HostEntry;
  DLIST           * e;
  USB_DEVICE      * UsbDevice;

  T_ASSERT_MAGIC(Host, HOST_CONTROLLER);
  USBH_LOG((USBH_MTYPE_CORE, "Core: USBH_RemoveHostController!"));
  HC_INC_REF(Host);                                            // Local reference for this function
  Host->State = HC_REMOVED;
  HostEntry->SetHcState(HostEntry->HcHandle, USBH_HOST_RESET); // Stop the host controller
  UbdRemoveHostController(Host);                               // Remove it from the list of HC's
  e = DlistGetNext(&Host->DeviceList);                         // Mark all devices as removed
  while (e != &Host->DeviceList) {
    UsbDevice = GET_USB_DEVICE_FROM_ENTRY(e);
    T_ASSERT_MAGIC(UsbDevice, USB_DEVICE);
    e = DlistGetNext(e);
    UbdUdevMarkDeviceAsRemoved(UsbDevice);
  }
  if (Host->LowSpeedEndpoint != NULL) {
    HC_INC_REF(Host);
    HostEntry->ReleaseEndpoint(Host->LowSpeedEndpoint, UbdDefaultReleaseEpCompletion, Host);
    Host->LowSpeedEndpoint = NULL;
  }
  if (Host->FullSpeedEndpoint != NULL) {
    HC_INC_REF(Host);
    HostEntry->ReleaseEndpoint(Host->FullSpeedEndpoint, UbdDefaultReleaseEpCompletion, Host);
    Host->FullSpeedEndpoint = NULL;
  }
  if (Host->HighSpeedEndpoint != NULL) {
    HC_INC_REF(Host);
    HostEntry->ReleaseEndpoint(Host->HighSpeedEndpoint, UbdDefaultReleaseEpCompletion, Host);
    Host->HighSpeedEndpoint = NULL;
  }
  HC_DEC_REF(Host); // Delete the local reference
  HC_DEC_REF(Host); // Delete the initial reference
}

/*********************************************************************
*
*       USBH_RegisterPnPNotification
*
*  Function description
*    Register a notification function. If a valid handle is returned,
*    the function USBH_UnregisterPnPNotification must be called to release the notification.
*/
USBH_NOTIFICATION_HANDLE USBH_RegisterPnPNotification(USBH_PNP_NOTIFICATION * PnPNotification) {
  PNP_NOTIFICATION * notification;
  USBH_LOG((USBH_MTYPE_CORE, "Core: USBH_RegisterPnPNotification: VID: 0x%x PID: 0x%x interface: %u", PnPNotification->InterfaceMask.VID, PnPNotification->InterfaceMask.PID, PnPNotification->InterfaceMask.Interface));
  T_ASSERT_MAGIC(&gUsbDriver, USB_DRIVER);
  notification = UbdNewNotification(PnPNotification);
  if (notification == NULL) {
    USBH_WARN((USBH_MTYPE_CORE, "Core:  USBH_RegisterPnPNotification: NewNotification!"));
    return NULL;
  }
  DlistInsertTail(&gUsbDriver.NotificationList, &notification->ListEntry);
  gUsbDriver.NotificationCount++;
  // Always USBH_AddDevice is sent after the notification function is added if an interface is available
  UbdProcessNotification(notification);
  return notification;
}

/*********************************************************************
*
*       USBH_RegisterPnPNotification
*
*  Function description
*    Unregister the notification function
*/
void USBH_UnregisterPnPNotification(USBH_NOTIFICATION_HANDLE Handle) {
  PNP_NOTIFICATION * notification;
  USBH_LOG((USBH_MTYPE_CORE, "Core: USBH_UnregisterPnPNotification!"));
  notification = (PNP_NOTIFICATION *)Handle;
  T_ASSERT_MAGIC(notification, PNP_NOTIFICATION);
  DlistRemoveEntry(&notification->ListEntry);
  T_ASSERT(gUsbDriver.NotificationCount);
  gUsbDriver.NotificationCount--;
  UbdReleaseNotification(notification);
}

/********************************* EOF ******************************/
