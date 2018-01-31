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
#include "USBH_MSD_Int.h"
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
  #define USBH_MSD_DEC_URBCT(pDevice)                                                        \
      (pDevice)->UrbRefCt--;                                                            \
      if ((pDevice)->UrbRefCt  != 0) {                                                  \
        USBH_WARN((USBH_MTYPE_MSD_INTERN, "MSD: Invalid UrbRefCt:%d",  (pDevice)->UrbRefCt));  \
        USBH_ASSERT0;                                                                     \
      }
  #define USBH_MSD_INC_URBCT(pDevice)                                                        \
      (pDevice)->UrbRefCt++;                                                            \
      if ((pDevice)->UrbRefCt  != 1) {                                                  \
        USBH_WARN((USBH_MTYPE_MSD_INTERN, "MSD: Invalid UrbRefCt:%d",  (pDevice)->UrbRefCt));  \
        USBH_ASSERT0;                                                                     \
      }
  #define USBH_MSD_DEC_SYNC(pDevice)                                                         \
      (pDevice)->SyncRefCt--;                                                           \
      if ((pDevice)->SyncRefCt != 0) {                                                  \
        USBH_WARN((USBH_MTYPE_MSD_INTERN, "MSD: Invalid SyncRefCt:%d", (pDevice)->SyncRefCt)); \
        USBH_ASSERT0;                                                                     \
      }
  #define USBH_MSD_INC_SYNC(pDevice)                                                         \
      (pDevice)->SyncRefCt++;                                                           \
      if ((pDevice)->SyncRefCt != 1) {                                                  \
        USBH_WARN((USBH_MTYPE_MSD_INTERN, "MSD: Invalid SyncRefCt:%d", (pDevice)->SyncRefCt)); \
        USBH_ASSERT0;                                                                     \
      }
#else
  #define USBH_MSD_DEC_URBCT(pDevice)
  #define USBH_MSD_INC_URBCT(pDevice)
  #define USBH_MSD_DEC_SYNC( pDevice)
  #define USBH_MSD_INC_SYNC( pDevice)
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
static void _ConvEndpointDesc(const U8 * Buffer, USB_ENDPOINT_DESCRIPTOR * pEpDesc) {
  pEpDesc->bLength          = Buffer[0];                  // Index 0 bLength
  pEpDesc->bDescriptorType  = Buffer[1];                  // Index 1 bDescriptorType
  pEpDesc->bEndpointAddress = Buffer[2];                  // Index 2 bEndpointAddress
  pEpDesc->bmAttributes     = Buffer[3];                  // Index 3 bmAttributes
  pEpDesc->wMaxPacketSize   = USBH_LoadU16LE(&Buffer[4]);
  pEpDesc->bInterval        = Buffer[6];                  // Index 6 bInterval
}

