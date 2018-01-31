/*********************************************************************
*               SEGGER MICROCONTROLLER SYSTEME GmbH                  *
*       Solutions for real time microcontroller applications         *
**********************************************************************
*                                                                    *
*       (C) 2003 - 2009   SEGGER Microcontroller Systeme GmbH        *
*                                                                    *
*       www.segger.com     Support: support@segger.com               *
*                                                                    *
**********************************************************************
*                                                                    *
*       TCP/IP stack for embedded applications                       *
*                                                                    *
**********************************************************************
----------------------------------------------------------------------
File    : IP_NI_STR912.c
Purpose : NI driver for ST STR 912
--------  END-OF-HEADER  ---------------------------------------------
*/

#include <stdio.h>
#include "IP_Int.h"
#include "IP_NI_STR912.h"

/*********************************************************************
*
*       Defines, configurable
*
**********************************************************************
*/
#define NUM_RX_BUFFERS  4         // One buffer is 1536 bytes. We use 4 buffers for 4 large packets.

#define _TX_BUFFER_SIZE            (1536)
#define _RX_BUFFER_SIZE            (1536)

#define _MAX_RX_PACKET_SIZE        (_RX_BUFFER_SIZE)

#define _ENET_VECT_INDEX           _VIC_IRQ_MAC_ID

#define _MII_WRITE_TO              (0x0004FFFF)
#define _MII_READ_TO               (0x0004FFFF)

#define _SCR_RX_MAX_BURST_SZ_VALUE    (0x0 << 4)
#define _SCR_TX_MAX_BURST_SZ_VALUE    (0x0 << 6)

/*********************************************************************
*
*       Defines, non configurable
*
**********************************************************************
*/

/****** Memory layout, base addresses for peripherals ***************/

#define _APB0_BASE_ADDR       (0x58000000)               // Base for unbuffered access
#define _APB1_BASE_ADDR       (0x5c000000)               // Base for unbuffered access

/****** ENET sfr definitions ****************************************/

#define _ENET_BASE_ADDR  (0x7C000000)                                // Unbufferd address in AHB space
#define _ENET_SCR        (*(volatile U32*)(_ENET_BASE_ADDR + 0x00))  // ENET Status/Control register
#define _ENET_IER        (*(volatile U32*)(_ENET_BASE_ADDR + 0x04))  // ENET interrupt enable register
#define _ENET_ISR        (*(volatile U32*)(_ENET_BASE_ADDR + 0x08))  // ENET interrupt status register
#define _ENET_CCR        (*(volatile U32*)(_ENET_BASE_ADDR + 0x0C))  // ENET clock control register
#define _ENET_RXSTR      (*(volatile U32*)(_ENET_BASE_ADDR + 0x10))  // ENET Rx DMA start register
#define _ENET_RXCR       (*(volatile U32*)(_ENET_BASE_ADDR + 0x14))  // ENET Rx DMA control register
#define _ENET_RXSAR      (*(volatile U32*)(_ENET_BASE_ADDR + 0x18))  // ENET Rx DMA start address register
#define _ENET_RXNDAR     (*(volatile U32*)(_ENET_BASE_ADDR + 0x1c))  // ENET Rx DMA next descriptor address register

#define _ENET_TXSTR      (*(volatile U32*)(_ENET_BASE_ADDR + 0x30))  // ENET Tx DMA start register
#define _ENET_TXNDAR     (*(volatile U32*)(_ENET_BASE_ADDR + 0x3C))  // ENET Tx DMA next descriptor Address register


#define _ENET_MAC_BASE_ADDR   (_ENET_BASE_ADDR + 0x400)   // Unbufferd address in AHB space

#define _ENET_MCR  (*(volatile U32*)(_ENET_MAC_BASE_ADDR + 0x00))  // ENET Control Register
#define _ENET_MAH  (*(volatile U32*)(_ENET_MAC_BASE_ADDR + 0x04))  // ENET Address High Register
#define _ENET_MAL  (*(volatile U32*)(_ENET_MAC_BASE_ADDR + 0x08))  // ENET Address Low Register
#define _ENET_MCHA (*(volatile U32*)(_ENET_MAC_BASE_ADDR + 0x0C))  // Multicast Address High Register
#define _ENET_MCLA (*(volatile U32*)(_ENET_MAC_BASE_ADDR + 0x10))  // Multicast Address Low Register
#define _ENET_MIIA (*(volatile U32*)(_ENET_MAC_BASE_ADDR + 0x14))  // MII Address Register
#define _ENET_MIID (*(volatile U32*)(_ENET_MAC_BASE_ADDR + 0x18))  // MII Data Register
#define _ENET_MCF  (*(volatile U32*)(_ENET_MAC_BASE_ADDR + 0x1C))  // ENET Control Frame Register
#define _ENET_VL1  (*(volatile U32*)(_ENET_MAC_BASE_ADDR + 0x20))  // VLAN1 Register
#define _ENET_VL2  (*(volatile U32*)(_ENET_MAC_BASE_ADDR + 0x24))  // VLAN2 Register
#define _ENET_MTS  (*(volatile U32*)(_ENET_MAC_BASE_ADDR + 0x28))  // ENET Transmission Status Register
#define _ENET_MRS  (*(volatile U32*)(_ENET_MAC_BASE_ADDR + 0x2C))  // ENET Reception Status Register

#define _SCR_SRESET_BIT              (0)        // ENET reset bit, no operation allowed as long as SRESET is high!
#define _SCR_RX_MAX_BURST_SZ_MASK    (0x3 << 4)
#define _SCR_TX_MAX_BURST_SZ_MASK    (0x3 << 6)

#define _RXSTR_DMA_EN_BIT            (0)        // Rx DMA enable
#define _RXSTR_START_FETCH_BIT       (2)        // Rx DMA start fetching descriptors
#define _RXSTR_DFETCH_DLY_MASK       0x00FFFF00 // Mask for descriptor fetch delay field in RXSTR register
#define _RXSTR_DFETCH_DLY_DEFAULT    0x00001000 // Rx Descriptor Fetch Delay default value

#define _RXNDAR_NPOL_EN_BIT          (0)

#define _RXCR_NEXT_EN_BIT            (14)

