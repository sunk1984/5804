/*********************************************************************
*  
*   IAR PowerPac
*
*   (c) Copyright IAR Systems 2010.  All rights reserved.
*
**********************************************************************
----------------------------------------------------------------------
File        : USBH_MSD.c
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

/*********************************************************************
*
*       #defines, non-configurable
*
**********************************************************************
*/

/*
  constants in the Class Interface Descriptor
  for USB Mass Storage devices
*/
#define MASS_STORAGE_CLASS  0x08
#define PROTOCOL_BULK_ONLY  0x50 // Bulk only
#define SUBCLASS_1          0x01 // Reduced Block Commands (flash)
#define SUBCLASS_2          0x02 // SFF-8020i, MMC-2, ATAPI (CD-ROM)
#define SUBCLASS_3          0x03 // QIC-157 (tape)
#define SUBCLASS_4          0x04 // UFI (floppy)
#define SUBCLASS_5          0x05 // SFF-8070i (removable)
#define SUBCLASS_6          0x06 // Transparent SCSI, that can be used as SUBCLASS_RBC

// Maximum size of the complete configuration descriptor for the used device in bytes.
// The size depends from the descriptor size in the device.
#define CONFIGURATION_DESC_BUFFER_LEN 255

#if CONFIGURATION_DESC_BUFFER_LEN < 9
  #error "define CONFIGURATION_DESC_BUFFER_LEN"
#endif

#define MAX_EP0_TRANSFER_BUFFER_LENGTH  64 // Size of control endpoint data stage buffer in the USBH MSD device object!
#define BULK_ONLY_NUMBER_OF_ENDPOINTS   2  // Number of endpoints used for the bulk only protocol

/*********************************************************************
*
*       #defines, function-replacement
*
**********************************************************************
*/

#if (USBH_DEBUG > 1)
  // Macros for reference counters to detect errors in the debug Verison of the USBH MSD library
  #define USBH_MSD_DEC_URBCT(devPtr)                                                        \
      (devPtr)->UrbRefCt--;                                                            \
      if ((devPtr)->UrbRefCt  != 0) {                                                  \
        USBH_WARN((USBH_MTYPE_MSD, "MSD: Invalid UrbRefCt:%d",  (devPtr)->UrbRefCt));  \
        T_ASSERT0;                                                                     \
      }
  #define USBH_MSD_INC_URBCT(devPtr)                                                        \
      (devPtr)->UrbRefCt++;                                                            \
      if ((devPtr)->UrbRefCt  != 1) {                                                  \
        USBH_WARN((USBH_MTYPE_MSD, "MSD: Invalid UrbRefCt:%d",  (devPtr)->UrbRefCt));  \
        T_ASSERT0;                                                                     \
      }
  #define USBH_MSD_DEC_SYNC(devPtr)                                                         \
      (devPtr)->SyncRefCt--;                                                           \
      if ((devPtr)->SyncRefCt != 0) {                                                  \
        USBH_WARN((USBH_MTYPE_MSD, "MSD: Invalid SyncRefCt:%d", (devPtr)->SyncRefCt)); \
        T_ASSERT0;                                                                     \
      }
  #define USBH_MSD_INC_SYNC(devPtr)                                                         \
      (devPtr)->SyncRefCt++;                                                           \
      if ((devPtr)->SyncRefCt != 1) {                                                  \
        USBH_WARN((USBH_MTYPE_MSD, "MSD: Invalid SyncRefCt:%d", (devPtr)->SyncRefCt)); \
        T_ASSERT0;                                                                     \
      }
#else
  #define USBH_MSD_DEC_URBCT(devPtr)
  #define USBH_MSD_INC_URBCT(devPtr)
  #define USBH_MSD_DEC_SYNC( devPtr)
  #define USBH_MSD_INC_SYNC( devPtr)
#endif

/*********************************************************************
*
*       Public data
*
**********************************************************************
*/

USBH_MSD_DRV USBH_MSD_Global; // Global driver object

/*********************************************************************
*
*       Static code
*
**********************************************************************
*/


/*********************************************************************
*
*       _FillCBW
*
*  Function description:
*    Initialize the complete command block without copying the command bytes
*
*/
static void _FillCBW(COMMAND_BLOCK_WRAPPER * pCBW, U32 Tag, U32 DataLength, U8 Flags, U8 Lun, U8 CommandLength)  {
  pCBW->Signature          = CBW_SIGNATURE;
  pCBW->Tag                = Tag;
  pCBW->Flags              = Flags;
  pCBW->Lun                = Lun;
  pCBW->DataTransferLength = DataLength;
  pCBW->Length             = CommandLength;
}

/*********************************************************************
*
*       _ConvEndpointDesc
*
*  Function description
*    _ConvEndpointDesc convert a received byte aligned buffer to
*    a machine independent struct USB_ENDPOINT_DESCRIPTOR
*/
static void _ConvEndpointDesc(const U8 * Buffer, USB_ENDPOINT_DESCRIPTOR * EpDesc) {
  EpDesc->bLength          = Buffer[0];                  // Index 0 bLength
  EpDesc->bDescriptorType  = Buffer[1];                  // Index 1 bDescriptorType
  EpDesc->bEndpointAddress = Buffer[2];                  // Index 2 bEndpointAddress
  EpDesc->bmAttributes     = Buffer[3];                  // Index 3 bmAttributes
  EpDesc->wMaxPacketSize   = USBH_LoadU16LE(&Buffer[4]);
  EpDesc->bInterval        = Buffer[6];                  // Index 6 bInterval
}

/*********************************************************************
*
*       _IsCSWValidandMeaningful
*
*  Function description
*    Checks if the command Status block is valid and meaningful
*/
static T_BOOL _IsCSWValidandMeaningful(USBH_MSD_DEVICE * pDev, const COMMAND_BLOCK_WRAPPER * cbw, const COMMAND_STATUS_WRAPPER * csw, const U32 CSWlength) {
  if (CSWlength < CSW_LENGTH) {
    USBH_WARN((USBH_MTYPE_MSD, "MSD: IsCSWValid: invalid CSW length: %lu",CSWlength));
    return 0;                      // False
  }
  if (csw->Signature != CSW_SIGNATURE) {

#if (USBH_DEBUG > 1)
    if (CSWlength == CSW_LENGTH) { // Prevents debug messages if test a regular data block
      USBH_WARN((USBH_MTYPE_MSD, "MSD: IsCSWValid: invalid CSW signature: 0x%08X",csw->Signature));
    }
#endif

    return FALSE;
  }
  if (csw->Tag != pDev->BlockWrapperTag) {

#if (USBH_DEBUG > 1)
    if (CSWlength == CSW_LENGTH) { // Prevent debug messages if test a regular data block
      USBH_WARN((USBH_MTYPE_MSD, "MSD: IsCSWValid: invalid Tag sent:0x%08x rcv:0x%08x", cbw->Tag,csw->Tag));
    }
#endif

    return FALSE;
  }
  if (2 == csw->Status) {            // CSW is valid
    return 1;
  }
  if (2 > csw->Status && csw->Residue <= cbw->DataTransferLength) {
    return 1;
  }
  return 0;
}

/*********************************************************************
*
*       _WriteTag
*
*  Function description
*    Writes a tag beginning with offset 4 of the command block wrapper in little endian byte order
*/
static void _WriteTag(USBH_MSD_DEVICE * pDev, U8 * cbwBuffer) {
  T_ASSERT_MAGIC(pDev, USBH_MSD_DEVICE);
  T_ASSERT_PTR  (cbwBuffer);
  pDev->BlockWrapperTag++;    // LSB
  * (cbwBuffer + 4) = (U8) pDev->BlockWrapperTag;
  * (cbwBuffer + 5) = (U8)(pDev->BlockWrapperTag >> 8);
  * (cbwBuffer + 6) = (U8)(pDev->BlockWrapperTag >> 16);
  * (cbwBuffer + 7) = (U8)(pDev->BlockWrapperTag >> 24);
}

/*********************************************************************
*
*       _ConvBufferToStatusWrapper
*
*  Function description
*    Converts a byte buffer to a structure of type COMMAND_STATUS_WRAPPER.
*    This function is independent from the byte order of the processor.
*    The buffer is in little endian byte format.
*
*  Return value
*    USBH_STATUS_SUCCESS for success,
*    USBH_STATUS_ERROR for error
*/
static int _ConvBufferToStatusWrapper(const U8 * buffer, int length, COMMAND_STATUS_WRAPPER * csw) {
  T_ASSERT_PTR(buffer);
  if (length < CSW_LENGTH) {
    return USBH_STATUS_LENGTH;
  }
  csw->Signature = USBH_LoadU32LE(buffer);
  csw->Tag       = USBH_LoadU32LE(buffer + 4);       //  4: tag Same as original command
  csw->Residue   = USBH_LoadU32LE(buffer + 8);       //  8: residue, amount of bytes not transferred
  csw->Status    = * (buffer + 12);                  // 12: Status
  return USBH_STATUS_SUCCESS;
}

/*********************************************************************
*
*       _OnSubmitUrbCompletion
*
*  Function description
*/
static void _OnSubmitUrbCompletion(URB * pUrb) {
  USBH_MSD_DEVICE * pDev;
  pDev       = (USBH_MSD_DEVICE *)pUrb->Header.Context;
  T_ASSERT_MAGIC(pDev, USBH_MSD_DEVICE);
  USBH_LOG((USBH_MTYPE_MSD_INTERN, "MSD: _OnSubmitUrbCompletion URB st: 0x%08x",pUrb->Header.Status));
  USBH_OS_SetEvent(pDev->UrbEvent);
}

