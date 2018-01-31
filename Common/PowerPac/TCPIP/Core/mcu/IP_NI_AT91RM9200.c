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
File    : IP_NI_AT91RM9200.c
Purpose : NI driver for AT91RM09200
--------  END-OF-HEADER  ---------------------------------------------
*/

#include  "IP_Int.h"
#include  "BSP.h"
#include  "IP_NI_AT91RM9200.h"

/*********************************************************************
*
*       Defines, configurable
*
**********************************************************************
*/
#define NUM_RX_BUFFERS      (4)              // Number of available buffers.
                                             // Receive buffers are word-aligned and are capable of containing

#define _RX_BUFFER_SIZE     (1536)           // Maximum length of an Ethernet frame:  1518 or 1522 bytes (BIG = 1 in ETH_CFG) of data

/*********************************************************************
*
*       Defines, non configurable
*
**********************************************************************
*/

/*********************************************************************
*
*       AT91RM9200
*/
//
// Register definition for EMAC peripheral
//
#define EMAC_CTL                    (*(volatile U32*) 0xFFFBC000)  // (EMAC) Network Control Register
#define EMAC_NCFG                   (*(volatile U32*) 0xFFFBC004)  // (EMAC) Network Configuration Register
#define EMAC_NSR                    (*(volatile U32*) 0xFFFBC008)  // (EMAC) Network Status Register
#define EMAC_TAR                    (*(volatile U32*) 0xFFFBC00C)  // (EMAC) Transmit Address Register
#define EMAC_TCR                    (*(volatile U32*) 0xFFFBC010)  // (EMAC) Transmit Control Register
#define EMAC_TSR                    (*(volatile U32*) 0xFFFBC014)  // (EMAC) Transmit Status Register
#define EMAC_RBQP                   (*(volatile U32*) 0xFFFBC018)  // (EMAC) Receive Buffer Queue Pointer
#define EMAC_RSR                    (*(volatile U32*) 0xFFFBC020)  // (EMAC) Receive Status Register
#define EMAC_ISR                    (*(volatile U32*) 0xFFFBC024)  // (EMAC) Interrupt Status Register
#define EMAC_IER                    (*(volatile U32*) 0xFFFBC028)  // (EMAC) Interrupt Enable Register
#define EMAC_IDR                    (*(volatile U32*) 0xFFFBC02C)  // (EMAC) Interrupt Disable Register
#define EMAC_IMR                    (*(volatile U32*) 0xFFFBC030)  // (EMAC) Interrupt Mask Register
#define EMAC_MAN                    (*(volatile U32*) 0xFFFBC034)  // (EMAC) PHY Maintenance Register
#define EMAC_HRB                    (*(volatile U32*) 0xFFFBC090)  // (EMAC) Hash Address Bottom[31:0]
#define EMAC_HRT                    (*(volatile U32*) 0xFFFBC094)  // (EMAC) Hash Address Top[63:32]
#define EMAC_SA1L                   (*(volatile U32*) 0xFFFBC098)  // (EMAC) Specific Address 1 Bottom, First 4 bytes
//
// EMAC_NCR: (EMAC Offset: 0x0)
//
#define EMAC_RE                     ((unsigned int) 0x1 <<  2)     // (EMAC) Receive enable.
#define EMAC_TE                     ((unsigned int) 0x1 <<  3)     // (EMAC) Transmit enable.
#define EMAC_BPRESSURE              ((unsigned int) 0x1 <<  8)     // (EMAC) back pressure
#define EMAC_TSTART                 ((unsigned int) 0x1 <<  9)     // (EMAC) Start Transmission.
//
// EMAC_NCFG: (EMAC Offset: 0x4) Network Configuration Register
//
#define EMAC_SPD                    ((unsigned int) 0x1 <<  0)     // (EMAC) Speed.
#define EMAC_FD                     ((unsigned int) 0x1 <<  1)     // (EMAC) Full duplex.
#define EMAC_CAF                    ((unsigned int) 0x1 <<  4)     // (EMAC) Copy all frames.
#define EMAC_NBC                    ((unsigned int) 0x1 <<  5)     // (EMAC) No broadcast.
#define EMAC_MTI                    ((unsigned int) 0x1 <<  6)     // (EMAC) Multicast hash event enable
#define EMAC_UNI                    ((unsigned int) 0x1 <<  7)     // (EMAC) Unicast hash enable.
#define EMAC_BIG                    ((unsigned int) 0x1 <<  8)     // (EMAC) Receive 1522 bytes.
#define EMAC_EAE                    ((unsigned int) 0x1 <<  9)     // (EMAC) External address match enable.
#define EMAC_CLK                    ((unsigned int) 0x3 << 10)     // (EMAC)
#define EMAC_RTY                    ((unsigned int) 0x1 << 12)     // (EMAC)
#define EMAC_PAE                    ((unsigned int) 0x1 << 13)     // (EMAC)
#define EMAC_RBOF                   ((unsigned int) 0x3 << 14)     // (EMAC)
#define EMAC_RLCE                   ((unsigned int) 0x1 << 16)     // (EMAC) Receive Length field Checking Enable
#define EMAC_DRFCS                  ((unsigned int) 0x1 << 17)     // (EMAC) Discard Receive FCS
#define EMAC_EFRHD                  ((unsigned int) 0x1 << 18)     // (EMAC)
#define EMAC_IRXFCS                 ((unsigned int) 0x1 << 19)     // (EMAC) Ignore RX FCS
//
// EMAC_NSR: (EMAC Offset: 0x8) Network Status Register
//
#define EMAC_LINKR                  ((unsigned int) 0x1 <<  0)     // (EMAC)
#define EMAC_MDIO                   ((unsigned int) 0x1 <<  1)     // (EMAC)
#define EMAC_IDLE                   ((unsigned int) 0x1 <<  2)     // (EMAC)
//
// EMAC_TSR: (EMAC Offset: 0x14) Transmit Status Register
//
#define EMAC_TXOVR                  ((unsigned int) 0x1 <<  0)     // (EMAC) OVR:  Ethernet Transmit Buffer Overrun
#define EMAC_COL                    ((unsigned int) 0x1 <<  1)     // (EMAC) COL:  Collision Occured
#define EMAC_RLE                    ((unsigned int) 0x1 <<  2)     // (EMAC) RLE:  Retry Limit Exceeded
#define EMAC_TXIDLE                 ((unsigned int) 0x1 <<  3)     // (EMAC) IDLE: Transmitter idle
#define EMAC_BNQ                    ((unsigned int) 0x1 <<  4)     // (EMAC) BNQ:  Ethernet Transmit Buffer not Queued
#define EMAC_COMP                   ((unsigned int) 0x1 <<  5)     // (EMAC) COMP: Transmit Complete
#define EMAC_UND                    ((unsigned int) 0x1 <<  6)     // (EMAC) UND:  Transmit Underrun
//
// EMAC_RSR: (EMAC Offset: 0x20) Receive Status Register
//
#define EMAC_BNA                    ((unsigned int) 0x1 <<  0)     // (EMAC)
#define EMAC_REC                    ((unsigned int) 0x1 <<  1)     // (EMAC)
#define EMAC_RXOVR                  ((unsigned int) 0x1 <<  2)     // (EMAC) OVR:  Ethernet Receive Buffer Overrun
//
// EMAC_ISR: (EMAC Offset: 0x24) Interrupt Status Register
//
#define EMAC_MFD                    ((unsigned int) 0x1 <<  0)     // (EMAC) DONE: Management Done
#define EMAC_RCOMP                  ((unsigned int) 0x1 <<  1)     // (EMAC) RCOM: Receive Complete
#define EMAC_RBNA                   ((unsigned int) 0x1 <<  2)     // (EMAC) RBNA: Receive Buffer Not Available
#define EMAC_TOVR                   ((unsigned int) 0x1 <<  3)     // (EMAC) TOVR: Transmit Buffer Overrun
#define EMAC_TUND                   ((unsigned int) 0x1 <<  4)     // (EMAC) TUND: Transmit Buffer Underrun
#define EMAC_RTRY                   ((unsigned int) 0x1 <<  5)     // (EMAC) RTRY: Retry Limit
#define EMAC_TBRE                   ((unsigned int) 0x1 <<  6)     // (EMAC) TBRE: Transmit Buffer Register Empty
#define EMAC_TCOMP                  ((unsigned int) 0x1 <<  7)     // (EMAC) TCOM: Transmit Complete
#define EMAC_TIDLE                  ((unsigned int) 0x1 <<  8)     // (EMAC) TIDLE: Transmit Idle
#define EMAC_LINK                   ((unsigned int) 0x1 <<  9)     // (EMAC) LINK
#define EMAC_ROVR                   ((unsigned int) 0x1 << 10)     // (EMAC) ROVR: RX Overrun
#define EMAC_ABT                    ((unsigned int) 0x1 << 11)     // (EMAC) ABT: Abort
//
// Buffer descriptor configuration masks
//
#define  EMAC_RXBUF_ADDRESS_MASK    (0xFFFFFFFC)       // Addr of Rx Buffer Descriptor Buf's
#define  EMAC_RXBUF_ADD_WRAP        (1UL <<  1)        // Last buffer in the ring.
#define  EMAC_RXBUF_SW_OWNED        (1UL <<  0)        // Software owns the buffer.
#define  EMAC_TXBUF_ADD_WRAP        (1UL << 30)        // Mark last buffer in the ring
#define  EMAC_TXBUF_ADD_LAST        (1UL << 15)        // This is the last buffer for the current frame
#define  EMAC_TXBUF_STATUS_USED     (1UL << 31)
//
// Register definition for PMC peripheral
//
#define PMC_PCER                    (*(volatile U32*) 0xFFFFFC10) // (PMC) Peripheral Clock Enable  Register
#define PMC_PCDR                    (*(volatile U32*) 0xFFFFFC14) // (PMC) Peripheral Clock Disable Register
//
// Peripheral ID definition
//
#define ID_EMAC           ((unsigned int) 24)           // Ethernet MAC
//
// Register definition for AIC peripheral
//
#define AIC_BASE_ADDR               (0xFFFFF000UL)
#define AIC_SMR_BASE_ADDR           (AIC_BASE_ADDR + 0x00)
#define AIC_SVR_BASE_ADDR           (AIC_BASE_ADDR + 0x80)
#define AIC_SMR                     (*(volatile U32*) 0xFFFFF000) // (AIC) Source Mode Register
#define AIC_SVR                     (*(volatile U32*) 0xFFFFF080) // (AIC) Source Vector Register
#define AIC_IECR                    (*(volatile U32*) 0xFFFFF120) // (AIC) Interrupt Enable Command Register
#define AIC_IDCR                    (*(volatile U32*) 0xFFFFF124) // (AIC) Interrupt Disable Command Register
#define AIC_ICCR                    (*(volatile U32*) 0xFFFFF128) // (AIC) Interrupt Clear Command Register
//
// AIC_SMR : (AIC Offset: 0x0) Control Register
//
#define AIC_PRIOR                   ((unsigned int) 0x7 <<  0) // (AIC) Priority Level
#define AIC_PRIOR_LOWEST            ((unsigned int) 0x0)       // (AIC) Lowest priority level
#define AIC_SRCTYPE_INT_HIGH_LEVEL  ((unsigned int) 0x0 <<  5) // (AIC) Internal Sources Code Label High-level Sensitive

