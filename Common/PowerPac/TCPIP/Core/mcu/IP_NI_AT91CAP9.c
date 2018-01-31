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
File    : IP_NI_AT91CAP9.c
Purpose : NI driver for AT91CAP
          Driver defaults to use the built-in SRAM.
          Driver defaults to use the built-in SRAM for Tx buffers,
          Tx descriptors and Rx descriptors. SDRAM is only used for
          Rx buffers.
          The Rx descriptors have also to remain in SRAM. This avoids
          problems with the cache and the required alignment in case
          of cache invalidation.
--------  END-OF-HEADER  ---------------------------------------------
*/

#include  "IP_Int.h"
#include  "BSP.h"
#include  "IP_NI_AT91CAP9.h"

/*********************************************************************
*
*       Defines, configurable
*
**********************************************************************
*/

#define _NUM_RX_BUFFERS   200 //(60 * 12 + 1)  // One buffer is 128 bytes.
                                               // 12 is enough for 1 large packets.
                                               // One extra to allow n large packets.

#if 1
#define _USE_RMII           (1)                // Use RMII interface to PHY
#else
#define _USE_RMII           (0)                // Use MII interface to PHY
#endif

#define _TX_BUFFER_SIZE     (1536)
#define _TX_MEM_START_ADDR  (0x100000)        // Internal SRAM, physical & virtual address
#define _TX_MEM_SIZE        (0x8000)          // Internal SRAM is 32KB
#define _NUM_TX_BUFFERS     (1)

/*********************************************************************
*
*       Defines, non configurable
*
**********************************************************************
*/

