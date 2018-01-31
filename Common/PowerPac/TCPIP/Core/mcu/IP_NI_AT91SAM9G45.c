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
File    : IP_NI_AT91SAM9G45.c
Purpose : NI driver for AT91SAM9G45
          Driver defaults to use the built-in SRAM for Tx buffers,
          Tx descriptors and Rx descriptors.
          SDRAM is used for Rx buffers. The descriptors should remain in SRAM. This
          avoids problems with the cache and the required alignment
          in case of cache invalidation.
--------  END-OF-HEADER  ---------------------------------------------
*/

#include "IP_Int.h"
#include "BSP.h"
#include "IP_NI_AT91SAM9G45.h"

/*********************************************************************
*
*       Defines, configurable
*
**********************************************************************
*/

#define _NUM_RX_BUFFERS       (3 * 12 + 1)  // One buffer is 128 bytes.
                                            // 12 is enough for 1 large packets.
                                            // One extra to allow n large packets.

#if 1
  #define _USE_RMII           (1)           // Use RMII interface to PHY
#else
  #define _USE_RMII           (0)           // Use MII interface to PHY
#endif

#define _TX_BUFFER_SIZE     (1536)
#define _TX_MEM_START_ADDR  (0x300000)      // Internal SRAM, original address
#define _TX_MEM_SIZE        (0x4000)        // Internal SRAM is 16KB
#define _NUM_TX_BUFFERS     (1)

/*********************************************************************
*
*       Defines, non configurable
*
**********************************************************************
*/

/*********************************************************************
*
*       AT91SAM9G45 peripheral addresses, sfrs and related control bits
*/

// ========== Register definition for RSTC peripheral ==========
#define AT91C_RSTC_BASE_ADDR                (0xFFFFFD00)
#define AT91C_RSTC_RSR  (*(volatile U32*)   (AT91C_RSTC_BASE_ADDR + 0x04)) // (RSTC) Reset Status Register
#define AT91C_RSTC_RMR  (*(volatile U32*)   (AT91C_RSTC_BASE_ADDR + 0x08)) // (RSTC) Reset Mode Register
#define AT91C_RSTC_RCR  (*(volatile U32*)   (AT91C_RSTC_BASE_ADDR + 0x00)) // (RSTC) Reset Control Register

#define AT91C_RSTC_ERSTL          (0xF <<  8) // (RSTC) User Reset Length
#define AT91C_RSTC_EXTRST         (0x1 <<  3) // (RSTC) External Reset
#define AT91C_RSTC_NRSTL          (0x1 << 16) // (RSTC) NRST pin level

// ========== Register definition for AIC peripheral ==========
#define AIC_BASE_ADDR      (0xfffff000)
#define AIC_SMR_BASE_ADDR  (AIC_BASE_ADDR + 0x00)
#define AIC_SVR_BASE_ADDR  (AIC_BASE_ADDR + 0x80)
#define AT91C_AIC_ICCR  (*(volatile U32*)   (AIC_BASE_ADDR + 0x128)) // (AIC) Interrupt Clear Command Register
#define AT91C_AIC_IECR  (*(volatile U32*)   (AIC_BASE_ADDR + 0x120)) // (AIC) Interrupt Enable Command Register
#define AT91C_AIC_SMR   (*(volatile U32*)   (AIC_BASE_ADDR + 0x000)) // (AIC) Source Mode Register
#define AT91C_AIC_IDCR  (*(volatile U32*)   (AIC_BASE_ADDR + 0x124)) // (AIC) Interrupt Disable Command Register
#define AT91C_AIC_SVR   (*(volatile U32*)   (AIC_BASE_ADDR + 0x080)) // (AIC) Source Vector Register

// -------- AIC_SMR : (AIC Offset: 0x0) Control Register --------
#define AT91C_AIC_PRIOR                     ((unsigned int) 0x7 <<  0) // (AIC) Priority Level
#define AT91C_AIC_PRIOR_LOWEST              ((unsigned int) 0x0)       // (AIC) Lowest priority level
#define AT91C_AIC_SRCTYPE_INT_HIGH_LEVEL    ((unsigned int) 0x0 <<  5) // (AIC) Internal Sources Code Label High-level Sensitive

// ========== Register definition for PMC peripheral ==========
#define AT91C_PMC_PCER   (*(volatile U32*)   0xFFFFFC10) // (PMC) Peripheral Clock Enable Register

