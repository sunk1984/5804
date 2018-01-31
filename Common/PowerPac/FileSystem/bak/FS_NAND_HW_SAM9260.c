/*********************************************************************
*
*   IAR PowerPac
*
*   (c) Copyright IAR Systems 2010.  All rights reserved.
*
**********************************************************************
----------------------------------------------------------------------
----------------------------------------------------------------------
File        : NAND_HW.c
Purpose     : NAND flash hardware layer for Atmel AT91SAM9260
----------------------------------------------------------------------
Known problems or limitations with current version
----------------------------------------------------------------------
None.
---------------------------END-OF-HEADER------------------------------
*/

/*********************************************************************
*
*       #include Section
*
**********************************************************************
*/
#include "NAND_X_HW.h"
#include "FS_Int.h"       // For FS_MEMCPY
#include <string.h>
/*********************************************************************
*
*       #include Section
*
**********************************************************************
*/
#ifdef __ICCARM__
  #define OPTIMIZE        /* __arm __ramfunc */
#else
  #define OPTIMIZE
#endif


/*********************************************************************
*
*       #define Macros
*
**********************************************************************
*/
#define NAND_BASE_ADDR          0x40000000
#define NAND_DATA               (U8 *)(NAND_BASE_ADDR + 0x000000)
#define NAND_ADDR               (U8 *)(NAND_BASE_ADDR + 0x200000)
#define NAND_CMD                (U8 *)(NAND_BASE_ADDR + 0x400000)

/*********************************************************************
*
*       #define sfrs
*
**********************************************************************
*/

#define SMC0_BASE_ADDR    0xFFFFEC00
#define SMC0_SETUP3       (*(volatile U32*)(SMC0_BASE_ADDR + 3 * 0x10 + 0x00))  // SMC CS3 Setup Register
#define SMC0_PULSE3       (*(volatile U32*)(SMC0_BASE_ADDR + 3 * 0x10 + 0x04))  // SMC CS3 Pulse Register
#define SMC0_CYCLE3       (*(volatile U32*)(SMC0_BASE_ADDR + 3 * 0x10 + 0x08))  // SMC CS3 Cycle Register
#define SMC0_CTRL3        (*(volatile U32*)(SMC0_BASE_ADDR + 3 * 0x10 + 0x0C))  // SMC CS3 Mode Register
/*      MATRIX + EBI interface */
#define MATRIX_BASE_ADDR (0xFFFFEE00)                                 // MATRIX Base Address
#define MATRIX_MCFG      (*(volatile U32*)(MATRIX_BASE_ADDR + 0x00))  // MATRIX Master configuration register
#define MATRIX_EBICSA    (*(volatile U32*)(MATRIX_BASE_ADDR + 0x11C)) // MATRIX EBI Chip Select Assignment register

#define PMC_BASE_ADDR    0xFFFFFC00
#define PMC_PCER         (*(volatile U32 *)(PMC_BASE_ADDR + 0x10)) // (PMC) Peripheral Clock Enable Register
#define PMC_PCDR         (*(volatile U32 *)(PMC_BASE_ADDR + 0x14)) // (PMC) Peripheral Clock Disable Register

// ========== Register definition for PIOA peripheral ==========
#define PIOA_BASE        0xFFFFF400
#define PIOA_PER         (*(volatile U32 *)(PIOA_BASE + 0x00)) // (PIOC) PIO Enable Register
#define PIOA_ODR         (*(volatile U32 *)(PIOA_BASE + 0x14)) // (PIOC) Output Disable Registerr
#define PIOA_IFDR        (*(volatile U32 *)(PIOA_BASE + 0x24)) // (PIOC) Input Filter Disable Register
#define PIOA_IDR         (*(volatile U32 *)(PIOA_BASE + 0x44)) // (PIOC) Interrupt Disable Register
#define PIOA_PPUDR       (*(volatile U32 *)(PIOA_BASE + 0x60)) // (PIOC) Pull-up Disable Register
#define PIOA_PDSR        (*(volatile U32 *)(PIOA_BASE + 0x3C)) // (PIOC) Pin Data Status Register

