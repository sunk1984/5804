/*********************************************************************
*  
*   IAR PowerPac
*
*   (c) Copyright IAR Systems 2010.  All rights reserved.
*
**********************************************************************
----------------------------------------------------------------------
File        : USBH.h
Purpose     : API of the USB host stack
---------------------------END-OF-HEADER------------------------------
*/

#ifndef _USBH_H_
#define _USBH_H_

#include <stdarg.h>
#include "SEGGER.h"
#include "USBH_ConfDefaults.h"

#if defined(__cplusplus)
  extern "C" {                 // Make sure we have C-declarations in C++ programs
#endif

#define USBH_VERSION   10405 // Format: Mmmrr. Example: 10201 is 1.02a

typedef struct USBH_HOST_CONTROLLER   USBH_HOST_CONTROLLER;
typedef struct USB_DEVICE             USB_DEVICE;
typedef struct USBH_OHCI_DEVICE       USBH_OHCI_DEVICE;
typedef struct USBH_HUB_PORT          USBH_HUB_PORT;
typedef struct USB_HUB                USB_HUB;
typedef struct USBH_OHCI_DUMMY_INT_EP USBH_OHCI_DUMMY_INT_EP;
typedef struct USB_INTERFACE          USB_INTERFACE;

/*********************************************************************
*
*       USBH_MTYPE
*
*  IDs to distinguish different message types
*
**********************************************************************
*/
#define USBH_MTYPE_INIT          (1UL <<  0)
#define USBH_MTYPE_CORE          (1UL <<  1)
#define USBH_MTYPE_TIMER         (1UL <<  2)
#define USBH_MTYPE_DRIVER        (1UL <<  3)
#define USBH_MTYPE_MEM           (1UL <<  4)
#define USBH_MTYPE_URB           (1UL <<  5)
#define USBH_MTYPE_OHCI          (1UL <<  6)
#define USBH_MTYPE_UBD           (1UL <<  7)
#define USBH_MTYPE_PNP           (1UL <<  8)
#define USBH_MTYPE_DEVICE        (1UL <<  9)
#define USBH_MTYPE_EP            (1UL << 10)
#define USBH_MTYPE_HUB           (1UL << 11)
#define USBH_MTYPE_MSD           (1UL << 12)
#define USBH_MTYPE_MSD_INTERN    (1UL << 13)
#define USBH_MTYPE_MSD_PHYS      (1UL << 14)
#define USBH_MTYPE_HID           (1UL << 15)
#define USBH_MTYPE_PRINTER_CLASS (1UL << 16)
#define USBH_MTYPE_APPLICATION   (1UL << 19)

void USBH_Logf_Application   (const char * sFormat, ...);
void USBH_Warnf_Application  (const char * sFormat, ...);
void USBH_sprintf_Application(      char * pBuffer, unsigned BufferSize, const char * sFormat, ...);

/*********************************************************************
*
*       Log/Warn functions
*
**********************************************************************
*/
void USBH_Log          (const char * s);
void USBH_Warn         (const char * s);
void USBH_SetLogFilter (U32 FilterMask);
void USBH_SetWarnFilter(U32 FilterMask);
void USBH_AddLogFilter (U32 FilterMask);
void USBH_AddWarnFilter(U32 FilterMask);
void USBH_Logf         (U32 Type,             const char * sFormat, ...);
void USBH_Warnf        (U32 Type,             const char * sFormat, ...);
void USBH_PrintfSafe   (char       * pBuffer, const char * sFormat, int BufferSize, va_list * pParamList);
void USBH_Panic        (const char * sError);

/*********************************************************************
*
*       USBH_OS_
*
**********************************************************************
*/
void USBH_OS_Delay           (unsigned ms);
void USBH_OS_DisableInterrupt(void);
void USBH_OS_EnableInterrupt (void);
void USBH_OS_Init            (void);
void USBH_OS_Unlock          (void);
void USBH_OS_AssertLock      (void);
void USBH_OS_Lock            (void);
U32  USBH_OS_GetTime32       (void);

// Lock / Unlock mutex / resource semaphore used for memory operations
void USBH_OS_LockSys       (void);
void USBH_OS_UnlockSys     (void);
// Wait and signal for USBH Main Task
void USBH_OS_WaitNetEvent  (unsigned ms);
void USBH_OS_SignalNetEvent(void);
// Wait and signal for USBH ISR Task
void USBH_OS_WaitISR       (void);
void USBH_OS_SignalISR     (void);

/*********************************************************************
*
*       USBH_OS_ - Event objects
*
**********************************************************************
*/
#define USBH_OS_EVENT_SIGNALED 0

typedef struct      USBH_OS_EVENT_OBJ     USBH_OS_EVENT_OBJ;
USBH_OS_EVENT_OBJ * USBH_OS_AllocEvent    (void);                       // Allocates and returns an event object.
void                USBH_OS_FreeEvent     (USBH_OS_EVENT_OBJ * pEvent); // Releases an object event.
void                USBH_OS_SetEvent      (USBH_OS_EVENT_OBJ * pEvent); // Sets the state of the specified event object to signaled.
void                USBH_OS_ResetEvent    (USBH_OS_EVENT_OBJ * pEvent); // Sets the state of the specified event object to none-signaled.
void                USBH_OS_WaitEvent     (USBH_OS_EVENT_OBJ * pEvent);
int                 USBH_OS_WaitEventTimed(USBH_OS_EVENT_OBJ * pEvent, U32 milliSeconds);

// Wait and signal for application tasks
void USBH_OS_WaitItem     (void * pWaitItem);
void USBH_OS_WaitItemTimed(void * pWaitItem, unsigned Timeout);
void USBH_OS_SignalItem   (void * pWaitItem);
void USBH_OS_AddTickHook  (void(* pfHook)(void));

void USBH_Task                (void);
char USBH_IsExpired           (I32 Time);
int  USBH_GetVersion          (void);
void USBH_Init                (void);
void USBH_X_Config            (void);
void USBH_AssignMemory        (U32 * pMem, U32 NumBytes);
void USBH_AssignTransferMemory(U32 * pMem, U32 NumBytes);

// Direction types
#define USB_IN_DIRECTION      0x80
#define USB_OUT_DIRECTION     0x00

// Request Type Direction
#define USB_TO_DEVICE         0
#define USB_TO_HOST           0x80

// End Point types
#define USB_EP_TYPE_CONTROL   0x00
#define USB_EP_TYPE_ISO       0x01
#define USB_EP_TYPE_BULK      0x02
#define USB_EP_TYPE_INT       0x03

// bcdUSB
#define USB_1                 0x0110
#define USB_2                 0x0210

// USB descriptor types
#define USB_DEVICE_DESCRIPTOR_TYPE                    0x01
#define USB_CONFIGURATION_DESCRIPTOR_TYPE             0x02
#define USB_STRING_DESCRIPTOR_TYPE                    0x03
#define USB_INTERFACE_DESCRIPTOR_TYPE                 0x04
#define USB_ENDPOINT_DESCRIPTOR_TYPE                  0x05
#define USB_DEVICE_QUALIFIER_DESCRIPTOR_TYPE          0x06
#define USB_OTHER_SPEED_CONFIGURATION_DESCRIPTOR_TYPE 0x07
#define USB_INTERFACE_ASSOCIATION_TYPE                0x0B
#define USB_HID_DESCRIPTOR_TYPE                       0x21
#define USB_HID_DESCRIPTOR_TYPE_REPORT                0x22

// Defines for Standard Configruation pDescriptor
// bmAttributes
#define USB_CONF_BUSPWR                               0x80 // Config. attribute: Bus powered
#define USB_CONF_SELFPWR                              0x40 // Config. attribute: Self powered
#define USB_CONF_REMOTE_WAKEUP                        0x20 // Config. attribute: Remote Wakeup

// USB classes
#define USB_DEVICE_CLASS_RESERVED                     0x00
#define USB_DEVICE_CLASS_AUDIO                        0x01
#define USB_DEVICE_CLASS_COMMUNICATIONS               0x02
#define USB_DEVICE_CLASS_HUMAN_INTERFACE              0x03
#define USB_DEVICE_CLASS_MONITOR                      0x04
#define USB_DEVICE_CLASS_PHYSICAL_INTERFACE           0x05
#define USB_DEVICE_CLASS_POWER                        0x06
#define USB_DEVICE_CLASS_PRINTER                      0x07
#define USB_DEVICE_CLASS_STORAGE                      0x08
#define USB_DEVICE_CLASS_HUB                          0x09
#define USB_DEVICE_CLASS_DATA                         0x0A
#define USB_DEVICE_CLASS_VENDOR_SPECIFIC              0xFF

// HID protocol and subclass definitions
#define HID_DEVICE_BOOT_INTERFACE_SUBCLASS            0x01
#define HID_DEVICE_KEYBOARD_PROTOCOL                  0x01
#define HID_DEVICE_MOUSE_PROTOCOL                     0x02

// USB endpoint types
#define USB_ENDPOINT_TYPE_CONTROL                     0x00
#define USB_ENDPOINT_TYPE_ISOCHRONOUS                 0x01
#define USB_ENDPOINT_TYPE_BULK                        0x02
#define USB_ENDPOINT_TYPE_INTERRUPT                   0x03

// Setup Request Types
#define USB_REQTYPE_MASK                              0x60 // Used to mask off request type
#define USB_REQTYPE_STANDARD                          0x00 // Standard Request
#define USB_REQTYPE_CLASS                             0x20 // Class Request
#define USB_REQTYPE_VENDOR                            0x40 // Vendor Request
#define USB_REQTYPE_RESERVED                          0x60 // Reserved or illegal request

// Request Type Recipient
#define USB_RECIPIENT_MASK                            0x1F // Bitsd D0..D4
#define USB_DEVICE_RECIPIENT                          0
#define USB_INTERFACE_RECIPIENT                       1
#define USB_ENDPOINT_RECIPIENT                        2
#define USB_OTHER_RECIPIENT                           3
#define USB_RESERVED_RECIPIENT                        4

// bRequest in USB Device Request
// Standard Request Codes
#define USB_REQ_GET_STATUS                            0x00
#define USB_REQ_CLEAR_FEATURE                         0x01
#define USB_REQ_SET_FEATURE                           0x03
#define USB_REQ_SET_ADDRESS                           0x05
#define USB_REQ_GET_DESCRIPTOR                        0x06
#define USB_REQ_SET_DESCRIPTOR                        0x07
#define USB_REQ_GET_CONFIGURATION                     0x08
#define USB_REQ_SET_CONFIGURATION                     0x09
#define USB_REQ_GET_INTERFACE                         0x0A
#define USB_REQ_SET_INTERFACE                         0x0B
#define USB_REQ_SYNCH_FRAME                           0x0C

// GetStatus Requests Recipients and STATUS Codes
#define USB_STATUS_DEVICE                             0x80 // Get Status: Device
#define USB_STATUS_INTERFACE                          0x81 // Get Status: Interface
#define USB_STATUS_ENDPOINT                           0x82 // Get Status: End Point
#define USB_STATUS_SELF_POWERED                       0x01
#define USB_STATUS_REMOTE_WAKEUP                      0x02
#define USB_STATUS_ENDPOINT_HALT                      0x01
#define USB_STATUS_LENGTH                             2 // 2 byte

// Standard Feature Selectors
#define USB_FEATURE_REMOTE_WAKEUP                     0x01
#define USB_FEATURE_STALL                             0x00
#define USB_FEATURE_TEST_MODE                         0x02

// Common descriptor indexes
#define USB_DESC_LENGTH_INDEX                         0
#define USB_DESC_TYPE_INDEX                           1

typedef struct USB_DEVICE_DESCRIPTOR { // Device descriptor
  U8  bLength;
  U8  bDescriptorType;
  U16 bcdUSB;
  U8  bDeviceClass;
  U8  bDeviceSubClass;
  U8  bDeviceProtocol;
  U8  bMaxPacketSize0;
  U16 idVendor;
  U16 idProduct;
  U16 bcdDevice;
  U8  iManufacturer;
  U8  iProduct;
  U8  iSerialNumber;
  U8  bNumConfigurations;
} USB_DEVICE_DESCRIPTOR;

#define USB_DEVICE_DESCRIPTOR_LENGTH                  (18)

typedef struct USB_CONFIGURATION_DESCRIPTOR { // Configuration descriptor
  U8  bLength;
  U8  bDescriptorType;
  U16 wTotalLength;
  U8  bNumInterfaces;
  U8  bConfigurationValue;
  U8  iConfiguration;
  U8  bmAttributes;
  U8  MaxPower;
} USB_CONFIGURATION_DESCRIPTOR;

#define USB_CONFIGURATION_DESCRIPTOR_LENGTH             (9)
#define USB_CONFIGURATION_DESCRIPTOR_BMATTRIBUTES_INDEX (7)
#define USB_CONFIGURATION_DESCRIPTOR_WTOTALLENGTH_INDEX (2)
#define USB_CONFIGURATION_DESCRIPTOR_POWER_INDEX        (8)

typedef struct USB_INTERFACE_DESCRIPTOR { // Interface descriptor
  U8 bLength;
  U8 bDescriptorType;
  U8 bInterfaceNumber;
  U8 bAlternateSetting;
  U8 bNumEndpoints;
  U8 bInterfaceClass;
  U8 bInterfaceSubClass;
  U8 bInterfaceProtocol;
  U8 iInterface;
} USB_INTERFACE_DESCRIPTOR;

#define USB_INTERFACE_DESCRIPTOR_LENGTH               (9)
#define USB_INTERFACE_DESC_NUMBER_OFS                 2
#define USB_INTERFACE_DESC_CLASS_OFS                  5
#define USB_INTERFACE_DESC_SUBCLASS_OFS               6
#define USB_INTERFACE_DESC_PROTOCOL_OFS               7

typedef struct USB_ENDPOINT_DESCRIPTOR { // Endpoint descriptor
  U8  bLength;
  U8  bDescriptorType;
  U8  bEndpointAddress;
  U8  bmAttributes;
  U16 wMaxPacketSize;
  U8  bInterval;
} USB_ENDPOINT_DESCRIPTOR;

#define USB_ENDPOINT_DESCRIPTOR_LENGTH                (7)
#define USB_EP_DESC_ADDRESS_OFS                       2
#define USB_EP_DESC_ATTRIB_OFS                        3
#define USB_EP_DESC_PACKET_SIZE_OFS                   4
#define USB_EP_DESC_INTERVAL_OFS                      6
#define USB_EP_DESC_ATTRIB_MASK                       0x03
#define USB_EP_DESC_DIR_MASK                          0x80

typedef struct USB_STRING_DESCRIPTOR { // String descriptor
  U8  bLength;
  U8  bDescriptorType;
  U16 bString[1];                        // Variable size
} USB_STRING_DESCRIPTOR;

#define USB_STRING_HEADER_LENGTH                      2
#define USB_LANGUAGE_DESC_LENGTH                      (4)
#define USB_LANGUAGE_ID                               (0x0409)

typedef struct USB_DEVICE_QUALIFIER_DESCRIPTOR { // Device qualifier descriptor
  U8  bLength;
  U8  bDescriptorType;
  U16 bcdUSB;
  U8  bDeviceClass;
  U8  bDeviceSubClass;
  U8  bDeviceProtocol;
  U8  bMaxPacketSize0;
  U8  bNumConfigurations;
  U8  bReserved;
} USB_DEVICE_QUALIFIER_DESCRIPTOR;

#define USB_DEVICE_QUALIFIER_DESCRIPTOR_LENGTH        (10)

typedef struct USB_INTERFACE_ASSOCIATION_DESCRIPTOR { // Interface association descriptor
  U8 bLength;
  U8 bDescriptorType;
  U8 bFirstInterface;
  U8 bInterfaceCount;
  U8 bFunctionClass;
  U8 bFunctionSubClass;
  U8 bFunctionProtocol;
  U8 iFunction;
} USB_INTERFACE_ASSOCIATION_DESCRIPTOR;

typedef struct USB_COMMON_DESCRIPTOR { // Common descriptor header
  U8 bLength;
  U8 bDescriptorType;
} USB_COMMON_DESCRIPTOR;

typedef struct USB_SETUP_PACKET { // CS Endpoint
  U8  Type;
  U8  Request;
  U16 Value;
  U16 Index;
  U16 Length;
} USBH_SETUP_PACKET;

#define USB_SETUP_PACKET_LEN                          8
#define USB_SETUP_TYPE_INDEX                          0
#define USB_SETUP_LENGTH_INDEX_LSB                    6
#define USB_SETUP_LENGTH_INDEX_MSB                    7

/*********************************************************************
*
*       pHub Device Class (HDC) *
*
**********************************************************************
*/

// HDC specific descriptor types
#define CDC_CS_INTERFACE_DESCRIPTOR_TYPE              0x24
#define CDC_CS_ENDPOINT_DESCRIPTOR_TYPE               0x25

// HDC Port Type Recipient. All other are device recipients!
#define HDC_PORT_RECIPIENT USB_OTHER_RECIPIENT

// pHub class descriptor
#define HDC_MAX_HUB_DESCRIPTOR_LENGTH                 71
#define USB_HUB_DESCRIPTOR_TYPE                       0x29

// Class specific hub  descriptor
#define HDC_DESC_PORT_NUMBER_OFS                      2
#define HDC_DESC_CHARACTERISTICS_LOW_OFS              3
#define HDC_DESC_CHARACTERISTICS_HIGH_OFS             4
#define HDC_DESC_POWER_GOOD_TIME_OFS                  5
#define HDC_DESC_MAX_CUURENT_OFS                      6
#define HDC_DESC_DEVICE_REMOVABLE_OFS                 7
#define HDC_DESC_POWER_SWITCH_MASK                    0x3
#define HDC_DESC_ALL_POWER_SWITCH_VALUE               0x0
#define HDC_DESC_SINGLE_POWER_SWITCH_VALUE            0x1
#define HDC_DESC_COMPOUND_DEVICE_MASK                 0x4
#define HDC_DESC_OVERCURRENT_MASK                     0x18
#define HDC_DESC_OVERCURRENT_GLOBAL_VAL               0x0
#define HDC_DESC_OVERCURRENT_SELECTIVE_VAL            0x08
#define HDC_DESC_NO_OVERCURRENT_MASK                  0x10
#define HDC_DESC_SUPPORT_INIDCATOR_MASK               0x80

// pHub status request length
#define HCD_GET_STATUS_LENGTH                         4
#define HDC_DESC_MIN_LENGTH                           8

// bRequest in USB Class Request
// HDC Standard Request Codes
#define HDC_REQTYPE_GET_STATUS                        0
#define HDC_REQTYPE_CLEAR_FEATRUE                     1
// RESERVED (used in previous specifications for GET_STATE)
#define HDC_REQTYPE_GET_STATUS_OLD                    2
#define HDC_REQTYPE_SET_FEATRUE                       3
// RESERVED 4 and 5
#define HDC_REQTYPE_GET_DESCRIPTOR                    6
#define HDC_REQTYPE_SET_DESCRIPTOR                    7
#define HDC_REQTYPE_CLEAR_TT_BUFFER                   8
#define HDC_REQTYPE_RESET_TT                          9
#define HDC_REQTYPE_GET_TT_STATE                      10
#define HDC_REQTYPE_STOP_TT                           11

// pHub class hub feature selectors
// pHub change bits
#define HDC_SELECTOR_C_HUB_LOCAL_POWER                0
#define HDC_SELECTOR_C_HUB_OVER_CURRENT               1

// pHub class port feature selectors
// Port Selectors
#define HDC_SELECTOR_PORT_CONNECTION                  0
#define HDC_SELECTOR_PORT_ENABLE                      1
#define HDC_SELECTOR_PORT_SUSPEND                     2
#define HDC_SELECTOR_PORT_OVER_CURREWNT               3
#define HDC_SELECTOR_PORT_RESET                       4
#define HDC_SELECTOR_PORT_POWER                       8
#define HDC_SELECTOR_PORT_LOW_SPEED                   9

// Port change bits
#define HDC_SELECTOR_C_PORT_CONNECTION                16
#define HDC_SELECTOR_C_PORT_ENABLE                    17
#define HDC_SELECTOR_C_PORT_SUSPEND                   18
#define HDC_SELECTOR_C_PORT_OVER_CURRENT              19
#define HDC_SELECTOR_C_PORT_RESET                     20

// Port Selectors
#define HDC_SELECTOR_PORT_TEST                        21
#define HDC_SELECTOR_PORT_INDICATOR                   22
#define HDC_GET_SELECTOR_MASK(Selector)               (((U32)0x0001)<<(Selector))

// Port status bits
#define PORT_STATUS_CONNECT                           0x00000001
#define PORT_STATUS_ENABLED                           0x00000002
#define PORT_STATUS_SUSPEND                           0x00000004
#define PORT_STATUS_OVER_CURRENT                      0x00000008
#define PORT_STATUS_RESET                             0x00000010
#define PORT_STATUS_POWER                             0x00000100
#define PORT_STATUS_LOW_SPEED                         0x00000200
#define PORT_STATUS_HIGH_SPEED                        0x00000400
#define PORT_C_ALL_MASK                               0x001F0000UL
#define PORT_C_STATUS_CONNECT                         0x00010000UL
#define PORT_C_STATUS_ENABLE                          0x00020000UL
#define PORT_C_STATUS_SUSPEND                         0x00040000UL
#define PORT_C_STATUS_OVER_CURRENT                    0x00080000UL
#define PORT_C_STATUS_RESET                           0x00100000UL

// pHub status bits
#define HUB_STATUS_LOCAL_POWER                        0x00000001
#define HUB_STATUS_OVER_CURRENT                       0x00000002
#define HUB_STATUS_C_LOCAL_POWER                      0x00010000UL
#define HUB_STATUS_C_OVER_CURRENT                     0x00020000UL

typedef unsigned int USBH_STATUS;
// Status Codes
#define USBH_STATUS_SUCCESS                            0x0000

// Host controller error codes
#define USBH_STATUS_CRC                                0x0001
#define USBH_STATUS_BITSTUFFING                        0x0002
#define USBH_STATUS_DATATOGGLE                         0x0003
#define USBH_STATUS_STALL                              0x0004
#define USBH_STATUS_NOTRESPONDING                      0x0005
#define USBH_STATUS_PID_CHECK                          0x0006
#define USBH_STATUS_UNEXPECTED_PID                     0x0007
#define USBH_STATUS_DATA_OVERRUN                       0x0008
#define USBH_STATUS_DATA_UNDERRUN                      0x0009
#define USBH_STATUS_BUFFER_OVERRUN                     0x000C
#define USBH_STATUS_BUFFER_UNDERRUN                    0x000D
#define USBH_STATUS_NOT_ACCESSED                       0x000F

// EHCI error codes
#define USBH_STATUS_BUFFER                             0x00010
#define USBH_STATUS_BABBLE                             0x00011
#define USBH_STATUS_XACT                               0x00012

#define USBH_STATUS_CHANNEL_HALTED                     0x00020
#define USBH_STATUS_CHANNEL_NAK                        0x00021

// Maximum error code for an hardware error number
#define USBH_STATUS_MAX_HARDWARE_ERROR                 0x00FF

// Bus driver error codes
#define USBH_STATUS_ERROR                              0x0101
#define USBH_STATUS_BUFFER_OVERFLOW                    0x0102
#define USBH_STATUS_INVALID_PARAM                      0x0103
#define USBH_STATUS_PENDING                            0x0104
#define USBH_STATUS_DEVICE_REMOVED                     0x0105
#define USBH_STATUS_CANCELED                           0x0106

// The endpoint, interface or device has pending requests and the operation requires no pending requests
#define USBH_STATUS_BUSY                               0x0109

// Returned on an invalid descriptor
#define USBH_STATUS_INVALID_DESCRIPTOR                 0x0110

// The endpoint has been halted. This error is reported by the USB host controller driver layer.
// A pipe will be halted when a data transmission error (CRC, bit stuff, DATA toggle) occurs.
#define USBH_STATUS_ENDPOINT_HALTED                    0x0111
#define USBH_STATUS_TIMEOUT                            0x0112
#define USBH_STATUS_PORT                               0x0113

// The following additional error codes are used from the USB Mass Storage Class Driver.
#define USBH_STATUS_LENGTH                             (0x0200)
#define USBH_STATUS_COMMAND_FAILED                     (0x0202)
#define USBH_STATUS_INTERFACE_PROTOCOL                 (0x0203)
#define USBH_STATUS_INTERFACE_SUB_CLASS                (0x0204)
#define USBH_STATUS_SENSE_STOP                         (0x0205)
#define USBH_STATUS_SENSE_REPEAT                       (0x0206)
#define USBH_STATUS_WRITE_PROTECT                      (0x0207)

// System errors
#define USBH_STATUS_INVALID_ALIGNMENT                  0x1007
#define USBH_STATUS_MEMORY                             0x1008
#define USBH_STATUS_RESOURCES                          0x1009
#define USBH_STATUS_DEMO                               0x2000

// URBs HcFlags allowed values
#define URB_CANCEL_PENDING_MASK                       0x01 // Pending URB must be canceled

// pHub initialization state machine
typedef enum {
  USBH_SUBSTATE_IDLE,                                     // Idle, if an URB is completed or if no URb is submitted and an Timeout occurrs!
  USBH_SUBSTATE_TIMER,                                    // USBH_URB_SubStateWait on success
  USBH_SUBSTATE_TIMERURB,                                 // USBH_URB_SubStateSubmitRequest on success
  USBH_SUBSTATE_TIMEOUT_PENDING_URB                       // On Timeout and pending URB
} USBH_SUBSTATE_STATE;

typedef void USBH_SUBSTATE_FUNC(void *context); // Is only called on an Timeout.

// @doc
// @module USBH_DLIST <en-> Double Linked List |
// The module USBH_DLIST provides an implementation of a double linked list.
// The USBH_DLIST structure and the related macros may be used for general purpose.

/*********************************************************************
*
*       Public part (interface)
*
**********************************************************************
*/

/*********************************************************************
*
*       Private part (implementation)
*
**********************************************************************
*/


// Double linked list structure. Can be used as either a list head, or as link words.

// @struct USBH_DLIST |
//   The USBH_DLIST structure is the link element of the double linked list. It is used as either a list head, or as link entry.
// @field  struct tDLIST * | Flink |
//   Pointer to the successor (forward link).
// @field  struct tDLIST * | pPrev |
//   Pointer to the predecessor (backward link).
// @comm By means of such elements any structures may be handled as a double linked list. The USBH_DLIST structure is to be inserted
//   into the structure which is to be handled. A pointer to the original structure can be obtained by means of the macro <f STRUCT_BASE_POINTER>.
typedef struct USBH_DLIST USBH_DLIST;
struct USBH_DLIST {
  USBH_DLIST * pNext;
  USBH_DLIST * pPrev;
};


#define HCM_ASSERT_ITEMHEADER_ADDRESS(pPool,Itemheader)                          \
        USBH_ASSERT(    (void*) (Itemheader) >= (void*)(pPool)->pItemHeaderStartAddr \
                  && (void*) (Itemheader) <= (void*)(pPool)->pItemHeaderEndAddr)

#define HCM_PHY_ADDR_IN_POOL(pPool, PhyAddr)                                     \
        (    (PhyAddr) >= (pPool) ->ContiguousMemoryPhyAddr                      \
          && (PhyAddr) <= (pPool) ->EndContiguousMemoryPhyAddr)

#define HCM_ASSERT_PHY_MEMORY(pPool,PhyAddr) USBH_ASSERT(HCM_PHY_ADDR_IN_POOL((pPool), (PhyAddr))

//
// Generic pool handling
//
#define HcmPoolEmpty(pPool) ((pPool)->pHead == NULL) // Returns true if pool is empty

#if (USBH_DEBUG > 1)                            // Get an item from pool / MUST ONLY be called if pool is NOT empty!
  #define HcmPoolGet(pPool, Item) {    \
    (Item)       = (pPool)->pHead;      \
    (pPool)->RefCount++;               \
    (pPool)->pHead = (Item)->Link.Next; \
  }