/*********************************************************************
*
*       AT91CAP9 peripheral addresses, sfrs and related control bits
*/
//
// Register definition for RSTC peripheral
//
#define RSTC_BASE_ADDR                (0xFFFFFD00)
#define RSTC_RCR                      (*(volatile U32*)(RSTC_BASE_ADDR + 0x00)) // (RSTC) Reset Control Register
#define RSTC_RSR                      (*(volatile U32*)(RSTC_BASE_ADDR + 0x04)) // (RSTC) Reset Status Register
#define RSTC_RMR                      (*(volatile U32*)(RSTC_BASE_ADDR + 0x08)) // (RSTC) Reset Mode Register
#define RSTC_ERSTL                    (0xF <<  8) // (RSTC) User Reset Length
#define RSTC_EXTRST                   (0x1 <<  3) // (RSTC) External Reset
#define RSTC_NRSTL                    (0x1 << 16) // (RSTC) NRST pin level
//
// Register definition for AIC peripheral
//
#define _AIC_BASE_ADDR      (0xFFFFF000)
#define _AIC_SMR_BASE_ADDR  (_AIC_BASE_ADDR + 0x00)
#define _AIC_SVR_BASE_ADDR  (_AIC_BASE_ADDR + 0x80)
#define AIC_SMR   (*(volatile U32*)   (_AIC_BASE_ADDR + 0x000)) // (AIC) Source Mode Register
#define AIC_SVR   (*(volatile U32*)   (_AIC_BASE_ADDR + 0x080)) // (AIC) Source Vector Register
#define AIC_IECR  (*(volatile U32*)   (_AIC_BASE_ADDR + 0x120)) // (AIC) Interrupt Enable Command Register
#define AIC_IDCR  (*(volatile U32*)   (_AIC_BASE_ADDR + 0x124)) // (AIC) Interrupt Disable Command Register
#define AIC_ICCR  (*(volatile U32*)   (_AIC_BASE_ADDR + 0x128)) // (AIC) Interrupt Clear Command Register
//
// AIC_SMR : (AIC Offset: 0x0) Control Register
//
#define AIC_PRIOR                     ((unsigned int) 0x7 <<  0) // (AIC) Priority Level
#define AIC_PRIOR_LOWEST              ((unsigned int) 0x0)       // (AIC) Lowest priority level
#define AIC_SRCTYPE_INT_HIGH_LEVEL    ((unsigned int) 0x0 <<  5) // (AIC) Internal Sources Code Label High-level Sensitive
//
// Register definition for PMC peripheral
//
#define PMC_PCER   (*(volatile U32*)   0xFFFFFC10) // (PMC) Peripheral Clock Enable Register
//
// Peripheral ID definitions for AT91CAP9
//
#define ID_EMAC   ((unsigned int) 22) // Ethernet MAC
//
// MATRIX definitions
//
#define MATRIX_BASE_ADDR              (0xFFFFEE00)
#define MATRIX_SCFG3                  (*(volatile U32*) (MATRIX_BASE_ADDR + 0x4C)) // Slave configuration register 3
#define MATRIX_PRAS3                  (*(volatile U32*) (MATRIX_BASE_ADDR + 0x98)) // Priority Register A for Slave 3
//
// Register definition for EMAC peripheral
//
#define EMAC_BASE_ADDR (0xFFFBC000)
#define EMAC_TID                      (*(volatile U32*)(EMAC_BASE_ADDR + 0x0B8)) // (EMAC) Type ID Checking Register
#define EMAC_SA3L                     (*(volatile U32*)(EMAC_BASE_ADDR + 0x0A8)) // (EMAC) Specific Address 3 Bottom, First 4 bytes
#define EMAC_STE                      (*(volatile U32*)(EMAC_BASE_ADDR + 0x084)) // (EMAC) SQE Test Error Register
#define EMAC_RSE                      (*(volatile U32*)(EMAC_BASE_ADDR + 0x074)) // (EMAC) Receive Symbol Errors Register
#define EMAC_IDR                      (*(volatile U32*)(EMAC_BASE_ADDR + 0x02C)) // (EMAC) Interrupt Disable Register
#define EMAC_TBQP                     (*(volatile U32*)(EMAC_BASE_ADDR + 0x01C)) // (EMAC) Transmit Buffer Queue Pointer
#define EMAC_TPQ                      (*(volatile U32*)(EMAC_BASE_ADDR + 0x0BC)) // (EMAC) Transmit Pause Quantum Register
#define EMAC_SA1L                     (*(volatile U32*)(EMAC_BASE_ADDR + 0x098)) // (EMAC) Specific Address 1 Bottom, First 4 bytes
#define EMAC_RLE                      (*(volatile U32*)(EMAC_BASE_ADDR + 0x088)) // (EMAC) Receive Length Field Mismatch Register
#define EMAC_IMR                      (*(volatile U32*)(EMAC_BASE_ADDR + 0x030)) // (EMAC) Interrupt Mask Register
#define EMAC_SA1H                     (*(volatile U32*)(EMAC_BASE_ADDR + 0x09C)) // (EMAC) Specific Address 1 Top, Last 2 bytes
#define EMAC_PFR                      (*(volatile U32*)(EMAC_BASE_ADDR + 0x03C)) // (EMAC) Pause Frames received Register
#define EMAC_FCSE                     (*(volatile U32*)(EMAC_BASE_ADDR + 0x050)) // (EMAC) Frame Check Sequence Error Register
#define EMAC_FTO                      (*(volatile U32*)(EMAC_BASE_ADDR + 0x040)) // (EMAC) Frames Transmitted OK Register
#define EMAC_TUND                     (*(volatile U32*)(EMAC_BASE_ADDR + 0x064)) // (EMAC) Transmit Underrun Error Register
#define EMAC_ALE                      (*(volatile U32*)(EMAC_BASE_ADDR + 0x054)) // (EMAC) Alignment Error Register
#define EMAC_SCF                      (*(volatile U32*)(EMAC_BASE_ADDR + 0x044)) // (EMAC) Single Collision Frame Register
#define EMAC_SA3H                     (*(volatile U32*)(EMAC_BASE_ADDR + 0x0AC)) // (EMAC) Specific Address 3 Top, Last 2 bytes
#define EMAC_ELE                      (*(volatile U32*)(EMAC_BASE_ADDR + 0x078)) // (EMAC) Excessive Length Errors Register
#define EMAC_CSE                      (*(volatile U32*)(EMAC_BASE_ADDR + 0x068)) // (EMAC) Carrier Sense Error Register
#define EMAC_DTF                      (*(volatile U32*)(EMAC_BASE_ADDR + 0x058)) // (EMAC) Deferred Transmission Frame Register
#define EMAC_RSR                      (*(volatile U32*)(EMAC_BASE_ADDR + 0x020)) // (EMAC) Receive Status Register
#define EMAC_USRIO                    (*(volatile U32*)(EMAC_BASE_ADDR + 0x0C0)) // (EMAC) USER Input/Output Register
#define EMAC_SA4L                     (*(volatile U32*)(EMAC_BASE_ADDR + 0x0B0)) // (EMAC) Specific Address 4 Bottom, First 4 bytes
#define EMAC_RRE                      (*(volatile U32*)(EMAC_BASE_ADDR + 0x06C)) // (EMAC) Receive Ressource Error Register
#define EMAC_RJA                      (*(volatile U32*)(EMAC_BASE_ADDR + 0x07C)) // (EMAC) Receive Jabbers Register
#define EMAC_TPF                      (*(volatile U32*)(EMAC_BASE_ADDR + 0x08C)) // (EMAC) Transmitted Pause Frames Register
#define EMAC_ISR                      (*(volatile U32*)(EMAC_BASE_ADDR + 0x024)) // (EMAC) Interrupt Status Register
#define EMAC_MAN                      (*(volatile U32*)(EMAC_BASE_ADDR + 0x034)) // (EMAC) PHY Maintenance Register
#define EMAC_WOL                      (*(volatile U32*)(EMAC_BASE_ADDR + 0x0C4)) // (EMAC) Wake On LAN Register
#define EMAC_USF                      (*(volatile U32*)(EMAC_BASE_ADDR + 0x080)) // (EMAC) Undersize Frames Register
#define EMAC_HRB                      (*(volatile U32*)(EMAC_BASE_ADDR + 0x090)) // (EMAC) Hash Address Bottom[31:0]
#define EMAC_PTR                      (*(volatile U32*)(EMAC_BASE_ADDR + 0x038)) // (EMAC) Pause Time Register
#define EMAC_HRT                      (*(volatile U32*)(EMAC_BASE_ADDR + 0x094)) // (EMAC) Hash Address Top[63:32]
#define EMAC_REV                      (*(volatile U32*)(EMAC_BASE_ADDR + 0x0FC)) // (EMAC) Revision Register
#define EMAC_MCF                      (*(volatile U32*)(EMAC_BASE_ADDR + 0x048)) // (EMAC) Multiple Collision Frame Register
#define EMAC_SA2L                     (*(volatile U32*)(EMAC_BASE_ADDR + 0x0A0)) // (EMAC) Specific Address 2 Bottom, First 4 bytes
#define EMAC_NCR                      (*(volatile U32*)(EMAC_BASE_ADDR + 0x000)) // (EMAC) Network Control Register
#define EMAC_FRO                      (*(volatile U32*)(EMAC_BASE_ADDR + 0x04C)) // (EMAC) Frames Received OK Register
#define EMAC_LCOL                     (*(volatile U32*)(EMAC_BASE_ADDR + 0x05C)) // (EMAC) Late Collision Register
#define EMAC_SA4H                     (*(volatile U32*)(EMAC_BASE_ADDR + 0x0B4)) // (EMAC) Specific Address 4 Top, Last 2 bytes
#define EMAC_NCFGR                    (*(volatile U32*)(EMAC_BASE_ADDR + 0x004)) // (EMAC) Network Configuration Register
#define EMAC_TSR                      (*(volatile U32*)(EMAC_BASE_ADDR + 0x014)) // (EMAC) Transmit Status Register
#define EMAC_SA2H                     (*(volatile U32*)(EMAC_BASE_ADDR + 0x0A4)) // (EMAC) Specific Address 2 Top, Last 2 bytes
#define EMAC_ECOL                     (*(volatile U32*)(EMAC_BASE_ADDR + 0x060)) // (EMAC) Excessive Collision Register
#define EMAC_ROV                      (*(volatile U32*)(EMAC_BASE_ADDR + 0x070)) // (EMAC) Receive Overrun Errors Register
#define EMAC_NSR                      (*(volatile U32*)(EMAC_BASE_ADDR + 0x008)) // (EMAC) Network Status Register
#define EMAC_RBQP                     (*(volatile U32*)(EMAC_BASE_ADDR + 0x018)) // (EMAC) Receive Buffer Queue Pointer
#define EMAC_IER                      (*(volatile U32*)(EMAC_BASE_ADDR + 0x028)) // (EMAC) Interrupt Enable Register
//
// EMAC_NCFGR : (EMAC Offset: 0x4) Network Configuration Register
//
#define EMAC_SPD                      (1uL <<  0) // (EMAC) Speed.
#define EMAC_FD                       (1uL <<  1) // (EMAC) Full duplex.
#define EMAC_CAF                      (1uL <<  4) // (EMAC) Copy all frames.
#define EMAC_NBC                      (1uL <<  5) // (EMAC) No broadcast.
#define EMAC_MTI                      (1uL <<  6) // (EMAC) Multicast hash event enable
#define EMAC_UNI                      (1uL <<  7) // (EMAC) Unicast hash enable.
#define EMAC_BIG                      (1uL <<  8) // (EMAC) Receive 1522 bytes.
#define EMAC_EAE                      (1uL <<  9) // (EMAC) External address match enable.
#define EMAC_CLK                      (3uL << 10) // (EMAC)
#define EMAC_RTY                      (1uL << 12) // (EMAC)
#define EMAC_PAE                      (1uL << 13) // (EMAC)
#define EMAC_RBOF                     (3uL << 14) // (EMAC)
#define EMAC_RLCE                     (1uL << 16) // (EMAC) Receive Length field Checking Enable
#define EMAC_DRFCS                    (1uL << 17) // (EMAC) Discard Receive FCS
#define EMAC_EFRHD                    (1uL << 18) // (EMAC)
#define EMAC_IRXFCS                   (1uL << 19) // (EMAC) Ignore RX FCS
//
// EMAC_NCR : (EMAC Offset: 0x0)
//
#define EMAC_RE                       (1uL <<  2) // (EMAC) Receive enable.
#define EMAC_TE                       (1uL <<  3) // (EMAC) Transmit enable.
#define EMAC_BPRESSURE                (1uL <<  8) // (EMAC) back pressure
#define EMAC_TSTART                   (1uL <<  9) // (EMAC) Start Transmission.
//
// EMAC_NSR : (EMAC Offset: 0x8) Network Status Register
//
#define EMAC_LINKR                    (1uL <<  0) // (EMAC)
#define EMAC_MDIO                     (1uL <<  1) // (EMAC)
#define EMAC_IDLE                     (1uL <<  2) // (EMAC)
//
// EMAC_TSR : (EMAC Offset: 0x14) Transmit Status Register
//
#define EMAC_UBR                      (1uL <<  0) // (EMAC)
#define EMAC_COL                      (1uL <<  1) // (EMAC)
#define EMAC_RLES                     (1uL <<  2) // (EMAC)
#define EMAC_TGO                      (1uL <<  3) // (EMAC) Transmit Go
#define EMAC_BEX                      (1uL <<  4) // (EMAC) Buffers exhausted mid frame
#define EMAC_COMP                     (1uL <<  5) // (EMAC)
#define EMAC_UND                      (1uL <<  6) // (EMAC)
//
// EMAC_RSR : (EMAC Offset: 0x20) Receive Status Register
//
#define EMAC_BNA                      (1uL <<  0)
#define EMAC_REC                      (1uL <<  1)
#define EMAC_OVR                      (1uL <<  2) // (EMAC)
//
// EMAC_ISR : (EMAC Offset: 0x24) Interrupt Status Register
//
#define EMAC_MFD                      (1uL <<  0) // (EMAC)
#define EMAC_RCOMP                    (1uL <<  1) // (EMAC)
#define EMAC_RXUBR                    (1uL <<  2) // (EMAC)
#define EMAC_TXUBR                    (1uL <<  3) // (EMAC)
#define EMAC_TUNDR                    (1uL <<  4) // (EMAC)
#define EMAC_RLEX                     (1uL <<  5) // (EMAC)
#define EMAC_TXERR                    (1uL <<  6) // (EMAC)
#define EMAC_TCOMP                    (1uL <<  7) // (EMAC)
#define EMAC_LINK                     (1uL <<  9) // (EMAC)
#define EMAC_ROVR                     (1uL << 10) // (EMAC)
#define EMAC_HRESP                    (1uL << 11) // (EMAC)
#define EMAC_PFRE                     (1uL << 12) // (EMAC)
#define EMAC_PTZ                      (1uL << 13) // (EMAC)

