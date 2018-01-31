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

typedef struct {
  U8                ConfigCompleted;
  U8                InitCompleted;
  USBH_HC_HANDLE    hHC;
  USBH_HC_BD_HANDLE hHCBD;
  struct {
    U32 TransferBufferSize;
    U8  NumRootHubs;
  } Config;
} USBH_GLOBAL;

EXTERN USBH_GLOBAL USBH_Global;
EXTERN USB_DRIVER  gUsbDriver;
  
#define USBH_PRINT_STATUS_VALUE(Type, status) USBH_WARN((Type, "%s", USBH_GetStatusStr(status)))

#if (USBH_DEBUG > 1)
  #define T_ASSERT(condition)             if (!(condition)) { USBH_WARN((USBH_MTYPE_CORE, "\nASSERTION FAILED: %s(%d)\n", __FILE__, __LINE__)); }
  #define T_ASSERT_PTR(Ptr)               T_ASSERT(Ptr != NULL)
  #define T_ASSERT_MAGIC(ptr,type)        T_ASSERT(PTRVALID((ptr),type))
  #define T_ASSERT0                       USBH_WARN((USBH_MTYPE_CORE, "\nASSERT0: %s(%d)\n", __FILE__, __LINE__));
#else
  #define T_ASSERT(condition)
  #define T_ASSERT_PTR(Ptr)
  #define T_ASSERT_MAGIC(ptr, type)
  #define T_ASSERT0
#endif

#ifndef ULONG_MAX
  #define ULONG_MAX 0xffffffff
#endif

#define FOUR_CHAR_ULONG(c1,c2,c3,c4)   (((U32)(c1)) | (((U32)(c2))<<8) | (((U32)(c3))<<16) | (((U32)(c4))<<24)) // Generates a magic ulong (four char code)
#define TWO_CHAR_USHORT(c1,c2)         (((U16)(c1)) | ( (U16)(c2) <<8))                                         // Generates a magic ulong (four char code)
#define GET_MASK_FROM_BITNUMBER(BitNb) (((U32)(1))<<(BitNb))
// Calculate the pointer to the base of an object given its type and a pointer to a field within the object.
#define ZERO_MEMORY(        mem,count)     USBH_MEMSET((mem), 0,   (count))
#define ZERO_STRUCT(        s)             ZERO_MEMORY(&(s),sizeof(s))
#define ZERO_ARRAY(         s)             ZERO_MEMORY( (s),sizeof(s))
#define ARRAY_ELEMENTS(     a)             (sizeof(a)     / sizeof(a[0]))
#define ARRAY_LIMIT(        a)             (&a[ARRAY_ELEMENTS(a)])
// COMPILE_TIME_ASSERT() can be used to perform many compile-time assertions: type sizes, field offsets, etc.
// An assertion failure results in error C2118: negative subscript.
#define COMPILE_TIME_ASSERT(exp) typedef char __compile_time_ASSERT__[(exp)?1:-1]
#define SUCC(status) ((status) == 0) // Test status value for SUCCESS

#if (USBH_DEBUG > 1)
  #define PTRVALID(ptr,type) ((ptr)!=NULL && (ptr)->Magic==type##_MAGIC) // Takes a pointer and its type and compares the Magic field with a constant
#else
  #define PTRVALID(ptr,type)
#endif

// Helper macro, used to convert enum constants to string values
// lint -save -e773
#define TB_ENUMSTR(e) (x==(e)) ? #e
// lint -restore
#define TB_IS_ALIGNED(val,size) (((val) &  ((size)-1)) == 0)          // Returns true if the given value is aligned to a 'size' boundary
#define TB_ALIGN_UP(  val,size) (((val) +  ((size)-1)) & ~((size)-1)) // Round up a value to the next 'size' boundary
#define TB_ALIGN_DOWN(val,size) ( (val) & ~((size)-1))                // Round down a value to the next 'size' boundary

#if (USBH_DEBUG > 1)                                                  // Handy macro to enable code in debug builds only
  #define IFDBG(x) { x; }
#else
  #define IFDBG(x)
#endif

#ifdef _lint // Macro to mark function arguments unused, this will quieten lint
  //lint -emacro(111,UNUSED_PARAM)
  #define UNUSED_PARAM(p) (p) = (p);
#else
  #define UNUSED_PARAM(p) (p) = (p);
#endif

#define UNUSED_ALWAYS(p)   (p) = (p);
#define SWAP_USHORT(  x) (((x) >> 8)  | ( (x) << 8))
#define SWAP_ULONG(   x) (((x) >> 24) | (((x) >> 8) & 0xFF00) | (((x) << 8) && 0xff0000) | ((x) << 24))
#define SCSI_6BYTE_COMMAND_LENGTH   6
#define SCSI_10BYTE_COMMAND_LENGTH 10
// Attention: all SCSI commands are in big endian byte order !!!
// Commands implemented by all SCSI device servers:
// SC_INQUIRY
// SC_REQUEST_SENSE
// SC_SEND_DIAGNOSTIC
// SC_TEST_UNIT_READY
// SCSI commands used from RBS devices
#define SC_TEST_UNIT_READY                             (0x00)
#define SC_REQUEST_SENSE                               (0x03)
#define SC_INQUIRY                                     (0x12)
// Inquiry command parameter
#define STANDARD_INQUIRY_DATA_LENGTH                   (0x24)
#define INQUIRY_ENABLE_PRODUCT_DATA                    (0x1)
#define INQUIRY_ENABLE_COMMAND_SUPPORT                 (0x2)
// mode page related defines
#define SC_MODE_SENSE_6                                (0x1a)
#define SC_MODE_SENSE_10                               (0x5a)
// Command block offset for the page parameter
#define PAGE_CODE_OFFSET                               2
#define MODE_WRITE_PROTECT_OFFSET                      2
#define MODE_WRITE_PROTECT_MASK                        0x80
#define MODE_SENSE_PARAMETER_LENGTH                    0xc0
#define SC_MODE_PARAMETER_HEADER_LENGTH_6              4
#define SC_MODE_PARAMETER_HEADER_LENGTH_10             8
// Common for both headers
#define MODE_PARAMETER_HEADER_DATA_LENGTH_OFS          0
// 6 byte Sense mode header
#define MODE_PARAMETER_HEADER_MEDIUM_TYPE_OFS_6        1
#define MODE_PARAMETER_HEADER_DEVICE_PARAM_OFS_6       2
#define MODE_PARAMETER_HEADER_BLOCK_DESC_LENGTH_OFS_6  3
// 10 byte Sense mode header
#define MODE_PARAMETER_HEADER_MEDIUM_TYPE_OFS_10       2
#define MODE_PARAMETER_HEADER_DEVICE_PARAM_OFS_10      3
#define MODE_PARAMETER_HEADER_BLOCK_DESC_LENGTH_OFS_10 6

