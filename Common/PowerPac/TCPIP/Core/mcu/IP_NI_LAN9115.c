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
File    : IP_NI_LAN9115.c
Purpose : Network interface driver for LAN9115 / LAN9215
--------  END-OF-HEADER  ---------------------------------------------
Tested on:
  PTW - AT91SAM7SE512 with LAN9115  [AW]

Notes
  - Filtering
    Filtering does not seem to work, so the driver uses promiscous mode (no HW-filtering), all filtering is done in SW
  - Logic for external PHY not yet implemented
  -
Literature:
  \\Fileserver\techinfo\Company\SMSC\LAN9115_Rev15.pdf
  \\Fileserver\techinfo\Company\SMSC\LAN9215.pdf

*/

#include "IP_Int.h"
#include "BSP.h"
#include "IP_NI_LAN9115.h"

/*********************************************************************
*
*       Defines, configurable
*
**********************************************************************
*/
#ifdef IP_LAN9115_MAXUNIT
  #define MAX_UNITS   IP_LAN9115_MAXUNIT
#else
  #define MAX_UNITS   1
#endif

#define  TX_FIFO_SIZE  5   // 5KB Tx FIFO size, results in 4608 bytes for TX data FIFO
#define  TX_FIFO_LOW_THRESHOLD   1600   //

#define ALLOW_FULL_DUPLEX  (0)

/*********************************************************************
*
*       Defines, non configurable
*
**********************************************************************
*/

#define ID_LAN9115    0x0115
#define ID_LAN9215    0x115A
#define ID_LAN9221    0x9221

/*********************************************************************
*
*       REG_
*
*  Registers (offsets) of the LAN9115 unit
*/

/*****  Data registers **********************************************/

#define REG_RX_DATA_FIFO          0x00
#define REG_RX_STATUS_FIFO        0x40
#define REG_RX_STATUS_FIFO_PEEK   0x44
#define REG_TX_DATA_FIFO          0x20
#define REG_TX_STATUS_FIFO        0x48
#define REG_TX_STATUS_FIFO_PEEK   0x4C

/*****  Slave registers *********************************************/

#define REG_ID_REV       0x50    // Identification / revision register 0x0115rrrr for 9115, 0x115Arrrr for 9215
#define REG_INT_CFG      0x54
#define REG_INT_STS      0x58
#define REG_INT_EN       0x5C
#define REG_BYTE_TEST    0x64
#define REG_FIFO_INT     0x68
#define REG_RX_CFG       0x6C
#define REG_TX_CFG       0x70
#define REG_HW_CFG       0x74
#define REG_RX_DP_CTRL   0x78
#define REG_RX_FIFO_INF  0x7C
#define REG_TX_FIFO_INF  0x80
#define REG_PMT_CTRL     0x84
#define REG_GPIO_CFG     0x88
#define REG_GPT_CFG      0x8C
#define REG_GPT_CNT      0x90
#define REG_ENDIAN       0x98
#define REG_FREE_RUN     0x9C
#define REG_RX_DROP      0xA0
#define REG_MAC_CSR_CMD  0xA4
#define REG_MAC_CSR_DATA 0xA8
#define REG_AFC_CFG      0xAC
#define REG_E2P_CMD      0xB0
#define REG_E2P_DATA     0xB4

/*****  Register bits, related to slave registers *******************/

#define MAC_CSR_BUSY_BIT     (31)
#define MAC_CSR_READ_BIT     (30)

#define MAC_CSR_BUSY_MASK    (1uL << MAC_CSR_BUSY_BIT)
#define MAC_CSR_READ_MASK    (1uL << MAC_CSR_READ_BIT)

#define HW_CFG_SRST_BIT          (0)
#define HW_CFG_EXT_PHY_EN_BIT    (2)
#define HW_CFG_SMI_SEL_BIT       (4)
#define HW_CFG_PHY_CLK_SEL_BIT   (5)
#define HW_CFG_MBO_BIT          (20)

#define HW_CFG_PHY_CLK_SEL_MASK (3uL << HW_CFG_PHY_CLK_SEL_BIT)
#define HW_CFG_SMI_SEL_MASK     (1uL << HW_CFG_SMI_SEL_BIT)
#define HW_CFG_EXT_PHY_EN_MASK  (1uL << HW_CFG_EXT_PHY_EN_BIT)
#define HW_CFG_SRST_MASK        (1uL << HW_CFG_SRST_BIT)
#define HW_CFG_MBO_MASK         (1uL << HW_CFG_MBO_BIT)

#define E2P_CMD_EPC_BUSY_BIT    (1)
#define E2P_CMD_EPC_BUSY_MASK   (1uL << E2P_CMD_EPC_BUSY_BIT)


