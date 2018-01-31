/*********************************************************************
*  
*   IAR PowerPac
*
*   (c) Copyright IAR Systems 2010.  All rights reserved.
*
**********************************************************************
----------------------------------------------------------------------
File        : USBH_MSD_PhysLayerSubclass06.c
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

// If an command struct consists only of bytes the struct is used as command buffer,
// else a command buffer is used and an additional convert function is called!
// This prevent the use of an certain struct alignment.

/*********************************************************************
*
*       _Inquiry
*
*  Function description
*    Returns product pData from the device
*
*  Parameters:
*    pData:    IN: -                  ; OUT: pData
*    Length:  IN: Maximum pData Length; OUT: Length of valid bytes in pData
*    Select:  Select the returned pData format
*    CmdPage: Specify the page number, not used if the parameter Select is equal Standard
*/
static USBH_STATUS _Inquiry(USBH_MSD_UNIT * pUnit, U8 * pData, U8 * pLength, INQUIRY_SELECT Select, U8 CmdPage) {
  U32            Length;
  SCSI_6BYTE_CMD aCommand;
  USBH_STATUS    Status;

  USBH_LOG((USBH_MTYPE_MSD_PHYS, "MSD SC6: _Inquiry"));
  T_ASSERT_MAGIC(pUnit, USBH_MSD_UNIT);
  T_ASSERT_PTR(pData);
  T_ASSERT_PTR(pLength);
  if (pUnit->pDev == NULL) {
    USBH_WARN((USBH_MTYPE_MSD_PHYS, "MSD SC6: _Inquiry: Device removed!"));
    return USBH_STATUS_INVALID_PARAM;
  }
  T_ASSERT_MAGIC(pUnit->pDev, USBH_MSD_DEVICE);
  T_ASSERT_PTR(pUnit->pDev->InterfaceHandle);
  ZERO_STRUCT(aCommand);
  aCommand.Cmd    = SC_INQUIRY;
  aCommand.Length = *pLength;
  Length          = *pLength;
  *pLength        = 0;          // Reset pData pLength
  Status          = 0;
  switch (Select) {
    case Standard:
      break;
    case Productpage:
      aCommand.idx1 = INQUIRY_ENABLE_PRODUCT_DATA;
      aCommand.idx2 = CmdPage;
      break;
    case CommandSupport:
      aCommand.idx1 = INQUIRY_ENABLE_COMMAND_SUPPORT;
      aCommand.idx2 = CmdPage;
      break;
    default:
      USBH_WARN((USBH_MTYPE_MSD_PHYS, "MSD SC6: _Inquiry: invalid value for parameter Select!"));
      Status = USBH_STATUS_INVALID_PARAM;
      break;
  }
  if (Status) { // On error
    return Status;
  }
  Status = pUnit->pDev->TlCommandReadData(pUnit, (U8 * ) &aCommand, sizeof(aCommand), pData, &Length, USBH_MSD_READ_TIMEOUT, FALSE);
  if (Status) {
    USBH_WARN((USBH_MTYPE_MSD_PHYS, "MSD SC6: _Inquiry: pUnit->pDev->TlCommandReadData"));
    USBH_PRINT_STATUS_VALUE(USBH_MTYPE_MSD_PHYS, Status);
  } else {
    if (Length > 0xff) {
      return USBH_STATUS_LENGTH;
    } else {   // Received inquiry block pLength
      *pLength = (U8)Length;
    }
  }
  return Status;
}

/*********************************************************************
*
*       USBH_MSD_Conv10ByteCommand
*
*  Function description
*    Returns a 10 byte aCommand descriptor block
*    IN: valid pointer; OUT: pData in descriptor
*/
static void USBH_MSD_Conv10ByteCommand(U8 OpCode, U32 Address, U16 Length, U8 * pCommand) {
  T_ASSERT_PTR(pCommand);
  ZERO_MEMORY(pCommand, SCSI_10BYTE_COMMAND_LENGTH);
  pCommand[0] = OpCode;
  USBH_StoreU32BE(&pCommand[2], Address); // Address
  USBH_StoreU16BE(&pCommand[7], Length);  // TransferLength
}