// Mode parameter struct, used to convert mode parameter(6) and mode parameter(10) header in this format
typedef struct _MODE_PARAMETER_HEADER {
  U16 DataLength; // Data length member of the received mode parameter header
  U8  MediumType;
  U8  DeviceParameter;
  U16 BlockDescriptorLength;
  U16 DataOffset; // Offset in data buffer where the mode pages parameter or the block descriptors (if available) begins
} MODE_PARAMETER_HEADER;

// Mode Sense/Select page constants.
#define MODE_PAGE_ERROR_RECOVERY        0x01
#define MODE_PAGE_DISCONNECT            0x02
#define MODE_PAGE_FORMAT_DEVICE         0x03
#define MODE_PAGE_RIGID_GEOMETRY        0x04
#define MODE_PAGE_FLEXIBILE             0x05 // disk
#define MODE_PAGE_WRITE_PARAMETERS      0x05 // cdrom
#define MODE_PAGE_VERIFY_ERROR          0x07
#define MODE_PAGE_CACHING               0x08
#define MODE_PAGE_PERIPHERAL            0x09
#define MODE_PAGE_CONTROL               0x0A
#define MODE_PAGE_MEDIUM_TYPES          0x0B
#define MODE_PAGE_NOTCH_PARTITION       0x0C
#define MODE_PAGE_CD_AUDIO_CONTROL      0x0E
#define MODE_PAGE_DATA_COMPRESS         0x0F
#define MODE_PAGE_DEVICE_CONFIG         0x10
#define MODE_PAGE_MEDIUM_PARTITION      0x11
#define MODE_PAGE_CDVD_FEATURE_SET      0x18
#define MODE_PAGE_POWER_CONDITION       0x1A
#define MODE_PAGE_FAULT_REPORTING       0x1C
#define MODE_PAGE_CDVD_INACTIVITY       0x1D // cdrom
#define MODE_PAGE_ELEMENT_ADDRESS       0x1D
#define MODE_PAGE_TRANSPORT_GEOMETRY    0x1E
#define MODE_PAGE_DEVICE_CAPABILITIES   0x1F
#define MODE_PAGE_CAPABILITIES          0x2A // cdrom
#define MODE_SENSE_RETURN_ALL_PAGES     0x3f
#define MODE_SENSE_CURRENT_VALUES       0x00
#define MODE_SENSE_CHANGEABLE_VALUES    0x40
#define MODE_SENSE_DEFAULT_VAULES       0x80
#define MODE_SENSE_SAVED_VALUES         0xc0
#define SC_START_STOP_UNIT                  (0x1b)
#define SC_SEND_DIAGNOSTIC                  (0x1d)
#define SC_READ_FORMAT_CAPACITY             (0x23)
#define SC_READ_FORMAT_CAPACITY_DATA_LENGTH (0xfe)
#define SC_READ_CAPACITY                    (0x25)
// Read capacity command parameter
#define READ_CAPACITY_CMD_LENGTH            (10)
#define SC_READ_10                          (0x28)
#define SC_WRITE_10                         (0x2a)

// Standard 6 byte command
typedef struct _SCSI_6BYTE_READ_WRITE_CMD {
  U8  Cmd;        // 0-command
  U8  MSBAddress; // 1-reserved Bits and MS bits
  U16 LSBAddress; // 2,3
  U8  Length;     // 4
  U8  Control;    // 5-always the last byte
} SCSI_6BYTE_READ_WRITE_CMD;

#define SC_6BYTE_CMD_MAX_SECTORS   0xff
#define SC_6BYTE_CMD_MAX_ADDRESS   0xffffff

typedef struct _SCSI_6BYTE_CMD {
  U8 Cmd; // 0-command
  U8 idx1;
  U8 idx2;
  U8 idx3;
  U8 Length;  // 4
  U8 Control; // 5-always the last byte
} SCSI_6BYTE_CMD;

typedef struct _SCSI_10BYTE_CMD {
  U8 Cmd;      // 0-command
  U8 Service;  // 1-reserved Bits and MS bits
  U32 Address; // 2,3,4,5
  U8 reserved; // 6
  U16 Length;  // 7,8
  U8 Control;  // 9-always the last byte
} SCSI_10BYTE_CMD;

#define SC_10BYTE_CMD_MAX_SECTORS   0xffff
#define SC_10BYTE_CMD_MAX_ADDRESS   0xffffffff
#define RD_CAPACITY_DATA_LENGTH     8

// Read capacity
typedef struct _RD_CAPACITY_DATA {
  U32 MaxBlockAddress;
  U32 BlockLength;
} RD_CAPACITY_DATA;

