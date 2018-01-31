/*********************************************************************
*  
*   IAR PowerPac
*
*   (c) Copyright IAR Systems 2010.  All rights reserved.
*
**********************************************************************
----------------------------------------------------------------------
File        : USBH_MSD_PhysLayerSubclass05.c
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

#define SFF8070_LENGTH                  12
#define SFF8070_CMD_INDEX               0
#define SFF8070_LUN_INDEX               1
#define SFF8070_BYTE_LENGTH_INDEX       4
#define SFF8070_START_STOP_PARAM_INDEX  4
#define SFF8070_WORD_LENGTH_OFS         7

typedef struct _SFF8070_CMD {
  U8  Cmd;      // 0 - aCommand
  U8  Lun;      // 1 - Bits 5..7 represent the LUN
  U32 Address;  // 2..5 - big endian address (if required)
  U8  Reserve1; // 6
  U16 Length;   // 7,8 - transfer or parameter list or allocation Length (if required)
  U8  Reserve2; // 9
  U8  Reserve3; // 10
  U8  Reserve4; // 11
} SFF8070_CMD;

typedef struct _SFF8070_EXTEND_CMD {
  U8  Cmd;      // 0 - aCommand
  U8  Lun;      // 1 - Bits 5..7 represent the LUN
  U32 Address;  // 2..5 - big endian address (if required)
  U32 Length;   // 6..9 - transfer or parameter list or allocation Length (if required)
  U8  Reserve3; // 10
  U8  Reserve4; // 11
} SFF8070_EXTEND_CMD;

typedef struct _SFF8070_EXTEND_CMD2 {
  U8 Cmd;       // 0 - aCommand
  U8 Lun;       // 1 - Bits 5..7 represent the LUN
  U8 Reserve2;  // 2
  U8 Reserve3;  // 3
  U8 Length;    // 4

  U8 Reserve4;  // 5
  U8 Reserve5;  // 6
  U8 Reserve6;  // 7
  U8 Reserve7;  // 8
  U8 Reserve8;  // 9

  U8 Reserve9;  // 10
  U8 Reserve10; // 11
} SFF8070_EXTEND_CMD2;

/*********************************************************************
*
*       _Conv_SFF8070_Command
*
*  Function description
*    Writes the opcode, the address and the transfer Length to the buffer
*
*  Parameters:
*    OpCode:                 SCSI operation code
*    Lun, Address:           logical block address
*    TransferLength, pBuffer: must have the SFF8070 12 byte Length
*/
static void _Conv_SFF8070_Command(U8 OpCode, U8 Lun, U32 Address, U16 TransferLength, U8 * pBuffer) {
  USBH_ASSERT_PTR(pBuffer);
  USBH_ZERO_MEMORY(pBuffer, SFF8070_LENGTH);
  pBuffer[0] = OpCode;
  pBuffer[1] = Lun;
  USBH_StoreU32BE(&pBuffer[2], Address);        // Address
  USBH_StoreU16BE(&pBuffer[7], TransferLength); // TransferLength
}

