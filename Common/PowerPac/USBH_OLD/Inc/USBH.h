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
#include "Global.h"
#include "USBH_ConfDefaults.h"

#if defined(__cplusplus)
  extern "C" {                 // Make sure we have C-declarations in C++ programs
#endif

#define USBH_VERSION   10000 // Format: Mmmrr. Example: 10201 is 1.02a

/*********************************************************************
*
*       USBH_MTYPE
*
*  IDs to distinguish different message types
*
**********************************************************************
*/
#define USBH_MTYPE_INIT         (1UL <<  0)
#define USBH_MTYPE_CORE         (1UL <<  1)
#define USBH_MTYPE_TIMER        (1UL <<  2)
#define USBH_MTYPE_DRIVER       (1UL <<  3)
#define USBH_MTYPE_MEM          (1UL <<  4)
#define USBH_MTYPE_URB          (1UL <<  5)
#define USBH_MTYPE_OHCI         (1UL <<  6)
#define USBH_MTYPE_UBD          (1UL <<  7)
#define USBH_MTYPE_PNP          (1UL <<  8)
#define USBH_MTYPE_DEVICE       (1UL <<  9)
#define USBH_MTYPE_EP           (1UL << 10)
#define USBH_MTYPE_HUB          (1UL << 11)
#define USBH_MTYPE_MSD          (1UL << 12)
#define USBH_MTYPE_MSD_INTERN   (1UL << 13)
#define USBH_MTYPE_MSD_PHYS     (1UL << 14)
#define USBH_MTYPE_HID          (1UL << 15)
#define USBH_MTYPE_APPLICATION  (1UL << 19)

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

// Defines for Standard Configruation Descriptor
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

typedef struct T_USB_DEVICE_DESCRIPTOR { // Device descriptor
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

typedef struct T_USB_CONFIGURATION_DESCRIPTOR { // Configuration descriptor
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

typedef struct T_USB_INTERFACE_DESCRIPTOR { // Interface descriptor
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

typedef struct T_USB_ENDPOINT_DESCRIPTOR { // Endpoint descriptor
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

typedef struct T_USB_STRING_DESCRIPTOR { // String descriptor
  U8  bLength;
  U8  bDescriptorType;
  U16 bString[1];                        // Variable size
} USB_STRING_DESCRIPTOR;

#define USB_STRING_HEADER_LENGTH                      2
#define USB_LANGUAGE_DESC_LENGTH                      (4)
#define USB_LANGUAGE_ID                               (0x0409)

typedef struct T_USB_DEVICE_QUALIFIER_DESCRIPTOR { // Device qualifier descriptor
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

typedef struct T_USB_INTERFACE_ASSOCIATION_DESCRIPTOR { // Interface association descriptor
  U8 bLength;
  U8 bDescriptorType;
  U8 bFirstInterface;
  U8 bInterfaceCount;
  U8 bFunctionClass;
  U8 bFunctionSubClass;
  U8 bFunctionProtocol;
  U8 iFunction;
} USB_INTERFACE_ASSOCIATION_DESCRIPTOR;

typedef struct T_USB_COMMON_DESCRIPTOR { // Common descriptor header
U8 bLength;
U8 bDescriptorType;
} USB_COMMON_DESCRIPTOR;

typedef struct T_USB_SETUP_PACKET { // CS Endpoint
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
*       Hub Device Class (HDC) *
*
**********************************************************************
*/

// HDC specific descriptor types
#define CDC_CS_INTERFACE_DESCRIPTOR_TYPE              0x24
#define CDC_CS_ENDPOINT_DESCRIPTOR_TYPE               0x25

// HDC Port Type Recipient. All other are device recipients!
#define HDC_PORT_RECIPIENT USB_OTHER_RECIPIENT

// Hub class descriptor
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

// Hub status request length
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

// Hub class hub feature selectors
// Hub change bits
#define HDC_SELECTOR_C_HUB_LOCAL_POWER                0
#define HDC_SELECTOR_C_HUB_OVER_CURRENT               1

// Hub class port feature selectors
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
#define PORT_C_ALL_MASK                               0x001F0000
#define PORT_C_STATUS_CONNECT                         0x00010000
#define PORT_C_STATUS_ENABLE                          0x00020000
#define PORT_C_STATUS_SUSPEND                         0x00040000
#define PORT_C_STATUS_OVER_CURRENT                    0x00080000
#define PORT_C_STATUS_RESET                           0x00100000

// Hub status bits
#define HUB_STATUS_LOCAL_POWER                        0x00000001
#define HUB_STATUS_OVER_CURRENT                       0x00000002
#define HUB_STATUS_C_LOCAL_POWER                      0x00010000
#define HUB_STATUS_C_OVER_CURRENT                     0x00020000

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

// Hub initialization state machine
typedef enum tag_SUBSTATE_STATE {
  SUBSTATE_IDLE,                                     // Idle, if an URB is completed or if no URb is ubmitted and an timeout occurres!
  SUBSTATE_TIMER,                                    // UrbSubStateWait on success
  SUBSTATE_TIMERURB,                                 // UrbSubStateSubmitRequest on success
  SUBSTATE_TIMEOUT_PENDING_URB                       // On timeout and pending URB
} SUBSTATE_STATE;

typedef void SubStateCallbackRoutine(void *context); // Is only called on an timeout.

// @doc
// @module DLIST <en-> Double Linked List |
// The module DLIST provides an implementation of a double linked list.
// The DLIST structure and the related macros may be used for general purpose.

/*********************************************************************
*
*       Public part (interface)
*
**********************************************************************
*/

// Some Macros used for calculation of structure pointers.

// Calculate the byte offset of a field in a structure of type type.
// lint -emacro(413,STRUCT_FIELD_OFFSET)
// lint -emacro(613,STRUCT_FIELD_OFFSET)

// @func long | STRUCT_FIELD_OFFSET |
//   This macro calculates the offset of <p field> relative to the base of the structure <p type>.
// @parm IN<spc>| type |
//   Type name of the structure
// @parm IN<spc>| field |
//   Field name
// @rdesc
//   Offset of the field <p field> relative to the base of the structure <p type>.
#define STRUCT_FIELD_OFFSET(type, field)((long)&(((type *)0)->field) )

// Calculate the pointer to the base of the structure given its type and a pointer to a field within the structure.
// lint -emacro(413,STRUCT_BASE_POINTER)
// lint -emacro(613,STRUCT_BASE_POINTER)

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
#define STRUCT_BASE_POINTER(fieldptr, type, field)((type *)(((char *)(fieldptr)) - ((char *)(&(((type *)0)->field)))))

// Double linked list structure. Can be used as either a list head, or as link words.
struct         tDLIST;
typedef struct tDLIST * PDLIST;

// Double linked list manipulation routines. Implemented as macros but logically these are procedures.
// void DlistInit(PDLIST ListHead);

// @func void | DlistInit |
//   <f DlistInit> initializes a DLIST element. The link pointers
//   points to the structure itself. This element represents
//   an empty DLIST.
// @parm IN PDLIST<spc>| ListHead |
//   Pointer to a structure of type DLIST.
// @comm Each list head has to be initialized by this function.
#define DlistInit(ListHead) {                         \
  (ListHead)->Flink = (ListHead)->Blink = (ListHead); \
}

// int DlistEmpty(PDLIST ListHead);

// @func int | DlistEmpty |
//   <f DlistEmpty> checks whether the list is empty.
// @parm IN PDLIST<spc>| ListHead |
//   Pointer to the list head.
// @rdesc
//   Returns 1 (TRUE) if the list is empty, 0 (FALSE) otherwise.
#define DlistEmpty(ListHead)((ListHead)->Flink == (ListHead))

// PDLIST DlistGetNext(PDLIST Entry);

// @func PDLIST<spc>| DlistGetNext |
//   <f DlistGetNext> returns a pointer to the successor.
// @parm IN PDLIST<spc>| Entry |
//   Pointer to a list entry.
// @rdesc
//   Pointer to the successor of <p Entry>.
#define DlistGetNext(Entry)((Entry)->Flink)

// PDLIST DlistGetPrev(PDLIST Entry);

// @func PDLIST<spc>| DlistGetPrev |
//   <f DlistGetPrev> returns a pointer to the predecessor.
// @parm IN PDLIST<spc>| Entry |
//   Pointer to a list entry.
// @rdesc
//   Pointer to the predecessor of <p Entry>.
#define DlistGetPrev(Entry)((Entry)->Blink)

// Remove a single entry
// void DlistRemoveEntry(PDLIST Entry);

// @func void | DlistRemoveEntry |
//   <f DlistRemoveEntry> detaches one element from the list.
// @parm IN PDLIST<spc>| Entry |
//   Pointer to the element to be detached.
// @comm Calling this function on an empty list results in
//   undefined behaviour.
#define DlistRemoveEntry(Entry) {                                            \
  PDLIST dlist_m_Entry               = (Entry);                              \
         dlist_m_Entry->Blink->Flink = dlist_m_Entry->Flink;                 \
         dlist_m_Entry->Flink->Blink = dlist_m_Entry->Blink;                 \
         dlist_m_Entry->Flink        = dlist_m_Entry->Blink = dlist_m_Entry; \
}


// void DlistRemoveHead(PDLIST ListHead, PDLIST *Entry);

// @func void | DlistRemoveHead |
//   <f DlistRemoveHead> detaches the first element from the list.
// @parm IN PDLIST<spc>| ListHead |
//   Pointer to the list head.
// @parm OUT PDLIST<spc>*| Entry |
//   Address of a pointer to the detached element.
// @comm Calling this function on an empty list results in
//   undefined behaviour.
#define DlistRemoveHead(ListHead, Entry) DlistRemoveEntry(*(Entry) = (ListHead)->Flink)

//void DlistRemoveTail(PDLIST ListHead, PDLIST *Entry);

// @func void | DlistRemoveTail |
//   <f DlistRemoveTail> detaches the last element from the list.
// @parm IN PDLIST<spc>| ListHead |
//   Pointer to the list head.
// @parm OUT PDLIST<spc>*| Entry |
//   Address of a pointer to the detached element.
// @comm Calling this function on an empty list results in
//   undefined behaviour.
#define DlistRemoveTail(ListHead,Entry) DlistRemoveEntry(*(Entry) = (ListHead)->Blink)

// Insert a single entry
// void DlistInsertEntry(PDLIST Entry, PDLIST NewEntry);
// Inserts NewEntry after Entry

// @func void | DlistInsertEntry |
//   <f DlistInsertEntry> inserts an element into a list.
// @parm IN PDLIST<spc>| Entry |
//   Pointer to the element after which the new entry is to be inserted.
// @parm IN PDLIST<spc>| NewEntry |
//   Pointer to the element to be inserted.
// @comm <p NewEntry> is inserted after <p Entry>, i. e. <p NewEntry>
//   becomes the successor of <p Entry>.
#define DlistInsertEntry(Entry,NewEntry) {                   \
  PDLIST dlist_m_Entry               = (Entry);              \
  PDLIST dlist_m_NewEntry            = (NewEntry);           \
         dlist_m_NewEntry->Flink     = dlist_m_Entry->Flink; \
         dlist_m_NewEntry->Blink     = dlist_m_Entry;        \
         dlist_m_Entry->Flink->Blink = dlist_m_NewEntry;     \
         dlist_m_Entry->Flink        = dlist_m_NewEntry;     \
}

// void DlistInsertHead(PDLIST ListHead, PDLIST Entry);

// @func void | DlistInsertHead |
//   <f DlistInsertHead> inserts an element at the beginning of a list.
// @parm IN PDLIST<spc>| ListHead |
//   Pointer to the list head.
// @parm IN PDLIST<spc>| Entry |
//   Pointer to the element to be inserted.
// @comm <p Entry> becomes the first list entry.
#define DlistInsertHead(ListHead,Entry) DlistInsertEntry(ListHead,Entry)

// void DlistInsertTail(PDLIST ListHead, PDLIST Entry);

// @func void | DlistInsertTail |
//   <f DlistInsertTail> inserts an element at the end of a list.
// @parm IN PDLIST<spc>| ListHead |
//   Pointer to the list head.
// @parm IN PDLIST<spc>| Entry |
//   Pointer to the element to be inserted.
// @comm <p Entry> becomes the last list entry.
#define DlistInsertTail(ListHead,Entry) DlistInsertEntry((ListHead)->Blink,Entry)

// Append another list
// void DlistAppend(PDLIST ListHead, PDLIST List);
// Appends List to tail of ListHead

// @func void | DlistAppend |
//   <f DlistAppend> concatenates two lists.
// @parm IN PDLIST<spc>| ListHead |
//   Pointer to the list head of the first list.
// @parm IN PDLIST<spc>| List |
//   Pointer to the list head of the second list.
// @comm The first element of <p List> becomes the successor
//   of the last element of <p ListHead>.
#define DlistAppend(ListHead,List) {                     \
PDLIST dlist_m_List               = (List);              \
PDLIST dlist_m_Tail               = (ListHead)->Blink;   \
       dlist_m_Tail->Flink        = dlist_m_List;        \
       dlist_m_List->Blink->Flink = (ListHead);          \
         (ListHead)->Blink        = dlist_m_List->Blink; \
       dlist_m_List->Blink        = dlist_m_Tail;        \
}

/*********************************************************************
*
*       Private part (implementation)
*
**********************************************************************
*/

// Double linked list structure. Can be used as either a list head, or as link words.

// @struct DLIST |
//   The DLIST structure is the link element of the double linked list. It is used as either a list head, or as link entry.
// @field  struct tDLIST * | Flink |
//   Pointer to the successor (forward link).
// @field  struct tDLIST * | Blink |
//   Pointer to the predecessor (backward link).
// @comm By means of such elements any structures may be handled as a double linked list. The DLIST structure is to be inserted
//   into the structure which is to be handled. A pointer to the original structure can be obtained by means of the macro <f STRUCT_BASE_POINTER>.
typedef struct tDLIST {
  struct tDLIST * Flink;
  struct tDLIST * Blink;
} DLIST;

#define HCM_POOL_MAGIC                        FOUR_CHAR_ULONG('P','O','O','L')
#define HCM_ITEM_HEADER_MAGIC                 FOUR_CHAR_ULONG('I','T','E','M')
#define HCM_POOL_VALID(poolPtr)               T_ASSERT(PTRVALID((poolPtr),       HCM_POOL))
#define HCM_ASSERT_ITEM_HEADER(itemHeaderPtr) T_ASSERT(PTRVALID((itemHeaderPtr), HCM_ITEM_HEADER))

// Needs the struct and the name of the list entry inside the struct
#define GET_HCMITEM_FROM_ENTRY(pListEntry) STRUCT_BASE_POINTER((pListEntry), HCM_ITEM_HEADER, Link.ListEntry)

#define HCM_ASSERT_ITEMHEADER_ADDRESS(Pool,Itemheader)                          \
        T_ASSERT(    (void*) (Itemheader) >= (void*)(Pool)->ItemHeaderStartAddr \
                  && (void*) (Itemheader) <= (void*)(Pool)->ItemHeaderEndAddr)

#define HCM_PHY_ADDR_IN_POOL(Pool, PhyAddr)                                     \
        (    (PhyAddr) >= (Pool) ->contiguousMemoryPhyAddr                      \
          && (PhyAddr) <= (Pool) ->EndcontiguousMemoryPhyAddr)

#define HCM_ASSERT_PHY_MEMORY(Pool,PhyAddr) T_ASSERT(HCM_PHY_ADDR_IN_POOL((Pool), (PhyAddr))

//
// Generic pool handling
//
#define HcmPoolEmpty(Pool) ((Pool)->Head == NULL) // Returns true if pool is empty

#if (USBH_DEBUG > 1)                              // Get an item from pool / MUST ONLY be called if pool is NOT empty!
#define HcmPoolGet(Pool, Item) {    \
  (Item)       = (Pool)->Head;      \
  (Pool)->RefCount++;               \
  (Pool)->Head = (Item)->Link.Next; \
}
#else
#define HcmPoolGet(Pool, Item) {    \
  (Item)       = (Pool)->Head;      \
  (Pool)->Head = (Item)->Link.Next; \
}
#endif

#if (USBH_DEBUG > 1)                              // Put an item into pool / implements a LIFO scheme
#define HcmPoolPut(Pool, Item)      \
{                                   \
  (Item)->Link.Next = (Pool)->Head; \
  T_ASSERT((Pool)->RefCount != 0);  \
  (Pool)->RefCount--;               \
  (Pool)->Head      = (Item);       \
}

#else

#define HcmPoolPut(Pool, Item) {    \
  (Item)->Link.Next = (Pool)->Head; \
  (Pool)->Head      = (Item);       \
}

