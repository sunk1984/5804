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
ENUM_ERROR_NOTIFICATION * _CreateEnumErrorNotification(void * pContext, USBH_ON_ENUM_ERROR_FUNC * pfOnEnumError);
ENUM_ERROR_NOTIFICATION * _CreateEnumErrorNotification(void * pContext, USBH_ON_ENUM_ERROR_FUNC * pfOnEnumError) {
  ENUM_ERROR_NOTIFICATION * pNotification;

  pNotification = (ENUM_ERROR_NOTIFICATION *)USBH_MallocZeroed(sizeof(ENUM_ERROR_NOTIFICATION));
  if (NULL == pNotification) {
    USBH_WARN((USBH_MTYPE_PNP, "PNP pNotification: _CreateEnumErrorNotification: USBH_malloc!"));
    return pNotification;
  }
  IFDBG(pNotification->Magic   = ENUM_ERROR_NOTIFICATION_MAGIC);
  pNotification->pContext      = pContext;
  pNotification->pfOnEnumError = pfOnEnumError;
  return pNotification;
}

/*********************************************************************
*
*       USBH_RegisterEnumErrorNotification
*
*  Function description
*    Register the port enumeration error pNotification function,
*    if an valid handle is returned, the function USBH_UnregisterEnumErrorNotification
*    must be called to release the pNotification.
*/
USBH_ENUM_ERROR_HANDLE USBH_RegisterEnumErrorNotification(void * pContext, USBH_ON_ENUM_ERROR_FUNC * pfOnEnumError) {
  ENUM_ERROR_NOTIFICATION * pNotification;

  USBH_LOG((USBH_MTYPE_PNP, "PNP pNotification: USBH_RegisterEnumErrorNotification context: 0%p",pContext));
  USBH_ASSERT_MAGIC(&USBH_Global.DriverInst, USBH_HOST_DRIVER_INST);
  USBH_ASSERT_PTR(pfOnEnumError);
  //
  // Create new pNotification
  //
  pNotification = (ENUM_ERROR_NOTIFICATION *)USBH_MallocZeroed(sizeof(ENUM_ERROR_NOTIFICATION));
  if (NULL == pNotification) {
    USBH_WARN((USBH_MTYPE_PNP, "PNP pNotification: USBH_RegisterEnumErrorNotification(): USBH_malloc!"));
    return pNotification;
  }
  IFDBG(pNotification->Magic       = ENUM_ERROR_NOTIFICATION_MAGIC);
  pNotification->pContext           = pContext;
  pNotification->pfOnEnumError = pfOnEnumError;

  USBH_DLIST_InsertTail(&USBH_Global.DriverInst.EnumErrorNotificationList, &pNotification->ListEntry);
  USBH_Global.DriverInst.EnumErrorNotificationCount++;
  // Always USBH_AddDevice is sent after the pNotification function is added if an interface is available
  //UbdProcessEnumErrorNotification(pNotification);
  return pNotification;
}

/*********************************************************************
*
*       _ReleaseEnumErrorNotification
*
*  Function description
*/
static void _ReleaseEnumErrorNotification(ENUM_ERROR_NOTIFICATION * pNotification) {
  if (NULL == pNotification) {
    USBH_WARN((USBH_MTYPE_PNP, "PNP pNotification: _ReleaseEnumErrorNotification: Notification is NULL!"));
    return;
  }
  USBH_ASSERT_MAGIC(pNotification, ENUM_ERROR_NOTIFICATION);
  USBH_Free     (pNotification);
}

/*********************************************************************
*
*       UbdEnumErrorUpdateNotifyFlag
*
*  Function description
*    Update the notify flag
*/
static int UbdEnumErrorUpdateNotifyFlag(int Flag, PORT_STATE State, U32 PortStatus) {
  switch (State) {
  case PORT_ERROR:
  case PORT_REMOVED:
    Flag |= USBH_ENUM_ERROR_STOP_ENUM_FLAG;
    break;
  case PORT_RESTART:
    Flag |= USBH_ENUM_ERROR_RETRY_FLAG;
    break;
  default:
    USBH_WARN((USBH_MTYPE_PNP, "PNP pNotification: UbdEnumErrorUpdateNotifyFlag: unexpected port state: %d!", State));
    break;
  }
  if (PortStatus & PORT_STATUS_CONNECT) {
    Flag |= USBH_ENUM_ERROR_DISCONNECT_FLAG;
  }
  return Flag;
}


