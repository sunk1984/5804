/*********************************************************************
*  
*   IAR PowerPac
*
*   (c) Copyright IAR Systems 2010.  All rights reserved.
*
**********************************************************************
----------------------------------------------------------------------
File    : IP_NI_STM32F207.c
Purpose : NI driver for ST STM32F207
--------  END-OF-HEADER  ---------------------------------------------
*/

#include  <stdio.h>      // For printf (when debugging, optional)

#include  "IP_Int.h"
#include  "BSP.h"        // Board specifics
#include  "IP_NI_STM32F207.h"

/*********************************************************************
*
*       Defines, configurable
*
**********************************************************************
*/
#define NUM_RX_BUFFERS     (3)
#define NUM_TX_BUFFERS     (1)
#define BUFFER_SIZE        (1536)
#define USE_HW_CHECKSUM_TX (1) // Enable Tx checksum computation of the MAC and disable the checksum computation of the stack..
                               // Disabling the Tx checksum computation via MAC can increase the performance for the zero-copy interface.
                               // Rx checksum computation is always enabled.

/*********************************************************************
*
*       Defines, non configurable
*
**********************************************************************
*/

#define RCC_BASE_ADDR             ((unsigned int)(0x40023800))
#define RCC_AHB1RSTR              (*(volatile unsigned int*)(RCC_BASE_ADDR + 0x10))
#define RCC_AHB1ENR               (*(volatile unsigned int*)(RCC_BASE_ADDR + 0x30))

