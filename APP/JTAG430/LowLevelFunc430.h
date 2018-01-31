/*==========================================================================*\
|                                                                            |
| LowLevelFunc430.h                                                          |
|                                                                            |
| Low Level function prototypes, macros, and pin-to-signal assignments       |
| regarding to user's hardware                                               |
*/
/****************************************************************************/
/* DEFINES & CONSTANTS                                                      */
/****************************************************************************/

#include "includes.h"

//! \brief JTAG interface

#ifndef __DATAFORMATS__
#define __DATAFORMATS__
#define F_BYTE                     8
#define F_WORD                     16
#endif

// Constants for runoff status
//! \brief return 0 = error
#define STATUS_ERROR     0      // false
//! \brief return 1 = no error
#define STATUS_OK        1      // true
//! \brief GetDevice returns this if the security fuse is blown
#define STATUS_FUSEBLOWN 2

//! \brief Replicator is active
#define STATUS_ACTIVE    2
//! \brief Replicator is idling
#define STATUS_IDLE      3

/****************************************************************************/
/* Macros for processing the JTAG port                                      */
/****************************************************************************/

#define JTAG_GPIO  AT91C_BASE_PIOB

#define RST_PIN     AT91C_PIO_PB10
#define TST_PIN     AT91C_PIO_PB11    
#define TDO_PIN     AT91C_PIO_PB24
#define TDI_PIN     AT91C_PIO_PB20
#define TMS_PIN     AT91C_PIO_PB25
#define TCK_PIN     AT91C_PIO_PB21    

#define TCLK_PIN    TDI_PIN

//以下是TC使用的变量
#define TC_CLKS_MCK2             0x0
#define TC_CLKS_MCK8             0x1
#define TC_CLKS_MCK32            0x2
#define TC_CLKS_MCK128           0x3

#define TIMER_PERIOD_1US         50    // TC_CLKS_MCK2, 100MHz 

#define JTAG_TC_BASE        AT91C_BASE_TC1
#define JTAG_TC_ID          AT91C_ID_TC1
#define JTAG_GPIO_ID        AT91C_ID_PIOB

/*
//! \brief JTAG macro: clear TMS signal
#define ClrTMS()    (JTAG_GPIO->PIO_CODR = TMS_PIN)
//! \brief JTAG macro: set TMS signal
#define SetTMS()    (JTAG_GPIO->PIO_SODR = TMS_PIN)
//! \brief JTAG macro: clear TDI signal
#define ClrTDI()    (JTAG_GPIO->PIO_CODR = TDI_PIN)
//! \brief JTAG macro: set TDI signal
#define SetTDI()    (JTAG_GPIO->PIO_SODR = TDI_PIN)
//! \brief JTAG macro: clear TCK signal
#define ClrTCK()    (JTAG_GPIO->PIO_CODR = TCK_PIN)
//! \brief JTAG macro: set TCK signal
#define SetTCK()    (JTAG_GPIO->PIO_SODR = TCK_PIN)
//! \brief JTAG macro: return current TCLK signal (on TDI pin)
#define StoreTCLK() (JTAG_GPIO->PIO_PDSR &  TCLK_PIN)
//! \brief JTAG macro: restore TCLK signal on TDI pin (based on input: x)
#define RestoreTCLK(x)  (x == 0 ? (JTAG_GPIO->PIO_CODR = TCLK_PIN) : (JTAG_GPIO->PIO_SODR = TCLK_PIN))
//! \brief JTAG macro: return TDO value (result 0 or TDO (0x40))
#define ScanTDO()   (JTAG_GPIO->PIO_PDSR &  TDO_PIN)
//! \brief JTAG macro: set RST signal
#define SetRST()    (JTAG_GPIO->PIO_SODR = RST_PIN)
//! \brief JTAG macro: clear RST signal
#define ClrRST()    (JTAG_GPIO->PIO_CODR = RST_PIN)
//! \brief JTAG macro: set TST signal
#define SetTST()    (JTAG_GPIO->PIO_SODR = TST_PIN)
//! \brief JTAG macro: clear TST signal
#define ClrTST()    (JTAG_GPIO->PIO_CODR = TST_PIN)
//! \brief clear the TCLK signal
#define ClrTCLK()  (JTAG_GPIO->PIO_CODR = TCLK_PIN)
//! \brief set the TCLK signal
#define SetTCLK()  (JTAG_GPIO->PIO_SODR = TCLK_PIN)
*/

//! \brief JTAG macro: clear TMS signal
extern void ClrTMS(void);
//! \brief JTAG macro: set TMS signal
extern void SetTMS(void);
//! \brief JTAG macro: clear TDI signal
extern void ClrTDI(void);
//! \brief JTAG macro: set TDI signal
extern void SetTDI(void);
//! \brief JTAG macro: clear TCK signal
extern void ClrTCK(void);
//! \brief JTAG macro: set TCK signal
extern void SetTCK(void);
//! \brief JTAG macro: set RST signal
extern void SetRST(void);
//! \brief JTAG macro: clear RST signal
extern void ClrRST(void);
//! \brief JTAG macro: set TST signal
extern void SetTST(void);
//! \brief JTAG macro: clear TST signal
extern void ClrTST(void);
//! \brief clear the TCLK signal
extern void ClrTCLK(void);
//! \brief set the TCLK signal
extern void SetTCLK(void);
//! \brief JTAG macro: return current TCLK signal (on TDI pin)
extern int StoreTCLK(void);
//! \brief JTAG macro: restore TCLK signal on TDI pin (based on input: x)
extern void RestoreTCLK(int x);
//! \brief JTAG macro: return TDO value (result 0 or TDO (0x40))
extern int ScanTDO(void);


/****************************************************************************/
/* FUNCTION PROTOTYPES                                                      */
/****************************************************************************/

void    DriveSignals( void );
void    ReleaseSignals(void);
void    Init_JTAG(void);
void    MsDelay(U16 milliseconds);      // millisecond delay loop, uses Timer_A
void    udelay(U16 microseconds);