#define _TXSTR_DMA_EN_BIT            (0)        // Rx DMA enable
#define _TXSTR_START_FETCH_BIT       (2)        // Rx DMA start fetching descriptors
#define _TXSTR_URUN_BIT              (5)        // Underrun enabled.
#define _TXSTR_DFETCH_DLY_MASK       0x00FFFF00 // Mask for descriptor fetch delay field in TXSTR register
#define _TXSTR_DFETCH_DLY_DEFAULT    0x00010000 // Tx Descriptor Fetch Delay default value

#define _TXNDAR_NPOL_EN_BIT          (0)

#define _MCR_RE_BIT                  (2)
#define _MCR_TE_BIT                  (3)

#define _MCR_FDM_BIT                (20)         // FDM, full duplex mode bit
#define _MCR_DRO_BIT                (23)         // DRO, Disable reception of own frame during transmission

#define _MIIA_WR_BIT                 (1)         // Write mode, set to 1 to start write operation
#define _MIIA_BUSY_BIT               (0)         // Busy bit to start operation

#define _IER_RX_INT_BIT             (15)
#define _IER_TX_INT_BIT             (31)

#define _IER_RX_FULL_INT_BIT        (1)
#define _IER_RX_PACKET_LOST_INT_BIT (5)
#define _IER_RX_NEXT_INT_BIT        (6)
#define _IER_RX_MERR_INT_BIT        (9)
#define _IER_RX_ERR_INT_MASK        ((1uL << _IER_RX_FULL_INT_BIT) | (1uL << _IER_RX_PACKET_LOST_INT_BIT) | (1uL << _IER_RX_NEXT_INT_BIT) | (1uL << _IER_RX_MERR_INT_BIT))

#define _DMA_DSCR_RX_STATUS_VALID_MSK    0x00010000 // Valid bit indicator, if set in RX descripor => Buffer available for DMA
#define _DMA_DSCR_TX_STATUS_VALID_MSK    0x00010000 // Valid bit indicator, if set in RX descripor => Buffer available for DMA

/****** Interrupt controller ****************************************/

#define _VIC0_BASE_ADDR       (0xFFFFF000)
#define _VIC0_INTER           (*(volatile U32*)(_VIC0_BASE_ADDR + 0x10))  // Interrupt enable register VIC0
#define _VIC0_INTECR          (*(volatile U32*)(_VIC0_BASE_ADDR + 0x14))  // Interrupt enable clear register VIC0

#define _VIC0_VECT0           (*(volatile U32*)(_VIC0_BASE_ADDR + 0x0100))
#define _VIC0_VECT0_CNTL      (*(volatile U32*)(_VIC0_BASE_ADDR + 0x0200))

#define _VIC_IRQ_MAC_ID       (11)     // Source IRQ11 VIC0: MAC
#define _INT_SOURCE_MASK      (0x0F)

/****** SCU *********************************************************/

#define _SCU_BASE_ADDR        (_APB1_BASE_ADDR + 0x2000)
#define _SCU_CLKCNTR          (*(volatile U32*)(_SCU_BASE_ADDR + 0x00))
#define _SCU_PCGRO            (*(volatile U32*)(_SCU_BASE_ADDR + 0x14))
#define _SCU_PRR0             (*(volatile U32*)(_SCU_BASE_ADDR + 0x1C))

#define _SCU_GPIOOUT0         (*(volatile U32*)(_SCU_BASE_ADDR + 0x44))
#define _SCU_GPIOOUT1         (*(volatile U32*)(_SCU_BASE_ADDR + 0x48))
#define _SCU_GPIOOUT2         (*(volatile U32*)(_SCU_BASE_ADDR + 0x4C))
#define _SCU_GPIOOUT3         (*(volatile U32*)(_SCU_BASE_ADDR + 0x50))
#define _SCU_GPIOOUT4         (*(volatile U32*)(_SCU_BASE_ADDR + 0x54))
#define _SCU_GPIOOUT5         (*(volatile U32*)(_SCU_BASE_ADDR + 0x58))
#define _SCU_GPIOOUT6         (*(volatile U32*)(_SCU_BASE_ADDR + 0x5C))
#define _SCU_GPIOOUT7         (*(volatile U32*)(_SCU_BASE_ADDR + 0x60))

#define _SCU_GPIOIN0          (*(volatile U32*)(_SCU_BASE_ADDR + 0x64))
#define _SCU_GPIOIN1          (*(volatile U32*)(_SCU_BASE_ADDR + 0x68))
#define _SCU_GPIOIN2          (*(volatile U32*)(_SCU_BASE_ADDR + 0x6C))
#define _SCU_GPIOIN3          (*(volatile U32*)(_SCU_BASE_ADDR + 0x70))
#define _SCU_GPIOIN4          (*(volatile U32*)(_SCU_BASE_ADDR + 0x74))
#define _SCU_GPIOIN5          (*(volatile U32*)(_SCU_BASE_ADDR + 0x78))
#define _SCU_GPIOIN6          (*(volatile U32*)(_SCU_BASE_ADDR + 0x7C))
#define _SCU_GPIOIN7          (*(volatile U32*)(_SCU_BASE_ADDR + 0x80))

#define _SCU_GPIOTYPE0        (*(volatile U32*)(_SCU_BASE_ADDR + 0x84))
#define _SCU_GPIOTYPE1        (*(volatile U32*)(_SCU_BASE_ADDR + 0x88))
#define _SCU_GPIOTYPE2        (*(volatile U32*)(_SCU_BASE_ADDR + 0x8C))
#define _SCU_GPIOTYPE3        (*(volatile U32*)(_SCU_BASE_ADDR + 0x90))
#define _SCU_GPIOTYPE4        (*(volatile U32*)(_SCU_BASE_ADDR + 0x94))
#define _SCU_GPIOTYPE5        (*(volatile U32*)(_SCU_BASE_ADDR + 0x98))
#define _SCU_GPIOTYPE6        (*(volatile U32*)(_SCU_BASE_ADDR + 0x9C))
#define _SCU_GPIOTYPE7        (*(volatile U32*)(_SCU_BASE_ADDR + 0xA0))

#define _SCU_RST_MAC_BIT      (11)
#define _SCU_PCGR_MAC_BIT     (11)
#define _SCU_PHYSEL_BIT       (12)

/****** GPIO ********************************************************/

