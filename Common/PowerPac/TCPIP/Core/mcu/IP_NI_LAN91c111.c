/*********************************************************************
*               SEGGER MICROCONTROLLER GmbH & Co. KG                 *
*       Solutions for real time microcontroller applications         *
**********************************************************************
*                                                                    *
*       (C) 2003 - 2008   SEGGER Microcontroller GmbH & Co. KG       *
*                                                                    *
*       www.segger.com     Support: support@segger.com               *
*                                                                    *
**********************************************************************
*                                                                    *
*       TCP/IP stack for embedded applications                       *
*                                                                    *
**********************************************************************
----------------------------------------------------------------------
File    : IP_NI_LAN91C111.c
Purpose : Network interface driver for SMSC91C111
--------  END-OF-HEADER  ---------------------------------------------
Tested on:
  PTW - Customer board with 91C111  [TS]

Notes:
  [N1] Register addresses
       Registers have a bank addr and and offset in the documentation. For simplicity,
       We refer to a register with a single parameter which contains both bank and offset as follows:
       RegIndex = Off + (Bank << 4)

Literature:
  [L1]  \\Fileserver\techinfo\Company\SMSC\91C111.pdf (SMSC LAN91C111 Datasheet Revision 1.7-B (11-16-04))

*/

#include "IP_Int.h"
#include "BSP.h"
#include "RTOS.h"
#include "IP_NI_LAN91C111.h"

/*********************************************************************
*
*       Defines, configurable
*
**********************************************************************
*/
#ifdef IP_LAN91C111_MAXUNIT
  #define MAX_UNITS   IP_LAN91C111_MAXUNIT
#else
  #define MAX_UNITS   1
#endif

/*********************************************************************
*
*       Defines, non configurable
*
**********************************************************************
*/

/*********************************************************************
*
*       PHY
*/
#define PHY_ST                       0x0001
#define PHY_READ                     0x0002
#define PHY_WRITE                    0x0001
// Generic MII registers.
#define MII_BMCR                     0x00       // Basic mode control register
#define MII_BSR                      0x01       // Basic mode status register
#define MII_PHYSID1                  0x02       // PHYS ID 1
#define MII_PHYSID2                  0x03       // PHYS ID 2
#define MII_ANAR                     0x04       // Auto-negotiation Advertisement register
#define MII_LPA                      0x05       // Link partner ability register
// PHY - Basic control register.
#define PHY_BMCR_CTST                (1 <<  7)  // Collision test
#define PHY_BMCR_FULLDPLX            (1 <<  8)  // Full duplex
#define PHY_BMCR_ANRESTART           (1 <<  9)  // Auto negotiation restart
#define PHY_BMCR_ISOLATE             (1 << 10)  // Disconnect PHY from MII
#define PHY_BMCR_PDOWN               (1 << 11)  // Powerdown PHY
#define PHY_BMCR_ANENABLE            (1 << 12)  // Enable auto negotiation
#define PHY_BMCR_SPEED100            (1 << 13)  // Select 100Mbps
#define PHY_BMCR_LOOPBACK            (1 << 14)  // TXD loopback bits
#define PHY_BMCR_RESET               (1 << 15)  // Reset PHY
// Basic status register.
#define PHY_BSR_LSTATUS              0x0004     // Link status
#define PHY_BSR_ANEGCOMPLETE         0x0020     // Auto-negotiation complete
// Link partner ability register
#define PHY_LPA_100HALF              0x0080     // Can do 100mbps half-duplex
#define PHY_LPA_100FULL              0x0100     // Can do 100mbps full-duplex
#define PHY_LPA_100BASE4             0x0200     // Can do 100mbps 4k packets
// SMSC91C111 ID
#define PHY_COMPANY_ID               0x0016
#define PHY_MANUFACTOR_ID            0xF840

#define SMSC91C111_PHYADDR           0          // Fixed to 0

#define PHY_MII_CLK_1                (1 << 2)
#define PHY_MII_CLK_0                (0 << 2)
#define PHY_MII_MDO_0                ((0 << 0) | (1 << 3))
#define PHY_MII_MDO_1                ((1 << 0) | (1 << 3))
#define PHY_MII_MDO_Z                ((0 << 0) | (0 << 3))

/*********************************************************************
*
*       MAC
*
*/

#define REG_BANK_SEL_OFFSET          0x0007

#define REG_TCR                      0x0000
#define REG_EPHSR                    0x0001
#define REG_RCR                      0x0002
#define REG_ECR                      0x0003
#define REG_MIR                      0x0004
#define REG_RPCR                     0x0005
#define REG_RESERVED_0               0x0006

#define REG_CFG                      0x0010
#define REG_BASE                     0x0011
#define REG_MAC_0                    0x0012
#define REG_MAC_2                    0x0013
#define REG_MAC_4                    0x0014
#define REG_GENERAL                  0x0015
#define REG_CTR                      0x0016

#define REG_MMU                      0x0020
#define REG_PNR                      0x0021
#define REG_FIFO                     0x0022
#define REG_PTR                      0x0023
#define REG_DATA_0                   0x0024
#define REG_DATA_2                   0x0025
#define REG_INT                      0x0026

