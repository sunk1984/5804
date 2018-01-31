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
#include "USBH_Int.h"

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
  { INQUIRY_DIRECT_DEVICE,  "INQUIRY_DIRECT_DEVICE" },
  { INQUIRY_SEQ_DEVICE,     "INQUIRY_SEQ_DEVICE" },
  { INQUIRY_WRITE_ONCE,     "INQUIRY_WRITE_ONCE" },
  { INQUIRY_CD_ROM,         "INQUIRY_CD_ROM" },
  { INQUIRY_NON_CD_OPTICAL, "INQUIRY_NON_CD_OPTICAL" }
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
*       _PlWCheckInquiryData
*
*  Function description:
*    TBD
*/
static T_BOOL _PlWCheckInquiryData(INQUIRY_STANDARD_RESPONSE * data) {
  if ((data->DeviceType &INQUIRY_DEVICE_TYPE_MASK) != INQUIRY_DIRECT_DEVICE) { // No direct access device
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

  T_ASSERT_MAGIC(pDev, USBH_MSD_DEVICE);
  pBuffer = UrbBufferAllocateTransferBuffer(STANDARD_INQUIRY_DATA_LENGTH);
  if (NULL == pBuffer) {
    USBH_WARN((USBH_MTYPE_MSD_PHYS, "MSD:  USBH_MSD_PlWInquiryDevice: no memory resources!"));
    return USBH_STATUS_RESOURCES;
  }
  Status = USBH_STATUS_ERROR;
  pUnit   = &USBH_MSD_Global.aUnit[0];
  for (i = 0; i < USBH_MSD_MAX_UNITS; i++, pUnit++) { // Call all units
    if (pUnit->pDev == pDev) {
      Size   = STANDARD_INQUIRY_DATA_LENGTH;
      Status = pDev->pPhysLayerAPI->PlInquiry(pUnit, pBuffer, &Size, Standard, 0);
      if (Status) { // On error
        USBH_WARN((USBH_MTYPE_MSD_PHYS, "MSD: USBH_MSD_PlWInquiryDevice: LUN: %u",pUnit->Lun));
        USBH_PRINT_STATUS_VALUE(USBH_MTYPE_MSD_PHYS, Status);
      } else { // Success, store parameters in the pUnit
        USBH_MEMCPY(&pUnit->InquiryData, pBuffer, sizeof(pUnit->InquiryData));
        USBH_MSD_PLW_PRINT_INQUIRYDATA(&pUnit->InquiryData);
        if (!_PlWCheckInquiryData(&pUnit->InquiryData)) {
          //
          // This LUN can not be handled by us since it is not a direct access device (CDROM/DVD)
          //
          USBH_WARN((USBH_MTYPE_MSD_PHYS, "MSD: USBH_MSD_PlWInquiryDevice: USBH_MSD_PlWCheckInquiryData!"));
          //Status = USBH_STATUS_ERROR;
          pUnit->pDev = NULL;
          pDev->UnitCnt--;
        }
      }
    }
  }
  UrbBufferFreeTransferBuffer(pBuffer);
  return Status;
}

/*********************************************************************
*
*       _ReadFormatCapacity
*
*  Function description:
*    Executes a READ FORMAT CAPACITY command.
*
*  Return value:
*    == 0        -  Success
*    != 0        -  Error
*/
static USBH_STATUS _ReadFormatCapacity(USBH_MSD_DEVICE * pDev) {
  int             i;
  USBH_MSD_UNIT * pUnit;
  USBH_STATUS     Status;
  unsigned        NumRetries = 20;

  Status      = USBH_STATUS_ERROR;
  pUnit        = &USBH_MSD_Global.aUnit[0];
  for (i = 0; i < USBH_MSD_MAX_UNITS; i++, pUnit++) {
    if (pUnit->pDev == pDev) {
Retry:
      Status = pDev->pPhysLayerAPI->PlReadFormatCapacity(pUnit);
      if (Status) {
        Status = pDev->pPhysLayerAPI->PlRequestSense(pUnit);
        if (Status == USBH_STATUS_SUCCESS) {
          if (pUnit->Sense.Sensekey == SS_SENSE_UNIT_ATTENTION) {
            USBH_WARN((USBH_MTYPE_MSD_PHYS, "MSD: Lun %d is not ready.", i));
            if (NumRetries--) {
              goto Retry;
            }
          }
        }

        USBH_WARN((USBH_MTYPE_MSD_PHYS, "MSD: USBH_MSD_PlWReadFormatCapacity LUN :%d",pUnit->Lun));
        USBH_PRINT_STATUS_VALUE(USBH_MTYPE_MSD_PHYS, Status);
      }
      break; // On error
    }
  }
  return Status;
}

/*********************************************************************
*
*       _ReadLunCapacity
*
*  Function description:
*    Executes a READ CAPACITY command on all logical units of the device.
*
*  Parameters:
*    pDev    - 
*  
*  Return value:
*    == 0        -  Success (sector Size and max sector address successfully obtained)
*    != 0        -  Error
*/
static USBH_STATUS _ReadLunCapacity(USBH_MSD_DEVICE * pDev) {
  int             i;
  USBH_MSD_UNIT * pUnit;
  USBH_STATUS     Status;

  Status      = USBH_STATUS_ERROR;
  pUnit        = &USBH_MSD_Global.aUnit[0];
  for (i = 0; i < USBH_MSD_MAX_UNITS; i++, pUnit++) {
    if (pUnit->pDev == pDev) {
      Status = pDev->pPhysLayerAPI->PlCapacity(pUnit, &pUnit->MaxSectorAddress, &pUnit->BytesPerSector); // Read the capacity of the logical unit
      if (Status) { // Error, break
        USBH_WARN((USBH_MTYPE_MSD_PHYS, "MSD: _ReadLunCapacity: PlCapacity: LUN: %u ",pUnit->Lun));
        USBH_PRINT_STATUS_VALUE(USBH_MTYPE_MSD_PHYS, Status);
        break;
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

  pBuffer = UrbBufferAllocateTransferBuffer(MODE_SENSE_PARAMETER_LENGTH);
  if (NULL == pBuffer) {
    USBH_WARN((USBH_MTYPE_MSD_PHYS, "MSD: USBH_MSD_PlWCheckModeParameters: no memory resources!"));
    return USBH_STATUS_RESOURCES;
  }
  Status = USBH_STATUS_ERROR;
  pUnit   = &USBH_MSD_Global.aUnit[0];
  for (i = 0; i < USBH_MSD_MAX_UNITS; i++, pUnit++) { // Call all units
    if (pUnit->pDev == pDev) {
      Size = sizeof(MODE_PARAMETER_HEADER);
      Status = pDev->pPhysLayerAPI->PlModeSense(pUnit, pBuffer, &Size, &ModeHeader, MODE_SENSE_RETURN_ALL_PAGES, 0);
      if (Status) { // On error
        USBH_WARN((USBH_MTYPE_MSD_PHYS, "MSD: USBH_MSD_PlWCheckModeParameter: PlModeSense!"));
        if (pDev->pPhysLayerAPI->PlRequestSense(pUnit) == USBH_STATUS_SUCCESS) {
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
  UrbBufferFreeTransferBuffer(pBuffer);
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

  T_ASSERT_MAGIC(pDev, USBH_MSD_DEVICE);
  Status = _InquiryDevice(pDev);
  if (Status) {
    USBH_WARN((USBH_MTYPE_MSD_PHYS, "MSD: USBH_MSD_PHY_InitSequence: _InquiryDevice!"));
    goto Exit;
  }
  Status = _ReadFormatCapacity(pDev);  // Query the capacity for this device
  if (Status) { // On error
    if ((Status != USBH_STATUS_SENSE_STOP) && (Status != USBH_STATUS_SENSE_REPEAT) && (Status != USBH_STATUS_COMMAND_FAILED)) {
      //
      // Transfer error, stop the initialization of the device
      //
      USBH_WARN((USBH_MTYPE_MSD_PHYS, "MSD: USBH_MSD_PHY_InitSequence: _ReadFormatCapacity!"));
      goto Exit;
    }
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
T_BOOL USBH_MSD_PHY_IsWriteProtected(USBH_MSD_UNIT * pUnit) {
  if (pUnit->ModeParamHeader.DeviceParameter & MODE_WRITE_PROTECT_MASK) {
    return TRUE;
  } else {
    return FALSE;
  }
}

/********************************* EOF ******************************/