/*********************************************************************
*
*       _ReadFormatCapacity
*
*  Function description
*    Sends a standard READ FORMAT CAPACITY aCommand to the device.
*    If successful the result is stored in Unit.
*/
static USBH_STATUS _ReadFormatCapacity(USBH_MSD_UNIT * pUnit) {
  USBH_MSD_DEVICE * pDev;
  U32               Length;
  U8                aCommand[SCSI_10BYTE_COMMAND_LENGTH];
  U8              * pData = NULL;
  USBH_STATUS       Status;

  USBH_LOG((USBH_MTYPE_MSD_PHYS, "MSD SC6: _ReadFormatCapacity"));
  T_ASSERT_MAGIC(pUnit, USBH_MSD_UNIT);
  pDev = pUnit->pDev;
  if (pDev == NULL) {
    USBH_WARN((USBH_MTYPE_MSD_PHYS, "MSD SC6: _ReadFormatCapacity: Device removed!"));
    return USBH_STATUS_INVALID_PARAM;
  }
  T_ASSERT_MAGIC(pDev, USBH_MSD_DEVICE);
  T_ASSERT_PTR(pDev->InterfaceHandle);
  Length = SC_READ_FORMAT_CAPACITY_DATA_LENGTH;
  USBH_MSD_Conv10ByteCommand(SC_READ_FORMAT_CAPACITY, 0, (U16)Length, aCommand);
  pData = UrbBufferAllocateTransferBuffer(SC_READ_FORMAT_CAPACITY_DATA_LENGTH);
  if (NULL == pData) {
    USBH_WARN((USBH_MTYPE_MSD_PHYS, "MSD SC6: _ReadFormatCapacity: no memory resources!"));
    return USBH_STATUS_RESOURCES;
  }
  Status = pUnit->pDev->TlCommandReadData(pUnit, aCommand, sizeof(aCommand), pData, &Length, USBH_MSD_READ_TIMEOUT, FALSE);
  if (Status) {
    USBH_WARN((USBH_MTYPE_MSD_PHYS, "MSD SC6: _ReadFormatCapacity: Unit->Dev->TlCommandReadData"));
    USBH_PRINT_STATUS_VALUE(USBH_MTYPE_MSD_PHYS, Status);
  }
  UrbBufferFreeTransferBuffer(pData);
  return Status;
}

/*********************************************************************
*
*       _ReadCapacity
*
*  Function description
*    Sends a standard READ CAPACITY aCommand to the device.
*    The result is stored in the parameters.
*
*  Parameters:
*    pBytesPerSector: Sector size in bytes
*/
static USBH_STATUS _ReadCapacity(USBH_MSD_UNIT * pUnit, U32 * pMaxSectorAddress, U16 * pBytesPerSector) {
  U32           Length;
  U8            aCommand[SCSI_10BYTE_COMMAND_LENGTH];
  U8          * pData;
  USBH_STATUS   Status;

  USBH_LOG((USBH_MTYPE_MSD_PHYS, "MSD SC6: _ReadCapacity"));
  T_ASSERT_MAGIC(pUnit, USBH_MSD_UNIT);
  T_ASSERT_PTR(pMaxSectorAddress);
  T_ASSERT_PTR(pBytesPerSector);
  if (pUnit->pDev == NULL) {
    USBH_WARN((USBH_MTYPE_MSD_PHYS, "MSD SC6: _ReadCapacity: Device removed!"));
    return USBH_STATUS_INVALID_PARAM;
  }
#if (USBH_DEBUG > 1)
  {
    USBH_MSD_DEVICE * pDev;
    pDev = pUnit->pDev;
    T_ASSERT_MAGIC(pDev, USBH_MSD_DEVICE);
    T_ASSERT_PTR(pDev->InterfaceHandle);
  }
#endif
  *pMaxSectorAddress = 0;
  *pBytesPerSector   = 0;
  Length             = RD_CAPACITY_DATA_LENGTH;
  pData               = UrbBufferAllocateTransferBuffer(RD_CAPACITY_DATA_LENGTH);
  if (NULL == pData) {
    USBH_WARN((USBH_MTYPE_MSD_PHYS, "MSD SC6: _ReadCapacity: no memory resources!"));
    return USBH_STATUS_RESOURCES;
  }
  // The Length field in the SCSI aCommand must be zero
  USBH_MSD_Conv10ByteCommand(SC_READ_CAPACITY, 0, 0, aCommand);
  Status = pUnit->pDev->TlCommandReadData(pUnit, aCommand, (U8)sizeof(aCommand), pData, &Length, USBH_MSD_READ_TIMEOUT, FALSE);
  if (Status) {
    USBH_WARN((USBH_MTYPE_MSD_PHYS, "MSD SC6: _ReadCapacity: pUnit->pDev->TlCommandReadData"));
    USBH_PRINT_STATUS_VALUE(USBH_MTYPE_MSD_PHYS, Status);
  } else { // On success
    U32 bytesPersector;
    if (USBH_MSD_ConvReadCapacity(pData, (U16)Length, pMaxSectorAddress, &bytesPersector)) {
      USBH_WARN((USBH_MTYPE_MSD_PHYS, "MSD SC6: _ReadCapacity: Length: %lu",Length));
    } else {
      * pBytesPerSector = (U16)bytesPersector;
    }
  }
  UrbBufferFreeTransferBuffer(pData);
  return Status;
}

