/*********************************************************************
*  
*   IAR PowerPac
*
*   (c) Copyright IAR Systems 2010.  All rights reserved.
*
**********************************************************************
----------------------------------------------------------------------
File        : USBH_MSD_PhysLayerWrapper.c
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
#include "USBH_MSD_Int.h"

#if (USBH_DEBUG > 1)
  #define USBH_MSD_PLW_PRINT_INQUIRYDATA(data)  _PlWPrintInquiryData(data)
#else
  #define USBH_MSD_PLW_PRINT_INQUIRYDATA(data)
#endif

/*********************************************************************
*
*       static const
*
**********************************************************************
*/

//
// Trace helpers
//
#if (USBH_DEBUG > 1)

typedef struct STATUS_TEXT_TABLE {
  int          Id;
  const char * sText;
} STATUS_TEXT_TABLE;

// String table for printing Status messages
static STATUS_TEXT_TABLE const _aDevTypeTable [] = {
  { INQUIRY_DIRECT_DEVICE,         "Direct Device" },
  { INQUIRY_SEQ_DEVICE,            "Sequential-access device (streamer)" },
  { INQUIRY_WRITE_ONCE_DEVICE,     "WriteOnce device" },
  { INQUIRY_CD_ROM_DEVICE,         "CD-ROM/DVD" },
  { INQUIRY_NON_CD_OPTICAL_DEVICE, "Optical memory device" }
};

// String table for printing Status messages
static STATUS_TEXT_TABLE const _aVersionTable [] = {
  { ANSI_VERSION_MIGHT_UFI,      "ANSI_VERSION_MIGHT_COMPLY with UFI" },
  { ANSI_VERSION_SCSI_1,         "ANSI_VERSION_SCSI_1" },
  { ANSI_VERSION_SCSI_2,         "ANSI_VERSION_SCSI_2" },
  { ANSI_VERSION_SCSI_3_SPC,     "ANSI_VERSION_SCSI_3_SPC" },
  { ANSI_VERSION_SCSI_3_SPC_2,   "ANSI_VERSION_SCSI_3_SPC_2" },
  { ANSI_VERSION_SCSI_3_SPC_3_4, "ANSI_VERSION_SCSI_3_SPC_3_4" }
};

// String table for printing Status messages
static STATUS_TEXT_TABLE const _aResponseFormatTable [] = {
  { INQUIRY_RESPONSE_SCSI_1,          "INQUIRY_RESPONSE_SCSI_1"},
  { INQUIRY_RESPONSE_IN_THIS_VERISON, "INQUIRY_RESPONSE_IN_THIS_VERISON"},
  { INQUIRY_RESPONSE_MIGTH_UFI,       "INQUIRY_RESPONSE_MIGHT_UFI"}
};

/*********************************************************************
*
*       Static helper functions
*
**********************************************************************
*/

/*********************************************************************
*
*       _Id2Text
*
*  Function description
*/
static const char * _Id2Text(int Id, const STATUS_TEXT_TABLE * pTable, unsigned NumItems) {
  unsigned i;
  for (i = 0; i < NumItems; i++) {
    if (pTable->Id == Id) {
      return pTable->sText;
    }
    pTable++;
  }
  return "";
}

/*********************************************************************
*
*       _UsbPrintDeviceType_
*
*  Function description
*/
static void _UsbPrintDeviceType(int DeviceType) {
  USBH_LOG((USBH_MTYPE_MSD_PHYS, "Inquiry type: %s", _Id2Text(DeviceType, _aDevTypeTable, USBH_COUNTOF(_aDevTypeTable))));
}

/*********************************************************************
*
*       _UsbPrintVersion_
*
*  Function description
*/
static void _UsbPrintVersion(int Version) {
  USBH_LOG((USBH_MTYPE_MSD_PHYS, "Inquiry version:", _Id2Text(Version, _aVersionTable, USBH_COUNTOF(_aVersionTable))));
}

