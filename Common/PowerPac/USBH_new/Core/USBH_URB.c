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

/*********************************************************************
*
*       Types
*
**********************************************************************
*/
typedef struct tag_URB_BUFFER_MALLOC_HEADER { // Helper struct to allocate aligned memory
  U8 * MemBlock;
} URB_BUFFER_MALLOC_HEADER;

/*********************************************************************
*
*       Static code
*
**********************************************************************
*/

/*********************************************************************
*
*       _SubStateTimerRoutine
*
*  Function description
*    Timer routine, timer is always started
*/
static void _SubStateTimerRoutine(void * pContext) {
  URB_SUB_STATE * pSubState;
  USBH_STATUS     Status;

  USBH_LOG((USBH_MTYPE_URB, "USBH_URB: _SubStateTimerRoutine!"));
  USBH_ASSERT_PTR(pContext);
  pSubState = (URB_SUB_STATE * )pContext;
  if (pSubState->TimerCancelFlag) {
    pSubState->TimerCancelFlag = FALSE;
    USBH_LOG((USBH_MTYPE_URB, "USBH_URB: _SubStateTimerRoutine: TimerCancelFlag true, return!"));
    return;
  }
  switch (pSubState->State) {
  case USBH_SUBSTATE_IDLE:
    break;
  case USBH_SUBSTATE_TIMER:
    if (NULL != pSubState->pDevRefCnt) {
      DEC_REF(pSubState->pDevRefCnt);
    }
    pSubState->State = USBH_SUBSTATE_IDLE;
    pSubState->pfCallback(pSubState->pContext);
    break;
  case USBH_SUBSTATE_TIMERURB:
    USBH_ASSERT_PTR(pSubState->pUrb);
    pSubState->State = USBH_SUBSTATE_TIMEOUT_PENDING_URB;
    Status           = pSubState->pHostController->pDriver->pfAbortEndpoint(*pSubState->phEP);
    if (Status != USBH_STATUS_SUCCESS) {
      USBH_WARN((USBH_MTYPE_URB, "USBH_URB: _SubStateTimerRoutine:pfAbortEndpoint failed %08x",Status));
      // On error call now the callback routine an set an timeout error
      pSubState->pUrb->Header.Status = USBH_STATUS_TIMEOUT;
      pSubState->pUrb                = NULL;
      pSubState->State               = USBH_SUBSTATE_IDLE;
      if (NULL != pSubState->pDevRefCnt) {
        DEC_REF(pSubState->pDevRefCnt);
      }
      pSubState->pfCallback(pSubState->pContext);
    }
    break;
  default:
    USBH_WARN((USBH_MTYPE_URB, "USBH_URB: _SubStateTimerRoutine: invalid state: %d!",pSubState->State));
  }
}

/*********************************************************************
*
*       _OnSubStateCompletion
*
*  Function description
*    USBH_URB completion routine! Called after call of USBH_URB_SubStateSubmitRequest!
*/
static void _OnSubStateCompletion(USBH_URB * pUrb) {
  URB_SUB_STATE * pSubState;
  USBH_ASSERT_PTR(pUrb);
  pSubState = (URB_SUB_STATE *)pUrb->Header.pInternalContext;
  USBH_ASSERT_PTR(pSubState);
  USBH_ASSERT_PTR(pSubState->pContext);
  USBH_ASSERT_PTR(pSubState->pUrb);
  USBH_ASSERT_PTR(pSubState->pfCallback);
  USBH_LOG((USBH_MTYPE_URB, "USBH_URB: -OnSubStateCompletion state:%d!",pSubState->State));
  if (NULL != pSubState->pDevRefCnt) {
    DEC_REF(pSubState->pDevRefCnt);
  }
  switch (pSubState->State) {
    case USBH_SUBSTATE_IDLE:
      break;
    case USBH_SUBSTATE_TIMEOUT_PENDING_URB:
      pSubState->State = USBH_SUBSTATE_IDLE;
      pSubState->pUrb  = NULL;
      pSubState->pfCallback(pSubState->pContext);
      break;
    case USBH_SUBSTATE_TIMERURB:
      pSubState->State           = USBH_SUBSTATE_IDLE;
      pSubState->pUrb            = NULL;
      pSubState->TimerCancelFlag = TRUE;
      USBH_CancelTimer(pSubState->hTimer);
      pSubState->pfCallback(pSubState->pContext);
      break;
    default:
      USBH_WARN((USBH_MTYPE_URB, "USBH_URB: -OnSubStateCompletion: invalid state: %d!",pSubState->State));
  }
}