/*********************************************************************
*
*       Types
*
**********************************************************************
*/
typedef struct {
  U32  Addr;        // Address of RX buffer.
  U32  Stat;        // Status of RX buffer.
} BUFFER_DESC;

/*********************************************************************
*
*       Public data
*
**********************************************************************
*/

// None

/*********************************************************************
*
*       Static data
*
**********************************************************************
*/
//
// Buffer descriptor related variables
//
static BUFFER_DESC  *_paRxBufDesc;                                       // Pointer to Rx buffer descriptors
static U16           _iNextRx;
static U16           _NumRxBuffers = NUM_RX_BUFFERS;

static char _TxIsBusy;
static U8   _IsInited;
//
// PHY
//
static IP_PHY_CONTEXT         _PHY_Context;
static const IP_PHY_DRIVER  * _PHY_pDriver = &IP_PHY_Generic;

//
// Statistics
//
#if DEBUG
  static struct {
    int TxSendCnt;
    int TxIntCnt;
    int RxCnt;
    int RxIntCnt;
    int RxErrCnt;
  } _DriverStats;
  #define INC(Cnt) Cnt++
#else
  #define INC(Cnt)
#endif

/*********************************************************************
*
*       Static code
*
**********************************************************************
*/

/*********************************************************************
*
*       _Align
*/
static void * _Align(void * pMem, U32 NumBytes) {
  U32 Addr;

  Addr      = (U32)pMem;
  NumBytes -= 1;
  Addr     += NumBytes;
  Addr     &= ~NumBytes;
  return (void*)Addr;
}