/*********************************************************************
*
*       _IsCSWValidandMeaningful
*
*  Function description
*    Checks if the command Status block is valid and meaningful
*/
static USBH_BOOL _IsCSWValidAndMeaningful(USBH_MSD_DEVICE * pDev, const COMMAND_BLOCK_WRAPPER * cbw, const COMMAND_STATUS_WRAPPER * csw, const U32 CSWlength) {
  if (CSWlength < CSW_LENGTH) {
    USBH_WARN((USBH_MTYPE_MSD_INTERN, "MSD: IsCSWValid: invalid CSW length: %lu",CSWlength));
    return 0;                      // False
  }
  if (csw->Signature != CSW_SIGNATURE) {

#if (USBH_DEBUG > 1)
    if (CSWlength == CSW_LENGTH) { // Prevents debug messages if test a regular data block
      USBH_WARN((USBH_MTYPE_MSD_INTERN, "MSD: IsCSWValid: invalid CSW signature: 0x%08X",csw->Signature));
    }
#endif

    return FALSE;
  }
  if (csw->Tag != pDev->BlockWrapperTag) {

#if (USBH_DEBUG > 1)
    if (CSWlength == CSW_LENGTH) { // Prevent debug messages if test a regular data block
      USBH_WARN((USBH_MTYPE_MSD_INTERN, "MSD: IsCSWValid: invalid Tag sent:0x%08x rcv:0x%08x", cbw->Tag,csw->Tag));
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
  USBH_ASSERT_MAGIC(pDev, USBH_MSD_DEVICE);
  USBH_ASSERT_PTR  (cbwBuffer);
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
  USBH_ASSERT_PTR(buffer);
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
static void _OnSubmitUrbCompletion(USBH_URB * pUrb) {
  USBH_MSD_DEVICE * pDev;
  pDev       = (USBH_MSD_DEVICE *)pUrb->Header.pContext;
  USBH_ASSERT_MAGIC(pDev, USBH_MSD_DEVICE);
  USBH_LOG((USBH_MTYPE_MSD_INTERN, "MSD: _OnSubmitUrbCompletion URB st: 0x%08x",pUrb->Header.Status));
  USBH_OS_SetEvent(pDev->pUrbEvent);
}

/*********************************************************************
*
*       _SubmitUrbAndWait
*
*  Function description
*    Submits an URB to the USB bus driver synchronous, it uses the
*    TAL event functions. On successful completion the URB Status is returned!
*/
static USBH_STATUS _SubmitUrbAndWait(USBH_MSD_DEVICE * pDev, USBH_URB * pUrb, U32 timeout) {
  USBH_STATUS Status;
  int         EventStatus;

  USBH_ASSERT(NULL != pDev->hInterface);
  USBH_ASSERT_PTR(pDev->pUrbEvent);
  USBH_LOG((USBH_MTYPE_MSD_INTERN, "MSD: _SubmitUrbAndWait"));
  USBH_MSD_INC_URBCT(pDev);
  pUrb->Header.pfOnCompletion = _OnSubmitUrbCompletion;
  pUrb->Header.pContext    = pDev;
  USBH_OS_ResetEvent(pDev->pUrbEvent);
  Status = USBH_SubmitUrb(pDev->hInterface, pUrb);
  if (Status != USBH_STATUS_PENDING) {
    USBH_LOG((USBH_MTYPE_MSD_INTERN, "MSD: _SubmitUrbAndWait: USBH_SubmitUrb st: 0x%08x",Status));
    Status = USBH_STATUS_ERROR;
  } else {                                // Pending URB
    Status       = USBH_STATUS_SUCCESS;
    EventStatus = USBH_OS_WaitEventTimed(pDev->pUrbEvent, timeout);
    if (EventStatus != USBH_OS_EVENT_SIGNALED) {
      USBH_BOOL abort    = TRUE;
      USBH_URB * abort_urb = &pDev->AbortUrb;
      USBH_LOG((USBH_MTYPE_MSD, "MSD: _SubmitUrbAndWait: timeout Status: 0x%08x, now abort the URB!",EventStatus));
      USBH_ZERO_MEMORY(abort_urb, sizeof(USBH_URB));
      switch (pUrb->Header.Function) {     // Not signaled abort and wait infinite
      case USBH_FUNCTION_BULK_REQUEST:
      case USBH_FUNCTION_INT_REQUEST:
        abort_urb->Request.EndpointRequest.Endpoint = pUrb->Request.BulkIntRequest.Endpoint;
        break;
      case USBH_FUNCTION_CONTROL_REQUEST:
        abort_urb->Request.EndpointRequest.Endpoint = 0;
        break;
      default:
        abort = FALSE;
        USBH_LOG((USBH_MTYPE_MSD_INTERN, "MSD: _SubmitUrbAndWait: invalid URB function: %d",pUrb->Header.Function));
        break;
      }
      if (abort) {
        USBH_WARN((USBH_MTYPE_MSD_INTERN, "MSD: _SubmitUrbAndWait: Abort Ep: 0x%x", abort_urb->Request.EndpointRequest.Endpoint));
        abort_urb->Header.Function = USBH_FUNCTION_ABORT_ENDPOINT;
        USBH_OS_ResetEvent(pDev->pUrbEvent);
        abort_urb->Header.pfOnCompletion = _OnSubmitUrbCompletion;
        abort_urb->Header.pContext    = pDev;
        Status = USBH_SubmitUrb(pDev->hInterface, abort_urb);
        if (Status) {
          USBH_LOG((USBH_MTYPE_MSD_INTERN, "MSD: _SubmitUrbAndWait: USBH_FUNCTION_ABORT_ENDPOINT st: 0x%08x",Status));
        }
        USBH_OS_WaitEvent(pDev->pUrbEvent);
      }
    }
    if (!Status) {
      Status = pUrb->Header.Status;       // URB completed return the pBuffer Status
      if (Status) {
        USBH_LOG((USBH_MTYPE_MSD_INTERN, "MSD: _SubmitUrbAndWait: URB Status: %s", USBH_GetStatusStr(Status)));
      }
    }
  }
  USBH_MSD_DEC_URBCT(pDev);
  return Status;
}

/*********************************************************************
*
*       _UsbDeviceReset
*
*  Function description
*/
static USBH_STATUS _UsbDeviceReset(USBH_MSD_DEVICE * pDev) {
  USBH_STATUS   status;
  USBH_URB    * urb;
  USBH_ASSERT_MAGIC(pDev, USBH_MSD_DEVICE);
  USBH_ASSERT_PTR  (pDev->hInterface);
  urb                    = &pDev->ControlUrb;
  urb->Header.Function   = USBH_FUNCTION_RESET_DEVICE;
  urb->Header.pfOnCompletion = NULL;
  status = USBH_SubmitUrb(pDev->hInterface, urb);      // No need to call _SubmitUrbAndWait because USBH_FUNCTION_RESET_DEVICE does never return with USBH_STATUS_PENDING

  if (status) { // Reset pipe does not wait
    USBH_WARN((USBH_MTYPE_MSD_INTERN, "MSD: _UsbDeviceReset: USBH_SubmitUrb st: 0x%08x",status));
    status = USBH_STATUS_ERROR;
  }
  return status;
}


/*********************************************************************
*
*       _ResetPipe
*
*  Function description
*/
static USBH_STATUS _ResetPipe(USBH_MSD_DEVICE * pDev, U8 EndPoint) {
  USBH_STATUS   status;
  USBH_URB    * urb;
  USBH_ASSERT_MAGIC(pDev, USBH_MSD_DEVICE);
  USBH_ASSERT_PTR  (pDev->hInterface);
  urb                                   = &pDev->ControlUrb;
  urb->Header.Function                  = USBH_FUNCTION_RESET_ENDPOINT;
  urb->Header.pfOnCompletion            = NULL;
  urb->Request.EndpointRequest.Endpoint = EndPoint;
  status                                = _SubmitUrbAndWait(pDev, urb, USBH_MSD_EP0_TIMEOUT); // On error this URB is not aborted
  if (status) { // Reset pipe does not wait
    USBH_WARN((USBH_MTYPE_MSD_INTERN, "MSD: _ResetPipe: USBH_SubmitUrb st: 0x%08x",status));
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
static USBH_STATUS _SetupRequest(USBH_MSD_DEVICE * pDev, USBH_URB * pUrb, U8 * pBuffer, U32 * pLength, U32 Timeout) {
  USBH_STATUS                             status;
  * pLength                             = 0;      // Clear returned pLength
  pUrb->Header.Function                 = USBH_FUNCTION_CONTROL_REQUEST;
  pUrb->Request.ControlRequest.Endpoint = 0;
  pUrb->Request.ControlRequest.pBuffer   = pBuffer;
  status                                = _SubmitUrbAndWait(pDev, pUrb, Timeout);
  if (status) {
    USBH_WARN((USBH_MTYPE_MSD_INTERN, "MSD: USBH_MSD_VendorRequest: st: 0x%08x", status));
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
static USBH_STATUS _ReadSync(USBH_MSD_DEVICE * pDev, U8 * pBuffer, U32 * pLength, U16 Timeout, USBH_BOOL DataPhaseFlag, USBH_BOOL SectorDataFlag) {
  U32           remainingLength, rdLength;
  U8          * buffer;
  USBH_STATUS   Status = USBH_STATUS_SUCCESS;
  USBH_URB    * pUrb;
  USBH_ASSERT_MAGIC(pDev, USBH_MSD_DEVICE);
  USBH_ASSERT_PTR  (pBuffer);
  USBH_ASSERT_PTR  (pLength);
  // Unused param, if needed for later use
  (void)DataPhaseFlag;
  (void)SectorDataFlag;
  USBH_LOG((USBH_MTYPE_MSD_INTERN, "MSD: _ReadSync Ep: %u,length: %4lu",(int)pDev->BulkInEp,*pLength));
  if (pDev->Removed) {
    USBH_WARN((USBH_MTYPE_MSD_INTERN, "MSD: _ReadSync: Device removed!"));
    return USBH_STATUS_DEVICE_REMOVED;
  }
  buffer                               =   pBuffer;
  remainingLength                      = * pLength;
  * pLength                            =   0;
  pUrb                                  =   &pDev->Urb;
  pUrb->Header.Function                 =   USBH_FUNCTION_BULK_REQUEST;
  pUrb->Request.BulkIntRequest.Endpoint =   pDev->BulkInEp;
  while (remainingLength) {                         // Remaining buffer
    rdLength                           = USBH_MIN(remainingLength, USBH_MSD_Global.MaxTransferSize);
    pUrb->Request.BulkIntRequest.pBuffer = buffer;
    pUrb->Request.BulkIntRequest.Length = rdLength;
    USBH_LOG((USBH_MTYPE_MSD_INTERN, "MSD: _ReadSync: DlReadSync bytes to read: %4lu",rdLength));
    Status                             = _SubmitUrbAndWait(pDev, pUrb, Timeout);
    rdLength                           = pUrb->Request.BulkIntRequest.Length;
    if (Status) {                                   // On error stops and discard data
      USBH_LOG((USBH_MTYPE_MSD, "MSD: _ReadSync: _SubmitUrbAndWait: length: %lu Status: %s", rdLength, USBH_GetStatusStr(Status)));
      break;
    } else {                                        // On success
        remainingLength -= rdLength;
      * pLength         += rdLength;
      if ((rdLength % pDev->BulkMaxPktSize) != 0) { // A short packet was received
        USBH_LOG((USBH_MTYPE_MSD_INTERN, "MSD: INFO _ReadSync: short packet with length %lu received!", rdLength));
        break;
      }
      buffer            += rdLength;                // Adjust destination
    }
  }
  USBH_LOG((USBH_MTYPE_MSD_INTERN, "MSD: _ReadSync: returned length: %lu ",*pLength));
  return Status;
}

/*********************************************************************
*
*       _WriteSync
*
*  Function description
*    _WriteSync writes all bytes to device via Bulk OUT transfers.
*    Transactions are performed in chunks of no more than USBH_MSD_Global.MaxTransferSize.
*/
static int _WriteSync(USBH_MSD_DEVICE * pDev, const U8 * Buffer, U32 * Length, U16 Timeout, USBH_BOOL DataPhaseFlag, USBH_BOOL SectorDataFlag) {
  U32        remainingLength, wrLength, oldLength;
  const U8 * buffer;
  int        Status;
  USBH_URB * urb;
  // Unused param, if needed for later use
  (void)DataPhaseFlag;
  (void)SectorDataFlag;
  Status = USBH_STATUS_SUCCESS;
  USBH_ASSERT_MAGIC(pDev, USBH_MSD_DEVICE);
  USBH_ASSERT_PTR  (Buffer);
  USBH_ASSERT_PTR  (Length);
  USBH_LOG((USBH_MTYPE_MSD_INTERN, "MSD: _WriteSync Ep: %4u,length: %4lu",pDev->BulkOutEp,*Length));
  if (pDev->Removed) {
    USBH_WARN((USBH_MTYPE_MSD_INTERN, "MSD: _ReadSync: Device removed!"));
    return USBH_STATUS_INVALID_PARAM;
  }
  buffer                               = Buffer;
  remainingLength                      = * Length;
  urb                                  = &pDev->Urb;
  urb->Header.Function                 = USBH_FUNCTION_BULK_REQUEST;
  urb->Request.BulkIntRequest.Endpoint = pDev->BulkOutEp;
  do {
    oldLength = wrLength               = USBH_MIN(remainingLength, USBH_MSD_Global.MaxTransferSize);
    urb->Request.BulkIntRequest.pBuffer = (void *)buffer;
    USBH_LOG((USBH_MTYPE_MSD_INTERN, "MSD: consider: DlWriteSync bytes to write: %4lu",wrLength));
    urb->Request.BulkIntRequest.Length = wrLength;
    Status                             = _SubmitUrbAndWait(pDev, urb, Timeout);
    if (Status) {
      USBH_WARN((USBH_MTYPE_MSD_INTERN, "MSD: _WriteSync: _SubmitUrbAndWait: st: 0x%08x",Status));
      break;
    }
    USBH_LOG((USBH_MTYPE_MSD_INTERN, "MSD: _WriteSync: %4lu written",wrLength));
    if (wrLength != oldLength) {
      USBH_WARN((USBH_MTYPE_MSD_INTERN, "MSD: DlWriteSync: Not all bytes written"));
      break;
    }
    remainingLength -= wrLength;
    buffer          += wrLength;        // Adjust source
  } while (remainingLength);
  * Length -= remainingLength;          // Does not consider the last write
  USBH_LOG((USBH_MTYPE_MSD_INTERN, "MSD: _WriteSync returned length: %4lu",*Length));
  return Status;
}

/*********************************************************************
*
*       _ReadCSW
*
*  Function description:
*    Reads the command Status block from the device and
*    checks if the Status block is valid and meaningful.
*    If the USB device stalls on the IN pipe
*    the endpoint is reset and the CSW is read again.
*
*  Return value:
*    USBH_STATUS_SUCESS          on success
*    USBH_STATUS_COMMAND_FAILED  the command failed, check the sense data
*    USBH_STATUS_ERROR           no command Status block received or Status block with a phase error
*/
static int _ReadCSW(USBH_MSD_DEVICE * pDev, COMMAND_BLOCK_WRAPPER * Cbw, COMMAND_STATUS_WRAPPER * Csw) {
  int   Status, i;
  U32   length;
  U8  * buffer;
  USBH_ASSERT_MAGIC(pDev, USBH_MSD_DEVICE);
  USBH_ASSERT_PTR  (Cbw);
  USBH_ASSERT_PTR  (Csw);

  if (pDev->Removed) {
    USBH_WARN((USBH_MTYPE_MSD_INTERN, "MSD: _ReadCSW: Device removed!"));
    return USBH_STATUS_INVALID_PARAM;
  }
  Status = USBH_STATUS_ERROR;
  buffer = pDev->pCswPhyTransferBuffer;
  i      = 2;
  length = 0;         // If the first Status block read fails (no timeout error) then read a second time

  while (i) {
    length = pDev->BulkMaxPktSize;
    Status = _ReadSync(pDev, buffer, &length, USBH_MSD_READ_TIMEOUT, 0, 0);
    if (!Status) {    // Success
      break;
    } else {          // Error
      USBH_WARN((USBH_MTYPE_MSD_INTERN, "MSD: _ReadCSW: _ReadSync: %s!", USBH_GetStatusStr(Status)));
      if (Status == USBH_STATUS_TIMEOUT) { // Timeout
        break;
      } else {        // On all other errors reset the pipe an try it again to read CSW
        Status = _ResetPipe(pDev, pDev->BulkInEp);
        if (Status) { // Reset error, break
          USBH_WARN((USBH_MTYPE_MSD_INTERN, "MSD: _ReadCSW: _ResetPipe: %s", USBH_GetStatusStr(Status)));
          break;
        }
      } // Try to read again the CSW
    }
    i--;
  }
  if (!Status) {                                                // On success
    if (length == CSW_LENGTH) {
      if (!_ConvBufferToStatusWrapper(buffer, length, Csw)) {   // Check CSW
        if (!_IsCSWValidAndMeaningful(pDev, Cbw, Csw, length)) {
          USBH_WARN((USBH_MTYPE_MSD_INTERN, "MSD: _ReadCSW: IsCSWValidandMeaningful: %s", USBH_GetStatusStr(Status)));
          Status = USBH_STATUS_ERROR;
        }
      } else {
        USBH_WARN((USBH_MTYPE_MSD_INTERN, "MSD: _ReadCSW: _ConvBufferToStatusWrapper %s", USBH_GetStatusStr(Status)));
      }
    } else {
      USBH_WARN((USBH_MTYPE_MSD_INTERN, "MSD: _ReadCSW: invalid length: %lu",length));
      Status = USBH_STATUS_ERROR;
    }
  }
  return Status;
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
static USBH_STATUS _PerformResetRecovery(USBH_MSD_DEVICE * pDev) {
  USBH_STATUS Status;

  USBH_LOG((USBH_MTYPE_MSD, "MSD: USBH_MSD_MassStorageReset"));
  USBH_ASSERT_MAGIC(pDev, USBH_MSD_DEVICE);
  if (pDev->Removed) {
    USBH_WARN((USBH_MTYPE_MSD_INTERN, "MSD: USBH_MSD_MassStorageReset: Device removed!"));
    return USBH_STATUS_INVALID_PARAM;
  }
  //
  // Is commented out, instead of a MSD reset and Reset pipes, we simply reenumerate the device.
  // Is is also done by Windows and Linux, so we achieve 100% compatibility to those OSes.
  //
#if 0
  USBH_LOG((USBH_MTYPE_MSD_INTERN, "MSD: DlResetPipe Ep-address: %u",pDev->BulkInEp));  // 2. Reset Bulk IN
  Status = _ResetPipe(pDev, pDev->BulkInEp);
  if (status) {
    USBH_WARN((USBH_MTYPE_MSD_INTERN, "MSD: USBH_MSD_MassStorageReset: _ResetPipe!"));
  }
  USBH_LOG((USBH_MTYPE_MSD_INTERN, "MSD: DlResetPipe Ep-address: %u",pDev->BulkOutEp)); // 3. Reset Bulk OUT
  Status = _ResetPipe(pDev, pDev->BulkOutEp);
  if (Status) {
    USBH_WARN((USBH_MTYPE_MSD_INTERN, "MSD: USBH_MSD_MassStorageReset: _ResetPipe!"));
    return USBH_STATUS_SUCCESS;
  }
#endif
  Status = _UsbDeviceReset(pDev);
  if (Status) {
    USBH_WARN((USBH_MTYPE_MSD_INTERN, "MSD: USBH_MSD_MassStorageReset: _BULKONLY_MassStorageReset Status: %d",Status));
  }
  return Status;
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
  USBH_ASSERT_MAGIC(pDev, USBH_MSD_DEVICE);
  if (pDev->Removed) {
    USBH_WARN((USBH_MTYPE_MSD_INTERN, "MSD: _BULKONLY_ErrorRecovery: Device removed!!"));
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
    USBH_WARN((USBH_MTYPE_MSD_INTERN, "MSD: _BULKONLY_ErrorRecovery: _PerformResetRecovery: st: 0x%08x!",status));
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
  USBH_URB          * urb;
  USBH_LOG((USBH_MTYPE_MSD_INTERN, "MSD: USBH_MSD_BulkOnlyMassStorageReset"));
  USBH_ASSERT_MAGIC(pDev, USBH_MSD_DEVICE);
  if (pDev->Removed) {
    USBH_WARN((USBH_MTYPE_MSD_INTERN, "MSD: _BULKONLY_MassStorageReset: Device removed!"));
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
    USBH_WARN((USBH_MTYPE_MSD_INTERN, "MSD: _BULKONLY_MassStorageReset st: 0x%08x!",status));
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
  USBH_URB          * urb;
  unsigned            NumRetries = 0;

  USBH_LOG((USBH_MTYPE_MSD, "MSD: _BULKONLY_GetMaxLUN "));
  USBH_ASSERT_MAGIC(pDev, USBH_MSD_DEVICE);
  * maxLunIndex = 0; // default value
  if (pDev->Removed) {
    USBH_WARN((USBH_MTYPE_MSD, "MSD: GetMaxLUN: Device removed!"));
    return USBH_STATUS_DEVICE_REMOVED;
  }
Retry:
  urb            = &pDev->Urb;
  setup          = &urb->Request.ControlRequest.Setup;
  setup->Type    = USB_REQTYPE_CLASS | USB_INTERFACE_RECIPIENT | USB_IN_DIRECTION;
  setup->Request = BULK_ONLY_GETLUN_REQ;
  setup->Index   = (U16)pDev->bInterfaceNumber;
  setup->Value   = 0;
  setup->Length  = BULK_ONLY_GETLUN_LENGTH; // Length is one byte
  status         = _SetupRequest(pDev, urb, pDev->pEP0PhyTransferBuffer, &length, USBH_MSD_EP0_TIMEOUT);
  if (status == USBH_STATUS_SUCCESS) {
    if (length != BULK_ONLY_GETLUN_LENGTH) {
      USBH_WARN((USBH_MTYPE_MSD_INTERN, "MSD: GetMaxLUN: invalid length received: %d",length));
    } else {
      * maxLunIndex = * pDev->pEP0PhyTransferBuffer;
    }
  } else if (status == USBH_STATUS_STALL) {
    urb            = &pDev->Urb;
    setup          = &urb->Request.ControlRequest.Setup;
    setup->Type    = 0x02;
    setup->Request = 0x01;
    setup->Index   = 0;
    setup->Value   = 0;
    setup->Length  = 0; // Length is one byte
    _SetupRequest(pDev, urb, NULL, &length, USBH_MSD_EP0_TIMEOUT);
    if (NumRetries <  BULK_ONLY_MAX_RECOVERY) {
      NumRetries++;
      goto Retry;
    }
  } else {
    USBH_WARN((USBH_MTYPE_MSD_INTERN, "MSD: _BULKONLY_GetMaxLUN st: 0x%08x!",status));
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
  USBH_ASSERT_PTR(Buffer);
  USBH_ASSERT_PTR(cbw);
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
static USBH_STATUS _SendCommandWriteData(USBH_MSD_UNIT * pUnit, U8 * pCmdBuffer, U8 CmdLength, const U8 * pDataBuffer, U32 * pDataLength, U16 Timeout, USBH_BOOL SectorDataFlag) {
  COMMAND_BLOCK_WRAPPER    cbw; // Stores the request until completion
  COMMAND_STATUS_WRAPPER   csw;
  USBH_MSD_DEVICE             * pDev;
  USBH_STATUS              status;
  U8                     * cbwBuffer;
  USBH_BOOL                   recovery;
  U32                      length;
  U32                      dataLength;

  USBH_ASSERT(pUnit       != NULL);
  USBH_ASSERT(pCmdBuffer  != NULL);
  USBH_ASSERT(pDataLength != NULL);
  pDev = pUnit->pDev;              // Get the pointer to the device
  if (pDev == NULL) {
    USBH_WARN((USBH_MTYPE_MSD_INTERN, "MSD: _SendCommandWriteData: Device removed!"));
    return USBH_STATUS_INVALID_PARAM;
  }
  USBH_ASSERT_MAGIC(pDev, USBH_MSD_DEVICE);
  USBH_ASSERT(pUnit->pDev->pDriver != NULL);
  USBH_ASSERT_PTR(pDev->pCbwPhyTransferBuffer);
  cbwBuffer  = pDev->pCbwPhyTransferBuffer;
  if ((CmdLength == 0) || (CmdLength > COMMAND_WRAPPER_CDB_FIELD_LENGTH)) {
    USBH_WARN((USBH_MTYPE_MSD_INTERN, "MSD: _SendCommandWriteData: CmdLength: %u",CmdLength));
    return USBH_STATUS_LENGTH;
  }
  USBH_ZERO_MEMORY(cbwBuffer, CBW_LENGTH);
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
        USBH_WARN((USBH_MTYPE_MSD_INTERN, "MSD: _SendCommandWriteData: USBH_MSD_ErrorRecovery"));
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
      USBH_WARN((USBH_MTYPE_MSD_INTERN, "MSD: _SendCommandWriteData: Command Phase: Status = %s", USBH_GetStatusStr(status)));
      if (status == USBH_STATUS_STALL) {
        USBH_LOG((USBH_MTYPE_MSD_INTERN, "MSD: DlResetPipe Ep-address: %u",pDev->BulkOutEp));
        status = _ResetPipe(pDev, pDev->BulkOutEp);
        if (status) {      // Reset error
          USBH_WARN((USBH_MTYPE_MSD_INTERN, "MSD: _SendCommandWriteData: _ResetPipe!"));
        }
      }
      recovery = TRUE;
      continue;
    }
    //
    // DATA PHASE Bulk OUT
    //
    if (dataLength) {
      USBH_ASSERT(pDataBuffer != NULL);
      length = dataLength;
      status = _WriteSync(pDev, pDataBuffer, &length, Timeout, TRUE, SectorDataFlag);
      if (status) {          // Error
        USBH_WARN((USBH_MTYPE_MSD_INTERN, "MSD: _SendCommandWriteData: Data OUT Phase"));
        if (status == USBH_STATUS_STALL) {
          USBH_LOG((USBH_MTYPE_MSD_INTERN, "MSD: DlResetPipe Ep-address: %u",pDev->BulkOutEp));
          status = _ResetPipe(pDev, pDev->BulkOutEp);
          if (status) {      // Reset error
            USBH_WARN((USBH_MTYPE_MSD_INTERN, "MSD: _SendCommandWriteData: _ResetPipe!"));
          }
        } else {
          USBH_WARN((USBH_MTYPE_MSD_INTERN, "MSD: _SendCommandWriteData data: other error!"));
          recovery = TRUE;   // Start error recovery
          continue;
        }
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
            USBH_WARN((USBH_MTYPE_MSD_INTERN, "MSD: _SendCommandWriteData: invalid Residue!"));
          }
        }
        pDev->ErrorRecoveryCt = 0;
        break;
      }
    // On phase error repeat the command
    }
    recovery = TRUE;       // Repeat the same command with error Recovery
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
*    pDataLength:   IN: Length of pDataBuffer; OUT: transferred bytes
*
*  Return value
*    USBH_STATUS_LENGTH: false command Length or data Length
*    USBH_STATUS_COMMAND_FAILED: the device could not interpret the command.
*/
static USBH_STATUS _SendCommandReadData(USBH_MSD_UNIT * pUnit, U8 * pCmdBuffer, U8 CmdLength, U8 * pDataBuffer, U32 * pDataLength, U16 Timeout, USBH_BOOL SectorDataFlag) {
  COMMAND_BLOCK_WRAPPER    cbw; // stores the request until completion
  COMMAND_STATUS_WRAPPER   csw;
  USBH_BOOL                   Recovery;
  U32                      Length, DataLength;
  USBH_STATUS              Status;
  USBH_MSD_DEVICE        * pDev;
  U8                     * cbwBuffer = NULL;

  USBH_ASSERT(pUnit       != NULL);
  USBH_ASSERT(pCmdBuffer  != NULL);
  USBH_ASSERT(pDataBuffer != NULL);
  USBH_ASSERT(pDataLength != NULL);
  pDev                  = pUnit->pDev;
  if (pDev == NULL) {
    USBH_WARN((USBH_MTYPE_MSD, "MSD: _SendCommandReadData: Device removed!"));
    return USBH_STATUS_INVALID_PARAM;
  }
  USBH_ASSERT_MAGIC(pDev, USBH_MSD_DEVICE);
  USBH_ASSERT(pUnit->pDev->pDriver != NULL);
  cbwBuffer = pDev->pCbwPhyTransferBuffer;
  if ((CmdLength == 0) || (CmdLength > COMMAND_WRAPPER_CDB_FIELD_LENGTH)) {
    USBH_WARN((USBH_MTYPE_MSD_INTERN, "MSD: _SendCommandReadData: CmdLength: %u",CmdLength));
    return USBH_STATUS_INVALID_PARAM;
  }
  USBH_ZERO_MEMORY(cbwBuffer, CBW_LENGTH);
  _FillCBW(&cbw, 0, *pDataLength, CBW_FLAG_READ, pUnit->Lun, CmdLength);
  _ConvCommandBlockWrapper(&cbw, cbwBuffer); // Convert the struct cbw to a cbw pBuffer and attach pCmdBuffer
  USBH_MEMCPY(&cbwBuffer[COMMAND_WRAPPER_CDB_OFFSET], pCmdBuffer, CmdLength);
  DataLength   = *pDataLength;                    // Transfer the command
  * pDataLength = 0;                              // Clear the returned Length
  Recovery     = FALSE;
  Status       = USBH_STATUS_ERROR;
  for (; ;) {
    if (Recovery) {
      Status = _BULKONLY_ErrorRecovery(pDev);
      if (Status) {                              // Recover counter is invalid after all tries
        USBH_WARN((USBH_MTYPE_MSD_INTERN, "MSD: _SendCommandReadData: _BULKONLY_ErrorRecovery, stop"));
        break;
      }
    }
    //
    // COMMAND PHASE
    //
    Length = CBW_LENGTH;
    _WriteTag(pDev, cbwBuffer);
    Status = _WriteSync(pDev, cbwBuffer, &Length, USBH_MSD_WRITE_TIMEOUT, FALSE, SectorDataFlag);
    if (Status) {
      USBH_WARN((USBH_MTYPE_MSD_INTERN, "MSD: _SendCommandReadData: Command Phase"));
      Recovery = TRUE;
      continue;
    }
    //
    // DATA PHASE
    //
    if (DataLength) {                            // DataLength always contains the original Length
      Length = DataLength;                       // data IN transfer (Length != 0)
      Status = _ReadSync(pDev, pDataBuffer, &Length, Timeout, TRUE, SectorDataFlag);
      if (Status) {                              // Error
        USBH_LOG((USBH_MTYPE_MSD, "MSD: _SendCommandReadData: Data IN Phase failed"));
        if (Status == USBH_STATUS_STALL) {        // Reset the IN pipe
          USBH_LOG((USBH_MTYPE_MSD_INTERN, "MSD: DlResetPipe Ep-address: %u",pDev->BulkInEp));
          Status = _ResetPipe(pDev, pDev->BulkInEp);
          if (Status) {                          // Reset error
            USBH_WARN((USBH_MTYPE_MSD_INTERN, "MSD: _ReadCSW: reset error!"));
          }
        } else {
          USBH_WARN((USBH_MTYPE_MSD_INTERN, "MSD: _SendCommandReadData data: other error!"));
          Recovery = TRUE;                       // Start error Recovery
          continue;
        }
      } else {
        if ((Length % pDev->BulkMaxPktSize) == CSW_LENGTH) {              // Last data packet Length is CSW_LENGTH, check command Status
          if (!_ConvBufferToStatusWrapper(pDataBuffer + Length - CSW_LENGTH, Length, &csw)) {
            if (_IsCSWValidAndMeaningful(pDev, &cbw, &csw, CSW_LENGTH)) { // device has stopped the data transfer by sending an CSW
              // This occurs if the toggle bit is not reset after USB clear feature endpoint halt!
              USBH_WARN((USBH_MTYPE_MSD_INTERN, "MSD: _SendCommandReadData: device breaks the data phase by sending a CSW: CSW-Status: %d!", (int)csw.Status));
              if (csw.Status != CSW_STATUS_PHASE_ERROR) {                // No phase error
                if (csw.Residue) { // This is not implemented in the same way from vendors!
                  * pDataLength = cbw.DataTransferLength - csw.Residue;
                } else {
                  * pDataLength = Length - CSW_LENGTH;                    // CSW_LENGTH because CSW sent at the end of the pBuffer
                }
                if (csw.Status == CSW_STATUS_FAIL) {
                  Status = USBH_STATUS_COMMAND_FAILED;
                } else {                                                 // on success
                  if (*pDataLength != Length - CSW_LENGTH) {
                    USBH_WARN((USBH_MTYPE_MSD_INTERN, "MSD: _SendCommandReadData: invalid Residue!"));
                  }
                }
                pDev->ErrorRecoveryCt = 0;
                break; // This breaks the for loop: indirect return!
              }
              Recovery = TRUE;
              continue; // Repeat all
            }
          }
        }
      }
    }
    //
    // STATUS PHASE
    //
    Status = _ReadCSW(pDev, &cbw, &csw);
    if (!Status) { // success
      if (csw.Status != CSW_STATUS_PHASE_ERROR) { // no phase error
        if (csw.Residue) { // This is not implemented in the same way from vendors!
          * pDataLength = cbw.DataTransferLength - csw.Residue;
        } else {
          * pDataLength = Length;
        }
        if (csw.Status == CSW_STATUS_FAIL) {
          Status = USBH_STATUS_COMMAND_FAILED;
        } else {
          if (*pDataLength != Length) {
            USBH_WARN((USBH_MTYPE_MSD_INTERN, "MSD: _SendCommandReadData: invalid Residue expected:%d rcv:%d!", *pDataLength,Length));
          }
        }
        pDev->ErrorRecoveryCt = 0;
        break; // Return
      }
    // On phase error repeat the same command
    }
    // Repeat the same command with error Recovery
    Recovery = TRUE; // Recover the device as long as the recover counter is valid
  }
  return Status;
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
  if (NULL != pDev->hInterface) {
    USBH_CloseInterface(pDev->hInterface);
    pDev->hInterface = NULL;
  }
  if (NULL != pDev->pUrbEvent) {
    USBH_OS_FreeEvent(pDev->pUrbEvent);
    pDev->pUrbEvent = NULL;
  }
  _FreeLuns(pDev); // Free all units
#if (USBH_DEBUG > 1)
  if (pDev->RefCnt != 0) {
    USBH_WARN((USBH_MTYPE_MSD_INTERN, "MSD: USBH_MSD_FreeDevice: RefCnt not zero: %d!",pDev->RefCnt));
  }
#endif
  pDev->Removed   = TRUE;
  pDev->Magic     = 0;
  pDev->IsValid = FALSE;

  if (NULL != pDev->pCbwPhyTransferBuffer) {
    USBH_URB_BufferFreeTransferBuffer(pDev->pCbwPhyTransferBuffer);
  }
  if (NULL != pDev->pCswPhyTransferBuffer) {
    USBH_URB_BufferFreeTransferBuffer(pDev->pCswPhyTransferBuffer);
  }
  if (NULL != pDev->pEP0PhyTransferBuffer) {
    USBH_URB_BufferFreeTransferBuffer(pDev->pEP0PhyTransferBuffer);
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
    USBH_PANIC("");  //((USBH_MTYPE_MSD, "MSD: Invalid USBH MSD RefCnt: %d",(pDev)->RefCnt));
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
  USBH_ASSERT_MAGIC(pDev, USBH_MSD_DEVICE);
  USBH_LOG((USBH_MTYPE_MSD_INTERN, "MSD: _AllocLuns: MaxLunIndex: %d",MaxLunIndex));
  if (USBH_ARRAY_ELEMENTS(pDev->apUnit) <= MaxLunIndex) {
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
  USBH_ASSERT(0 == pDev->UnitCnt);
  USBH_ASSERT(0 == pDev->OpenUnitCt);
  for (i = 0; i < NumUnits; i++) {
    pUnit   = &USBH_MSD_Global.aUnit[i];
    if (pUnit->pDev == NULL) {
      USBH_ZERO_MEMORY(pUnit, sizeof(USBH_MSD_UNIT)); // Clear all data
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
  USBH_ZERO_MEMORY(pDev, sizeof(USBH_MSD_DEVICE));
  pDev->Magic                = USBH_MSD_DEVICE_MAGIC;
  pDev->pDriver            = &USBH_MSD_Global;
  pDev->IsValid            = TRUE;
  pDev->InterfaceID          = interfaceID;
  pDev->RefCnt                = 1; // Initial reference
  pDev->pCbwPhyTransferBuffer = (U8 *)USBH_URB_BufferAllocateTransferBuffer(CBW_LENGTH);
  if (NULL == pDev->pCbwPhyTransferBuffer) {
    status = USBH_STATUS_RESOURCES;
    USBH_WARN((USBH_MTYPE_MSD, "MSD: _InitDevice: Could not allocate CBW transfer buffer!"));
    goto exit;
  }
  pDev->pCswPhyTransferBuffer = (U8 *)USBH_URB_BufferAllocateTransferBuffer(CSW_LENGTH);
  if (NULL == pDev->pCswPhyTransferBuffer) {
    status = USBH_STATUS_RESOURCES;
    USBH_WARN((USBH_MTYPE_MSD, "MSD: _InitDevice: Could not allocate CSW transfer buffer!"));
    goto exit;
  }
  pDev->pEP0PhyTransferBuffer = (U8 *)USBH_URB_BufferAllocateTransferBuffer(MAX_EP0_TRANSFER_BUFFER_LENGTH);
  if (NULL == pDev->pEP0PhyTransferBuffer) {
    status = USBH_STATUS_RESOURCES;
    USBH_WARN((USBH_MTYPE_MSD, "MSD: _InitDevice: Could not allocate EP0 transfer buffer!"));
    goto exit;
  }
  return USBH_STATUS_SUCCESS;
exit:
  if (NULL != pDev->pCbwPhyTransferBuffer) {
    USBH_URB_BufferFreeTransferBuffer(pDev->pCbwPhyTransferBuffer);
  }
  if (NULL != pDev->pCswPhyTransferBuffer) {
    USBH_URB_BufferFreeTransferBuffer(pDev->pCswPhyTransferBuffer);
  }
  if (NULL != pDev->pEP0PhyTransferBuffer) {
    USBH_URB_BufferFreeTransferBuffer(pDev->pEP0PhyTransferBuffer);
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
*    in the debug version InterfaceId is checked if in use
*/
static USBH_MSD_DEVICE * _NewDevice(USBH_INTERFACE_ID InterfaceId) {
  int          i;
  USBH_MSD_DEVICE * pDev;
  USBH_STATUS       Status;

  USBH_LOG((USBH_MTYPE_MSD_INTERN, "MSD: _NewDevice"));
#if (USBH_DEBUG > 1)
  pDev = _SearchDevicePtrByInterfaceID(InterfaceId); // check that not the same ID does exists
  if (NULL != pDev) {
    USBH_WARN((USBH_MTYPE_MSD, "MSD: _NewDevice: Same interface Id alreadey xists!"));
  }
#endif
  for (i = 0; i < USBH_MSD_MAX_DEVICES; i++) {
    pDev = &USBH_MSD_Global.aDevice[i];
    if (!pDev->IsValid) {
      Status = _InitDevice(pDev, InterfaceId);
      if (Status) { // On error
        USBH_WARN((USBH_MTYPE_MSD, "MSD: _NewDevice: _InitDevice failed (%s)!", USBH_GetStatusStr(Status)));
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
      pDev->pfReset            = _BULKONLY_MassStorageReset;
      pDev->pfGetMaxLUN        = _BULKONLY_GetMaxLUN;
      pDev->pfCommandReadData  = _SendCommandReadData;
      pDev->pfCommandWriteData = _SendCommandWriteData;
      break;
    default:
      USBH_WARN((USBH_MTYPE_MSD_INTERN, "MSD: Device contains an invalid protocol: %u!",pDev->Interfaceprotocol));
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
    USBH_WARN((USBH_MTYPE_MSD_INTERN, "MSD: Device contains an invalid sub class: %d!",pDev->InterfaceSubClass));
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
  count             = sizeof(desc);
  status            = USBH_GetEndpointDescriptor(pDev->hInterface, 0, &ep_mask, desc, &count);
  if (status || count != USB_ENDPOINT_DESCRIPTOR_LENGTH) {
    USBH_WARN((USBH_MTYPE_MSD_INTERN, "MSD: Failed to get BulkEP In (%s)", USBH_GetStatusStr(status)));
    return status;
  }
  // Save information
  _ConvEndpointDesc(desc, &usbEpDesc);
  pDev->BulkMaxPktSize = usbEpDesc.wMaxPacketSize;
  pDev->BulkInEp       = usbEpDesc.bEndpointAddress;
  // Use previous mask change direction to bulk OUT
  ep_mask.Direction = 0;
  count = sizeof(desc);
  status            = USBH_GetEndpointDescriptor(pDev->hInterface, 0, &ep_mask, desc, &count);
  if (status) {
    USBH_WARN((USBH_MTYPE_MSD_INTERN, "MSD: Failed to get BulkEP Out (%s)", USBH_GetStatusStr(status)));
    return status;
  }
  _ConvEndpointDesc(desc, &usbEpDesc);
  if (pDev->BulkMaxPktSize != usbEpDesc.wMaxPacketSize) {
    USBH_WARN((USBH_MTYPE_MSD_INTERN, "MSD: USBH_MSD_GetAndSaveProtocolEndpoints: different max.packet sizes between ep: 0x%x and ep: 0x%x", pDev->BulkInEp, usbEpDesc.bEndpointAddress));
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
static USBH_STATUS _ValidateInterface(USBH_MSD_DEVICE * pDev, USBH_INTERFACE_INFO * pInfo) {
  USBH_STATUS Status = USBH_STATUS_SUCCESS;
  U8         InterfaceClass;
  U8         InterfaceSubClass;
  U8         InterfaceProtocol;

  InterfaceClass    = pInfo->Class;
  InterfaceSubClass = pInfo->SubClass;
  InterfaceProtocol = pInfo->Protocol;
  if (InterfaceClass != MASS_STORAGE_CLASS) {
    USBH_WARN((USBH_MTYPE_MSD_INTERN, "MSD: : USBH_MSD_CheckInterface: Invalid device class: %u",(unsigned int)InterfaceClass));
    Status = USBH_STATUS_ERROR;
    goto Exit;
  }
  switch (InterfaceSubClass) {
  case SUBCLASS_6:         // Valid sub class
    break;
  case SUBCLASS_5:         // Valid sub class
    break;
  default:
    USBH_WARN((USBH_MTYPE_MSD_INTERN, "MSD: USBH_MSD_CheckInterface: Invalid sub class: %u",(unsigned int)InterfaceSubClass));
    Status = USBH_STATUS_INTERFACE_SUB_CLASS;
    goto Exit;
  }
  switch (InterfaceProtocol) {
  case PROTOCOL_BULK_ONLY: // Valid protocol
    break;
  default:
    USBH_WARN((USBH_MTYPE_MSD_INTERN, "MSD: USBH_MSD_CheckInterface: Invalid interface protocol: %u",(unsigned int)InterfaceProtocol));
    Status = USBH_STATUS_INTERFACE_PROTOCOL;
  }
Exit:
  if (!Status) {             // On success
    pDev->Interfaceprotocol = InterfaceProtocol;
    pDev->InterfaceSubClass = InterfaceSubClass;
  }
  return Status;
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
  USBH_STATUS           Status  = USBH_STATUS_ERROR;
  USBH_INTERFACE_INFO   InterFaceInfo;

  USBH_LOG((USBH_MTYPE_MSD_INTERN, "MSD: _CheckAndOpenInterface"));
  USBH_ASSERT_MAGIC(pDev, USBH_MSD_DEVICE);
  if (pDev->Removed) {
    USBH_WARN((USBH_MTYPE_MSD_INTERN, "MSD: _CheckAndOpenInterface: Device removed!"));
    return USBH_STATUS_INVALID_PARAM;
  }
  Status = USBH_GetInterfaceInfo(pDev->InterfaceID, &InterFaceInfo);
  if (USBH_STATUS_SUCCESS != Status) {
    USBH_WARN((USBH_MTYPE_MSD, "MSD: _CheckAndOpenInterface: interface info failed 0x%08x!",Status));
    return Status;
  }
  Status = _ValidateInterface(pDev, &InterFaceInfo);
  if (USBH_STATUS_SUCCESS != Status) {
    USBH_WARN((USBH_MTYPE_MSD, "MSD: _CheckAndOpenInterface: invalid mass storage interface 0x%08x!",Status));
    return Status;
  }
  Status = USBH_OpenInterface(pDev->InterfaceID, TRUE, &pDev->hInterface); // Open interface exclusive
  if (Status) {
    USBH_WARN((USBH_MTYPE_MSD, "MSD: _CheckAndOpenInterface: USBH_OpenInterface 0x%08x!", Status));
    return Status;
  }
  Status = _GetAndSavelEndpointInformations(pDev);                                             // Save endpoint information
  if (Status) {                                                                                  // Error
    USBH_WARN((USBH_MTYPE_MSD, "MSD: _CheckAndOpenInterface: USBH_MSD_GetAndSaveProtocolEndpoints!"));
    return Status;
  }
  return Status;
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
  int         MaxLun;
  USBH_STATUS Status;
  USBH_LOG((USBH_MTYPE_MSD_INTERN, "MSD: _StartDevice IN-Ep: 0x%x Out-Ep: 0x%x",pDev->BulkInEp,pDev->BulkOutEp));
  USBH_ASSERT_MAGIC(pDev, USBH_MSD_DEVICE);
  if (pDev->Removed) {
    USBH_WARN((USBH_MTYPE_MSD, "MSD: _StartDevice: Device removed!"));
    return USBH_STATUS_INVALID_PARAM;
  }
  Status = pDev->pfGetMaxLUN(pDev, &MaxLun);
  if (Status) {                                     // On error
    if (Status == USBH_STATUS_STALL) {               // stall is allowed
      MaxLun = 0;
    } else {
      USBH_WARN((USBH_MTYPE_MSD, "MSD: _StartDevice: TlGetMaxLUN: st: 0x%08x",Status));
      return Status;
    }
  }
  Status = _AllocLuns(pDev, MaxLun);              // Allocate the logical units for this device
  if (Status) {
    USBH_WARN((USBH_MTYPE_MSD, "MSD: _StartDevice: no LUN resources"));
    return Status;
  }
  Status = pDev->pPhysLayerAPI->pfInitSequence(pDev); // Initialize the device with a protocol specific sequence
  return Status;
}

/*********************************************************************
*
*       _AddDevice
*
*  Function description
*    Adds a USB mass storage interface to the library.
*/
static USBH_STATUS _AddDevice(USBH_INTERFACE_ID InterfaceID) {
  USBH_STATUS        Status;
  U32                Length;
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
  pDev->pUrbEvent = USBH_OS_AllocEvent();
  if (pDev->pUrbEvent == NULL) {
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
    USBH_WARN((USBH_MTYPE_MSD, "MSD: _AddDevice: _StartDevice:Invalid device! st:0x%x!",Status));
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
  USBH_USE_PARA(Context);
  switch (Event) {
  case USBH_ADD_DEVICE:
    USBH_LOG((USBH_MTYPE_MSD_INTERN, "MSD: _OnDeviceNotify: USBH_ADD_DEVICE InterfaceId: %u !",InterfaceID));
    _AddDevice(InterfaceID); // -1 means check all interfaces
    break;
  case USBH_REMOVE_DEVICE:
    USBH_LOG((USBH_MTYPE_MSD_INTERN, "MSD: _OnDeviceNotify: USBH_REMOVE_DEVICE InterfaceId: %u !",InterfaceID));
    _MarkDeviceAsRemoved(InterfaceID);
    break;
  default:
    break;
  }
}

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
*    pContext:         pContext of LunNotification
*/
int USBH_MSD_Init(USBH_MSD_LUN_NOTIFICATION_FUNC * pfLunNotification, void * pContext) {
  USBH_PNP_NOTIFICATION PnPNotify;
  USBH_INTERFACE_MASK   PnPNotifyMask;

  USBH_MEMSET(&PnPNotifyMask, 0, sizeof(USBH_INTERFACE_MASK));
  PnPNotifyMask.Mask     = USBH_INFO_MASK_CLASS | USBH_INFO_MASK_PROTOCOL;
  PnPNotifyMask.Class    = MASS_STORAGE_CLASS;
  PnPNotifyMask.Protocol = PROTOCOL_BULK_ONLY;
  //
  // Clear device, unit and driver object
  //
  USBH_MEMSET(&USBH_MSD_Global, 0, sizeof(USBH_MSD_Global));
  USBH_MSD_Global.pfLunNotification    = pfLunNotification;
  USBH_MSD_Global.pContext             = pContext;
  USBH_MSD_Global.MaxTransferSize      = USBH_MSD_MAX_TRANSFER_SIZE;
  //
  // Add an plug an play notification routine
  //
  PnPNotify.pContext             = NULL;
  PnPNotify.InterfaceMask       = PnPNotifyMask;
  PnPNotify.pfPnpNotification     = _OnDeviceNotify;
  USBH_MSD_Global.hPnPNotify    = USBH_RegisterPnPNotification(&PnPNotify);
  if (NULL == USBH_MSD_Global.hPnPNotify) {
    USBH_WARN((USBH_MTYPE_MSD, "MSD: USBH_MSD_Init: Failed to register the MSD notification"));
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

  //
  // 1. Unregister all PnP notifications of the device driver.
  // 2. Release all USBH MSD device  resources and delete the device.
  //
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
USBH_STATUS USBH_MSD_GetLuns(USBH_MSD_DEVICE_HANDLE hDevice, int * pLuns) {
  USBH_MSD_DEVICE * pDev;

  USBH_LOG((USBH_MTYPE_MSD, "MSD: USBH_MSD_GetLuns"));
  T_ASSERT_PTR(pLuns);
  *pLuns = 0;
  pDev = (USBH_MSD_DEVICE *)hDevice;
  T_ASSERT_MAGIC(pDev, USBH_MSD_DEVICE);
  if (pDev->Removed) {
    USBH_WARN((USBH_MTYPE_MSD, "MSD: USBH_MSD_GetLuns: device removed!"));
    return USBH_STATUS_DEVICE_REMOVED;
  }
  *pLuns = pDev->UnitCnt;
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
  USBH_MSD_UNIT   * pUnit;
  USBH_MSD_DEVICE * pDev;
  int               Status;

  USBH_LOG((USBH_MTYPE_MSD, "MSD: USBH_MSD_ReadSectors: address: %lu, sectors: %lu",SectorAddress,NumSectors));
  pUnit = &USBH_MSD_Global.aUnit[Unit];
  USBH_ASSERT_MAGIC(pUnit, USBH_MSD_UNIT);
  pDev = pUnit->pDev;
  if (pDev) {
    USBH_ASSERT_MAGIC(pDev, USBH_MSD_DEVICE);
    USBH_ASSERT(NumSectors);
    if (pDev->Removed) {
      USBH_WARN((USBH_MTYPE_MSD, "MSD: USBH_MSD_ReadSectors: device removed!"));
      return USBH_STATUS_DEVICE_REMOVED;
    }
    USBH_ASSERT_PTR(Buffer);
    _IncRefCnt(pDev);
    USBH_MSD_INC_SYNC(pDev);
    Status = pDev->pPhysLayerAPI->pfReadSectors(pUnit, SectorAddress, Buffer, (U16)NumSectors); // Read from the device with the correct protocol layer
    if (Status == USBH_STATUS_COMMAND_FAILED) {
      if (pDev->pPhysLayerAPI->pfRequestSense(pUnit) == USBH_STATUS_SUCCESS) {
        USBH_WARN((USBH_MTYPE_MSD, "MSD: USBH_MSD_ReadSectors failed, SenseCode = 0x%08x",pUnit->Sense.Sensekey));

      }
    } else if (Status) {
      USBH_WARN((USBH_MTYPE_MSD, "MSD: USBH_MSD_ReadSectors: Status %s",USBH_GetStatusStr(Status)));
    }
    _DecRefCnt (pDev);
    USBH_MSD_DEC_SYNC(pDev);
  } else {
    Status = USBH_STATUS_DEVICE_REMOVED;
  }
  return Status;
}

/*********************************************************************
*
*       USBH_MSD_WriteSectors
*
*  Function description
*    Writes sectors to a USB MSD
*/
int USBH_MSD_WriteSectors(U8 Unit, U32 SectorAddress, U32 NumSectors, const U8 * pBuffer) {
  USBH_MSD_UNIT   * pUnit;
  USBH_MSD_DEVICE * pDev;
  int               Status;

  USBH_LOG((USBH_MTYPE_MSD, "MSD: USBH_MSD_WriteSectors: address: %lu, sectors: %lu", SectorAddress, NumSectors));
  pUnit = &USBH_MSD_Global.aUnit[Unit];
  pDev  = pUnit->pDev;
  if (pDev) {
    USBH_ASSERT_MAGIC(pUnit, USBH_MSD_UNIT);
    USBH_ASSERT_MAGIC(pDev, USBH_MSD_DEVICE);
    USBH_ASSERT(NumSectors);
    if (pDev->Removed) {
      USBH_WARN((USBH_MTYPE_MSD, "MSD: USBH_MSD_WriteSectors: device removed!"));
      return USBH_STATUS_DEVICE_REMOVED;
    }
    USBH_ASSERT_PTR(pBuffer);
    _IncRefCnt(pDev);
    USBH_MSD_INC_SYNC(pDev);
    Status = pDev->pPhysLayerAPI->pfWriteSectors(pUnit, SectorAddress, pBuffer, (U16)NumSectors); // Write to the device with the right protocol layer
    if (Status == USBH_STATUS_COMMAND_FAILED) {
      if (pDev->pPhysLayerAPI->pfRequestSense(pUnit) == USBH_STATUS_SUCCESS) {
        USBH_WARN((USBH_MTYPE_MSD, "MSD: USBH_MSD_WriteSectors failed, SenseCode = 0x%08x",pUnit->Sense.Sensekey));

      }
    } else if (Status) {
      USBH_WARN((USBH_MTYPE_MSD, "MSD: USBH_MSD_WriteSectors: Status %s",USBH_GetStatusStr(Status)));
    }
    _DecRefCnt(pDev);
    USBH_MSD_DEC_SYNC(pDev);
  } else {
    Status = USBH_STATUS_DEVICE_REMOVED;
  }
  return Status;
}

/*********************************************************************
*
*       USBH_MSD_GetStatus
*
*  Function description
*    Checks the Status of a device. Therefore it calls USBH_MSD_GetUnit to
*    test if the device is still connected and if a logical unit is assigned.
*
*  Return value:
*    on success: USBH_STATUS_SUCCESS device is ready for operations
*    other values are errors
*/
USBH_STATUS USBH_MSD_GetStatus(U8 Unit) {
  USBH_MSD_UNIT  * pUnit;
  USBH_STATUS      Status = USBH_STATUS_SUCCESS;

  pUnit = &USBH_MSD_Global.aUnit[Unit];
  if (pUnit->pDev) {
    USBH_ASSERT_MAGIC(pUnit, USBH_MSD_UNIT);
    USBH_ASSERT_MAGIC(pUnit->pDev, USBH_MSD_DEVICE);
    if (pUnit->pDev->Removed) {
      USBH_WARN((USBH_MTYPE_MSD, "MSD: USBH_MSD_GetStatus: device removed!"));
      return USBH_STATUS_DEVICE_REMOVED;
    }
  } else {
    return USBH_STATUS_DEVICE_REMOVED;
  }
  return Status;
}

/*********************************************************************
*
*       USBH_MSD_GetUnitInfo
*
*  Function description
*    Returns basic information about the LUN
*/
USBH_STATUS USBH_MSD_GetUnitInfo(U8 Unit, USBH_MSD_UNIT_INFO * pInfo) {
  USBH_MSD_UNIT * pUnit;
  USBH_INTERFACE_INFO IFaceInfo;


  USBH_ASSERT_PTR(pInfo);
  pUnit = &USBH_MSD_Global.aUnit[Unit];
  if (pUnit->pDev) {
    USBH_ASSERT_MAGIC(pUnit, USBH_MSD_UNIT);
    USBH_ASSERT_MAGIC(pUnit->pDev, USBH_MSD_DEVICE);
    if (pUnit->pDev->Removed) {
      USBH_WARN((USBH_MTYPE_MSD, "MSD: USBH_MSD_GetUnitInfo: device removed!"));
      return USBH_STATUS_DEVICE_REMOVED;
    }
    USBH_GetInterfaceInfo(pUnit->pDev->InterfaceID, &IFaceInfo);
    USBH_MEMSET(pInfo, 0, sizeof(USBH_MSD_UNIT_INFO));
    pInfo->WriteProtectFlag = (0 != (pUnit->ModeParamHeader.DeviceParameter &MODE_WRITE_PROTECT_MASK)) ? TRUE : FALSE;
    pInfo->BytesPerSector   = pUnit->BytesPerSector;
    pInfo->TotalSectors     = pUnit->MaxSectorAddress + 1;
    pInfo->VendorId         = IFaceInfo.VendorId;
    pInfo->ProductId        = IFaceInfo.ProductId;
    USBH_MEMCPY(&pInfo->acVendorName[0],  &pUnit->InquiryData.aVendorIdentification[0],  sizeof(pUnit->InquiryData.aVendorIdentification));
    USBH_MEMCPY(&pInfo->acProductName[0], &pUnit->InquiryData.aProductIdentification[0], sizeof(pUnit->InquiryData.aProductIdentification));
    USBH_MEMCPY(&pInfo->acRevision[0],    &pUnit->InquiryData.aRevision[0],              sizeof(pUnit->InquiryData.aRevision));
  } else {
    return USBH_STATUS_DEVICE_REMOVED;
  }
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
int USBH_MSD_ConvStandardSense(const U8 * pBuffer, U16 Length, STANDARD_SENSE_DATA * pSense) {
  USBH_ASSERT_PTR(pBuffer);
  USBH_ASSERT_PTR(pSense);
  if (Length < STANDARD_SENSE_LENGTH) {
    return USBH_STATUS_ERROR;
  }
  pSense->ResponseCode  = pBuffer[0];
  pSense->Obsolete      = pBuffer[1];                  //1
  pSense->Sensekey      = pBuffer[2];                  //2
  pSense->Info          = USBH_LoadU32BE(&pBuffer[3]); //3,4,5,6
  pSense->AddLength     = pBuffer[7];                  //7
  pSense->Cmdspecific   = USBH_LoadU32BE(&pBuffer[8]); //8,9,10,11
  pSense->Sensecode     = pBuffer[12];                 //12
  pSense->Sensequalifier= pBuffer[13];                 //13
  pSense->Unitcode      = pBuffer[14];                 //14
  pSense->Keyspecific1  = pBuffer[15];                 //15
  pSense->Keyspecific2  = pBuffer[16];                 //16
  pSense->Keyspecific3  = pBuffer[17];                 //17
  USBH_LOG((USBH_MTYPE_MSD, "MSD: USBH_MSD_ConvStandardSense code: 0x%x, sense key: 0x%x, ASC: 0x%x, ASCQ: 0x%x ", pSense->ResponseCode, pSense->Sensekey & 0xf, pSense->Sensecode, pSense->Sensequalifier));
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
int USBH_MSD_ConvReadCapacity(const U8 * pData, U16 Length, U32 * pMaxBlockAddress, U32 * pBlockLength) {
  USBH_ASSERT_PTR(pData);
  USBH_ASSERT_PTR(pMaxBlockAddress);
  USBH_ASSERT_PTR(pBlockLength);
  if (Length < RD_CAPACITY_DATA_LENGTH) {
    return USBH_STATUS_ERROR;
  }
  *pMaxBlockAddress = USBH_LoadU32BE(pData);     // Last possible block address
  *pBlockLength     = USBH_LoadU32BE(pData + 4); // Number of bytes per sector
  return USBH_STATUS_SUCCESS;
}

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
void USBH_MSD_ConvModeParameterHeader(MODE_PARAMETER_HEADER * pModeHeader, const U8 * pBuffer, USBH_BOOL IsModeSense6) {
  USBH_ASSERT_PTR(pModeHeader);
  USBH_ASSERT_PTR(pBuffer);
  if (IsModeSense6) { // Mode sense(6)
    pModeHeader->DataLength            = pBuffer[MODE_PARAMETER_HEADER_DATA_LENGTH_OFS];                            // One byte
    pModeHeader->MediumType            = pBuffer[MODE_PARAMETER_HEADER_MEDIUM_TYPE_OFS_6];
    pModeHeader->DeviceParameter       = pBuffer[MODE_PARAMETER_HEADER_DEVICE_PARAM_OFS_6];
    pModeHeader->BlockDescriptorLength = pBuffer[MODE_PARAMETER_HEADER_BLOCK_DESC_LENGTH_OFS_6];
    pModeHeader->DataOffset            = MODE_PARAMETER_HEADER_BLOCK_DESC_LENGTH_OFS_6 + 1;
  } else {          // Mode sense(10)
    pModeHeader->DataLength            = USBH_LoadU16BE (pBuffer);                                                  // Data pLength
    pModeHeader->MediumType            = pBuffer[MODE_PARAMETER_HEADER_MEDIUM_TYPE_OFS_10];
    pModeHeader->DeviceParameter       = pBuffer[MODE_PARAMETER_HEADER_DEVICE_PARAM_OFS_10];
    pModeHeader->BlockDescriptorLength = USBH_LoadU16BE(&pBuffer[MODE_PARAMETER_HEADER_BLOCK_DESC_LENGTH_OFS_10]);  // Data pLength
    pModeHeader->DataOffset            = MODE_PARAMETER_HEADER_BLOCK_DESC_LENGTH_OFS_6 + 2;                        // Because the pLength is a 16 bit value
  }
}


/******************************* EOF ********************************/
