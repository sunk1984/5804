/*********************************************************************
*  
*   IAR PowerPac
*
*   (c) Copyright IAR Systems 2010.  All rights reserved.
*
**********************************************************************
----------------------------------------------------------------------
File        : USBH_URB.c
Purpose     : USB Host transfer buffer memory pool
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

typedef struct tag_URB_BUFFER_MALLOC_HEADER { // Helper struct to allocate aligned memory
  U8 * MemBlock;
} URB_BUFFER_MALLOC_HEADER;

#if USBH_TRANSFER_BUFFER_ALIGNMENT != 1

/*********************************************************************
*
*       UrbBufferAllocateTransferBuffer
*
*  Function description
*/
void * UrbBufferAllocateTransferBuffer(U32 size) {
  U32                        totalSize;
  U8                       * p, * memBlock;
  URB_BUFFER_MALLOC_HEADER * header;
  totalSize = USBH_TRANSFER_BUFFER_ALIGNMENT + size + sizeof(URB_BUFFER_MALLOC_HEADER);
  p         = USBH_malloc(totalSize);
  if (NULL == p) {
    USBH_WARN((USBH_MTYPE_URB, "URB: UrbBufferAllocateTransferBuffer: no memory!"));
    return NULL;
  }
  memBlock  = p;
  p        += sizeof(URB_BUFFER_MALLOC_HEADER);
  p         = (U8 *)TB_ALIGN_UP((U32)p, USBH_TRANSFER_BUFFER_ALIGNMENT);
  // p points to an aligned address
  header = (URB_BUFFER_MALLOC_HEADER *)p;
  header--;
  header->MemBlock = memBlock; // Points to the allocated memory
  DBGOUT(DBG_BUF,DbgPrint(DBGPFX "UrbBufferAllocateTransferBuffer: buffer: 0x%x length: %d total length;: %d!", p, size, totalSize));
  return p;
}

/*********************************************************************
*
*       UrbBufferAllocateTransferBuffer
*
*  Function description
*/
void UrbBufferFreeTransferBuffer(void * memBlock) {
  URB_BUFFER_MALLOC_HEADER * header;
  header = (URB_BUFFER_MALLOC_HEADER *)memBlock;
  header--;
  T_ASSERT_PTR(header->MemBlock);
  USBH_Free(header->MemBlock);
}

#else

/*********************************************************************
*
*       UrbBufferAllocateTransferBuffer
*
*  Function description
*/
void * UrbBufferAllocateTransferBuffer(U32 size) {
  return USBH_TryMalloc(size);
}

/*********************************************************************
*
*       UrbBufferFreeTransferBuffer
*
*  Function description
*/
void UrbBufferFreeTransferBuffer(void * memBlock) {
  USBH_Free(memBlock);
}

#endif