/*********************************************************************
*
*       _PHY_WriteReg
*
*  Function description
*    Writes a PHY register.
*/
static void _PHY_WriteReg(IP_PHY_CONTEXT* pContext, unsigned RegIndex, unsigned v) {
  unsigned Addr;

  Addr     = pContext->Addr;
  EMAC_MAN = (1uL           << 30) // [31:30] SOF: Start of frame code (must be 0x01)
           | (1uL           << 28) // [29:28] RW flags: b10 Read, b01 Write
           | ((U32)Addr     << 23) // [27:23] 5 bit PhyAddr
           | ((U32)RegIndex << 18) // [22:18] 5 bit RegisterIndex
           | (2uL           << 16) // Code, must be b10
           | v;
  //
  // Wait for commmand to finish
  //
  while ((EMAC_NSR & EMAC_IDLE) == 0);
}

/*********************************************************************
*
*       _PHY_ReadReg
*
*  Function description
*    Reads a PHY register.
*/
static unsigned _PHY_ReadReg(IP_PHY_CONTEXT* pContext, unsigned RegIndex) {
  unsigned Addr;
  unsigned r;

  Addr = pContext->Addr;
  EMAC_MAN    = (1uL      << 30)  // [31:30] SOF: Start of frame code (must be 0x01)
              | (2uL      << 28)  // [29:28] RW flags: b10 Read, b01 Write
              | (Addr     << 23)  // [27:23] 5 bit PhyAddr
              | (RegIndex << 18)  // [22:18] 5 bit RegisterIndex
              | (2UL      << 16); // Code, must be b10
  //
  // Wait for commmand to finish
  //
  while ((EMAC_NSR & EMAC_IDLE) == 0);
  //
  // Read and return data
  //
  r = EMAC_MAN & 0x0000FFFF;
  return r;
}

