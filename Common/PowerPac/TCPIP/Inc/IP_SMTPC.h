/*********************************************************************
*                SEGGER MICROCONTROLLER SYSTEME GmbH                 *
*        Solutions for real time microcontroller applications        *
**********************************************************************
*                                                                    *
*        (c) 1996 - 2004  SEGGER Microcontroller Systeme GmbH        *
*                                                                    *
*        Internet: www.segger.com    Support:  support@segger.com    *
*                                                                    *
**********************************************************************

----------------------------------------------------------------------
File        : IP_SMTPC.h
Purpose     : Publics for the SMTP client
---------------------------END-OF-HEADER------------------------------

Attention : Do not modify this file !
*/

#ifndef  IP_SMTPCLIENT_H
#define  IP_SMTPCLIENT_H

#include "SMTPC_Conf.h"
#include "SEGGER.h"

#if defined(__cplusplus)
extern "C" {     /* Make sure we have C-declarations in C++ programs */
#endif

/*********************************************************************
*
*       defines
*
**********************************************************************
*/

#ifndef   SMTPC_OUT_BUFFER_SIZE
  #define SMTPC_OUT_BUFFER_SIZE   256
#endif

#define SMTPC_REC_TYPE_TO         1
#define SMTPC_REC_TYPE_CC         2
#define SMTPC_REC_TYPE_BCC        3
#define SMTPC_REC_TYPE_FROM       4

/*********************************************************************
*
*       Types
*
**********************************************************************
*/
typedef struct IP_SMTPC_MTA {
  const char * sServer;   // Address of the SMTP server
  const char * sUser;     // User name used for the authentication
  const char * sPass;     // Password used for the authentication
} IP_SMTPC_MTA;

typedef struct IP_SMTPC_MAIL_ADDR {
  const char * sName;
  const char * sAddr;
  int Type;               // SMTPC_REC_TYPE_TO, SMTPC_REC_TYPE_CC, SMTPC_REC_TYPE_BCC
} IP_SMTPC_MAIL_ADDR;

typedef struct IP_SMTPC_MESSAGE {
  const char      * sSubject;
  const char      * sBody;
  int               MessageSize;
} IP_SMTPC_MESSAGE;

typedef struct IP_SMTPC_APPLICATION {
  U32 (*pfGetTimeDate) (void);
  int (*pfCallback)(int Stat, void *p);
  const char * sDomain;   // email domain
  const char * sTimezone; // Time zone. The zone specifies the offset from Coordinated Universal Time (UTC). Can be null.
} IP_SMTPC_APPLICATION;


typedef void * SMTPC_SOCKET;

typedef struct IP_SMTPC_API {
  SMTPC_SOCKET (*pfConnect)    (char * SrvAddr);
  void         (*pfDisconnect) (SMTPC_SOCKET Socket);
  int          (*pfSend)       (const char * pData, int Len, SMTPC_SOCKET Socket);
  int          (*pfReceive)    (char * pData, int Len, SMTPC_SOCKET Socket);
} IP_SMTPC_API;




/*********************************************************************
*
*       Functions
*
**********************************************************************
*/
int IP_SMTPC_Send(const IP_SMTPC_API * pIP_API, IP_SMTPC_MAIL_ADDR * paMailAddr, int NumMailAddr, IP_SMTPC_MESSAGE * pMessage, const IP_SMTPC_MTA * pMTA, const IP_SMTPC_APPLICATION * pApplication);

#if defined(__cplusplus)
  }
#endif


#endif   /* Avoid multiple inclusion */

/*************************** End of file ****************************/