/****** Peripheral ID definitions for AT91SAM9G45 *******************/
#define AT91C_ID_PIOA   ((unsigned int)  2)
#define AT91C_ID_PIOB   ((unsigned int)  3)
#define AT91C_ID_PIOCDE ((unsigned int)  4) // Parallel I/O Controller C, D and E
#define AT91C_ID_EMAC   ((unsigned int) 25) // Ethernet MAC

// ========== Register definition for EMAC peripheral ==========
#define EMAC_BASE_ADDR (0xFFFBC000)

#define AT91C_EMAC_TID    (*(volatile U32 *)  (EMAC_BASE_ADDR + 0x0B8)) // (EMAC) Type ID Checking Register
#define AT91C_EMAC_SA3L   (*(volatile U32 *)  (EMAC_BASE_ADDR + 0x0A8)) // (EMAC) Specific Address 3 Bottom, First 4 bytes
#define AT91C_EMAC_STE    (*(volatile U32 *)  (EMAC_BASE_ADDR + 0x084)) // (EMAC) SQE Test Error Register
#define AT91C_EMAC_RSE    (*(volatile U32 *)  (EMAC_BASE_ADDR + 0x074)) // (EMAC) Receive Symbol Errors Register
#define AT91C_EMAC_IDR    (*(volatile U32 *)  (EMAC_BASE_ADDR + 0x02C)) // (EMAC) Interrupt Disable Register
#define AT91C_EMAC_TBQP   (*(volatile U32 *)  (EMAC_BASE_ADDR + 0x01C)) // (EMAC) Transmit Buffer Queue Pointer
#define AT91C_EMAC_TPQ    (*(volatile U32 *)  (EMAC_BASE_ADDR + 0x0BC)) // (EMAC) Transmit Pause Quantum Register
#define AT91C_EMAC_SA1L   (*(volatile U32 *)  (EMAC_BASE_ADDR + 0x098)) // (EMAC) Specific Address 1 Bottom, First 4 bytes
#define AT91C_EMAC_RLE    (*(volatile U32 *)  (EMAC_BASE_ADDR + 0x088)) // (EMAC) Receive Length Field Mismatch Register
#define AT91C_EMAC_IMR    (*(volatile U32 *)  (EMAC_BASE_ADDR + 0x030)) // (EMAC) Interrupt Mask Register
#define AT91C_EMAC_SA1H   (*(volatile U32 *)  (EMAC_BASE_ADDR + 0x09C)) // (EMAC) Specific Address 1 Top, Last 2 bytes
#define AT91C_EMAC_PFR    (*(volatile U32 *)  (EMAC_BASE_ADDR + 0x03C)) // (EMAC) Pause Frames received Register
#define AT91C_EMAC_FCSE   (*(volatile U32 *)  (EMAC_BASE_ADDR + 0x050)) // (EMAC) Frame Check Sequence Error Register
#define AT91C_EMAC_FTO    (*(volatile U32 *)  (EMAC_BASE_ADDR + 0x040)) // (EMAC) Frames Transmitted OK Register
#define AT91C_EMAC_TUND   (*(volatile U32 *)  (EMAC_BASE_ADDR + 0x064)) // (EMAC) Transmit Underrun Error Register
#define AT91C_EMAC_ALE    (*(volatile U32 *)  (EMAC_BASE_ADDR + 0x054)) // (EMAC) Alignment Error Register
#define AT91C_EMAC_SCF    (*(volatile U32 *)  (EMAC_BASE_ADDR + 0x044)) // (EMAC) Single Collision Frame Register
#define AT91C_EMAC_SA3H   (*(volatile U32 *)  (EMAC_BASE_ADDR + 0x0AC)) // (EMAC) Specific Address 3 Top, Last 2 bytes
#define AT91C_EMAC_ELE    (*(volatile U32 *)  (EMAC_BASE_ADDR + 0x078)) // (EMAC) Excessive Length Errors Register
#define AT91C_EMAC_CSE    (*(volatile U32 *)  (EMAC_BASE_ADDR + 0x068)) // (EMAC) Carrier Sense Error Register
#define AT91C_EMAC_DTF    (*(volatile U32 *)  (EMAC_BASE_ADDR + 0x058)) // (EMAC) Deferred Transmission Frame Register
#define AT91C_EMAC_RSR    (*(volatile U32 *)  (EMAC_BASE_ADDR + 0x020)) // (EMAC) Receive Status Register
#define AT91C_EMAC_USRIO  (*(volatile U32 *)  (EMAC_BASE_ADDR + 0x0C0)) // (EMAC) USER Input/Output Register
#define AT91C_EMAC_SA4L   (*(volatile U32 *)  (EMAC_BASE_ADDR + 0x0B0)) // (EMAC) Specific Address 4 Bottom, First 4 bytes
#define AT91C_EMAC_RRE    (*(volatile U32 *)  (EMAC_BASE_ADDR + 0x06C)) // (EMAC) Receive Ressource Error Register
#define AT91C_EMAC_RJA    (*(volatile U32 *)  (EMAC_BASE_ADDR + 0x07C)) // (EMAC) Receive Jabbers Register
#define AT91C_EMAC_TPF    (*(volatile U32 *)  (EMAC_BASE_ADDR + 0x08C)) // (EMAC) Transmitted Pause Frames Register
#define AT91C_EMAC_ISR    (*(volatile U32 *)  (EMAC_BASE_ADDR + 0x024)) // (EMAC) Interrupt Status Register
#define AT91C_EMAC_MAN    (*(volatile U32 *)  (EMAC_BASE_ADDR + 0x034)) // (EMAC) PHY Maintenance Register
#define AT91C_EMAC_WOL    (*(volatile U32 *)  (EMAC_BASE_ADDR + 0x0C4)) // (EMAC) Wake On LAN Register
#define AT91C_EMAC_USF    (*(volatile U32 *)  (EMAC_BASE_ADDR + 0x080)) // (EMAC) Undersize Frames Register
#define AT91C_EMAC_HRB    (*(volatile U32 *)  (EMAC_BASE_ADDR + 0x090)) // (EMAC) Hash Address Bottom[31:0]
#define AT91C_EMAC_PTR    (*(volatile U32 *)  (EMAC_BASE_ADDR + 0x038)) // (EMAC) Pause Time Register
#define AT91C_EMAC_HRT    (*(volatile U32 *)  (EMAC_BASE_ADDR + 0x094)) // (EMAC) Hash Address Top[63:32]
#define AT91C_EMAC_REV    (*(volatile U32 *)  (EMAC_BASE_ADDR + 0x0FC)) // (EMAC) Revision Register
#define AT91C_EMAC_MCF    (*(volatile U32 *)  (EMAC_BASE_ADDR + 0x048)) // (EMAC) Multiple Collision Frame Register
#define AT91C_EMAC_SA2L   (*(volatile U32 *)  (EMAC_BASE_ADDR + 0x0A0)) // (EMAC) Specific Address 2 Bottom, First 4 bytes
#define AT91C_EMAC_NCR    (*(volatile U32 *)  (EMAC_BASE_ADDR + 0x000)) // (EMAC) Network Control Register
#define AT91C_EMAC_FRO    (*(volatile U32 *)  (EMAC_BASE_ADDR + 0x04C)) // (EMAC) Frames Received OK Register
#define AT91C_EMAC_LCOL   (*(volatile U32 *)  (EMAC_BASE_ADDR + 0x05C)) // (EMAC) Late Collision Register
#define AT91C_EMAC_SA4H   (*(volatile U32 *)  (EMAC_BASE_ADDR + 0x0B4)) // (EMAC) Specific Address 4 Top, Last 2 bytes
#define AT91C_EMAC_NCFGR  (*(volatile U32 *)  (EMAC_BASE_ADDR + 0x004)) // (EMAC) Network Configuration Register
#define AT91C_EMAC_TSR    (*(volatile U32 *)  (EMAC_BASE_ADDR + 0x014)) // (EMAC) Transmit Status Register
#define AT91C_EMAC_SA2H   (*(volatile U32 *)  (EMAC_BASE_ADDR + 0x0A4)) // (EMAC) Specific Address 2 Top, Last 2 bytes
#define AT91C_EMAC_ECOL   (*(volatile U32 *)  (EMAC_BASE_ADDR + 0x060)) // (EMAC) Excessive Collision Register
#define AT91C_EMAC_ROV    (*(volatile U32 *)  (EMAC_BASE_ADDR + 0x070)) // (EMAC) Receive Overrun Errors Register
#define AT91C_EMAC_NSR    (*(volatile U32 *)  (EMAC_BASE_ADDR + 0x008)) // (EMAC) Network Status Register
#define AT91C_EMAC_RBQP   (*(volatile U32 *)  (EMAC_BASE_ADDR + 0x018)) // (EMAC) Receive Buffer Queue Pointer
#define AT91C_EMAC_IER    (*(volatile U32 *)  (EMAC_BASE_ADDR + 0x028)) // (EMAC) Interrupt Enable Register


