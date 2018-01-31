/*******************************************************************************
    RTOSInit_AT91SAM9260.c
    Initializes and handles the hardware for the OS as far as required by the OS.
    Feel free to modify this file acc. to your target system.

    Copyright(C) 2010, Honeywell Integrated Technology (China) Co.,Ltd.
    Security FCT team
    All rights reserved.

    IDE:    IAR EWARM V5.5  5505-2002
    ICE:    J-Link V8
    BOARD:  Merak main board V1.0

    History
    2010.07.22  ver.1.00    First release by Taoist
*******************************************************************************/

#include "RTOS.h"

/*********************************************************************
*
*       Configuration
*
**********************************************************************
*/

#define ALLOW_NESTED_INTERRUPTS 0   // Caution: Nesting interrupts will cause higher stack load on system stack CSTACK

/*********************************************************************
*
*       Clock frequency settings
*/
#define OS_FSYS (198656000uL)   // Usage of EMAC limits the AHB frequency to 160 MHz

#ifndef   OS_PCLK_TIMER
  #define OS_PCLK_TIMER (OS_FSYS / 2)
#endif

#ifndef   OS_TICK_FREQ
  #define OS_TICK_FREQ (1000)
#endif

#define OS_TIMER_PRESCALE (16)  // prescaler for system timer is fixed to 16

#define MUL_PLLA     (96)       // Multiplier
#define OUT_PLLA     (0x02)     // High speed setting
#define COUNT_PLLA   (0x3F)     // startup counter
#define DIV_PLLA     (9)        // Divider

#define _PLLAR_VALUE ((1 << 29)                  \
                     |((MUL_PLLA-1) << 16)       \
                     |(OUT_PLLA     << 14)       \
                     |(COUNT_PLLA   << 8)        \
                     |(DIV_PLLA     << 0))

#define MUL_PLLB     (26*2)     // Multiplier
#define OUT_PLLB     (0x00)     // High speed setting
#define COUNT_PLLB   (0x20)     // startup counter
#define DIV_PLLB     (10)       // Divider
#define USB_DIV      (01)       // USB-clock Divider

#define _PLLBR_VALUE ((USB_DIV      << 28)       \
                     |((MUL_PLLB-1) << 16)       \
                     |(OUT_PLLB     << 14)       \
                     |(COUNT_PLLB   << 8)        \
                     |(DIV_PLLB     << 0))

#define MCKR_MDIV    (0x01)     // Main clock is processor clock / 2
#define MCKR_PRES    (0)        // Processor clock is selected clock
#define MCKR_CSS     (0x02)     // PLLA is selected clock

#define _MCKR_VALUE  ((MCKR_MDIV << 8)   \
                     |(MCKR_PRES << 2)   \
                     |(MCKR_CSS  << 0))

/*********************************************************************
*
*       Configuration of communication to OSView
*/
#ifndef   OS_VIEW_ENABLE            // Global enable of communication
  #define OS_VIEW_ENABLE    (1)     // Default: on
#endif

#ifndef   OS_VIEW_USE_UART          // If set, UART will be used
  #define OS_VIEW_USE_UART  (0)     // Default: 0 => DCC is used
#endif                              // if OS_VIEW_ENABLE is on

/*********************************************************************
*
*       UART settings for OSView
*       If you do not want (or can not due to hardware limitations)
*       to dedicate a UART to OSView, please define it to be -1
*       Currently UART0 and UART1 are supported and the standard
*       setup enables UART 0 per default
*       When using DCC for communiction, the UART is not used for embOSView,
*       regardless the OS_UART settings
*/
#ifndef   OS_UART
  #define OS_UART (0)
#endif

#ifndef   OS_PCLK_UART
  #define OS_PCLK_UART (OS_FSYS / 2)
#endif

#ifndef   OS_BAUDRATE
  #define OS_BAUDRATE (115200)
#endif

/****** End of configuration settings *******************************/

