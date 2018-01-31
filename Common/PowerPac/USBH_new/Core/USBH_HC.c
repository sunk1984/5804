/*********************************************************************
*  
*   IAR PowerPac
*
*   (c) Copyright IAR Systems 2010.  All rights reserved.
*
**********************************************************************
----------------------------------------------------------------------
File        : USBH_HC.c
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
*       USBH_CreateHostController
*
*  Function description
*    Creates a host controller object. This always includes allocating
*    of one hub object and one root hub object. No host controller driver
*    interface function is called at this time because at this time the
*    host controller driver interface is only allocated but not initialized!
*
*  Parameters:
*    HostEntry: Host driver function interface
*/
USBH_HOST_CONTROLLER * USBH_CreateHostController(USBH_HOST_DRIVER * pDriver, USBH_HC_HANDLE hHostController) {
  USBH_HOST_CONTROLLER * pHost;
  USBH_STATUS       Status;

  USBH_LOG((USBH_MTYPE_CORE, "Core: USBH_CreateHostController!"));
  pHost = (USBH_HOST_CONTROLLER *)USBH_Malloc(sizeof(USBH_HOST_CONTROLLER));
  if (NULL == pHost) {
    USBH_WARN((USBH_MTYPE_CORE, "Core: USBH_CreateHostController: USBH_malloc!"));
    return NULL;
  }
  USBH_ZERO_MEMORY(pHost, sizeof(USBH_HOST_CONTROLLER));
  IFDBG(pHost->Magic = USBH_HOST_CONTROLLER_MAGIC);
  pHost->pInst = &USBH_Global.DriverInst;
  // Set the host controller driver function interface
  pHost->pDriver = pDriver;
  pHost->hHostController = hHostController;
  USBH_DLIST_Init(&pHost->DeviceList);
  Status = USBH_ROOTHUB_Init(pHost);      // Init the root hub
  if (Status != USBH_STATUS_SUCCESS) {
    USBH_Free(pHost);
  }
  HC_INC_REF(pHost);                   // Add the initial reference
  return pHost;
}

/*********************************************************************
*
*       USBH_DeleteHostController
*
*  Function description
*    Deletes the host controller object
*/
void USBH_DeleteHostController(USBH_HOST_CONTROLLER * pHost) {
  USBH_HOST_DRIVER * pDriver = pHost->pDriver;
  USBH_LOG((USBH_MTYPE_CORE, "Core: [USBH_DeleteHostController!"));
  USBH_ROOTHUB_Release(&pHost->RootHub);        // Release the root hub, and the timer
  pDriver->pfHostExit(pHost->hHostController); // Inform the HC driver that everything is released
  USBH_Free(pHost);                          // Delete the Host
  USBH_LOG((USBH_MTYPE_CORE, "Core: ]USBH_DeleteHostController!"));
}

/*********************************************************************
*
*       USBH_AddUsbDevice
*
*  Function description
*/
void USBH_AddUsbDevice(USB_DEVICE * pDevice) {
  USBH_HOST_CONTROLLER * host;
  USBH_ASSERT_MAGIC(pDevice, USB_DEVICE);
  USBH_LOG((USBH_MTYPE_CORE, "Core: USBH_AddUsbDevice Dev-Addr: %u", pDevice->UsbAddress));

  // Set the port pointer to the device, now hub notify and root hub notify function can detect a device
  // on a port and now it is allowed to call UbdUdevMarkParentAndChildDevicesAsRemoved!!!. State machines
  // checks the port state at the entry point and delete self a not complete enumerated device!
  pDevice->State              = DEV_STATE_WORKING;
  pDevice->pParentPort->Device = pDevice;
  host                       = pDevice->pHostController;
  USBH_ASSERT_MAGIC(host, USBH_HOST_CONTROLLER);
  USBH_DLIST_InsertTail(&host->DeviceList, &pDevice->ListEntry);
  host->DeviceCount++;
  DUMP_USB_DEVICE_INFO(pDevice);
}