/*********************************************************************
*
*       _PHY_pAccess
*/
static const IP_PHY_ACCESS _PHY_pAccess = {
  _PHY_ReadReg,
  _PHY_WriteReg
};

/*********************************************************************
*
*       _ComputeHash
*
*  Function description
*     Computes the has value acc. to the [1]:39.3.7
*
*  Add. information
*    [1] says
*        The destination address is reduced to a 6-bit
*        index into the 64-bit hash register using the following hash function. The hash function is an
*        exclusive or of every sixth bit of the destination address.
*        hash_index[5] = da[5] ^ da[11] ^ da[17] ^ da[23] ^ da[29] ^ da[35] ^ da[41] ^ da[47]
*        hash_index[4] = da[4] ^ da[10] ^ da[16] ^ da[22] ^ da[28] ^ da[34] ^ da[40] ^ da[46]
*        hash_index[3] = da[3] ^ da[09] ^ da[15] ^ da[21] ^ da[27] ^ da[33] ^ da[39] ^ da[45]
*        hash_index[2] = da[2] ^ da[08] ^ da[14] ^ da[20] ^ da[26] ^ da[32] ^ da[38] ^ da[44]
*        hash_index[1] = da[1] ^ da[07] ^ da[13] ^ da[19] ^ da[25] ^ da[31] ^ da[37] ^ da[43]
*        hash_index[0] = da[0] ^ da[06] ^ da[12] ^ da[18] ^ da[24] ^ da[30] ^ da[36] ^ da[42]
*        da[0] represents the least significant bit of the first byte received, that is, the multicast/unicast
*        indicator, and da[47] represents the most significant bit of the last byte received.
*/
static U32 _ComputeHash(U32 l, U32 h) {
  l  = l ^ (l >> 24) ^ (h << 8);  // Fold 48 bits to 24 bits
  l ^= l >> 12;                   // Fold to 12 bits
  l ^= l >> 6;                    // Fold to 6 bits
  return l & 63;
}

/*********************************************************************
*
*       _SetFilter
*
*  Function description
*    Sets the MAC filter(s)
*    The stack tells the driver which addresses should go thru  the filter.
*    The number of addresses can generally be unlimited.
*    In most cases, only one address is set.
*    However, if the NI is in multiple nets at the same time or if multicast is used,
*    multiple addresses can be set.
*
*  Notes
*    (1) Procedure
*        In general, precise filtering is used as far as supported by the hardware.
*        If the more addresses need to be filtered than precise address filters are
*        available (4 for this MAC), then the hash filter is used.
*        Alternativly, the MAC can be switched to promiscuous mode for simple implementations.
*/
static int _SetFilter(IP_NI_CMD_SET_FILTER_DATA * pFilter) {
  unsigned   NumAddr;
  const U8 * pAddrData;
  U32        aHashFilter[2];
  U32 *      pAddrFilter;
  char       EnableHashUC;
  char       EnableHashMC;
  U32        v;
  U32        w;
  unsigned   i;

  EnableHashUC   = 0;
  EnableHashMC   = 0;
  aHashFilter[0] = 0;
  aHashFilter[1] = 0;
  NumAddr        = pFilter->NumAddr;
  pAddrFilter    = (U32*)&EMAC_SA1L;
  //
  // Use the precise filters for the first 4 addresses, hash for the remaining ones
  //
  for (i = 0; i < NumAddr; i++) {
    pAddrData = *(&pFilter->pHWAddr + i);
    v = IP_LoadU32LE(pAddrData);     // lower (first) 32 bits
    w = IP_LoadU16LE(pAddrData + 4); // upper (last)  16 bits

    if (i < 4) {
      *pAddrFilter++ = v;
      *pAddrFilter++ = w;
    } else {
      if (v & 1) {
        EnableHashMC = 1;
      } else {
        EnableHashUC = 1;
      }
      v = _ComputeHash(v, w);
      aHashFilter[v >> 5] = (1 << (v & 31));
    }
    pAddrData += 6;
  }
  //
  // Fill the unused precise filter with Broadcast addr
  //
  for (; i < 4; i++) {
    *pAddrFilter++ = 0xFFFFFFFF;
    *pAddrFilter++ = 0xFFFF;
  }
  //
  // Update hash filter
  //
  EMAC_HRB = aHashFilter[0];
  EMAC_HRT = aHashFilter[1];
  //
  // Update control register to allow hashing for Unicast/Multicast
  //
  v  =  EMAC_NCFG;
  v &= ~(EMAC_UNI | EMAC_MTI);
  v |= EnableHashUC * EMAC_UNI + EnableHashMC * EMAC_MTI;
  EMAC_NCFG = v;
  return 0;     // O.K.
}