/*********************************************************************
*
*       _UsbResponseFormat_
*
*  Function description
*/
static void _UsbResponseFormat(int Format) {
  USBH_LOG((USBH_MTYPE_MSD_PHYS, "Format:", _Id2Text(Format, _aResponseFormatTable, USBH_COUNTOF(_aResponseFormatTable))));
}

/*********************************************************************
*
*       _PlWPrintInquiryData
*
*  Function description
*/
static void _PlWPrintInquiryData(INQUIRY_STANDARD_RESPONSE * data) {
  int response_ver;
  if (USBH_DEBUG > 1) {                                                     // Print some information if DBG_INFO is set:
    _UsbPrintDeviceType(data->DeviceType & INQUIRY_DEVICE_TYPE_MASK);       // Device type
    if (data->RMB & INQUIRY_REMOVE_MEDIA_MASK) {                            // If device is removable
      USBH_LOG((USBH_MTYPE_MSD_PHYS, "Inquiry data:    Medium is removeable!"));
    }
    _UsbPrintVersion(data->Version & INQUIRY_VERSION_MASK);                 // ANSI version
    response_ver = data->ResponseFormat & INQUIRY_RESPONSE_FORMAT_MASK;     // Response data format
    _UsbResponseFormat(response_ver);
  }
}

#endif

/*********************************************************************
*
*       static code
*
**********************************************************************
*/

/*********************************************************************
*
*       _CheckInquiryData
*
*  Function description:
*    Checks whether the device (USB floppy, direct memory access and hard disk)
*    can be handled by us.
*
*  Return value:
*    == TRUE     -  Device can be handled by us
*    == FALSE    -  Device is a type that can not be handled by us.
*
*/
static USBH_BOOL _CheckInquiryData(INQUIRY_STANDARD_RESPONSE * data) {
  if ((data->DeviceType & INQUIRY_DEVICE_TYPE_MASK) != INQUIRY_DIRECT_DEVICE) { // No direct access device
    return FALSE;
  } else {
    return TRUE;
  }
}

/*********************************************************************
*
*       _InquiryDevice
*
*  Function description:
*    Sends the standard INQUIRY command to the device and checks important parameters.
*    The device must be a direct access device.
*
*  Return value:
*    == 0        -  Success
*    != 0        -  Error
*/
static USBH_STATUS _InquiryDevice(USBH_MSD_DEVICE * pDev) {
  int               i;
  USBH_MSD_UNIT  * pUnit;
  USBH_STATUS       Status;
  U8                Size;
  U8              * pBuffer;

  USBH_ASSERT_MAGIC(pDev, USBH_MSD_DEVICE);
  pBuffer = (U8 *)USBH_URB_BufferAllocateTransferBuffer(STANDARD_INQUIRY_DATA_LENGTH);
  if (NULL == pBuffer) {
    USBH_WARN((USBH_MTYPE_MSD_PHYS, "MSD:  USBH_MSD_PlWInquiryDevice: no memory resources!"));
    return USBH_STATUS_RESOURCES;
  }
  Status = USBH_STATUS_ERROR;
  pUnit   = &USBH_MSD_Global.aUnit[0];
  for (i = 0; i < USBH_MSD_MAX_UNITS; i++, pUnit++) { // Call all units
    if (pUnit->pDev == pDev) {
      Size   = STANDARD_INQUIRY_DATA_LENGTH;
      Status = pDev->pPhysLayerAPI->pfInquiry(pUnit, pBuffer, &Size, Standard, 0);
      if (Status) { // On error
        USBH_WARN((USBH_MTYPE_MSD_PHYS, "MSD: USBH_MSD_PlWInquiryDevice: LUN: %u",pUnit->Lun));
        USBH_PRINT_STATUS_VALUE(USBH_MTYPE_MSD_PHYS, Status);
      } else { // Success, store parameters in the pUnit
        USBH_MEMCPY(&pUnit->InquiryData, pBuffer, sizeof(pUnit->InquiryData));
        USBH_MSD_PLW_PRINT_INQUIRYDATA(&pUnit->InquiryData);
        if (!_CheckInquiryData(&pUnit->InquiryData)) {
          unsigned DeviceType;
          //
          // This LUN can not be handled by us since it is not a direct access device (CDROM/DVD)
          //
          DeviceType = pUnit->InquiryData.DeviceType & INQUIRY_DEVICE_TYPE_MASK;
          USBH_USE_PARA(DeviceType);
          USBH_WARN((USBH_MTYPE_MSD_PHYS, "MSD: Device can not be handled, device type %s is not supported!", _Id2Text(DeviceType, _aDevTypeTable, USBH_COUNTOF(_aDevTypeTable))));
          //Status = USBH_STATUS_ERROR;
          pUnit->pDev = NULL;
          pDev->UnitCnt--;
        }
      }
    }
  }
  USBH_URB_BufferFreeTransferBuffer(pBuffer);
  return Status;
}

