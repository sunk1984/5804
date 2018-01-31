/*********************************************************************
*               SEGGER MICROCONTROLLER SYSTEME GmbH                  *
*       Solutions for real time microcontroller applications         *
**********************************************************************
*                                                                    *
*       (C) 2003 - 2010   SEGGER Microcontroller Systeme GmbH        *
*                                                                    *
*       www.segger.com     Support: support@segger.com               *
*                                                                    *
**********************************************************************
*                                                                    *
*       TCP/IP stack for embedded applications                       *
*                                                                    *
**********************************************************************
----------------------------------------------------------------------
File    : IP_NI_LPC24xx.c
Purpose : Network interface driver template

Memory layout:
For a system with 4 Rx and 2 Tx buffers, the memory layout is as follows:

  0x7FE00000  RxDesc[0]
  0x7FE00008  RxDesc[1]
  0x7FE00010  RxDesc[2]
  0x7FE00018  RxDesc[3]
  0x7FE00020  RxStat[0]
  0x7FE00028  RxStat[1]
  0x7FE00030  RxStat[2]
  0x7FE00038  RxStat[3]
  0x7FE00040  RxBuffer[0]
  0x7FE00640  RxBuffer[1]
  0x7FE00c40  RxBuffer[2]
  0x7FE01240  RxBuffer[3]
  0x7FE01840  TxDesc[0]
  0x7FE01848  TxDesc[1]
  0x7FE01850  TxStat[0]
  0x7FE01854  TxStat[1]
  0x7FE01858  TxBuffer[0]
  0x7FE01E58  TxBuffer[1]

--------  END-OF-HEADER  ---------------------------------------------
*/

#include "IP_Int.h"
#include "BSP.h"
#include "IP_NI_LPC24xx.h"

#ifndef NI_USE_ISR
  #define NI_USE_ISR   1
#endif


/*********************************************************************
*
*       Defines, configurable
*
**********************************************************************
*/
#define NUM_RX_BUFFERS   4
#define NUM_TX_BUFFERS   4
#define RX_BUFFER_SIZE   0x600
#define TX_BUFFER_SIZE   0x600

/*********************************************************************
*
*       Defines, non configurable
*
**********************************************************************
*/

#define SCB_BASE_ADDR	      (0xE01FC000)
#define PCON                (*(volatile unsigned int*)(SCB_BASE_ADDR + 0x0C0))
#define PCONP               (*(volatile unsigned int*)(SCB_BASE_ADDR + 0x0C4)) // Power Control for Peripherals Register
#define PCONP_PCENET_MASK   (1 << 30)                                          // Ethernet block power/clock control bit.

//
// Ethernet MAC (32 bit data bus) -- all registers are RW unless indicated in parentheses
//
#define MAC_BASE_ADDR		    (0xFFE00000)                               // AHB Peripheral # 0
#define MAC_MAC1            (*(volatile U32 *)(MAC_BASE_ADDR + 0x000)) // MAC config reg 1
#define MAC_MAC2            (*(volatile U32 *)(MAC_BASE_ADDR + 0x004)) // MAC config reg 2
#define MAC_IPGT            (*(volatile U32 *)(MAC_BASE_ADDR + 0x008)) // b2b InterPacketGap reg
#define MAC_IPGR            (*(volatile U32 *)(MAC_BASE_ADDR + 0x00C)) // non b2b InterPacketGap reg
#define MAC_CLRT            (*(volatile U32 *)(MAC_BASE_ADDR + 0x010)) // CoLlision window/ReTry reg
#define MAC_MAXF            (*(volatile U32 *)(MAC_BASE_ADDR + 0x014)) // MAXimum Frame reg
#define MAC_SUPP            (*(volatile U32 *)(MAC_BASE_ADDR + 0x018)) // PHY SUPPort reg
#define MAC_TEST            (*(volatile U32 *)(MAC_BASE_ADDR + 0x01C)) // TEST reg
#define MAC_MCFG            (*(volatile U32 *)(MAC_BASE_ADDR + 0x020)) // MII Mgmt ConFiG reg
#define MAC_MCMD            (*(volatile U32 *)(MAC_BASE_ADDR + 0x024)) // MII Mgmt CoMmanD reg
#define MAC_MADR            (*(volatile U32 *)(MAC_BASE_ADDR + 0x028)) // MII Mgmt ADdRess reg
#define MAC_MWTD            (*(volatile U32 *)(MAC_BASE_ADDR + 0x02C)) // MII Mgmt WriTe Data reg (WO)
#define MAC_MRDD            (*(volatile U32 *)(MAC_BASE_ADDR + 0x030)) // MII Mgmt ReaD Data reg (RO)
#define MAC_MIND            (*(volatile U32 *)(MAC_BASE_ADDR + 0x034)) // MII Mgmt INDicators reg (RO)