// Sense command parameter
#define SCS_DISABLE_BLOCK_DESC     (0x08)
#define SCS_CURRENT_PARAMETER      (0)
#define SCS_CHANGEABLE_PARAMETER   (1 << 6)
#define SCS_DEFAULT_PARAMETER      (2 << 6)
#define SCS_SAVED_PARAMETER        (3 << 6)
// Sense codes
#define SS_SENSE_NO_SENSE         0x00
#define SS_SENSE_RECOVERED_ERROR  0x01
#define SS_SENSE_NOT_READY        0x02
#define SS_SENSE_MEDIUM_ERROR     0x03
#define SS_SENSE_HARDWARE_ERROR   0x04
#define SS_SENSE_ILLEGAL_REQUEST  0x05
#define SS_SENSE_UNIT_ATTENTION   0x06
#define SS_SENSE_DATA_PROTECT     0x07
#define SS_SENSE_BLANK_CHECK      0x08
#define SS_SENSE_UNIQUE           0x09
#define SS_SENSE_COPY_ABORTED     0x0A
#define SS_SENSE_ABORTED_COMMAND  0x0B
#define SS_SENSE_EQUAL            0x0C
#define SS_SENSE_VOL_OVERFLOW     0x0D
#define SS_SENSE_MISCOMPARE       0x0E
#define SS_SENSE_RESERVED         0x0F
// Additional tape bit
#define SS_ILLEGAL_LENGTH         0x20
#define SS_EOM                    0x40
#define SS_FILE_MARK              0x80
// Additional Sense codes
#define SS_ADSENSE_NO_SENSE           0x00
#define SS_ADSENSE_LUN_NOT_READY      0x04
#define SS_ADSENSE_TRACK_ERROR        0x14
#define SS_ADSENSE_SEEK_ERROR         0x15
#define SS_ADSENSE_REC_DATA_NOECC     0x17
#define SS_ADSENSE_REC_DATA_ECC       0x18
#define SS_ADSENSE_ILLEGAL_COMMAND    0x20
#define SS_ADSENSE_ILLEGAL_BLOCK      0x21
#define SS_ADSENSE_INVALID_CDB        0x24
#define SS_ADSENSE_INVALID_LUN        0x25
#define SS_ADWRITE_PROTECT            0x27
#define SS_ADSENSE_MEDIUM_CHANGED     0x28
#define SS_ADSENSE_BUS_RESET          0x29
#define SS_ADSENSE_INVALID_MEDIA      0x30
#define SS_ADSENSE_NO_MEDIA_IN_DEVICE 0x3a
#define SS_ADSENSE_POSITION_ERROR     0x3b
#define SS_ADSENSE_FAILURE_PREDICTION_THRESHOLD_EXCEEDED 0x5d
#define SS_FAILURE_PREDICTION_THRESHOLD_EXCEEDED         SS_ADSENSE_FAILURE_PREDICTION_THRESHOLD_EXCEEDED
#define SS_ADSENSE_COPY_PROTECTION_FAILURE               0x6f
#define SS_ADSENSE_VENDOR_UNIQUE                         0x80
#define SS_ADSENSE_MUSIC_AREA                            0xA0
#define SS_ADSENSE_DATA_AREA                             0xA1
#define SS_ADSENSE_VOLUME_OVERFLOW                       0xA7
// SS_ADSENSE_LUN_NOT_READY (0x04) qualifiers
#define SS_SENSEQ_CAUSE_NOT_REPORTABLE         0x00
#define SS_SENSEQ_BECOMING_READY               0x01
#define SS_SENSEQ_INIT_COMMAND_REQUIRED        0x02
#define SS_SENSEQ_MANUAL_INTERVENTION_REQUIRED 0x03
#define SS_SENSEQ_FORMAT_IN_PROGRESS           0x04
#define SS_SENSEQ_OPERATION_IN_PROGRESS        0x07
// SS_ADSENSE_NO_SENSE (0x00) qualifiers
#define SS_SENSEQ_FILEMARK_DETECTED            0x01
#define SS_SENSEQ_END_OF_MEDIA_DETECTED        0x02
#define SS_SENSEQ_SETMARK_DETECTED             0x03
#define SS_SENSEQ_BEGINNING_OF_MEDIA_DETECTED  0x04
// SS_ADSENSE_ILLEGAL_BLOCK (0x21) qualifiers
#define SS_SENSEQ_ILLEGAL_ELEMENT_ADDR         0x01
// SS_ADSENSE_POSITION_ERROR (0x3b) qualifiers
#define SS_SENSEQ_DESTINATION_FULL             0x0d
#define SS_SENSEQ_SOURCE_EMPTY                 0x0e
// SS_ADSENSE_INVALID_MEDIA (0x30) qualifiers
#define SS_SENSEQ_INCOMPATIBLE_MEDIA_INSTALLED 0x00
#define SS_SENSEQ_UNKNOWN_FORMAT               0x01
#define SS_SENSEQ_INCOMPATIBLE_FORMAT          0x02
// SS_ADSENSE_COPY_PROTECTION_FAILURE (0x6f) qualifiers
#define SS_SENSEQ_AUTHENTICATION_FAILURE                          0x00
#define SS_SENSEQ_KEY_NOT_PRESENT                                 0x01
#define SS_SENSEQ_KEY_NOT_ESTABLISHED                             0x02
#define SS_SENSEQ_READ_OF_SCRAMBLED_SECTOR_WITHOUT_AUTHENTICATION 0x03
#define SS_SENSEQ_MEDIA_CODE_MISMATCHED_TO_LOGICAL_UNIT           0x04
#define SS_SENSEQ_LOGICAL_UNIT_RESET_COUNT_ERROR                  0x05
// length of standard Sense answer
#define STANDARD_SENSE_LENGTH   (18)

// 18 byte standard Sense data struct
typedef struct _STANDARD_SENSE_DATA {
  U8  ResponseCode;   //only 0x70 is supported
  U8  Obsolete;       //1
  U8  Sensekey;       //2
  U32 Info;           //3,4,5,6
  U8  AddLength;      //7
  U32 Cmdspecific;    //8,9,10,11
  U8  Sensecode;      //12
  U8  Sensequalifier; //13
  U8  Unitcode;       //14
  U8  Keyspecific1;   //15
  U8  Keyspecific2;   //16
  U8  Keyspecific3;   //17
} STANDARD_SENSE_DATA;

// START STOP UNIT command parameter
#define STARTSTOP_PWR_INDEX    (4)
// Do not change the power condition
#define STARTSTOP_PWR_NO_CHANGE (0)
// Power state defines
#define STARTSTOP_PWR_ACTIVE  (1)
#define STARTSTOP_PWR_IDLE    (2)
#define STARTSTOP_PWR_STANDBY (3)
#define STARTSTOP_PWR_SLEEP   (4)
// Make the device ready for use
#define STARTSTOP_PWR_START (0x01)
// Byte length of the returned inquiry data
#define STANDARD_INQUIRY_LENGTH (96)

typedef enum _INQUIRY_SELECT {
  Standard,
  Productpage,
  CommandSupport
} INQUIRY_SELECT;

// First four bytes of the inquiry response page
typedef struct _INQUIRY_STANDARD_RESPONSE {
  U8 DeviceType;
  U8 RMB;
  U8 Version;
  U8 ResponseFormat;
} INQUIRY_STANDARD_RESPONSE;

// Device type
// 00h     direct access device (e.g. UHD floppy disk)
// 01h     sequential access device (e.g. magnetic tape)
// 02-03h  reserved
// 04h     write once device (e.g. WORM optical disk)
// 05h     CD-ROM device
// 06h     reserved
// 07h     optical memory device (e.g. optical disks (not CD))
// 08h-1Eh reserved
// 1Fh     unknown or no device type

// Inquiry page device type
#define INQUIRY_DIRECT_DEVICE       0
#define INQUIRY_SEQ_DEVICE          1
#define INQUIRY_WRITE_ONCE          4
#define INQUIRY_CD_ROM              5
#define INQUIRY_NON_CD_OPTICAL      7
#define INQUIRY_DEVICE_TYPE_MASK    0x1f

// INQUIRY ANSI version
// 0h      The device might or might not comply to an ANSI approved standard.
// 1h      The device complies to ANSI X3.131-1986 (SCSI-1).
// 2h      The device complies to this version of SCSI. This code is reserved to designate this standard upon approval by ANSI.
// 3h - 7h Reserved