#define ETHERNET_BASE_ADDR         (0x40028000)
//
// MAC registers
//
#define ETH_MACCR                  (*(volatile unsigned int*)(ETHERNET_BASE_ADDR +   0x0))  // MAC configuration register (ETH_MACCR)
#define ETH_MACFFR                 (*(volatile unsigned int*)(ETHERNET_BASE_ADDR +   0x4))  // MAC frame filter register (ETH_MACFFR)
#define ETH_MACHTHR                (*(volatile unsigned int*)(ETHERNET_BASE_ADDR +   0x8))  // MAC hash table high register (ETH_MACHTHR)
#define ETH_MACHTLR                (*(volatile unsigned int*)(ETHERNET_BASE_ADDR +   0xC))  // MAC hash table low register (ETH_MACHTLR)
#define ETH_MACMIIAR               (*(volatile unsigned int*)(ETHERNET_BASE_ADDR +  0x10))  // MAC MII address register (ETH_MACMIIAR)
#define ETH_MACMIIDR               (*(volatile unsigned int*)(ETHERNET_BASE_ADDR +  0x14))  // MAC MII data register (ETH_MACMIIDR)
#define ETH_MACFCR                 (*(volatile unsigned int*)(ETHERNET_BASE_ADDR +  0x18))  // MAC flow control register (ETH_MACFCR)
#define ETH_MACVLANTR              (*(volatile unsigned int*)(ETHERNET_BASE_ADDR +  0x1C))  // MAC VLAN tag register (ETH_MACVLANTR)
#define ETH_MACRWUFFR              (*(volatile unsigned int*)(ETHERNET_BASE_ADDR +  0x28))  // MAC remote wakeup frame filter register (ETH_MACRWUFFR)
#define ETH_MACPMTCSR              (*(volatile unsigned int*)(ETHERNET_BASE_ADDR +  0x2C))  // MAC PMT control and status register (ETH_MACPMTCSR)
#define ETH_MACDBGR                (*(volatile unsigned int*)(ETHERNET_BASE_ADDR +  0x34))  // MAC debug register (ETH_MACDBGR)
#define ETH_MACSR                  (*(volatile unsigned int*)(ETHERNET_BASE_ADDR +  0x38))  // MAC interrupt status register (ETH_MACSR)
#define ETH_MACIMR                 (*(volatile unsigned int*)(ETHERNET_BASE_ADDR +  0x3C))  // MAC interrupt mask register (ETH_MACIMR)
#define ETH_MACA0HR                (*(volatile unsigned int*)(ETHERNET_BASE_ADDR +  0x40))  // MAC address 0 high register (ETH_MACA0HR)
#define ETH_MACA0LR                (*(volatile unsigned int*)(ETHERNET_BASE_ADDR +  0x44))  // MAC address 0 low register (ETH_MACA0LR)
#define ETH_MACA1HR                (*(volatile unsigned int*)(ETHERNET_BASE_ADDR +  0x48))  // MAC address 1 high register (ETH_MACA1HR)
#define ETH_MACA1LR                (*(volatile unsigned int*)(ETHERNET_BASE_ADDR +  0x4C))  // MAC address1 low register (ETH_MACA1LR)
#define ETH_MACA2HR                (*(volatile unsigned int*)(ETHERNET_BASE_ADDR +  0x50))  // MAC address 2 high register (ETH_MACA2HR)
#define ETH_MACA2LR                (*(volatile unsigned int*)(ETHERNET_BASE_ADDR +  0x54))  // MAC address 2 low register (ETH_MACA2LR)
#define ETH_MACA3HR                (*(volatile unsigned int*)(ETHERNET_BASE_ADDR +  0x58))  // MAC address 3 high register (ETH_MACA3HR)
#define ETH_MACA3LR                (*(volatile unsigned int*)(ETHERNET_BASE_ADDR +  0x5C))  // MAC address 3 low register (ETH_MACA3LR)
//
// MMC registers
//
#define ETH_MMCCR                  (*(volatile unsigned int*)(ETHERNET_BASE_ADDR +  0x100)) // MMC control register (ETH_MMCCR)
#define ETH_MMCRIR                 (*(volatile unsigned int*)(ETHERNET_BASE_ADDR +  0x104)) // MMC receive interrupt register (ETH_MMCRIR)
#define ETH_MMCTIR                 (*(volatile unsigned int*)(ETHERNET_BASE_ADDR +  0x108)) // MMC transmit interrupt register (ETH_MMCTIR)
#define ETH_MMCRIMR                (*(volatile unsigned int*)(ETHERNET_BASE_ADDR +  0x10C)) // MMC receive interrupt mask register (ETH_MMCRIMR)
#define ETH_MMCTIMR                (*(volatile unsigned int*)(ETHERNET_BASE_ADDR +  0x110)) // MMC transmit interrupt mask register (ETH_MMCTIMR)
#define ETH_MMCTGFSCCR             (*(volatile unsigned int*)(ETHERNET_BASE_ADDR +  0x14C)) // MMC transmitted good frames after a single collision counter register (ETH_MMCTGFSCCR)
#define ETH_MMCTGFMSCCR            (*(volatile unsigned int*)(ETHERNET_BASE_ADDR +  0x150)) // MMC transmitted good frames after more than a single collision counter register (ETH_MMCTGFMSCCR)
#define ETH_MMCTGFCR               (*(volatile unsigned int*)(ETHERNET_BASE_ADDR +  0x168)) // MMC transmitted good frames counter register (ETH_MMCTGFCR)
#define ETH_MMCRFCECR              (*(volatile unsigned int*)(ETHERNET_BASE_ADDR +  0x194)) // MMC received frames with CRC error counter register (ETH_MMCRFCECR)
#define ETH_MMCRFAECR              (*(volatile unsigned int*)(ETHERNET_BASE_ADDR +  0x198)) // MMC received frames with alignment error counter register (ETH_MMCRFAECR)
#define ETH_MMCRGUFCR              (*(volatile unsigned int*)(ETHERNET_BASE_ADDR +  0x1C4)) // MMC received good unicast frames counter register (ETH_MMCRGUFCR)
//
// IEEE 1588 time stamp registers
//
#define ETH_PTPTSCR                (*(volatile unsigned int*)(ETHERNET_BASE_ADDR +  0x700)) // PTP time stamp control register (ETH_PTPTSCR)
#define ETH_PTPSSIR                (*(volatile unsigned int*)(ETHERNET_BASE_ADDR +  0x704)) // PTP subsecond increment register (ETH_PTPSSIR)
#define ETH_PTPTSHR                (*(volatile unsigned int*)(ETHERNET_BASE_ADDR +  0x708)) // PTP time stamp high register (ETH_PTPTSHR)
#define ETH_PTPTSLR                (*(volatile unsigned int*)(ETHERNET_BASE_ADDR +  0x70C)) // PTP time stamp low register (ETH_PTPTSLR)
#define ETH_PTPTSHUR               (*(volatile unsigned int*)(ETHERNET_BASE_ADDR +  0x710)) // PTP time stamp high update register (ETH_PTPTSHUR)
#define ETH_PTPTSLUR               (*(volatile unsigned int*)(ETHERNET_BASE_ADDR +  0x714)) // PTP time stamp low update register (ETH_PTPTSLUR)
#define ETH_PTPTSAR                (*(volatile unsigned int*)(ETHERNET_BASE_ADDR +  0x718)) // PTP time stamp addend register (ETH_PTPTSAR)
#define ETH_PTPTTHR                (*(volatile unsigned int*)(ETHERNET_BASE_ADDR +  0x71C)) // PTP target time high register (ETH_PTPTTHR)
#define ETH_PTPTTLR                (*(volatile unsigned int*)(ETHERNET_BASE_ADDR +  0x720)) // PTP target time low register (ETH_PTPTTLR)
//
// DMA registers
//
#define ETH_DMABMR                 (*(volatile unsigned int*)(ETHERNET_BASE_ADDR + 0x1000)) // DMA bus mode register (ETH_DMABMR)
#define ETH_DMATPDR                (*(volatile unsigned int*)(ETHERNET_BASE_ADDR + 0x1004)) // DMA transmit poll demand register (ETH_DMATPDR)
#define ETH_DMARPDR                (*(volatile unsigned int*)(ETHERNET_BASE_ADDR + 0x1008)) // DMA receive poll demand register (ETH_DMARPDR)
#define ETH_DMARDLAR               (*(volatile unsigned int*)(ETHERNET_BASE_ADDR + 0x100C)) // DMA receive descriptor list address register (ETH_DMARDLAR)
#define ETH_DMATDLAR               (*(volatile unsigned int*)(ETHERNET_BASE_ADDR + 0x1010)) // DMA transmit descriptor list address register (ETH_DMATDLAR)
#define ETH_DMASR                  (*(volatile unsigned int*)(ETHERNET_BASE_ADDR + 0x1014)) // DMA status register (ETH_DMASR)
#define ETH_DMAOMR                 (*(volatile unsigned int*)(ETHERNET_BASE_ADDR + 0x1018)) // DMA operation mode register (ETH_DMAOMR)
#define ETH_DMAIER                 (*(volatile unsigned int*)(ETHERNET_BASE_ADDR + 0x101C)) // DMA interrupt enable register (ETH_DMAIER)
#define ETH_DMAMFBOCR              (*(volatile unsigned int*)(ETHERNET_BASE_ADDR + 0x1020)) // DMA missed frame and buffer overflow counter register (ETH_DMAMFBOCR)
#define ETH_DMACHTDR               (*(volatile unsigned int*)(ETHERNET_BASE_ADDR + 0x1048)) // DMA current host transmit descriptor register (ETH_DMACHTDR)
#define ETH_DMACHRDR               (*(volatile unsigned int*)(ETHERNET_BASE_ADDR + 0x104C)) // DMA current host receive descriptor register (ETH_DMACHRDR)
#define ETH_DMACHTBAR              (*(volatile unsigned int*)(ETHERNET_BASE_ADDR + 0x1050)) // DMA current host transmit buffer address register (ETH_DMACHTBAR)
#define ETH_DMACHRBAR              (*(volatile unsigned int*)(ETHERNET_BASE_ADDR + 0x1054)) // DMA current host receive buffer address register (ETH_DMACHRBAR)