#define REG_MULTICAST_0              0x0030
#define REG_MULTICAST_2              0x0031
#define REG_MULTICAST_4              0x0032
#define REG_MULTICAST_6              0x0033
#define REG_MGMT                     0x0034
#define REG_REV                      0x0035
#define REG_ERCV                     0x0036

#define REG_INT_MASK_EN              0xFF00  // Mask out interrupt acknowledge bits

#define MASK_RPCR_LED_A_LINK         0x0000  // 0 : Logical OR of 100 Mbps or 10 Mbps link detected
#define MASK_RPCR_LED_A_LINK_10      0x0040  // 2 : 10 Mbps link detected
#define MASK_RPCR_LED_A_FULL_DUPLEX  0x0060  // 3 : Full Duplex Mode Enabled
#define MASK_RPCR_LED_A_RX_TX        0x0080  // 4 : Tx or Rx packet occurred
#define MASK_RPCR_LED_A_LINK_100     0x00A0  // 5 : 100 Mbps link detected
#define MASK_RPCR_LED_A_RX           0x00C0  // 6 : Rx packet occurred
#define MASK_RPCR_LED_A_TX           0x00E0  // 7 : Tx packet occurred
#define MASK_RPCR_LED_B_LINK         0x0000  // 0 : Logical OR of 100 Mbps or 10 Mbps link detected
#define MASK_RPCR_LED_B_LINK_10      0x0008  // 2 : 10 Mbps link detected
#define MASK_RPCR_LED_B_FULL_DUPLEX  0x000C  // 3 : Full Duplex Mode Enabled
#define MASK_RPCR_LED_B_RX_TX        0x0010  // 4 : Tx or Rx packet occurred
#define MASK_RPCR_LED_B_LINK_100     0x0014  // 5 : 100 Mbps link detected
#define MASK_RPCR_LED_B_RX           0x0018  // 6 : Rx packet occurred
#define MASK_RPCR_LED_B_TX           0x001C  // 7 : Tx packet occurred

#define MMU_CMD_NOP                  0x0000  // No op.
#define MMU_CMD_TX_ALLOC             0x0020  // Request Tx buffer
#define MMU_CMD_RESET                0x0040  // Reset MMU
#define MMU_CMD_RX_REMOVE            0x0060  // Remove           Rx frame from Rx FIFO.
#define MMU_CMD_RX_REMOVE_REL        0x0080  // Remove & release Rx frame from Rx FIFO.
#define MMU_CMD_REL_PKT              0x00A0  // Release packet with packet number in REG_PNR
#define MMU_CMD_TX_PKT               0x00C0  // Send packet
#define MMU_CMD_TX_RESET             0x00E0  // Reset Tx FIFOs

#define PKT_FRAME_SIZE_OVRHD         6       // Number of overhead bytes in frame.

#define PKT_FRAME_SIZE_MASK          0x07FE
#define PKT_FRAME_CTRL_MASK          0xFF00
#define PKT_FRAME_LAST_OCTET_MASK    0x00FF

#define PKT_FRAME_STATUS_NONE        0x00
#define PKT_FRAME_IX_STATUS          0x00
#define PKT_FRAME_IX_SIZE            0x02
#define PKT_FRAME_IX_DATA            0x04

#define PKT_FRAME_CTRL_TX_CRC        (1 << 12)
#define PKT_FRAME_CTRL_TX_ODD        (1 << 13)
#define PKT_FRAME_CTRL_RX_ODD        (1 << 13)

typedef struct {
  unsigned Bank;
  unsigned RegPtr;  // Has to be saved if receive interrupt occurs during prepare for send
} CONTEXT;

typedef struct {
  volatile U16 * pHardware;
} LAN91C111_INST;


/*********************************************************************
*
*       Static data
*
**********************************************************************
*/
static OS_U8    _IsInited;  // Flag for check, if PHY and MAC are initialzed
static OS_U8    _TxIsBusy;

static LAN91C111_INST _aInst[MAX_UNITS];
extern char IP_RxTaskStarted;

//
// Statistics
//
#if DEBUG
  int _TxSendCnt;
  int _TxIntCnt;
  int _RxCnt;
  int _RxIntCnt;
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
*       _SelBank
*/
static void _SelBank(unsigned Unit, unsigned Bank) {
  LAN91C111_INST * pInst;

  pInst = &_aInst[Unit];
  *(pInst->pHardware + REG_BANK_SEL_OFFSET) = Bank;
}


/*********************************************************************
*
*       _MAC_ReadReg
*/
static OS_U16 _MAC_ReadReg(unsigned Unit, OS_U16 Reg) {
  OS_U16 v;
  LAN91C111_INST * pInst;

  pInst = &_aInst[Unit];
  _SelBank(Unit, Reg >> 4);
  v = *(pInst->pHardware + (Reg & 7));
  return v;
}

/*********************************************************************
*
*       _MAC_WriteReg
*/
static void _MAC_WriteReg(unsigned Unit, OS_U16 Reg, OS_U16 Data) {
  LAN91C111_INST * pInst;

  pInst = &_aInst[Unit];
  _SelBank(Unit, Reg >> 4);
  *(pInst->pHardware + (Reg & 7)) = Data;
}

