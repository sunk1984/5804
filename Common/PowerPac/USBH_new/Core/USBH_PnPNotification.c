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
*       USBH_PNP_NewNotification
*
*  Function description:
*/
USBH__PNP_NOTIFICATION * USBH_PNP_NewNotification(USBH_PNP_NOTIFICATION * pUbdNotification) {
  USBH__PNP_NOTIFICATION * pNotification;

  pNotification     = (USBH__PNP_NOTIFICATION *)USBH_Malloc(sizeof(USBH__PNP_NOTIFICATION));
  if (NULL == pNotification) {
    USBH_WARN((USBH_MTYPE_PNP, "PNP Error:  NewNotification: USBH_malloc!"));
    return pNotification;
  }
  USBH_ZERO_MEMORY(pNotification, sizeof(USBH__PNP_NOTIFICATION));
  IFDBG(pNotification->Magic = USBH__PNP_NOTIFICATION_MAGIC);
  USBH_MEMCPY(&pNotification->UbdNotification, pUbdNotification, sizeof(USBH_PNP_NOTIFICATION));
  return pNotification;
}

/*********************************************************************
*
*       USBH_PNP_ReleaseNotification
*
*  Function description:
*/
void USBH_PNP_ReleaseNotification(USBH__PNP_NOTIFICATION * pPnpNotification) {
  if (NULL == pPnpNotification) {
    USBH_WARN((USBH_MTYPE_PNP, "PNP Warning: ReleaseNotification: pPnpNotification is NULL!"));
    return;
  }
  USBH_ASSERT_MAGIC(pPnpNotification, USBH__PNP_NOTIFICATION);
  USBH_Free(pPnpNotification);
}

/*********************************************************************
*
*       USBH_PNP_NotifyWrapperCallbackRoutine
*
*  Function description:
*    Instead direct call of the user PNP notification routine an timer
*    routine calls the user notification callback routines.
*    wrapper pContext is used to call the user notification routines
*    in the timer pContext.
*/
void USBH_PNP_NotifyWrapperCallbackRoutine(void * pContext) {
  USBH_DLIST                      * pEntry;
  DELAYED_PNP_NOTIFY_CONTEXT * delayed_pnp_context;
  USBH_LOG((USBH_MTYPE_PNP, "PNP: USBH_PNP_NotifyWrapperCallbackRoutine!"));
  USBH_USE_PARA(pContext);
  //
  // Search all entries in gUsbDriver.DelayedPnPNotificationList
  // and execute the notification routine.
  // Delete the entry from the list.
  //
  USBH_LOG((USBH_MTYPE_PNP, "PNP:  USBH_PNP_NotifyWrapperCallbackRoutine: notify-ct: %d", USBH_Global.DriverInst.DelayedPnPNotificationCount));
  while (!USBH_DLIST_IsEmpty(&USBH_Global.DriverInst.DelayedPnPNotificationList)) { // Check all entries
    USBH_ASSERT(USBH_Global.DriverInst.DelayedPnPNotificationCount);
    pEntry               = USBH_DLIST_GetNext(&USBH_Global.DriverInst.DelayedPnPNotificationList);
    delayed_pnp_context = GET_DELAYED_PNP_NOTIFY_CONTEXT_FROM_ENTRY(pEntry);
    USBH_ASSERT_MAGIC(delayed_pnp_context, DELAYED_PNP_NOTIFY_CONTEXT);
    USBH_ASSERT_PTR  (delayed_pnp_context->NotifyCallback);
    USBH_DLIST_RemoveEntry(pEntry);                                    // Remove entry from the list
    USBH_Global.DriverInst.DelayedPnPNotificationCount--;
    // Call the notification routine and release the list object
    USBH_LOG((USBH_MTYPE_PNP, "PNP:  USBH_PNP_NotifyWrapperCallbackRoutine notification for interface ID: %d!",delayed_pnp_context->Id));
    delayed_pnp_context->NotifyCallback(delayed_pnp_context->pContext, delayed_pnp_context->Event, delayed_pnp_context->Id);
    USBH_Free(delayed_pnp_context);
  }
}

