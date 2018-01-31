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
*       USBH_BD_ProcessDevicePnpNotifications
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

void USBH_BD_ProcessDevicePnpNotifications(USB_DEVICE * pDevice, USBH_PNP_EVENT Event) {
  USBH_DLIST            * pEntry;
  USBH__PNP_NOTIFICATION * pNotification;

  // Get the driver object
  USBH_ASSERT_MAGIC(pDevice, USB_DEVICE);
  USBH_ASSERT_MAGIC(pDevice->pHostController, USBH_HOST_CONTROLLER);
  USBH_ASSERT_MAGIC(pDevice->pHostController->pInst, USBH_HOST_DRIVER_INST);
  pEntry = USBH_DLIST_GetNext(&USBH_Global.DriverInst.NotificationList);
  while (pEntry != &USBH_Global.DriverInst.NotificationList) { // Check all device interfaces
    pNotification = GET_PNP_NOTIFICATION_FROM_ENTRY(pEntry);
    USBH_ASSERT_MAGIC(pNotification, USBH__PNP_NOTIFICATION);
    USBH_PNP_ProcessDeviceNotifications(pNotification, pDevice, Event);
    pEntry = USBH_DLIST_GetNext(pEntry);
  }
}

/*********************************************************************
*
*       USBH_BD_AddHostController
*
*  Function description
*    Adds a host controller to the HostControllerList.
*    The host controller object must be created with  NewHostController().
*/
void USBH_BD_AddHostController(USBH_HOST_CONTROLLER * pHostController) {
  USBH_ASSERT_MAGIC(pHostController, USBH_HOST_CONTROLLER);
  USBH_DLIST_InsertTail(&USBH_Global.DriverInst.HostControllerList, &pHostController->ListEntry);
  USBH_Global.DriverInst.HostControllerCount++;
}

/*********************************************************************
*
*       USBH_BD_RemoveHostController
*
*  Function description
*    Removes a host controller from the HostControllerList
*/
void USBH_BD_RemoveHostController(USBH_HOST_CONTROLLER * pHostController) {
  USBH_ASSERT_MAGIC(&USBH_Global.DriverInst, USBH_HOST_DRIVER_INST);
  USBH_ASSERT(USBH_Global.DriverInst.HostControllerCount);
  USBH_ASSERT_MAGIC(pHostController, USBH_HOST_CONTROLLER);
  USBH_DLIST_RemoveEntry(&pHostController->ListEntry);
  USBH_Global.DriverInst.HostControllerCount--;
}

/*********************************************************************
*
*       USBH_BD_AddNotification
*
*  Function description
*    Add device notification, check if an PNP notification does match with this device
*/
void USBH_BD_AddNotification(USB_DEVICE * pDevice) {
  USBH_DLIST            * pEntry;
  USBH__PNP_NOTIFICATION * pPNPNotification;

  pEntry           = USBH_DLIST_GetNext(&USBH_Global.DriverInst.NotificationList);
  while (pEntry != &USBH_Global.DriverInst.NotificationList) {
    pPNPNotification = GET_PNP_NOTIFICATION_FROM_ENTRY(pEntry);
    USBH_ASSERT_MAGIC(pPNPNotification, USBH__PNP_NOTIFICATION);
    USBH_PNP_ProcessDeviceNotifications(pPNPNotification, pDevice, USBH_ADD_DEVICE);
    pEntry = USBH_DLIST_GetNext(pEntry);
  }
}

/*********************************************************************
*
*       USBH_BD_RemoveNotification
*
*  Function description
*    Remove device notification
*/
void USBH_BD_RemoveNotification(USB_DEVICE * pDevice) {
  USBH_DLIST            * pEntry;
  USBH__PNP_NOTIFICATION * pPNPNotification;

  pEntry            = USBH_DLIST_GetNext(&USBH_Global.DriverInst.NotificationList);
  while (pEntry != &USBH_Global.DriverInst.NotificationList) {
    pPNPNotification = GET_PNP_NOTIFICATION_FROM_ENTRY(pEntry);
    USBH_ASSERT_MAGIC(pPNPNotification, USBH__PNP_NOTIFICATION);
    USBH_PNP_ProcessDeviceNotifications(pPNPNotification, pDevice, USBH_REMOVE_DEVICE);
    pEntry = USBH_DLIST_GetNext(pEntry);
  }
}

/*********************************************************************
*
*       USBH_BD_GetNextInterfaceId
*
*  Function description
*/
USBH_INTERFACE_ID USBH_BD_GetNextInterfaceId(void) {
  USBH_INTERFACE_ID id;

  USBH_ASSERT_MAGIC(&USBH_Global.DriverInst, USBH_HOST_DRIVER_INST);
  USBH_Global.DriverInst.NextInterfaceId++;
  id = USBH_Global.DriverInst.NextInterfaceId;
  USBH_LOG((USBH_MTYPE_UBD, "UBD: USBH_BD_GetNextInterfaceId: id: %u!", id));
  return id;
}