/*********************************************************************
*
*       _SaveContext
*/
static void _SaveContext(unsigned Unit, CONTEXT * pContext) {
  LAN91C111_INST * pInst;

  pInst = &_aInst[Unit];
  pContext->Bank   = *(pInst->pHardware + REG_BANK_SEL_OFFSET);
  pContext->RegPtr = _MAC_ReadReg(Unit, REG_PTR);
}

/*********************************************************************
*
*       _RestoreContext
*/
static void _RestoreContext(unsigned Unit, CONTEXT * pContext) {
  LAN91C111_INST * pInst;

  pInst = &_aInst[Unit];
  _MAC_WriteReg(Unit, REG_PTR, pContext->RegPtr);
  *(pInst->pHardware + REG_BANK_SEL_OFFSET) = pContext->Bank;
}

/*********************************************************************
*
*       _PHY_MII_Write0
*
*  Function description
*    Outputs one clock with data high.
*/
static void _PHY_MII_Write0(unsigned Unit) {
  _MAC_WriteReg(Unit, REG_MGMT, 0x3330 | PHY_MII_CLK_0 | PHY_MII_MDO_0);  // MDO = 0
  _MAC_WriteReg(Unit, REG_MGMT, 0x3330 | PHY_MII_CLK_1 | PHY_MII_MDO_0);  // CLK = 1
  _MAC_WriteReg(Unit, REG_MGMT, 0x3330 | PHY_MII_CLK_0 | PHY_MII_MDO_0);  // CLK = 0
}

/*********************************************************************
*
*       _PHY_MII_Write1
*
*  Function description
*    Outputs one clock with data low.
*/
static void _PHY_MII_Write1(unsigned Unit) {
  _MAC_WriteReg(Unit, REG_MGMT, 0x3330 | PHY_MII_CLK_0 | PHY_MII_MDO_1);  // MDO = 1
  _MAC_WriteReg(Unit, REG_MGMT, 0x3330 | PHY_MII_CLK_1 | PHY_MII_MDO_1);  // CLK = 1
  _MAC_WriteReg(Unit, REG_MGMT, 0x3330 | PHY_MII_CLK_0 | PHY_MII_MDO_1);  // CLK = 0
}

/*********************************************************************
*
*       _PHY_MII_OutputSync
*
*  Function description
*    Write 32 clocks start sequence as described in [L1] 7.5.3 .
*/
static void _PHY_MII_OutputSync(unsigned Unit) {
  int i;

  for (i = 0; i < 32; i++) {
    _PHY_MII_Write1(Unit);
  }
}

/*********************************************************************
*
*       _PHY_WriteReg
*/
static void _PHY_WriteReg(unsigned Unit, U8 RegAddr, U16 Data) {
  int i;
  U32 v;

  //
  // Write Preample
  //
  _MAC_WriteReg(Unit, REG_MGMT, 0x3330 | PHY_MII_CLK_0 | PHY_MII_MDO_1);  // MDO = 1
  _PHY_MII_OutputSync(Unit);
  //
  // Write 32 Bits with ST, OP, PHY addr., REG addr., turnaround, data
  //
  v = 0
      | (PHY_ST             << 30)  // ST code
      | (PHY_WRITE          << 28)  // OP code
      | (SMSC91C111_PHYADDR << 23)  // PHY addr.
      | (RegAddr            << 18)  // Reg addr.
      | (0x2                << 16)  // b10 is written for turnaround
      | (Data               <<  0)  // Data
      ;
  for (i = 0; i < 32; i++) {
    if((v & (U32)(1 << 31)) == 0) {
      _PHY_MII_Write0(Unit);
    } else {
      _PHY_MII_Write1(Unit);
    }
    v <<= 1;
  }
  //
  // Release Bus
  //
  _MAC_WriteReg(Unit, REG_MGMT, 0x3330 | PHY_MII_CLK_0 | PHY_MII_MDO_Z);  // Disable output
}

