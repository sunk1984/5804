/*********************************************************************
*  
*   IAR PowerPac
*
*   (c) Copyright IAR Systems 2010.  All rights reserved.
*
**********************************************************************
----------------------------------------------------------------------
File    : USBH_MSD_FS.c
Purpose : Implementation of an interface between the USB Host library
          and the file system.
--------  END-OF-HEADER  ---------------------------------------------
*/


#include "FS_Int.h"
#include "USBH_Int.h"

/*********************************************************************
*
*       Static code
*
**********************************************************************
*/

/*********************************************************************
*
*       _ReadSectors
*
*  Description:
*    Reads one or more sectors from the medium.
*
*  Parameters:
*    Unit            - Device number.
*    SectorIndex     - SectorIndex to be read from the device.
*    pBuffer         - Pointer to buffer for storing the data.
*    NumSectors      - Number of sectors
*
*  Return value:
*    ==0             - SectorIndex has been read and copied to pBuffer.
*     <0             - An error has occurred.
*/
static int _ReadSectors(U8 Unit, U32 SectorIndex, void * pBuffer, U32 NumSectors) {
  int Status;

  Status = USBH_MSD_ReadSectors(Unit, SectorIndex, NumSectors, pBuffer);
  if (Status) {
    FS_DEBUG_LOG((FS_MTYPE_DRIVER, "ERROR _ReadSectors status: 0x%08x\n", Status));
    return -1;
  }
  return 0;
}

/*********************************************************************
*
*       _WriteSectors
*
*  Description:
*  FS driver function. Writes a sector to the medium.
*
*  Parameters:
*    Unit           - Device number.
*    SectorIndex    - SectorIndex to be written on the device.
*    pBuffer        - Pointer to a buffer containing the data to be written.
*    NumSectors     - Number of sectors
* 
*  Return value:
*    ==0            - SectorIndex has been written to the device.
*     <0            - An error has occurred.
*/

static int _WriteSectors(U8 Unit, U32 SectorIndex, const void * pBuffer, U32 NumSectors, U8 RepeatSame) {
  int Status;

  if (RepeatSame) {
    do {
      Status = USBH_MSD_WriteSectors(Unit, SectorIndex++, 1, pBuffer);
      if (Status) {
        goto OnError;
      }
    } while (--NumSectors);
  } else {
    Status = USBH_MSD_WriteSectors(Unit, SectorIndex, NumSectors, pBuffer);
    if (Status) {
      goto OnError;
    }
  }
  return 0;
OnError:
  FS_DEBUG_ERROROUT((FS_MTYPE_DRIVER, "ERROR _WriteSectors status: 0x%08x\n", Status));
  return -1;
}


/*********************************************************************
*
*       _IoCtl
*
*  Description:
*    Executes a device command.
*
*  Parameters:
*    Unit         - Device number.
*    Cmd          - Command to be executed.
*    Aux          - Parameter depending on command.
*    pBuffer      - Pointer to a buffer used for the command.
* 
*  Return value:
*    This function is used to execute device specific commands.
*    In the file fs_api.h you can find the I/O commands that are currently
*    implemented. If the higher layers don't know the command, they
*    send it to the next lower. Your driver does not have to
*    implement one of these commands. Only if automatic formatting
*    is used or user routines need to get the size of the medium,
*    FS_CMD_GET_DEVINFO must be implemented.
*/
static int _IoCtl(U8 Unit, I32 Cmd, I32 Aux, void * pBuffer) {
  FS_DEV_INFO        * pInfo;
  USBH_MSD_UNIT_INFO   Info;
  int                  Status;

  FS_USE_PARA(Aux);
  switch (Cmd) {
  case FS_CMD_GET_DEVINFO:
    if (pBuffer == 0) {
      return -1;
    }
    pInfo                  = (FS_DEV_INFO *)pBuffer; // The parameter pBuffer contains the pointer to the structure
    pInfo->NumHeads        = 0;                      // Relevant only for mechanical drives
    pInfo->SectorsPerTrack = 0;                      // Relevant only for mechanical drives
    Status = USBH_MSD_GetUnitInfo(Unit, &Info);
    if (Status < 0) {
      FS_DEBUG_ERROROUT((FS_MTYPE_DRIVER, "ERROR _IoCtl: no device information: 0x%08x\n", Status));
      return -1;
    } else {
      pInfo->BytesPerSector = Info.BytesPerSector;
      pInfo->NumSectors     = Info.TotalSectors;
      FS_DEBUG_LOG((FS_MTYPE_DRIVER, "INFO _IoCtl: bytes per sector: %d total sectors: %d\n", pInfo->BytesPerSector, pInfo->NumSectors));
    }
    break;
  default:
    break;
  }
  return 0; /* Return zero if no problems have occurred. */
}

/*********************************************************************
*
*       _GetStatus
*
*  Description:
*    FS driver function. Gets status of the device. This function is also
*    used to initialize the device and to detect a media change.
*
*  Parameters:
*    Unit                       - Device number.
* 
*  Return values:
*   FS_MEDIA_IS_PRESENT         - Device okay and ready for operation.
*   FS_MEDIA_NOT_PRESENT
*/
static int _GetStatus(U8 Unit) {
  int Status;

  Status = USBH_MSD_GetStatus(Unit);
  if (Status) {
    // unit not ready
    FS_DEBUG_ERROROUT((FS_MTYPE_DRIVER, "ERROR _GetStatus: device not ready, USBH MSD Status: 0x%08x\n",Status));
    return FS_MEDIA_NOT_PRESENT;
  }
  return FS_MEDIA_IS_PRESENT;
}

/*********************************************************************
*
*       _InitMedium
*
*/
static int _InitMedium(U8 Unit) {
  FS_USE_PARA(Unit);
  FS_DEBUG_LOG((FS_MTYPE_DRIVER, "INFO:_InitMedium:  unit:%d\n",(int)Unit));
  return FS_ERR_OK;
}

/*********************************************************************
*
*       _GetNumUnits
*
*/
static int _GetNumUnits(void) {
  return USBH_MSD_MAX_UNITS;
}

/*********************************************************************
*
*       _GetDriverName
*/
static const char * _GetDriverName(U8 Unit) {
  FS_USE_PARA(Unit);
  return "msd";
}

/*********************************************************************
*
*       _AddDevice
*/
static int _AddDevice(void) {
  return 0;
}


/*********************************************************************
*
*       Global variables
*
**********************************************************************
*/
const FS_DEVICE_TYPE USBH_MSD_FS_Driver = {
  _GetDriverName,
  _AddDevice,
  _ReadSectors,                       // Device read sector
  _WriteSectors,                      // Device write sector
  _IoCtl,                             // IO control interface
  _InitMedium,                        // not used, only for debugging
  _GetStatus,                         // Device status
  _GetNumUnits
};

/*************************** End of file ****************************/
