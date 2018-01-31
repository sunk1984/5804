/*********************************************************************
*  
*   IAR PowerPac
*
*   (c) Copyright IAR Systems 2010.  All rights reserved.
*
**********************************************************************
----------------------------------------------------------------------
File    : SMTPC_Conf.h
Purpose : SMTP client configuration file for Win32 simulation
---------------------------END-OF-HEADER------------------------------
*/

#ifndef _SMTPC_CONF_H_
#define _SMTPC_CONF_H_ 1

/*********************************************************************
*
*       Defines, configurable
*
**********************************************************************
*/
#define SMTPC_SERVER_PORT             25

#define SMTPC_IN_BUFFER_SIZE         256
#define SMTPC_AUTH_USER_BUFFER_SIZE   48
#define SMTPC_AUTH_PASS_BUFFER_SIZE   48

//
// Logging
//
#ifdef DEBUG
  #include "IP.h"
  #define SMTPC_WARN(p)              IP_Logf_Application p
  #define SMTPC_LOG(p)               IP_Logf_Application p
#else
  #define SMTPC_WARN(p)
  #define SMTPC_LOG(p)
#endif

#ifndef   IP_MEMSET
  #define IP_MEMSET      memset
#endif

#endif     // Avoid multiple inclusion


/*************************** End of file ****************************/
