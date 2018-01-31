/*********************************************************************
*  
*   IAR PowerPac
*
*   (c) Copyright IAR Systems 2010.  All rights reserved.
*
**********************************************************************
----------------------------------------------------------------------
File        : USBH_Int.h
Purpose     : Internals used accross different layers of the USB device stack
---------------------------END-OF-HEADER------------------------------
*/

#ifndef _USBH_INT_H_ // Avoid multiple/recursive inclusion

#define _USBH_INT_H_

#include <stdlib.h>  // for atoi(), exit()
#include <string.h>
#include <stdio.h>

#include "Segger.h"
#include "USBH.h"
#include "USBH_ConfDefaults.h"

#if defined(__cplusplus)
  extern "C" { // Make sure we have C-declarations in C++ programs
#endif

#ifdef USBHCORE_C
  #define EXTERN
#else
  #define EXTERN extern
#endif

#if USBH_SUPPORT_LOG
  #define USBH_LOG(p) USBH_Logf p
#else
  #define USBH_LOG(p)
#endif

#if USBH_SUPPORT_WARN
  #define USBH_WARN(p) USBH_Warnf p
#else
  #define USBH_WARN(p)
#endif

#if USBH_DEBUG >= 3
  #define USBH_WARN_INTERNAL(p) USBH_Warnf p
#else
  #define USBH_WARN_INTERNAL(p)
#endif

// Useful macros:
#define USBH_MIN(x,y)    ((x)  <  (y)   ?  (x)   :  (y))
#define USBH_MAX(x,y)    ((x)  >  (y)   ?  (x)   :  (y))
#define USBH_COUNTOF(a)  (sizeof(a)/sizeof(a[0]))

/*********************************************************************
*
*       USBH_GLOBAL
*/

typedef struct USBH_GLOBAL {
  U8                      ConfigCompleted;
  U8                      InitCompleted;
  USBH_HC_HANDLE          hHC;
  USBH_HC_BD_HANDLE       hHCBD;
  USBH_HOST_DRIVER_INST   DriverInst;
  USBH_HOST_DRIVER      * pDriver;
  struct {
    U32 TransferBufferSize;
    U8  NumRootHubs;
    U8  RootHubPortsAlwaysPowered;
    U8  RootHubPerPortPowered;
    U8  RootHubSupportOvercurrent;
    U8  NumUSBDevices;
    U8  NumBulkEndpoints;
    U8  NumIntEndpoints;
    U8  NumIsoEndpoints;
    U8  SupportExternalHubs;
  } Config;
} USBH_GLOBAL;

EXTERN USBH_GLOBAL USBH_Global;

#define USBH_PRINT_STATUS_VALUE(Type, status) USBH_WARN((Type, "%s", USBH_GetStatusStr(status)))

#if (USBH_DEBUG > 1)
  #define USBH_ASSERT(condition)          if (!(condition)) { USBH_WARN((USBH_MTYPE_CORE, "\nASSERTION FAILED: %s(%d)\n", __FILE__, __LINE__)); }
  #define USBH_ASSERT_PTR(Ptr)               USBH_ASSERT(Ptr != NULL)
  #define USBH_ASSERT_MAGIC(ptr,type)        USBH_ASSERT(USBH_IS_PTR_VALID((ptr),type))
  #define USBH_ASSERT0                       USBH_WARN((USBH_MTYPE_CORE, "\nASSERT0: %s(%d)\n", __FILE__, __LINE__));
#else
  #define USBH_ASSERT(condition)
  #define USBH_ASSERT_PTR(Ptr)
  #define USBH_ASSERT_MAGIC(ptr, type)
  #define USBH_ASSERT0
#endif

#define FOUR_CHAR_ULONG(c1,c2,c3,c4)   (((U32)(c1)) | (((U32)(c2))<<8) | (((U32)(c3))<<16) | (((U32)(c4))<<24)) // Generates a magic ulong (four char code)
#define TWO_CHAR_USHORT(c1,c2)         (((U16)(c1)) | ( (U16)(c2) <<8))                                         // Generates a magic ulong (four char code)
#define GET_MASK_FROM_BITNUMBER(BitNb) (((U32)(1))<<(BitNb))
// Calculate the pointer to the base of an object given its type and a pointer to a field within the object.
#define USBH_ZERO_MEMORY(        mem,count)     USBH_MEMSET((mem), 0,   (count))
#define USBH_ZERO_STRUCT(        s)             USBH_ZERO_MEMORY(&(s),sizeof(s))
#define USBH_ZERO_ARRAY(         s)             USBH_ZERO_MEMORY( (s),sizeof(s))
#define USBH_ARRAY_ELEMENTS(     a)             (sizeof(a)     / sizeof(a[0]))
#define USBH_ARRAY_LIMIT(        a)             (&a[USBH_ARRAY_ELEMENTS(a)])

#if (USBH_DEBUG > 1)
  #define USBH_IS_PTR_VALID(p,type) ((p)!=NULL && (p)->Magic==type##_MAGIC) // Takes a pointer and its type and compares the Magic field with a constant
#else
  #define USBH_IS_PTR_VALID(ptr,type)
#endif

// Helper macro, used to convert enum constants to string values
// lint -save -e773
#define USBH_ENUM_TO_STR(e) (x==(e)) ? #e
// lint -restore
#define USBH_IS_ALIGNED(val,size) (((val) &  ((size)-1)) == 0)          // Returns true if the given value is aligned to a 'size' boundary
#define USBH_ALIGN_UP(  val,size) (((val) +  ((size)-1)) & ~((size)-1)) // Round up a value to the next 'size' boundary
#define USBH_ALIGN_DOWN(val,size) ( (val) & ~((size)-1))                // Round down a value to the next 'size' boundary

#if (USBH_DEBUG > 1)                                                  // Handy macro to enable code in debug builds only
  #define IFDBG(x) { x; }
#else
  #define IFDBG(x)
#endif

/* xyxy */
#define URB_BUFFER_POOL_MAGIC                   FOUR_CHAR_ULONG('T','P','O','O')
#define ENUM_ERROR_NOTIFICATION_MAGIC           FOUR_CHAR_ULONG('E','N','O','T')
#define USBH__PNP_NOTIFICATION_MAGIC            FOUR_CHAR_ULONG('P','N','P','N')
#define DELAYED_PNP_NOTIFY_CONTEXT_MAGIC        FOUR_CHAR_ULONG('P','N','P','D')
#define INTERFACE_ENTRY_MAGIC                   FOUR_CHAR_ULONG('I','F','A','E')
#define OHD_EP0_MAGIC                           FOUR_CHAR_ULONG('E','P','0','M')
#define USBH_OHCI_DEVICE_MAGIC                  FOUR_CHAR_ULONG('O','D','E','V')
#define USBH_STM32_INST_MAGIC                   FOUR_CHAR_ULONG('S','T','M','I')
#define DEFAULT_EP_MAGIC                        FOUR_CHAR_ULONG('E','P','0',' ')
#define USB_ENDPOINT_MAGIC                      FOUR_CHAR_ULONG('E','N','D','P')
#define USB_INTERFACE_MAGIC                     FOUR_CHAR_ULONG('U','I','F','U')
#define USB_DEVICE_MAGIC                        FOUR_CHAR_ULONG('U','D','E','V')
#define USBH_HOST_DRIVER_INST_MAGIC             FOUR_CHAR_ULONG('U','D','R','V')
#define ROOT_HUB_MAGIC                          FOUR_CHAR_ULONG('R','H','U','B')
#define USB_HUB_MAGIC                           FOUR_CHAR_ULONG('U','H','U','B')
#define USBH_HUB_PORT_MAGIC                     FOUR_CHAR_ULONG('P','O','R','T')
#define USBH_HOST_CONTROLLER_MAGIC              FOUR_CHAR_ULONG('H','O','S','T')
#define USBH_HCM_POOL_MAGIC                     FOUR_CHAR_ULONG('P','O','O','L')
#define USBH_HCM_ITEM_HEADER_MAGIC                   FOUR_CHAR_ULONG('I','T','E','M')


#define USBH_HCM_POOL_VALID(pPool)               USBH_ASSERT(USBH_IS_PTR_VALID((pPool),       USBH_HCM_POOL))
#define USBH_HCM_ASSERT_ITEM_HEADER(pItemHeader) USBH_ASSERT(USBH_IS_PTR_VALID((pItemHeader), USBH_HCM_ITEM_HEADER))
#define USBH_OCHI_IS_DEV_VALID(pDev)        USBH_ASSERT(USBH_IS_PTR_VALID(pDev, USBH_OHCI_DEVICE))
#define USBH_OHCI_HANDLE_TO_PTR(pDev,USBH_hc_handle)     (pDev) = ((USBH_OHCI_DEVICE *)(USBH_hc_handle))
#define USBH_STM32_IS_DEV_VALID(pDev)       USBH_ASSERT(USBH_IS_PTR_VALID(pDev, USBH_STM32_INST))
#define USBH_STM32_HANDLE_TO_PTR(pDev,USBH_hc_handle)     (pDev) = ((USBH_STM32_INST *)(USBH_hc_handle))

// Some Macros used for calculation of structure pointers.

// Calculate the byte offset of a field in a structure of type type.
// @func long | STRUCT_FIELD_OFFSET |
//   This macro calculates the offset of <p field> relative to the base of the structure <p type>.
// @parm IN<spc>| type |
//   Type name of the structure
// @parm IN<spc>| field |
//   Field name
// @rdesc
//   Offset of the field <p field> relative to the base of the structure <p type>.
// lint -emacro({413},STRUCT_FIELD_OFFSET)
// lint -emacro({613},STRUCT_FIELD_OFFSET)
#define STRUCT_FIELD_OFFSET(type, field)((long)&(((type *)0)->field) )

// Calculate the pointer to the base of the structure given its type and a pointer to a field within the structure.
// @func (type *) | STRUCT_BASE_POINTER |
//   This macro calculates the pointer to the base of the structure given its type and a pointer to a field within the structure.
// @parm IN<spc>| fieldptr |
//   Pointer to the field <p field> of the structure
// @parm IN<spc>| type |
//   Type name of the structure
// @parm IN<spc>| field |
//   Field name
// @rdesc
//   Address of the structure which contains <p field>.
// @comm
//   The returned pointer is of type 'pointer to <p type>'.
// lint -emacro({413},STRUCT_BASE_POINTER)
// lint -emacro({613},STRUCT_BASE_POINTER)
#define STRUCT_BASE_POINTER(fieldptr, type, field)((type *)(((char *)(fieldptr)) - ((char *)(&(((type *)0)->field)))))



// Needs the struct and the name of the list entry inside the struct
#define GET_HCMITEM_FROM_ENTRY(pListEntry)                    STRUCT_BASE_POINTER(pListEntry, USBH_HCM_ITEM_HEADER,            Link.ListEntry)
#define GET_BUFFER_FROM_ENTRY(pListEntry)                     STRUCT_BASE_POINTER(pListEntry, URB_BUFFER,                 ListEntry)
#define GET_ENUM_ERROR_NOTIFICATION_FROM_ENTRY(pListEntry)    STRUCT_BASE_POINTER(pListEntry, ENUM_ERROR_NOTIFICATION,    ListEntry)
#define GET_PNP_NOTIFICATION_FROM_ENTRY(pListEntry)           STRUCT_BASE_POINTER(pListEntry, USBH__PNP_NOTIFICATION,     ListEntry)
#define GET_DELAYED_PNP_NOTIFY_CONTEXT_FROM_ENTRY(pListEntry) STRUCT_BASE_POINTER(pListEntry, DELAYED_PNP_NOTIFY_CONTEXT, ListEntry)
#define GET_INTERFACE_ENTRY_FROM_ENTRY(pListEntry)            STRUCT_BASE_POINTER(pListEntry, INTERFACE_ENTRY,            ListEntry)
#define GET_CONTROL_EP_FROM_ENTRY(pListEntry)                 STRUCT_BASE_POINTER(pListEntry, USBH_OHCI_EP0,              ListEntry)
#define GET_HCM_ITEM_HEADER_FROM_ENTRY(pListEntry)            STRUCT_BASE_POINTER(pListEntry, USBH_HCM_ITEM_HEADER,            Link.ListEntry)
#define GET_URB_HEADER_FROM_ENTRY(pListEntry)                 STRUCT_BASE_POINTER(pListEntry, USBH_HEADER,                ListEntry)
#define GET_CONTROL_EP_FROM_ENTRY(pListEntry)                 STRUCT_BASE_POINTER(pListEntry, USBH_OHCI_EP0,              ListEntry)
#define GET_BULKINT_EP_FROM_ENTRY(pListEntry)                 STRUCT_BASE_POINTER(pListEntry, USBH_OHCI_BULK_INT_EP,            ListEntry)
#define GET_ISO_EP_FROM_ENTRY(pListEntry)                     STRUCT_BASE_POINTER(pListEntry, USBH_OHCI_ISO_EP,           ListEntry)
#define GET_HUB_PORT_PTR(pListEntry)                          STRUCT_BASE_POINTER(pListEntry, USBH_HUB_PORT,              ListEntry)
#define GET_BULKINT_EP_FROM_ENTRY(pListEntry)                 STRUCT_BASE_POINTER(pListEntry, USBH_OHCI_BULK_INT_EP,            ListEntry)
#define GET_USB_DEVICE_FROM_ENTRY(pListEntry)                 STRUCT_BASE_POINTER(pListEntry, USB_DEVICE,                 ListEntry)
#define GET_USB_DEVICE_FROM_TEMP_ENTRY(pListEntry)            STRUCT_BASE_POINTER(pListEntry, USB_DEVICE,                 TempEntry)
#define GET_HOST_CONTROLLER_FROM_ENTRY(pListEntry)            STRUCT_BASE_POINTER(pListEntry, USBH_HOST_CONTROLLER,       ListEntry)
#define GET_USB_ENDPOINT_FROM_ENTRY(pListEntry)               STRUCT_BASE_POINTER(pListEntry, USB_ENDPOINT,               ListEntry)
#define GET_USB_INTERFACE_FROM_ENTRY(pListEntry)              STRUCT_BASE_POINTER(pListEntry, USB_INTERFACE,              ListEntry)

typedef struct URB_BUFFER_POOL {
  U32                   Magic;
  USBH_DLIST            ListEntry;
  U32                   NumberOfBuffer; // Allocated number number of buffers in pool
  U32                   BufferCt;       // Number of buffers in buffer pool
  U32                   Size;           // Size of one buffer in bytes
  USBH_INTERFACE_HANDLE hInterface;
  U8                    Endpoint;
  U32                   Index;
  int                   ResetFlag;
  int                   BusMasterMemoryFlag;
} URB_BUFFER_POOL;


typedef struct URB_BUFFER {
  USBH_DLIST        ListEntry;
  U8              * pTransferBuffer; // Transfer buffer
  USBH_URB          Urb;            // Allocated URB
  U32               Size;           // Size of buffer in bytes
  URB_BUFFER_POOL * pPool;           // Owning pool
  U32               Index;          // Index number for debugging
} URB_BUFFER;

URB_BUFFER_POOL * USBH_URB_CreateTransferBufferPool(USBH_INTERFACE_HANDLE IfaceHandle, U8 Endpoint, U32 SizePerBuffer, U32 BufferNumbers, int BusMasterTransferMemoryFlag);

URB_BUFFER * USBH_URB_GetFromTransferBufferPool  (URB_BUFFER_POOL * Pool);
void         USBH_URB_PutToTransferBufferPool    (URB_BUFFER      * Buffer);
void         USBH_URB_DeleteTransferBufferPool   (URB_BUFFER_POOL * Pool);
void         USBH_URB_InitUrbBulkTransfer        (URB_BUFFER      * Buffer, USBH_ON_COMPLETION_FUNC * pfOnCompletion, void * Context);
U32          USBH_URB_GetPendingCounterBufferPool(URB_BUFFER_POOL * Pool);

// Allocates always USBH_TRANSFER_BUFFER_ALIGNMENT aligned transfer buffer from the heap
void * USBH_URB_BufferAllocateTransferBuffer(U32 size);
// Frees buffer allocated with USBH_URB_BufferAllocateTransferBuffer
void   USBH_URB_BufferFreeTransferBuffer(void * pMemBlock);

// This macro need the struct and the name of the list entry inside the struct.

typedef struct ENUM_ERROR_NOTIFICATION {
#if (USBH_DEBUG > 1)
  U32                       Magic;
#endif
  USBH_DLIST                ListEntry;         // To store this object in the BUS_DRIVER object
  void                    * pContext;           // User context / A copy of the parameter passed to USBH_RegisterEnumErrorNotification
  USBH_ON_ENUM_ERROR_FUNC * pfOnEnumError;
} ENUM_ERROR_NOTIFICATION;

void   UbdFireEnumErrorNotification                (USBH_ENUM_ERROR * pEnumError);                                                          // Walk trough the device driver enum error notification list and call registered notify callback routines!
void   UbdSetEnumErrorNotificationRootPortReset    (USBH_HUB_PORT   * pPort, USBH_ROOT_HUB_PORTRESET_STATE state,  USBH_STATUS status);      // Notify about an root hub pPort reset error
void   UbdSetEnumErrorNotificationProcessDeviceEnum(USBH_HUB_PORT   * pPort, DEV_ENUM_STATE state,      USBH_STATUS status, int hub_flag);   // Notify about a USB device enumeration error
void   UbdSetEnumErrorNotificationHubPortReset(     USBH_HUB_PORT   * pPort, USBH_HUB_PORTRESET_STATE state, USBH_STATUS status);  // Notify about an external hub pPort reset error

#define USBH_MAX_RECURSIVE 20

#if (USBH_DEBUG > 1)
  #define INC_RECURSIVE_CT(funcname)                                                   \
    static int USBH_recursive_ct;                                                      \
    USBH_recursive_ct++;                                                               \
    if (USBH_recursive_ct > USBH_MAX_RECURSIVE) {                                      \
      USBH_WARN((USBH_MTYPE_CORE, ""#funcname ":recursive-ct:%ld",USBH_recursive_ct)); \
    }

  // The second test is only for testing of the macro
  #define DEC_RECURSIVE_CT(funcname)                                                        \
    if(0>=USBH_recursive_ct){                                                               \
      USBH_WARN((USBH_MTYPE_CORE, ""#funcname ":recursive <= 0 ct:%ld",USBH_recursive_ct)); \
    }                                                                                       \
    USBH_recursive_ct--
#else
  #define INC_RECURSIVE_CT(funcname)
  #define DEC_RECURSIVE_CT(funcname)
#endif

// Return the pointer to the beginning of the descriptor or NULL if not Desc. is found
// const void * PrevDesc - Pointer to a descriptor
// int * Length          - IN:  Remaining bytes from Desc.
//                         OUT: If the descriptor is found then that is the remaining length from the beginning of the returned descriptor
// int DescType          - Descriptor type, see USB spec
const void * USBH_SearchNextDescriptor(const void * PrevDesc, int * Length, int DescType);


// This macro need the struct and the name of the list entry inside the struct.

typedef struct USBH__PNP_NOTIFICATION { // The USB device object
#if (USBH_DEBUG > 1)
  U32                   Magic;
#endif
  USBH_DLIST                 ListEntry;       // To store this object in the BUS_DRIVER object
  USBH_PNP_NOTIFICATION UbdNotification; // A copy of the notification passed to USBH_RegisterPnPNotification
} USBH__PNP_NOTIFICATION;


// Used for indirect calling of the user notification routine
typedef struct DELAYED_PNP_NOTIFY_CONTEXT {
#if (USBH_DEBUG > 1)
  U32 Magic;
#endif
  // To store this object in the BUS_DRIVER object
  USBH_DLIST               ListEntry;
  void                   * pContext;
  USBH_PNP_EVENT           Event;
  USBH_ON_PNP_EVENT_FUNC * NotifyCallback;
  USBH_INTERFACE_ID        Id;
} DELAYED_PNP_NOTIFY_CONTEXT;

USBH__PNP_NOTIFICATION * USBH_PNP_NewNotification    (USBH_PNP_NOTIFICATION  * pfOnUbdNotification);
void                     USBH_PNP_ReleaseNotification(USBH__PNP_NOTIFICATION * pfOnPnpNotification);

// If this interface matches with the interface Mask of pfOnPnpNotification the event notification function is called with the event.
// Parameters:
//   pfOnPnpNotification: Pointer to the notification
//   pDev:             Pointer to an device
//   event:           device is connected, device is removed!
//                    Normally one device at the time is changed!
void USBH_PNP_ProcessDeviceNotifications(USBH__PNP_NOTIFICATION * pfOnPnpNotification, USB_DEVICE * pDev, USBH_PNP_EVENT Event);

// Check the notification against all interfaces. If an device is removed or connected and the interface matches
// and the event has been not sent the notification function is called.
void USBH_PNP_ProcessNotification(USBH__PNP_NOTIFICATION * PnpNotification);
void USBH_PNP_NotifyWrapperCallbackRoutine(void    * Context);

// This macro need the struct and the name of the list entry inside the struct

// The interface list object based on one host controller!
typedef struct INTERFACE_LIST {
  USBH_DLIST   UsbInterfaceEntryList; // List for interfaces of type INTERFACE_ENTRY
  unsigned int InterfaceCount;        // Number of entries in the UsbInterfaceList
} INTERFACE_LIST;

// the entry to keep this object in the InterfaceList
typedef struct INTERFACE_ENTRY {
#if (USBH_DEBUG > 1)
  U32                      Magic;
#endif
  USBH_DLIST               ListEntry;
  USBH_HOST_CONTROLLER   * HostController; // Pointer to the owning host controller
  USBH_INTERFACE_ID        InterfaceID;    // The interface ID
} INTERFACE_ENTRY;

/*********************************************************************
*
*       OHCI specific
*
**********************************************************************
*/
#define OH_ISO_VALID(OHD_ISO_EP_Ptr)              USBH_HCM_ASSERT_ITEM_HEADER(&OHD_ISO_EP_Ptr->ItemHeader)
#define OH_BULKINT_VALID(OHD_BULK_INT_EP_Ptr)     USBH_HCM_ASSERT_ITEM_HEADER(&OHD_BULK_INT_EP_Ptr->ItemHeader)
#define OH_EP0_VALID(OHD_EP0_Ptr)                 USBH_HCM_ASSERT_ITEM_HEADER(&OHD_EP0_Ptr->ItemHeader)
#define OH_ASSERT_PORT_NUMBER(devPtr,PortNumber)  USBH_ASSERT((PortNumber) != 0); USBH_ASSERT((PortNumber) <= (devPtr) -> RootHub.PortCount)


//lint -emacro((826),OhcWriteReg)
// Base must be an character pointer.
#define OhHalWriteReg(Base, Offset, Value) USBH_WriteReg32 (((Base) + (Offset)), (Value))
//lint -emacro((826),OhcReadReg)
#define OhHalReadReg( Base, Offset)        USBH_ReadReg32  (((Base) + (Offset)))
#define OhHalTestReg( Base, Offset, Mask) (0 != (OhHalReadReg((Base), (Offset)) & (Mask)) ? TRUE : FALSE)

#define OhHalSetReg(Base,Offset, Mask)                      \
  { U32    temp;                                            \
    temp = OhHalReadReg((Base), (Offset));                  \
    OhHalWriteReg      ((Base), (Offset), (temp | (Mask))); \
  }

#define OhHalClrReg(Base,Offset, Mask)            \
  { U32 temp;                                     \
    temp  = OhHalReadReg((Base), (Offset));       \
    temp &= ~(U32)(Mask);                         \
    OhHalWriteReg       ((Base), (Offset), temp); \
  }

U8   USBH_ReadReg8  (U8  * pAddr);
U16  USBH_ReadReg16 (U16 * pAddr);
void USBH_WriteReg32(U8  * pAddr, U32 Value);
U32  USBH_ReadReg32 (U8  * pAddr);

// Control endpoint states
typedef enum USBH_EP0_PHASE {
  ES_IDLE   = 0,
  ES_SETUP,
  ES_DATA,
  ES_COPY_DATA,
  ES_PROVIDE_HANDSHAKE,
  ES_HANDSHAKE,
  ES_ERROR
} USBH_EP0_PHASE;

typedef struct SETUP_BUFFER {
  // Recommended!!!:
  //   first filed:  USBH_HCM_ITEM_HEADER
  //   second field: U8 EndpointType
  USBH_HCM_ITEM_HEADER ItemHeader;
} SETUP_BUFFER;

#define OHD_MAX_TD 200         // Maximum number of all transfer descriptors

typedef enum T_OHD_TD_STATUS { // Events for the PnP function
  OH_TD_PENDING,               // The TD is pending
  OH_TD_COMPLETED,             // TD is complete
  OH_TD_CANCELED,              // TD is canceled
  OH_TD_EMPTY                  // The TD is not used
} OHD_TD_STATUS;

typedef enum T_OHD_TD_PID {
  OH_SETUP_PID = 0,
  OH_OUT_PID,
  OH_IN_PID
} USBH_OHCI_TD_PID;

// This bits in the OHCI TD and ISO TD DWORD 0 are not modified and are used
#define OHCI_TD_DONE_MASK GET_MASK_FROM_BITNUMBER(OHCI_TD_NOT_USED_BIT_1)
#define OHCI_TD_ISO_MASK  GET_MASK_FROM_BITNUMBER(OHCI_TD_NOT_USED_BIT_2)

// The logical general transfer descriptor object ! General Transfer descriptor of the host controller driver, this includes a
// memory pool object that contains the physical address of the host controller transfer descriptor.
typedef struct USBH_OHCI_INFO_GENERAL_TRANS_DESC {
  USBH_HCM_ITEM_HEADER  ItemHeader;        // The struct must always begin with an item header that includes the physical address
  OHD_TD_STATUS         Status;            // Current TD status
  USBH_BOOL             CancelPendingFlag; // True if the URB request has been canceled and this TD is waiting for cleanup!
  U32                   Size;              // Total number of bytes that are queued for this transfer
  U8                    EndpointType;      // the type of the endpoint, one of USB_EP_TYPE_CONTROL, ... used to find the endpoint list
  void                * pEp;               // Pointer to the endpoint to which the transfer is queued
} USBH_OHCI_INFO_GENERAL_TRANS_DESC;

// This tow operations are only valid on the TD if the TD not in the ED list of the host or the ED list is disabled!
#define OH_ED_SET_SKIP_BIT(edPtr) (edPtr->Dword0 |=       OHCI_ED_K)
#define OH_ED_CLR_SKIP_BIT(edPtr) (edPtr->Dword0 &= (U32)~OHCI_ED_K)
#define OH_MAX_PKT_SIZE_EP0_LOWSPEED  8
#define OH_MAX_PKT_SIZE_EP0           64
#define OH_MAX_PKT_SIZE_BULK          64
#define OH_MAX_PKT_SIZE_INT           64
#define OH_MAX_PKT_SIZE_ISO           1023

// Additional endpoint mask bits

// Endpoint flags field
#define OH_DUMMY_ED_EPFLAG    0x01UL
#define OH_SHORT_PKT_EPFLAG   0x01UL

// Endpoint states

typedef enum USBH_EP_STATE {
  OH_EP_IDLE,        // The endpoint is not linked
  OH_EP_UNLINK,      // If the timer routine runs then the endpoint is removed and deleted
  OH_EP_LINK,        // The endpoint is linked
  OH_EP_UNLINK_TIMER // Endpoint is unlinked but the current timer routine must restart the timer
} USBH_EP_STATE;

// The logical control EP object
typedef struct USBH_OHCI_EP0 {
  // Recommended!!!:
  //   first filed:  USBH_HCM_ITEM_HEADER
  //   second field: U8 EndpointType
  USBH_HCM_ITEM_HEADER              ItemHeader;
  U8                                EndpointType;        // Endpoint type
  USBH_OHCI_DEVICE                * pDev;                 // Backward pointer to the device
  USBH_EP_STATE                     State;
  USBH_DLIST                        ListEntry;           // USBH_OHCI_EP0 list
  USBH_DLIST                        UrbList;             // submitted URB list
  U16                               UrbCount;            // number of requests
  USBH_URB                        * pPendingUrb;          // pending URB
  U16                               TdCounter;           // number of TDs on this endpoint
  U8                                AbortMask;
  USBH_EP0_PHASE                    Ep0Phase;            // pSetup, data or handshake phase
  U8                              * pSetup;               // pointer to the address of the buffer in pSetupPacket
  SETUP_BUFFER                    * pSetupPacket;         // pointer to an HCM pool entry which contains the pSetup packet address
  USBH_OHCI_TRANSFER_BUFFER       * pDataPhaseCopyBuffer; // buffer used during data phase,  pointer is only valid in the data phase
  USBH_RELEASE_EP_COMPLETION_FUNC * pfReleaseCompletion;
  void                            * pReleaseContext;
  U32                               Mask;
  U8                                DeviceAddress;
  U8                                EndpointAddress;
  U16                               MaxPacketSize;      // Maximum packet size for that endpoint
  USBH_SPEED                        Speed;
} USBH_OHCI_EP0;


typedef struct USBH_OHCI_BULK_INT_EP { // Logical bulk and interrupt EP object
  // Recommended:
  //   First  field: USBH_HCM_ITEM_HEADER
  //   Second field: U8 EndpointType
  USBH_HCM_ITEM_HEADER              ItemHeader;
  U8                                EndpointType;
  USBH_OHCI_DEVICE                * pDev;
  USBH_EP_STATE                     State;
  USBH_DLIST                        ListEntry;         // The entry to keep the element in the HC list
  USBH_DLIST                        UrbList;           // Submitted URB list
  U16                               UrbCount;          // Number of requests
  USBH_URB                        * pPendingUrb;       // Active URB  removed from the list
  USBH_OHCI_TRANSFER_BUFFER       * pCopyBuffer;       // If URB's buffer address lies out of transfer memory range the buffer is copied
  U16                               UrbTotalTdNumber;  // Number of Tds for the current URB
  U16                               UrbDoneTdNumber;   // Current number of doen tds
  U16                               TdCounter;         // Current number of TDs on this ED
  U16                               UpDownTDCounter;   // DoneTDCounter is used in the done interrupt endpoint functions
  U8                                AbortMask;         // Current abort state  see also EP_SKIP_TIMEOUT_MASK and EP_ABORT_MASK

  U8                                CancelPendingFlag; // TRUE if pfAbortEndpoint is called
  USBH_OHCI_DUMMY_INT_EP          * pDummyIntEp;       // pDummyIntEp holds an backward pointer to an dummy interrupt endpoint
  USBH_RELEASE_EP_COMPLETION_FUNC * pfOnReleaseCompletion; // Callback function that is called if the endpoint is removed
  void                            * pReleaseContext;
  U32                               Flags;             // Endpoint flags
  // Members for operation
  U8                                DeviceAddress;
  U8                                EndpointAddress;
  U16                               MaxPacketSize;        // Maximum transfer fifo size in the host controller for that endpoint
  USBH_SPEED                        Speed;
  U16                               IntervalTime;
  U8                                HaltFlag;          // Set in DONE routine if HALT condition can not deleted! Reset only with an endpoint reset.
// This flag prevents submitting of new URBs!
} USBH_OHCI_BULK_INT_EP;

typedef struct USBH_OHCI_ISO_EP { // Logical ISO EP object
  // Recommended:
  //   First  field: USBH_HCM_ITEM_HEADER
  //   Second field: U8 EndpointType
  USBH_HCM_ITEM_HEADER              ItemHeader;
  USBH_OHCI_DEVICE                * pDev;
  USBH_EP_STATE                     State;
  USBH_DLIST                        ListEntry;         // The entry to keep the element in the HC list
  USBH_DLIST                        UrbList;           // Submitted URB list
  U16                               UrbCount;          // Number of requests
  USBH_URB                        * pPendingUrb;       // Active URB  removed from the list
  USBH_OHCI_TRANSFER_BUFFER       * pCopyBuffer;       // If URB's buffer address lies out of transfer memory range the buffer is copied
  U16                               UrbTotalTdNumber;  // Number of Tds for the current URB
  U16                               UrbDoneTdNumber;   // Current number of doen tds
  U16                               TdCounter;         // Current number of TDs on this ED
  U16                               UpDownTDCounter;   // DoneTDCounter is used in the done interrupt endpoint functions
  U8                                AbortMask;         // Current abort state  see also EP_SKIP_TIMEOUT_MASK and EP_ABORT_MASK
  U8                                CancelPendingFlag; // TRUE if pfAbortEndpoint is called
  USBH_RELEASE_EP_COMPLETION_FUNC * pfOnReleaseCompletion; // Callback function that is called if the endpoint is removed
  void                            * pReleaseContext;
  U32                               Flags;             // Endpoint flags
  // Members for operation
  U8                                DeviceAddress;
  U8                                EndpointAddress;
  U16                               MaxPacketSize;        // Maximum transfer fifo size in the host controller for that endpoint
  USBH_SPEED                        Speed;
  U16                               IntervalTime;
  U8                                HaltFlag;          // Set in DONE routine if HALT condition can not deleted! Reset only with an endpoint reset.
  USBH_OHCI_DUMMY_INT_EP          * pDummyIntEp;       // pDummyIntEp holds an backward pointer to an dummy interrupt endpoint
  // This flag prevents submitting of new URBs!
} USBH_OHCI_ISO_EP;


struct USBH_OHCI_DUMMY_INT_EP { // Logical bulk and interrupt EP object
  // Recommended!!!:
  //   First  field: USBH_HCM_ITEM_HEADER
  //   Second field: U8 EndpointType
  USBH_HCM_ITEM_HEADER   ItemHeader;
  U8                     EndpointType;  // Endpoint type
  USBH_OHCI_DEVICE     * pDev;        // Entry to keep the element in the HC list
  USBH_EP_STATE          State;
  USBH_DLIST             ActiveList;    // USBH_OHCI_BULK_INT_EP (user endpoint) list
  U32                    Bandwidth;     // Sum of max packet sizes all appended user endpoints
  void                 * pNextDummyEp;  // Points to an NULL pointer if this is the 1ms interval or to the next dummy endpoint.
  U16                    IntervalTime;  // Interval time 1,2 4,..ms
  U8                     Mask;          // Bits see usbohc_epglob.h
};

/*********************************************************************
*
*       Endpoint descriptor
*
**********************************************************************
*/

typedef struct USBH_OHCI_ED {
  volatile U32 Dword0;
  volatile U32 TailP;
  volatile U32 HeadP;
  volatile U32 NextED;
} USBH_OHCI_ED;

#define OH_ED_SIZE          (sizeof(USBH_OHCI_ED))
#define OH_ED_ALIGNMENT     16
#define OHCI_ED_FA          0x0000007fUL // Function addresses
#define OHCI_ED_EN          0x00000780UL // Endpoint number
#define OHCI_ED_EN_BIT      7
#define OHCI_ED_D           0x00001800UL // direction
#define OHCI_ED_DIR_BIT     11
#define OHCI_ED_S           0x00002000UL // Low Speed active
#define OHCI_ED_K           0x00004000UL // Skip
#define OHCI_ED_F           0x00008000UL // Format (iso)
#define OHCI_ED_MPS         0x07ff0000UL // Maximum packet size
#define OHCI_ED_RSV         0xf8000000UL // Reserved bits
#define OHCI_ED_DIR_FROM_TD 0x00
#define OHCI_ED_OUT_DIR     0x01
#define OHCI_ED_IN_DIR      0x02

// Mask on ED fields HeadP
#define OHCI_ED_C           0x02UL // Last data toggle carry bit
#define OHCI_ED_H           0x01UL // ED halt bit

/*********************************************************************
*
*       General transfer descriptor
*
**********************************************************************
*/

typedef struct USBH_OHCI_TRANSFER_DESC {
  volatile U32 Dword0;
  volatile U32 CBP;
  volatile U32 NextTD;
  volatile U32 BE;
} USBH_OHCI_TRANSFER_DESC;

#define OH_GTD_SIZE         (sizeof(USBH_OHCI_TRANSFER_DESC))
#define OH_GTD_ALIGNMENT    16
#define OHCI_TD_RSV         0x0003ffff // Reserved values
#define OHCI_TD_R           0x00040000 // Buffer rounding bit
#define OHCI_TD_DP          0x00180000 // PID
#define OHCI_TD_PID_BIT     19
#define OHCI_TD_DI          0x00e00000 // Delay interrupt
#define OHCI_TD_DI_BIT      21
#define OHCI_TD_T           0x03000000 // Toggle Mask MSB=1 -> toggle value taken from LSB bit 24
#define OHCI_TD_T_MSB       0x02000000
#define OHCI_TD_T_LSB       0x01000000
#define OHCI_TD_T_BIT       24

// Forces an DATA 0 or DATA 1 toggle bit at the next start or transfer if not used the toggle bit is taken fro the endpoint
#define OHCI_TD_FORCE_DATA0 0x02000000 // DATA0
#define OHCI_TD_FORCE_DATA1 0x03000000 // DATA1
#define OHCI_TD_EC          0x0c000000 // Error Counter
#define OHCI_TD_CC          0xf0000000 // Condition code
#define OHCI_TD_CC_BIT      28         // Condition code code starts with LSB bit 28

/*********************************************************************
*
*       Isochronous transfer descriptor
*
**********************************************************************
*/

typedef struct USBH_OHCI_ISO_TRANS_DESC {
  volatile U32 Dword0;
  volatile U32 Dword1;
  volatile U32 NextTD;
  volatile U32 BE;
  volatile U16 OfsPsw[8];
} USBH_OHCI_ISO_TRANS_DESC;

#define OH_ISO_TD_SIZE          (sizeof(USBH_OHCI_ISO_TRANS_DESC))
#define OH_ISO_TD_ALIGNMENT     32
// Dword0
#define OHCI_TDI_SF             0x0000ffffUL
#define OHCI_TDI_RSV            0x001f0000UL
#define OHCI_TDI_DI             0x00e00000UL
#define OHCI_TDI_FC             0x07000000UL
#define OHCI_TDI_RSV1           0x08000000UL
#define OHCI_TDI_CC             0xf0000000UL
// Dword1
#define OHCI_TDI_BP0            0xfffff000UL
#define OHCI_TDI_RSV2           0x00000fffUL
// Following TD bits can be used for own information
#define OHCI_TD_NOT_USED_BIT_1  16
#define OHCI_TD_NOT_USED_BIT_2  17

/*********************************************************************
*
*       Host Controller Communications area HCCA
*
**********************************************************************
*/

typedef struct USBH_OHCI_HCCA_REG { // 256 bytes
  volatile U32 InterruptTable[32];
  volatile U16 FrameNumber;
  volatile U16 Pad1;
  volatile U32 DoneHead;
  volatile U32 Reserved[30];
} USBH_OHCI_HCCA_REG;

#define OH_DONE_HEAD_INT_MASK  0x00000001UL // Set if another OHCI interrupt is occurred

/*********************************************************************
*
*       Host Controller Register
*
**********************************************************************
*/

// Number of downstream ports
//#define RH_NDP                  2
#define RH_NDP                  3 // VOS 15.08.01
#define OH_REVISION_MASK        0xff
#define OH_REVISION             0x10
// There are three downstream ports

// OHCI Register
#define OH_REG_REVISION         0x000
#define OH_REG_CONTROL          0x004
#define OH_REG_COMMANDSTATUS    0x008
#define OH_REG_INTERRUPTSTATUS  0x00c
#define OH_REG_INTERRUPTENABLE  0x010
#define OH_REG_INTERRUPTDISABLE 0x014
#define OH_REG_HCCA             0x018
#define OH_REG_PERIODCURRENTED  0x01c
#define OH_REG_CONTROLHEADED    0x020
#define OH_REG_CONTROLCURRENTED 0x024
#define OH_REG_BULKHEADED       0x028
#define OH_REG_BULKCURRENTED    0x02c
#define OH_REG_DONEHEAD         0x030
#define OH_REG_FMINTERVAL       0x034
#define OH_REG_FMREMAINING      0x038
#define OH_REG_FMNUMBER         0x03c
#define OH_REG_PERIODICSTART    0x040
#define OH_REG_LSTHRESHOLD      0x044
#define OH_REG_RHDESCRIPTORA    0x048
#define OH_REG_RHDESCRIPTORB    0x04c
#define OH_REG_RHSTATUS         0x050
#define OH_REG_RHPORTSTATUS     0x054

#define GET_PORTSTATUS_REG(pPort) (OH_REG_RHPORTSTATUS + (((pPort)-1) << 2))  // Return the pPort status register param: Port: Portindex is an one based index

#define HC_CONTROL_CBSR       0x00000003UL                                   // Control bulk ratio
#define HC_CONTROL_PLE        0x00000004UL                                   // Periodic list enable
#define HC_CONTROL_IE         0x00000008UL                                   // Isochronous enable
#define HC_CONTROL_CLE        0x00000010UL                                   // Control list enable
#define HC_CONTROL_BLE        0x00000020UL                                   // Bulk list enable
#define HC_CONTROL_HCFS       0x000000c0UL                                   // Host functional state mask
#define HC_CONTROL_IR         0x00000100UL                                   // Interrupt routing enable
#define HC_CONTROL_RWC        0x00000200UL                                   // Indicates whether HC supports remote wakeup signaling.
#define HC_CONTROL_RWE        0x00000400UL                                   // Enable remote wakeup
#define HC_USB_RESET          0x00
#define HC_USB_RESUME         0x40
#define HC_USB_OPERATIONAL    0x80
#define HC_USB_SUSPEND        0xc0
#define HC_COMMAND_STATUS_HCR 0x00000001UL // Host reset
#define HC_COMMAND_STATUS_CLF 0x00000002UL // Control list filled
#define HC_COMMAND_STATUS_BLF 0x00000004UL // Bulk list filled
#define HC_COMMAND_STATUS_OCR 0x00000008UL // Ownership change request
#define HC_COMMAND_STATUS_SOC 0x00030000UL // Scheduling overrun

// HcInterruptEnable / Disable HCInterruptStatus
#define HC_INT_SO   0x00000001UL                     // Scheduling overrun
#define HC_INT_WDH  0x00000002UL                     // HcDoenHead
#define HC_INT_SF   0x00000004UL                     // SOF
#define HC_INT_RD   0x00000008UL                     // Resume detect
#define HC_INT_UE   0x00000010UL                     // Unrecoverable error
#define HC_INT_FNO  0x00000020UL                     // Frame number overflow, resume no frame number generation
#define HC_INT_RHSC 0x00000040UL                     // Root hub status change
#define HC_INT_OC   0x40000000UL                     // Ownership change
#define HC_INT_MIE  0x80000000UL                     // Master interrupt bit

#define HC_INT_STATUS_VALIDATION_BIT (1UL << 31) // Bit in the HC_INT_STATUS_VALID_MASK in the interrupt status register must be zero
#define HC_INT_STATUS_MASK_WITHOUT_OWNERCHIP 0x07f

// Frame interval
#define HC_FM_INTERVAL_FIT        (1UL<<31)
#define HC_FM_INTERVAL_FSMPS_BIT  16
#define HC_FM_INTERVAL_FSMPS_MASK 0x7FFF0000UL
#define HC_FM_INTERVAL_FI_MASK    0x00003FFFUL


// Interrupt intervals
#define OHD_1MS   1
#define OHD_2MS   2
#define OHD_4MS   4
#define OHD_8MS   8
#define OHD_16MS  16
#define OHD_32MS  32

#define OHD_DUMMY_INT_NUMBER  (OHD_1MS + OHD_2MS + OHD_4MS + OHD_8MS + OHD_16MS +  OHD_32MS)










/*********************************************************************
*
*       Externals
*
**********************************************************************
*/

// All timeouts in milliseconds!
#define POWERON_TO_POWERGOOD_TIME   150 // This time the library waits internally after detection of an connect

#if (POWERON_TO_POWERGOOD_TIME/2) > 0xff
  #error POWERON_TO_POWERGOOD_TIME
#endif


/*********************************************************************
*
*       Additional external hub definitions
*
**********************************************************************
*/

typedef struct USBH_OHCI_HUB_PORT { // pHub pPort object
  U8  Port;                     // One base pPort index
  U8  Power;                    // 1- power on 0 -power off
  U16 Status;                  // Port status and change bits
  U16 Change;
} USBH_OHCI_HUB_PORT;

typedef struct USBH_OHCI_ROOT_HUB {                       // Root pHub object
  USBH_OHCI_DEVICE                * pDev;
  USBH_ROOT_HUB_NOTIFICATION_FUNC * pfUbdRootHubNotification; // External init values and callback functions
  void                            * pRootHubNotificationContext;
  U16                               PortCount;
  U16                               PowerOnToPowerGoodTime;
  U16                               Status; // pHub status and change bits
  U16                               Change;
  USBH_OHCI_HUB_PORT              * apHcdPort;
} USBH_OHCI_ROOT_HUB;



typedef void REMOVE_HC_COMPLETION_FUNC (void * pContext);

#define OH_HCCA_VALID(OHD_HCCA_Ptr) USBH_HCM_ASSERT_ITEM_HEADER(&OHD_HCCA_Ptr->ItemHeader)

typedef struct USBH_OHCI_HCCA {  // Logical HCCA object
  USBH_HCM_ITEM_HEADER ItemHeader; // For dynamic allocating the struct must begin with the specific memory pool header
} USBH_OHCI_HCCA;

struct USBH_OHCI_DEVICE { // The global driver object. The object is cleared in the function USBH_HostInit!
#if (USBH_DEBUG > 1)
  U32 Magic;
#endif
  USBH_HCM_POOL                 TransferBufferPool; // If the transfer buffer in an URB out of physical memory address range then this buffer is transferred with an buffer from this pool
  // EP pools
  USBH_HCM_POOL                 IsoEPPool;
  USBH_HCM_POOL                 BulkEPPool;
  USBH_HCM_POOL                 IntEPPool;
  USBH_HCM_POOL                 DummyIntEPPool;
  USBH_HCM_POOL                 ControlEPPool;
  USBH_HCM_POOL                 SetupPacketPool;  // Setup buffer pool
  USBH_HCM_POOL                 GTDPool;          // GTD pool
  USBH_HCM_POOL                 IsoTDPool;        // ISO TD pool
  USBH_HCM_POOL                 HccaPool;         // HCCA pPool
  USBH_OHCI_HCCA              * pHcca;            // Pointer to the HccaPoolItem
  volatile USBH_OHCI_HCCA_REG * pOhHcca;      // OHCI HCCA memory, points to the base address of HCCA,  used to access the HCCA memory
  // Control endpoints
  USBH_DLIST                    ControlEpList; // Number of pending endpoints on the HC
  U32                           ControlEpCount;
  USBH_TIMER_HANDLE             ControlEpRemoveTimer;
  USBH_TIMER_HANDLE             ControlEpAbortTimer;
  USBH_BOOL                     ControlEpRemoveTimerRunFlag;    // True if timer is started, set to false in the timer routine
  USBH_BOOL                     ControlEpRemoveTimerCancelFlag;
  USBH_BOOL                     ControlEpAbortTimerRunFlag;
  USBH_BOOL                     ControlEpAbortTimerCancelFlag;
  // Bulk endpoints
  USBH_DLIST                    BulkEpList;
  U32                           BulkEpCount;
  USBH_TIMER_HANDLE             hBulkEpRemoveTimer;
  USBH_BOOL                     BulkEpRemoveTimerRunFlag;     // True if timer is started, set to false in the timer routine
  USBH_BOOL                     BulkEpRemoveTimerCancelFlag;
  USBH_TIMER_HANDLE             hBulkEpAbortTimer;
  USBH_BOOL                     BulkEpAbortTimerRunFlag;      // True if timer is started, set to false in the timer routine
  USBH_BOOL                     BulkEpAbortTimerCancelFlag;
  // Interrupt endpoints
  // The array index 0 contains the dummy EP of 1milliseconds, the index 1 the dummy EPs of 2 milliseconds and so on
  // IntervalStartEpIndex = Intervaltime - 1. Every dummy EP!
  USBH_OHCI_DUMMY_INT_EP      * DummyInterruptEpArr[OHD_DUMMY_INT_NUMBER];
  U32                           IntEpCount;                  // IntEpCount is an reference counter that counts the number of active interrupt endpoints without the dummy interrupt endpoints
  USBH_TIMER_HANDLE             hIntEpRemoveTimer;
  USBH_BOOL                     IntEpRemoveTimerRunFlag;     // True if timer is started, set to false in the timer routine
  USBH_BOOL                     IntEpRemoveTimerCancelFlag;
  USBH_TIMER_HANDLE             hIntEpAbortTimer;
  USBH_BOOL                     IntEpAbortTimerRunFlag;      // True if timer is started, set to false in the timer routine
  USBH_BOOL                     IntEpAbortTimerCancelFlag;
#if (USBH_DEBUG > 1)
  USBH_BOOL                     IntRemoveFlag;
  USBH_BOOL                     IsoRemoveFlag;
#endif
  // Iso endpoints
  USBH_DLIST                    IsoEpList;
  U32                           IsoEpCount;
  USBH_TIMER_HANDLE             hIsoEpRemoveTimer;
  USBH_BOOL                     IsoEpRemoveTimerRunFlag;     // True if timer is started, set to false in the timer routine
  USBH_BOOL                     IsoEpRemoveTimerCancelFlag;
  USBH_TIMER_HANDLE             hIsoEpAbortTimer;
  USBH_BOOL                     IsoEpAbortTimerRunFlag;      // True if timer is started, set to false in the timer routine
  USBH_BOOL                     IsoEpAbortTimerCancelFlag;
  // General information
  USBH_TIMER_HANDLE             hInitDeviceTimer;            // This timer is used in the initialization routine
  volatile U16                  UpperFrameCounter;           // UpperFrameCounter: valid bits: Bit 16..to Bit 31, LSB frame word: in the Hc area.
  volatile U16                  LastFrameCounter;
  U8                          * pRegBase;      // OHCI register base address
  U32                           FmIntervalReg; // Saved content from interval register
  USBH_OHCI_DEV_STATE           State;  // Devices state
  USBH_OHCI_ROOT_HUB            RootHub;
  USBH_HC_BD_HANDLE             hBusDriver;
  REMOVE_HC_COMPLETION_FUNC   * pfRemoveCompletion;   // pfHostExit Callback function and pContext
  void                        * pRemoveContext;
};

/*********************************************************************
*
*       Operations
*
**********************************************************************
*/
#define  USBH_OHC_CreateController USBH_OHCI_CreateController
#define  USBH_OHC_ServiceISR       USBH_OHCI_ServiceISR
#define  USBH_OHC_AddController    USBH_OHCI_AddController
#define  USBH_OHC_RemoveController USBH_OHCI_RemoveController
#define  USBH_OHC_DeleteController USBH_OHCI_DeleteController
#define  USBH_OHC_ProcessInterrupt USBH_OHCI_ProcessInterrupt


USBH_STATUS    USBH_OHCI_AddController             (USBH_HC_HANDLE       HcHandle, USBH_HC_BD_HANDLE    * HcBdHandle);
USBH_HC_HANDLE USBH_OHCI_CreateController          (void               * pBaseAddress);
int            USBH_OHCI_ServiceISR                (USBH_HC_HANDLE       HcHandle);
USBH_STATUS    USBH_OHCI_RemoveController          (USBH_HC_HANDLE       HcHandle, REMOVE_HC_COMPLETION_FUNC * pfOnCompletion, void * pContext);
void           USBH_OHCI_DeleteController          (USBH_HC_HANDLE       HcHandle);
void           USBH_OHCI_ProcessInterrupt          (USBH_HC_HANDLE       HcHandle);


USBH_STATUS     USBH_OHCI_EP0_Init                  (USBH_OHCI_EP0            * Ep,  USBH_OHCI_DEVICE * pDev, U32 Mask, U8 DeviceAddress, U8 EndpointAddress, U16 MaxFifoSize, USBH_SPEED Speed);
void            USBH_OHCI_EP0_Insert                (USBH_OHCI_EP0            * Ep);
void            USBH_OHCI_EP0_ReleaseEp             (USBH_OHCI_EP0            * Ep,  USBH_RELEASE_EP_COMPLETION_FUNC * pfOnReleaseEpCompletion, void * pContext);
USBH_STATUS     USBH_OHCI_EP0_AbortEp               (USBH_OHCI_EP0            * Ep);
USBH_STATUS     USBH_OHCI_EP0_AddUrb                (USBH_OHCI_EP0            * Ep,  USBH_URB * Urb);
void            USBH_OHCI_EP0_DoneCheckForCompletion(USBH_OHCI_INFO_GENERAL_TRANS_DESC * Gtd);
void            USBH_OHCI_Ep0RemoveEndpoints        (USBH_OHCI_DEVICE     * pDev, USBH_BOOL AllEndpointFlag);
void            USBH_OHCI_EP0_OnReleaseEpTimer      (void               * pContext); // Common ep0 timer callback routine
void            USBH_OHCI_EP0_OnAbortUrbTimer       (void               * pContext);
USBH_STATUS     USBH_OHCI_EP0_Alloc                 (USBH_HCM_POOL * EpPool, USBH_HCM_POOL * SetupPacketPool, U32 Numbers);  // Allocates all resources needed for control transfer, endpoints will not added to the control list
void            USBH_OHCI_Ep0Free                   (USBH_HCM_POOL * EpPool); // Releases all needed resources used for all control endpoints
USBH_OHCI_EP0 * USBH_OHCI_EP0_Get                   (USBH_HCM_POOL * EpPool, USBH_HCM_POOL * pSetupPacketPool);
void            USBH_OHCI_EP0_Put                   (USBH_OHCI_EP0  * Ep);     // Puts first all appended TDs back to the pool and as last the  control endpoint object

void                       USBH_OHCI_EpGlobInitED              (USBH_HCM_ITEM_HEADER * Header,   U8                DeviceAddr,  U8    EpWithDirection, U32 MaxPktSize, USBH_BOOL IsoFlag, USBH_BOOL SkipFlag, USBH_SPEED Speed);
void                       USBH_OHCI_EpGlobInsertTD            (USBH_HCM_ITEM_HEADER * EpHeader, USBH_HCM_ITEM_HEADER * NewTdHeader, U16 * TdCounter);
void                       USBH_OHCI_EpGlobDeleteAllPendingTD  (USBH_HCM_ITEM_HEADER * EpHeader, U16             * TdCounter);
void                       USBH_OHCI_EpGlobLinkEds             (USBH_HCM_ITEM_HEADER * Last,     USBH_HCM_ITEM_HEADER * New);
U32                        USBH_OHCI_EpGlobUnlinkEd            (USBH_HCM_ITEM_HEADER * Prev,     USBH_HCM_ITEM_HEADER * Remove);
void                       USBH_OHCI_EpGlobRemoveAllTDtoPool   (USBH_HCM_ITEM_HEADER * EpHeader, U16 * pTdCounter);
int                        USBH_OHCI_EpGlobIsTDActive          (USBH_HCM_ITEM_HEADER * EpHeader);
USBH_OHCI_INFO_GENERAL_TRANS_DESC * USBH_OHCI_EpGlobGetLastTDFromED     (USBH_HCM_ITEM_HEADER * EpHeader);
USBH_OHCI_INFO_GENERAL_TRANS_DESC * USBH_OHCI_EpGlobGetFirstTDFromED    (USBH_HCM_ITEM_HEADER * EpHeader);
void                       USBH_OHCI_EpGlobDeleteDoneTD        (USBH_HCM_ITEM_HEADER * TdItem,   U16 * pTdCounter);
void                       USBH_OHCI_EpGlobClearSkip           (USBH_HCM_ITEM_HEADER * EpHeader);
void                       USBH_OHCI_EpGlobSetSkip             (USBH_HCM_ITEM_HEADER * EpHeader);
int                        USBH_OHCI_EpGlobIsSkipped           (USBH_HCM_ITEM_HEADER * EpHeader);
void                       USBH_OHCI_EpGlobClearHalt           (USBH_HCM_ITEM_HEADER * EpHeader);
int                        USBH_OHCI_EpGlobIsHalt              (USBH_HCM_ITEM_HEADER * EpHeader);
void                       USBH_OHCI_EpClearToggle             (USBH_HCM_ITEM_HEADER * EpHeader);
void                       USBH_OHCI_EpSetToggle               (USBH_HCM_ITEM_HEADER * EpHeader, USBH_BOOL Toggle);
USBH_BOOL                  USBH_OHCI_EpGetToggle               (USBH_HCM_ITEM_HEADER * EpHeader);
U32                        USBH_OHCI_EpGlobGetTdCount          (USBH_HCM_ITEM_HEADER * EpHeader, USBH_HCM_POOL * TdPool);

/*********************************************************************
*
*       Resources allocation and releasing functions
*
**********************************************************************
*/
USBH_STATUS             USBH_OHCI_BULK_INT_AllocPool(USBH_HCM_POOL        * EpPool, unsigned int MaxEps);
USBH_OHCI_BULK_INT_EP * USBH_OHCI_BULK_INT_GetEp    (USBH_HCM_POOL        * EpPool);
void                    USBH_OHCI_BULK_INT_PutEp    (USBH_OHCI_BULK_INT_EP * Ep);

/*********************************************************************
*
*       Operations
*
**********************************************************************
*/

USBH_OHCI_BULK_INT_EP * USBH_OHCI_BULK_INT_InitEp                    (USBH_OHCI_BULK_INT_EP * Ep, USBH_OHCI_DEVICE * Dev, U8 EndpointType, U8 DeviceAddress, U8 EndpointAddress, U16 MaxFifoSize, U16 IntervalTime, USBH_SPEED Speed, U32 Flags);
USBH_STATUS             USBH_OHCI_BULK_INT_AddUrb                    (USBH_OHCI_BULK_INT_EP * Ep, USBH_URB * Urb);
USBH_STATUS             USBH_OHCI_BULK_INT_AbortEp                   (USBH_OHCI_BULK_INT_EP * Ep);
USBH_STATUS             USBH_OHCI_BULK_INT_CheckAndCancelAbortedUrbs (USBH_OHCI_BULK_INT_EP * Ep, int TDDoneFlag);
void                    USBH_OHCI_BULK_ReleaseEp                     (USBH_OHCI_BULK_INT_EP * Ep, USBH_RELEASE_EP_COMPLETION_FUNC * pfOnReleaseEpCompletion, void * pContext);
void                    USBH_OHCI_BULK_INT_SubmitUrbsFromList        (USBH_OHCI_BULK_INT_EP * Ep);
void                    USBH_OHCI_BULK_InsertEp                      (USBH_OHCI_BULK_INT_EP * Ep);
void                    USBH_OHCI_BULK_INT_UpdateTDLengthStatus      (USBH_OHCI_INFO_GENERAL_TRANS_DESC * Gtd);
void                    USBH_OHCI_BULK_INT_CheckForCompletion        (USBH_OHCI_INFO_GENERAL_TRANS_DESC * Gtd);

USBH_STATUS             USBH_OHCI_INT_DummyEpAllocPool               (USBH_HCM_POOL            * pEpPool);
void                    USBH_OHCI_INT_ReleaseEp                      (USBH_OHCI_BULK_INT_EP    * pEp, USBH_RELEASE_EP_COMPLETION_FUNC * pfOnReleaseEpCompletion, void * pContext);
USBH_STATUS             USBH_OHCI_INT_InsertEp                       (USBH_OHCI_BULK_INT_EP    * pEp);
USBH_STATUS             USBH_OHCI_INT_InitAllocDummyIntEps           (USBH_OHCI_DEVICE * pDev);
void                    USBH_OHCI_INT_PutAllDummyEp                  (USBH_OHCI_DEVICE * pDev);
void                    USBH_OHCI_INT_BuildDummyEpTree               (USBH_OHCI_DEVICE * pDev);
void                    USBH_OHCI_INT_RemoveAllUserEDFromPhysicalLink(USBH_OHCI_DEVICE * pDev);
USBH_BOOL               USBH_OHCI_INT_RemoveEDFromLogicalListAndFree (USBH_OHCI_DEVICE * pDev, USBH_BOOL All);
U32                     USBH_OHCI_INT_GetBandwidth                   (USBH_OHCI_DEVICE * pDev, U16 IntervalTime, USBH_OHCI_DUMMY_INT_EP ** ppEp);

void                    USBH_OHCI_BULK_RemoveEps                     (USBH_OHCI_DEVICE * pDev, USBH_BOOL AllEndpointFlag);

void                    USBH_OHCI_INT_OnReleaseEpTimer               (void    * pContext);
void                    USBH_OHCI_INT_OnAbortUrbTimer                (void    * pContext);
void                    USBH_OHCI_BULK_OnAbortUrbTimer               (void    * pContext);
void                    USBH_OHCI_BULK_INT_OnRemoveEpTimer           (void    * pContext);

/*********************************************************************
*
*       Iso operations
*
**********************************************************************
*/
USBH_STATUS        USBH_OHCI_ISO_AllocPool                (USBH_HCM_POOL    * EpPool, unsigned int MaxEps);
USBH_OHCI_ISO_EP * USBH_OHCI_ISO_GetEp                    (USBH_HCM_POOL    * EpPool);
void               USBH_OHCI_ISO_PutEp                    (USBH_OHCI_ISO_EP * Ep);
USBH_OHCI_ISO_EP * USBH_OHCI_ISO_InitEp                   (USBH_OHCI_ISO_EP * Ep, USBH_OHCI_DEVICE * Dev, U8 EndpointType, U8 DeviceAddress, U8 EndpointAddress, U16 MaxFifoSize, U16 IntervalTime, USBH_SPEED Speed, U32 Flags);
USBH_STATUS        USBH_OHCI_ISO_AddUrb                   (USBH_OHCI_ISO_EP * Ep, USBH_URB * Urb);
USBH_STATUS        USBH_OHCI_ISO_AbortEp                  (USBH_OHCI_ISO_EP * Ep);
USBH_STATUS        USBH_OHCI_ISO_CheckAndCancelAbortedUrbs(USBH_OHCI_ISO_EP * Ep, int TDDoneFlag);
void               USBH_OHCI_ISO_ReleaseEndpoint          (USBH_OHCI_ISO_EP * Ep, USBH_RELEASE_EP_COMPLETION_FUNC * pfOnReleaseEpCompletion, void * pContext);
void               USBH_OHCI_ISO_SubmitUrbsFromList       (USBH_OHCI_ISO_EP * Ep);
USBH_STATUS        USBH_OHCI_ISO_InsertEp                 (USBH_OHCI_ISO_EP * Ep);
void               USBH_OHCI_ISO_UpdateTDLengthStatus     (USBH_OHCI_INFO_GENERAL_TRANS_DESC * Gtd);
void               USBH_OHCI_ISO_CheckForCompletion       (USBH_OHCI_INFO_GENERAL_TRANS_DESC * Gtd);
void               USBH_OHCI_ISO_OnAbortUrbTimer          (void    * pContext);
void               USBH_OHCI_ISO_OnRemoveEpTimer          (void    * pContext);
void               USBH_OHCI_ISO_RemoveEps                (USBH_OHCI_DEVICE * dev, USBH_BOOL AllEndpointFlag);

USBH_STATUS                      USBH_OHCI_TdGetStatusAndLength(USBH_OHCI_INFO_GENERAL_TRANS_DESC  * Gtd,            U32 * Transferred,  USBH_BOOL     * shortPkt);
USBH_STATUS                      USBH_OHCI_TdAlloc             (USBH_HCM_POOL * GeneralTd,      U32   GeneralTdNumbers, unsigned Alignment);
void                             USBH_OHCI_TdInit              (USBH_OHCI_INFO_GENERAL_TRANS_DESC  * gtd, void * Ep, U8    EndpointType, USBH_OHCI_TD_PID   Pid, U32 StartAddr, U32 EndAddr, U32 Dword0Mask);
USBH_OHCI_INFO_GENERAL_TRANS_DESC * USBH_OHCI_GetTransDesc     (USBH_HCM_POOL * GeneralTd);
U32                              USBH_OHCI_TdGetNextTd         (U32             TdAddress);
void                             USBH_OHCI_IsoTdInit           (USBH_OHCI_INFO_GENERAL_TRANS_DESC * gtd, USBH_OHCI_ISO_EP * pIsoEp, USBH_ISO_REQUEST * pIsoRequest, U32 DWord0, U32 StartAddr, int NumBytes, int Index);
USBH_STATUS                      USBH_OHCI_ISO_TdGetStatusAndLength(USBH_OHCI_INFO_GENERAL_TRANS_DESC * pGlobalTransDesc, U32 * pTransferred, USBH_BOOL * pShortPacket);




/*********************************************************************
*
*       Host device driver interface functions
*
**********************************************************************
*/
void                 USBH_OHCI_ReleaseEndpoint (USBH_HC_EP_HANDLE hEP,             USBH_RELEASE_EP_COMPLETION_FUNC *  pfReleaseEpCompletion,    void * pContext);
USBH_STATUS          USBH_OHCI_HostInit        (USBH_HC_HANDLE    hHostController, USBH_ROOT_HUB_NOTIFICATION_FUNC *  pfUbdRootHubNotification, void * pRootHubNotificationContext);
USBH_STATUS          USBH_OHCI_SetHcState      (USBH_HC_HANDLE    hHostController, USBH_HOST_STATE HostState);
USBH_STATUS          USBH_OHCI_HostExit        (USBH_HC_HANDLE    hHostController);
USBH_HC_EP_HANDLE    USBH_OHCI_AddEndpoint     (USBH_HC_HANDLE    hHostController, U8 EndpointType, U8 DeviceAddress, U8 EndpointAddress, U16 MaxFifoSize, U16 IntervalTime, USBH_SPEED Speed);

U32                  USBH_OHCI_GetFrameNumber         (USBH_HC_HANDLE     hHostController);
USBH_OHCI_TRANSFER_BUFFER * USBH_OHCI_GetCopyTransferBuffer           (USBH_HCM_POOL           * pTransferBufferPool);
USBH_OHCI_TRANSFER_BUFFER * USBH_OHCI_GetInitializedCopyTransferBuffer(USBH_HCM_POOL           * pTransferBufferPool, U8 * pUrbBuffer, U32 UrbBufferLength);
U32                  USBH_OHCI_FillCopyTransferBuffer                 (USBH_OHCI_TRANSFER_BUFFER * pTransferBuffer);
U32                  USBH_OHCI_CopyToUrbBufferUpdateTransferBuffer    (USBH_OHCI_TRANSFER_BUFFER * pTransferBuffer,     U32   Transferred);
U32                  USBH_OHCI_UpdateCopyTransferBuffer               (USBH_OHCI_TRANSFER_BUFFER * pTransferBuffer,     U32   Transferred);
U8                 * USBH_OHCI_GetBufferLengthFromCopyTransferBuffer  (USBH_OHCI_TRANSFER_BUFFER * pTransferBuffer,     U32 * pLength);
void                 USBH_OHCI_UpdateUpperFrameCounter                (USBH_OHCI_DEVICE * pDev);
void                 USBH_OHCI_EndpointListEnable                     (USBH_OHCI_DEVICE * pDev, U8 EpType, USBH_BOOL Enable, USBH_BOOL ListFill);

/*********************************************************************
*
*       Driver root hub interface
*
**********************************************************************
*/

unsigned int USBH_OHCI_ROOTHUB_GetPortCount    (USBH_HC_HANDLE hHostController);
unsigned int USBH_OHCI_ROOTHUB_GetPowerGoodTime(USBH_HC_HANDLE hHostController);
U32          USBH_OHCI_ROOTHUB_GetHubStatus    (USBH_HC_HANDLE hHostController);
void         USBH_OHCI_ROOTHUB_ClearHubStatus  (USBH_HC_HANDLE hHostController,          U16 FeatureSelector);
void         USBH_OHCI_ROOTHUB_ClearPortStatus (USBH_HC_HANDLE hHostController, U8 Port, U16 FeatureSelector);
U32          USBH_OHCI_ROOTHUB_GetPortStatus   (USBH_HC_HANDLE hHostController, U8 Port);
void         USBH_OHCI_ROOTHUB_SetPortPower    (USBH_HC_HANDLE hHostController, U8 Port, U8 PowerOn);
void         USBH_OHCI_ROOTHUB_ResetPort       (USBH_HC_HANDLE hHostController, U8 Port);
void         USBH_OHCI_ROOTHUB_DisablePort     (USBH_HC_HANDLE hHostController, U8 Port);
void         USBH_OHCI_ROOTHUB_SetPortSuspend  (USBH_HC_HANDLE hHostController, U8 Port, USBH_PORT_POWER_STATE State);
USBH_STATUS  USBH_OHCI_ROOTHUB_Init            (USBH_OHCI_DEVICE * pDev, USBH_ROOT_HUB_NOTIFICATION_FUNC * pfUbdRootHubNotification, void * pfRootHubNotificationContext);
void         USBH_OHCI_ROOTHUB_ProcessInterrupt(USBH_OHCI_ROOT_HUB    * pHub);



USBH_OHCI_HCCA * USBH_OHCI_HccaAlloc            (USBH_HCM_POOL * Pool);
void             USBH_OHCI_HccaRelease          (USBH_OHCI_HCCA * OhdHcca);
void             USBH_OHCI_HccaSetInterruptTable(USBH_OHCI_HCCA * OhdHcca, USBH_OHCI_DUMMY_INT_EP * dummyInterruptEndpointList []);

void             USBH_OnISREvent                     (void);
void             USBH_Free                           (void               * pMemBlock);
void           * USBH_Malloc                         (U32                  Size);
void           * USBH_MallocZeroed                   (U32                  Size);
void           * USBH_TryMalloc                      (U32                  Size);
void           * USBH_AllocTransferMemory            (U32                  NumBytes, unsigned Alignment);

USBH_HC_HANDLE USBH_STM32_CreateController(void * pBaseAddress);


/*********************************************************************
*
*       Utility functions
*
*  RS: Maybe we should move them into a UTIL module some time ? (We can keep macros here for compatibility)
*
**********************************************************************
*/
I32          USBH_BringInBounds(I32 v, I32 Min, I32 Max);
U32          USBH_LoadU32BE(const U8 * pData);
U32          USBH_LoadU32LE(const U8 * pData);
U32          USBH_LoadU32TE(const U8 * pData);
unsigned     USBH_LoadU16BE(const U8 * pData);
unsigned     USBH_LoadU16LE(const U8 * pData);
void         USBH_StoreU16BE     (U8 * p, unsigned v);
void         USBH_StoreU16LE     (U8 * p, unsigned v);
void         USBH_StoreU32BE     (U8 * p, U32      v);
void         USBH_StoreU32LE     (U8 * p, U32      v);
U32          USBH_SwapU32                (U32      v);

void         USBH_HC_ClrActivePortReset(USBH_HOST_CONTROLLER * pHost, USBH_HUB_PORT * pEnumPort);
void         USBH_HC_SetActivePortReset(USBH_HOST_CONTROLLER * pHost, USBH_HUB_PORT * pEnumPort);

const char * USBH_PortSpeed2Str(USBH_SPEED    x);
const char * USBH_GetStatusStr(USBH_STATUS   x);
const char * USBH_PortState2Str(PORT_STATE    x);
const char * USBH_UrbFunction2Str(USBH_FUNCTION x);

const char * USBH_HubPortResetState2Str   (USBH_HUB_PORTRESET_STATE x);
const char * USBH_HubNotificationState2Str(USBH_HUB_NOTIFY_STATE    x);
const char * USBH_HubEnumState2Str        (USBH_HUB_ENUM_STATE      x);
const char * USBH_EnumState2Str       (DEV_ENUM_STATE        x);
const char * USBH_RhPortResetState2Str(USBH_ROOT_HUB_PORTRESET_STATE    x);
const char * USBH_HcState2Str         (HOST_CONTROLLER_STATE x);
const char * USBH_Ep0State2Str        (USBH_EP0_PHASE x);

void         USBH_DLIST_Append     (USBH_DLIST * ListHead, USBH_DLIST * List);
void         USBH_DLIST_InsertTail (USBH_DLIST * ListHead, USBH_DLIST * Entry);
void         USBH_DLIST_InsertHead (USBH_DLIST * ListHead, USBH_DLIST * Entry);
void         USBH_DLIST_InsertEntry(USBH_DLIST * Entry,    USBH_DLIST * NewEntry);
void         USBH_DLIST_RemoveTail (USBH_DLIST * ListHead, USBH_DLIST ** Entry);
void         USBH_DLIST_RemoveHead (USBH_DLIST * ListHead, USBH_DLIST ** Entry);
void         USBH_DLIST_RemoveEntry(USBH_DLIST * Entry);
USBH_DLIST * USBH_DLIST_GetPrev    (USBH_DLIST * Entry);
USBH_DLIST * USBH_DLIST_GetNext    (USBH_DLIST * Entry);
int          USBH_DLIST_IsEmpty    (USBH_DLIST * ListHead);
void         USBH_DLIST_Init       (USBH_DLIST * ListHead);


/*********************************************************************
*
*       USBH_DLIST
*/
typedef struct USBH_DLIST_ITEM {
  struct USBH_DLIST_ITEM * pNext;
  struct USBH_DLIST_ITEM * pPrev;
} USBH_DLIST_ITEM;

typedef struct {
  struct USBH_DLIST_ITEM * pFirst;
  int NumItems;
} USBH_DLIST_HEAD;

void USBH_DLIST_Remove(USBH_DLIST_HEAD * pHead, USBH_DLIST_ITEM * pItem);
void USBH_DLIST_Add   (USBH_DLIST_HEAD * pHead, USBH_DLIST_ITEM * pNew);

void USBH__ConvSetupPacketToBuffer(const USBH_SETUP_PACKET * Setup, U8 * pBuffer);


#undef EXTERN

#if defined(__cplusplus)
  }
#endif

#endif

/******************************* EOF ********************************/