#else
  #define HcmPoolGet(pPool, Item) {    \
    (Item)       = (pPool)->pHead;      \
    (pPool)->pHead = (Item)->Link.Next; \
  }
#endif

#if (USBH_DEBUG > 1)                              // Put an item into pool / implements a LIFO scheme
  #define HcmPoolPut(pPool, Item)      {   \
    (Item)->Link.Next = (pPool)->pHead;  \
    USBH_ASSERT((pPool)->RefCount != 0); \
    (pPool)->RefCount--;                 \
    (pPool)->pHead      = (Item);        \
  }
#else
  #define HcmPoolPut(pPool, Item) {     \
    (Item)->Link.Next = (pPool)->pHead; \
    (pPool)->pHead      = (Item);       \
  }
#endif

// With USBH_HCM_ITEM_HEADER begins every pool item allocated in virtual memory. The user can extend this pool header by the
// parameter SizeOfExtension in the function  Hcm_AllocPool.
typedef struct USBH_HCM_ITEM_HEADER {
#if (USBH_DEBUG > 1)
  U32                 Magic;
#endif
  // link element, used to create chains
  union {
    struct USBH_HCM_ITEM_HEADER * Next;
    USBH_DLIST                    ListEntry;
  } Link;
  volatile U32           PhyAddr;            // Physical start address of associated contiguous memory
  struct USBH_HCM_POOL * pOwningPool;         // Pointer to pool the descriptor was allocated from
} USBH_HCM_ITEM_HEADER;