/*********************************************************************
*
*       Public code
*
**********************************************************************
*/

/*********************************************************************
*
*       USBH_URB_BufferAllocateTransferBuffer
*
*  Function description
*/
void * USBH_URB_BufferAllocateTransferBuffer(U32 Size) {
  return USBH_TryMalloc(Size);
}

/*********************************************************************
*
*       USBH_URB_BufferFreeTransferBuffer
*
*  Function description
*/
void USBH_URB_BufferFreeTransferBuffer(void * pMemBlock) {
  USBH_Free(pMemBlock);
}

/*********************************************************************
*
*       USBH_URB_CreateTransferBufferPool
*
*  Function description
*/
URB_BUFFER_POOL * USBH_URB_CreateTransferBufferPool(USBH_INTERFACE_HANDLE IfaceHandle, U8 Endpoint, U32 SizePerBuffer, U32 BufferNumbers, int BusMasterTransferMemoryFlag) {
  URB_BUFFER      * buffer;
  URB_BUFFER_POOL * pPool;
  int               i;
  pPool = (URB_BUFFER_POOL *)USBH_Malloc(sizeof(URB_BUFFER_POOL));
  if (pPool == NULL) {
    return NULL;
  }
  USBH_MEMSET(pPool, 0, sizeof(URB_BUFFER_POOL));
  USBH_DLIST_Init(&pPool->ListEntry);
  pPool->Size                = SizePerBuffer;
  pPool->NumberOfBuffer      = BufferNumbers;
  pPool->Endpoint            = Endpoint;
  pPool->hInterface         = IfaceHandle;
  pPool->BusMasterMemoryFlag = BusMasterTransferMemoryFlag;
  pPool->Magic               = URB_BUFFER_POOL_MAGIC;
  i                         = 0;
  do {
    buffer = (URB_BUFFER *)USBH_Malloc(sizeof(URB_BUFFER));
    if (buffer == NULL) {
      USBH_WARN((USBH_MTYPE_URB, "USBH_URB: USBH_URB_CreateTransferBufferPool: USBH_malloc!"));
      USBH_URB_DeleteTransferBufferPool(pPool);
      return NULL;
    }
    if (pPool->BusMasterMemoryFlag) { // Non cached bus master memory must not be aligned!
      buffer->pTransferBuffer = (U8 *)USBH_AllocTransferMemory       (SizePerBuffer, 1);
    } else {                         // Other memory must be aligned
      buffer->pTransferBuffer = (U8 *)USBH_URB_BufferAllocateTransferBuffer(SizePerBuffer);
    }
    USBH_WARN((USBH_MTYPE_URB, "USBH_URB: USBH_URB_CreateTransferBufferPool: buffer: 0x%x length: %d !", buffer->pTransferBuffer,SizePerBuffer));
    if (buffer->pTransferBuffer == NULL) {
      USBH_WARN((USBH_MTYPE_URB, "USBH_URB: USBH_URB_CreateTransferBufferPool: TAL allocate transfer memory!"));
      USBH_Free(buffer);
      USBH_URB_DeleteTransferBufferPool(pPool);
      return NULL;
    }
    buffer->pPool  = pPool;
    buffer->Index = i; /* buffer numbers 0..4 */
    i++;
    USBH_DLIST_InsertTail(&pPool->ListEntry, &buffer->ListEntry);
    pPool->BufferCt++;
  } while (--BufferNumbers);
  return pPool;
}