#define MAC_SA0             (*(volatile U32 *)(MAC_BASE_ADDR + 0x040)) // Station Address 0 reg
#define MAC_SA1             (*(volatile U32 *)(MAC_BASE_ADDR + 0x044)) // Station Address 1 reg
#define MAC_SA2             (*(volatile U32 *)(MAC_BASE_ADDR + 0x048)) // Station Address 2 reg

#define MAC_COMMAND         (*(volatile U32 *)(MAC_BASE_ADDR + 0x100)) // Command reg
#define MAC_STATUS          (*(volatile U32 *)(MAC_BASE_ADDR + 0x104)) // Status reg (RO)
#define MAC_RXDESCRIPTOR    (*(volatile U32 *)(MAC_BASE_ADDR + 0x108)) // Rx descriptor base address reg
#define MAC_RXSTATUS        (*(volatile U32 *)(MAC_BASE_ADDR + 0x10C)) // Rx status base address reg
#define MAC_RXDESCRIPTORNUM (*(volatile U32 *)(MAC_BASE_ADDR + 0x110)) // Rx number of descriptors reg
#define MAC_RXPRODUCEINDEX  (*(volatile U32 *)(MAC_BASE_ADDR + 0x114)) // Rx produce index reg (RO)
#define MAC_RXCONSUMEINDEX  (*(volatile U32 *)(MAC_BASE_ADDR + 0x118)) // Rx consume index reg
#define MAC_TXDESCRIPTOR    (*(volatile U32 *)(MAC_BASE_ADDR + 0x11C)) // Tx descriptor base address reg
#define MAC_TXSTATUS        (*(volatile U32 *)(MAC_BASE_ADDR + 0x120)) // Tx status base address reg
#define MAC_TXDESCRIPTORNUM (*(volatile U32 *)(MAC_BASE_ADDR + 0x124)) // Tx number of descriptors reg
#define MAC_TXPRODUCEINDEX  (*(volatile U32 *)(MAC_BASE_ADDR + 0x128)) // Tx produce index reg
#define MAC_TXCONSUMEINDEX  (*(volatile U32 *)(MAC_BASE_ADDR + 0x12C)) // Tx consume index reg (RO)

#define MAC_TSV0            (*(volatile U32 *)(MAC_BASE_ADDR + 0x158)) // Tx status vector 0 reg (RO)
#define MAC_TSV1            (*(volatile U32 *)(MAC_BASE_ADDR + 0x15C)) // Tx status vector 1 reg (RO)
#define MAC_RSV             (*(volatile U32 *)(MAC_BASE_ADDR + 0x160)) // Rx status vector reg (RO)

#define MAC_FLOWCONTROLCNT  (*(volatile U32 *)(MAC_BASE_ADDR + 0x170)) // Flow control counter reg
#define MAC_FLOWCONTROLSTS  (*(volatile U32 *)(MAC_BASE_ADDR + 0x174)) // Flow control status reg

#define MAC_RXFILTERCTRL    (*(volatile U32 *)(MAC_BASE_ADDR + 0x200)) // Rx filter ctrl reg
#define MAC_RXFILTERWOLSTS  (*(volatile U32 *)(MAC_BASE_ADDR + 0x204)) // Rx filter WoL status reg (RO)
#define MAC_RXFILTERWOLCLR  (*(volatile U32 *)(MAC_BASE_ADDR + 0x208)) // Rx filter WoL clear reg (WO)

#define MAC_HASHFILTERL     (*(volatile U32 *)(MAC_BASE_ADDR + 0x210)) // Hash filter LSBs reg
#define MAC_HASHFILTERH     (*(volatile U32 *)(MAC_BASE_ADDR + 0x214)) // Hash filter MSBs reg