/*********************************************************************
*
*       USBH_HC_RemoveDeviceFromList
*
*  Function description
*/
void USBH_HC_RemoveDeviceFromList(USB_DEVICE * pDevice) {
  USBH_HOST_CONTROLLER * host;

  USBH_ASSERT_MAGIC(pDevice, USB_DEVICE);
  USBH_LOG((USBH_MTYPE_CORE, "Core: USBH_HC_RemoveDeviceFromList Dev-Addr: %u", pDevice->UsbAddress));
  host            = pDevice->pHostController;
  USBH_ASSERT_MAGIC(host, USBH_HOST_CONTROLLER);
  USBH_DLIST_RemoveEntry(&pDevice->ListEntry);
  USBH_ASSERT(host->DeviceCount);
  host->DeviceCount--;
}

/*********************************************************************
*
*       USBH_DefaultReleaseEpCompletion
*
*  Function description
*/
void USBH_DefaultReleaseEpCompletion(void * pContext) {
  USBH_HOST_CONTROLLER * pHostController = (USBH_HOST_CONTROLLER *)pContext;
  HC_DEC_REF(pHostController);
}

/*********************************************************************
*
*       USBH_EnumerateDevices
*
*  Function description
*    Enumerates the devices on a given host controller. This is called from an external init function.
*/
void USBH_EnumerateDevices(USBH_HC_BD_HANDLE hHcBd) {
  USBH_STATUS        Status;
  USBH_HOST_CONTROLLER  * pHostController = (USBH_HOST_CONTROLLER *)hHcBd;
  USBH_HOST_DRIVER * pDriver         = pHostController->pDriver;

  USBH_ASSERT_MAGIC(pHostController, USBH_HOST_CONTROLLER);
  // Create the required endpoints to make the communication on EP0 */
  pHostController->LowSpeedEndpoint = pDriver->pfAddEndpoint(pHostController->hHostController, // USBH_HC_HANDLE hHostController,
    USB_EP_TYPE_CONTROL,                                                         // U8 EndpointType,
    0,                                                                           // U8 DeviceAddress,
    0,                                                                           // U8 EndpointAddress,
    8,                                                                           // U16 MaxFifoSize,
    0,                                                                           // U16 IntervalTime,
    USBH_LOW_SPEED                                                               // USBH_SPEED Speed
  );
  if (pHostController->LowSpeedEndpoint == NULL) {
    USBH_WARN((USBH_MTYPE_CORE, "Core: USBH_CreateHostController:pfAddEndpoint LS failed!"));
  }
  pHostController->FullSpeedEndpoint = pDriver->pfAddEndpoint(pHostController->hHostController, // USBH_HC_HANDLE hHostController,
    USB_EP_TYPE_CONTROL,                                                          // U8 EndpointType,
    0,                                                                            // U8 DeviceAddress,
    0,                                                                            // U8 EndpointAddress,
    64,                                                                           // U16 MaxFifoSize,
    0,                                                                            // U16 IntervalTime,
    USBH_FULL_SPEED                                                               // USBH_SPEED Speed
  );
  if (pHostController->FullSpeedEndpoint == NULL) {
    USBH_WARN((USBH_MTYPE_CORE, "Core: USBH_CreateHostController:pfAddEndpoint FS failed!"));
  }
  pHostController->HighSpeedEndpoint = pDriver->pfAddEndpoint(pHostController->hHostController, // USBH_HC_HANDLE hHostController,
    USB_EP_TYPE_CONTROL,                                                          // U8 EndpointType,
    0,                                                                            // U8 DeviceAddress,
    0,                                                                            // U8 EndpointAddress,
    64,                                                                           // U16 MaxFifoSize,
    0,                                                                            // U16 IntervalTime,
    USBH_HIGH_SPEED                                                               // USBH_SPEED Speed
  );
  if (pHostController->HighSpeedEndpoint == NULL) {
    USBH_WARN((USBH_MTYPE_CORE, "Core: USBH_CreateHostController:pfAddEndpoint HS failed!"));
  }
  Status = pDriver->pfSetHcState(pHostController->hHostController, USBH_HOST_RUNNING); // Turn on the host controller
  if (Status != USBH_STATUS_SUCCESS) {
    USBH_LOG((USBH_MTYPE_CORE, "Core: USBH_EnumerateDevices:pfSetHcState failed %08x",Status));
    return;
  }
  pHostController->State = HC_WORKING;                                      // Update the host controller state to working
  Status = USBH_ROOTHUB_AddPortsStartPowerGoodTime(&pHostController->RootHub); // start the enumeration of the complete bus
  if (Status != USBH_STATUS_SUCCESS) {
    USBH_LOG((USBH_MTYPE_CORE, "Core: USBH_EnumerateDevices:USBH_ROOTHUB_AddPortsStartPowerGoodTime failed %08x",Status));
    return;
  }
}