/*********************************************************************
*
*       USBH_URB_DeleteTransferBufferPool
*
*  Function description
*/
void USBH_URB_DeleteTransferBufferPool(URB_BUFFER_POOL * pPool) {
  USBH_DLIST      * pEntry;
  URB_BUFFER * pBuffer;
  USBH_ASSERT_MAGIC(pPool, URB_BUFFER_POOL);
  if (NULL == pPool) {
    USBH_WARN((USBH_MTYPE_URB, "USBH_URB: USBH_URB_DeleteTransferBufferPool: invalid parameter pool!"));
    return;
  }
  while (!USBH_DLIST_IsEmpty(&pPool->ListEntry)) {
    USBH_DLIST_RemoveHead(&pPool->ListEntry, &pEntry);
    pBuffer       = GET_BUFFER_FROM_ENTRY(pEntry);
    pBuffer->pPool = NULL;
    if (pBuffer->pTransferBuffer != NULL) {
      if (pPool->BusMasterMemoryFlag) {
        USBH_Free(pBuffer->pTransferBuffer);
      } else {
        USBH_URB_BufferFreeTransferBuffer(pBuffer->pTransferBuffer);
      }
    }
    USBH_Free(pBuffer);
  }
  pPool->Size           = 0;
  pPool->BufferCt       = 0;
  pPool->NumberOfBuffer = 0;
  USBH_Free(pPool);     // Destroy the pPool object
}

/*********************************************************************
*
*       USBH_URB_InitUrbBulkTransfer
*
*  Function description
*/
void USBH_URB_InitUrbBulkTransfer(URB_BUFFER * pBuffer, USBH_ON_COMPLETION_FUNC * pfCompletion, void * pContext) {
  USBH_URB * pUrb;

  pUrb                                  = &pBuffer->Urb;
  pBuffer->Size                         = pBuffer->pPool->Size;
  pUrb->Request.BulkIntRequest.Length   = pBuffer->Size;
  pUrb->Header.Function                 = USBH_FUNCTION_BULK_REQUEST;
  pUrb->Request.BulkIntRequest.Endpoint = pBuffer->pPool->Endpoint;
  pUrb->Request.BulkIntRequest.pBuffer  = pBuffer->pTransferBuffer;
  pUrb->Header.pfOnCompletion           = pfCompletion;
  pUrb->Header.pContext                 = pContext;
}

/*********************************************************************
*
*       USBH_URB_GetFromTransferBufferPool
*
*  Function description
*/
URB_BUFFER * USBH_URB_GetFromTransferBufferPool(URB_BUFFER_POOL * pPool) {
  USBH_DLIST      * pEntry;
  URB_BUFFER * pBuffer;
  USBH_ASSERT_MAGIC(pPool, URB_BUFFER_POOL);
  if (USBH_DLIST_IsEmpty(&pPool->ListEntry)) {
    return NULL;
  }
#if (USBH_DEBUG > 1)
  if (!pPool->BufferCt) {
    USBH_WARN((USBH_MTYPE_URB, "USBH_URB: USBH_URB_GetFromTransferBufferPool: pPool already empty!"));
    return NULL;
  }
#endif
  pPool->BufferCt--;
  USBH_DLIST_RemoveHead(&pPool->ListEntry, &pEntry);
  pBuffer = GET_BUFFER_FROM_ENTRY(pEntry);
  return pBuffer;
}

/*********************************************************************
*
*       USBH_URB_PutToTransferBufferPool
*
*  Function description
*/
void USBH_URB_PutToTransferBufferPool(URB_BUFFER * pBuffer) {
  URB_BUFFER_POOL * pPool;
  pPool            = pBuffer->pPool;
  USBH_ASSERT_MAGIC(pPool, URB_BUFFER_POOL);
  if (NULL == pPool) {
    USBH_WARN((USBH_MTYPE_URB, "USBH_URB: USBH_URB_PutToTransferBufferPool: invalid pBuffer!"));
    return;
  }
  pPool->BufferCt++;
#if (USBH_DEBUG > 1)
  if (pPool->BufferCt > pPool->NumberOfBuffer) {
    USBH_WARN((USBH_MTYPE_URB, "USBH_URB: USBH_URB_PutToTransferBufferPool: pPool Ep: 0x%x already full!",pPool->Endpoint));
    return;
  }
#endif
  USBH_WARN((USBH_MTYPE_CORE, "Core: USBH_URB_PutToTransferBufferPool: Ep: 0x%x Pending buffers: %lu!", (int)pPool->Endpoint, USBH_URB_GetPendingCounterBufferPool(pPool)));
  USBH_DLIST_InsertTail(&pPool->ListEntry, &pBuffer->ListEntry);
}

