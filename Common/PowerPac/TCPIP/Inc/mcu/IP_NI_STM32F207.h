/*********************************************************************
*  
*   IAR PowerPac
*
*   (c) Copyright IAR Systems 2010.  All rights reserved.
*
**********************************************************************
----------------------------------------------------------------------
File    : IP_NI_STM32F207.h
Purpose : Driver specific header file for the ST STM32F207 (Barracuda)
--------  END-OF-HEADER  ---------------------------------------------
*/

#ifndef IP_NI_STM32F207_H
#define IP_NI_STM32F207_H

#if defined(__cplusplus)
extern "C" {     /* Make sure we have C-declarations in C++ programs */
#endif

/*********************************************************************
*
*       Public data
*
**********************************************************************
*/
extern const IP_HW_DRIVER IP_Driver_STM32F207;

/*********************************************************************
*
*       Public code
*
**********************************************************************
*/
void IP_NI_STM32F207_ConfigNumRxBuffers(U16 NumRxBuffers);

#if defined(__cplusplus)
  }    // Make sure we have C-declarations in C++ programs
#endif

/********************************************************************/

#endif // Avoid multiple inclusion

/*************************** End of file ****************************/