/*********************************************************************
*
*       _SubmitUrbAndWait
*
*  Function description
*    Submits an URB to the USB bus driver synchronous, it uses the
*    TAL event functions. On successful completion the URB Status is returned!
*/
static USBH_STATUS _SubmitUrbAndWait(USBH_MSD_DEVICE * pDev, URB * pUrb, U32 timeout) {
  USBH_STATUS Status;
  int         EventStatus;

  T_ASSERT(NULL != pDev->InterfaceHandle);
  T_ASSERT_PTR(pDev->UrbEvent);
  USBH_LOG((USBH_MTYPE_MSD_INTERN, "MSD: _SubmitUrbAndWait"));
  USBH_MSD_INC_URBCT(pDev);
  pUrb->Header.Completion = _OnSubmitUrbCompletion;
  pUrb->Header.Context    = pDev;
  USBH_OS_ResetEvent(pDev->UrbEvent);
  Status                 = USBH_SubmitUrb(pDev->InterfaceHandle, pUrb);
  if (Status != USBH_STATUS_PENDING) {
    USBH_WARN((USBH_MTYPE_MSD, "MSD: _SubmitUrbAndWait: USBH_SubmitUrb st: 0x%08x",Status));
    Status = USBH_STATUS_ERROR;
  } else {                                // Pending URB
    Status       = USBH_STATUS_SUCCESS;
    EventStatus = USBH_OS_WaitEventTimed(pDev->UrbEvent, timeout);
    if (EventStatus != USBH_OS_EVENT_SIGNALED) {
      T_BOOL abort    = TRUE;
      URB * abort_urb = &pDev->ControlUrb;
      USBH_WARN((USBH_MTYPE_MSD, "MSD: _SubmitUrbAndWait: timeout Status: 0x%08x, now abort the URB!",EventStatus));
      ZERO_MEMORY(abort_urb, sizeof(URB));
      switch (pUrb->Header.Function) {     // Not signaled abort and wait infinite
      case USBH_FUNCTION_BULK_REQUEST:
      case USBH_FUNCTION_INT_REQUEST:
        abort_urb->Request.EndpointRequest.Endpoint = pUrb->Request.BulkIntRequest.Endpoint;
        break;
      case USBH_FUNCTION_CONTROL_REQUEST:
        abort_urb->Request.EndpointRequest.Endpoint = 0;
      default:
        abort = FALSE;
        USBH_WARN((USBH_MTYPE_MSD, "MSD: _SubmitUrbAndWait: invalid URB function: %d",pUrb->Header.Function));
        break;
      }
      if (abort) {
        USBH_WARN((USBH_MTYPE_MSD, "MSD: _SubmitUrbAndWait: Abort Ep: 0x%x", pUrb->Request.EndpointRequest.Endpoint));
        abort_urb->Header.Function = USBH_FUNCTION_ABORT_ENDPOINT;
        USBH_OS_ResetEvent(pDev->UrbEvent);
        abort_urb->Header.Completion = _OnSubmitUrbCompletion;
        abort_urb->Header.Context    = pDev;
        Status = USBH_SubmitUrb(pDev->InterfaceHandle, abort_urb);
        if (Status) {
          USBH_WARN((USBH_MTYPE_MSD, "MSD: _SubmitUrbAndWait: USBH_FUNCTION_ABORT_ENDPOINT st: 0x%08x",Status));
        }
        USBH_OS_WaitEvent(pDev->UrbEvent); // Wait infinite for completion
      }
    }
    if (!Status) {
      Status = pUrb->Header.Status;       // URB completed return the pBuffer Status
      if (Status) {
        USBH_WARN((USBH_MTYPE_MSD, "MSD: _SubmitUrbAndWait: URB Status: 0x%08x",Status));
      }
    }
  }
  USBH_MSD_DEC_URBCT(pDev);
  return Status;
}

/*********************************************************************
*
*       _ResetPipe
*
*  Function description
*/
static USBH_STATUS _ResetPipe(USBH_MSD_DEVICE * pDev, U8 EndPoint) {
  USBH_STATUS   status;
  URB         * urb;
  T_ASSERT_MAGIC(pDev, USBH_MSD_DEVICE);
  T_ASSERT_PTR  (pDev->InterfaceHandle);
  urb                                   = &pDev->ControlUrb;
  urb->Header.Function                  = USBH_FUNCTION_RESET_ENDPOINT;
  urb->Header.Completion                = NULL;
  urb->Request.EndpointRequest.Endpoint = EndPoint;
  status                                = _SubmitUrbAndWait(pDev, urb, USBH_MSD_EP0_TIMEOUT); // On error this URB is not aborted
  if (status) { // Reset pipe does not wait
    USBH_WARN((USBH_MTYPE_MSD, "MSD: _ResetPipe: USBH_SubmitUrb st: 0x%08x",status));
    status = USBH_STATUS_ERROR;
  }
  return status;
}

/*********************************************************************
*
*       _SetupRequest
*
*  Function description
*    Synchronous vendor request
*
*  Parameters:
*    pUrb: IN: pUrb.Request.ControlRequest.Setup, other URB fields undefined / OUT: status
*    pBuffer:  IN: -  OUT: valid pointer or NULL
*    pLength:  IN: -  OUT: transferred bytes
*/
static USBH_STATUS _SetupRequest(USBH_MSD_DEVICE * pDev, URB * pUrb, U8 * pBuffer, U32 * pLength, U32 Timeout) {
  USBH_STATUS                             status;
  * pLength                             = 0;      // Clear returned pLength
  pUrb->Header.Function                 = USBH_FUNCTION_CONTROL_REQUEST;
  pUrb->Request.ControlRequest.Endpoint = 0;
  pUrb->Request.ControlRequest.Buffer   = pBuffer;
  status                                = _SubmitUrbAndWait(pDev, pUrb, Timeout);
  if (status) {
    USBH_WARN((USBH_MTYPE_MSD, "MSD: USBH_MSD_VendorRequest: st: 0x%08x",status));
  } else {
    * pLength = pUrb->Request.ControlRequest.Length;
  }
  return status;
}

/*********************************************************************
*
*       _ReadSync
*
*  Function description
*    _ReadSync reads all bytes to buffer via Bulk IN transfers.
*    Transactions are performed in chunks of no more than USBH_MSD_Global.MaxTransferSize.
*/
static USBH_STATUS _ReadSync(USBH_MSD_DEVICE * pDev, U8 * pBuffer, U32 * pLength, U16 Timeout, T_BOOL DataPhaseFlag, T_BOOL SectorDataFlag) {
  U32           remainingLength, rdLength;
  U8          * buffer;
  USBH_STATUS   status = USBH_STATUS_SUCCESS;
  URB         * urb;
  T_ASSERT_MAGIC(pDev, USBH_MSD_DEVICE);
  T_ASSERT_PTR  (pBuffer);
  T_ASSERT_PTR  (pLength);
  // Unused param, if needed for later use
  (void)DataPhaseFlag;
  (void)SectorDataFlag;
  USBH_LOG((USBH_MTYPE_MSD_INTERN, "MSD: _ReadSync Ep: %u,length: %4lu",(int)pDev->BulkInEp,*pLength));
  if (pDev->Removed) {
    USBH_WARN((USBH_MTYPE_MSD, "MSD: _ReadSync: Device removed!"));
    return USBH_STATUS_DEVICE_REMOVED;
  }
  buffer                               =   pBuffer;
  remainingLength                      = * pLength;
  * pLength                            =   0;
  urb                                  =   &pDev->Urb;
  urb->Header.Function                 =   USBH_FUNCTION_BULK_REQUEST;
  urb->Request.BulkIntRequest.Endpoint =   pDev->BulkInEp;
  while (remainingLength) {                         // Remaining buffer
    rdLength                           = USBH_MIN(remainingLength, USBH_MSD_Global.MaxTransferSize);
    urb->Request.BulkIntRequest.Buffer = buffer;
    urb->Request.BulkIntRequest.Length = rdLength;
    USBH_LOG((USBH_MTYPE_MSD_INTERN, "MSD: _ReadSync: DlReadSync bytes to read: %4lu",rdLength));
    status                             = _SubmitUrbAndWait(pDev, urb, Timeout);
    rdLength                           = urb->Request.BulkIntRequest.Length;
    if (status) {                                   // On error stops and discard data
      USBH_WARN((USBH_MTYPE_MSD, "MSD: _ReadSync: _SubmitUrbAndWait: length: %lu st: 0x%08x", rdLength,status));
      break;
    } else {                                        // On success
        remainingLength -= rdLength;
      * pLength         += rdLength;
      if ((rdLength % pDev->BulkMaxPktSize) != 0) { // A short packet was received
        USBH_LOG((USBH_MTYPE_MSD_INTERN, "MSD: INFO _ReadSync: short packet with length %lu received!",rdLength));
        break;
      }
      buffer            += rdLength;                // Adjust destination
    }
  }
  USBH_LOG((USBH_MTYPE_MSD_INTERN, "MSD: _ReadSync: returned length: %lu ",*pLength));
  return status;
}

/*********************************************************************
*
*       _WriteSync
*
*  Function description
*    _WriteSync writes all bytes to device via Bulk OUT transfers.
*    Transactions are performed in chunks of no more than USBH_MSD_Global.MaxTransferSize.
*/
static int _WriteSync(USBH_MSD_DEVICE * pDev, const U8 * Buffer, U32 * Length, U16 Timeout, T_BOOL DataPhaseFlag, T_BOOL SectorDataFlag) {
  U32        remainingLength, wrLength, oldLength;
  const U8 * buffer;
  int        status;
  URB      * urb;
  // Unused param, if needed for later use
  (void)DataPhaseFlag;
  (void)SectorDataFlag;
  status = USBH_STATUS_SUCCESS;
  T_ASSERT_MAGIC(pDev, USBH_MSD_DEVICE);
  T_ASSERT_PTR  (Buffer);
  T_ASSERT_PTR  (Length);
  USBH_LOG((USBH_MTYPE_MSD_INTERN, "MSD: _WriteSync Ep: %4u,length: %4lu",pDev->BulkOutEp,*Length));
  if (pDev->Removed) {
    USBH_WARN((USBH_MTYPE_MSD, "MSD: _ReadSync: Device removed!"));
    return USBH_STATUS_INVALID_PARAM;
  }
  buffer                               = Buffer;
  remainingLength                      = * Length;
  urb                                  = &pDev->Urb;
  urb->Header.Function                 = USBH_FUNCTION_BULK_REQUEST;
  urb->Request.BulkIntRequest.Endpoint = pDev->BulkOutEp;
  do {
    oldLength = wrLength               = USBH_MIN(remainingLength, USBH_MSD_Global.MaxTransferSize);
    urb->Request.BulkIntRequest.Buffer = (void *)buffer;
    USBH_LOG((USBH_MTYPE_MSD_INTERN, "MSD: consider: DlWriteSync bytes to write: %4lu",wrLength));
    urb->Request.BulkIntRequest.Length = wrLength;
    status                             = _SubmitUrbAndWait(pDev, urb, Timeout);
    if (status) {
      USBH_WARN((USBH_MTYPE_MSD, "MSD: _WriteSync: _SubmitUrbAndWait: st: 0x%08x",status));
      break;
    }
    USBH_LOG((USBH_MTYPE_MSD_INTERN, "MSD: _WriteSync: %4lu written",wrLength));
    if (wrLength != oldLength) {
      USBH_WARN((USBH_MTYPE_MSD, "MSD: DlWriteSync: Not all bytes written"));
      break;
    }
    remainingLength -= wrLength;
    buffer          += wrLength;        // Adjust source
  } while (remainingLength);
  * Length -= remainingLength;          // Does not consider the last write
  USBH_LOG((USBH_MTYPE_MSD_INTERN, "MSD: _WriteSync returned length: %4lu",*Length));
  return status;
}

