/*********************************************************************
*  
*   IAR PowerPac
*
*   (c) Copyright IAR Systems 2010.  All rights reserved.
*
**********************************************************************
--------  END-OF-HEADER  ---------------------------------------------
*/

#ifndef OS_CONFIG_H                     /* Avoid multiple inclusion */
#define OS_CONFIG_H

#ifndef   DEBUG
  #define DEBUG 0
#endif

/*********************************************************************
*
*       Configuration for RTOS build and UART
*
*  One of the following builds needs to be selected for both DEBUG and Release builds:
*
*  OS_LIBMODE_XR   Extremly small release build without Round robin
*  OS_LIBMODE_R     Release build
*  OS_LIBMODE_S     Release build with stack check
*  OS_LIBMODE_D     Debug build
*/

#if DEBUG
  #define OS_LIBMODE_D
  #define OS_UART        -1
  #define OS_VIEW_ENABLE  0
#else
  #define OS_LIBMODE_R
  #define OS_UART        -1
  #define OS_VIEW_ENABLE  0
#endif

#endif                                  /* Avoid multiple inclusion */

/*****  EOF  ********************************************************/
