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
File    : IP_NI_LM3S9B90.c
Purpose : Network interface driver for the LM3S9B90
--------  END-OF-HEADER  ---------------------------------------------
*/

#include "IP_Int.h"
#include "BSP.h"
#include "IP_NI_LM3S9B90.h"

/*********************************************************************
*
*       Defines, configurable
*
**********************************************************************
*/

/*********************************************************************
*
*       Defines, non configurable
*
**********************************************************************
*/
#define MAC_BASE_ADDR          0x40048000
#define MACRIS                 (*(volatile U32 *)(MAC_BASE_ADDR + 0x000))  // Ethernet MAC Raw Interrupt Status (MACRIS)
#define MACIACK                (*(volatile U32 *)(MAC_BASE_ADDR + 0x000))  // Ethernet MAC Interrupt Acknowledge (MACIACK)
#define MACIM                  (*(volatile U32 *)(MAC_BASE_ADDR + 0x004))  // Ethernet MAC Interrupt Mask (MACIM)
#define MACRCTL                (*(volatile U32 *)(MAC_BASE_ADDR + 0x008))  // Ethernet MAC Receive Control (MACRCTL)
#define MACTCTL                (*(volatile U32 *)(MAC_BASE_ADDR + 0x00C))  // Ethernet MAC Data (MACDATA)
#define MACDATA                (*(volatile U32 *)(MAC_BASE_ADDR + 0x010))  // Ethernet MAC Individual Address 0 (MACIA0)
#define MACIA0                 (*(volatile U32 *)(MAC_BASE_ADDR + 0x014))  // Ethernet MAC Individual Address 1 (MACIA1)
#define MACIA1                 (*(volatile U32 *)(MAC_BASE_ADDR + 0x018))  // Ethernet MAC Threshold (MACTHR)
#define MACTHR                 (*(volatile U32 *)(MAC_BASE_ADDR + 0x01C))  // Ethernet MAC Management Control (MACMCTL)
#define MACMCTL                (*(volatile U32 *)(MAC_BASE_ADDR + 0x020))  // Ethernet MAC Management Divider (MACMDV)
#define MACMDV                 (*(volatile U32 *)(MAC_BASE_ADDR + 0x024))  // Ethernet MAC Management Address (MACMADD)
#define MACMADD                (*(volatile U32 *)(MAC_BASE_ADDR + 0x028))  // Ethernet MAC Management Transmit Data (MACMTXD)
#define MACMTXD                (*(volatile U32 *)(MAC_BASE_ADDR + 0x02C))  // Ethernet MAC Management Receive Data (MACMRXD)
#define MACMRXD                (*(volatile U32 *)(MAC_BASE_ADDR + 0x030))  // Ethernet MAC Number of Packets (MACNP)
#define MACNP                  (*(volatile U32 *)(MAC_BASE_ADDR + 0x034))  // Ethernet MAC Transmission Request (MACTR)
#define MACTR                  (*(volatile U32 *)(MAC_BASE_ADDR + 0x038))

#define MAC_RXINT              (1 << 0)
#define MAC_TXER               (1 << 1)
#define MAC_TXEMP              (1 << 2)
#define MAC_FOV                (1 << 3)
#define MAC_RXER               (1 << 4)
#define MAC_MDINT              (1 << 5)

#define SYSCTL_BASE_ADDR       0x400FE000
#define SYSCTL_RCGC2           (*(volatile U32*)(SYSCTL_BASE_ADDR + 0x108))  // Run Mode Clock Gating Control Register 2 (RCGC2)
#define SYSCTL_SRCR2           (*(volatile U32*)(SYSCTL_BASE_ADDR + 0x108))  // Software Reset Control 2 (SRCR2)