/*********************************************************************
*
*       USBH_BD_GetNextDeviceId
*
*  Function description
*/
USBH_DEVICE_ID USBH_BD_GetNextDeviceId(void) {
  USBH_DEVICE_ID id;

  USBH_ASSERT_MAGIC(&USBH_Global.DriverInst, USBH_HOST_DRIVER_INST);
  USBH_Global.DriverInst.NextDeviceId++;
  id = USBH_Global.DriverInst.NextDeviceId;
  USBH_LOG((USBH_MTYPE_UBD, "UBD: USBH_BD_GetNextDeviceId: id: %u!", id));
  return id;
}

/*********************************************************************
*
*       USBH_BD_GetDeviceById
*
*  Function description
*/
USB_DEVICE * USBH_BD_GetDeviceById(USBH_DEVICE_ID DeviceId) {
  USBH_DLIST           * pEntry;
  USBH_DLIST           * pHostList;
  USBH_DLIST           * pDevEntry;
  USBH_HOST_CONTROLLER * pHost;
  USB_DEVICE      * pUSBDev;

  USBH_LOG((USBH_MTYPE_UBD, "UBD: USBH_BD_GetDeviceById: DeviceId: %u!", DeviceId));
  pHostList = &USBH_Global.DriverInst.HostControllerList;
  pEntry    = USBH_DLIST_GetNext(pHostList);
  while (pEntry != pHostList) { // Search in all host controller
    pHost = GET_HOST_CONTROLLER_FROM_ENTRY(pEntry);
    USBH_ASSERT_MAGIC(pHost, USBH_HOST_CONTROLLER);
    pDevEntry = USBH_DLIST_GetNext(&pHost->DeviceList);
    while (pDevEntry != &pHost->DeviceList) { // Search in all devices
      pUSBDev = GET_USB_DEVICE_FROM_ENTRY(pDevEntry);
      USBH_ASSERT_MAGIC(pUSBDev, USB_DEVICE);
      if (pUSBDev->DeviceId == DeviceId) {
        return pUSBDev;
      }
      pDevEntry = USBH_DLIST_GetNext(pDevEntry);
    }
  }
  USBH_LOG((USBH_MTYPE_UBD, "UBD: USBH_BD_GetDeviceById: No device  found!"));
  return NULL;
}