#define _GPIO_BASE_ADDR       (_APB0_BASE_ADDR + 0x6000)
#define _GPIO0_BASE_ADDR      (_APB0_BASE_ADDR + 0x6000)
#define _GPIO1_BASE_ADDR      (_APB0_BASE_ADDR + 0x7000)
#define _GPIO2_BASE_ADDR      (_APB0_BASE_ADDR + 0x8000)
#define _GPIO3_BASE_ADDR      (_APB0_BASE_ADDR + 0x9000)
#define _GPIO4_BASE_ADDR      (_APB0_BASE_ADDR + 0xA000)
#define _GPIO5_BASE_ADDR      (_APB0_BASE_ADDR + 0xB000)
#define _GPIO6_BASE_ADDR      (_APB0_BASE_ADDR + 0xC000)
#define _GPIO7_BASE_ADDR      (_APB0_BASE_ADDR + 0xD000)
#define _GPIO8_BASE_ADDR      (_APB0_BASE_ADDR + 0xE000)
#define _GPIO9_BASE_ADDR      (_APB0_BASE_ADDR + 0xF000)

#define _GPIO_DIR_OFFS 0x400
#define _GPIO_SEL_OFFS 0x41C

/*********************************************************************
*
*       PHY
*/
// Generic MII registers.
#define MII_BCR                            0x00                // Basic mode control register
#define MII_BSR                            0x01                // Basic mode status register
#define MII_PHYSID1                        0x02                // PHYS ID 1
#define MII_PHYSID2                        0x03                // PHYS ID 2
#define MII_ANAR                           0x04                // Auto-negotiation Advertisement control reg
#define MII_LPA                            0x05                // Link partner ability reg
#define MII_EXPANSION                      0x06                // Expansion register

// PHY - Basic control register.
#define PHY_BCR_CTST                       (1 <<  7)           // Collision test
#define PHY_BCR_FULLDPLX                   (1 <<  8)           // Full duplex
#define PHY_BCR_ANRESTART                  (1 <<  9)           // Auto negotiation restart
#define PHY_BCR_ISOLATE                    (1 << 10)           // Disconnect PHY from MII
#define PHY_BCR_PDOWN                      (1 << 11)           // Powerdown PHY
#define PHY_BCR_ANENABLE                   (1 << 12)           // Enable auto negotiation
#define PHY_BCR_SPEED100                   (1 << 13)           // Select 100Mbps
#define PHY_BCR_LOOPBACK                   (1 << 14)           // TXD loopback bits
#define PHY_BCR_RESET                      (1 << 15)           // Reset PHY

// Basic status register.
#define PHY_BSR_LSTATUS                    0x0004              // status
#define PHY_BSR_ANEGCOMPLETE               0x0020              // Auto-negotiation complete

// Link partner ability register
#define PHY_LPA_10HALF                     0x0020              // Can do 10mbps half-duplex
#define PHY_LPA_10FULL                     0x0040              // Can do 10mbps full-duplex

/****** Macros ******************************************************/

#if DEBUG
  #define INC(v) (v++)
#else
  #define INC(v)
#endif

/*********************************************************************
*
*       Types
*
**********************************************************************
*/

typedef struct DMA_BUFFER_DESC DMA_BUFFER_DESC;   // See 'STR912 ENET DMA descriptor in memory'

struct DMA_BUFFER_DESC {
  U32  Control;             // DMA control word
  U32  StartAddr;           // Buffer start address
  DMA_BUFFER_DESC * pNext;  // Next descriptor
  U32  PacketStatus;        // Contains result of transfer
};                          // Refer to 'STR912 ENET DMA descriptor in memory'

/*********************************************************************
*
*       Static data
*
**********************************************************************
*/
static U8  _PhyAddr = 0xFF;   // Set to default value
static U16 _PhyAnar;          // Value written to ANAR (Auto-negotiation Advertisement register)
static U16 _PhyBmcr;          // Value written to BMCR (basic mode control register)

static DMA_BUFFER_DESC  *_paRxBufDesc;        // Pointer to Rx buffer descriptors
static DMA_BUFFER_DESC  *_pRxCurrentBufDesc;  // Pointer to current Rx buffer descriptor
static DMA_BUFFER_DESC  *_pTxBufDesc;         // Pointer to the one and only Tx buffer descriptor

static char _TxIsBusy;

static U32 _TxBuffer[_TX_BUFFER_SIZE / 4];  // One Tx-packet has a maximum size of 1536 bytes
                                            // The buffer start address has to be 32bit aligned for DMA transfer !
#if DEBUG
struct {
  int TxSendCnt;
  int TxIntCnt;
  int RxIntCnt;
  int IsrErrCnt;
  int IsrErrCntFull;
  int IsrErrCntPacket;
  int IsrErrCntNext;
  int IsrErrCntMerr;
  int RxCnt;
  int GetPacketError;
} DriverStats;
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
*       _EnableRx
*/
static void _EnableRx(void) {
  _ENET_MCR    |= (1uL << _MCR_RE_BIT);
  _ENET_RXSTR  |= (1uL << _RXSTR_START_FETCH_BIT);    /* Start the DMA Fetch */
}

/*********************************************************************
*
*       _EnableTx
*/
static void _EnableTx(void) {
  _ENET_MCR |= (1uL << _MCR_TE_BIT);
}

/*********************************************************************
*
*       _EnableRxInt
*/
static  void  _EnableRxInt (void) {
  _ENET_IER |= (1uL << _IER_RX_INT_BIT) | _IER_RX_ERR_INT_MASK;
}

/*********************************************************************
*
*       _EnableTxInt
*/
static  void  _EnableTxInt (void) {
  _ENET_IER |= (1uL << _IER_TX_INT_BIT);
}

/*********************************************************************
*
*       _UpdateEMACSettings
*
*     Needs to be called whenever speed and duplex settings change
*/
static void _UpdateEMACSettings(U32 Duplex, U32 Speed) {
  if (Duplex == IP_DUPLEX_FULL)  {
    _ENET_MCR |=  (1uL << _MCR_FDM_BIT);   /* full duplex mode */
    _ENET_MCR |=  (1uL << _MCR_DRO_BIT);   /* disable frame reception during transmission */
  } else {
    _ENET_MCR &= ~(1uL << _MCR_FDM_BIT);   /* half duplex mode */
    _ENET_MCR |=  (1uL << _MCR_DRO_BIT);   /* disable frame reception during transmission */
  }
}