#define  EMAC_RXBUF_ADDRESS_MASK      (0xFFFFFFFC)        // Addr of Rx Buffer Descriptor Buf's
#define  EMAC_RXBUF_ADD_WRAP          (1 <<  1)           // Last buffer in the ring.
#define  EMAC_RXBUF_SW_OWNED          (1 <<  0)           // Software owns the buffer.
#define  EMAC_RXBUF_SOF_MASK          (1 << 14)           // Start of frame mask
#define  EMAC_RXBUF_EOF_MASK          (1 << 15)           // End of frame mask

#define  EMAC_TXBUF_ADD_WRAP          (1 << 30)           // Mark last buffer in the ring
#define  EMAC_TXBUF_ADD_LAST          (0x01 << 15)        // This is the last buffer for the current frame
#define  EMAC_TXBUF_STATUS_USED       (1uL << 31)

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
//None

/*********************************************************************
*
*       Static data
*
**********************************************************************
*/
static U8   _IsInited;
//
// PHY
//
static IP_PHY_CONTEXT         _PHY_Context;
static const IP_PHY_DRIVER  * _PHY_pDriver = &IP_PHY_Generic;
//
// Rx control variables
//
static BUFFER_DESC  * _paRxBufDesc;              // Pointer to Rx buffer descriptors
static U32          * _paRxBuffer;               // Pointer to Rx buffers. Should be 16-byte aligned.
static U16            _iNextRx;
static U16            _NumRxBuffers  = _NUM_RX_BUFFERS;
//
// Tx control variables
//
static char          _TxIsBusy;
static void*         _pTxBuffer     = (void*) _TX_MEM_START_ADDR;
static BUFFER_DESC*  _pTxBufferDesc = (BUFFER_DESC*) ((_TX_MEM_START_ADDR + _TX_BUFFER_SIZE + 7) &~0x07);

