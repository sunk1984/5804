/*********************************************************************
*              SEGGER MICROCONTROLLER SYSTEME GmbH                   *
*        Solutions for real time microcontroller applications        *
**********************************************************************
*                                                                    *
*        (c) 1996-2005 SEGGER Microcontroller Systeme GmbH           *
*                                                                    *
* Internet: www.segger.com Support: support@segger.com               *
*                                                                    *
**********************************************************************
----------------------------------------------------------------------
File    : IP_Webserver.c
Purpose : Webserver add-on.

Literature
  RFC 1123
  RFC 2616


RFC 822
     5.  DATE AND TIME SPECIFICATION
     5.1.  SYNTAX
     date-time   =  [ day "," ] date time        ; dd mm yy
                                                 ;  hh:mm:ss zzz
     day         =  "Mon"  / "Tue" /  "Wed"  / "Thu"
                 /  "Fri"  / "Sat" /  "Sun"
     date        =  1*2DIGIT month 2DIGIT        ; day month year
                                                 ;  e.g. 20 Jun 82
     month       =  "Jan"  /  "Feb" /  "Mar"  /  "Apr"
                 /  "May"  /  "Jun" /  "Jul"  /  "Aug"
                 /  "Sep"  /  "Oct" /  "Nov"  /  "Dec"
     time        =  hour zone                    ; ANSI and Military
     hour        =  2DIGIT ":" 2DIGIT [":" 2DIGIT]
                                                 ; 00:00:00 - 23:59:59
     zone        =  "UT"  / "GMT"                ; Universal Time
                                                 ; North American : UT
                 /  "EST" / "EDT"                ;  Eastern:  - 5/ - 4
                 /  "CST" / "CDT"                ;  Central:  - 6/ - 5
                 /  "MST" / "MDT"                ;  Mountain: - 7/ - 6
                 /  "PST" / "PDT"                ;  Pacific:  - 8/ - 7
                 /  1ALPHA                       ; Military: Z = UT;
                                                 ;  A:-1; (J not used)
                                                 ;  M:-12; N:+1; Y:+12
                 / ( ("+" / "-") 4DIGIT )        ; Local differential
                                                 ;  hours+min. (HHMM)

RFC 1123
      5.2.14  RFC-822 Date and Time Specification: RFC-822 Section 5
         The syntax for the date is hereby changed to:
            date = 1*2DIGIT month 2*4DIGIT
         All mail software SHOULD use 4-digit years in dates, to ease
         the transition to the next century.



RFC 2616
14.18 Date
   The Date general-header field represents the date and time at which
   the message was originated, having the same semantics as orig-date in
   RFC 822. The field value is an HTTP-date, as described in section
   3.3.1; it MUST be sent in RFC 1123 [8]-date format.

       Date  = "Date" ":" HTTP-date

   An example is

       Date: Tue, 15 Nov 1994 08:12:31 GMT

   Origin servers MUST include a Date header field in all responses,
   except in these cases:
     ...

14.18.1 Clockless Origin Server Operation
   Some origin server implementations might not have a clock available.
   An origin server without a clock MUST NOT assign Expires or Last-
   Modified values to a response, unless these values were associated
   with the resource by a system or user with a reliable clock. It MAY
   assign an Expires value that is known, at or before server
   configuration time, to be in the past (this allows "pre-expiration"
   of responses without storing separate Expires values for each
   resource).



14.21 Expires
   The Expires entity-header field gives the date/time after which the
   response is considered stale. A stale cache entry may not normally be
   returned by a cache (either a proxy cache or a user agent cache)
   unless it is first validated with the origin server (or with an
   intermediate cache that has a fresh copy of the entity). See section
   13.2 for further discussion of the expiration model.

   The presence of an Expires field does not imply that the original
   resource will change or cease to exist at, before, or after that
   time.

   The format is an absolute date and time as defined by HTTP-date in
   section 3.3.1; it MUST be in RFC 1123 date format:

      Expires = "Expires" ":" HTTP-date

   An example of its use is

      Expires: Thu, 01 Dec 1994 16:00:00 GMT






---------------------------END-OF-HEADER------------------------------
*/

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "IP_WebServer.h"
#include "IP_UTIL.h"
#include "SEGGER.h"
#include "WEBS_Conf.h"

/*********************************************************************
*
*       defines, configurable
*
**********************************************************************
*/
#ifndef   WEBS_WARN
  #define WEBS_WARN(p)
#endif

#ifndef   WEBS_LOG
  #define WEBS_LOG(p)
#endif

#ifndef   WEBS_CGI_START_STRING
  #define WEBS_CGI_START_STRING "<!--#exec cgi=\""
#endif

#ifndef   WEBS_CGI_END_STRING
  #define WEBS_CGI_END_STRING "\"-->"
#endif

#ifndef   WEBS_401_PAGE
  #define WEBS_401_PAGE                                                                    \
                         "<HTML>\n"                                                        \
                         "<HEAD><TITLE>401 Unauthorized</TITLE></HEAD>\n"                  \
                         "<BODY>\n"                                                        \
                         "<H1>401 Unauthorized</H1>\n"                                     \
                         "Browser not authentication-capable or authentication failed.\n"  \
                         "</BODY>\n"                                                       \
                         "</HTML>\n"
#endif

#ifndef   WEBS_404_PAGE
  #define WEBS_404_PAGE                                                                    \
                         "<HTML>\n"                                                        \
                         "<HEAD><TITLE>404 Not Found</TITLE></HEAD>\n"                     \
                         "<BODY>\n"                                                        \
                         "<H1>Not Found</H1>\n"                                            \
                         "The requested document was not found on this server.\n"          \
                         "</BODY>\n"                                                       \
                         "</HTML>\n"
#endif

#ifndef   WEBS_501_PAGE
  #define WEBS_501_PAGE                                                                    \
                         "<HTML>\n"                                                        \
                         "<HEAD><TITLE>501 Not implemented</TITLE></HEAD>\n"               \
                         "<BODY>\n"                                                        \
                         "<H1>501 Command (method) is not implemented</H1>\n"              \
                         "</BODY>\n"                                                       \
                         "</HTML>\n"
#endif

#ifndef   WEBS_503_PAGE
  #define WEBS_503_PAGE                                                                    \
                         "<HTML>\n"                                                        \
                         "<HEAD><TITLE>503 Connection limit reached</TITLE></HEAD>\n"      \
                         "<BODY>\n"                                                        \
                         "<H1>503 Connection limit reached</H1>\n"                             \
                         "The max. number of simulateous connections to this server reached.\n<P>"  \
                         "Please try again later.\n"                                       \
                         "</BODY>\n"                                                       \
                         "</HTML>\n"
#endif

/*********************************************************************
*
*       defines & enums, fixed
*
**********************************************************************
*/

/*********************************************************************
*
*       ASCII control characters
*/
#define TAB   0x9
#define LF    0xa
#define CR    0xd
#define SPACE 0x20

enum {
  ENCODING_RAW,
  ENCODING_CHUNKED
};

#define CGI_START_LEN (sizeof(WEBS_CGI_START_STRING)-1)
#define CGI_END_LEN   (sizeof(WEBS_CGI_END_STRING)-1)

enum {
  METHOD_GET,
  METHOD_HEAD,
  METHOD_POST
};

/*********************************************************************
*
*       Types
*
**********************************************************************
*/
typedef struct {
  IP_WEBS_tSend pfSend;
  IP_WEBS_tReceive pfReceive;
  void* pConnectInfo;
  U8    HTTPVer;
  U8    CloseWhenDone;
  U8    HasError;
  IP_WEBS_FILE_INFO FileInfo;
} CONNECTION_CONTEXT;

typedef struct {
  CONNECTION_CONTEXT * pConnection;
  U8 * pBuffer;
  int Size;
  int Cnt;
  int RdOff;
} IN_BUFFER_DESC;

typedef struct {
  CONNECTION_CONTEXT * pConnection;
  U8   * pBuffer;
#if WEBS_PARA_BUFFER_SIZE
  char * pPara;
#endif
  int BufferSize;
  int Cnt;
  int Encoding;
} OUT_BUFFER_CONTEXT;

typedef struct {
  CONNECTION_CONTEXT       Connection;  // Connection info
  const IP_FS_API        * pFS_API;     // File system info
  const WEBS_APPLICATION * pApplication;
  IN_BUFFER_DESC           InBufferDesc;
  OUT_BUFFER_CONTEXT       OutBuffer;
  char acUserPass[WEBS_AUTH_BUFFER_SIZE];
} WEBS_CONTEXT;

/*********************************************************************
*
*       static data
*
**********************************************************************
*/

static IP_WEBS_pfGetFileInfo _pfGetFileInfo;

/*********************************************************************
*
*       static const
*
**********************************************************************
*/
static const char _aV2C[16] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' };
const char * _asMonth[12] = { "JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC" };







/*********************************************************************
*
*       static Code
*
**********************************************************************
*/

/*********************************************************************
*
*       _IsWhite
*/
static int _IsWhite(char c) {
  if (c == SPACE) {
    return 1;
  }
  if (c == TAB) {
    return 1;
  }
  return 0;
}

/*********************************************************************
*
*       _StoreUnsigned
*/
static char * _StoreUnsigned(char * pDest, U32 v, unsigned Base, int NumDigits) {
  unsigned Div;
  U32 Digit;

  Digit = 1;
  //
  // Count how many digits are required
  //
  while (((v / Digit) >= Base) | (NumDigits-- > 1)) {
    Digit *= Base;
  }
  //
  // Output digits
  //
  do {
    Div = v / Digit;
    v  -= Div * Digit;
    *pDest++ = _aV2C[Div];
    Digit /= Base;
  } while (Digit);
  return pDest;
}