/*********************************************************************
*
*       _ReadCSW
*
*  Function description:
*    Reads the command status block from the device and
*    checks if the status block is valid and meaningful.
*    If the USB device stalls on the IN pipe
*    the endpoint is reset and the CSW is read again.
*
*  Return value:
*    USBH_STATUS_SUCESS          on success
*    USBH_STATUS_COMMAND_FAILED  the command failed, check the sense data
*    USBH_STATUS_ERROR           no command status block received or status block with a phase error
*/
static int _ReadCSW(USBH_MSD_DEVICE * pDev, COMMAND_BLOCK_WRAPPER * Cbw, COMMAND_STATUS_WRAPPER * Csw) {
  int   status, i;
  U32   length;
  U8  * buffer;
  T_ASSERT_MAGIC(pDev, USBH_MSD_DEVICE);
  T_ASSERT_PTR  (Cbw);
  T_ASSERT_PTR  (Csw);

  if (pDev->Removed) {
    USBH_WARN((USBH_MTYPE_MSD, "MSD: _ReadCSW: Device removed!"));
    return USBH_STATUS_INVALID_PARAM;
  }
  status = USBH_STATUS_ERROR;
  buffer = pDev->CswPhyTransferBuffer;
  i      = 2;
  length = 0;         // If the first status block read fails (no timeout error) then read a second time

  while (i) {
    length = pDev->BulkMaxPktSize;
    status = _ReadSync(pDev, buffer, &length, USBH_MSD_READ_TIMEOUT, 0, 0);
    if (!status) {    // Success
      break;
    } else {          // Error
      USBH_WARN((USBH_MTYPE_MSD, "MSD: _ReadCSW: _ReadSync!"));
      USBH_PRINT_STATUS_VALUE(USBH_MTYPE_MSD, status);
      if (status != USBH_STATUS_TIMEOUT) { // Timeout
        break;
      } else {        // On all other errors reset the pipe an try it again to read CSW
        status = _ResetPipe(pDev, pDev->BulkInEp);
        if (status) { // Reset error, break
          USBH_WARN((USBH_MTYPE_MSD, "MSD: _ReadCSW: _ResetPipe: 0x%08x",status));
          break;
        }
      } // Try to read again the CSW
    }
    i--;
  }
  if (!status) {                                                // On success
    if (length == CSW_LENGTH) {
      if (!_ConvBufferToStatusWrapper(buffer, length, Csw)) {   // Check CSW
        if (!_IsCSWValidandMeaningful(pDev, Cbw, Csw, length)) {
          USBH_WARN((USBH_MTYPE_MSD, "MSD: _ReadCSW: IsCSWValidandMeaningful:"));
          USBH_PRINT_STATUS_VALUE(USBH_MTYPE_MSD, status);
          status = USBH_STATUS_ERROR;
        }
      } else {
        USBH_WARN((USBH_MTYPE_MSD, "MSD: _ReadCSW: _ConvBufferToStatusWrapper"));
        USBH_PRINT_STATUS_VALUE(USBH_MTYPE_MSD,  status);
      }
    } else {
      USBH_WARN((USBH_MTYPE_MSD, "MSD: _ReadCSW: invalid length: %lu",length));
      status = USBH_STATUS_ERROR;
    }
  }
  return status;
}

/*********************************************************************
*
*       _PerformResetRecovery
*
*  Function description
*    TBD
*
*  Return value
*    TBD
*/
static int _PerformResetRecovery(USBH_MSD_DEVICE * pDev) {
  int status;
  USBH_LOG((USBH_MTYPE_MSD, "MSD: USBH_MSD_MassStorageReset"));
  T_ASSERT_MAGIC(pDev, USBH_MSD_DEVICE);
  if (pDev->Removed) {
    USBH_WARN((USBH_MTYPE_MSD, "MSD: USBH_MSD_MassStorageReset: Device removed!"));
    return USBH_STATUS_INVALID_PARAM;
  }
  status = pDev->TlReset(pDev);                                                       // 1. Generates an bulk only mass storage reset. (This is a vendor request!)
  if (status) {
    if (status == USBH_STATUS_STALL) { // Invalid command
      USBH_WARN((USBH_MTYPE_MSD, "MSD: USBH_MSD_MassStorageReset: Device does not support bulk only mass storage reset!"));
    } else {
      USBH_WARN((USBH_MTYPE_MSD, "MSD: USBH_MSD_MassStorageReset: _BULKONLY_MassStorageReset status: %d",status));
      return status;
    }
  }
  USBH_LOG((USBH_MTYPE_MSD_INTERN, "MSD: DlResetPipe Ep-address: %u",pDev->BulkInEp));  // 2. Reset Bulk IN
  status = _ResetPipe(pDev, pDev->BulkInEp);
  if (status) {
    USBH_WARN((USBH_MTYPE_MSD, "MSD: USBH_MSD_MassStorageReset: _ResetPipe!"));
  }
  USBH_LOG((USBH_MTYPE_MSD_INTERN, "MSD: DlResetPipe Ep-address: %u",pDev->BulkOutEp)); // 3. Reset Bulk OUT
  status = _ResetPipe(pDev, pDev->BulkOutEp);
  if (status) {
    USBH_WARN((USBH_MTYPE_MSD, "MSD: USBH_MSD_MassStorageReset: _ResetPipe!"));
    return USBH_STATUS_SUCCESS;
  }
  return status;
}

/*********************************************************************
*
*       _BULKONLY_ErrorRecovery
*
*  Function description
*    _BULKONLY_ErrorRecovery performs a USB Mass Storage reset.
*    If the command fails then a USB bus reset and a device initialization
*    is performed. If the USB bus reset or the device initialization fails
*    the device removed flag is checked and recovery counter is set to BULK_ONLY_MAX_RECOVERY+1.
*    Otherwise the counter is incremented.
*
*    Parameters
*      pDev:   pointer to the device object
*
*    Return value
*      USBH_STATUS_SUCCESS: Successfully execution of a recovery function,
*      USBH_STATUS_ERROR:   It is not possible to recover the device.
*/
static int _BULKONLY_ErrorRecovery(USBH_MSD_DEVICE * pDev) {
  int status = USBH_STATUS_SUCCESS;                               // Default status
  USBH_LOG((USBH_MTYPE_MSD_INTERN, "MSD: _BULKONLY_ErrorRecovery"));
  T_ASSERT_MAGIC(pDev, USBH_MSD_DEVICE);
  if (pDev->Removed) {
    USBH_WARN((USBH_MTYPE_MSD, "MSD: _BULKONLY_ErrorRecovery: Device removed!!"));
    return USBH_STATUS_DEVICE_REMOVED;
  }
  if (pDev->ErrorRecoveryCt >= BULK_ONLY_MAX_RECOVERY) {          // On maximum recoveries
    pDev->Removed = TRUE;
    return USBH_STATUS_ERROR;
  } else {
    pDev->ErrorRecoveryCt++;
  }
  status = _PerformResetRecovery(pDev);
  if (status) {
    USBH_WARN((USBH_MTYPE_MSD, "MSD: _BULKONLY_ErrorRecovery: _PerformResetRecovery: st: 0x%08x!",status));
  }
  return USBH_STATUS_SUCCESS;                                     // Try again until BULK_ONLY_MAX_RECOVERY repetitions
}

/*********************************************************************
*
*       _BULKONLY_MassStorageReset
*
*  Function description
*    TBD
*
*  Return value
*    TBD
*/
static USBH_STATUS _BULKONLY_MassStorageReset(USBH_MSD_DEVICE * pDev) {
  USBH_STATUS         status;
  USBH_SETUP_PACKET * setup;
  U32                 length = 0;
  URB               * urb;
  USBH_LOG((USBH_MTYPE_MSD_INTERN, "MSD: USBH_MSD_BulkOnlyMassStorageReset"));
  T_ASSERT_MAGIC(pDev, USBH_MSD_DEVICE);
  if (pDev->Removed) {
    USBH_WARN((USBH_MTYPE_MSD, "MSD: _BULKONLY_MassStorageReset: Device removed!"));
    return USBH_STATUS_DEVICE_REMOVED;
  }
  urb            = &pDev->Urb;
  setup          = &urb->Request.ControlRequest.Setup;
  setup->Type    = USB_REQTYPE_CLASS | USB_INTERFACE_RECIPIENT;
  setup->Request = BULK_ONLY_RESET_REQ;
  setup->Index   = (U16)pDev->bInterfaceNumber;
  setup->Value   = 0;
  setup->Length  = 0;
  status         = _SetupRequest(pDev, urb, (U8 * )NULL, &length, USBH_MSD_EP0_TIMEOUT);
  if (status) {
    USBH_WARN((USBH_MTYPE_MSD, "MSD: _BULKONLY_MassStorageReset st: 0x%08x!",status));
  }
  return status;
}

/*********************************************************************
*
*       _BULKONLY_GetMaxLUN
*
*  Function description
*    see USBH_MSD_TL_GETMAX_LUN_INTERFACE
*
*  Return value
*    TBD
*/
static USBH_STATUS _BULKONLY_GetMaxLUN(USBH_MSD_DEVICE * pDev, int * maxLunIndex) {
  U32                 length;
  USBH_SETUP_PACKET * setup;
  USBH_STATUS         status;
  URB               * urb;
  USBH_LOG((USBH_MTYPE_MSD, "MSD: _BULKONLY_GetMaxLUN "));
  T_ASSERT_MAGIC(pDev, USBH_MSD_DEVICE);
  * maxLunIndex = 0; // default value
  if (pDev->Removed) {
    USBH_WARN((USBH_MTYPE_MSD, "MSD: _BULKONLY_GetMaxLUN: Device removed!"));
    return USBH_STATUS_DEVICE_REMOVED;
  }
  urb            = &pDev->Urb;
  setup          = &urb->Request.ControlRequest.Setup;
  setup->Type    = USB_REQTYPE_CLASS | USB_INTERFACE_RECIPIENT | USB_IN_DIRECTION;
  setup->Request = BULK_ONLY_GETLUN_REQ;
  setup->Index   = (U16)pDev->bInterfaceNumber;
  setup->Value   = 0;
  setup->Length  = BULK_ONLY_GETLUN_LENGTH; // Length is one byte
  status         = _SetupRequest(pDev, urb, pDev->Ep0PhyTransferBuffer, &length, USBH_MSD_EP0_TIMEOUT);
  if (status) {
    USBH_WARN((USBH_MTYPE_MSD, "MSD: _BULKONLY_GetMaxLUN st: 0x%08x!",status));
  } else {
    if (length != BULK_ONLY_GETLUN_LENGTH) {
      USBH_WARN((USBH_MTYPE_MSD, "MSD: _BULKONLY_GetMaxLUN: invalid length rcv.: %d",length));
    } else {
      * maxLunIndex = * pDev->Ep0PhyTransferBuffer;
    }
  }
  return USBH_STATUS_SUCCESS;
}

/*********************************************************************
*
*       _ConvCommandBlockWrapper
*
*  Function description
*    _ConvCommandBlockWrapper copies the structure cbw to a byte pBuffer.
*    This function is independent from the byte order of the processor.
*    The pBuffer is in little endian byte format.
*    The minimum length of pBuffer must be CBW_LENGTH.
*/
static void _ConvCommandBlockWrapper(const COMMAND_BLOCK_WRAPPER * cbw, U8 * Buffer) {
  T_ASSERT_PTR(Buffer);
  T_ASSERT_PTR(cbw);
  USBH_StoreU32LE(Buffer,     cbw->Signature);          // index 0:Signature
  USBH_StoreU32LE(Buffer + 4, cbw->Tag);                // index:4 Tag
  USBH_StoreU32LE(Buffer + 8, cbw->DataTransferLength); // index:8 DataTransferLength
  Buffer[12] = cbw->Flags;
  Buffer[13] = cbw->Lun;
  Buffer[14] = cbw->Length;
}