/*********************************************************************
*
*       CreateTransferBufferPool
*
*  Function description
*/
URB_BUFFER_POOL * CreateTransferBufferPool(USBH_INTERFACE_HANDLE IfaceHandle, U8 Endpoint, U32 SizePerBuffer, U32 BufferNumbers, int BusMasterTransferMemoryFlag) {
  URB_BUFFER      * buffer;
  URB_BUFFER_POOL * Pool;
  int               i;
  Pool = (URB_BUFFER_POOL *)USBH_Malloc(sizeof(URB_BUFFER_POOL));
  if (Pool == NULL) {
    return NULL;
  }
  USBH_MEMSET(Pool, 0, sizeof(URB_BUFFER_POOL));
  DlistInit(&Pool->ListEntry);
  Pool->Size                = SizePerBuffer;
  Pool->NumberOfBuffer      = BufferNumbers;
  Pool->Endpoint            = Endpoint;
  Pool->IfaceHandle         = IfaceHandle;
  Pool->BusMasterMemoryFlag = BusMasterTransferMemoryFlag;
  Pool->Magic               = URB_BUFFER_POOL_MAGIC;
  i                         = 0;
  do {
    buffer = (URB_BUFFER *)USBH_Malloc(sizeof(URB_BUFFER));
    if (buffer == NULL) {
      USBH_WARN((USBH_MTYPE_URB, "URB: CreateTransferBufferPool: USBH_malloc!"));
      DeleteTransferBufferPool(Pool);
      return NULL;
    }
    if (Pool->BusMasterMemoryFlag) { // Non cached bus master memory must not be aligned!
      buffer->TransferBuffer = (U8 *)USBH_AllocTransferMemory       (SizePerBuffer, 1);
    } else {                         // Other memory must be aligned
      buffer->TransferBuffer = (U8 *)UrbBufferAllocateTransferBuffer(SizePerBuffer);
    }
    USBH_WARN((USBH_MTYPE_URB, "URB: CreateTransferBufferPool: buffer: 0x%x length: %d !", buffer->TransferBuffer,SizePerBuffer));
    if (buffer->TransferBuffer == NULL) {
      USBH_WARN((USBH_MTYPE_URB, "URB: CreateTransferBufferPool: TAL allocate transfer memory!"));
      USBH_Free(buffer);
      DeleteTransferBufferPool(Pool);
      return NULL;
    }
    buffer->Pool  = Pool;
    buffer->Index = i; /* buffer numbers 0..4 */
    i++;
    DlistInsertTail(&Pool->ListEntry, &buffer->ListEntry);
    Pool->BufferCt++;
  } while (--BufferNumbers);
  return Pool;
}

/*********************************************************************
*
*       DeleteTransferBufferPool
*
*  Function description
*/
void DeleteTransferBufferPool(URB_BUFFER_POOL * Pool) {
  PDLIST       entry;
  URB_BUFFER * Buffer;
  T_ASSERT_MAGIC(Pool, URB_BUFFER_POOL);
  if (NULL == Pool) {
    USBH_WARN((USBH_MTYPE_URB, "URB: DeleteTransferBufferPool: invalid parameter pool!"));
    return;
  }
  while (!DlistEmpty(&Pool->ListEntry)) {
    DlistRemoveHead(&Pool->ListEntry, &entry);
    Buffer       = GET_BUFFER_FROM_ENTRY(entry);
    Buffer->Pool = NULL;
    if (Buffer->TransferBuffer != NULL) {
      if (Pool->BusMasterMemoryFlag) {
        USBH_Free(Buffer->TransferBuffer);
      } else {
        UrbBufferFreeTransferBuffer(Buffer->TransferBuffer);
      }
    }
    USBH_Free(Buffer);
  }
  Pool->Size           = 0;
  Pool->BufferCt       = 0;
  Pool->NumberOfBuffer = 0;
  USBH_Free(Pool);     // Destroy the Pool object
}

/*********************************************************************
*
*       InitUrbBulkTransfer
*
*  Function description
*/
void InitUrbBulkTransfer(URB_BUFFER * Buffer, USBH_ON_COMPLETION_FUNC Completion, void * Context) {
  URB * urb;
  urb                                  = &Buffer->Urb;
  Buffer->Size                         = Buffer->Pool->Size;
  urb->Request.BulkIntRequest.Length   = Buffer->Size;
  urb->Header.Function                 = USBH_FUNCTION_BULK_REQUEST;
  urb->Request.BulkIntRequest.Endpoint = Buffer->Pool->Endpoint;
  urb->Request.BulkIntRequest.Buffer   = Buffer->TransferBuffer;
  urb->Header.Completion               = Completion;
  urb->Header.Context                  = Context;
}

/*********************************************************************
*
*       GetFromTransferBufferPool
*
*  Function description
*/
URB_BUFFER * GetFromTransferBufferPool(URB_BUFFER_POOL * Pool) {
  PDLIST       entry;
  URB_BUFFER * buffer;
  T_ASSERT_MAGIC(Pool, URB_BUFFER_POOL);
  if (DlistEmpty(&Pool->ListEntry)) {
    return NULL;
  }
#if (USBH_DEBUG > 1)
  if (!Pool->BufferCt) {
    USBH_WARN((USBH_MTYPE_URB, "URB: GetFromTransferBufferPool: Pool already empty!"));
    return NULL;
  }
#endif
  Pool->BufferCt--;
  DlistRemoveHead(&Pool->ListEntry, &entry);
  buffer = GET_BUFFER_FROM_ENTRY(entry);
  return buffer;
}