/*********************************************************************
*
*       _PHY_WriteReg
*/
static void _PHY_WriteReg(U8 RegIndex,  U16 Data) {
  U32 Addr;
  U32 Temp;     /* temporary result for address register status */
  U32 Timeout;

  if (RegIndex == MII_ANAR) {
    _PhyAnar = Data;
  }
  if (RegIndex == MII_BCR) {
    _PhyBmcr = Data;
  }
  //
  // Prepare the MII register address
  //
  Addr = ((_PhyAddr & 0x1F) << 11)  // Phy address is located at b11 and is 5 bits wide
       | ((RegIndex & 0x1F) << 6)   // Phy register index is located at b6 and is 5 bits wide
       | (1uL << _MIIA_WR_BIT)      // Set write mode
       | (1uL << _MIIA_BUSY_BIT)    // Set busy bit to start operation
       ;
  //
  // Wait until PHY is not busy
  //
  Timeout = _MII_WRITE_TO;
  do {
    Timeout--;
    Temp = _ENET_MIIA & (1uL << _MIIA_BUSY_BIT);
  } while (Temp && Timeout);
  //
  // Write data into the data register, then address and command in the address register
  //
  _ENET_MIID = Data & 0xFFFF;
  _ENET_MIIA = Addr;
  //
  // Wait until PHY finished execution of command
  //
  Timeout = _MII_WRITE_TO;
  do {
    Timeout--;
    Temp = _ENET_MIIA & (1uL << _MIIA_BUSY_BIT);
  } while (Temp && Timeout);
}

/*********************************************************************
*
*       _PHY_ReadReg
*/
static U16 _PHY_ReadReg(U8 RegIndex) {
  U32 r;
  U32 Addr;
  U32 Temp;    /* temporary result for address register status */
  U32 Timeout; /* timeout value for read process */

  //
  // Prepare the MII register address
  //
  Addr = ((_PhyAddr & 0x1F) << 11)  // Phy address is located at b11 and is 5 bits wide
       | ((RegIndex & 0x1F) << 6)   // Phy register index is located at b6 and is 5 bits wide
       | (0uL << _MIIA_WR_BIT)      // Set read mode, wr-bit == 0
       | (1uL << _MIIA_BUSY_BIT)    // Set busy bit to start operation
       ;
  //
  // Wait until PHY is not busy
  //
  Timeout = _MII_READ_TO;
  do {
    Timeout--;
    Temp = _ENET_MIIA & (1uL << _MIIA_BUSY_BIT);
  } while (Temp && Timeout);
  /* write the result value into the MII Address register */
  _ENET_MIIA = Addr;
  //
  // Wait until PHY finished execution of command
  //
  Timeout = _MII_READ_TO;
  do {
    Timeout--;
    Temp = _ENET_MIIA & (1uL << _MIIA_BUSY_BIT);
  } while (Temp && Timeout);
  //
  // Read result from PHY
  //
  r = _ENET_MIID;
  return (U16)r;
}

/*********************************************************************
*
*       _PHY_Init
*/
static int _PHY_Init(void) {
  U16 v, w;
  unsigned i;
  unsigned FirstAddr;
  unsigned LastAddr;
  volatile U32 RegValue;

  //
  // Try to detect PHY on any permitted addr
  //
  if (_PhyAddr == 0xff) {
    FirstAddr = 0;
    LastAddr  = 0x1f;
  } else {
    FirstAddr = _PhyAddr;
    LastAddr  = _PhyAddr;
  }
  for (i = FirstAddr; ; i++) {
    if (i > LastAddr) {
      IP_WARN((IP_MTYPE_DRIVER, "DRIVER: no PHY found."));
      return 1;              // No PHY found
    }
    _PhyAddr = (U8)i;
    v = _PHY_ReadReg(MII_ANAR);
    v &= 0x1f;    // Lower 5 bits are fixed: 00001b
    if (v != 1) {
      continue;
    }
    v = _PHY_ReadReg(MII_PHYSID1);
    if ((v == 0) || (v == 0xFFFF)) {
      continue;
    }
    w = _PHY_ReadReg(MII_PHYSID1);
    if (v != w) {
      continue;
    }
    IP_LOG((IP_MTYPE_DRIVER | IP_MTYPE_INIT, "DRIVER: Found PHY with Id 0x%x at addr 0x%x", v, _PhyAddr));
    break;
  }
  //
  // Reset the Phy
  //
  _PHY_WriteReg(MII_BCR, PHY_BCR_RESET);
  do {
    v = _PHY_ReadReg(MII_BCR);
  } while (v & PHY_BCR_RESET);
  //
  // Clear Full duplex bits
  //
  v = _PHY_ReadReg(MII_ANAR);
  v &= ~((1 << 6) | (1 << 8));     // Clear FDX bits, Bit 6 - 10BASE-T Full Duplex Support: 0  Not enabled, Bit 8 - 100BASE-TX Full Duplex Support: 0 Not enabled
  _PHY_WriteReg(MII_ANAR, v);
  if (IP_DEBUG) {
    w = _PHY_ReadReg(MII_ANAR);
    if (v != w) {
      IP_WARN((IP_MTYPE_DRIVER, "DRIVER: Can not write PHY Reg"));
      return 1;
    }
  }
  //
  // Enable 100BASE-TX, 10BASE-T support
  //
  v = _PHY_ReadReg(MII_ANAR);
  v |= ((1 << 7) | (1 << 5));
  _PHY_WriteReg(MII_ANAR, v);
  if (IP_DEBUG) {
    w = _PHY_ReadReg(MII_ANAR);
    if (v != w) {
      IP_WARN((IP_MTYPE_DRIVER, "DRIVER: Can not write PHY Reg"));
      return 1;
    }
  }
  //
  // Connect MII-interface by clearing "ISOLATE" and start auto negotiation
  //
  v = PHY_BCR_ANRESTART       // Restart auto-negotiation
    | PHY_BCR_ANENABLE        // Enable  auto-negotiation
    ;
  _PHY_WriteReg(MII_BCR, v);
  return 0;
}