#endif

// With HCM_ITEM_HEADER begins every pool item allocated in virtual memory. The user can extend this pool header by the
// parameter SizeOfExtension in the function  Hcm_AllocPool.
typedef struct T_HCM_ITEM_HEADER {
// link element, used to create chains
#if (USBH_DEBUG > 1)
  U32                 Magic;
#endif
  union {
    struct T_HCM_ITEM_HEADER * Next;
    DLIST                      ListEntry;
  } Link;
  volatile U32        PhyAddr;            // Physical and virtual start address of associated contiguous memory
  void              * VirtAddr;
  struct T_HCM_POOL * OwningPool;         // Pointer to pool the descriptor was allocated from
} HCM_ITEM_HEADER;

typedef struct T_HCM_POOL {
#if (USBH_DEBUG > 1)
  U32 Magic;
#endif
  void            * contiguousMemoryVirtAddr; // Start address of contiguous memory used to release the memory
  U32               contiguousMemoryPhyAddr;
  void            * ItemHeaderStartAddr;      // Start address of not contiguous memory, used to release the memory
  HCM_ITEM_HEADER * Head;                     // Pointer to first buffer, Head==NULL if pool is empty

  // Additional fields used for calculations
  U32               NumberOfItems;            // Number of items should be grater than zero
  U32               SizeOfItem;               // Size in bytes of item in contiguous non paged physical memory
  U32               SizeOfExtension;          // Size in bytes of an item in not contiguous memory

  // Only for debug
  void            * ItemHeaderEndAddr;
  void            * EndcontiguousMemoryVirtAddr;
  U32               EndcontiguousMemoryPhyAddr;
  U32               RefCount;                 // GetItem and PutItem validation ref.counter*/
} HCM_POOL;

// Get HCM_ITEM_HEADER pointer from pointer to embedded DLIST ListEntry
#define GET_HCM_ITEM_HEADER_FROM_ENTRY(ple) STRUCT_BASE_POINTER((ple), HCM_ITEM_HEADER, Link.ListEntry)

// Get pointer to extra space allocated after HCM_ITEM_HEADER
#define HCM_ITEM_HEADER_EXTRA_SPACE(ItemHeaderPtr) ((void*)((ItemHeaderPtr) + 1))

// Get HCM_ITEM_HEADER pointer from pointer to extra space
// lint -emacro((740),HCM_ITEM_HEADER_FROM_EXTRA_SPACE)
#define HCM_ITEM_HEADER_FROM_EXTRA_SPACE(pes) (((HCM_ITEM_HEADER *)(void *)(pes)) - 1)

USBH_STATUS       HcmAllocPool         (HCM_POOL        * Pool,    U32 NumberOfItems, U32 SizeOfPhysItem, U32 SizeOfExtension, U32 Alignment);
void              HcmFreePool          (HCM_POOL        * MemPool);
HCM_ITEM_HEADER * HcmGetItem           (HCM_POOL        * MemPool);
HCM_ITEM_HEADER * HcmGetItemFromPhyAddr(HCM_POOL        * MemPool, U32 PhyAddr);
void              HcmPutItem           (HCM_ITEM_HEADER * Item);
void              HcmFillPhyMemory     (HCM_ITEM_HEADER * Item,    U8 Val);
U32               HcmIsPhysAddrInPool  (HCM_POOL        * MemPool, U32 PhyAddr);

// The following defines must not be changed!
#define OH_DEV_MAX_TRANSFER_DESC      (9)                           // default value: 7 (reserved for enumeration)
#define OH_DEV_ENUMERATION_CONTROL_EP 7                             // +1=dummy endpoint
#define OH_DEV_MAX_BULK_EP            (HC_DEVICE_BULK_ENDPOINTS +1) // +1=dummy endpoint
#define OH_DEV_MAX_CONTROL_EP         (HC_DEVICE_MAX_USB_DEVICES + OH_DEV_ENUMERATION_CONTROL_EP + 1)
#define OH_DEV_MAX_INT_EP              HC_DEVICE_INTERRUPT_ENDPOINTS
#define OH_DEV_MAX_ISO_TD             (HC_DEVICE_ISO_ENDPOINTS * OH_DEV_MAX_TRANSFER_DESC)                         // Maximum number of ISO Ep host controller TD's
#define OH_DEV_MAX_BULK_TD            (OH_DEV_MAX_BULK_EP * OH_DEV_MAX_TRANSFER_DESC)                              // Maximum number of BULK Ep host controller TD's
#define OH_DEV_MAX_INT_TD             (HC_DEVICE_INTERRUPT_ENDPOINTS * OH_DEV_MAX_TRANSFER_DESC)                   // Maximum number of INTERRUPT Ep host controller TD's*, dummy endpoint have not transfer descriptors !!!
#define OH_DEV_MAX_CONTROL_TD         (OH_DEV_MAX_CONTROL_EP+  (OH_DEV_MAX_CONTROL_EP * OH_DEV_MAX_TRANSFER_DESC)) // Maximum number of CONTROL Ep host controller TD's
#define OH_TOTAL_GTD                  (OH_DEV_MAX_BULK_TD+OH_DEV_MAX_CONTROL_TD)                                   // Number transfer and endpoint descriptors.
#define OH_TOTAL_ED                   (HC_DEVICE_ISO_ENDPOINTS+HC_DEVICE_INTERRUPT_ENDPOINTS+OH_DEV_MAX_BULK_EP +OH_DEV_MAX_CONTROL_EP+OHD_DUMMY_INT_NUMBER)
#define OH_MAX_ED_SIZE                (OH_ED_SIZE+OH_ED_ALIGNMENT)
#define OH_MAX_TD_SIZE                (OH_GTD_SIZE+OH_GTD_ALIGNMENT)

// HCCA
#define OH_HCCA_LENGTH         256
#define OH_HCCA_ALIGNMENT      256
#define OH_HCCA_MAX_SIZE       (OH_HCCA_LENGTH+OH_HCCA_ALIGNMENT)

// This defines includes also the setup buffer for all control endpoint and the additional transfer buffer in the host driver.
// It does not include any from user allocated physical buffer
#define OH_OHCI_MEMORY_SIZE           ((OH_MAX_ED_SIZE*OH_TOTAL_ED) + (OH_TOTAL_GTD*OH_MAX_TD_SIZE) + OH_HCCA_MAX_SIZE + (OH_DEV_MAX_CONTROL_TD*8))

// OHD_BULK_INT_EP AbortMask values
#define EP_ABORT_MASK                 0x0001 // 1. Endpoint is skipped and an timer with an timeout of about two frames is started
#define EP_ABORT_SKIP_TIME_OVER_MASK  0x0002 // 2. Endpoint skip timeout is over
#define EP_ABORT_PROCESS_FLAG_MASK    0x0004 // 3. temporary flag to process the aborted endpoint
#define EP_ABORT_START_TIMER_MASK     0x0008 // additional flag to restart the abort timer in the timer routine if another endpoint

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

#define HC_DEVICE_MAGIC FOUR_CHAR_ULONG('O','D','E','V')
#define OH_DEV_VALID(devPtr) T_ASSERT(PTRVALID(devPtr,HC_DEVICE))
#define OH_DEV_FROM_HANDLE(devPtr,USBH_hc_handle)(devPtr) = ((HC_DEVICE *)(USBH_hc_handle))

typedef enum tag_OhDevState {
  OH_DEV_HALT,
  OH_DEV_SUSPEND, // From OH_DEV_SUSPEND only halt or resume is allowed, if the host is reset then the host not be in suspend!
  OH_DEV_RESUME,
  OH_DEV_RUNNING
} OhDevState;

// Used to transfer data from bulk control and interrupt endpoints if the transfer memory does not support bus master transfer
struct T_OH_TRANSFER_BUFFER {
  HCM_ITEM_HEADER   ItemHeader;
  U8              * UrbBuffer;       // Current buffer pointer
  U32               Transferred;     // Transferred length
  U32               RemainingLength; // IN: size of URB buffer OUT:not transferred bytes
  int               UsbToHostFlag;   // True: USB IN transfer
};

typedef struct T_OH_TRANSFER_BUFFER OH_TRANSFER_BUFFER;

#define USBH_VERSION_MJ  0x00
#define USBH_VERSION_MN  0x10

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
typedef struct tag_USBH_INTERFACE_MASK {
  U16 Mask;
  U16 VID;
  U16 PID;
  U16 bcdDevice;
  U8  Interface;
  U8  Class;
  U8  SubClass;
  U8  Protocol;
} USBH_INTERFACE_MASK;

typedef void * USBH_INTERFACE_LIST_HANDLE; // Handle to the interface list

typedef enum tag_USBH_SPEED {
  USBH_SPEED_UNKNOWN,
  USBH_LOW_SPEED,
  USBH_FULL_SPEED,
  USBH_HIGH_SPEED
} USBH_SPEED;

// This structure contains information about a USB interface and the related device
typedef struct tag_USBH_INTERFACE_INFO {
  USBH_INTERFACE_ID InterfaceID;
  USBH_DEVICE_ID    DeviceID;
  U16               VID;
  U16               PID;
  U16               bcdDevice;
  U8                Interface;
  U8                Class;
  U8                SubClass;
  U8                Protocol;
  unsigned int      OpenCount;
  U8                ExclusiveUsed;
  USBH_SPEED        Speed;
  U8                SerialNumber[256]; // The serial number in UNICODE format, not zero terminated
  U8                SerialNumberSize;  // The size of the serial number, 0 means not available or error during request
} USBH_INTERFACE_INFO;

USBH_INTERFACE_LIST_HANDLE USBH_CreateInterfaceList (USBH_INTERFACE_MASK * InterfaceMask, unsigned int * InterfaceCount);
void                       USBH_DestroyInterfaceList(USBH_INTERFACE_LIST_HANDLE InterfaceListHandle);
USBH_INTERFACE_ID          USBH_GetInterfaceID      (USBH_INTERFACE_LIST_HANDLE InterfaceListHandle, unsigned int Index);
typedef USBH_STATUS        USBH_GET_INTERFACE_INFO  (USBH_INTERFACE_ID          InterfaceID, USBH_INTERFACE_INFO * InterfaceInfo);
USBH_STATUS                USBH_GetInterfaceInfo    (USBH_INTERFACE_ID          InterfaceID, USBH_INTERFACE_INFO * InterfaceInfo);

// Events for the PnP function
typedef enum tag_USBH_PNP_EVENT {
  USBH_AddDevice,
  USBH_RemoveDevice
} USBH_PNP_EVENT;

// This function is called by the USB Bus Driver Core if a PnP event occurs.
// It is typically called the first time in the context of USBH_RegisterPnPNotification.
typedef void USBH_ON_PNP_EVENT_FUNC(void * Context, USBH_PNP_EVENT Event, USBH_INTERFACE_ID InterfaceID);

// struct USBH_PNP_NOTIFICATION is used as parameter to notification functions
typedef struct tag_USBH_PNP_NOTIFICATION {
  USBH_ON_PNP_EVENT_FUNC * PnpNotification; // The notification function
  void                   * Context;         // The notification context, passed to USBH_RegisterPnPNotification
  USBH_INTERFACE_MASK      InterfaceMask;   // Mask to the interface
} USBH_PNP_NOTIFICATION;

typedef USBH_NOTIFICATION_HANDLE   USBH_REGISTER_PNP_NOTIFICATION  (USBH_PNP_NOTIFICATION    * PnPNotification );
USBH_NOTIFICATION_HANDLE           USBH_RegisterPnPNotification    (USBH_PNP_NOTIFICATION    * PnPNotification);
typedef void                       USBH_UNREGISTER_PNP_NOTIFICATION(USBH_NOTIFICATION_HANDLE   Handle);
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
#define USBH_ENUM_ERROR_ROOT_PORT_RESET 0x10 // Eerror during reset of a USB device on an root hub port
#define USBH_ENUM_ERROR_HUB_PORT_RESET  0x20 // Error during reset of a USB device on an external hub port
#define UDB_ENUM_ERROR_INIT_DEVICE      0x30 // Error during initialization of an device until it is in the configured state
#define UDB_ENUM_ERROR_INIT_HUB         0x40 // Error during initialization of an configured external hub device until the installation of an interrupt IN status pipe
#define USBH_ENUM_ERROR_LOCATION_MASK   \
(USBH_ENUM_ERROR_ROOT_PORT_RESET      | \
 USBH_ENUM_ERROR_HUB_PORT_RESET       | \
 UDB_ENUM_ERROR_INIT_DEVICE           | \
 UDB_ENUM_ERROR_INIT_HUB)

// This struct is only for information
typedef struct tag_USBH_ENUM_ERROR {
  int         Flags;
  int         PortNumber;
  USBH_STATUS Status;
  int         ExtendedErrorInformation; // For internal contains an state value
} USBH_ENUM_ERROR;

// This function is called by the USB Bus Driver Core if a error during USB device enumeration occurs.
// To install this notification routine USBH_RegisterEnumErrorNotification() must be called.
typedef void           USBH_ON_ENUM_ERROR_FUNC             (void * Context, const USBH_ENUM_ERROR   * EnumError);
USBH_ENUM_ERROR_HANDLE USBH_RegisterEnumErrorNotification  (void * Context, USBH_ON_ENUM_ERROR_FUNC * EnumErrorCallback);
void                   USBH_UnregisterEnumErrorNotification(USBH_ENUM_ERROR_HANDLE Handle);
void                   USBH_RestartEnumError               (void);