/*********************************************************************
*
*       _Inquiry
*
*  Function description
*    Returns product pData from the device / not supported by the SFF8070i
*
*  Return value:
*    IN:  maximum pData Length
*    OUT: Length of valid bytes in Data
*/
static USBH_STATUS _Inquiry(USBH_MSD_UNIT * pUnit, U8 * Data, U8 * pLength, INQUIRY_SELECT NotSupported, U8 NotSupported2) {
  U32         Length;
  U8          aCommand[SFF8070_LENGTH]; // SFF8070 standard aCommand Length
  USBH_STATUS Status;
  USBH_LOG((USBH_MTYPE_MSD_PHYS, "MSD      :USBH_MSD_P05Inquiry"));

  USBH_ASSERT_MAGIC(pUnit, USBH_MSD_UNIT);
  USBH_ASSERT_PTR(Data);
  USBH_ASSERT_PTR(pLength);

  // For later extensions, prevents compiler warnings
  NotSupported2 = NotSupported2;
  NotSupported  = NotSupported;

  if (pUnit->pDev == NULL) {
    USBH_WARN((USBH_MTYPE_MSD_PHYS, "MSD Error: USBH_MSD_P05Inquiry: Device removed!"));
    return USBH_STATUS_INVALID_PARAM;
  }
  USBH_ASSERT_MAGIC(pUnit->pDev, USBH_MSD_DEVICE);

  USBH_ZERO_ARRAY(aCommand);
  aCommand[SFF8070_CMD_INDEX]         = SC_INQUIRY;
  aCommand[SFF8070_LUN_INDEX]         = pUnit->Lun;
  aCommand[SFF8070_BYTE_LENGTH_INDEX] = *pLength;
  Length                             = *pLength;
  *pLength                            = 0;          // Reset pData Length
  Status = pUnit->pDev->pfCommandReadData(pUnit, (U8 *) &aCommand, sizeof(aCommand), Data, &Length, USBH_MSD_READ_TIMEOUT, FALSE);
  if (Status) {
    USBH_WARN((USBH_MTYPE_MSD_PHYS, "MSD Error: USBH_MSD_P05Inquiry:"));
    USBH_PRINT_STATUS_VALUE(USBH_MTYPE_MSD_PHYS, Status);
  } else {
    if (Length > 0xff) {
      return USBH_STATUS_LENGTH;
    } else {
      *pLength = (U8)Length;                       // Received inquiry block Length
    }
  }
  return Status;
}

/*********************************************************************
*
*       _ReadCapacity
*
*  Function description
*    Sends a standard READ CAPACITY aCommand to the device.
*    The result is stored in the parameters.
*/
static USBH_STATUS _ReadCapacity(USBH_MSD_UNIT * pUnit, U32 * MaxSectorAddress, U16 * BytesPerSector) {
  U32           Length;
  U8            aCommand[SFF8070_LENGTH];
  U8          * pData;
  //U8 pData[RD_CAPACITY_DATA_LENGTH];
  USBH_STATUS   Status;
  USBH_LOG((USBH_MTYPE_MSD_PHYS, "MSD      :USBH_MSD_P05ReadCapacity"));
  USBH_ASSERT_MAGIC(pUnit, USBH_MSD_UNIT);
  USBH_ASSERT_PTR(MaxSectorAddress);
  USBH_ASSERT_PTR(BytesPerSector);
  *MaxSectorAddress = 0;
  *BytesPerSector   = 0;
  if (pUnit->pDev == NULL) {
    USBH_WARN((USBH_MTYPE_MSD_PHYS, "MSD Error: USBH_MSD_P05ReadCapacity: Device removed!"));
    return USBH_STATUS_INVALID_PARAM;
  }
  USBH_ASSERT_MAGIC(pUnit->pDev, USBH_MSD_DEVICE);
  pData = (U8 *)USBH_URB_BufferAllocateTransferBuffer(RD_CAPACITY_DATA_LENGTH);
  if (NULL == pData) {
    USBH_WARN((USBH_MTYPE_MSD_PHYS, "MSD Error: USBH_MSD_P05ReadCapacity: no memory resources!"));
    return USBH_STATUS_RESOURCES;
  }
  Length = RD_CAPACITY_DATA_LENGTH;
  USBH_ZERO_ARRAY(aCommand);
  aCommand[SFF8070_CMD_INDEX] = SC_READ_CAPACITY;
  aCommand[SFF8070_LUN_INDEX] = pUnit->Lun;
  aCommand[SFF8070_BYTE_LENGTH_INDEX] = (U8)Length;
  Status = pUnit->pDev->pfCommandReadData(pUnit, aCommand, (U8)sizeof(aCommand), pData, &Length, USBH_MSD_READ_TIMEOUT, FALSE);
  if (Status) {
    USBH_WARN((USBH_MTYPE_MSD_PHYS, "MSD Error: USBH_MSD_P05ReadCapacity: pUnit->pDev->TlCommandReadData:"));
    USBH_PRINT_STATUS_VALUE(USBH_MTYPE_MSD_PHYS, Status);
  } else { // On success
    U32 sector_length;
    if (USBH_MSD_ConvReadCapacity(pData, (U16)Length, MaxSectorAddress, &sector_length)) {
      USBH_WARN((USBH_MTYPE_MSD_PHYS, "MSD Error: USBH_MSD_P05ReadCapacity: Length: %lu",Length));
    } else {
      *BytesPerSector = (U16)sector_length;
    }
  }
  USBH_URB_BufferFreeTransferBuffer(pData);
  return Status;
}

