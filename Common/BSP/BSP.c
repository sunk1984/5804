/*********************************************************************
*  
*   IAR PowerPac
*
*   (c) Copyright IAR Systems 2010.  All rights reserved.
*
**********************************************************************
----------------------------------------------------------------------
File    : BSP.c
Purpose : BSP for AT91SAM9260-EK and AT91SAM9XE-EK
--------  END-OF-HEADER  ---------------------------------------------
*/

#include "includes.h"

void BSP_USBH_Init(void)
{
    AT91C_BASE_PMC->PMC_PCER = (1 << USBH_ID);
    AT91C_BASE_PMC->PMC_SCER = (1 << 6);    // Enable clock for USB host controller
}

void BSP_USBH_InstallISR(void (*pfISR)(void))
{
    OS_ARM_InstallISRHandler(USBH_ID, pfISR);
    OS_ARM_EnableISR(USBH_ID);
}
/*********************************************************************
*
*       BSP_ETH_Init()
*
*  Function description
*    This function is called from the network interface driver.
*    It initializes the network interface. This function should be used
*    to enable the ports which are connected to the network hardware.
*    It is called from the driver during the initialization process.
*
*  Note:
*    (1) If your MAC is connected to the PHY via Media Independent
*        Interface (MII) change the macro _USE_RMII and call
*        IP_NI_ConfigPHYMode() from within IP_X_Config()
*        to change the default of driver.
*
*/
void BSP_ETH_Init(unsigned Unit)
{
    AT91C_BASE_PMC->PMC_PCER |= (1 << AT91C_ID_EMAC); /* Enable peripheral clock for LED-Port    */
    //
    // Initialize priority of BUS MATRIX. EMAC needs highest priority for SDRAM access
    //
    MATRIX_SCFG3 = 0x01160030;     // Assign EMAC as default master, activate priority arbitration, increase cycles
    MATRIX_PRAS3 = 0x00320000;     // Set Priority of EMAC to 3 (highest value)
}

/*********************************************************************
*
*       BSP_CACHE_InvalidateRange()
*/
void BSP_CACHE_InvalidateRange(void * p, unsigned NumBytes){}