/*********************************************************************
*
*       _SendCommandWriteData
*
*  Function description
*
*  Parameters
*    pCmdBuffer:    Command pBuffer, must contain a valid device command
*    CmdLength:    Size of command pBuffer, valid values:1-16
*    pDataBuffer:   Transfer pBuffer
*    pDataLength:   IN: length of pDataBuffer; OUT: transferred bytes
*
*  Return value
*    USBH_STATUS_LENGTH: false command length or data length
*    USBH_STATUS_COMMAND_FAILED: the device could not interpret the command.
*/
static USBH_STATUS _SendCommandWriteData(USBH_MSD_UNIT * pUnit, U8 * pCmdBuffer, U8 CmdLength, const U8 * pDataBuffer, U32 * pDataLength, U16 Timeout, T_BOOL SectorDataFlag) {
  COMMAND_BLOCK_WRAPPER    cbw; // Stores the request until completion
  COMMAND_STATUS_WRAPPER   csw;
  USBH_MSD_DEVICE             * pDev;
  int                      status;
  U8                     * cbwBuffer;
  T_BOOL                   recovery;
  U32                      length, dataLength;
  T_ASSERT(pUnit       != NULL);
  T_ASSERT(pCmdBuffer  != NULL);
  T_ASSERT(pDataBuffer != NULL);
  T_ASSERT(pDataLength != NULL);
  pDev = pUnit->pDev;              // Get the pointer to the device
  if (pDev == NULL) {
    USBH_WARN((USBH_MTYPE_MSD, "MSD: _SendCommandWriteData: Device removed!"));
    return USBH_STATUS_INVALID_PARAM;
  }
  T_ASSERT_MAGIC(pDev, USBH_MSD_DEVICE);
  T_ASSERT(pUnit->pDev->pDriver != NULL);
  T_ASSERT_PTR(pDev->CbwPhyTransferBuffer);
  cbwBuffer  = pDev->CbwPhyTransferBuffer;
  if ((CmdLength == 0) || (CmdLength > COMMAND_WRAPPER_CDB_FIELD_LENGTH)) {
    USBH_WARN((USBH_MTYPE_MSD, "MSD: _SendCommandWriteData: CmdLength: %u",CmdLength));
    return USBH_STATUS_LENGTH;
  }
  ZERO_MEMORY(cbwBuffer, CBW_LENGTH);
  _FillCBW(&cbw, 0, *pDataLength, CBW_FLAG_WRITE, pUnit->Lun, CmdLength);       // Setup the command block wrapper
  _ConvCommandBlockWrapper(&cbw, cbwBuffer);                                   // Convert the command wrapper to a cbw pBuffer
  USBH_MEMCPY(&cbwBuffer[COMMAND_WRAPPER_CDB_OFFSET], pCmdBuffer, CmdLength); // Copy the command to the command block offset
  dataLength    = * pDataLength;
  * pDataLength = 0;                                                          // Set the parameter Length to zero
  recovery      = FALSE;
  status        = USBH_STATUS_ERROR;
  for (; ;) {
    if (recovery) {
      status = _BULKONLY_ErrorRecovery(pDev);                                  // Send a reset recovery command
      if (status) {                                                          // Recovery failed
        USBH_WARN((USBH_MTYPE_MSD, "MSD: _SendCommandWriteData: USBH_MSD_ErrorRecovery"));
        break;
      }
    }
    //
    // COMMAND PHASE
    //
    length = CBW_LENGTH;
    _WriteTag(pDev, cbwBuffer);
    status = _WriteSync(pDev, cbwBuffer, &length, USBH_MSD_WRITE_TIMEOUT, FALSE, SectorDataFlag);
    if (status) {
      USBH_WARN((USBH_MTYPE_MSD, "MSD: _SendCommandWriteData: Command Phase"));
      USBH_PRINT_STATUS_VALUE(USBH_MTYPE_MSD, status);
      if (status == USBH_STATUS_STALL) {
        USBH_LOG((USBH_MTYPE_MSD_INTERN, "MSD: DlResetPipe Ep-address: %u",pDev->BulkOutEp));
        status = _ResetPipe(pDev, pDev->BulkOutEp);
        if (status) {      // Reset error
          USBH_WARN((USBH_MTYPE_MSD, "MSD: _SendCommandWriteData: _ResetPipe!"));
        }
      }
      recovery = TRUE;
      continue;
    }
    //
    // DATA PHASE Bulk OUT
    //
    length = dataLength;
    status = _WriteSync(pDev, pDataBuffer, &length, Timeout, TRUE, SectorDataFlag);
    if (status) {          // Error
      USBH_WARN((USBH_MTYPE_MSD, "MSD: _SendCommandWriteData: Data OUT Phase"));
      if (status == USBH_STATUS_STALL) {
        USBH_LOG((USBH_MTYPE_MSD_INTERN, "MSD: DlResetPipe Ep-address: %u",pDev->BulkOutEp));
        status = _ResetPipe(pDev, pDev->BulkOutEp);
        if (status) {      // Reset error
          USBH_WARN((USBH_MTYPE_MSD, "MSD: _SendCommandWriteData: _ResetPipe!"));
        }
      } else {
        USBH_WARN((USBH_MTYPE_MSD, "MSD: _SendCommandWriteData data: other error!"));
        recovery = TRUE;   // Start error recovery
        continue;
      }
    }
    //
    // STATUS PHASE
    //
    status = _ReadCSW(pDev, &cbw, &csw);
    if (!status) {         // Success
      if (csw.Status != CSW_STATUS_PHASE_ERROR) {
        if (csw.Residue) { // This is not implemented in the same way from vendors!! */
          * pDataLength = cbw.DataTransferLength - csw.Residue;
        } else {
          * pDataLength = length;
        }

        if (csw.Status == CSW_STATUS_FAIL) {
          status = USBH_STATUS_COMMAND_FAILED;
        } else {           // On success
          if (*pDataLength != length) {
            USBH_WARN((USBH_MTYPE_MSD, "MSD: _SendCommandWriteData: invalid Residue!"));
          }
        }
        pDev->ErrorRecoveryCt = 0;
        break;
      }
    // On phase error repeat the command
    }
    recovery = TRUE;       // Repeat the same command with error recovery
  }
  return status;
}

/*********************************************************************
*
*       _SendCommandReadData
*
*  Function description
*    Transport layer function to send a command for receiving data
*
*  Parameters
*    pCmdBuffer:    Command pBuffer, must contain a valid device command
*    CmdLength:    Size of command pBuffer, valid values:1-16
*    pDataBuffer:   Transfer pBuffer
*    pDataLength:   IN: length of pDataBuffer; OUT: transferred bytes
*
*  Return value
*    USBH_STATUS_LENGTH: false command length or data length
*    USBH_STATUS_COMMAND_FAILED: the device could not interpret the command.
*/
static USBH_STATUS _SendCommandReadData(USBH_MSD_UNIT * pUnit, U8 * pCmdBuffer, U8 CmdLength, U8 * pDataBuffer, U32 * pDataLength, U16 Timeout, T_BOOL SectorDataFlag) {
  COMMAND_BLOCK_WRAPPER    cbw; // stores the request until completion
  COMMAND_STATUS_WRAPPER   csw;
  T_BOOL                   recovery;
  U32                      length, dataLength;
  USBH_STATUS              status;
  USBH_MSD_DEVICE        * pDev;
  U8                     * cbwBuffer = NULL;

  T_ASSERT(pUnit       != NULL);
  T_ASSERT(pCmdBuffer  != NULL);
  T_ASSERT(pDataBuffer != NULL);
  T_ASSERT(pDataLength != NULL);
  pDev                  = pUnit->pDev;
  if (pDev == NULL) {
    USBH_WARN((USBH_MTYPE_MSD, "MSD: _SendCommandReadData: Device removed!"));
    return USBH_STATUS_INVALID_PARAM;
  }
  T_ASSERT_MAGIC(pDev, USBH_MSD_DEVICE);
  T_ASSERT(pUnit->pDev->pDriver != NULL);
  cbwBuffer = pDev->CbwPhyTransferBuffer;
  if ((CmdLength == 0) || (CmdLength > COMMAND_WRAPPER_CDB_FIELD_LENGTH)) {
    USBH_WARN((USBH_MTYPE_MSD, "MSD: _SendCommandReadData: CmdLength: %u",CmdLength));
    return USBH_STATUS_INVALID_PARAM;
  }
  ZERO_MEMORY(cbwBuffer, CBW_LENGTH);
  _FillCBW(&cbw, 0, *pDataLength, CBW_FLAG_READ, pUnit->Lun, CmdLength);
  _ConvCommandBlockWrapper(&cbw, cbwBuffer); // Convert the struct cbw to a cbw pBuffer and attach pCmdBuffer
  USBH_MEMCPY(&cbwBuffer[COMMAND_WRAPPER_CDB_OFFSET], pCmdBuffer, CmdLength);
  dataLength   = *pDataLength;                    // Transfer the command
  * pDataLength = 0;                              // Clear the returned length
  recovery     = FALSE;
  status       = USBH_STATUS_ERROR;
  for (; ;) {
    if (recovery) {
      status = _BULKONLY_ErrorRecovery(pDev);
      if (status) {                              // Recover counter is invalid after all tries
        USBH_WARN((USBH_MTYPE_MSD, "MSD: _SendCommandReadData: _BULKONLY_ErrorRecovery, stop"));
        break;
      }
    }
    //
    // COMMAND PHASE
    //
    length = CBW_LENGTH;
    _WriteTag(pDev, cbwBuffer);
    status = _WriteSync(pDev, cbwBuffer, &length, USBH_MSD_WRITE_TIMEOUT, FALSE, SectorDataFlag);
    if (status) {
      USBH_WARN((USBH_MTYPE_MSD, "MSD: _SendCommandReadData: Command Phase"));
      recovery = TRUE;
      continue;
    }
    //
    // DATA PHASE
    //
    if (dataLength) {                            // dataLength always contains the original length
      length = dataLength;                       // data IN transfer (length != 0)
      status = _ReadSync(pDev, pDataBuffer, &length, Timeout, TRUE, SectorDataFlag);
      if (status) {                              // Error
        USBH_WARN((USBH_MTYPE_MSD, "MSD: _SendCommandReadData: Data IN Phase"));
        if (status == USBH_STATUS_STALL) {        // Reset the IN pipe
          USBH_LOG((USBH_MTYPE_MSD_INTERN, "MSD: DlResetPipe Ep-address: %u",pDev->BulkInEp));
          status = _ResetPipe(pDev, pDev->BulkInEp);
          if (status) {                          // Reset error
            USBH_WARN((USBH_MTYPE_MSD, "MSD: _ReadCSW: reset error!"));
          }
        } else {
          USBH_WARN((USBH_MTYPE_MSD, "MSD: _SendCommandReadData data: other error!"));
          recovery = TRUE;                       // Start error recovery
          continue;
        }
      } else {
        if ((length % pDev->BulkMaxPktSize) == CSW_LENGTH) {              // Last data packet length is CSW_LENGTH, check command status
          if (!_ConvBufferToStatusWrapper(pDataBuffer + length - CSW_LENGTH, length, &csw)) {
            if (_IsCSWValidandMeaningful(pDev, &cbw, &csw, CSW_LENGTH)) { // device has stopped the data transfer by sending an CSW
              // This occurs if the toggle bit is not reset after USB clear feature endpoint halt!
              USBH_WARN((USBH_MTYPE_MSD, "MSD: _SendCommandReadData: device breaks the data phase by sending an CSW: CSW-Status: %d!", (int)csw.Status));
              if (csw.Status != CSW_STATUS_PHASE_ERROR) {                // No phase error
                if (csw.Residue) { // This is not implemented in the same way from vendors!
                  * pDataLength = cbw.DataTransferLength - csw.Residue;
                } else {
                  * pDataLength = length - CSW_LENGTH;                    // CSW_LENGTH because CSW sent at the end of the pBuffer
                }
                if (csw.Status == CSW_STATUS_FAIL) {
                  status = USBH_STATUS_COMMAND_FAILED;
                } else {                                                 // on success
                  if (*pDataLength != length - CSW_LENGTH) {
                    USBH_WARN((USBH_MTYPE_MSD, "MSD: _SendCommandReadData: invalid Residue!"));
                  }
                }
                pDev->ErrorRecoveryCt = 0;
                break; // This breaks the for loop: indirect return!
              }
              recovery = TRUE;
              continue; // Repeat all
            }
          }
        }
      }
    }
    //
    // STATUS PHASE
    //
    status = _ReadCSW(pDev, &cbw, &csw);
    if (!status) { // success
      if (csw.Status != CSW_STATUS_PHASE_ERROR) { // no phase error
        if (csw.Residue) { // This is not implemented in the same way from vendors!
          * pDataLength = cbw.DataTransferLength - csw.Residue;
        } else {
          * pDataLength = length;
        }
        if (csw.Status == CSW_STATUS_FAIL) {
          status = USBH_STATUS_COMMAND_FAILED;
        } else {
          if (*pDataLength != length) {
            USBH_WARN((USBH_MTYPE_MSD, "MSD: _SendCommandReadData: invalid Residue expected:%d rcv:%d!", *pDataLength,length));
          }
        }
        pDev->ErrorRecoveryCt = 0;
        break; // Return
      }
    // On phase error repeat the same command
    }
    // Repeat the same command with error recovery
    recovery = TRUE; // Recover the device as long as the recover counter is valid
  }
  return status;
}

