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
File    : IP_NI_DM9000.c
Purpose : Network interface driver for DM9000 / DM9000A
--------  END-OF-HEADER  ---------------------------------------------
Literature:
  [1]    DAVICOM DM9000 data sheet          \\fileserver\techinfo\Company\Davicom\MAC_Phy\DM9000-DS-F01-041202s.pdf
  [2]    DAVICOM DM9000 application notes   \\fileserver\techinfo\Company\Davicom\MAC_Phy\DM9000_Application_Notes_Ver_1_22 061104.pdf

Tested with the following eval boards:
  ATMEL   AT91SAM9261-EK
  Toshiba TOPASA910
  IAR     TMPA910-SK
*/

#include "IP_Int.h"
#include "BSP.h"
#include "IP_NI_DM9000.h"


/*********************************************************************
*
*       Defines, configurable
*
**********************************************************************
*/
#ifdef IP_DM9000_MAXUNIT
  #define MAX_UNITS   IP_DM9000_MAXUNIT
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
*       REG_
*
*  Registers of the MAC
*    NCR    Network Control Register 00H 00H
*    NSR    Network Status Register 01H 00H
*    TCR    TX Control Register 02H 00H
*    TSR I  TX Status Register I 03H 00H
*    TSR II TX Status Register II 04H 00H
*    RCR RX Control Register 05H 00H
*    RSR RX Status Register 06H 00H
*    ROCR   Receive Overflow Counter Register 07H 00H
*    BPTR   Back Pressure Threshold Register 08H 37H
*    FCTR   Flow Control Threshold Register 09H 38HFCR RX Flow Control Register 0AH 00H
*    EPCR   EEPROM & PHY Control Register 0BH 00H
*    EPAR   EEPROM & PHY Address Register 0CH 40H
*    EPDRL  EEPROM & PHY Low Byte Data Register 0DH XXH
*    EPDRH  EEPROM & PHY High Byte Data Register 0EH XXH
*    WCR    Wake Up Control Register 0FH 00H
*    PAR    Physical Address Register 10H-15H Determined by EEPROM
*    MAR    Multicast Address Register 16H-1DH XXH
*    GPCR   General Purpose Control Register 1EH 01H
*    GPR    General Purpose Register 1FH XXH
*    TRPAL  TX SRAM Read Pointer Address Low Byte 22H 00H
*    TRPAH  TX SRAM Read Pointer Address High Byte 23H 00H
*    RWPAL  RX SRAM Write Pointer Address Low Byte 24H 04H
*    RWPAH  RX SRAM Write Pointer Address High Byte 25H 0CH
*    VID    Vendor ID 28H-29H 0A46H
*    PID    Product ID 2AH-2BH 9000H
*    CHIPR  CHIP Revision 2CH 00H
*    SMCR   Special Mode Control Register 2FH 00H
*    MRCMDX Memory Data Read Command Without Address Increment Register F0H XXH
*    MRCMD  Memory Data Read Command With Address Increment Register F2H XXH
*    MRRL   Memory Data Read_ address Register Low Byte F4H 00H
*    MRRH   Memory Data Read_ address Register High Byte F5H 00H
*    MWCMDX Memory Data Write Command Without Address Increment Register F6H XXH
*    MWCMD  Memory Data Write Command With Address Increment Register F8H XXH
*    MWRL   Memory Data Write_ address Register Low Byte FAH 00H
*    MWRH   Memory Data Write _ address Register High Byte FBH 00H
*    TXPLL  TX Packet Length Low Byte Register FCH XXH
*    TXPLH  TX Packet Length High Byte Register FDH XXH
*    ISR    Interrupt Status Register FEH 00H
*    IMR    Interrupt Mask Register FFH 00H
*/
#define REG_NCR  0          // Network Control Register
#define REG_NSR  1          // Network Status Register
#define REG_TCR  2          // TX Control Register
#define REG_TSR1 3          // TX Status Register 1
#define REG_TSR2 4          // TX Status Register 2
#define REG_RCR  5          // RX Control Register
#define REG_RSR  6          // RX Status Register
#define REG_BPTR 8          // Back Pressure Threshold Register
#define REG_FCTR 9          // Flow Control Threshold Register
#define REG_FCR 10          // RX Flow Control Register
#define REG_EPCR 11         // EEPROM & PHY Control Register
#define REG_EPAR 12         // EEPROM & PHY Address Register
#define REG_EPDRL 13        // EEPROM & PHY Low Byte Data Register
#define REG_EPDRH 14        // EEPROM & PHY High Byte Data Register
#define REG_PAR   0x10      // Physical Address Register: 6 bytes
#define REG_MAR   0x16      // Multicast Address Register
#define REG_GPCR  0x1E      // General Purpose Control Register
#define REG_GPR   0x1F      // General Purpose Register
#define REG_VID   0x28
#define REG_PID   0x2A
#define REG_CHIPR 0x2C
#define REG_SMCR  0x2F      // Special Mode Control Register