/*********************************************************************
*
*       USBH_UnregisterEnumErrorNotification
*
*  Function description
*    Unregister the pNotification function from the pNotification list.
*    Release the pNotification element!
*/
void USBH_UnregisterEnumErrorNotification(USBH_ENUM_ERROR_HANDLE Handle) {
  ENUM_ERROR_NOTIFICATION * pNotification;

  USBH_LOG((USBH_MTYPE_PNP, "PNP pNotification: USBH_UnregisterEnumErrorNotification!"));
  pNotification =  (ENUM_ERROR_NOTIFICATION *)Handle;
  USBH_ASSERT_MAGIC  ( pNotification, ENUM_ERROR_NOTIFICATION);
  USBH_DLIST_RemoveEntry(&pNotification->ListEntry);
  USBH_ASSERT(USBH_Global.DriverInst.EnumErrorNotificationCount);
  USBH_Global.DriverInst.EnumErrorNotificationCount--;
  _ReleaseEnumErrorNotification(pNotification);
}

/*********************************************************************
*
*       UbdFireEnumErrorNotification
*
*  Function description
*    Called from any device enumeration state machine if an error occurs
*/
void UbdFireEnumErrorNotification(USBH_ENUM_ERROR * pEnumError) {
  USBH_DLIST                   * pEntry;
  USBH_DLIST                   * pNotifyList;
  ENUM_ERROR_NOTIFICATION * pEnumErrorNotify;

  USBH_LOG((USBH_MTYPE_PNP, "PNP pNotification: UbdFireEnumErrorNotification!"));
  USBH_ASSERT_MAGIC(&USBH_Global.DriverInst, USBH_HOST_DRIVER_INST);

  // Walk trough the driver enum error notify list and notify user from enum error!
  pNotifyList = &USBH_Global.DriverInst.EnumErrorNotificationList;
  pEntry      = USBH_DLIST_GetNext(pNotifyList);

  while (pEntry != pNotifyList) {
    pEnumErrorNotify = GET_ENUM_ERROR_NOTIFICATION_FROM_ENTRY(pEntry);
    USBH_ASSERT_MAGIC(pEnumErrorNotify, ENUM_ERROR_NOTIFICATION);
    pEnumErrorNotify->pfOnEnumError(pEnumErrorNotify->pContext, pEnumError);
    pEntry = USBH_DLIST_GetNext(pEntry);   // Next
  }
}

/*********************************************************************
*
*       UbdSetEnumErrorNotificationRootPortReset
*
*  Function description
*/
void UbdSetEnumErrorNotificationRootPortReset(USBH_HUB_PORT * pPort, USBH_ROOT_HUB_PORTRESET_STATE State, USBH_STATUS Status) {
  USBH_ENUM_ERROR EnumError;

  USBH_ASSERT_PTR(pPort);
  USBH_ZERO_STRUCT(EnumError);
  EnumError.Flags                    = USBH_ENUM_ERROR_ROOT_PORT_RESET;
  EnumError.ExtendedErrorInformation = State;
  if (!Status) {
    EnumError.Status                 = USBH_STATUS_ERROR; // status not specified
  } else {
    EnumError.Status                 = Status;
  }
  EnumError.PortNumber               = pPort->HubPortNumber;
  EnumError.Flags                    = UbdEnumErrorUpdateNotifyFlag(EnumError.Flags, pPort->PortState, pPort->PortStatus);
  UbdFireEnumErrorNotification(&EnumError);
}

/*********************************************************************
*
*       UbdSetEnumErrorNotificationProcessDeviceEnum
*
*  Function description
*/
void UbdSetEnumErrorNotificationProcessDeviceEnum(USBH_HUB_PORT * pPort, DEV_ENUM_STATE State, USBH_STATUS Status, int HubFlag) {
  USBH_ENUM_ERROR EnumError;

  USBH_ZERO_STRUCT(EnumError);
  if (HubFlag) {
    EnumError.Flags = UDB_ENUM_ERROR_INIT_HUB;
  } else {
    EnumError.Flags = UDB_ENUM_ERROR_INIT_DEVICE;
  }
  EnumError.ExtendedErrorInformation = State;
  if (!Status) {
    EnumError.Status = USBH_STATUS_ERROR;               // Status not specified
  } else {
    EnumError.Status = Status;
  }
  EnumError.PortNumber = pPort->HubPortNumber;
  if (NULL == pPort->RootHub) {
    EnumError.Flags |= UDB_ENUM_ERROR_EXTHUBPORT_FLAG; // Port is not connected to the root hub
  }
  EnumError.Flags = UbdEnumErrorUpdateNotifyFlag(EnumError.Flags, pPort->PortState, pPort->PortStatus);
  UbdFireEnumErrorNotification(&EnumError);
}