/*********************************************************************
*
*       DCC and UART settings for OSView
*
*       Automatically generated from configuration settings
*/
#define OS_USE_DCC     (OS_VIEW_ENABLE && (OS_VIEW_USE_UART == 0))

#define OS_UART_USED   (OS_VIEW_ENABLE && (OS_VIEW_USE_UART != 0)) && ((OS_UART == 0) || (OS_UART == 1))

/********************************************************************/

#if (DEBUG || OS_USE_DCC)
  #include "JLINKDCC.h"
#endif

/*********************************************************************
*
*       Local defines (sfrs used in RTOSInit.c)
*
**********************************************************************
*/

/*      USART, used for OSView communication */
#define _USART0_BASE_ADDR  (0xFFFB0000)
#define _USART1_BASE_ADDR  (0xFFFB4000)
#define _USART2_BASE_ADDR  (0xFFFB8000)

/*      Debug unit */
#define _DBGU_BASE_ADDR    (0xFFFFF200)
#define _DBGU_IMR     (*(volatile OS_U32*) (_DBGU_BASE_ADDR + 0x10)) /* Interrupt Mask Register */
#define _DBGU_SR      (*(volatile OS_U32*) (_DBGU_BASE_ADDR + 0x14)) /* Channel Status Register */
#define DBGU_COMMRX   (1 << 31)
#define DBGU_COMMTX   (1 << 30)
#define DBGU_RXBUFF   (1 << 12)
#define DBGU_TXBUFE   (1 << 11)
#define DBGU_TXEMPTY  (1 <<  9)
#define DBGU_PARE     (1 <<  7)
#define DBGU_FRAME    (1 <<  6)
#define DBGU_OVRE     (1 <<  5)
#define DBGU_ENDTX    (1 <<  4)
#define DBGU_ENDRX    (1 <<  3)
#define DBGU_TXRDY    (1 <<  1)
#define DBGU_RXRDY    (1 <<  0)
#define DBGU_MASK_ALL (DBGU_COMMRX | DBGU_COMMTX  | DBGU_RXBUFF |  \
                       DBGU_TXBUFE | DBGU_TXEMPTY | DBGU_PARE   |  \
                       DBGU_FRAME  | DBGU_OVRE    | DBGU_ENDTX  |  \
                       DBGU_ENDRX  | DBGU_TXRDY   | DBGU_RXRDY)

/*      Reset controller */
#define _RSTC_BASE_ADDR    (0xFFFFFD00)
#define _RSTC_CR      (*(volatile OS_U32*) (_RSTC_BASE_ADDR + 0x00))
#define _RSTC_SR      (*(volatile OS_U32*) (_RSTC_BASE_ADDR + 0x04))
#define _RSTC_MR      (*(volatile OS_U32*) (_RSTC_BASE_ADDR + 0x08))
#define _RSTC_URSTEN  (1 <<  0)  /* User reset enable           */
#define RSTC_BODIEN   (1 << 16)  /* Brownout interrupt enable   */
#define RSTC_URSTIEN  (1 <<  4)  /* User reset interrupt enable */
#define RSTC_BODSTS   (1 <<  1)  /* Brownout status             */
#define RSTC_URSTS    (1 <<  0)  /* User reset status           */

/*      Real time timer */
#define _RTT_BASE_ADDR     (0xFFFFFD20)
#define _RTT_MR       (*(volatile OS_U32*) (_RTT_BASE_ADDR + 0x00))
#define _RTT_SR       (*(volatile OS_U32*) (_RTT_BASE_ADDR + 0x0C))
#define RTT_RTTINCIEN (1 << 17)
#define RTT_ALMIEN    (1 << 16)
#define RTT_RTTINC    (1 << 1)
#define RTT_ALMS      (1 << 0)