// Used to access an interface
typedef void        * USBH_INTERFACE_HANDLE;
typedef USBH_STATUS   USBH_OPEN_INTERFACE (USBH_INTERFACE_ID InterfaceID, U8 Exclusive, USBH_INTERFACE_HANDLE * InterfaceHandle );
USBH_STATUS           USBH_OpenInterface  (USBH_INTERFACE_ID InterfaceID, U8 Exclusive, USBH_INTERFACE_HANDLE * InterfaceHandle);
typedef void          USBH_CLOSE_INTERFACE(USBH_INTERFACE_HANDLE Handle);
void                  USBH_CloseInterface (USBH_INTERFACE_HANDLE Handle);

/*******************************************************************************
*
*       Information requests
*
********************************************************************************
*/

USBH_STATUS USBH_GetDeviceDescriptor              (USBH_INTERFACE_HANDLE Handle, U8 * Descriptor, unsigned int Size, unsigned int * Count);
USBH_STATUS USBH_GetCurrentConfigurationDescriptor(USBH_INTERFACE_HANDLE Handle, U8 * Descriptor, unsigned int Size, unsigned int * Count);
USBH_STATUS USBH_GetSerialNumber                  (USBH_INTERFACE_HANDLE Handle, U8 * Descriptor, unsigned int Size, unsigned int * Count);
USBH_STATUS USBH_GetInterfaceDescriptor           (USBH_INTERFACE_HANDLE Handle, U8 AlternateSetting, U8 * Descriptor, unsigned int Size, unsigned int * Count);

// Mask bits for device mask
#define USBH_EP_MASK_INDEX     0x0001
#define USBH_EP_MASK_ADDRESS   0x0002
#define USBH_EP_MASK_TYPE      0x0004
#define USBH_EP_MASK_DIRECTION 0x0008

typedef struct tag_USBH_EP_MASK {
  U32 Mask;
  U8  Index;
  U8  Address;
  U8  Type;
  U8  Direction;
} USBH_EP_MASK;

// It returns the endpoint descriptor for a given endpoint or returns with status invalid parameter.
typedef USBH_STATUS USBH_GET_ENDPOINT_DESCRIPTOR(USBH_INTERFACE_HANDLE Handle, U8 AlternateSetting, const USBH_EP_MASK * Mask, U8 * Descriptor, unsigned int Size, unsigned int * Count);
USBH_STATUS         USBH_GetEndpointDescriptor  (USBH_INTERFACE_HANDLE Handle, U8 AlternateSetting, const USBH_EP_MASK * Mask, U8 * Descriptor, unsigned int Size, unsigned int * Count);

// Return the operating speed of the device.
USBH_STATUS         USBH_GetSpeed        (USBH_INTERFACE_HANDLE Handle, USBH_SPEED * Speed);
typedef USBH_STATUS USBH_GET_FRAME_NUMBER(USBH_INTERFACE_HANDLE Handle, U32 * FrameNumber);
USBH_STATUS         USBH_GetFrameNumber  (USBH_INTERFACE_HANDLE Handle, U32 * FrameNumber);

// Returns the interface ID for a given handle
USBH_STATUS USBH_GetInterfaceIDByHandle(USBH_INTERFACE_HANDLE Handle, USBH_INTERFACE_ID * InterfaceID);

/*********************************************************************
*
*       Async URB based requests
*
**********************************************************************
*/

// Function codes
typedef enum tag_USBH_FUNCTION {
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

struct tag_URB;                                             // Forward
typedef void USBH_ON_COMPLETION_FUNC(struct tag_URB * Urb); // The completion function
typedef struct tag_USBH_HEADER {                            // Is common for all URB based requests
  USBH_FUNCTION             Function;                       // Function code defines the operation of the URB
  USBH_STATUS               Status;                         // Is returned by the USB Bus Driver Core
  USBH_ON_COMPLETION_FUNC * Completion;                     // Completion function, must be a valid pointer is not optional
  void                    * Context;                        // This member can be used by the caller to store a context, it is not changed by the USB Bus Driver Core
  DLIST                     ListEntry;                      // List entry to keep the URB in a list, the owner can use this entry
  // For internal use
  void                    * UbdContext;                     // Context used from the UBD driver
  USBH_ON_COMPLETION_FUNC * InternalCompletion;             // Completion function, must be a valid pointer is not optional
  void                    * InternalContext;                // This member can be used by the caller to store a context, it is not changed by the USB Bus Driver Core
  U32                       HcFlags;                        // Context used from the Host controller driver
} USBH_HEADER;

#define GET_URB_HEADER_FROM_ENTRY(Entry) STRUCT_BASE_POINTER(Entry, USBH_HEADER, ListEntry)

// Used with USBH_FUNCTION_CONTROL_REQUEST
typedef struct tag_USBH_CONTROL_REQUEST {
  USBH_SETUP_PACKET   Setup;    // The setup packet, direction of data phase, the length field must be valid!
  U8                  Endpoint; // Use 0 for default endpoint
  void              * Buffer;   // Pointer to the caller provided storage, can be NULL
  U32                 Length;   // IN:- OUT: bytes transferred
} USBH_CONTROL_REQUEST;

// Used with USBH_FUNCTION_BULK_REQUEST and USBH_FUNCTION_INT_REQUEST
typedef struct tag_USBH_BULK_INT_REQUEST {
  U8     Endpoint; // With direction bit
  void * Buffer;
  U32    Length;   // IN: length in bytes of buffer OUT: bytes transferred
} USBH_BULK_INT_REQUEST;

// A helper struct to define ISO requests. Each struct of this type describes the amount of data for one USB frame.
// The data structure ISO_REQUEST ends with an array of such structures.
typedef struct tag_USBH_ISO_FRAME {
  U32         Offset;
  U32         Length;
  USBH_STATUS Status;
} USBH_ISO_FRAME;

// Start the transfer as soon as possible
#define USBH_ISO_ASAP 0x01
#define URB_GET_ISO_URB_SIZE(Frames) (sizeof(USBH_HEADER) + sizeof(ISO_REQUEST) (Frames)* sizeof(ISO_FRAME))

// Used with USBH_FUNCTION_ISO_REQUEST. At the end of this data structure is an variable sized array of ISO_FRAME.
// The macro GET_ISO_URB_SIZE returns the size of this URB for a given number of frames.
typedef struct tag_USBH_ISO_REQUEST {
  U8             Endpoint;   // With direction bit
  void         * Buffer; // Pointer to the caller provided storage, can be NULL
  U32            Length;    // IN: buffer size, OUT: bytes transferred
  unsigned int   Flags;
  unsigned int   StartFrame;
  unsigned int   Frames;
} USBH_ISO_REQUEST;

typedef struct tag_USBH_ENDPOINT_REQUEST {  // Used with USBH_FUNCTION_ABORT_ENDPOINT and USBH_FUNCTION_RESET_ENDPOINT.
  U8 Endpoint;                              // With direction bit
} USBH_ENDPOINT_REQUEST;

typedef struct tag_USBH_SET_CONFIGURATION { // Used with USBH_FUNCTION_SET_CONFIGURATION. Changing the configuration caused PnP events for all interfaces.
  U8 ConfigurationDescriptorIndex;          // Zero based
} USBH_SET_CONFIGURATION;

typedef struct tag_USBH_SET_INTERFACE {     // Used with USBH_FUNCTION_SET_INTERFACE. The interface is given by the handle.
  U8 AlternateSetting;                      // Zero based
} USBH_SET_INTERFACE;

typedef enum tag_USBH_POWER_STATE {
  USBH_NORMAL_POWER,
  USBH_SUSPEND
} USBH_POWER_STATE;

typedef struct tag_USBH_SET_POWER_STATE {
  USBH_POWER_STATE PowerState;
} USBH_SET_POWER_STATE;

typedef struct tag_URB {                    // Common USB request block structure. It is used for all async. Requests
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
} URB;

typedef USBH_STATUS USBH_SUBMIT_URB(USBH_INTERFACE_HANDLE Handle, URB* Urb);

// Interface function for all asynchronous requests. If the function returns USBH_STATUS_PENDING the completion routine is called.
// On each other status code the completion routine is never called. The storage of the URB must be provided by the caller and
// must be permanent until the URB is returned by the completion routine.
USBH_STATUS   USBH_SubmitUrb(USBH_INTERFACE_HANDLE Handle, URB * Urb);
const char  * USBH_GetStatusStr(USBH_STATUS x);

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
} OHD_TD_PID;

// This bits in the OHCI TD and ISO TD DWORD 0 are not modified and are used
#define OHCI_TD_DONE_MASK GET_MASK_FROM_BITNUMBER(OHCI_TD_NOT_USED_BIT_1)
#define OHCI_TD_ISO_MASK  GET_MASK_FROM_BITNUMBER(OHCI_TD_NOT_USED_BIT_2)

typedef struct T_OHD_TDI {             // Isochronous transfer descriptor !
  HCM_ITEM_HEADER   ItemHeader;        // The struct must always begin with an item header that includes the physical address
  OHD_TD_STATUS     Status;            // Current TD status
  T_BOOL            CancelPendingFlag; // True if the URB request has been canceledd and this TD iswaitingng for cleanup! On the control endpoint this flag is in the endpoint !
  U32               Size;              // Total number of bytes that are queued for this transfer
  void            * Ep;                // Pointer to the endpoint to which the transfer is queued
} OHD_TDI;

// The logical general transfer descriptor object ! General Transfer descriptor of the host controller driver, this includes a
// memory pool object that contains the virtual and physical address of the host controller transfer descriptor.
typedef struct T_OHD_GTD {
  HCM_ITEM_HEADER   ItemHeader;        // The struct must always begin with an item header that includes the physical address
  OHD_TD_STATUS     Status;            // Current TD status
  T_BOOL            CancelPendingFlag; // True if the URB request has been canceled and this TD is waiting for cleanup!
  U32               Size;              // Total number of bytes that are queued for this transfer
  U8                EndpointType;      // the type of the endpoint, one of USB_EP_TYPE_CONTROL, ... used to find the endpoint list
  void            * Ep;                // Pointer to the endpoint to which the transfer is queued
} OHD_GTD;

USBH_STATUS   OhTdGetStatusAndLength(OHD_GTD  * Gtd,            U32 * Transferred,  T_BOOL     * shortPkt);
USBH_STATUS   OhTdAlloc             (HCM_POOL * GeneralTd,      U32   GeneralTdNumbers);
void          OhTdInit              (OHD_GTD  * gtd, void * Ep, U8    EndpointType, OHD_TD_PID   Pid, U32 StartAddr, U32 EndAddr, U32 Dword0Mask);
OHD_GTD     * OhTdGet               (HCM_POOL * GeneralTd);
U32           OhTdGetNextTd         (void     * VirtTdAddr);

// This tow operations are only vlaid on the TD if the TD not in the ED list of the host or the ED list is disabled!
#define OH_ED_SET_SKIP_BIT(edPtr) (edPtr->Dword0 |=       OHCI_ED_K)
#define OH_ED_CLR_SKIP_BIT(edPtr) (edPtr->Dword0 &= (U32)~OHCI_ED_K)
#define OH_MAX_PKT_SIZE_EP0_LOWSPEED  8
#define OH_MAX_PKT_SIZE_EP0           64
#define OH_MAX_PKT_SIZE_BULK          64
#define OH_MAX_PKT_SIZE_INT           64
#define OH_MAX_PKT_SIZE_ISO           1023

// Additional endpoint mask bits

// Endpoint flags field
#define OH_DUMMY_ED_EPFLAG    0x01
#define OH_SHORT_PKT_EPFLAG   0x01

// Endpoint states

typedef enum tag_OhEpState {
  OH_EP_IDLE,        // The endpoint is not linked
  OH_EP_UNLINK,      // If the timer routine runs then the endpoint is removed and deleted
  OH_EP_LINK,        // The endpoint is linked
  OH_EP_UNLINK_TIMER // Endpoint is unlinked but the current timer routine must restart the timer
} OhEpState;

void OhEpGlobInitED            (HCM_ITEM_HEADER * Header,   U8                DeviceAddr,  U8    EpWithDirection, U32 MaxPktSize, T_BOOL IsoFlag, T_BOOL SkipFlag, USBH_SPEED Speed);
void OhEpGlobInsertTD          (HCM_ITEM_HEADER * EpHeader, HCM_ITEM_HEADER * NewTdHeader, U32 * TdCounter);
void OhEpGlobDeleteAllPendingTD(HCM_ITEM_HEADER * EpHeader, U32             * TdCounter);
void OhEpGlobLinkEds           (HCM_ITEM_HEADER * Last,     HCM_ITEM_HEADER * New);
U32  OhEpGlobUnlinkEd          (HCM_ITEM_HEADER * Prev,     HCM_ITEM_HEADER * Remove);

void      OhEpGlobRemoveAllTDtoPool(HCM_ITEM_HEADER * EpHeader, U32 * TdCounter);
int       OhEpGlobIsTDActive       (HCM_ITEM_HEADER * EpHeader);
OHD_GTD * OhEpGlobGetLastTDFromED  (HCM_ITEM_HEADER * EpHeader);
OHD_GTD * OhEpGlobGetFirstTDFromED (HCM_ITEM_HEADER * EpHeader);
void      OhEpGlobDeleteDoneTD     (HCM_ITEM_HEADER * TdItem,   U32 * TdCounter);

void OhEpGlobClearSkip(HCM_ITEM_HEADER * EpHeader);
void OhEpGlobSetSkip  (HCM_ITEM_HEADER * EpHeader);
int  OhEpGlobIsSkipped(HCM_ITEM_HEADER * EpHeader);
void OhEpGlobClearHalt(HCM_ITEM_HEADER * EpHeader);
int  OhEpGlobIsHalt   (HCM_ITEM_HEADER * EpHeader);

void   OhEpClearToggle   (HCM_ITEM_HEADER * EpHeader);
void   OhEpSetToggle     (HCM_ITEM_HEADER * EpHeader, T_BOOL Toggle);
T_BOOL OhEpGetToggle     (HCM_ITEM_HEADER * EpHeader);
U32    OhEpGlobGetTdCount(HCM_ITEM_HEADER * EpHeader, HCM_POOL * TdPool);

typedef void * USBH_HC_BD_HANDLE; // Handle of the bus driver of a host controller
typedef void * USBH_HC_HANDLE;    // Context for the host controller driver

USBH_STATUS USBH_UBD_PreInit     (void);
USBH_STATUS USBH_UBD_Init        (void);
void        USBH_Exit            (void);
void        USBH_EnumerateDevices(USBH_HC_BD_HANDLE HcBdHandle);


/*********************************************************************
*
*       Host controller interface
*
**********************************************************************
*/

typedef void USBH_RootHubNotification(void * Context, U32 Notification); // bit0 indicates a status change of the HUB, bit 1 of port 1 of the hub and so on.

// Is called in the context of USBH_AddHostController make a basic initialization of the hardware,
// reset the hardware, setup internal lists, leave the host in the state  UBB_HOST_RESET
typedef USBH_STATUS USBH_HostInit(USBH_HC_HANDLE HcHandle, USBH_RootHubNotification UbdRootHubNotification, void * RootHubNotificationContext);

