/*********************************************************************
*  
*   IAR PowerPac
*
*   (c) Copyright IAR Systems 2010.  All rights reserved.
*
**********************************************************************
----------------------------------------------------------------------
File    : IP_NI_iMX25.c
Purpose : Network interface driver for Freescale ADS i.MX25 eval board
--------  END-OF-HEADER  ---------------------------------------------
*/

#include  "IP_Int.h"

/*********************************************************************
*
*       Extern data
*
**********************************************************************
*/
extern void BSP_ETH_Init(unsigned Unit);
extern void BSP_ETH_InstallISR(void (*pfISR)(void));

/*********************************************************************
*
*       Defines, configurable
*
**********************************************************************
*/

#define NUM_RX_BUFFERS    4
#define NUM_TX_BUFFERS    4
#define RX_BUFFER_SIZE    1520  // Buffer size MUST be divisible by 16 and less than 2047 bytes, but big enough to hold entire frame >= 1522/1518
#define TX_BUFFER_SIZE    1520  // Buffer size MUST be divisible by 16 and less than 2047 bytes

/*********************************************************************
*
*       Defines, non configurable
*
**********************************************************************
*/

/*********************************************************************
*
*       iMX25
*/

/****** Power, reset clock control unit register ********************/
#define _CRM_BASE_ADDR 0x53F80000UL
#define _CRM_CGCR0       (*(volatile U32 *)(_CRM_BASE_ADDR+0x00C))  //Peripheral Clock Control Register 0
#define _CRM_CGCR2       (*(volatile U32 *)(_CRM_BASE_ADDR+0x014))  //Peripheral Clock Control Register 1

#define _CRM_PCCR0_HCLK_FEC (1UL << 7)
#define _CRM_PCCR2_FEC_EN   (1UL << 15)

/****** FEC *********************************************************/
#define _FEC_BASE_ADDR   (0x50038000UL)
#define _FEC_EIR         (*(volatile U32*)(_FEC_BASE_ADDR + 0x004)) // Interrupt Event Register
#define _FEC_EIMR        (*(volatile U32*)(_FEC_BASE_ADDR + 0x008)) // Interrupt Mask Register
#define _FEC_RDAR        (*(volatile U32*)(_FEC_BASE_ADDR + 0x010)) // Receive Descriptor Active Register
#define _FEC_TDAR        (*(volatile U32*)(_FEC_BASE_ADDR + 0x014)) // Transmit Descriptor Active Register
#define _FEC_ECR         (*(volatile U32*)(_FEC_BASE_ADDR + 0x024)) // Ethernet Control Register
#define _FEC_MMFR        (*(volatile U32*)(_FEC_BASE_ADDR + 0x040)) // MII Management Frame Register
#define _FEC_MSCR        (*(volatile U32*)(_FEC_BASE_ADDR + 0x044)) // MII Speed Control Register
#define _FEC_MIBC        (*(volatile U32*)(_FEC_BASE_ADDR + 0x064)) // MIB Control/Status Register
#define _FEC_RCR         (*(volatile U32*)(_FEC_BASE_ADDR + 0x084)) // Receive Control Register
#define _FEC_TCR         (*(volatile U32*)(_FEC_BASE_ADDR + 0x0c4)) // Transmit Control Register
#define _FEC_PALR        (*(volatile U32*)(_FEC_BASE_ADDR + 0x0e4)) // Physical Address Low Register
#define _FEC_PAUR        (*(volatile U32*)(_FEC_BASE_ADDR + 0x0e8)) // Physical Address High Register
#define _FEC_OPD         (*(volatile U32*)(_FEC_BASE_ADDR + 0x0ec)) // Opcode/Pause Duration
#define _FEC_IAUR        (*(volatile U32*)(_FEC_BASE_ADDR + 0x118)) // Descriptor Individual Upper Address Register
#define _FEC_IALR        (*(volatile U32*)(_FEC_BASE_ADDR + 0x11c)) // Descriptor Individual Lower Address Register
#define _FEC_GAUR        (*(volatile U32*)(_FEC_BASE_ADDR + 0x120)) // Descriptor Group Upper Address Register
#define _FEC_GALR        (*(volatile U32*)(_FEC_BASE_ADDR + 0x124)) // Descriptor Group Lower Address Register
#define _FEC_TFWR        (*(volatile U32*)(_FEC_BASE_ADDR + 0x144)) // Transmit FIFO Watermark
#define _FEC_FRBR        (*(volatile U32*)(_FEC_BASE_ADDR + 0x14c)) // FIFO Receive Bound Register
#define _FEC_FRSR        (*(volatile U32*)(_FEC_BASE_ADDR + 0x150)) // FIFO Receive FIFO Start Register
#define _FEC_ERDSR       (*(volatile U32*)(_FEC_BASE_ADDR + 0x180)) // Pointer to Receive Descriptor Ring
#define _FEC_ETDSR       (*(volatile U32*)(_FEC_BASE_ADDR + 0x184)) // Pointer to Transmit Descriptor Ring
#define _FEC_EMRBR       (*(volatile U32*)(_FEC_BASE_ADDR + 0x188)) // Maximum Receive Buffer Size
#define _FEC_MIIGSK_CFGR (*(volatile U32*)(_FEC_BASE_ADDR + 0x300)) // MIIGSK Enable Register
#define _FEC_MIIGSK_ENR  (*(volatile U32*)(_FEC_BASE_ADDR + 0x308)) // MIIGSK Enable Register