#define INQUIRY_VERSION_MASK         0x07
#define ANSI_VERSION_MIGHT_UFI       0
#define ANSI_VERSION_SCSI_1          1
#define ANSI_VERSION_SCSI_2          2
#define ANSI_VERSION_SCSI_3_SPC      3
#define ANSI_VERSION_SCSI_3_SPC_2    4
#define ANSI_VERSION_SCSI_3_SPC_3_4  5

#define INQUIRY_REMOVE_MEDIA_MASK        0x80
#define INQUIRY_RESPONSE_FORMAT_MASK     0x0f
#define INQUIRY_RESPONSE_SCSI_1          0
#define INQUIRY_RESPONSE_MIGTH_UFI       1
#define INQUIRY_RESPONSE_IN_THIS_VERISON 2

#define USBH_MSD_MAX_UNITS (USBH_MSD_MAX_LUNS_PER_DEVICE * USBH_MSD_MAX_DEVICES)

typedef void * USBH_MSD_HANDLE;      // Device identifier (device handle)

typedef struct _USBH_MSD_UNIT {                   // USBH_MSD_UNIT describes a logical unit of a device
  U32                       Magic;
  struct _USBH_MSD_DEVICE * pDev;              // Pointer to the device, if NULL then the unit is invalid
  U8                        Lun;              // Used to address the device in the transport layer
  STANDARD_SENSE_DATA       Sense;            // Store the last Sense code from the device
  U16                       BytesPerSector;   // Size of a sector (logical block) in bytes, if zero the field is invalid
  U32                       MaxSectorAddress;
  INQUIRY_STANDARD_RESPONSE InquiryData;
  MODE_PARAMETER_HEADER     ModeParamHeader;
} USBH_MSD_UNIT;

#define USBH_MSD_UNIT_MAGIC   FOUR_CHAR_ULONG('U','N','I','T')
#define USBH_MSD_DEVICE_MAGIC FOUR_CHAR_ULONG('M','D','E','V')

/*********************************************************************
*
*       Protocol layer interface
*
**********************************************************************
*/

typedef USBH_STATUS USBH_MSD_PL_READ_SECTORS        (USBH_MSD_UNIT * unit, U32   SectorAddress,       U8  * Buffer, U16 NumSectors); // returns: 0 for success, other values for errors
typedef USBH_STATUS USBH_MSD_PL_WRITE_SECTORS       (USBH_MSD_UNIT * unit, U32   SectorAddress, const U8  * Buffer, U16 NumSectors); // returns: 0 for success, other values for errors
typedef USBH_STATUS USBH_MSD_PL_READ_CAPACITY       (USBH_MSD_UNIT * Unit, U32 * MaxSectorAddress,    U16 * BytesPerSector);
typedef USBH_STATUS USBH_MSD_PL_READ_FORMAT_CAPACITY(USBH_MSD_UNIT * Unit);

// Length:  length of data buffer, should be STANDARD_INQUIRY_LENGTH
// Select:  Type selection of the returned data format
// CmdPage: specify the page number (unused if Select == Standard)
typedef USBH_STATUS USBH_MSD_PL_INQUIRY(USBH_MSD_UNIT * Unit, U8 * pData, U8  * Length, INQUIRY_SELECT Select, U8 CmdPage);

// Protocol layer function that executes a TEST UNIT READY command.
// Returns: 0 for success (device is ready), other values for errors
typedef USBH_STATUS USBH_MSD_PL_DEVICE_READY(USBH_MSD_UNIT * Unit);

// Protocol layer function that sends a START STOP UNIT command to the device. Some direct access block devices may be switched
// from being ready to being not ready by using the SC_START_STOP_UNIT command. An application may need to issue a SC_START_STOP_UNIT
// command with the START bit set to activate the ready state of a block device. This SCSI command is optional.
// Parameters:
//   Unit:             logical unit number
//   Start:            TRUE to start the device
// Returns:
//   0 for success, other values for errors
typedef USBH_STATUS USBH_MSD_PL_START_DEVICE(USBH_MSD_UNIT * Unit,T_BOOL  Start);

// Protocol layer function that initializes the USB Mass Storage device after power off/on or after a USB bus reset.
// USBH_MSD_PL_INIT inquiries the device specification, queries the sector size and the maximum sector address.
// The capacity for each logical unit of the device will be determined. If the device supports mode pages then these pages are checked.
// Parameters:
//   Dev: Pointer to the device, if NULL then the unit is invalid
// Returns:
//   0 for success, other values for errors
typedef USBH_STATUS USBH_MSD_PL_INIT(struct _USBH_MSD_DEVICE * Dev); 

// Protocol layer function that returns some parameters of the device.
// Parameters:
//   Unit        IN:  pointer to a valid unit
//   data:       IN:  valid data buffer;
//               OUT: filled buffer with page parameter data inclusive header
//   Length:     IN:  length of the data buffer;
//               OUT: number of bytes written to data buffer
//   Header:     IN:
//               OUT: converted values of the mode parameter header located at the beginning of the data buffer
//   Page:            page code, (a page code of 3Fh indicates that all mode pages implemented by the ATAPI block
//                    device shall be returned to the host computer)
//   PageControlCode: determines the parameter returned from the device
typedef USBH_STATUS USBH_MSD_PL_MODE_SENSE(USBH_MSD_UNIT * Unit, U8 * data, U8 * Length, MODE_PARAMETER_HEADER * Header, U8 Page, U8 PageControlCode);

// Issues a REQUEST SENSE command to receive the Sense data for the last requested command.
// If the application client issues a command other than REQUEST SENSE, the Sense data for the last command is lost.
// Parameters:
//   Unit: IN: valid unit; OUT: unit with Sense data
// Returns:
//   0 for success (Sense data is copied to Unit->Sense), other values for errors
typedef USBH_STATUS USBH_MSD_PL_REQUEST_SENSE(USBH_MSD_UNIT * Unit);

typedef struct {
  USBH_MSD_PL_INIT                 * PlInitSequence;
  USBH_MSD_PL_INQUIRY              * PlInquiry;
  USBH_MSD_PL_READ_FORMAT_CAPACITY * PlReadFormatCapacity;
  USBH_MSD_PL_READ_CAPACITY        * PlCapacity;
  USBH_MSD_PL_READ_SECTORS         * PlReadSectors;
  USBH_MSD_PL_WRITE_SECTORS        * PlWriteSectors;
  USBH_MSD_PL_DEVICE_READY         * PlReady;
  USBH_MSD_PL_START_DEVICE         * PlStartStop;
  USBH_MSD_PL_MODE_SENSE           * PlModeSense;
  USBH_MSD_PL_REQUEST_SENSE        * PlRequestSense;
} USBH_MSD_PHYS_LAYER_API;