#define MAC_INTSTATUS       (*(volatile U32 *)(MAC_BASE_ADDR + 0xFE0)) // Interrupt status reg (RO)
#define MAC_INTENABLE       (*(volatile U32 *)(MAC_BASE_ADDR + 0xFE4)) // Interrupt enable reg
#define MAC_INTCLEAR        (*(volatile U32 *)(MAC_BASE_ADDR + 0xFE8)) // Interrupt clear reg (WO)
#define MAC_INTSET          (*(volatile U32 *)(MAC_BASE_ADDR + 0xFEC)) // Interrupt set reg (WO)

#define MAC_POWERDOWN       (*(volatile U32 *)(MAC_BASE_ADDR + 0xFF4)) // Power-down reg
#define MAC_MODULEID        (*(volatile U32 *)(MAC_BASE_ADDR + 0xFFC)) // Module ID reg (RO)

//
// MAC1 Configuration Register
//
#define MAC1_RX_ENABLE        (1 << 0)
#define MAC1_RX_PASS_ALL      (1 << 1)
#define MAC1_RX_FLOW_CTRL     (1 << 2)
#define MAC1_TX_FLOW_CTRL     (1 << 3)
#define MAC1_LOOPBACK         (1 << 4)
#define MAC1_RESET_TX         (1 << 8)
#define MAC1_RESET_MCS_TX     (1 << 9)
#define MAC1_RESET_RX         (1 << 10)
#define MAC1_RESET_MCS_RX     (1 << 11)
#define MAC1_RESET_SIM        (1 << 14)
#define MAC1_SOFT_RESET       (1 << 15)

//
// MAC2 Configuration Register
//
#define MAC2_FULL_DUPLEX      (1 << 0)
#define MAC2_FRAME_CHECK      (1 << 1)
#define MAC2_HUGE_FRAME       (1 << 2)
#define MAC2_DELAYED_CRC      (1 << 3)
#define MAC2_CRC_ENABLE       (1 << 4)
#define MAC2_PAD_CRC          (1 << 5)
#define MAC2_VLAN_PAD         (1 << 6)
#define MAC2_AUTO_DETECT      (1 << 7)
#define MAC2_PURE_PREAMBLE    (1 << 8)
#define MAC2_LONG_PREAMBLE    (1 << 9)
#define MAC2_NO_BACKOFF       (1 << 12)
#define MAC2_BACK_PRESSURE    (1 << 13)
#define MAC2_EXCESS_DEFER     (1 << 14)

//
// MII Mgmt Configuration Register
//
#define MCFG_SCAN_INCR        (1 << 0)
#define MCFG_SUPP_PREAMBLE    (1 << 1)
#define MCFG_HCLK_DIV_4       (0 << 2)
#define MCFG_HCLK_DIV_6       (2 << 2)
#define MCFG_HCLK_DIV_8       (3 << 2)
#define MCFG_HCLK_DIV_10      (4 << 2)
#define MCFG_HCLK_DIV_14      (5 << 2)
#define MCFG_HCLK_DIV_20      (6 << 2)
#define MCFG_HCLK_DIV_28      (7 << 2)
#define MCFG_RESET_MII_MGMT   (1 << 15)

//
// MII Mgmt Command Register
//
#define MCMD_READ             (1 << 0)
#define MCMD_SCAN             (1 << 1)

//
// MII Mgmt Indicators Register
//
#define MIND_BUSY             (1 << 0)
#define MIND_SCANNING         (1 << 1)
#define MIND_NOT_VALID        (1 << 2)
#define MIND_LINK_FAIL        (1 << 3)

//
// Command Register
//
#define CMD_RX_ENABLE         (1 << 0)
#define CMD_TX_ENABLE         (1 << 1)
#define CMD_REG_RESET         (1 << 3)
#define CMD_TX_RESET          (1 << 4)
#define CMD_RX_RESET          (1 << 5)
#define CMD_PASS_RUNT         (1 << 6)
#define CMD_PASS_RX_FILT      (1 << 7)
#define CMD_TX_FLOW_CTRL      (1 << 8)
#define CMD_RMII              (1 << 9)
#define CMD_FULL_DUPLEX       (1 << 10)