typedef struct USBH_HCM_POOL {
#if (USBH_DEBUG > 1)
  U32 Magic;
#endif
  U32               ContiguousMemoryPhyAddr;
  void            * pItemHeaderStartAddr;      // Start address of not contiguous memory, used to release the memory
  USBH_HCM_ITEM_HEADER * pHead;                     // Pointer to first buffer, pHead==NULL if pool is empty

  // Additional fields used for calculations
  U32               NumberOfItems;            // Number of items should be grater than zero
  U32               SizeOfItem;               // Size in bytes of item in contiguous non paged physical memory
  U32               SizeOfExtension;          // Size in bytes of an item in not contiguous memory

  // Only for debug
  void            * pItemHeaderEndAddr;
  U32               EndContiguousMemoryPhyAddr;
  U32               RefCount;                 // GetItem and PutItem validation ref.counter*/
} USBH_HCM_POOL;

// Get USBH_HCM_ITEM_HEADER pointer from pointer to embedded USBH_DLIST ListEntry

// Get pointer to extra space allocated after USBH_HCM_ITEM_HEADER
#define HCM_ITEM_HEADER_EXTRA_SPACE(ItemHeaderPtr) ((void*)((ItemHeaderPtr) + 1))

// Get USBH_HCM_ITEM_HEADER pointer from pointer to extra space
// lint -emacro((740),HCM_ITEM_HEADER_FROM_EXTRA_SPACE)
#define HCM_ITEM_HEADER_FROM_EXTRA_SPACE(pes) (((USBH_HCM_ITEM_HEADER *)(void *)(pes)) - 1)

USBH_STATUS            USBH_HCM_AllocPool         (USBH_HCM_POOL        * Pool,    U32 NumberOfItems, U32 SizeOfPhysItem, U32 SizeOfExtension, U32 Alignment);
void                   USBH_HCM_FreePool          (USBH_HCM_POOL        * MemPool);
USBH_HCM_ITEM_HEADER * USBH_HCM_GetItem           (USBH_HCM_POOL        * MemPool);
USBH_HCM_ITEM_HEADER * USBH_HCM_GetItemFromPhyAddr(USBH_HCM_POOL        * MemPool, U32 PhyAddr);
void                   USBH_HCM_PutItem           (USBH_HCM_ITEM_HEADER * Item);
void                   USBH_HCM_FillPhyMemory     (USBH_HCM_ITEM_HEADER * Item,    U8 Val);
U32                    USBH_HCM_IsPhysAddrInPool  (USBH_HCM_POOL        * MemPool, U32 PhyAddr);

// The following defines must not be changed!
#define OH_DEV_MAX_TRANSFER_DESC      (9)                           // default value: 7 (reserved for enumeration)
#define OH_DEV_ENUMERATION_CONTROL_EP 7                             // +1=dummy endpoint
#define USBH_OHCI_MAX_BULK_EP            (USBH_Global.Config.NumBulkEndpoints + 1) // +1=dummy endpoint
#define USBH_OHCI_MAX_CONTROL_EP         (USBH_Global.Config.NumUSBDevices + OH_DEV_ENUMERATION_CONTROL_EP + 1)
#define USBH_OHCI_MAX_INT_EP             (USBH_Global.Config.NumIntEndpoints)
#define USBH_OHCI_MAX_ISO_EP             (USBH_Global.Config.NumIsoEndpoints)



#define OH_DEV_MAX_ISO_TD             (USBH_OHCI_MAX_ISO_EP * OH_DEV_MAX_TRANSFER_DESC)                         // Maximum number of ISO pEp host controller TD's
#define OH_DEV_MAX_BULK_TD            (USBH_OHCI_MAX_BULK_EP * OH_DEV_MAX_TRANSFER_DESC)                              // Maximum number of BULK pEp host controller TD's
#define OH_DEV_MAX_INT_TD             (USBH_OHCI_MAX_INT_EP * OH_DEV_MAX_TRANSFER_DESC)                   // Maximum number of INTERRUPT pEp host controller TD's*, dummy endpoint have not transfer descriptors !!!
#define OH_DEV_MAX_CONTROL_TD         (USBH_OHCI_MAX_CONTROL_EP + (USBH_OHCI_MAX_CONTROL_EP * OH_DEV_MAX_TRANSFER_DESC)) // Maximum number of CONTROL pEp host controller TD's

#define OH_TOTAL_GTD                  (OH_DEV_MAX_BULK_TD + OH_DEV_MAX_CONTROL_TD)                                 // Number transfer and endpoint descriptors.
#define OH_TOTAL_ED                   (USBH_OHCI_MAX_ISO_EP + USBH_OHCI_MAX_INT_EP + USBH_OHCI_MAX_BULK_EP + USBH_OHCI_MAX_CONTROL_EP + OHD_DUMMY_INT_NUMBER)

#define OH_MAX_ED_SIZE                (OH_ED_SIZE + OH_ED_ALIGNMENT)
#define OH_MAX_TD_SIZE                (OH_GTD_SIZE + OH_GTD_ALIGNMENT)

// HCCA
#define OH_HCCA_LENGTH         256
#define OH_HCCA_ALIGNMENT      256
#define OH_HCCA_MAX_SIZE       (OH_HCCA_LENGTH+OH_HCCA_ALIGNMENT)

// This defines includes also the setup buffer for all control endpoint and the additional transfer buffer in the host driver.
// It does not include any from user allocated physical buffer
#define OH_OHCI_MEMORY_SIZE           ((OH_MAX_ED_SIZE*OH_TOTAL_ED) + (OH_TOTAL_GTD*OH_MAX_TD_SIZE) + OH_HCCA_MAX_SIZE + (OH_DEV_MAX_CONTROL_TD*8))

// OHD_BULK_INT_EP AbortMask values
#define EP_ABORT_MASK                 0x0001UL // 1. Endpoint is skipped and an timer with an Timeout of about two frames is started
#define EP_ABORT_SKIP_TIME_OVER_MASK  0x0002UL // 2. Endpoint skip Timeout is over
#define EP_ABORT_PROCESS_FLAG_MASK    0x0004UL // 3. temporary flag to process the aborted endpoint
#define EP_ABORT_START_TIMER_MASK     0x0008UL // additional flag to restart the abort timer in the timer routine if another endpoint

// Only handled interrupts generate are enabled. Unrecoverable error (HC_INT_UE) and ownerchip change (HC_INT_OC) are enabled
// because of detecting invalid host controller states!
#define OH_ENABLED_INTERRUPTS     (HC_INT_WDH |  HC_INT_FNO | HC_INT_RHSC | HC_INT_UE | HC_INT_OC)

// Unhandled interrupts status bits are also checked. Process interrupts does nothing with the following interrupt status bits
#define OH_NOT_HANDLED_INTERRUPTS (HC_INT_SO | HC_INT_SF | HC_INT_RD | HC_INT_UE | HC_INT_OC)

// Frame interval in bit times
#define OH_DEV_NOT_USED_BITTIMES    (210 )
#define OH_DEV_FRAME_INTERVAL       (11999) /* 12000 bits per frame (-1) */
#define OH_DEV_LARGEST_DATA_PACKET  ((6 * (OH_DEV_FRAME_INTERVAL - OH_DEV_NOT_USED_BITTIMES)) / 7)
#define OH_DEV_DEFAULT_FMINTERVAL   ((OH_DEV_LARGEST_DATA_PACKET << 16 ) | OH_DEV_FRAME_INTERVAL)
#define OH_DEV_LOW_SPEED_THRESHOLD  0x0628

#define OH_STOP_DELAY_TIME  2 // Delay time in ms until an stopped endpoint list is not processed for the HC

// Default addresses used e.g. for creating dummy endpoints
#define OH_DEFAULT_DEV_ADDR                 0
#define OH_DEFAULT_EP_ADDR                  0
#define OH_DEFAULT_SPEED                    USBH_FULL_SPEED
#define OH_DEFAULT_MAX_PKT_SIZE             64

// Maximum wait time in the initialization routine to wait for clearing the interrupt routing bit from BIOS after Ownerchangerequest was set!
#define OH_OWNER_CHANGE_WAIT_TIME 500000 // in us = 500 ms
#define OH_OWNER_CHANGE_TEST_TIME   1000 // in us = 1 ms
#define OH_TIME_SOFTWARE_RESET        30 // in us
#define OH_RESUME_TIME             30000 // 30ms in us
#define OH_RESET_STATE_TIMEOUT     30000 // 10ms in us - Timout where the OHCI controller is hold in the reset state

/*********************************************************************
*
*       Endpoint and transfer descriptor definitions and macros
*
**********************************************************************
*/
typedef enum {
  USBH_OHCI_DEV_STATE_HALT,
  USBH_OHCI_DEV_STATE_SUSPEND, // From USBH_OHCI_DEV_STATE_SUSPEND only halt or resume is allowed, if the host is reset then the host not be in suspend!
  USBH_OHCI_DEV_STATE_RESUME,
  USBH_OHCI_DEV_STATE_RUNNING
} USBH_OHCI_DEV_STATE;

// Used to transfer data from bulk control and interrupt endpoints if the transfer memory does not support bus master transfer
typedef struct USBH_OHCI_TRANSFER_BUFFER {
  USBH_HCM_ITEM_HEADER   ItemHeader;
  U8              * pUrbBuffer;       // Current buffer pointer
  U32               Transferred;     // Transferred length
  U32               RemainingLength; // IN: size of URB buffer OUT:not transferred bytes
  int               UsbToHostFlag;   // True: USB IN transfer
} USBH_OHCI_TRANSFER_BUFFER;


/*********************************************************************
*
* The USB Bus Driver Core is a software which handles the complete basic function of a USB host controller. It provides a
* software interface that can be used to implement applications or class drivers on an abstract level.
*
* The USB Bus Driver Core manages the
* - enumeration of the devices
* - hot plug and play handling
* - PnP notification events
* - band width management
* - priority schedule for the transfer types
* - external HUB's (optional)
*
* The USB Bus Driver must be synchronized externally. The USB Bus Driver requires functions from an OS
* like "Wait on an event" or "Wait for a time". See the OS abstraction layer for details.
*
**********************************************************************
*/

// The API interface uses the prefix UBD for USB Bus Driver. This should prevent conflicts with other libraries.

typedef void * USBH_NOTIFICATION_HANDLE; // Handle for the notification

