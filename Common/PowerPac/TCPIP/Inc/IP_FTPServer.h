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
File        : IP_FTPServer.h
Purpose     : Publics for the FTP server
---------------------------END-OF-HEADER------------------------------

Attention : Do not modify this file !
*/

#ifndef  IP_FTPS_H
#define  IP_FTPS_H

#include "SEGGER.h"
#include "IP_FS.h"

#if defined(__cplusplus)
extern "C" {     /* Make sure we have C-declarations in C++ programs */
#endif

/*********************************************************************
*
*       defines
*
**********************************************************************
*/
#define IP_FS_ATTRIB_DIR (1 << 0)

#define IP_FTPS_PERM_VISIBLE  (1 << 0)
#define IP_FTPS_PERM_READ     (1 << 1)
#define IP_FTPS_PERM_WRITE    (1 << 2)

/*********************************************************************
*
*       Types
*
**********************************************************************
*/
typedef void* FTPS_OUTPUT;

typedef struct {
  int (*pfFindUser)   (const char * sUser);
  int (*pfCheckPass)  (int UserId, const char * sPass);
  int (*pfGetDirInfo) (int UserId, const char * sDirIn, char * sDirOut, int SizeOfDirOut);
} FTPS_ACCESS_CONTROL;

typedef struct {
  FTPS_ACCESS_CONTROL * pAccess;
  U32 (*pfGetTimeDate) (void);
} FTPS_APPLICATION;

/*********************************************************************
*
*       Functions
*
**********************************************************************
*/

typedef void * FTPS_SOCKET;

typedef struct {
  int         (*pfSend)       (const unsigned char * pData, int len, FTPS_SOCKET Socket);
  int         (*pfReceive)    (unsigned char * pData, int len, FTPS_SOCKET Socket);
  FTPS_SOCKET (*pfConnect)    (FTPS_SOCKET CtrlSocket, U16 Port);
  void        (*pfDisconnect) (FTPS_SOCKET DataSocket);
  FTPS_SOCKET (*pfListen)     (FTPS_SOCKET CtrlSocket, U16 * pPort, U8 * pIPAddr);
  int         (*pfAccept)     (FTPS_SOCKET CtrlSocket, FTPS_SOCKET * pDataSocket);
} IP_FTPS_API;


int  IP_FTPS_Process           (const IP_FTPS_API * pIP_API, void * pConnectInfo, const IP_FS_API * pFS_API, const FTPS_APPLICATION * pApplication);
void IP_FTPS_OnConnectionLimit (const IP_FTPS_API * pIP_API, void * pConnectInfo);

#if defined(__cplusplus)
  }
#endif


#endif   /* Avoid multiple inclusion */

/*************************** End of file ****************************/




