/*********************************************************************
*  
*   IAR PowerPac
*
*   (c) Copyright IAR Systems 2010.  All rights reserved.
*
**********************************************************************
----------------------------------------------------------------------
File    : USBH_ConfDefaults.h
Purpose :
--------------------------  END-OF-HEADER  ---------------------------
*/

#ifndef   USBH_CONFDEFAULTS_H
#define   USBH_CONFDEFAULTS_H

#include "USBH_Conf.h"

/*********************************************************************
*
*       Basic types
*/
#define     T_BOOL char

#ifndef     FALSE
  #define     FALSE (1 == 0)
#endif
#ifndef     TRUE
  #define     TRUE         (1 == 1)
#endif
#ifndef     USBH_DEBUG
  #define   USBH_DEBUG   (0)
#endif
#ifndef     USBH_MEMCPY
  #define   USBH_MEMCPY  memcpy
#endif
#ifndef     USBH_MEMSET
  #define   USBH_MEMSET  memset
#endif
#ifndef     USBH_MEMMOVE
  #define   USBH_MEMMOVE memmove
#endif
#ifndef     USBH_MEMCMP
  #define   USBH_MEMCMP  memcmp
#endif
#ifndef     USBH_OPTIMIZE
  #define   USBH_OPTIMIZE
#endif
#ifndef     USBH_IS_BIG_ENDIAN
  #define   USBH_IS_BIG_ENDIAN 0      // Little endian is default
#endif

#ifndef     USBH_PANIC
  #if       USBH_DEBUG
    #define USBH_PANIC(s) USBH_Panic(s)
  #else
    #define USBH_PANIC(s)
  #endif
#endif
#ifndef     USBH_SUPPORT_LOG
  #if       USBH_DEBUG > 1
    #define USBH_SUPPORT_LOG   1
  #else
    #define USBH_SUPPORT_LOG   0
  #endif
#endif
#ifndef     USBH_SUPPORT_WARN
  #if       USBH_DEBUG > 1
    #define USBH_SUPPORT_WARN  1
  #else
    #define USBH_SUPPORT_WARN  0
  #endif
#endif

#ifndef    OH_ISO_ENABLE
  #define  OH_ISO_ENABLE      0
#endif

#define USBH_TRANSFER_BUFFER_ALIGNMENT                   1
#define USBH_IS_VALID_TRANSFER_BUFFER_RANGE(    Buffer) (TRUE)
#define USBH_IS_VALID_TRANSFER_BUFFER_ALIGNMENT(Buffer) (FALSE)
#define USBH_IS_VALID_TRANSFER_MEM(             Buffer) (USBH_IS_VALID_TRANSFER_BUFFER_RANGE(Buffer) && USBH_IS_VALID_TRANSFER_BUFFER_ALIGNMENT(Buffer) ? TRUE : FALSE)

/*********************************************************************
*
*       ROOT HUB configuration
*/
#define HC_ROOTHUB_PORTS_ALWAYS_POWERED 0 // If set, ports are always powered on  when the Host Controller is powered on. The default value is 0.
// If this define is set each port is powered individually. If this define is not set all ports powered on at the same time.
// Not all host controller supports individually port switching. Because of this the default value is 0.
#define HC_ROOTHUB_PER_PORT_POWERED     0
// This define can set to 1 if the hardware on the USB port detects an overcurrent condition on the Vbus line.
// If this define is set to 1 and the port status has an overcurrent condition the port is automatically disabled.
#define HC_ROOTHUB_OVERCURRENT          1
// USB host controller driver endpoint resources. That are all endpoints that can be used at the same time. The number of control endpoint
// is calculated from the number of the USB devices and additional control endpoints that are needed for the USB device enumeration. The
// following defines determine indirect also additional bus master memory (physical none cached and none swapped memory).
#define HC_DEVICE_MAX_USB_DEVICES       2 // Number connected USB devices
#define HC_DEVICE_INTERRUPT_ENDPOINTS   2 // Number of interrupt endpoints
#define HC_DEVICE_BULK_ENDPOINTS        2 // Numbers of bulk endpoints
// USB host stack does not support isochronous transfer
#define HC_ISO_ENABLE                   0
#define HC_DEVICE_ISO_ENDPOINTS         0

