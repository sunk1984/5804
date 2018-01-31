/*********************************************************************
*  
*   IAR PowerPac
*
*   (c) Copyright IAR Systems 2010.  All rights reserved.
*
**********************************************************************
----------------------------------------------------------------------
File        : USBH_InterfaceList.c
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
*       IlistAllocAdd
*
*  Function description:
*    Allocate an INTERFACE_ENTRY object and adds this to the interface list
*/
static USBH_STATUS IlistAllocAdd(USBH_INTERFACE_ID InterfaceID, INTERFACE_LIST * List, HOST_CONTROLLER * host) {
  INTERFACE_ENTRY * ifaceEntry;
  USBH_LOG((USBH_MTYPE_CORE, "Core: IlistAllocAdd!"));
  T_ASSERT_MAGIC(host, HOST_CONTROLLER);
  T_ASSERT(List != NULL);
  ifaceEntry = USBH_Malloc(sizeof(INTERFACE_ENTRY));
  if (NULL == ifaceEntry) {
    USBH_WARN((USBH_MTYPE_CORE, "Core: IlistAllocAdd: USBH_malloc!"));
    return USBH_STATUS_MEMORY;
  }
  // Setup the interface ID and the number of interfaces in the list
#if (USBH_DEBUG > 1)
  ifaceEntry->Magic          = INTERFACE_ENTRY_MAGIC;
#endif
  ifaceEntry->InterfaceID    = InterfaceID;
  ifaceEntry->HostController = host;
  DlistInsertTail(&List->UsbInterfaceEntryList, &ifaceEntry->ListEntry);
  List->InterfaceCount++;
  return USBH_STATUS_SUCCESS;
}

/*********************************************************************
*
*       IlistAllocAdd
*
*  Function description:
*    Destroy all linked INTERFACE_ENTRY objects and at least the interface
*    list it self. After the function returns the list pointer is invalid!
*/
static void IlistDestroy(INTERFACE_LIST * List) {
  PDLIST            entry;
  INTERFACE_ENTRY * ifaceEntry;
  T_ASSERT(List != NULL);
  while (!DlistEmpty(&List->UsbInterfaceEntryList)) {
    entry      = DlistGetNext(&List->UsbInterfaceEntryList);
    ifaceEntry = GET_INTERFACE_ENTRY_FROM_ENTRY(entry);
    T_ASSERT_MAGIC(ifaceEntry, INTERFACE_ENTRY);
    DlistRemoveEntry(entry);
    USBH_Free(ifaceEntry);   // Delete this object
  }
  USBH_Free(List);
}

/*********************************************************************
*
*       USB Bus Driver API
*
**********************************************************************
*/