/*********************************************************************
*
*       _Hex2Bin
*/
static int _Hex2Bin(char c) {
  if ((c >= '0') && (c <= '9')) {
    return c - '0';
  }
  if ((c >= 'A') && (c <= 'F')) {
    return c - 'A' + 10;
  }
  if ((c >= 'a') && (c <= 'f')) {
    return c - 'a' + 10;
  }
  return 0;
}

/*********************************************************************
*
*       _IsAlphaNum
*/
static int _IsAlphaNum(char c) {
  if ((c >= '0') && (c <= '9')) {
    return 1;
  }
  if ((c >= 'A') && (c <= 'Z')) {
    return 1;
  }
  if ((c >= 'a') && (c <= 'z')) {
    return 1;
  }
  return 0;
}

/*********************************************************************
*
*       _CompareFilenameExt
*
*  Function description
*    Checks if the given filename has the given extension
*    The test is case-sensitive, meaning:
*    _CompareFilenameExt("Index.html", ".html")           ---> Match
*    _CompareFilenameExt("Index.htm",  ".html")           ---> Mismatch
*    _CompareFilenameExt("Index.HTML", ".html")           ---> Mismatch
*    _CompareFilenameExt("Index.html", ".HTML")           ---> Mismatch
*
*  Parameters
*    sFilename     Null-terminated filename, such as "Index.html"
*    sExtension    Null-terminated filename extension with dot, such as ".html"
*
*  Return value
*     0   match
*  != 0   mismatch
*/
static char _CompareFilenameExt(const char * sFilename, const char * sExt) {
  int LenFilename;
  int LenExt;
  int i;
  char c0;
  char c1;

  LenFilename = strlen(sFilename);
  LenExt = strlen(sExt);
  if (LenFilename < LenExt) {
    return 1;                     // mismatch
  }
  for (i = 0; i < LenExt; i++) {
    c0 = *(sFilename + LenFilename -i -1);
    c1 = *(sExt + LenExt -i -1);
    if (c0 != c1) {
      return 1;                   // mismatch
    }
  }
  return 0;
}

/**************************************************************************************************************************************************************
*
*       Output related code
*/

/*********************************************************************
*
*       _Send
*
*  Function description
*    Calls the Send callback function.
*    Return value is checked; on error, the connection is normally broken
*    and the Hook function pointer is reset.
*/
static void _Send(OUT_BUFFER_CONTEXT * pOutContext, U8 * pData, unsigned NumBytes) {
  int r;
  IP_WEBS_tSend pfSend;

  pfSend = pOutContext->pConnection->pfSend;
  if (pfSend) {
    r = pfSend(pData, NumBytes, pOutContext->pConnection->pConnectInfo);
    if (r < 0) {
      pOutContext->pConnection->pfSend = (IP_WEBS_tSend)NULL;
      pOutContext->pConnection->HasError = 1;
    }
  }
}

/*********************************************************************
*
*       _FlushChunked
*
*  Function description
*/
static void _FlushChunked(OUT_BUFFER_CONTEXT * pOutContext) {
  char ac[9];
  char * s;
  int NumBytes;

  NumBytes = pOutContext->Cnt;
  ac[0] = CR;
  ac[1] = LF;
  s = _StoreUnsigned(&ac[2], NumBytes, 16, 0);
  *s++ = CR;
  *s++ = LF;
  _Send(pOutContext, (U8 *)ac, s - &ac[0]);
  _Send(pOutContext, pOutContext->pBuffer, NumBytes);
  pOutContext->Cnt = 0;
}

/*********************************************************************
*
*       _FlushRaw
*/
static void _FlushRaw(OUT_BUFFER_CONTEXT * pOutContext) {
  int Len;

  Len = pOutContext->Cnt;
  if (Len) {
    _Send(pOutContext, pOutContext->pBuffer, Len);
    pOutContext->Cnt = 0;
  }
}

/*********************************************************************
*
*       _Flush
*/
static void _Flush(OUT_BUFFER_CONTEXT * pOutContext) {
  int Len;

  Len = pOutContext->Cnt;
  if (Len) {
    if (pOutContext->Encoding == ENCODING_RAW) {
      _FlushRaw(pOutContext);
    } else {
      _FlushChunked(pOutContext);
    }
  }
}

/*********************************************************************
*
*       _WriteChar
*/
static void _WriteChar(OUT_BUFFER_CONTEXT * pOutContext, char c) {
  int Len;

  Len = pOutContext->Cnt;
  *(pOutContext->pBuffer + Len) = c;
  Len++;
  pOutContext->Cnt = Len;
  if (Len == pOutContext->BufferSize) {
    _Flush(pOutContext);
  }
}

/*********************************************************************
*
*       _WriteString
*/
static void _WriteString(OUT_BUFFER_CONTEXT * pOutContext, const char * s) {
  char c;

  do {
    c = *s++;
    if (c == 0) {
      break;
    }
    _WriteChar(pOutContext, c);
  } while (1);
}

/*********************************************************************
*
*       _WriteMem
*/
static void _WriteMem(OUT_BUFFER_CONTEXT * pOutContext, const char *pSrc, int NumBytes) {
  char c;
  while (NumBytes-- > 0) {
    c = *pSrc++;
    _WriteChar(pOutContext, c);
  }
}

/*********************************************************************
*
*       _WriteUnsigned
*
*  Function description
*    Writes an unsigned numerical value into the out context.
*
*  Parameters
*    pOutContext   Out context containing buffer information and output functions.
*    v             Value to output
*    Base          Numerical base. Typically 2, 10 or 16.
*    NumDigits     Minimum number of digits to output.
*
*  Examples
*    _WriteUnsigned(p, 100, 10, 0)   ->   "100"
*    _WriteUnsigned(p, 100, 10, 4)   ->   "0100"
*    _WriteUnsigned(p, 100, 16, 0)   ->   "64"
*    _WriteUnsigned(p, 100,  2, 0)   ->   "1100100"
*/
static void _WriteUnsigned(OUT_BUFFER_CONTEXT * pOutContext, U32 v, unsigned Base, int NumDigits) {
  unsigned Div;
  U32 Digit = 1;
  //
  // Count how many digits are required
  //
  while (((v / Digit) >= Base) | (NumDigits-- > 1)) {
    Digit *= Base;
  }
  //
  // Output digits
  //
  do {
    Div = v / Digit;
    v  -= Div * Digit;
    _WriteChar(pOutContext, _aV2C[Div]);
    Digit /= Base;
  } while (Digit);
}

/*********************************************************************
*
*       _Read
*
*  Function description
*    Calls receive once, trying to read as many bytes as fit into the input buffer.
*
*  Return value
*    0    Connection closed
*  > 0    Number of bytes read
*  < 0    Error
*/
static int _Read(IN_BUFFER_DESC * pBufferDesc) {
  U8 * p;
  int WrOff;
  int i;
  int Cnt;
  int Size;
  int Limit;
  int NumBytes;

  if (pBufferDesc->pConnection == NULL) {
    return -1;      // Error occurred.
  }
  if (pBufferDesc->Cnt == 0) {
    pBufferDesc->RdOff = 0;
  }
  Cnt  = pBufferDesc->Cnt;
  Size = pBufferDesc->Size;
  WrOff = pBufferDesc->RdOff + Cnt;
  if (WrOff >= Size) {
    WrOff -= Size;
  }
  p = pBufferDesc->pBuffer;

  Limit = Size;
  if (pBufferDesc->RdOff > WrOff) {
    Limit = pBufferDesc->RdOff;
  }
  NumBytes = Limit - WrOff;
  i = pBufferDesc->pConnection->pfReceive(p + WrOff, NumBytes, pBufferDesc->pConnection->pConnectInfo);
  if (i > 0) {
    pBufferDesc->Cnt = Cnt + i;
  }
  return i;
}

/*********************************************************************
*
*       _FindString
*
*  Function description
*    Locates a string in a buffer.
*
*  Return value
*    == (BufferSize - sizeofTag)   String not found
*    <  (BufferSize - sizeofTag)   String either completely or partially found
*
*/
static int _FindString(const char * sBuffer, const char * sTag, int sizeofTag, int BufferSize) {
  const char * s;
  int MaxPos;
  int i;
  int j;

  MaxPos = BufferSize - sizeofTag;
  if (MaxPos <= 0) {
    return BufferSize;
  }
  for (i = 0; i <= MaxPos; i++) {
    int jMax;
    jMax = BufferSize - i - 1;
    if (jMax > sizeofTag - 1) {
      jMax = sizeofTag - 1;
    }
    s = sBuffer + i;
    for (j = 0; ; j++) {
      char cBuffer;
      char cTag;
      cBuffer = *(s    + j);
      cTag    = *(sTag + j);
      if (cBuffer != cTag) {
        break;
      }
      if (j == jMax) {
        return i;             // String found (at least partially)
      }
    }
  }
  return MaxPos;              // String not found
}