/*********************************************************************
*
*       _TestUnitReady
*
*  Function description
*    Checks if the device is ready, if the aCommand fails, a sense aCommand is generated.
*/
static USBH_STATUS _TestUnitReady(USBH_MSD_UNIT * pUnit) {
  U32         Length;
  U8          aCommand[SFF8070_LENGTH];
  USBH_STATUS Status;
  USBH_LOG((USBH_MTYPE_MSD_PHYS, "MSD      :USBH_MSD_P05TestUnitReady"));
  USBH_ASSERT_MAGIC(pUnit, USBH_MSD_UNIT);
  // Set the parameters
  USBH_ZERO_ARRAY(aCommand);
  aCommand[SFF8070_CMD_INDEX] = SC_TEST_UNIT_READY;
  aCommand[SFF8070_LUN_INDEX] = pUnit->Lun;
  Length = 0;
  Status = pUnit->pDev->pfCommandWriteData(pUnit, (U8 *) &aCommand, sizeof(aCommand), (U8 *)NULL, &Length, USBH_MSD_WRITE_TIMEOUT, FALSE); // No sector data
  if (Status) {
    USBH_WARN((USBH_MTYPE_MSD_PHYS, "MSD Error: USBH_MSD_P05TestUnitReady: pUnit->pDev->TlCommandWriteData"));
    USBH_PRINT_STATUS_VALUE(USBH_MTYPE_MSD_PHYS, Status);
  }
  return Status;
}

/*********************************************************************
*
*       _StartStopUnit
*
*  Function description
*    Sends a standard START STOP UNIT aCommand to the device
*/
static USBH_STATUS _StartStopUnit(USBH_MSD_UNIT * pUnit, USBH_BOOL Start) {
  U32         Length;
  U8          aCommand[SFF8070_LENGTH];
  USBH_STATUS Status;
  USBH_LOG((USBH_MTYPE_MSD_PHYS, "MSD      :USBH_MSD_P05StartStopUnit"));
  USBH_ASSERT_MAGIC(pUnit, USBH_MSD_UNIT);
  if (pUnit->pDev == NULL) {
    USBH_WARN((USBH_MTYPE_MSD_PHYS, "MSD Error: USBH_MSD_P05StartStopUnit: Device removed!"));
    return USBH_STATUS_INVALID_PARAM;
  }
  // Set the parameters
  USBH_ZERO_ARRAY(aCommand);
  aCommand[SFF8070_CMD_INDEX] = SC_START_STOP_UNIT;
  aCommand[SFF8070_LUN_INDEX] = pUnit->Lun;
  if (Start) {
    aCommand[SFF8070_START_STOP_PARAM_INDEX] = STARTSTOP_PWR_START;
  }
  Length = 0;
  Status = pUnit->pDev->pfCommandWriteData(pUnit, (U8 * ) &aCommand, sizeof(aCommand), (U8 * )NULL, &Length, USBH_MSD_WRITE_TIMEOUT, FALSE);
  if (Status) {
    USBH_WARN((USBH_MTYPE_MSD_PHYS, "MSD Error: USBH_MSD_P05StartStopUnit: pUnit->pDev->TlCommandWriteData:"));
    USBH_PRINT_STATUS_VALUE(USBH_MTYPE_MSD_PHYS, Status);
  }
  return Status;
}