/*********************************************************************
*
*       PnP and enumeration
*
**********************************************************************
*/

typedef unsigned int USBH_INTERFACE_ID; // This ID identifies an interface in a unique way, a value of zero represents an invalid ID!
typedef unsigned int USBH_DEVICE_ID;    // This ID identifies a device in a unique way

// Mask bits for device mask
#define USBH_INFO_MASK_VID       0x0001
#define USBH_INFO_MASK_PID       0x0002
#define USBH_INFO_MASK_DEVICE    0x0004
#define USBH_INFO_MASK_INTERFACE 0x0008
#define USBH_INFO_MASK_CLASS     0x0010
#define USBH_INFO_MASK_SUBCLASS  0x0020
#define USBH_INFO_MASK_PROTOCOL  0x0040

// This structure is used to describe a device. The mask contains the information, which fields are valid.
// If the Mask is 0 the function USBH_ON_PNP_EVENT_FUNC is called for all interfaces.
typedef struct USBH_INTERFACE_MASK {
  U16 Mask;
  U16 VendorId;
  U16 ProductId;
  U16 bcdDevice;
  U8  Interface;
  U8  Class;
  U8  SubClass;
  U8  Protocol;
} USBH_INTERFACE_MASK;

typedef void * USBH_INTERFACE_LIST_HANDLE; // Handle to the interface list

typedef enum {
  USBH_SPEED_UNKNOWN,
  USBH_LOW_SPEED,
  USBH_FULL_SPEED,
  USBH_HIGH_SPEED
} USBH_SPEED;

// This structure contains information about a USB interface and the related device
typedef struct USBH_INTERFACE_INFO {
  USBH_INTERFACE_ID InterfaceId;
  USBH_DEVICE_ID    DeviceId;
  U16               VendorId;
  U16               ProductId;
  U16               bcdDevice;
  U8                Interface;
  U8                Class;
  U8                SubClass;
  U8                Protocol;
  unsigned int      OpenCount;
  U8                ExclusiveUsed;
  USBH_SPEED        Speed;
  U8                acSerialNumber[256]; // The serial number in UNICODE format, not zero terminated
  U8                SerialNumberSize;  // The size of the serial number, 0 means not available or error during request
  U8                NumConfigurations;
} USBH_INTERFACE_INFO;

/*********************************************************************
*
*       Compatibility macros
*
**********************************************************************
*/
#define USBH_GetInterfaceID(hInterfaceList, Index)   USBH_GetInterfaceId(hInterfaceList, Index)

USBH_INTERFACE_LIST_HANDLE USBH_CreateInterfaceList (USBH_INTERFACE_MASK        * pInterfaceMask, unsigned int * pInterfaceCount);
void                       USBH_DestroyInterfaceList(USBH_INTERFACE_LIST_HANDLE   hInterfaceList);
USBH_INTERFACE_ID          USBH_GetInterfaceId      (USBH_INTERFACE_LIST_HANDLE   hInterfaceList, unsigned int Index);
USBH_STATUS                USBH_GetInterfaceInfo    (USBH_INTERFACE_ID            InterfaceId, USBH_INTERFACE_INFO * pInterfaceInfo);

// Events for the PnP function
typedef enum {
  USBH_ADD_DEVICE,
  USBH_REMOVE_DEVICE
} USBH_PNP_EVENT;

// This function is called by the USB Bus Driver Core if a PnP event occurs.
// It is typically called the first time in the pContext of USBH_RegisterPnPNotification.
typedef void USBH_ON_PNP_EVENT_FUNC(void * pContext, USBH_PNP_EVENT Event, USBH_INTERFACE_ID InterfaceId);

// struct USBH_PNP_NOTIFICATION is used as parameter to notification functions
typedef struct USBH_PNP_NOTIFICATION {
  USBH_ON_PNP_EVENT_FUNC * pfPnpNotification; // The notification function
  void                   * pContext;         // The notification pContext, passed to USBH_RegisterPnPNotification
  USBH_INTERFACE_MASK      InterfaceMask;   // Mask to the interface
} USBH_PNP_NOTIFICATION;

USBH_NOTIFICATION_HANDLE           USBH_RegisterPnPNotification    (USBH_PNP_NOTIFICATION    * pPnPNotification);
void                               USBH_UnregisterPnPNotification  (USBH_NOTIFICATION_HANDLE   Handle);
typedef void                     * USBH_ENUM_ERROR_HANDLE; // Handle for the notification

// Error type
#define UDB_ENUM_ERROR_EXTHUBPORT_FLAG  0x01 // The device is connected to an external hub
#define USBH_ENUM_ERROR_RETRY_FLAG      0x02 // The enumeration is retried
#define USBH_ENUM_ERROR_STOP_ENUM_FLAG  0x04 // The enumeration is stopped after retries
#define USBH_ENUM_ERROR_DISCONNECT_FLAG 0x08
// Additional information. The parent port status is disconnected, this means the USB device is not connected or it is connected
// and has an error. USBH_RestartEnumError() does nothing if  the  port status is disconnected.

// Error location
#define USBH_ENUM_ERROR_ROOT_PORT_RESET 0x10 // Error during reset of a USB device on an root hub port
#define USBH_ENUM_ERROR_HUB_PORT_RESET  0x20 // Error during reset of a USB device on an external hub port
#define UDB_ENUM_ERROR_INIT_DEVICE      0x30 // Error during initialization of an device until it is in the configured state
#define UDB_ENUM_ERROR_INIT_HUB         0x40 // Error during initialization of an configured external hub device until the installation of an interrupt IN status pipe
#define USBH_ENUM_ERROR_LOCATION_MASK  (USBH_ENUM_ERROR_ROOT_PORT_RESET      | \
                                        USBH_ENUM_ERROR_HUB_PORT_RESET       | \
                                        UDB_ENUM_ERROR_INIT_DEVICE           | \
                                        UDB_ENUM_ERROR_INIT_HUB)

// This struct is only for information
typedef struct USBH_ENUM_ERROR {
  int         Flags;
  int         PortNumber;
  USBH_STATUS Status;
  int         ExtendedErrorInformation; // For internal contains an state value
} USBH_ENUM_ERROR;

// This function is called by the USB Bus Driver Core if a error during USB device enumeration occurs.
// To install this notification routine USBH_RegisterEnumErrorNotification() must be called.
typedef void           USBH_ON_ENUM_ERROR_FUNC             (void * pContext, const USBH_ENUM_ERROR   * pfEnumError);
USBH_ENUM_ERROR_HANDLE USBH_RegisterEnumErrorNotification  (void * pContext, USBH_ON_ENUM_ERROR_FUNC * pfEnumErrorCallback);
void                   USBH_UnregisterEnumErrorNotification(USBH_ENUM_ERROR_HANDLE Handle);
void                   USBH_RestartEnumError               (void);

// Used to access an interface
typedef void        * USBH_INTERFACE_HANDLE;
USBH_STATUS           USBH_OpenInterface  (USBH_INTERFACE_ID InterfaceId, U8 Exclusive, USBH_INTERFACE_HANDLE * InterfaceHandle);
void                  USBH_CloseInterface (USBH_INTERFACE_HANDLE hInterface);

/*******************************************************************************
*
*       Information requests
*
********************************************************************************
*/

USBH_STATUS USBH_GetDeviceDescriptor              (USBH_INTERFACE_HANDLE hInterface, U8 * pBuffer, unsigned int * pBufferSize);
USBH_STATUS USBH_GetCurrentConfigurationDescriptor(USBH_INTERFACE_HANDLE hInterface, U8 * pBuffer, unsigned int * pBufferSize);
USBH_STATUS USBH_GetSerialNumber                  (USBH_INTERFACE_HANDLE hInterface, U8 * pBuffer, unsigned int * pBufferSize);
USBH_STATUS USBH_GetInterfaceDescriptor           (USBH_INTERFACE_HANDLE hInterface, U8 AlternateSetting, U8 * pBuffer,   unsigned int * pBufferSize);

// Mask bits for device mask
#define USBH_EP_MASK_INDEX     0x0001
#define USBH_EP_MASK_ADDRESS   0x0002
#define USBH_EP_MASK_TYPE      0x0004
#define USBH_EP_MASK_DIRECTION 0x0008

typedef struct USBH_EP_MASK {
  U32 Mask;
  U8  Index;
  U8  Address;
  U8  Type;
  U8  Direction;
} USBH_EP_MASK;

// It returns the endpoint descriptor for a given endpoint or returns with status invalid parameter.
USBH_STATUS USBH_GetEndpointDescriptor(USBH_INTERFACE_HANDLE hInterface, U8 AlternateSetting, const USBH_EP_MASK * pMask, U8 * pBuffer, unsigned int * pBufferSize);
USBH_STATUS USBH_GetDescriptor        (USBH_INTERFACE_HANDLE hInterface, U8 AlternateSetting,   U8 Type, U8 * pBuffer, unsigned * pBufferSize);
USBH_STATUS USBH_GetDescriptorEx      (USBH_INTERFACE_HANDLE hInterface, U8 Type, U8 DescIndex, U16 LangId, U8 * pBuffer, unsigned * pBufferSize);

// Return the operating speed of the device.
USBH_STATUS USBH_GetSpeed        (USBH_INTERFACE_HANDLE hInterface, USBH_SPEED * pSpeed);
USBH_STATUS USBH_GetFrameNumber  (USBH_INTERFACE_HANDLE hInterface, U32 * pFrameNumber);

// Returns the interface ID for a given handle
USBH_STATUS USBH_GetInterfaceIdByHandle(USBH_INTERFACE_HANDLE hInterface, USBH_INTERFACE_ID * pInterfaceId);

/*********************************************************************
*
*       Async URB based requests
*
**********************************************************************
*/

// Function codes
typedef enum USBH_FUNCTION_TAG {
  USBH_FUNCTION_CONTROL_REQUEST,   // use USBH_CONTROL_REQUEST
  USBH_FUNCTION_BULK_REQUEST,      // use USBH_BULK_INT_REQUEST
  USBH_FUNCTION_INT_REQUEST,       // use USBH_BULK_INT_REQUEST
  USBH_FUNCTION_ISO_REQUEST,       // use USBH_ISO_REQUEST
  // A device reset starts a new enumeration of the device. PnP events for all related interfaces occur.
  USBH_FUNCTION_RESET_DEVICE,      // use USBH_HEADER
  USBH_FUNCTION_RESET_ENDPOINT,    // use USBH_ENDPOINT_REQUEST
  USBH_FUNCTION_ABORT_ENDPOINT,    // use USBH_ENDPOINT_REQUEST
  USBH_FUNCTION_SET_CONFIGURATION, // use USBH_SET_CONFIGURATION
  USBH_FUNCTION_SET_INTERFACE,     // use USBH_SET_INTERFACE
  USBH_FUNCTION_SET_POWER_STATE
} USBH_FUNCTION;

typedef struct USBH_URB USBH_URB;
typedef void USBH_ON_COMPLETION_FUNC(USBH_URB * Urb); // The completion function
typedef void USBH_ON_COMPLETION_USER_FUNC(void * pContextData);

typedef struct USBH_HEADER_TAG {                            // Is common for all URB based requests
  USBH_FUNCTION                  Function;                       // Function code defines the operation of the URB
  USBH_STATUS                    Status;                         // Is returned by the USB Bus Driver Core
  USBH_ON_COMPLETION_FUNC      * pfOnCompletion;                 // Completion function, must be a valid pointer is not optional
  void                         * pContext;                       // This member can be used by the caller to store a pContext, it is not changed by the USB Bus Driver Core
  USBH_DLIST                     ListEntry;                      // List entry to keep the URB in a list, the owner can use this entry
  // For internal use
  void                         * pUbdContext;                     // Context used from the UBD driver
  USBH_ON_COMPLETION_FUNC      * pfOnInternalCompletion;          // Completion function, must be a valid pointer is not optional
  void                         * pInternalContext;                // This member can be used by the caller to store a pContext, it is not changed by the USB Bus Driver Core
  U32                            HcFlags;                         // Context used from the Host controller driver
  USBH_ON_COMPLETION_USER_FUNC * pfOnUserCompletion;
  void                         * pUserContext;
} USBH_HEADER;


// Used with USBH_FUNCTION_CONTROL_REQUEST
typedef struct USBH_CONTROL_REQUEST {
  USBH_SETUP_PACKET   Setup;    // The setup packet, direction of data phase, the length field must be valid!
  U8                  Endpoint; // Use 0 for default endpoint
  void              * pBuffer;   // Pointer to the caller provided storage, can be NULL
  U32                 Length;   // IN:- OUT: bytes transferred
} USBH_CONTROL_REQUEST;

// Used with USBH_FUNCTION_BULK_REQUEST and USBH_FUNCTION_INT_REQUEST
typedef struct USBH_BULK_INT_REQUEST {
  U8     Endpoint; // With direction bit
  void * pBuffer;
  U32    Length;   // IN: length in bytes of buffer OUT: bytes transferred
} USBH_BULK_INT_REQUEST;

// A helper struct to define ISO requests. Each struct of this type describes the amount of data for one USB frame.
// The data structure ISO_REQUEST ends with an array of such structures.
typedef struct USBH_ISO_PACKET_DESCRIPTOR {
  U32         Offset;
  U32         Length;
  USBH_STATUS Status;
} USBH_ISO_PACKET_DESCRIPTOR;