extern const USBH_MSD_PHYS_LAYER_API USBH_MSD_PhysLayerSC06;
extern const USBH_MSD_PHYS_LAYER_API USBH_MSD_PhysLayerSC05;


/*********************************************************************
*
*       Transport layer interface
*
**********************************************************************
*/
  
// Transport layer function that resets the USB Mass Storage device with a command depending on the current used transport layer.
// Return value:
//   0 for success, other values are errors
typedef USBH_STATUS USBH_MSD_TL_RESET_INTERFACE(struct _USBH_MSD_DEVICE * dev);

// Transport layer function that returns the maximum logical unit number supported by the device.
// Return value: The maximum logical unit number (LUN) begins with zero.
typedef USBH_STATUS USBH_MSD_TL_GETMAX_LUN_INTERFACE(struct _USBH_MSD_DEVICE * dev, int * maxLunIndex);

// Transport layer function that writes data to the device and waits for completion.
// Parameters:
//   CmdBuffer:  Command buffer, must contain a valid device command
//   CmdLength:  Size of the command buffer, valid values are 1-16
//   DataBuffer: Transfer buffer
//   DataLenght: IN: length of DataBuffer; OUT: transferred bytes
// Return value:
//   0 for success, other values for errors
typedef USBH_STATUS USBH_MSD_TL_WRITE_INTERFACE(USBH_MSD_UNIT * unit, U8 * CmdBuffer, U8 CmdLength, const U8 * DataBuffer, U32 * DataLength, U16 Timeout, T_BOOL SectorDataFlag);

// Transport layer function that reads data from the device and waits for completion.
// Parameters:
//   CmdBuffer:  Command buffer, must contain a valid device command
//   CmdLength:  Size of the command buffer
//   DataBuffer: Transfer buffer
//   DataLenght: IN: length of DataBuffer; OUT: transferred bytes
// Returns value:
//   0 for success, other values for errors
typedef USBH_STATUS USBH_MSD_TL_READ_INTERFACE(USBH_MSD_UNIT * unit, U8 * CmdBuffer, U8 CmdLength, U8 * DataBuffer, U32 * DataLength, U16 Timeout, T_BOOL SectorDataFlag);


/*********************************************************************
*
*       Device object
*
**********************************************************************
*/
// USB Mass storage device.
typedef struct _USBH_MSD_DEVICE {
  U32                             Magic;
  int                             RefCnt;                          // Reference counter, see also INC_REF_CT, DEC_REF_CT
  struct _USBH_MSD_DRV          * pDriver;                         // Backward pointer to the USBH_MSD driver object
  T_BOOL                          IsValid;                         // True if this device object valid
  T_BOOL                          Removed;                         // Set if the device is removed if the error recovery routine fails
  U8                              DeviceIndex;                     // 0-based device index
  int                             UnitCnt;                         // Maximum units of this device
  int                             OpenUnitCt;                      // Number of opened units, can be increased up to UnitCt
  USBH_MSD_UNIT                * apUnit[USBH_MSD_MAX_LUNS_PER_DEVICE]; // Pointer to units
  USBH_INTERFACE_ID               InterfaceID;
  USBH_INTERFACE_HANDLE           InterfaceHandle;                 // UBD driver interface
  USBH_OS_EVENT_OBJ             * UrbEvent;                        // Event for synchronous URB requests
  URB                             ControlUrb;                      // Control endpoint
  URB                             Urb;                             // Data endpoint
  // Control request setup buffer
  U8                            * CswPhyTransferBuffer;
  U8                            * Ep0PhyTransferBuffer;
  U8                            * CbwPhyTransferBuffer;

#if (USBH_DEBUG > 1)
  int                             UrbRefCt;                        // UrbCt inc. if Urb in use dec. if Urb complete (s.a. INC_URB_CT, DEC_URB_CT)
  int                             SyncRefCt;                       // Used to detect synchronous errors in the upper level sector transfer
#endif

  // Private Data
  int                             bInterfaceNumber;                // Zero based interface number of the current used USB Mass Storage interface
  int                             bNumInterfaces;                  // Number of interfaces supported by the device.
  // Protocol layer
  U8                              InterfaceSubClass;
  U8                              Interfaceprotocol;
  const USBH_MSD_PHYS_LAYER_API * pPhysLayerAPI;
  // Transport layer
  U8                              BulkInEp;
  U16                             BulkMaxPktSize;
  U8                              BulkOutEp;
  U32                             BlockWrapperTag;                 // Tag is used for the bulk only command and status wrapper
  // If ErrorRecoveryCt reaches BULK_ONLY_MAX_RECOVERY then TlCommandReadData and TlCommandWriteData returns immediately
  // without any transfer. ErrorRecoveryCt will be cleared when the device is initialized or after a successful transfer.
  int                             ErrorRecoveryCt;
  USBH_MSD_TL_READ_INTERFACE          * TlCommandReadData;
  USBH_MSD_TL_WRITE_INTERFACE         * TlCommandWriteData;
  USBH_MSD_TL_RESET_INTERFACE         * TlReset;
  USBH_MSD_TL_GETMAX_LUN_INTERFACE    * TlGetMaxLUN;
} USBH_MSD_DEVICE;

/*********************************************************************
*
*       Driver object
*
**********************************************************************
*/

typedef struct _USBH_MSD_DRV {
  USBH_MSD_UNIT                   aUnit[USBH_MSD_MAX_UNITS];     // Maximum number of logical units of all connected USB Mass Storage devices
  USBH_MSD_DEVICE                       aDevice[USBH_MSD_MAX_DEVICES]; // Maximum number of connected USB devices
  USBH_NOTIFICATION_HANDLE         hPnPNotify;
  USBH_MSD_LUN_NOTIFICATION_FUNC * pfLunNotification;        // This user callback function is called if a new logical unit(s) is found
  void                           * pContext;                 // LunNotification context
  U32                              MaxTransferSize;
} USBH_MSD_DRV;

extern USBH_MSD_DRV USBH_MSD_Global;

/*********************************************************************
*
*       Defines and macros
*
**********************************************************************
*/