/*********************************************************************
*
*       _PHY_ReadReg
*/
static U16 _PHY_ReadReg(unsigned Unit, U8 RegAddr) {
  int i;
  unsigned Data;

  //
  // Write Preample
  //
  _MAC_WriteReg(Unit, REG_MGMT, 0x3330 | PHY_MII_CLK_0 | PHY_MII_MDO_1);  // MDO = 1
  _PHY_MII_OutputSync(Unit);
  //
  // Write 14 Bits with ST, OP, PHY addr., REG addr.
  //
  Data = 0
         | (PHY_ST             << 14)  // ST code
         | (PHY_READ           << 12)  // OP code
         | (SMSC91C111_PHYADDR <<  7)  // PHY addr.
         | (RegAddr            <<  2)  // Reg addr.
         ;
  for (i = 0; i < 14; i++) {
    if((Data & (1 << 15)) == 0) {
      _PHY_MII_Write0(Unit);
    } else {
      _PHY_MII_Write1(Unit);
    }
    Data <<= 1;
  }
  //
  // Release Bus and output 1 turn around bit
  //
  _MAC_WriteReg(Unit, REG_MGMT, 0x3330 | PHY_MII_CLK_0 | PHY_MII_MDO_Z);    // Disable output
  _MAC_WriteReg(Unit, REG_MGMT, 0x3330 | PHY_MII_CLK_1 | PHY_MII_MDO_Z);    // CLK = 1
  _MAC_WriteReg(Unit, REG_MGMT, 0x3330 | PHY_MII_CLK_0 | PHY_MII_MDO_Z);    // CLK = 0

  //
  // Read 16 data bits
  //
  for (i = 0; i < 16; i++) {
    _MAC_WriteReg(Unit, REG_MGMT, 0x3330 | PHY_MII_CLK_1 | PHY_MII_MDO_Z);  // CLK = 1
    _MAC_WriteReg(Unit, REG_MGMT, 0x3330 | PHY_MII_CLK_1 | PHY_MII_MDO_Z);  // CLK = 1
    Data <<= 1;
    Data |= ((_MAC_ReadReg(Unit, REG_MGMT) & (1 << 1)) >> 1);               // Read MDI pin
    _MAC_WriteReg(Unit, REG_MGMT, 0x3330 | PHY_MII_CLK_0 | PHY_MII_MDO_Z);  // CLK = 0
  }
  //
  // Output final bit (which does not transfer data)
  //
  _MAC_WriteReg(Unit, REG_MGMT, 0x3330 | PHY_MII_CLK_1 | PHY_MII_MDO_Z);    // CLK = 1
  _MAC_WriteReg(Unit, REG_MGMT, 0x3330 | PHY_MII_CLK_0 | PHY_MII_MDO_Z);    // CLK = 0
  return (U16)Data;
}

/*********************************************************************
*
*       _PHY_Init
*/
static int _PHY_Init(unsigned Unit) {
  U16 CompanyId;
  U16 ManufactorId;
  U16 v;

  //
  // Try to detect PHY on fixed addr 0
  //
  CompanyId    = _PHY_ReadReg(Unit, MII_PHYSID1);
  ManufactorId = _PHY_ReadReg(Unit, MII_PHYSID2);
  if ((CompanyId != PHY_COMPANY_ID) || (ManufactorId != PHY_MANUFACTOR_ID)) {
    IP_WARN((IP_MTYPE_DRIVER, "DRIVER: no PHY found."));
  } else {
    IP_LOG((IP_MTYPE_DRIVER | IP_MTYPE_INIT, "DRIVER: Found PHY with Id 0x%x at addr 0x00", PHY_COMPANY_ID));
  }
  //
  // Reset PHY
  //
  v  = _PHY_ReadReg(Unit, MII_BMCR);
  v |= (1 << 15);
  _PHY_WriteReg(Unit, MII_BMCR, v);
  //
  // Wait until PHY is out of RESET
  //
  OS_Delay(50);  // PHY reset is guarenteed to finish within 50 ms. According to [L1] 9.1 , no PHY access should take place within this time.
  while (1) {
    v = _PHY_ReadReg(Unit, MII_BMCR);
    if ((v & (1 << 15)) == 0) {
      break;
    }
  }
  //
  // Clear Full duplex bits
  //
  v  = _PHY_ReadReg(Unit, MII_ANAR);
  v &= ~((1 << 6) | (1 << 8));  // Clear FDX bits
  _PHY_WriteReg(Unit, MII_ANAR, v);
  //
  // Connect MII-interface by clearing "ISOLATE" and start auto negotiation
  //
  v = 0
      | PHY_BMCR_ANRESTART  // Restart auto-negotiation
      | PHY_BMCR_ANENABLE   // Enable  auto-negotiation
      ;
  _PHY_WriteReg(Unit, MII_BMCR, v);
  while (1) {
    v = _PHY_ReadReg(Unit, MII_BMCR);
    if ((v & (1 << 9)) == 0) {
      break;
    }
  }
  return 0;
}

/*********************************************************************
*
*       _PHY_GetLinkState
*/
static void _PHY_GetLinkState(unsigned Unit, U32 * pDuplex, U32 * pSpeed) {
  U32 Bmsr;
  U32 Bmcr;
  U32 Speed;
  U32 Duplex;

  Speed  = 0;
  Duplex = IP_DUPLEX_UNKNOWN;
  //
  // Get Link Status from PHY status reg. Requires 2 reads
  //
  Bmsr = _PHY_ReadReg(Unit, MII_BSR);
  if (Bmsr & PHY_BSR_LSTATUS) {  // Link established ?
    Bmcr = _PHY_ReadReg(Unit, MII_BMCR);
    if (Bmcr & PHY_BMCR_SPEED100) {
      Speed = IP_SPEED_100MHZ;
    } else {
      Speed = IP_SPEED_10MHZ;
    }
    if (Bmcr & PHY_BMCR_FULLDPLX) {
      Duplex = IP_DUPLEX_FULL;
    } else {
      Duplex = IP_DUPLEX_HALF;
    }
  }
  *pDuplex = Duplex;
  *pSpeed  = Speed;
}

