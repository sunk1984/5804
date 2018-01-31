/*********************************************************************
*               SEGGER MICROCONTROLLER GmbH & Co KG                  *
*       Solutions for real time microcontroller applications         *
**********************************************************************
*                                                                    *
*       (c) 1995 - 2008  SEGGER Microcontroller GmbH & Co KG         *
*                                                                    *
*       www.segger.com     Support: support@segger.com               *
*                                                                    *
**********************************************************************

----------------------------------------------------------------------
File    : IP_Debug.c
Purpose : Implements the global variables defined in (IP_Int.h)
--------- END-OF-HEADER ----------------------------------------------
*/

#define IPDEBUG_C

#include "IP_Int.h"

/*********************************************************************
*
*       Global data
*
**********************************************************************
*/

// We define additional dummy variables here to get additional structure 
// information for the debugger. This may be required for some debugger
// targets to show additional information
// The variables itself are not referenced and will threrefore
// not be linked into the output file.
//
IP_GLOBAL IP_GlobalDummy;
IP_DEBUG_VARS  IP_DebugVarsDummy;          

/*************************** end of file ****************************/