/*********************************************************************
*
*       _ReadLunCapacity
*
*  Function description:
*    Executes a READ CAPACITY command on all logical units of the device.
*
*  Return value:
*    == 0        -  Success (sector Size and max sector address successfully obtained)
*    != 0        -  Error
*/
static USBH_STATUS _ReadLunCapacity(USBH_MSD_DEVICE * pDev) {
  int             i;
  USBH_MSD_UNIT * pUnit;
  USBH_STATUS     Status;
  unsigned        NumRetries = 20;

  Status      = USBH_STATUS_ERROR;
  pUnit        = &USBH_MSD_Global.aUnit[0];
  for (i = 0; i < USBH_MSD_MAX_UNITS; i++, pUnit++) {
    if (pUnit->pDev == pDev) {
Retry:
      Status = pDev->pPhysLayerAPI->pfReadCapacity(pUnit, &pUnit->MaxSectorAddress, &pUnit->BytesPerSector); // Read the capacity of the logical unit
      if (Status) { // Error, break
        Status = pDev->pPhysLayerAPI->pfRequestSense(pUnit);
        if (Status == USBH_STATUS_SUCCESS) {
          if (pUnit->Sense.Sensekey == SS_SENSE_UNIT_ATTENTION) {
            USBH_WARN((USBH_MTYPE_MSD_PHYS, "MSD: Lun %d is not ready.", i));
            if (NumRetries--) {
              USBH_OS_Delay(1000);
              goto Retry;
            }
          }
        }
        USBH_WARN((USBH_MTYPE_MSD_PHYS, "MSD: _ReadLunCapacity: PlCapacity: LUN: %d, Status=%s ", pUnit->Lun, USBH_GetStatusStr(Status)));
      }
#if (USBH_DEBUG > 1)
      else {
        USBH_LOG((USBH_MTYPE_MSD_PHYS, "INFO _ReadLunCapacity LUN: %u max. sector address: %lu bytes per sector: %d", pUnit->Lun,pUnit->MaxSectorAddress,(int)pUnit->BytesPerSector));
      }

#endif
    }
  }
  return Status;
}