// if CSW_ALSO_VALID_IF_LENGTH_EQUAL_OR_GREATER is set to a none zero value then the received
// CSW block is also valid if the length is greater as the CSW length but all other bytes are ok.
#define CSW_ALSO_VALID_IF_LENGTH_EQUAL_OR_GREATER 1
// Maximum count for repeating a reading or writing command with a bulk-only USB Mass Storage reset
// command between the transfer, if the maximum is reached a USB bus reset is done with a set configuration request
#define BULK_ONLY_MAX_RECOVERY                    3
#define CBW_SIGNATURE FOUR_CHAR_ULONG             ('U','S','B','C')
#define USB_BULK_IN_FLAG                          0x80
#define CSW_SIGNATURE FOUR_CHAR_ULONG             ('U','S','B','S')
#define CSW_STATUS_GOOD                           0
#define CSW_STATUS_FAIL                           1
#define CSW_STATUS_PHASE_ERROR                    2
// Bulk only class specific requests
#define BULK_ONLY_RESET_REQ                       0xff
#define BULK_ONLY_GETLUN_REQ                      0xfe
#define BULK_ONLY_GETLUN_LENGTH                   1    //length in bytes of BULK_ONLY_GETLUN_REQ
#define CBW_FLAG_READ                             0x80
#define CBW_FLAG_WRITE                            0x0
// Command block wrapper field length
#define CBW_LENGTH                                31
#define COMMAND_WRAPPER_CDB_OFFSET                15
#define COMMAND_WRAPPER_CDB_FIELD_LENGTH          16
#define COMMAND_WRAPPER_FLAGS_OFFSET              12
// All CBW transfers shall be ordered with the LSB first(little endian)
#define CBW_LENGTH                                31

typedef struct _COMMAND_BLOCK_WRAPPER {
  U32 Signature;          //  0: Contains 'USBC'
  U32 Tag;                //  4: Unique per command id
  U32 DataTransferLength; //  8: Size of the data
  U8  Flags;              // 12: Direction in bit 7
  U8  Lun;                // 13: LUN (normally 0)
  U8  Length;             // 14: Length of CDB, <= MAX_COMMAND_SIZE
  U8  CDB[16];            // 15: Command data block
} COMMAND_BLOCK_WRAPPER;

#define CSW_LENGTH                        13
#define STATUS_WRAPPER_STATUS_OFFSET      12

typedef struct _COMMAND_STATUS_WRAPPER {
  U32 Signature; // 0: Signature, should be 'USBS'
  U32 Tag;       // 4: Tag, same as original command
  U32 Residue;   // 8: The difference between the amount of data expected (as stated in cbw->DataTransferLength)  and the actual amount of data processed by the device
  U8  Status;    //12: Status 0:GOOD 1:FAILED 2:Phase Error(repeat the command)
} COMMAND_STATUS_WRAPPER;

int  USBH_MSD_ConvStandardSense(const U8 * buffer, U16 length, STANDARD_SENSE_DATA * SensePtr);
int  USBH_MSD_ConvReadCapacity (const U8 * data,   U16 length, U32 * MaxBlockAddress, U32 * BlockLength);
void USBH_MSD_ConvModeParameterHeader(MODE_PARAMETER_HEADER * ModeHeader, const U8 * Buffer, T_BOOL ModeSense6 /* true if mode Sense(6) command data is used, else mode Sense(10) is used */);

#define URB_BUFFER_POOL_MAGIC FOUR_CHAR_ULONG('T','P','O','O')

typedef struct _URB_BUFFER_POOL {
  U32                   Magic;
  DLIST                 ListEntry;
  U32                   NumberOfBuffer; // Allocated number number of buffers in pool
  U32                   BufferCt;       // Number of buffers in buffer pool
  U32                   Size;           // Size of one buffer in bytes
  USBH_INTERFACE_HANDLE IfaceHandle;
  U8                    Endpoint;
  U32                   Index;
  int                   ResetFlag;
  int                   BusMasterMemoryFlag;
} URB_BUFFER_POOL;

#define GET_BUFFER_FROM_ENTRY(pListEntry)  STRUCT_BASE_POINTER(pListEntry, URB_BUFFER, ListEntry)

typedef struct _BUFFER_RENTRY {
  DLIST             ListEntry;
  U8              * TransferBuffer; // Transfer buffer
  URB               Urb;            // Allocated URB
  U32               Size;           // Size of buffer in bytes
  URB_BUFFER_POOL * Pool;           // Owning pool
  U32               Index;          // Index number for debugging
} URB_BUFFER;

URB_BUFFER_POOL * CreateTransferBufferPool(USBH_INTERFACE_HANDLE IfaceHandle, U8 Endpoint, U32 SizePerBuffer, U32 BufferNumbers, int BusMasterTransferMemoryFlag);

URB_BUFFER * GetFromTransferBufferPool  (URB_BUFFER_POOL * Pool);
void         PutToTransferBufferPool    (URB_BUFFER      * Buffer);
void         DeleteTransferBufferPool   (URB_BUFFER_POOL * Pool);
void         InitUrbBulkTransfer        (URB_BUFFER      * Buffer, USBH_ON_COMPLETION_FUNC Completion, void * Context);
U32          GetPendingCounterBufferPool(URB_BUFFER_POOL * Pool);

// Allocates always USBH_TRANSFER_BUFFER_ALIGNMENT aligned transfer buffer from the heap
void * UrbBufferAllocateTransferBuffer(U32 size);
// Frees buffer allocated with UrbBufferAllocateTransferBuffer
void   UrbBufferFreeTransferBuffer(void * memBlock);

#define ENUM_ERROR_NOTIFICATION_MAGIC FOUR_CHAR_ULONG('E','N','O','T')
// This macro need the struct and the name of the list entry inside the struct.
#define GET_ENUM_ERROR_NOTIFICATION_FROM_ENTRY(pListEntry) STRUCT_BASE_POINTER(pListEntry, ENUM_ERROR_NOTIFICATION, ListEntry)

typedef struct tag_ENUM_ERROR_NOTIFICATION {
#if (USBH_DEBUG > 1)
  U32                       Magic;
#endif
  DLIST                     ListEntry;         // To store this object in the BUS_DRIVER object
  void                    * Context;           // User context / A copy of the parameter passed to USBH_RegisterEnumErrorNotification
  USBH_ON_ENUM_ERROR_FUNC * EnumErrorCallback;
} ENUM_ERROR_NOTIFICATION;

// Walk trough the device driver enum error notification list and call registered notify callback routines!
void   UbdFireEnumErrorNotification                (USBH_ENUM_ERROR * enumError);
// Notify about an root hub port reset error
void   UbdSetEnumErrorNotificationRootPortReset    (HUB_PORT        * port, RH_PORTRESET_STATE state,  USBH_STATUS status);
// Notify about a USB device enumeration error
void   UbdSetEnumErrorNotificationProcessDeviceEnum(HUB_PORT        * port, DEV_ENUM_STATE state,      USBH_STATUS status, int hub_flag);