#define GPIO_PORTF_BASE_ADDR   0x40025000
#define GPIO_PORTF_DIR         (*(volatile U32*)(GPIO_PORTF_BASE_ADDR + 0x400))   // GPIO Direction (GPIODIR)
#define GPIO_PORTF_AFSEL       (*(volatile U32*)(GPIO_PORTF_BASE_ADDR + 0x420))   // GPIO Alternate Function Select (GPIOAFSEL)
#define GPIO_PORTF_DR2R        (*(volatile U32*)(GPIO_PORTF_BASE_ADDR + 0x500))   // GPIO 2-mA Drive Select (GPIODR2R)
#define GPIO_PORTF_DEN         (*(volatile U32*)(GPIO_PORTF_BASE_ADDR + 0x51C))   // GPIO Digital Enable (GPIODEN)


#define MAC_MCTL_START_BIT (1 << 0)
#define MAC_MCTL_WRITE_BIT (1 << 1)

/*********************************************************************
*
*       PHY
*/
// Generic MII registers.
#define MII_BMCR                            0x00       // Basic mode control register
#define MII_BSR                             0x01       // Basic mode status register
#define MII_PHYSID1                         0x02       // PHYS ID 1
#define MII_PHYSID2                         0x03       // PHYS ID 2
#define MII_ANAR                            0x04       // Auto-negotiation Advertisement register
#define MII_LPA                             0x05       // Link partner ability register

// PHY - Basic control register.
#define PHY_BMCR_CTST                       (1 <<  7)   // Collision test
#define PHY_BMCR_FULLDPLX                   (1 <<  8)   // Full duplex
#define PHY_BMCR_ANRESTART                  (1 <<  9)   // Auto negotiation restart
#define PHY_BMCR_ISOLATE                    (1 << 10)   // Disconnect PHY from MII
#define PHY_BMCR_PDOWN                      (1 << 11)   // Powerdown PHY
#define PHY_BMCR_ANENABLE                   (1 << 12)   // Enable auto negotiation
#define PHY_BMCR_SPEED100                   (1 << 13)   // Select 100Mbps
#define PHY_BMCR_LOOPBACK                   (1 << 14)   // TXD loopback bits
#define PHY_BMCR_RESET                      (1 << 15)   // Reset PHY

// Basic status register.
#define PHY_BSR_LSTATUS                     0x0004      // Link status
#define PHY_BSR_ANEGCOMPLETE                0x0020      // Auto-negotiation complete

// Link partner ability register
#define PHY_LPA_100HALF                     0x0080      // Can do 100mbps half-duplex
#define PHY_LPA_100FULL                     0x0100      // Can do 100mbps full-duplex
#define PHY_LPA_100BASE4                    0x0200      // Can do 100mbps 4k packets

#define PHY_LPA_100                         (PHY_LPA_100FULL | PHY_LPA_100HALF | PHY_LPA_100BASE4)

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
static char _TxIsBusy;
static U32  _FirstWord;

static U8   _PhyAddr = 0xff;
static U16  _PhyAnar;      // Value written to ANAR (Auto-negotiation Advertisement register)
static U16  _PhyBmcr;      // Value written to BMCR (basic mode control register)


/****** Statistics **************************************************/