/*********************************************************************
*
*       USBH_BD_GetUsbAddress
*
*  Function description
*/
U8 USBH_BD_GetUsbAddress(USBH_HOST_CONTROLLER * pHostController) {
  U8 i;
  for (i = 1; i < 128; i++) {
    if (pHostController->UsbAddressArray[i] == 0) {
      pHostController->UsbAddressArray[i] = 1;
      return i;
    }
  }
  USBH_WARN((USBH_MTYPE_CORE, "Core: FATAL USBH_BD_GetUsbAddress failed!"));
  return 0;
}

/*********************************************************************
*
*       USBH_BD_FreeUsbAddress
*
*  Function description
*/
void USBH_BD_FreeUsbAddress(USBH_HOST_CONTROLLER * pHostController, U8 Address) {
  USBH_ASSERT(Address < 128);
  pHostController->UsbAddressArray[Address] = 0;
}

/*********************************************************************
*
*       USBH_HC_ServicePorts
*
*  Function description
*    If need an root hub or hub service and no hub service state machine
*    and no root hub service state machine is pending an new service
*    state machine for an port is started. An new service is always started
*    if all service state machines are Idle!
*/
void USBH_HC_ServicePorts(USBH_HOST_CONTROLLER * hc) {
  USBH_ASSERT_MAGIC(hc, USBH_HOST_CONTROLLER);
  USBH_ASSERT_MAGIC(&hc->RootHub, ROOT_HUB);
  if (NULL != hc->pActivePortReset) { // Any port is reset
    return;
  }

  if (USBH_Global.Config.SupportExternalHubs) {
    if (!USBH_ROOTHUB_ServicePorts(&hc->RootHub)) {
      USBH_BD_ServiceAllHubs(hc);
    }
  } else {
    USBH_ROOTHUB_ServicePorts(&hc->RootHub);
  }
}

/*********************************************************************
*
*       USBH_HCM_InitItemHeaders
*
*  Function description:
*    Initialize all item headers in the memory pool!
*/
static void USBH_HCM_InitItemHeaders(USBH_HCM_POOL * Pool) {
  U8              * itemPtr;
  U32               phyAddr;
  U8              * virtAddr;
  USBH_HCM_ITEM_HEADER * itemHeader;
  U32               i;

  USBH_LOG((USBH_MTYPE_CORE, "Core: [HcmInitItemHeader "));
  USBH_ASSERT(USBH_IS_PTR_VALID(Pool, USBH_HCM_POOL));
  // Init all used pointers with start addresses
  itemPtr    = (U8 *)Pool->pItemHeaderStartAddr;
  phyAddr    =       Pool->ContiguousMemoryPhyAddr;
  Pool->pHead = NULL;
  for (i = 0; i < Pool->NumberOfItems; i++) { // Build the item pHeader list
    itemHeader             = (USBH_HCM_ITEM_HEADER *)itemPtr;
    // Init the item
    itemHeader->PhyAddr    = phyAddr;
    itemHeader->pOwningPool = Pool;
    itemHeader->Link.Next  = Pool->pHead;      // First inserted pool item points to NULL
    Pool->pHead             = itemHeader;      // Pool points to the last item of the pool
    phyAddr               += Pool->SizeOfItem;             // Update addresses of non paged memory pointers
    virtAddr              += Pool->SizeOfItem;
    itemPtr               += Pool->SizeOfExtension;        // itemPtr points to the next pHeader
  }
  USBH_LOG((USBH_MTYPE_CORE, "Core: ]HcmInitItemHeader "));
}