// Is the last call on this interface. It is called after all URB's are returned, all endpoints are released and no further
// reference to the host controller exists. In this call the host controller driver can check that all lists (URB's, Endpoints)
// are empty and delete all resources, disable interrupts. The HC state is USBH_HOST_RESET if this function is called
typedef USBH_STATUS USBH_HostExit(USBH_HC_HANDLE HcHandle);

typedef enum tag_USBH_HostState {
  USBH_HOST_RESET,                                                                          // Do nothing on the ports, power off
  USBH_HOST_RUNNING,                                                                        // Turn on generation of SOF
  USBH_HOST_SUSPEND                                                                         // Stop processing of all queues, stop SOF's
} USBH_HostState;

typedef USBH_STATUS USBH_SetHcState(USBH_HC_HANDLE   HcHandle, USBH_HostState   HostState); // Set the state of the HC
typedef U32    USBH_GetHcFrameNumber     (USBH_HC_HANDLE   HcHandle);                       // Returns the frame number as a 32 bit value
typedef void   USBH_HcCompletion         (void          * Context,  URB           * Urb);
typedef void * USBH_HC_EP_HANDLE;                                                           // Handle to an endpoint

typedef USBH_HC_EP_HANDLE USBH_ADD_ENDPOINT_FUNC(                                           // Returns an endpoint handle for a new created endpoint
USBH_HC_HANDLE HcHandle,
U8             EndpointType,                                                                // Type of the endpoint, one of USB_EP_TYPE_CONTROL, ...
U8             DeviceAddress,                                                               // Device address, 0 is allowed
U8             EndpointAddress,                                                             // Endpoint address with direction bit
U16            MaxFifoSize,                                                                 // Maximum transfer fifo size in the host controller for that endpoint
U16            IntervalTime,                                                                // Interval time in or the NAK rate if this is an USB highspeed bulk endpoint (in milliseconds)
USBH_SPEED     Speed                                                                        // The speed of the endpoint
);

// The bus driver calls these functions with valid handles. The HC must not take care to check the handles.

typedef void        USBH_RELEASE_EP_COMPLETION_FUNC(void * pContext);                         // This is the  USBH_ReleaseEndpoint completion function.
typedef void        USBH_RELEASE_ENDPOINT_FUNC     (USBH_HC_EP_HANDLE hEndPoint,
  USBH_RELEASE_EP_COMPLETION_FUNC ReleaseEpCompletion, void * pContext);                      // Releases that endpoint. This function returns immediately. If the Completion function is called the endpoint is removed.
typedef USBH_STATUS USBH_ABORT_ENDPOINT_FUNC       (USBH_HC_EP_HANDLE hEndPoint);             // Complete all pending requests. This function returns immediately. But the URB's may completed delayed, if the hardware require this.
typedef USBH_STATUS USBH_RESET_ENDPOINT_FUNC       (USBH_HC_EP_HANDLE hEndPoint);             // Resets the data toggle bit to 0. The bus driver takes care that this function is called only if no pending URB for this EP is scheduled.
typedef USBH_STATUS USBH_SUBMIT_REQUEST_FUNC       (USBH_HC_EP_HANDLE hEndPoint, URB * pUrb); // Submit a request to the HC. If USBH_STATUS_PENDING is returned the request is in the queue and the completion routine is called later.

/*********************************************************************
*
*       Root Hub Functions
*
**********************************************************************
*/

typedef unsigned int USBH_GET_PORT_COUNT_FUNC     (USBH_HC_HANDLE HcHandle); // Returns the number of root hub ports. An zero value is returned on an error.
typedef unsigned int USBH_GET_POWER_GOOD_TIME_FUNC(USBH_HC_HANDLE HcHandle); // Returns the power on to power good time in ms
typedef U32          USBH_GET_HUB_STATUS_FUNC     (USBH_HC_HANDLE HcHandle); // Returns the HUB status as defined in the USB specification 11.24.2.6

// This request is identical to an hub class ClearHubFeature request with the restriction that only HUB CHANGE bits can be cleared.
// For all other hub features other root hub functions must be used. The physical change bits are cleared in the root hub interrupt routine.
typedef void USBH_CLEAR_HUB_STATUS_FUNC (USBH_HC_HANDLE HcHandle, U16 FeatureSelector);
typedef U32  USBH_GET_PORT_STATUS_FUNC  (USBH_HC_HANDLE HcHandle, U8  Port); // One based index of the port / return the port status as defined in the USB specification 11.24.2.7

// This request is identical to an hub class ClearPortFeature request with the restriction that only PORT CHANGE bits can be cleared.
// For all other port features other root hub functions must be used. The physical change bits are cleared in the root hub interrupt routine.
typedef void USBH_CLEAR_PORT_STATUS_FUNC(USBH_HC_HANDLE HcHandle, U8  Port, U16 FeatureSelector); // One based index of the port

// Set the power state of a port. If the HC cannot handle the power switching for individual ports, it must turn on all ports if
// at least one port requires power. It turns off the power if no port requires power
typedef void USBH_SET_PORT_POWER_FUNC   (USBH_HC_HANDLE HcHandle, U8  Port, U8 PowerOn); // one based index of the port / 1 to turn the power on or 0 for off

// Reset the port (USB Reset) and send a port change notification if ready.
// If reset was successful the port is enabled after reset and the speed is detected
typedef void USBH_RESET_PORT_FUNC       (USBH_HC_HANDLE HcHandle, U8  Port); // One based index of the port
typedef void USBH_DISABLE_PORT_FUNC     (USBH_HC_HANDLE HcHandle, U8  Port); // One based index of the port// Disable the port, no requests and SOF's are issued on this port

typedef enum tag_USBH_PortPowerState {
  USBH_PortPowerRunning,
  USBH_PortPowerSuspend
} USBH_PortPowerState;

typedef void USBH_SET_PORT_SUSPEND_FUNC (USBH_HC_HANDLE HcHandle, U8  Port, USBH_PortPowerState State); // One based index of the port / Switch the port power between running and suspend

typedef struct {
  USBH_HC_HANDLE HcHandle;                         // Handle for the HC driver. It is passed to each function
  // Global HC functions
  USBH_HostInit                 * HostInit;        // Is called by the bus driver in the context of USBH_AddHostController
  USBH_HostExit                 * HostExit;
  USBH_SetHcState               * SetHcState;
  USBH_GetHcFrameNumber         * GetFrameNumber;
  // Endpoint functions
  USBH_ADD_ENDPOINT_FUNC        * AddEndpoint;     // Add an endpoint to the HC
  USBH_RELEASE_ENDPOINT_FUNC    * ReleaseEndpoint; // Release the endpoint, free the endpoint structure
  USBH_ABORT_ENDPOINT_FUNC      * AbortEndpoint;   // Return all pending requests from this endpoint with status USBH_STATUS_CANCELLED.
  // The requests must not completed with in the function call.
  USBH_RESET_ENDPOINT_FUNC      * ResetEndpoint;   // Reset the endpoint data toggle bit
  USBH_SUBMIT_REQUEST_FUNC      * SubmitRequest;   // Submit a request
  // Root Hub functions
  USBH_GET_PORT_COUNT_FUNC      * GetPortCount;
  USBH_GET_POWER_GOOD_TIME_FUNC * GetPowerGoodTime;
  USBH_GET_HUB_STATUS_FUNC      * GetHubStatus;
  USBH_CLEAR_HUB_STATUS_FUNC    * ClearHubStatus;
  USBH_GET_PORT_STATUS_FUNC     * GetPortStatus;
  USBH_CLEAR_PORT_STATUS_FUNC   * ClearPortStatus;
  USBH_SET_PORT_POWER_FUNC      * SetPortPower;
  USBH_RESET_PORT_FUNC          * ResetPort;
  USBH_DISABLE_PORT_FUNC        * DisablePort;
  USBH_SET_PORT_SUSPEND_FUNC    * SetPortSuspend;
} USB_HOST_ENTRY;

USBH_HC_BD_HANDLE USBH_AddHostController   (USB_HOST_ENTRY        * HostEntry);
void              USBH_RemoveHostController(USBH_HC_BD_HANDLE       HcBdHandle);
USBH_STATUS       USBH_ResetEndpoint       (USBH_INTERFACE_HANDLE   IfaceHandle, URB * urb, U8 Endpoint, USBH_ON_COMPLETION_FUNC Completion, void * Context);

#define OH_BULKINT_VALID(OHD_BULK_INT_EP_Ptr) HCM_ASSERT_ITEM_HEADER(&OHD_BULK_INT_EP_Ptr->ItemHeader)

// This macro need the struct and the name of the list entry inside the struct
#define GET_BULKINT_EP_FROM_ENTRY(pListEntry) STRUCT_BASE_POINTER(pListEntry, OHD_BULK_INT_EP, ListEntry)

struct T_OHD_DUMMY_INT_EP;

typedef struct tag_OHD_BULK_INT_EP { // Logical bulk and interrupt EP object
  // Recommended:
  //   First  field: HCM_ITEM_HEADER
  //   Second field: U8 EndpointType
  HCM_ITEM_HEADER                   ItemHeader;
  U8                                EndpointType;
  struct T_HC_DEVICE              * Dev;
  OhEpState                         State;

  DLIST                             ListEntry;         // The entry to keep the element in the HC list
  DLIST                             UrbList;           // Submitted URB list
  U32                               UrbCount;          // Number of requests
  URB                             * PendingUrb;        // Active URB  removed from the list
  struct T_OH_TRANSFER_BUFFER     * CopyBuffer;        // If URB's buffer address lies out of transfer memory range the buffer is copied

  U32                               UrbTotalTdNumber;  // Number of Tds for the current URB
  U32                               UrbDoneTdNumber;   // Current number of doen tds
  U32                               TdCounter;         // Current number of TDs on this ED
  U32                               UpDownTDCounter;   // DoneTDCounter is used in the done interrupt endpoint functions
  U32                               AbortMask;         // Current abort state  see also EP_SKIP_TIMEOUT_MASK and EP_ABORT_MASK

  int                               CancelPendingFlag; // TRUE if AbortEndpoint is called
  struct T_OHD_DUMMY_INT_EP       * DummyIntEp;        // DummyIntEp holds an backward pointer to an dummy interrupt endpoint
  USBH_RELEASE_EP_COMPLETION_FUNC * ReleaseCompletion; // Callback fuunction that is called if the endpoint is removed
  void                            * ReleaseContext;
  U32                               Flags;             // Endpoint flags

  // Members for operation
  U8                                DeviceAddress;
  U8                                EndpointAddress;
  U16                               MaxPktSize;        // Maximum transfer fifo size in the host controller for that endpoint
  USBH_SPEED                        Speed;
  U16                               IntervalTime;
  int                               HaltFlag;          // Set in DONE routine if HALT condition can not deleted! Reset only with an endpoint reset.
// This flag prevents submitting of new URBs!
} OHD_BULK_INT_EP;

/*********************************************************************
*
*       Resources allocation and releasing functions
*
**********************************************************************
*/

USBH_STATUS       OhBulkIntAllocPool(HCM_POOL        * EpPool, unsigned int MaxEps);
OHD_BULK_INT_EP * OhBulkIntEpGet    (HCM_POOL        * EpPool);
void              OhBulkIntEpPut    (OHD_BULK_INT_EP * Ep);

/*********************************************************************
*
*       Operations
*
**********************************************************************
*/

OHD_BULK_INT_EP * OhBulkIntInitEp                   (OHD_BULK_INT_EP * Ep, struct T_HC_DEVICE * Dev, U8 EndpointType, U8 DeviceAddress, U8 EndpointAddress, U16 MaxFifoSize, U16 IntervalTime, USBH_SPEED Speed, U32 Flags);
USBH_STATUS       OhBulkIntAddUrb                   (OHD_BULK_INT_EP * Ep, URB * Urb);
USBH_STATUS       OhBulkIntAbortEndpoint            (OHD_BULK_INT_EP * Ep);
USBH_STATUS       OhBulkIntCheckAndCancelAbortedUrbs(OHD_BULK_INT_EP * Ep, int TDDoneFlag);
void              OhBulk_ReleaseEndpoint            (OHD_BULK_INT_EP * Ep, USBH_RELEASE_EP_COMPLETION_FUNC * ReleaseEpCompletion, void * pContext);
void              OhBulkIntSubmitUrbsFromList       (OHD_BULK_INT_EP * Ep);
void              OhBulkInsertEndpoint              (OHD_BULK_INT_EP * Ep);
void              OhBulkIntUpdateTDLengthStatus     (OHD_GTD * Gtd);
void              OhBulkIntCheckForCompletion       (OHD_GTD * Gtd);
void              OhBulkAbortUrb_TimerCallback      (void    * Context);
void              OhBulkRemoveEp_TimerCallback      (void    * Context);
void              OhBulkRemoveEndpoints             (struct T_HC_DEVICE * dev, T_BOOL AllEndpointFlag);

typedef struct T_OHD_DUMMY_INT_EP { // Logical bulk and interrupt EP object
  // Recommended!!!:
  //   First  field: HCM_ITEM_HEADER
  //   Second field: U8 EndpointType
  HCM_ITEM_HEADER      ItemHeader;
  U8                   EndpointType; // Endpoint type
  struct T_HC_DEVICE * Dev;          // Entry to keep the element in the HC list
  OhEpState            State;
  DLIST                ActiveList;   // OHD_BULK_INT_EP (user endpoint) list
  U32                  Bandwidth;    // Sum of max packet sizes all appended user endpoints
  void               * NextDummyEp;  // Points to an NULL pointer if this is the 1ms interval or to the next dummy endpoint.
  U16                  IntervalTime; // Interval time 1,2 4,..ms
  U32                  Mask;         // Bits see usbohc_epglob.h
} OHD_DUMMY_INT_EP;

USBH_STATUS OhIntDummyEpAllocPool                (HCM_POOL           * EpPool);
void        OhInt_ReleaseEndpoint                (OHD_BULK_INT_EP    * Ep, USBH_RELEASE_EP_COMPLETION_FUNC * ReleaseEpCompletion, void * Context);
USBH_STATUS OhIntInsertEndpoint                  (OHD_BULK_INT_EP    * Ep);
USBH_STATUS OhInitAllocDummyInterruptEndpoints   (struct T_HC_DEVICE * Dev);
void        OhIntPutAllDummyEp                   (struct T_HC_DEVICE * Dev);
void        OhIntBuildDummyEpTree                (struct T_HC_DEVICE * Dev);
void        OhInt_RemoveAllUserEDFromPhysicalLink(struct T_HC_DEVICE * dev);
T_BOOL      OhIntRemoveEDFromLogicalListAndFree  (struct T_HC_DEVICE * dev, T_BOOL all);
void        OhIntReleaseEp_TimerCallback         (void               * Context);
void        OhIntAbortUrb_TimerCallback          (void               * Context);

#define OH_HCCA_VALID(OHD_HCCA_Ptr) HCM_ASSERT_ITEM_HEADER(&OHD_HCCA_Ptr->ItemHeader)

