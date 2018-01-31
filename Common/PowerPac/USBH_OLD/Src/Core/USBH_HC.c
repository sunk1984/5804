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
*       UbdCreateHostController
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
HOST_CONTROLLER * UbdCreateHostController(USB_HOST_ENTRY * HostEntry) {
  HOST_CONTROLLER * host;
  USBH_STATUS       Status;

  USBH_LOG((USBH_MTYPE_CORE, "Core: UbdCreateHostController!"));
  host = USBH_Malloc(sizeof(HOST_CONTROLLER));
  if (NULL == host) {
    USBH_WARN((USBH_MTYPE_CORE, "Core: UbdCreateHostController: USBH_malloc!"));
    return NULL;
  }
  ZERO_MEMORY(host, sizeof(HOST_CONTROLLER));

#if (USBH_DEBUG > 1)
  host->Magic = HOST_CONTROLLER_MAGIC;
#endif

  host->Driver = &gUsbDriver;
  // Set the host controller driver function interface
  USBH_MEMCPY(&host->HostEntry, HostEntry, sizeof(USB_HOST_ENTRY));
  DlistInit(&host->DeviceList);
  Status = UbdInitRootHub(host);      // Init the root hub
  if (Status != USBH_STATUS_SUCCESS) {
    USBH_Free(host);
  }
  HC_INC_REF(host);                   // Add the initial reference
  return host;
}

/*********************************************************************
*
*       UbdDeleteHostController
*
*  Function description
*    Deletes the host controller object
*/
void UbdDeleteHostController(HOST_CONTROLLER * Host) {
  USB_HOST_ENTRY * HostEntry = &Host->HostEntry;
  USBH_LOG((USBH_MTYPE_CORE, "Core: [UbdDeleteHostController!"));
  UbdReleaseRootHub(&Host->RootHub);        // Release the root hub, and the timer
  HostEntry->HostExit(HostEntry->HcHandle); // Inform the HC driver that everything is released
  USBH_Free(Host);                          // Delete the Host
  USBH_LOG((USBH_MTYPE_CORE, "Core: ]UbdDeleteHostController!"));
}

/*********************************************************************
*
*       UbdAddUsbDevice
*
*  Function description
*/
void UbdAddUsbDevice(USB_DEVICE * Device) {
  HOST_CONTROLLER * host;
  T_ASSERT_MAGIC(Device, USB_DEVICE);
  USBH_LOG((USBH_MTYPE_CORE, "Core: UbdAddUsbDevice Dev-Addr: %u", Device->UsbAddress));

  // Set the port pointer to the device, now hub notify and root hub notify function can detect an device
  // on an port and now it is allowed to call UbdUdevMarkParentAndChildDevicesAsRemoved!!!. State machines
  // checks the port state at the entry point and delete self an not complete enumerated device!
  Device->State              = DEV_STATE_WORKING;
  Device->ParentPort->Device = Device;
  host                       = Device->HostController;
  T_ASSERT_MAGIC(host, HOST_CONTROLLER);
  DlistInsertTail(&host->DeviceList, &Device->ListEntry);
  host->DeviceCount++;
  DUMP_USB_DEVICE_INFO(Device);
}

/*********************************************************************
*
*       UbdHcRemoveDeviceFromList
*
*  Function description
*/
void UbdHcRemoveDeviceFromList(USB_DEVICE * Device) {
  HOST_CONTROLLER * host;
  T_ASSERT_MAGIC(Device, USB_DEVICE);
  USBH_LOG((USBH_MTYPE_CORE, "Core: UbdHcRemoveDeviceFromList Dev-Addr: %u", Device->UsbAddress));
  host            = Device->HostController;
  T_ASSERT_MAGIC(host, HOST_CONTROLLER);
  DlistRemoveEntry(&Device->ListEntry);
  T_ASSERT(host->DeviceCount);
  host->DeviceCount--;
}

/*********************************************************************
*
*       UbdDefaultReleaseEpCompletion
*
*  Function description
*/
void UbdDefaultReleaseEpCompletion(void * Context) {
  HOST_CONTROLLER * HostController = (HOST_CONTROLLER *)Context;
  HC_DEC_REF(HostController);
}