/*********************************************************************
*
*       _PHY_GetLinkState
*/
static void _PHY_GetLinkState(U32 * pDuplex, U32 * pSpeed) {
  U32 bmsr;
  U32 bmcr;
  U32 lpa;             // Link partner ability
  U32 Speed;
  U32 Duplex;
  U32 v;

  Speed  = 0;
  Duplex = IP_DUPLEX_UNKNOWN;
  //
  // Get Link Status from PHY status reg. Requires 2 reads
  //
  bmsr = _PHY_ReadReg(MII_BSR);
  bmsr = _PHY_ReadReg(MII_BSR);
  if (bmsr & PHY_BSR_LSTATUS) {                       // Link established ?
    if (_PhyBmcr & (1 << 12)) {                       // Auto-negotiation enabled ?
      lpa  = _PHY_ReadReg(MII_LPA);
      if (lpa & 0x1F != 1) {                          // Some PHY require reading LPA twice
        lpa = _PHY_ReadReg(MII_LPA);
      }
      v = lpa & _PhyAnar;
      if (v & (1 << 8)) {
        Speed  = IP_SPEED_100MHZ;
        Duplex = IP_DUPLEX_FULL;
      } else if (v & (1 << 7)) {
        Speed  = IP_SPEED_100MHZ;
        Duplex = IP_DUPLEX_HALF;
      } else if (v & (1 << 6)) {
        Speed  = IP_SPEED_10MHZ;
        Duplex = IP_DUPLEX_FULL;
      } else if (v & (1 << 5)) {
        Speed  = IP_SPEED_10MHZ;
        Duplex = IP_DUPLEX_HALF;
      }
    } else {
      bmcr = _PhyBmcr;
      if (bmcr & PHY_BCR_SPEED100) {
        Speed = IP_SPEED_100MHZ;
      } else {
        Speed = IP_SPEED_10MHZ;
      }
      if (bmcr & PHY_BCR_FULLDPLX) {
        Duplex = IP_DUPLEX_FULL;
      } else {
        Duplex = IP_DUPLEX_HALF;
      }
    }
  }
  *pDuplex = Duplex;
  *pSpeed  = Speed;
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

  _PHY_GetLinkState(&Duplex, &Speed);
  if (IP_SetCurrentLinkState(Duplex, Speed)) {
    _UpdateEMACSettings (Duplex, Speed);              /* Inform the EMAC about the current PHY settings       */
  }
}

/*********************************************************************
*
*       _InitPIO
*/
static void _InitPIO(void) {
  U32* pSfr;

  //
  // Enable clock for ENET/MAC unit and release reset state
  //
  _SCU_PCGRO   |= (1uL << _SCU_PCGR_MAC_BIT);     // Enable peripheral clock
  _SCU_PRR0    |= (1uL << _SCU_RST_MAC_BIT);      // Release reset state
  _SCU_CLKCNTR |= (1uL << _SCU_PHYSEL_BIT);       // Enable MII-PHY clock
  //
  // Setup GPIO1, Pin 1,2,3,4 and 7, needed as output for MII
  //
  pSfr   = (U32*)(_GPIO_BASE_ADDR + (0x1000 * 1) + _GPIO_DIR_OFFS);
  *pSfr |= ((1 << 1) | (1 << 2) | (1 << 3) | (1 << 4) | (1 << 7));
  //
  // Setup GPIO5 Pin 2 and 3 as alternate output 2
  //
  pSfr    = (U32*)&_SCU_GPIOOUT1;
  *pSfr  &= ~((3 << (1 * 2)) | (3 << (2 * 2)) | (3 << (3 * 2)) | (3 << (4 * 2)) | (3 << (7 * 2)));  // Clear current mode:        (Mask << (Pin * 2))
  *pSfr  |=  ((2 << (1 * 2)) | (2 << (2 * 2)) | (2 << (3 * 2)) | (2 << (4 * 2)) | (2 << (7 * 2)));  // Set alternate output mode: (Mode2 << (Pin * 2))
  //
  // Setup GPIO5 output type for Pin2 and pin 3 as Push-Pull
  //
  _SCU_GPIOTYPE1 &= ~((1 << 1) | (1 << 2) | (1 << 3) | (1 << 4) | (1 << 7));
  //
  // Disconnect Input of GPIO5, Pin 2 and 3
  //
  _SCU_GPIOIN1 &= ~((1 << 1) | (1 << 2) | (1 << 3) | (1 << 4) | (1 << 7));
  //
  // Setup GPIO5, Pin 2 and 3 as output, needed as output for MII
  //
  pSfr   = (U32*)(_GPIO_BASE_ADDR + (0x1000 * 5) + _GPIO_DIR_OFFS);
  *pSfr |= ((1 << 2) | (1 << 3));
  //
  // Setup GPIO5 Pin 2 and 3 as alternate output 2
  //
  pSfr    = (U32*)&_SCU_GPIOOUT5;
  *pSfr  &= ~((3 << (2 * 2)) | (3 << (3 * 2)));  // Clear current mode: Mask << (Pin * 2)
  *pSfr  |=  ((2 << (2 * 2)) | (2 << (3 * 2)));  // Set alternate output mode: Mode2 << (Pin * 2)
  //
  // Setup GPIO5 output type for Pin2 and pin 3 as Push-Pull
  //
  _SCU_GPIOTYPE5 &= ~((1 << 2) | (1 << 3));
  //
  // Disconnect Input of GPIO5, Pin 2 and 3
  //
  _SCU_GPIOIN5 &= ~((1 << 2) | (1 << 3));
}

/*********************************************************************
*
*         _InitRxBufferDescs()
*
*  Function description:
*    Initializes the Rx buffers and descriptors descriptors
*
*  Notes
*    (1) Buffer descriptors must start on a 32-bit boundary.
*/
static  void  _InitRxBufferDescs (void) {
  int     i;
  DMA_BUFFER_DESC  *pBufferDesc;
  U32 DataAddr;

  pBufferDesc = _paRxBufDesc;
  DataAddr     = ((U32)pBufferDesc) + sizeof(DMA_BUFFER_DESC) * (NUM_RX_BUFFERS + 1);   // Addr of first rx buffer
  //
  // Initialize Rx buffer descriptors
  //
  for (i = 0; i < NUM_RX_BUFFERS; i++) {
    pBufferDesc->Control      = _MAX_RX_PACKET_SIZE | (1uL << _RXCR_NEXT_EN_BIT);            // Initialize ENET DMA control word: maximum packet size which can be received, also allow contionous descriptor fetch
    pBufferDesc->StartAddr    = DataAddr;                       // Start address of data buffer
    pBufferDesc->pNext        = (DMA_BUFFER_DESC*) ((U32) (pBufferDesc + 1) | (1uL << _RXNDAR_NPOL_EN_BIT));  // Point to the next descriptor, enable automatic polling
    pBufferDesc->PacketStatus = _DMA_DSCR_RX_STATUS_VALID_MSK;  // Initialize ENET DMA packet status: Valid descriptor, may be used by DMA
    DataAddr += _RX_BUFFER_SIZE;
    pBufferDesc++;
  }
  (pBufferDesc - 1)->pNext = (DMA_BUFFER_DESC*) ((U32) _paRxBufDesc | (1uL << _RXNDAR_NPOL_EN_BIT));  // The last descriptor points to the first (ring list), enable automatic polling
  //
  // Initialize ENET/MAC to use the first descriptor initially
  //
  _ENET_RXNDAR = ((U32)_paRxBufDesc) | (1uL << _RXNDAR_NPOL_EN_BIT);  // Also allow automatic polling of next descriptor
}