#if USBH_EXTHUB_SUPPORT
  // Notify about an external hub port reset error
  void UbdSetEnumErrorNotificationHubPortReset(     HUB_PORT        * port, HUB_PORTRESET_STATE state, USBH_STATUS status);
#endif 

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
const void * TSearchNextDescriptor(const void * PrevDesc, int * Length, int DescType);

#define PNP_NOTIFICATION_MAGIC FOUR_CHAR_ULONG('P','N','P','N')

// This macro need the struct and the name of the list entry inside the struct.
#define GET_PNP_NOTIFICATION_FROM_ENTRY(pListEntry) STRUCT_BASE_POINTER(pListEntry, PNP_NOTIFICATION, ListEntry)

typedef struct tag_PNP_NOTIFICATION { // The USB device object
#if (USBH_DEBUG > 1)
  U32                   Magic;
#endif
  DLIST                 ListEntry;       // To store this object in the BUS_DRIVER object
  USBH_PNP_NOTIFICATION UbdNotification; // A copy of the notification passed to USBH_RegisterPnPNotification
} PNP_NOTIFICATION;

#define DELAYED_PNP_NOTIFY_CONTEXT_MAGIC FOUR_CHAR_ULONG('P','N','P','D')
#define GET_DELAYED_PNP_NOTIFY_CONTEXT_FROM_ENTRY(pListEntry) STRUCT_BASE_POINTER(pListEntry, DELAYED_PNP_NOTIFY_CONTEXT, ListEntry)

// Used for indirect calling of the user notificaion routine
typedef struct tag_DELAYED_PNP_NOTIFY_CONTEXT {
#if (USBH_DEBUG > 1)
  U32 Magic;
#endif
  // To store this object in the BUS_DRIVER object
  DLIST                    ListEntry;
  void                   * Context;
  USBH_PNP_EVENT           Event;
  USBH_ON_PNP_EVENT_FUNC * NotifyCallback;
  USBH_INTERFACE_ID        Id;
} DELAYED_PNP_NOTIFY_CONTEXT;

PNP_NOTIFICATION * UbdNewNotification(USBH_PNP_NOTIFICATION * UbdNotification);
void               UbdReleaseNotification (PNP_NOTIFICATION * PnpNotification);

// If this interface matches with the interface Mask of PnpNotification the event notification function is called with the event.
// Parameters:
//   PnpNotification: Pointer to the notification
//   dev:             Pointer to an device
//   event:           device is connected, device is removed!
//                    Normaly one device at the time is changed!
void UbdProcessDeviceNotifications(PNP_NOTIFICATION * PnpNotification, USB_DEVICE * dev, USBH_PNP_EVENT event);

// Check the notification against all interfaces. If an device is removed or connected and the interface matches
// and the event has been not sent the notification functon is called.
void UbdProcessNotification(PNP_NOTIFICATION * PnpNotification);
void UbdNotifyWrapperCallbackRoutine(void    * Context);

#define INTERFACE_ENTRY_MAGIC FOUR_CHAR_ULONG('I','F','A','E')
// This macro need the struct and the name of the list entry inside the struct
#define GET_INTERFACE_ENTRY_FROM_ENTRY(pListEntry)  STRUCT_BASE_POINTER(pListEntry, INTERFACE_ENTRY, ListEntry)

// The interface list object based on one host controller!
typedef struct tag_INTERFACE_LIST {
  DLIST        UsbInterfaceEntryList; // List for interfaces of type INTERFACE_ENTRY
  unsigned int InterfaceCount;        // Number of entries in the UsbInterfaceList
} INTERFACE_LIST;

// the entry to keep this object in the InterfaceList
typedef struct tag_INTERFACE_ENTRY {
#if (USBH_DEBUG > 1)
  U32                 Magic;
#endif
  DLIST               ListEntry;
  HOST_CONTROLLER   * HostController; // Pointer to the owning host controller
  USBH_INTERFACE_ID   InterfaceID;    // The interface ID
} INTERFACE_ENTRY;

// Sends the init sequence to a device that supports the transparent SCSI protocol
USBH_STATUS USBH_MSD_PHY_InitSequence    (USBH_MSD_DEVICE * Dev);
T_BOOL      USBH_MSD_PHY_IsWriteProtected(USBH_MSD_UNIT  * unit); // Checks if the specified unit is write protected.

// Defines the maximum number of repetitions if a valid command status block is
// received but the content indicates that the command failed.
// This depends also from the value of the Sense code and whether or not a command is sent a second time.
#define MAX_COMMAND_REPETITIONS     3
// Specifies the time, in milliseconds, to wait before the command will be sent again
// in case of a Sense error.
#define TIMEOUT_REPEAT_SENSE_ERROR  10
// Defines the maximum number of repetitions if a valid command status block is
// received but the content indicates that the command failed.
// This depends also from the value of the Sense code and whether or not a command is sent a second time.
#define MAX_COMMAND_REPETITIONS     3
// Specifies the time, in milliseconds, to wait before the command will be sent again
// in case of a Sense error.
#define TIMEOUT_REPEAT_SENSE_ERROR  10

/*********************************************************************
*
*       Register access macros.
*
**********************************************************************
*/

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

#define OH_EP0_VALID(OHD_EP0_Ptr)   HCM_ASSERT_ITEM_HEADER(&OHD_EP0_Ptr->ItemHeader)
// This macro need the struct and the name of the list entry inside the struct
#define GET_CONTROL_EP_FROM_ENTRY(pListEntry) STRUCT_BASE_POINTER(pListEntry, OHD_EP0, ListEntry)
#define OHD_EP0_MAGIC FOUR_CHAR_ULONG('E','P','0','M')

// Control endpoint states
typedef enum T_Ep0_Phase {
  ES_IDLE   = 0,
  ES_SETUP,             // 1
  ES_DATA,              // 2
  ES_COPY_DATA,         // 3
  ES_PROVIDE_HANDSHAKE,
  ES_HANDSHAKE,         // 4
  ES_ERROR              // 5
} Ep0_Phase;

typedef struct tag_SETUP_BUFFER {
  // Recommended!!!:
  //   first filed:  HCM_ITEM_HEADER
  //   second field: U8 EndpointType
  HCM_ITEM_HEADER ItemHeader;
} SETUP_BUFFER;