#define _FEC_EIR_EIMR_UN            (1UL<<19)
#define _FEC_EIR_EIMR_RL            (1UL<<20)
#define _FEC_EIR_EIMR_LC            (1UL<<21)
#define _FEC_EIR_EIMR_EBERR         (1UL<<22)
#define _FEC_EIR_EIMR_MII           (1UL<<23)
#define _FEC_EIR_EIMR_RXB           (1UL<<24)
#define _FEC_EIR_EIMR_RXF           (1UL<<25)
#define _FEC_EIR_EIMR_TXB           (1UL<<26)
#define _FEC_EIR_EIMR_TXF           (1UL<<27)
#define _FEC_EIR_EIMR_GRA           (1UL<<28)
#define _FEC_EIR_EIMR_BABT          (1UL<<29)
#define _FEC_EIR_EIMR_BABR          (1UL<<30)
#define _FEC_EIR_EIMR_HBERR         (1UL<<31)

#define _FEC_ECR_RESET              (1UL<<0)
#define _FEC_ECR_ETHER_EN           (1UL<<1)

#define _FEC_MMFR_PHY_ARRD(addr)    ((((U32)addr)& 0x1FUL  )<<23)
#define _FEC_MMFR_PHY_REG(reg)      ((((U32)reg) & 0x1FUL  )<<18)
#define _FEC_MMFR_PHY_DATA(data)     (((U32)data)& 0xFFFFUL)
#define _FEC_MMFR_PHY_RD            0x60020000
#define _FEC_MMFR_PHY_WR            0x50020000

#define _FEC_RCR_LOOP               (1UL<<0)
#define _FEC_RCR_DRT                (1UL<<1)
#define _FEC_RCR_MII_MODE           (1UL<<2)
#define _FEC_RCR_PROM               (1UL<<3)
#define _FEC_RCR_BC_REJ             (1UL<<4)
#define _FEC_RCR_BC_FCE             (1UL<<5)
#define _FEC_RCR_BC_MAX_FL(len)     ((((U32)len) & 0x7FF)<<16)

#define _FEC_TCR_GTS                (1UL<<0)
#define _FEC_TCR_HBC                (1UL<<1)
#define _FEC_TCR_FDEN               (1UL<<2)
#define _FEC_TCR_TFC_PAUSE          (1UL<<3)
#define _FEC_TCR_RFC_PAUSE          (1UL<<4)