/*********************************************************************
*
*         _InitTxBufferDesc()
*
*  Function description:
*    Initializes the Tx buffer and descriptor descriptors
*
*  Notes
*    (1) Buffer descriptors must start on a 32-bit boundary.
*/
static  void  _InitTxBufferDesc (void) {
  //
  // Configure ENET DMA for transmission
  //
  _pTxBufDesc->StartAddr    = 0;                  // Start address of tx data buffer is unknown right now
  _pTxBufDesc->pNext        = _pTxBufDesc;        // We have one descriptor only, link to itself
  _pTxBufDesc->Control      = 0;                  // Initialize ENET DMA control and status word
  _pTxBufDesc->PacketStatus = 0;                  // Initialize ENET DMA packet status
  //
  // Setup ENET DMA to use the Tx descriptor
  //
  _ENET_TXNDAR  = (U32) _pTxBufDesc;              // Set tx next start address to Tx decriptor table base address
  _ENET_TXNDAR |= (1uL << _TXNDAR_NPOL_EN_BIT);   // Enable next polling
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
  DMA_BUFFER_DESC  *pBufferDesc;
  U8 * pMem;

  //
  // Alloc memory for buffer descriptors and buffers
  //
  pMem = (U8*)IP_Alloc(NUM_RX_BUFFERS * (sizeof(DMA_BUFFER_DESC) + _RX_BUFFER_SIZE) + 4 + sizeof(DMA_BUFFER_DESC));   // Alloc n* (buffer descriptor + buffer) + space for aligment + 1 Tx buffer descriptor
  pMem = (U8*)_Align(pMem, 8);
  pBufferDesc        = (DMA_BUFFER_DESC*) pMem;
  _paRxBufDesc       = pBufferDesc;
  _pRxCurrentBufDesc = pBufferDesc;                                                     // Initially set marker for current buffer descriptor to start of list
  //
  // Initialize Tx buffer descriptor(s)
  //
  _pTxBufDesc   = pBufferDesc + NUM_RX_BUFFERS;

}

/*********************************************************************
*
*       _FreeRxBuffer
*
*  Function description
*    Frees the current Rx buffer(s).
*/
static void _FreeRxBuffer(void) {
  DMA_BUFFER_DESC *pBufDesc;

  //
  // Advance to next buffer descriptor.
  //
  pBufDesc = _pRxCurrentBufDesc;
  _pRxCurrentBufDesc = (DMA_BUFFER_DESC*) ((U32) pBufDesc->pNext & ~0x3);  // The Next pointer may contain 2 additional control bits which have to be masked out !!!
  //
  // Release packet state, give buffer back to DMA, restart DMA
  //
  pBufDesc->PacketStatus = _DMA_DSCR_RX_STATUS_VALID_MSK;  // Initialize ENET DMA packet status: Valid descriptor, may be re-used by DMA
}

/*********************************************************************
*
*       _GetPacketSize()
*
*  Function description
*    Reads buffer descriptors in order to find out if a packet has been received.
*    Different error conditions are checked and handled.
*
*  Return value
*    Number of buffers used for the next packet.
*    0 if no complete packet is available.
*/
static int _GetPacketSize(unsigned Unit) {
  int PacketSize;
  int PacketStatus;

  PacketSize = 0;
  if (_pRxCurrentBufDesc) {
    PacketStatus = _pRxCurrentBufDesc->PacketStatus;
    if ((PacketStatus & _DMA_DSCR_RX_STATUS_VALID_MSK) == 0) {  // Valid bit cleared => A packet was received
      //
      // Buffer contains a packet, check whether it is valid
      //
      PacketSize = (PacketStatus & 0x7FF) - 4;  // The lower 11 bits contain the packet size
      if (PacketSize > _MAX_RX_PACKET_SIZE) {
        goto Error;
      }
      if (PacketStatus & (1uL << 31)) {         // frame abort?
        goto Error;
      }
      if (PacketSize < 60) { //((_pRxCurrentBufDesc->PacketStatus & (1uL << 30)) == 0 ) {  // Did packet pass the filter condition?
        goto Error;
      }
      //
      // All tests passed, valid packet received
      //
    }
  }
  return PacketSize;

Error:
  INC(DriverStats.GetPacketError);
  PacketSize = 0;
  _FreeRxBuffer();  // Remove current packet, re-enable DMA
  return 0;
}

/*********************************************************************
*
*       _ReadPacket
*/
static int _ReadPacket(unsigned Unit, U8 *pDest, unsigned NumBytes) {
  U8 *pSrc;

  if (pDest) {
    pSrc = (U8 *) _pRxCurrentBufDesc->StartAddr;
    IP_MEMCPY(pDest, pSrc, NumBytes);
    INC(DriverStats.RxCnt);
  }
  if (pDest) {
    IP_LOG((IP_MTYPE_DRIVER, "Packet: %d Bytes --- Read\n", NumBytes));
  } else {
    IP_LOG((IP_MTYPE_DRIVER, "Packet: %d Bytes --- Discarded\n", NumBytes));
  }
  //
  // Free packet
  //
  _FreeRxBuffer();
  return 0;
}