/*********************************************************************
*
*       _SearchDevicePtrByInterfaceID
*
*  Function description
*    Searches in the global driver object an USBH MSD device that has the
*    same interfaceID. On fail a NULL is returned.
*/
static USBH_MSD_DEVICE * _SearchDevicePtrByInterfaceID(USBH_INTERFACE_ID InterfaceID) {
  int          i;
  USBH_MSD_DEVICE * pDev;
  pDev = &USBH_MSD_Global.aDevice[0];
  for (i = 0; i < USBH_MSD_MAX_DEVICES; i++, pDev++) {
    if (pDev->IsValid) {
      if (pDev->InterfaceID == InterfaceID) {
        return pDev;
      }
    }
  }
  return NULL;
}

/*********************************************************************
*
*       _FreeLuns
*
*  Function description
*    Frees the unit resources of the device.
*/
static void _FreeLuns(USBH_MSD_DEVICE * pDev) {
  int i;
  USBH_LOG((USBH_MTYPE_MSD, "MSD: _FreeLuns Luns: %d",pDev->UnitCnt));
  for (i = 0; i < pDev->UnitCnt; i++) { // invalidate the unit object
    pDev->apUnit[i]->pDev    = (USBH_MSD_DEVICE * )NULL;
    pDev->apUnit[i]->Magic  = 0;
  }
  pDev->UnitCnt     = 0;
  pDev->OpenUnitCt = 0;
}

/*********************************************************************
*
*       _DeleteDevice
*
*  Function description
*    Deletes all units that are connected with the device and marks the
*    device object as unused by setting the driver handle to zero.
*/
static void _DeleteDevice(USBH_MSD_DEVICE * pDev) {
  USBH_LOG((USBH_MTYPE_MSD, "MSD: USBH_MSD_FreeDevObject"));
  if (NULL != pDev->InterfaceHandle) {
    USBH_CloseInterface(pDev->InterfaceHandle);
    pDev->InterfaceHandle = NULL;
  }
  if (NULL != pDev->UrbEvent) {
    USBH_OS_FreeEvent(pDev->UrbEvent);
    pDev->UrbEvent = NULL;
  }
  _FreeLuns(pDev); // Free all units
#if (USBH_DEBUG > 1)
  if (pDev->RefCnt != 0) {
    USBH_WARN((USBH_MTYPE_MSD, "MSD: USBH_MSD_FreeDevice: RefCnt not zero: %d!",pDev->RefCnt));
  }
#endif
  pDev->Removed   = TRUE;
  pDev->Magic     = 0;
  pDev->IsValid = FALSE;

  if (NULL != pDev->CbwPhyTransferBuffer) {
    UrbBufferFreeTransferBuffer(pDev->CbwPhyTransferBuffer);
  }
  if (NULL != pDev->CswPhyTransferBuffer) {
    UrbBufferFreeTransferBuffer(pDev->CswPhyTransferBuffer);
  }
  if (NULL != pDev->Ep0PhyTransferBuffer) {
    UrbBufferFreeTransferBuffer(pDev->Ep0PhyTransferBuffer);
  }
}

/*********************************************************************
*
*       _IncRefCnt
*
*  Function description
*/
static void _IncRefCnt(USBH_MSD_DEVICE * pDev) {
  pDev->RefCnt++;
  USBH_LOG((USBH_MTYPE_MSD_INTERN, "MSD: _IncRefCnt: %d ",pDev->RefCnt));
}

/*********************************************************************
*
*       _DecRefCnt
*
*  Function description
*/
static void _DecRefCnt(USBH_MSD_DEVICE * pDev) {
  pDev->RefCnt--;
  if (pDev->RefCnt < 0) {
    USBH_PANIC("");  //((USBH_MTYPE_MSD, "MSD: Invalid USBH MSD RefCnt: %d",(devPtr)->RefCnt));
  }
  USBH_LOG((USBH_MTYPE_MSD_INTERN, "MSD: _DecRefCnt: %d ",pDev->RefCnt));
  if (pDev->RefCnt == 0) {
    _DeleteDevice(pDev);
  }
}

/*********************************************************************
*
*       _MarkDeviceAsRemoved
*
*  Function description
*    If an device with the interfaceID exists the remove Flag is set
*    and the reference counter is decremented.
*/
static void _MarkDeviceAsRemoved(USBH_INTERFACE_ID InterfaceId) {
  USBH_MSD_DEVICE  * pDev;

  pDev = _SearchDevicePtrByInterfaceID(InterfaceId);
  if (pDev) {  // Send Remove notification
    if (USBH_MSD_Global.pfLunNotification) {
      USBH_MSD_Global.pfLunNotification(USBH_MSD_Global.pContext, pDev->DeviceIndex, USBH_MSD_EVENT_REMOVE);
    }
    pDev->Removed = TRUE;
    _DecRefCnt(pDev); // Delete the device object
  }
#if (USBH_DEBUG > 1)
  else {
    USBH_WARN((USBH_MTYPE_MSD, "MSD: _MarkDeviceAsRemoved: no device found!"));
  }
#endif

}

/*********************************************************************
*
*       _AllocLuns
*
*  Function description
*    Allocates logical units, saves the unit pointer in the device object.
*
*  Parameters:
*    pDev:     Pointer to a USB device
*    MaxLun:  Maximum logical unit number to use (zero based)
*  Return:
*    0 on success
*    other values on error
*/
static USBH_STATUS _AllocLuns(USBH_MSD_DEVICE * pDev, int MaxLunIndex) {
  int          i;
  int          NumUnits;
  USBH_STATUS  status;
  USBH_MSD_UNIT  * pUnit;

  NumUnits = USBH_COUNTOF(USBH_MSD_Global.aUnit);
  T_ASSERT_MAGIC(pDev, USBH_MSD_DEVICE);
  USBH_LOG((USBH_MTYPE_MSD_INTERN, "MSD: _AllocLuns: MaxLunIndex: %d",MaxLunIndex));
  if (ARRAY_ELEMENTS(pDev->apUnit) <= MaxLunIndex) {
    USBH_WARN((USBH_MTYPE_MSD, "MSD: _AllocLuns: Too many Luns: %d",MaxLunIndex+1));
    return USBH_STATUS_RESOURCES;
  }
  status = USBH_STATUS_ERROR;

  //
  // Check if we already have a unit
  //
  for (i = 0; i < NumUnits; i++) {
    pUnit   = &USBH_MSD_Global.aUnit[i];
    if (pUnit->pDev == pDev) {
      USBH_WARN((USBH_MTYPE_MSD, "MSD: _AllocLuns: Units with the same device already allocated?"));
      return USBH_STATUS_ERROR;
    }
  }
  //
  // Allocate units and save it in the device object
  //
  T_ASSERT(0 == pDev->UnitCnt);
  T_ASSERT(0 == pDev->OpenUnitCt);
  for (i = 0; i < NumUnits; i++) {
    pUnit   = &USBH_MSD_Global.aUnit[i];
    if (pUnit->pDev == NULL) {
      ZERO_MEMORY(pUnit, sizeof(USBH_MSD_UNIT)); // Clear all data
      pUnit->Magic  = USBH_MSD_UNIT_MAGIC;
      pUnit->pDev    = pDev;
      pUnit->Lun    = (U8)pDev->UnitCnt;       // Start with LUN number zero
      pUnit->BytesPerSector = USBH_MSD_DEFAULT_SECTOR_SIZE;
      pDev->apUnit[pDev->UnitCnt] = pUnit;        // Save units also in the device
      pDev->UnitCnt++;

      if (MaxLunIndex == 0) {
        status = USBH_STATUS_SUCCESS;
        break;
      }
      MaxLunIndex--;
    }
  }
  return status;
}

/*********************************************************************
*
*       _InitDevice
*
*  Function description
*    Makes an basic initialization of the USBH MSD device object.
*    Physical transfer buffers are allocated if needed.
*/
static USBH_STATUS _InitDevice(USBH_MSD_DEVICE * pDev, USBH_INTERFACE_ID interfaceID) {
  USBH_STATUS status = USBH_STATUS_SUCCESS;
  USBH_LOG((USBH_MTYPE_MSD_INTERN, "MSD: USBH_MSD_InitDevObject"));
  ZERO_MEMORY(pDev, sizeof(USBH_MSD_DEVICE));
  pDev->Magic                = USBH_MSD_DEVICE_MAGIC;
  pDev->pDriver            = &USBH_MSD_Global;
  pDev->IsValid            = TRUE;
  pDev->InterfaceID          = interfaceID;
  pDev->RefCnt                = 1; // Initial reference
  pDev->CbwPhyTransferBuffer = UrbBufferAllocateTransferBuffer(CBW_LENGTH);
  if (NULL == pDev->CbwPhyTransferBuffer) {
    status = USBH_STATUS_RESOURCES;
    USBH_WARN((USBH_MTYPE_MSD, "MSD: _InitDevice: allocate transfer buffer!"));
    goto exit;
  }
  pDev->CswPhyTransferBuffer = UrbBufferAllocateTransferBuffer(CSW_LENGTH);
  if (NULL == pDev->CswPhyTransferBuffer) {
    status = USBH_STATUS_RESOURCES;
    USBH_WARN((USBH_MTYPE_MSD, "MSD: _InitDevice: allocate transfer buffer!"));
    goto exit;
  }
  pDev->Ep0PhyTransferBuffer = UrbBufferAllocateTransferBuffer(MAX_EP0_TRANSFER_BUFFER_LENGTH);
  if (NULL == pDev->Ep0PhyTransferBuffer) {
    status = USBH_STATUS_RESOURCES;
    USBH_WARN((USBH_MTYPE_MSD, "MSD: _InitDevice: allocate transfer buffer!"));
    goto exit;
  }
  return USBH_STATUS_SUCCESS;
  exit:
  if (NULL != pDev->CbwPhyTransferBuffer) {
    UrbBufferFreeTransferBuffer(pDev->CbwPhyTransferBuffer);
  }
  if (NULL != pDev->CswPhyTransferBuffer) {
    UrbBufferFreeTransferBuffer(pDev->CswPhyTransferBuffer);
  }
  if (NULL != pDev->Ep0PhyTransferBuffer) {
    UrbBufferFreeTransferBuffer(pDev->Ep0PhyTransferBuffer);
  }
  return status;
}