// -------- EMAC_NCFGR : (EMAC Offset: 0x4) Network Configuration Register --------
#define AT91C_EMAC_SPD        (1UL <<  0) // (EMAC) Speed.
#define AT91C_EMAC_FD         (1UL <<  1) // (EMAC) Full duplex.
#define AT91C_EMAC_CAF        (1UL <<  4) // (EMAC) Copy all frames.
#define AT91C_EMAC_NBC        (1UL <<  5) // (EMAC) No broadcast.
#define AT91C_EMAC_MTI        (1UL <<  6) // (EMAC) Multicast hash event enable
#define AT91C_EMAC_UNI        (1UL <<  7) // (EMAC) Unicast hash enable.
#define AT91C_EMAC_BIG        (1UL <<  8) // (EMAC) Receive 1522 bytes.
#define AT91C_EMAC_EAE        (1UL <<  9) // (EMAC) External address match enable.
#define AT91C_EMAC_CLK        (3UL << 10) // (EMAC)
#define AT91C_EMAC_RTY        (1UL << 12) // (EMAC)
#define AT91C_EMAC_PAE        (1UL << 13) // (EMAC)
#define AT91C_EMAC_RBOF       (3UL << 14) // (EMAC)
#define AT91C_EMAC_RLCE       (1UL << 16) // (EMAC) Receive Length field Checking Enable
#define AT91C_EMAC_DRFCS      (1UL << 17) // (EMAC) Discard Receive FCS
#define AT91C_EMAC_EFRHD      (1UL << 18) // (EMAC)
#define AT91C_EMAC_IRXFCS     (1UL << 19) // (EMAC) Ignore RX FCS