typedef struct tag_OH_HHCA {  // Logical HCCA object
  HCM_ITEM_HEADER ItemHeader; // For dynamic allocating the struct must begin with the specific memory pool header
} OHD_HCCA;

OHD_HCCA * OhHccaAlloc            (HCM_POOL * Pool);
void       OhHccaRelease          (OHD_HCCA * OhdHcca);
void       OhHccaSetInterruptTable(OHD_HCCA * OhdHcca, OHD_DUMMY_INT_EP * dummyInterruptEndpointList []);

/*********************************************************************
*
*       Endpoint descriptor
*
**********************************************************************
*/

typedef struct _OHCI_ED {
  volatile U32 Dword0;
  volatile U32 TailP;
  volatile U32 HeadP;
  volatile U32 NextED;
} OHCI_ED;

#define OH_ED_SIZE          (sizeof(OHCI_ED))
#define OH_ED_ALIGNMENT     16
#define OHCI_ED_FA          0x0000007fL // Function addresses
#define OHCI_ED_EN          0x00000780L // Endpoint number
#define OHCI_ED_EN_BIT      7
#define OHCI_ED_D           0x00001800L // direction
#define OHCI_ED_DIR_BIT     11
#define OHCI_ED_S           0x00002000L // Low Speed active
#define OHCI_ED_K           0x00004000L // Skip
#define OHCI_ED_F           0x00008000L // Format (iso)
#define OHCI_ED_MPS         0x07ff0000L // Maximum packet size
#define OHCI_ED_RSV         0xf8000000L // Reserved bits
#define OHCI_ED_DIR_FROM_TD 0x00
#define OHCI_ED_OUT_DIR     0x01
#define OHCI_ED_IN_DIR      0x02

// Mask on ED fields HeadP
#define OHCI_ED_C           0x02L // Last data toggle carry bit
#define OHCI_ED_H           0x01L // ED halt bit

/*********************************************************************
*
*       General transfer descriptor
*
**********************************************************************
*/

typedef struct _OHCI_TD {
  volatile U32 Dword0;
  volatile U32 CBP;
  volatile U32 NextTD;
  volatile U32 BE;
} OHCI_TD;

#define OH_GTD_SIZE         (sizeof(OHCI_TD))
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

typedef struct _OHCI_TDI {
  volatile U32 Dword0;
  volatile U32 Dword1;
  volatile U32 NextTD;
  volatile U32 BE;
  volatile U16 OfsPsw[8];
} OHCI_TDI;

#define OH_ISO_TD_SIZE          (sizeof(OHCI_TDI))
#define OH_ISO_TD_ALIGNMENT     32
// Dword0
#define OHCI_TDI_SF             0x0000ffffL
#define OHCI_TDI_RSV            0x001f0000L
#define OHCI_TDI_DI             0x00e00000L
#define OHCI_TDI_FC             0x07000000L
#define OHCI_TDI_RSV1           0x08000000L
#define OHCI_TDI_CC             0xf0000000L
// Dword1
#define OHCI_TDI_BP0            0xfffff000L
#define OHCI_TDI_RSV2           0x00000fffL
// Following TD bits can be used for own information
#define OHCI_TD_NOT_USED_BIT_1  16
#define OHCI_TD_NOT_USED_BIT_2  17

/*********************************************************************
*
*       Host Controller Communications area HCCA
*
**********************************************************************
*/

typedef struct _OHCI_HCCA { // 256 bytes
  volatile U32 InterruptTable[32];
  volatile U16 FrameNumber;
  volatile U16 Pad1;
  volatile U32 DoneHead;
  volatile U32 Reserved[30];
} OHCI_HCCA, * POHCI_HCCA;

#define OH_DONE_HEAD_INT_MASK  0x00000001L // Set if another OHCI interrupt is occurred

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

#define GET_PORTSTATUS_REG(port) (OH_REG_RHPORTSTATUS + (((port)-1) << 2))  // Return the portstatus register param: Port: Portindex is an one based index

#define HC_CONTROL_CBSR       0x00000003L                                   // Control bulk ratio
#define HC_CONTROL_PLE        0x00000004L                                   // Periodic list enable
#define HC_CONTROL_IE         0x00000008L                                   // Isochronous enable
#define HC_CONTROL_CLE        0x00000010L                                   // Control list enable
#define HC_CONTROL_BLE        0x00000020L                                   // Bulk list enable
#define HC_CONTROL_HCFS       0x000000c0L                                   // Host functional state mask
#define HC_CONTROL_IR         0x00000100L                                   // Interrupt routing enable
#define HC_CONTROL_RWC        0x00000200L                                   // Indicates whether HC supports remote wakeup signaling.
#define HC_CONTROL_RWE        0x00000400L                                   // Enable remote wakeup
#define HC_USB_RESET          0x00
#define HC_USB_RESUME         0x40
#define HC_USB_OPERATIONAL    0x80
#define HC_USB_SUSPEND        0xc0
#define HC_COMMAND_STATUS_HCR 0x00000001L // Host reset
#define HC_COMMAND_STATUS_CLF 0x00000002L // Control list filled
#define HC_COMMAND_STATUS_BLF 0x00000004L // Bulk list filled
#define HC_COMMAND_STATUS_OCR 0x00000008L // Ownership change request
#define HC_COMMAND_STATUS_SOC 0x00030000L // Scheduling overrun

// HcInterruptEnable / Disable HCInterruptStatus
#define HC_INT_SO   0x00000001L                     // Scheduling overrun
#define HC_INT_WDH  0x00000002L                     // HcDoenHead
#define HC_INT_SF   0x00000004L                     // SOF
#define HC_INT_RD   0x00000008L                     // Resume detect
#define HC_INT_UE   0x00000010L                     // Unrecoverable error
#define HC_INT_FNO  0x00000020L                     // Frame number overflow, resume no frame number generation
#define HC_INT_RHSC 0x00000040L                     // Root hub status change
#define HC_INT_OC   0x40000000L                     // Ownership change
#define HC_INT_MIE  0x80000000L                     // Master interrupt bit

#define HC_INT_STATUS_VALIDATION_BIT ((U32)1 << 31) // Bit in the HC_INT_STATUS_VALID_MASK in the interrupt status register must be zero
#define HC_INT_STATUS_MASK_WITHOUT_OWNERCHIP 0x07f

// Frame interval
#define HC_FM_INTERVAL_FIT        ((U32)1<<31)
#define HC_FM_INTERVAL_FSMPS_BIT  16
#define HC_FM_INTERVAL_FSMPS_MASK 0x7FFF0000L
#define HC_FM_INTERVAL_FI_MASK    0x00003FFFL

/*********************************************************************
*
*       ROOT HUB registers
*
**********************************************************************
*/

// Root Hub Descriptor A register
#define ROOT_HUB_MAX_DOWNSTREAM_PORTS 15
#define ROOT_HUB_NDP                  0x0ff       // Number of ports mask
#define ROOT_HUB_PSM                  0x00000100L // Power switch mode
#define ROOT_HUB_NPS                  0x00000200L // NoPowerSwitching, ports always on!!
#define ROOT_HUB_DT                   0x00000400L // Compound device always zero on root hub
#define ROOT_HUB_OCPM                 0x00000800L // Overcurrent per port
#define ROOT_HUB_NOCP                 0x00001000L // No overcurrent protection
#define ROOT_HUB_POTPGT_BIT           24          // Power on to power good time in units of 2 ms max.0xff

// Root Hub Descriptor B register
#define ROOT_HUB_DR_MASK(Port)   (1        <<(Port)) // Device is removable
#define ROOT_HUB_PPCM_MASK(Port) (0x10000L <<(Port)) // Port powered control mask, Port is one indexed!

// Root Hub status register
#define ROOT_HUB_LPS  0x00000001L // LocalPowerStatus Clear global power
#define ROOT_HUB_OCI  0x00000002L // OverCurrentIndicator If not per port poover current protection is active
#define ROOT_HUB_DRWE 0x00008000L // This bit enables a ConnectStatusChange bit as a resume event, causing a USBSUSPEND to USBRESUME
// state transition and setting the ResumeDetected interrupt. Must be used if the host is in
// suspend state and must be wake up if an device is connected or removed in this state. The resume
// signal detection is in suspend state always enabled.
#define ROOT_HUB_LPSC 0x00010000L       // LocalPowerStatusChange SetGlobalPower
#define ROOT_HUB_OCIC 0x00020000L       // OverCurrentIndicatorChange.
#define ROOT_HUB_CRWE 0x80000000L       // ClearRemoteWakeupEnable. Ends the remote wakeup signaling. only write enabled
#define ROOT_HUB_CHANGE_MASK (ROOT_HUB_LPSC | ROOT_HUB_OCIC)

#define RH_PORT_STATUS_CCS  0x00000001L // CurrentCOnnectStatus. ClearPortEnable Always 1 if the device is nonremovablee
#define RH_PORT_STATUS_PES  0x00000002L // PortEnableStatus, SetPortEnable
#define RH_PORT_STATUS_PSS  0x00000004L // PortSuspendStatus, SetPortSuspend
#define RH_PORT_STATUS_POCI 0x00000008L // PortOverCurrentIndicator, ClearSuspendStatus
#define RH_PORT_STATUS_PRS  0x00000010L // PortResetStatus, SetPortReset
#define RH_PORT_STATUS_PPS  0x00000100L // PortPowerStatus (regardless of type of power switching mode) SetPortPower
#define RH_PORT_STATUS_LSDA 0x00000200L // LowSpeedDeviceAttached, ClearPortPower

// Root Hub Port status change bits
#define RH_PORT_CH_BIT_MASK        0x001F0000L // Change status bits
#define RH_PORT_STATUS_CSC         0x00010000L // ConnectStatusChange Request + clear
#define RH_PORT_STATUS_PESC        0x00020000L // PortEnableStatusChnage + Clear
#define RH_PORT_STATUS_PSSC        0x00040000L // PortSuspendStatusChnage (full resume sequence has been completed) + clear
#define RH_PORT_STATUS_OCIC        0x00080000L // PortOverCurrentIndicatorChange + clear
#define RH_PORT_STATUS_PRSC        0x00100000L // Port Reset Status change + clear (end of 10ms port reset signal)
#define HUB_PORT_STATUS_HIGH_SPEED 0x00000400L
#define HUB_PORT_STATUS_TEST_MODE  0x00000800L
#define HUB_PORT_STATUS_INDICATOR  0x00001000L

// Relevant status bits in the root hub port status register.
// This includes: Connect-,Enable-,Suspend-,Overcurrent-,Reset-,Portpower- and Speedstatus.
#define HUB_PORT_STATUS_MASK       0x0000031FL
#define HUB_PORT_CHANGE_MASK       ( RH_PORT_STATUS_CSC  \
                                   | RH_PORT_STATUS_PESC \
                                   | RH_PORT_STATUS_PSSC \
                                   | RH_PORT_STATUS_OCIC \
                                   | RH_PORT_STATUS_PRSC )

// Interrupt intervals
#define OHD_1MS   1
#define OHD_2MS   2
#define OHD_4MS   4
#define OHD_8MS   8
#define OHD_16MS  16
#define OHD_32MS  32

#define OHD_DUMMY_INT_NUMBER  (OHD_1MS + OHD_2MS + OHD_4MS + OHD_8MS + OHD_16MS +  OHD_32MS)

typedef void * USBH_TIMER_HANDLE;                                                                   // Handle to a TAL timer object
typedef void   USBH_TIMER_CB_ROUTINE(void * pContext);                                              // Typedef callback function which is called on a timer timeout

USBH_TIMER_HANDLE USBH_AllocTimer (USBH_TIMER_CB_ROUTINE * pfTimerCallbackRoutine, void * Context); // Allocates a timer object and returns the timer handle.
void              USBH_FreeTimer  (USBH_TIMER_HANDLE TimerHandle);                                  // Frees a timer object via timer handle.
void              USBH_StartTimer (USBH_TIMER_HANDLE TimerHandle, U32 ms);                          // Starts a timer. The timer is restarted again if it is running.
void              USBH_CancelTimer(USBH_TIMER_HANDLE TimerHandle);                                  // Cancels an timer if running, the completion routine is not called.

/*********************************************************************
*
*       Structs
*
**********************************************************************
*/

// Prototype to enter and leave synchronization used at the start and the after the end  of the timer routine
typedef void TIMER_ROUTINE_SYNCH(void);
typedef void TIMER_CALLBACK_ROUTINE(void * Context);

#define TAL_TIMER_ENTRY_MAGIC    FOUR_CHAR_ULONG('T','I','M','E')

struct TAL_TIMER_tag;

#define GET_TIMER_FROM_ENTRY(pListEntry) STRUCT_BASE_POINTER(pListEntry, TAL_TIMER_ENTRY, entry)

typedef struct TIMER_ENTRY_tag {
#if (USBH_DEBUG > 1)
  U32 Magic;
#endif
  struct TAL_TIMER_tag   * TalTimer;        // Backward pointer
  DLIST                    entry;           // Link element
  TIMER_CALLBACK_ROUTINE * CallbackRoutine;
  void                   * CallbackContext;
  unsigned int             TimerId;         // TimerID form the started Timer, set to zero before the timer is killed
  U32                      StartTime;       // Absolute time where the timeout ends
  U32                      Timeout;         // Timeout in milliseconds or zero if this is an inactive Timer.
  U32                      RemoveFlag;      // TRUE if the timer entry has to removed from the list
  U32                      CancelFlag;      // TRUE: if the timer is canceled
} TAL_TIMER_ENTRY;

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

#define OH_ASSERT_PORT_NUMBER(devPtr,PortNumber) T_ASSERT((PortNumber) != 0); T_ASSERT((PortNumber) <= (devPtr) -> RootHub.PortCount)

/*********************************************************************
*
*       Additional external hub definitions
*
**********************************************************************
*/

// Root Hub macros
// Port number is an one based index of the port
#define RhReadStatus(             DevPtr)                  OhHalReadReg( (DevPtr)->RegBase,OH_REG_RHSTATUS          ) // Reading and writing HcRhStatus Register
#define RhWriteStatus(            DevPtr, Status)          OhHalWriteReg((DevPtr)->RegBase,OH_REG_RHSTATUS, (Status))
#define RhReadPort(               DevPtr, Port)            OhHalReadReg( (DevPtr)->RegBase,GET_PORTSTATUS_REG(Port) ) // Reading and writing hub port status register
#define RhWritePort(              DevPtr, Port,StatusMask) OhHalWriteReg((DevPtr)->RegBase,GET_PORTSTATUS_REG(Port  ), (StatusMask))

