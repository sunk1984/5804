/*********************************************************************
*  
*   IAR PowerPac
*
*   (c) Copyright IAR Systems 2010.  All rights reserved.
*
**********************************************************************
----------------------------------------------------------------------
File        : USBH_MEM.c
Purpose     : USB host, memory management
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

#ifdef USBH_DEBUG
#undef USBH_DEBUG
#define USBH_DEBUG 1
#endif

/*********************************************************************
*
*       #define constants
*
**********************************************************************
*/

#define MAX_BLOCK_SIZE_INDEX 15
#define MIN_BLOCK_SIZE       64UL        // Needs to be a power of 2
#define MIN_BLOCK_SIZE_SHIFT  6UL

/*********************************************************************
*
*       Local data types
*
**********************************************************************
*/

typedef struct MEM_POOL_FREE {
  struct MEM_POOL_FREE * pNext;
} MEM_POOL_FREE;

typedef struct {
  U32 BlockSize;                  // Block size incl. management data
  U32 Adjust;
  U32 Tag;
  U32 Dummy;
} MEM_POOL_ALLOC_INFO;

typedef struct {
  void          * pBaseAddr;
  U32             NumBytes;
  MEM_POOL_FREE * apFreeList[MAX_BLOCK_SIZE_INDEX + 1];

#if USBH_DEBUG
  struct {
    U32 Free;
    U32 NumBytesAllocated;
  } Debug;
#endif

} MEM_POOL;

void   USBH_MEM_POOL_Create(MEM_POOL * pPool, void * pMem,      U32 NumBytes);
void * USBH_MEM_POOL_Alloc (MEM_POOL * pPool, U32 NumBytesUser, U32 Alignment);
void   USBH_MEM_POOL_Free  (MEM_POOL * pPool, void * p);

/*********************************************************************
*
*       Static data
*
**********************************************************************
*/

static MEM_POOL _aMemPool[2];

/*********************************************************************
*
*       Public data
*
**********************************************************************
*/

/*********************************************************************
*
*       #define function replacement
*
**********************************************************************
*/

/*********************************************************************
*
*       Static code
*
**********************************************************************
*/

/*********************************************************************
*
*       Helper functions
*
**********************************************************************
*/

/*********************************************************************
*
*       _ld
*
*  Function description
*/
static U16 _ld(U32 Value) {
  U16 i;
  for (i = 0; i < 16; i++) {
    if ((1UL << i) == Value) {
      break;
    }
  }
  return i;
}

/*********************************************************************
*
*       _ConvertSizeToBlockSizeIndex
*
*  Function description
*    Frees a memory block, putting it back into the pool.
*    The memory must have been allocated from this pool before.
*/
static int _ConvertSizeToBlockSizeIndex(U32 NumBytes) {
  int i;
  for (i = 0; i <= MAX_BLOCK_SIZE_INDEX; i++) {
    if (NumBytes <= (MIN_BLOCK_SIZE << i)) {
      return i;
    }
  }
  USBH_PANIC("Illegal block size requested.");
  return 0;      // To keep compiler happy
}

/*********************************************************************
*
*       _Index2Size
*
*  Function description
*    TBD
*/
static U32 _Index2Size(int i) {
  return MIN_BLOCK_SIZE << i;
}


/*********************************************************************
*
*       _Size2Index
*
*  Function description
*    TBD
*/
static U32 _Size2Index(U32 BlockSize) {
  return _ld(BlockSize) - MIN_BLOCK_SIZE_SHIFT;
}

/*********************************************************************
*
*       _RemoveBlock
*
*  Function description
*    TBD
*/
static void * _RemoveBlock(MEM_POOL * pPool, int BlockSizeIndex) {
  MEM_POOL_FREE                     * pFreeList;
  pFreeList                         = pPool->apFreeList[BlockSizeIndex];
  pPool->apFreeList[BlockSizeIndex] = pFreeList->pNext;
  pFreeList->pNext                  = NULL;
  return pFreeList;
}

/*********************************************************************
*
*       _AddBlock
*
*  Function description
*    TBD
*/
static void _AddBlock(MEM_POOL * pPool, void * p, int BlockSizeIndex) {
  MEM_POOL_FREE * * ppFreeList;

  ppFreeList = &pPool->apFreeList[BlockSizeIndex];
  do {
    if (* ppFreeList == NULL) {
      * ppFreeList = (MEM_POOL_FREE *)p;
      (* ppFreeList)->pNext = NULL;
      break;
    }
    ppFreeList = &((* ppFreeList)->pNext);
  } while (1);
}