/*********************************************************************
*
*       USBH_EnumerateDevices
*
*  Function description
*    Enumerates the devices on a given host controller. This is called from an external init function.
*/
void USBH_EnumerateDevices(USBH_HC_BD_HANDLE HcBdHandle) {
  HOST_CONTROLLER * HostController = (HOST_CONTROLLER *)HcBdHandle;
  USB_HOST_ENTRY  * HostEntry      = &HostController->HostEntry;
  USBH_STATUS       Status;

  T_ASSERT_MAGIC(HostController, HOST_CONTROLLER);
  // Create the required endpoints to make the communication on EP0 */
  HostController->LowSpeedEndpoint = HostEntry->AddEndpoint(HostEntry->HcHandle, // USBH_HC_HANDLE HcHandle,
    USB_EP_TYPE_CONTROL,                                                         // U8 EndpointType,
    0,                                                                           // U8 DeviceAddress,
    0,                                                                           // U8 EndpointAddress,
    8,                                                                           // U16 MaxFifoSize,
    0,                                                                           // U16 IntervalTime,
    USBH_LOW_SPEED                                                               // USBH_SPEED Speed
  );
  if (HostController->LowSpeedEndpoint == NULL) {
    USBH_WARN((USBH_MTYPE_CORE, "Core: UbdCreateHostController:AddEndpoint LS failed!"));
  }
  HostController->FullSpeedEndpoint = HostEntry->AddEndpoint(HostEntry->HcHandle, // USBH_HC_HANDLE HcHandle,
    USB_EP_TYPE_CONTROL,                                                          // U8 EndpointType,
    0,                                                                            // U8 DeviceAddress,
    0,                                                                            // U8 EndpointAddress,
    64,                                                                           // U16 MaxFifoSize,
    0,                                                                            // U16 IntervalTime,
    USBH_FULL_SPEED                                                               // USBH_SPEED Speed
  );
  if (HostController->FullSpeedEndpoint == NULL) {
    USBH_WARN((USBH_MTYPE_CORE, "Core: UbdCreateHostController:AddEndpoint FS failed!"));
  }
  HostController->HighSpeedEndpoint = HostEntry->AddEndpoint(HostEntry->HcHandle, // USBH_HC_HANDLE HcHandle,
    USB_EP_TYPE_CONTROL,                                                          // U8 EndpointType,
    0,                                                                            // U8 DeviceAddress,
    0,                                                                            // U8 EndpointAddress,
    64,                                                                           // U16 MaxFifoSize,
    0,                                                                            // U16 IntervalTime,
    USBH_HIGH_SPEED                                                               // USBH_SPEED Speed
  );
  if (HostController->HighSpeedEndpoint == NULL) {
    USBH_WARN((USBH_MTYPE_CORE, "Core: UbdCreateHostController:AddEndpoint HS failed!"));
  }
  Status = HostController->HostEntry.SetHcState(HostController->HostEntry.HcHandle, USBH_HOST_RUNNING); // Turn on the host controller
  if (Status != USBH_STATUS_SUCCESS) {
    USBH_LOG((USBH_MTYPE_CORE, "Core: USBH_EnumerateDevices:SetHcState failed %08x",Status));
    return;
  }
  HostController->State = HC_WORKING;                                      // Update the host controller state to working
  Status = UbdRootHubAddPortsStartPowerGoodTime(&HostController->RootHub); // start the enumeration of the complete bus
  if (Status != USBH_STATUS_SUCCESS) {
    USBH_LOG((USBH_MTYPE_CORE, "Core: USBH_EnumerateDevices:UbdRootHubAddPortsStartPowerGoodTime failed %08x",Status));
    return;
  }
}

/*********************************************************************
*
*       UbdGetUsbAddress
*
*  Function description
*/
U8 UbdGetUsbAddress(HOST_CONTROLLER * HostController) {
  U8 i;
  for (i = 1; i < 128; i++) {
    if (HostController->UsbAddressArray[i] == 0) {
      HostController->UsbAddressArray[i] = 1;
      return i;
    }
  }
  USBH_WARN((USBH_MTYPE_CORE, "Core: FATAL UbdGetUsbAddress failed!"));
  return 0;
}

/*********************************************************************
*
*       UbdFreeUsbAddress
*
*  Function description
*/
void UbdFreeUsbAddress(HOST_CONTROLLER * HostController, U8 Address) {
  T_ASSERT(Address < 128);
  HostController->UsbAddressArray[Address] = 0;
}