/*********************************************************************
*
*       _UpdateEMACSettings
*
*  Function description
*     Updates the speed & duplex settings of the EMAC.
*     Needs to be called whenever speed and duplex settings change.
*
*/
static void _UpdateEMACSettings(U32 Duplex, U32 Speed) {
  U32 v;
  //
  // Get EMAC config, clear speed & duplex bits
  //
  v  =  EMAC_NCFG;
  v &= ~(EMAC_SPD | EMAC_FD);
  if (Duplex == IP_DUPLEX_FULL) {
    v |= EMAC_FD;
  }
  if (Speed ==  IP_SPEED_100MHZ) {
    v |= EMAC_SPD;
  }
  EMAC_NCFG = v;
}

/*********************************************************************
*
*       _UpdateLinkState
*
*  Function description
*    Reads link state information from PHY and updates EMAC if necessary.
*    Should be called regularily to make sure that EMAC is notified if the link changes.
*/
static void _UpdateLinkState(void) {
  U32  Speed;
  U32  Duplex;

  _PHY_pDriver->pfGetLinkState(&_PHY_Context, &Duplex, &Speed);
  if (IP_SetCurrentLinkState(Duplex, Speed)) {
    _UpdateEMACSettings (Duplex, Speed);              /* Inform the EMAC about the current PHY settings       */
  }
}

/**********************************************************************************************************
*
*       _InitBufferDescs()
*
*  Function description:
*    Initializes the Rx buffer descriptors
*
*  Notes
*    (1) Buffer descriptors MUST start on a 64-byte boundary.
*/
static void _InitBufferDescs(void) {
  BUFFER_DESC * pBufferDesc;
  U32           DataAddr;
  int           i;

  pBufferDesc = _paRxBufDesc;
  DataAddr    = ((U32)pBufferDesc) + sizeof(BUFFER_DESC) * _NumRxBuffers;   // Addr of first buffer
  //
  // Initialize Rx buffer descriptors
  //
  for (i = 0; i < _NumRxBuffers; i++) {
    pBufferDesc->Addr = (DataAddr & EMAC_RXBUF_ADDRESS_MASK);
    pBufferDesc->Stat = 0;
    DataAddr         += _RX_BUFFER_SIZE;
    pBufferDesc++;
  }
  (pBufferDesc - 1)->Addr |= EMAC_RXBUF_ADD_WRAP;    // Set the Wrap bit on the last descriptor
  _iNextRx = 0;
  EMAC_RBQP  = (U32)_paRxBufDesc;
}

/**********************************************************************************************************
*
*       _ResetRxError()
*
*  Function description:
*    Resets the receiver logic in case of Fatal Rx errors (not simple packet corruption like CRC error)
*/
static void _ResetRxError(void) {
  EMAC_CTL &= ~EMAC_RE;       // Disable receiver
  _InitBufferDescs();
  EMAC_CTL |=  EMAC_RE;       // Re-enable receiver
}

/**********************************************************************************************************
*
*       _AllocBufferDescs()
*
*  Function description:
*    Allocates the Rx buffer descriptors
*
*  Notes
*    (1) Buffer descriptors MUST start on a 64-byte boundary.
*/
static void _AllocBufferDescs(void) {
  U8 * pMem;

  //
  // Alloc memory for buffer descriptors and buffers
  //
  pMem = (U8*)IP_Alloc(_NumRxBuffers * (sizeof(BUFFER_DESC) + _RX_BUFFER_SIZE) + 0x40);   // Alloc n * (8 byte buffer desc + 1536 buffer) + space for aligment
  pMem = (U8*)_Align(pMem, 0x40);
  _paRxBufDesc =  (BUFFER_DESC*) pMem;
}

