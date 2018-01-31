/*********************************************************************
*  
*   IAR PowerPac
*
*   (c) Copyright IAR Systems 2010.  All rights reserved.
*
**********************************************************************
----------------------------------------------------------------------
File        : USBH_InterfaceList.c
Purpose     : USB pHost implementation
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
static USBH_STATUS IlistAllocAdd(USBH_INTERFACE_ID InterfaceID, INTERFACE_LIST * pList, USBH_HOST_CONTROLLER * pHost) {
  INTERFACE_ENTRY * pInterfaceEntry;

  USBH_LOG((USBH_MTYPE_CORE, "Core: IlistAllocAdd!"));
  USBH_ASSERT_MAGIC(pHost, USBH_HOST_CONTROLLER);
  USBH_ASSERT(pList != NULL);
  pInterfaceEntry = (INTERFACE_ENTRY *)USBH_Malloc(sizeof(INTERFACE_ENTRY));
  if (NULL == pInterfaceEntry) {
    USBH_WARN((USBH_MTYPE_CORE, "Core: IlistAllocAdd: USBH_malloc!"));
    return USBH_STATUS_MEMORY;
  }
  // Setup the interface ID and the number of interfaces in the list
  IFDBG(pInterfaceEntry->Magic    = INTERFACE_ENTRY_MAGIC);
  pInterfaceEntry->InterfaceID    = InterfaceID;
  pInterfaceEntry->HostController = pHost;
  USBH_DLIST_InsertTail(&pList->UsbInterfaceEntryList, &pInterfaceEntry->ListEntry);
  pList->InterfaceCount++;
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
static void IlistDestroy(INTERFACE_LIST * pList) {
  USBH_DLIST           * pEntry;
  INTERFACE_ENTRY * pInterfaceEntry;

  USBH_ASSERT(pList != NULL);
  while (!USBH_DLIST_IsEmpty(&pList->UsbInterfaceEntryList)) {
    pEntry          = USBH_DLIST_GetNext(&pList->UsbInterfaceEntryList);
    pInterfaceEntry = GET_INTERFACE_ENTRY_FROM_ENTRY(pEntry);
    USBH_ASSERT_MAGIC(pInterfaceEntry, INTERFACE_ENTRY);
    USBH_DLIST_RemoveEntry(pEntry);
    USBH_Free(pInterfaceEntry);   // Delete this object
  }
  USBH_Free(pList);
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
USBH_INTERFACE_LIST_HANDLE USBH_CreateInterfaceList(USBH_INTERFACE_MASK * pInterfaceMask, unsigned int * pInterfaceCount) {
  INTERFACE_LIST  * pList;
  USBH_DLIST           * pEntry;
  USBH_DLIST           * pHostList;
  USBH_DLIST           * pInterfaceEntry;
  USBH_DLIST           * pDevEntry;
  USBH_HOST_CONTROLLER * pHostController;
  USB_DEVICE      * pUsbDev;
  USB_INTERFACE   * pInterface;
  USBH_STATUS       Status;

  USBH_LOG((USBH_MTYPE_CORE, "Core: USBH_CreateInterfaceList!"));
  pList = (INTERFACE_LIST *)USBH_Malloc(sizeof(INTERFACE_LIST));
  if (NULL == pList) {
    USBH_WARN((USBH_MTYPE_CORE, "Core: USBH_CreateInterfaceList: USBH_malloc!"));
    return NULL;
  }
  USBH_ZERO_MEMORY(pList, sizeof(INTERFACE_LIST));
  USBH_DLIST_Init(&pList->UsbInterfaceEntryList);
  Status           = USBH_STATUS_SUCCESS;
  *pInterfaceCount = 0;
  pHostList        = &USBH_Global.DriverInst.HostControllerList;
  pEntry           = USBH_DLIST_GetNext(pHostList);
  while (pEntry != pHostList) {                                     // Search in all pHost controller
    pHostController     = GET_HOST_CONTROLLER_FROM_ENTRY(pEntry);
    USBH_ASSERT_MAGIC(pHostController, USBH_HOST_CONTROLLER);
    pDevEntry = USBH_DLIST_GetNext(&pHostController->DeviceList);
    while (pDevEntry != &pHostController->DeviceList) {                       // Search in all devices
      pUsbDev       = GET_USB_DEVICE_FROM_ENTRY(pDevEntry);
      USBH_ASSERT_MAGIC(pUsbDev, USB_DEVICE);
      pInterfaceEntry = USBH_DLIST_GetNext(&pUsbDev->UsbInterfaceList);         // For each interface
      while (pInterfaceEntry != &pUsbDev->UsbInterfaceList) {
        pInterface = GET_USB_INTERFACE_FROM_ENTRY(pInterfaceEntry);
        USBH_ASSERT_MAGIC(pInterface, USB_INTERFACE);
        if (USBH_STATUS_SUCCESS == USBH_BD_CompareUsbInterface(pInterface, pInterfaceMask, FALSE)) {
          Status = IlistAllocAdd(pInterface->InterfaceId, pList, pHostController); // InterfaceMask has matched, add it to the list
          if (Status) {                                           // On error
            USBH_WARN((USBH_MTYPE_CORE, "Core: USBH_CreateInterfaceList: IlistCreateEntryAddToList!"));
            goto exit;
          }
          * pInterfaceCount = *pInterfaceCount + 1;                 // Increment counter
        }
        pInterfaceEntry = USBH_DLIST_GetNext(pInterfaceEntry);
      }

      pDevEntry = USBH_DLIST_GetNext(pDevEntry);
    }

    pEntry = USBH_DLIST_GetNext(pEntry);                                  // Next pHost
  }
  exit:
  ;
  if (Status) {
    IlistDestroy(pList);                                           // Destroy all allocated memory
    USBH_WARN((USBH_MTYPE_CORE, "Core: USBH_CreateInterfaceList!"));
    return NULL;
  }
  USBH_LOG((USBH_MTYPE_CORE, "Core: USBH_CreateInterfaceList returned interfaces: %u!",*pInterfaceCount));
  return pList;
}

/*********************************************************************
*
*       USBH_DestroyInterfaceList
*
*  Function description:
*    Destroy a internal device list and free the related resources.
*/
void USBH_DestroyInterfaceList(USBH_INTERFACE_LIST_HANDLE hInterfaceList) {
  INTERFACE_LIST * pList;
  pList = (INTERFACE_LIST *)hInterfaceList;
  IlistDestroy(pList);
}