/*********************************************************************
*
*       _FindChar
*
*  Function description
*    Locates a character in a buffer.
*    Examples:
*      "Rolf&",  &, 5     -> 4                   // Found at position 4
*      "Rolf\0", &, 5     -> 5                   // Not found
*
*  Return value
*    == BufferSize:   Character not found
*    <  BufferSize:   Number of charcters before the hit.
*
*/
static int _FindChar(const char * sBuffer, char cRef, int BufferSize) {
  char c;
  int i;

  for (i = 0; i < BufferSize; i++) {
    c = *(sBuffer + i);
    if (c == cRef) {
      return i;
    }
  }
  return BufferSize;          // Not found
}

/*********************************************************************
*
*       _CallApplication
*
*  Function description
*/
static int _CallApplication(WEBS_CONTEXT * pContext, const char * sCmd, int NumBytes, const char * sValue) {
  OUT_BUFFER_CONTEXT * pOutContext;
  const WEBS_CGI * pCGI;
  int i;

  pCGI        = pContext->pApplication->paCGI;
  pOutContext = &pContext->OutBuffer;
  for (i = 0; ; i++) {
    if (pCGI->sName == NULL) {
      break;
    }
    if ((memcmp(sCmd, pCGI->sName, NumBytes) == 0) && (pCGI->pf != NULL)) {
      pCGI->pf((WEBS_OUTPUT*)pOutContext, NULL, sValue);
      return i;
    }
    pCGI++;
  }
  //
  // At this point no entry was found, start default handler if available.
  //
  if (pCGI->pf != NULL) {
    pCGI->pf((WEBS_OUTPUT*)pOutContext, sCmd, sValue);
    return i;
  }
  return -1;
}

/*********************************************************************
*
*       _CallVFile
*
*  Function description
*/
static int _CallVFile(WEBS_CONTEXT * pContext, const char * sVFileName, int NumBytes) {
  OUT_BUFFER_CONTEXT * pOutContext;
  const WEBS_VFILES  * pVFile;
  int i;

  pVFile      = pContext->pApplication->paVFiles;
  pOutContext = &pContext->OutBuffer;
  for (i = 0; ; i++) {
    if (pVFile->sName == NULL) {
      break;
    }
    if ((memcmp(sVFileName, pVFile->sName, NumBytes) == 0) && (pVFile->pf != NULL)) {
#if WEBS_PARA_BUFFER_SIZE
      pVFile->pf((WEBS_OUTPUT*)pOutContext, pOutContext->pPara);
#else
      pVFile->pf((WEBS_OUTPUT*)pOutContext, NULL);
#endif
      return i;
    }
    pVFile++;
  }
  return -1;
}

/*********************************************************************
*
*       _ExecCGI
*
*  Function description
*/
static void _ExecCGI(WEBS_CONTEXT * pContext, const char * sCmd, int NumBytes) {
  int i;

  i = _FindChar(sCmd, '?', NumBytes);
  if (i < NumBytes) {
    _CallApplication(pContext, sCmd, i, sCmd+i);
  } else {
    _CallApplication(pContext, sCmd, i, NULL);
  }
}

/*********************************************************************
*
*       _SendError
*
*  Function description
*    Sends HTTP header and error message.
*/
static void _SendError(OUT_BUFFER_CONTEXT * pOutContext, const char * sErrHeader, const char * sErrBody, U16 ErrorCode) {
  int NumBytes;
  int NumBytesFiller;
  int i;
  static const char _sFiller[] = "<!-- Fill-up to avoid error in Internet explorer -->\n";

  _WriteString(pOutContext, sErrHeader);
  _WriteString(pOutContext, "Server: embOS/IP\r\n"
                            "Accept-Ranges: bytes\r\n"
                            "Content-Length: "
                            );
  NumBytes       = strlen(sErrBody);
  NumBytesFiller = strlen(_sFiller);
  _WriteUnsigned(pOutContext, NumBytes + 10 * NumBytesFiller, 10, 0);
  _WriteString(pOutContext, "\r\n"
                            "Content-Type: text/html\r\n"
                            "X-Pad: avoid browser bug\r\n"
                            );
  if (ErrorCode == 401) {
    _WriteString(pOutContext, "WWW-Authenticate: Basic realm=\"Embedded webserver Username: user Password: pass\"\r\n");
  } else {
    _WriteString(pOutContext, "Connection: close\r\n");
  }
  _WriteString(pOutContext, "\r\n");
  _WriteString(pOutContext, sErrBody);
  for (i = 0; i < 10; i++) {
    _WriteString(pOutContext, _sFiller);
  }
  _Flush(pOutContext);
}

/*********************************************************************
*
*       _SendFile
*
*  Function description
*/
static void _SendFile(WEBS_CONTEXT * pContext, void * hFile, char AllowDynContent) {
  int FileLen;
  int FilePos;
  int NumBytesAtOnce;
  OUT_BUFFER_CONTEXT * pOutContext;
  char acTempBuffer[WEBS_TEMP_BUFFER_SIZE];
  int i;
  int NumBytesInBuffer;

  pOutContext = &pContext->OutBuffer;
  FileLen =  pContext->pFS_API->pfGetLen(hFile);
  FilePos = 0;
  NumBytesInBuffer = 0;
  _Flush(&pContext->OutBuffer);
  if (pContext->Connection.HTTPVer == 100) {
    pContext->OutBuffer.Encoding = ENCODING_RAW;     // HTTP V1.0 does not support chunked encoding.
  } else {
    pContext->OutBuffer.Encoding = ENCODING_CHUNKED;
  }
  if (AllowDynContent) {
    while ((FileLen > 0) || (NumBytesInBuffer != 0)) {
      //
      // Early out in case of error
      //
      if (pOutContext->pConnection->HasError) {
        return;
      }
      //
      // Read as much as we can (based on free space in buffer and remaining file size)
      //
      NumBytesAtOnce = sizeof(acTempBuffer) - NumBytesInBuffer;
      if (NumBytesAtOnce > FileLen) {
        NumBytesAtOnce = FileLen;
      }
      if (NumBytesAtOnce) {
        pContext->pFS_API->pfReadAt(hFile, &acTempBuffer[NumBytesInBuffer], FilePos, NumBytesAtOnce);
        FilePos += NumBytesAtOnce;
        FileLen -= NumBytesAtOnce;
        NumBytesInBuffer += NumBytesAtOnce;
      }
      //
      // Handle everything which is certainly static
      //
      i = _FindString(acTempBuffer, WEBS_CGI_START_STRING, sizeof(WEBS_CGI_START_STRING) -1, NumBytesInBuffer);
      if (i) {
        _WriteMem(&pContext->OutBuffer, acTempBuffer, i);
        if (i != NumBytesInBuffer) {
          memmove(acTempBuffer, &acTempBuffer[i], NumBytesInBuffer - i);
        }
        NumBytesInBuffer -= i;
      } else {
        const char * sCGI;
        //
        // Handle dynamic content. Find end delimiter first.
        //
        sCGI = &acTempBuffer[CGI_START_LEN];
        i = _FindString(sCGI, WEBS_CGI_END_STRING, CGI_END_LEN, NumBytesInBuffer - CGI_START_LEN);
        _ExecCGI(pContext, sCGI, i);
        i += CGI_START_LEN + CGI_END_LEN;
        memmove(acTempBuffer, &acTempBuffer[i], NumBytesInBuffer - i);
        NumBytesInBuffer -= i;
      }
    }
  } else {
    //
    // Static files (such as gifs etc)
    //
    while (FileLen > 0) {
      //
      // Early out in case of error
      //
      if (pOutContext->pConnection->HasError) {
        return;
      }
      //
      // Read as much as we can (based on free space in buffer and remaining file size)
      //
      NumBytesAtOnce = pContext->OutBuffer.BufferSize;
      if (NumBytesAtOnce > FileLen) {
        NumBytesAtOnce = FileLen;
      }
      pContext->pFS_API->pfReadAt(hFile, pContext->OutBuffer.pBuffer, FilePos, NumBytesAtOnce);
      FilePos += NumBytesAtOnce;
      FileLen -= NumBytesAtOnce;
      pContext->OutBuffer.Cnt = NumBytesAtOnce;
      _Flush(&pContext->OutBuffer);
    }
  }
  _Flush(&pContext->OutBuffer);
  if (pContext->Connection.HTTPVer != 100) {
    pContext->OutBuffer.Encoding = ENCODING_RAW;
    _WriteString(pOutContext, "\r\n0\r\n\r\n");
    _Flush(&pContext->OutBuffer);
  }
}


typedef struct {
  const char *sExt;
  const char *sContent;
} FILE_TYPE;

static const FILE_TYPE _aFileType[] = {
  { "gif",  "image/gif"},
  { "png",  "image/png"},
  { "jpg",  "image/jpeg"},
  { "jpeg", "image/jpeg"},
  { "htm",  "text/html"},
  { "html", "text/html"},
  { "cgi",  "text/html"},
  { "js",   "application/x-javascript"},
  { 0},
};

/*********************************************************************
*
*       _sFilename2sContent
*
*  Function description
*/
static const char * _sFilename2sContent(const char * sFilename) {
  unsigned int NumBytes;
  int i;
  int DotPos;

  NumBytes = strlen(sFilename);
  //
  // Find postion of dot
  //
  for (i = NumBytes - 1; ;i--) {
    if (i < 0) {
      return "Unknown";
    }
    if (*(sFilename + i) == '.') {
      break;
    }
  }
  DotPos = i;
  //
  // Find file type in list
  //
  for (i = 0; ;i++) {
    const char * sExt;
    sExt = _aFileType[i].sExt;
    if (sExt == NULL) {
      break;
    }
    if (strlen(sExt) == NumBytes -1 - DotPos) {
      if (strcmp(sExt, sFilename + DotPos + 1) == 0) {
        return _aFileType[i].sContent;
      }
    }
  }
  return "Unknown";
}