// Start the transfer as soon as possible
#define USBH_ISO_ASAP 0x01
#define USBH_URB_GET_ISO_URB_SIZE(NumPackets) (sizeof(USBH_HEADER) + sizeof(USBH_ISO_REQUEST) + (NumPackets) * sizeof(USBH_ISO_PACKET_DESCRIPTOR))


// Used with USBH_FUNCTION_ISO_REQUEST. At the end of this data structure is an variable sized array of ISO_FRAME.
// The macro GET_ISO_URB_SIZE returns the size of this URB for a given number of frames.
typedef struct USBH_ISO_REQUEST {
  U8             Endpoint;   // With direction bit
  void         * pBuffer; // Pointer to the caller provided storage, can be NULL
  U32            Length;    // IN: buffer size, OUT: bytes transferred
  unsigned int   Flags;
  unsigned int   StartFrame;
  unsigned int                NumPackets;
  unsigned int                ErrorCount;
  USBH_ISO_PACKET_DESCRIPTOR  aIsoPacket[1];
} USBH_ISO_REQUEST;

typedef struct USBH_ENDPOINT_REQUEST {  // Used with USBH_FUNCTION_ABORT_ENDPOINT and USBH_FUNCTION_RESET_ENDPOINT.
  U8 Endpoint;                          // With direction bit
} USBH_ENDPOINT_REQUEST;

typedef struct USBH_SET_CONFIGURATION { // Used with USBH_FUNCTION_SET_CONFIGURATION. Changing the configuration caused PnP events for all interfaces.
  U8 ConfigurationDescriptorIndex;      // Zero based
} USBH_SET_CONFIGURATION;

typedef struct USBH_SET_INTERFACE {     // Used with USBH_FUNCTION_SET_INTERFACE. The interface is given by the handle.
  U8 AlternateSetting;                  // Zero based
} USBH_SET_INTERFACE;

typedef enum USBH_POWER_STATE {
  USBH_NORMAL_POWER,
  USBH_SUSPEND
} USBH_POWER_STATE;

typedef struct USBH_SET_POWER_STATE {
  USBH_POWER_STATE PowerState;
} USBH_SET_POWER_STATE;

struct USBH_URB {                    // Common USB request block structure. It is used for all async. Requests
  USBH_HEADER Header;
  union {
    USBH_CONTROL_REQUEST   ControlRequest;
    USBH_BULK_INT_REQUEST  BulkIntRequest;
    USBH_ISO_REQUEST       IsoRequest;
    USBH_ENDPOINT_REQUEST  EndpointRequest;
    USBH_SET_CONFIGURATION SetConfiguration;
    USBH_SET_INTERFACE     SetInterface;
    USBH_SET_POWER_STATE   SetPowerState;
  } Request;
};

// Interface function for all asynchronous requests. If the function returns USBH_STATUS_PENDING the completion routine is called.
// On each other status code the completion routine is never called. The storage of the URB must be provided by the caller and
// must be permanent until the URB is returned by the completion routine.
USBH_STATUS   USBH_SubmitUrb(USBH_INTERFACE_HANDLE hInterface, USBH_URB * Urb);
const char  * USBH_GetStatusStr(USBH_STATUS x);

typedef void * USBH_HC_BD_HANDLE; // Handle of the bus driver of a host controller
typedef void * USBH_HC_HANDLE;    // Context for the host controller driver

USBH_STATUS USBH_BD_PreInit     (void);
USBH_STATUS USBH_BD_Init        (void);
void        USBH_Exit            (void);
void        USBH_EnumerateDevices(USBH_HC_BD_HANDLE HcBdHandle);

typedef enum USBH_HOST_STATE {
  USBH_HOST_RESET,                                                                          // Do nothing on the ports, power off
  USBH_HOST_RUNNING,                                                                        // Turn on generation of SOF
  USBH_HOST_SUSPEND                                                                         // Stop processing of all queues, stop SOF's
} USBH_HOST_STATE;

typedef void *      USBH_HC_EP_HANDLE;                                                           // Handle to an endpoint
typedef enum {
  USBH_PORT_POWER_RUNNING,
  USBH_PORT_POWER_SUSPEND
} USBH_PORT_POWER_STATE;


/*********************************************************************
*
*       Host controller interface
*
**********************************************************************
*/

typedef void        USBH_ROOT_HUB_NOTIFICATION_FUNC(void * pContext, U32 Notification); // bit0 indicates a status change of the HUB, bit 1 of port 1 of the hub and so on.
// Is called in the pContext of USBH_AddHostController make a basic initialization of the hardware,
// reset the hardware, setup internal lists, leave the host in the state  UBB_HOST_RESET
typedef USBH_STATUS USBH_HOST_INIT_FUNC(USBH_HC_HANDLE hHostController, USBH_ROOT_HUB_NOTIFICATION_FUNC * pfUbdRootHubNotification, void * pRootHubNotificationContext);

// Is the last call on this interface. It is called after all URB's are returned, all endpoints are released and no further
// reference to the host controller exists. In this call the host controller driver can check that all lists (URB's, Endpoints)
// are empty and delete all resources, disable interrupts. The HC state is USBH_HOST_RESET if this function is called
typedef USBH_STATUS USBH_HOST_EXIT_FUNC(USBH_HC_HANDLE hHostController);


typedef USBH_STATUS USBH_SET_HC_STATE_FUNC       (USBH_HC_HANDLE   hHostController, USBH_HOST_STATE HostState); // Set the state of the HC
typedef U32         USBH_GET_HC_FRAME_NUMBER_FUNC(USBH_HC_HANDLE   hHostController);                       // Returns the frame number as a 32 bit value
typedef void        USBH_HC_ON_COMPLETION_FUNC   (void          * Context,  USBH_URB           * Urb);
// Returns an endpoint handle for a new created endpoint
// EndpointType :   Type of the endpoint, one of USB_EP_TYPE_CONTROL, ...
// DeviceAddress:   Device address, 0 is allowed
// EndpointAddress: Endpoint address with direction bit
// MaxFifoSize:     Maximum transfer FIFO size in the host controller for that endpoint
// IntervalTime:    Interval time in or the NAK rate if this is an USB high speed bulk endpoint (in milliseconds)
// Speed:           The speed of the endpoint
typedef USBH_HC_EP_HANDLE USBH_ADD_ENDPOINT_FUNC(USBH_HC_HANDLE hHostController, U8 EndpointType, U8 DeviceAddress, U8 EndpointAddress, U16 MaxFifoSize, U16 IntervalTime, USBH_SPEED Speed);
// The bus driver calls these functions with valid handles. The HC must not take care to check the handles.
typedef void        USBH_RELEASE_EP_COMPLETION_FUNC(void * pContext);                         // This is the  USBH_ReleaseEndpoint completion function.
typedef void        USBH_RELEASE_ENDPOINT_FUNC     (USBH_HC_EP_HANDLE hEndPoint, USBH_RELEASE_EP_COMPLETION_FUNC * pfReleaseEpCompletion, void * pContext);                      // Releases that endpoint. This function returns immediately. If the Completion function is called the endpoint is removed.
typedef USBH_STATUS USBH_ABORT_ENDPOINT_FUNC       (USBH_HC_EP_HANDLE hEndPoint);             // Complete all pending requests. This function returns immediately. But the URB's may completed delayed, if the hardware require this.
typedef USBH_STATUS USBH_RESET_ENDPOINT_FUNC       (USBH_HC_EP_HANDLE hEndPoint);             // Resets the data toggle bit to 0. The bus driver takes care that this function is called only if no pending URB for this EP is scheduled.
typedef USBH_STATUS USBH_SUBMIT_REQUEST_FUNC       (USBH_HC_EP_HANDLE hEndPoint, USBH_URB * pUrb); // Submit a request to the HC. If USBH_STATUS_PENDING is returned the request is in the queue and the completion routine is called later.

/*********************************************************************
*
*       Root pHub Functions
*
**********************************************************************
*/

typedef unsigned int USBH_GET_PORT_COUNT_FUNC     (USBH_HC_HANDLE hHostController); // Returns the number of root hub ports. An zero value is returned on an error.
typedef unsigned int USBH_GET_POWER_GOOD_TIME_FUNC(USBH_HC_HANDLE hHostController); // Returns the power on to power good time in ms
typedef U32          USBH_GET_HUB_STATUS_FUNC     (USBH_HC_HANDLE hHostController); // Returns the HUB status as defined in the USB specification 11.24.2.6

// This request is identical to an hub class ClearHubFeature request with the restriction that only HUB CHANGE bits can be cleared.
// For all other hub features other root hub functions must be used. The physical change bits are cleared in the root hub interrupt routine.
typedef void USBH_CLEAR_HUB_STATUS_FUNC (USBH_HC_HANDLE hHostController, U16 FeatureSelector);
typedef U32  USBH_GET_PORT_STATUS_FUNC  (USBH_HC_HANDLE hHostController, U8  Port); // One based index of the port / return the port status as defined in the USB specification 11.24.2.7

// This request is identical to an hub class ClearPortFeature request with the restriction that only PORT CHANGE bits can be cleared.
// For all other port features other root hub functions must be used. The physical change bits are cleared in the root hub interrupt routine.
typedef void USBH_CLEAR_PORT_STATUS_FUNC(USBH_HC_HANDLE hHostController, U8  Port, U16 FeatureSelector); // One based index of the port

// Set the power State of a port. If the HC cannot handle the power switching for individual ports, it must turn on all ports if
// at least one port requires power. It turns off the power if no port requires power
typedef void USBH_SET_PORT_POWER_FUNC   (USBH_HC_HANDLE hHostController, U8  Port, U8 PowerOn); // one based index of the port / 1 to turn the power on or 0 for off

// Reset the port (USB Reset) and send a port change notification if ready.
// If reset was successful the port is enabled after reset and the speed is detected
typedef void USBH_RESET_PORT_FUNC       (USBH_HC_HANDLE hHostController, U8  Port); // One based index of the port
typedef void USBH_DISABLE_PORT_FUNC     (USBH_HC_HANDLE hHostController, U8  Port); // One based index of the port// Disable the port, no requests and SOF's are issued on this port
typedef void USBH_SET_PORT_SUSPEND_FUNC (USBH_HC_HANDLE hHostController, U8  Port, USBH_PORT_POWER_STATE State); // One based index of the port / Switch the port power between running and suspend
typedef int  USBH_CHECK_INTERRUPT_FUNC  (USBH_HC_HANDLE hHostController);
typedef void USBH_ISR_FUNC              (USBH_HC_HANDLE hHostController);

typedef struct USBH_HOST_DRIVER {
  // Global HC functions
  USBH_HOST_INIT_FUNC           * pfHostInit;        // Is called by the bus driver in the pContext of USBH_AddHostController
  USBH_HOST_EXIT_FUNC           * pfHostExit;
  USBH_SET_HC_STATE_FUNC        * pfSetHcState;
  USBH_GET_HC_FRAME_NUMBER_FUNC * pfGetFrameNumber;
  // Endpoint functions
  USBH_ADD_ENDPOINT_FUNC        * pfAddEndpoint;     // Add an endpoint to the HC
  USBH_RELEASE_ENDPOINT_FUNC    * pfReleaseEndpoint; // Release the endpoint, free the endpoint structure
  USBH_ABORT_ENDPOINT_FUNC      * pfAbortEndpoint;   // Return all pending requests from this endpoint with status USBH_STATUS_CANCELLED.
  // The requests must not completed with in the function call.
  USBH_RESET_ENDPOINT_FUNC      * pfResetEndpoint;   // Reset the endpoint data toggle bit
  USBH_SUBMIT_REQUEST_FUNC      * pfSubmitRequest;   // Submit a request
  // Root pHub functions
  USBH_GET_PORT_COUNT_FUNC      * pfGetPortCount;
  USBH_GET_POWER_GOOD_TIME_FUNC * pfGetPowerGoodTime;
  USBH_GET_HUB_STATUS_FUNC      * pfGetHubStatus;
  USBH_CLEAR_HUB_STATUS_FUNC    * pfClearHubStatus;
  USBH_GET_PORT_STATUS_FUNC     * pfGetPortStatus;
  USBH_CLEAR_PORT_STATUS_FUNC   * pfClearPortStatus;
  USBH_SET_PORT_POWER_FUNC      * SetPortPower;
  USBH_RESET_PORT_FUNC          * pfResetPort;
  USBH_DISABLE_PORT_FUNC        * pfDisablePort;
  USBH_SET_PORT_SUSPEND_FUNC    * pfSetPortSuspend;
  USBH_CHECK_INTERRUPT_FUNC     * pfCheckIsr;
  USBH_ISR_FUNC                 * pfIsr;
} USBH_HOST_DRIVER;

USBH_HC_BD_HANDLE USBH_AddHostController   (USBH_HOST_DRIVER      * pDriver, USBH_HC_HANDLE hController);
void              USBH_RemoveHostController(USBH_HC_BD_HANDLE       HcBdHandle);
USBH_STATUS       USBH_ResetEndpoint       (USBH_INTERFACE_HANDLE   IfaceHandle, USBH_URB * urb, U8 Endpoint, USBH_ON_COMPLETION_FUNC Completion, void * Context);


typedef void * USBH_TIMER_HANDLE;                                                                   // Handle to a TAL timer object
typedef void   USBH_TIMER_FUNC(void * pContext);                                              // Typedef callback function which is called on a timer Timeout