/*********************************************************************
*
*       USBH_HCM_AllocContiguousMemory
*
*  Function description:
*    Allocates contiguous memory and checks the returned alignment of the physical addresses
*/
static USBH_STATUS USBH_HCM_AllocContiguousMemory(U32 NumberOfBytes, U32 Alignment, U32 * pPhyAddr) {
  USBH_STATUS   Status;
  void         * pMemArea;

  Status      = USBH_STATUS_SUCCESS;
  USBH_LOG((USBH_MTYPE_CORE, "Core: [USBH_HCM_AllocContiguousMemory "));
  pMemArea= USBH_AllocTransferMemory(NumberOfBytes, Alignment);
  if (pMemArea == NULL) {
    USBH_WARN((USBH_MTYPE_CORE, "Core: ERROR USBH_HCM_AllocContiguousMemory: no memory!"));
    Status = USBH_STATUS_MEMORY;
  } else if ((((U32) pMemArea) % Alignment) != 0) {
    USBH_WARN((USBH_MTYPE_CORE, "Core: ERROR USBH_HCM_AllocContiguousMemory: Alignment error: virt. addr: 0x%lx!", pMemArea));
    Status = USBH_STATUS_INVALID_ALIGNMENT;
  } else {
    *pPhyAddr = (U32)pMemArea;
    if (*pPhyAddr == 0) {
      USBH_WARN((USBH_MTYPE_CORE, "Core: ERROR USBH_HCM_AllocContiguousMemory: TAL_GetPhysicalAddress: return NULL!"));
      Status = USBH_STATUS_ERROR;
    } else if (!USBH_IS_ALIGNED(* pPhyAddr, Alignment)) { // Alignment error
      USBH_WARN((USBH_MTYPE_CORE, "Core: ERROR USBH_HCM_AllocContiguousMemory: Alignment error: phys. addr: 0x%lx!", *pPhyAddr));
      Status = USBH_STATUS_INVALID_ALIGNMENT;
    }
  }
  if (Status) {                                       // On error release also allocated contiguous memory
    if (pMemArea != NULL) {
      USBH_Free(pMemArea);
      pMemArea = NULL;
    }
  }
  USBH_LOG((USBH_MTYPE_CORE, "Core: ]USBH_HCM_AllocContiguousMemory "));
  return Status;
}