/****** Statistics **************************************************/

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
              | (2uL      << 16); // Code, must be b10
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
  l  = l ^ (l >> 24) ^ (h << 8);   // Fold 48 bits to 24 bits
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
  U32 v;
  U32 w;
  unsigned i;
  unsigned NumAddr;
  const U8 * pAddrData;
  U32 aHashFilter[2];
  U32 * pAddrFilter;
  char EnableHashUC;
  char EnableHashMC;

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
    v         = IP_LoadU32LE(pAddrData);     // Lower (first) 32 bits
    w         = IP_LoadU16LE(pAddrData + 4); // Upper (last)  16 bits

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
  v  =  EMAC_NCFGR;
  v &= ~(EMAC_UNI | EMAC_MTI);
  v |= EnableHashUC * EMAC_UNI + EnableHashMC * EMAC_MTI;
  EMAC_NCFGR = v;
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
  v  = EMAC_NCFGR;
  v &= ~(EMAC_SPD | EMAC_FD);
  if (Duplex == IP_DUPLEX_FULL) {
    v |= EMAC_FD;
  }
  if (Speed ==  IP_SPEED_100MHZ) {
    v |= EMAC_SPD;
  }
  EMAC_NCFGR = v;
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
    _UpdateEMACSettings (Duplex, Speed);              // Inform the EMAC about the current PHY settings
  }
}