/*********************************************************************
*
*       _TestUnitReady
*
*  Function description
*    Checks if the device is ready, if the command fails, a sense command is generated.
*/
static USBH_STATUS _TestUnitReady(USBH_MSD_UNIT * pUnit) {
  U32            length;
  SCSI_6BYTE_CMD command;
  USBH_STATUS    status;
  USBH_LOG((USBH_MTYPE_MSD_PHYS, "MSD SC6: _TestUnitReady"));
  T_ASSERT_MAGIC(pUnit, USBH_MSD_UNIT);
  ZERO_STRUCT(command);
  command.Cmd = SC_TEST_UNIT_READY;
  length = 0;
  status = pUnit->pDev->TlCommandWriteData(pUnit, (U8 * ) &command, sizeof(command), (U8 * )NULL, &length, USBH_MSD_WRITE_TIMEOUT, FALSE);
  if (status) {
    USBH_WARN((USBH_MTYPE_MSD_PHYS, "MSD SC6: _TestUnitReady: pUnit->pDev->TlCommandWriteData:"));
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
  U32              length;
  SCSI_6BYTE_CMD   command;
  USBH_STATUS      status;
  USBH_MSD_DEVICE     * dev;
  USBH_LOG((USBH_MTYPE_MSD_PHYS, "MSD SC6: _StartStopUnit"));
  T_ASSERT_MAGIC(pUnit, USBH_MSD_UNIT);
  if (pUnit->pDev == NULL) {
    USBH_WARN((USBH_MTYPE_MSD_PHYS, "MSD SC6: _StartStopUnit: Device removed!"));
    return USBH_STATUS_INVALID_PARAM;
  }
  dev = pUnit->pDev;
  T_ASSERT_MAGIC(dev, USBH_MSD_DEVICE);
  T_ASSERT_PTR(dev->InterfaceHandle);
  ZERO_STRUCT(command);
  command.Cmd = SC_START_STOP_UNIT;
  if (Start) {
    command.Length |= STARTSTOP_PWR_START;
  }
  length = 0;
  status = dev->TlCommandWriteData(pUnit, (U8 * ) &command, sizeof(command), (U8 * )NULL, &length, USBH_MSD_WRITE_TIMEOUT, FALSE);
  if (status) {
    USBH_WARN((USBH_MTYPE_MSD_PHYS, "MSD SC6: _StartStopUnit: TlCommandWriteData:"));
    USBH_PRINT_STATUS_VALUE(USBH_MTYPE_MSD_PHYS, status);
  }
  return status;
}

/*********************************************************************
*
*       _ModeSense
*
*  Function description
*    Returns some parameters of the device
*
*  Parameters:
*    pData:   pData buffer
*    pLength: IN: max. length in bytes of pData; OUT: received length
*    pHeader: Converted mode parameter header values located at the beginning of the pData buffer
*/
static USBH_STATUS _ModeSense(USBH_MSD_UNIT * pUnit, U8 * pData, U8 * pLength, MODE_PARAMETER_HEADER * pHeader, U8 Page, U8 PageControlCode) {
  U32            length;
  SCSI_6BYTE_CMD command; // Byte array, no converting is needed
  USBH_STATUS    status;

  USBH_LOG((USBH_MTYPE_MSD_PHYS, "MSD SC6: _ModeSense"));
  T_ASSERT_MAGIC(pUnit, USBH_MSD_UNIT);
  T_ASSERT_PTR(pData);
  T_ASSERT_PTR(pLength);
  T_ASSERT_PTR(pHeader);
  if (pUnit->pDev == NULL) {
    USBH_WARN((USBH_MTYPE_MSD_PHYS, "MSD SC6: _ModeSense: Device removed!"));
    return USBH_STATUS_INVALID_PARAM;
  }
  T_ASSERT_MAGIC(pUnit->pDev, USBH_MSD_DEVICE);
  T_ASSERT_PTR(pUnit->pDev->InterfaceHandle);
  ZERO_STRUCT(command);
  command.Cmd = SC_MODE_SENSE_6;
  command.idx2 = (U8)(Page | PageControlCode);
  length         = *pLength;
  command.Length = *pLength;
  *pLength       = 0;
  status = pUnit->pDev->TlCommandReadData(pUnit, (U8 * ) &command, sizeof(command), pData, &length, USBH_MSD_READ_TIMEOUT, FALSE);
  if (status) {
    USBH_WARN((USBH_MTYPE_MSD_PHYS, "MSD SC6: _ModeSense: pUnit->pDev->TlCommandReadData:"));
    USBH_PRINT_STATUS_VALUE(USBH_MTYPE_MSD_PHYS, status);
  } else {
    if (length < SC_MODE_PARAMETER_HEADER_LENGTH_6 || length > 0xff) {
      return USBH_STATUS_LENGTH;
    } else {
      * pLength = (U8)length;
      USBH_MSD_ConvModeParameterHeader(pHeader, pData, TRUE); // TRUE=6-byte command
    }
  }
  return status;
}

/*********************************************************************
*
*       _RequestSense
*
*  Function description
*    Issues a REQUEST SENSE command to receive
*    the sense pData for the last requested command.
*    If the application client issues a command other than REQUEST
*    SENSE, the sense pData for the last command is lost.
*
*  Return value:
*    ==0:  For success, sense pData is copied to structure pUnit->sense
*    !=0:  For error
*/
static USBH_STATUS _RequestSense(USBH_MSD_UNIT * pUnit) {
  U32              length;
  SCSI_6BYTE_CMD   buffer; // Byte array
  USBH_STATUS      status;
  U8             * SenseBuffer;
  USBH_MSD_DEVICE     * dev;

  USBH_LOG((USBH_MTYPE_MSD_PHYS, "MSD SC6: _RequestSense"));
  T_ASSERT_MAGIC(pUnit, USBH_MSD_UNIT);
  dev = pUnit->pDev;
  if (dev == NULL) {
    USBH_WARN((USBH_MTYPE_MSD_PHYS, "MSD SC6: _RequestSense: Device removed!"));
    return USBH_STATUS_INVALID_PARAM;
  }
  T_ASSERT_MAGIC(dev, USBH_MSD_DEVICE);
  T_ASSERT_PTR(dev->InterfaceHandle);
  length        = STANDARD_SENSE_LENGTH;
  ZERO_STRUCT(buffer);
  buffer.Cmd    = SC_REQUEST_SENSE;
  buffer.Length = (U8)length;
  SenseBuffer   = UrbBufferAllocateTransferBuffer(STANDARD_SENSE_LENGTH);
  if (NULL == SenseBuffer) {
    USBH_WARN((USBH_MTYPE_MSD_PHYS, "MSD SC6: _RequestSense: no memory resources!"));
    return USBH_STATUS_RESOURCES;
  }
  // Because _RequestSense is used from pUnit->pDev->TlCommandReadData and pUnit->pDev->TlCommandWriteData it must call TlCommandReadData directly.
  status = dev->TlCommandReadData(pUnit, (U8 *)&buffer, sizeof(buffer), SenseBuffer, &length, USBH_MSD_READ_TIMEOUT, FALSE);
  if (status) {                   // On error
    pUnit->Sense.ResponseCode = 0; // invalidate the sense pData
    USBH_WARN((USBH_MTYPE_MSD_PHYS, "MSD SC6: USBH_MSD_RequestSense: TlCommandReadData:"));
    USBH_PRINT_STATUS_VALUE(USBH_MTYPE_MSD_PHYS, status);
  } else {
    USBH_MSD_ConvStandardSense(SenseBuffer, (U16)length, &pUnit->Sense);
  }
  UrbBufferFreeTransferBuffer(SenseBuffer);
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
*    pData:           pData buffer
*    Sectors:        Number of contiguous logical blocks to read, max. 127
*
*/
static USBH_STATUS _ReadSectors(USBH_MSD_UNIT * pUnit, U32 SectorAddress, U8 * pData, U16 Sectors) {
  U32         length, oldLength;
  U8          command[SCSI_10BYTE_COMMAND_LENGTH];
  USBH_STATUS status;
  USBH_LOG((USBH_MTYPE_MSD_PHYS, "MSD SC6: _ReadSectors: address: %lu, sectors: %u",SectorAddress,Sectors));
  T_ASSERT_MAGIC(pUnit, USBH_MSD_UNIT);
  T_ASSERT_PTR(pData);
  T_ASSERT(Sectors);
  if (pUnit->pDev == NULL) {
    USBH_WARN((USBH_MTYPE_MSD_PHYS, "MSD SC6: _ReadSectors: Device removed!"));
    return USBH_STATUS_INVALID_PARAM;
  }
  T_ASSERT_MAGIC(pUnit->pDev, USBH_MSD_DEVICE);
  T_ASSERT_PTR(pUnit->pDev->InterfaceHandle);
  if (SectorAddress > pUnit->MaxSectorAddress) {
    USBH_WARN((USBH_MTYPE_MSD_PHYS, "MSD SC6: _ReadSectors: invalid sector address! max. address: %lu, used address: %lu",
      pUnit->MaxSectorAddress,SectorAddress));
    return USBH_STATUS_INVALID_PARAM;
  }
  oldLength = length = Sectors * pUnit->BytesPerSector;
  USBH_MSD_Conv10ByteCommand(SC_READ_10, SectorAddress, Sectors, command);
  status = pUnit->pDev->TlCommandReadData(pUnit, (U8 * ) &command, sizeof(command), pData, &length, USBH_MSD_READ_TIMEOUT, TRUE);
  if (status) {
    USBH_WARN((USBH_MTYPE_MSD_PHYS, "MSD SC6: _ReadSectors: pUnit->pDev->TlCommandReadData:"));
    USBH_PRINT_STATUS_VALUE(USBH_MTYPE_MSD_PHYS, status);
  } else {
    if (length != oldLength) { // Not all sectors read
      USBH_WARN((USBH_MTYPE_MSD_PHYS, "MSD SC6: _ReadSectors: bytes to read: %lu, bytes read: %lu",oldLength,length));
      status = USBH_STATUS_LENGTH;
    } else {
      USBH_LOG((USBH_MTYPE_MSD_PHYS, "MSD SC6: _ReadSectors: bytes read: %lu",length));
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
*   pData:           pData buffer
*   Sectors:        Number of contiguous logical blocks written to the device
*/
static USBH_STATUS _WriteSectors(USBH_MSD_UNIT * pUnit, U32 SectorAddress, const U8 * pData, U16 Sectors) {
  U32         length, oldLength;
  U8          command[SCSI_10BYTE_COMMAND_LENGTH];
  USBH_STATUS status;
  USBH_LOG((USBH_MTYPE_MSD_PHYS, "MSD SC6: _WriteSectors: address: %lu, sectors: %lu",SectorAddress,Sectors));
  T_ASSERT_MAGIC(pUnit, USBH_MSD_UNIT);
  T_ASSERT_PTR(pData);
  T_ASSERT(Sectors);
  if (SectorAddress > pUnit->MaxSectorAddress) {
    USBH_WARN((USBH_MTYPE_MSD_PHYS, "MSD SC6: _WriteSectors: invalid sector address! max. address: %lu, used address: %lu", pUnit->MaxSectorAddress,SectorAddress));
    return USBH_STATUS_INVALID_PARAM;
  }
  if (USBH_MSD_PHY_IsWriteProtected(pUnit)) {                 // Check if unit is write protected
    return USBH_STATUS_WRITE_PROTECT;
  }
  oldLength = length = Sectors * pUnit->BytesPerSector; // pLength = sectors * bytes per sector
  USBH_MSD_Conv10ByteCommand(SC_WRITE_10, SectorAddress, Sectors, command);
  status = pUnit->pDev->TlCommandWriteData(pUnit, command, sizeof(command), pData, &length, USBH_MSD_WRITE_TIMEOUT, TRUE);
  if (status) {
    USBH_WARN((USBH_MTYPE_MSD_PHYS, "MSD SC6: _WriteSectors: pUnit->pDev->TlCommandWriteData:"));
    USBH_PRINT_STATUS_VALUE(USBH_MTYPE_MSD_PHYS, status);
  } else {
    if (length != oldLength) { // Error, the device must write all bytes
      USBH_WARN((USBH_MTYPE_MSD_PHYS, "MSD SC6: _WriteSectors: bytes to write: %lu, bytes written: %lu",oldLength,length));
      status = USBH_STATUS_LENGTH;
    } else {
      USBH_LOG((USBH_MTYPE_MSD_PHYS, "MSD SC6: _WriteSectors: bytes written: %lu",length));
    }
  }
  return status;
}

const USBH_MSD_PHYS_LAYER_API USBH_MSD_PhysLayerSC06 = {
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

/******************************* EOF ********************************/