// ========== Register definition for PIOC peripheral ==========
#define PIOC_BASE        0xFFFFF800
#define PIOC_PER         (*(volatile U32 *)(PIOC_BASE + 0x00)) // (PIOC) PIO Enable Register
#define PIOC_PDR         (*(volatile U32 *)(PIOC_BASE + 0x04)) // (PIOC) PIO Disable Register
#define PIOC_PSR         (*(volatile U32 *)(PIOC_BASE + 0x08)) // (PIOC) PIO Status Register
#define PIOC_OER         (*(volatile U32 *)(PIOC_BASE + 0x10)) // (PIOC) Output Enable Register
#define PIOC_ODR         (*(volatile U32 *)(PIOC_BASE + 0x14)) // (PIOC) Output Disable Registerr
#define PIOC_OSR         (*(volatile U32 *)(PIOC_BASE + 0x18)) // (PIOC) Output Status Register
#define PIOC_IFER        (*(volatile U32 *)(PIOC_BASE + 0x20)) // (PIOC) Input Filter Enable Register
#define PIOC_IFDR        (*(volatile U32 *)(PIOC_BASE + 0x24)) // (PIOC) Input Filter Disable Register
#define PIOC_IFSR        (*(volatile U32 *)(PIOC_BASE + 0x28)) // (PIOC) Input Filter Status Register
#define PIOC_SODR        (*(volatile U32 *)(PIOC_BASE + 0x30)) // (PIOC) Set Output Data Register
#define PIOC_CODR        (*(volatile U32 *)(PIOC_BASE + 0x34)) // (PIOC) Clear Output Data Register
#define PIOC_ODSR        (*(volatile U32 *)(PIOC_BASE + 0x38)) // (PIOC) Output Data Status Register
#define PIOC_PDSR        (*(volatile U32 *)(PIOC_BASE + 0x3C)) // (PIOC) Pin Data Status Register
#define PIOC_IER         (*(volatile U32 *)(PIOC_BASE + 0x40)) // (PIOC) Interrupt Enable Register
#define PIOC_IDR         (*(volatile U32 *)(PIOC_BASE + 0x44)) // (PIOC) Interrupt Disable Register
#define PIOC_IMR         (*(volatile U32 *)(PIOC_BASE + 0x48)) // (PIOC) Interrupt Mask Register
#define PIOC_ISR         (*(volatile U32 *)(PIOC_BASE + 0x4C)) // (PIOC) Interrupt Status Register
#define PIOC_MDER        (*(volatile U32 *)(PIOC_BASE + 0x50)) // (PIOC) Multi-driver Enable Register
#define PIOC_MDDR        (*(volatile U32 *)(PIOC_BASE + 0x54)) // (PIOC) Multi-driver Disable Register
#define PIOC_MDSR        (*(volatile U32 *)(PIOC_BASE + 0x58)) // (PIOC) Multi-driver Status Register
#define PIOC_PPUDR       (*(volatile U32 *)(PIOC_BASE + 0x60)) // (PIOC) Pull-up Disable Register
#define PIOC_PPUER       (*(volatile U32 *)(PIOC_BASE + 0x64)) // (PIOC) Pull-up Enable Register
#define PIOC_PPUSR       (*(volatile U32 *)(PIOC_BASE + 0x68)) // (PIOC) Pull-up Status Register
#define PIOC_ASR         (*(volatile U32 *)(PIOC_BASE + 0x70)) // (PIOC) Select A Register
#define PIOC_BSR         (*(volatile U32 *)(PIOC_BASE + 0x74)) // (PIOC) Select B Register
#define PIOC_ABSR        (*(volatile U32 *)(PIOC_BASE + 0x78)) // (PIOC) AB Select Status Register
#define PIOC_OWER        (*(volatile U32 *)(PIOC_BASE + 0xA0)) // (PIOC) Output Write Enable Register
#define PIOC_OWDR        (*(volatile U32 *)(PIOC_BASE + 0xA4)) // (PIOC) Output Write Disable Register
#define PIOC_OWSR        (*(volatile U32 *)(PIOC_BASE + 0xA8)) // (PIOC) Output Write Status Register

#define PERIPHAL_ID_PIOA        (2)  // Parallel IO Controller A
#define PERIPHAL_ID_PIOC        (4)  // Parallel IO Controller C, D, E

/*********************************************************************
*
*       Static data
*
**********************************************************************
*/
static U8 * _pCurrentNANDAddr;
/*********************************************************************
*
*       Static code
*
**********************************************************************
*/

/*********************************************************************
*
*       Public code
*
**********************************************************************
*/


/*********************************************************************
*
*       FS_NAND_HW_X_EnableCE
*/
OPTIMIZE void FS_NAND_HW_X_EnableCE(U8 Unit) {
  PIOC_CODR = (1 << 14); // Enable NAND CE
}

/*********************************************************************
*
*       FS_NAND_HW_X_DisableCE
*/
OPTIMIZE void FS_NAND_HW_X_DisableCE(U8 Unit) {
  PIOC_SODR = (1 << 14); // Disable NAND CE
}


/*********************************************************************
*
*       FS_NAND_HW_X_SetData
*/
OPTIMIZE void FS_NAND_HW_X_SetDataMode(U8 Unit) {
  FS_USE_PARA(Unit);
  // CLE low, ALE low
  _pCurrentNANDAddr = NAND_DATA;
}


/*********************************************************************
*
*       FS_NAND_HW_X_SetCmd
*/
OPTIMIZE void FS_NAND_HW_X_SetCmdMode(U8 Unit) {
  FS_USE_PARA(Unit);
  //CLE high, ALE low
  _pCurrentNANDAddr = NAND_CMD;
}