/**********************************************************************************************************
*
*       _InitBufferDescs()
*
*  Function description:
*    Initializes the Rx buffer descriptors
*/
static void _InitBufferDescs(void) {
  BUFFER_DESC * pBufferDesc;
  U32 DataAddr;
  int i;

  //
  // Build buffer descriptors in allocated SDRAM
  //
  DataAddr    = (U32)_paRxBuffer;   // physical Addr of first buffer
  pBufferDesc = (BUFFER_DESC*) _paRxBufDesc;
  //
  // Initialize Rx buffer descriptors
  //
  for (i = 0; i < _NumRxBuffers; i++) {
    pBufferDesc->Addr   = DataAddr;
    pBufferDesc->Stat   = 0;
    DataAddr   += 128;
    pBufferDesc++;
  }
  (pBufferDesc - 1)->Addr |= EMAC_RXBUF_ADD_WRAP;    // Set the Wrap bit on the last descriptor
  _iNextRx = 0;                                      // Initialze access to first buffer

  EMAC_RBQP  = ((U32)_paRxBufDesc);
}

/**********************************************************************************************************
*
*       _ResetRxError()
*
*  Function description:
*    Resets the receiver logic in case of Fatal Rx errors (not simple packet corruption like CRC error)
*/
static void _ResetRxError(void) {
  EMAC_NCR &= ~EMAC_RE;       // Disable receiver
  _InitBufferDescs();
  EMAC_NCR |=  EMAC_RE;       // Re-enable receiver
}

/**********************************************************************************************************
*
*       _AllocBufferDescs()
*
*  Function description:
*    Allocates memory for Rx buffers & Rx descriptors
*/
static void _AllocBufferDescs(void) {
  U8 * pMem;

  //
  // Alloc memory for rx buffers. Make sure that addr. is 32 byte aligned, since only complete cache lines can be invalidated.
  //
  pMem = (U8*)IP_Alloc(_NumRxBuffers * 128 + 28);   // Alloc n* 128 bytes + space for aligment
  pMem = (U8*)_Align(pMem, 32);
  _paRxBuffer = (U32*) pMem;
  //
  // Buffer descriptors are located in internal SRAM0, behind Tx buffer and descriptor. This is required to avoid cache problems with descriptors being read and written by DMA.
  //
  _paRxBufDesc = _pTxBufferDesc + _NUM_TX_BUFFERS;
  if ((U32)(_paRxBufDesc + _NumRxBuffers) > (_TX_MEM_START_ADDR + _TX_MEM_SIZE)) {
    IP_PANIC("Too little RAM for config.");
  }
}