#define TX_CFG_STOP_TX_BIT   (0)
#define TX_CFG_TX_ON_BIT     (1)
#define TX_CFG_TXSAO_BIT     (2)
#define TX_CFG_TXD_DUMP_BIT  (14)
#define TX_CFG_TXS_DUMP_BIT  (15)

#define TX_CFG_STOP_TX_MASK  (1uL << TX_CFG_STOP_TX_BIT)
#define TX_CFG_TX_ON_MASK    (1uL << TX_CFG_TX_ON_BIT)
#define TX_CFG_TXSAO_MASK    (1uL << TX_CFG_TXSAO_BIT)
#define TX_CFG_TXD_DUMP_MASK (1uL << TX_CFG_TXD_DUMP_BIT)
#define TX_CFG_TXS_DUMP_MASK (1uL << TX_CFG_TXS_DUMP_BIT)

/*****  Register bits, related to MAC registers *********************/

#define MII_BUSY_BIT     (0)
#define MII_WRITE_BIT    (1)

#define MII_BUSY_MASK    (1uL << MII_BUSY_BIT)
#define MII_WRITE_MASK   (1uL << MII_WRITE_BIT)

/*****  MAC control and status registers index **********************/

#define MAC_CR           1
#define ADDRH            2
#define ADDRL            3
#define HASHH            4
#define HASHL            5
#define MII_ACC          6
#define MII_DATA         7
#define FLOW             8
#define VLAN1            9
#define VLAN2           10
#define WUFF            11
#define WUCSR           12

#define MAC_CR_RXEN_MASK   (1 << 2)
#define MAC_CR_TXEN_MASK   (1 << 3)

/*****  Register bits of Interrupt enable register ******************/

#define RSFL_INT_EN_BIT    (3)
#define TSFL_INT_EN_BIT    (7)
#define TDFA_INT_EN_BIT    (9)

/*****  Assign interrupt enable bits ********************************/

#define RX_INT_EN_BIT     RSFL_INT_EN_BIT
#define RX_INT_EN_MASK    (1uL << RSFL_INT_EN_BIT)

#define TX_INT_EN_BIT     TSFL_INT_EN_BIT
#define TX_INT_EN_MASK    (1uL << TSFL_INT_EN_BIT)

/*********************************************************************
*
*       PHY
*/
// Generic MII registers.
#define MII_BMCR                  0x00       // Basic mode control register
#define MII_BSR                   0x01       // Basic mode status register
#define MII_PHYSID1               0x02       // PHYS ID 1
#define MII_PHYSID2               0x03       // PHYS ID 2
#define MII_ANAR                  0x04       // Auto-negotiation Advertisement register
#define MII_LPA                   0x05       // Link partner ability register

// PHY - Basic control register.
#define PHY_BMCR_CTST             (1 <<  7)  // Collision test
#define PHY_BMCR_FULLDPLX         (1 <<  8)  // Full duplex
#define PHY_BMCR_ANRESTART        (1 <<  9)  // Auto negotiation restart
#define PHY_BMCR_ISOLATE          (1 << 10)  // Disconnect PHY from MII
#define PHY_BMCR_PDOWN            (1 << 11)  // Powerdown PHY
#define PHY_BMCR_ANENABLE         (1 << 12)  // Enable auto negotiation
#define PHY_BMCR_SPEED100         (1 << 13)  // Select 100Mbps
#define PHY_BMCR_LOOPBACK         (1 << 14)  // TXD loopback bits
#define PHY_BMCR_RESET            (1 << 15)  // Reset PHY

// Basic status register.
#define PHY_BSR_LSTATUS           0x0004     // Link status
#define PHY_BSR_ANEGCOMPLETE      0x0020     // Auto-negotiation complete

// Link partner ability register
#define PHY_LPA_100HALF           0x0080     // Can do 100mbps half-duplex
#define PHY_LPA_100FULL           0x0100     // Can do 100mbps full-duplex
#define PHY_LPA_100BASE4          0x0200     // Can do 100mbps 4k packets

#define PHY_LPA_100     (PHY_LPA_100FULL | PHY_LPA_100HALF | PHY_LPA_100BASE4)

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

typedef struct LAN9115_INST LAN9115_INST;

struct LAN9115_INST {
  U32*                   pBase;
  IP_PHY_CONTEXT         PhyContext;
  const IP_PHY_DRIVER  * Phy_pDriver;
  U16            TxTag;
  U8             PhyAddrIsValid;
  U8             UseExternalPhy;
  U8             IsEnabled;
  U8             TxIsBusy;
#if DEBUG
  U32            TxSendCnt;
  U32            TxIntCnt;
  U32            RxCnt;
  U32            RxIntCnt;
#endif
};

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

static int          _InterruptLockCnt = 1;