/*********************************************************************
*
*       USBH_GetInterfaceId
*
*  Function description:
*/
USBH_INTERFACE_ID USBH_GetInterfaceId(USBH_INTERFACE_LIST_HANDLE hInterfaceList, unsigned int Index) {
  INTERFACE_LIST * pList;
  USBH_ASSERT(NULL != hInterfaceList);
  pList = (INTERFACE_LIST * )hInterfaceList;
  if (Index >= pList->InterfaceCount) {
    USBH_WARN((USBH_MTYPE_CORE, "Core: USBH_GetInterfaceId: Index does not exist!"));
    return 0;
  } else {        // Search in the interface list
    USBH_DLIST           * pEntry;
    INTERFACE_ENTRY * pInterfaceEntry;
    unsigned int      i           = 0;
    pEntry                         = USBH_DLIST_GetNext(&pList->UsbInterfaceEntryList);
    while (pEntry != &pList->UsbInterfaceEntryList) {
      if (i == Index) {
        pInterfaceEntry = GET_INTERFACE_ENTRY_FROM_ENTRY(pEntry);
        USBH_ASSERT_MAGIC(pInterfaceEntry, INTERFACE_ENTRY);
        return pInterfaceEntry->InterfaceID;
      }
      pEntry = USBH_DLIST_GetNext(pEntry);
      i++;
    }
  }
  USBH_WARN((USBH_MTYPE_CORE, "Core: USBH_GetInterfaceId: corrupted interface list!"));
  return 0;
}

/******************************* EOF ********************************/