/*      Periodic interval timer */
#define _PIT_BASE_ADDR     (0xFFFFFD30)
#define _PIT_MR       (*(volatile OS_U32*) (_PIT_BASE_ADDR + 0x00))
#define _PIT_SR       (*(volatile OS_U32*) (_PIT_BASE_ADDR + 0x04))
#define _PIT_PIVR     (*(volatile OS_U32*) (_PIT_BASE_ADDR + 0x08))
#define _PIT_PIIR     (*(volatile OS_U32*) (_PIT_BASE_ADDR + 0x0C))

/*      Watchdog */
#define _WDT_BASE_ADDR     (0xFFFFFD40)
#define _WDT_CR       (*(volatile OS_U32*) (_WDT_BASE_ADDR + 0x00))
#define _WDT_MR       (*(volatile OS_U32*) (_WDT_BASE_ADDR + 0x04))
#define _WDT_SR       (*(volatile OS_U32*) (_WDT_BASE_ADDR + 0x08))
#define WDT_WDFIEN    (1 << 12) /* Watchdog interrupt enable flag in mode register */
#define WDT_WDERR     (1 <<  1) /* Watchdog error status flag                      */
#define WDT_WDUNF     (1 <<  0) /* Watchdog underflow status flag                  */

/*      PIO control register */
#define _PIOA_BASE_ADDR    (0xfffff400)
#define _PIOB_BASE_ADDR    (0xfffff600)
#define _PIOC_BASE_ADDR    (0xfffff800)

/*      Power management controller */
#define _PMC_BASE_ADDR     (0xFFFFFC00)

#define _PMC_CKGR_PLLAR (*(volatile OS_U32*) (_PMC_BASE_ADDR + 0x28))  /* PLLA register */
#define _PMC_CKGR_PLLBR (*(volatile OS_U32*) (_PMC_BASE_ADDR + 0x2c))  /* PLLB register */
#define _PMC_LOCKA    (1 <<  1)
#define _PMC_LOCKB    (1 <<  2)

#define _PMC_SCDR     (*(volatile OS_U32*) (_PMC_BASE_ADDR + 0x04))  /* System Clock Disable Register */
#define _PMC_PCER     (*(volatile OS_U32*) (_PMC_BASE_ADDR + 0x10))  /* Peripheral clock enable register */
#define _PMC_MOR      (*(volatile OS_U32*) (_PMC_BASE_ADDR + 0x20))  /* main oscillator register */
#define _PMC_PLLR     (*(volatile OS_U32*) (_PMC_BASE_ADDR + 0x2c))  /* PLL register */
#define _PMC_MCKR     (*(volatile OS_U32*) (_PMC_BASE_ADDR + 0x30))  /* Master clock register */
#define _PMC_SR       (*(volatile OS_U32*) (_PMC_BASE_ADDR + 0x68))  /* status register */
#define _PMC_IMR      (*(volatile OS_U32*) (_PMC_BASE_ADDR + 0x6C))  /* interrupt mask register */
#define _PMC_PCKRDY2   (1 << 10)
#define _PMC_PCKRDY1   (1 <<  9)
#define _PMC_PCKRDY0   (1 <<  8)
#define _PMC_MCKRDY    (1 <<  3)
#define _PMC_LOCK      (1 <<  2)
#define _PMC_MOSCS     (1 <<  0)
#define _PMC_MASK_ALL  (_PMC_PCKRDY2 | _PMC_PCKRDY1 | _PMC_PCKRDY0 | \
                        _PMC_MCKRDY  | _PMC_LOCK    | _PMC_MOSCS)

/*      MATRIX + EBI interface */
#define _MATRIX_BASE_ADDR   (0xFFFFEE00)                                 // MATRIX Base Address

#define _MATRIX_MCFG   (*(volatile OS_U32*) (_MATRIX_BASE_ADDR + 0x00))  // MATRIX Master configuration register
#define _MATRIX_EBICSA (*(volatile OS_U32*) (_MATRIX_BASE_ADDR + 0x11C)) // MATRIX EBI Chip Select Assignment register