/*********************************************************************
*
*       _EnableTx
*
*  Function description
*    Set TXENA to reenable transmission
*/
static void _EnableTx(unsigned Unit) {
  U16 v;

  v  = _MAC_ReadReg(Unit, REG_TCR);
  if ((v & (1 << 0)) == 0) {
    v |= (1 << 0);
    _MAC_WriteReg(Unit, REG_TCR, v);
  }
}

/*********************************************************************
*
*       _EMAC_Init
*
*  Function description
*    Initialize the EMAC unit
*/
static void _EMAC_Init(unsigned Unit) {
  OS_U16  v;

  //
  // Initiate SW reset
  //
  v  = _MAC_ReadReg(Unit, REG_RCR);
  v |= (1 << 15);   // Initiate SW reset
  _MAC_WriteReg(Unit, REG_RCR, v);
  IP_OS_Delay(50);
  v &= ~(1 << 15);  // Clr SW reset
  _MAC_WriteReg(Unit, REG_RCR, v);
  IP_OS_Delay(50);
  //
  // Reset MMU
  //
  _MAC_WriteReg(Unit, REG_MMU, MMU_CMD_RESET);
  do {
    v = _MAC_ReadReg(Unit, REG_MMU);  // Wait until BUSY bit signals MMU finished
  } while (v & (1 << 0));
  //
  // Initialize MAC Regs
  //
  v = 0
      | (0 << 15)  // 0: Half duplex, 1: Full duplex
      | (0 << 13)  // 0: EPH Lpbk disabled, 1: EPH Lpbk enabled
      | (0 << 12)  // 0: Ignores SQET, 1: Traps SQET
      | (0 << 11)  // 0: Rx Lpbk disabled, 1: Rx Lpbk enabled
      | (0 << 10)  // 0: Ignore  Carrier, 1: Monitor Carrier
      | (0 <<  8)  // 0: Tx CRC appended, 1: Tx CRC NOT appended
      | (1 <<  7)  // 0: Tx frames NOT padded, 1: Tx frames padded
      | (0 <<  2)  // 0: Tx Collisions NOT forced, 1: Tx Collisions forced
      | (0 <<  1)  // 0: Force Lpbk output pin low, 1: Force Lpbk output pin high
      | (0 <<  0)  // 0: Tx disabled, 1: Tx enabled
      ;
  _MAC_WriteReg(Unit, REG_TCR, v);

  v = 0
      | (0 << 14)  // 0: Rx frames immediately, 1: Rx frames after 12-bit carrier sense
      | (1 << 13)  // 0: Do NOT abort Rx Collision frames, 1: Abort Rx Collision frames
      | (1 <<  9)  // 0: Rx CRC appended, 1: Rx CRC NOT appended
      | (0 <<  8)  // 0: Rx disabled, 1: Rx enabled
      | (1 <<  2)  // 0: Rx Tbl Multicast addresses only, 1: Rx all Multicast addresses
      | (1 <<  0)  // 0: Rx dest frames only, 1: Rx all frames
      ;
  _MAC_WriteReg(Unit, REG_RCR, v);

  v = 0
      | (0 << 13)  // 0: 10 Mbps, 1: 100 Mbps
      | (0 << 12)  // 0: Half duplex, 1: Full duplex
      | (1 << 11)  // 0: Auto negotiation disabled, 1: Auto negotiation enabled
      | (1 << 11)  // 0: Auto negotiation disabled, 1: Auto negotiation enabled
      | MASK_RPCR_LED_A_LINK
      | MASK_RPCR_LED_B_RX_TX
      ;
  _MAC_WriteReg(Unit, REG_RPCR,v);

  v = 0x20B1       // Reserved bits. Reset state described in [L1] 8.11
      | (1 << 15)  // 0: EPH low power mode enabled, 1: EPH low power mode disabled
      | (1 << 12)  // 0: Wait states enabled, 1: Wait states disabled
      | (0 <<  9)  // 0: Internal PHY, 1: External PHY
      ;
  _MAC_WriteReg(Unit, REG_CFG, v);

  v = 0x1210       // Reserved bits. Reset state described in [L1] 8.15
      | (1 << 11)  // 0: Manual packet release on successful transmission, 1: Automatic packet release on successful transmission
      ;
  _MAC_WriteReg(Unit, REG_CTR, v);
}