/*********************************************************************
*
*       _FreeRxBuffers
*
*  Function description
*    Frees the specified Rx buffers.
*/
static void _FreeRxBuffers(int NumBuffers) {
  BUFFER_DESC *pBufDesc;
  int i;

  _iNextRx += NumBuffers;
  if (_iNextRx >= _NumRxBuffers) {
    _iNextRx -= _NumRxBuffers;
  }
  i = _iNextRx;
  do {
    if (--i < 0) {
      i += _NumRxBuffers;
    }
    pBufDesc = _paRxBufDesc + i;
    pBufDesc->Addr &= ~EMAC_RXBUF_SW_OWNED;
  } while (--NumBuffers);
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

  IP_GetNextOutPacket(&pPacket, &NumBytes);
  if (NumBytes == 0) {
    _TxIsBusy = 0;
    return 0;
  }
  IP_LOG((IP_MTYPE_DRIVER, "DRIVER: Sending packet: %d bytes", NumBytes));
  INC(_DriverStats.TxSendCnt);
  //
  // Perform checks in debug build
  //
#if IP_DEBUG
{
  U32 v;
  v = EMAC_TSR;
  if (v & EMAC_TGO) {
    IP_PANIC("Transmitter is busy, but flag indicates that it should not.");
  }
  if ((_pTxBufferDesc->Stat & EMAC_TXBUF_STATUS_USED) == 0) {
    IP_PANIC("No Tx descriptor available");
  }
}
#endif
  //
  // We always copy into local tx buffer, to avoid matrix arbitration problem in AT91SAM9260
  //
  IP_MEMCPY(_pTxBuffer, pPacket, NumBytes);
  //
  // Clear all transmitter errors.
  //
  EMAC_TSR = EMAC_UBR
           | EMAC_COL
           | EMAC_RLES
           | EMAC_BEX
           | EMAC_COMP
           | EMAC_UND
           ;
  //
  // Start transmission by modifying the descriptor and telling EMAC to start
  //
  _pTxBufferDesc->Addr = (U32)_pTxBuffer;
  _pTxBufferDesc->Stat = EMAC_TXBUF_ADD_LAST | EMAC_TXBUF_ADD_WRAP | NumBytes;
  EMAC_NCR |= EMAC_TSTART;                          // Start the transmission
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
  volatile U32 ISR_Status;
  U32 RSR_Status;
  U32 TSR_Status;

  ISR_Status = EMAC_ISR;   // Clears the bits in the ISR status register, therefore required!
  RSR_Status = EMAC_RSR;
  TSR_Status = EMAC_TSR;
  EMAC_RSR   = RSR_Status; // Clear Receive Status Register
  EMAC_TSR   = TSR_Status; // Clear Transmit Status Register
  //
  // Handle Rx errors first, then handle reception
  //
  if (RSR_Status & (EMAC_BNA | EMAC_OVR)) {
    INC(_DriverStats.RxErrCnt);
    if (RSR_Status & EMAC_BNA) {
      IP_WARN_INTERNAL((IP_MTYPE_DRIVER, "DRIVER: No Rx DMA buffers available"));
    }
    if (RSR_Status & EMAC_OVR) {
      IP_WARN_INTERNAL((IP_MTYPE_DRIVER, "DRIVER: DMA could not access memory"));
    }
    _ResetRxError();
  } else if (RSR_Status & EMAC_REC) { // Did we recieve a frame ?
    INC(_DriverStats.RxIntCnt);
    IP_OnRx();
  }
  //
  // Finally check transmission state
  //
  if (TSR_Status & EMAC_COMP) {       // Frame completly sent ?
    if (_TxIsBusy) {
      IP_RemoveOutPacket();
      INC(_DriverStats.TxIntCnt);
      _SendPacket();
    } else {
      IP_WARN((IP_MTYPE_DRIVER, "DRIVER: Tx complete interrupt, but no packet sent."));
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
  AIC_IDCR                 =  1uL << ID_EMAC;     // Ensure interrupts are disabled before init
  *(volatile U32*)(_AIC_SVR_BASE_ADDR + 4 * ID_EMAC)   = (U32)_ISR_Handler;
  *(volatile U32*)(_AIC_SMR_BASE_ADDR + 4 * ID_EMAC)   =  AIC_SRCTYPE_INT_HIGH_LEVEL | AIC_PRIOR_LOWEST;
  AIC_ICCR                 =  1uL << ID_EMAC;
  EMAC_IDR                =  0x3FFF;                    // Disable all EMAC interrupts
  AIC_IECR                 =  1uL << ID_EMAC;     // Enable the AIC EMAC Interrupt
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
  PMC_PCER    = (1 << ID_EMAC);             // Enable the peripheral clock for the EMAC
  EMAC_NCR    = 0;                          // Disable Rx, Tx (Should be RESET state, to be safe)
  BSP_ETH_Init(0);
  EMAC_NCR    = (1 << 4);                   // Enable MDIO,
  EMAC_NCFGR  = (0 << 8)                    // 1: BIG: Allow packets of up to 1536 bytes
              | (3 << 10)                   // (EMAC) HCLK divided by 64. This works up to 160 MHz, and communication is not time-critical.
              | (2 << 14)                   // RBOF: Receive Buffer Offset. 2 is best so that the IP-part is aligned.
              | (1 << 16)                   // RLCE: Receive Length field Checking Enable
              | (1 << 17)                   // DRFCS: FCS field of received frames are not be copied to memory.
              ;
  //
  // Init PIO, PHY and update link state
  //
  _PHY_pDriver->pfInit(&_PHY_Context);      // Configure the PHY
#if _RX_BUFFERS_LOCATED_IN_SDRAM
  //
  // Initialize priority of BUS MATRIX. EMAC needs highest priority for SDRAM access
  //
  MATRIX_SCFG3 = 0x01160030;                // Assign EMAC as default master, activate priority arbitration, increase cycles
  MATRIX_PRAS3 = 0x00320000;                // Set Priority of EMAC to 3 (highest value)
#endif
  EMAC_USRIO = 2                            // Clock enable
             | _PHY_Context.UseRMII;        // Bit0 : 0: MII, 1: RMII
  //
  // Initialize buffers and Rx buffer descriptors
  //
  _AllocBufferDescs();
  _InitBufferDescs();
  //
  // Initialize Tx buffer descriptor
  //
  _pTxBufferDesc->Stat  = EMAC_TXBUF_STATUS_USED |  EMAC_TXBUF_ADD_WRAP;
  //
  // Set start addr of descriptors for both, rx and tx
  //
  EMAC_RBQP  = (U32) _paRxBufDesc;
  EMAC_TBQP  = (U32) _pTxBufferDesc;
  _UpdateLinkState();
  //
  // Enable interrupts
  //
  EMAC_RSR    =    (EMAC_OVR | EMAC_REC | EMAC_BNA);   // Clear receive status register
  _IntInit();
  EMAC_NCR   |= EMAC_TE;          // EnableTx
  EMAC_NCR   |= EMAC_RE;          // EnableRx
  EMAC_IER   |= EMAC_RCOMP        // Enable 'Reception complete' interrupt.
                    | EMAC_ROVR         // Enable 'Receiver overrun' interrupt.
                    | EMAC_RXUBR
                    | EMAC_TCOMP        // Enable Tx complete and Transmit bit used Intr's
                    | EMAC_TXUBR
                    ;
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
*    Number of buffers used for the next packet.
*    0 if no complete packet is available.
*/
static int _GetPacketSize(unsigned Unit) {
  BUFFER_DESC * pBufDesc;
  U32 Addr;
  U32 Stat;
  int i;
  int j;
  int NumBuffers;
  int PacketSize;

  IP_USE_PARA(Unit);
Start:
  i = _iNextRx;
  NumBuffers = 1;
  do {
    if (i >= _NumRxBuffers) {
      i -= _NumRxBuffers;
    }
    pBufDesc = _paRxBufDesc + i;
    Addr     = pBufDesc->Addr;
    Stat     = pBufDesc->Stat;
    if ((Addr & EMAC_RXBUF_SW_OWNED) == 0) {
      return 0;        //  This happens all the time since the stack polls until we are out of packets
    }
    if (NumBuffers == 1) {
      if ((Stat & EMAC_RXBUF_SOF_MASK) == 0) {
        goto Error;
      }
    } else {
      if (Stat & EMAC_RXBUF_SOF_MASK) {
        //
        // Error, a packet with FCS error has been received. We need to eliminate the fragment from RxBuffer
        //
        _FreeRxBuffers(NumBuffers);
        goto Start;
      }
    }
    if (Stat & EMAC_RXBUF_EOF_MASK) {
      // FoundPacket
      PacketSize = Stat & 0xFFF;         // Packet size is lower 12 bits of Status.
      j = (PacketSize + 127 + 2) >> 7;   // Compute the number of buffers for a packet of this size. Note: The first buffer holds only 126 bytes!
      if (j != NumBuffers) {
        goto Error;
      }
      return PacketSize;
    }
    i++;
  } while (++NumBuffers < _NumRxBuffers);
Error:
  //
  // Error: EMAC seems to have created confusion.
  // We will clean up with the error interrupt.
  //
  _ResetRxError();
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
*  Notes
*    (1)  Only complete cache lines can be invalidated. This requires, that the base address of the memory area has to be located at a
*         32 byte boundary and the number of bytes to be invalidated has to be a multiple of 32 bytes.
*
*    (2)  The data is in one ore more hardware buffers of 128 bytes each.
*         Since all buffers are adjacent, all of the data can be copied in either on or 2 chunks (if buffer end is reached and data wraps around)
*/
static int _ReadPacket(unsigned Unit, U8 *pDest, unsigned NumBytes) {
  U8 * pSrc;
  U8 * pInvalidate;
  unsigned NumBytesAtOnce;
  unsigned NumBytesToInvalidate;
  U32 Addr;
  int i;
  int NumBuffers;

  NumBuffers = (NumBytes + 127 + 2) >> 7;
  if (pDest) {
    BUFFER_DESC * pBufDesc;
    i              = _iNextRx;
    pBufDesc       = _paRxBufDesc + i;
    Addr           = pBufDesc->Addr;
    pSrc           = (U8*)(Addr & EMAC_RXBUF_ADDRESS_MASK) + 2;       // We initialized a receive buffer offset of 2 which has to be taken into account !
    NumBytesAtOnce = NumBytes;
    i             += NumBuffers;
    if (i >= _NumRxBuffers) {
      i -= _NumRxBuffers;
      NumBytesAtOnce = (_NumRxBuffers - _iNextRx) * 128 - 2;  // We initialized a receive buffer offset of 2 which has to be taken into account !
      if (NumBytesAtOnce > NumBytes) {
        NumBytesAtOnce = NumBytes;
      }
    }
    //
    // Refer to Note 2
    //
    pInvalidate = (U8*)((unsigned)pSrc & ~3);                       // Mask out bit0 and bit1 because of the 32 byte boundary
    NumBytesToInvalidate = (NumBytesAtOnce + 2 + 31) & ~0x1F;       // NumBytesAtOnce + offset of the first Rx buffer + space for aligment
    BSP_CACHE_InvalidateRange(pInvalidate, NumBytesToInvalidate);
    //
    // Copy first chunk of data (Note 1)
    //
    IP_MEMCPY(pDest, pSrc, NumBytesAtOnce);
    //
    // If we have reached the end of the buffer array, we may have to copy the rest from start of buffer
    //
    if (NumBytes > NumBytesAtOnce) {
      pDest    += NumBytesAtOnce;
      NumBytes -= NumBytesAtOnce;
      pBufDesc  = _paRxBufDesc;
      Addr      = pBufDesc->Addr;
      pSrc      = (U8*)(Addr & EMAC_RXBUF_ADDRESS_MASK);
      NumBytesToInvalidate = (NumBytes + 31) & ~0x1F;               // NumBytes + space for aligment
      BSP_CACHE_InvalidateRange(pSrc, NumBytesToInvalidate);
      IP_MEMCPY(pDest, pSrc, NumBytes);
    }
  }
  if (pDest) {
    IP_LOG((IP_MTYPE_DRIVER, "Packet: %d Bytes --- Read", NumBytes));
  } else {
    IP_LOG((IP_MTYPE_DRIVER, "Packet: %d Bytes --- Discarded", NumBytes));
  }
  INC(_DriverStats.RxCnt);
  //
  // Free packet
  //
  _FreeRxBuffers(NumBuffers);
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
    EMAC_NCR |= EMAC_BPRESSURE;
    return 0;
  case IP_NI_CMD_CLR_BPRESSURE:
    EMAC_NCR &= ~EMAC_BPRESSURE;
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
*       IP_NI_CAP9_ConfigNumRxBuffers
*
*  Function description
*    Sets the number of Rx Buffers in the config phase.
*/
void IP_NI_CAP9_ConfigNumRxBuffers(U16 NumRxBuffers) {
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
const IP_HW_DRIVER IP_Driver_CAP9 = {
  _Init,
  _SendPacketIfTxIdle,
  _GetPacketSize,
  _ReadPacket,
  _Timer,
  _Control
};

/*************************** End of file ****************************/