static LAN9115_INST _aInst[MAX_UNITS];

/****** Statistics **************************************************/

#if DEBUG
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
*       _LockInterrupt();
*/
static void _LockInterrupt(LAN9115_INST * pInst) {
  IP_OS_DisableInterrupt();
  _InterruptLockCnt++;
  //
  // Disable Interrupts of LAN9115 if required
  //

  IP_OS_EnableInterrupt();
}

/*********************************************************************
*
*       _UnLockInterrupt();
*/
static void _UnLockInterrupt(LAN9115_INST * pInst) {
  IP_OS_DisableInterrupt();
  _InterruptLockCnt--;
  if (_InterruptLockCnt == 0) {
    //
    // Enable Interrupts of LAN9115 if required
    //
  }
  IP_OS_EnableInterrupt();
}

/*********************************************************************
*
*       _ReadReg
*
*  Function description
*    Reads the data from one register of the LAN9115 from the given offset
*/
static U32 _ReadReg(LAN9115_INST * pInst, unsigned Offset) {
           U32  r;
  volatile U32* pSrc;

  pSrc = pInst->pBase + (Offset/4);
  r    = *pSrc;
  return r;
}

/*********************************************************************
*
*       _WriteReg
*
*  Function description
*    Writes the data into one register of the LAN9115 at the given offset
*/
static void _WriteReg(LAN9115_INST * pInst, unsigned Offset,  unsigned  Data) {
  U32* pDest;

  pDest  = pInst->pBase + (Offset/4);
  *pDest = Data;
}

/*********************************************************************
*
*       _WaitUntilMacIsReady()
*/
static void _WaitUntilMacIsReady(LAN9115_INST * pInst) {
  do {
  } while(_ReadReg(pInst, REG_MAC_CSR_CMD) & (U32) MAC_CSR_BUSY_MASK);
}

/*********************************************************************
*
*       _WaitUntilMiiIsReady()
*/
static void _WaitUntilMiiIsReady(LAN9115_INST * pInst) {
  U32 IsBusy;
  do {
    _WaitUntilMacIsReady(pInst);
    _WriteReg(pInst, REG_MAC_CSR_CMD, (MAC_CSR_BUSY_MASK | MAC_CSR_READ_MASK | MII_ACC) );
    _WaitUntilMacIsReady(pInst);
    IsBusy = (_ReadReg(pInst, REG_MAC_CSR_DATA) & MII_BUSY_MASK);
  } while (IsBusy);
}

/*********************************************************************
*
*        _ReadMacReg()
*
*  Function description
*    Reads the selected register form the MAC unit of the LAN9115
*/
static U32 _ReadMacReg(LAN9115_INST * pInst, U32 RegIndex) {
  U32 r;

  IP_OS_DisableInterrupt();              // Disable Interrupts
  _WriteReg(pInst, REG_MAC_CSR_CMD, 0);  // Flush previous write */
  _WriteReg(pInst, REG_MAC_CSR_CMD, (MAC_CSR_BUSY_MASK | MAC_CSR_READ_MASK | RegIndex));
  r = _ReadReg(pInst, REG_BYTE_TEST);    // force flush of previous write
  _WaitUntilMacIsReady(pInst);
  r = _ReadReg(pInst, REG_MAC_CSR_DATA);
  IP_OS_EnableInterrupt();               // Re-Enable Interrupts
  return r;
}

/*********************************************************************
*
*       _WriteMacReg()
*
*  Function description
*    Writes the data into the selected MAC register of the LAN9115
*/
static void _WriteMacReg(LAN9115_INST * pInst, U32 RegIndex, U32 Data) {
  volatile U32 Dummy;

  IP_OS_DisableInterrupt();                // Disable Interrupts
  _WriteReg(pInst, REG_MAC_CSR_DATA, Data);
  _WriteReg(pInst, REG_MAC_CSR_CMD, (MAC_CSR_BUSY_MASK | RegIndex));
  Dummy = _ReadReg(pInst, REG_BYTE_TEST);  // Force flush of previous write
  Dummy = Dummy;                           // Add a small delay to allow internal status update!
  _WaitUntilMacIsReady(pInst);
  IP_OS_EnableInterrupt();                 // Re-Enable Interrupts
}