/*********************************************************************
*
*       _UpdateLinkState
*/
static void _UpdateLinkState(unsigned Unit) {
  U32  Speed;
  U32  Duplex;

  _PHY_GetLinkState(Unit, &Duplex, &Speed);
  IP_SetCurrentLinkState(Duplex, Speed);
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
static int _SendPacket(unsigned Unit) {
  OS_U32 NumBytes;
  OS_U16 v;
  OS_U16 PktSize;
  OS_U16 NumBytesRem;
  OS_U16 Data;
  OS_U16 *pData;
  void   *pPkt;
  int    TryCnt = 0;

  do {
    IP_GetNextOutPacket(&pPkt, &NumBytes);
    if (NumBytes == 0) {
      _TxIsBusy = 0;  // No more data to send
      return 0;
    }
    IP_LOG((IP_MTYPE_DRIVER, "DRIVER: Sending packet: %d bytes", NumBytes));
    //
    // Alloc buffer inside of the Ethernet controller.
    // If this fails (which should not normally happen), release packet anyhow and try sending the next one.
    //
    _MAC_WriteReg(Unit, REG_MMU, MMU_CMD_TX_ALLOC);
    while (1) {
      v = _MAC_ReadReg(Unit, REG_INT);
      if (v & (1 << 3)) {  // Allocate succeeded; check ALLOC INT bit ([L1] 8.21) first as it seems that ALLOC FAILED bit needs some time to be set/cleared
        goto Allocated;
      }
      v = _MAC_ReadReg(Unit, REG_PNR);
      if (v & (1 << 15)) {  // Check for FAILED bit [L1] 8.17
        IP_LOG((IP_MTYPE_DRIVER, "Allocation failed."));
        break;
      }
    }
    //
    // Reset MMU, trying (hoping ...) to free buffers
    //
    _MAC_WriteReg(Unit, REG_MMU, 0x0040);  // Free all packets, clear interrupts and reset FIFO pointers
  } while (TryCnt++ < 10);
  IP_RemoveOutPacket();
  return 1;  // Could not send


Allocated:
  INC(_TxSendCnt);
  //
  // Buffer has been allocated, we are now ready to send
  //
  v = _MAC_ReadReg(Unit, REG_PNR);
  v = (v >> 8) & 0x3F;  // Packet number is only 6-bit wide
  _MAC_WriteReg(Unit, REG_PNR, v);  // Use packet number
  //
  // Write Tx Pkt Frame
  //
  v = 0
      | (0                   << 15)  // 0: Tx FIFO Pkt Ptr, 1: Rx FIFO Pkt Ptr
      | (1                   << 14)  // 0: Manual pointer inc., 1: Auto pointer inc.
      | (0                   << 13)  // 0: Write, 1: Read
      | (0                   << 12)  // 0: Early Tx underrun detection disabled, 1: Early Tx underrun detection enabled
      | (0                   << 12)  // 0: Early Tx underrun detection disabled, 1: Early Tx underrun detection enabled
      | (PKT_FRAME_IX_STATUS <<  0)
      ;
  _MAC_WriteReg(Unit, REG_PTR, v);
  _MAC_WriteReg(Unit, REG_DATA_0, PKT_FRAME_STATUS_NONE);
  //
  // Write packet size
  //
  NumBytesRem = NumBytes;
  PktSize     = NumBytes & PKT_FRAME_SIZE_MASK;
  PktSize    +=            PKT_FRAME_SIZE_OVRHD;
  _MAC_WriteReg(Unit, REG_DATA_0, PktSize);
  //
  // Write packet data in 16 bit units
  //
  pData = (OS_U16*)pPkt;
  while (NumBytesRem > 1) {
    Data = *pData++;
    _MAC_WriteReg(Unit, REG_DATA_0, Data);
    NumBytesRem -= 2;
  }
  Data = PKT_FRAME_CTRL_TX_CRC;
  if (NumBytesRem > 0) {  // Write last byte, if available
    Data      |=  PKT_FRAME_CTRL_TX_ODD;
    Data      |= *(OS_U8*)pData;
  }
  _MAC_WriteReg(Unit, REG_DATA_0, Data);
  //
  // Remove packet from send Q
  //
  IP_RemoveOutPacket();
  //
  // Send packet
  //
  _MAC_WriteReg(Unit, REG_MMU, MMU_CMD_TX_PKT);
  //
  // Enable Tx int, Tx empty interrupt
  //
  v  = _MAC_ReadReg(Unit, REG_INT);
  v |= (3 << 9);
  v &= REG_INT_MASK_EN;
  _MAC_WriteReg(Unit, REG_INT, v);

  return 0;
}

/*********************************************************************
*
*       IP_NI_LAN91C111_ISR_Handler
*
*  Function description
*    This is the interrupt service routine for the NI (EMAC).
*    It handles all interrupts (Rx, Tx, Error).
*
* Note #1: Since LAN91C111 receive interrupt is cleared ONLY when the receive FIFO is empty
*          (see SMSC LAN91C111, Section 8.21 'RCV INT'), receive interrupts MUST be DISABLED
*          until the receive packet is read from the receive FIFO
*
*/
void IP_NI_LAN91C111_ISR_Handler(unsigned Unit) {
  OS_U16  IntStat;  // Always contains the last value read or written from/to REG_INT whatever operation was last
  OS_U16  v;
  CONTEXT Context;

  if (_IsInited == 0) {
    return;
  }
  _SaveContext(Unit, &Context);
  IntStat = _MAC_ReadReg(Unit, REG_INT);  // Get interrupt status flags
  //
  // Service receive interrupt
  //
  if (IntStat & (1 << 0)) {
    //
    // Disable RX INT. This is needed if we use the Rx task.
    //
    if (IP_RxTaskStarted) {
      IP_OS_DisableInterrupt();
      IntStat &= ~(1 << 8);  // Update interrupt mask to not be enabled later in ISR routine
      v  = IntStat;
      v &= REG_INT_MASK_EN;
      _MAC_WriteReg(Unit, REG_INT, v);
      IP_OS_EnableInterrupt();
    }
    //
    // Process Rx packet
    //
    INC(_RxIntCnt);
    IP_OnRx();  // Tell Ip stack that there is a new packet
  }
  //
  // Set TXENA for the case that it is disabled due to a not processed send error
  //
  _EnableTx(Unit);
  //
  // Service send or send error interrupt
  //
  if (_TxIsBusy && (IntStat & (1 << 2))) {  // Check for _TxIsBusy to indicate that a send process has been queued and
                                            // check TX EMPTY INT which indicates that the transfer already has been finished
    INC(_TxIntCnt);
    //
    // Disable Tx empty, Tx int
    //
    IntStat &= ~(3 << 9);  // Update interrupt mask to not be enabled later in ISR routine
    v  = IntStat;
    v &= REG_INT_MASK_EN;
    _MAC_WriteReg(Unit, REG_INT, v);
    //
    // Service send error. Release failed packet and reenable sending
    //
    if (IntStat & (1 << 1)) {  // Check for TX INT which indicates transmission failed (16COL, SQET, LATCOL, LOST_CARR)
      //
      // Release failed packet
      //
      do {
        v = _MAC_ReadReg(Unit, REG_MMU);
      } while (v & (1 << 0));                         // Wait for MMU to signal finished; make sure MMU accepts new command
      _MAC_WriteReg(Unit, REG_MMU, MMU_CMD_REL_PKT);  // Release failed packet with packet number in REG_PNR

      do {
        v  = _MAC_ReadReg(Unit, REG_MMU);             // Wait until BUSY bit signals MMU finished
      } while (v & (1 << 0));
      //
      // Acknowledge Tx int. bit
      //
      v  = IntStat;
      v &= REG_INT_MASK_EN;
      v |= (1 << 1);
      _MAC_WriteReg(Unit, REG_INT, v);
      _EnableTx(Unit);
    }
    //
    // Service send interrupt by acknowledging Tx empty bit
    //
    v  = IntStat;
    v &= REG_INT_MASK_EN;
    v |= (1 << 2);
    _MAC_WriteReg(Unit, REG_INT, v);
    //
    // Process next packet if available
    //
    _SendPacket(Unit);
  }
  //
  // Service RxOverrun interrupt
  //
  if (IntStat & (1 << 4)) {                  // Check for RxOverrun INT
    _MAC_WriteReg(Unit, REG_INT, (1 << 4));  // Acknowledge RxOverrun
    _MAC_WriteReg(Unit, REG_MMU, 0x0040);    // Free all packets, clear interrupts and reset FIFO pointers
  }
  _RestoreContext(Unit, &Context);
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
  if (_TxIsBusy == 0) {
    _SendPacket(Unit);
    _TxIsBusy = 1;
  }
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
*    Number of received bytes
*    0 if no complete packet is available.
*/
static int _GetPacketSize(unsigned Unit) {
  OS_U16 v;
  OS_U16 Size;
  OS_U16 CtrIx;
  OS_U16 Ctrl;

  v = _MAC_ReadReg(Unit, REG_FIFO);
  if (v & (1 << 15)) {  // Read FIFO port register to check if all data has been processed
    //
    // Enable RX INT. This is needed if we use the Rx task.
    //
    if (IP_RxTaskStarted) {
      IP_OS_DisableInterrupt();
      v  = _MAC_ReadReg(Unit, REG_INT);
      v |= (1 << 8);
      v &= REG_INT_MASK_EN;
      _MAC_WriteReg(Unit, REG_INT, v);
      IP_OS_EnableInterrupt();
    }
    return 0;           // No complete packet available
  } else {
    //
    // Read Size from packet frame
    //
    v = 0
        | (1                 << 15)  // 0: Tx FIFO Pkt Ptr, 1: Rx FIFO Pkt Ptr
        | (0                 << 14)  // 0: Manual pointer inc., 1: Auto pointer inc.
        | (1                 << 13)  // 0: Write, 1: Read
        | (PKT_FRAME_IX_SIZE <<  0)  // Set pointer to word containing packet size
        ;
    _MAC_WriteReg(Unit, REG_PTR, v);
    Size  = _MAC_ReadReg(Unit, REG_DATA_0);
    Size &= PKT_FRAME_SIZE_MASK;
    Size -= PKT_FRAME_SIZE_OVRHD;
    //
    // Read control from packet frame
    //
    CtrIx = PKT_FRAME_IX_DATA + Size;
    v = 0
        | (1     << 15)  // 0: Tx FIFO Pkt Ptr, 1: Rx FIFO Pkt Ptr
        | (0     << 14)  // 0: Manual pointer inc., 1: Auto pointer inc.
        | (1     << 13)  // 0: Write, 1: Read
        | (CtrIx <<  0)  // Set pointer to control byte located at end of data area
        ;
    _MAC_WriteReg(Unit, REG_PTR, v);
    Ctrl = _MAC_ReadReg(Unit, REG_DATA_0);
    v = _MAC_ReadReg(Unit, REG_REV);
    if (v == 0x90) {
      Ctrl |= PKT_FRAME_CTRL_RX_ODD;  // The LAN91C111 rev. A has a bug and never sets this bit. Force it set
    }
    if (Ctrl & PKT_FRAME_CTRL_RX_ODD) {  // If odd bit set, inc. size
      Size++;
    }
  }
  return Size;
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
static int _ReadPacket(unsigned Unit, OS_U8 *pDest, unsigned NumBytes) {
  OS_U16 *pData;
  OS_U16  NumBytesRem;
  OS_U16  v;
  OS_U8  *pDataLast;

  if (pDest) {
    pData       = (OS_U16*)pDest;
    NumBytesRem = NumBytes;
    //
    // Setup Rx Pkt frame ptr to data IX
    //
    v = 0
        | (1                 << 15)  // 0: Tx FIFO Pkt Ptr, 1: Rx FIFO Pkt Ptr
        | (1                 << 14)  // 0: Manual pointer inc., 1: Auto pointer inc.
        | (1                 << 13)  // 0: Write, 1: Read
        | (PKT_FRAME_IX_DATA <<  0)  // Set pointer to start of data area
        ;
    _MAC_WriteReg(Unit, REG_PTR, v);
    //
    // Read received packet frame
    //
    while (NumBytesRem > 1) {  // Read in units of 16 bit
      *pData++ = _MAC_ReadReg(Unit, REG_DATA_0);
      NumBytesRem -= 2;
    }
    if (NumBytesRem > 0) {  //  Check if we have a last byte to read
      pDataLast       = (OS_U8*)pData;
      *pDataLast      = (OS_U8)_MAC_ReadReg(Unit, REG_DATA_0);  // Mask out control byte and read last byte
    }
    IP_LOG((IP_MTYPE_DRIVER, "Packet: %d Bytes --- Read", NumBytes));
  } else {
    IP_LOG((IP_MTYPE_DRIVER, "Packet: %d Bytes --- Discarded", NumBytes));
  }
  //
  // Remove packet from buffer and wait until MMU signals finished state
  //
  _MAC_WriteReg(Unit, REG_MMU, MMU_CMD_RX_REMOVE_REL);
  do {
    v = _MAC_ReadReg(Unit, REG_MMU);
  } while (v & (1 << 0));  // Wait for MMU to signal finished
  INC(_RxCnt);
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
  CONTEXT Context;

  _SaveContext(Unit, &Context);
  _UpdateLinkState(Unit);
  _RestoreContext(Unit, &Context);
}

/*********************************************************************
*
*       _SetFilter
*
*  Function description
*    Sets the MAC filter(s)
*    The stack tells the driver which addresses should go thru the filter.
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
  _MAC_WriteReg(0, REG_MAC_0, *((U16*)pFilter->pHWAddr + 0));
  _MAC_WriteReg(0, REG_MAC_2, *((U16*)pFilter->pHWAddr + 1));
  _MAC_WriteReg(0, REG_MAC_4, *((U16*)pFilter->pHWAddr + 2));
  return 0;  // O.K.
}


/*********************************************************************
*
*       _Control
*
*  Function description
*    Various control functions
*/
static int _Control(unsigned Unit, int Cmd, void * p) {
  if (Cmd == IP_NI_CMD_POLL) {
    IP_NI_LAN91C111_ISR_Handler(Unit);
    return 0;
  } else if (Cmd == IP_NI_CMD_SET_FILTER) {
    return _SetFilter((IP_NI_CMD_SET_FILTER_DATA*)p);
  }
  return -1;
}

/*********************************************************************
*
*       _Init
*
*  Function description
*    General init function of the driver.
*    Called by the stack in the init phase before any other driver function.
*
*  Return value
*    0     O.K.
*    1     Error
*/
static int _Init(unsigned Unit) {
  U16 v;

  BSP_ETH_Init(Unit);
  _EMAC_Init(Unit);
  _PHY_Init(Unit);
  _UpdateLinkState(Unit);
  //
  // Enable LAN91C111 Receiver
  //
  v  = _MAC_ReadReg(Unit, REG_RCR);
  v |= (1 << 8);  // 0: Rx disable, 1: Rx enable
  _MAC_WriteReg(Unit, REG_RCR, v);
  //
  // Enable LAN91C111 Transmitter
  //
  _EnableTx(Unit);

  _MAC_WriteReg(Unit, REG_INT, 0x0104);  // Enable Rx int; acknowledge Tx empty int.

  _IsInited = 1;
  return 0;
}

/*********************************************************************
*
*       IP_NI_LAN91C111_ConfigAddr
*
*  Function description
*    Sets the base address for command and data address
*/
void IP_NI_LAN91C111_ConfigAddr(unsigned Unit, void* pBase) {
  if (_IsInited == 0) {
    _aInst[Unit].pHardware = (U16*)pBase;
  } else {
    IP_WARN((IP_MTYPE_DRIVER, "DRIVER: Can not configure HW address after init."));
  }
}

/*********************************************************************
*
*       Driver API Table
*
**********************************************************************
*/
const IP_HW_DRIVER IP_Driver_LAN91C111 = {
  _Init,
  _SendPacketIfTxIdle,
  _GetPacketSize,
  _ReadPacket,
  _Timer,
  _Control
};

/*************************** End of file ****************************/
