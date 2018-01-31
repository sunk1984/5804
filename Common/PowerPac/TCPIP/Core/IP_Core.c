/*********************************************************************
*               SEGGER MICROCONTROLLER SYSTEME GmbH                  *
*       Solutions for real time microcontroller applications         *
**********************************************************************
*                                                                    *
*       (C) 2003 - 2007   SEGGER Microcontroller Systeme GmbH        *
*                                                                    *
*       www.segger.com     Support: support@segger.com               *
*                                                                    *
**********************************************************************
*                                                                    *
*       TCP/IP stack for embedded applications                       *
*                                                                    *
**********************************************************************
----------------------------------------------------------------------
File        : IP_Core.c
Purpose     : TCP/IP system core routines
---------------------------END-OF-HEADER------------------------------
*/

/*********************************************************************
*
*       #include Section
*
**********************************************************************
*/

#include <stdlib.h>

#define IPCORE_C

#include "IP_Int.h"
#include "IP_TCP.h"
#include "IP_sockvar.h"

/*********************************************************************
*
*       #define constants
*
**********************************************************************
*/
#define MAX_TIMERS     10


/*********************************************************************
*
*       Statistic counters (used in higher debug levels only)
*
**********************************************************************
*/

int IP_SendPacketCnt;

/*********************************************************************
*
*       Local data types
*
**********************************************************************
*/

typedef struct {
  void (*pfHandler)(void);
  I32  Period;
  I32  NextTime;
} TIMER;

/*********************************************************************
*
*       Static data
*
**********************************************************************
*/

static int      _AddBufferIndex;
static int      _NumTimers;
static TIMER    _aTimer[MAX_TIMERS];

/*********************************************************************
*
*       Public data
*
**********************************************************************
*/
char IP_RxTaskStarted;
char IP_UseBPressure = 1;

/*********************************************************************
*
*       #define function replacement
*
**********************************************************************
*/


/* ET_TYPE_GET(e) - get Ethernet type from Ethernet header pointed to by char * e
 * Note returned Ethernet type is in host order!
 */
#define ET_TYPE_GET(e)  (((unsigned)(*((e) + 12)) << 8) + (*((e) + 12 + 1) & 0xff))

/*********************************************************************
*
*       Static code
*
**********************************************************************
*/

/*********************************************************************
*
*       _SetFilter
*
*  Function description
*    Set MAC Filter of driver
*/
static void  _SetFilter(void) {
  IP_NI_CMD_SET_FILTER_DATA Data;

  Data.NumAddr = 1;
  Data.pHWAddr = &IP_aIFace[0].abHWAddr[0];
  (*IP_aIFace[0].pDriver->pfControl)(0, IP_NI_CMD_SET_FILTER, &Data);
}