/*********************************************************************
*
*       UbdHcServicePorts
*
*  Function description
*    If need an root hub or hub service and no hub service state machine
*    and no root hub service state machine is pending an new service
*    statemachine for an port is started. An new service is always started
*    if all service state machines are Idle!
*/
void UbdHcServicePorts(HOST_CONTROLLER * hc) {
  T_ASSERT_MAGIC(hc, HOST_CONTROLLER);
  T_ASSERT_MAGIC(&hc->RootHub, ROOT_HUB);
  if (NULL != hc->ActivePortReset) { // Any port is reset
    return;
  }

#if USBH_EXTHUB_SUPPORT
  if (!UbdServiceRootHubPorts(&hc->RootHub)) {
    UbdServiceAllHubs(hc);
  }
#else
  UbdServiceRootHubPorts(&hc->RootHub);
#endif
}

/*********************************************************************
*
*       HcmInitItemHeaders
*
*  Function description:
*    Initialize all item headers in the memory pool!
*/
static void HcmInitItemHeaders(HCM_POOL * Pool) {
  U8              * itemPtr;
  U32               phyAddr;
  U8              * virtAddr;
  HCM_ITEM_HEADER * itemHeader;
  U32               i;

  USBH_LOG((USBH_MTYPE_CORE, "Core: [HcmInitItemHeader "));
  T_ASSERT(PTRVALID(Pool, HCM_POOL));
  // Init all used pointers with start addresses
  itemPtr    =       Pool->ItemHeaderStartAddr;
  phyAddr    =       Pool->contiguousMemoryPhyAddr;
  virtAddr   = (U8 *)Pool->contiguousMemoryVirtAddr;
  Pool->Head = NULL;
  for (i = 0; i < Pool->NumberOfItems; i++) { // Build the item header list
    itemHeader             = (HCM_ITEM_HEADER *)itemPtr;
    // Init the item
    itemHeader->PhyAddr    = phyAddr;
    itemHeader->VirtAddr   = virtAddr;
    itemHeader->OwningPool = Pool;
    itemHeader->Link.Next  = Pool->Head;      // First inserted pool item points to NULL
    Pool->Head             = itemHeader;      // Pool points to the last item of the pool
    phyAddr               += Pool->SizeOfItem;             // Update addresses of non paged memory pointers
    virtAddr              += Pool->SizeOfItem;
    itemPtr               += Pool->SizeOfExtension;        // itemPtr points to the next header
  }
  USBH_LOG((USBH_MTYPE_CORE, "Core: ]HcmInitItemHeader "));
}

/*********************************************************************
*
*       HcmAllocContiguousMemory
*
*  Function description:
*    Allocates contiguous memory and checks the returned alignment of the physical addresses
*/
static USBH_STATUS HcmAllocContiguousMemory(U32 NumberOfBytes, U32 Alignment, void * * ContiguousAddr, U32 * PhyAddr) {
  USBH_STATUS   status;
  status      = USBH_STATUS_SUCCESS;
  USBH_LOG((USBH_MTYPE_CORE, "Core: [HcmAllocContiguousMemory "));
  * ContiguousAddr = USBH_AllocTransferMemory(NumberOfBytes, Alignment);
  if (* ContiguousAddr == NULL) {
    USBH_WARN((USBH_MTYPE_CORE, "Core: ERROR HcmAllocContiguousMemory: no memory!"));
    status = USBH_STATUS_MEMORY;
  } else if ((((U32) * ContiguousAddr) % Alignment) != 0) {
    USBH_WARN((USBH_MTYPE_CORE, "Core: ERROR HcmAllocContiguousMemory: Alignment error: virt. addr: 0x%lx!", *ContiguousAddr));
    status = USBH_STATUS_INVALID_ALIGNMENT;
  } else {
    * PhyAddr = (U32)(* ContiguousAddr);
    if (* PhyAddr == 0) {
      USBH_WARN((USBH_MTYPE_CORE, "Core: ERROR HcmAllocContiguousMemory: TAL_GetPhysicalAddress: return NULL!"));
      status = USBH_STATUS_ERROR;
    } else if (!TB_IS_ALIGNED(* PhyAddr, Alignment)) { // Alignment error
      USBH_WARN((USBH_MTYPE_CORE, "Core: ERROR HcmAllocContiguousMemory: Alignment error: phys. addr: 0x%lx!", *PhyAddr));
      status = USBH_STATUS_INVALID_ALIGNMENT;
    }
  }
  if (status) {                                       // On error release also allocated contiguous memory
    if (* ContiguousAddr != NULL) {
      USBH_Free(ContiguousAddr);
      * ContiguousAddr = NULL;
    }
  }
  USBH_LOG((USBH_MTYPE_CORE, "Core: ]HcmAllocContiguousMemory "));
  return status;
}

