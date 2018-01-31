/*==========================================================================*\
|                                                                            |
| Devices430.h                                                               |
|                                                                            |
| Device Function Prototypes and Definitions for FLASH programming.          |
|                                                                            |
*/
/****************************************************************************/
/* Defines                                                                  */
/****************************************************************************/

#ifndef _DEVICES_H_
#define _DEVICES_H_

#ifndef __BYTEWORD__
#define __BYTEWORD__
typedef unsigned short word;
typedef unsigned char byte;
#endif

typedef unsigned char  bool;

// Constants for flash erasing modes
//! \brief Constant for flash erase: main & info of ALL      mem arrays
#define ERASE_GLOB                 0xA50E
//! \brief Constant for flash erase: main        of ALL      mem arrays
#define ERASE_ALLMAIN              0xA50C
//! \brief Constant for flash erase: main & info of SELECTED mem arrays
#define ERASE_MASS                 0xA506
//! \brief Constant for flash erase: main        of SELECTED mem arrays
#define ERASE_MAIN                 0xA504
//! \brief Constant for flash erase: SELECTED segment
#define ERASE_SGMT                 0xA502

/****************************************************************************/
/* Function prototypes                                                      */
/****************************************************************************/

void SetDevice(word wDeviceId);
bool DeviceHas_TestPin(void);
bool DeviceHas_CpuX(void);
bool DeviceHas_DataQuick(void);
bool DeviceHas_FastFlash(void);
bool DeviceHas_EnhVerify(void);
bool DeviceHas_JTAG(void);
bool DeviceHas_SpyBiWire(void);
word Device_RamStart(void);
word Device_RamEnd(void);
word Device_MainStart(void);

#endif