#define ETH_MACMII_BUSY_MASK       (0x00000001)

#define ETH_MACCR_CSD_MASK         (1UL << 16)   // CSD: Carrier sense disable. Required in half-duplex mode.
#define ETH_MACCR_FES_MASK         (1UL << 14)   // FES: Fast Ethernet speed
#define ETH_MACCR_DP_MASK          (1UL << 11)   // DM: Duplex mode


#define EMAC_RXBUF_DMA_OWNED_MASK  (1UL << 31)
#define EMAC_RXBUF_EOF_MASK        (1UL <<  8) // Last descriptor: When set, this bit indicates that the buffers pointed to by this descriptor are the last buffers of the frame.


#define EMAC_DMA_ISR_NIS_MASK      (1UL << 16) // NIS:  Normal interrupt status
#define EMAC_DMA_ISR_AIS_MASK      (1UL << 15) // AIS:  Abnormal interrupt status
#define EMAC_DMA_ISR_FBES_MASK     (1UL << 13) // FBES: Fatal bus error status
#define EMAC_DMA_ISR_RBUS_MASK     (1UL <<  7) // RBUS: Receive buffer unavailable status
#define EMAC_DMA_ISR_RS_MASK       (1UL <<  6) // RS:   Receive status
#define EMAC_DMA_ISR_ROS_MASK      (1UL <<  4) // ROS:  Receive overflow status
#define EMAC_DMA_ISR_TBUS_MASK     (1UL <<  2) // TBUS: Transmit interrupt enable
#define EMAC_DMA_ISR_TS_MASK       (1UL <<  0) // TIE:  Transmit interrupt enable


/*********************************************************************
*
*       Types
*
**********************************************************************
*/
typedef struct {
  U32  BufDesc0;
  U32  BufDesc1;
  U32  BufDesc2;
  U32  BufDesc3;
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

static BUFFER_DESC  *_paRxBufDesc;              // Pointer to Rx buffer descriptors
static BUFFER_DESC  *_pTxBufDesc;               // Pointer to the only Tx buffer descriptor
static U16           _iNextRx;
static char          _TxIsBusy;
static U16           _NumRxBuffers = NUM_RX_BUFFERS;

static IP_PHY_CONTEXT         _PHY_Context;
static const IP_PHY_DRIVER  * _PHY_pDriver = &IP_PHY_Generic;
static U8                     _IsInited;

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
  unsigned Val;

  Addr = pContext->Addr;
  Val    = ETH_MACMIIAR;
  if ((Val & ETH_MACMII_BUSY_MASK) == 0) {  // Check if busy
    ETH_MACMIIDR = (v & 0xFFFF);
    Val = (Addr     << 11  // [15:11] PA: PHY Address
        |  RegIndex <<  6  //  [10:6] MR: MII Register
        |  0        <<  5  //     [5] Reserved
        |  0        <<  2  //   [4:2] CR: Clock Range, valid values are: 000  60-100 MHz HCLK/42; 001  100-120 MHz   HCLK/62; 010 20-35 MHz HCLK/16; 011  35-60 MHz HCLK/26; 100, 101, 110, 111   Reserved
        |  1        <<  1  //     [1] MW: MII Write. When set, this bit tells the PHY that this will be a Write operation using the MII Data register. If this bit is not set, this will be a Read operation, placing the data in the MII Data register.
        |  1        <<  0  //     [0] MW: MII Busy
          );
    ETH_MACMIIAR = Val;
  }
  //
  // Wait for commmand to finish
  //
  while (ETH_MACMIIAR & ETH_MACMII_BUSY_MASK);
}