/*********************************************************************
*
*       _RequestSense
*
*  Function description
*    Issues a REQUEST SENSE aCommand to receive the sense data for the
*    last requested aCommand. If the application client issues a aCommand
*    other than REQUEST SENSE, the sense data for the last aCommand is lost.
*
*  Return value:
*    ==0:  for success, sense data is copied to structure pUnit->sense
*    !=0:  for error
*/
static USBH_STATUS _RequestSense(USBH_MSD_UNIT * pUnit) {
  U32           Length;
  U8            aCommand[SFF8070_LENGTH];
  USBH_STATUS   Status;
  USBH_MSD_DEVICE  * pDev;
  U8          * senseBuffer;
  //U8   senseBuffer[STANDARD_SENSE_LENGTH];
  USBH_LOG((USBH_MTYPE_MSD_PHYS, "MSD :USBH_MSD_P05RequestSense"));
  pDev = pUnit->pDev;
  if (pDev == NULL) {
    USBH_WARN((USBH_MTYPE_MSD_PHYS, "MSD Error: USBH_MSD_P05RequestSense: Device removed!"));
    return USBH_STATUS_INVALID_PARAM;
  }
  USBH_ASSERT_MAGIC(pDev, USBH_MSD_DEVICE);
  USBH_ASSERT_PTR(pDev->hInterface);
  USBH_ZERO_ARRAY(aCommand);
  Length = STANDARD_SENSE_LENGTH;
  aCommand[SFF8070_CMD_INDEX] = SC_REQUEST_SENSE;
  aCommand[SFF8070_LUN_INDEX] = pUnit->Lun;
  aCommand[SFF8070_BYTE_LENGTH_INDEX] = (U8)Length;
  senseBuffer = (U8 *)USBH_URB_BufferAllocateTransferBuffer(STANDARD_SENSE_LENGTH);
  if (NULL == senseBuffer) {
    USBH_WARN((USBH_MTYPE_MSD_PHYS, "MSD Error: USBH_MSD_P05RequestSense: no memory resources!"));
    return USBH_STATUS_RESOURCES;
  }
  // Because USBH_MSD_P05RequestSense is used from pUnit->pDev->TlCommandReadData and pUnit->pDev->TlCommandWriteData it must call TlCommandReadData directly
  Status = pDev->pfCommandReadData(pUnit, aCommand, sizeof(aCommand), senseBuffer, &Length, USBH_MSD_READ_TIMEOUT, FALSE);
  if (Status) {
    pUnit->Sense.ResponseCode = 0; // invalidate the sense data
    USBH_WARN((USBH_MTYPE_MSD_PHYS, "MSD Error: USBH_MSD_P05RequestSense: TlCommandReadData:"));
    USBH_PRINT_STATUS_VALUE(USBH_MTYPE_MSD_PHYS, Status);
  } else { // On success
    USBH_MSD_ConvStandardSense(senseBuffer, (U16)Length, &pUnit->Sense);
  }
  USBH_URB_BufferFreeTransferBuffer(senseBuffer);
  return Status;
}