#define RhSetGlobalPower(         DevPtr)       RhWriteStatus((DevPtr),        ROOT_HUB_LPSC      )                   // switch on global power pins
#define RhClearGlobalPower(       DevPtr)       RhWriteStatus((DevPtr),        ROOT_HUB_LPS       )                   // switch off all global ports power
#define RhResetPort(              DevPtr, Port) RhWritePort(  (DevPtr),(Port), RH_PORT_STATUS_PRS )                   // Start an port based USB reset signal
#define RhSetPortPower(           DevPtr, Port) RhWritePort(  (DevPtr),(Port), RH_PORT_STATUS_PPS )                   // Switch on port power on an port
#define RhClearPortPower(         DevPtr, Port) RhWritePort(  (DevPtr),(Port), RH_PORT_STATUS_LSDA)                   // Switch odd port power on an port
#define RhSetPortSuspend(         DevPtr, Port) RhWritePort(  (DevPtr),(Port), RH_PORT_STATUS_PSS )
#define RhStartPortResume(        DevPtr, Port) RhWritePort(  (DevPtr),(Port), RH_PORT_STATUS_POCI)                   // clear suspend status starts resume signaling
#define RhClearPortResumeChange(  DevPtr, Port) RhWritePort(  (DevPtr),(Port), RH_PORT_STATUS_PSSC)
#define RhSetPortReset(           DevPtr, Port) RhWritePort(  (DevPtr),(Port), RH_PORT_STATUS_PRS )
#define RhDisablePort(            DevPtr, Port) RhWritePort(  (DevPtr),(Port), RH_PORT_STATUS_CCS )

#define RhIsPortConnected(        PortStatus) ( (PortStatus) & RH_PORT_STATUS_CCS       )
#define RhIsPortResetActive(      PortStatus) ( (PortStatus) & RH_PORT_STATUS_PRS       )
#define RhIsPortResetChangeActive(PortStatus) ( (PortStatus) & RH_PORT_STATUS_PRSC      )
#define RhIsPortEnabled(          PortStatus) ( (PortStatus) & RHP_ENABLE_STATUS_CHANGE )
#define RhIsPortConnectChanged(   PortStatus) ( (PortStatus) & RH_PORT_STATUS_PESC      )
#define RhIsLowSpeed(             PortStatus) ( (PortStatus) & RH_PORT_STATUS_LSDA ? 1:0)
#define UmhHalIsSuspended(        PortStatus) ( (PortStatus) & RH_PORT_STATUS_PSS  ? 1:0)

#if HC_ROOTHUB_PER_PORT_POWERED
#define INIT_ROOT_HUB_STATUS  0

#else
#define INIT_ROOT_HUB_STATUS  ROOT_HUB_LPS // Switch of all port power

#endif

#if HC_ROOTHUB_OVERCURRENT
  #define OH_OVERCURRENT_MASK   0
#else
  #define OH_OVERCURRENT_MASK  ROOT_HUB_NOCP   // No overcurrent protection circuit
#endif

#if HC_ROOTHUB_PORTS_ALWAYS_POWERED            // Ports always powered ROOT_HUB_NPS: ports always powered on
  #define INIT_ROOT_HUB_DESC_A (((POWERON_TO_POWERGOOD_TIME/2) << ROOT_HUB_POTPGT_BIT) | OH_OVERCURRENT_MASK | ROOT_HUB_NPS)
  #define INIT_ROOT_HUB_DESC_B  0
#else
  #if HC_ROOTHUB_PER_PORT_POWERED              // Check per port powered
    #define INIT_ROOT_HUB_DESC_A (((POWERON_TO_POWERGOOD_TIME/2) << ROOT_HUB_POTPGT_BIT) | OH_OVERCURRENT_MASK | ROOT_HUB_OCPM | ROOT_HUB_PSM)
    #define INIT_ROOT_HUB_DESC_B  0
  #else
    #define INIT_ROOT_HUB_DESC_A (((POWERON_TO_POWERGOOD_TIME/2) << ROOT_HUB_POTPGT_BIT) | OH_OVERCURRENT_MASK)
    #define INIT_ROOT_HUB_DESC_B  0
  #endif
#endif

typedef struct T_HC_HUB_PORT { // Hub port object
  U8  Port;                     // One base port index
  U8  Power;                    // 1- power on 0 -power off
  U16 Status;                  // Port status and change bits
  U16 Change;
} HC_HUB_PORT;

typedef struct T_HC_ROOT_HUB {                       // Root Hub object
  struct T_HC_DEVICE       * Dev;
  USBH_RootHubNotification * UbdRootHubNotification; // External init values and callback functions
  void                     * RootHubNotificationContext;
  U16                        PortCount;
  U16                        PowerOnToPowerGoodTime;
  U16                        Status; // Hub status and change bits
  U16                        Change;
  HC_HUB_PORT              * apHcdPort;
} HC_ROOT_HUB;

USBH_STATUS OhRhInit            (struct T_HC_DEVICE * Dev, USBH_RootHubNotification * UbdRootHubNotification, void * RootHubNotificationContext);
void        OhRhProcessInterrupt(HC_ROOT_HUB        * Hub);

/*********************************************************************
*
*       Driver root hub interface
*
**********************************************************************
*/

unsigned int OhRh_GetPortCount    (USBH_HC_HANDLE HcHandle);
unsigned int OhRh_GetPowerGoodTime(USBH_HC_HANDLE HcHandle);
U32          OhRh_GetHubStatus    (USBH_HC_HANDLE HcHandle);
void         OhRh_ClearHubStatus  (USBH_HC_HANDLE HcHandle,          U16 FeatureSelector);
void         OhRh_ClearPortStatus (USBH_HC_HANDLE HcHandle, U8 Port, U16 FeatureSelector);
U32          OhRh_GetPortStatus   (USBH_HC_HANDLE HcHandle, U8 Port);
void         OhRh_SetPortPower    (USBH_HC_HANDLE HcHandle, U8 Port, U8 PowerOn);
void         OhRh_ResetPort       (USBH_HC_HANDLE HcHandle, U8 Port);
void         OhRh_DisablePort     (USBH_HC_HANDLE HcHandle, U8 Port);
void         OhRh_SetPortSuspend  (USBH_HC_HANDLE HcHandle, U8 Port, USBH_PortPowerState State);

typedef void REMOVE_HC_COMPLETION (void * Context);

typedef struct T_HC_DEVICE { // The global driver object. The object is cleared in the function OHC_HostInit!
#if (USBH_DEBUG > 1)
  U32 Magic;
#endif
  // If the transfer buffer in an URB out of physical memory address range then this buffer is transferred with an buffer from this pool
  HCM_POOL   TransferBufferPool;
  // EP pools
  HCM_POOL             IsoEPPool;
  HCM_POOL             BulkEPPool;
  HCM_POOL             IntEPPool;
  HCM_POOL             DummyIntEPPool;
  HCM_POOL             ControlEPPool;
  HCM_POOL             SetupPacketPool;  // Setup buffer pool
  HCM_POOL             GTDPool;          // GTD pool
  HCM_POOL             IsoTDPool;        // ISO TD pool
  HCM_POOL             HccaPool;         // HCCA Pool
  OHD_HCCA           * Hcca;             // Pointer to the HccaPoolItem
  volatile OHCI_HCCA * OhHcca; // OHCI HCCA memory, points to the virtual base address of HCCA,  used to access the HCCA memory

  // Control endpoint
  DLIST               ControlEpList; // Number of pending endpoints on the HC
  U32                 ControlEpCount;
  USBH_TIMER_HANDLE   ControlEpRemoveTimer;
  USBH_TIMER_HANDLE   ControlEpAbortTimer;

  // True if timer is started, set to false in the timer routine
  T_BOOL              ControlEpRemoveTimerRunFlag;
  T_BOOL              ControlEpRemoveTimerCancelFlag;
  T_BOOL              ControlEpAbortTimerRunFlag;
  T_BOOL              ControlEpAbortTimerCancelFlag;
  DLIST               BulkEpList;
  U32                 BulkEpCount;
  USBH_TIMER_HANDLE   BulkEpRemoveTimer;

  // True if timer is started, set to false in the timer routine
  T_BOOL              BulkEpRemoveTimerRunFlag;
  T_BOOL              BulkEpRemoveTimerCancelFlag;
  USBH_TIMER_HANDLE   BulkEpAbortTimer;

  // True if timer is started, set to false in the timer routine
  T_BOOL              BulkEpAbortTimerRunFlag;
  T_BOOL              BulkEpAbortTimerCancelFlag;

  // The array index 0 contains the dummy EP of 1milliseconds, the index 1 the dummy EPs of 2 milliseconds and so on
  // IntervalStartEpIndex=Intervaltime-1. Every dummy EP!
  OHD_DUMMY_INT_EP  * DummyInterruptEpArr[OHD_DUMMY_INT_NUMBER];
  U32 IntEpCount; // IntEpCount is an reference counter that counts the number of active interrupt endpoints without the dummy interrupt endpoints
  USBH_TIMER_HANDLE   IntEpRemoveTimer;

  // True if timer is started, set to false in the timer routine
  T_BOOL              IntEpRemoveTimerRunFlag;
  T_BOOL              IntEpRemoveTimerCancelFlag;
  USBH_TIMER_HANDLE   IntEpAbortTimer;

  // True if timer is started, set to false in the timer routine
  T_BOOL              IntEpAbortTimerRunFlag;
  T_BOOL              IntEpAbortTimerCancelFlag;

#if (USBH_DEBUG > 1)
  T_BOOL              IntRemoveFlag;
#endif
  DLIST               IsoEpList;
  U32                 IsoEpCount;
  USBH_TIMER_HANDLE   InitDeviceTimer; // This timer is used in the initialization routine

  // UpperFrameCounter: valid bits: Bit 16..to Bit 31
  // LSB frame word: in the Hc area.
  volatile U16        UpperFrameCounter;
  volatile U16        LastFrameCounter;
  U8                * RegBase;      // OHCI register base address
  U32                 FmIntervalReg; // Saved content from interval register
  OhDevState          state;  // Devices state
  HC_ROOT_HUB         RootHub;
  USBH_HC_BD_HANDLE   ubdHandle;

  // HostExit Callback function and context
  REMOVE_HC_COMPLETION * RemoveCompletion;
  void                 * RemoveContext;
} HC_DEVICE;

T_BOOL OhdIsTimeOver(U32 Waittime, U32 StartTime);

/*********************************************************************
*
*       Host device driver interface functions
*
**********************************************************************
*/

void              Ohd_ReleaseEndpoint(USBH_HC_EP_HANDLE EpHandle, USBH_RELEASE_EP_COMPLETION_FUNC pfReleaseEpCompletion, void * pContext);
USBH_STATUS       OHC_HostInit       (USBH_HC_HANDLE    HcHandle, USBH_RootHubNotification UbdRootHubNotification, void * RootHubNotificationContext);
USBH_STATUS       Ohd_SetHcState     (USBH_HC_HANDLE    HcHandle, USBH_HostState HostState);
USBH_STATUS       OHC_HostExit       (USBH_HC_HANDLE    HcHandle);
USBH_HC_EP_HANDLE Ohd_AddEndpoint    (USBH_HC_HANDLE    HcHandle, U8 EndpointType, U8 DeviceAddress, U8 EndpointAddress, U16 MaxFifoSize, U16 IntervalTime, USBH_SPEED Speed);

void                 OhdUpdateUpperFrameCounter(HC_DEVICE          * dev);
void                 OhdEndpointListEnable     (HC_DEVICE          * Dev, U8 EpType, T_BOOL Enable, T_BOOL ListFill);
U32                  Ohd_GetFrameNumber        (USBH_HC_HANDLE       HcHandle);
OH_TRANSFER_BUFFER * OhGetCopyTransferBuffer   (HCM_POOL           * transferBufferPool);
void                 OhPutTransferBuffer       (OH_TRANSFER_BUFFER * transferBuffer);

OH_TRANSFER_BUFFER * OhGetInitializedCopyTransferBuffer     (HCM_POOL           * transferBufferPool, U8 * urbBuffer, U32 urbBufferLength);
U32                  OhFillCopyTransferBuffer               (OH_TRANSFER_BUFFER * transferBuffer);
U32                  OhCopyToUrbBufferUpdateTransferBuffer  (OH_TRANSFER_BUFFER * transferBuffer,     U32   transferred);
U32                  OhUpdateCopyTransferBuffer             (OH_TRANSFER_BUFFER * transferBuffer,     U32   transferred);
U8                 * OhGetBufferLengthFromCopyTransferBuffer(OH_TRANSFER_BUFFER * transferBuffer,     U32 * length);

// The global instance of the HC device. ### it depends on the implementation of the HC is global and if the resource assignment
// is static or dynamic. This member must not be touched inside the library. It is sued from external to connect the library to
// the bus driver wit the call USBH_AddHostController. It is used as USBH_HC_HANDLE in the USB_HOST_ENTRY structure.
extern HC_DEVICE gUsbDevice;

typedef struct tag_URB_SUB_STATE {
  U8                           TimerCancelFlag; // Timer to for detecting an timeout
  USBH_TIMER_HANDLE            Timer;
  SUBSTATE_STATE               state;
  URB                        * Urb;
  // Additional pointer for faster accesses
  struct tag_HOST_CONTROLLER * Hc;
  USBH_HC_EP_HANDLE          * EpHandle;
  USBH_SUBMIT_REQUEST_FUNC   * SubmitRequest;
  USBH_ABORT_ENDPOINT_FUNC   * AbortEndpoint;
  struct tag_USB_DEVICE      * RefCtDev;
  SubStateCallbackRoutine    * CallbackRoutine; // This callback routine is called if an URB is complete or on an timer timeout
  // started with UrbSubStateWait. If the timer routine runs and an pending urb exist
  // then the urb ios aborted and the CallbackRoutine is not called.
  void                       * Context;
} URB_SUB_STATE;

URB_SUB_STATE * UrbSubStateAllocInit                              (struct tag_HOST_CONTROLLER * hc, USBH_HC_EP_HANDLE * epHandle, SubStateCallbackRoutine * cbRoutine, void * context);
USBH_STATUS     UrbSubStateInit         (URB_SUB_STATE * subState, struct tag_HOST_CONTROLLER * hc, USBH_HC_EP_HANDLE * epHandle, SubStateCallbackRoutine * cbRoutine, void * context);
USBH_STATUS     UrbSubStateSubmitRequest(URB_SUB_STATE * subState, URB * urb, U32 timeout, struct tag_USB_DEVICE * refCtDev);
void            UrbSubStateWait         (URB_SUB_STATE * subState, U32 timeout, struct tag_USB_DEVICE * refCtDev);
void            UrbSubStateExit         (URB_SUB_STATE * subState);
void            UrbSubStateFree         (URB_SUB_STATE * subState);
T_BOOL          UrbSubStateIsIdle       (URB_SUB_STATE * subState);

USBH_STATUS OhT_SubmitRequest      (USBH_HC_EP_HANDLE   EpHandle, URB * Urb);
void        OhTProcessDoneInterrupt(HC_DEVICE         * dev);

// URBs HcFlags allowed values
#define URB_CANCEL_PENDING_MASK 0x01 // Pending URB must be canceled

#define DEFAULT_EP_MAGIC      FOUR_CHAR_ULONG('E','P','0',' ')

typedef struct tag_DEFAULT_EP {
#if (USBH_DEBUG > 1)
  U32                     Magic;
#endif
  struct tag_USB_DEVICE * UsbDevice; // Pointer to the owning host controller
  USBH_HC_EP_HANDLE       EpHandle;  // Endpoint handle must be used to submit an URB
  unsigned int            UrbCount;
} DEFAULT_EP;