/*********************************************************************
*
*       _PHY_WriteReg
*
*  Function description
*    Writes the data into the selected PHY register of the addressed PHY
*/
static void _PHY_WriteReg(IP_PHY_CONTEXT * pContext, unsigned RegIndex, unsigned v) {
  LAN9115_INST * pInst;
  unsigned       Addr;

  pInst = (LAN9115_INST *) pContext->pContext;
  Addr  = pContext->Addr & 0x1F;           // Only lower 5 bits are valid

  IP_OS_DisableInterrupt();                // Disable Interrupts
  //
  // Write data into MAC_CSR_DATA register
  //
  _WriteReg(pInst, REG_MAC_CSR_DATA, v);
  //
  // Write command into MAC_CSR_CMD register, address MII_DATA
  //
  _WriteReg(pInst, REG_MAC_CSR_CMD, (MAC_CSR_BUSY_MASK | MII_DATA));
  //
  // Wait until MAC is ready, then write MII ACC write command into MAC_DATA register
  //
  _WaitUntilMacIsReady(pInst);
  _WriteReg(pInst, REG_MAC_CSR_DATA, ((Addr << 11) | (RegIndex << 6) | MII_WRITE_MASK | MII_BUSY_MASK)); /* PHY addr = 1; MII register addr = 4 */
  //
  // Write command into MAC_CSR_CMD register, address MII_ACC
  //
  _WriteReg(pInst, REG_MAC_CSR_CMD, (MAC_CSR_BUSY_MASK | MII_ACC));
  //
  // Wait until MII is ready
  //
  _WaitUntilMiiIsReady(pInst);
  IP_OS_EnableInterrupt();               // Re-Enable Interrupts
}