/*********************************************************************
*
*       _CreateFreeBlock
*
*  Function description
*    Creates a memory pool
*
*  Return value
*    0: Success
*    1: Could not create block
*/
static int _CreateFreeBlock(MEM_POOL * pPool, int Index) {
  U8  * p;
  int   i;
Restart:
  for (i = Index + 1; i <= MAX_BLOCK_SIZE_INDEX; i++) {
    if (pPool->apFreeList[i]) { // Found a larger block which can be split
      USBH_LOG((USBH_MTYPE_MEM, "MEM: Splitting block of %d bytes", _Index2Size(i)));
      p = (U8*)_RemoveBlock(pPool, i);
      _AddBlock(pPool, p, i - 1);
      _AddBlock(pPool, p + _Index2Size(i - 1), i - 1);
      if (i == Index + 1) {
        return 0;               // Success!
      }
      goto Restart;
    }
  }
  return 1;                     // Out of memory
}

/*********************************************************************
*
*       USBH_MEM_POOL_Create
*
*  Function description
*    Creates a memory pool
*/
void USBH_MEM_POOL_Create(MEM_POOL * pPool, void * pMem, U32 NumBytes) {
  MEM_POOL_FREE * p;
  int             i;
  U32             Size;
  U8            * pMem8;
  USBH_MEMSET(pPool, 0, sizeof(MEM_POOL));
  USBH_MEMSET(pMem,  0, NumBytes);
  pPool->pBaseAddr = pMem;
  pPool->NumBytes  = NumBytes;
  pMem8            = (U8 *)pMem;
  for (i = MAX_BLOCK_SIZE_INDEX; i >= 0; i--) {
    Size = MIN_BLOCK_SIZE << i;
    while (NumBytes >= Size) {
      NumBytes             -= Size;
      p                     = (MEM_POOL_FREE *)pMem8;
      pMem8                += Size;
      p->pNext              = pPool->apFreeList[i];
      pPool->apFreeList[i]  = p;
    }
  }
}

/*********************************************************************
*
*       USBH_MEM_POOL_Alloc
*
*  Function description
*    Allocates a memory block from a pool.
*/
void * USBH_MEM_POOL_Alloc(MEM_POOL * pPool, U32 NumBytesUser, U32 Alignment) {
  U32                   NumBytes;
  unsigned              i;
  MEM_POOL_FREE       * p;
  MEM_POOL_ALLOC_INFO * pAlloc;
  void                * r       = NULL;
  U32                   Adjust;
  //
  // If alignment of Pool base addr. is insufficient, increase NumBytes and perform alignment at the end
  //
  Adjust = 0;
  if (Alignment > sizeof(MEM_POOL_ALLOC_INFO)) {
    Adjust = Alignment - sizeof(MEM_POOL_ALLOC_INFO);
  }
  USBH_LOG((USBH_MTYPE_MEM, "MEM: Allocating %d bytes from memory pool 0x%8x", NumBytesUser, pPool->pBaseAddr));
  //
  // Convert the number of bytes requested by user into the smallest block size.
  //
  NumBytes  = (NumBytesUser + 3) & ~3UL;                                 // Make size a multiple of 4 bytes (for alignment purposes)
  NumBytes += sizeof(MEM_POOL_ALLOC_INFO);
  i         = _ConvertSizeToBlockSizeIndex(NumBytes);
  USBH_OS_LockSys();
  if (pPool->apFreeList[i]) {
Found:
    p                    = pPool->apFreeList[i];
    pPool->apFreeList[i] = p->pNext;                                   // Unlink it
    pAlloc               = (MEM_POOL_ALLOC_INFO *) ((U8 *)p + Adjust); // Take care of Adjustment
    pAlloc->BlockSize    = _Index2Size(i);
    pAlloc->Adjust       = Adjust;
    pAlloc->Tag          = 0;
    r                    = (pAlloc + 1);
    goto End;
  }
  _CreateFreeBlock(pPool,i);
  if (pPool->apFreeList[i]) {
    goto Found;
  }
End:

#if USBH_DEBUG                                                         // DEBUG_ERROR Out of memory
  if (r) {
    pPool->Debug.NumBytesAllocated += _Index2Size(i);
  }
#endif

  USBH_OS_UnlockSys();
  return r;
}