/*********************************************************************
*
*       _NewDevice
*
*  Function description
*    Allocates USBH MSD device object and makes an basic initialization. Set
*    the reference counter to one.No unit is available and all function
*    pointers to protocol and transport layer functions are NULL.
*
*  Parameters:
*    in the debug version interfaceID is checked if in use
*/
static USBH_MSD_DEVICE * _NewDevice(USBH_INTERFACE_ID interfaceID) {
  int          i;
  USBH_MSD_DEVICE * pDev;
  USBH_STATUS   status;
  USBH_LOG((USBH_MTYPE_MSD_INTERN, "MSD: _NewDevice"));

#if (USBH_DEBUG > 1)
  pDev = _SearchDevicePtrByInterfaceID(interfaceID); // check that not the same ID does exists
  if (NULL != pDev) {
    USBH_WARN((USBH_MTYPE_MSD, "MSD: _NewDevice: Same interface ID exists!"));
  }
#endif
  for (i = 0; i < USBH_MSD_MAX_DEVICES; i++) {
    pDev = &USBH_MSD_Global.aDevice[i];
    if (!pDev->IsValid) {
      status = _InitDevice(pDev, interfaceID);
      if (status) { // On error
        USBH_WARN((USBH_MTYPE_MSD, "MSD: _NewDevice: _InitDevice: st: 0x%08x!",status));
        return NULL;
      } else {
        pDev->DeviceIndex = i;
        return pDev;
      }
    }
  }
  return NULL;
}

/*********************************************************************
*
*       _InitTransportLayer
*
*  Function description
*    Init the protocol and transport function pointer in the device,
*    Returns 0 for success, another value indicates an error
*/
static USBH_STATUS _InitTransportLayer(USBH_MSD_DEVICE * pDev) {
  USBH_STATUS status = USBH_STATUS_SUCCESS;
  USBH_LOG((USBH_MTYPE_MSD_INTERN, "MSD: _InitTransportLayer"));
  switch (pDev->Interfaceprotocol) {
    case PROTOCOL_BULK_ONLY:
      USBH_LOG((USBH_MTYPE_MSD_INTERN, "MSD: INFO Interface protocol: bulk only!"));
      pDev->ErrorRecoveryCt    = 0;
      pDev->TlReset            = _BULKONLY_MassStorageReset;
      pDev->TlGetMaxLUN        = _BULKONLY_GetMaxLUN;
      pDev->TlCommandReadData  = _SendCommandReadData;
      pDev->TlCommandWriteData = _SendCommandWriteData;
      break;
    default:
      USBH_WARN((USBH_MTYPE_MSD, "MSD: _InitTransportLayer: invalid protocol: %u!",pDev->Interfaceprotocol));
      status = USBH_STATUS_ERROR;
  }
  if (status) {
    return status;
  }
  switch (pDev->InterfaceSubClass) {
    case SUBCLASS_6:
      USBH_LOG((USBH_MTYPE_MSD_INTERN, "MSD: INFO Interface sub class: SCSI transparent protocol!"));
      pDev->pPhysLayerAPI = &USBH_MSD_PhysLayerSC06;
      break;
    case SUBCLASS_5:  //SFF8070i, floppy disk drivers (see also UFI)
      USBH_LOG((USBH_MTYPE_MSD_INTERN, "MSD: INFO Interface sub class:  valid sub class: SFF8070i!"));
      pDev->pPhysLayerAPI = &USBH_MSD_PhysLayerSC05;
      break;
    default:
      USBH_WARN((USBH_MTYPE_MSD, "MSD: _InitTransportLayer: Invalid sub class: %d!",pDev->InterfaceSubClass));
      status = USBH_STATUS_ERROR;
  }
  return status;
}

/*********************************************************************
*
*       _GetAndSavelEndpointInformations
*
*  Function description
*/
static USBH_STATUS _GetAndSavelEndpointInformations(USBH_MSD_DEVICE * pDev) {
  USBH_STATUS              status;
  USBH_EP_MASK             ep_mask;
  unsigned int            count;
  U8                      desc[USB_ENDPOINT_DESCRIPTOR_LENGTH];
  USB_ENDPOINT_DESCRIPTOR usbEpDesc;
  // Get bulk IN endpoint
  ep_mask.Mask      = USBH_EP_MASK_DIRECTION | USBH_EP_MASK_TYPE;
  ep_mask.Direction = USB_TO_HOST;
  ep_mask.Type      = USB_EP_TYPE_BULK;
  status            = USBH_GetEndpointDescriptor(pDev->InterfaceHandle, 0, &ep_mask, desc, sizeof(desc), &count);
  if (status || count != USB_ENDPOINT_DESCRIPTOR_LENGTH) {
    USBH_WARN((USBH_MTYPE_MSD, "MSD: USBH_MSD_GetAndSaveProtocolEndpoints:USBH_GetEndpointDescriptor: 0x%08x",status));
    return status;
  }
  // Save information
  _ConvEndpointDesc(desc, &usbEpDesc);
  pDev->BulkMaxPktSize = usbEpDesc.wMaxPacketSize;
  pDev->BulkInEp       = usbEpDesc.bEndpointAddress;
  // Use previous mask change direction to bulk OUT
  ep_mask.Direction = 0;
  status            = USBH_GetEndpointDescriptor(pDev->InterfaceHandle, 0, &ep_mask, desc, sizeof(desc), &count);
  if (status) {
    USBH_WARN((USBH_MTYPE_MSD, "MSD: USBH_MSD_GetAndSaveProtocolEndpoints:USBH_GetEndpointDescriptor: 0x%08x",status));
    return status;
  }
  _ConvEndpointDesc(desc, &usbEpDesc);
  if (pDev->BulkMaxPktSize != usbEpDesc.wMaxPacketSize) {
    USBH_WARN((USBH_MTYPE_MSD, "MSD: USBH_MSD_GetAndSaveProtocolEndpoints: different max.packet sizes between ep: 0x%x and ep: 0x%x", pDev->BulkInEp, usbEpDesc.bEndpointAddress));
    return USBH_STATUS_LENGTH;
  }
  pDev->BulkOutEp = usbEpDesc.bEndpointAddress;
  return USBH_STATUS_SUCCESS;
}

/*********************************************************************
*
*       _ValidateInterface
*
*  Function description
*/
static USBH_STATUS _ValidateInterface(USBH_MSD_DEVICE * pDev, USBH_INTERFACE_INFO * info) {
  USBH_STATUS status = USBH_STATUS_SUCCESS;
  U8         interfaceClass;
  U8         interfaceSubClass;
  U8         interfaceProtocol;
  interfaceClass    = info->Class;
  interfaceSubClass = info->SubClass;
  interfaceProtocol = info->Protocol;
  if (interfaceClass != MASS_STORAGE_CLASS) {
    USBH_WARN((USBH_MTYPE_MSD, "MSD: : USBH_MSD_CheckInterface: Invalid device class: %u",(unsigned int)interfaceClass));
    status = USBH_STATUS_ERROR;
    goto exit;
  }
  switch (interfaceSubClass) {
    case SUBCLASS_6:         // Valid sub class
      break;

    case SUBCLASS_5:         // Valid sub class
      break;
    default:
      USBH_WARN((USBH_MTYPE_MSD, "MSD: USBH_MSD_CheckInterface: Invalid sub class: %u",(unsigned int)interfaceSubClass));
      status = USBH_STATUS_INTERFACE_SUB_CLASS;
      goto exit;
  }
  switch (interfaceProtocol) {
    case PROTOCOL_BULK_ONLY: // Valid protocol
      break;
    default:
      USBH_WARN((USBH_MTYPE_MSD, "MSD: USBH_MSD_CheckInterface: Invalid interface protocol: %u",(unsigned int)interfaceProtocol));
      status = USBH_STATUS_INTERFACE_PROTOCOL;
  }
  exit:
  if (!status) {             // On success
    pDev->Interfaceprotocol = interfaceProtocol;
    pDev->InterfaceSubClass = interfaceSubClass;
  }
  return status;
}

/*********************************************************************
*
*       _CheckAndOpenInterface
*
*  Function description
*    _CheckAndOpenInterface checks if the interface contains an valid
*    USB mass storage class interface.
*
*  Return value:
*    !0: error
*     0: no error
*/
static USBH_STATUS _CheckAndOpenInterface(USBH_MSD_DEVICE * pDev) {
  USBH_STATUS status  = USBH_STATUS_ERROR;
  USBH_INTERFACE_INFO   iface_info;
  USBH_LOG((USBH_MTYPE_MSD_INTERN, "MSD: _CheckAndOpenInterface"));
  T_ASSERT_MAGIC(pDev, USBH_MSD_DEVICE);
  if (pDev->Removed) {
    USBH_WARN((USBH_MTYPE_MSD, "MSD: _CheckAndOpenInterface: Device removed!"));
    return USBH_STATUS_INVALID_PARAM;
  }
  status = USBH_GetInterfaceInfo(pDev->InterfaceID, &iface_info);
  if (USBH_STATUS_SUCCESS != status) {
    USBH_WARN((USBH_MTYPE_MSD, "MSD: _CheckAndOpenInterface: interface info failed 0x%08x!",status));
    return status;
  }
  status = _ValidateInterface(pDev, &iface_info);
  if (USBH_STATUS_SUCCESS != status) {
    USBH_WARN((USBH_MTYPE_MSD, "MSD: _CheckAndOpenInterface: invalid mass storage interface 0x%08x!",status));
    return status;
  }
  status = USBH_OpenInterface(pDev->InterfaceID, TRUE, &pDev->InterfaceHandle); // Open interface exlusive
  if (status) {
    USBH_WARN((USBH_MTYPE_MSD, "MSD: _CheckAndOpenInterface: USBH_OpenInterface 0x%08x!",status));
    return status;
  }
  status = _GetAndSavelEndpointInformations(pDev);                                             // Save endpoint information
  if (status) {                                                                                  // Error
    USBH_WARN((USBH_MTYPE_MSD, "MSD: _CheckAndOpenInterface: USBH_MSD_GetAndSaveProtocolEndpoints!"));
    return status;
  }
  return status;
}