/*      PIOC, used as data BUS */
#define _PIOC_PDR      (*(volatile OS_U32*) (_PIOC_BASE_ADDR + 0x04))    // PIOC disable register
#define _PIOC_MDDR     (*(volatile OS_U32*) (_PIOC_BASE_ADDR + 0x54))    // PIOC multi driver disable register
#define _PIOC_ASR      (*(volatile OS_U32*) (_PIOC_BASE_ADDR + 0x70))    // PIOC peripheral A select register

/*      SDRAM controller */
#define _SDRAMC_BASE_ADDR  (0xFFFFEA00)   // SDRAMC Base Address
#define _SDRAMC_MR     (*(volatile OS_U32*) (_SDRAMC_BASE_ADDR + 0x00)) // (SDRAMC) SDRAM Controller Mode Register
#define _SDRAMC_TR     (*(volatile OS_U32*) (_SDRAMC_BASE_ADDR + 0x04)) // (SDRAMC) SDRAM Controller Refresh timer Register
#define _SDRAMC_CR     (*(volatile OS_U32*) (_SDRAMC_BASE_ADDR + 0x08)) // (SDRAMC) SDRAM Controller Configuration Register
#define _SDRAMC_LPR    (*(volatile OS_U32*) (_SDRAMC_BASE_ADDR + 0x10)) // (SDRAMC) SDRAM Controller Low Power Register
#define _SDRAMC_MDR    (*(volatile OS_U32*) (_SDRAMC_BASE_ADDR + 0x24)) // (SDRAMC) SDRAM Controller Memory Device Register

#define _SDRAMC_MODE_NORMAL_CMD   (0x0) // (SDRAMC) Normal Mode
#define _SDRAMC_MODE_NOP_CMD      (0x1) // (SDRAMC) Issue a All Banks Precharge Command at every access
#define _SDRAMC_MODE_PRCGALL_CMD  (0x2) // (SDRAMC) Issue a All Banks Precharge Command at every access
#define _SDRAMC_MODE_LMR_CMD      (0x3) // (SDRAMC) Issue a Load Mode Register at every access
#define _SDRAMC_MODE_RFSH_CMD     (0x4) // (SDRAMC) Issue a Refresh

#define SDRAM_BASE_ADDR   (0x20000000)

/*      Advanced interrupt controller (AIC) */
#define _AIC_BASE_ADDR      (0xfffff000)
#define _AIC_SMR_BASE_ADDR  (_AIC_BASE_ADDR + 0x00)
#define _AIC_SVR_BASE_ADDR  (_AIC_BASE_ADDR + 0x80)
#define _AIC_SVR0      (*(volatile OS_U32*) (_AIC_SVR_BASE_ADDR + 0x00))
#define _AIC_SVR1      (*(volatile OS_U32*) (_AIC_SVR_BASE_ADDR + 0x04))
#define _AIC_IVR       (*(volatile OS_U32*) (_AIC_BASE_ADDR + 0x100))
#define _AIC_ISR       (*(volatile OS_U32*) (_AIC_BASE_ADDR + 0x108))
#define _AIC_IPR       (*(volatile OS_U32*) (_AIC_BASE_ADDR + 0x10c))
#define _AIC_IDCR      (*(volatile OS_U32*) (_AIC_BASE_ADDR + 0x124))
#define _AIC_ICCR      (*(volatile OS_U32*) (_AIC_BASE_ADDR + 0x128))
#define _AIC_IECR      (*(volatile OS_U32*) (_AIC_BASE_ADDR + 0x120))
#define _AIC_EOICR     (*(volatile OS_U32*) (_AIC_BASE_ADDR + 0x130))
#define _AIC_SPU       (*(volatile OS_U32*) (_AIC_BASE_ADDR + 0x134))
#define _AIC_DCR       (*(volatile OS_U32*) (_AIC_BASE_ADDR + 0x138))
#define _AIC_FFDR      (*(volatile OS_U32*) (_AIC_BASE_ADDR + 0x144))