/*********************************************************************
*
*       USBH_URB_GetPendingCounterBufferPool
*
*  Function description
*    Returns pending buffers that are returned with USBH_URB_GetFromTransferBufferPool
*/
U32 USBH_URB_GetPendingCounterBufferPool(URB_BUFFER_POOL * pPool) {
  USBH_ASSERT(pPool->NumberOfBuffer >= pPool->BufferCt);
  return pPool->NumberOfBuffer - pPool->BufferCt;
}

/*********************************************************************
*
*       USBH_URB_SubStateIsIdle
*
*  Function description
*/
USBH_BOOL USBH_URB_SubStateIsIdle(URB_SUB_STATE * pSubState) {
  if (pSubState->State == USBH_SUBSTATE_IDLE) {
    return TRUE;
  } else {
    return FALSE;
  }
}

/*********************************************************************
*
*       USBH_URB_SubStateAllocInit
*
*  Function description
*    For dynamically allocating. The object is also initialized.
*/
URB_SUB_STATE * USBH_URB_SubStateAllocInit(USBH_HOST_CONTROLLER * pHostController, USBH_HC_EP_HANDLE * phEP, USBH_SUBSTATE_FUNC * pfRoutine, void * pContext) {
  URB_SUB_STATE * pSubState;

  USBH_LOG((USBH_MTYPE_URB, "USBH_URB: USBH_URB_SubStateAllocInit!"));
  USBH_ASSERT_PTR(pfRoutine);
  pSubState = (URB_SUB_STATE *)USBH_Malloc(sizeof(URB_SUB_STATE));
  if (NULL == pSubState) {
    return NULL;
  }
  USBH_URB_SubStateInit(pSubState, pHostController, phEP, pfRoutine, pContext);
  return pSubState;
}

/*********************************************************************
*
*       USBH_URB_SubStateInit
*
*  Function description:
*    Object initialization, used for embedded objects.
*
*  Return value:
*    0 on success
*    other value son errors
*/
USBH_STATUS USBH_URB_SubStateInit(URB_SUB_STATE * pSubState, USBH_HOST_CONTROLLER * pHostController, USBH_HC_EP_HANDLE * phEP, USBH_SUBSTATE_FUNC * pfRoutine, void * pContext) {
  USBH_LOG((USBH_MTYPE_URB, "USBH_URB: USBH_URB_SubStateInit!"));
  USBH_ASSERT_PTR(pSubState);
  USBH_ASSERT_PTR(pfRoutine);
  USBH_ASSERT_PTR(pHostController);
  USBH_ZERO_MEMORY(pSubState, sizeof(URB_SUB_STATE));
  pSubState->hTimer = USBH_AllocTimer(_SubStateTimerRoutine, pSubState);
  if (NULL == pSubState->hTimer) {
    USBH_WARN((USBH_MTYPE_URB, "USBH_URB: USBH_URB_SubStateAllocInit: USBH_AllocTimer!"));
    return USBH_STATUS_RESOURCES;
  }
  pSubState->pHostController = pHostController;
  pSubState->phEP            = phEP;
  pSubState->pContext        = pContext;
  pSubState->pfSubmitRequest = pHostController->pDriver->pfSubmitRequest;
  pSubState->pfAbortEndpoint = pHostController->pDriver->pfAbortEndpoint;
  pSubState->pfCallback      = pfRoutine;
  return USBH_STATUS_SUCCESS;
}