/*********************************************************************
*
*       PutToTransferBufferPool
*
*  Function description
*/
void PutToTransferBufferPool(URB_BUFFER * Buffer) {
  URB_BUFFER_POOL * Pool;
  Pool            = Buffer->Pool;
  T_ASSERT_MAGIC(Pool, URB_BUFFER_POOL);
  if (NULL == Pool) {
    USBH_WARN((USBH_MTYPE_URB, "URB: PutToTransferBufferPool: invalid Buffer!"));
    return;
  }
  Pool->BufferCt++;
#if (USBH_DEBUG > 1)
  if (Pool->BufferCt > Pool->NumberOfBuffer) {
    USBH_WARN((USBH_MTYPE_URB, "URB: PutToTransferBufferPool: Pool Ep: 0x%x already full!",Pool->Endpoint));
    return;
  }
#endif
  USBH_WARN((USBH_MTYPE_CORE, "Core: PutToTransferBufferPool: Ep: 0x%x Pending buffers: %lu!", (int)Pool->Endpoint, GetPendingCounterBufferPool(Pool)));
  DlistInsertTail(&Pool->ListEntry, &Buffer->ListEntry);
}

/*********************************************************************
*
*       GetPendingCounterBufferPool
*
*  Function description
*    Returns pending buffers that are returned with GetFromTransferBufferPool
*/
U32 GetPendingCounterBufferPool(URB_BUFFER_POOL * Pool) {
  T_ASSERT(Pool->NumberOfBuffer >= Pool->BufferCt);
  return Pool->NumberOfBuffer - Pool->BufferCt;
}

/*********************************************************************
*
*       UrbSubStateTimerRoutine
*
*  Function description
*    Timer routine, timer is always started
*/
static void UrbSubStateTimerRoutine(void * context) {
  URB_SUB_STATE * sub_state;
  USBH_STATUS     status;
  USBH_LOG((USBH_MTYPE_URB, "URB: UrbSubStateTimerRoutine!"));
  T_ASSERT_PTR(context);
  sub_state = (URB_SUB_STATE * )context;
  if (sub_state->TimerCancelFlag) {
    sub_state->TimerCancelFlag = FALSE;
    USBH_LOG((USBH_MTYPE_URB, "URB: UrbSubStateTimerRoutine: TimerCancelFlag true, retrun!"));
    return;
  }
  switch (sub_state->state) {
    case SUBSTATE_IDLE:
      break;
    case SUBSTATE_TIMER:
      if (NULL != sub_state->RefCtDev) {
        DEC_REF(sub_state->RefCtDev);
      }
      sub_state->state = SUBSTATE_IDLE;
      sub_state->CallbackRoutine(sub_state->Context);
      break;
    case SUBSTATE_TIMERURB:
      T_ASSERT_PTR(sub_state->Urb);
      sub_state->state = SUBSTATE_TIMEOUT_PENDING_URB;
      status           = sub_state->Hc->HostEntry.AbortEndpoint(* sub_state->EpHandle);
      if (status != USBH_STATUS_SUCCESS) {
        USBH_WARN((USBH_MTYPE_URB, "URB: UrbSubStateTimerRoutine:AbortEndpoint failed %08x",status));
        // On error call now the callback routine an set an timeout error
        sub_state->Urb->Header.Status = USBH_STATUS_TIMEOUT;
        sub_state->Urb                = NULL;
        sub_state->state              = SUBSTATE_IDLE;
        if (NULL != sub_state->RefCtDev) {
          DEC_REF(sub_state->RefCtDev);
        }
        sub_state->CallbackRoutine(sub_state->Context);
      }
      break;
    default:
      USBH_WARN((USBH_MTYPE_URB, "URB: UrbSubStateTimerRoutine: invlaid state: %d!",sub_state->state));
  }
}

