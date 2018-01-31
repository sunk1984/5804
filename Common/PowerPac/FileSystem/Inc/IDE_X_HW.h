/*********************************************************************
*                SEGGER MICROCONTROLLER GmbH & Co. KG                *
*        Solutions for real time microcontroller applications        *
**********************************************************************
*                                                                    *
*        (c) 2003-2007     SEGGER Microcontroller GmbH & Co KG       *
*                                                                    *
*        Internet: www.segger.com    Support:  support@segger.com    *
*                                                                    *
**********************************************************************

**** emFile file system for embedded applications ****
emFile is protected by international copyright laws. Knowledge of the
source code may not be used to write a similar product. This file may
only be used in accordance with a license and should not be re-
distributed in any way. We appreciate your understanding and fairness.
----------------------------------------------------------------------
File        : IDE_X_HW.h
Purpose     : IDE hardware layer
----------------------------------------------------------------------
Known problems or limitations with current version
----------------------------------------------------------------------
None.
---------------------------END-OF-HEADER------------------------------
*/

#ifndef __IDE_X_HW_H__
#define __IDE_X_HW_H__

#include "SEGGER.h"

/*********************************************************************
*
*             Global function prototypes
*
**********************************************************************
*/

/* Control line functions */
void FS_IDE_HW_Reset     (U8 Unit);
int  FS_IDE_HW_IsPresent (U8 Unit);
void FS_IDE_HW_Delay400ns(U8 Unit);

U16  FS_IDE_HW_ReadReg  (U8 Unit, unsigned AddrOff);
void FS_IDE_HW_ReadData (U8 Unit,       U8 * pData, unsigned NumBytes);
void FS_IDE_HW_WriteData(U8 Unit, const U8 * pData, unsigned NumBytes);
void FS_IDE_HW_WriteReg (U8 Unit, unsigned AddrOff, U16 Data);
/* Status detection functions */

#endif  /* __IDE_X_HW_H__ */

/*************************** End of file ****************************/