/*********************************************************************
*
*       HcmAllocPool
*
*  Function description:
*    Allocates and initializes a memory pool.
*    The item(s) are allocated in the virtual memory with malloc
*    and the buffer(s) are allocate in physical memory with an buffer alignment!
*
*  Parameters:
*    Pool             - Pointer to the memory pool
*    NumberOfItems    - Number of buffers, the number of items is the same
*    SizeOfItem       - Size of buffer allocated in physical memory
*    SizeOfExtension  - Size of one item that is allocated in virtual memory,
                        this size includes the item header, see also HCM_ITEM_HEADER
*    Alignment        - Number of bytes for alignment of each physical item in the memory.
*
*  Return value:
*    USBH_STATUS_SUCCESS           - O.K.
*    USBH_STATUS_MEMORY            - No memory available
*    USBH_STATUS_INVALID_ALIGNMENT - Invalid memory alignment
*    USBH_STATUS_INVALID_PARAM     - Invalid parameter
*/
USBH_STATUS HcmAllocPool(HCM_POOL * Pool, U32 NumberOfItems, U32 SizeOfItem, U32 SizeOfExtension, U32 Alignment) {
  U32         totalSize;
  U32         extensionSize;
  U32         phyAddr;
  USBH_STATUS status;
  U32         upAlignedSize;

  USBH_LOG((USBH_MTYPE_CORE, "Core: [HcmAllocPool: Items: %lu SizePhyItem: %lu SizeItem: %lu Alignment: %lu",
                                                   NumberOfItems, SizeOfItem, SizeOfExtension, Alignment));
  status = USBH_STATUS_SUCCESS;
  T_ASSERT(Pool != NULL);
  if (Alignment == 0 || SizeOfExtension < sizeof(HCM_ITEM_HEADER) || SizeOfItem == 0 || NumberOfItems == 0) { // Check parameter
    USBH_WARN((USBH_MTYPE_CORE, "Core: HcmAllocPool: invalid parameter!"));
    status = USBH_STATUS_INVALID_PARAM;
    goto exit;
  }
  ZERO_MEMORY(Pool, sizeof(HCM_POOL)); // Clear and set magic

#if (USBH_DEBUG > 1)
  Pool->Magic        = HCM_POOL_MAGIC;
#endif

  phyAddr = 0;                         // Allocate contiguous none paged memory
  if (1 != NumberOfItems) {
    upAlignedSize    = TB_ALIGN_UP(SizeOfItem, Alignment);
    Pool->SizeOfItem = upAlignedSize;
    totalSize        = NumberOfItems * upAlignedSize;
  } else {
    totalSize        = SizeOfItem;
    Pool->SizeOfItem = SizeOfItem;
  }
  status = HcmAllocContiguousMemory(totalSize, Alignment, &Pool->contiguousMemoryVirtAddr, &phyAddr);
  if (status) {                        // On error
    goto alloc_err;
  }
  // Allocate virtual memory for items
  extensionSize             = NumberOfItems * SizeOfExtension;
  Pool->ItemHeaderStartAddr = USBH_Malloc(extensionSize);
  Pool->ItemHeaderEndAddr   = (U8 *)Pool->ItemHeaderStartAddr + extensionSize - 1;
  if (Pool->ItemHeaderStartAddr == NULL) { // On error
    USBH_WARN((USBH_MTYPE_CORE, "Core: Hcm_Alloc: USBH_malloc!"));
    status = USBH_STATUS_MEMORY;
    goto alloc_err;
  }
  // Initialize other members of the pool
  Pool->contiguousMemoryPhyAddr = phyAddr;
  Pool->NumberOfItems           = NumberOfItems;
  Pool->SizeOfExtension         = SizeOfExtension;
  // Calculate the item pool physical and virtual end addresses
  Pool->EndcontiguousMemoryVirtAddr = (U8 *)Pool->contiguousMemoryVirtAddr + totalSize - 1;
  Pool->EndcontiguousMemoryPhyAddr  = Pool->contiguousMemoryPhyAddr + totalSize - 1;
  HcmInitItemHeaders(Pool);
  USBH_LOG((USBH_MTYPE_CORE, "Core: Hcm_Alloc: contiguous memory: virt.Addr: 0x%lx phy.Addr: 0x%lx item size: %lu item size: %lu", (U32)Pool->contiguousMemoryVirtAddr, phyAddr, totalSize,Pool->SizeOfItem));
  goto exit;
  // On error
  alloc_err:
  HcmFreePool(Pool);
exit:
  USBH_LOG((USBH_MTYPE_CORE, "Core: ]HcmAllocPool"));
  return status;
}

