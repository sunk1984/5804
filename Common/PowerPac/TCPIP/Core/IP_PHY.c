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
File    : IP_PHY.c
Purpose : Generic PHY driver
--------  END-OF-HEADER  ---------------------------------------------
*/

#include  "IP_Int.h"

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

#define MII_DAVICOM_DSCR          0x10       // Special configuration reg for Davicom PHYs

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
*       Static data
*
**********************************************************************
*/

// None

/*********************************************************************
*
*       Static code
*
**********************************************************************
*/



/*********************************************************************
*
*       _Write
*
*  Function description
*    Writes the data to the indexed Phy register by calling the driver specific write function
*/
static void _Write(IP_PHY_CONTEXT * pContext, unsigned RegIndex, unsigned Data) {
  if (RegIndex == MII_ANAR) {
    pContext->Anar = Data;
  }
  if (RegIndex == MII_BMCR) {
    pContext->Bmcr = Data;
  }
  pContext->pAccess->pfWrite(pContext, RegIndex, Data);
}

/*********************************************************************
*
*       _IP_PHY_Init
*
*  Function description
*
*  Return value
*    0    OK
*    1    Error (No PHY found)
*/
static int _IP_PHY_Init(IP_PHY_CONTEXT * pContext) {
  unsigned v, w;
  U16 Id1;
  unsigned Addr;
  unsigned FirstAddr;
  unsigned LastAddr;

  //
  // Try to detect PHY on any permitted addr
  //
  if (pContext->Addr == IP_PHY_ADDR_ANY) {
    FirstAddr = 0;
    LastAddr  = 0x1f;
  } else {
    FirstAddr = pContext->Addr;
    LastAddr  = pContext->Addr;
  }
  for (Addr = FirstAddr; ; Addr++) {
    if (Addr > LastAddr) {
      IP_WARN((IP_MTYPE_DRIVER, "DRIVER: no PHY found."));
      return 1;              // No PHY found
    }
    pContext->Addr = (U8)Addr;
    v = pContext->pAccess->pfRead(pContext, MII_ANAR);
    v &= 0x1f;    // Lower 5 bits are fixed: 00001b
    if (v != 1) {
      continue;
    }
    v = pContext->pAccess->pfRead(pContext, MII_PHYSID1);
    if ((v == 0) || (v == 0xFFFF)) {
      continue;
    }
    w = pContext->pAccess->pfRead(pContext, MII_PHYSID1);
    if (v != w) {
      continue;
    }
    Id1 = v;
    IP_LOG((IP_MTYPE_DRIVER | IP_MTYPE_INIT, "DRIVER: Found PHY with Id 0x%x at addr 0x%x", v, Addr));
    break;
  }
  //
  // Reset PHY
  //
  v = pContext->pAccess->pfRead(pContext, MII_BMCR);
  v |= (1 << 15);    // Reset
  _Write(pContext, MII_BMCR, v);
  //
  // Wait until PHY is out of RESET
  //
  while (1) {
    v = pContext->pAccess->pfRead(pContext, MII_BMCR);
    if ((v & (1 << 15)) == 0) {
      break;
    }
  }
  //
  // For Davicom PHYs, let us make sure RMII / MII is correctly selected
  // Note that this should normally have been done correctly by hardware (mode is typically sampled during power-on RESET)
  //
  if (Id1 == 0x181) {
    v = pContext->pAccess->pfRead(pContext, MII_DAVICOM_DSCR);
    w = v & ~(1uL << 8);
    if (pContext->UseRMII) {    // RMII selected ?
      w |= (1 << 8);
    }
    if (v != w) {
      IP_LOG((IP_MTYPE_DRIVER | IP_MTYPE_INIT, "DRIVER: PHY mode selected RMII/MII incorrect. Fixed.", v, Addr));
      _Write(pContext, MII_DAVICOM_DSCR, w);
    }
  }
  if (pContext->SupportedModes == 0) {
    pContext->SupportedModes = 0
                             | IP_PHY_MODE_10_HALF
                             | IP_PHY_MODE_10_FULL
                             | IP_PHY_MODE_100_HALF
                             | IP_PHY_MODE_100_FULL
                             ;
  }
  //
  // Configure PHY ANAR register
  //
  v  = pContext->pAccess->pfRead(pContext, MII_ANAR);
  v &= ~(0
       | IP_PHY_MODE_10_HALF
       | IP_PHY_MODE_10_FULL
       | IP_PHY_MODE_100_HALF
       | IP_PHY_MODE_100_FULL
       );
  v |= pContext->SupportedModes;
  _Write(pContext, MII_ANAR, v);
  //
  // Connect MII-interface by clearing "ISOLATE" and start auto negotiation
  //
  v = PHY_BMCR_ANRESTART       // Restart auto-negotiation
    | PHY_BMCR_ANENABLE        // Enable  auto-negotiation
    ;
  _Write(pContext, MII_BMCR, v);
  return 0;
}


/*********************************************************************
*
*       _IP_PHY_GetLinkState
*/
static void _IP_PHY_GetLinkState(IP_PHY_CONTEXT * pContext, U32 * pDuplex, U32 * pSpeed) {
  U32 bmsr;            // Basic Mode Status Register
  U32 bmcr;            // Basic Mode Control Register
  U32 lpa;             // Link Partner Ability
  U32 Speed;
  U32 Duplex;
  U32 v;

  Speed  = 0;
  Duplex = IP_DUPLEX_UNKNOWN;
  //
  // Get Link Status from PHY status reg. Requires 2 reads
  //
  bmsr = pContext->pAccess->pfRead(pContext, MII_BSR);
  if (bmsr & PHY_BSR_LSTATUS) {         // Link established ?
    if (pContext->Bmcr & (1 << 12)) {   // Auto-negotiation enabled ?
      lpa  = pContext->pAccess->pfRead(pContext, MII_LPA);
      if ((lpa & 0x1F) != 1) {          // Some PHY require reading LPA twice
        lpa = pContext->pAccess->pfRead(pContext, MII_LPA);
      }
      v = lpa & pContext->Anar;
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
      bmcr = pContext->Bmcr;
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
*       Driver API Table
*
**********************************************************************
*/
const IP_PHY_DRIVER IP_PHY_Generic = {
  _IP_PHY_Init,
  _IP_PHY_GetLinkState,
};

/*************************** End of file ****************************/