/*********************************************************************
*
*       _SendPacket
*
*  Function description
*    Send the next packet in the send queue.
*    Function is called from a task via function pointer in Driver API table
*    or from Tx-interrupt.
*/
static int  _SendPacket(void) {
  U32      v;
  void   * pPacket;
  unsigned NumBytes;

  IP_GetNextOutPacket(&pPacket, &NumBytes);
  if (NumBytes == 0) {
    _TxIsBusy = 0;
    return 0;
  }
  IP_LOG((IP_MTYPE_DRIVER, "DRIVER: Sending packet: %d bytes", NumBytes));
  //
  // Make sure transmitter is actually idle
  //
  v = EMAC_TSR;
  if ((v & EMAC_BNQ) == 0) {
      IP_PANIC("Transmitter is busy, but flag indicates that it should not.");
  }
  //
  // Clear all transmitter errors.
  //
  EMAC_TSR = EMAC_TXOVR
           | EMAC_COL
           | EMAC_RLE
           | EMAC_BNQ
           | EMAC_COMP
           | EMAC_UND
           ;
  //
  // Write packet address in the EMAC Transmit Address Register
  // Write packet length into EMAC Transmit Control Register to start transmission
  //
  EMAC_TAR = (U32)pPacket;
  EMAC_TCR = NumBytes;
  INC(_DriverStats.TxSendCnt);
  return 0;
}

/*********************************************************************
*
*       _ISR_Handler
*
*  Function description
*    This is the interrupt service routine for the NI (EMAC).
*    It handles all interrupts (Rx, Tx, Error).
*
*/
static void  _ISR_Handler(void) {
  volatile U32  ISR_Status;
  U32  RSR_Status;
  U32  TSR_Status;

  ISR_Status  =  EMAC_ISR;     // Clears the bits in the ISR register, therefor required!
  RSR_Status  =  EMAC_RSR;
  TSR_Status  =  EMAC_TSR;
  EMAC_RSR    = RSR_Status;    // Clear Receive Status Register
  EMAC_TSR    = TSR_Status;    // Clear Transmit Status Register
  //
  // Handle Rx errors
  //
  if (RSR_Status & (EMAC_BNA | EMAC_RXOVR)) {
    INC(_DriverStats.RxErrCnt);
    _ResetRxError();
  }
  else if (RSR_Status & EMAC_REC) {  // Did we recieve a frame ?
    INC(_DriverStats.RxIntCnt);
    IP_OnRx();
  }
  if (TSR_Status & EMAC_COMP) {      // Frame completly sent ?
    if (_TxIsBusy) {
      IP_RemoveOutPacket();
      INC(_DriverStats.TxIntCnt);
      _SendPacket();
    }
  }
}

/*********************************************************************
*
*       _IntInit
*
*  Function description
*    Initialize the interrupt controller of the CPU for interrupts of EMAC.
*
*/
static void  _IntInit (void) {
  AIC_IDCR = 1UL << ID_EMAC;    // Ensure interrupts are disabled before init
  *(volatile U32*)(AIC_SVR_BASE_ADDR + 4 * ID_EMAC) = (U32)_ISR_Handler;
  *(volatile U32*)(AIC_SMR_BASE_ADDR + 4 * ID_EMAC) =  AIC_SRCTYPE_INT_HIGH_LEVEL | AIC_PRIOR_LOWEST;
  AIC_ICCR = 1UL << ID_EMAC;
  EMAC_IDR = 0x3FFF;            // Disable all EMAC interrupts
  AIC_IECR = 1UL << ID_EMAC;    // Enable the AIC EMAC Interrupt
}