/*********************************************************************
*
*       _GetCharND
*
*  Function description
*    Get character from buffer without removing it from buffer
*
*  Return value
*    >= 0  value of character at the given buffer position
*    -1    no character at given buffer position
*/
static int _GetCharND(IN_BUFFER_DESC * pBufferDesc, int Off) {
  int r;
  int Pos;
  r = -1;
  if (Off >= pBufferDesc->Cnt) {
    return -1;
  }
  Pos = pBufferDesc->RdOff + Off;
  if (Pos >= pBufferDesc->Size) {
    Pos -= pBufferDesc->Size;
  }
  r = *(pBufferDesc->pBuffer + Pos);
  return r;
}

/*********************************************************************
*
*       _GetChar
*
*  Function description
*    Get & remove a single character from buffer
*
*  Return value
*    >= 0  value of character at the given buffer position
*    -1    no character at given buffer position
*/
static int _GetChar(IN_BUFFER_DESC * pBufferDesc) {
  int r;
  int RdOff;
  r = -1;
  if (pBufferDesc->Cnt == 0) {
    return -1;
  }
  RdOff = pBufferDesc->RdOff;
  if (RdOff >= pBufferDesc->Size) {
    RdOff -= pBufferDesc->Size;
  }
  r = *(pBufferDesc->pBuffer + RdOff);
  //
  // Adjust Cnt + RdOff
  //
  pBufferDesc->Cnt--;
  pBufferDesc->RdOff++;
  if (pBufferDesc->RdOff >= pBufferDesc->Size) {
    pBufferDesc->RdOff = 0;
  }
  return r;
}

/*********************************************************************
*
*       _GetLineLen
*
*  Function description
*    Returns the number of characters in the next complete line in the buffer
*    Does not modify the buffer content
*
*  Return value
*      0 No complete line in buffer
*    > 0 Number of characters in next line, incl. CR & LF
*
*  Sample
*    "GET / \r\n" returns 8
*/
static int _GetLineLen(IN_BUFFER_DESC * pBufferDesc) {
  int i;
  int c;

  i = 0;
  while((i + 1) < pBufferDesc->Cnt) {
    c = _GetCharND(pBufferDesc, i);
    i++;
    if (c == 0x0D) {
      c = _GetCharND(pBufferDesc, i);
      if (c == 0x0A) {
        return i + 1;
      }
    }
  }
  return 0;
}

/*********************************************************************
*
*       _EatChars
*
*  Function description
*    Swallow the given number of bytes
*/
static void _EatChars(IN_BUFFER_DESC * pBufferDesc, int NumChars) {
  int RdPos;
  if (NumChars > pBufferDesc->Cnt) {
    WEBS_WARN(("_EatChars(): Trying to eat more bytes than in buffer"));
    pBufferDesc->Cnt   = 0;
    pBufferDesc->RdOff = 0;
    return;
  }
  pBufferDesc->Cnt -= NumChars;
  RdPos = pBufferDesc->RdOff + NumChars;
  if (RdPos >= pBufferDesc->Size) {
    RdPos -= pBufferDesc->Size;
  }
  if (pBufferDesc->Cnt == 0) {
    RdPos = 0;
  }
  pBufferDesc->RdOff = RdPos;
}

/*********************************************************************
*
*       _EatLine
*
*  Function description
*    Swallow everything until end of line (0D 0A)
*
*  Return value
*    0  No complete line in buffer
*    1  Line eaten.
*
*/
static int _EatLine(IN_BUFFER_DESC * pBufferDesc) {
  int r;
  r = -1;
  r = _GetLineLen(pBufferDesc);
  if (r > 0) {
    _EatChars(pBufferDesc, r);
    return 1;
  }
  return 0;
}

/*********************************************************************
*
*       _EatWhite
*
*  Return value
*    0   No white space eaten
*    >0  No of eaten white spaces
*/
static int _EatWhite(IN_BUFFER_DESC * pBufferDesc) {
  char c;
  int RdOff;
  int r;

  RdOff = pBufferDesc->RdOff;
  r = 0;
  while (1) {
    if (r == pBufferDesc->Cnt) {
      break;                        // End of buffer reached
    }
    c = * (pBufferDesc->pBuffer + RdOff);
    if (_IsWhite(c) == 0) {
      break;
    }
    r++;
    RdOff++;
    if (RdOff >= pBufferDesc->Size) {
      RdOff = 0;
    }
  }
  pBufferDesc->RdOff = RdOff;
  pBufferDesc->Cnt -= r;
  return r;
}

/*********************************************************************
*
*       _CompareCmd
*
*  Function description
*
*  Return value
*    Number of bytes "swallowed"
*/
static int _CompareCmd(IN_BUFFER_DESC * pBufferDesc, const char * sCmd) {
  char c;
  char c1;
  U8 * pBuffer;
  int RdOff;
  int i;

  i = 0;
  pBuffer = pBufferDesc->pBuffer;
  RdOff   = pBufferDesc->RdOff;
  do {
    c = *sCmd++;
    c = tolower(c);
    c1 = *(pBuffer + RdOff);
    c1 = tolower(c1);
    if (c == 0) {           // Last char of Command reached ?
      if (isalpha(c1) == 0) {
        return i;           // Command found.
      } else {
        return 0;           // Command not found
      }
    }
    if (c != c1) {
      return 0;             // Command not found
    }
    i++;
    RdOff++;
    if (RdOff >= pBufferDesc->Size) {
      RdOff = 0;
    }
  } while (1);
}

/*********************************************************************
*
*       _ReadLine
*
*  Function description
*    Make sure entire line is in buffer.
*    If no entire line is in buffer, read data until an entire line is in buffer or buffer is full.
*
*    >0  Number of bytes in line
*   <=0  Error
*/
static int _ReadLine(IN_BUFFER_DESC * pBufferDesc) {
  int i;
  while (1) {
    i = _GetLineLen(pBufferDesc);
    if (i > 0) {
      break;
    }
    i = _Read(pBufferDesc);
    if (i <= 0) {
      return -1;     // Error, close connection
    }
  }
  return i;          // Complete line in buffer
}

/*********************************************************************
*
*       _GetURI
*
*  Function description
*    Get & remove a URI from buffer
*
*  Return value:
*
*/
static int _GetURI(IN_BUFFER_DESC * pBufferDesc, char * sURI, int NumBytes) {
  char c;
  U8 * pBuffer;
  int RdOff;
  int Cnt;
  int i;

  i = 0;
  pBuffer = pBufferDesc->pBuffer;
  RdOff   = pBufferDesc->RdOff;
  Cnt     = pBufferDesc->Cnt;
  do {
    if (Cnt == 0) {
      WEBS_WARN(("Line not terminated"));
      return 0;
    }
    c = *(pBuffer + RdOff);
    if ((c <= 32)  || (c >= 127)) {
      break;
    }
    if (c == '?') {
      break;
    }
    *sURI++ = c;
    i++;
    if (i >= NumBytes) {
      WEBS_WARN(("Error, URI does not fit"));
      return 0;
    }
    RdOff++;
    Cnt--;
    if (RdOff >= pBufferDesc->Size) {
      RdOff = 0;
    }
  } while (1);
  pBufferDesc->RdOff = RdOff;
  pBufferDesc->Cnt   = Cnt;
  *sURI = 0;
  return i;
}

/*********************************************************************
*
*       _GetChars
*
*  Function description
*    Get & remove a given number of character from buffer
*/
static int _GetChars(IN_BUFFER_DESC * pBufferDesc, char * sDest, int NumBytes) {
  char c;
  U8 * pBuffer;
  int RdOff;
  int Cnt;
  int i;

  pBuffer = pBufferDesc->pBuffer;
  RdOff   = pBufferDesc->RdOff;
  Cnt     = pBufferDesc->Cnt;
  if (Cnt < NumBytes) {
    WEBS_WARN(("Too few bytes"));
    return 0;
  }
  for (i = 0; i < NumBytes; i++) {
    c = *(pBuffer + RdOff);
    *sDest++ = c;
    RdOff++;
    Cnt--;
    if (RdOff >= pBufferDesc->Size) {
      RdOff = 0;
    }
  }
  pBufferDesc->RdOff = RdOff;
  pBufferDesc->Cnt   = Cnt;
  return i;
}

/*********************************************************************
*
*       _OpenFile
*
*  Function description
*/
static void * _OpenFile(WEBS_CONTEXT   * pContext, const char * s) {
  return pContext->pFS_API->pfOpenFile(s);
}