//
// Receive Filter Control Register
//
#define RFC_UNICAST           (1 << 0)
#define RFC_BROADCAST         (1 << 1)
#define RFC_MULTICAST         (1 << 2)
#define RFC_UNICAST_HASH      (1 << 3)
#define RFC_MULTICAST_HASH    (1 << 4)
#define RFC_PERFECT           (1 << 5)
#define RFC_MAGIC_PACKET      (1 << 12)
#define RFC_ENABLE_WOL        (1 << 13)

//
// Ethernet Interrupt Enable/Set/Clear Registers
//
#define INT_RX_OVERRUN        (1 << 0)
#define INT_RX_ERROR          (1 << 1)
#define INT_RX_FINISHED       (1 << 2)
#define INT_RX_DONE           (1 << 3)
#define INT_TX_UNDERRUN       (1 << 4)
#define INT_TX_ERROR          (1 << 5)
#define INT_TX_FINISHED       (1 << 6)
#define INT_TX_DONE           (1 << 7)
#define INT_SOFT              (1 << 12)
#define INT_WAKEUP            (1 << 13)

//
// Receive Status Information Word
//
#define RSI_RXSIZE_MSK        (0x7ff)
#define RSI_CTRL_FRAME        (1 << 18)
#define RSI_VLAN              (1 << 19)
#define RSI_FAIL_FILTER       (1 << 20)
#define RSI_MULTICAST         (1 << 21)
#define RSI_BROADCAST         (1 << 22)
#define RSI_CRC_ERROR         (1 << 23)
#define RSI_SYMBOL_ERROR      (1 << 24)
#define RSI_LENGTH_ERROR      (1 << 25)
#define RSI_RANGE_ERROR       (1 << 26)
#define RSI_ALIGN_ERROR       (1 << 27)
#define RSI_OVERRUN           (1 << 28)
#define RSI_NODESCRIPTOR      (1 << 29)
#define RSI_LAST              (1 << 30)
#define RSI_ERROR             ((unsigned)1 << 31)

/*********************************************************************
*
*       Check configuration
*
**********************************************************************
*/

/*********************************************************************
*
*       Types
*
**********************************************************************
*/

typedef struct {
  U32  Addr;        // Address of RX buffer.
  U32  Ctrl;        // Control information. [10:0]: Len, b31: IntEnable
} RX_BUFFER_DESC;

typedef struct {
  U32  Stat;
  U32  Crc;
} RX_BUFFER_STAT;

typedef struct {
  U32  Addr;        // Address of RX buffer.
  U32  Ctrl;        // Control information. [10:0]: Len, b31: IntEnable
} TX_BUFFER_DESC;

typedef struct {
  U32  Stat;
} TX_BUFFER_STAT;

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
static IP_PHY_CONTEXT        _PHY_Context;
static const IP_PHY_DRIVER * _PHY_pDriver = &IP_PHY_Generic;

static RX_BUFFER_DESC *      _paRxBufDesc;              // Pointer to Rx buffer descriptors
static RX_BUFFER_STAT *      _paRxBufStat;
static U8 *                  _pRxBuffer;

static TX_BUFFER_DESC  *     _paTxBufDesc;              // Pointer to Tx buffer descriptors
static TX_BUFFER_STAT  *     _paTxBufStat;
static U8 *                  _pTxBuffer;

static U16                   _NumRxBuffers = NUM_RX_BUFFERS;
static U16                   _NumTxBuffers = NUM_TX_BUFFERS;
static char                  _TxIsBusy;

static U8                    _UseISR = NI_USE_ISR;
static U8                    _IsInited;

/****** Statistics **************************************************/

#if DEBUG
  static int _TxSendCnt;
  static int _TxIntCnt;
  static int _RxCnt;
  static int _RxIntCnt;
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
*       _PHY_WriteReg
*/
static void _PHY_WriteReg(IP_PHY_CONTEXT* pContext, unsigned RegIndex, unsigned v) {
  unsigned Addr;

  Addr = pContext->Addr;

  MAC_MCMD = 0;
  MAC_MADR = (Addr << 8) | (RegIndex & 0x1f);
  MAC_MWTD = v;
  //
  // Wait for completion
  //
  while (MAC_MIND & MIND_BUSY);
}