#define _FEC_MSCR_MII_SPEED(speed)  ((((U32)speed) & 0x3F)<<1)
#define _FEC_MSCR_DIS_PRE           (1UL<<7)

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
#define MII_PHYSTS                0x1f       // PHY Special Control/Status register

#define PHY_ID1_VALUE                       0x0180
#define PHY_ID1_MASK                        (0xFFFE)    // Ignore LSB of value read from PHY ID1 register
// PHY - Basic control register.
#define PHY_BMCR_CTST                        (1 <<  7)   // Collision test
#define PHY_BMCR_FULLDPLX                    (1 <<  8)   // Full duplex
#define PHY_BMCR_ANRESTART                   (1 <<  9)   // Auto negotiation restart
#define PHY_BMCR_ISOLATE                     (1 << 10)   // Disconnect PHY from MII
#define PHY_BMCR_PDOWN                       (1 << 11)   // Powerdown PHY
#define PHY_BMCR_ANENABLE                    (1 << 12)   // Enable auto negotiation
#define PHY_BMCR_SPEED100                    (1 << 13)   // Select 100Mbps
#define PHY_BMCR_LOOPBACK                    (1 << 14)   // TXD loopback bits
#define PHY_BMCR_RESET                       (1 << 15)   // Reset PHY

// Basic status register.
#define PHY_BSR_LSTATUS                     0x0004      // Link status
#define PHY_BSR_ANEGCOMPLETE                0x0020      // Auto-negotiation complete

// Link partner ability register
#define PHY_LPA_100HALF                     0x0080      // Can do 100mbps half-duplex
#define PHY_LPA_100FULL                     0x0100      // Can do 100mbps full-duplex
#define PHY_LPA_100BASE4                    0x0200      // Can do 100mbps 4k packets

#define PHY_LPA_100     (PHY_LPA_100FULL | PHY_LPA_100HALF | PHY_LPA_100BASE4)

#define PHY_PHYSTS_FD   (4UL<<2)
#define PHY_PHYSTS_100  (2UL<<2)
#define PHY_PHYSTS_10   (1UL<<2)

#define PHY_LAN8700_ID1 0x0007

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
typedef struct __TxBD_t
{
  union
  {
    U32 Ctrl;
    struct
    {
      U32 DataLength  : 16;
      U32             :  9;
      U32 ABC         :  1;
      U32 TC          :  1;
      U32 L           :  1;
      U32 TO2         :  1;
      U32 W           :  1;
      U32 TO1         :  1;
      U32 R           :  1;
    };
  };
  U32 Addr;
} _TxBD_t;

typedef struct __RxBD_t
{
  union
  {
    U32 Ctrl;
    struct {
      U32 DataLength  : 16;
      U32 TR          :  1;
      U32 OV          :  1;
      U32 CR          :  1;
      U32             :  1;
      U32 NO          :  1;
      U32 LG          :  1;
      U32 MC          :  1;
      U32 BC          :  1;
      U32 M           :  1;
      U32             :  2;
      U32 L           :  1;
      U32 RO2         :  1;
      U32 W           :  1;
      U32 RO1         :  1;
      U32 E           :  1;
    };
  };
  U32 Addr;
} _RxBD_t;

#pragma pack()

#define RX_BD_TR    (1UL<<16)
#define RX_BD_OV    (1UL<<17)
#define RX_BD_CR    (1UL<<18)
#define RX_BD_NO    (1UL<<20)
#define RX_BD_LG    (1UL<<21)
#define RX_BD_MC    (1UL<<22)
#define RX_BD_BC    (1UL<<23)
#define RX_BD_M     (1UL<<24)
#define RX_BD_L     (1UL<<27)
#define RX_BD_RO2   (1UL<<28)
#define RX_BD_W     (1UL<<29)
#define RX_BD_RO1   (1UL<<30)
#define RX_BD_E     (1UL<<31)

#define TX_BD_ABC   (1UL<<25)
#define TX_BD_TC    (1UL<<26)
#define TX_BD_L     (1UL<<27)
#define TX_BD_TO2   (1UL<<28)
#define TX_BD_W     (1UL<<29)
#define TX_BD_TO1   (1UL<<30)
#define TX_BD_R     (1UL<<31)

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