/*********************************************************************
*
*       FS_NAND_HW_X_SetAddr
*/
OPTIMIZE void FS_NAND_HW_X_SetAddrMode(U8 Unit) {
  FS_USE_PARA(Unit);
  // CLE low, ALE high
  _pCurrentNANDAddr = NAND_ADDR;
}

/*********************************************************************
*
*       FS_NAND_HW_X_Read_x8
*/
void FS_NAND_HW_X_Read_x8(U8 Unit, void * pData, unsigned NumBytes) {
  FS_MEMCPY(pData, _pCurrentNANDAddr, NumBytes);
}

/*********************************************************************
*
*       FS_NAND_HW_X_Read_x16
*/
void FS_NAND_HW_X_Read_x16(U8 Unit, void * pData, unsigned NumBytes) {
  FS_MEMCPY(pData, _pCurrentNANDAddr, NumBytes);
}

/*********************************************************************
*
*       FS_NAND_HW_X_Write_x8
*/
void FS_NAND_HW_X_Write_x8(U8 Unit, const void * pData, unsigned NumBytes) {
  FS_MEMCPY(_pCurrentNANDAddr, pData, NumBytes);
}

/*********************************************************************
*
*       FS_NAND_HW_X_Write_x16
*/
void FS_NAND_HW_X_Write_x16(U8 Unit, const void * pData, unsigned NumBytes) {
  FS_MEMCPY(_pCurrentNANDAddr, pData, NumBytes);
}

/*********************************************************************
*
*       FS_NAND_HW_X_Init_x8
*/
void FS_NAND_HW_X_Init_x8(U8 Unit) {
  //
  // Update external bus interface, static memory controller
  //

  MATRIX_EBICSA |= (1 << 3); // Assign CS3 for use with NAND flash
  SMC0_SETUP3 = (0x01 <<  0)  // 1 cycle nWE setup length
              | (0x00 <<  8)  // 1 cycle nCSWE setup length for write access
              | (0x01 << 16)  // 1 cycle nRE setup length
              | (0x00 << 24)  // 1 cycle nCSRD setup length for read access
              ;
  SMC0_PULSE3 = (0x05 <<  0)  // 5 cycles nWR pulse length
              | (0x05 <<  8)  // 5 cycles nCS pulse length in write access
              | (0x05 << 16)  // 5 cycles nRD pulse length
              | (0x05 << 24)  // 5 cycles nCS pulse length in read access
              ;
  SMC0_CYCLE3 = (0x08 <<  0)  // 8 cycles for complete write length
              | (0x08 << 16)  // 8 cycles for complete read length
              ;
  SMC0_CTRL3  = (1 <<  0)     // read operation is controlled by nRD
             |  (1 <<  1)     // write operation is controlled by nWE
             |  (0 <<  4)     // nWait is not used
             |  (0 <<  8)     // bytes access is realized by the nBSx signals
             |  (0 << 12)     // DBW: 0: 8-bit NAND, 1: 16-bit NAND
             |  (2 << 16)     // Add 2 cycles for data float time
             |  (0 << 20)     // data float time is not optimized
             |  (0 << 24)     // Page mode is not enabled
             ;
  //
  // Enable clocks for PIOA, PIOD
  //
  PMC_PCER = (1 << PERIPHAL_ID_PIOC);

  //
  //  Set PIOC pin 13 as port pin, for use as NAND Ready/Busy line.  gaoxi modify 13 to 16
  //
  PIOC_ODR  = 1 << 16;    // Configure input
  PIOC_PER  = 1 << 16;    // Set pin as port pin

  //
  //  Set PIOC pin 14 as port pin (output), for use as NAND CS.
  //
  PIOC_SODR = 1 << 14;    // Set pin high
  PIOC_OER  = 1 << 14;    // Configure as output
  PIOC_PER  = 1 << 14;    // Set pin as port pin
}

/*********************************************************************
*
*       FS_NAND_HW_X_Init_x16
*/
void FS_NAND_HW_X_Init_x16(U8 Unit) {
  FS_NAND_HW_X_Init_x8(Unit);
  SMC0_CTRL3  = (1 << 12)     // DBW: 0: 8-bit NAND, 1: 16-bit NAND
              |(3 << 0);     // Use NRD & NWE signals for read / write
}


/*********************************************************************
*
*             FS_NAND_HW_X_WaitTimer
*/

OPTIMIZE void FS_NAND_HW_X_Delayus(unsigned  Period) {
  int Cnt;
  Cnt = Period * 70;
  do {} while (--Cnt);
}

/*********************************************************************
*
*             FS_NAND_HW_X_WaitWhileBusy
*/
OPTIMIZE int FS_NAND_HW_X_WaitWhileBusy(U8 Unit, unsigned us) {
FS_NAND_HW_X_Delayus(100);
//  while ((PIOC_PDSR & (1 << 9)) == 0);
  return 1;
}

/**************************** end of file ***************************/