/*      AIC interrupt sources and peripheral IDs        */
#define _SYSTEM_IRQ_ID  (1)   /* System IRQ ID             */
#define _US0IRQ_ID      (6)   /* USART Channel 0 interrupt */
#define _US1IRQ_ID      (7)   /* USART Channel 1 interrupt */
#define _US2IRQ_ID      (8)   /* USART Channel 2 interrupt */

#ifndef   _NUM_INT_SOURCES
  #define _NUM_INT_SOURCES   (32)
#endif

#define _INT_PRIORITY_MASK (0x07)
#define _NUM_INT_PRIORITIES   (8)

/*********************************************************************
*
*       Static data
*
**********************************************************************
*/

/*********************************************************************
*
*       Local functions
*
**********************************************************************
*/

/*********************************************************************
*
*       _HandlePmcIrq(), Power management controller interrupt
*       If not used for application, this handler may be removed
*/
static void _HandlePmcIrq(void) {
#if DEBUG
  OS_U32 IrqSource;
  IrqSource  = _PMC_IMR;
  IrqSource &= (_PMC_SR & _PMC_MASK_ALL);
  if (IrqSource) {  /* PMC interrupt pending? */
    while(1);       /* Not implemented        */
  }
#endif
}

/*********************************************************************
*
*       _HandleRttIrq(), Real time timer interrupt handler
*       If not used for application, this handler may be removed
*/
static void _HandleRttIrq(void) {
#if DEBUG
  OS_U32 IrqStatus;
  OS_U32 IrqEnabled;

  IrqEnabled = _RTT_MR & (RTT_RTTINCIEN  | RTT_ALMIEN);
  IrqStatus  = _RTT_SR & (RTT_RTTINC | RTT_ALMS);
  if ((IrqStatus & RTT_RTTINC) && (IrqEnabled & RTT_RTTINCIEN )) { /* RTT inc. interrupt pending ? */
    while(1);                /* Not implemented */
  }
  if ((IrqStatus & RTT_ALMS) && (IrqEnabled & RTT_ALMIEN )) {      /* Alarm interrupt pending ? */
    while(1);                /* Not implemented */
  }
#endif
}

/*********************************************************************
*
*       _HandleDbguIrq(), Debug unit interrupt handler
*       If not used for application, this handler may be removed
*/
static void _HandleDbguIrq(void) {
//#if DEBUG
  OS_U32 IrqSource;

  IrqSource  = _DBGU_IMR;
  IrqSource &= (_DBGU_SR & DBGU_MASK_ALL);
  if (IrqSource) { /* Any interrupt pending ? */
    USDBGU_ISR_Handler();
//    while(1);      /* Not implemented         */
  }
//#endif
}

/*********************************************************************
*
*       _HandleRstcIrq(), Reset controller interrupt handler
*       If not used for application, this handler may be removed
*/
static void _HandleRstcIrq(void) {
#if DEBUG
  OS_U32 IrqStatus;
  OS_U32 IrqEnabled;

  IrqEnabled = _RSTC_MR & (RSTC_BODIEN | RSTC_URSTIEN);
  IrqStatus  = _RSTC_SR & (RSTC_BODSTS | RSTC_URSTS);
  if ((IrqStatus & RSTC_BODSTS) && (IrqEnabled & RSTC_BODIEN )) {  /* Brownout interrupt pending ?   */
    while(1);                /* Not implemented */
  }
  if ((IrqStatus & RSTC_URSTS) && (IrqEnabled & RSTC_URSTIEN )) {  /* User reset interrupt pending ? */
    while(1);                /* Not implemented */
  }
#endif
}

/*********************************************************************
*
*       _HandleWdtIrq(), watchdog timer interrupt handler
*       If not used for application, this handler may be removed
*/
static void _HandleWdtIrq(void) {
#if DEBUG
  OS_U32 IrqStatus;

  IrqStatus = _WDT_SR & (WDT_WDERR | WDT_WDUNF);
  if (IrqStatus && (_WDT_MR & WDT_WDFIEN)) { /* Watchdog error interrupt pending ? */
    while(1);                                /* Not implemented */
  }
#endif
}