/*********************************************************************
*
*       _PHY_ReadReg
*
*  Function description
*    Reads the data from the selected PHY register of the addressed PHY
*/
static unsigned _PHY_ReadReg(IP_PHY_CONTEXT * pContext, unsigned RegIndex) {
  LAN9115_INST * pInst;
  unsigned       Addr;
  unsigned       r;

  pInst = (LAN9115_INST *) pContext->pContext;
  Addr  = pContext->Addr & 0x1F;           // Only lower 5 bits are valid

  IP_OS_DisableInterrupt();                // Disable Interrupts
  //
  // Transfer address of phy register to MAC_DATA register
  //
  _WriteReg(pInst, REG_MAC_CSR_DATA, ((Addr << 11) | (RegIndex << 6) | MII_BUSY_MASK));
  //
  // Send command to transfer address into MII ACC register
  //
  _WriteReg(pInst, REG_MAC_CSR_CMD, (MAC_CSR_BUSY_MASK | MII_ACC));
  //
  // Wait until MII is ready
  //
  _WaitUntilMiiIsReady(pInst);
  //
  // Write command to address and read MII data register
  //
  _WriteReg(pInst, REG_MAC_CSR_CMD, (MAC_CSR_BUSY_MASK | MAC_CSR_READ_MASK | MII_DATA));
  //
  // Wait until data is available
  //
  _WaitUntilMacIsReady(pInst);
  //
  // Read out data
  //
  r = _ReadReg(pInst, REG_MAC_CSR_DATA);
  IP_OS_EnableInterrupt();                 // Re-Enable Interrupts
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
*       _PHY_Init
*/
static int _PHY_Init(LAN9115_INST * pInst) {
  return pInst->Phy_pDriver->pfInit(&pInst->PhyContext);                                // Configure the PHY
}

/*********************************************************************
*
*       _PHY_GetLinkState
*/
static void _PHY_GetLinkState(LAN9115_INST * pInst, U32 * pDuplex, U32 * pSpeed) {
  pInst->Phy_pDriver->pfGetLinkState(&pInst->PhyContext, pDuplex, pSpeed);
}

/*********************************************************************
*
*       _EnableTxInt()
*/
static void _EnableTxInt(LAN9115_INST * pInst) {
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
*        available, then the hash filter is used.
*        Alternativly, the MAC can be switched to promiscuous mode for simple implementations.
*/
static int _SetFilter(LAN9115_INST * pInst, IP_NI_CMD_SET_FILTER_DATA * pFilter) {
  U32        MacAddrL;
  U32        MacAddrH;
  U32        Data;
  unsigned   NumAddr;
  const U8 * pAddrData;

  NumAddr = pFilter->NumAddr;
  _LockInterrupt(pInst); // The following needs to be an integral operation, LAN9115 interrupt has to be disabled
  //
  // Set the MAC Address
  //
  if (NumAddr == 1) {
    pAddrData = *(&pFilter->pHWAddr);
    MacAddrL  = IP_LoadU32LE(pAddrData);      // lower (first) 32 bits
    MacAddrH  = IP_LoadU32LE(pAddrData+2);      // upper (last)  16 bits
    MacAddrH >>= 16;
    //
    // Update Mac registers
    _WriteMacReg(pInst, ADDRL, MacAddrL);
    _WriteMacReg(pInst, ADDRH, MacAddrH);
    Data = _ReadMacReg(pInst, MAC_CR);
    Data &= ~ ((1 << 31) | (1 << 18) | (1 << 13));  // Clear RXALL, promiscous mode, HASH filtering
// Filtering does not seem to work as described in the manual !
// We set promiscous mode here
    Data |=  (1 << 18);  // Set promiscous mode
    _WriteMacReg(pInst, MAC_CR, Data);
  } else {
    //
    // Set promiscous mode
    //
    Data  = _ReadMacReg(pInst, MAC_CR);
    Data &= ~((1 << 31) | (1 << 13));  // Clear RXALL, HASH filtering
    Data |= (1 << 18);                 // Enable promiscous mode
    _WriteMacReg(pInst, MAC_CR, Data);
  }
  _UnLockInterrupt(pInst);
  return 0;     // O.K.
}

/*********************************************************************
*
*       _UpdateEMACSettings
*
*  Function description
*     Updates the speed & duplex settings of the EMAC.
*     Needs to be called whenever speed and duplex settings change.
*/
static void _UpdateEMACSettings(U32 Duplex, U32 Speed) {
  // xxx We might set FDX bit according to Duplex

}

/*********************************************************************
*
*       _UpdateLinkState
*
*  Function description
*    Reads link state information from PHY and updates EMAC if necessary.
*    Should be called regularily to make sure that EMAC is notified if the link changes.
*/
static void _UpdateLinkState(LAN9115_INST * pInst) {
  U32  Speed;
  U32  Duplex;

  _LockInterrupt(pInst);                              // The following needs to be an integral operation, LAN9115 interrupt has to be disabled
  _PHY_GetLinkState(pInst, &Duplex, &Speed);
  if (IP_SetCurrentLinkState(Duplex, Speed)) {
    _UpdateEMACSettings (Duplex, Speed);              /* Inform the EMAC about the current PHY settings       */
  }
  _UnLockInterrupt(pInst);                            // Needs to be integral operation, LAN9115 interrupt has to be disabled
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
static int  _SendPacket(LAN9115_INST * pInst) {
  void *   pPacket;
  U32 *    pSrc;
  U32 *    pDest;
  unsigned NumBytes;
  unsigned NumWords;
  unsigned Data;
  unsigned char StartOffset;

  IP_GetNextOutPacket(&pPacket, &NumBytes);
  if (NumBytes == 0) {
    pInst->TxIsBusy = 0;
    return 0;
  }
  IP_LOG((IP_MTYPE_DRIVER, "DRIVER: Sending packet: %d bytes", NumBytes));
  INC(pInst->TxSendCnt);
  StartOffset = (U32) pPacket & 0x03;
  //
  // Transfer data
  //
  _LockInterrupt(pInst);  // The following needs to be an integral operation, LAN9115 interrupt has to be disabled
  //
  // Disable transmitter
  //
  Data  = _ReadReg(pInst, REG_TX_CFG);       // Disable transmission in general configuration register
  Data &= ~TX_CFG_TX_ON_MASK;
  _WriteReg(pInst, REG_TX_CFG, Data);
  //
  // Build command TX_CMD_A
  //
  Data = 0                                   // End-alignment = 0 => 4 byte aligned
       | (StartOffset << 16)                 // Set start offset
       | (1 << 13)                           // First segment
       | (1 << 12)                           // Last segment => Send entire packet in one action
       | (NumBytes & 0x7FF);                 // add size (last 11 bits)
  _WriteReg(pInst, REG_TX_DATA_FIFO, Data);  // Write TX_CMD_A to tx data FIFO
  //
  // Build command TX_CMD_B
  //
  pInst->TxTag++;
  Data = (pInst->TxTag << 16)                // Set a unique tag
       | (NumBytes & 0x7FF);                 // size
  _WriteReg(pInst, REG_TX_DATA_FIFO, Data);  // Write TX_CMD_B to tx data FIFO
  //
  // Write data into TX FIFO
  //
  NumWords = ((NumBytes + StartOffset) + 3) >> 2;               // rounded up on 32 bit boundary
  pSrc  = (U32*) ((U32) pPacket & 0xFFFFFFFC);   // Truncate last two bits of source address
  pDest = (U32*) ((U32) pInst->pBase + REG_TX_DATA_FIFO);
  while (NumWords) {
    Data = *pSrc++;
    *pDest = Data;
    NumWords--;
  }
  IP_RemoveOutPacket();
  _EnableTxInt(pInst);
  //
  // Finally enable transmitter und allow interrupts
  //
  Data  = _ReadReg(pInst, REG_TX_CFG);       // Enable transmission in general configuration register
  Data |= TX_CFG_TX_ON_MASK;
  _WriteReg(pInst, REG_TX_CFG, Data);
  _UnLockInterrupt(pInst);
  return 0;
}

/*********************************************************************
*
*       IP_NI_LAN9115_ISR_Handler
*
*  Function description
*    This is the interrupt service routine for the NI (EMAC).
*    It handles all interrupts (Rx, Tx, Error).
*/
void IP_NI_LAN9115_ISR_Handler(unsigned Unit) {
  U32 IntStat;
  LAN9115_INST * pInst;
  volatile U32 TxStatus;

  if (_InterruptLockCnt == 0) {  // Is interrupt handling allowed?
    pInst    = &_aInst[Unit];
    IntStat  = _ReadReg(pInst, REG_INT_STS);   // Get pending interrupts
//    IntStat &= _ReadReg(pInst, REG_INT_EN);    // Mask with enabled ints
    //
    // Handle receiver
    //
    if (IntStat & RX_INT_EN_MASK) {
      INC(pInst->RxIntCnt);
      IP_OnRx();
      _WriteReg(pInst, REG_INT_STS, RX_INT_EN_MASK); // Clear interrupt pending condition
    }
    //
    // Handle transmitter
    //
    if (IntStat & TX_INT_EN_MASK) {
      //
      // Reset Tx interrupt, read TX Status FIFO
      //
      TxStatus = _ReadReg(pInst, REG_TX_STATUS_FIFO);
      _WriteReg(pInst, REG_INT_STS, TX_INT_EN_MASK);
      if (pInst->TxIsBusy) {
        INC(pInst->TxIntCnt);
        _SendPacket(pInst);
      } else {
        IP_WARN((IP_MTYPE_DRIVER, "DRIVER: Tx complete interrupt, but no packet sent."));
      }
    }
  }
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
static  int  _Init(unsigned Unit) {
  U32 r;
  LAN9115_INST * pInst;

  pInst = &_aInst[Unit];
  //
  // Initialize PHY driver and PHY interface settings
  //
  pInst->Phy_pDriver = &IP_PHY_Generic;
  pInst->PhyContext.pAccess  = &_PHY_pAccess;        // PHY read/write functions are static in this module
  pInst->PhyContext.pContext = (void*) pInst;        // Pass owner of Phy to Phy-context

  if (pInst->PhyAddrIsValid == 0) {  // PHY address was not set, select internal PHY as default
    pInst->UseExternalPhy  = 0;
    pInst->PhyContext.Addr = 0x01;   // Internal Phy is located at address 0x01
    pInst->PhyAddrIsValid  = 1;
  }
  //
  // Initialize hardware, BSP specific
  //
  BSP_ETH_Init(Unit);
  //
  // Check ID, check whter device is accessible
  //
  r = _ReadReg(pInst, REG_ID_REV) >> 16;
  if ((r != ID_LAN9115) && (r != ID_LAN9215) && (r != ID_LAN9221)) {
    IP_WARN((IP_MTYPE_DRIVER, "DRIVER: MAC ID wrong. Expected %x or %x or %x, found %x", ID_LAN9115, ID_LAN9215, ID_LAN9221, r));
    return 1;
  }
  IP_LOG((IP_MTYPE_DRIVER | IP_MTYPE_INIT, "DRIVER: Compatible MAC VID %x verified", r));
  //
  // Reset the controller using software reset
  //
  r = _ReadReg(pInst, REG_HW_CFG);
  _WriteReg(pInst, REG_HW_CFG, (r | HW_CFG_SRST_MASK | HW_CFG_MBO_MASK));
  //
  // Wait until software reset is finished
  //
  do {
  } while((_ReadReg(pInst, REG_HW_CFG) & HW_CFG_SRST_MASK) != 0);
  //
  // Wait until EEPROM is read by controller
  //
  do {
  } while((_ReadReg(pInst, REG_HW_CFG) & E2P_CMD_EPC_BUSY_MASK) != 0);
  //
  // Setup FIFO allocation and flow control
  //
  _WriteReg(pInst, REG_AFC_CFG, 0x006e3700);  // AFC high level = 0x6e, AFC low level = 0x37
  //
  // Setup MAC, Disable receive own, default is half duplex, Setup Promiscuous Mode (Bit 18), Disable Receiver, disable transmitter, don't pass bad packets
  //
  _WriteMacReg(pInst, MAC_CR, ((1uL << 23) | (1uL << 18)));
  //
  // Setup transmitter, transmitter FIFO size, Allow status overflow, clear data FIFO, clear status FIFO
  //
  _WriteReg(pInst, REG_TX_CFG, (TX_CFG_TXSAO_MASK | TX_CFG_TXD_DUMP_MASK | TX_CFG_TXS_DUMP_MASK));
  r = _ReadReg(pInst, REG_HW_CFG);
  //
  // Setup TX FIFO size
  //
  // TX buffer size might be reduced, normally, we do not need 5KB Tx FIFO
  _WriteReg(pInst, REG_HW_CFG, (r | (TX_FIFO_SIZE << 16) | HW_CFG_MBO_MASK));   // Tx FIFO size, results in 4608 bytes for TX data FIFO
  //
  // Initialize interface to external Phy if external Phy is selected
  //
  if (pInst->UseExternalPhy != 0) {  // Initialize interface for external PHY
    //
    // Disable clock to PHY
    //
    r  = _ReadReg(pInst, REG_HW_CFG);
    r &= ~HW_CFG_PHY_CLK_SEL_MASK;
    r |= (2uL << HW_CFG_PHY_CLK_SEL_BIT);        // 2 => Clock disabled
    r |= HW_CFG_MBO_MASK;
    _WriteReg(pInst, REG_HW_CFG, r);
    //
    // Switch SMI (serial control interface) to external PHY
    //
    r |= HW_CFG_SMI_SEL_MASK;
    _WriteReg(pInst, REG_HW_CFG, r);
    //
    // Activate MII interface to external PHY
    //
    r |= HW_CFG_EXT_PHY_EN_MASK;
    _WriteReg(pInst, REG_HW_CFG, r);
    //
    // Activate clock for external PHY
    //
    r &= ~HW_CFG_PHY_CLK_SEL_MASK;
    r |= (1uL << HW_CFG_PHY_CLK_SEL_BIT);       // 1 => clock to external PHY
    _WriteReg(pInst, REG_HW_CFG, r);
  }
  //
  // Initialize the PHY
  //
  r = _PHY_Init(pInst);                         // Configure the PHY
  if (r) {
    return 1;
  }
  //
  // Enable Interrupt logic of CPU to accept LAN9115 interrupts
  // Not required as long as polling is selected

  //
  // Enable receiver
  //
  r = _ReadMacReg(pInst, MAC_CR);
  _WriteMacReg(pInst, MAC_CR, (r | MAC_CR_RXEN_MASK));
  //
  // Enable transmitter in MAC, transmitter in DMA is enabled during _SendPacket !
  //
  r  = _ReadMacReg(pInst, MAC_CR);       // Also enable transmission in MAC configuration
  _WriteMacReg(pInst, MAC_CR, (r | MAC_CR_TXEN_MASK));
  //
  // Mark init as done, restore interrupt state
  //
  pInst->IsEnabled = 1;
  _InterruptLockCnt = 0;   // Now interrupts may be handled

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
  if (_aInst[Unit].TxIsBusy == 0) {
    _aInst[Unit].TxIsBusy = 1;
    IP_OS_EnableInterrupt();
    _SendPacket(&_aInst[Unit]);
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
*    Number of received bytes
*    0 if no complete packet is available.
*/
static int _GetPacketSize(unsigned Unit) {
  U32 Stat;
  U32 NumBytes;
  LAN9115_INST * pInst;

  pInst    = &_aInst[Unit];
  NumBytes = 0;
  _LockInterrupt(pInst);              // The following needs to be an integral operation, LAN9115 interrupt has to be disabled
  Stat = _ReadReg(pInst, REG_RX_FIFO_INF);   // Check whether data is available in FIFO
  if (Stat & 0x00FF0000uL) {          // Examine current FIFO byte count
    Stat = _ReadReg(pInst, REG_RX_STATUS_FIFO);
    Stat >>= 16;
    Stat &= 0x3FFF;                   // mask upper two bits to extract size
    if (Stat < 4) {
      NumBytes = 0;
    }
    NumBytes = Stat - 4;  // Adjust to real packet size
  }
  _UnLockInterrupt(pInst);
  return NumBytes;
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
  LAN9115_INST * pInst;
  U16 * pBuffer;      // Destination is aligned to 16bit
  U32   Data;         // Data is read as 32 bit value
  int   NumItems;

  pInst = &_aInst[Unit];

  _LockInterrupt(pInst);         // The following needs to be an integral operation, LAN9115 interrupt has to be disabled

  if (pDest) {
    //
    // A packet should be read
    //
    pBuffer  = (U16*) pDest;
    NumItems = NumBytes >> 2;
    while (NumItems) {  // Store 32 bit words first */
      Data     = _ReadReg(pInst, REG_RX_DATA_FIFO);
      *pBuffer++ = (U16) (Data & 0xFFFF);  // For 16 bit aligned buffer, write in two steps
      *pBuffer++ = (U16) (Data >> 16);
      NumItems--;
    }
    //
    // Store remaining bytes
    //
    NumItems = NumBytes & 0x03;  /* Store remaining bytes */
    if (NumItems) {
      Data = _ReadReg(pInst, REG_RX_DATA_FIFO);
      if (NumItems > 1) {
        *pBuffer++ = (Data & 0xFFFF);
        Data >>= 16;
        NumItems -= 2;
      }
      if (NumItems != 0) {
        *(char*)pBuffer = (Data & 0xFF);
      }
    }
    //
    // Read one more word from FIFO (checksum) to flush the FIFO
    //
    Data = _ReadReg(pInst, REG_RX_DATA_FIFO);  /* Read one more word (checksum) to flush FIFO */
    INC(pInst->RxCnt);
    IP_LOG((IP_MTYPE_DRIVER, "Packet: %d Bytes --- Read", NumBytes));
  } else {
    //
    // Discard current packet
    //
    _WriteReg(pInst, REG_RX_DP_CTRL, (1uL << 31));
    do {
    } while ((_ReadReg(pInst, REG_RX_DP_CTRL) & (1uL << 31)) != 0);
    IP_LOG((IP_MTYPE_DRIVER, "Packet: %d Bytes --- Discarded", NumBytes));
  }
  _UnLockInterrupt(pInst);
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
  _UpdateLinkState(&_aInst[Unit]);
}

/*********************************************************************
*
*       _EnableUnit
*
*  Function description
*    Enables the MAC and selected PHY if not already enabled
*    The function may be called after the unit has been disabled by the application
*/
static int _EnableUnit(unsigned Unit) {
  if (_aInst[Unit].IsEnabled != 0) {
    return -1;
  }
  //
  // Unit is not enabled, perform a normal initialization
  //
  return _Init(Unit);
}

/*********************************************************************
*
*       _DisableUnit
*
*  Function description
*/
static int _DisableUnit(unsigned Unit) {
  if (_aInst[Unit].IsEnabled == 0) {
    //
    // Unit is not enabled, nothing to do.
    //
    return -1;
  }
  //
  // Disable and isolate connected PHY
  //

  //
  // Disable MAC unit
  //

  //
  // Finally mark unit as not enabled
  //
  _aInst[Unit].IsEnabled = 0;
  return 0;
}

/*********************************************************************
*
*       _Control
*
*  Function description
*    Various control functions
*/
static int _Control(unsigned Unit, int Cmd, void * p) {
  switch (Cmd) {
  case IP_NI_CMD_ENABLE:
    //
    // Enable the MAC unit and PHY
    //
    return _EnableUnit(Unit);
  case IP_NI_CMD_DISABLE:
    //
    // Disable the MAC unit and PHY
    //
    return _DisableUnit(Unit);
  case IP_NI_CMD_SET_FILTER:
    return _SetFilter(&_aInst[Unit], (IP_NI_CMD_SET_FILTER_DATA*)p);
  case IP_NI_CMD_SET_BPRESSURE:
    //
    // Back pressure mode may be activated here if required
    //
    break;
  case IP_NI_CMD_CLR_BPRESSURE:
    //
    // Back pressure mode may be de-activated here if required
    //
    break;
  case IP_NI_CMD_SET_PHY_ADDR:
    //
    // May be used to select external PHY, can not be modified when Unit is already enabled
    //
    if (_aInst[Unit].IsEnabled) {
      return -1;
    }
    if ((unsigned) p == (unsigned) IP_PHY_ADDR_INTERNAL) {
      _aInst[Unit].PhyContext.Addr = 0x01;       // Internal Phy has addres 0x01;
      _aInst[Unit].UseExternalPhy = 0;
    } else {
      _aInst[Unit].UseExternalPhy = 1;           // Use external Phy
      _aInst[Unit].PhyContext.Addr = (U8)(int)p;
    }
    _aInst[Unit].PhyAddrIsValid  = 1;
    return 0;
  case IP_NI_CMD_SET_PHY_MODE:
    //
    // May have no effect on internal PHY, can not be modified when Unit is already enabled
    //
    if (_aInst[Unit].IsEnabled) {
      return -1;
    }
    _aInst[Unit].PhyContext.UseRMII = (U8)(int)p;
    return 0;
  case IP_NI_CMD_POLL:
    IP_NI_LAN9115_ISR_Handler(Unit);
    return 0;
  case IP_NI_CMD_GET_MAC_ADDR:
    //
    // Not supported
    //
    return -1;
  default:
    //
    // Any unsupported command
    return -1;
  }
  return -1;
}

/*********************************************************************
*
*       IP_NI_LAN9115_ConfigBaseAddr
*
*  Function description
*    Sets the base address of the MAC unit
*/
void IP_NI_LAN9115_ConfigBaseAddr(unsigned Unit, void* pBase) {
  if (_aInst[Unit].IsEnabled == 0) {
    _aInst[Unit].pBase = pBase;
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
const IP_HW_DRIVER IP_Driver_LAN9115 = {
  _Init,
  _SendPacketIfTxIdle,
  _GetPacketSize,
  _ReadPacket,
  _Timer,
  _Control
};

/*************************** End of file ****************************/