// -------- EMAC_NCR : (EMAC Offset: 0x0)  --------
#define AT91C_EMAC_RE         (1UL <<  2) // (EMAC) Receive enable.
#define AT91C_EMAC_TE         (1UL <<  3) // (EMAC) Transmit enable.
#define AT91C_EMAC_BPRESSURE  (1UL <<  8) // (EMAC) back pressure
#define AT91C_EMAC_TSTART     (1UL <<  9) // (EMAC) Start Transmission.

// -------- EMAC_NSR : (EMAC Offset: 0x8) Network Status Register --------
#define AT91C_EMAC_LINKR      (1UL <<  0) // (EMAC)
#define AT91C_EMAC_MDIO       (1UL <<  1) // (EMAC)
#define AT91C_EMAC_IDLE       (1UL <<  2) // (EMAC)

// -------- EMAC_TSR : (EMAC Offset: 0x14) Transmit Status Register --------
#define AT91C_EMAC_UBR        (1UL <<  0) // (EMAC)
#define AT91C_EMAC_COL        (1UL <<  1) // (EMAC)
#define AT91C_EMAC_RLES       (1UL <<  2) // (EMAC)
#define AT91C_EMAC_TGO        (1UL <<  3) // (EMAC) Transmit Go
#define AT91C_EMAC_BEX        (1UL <<  4) // (EMAC) Buffers exhausted mid frame
#define AT91C_EMAC_COMP       (1UL <<  5) // (EMAC)
#define AT91C_EMAC_UND        (1UL <<  6) // (EMAC)

// -------- EMAC_RSR : (EMAC Offset: 0x20) Receive Status Register --------
#define AT91C_EMAC_BNA        (1UL <<  0)
#define AT91C_EMAC_REC        (1UL <<  1)
#define AT91C_EMAC_OVR        (1UL <<  2) // (EMAC)
// -------- EMAC_ISR : (EMAC Offset: 0x24) Interrupt Status Register --------
#define AT91C_EMAC_MFD        (1UL <<  0) // (EMAC)
#define AT91C_EMAC_RCOMP      (1UL <<  1) // (EMAC)
#define AT91C_EMAC_RXUBR      (1UL <<  2) // (EMAC)
#define AT91C_EMAC_TXUBR      (1UL <<  3) // (EMAC)
#define AT91C_EMAC_TUNDR      (1UL <<  4) // (EMAC)
#define AT91C_EMAC_RLEX       (1UL <<  5) // (EMAC)
#define AT91C_EMAC_TXERR      (1UL <<  6) // (EMAC)
#define AT91C_EMAC_TCOMP      (1UL <<  7) // (EMAC)
#define AT91C_EMAC_LINK       (1UL <<  9) // (EMAC)
#define AT91C_EMAC_ROVR       (1UL << 10) // (EMAC)
#define AT91C_EMAC_HRESP      (1UL << 11) // (EMAC)
#define AT91C_EMAC_PFRE       (1UL << 12) // (EMAC)
#define AT91C_EMAC_PTZ        (1UL << 13) // (EMAC)