#if HC_ISO_ENABLE && (HC_DEVICE_ISO_ENDPOINTS==0)
  #error error HC_DEVICE_ISO_ENDPOINTS
#endif

// Validates root hub port defines.
#if HC_ROOTHUB_PORTS_ALWAYS_POWERED && HC_ROOTHUB_PER_PORT_POWERED
  #error HC_ROOTHUB_PORTS_ALWAYS_POWERED or HC_ROOTHUB_PER_PORT_POWERED is allowed
#endif
#ifndef USBH_MSD_EP0_TIMEOUT
  #define USBH_MSD_EP0_TIMEOUT 5000            // Specifies the default timeout, in milliseconds, to be used for synchronous operations on the control endpoint.
#endif
#ifndef USBH_MSD_READ_TIMEOUT
  #define USBH_MSD_READ_TIMEOUT      3000 // Specifies the maximum time in milliseconds, for reading all bytes with the bulk read operation.
#endif
#ifndef USBH_MSD_WRITE_TIMEOUT 
  #define USBH_MSD_WRITE_TIMEOUT     1000 // Specifies the maximum time, in milliseconds, for writing all bytes with the bulk write operation.
#endif
// Must be a multiple of the maximum packet length for bulk data endpoints.
// That are 64 bytes for a USB 1.1 device and 512 bytes for a USB 2.0 high speed device.
#ifndef USBH_MSD_MAX_TRANSFER_SIZE
  #define USBH_MSD_MAX_TRANSFER_SIZE (32 * 1024) // [bytes]
#endif
#ifndef USBH_MSD_DEFAULT_SECTOR_SIZE
  #define USBH_MSD_DEFAULT_SECTOR_SIZE    512 // Specifies the default sector size in bytes to be used for reading and writing.
#endif
#ifndef USBH_MSD_MAX_DEVICES
  #define USBH_MSD_MAX_DEVICES            1   // Maximum number of USB Mass Storage devices that are supported from the library. A lower value saves memory.
#endif
#ifndef USBH_MSD_MAX_LUNS_PER_DEVICE
  #define USBH_MSD_MAX_LUNS_PER_DEVICE    1   // Maximum number of logical units per device that are supported from the library. A lower value saves memory.
#endif

#define USBH_EXTHUB_SUPPORT          1   // Set USBH_EXTHUB_SUPPORT to 1 if the UBD driver should support external hubs
#define DEFAULT_RESET_TIMEOUT        40  // The reset on the root hub is aborted after this time
// The host controller waits this time after reset before the Set Address command is sent. Some devices require some time
// before they can answer correctly. It is not required by the USB specification but Windows makes this gap with 80 ms.
#define WAIT_AFTER_RESET            80
// The bus driver waits this time before the next command is sent after Set Address. The device must answer to SetAddress on USB address 0 with the
// handshake and than set the new address. This is a potential racing condition if this step is performed in the firmware.
// Give the device this time to set the new address.
#define WAIT_AFTER_SETADDRESS       30
// If the device makes an error during USB reset and set address the enumeration process is repeated. The repeat count is defined by this define.
// Possible errors are OverCurrent, remove during reset, no answer to SetAddress.
#define RESET_RETRY_COUNTER         10
#define DELAY_FOR_REENUM            1000 // Describes the time before a USB reset is restarted if the last try has failed.
// The bus controller waits this time after a USB device connect event is detected
// before a USB bus reset is applied to the device
#define WAIT_AFTER_CONNECT          200
// The default size of the buffer to get descriptors from the device. If the buffer is too small for the configuration descriptor,
// a new buffer is dynamically allocated. The default size must be at least 256 bytes to read all possible string descriptors.
#define DEFAULT_TRANSFERBUFFER_SIZE 256
#define DEFAULT_SETUP_TIMEOUT       500  // Default timeout for all setup requests. After this time a not completed setup request is terminated.
// If the enumeration of the device (get descriptors, set configuration) fails, it is repeated after a time gap of this value
#define DEFAULT_RETRY_TIMEOUT       1000

// In order to avoid warnings for undefined parameters
#ifndef USBH_USE_PARA
  #if defined(NC30) || defined(NC308)
    #define USBH_USE_PARA(para)
  #else
    #define USBH_USE_PARA(para) para=para;
  #endif
#endif

#endif

/*************************** End of file ****************************/