/*********************************************************************
*
*       _ModeSense
*
*  Function description
*    Returns some parameters of the device
*
*  Data buffer:
*    IN: max. Length in bytes of data; OUT: received Length
*    Converted mode parameter header values located at the beginning of the data buffer
*/
static USBH_STATUS _ModeSense(USBH_MSD_UNIT * pUnit, U8 * data, U8 * pLength, MODE_PARAMETER_HEADER * Header, U8 Page, U8 PageControlCode) {
  U32         Length;
  U8          aCommand[SFF8070_LENGTH];
  USBH_STATUS Status;
  USBH_LOG((USBH_MTYPE_MSD_PHYS, "MSD :USBH_MSD_P05ModeSense"));
  USBH_ASSERT_MAGIC(pUnit, USBH_MSD_UNIT);
  USBH_ASSERT_PTR(data);
  USBH_ASSERT_PTR(pLength);
  USBH_ASSERT_PTR(Header);
  if (pUnit->pDev == NULL) {
    USBH_WARN((USBH_MTYPE_MSD_PHYS, "MSD Error: USBH_MSD_P05ModeSense: Device removed!"));
    return USBH_STATUS_INVALID_PARAM;
  }
  USBH_ASSERT_MAGIC(pUnit->pDev, USBH_MSD_DEVICE);
  USBH_ASSERT_PTR(pUnit->pDev->hInterface);
  _Conv_SFF8070_Command(SC_MODE_SENSE_10, pUnit->Lun, 0, *pLength, aCommand );
  aCommand[PAGE_CODE_OFFSET] = (U8)(Page | PageControlCode);
  Length   = *pLength;
  *pLength = 0;
  Status = pUnit->pDev->pfCommandReadData(pUnit, (U8 * ) &aCommand, sizeof(aCommand), data, &Length, USBH_MSD_READ_TIMEOUT, FALSE); // No sector data
  if (Status) {
    USBH_WARN((USBH_MTYPE_MSD_PHYS, "MSD Error: USBH_MSD_P05ModeSense: pUnit->pDev->TlCommandReadData:"));
    USBH_PRINT_STATUS_VALUE(USBH_MTYPE_MSD_PHYS, Status);
  } else {
    if ((Length < SC_MODE_PARAMETER_HEADER_LENGTH_6) || (Length > 0xff)) {
      return USBH_STATUS_LENGTH;
    } else {
      *pLength = (U8)Length;
      USBH_MSD_ConvModeParameterHeader(Header, data, TRUE);                                                                   // TRUE = 6-byte aCommand
    }
  }
  return Status;
}

/*********************************************************************
*
*       _ReadSectors
*
*  Function description
*    Reads sectors from a device. The maximum number of sectors that can be read at once is 127!
*
*  Parameters:
*    pUnit:           Pointer to a structure that contains the LUN
*    SectorAddress:  Sector address of the first sector
*    data:           Data buffer
*    Sectors:        Number of contiguous logical blocks to read, max. 127
*/
static USBH_STATUS _ReadSectors(USBH_MSD_UNIT * pUnit, U32 SectorAddress, U8 * data, U16 Sectors) {
  U32         Length, OldLength;
  U8          aCommand[SFF8070_LENGTH];
  USBH_STATUS Status;
  USBH_LOG((USBH_MTYPE_MSD_PHYS, "MSD :USBH_MSD_P05Read10: address: %lu, sectors: %u",SectorAddress,Sectors));
  USBH_ASSERT_MAGIC(pUnit, USBH_MSD_UNIT);
  USBH_ASSERT_PTR(data);
  USBH_ASSERT(Sectors);
  if (pUnit->pDev == NULL) {
    USBH_WARN((USBH_MTYPE_MSD_PHYS, "MSD Error: USBH_MSD_P05Read10: Device removed!"));
    return USBH_STATUS_INVALID_PARAM;
  }
  if (SectorAddress > pUnit->MaxSectorAddress) {
    USBH_WARN((USBH_MTYPE_MSD_PHYS, "MSD Error: USBH_MSD_P05Read10: invalid sector address! max. address: %lu, used address: %lu", pUnit->MaxSectorAddress, SectorAddress));
    return USBH_STATUS_INVALID_PARAM;
  }
  _Conv_SFF8070_Command(SC_READ_10, pUnit->Lun, SectorAddress, Sectors, aCommand);
  OldLength = Length = Sectors * pUnit->BytesPerSector; // Determine the sector Length in bytes
  Status = pUnit->pDev->pfCommandReadData(pUnit, (U8 * ) &aCommand, sizeof(aCommand), data, &Length, USBH_MSD_READ_TIMEOUT, TRUE); // Sector data
  if (Status) {
    USBH_WARN((USBH_MTYPE_MSD_PHYS, "MSD Error: USBH_MSD_P05Read10: pUnit->pDev->TlCommandReadData:"));
    USBH_PRINT_STATUS_VALUE(USBH_MTYPE_MSD_PHYS, Status);
  } else {
    if (Length != OldLength) { // Not all sectors read
      USBH_WARN((USBH_MTYPE_MSD_PHYS, "MSD Error: USBH_MSD_P05Read10: bytes to read: %lu, bytes read: %lu",OldLength,Length));
      Status = USBH_STATUS_LENGTH;
    } else {
      USBH_LOG((USBH_MTYPE_MSD_PHYS, "MSD      :USBH_MSD_P05Read10: bytes read: %lu",Length));
    }
  }
  return Status;
}

