/*********************************************************************
*  
*   IAR PowerPac
*
*   (c) Copyright IAR Systems 2010.  All rights reserved.
*
**********************************************************************
----------------------------------------------------------------------
File    : USB_MSD.h
Purpose : Public header of the mass storage device client
--------  END-OF-HEADER  ---------------------------------------------
*/

#ifndef MSD_H          /* Avoid multiple inclusion */
#define MSD_H

#include "SEGGER.h"

#if defined(__cplusplus)
extern "C" {     /* Make sure we have C-declarations in C++ programs */
#endif

/*********************************************************************
*
*       Config defaults
*
**********************************************************************
*/

#ifndef   MSD_DEBUG_LEVEL
  #define MSD_DEBUG_LEVEL 0
#endif

#ifndef   MSD_USE_PARA
  #define MSD_USE_PARA(para) para = para
#endif

#ifndef   USB_MSD_MAX_UNIT
  #define USB_MSD_MAX_UNIT 4
#endif

#ifndef   MSD_USB_CLASS
  #define MSD_USB_CLASS     8       // 8: Mass storage
#endif

#ifndef   MSD_USB_SUBCLASS
  #define MSD_USB_SUBCLASS  6       // 1: RBC (reduced SCSI) 2: ATAPI, 3: QIC 157, 4: UFI, 6: SCSI
#endif

#ifndef   MSD_USB_PROTOCOL
  #define MSD_USB_PROTOCOL  0x50    // 0x50: BOT (Bulk-only-transport)
#endif


/*********************************************************************
*
*       Types
*
**********************************************************************
*/
typedef struct _LUN_INFO LUN_INFO;

typedef void (PREVENT_ALLOW_REMOVAL_HOOK)(U8 PreventRemoval);
typedef void (READ_WRITE_HOOK)(U8 Lun, U8 IsRead, U8 OnOff, U32 StartLBA, U32 NumBlocks);
typedef U8   (USB_MSD_HANDLE_CMD)(LUN_INFO * pLUNInfo, U8 * pCmdBlock, U32 * pNumBytes);

/*********************************************************************
*
*       Storage interface
*/
typedef struct {
  U32 NumSectors;
  U16 SectorSize;
} USB_MSD_INFO;


/*********************************************************************
*
*       Storage interface
*/
typedef struct {
  U8 EPIn;
  U8 EPOut;
  U8 InterfaceNum;
} USB_MSD_INIT_DATA;

typedef struct {
  void     * pStart;
  U32        StartSector;
  U32        NumSectors;
  U16        SectorSize;
  void     * pSectorBuffer;
  unsigned   NumBytes4Buffer;
} USB_MSD_INST_DATA_DRIVER;

typedef struct {
  void       (*pfInit)           (U8 Lun, const USB_MSD_INST_DATA_DRIVER * pDriverData);
  void       (*pfGetInfo)        (U8 Lun, USB_MSD_INFO * pInfo);
  U32        (*pfGetReadBuffer)  (U8 Lun, U32 SectorIndex, void      **ppData, U32 NumSectors);
  char       (*pfRead)           (U8 Lun, U32 SectorIndex,       void * pData, U32 NumSector);
  U32        (*pfGetWriteBuffer) (U8 Lun, U32 SectorIndex, void      **ppData, U32 NumSectors);
  char       (*pfWrite)          (U8 Lun, U32 SectorIndex, const void * pData, U32 NumSectors);
  char       (*pfMediumIsPresent)(U8 Lun);
  void       (*pfDeInit)         (U8 Lun);
} USB_MSD_STORAGE_API;

typedef struct {
  const USB_MSD_STORAGE_API * pAPI;
  USB_MSD_INST_DATA_DRIVER    DriverData;
  U8                          DeviceType;      // 0: Direct access block device ... 5: CD/DVD
  U8                          IsPresent;
  USB_MSD_HANDLE_CMD       *  pfHandleCmd;
  U8                          IsWriteProtected;
} USB_MSD_INST_DATA;


/*********************************************************************
*
*       API functions
*
**********************************************************************
*/
void USB_MSD_Add     (const USB_MSD_INIT_DATA * pInitData);
void USB_MSD_AddUnit (const USB_MSD_INST_DATA * pInstData);
void USB_MSD_AddCDRom(const USB_MSD_INST_DATA * pInstData);

void USB_MSD_SetPreventAllowRemovalHook(U8 Lun, PREVENT_ALLOW_REMOVAL_HOOK * pfOnPreventAllowRemoval);
void USB_MSD_SetReadWriteHook          (U8 Lun, READ_WRITE_HOOK * pfOnReadWrite);

void USB_MSD_Task   (void);

const char * USB_MSD_GetProductVer (U8 Lun);
const char * USB_MSD_GetProductName(U8 Lun);
const char * USB_MSD_GetVendorName (U8 Lun);
const char * USB_MSD_GetSerialNo   (U8 Lun);


void USB_MSD_RequestDisconnect   (U8 Lun);
void USB_MSD_Disconnect          (U8 Lun);
int  USB_MSD_WaitForDisconnection(U8 Lun, U32 TimeOut);
void USB_MSD_Connect             (U8 Lun);
void USB_MSD_UpdateWriteProtect  (U8 Lun, U8 IsWriteProtected);

/*********************************************************************
*
*       Storage interface
*
**********************************************************************
*/
//
//  Obsolete name for USB MSD storage drivers
//
//#define USB_MSD_StorageFS        USB_MSD_StorageByIndex
//#define USB_MSD_StorageFSDriver  USB_MSD_StorageByName

extern const USB_MSD_STORAGE_API USB_MSD_StorageRAM;
extern const USB_MSD_STORAGE_API USB_MSD_StorageByIndex;
extern const USB_MSD_STORAGE_API USB_MSD_StorageByName;

#if defined(__cplusplus)
  }              /* Make sure we have C-declarations in C++ programs */
#endif

#endif                 /* Avoid multiple inclusion */

/**************************** end of file ***************************/