/*********************************************************************
*
*       USBH_PNP_ProcessDeviceNotifications
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
void USBH_PNP_ProcessDeviceNotifications(USBH__PNP_NOTIFICATION * pPnpNotification, USB_DEVICE * pDev, USBH_PNP_EVENT Event) {
  USB_INTERFACE              * iface;
  USBH_ON_PNP_EVENT_FUNC     * NotifyCallback; // Notification function
  void                       * pContext;
  USBH_INTERFACE_MASK        * iMask;
  USBH_DLIST                      * pEntry;
  DELAYED_PNP_NOTIFY_CONTEXT * pDelayedPnpContext;

  USBH_ASSERT_MAGIC(pPnpNotification, USBH__PNP_NOTIFICATION);
  USBH_ASSERT_MAGIC(pDev, USB_DEVICE);
  // Get notification values
  NotifyCallback =  pPnpNotification->UbdNotification.pfPnpNotification;
  pContext        =  pPnpNotification->UbdNotification.pContext;
  iMask          = &pPnpNotification->UbdNotification.InterfaceMask;
  pEntry          = USBH_DLIST_GetNext(&pDev->UsbInterfaceList);
  while (pEntry != &pDev->UsbInterfaceList) { // Search in all device interfaces and notify every interface
    iface = GET_USB_INTERFACE_FROM_ENTRY(pEntry);
    USBH_ASSERT_MAGIC(iface, USB_INTERFACE);
    if (USBH_STATUS_SUCCESS == USBH_BD_CompareUsbInterface(iface, iMask, TRUE)) {
      // One of the devices interfaces does match
      // Old: NotifyCallback(pContext,Event,iface->InterfaceId);
      pDelayedPnpContext = (DELAYED_PNP_NOTIFY_CONTEXT *)USBH_Malloc(sizeof(DELAYED_PNP_NOTIFY_CONTEXT));
      if (NULL == pDelayedPnpContext) {
        USBH_WARN((USBH_MTYPE_PNP, "PNP Error: USBH_PNP_ProcessDeviceNotifications: USBH_malloc: no resources!"));
      } else { // Initialize the allocated delayed Pnp pContext
        USBH_LOG((USBH_MTYPE_PNP, "PNP: USBH_PNP_ProcessDeviceNotifications: NotifyCallback: USB addr:%u Interf.ID: %d Event:%d!", pDev->UsbAddress, iface->InterfaceId, Event));
        IFDBG(pDelayedPnpContext->Magic    = DELAYED_PNP_NOTIFY_CONTEXT_MAGIC);
        pDelayedPnpContext->pContext       = pContext;
        pDelayedPnpContext->Event          = Event;
        pDelayedPnpContext->Id             = iface->InterfaceId;
        pDelayedPnpContext->NotifyCallback = NotifyCallback;
        USBH_DLIST_InsertTail(&USBH_Global.DriverInst.DelayedPnPNotificationList, &pDelayedPnpContext->ListEntry); // Insert entry at the tail of the list
        USBH_Global.DriverInst.DelayedPnPNotificationCount++;
        USBH_ASSERT_PTR(USBH_Global.DriverInst.DelayedPnPNotifyTimer);                                           // Restart delayed notification timer
        USBH_StartTimer(USBH_Global.DriverInst.DelayedPnPNotifyTimer, 1);                                     // Use always the minimum timeout!
      }
    }
    pEntry = USBH_DLIST_GetNext(pEntry);
  }
}

/*********************************************************************
*
*       USBH_PNP_ProcessNotification
*
*  Function description:
*    If found an valid interface the USBH_ADD_DEVICE Event is sent.
*    if not found an valid interface nothing is sent.
*    This function is called the first time an notification is registered.
*    It searches in all host controller device lists.
*/
void USBH_PNP_ProcessNotification(USBH__PNP_NOTIFICATION * pPnpNotification) {
  USBH_DLIST           * pEntry;
  USBH_DLIST           * pHostList;
  USBH_DLIST           * pDevEntry;
  USBH_HOST_CONTROLLER * pHost;
  USB_DEVICE      * pUSBDev;
  // Notification function
  USBH_LOG((USBH_MTYPE_PNP, "PNP:  USBH_PNP_ProcessNotification!"));
  USBH_ASSERT_MAGIC(&USBH_Global.DriverInst, USBH_HOST_DRIVER_INST);
  USBH_ASSERT_MAGIC(pPnpNotification, USBH__PNP_NOTIFICATION);
  pHostList = &USBH_Global.DriverInst.HostControllerList;
  pEntry    = USBH_DLIST_GetNext(pHostList);
  while (pEntry != pHostList) {
    // Search in all host controller
    pHost     = GET_HOST_CONTROLLER_FROM_ENTRY(pEntry);
    USBH_ASSERT_MAGIC(pHost, USBH_HOST_CONTROLLER);
    pDevEntry = USBH_DLIST_GetNext(&pHost->DeviceList);
    while (pDevEntry != &pHost->DeviceList) {
      pUSBDev = GET_USB_DEVICE_FROM_ENTRY(pDevEntry);
      USBH_ASSERT_MAGIC(pUSBDev, USB_DEVICE);
      USBH_PNP_ProcessDeviceNotifications(pPnpNotification, pUSBDev, USBH_ADD_DEVICE);
      pDevEntry = USBH_DLIST_GetNext(pDevEntry);
    }
    pEntry = USBH_DLIST_GetNext(pEntry);
  }
}

/******************************* EOF ********************************/
