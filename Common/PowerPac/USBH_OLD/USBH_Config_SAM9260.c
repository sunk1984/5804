/*********************************************************************
*  
*   IAR PowerPac
*
*   (c) Copyright IAR Systems 2010.  All rights reserved.
*
**********************************************************************
----------------------------------------------------------------------
File        : USBH_Config_SAM9260.c
Purpose     : emUSB Host configuration file for the Atmel AT91SAM9260
---------------------------END-OF-HEADER------------------------------
*/

/*********************************************************************
*
*       #include Section
*
**********************************************************************
*/
#include <stdlib.h>
#include "USBH.h"
#include "BSP.h"

/*********************************************************************
*
*       Defines, configurable
*
**********************************************************************
*/
#define ALLOC_SIZE                 0xA000      // Size of memory dedicated to the stack in bytes
#define OHCI_BASE_ADDRESS          0x00500000

#define ALLOC_BASE                 (((U32)&_aPool[0]) + 0x4000000)                      // Use the non cached SDRAM area
/*********************************************************************
*
*       Static data
*
**********************************************************************
*/
static U32 _aPool[((ALLOC_SIZE + 256) / 4)];             // Memory area used by the stack. add additional 256 bytes in order to have a 256 byte aligned address

/*********************************************************************
*
*       Public code
*
**********************************************************************
*/

/*********************************************************************
*
*       _ISR
*
*  Function description
*/
static void _ISR(void) {
  USBH_ServiceISR(0);
}

/*********************************************************************
*
*       USBH_X_Config
*
*  Function description
*/
void USBH_X_Config(void) {
  USBH_AssignMemory((void *)((ALLOC_BASE + 0xff) & ~0xff), ALLOC_SIZE);    // Assigning memory should be the first thing
//  USBH_AssignTransferMemory((void*)TRANSFER_MEMORY_BASE, TRANSFER_MEMORY_SIZE);
  //
  // Define log and warn filter
  // Note: The terminal I/O emulation affects the timing
  // of your communication, since the debugger stops the target
  // for every terminal I/O output unless you use DCC!
  //
  USBH_SetWarnFilter(0xFFFFFFFF);               // 0xFFFFFFFF: Do not filter: Output all warnings.
  USBH_SetLogFilter(0
                    | USBH_MTYPE_INIT
  //                  | USBH_MTYPE_CORE
  //                  | USBH_MTYPE_DRIVER
  //                  | USBH_MTYPE_MEM
                    | USBH_MTYPE_APPLICATION
                    | USBH_MTYPE_HID
//                    | USBH_MTYPE_MSD
                    );
//  USBH_SetLogFilter(0xffffffff);
  BSP_USBH_Init();
  USBH_OHC_Add((void*)OHCI_BASE_ADDRESS);
  BSP_USBH_InstallISR(_ISR);
}
/********************************* EOF ******************************/