/*********************************************************************
*
*       _CheckModeParameters
*
*  Function description:
*    Sends the SCSI command MODE SENSE with the parameter MODE_SENSE_RETURN_ALL_PAGES to get
*    all supported parameters of all pages. Only the mode parameter ModeHeader is stored in the
*    unit object of the device. This ModeHeader is used to detect if the unit is write protected.
*
*  Return value:
*    == 0        -  Success
*    != 0        -  Error
*/
static USBH_STATUS _CheckModeParameters(USBH_MSD_DEVICE * pDev) {
  int                     i;
  USBH_MSD_UNIT         * pUnit;
  USBH_STATUS             Status;
  U8                      Size;
  U8                    * pBuffer;
  MODE_PARAMETER_HEADER   ModeHeader;

  pBuffer = (U8 *)USBH_URB_BufferAllocateTransferBuffer(MODE_SENSE_PARAMETER_LENGTH);
  if (NULL == pBuffer) {
    USBH_WARN((USBH_MTYPE_MSD_PHYS, "MSD: USBH_MSD_PlWCheckModeParameters: no memory resources!"));
    return USBH_STATUS_RESOURCES;
  }
  Status = USBH_STATUS_ERROR;
  pUnit   = &USBH_MSD_Global.aUnit[0];
  for (i = 0; i < USBH_MSD_MAX_UNITS; i++, pUnit++) { // Call all units
    if (pUnit->pDev == pDev) {
      Size = sizeof(MODE_PARAMETER_HEADER);
      Status = pDev->pPhysLayerAPI->pfModeSense(pUnit, pBuffer, &Size, &ModeHeader, MODE_SENSE_RETURN_ALL_PAGES, 0);
      if (Status) { // On error
        USBH_WARN((USBH_MTYPE_MSD_PHYS, "MSD: USBH_MSD_PlWCheckModeParameter: PlModeSense!"));
        if (pDev->pPhysLayerAPI->pfRequestSense(pUnit) == USBH_STATUS_SUCCESS) {
          if (pUnit->Sense.Sensekey == SS_SENSE_NOT_READY) {
            USBH_WARN((USBH_MTYPE_MSD_PHYS, "MSD: Lun %d is not ready.", i));
            Status = USBH_STATUS_SENSE_REPEAT;
          }
        }
        break;
      } else { // On success, copy the received ModeHeader to the device object!
        USBH_MEMCPY(&pUnit->ModeParamHeader, &ModeHeader, sizeof(ModeHeader));
      }
    }
  }
  USBH_URB_BufferFreeTransferBuffer(pBuffer);
  return Status;
}

/*********************************************************************
*
*       Public code
*
**********************************************************************
*/

/*********************************************************************
*
*       USBH_MSD_PHY_InitSequence
*
*  Function description:
*    Sends the init sequence to a device that supports the transparent SCSI protocol
*
*  Return value:
*    == 0        -  Success
*    != 0        -  Error
*/
USBH_STATUS USBH_MSD_PHY_InitSequence(USBH_MSD_DEVICE * pDev) {
  USBH_STATUS Status;

  USBH_ASSERT_MAGIC(pDev, USBH_MSD_DEVICE);
  Status = _InquiryDevice(pDev);
  if (Status) {
    USBH_WARN((USBH_MTYPE_MSD_PHYS, "MSD: USBH_MSD_PHY_InitSequence: _InquiryDevice!"));
    goto Exit;
  }
  Status = _ReadLunCapacity(pDev);     // Query the capacity for all LUNS of this device
  if (Status) {
    USBH_WARN((USBH_MTYPE_MSD_PHYS, "MSD: USBH_MSD_PHY_InitSequence: _ReadLunCapacity!"));
    goto Exit;
  }
  Status = _CheckModeParameters(pDev); // Check mode parameters
  if (Status) {
    USBH_WARN((USBH_MTYPE_MSD_PHYS, "MSD: USBH_MSD_PHY_InitSequence: _CheckModeParameters!"));
    goto Exit;
  }
  while (1) {
    Status = pDev->pPhysLayerAPI->pfTestUnitReady(&USBH_MSD_Global.aUnit[0]);
    if (Status == USBH_STATUS_SUCCESS) {
      break;
    }
  }
Exit:
  return Status;
}

/*********************************************************************
*
*       USBH_MSD_PHY_IsWriteProtected
*
*  Function description:
*    Checks if the specified unit is write protected.
*
*  Parameters:
*
*  Return value:
*    == TRUE     -  Drive is     write protected
*    == FALSE    -  Drive is not write protected
*/
USBH_BOOL USBH_MSD_PHY_IsWriteProtected(USBH_MSD_UNIT * pUnit) {
  if (pUnit->ModeParamHeader.DeviceParameter & MODE_WRITE_PROTECT_MASK) {
    return TRUE;
  } else {
    return FALSE;
  }
}

/********************************* EOF ******************************/