/*********************************************************************
*
*       USBH_BD_GetInterfaceById
*
*  Function description
*/
USB_INTERFACE * USBH_BD_GetInterfaceById(USBH_INTERFACE_ID InterfaceID) {
  USBH_DLIST           * pEntry;
  USBH_DLIST           * pHostList;
  USBH_DLIST           * pDevEntry;
  USBH_DLIST           * pInterfaceEntry;
  USBH_HOST_CONTROLLER * pHost;
  USB_DEVICE      * pUSBDev;
  USB_INTERFACE   * pUSBInterface;

  USBH_LOG((USBH_MTYPE_UBD, "UBD: USBH_BD_GetDeviceById: InterfaceID: %u!", InterfaceID));
  pHostList = &USBH_Global.DriverInst.HostControllerList;
  pEntry    = USBH_DLIST_GetNext(pHostList);
  while (pEntry != pHostList) { // Search in all host controller
    pHost     = GET_HOST_CONTROLLER_FROM_ENTRY(pEntry);
    USBH_ASSERT_MAGIC(pHost, USBH_HOST_CONTROLLER);
    pDevEntry = USBH_DLIST_GetNext(&pHost->DeviceList);
    while (pDevEntry != &pHost->DeviceList) { // Search in all devices
      pUSBDev       = GET_USB_DEVICE_FROM_ENTRY(pDevEntry);
      USBH_ASSERT_MAGIC(pUSBDev, USB_DEVICE);
      pInterfaceEntry = USBH_DLIST_GetNext(&pUSBDev->UsbInterfaceList);
      while (pInterfaceEntry != &pUSBDev->UsbInterfaceList) { // Search in all interfaces
        pUSBInterface = GET_USB_INTERFACE_FROM_ENTRY(pInterfaceEntry);
        USBH_ASSERT_MAGIC(pUSBInterface, USB_INTERFACE);
        if (pUSBInterface->InterfaceId == InterfaceID) { // USB interface does match
          return pUSBInterface;
        }
        pInterfaceEntry = USBH_DLIST_GetNext(pInterfaceEntry);
      }
      pDevEntry = USBH_DLIST_GetNext(pDevEntry);
    }
    pEntry = USBH_DLIST_GetNext(pEntry); // Next host
  }
  USBH_LOG((USBH_MTYPE_UBD, "UBD: USBH_BD_GetDeviceById: No interface found!"));
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
*       USBH_BD_PreInit
*
*  Function description
*    This function is called before USBH_X_Config is called.
*    Since memory will be configured when USBH_X_Config is called
*    We remove everything that may allocate some memory.
*    Within USBH_X_Config, USBH_...Controller is called which require
*    this information gUsbDriver initialization.
*    After USBH_X_Config is called, we then call USBH_BD_Init
*    in order to do the allocation stuff.
*/
USBH_STATUS USBH_BD_PreInit(void) {
  USBH_LOG((USBH_MTYPE_INIT, "INIT: USBH_PreInit!"));
  USBH_ZERO_STRUCT(USBH_Global.DriverInst);
  USBH_DLIST_Init(&USBH_Global.DriverInst.HostControllerList);
  USBH_DLIST_Init(&USBH_Global.DriverInst.NotificationList);
  USBH_DLIST_Init(&USBH_Global.DriverInst.EnumErrorNotificationList);
  USBH_DLIST_Init(&USBH_Global.DriverInst.DelayedPnPNotificationList);
  return USBH_STATUS_SUCCESS;
}

/*********************************************************************
*
*       USBH_BD_Init
*
*  Function description
*    Is called for the basic initialization of the USB Bus Driver.
*    Call this function one time during startup before any other function.
*    The USB Bus Driver initializes or allocates global resources.
*/
USBH_STATUS USBH_BD_Init(void) {
  USBH_Global.DriverInst.DelayedPnPNotifyTimer = USBH_AllocTimer(USBH_PNP_NotifyWrapperCallbackRoutine, NULL);
  if (NULL == USBH_Global.DriverInst.DelayedPnPNotifyTimer) {
    USBH_WARN((USBH_MTYPE_CORE, "Core: USBH_Init: USBH_AllocTimer: no resources!"));
    return USBH_STATUS_RESOURCES;
  }

  IFDBG(USBH_Global.DriverInst.Magic = USBH_HOST_DRIVER_INST_MAGIC);
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
  USBH_ASSERT(0 == USBH_Global.DriverInst.HostControllerCount);         // Check that USBH_RemoveHostControlle is called
  USBH_ASSERT(0 == USBH_Global.DriverInst.NotificationCount);           // Check that USBH_UnregisterPnPNotification is called
  USBH_ASSERT(0 == USBH_Global.DriverInst.DelayedPnPNotificationCount); // Check that the PnP notification routine returns
  USBH_ASSERT(0 == USBH_Global.DriverInst.EnumErrorNotificationCount);  // Check call of USBH_UnregisterEnumErrorNotification
  USBH_LOG((USBH_MTYPE_INIT, "INIT: USBH_Exit!"));
  if (NULL != USBH_Global.DriverInst.DelayedPnPNotifyTimer) {
    USBH_FreeTimer(USBH_Global.DriverInst.DelayedPnPNotifyTimer);
  }
}

/*********************************************************************
*
*       USBH_AddHostController
*
*  Function description
*/
USBH_HC_HANDLE USBH_AddHostController(USBH_HOST_DRIVER * pDriver, USBH_HC_HANDLE hHostController) {
  void            * pContext;
  USBH_HOST_CONTROLLER * pHost;
  USBH_STATUS       Status;

  pHost = USBH_CreateHostController(pDriver, hHostController);
  if (!pHost) {
    USBH_WARN((USBH_MTYPE_CORE, "Core:  USBH_AddHostController!"));
    return NULL;
  }
  USBH_BD_AddHostController(pHost);
  pContext = &pHost->RootHub;
  Status  = pDriver->pfHostInit(pHost->hHostController, USBH_ROOTHUB_OnNotification, pContext); // Initialize the host and enable all interrupts
  if (Status) {
    USBH_WARN((USBH_MTYPE_CORE, "Core:  USBH_AddHostController: pfHostInit (0x%x) !", Status));
    USBH_BD_RemoveHostController(pHost);
    USBH_ROOTHUB_Release(&pHost->RootHub);
    // Delete the Host
    USBH_Free(pHost);
    pHost = NULL;
  }
  return (USBH_HC_HANDLE)pHost;
}

/*********************************************************************
*
*       USBH_RemoveHostController
*
*  Function description
*/
void USBH_RemoveHostController(USBH_HC_BD_HANDLE HcBdHandle) {
  USBH_HOST_CONTROLLER  * pHostController = (USBH_HOST_CONTROLLER *)HcBdHandle;
  USBH_HOST_DRIVER * pDriver         = pHostController->pDriver;
  USBH_DLIST            * pList;
  USB_DEVICE       * pUsbDevice;

  USBH_ASSERT_MAGIC(pHostController, USBH_HOST_CONTROLLER);
  USBH_LOG((USBH_MTYPE_CORE, "Core: USBH_RemoveHostController!"));
  HC_INC_REF(pHostController);                                            // Local reference for this function
  pHostController->State = HC_REMOVED;
  pDriver->pfSetHcState(pHostController->hHostController, USBH_HOST_RESET); // Stop the host controller
  USBH_BD_RemoveHostController(pHostController);                               // Remove it from the list of HC's
  pList = USBH_DLIST_GetNext(&pHostController->DeviceList);                         // Mark all devices as removed
  while (pList != &pHostController->DeviceList) {
    pUsbDevice = GET_USB_DEVICE_FROM_ENTRY(pList);
    USBH_ASSERT_MAGIC(pUsbDevice, USB_DEVICE);
    pList = USBH_DLIST_GetNext(pList);
    USBH_MarkDeviceAsRemoved(pUsbDevice);
  }
  if (pHostController->LowSpeedEndpoint != NULL) {
    HC_INC_REF(pHostController);
    pDriver->pfReleaseEndpoint(pHostController->LowSpeedEndpoint, USBH_DefaultReleaseEpCompletion, pHostController);
    pHostController->LowSpeedEndpoint = NULL;
  }
  if (pHostController->FullSpeedEndpoint != NULL) {
    HC_INC_REF(pHostController);
    pDriver->pfReleaseEndpoint(pHostController->FullSpeedEndpoint, USBH_DefaultReleaseEpCompletion, pHostController);
    pHostController->FullSpeedEndpoint = NULL;
  }
  if (pHostController->HighSpeedEndpoint != NULL) {
    HC_INC_REF(pHostController);
    pDriver->pfReleaseEndpoint(pHostController->HighSpeedEndpoint, USBH_DefaultReleaseEpCompletion, pHostController);
    pHostController->HighSpeedEndpoint = NULL;
  }
  HC_DEC_REF(pHostController); // Delete the local reference
  HC_DEC_REF(pHostController); // Delete the initial reference
}

/*********************************************************************
*
*       USBH_RegisterPnPNotification
*
*  Function description
*    Register a notification function. If a valid handle is returned,
*    the function USBH_UnregisterPnPNotification must be called to release the notification.
*/
USBH_NOTIFICATION_HANDLE USBH_RegisterPnPNotification(USBH_PNP_NOTIFICATION * pPnPNotification) {
  USBH__PNP_NOTIFICATION * pNotification;

  USBH_LOG((USBH_MTYPE_CORE, "Core: USBH_RegisterPnPNotification: VendorId: 0x%x ProductId: 0x%x interface: %u", pPnPNotification->InterfaceMask.VendorId, pPnPNotification->InterfaceMask.ProductId, pPnPNotification->InterfaceMask.Interface));
  USBH_ASSERT_MAGIC(&USBH_Global.DriverInst, USBH_HOST_DRIVER_INST);
  pNotification = USBH_PNP_NewNotification(pPnPNotification);
  if (pNotification == NULL) {
    USBH_WARN((USBH_MTYPE_CORE, "Core:  USBH_RegisterPnPNotification: NewNotification!"));
    return NULL;
  }
  USBH_DLIST_InsertTail(&USBH_Global.DriverInst.NotificationList, &pNotification->ListEntry);
  USBH_Global.DriverInst.NotificationCount++;
  // Always USBH_ADD_DEVICE is sent after the notification function is added if an interface is available
  USBH_PNP_ProcessNotification(pNotification);
  return pNotification;
}

/*********************************************************************
*
*       USBH_RegisterPnPNotification
*
*  Function description
*    Unregister the notification function
*/
void USBH_UnregisterPnPNotification(USBH_NOTIFICATION_HANDLE Handle) {
  USBH__PNP_NOTIFICATION * pNotification;

  USBH_LOG((USBH_MTYPE_CORE, "Core: USBH_UnregisterPnPNotification!"));
  pNotification = (USBH__PNP_NOTIFICATION *)Handle;
  USBH_ASSERT_MAGIC(pNotification, USBH__PNP_NOTIFICATION);
  USBH_DLIST_RemoveEntry(&pNotification->ListEntry);
  USBH_ASSERT(USBH_Global.DriverInst.NotificationCount);
  USBH_Global.DriverInst.NotificationCount--;
  USBH_PNP_ReleaseNotification(pNotification);
}

/********************************* EOF ******************************/