/*********************************************************************
*
*       _StartDevice
*
*  Function description
*    First configures the device by a call to DlInitDevice, then queries
*    the number of LUNs for the device, after that it allocates the LUNs
*    of the device and finally initializes the device.
*/
static USBH_STATUS _StartDevice(USBH_MSD_DEVICE * pDev) {
  int        maxLun;
  USBH_STATUS status;
  USBH_LOG((USBH_MTYPE_MSD_INTERN, "MSD: _StartDevice IN-Ep: 0x%x Out-Ep: 0x%x",pDev->BulkInEp,pDev->BulkOutEp));
  T_ASSERT_MAGIC(pDev, USBH_MSD_DEVICE);
  if (pDev->Removed) {
    USBH_WARN((USBH_MTYPE_MSD, "MSD: _StartDevice: Device removed!"));
    return USBH_STATUS_INVALID_PARAM;
  }
  status = pDev->TlGetMaxLUN(pDev, &maxLun);
  if (status) {                                     // On error
    if (status == USBH_STATUS_STALL) {               // stall is allowed
      maxLun = 0;
    } else {
      USBH_WARN((USBH_MTYPE_MSD, "MSD: _StartDevice: TlGetMaxLUN: st: 0x%08x",status));
      return status;
    }
  }
  status = _AllocLuns(pDev, maxLun);              // Allocate the logical units for this device
  if (status) {
    USBH_WARN((USBH_MTYPE_MSD, "MSD: _StartDevice: no LUN resources"));
    return status;
  }
  status = pDev->pPhysLayerAPI->PlInitSequence(pDev); // Initialize the device with a protocol specific sequence
  return status;
}

/*********************************************************************
*
*       _AddDevice
*
*  Function description
*    Adds a USB mass storage interface to the library.
*/
static USBH_STATUS _AddDevice(USBH_INTERFACE_ID InterfaceID) {
  USBH_STATUS   Status;
  U32           Length;
  USBH_MSD_DEVICE *  pDev;

  USBH_LOG((USBH_MTYPE_MSD, "MSD: _AddDevice:"));
  pDev = _NewDevice(InterfaceID);        // Allocate device, REFCT=1
  if (pDev == NULL) {
    USBH_WARN((USBH_MTYPE_MSD, "MSD: _AddDevice: USBH_MSD_AllocDevice!"));
    return USBH_STATUS_RESOURCES;
  }
  Status = _CheckAndOpenInterface(pDev); // Check the interface descriptor and save endpoint information.
  if (Status) {
    USBH_WARN((USBH_MTYPE_MSD, "MSD: _AddDevice:_CheckAndOpenInterface st:%d!",Status));
    goto exit;
  }
  if (CSW_LENGTH > pDev->BulkMaxPktSize) {  // Calculate an buffer Length that is an multiple of maximum packet size and greater than CSW_LENGTH
    Length = (CSW_LENGTH / pDev->BulkMaxPktSize) * pDev->BulkMaxPktSize;
    if (CSW_LENGTH > Length) {
      Length += pDev->BulkMaxPktSize;
    }
  } else {
    Length = pDev->BulkMaxPktSize;
  }
  USBH_LOG((USBH_MTYPE_MSD_INTERN, "MSD: _AddDevice:CSW buffer Length: %lu!",Length));
  pDev->UrbEvent = USBH_OS_AllocEvent();
  if (pDev->UrbEvent == NULL) {
    USBH_WARN((USBH_MTYPE_MSD, "MSD: _AddDevice: USBH_OS_AllocEvent"));
    goto exit;
  }
  Status = _InitTransportLayer(pDev);    // Initialize the transport and protocol layer
  if (Status) {
    USBH_WARN((USBH_MTYPE_MSD, "MSD: _AddDevice: USBH_MSD_InitTranportLayer st:%d!",Status));
    goto exit;
  }
  USBH_LOG((USBH_MTYPE_MSD, "MSD: _AddDevice: subclass: 0x%x protocol: 0x%x", (int)pDev->InterfaceSubClass,(int)pDev->Interfaceprotocol));
  Status = _StartDevice(pDev);           // Retrieve information of the mass storage device and save it
  if (Status) {                            // Operation failed
    USBH_WARN((USBH_MTYPE_MSD, "MSD: _AddDevice: _StartDevice:Invalid device! st:%d!",Status));
    goto exit;
  }
  if (USBH_MSD_Global.pfLunNotification != NULL) {   // On success call the USBH MSD notification function
    USBH_MSD_Global.pfLunNotification(USBH_MSD_Global.pContext, pDev->DeviceIndex, USBH_MSD_EVENT_ADD);
  }

exit:
  if (Status) {                            // Error, release the device object
    _DecRefCnt(pDev);
  } else {                                 // Success
    USBH_LOG((USBH_MTYPE_MSD_INTERN, "MSD: _AddDevice success! LUNs: %d",pDev->UnitCnt));
  }
  return Status;
}

/*********************************************************************
*
*       _OnDeviceNotify
*
*  Function description
*    Called if a USB Mass storage interface is found.
*/
static void _OnDeviceNotify(void * Context, USBH_PNP_EVENT Event, USBH_INTERFACE_ID InterfaceID) {
  UNUSED_ALWAYS(Context);
  switch (Event) {
    case USBH_AddDevice:
      USBH_LOG((USBH_MTYPE_MSD_INTERN, "MSD: _OnDeviceNotify: USBH_AddDevice InterfaceId: %u !",InterfaceID));
      _AddDevice(InterfaceID); // -1 means check all interfaces
      break;
    case USBH_RemoveDevice:
      USBH_LOG((USBH_MTYPE_MSD_INTERN, "MSD: _OnDeviceNotify: USBH_RemoveDevice InterfaceId: %u !",InterfaceID));
      _MarkDeviceAsRemoved(InterfaceID);
      break;
    default:
      ;
  }
}

/*********************************************************************
*
*       _UsbDeviceReset
*
*  Function description
*/
/*
static USBH_STATUS _UsbDeviceReset(USBH_MSD_DEVICE * pDev) {
  USBH_STATUS   status;
  URB        * urb;
  T_ASSERT_MAGIC(pDev, USBH_MSD_DEVICE);
  T_ASSERT_PTR  (pDev->InterfaceHandle);
  urb                    = &pDev->ControlUrb;
  urb->Header.Function   = USBH_FUNCTION_RESET_DEVICE;
  urb->Header.Completion = NULL;
  status = USBH_SubmitUrb(pDev->InterfaceHandle, urb);      // No need to call _SubmitUrbAndWait because USBH_FUNCTION_RESET_DEVICE does never return with USBH_STATUS_PENDING

  if (status) { // Reset pipe does not wait
    USBH_WARN((USBH_MTYPE_MSD, "MSD: _UsbDeviceReset: USBH_SubmitUrb st: 0x%08x",status));
    status = USBH_STATUS_ERROR;
  }
  return status;
}
*/
/*********************************************************************
*
*       Global Library functions
*
**********************************************************************
*/

/*********************************************************************
*
*       USBH_MSD_Init
*
*  Function description
*    USBH_MSD_Init initializes the USB Mass Storage Class Driver
*
*  Parameters:
*    LunNotification: File system notification function
*    Context:         Context of LunNotification
*/
int USBH_MSD_Init(USBH_MSD_LUN_NOTIFICATION_FUNC * pfLunNotification, void * pContext) {
  USBH_PNP_NOTIFICATION PnPNotify;
  const USBH_INTERFACE_MASK PnPNotifyMask =  {
    USBH_INFO_MASK_CLASS | USBH_INFO_MASK_PROTOCOL, // Mask;
    0,                                              // VID;
    0,                                              // PID;
    0,                                              // bcdDevice;
    0,                                              // Interface;
    MASS_STORAGE_CLASS,                             // Class;
    0,                                              // Subclass
    PROTOCOL_BULK_ONLY};                            // Protocol

  //
  // Clear device, unit and driver object
  //
  ZERO_STRUCT(USBH_MSD_Global);
  USBH_MSD_Global.pfLunNotification    = pfLunNotification;
  USBH_MSD_Global.pContext             = pContext;
  USBH_MSD_Global.MaxTransferSize      = USBH_MSD_MAX_TRANSFER_SIZE;
  //
  // Add an plug an play notification routine
  //
  PnPNotify.Context             = NULL;
  PnPNotify.InterfaceMask       = PnPNotifyMask;
  PnPNotify.PnpNotification     = _OnDeviceNotify;
  USBH_MSD_Global.hPnPNotify    = USBH_RegisterPnPNotification(&PnPNotify);
  if (NULL == USBH_MSD_Global.hPnPNotify) {
    USBH_WARN((USBH_MTYPE_MSD, "MSD: USBH_MSD_Init: DlRegisterPnPNotification"));
    return FALSE;
  }
  return TRUE; // On success
}

/*********************************************************************
*
*       USBH_MSD_Exit
*
*  Function description
*    Releases all resources, closes all handles to the USB bus
*    driver and unregister all notification functions. Has to be called
*    if the application is closed before the USB bus driver is closed.
*/
void USBH_MSD_Exit(void) {
  int          i;
  USBH_MSD_DEVICE * pDev;
  // 1. Unregister all PnP notifications of the device driver.
  // 2. Release all USBH MSD device  resources and delete the device.
  if (USBH_MSD_Global.hPnPNotify != NULL) {
    USBH_UnregisterPnPNotification(USBH_MSD_Global.hPnPNotify);
    USBH_MSD_Global.hPnPNotify = NULL;
  }
  pDev = &USBH_MSD_Global.aDevice[0];
  for (i = 0; i < USBH_MSD_MAX_DEVICES; i++, pDev++) { // First mark not removed devices as removed
    if (pDev->IsValid) {
      _MarkDeviceAsRemoved(pDev->InterfaceID);
    }
  }
  for (i = 0; i < USBH_MSD_MAX_DEVICES; i++, pDev++) { // Delete also devices where the reference counter is greater than 0. Then a warning is printed.
    if (pDev->IsValid) {
      _DeleteDevice(pDev);
    }
  }
}

/*********************************************************************
*
*       USBH_MSD_GetLuns
*
*  Function description
*    Returns an array of logical unit numbers.
*/
#if 0
USBH_STATUS USBH_MSD_GetLuns(USBH_MSD_DEVICE_HANDLE DevHandle, int * Luns) {
  USBH_MSD_DEVICE * pDev;
  USBH_LOG((USBH_MTYPE_MSD, "MSD: USBH_MSD_GetLuns"));
  T_ASSERT_PTR(Luns);
  * Luns = 0;
  pDev = (USBH_MSD_DEVICE *)DevHandle;
  T_ASSERT_MAGIC(pDev, USBH_MSD_DEVICE);
  if (pDev->removed) {
    USBH_WARN((USBH_MTYPE_MSD, "MSD: USBH_MSD_GetLuns: device removed!"));
    return USBH_STATUS_DEVICE_REMOVED;
  }
  * Luns = pDev->UnitCt;
  return USBH_STATUS_SUCCESS;
}
#endif