/*********************************************************************
*
*       _Init
*
*  Function description
*    General init function of the driver.
*    Called by the stack in the init phase before any other driver function.
*/
static  int  _Init(unsigned Unit) {
  _PHY_Context.pAccess = &_PHY_pAccess;     // PHY read/write functions are static in this module
  if (_PHY_Context.Addr == 0) {             // Check if address has been set via control function
    _PHY_Context.Addr    = IP_PHY_ADDR_ANY; // Activate automatic scan for external PHY
  }
  BSP_ETH_Init(Unit);
  //
  // Disable all EMAC interrupts
  //
  EMAC_IDR = EMAC_MFD        // (EMAC) DONE:  Management Done
           | EMAC_RCOMP      // (EMAC) RCOM:  Receive Complete
           | EMAC_RBNA       // (EMAC) RBNA:  Receive Buffer Not Available
           | EMAC_TOVR       // (EMAC) TOVR:  Transmit Buffer Overrun
           | EMAC_TUND       // (EMAC) TUND:  Transmit Buffer Underrun
           | EMAC_RTRY       // (EMAC) RTRY:  Retry Limit
           | EMAC_TBRE       // (EMAC) TBRE:  Transmit Buffer Register Empty
           | EMAC_TCOMP      // (EMAC) TCOM:  Transmit Complete
           | EMAC_TIDLE      // (EMAC) TIDLE: Transmit Idle
           | EMAC_LINK       // (EMAC) LINK
           | EMAC_ROVR       // (EMAC) ROVR:  RX Overrun
           | EMAC_ABT        // (EMAC) ABT:   Abort
		       ;
  EMAC_CTL = 0;              // Disable Rx, Tx (Should be RESET state, to be safe)
  _AllocBufferDescs();
  _InitBufferDescs();
  EMAC_CTL = (0uL <<  0)     // LB: Loopback
           | (0uL <<  1)     // LBL: Loopback Local
           | (0uL <<  2)     // RE: Receive Enable
           | (0uL <<  3)     // TE: Transmit Enable 				
           | (1uL <<  4)     // MPE: Management Port Enable, enable MDIO
           | (1uL <<  5)     // CSR: Clear Statistics Registers
           | (0uL <<  6)     // ISR: Increment Statistics Registers 				
           | (0uL <<  7)     // WES: Write Enable for Statistics Registers 				
           | (0uL <<  8)     // BP: Back Pressure 				  				
           ;

  EMAC_NCFG = (1uL <<  0)    // SPD: Speed 1: 100MHZ 0: 10 MHZ
            | (0uL <<  1)    // FD: Full Duplex
            | (0uL <<  2)    // BR: Bit Rate (Optional)
            | (0uL <<  4)    // CAF: Copy all frames
            | (0uL <<  5)    // NBC: No Broadcast
            | (0uL <<  6)    // MTI: Multicast Hash Enable
            | (0uL <<  7)    // UNI: Unicast Hash Enable
            | (0uL <<  8)    // 1: BIG: the MAC receives up to 1522 bytes (extended Ethernet frame with VLAN tag (IEEE 802.3ac)). Normally the MAC receives frames up to 1518 bytes in length.
            | (0uL <<  9)    // EAE: External Address Match Enable
            | (2uL << 10)    // (EMAC) HCLK divided by 32. This works at all speeds, and communication is not time-critical.
            | (0uL << 12)    // RTY: Retry Test
            | (1uL << 13)    // RMII: Reduce MII
            ;
  //
  // Init PHY and update link state
  //
  _PHY_pDriver->pfInit(&_PHY_Context);      // Configure the PHY
  _UpdateLinkState();
  //
  // Enable interrupts
  //
  EMAC_RSR  = (EMAC_RXOVR | EMAC_REC | EMAC_BNA);   // Clear receive status register
  _IntInit();
  EMAC_IER |= ( EMAC_RCOMP   // Enable 'Receive complete' interrupt.
              | EMAC_ROVR    // Enable 'RX Overrun' interrupt.
              | EMAC_RBNA    // Enable 'Receive Buffer Not Available'.
              | EMAC_TCOMP   // Enable 'Tx Complete'
              | EMAC_TOVR    // Enable 'Transmit Buffer Overrun'
              );
  EMAC_CTL |= EMAC_TE;       // EnableTx
  EMAC_CTL |= EMAC_RE;       // EnableRx
  _IsInited = 1;
  return 0;
}

/*********************************************************************
*
*       _SendPacketIfTxIdle
*
*  Function description
*    Send the next packet in the send queue if transmitter is idle.
*    If transmitter is busy, nothing is done since the next packet is sent
*    automatically with Tx-interrupt.
*    Function is called from a task via function pointer in Driver API table.
*
*  Return value
*    0   O.K.
*    1   Interface error
*/
static int  _SendPacketIfTxIdle(unsigned Unit) {
  IP_OS_DisableInterrupt();
  if (_TxIsBusy == 0) {
    _TxIsBusy = 1;
    IP_OS_EnableInterrupt();
    _SendPacket();
    return 0;
  }
  IP_OS_EnableInterrupt();
  return 0;
}

/*********************************************************************
*
*       _GetPacketSize()
*
*  Function description
*    Reads buffer descriptors in order to find out if a packet has been received.
*    Different error conditions are checked and handled.
*    Function is called from a task via function pointer in Driver API table.
*
*  Return value
*    Number of bytes.
*    0 if no complete packet is available.
*/
static int _GetPacketSize(unsigned Unit) {
  U32 Addr;
  U32 Stat;
  BUFFER_DESC *pBufDesc;
  int i;
  int PacketSize;

  i = _iNextRx;
  if (i >= _NumRxBuffers) {
    i -= _NumRxBuffers;
  }
  pBufDesc = _paRxBufDesc + i;
  Addr = pBufDesc->Addr;
  Stat = pBufDesc->Stat;
  if ((Addr & EMAC_RXBUF_SW_OWNED) == 0) {
    return 0;                    //  This happens all the time since the stack polls until we are out of packets
  }
  PacketSize = Stat & 0x7FF;     // Packet size is lower bits[10:0] of buffer descriptor status.
  return PacketSize;
}