// State for device enumeration
typedef enum tag_DEV_ENUM_STATE {
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

typedef enum tag_USB_DEV_STATE { // Do not modify the sequence
  DEV_STATE_UNKNOWN = 0,
  DEV_STATE_REMOVED,
  DEV_STATE_ENUMERATE,
  DEV_STATE_WORKING,
  DEV_STATE_SUSPEND
} USB_DEV_STATE;

// Is called after the standard enumeration has been completed
typedef void POST_ENUM_FUNCTION(void* Context);

typedef struct tag_USB_DEVICE {
#if (USBH_DEBUG > 1)
  U32                          Magic;
#endif
  DLIST                        ListEntry;              // To store this object in the host controller object
  DLIST                        TempEntry;              // To store this object in an temporary list
  T_BOOL                       TempFlag;
  long                         RefCount;
  struct tag_HOST_CONTROLLER * HostController;         // Pointer to the owning host controller
  DLIST                        UsbInterfaceList;       // List for interfaces
  unsigned int                 InterfaceCount;
  struct tag_HUB_PORT        * ParentPort;             // This is the hub port where this device is connected to
  U8                           UsbAddress;             // This is the USB address of the device
  USBH_SPEED                   DeviceSpeed;            // Speed of the device connection
  U8                           MaxFifoSize;            // The FIFO size
  U8                           ConfigurationIndex;     // The index of the current configuration

  // Descriptors
  USB_DEVICE_DESCRIPTOR        DeviceDescriptor;       // A typed copy
  U8                           DeviceDescriptorBuffer[USB_DEVICE_DESCRIPTOR_LENGTH];
  U8                         * ConfigDescriptor;       // Points to the current configuration descriptor
  U16                          ConfigDescriptorSize;
  U16                          LanguageID;             // First language ID
  U8                         * SerialNumber;           // Serial number without header, UNICODE
  unsigned int                 SerialNumberSize;
  U16                          DevStatus;              // Device status returned from USB GetStatus
  struct tag_DEFAULT_EP        DefaultEp;              // Embedded default endpoint
  struct tag_USB_HUB         * UsbHub;                 // This pointer is valid if the device is a hub
  USB_DEV_STATE                State;                  // Current device state
  URB                          EnumUrb;                // Embedded URB
  void                       * CtrlTransferBuffer;     // Used from UbdProcessEnum and ProcessEnumHub()
  unsigned int                 CtrlTransferBufferSize;
  // State variables for device enumeration
  // Enumeration state
  URB_SUB_STATE                SubState;
  DEV_ENUM_STATE               EnumState;
  T_BOOL                       EnumSubmitFlag;         // Used during enumeration if the device is as an hub
  // Post enumeration
  POST_ENUM_FUNCTION         * PostEnumFunction;
  void                       * PostEnumerationContext;
  USBH_DEVICE_ID               DeviceID;               // Device ID for this device
} USB_DEVICE;

#define USB_ENDPOINT_MAGIC      FOUR_CHAR_ULONG('E','N','D','P')

typedef struct tag_USB_ENDPOINT {
  DLIST                      ListEntry;          // to store this object in the interface object */
#if (USBH_DEBUG > 1)
  U32                        Magic;
#endif
  struct tag_USB_INTERFACE * UsbInterface;       // Backward pointer
  U8                       * EndpointDescriptor; // Descriptors
  USBH_HC_EP_HANDLE          EpHandle;           // Endpoint handle must be used to submit an URB
  U32                        UrbCount;
} USB_ENDPOINT;

#define USB_INTERFACE_MAGIC FOUR_CHAR_ULONG('U','I','F','U')

#define USB_MAX_ENDPOINTS 32 // Needs the struct and the name of the list entry inside the struct.

// Make a index in the range between 0 and 31 from an EP address IN EP's in the range from 0x10 to 0x1f, OUT EP's are in the range 0x00 to 0x0f
#define EP_INDEX(EpAddr) ((EpAddr) & 0x80) ? (((EpAddr)&0xf) | 0x10) : ((EpAddr)&0xf)

typedef struct tag_USB_INTERFACE {
#if (USBH_DEBUG > 1)
  U32                 Magic;
#endif
  DLIST               ListEntry;                // To store this object in the device object
  USB_DEVICE        * Device;                   // Backward pointer
  DLIST               UsbEndpointList;          // List for endpoints
  unsigned int        EndpointCount;
  U8                  CurrentAlternateSetting;
  U8                * InterfaceDescriptor;
  U8                * AlternateSettingDescriptor;
  U8                  NewAlternateSetting;
  U8                * NewAlternateSettingDescriptor;
  unsigned int        OpenCount;
  U8                  ExclusiveUsed;
  USB_ENDPOINT      * EpMap[USB_MAX_ENDPOINTS]; // A map for fast access to endpoint structures
  USBH_INTERFACE_ID   InterfaceID;              // ID of this interface
} USB_INTERFACE;

USBH_STATUS UbdInitDefaultEndpoint(struct tag_USB_DEVICE    * UsbDevice);
USBH_STATUS UbdDefaultEpSubmitUrb (struct tag_USB_DEVICE    * Dev,       URB * Urb);
USBH_STATUS UbdSubmitSetInterface (struct tag_USB_INTERFACE * UsbInterface, U16 Interface, U16 AlternateSetting, USBH_ON_COMPLETION_FUNC * Completion, URB * OriginalUrb);
USBH_STATUS UbdSubmitClearFeatureEndpointStall(DEFAULT_EP   * DefaultEp, URB * Urb, U8 Endpoint, USBH_ON_COMPLETION_FUNC * InternalCompletion, void * HcContext);
void UbdReleaseDefaultEndpoint                (DEFAULT_EP   * UsbEndpoint);
void UbdDefaultEpUrbCompletion                (URB          * Urb);

#define USB_DEVICE_MAGIC FOUR_CHAR_ULONG('U','D','E','V')

// Dump the info of an allocated device /DBG_ADDREMOVE must be set
#define DUMP_USB_DEVICE_INFO(UsbDev)                                                               \
USBH_LOG((USBH_MTYPE_DEVICE, "Device: Added Dev: USB addr: %d Id:%u speed: %s parent port: %d %s", \
      (UsbDev)->DeviceID,                                                                          \
      (int)(UsbDev)->UsbAddress,                                                                   \
      UbdPortSpeedStr((UsbDev)->DeviceSpeed),                                                      \
      (UsbDev)->ParentPort != NULL ? (int)(UsbDev)->ParentPort->HubPortNumber: -1,                 \
      ((UsbDev)->DevStatus & USB_STATUS_SELF_POWERED) ? "self powered" : "bus powered"))

// Needs the struct and the name of the list entry inside the struct
#define GET_USB_DEVICE_FROM_ENTRY(     pListEntry) STRUCT_BASE_POINTER(pListEntry, USB_DEVICE, ListEntry)
#define GET_USB_DEVICE_FROM_TEMP_ENTRY(pListEntry) STRUCT_BASE_POINTER(pListEntry, USB_DEVICE, TempEntry)
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
    UbdDeleteDevice(devPtr);                                                                  \
  }
#else
#define DEC_REF(devPtr)          \
  (devPtr)->RefCount--;          \
  if ((devPtr)->RefCount == 0) { \
    UbdDeleteDevice(devPtr);     \
  }
#endif

USB_DEVICE * UbdNewUsbDevice(struct tag_HOST_CONTROLLER * HostController);

void         UbdStartEnumeration                      (USB_DEVICE * Dev, POST_ENUM_FUNCTION * PostEnumFunction, void * Context);
void         UbdDeleteDevice                          (USB_DEVICE * Dev);
void         UbdDeleteInterfaces                      (USB_DEVICE * Dev);
void         UbdUdevMarkDeviceAsRemoved               (USB_DEVICE * Dev);
void         UbdUdevMarkParentAndChildDevicesAsRemoved(USB_DEVICE * Dev);
void         UbdProcessSetConf                        (USB_DEVICE * Dev);
U8         * UbdGetNextInterfaceDesc                  (USB_DEVICE * Dev, U8  * Start, U8 InterfaceNumber, unsigned int AlternateSetting);
U8         * UbdGetNextEndpointDesc                   (USB_DEVICE * Dev, U8  * Start, U8 Endpoint);
void         UbdCreateInterfaces                      (void * Context);
U16          UbdGetUshortFromDesc                     (void * Buffer,    U16   Offset);
U8           UbdGetUcharFromDesc                      (void * Buffer,    U16   Offset);

/*********************************************************************
*
*       Helper functions
*
**********************************************************************
*/

void         UbdEnumTimerFunction      (void       * Context);
void         UbdSetConfTimerFunction   (void       * Context);
void         UbdProcessEnum            (void       * usbDevice);
unsigned int UbdDevGetPendingUrbCount  (USB_DEVICE * Dev);
int          UbdCheckCtrlTransferBuffer(USB_DEVICE * Dev, U16                   RequestLength);
void         UbdEnumParentPortRestart  (USB_DEVICE * Dev, USBH_STATUS           status);
void         UbdProcessEnumPortError   (USB_DEVICE * dev, USBH_STATUS           enumStatus);
USBH_STATUS  UbdSearchUsbInterface     (USB_DEVICE * dev, USBH_INTERFACE_MASK * iMask, struct tag_USB_INTERFACE * * iface);

USBH_STATUS  UbdGetEndpointDescriptorFromInterface(struct tag_USB_INTERFACE * usbInterface, U8 alternateSetting, const USBH_EP_MASK * mask, U8 * * descriptor);

USB_ENDPOINT * UbdNewEndpoint(struct tag_USB_INTERFACE * UsbInterface, U8 * EndpointDescriptor);
void           UbdDeleteEndpoint        (USB_ENDPOINT  * UsbEndpoint);
USBH_STATUS    UbdEpSubmitUrb           (USB_ENDPOINT  * UsbEndpoint, URB * Urb);
void           UbdEpUrbCompletion       (URB           * Urb);

// Needs the struct and the name of the list entry inside the struct
#define GET_HOST_CONTROLLER_FROM_ENTRY(pListEntry) STRUCT_BASE_POINTER(pListEntry, HOST_CONTROLLER, ListEntry)
#define GET_USB_ENDPOINT_FROM_ENTRY(   pListEntry) STRUCT_BASE_POINTER(pListEntry, USB_ENDPOINT,    ListEntry)
#define GET_USB_INTERFACE_FROM_ENTRY(  pListEntry) STRUCT_BASE_POINTER(pListEntry, USB_INTERFACE,   ListEntry)
#define GET_EP_FROM_ADDRESS(uif,ep)     uif->EpMap[((ep) & 0xf) | (((ep) & 0x80) >> 3)]
#define SET_EP_FOR_ADDRESS( uif,ep,uep) uif->EpMap[((ep) & 0xf) | (((ep) & 0x80) >> 3)] = (uep)

USB_INTERFACE * UbdNewUsbInterface    (USB_DEVICE    * Device);
void            UbdDeleteUsbInterface (USB_INTERFACE * UsbInterface);
USBH_STATUS     UbdCreateEndpoints    (USB_INTERFACE * UsbInterface);
void            UbdRemoveEndpoints    (USB_INTERFACE * UsbInterface);
void            UbdAddUsbEndpoint     (USB_ENDPOINT  * UsbEndpoint);
void            USBH_RemoveUsbEndpoint(USB_ENDPOINT  * UsbEndpoint);
USBH_STATUS     UbdCompareUsbInterface(USB_INTERFACE * Interface, USBH_INTERFACE_MASK * InterfaceMask, T_BOOL EnableHubInterfaces);
unsigned int    UbdGetPendingUrbCount (USB_INTERFACE * Interface);
void            UbdAddUsbInterface    (USB_INTERFACE * UsbInterface);
void            UbdRemoveUsbInterface (USB_INTERFACE * UsbInterface);

USB_ENDPOINT  * UbdSearchUsbEndpointInInterface(USB_INTERFACE * Interface, const USBH_EP_MASK * mask);

#define USB_DRIVER_MAGIC FOUR_CHAR_ULONG('U','D','R','V')

typedef struct tag_USB_DRIVER { // The global driver object
#if (USBH_DEBUG > 1)
  U32               Magic;
#endif
  DLIST             HostControllerList;
  U32               HostControllerCount;
  // Registered PNP notifications
  DLIST             NotificationList;
  U32               NotificationCount;
  // Delayed Pnp notifications, called in an timer routine
  DLIST             DelayedPnPNotificationList;
  U32               DelayedPnPNotificationCount;
  USBH_TIMER_HANDLE DelayedPnPNotifyTimer;
  DLIST             EnumErrorNotificationList;
  U32               EnumErrorNotificationCount;
  // Next free ID's for a new enumerated device
  USBH_INTERFACE_ID NextInterfaceID;
  USBH_DEVICE_ID    NextDeviceID;
} USB_DRIVER;

typedef struct tag_USBH_MALLOC_HEADER { // Helper struct to allocate aligned memory
  U8 * MemBlock;
} USBH_MALLOC_HEADER;

void UbdAddHostController            (struct tag_HOST_CONTROLLER * HostController);
void UbdRemoveHostController         (struct tag_HOST_CONTROLLER * HostController);
void UbdProcessDevicePnpNotifications(USB_DEVICE * Device, USBH_PNP_EVENT event);
void UbdAddNotification              (USB_DEVICE * Device);
void UbdRemoveNotification           (USB_DEVICE * Device);

USBH_INTERFACE_ID   UbdGetNextInterfaceID(void);
USBH_DEVICE_ID      UbdGetNextDeviceID   (void);
USB_DEVICE        * UbdGetDeviceByID     (USBH_DEVICE_ID    DeviceID);
USB_INTERFACE     * GetInterfaceByID     (USBH_INTERFACE_ID InterfaceID);

#define ROOT_HUB_MAGIC FOUR_CHAR_ULONG('R','H','U','B')

typedef enum tag_RH_PORTRESET_STATE {
  RH_PORTRESET_IDLE, // Only this state allows an new root hub port reset
  RH_PORTRESET_REMOVED,
  RH_PORTRESET_INIT, // RH_PORTRESET_INIT prevents starting of root hub enumeration until power good time is elapsed!
  RH_PORTRESET_START,
  RH_PORTRESET_RESTART,
  // Following states are always entered in the context of a callback completion routine never by direct calling of RootHubProcessDeviceReset()
  RH_PORTRESET_WAIT_RESTART,
  RH_PORTRESET_RES,
  RH_PORTRESET_WAIT_RESET,
  RH_PORTRESET_SET_ADDRESS,
  RH_PORTRESET_WAIT_ADDRESS
} RH_PORTRESET_STATE;