/*********************************************************************
*
*       UrbSubStateCompletion
*
*  Function description
*    URB completion routine! Called after call of UrbSubStateSubmitRequest!
*/
static void UrbSubStateCompletion(URB * urb) {
  URB_SUB_STATE * sub_state;
  T_ASSERT_PTR(urb);
  sub_state = (URB_SUB_STATE *)urb->Header.InternalContext;
  T_ASSERT_PTR(sub_state);
  T_ASSERT_PTR(sub_state->Context);
  T_ASSERT_PTR(sub_state->Urb);
  T_ASSERT_PTR(sub_state->CallbackRoutine);
  USBH_LOG((USBH_MTYPE_URB, "URB: UrbSubStateCompletion state:%d!",sub_state->state));
  if (NULL != sub_state->RefCtDev) {
    DEC_REF(sub_state->RefCtDev);
  }
  switch (sub_state->state) {
    case SUBSTATE_IDLE:
      break;
    case SUBSTATE_TIMEOUT_PENDING_URB:
      sub_state->state = SUBSTATE_IDLE;
      sub_state->Urb   = NULL;
      sub_state->CallbackRoutine(sub_state->Context);
      break;
    case SUBSTATE_TIMERURB:
      sub_state->state           = SUBSTATE_IDLE;
      sub_state->Urb             = NULL;
      sub_state->TimerCancelFlag = TRUE;
      USBH_CancelTimer(sub_state->Timer);
      sub_state->CallbackRoutine(sub_state->Context);
      break;
    default:
      USBH_WARN((USBH_MTYPE_URB, "URB: UrbSubStateCompletion: invalid state: %d!",sub_state->state));
  }
}

/*********************************************************************
*
*       UrbSubStateIsIdle
*
*  Function description
*/
T_BOOL UrbSubStateIsIdle(URB_SUB_STATE * subState) {
  if (subState->state == SUBSTATE_IDLE) {
    return TRUE;
  } else {
    return FALSE;
  }
}

/*********************************************************************
*
*       UrbSubStateAllocInit
*
*  Function description
*    For dynamically allocating. The object is also initialized.
*/
URB_SUB_STATE * UrbSubStateAllocInit(HOST_CONTROLLER * hc, USBH_HC_EP_HANDLE * epHandle, SubStateCallbackRoutine * cbRoutine, void * context) {
  URB_SUB_STATE * sub_state;
  USBH_LOG((USBH_MTYPE_URB, "URB: UrbSubStateAllocInit!"));
  T_ASSERT_PTR(cbRoutine);
  sub_state = USBH_Malloc(sizeof(URB_SUB_STATE));
  if (NULL == sub_state) {
    return NULL;
  }
  UrbSubStateInit(sub_state, hc, epHandle, cbRoutine, context);
  return sub_state;
}

/*********************************************************************
*
*       UrbSubStateInit
*
*  Function description:
*    Object initialization, used for embeedded objects.
*
*  Return value:
*    0 on success
*    other value son errors
*/
USBH_STATUS UrbSubStateInit(URB_SUB_STATE * subState, HOST_CONTROLLER * hc, USBH_HC_EP_HANDLE * epHandle, SubStateCallbackRoutine * cbRoutine, void * context) {
  USBH_LOG((USBH_MTYPE_URB, "URB: UrbSubStateInit!"));
  T_ASSERT_PTR(subState);
  T_ASSERT_PTR(cbRoutine);
  T_ASSERT_PTR(hc);
  ZERO_MEMORY(subState, sizeof(URB_SUB_STATE));
  subState->Timer = USBH_AllocTimer(UrbSubStateTimerRoutine, subState);
  if (NULL == subState->Timer) {
    USBH_WARN((USBH_MTYPE_URB, "URB: UrbSubStateAllocInit: USBH_AllocTimer!"));
    return USBH_STATUS_RESOURCES;
  }
  subState->Hc              = hc;
  subState->EpHandle        = epHandle;
  subState->Context         = context;
  subState->SubmitRequest   = hc->HostEntry.SubmitRequest;
  subState->AbortEndpoint   = hc->HostEntry.AbortEndpoint;
  subState->CallbackRoutine = cbRoutine;
  return USBH_STATUS_SUCCESS;
}