/*********************************************************************
*
*       _DefaultFiqHandler(), a dummy FIQ handler
*/
static void _DefaultFiqHandler(void) {
  while(1);
}

/*********************************************************************
*
*       _DefaultIrqHandler, a dummy IRQ handler
*
*       This handler is initially written into all AIC interrupt vectors
*       It is called, if no interrupt vector was installed for
*       specific interrupt source.
*       May be used during debugging to detect uninstalled interrupts
*/
static void _DefaultIrqHandler(void) {
  OS_U32 IrqSource;
  IrqSource = _AIC_ISR;  /* detect source of uninstalled interrupt */
  while(IrqSource == _AIC_ISR);
}

/*********************************************************************
*
*       _SpuriousIrqHandler(), a dummy spurious IRQ handler
*/
static volatile OS_U32 _SpuriousIrqCnt;
static void _SpuriousIrqHandler(void) {
  _SpuriousIrqCnt++;
}

/*********************************************************************
*
*       _OS_SystemIrqhandler()
*       the the OS system interrupt, handles OS timer
*/
static void _OS_SystemIrqhandler(void) {
  volatile int Dummy;

  if (_PIT_SR & (1 << 0)) {  /* Timer interupt pending?            */
    Dummy = _PIT_PIVR;       /* Reset interrupt pending condition  */
    OS_HandleTick();         /* Call OS tick handler            */
#if (DEBUG || OS_USE_DCC)
    DCC_Process();
#endif
  }
  /* Call to following handlers may be removed if not used by application */
  _HandlePmcIrq();
  _HandleRttIrq();
  _HandleDbguIrq();
  _HandleRstcIrq();
  _HandleWdtIrq();
}

/*********************************************************************
*
*       _InitAIC()
*
*       Initialize interupt controller by setting default vectors
*       and clearing all interrupts
*
* NOTES: (1) This function may be called from __low_level_init() and therefore
*            must not use or call any function which relies on any variable,
*            because variables are not initialized before __low_level_init()
*            is called !
*/
static void _InitAIC(void) {
  int  i;
  OS_ISR_HANDLER** papISR;

  _AIC_IDCR = 0xFFFFFFFF;                     /* Disable all interrupts     */
  _AIC_ICCR = 0xFFFFFFFF;                     /* Clear all interrupts       */
  _AIC_FFDR = 0xFFFFFFFF;                     /* Reset fast forcingts       */
  _AIC_SVR0 = (int) _DefaultFiqHandler;       /* dummy FIQ handler          */
  _AIC_SPU  = (int) _SpuriousIrqHandler ;     /* dummy spurious handler     */
  papISR = (OS_ISR_HANDLER**) _AIC_SVR_BASE_ADDR;
  for (i = 1; i < _NUM_INT_SOURCES; i++)  {   /* initially set all sources  */
    *(papISR + i) = &_DefaultIrqHandler;      /* to dummy irq handler       */
  }
  for (i = 0; i < _NUM_INT_PRIORITIES; i++) {
    _AIC_EOICR = 0;                           /* Reset interrupt controller */
  }
#if DEBUG            // For debugging activate AIC protected mode
  _AIC_DCR |= 0x01;  // Enable AIC protected mode
#endif
}


/*********************************************************************
*
*       _InitRTT()
*
* Function description
*   Initialize Real time timer.
*
* NOTES: (1) Not used by embOS, we disable interrupts here to avoid unhandled interrupts
*            May be modified by user if RTT is required for application.
*/
static void _InitRTT(void) {
  //
  // Disable Real-Time Timer interrupts
  //
  _RTT_MR &= ~ (RTT_RTTINCIEN  | RTT_ALMIEN);
}


/*********************************************************************
*
*       Global functions
*
**********************************************************************
*/