/*********************************************************************
*
*       _IF_Timer
*
*  Function description
*    Periodically calls the (optional) timer routines of the driver, which allow the
*    driver to check certain things such as link state periodically.
*/
static void _IF_Timer(void) {
  void (*pfTimer)(unsigned Unit);

  pfTimer = IP_aIFace[0].pDriver->pfTimer;
  if (pfTimer) {
    pfTimer(0);
  }
  //
  // Check if NI requires re-init
  //
  if (IP_aIFace[0].HasError) {
    IP_WARN((IP_MTYPE_DRIVER, "DRIVER: Re-init."));
    IP_aIFace[0].HasError = 0;
    IP_aIFace[0].pDriver->pfInit(0);
    _SetFilter();
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
*       IP_TryAlloc
*
*  Function description
*    Semi-dynamic memory allocation.
*    This fucntion is called primarily during Init.
*    by different components of the TCP/IP stack.
*    Since in a typical embedded application this process is not reversed,
*    there is no counterpart such as "Free()", which helps us to keep the
*    allocation function very simple and associated memory overhead small.
*/
void * IP_TryAlloc(U32 NumBytesReq) {
  void * p;

  IP_LOG((IP_MTYPE_ALLOC, "ALLOC: Allocating %d bytes (of %d available)", NumBytesReq, IP_Global.NumBytesRem));
  NumBytesReq = (NumBytesReq + 3) & ~3;     // Round upwards to a multiple of 4 (memory is managed in 32-bit units)
  if (NumBytesReq > IP_Global.NumBytesRem) {
    IP_WARN((IP_MTYPE_CORE, "CORE: Out of memory"));
    return NULL;                      // Out of memory. Fatal error caught in caller.
  }
  p = IP_Global.pMem;
  IP_Global.pMem = (U32*) (((U8*)IP_Global.pMem) + NumBytesReq);
  IP_Global.NumBytesRem -= NumBytesReq;
  return p;
}

/*********************************************************************
*
*       IP_Alloc
*
*  Function description
*    Semi-dynamic memory allocation.
*    This fucntion is called primarily during Init.
*    by different components of the TCP/IP stack.
*    Since in a typical embedded application this process is not reversed,
*    there is no counterpart such as "Free()", which helps us to keep the
*    allocation function very simple and associated memory overhead small.
*
*    Panics on failure to allocate memory.
*/
void * IP_Alloc(U32 NumBytesReq) {
  void * p;

  p = IP_TryAlloc(NumBytesReq);
  if (p == NULL) {
#if IP_DEBUG
    IP_PANIC("CORE: Out of memory");
#else
    IP_Panic("");    // Empty string to make sure memory usage is minimal in release build
#endif

  }
  return p;
}

/*********************************************************************
*
*       IP_AllocZeroed
*
*  Function description
*    Same as IP_Alloc; the only difference is that this function returns a pointer to 0-filled memory
*/
void * IP_AllocZeroed(U32 NumBytesReq) {
  void * p;

  p = IP_Alloc(NumBytesReq);
  IP_MEMSET(p, 0, NumBytesReq);

  return p;
}

/*********************************************************************
*
*       IP_AssignMemory
*
*  Function description
*    This function is called in the init phase.
*/
void IP_AssignMemory(U32 *pMem, U32 NumBytes) {
  IP_Global.pMem        = pMem;
  IP_Global.NumBytesRem = NumBytes;
}

/*********************************************************************
*
*       IP_ReadPackets
*
*  Function description
*    Reads all available packets from receiver and places them in the receive queue.
*    Called from Rx-ISR or from Rx Task (if a separate Rx task has been started).
*/
void IP_ReadPackets(void) {
  IP_PACKET * pPacket;
  void * pDest;
  int NumBytes;
  const IP_HW_DRIVER * pDriver;
  int SignalRequired;
  int i;

  SignalRequired = 0;
  pDriver = IP_aIFace[0].pDriver;
  do {
    NumBytes = pDriver->pfGetPacketSize(0);
    if (NumBytes <= 0) {
      break;
    }
    IP_LOG((IP_MTYPE_NET_IN, "NET: %u byte packet", NumBytes));
    pPacket = IP_PACKET_Alloc(NumBytes + ETHHDR_BIAS);
    if (pPacket) {
      pDest = pPacket->pBuffer + ETHHDR_BIAS;
      i = pDriver->pfReadPacket(0, (U8*)pDest, NumBytes);
      //
      // Check if this packet is actually valid. Some drivers detect invalid packets rather late, so this is
      // a (rather late) chance for the driver to tell us to drop the packet.
      //
      if (i != 0) {
        IP_LOG((IP_MTYPE_NET_IN, "NET_IN: Packet invalid. --- Discarded"));
        pk_free(pPacket);
        continue;     // Next packet !
      }
      //
      // Check if this packet is for us, meaning either our MAC addr or Broadcast
      // and update interface statistics
      //
      if (0xFFFF == *(U16*)(pPacket->pBuffer + ETHHDR_BIAS)) {     // first 2 bytes 0xFFFF are not assigned to any vendor, so this is sufficient
        IP_STAT_INC(IP_aIFace[0].mib.ifInNUcastPkts);
      } else {
        IP_STAT_INC(IP_aIFace[0].mib.ifInUcastPkts);
#if IP_DEBUG && (IP_SUPPORT_MULTICAST == 0)
        //
        // MAC filter: Make sure MAC address is ours.
        // This is normally done by hardware (driver responsibility), so we do not do this in a release build.
        //
        if (IP_MEMCMP(IP_aIFace[0].abHWAddr, pPacket->pBuffer + ETHHDR_BIAS, 6)) {
          IP_WARN((IP_MTYPE_NET_IN, "NET: MAC addr check failed. --- Discarded"));
          pk_free(pPacket);
          continue;     // Next packet !
        }
#endif
      }
      pPacket->pNet = &IP_aIFace[0];
      pPacket->NumBytes   = NumBytes - (ETHHDR_SIZE - ETHHDR_BIAS);
      pPacket->pData   = pPacket->pBuffer + ETHHDR_SIZE;
      IP_PACKET_Q_ADD(&IP_Global.RxPacketQ, pPacket);            // Give received pPacket to stack
      SignalRequired = 1;
    } else {
      pDriver->pfReadPacket(0, NULL, NumBytes);
    }
  } while (1);
  if (IP_UseBPressure) {
    IP_NI_ClrBPressure(0);
  }
  if (SignalRequired) {
    IP_OS_SignalNetEvent();
  }
}

/*********************************************************************
*
*       IP_NI_SetBPressure
*
*  Function description
*    TBD
*/
void IP_NI_SetBPressure(unsigned Unit) {
  IP_aIFace[0].pDriver->pfControl(Unit, IP_NI_CMD_SET_BPRESSURE, NULL);
}

/*********************************************************************
*
*       IP_NI_ClrBPressure
*
*  Function description
*    TBD
*/
void IP_NI_ClrBPressure(unsigned Unit) {
  IP_aIFace[0].pDriver->pfControl(Unit, IP_NI_CMD_CLR_BPRESSURE, NULL);
}

/*********************************************************************
*
*       IP_OnRx
*
*  Function description
*    Typically called from within ISR (Rx interrupt)
*    Depending on the task configuration, the receive task is signaled or
*    IP_ReadPackets() is called.
*/
void IP_OnRx(void) {
  if (IP_UseBPressure) {
    IP_NI_SetBPressure(0);
  }
  if (IP_RxTaskStarted == 0) {
    IP_ReadPackets();
  } else {
    IP_OS_SignalRxEvent();
  }
}

/*********************************************************************
*
*       IP__SendPacket
*
*  Function description
*    Sends a packet on the selected interface.
*
*  Return value
*    0   O.K., packet in out queue
*    1   Interface error
*/
int IP__SendPacket(IP_PACKET * pPacket) {
  int r;
  IP_STAT_INC(IP_SendPacketCnt);
  IP_PACKET_Q_ADD(&IP_Global.TxPacketQ, pPacket);    // Add it to the TxPacketQ
  r = pPacket->pNet->pDriver->pfSendPacket(0);
  return r;
}

/*********************************************************************
*
*       IP_SendPacket
*
*  Function description
*    Sends a packet on the first (and typically only) interface.
*    It does so by allocating a packet control block (IP_PACKET) and adding this to the Out Queue.
*
*  Return value
*    0   O.K., packet in out queue
*   -1   Error: Could not alloc packet control block
*    1   Error: Interface can not send
*/
int IP_SendPacket(unsigned IFace, void * pData, int NumBytes) {
  IP_PACKET * pPacket;

  pPacket = IP_PACKET_Alloc(NumBytes);
  if (!pPacket)  {
    IP_WARN((IP_MTYPE_CORE, "IP_CORE: IP_SendPacket(): Can not alloc packet"));
    return -1;     // Could not alloc packet control block
  }
  pPacket->pNet = &IP_aIFace[IFace];
  pPacket->pData   = pPacket->pBuffer;
  pPacket->NumBytes = NumBytes;
  IP_MEMCPY(pPacket->pData, pData, NumBytes);
  return IP__SendPacket(pPacket);                            // Packet placed in Out-Queue
}

/*********************************************************************
*
*       IP_GetNextOutPacket
*
*  Function description
*    Returns the first packet from the Send-Q if available.
*    Does NOT remove it from the Q.
*/
void IP_GetNextOutPacket(void ** ppData, unsigned * pNumBytes) {
  IP_PACKET * pPacket;
  pPacket = (IP_PACKET *) IP_Q_GET_FIRST(&IP_Global.TxPacketQ);
  if (pPacket) {
    *ppData    = pPacket->pData;
    *pNumBytes = pPacket->NumBytes;
  } else {
    *pNumBytes = 0;
  }
}

/*********************************************************************
*
*       IP_GetNextOutPacketFast
*
*  Function description
*    Same functionality as IP_GetNextOutPacket(), but faster because
*    PacketSize is returned as return value instead of indirectly via pointer.
*/
unsigned IP_GetNextOutPacketFast(void ** ppData) {
  IP_PACKET * pPacket;
  unsigned NumBytes;

  pPacket = (IP_PACKET *) IP_Q_GET_FIRST(&IP_Global.TxPacketQ);
  if (pPacket) {
    *ppData   = pPacket->pData;
    NumBytes  = pPacket->NumBytes;
  } else {
    NumBytes = 0;
  }
  return NumBytes;
}

/*********************************************************************
*
*       IP_RemoveOutPacket
*
*  Function description
*    Removes the first packet from the send Q.
*    Typically called from within ISR (Tx complete interrupt)
*/
void IP_RemoveOutPacket(void) {
  IP_PACKET *pPacket;
  pPacket = (IP_PACKET*)IP_PACKET_Q_GET_REMOVE_FIRST(&IP_Global.TxPacketQ);
  if (pPacket) {
    pk_free(pPacket);
  } else {
    IP_WARN((IP_MTYPE_DRIVER, "DRIVER: Can not remove packet from empty Send Q"));
  }
}

/*********************************************************************
*
*       Helper functions
*
**********************************************************************
*/


/*********************************************************************
*
*       IP_BringInBounds
*/
I32 IP_BringInBounds(I32 v, I32 Min, I32 Max) {
  if (v < Min) {
    return Min;
  }
  if (v > Max) {
    return Max;
  }
  return v;
}

/*********************************************************************
*
*       IP_LoadU32LE
*/
U32 IP_LoadU32LE(const U8 * pData) {
  U32 r;
  r  = *pData++;
  r |= *pData++ << 8;
  r |= (U32)*pData++ << 16;
  r |= (U32)*pData   << 24;
  return r;
}

/*********************************************************************
*
*       IP_LoadU32BE
*/
U32 IP_LoadU32BE(const U8 * pData) {
  U32 r;
  r = *pData++;
  r = (r << 8) | *pData++;
  r = (r << 8) | *pData++;
  r = (r << 8) | *pData;
  return r;
}

/*********************************************************************
*
*       IP_LoadU32TE
*/
U32 IP_LoadU32TE(const U8 * pData ) {
  U32  v;
  U8 * p2 =  (U8 *)&v;

  *p2++ = *pData++;
  *p2++ = *pData++;
  *p2++ = *pData++;
  *p2++ = *pData++;
  return v;
}

/*********************************************************************
*
*       IP_LoadU16BE
*/
U32 IP_LoadU16BE(const U8 * pData) {
  U32 r;
  r = *pData++;
  r = (r << 8) | *pData;
  return r;
}

/*********************************************************************
*
*       IP_LoadU16LE
*/
U32 IP_LoadU16LE(const U8 * pData) {
  U32 r;
  r  = *pData++;
  r |= (*pData++ << 8);
  return r;
}

/*********************************************************************
*
*       IP_StoreU32BE
*/
void IP_StoreU32BE(U8 * p, U32 v) {
  *p       = (U8)((v >> 24) & 255);
  *(p + 1) = (U8)((v >> 16) & 255);
  *(p + 2) = (U8)((v >> 8) & 255);
  *(p + 3) = (U8)( v       & 255);
}

/*********************************************************************
*
*       IP_StoreU32LE
*/
void IP_StoreU32LE(U8 * p, U32 v) {
  *p++ = (U8)v;
  v >>= 8;
  *p++ = (U8)v;
  v >>= 8;
  *p++ = (U8)v;
  v >>= 8;
  *p = (U8)v;
}

/*********************************************************************
*
*       IP_SwapU32
*/
U32 IP_SwapU32(U32 v) {
  U32 r;
  r   = ((v << 0)  >> 24) << 0;
  r  |= ((v << 8)  >> 24) << 8;
  r  |= ((v << 16) >> 24) << 16;
  r  |= ((v << 24) >> 24) << 24;
  return r;
}

/*********************************************************************
*
*       IP_tolower
*/
char IP_tolower(char c) {
  if ((c >= 'A') && (c <= 'Z')) {
    c += 32;
  }
  return c;
}

/*********************************************************************
*
*       IP_isalpha
*/
char IP_isalpha(char c) {
  c = IP_tolower(c);
  if ((c >= 'a') && (c <= 'z')) {
    return 1;
  }
  return 0;
}

/*********************************************************************
*
*       IP_isalnum
*/
char IP_isalnum(char c) {
  if (IP_isalpha(c)) {
    return 1;
  }
  if ((c >= '0') && (c <= '9')) {
    return 1;
  }
  return 0;
}

/*********************************************************************
*
*       IP_AddEtherInterface
*/
void IP_AddEtherInterface(const IP_HW_DRIVER *pDriver) {
  IP_aIFace[0].pDriver = pDriver;
  IP_aIFace[0].ifType = ETHERNET;
  IP_AddTimer(IP_ARP_Timer, 1000);
  IP_AddTimer(_IF_Timer, 1000);
}

/*********************************************************************
*
*       IP_AddTimer
*
*  Function description
*    Adds a timer to the list of timer.
*    This timer function will be called regularly.
*/
void IP_AddTimer(void (*pfHandler)(void), int Period) {
  int i;
  //
  // Check if this timer is already in list
  //
  for (i = 0; i < _NumTimers; i++) {
    if (pfHandler == _aTimer[i].pfHandler) {
      return;
    }
  }
  //
  // Optional debug check
  //
  if (_NumTimers >= COUNTOF(_aTimer)) {
    IP_PANIC("Too many timers");
  }
  //
  // O.K., Add it to the list
  //
  _aTimer[i].pfHandler = pfHandler;
  _aTimer[i].Period    = Period;
  _NumTimers++;
}

/*********************************************************************
*
*       _HandleTimers
*
*  Function description
*    This handles all TCP/IP related timeouts. Ideally this should be
*    called about 10 times a second; and no less than twice a second
*    (The minimum for TCP timeouts). Does NOT handle most
*    application timeouts.
*/
static void _HandleTimers(void) {
  I32 t;
  I32 t0;
  I32 Diff;
  TIMER * pTimer;
  int i;
  void (*pfHandler)(void);

  for (i = 0; i < _NumTimers; i++) {
    pTimer = &_aTimer[i];
    pfHandler = pTimer->pfHandler;
    if (pfHandler) {
      t0 = IP_OS_GetTime32();
      Diff = pTimer->NextTime - t0;
      if (Diff < 0) {
        pfHandler();
        t = pTimer->NextTime + pTimer->Period;
        if ((t - t0) <= 0) {
          t = t0 + pTimer->Period;
        }
        pTimer->NextTime = t;
      }
    }
  }
}

/*********************************************************************
*
*       _GetTimeout
*
*  Function description
*/
static int _GetTimeout(void) {
  I32 t;
  I32 t0;
  I32 Diff;
  I32 Timeout;
  TIMER * pTimer;
  int     i;

  t0 = IP_OS_GetTime32();
  Timeout = t0 + 2000;  // Safe value. In real life, we wake up at least once per second.
  for (i = 0; i < _NumTimers; i++) {
    pTimer = &_aTimer[i];
    if (pTimer->pfHandler) {
      t = pTimer->NextTime;
      if ((t - Timeout) < 0) {
        Timeout = t;
      }
    }
  }
  Diff = Timeout - t0;
  if (Diff <= 0) {
    return 10;                      // Already expired. Means we have a hard time keeping up. Delay at least a bit to avoid using too much time with housekeeping.
  }
  return Diff;
}

/*********************************************************************
*
*       _DispatchIPPacket()
*
*  Function description
*    Handles a received packet by dispatching it to the appropriate module,
*    either IP or ARP
*/
static void _DispatchIPPacket(IP_PACKET * pPacket) {
  int r;
  unsigned Type;

  ASSERT_LOCK();
  Type = *(U16*)(pPacket->pBuffer + 14);
  //
  // Call hook function to give application a chance to intercept the packet
  //
  if (IP_Global.pfOnRx) {
    r = (IP_Global.pfOnRx)(pPacket);
    if (r) {
      pk_free(pPacket);
      return;
    }
  }
  //
  // Dispatch to either ARP or IP dispatcher, depending on packet type
  //
  if (Type == IP_TYPE_IP) {            // IP type?
    IP_IP_OnRx(pPacket);
  } else if (Type == IP_TYPE_ARP) {    // ARP type?
    IP_ARP_OnRx(pPacket);
  } else {
    IP_WARN((IP_MTYPE_NET_IN, "NET: Unknown Packet type %d --- Discarded", Type));
    IP_STAT_INC(pPacket->pNet->mib.ifInUnknownProtos);
    pk_free(pPacket);           /* return to free buffer */
  }
}

/*********************************************************************
*
*       _DemuxInPackets()
*
*  Function description
*    The received packet demuxing task. We try to run this
*    whenever there are unprocessed packets in IP_Global.RxPacketQ. It dequeues the
*    packet and sends it to a protocol stack based on type.
*
*/
static void _DemuxInPackets(void) {
  IP_PACKET *   pPacket;

  while (IP_Global.RxPacketQ.q_len) {
    pPacket = (IP_PACKET *)IP_PACKET_Q_GET_REMOVE_FIRST(&IP_Global.RxPacketQ);
    _DispatchIPPacket(pPacket);
  }
}

/*********************************************************************
*
*       IP_SetHWAddr
*
*  Function description
*    Sets the hardware address (MAC) of an interface.
*/
void  IP_SetHWAddr(const U8 * pHWAddr) {
  IP_MEMCPY(IP_aIFace[0].abHWAddr, pHWAddr, 6);
}

/*********************************************************************
*
*       IP_GetHWAddr
*
*  Function description
*    Gets the hardware address (MAC) of an interface from the driver
*
*  Note:
*    As long as we not support multiple interfaces, IFace will not be
*    used as index for IP_aIFace.
*/
void  IP_GetHWAddr(U8 IFace, U8 * pDest, unsigned Len) {
  if (Len > 6) {
    Len = 6;
  }
  IP_MEMCPY(pDest, IP_aIFace[0].abHWAddr, Len);
}

/*********************************************************************
*
*       IP_SetAddrMask
*
*  Function description
*    Sets the IP address and subnet mask of an interface.
*/
void IP_SetAddrMask(U32 Addr, U32 Mask) {
  Addr = htonl(Addr);
  Mask = htonl(Mask);
  IP_aIFace[0].n_ipaddr   = Addr;
  IP_aIFace[0].snmask     = Mask;
}

/*********************************************************************
*
*       IP_GetAddrMask
*
*  Function description
*    Gets the IP address and subnet mask of an interface.
*    The values are stored in target order.
*    Target order means: If *pAddr points to a variable Addr and the IP Addr is 192.168.0.1,
*    the value of the variable will be 0xc0a80001 after the call.
*/
void IP_GetAddrMask(U8 IFace, U32 * pAddr, U32 * pMask) {
  if (pAddr) {
    *pAddr = htonl(IP_aIFace[IFace].n_ipaddr);
  }
  if (pMask) {
    *pMask = htonl(IP_aIFace[IFace].snmask);
  }
}

/*********************************************************************
*
*       IP_SetGWAddr
*
*  Function description
*    Sets the default gateway address of the interface.
*
*  Note:
*    As long as we not support multiple interfaces, IFace will not be
*    used as index for IP_aIFace.
*/
void IP_SetGWAddr(U8 IFace, U32 GWAddr) {
  GWAddr = htonl(GWAddr);
  IP_aIFace[0].n_defgw = GWAddr;
}

/*********************************************************************
*
*       IP_GetGWAddr
*
*  Function description
*    Gets the default gateway address of the interface.
*
*  Note:
*    As long as we not support multiple interfaces, IFace will not be
*    used as index for IP_aIFace.
*/
U32 IP_GetGWAddr(U8 IFace) {
  return htonl(IP_aIFace[0].n_defgw);
}

/*********************************************************************
*
*       IP_GetIPAddr
*
*  Function description
*    Returns the IP address of the interface in network endianess (Big).
*    Example:
*      192.168.0.1   is returned as 0xc0a80001
*
*  Note:
*    As long as we not support multiple interfaces, IFace will not be
*    used as index for IP_aIFace.
*/
U32 IP_GetIPAddr(U8 IFace) {
  return htonl(IP_aIFace[0].n_ipaddr);
}

/*********************************************************************
*
*       IP_Exec
*
*  Function description
*    Main thread for starting the net. After startup, it settles into
*    a loop handling received packets. This loop sleeps until a packet
*    has been queued in IP_Global.RxPacketQ; at which time it should be awakend by the
*    driver which queued the packet.
*/
void IP_Exec(void) {
  IP_OS_Lock();
  /* see if there's newly received network packets */
  _DemuxInPackets();
  _HandleTimers();  /* let various timeouts occur */
  IP_OS_Unlock();
}


/*********************************************************************
*
*       IP_Task
*
*  Function description
*    Main thread for starting the net. After startup, it settles into
*    a loop handling received packets. This loop sleeps until a packet
*    has been queued in IP_Global.RxPacketQ; at which time it should be awakend by the
*    driver which queued the packet.
*/
void IP_Task(void) {
  int Timeout;
  IP_LOG((IP_MTYPE_INIT, "INIT: IP_Task started"));
  while (1) {
    Timeout = _GetTimeout();   // TBD: Should be calculated based on timers
    IP_OS_WaitNetEvent(Timeout);
    IP_Exec();
  }
}

/*********************************************************************
*
*       IP_RxTask
*
*  Function description
*    Task routine. Reads all available packets from receiver.
*    Sleeps until a new packet is received.
*    Note: This task is optional.
*/
void IP_RxTask(void) {
  IP_RxTaskStarted = 1;
  IP_LOG((IP_MTYPE_INIT, "INIT: IP_RxTask started"));
  while (1) {
    IP_OS_WaitRxEvent();
    IP_ReadPackets();
  }
}


/*********************************************************************
*
*       IP_cksum
*
*  Function description
*    Compute Internet Checksum as defined in rfc1071
*    This routine has been written from scratch.
*    The code suggested in RFC1071 is way less efficient since it does not use
*    loop unrolling and does not use 32-bit reads.
*    Interestingly, the 32-bit optimization also provides the best possible performance
*    doable in "C" even on typical 16-bit CPUs.
*
*  Notes
*    (1) Adding 32 bits at a time
*        Since the 32-bits are folded to 16-bits anyhow, the addition can also be done in units
*        of 32 bits. This speeds up the code by app.factor 2 on 32-bit CPUs.
*    (2) Further speed up
*        This routine can only be accelerated in assembly language.
*        For ARM CPUs, an assembly version is available. The advantage of the assembly code
*        is that it can use a LDMIA (load multiple) to read multiple words with a single instruction.
*        The optimized assembly version is about 20% faster on ARM cores.
*/
IP_OPTIMIZE
unsigned IP_cksum(void * ptr, unsigned NumHWords) {
  U16 * pData;
  U32 Sum;
  U16 Data;

  pData = (U16 *)ptr;
  Sum  = 0;
  //
  // Fast loop, 8 half words at a time
  //
  if (NumHWords >= 8) {
    do {
      Sum += *pData++;
      Sum += *pData++;
      Sum += *pData++;
      Sum += *pData++;
      Sum += *pData++;
      Sum += *pData++;
      Sum += *pData++;
      Sum += *pData++;
      NumHWords -= 8;
    } while (NumHWords >= 8);
  }
  //
  // Last items
  //
  while (NumHWords--) {
    Data = *pData++;
    Sum += Data;
  }
  //
  //  Fold 32-bit sum to 16 bits
  //
  do {
    Sum = ((U16)Sum) + (Sum >> 16);
  } while (Sum >> 16);
  return (unsigned)Sum;
}

/*********************************************************************
*
*       IP_CalcChecksum_Byte
*
*  Function description
*    Compute the internet checksum on an array of bytes.
*
*  Return value
*    The return value is purposly an unsigned, not a U16.
*    This generates more efficient code on most 32 bit CPUs.
*/
unsigned IP_CalcChecksum_Byte(const void * pData, unsigned NumBytes) {
  U32 Sum;
  U16 v;

  Sum = IP_CKSUM((U16*)pData, NumBytes >> 1);
  //
  // For an odd number of bytes, take the last byte into account now
  //
  if (NumBytes & 1) {
    v = *(((U8*)pData) + NumBytes - 1); // Load last byte
#if IP_IS_BIG_ENDIAN
  v <<= 8;
#endif

    Sum += v;
    //
    //  Fold 32-bit sum to 16 bits
    //
    Sum = ((U16)Sum) + (Sum >> 16);
  }
  return (unsigned)Sum;
}

/*********************************************************************
*
*       IP_AddBuffers
*
*  Function description
*    Add buffers. This is a configuration function, typically called from IP_X_Prepare().
*    It needs to be called 2 times, one per buffer size.
*
*  Additional information
*    Typical usage is as follows:
*      IP_AddBuffers(20, 200);      // Small buffers.
*      IP_AddBuffers(12, 1536);     // Big buffers. Size should be 1536 to allow a full ether packet to fit.
*/
void IP_AddBuffers(int NumBuffers, int BytesPerBuffer) {
  int i;
  i = _AddBufferIndex++;
  if (i >= COUNTOF(IP_Global.aBufferConfigNum)) {
    IP_PANIC("IP_AddBuffers() called too many times.");
  }
  IP_Global.aBufferConfigSize[i] = BytesPerBuffer;
  IP_Global.aBufferConfigNum [i] = NumBuffers;
}

/*********************************************************************
*
*       IP_ConfTCPSpace
*
*  Function description
*    Configure TCP/IP send and receive space.
*/
void IP_ConfTCPSpace(unsigned SendSpace, unsigned RecvSpace) {
  IP_Global.TCP_TxWindowSize = SendSpace;
  IP_Global.TCP_RxWindowSize = RecvSpace;
}

/*********************************************************************
*
*       IP_SetCurrentLinkState
*
*  Function description
*    Called from driver whenever the link state needs to be updated.
*/
int IP_SetCurrentLinkState(U32 Duplex, U32 Speed) {
  if ((Duplex != IP_LinkDuplex) || (Speed != IP_LinkSpeed)) {
    IP_LinkDuplex = Duplex;
    IP_LinkSpeed  = Speed;
    IP_LOG((IP_MTYPE_LINK_CHANGE, "LINK: Link state changed: %s, %s"
            ,(Duplex == 0) ? "No Link" : (Duplex == 1) ? "Half duplex" : "Full duplex"
            ,(Speed  == 0) ? "" : (Speed == 1) ? "10 MHz" : "100 MHz"));
    return 1;    // Linkstate has changed
  }
  return 0;    // Linkstate unchanged
}

/*********************************************************************
*
*       IP_GetCurrentLinkSpeed
*
*  Function description
*    Returns the current link speed, which is periodically updated.
*/
int IP_GetCurrentLinkSpeed(void) {
  return IP_LinkSpeed;
}

/*********************************************************************
*
*       IP_IFaceIsReady
*
*  Function description
*    Returns if the interface is ready to send and receive IP packets.
*    In general, this means:
*    - Link is up
*    - Interface is configured
*/
int IP_IFaceIsReady(void) {
  if (IP_LinkSpeed && IP_aIFace[0].n_ipaddr) {
    return 1;   // Ready !
  }
  return 0;     // Not ready
}



/*********************************************************************
*
*       IP_IsExpired()
*
*  Notes
*    (1) How to compare
*        Important: We need to check the sign of the difference.
*        Simple comparison will fail when 0x7fffffff and 0x80000000 is compared.
*/
char IP_IsExpired(I32 Time) {
  I32 t;
  t = IP_OS_GetTime32();
  if ((t - Time) >= 0) {     // (Note 1)
    return 1;
  }
  return 0;
}

/*********************************************************************
*
*       IP_AllocWithQ
*
*  Function description
*    Allocate storage space.
*    It is either fetched from the free queue or allocated.
*    "Panics" if no memory is available.
*    Memory returned is initialized to 0
*/
void * IP_AllocWithQ(QUEUE * pQueue, int NumBytes) {
  void * p;

  p = IP_Q_TryGetRemoveFirst(pQueue);
  if (p) {
    IP_MEMSET(p, 0, NumBytes);
  } else {
    p = IP_AllocZeroed(NumBytes);
  }
  return p;
}

/*********************************************************************
*
*       IP_TryAllocWithQ
*
*  Function description
*    Allocate storage space.
*    It is either fetched from the free queue or allocated.
*    Memory returned is initialized to 0
*/
void * IP_TryAllocWithQ(QUEUE * pQueue, int NumBytes) {
  void * p;
  p = IP_Q_TryGetRemoveFirst(pQueue);
  if (p == NULL) {
    p = IP_TryAlloc(NumBytes);
  }
  if (p) {
    IP_MEMSET(p, 0, NumBytes);
  }
  return p;
}

/*********************************************************************
*
*       IP_GetVersion
*
*  Function description
*    Returns the version of the stack.
*    Format: Mmmrr. Sample 10201 is 1.02a
*/
int  IP_GetVersion    (void) {
  return IP_VERSION;
}

/*********************************************************************
*
*       IP_SetDefaultTTL
*
*  Function description
*    Sets the default TTL (time to life).
*    TTL is decremented typically by one on every hub.
*/
void IP_SetDefaultTTL(int v) {
  IP_Global.TTL       = v;
  ip_mib.ipDefaultTTL = v;
}

int if_netnumber(NET * nptr) {
  return 0;
}

/********************************************************************
*
*       ip_mymach
*
* Function description
*   Returns the address of our machine relative to a certain foreign host.
*
* Parameters
*   host: ip_addr host
*
* Return value
*   The address of our machine relative to a certain foreign host.
*/
ip_addr ip_mymach(ip_addr host) {
   return IP_aIFace[0].n_ipaddr;
}

/* FUNCTION: iproute()
 *
 * iproute() - Route a packet. Takes the internet address that we
 * want to send a packet to and returns the net interface to send it
 * through.
 *
 *
 * PARAM1: ip_addr host
 * PARAM2: ip_addr * hop1
 *
 * RETURNS: Returns NULL when unable to route, else returns a NET pointer.
 */

NET * iproute(ip_addr host, ip_addr * hop1) {
  return &IP_aIFace[0];
}

/*********************************************************************
*
*       IP_Init
*
*/
void IP_Init(void) {
  IP_LOG((IP_MTYPE_INIT, "INIT: Init started. Version %d.%2d.%2d", IP_VERSION / 10000, (IP_VERSION / 100) % 100, IP_VERSION % 100));

#if IP_DEBUG > 0
  if (IP_GetVersion() != IP_VERSION) {
    IP_PANIC("Version stamps in Code and header do not match");
  }
#endif

#if IP_DEBUG > 0
  ip_mib.ipForwarding = 2;   /* default to host, not gateway (router) */
  ip_mib.ipDefaultTTL = IP_TTL;
#endif
  IP_Global.TTL      = IP_TTL;
  IP_Global.PacketId = 1;
  //
  // Set default values for Interface(s)
  IP_aIFace[0].n_mtu = 1500;         // Max. IP size is 1500 bytes:   1518 total - 2 * 6 MAC - 2 Type - 4 CRC
  IP_aIFace[0].n_lnh = ETHHDR_SIZE;
  IP_aIFace[0].ifAdminStatus = NI_UP;
  IP_aIFace[0].n_hal = 6;

  IP_OS_Init();
  IP_X_Config();
#if IP_DEBUG > 0
  IP_Global.ConfigCompleted = 1;
#endif


 /* set ifAdminStatus in case init() routine forgot to. IfOperStatus
  * is not nessecarily up at this point, as in the case of a modem which
  * is now in autoanswer mode.
  */

  /* build broadcast addresses */
  if(IP_aIFace[0].n_ipaddr != 0) {
     IP_aIFace[0].n_netbr    = IP_aIFace[0].n_ipaddr | ~IP_aIFace[0].snmask;
  }

  pk_init();
  IP_MBUF_Init();
  tcp_init();    /* call the BSD init in tcp_usr.c */
  IP_AddTimer(IP_TCP_SlowTimer, IP_TCP_SLOW_PERIOD);
#ifdef DO_DELAY_ACKS
  IP_AddTimer(IP_TCP_FastTimer, IP_TCP_DACK_PERIOD);
#endif
  //
  // Init interfaces
  //
  (*IP_aIFace[0].pDriver->pfInit)(0);
  //
  // If hardware addr has not been set in config, try to load it from hardware (typically the EEPROM)
  //
  if (IP_LoadU32LE(IP_aIFace[0].abHWAddr) == 0) {
    IP_NI_LoadHWAddr(0);
  }
  //
  // Get capabilities. This is done here so we do not have to query the driver every time we need them.
  //
  IP_aIFace[0].Caps = IP_NI_GetCaps(0);
  //
  // If interface is ethernet, set bcast flag bit. This should really be done by the init routine, but we handle it
  // here to support MAC drivers which predate the flags field.
  //
  if (IP_aIFace[0].ifType == ETHERNET) {
    IP_aIFace[0].n_flags |= NF_BCAST;

#if IP_SUPPORT_MULTICAST == 0
    //
    // Check that chosen MAC addr has bit 0 of first byte == 0. This is a must since it destinguishes between
    // Multicast (b0 == 1) and Unicast MAC addresses.
    //
    if (IP_aIFace[0].abHWAddr[0] & 1)  {
      IP_PANIC("Multicast Ethernet addr. not allowed.");
    }
#endif
    _SetFilter();
  }
  if (IP_LinkSpeed == 0) {
    IP_LOG((IP_MTYPE_INIT, "INIT: Link is down"));
  }
  IP_Global.InitCompleted = 1;
  IP_LOG((IP_MTYPE_INIT, "INIT: Init completed"));
}

/*********************************************************************
*
*       IP_AllowBackPressure
*
*/
void IP_AllowBackPressure(char v) {
  IP_UseBPressure = v;
}

/*********************************************************************
*
*       IP_NI_ConfigPHYMode
*
*  Function description
*    Configure PHY Mode: 0: MII, 1: RMII
*/
void IP_NI_ConfigPHYMode (unsigned Unit, U8 Mode) {
  if (IP_Global.ConfigCompleted) {
    IP_WARN((IP_MTYPE_INIT, "INIT: Config function called too late"));
  }
  IP_aIFace[0].pDriver->pfControl(Unit, IP_NI_CMD_SET_PHY_MODE, (void*)(int)Mode );
}

/*********************************************************************
*
*       IP_NI_ConfigPHYAddr
*
*  Function description
*    Configure PHY Addr (5-bit)
*/
void IP_NI_ConfigPHYAddr (unsigned Unit, U8 Addr) {
  if (IP_Global.ConfigCompleted) {
    IP_WARN((IP_MTYPE_INIT, "INIT: Config function called too late"));
  }
  IP_aIFace[0].pDriver->pfControl(Unit, IP_NI_CMD_SET_PHY_ADDR, (void*)(int)Addr);
}

/*********************************************************************
*
*       IP_NI_SetError
*
*  Function description
*    Sets the error flag of a network interface.
*/
void IP_NI_SetError(unsigned Unit) {
  IP_aIFace[Unit].HasError = 1;
}

/*********************************************************************
*
*       _PollISR
*/
static void _PollISR(void) {
  if (IP_Global.InitCompleted) {
    IP_aIFace[0].pDriver->pfControl(0, IP_NI_CMD_POLL, (void*)0);
  }
}

/*********************************************************************
*
*       IP_NI_ConfigPoll
*
*  Function description
*    Select polled mode for the network interface.
*    This should be used only if the NI can not activate an ISR itself.
*/
void IP_NI_ConfigPoll(unsigned Unit) {
  if (IP_Global.ConfigCompleted) {
    IP_WARN((IP_MTYPE_INIT, "INIT: Config function called too late"));
    return;
  }
  IP_aIFace[0].pDriver->pfControl(0, IP_NI_CMD_CFG_POLL, (void*)0);
  IP_OS_AddTickHook(_PollISR);
}

/*********************************************************************
*
*       IP_NI_LoadHWAddr
*
*  Function description
*    Gets the hardware address (MAC) from the driver.
*    This can be usefull if the hardware has loaded the MAC addr from an EEPROM.
*/
int IP_NI_LoadHWAddr(unsigned Unit) {
  return IP_aIFace[0].pDriver->pfControl(Unit, IP_NI_CMD_GET_MAC_ADDR, IP_aIFace[0].abHWAddr);
}

/*********************************************************************
*
*       IP_NI_GetCaps
*
*  Function description
*    Gets the capabilities from the driver.
*    This can be usefull to find out which hardware optimizations to support.
*/
int IP_NI_GetCaps(unsigned Unit) {
  int r;
  r = IP_aIFace[0].pDriver->pfControl(Unit, IP_NI_CMD_GET_CAPS, (void*)0);
  if (r == -1) {
    r = 0;
  }
  return r;
}

/*********************************************************************
*
*       IP_NI_SetTxBufferSize
*
*  Function description
*    Sets the size of the Tx buffer of the network interface. This
*    can be useful on targets with less RAM and a small MTU. Default
*    Tx buffer size is 1536 bytes. The size of the Tx buffer should
*    be at least MTU + 16 bytes.
*    If the Tx buffer size should be modified, the function should be
*    called in IP_X_Config().
*
*  Note:
*    This function is not supported from all drivers.
*
*  Return value:
*    -1 - Error, not supported
*     0 - OK
*     1 - Error, called after driver initialization has been completed.
*/
int IP_NI_SetTxBufferSize(unsigned Unit, unsigned NumBytes) {
  int r;
  r = IP_aIFace[0].pDriver->pfControl(Unit, IP_NI_CMD_SET_TX_BUFFER_SIZE, (void*)NumBytes);
  return r;
}

/*********************************************************************
*
*       IP_SetReceivePacketHook
*
*  Function description
*    Allows to set a hook function that handles received packets.
*/
void IP_SetRxHook(IP_RX_HOOK * pfRxHook) {
  IP_Global.pfOnRx = pfRxHook;
}

/*********************************************************************
*
*       IP_GetIPPacketInfo
*
*  Function description
*    Returns the start address of the data part of an IP_PACKET.
*
*  Return value
*    >0 - Start address of the data
*    0  - On failure
*/
const char * IP_GetIPPacketInfo(IP_PACKET * pPacket) {
  struct ip * pIPHeader;
  U32 HeaderLen;

  if (pPacket) {
    pIPHeader = (struct ip *)pPacket->pData;
    HeaderLen = (pIPHeader->ip_ver_ihl  &  0x0f) << 2;
    return (const char *)pIPHeader + HeaderLen;
  }
  return NULL;
}

/*********************************************************************
*
*       IP_SetMTU
*
*  Function description
*    Allows to set the maximum transmission unit (MTU) of an interface.
*    The MTU is the MTU from an IP standpoint, so the size of the IP-packet without local net header.
*    A typical value for ethernet is 1500, since the maximum size of an Ethernet packet is
*    1518 bytes. Since Ethernet uses 12 bytes for MAC addresses, 2 bytes for type and 4 bytes for CRC,
*    1500 bytes "payload" remain.
*/
void IP_SetMTU(U8 IFace, U32 Mtu) {
  IP_aIFace[0].n_mtu = Mtu;
}

/*********************************************************************
*
*       IP_SetSupportedDuplexModes
*
*  Function description
*    Allows to set the supported duplex mode of the device.
*
*  Supported values for DuplexMode (OR-combination):
*    IP_PHY_MODE_10_HALF
*    IP_PHY_MODE_10_FULL
*    IP_PHY_MODE_100_HALF
*    IP_PHY_MODE_100_FULL
*/
int IP_SetSupportedDuplexModes(unsigned Unit, unsigned DuplexMode) {
  int r;
  r = IP_aIFace[0].pDriver->pfControl(Unit, IP_NI_CMD_SET_SUPPORTED_DUPLEX_MODES, (void*)DuplexMode);
  return r;
}

/*************************** End of file ****************************/