USBH_TIMER_HANDLE USBH_AllocTimer (USBH_TIMER_FUNC * pfTimerCallbackRoutine, void * Context); // Allocates a timer object and returns the timer handle.
void              USBH_FreeTimer  (USBH_TIMER_HANDLE hTimer);                                  // Frees a timer object via timer handle.
void              USBH_StartTimer (USBH_TIMER_HANDLE hTimer, U32 ms);                          // Starts a timer. The timer is restarted again if it is running.
void              USBH_CancelTimer(USBH_TIMER_HANDLE hTimer);                                  // Cancels an timer if running, the completion routine is not called.

/*********************************************************************
*
*       Structs
*
**********************************************************************
*/


USBH_BOOL USBH_IsTimeOver(U32 Waittime, U32 StartTime);


typedef struct URB_SUB_STATE {
  U8                           TimerCancelFlag; // Timer to for detecting an Timeout
  USBH_TIMER_HANDLE            hTimer;
  USBH_SUBSTATE_STATE               State;
  USBH_URB                   * pUrb;
  // Additional pointer for faster accesses
  USBH_HOST_CONTROLLER       * pHostController;
  USBH_HC_EP_HANDLE          * phEP;
  USBH_SUBMIT_REQUEST_FUNC   * pfSubmitRequest;
  USBH_ABORT_ENDPOINT_FUNC   * pfAbortEndpoint;
  USB_DEVICE                 * pDevRefCnt;
  USBH_SUBSTATE_FUNC         * pfCallback; // This callback routine is called if an URB is complete or on an timer Timeout
  // started with USBH_URB_SubStateWait. If the timer routine runs and an pending pUrb exist
  // then the pUrb ios aborted and the CallbackRoutine is not called.
  void                       * pContext;
} URB_SUB_STATE;

URB_SUB_STATE * USBH_URB_SubStateAllocInit    (USBH_HOST_CONTROLLER * pHostController, USBH_HC_EP_HANDLE * phEP, USBH_SUBSTATE_FUNC * pfRoutine, void * pContext);
USBH_STATUS     USBH_URB_SubStateInit         (URB_SUB_STATE * pSubState, USBH_HOST_CONTROLLER * pHostController, USBH_HC_EP_HANDLE * phEP, USBH_SUBSTATE_FUNC * pfRoutine, void * pContext);
USBH_STATUS     USBH_URB_SubStateSubmitRequest(URB_SUB_STATE * pSubState, USBH_URB * pUrb, U32 Timeout, USB_DEVICE * pDevRefCnt);
void            USBH_URB_SubStateWait         (URB_SUB_STATE * pSubState, U32 Timeout, USB_DEVICE * pDevRefCnt);
void            USBH_URB_SubStateExit         (URB_SUB_STATE * pSubState);
void            USBH_URB_SubStateFree         (URB_SUB_STATE * pSubState);
USBH_BOOL       USBH_URB_SubStateIsIdle       (URB_SUB_STATE * pSubState);

USBH_STATUS USBH_OHCI_TRANSFER_SubmitRequest       (USBH_HC_EP_HANDLE hEP, USBH_URB * pUrb);
void        USBH_OHCI_TRANSFER_ProcessDoneInterrupt(USBH_OHCI_DEVICE * pDev);

// URBs HcFlags allowed values
#define URB_CANCEL_PENDING_MASK 0x01 // Pending URB must be canceled


typedef struct USBH_DEFAULT_EP {
#if (USBH_DEBUG > 1)
  U32                  Magic;
#endif
  USB_DEVICE         * pUsbDevice; // Pointer to the owning host controller
  USBH_HC_EP_HANDLE    hEP;  // Endpoint handle must be used to submit an URB
  unsigned int         UrbCount;
} USBH_DEFAULT_EP;


// State for device enumeration
typedef enum {
  DEV_ENUM_IDLE,                 // No enumeration running
  DEV_ENUM_START,                // First state
  DEV_ENUM_GET_DEVICE_DESC_PART, // Get the first 8 bytes of the device descriptor
  DEV_ENUM_GET_DEVICE_DESC,      // Get the complete device descriptor
  DEV_ENUM_GET_CONFIG_DESC_PART, // Get the first part of the configuration descriptor
  DEV_ENUM_GET_CONFIG_DESC,      // Get the complete configuration descriptor
  DEV_ENUM_GET_LANG_ID,          // Get the language ID's
  DEV_ENUM_GET_SERIAL_DESC,      // Get the serial number
  DEV_ENUM_SET_CONFIGURATION,    // Set the configuration
  DEV_ENUM_INIT_HUB,             // The device is an hub and is  initialized
  DEV_ENUM_RESTART,              // A transaction fails and a timer runs to restart
  DEV_ENUM_REMOVED               // The device is removed, clean up enumeration
} DEV_ENUM_STATE;

typedef enum { // Do not modify the sequence
  DEV_STATE_UNKNOWN = 0,
  DEV_STATE_REMOVED,
  DEV_STATE_ENUMERATE,
  DEV_STATE_WORKING,
  DEV_STATE_SUSPEND
} USB_DEV_STATE;

// Is called after the standard enumeration has been completed
typedef void POST_ENUM_FUNC(void* pContext);

struct USB_DEVICE {
#if (USBH_DEBUG > 1)
  U32                          Magic;
#endif
  USBH_DLIST                   ListEntry;              // To store this object in the host controller object
  USBH_DLIST                   TempEntry;              // To store this object in an temporary list
  USBH_BOOL                    TempFlag;
  long                         RefCount;
  USBH_HOST_CONTROLLER       * pHostController;         // Pointer to the owning host controller
  USBH_DLIST                   UsbInterfaceList;       // List for interfaces
  unsigned int                 InterfaceCount;
  USBH_HUB_PORT              * pParentPort;             // This is the hub port where this device is connected to
  U8                           UsbAddress;             // This is the USB address of the device
  USBH_SPEED                   DeviceSpeed;            // pSpeed of the device connection
  U8                           MaxFifoSize;            // The FIFO size
  U8                           ConfigurationIndex;     // The index of the current configuration
  U8                           NumConfigurations;      // The index of the current configuration
  // Descriptors
  USB_DEVICE_DESCRIPTOR        DeviceDescriptor;       // A typed copy
  U8                           aDeviceDescriptorBuffer[USB_DEVICE_DESCRIPTOR_LENGTH];
  U8                         * pConfigDescriptor;       // Points to the current configuration descriptor
  U16                          ConfigDescriptorSize;
  U16                          LanguageId;             // First language ID
  U8                         * pSerialNumber;           // Serial number without header, UNICODE
  unsigned int                 SerialNumberSize;
  U16                          DevStatus;              // Device status returned from USB GetStatus
  USBH_DEFAULT_EP              DefaultEp;              // Embedded default endpoint
  USB_HUB                    * pUsbHub;                 // This pointer is valid if the device is a hub
  USB_DEV_STATE                State;                  // Current device state
  USBH_URB                     EnumUrb;                // Embedded URB
  void                       * pCtrlTransferBuffer;     // Used from USBH_BD_ProcessEnum and ProcessEnumHub()
  unsigned int                 CtrlTransferBufferSize;
  // State variables for device enumeration
  // Enumeration state
  URB_SUB_STATE                SubState;
  DEV_ENUM_STATE               EnumState;
  USBH_BOOL                    EnumSubmitFlag;         // Used during enumeration if the device is as an hub
  // Post enumeration
  POST_ENUM_FUNC             * pfPostEnumFunction;
  void                       * pPostEnumerationContext;
  USBH_DEVICE_ID               DeviceId;               // Device ID for this device
};


typedef struct USB_ENDPOINT {
  USBH_DLIST          ListEntry;          // to store this object in the interface object */
#if (USBH_DEBUG > 1)
  U32                 Magic;
#endif
  USB_INTERFACE     * pUsbInterface;       // Backward pointer
  U8                * pEndpointDescriptor; // Descriptors
  USBH_HC_EP_HANDLE   hEP;           // Endpoint handle must be used to submit an URB
  U32                 UrbCount;
} USB_ENDPOINT;


#define USB_MAX_ENDPOINTS 32 // Needs the struct and the name of the list entry inside the struct.

// Make a index in the range between 0 and 31 from an EP address IN EP's in the range from 0x10 to 0x1f, OUT EP's are in the range 0x00 to 0x0f
#define USBH_EP_INDEX(EpAddr) ((EpAddr) & 0x80) ? (((EpAddr)&0xf) | 0x10) : ((EpAddr)&0xf)

struct USB_INTERFACE {
#if (USBH_DEBUG > 1)
  U32                 Magic;
#endif
  USBH_DLIST          ListEntry;                // To store this object in the device object
  USB_DEVICE        * pDevice;                   // Backward pointer
  USBH_DLIST          UsbEndpointList;          // List for endpoints
  unsigned int        EndpointCount;
  U8                  CurrentAlternateSetting;
  U8                * pInterfaceDescriptor;
  U8                * pAlternateSettingDescriptor;
  U8                  NewAlternateSetting;
  U8                * pNewAlternateSettingDescriptor;
  unsigned int        OpenCount;
  U8                  ExclusiveUsed;
  USB_ENDPOINT      * pEpMap[USB_MAX_ENDPOINTS]; // A map for fast access to endpoint structures
  USBH_INTERFACE_ID   InterfaceId;              // ID of this interface
};

USBH_STATUS USBH_BD_InitDefaultEndpoint            (USB_DEVICE    * pUsbDevice);
USBH_STATUS USBH_BD_DefaultEpSubmitUrb             (USB_DEVICE    * Dev,       USBH_URB * Urb);
USBH_STATUS USBH_BD_SubmitSetInterface             (USB_INTERFACE * UsbInterface, U16 Interface, U16 AlternateSetting, USBH_ON_COMPLETION_FUNC * Completion, USBH_URB * OriginalUrb);
USBH_STATUS USBH_BD_SubmitClearFeatureEndpointStall(USBH_DEFAULT_EP   * DefaultEp, USBH_URB * Urb, U8 Endpoint, USBH_ON_COMPLETION_FUNC * InternalCompletion, void * HcContext);
void        USBH_BD_ReleaseDefaultEndpoint         (USBH_DEFAULT_EP   * UsbEndpoint);
void        USBH_BD_DefaultEpUrbCompletion         (USBH_URB          * Urb);

// Dump the info of an allocated device /DBG_ADDREMOVE must be set
#define DUMP_USB_DEVICE_INFO(UsbDev)                                                               \
USBH_LOG((USBH_MTYPE_DEVICE, "Device: Added Dev: USB addr: %d Id:%u speed: %s parent port: %d %s", \
      (UsbDev)->DeviceId,                                                                          \
      (int)(UsbDev)->UsbAddress,                                                                   \
      USBH_PortSpeed2Str((UsbDev)->DeviceSpeed),                                                      \
      (UsbDev)->pParentPort != NULL ? (int)(UsbDev)->pParentPort->HubPortNumber: -1,                 \
      ((UsbDev)->DevStatus & USB_STATUS_SELF_POWERED) ? "self powered" : "bus powered"))

// Needs the struct and the name of the list entry inside the struct
#define INC_REF(devPtr) (devPtr)->RefCount++ // Reference counting macros to the USB_DEVICE object

#if (USBH_DEBUG > 1)
#define DEC_REF(devPtr)                                                                       \
  (devPtr)->RefCount--;                                                                       \
  if ((devPtr)->RefCount == 1) {                                                              \
    USBH_LOG((USBH_MTYPE_CORE, "DEC_REF RefCount is 1 %s(%d)\n", __FILE__, __LINE__));        \
  }                                                                                           \
  if ((devPtr)->RefCount <  0) {                                                              \
    USBH_LOG((USBH_MTYPE_CORE, "DEC_REF RefCount less than 0 %s(%d)\n", __FILE__, __LINE__)); \
  }                                                                                           \
  if ((devPtr)->RefCount == 0) {                                                              \
    USBH_LOG((USBH_MTYPE_CORE, "DEC_REF RefCount is 0 %s(%d)\n", __FILE__, __LINE__));        \
    USBH_DeleteDevice(devPtr);                                                                  \
  }
#else
#define DEC_REF(devPtr)          \
  (devPtr)->RefCount--;          \
  if ((devPtr)->RefCount == 0) { \
    USBH_DeleteDevice(devPtr);     \
  }
#endif

USB_DEVICE * USBH_CreateNewUsbDevice(USBH_HOST_CONTROLLER * HostController);

void         USBH_StartEnumeration                  (USB_DEVICE * Dev, POST_ENUM_FUNC * PostEnumFunction, void * Context);
void         USBH_DeleteDevice                      (USB_DEVICE * Dev);
void         USBH_DeleteInterfaces                  (USB_DEVICE * Dev);
void         USBH_MarkDeviceAsRemoved               (USB_DEVICE * Dev);
void         USBH_MarkParentAndChildDevicesAsRemoved(USB_DEVICE * Dev);
void         UbdProcessSetConf                      (USB_DEVICE * Dev);
U8         * USBH_GetNextInterfaceDesc              (USB_DEVICE * Dev, U8  * Start, U8 InterfaceNumber, unsigned int AlternateSetting);
U8         * USBH_GetNextEndpointDesc               (USB_DEVICE * Dev, U8  * Start, U8 Endpoint);
void         USBH_CreateInterfaces                  (void * Context);
U16          USBH_GetUshortFromDesc                 (void * Buffer,    U16   Offset);
U8           USBH_GetUcharFromDesc                  (void * Buffer,    U16   Offset);

