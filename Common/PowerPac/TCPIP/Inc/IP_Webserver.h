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
File        : IP_WebServer.h
Purpose     : Publics for the WebServer
---------------------------END-OF-HEADER------------------------------

Attention : Do not modify this file !

Note:
  TimeDate is a 32 bit variable in the following format:
    Bit 0-4:   2-second count (0-29)
    Bit 5-10:  Minutes (0-59)
    Bit 11-15: Hours (0-23)
    Bit 16-20: Day of month (1-31)
    Bit 21-24: Month of year (1-12)
    Bit 25-31: Count of years from 1980 (0-127)
*/

#ifndef  IP_WEBS_H
#define  IP_WEBS_H

#include "IP_FS.h"
#include "WEBS_Conf.h"

/*********************************************************************
*
*       Defaults for config values
*
**********************************************************************
*/
#ifndef   WEBS_IN_BUFFER_SIZE
  #define WEBS_IN_BUFFER_SIZE       512
#endif

#ifndef   WEBS_OUT_BUFFER_SIZE
  #define WEBS_OUT_BUFFER_SIZE      512
#endif

#ifndef   WEBS_TEMP_BUFFER_SIZE
  #define WEBS_TEMP_BUFFER_SIZE     256         // Used as file input buffer and for form parameters
#endif

#ifndef   WEBS_ERR_BUFFER_SIZE
  #define WEBS_ERR_BUFFER_SIZE      128         // Used in case of connection limit only
#endif

#ifndef   WEBS_AUTH_BUFFER_SIZE
  #define WEBS_AUTH_BUFFER_SIZE      32
#endif

#ifndef   WEBS_FILENAME_BUFFER_SIZE
  #define WEBS_FILENAME_BUFFER_SIZE  32
#endif

#define WEBS_STACK_SIZE_CHILD   (1400 + WEBS_IN_BUFFER_SIZE + WEBS_OUT_BUFFER_SIZE + WEBS_TEMP_BUFFER_SIZE + WEBS_PARA_BUFFER_SIZE)    // This size can not be guaranteed on all systems. Actual size depends on CPU & compiler

#if defined(__cplusplus)
extern "C" {     /* Make sure we have C-declarations in C++ programs */
#endif

/*********************************************************************
*
*       Types
*
**********************************************************************
*/

typedef int (*IP_WEBS_tSend)   (const unsigned char * pData, int len, void* pConnectInfo);
typedef int (*IP_WEBS_tReceive)(      unsigned char * pData, int len, void* pConnectInfo);

typedef void* WEBS_OUTPUT;
typedef struct {
  const char * sName;   // e.g. "Counter"
  void  (*pf)(WEBS_OUTPUT * pOutput, const char * sParameters, const char * sValue);
} WEBS_CGI;

typedef struct {
  const char * sName;   // e.g. "Counter.cgi"
  void  (*pf)(WEBS_OUTPUT * pOutput, const char * sParameters);
} WEBS_VFILES;

typedef struct {
  const char * sPath;
  const char * sRealm;
  const char * sUserPass;
} WEBS_ACCESS_CONTROL;

typedef struct {
  const WEBS_CGI      * paCGI;
  WEBS_ACCESS_CONTROL * paAccess;
  void  (*pfHandleParameter)(WEBS_OUTPUT * pOutput, const char * sPara, const char * sValue);
  const WEBS_VFILES   * paVFiles;
} WEBS_APPLICATION;


typedef struct {
  U32 DateLastMod;        // Used for "Last modified" header field
  U32 DateExp;            // Used for "Expires" header field
  U8  IsVirtual;
  U8  AllowDynContent;
} IP_WEBS_FILE_INFO;

typedef void (*IP_WEBS_pfGetFileInfo)(const char * sFilename, IP_WEBS_FILE_INFO * pFileInfo);



/*********************************************************************
*
*       Functions
*
**********************************************************************
*/

extern const IP_FS_API IP_FS_ReadOnly;

int  IP_WEBS_Process           (IP_WEBS_tSend pfSend, IP_WEBS_tReceive pfReceive, void* pConnectInfo, const IP_FS_API * pFS_API, const WEBS_APPLICATION * pApplication);
int  IP_WEBS_ProcessLast       (IP_WEBS_tSend pfSend, IP_WEBS_tReceive pfReceive, void* pConnectInfo, const IP_FS_API * pFS_API, const WEBS_APPLICATION * pApplication);
void IP_WEBS_OnConnectionLimit (IP_WEBS_tSend pfSend, IP_WEBS_tReceive pfReceive, void* pConnectInfo);

int  IP_WEBS_SendMem      (WEBS_OUTPUT * pOutput, const char * s, unsigned NumBytes);
int  IP_WEBS_SendString   (WEBS_OUTPUT * pOutput, const char * s);
int  IP_WEBS_SendStringEnc(WEBS_OUTPUT * pOutput, const char * s);
int  IP_WEBS_SendUnsigned(WEBS_OUTPUT * pOutput, unsigned long v, unsigned Base, int NumDigits);

int  IP_WEBS_GetNumParas (const char * sParameters);
int  IP_WEBS_GetParaValue(const char * sBuffer, int ParaNum, char * sPara, int ParaLen, char * sValue, int ValueLen);
void IP_WEBS_DecodeAndCopyStr(char * pDest, int DestLen, char * pSrc, int SrcLen);
int  IP_WEBS_DecodeString(const char * s);
void IP_WEBS_SetFileInfoCallback(IP_WEBS_pfGetFileInfo pf);
char IP_WEBS_CompareFilenameExt(const char * sFilename, const char * sExt);

#if defined(__cplusplus)
  }
#endif


#endif   /* Avoid multiple inclusion */

/*************************** End of file ****************************/