#define REG_MWCMD 0xF8      // Memory Data Write Command With Address Increment Register
#define REG_TXPLL 0xFC      // TX Packet Length Low Byte Register
#define REG_TXPLH 0xFD      // TX Packet Length High Byte Register
#define REG_ISR   0xFE      // Interrupt Status Register
#define REG_IMR   0xFF      // Interrupt Mask Register

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
#define PHY_BMCR_CTST                        (1 <<  7)            // Collision test
#define PHY_BMCR_FULLDPLX                    (1 <<  8)            // Full duplex
#define PHY_BMCR_ANRESTART                   (1 <<  9)            // Auto negotiation restart
#define PHY_BMCR_ISOLATE                     (1 << 10)            // Disconnect PHY from MII
#define PHY_BMCR_PDOWN                       (1 << 11)            // Powerdown PHY
#define PHY_BMCR_ANENABLE                    (1 << 12)            // Enable auto negotiation
#define PHY_BMCR_SPEED100                    (1 << 13)            // Select 100Mbps
#define PHY_BMCR_LOOPBACK                    (1 << 14)            // TXD loopback bits
#define PHY_BMCR_RESET                       (1 << 15)            // Reset PHY

// Basic status register.
#define PHY_BSR_LSTATUS                      0x0004               // Link status
#define PHY_BSR_ANEGCOMPLETE                 0x0020               // Auto-negotiation complete

// Link partner ability register
#define PHY_LPA_100HALF                      0x0080               // Can do 100mbps half-duplex
#define PHY_LPA_100FULL                      0x0100               // Can do 100mbps full-duplex
#define PHY_LPA_100BASE4                     0x0200               // Can do 100mbps 4k packets

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

static U8          _TxIsBusy;
static int         _InterruptLockCnt = 1;
static U8          _ImrStat;

static DM9000_INST _aInst[MAX_UNITS];
static U8          _IsInited;

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
*       _LockInterrupt();
*/
static void _LockInterrupt(DM9000_INST * pInst) {
  IP_OS_DisableInterrupt();
  _InterruptLockCnt++;
  //
  // Disable Interrupts of DM9000
  //
  _ImrStat &= ~((1 << 1) | (1 << 0)); // Disable Packet Transmitted Latch interrupt and Packet Received Latch interrupt
  pInst->pAccess->pf_WriteReg8(pInst, REG_IMR, _ImrStat);
  IP_OS_EnableInterrupt();
}

/*********************************************************************
*
*       _UnLockInterrupt();
*/
static void _UnLockInterrupt(DM9000_INST * pInst) {
  IP_OS_DisableInterrupt();
  _InterruptLockCnt--;
  if (_InterruptLockCnt == 0) {
    //
    // Enable Interrupts of DM9000
    //
    _ImrStat |= ((1 << 1) | (1 << 0)); // Enable Packet Transmitted Latch interrupt and Packet Received Latch interrupt
    pInst->pAccess->pf_WriteReg8(pInst, REG_IMR, _ImrStat);
  }
  IP_OS_EnableInterrupt();
}