/*********************************************************************
*
*       OS_InitHW()
*
*       Initialize the hardware (timer) required for the OS to run.
*       May be modified, if an other timer should be used
*/
#define OS_TIMER_RELOAD ((OS_PCLK_TIMER/OS_TIMER_PRESCALE/OS_TICK_FREQ) - 1)
#if (OS_TIMER_RELOAD >= 0x00100000)
  #error "PIT timer can not be used, please check configuration"
#endif

void OS_InitHW(void) {
  OS_IncDI();
  //OS_ARM_CACHE_Sync();          // Ensure, caches are synchronized
  /* Initialize PIT as OS timer, enable timer + timer interrupt */
  _PIT_MR = ((OS_TIMER_RELOAD & 0x000FFFFF) | (1 << 25) | (1 << 24));
  OS_ARM_InstallISRHandler(_SYSTEM_IRQ_ID, _OS_SystemIrqhandler);
  OS_ARM_EnableISR(_SYSTEM_IRQ_ID);
  OS_DecRI();
}

/*********************************************************************
*
*       (OS_Idle)
*
*       Please note:
*       This is basically the "core" of the idle loop.
*       This core loop can be changed, but:
*       The idle loop does not have a stack of its own, therefore no
*       functionality should be implemented that relies on the stack
*       to be preserved. However, a simple program loop can be programmed
*       (like toggeling an output or incrementing a counter)
*/
void OS_Idle(void) {  /* Idle loop: No task is ready to exec */
  while (1) {
#if DEBUG == 0
    _PMC_SCDR = 1;    // Switch off CPU clock to save power
#endif
  }
}

/*********************************************************************
*
*       OS_GetTime_Cycles()
*
*       This routine is required for task-info via OSView or high
*       resolution time maesurement functions.
*       It returns the system time in timer clock cycles.
*/
OS_U32 OS_GetTime_Cycles(void) {
  unsigned int t_cnt;
  OS_U32 time ;

  t_cnt = _PIT_PIIR;           /* Read current timer value   */
  time  = OS_GetTime32();      /* Read current OS time    */
  if (t_cnt & 0xFFF00000) {    /* Timer Interrupt pending ?  */
    time  += (t_cnt >> 20);    /* Adjust result              */
    t_cnt &= 0x000FFFFF;
  }
  return (OS_TIMER_RELOAD * time) + t_cnt;
}

/*********************************************************************
*
*       OS_ConvertCycles2us()
*
*       Convert Cycles into micro seconds.
*
*       If your clock frequency is not a multiple of 1 MHz,
*       you may have to modify this routine in order to get proper
*       diagonstics.
*
*       This routine is required for profiling or high resolution time
*       measurement only. It does not affect operation of the OS.
*/
OS_U32 OS_ConvertCycles2us(OS_U32 Cycles) {
  return Cycles/(OS_PCLK_TIMER/OS_TIMER_PRESCALE/1000000);
}

/****** Final check of configuration ********************************/

#ifndef OS_UART_USED
  #error "OS_UART_USED has to be defined"
#endif

/*********************************************************************
*
*       OS interrupt handler and ISR specific functions
*
**********************************************************************
*/

/*********************************************************************
*
*       OS_irq_handler
*
*       Detect reason for IRQ and call correspondig service routine.
*       OS_irq_handler is called from OS_IRQ_SERVICE function
*       found in RTOSVect.asm
*/
OS_INTERWORK void OS_irq_handler(void) {
  OS_ISR_HANDLER* pISR;

  pISR = (OS_ISR_HANDLER*) _AIC_IVR;   // Read interrupt vector to release NIRQ to CPU core
#if DEBUG
  _AIC_IVR = (OS_U32) pISR;            // Write back any value to IVR register to allow interrupt stacking in protected mode
#endif
#if ALLOW_NESTED_INTERRUPTS
  OS_EnterNestableInterrupt();         // Now interrupts may be reenabled. If nesting should be allowed
#else
  OS_EnterInterrupt();                 // Inform OS that interrupt handler is running
#endif
  pISR();                              // Call interrupt service routine
  OS_DI();                             // Disable interrupts and unlock
  _AIC_EOICR = 0;                      // interrupt controller =>  Restore previous priority
#if ALLOW_NESTED_INTERRUPTS
  OS_LeaveNestableInterrupt();         // Leave nestable interrupt, perform task switch if required
#else
  OS_LeaveInterrupt();                 // Leave interrupt, perform task switch if required
#endif
}

