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
#include "USBH_Int.h"

#define SFF8070_LENGTH                  12
#define SFF8070_CMD_INDEX               0
#define SFF8070_LUN_INDEX               1
#define SFF8070_BYTE_LENGTH_INDEX       4
#define SFF8070_START_STOP_PARAM_INDEX  4
#define SFF8070_WORD_LENGTH_OFS         7

typedef struct _SFF8070_CMD {
  U8  Cmd;      // 0 - command
  U8  Lun;      // 1 - Bits 5..7 represent the LUN
  U32 Address;  // 2..5 - big endian address (if required)
  U8  Reserve1; // 6
  U16 Length;   // 7,8 - transfer or parameter list or allocation length (if required)
  U8  Reserve2; // 9
  U8  Reserve3; // 10
  U8  Reserve4; // 11
} SFF8070_CMD;

typedef struct _SFF8070_EXTEND_CMD {
  U8  Cmd;      // 0 - command
  U8  Lun;      // 1 - Bits 5..7 represent the LUN
  U32 Address;  // 2..5 - big endian address (if required)
  U32 Length;   // 6..9 - transfer or parameter list or allocation length (if required)
  U8  Reserve3; // 10
  U8  Reserve4; // 11
} SFF8070_EXTEND_CMD;

typedef struct _SFF8070_EXTEND_CMD2 {
  U8 Cmd;       // 0 - command
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
*    Writes the opcode, the address and the transfer length to the buffer
*
*  Parameters:
*    OpCode:                 SCSI operation code
*    Lun, Address:           logical block address
*    TransferLength, Buffer: must have the SFF8070 12 byte length
*/
static void _Conv_SFF8070_Command(U8 OpCode, U8 Lun, U32 Address, U16 TransferLength, U8 * Buffer) {
  T_ASSERT_PTR(Buffer);
  ZERO_MEMORY(Buffer, SFF8070_LENGTH);
  Buffer[0] = OpCode;
  Buffer[1] = Lun;
  USBH_StoreU32BE(&Buffer[2], Address);        // Address
  USBH_StoreU16BE(&Buffer[7], TransferLength); // TransferLength
}

/*********************************************************************
*
*       _Inquiry
*
*  Function description
*    Returns product data from the device / not supported by the SFF8070i
*
*  Return value:
*    IN:  maximum data length
*    OUT: length of valid bytes in Data
*/
static USBH_STATUS _Inquiry(USBH_MSD_UNIT * pUnit, U8 * Data, U8 * Length, INQUIRY_SELECT NotSupported, U8 NotSupported2) {
  U32         length;
  U8          command[SFF8070_LENGTH]; // SFF8070 standard command length
  USBH_STATUS status;
  USBH_LOG((USBH_MTYPE_MSD_PHYS, "MSD      :USBH_MSD_P05Inquiry"));

  T_ASSERT_MAGIC(pUnit, USBH_MSD_UNIT);
  T_ASSERT_PTR(Data);
  T_ASSERT_PTR(Length);

  // For later extensions, prevents compiler warnings
  NotSupported2 = NotSupported2;
  NotSupported  = NotSupported;

  if (pUnit->pDev == NULL) {
    USBH_WARN((USBH_MTYPE_MSD_PHYS, "MSD Error: USBH_MSD_P05Inquiry: Device removed!"));
    return USBH_STATUS_INVALID_PARAM;
  }
  T_ASSERT_MAGIC(pUnit->pDev, USBH_MSD_DEVICE);

  ZERO_ARRAY(command);
  command[SFF8070_CMD_INDEX]         = SC_INQUIRY;
  command[SFF8070_LUN_INDEX]         = pUnit->Lun;
  command[SFF8070_BYTE_LENGTH_INDEX] = *Length;
  length                             = *Length;
  *Length                            = 0;          // Reset data length
  status = pUnit->pDev->TlCommandReadData(pUnit, (U8 *) &command, sizeof(command), Data, &length, USBH_MSD_READ_TIMEOUT, FALSE);
  if (status) {
    USBH_WARN((USBH_MTYPE_MSD_PHYS, "MSD Error: USBH_MSD_P05Inquiry: pUnit->pDev->TlCommandReadData:"));
    USBH_PRINT_STATUS_VALUE(USBH_MTYPE_MSD_PHYS, status);
  } else {
    if (length > 0xff) {
      return USBH_STATUS_LENGTH;
    } else {
      * Length = (U8)length;                       // Received inquiry block length
    }
  }
  return status;
}

/*********************************************************************
*
*       _ReadFormatCapacity
*
*  Function description
*    Sends a standard READ FORMAT CAPACITY command to the device.
*    If successful the result is stored in pUnit.
*/
static USBH_STATUS _ReadFormatCapacity(USBH_MSD_UNIT * pUnit) {
  U32           length;
  U8            command[SFF8070_LENGTH];
  U8          * data;
  //U8    data[SC_READ_FORMAT_CAPACITY_DATA_LENGTH];
  USBH_STATUS   status;
  USBH_LOG((USBH_MTYPE_MSD_PHYS, "MSD :USBH_MSD_P05ReadFormatCapacity"));
  T_ASSERT_MAGIC(pUnit, USBH_MSD_UNIT);
  if (pUnit->pDev == NULL) {
    USBH_WARN((USBH_MTYPE_MSD_PHYS, "MSD Error: USBH_MSD_P05ReadFormatCapacity: Device removed!"));
    return USBH_STATUS_INVALID_PARAM;
  }
  T_ASSERT_MAGIC(pUnit->pDev, USBH_MSD_DEVICE);
  data = UrbBufferAllocateTransferBuffer(SC_READ_FORMAT_CAPACITY_DATA_LENGTH);
  if (NULL == data) {
    USBH_WARN((USBH_MTYPE_MSD_PHYS, "MSD Error: USBH_MSD_P05ReadFormatCapacity: no memory resources!"));
    return USBH_STATUS_RESOURCES;
  }
  length = SC_READ_FORMAT_CAPACITY_DATA_LENGTH;
  _Conv_SFF8070_Command(SC_READ_FORMAT_CAPACITY, pUnit->Lun, 0, (U16)length, command );
  status = pUnit->pDev->TlCommandReadData(pUnit, command, sizeof(command), data, &length, USBH_MSD_READ_TIMEOUT, FALSE);
  if (status) {
    USBH_WARN((USBH_MTYPE_MSD_PHYS, "MSD Error: USBH_MSD_P05ReadFormatCapacity: pUnit->pDev->TlCommandReadData:"));
    USBH_PRINT_STATUS_VALUE(USBH_MTYPE_MSD_PHYS, status);
  }
  UrbBufferFreeTransferBuffer(data);
  return status;
}

/*********************************************************************
*
*       _ReadCapacity
*
*  Function description
*    Sends a standard READ CAPACITY command to the device.
*    The result is stored in the parameters.
*/
static USBH_STATUS _ReadCapacity(USBH_MSD_UNIT * pUnit, U32 * MaxSectorAddress, U16 * BytesPerSector) {
  U32           length;
  U8            command[SFF8070_LENGTH];
  U8          * data;
  //U8 data[RD_CAPACITY_DATA_LENGTH];
  USBH_STATUS   status;
  USBH_LOG((USBH_MTYPE_MSD_PHYS, "MSD      :USBH_MSD_P05ReadCapacity"));
  T_ASSERT_MAGIC(pUnit, USBH_MSD_UNIT);
  T_ASSERT_PTR(MaxSectorAddress);
  T_ASSERT_PTR(BytesPerSector);
  *MaxSectorAddress = 0;
  *BytesPerSector   = 0;
  if (pUnit->pDev == NULL) {
    USBH_WARN((USBH_MTYPE_MSD_PHYS, "MSD Error: USBH_MSD_P05ReadCapacity: Device removed!"));
    return USBH_STATUS_INVALID_PARAM;
  }
  T_ASSERT_MAGIC(pUnit->pDev, USBH_MSD_DEVICE);
  data = UrbBufferAllocateTransferBuffer(RD_CAPACITY_DATA_LENGTH);
  if (NULL == data) {
    USBH_WARN((USBH_MTYPE_MSD_PHYS, "MSD Error: USBH_MSD_P05ReadCapacity: no memory resources!"));
    return USBH_STATUS_RESOURCES;
  }
  length = RD_CAPACITY_DATA_LENGTH;
  ZERO_ARRAY(command);
  command[SFF8070_CMD_INDEX] = SC_READ_CAPACITY;
  command[SFF8070_LUN_INDEX] = pUnit->Lun;
  command[SFF8070_BYTE_LENGTH_INDEX] = (U8)length;
  status = pUnit->pDev->TlCommandReadData(pUnit, command, (U8)sizeof(command), data, &length, USBH_MSD_READ_TIMEOUT, FALSE);
  if (status) {
    USBH_WARN((USBH_MTYPE_MSD_PHYS, "MSD Error: USBH_MSD_P05ReadCapacity: pUnit->pDev->TlCommandReadData:"));
    USBH_PRINT_STATUS_VALUE(USBH_MTYPE_MSD_PHYS, status);
  } else { // On success
    U32 sector_length;
    if (USBH_MSD_ConvReadCapacity(data, (U16)length, MaxSectorAddress, &sector_length)) {
      USBH_WARN((USBH_MTYPE_MSD_PHYS, "MSD Error: USBH_MSD_P05ReadCapacity: length: %lu",length));
    } else {
      *BytesPerSector = (U16)sector_length;
    }
  }
  UrbBufferFreeTransferBuffer(data);
  return status;
}

/*********************************************************************
*
*       _TestUnitReady
*
*  Function description
*    Checks if the device is ready, if the command fails, a sense command is generated.
*/
static USBH_STATUS _TestUnitReady(USBH_MSD_UNIT * pUnit) {
  U32         length;
  U8          command[SFF8070_LENGTH];
  USBH_STATUS status;
  USBH_LOG((USBH_MTYPE_MSD_PHYS, "MSD      :USBH_MSD_P05TestUnitReady"));
  T_ASSERT_MAGIC(pUnit, USBH_MSD_UNIT);
  // Set the parameters
  ZERO_ARRAY(command);
  command[SFF8070_CMD_INDEX] = SC_TEST_UNIT_READY;
  command[SFF8070_LUN_INDEX] = pUnit->Lun;
  length = 0;
  status = pUnit->pDev->TlCommandWriteData(pUnit, (U8 *) &command, sizeof(command), (U8 *)NULL, &length, USBH_MSD_WRITE_TIMEOUT, FALSE); // No sector data
  if (status) {
    USBH_WARN((USBH_MTYPE_MSD_PHYS, "MSD Error: USBH_MSD_P05TestUnitReady: pUnit->pDev->TlCommandWriteData"));
    USBH_PRINT_STATUS_VALUE(USBH_MTYPE_MSD_PHYS, status);
  }
  return status;
}

/*********************************************************************
*
*       _StartStopUnit
*
*  Function description
*    Sends a standard START STOP UNIT command to the device
*/
static USBH_STATUS _StartStopUnit(USBH_MSD_UNIT * pUnit, T_BOOL Start) {
  U32         length;
  U8          command[SFF8070_LENGTH];
  USBH_STATUS status;
  USBH_LOG((USBH_MTYPE_MSD_PHYS, "MSD      :USBH_MSD_P05StartStopUnit"));
  T_ASSERT_MAGIC(pUnit, USBH_MSD_UNIT);
  if (pUnit->pDev == NULL) {
    USBH_WARN((USBH_MTYPE_MSD_PHYS, "MSD Error: USBH_MSD_P05StartStopUnit: Device removed!"));
    return USBH_STATUS_INVALID_PARAM;
  }
  // Set the parameters
  ZERO_ARRAY(command);
  command[SFF8070_CMD_INDEX] = SC_START_STOP_UNIT;
  command[SFF8070_LUN_INDEX] = pUnit->Lun;
  if (Start) {
    command[SFF8070_START_STOP_PARAM_INDEX] = STARTSTOP_PWR_START;
  }
  length = 0;
  status = pUnit->pDev->TlCommandWriteData(pUnit, (U8 * ) &command, sizeof(command), (U8 * )NULL, &length, USBH_MSD_WRITE_TIMEOUT, FALSE);
  if (status) {
    USBH_WARN((USBH_MTYPE_MSD_PHYS, "MSD Error: USBH_MSD_P05StartStopUnit: pUnit->pDev->TlCommandWriteData:"));
    USBH_PRINT_STATUS_VALUE(USBH_MTYPE_MSD_PHYS, status);
  }
  return status;
}

/*********************************************************************
*
*       _RequestSense
*
*  Function description
*    Issues a REQUEST SENSE command to receive the sense data for the
*    last requested command. If the application client issues a command
*    other than REQUEST SENSE, the sense data for the last command is lost.
*
*  Return value:
*    ==0:  for success, sense data is copied to structure pUnit->sense
*    !=0:  for error
*/
static USBH_STATUS _RequestSense(USBH_MSD_UNIT * pUnit) {
  U32           length;
  U8            command[SFF8070_LENGTH];
  USBH_STATUS   status;
  USBH_MSD_DEVICE  * dev;
  U8          * senseBuffer;
  //U8   senseBuffer[STANDARD_SENSE_LENGTH];
  USBH_LOG((USBH_MTYPE_MSD_PHYS, "MSD :USBH_MSD_P05RequestSense"));
  dev = pUnit->pDev;
  if (dev == NULL) {
    USBH_WARN((USBH_MTYPE_MSD_PHYS, "MSD Error: USBH_MSD_P05RequestSense: Device removed!"));
    return USBH_STATUS_INVALID_PARAM;
  }
  T_ASSERT_MAGIC(dev, USBH_MSD_DEVICE);
  T_ASSERT_PTR(dev->InterfaceHandle);
  ZERO_ARRAY(command);
  length = STANDARD_SENSE_LENGTH;
  command[SFF8070_CMD_INDEX] = SC_REQUEST_SENSE;
  command[SFF8070_LUN_INDEX] = pUnit->Lun;
  command[SFF8070_BYTE_LENGTH_INDEX] = (U8)length;
  senseBuffer = UrbBufferAllocateTransferBuffer(STANDARD_SENSE_LENGTH);
  if (NULL == senseBuffer) {
    USBH_WARN((USBH_MTYPE_MSD_PHYS, "MSD Error: USBH_MSD_P05RequestSense: no memory resources!"));
    return USBH_STATUS_RESOURCES;
  }
  // Because USBH_MSD_P05RequestSense is used from pUnit->pDev->TlCommandReadData and pUnit->pDev->TlCommandWriteData it must call TlCommandReadData directly
  status = dev->TlCommandReadData(pUnit, command, sizeof(command), senseBuffer, &length, USBH_MSD_READ_TIMEOUT, FALSE);
  if (status) {
    pUnit->Sense.ResponseCode = 0; // invalidate the sense data
    USBH_WARN((USBH_MTYPE_MSD_PHYS, "MSD Error: USBH_MSD_P05RequestSense: TlCommandReadData:"));
    USBH_PRINT_STATUS_VALUE(USBH_MTYPE_MSD_PHYS, status);
  } else { // On success
    USBH_MSD_ConvStandardSense(senseBuffer, (U16)length, &pUnit->Sense);
  }
  UrbBufferFreeTransferBuffer(senseBuffer);
  return status;
}

/*********************************************************************
*
*       _ModeSense
*
*  Function description
*    Returns some parameters of the device
*
*  Data buffer:
*    IN: max. length in bytes of data; OUT: received length
*    Converted mode parameter header values located at the beginning of the data buffer
*/
static USBH_STATUS _ModeSense(USBH_MSD_UNIT * pUnit, U8 * data, U8 * Length, MODE_PARAMETER_HEADER * Header, U8 Page, U8 PageControlCode) {
  U32         length;
  U8          command[SFF8070_LENGTH];
  USBH_STATUS status;
  USBH_LOG((USBH_MTYPE_MSD_PHYS, "MSD :USBH_MSD_P05ModeSense"));
  T_ASSERT_MAGIC(pUnit, USBH_MSD_UNIT);
  T_ASSERT_PTR(data);
  T_ASSERT_PTR(Length);
  T_ASSERT_PTR(Header);
  if (pUnit->pDev == NULL) {
    USBH_WARN((USBH_MTYPE_MSD_PHYS, "MSD Error: USBH_MSD_P05ModeSense: Device removed!"));
    return USBH_STATUS_INVALID_PARAM;
  }
  T_ASSERT_MAGIC(pUnit->pDev, USBH_MSD_DEVICE);
  T_ASSERT_PTR(pUnit->pDev->InterfaceHandle);
  _Conv_SFF8070_Command(SC_MODE_SENSE_10, pUnit->Lun, 0, *Length, command );
  command[PAGE_CODE_OFFSET] = (U8)(Page | PageControlCode);
  length   = *Length;
  * Length = 0;
  status = pUnit->pDev->TlCommandReadData(pUnit, (U8 * ) &command, sizeof(command), data, &length, USBH_MSD_READ_TIMEOUT, FALSE); // No sector data
  if (status) {
    USBH_WARN((USBH_MTYPE_MSD_PHYS, "MSD Error: USBH_MSD_P05ModeSense: pUnit->pDev->TlCommandReadData:"));
    USBH_PRINT_STATUS_VALUE(USBH_MTYPE_MSD_PHYS, status);
  } else {
    if ((length < SC_MODE_PARAMETER_HEADER_LENGTH_6) || (length > 0xff)) {
      return USBH_STATUS_LENGTH;
    } else {
      * Length = (U8)length;
      USBH_MSD_ConvModeParameterHeader(Header, data, TRUE);                                                                   // TRUE = 6-byte command
    }
  }
  return status;
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
  U32         length, oldLength;
  U8          command[SFF8070_LENGTH];
  USBH_STATUS status;
  USBH_LOG((USBH_MTYPE_MSD_PHYS, "MSD :USBH_MSD_P05Read10: address: %lu, sectors: %u",SectorAddress,Sectors));
  T_ASSERT_MAGIC(pUnit, USBH_MSD_UNIT);
  T_ASSERT_PTR(data);
  T_ASSERT(Sectors);
  if (pUnit->pDev == NULL) {
    USBH_WARN((USBH_MTYPE_MSD_PHYS, "MSD Error: USBH_MSD_P05Read10: Device removed!"));
    return USBH_STATUS_INVALID_PARAM;
  }
  if (SectorAddress > pUnit->MaxSectorAddress) {
    USBH_WARN((USBH_MTYPE_MSD_PHYS, "MSD Error: USBH_MSD_P05Read10: invalid sector address! max. address: %lu, used address: %lu", pUnit->MaxSectorAddress, SectorAddress));
    return USBH_STATUS_INVALID_PARAM;
  }
  _Conv_SFF8070_Command(SC_READ_10, pUnit->Lun, SectorAddress, Sectors, command);
  oldLength = length = Sectors * pUnit->BytesPerSector; // Determine the sector length in bytes
  status = pUnit->pDev->TlCommandReadData(pUnit, (U8 * ) &command, sizeof(command), data, &length, USBH_MSD_READ_TIMEOUT, TRUE); // Sector data
  if (status) {
    USBH_WARN((USBH_MTYPE_MSD_PHYS, "MSD Error: USBH_MSD_P05Read10: pUnit->pDev->TlCommandReadData:"));
    USBH_PRINT_STATUS_VALUE(USBH_MTYPE_MSD_PHYS, status);
  } else {
    if (length != oldLength) { // Not all sectors read
      USBH_WARN((USBH_MTYPE_MSD_PHYS, "MSD Error: USBH_MSD_P05Read10: bytes to read: %lu, bytes read: %lu",oldLength,length));
      status = USBH_STATUS_LENGTH;
    } else {
      USBH_LOG((USBH_MTYPE_MSD_PHYS, "MSD      :USBH_MSD_P05Read10: bytes read: %lu",length));
    }
  }
  return status;
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
  U32         length, oldLength;
  U8          command[SFF8070_LENGTH];
  USBH_STATUS status;
  USBH_LOG((USBH_MTYPE_MSD_PHYS, "MSD :USBH_MSD_P05Write10: address: %lu, sectors: %lu",SectorAddress,Sectors));
  T_ASSERT_MAGIC(pUnit, USBH_MSD_UNIT);
  T_ASSERT_PTR(data);
  T_ASSERT(Sectors);
  if (SectorAddress > pUnit->MaxSectorAddress) {
    USBH_WARN((USBH_MTYPE_MSD_PHYS, "MSD Error: USBH_MSD_P05Write10: invalid sector address! max. address: %lu, used address: %lu",
    pUnit->MaxSectorAddress,SectorAddress));
    return USBH_STATUS_INVALID_PARAM;
  }
  if (USBH_MSD_PHY_IsWriteProtected(pUnit)) {                 // Check if unit is write protected
    return USBH_STATUS_WRITE_PROTECT;
  }
  oldLength = length = Sectors * pUnit->BytesPerSector; // Length = sectors * bytes per sector
  _Conv_SFF8070_Command(SC_WRITE_10, pUnit->Lun, SectorAddress, Sectors, command);
  status = pUnit->pDev->TlCommandWriteData(pUnit, command, sizeof(command), data, &length, USBH_MSD_WRITE_TIMEOUT, TRUE); // Sector data
  if (status) {
    USBH_WARN((USBH_MTYPE_MSD_PHYS, "MSD Error: USBH_MSD_P05Write10: pUnit->pDev->TlCommandWriteData:"));
    USBH_PRINT_STATUS_VALUE(USBH_MTYPE_MSD_PHYS, status);
  } else {
    if (length != oldLength) {                         // Error, the device must write all bytes
      USBH_WARN((USBH_MTYPE_MSD_PHYS, "MSD Error: USBH_MSD_P05Write10: bytes to write: %lu, bytes written: %lu",oldLength,length));
      status = USBH_STATUS_LENGTH;
    } else {
      USBH_LOG((USBH_MTYPE_MSD_PHYS, "MSD      :USBH_MSD_P05Write10: bytes written: %lu",length));
    }
  }
  return status;
}

const USBH_MSD_PHYS_LAYER_API USBH_MSD_PhysLayerSC05 = {
  USBH_MSD_PHY_InitSequence,
  _Inquiry,
  _ReadFormatCapacity,
  _ReadCapacity,
  _ReadSectors,
  _WriteSectors,
  _TestUnitReady,
  _StartStopUnit,
  _ModeSense,
  _RequestSense
};

/**************************************  EOF ****************************************/
