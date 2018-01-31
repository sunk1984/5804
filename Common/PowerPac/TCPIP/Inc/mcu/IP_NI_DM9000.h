/*********************************************************************
*               SEGGER MICROCONTROLLER SYSTEME GmbH                  *
*       Solutions for real time microcontroller applications         *
**********************************************************************
*                                                                    *
*       (C) 2003 - 2010   SEGGER Microcontroller Systeme GmbH        *
*                                                                    *
*       www.segger.com     Support: support@segger.com               *
*                                                                    *
**********************************************************************
*                                                                    *
*       TCP/IP stack for embedded applications                       *
*                                                                    *
**********************************************************************
----------------------------------------------------------------------
File    : IP_NI_DM9000.h
Purpose : Driver specific header file for the Davicom DM9000
--------  END-OF-HEADER  ---------------------------------------------
*/

#ifndef IP_NI_DM9000_H
#define IP_NI_DM9000_H

#if defined(__cplusplus)
extern "C" {  // Make sure we have C-declarations in C++ programs
#endif

/*********************************************************************
*
*       Public data
*
**********************************************************************
*/
extern const IP_HW_DRIVER IP_Driver_DM9000;

/*********************************************************************
*
*       Public code
*
**********************************************************************
*/
typedef struct IP_NI_DM9000_ACCESS  IP_NI_DM9000_ACCESS;
typedef struct DM9000_INST DM9000_INST;

struct DM9000_INST {
  U8 *  pHardware;
  U8 *  pValue;
  U8    BusWidth;    // 8, 16 or 32 bits
  IP_NI_DM9000_ACCESS* pAccess;
};

struct IP_NI_DM9000_ACCESS {
  void (*pf_WriteReg8)    (DM9000_INST * pInst, unsigned RegIndex,  unsigned  val);
  U16  (*pf_ReadReg8)     (DM9000_INST * pInst, unsigned RegIndex);
  void (*pf_ReadData)     (DM9000_INST * pInst, U8*   pDest, U32 NumBytes);
  void (*pf_WriteData)    (DM9000_INST * pInst, void* pPacket, U32 NumBytes);
};

void IP_NI_DM9000_ISR_Handler (unsigned Unit);
void IP_NI_DM9000_ConfigAddr  (unsigned Unit, void* pBase, void* pValue);
void IP_NI_DM9000_ConfigAccess(unsigned Unit, IP_NI_DM9000_ACCESS * pAccess);

#if defined(__cplusplus)
  }           // Make sure we have C-declarations in C++ programs
#endif

/********************************************************************/

#endif        // Avoid multiple inclusion

/*************************** End of file ****************************/