/*********************************************************************
*
*       USBH_MEM_POOL_Free
*
*  Function description
*    Frees a memory block, putting it back into the pool.
*    The memory must have been allocated from this pool before.
*/
void USBH_MEM_POOL_Free  (MEM_POOL * pPool, void * p) {
  U32                   BlockSize;
  unsigned              BlockIndex;
  MEM_POOL_ALLOC_INFO * pInfo;
  U8                  * pFree;

  pInfo      = (MEM_POOL_ALLOC_INFO *)p;
  pInfo--;
  BlockSize  = pInfo->BlockSize;
  pFree      = (U8 *)pInfo - pInfo->Adjust;
  USBH_LOG((USBH_MTYPE_MEM, "MEM: Freeing block of %d bytes @ addr: 0x%8x", BlockSize, p));
  BlockIndex = _Size2Index(BlockSize);

#if USBH_DEBUG
  pPool->Debug.NumBytesAllocated -= BlockSize;
#endif

  USBH_OS_LockSys();
  _AddBlock(pPool, pFree, BlockIndex);
  USBH_OS_UnlockSys();
}

/*********************************************************************
*
*       USBH_TryMalloc
*
*  Function description
*    Tries to allocate a memory block.
*    Failures are permitted and do not cause panic.
*/
void * USBH_TryMalloc(U32 Size) {
  void * p;
  if (_aMemPool[0].pBaseAddr == NULL) {
    USBH_PANIC("No memory was assigned to standard memory pool");
  }
  p = USBH_MEM_POOL_Alloc(&_aMemPool[0], Size, 1);
  return p;
}

/*********************************************************************
*
*       USBH_Malloc
*
*  Function description
*    Tries to allocate a memory block.
*    Failure is NOT permitted and causes panic.
*/
void * USBH_Malloc(U32 Size) {
  void * p;
  p = USBH_TryMalloc(Size);
  if (p == NULL) {
    USBH_PANIC("No memory available");
  }
  return p;
}

/*********************************************************************
*
*       USBH_MallocZeroed
*
*  Function description
*    Allocates memory blocks.
*/
void * USBH_MallocZeroed(U32 Size) {
  void * p;
  p = USBH_Malloc(Size);
  if (p) {
    USBH_MEMSET(p, 0, Size);
  }
  return p;
}

/*********************************************************************
*
*       USBH_Free
*
*  Function description
*    Deallocates or frees a memory block.
*/
void USBH_Free(void * pMemBlock) {
  U8         * pMemBlock8;
  U8         * pMemPoolStart;
  U8         * pMemPoolEnd;
  MEM_POOL   * pPool;
  unsigned     iPool;
  pMemBlock8 = (U8 *)pMemBlock;
  //
  //  Iterate over all memory pools and check, from which pool, this memory pool was allocated.
  //
  for (iPool = 0; iPool < USBH_COUNTOF(_aMemPool); iPool++) {
    pPool               = &_aMemPool[iPool];
    pMemPoolStart       = (U8 *)pPool->pBaseAddr;
    pMemPoolEnd         = pMemPoolStart + pPool->NumBytes - 1;
    if ((pMemPoolStart <= pMemBlock8) && (pMemBlock8 < pMemPoolEnd)) {
      USBH_MEM_POOL_Free(pPool, pMemBlock);
      return;
    }
  }
  USBH_PANIC("USBH_Free()");
}

/*********************************************************************
*
*       USBH_AssignMemory
*
*  Function description
*    This function is called in the init phase.
*/
void USBH_AssignMemory(U32 * pMem, U32 NumBytes) {
  USBH_MEM_POOL_Create(&_aMemPool[0], pMem, NumBytes);
}

/*********************************************************************
*
*       USBH_AssignTransferMemory
*
*  Function description
*    This function is called in the init phase.
*/
void USBH_AssignTransferMemory(U32 * pMem, U32 NumBytes) {
  USBH_MEM_POOL_Create(&_aMemPool[1], pMem, NumBytes);
}

/*********************************************************************
*
*       USBH_AllocTransferMemory
*
*  Function description
*    Allocates a block of memory which can be used for transfers.
*    Memory should be uncached, physical addr = VA in case an MMU is present.
*/
void * USBH_AllocTransferMemory(U32 NumBytes, unsigned Alignment) {
  void     * r;
  MEM_POOL * pPool;
  //
  // Allocate memory from transfer pool. If no pool has been defined for transfer memory,
  // we assume that regular memory can be used.
  //
  if (_aMemPool[1].pBaseAddr) {
    pPool = &_aMemPool[1];
  } else {
    pPool = &_aMemPool[0];
  }
  r = USBH_MEM_POOL_Alloc(pPool, NumBytes, Alignment);
  return r;
}

/******************************* EOF ********************************/