/*********************************************************************
*
*       Helper functions
*
**********************************************************************
*/

void         USBH_BD_EnumTimerFunction      (void       * Context);
void         USBH_BD_SetConfTimerFunction   (void       * Context);
void         USBH_BD_ProcessEnum            (void       * usbDevice);
unsigned int USBH_GetPendingUrbCount        (USB_DEVICE * Dev);
int          USBH_CheckCtrlTransferBuffer   (USB_DEVICE * Dev, U16                   RequestLength);
void         USBH_EnumParentPortRestart     (USB_DEVICE * Dev, USBH_STATUS           status);
void         USBH_ProcessEnumPortError      (USB_DEVICE * dev, USBH_STATUS           enumStatus);
USBH_STATUS  USBH_SearchUsbInterface        (USB_DEVICE * dev, USBH_INTERFACE_MASK * iMask, USB_INTERFACE * * iface);

USBH_STATUS    USBH_GetEndpointDescriptorFromInterface(USB_INTERFACE * usbInterface, U8 alternateSetting, const USBH_EP_MASK * mask, U8 * * descriptor);
USBH_STATUS    USBH_GetDescriptorFromInterface(USB_INTERFACE * pUsbInterface, U8 alternateSetting, U8 Type, U8 ** ppDescriptor);
USB_ENDPOINT * USBH_BD_NewEndpoint    (USB_INTERFACE * UsbInterface, U8 * EndpointDescriptor);
void           USBH_BD_DeleteEndpoint (USB_ENDPOINT  * UsbEndpoint);
USBH_STATUS    USBH_BD_EpSubmitUrb    (USB_ENDPOINT  * UsbEndpoint, USBH_URB * Urb);
void           USBH_BD_EpUrbCompletion(USBH_URB      * Urb);

// Needs the struct and the name of the list entry inside the struct
#define GET_EP_FROM_ADDRESS(uif,ep)     uif->pEpMap[((ep) & 0xf) | (((ep) & 0x80) >> 3)]
#define SET_EP_FOR_ADDRESS( uif,ep,uep) uif->pEpMap[((ep) & 0xf) | (((ep) & 0x80) >> 3)] = (uep)

USB_INTERFACE * USBH_BD_NewUsbInterface    (USB_DEVICE    * pDevice);
void            USBH_BD_DeleteUsbInterface (USB_INTERFACE * pUsbInterface);
USBH_STATUS     USBH_BD_CreateEndpoints    (USB_INTERFACE * pUsbInterface);
void            USBH_BD_RemoveEndpoints    (USB_INTERFACE * pUsbInterface);
void            USBH_BD_AddUsbEndpoint     (USB_ENDPOINT  * pUsbEndpoint);
void            USBH_RemoveUsbEndpoint     (USB_ENDPOINT  * pUsbEndpoint);
USBH_STATUS     USBH_BD_CompareUsbInterface(USB_INTERFACE * pInterface, USBH_INTERFACE_MASK * pInterfaceMask, USBH_BOOL EnableHubInterfaces);
unsigned int    USBH_BD_GetPendingUrbCount (USB_INTERFACE * pInterface);
void            USBH_BD_AddUsbInterface    (USB_INTERFACE * pUsbInterface);
void            USBH_BD_RemoveUsbInterface (USB_INTERFACE * pUsbInterface);

USB_ENDPOINT  * USBH_BD_SearchUsbEndpointInInterface(USB_INTERFACE * Interface, const USBH_EP_MASK * mask);

typedef struct USBH_HOST_DRIVER_INST { // The global driver object
#if (USBH_DEBUG > 1)
  U32               Magic;
#endif
  USBH_DLIST        HostControllerList;
  U32               HostControllerCount;
  // Registered PNP notifications
  USBH_DLIST        NotificationList;
  U32               NotificationCount;
  // Delayed Pnp notifications, called in an timer routine
  USBH_DLIST        DelayedPnPNotificationList;
  U32               DelayedPnPNotificationCount;
  USBH_TIMER_HANDLE DelayedPnPNotifyTimer;
  USBH_DLIST        EnumErrorNotificationList;
  U32               EnumErrorNotificationCount;
  // Next free ID's for a new enumerated device
  USBH_INTERFACE_ID NextInterfaceId;
  USBH_DEVICE_ID    NextDeviceId;
} USBH_HOST_DRIVER_INST;

void USBH_BD_AddHostController            (USBH_HOST_CONTROLLER * HostController);
void USBH_BD_RemoveHostController         (USBH_HOST_CONTROLLER * HostController);
void USBH_BD_ProcessDevicePnpNotifications(USB_DEVICE * Device, USBH_PNP_EVENT event);
void USBH_BD_AddNotification              (USB_DEVICE * Device);
void USBH_BD_RemoveNotification           (USB_DEVICE * Device);

USBH_INTERFACE_ID   USBH_BD_GetNextInterfaceId(void);
USBH_DEVICE_ID      USBH_BD_GetNextDeviceId   (void);
USB_DEVICE        * USBH_BD_GetDeviceById     (USBH_DEVICE_ID    DeviceId);
USB_INTERFACE     * USBH_BD_GetInterfaceById     (USBH_INTERFACE_ID InterfaceId);


typedef enum {
  RH_PORTRESET_IDLE, // Only this state allows an new root hub port reset
  RH_PORTRESET_REMOVED,
  RH_PORTRESET_INIT, // RH_PORTRESET_INIT prevents starting of root hub enumeration until power good time is elapsed!
  RH_PORTRESET_START,
  RH_PORTRESET_RESTART,
  // Following states are always entered in the pContext of a callback completion routine never by direct calling of RootHubProcessDeviceReset()
  RH_PORTRESET_WAIT_RESTART,
  RH_PORTRESET_RES,
  RH_PORTRESET_WAIT_RESET,
  RH_PORTRESET_SET_ADDRESS,
  RH_PORTRESET_WAIT_ADDRESS
} USBH_ROOT_HUB_PORTRESET_STATE;

typedef struct ROOT_HUB {                       // The global driver object
#if (USBH_DEBUG > 1)
  U32                             Magic;
#endif
  USBH_HOST_CONTROLLER          * pHostController;      // Backward pointer to the host controller
  unsigned int                    PowerGoodTime;       // Power on to power good time in ms
  unsigned int                    PortCount;           // Number of ports
  USBH_DLIST                      PortList;
  URB_SUB_STATE                   SubState;            // Sub state machine for device reset and set address,  easier handling if both an timer and URB is started!
  URB_SUB_STATE                   InitHubPortSubState;
  USBH_ROOT_HUB_PORTRESET_STATE   PortResetEnumState;
  USBH_HUB_PORT                 * pEnumPort;
  USB_DEVICE                    * pEnumDevice;
  USBH_URB                        EnumUrb;             // Embedded URB
  USBH_HC_EP_HANDLE               hEnumEP;
} ROOT_HUB;

USBH_STATUS     USBH_ROOTHUB_Init                      (USBH_HOST_CONTROLLER * pHostController);
void            USBH_ROOTHUB_OnNotification            (void     * pRootHubContext, U32 Notification);
void            USBH_ROOTHUB_Release                   (ROOT_HUB * pRootHub);
USBH_STATUS     USBH_ROOTHUB_AddPortsStartPowerGoodTime(ROOT_HUB * pRootHub);
USBH_BOOL       USBH_ROOTHUB_ServicePorts              (ROOT_HUB * pRootHub);
USBH_HUB_PORT * USBH_ROOTHUB_GetPortByNumber           (ROOT_HUB * pRootHub, U8 Port);

#define DEFAULT_NOTIFY_RETRY_TIMEOUT      100
#define USBHUB_DEFAULT_ALTERNATE_SETTING  0
#define USBHUB_DEFAULT_INTERFACE          0

typedef enum {
  PORT_UNKNOWN,
  PORT_REMOVED,               // Set from notification
  PORT_CONNECTED,             // Set from notification
  PORT_RESTART,               // Set from notification or enumeration (both functions are synchronized)
  PORT_SUSPEND,               // Set from notification
  PORT_RESET,                 // Set from enumeration
  PORT_ENABLED,               // Set from enumeration
  PORT_ERROR                  // Errors during port enumeration
} PORT_STATE;

struct USBH_HUB_PORT {              // Global driver object
#if (USBH_DEBUG > 1)
  U32                  Magic;
#endif
  USBH_DLIST           ListEntry;          // Entry for hub or root hub
  ROOT_HUB           * RootHub;            // Null if no root hub port
  USB_HUB            * ExtHub;             // Null if no external hub
  U32                  PortStatus;         // A copy of the port status returned from the HUB
  U32                  PortStatusShadow;   // Shadow register
  USBH_SPEED           PortSpeed;          // The current speed of the device
  PORT_STATE           PortState;          // The current port state of the port
  USB_DEVICE         * Device;             // Device connected to this port, for tree operation
  U8                   HubPortNumber;      // The one based index of the hub port
  unsigned int         RetryCounter;       // Counts the number of retries
  U8                   ConfigurationIndex; // This is the configuration index for the device
  USBH_BOOL            HighPowerFlag;      // True if the port is an high powered port min.500ma
};

typedef enum { // Device reset
  USBH_HUB_PORTRESET_IDLE,
  USBH_HUB_PORTRESET_REMOVED,               // Port or Hub is not connected
  USBH_HUB_PORTRESET_START,
  USBH_HUB_PORTRESET_RESTART,
  USBH_HUB_PORTRESET_WAIT_RESTART,
  USBH_HUB_PORTRESET_RES,
  USBH_HUB_PORTRESET_IS_ENABLED,
  USBH_HUB_PORTRESET_WAIT_RESET,
  USBH_HUB_PORTRESET_SET_ADDRESS,
  USBH_HUB_PORTRESET_WAIT_SET_ADDRESS,
  USBH_HUB_PORTRESET_START_DEVICE_ENUM,
  USBH_HUB_PORTRESET_DISABLE_PORT
} USBH_HUB_PORTRESET_STATE;

typedef enum { // Hub initialization state machine
  USBH_HUB_ENUM_IDLE,                  // Idle
  USBH_HUB_ENUM_START,                 // Start the state machine
  USBH_HUB_ENUM_GET_STATUS,            // Get the device status
  USBH_HUB_ENUM_HUB_DESC,              // Check the hub descriptor
  USBH_HUB_ENUM_SET_POWER,             // Set power for all ports
  USBH_HUB_ENUM_POWER_GOOD,            // Power good time elapsed
  USBH_HUB_ENUM_PORT_STATE,            // Get all port status, set the port state
  USBH_HUB_ENUM_ADD_DEVICE,            // Add the hub device to the hosts device list
  USBH_HUB_ENUM_REMOVED                // Active if the parent port is removed
} USBH_HUB_ENUM_STATE;

// This states are used in conjunction with the NotifySubState
typedef enum {
  USBH_HUB_NOTIFY_IDLE,               // Idle
  USBH_HUB_NOTIFY_START,              // Start state
  USBH_HUB_NOTIFY_GET_HUB_STATUS,     // Start the state machine
  USBH_HUB_NOTIFY_CLEAR_HUB_STATUS,
  USBH_HUB_NOTIFY_GET_PORT_STATUS,    // Set power for all ports
  USBH_HUB_NOTIFY_CLR_PORT_STATUS,    // Power good time elapsed
  USBH_HUB_NOTIFY_CHECK_OVER_CURRENT,
  USBH_HUB_NOTIFY_CHECK_CONNECT,
  USBH_HUB_NOTIFY_CHECK_REMOVE,
  USBH_HUB_NOTIFY_DISABLE_PORT,       // Disable an port
  USBH_HUB_NOTIFY_REMOVED,            // Hub device state is not WORKING
  USBH_HUB_NOTIFY_ERROR               // Error submitting of an URB to the hub device after max. retries
} USBH_HUB_NOTIFY_STATE;

struct USB_HUB { // USB HUB object
#if USBH_DEBUG > 1
  U32                   Magic;
#endif
  USB_DEVICE          * pHubDevice;                   // Backward pointer to the USB hub device
  unsigned int          PowerGoodTime;               // Power on to power good time in ms
  unsigned int          Characteristics;             // Power on to power good time in ms
  unsigned int          PortCount;                   // Number of ports
  USBH_DLIST            PortList;                    // List of ports
  USBH_BOOL             SubmitFlag;                  // Helper var. in ProcessHubNotification

  // Hub notification
  // This urb contains hub notification information and is used for all hub requests in ProcessHubNotification()
  USBH_URB              NotifyUrb;
  USBH_HUB_NOTIFY_STATE NotifyState;
  USBH_HUB_NOTIFY_STATE OldNotifyState;
  URB_SUB_STATE         NotifySubState;              // hub notify sub state machine (submitting and aborting of URBs)

  // Current not processed ports in ProcessHubNotification()
  unsigned int          NotifyPortCt;
  USBH_HUB_PORT       * NotifyPort;
  U32                   NotifyTemp;                  // Temporary variable
  U32                   Notification;                // Received Notification max. 4 bytes
  U32                   Status;                      // todo: read hub status after hub status
  USBH_HUB_PORT       * EnumPort;                    // Hub device enumeration
  USB_DEVICE          * EnumDevice;
  USBH_URB              EnumUrb;
  USBH_HUB_ENUM_STATE   EnumState;                   // State of the Hubs initialization process
  URB_SUB_STATE         EnumSubState;                // helper sub state for hub enumeration