/*********************************************************************
*
*       _WriteReg8
*/
static void _WriteReg8(DM9000_INST * pInst, unsigned  RegIndex,  unsigned  val) {
  * pInst->pHardware = RegIndex;
  * pInst->pValue    = val;
}

/*********************************************************************
*
*       _ReadReg8
*/
static U16 _ReadReg8(DM9000_INST * pInst, unsigned RegIndex) {
  U32 r;

  * pInst->pHardware = RegIndex;
  r = * pInst->pValue;
  return (U16)r;
}

/*********************************************************************
*
*       _WriteReg16
*/
static void _WriteReg16(DM9000_INST * pInst, U8  RegIndex,  U16  val) {
  pInst->pAccess->pf_WriteReg8(pInst, RegIndex, (U8)val);
  pInst->pAccess->pf_WriteReg8(pInst, RegIndex + 1, (U8)(val >> 8));
}

/*********************************************************************
*
*       _ReadReg16
*/
static U16 _ReadReg16(DM9000_INST * pInst, U8 RegIndex) {
  U32 r;

  r = pInst->pAccess->pf_ReadReg8(pInst, RegIndex);
  r = r | (pInst->pAccess->pf_ReadReg8(pInst, RegIndex + 1) << 8);
  return (U16)r;
}

/*********************************************************************
*
*       _ReadData
*/
static void _ReadData(DM9000_INST * pInst, U8* pDest, U32 NumBytes) {
  U16 * p;
  U16 * pDest16;
  unsigned NumItems;

  p = (U16*) pInst->pValue;
  NumItems = (NumBytes + 1) >> 1;
  pDest16 = (U16*)pDest;
  do {
    *pDest16++ = *p;
  } while (--NumItems);
}

/*********************************************************************
*
*       _WriteData
*/
static void _WriteData(DM9000_INST * pInst, void* pPacket, U32 NumBytes) {
  unsigned i;

  if (pInst->BusWidth == 8) {
    U8 * p = (U8*) pInst->pValue;
    for (i = 0; i < NumBytes;) {
      U8 Data;
      Data = *(U8*)pPacket;
      *p = Data;
      pPacket = (U8*)pPacket + 1;
      i      += 1;
    }
  } else if (pInst->BusWidth == 16) {
    U16 * p = (U16*) pInst->pValue;
    for (i = 0; i < NumBytes;) {
      U16 Data;
      Data = *(U16*)pPacket;
      *p = Data;
      pPacket = (U16*)pPacket + 1;
      i       += 2;

    }
  } else if (pInst->BusWidth == 32) {
    U32 * p = (U32*) pInst->pValue;
    for (i = 0; i < NumBytes;) {
      U32 Data;
      Data = *(U32*)pPacket;
      *p = Data;
      pPacket = (U32*)pPacket + 1;
      i       += 4;
    }
  }
}

/*********************************************************************
*
*       _PHY_WriteReg
*/
static void _PHY_WriteReg(DM9000_INST * pInst, U8  RegIndex,  U16  val) {
  U16 r;
  _WriteReg16(pInst, REG_EPDRL, val);
  pInst->pAccess->pf_WriteReg8(pInst, REG_EPAR, RegIndex | 0x40);
  pInst->pAccess->pf_WriteReg8(pInst, REG_EPCR, (1 << 3)              // EPOS: 1: Select PHY, 0: Select EEPROM
                                              | (0 << 2)              // ERPRR: 1 to read
                                              | (1 << 1)              // ERPWR: 1 to write
            );
  while (1) {
    r = pInst->pAccess->pf_ReadReg8(pInst, REG_EPCR);
    if ((r & 1) == 0) {      // Busy ?
      break;
    }
  }
  pInst->pAccess->pf_WriteReg8(pInst, REG_EPCR, 0);                   // ERPRR must be cleared !
}