typedef struct tag_ROOT_HUB {                       // The global driver object
#if (USBH_DEBUG > 1)
  U32                          Magic;
#endif
  struct tag_HOST_CONTROLLER * HostController;      // Backward pointer to the host controller
  unsigned int                 PowerGoodTime;       // Power on to power good time in ms
  unsigned int                 PortCount;           // Number of ports
  DLIST                        PortList;
  URB_SUB_STATE                SubState;            // Sub state machine for device reset and set address,  easier handling if both an timer and URB is started!
  URB_SUB_STATE                InitHubPortSubState;
  RH_PORTRESET_STATE           PortResetEnumState;
  struct tag_HUB_PORT        * EnumPort;
  USB_DEVICE                 * EnumDevice;
  URB                          EnumUrb;             // Embedded URB
  USBH_HC_EP_HANDLE            EnumEpHandle;
} ROOT_HUB;

USBH_STATUS           UbdInitRootHub                      (struct tag_HOST_CONTROLLER * HostController);
void                  UbdRootHubNotification              (void * RootHubContext, U32 Notification);
void                  UbdReleaseRootHub                   (ROOT_HUB * RootHub);
USBH_STATUS           UbdRootHubAddPortsStartPowerGoodTime(ROOT_HUB * RootHub);
T_BOOL                UbdServiceRootHubPorts              (ROOT_HUB * RootHub);
struct tag_HUB_PORT * UbdGetRootHubPortByNumber           (ROOT_HUB * RootHub, U8 port);

#define DEFAULT_NOTIFY_RETRY_TIMEOUT      100
#define USB_HUB_MAGIC                     FOUR_CHAR_ULONG('U','H','U','B')
#define USBHUB_DEFAULT_ALTERNATE_SETTING  0
#define USBHUB_DEFAULT_INTERFACE          0
#define HUB_PORT_MAGIC                    FOUR_CHAR_ULONG('P','O','R','T')

typedef enum tag_PORT_STATE {
  PORT_UNKNOWN,
  PORT_REMOVED,               // Set from notification
  PORT_CONNECTED,             // Set from notification
  PORT_RESTART,               // Set from notification or enumeration (both functions are synchronized)
  PORT_SUSPEND,               // Set from notification
  PORT_RESET,                 // Set from enumeration
  PORT_ENABLED,               // Set from enumeration
  PORT_ERROR                  // Errors during port enumeration
} PORT_STATE;

typedef struct tag_HUB_PORT {              // Global driver object
#if (USBH_DEBUG > 1)
  U32                  Magic;
#endif
  DLIST                ListEntry;          // Entry for hub or root hub
  ROOT_HUB           * RootHub;            // Null if no root hub port
  struct tag_USB_HUB * ExtHub;             // Null if no external hub
  U32                  PortStatus;         // A copy of the port status returned from the HUB
  U32                  PortStatusShadow;   // Shadow register
  USBH_SPEED           PortSpeed;          // The current speed of the device
  PORT_STATE           PortState;          // The current port state of the port
  USB_DEVICE         * Device;             // Device connected to this port, for tree operation
  U8                   HubPortNumber;      // The one based index of the hub port
  unsigned int         RetryCounter;       // Counts the number of retries
  U8                   ConfigurationIndex; // This is the configuration index for the device
  T_BOOL               HighPowerFlag;      // True if the port is an high powered port min.500ma
} HUB_PORT;

typedef enum tag_HUB_PORTRESET_STATE { // Device reset
  HUB_PORTRESET_IDLE,
  HUB_PORTRESET_REMOVED,               // Port or Hub is not connected
  HUB_PORTRESET_START,
  HUB_PORTRESET_RESTART,
  HUB_PORTRESET_WAIT_RESTART,
  HUB_PORTRESET_RES,
  HUB_PORTRESET_IS_ENABLED,
  HUB_PORTRESET_WAIT_RESET,
  HUB_PORTRESET_SET_ADDRESS,
  HUB_PORTRESET_WAIT_SET_ADDRESS,
  HUB_PORTRESET_START_DEVICE_ENUM,
  HUB_PORTRESET_DISABLE_PORT
} HUB_PORTRESET_STATE;

typedef enum tag_HUB_ENUM_STATE { // Hub initialization state machine
  HUB_ENUM_IDLE,                  // Idle
  HUB_ENUM_START,                 // Start the state machine
  HUB_ENUM_GET_STATUS,            // Get the device sdtauts
  HUB_ENUM_HUB_DESC,              // Check the hub descriptor
  HUB_ENUM_SET_POWER,             // Set power for all ports
  HUB_ENUM_POWER_GOOD,            // Power good time elapsed
  HUB_ENUM_PORT_STATE,            // Get all port status, set the port state
  HUB_ENUM_ADD_DEVICE,            // Add the hub device to the hosts device list
  HUB_ENUM_REMOVED                // Active if the parent port is removed
} HUB_ENUM_STATE;

// This states are used in conjunction with the NotifySubState
typedef enum tag_HUB_NOTIFY_STATE {
  HUB_NOTIFY_IDLE,               // Idle
  HUB_NOTIFY_START,              // Start state
  HUB_NOTIFY_GET_HUB_STATUS,     // Start the state machine
  HUB_NOTIFY_CLEAR_HUB_STATUS,
  HUB_NOTIFY_GET_PORT_STATUS,    // Set power for all ports
  HUB_NOTIFY_CLR_PORT_STATUS,    // Power good time elapsed
  HUB_NOTIFY_CHECK_OVER_CURRENT,
  HUB_NOTIFY_CHECK_CONNECT,
  HUB_NOTIFY_CHECK_REMOVE,
  HUB_NOTIFY_DISABLE_PORT,       // Disable an port
  HUB_NOTIFY_REMOVED,            // Hub device state is not WORKING
  HUB_NOTIFY_ERROR               // Error submitting of an URB to the hub device after max. retries
} HUB_NOTIFY_STATE;

typedef struct tag_USB_HUB { // USB HUB object
#if USBH_DEBUG > 1
  U32                   Magic;
#endif
  USB_DEVICE          * HubDevice;                   // Backward pointer to the USB hub device
  unsigned int          PowerGoodTime;               // Power on to power good time in ms
  unsigned int          Characteristics;             // Power on to power good time in ms
  unsigned int          PortCount;                   // Number of ports
  DLIST                 PortList;                    // List of ports
  T_BOOL                SubmitFlag;                  // Helper var. in ProcessHubNotification

  // Hub notification
  // This urb contains hub notification information and is used for all hub requests in ProcessHubNotification()
  URB                   NotifyUrb;
  HUB_NOTIFY_STATE      NotifyState;
  HUB_NOTIFY_STATE      OldNotifyState;
  URB_SUB_STATE         NotifySubState;              // hub notify sub state machine (submitting and aborting of URBs)

  // Current not processed ports in ProcessHubNotification()
  unsigned int          NotifyPortCt;
  struct tag_HUB_PORT * NotifyPort;
  U32                   NotifyTemp;                  // Temporary variable
  U32                   Notification;                // Received Notifcation max. 4 bytes
  U32                   Status;                      // todo: read hub status after hub status
  struct tag_HUB_PORT * EnumPort;                    // Hub device enumeration
  USB_DEVICE          * EnumDevice;
  URB                   EnumUrb;
  HUB_ENUM_STATE        EnumState;                   // State of the Hubs initialization process
  URB_SUB_STATE         EnumSubState;                // helper sub state for hub enumeration

  // Hold post function and context! Used in the state HUB_ENUM_ADD_DEVICE in ProcessEnumHub()
  POST_ENUM_FUNCTION  * PostEnumFunction;
  void                * PostEnumContext;
  HUB_PORTRESET_STATE   PortResetEnumState;          // Hub port reset state machine / Current HubProcessPortResetSetAddress() state

  // Helper sub state machines
  URB_SUB_STATE         PortResetSubState;
  URB_SUB_STATE         PortResetControlUrbSubState;
  USBH_HC_EP_HANDLE     PortResetEp0Handle;

  // To get hub and port notifications
  USB_ENDPOINT        * InterruptEp;
  URB                   interruptUrb;
  void                * InterruptTransferBuffer;
  unsigned int          InterruptTransferBufferSize;
} USB_HUB;

void                 UbdDeleteHub(struct tag_USB_HUB * hub);
void                 UbdStartHub (struct tag_USB_HUB * Hub, POST_ENUM_FUNCTION * PostEnumFunction, void * Context);
struct tag_USB_HUB * UbdAllocInitUsbHub (USB_DEVICE  * dev);

/* Called if the root hub does not need any service */
void UbdServiceAllHubs         (struct tag_HOST_CONTROLLER * hc);
void UbdHubPrepareClrFeatureReq(URB                        * urb, U16 feature, U16 selector);

#define HUB_PORT_PTR(entry) STRUCT_BASE_POINTER(entry, HUB_PORT, ListEntry);

HUB_PORT * UbdNewHubPort   (void); // Return null on error
void       UbdDeleteHubPort(HUB_PORT * HubPort);
void       UbdSetPortState (HUB_PORT * hubPort, PORT_STATE state);

/*********************************************************************
*
*       UbdHubBuildChildDeviceList
*
*  Function Description:
*    Builds a device list of all devices that are connected to a parent
*    device inclusive the parent device. The first device in the list is
*    the parent device. The list ends if no hub device on an port is found! 
*
*  Parameters:
*    hubDevice: Parent device
*    devList:   Pointer to a temporary list
*
*  Return value:
*    Number of devices in the list inclusive the rootHubDevice!
*    0: rootHubDevice is no hub device!
*/
int UbdHubBuildChildDeviceList(USB_DEVICE * hubDevice, DLIST * devList);

// Returns the hub port by port number.
// Attention: The state of the hub device is not checked!
struct tag_HUB_PORT * HubGetPortByNumber(USB_HUB * Hub, unsigned char Number);

#define HOST_CONTROLLER_MAGIC FOUR_CHAR_ULONG('H','O','S','T')

typedef enum tag_HOST_CONTROLLER_STATE {
  HC_UNKNOWN,
  HC_REMOVED,
  HC_WORKING,
  HC_SUSPEND
} HOST_CONTROLLER_STATE;

typedef struct tag_HOST_CONTROLLER {            // Global driver object
  DLIST                   ListEntry;            // List entry for USB driver
  long                    RefCount;             // Ref Count
  HOST_CONTROLLER_STATE   State;                // The state of the HC
  USB_DRIVER            * Driver;               // Backward pointer
  DLIST                   DeviceList;           // List of USB devices
  U32                     DeviceCount;
  USB_HOST_ENTRY          HostEntry;            // Host controller entry
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
  HUB_PORT              * ActivePortReset;
} HOST_CONTROLLER;

#define HC_INC_REF(devPtr) (devPtr)->RefCount++                                                    // Reference counting macros to the USB_DEVICE object

#if (USBH_DEBUG > 1)
#define HC_DEC_REF(devPtr)                                                                         \
    (devPtr)->RefCount--;                                                                          \
    if ((devPtr)->RefCount == 1) {                                                                 \
      USBH_LOG((USBH_MTYPE_CORE, "HC_DEC_REF RefCount is 1 %s(%d)\n", __FILE__, __LINE__));        \
    }                                                                                              \
    if ((devPtr)->RefCount <  0) {                                                                 \
      USBH_LOG((USBH_MTYPE_CORE, "HC_DEC_REF RefCount less than 0 %s(%d)\n", __FILE__, __LINE__)); \
    }                                                                                              \
    if ((devPtr)->RefCount == 0) {                                                                 \
      USBH_LOG((USBH_MTYPE_CORE, "HC_DEC_REF RefCount is 0 %s(%d)\n", __FILE__, __LINE__));        \
      UbdDeleteHostController(devPtr);                                                             \
    }
#else
  #define HC_DEC_REF(devPtr)           \
    (devPtr)->RefCount--;              \
    if ((devPtr)->RefCount == 0) {     \
      UbdDeleteHostController(devPtr); \
    }
#endif

HOST_CONTROLLER * UbdCreateHostController      (USB_HOST_ENTRY  * HostEntry);
void              UbdFreeUsbAddress            (HOST_CONTROLLER * HostController, U8 Address);
void              UbdHcServicePorts            (HOST_CONTROLLER * HostController);
U8                UbdGetUsbAddress             (HOST_CONTROLLER * HostController);
void              UbdDeleteHostController      (HOST_CONTROLLER * Host);
void              UbdAddUsbDevice              (USB_DEVICE      * Device);
void              UbdHcRemoveDeviceFromList    (USB_DEVICE      * Device);
void              UbdDefaultReleaseEpCompletion(void            * Context);
void              USBH_OHC_Add                 (void            * pBase);
void              USBH_Task                    (void);
void              USBH_ISRTask                 (void);
void              USBH_ServiceISR              (unsigned          Index);
void              USBH_EnumTask                (void);
void              USBH_ConfigTransferBufferSize(U32               Size);

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

typedef struct {
    U8 data;  
} USBH_HID_SCANGUN_DATA;

typedef struct {
  U16 KeyCode;
  char ch;
} SCANCODE_TO_CH;

typedef void (USBH_HID_ON_KEYBOARD_FUNC) (USBH_HID_KEYBOARD_DATA * pKeyData);
typedef void (USBH_HID_ON_MOUSE_FUNC)    (USBH_HID_MOUSE_DATA    * pMouseData);
typedef void (USBH_HID_ON_SCANGUN_FUNC)    (USBH_HID_SCANGUN_DATA    * pScanGunData);


void   USBH_HID_Exit(void);
T_BOOL USBH_HID_Init(void);
void   USBH_HID_SetOnMouseStateChange   (USBH_HID_ON_MOUSE_FUNC    * pfOnChange);
void   USBH_HID_SetOnKeyboardStateChange(USBH_HID_ON_KEYBOARD_FUNC * pfOnChange);


/*********************************************************************
*
*       USBH_MSD
*/
// Function parameter for the user callback function USBH_MSD_LUN_NOTIFICATION_FUNC
typedef enum {
  USBH_MSD_EVENT_ADD,
  USBH_MSD_EVENT_REMOVE
} USBH_MSD_EVENT;

// Contains logical unit information
typedef struct tag_USBH_MSD_UNIT_INFO {
  U32 TotalSectors;     // Total number of sectors on the medium
  U16 BytesPerSector;   // Number of bytes per sector
  int WriteProtectFlag; // Unequal zero if the device is write protected
} USBH_MSD_UNIT_INFO;

// This callback function is called when new a logical units is added or removed
// To get detailed information USBH_MSD_GetLuns has to be called.
// The LUN indexes must be used to get access to an specified unit of the device.
typedef void USBH_MSD_LUN_NOTIFICATION_FUNC(void * pContext, U8 DevIndex, USBH_MSD_EVENT Event);

int         USBH_MSD_Init        (USBH_MSD_LUN_NOTIFICATION_FUNC * pfLunNotification, void * pContext);
void        USBH_MSD_Exit        (void);
int         USBH_MSD_ReadSectors (U8 Unit, U32 SectorNo, U32 NumSectors,       U8 * pBuffer);
int         USBH_MSD_WriteSectors(U8 Unit, U32 SectorNo, U32 NumSectors, const U8 * pBuffer);
USBH_STATUS USBH_MSD_GetUnitInfo (U8 Unit, USBH_MSD_UNIT_INFO * Info);
USBH_STATUS USBH_MSD_GetStatus   (U8 Unit);


#if defined(__cplusplus)
  }
#endif

#endif

/********************************* EOF ******************************/