/*********************************************************************
*
*       UbdSetEnumErrorNotificationHubPortReset
*
*  Function description
*/
void UbdSetEnumErrorNotificationHubPortReset(USBH_HUB_PORT * pPort, USBH_HUB_PORTRESET_STATE State, USBH_STATUS Status) {
  USBH_ENUM_ERROR EnumError;

  USBH_ASSERT_PTR(pPort);
  USBH_ZERO_STRUCT(EnumError);
  EnumError.Flags                    = USBH_ENUM_ERROR_HUB_PORT_RESET | UDB_ENUM_ERROR_EXTHUBPORT_FLAG;
  EnumError.ExtendedErrorInformation = State;

  if (!Status) {
    EnumError.Status = USBH_STATUS_ERROR; // Status not specified
  } else {
    EnumError.Status = Status;
  }
  EnumError.PortNumber = pPort->HubPortNumber;
  EnumError.Flags      = UbdEnumErrorUpdateNotifyFlag(EnumError.Flags, pPort->PortState, pPort->PortStatus);
  UbdFireEnumErrorNotification(&EnumError);
}

/*********************************************************************
*
*       USBH_RestartEnumError
*
*  Function description
*/
void USBH_RestartEnumError(void) {
  // First checks all root hub ports
  USBH_HUB_PORT        * pPort;
  USBH_DLIST           * pHostControllerEntry;
  USBH_DLIST           * pPortEntry;
  USBH_HOST_CONTROLLER * pHostController;
  int               succ;

  USBH_LOG((USBH_MTYPE_PNP, "PNP pNotification: INFO USBH_RestartEnumError!"));
  pHostControllerEntry       = USBH_DLIST_GetNext(&USBH_Global.DriverInst.HostControllerList);           // For all hosts checks all ports
  while (pHostControllerEntry != &USBH_Global.DriverInst.HostControllerList) {
    pHostController           = GET_HOST_CONTROLLER_FROM_ENTRY(pHostControllerEntry);
    USBH_ASSERT_MAGIC(pHostController, USBH_HOST_CONTROLLER);
    pPortEntry   = USBH_DLIST_GetNext(&pHostController->RootHub.PortList);                // Check all root hub ports
    succ         = FALSE;
    while (pPortEntry != &pHostController->RootHub.PortList) {
      pPort       = GET_HUB_PORT_PTR(pPortEntry);
      USBH_ASSERT_MAGIC(pPort, USBH_HUB_PORT);
      pPortEntry = USBH_DLIST_GetNext(pPortEntry);
      if (pPort->PortStatus & PORT_STATUS_CONNECT && pPort->PortState == PORT_ERROR) {
        pPort->RetryCounter = 0;
        USBH_BD_SetPortState(pPort, PORT_RESTART);
        succ     = TRUE;
      }
    }
    if (USBH_Global.Config.SupportExternalHubs) {
      USB_DEVICE * pDev;
      USBH_DLIST      * pDevEntry;
      USB_HUB    * pHub;

      pDevEntry  = USBH_DLIST_GetNext(&pHostController->DeviceList); // Searches in all host controller devices
      while (pDevEntry != &pHostController->DeviceList) {
        pDev = GET_USB_DEVICE_FROM_ENTRY(pDevEntry);
        USBH_ASSERT_MAGIC(pDev, USB_DEVICE);
        pDevEntry = USBH_DLIST_GetNext(pDevEntry);
        if (NULL != pDev->pUsbHub && pDev->State == DEV_STATE_WORKING) {
          pHub = pDev->pUsbHub;                         // device is a hub device
          USBH_ASSERT_MAGIC(pHub, USB_HUB);
          pPortEntry = USBH_DLIST_GetNext(&pHub->PortList);
          while (pPortEntry != &pHub->PortList) {
            pPort = GET_HUB_PORT_PTR(pPortEntry);
            USBH_ASSERT_MAGIC(pPort, USBH_HUB_PORT);
            pPortEntry = USBH_DLIST_GetNext(pPortEntry);
            if (pPort->PortStatus & PORT_STATUS_CONNECT && pPort->PortState == PORT_ERROR) {
              pPort->RetryCounter = 0;
              USBH_BD_SetPortState(pPort, PORT_RESTART);
              succ = TRUE;
            }
          }
        }
      }
    }
    if (succ) {
      USBH_HC_ServicePorts(pHostController); // Services all host controller ports
    }
    pHostControllerEntry = USBH_DLIST_GetNext(pHostControllerEntry);
  }
}

/********************************* EOF ******************************/