/*********************************************************************
*
*       _ReadPacket
*
*  Function description
*    Reads the first packet into the buffer.
*    NumBytes must be the correct number of bytes as retrieved by _GetPacketSize();
*    Function is called from a task via function pointer in Driver API table.
*/
static int _ReadPacket(unsigned Unit, U8 *pDest, unsigned NumBytes) {
  U8     *pSrc;
  U32 Addr;
  int i;
  BUFFER_DESC *pBufDesc;

  if (pDest) {
    i = _iNextRx;
    pBufDesc = _paRxBufDesc + i;
    Addr = pBufDesc->Addr;
    if (Addr & EMAC_RXBUF_SW_OWNED) {
      pSrc = (U8 *) (Addr & EMAC_RXBUF_ADDRESS_MASK);
      IP_MEMCPY(pDest, pSrc, NumBytes);
      //
      // Clear owner flag
      //
      if (Addr & EMAC_RXBUF_ADD_WRAP) { // Check if last buffer descriptor in descriptor list to keep wrap bit
        Addr &= ~EMAC_RXBUF_SW_OWNED;
        pBufDesc->Addr = Addr;
      } else {
        pBufDesc->Addr = Addr & EMAC_RXBUF_ADDRESS_MASK;
      }
      //
      // Free packet
      //
      (_paRxBufDesc + i)->Stat  = 0xacab;//0;
      if (pDest) {
        IP_LOG((IP_MTYPE_DRIVER, "Packet: %d Bytes --- Read", NumBytes));
      } else {
        IP_LOG((IP_MTYPE_DRIVER, "Packet: %d Bytes --- Discarded", NumBytes));
      }
      _iNextRx++;
      if (_iNextRx >= _NumRxBuffers) {
        _iNextRx -= _NumRxBuffers;
      }
      i = _iNextRx;
      INC(_DriverStats.RxCnt);
    }
  }
  return 0;
}

/*********************************************************************
*
*       _Timer
*
*  Function description
*    Timer function called by the Net task once per second.
*    Function is called from a task via function pointer in Driver API table.
*/
static void _Timer(unsigned Unit) {
  _UpdateLinkState();
}


/*********************************************************************
*
*       _Control
*
*  Function description
*    Allows driver specific configuration settings.
*/
static int _Control(unsigned Unit, int Cmd, void * p) {
  IP_USE_PARA(Unit);
  switch (Cmd) {
  case IP_NI_CMD_SET_BPRESSURE:
    EMAC_CTL |= EMAC_BPRESSURE;
    return 0;
  case IP_NI_CMD_CLR_BPRESSURE:
    EMAC_CTL &= ~EMAC_BPRESSURE;
    return 0;
  case IP_NI_CMD_SET_FILTER:
    return _SetFilter((IP_NI_CMD_SET_FILTER_DATA*)p);
  case IP_NI_CMD_SET_PHY_ADDR:
    if (_IsInited) {
      break;
    }
    _PHY_Context.Addr = (U8)(int)p;
    return 0;
  case IP_NI_CMD_SET_PHY_MODE:
    if (_IsInited) {
      break;
    }
    _PHY_Context.UseRMII = (U8)(int)p;
    return 0;
  case IP_NI_CMD_SET_SUPPORTED_DUPLEX_MODES:
    if (_IsInited) {
      break;
    }
    _PHY_Context.SupportedModes = (U16)(int)p;
    break;
  case IP_NI_CMD_POLL:  //  Poll MAC (typically once per ms) in cases where MAC does not trigger an interrupt.
    _ISR_Handler();
    break;
  default:
    ;
  }
  return -1;
}

/*********************************************************************
*
*       IP_NI_AT91RM9200_ConfigNumRxBuffers
*
*  Function description
*    Sets the number of Rx Buffers in the config phase.
*/
void IP_NI_AT91RM9200_ConfigNumRxBuffers(U16 NumRxBuffers) {
  if (_IsInited == 0) {
    _NumRxBuffers = NumRxBuffers;
  } else {
    IP_WARN((IP_MTYPE_DRIVER, "DRIVER: Can not configure driver after init."));
  }
}

/*********************************************************************
*
*       Driver API Table
*
**********************************************************************
*/
const IP_HW_DRIVER IP_Driver_AT91RM9200 = {
  _Init,
  _SendPacketIfTxIdle,
  _GetPacketSize,
  _ReadPacket,
  _Timer,
  _Control
};

/*************************** End of file ****************************/