static char       _TxIsBusy;
#pragma section="DATA_NO_CACHE"
#pragma location="DATA_NO_CACHE"
#pragma data_alignment=128
static _RxBD_t    _RxBD[NUM_RX_BUFFERS];
#pragma location="DATA_NO_CACHE"
#pragma data_alignment=128
static _TxBD_t    _TxBD[NUM_TX_BUFFERS];
#pragma location="DATA_NO_CACHE"
#pragma data_alignment=4
static U8 FEC_Buffer[RX_BUFFER_SIZE*NUM_RX_BUFFERS + TX_BUFFER_SIZE*NUM_TX_BUFFERS];

static U8 _PhyAddr = 0xFF;
static _RxBD_t * _pCurRX;
static _TxBD_t * _pCurTX;

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
static void _PHY_WriteReg(U8  RegIndex,  U16  val) {
  _FEC_EIR = _FEC_EIR_EIMR_MII; // clear MII interrupt flag
  _FEC_MMFR = _FEC_MMFR_PHY_WR
            | _FEC_MMFR_PHY_DATA(val)
            | _FEC_MMFR_PHY_ARRD(_PhyAddr)
            | _FEC_MMFR_PHY_REG(RegIndex);
  while(0 == (_FEC_EIR & _FEC_EIR_EIMR_MII));
}