/*********************************************************************
*
*       _PHY_ReadReg
*/
static unsigned _PHY_ReadReg(IP_PHY_CONTEXT* pContext, unsigned RegIndex) {
  unsigned v;
  unsigned Addr;

  Addr = pContext->Addr;
  v    = ETH_MACMIIAR;
  if ((v & ETH_MACMII_BUSY_MASK) == 0) {  // Check if busy
    v = (Addr     << 11  // [15:11] PA: PHY Address
      |  RegIndex <<  6  //  [10:6] MR: MII Register
      |  0        <<  5  //     [5] Reserved
      |  0        <<  2  //   [4:2] CR: Clock Range, valid values are: 000  60-100 MHz HCLK/42; 001  100-120 MHz   HCLK/62; 010 20-35 MHz HCLK/16; 011  35-60 MHz HCLK/26; 100, 101, 110, 111   Reserved
      |  0        <<  1  //     [1] MW: MII Write. When set, this bit tells the PHY that this will be a Write operation using the MII Data register. If this bit is not set, this will be a Read operation, placing the data in the MII Data register.
      |  1        <<  0  //     [0] MW: MII Busy
        );
    ETH_MACMIIAR = v;
  }
  //
  // Wait for commmand to finish
  //
  while (ETH_MACMIIAR & ETH_MACMII_BUSY_MASK);
  //
  // Read and return data
  //
  v = (ETH_MACMIIDR & 0xFFFF);
  return v;
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
//static U32 _ComputeHash(U32 l, U32 h) {
//  l = l ^ (l >> 24) ^ (h << 8);   // Fold 48 bits to 24 bits
//  l ^= l >> 12;                   // Fold to 12 bits
//  l ^= l >> 6;                    // Fold to 6 bits
//  return l & 63;
//}

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
*        available, then the hash filter is used. Alternativly, the MAC can be switched
*        to promiscuous mode for simple implementations.
*/
static int _SetFilter(IP_NI_CMD_SET_FILTER_DATA * pFilter) {
  const U8 * pAddrData;

  //
  // The first DA byte that is received on the MII interface corresponds to the LS Byte
  // (bits [7:0]) of the MAC address low register. For example, if 0x1122 3344 5566
  // is received (0x11 is the first byte) on the MII as the destination address, then the MAC
  // address 0 register [47:0] is compared with 0x6655 4433 2211.
  //
  pAddrData   = *(&pFilter->pHWAddr);
  ETH_MACA0LR = IP_LoadU32LE(pAddrData);      // lower (first) 32 bits
  ETH_MACA0HR = IP_LoadU16LE(pAddrData + 4);  // upper (last)  16 bits
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
  v = ETH_MACCR;
  v &= ~(ETH_MACCR_FES_MASK | ETH_MACCR_DP_MASK | ETH_MACCR_CSD_MASK);
  if (Duplex == IP_DUPLEX_FULL) {
    v |= ETH_MACCR_DP_MASK;
  } else {
    v |= ETH_MACCR_CSD_MASK;
  }
  if (Speed ==  IP_SPEED_100MHZ) {
    v |= ETH_MACCR_FES_MASK;
  }
  ETH_MACCR = v;
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
    _UpdateEMACSettings (Duplex, Speed);        // Inform the EMAC about the current PHY settings
  }
}

/**********************************************************************************************************
*
*       _AllocBufferDescs()
*
*  Function description:
*    Allocates the Tx and Rx buffer descriptors.
*/
static void _AllocBufferDescs(void) {
  BUFFER_DESC  *pBufferDesc;
  U8 * pMem;

  //
  // Alloc memory for buffer descriptors and buffers
  //
  pMem = (U8*)IP_Alloc((_NumRxBuffers * (sizeof(BUFFER_DESC) + 1536)) + (NUM_TX_BUFFERS * sizeof(BUFFER_DESC)) + 8);   // Alloc nRxBufferDesc * (32 bytes buffer desc + 1536 buffer) + Tx buffer descriptors + space for aligment
  pMem = (U8*)_Align(pMem, 8);
  pBufferDesc = (BUFFER_DESC*) pMem;
  _pTxBufDesc  = pBufferDesc;
  _paRxBufDesc = pBufferDesc + NUM_TX_BUFFERS;
}