/*********************************************************************
*
*       HcmFreePool
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
void HcmFreePool(HCM_POOL * MemPool) {
  USBH_LOG((USBH_MTYPE_CORE, "Core: [Hcm_FreePool"));
  T_ASSERT(PTRVALID(MemPool, HCM_POOL));
  // Frees array in virtual memory

#if (USBH_DEBUG > 1)
  if (0 != MemPool->RefCount) {
    USBH_WARN((USBH_MTYPE_CORE, "Core: FATAL HcmFreePool: Number of items not in the pool: %u!",MemPool->RefCount));
  }
#endif

  if (MemPool->ItemHeaderStartAddr != NULL) {
    USBH_Free(MemPool->ItemHeaderStartAddr);
    MemPool->ItemHeaderStartAddr    = NULL;
  }
  if (MemPool->contiguousMemoryVirtAddr != NULL) {
    USBH_Free(MemPool->contiguousMemoryVirtAddr);
    MemPool->contiguousMemoryVirtAddr    = NULL;
  }
  MemPool->NumberOfItems = 0;
  // Make the pool invalid
  IFDBG(MemPool->Magic = 0)
  USBH_LOG((USBH_MTYPE_CORE, "Core: ]HcmFreePool"));
}

/*********************************************************************
*
*       HcmGetItem
*
*  Function description:
*    Returns an item with an size of the parameter SizeOfExtension
*    from Hcm_InitPool.
*    The offset 0 of the returned item pointer points to an valid HCM_ITEM_HEADER struct
*
*  Parameters:
*    MemPool    - Pointer to the memory pool
*
*  Return value:
*    NULL   - No item available
*    != 0   - Success
*/
HCM_ITEM_HEADER * HcmGetItem(HCM_POOL * MemPool) {
  HCM_ITEM_HEADER * itemHeader;
  T_ASSERT(PTRVALID(MemPool, HCM_POOL));
  if (HcmPoolEmpty(MemPool)) {
    USBH_WARN((USBH_MTYPE_CORE, "Core: Hcm_GetItem: Empty Pool!"));
    return NULL;
  } else {
    HcmPoolGet(MemPool, itemHeader);
    T_ASSERT(itemHeader != NULL);
    USBH_LOG((USBH_MTYPE_CORE, "Core: Hcm_GetItem: available: %d addreses: log: 0x%x phy: 0x%lx", MemPool->NumberOfItems - MemPool->RefCount, itemHeader->VirtAddr, itemHeader->PhyAddr));
    IFDBG(itemHeader->Magic = HCM_ITEM_HEADER_MAGIC);
    return itemHeader;
  }
}

/*********************************************************************
*
*       HcmPutItem
*
*  Function description:
*    Puts the allocated item back in the memory pool.
*    The offset 0 of the returned item pointer points to an valid HCM_ITEM_HEADER struct.
*
*  Parameters:
*    Item    - Pointer to item.
*/
void HcmPutItem(HCM_ITEM_HEADER * Item) {
  HCM_POOL * memPool;
  memPool  = Item->OwningPool;
  T_ASSERT(PTRVALID(memPool, HCM_POOL));
  HCM_ASSERT_ITEMHEADER_ADDRESS(memPool, Item);
  IFDBG(Item->Magic = 0);
  HcmPoolPut(memPool, Item);
  USBH_LOG((USBH_MTYPE_CORE, "Core: Hcm_PutItem: available: %d from %d items", memPool->NumberOfItems - memPool->RefCount,memPool->NumberOfItems));
}