/*********************************************************************
*
*       _HandleFormData
*
*  Function description
*    Handle form data. The form data can be either from a "GET" or a "POST"
*    message and is in the following format:
*    ?<Param0>=<Value0>&<Param1>=<Value1>& ... <Paramn>=<Valuen>
*/
static void _HandleFormData(WEBS_CONTEXT * pContext, int LineLen) {
  char acTempBuffer[WEBS_TEMP_BUFFER_SIZE];
  IN_BUFFER_DESC * pBufferDesc;
  int ParaLen;
  int ValueLen;
  char c;

  pBufferDesc = &pContext->InBufferDesc;
  if (LineLen <= 2) {
    return;
  }
  while (1) {
    //
    // Copy Parameter name
    //
    ParaLen  = 0;
    ValueLen = 0;
    while (1) {
      c = _GetChar(pBufferDesc);
      if ((c >= 'A') & (c <= 'Z')) {
        goto NextPara;
      }
      if ((c >= 'a') & (c <= 'z')) {
        goto NextPara;
      }
      if ((c >= '0') & (c <= '9')) {
        goto NextPara;
      }
      switch (c) {
      case '.':
      case '-':
      case '_':
        goto NextPara;
      }
      break;
NextPara: acTempBuffer[ParaLen++] = c;
    }
    acTempBuffer[ParaLen++] = 0;
    if (c != '=') {
      return;        // Expected a '='
    }
    //
    // Copy Value name
    //
    while (1) {
      c = _GetChar(pBufferDesc);
      if (c == '%') {
        char c0, c1;
        c0 = _GetChar(pBufferDesc);
        c1 = _GetChar(pBufferDesc);
        c  = _Hex2Bin(c0) << 4;
        c |= _Hex2Bin(c1);
        goto NextVChar;
      }
      if ((c >= 'A') & (c <= 'Z')) {
        goto NextVChar;
      }
      if ((c >= 'a') & (c <= 'z')) {
        goto NextVChar;
      }
      if ((c >= '0') & (c <= '9')) {
        goto NextVChar;
      }
      if (c == '+') {
        c = ' ';
        goto NextVChar;
      }
      switch (c) {
      case '.':
      case '-':
      case '_':
      case '*':
      case '(':
      case ')':
      case '!':
      case '$':
      case '\'':
        goto NextVChar;
      }
      break;
NextVChar:
      acTempBuffer[ParaLen + ValueLen++] = c;
    }
    acTempBuffer[ParaLen + ValueLen] = 0;
    if (_CallApplication(pContext, &acTempBuffer[0], ParaLen - 1, &acTempBuffer[ParaLen]) < 0) {
      pContext->pApplication->pfHandleParameter((WEBS_OUTPUT*)&pContext->OutBuffer, &acTempBuffer[0], &acTempBuffer[ParaLen]);
    }
  }
}

/*********************************************************************
*
*       _DecodeAndCopyStr
*
*  Function description
*    Checks if a string includes url encoded characters, decodes the
*    characters and copies them into destination buffer.
*    Destination string is 0-terminated.
*    Source and destination buffer can be identical.
*
*     pSrc               | SrcLen | pDest              | DestLen
*    ------------------------------------------------------------
*     "FirstName=J%F6rg" | 16     | "FirstName=Jörg\0" | 15
*     "FirstName=John"   | 14     | "FirstName=John\0" | 15
*/
static void _DecodeAndCopyStr(char * pDest, int DestLen, char * pSrc, int SrcLen) {
  int i;
  char c;

  //
  // Decode characters (if required) and copy into buffer.
  //
  i = 0;
  while (i < (DestLen - 1)) {
    if (--SrcLen  < 0) {            // Complete source copied?
      break;
    }
    c = *pSrc++;
    if ((c >= 'A') & (c <= 'Z')) {
      goto AddChar;
    }
    if ((c >= 'a') & (c <= 'z')) {
      goto AddChar;
    }
    if ((c >= '0') & (c <= '9')) {
      goto AddChar;
    }
    if (c == '+') {
      c = ' ';
      goto AddChar;
    }
    if (c == '%') {
      char c0, c1;
      SrcLen -= 2;
      if (SrcLen  < 0) {            // Sufficient source characters remaining?
        break;                      // Error, string ends after '%'
      }
      c0 = *pSrc++;
      c1 = *pSrc++;
      c  = _Hex2Bin(c0) << 4;
      c |= _Hex2Bin(c1);
      goto AddChar;
    }
    switch (c) {
    case '.':
    case '-':
    case '_':
    case '*':
    case '(':
    case ')':
    case '!':
    case '$':
    case '\'':
      goto AddChar;
    default:
      break;         // Illegal character -> We are done
    }
AddChar:
    i++;
    *pDest++ = c;
  }
  *pDest = 0;        // Zero-terminate destination
}

/*********************************************************************
*
*       _IsAuthorized
*
*  Function description
*/
static char _IsAuthorized(WEBS_CONTEXT * pContext, const char * sFile, const char * acUserPass) {
  int Len;
  WEBS_ACCESS_CONTROL * pAccess;

  pAccess = pContext->pApplication->paAccess;
  if (pAccess == NULL) {
    return 1;
  }
  while (pAccess->sPath) {
    Len = strlen(pAccess->sPath);
    if (memcmp(pAccess->sPath, sFile, Len) == 0) {
      if (pAccess->sRealm == NULL) {
        return 1;                // Unprotected file
      }
      Len = strlen(pAccess->sUserPass);
      if (memcmp(pAccess->sUserPass, acUserPass, Len) == 0) {
        return 1;                // Valid authorization
      }
      return 0;                  // Invalid authorization
    }
    pAccess++;
  }
  return 0;
}

/*********************************************************************
*
*       _WriteHeaderDate
*
*  Function description
*    Writes the time and date stamp into the output buffer.
*    Time and Date stamp should look like:
*                                 26 OCT 1995 00:00:00 GMT\r\n
*/
static void _WriteHeaderDate(OUT_BUFFER_CONTEXT * pOutContext, U32 SysTime) {
  U32 Sec;
  U32 Min;
  U32 Hour;
  U32 Day;
  U32 Month;
  U32 Year;

  //
  // Get time and date and build header field.
  //
  Day   = (SysTime >> 16) & 0x1F;
  Month = (SysTime >> 21) & 0x0F;
  Year  = (SysTime >> 25) & 0x7F;
  Hour  = (SysTime >> 11) & 0x1F;
  Min   = (SysTime >>  5) & 0x3F;
  Sec   =  SysTime        & 0x1F;
  _WriteUnsigned(pOutContext, Day, 10, 0);
  _WriteString(pOutContext, " ");
  _WriteString(pOutContext, _asMonth[Month-1]);
  _WriteString(pOutContext, " ");
  _WriteUnsigned(pOutContext, Year + 1980, 10, 0);
  _WriteString(pOutContext, " ");
  _WriteUnsigned(pOutContext, Hour, 10, 2);
  _WriteString(pOutContext, ":");
  _WriteUnsigned(pOutContext, Min, 10, 2);
  _WriteString(pOutContext, ":");
  _WriteUnsigned(pOutContext, Sec, 10, 2);
  _WriteString(pOutContext, " GMT\r\n");
}

/*********************************************************************
*
*       _OutputHeader
*
*  Function description
*    Outputs a valid HTML header.
*/
static void _OutputHeader(WEBS_CONTEXT * pContext, const char * sFilename) {
  OUT_BUFFER_CONTEXT * pOutContext;
  const char * sContent;

  sContent = _sFilename2sContent(sFilename);
  pOutContext = &pContext->OutBuffer;
  _WriteString(pOutContext,   "HTTP/1.1 200 OK\r\n"
                              "Content-Type: ");
  _WriteString(pOutContext,   sContent);
  _WriteString(pOutContext,   "\r\n"
                              "Server: embOS/IP\r\n");
  //
  // If "Expires" time stamp is available, add to header
  //
  if (pContext->Connection.FileInfo.DateExp != 0) {
    _WriteString(pOutContext, "Expires: ");
    _WriteHeaderDate(pOutContext, pContext->Connection.FileInfo.DateExp);
  }
  //
  // If "Last-Modified" time stamp is available, add to header
  //
  if (pContext->Connection.FileInfo.DateLastMod != 0) {
    _WriteString(pOutContext, "Last-Modified: ");
    _WriteHeaderDate(pOutContext, pContext->Connection.FileInfo.DateLastMod);
  }
  _WriteString(pOutContext,   "");
  //
  // HTTP versions < 1.1 do not support chunked transfer-encoding.
  //
  if (pContext->Connection.HTTPVer == 100) {
    _WriteString(pOutContext, "\r\n");
  } else {
    _WriteString(pOutContext, "Transfer-Encoding: chunked\r\n");
  }
}

/*********************************************************************
*
*       _HandleVFile
*
*  Function description
*/
static void _HandleVFile(WEBS_CONTEXT * pContext, const char * pURI) {
  _OutputHeader(pContext, pURI);
  //
  // Clean buffer and check if chunked encoding is allowed.
  //
  _Flush(&pContext->OutBuffer);
  if (pContext->Connection.HTTPVer == 100) {
    pContext->OutBuffer.Encoding = ENCODING_RAW;     // HTTP V1.0 does not support chunked encoding.
  } else {
    pContext->OutBuffer.Encoding = ENCODING_CHUNKED;
  }
  //
  // Call CGI function in user application
  //
  _CallVFile(pContext, pURI + 1, strlen(pURI + 1));
  _Flush(&pContext->OutBuffer);
  //
  // Send end of transmission if chunked encoding was used and clean up (send buffer content and reset encoding type to RAW)
  //
  pContext->OutBuffer.Encoding = ENCODING_RAW;
  _WriteString(&pContext->OutBuffer, "\r\n0\r\n\r\n");
  _Flush(&pContext->OutBuffer);
}