/*********************************************************************
*
*       USBH_CreateInterfaceList
*
*  Function description:
*    The internal interface list contains all devices matching to the InterfaceMask at
*    the point of time where the function is called. The handle must be released with
*    USBH_DestroyInterfaceList to free the related memory. Hub device are not added to the list!
*/
USBH_INTERFACE_LIST_HANDLE USBH_CreateInterfaceList(USBH_INTERFACE_MASK * InterfaceMask, unsigned int * InterfaceCount) {
  INTERFACE_LIST  * list;
  PDLIST            entry, hostList, ifaceEntry, devEntry;
  HOST_CONTROLLER * host;
  USB_DEVICE      * uDev;
  USB_INTERFACE   * iface;
  USBH_STATUS       status;

  USBH_LOG((USBH_MTYPE_CORE, "Core: USBH_CreateInterfaceList!"));
  list = USBH_Malloc(sizeof(INTERFACE_LIST));
  if (NULL == list) {
    USBH_WARN((USBH_MTYPE_CORE, "Core: USBH_CreateInterfaceList: USBH_malloc!"));
    return NULL;
  }
  ZERO_MEMORY(list, sizeof(INTERFACE_LIST));
  DlistInit(&list->UsbInterfaceEntryList);
  status           = USBH_STATUS_SUCCESS;
  * InterfaceCount = 0;
  hostList         = &gUsbDriver.HostControllerList;
  entry            = DlistGetNext(hostList);
  while (entry != hostList) {                                     // Search in all host controller
    host     = GET_HOST_CONTROLLER_FROM_ENTRY(entry);
    T_ASSERT_MAGIC(host, HOST_CONTROLLER);
    devEntry = DlistGetNext(&host->DeviceList);
    while (devEntry != &host->DeviceList) {                       // Search in all devices
      uDev       = GET_USB_DEVICE_FROM_ENTRY(devEntry);
      T_ASSERT_MAGIC(uDev, USB_DEVICE);
      ifaceEntry = DlistGetNext(&uDev->UsbInterfaceList);         // For each interface
      while (ifaceEntry != &uDev->UsbInterfaceList) {
        iface = GET_USB_INTERFACE_FROM_ENTRY(ifaceEntry);
        T_ASSERT_MAGIC(iface, USB_INTERFACE);
        if (USBH_STATUS_SUCCESS == UbdCompareUsbInterface(iface, InterfaceMask, FALSE)) {
          status = IlistAllocAdd(iface->InterfaceID, list, host); // InterfaceMask has matched, add it to the list
          if (status) {                                           // On error
            USBH_WARN((USBH_MTYPE_CORE, "Core: USBH_CreateInterfaceList: IlistCreateEntryAddToList!"));
            goto exit;
          }
          * InterfaceCount = *InterfaceCount + 1;                 // Increment counter
        }
        ifaceEntry = DlistGetNext(ifaceEntry);
      }

      devEntry = DlistGetNext(devEntry);
    }

    entry = DlistGetNext(entry);                                  // Next host
  }
  exit:
  ;
  if (status) {
    IlistDestroy(list);                                           // Destroy all allocated memory
    USBH_WARN((USBH_MTYPE_CORE, "Core: USBH_CreateInterfaceList!"));
    return NULL;
  }
  USBH_LOG((USBH_MTYPE_CORE, "Core: USBH_CreateInterfaceList returned interfaces: %u!",*InterfaceCount));
  return list;
}

/*********************************************************************
*
*       USBH_DestroyInterfaceList
*
*  Function description:
*    Destroy a internal device list and free the related resources.
*/
void USBH_DestroyInterfaceList(USBH_INTERFACE_LIST_HANDLE InterfaceListHandle) {
  INTERFACE_LIST * list;
  list = (INTERFACE_LIST *)InterfaceListHandle;
  IlistDestroy(list);
}

/*********************************************************************
*
*       USBH_GetInterfaceID
*
*  Function description:
*/
USBH_INTERFACE_ID USBH_GetInterfaceID(USBH_INTERFACE_LIST_HANDLE InterfaceListHandle, unsigned int Index) {
  INTERFACE_LIST * list;
  T_ASSERT(NULL != InterfaceListHandle);
  list = (INTERFACE_LIST * )InterfaceListHandle;
  if (Index >= list->InterfaceCount) {
    USBH_WARN((USBH_MTYPE_CORE, "Core: USBH_GetInterfaceID: Index does not exist!"));
    return 0;
  } else {        // Search in the interface list
    PDLIST            entry;
    INTERFACE_ENTRY * ifaceEntry;
    unsigned int      i           = 0;
    entry                         = DlistGetNext(&list->UsbInterfaceEntryList);
    while (entry != &list->UsbInterfaceEntryList) {
      if (i == Index) {
        ifaceEntry = GET_INTERFACE_ENTRY_FROM_ENTRY(entry);
        T_ASSERT_MAGIC(ifaceEntry, INTERFACE_ENTRY);
        return ifaceEntry->InterfaceID;
      }
      entry = DlistGetNext(entry);
      i++;
    }
  }
  USBH_WARN((USBH_MTYPE_CORE, "Core: USBH_GetInterfaceID: corrupted interface list!"));
  return 0;
}

/******************************* EOF ********************************/