/*********************************************************************
*
*       _PHY_ReadReg
*/
static unsigned _PHY_ReadReg(IP_PHY_CONTEXT* pContext, unsigned RegIndex) {
  unsigned Addr;
  unsigned r;

  Addr = pContext->Addr;
  MAC_MCMD = MCMD_READ;
  MAC_MADR = (Addr << 8) | (RegIndex & 0x1f);
  //
  // Wait for completion
  //
  while (MAC_MIND & (MIND_BUSY | MIND_NOT_VALID));
  MAC_MCMD = 0;                        // Clear READ command
  r = MAC_MRDD & 0x0000FFFF;
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
*        available, then the hash filter is used.
*        Alternativly, the MAC can be switched to promiscuous mode for simple implementations.
*/
static int _SetFilter(IP_NI_CMD_SET_FILTER_DATA * pFilter) {
  U32 v;
  U32 w;
  unsigned i;
  unsigned NumAddr;
  const U8 * pAddrData;

  NumAddr = pFilter->NumAddr;
  //
  // Use the precise filters for the first address, hash for the remaining ones
  //
  for (i = 0; i < NumAddr; i++) {
    pAddrData   = *(&pFilter->pHWAddr + i);
    // Note:
    //  Station Address 0 Register
    //     7:0 Second octet
    //    15:8 First octet
    //  Station Address 1 Register
    //     7:0 Fourth octet
    //    15:8 Third octet
    //  Station Address 2 Register
    //     7:0 Sixth octet
    //    15:8 Fifth octet
    v  = IP_LoadU16LE(pAddrData + 4);
    v |= IP_LoadU16LE(pAddrData + 2) << 16;
    w  = IP_LoadU16LE(pAddrData);
    if (i < 1) {         // Perfect filter available ?
      MAC_SA0 = v & 0xFFFF;
      MAC_SA1 = v >> 16;
      MAC_SA2 = w;
    } else {
      // TBD: add to hash filter variables
    }
    pAddrData += 6;
  }
  //
  // Update hash filter
  //
  MAC_RXFILTERCTRL = RFC_PERFECT
                   | RFC_BROADCAST
                   ;
  return 0;        // O.K.
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
  void   * pPacket;
  unsigned NumBytes;
  unsigned WrPos;
  unsigned NextWr;
  TX_BUFFER_DESC  *pTxBufDesc;
  void *pBuffer;

  IP_GetNextOutPacket(&pPacket, &NumBytes);
  if (NumBytes == 0) {
    _TxIsBusy = 0;
    return 0;
  }
  IP_LOG((IP_MTYPE_DRIVER, "DRIVER: Sending packet: %d bytes", NumBytes));
  INC(_TxSendCnt);
  //
  // Copy data into Ethernet RAM
  //
  WrPos = MAC_TXPRODUCEINDEX;
  pBuffer = _pTxBuffer + WrPos * TX_BUFFER_SIZE;
  IP_MEMCPY(pBuffer, pPacket, NumBytes);
  IP_RemoveOutPacket();      // Right after memcopy, stack can forget about the packet
  //
  // Prepare descriptor
  //
  if (NumBytes < 60) {
    NumBytes = 60;                  // Make sure packet is at least 64 bytes (4 bytes are CRC) so we do not rely on the hardware to pad
  }
  pTxBufDesc = _paTxBufDesc + WrPos;
  pTxBufDesc->Addr = (U32)pBuffer;
  pTxBufDesc->Ctrl = (NumBytes - 1) // The size is -1 encoded e.g. if the buffer is 8 bytes the size field should be equal to 7.
                   | (1 << 26)
                   | (1 << 29)
                   | (1 << 30)
                   | (1UL << 31)
                   ;
  //
  // Start send by incrementing producer index
  //
  NextWr = MAC_TXPRODUCEINDEX + 1;
  if (NextWr >= _NumTxBuffers) {
    NextWr = 0;
  }
  MAC_TXPRODUCEINDEX = NextWr;
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
  U32 IntStat;

  IntStat = MAC_INTSTATUS;
  MAC_INTCLEAR = IntStat;
  //
  // Handle Rx
  //
  if (IntStat & (INT_RX_DONE | INT_RX_ERROR)) {
    INC(_RxIntCnt);
    IP_OnRx();
  }
  //
  // Handle Tx
  //
  if (IntStat & (INT_TX_DONE | INT_TX_ERROR)) {
    if (_TxIsBusy) {
      INC(_TxIntCnt);
      _SendPacket();
    } else {
      IP_WARN((IP_MTYPE_DRIVER, "DRIVER: Tx complete interrupt, but no packet sent."));
    }
  }
  //
  // Handle receive overrun
  //
  if (IntStat & INT_RX_OVERRUN) {
    MAC_COMMAND |= CMD_RX_RESET;
    MAC_COMMAND |= CMD_RX_ENABLE;
    MAC_MAC1 |= MAC1_RX_ENABLE;
  }
  //
  // Handle transmitter underrun
  //
  if (IntStat & INT_TX_UNDERRUN) {
    MAC_COMMAND |= CMD_TX_RESET;
    MAC_COMMAND |= CMD_TX_ENABLE;
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
*    (1) Buffer descriptors MUST start on a 32-byte boundary.
*/
static void _InitBufferDescs(void) {
  RX_BUFFER_DESC * pRxBufferDesc;
  RX_BUFFER_STAT * pRxBufferStat;
  TX_BUFFER_DESC * pTxBufferDesc;
  TX_BUFFER_STAT * pTxBufferStat;
  U32 DataAddr;
  int i;

  //
  // Initialize Rx buffer descriptors
  //
  pRxBufferDesc = _paRxBufDesc;
  pRxBufferStat = _paRxBufStat;
  DataAddr    = (U32)_pRxBuffer;
  for (i = 0; i < _NumRxBuffers; i++) {
    pRxBufferDesc->Addr   = DataAddr;
    pRxBufferDesc->Ctrl   = (1UL << 31) | (RX_BUFFER_SIZE -1);
    pRxBufferStat->Stat   = 0;
    DataAddr   += RX_BUFFER_SIZE;
    pRxBufferDesc++;
    pRxBufferStat++;
  }
  //
  // Initialize Tx buffer descriptors
  //
  pTxBufferDesc = _paTxBufDesc;
  pTxBufferStat = _paTxBufStat;
  DataAddr    = (U32)_pTxBuffer;
  for (i = 0; i < _NumTxBuffers; i++) {
    pTxBufferDesc->Addr   = DataAddr;
    pTxBufferDesc->Ctrl   = 0;
    pTxBufferStat->Stat   = 0;
    DataAddr   += TX_BUFFER_SIZE;
    pTxBufferDesc++;
    pTxBufferStat++;
  }
}

/**********************************************************************************************************
*
*       _AllocBufferDescs()
*
*  Function description:
*    Allocates the Rx buffer descriptors
*
*  Notes
*    (1) Buffer descriptors MUST start on a 32-byte boundary.
*/
static void _AllocBufferDescs(void) {
  U8 * pMem;

  pMem         = (U8*)0x7FE00000;
  //
  // Assign memory to RX descriptors, status blocks and buffers
  //
  _paRxBufDesc = (RX_BUFFER_DESC *)pMem;
  pMem        += _NumRxBuffers * sizeof(RX_BUFFER_DESC);
  _paRxBufStat = (RX_BUFFER_STAT *)pMem;
  pMem        += _NumRxBuffers * sizeof(RX_BUFFER_STAT);
  _pRxBuffer   = pMem;
  pMem        += _NumRxBuffers * RX_BUFFER_SIZE;
  //
  // Assign memory to TX descriptors, status blocks and buffers
  //
  _paTxBufDesc = (TX_BUFFER_DESC *)pMem;
  pMem        += _NumTxBuffers * sizeof(TX_BUFFER_DESC);
  _paTxBufStat = (TX_BUFFER_STAT *)pMem;
  pMem        += _NumTxBuffers * sizeof(TX_BUFFER_STAT);
  _pTxBuffer   = pMem;
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
  // Set speed
  //
  v = 0;
  if (Speed ==  IP_SPEED_100MHZ) {
    v = (1 << 8);     // [1]: 11.10.7: Speed bit needs to bet set for RMII and 100MHz operation
  } else {
    v = 0;
  }
  MAC_SUPP = v;
  //
  // Set duplex mode
  //
  v  = MAC_COMMAND;
  v &= ~(1uL << 10);  // Remove Full Duplex bit.
  if (Duplex == IP_DUPLEX_FULL) {
    MAC_IPGT = 0x00000015; // BACK-TO-BACK INTER-PACKET-GAP
    v |= (1 << 10);
  } else {
    MAC_IPGT = 0x00000012; // BACK-TO-BACK INTER-PACKET-GAP
  }
  MAC_COMMAND = v;
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

/*********************************************************************
*
*       _Init
*
*  Function description
*    General init function of the driver.
*    Called by the stack in the init phase before any other driver function.
*/
static  int  _Init(unsigned Unit) {
  U32 v;

  _PHY_Context.pAccess = &_PHY_pAccess;        // PHY read/write functions are static in this module
  _PHY_Context.Addr    = IP_PHY_ADDR_ANY;      // Activate automatic scan for external PHY
  _PHY_Context.UseRMII = 1;                    // Use RMII mode
  //
  // Enable power
  //
  PCONP |= PCONP_PCENET_MASK;
  BSP_ETH_Init(Unit);
  //
  // Check MAC Id
  //
  v = MAC_MODULEID;
  if ((v & 0xFFFF0000) == 0x39020000) {
    IP_LOG((IP_MTYPE_DRIVER | IP_MTYPE_INIT, "DRIVER: MAC with Id %x found.", v));
  } else {
    IP_WARN((IP_MTYPE_DRIVER | IP_MTYPE_INIT, "DRIVER: Wrong MAC Id."));
    return 1;
  }
  //
  // Soft Reset  MAC
  //
  MAC_MAC1 = (1 << 15);
  //
  // Perform soft reset of the controller
  //
  MAC_MAC1 = ( 0
             | MAC1_RESET_TX
             | MAC1_RESET_MCS_TX
             | MAC1_RESET_RX
             | MAC1_RESET_MCS_RX
             | MAC1_RESET_SIM
             | MAC1_SOFT_RESET
             );
   //
   // Reset host interface
   //
   MAC_COMMAND = ( 0
                 | CMD_REG_RESET
                 | CMD_TX_RESET
                 | CMD_RX_RESET
                 );
  MAC_MAC2 = MAC2_CRC_ENABLE | MAC2_PAD_CRC | MAC2_AUTO_DETECT;
  IP_OS_Delay(1);
  MAC_MAC1 = 0;
  IP_OS_Delay(1);
  //
  // Reset the MII Management hardware.
  //
  MAC_MCFG = MCFG_RESET_MII_MGMT;
  IP_OS_Delay(1);
  MAC_MCFG = MCFG_HCLK_DIV_28;       // Dividing by 28 allows up 70 MHz Clock speed
  if (_PHY_Context.UseRMII) {
    MAC_COMMAND = CMD_RMII;
  } else {
    MAC_COMMAND = 0;
  }
  //
  // Configure the PHY
  //
  _PHY_pDriver->pfInit(&_PHY_Context);
  //
  // Allocate and initialize the buffer descriptors
  //
  _AllocBufferDescs();
  _InitBufferDescs();
  //
  // Inform EMAC about Descriptors, status-blocks and buffers
  //
  MAC_RXDESCRIPTOR    = (U32) _paRxBufDesc;
  MAC_RXSTATUS        = (U32) _paRxBufStat;
  MAC_RXDESCRIPTORNUM = _NumRxBuffers - 1;
  MAC_TXDESCRIPTOR    = (U32) _paTxBufDesc;
  MAC_TXSTATUS        = (U32) _paTxBufStat;
  MAC_TXDESCRIPTORNUM = _NumTxBuffers - 1;
  //
  //
  //
  MAC_IPGR = (0x12 << 0) // NON-BACK-TO-BACK INTER-PACKET-GAP PART2
           | (0x0C << 8) // NON-BACK-TO-BACK INTER-PACKET-GAP PART1
           ;
  //
  // Enable Rx & Tx
  //
  MAC_MAC1    |= (1 << 0);
  MAC_COMMAND |= (1 << 0)      // RxEnable
              |  (1 << 1)      // TxEnable
              ;
  _UpdateLinkState();
  if (_UseISR) {
    //
    // Enable interrupts
    //
    BSP_ETH_InstallISR(_ISR_Handler);
    MAC_INTENABLE = INT_RX_OVERRUN
                  | INT_RX_DONE
                  | INT_TX_UNDERRUN
                  | INT_TX_ERROR
                  | INT_TX_DONE
                  ;
  }
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
*/
static int  _SendPacketIfTxIdle(unsigned Unit) {
  (void)Unit;   // Avoid parameter never referenced warning.
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
*    Number of bytes used for the next packet.
*    0 if no complete packet is available.
*/
static int _GetPacketSize(unsigned Unit) {
  U32 RdIndex;
  U32 WrIndex;
  U32 v;
  RX_BUFFER_STAT * pRxStat;

  (void)Unit;   // Avoid parameter never referenced warning.
  RdIndex = MAC_RXCONSUMEINDEX;
  WrIndex = MAC_RXPRODUCEINDEX;
  while (1) {
    if (RdIndex == WrIndex) {
      break;                      // We are done, no packet
    }
    pRxStat = _paRxBufStat + RdIndex;
    v = pRxStat->Stat;
    if ((v & (RSI_CRC_ERROR | RSI_SYMBOL_ERROR | RSI_LENGTH_ERROR | RSI_ALIGN_ERROR | RSI_LAST)) == RSI_LAST) {
      v  = (v & 0x7FF) + 1; // Get packet size
      return (v - 4);       // Give packet size without the trailing 4-bytes CRC to the stack
    }
    if (v & RSI_CRC_ERROR) {
      IP_WARN((IP_MTYPE_DRIVER, "DRIVER: CRC error. Rx packet dropped."));
    }
    //
    // Increment Consume Index to discard packet
    //
    if (++RdIndex == _NumRxBuffers) {
      RdIndex = 0;
    }
    MAC_RXCONSUMEINDEX = RdIndex;
  }
  return 0;
}

/*********************************************************************
*
*       _ReadPacket
*
*  Function description
*    Reads the first packet into the buffer.
*    NumBytes must be the correct number of bytes as retrieved by _GetPacketSize();
*    Function is called from a task via function pointer in Driver API table.
*
*/
static int _ReadPacket(unsigned Unit, U8 *pDest, unsigned NumBytes) {
  U32 RdIndex;
  U32 Addr;
  RX_BUFFER_DESC * pRxDesc;

  (void)Unit;   // Avoid parameter never referenced warning.
  RdIndex = MAC_RXCONSUMEINDEX;
  pRxDesc = _paRxBufDesc + RdIndex;
  Addr = pRxDesc->Addr;
  if (pDest != NULL) {
    IP_MEMCPY (pDest, (void*)Addr, NumBytes);  // Copy the packet without the 4 bytes Ethernet CRC.
  }
  // Increment Consume Index to discard packet
  if (++RdIndex == _NumRxBuffers) {
    RdIndex = 0;
  }
  INC(_RxCnt);
  MAC_RXCONSUMEINDEX = RdIndex;
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
  (void)Unit;   // Avoid parameter never referenced warning.
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
  (void)Unit;   // Avoid parameter never referenced warning.
  switch (Cmd) {
  case IP_NI_CMD_SET_BPRESSURE:
    //
    // TBD: Enable back pressure (if supported) and change return value to 0
    //
    break;
  case IP_NI_CMD_CLR_BPRESSURE:
    //
    // TBD: Disable back pressure (if supported) and change return value to 0
    //
    return -1;
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
    return 0;
  case IP_NI_CMD_POLL:  //  Poll MAC (typically once per ms) in cases where MAC does not trigger an interrupt.
    _ISR_Handler();
    return 0;
  case IP_NI_CMD_CFG_POLL:  //  Poll MAC (typically once per ms) in cases where MAC does not trigger an interrupt.
    if (_IsInited) {
      break;
    }
    _UseISR = 0;
    return 0;
  default:
    ;
  }
  return -1;
}


/*********************************************************************
*
*       Driver API Table
*
**********************************************************************
*/
const IP_HW_DRIVER IP_Driver_LPC24xx = {
  _Init,
  _SendPacketIfTxIdle,
  _GetPacketSize,
  _ReadPacket,
  _Timer,
  _Control
};

/*************************** End of file ****************************/