/*********************************************************************
*
*       _WriteSectors
*
*  Function description
*    Writes sectors to a device. The maximum number of sectors that can be written at once is 127!
*
*  Parameters:
*   pUnit:           Pointer to a structure that contains the LUN
*   SectorAddress:  Sector address of the first sector
*   data:           Data buffer
*   Sectors:        Number of contiguous logical blocks written to the device
*
*/
static USBH_STATUS _WriteSectors(USBH_MSD_UNIT * pUnit, U32 SectorAddress, const U8 * data, U16 Sectors) {
  U32         Length, OldLength;
  U8          aCommand[SFF8070_LENGTH];
  USBH_STATUS Status;
  USBH_LOG((USBH_MTYPE_MSD_PHYS, "MSD :USBH_MSD_P05Write10: address: %lu, sectors: %lu",SectorAddress,Sectors));
  USBH_ASSERT_MAGIC(pUnit, USBH_MSD_UNIT);
  USBH_ASSERT_PTR(data);
  USBH_ASSERT(Sectors);
  if (SectorAddress > pUnit->MaxSectorAddress) {
    USBH_WARN((USBH_MTYPE_MSD_PHYS, "MSD Error: USBH_MSD_P05Write10: invalid sector address! max. address: %lu, used address: %lu",
    pUnit->MaxSectorAddress,SectorAddress));
    return USBH_STATUS_INVALID_PARAM;
  }
  if (USBH_MSD_PHY_IsWriteProtected(pUnit)) {                 // Check if unit is write protected
    return USBH_STATUS_WRITE_PROTECT;
  }
  OldLength = Length = Sectors * pUnit->BytesPerSector; // Length = sectors * bytes per sector
  _Conv_SFF8070_Command(SC_WRITE_10, pUnit->Lun, SectorAddress, Sectors, aCommand);
  Status = pUnit->pDev->pfCommandWriteData(pUnit, aCommand, sizeof(aCommand), data, &Length, USBH_MSD_WRITE_TIMEOUT, TRUE); // Sector pData
  if (Status) {
    USBH_WARN((USBH_MTYPE_MSD_PHYS, "MSD Error: USBH_MSD_P05Write10: pUnit->pDev->TlCommandWriteData:"));
    USBH_PRINT_STATUS_VALUE(USBH_MTYPE_MSD_PHYS, Status);
  } else {
    if (Length != OldLength) {                         // Error, the device must write all bytes
      USBH_WARN((USBH_MTYPE_MSD_PHYS, "MSD Error: USBH_MSD_P05Write10: bytes to write: %lu, bytes written: %lu",OldLength,Length));
      Status = USBH_STATUS_LENGTH;
    } else {
      USBH_LOG((USBH_MTYPE_MSD_PHYS, "MSD      :USBH_MSD_P05Write10: bytes written: %lu",Length));
    }
  }
  return Status;
}

const USBH_MSD_PHYS_LAYER_API USBH_MSD_PhysLayerSC05 = {
  USBH_MSD_PHY_InitSequence,
  _Inquiry,
  _ReadCapacity,
  _ReadSectors,
  _WriteSectors,
  _TestUnitReady,
  _StartStopUnit,
  _ModeSense,
  _RequestSense
};

/**************************************  EOF ****************************************/