/*********************************************************************
*
*       USBH_HCM_AllocPool
*
*  Function description:
*    Allocates and initializes a memory pool.
*    The item(s) are allocated in the virtual memory with malloc
*    and the buffer(s) are allocate in physical memory with a buffer alignment!
*
*  Parameters:
*    pPool             - Pointer to the memory pool
*    NumberOfItems    - Number of buffers, the number of items is the same
*    SizeOfItem       - Size of buffer allocated in physical memory
*    SizeOfExtension  - Size of one item that is allocated in virtual memory,
                        this size includes the item pHeader, see also USBH_HCM_ITEM_HEADER
*    Alignment        - Number of bytes for alignment of each physical item in the memory.
*
*  Return value:
*    USBH_STATUS_SUCCESS           - O.K.
*    USBH_STATUS_MEMORY            - No memory available
*    USBH_STATUS_INVALID_ALIGNMENT - Invalid memory alignment
*    USBH_STATUS_INVALID_PARAM     - Invalid parameter
*/
USBH_STATUS USBH_HCM_AllocPool(USBH_HCM_POOL * pPool, U32 NumberOfItems, U32 SizeOfItem, U32 SizeOfExtension, U32 Alignment) {
  U32         TotalSize;
  U32         ExtensionSize;
  U32         PhyAddr;
  USBH_STATUS Status;
  U32         UpAlignedSize;

  USBH_LOG((USBH_MTYPE_CORE, "Core: [USBH_HCM_AllocPool: Items: %lu SizePhyItem: %lu SizeItem: %lu Alignment: %lu",
                                                   NumberOfItems, SizeOfItem, SizeOfExtension, Alignment));
  Status = USBH_STATUS_SUCCESS;
  USBH_ASSERT(pPool != NULL);
  if (Alignment == 0 || SizeOfExtension < sizeof(USBH_HCM_ITEM_HEADER) || SizeOfItem == 0 || NumberOfItems == 0) { // Check parameter
    USBH_WARN((USBH_MTYPE_CORE, "Core: USBH_HCM_AllocPool: invalid parameter!"));
    Status = USBH_STATUS_INVALID_PARAM;
    goto exit;
  }
  USBH_ZERO_MEMORY(pPool, sizeof(USBH_HCM_POOL)); // Clear and set magic
  IFDBG(pPool->Magic = USBH_HCM_POOL_MAGIC);
  PhyAddr = 0;                         // Allocate contiguous none paged memory
  if (1 != NumberOfItems) {
    UpAlignedSize    = USBH_ALIGN_UP(SizeOfItem, Alignment);
    pPool->SizeOfItem = UpAlignedSize;
    TotalSize        = NumberOfItems * UpAlignedSize;
  } else {
    TotalSize        = SizeOfItem;
    pPool->SizeOfItem = SizeOfItem;
  }
  Status = USBH_HCM_AllocContiguousMemory(TotalSize, Alignment, &PhyAddr);
  if (Status) {                        // On error
    goto alloc_err;
  }
  // Allocate virtual memory for items
  ExtensionSize             = NumberOfItems * SizeOfExtension;
  pPool->pItemHeaderStartAddr = USBH_Malloc(ExtensionSize);
  pPool->pItemHeaderEndAddr   = (U8 *)pPool->pItemHeaderStartAddr + ExtensionSize - 1;
  if (pPool->pItemHeaderStartAddr == NULL) { // On error
    USBH_WARN((USBH_MTYPE_CORE, "Core: Hcm_Alloc: USBH_malloc!"));
    Status = USBH_STATUS_MEMORY;
    goto alloc_err;
  }
  // Initialize other members of the pool
  pPool->ContiguousMemoryPhyAddr = PhyAddr;
  pPool->NumberOfItems           = NumberOfItems;
  pPool->SizeOfExtension         = SizeOfExtension;
  // Calculate the item pool physical and virtual end addresses
//  pPool->pEndcontiguousMemoryVirtAddr = (U8 *)pPool->pContiguousMemoryVirtAddr + TotalSize - 1;
  pPool->EndContiguousMemoryPhyAddr  = pPool->ContiguousMemoryPhyAddr + TotalSize - 1;
  USBH_HCM_InitItemHeaders(pPool);
  USBH_LOG((USBH_MTYPE_CORE, "Core: Hcm_Alloc: contiguous memory: phy.Addr: 0x%lx item size: %lu item size: %lu", PhyAddr, TotalSize,pPool->SizeOfItem));
  goto exit;
  // On error
  alloc_err:
  USBH_HCM_FreePool(pPool);
exit:
  USBH_LOG((USBH_MTYPE_CORE, "Core: ]USBH_HCM_AllocPool"));
  return Status;
}

/*********************************************************************
*
*       USBH_HCM_FreePool
*
*  Function description:
*    Frees all allocated memory used from Hcm_InitPool
*
*  Parameters:
*    MemPool    -
*
*  Return value:
*    void       -
*/
void USBH_HCM_FreePool(USBH_HCM_POOL * MemPool) {
  USBH_LOG((USBH_MTYPE_CORE, "Core: [Hcm_FreePool"));
  USBH_ASSERT(USBH_IS_PTR_VALID(MemPool, USBH_HCM_POOL));
  // Frees array in virtual memory

#if (USBH_DEBUG > 1)
  if (0 != MemPool->RefCount) {
    USBH_WARN((USBH_MTYPE_CORE, "Core: FATAL USBH_HCM_FreePool: Number of items not in the pool: %u!",MemPool->RefCount));
  }
#endif

  if (MemPool->pItemHeaderStartAddr != NULL) {
    USBH_Free(MemPool->pItemHeaderStartAddr);
    MemPool->pItemHeaderStartAddr    = NULL;
  }
  if (MemPool->ContiguousMemoryPhyAddr != 0) {
    USBH_Free((void *)MemPool->ContiguousMemoryPhyAddr);
    MemPool->ContiguousMemoryPhyAddr = 0;
  }
  MemPool->NumberOfItems = 0;
  // Make the pool invalid
  IFDBG(MemPool->Magic = 0)
  USBH_LOG((USBH_MTYPE_CORE, "Core: ]USBH_HCM_FreePool"));
}