/*********************************************************************
*
*       _ExecMethod
*
*  Function description
*/
static void _ExecMethod(WEBS_CONTEXT * pContext, unsigned Method) {
  char acURI[WEBS_FILENAME_BUFFER_SIZE];
  char acAuthIn[4];
  const char * s;
  int i;
  void * hFile;
  OUT_BUFFER_CONTEXT * pOutContext;
  U8 c;
  int ContentLen;

  hFile = NULL;
  pOutContext = &pContext->OutBuffer;
  //
  // Make sure entire line is in buffer
  //
  i = _ReadLine(&pContext->InBufferDesc);
  if (i < 0) {
    pContext->InBufferDesc.pConnection = NULL; // Fatal error! No complete line in buffer!
    return;
  }
  //
  // Get URI (Filename)
  //
  // Original line is similar to the following:
  // /index.htm HTTP/1.0
  //
  i = _GetURI(&pContext->InBufferDesc, acURI, sizeof(acURI));
  if (i <= 0) {
OnError:
    pContext->InBufferDesc.pConnection = NULL; // Fatal error !
    return;
  }
  WEBS_LOG(("Get %s\n", acURI));
  //
  // Complete the requested file name if required.
  //
  s = acURI;
  if (strcmp(s, "/") == 0) {
    s = "/index.htm";
  }

  //
  // Retrieve file info from application. Before we call the optional callback, we set reasonable defaults.
  //
  pContext->Connection.FileInfo.AllowDynContent = 1;
  pContext->Connection.FileInfo.DateExp         = (1L << 16) | (1L << 21) | (15L << 25);   // Default expiration is Jan.1 1995, 00:00:00.
  if (_pfGetFileInfo) {
    _pfGetFileInfo(s, &pContext->Connection.FileInfo);
  }
#ifdef WEBS_PARA_BUFFER_SIZE
  if ((pContext->Connection.FileInfo.IsVirtual == 1) && (Method == METHOD_GET)) {
    //
    // Store CGI parameters for processing in virtual files.
    //
    i = _ReadLine(&pContext->InBufferDesc);
    if (i > 2) {
      int Len;
      c = _GetCharND(&pContext->InBufferDesc, 0);
      if (c == '?') {
        _EatChars(&pContext->InBufferDesc, 1);
        Len = _GetLineLen(&pContext->InBufferDesc);
        i = _FindString((const char *)pContext->InBufferDesc.pBuffer, " HTML/", 6, Len);  // The first line of a HTML header ends with the protocol version.
        i -= 5;
        memcpy(pOutContext->pPara, pContext->InBufferDesc.pBuffer + pContext->InBufferDesc.RdOff, i);
        *(pOutContext->pPara + i) = 0;
        _EatChars(&pContext->InBufferDesc, i); // Remove processed bytes from in buffer.
      }
    }
  }
#endif
  //
  // Check if client supports HTTP V1.1, earlier HTTP versions do not support chunked transmissions
  //
  _EatWhite(&pContext->InBufferDesc);
  if (_CompareCmd(&pContext->InBufferDesc, "HTTP/1.1")) {
    pContext->Connection.HTTPVer = 110;
  }
  //
  // Handle parameter if request method is GET
  //
  if (Method == METHOD_GET) {
    i = _ReadLine(&pContext->InBufferDesc);
    if (i > 2) {
      c = _GetCharND(&pContext->InBufferDesc, 0);
      if (c == '?') {
        _EatChars(&pContext->InBufferDesc, 1);
        _HandleFormData(pContext, i - 1);
      }
    }
  }
  _EatLine(&pContext->InBufferDesc);
  ContentLen = 0;
  //
  // Analyze header.
  // Header consists of a number of lines followed and terminated by an empty line (just CR LF).
  //
  // Sample header:
  //   Host: 192.168.3.1
  //   User-Agent: Mozilla/5.0 (Windows; U; Windows NT 5.0; en-GB; rv:1.8.1.7) Gecko/20070914 Firefox/2.0.0.7
  //   Accept: text/xml,application/xml,application/xhtml+xml,text/html;q=0.9,text/plain;q=0.8,image/png,*/*;q=0.5
  //   Accept-Language: en-gb,en;q=0.5
  //   Accept-Encoding: gzip,deflate
  //   Accept-Charset: ISO-8859-1,utf-8;q=0.7,*;q=0.7
  //   Keep-Alive: 300
  //   Connection: keep-alive
  //   Referer: http://192.168.3.1/
  //   Authorization: Basic YWRtaW46c2VnZ2VyMjM2
  //
  pContext->acUserPass[0] = 0;
  do {
    int CmdLen;
    i = _ReadLine(&pContext->InBufferDesc);
    if (i == 2) {
      _EatChars(&pContext->InBufferDesc, i);
      break;       // Empty line indicates end of header
    }
    //
    // Handle Content-Length           Sample: Content-Length: 29
    //
    CmdLen = _CompareCmd(&pContext->InBufferDesc, "Content-Length:");
    if (CmdLen) {
      _EatChars(&pContext->InBufferDesc, CmdLen);
      i -= CmdLen;
      i -= _EatWhite(&pContext->InBufferDesc);
      while (i > 2) {
        c = _GetChar(&pContext->InBufferDesc);
        if ((c < '0') || (c > '9')) {
          goto OnError;                      // Error, expected a dec. digit
        }
        ContentLen *= 10;
        ContentLen += c - '0';
        i--;
      }
      goto NextLine;
    }
    //
    // Handle Authorization: Basic.    Sample: Authorization: Basic YWRtaW46c2Vn==
    //
    CmdLen = _CompareCmd(&pContext->InBufferDesc, "Authorization:");
    if (CmdLen) {
      _EatChars(&pContext->InBufferDesc, CmdLen);
      i -= CmdLen;
      i -= _EatWhite(&pContext->InBufferDesc);
      if (i > 2) {
        CmdLen = _CompareCmd(&pContext->InBufferDesc, "Basic");
        if (CmdLen) {
          int DestOff;
          DestOff = 0;
          _EatChars(&pContext->InBufferDesc, CmdLen);
          i -= CmdLen;
          i -= _EatWhite(&pContext->InBufferDesc);
          while (i >= 6) {      // A complete set is always 4 bytes. These should be followed by at least a CR LF, making a min. of 6 characters.
            int NumBytesDest;
            if (DestOff > WEBS_AUTH_BUFFER_SIZE - 4) {    // We need space for 3 characters + terminating NUL
              break;
            }
            //
            // Read and decode 4 byte chunk
            //
            NumBytesDest = 3;
            _GetChars(&pContext->InBufferDesc, acAuthIn, 4);
            IP_UTIL_BASE64_Decode((unsigned char const *)acAuthIn, 4, (unsigned char *)&pContext->acUserPass[DestOff], &NumBytesDest);
            DestOff += NumBytesDest;
            i -= 4;
          }
          pContext->acUserPass[DestOff] = 0;    // String ends with NUL character
        }
      }
    }
    //
    // Unhandled command
    //
NextLine:
    _EatChars(&pContext->InBufferDesc, i);

  } while (1);
  //
  // Check access rights
  //
  if (_IsAuthorized(pContext, s, pContext->acUserPass) == 0) {
    WEBS_LOG(("Unauthorized\n"));
    _SendError(pOutContext, "HTTP/1.1 401 Unauthorized\r\n", WEBS_401_PAGE, 401);
    return;
  }
  //
  // Check if requested file is a "virtual" file and request method is "GET".
  //
  if ((pContext->Connection.FileInfo.IsVirtual == 1) && (Method == METHOD_GET)) {
    _HandleVFile(pContext, acURI);
    return;
  }
  //
  // Process normal file
  //
  if (pContext->Connection.FileInfo.IsVirtual != 1) {
    //
    // Open file if "real" file
    //
    hFile = _OpenFile(pContext, s);
    //
    // Send error reply in case we could not open file
    //
    if (hFile == NULL) {
      WEBS_LOG(("File %s not found\n", s));
      _SendError(pOutContext, "HTTP/1.1 404 Not Found\r\n", WEBS_404_PAGE, 404);
      return;
    }
    _OutputHeader(pContext, s); // Send header for requested file
  }
  //
  // Handle parameters in case of post
  //
  if (Method == METHOD_POST) {

    if (ContentLen > pContext->InBufferDesc.Size) {
      WEBS_WARN(("InBuffer too small for POST data."));
      return;                        // Error
    }
    while (pContext->InBufferDesc.Cnt < ContentLen) {
      i = _Read(&pContext->InBufferDesc);
      if (i <= 0) {
        return;                        // Error
      }
    }
#ifdef WEBS_PARA_BUFFER_SIZE
    if (pContext->Connection.FileInfo.IsVirtual == 1) {
      int NumBytes;
      int NumBytesRemain;
      //
      // Copy parameter into buffer.
      //
      NumBytes = pContext->InBufferDesc.Size - pContext->InBufferDesc.RdOff;
      if (NumBytes > ContentLen) {
        memcpy(pOutContext->pPara, pContext->InBufferDesc.pBuffer + pContext->InBufferDesc.RdOff, ContentLen);
       _EatChars(&pContext->InBufferDesc, ContentLen); // Remove processed bytes from in buffer.
      } else {
        //
        // Copy in two steps since in buffer descriptor wraps around.
        //
        memcpy(pOutContext->pPara, pContext->InBufferDesc.pBuffer + pContext->InBufferDesc.RdOff, NumBytes);
        NumBytesRemain = ContentLen - NumBytes;
        memcpy(pOutContext->pPara + NumBytes, pContext->InBufferDesc.pBuffer, NumBytesRemain);
        _EatChars(&pContext->InBufferDesc, ContentLen); // Remove processed bytes from in buffer.
      }
      *(pOutContext->pPara + ContentLen) = 0;
      _HandleVFile(pContext, acURI);
      return;
    } else
#endif
    {
      _HandleFormData(pContext, ContentLen);
    }
  }
  //
  // Send file (in case method was get)
  //
  if ((Method == METHOD_GET) | (Method == METHOD_POST)) {
    _SendFile(pContext, hFile, pContext->Connection.FileInfo.AllowDynContent);
  }
  pContext->pFS_API->pfCloseFile(hFile);
  _Flush(&pContext->OutBuffer);
}

