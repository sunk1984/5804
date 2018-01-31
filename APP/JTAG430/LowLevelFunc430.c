/*==========================================================================*\
|                                                                            |
| LowLevelFunc430.c                                                          |
|                                                                            |
| Low Level Functions regarding user's hardware                              |
|----------------------------------------------------------------------------|
*/
/****************************************************************************/
/* INCLUDES                                                                 */
/****************************************************************************/

#include "LowLevelFunc430.h"
#include "includes.h"

//! \brief JTAG macro: clear TMS signal
void ClrTMS(void)
{
    JTAG_GPIO->PIO_CODR = TMS_PIN;
}

//! \brief JTAG macro: set TMS signal
void SetTMS(void)
{
    JTAG_GPIO->PIO_SODR = TMS_PIN;
}

//! \brief JTAG macro: clear TDI signal
void ClrTDI(void)
{
    JTAG_GPIO->PIO_CODR = TDI_PIN;
}

//! \brief JTAG macro: set TDI signal
void SetTDI(void)
{
    JTAG_GPIO->PIO_SODR = TDI_PIN;
}

//! \brief JTAG macro: clear TCK signal
void ClrTCK(void)
{
    JTAG_GPIO->PIO_CODR = TCK_PIN;
}

//! \brief JTAG macro: set TCK signal
void SetTCK(void)
{
    JTAG_GPIO->PIO_SODR = TCK_PIN;
}

//! \brief JTAG macro: set RST signal
void SetRST(void)
{
    JTAG_GPIO->PIO_SODR = RST_PIN;
}

//! \brief JTAG macro: clear RST signal
void ClrRST(void)
{
    JTAG_GPIO->PIO_CODR = RST_PIN;
}

//! \brief JTAG macro: set TST signal
void SetTST(void)
{
    JTAG_GPIO->PIO_SODR = TST_PIN;
}

//! \brief JTAG macro: clear TST signal
void ClrTST(void)
{
    JTAG_GPIO->PIO_CODR = TST_PIN;
}

//! \brief clear the TCLK signal
void ClrTCLK(void)
{
    JTAG_GPIO->PIO_CODR = TCLK_PIN;
}

//! \brief set the TCLK signal
void SetTCLK(void)
{
    JTAG_GPIO->PIO_SODR = TCLK_PIN;
}

//! \brief JTAG macro: return current TCLK signal (on TDI pin)
int StoreTCLK(void)
{
    return(JTAG_GPIO->PIO_PDSR &  TCLK_PIN);
}

//! \brief JTAG macro: restore TCLK signal on TDI pin (based on input: x)
void RestoreTCLK(int x)
{
    x == 0 ? (JTAG_GPIO->PIO_CODR = TCLK_PIN) : (JTAG_GPIO->PIO_SODR = TCLK_PIN);
}

//! \brief JTAG macro: return TDO value (result 0 or TDO (0x40))
int ScanTDO(void)
{
    return(JTAG_GPIO->PIO_PDSR &  TDO_PIN);
}


//----------------------------------------------------------------------------
//! \brief Set up I/O pins for JTAG communication
void DriveSignals(void)
{
    JTAG_GPIO->PIO_OER = TCK_PIN; 
    JTAG_GPIO->PIO_SODR = TCK_PIN;
    
    JTAG_GPIO->PIO_OER = TDI_PIN; 
    JTAG_GPIO->PIO_SODR = TDI_PIN;

    JTAG_GPIO->PIO_OER = TMS_PIN; 
    JTAG_GPIO->PIO_SODR = TMS_PIN;

    JTAG_GPIO->PIO_OER = TST_PIN; 
    JTAG_GPIO->PIO_SODR = TST_PIN;

    JTAG_GPIO->PIO_OER = RST_PIN; 
    JTAG_GPIO->PIO_SODR = RST_PIN;

    JTAG_GPIO->PIO_ODR = TDO_PIN; 
}

//----------------------------------------------------------------------------
//! \brief Release I/O pins
void ReleaseSignals(void)
{
    JTAG_GPIO->PIO_ODR = TCK_PIN; 
    JTAG_GPIO->PIO_ODR = TDI_PIN; 
    JTAG_GPIO->PIO_ODR = TMS_PIN; 
    JTAG_GPIO->PIO_ODR = TDO_PIN; 
    JTAG_GPIO->PIO_ODR = TST_PIN; 
    JTAG_GPIO->PIO_ODR = RST_PIN; 
}

//----------------------------------------------------------------------------
//! \brief Initialization of the Target Board (switch voltages on, preset JTAG 
//! pins)
//! \details For devices with normal 4wires JTAG  (JTAG4SBW=0)\n
//! For devices with Spy-Bi-Wire to work in 4wires JTAG (JTAG4SBW=1)
//! \brief Initialization of the Timer
void Init_JTAG(void)
{
    AT91C_BASE_PMC->PMC_PCER |= 1 << JTAG_GPIO_ID;

    JTAG_GPIO->PIO_PER = TCK_PIN; 
    JTAG_GPIO->PIO_PER = TDI_PIN; 
    JTAG_GPIO->PIO_PER = TMS_PIN; 
    JTAG_GPIO->PIO_PER = TST_PIN; 
    JTAG_GPIO->PIO_PER = RST_PIN; 
    JTAG_GPIO->PIO_PER = TDO_PIN; 

  	//Create a timer for function uDelay()
    AT91C_BASE_PMC->PMC_PCER |= 1 << JTAG_TC_ID;    // Open timer5
    
    JTAG_TC_BASE->TC_CCR = AT91C_TC_CLKDIS ;    // Disable the clock and the interrupts
    JTAG_TC_BASE->TC_IDR = 0xFFFFFFFF ;

    JTAG_TC_BASE->TC_SR;        // Clear status bit
    
    JTAG_TC_BASE->TC_CMR = TC_CLKS_MCK2 | AT91C_TC_CPCTRG ; // Set the Mode of the Timer Counter

    JTAG_TC_BASE->TC_CCR = AT91C_TC_CLKEN;  // Enable the clock

    JTAG_TC_BASE->TC_SR;
}

//----------------------------------------------------------------------------
//! \brief Delay function (resolution is 1 ms)
//! \param[in] milliseconds (number of ms, max number is 0xFFFF)
void MsDelay(U16 milliseconds)
{
    OS_Delay(milliseconds);
}

void udelay(U16 microseconds)
{
    JTAG_TC_BASE->TC_RC = microseconds * TIMER_PERIOD_1US;  // Set compare value.
    JTAG_TC_BASE->TC_SR;
    JTAG_TC_BASE->TC_CCR = AT91C_TC_SWTRG;      // // Start timer5  
    JTAG_TC_BASE->TC_SR;
    while((JTAG_TC_BASE->TC_SR & AT91C_TC_CPCS) == 0)
    {
    	;
    }
}

/****************************************************************************/
/*                         END OF SOURCE FILE                               */
/****************************************************************************/