/*********************************************************************
*
*       _PHY_ReadReg
*/
static U16 _PHY_ReadReg(DM9000_INST * pInst, U8 RegIndex) {
  U32 r;

  pInst->pAccess->pf_WriteReg8(pInst, REG_EPAR, RegIndex | 0x40);
  pInst->pAccess->pf_WriteReg8(pInst, REG_EPCR, (1 << 3)              // EPOS: 1: Select PHY, 0: Select EEPROM
                                              | (1 << 2)              // ERPRR: 1 to read
                                              | (0 << 1)              // ERPWR: 1 to write
            );
  while (1) {
    r = pInst->pAccess->pf_ReadReg8(pInst, REG_EPCR);
    if ((r & 1) == 0) {      // Busy ?
      break;
    }
  }
  r = _ReadReg16(pInst, REG_EPDRL);
  pInst->pAccess->pf_WriteReg8(pInst, REG_EPCR, 0);                   // ERPRR must be cleared !
  return (U16)r;
}

/*********************************************************************
*
*       _PHY_Init
*/
static int _PHY_Init(DM9000_INST * pInst) {
  U16 v;

  //
  // Try to detect PHY on any permitted addr
  //
  v = _PHY_ReadReg(pInst, MII_PHYSID1);
  if (v != 0x181) {
    IP_WARN((IP_MTYPE_DRIVER | IP_MTYPE_INIT, "DRIVER: Could not find PHY"));
  }
  //
  // Reset PHY
  //
  v = _PHY_ReadReg(pInst, MII_BMCR);
  v |= (1 << 15);    // Reset
  _PHY_WriteReg(pInst, MII_BMCR, v);
  //
  // Wait until PHY is out of RESET
  //
  while (1) {
    v = _PHY_ReadReg(pInst, MII_BMCR);
    if ((v & (1 << 15)) == 0) {
      break;
    }
  }
  //
  // Clear Full duplex bits
  //
  v = _PHY_ReadReg(pInst, MII_ANAR);
  v &= ~((1 << 6) | (1 << 8));     // Clear FDX bits
  _PHY_WriteReg(pInst, MII_ANAR, v);
  if (IP_DEBUG) {
    U16 w;
    w = _PHY_ReadReg(pInst, MII_ANAR);
    if (v != w) {
      IP_WARN((IP_MTYPE_DRIVER | IP_MTYPE_INIT, "DRIVER: Can not write PHY Reg"));
      return 1;
    }
  }
  //
  // Connect MII-interface by clearing "ISOLATE" (bit 10 of BMCR)
  //
  v = _PHY_ReadReg(pInst, MII_BMCR);
  v |= PHY_BMCR_ANRESTART;        // Restart auto-negotiation
  v &= ~PHY_BMCR_ISOLATE;
  _PHY_WriteReg(pInst, MII_BMCR, v);

  return 0;
}