/*********************************************************************
*
*       USBH_URB_SubStateExit
*
*  Function description:
*    Must be called if an embedded object is released
*/
void USBH_URB_SubStateExit(URB_SUB_STATE * pSubState) {
  USBH_LOG((USBH_MTYPE_URB, "USBH_URB: USBH_URB_SubStateExit!"));
  USBH_ASSERT_PTR(pSubState);
  if (NULL != pSubState->hTimer) {
    pSubState->TimerCancelFlag = TRUE;
    USBH_CancelTimer(pSubState->hTimer);
    USBH_FreeTimer  (pSubState->hTimer);
    pSubState->hTimer           = NULL;
  }
}

/*********************************************************************
*
*       USBH_URB_SubStateFree
*
*  Function description:
*    Releases all resources. Must be called if AllocUrbSubState is used
*/
void USBH_URB_SubStateFree(URB_SUB_STATE * pSubState) {
  USBH_ASSERT_PTR(pSubState);
  USBH_Free   (pSubState);
}

/*********************************************************************
*
*       USBH_URB_SubStateSubmitRequest
*
*  Function description:
*    Submits an USBH_URB without adding a refcount
*/
USBH_STATUS USBH_URB_SubStateSubmitRequest(URB_SUB_STATE * pSubState, USBH_URB * pUrb, U32 Timeout, USB_DEVICE * pDevRefCnt) {
  USBH_STATUS status;
  USBH_LOG((USBH_MTYPE_URB, "USBH_URB: [USBH_URB_SubStateSubmitRequest timeout:%lu",Timeout));
  USBH_ASSERT_PTR(pSubState);
  USBH_ASSERT_PTR(pUrb);
  USBH_ASSERT(NULL == pSubState->pUrb);
  USBH_ASSERT_PTR(pSubState->pfSubmitRequest);
  USBH_ASSERT(*pSubState->phEP != 0);
  pUrb->Header.pfOnInternalCompletion = _OnSubStateCompletion;
  pUrb->Header.pInternalContext    = pSubState;
  pSubState->pUrb                  = pUrb;
  // Setup a timeout
  pSubState->TimerCancelFlag      = FALSE;
  USBH_StartTimer(pSubState->hTimer, Timeout);
  pSubState->State                = USBH_SUBSTATE_TIMERURB;
  if (NULL != pDevRefCnt) {
    pSubState->pDevRefCnt = pDevRefCnt;
    INC_REF(pDevRefCnt);
  } else {
    pSubState->pDevRefCnt = NULL;
  }
  status = pSubState->pfSubmitRequest(*pSubState->phEP, pUrb);
  if (status != USBH_STATUS_PENDING) {
    USBH_WARN((USBH_MTYPE_URB, "USBH_URB: USBH_URB_SubStateSubmitRequest:pfSubmitRequest failed %08x", status));
    // Cancel the timer and return
    pSubState->State           = USBH_SUBSTATE_IDLE;
    pSubState->TimerCancelFlag = TRUE;
    USBH_CancelTimer(pSubState->hTimer);
    pSubState->pUrb             = NULL;
    if (NULL != pDevRefCnt) {
      DEC_REF(pDevRefCnt);
    }
  }
  USBH_LOG((USBH_MTYPE_URB, "USBH_URB: ]USBH_URB_SubStateSubmitRequest"));
  return status;
}

/*********************************************************************
*
*       USBH_URB_SubStateWait
*
*  Function description:
*    Starts an timer an wait for completion
*/
void USBH_URB_SubStateWait(URB_SUB_STATE * pSubState, U32 Timeout, USB_DEVICE * pDevRefCnt) {
  USBH_LOG((USBH_MTYPE_URB, "USBH_URB: USBH_URB_SubStateWait timeout:%lu",Timeout));
  USBH_ASSERT_PTR(pSubState);
  if (NULL != pDevRefCnt) {
    pSubState->pDevRefCnt = pDevRefCnt;
    INC_REF(pDevRefCnt);
  } else {
    pSubState->pDevRefCnt = NULL;
  }
  pSubState->State = USBH_SUBSTATE_TIMER;
  // Wait for timeout
  pSubState->TimerCancelFlag = FALSE;
  USBH_StartTimer(pSubState->hTimer, Timeout);
}

/******************************* EOF ********************************/