  // Hold post function and pContext! Used in the state USBH_HUB_ENUM_ADD_DEVICE in ProcessEnumHub()
  POST_ENUM_FUNC  * PostEnumFunction;
  void                * PostEnumContext;
  USBH_HUB_PORTRESET_STATE   PortResetEnumState;          // Hub port reset state machine / Current HubProcessPortResetSetAddress() state

  // Helper sub state machines
  URB_SUB_STATE         PortResetSubState;
  URB_SUB_STATE         PortResetControlUrbSubState;
  USBH_HC_EP_HANDLE     PortResetEp0Handle;

  // To get hub and port notifications
  USB_ENDPOINT        * InterruptEp;
  USBH_URB              interruptUrb;
  void                * InterruptTransferBuffer;
  unsigned int          InterruptTransferBufferSize;
};

void      USBH_BD_DeleteHub(USB_HUB * hub);
void      USBH_BD_StartHub (USB_HUB * Hub, POST_ENUM_FUNC * PostEnumFunction, void * Context);
USB_HUB * USBH_BD_AllocInitUsbHub (USB_DEVICE  * dev);

/* Called if the root hub does not need any service */
void USBH_BD_ServiceAllHubs         (USBH_HOST_CONTROLLER * hc);
void USBH_BD_HubPrepareClrFeatureReq(USBH_URB * urb, U16 feature, U16 selector);


USBH_HUB_PORT * USBH_BD_NewHubPort   (void); // Return null on error
void       USBH_BD_DeleteHubPort(USBH_HUB_PORT * HubPort);
void       USBH_BD_SetPortState (USBH_HUB_PORT * hubPort, PORT_STATE state);

/*********************************************************************
*
*       USBH_BD_HubBuildChildDeviceList
*
*  Function Description:
*    Builds a device list of all devices that are connected to a parent
*    device inclusive the parent device. The first device in the list is
*    the parent device. The list ends if no hub device on an port is found!
*
*  Parameters:
*    pHubDevice: Parent device
*    pDevList:   Pointer to a temporary list
*
*  Return value:
*    Number of devices in the list inclusive the rootHubDevice!
*    0: rootHubDevice is no hub device!
*/
int USBH_BD_HubBuildChildDeviceList(USB_DEVICE * pHubDevice, USBH_DLIST * pDevList);

// Returns the hub port by port number.
// Attention: The state of the hub device is not checked!
USBH_HUB_PORT * USBH_BD_HubGetPortByNumber(USB_HUB * Hub, unsigned char Number);

typedef enum {
  HC_UNKNOWN,
  HC_REMOVED,
  HC_WORKING,
  HC_SUSPEND
} HOST_CONTROLLER_STATE;

struct USBH_HOST_CONTROLLER {                   // Global driver object
  USBH_DLIST              ListEntry;            // List entry for USB driver
  long                    RefCount;             // Ref pCount
  HOST_CONTROLLER_STATE   State;                // The state of the HC
  USBH_HOST_DRIVER_INST * pInst;                // Backward pointer
  USBH_DLIST              DeviceList;           // List of USB devices
  U32                     DeviceCount;
  USBH_HOST_DRIVER      * pDriver;              // Host controller entry
  USBH_HC_HANDLE          hHostController;      // Handle for the HC driver. It is passed to each function
  U8                      UsbAddressArray[128];
  ROOT_HUB                RootHub;              // Embedded root hub
  USBH_HC_EP_HANDLE       LowSpeedEndpoint;
  USBH_HC_EP_HANDLE       FullSpeedEndpoint;
  USBH_HC_EP_HANDLE       HighSpeedEndpoint;
#if (USBH_DEBUG > 1)
  U32                     Magic;
#endif
  // PortResetActive points to a port where the port reset state machine is started or is active. At the end of the
  // set address state of a port reset or if the device where the port is located is removed this pointer is set to NULL!
  USBH_HUB_PORT              * pActivePortReset;
};

#define HC_INC_REF(devPtr) (devPtr)->RefCount++                                                    // Reference counting macros to the USB_DEVICE object

#if (USBH_DEBUG > 1)
#define HC_DEC_REF(pDevice)                                                                         \
    (pDevice)->RefCount--;                                                                          \
    if ((pDevice)->RefCount == 1) {                                                                 \
      USBH_LOG((USBH_MTYPE_CORE, "HC_DEC_REF RefCount is 1 %s(%d)\n", __FILE__, __LINE__));        \
    }                                                                                              \
    if ((pDevice)->RefCount <  0) {                                                                 \
      USBH_LOG((USBH_MTYPE_CORE, "HC_DEC_REF RefCount less than 0 %s(%d)\n", __FILE__, __LINE__)); \
    }                                                                                              \
    if ((pDevice)->RefCount == 0) {                                                                 \
      USBH_LOG((USBH_MTYPE_CORE, "HC_DEC_REF RefCount is 0 %s(%d)\n", __FILE__, __LINE__));        \
      USBH_DeleteHostController(pDevice);                                                             \
    }
#else
  #define HC_DEC_REF(pDevice)           \
    (pDevice)->RefCount--;              \
    if ((pDevice)->RefCount == 0) {     \
      USBH_DeleteHostController(pDevice); \
    }
#endif

#define USBH_OHC_Add   USBH_OHCI_Add

USBH_HOST_CONTROLLER * USBH_CreateHostController      (USBH_HOST_DRIVER * pDriver, USBH_HC_HANDLE hController);
void                   USBH_BD_FreeUsbAddress         (USBH_HOST_CONTROLLER * HostController, U8 Address);
void                   USBH_HC_ServicePorts           (USBH_HOST_CONTROLLER * HostController);
U8                     USBH_BD_GetUsbAddress          (USBH_HOST_CONTROLLER * HostController);
void                   USBH_DeleteHostController      (USBH_HOST_CONTROLLER * Host);
void                   USBH_AddUsbDevice              (USB_DEVICE      * Device);
void                   USBH_HC_RemoveDeviceFromList   (USB_DEVICE      * Device);
void                   USBH_DefaultReleaseEpCompletion(void            * Context);
void                   USBH_Task                      (void);
void                   USBH_ISRTask                   (void);
void                   USBH_EnumTask                  (void);
void                   USBH_ConfigTransferBufferSize  (U32 Size);
void                   USBH_ConfigRootHub             (U8 SupportOvercurrent, U8 PortsAlwaysPowered, U8 PerPortPowered);
void                   USBH_ConfigMaxUSBDevices       (U8 NumDevices);
void                   USBH_ConfigMaxNumEndpoints     (U8 MaxNumBulkEndpoints, U8 MaxNumIntEndpoints, U8 MaxNumIsoEndpoints);
void                   USBH_ConfigSupportExternalHubs (U8 OnOff);
void                   USBH_AddController             (void);
void                   USBH_OHCI_Add                  (void * pBase);
void                   USBH_ProcessISR                (unsigned Index);
void                   USBH_ServiceISR                (unsigned Index);
USBH_URB *             USBH_AllocIsoUrb               (unsigned NumIsoPackets, unsigned NumBytesForBuffer);
void                   USBH_FreeIsoUrb                (USBH_URB * pUrb);

typedef enum {
  USBH_DEVICE_EVENT_ADD,
  USBH_DEVICE_EVENT_REMOVE
} USBH_DEVICE_EVENT;

typedef void USBH_NOTIFICATION_FUNC(void * pContext, U8 DevIndex, USBH_DEVICE_EVENT Event);


/*********************************************************************
*
*       USBH_HID
*/
typedef struct {
  unsigned Code;
  int      Value;
} USBH_HID_KEYBOARD_DATA;

typedef struct {
  int xChange;
  int yChange;
  int WheelChange;
  int ButtonState;
} USBH_HID_MOUSE_DATA;

typedef enum {
  USBH_HID_INPUT_REPORT = 0,
  USBH_HID_OUTPUT_REPORT,
  USBH_HID_FEATURE_REPORT
} USBH_HID_REPORT_TYPE;

typedef int USBH_HID_HANDLE;
typedef struct {
  USBH_HID_REPORT_TYPE ReportType;
  U32 ReportId;
  U32 ReportSize;
} USBH_HID_REPORT_INFO;

typedef struct {
  U16  InputReportSize;
  U16  OutputReportSize;
  U16  ProductId;
  U16  VendorId;
  char acName[7];
} USBH_HID_DEVICE_INFO;

typedef void USBH_HID_USER_FUNC(void * pContext);


typedef void (USBH_HID_ON_KEYBOARD_FUNC) (USBH_HID_KEYBOARD_DATA * pKeyData);
typedef void (USBH_HID_ON_MOUSE_FUNC)    (USBH_HID_MOUSE_DATA    * pMouseData);

void      USBH_HID_Exit(void);
USBH_BOOL USBH_HID_Init(void);
void      USBH_HID_SetOnMouseStateChange   (USBH_HID_ON_MOUSE_FUNC    * pfOnChange);
void      USBH_HID_SetOnKeyboardStateChange(USBH_HID_ON_KEYBOARD_FUNC * pfOnChange);

USBH_STATUS     USBH_HID_Close                    (USBH_HID_HANDLE hDevice);
int             USBH_HID_GetNumDevices            (USBH_HID_DEVICE_INFO * pDevInfo, U32 NumItems);
USBH_STATUS     USBH_HID_GetReportDescriptorParsed(USBH_HID_HANDLE hDevice, USBH_HID_REPORT_INFO * pReportInfo, unsigned * pNumEntries);
USBH_STATUS     USBH_HID_GetDeviceInfo            (USBH_HID_HANDLE hDevice, USBH_HID_DEVICE_INFO * pDevInfo);
USBH_STATUS     USBH_HID_GetReportDescriptor      (USBH_HID_HANDLE hDevice, U8 * pReportDescriptor, unsigned NumBytes);
USBH_STATUS     USBH_HID_GetReport                (USBH_HID_HANDLE hDevice, U8 * pReport, U32 NumBytes, USBH_HID_USER_FUNC * pfFunc, void * pContextData);
USBH_HID_HANDLE USBH_HID_Open                     (const char * sName);
USBH_STATUS     USBH_HID_SetReport                (USBH_HID_HANDLE hDevice, const U8 * pReport, U32 NumBytes, USBH_HID_USER_FUNC * pfFunc, void * pContextData);
USBH_STATUS     USBH_HID_CancelIo                 (USBH_HID_HANDLE hDevice);
void            USBH_HID_RegisterNotification     (USBH_NOTIFICATION_FUNC * pfNotification, void * pContext);
void            USBH_HID_ConfigureControlWriteRetries(U8 NumRetries);
void            USBH_HID_ConfigureControlWriteTimeout(U32 Timeout);

/*********************************************************************
*
*       USBH_LCD, used to control an EPSON USB 2 LCD controller
*/

typedef int USBH_LCD_HANDLE;

USBH_BOOL       USBH_LCD_Init(void);
void            USBH_LCD_Exit(void);
USBH_LCD_HANDLE USBH_LCD_Open(const char * sName);
USBH_STATUS     USBH_LCD_WriteCmd(USBH_LCD_HANDLE hDevice, U8 * pCmdBuffer, unsigned NumBytesCmd, U8 * pStatusBuffer, unsigned NumBytesStatus);
USBH_STATUS     USBH_LCD_WriteDisplayData(USBH_LCD_HANDLE hDevice, U8 * pData, unsigned NumBytes);
USBH_STATUS     USBH_LCD_Close(USBH_LCD_HANDLE hDevice);
int             USBH_LCD_GetNumDevices(void);

/*********************************************************************
*
*       USBH_LCD, used to control an EPSON USB 2 LCD controller
*/

typedef int USBH_PRINTER_HANDLE;
/*********************************************************************
*
*       USBH_PRINTER
*/
// Function parameter for the user callback function USBH_DEVICE_LUN_NOTIFICATION_FUNC

USBH_BOOL           USBH_PRINTER_Init(void);
void                USBH_PRINTER_Exit(void);
USBH_PRINTER_HANDLE USBH_PRINTER_Open(const char * sName);
USBH_STATUS         USBH_PRINTER_Write(USBH_PRINTER_HANDLE hDevice, const U8 * pData, unsigned NumBytes);
USBH_STATUS         USBH_PRINTER_Read(USBH_PRINTER_HANDLE hDevice, U8 * pData, unsigned NumBytes);
USBH_STATUS         USBH_PRINTER_GetPortStatus(USBH_PRINTER_HANDLE hDevice, U8 * pStatus);
USBH_STATUS         USBH_PRINTER_ExecSoftReset(USBH_PRINTER_HANDLE hDevice);
USBH_STATUS         USBH_PRINTER_GetDeviceId(USBH_PRINTER_HANDLE hDevice, U8 * pData, unsigned NumBytes);
USBH_STATUS         USBH_PRINTER_Close(USBH_PRINTER_HANDLE hDevice);
int                 USBH_PRINTER_GetNumDevices(void);
void                USBH_PRINTER_RegisterNotification(USBH_NOTIFICATION_FUNC * pfNotification, void * pContext);
void                USBH_PRINTER_ConfigureTimeout(U32 Timeout);

#if defined(__cplusplus)
  }
#endif

#endif

/********************************* EOF ******************************/