/*********************************************************************
*
*       OS_ARM_InstallISRHandler
*/
OS_ISR_HANDLER* OS_ARM_InstallISRHandler (int ISRIndex, OS_ISR_HANDLER* pISRHandler) {
  OS_ISR_HANDLER*  pOldHandler;
  OS_ISR_HANDLER** papISR;

#if DEBUG
  if ((unsigned)ISRIndex >= _NUM_INT_SOURCES) {
    OS_Error(OS_ERR_ISR_INDEX);
    return NULL;
  }
#endif
  OS_DI();
  papISR = (OS_ISR_HANDLER**)_AIC_SVR_BASE_ADDR;
  pOldHandler          = *(papISR + ISRIndex);
  *(papISR + ISRIndex) = pISRHandler;
  OS_RestoreI();
  return pOldHandler;
}

/*********************************************************************
*
*       OS_ARM_EnableISR
*/
void OS_ARM_EnableISR(int ISRIndex) {
#if DEBUG
  if ((unsigned)ISRIndex >= _NUM_INT_SOURCES) {
    OS_Error(OS_ERR_ISR_INDEX);
    return;
  }
#endif

  OS_DI();
  _AIC_IECR = (1 << ISRIndex);
  OS_RestoreI();
}

/*********************************************************************
*
*       OS_ARM_DisableISR
*/
void OS_ARM_DisableISR(int ISRIndex) {
#if DEBUG
  if ((unsigned)ISRIndex >= _NUM_INT_SOURCES) {
    OS_Error(OS_ERR_ISR_INDEX);
    return;
  }
#endif

  OS_DI();
  _AIC_IDCR = (1 << ISRIndex);
  OS_RestoreI();
}

/*********************************************************************
*
*       OS_ARM_ISRSetPrio
*/
int OS_ARM_ISRSetPrio(int ISRIndex, int Prio) {
  OS_U32* pPrio;
  int     OldPrio;

#if DEBUG
  if ((unsigned)ISRIndex >= _NUM_INT_SOURCES) {
    OS_Error(OS_ERR_ISR_INDEX);
    return 0;
  }
#endif
  OS_DI();
  pPrio = (OS_U32*)_AIC_SMR_BASE_ADDR;
  OldPrio = pPrio[ISRIndex];
  pPrio[ISRIndex] = (OldPrio & ~_INT_PRIORITY_MASK) | (Prio & _INT_PRIORITY_MASK);
  OS_RestoreI();
  return OldPrio & _INT_PRIORITY_MASK;
}


/*********************************************************************
*
*       __low_level_init()
*
*       Initialize memory controller, clock generation and pll
*
*       Has to be modified, if another CPU clock frequency should be used.
*       This function is called during startup and
*       has to return 1 to perform segment initialization
*       Because __low_level_init() is called before segment initialization,
*       no access that relies on any variable can be performed.
*/
OS_INTERWORK int __low_level_init(void) {
  _WDT_MR = (1 << 15);                    // Initially disable watchdog

  //
  // Init interrupt controller
  //
  _InitAIC();
  //
  // Initialize real time timer
  //
  _InitRTT();
  //
  // Perform other initialization here, if required
  //
  _RSTC_MR = ((0xA5 << 24) | _RSTC_URSTEN);  // write KEY and URSTEN to allow USER RESET
  return 1;
}

/*****  EOF  ********************************************************/