#if DEBUG
  static struct {
    int TxSendCnt;
    int TxIntCnt;
    int RxCnt;
    int RxIntCnt;
    int RxOverflowCnt;
    int RxErrCnt;
    int RxNoPacket;
    int RxNoPacketSize;
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
*       _PHY_WriteReg
*/
static void _PHY_WriteReg(U8  RegIndex,  U16  val) {
  U32 v;

  if (RegIndex == MII_ANAR) {
    _PhyAnar = val;
  }
  if (RegIndex == MII_BMCR) {
    _PhyBmcr = val;
  }
  MACMTXD = val;
  v = ((RegIndex << 3) | (MAC_MCTL_WRITE_BIT | MAC_MCTL_START_BIT));
  MACMCTL = v;
  while(MACMCTL & (MAC_MCTL_START_BIT)){
  };
}

/*********************************************************************
*
*       _PHY_ReadReg
*/
static U16 _PHY_ReadReg(U8 RegIndex) {
  U32 v;

  v = (((RegIndex << 3) & 0x000000F8) | MAC_MCTL_START_BIT);
  MACMCTL = v;
  while(MACMCTL & (MAC_MCTL_START_BIT)){
  };
  return ((U16)MACMRXD);
}

/*********************************************************************
*
*       _PHY_Init
*/
static int _PHY_Init(unsigned Unit) {
  U16 v, w;
  unsigned i;
  unsigned FirstAddr;
  unsigned LastAddr;

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
  _PHY_WriteReg(MII_BMCR, PHY_BMCR_RESET);
  //
  // Wait until PHY is out of RESET
  //
  IP_OS_Delay(3);                 // Wait 3 usec */
  do {
    v = _PHY_ReadReg(MII_BMCR);
  } while (v & PHY_BMCR_RESET);
  //
  // Clear Full duplex bits
  //
  v = _PHY_ReadReg(MII_ANAR);
  v &= ~((1 << 6) | (1 << 8));     // Clear FDX bits
  _PHY_WriteReg(MII_ANAR, v);
  if (IP_DEBUG) {
    U16 w;
    w = _PHY_ReadReg(MII_ANAR);
    if (v != w) {
      IP_WARN((IP_MTYPE_DRIVER, "DRIVER: Can not write PHY Reg"));
      return 1;
    }
  }
  //
  // Connect MII-interface by clearing "ISOLATE" (bit 10 of BMCR)
  //
  v = _PHY_ReadReg(MII_BMCR);
  v |= PHY_BMCR_ANRESTART;        // Restart auto-negotiation
  v &= ~PHY_BMCR_ISOLATE;
  _PHY_WriteReg(MII_BMCR, v);
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
  if (bmsr & PHY_BSR_LSTATUS) {                                  // Link established ?
    if (_PhyBmcr & (1 << 12)) {   // Auto-negotiation enabled ?
      lpa  = _PHY_ReadReg(MII_LPA);
      if (lpa & 0x1F != 1) {      // Some PHY require reading LPA twice
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
static int _SetFilter(IP_NI_CMD_SET_FILTER_DATA * pFilter) {
  //
  // Implement if required.
  //
  return 0;     // O.K.
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
  U32    * pPacket;
  unsigned NumBytes;
  U32      FirstWord;
  int      NumItems;

  IP_GetNextOutPacket((void*)&pPacket, &NumBytes);
  if (NumBytes == 0) {
    _TxIsBusy = 0;
    return 0;
  }
  IP_LOG((IP_MTYPE_DRIVER, "DRIVER: Sending packet: %d bytes", NumBytes));
  INC(_DriverStats.TxSendCnt);
  //
  // The first word of the Tx Fifo has the following structure:
  //  b[ 7:0 ] Data Length LSB
  //  b[15:8 ] Data Length MSB
  //  b[23:16] DA oct 1
  //  b[31:24] DA oct 2
  // The Data Length field in the first FIFO word refers to the
  // Ethernet frame data payload. NumBytes is the size of the payload + the Ethernet
  // header, so we have to substract 14 bytes.
  //
  FirstWord = ((*pPacket & 0x0000FFFF) << 16) | (NumBytes - 14);
  pPacket = (U32*)((U8*)pPacket + 2);
  //
  // Write data in the Tx FIFO
  //
  MACDATA = FirstWord;    // Write packet size into Tx FIFO
#if 1
  NumItems = (NumBytes + 3) >> 2;
  while (NumItems >= 4) {
    MACDATA = *pPacket++;
    MACDATA = *pPacket++;
    MACDATA = *pPacket++;
    MACDATA = *pPacket++;
    NumItems -= 4;
  } ;
  if (NumItems > 0) {
    do {
      MACDATA = *pPacket++;
    } while (--NumItems);
  }
#else
  while (Cnt < NumBytes) {
    MACDATA = *pPacket++;
    Cnt += 4;
  }
#endif
  MACTR |= (1 << 0);      // Initiate a transmission once the packet has been placed in the TX FIFO.
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
//  int v;

  INC(_DriverStats.RxOverflowCnt);
  MACRCTL &= ~(1 << 0);   // Disable Rx
  MACRCTL |=  (1 << 4);   // Reset Rx FIFO
//  v = MACRCTL;
//  while (v & (1 << 4)) {
//    v++;
//  }
  MACRCTL |= (1 << 0);   // Enable Rx
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
static void _ISR_Handler(void) {
  U32  ISR_Status;

  ISR_Status = MACRIS;
  //
  // Handle Rx errors
  //
  if (ISR_Status & MAC_RXER) {
    INC(_DriverStats.RxErrCnt);
  } else if (ISR_Status & MAC_FOV) {
    _ResetRxError();
  } else if (ISR_Status & MAC_RXINT) {  // Did we receive a frame ?
    INC(_DriverStats.RxIntCnt);
    MACIM  &= ~(1 << 0);  // Rx disable interrupt
    IP_OnRx();
  }

  if (ISR_Status & MAC_TXEMP) {         // Frame completly sent ?
    if (_TxIsBusy) {
      IP_RemoveOutPacket();
      INC(_DriverStats.TxIntCnt);
      _SendPacket();
    }
  }
  MACIACK    = ISR_Status; // Clears the bits in the ISR register
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
*       _Init
*
*  Function description
*    General init function of the driver.
*    Called by the stack in the init phase before any other driver function.
*/
static  int  _Init(unsigned Unit) {
  int r;
  U32 MacAddrLow;
  U16 MacAddrHigh;
  //
  // Enable clocks for MAC and PHY
  //
  SYSCTL_RCGC2 |= 0
               | (1 << 30)    // EPHY0
               | (1 << 28)    // EMAC0
               ;
  //
  // Reset MAC and PHY
  //
  SYSCTL_SRCR2 |= 0
               | (1 << 30)     // EPHY0
               | (1 << 28)     // EMAC0
               ;
  BSP_ETH_Init(Unit);
  MACMDV  = 0x0A;   // Set MDIO clock divider
  MACIM   = 0;      // Disable MAC interrupts
  MACIACK = 0x7FF;  // Clear all pending interrupts
  //
  // Setup MAC Address
  //
  MacAddrLow  = (IP_aIFace[0].abHWAddr[3]  << 24)| (IP_aIFace[0].abHWAddr[2] << 16) | (IP_aIFace[0].abHWAddr[1] << 8) | IP_aIFace[0].abHWAddr[0];
  MacAddrHigh = (IP_aIFace[0].abHWAddr[5] << 8) | IP_aIFace[0].abHWAddr[4];
  MACIA0      = MacAddrLow;
  MACIA1      = MacAddrHigh;
  r = _PHY_Init(Unit);                                // Configure the PHY
  if (r) {
    return 1;  // Error
  }
  _UpdateLinkState();
  BSP_ETH_InstallISR_Ex(58, _ISR_Handler, 240);
  MACRCTL = 0;          // Disable the receiver
  MACRCTL = (1 << 4);   // Reset Rx FIFO
  //
  // Enable receiver and transmitter
  //
  MACTCTL |= 0
          | (1 << 2)   // Generate CRC
          | (1 << 1)   // Enable padding
          | (1 << 0);  // Enable Tx
  MACRCTL = 0
          | (1 << 4)   // Reset Rx FIFO
          | (1 << 3)   // Reject frames with an incorrectly calculated CRC
          | (0 << 2)   // Enable promiscious mode
          | (0 << 1)   // Enable receiption of multicast frames
          | (1 << 0)   // Enable Rx
          ;
  //
  // Enable MAC interrupts
  //
  MACIM  =  0
         | (0 << 6)    // PHY interrupt
         | (0 << 5)    // MDIO interrupt
         | (0 << 4)    // Rx error
         | (1 << 3)    // Rx overflow
         | (1 << 2)    // Tx empty interrupt
         | (0 << 1)    // Tx error interrupt
         | (1 << 0)    // Rx interrupt
         ;
  return 0;             // O.K.
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
  if (_TxIsBusy == 0) {
    _TxIsBusy = 1;
    _SendPacket();
    return 0;
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
*    Number of bytes used for the next packet.
*    0 if no complete packet is available.
*/
static int _GetPacketSize(unsigned Unit) {
  int NumBytes;
  U32 NumPackets;

  NumPackets = MACNP;
  if (NumPackets != 0) {
    INC(_DriverStats.RxCnt);
    _FirstWord = MACDATA;  // Store first word since we need the 2 data bytes.
    NumBytes   = _FirstWord & 0x0000FFFF;
    if ((NumBytes >= 66) && (NumBytes < 0x600)) { // 66 bytes is the minimum packet size. 64 Ethernet packet including the FCS + 2 frame length size.
      return NumBytes - 6;  // Remove the 2 bytes frame length and the 4 bytes FCS.
    } else {
      INC(_DriverStats.RxNoPacketSize);
      _ResetRxError();
    }
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
  U32 * pPacket;
  U32   Data;
  int   NumItems;

  pPacket  = (U32*)pDest;
  *pPacket = (_FirstWord >> 16);
  pPacket  = (U32*)((U32)pPacket + 2);
  if (pDest != NULL) {
    NumBytes -= 2;   // First 2 data bytes of the packet has been read before.
    NumItems  = (NumBytes + 4 + 3) >> 2;  // NumBytes is the number of bytes which we copy into the buffer. The 4 bytes are the CRC which we do not copy but have to read. 3 bytes for the alignement.
    NumItems--;     // Don't store the CRC of the Ethernet frame in the buffer. The last item will only be read.
    while (NumItems >= 4) {
      Data     = MACDATA;
      *pPacket++  = Data;
      Data     = MACDATA;
      *pPacket++  = Data;
      Data     = MACDATA;
      *pPacket++  = Data;
      Data     = MACDATA;
      *pPacket++  = Data;
      NumItems -= 4;
    }
    if (NumItems) {
      do {
        Data     = MACDATA;
        *pPacket++  = Data;
      } while (--NumItems);
    }
    Data = MACDATA; // Remove the CRC of the Ethernet frame from the Rx FIFO.
  } else {
    if (NumItems) {
      do {
        Data     = MACDATA;
      } while (--NumItems);
    }
  }
  MACIM  |= (1 << 0);  // Enable Rx Interrupt
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
*    Various control functions
*/
static int _Control(unsigned Unit, int Cmd, void * p) {
  if (Cmd == IP_NI_CMD_SET_BPRESSURE) {
    return -1;
  } else if (Cmd == IP_NI_CMD_CLR_BPRESSURE) {
    return -1;
  } else if (Cmd == IP_NI_CMD_SET_FILTER) {
    return _SetFilter((IP_NI_CMD_SET_FILTER_DATA*)p);
  } else if (Cmd == IP_NI_CMD_SET_PHY_ADDR) {
    _PhyAddr = (U8)(int)p;
    return 0;
  } else if (Cmd == IP_NI_CMD_SET_PHY_MODE) {
    return -1;
  }
  return -1;
}

/*********************************************************************
*
*       Driver API Table
*
**********************************************************************
*/
const IP_HW_DRIVER IP_Driver_LM3S9B90 = {
  _Init,
  _SendPacketIfTxIdle,
  _GetPacketSize,
  _ReadPacket,
  _Timer,
  _Control
};

/*************************** End of file ****************************/