/*********************************************************************
*
*       _ExecHead
*
*  Function description
*    Execute the "HEAD" method
*/
static void _ExecHead(WEBS_CONTEXT * pContext) {
  _ExecMethod(pContext, METHOD_HEAD);
}

/*********************************************************************
*
*       _ExecGet
*
*  Function description
*    Execute the "GET" method
*/
static void _ExecGet(WEBS_CONTEXT * pContext) {
  _ExecMethod(pContext, METHOD_GET);
}

/*********************************************************************
*
*       _ExecPost
*
*  Function description
*    Execute the "POST" method.
*
*  Notes
*    A typical command looks as follows:
*      POST /FormsPOST.htm HTTP/1.1
*      Host: 192.168.84.2
*      User-Agent: Mozilla/5.0 (Windows; U; Windows NT 5.0; en-US; rv:1.7.10) Gecko/20050716 Firefox/1.0.6
*      Accept: text/xml,application/xml,application/xhtml+xml,text/html;q=0.9,text/plain;q=0.8,image/png,;q=0.5
*      Accept-Language: en-us,en;q=0.5
*      Accept-Encoding: gzip,deflate
*      Accept-Charset: ISO-8859-1,utf-8;q=0.7,*;q=0.7
*      Keep-Alive: 300
*      Connection: keep-alive
*      Referer: http://192.168.84.2/FormsPOST.htm
*      Content-Type: application/x-www-form-urlencoded
*      Content-Length: 29
*
*      FirstName=hans&LastName=dampf
*/
static void _ExecPost(WEBS_CONTEXT * pContext) {
  _ExecMethod(pContext, METHOD_POST);
}

/*********************************************************************
*
*       _EatBytes
*
*  Function description
*/
static int _EatBytes(IN_BUFFER_DESC * pBufferDesc, int NumBytes) {
  int i;

  i = pBufferDesc->Cnt;
  i -= NumBytes;
  if (i < 0) {
    WEBS_WARN(("_EatBytes() wants to swallow too many bytes!"));
    pBufferDesc->Cnt = 0;
    return 0;
  }
  pBufferDesc->Cnt = i;

  i =  pBufferDesc->RdOff;
  i += NumBytes;
  if (i >= pBufferDesc->Size) {
    i -= pBufferDesc->Size;
  }
  pBufferDesc->RdOff = i;
  return NumBytes;
}

/*********************************************************************
*
*       _ParseInput
*
*  Function description
*
*    Parses the command.
*    Supported commands are:
*      "GET", "HEAD", "POST"
*
*    Unsupported commands are:
*      OPTIONS, PUT, DELETE, TRACE, CONNECT
*/
static int _ParseInput(WEBS_CONTEXT * pContext) {
  IN_BUFFER_DESC * pBufferDesc;

  pBufferDesc = &pContext->InBufferDesc;
  if (_CompareCmd(pBufferDesc, "get")) {
    _EatBytes(pBufferDesc, 4);
    _ExecGet(pContext);
    return 0;
  }
  if (_CompareCmd(pBufferDesc, "head")) {
    _EatBytes(pBufferDesc, 5);
    _ExecHead(pContext);
    return 0;
  }
  if (_CompareCmd(pBufferDesc, "post")) {
    _EatBytes(pBufferDesc, 5);
    _ExecPost(pContext);
    return 0;
  }
  _SendError(&pContext->OutBuffer, "HTTP/1.1 501 Not Implemented\r\n", WEBS_501_PAGE, 501);
  return -1;     // Error, unknown command
}

/*********************************************************************
*
*       _Process
*
*  This is the main loop of the web server
*/
static void _Process(WEBS_CONTEXT * pContext) {
  int i;

  do {
    i = _ReadLine(&pContext->InBufferDesc);
    if (i <= 0) {
      return;     // Error, close connection
    }
    i = _ParseInput(pContext);
    if (i < 0) {
      return;     // Error, close connection
    }
    if ((pContext->Connection.HTTPVer == 100) || pContext->Connection.CloseWhenDone || pContext->Connection.HasError) {
      return;    // Close connection
    }
  } while (1);
}

/*********************************************************************
*
*       _ShowResourceError
*/
static void _ShowResourceError(OUT_BUFFER_CONTEXT * pOutContext) {
  _SendError(pOutContext, "HTTP/1.1 503 Service unavailable\r\n", WEBS_503_PAGE, 503);
}

/*********************************************************************
*
*       Public code
*
**********************************************************************
*/

/*********************************************************************
*
*       IP_WEBS_GetNumParas
*
*  Function description
*    Returns the number of parameters/value pairs.
*
*  Return value
*    -1  -  No parameter/value pair
*    >0 -  Number of parameter/value pairs
*/
int IP_WEBS_GetNumParas(const char * sParameters) {
  int i;
  int NumParas;
  int Len;

  //
  // Check if string contains at least one parameter value pair.
  //
  Len = strlen(sParameters);
  i = _FindChar(sParameters, '=', Len);
  if (i == Len) {
    return  -1;  // No valid parameter value pair.
  }
  NumParas = 1;
  i        = 0;
  do {
    i = _FindChar(sParameters + i, '&', Len);
    if (i < Len) {
      Len -= i;
      NumParas++;
    }
  } while (i != Len);
  return NumParas;
}

/*********************************************************************
*
*       IP_WEBS_CompareFilenameExt
*
*  Function description
*    Checks if the given filename has the given extension
*    The test is case-sensitive, meaning:
*    IP_WEBS_CompareFilenameExt("Index.html", ".html")           ---> Match
*    IP_WEBS_CompareFilenameExt("Index.htm",  ".html")           ---> Mismatch
*    IP_WEBS_CompareFilenameExt("Index.HTML", ".html")           ---> Mismatch
*    IP_WEBS_CompareFilenameExt("Index.html", ".HTML")           ---> Mismatch
*
*  Parameters
*    sFilename     Null-terminated filename, such as "Index.html"
*    sExtension    Null-terminated filename extension with dot, such as ".html"
*
*  Return value
*     0   match
*  != 0   mismatch
*/
char IP_WEBS_CompareFilenameExt(const char * sFilename, const char * sExt) {
  int r;

  r = _CompareFilenameExt(sFilename, sExt);
  return r;
}

/*********************************************************************
*
*       IP_WEBS_DecodeAndCopyStr
*
*  Function description
*    Checks if a string includes url encoded characters, decodes the
*    characters and copies them into destination buffer.
*    Destination string is 0-terminated.
*    Source and destination buffer can be identical.
*
*     pSrc               | SrcLen | pDest              | DestLen
*    ------------------------------------------------------------
*     "FirstName=J%F6rg" | 16     | "FirstName=Jörg\0" | 15
*     "FirstName=John"   | 14     | "FirstName=John\0" | 15
*/
void IP_WEBS_DecodeAndCopyStr(char * pDest, int DestLen, char * pSrc, int SrcLen) {
  _DecodeAndCopyStr(pDest, DestLen, pSrc, SrcLen);
}

/*********************************************************************
*
*       IP_WEBS_DecodeString
*
*  Function description
*    Checks if a string includes url encoded characters, decodes the
*    characters. Examples:
*    FirstName=John&LastName=Doo   -> FirstName=John&LastName=Doo   // No change, string does not include url encoded characters
*    FirstName=J%F6rg&LastName=Doo -> FirstName=Jörg&LastName=Doo   // Change, one url encoded character decoded
*    Pass=%21%22%24%25123          -> Pass=!"$%123                  // Change, four url encoded characters decoded
*
*  Parameters
*    s: Pointer to a zero-terminated string.
*
*  Return value
*    0  String does not include url encoded characters. No change.
*   >0  Length of the decoded string, including the terminating null character.
*/
int IP_WEBS_DecodeString(const char * s) {
  int i;
  int j;
  char c;
  int Len;
  char * p;

  p = (char*)s;
  //
  // Check if URL encoded characters are included
  //
  Len = strlen(s);
  i = _FindChar(s, '%', Len);
  j = _FindChar(s, '+', Len);
  //
  // Return if no URL encoded characters are included
  //
  if ((i == Len) && (j == Len)) {
    return 0;
  }
  //
  // Decode characters (if required)
  //
  i = 0;
  while(i < Len) {
    c = *s++;
    i++;
    if (c == '%') {
      char c0, c1;
      c0 = *s++;
      c1 = *s++;
      c  = _Hex2Bin(c0) << 4;
      c |= _Hex2Bin(c1);
      goto AddChar;
    }
    if ((c >= 'A') & (c <= 'Z')) {
      goto AddChar;
    }
    if ((c >= 'a') & (c <= 'z')) {
      goto AddChar;
    }
    if ((c >= '0') & (c <= '9')) {
      goto AddChar;
    }
    if (c == '+') {
      c = ' ';
      goto AddChar;
    }
    switch (c) {
    case '.':
    case '-':
    case '_':
    case '*':
    case '(':
    case ')':
    case '!':
    case '\'':
    //
    // URL syntax characters
    //
    case ';':
    case '/':
    case '?':
    case ':':
    case '@':
    case '=':
    case '#':
    case '&':
      goto AddChar;
    }
    break;
AddChar:
    *p++ = c;
  }
  *p = 0;
  return i;
}

