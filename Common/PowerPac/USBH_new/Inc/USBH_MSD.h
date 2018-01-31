/*********************************************************************
*  
*   IAR PowerPac
*
*   (c) Copyright IAR Systems 2010.  All rights reserved.
*
**********************************************************************
----------------------------------------------------------------------
File        : USBH_MSD.h
Purpose     : MSD API of the USB host stack
---------------------------END-OF-HEADER------------------------------
*/

#ifndef _USBH_MSD_H_
#define _USBH_MSD_H_

#include <stdarg.h>
#include "SEGGER.h"
#include "USBH.h"

#if defined(__cplusplus)
  extern "C" {                 // Make sure we have C-declarations in C++ programs
#endif

#ifndef    USBH_MSD_MAX_UNITS
  #define  USBH_MSD_MAX_UNITS (USBH_MSD_MAX_LUNS_PER_DEVICE * USBH_MSD_MAX_DEVICES)
#endif



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
  U32   TotalSectors;     // Total number of sectors on the medium
  U16   BytesPerSector;   // Number of bytes per sector
  int   WriteProtectFlag; // Unequal zero if the device is write protected
  U16   VendorId;
  U16   ProductId;
  char  acVendorName[9];
  char  acProductName[17];
  char  acRevision[5];
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