/*********************************************************************
*
*       HcmGetItemFromPhyAddr
*
*  Function description:
*    Returns a pointer to the pool item;
*
*  Parameters:
*    MemPool    - Pointer to the memory pool
*    PhyAddr    - Element PhysicalAddr in an item header.
*
*  Return value:
*       NULL       -  invalid address or the item was not allocated.
*    != NULL       -  Pointer to the item from the physical address
*/
HCM_ITEM_HEADER * HcmGetItemFromPhyAddr(HCM_POOL * MemPool, U32 PhyAddr) {
  U32               physItemIdx;
  HCM_ITEM_HEADER * header;

  USBH_LOG((USBH_MTYPE_CORE, "Core: [HcmGetItemFromPhyAddr"));
  T_ASSERT(PTRVALID(MemPool, HCM_POOL));
  header = NULL;
  if (!HCM_PHY_ADDR_IN_POOL(MemPool, PhyAddr)) {
    USBH_WARN((USBH_MTYPE_CORE, "Core: HcmGetItemFromPhyAddr: phy addr: 0x%x not in pool!",PhyAddr));
    goto exit;
  }
  // item = item startaddr. + (contiguous memory item index * virtual item size)
  physItemIdx = (PhyAddr - MemPool->contiguousMemoryPhyAddr) / MemPool->SizeOfItem;
  header      = (HCM_ITEM_HEADER *)((U8 *)MemPool->ItemHeaderStartAddr + (physItemIdx * MemPool->SizeOfExtension));
  T_ASSERT(header->PhyAddr == PhyAddr);
  // Check the minimum allocated items
  T_ASSERT(MemPool->RefCount != 0);
exit:
  USBH_LOG((USBH_MTYPE_CORE, "Core: ]HcmGetItemFromPhyAddr"));
  return header;
}

/*********************************************************************
*
*       HcmIsPhysAddrInPool
*
*  Function description:
*/
U32 HcmIsPhysAddrInPool(HCM_POOL * MemPool, U32 PhyAddr) {
  if (HCM_PHY_ADDR_IN_POOL(MemPool, PhyAddr)) {
    return PhyAddr;
  } else {
    USBH_LOG((USBH_MTYPE_CORE, "Core: HcmIsPhysAddrInPool Addr. not in pool!"));
    return 0;
  }
}

/*********************************************************************
*
*       HcmFillPhyMemory
*
*  Function description:
*    HcmFillPhyMemory clears the contiguous memory of an item.
*
*  Parameters:
*    Item    - Pointer to the extra memory
*    Val     - value write to each address in memory
*/
void HcmFillPhyMemory(HCM_ITEM_HEADER * Item, U8 Val) {
  U32   ct;
  U8  * ptr;
  ct  = Item->OwningPool->SizeOfItem;
  ptr = (U8 *)Item->VirtAddr;
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
void USBH_HC_SetActivePortReset(HOST_CONTROLLER * pHost, HUB_PORT * pEnumPort) {
  T_ASSERT_MAGIC(pHost, HOST_CONTROLLER);
  if (NULL != pHost->ActivePortReset) {
    USBH_WARN((USBH_MTYPE_CORE, "Core: HcSetActivePortReset: ActivePortReset is not NULL!"));
  } else {
    T_ASSERT_MAGIC(pEnumPort, HUB_PORT);
    pHost->ActivePortReset = pEnumPort;
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
void USBH_HC_ClrActivePortReset(HOST_CONTROLLER * pHost, HUB_PORT * pEnumPort) {
  T_ASSERT_MAGIC(pHost, HOST_CONTROLLER);
  if (NULL == pHost->ActivePortReset) {
    USBH_WARN((USBH_MTYPE_CORE, "Core: HcClearActivePortReset: ActivePortReset already NULL!"));
  } else {
    T_ASSERT_MAGIC(pEnumPort, HUB_PORT);
    if (pHost->ActivePortReset != pEnumPort) {
      USBH_WARN((USBH_MTYPE_CORE, "Core: HcClearActivePortReset: not the same port as at the start!"));
    }
    pHost->ActivePortReset = NULL;
  }
}

/******************************* EOF ********************************/