/*********************************************************************
*
*       USBH_HCM_GetItem
*
*  Function description:
*    Returns an item with an size of the parameter SizeOfExtension
*    from Hcm_InitPool.
*    The offset 0 of the returned item pointer points to an valid USBH_HCM_ITEM_HEADER struct
*
*  Parameters:
*    pMemPool    - Pointer to the memory pool
*
*  Return value:
*    NULL   - No item available
*    != 0   - Success
*/
USBH_HCM_ITEM_HEADER * USBH_HCM_GetItem(USBH_HCM_POOL * pMemPool) {
  USBH_HCM_ITEM_HEADER * pItemHeader;

  USBH_ASSERT(USBH_IS_PTR_VALID(pMemPool, USBH_HCM_POOL));
  if (HcmPoolEmpty(pMemPool)) {
    USBH_WARN((USBH_MTYPE_CORE, "Core: Hcm_GetItem: Empty pPool!"));
    return NULL;
  } else {
    HcmPoolGet(pMemPool, pItemHeader);
    USBH_ASSERT(pItemHeader != NULL);
    USBH_LOG((USBH_MTYPE_CORE, "Core: Hcm_GetItem: available: %d addreses: phy: 0x%lx", pMemPool->NumberOfItems - pMemPool->RefCount, pItemHeader->PhyAddr));
    IFDBG(pItemHeader->Magic = USBH_HCM_ITEM_HEADER_MAGIC);
    return pItemHeader;
  }
}

/*********************************************************************
*
*       USBH_HCM_PutItem
*
*  Function description:
*    Puts the allocated item back in the memory pool.
*    The offset 0 of the returned item pointer points to an valid USBH_HCM_ITEM_HEADER struct.
*
*  Parameters:
*    pItem    - Pointer to item.
*/
void USBH_HCM_PutItem(USBH_HCM_ITEM_HEADER * pItem) {
  USBH_HCM_POOL * pMemPool;

  pMemPool  = pItem->pOwningPool;
  USBH_ASSERT(USBH_IS_PTR_VALID(pMemPool, USBH_HCM_POOL));
  HCM_ASSERT_ITEMHEADER_ADDRESS(pMemPool, pItem);
  IFDBG(pItem->Magic = 0);
  HcmPoolPut(pMemPool, pItem);
  USBH_LOG((USBH_MTYPE_CORE, "Core: Hcm_PutItem: available: %d from %d items", pMemPool->NumberOfItems - pMemPool->RefCount,pMemPool->NumberOfItems));
}

/*********************************************************************
*
*       USBH_HCM_GetItemFromPhyAddr
*
*  Function description:
*    Returns a pointer to the pool item;
*
*  Parameters:
*    pMemPool    - Pointer to the memory pool
*    PhyAddr    - Element PhysicalAddr in an item pHeader.
*
*  Return value:
*       NULL       -  invalid address or the item was not allocated.
*    != NULL       -  Pointer to the item from the physical address
*/
USBH_HCM_ITEM_HEADER * USBH_HCM_GetItemFromPhyAddr(USBH_HCM_POOL * pMemPool, U32 PhyAddr) {
  U32               PhyItemIndex;
  USBH_HCM_ITEM_HEADER * pHeader;

  USBH_LOG((USBH_MTYPE_CORE, "Core: [USBH_HCM_GetItemFromPhyAddr"));
  USBH_ASSERT(USBH_IS_PTR_VALID(pMemPool, USBH_HCM_POOL));
  pHeader = NULL;
  if (!HCM_PHY_ADDR_IN_POOL(pMemPool, PhyAddr)) {
    USBH_WARN((USBH_MTYPE_CORE, "Core: USBH_HCM_GetItemFromPhyAddr: phy addr: 0x%x not in pool!",PhyAddr));
    goto exit;
  }
  // item = item startaddr. + (contiguous memory item index * virtual item size)
  PhyItemIndex = (PhyAddr - pMemPool->ContiguousMemoryPhyAddr) / pMemPool->SizeOfItem;
  pHeader      = (USBH_HCM_ITEM_HEADER *)((U8 *)pMemPool->pItemHeaderStartAddr + (PhyItemIndex * pMemPool->SizeOfExtension));
  USBH_ASSERT(pHeader->PhyAddr == PhyAddr);
  // Check the minimum allocated items
  USBH_ASSERT(pMemPool->RefCount != 0);
exit:
  USBH_LOG((USBH_MTYPE_CORE, "Core: ]USBH_HCM_GetItemFromPhyAddr"));
  return pHeader;
}