#define  EMAC_RXBUF_ADDRESS_MASK    (0xFFFFFFFC)        // Addr of Rx Buffer Descriptor Buf's
#define  EMAC_RXBUF_ADD_WRAP        (1uL <<  1)         // Last buffer in the ring.
#define  EMAC_RXBUF_SW_OWNED        (1uL <<  0)         // Software owns the buffer.
#define  EMAC_RXBUF_SOF_MASK        (1uL << 14)         // Start of frame mask
#define  EMAC_RXBUF_EOF_MASK        (1uL << 15)         // End of frame mask

#define  EMAC_TXBUF_ADD_WRAP        (1uL << 30)         // Mark last buffer in the ring
#define  EMAC_TXBUF_ADD_LAST        (0x01uL << 15)      // This is the last buffer for the current frame
#define  EMAC_TXBUF_STATUS_USED     (1uL << 31)

/*********************************************************************
*
*       Check configuration
*
**********************************************************************
*/
#ifndef _USE_RMII
  #error "_USE_RMII has to be defined and set to 0 or 1"
#endif

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

static U8                     _PhyMode = _USE_RMII;

static IP_PHY_CONTEXT         _PHY_Context;
static const IP_PHY_DRIVER  * _PHY_pDriver = &IP_PHY_Generic;
static U8                     _IsInited;

/****** Rx control variables ****************************************/

static BUFFER_DESC  *_paRxBufDesc;              // Pointer to Rx buffer descriptors
static U32          *_paRxBuffer;               // Pointer to Rx buffers. Should be 16-byte aligned.
static U16           _iNextRx;
static U16           _NumRxBuffers = _NUM_RX_BUFFERS;

/****** Tx control variables ****************************************/

static char          _TxIsBusy;
static void*         _pTxBuffer     = (void*) _TX_MEM_START_ADDR;
static BUFFER_DESC*  _pTxBufferDesc = (BUFFER_DESC*) ((_TX_MEM_START_ADDR + _TX_BUFFER_SIZE + 7) &~0x07);

/****** Statistics **************************************************/

#if DEBUG
  static int _TxSendCnt;
  static int _TxIntCnt;
  static int _RxCnt;
  static int _RxIntCnt;
  static int _RxErrCnt;
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
  Addr = (U32)pMem;
  NumBytes -= 1;
  Addr += NumBytes;
  Addr &= ~NumBytes;
  return (void*)Addr;
}

/*********************************************************************
*
*       _PHY_WriteReg
*/
static void _PHY_WriteReg(IP_PHY_CONTEXT* pContext, unsigned RegIndex, unsigned v) {
  unsigned Addr;

  Addr = pContext->Addr;
  AT91C_EMAC_MAN    =   (1UL           << 30) // [31:30] SOF: Start of frame code (must be 0x01)
                      | (1UL           << 28) // [29:28] RW flags: b10 Read, b01 Write
                      | ((U32)Addr     << 23) // [27:23] 5 bit PhyAddr
                      | ((U32)RegIndex << 18) // [22:18] 5 bit RegisterIndex
                      | (2UL           << 16) // Code, must be b10
                      | v & 0xFFFF;
  //
  // Wait for command to finish
  //
  while ((AT91C_EMAC_NSR & AT91C_EMAC_IDLE) == 0);
}