/*********************************************************************
*
*       _PHY_GetLinkState
*/
static void _PHY_GetLinkState(DM9000_INST * pInst, U32 * pDuplex, U32 * pSpeed) {
  U32 bmsr;
  U32 bmcr;
  U32 Speed;
  U32 Duplex;

  Speed  = 0;
  Duplex = IP_DUPLEX_UNKNOWN;
  //
  // Get Link Status from PHY status reg. Requires 2 reads
  //
  bmsr = _PHY_ReadReg(pInst, MII_BSR);
  if (bmsr & PHY_BSR_LSTATUS) {                                  // Link established ?
    bmcr = _PHY_ReadReg(pInst, MII_BMCR);
    if (bmcr & PHY_BMCR_SPEED100) {
      Speed = IP_SPEED_100MHZ;
    } else {
      Speed = IP_SPEED_10MHZ;
    }
    if (bmcr & PHY_BMCR_FULLDPLX) {
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
static int _SetFilter(DM9000_INST * pInst, IP_NI_CMD_SET_FILTER_DATA * pFilter) {
  U8 rcr;
  int i;
  U32 v;
  unsigned NumAddr;
  const U8 * pAddrData;

  NumAddr = pFilter->NumAddr;

  rcr =  (1 << 5)      // DIS_LONG. 1: Discard Long Packets ( over 1522bytes)
       | (1 << 4)      // DIS_CRC.  1: Discard CRC Error Packets
       | (1 << 0);     // RX Enable

  _LockInterrupt(pInst); // The following needs to be an integral operation, DM9000 interrupt has to be disabled
  if (NumAddr == 1) {
    pAddrData   = *(&pFilter->pHWAddr);
    for (i = 0; i < 6; i++) {
      v = *(pAddrData + i);
      pInst->pAccess->pf_WriteReg8(pInst, REG_PAR + i, v);
    }
  } else {
    rcr |= (1 << 1);      // Promiscuous Mode
  }

  pInst->pAccess->pf_WriteReg8(pInst, REG_MAR + 7, 0x80);   // Enable broadcast


  //rcr |= (1 << 1);      // Always use Promiscuous Mode. Filter does not seem to work.
  pInst->pAccess->pf_WriteReg8(pInst, REG_RCR, rcr);
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
*
*/
static void _UpdateEMACSettings(U32 Duplex, U32 Speed) {
}

/*********************************************************************
*
*       _UpdateLinkState
*
*  Function description
*    Reads link state information from PHY and updates EMAC if necessary.
*    Should be called regularily to make sure that EMAC is notified if the link changes.
*/
static void _UpdateLinkState(DM9000_INST * pInst) {
  U32  Speed;
  U32  Duplex;

  _LockInterrupt(pInst);                              // The following needs to be an integral operation, DM9000 interrupt has to be disabled
  _PHY_GetLinkState(pInst, &Duplex, &Speed);
  if (IP_SetCurrentLinkState(Duplex, Speed)) {
    _UpdateEMACSettings (Duplex, Speed);              /* Inform the EMAC about the current PHY settings       */
  }
  _UnLockInterrupt(pInst);                            // Needs to be integral operation, DM9000 interrupt has to be disabled
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
static int  _SendPacket(DM9000_INST * pInst) {
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
  // Transfer data
  //
  _LockInterrupt(pInst);  // The following needs to be an integral operation, DM9000 interrupt has to be disabled
  * pInst->pHardware = REG_MWCMD;
  pInst->pAccess->pf_WriteData(pInst, (U8*) pPacket, NumBytes);
  //
  // Start send
  //
  _WriteReg16(pInst, REG_TXPLL, (U16)NumBytes);
  pInst->pAccess->pf_WriteReg8(pInst, REG_TCR, 1);      // Start send
  IP_RemoveOutPacket();
  _UnLockInterrupt(pInst);
  return 0;
}

/*********************************************************************
*
*       IP_NI_DM9000_ISR_Handler
*
*  Function description
*    This is the interrupt service routine for the NI (EMAC).
*    It handles all interrupts (Rx, Tx, Error).
*
*/
void IP_NI_DM9000_ISR_Handler(unsigned Unit) {
  U32 Nsr;
  U32 IntStat;
  DM9000_INST * pInst;

  if (_InterruptLockCnt == 0) {  // Is interrupt handling allowed?
    pInst   = &_aInst[Unit];
    Nsr     = pInst->pAccess->pf_ReadReg8(pInst, REG_NSR);
    IntStat = pInst->pAccess->pf_ReadReg8(pInst, REG_ISR);
    pInst->pAccess->pf_WriteReg8(pInst, REG_ISR, IntStat);     // Clear all pending interrupts
    //
    // Handle receiver
    //
    if (IntStat & (1 << 0)) {
      INC(_RxIntCnt);
      IP_OnRx();
    }
    //
    // Handle transmitter
    //
    if (Nsr & ((1 << 2) | (1 << 3))) {                         // Tx finished?
      if (_TxIsBusy) {
        INC(_TxIntCnt);
        _SendPacket(pInst);
      } else {
        IP_WARN((IP_MTYPE_DRIVER, "DRIVER: Tx complete interrupt, but no packet sent."));
      }
    }
  }
}

/*********************************************************************
*
*       Default DM9000 access functions
*/
static IP_NI_DM9000_ACCESS _DM9000_Access = {
  _WriteReg8,
  _ReadReg8,
  _ReadData,
  _WriteData
};

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
  int i;
  DM9000_INST * pInst;

  pInst = &_aInst[Unit];
  //
  // Initialize default hardware access functions
  //
  if (pInst->pAccess == NULL) {
    pInst->pAccess = &_DM9000_Access;
  }

  BSP_ETH_Init(Unit);

  r = _ReadReg16(pInst, REG_VID);
  if (r != 0xa46) {
    IP_WARN((IP_MTYPE_DRIVER, "DRIVER: MAC VID wrong. Expected 0x0a46, found %x", r));
    return 1;
  }
  r = _ReadReg16(pInst, REG_PID);
  if (r != 0x9000) {
    IP_WARN((IP_MTYPE_DRIVER, "DRIVER: MAC PID wrong. Expected 0x9000, found %x", r));
    return 1;
  }
  IP_LOG((IP_MTYPE_DRIVER | IP_MTYPE_INIT, "DRIVER: DM9000 MAC VID verified"));

  //
  // [2]: 5.2 Driver Initializing Steps
  // 1. if the internal PHY is required open, the following steps are to activate it:
  //    i. set GPCR (REG_1E) GEP_CNTL0 bit[0]=1
  //    ii. set GPR (REG_1F) GEPIO0 bit[0]=0
  // The default status of the DM9000 is to power down the internal PHY by the value GEPIO0=1.
  // Since the internal PHY have been powered down, the wakeup procedure will be needed to
  // enable it. Please refer to the section 3.4 about the GPIO settings.
  //
  pInst->pAccess->pf_WriteReg8(pInst, REG_GPCR, 1);
  pInst->pAccess->pf_WriteReg8(pInst, REG_GPR, 0);
  //
  //
  // 2. do the software reset twice to initial DM9000:
  // i. set NCR (REG_00) bit[2:0]=011 for a period time, at least 20 us.
  // ii. clear NCR (REG_00) bit[2:0]=000
  // issue 2 nd reset
  // iii. set NCR (REG_00) bit[2:0]=011 for a period time, at least 20 us.
  // iv. clear NCR (REG_00) bit[2:0]=000
  //
  pInst->pAccess->pf_WriteReg8(pInst, REG_NCR, 3);
  IP_OS_Delay(2);
  pInst->pAccess->pf_WriteReg8(pInst, REG_NCR, 0);
  pInst->pAccess->pf_WriteReg8(pInst, REG_NCR, 3);
  IP_OS_Delay(2);
  pInst->pAccess->pf_WriteReg8(pInst, REG_NCR, 0);
  //
  // 3. program the NCR register. Default works for us.
  //

  //
  // Check bus width.
  //
  r = pInst->pAccess->pf_ReadReg8(pInst, REG_ISR);
  r >>= 6;
  if (r == 0) {
    IP_LOG((IP_MTYPE_DRIVER | IP_MTYPE_INIT, "DRIVER: DM9000: 16 bit interface detected"));
    pInst->BusWidth = 16;
  } else if (r == 1) {
    IP_LOG((IP_MTYPE_DRIVER | IP_MTYPE_INIT, "DRIVER: DM9000: 32 bit interface detected"));
    pInst->BusWidth = 32;
  } else if (r == 2) {
    IP_LOG((IP_MTYPE_DRIVER | IP_MTYPE_INIT, "DRIVER: DM9000: 8 bit interface detected"));
    pInst->BusWidth = 8;
  } else {
    IP_WARN((IP_MTYPE_DRIVER | IP_MTYPE_INIT, "DRIVER: DM9000: No bus interface detected"));
    return 1;
  }
  //
  // 4. clear TX status by reading the NSR register (REG_01). The bit [2] TX1END, bit [3] TX2END,
  // and bit [5] WAKEST will be automatically cleared by reading it or writing "1". Please refer to
  // the datasheet ch.6.2 about the NSR register setting.
  //
  pInst->pAccess->pf_ReadReg8(pInst,  REG_NSR);

  //
  // 5. Read the EEPROM saved data. Not done here.
  //
  //
  // 6. Write Node address 6 bytes into the physical address registers (REG_10 ~ REG_15).
  //    We do not do this here; done by _SetFilter.
  //
  //
  // 7. Write Hash table 8 bytes into the multicast address registers (REG_16 ~ REG_1D).
  //
  for (i = 0; i < 8; i++) {
    pInst->pAccess->pf_WriteReg8(pInst, REG_MAR + i, 0);
  }
  //
  // 8. set the IMR register (REG_FF) PAR bit[7]=1 to enable the Pointer Automatic Return
  //    function, which is the memory read/ write address pointer of the RX/ TX FIFO SRAM.
  //
  //
  // 9. depend on OS and DDK of the system to handle the NIC interrupt or polling service.
  // (Taken care of before init, so nothing done here)
  //
  // 10. Program the IMR register (REG_FF) PRM bit [0]/ PTM bit [1] to enable the RX/ TX interrupt.
  //     Before doing this, the system designer needs to register the interrupt handler routine.
  //     For example, if the driver needs to generate the interrupt after one package is transmitted, the
  //     interrupt mask register IMR PTM bit [1] =1 will be set. And, if the interrupt is generated after
  //     the DM9000 received one new packet incoming, IMR PRM bit [0] should be set to "1".
  _ImrStat = (1 << 7)      // PAR: Needs to be enabled
           | (1 << 1)      // Enable Packet Transmitted Latch interrupt
           | (1 << 0)      // Enable Packet Received Latch interrupt
           ;
  pInst->pAccess->pf_WriteReg8(pInst, REG_IMR, _ImrStat);
  //
  // 11. Program the RCR register to enable RX. The RX function is enabled by setting the RX
  //     control register (REG_05) RXEN bit [0] =1. The choice of the other bits bit [6:0] is depended
  //     on the system design. Please refer to the datasheet ch.6.6 about setting the RCR register.
  //
  // This is done in _SetFilter
  r = _PHY_Init(pInst);                                // Configure the PHY
  if (r) {
    return 1;
  }
  pInst->pAccess->pf_WriteReg8(pInst, REG_FCR, (1 << 3));     // b3: back pressure if DA matches
  //
  // Enable Interrupt logic of CPU to accept DM9000 interrupts
  //

  _InterruptLockCnt = 0;   // Now interrupts may be handled
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
*    Number of buffers used for the next packet.
*    0 if no complete packet is available.
*/
static int _GetPacketSize(unsigned Unit) {
  U16 Stat;
  U16 NumBytes;
  DM9000_INST * pInst;

  pInst = &_aInst[Unit];

  _LockInterrupt(pInst);                                // The following needs to be an integral operation, DM9000 interrupt has to be disabled
  Stat = pInst->pAccess->pf_ReadReg8(pInst, 0xF0);      // Dummy read required
  pInst->pAccess->pf_ReadData(pInst, (U8*) &Stat, 1);   // Read status byte. This should be 0 (no packet) or 1 (packet available)
  Stat &= 0xFF;                                         // Ensure only the status byte is examined, driver might return two bytes

  if (Stat == 0) {
    _UnLockInterrupt(pInst);
    return 0;
  }
  if (Stat != 1) {
    IP_NI_SetError(Unit);
    pInst->pAccess->pf_WriteReg8(pInst, REG_RCR, 0);    // Disable receiver
    _UnLockInterrupt(pInst);
    return 0;
  }
  //
  // Read status and number of bytes
  //
  * pInst->pHardware = 0xF2;     // read with auto increment
  if (pInst->BusWidth == 8) {
    //
    // Not implemented right now
    //
  } else if (pInst->BusWidth == 16) {
    pInst->pAccess->pf_ReadData(pInst, (U8*) &Stat, 1);
    Stat &= 0xFF;                   // Ensure only the byte is examined
    pInst->pAccess->pf_ReadData(pInst, (U8*) &NumBytes, 2);
  } else if (pInst->BusWidth == 32) {
    //
    // Not implemented right now
    //
  }
  _UnLockInterrupt(pInst);
  NumBytes -= 4;  // Subtract 4 bytes ethernet CRC checksum which is expected to be NOT included by the IP stack
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
  DM9000_INST * pInst;
  volatile U32 Dummy;

  pInst = &_aInst[Unit];

  _LockInterrupt(pInst);         // The following needs to be an integral operation, DM9000 interrupt has to be disabled
  * pInst->pHardware = 0xF2;     // read with auto increment
  if (pInst->BusWidth == 8) {
    //
    // Not implemented right now
    //
  } else if (pInst->BusWidth == 16) {
    if (pDest) {                          // Read a packet?
      pInst->pAccess->pf_ReadData(pInst, pDest, NumBytes);
    } else {                              // Flush buffer
      while (NumBytes--) {
        pInst->pAccess->pf_ReadData(pInst, (U8*) &Dummy, 1);
      }
    }
    pInst->pAccess->pf_ReadData(pInst, (U8*) &Dummy, 4);  // Read ethernet CRC checksum but do not add it to the packet
  } else if (pInst->BusWidth == 32) {
    //
    // Not implemented right now
    //
  }
  _UnLockInterrupt(pInst);
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
  _UpdateLinkState(&_aInst[Unit]);
}

/*********************************************************************
*
*       _Control
*
*  Function description
*    Various control functions
*/
static int _Control(unsigned Unit, int Cmd, void * p) {
  if (Cmd == IP_NI_CMD_SET_FILTER) {
    return _SetFilter(&_aInst[Unit], (IP_NI_CMD_SET_FILTER_DATA*)p);
/* TBD: Enable back pressure (if supported). Optional.
  } else if (Cmd == IP_NI_CMD_SET_BPRESSURE) {
    return 0;
  } else if (Cmd == IP_NI_CMD_CLR_BPRESSURE) {
    return 0;
*/
  /*  // Not required since we use the internal PHY
  } else if (Cmd == IP_NI_CMD_SET_PHY_ADDR) {
    _PhyAddr = (U8)(int)p;
    return 0;
  } else if (Cmd == IP_NI_CMD_SET_PHY_MODE) {
    _PhyMode = (U8)(int)p;
    return 0;
*/
  } else if (Cmd == IP_NI_CMD_POLL) {
    IP_NI_DM9000_ISR_Handler(0);
  } else if (Cmd == IP_NI_CMD_GET_MAC_ADDR) {
    int i;
    for (i = 0;i < 6; i++) {
      ((char*)p)[i] = _aInst[Unit].pAccess->pf_ReadReg8(&_aInst[Unit], REG_PAR + i);
    }
    return 0;
  }
  return -1;
}

/*********************************************************************
*
*       IP_NI_DM9000_ConfigAddr
*
*  Function description
*    Sets the base address (for command) and data address
*/
void IP_NI_DM9000_ConfigAddr(unsigned Unit, void* pBase, void* pValue) {
  if (_IsInited == 0) {
    _aInst[Unit].pHardware = (U8*)pBase;
    _aInst[Unit].pValue    = (U8*)pValue;
  } else {
    IP_WARN((IP_MTYPE_DRIVER, "DRIVER: Can not configure HW address after init."));
  }
}

/*********************************************************************
*
*       IP_NI_DM9000_ConfigAccess
*
*  Function description
*    Sets the function table address for DM9000 access.
*    This allows assigning special user functions if required
*/
void IP_NI_DM9000_ConfigAccess(unsigned Unit, IP_NI_DM9000_ACCESS * pAccess) {
  _aInst[Unit].pAccess = pAccess;
}

/*********************************************************************
*
*       Driver API Table
*
**********************************************************************
*/
const IP_HW_DRIVER IP_Driver_DM9000 = {
  _Init,
  _SendPacketIfTxIdle,
  _GetPacketSize,
  _ReadPacket,
  _Timer,
  _Control
};

/*************************** End of file ****************************/