/*********************************************************************
*
*       _SendPacket
*/
static int  _SendPacket(void) {
  void   * pPacket;
  unsigned NumBytes;

  IP_GetNextOutPacket(&pPacket, &NumBytes);
  if (NumBytes == 0) {
    _TxIsBusy = 0;
    return 0;
  }
  IP_LOG((IP_MTYPE_DRIVER, "Sending packet: %d bytes\n", NumBytes));
  INC(DriverStats.TxSendCnt);
  //
  // Set start address for DMA transfer. The address has to be 32 bit aligned
  //
  if (((U32) pPacket & 0x3) != 0) {
    // Not aligned, we have to copy the packet and send from local buffer
    IP_MEMCPY(_TxBuffer, pPacket, NumBytes);
    _pTxBufDesc->StartAddr = (U32)&_TxBuffer;
  } else {
    // Original data is aligned, we do not need to move or copy
    _pTxBufDesc->StartAddr  = (U32) pPacket;                 // Set start address for DMA transfer
  }
  _pTxBufDesc->Control      = NumBytes & 0xFFF;              // Initialize ENET DMA control and status word
  _pTxBufDesc->PacketStatus = _DMA_DSCR_TX_STATUS_VALID_MSK; // Mark data as valid, DMA send this packet
  //
  // Finally start transmission
  //
  _ENET_TXSTR |= (1uL << _TXSTR_START_FETCH_BIT);
  return 0;
}

/*********************************************************************
*
*       _SendPacketIfTxIdle
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

/**********************************************************************************************************
*
*       _ResetRxError()
*
*  Function description:
*    Resets the receiver logic in case of Fatal Rx errors (not simple packet corruption like CRC error)
*/
static void _ResetRxError(void) {
  _ENET_MCR    &= ~(1uL << _MCR_RE_BIT);
  _InitRxBufferDescs();
  _ENET_MCR    |= (1uL << _MCR_RE_BIT);
  _ENET_RXSTR   = (1uL << 6) |  (1uL << 5) | (1uL << _RXSTR_START_FETCH_BIT);
}

/*********************************************************************
*
*       _ISR_Handler
*/
static void  _ISR_Handler(void) {
  U32  ISR_Status;

  ISR_Status  = _ENET_ISR;        // Get current interrupt status
  _ENET_ISR   = ISR_Status;       // A write clears the bits in the ISR register
  if (ISR_Status & (1uL << _IER_RX_INT_BIT)) {  // Did we recieve a frame ?
    INC(DriverStats.RxIntCnt);
    IP_OnRx();
  }
  if (ISR_Status & (1uL << _IER_TX_INT_BIT)) {  // Transmission finished ?
    IP_RemoveOutPacket();
    INC(DriverStats.TxIntCnt);
    _SendPacket();
  }
  //
  // Handle Rx errors
  //
  if (ISR_Status & _IER_RX_ERR_INT_MASK)  {  // Any other Interrupt?
    INC(DriverStats.IsrErrCnt);
    if (ISR_Status & _IER_RX_FULL_INT_BIT) {
      INC(DriverStats.IsrErrCntFull);
    }
    if (ISR_Status & _IER_RX_PACKET_LOST_INT_BIT) {
      INC(DriverStats.IsrErrCntPacket);
    }
    if (ISR_Status & _IER_RX_NEXT_INT_BIT) {
      INC(DriverStats.IsrErrCntNext);
    }
    if (ISR_Status & _IER_RX_MERR_INT_BIT) {
      INC(DriverStats.IsrErrCntMerr);
    }
    _FreeRxBuffer();
    _ResetRxError();
  }
}

/*********************************************************************
*
*       _IntInit()
*
*  Function description
*    Initializes the interrupt controller to handle NI interrupts.
*    The MAC unit and its interrupt logic will be initialized separately
*/
static void  _IntInit (void) {
  U32* pCntlRegister;
  U32   CntlRegister;

  _VIC0_INTECR = (1 << _VIC_IRQ_MAC_ID);    // Disable Ethernet MAC interrupts
  //
  // Install handler, set interrupt vector
  //
  *(U32*)(&_VIC0_VECT0 + _ENET_VECT_INDEX) = (U32) &_ISR_Handler;
  //
  // Assign ENET Source to interrupt vector and enable interrupt vector
  //
  pCntlRegister = (U32*) &_VIC0_VECT0_CNTL;
  pCntlRegister += _ENET_VECT_INDEX;
  CntlRegister   = *(pCntlRegister) & ~_INT_SOURCE_MASK;
  *pCntlRegister = (CntlRegister | (_VIC_IRQ_MAC_ID & _INT_SOURCE_MASK) | (1 << 5)); // Set Interrupt source and interrupt enable bit
  //
  // Enable ENET interrupt source
  //
  _VIC0_INTER = (1 << _VIC_IRQ_MAC_ID);
}

/*********************************************************************
*
*       _Timer
*/
static void _Timer(unsigned Unit) {
  _UpdateLinkState();
}

/*********************************************************************
*
*       _Control
*
*  Function description
*/
static int _Control(unsigned Unit, int Cmd, void * p) {
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
    break;
  case IP_NI_CMD_GET_CAPS:
    //
    // TBD: Retrieves the capabilites, which are a logical-or combination of the IP_NI_CAPS (if any)
    //
    // {
    // int v;
    //
    // v = 0
    //   | IP_NI_CAPS_WRITE_IP_CHKSUM     // Driver capable of inserting the IP-checksum into an outgoing packet?
    //   | IP_NI_CAPS_WRITE_UDP_CHKSUM    // Driver capable of inserting the UDP-checksum into an outgoing packet?
    //   | IP_NI_CAPS_WRITE_TCP_CHKSUM    // Driver capable of inserting the TCP-checksum into an outgoing packet?
    //   | IP_NI_CAPS_WRITE_ICMP_CHKSUM   // Driver capable of inserting the ICMP-checksum into an outgoing packet?
    //   | IP_NI_CAPS_CHECK_IP_CHKSUM     // Driver capable of computing and comparing the IP-checksum of an incoming packet?
    //   | IP_NI_CAPS_CHECK_UDP_CHKSUM    // Driver capable of computing and comparing the UDP-checksum of an incoming packet?
    //   | IP_NI_CAPS_CHECK_TCP_CHKSUM    // Driver capable of computing and comparing the TCP-checksum of an incoming packet?
    //   | IP_NI_CAPS_CHECK_ICMP_CHKSUM   // Driver capable of computing and comparing the ICMP-checksum of an incoming packet?
    //
    // return v;
    // }
    break;
  case IP_NI_CMD_POLL:
    //
    //  Poll MAC (typically once per ms) in cases where MAC does not trigger an interrupt.
    //
    break;
  default:
    ;
  }
  return -1;
}

