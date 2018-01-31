/*********************************************************************
*  
*   IAR PowerPac
*
*   (c) Copyright IAR Systems 2010.  All rights reserved.
*
**********************************************************************
----------------------------------------------------------------------
File        : USBH_PnPNotification.c
Purpose     : USB Bus Driver Core
              PNP notification object
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
*       UbdNewNotification
*
*  Function description:
*/
PNP_NOTIFICATION * UbdNewNotification(USBH_PNP_NOTIFICATION * pUbdNotification) {
  PNP_NOTIFICATION * pNotification;

  pNotification     = USBH_Malloc(sizeof(PNP_NOTIFICATION));
  if (NULL == pNotification) {
    USBH_WARN((USBH_MTYPE_PNP, "PNP Error:  NewNotification: USBH_malloc!"));
    return pNotification;
  }
  ZERO_MEMORY(pNotification, sizeof(PNP_NOTIFICATION));
#if (USBH_DEBUG > 1)
  pNotification->Magic = PNP_NOTIFICATION_MAGIC;
#endif
  USBH_MEMCPY(&pNotification->UbdNotification, pUbdNotification, sizeof(USBH_PNP_NOTIFICATION));
  return pNotification;
}

/*********************************************************************
*
*       UbdReleaseNotification
*
*  Function description:
*/
void UbdReleaseNotification(PNP_NOTIFICATION * pPnpNotification) {
  if (NULL == pPnpNotification) {
    USBH_WARN((USBH_MTYPE_PNP, "PNP Warning: ReleaseNotification: pPnpNotification is NULL!"));
    return;
  }
  T_ASSERT_MAGIC(pPnpNotification, PNP_NOTIFICATION);
  USBH_Free(pPnpNotification);
}

/*********************************************************************
*
*       UbdNotifyWrapperCallbackRoutine
*
*  Function description:
*    Instead direct call of the user PNP notification routine an timer
*    routine calls the user notification callback routines.
*    wrapper context is used to call the user notification routines
*    in the timer context.
*/
void UbdNotifyWrapperCallbackRoutine(void * pContext) {
  PDLIST                       entry;
  DELAYED_PNP_NOTIFY_CONTEXT * delayed_pnp_context;
  USBH_LOG((USBH_MTYPE_PNP, "PNP: UbdNotifyWrapperCallbackRoutine!"));
  UNUSED_PARAM(pContext);
  //
  // Search all entries in gUsbDriver.DelayedPnPNotificationList
  // and execute the notification routine.
  // Delete the entry from the list.
  //
  USBH_LOG((USBH_MTYPE_PNP, "PNP:  UbdNotifyWrapperCallbackRoutine: notify-ct: %d", gUsbDriver.DelayedPnPNotificationCount));
  while (!DlistEmpty(&gUsbDriver.DelayedPnPNotificationList)) { // Check all entries
    T_ASSERT(gUsbDriver.DelayedPnPNotificationCount);
    entry               = DlistGetNext(&gUsbDriver.DelayedPnPNotificationList);
    delayed_pnp_context = GET_DELAYED_PNP_NOTIFY_CONTEXT_FROM_ENTRY(entry);
    T_ASSERT_MAGIC(delayed_pnp_context, DELAYED_PNP_NOTIFY_CONTEXT);
    T_ASSERT_PTR  (delayed_pnp_context->NotifyCallback);
    DlistRemoveEntry(entry);                                    // Remove entry from the list
    gUsbDriver.DelayedPnPNotificationCount--;
    // Call the notification routine and release the list object
    USBH_LOG((USBH_MTYPE_PNP, "PNP:  UbdNotifyWrapperCallbackRoutine notification for interface ID: %d!",delayed_pnp_context->Id));
    delayed_pnp_context->NotifyCallback(delayed_pnp_context->Context, delayed_pnp_context->Event, delayed_pnp_context->Id);
    USBH_Free(delayed_pnp_context);
  }
}