/*********************************************************************
*
*       _PHY_ReadReg
*/
static unsigned _PHY_ReadReg(IP_PHY_CONTEXT* pContext, unsigned RegIndex) {
  unsigned r;
  unsigned Addr;

  Addr = pContext->Addr;
  AT91C_EMAC_MAN    =   (1UL           << 30)  // [31:30] SOF: Start of frame code (must be 0x01)
                      | (2UL           << 28)  // [29:28] RW flags: b10 Read, b01 Write
                      | ((U32)Addr     << 23)  // [27:23] 5 bit PhyAddr
                      | ((U32)RegIndex << 18)  // [22:18] 5 bit RegisterIndex
                      | (2UL           << 16); // Code, must be b10
  //
  // Wait for command to finish
  //
  while ((AT91C_EMAC_NSR & AT91C_EMAC_IDLE) == 0);
  //
  // Read and return data
  //
  r = AT91C_EMAC_MAN & 0x0000FFFF;
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
  l = l ^ (l >> 24) ^ (h << 8);   // Fold 48 bits to 24 bits
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

  EnableHashUC = 0;
  EnableHashMC = 0;
  aHashFilter[0] = 0;
  aHashFilter[1] = 0;
  NumAddr = pFilter->NumAddr;
  pAddrFilter = (U32*)&AT91C_EMAC_SA1L;
  //
  // Use the precise filters for the first 4 addresses, hash for the remaining ones
  //
  for (i = 0; i < NumAddr; i++) {
    pAddrData   = *(&pFilter->pHWAddr + i);
    v = IP_LoadU32LE(pAddrData);     // lower (first) 32 bits
    w = IP_LoadU16LE(pAddrData + 4);            // upper (last)  16 bits

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
  AT91C_EMAC_HRB = aHashFilter[0];
  AT91C_EMAC_HRT = aHashFilter[1];
  //
  // Update control register to allow hashing for Unicast/Multicast
  //
  v  =  AT91C_EMAC_NCFGR;
  v &= ~(AT91C_EMAC_UNI | AT91C_EMAC_MTI);
  v |= EnableHashUC * AT91C_EMAC_UNI + EnableHashMC * AT91C_EMAC_MTI;
  AT91C_EMAC_NCFGR = v;
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
  v  =  AT91C_EMAC_NCFGR;
  v &= ~(AT91C_EMAC_SPD | AT91C_EMAC_FD);
  if (Duplex == IP_DUPLEX_FULL) {
    v |= AT91C_EMAC_FD;
  }
  if (Speed ==  IP_SPEED_100MHZ) {
    v |= AT91C_EMAC_SPD;
  }
  AT91C_EMAC_NCFGR = v;
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
*/
static void _InitBufferDescs(void) {
  int     i;
  BUFFER_DESC  *pBufferDesc;
  U32 DataAddr;
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

  AT91C_EMAC_RBQP  = ((U32)_paRxBufDesc);
}

/**********************************************************************************************************
*
*       _ResetRxError()
*
*  Function description:
*    Resets the receiver logic in case of Fatal Rx errors (not simple packet corruption like CRC error)
*/
static void _ResetRxError(void) {
  AT91C_EMAC_NCR &= ~AT91C_EMAC_RE;       // Disable receiver
  _InitBufferDescs();
  AT91C_EMAC_NCR |=  AT91C_EMAC_RE;       // Re-enable receiver
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
  U32      v;
  void   * pPacket;
  unsigned NumBytes;

  IP_GetNextOutPacket(&pPacket, &NumBytes);
  if (NumBytes == 0) {
    _TxIsBusy = 0;
    return 0;
  }
  IP_LOG((IP_MTYPE_DRIVER, "DRIVER: Sending packet: %d bytes", NumBytes));
  INC(_TxSendCnt);
  //
  // Make sure transmitter is actually idle
  //
  v = AT91C_EMAC_TSR;
  if (v & AT91C_EMAC_TGO) {
    IP_PANIC("Transmitter is busy, but flag indicates that it should not.");
  }
  if ((_pTxBufferDesc->Stat & EMAC_TXBUF_STATUS_USED) == 0) {
    IP_PANIC("No Tx descriptor available");
  }
  //
  // We always copy into local tx buffer, to avoid matrix arbitration problem in AT91SAM9G45
  //
  IP_MEMCPY(_pTxBuffer, pPacket, NumBytes);
  //
  // Clear all transmitter errors.
  //
  AT91C_EMAC_TSR  = AT91C_EMAC_UBR
                  | AT91C_EMAC_COL
                  | AT91C_EMAC_RLES
                  | AT91C_EMAC_BEX
                  | AT91C_EMAC_COMP
                  | AT91C_EMAC_UND
                  ;
  //
  // Start transmission by modifying the descriptor and telling EMAC to start
  //
  _pTxBufferDesc->Addr = (U32)_pTxBuffer;
  _pTxBufferDesc->Stat = EMAC_TXBUF_ADD_LAST | EMAC_TXBUF_ADD_WRAP | NumBytes;
  AT91C_EMAC_NCR |= AT91C_EMAC_TSTART;                          // Start the transmission
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

  ISR_Status      = AT91C_EMAC_ISR;   // Clears the bits in the ISR status register, therefore required!
  RSR_Status      = AT91C_EMAC_RSR;
  TSR_Status      = AT91C_EMAC_TSR;
  AT91C_EMAC_RSR  = RSR_Status;       // Clear Receive Status Register
  AT91C_EMAC_TSR  = TSR_Status;       // Clear Transmit Status Register
  //
  // Handle Rx errors first, then handle reception
  //
  if (RSR_Status & (AT91C_EMAC_BNA | AT91C_EMAC_OVR)) {
    INC(_RxErrCnt);
    if (RSR_Status & AT91C_EMAC_BNA) {
      IP_WARN_INTERNAL((IP_MTYPE_DRIVER, "DRIVER: No Rx DMA buffers available"));
    }
    if (RSR_Status & AT91C_EMAC_OVR) {
      IP_WARN_INTERNAL((IP_MTYPE_DRIVER, "DRIVER: DMA could not access memory"));
    }
    _ResetRxError();
  } else if (RSR_Status & AT91C_EMAC_REC) { // Did we recieve a frame ?
    INC(_RxIntCnt);
    IP_OnRx();
  }
  //
  // Finally check transmission state
  //
  if (TSR_Status & AT91C_EMAC_COMP) {       // Frame completly sent ?
    if (_TxIsBusy) {
      IP_RemoveOutPacket();
      INC(_TxIntCnt);
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
  AT91C_AIC_IDCR                 =  1UL << AT91C_ID_EMAC;     // Ensure interrupts are disabled before init
  *(volatile U32*)(AIC_SVR_BASE_ADDR + 4 * AT91C_ID_EMAC) = (U32)_ISR_Handler;
  *(volatile U32*)(AIC_SMR_BASE_ADDR + 4 * AT91C_ID_EMAC) =  AT91C_AIC_SRCTYPE_INT_HIGH_LEVEL | AT91C_AIC_PRIOR_LOWEST;
  AT91C_AIC_ICCR                 =  1UL << AT91C_ID_EMAC;
  AT91C_EMAC_IDR                 =  0x3FFF;                   // Disable all EMAC interrupts
  AT91C_AIC_IECR                 =  1UL << AT91C_ID_EMAC;     // Enable the AIC EMAC Interrupt
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

  IP_USE_PARA(Unit);                           // Multiple units not supported, avoid warning

  _PHY_Context.pAccess = &_PHY_pAccess;       // PHY read/write functions are static in this module
  _PHY_Context.Addr    = IP_PHY_ADDR_ANY;     // Activate automatic scan for external PHY
  _PHY_Context.UseRMII = _PhyMode;            // Set the default value for RMII/MII mode;

  AT91C_PMC_PCER    = (1uL << AT91C_ID_EMAC); // Enable the peripheral clock for the EMAC
  AT91C_EMAC_NCR    = 0;                      // Disable Rx, Tx (Should be RESET state, to be safe)
  BSP_ETH_Init(0);
  AT91C_EMAC_NCR    = (1uL << 4);             // Enable MDIO,
  AT91C_EMAC_NCFGR  = (0uL << 8)              // 1: BIG: Allow packets of up to 1536 bytes
                    | (3uL << 10)             // (EMAC) HCLK divided by 64. This works up to 160 MHz, and communication is not time-critical.
                    | (2uL << 14)             // RBOF: Receive Buffer Offset. 2 is best so that the IP-part is aligned.
                    | (1uL << 16)             // RLCE: Receive Length field Checking Enable
                    | (1uL << 17)             // DRFCS: FCS field of received frames are not be copied to memory.
                    ;
  _PHY_pDriver->pfInit(&_PHY_Context);        // Configure the PHY

  if (_PHY_Context.UseRMII) {
    AT91C_EMAC_USRIO |= (1uL << 0)            // Enable RMII mode,
                      | (1uL << 1)            // Enable clock for interface to Phy
                      ;
  } else {
    AT91C_EMAC_USRIO &= ~(1uL << 0);          // Clear RMII mode
    AT91C_EMAC_USRIO |=  (1uL << 1);          // Enable clock for interface to Phy
  }
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
  AT91C_EMAC_RBQP  = (U32) _paRxBufDesc;
  AT91C_EMAC_TBQP  = (U32) _pTxBufferDesc;
  _UpdateLinkState();
  //
  // Enable interrupts
  //
  AT91C_EMAC_RSR    = (AT91C_EMAC_OVR | AT91C_EMAC_REC | AT91C_EMAC_BNA);   // Clear receive status register
  _IntInit();
  AT91C_EMAC_NCR   |= AT91C_EMAC_TE;          // EnableTx
  AT91C_EMAC_NCR   |= AT91C_EMAC_RE;          // EnableRx
  AT91C_EMAC_IER   |= AT91C_EMAC_RCOMP        // Enable 'Reception complete' interrupt.
                    | AT91C_EMAC_ROVR         // Enable 'Receiver overrun' interrupt.
                    | AT91C_EMAC_RXUBR
                    | AT91C_EMAC_TCOMP        // Enable Tx complete and Transmit bit used Intr's
                    | AT91C_EMAC_TXUBR
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
  IP_USE_PARA(Unit);           // Multiple units not supported, avoid warning
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
  U32 Addr;
  U32 Stat;
  BUFFER_DESC *pBufDesc;
  int i;
  int j;
  int NumBuffers;
  int PacketSize;

  IP_USE_PARA(Unit);  // Multiple units not supported, avoid warning

Start:
  i = _iNextRx;
  NumBuffers = 1;
  do {
    if (i >= _NumRxBuffers) {
      i -= _NumRxBuffers;
    }
    pBufDesc = _paRxBufDesc + i;
    Addr = pBufDesc->Addr;
    Stat = pBufDesc->Stat;
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
  U8     *pSrc;
  U8     *pInvalidate;
  unsigned NumBytesAtOnce;
  unsigned NumBytesToInvalidate;
  U32 Addr;
  int i;
  int NumBuffers;

  IP_USE_PARA(Unit);    // Multiple units not supported, avoid warning
  NumBuffers = (NumBytes + 127 + 2) >> 7;
  if (pDest) {
    BUFFER_DESC* pBufDesc;

    i = _iNextRx;

    pBufDesc = _paRxBufDesc + i;
    Addr = pBufDesc->Addr;
    pSrc = (U8 *) (Addr & EMAC_RXBUF_ADDRESS_MASK) + 2;       // We initialized a receive buffer offset of 2 which has to be taken into account !
    NumBytesAtOnce = NumBytes;
    i += NumBuffers;
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
    pInvalidate = (U8*)((unsigned)pSrc & ~3uL);                     // Mask out bit0 and bit1 because of the 32 byte boundary
    NumBytesToInvalidate = (NumBytesAtOnce + 2 + 31) & ~0x1FuL;     // NumBytesAtOnce + offset of the first Rx buffer + space for aligment
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
      pBufDesc = _paRxBufDesc;
      Addr = pBufDesc->Addr;
      pSrc = (U8*) (Addr & EMAC_RXBUF_ADDRESS_MASK);
      NumBytesToInvalidate = (NumBytes + 31) & ~0x1FuL;             // NumBytes + space for aligment
      BSP_CACHE_InvalidateRange(pSrc, NumBytesToInvalidate);
      IP_MEMCPY(pDest, pSrc, NumBytes);
    }
  }
  if (pDest) {
    IP_LOG((IP_MTYPE_DRIVER, "Packet: %d Bytes --- Read", NumBytes));
  } else {
    IP_LOG((IP_MTYPE_DRIVER, "Packet: %d Bytes --- Discarded", NumBytes));
  }
  INC(_RxCnt);
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
  IP_USE_PARA(Unit);     // Multiple units not supported, avoid warning
  _UpdateLinkState();
}

/*********************************************************************
*
*       _Control
*
*  Function description
*/
static int _Control(unsigned Unit, int Cmd, void * p) {
  IP_USE_PARA(Unit);     // Multiple units not supported, avoid warning
  if (Cmd == IP_NI_CMD_SET_FILTER) {
    return _SetFilter((IP_NI_CMD_SET_FILTER_DATA*)p);
  } else if (Cmd == IP_NI_CMD_SET_BPRESSURE) {
    AT91C_EMAC_NCR |=  AT91C_EMAC_BPRESSURE;
    return 0;
  } else if (Cmd == IP_NI_CMD_CLR_BPRESSURE) {
    AT91C_EMAC_NCR &=  ~AT91C_EMAC_BPRESSURE;
    return 0;
  } else if (Cmd == IP_NI_CMD_SET_PHY_ADDR) {
    _PHY_Context.Addr = (U8)(int)p;
    return 0;
  } else if (Cmd == IP_NI_CMD_SET_PHY_MODE) {
    _PhyMode = (U8)(int)p;
    _PHY_Context.UseRMII = _PhyMode;
    return 0;
  }
  return -1;
}

/*********************************************************************
*
*       IP_Driver_SAM9G45_ConfigNumRxBuffers
*
*  Function description
*    Sets the number of Rx Buffers in the config phase.
*/
void IP_NI_SAM9G45_ConfigNumRxBuffers(U16 NumRxBuffers) {
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
const IP_HW_DRIVER IP_Driver_SAM9G45 = {
  _Init,
  _SendPacketIfTxIdle,
  _GetPacketSize,
  _ReadPacket,
  _Timer,
  _Control
};

/*************************** End of file ****************************/