/*********************************************************************
*
*       _Init
*/
static  int  _Init(unsigned Unit) {
  U32  MacAddrLow;
  U32  MacAddrHigh;
  U32  RegValue;

  //
  // Initialize ports and clock for EMAC module
  //
  _InitPIO();
  //
  // Initialize MAC
  //
  _ENET_SCR &= ~(1uL << _SCR_SRESET_BIT);   // First of all cancel the RESET state. This has to be the first and only operation, before any other register or bit of the ENET module can be accessed
  //
  // Initialize MAC control of ENET unit
  //
  _ENET_MCR   = (0uL << 31)  // RA, receive all: 0 => use filter method
              | (0uL << 30)  // Endianity: 0 => little
              | (0uL << 26)  // b29..b26: reserved, write as 0
              | (1uL << 24)  // b25..b24: Prescaler: 01 >= HCLK > 50MHz
              | (1uL << 23)  // b23: DRO, disable receive own: should be set in full duplex or non loopback mode
              | (0uL << 21)  // b22..b21: LM, Loopback mode: 0 => normal operation
              | (0uL << 20)  // b20: FDM, full duplex mode: 1 => Duplex
              | (0uL << 17)  // b19..b17: AFM, Address filter mode: 0 => perfect filtering
              | (0uL << 16)  // PWF, pass wrong frame: 0 => disabled
              | (0uL << 15)  // VFM, VLAN filtering Mode
              | (0uL << 13)  // b14..b13: Reserved, write as 0
              | (0uL << 12)  // ELC, Late collision detect: 0 => Collision causes a status update
              | (0uL << 11)  // DBF, Disable broadcast frame: 0 => Broadcast reception enabled
              | (1uL << 11)  // DBF, Disable broadcast frame: 0 => Broadcast reception enabled
              | (0uL << 10)  // DPR, Disable packet retry: 0 => Retry enabled
              | (1uL <<  9)  // RVFF, VCI Rx filtering: 1 => VCI Rx Filtering enabled
              | (1uL <<  8)  // APR, Automatic PAD Removal: 1 => Enabled
              | (0uL <<  6)  // b7..b6: BL, Back-off Limit
              | (1uL <<  5)  // DCE, Deferral Check Enable: 1 => enabled
              | (0uL <<  4)  // RVBE, Reception VCI Burst Enable: 0 => disabled
              | (0uL <<  3)  // TE, Transmission enable: 0 => disabled, will be enabled later
              | (0uL <<  2)  // RE, Reception enable: 0 => disabled, will be enabled later
              | (0uL <<  1)  // Reserved, write as 0
              | (0uL <<  0)  // RCFA, Reverse control frame address: 0, normal
              ;
  //
  // Initialize DMA units, setup Rx/Tx max burst size
  //
  RegValue = _ENET_SCR;
  //
  // Setup Tx Max burst size
  RegValue &= ~(U32)(_SCR_TX_MAX_BURST_SZ_MASK  | _SCR_RX_MAX_BURST_SZ_MASK);
  RegValue |=  (U32)(_SCR_TX_MAX_BURST_SZ_VALUE | _SCR_RX_MAX_BURST_SZ_VALUE);
  _ENET_SCR = RegValue;
  //
  // Initialize PHY
  //
  _PHY_Init();
  //
  // Setup MAC Address
  //
  MacAddrLow  = IP_LoadU32LE(&IP_aIFace[0].abHWAddr[0]);
  MacAddrHigh = IP_LoadU16LE(&IP_aIFace[0].abHWAddr[4]);
  _ENET_MAH   = MacAddrHigh;
  _ENET_MAL   = MacAddrLow;
  //
  // Allocate and initialize the Rx/Tx buffers and buffer descriptors
  //
  _AllocBufferDescs();
  _InitRxBufferDescs();
  _InitTxBufferDesc();
  //
  // Start the ENET/MAC module
  //
  _ENET_RXSTR &= ~(1uL << _RXSTR_DMA_EN_BIT);   // Force an ENET abort by software for the receive block
  _ENET_TXSTR &= ~(1uL << _TXSTR_DMA_EN_BIT);   // Force an ENET abort by software for the transmit block
  _ENET_IER    = 0x0;                           // Disable all interrupts
  _ENET_ISR    = 0xFFFFFFFF;                    // Reset all interrupts
  //
  // Setup Descriptor Fetch timing values for receiver
  //
  RegValue    = _ENET_RXSTR;
  RegValue   &= ~_RXSTR_DFETCH_DLY_MASK;
  RegValue   |= _RXSTR_DFETCH_DLY_DEFAULT;
  RegValue   |= (1 << 6);                       // DMA discards damaged frames automatically
  RegValue   |= (1 << 7);                       // DMA discards frames with late collision set automatically
  RegValue   |= _RXSTR_DFETCH_DLY_DEFAULT;

  _ENET_RXSTR = RegValue;
  //
  // Setup Descriptor Fetch timing values for transmitter
  //
  RegValue    = _ENET_TXSTR;
  RegValue   &= ~_TXSTR_DFETCH_DLY_MASK;
  RegValue   |=  _TXSTR_DFETCH_DLY_DEFAULT;
  _ENET_TXSTR = RegValue;
  RegValue   |= (1uL << _TXSTR_URUN_BIT);
  _ENET_TXSTR = RegValue;

  _ENET_RXCR = (1uL << _RXCR_NEXT_EN_BIT);      // Enable next descriptor fetch
  //
  // Finally initialize interrupts and enable reception and transmission
  //
  _IntInit();                                   // Initialize Interrupt controller of CPU
  _EnableTx();
  _EnableRx();
  _EnableRxInt();                               // Enable Rx interrupt of ENET MAC
  _EnableTxInt();                               // Enable Tx interrupt of ENET MAC
  return 0;
}

/*********************************************************************
*
*       Driver API Table
*
**********************************************************************
*/
const IP_HW_DRIVER IP_Driver_STR912 = {
  _Init,
  _SendPacketIfTxIdle,
  _GetPacketSize,
  _ReadPacket,
  _Timer,
  _Control
};

/*************************** End of file ****************************/