/*********************************************************************
*
*       USBH_HCM_IsPhysAddrInPool
*
*  Function description:
*/
U32 USBH_HCM_IsPhysAddrInPool(USBH_HCM_POOL * MemPool, U32 PhyAddr) {
  if (HCM_PHY_ADDR_IN_POOL(MemPool, PhyAddr)) {
    return PhyAddr;
  } else {
    USBH_LOG((USBH_MTYPE_CORE, "Core: USBH_HCM_IsPhysAddrInPool Addr. not in pool!"));
    return 0;
  }
}

/*********************************************************************
*
*       USBH_HCM_FillPhyMemory
*
*  Function description:
*    USBH_HCM_FillPhyMemory clears the contiguous memory of an item.
*
*  Parameters:
*    Item    - Pointer to the extra memory
*    Val     - value write to each address in memory
*/
void USBH_HCM_FillPhyMemory(USBH_HCM_ITEM_HEADER * Item, U8 Val) {
  U32   ct;
  U8  * ptr;
  ct  = Item->pOwningPool->SizeOfItem;
  ptr = (U8 *)Item->PhyAddr;
  while (ct--) {
    * ptr++ = Val;
  }
}

/*********************************************************************
*
*       USBH_HC_SetActivePortReset
*
*  Function description:
*    Sets the host controller active port reset pointer.
*
*  Parameters:
*     pHost   :   Host controller
*     pEnumPort:  Port, where an port reset must be started.
*/
void USBH_HC_SetActivePortReset(USBH_HOST_CONTROLLER * pHost, USBH_HUB_PORT * pEnumPort) {
  USBH_ASSERT_MAGIC(pHost, USBH_HOST_CONTROLLER);
  if (NULL != pHost->pActivePortReset) {
    USBH_WARN((USBH_MTYPE_CORE, "Core: USBH_HC_SetActivePortReset: pActivePortReset is not NULL!"));
  } else {
    USBH_ASSERT_MAGIC(pEnumPort, USBH_HUB_PORT);
    pHost->pActivePortReset = pEnumPort;
  }
}

/*********************************************************************
*
*       USBH_HC_ClrActivePortReset
*
*  Function description:
*      Sets the host controller active port reset pointer to NULL.
*      It must be called at the end of a port reset.
*      If this function is not called a new enumeration is never started.
*
*  Parameters:
*     pHost   :   Host controller
*     pEnumPort:  Port, where an port reset must be started.
*/
void USBH_HC_ClrActivePortReset(USBH_HOST_CONTROLLER * pHost, USBH_HUB_PORT * pEnumPort) {
  USBH_ASSERT_MAGIC(pHost, USBH_HOST_CONTROLLER);
  if (NULL == pHost->pActivePortReset) {
    USBH_WARN((USBH_MTYPE_CORE, "Core: USBH_HC_ClrActivePortReset: pActivePortReset already NULL!"));
  } else {
    USBH_ASSERT_MAGIC(pEnumPort, USBH_HUB_PORT);
    if (pHost->pActivePortReset != pEnumPort) {
      USBH_WARN((USBH_MTYPE_CORE, "Core: USBH_HC_ClrActivePortReset: not the same port as at the start!"));
    }
    pHost->pActivePortReset = NULL;
  }
}

/******************************* EOF ********************************/