// The logical control EP object
typedef struct tag_OHD_EP0 {
  // Recommended!!!:
  //   first filed:  HCM_ITEM_HEADER
  //   second field: U8 EndpointType
  HCM_ITEM_HEADER                   ItemHeader;
  U8                                EndpointType;        // Endpoint type
  struct T_HC_DEVICE              * Dev;                 // Backward pointer to the device
  OhEpState                         State;
  DLIST                             ListEntry;           // OHD_EP0 list
  DLIST                             UrbList;             // submitted URB list
  U32                               UrbCount;            // number of requests
  URB                             * PendingUrb;          // pending URB
  U32                               TdCounter;           // number of TDs on this endpoint
  U32                               AbortMask;
  Ep0_Phase                         Ep0Phase;            // Setup, data or handshake phase
  U8                              * Setup;               // pointer to the virtual address of the buffer in SetupPacket
  SETUP_BUFFER                    * SetupPacket;         // pointer to an HCM pool entry which contains the Setup packet address
  struct T_OH_TRANSFER_BUFFER     * DataPhaseCopyBuffer; // buffer used during data phase,  pointer is only valid in the data phase
  USBH_RELEASE_EP_COMPLETION_FUNC * ReleaseCompletion;
  void                            * ReleaseContext;
  U32                               Mask;
  U8                                DeviceAddress;
  U8                                EndpointAddress;
  U16                               MaxPktSize;      // Maximum packet size for that endpoint
  USBH_SPEED                        Speed;
} OHD_EP0;

/*********************************************************************
*
*       Resources allocation and releasing functions
*
**********************************************************************
*/

// Allocates all resources needed for control transfer, endpoints will not added to the control list
USBH_STATUS   OhEp0Alloc(HCM_POOL * EpPool, HCM_POOL * SetupPacketPool, U32 Numbers);
void          OhEp0Free (HCM_POOL * EpPool); // Releases all needed resources used for all control endpoints
OHD_EP0     * OhEp0Get  (HCM_POOL * EpPool, HCM_POOL * SetupPacketPool);
void          OhEp0Put  (OHD_EP0  * Ep);     // Puts first all appended TDs back to the pool and as last the  control endpoint object

/*********************************************************************
*
*       Operations
*
**********************************************************************
*/

USBH_STATUS   OhEp0Init                   (OHD_EP0            * Ep,  struct T_HC_DEVICE * Dev, U32 Mask, U8 DeviceAddress, U8 EndpointAddress, U16 MaxFifoSize, USBH_SPEED Speed);
void          OhEp0Insert                 (OHD_EP0            * Ep);
// Releases that endpoint. This function returns immediately. If the Completion function is called the endpoint is removed.
void          OhEp0_ReleaseEndpoint       (OHD_EP0            * Ep,  USBH_RELEASE_EP_COMPLETION_FUNC * ReleaseEpCompletion, void * Context);
// 1. If an TD is pending (on control endpoint, only one TD per request is allowed) then the Skip bit is set and an timer is scheduled
// 2. In the timer completion routine the TD is canceled. The endpoint skip bit is set to zero and the list filled bit is set again!
// 3. All other URBs are complete in the rigth range.
USBH_STATUS   OhEp0AbortEndpoint          (OHD_EP0            * Ep);
// Returns a pointer to an control endpoint with the device address on none dummy endpoints
OHD_EP0     * OhEp0GetFromDevAddress      (struct T_HC_DEVICE * Dev);
USBH_STATUS   OhEp0AddUrb                 (OHD_EP0            * Ep,  URB * Urb);
// Called during the second enumeration of the DONE TD list. This function checks for errors an switch to the next setup phase.
void          OhEp0DoneCheckForCompletion (OHD_GTD            * Gtd);
// Parameters:  AllEndpointFlag:  If true all control endpoints in the linked list are deleted, else only endpoints where the UNLINK Flag is on
void          OhEp0RemoveEndpoints        (struct T_HC_DEVICE * dev, T_BOOL AllEndpointFlag);
void          OhEp0ReleaseEp_TimerCallback(void               * Context); // Common ep0 timer callback routine
// Abort timer routine for all canceled URBs.
// 1. If no pending URb available complete other aborted URBs.
// 2. If the URB pending:
//    If the URb is not on the DONE list and not in the EDs TD list wait for DONE interrupt.
// Others:
//   It is allowed to submit an new URB on an aborted endpoint and to abort this new submitted endpoint in the
//   context of OhEp0AbortUrb_TimerCallback in the URbs compleiton routines.
void OhEp0AbortUrb_TimerCallback          (void               * Context);
void             USBH_OnISREvent          (void);
void             USBH_Free                (void               * MemBlock);
USBH_HC_HANDLE   USBH_OHC_CreateController(void               * BaseAddress);
U8               USBH_OHC_ServiceISR      (USBH_HC_HANDLE       HcHandle);
USBH_STATUS      USBH_OHC_AddController   (USBH_HC_HANDLE       HcHandle, USBH_HC_BD_HANDLE    * HcBdHandle);
USBH_STATUS      USBH_OHC_RemoveController(USBH_HC_HANDLE       HcHandle, REMOVE_HC_COMPLETION * Completion, void * Context);
void             USBH_OHC_DeleteController(USBH_HC_HANDLE       HcHandle);
void             USBH_OHC_ProcessInterrupt(USBH_HC_HANDLE       HcHandle);
void           * USBH_Malloc              (U32                  Size);
void           * USBH_MallocZeroed        (U32                  Size);
void           * USBH_TryMalloc           (U32                  Size);
void           * USBH_AllocTransferMemory (U32                  NumBytes, unsigned Alignment);

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

void         USBH_HC_ClrActivePortReset(HOST_CONTROLLER * pHost, HUB_PORT * pEnumPort);
void         USBH_HC_SetActivePortReset(HOST_CONTROLLER * pHost, HUB_PORT * pEnumPort);

const char * UbdPortSpeedStr  (USBH_SPEED    x);
const char * USBH_GetStatusStr(USBH_STATUS   x);
const char * UbdPortStateStr  (PORT_STATE    x);
const char * UbdUrbFunctionStr(USBH_FUNCTION x);

#if USBH_EXTHUB_SUPPORT
  const char * UbdHubPortResetStateStr   (HUB_PORTRESET_STATE x);
  const char * UbdHubNotificationStateStr(HUB_NOTIFY_STATE    x);
  const char * UbdHubEnumStateStr        (HUB_ENUM_STATE      x);
#endif

const char * UbdEnumStateStr       (DEV_ENUM_STATE        x);
const char * UbdRhPortResetStateStr(RH_PORTRESET_STATE    x);
const char * UbdHcStateStr         (HOST_CONTROLLER_STATE x);

#undef EXTERN

#if defined(__cplusplus)
  }
#endif

#endif

/******************************* EOF ********************************/