/**********************************************************************************************************
*
*       _InitRxBufferDescs()
*
*  Function description:
*    Initializes the Rx buffer descriptors.
*/
static void _InitRxBufferDescs(void) {
  BUFFER_DESC * pBufferDesc;
  U32           DataAddr;
  int           i;

  pBufferDesc = _paRxBufDesc;
  DataAddr    = ((U32)_paRxBufDesc) + sizeof(BUFFER_DESC) * _NumRxBuffers;   // Addr of first buffer
  for (i = 0; i < _NumRxBuffers; i++) {
    pBufferDesc->BufDesc0 = ( 0
                          |  (1uL << 31) // OWN: Own bit
                          |  (0uL << 16) // AFM: Destination address filter fail [29:16]
                          |  (0uL << 15) // ES: Error summary
                          |  (0uL << 14) // DE: Descriptor error
                          |  (0uL << 13) // SAF: Source address filter fail
                          |  (0uL << 12) // LE: Length error
                          |  (0uL << 11) // OE: Overflow error
                          |  (0uL << 10) // VLAN: VLAN tag
                          |  (0uL <<  9) // FS: First descriptor
                          |  (0uL <<  8) // LS: Last descriptor
                          |  (0uL <<  7) // IPHCE: IPv header checksum error
                          |  (0uL <<  6) // LC: Late collision
                          |  (0uL <<  5) // FT: Frame type
                          |  (0uL <<  4) // RWT: Receive watchdog timeout
                          |  (0uL <<  3) // RE: Receive error
                          |  (0uL <<  2) // DE: Dribble bit error
                          |  (0uL <<  1) // CE: CRC error
                          |  (0uL <<  0) // RMAM/PCE: Rx MAC address matched/Payload checksum error
                          );
    pBufferDesc->BufDesc1 = ( 0
                          |  (1uL << 14)
                          |  (BUFFER_SIZE & 0xFFF)
                            );
    pBufferDesc->BufDesc2 = DataAddr;
    pBufferDesc->BufDesc3 = (U32)(pBufferDesc + 1);
    DataAddr             += BUFFER_SIZE;
    pBufferDesc++;
  }
  (pBufferDesc - 1)->BufDesc1 |= (1 << 15); // RER: Receive end of ring
  (pBufferDesc - 1)->BufDesc3  = 0;
  _iNextRx = 0;
}

/**********************************************************************************************************
*
*       _InitTxBufferDescs()
*
*  Function description:
*    Initializes the Tx buffer descriptors.
*/
static void _InitTxBufferDescs(void) {
  BUFFER_DESC * pBufferDesc;

  pBufferDesc = _pTxBufDesc;
  pBufferDesc->BufDesc0 = ( 0
                        |  (0 << 31) // OWN: Own bit
                        |  (1 << 30) // IC:  Interrupt on completion
                        |  (1 << 29) // LS:  Last segment
                        |  (1 << 28) // FS:  First segment
                        |  (0 << 27) // DC:  Disable CRC
                        |  (0 << 26) // DP:  Disable pad
#if USE_HW_CHECKSUM_TX
                        |  (3 << 22) // CIC: Checksum insertion control
#else
                        |  (0 << 22) // CIC: Checksum insertion control
#endif
                        |  (0 << 21) // TER: Transmit end of ring
                        |  (1 << 20) // TCH: Second address chained
                        |  (0 << 16) // IHE: IP header error
                        |  (0 << 15) // ES:  Error summary
                          );
  pBufferDesc->BufDesc1 = (1 << 21); // RER: Receive end of ring
  pBufferDesc->BufDesc2 = 0;
  pBufferDesc->BufDesc3 = (U32)pBufferDesc;
}

/**********************************************************************************************************
*
*       _InitBufferDescs()
*
*  Function description:
*    Initializes the Tx and Rx buffer descriptors.
*/
static void _InitBufferDescs(void) {
  _InitTxBufferDescs();
  _InitRxBufferDescs();
}