/*********************************************************************
*
*       IP_WEBS_GetParaValue
*
*  Function description
*    Parses a string for valid paramter/value pairs and writes
*    results in buffer.
*
*    A valid string is in the following format:
*    <Param0>=<Value0>&<Param1>=<Value1>& ... <Paramn>=<Valuen>
*
*    "FirstName=John&LastName=Doo", 0   -> sPara  = "FirstName", sValue="John"   // Assuming output buffers are big enough
*    "FirstName=John&LastName=Doo", 1   -> sPara  = "LastName", sValue="Doo"     // Assuming output buffers are big enough
*
*  Paramters
*    sBuffer   - [IN]  String that should be parsed
*    ParaNum   - [IN]  Number of parameter/value pair (e.g., 0 for the first paramter/value pair, 1 for the second...)
*    sPara     - [OUT] Pointer to the buffer for paramter name (optional, can be NULL)
*    ParaLen   - [IN]  Size of parameter name buffer
*    sValue    - [OUT] Pointer to the buffer for value (optional, can be NULL)
*    ValueLen  - [IN]  Size of value buffer
*
*  Return value
*    0   OK
*    >0  Error
*/
int IP_WEBS_GetParaValue(const char * sBuffer, int ParaNum, char * sPara, int ParaLen, char * sValue, int ValueLen) {
  int i;
  int Start;
  int End;
  int Len;
  int Pos;
  int NumBytes;

  NumBytes = strlen(sBuffer);
  //
  // Find parameter/value pair start position
  //
  Start = 0;
  for (i = 0; i < ParaNum; i++) {
    Pos = _FindChar(sBuffer + Start, '&', NumBytes);
    Start += Pos + 1; // Pos is the position of "&", we need the position of the following character
    if (Start == NumBytes) {
      WEBS_LOG(("String does not included as many parameters as requested.\n"));
      return 1;
    }
  }
  End = Start;
  Len = NumBytes - Start;
  //
  // Find parameter end
  //
  Pos = _FindChar(sBuffer + Start, '=', Len);
  if (Pos != NumBytes) {
    End += Pos;
  }
  Len = End - Start;
  if (sPara) {
    _DecodeAndCopyStr(sPara, ParaLen, (char*)sBuffer + Start, Len);
  }
  //
  // Find value start
  //
  Start = ++End;           // End is the position of "=", start of value the following character
  if (Start == NumBytes) { // Check if "=" is the last character in string (no value transfered)
    if (sValue) {
      *sValue = 0;
    }
    return 0;
  }
  Len = NumBytes - Start;
  //
  // Find value end
  //
  Pos = _FindChar(sBuffer + Start, '&', Len);
  if (sValue) {
    _DecodeAndCopyStr(sValue, ValueLen, (char*)sBuffer + Start, Pos);
  }
  return 0;
}

/*********************************************************************
*
*       IP_WEBS_SendMem
*/
int  IP_WEBS_SendMem (WEBS_OUTPUT * pOutput, const char * s, unsigned NumBytes) {
  _WriteMem((OUT_BUFFER_CONTEXT *) pOutput, s, NumBytes);
  return 0;
}

/*********************************************************************
*
*       IP_WEBS_SendString
*/
int  IP_WEBS_SendString(WEBS_OUTPUT * pOutput, const char * s) {
  _WriteString((OUT_BUFFER_CONTEXT *) pOutput, s);
  return 0;
}

/*********************************************************************
*
*       IP_WEBS_SendStringEnc
*/
int  IP_WEBS_SendStringEnc(WEBS_OUTPUT * pOutput, const char * s) {
  char c;
  char ac[2];

  while (1) {
    c = *s++;
    if (c == 0) {
      break;
    }
    if (!_IsAlphaNum(c)) {
      _WriteChar((OUT_BUFFER_CONTEXT *) pOutput, '%');
      _StoreUnsigned(&ac[0], c, 16, 2);
      _WriteChar((OUT_BUFFER_CONTEXT *) pOutput, ac[0]);
      _WriteChar((OUT_BUFFER_CONTEXT *) pOutput, ac[1]);
    } else {
      _WriteChar((OUT_BUFFER_CONTEXT *) pOutput, c);
    }
  }
  return 0;
}

/*********************************************************************
*
*       IP_WEBS_SendUnsigned
*/
int IP_WEBS_SendUnsigned(WEBS_OUTPUT * pOutput, unsigned long v, unsigned Base, int NumDigits) {
  _WriteUnsigned((OUT_BUFFER_CONTEXT *) pOutput, v, Base, NumDigits);
  return 0;
}

/*********************************************************************
*
*       _Start
*
*  Function description
*    Thread functionality of the web server.
*    Returns when the connection is closed or a fatal error occurs.
*/
static int  _Start (IP_WEBS_tSend pfSend, IP_WEBS_tReceive pfReceive, void* pConnectInfo, const IP_FS_API * pFS_API, const WEBS_APPLICATION * pApplication, U8 CloseWhenDone) {
  U8 acIn[WEBS_IN_BUFFER_SIZE];
  U8 acOut[WEBS_OUT_BUFFER_SIZE];
#if WEBS_PARA_BUFFER_SIZE
  char acPara[WEBS_PARA_BUFFER_SIZE];
#endif
  WEBS_CONTEXT Context;

  memset(&Context, 0, sizeof(Context));

  Context.pFS_API                  = pFS_API;
  Context.pApplication             = pApplication;
  Context.Connection.pfSend        = pfSend;
  Context.Connection.pfReceive     = pfReceive;
  Context.Connection.pConnectInfo  = pConnectInfo;
  Context.Connection.HTTPVer       = 100;
  Context.Connection.CloseWhenDone = CloseWhenDone;

  Context.InBufferDesc.pConnection = &Context.Connection;
  Context.InBufferDesc.pBuffer     = acIn;
  Context.InBufferDesc.Size        = sizeof(acIn);

  Context.OutBuffer.pConnection    = &Context.Connection;
  Context.OutBuffer.pBuffer        = acOut;
  Context.OutBuffer.BufferSize     = sizeof(acOut);
#if WEBS_PARA_BUFFER_SIZE
  Context.OutBuffer.pPara          = acPara;
#endif

  if (pFS_API) {
    _Process(&Context);
  } else {
    _ShowResourceError(&Context.OutBuffer);
  }
  return 0;
}

/*********************************************************************
*
*       IP_WEBS_Process
*
*  Function description
*    Thread functionality of the web server.
*    Returns when the connection is closed or a fatal error occurs.
*/
int  IP_WEBS_Process (IP_WEBS_tSend pfSend, IP_WEBS_tReceive pfReceive, void* pConnectInfo, const IP_FS_API * pFS_API, const WEBS_APPLICATION * pApplication) {
  return _Start(pfSend, pfReceive, pConnectInfo, pFS_API, pApplication, 0);
}

/*********************************************************************
*
*       IP_WEBS_ProcessLast
*
*  Function description
*    Thread functionality of the web server.
*    This is typically called for the last available connection.
*    In contrast to IP_WEBS_Process, this function closes the connection as soon as the command is completed
*    in order to not block the last connection longer than necessary and avoid Connection-limit errorss.
*/
int  IP_WEBS_ProcessLast(IP_WEBS_tSend pfSend, IP_WEBS_tReceive pfReceive, void* pConnectInfo, const IP_FS_API * pFS_API, const WEBS_APPLICATION * pApplication) {
  return _Start(pfSend, pfReceive, pConnectInfo, pFS_API, pApplication, 1);
}

/*********************************************************************
*
*       IP_WEBS_OnConnectionLimit
*
*  Function description
*    Outputs an error message to the connected client.
*    This function is typically called by the application if the connection limit is reached.
*
*    Pseudo code:
*      //
*      // Call IP_WEBS_Process() or IP_WEBS_ProcessLast() if multiple or just one more connection is available
*      //
*      do {
*        if (NumAvailableConnections > 1) {
*          IP_WEBS_Process();
*          return;
*        } else if (NumAvailableConnections == 1) {
*          IP_WEBS_ProcessLast();
*          return;
*        }
*        Delay();
*      } while (!Timeout)
*      //
*      // No connection available even after waiting => Output error message
*      //
*      IP_WEBS_OnConnectionLimit();  
*/
void IP_WEBS_OnConnectionLimit(IP_WEBS_tSend pfSend, IP_WEBS_tReceive pfReceive, void* pConnectInfo) {
  U8 acOut[WEBS_ERR_BUFFER_SIZE];
  OUT_BUFFER_CONTEXT OutContext;
  CONNECTION_CONTEXT Connection;

  memset(&OutContext, 0, sizeof(OutContext));
  //
  // Save parameters
  //
  Connection.pfSend        = pfSend;
  Connection.pfReceive     = pfReceive;
  Connection.pConnectInfo  = pConnectInfo;

  OutContext.pConnection    = &Connection;
  OutContext.pBuffer        = acOut;
  OutContext.BufferSize     = sizeof(acOut);
  _ShowResourceError(&OutContext);
}

/*********************************************************************
*
*       IP_WEBS_SetFileDateCallback
*
*  Function description
*/
void IP_WEBS_SetFileInfoCallback(IP_WEBS_pfGetFileInfo pf) {
  _pfGetFileInfo = pf;
}

/*************************** end of file ****************************/