/*********************************************************************
*
*       _PHY_ReadReg
*/
static U16 _PHY_ReadReg(U8 RegIndex) {
  U32 r;
  _FEC_EIR = _FEC_EIR_EIMR_MII; // clear MII interrupt flag
  _FEC_MMFR = _FEC_MMFR_PHY_RD
            | _FEC_MMFR_PHY_ARRD(_PhyAddr)
            | _FEC_MMFR_PHY_REG(RegIndex);
  while(0 == (_FEC_EIR & _FEC_EIR_EIMR_MII));
  r = _FEC_MMFR & 0xFFFF;  // TBD
  return (U16)r;
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
  // Check PHY Id
  //
  v = _PHY_ReadReg(MII_PHYSID1);
  if (v != PHY_LAN8700_ID1) {
    IP_WARN((IP_MTYPE_DRIVER, "DRIVER: MAC PHY wrong. Expected 0x0007, found %x", v));
    return 1;
  }
  //
  // Reset PHY
  //
  v = _PHY_ReadReg(MII_BMCR);
  v |= PHY_BMCR_RESET;    // Reset
  _PHY_WriteReg(MII_BMCR, v);
  //
  // Wait until PHY is out of RESET
  //
  while (1) {
    v = _PHY_ReadReg(MII_BMCR);
    if ((v & PHY_BMCR_RESET) == 0) {
      break;
    }
  }

#if 0
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
#endif

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
  U32 physta;
  U32 Speed;
  U32 Duplex;

  Speed  = IP_SPEED_UNKNOWN;
  Duplex = IP_DUPLEX_UNKNOWN;
  //
  // Get Link Status from PHY status reg. Requires 2 reads
  //
  bmsr = _PHY_ReadReg(MII_BSR);
  if (bmsr & PHY_BSR_LSTATUS) {                                  // Link established ?
    bmcr = _PHY_ReadReg(MII_BMCR);
    if(0 == (PHY_BMCR_ANENABLE & bmcr))
    {
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
    else
    {
      physta = _PHY_ReadReg(MII_PHYSTS);
      if(PHY_PHYSTS_FD & physta)
      {
        Duplex = IP_DUPLEX_FULL;
      }
      else
      {
        Duplex = IP_DUPLEX_HALF;
      }
      if(PHY_PHYSTS_100 & physta)
      {
        Speed = IP_SPEED_100MHZ;
      }
      else if (PHY_PHYSTS_10 & physta)
      {
        Speed = IP_SPEED_10MHZ;
      }
    }
  }
  *pDuplex = Duplex;
  *pSpeed  = Speed;
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
  _FEC_ECR  = 0;                                  // Disable FEC
  if (Duplex == IP_DUPLEX_FULL) {
    _FEC_RCR  =  _FEC_RCR_MII_MODE
               | _FEC_RCR_BC_MAX_FL(RX_BUFFER_SIZE); // Maximum frame length
    _FEC_TCR |= _FEC_TCR_FDEN;
  }
  else
  {
    _FEC_RCR  =  _FEC_RCR_DRT      |                  // Disable reception of frames while transmitting
                 _FEC_RCR_MII_MODE |
                 _FEC_RCR_BC_MAX_FL(RX_BUFFER_SIZE);  // Maximum frame length
    _FEC_TCR &= ~_FEC_TCR_FDEN;
  }
  _FEC_ECR  = _FEC_ECR_ETHER_EN;
  _FEC_RDAR = 0;

  _FEC_MIIGSK_ENR = 0;
  while(((1<< 2)& _FEC_MIIGSK_ENR));
  if(IP_SPEED_100MHZ == Speed)
  {
    _FEC_MIIGSK_CFGR =  1         // RMII Mode
                     | (0 << 6)   // 100 Mb
                     ;
  }
  else if (IP_SPEED_10MHZ == Speed)
  {
    _FEC_MIIGSK_CFGR =  1         // RMII Mode
                     | (1 << 6)   // 10 Mb
                     ;
  }
  _FEC_MIIGSK_ENR = 1<<1;
  while(!((1<< 2)& _FEC_MIIGSK_ENR));
}

/*********************************************************************
*
*       _UpdateLinkState
*
*  Function description
*    Reads link state information from PHY and updates EMAC if necessary.
*    Should be called regularity to make sure that EMAC is notified if the link changes.
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
*        Alternatively, the MAC can be switched to promiscuous mode for simple implementations.
*/
static int _SetFilter(IP_NI_CMD_SET_FILTER_DATA * pFilter) {
  U32 v;
  U32 w;
  unsigned i;
  unsigned NumAddr;
  const U8 * pAddrData;

  NumAddr = pFilter->NumAddr;
  //
  // Use the precise filters for the first 4 addresses, hash for the remaining ones
  //
  for (i = 0; i < NumAddr; i++) {
    pAddrData   = *(&pFilter->pHWAddr + i);
    v = IP_LoadU32BE(pAddrData);      // lower (first) 32 bits
    w = IP_LoadU32BE(pAddrData + 4) & 0xFFFF0000;  // upper (last)  16 bits

    if (i < 1) {         // Perfect filter available ?
      _FEC_PALR = v;
      _FEC_PAUR = w | 0x8808UL;
    } else {
      // TBD: add to hash filter variables
    }
    pAddrData += 6;
  }

  //
  // Update hash filter
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
  void   * pPacket;
  unsigned NumBytes;

  IP_GetNextOutPacket(&pPacket, &NumBytes);
  if (NumBytes == 0) {
    _TxIsBusy = 0;
    return 0;
  }

  IP_LOG((IP_MTYPE_DRIVER, "DRIVER: Sending packet: %d bytes", NumBytes));
  INC(_TxSendCnt);

  if(_pCurTX->R)
  {
    // TX queue full
    IP_PANIC("TX queue full!!!");
    return 0;
  }

  //
  // Copy data into Ethernet RAM
  //
  memcpy((void *)_pCurTX->Addr, pPacket, NumBytes);
  IP_RemoveOutPacket();      // Right after memcopy, stack can forget about the packet

  //
  // Prepare descriptor
  //
  if (NumBytes < 60) {
    NumBytes = 60;       // Make sure packet is at least 64 bytes (4 bytes are CRC) so we do not rely on the hardware to pad
  }
  _pCurTX->DataLength = NumBytes;
  _pCurTX->L   = 1;
  _pCurTX->TC  = 1;
  _pCurTX->ABC = 0;
  _pCurTX->R   = 1;

  if(_pCurTX->W)
  {
    _pCurTX = _TxBD;
  }
  else
  {
    ++_pCurTX;
  }
  //
  // Start send
  //
  _FEC_TDAR = 0;
  return 0;
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
  int     i;
  U8 * pBufferStat = FEC_Buffer;
  //
  // Initialize Rx buffer descriptors
  //
  for (i = 0; i < NUM_RX_BUFFERS; i++)
  {
    _RxBD[i].Ctrl = (i == (NUM_RX_BUFFERS-1))?RX_BD_E | RX_BD_W:RX_BD_E;
    _RxBD[i].Addr = (U32)pBufferStat;
    pBufferStat += RX_BUFFER_SIZE;
  }

  // Initialize Tx buffer descriptors
  //
  for (i = 0; i < NUM_TX_BUFFERS; i++)
  {
    _TxBD[i].Ctrl = (i == (NUM_TX_BUFFERS-1))?TX_BD_W:0;
    _TxBD[i].Addr = (U32)pBufferStat;
    pBufferStat += TX_BUFFER_SIZE;
  }
}

/*********************************************************************
*
*       _OnTx
*
*  Function description
*    Tx interrupt handler
*/
static void _OnTx(void) {
  if (_TxIsBusy) {
    INC(_TxIntCnt);
    _SendPacket();
  } else {
    IP_WARN((IP_MTYPE_DRIVER, "DRIVER: Tx complete interrupt, but no packet sent."));
  }
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
  if(_FEC_EIR & _FEC_EIR_EIMR_EBERR)
  {
    // Clear interrupt flag
    _FEC_EIR = _FEC_EIR_EIMR_EBERR;
    // Ethernet bus error
    // Enable FEC
    _FEC_ECR = _FEC_ECR_ETHER_EN;
    _FEC_RDAR = 0;
  }
  if (_FEC_EIR & _FEC_EIR_EIMR_RXF)
  {
    // Clear interrupt flag
    _FEC_EIR = _FEC_EIR_EIMR_RXF;
    INC(_RxIntCnt);
    IP_OnRx();
  }
  if(_FEC_EIR & (_FEC_EIR_EIMR_UN | _FEC_EIR_EIMR_LC))
  {
    _FEC_EIR = _FEC_EIR_EIMR_TXF | _FEC_EIR_EIMR_UN | _FEC_EIR_EIMR_LC;
    // try to retransmit
    _pCurTX->L   = 1;
    _pCurTX->TC  = 1;
    _pCurTX->ABC = 0;
    _pCurTX->R   = 1;
    _FEC_TDAR = 0;
  }
  else if(_FEC_EIR & _FEC_EIR_EIMR_TXF)
  {
    // Clear interrupt flag
    _FEC_EIR = _FEC_EIR_EIMR_TXF;
    _OnTx();
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
  U32  Speed;
  U32  Duplex;
  int r;
  // Clock enable
  _CRM_CGCR0   |=   _CRM_PCCR0_HCLK_FEC;
  _CRM_CGCR2   |=   _CRM_PCCR2_FEC_EN;
  BSP_ETH_Init(Unit);
  // Software reset FEC
  _FEC_ECR = _FEC_ECR_RESET;
  while(_FEC_ECR & _FEC_ECR_RESET);
  _FEC_EMRBR = RX_BUFFER_SIZE;

  // Disable ALL FEC interrupts
  _FEC_EIMR = 0x00000000;
  // Clear all pending interrupts
  _FEC_EIR = 0xFFFFFFFF;
  // Init receive control reg
  _FEC_RCR = _FEC_RCR_MII_MODE
           | _FEC_RCR_BC_MAX_FL(RX_BUFFER_SIZE);  // Maximum frame length
  // Init Transmit control reg
  _FEC_TCR = 0;
  // Init MII interface
  _FEC_MSCR = _FEC_MSCR_MII_SPEED(13);

  // individual address upper register
  _FEC_IAUR = 0;
  // individual address lower register
  _FEC_IALR = 0;
  // group (multicast) address upper register
  _FEC_GAUR = 0;
  // group (multicast) address lower register
  _FEC_GALR = 0;

  _FEC_MIIGSK_ENR = 0;
  _FEC_MIIGSK_CFGR =  1         // RMII Mode
                   | (0 << 6)   // 100 Mb
                   ;
  _FEC_MIIGSK_ENR = 1<<1;


  r = _PHY_Init(Unit);                                // Configure the PHY
  if (r) {
    return 1;
  }

  // Init RX and Tx descriptors
  _InitBufferDescs();

  // Set FEC descriptor registers
  _pCurRX = _RxBD;
  _FEC_ERDSR = (U32)_RxBD;
  _pCurTX = _TxBD;
  _FEC_ETDSR = (U32)_TxBD;

  // FEC interrupts
  _FEC_EIMR = _FEC_EIR_EIMR_UN    | // Transmit FIFO underrun.
              _FEC_EIR_EIMR_EBERR | // Ethernet bus error.
              _FEC_EIR_EIMR_RXF   | // Receive frame interrupt
              _FEC_EIR_EIMR_UN    | // Transmit FIFO underrun
              _FEC_EIR_EIMR_LC    | // Late collision
              _FEC_EIR_EIMR_TXF;    // Transmit frame interrupt
  // Install interrupt handle subroutine and enable FEC interrupts
  BSP_ETH_InstallISR(_ISR_Handler);

  _PHY_GetLinkState(&Duplex, &Speed);
  IP_SetCurrentLinkState(Duplex, Speed);
  _UpdateEMACSettings (Duplex, Speed);              /* Inform the EMAC about the current PHY settings       */

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
  // Check for error and discard invalid frames
  while(1)
  {
    if(_pCurRX->E)
    {
      return 0;
    }
    if(  !_pCurRX->L
       || _pCurRX->Ctrl & (RX_BD_TR | RX_BD_OV | RX_BD_CR | RX_BD_NO | RX_BD_LG))
    {
      if(_pCurRX->W)
      {
        _pCurRX->Ctrl = RX_BD_E | RX_BD_W;
        _pCurRX = _RxBD;
      }
      else
      {
        _pCurRX->Ctrl = RX_BD_E;
        ++_pCurRX;
      }
      _FEC_RDAR = 0;
      continue;
    }
    break;
  }
  // return frame size
  return(_pCurRX->DataLength);
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

  U32 Addr = _pCurRX->Addr;

  // Copy received frame
  if (pDest) {
    IP_MEMCPY(pDest, (void*)Addr, NumBytes);
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
  if(_pCurRX->W)
  {
    _pCurRX->Ctrl = RX_BD_E | RX_BD_W;
    _pCurRX = _RxBD;
  }
  else
  {
    _pCurRX->Ctrl = RX_BD_E;
    ++_pCurRX;
  }
  // indicate that the receive descriptor ring has been updated
  _FEC_RDAR = 0;
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
    //
    // TBD: Enable back pressure (if supported) and change return value to 0
    //
    return -1;
  } else if (Cmd == IP_NI_CMD_CLR_BPRESSURE) {
    //
    // TBD: Disable back pressure (if supported) and change return value to 0
    //
    return -1;
  } else if (Cmd == IP_NI_CMD_SET_FILTER) {
    return _SetFilter((IP_NI_CMD_SET_FILTER_DATA*)p);
  } else if (Cmd == IP_NI_CMD_SET_PHY_ADDR) {
    _PhyAddr = (U8)(int)p;
    return 0;
  }
  return -1;
}

/*********************************************************************
*
*       Driver API Table
*
**********************************************************************
*/
const IP_HW_DRIVER IP_Driver_iMX25 = {
  _Init,
  _SendPacketIfTxIdle,
  _GetPacketSize,
  _ReadPacket,
  _Timer,
  _Control
};

/*************************** End of file ****************************/