/*********************************************************************
*
*       UrbSubStateExit
*
*  Function description:
*    Must be called if an embedded object is released
*/
void UrbSubStateExit(URB_SUB_STATE * subState) {
  USBH_LOG((USBH_MTYPE_URB, "URB: UrbSubStateExit!"));
  T_ASSERT_PTR(subState);
  if (NULL != subState->Timer) {
    subState->TimerCancelFlag = TRUE;
    USBH_CancelTimer(subState->Timer);
    USBH_FreeTimer  (subState->Timer);
    subState->Timer           = NULL;
  }
}

/*********************************************************************
*
*       UrbSubStateFree
*
*  Function description:
*    Releases all resources. Must be called if AllocUrbSubState is used
*/
void UrbSubStateFree(URB_SUB_STATE * subState) {
  T_ASSERT_PTR(subState);
  USBH_Free   (subState);
}

/*********************************************************************
*
*       UrbSubStateSubmitRequest
*
*  Function description:
*    Submits an URB without adding an refcount
*/
USBH_STATUS UrbSubStateSubmitRequest(URB_SUB_STATE * subState, URB * urb, U32 timeout, USB_DEVICE * refCtDev) {
  USBH_STATUS status;
  USBH_LOG((USBH_MTYPE_URB, "URB: [UrbSubStateSubmitRequest timeout:%lu",timeout));
  T_ASSERT_PTR(subState);
  T_ASSERT_PTR(urb);
  T_ASSERT(NULL == subState->Urb);
  T_ASSERT_PTR(subState->SubmitRequest);
  T_ASSERT(*subState->EpHandle != 0);
  urb->Header.InternalCompletion = UrbSubStateCompletion;
  urb->Header.InternalContext    = subState;
  subState->Urb                  = urb;
  // Setup a timeout
  subState->TimerCancelFlag      = FALSE;
  USBH_StartTimer(subState->Timer, timeout);
  subState->state                = SUBSTATE_TIMERURB;
  if (NULL != refCtDev) {
    subState->RefCtDev = refCtDev;
    INC_REF(refCtDev);
  } else {
    subState->RefCtDev = NULL;
  }
  status = subState->SubmitRequest(*subState->EpHandle, urb);
  if (status != USBH_STATUS_PENDING) {
    USBH_WARN((USBH_MTYPE_URB, "URB: UrbSubStateSubmitRequest:SubmitRequest failed %08x", status));
    // Cancel the timer and return
    subState->state           = SUBSTATE_IDLE;
    subState->TimerCancelFlag = TRUE;
    USBH_CancelTimer(subState->Timer);
    subState->Urb             = NULL;
    if (NULL != refCtDev) {
      DEC_REF(refCtDev);
    }
  }
  USBH_LOG((USBH_MTYPE_URB, "URB: ]UrbSubStateSubmitRequest"));
  return status;
}

/*********************************************************************
*
*       UrbSubStateWait
*
*  Function description:
*    Starts an timer an wait for completion
*/
void UrbSubStateWait(URB_SUB_STATE * subState, U32 timeout, USB_DEVICE * refCtDev) {
  USBH_LOG((USBH_MTYPE_URB, "URB: UrbSubStateWait timeout:%lu",timeout));
  T_ASSERT_PTR(subState);
  if (NULL != refCtDev) {
    subState->RefCtDev = refCtDev;
    INC_REF(refCtDev);
  } else {
    subState->RefCtDev = NULL;
  }
  subState->state = SUBSTATE_TIMER;
  // Wait for timeout
  subState->TimerCancelFlag = FALSE;
  USBH_StartTimer(subState->Timer, timeout);
}

/******************************* EOF ********************************/