/**********************************************************************************************************
*
*       _ResetRxError()
*
*  Function description:
*    Resets the receiver logic in case of Fatal Rx errors (not simple packet corruption like CRC error)
*/
static void _ResetRxError(void) {
  _InitRxBufferDescs();
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
    pBufDesc->BufDesc0 = EMAC_RXBUF_DMA_OWNED_MASK;
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
static void  _SendPacket(void) {
  void   * pPacket;
  unsigned NumBytes;
  U32 v;

  NumBytes = IP_GetNextOutPacketFast(&pPacket);
  if (NumBytes == 0) {
    _TxIsBusy = 0;
    return;
  }
  IP_LOG((IP_MTYPE_DRIVER, "DRIVER: Sending packet: %d bytes", NumBytes));
  INC(_DriverStats.TxSendCnt);
  _pTxBufDesc->BufDesc1  = ((NumBytes) & 0xFFF);
  _pTxBufDesc->BufDesc2  = ((U32)pPacket);
  _pTxBufDesc->BufDesc0 |= (1UL << 31);    // Set OWN bit
  //
  // Check if transmit buffer unavailable status bit is set and clear it if necessary
  //
  v = ETH_DMASR;
  if (v & (1 << 2)) {
    ETH_DMASR = (1 << 2);  // Clear transmit buffer unavailable status bit
  }
  ETH_DMATPDR = 0;         // When these bits are written with any value, the DMA reads the current descriptor pointed to by the ETH_DMACHRDR register.
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
  U32 v;

  v = ETH_DMASR;
  ETH_DMASR = v;       // Clear DMA interrupt status register
  //
  // Process normal interrupts
  //
  if (v & EMAC_DMA_ISR_NIS_MASK) {
    //
    // Rx interrupt
    //
    if (v & EMAC_DMA_ISR_RS_MASK) {
      INC(_DriverStats.RxIntCnt);
      IP_OnRx();
    }
    //
    // Tx interrupt
    //
    if (v & EMAC_DMA_ISR_TS_MASK) {
      if (_TxIsBusy) {
        INC(_DriverStats.TxIntCnt);
        IP_RemoveOutPacket();
        _SendPacket();
      } else {
        IP_WARN((IP_MTYPE_DRIVER, "DRIVER: Tx complete interrupt, but no packet sent."));
      }
    }
    if (v & EMAC_DMA_ISR_TBUS_MASK) {
      if (_TxIsBusy) {
        INC(_DriverStats.TxIntCnt);
        ETH_DMATPDR = 0;
      }
    }
  }
  //
  // Check process Rx error interrupts
  //
  if (v & EMAC_DMA_ISR_AIS_MASK) {
    INC(_DriverStats.RxErrCnt);
    if (v & (EMAC_DMA_ISR_FBES_MASK | EMAC_DMA_ISR_RBUS_MASK | EMAC_DMA_ISR_ROS_MASK)) {
      IP_WARN_INTERNAL((IP_MTYPE_DRIVER, "DRIVER: No Rx DMA buffers available"));
      _ResetRxError();
      ETH_DMATPDR = 0;
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
*    1. Write to ETH_DMABMR to set STM32F207xx bus access parameters.
*    2. Write to the ETH_DMAIER register to mask unnecessary interrupt causes.
*    3. The software driver creates the transmit and receive descriptor lists. Then it writes to both the ETH_DMARDLAR and ETH_DMATDLAR registers, providing the DMA with the start address of each list.
*    4. Write to MAC Registers 1, 2, and 3 to choose the desired filtering options.
*    5. Write to the MAC ETH_MACCR register to configure and enable the transmit and receive operating modes. The PS and DM bits are set based on the auto-negotiation result (read from the PHY).
*    6. Write to the ETH_DMAOMR register to set bits 13 and 1 and start transmission and reception.
*    7. The transmit and receive engines enter the running state and attempt to acquire descriptors from the respective descriptor lists. The receive and transmit engines then begin processing receive and transmit operations. The transmit and receive processes are independent of each other and can be started or stopped separately.
*/
static  int  _Init(unsigned Unit) {
  _PHY_Context.pAccess = &_PHY_pAccess;        // PHY read/write functions are static in this module
  _PHY_Context.Addr    = IP_PHY_ADDR_ANY;      // Activate automatic scan for external PHY

  BSP_ETH_Init(Unit);                          // Configure the pins and interface type (MII or RMII)
  //
  // Enable EMAC clocks
  //
  RCC_AHB1ENR  |= (0
                | (1 << 28)  // ETHMACTPEN: Ethernet PTP clock enable
                | (1 << 27)  // ETHMACRXEN: Ethernet MAC reception clock enable
                | (1 << 26)  // ETHMACTXEN: Ethernet MAC transmission clock enable
                | (1 << 25)  // ETHMACEN: Ethernet MAC clock enable
                  );
  //
  // Reset the ETH controller
  //
  RCC_AHB1RSTR |= (1uL << 25);
  RCC_AHB1RSTR &= ~(1uL << 25);
  //
  // Init PIO, PHY and update link state
  //
  _PHY_pDriver->pfInit(&_PHY_Context);                                // Configure the PHY
  _UpdateLinkState();
  //
  // Creates descriptor lists and provide the DMA with the start address of each list.
  //
  _AllocBufferDescs();
  _InitBufferDescs();
  //
  // Set bus access parameters
  //
  ETH_DMABMR = ( 1 << 25 // AAB[25]:     Address-aligned beats
               | 0 << 24 // FPM[24]:     4xPBL mode
               | 1 << 23 // USP[23]:     Use separate PBL
               | 1 << 17 // RDP[22:17]:  Rx DMA PBL
               | 0 << 16 // FB[16]:      Fixed burst
               | 0 << 14 // RTPR[15:14]: Rx Tx priority ratio
               | 1 <<  8 // PBL[13:8]:   Programmable burst length
               | 0 <<  2 // DSL[6:2]:    Descriptor skip length
               | 0 <<  1 // DA[1]:       DMA Arbitration
               | 0 <<  0 // SR[0]:       Software reset
               );
  //
  // Enable DMA interrupts
  //
  ETH_DMAIER = ( 1 << 16 // NISE:  Normal interrupt summary enable
               | 1 << 15 // AISE:  Abnormal interrupt summary enable
               | 0 << 14 // ERIE:  Early receive interrupt enable
               | 1 << 13 // FBEIE: Fatal bus error interrupt enable
               | 0 << 10 // ETIE:  Early transmit interrupt enable
               | 0 <<  9 // RWTIE: receive watchdog timeout interrupt enable
               | 0 <<  8 // RPSIE: Receive process stopped interrupt enable
               | 1 <<  7 // RBUIE: Receive buffer unavailable interrupt enable
               | 1 <<  6 // RIE:   Receive interrupt enable
               | 1 <<  5 // TUIE:  Underflow interrupt enable
               | 1 <<  4 // ROIE:  Overflow interrupt enable
               | 0 <<  3 // TJTIE: Transmit jabber timeout interrupt enable
               | 1 <<  2 // TBUIE: Transmit buffer unavailable interrupt enable
               | 0 <<  1 // TPSIE: Transmit process stopped interrupt enable
               | 1 <<  0 // TIE:   Transmit interrupt enable
               );
  ETH_DMARDLAR = (U32)_paRxBufDesc;
  ETH_DMATDLAR = (U32)_pTxBufDesc;
  //
  //
  //
  ETH_MACFFR = ( 0
               | 0 << 31 // RA:   Receive all
               | 1 << 10 // HPF:  Hash or perfect filter
               | 0 <<  9 // SAF:  Source address filter
               | 0 <<  8 // SAIF: Source address inverse filtering
               | 0 <<  6 // PCF:  Pass control frames, 00 or 01: MAC prevents all control frames from reaching the application, 10: MAC forwards all control frames to application even if they fail the address filter, 11: MAC forwards control frames that pass the address filter.
               | 0 <<  5 // BFD:  Broadcast frames disable
               | 0 <<  4 // PAM:  Pass all multicast
               | 0 <<  3 // DAIF: Destination address inverse filtering
               | 0 <<  2 // HM:   Hash multicast
               | 0 <<  1 // HU:   Hash unicast
               | 0 <<  0 // PM:   Promiscuous mode
               );
  ETH_DMAOMR = ( 0
               | 0 << 26 // DTCEFD:     Dropping of TCP/IP checksum error frames disable
               | 0 << 25 // RSF:        Receive store and forward
               | 0 << 24 // DFRF:       Disable flushing of received frames
#if USE_HW_CHECKSUM_TX
               | 1 << 21 // TSF:        Transmit store and forward
#else
               | 0 << 20 // TSF:        Transmit store and forward
#endif
               | 0 << 20 // FTF:        Flush transmit FIFO
               | 0 << 14 // TTC[16:14]: Transmit threshold control
               | 0 << 13 // ST:         Start/stop transmission
               | 0 <<  7 // FEF:        Forward error frames
               | 0 <<  6 // FUGF:       Forward undersized good frames
               | 0 <<  3 // RTC[4:3]:   Receive threshold control
               | 0 <<  2 // OSF:        Operate on second frame
               | 0 <<  1 // SR:         Start/stop receive
               );
  BSP_ETH_InstallISR(_ISR_Handler);
  ETH_MACCR  = ( 0
               | 0 << 21  // WD:         Watchdog disable
               | 0 << 20  // JD:         Jabber disable
               | 0 << 17  // IFG[19:17]: Interframe gap
               | 0 << 16  // CSD:        Carrier sense disable
               | 1 << 14  // FES:        Fast Ethernet speed
               | 0 << 13  // ROD:        Receive own disable
               | 0 << 12  // LM:         Loopback mode
               | 1 << 11  // DM:         Duplex mode
               | 1 << 10  // IPCO:       IPv4 checksum offload
               | 0 <<  9  // RD:         Retry disable
               | 0 <<  7  // APCS:       Automatic pad/CRC stripping
               | 0 <<  5  // BL[6:5]:    Back-off limit
               | 0 <<  4  // DC:         Deferral check
               | 1 <<  3  // TE:         Transmitter enable
               | 1 <<  2  // RE:         Receiver enable
               );
  ETH_DMAOMR |= ( 0
                | 1 << 13 // ST: Start/stop transmission
                );
  ETH_DMAOMR |= ( 0
                | 1 <<  1 // SR: Start/stop receive
                );
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
*
*  Return value
*    0   O.K.
*    1   Interface error
*/
static int  _SendPacketIfTxIdle(unsigned Unit) {
  (void)Unit;
  if (_TxIsBusy == 0) {
    _TxIsBusy = 1;
    _SendPacket();
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
*    Number of buffers used for the next packet.
*    0 if no complete packet is available.
*/
static int _GetPacketSize(unsigned Unit) {
  U32 Stat;
  BUFFER_DESC * pBufferDesc;
  int i;
  int NumBuffers;
  int PacketSize;

  (void)Unit;
  i = _iNextRx;
  NumBuffers = 1;
  do {
    if (i >= _NumRxBuffers) {
      i -= _NumRxBuffers;
    }
    pBufferDesc = _paRxBufDesc + i;
    Stat = pBufferDesc->BufDesc0;

    if ((Stat & EMAC_RXBUF_DMA_OWNED_MASK) == 1) {
      return 0;        //  This happens all the time since the stack polls until we are out of packets
    }
    if (Stat & EMAC_RXBUF_EOF_MASK) {
      // FoundPacket
      PacketSize  = ((Stat & 0x3FFF0000) >> 16); // Packet size are bit [29:16] of Status.
      PacketSize -= 4;                           // Substract 4 bytes to remove the CRC of the Ethernet frame.
      return PacketSize;
    }
    i++;
  } while (++NumBuffers < _NumRxBuffers);
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
*/
static int _ReadPacket(unsigned Unit, U8 *pDest, unsigned NumBytes) {
  U8     *pSrc;
  U32 Addr;
  int NumBuffers;

  (void)Unit;
  NumBuffers = 1;
  if (pDest) {
    Addr = (_paRxBufDesc + _iNextRx)->BufDesc2;
    pSrc = (U8*) Addr;
    IP_MEMCPY(pDest, pSrc, NumBytes);
    INC(_DriverStats.RxCnt);
  }
  if (pDest) {
    IP_LOG((IP_MTYPE_DRIVER, "Packet: %d Bytes --- Read", NumBytes));
  } else {
    IP_LOG((IP_MTYPE_DRIVER, "Packet: %d Bytes --- Discarded", NumBytes));
  }
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
  (void)Unit;
  _UpdateLinkState();
}

/*********************************************************************
*
*       _Control
*
*  Function description
*/
static int _Control(unsigned Unit, int Cmd, void * p) {
  (void)Unit;
  if (Cmd == IP_NI_CMD_SET_FILTER) {
    return _SetFilter((IP_NI_CMD_SET_FILTER_DATA*)p);
  } else if (Cmd == IP_NI_CMD_SET_BPRESSURE) {
    return -1;
  } else if (Cmd == IP_NI_CMD_CLR_BPRESSURE) {
    return -1;
  } else if (Cmd == IP_NI_CMD_SET_PHY_ADDR) {
    _PHY_Context.Addr = (U8)(int)p;
    return 0;
  } else if (Cmd == IP_NI_CMD_SET_PHY_MODE) {
    _PHY_Context.UseRMII = (U8)(int)p;
    return 0;
  } else if (Cmd == IP_NI_CMD_GET_CAPS) {
    int v;
    v = 0
#if USE_HW_CHECKSUM_TX
     | IP_NI_CAPS_WRITE_IP_CHKSUM     // Driver capable of inserting the IP-checksum into an outgoing packet?
     | IP_NI_CAPS_WRITE_UDP_CHKSUM    // Driver capable of inserting the UDP-checksum into an outgoing packet?
     | IP_NI_CAPS_WRITE_TCP_CHKSUM    // Driver capable of inserting the TCP-checksum into an outgoing packet?
     | IP_NI_CAPS_WRITE_ICMP_CHKSUM   // Driver capable of inserting the ICMP-checksum into an outgoing packet?
#endif
     | IP_NI_CAPS_CHECK_IP_CHKSUM     // Driver capable of computing and comparing the IP-checksum of an incoming packet?
     | IP_NI_CAPS_CHECK_UDP_CHKSUM    // Driver capable of computing and comparing the UDP-checksum of an incoming packet?
     | IP_NI_CAPS_CHECK_TCP_CHKSUM    // Driver capable of computing and comparing the TCP-checksum of an incoming packet?
     | IP_NI_CAPS_CHECK_ICMP_CHKSUM   // Driver capable of computing and comparing the ICMP-checksum of an incoming packet?
     ;
    return v;
  }
  else if ( Cmd == IP_NI_CMD_SET_SUPPORTED_DUPLEX_MODES) {
    if (_IsInited) {
      return -1;
    }
    _PHY_Context.SupportedModes = (U16)(int)p;
    return 0;
  }
  return -1;
}

/*********************************************************************
*
*       IP_NI_STM32F207_ConfigNumRxBuffers
*
*  Function description
*    Sets the number of Rx Buffers in the config phase.
*/
void IP_NI_STM32F207_ConfigNumRxBuffers(U16 NumRxBuffers) {
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
const IP_HW_DRIVER IP_Driver_STM32F207 = {
  _Init,
  _SendPacketIfTxIdle,
  _GetPacketSize,
  _ReadPacket,
  _Timer,
  _Control
};

/*************************** End of file ****************************/