/*********************************************************************
*
*       USBH_MSD_ReadSectors
*
*  Function description
*    Reads sectors from a USB MSD
*/
int USBH_MSD_ReadSectors(U8 Unit, U32 SectorAddress, U32 NumSectors, U8 * Buffer) {
  USBH_MSD_UNIT  * pUnit;
  USBH_MSD_DEVICE * pDev;
  int          status;

  USBH_LOG((USBH_MTYPE_MSD, "MSD: USBH_MSD_ReadSectors: address: %lu, sectors: %lu",SectorAddress,NumSectors));
  pUnit = &USBH_MSD_Global.aUnit[Unit];
  T_ASSERT_MAGIC(pUnit, USBH_MSD_UNIT);
  pDev = pUnit->pDev;
  T_ASSERT_MAGIC(pDev, USBH_MSD_DEVICE);
  T_ASSERT(NumSectors);
  if (pDev->Removed) {
    USBH_WARN((USBH_MTYPE_MSD, "MSD: USBH_MSD_ReadSectors: device removed!"));
    return USBH_STATUS_DEVICE_REMOVED;
  }
  T_ASSERT_PTR(Buffer);
  _IncRefCnt(pDev);
  USBH_MSD_INC_SYNC(pDev);
  status = pDev->pPhysLayerAPI->PlReadSectors(pUnit, SectorAddress, Buffer, (U16)NumSectors); // Read from the device with the correct protocol layer
#if (USBH_DEBUG > 1)
  if (status) {
    USBH_WARN((USBH_MTYPE_MSD, "MSD: USBH_MSD_ReadSectors: st: 0x%08x",status));
    USBH_PRINT_STATUS_VALUE(USBH_MTYPE_MSD, status);
  }
#endif
  _DecRefCnt (pDev);
  USBH_MSD_DEC_SYNC(pDev);
  return status;
}

/*********************************************************************
*
*       USBH_MSD_WriteSectors
*
*  Function description
*    Writes sectors to a USB MSD
*/
int USBH_MSD_WriteSectors(U8 Unit, U32 SectorAddress, U32 NumSectors, const U8 * Buffer) {
  USBH_MSD_UNIT  * pUnit;
  USBH_MSD_DEVICE * pDev;
  int          status;

  USBH_LOG((USBH_MTYPE_MSD, "MSD: USBH_MSD_WriteSectors: address: %lu, sectors: %lu",SectorAddress,NumSectors));
  pUnit = &USBH_MSD_Global.aUnit[Unit];
  T_ASSERT_MAGIC(pUnit, USBH_MSD_UNIT);
  pDev = pUnit->pDev;
  T_ASSERT_MAGIC(pDev, USBH_MSD_DEVICE);
  T_ASSERT(NumSectors);
  if (pDev->Removed) {
    USBH_WARN((USBH_MTYPE_MSD, "MSD: USBH_MSD_WriteSectors: device removed!"));
    return USBH_STATUS_DEVICE_REMOVED;
  }
  T_ASSERT_PTR(Buffer);
  _IncRefCnt(pDev);
  USBH_MSD_INC_SYNC(pDev);
  status = pDev->pPhysLayerAPI->PlWriteSectors(pUnit, SectorAddress, Buffer, (U16)NumSectors); // Write to the device with the right protocol layer
  if (status) {
    USBH_WARN((USBH_MTYPE_MSD, "MSD: USBH_MSD_WriteSectors: st: 0x%08x:",status));
    USBH_PRINT_STATUS_VALUE(USBH_MTYPE_MSD, status);
  }
  _DecRefCnt(pDev);
  USBH_MSD_DEC_SYNC(pDev);
  return status;
}

/*********************************************************************
*
*       USBH_MSD_GetStatus
*
*  Function description
*    Checks the status of a device. Therefore it calls USBH_MSD_GetUnit to
*    test if the device is still connected and if a logical unit is assigned.
*
*  Return value:
*    on success: USBH_STATUS_SUCCESS device is ready for operations
*    other values are errors
*/
USBH_STATUS USBH_MSD_GetStatus(U8 Unit) {
  USBH_MSD_UNIT  * pUnit;
  int status = USBH_STATUS_SUCCESS;

  pUnit = &USBH_MSD_Global.aUnit[Unit];
  T_ASSERT_MAGIC(pUnit, USBH_MSD_UNIT);
  T_ASSERT_MAGIC(pUnit->pDev, USBH_MSD_DEVICE);
  if (pUnit->pDev->Removed) {
    USBH_WARN((USBH_MTYPE_MSD, "MSD: USBH_MSD_GetStatus: device removed!"));
    return USBH_STATUS_DEVICE_REMOVED;
  }
  return status;
}

/*********************************************************************
*
*       USBH_MSD_GetUnitInfo
*
*  Function description
*    Returns basic information about the LUN
*/
USBH_STATUS USBH_MSD_GetUnitInfo(U8 Unit, USBH_MSD_UNIT_INFO * Info) {
  USBH_MSD_UNIT * pUnit;

  T_ASSERT_PTR(Info);
  pUnit = &USBH_MSD_Global.aUnit[Unit];
  T_ASSERT_MAGIC(pUnit, USBH_MSD_UNIT);
  T_ASSERT_MAGIC(pUnit->pDev, USBH_MSD_DEVICE);
  if (pUnit->pDev->Removed) {
    USBH_WARN((USBH_MTYPE_MSD, "MSD: USBH_MSD_GetUnitInfo: device removed!"));
    return USBH_STATUS_DEVICE_REMOVED;
  }
  Info->WriteProtectFlag = (0 != (pUnit->ModeParamHeader.DeviceParameter &MODE_WRITE_PROTECT_MASK)) ? TRUE : FALSE;
  Info->BytesPerSector   = pUnit->BytesPerSector;
  Info->TotalSectors     = pUnit->MaxSectorAddress + 1;
  return USBH_STATUS_SUCCESS;
}

/*********************************************************************
*
*       USBH_MSD_ConvStandardSense
*
*  Function description
*    USBH_MSD_ConvStandardSense fills out STANDARD_SENSE_DATA with the
*    received SC_REQUEST_SENSE command data. This function is independent
*    from the byte order of the processor.
*
*  Parameters:
*    buffer:   Pointer to the sense data buffer
*    length:   Length of bytes in buffer
*    SensePtr: IN: valid pointer; OUT: sense data
*/
int USBH_MSD_ConvStandardSense(const U8 * buffer, U16 length, STANDARD_SENSE_DATA * SensePtr) {
  T_ASSERT_PTR(buffer);
  T_ASSERT_PTR(SensePtr);
  if (length < STANDARD_SENSE_LENGTH) {
    return USBH_STATUS_ERROR;
  }
  SensePtr->ResponseCode  = buffer[0];
  SensePtr->Obsolete      = buffer[1];                  //1
  SensePtr->Sensekey      = buffer[2];                  //2
  SensePtr->Info          = USBH_LoadU32BE(&buffer[3]); //3,4,5,6
  SensePtr->AddLength     = buffer[7];                  //7
  SensePtr->Cmdspecific   = USBH_LoadU32BE(&buffer[8]); //8,9,10,11
  SensePtr->Sensecode     = buffer[12];                 //12
  SensePtr->Sensequalifier= buffer[13];                 //13
  SensePtr->Unitcode      = buffer[14];                 //14
  SensePtr->Keyspecific1  = buffer[15];                 //15
  SensePtr->Keyspecific2  = buffer[16];                 //16
  SensePtr->Keyspecific3  = buffer[17];                 //17
  USBH_LOG((USBH_MTYPE_MSD, "MSD: USBH_MSD_ConvStandardSense code: 0x%x, sense key: 0x%x, ASC: 0x%x, ASCQ: 0x%x ", SensePtr->ResponseCode, SensePtr->Sensekey & 0xf, SensePtr->Sensecode, SensePtr->Sensequalifier));
  return USBH_STATUS_SUCCESS;
}

/*********************************************************************
*
*       USBH_MSD_ConvReadCapacity
*
*  Function description:
*    USBH_MSD_ConvReadCapacity returns values taken from the received
*    SC_READ_CAPACITY command data block. This function is independent
*    from the byte order of the processor.
*
*  Parameters:
*    data:            Received SC_READ_CAPACITY data block
*    length           Length of data block
*    MaxBlockAddress: IN: valid pointer to a U32; OUT: the last possible block address
*    Blocklength:     IN: valid pointer to a U32; OUT: the number of bytes per sector
*/
int USBH_MSD_ConvReadCapacity(const U8 * data, U16 length, U32 * MaxBlockAddress, U32 * BlockLength) {
  T_ASSERT_PTR(data);
  T_ASSERT_PTR(MaxBlockAddress);
  T_ASSERT_PTR(BlockLength);
  if (length < RD_CAPACITY_DATA_LENGTH) {
    return USBH_STATUS_ERROR;
  }
  * MaxBlockAddress = USBH_LoadU32BE(data); // Last possible block address
  * BlockLength = USBH_LoadU32BE(data + 4); // Number of bytes per sector
  return USBH_STATUS_SUCCESS;
}

// Little endian
U32 Signature;          // Contains 'USBC'
U32 Tag;                // Unique per command id
U32 DataTransferLength; // Size of the data
U8  Flags;              // Direction in bit 7
U8  Lun;                // LUN (normally 0)
U8  Length;             // pLength  of the CDB, <= MAX_COMMAND_SIZE
U8  CDB[16];            // Command data block

/*********************************************************************
*
*       USBH_MSD_ConvModeParameterHeader
*
*  Function description:
*    Converts received sense mode data to a structure of type MODE_PARAMETER_HEADER.
*
*  Parameter:
*    ModeSense6: True if mode sense(6) command data is used, else mode sense(10) is used
*/
void USBH_MSD_ConvModeParameterHeader(MODE_PARAMETER_HEADER * ModeHeader, const U8 * Buffer, T_BOOL ModeSense6) {
  T_ASSERT_PTR(ModeHeader);
  T_ASSERT_PTR(Buffer);
  if (ModeSense6) { // Mode sense(6)
    ModeHeader->DataLength            = Buffer[MODE_PARAMETER_HEADER_DATA_LENGTH_OFS];                            // One byte
    ModeHeader->MediumType            = Buffer[MODE_PARAMETER_HEADER_MEDIUM_TYPE_OFS_6];
    ModeHeader->DeviceParameter       = Buffer[MODE_PARAMETER_HEADER_DEVICE_PARAM_OFS_6];
    ModeHeader->BlockDescriptorLength = Buffer[MODE_PARAMETER_HEADER_BLOCK_DESC_LENGTH_OFS_6];
    ModeHeader->DataOffset            = MODE_PARAMETER_HEADER_BLOCK_DESC_LENGTH_OFS_6 + 1;
  } else {          // Mode sense(10)
    ModeHeader->DataLength            = USBH_LoadU16BE (Buffer);                                                  // Data pLength
    ModeHeader->MediumType            = Buffer[MODE_PARAMETER_HEADER_MEDIUM_TYPE_OFS_10];
    ModeHeader->DeviceParameter       = Buffer[MODE_PARAMETER_HEADER_DEVICE_PARAM_OFS_10];
    ModeHeader->BlockDescriptorLength = USBH_LoadU16BE(&Buffer[MODE_PARAMETER_HEADER_BLOCK_DESC_LENGTH_OFS_10]);  // Data pLength
    ModeHeader->DataOffset            = MODE_PARAMETER_HEADER_BLOCK_DESC_LENGTH_OFS_6 + 2;                        // Because the pLength is a 16 bit value
  }
}


/******************************* EOF ********************************/