/*********************************************************************
*
*       UbdProcessDeviceNotifications
*
*  Function description:
*    If this interface matches with the interface Mask of pPnpNotification
*    the Event notification function is called with the Event.
*
*  Parameters:
*    pPnpNotification  - Pointer to the notification
*    pDev              - Pointer to a device
*    Event            - Device is connected, device is removed!
*                       Normally one device at the time is changed
*/
void UbdProcessDeviceNotifications(PNP_NOTIFICATION * pPnpNotification, USB_DEVICE * pDev, USBH_PNP_EVENT Event) {
  USB_INTERFACE          * iface;
  USBH_ON_PNP_EVENT_FUNC * NotifyCallback; // Notification function
  void                   * context;
  USBH_INTERFACE_MASK    * iMask;
  PDLIST                   entry;

  T_ASSERT_MAGIC(pPnpNotification, PNP_NOTIFICATION);
  T_ASSERT_MAGIC(pDev, USB_DEVICE);
  // Get notification values
  NotifyCallback =  pPnpNotification->UbdNotification.PnpNotification;
  context        =  pPnpNotification->UbdNotification.Context;
  iMask          = &pPnpNotification->UbdNotification.InterfaceMask;
  entry          = DlistGetNext(&pDev->UsbInterfaceList);
  while (entry != &pDev->UsbInterfaceList) { // Search in all device interfaces and notify every interface
    iface = GET_USB_INTERFACE_FROM_ENTRY(entry);
    T_ASSERT_MAGIC(iface, USB_INTERFACE);
    if (USBH_STATUS_SUCCESS == UbdCompareUsbInterface(iface, iMask, TRUE)) {
      DELAYED_PNP_NOTIFY_CONTEXT * delayed_pnp_context;
      // One of the devices interfaces does match
      // Old: NotifyCallback(context,Event,iface->InterfaceID);
      delayed_pnp_context = USBH_Malloc(sizeof(DELAYED_PNP_NOTIFY_CONTEXT));
      if (NULL == delayed_pnp_context) {
        USBH_WARN((USBH_MTYPE_PNP, "PNP Error: UbdProcessDeviceNotifications: USBH_malloc: no resources!"));
      } else { // Initialize the allocated delayed Pnp context
        USBH_LOG((USBH_MTYPE_PNP, "PNP: UbdProcessDeviceNotifications: NotifyCallback: USB addr:%u Interf.ID: %d Event:%d!", pDev->UsbAddress, iface->InterfaceID, Event));
#if (USBH_DEBUG > 1)
        delayed_pnp_context->Magic          = DELAYED_PNP_NOTIFY_CONTEXT_MAGIC;
#endif
        delayed_pnp_context->Context        = context;
        delayed_pnp_context->Event          = Event;
        delayed_pnp_context->Id             = iface->InterfaceID;
        delayed_pnp_context->NotifyCallback = NotifyCallback;
        DlistInsertTail(&gUsbDriver.DelayedPnPNotificationList, &delayed_pnp_context->ListEntry); // Insert entry at the tail of the list
        gUsbDriver.DelayedPnPNotificationCount++;
        T_ASSERT_PTR(gUsbDriver.DelayedPnPNotifyTimer);                                           // Restart delayed notification timer
        USBH_StartTimer(gUsbDriver.DelayedPnPNotifyTimer, 1);                                     // Use always the minimum timeout!
      }
    }
    entry = DlistGetNext(entry);
  }
}

/*********************************************************************
*
*       UbdProcessNotification
*
*  Function description:
*    If found an valid interface the USBH_AddDevice Event is sent.
*    if not found an valid interface nothing is sent.
*    This function is called the first time an notification is registered.
*    It searches in all host controller device lists.
*/
void UbdProcessNotification(PNP_NOTIFICATION * pPnpNotification) {
  PDLIST            entry, hostList, devEntry;
  HOST_CONTROLLER * host;
  USB_DEVICE      * uDev;
  // Notification function
  USBH_LOG((USBH_MTYPE_PNP, "PNP:  UbdProcessNotification!"));
  T_ASSERT_MAGIC(&gUsbDriver, USB_DRIVER);
  T_ASSERT_MAGIC(pPnpNotification, PNP_NOTIFICATION);
  hostList = &gUsbDriver.HostControllerList;
  entry    = DlistGetNext(hostList);
  while (entry != hostList) {
    // Search in all host controller
    host     = GET_HOST_CONTROLLER_FROM_ENTRY(entry);
    T_ASSERT_MAGIC(host, HOST_CONTROLLER);
    devEntry = DlistGetNext(&host->DeviceList);
    while (devEntry != &host->DeviceList) {
      uDev = GET_USB_DEVICE_FROM_ENTRY(devEntry);
      T_ASSERT_MAGIC(uDev, USB_DEVICE);
      UbdProcessDeviceNotifications(pPnpNotification, uDev, USBH_AddDevice);
      devEntry = DlistGetNext(devEntry);
    }
    entry = DlistGetNext(entry);
  }
}

/******************************* EOF ********************************/
