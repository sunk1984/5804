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
File    : IP_FTPServer.c
Purpose : FTPserver add-on.


From RFC 959

5.3.1.  FTP COMMANDS

   The following are the FTP commands:

      USER <SP> <username> <CRLF>
      PASS <SP> <password> <CRLF>
      ACCT <SP> <account-information> <CRLF>
      CWD  <SP> <pathname> <CRLF>
      CDUP <CRLF>
      SMNT <SP> <pathname> <CRLF>
      QUIT <CRLF>
      REIN <CRLF>
      PORT <SP> <host-port> <CRLF>
      PASV <CRLF>
      TYPE <SP> <type-code> <CRLF>
      STRU <SP> <structure-code> <CRLF>
      MODE <SP> <mode-code> <CRLF>
      RETR <SP> <pathname> <CRLF>
      STOR <SP> <pathname> <CRLF>
      STOU <CRLF>
      APPE <SP> <pathname> <CRLF>
      ALLO <SP> <decimal-integer>
          [<SP> R <SP> <decimal-integer>] <CRLF>
      REST <SP> <marker> <CRLF>
      RNFR <SP> <pathname> <CRLF>
      RNTO <SP> <pathname> <CRLF>
      ABOR <CRLF>
      DELE <SP> <pathname> <CRLF>
      RMD  <SP> <pathname> <CRLF>
      MKD  <SP> <pathname> <CRLF>
      PWD  <CRLF>
      LIST [<SP> <pathname>] <CRLF>
      NLST [<SP> <pathname>] <CRLF>
      SITE <SP> <string> <CRLF>
      SYST <CRLF>
      STAT [<SP> <pathname>] <CRLF>
      HELP [<SP> <string>] <CRLF>
      NOOP <CRLF>


4.2.2 Numeric  Order List of Reply Codes

         110 Restart marker reply.
             In this case, the text is exact and not left to the
             particular implementation; it must read:
                  MARK yyyy = mmmm
             Where yyyy is User-process data stream marker, and mmmm
             server's equivalent marker (note the spaces between markers
             and "=").
         120 Service ready in nnn minutes.
         125 Data connection already open; transfer starting.
         150 File status okay; about to open data connection.
         200 Command okay.
         202 Command not implemented, superfluous at this site.
         211 System status, or system help reply.
         212 Directory status.
         213 File status.
         214 Help message.
             On how to use the server or the meaning of a particular
             non-standard command.  This reply is useful only to the
             human user.
         215 NAME system type.
             Where NAME is an official system name from the list in the
             Assigned Numbers document.
         220 Service ready for new user.
         221 Service closing control connection.
             Logged out if appropriate.
         225 Data connection open; no transfer in progress.
         226 Closing data connection.
             Requested file action successful (for example, file
             transfer or file abort).
         227 Entering Passive Mode (h1,h2,h3,h4,p1,p2).
         230 User logged in, proceed.
         250 Requested file action okay, completed.
         257 "PATHNAME" created.

         331 User name okay, need password.
         332 Need account for login.
         350 Requested file action pending further information.

         421 Service not available, closing control connection.
             This may be a reply to any command if the service knows it
             must shut down.
         425 Can't open data connection.
         426 Connection closed; transfer aborted.
         450 Requested file action not taken.
             File unavailable (e.g., file busy).
         451 Requested action aborted: local error in processing.
         452 Requested action not taken.
             Insufficient storage space in system.
         500 Syntax error, command unrecognized.
             This may include errors such as command line too long.
         501 Syntax error in parameters or arguments.
         502 Command not implemented.
         503 Bad sequence of commands.
         504 Command not implemented for that parameter.
         530 Not logged in.
         532 Need account for storing files.
         550 Requested action not taken.
             File unavailable (e.g., file not found, no access).
         551 Requested action aborted: page type unknown.
         552 Requested file action aborted.
             Exceeded storage allocation (for current directory or
             dataset).
         553 Requested action not taken.
             File name not allowed.


    Command-Reply Sequences

         In this section, the command-reply sequence is presented.  Each
         command is listed with its possible replies; command groups are
         listed together.  Preliminary replies are listed first (with
         their succeeding replies indented and under them), then
         positive and negative completion, and finally intermediary
         replies with the remaining commands from the sequence
         following.  This listing forms the basis for the state
         diagrams, which will be presented separately.

            Connection Establishment
               120
                  220
               220
               421
            Login
               USER
                  230
                  530
                  500, 501, 421
                  331, 332
               PASS
                  230
                  202
                  530
                  500, 501, 503, 421
                  332
               ACCT
                  230
                  202
                  530
                  500, 501, 503, 421
               CWD
                  250
                  500, 501, 502, 421, 530, 550
               CDUP
                  200
                  500, 501, 502, 421, 530, 550
               SMNT
                  202, 250
                  500, 501, 502, 421, 530, 550
            Logout
               REIN
                  120
                     220
                  220
                  421
                  500, 502
               QUIT
                  221
                  500
            Transfer parameters
               PORT
                  200
                  500, 501, 421, 530
               PASV
                  227
                  500, 501, 502, 421, 530
               MODE
                  200
                  500, 501, 504, 421, 530
               TYPE
                  200
                  500, 501, 504, 421, 530
               STRU
                  200
                  500, 501, 504, 421, 530
            File action commands
               ALLO
                  200
                  202
                  500, 501, 504, 421, 530
               REST
                  500, 501, 502, 421, 530
                  350
               STOR
                  125, 150
                     (110)
                     226, 250
                     425, 426, 451, 551, 552
                  532, 450, 452, 553
                  500, 501, 421, 530
               STOU
                  125, 150
                     (110)
                     226, 250
                     425, 426, 451, 551, 552
                  532, 450, 452, 553
                  500, 501, 421, 530
               RETR
                  125, 150
                     (110)
                     226, 250
                     425, 426, 451
                  450, 550
                  500, 501, 421, 530
               LIST
                  125, 150
                     226, 250
                     425, 426, 451
                  450
                  500, 501, 502, 421, 530
               NLST
                  125, 150
                     226, 250
                     425, 426, 451
                  450
                  500, 501, 502, 421, 530
               APPE
                  125, 150
                     (110)
                     226, 250
                     425, 426, 451, 551, 552
                  532, 450, 550, 452, 553
                  500, 501, 502, 421, 530
               RNFR
                  450, 550
                  500, 501, 502, 421, 530
                  350
               RNTO
                  250
                  532, 553
                  500, 501, 502, 503, 421, 530
               DELE
                  250
                  450, 550
                  500, 501, 502, 421, 530
               RMD
                  250
                  500, 501, 502, 421, 530, 550
               MKD
                  257
                  500, 501, 502, 421, 530, 550
               PWD
                  257
                  500, 501, 502, 421, 550
               ABOR
                  225, 226
                  500, 501, 502, 421
            Informational commands
               SYST
                  215
                  500, 501, 502, 421
               STAT
                  211, 212, 213
                  450
                  500, 501, 502, 421, 530
               HELP
                  211, 214
                  500, 501, 502, 421
            Miscellaneous commands
               SITE
                  200
                  202
                  500, 501, 530
               NOOP
                  200
                  500 421

         REPRESENTATION TYPE (TYPE)

            The argument specifies the representation type as described
            in the Section on Data Representation and Storage.  Several
            types take a second parameter.  The first parameter is
            denoted by a single Telnet character, as is the second
            Format parameter for ASCII and EBCDIC; the second parameter
            for local byte is a decimal integer to indicate Bytesize.
            The parameters are separated by a <SP> (Space, ASCII code
            32).

            The following codes are assigned for type:

                         \    /
               A - ASCII |    | N - Non-print
                         |-><-| T - Telnet format effectors
               E - EBCDIC|    | C - Carriage Control (ASA)
                         /    \
               I - Image

               L <byte size> - Local byte Byte size



  **************************************
  (http://en.wikipedia.org/wiki/File_system_permissions)

    Unix file system permission notation

    Symbolic notation

    This scheme represents permissions as a series of 10 characters.

    The first character indicates the file type:
        * - denotes a regular file
        * d denotes a directory
        * b denotes a block special file
        * c denotes a character special file
        * l denotes a symbolic link
        * p denotes a named pipe
        * s denotes a domain socket

    Each class of permissions is represented by three characters.
    The first set of characters represents the user class.
    The second set represents the group class.
    The third and final set of three characters represents the others class.

    Each of the three characters represent the read, write, and execute permissions respectively:
        * r if the read bit is set, - if it is not.
        * w if the write bit is set, - if it is not.
        * x if the execute bit is set, - if it is not.

    The following are some examples of symbolic notation:
        * -rwxr-xr-x for a regular file whose user class has full permissions and whose group and others classes have only the read and execute permissions.
        * crw-rw-r-- for a character special file whose user and group classes have the read and write permissions and whose others class has only the read permission.
        * dr-x------ for a directory whose user class has read and execute permissions and whose group and others classes have no permissions.
  **************************************

---------------------------END-OF-HEADER------------------------------
*/

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "IP_FTPServer.h"
#include "SEGGER.h"
#include "FTPS_Conf.h"

/*********************************************************************
*
*       defines, configurable
*
**********************************************************************
*/
#ifndef   FTPS_WARN
  #define FTPS_WARN(p)
#endif

#ifndef   FTPS_LOG
  #define FTPS_LOG(p)
#endif

#ifndef   FTPS_AUTH_BUFFER_SIZE
  #define FTPS_AUTH_BUFFER_SIZE   32
#endif

#ifndef   FTPS_BUFFER_SIZE
  #define FTPS_BUFFER_SIZE       512
#endif

#ifndef   FTPS_MAX_PATH
  #define FTPS_MAX_PATH          128
#endif

#ifndef   FTPS_MAX_PATH_DIR
  #define FTPS_MAX_PATH_DIR       64
#endif

#ifndef   FTPS_ERR_BUFFER_SIZE
  #define FTPS_ERR_BUFFER_SIZE  (FTPS_BUFFER_SIZE / 2)    // Used in case of connection limit only
#endif

#ifndef   FTPS_SIGN_ON_MSG
  #define FTPS_SIGN_ON_MSG   "Embedded ftp server"
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

/*********************************************************************
*
*       Types
*
**********************************************************************
*/

typedef struct {
  const IP_FTPS_API * pIP_API;
  FTPS_SOCKET Sock;
  U8 * pBuffer;                  // Pointer to the data buffer
  int Size;                      // Size of buffer
  int Cnt;                       // Number of bytes in buffer
  int RdOff;
} IN_BUFFER_DESC;

typedef struct {
  const IP_FTPS_API * pIP_API;
  FTPS_SOCKET Sock;
  U8 * pBuffer;                  // Pointer to the data buffer
  int BufferSize;                // Size of buffer
  int Cnt;                       // Number of bytes in buffer
} OUT_BUFFER_CONTEXT;

typedef struct {
  const IP_FS_API        * pFS_API;       // File system info
  const FTPS_APPLICATION * pApplication;
  IN_BUFFER_DESC           InBufferDesc;
  OUT_BUFFER_CONTEXT       DataOut;
  OUT_BUFFER_CONTEXT       CtrlOut;
  char                     acCurDir[FTPS_MAX_PATH_DIR];  // "/" or "/Dir/" or "/Dir/Sub/..."
  int                      UserId;                       // 0: No user, < 0 user known, waiting for password, > 0 valid user
} FTPS_CONTEXT;

/*********************************************************************
*
*       static const
*
**********************************************************************
*/

static const char _aV2C[16] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' };

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
*       _GenerateAbsFilename
*
*  Function description
*    Makes sure the file name in the buffer is absolute.
*    If the file name is relative, it is converted into an absolute file name.
*
*  Sample:
*    CurrentDir="\bin\" Filename ="file.txt" ->  "\bin\file.txt"
*    CurrentDir="\bin\" Filename ="\file.txt" ->  "\file.txt"
*
*  Return value
*    0    O.K.
*  !=0    Error
*/
static int _GenerateAbsFilename(FTPS_CONTEXT * pContext, char * sFileName, int BufferSize) {
  int i;
  int LenFileName;

  if (*sFileName != '/') {            // Relative file name ?
    //
    // Add current directory to file name
    //
    i = strlen(pContext->acCurDir);
    LenFileName = strlen(sFileName);
    if (LenFileName + i >= BufferSize) {
      return 1;      // Error: Buffer too small
    }
    memmove(sFileName + i, sFileName, LenFileName + 1);
    memcpy (sFileName, pContext->acCurDir, i);
  }
  return 0;
}

/**************************************************************************************************************************************************************
*
*       Output related code
*/

/*********************************************************************
*
*       _Flush
*/
static int _Flush(OUT_BUFFER_CONTEXT * pOutContext) {
  int Len;
  int r;

  r = 0;
  Len = pOutContext->Cnt;
  if (Len) {
    r = pOutContext->pIP_API->pfSend(pOutContext->pBuffer, Len, pOutContext->Sock);
    pOutContext->Cnt = 0;
  }
  return r;
}

/*********************************************************************
*
*       _WriteChar
*/
static int _WriteChar(OUT_BUFFER_CONTEXT * pOutContext, char c) {
  int Len;
  int r;

  Len = pOutContext->Cnt;
  *(pOutContext->pBuffer + Len) = c;
  Len++;
  pOutContext->Cnt = Len;
  if (Len == pOutContext->BufferSize) {
    r = _Flush(pOutContext);
  }
  return r;
}

/*********************************************************************
*
*       _WriteString
*/
static int _WriteString(OUT_BUFFER_CONTEXT * pOutContext, const char *s) {
  char c;
  int r;

  do {
    c = *s++;
    if (c == 0) {
      break;
    }
    r = _WriteChar(pOutContext, c);
    if (r == -1) {
      break;
    }
  } while (1);
  return r;
}

/*********************************************************************
*
*       _WriteMem
*/
static int _WriteMem(OUT_BUFFER_CONTEXT * pOutContext, const char *pSrc, int NumBytes) {
  char c;
  int r;

  while (NumBytes-- > 0) {
    c = *pSrc++;
    r = _WriteChar(pOutContext, c);
    if (r == -1) {
      break;
    }
  }
  return r;
}

/*********************************************************************
*
*       _WriteUnsigned
*/
static int _WriteUnsigned(OUT_BUFFER_CONTEXT * pOutContext, U32 v, unsigned Base, int NumDigits) {
  unsigned Div;
  U32 Digit;
  int r;

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
    r = _WriteChar(pOutContext, _aV2C[Div]);
    if (r < 0) {
      return r;
    }
    Digit /= Base;
  } while (Digit);
  return r;
}

/*********************************************************************
*
*       _WriteDataPort
*/
static int _WriteDataPort(FTPS_CONTEXT * pContext, const char * pData, int NumBytes) {
  return _WriteMem(&pContext->DataOut, pData, NumBytes);
}

/*********************************************************************
*
*       _WriteStringDataPort
*/
static int _WriteStringDataPort(FTPS_CONTEXT * pContext, const char * s) {
  return _WriteString(&pContext->DataOut, s);
}

/*********************************************************************
*
*       _WriteUnsignedDataPort
*/
static int _WriteUnsignedDataPort(FTPS_CONTEXT * pContext,U32 v, unsigned Base, int NumDigits) {
  return _WriteUnsigned(&pContext->DataOut, v, Base, NumDigits);
}

/*********************************************************************
*
*       _WriteFileTimeDataPort
*
*  Function description
*    Writes the file time to the data port.
*
*    Format: " Month Day Year " for example " Jan 1 00:00 " or " Jan 1  1980 "
*/
static void _WriteFileTimeDataPort(FTPS_CONTEXT * pContext, U32 FileTime) {
  int Day;
  int Month;
  int Year;
  int Hour;
  int Minute;
  int SystemTime;
  int  CurrYear;
  const char * s;
  const char * asMonth[12] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

  _WriteStringDataPort(pContext, " ");
  Month = (FileTime >> 21) & 0xF;
  s = asMonth[Month - 1];
  _WriteStringDataPort(pContext, s);
  _WriteStringDataPort(pContext, " ");
  Day   = (FileTime >> 16) & 0x1F;
  _WriteUnsignedDataPort(pContext, Day, 10, 0);
  _WriteStringDataPort(pContext, " ");
  SystemTime = pContext->pApplication->pfGetTimeDate();
  CurrYear = (SystemTime >> 25) & 0x7F;
  Year = (FileTime >> 25) & 0x7F;
  if (CurrYear != Year) {
    Year += 1980;
    _WriteStringDataPort(pContext, " ");
    _WriteUnsignedDataPort(pContext, Year, 10, 0);
    _WriteStringDataPort(pContext, " ");
  } else {
    Hour = (FileTime >> 11) & 0x1F;
    if (Hour < 10) {
      _WriteUnsignedDataPort(pContext, 0, 10, 0);
    }
    _WriteUnsignedDataPort(pContext, Hour, 10, 0);
    _WriteStringDataPort(pContext, ":");
    Minute = (FileTime >> 5) & 0x3F;
    if (Minute < 10) {
      _WriteUnsignedDataPort(pContext, 0, 10, 0);
    }
    _WriteUnsignedDataPort(pContext, Minute, 10, 0);
    _WriteStringDataPort(pContext, " ");
  }
}

/*********************************************************************
*
*       _SendFTPString
*/
static int _SendFTPString(OUT_BUFFER_CONTEXT * pOutContext, U32 Num, const char * s) {
  const char * sLine;
  char c;
  U32 Cnt;

  while (1) {       // Line loop
    Cnt = 0;
    sLine = s;
    while (1) {     // Character loop
      c = *s++;
      if (c == 0) {
        goto End;
      }
      if (c == CR) {
        c = *s;
        if (c == LF) {
        goto End;
        }
      }
      Cnt++;
    }
  }
End:
  _WriteUnsigned(pOutContext, Num, 10, 0);
  _WriteChar(pOutContext, ' ');
  _WriteMem(pOutContext, sLine, Cnt);
  _WriteChar(pOutContext, CR);
  _WriteChar(pOutContext, LF);
  return _Flush(pOutContext);
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


  if (pBufferDesc->Sock == NULL) {
    return -1;      // Error occurred.
  }
  if (pBufferDesc->Cnt == 0) {
    pBufferDesc->RdOff = 0;
  }
  Cnt   = pBufferDesc->Cnt;
  Size  = pBufferDesc->Size;
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
  i = pBufferDesc->pIP_API->pfReceive(p + WrOff, NumBytes, pBufferDesc->Sock);
  if (i > 0) {
    pBufferDesc->Cnt = Cnt + i;
  }
  return i;
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
    FTPS_WARN(("_EatChars(): Trying to eat more bytes than in buffer"));
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
*       _GetDec
*/
static unsigned _GetDec(IN_BUFFER_DESC * pBufferDesc) {
  unsigned v;
  unsigned Off;
  U8 c;

  Off = 0;
  v = 0;
  while (1) {
    c = _GetCharND(pBufferDesc, Off);
    if ((c < '0') || (c > '9')) {
      _EatChars(pBufferDesc, Off);
      return v;
    }
    Off++;
    v = v * 10 + c - '0';
  }
}

/*********************************************************************
*
*       _GetLine
*
*  Function description
*    Get & remove a line from buffer
*/
static int _GetLine(IN_BUFFER_DESC * pBufferDesc, char * s, int NumBytes) {
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
      FTPS_WARN(("Line not terminated"));
      return 0;
    }
    c = *(pBuffer + RdOff);
    if ((c < 32)  || (c >= 127)) {
      break;
    }
    *s++ = c;
    i++;
    if (i >= NumBytes) {
      FTPS_WARN(("Error, string does not fit"));
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
  *s = 0;
  return i;
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
*       _OpenFile
*
*  Function description
*/
static void * _OpenFile(FTPS_CONTEXT   * pContext, const char * s) {
  return pContext->pFS_API->pfOpenFile(s);
}

/*********************************************************************
*
*       _CloseFile
*
*  Function description
*/
static int _CloseFile(FTPS_CONTEXT   * pContext, void * hFile) {
  return pContext->pFS_API->pfCloseFile(hFile);
}

/*********************************************************************
*
*       _DeleteFile
*
*  Function description
*/
static int _DeleteFile(FTPS_CONTEXT   * pContext, void * hFile) {
  return (int)pContext->pFS_API->pfDeleteFile((const char*)hFile);
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
    FTPS_WARN(("_EatBytes() wants to swallow too many bytes!"));
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
*       _Disconnect
*
*  Function description
*/
static void _Disconnect(FTPS_CONTEXT   * pContext) {
  FTPS_SOCKET DataSock;
  DataSock = pContext->DataOut.Sock;
  if (DataSock) {
    pContext->DataOut.pIP_API->pfDisconnect(DataSock);
    pContext->DataOut.Sock = 0;
  }
}

/*********************************************************************
*
*       _ReceiveFile
*
*  Function description
*/
static int _ReceiveFile(FTPS_CONTEXT * pContext, void * hFile) {
  int  NumBytesAtOnce;
  int  FilePos;
  int  r;

  FilePos = 0;
  NumBytesAtOnce = pContext->InBufferDesc.Size;
  while (1) {
    r = pContext->DataOut.pIP_API->pfReceive(pContext->InBufferDesc.pBuffer, NumBytesAtOnce, pContext->DataOut.Sock);
    if ((r == -1) || (r == 0)) {
      break;
    }
    pContext->pFS_API->pfWriteAt(hFile, pContext->InBufferDesc.pBuffer, FilePos, r);
    FilePos += r;
  }
  return r;
}

/*********************************************************************
*
*       _SendFile
*
*  Function description
*/
static int _SendFile(FTPS_CONTEXT * pContext, void * hFile) {
  int FileLen;
  int FilePos;
  int NumBytesAtOnce;
  OUT_BUFFER_CONTEXT * pOutContext;
  int r;

  pOutContext = &pContext->DataOut;
  FileLen =  pContext->pFS_API->pfGetLen(hFile);
  FilePos = 0;
  while (FileLen > 0) {
    //
    // Read as much as we can (based on free space in buffer and remaining file size)
    //
    NumBytesAtOnce = pContext->DataOut.BufferSize;
    if (NumBytesAtOnce > FileLen) {
      NumBytesAtOnce = FileLen;
    }
    pContext->pFS_API->pfReadAt(hFile, pContext->DataOut.pBuffer, FilePos, NumBytesAtOnce);
    FilePos += NumBytesAtOnce;
    FileLen -= NumBytesAtOnce;
    r = pOutContext->pIP_API->pfSend(pContext->DataOut.pBuffer, NumBytesAtOnce, pOutContext->Sock);
    if (r == -1) {
      return -1;
    }
  }
  return 0;
}

/*********************************************************************
*
*       _ExecCDUP
*
*  Function description
*    Execute CDUP command
*    Changes pContext->acCurDir.
*
*  Add. information
*    RFC 959 says:
*         CHANGE TO PARENT DIRECTORY (CDUP)
*
*           This command is a special case of CWD, and is included to
*           simplify the implementation of programs for transferring
*           directory trees between operating systems having different
*           syntaxes for naming the parent directory.  The reply codes
*           shall be identical to the reply codes of CWD.
*
*  Sample:
*    "\dir\sub\"  ->  "\dir\"
*    "\dir\"      ->  "\"
*    "\"          ->  "\"
*
*/
static int _ExecCDUP(FTPS_CONTEXT * pContext) {
  char c;
  char * s;
  int i;
  int perm;

  c = 0;
  s = pContext->acCurDir;
  i = strlen(pContext->acCurDir) - 2;
  while (i >= 0) {
    c = *(s + i);
    if (c == '/') {
      c = *(s + i + 1);      // Save character before replacing it
      *(s + i + 1) = '\0';
      break;
    }
    i--;
  }
  //
  // Check permission to this directory
  //
  perm = pContext->pApplication->pAccess->pfGetDirInfo(pContext->UserId, pContext->acCurDir, NULL, 0);
  if ((perm & IP_FTPS_PERM_VISIBLE) == 0) {
    *(s + i + 1) = c;       // Replace character
    _SendFTPString(&pContext->CtrlOut, 550, "Requested action not taken.");
    return 1;               // No permission to read this directory
  }
  _SendFTPString(&pContext->CtrlOut, 200, "CDUP command successful.");
  return 0;
}

/*********************************************************************
*
*       _ExecCWD
*
*  Function description
*    Execute CWD command: Change current working directory
*
*  Return value
*    0    User invalid or unknown
*  > 0    User valid
*  < 0    Error
*
*  Add. information
*    RFC 959 says:
*         CHANGE WORKING DIRECTORY (CWD)
*
*            This command allows the user to work with a different
*            directory or dataset for file storage or retrieval without
*            altering his login or accounting information.  Transfer
*            parameters are similarly unchanged.  The argument is a
*            pathname specifying a directory or other system dependent
*            file group designator.
*
*/
static int _ExecCWD(FTPS_CONTEXT * pContext) {
  char acDirName[64];
  U32  LenDirName;
  U32  i;
  const char * s;

  //
  // Get directory name.
  //
  _EatWhite(&pContext->InBufferDesc);
  _GetLine(&pContext->InBufferDesc, &acDirName[0], sizeof(acDirName));
  _EatLine(&pContext->InBufferDesc);
  LenDirName = strlen(acDirName);
  if (LenDirName == 2) {
    if (acDirName[0] == '.') {
      if (acDirName[1] == '.') {
        return _ExecCDUP(pContext);
      }
    }
  }
  //
  // Handle absolute name, such as "/bin" or "/bin/sub" or "/bin/sub/"
  //
  if (acDirName[0] == '/') {
    if (LenDirName >= sizeof(pContext->acCurDir)) {
      goto ErrDirNameTooLong;
    }
  }
  //
  // Handle relative name, such as "sub"
  //
  else {
    i = strlen(pContext->acCurDir);
    if ((LenDirName + i) >= sizeof(pContext->acCurDir)) {
      goto ErrDirNameTooLong;
    }
    memmove(&acDirName[i], &acDirName[0], LenDirName);
    memcpy(&acDirName[0], pContext->acCurDir, i);
    acDirName[LenDirName + i] = 0;
  }
  //
  // Check if this directory exists and is valid for current user
  //
  s = acDirName;
  if (*s == '/') {
    s++;
  }
  i = pContext->pApplication->pAccess->pfGetDirInfo(pContext->UserId, s, NULL, 0);
  if ((i & IP_FTPS_PERM_READ) == 0) {

  _SendFTPString(&pContext->CtrlOut, 550, "Access denied.");
    return 0;    // No permission to read this directory
  }
  strcpy(pContext->acCurDir, acDirName);
  //
  // Make sure directory is valid: Last char needs to be '/'
  //
  LenDirName = strlen(acDirName);
  if (acDirName[LenDirName - 1] != '/') {
    if (LenDirName >= sizeof(acDirName) - 1) {
      goto ErrDirNameTooLong;
    }
    strcat(pContext->acCurDir, "/");
  }
  //
  // Send reply
  //
  _WriteString(&pContext->CtrlOut, "250 directory changed to ");
  _WriteString(&pContext->CtrlOut, pContext->acCurDir);
  _WriteString(&pContext->CtrlOut, "\r\n");
  _Flush(&pContext->CtrlOut);
  return 0;

ErrDirNameTooLong:
  _SendFTPString(&pContext->CtrlOut, 550, "Directory name size too long");
  return 0;
}

/*********************************************************************
*
*       _ExecDELE
*
*  Function description
*    Execute DELE command: Delete
*
*  Return value
*  >= 0    File deleted
*  <  0    Error
*
*  Add. information
*    RFC 959 says:
*         DELETE (DELE)
*
*           This command causes the file specified in the pathname to be
*           deleted at the server site.  If an extra level of protection
*           is desired (such as the query, "Do you really wish to
*           delete?"), it should be provided by the user-FTP process.
*
*/
static int _ExecDELE(FTPS_CONTEXT * pContext) {
  char acFilename[64];
  int i;
  int LenFileName;

  _EatWhite(&pContext->InBufferDesc);
  _GetLine(&pContext->InBufferDesc, &acFilename[0], sizeof(acFilename));
  _EatLine(&pContext->InBufferDesc);
  //
  // Check if we have write permission
  //
  i = pContext->pApplication->pAccess->pfGetDirInfo(pContext->UserId, pContext->acCurDir, NULL, 0);
  if ((i & IP_FTPS_PERM_WRITE) == 0) {
    _SendFTPString(&pContext->CtrlOut, 553, "Requested action not taken - Not allowed.");
    return 0;   // No write permission for this directory
  }
  if (acFilename[0] != '/') {            // Relative file name ?
    //
    // Add current directory to file name
    //
    i = strlen(pContext->acCurDir);
    LenFileName = strlen(acFilename);
    if (LenFileName + i >= sizeof(acFilename)) {
      _SendFTPString(&pContext->CtrlOut, 550, "Filename size too long");
      return 1;
    }
    memmove(&acFilename[i], &acFilename[0], LenFileName + 1);
    memcpy(&acFilename[0], pContext->acCurDir, i);
  }
  i = _DeleteFile(pContext, &acFilename[0]);
  if (i == -1) {
    _SendFTPString(&pContext->CtrlOut, 550, "Requested action not taken.");
  } else {
    _SendFTPString(&pContext->CtrlOut, 250, "File removed!");
  }
  return 0;
}

/*********************************************************************
*
*       _cbList
*/
static void _cbList (void * pvoidContext, void * pFileEntry) {
  char ac[64];
  FTPS_CONTEXT * pContext;
  int FileLen;
  U32 FileSize;
  U32 FileTime;
  U32 IsDir;
  int i;
  pContext = (FTPS_CONTEXT *)pvoidContext;

  pContext->pFS_API->pfGetDirEntryFileName(pFileEntry, ac, sizeof(ac));
  IsDir = pContext->pFS_API->pfGetDirEntryAttributes(pFileEntry);
  if (IsDir) {
    i = pContext->pApplication->pAccess->pfGetDirInfo(pContext->UserId, ac, NULL, 0);
    if ((i & IP_FTPS_PERM_VISIBLE) == 0) {
      return;    // No permission to see this directory
    }
    _WriteStringDataPort(pContext, "drw-r--r--   1 root ");
  } else {
    _WriteStringDataPort(pContext, "-rw-r--r--   1 root ");
  }
  FileLen = pContext->pFS_API->pfGetDirEntryFileSize(pFileEntry, &FileSize);
  _WriteUnsignedDataPort(pContext, FileLen, 10, 0);
  FileTime = pContext->pFS_API->pfGetDirEntryFileTime(pFileEntry);
  if ((FileTime == 0x00210000) || (FileTime == 0x0)) {  // Check if timestamp of file correlates with the MSDOS file system initialization date/time (1980-01-01 00:00)
    _WriteStringDataPort(pContext, " Jan  1  1980 ");
  } else {
    _WriteFileTimeDataPort(pContext, FileTime);
  }
  _WriteStringDataPort(pContext, &ac[0]);
  _WriteDataPort(pContext, "\r\n", 2);
}

/*********************************************************************
*
*       _ExecLIST
*
*  Function description
*    Execute LIST command: List
*
*  Add. information
*    RFC 959 says:
*         LIST (LIST)
*
*           This command causes a list to be sent from the server to the
*           passive DTP.  If the pathname specifies a directory or other
*           group of files, the server should transfer a list of files
*           in the specified directory.  If the pathname specifies a
*           file then the server should send current information on the
*           file.  A null argument implies the user's current working or
*           default directory.
*/
static int _ExecLIST(FTPS_CONTEXT * pContext) {
  int r;

  _EatLine(&pContext->InBufferDesc);
  _SendFTPString(&pContext->CtrlOut, 150, "File status okay; about to open data connection.");
  pContext->pFS_API->pfForEachDirEntry (pContext, pContext->acCurDir, _cbList);
  r = _Flush(&pContext->DataOut);
  if (r == -1) {
    _SendFTPString(&pContext->CtrlOut, 426, "Connection closed; transfer aborted.");
  } else {
    _SendFTPString(&pContext->CtrlOut, 226, "Closing data connection. Requested file action successful.");
  }
  _Disconnect(pContext);
  return 0;
}

/*********************************************************************
*
*       _ExecMKD
*
*  Function description
*    Execute MKD command: Make directory
*
*  Add. information
*    RFC 959 says:
*         MAKE DIRECTORY (MKD)
*
*           This command causes the directory specified in the pathname
*           to be created as a directory (if the pathname is absolute)
*           or as a subdirectory of the current working directory (if
*           the pathname is relative).
*/
static int _ExecMKD(FTPS_CONTEXT * pContext) {
  char acDirname[64];
  int r;

  _EatWhite(&pContext->InBufferDesc);
  _GetLine(&pContext->InBufferDesc, &acDirname[0], sizeof(acDirname));
  _EatLine(&pContext->InBufferDesc);
  if (_GenerateAbsFilename(pContext, &acDirname[0], sizeof(acDirname))) {
    _SendFTPString(&pContext->CtrlOut, 550, "Dir name too long.");
    return 1;
  }
  r = pContext->pFS_API->pfMKDir(acDirname);
  if (r < 0) {
    _SendFTPString(&pContext->CtrlOut, 550, "Requested action not taken.");
  } else {
    _SendFTPString(&pContext->CtrlOut, 227, "Directory successfully created.");
  }
  return 0;
}

/*********************************************************************
*
*       _cbNLST
*/
static void _cbNLST (void * pvoidContext, void * pFileEntry) {
  char ac[64];
  FTPS_CONTEXT * pContext;
  U32 IsDir;
  int i;
  pContext = (FTPS_CONTEXT *)pvoidContext;

  pContext->pFS_API->pfGetDirEntryFileName(pFileEntry, ac, sizeof(ac));
  IsDir = pContext->pFS_API->pfGetDirEntryAttributes(pFileEntry);
  if (IsDir) {
    i = pContext->pApplication->pAccess->pfGetDirInfo(pContext->UserId, ac, NULL, 0);
    if ((i & IP_FTPS_PERM_VISIBLE) == 0) {
      return;    // No permission to see this directory
    }
  }
  _WriteStringDataPort(pContext, &ac[0]);
  _WriteDataPort(pContext, "\r\n", 2);
}

/*********************************************************************
*
*       _ExecNLST
*
*  Function description
*    Execute NLST command: Name list
*
*  Add. information
*    RFC 959 says:
*         NAME LIST (NLST)
*
*            This command causes a directory listing to be sent from
*            server to user site.  The pathname should specify a
*            directory or other system-specific file group descriptor; a
*            null argument implies the current directory.  The server
*            will return a stream of names of files and no other
*            information.
*/
static int _ExecNLST(FTPS_CONTEXT * pContext) {
  int r;

  _EatLine(&pContext->InBufferDesc);
  _SendFTPString(&pContext->CtrlOut, 150, "File status okay; about to open data connection.");
  pContext->pFS_API->pfForEachDirEntry (pContext, pContext->acCurDir, _cbNLST);
  r = _Flush(&pContext->DataOut);
  if (r == -1) {
    _SendFTPString(&pContext->CtrlOut, 426, "Connection closed; transfer aborted.");
  } else {
    _SendFTPString(&pContext->CtrlOut, 226, "Closing data connection. Requested file action successful.");
  }
  _Disconnect(pContext);
  return 0;
}

/*********************************************************************
*
*       _ExecPASS
*
*  Function description
*    Execute PASS command: Password
*
*  Return value
*    0    UserID invalid or unknown
*  < 0    UserID, no password required
*  > 0    UserID, password required
*
*  Add. information
*    RFC 959 says:
*         Password (PASS)
*
*            The argument field is a Telnet string specifying the user's
*            password.  This command must be immediately preceded by the
*            user name command, and, for some sites, completes the user's
*            identification for access control.  Since password
*            information is quite sensitive, it is desirable in general
*            to "mask" it or suppress typeout.  It appears that the
*            server has no foolproof way to achieve this.  It is
*            therefore the responsibility of the user-FTP process to hide
*            the sensitive password information.
*/
static int _ExecPASS(FTPS_CONTEXT * pContext) {
  FTPS_ACCESS_CONTROL * pAccess;
  char acPass[32] = {0};
  int r;

  if (pContext->UserId == 0) {
    _SendFTPString(&pContext->CtrlOut, 530, "Login incorrect.");
  }
  if (pContext->UserId > 0) {
    _SendFTPString(&pContext->CtrlOut, 230, "User logged in, proceed.");
    return 0;
  } else {
    pAccess = pContext->pApplication->pAccess;
    _GetLine(&pContext->InBufferDesc, acPass, sizeof(acPass));
    _EatLine(&pContext->InBufferDesc);
    r = pAccess->pfCheckPass(-pContext->UserId, acPass);
    if (r == 0) {
      _SendFTPString(&pContext->CtrlOut, 230, "User logged in, proceed.");
      pContext->UserId = -pContext->UserId;
    } else {
      _SendFTPString(&pContext->CtrlOut, 530, "Login incorrect.");
    }
    return r;
  }
}

/*********************************************************************
*
*       _ExecPASV
*
*  Function description
*    Execute PASV command: Passiv
*
*  Return value
*     0    OK
*  != 0    Error
*
*  Add. information
*    RFC 959 says:
*         Passiv (PASV)
*
*            This command requests the server-DTP to "listen" on a data
*            port (which is not its default data port) and to wait for a
*            connection rather than initiate one upon receipt of a
*            transfer command.  The response to this command includes the
*            host and port address this server is listening on.
*/
static int _ExecPASV(FTPS_CONTEXT * pContext) {
  OUT_BUFFER_CONTEXT * pOutContext;
  U16 Port;
  U8 acIPAddr[4];
  int i;

  pOutContext = &pContext->CtrlOut;
  _EatLine(&pContext->InBufferDesc);
  _Disconnect(pContext);
  //
  // Create data socket and connect to "Port"
  //
  pContext->DataOut.Sock = pContext->DataOut.pIP_API->pfListen(pContext->CtrlOut.Sock, &Port, &acIPAddr[0]);
  if (pContext->DataOut.Sock == NULL) {
    _SendFTPString(&pContext->CtrlOut, 530, "Could not create socket!");
    return 1;
  }
  //
  // Send data connection info back to client
  //
  _WriteString(pOutContext, "227 Entering Passive Mode (");
  for (i = 0; i < 4; i++) {
    _WriteUnsigned(pOutContext, acIPAddr[i], 10, 0);
    _WriteChar    (pOutContext, ',');
  }
  _WriteUnsigned(pOutContext, Port >> 8, 10, 0);
  _WriteChar    (pOutContext, ',');
  _WriteUnsigned(pOutContext, Port & 255, 10, 0);
  _WriteString  (pOutContext, ")\r\n" );
  _Flush(pOutContext);
  //
  // Wait for client to connect on data port
  //
  if (pContext->DataOut.pIP_API->pfAccept(pContext->CtrlOut.Sock, &pContext->DataOut.Sock)) {
    return 1;
  }
  return 0;
}

/*********************************************************************
*
*       _ExecPORT
*
*  Function description
*    Execute PORT command: Data port
*
*  Return value
*     0    OK
*  != 0    Error
*
*  Add. information
*    RFC 959 says:
*         Data port (PORT)
*
*            The argument is a HOST-PORT specification for the data port
*            to be used in data connection.  There are defaults for both
*            the user and server data ports, and under normal
*            circumstances this command and its reply are not needed.  If
*            this command is used, the argument is the concatenation of a
*            32-bit internet host address and a 16-bit TCP port address.
*            This address information is broken into 8-bit fields and the
*            value of each field is transmitted as a decimal number (in
*            character string representation).  The fields are separated
*            by commas.  A port command would be:
*
*               PORT h1,h2,h3,h4,p1,p2
*
*            where h1 is the high order 8 bits of the internet host
*            address.
*/
static int _ExecPORT(FTPS_CONTEXT * pContext) {
  U8  c;
  U16 Port;
  int i;

  //
  // Read IP Addr consisting of 4 comma-separated decimal numbers such as "192,168,84,2,"
  //
  for (i = 0; i < 4; i++) {
    _GetDec(&pContext->InBufferDesc);
    c = _GetChar(&pContext->InBufferDesc);
    if (c != ',') {
      return -1;            // Error
    }
  }
  //
  // Read Port Number
  //
  Port = _GetDec(&pContext->InBufferDesc) << 8;
  c = _GetChar(&pContext->InBufferDesc);
  if (c != ',') {
    return -1;            // Error
  }
  Port += _GetDec(&pContext->InBufferDesc);
  _EatLine(&pContext->InBufferDesc);
  //
  // Create data socket and connect to "Port"
  //
  pContext->DataOut.Sock = pContext->CtrlOut.pIP_API->pfConnect(pContext->CtrlOut.Sock, Port);
  if (pContext->DataOut.Sock == NULL) {
    _SendFTPString(&pContext->CtrlOut, 530, "Could not create socket!");
    return 1;
  }
  _SendFTPString(&pContext->CtrlOut, 200, "Command okay.");
  return 0;
}

/*********************************************************************
*
*       _ExecPWD
*
*  Function description
*    Execute PWD command: Print working directory
*
*  Add. information
*    RFC 959 say:
*           This command causes the name of the current working
*           directory to be returned in the reply.  See Appendix II.
*           MKD pathname
*           257 "/usr/dm/pathname" directory created
*/
static int _ExecPWD(FTPS_CONTEXT * pContext) {
  _WriteString(&pContext->CtrlOut, "257 \"");
  _WriteString(&pContext->CtrlOut, pContext->acCurDir);
  _WriteString(&pContext->CtrlOut, "\" is current directory\r\n");
  _Flush(&pContext->CtrlOut);
  return 0;
}

/*********************************************************************
*
*       _ExecRETR
*
*  Function description
*    Execute RETR command: Retrieve
*
*  Return value
*     0    OK
*  != 0    Error
*
*  Add. information
*    RFC 959 says:
*         RETRIEVE (RETR)
*
*            This command causes the server-DTP to transfer a copy of the
*            file, specified in the pathname, to the server- or user-DTP
*            at the other end of the data connection.  The status and
*            contents of the file at the server site shall be unaffected.
*/
static int _ExecRETR(FTPS_CONTEXT * pContext) {
  char acFilename[64];
  void * hFile;
  int LenFileName;
  int i;
  int r;

  _EatWhite(&pContext->InBufferDesc);
  _GetLine(&pContext->InBufferDesc, &acFilename[0], sizeof(acFilename));
  _EatLine(&pContext->InBufferDesc);
  if (acFilename[0] != '/') {            // Relative file name ?
    //
    // Add current directory to file name
    //
    i = strlen(pContext->acCurDir);
    LenFileName = strlen(acFilename);
    if (LenFileName + i >= sizeof(acFilename)) {
      _SendFTPString(&pContext->CtrlOut, 550, "Filename size too long");
      _Disconnect(pContext);
      return 1;
    }
    memmove(&acFilename[i], &acFilename[0], LenFileName + 1);
    memcpy(&acFilename[0], pContext->acCurDir, i);
  }
  hFile = _OpenFile(pContext, &acFilename[0]);
  if (hFile) {
    _SendFTPString(&pContext->CtrlOut, 150, "File status okay; about to open data connection.");
    r = _SendFile(pContext, hFile);
    if (r == -1) {
      _SendFTPString(&pContext->CtrlOut, 426, "Connection closed; transfer aborted.");
    } else {
      _SendFTPString(&pContext->CtrlOut, 226, "Closing data connection. Requested file action successful.");
    }
    _CloseFile(pContext, hFile);
  } else {
    _SendFTPString(&pContext->CtrlOut, 550, "Requested action not taken.");
  }
  _Disconnect(pContext);
  return 0;
}

/*********************************************************************
*
*       _ExecRMD
*
*  Function description
*    Execute RMD command: Remove directory
*
*  Return value
*     0    OK
*  != 0    Error
*
*  Add. information
*    RFC 959 says:
*         REMOVE DIRECTORY (RMD)
*
*           This command causes the directory specified in the pathname
*           to be removed as a directory (if the pathname is absolute)
*           or as a subdirectory of the current working directory (if
*           the pathname is relative).
*/
static int _ExecRMD(FTPS_CONTEXT * pContext) {
  char acDirname[64];
  int r;

  //
  // Check if we have write permission
  //
  r = pContext->pApplication->pAccess->pfGetDirInfo(pContext->UserId, pContext->acCurDir, NULL, 0);
  if ((r & IP_FTPS_PERM_WRITE) == 0) {
    _SendFTPString(&pContext->CtrlOut, 553, "Requested action not taken - Not allowed.");
    return 0;   // No write permission for this directory
  }
  _EatWhite(&pContext->InBufferDesc);
  _GetLine(&pContext->InBufferDesc, &acDirname[0], sizeof(acDirname));
  _EatLine(&pContext->InBufferDesc);
  if (_GenerateAbsFilename(pContext, &acDirname[0], sizeof(acDirname))) {
    _SendFTPString(&pContext->CtrlOut, 550, "Dir name too long.");
    return 1;
  }
  r = pContext->pFS_API->pfRMDir(acDirname);
  if (r < 0) {
    _SendFTPString(&pContext->CtrlOut, 550, "Requested action not taken.");
  } else {
    _SendFTPString(&pContext->CtrlOut, 250, "Directory successfully removed.");
  }
  return 0;
}

/*********************************************************************
*
*       _ExecSIZE
*
*  Function description
*    Execute SIZE command: Size
*
*  Return value
*     0    OK
*  != 0    Error
*/
static int _ExecSIZE(FTPS_CONTEXT * pContext) {
  char acFilename[64];
  void * hFile;
  int FileSize;
  char ac[12];
  char * s;

  _EatWhite(&pContext->InBufferDesc);
  _GetLine(&pContext->InBufferDesc, &acFilename[0], sizeof(acFilename));
  _EatLine(&pContext->InBufferDesc);
  if (_GenerateAbsFilename(pContext, &acFilename[0], sizeof(acFilename))) {
    _SendFTPString(&pContext->CtrlOut, 550, "Filename too long.");
    return 1;
  }
  hFile = _OpenFile(pContext, &acFilename[0]);
  if (hFile) {
    FileSize = pContext->pFS_API->pfGetLen(hFile);
    s = _StoreUnsigned(&ac[0], FileSize, 10, 0);
    *s = 0;
    _SendFTPString(&pContext->CtrlOut, 213, ac);
    _CloseFile(pContext, hFile);
  } else {
    _SendFTPString(&pContext->CtrlOut, 550, "Requested action not taken.");
  }
  return 0;
}

/*********************************************************************
*
*       _ExecSTOR
*
*  Function description
*    Execute STOR command: Store
*
*  Return value
*     0    OK
*  != 0    Error
*
*  Add. information
*    RFC 959 says:
*         STORE (STOR)
*
*            This command causes the server-DTP to accept the data
*            transferred via the data connection and to store the data as
*            a file at the server site.  If the file specified in the
*            pathname exists at the server site, then its contents shall
*            be replaced by the data being transferred.  A new file is
*            created at the server site if the file specified in the
*            pathname does not already exist.
*/
static int _ExecSTOR(FTPS_CONTEXT * pContext) {
  void * hFile;
  char acFileName[FTPS_MAX_PATH];
  int i;
  int r;

  //
  // Check if we have write permission
  //
  i = pContext->pApplication->pAccess->pfGetDirInfo(pContext->UserId, pContext->acCurDir, NULL, 0);
  if ((i & IP_FTPS_PERM_WRITE) == 0) {
    _EatLine(&pContext->InBufferDesc);
    _SendFTPString(&pContext->CtrlOut, 553, "Requested action not taken - Not allowed.");
    return 1;   // No write permission for this directory
  }
  _EatWhite(&pContext->InBufferDesc);
  _GetLine(&pContext->InBufferDesc, &acFileName[0], sizeof(acFileName));
  _EatLine(&pContext->InBufferDesc);
  if (_GenerateAbsFilename(pContext, &acFileName[0], sizeof(acFileName))) {
    _SendFTPString(&pContext->CtrlOut, 552, "Requested file action aborted.");
    return 1;
  }
  hFile = pContext->pFS_API->pfCreate(&acFileName[0]);
  if (hFile == NULL) {
    _SendFTPString(&pContext->CtrlOut, 426, "Connection closed; transfer aborted.");
  } else {
    _SendFTPString(&pContext->CtrlOut, 150, "File status okay; about to open data connection.");
    r = _ReceiveFile(pContext, hFile);
    if (r == 0) {
      _SendFTPString(&pContext->CtrlOut, 226, "Closing data connection. Requested file action successful.");
    } else {
      _SendFTPString(&pContext->CtrlOut, 426, "Connection closed; transfer aborted.");
    }
    pContext->pFS_API->pfCloseFile(hFile);
  }
  _Disconnect(pContext);
  return 0;
}

/*********************************************************************
*
*       _ExecType
*
*  Function description
*    Execute TYPE command: Representation type
*
*  Return value
*     0    OK
*  != 0    Error
*
*  Add. information
*    RFC 959 says:
*         REPRESENTATION TYPE (TYPE)
*
*            The argument specifies the representation type as described
*            in the Section on Data Representation and Storage.  Several
*            types take a second parameter.  The first parameter is
*            denoted by a single Telnet character, as is the second
*            Format parameter for ASCII and EBCDIC; the second parameter
*            for local byte is a decimal integer to indicate Bytesize.
*            The parameters are separated by a <SP> (Space, ASCII code
*            32).
*/
static int _ExecTYPE(FTPS_CONTEXT * pContext) {
  int i;
  U8 c;

  c = _GetChar(&pContext->InBufferDesc);
  c = tolower(c);
  if (c == 'i') {
    _EatLine(&pContext->InBufferDesc);
    _SendFTPString(&pContext->CtrlOut, 200, "Command okay.");
  } else if (c == 'a') {
    _EatWhite(&pContext->InBufferDesc);
    i = _GetLineLen(&pContext->InBufferDesc);
    if (i > 2) {
      c = _GetChar(&pContext->InBufferDesc);
      c = tolower(c);
      _EatLine(&pContext->InBufferDesc);
      switch (c) {
        case 'n':
          _SendFTPString(&pContext->CtrlOut, 200, "Command okay.");
          break;
        case 't':
          _SendFTPString(&pContext->CtrlOut, 200, "Command okay.");
          break;
        case 'c':
          _SendFTPString(&pContext->CtrlOut, 200, "Command okay.");
          break;
      }
    } else {
      _EatLine(&pContext->InBufferDesc);
      _SendFTPString(&pContext->CtrlOut, 200, "Command okay.");
    }
  }
  return 0;
}

/*********************************************************************
*
*       _ExecUser
*
*  Function description
*    Execute USER command: User name
*
*  Return value
*    0    UserID invalid or unknown
*  > 0    UserID, no password required
*  < 0    UserID, password required
*
*  Add. information
*    Typical communication flow is:
*      USER Admin
*      331 Password required for Admin.
*      pass mcu
*      230 User Admin logged in.  Access restrictions apply.
*
*    RFC 959 says:
*         USER NAME (USER)
*
*            The argument field is a Telnet string identifying the user.
*            The user identification is that which is required by the
*            server for access to its file system.  This command will
*            normally be the first command transmitted by the user after
*            the control connections are made (some servers may require
*            this).  Additional identification information in the form of
*            a password and/or an account command may also be required by
*            some servers.  Servers may allow a new USER command to be
*            entered at any point in order to change the access control
*            and/or accounting information.  This has the effect of
*            flushing any user, password, and account information already
*            supplied and beginning the login sequence again.  All
*            transfer parameters are unchanged and any file transfer in
*            progress is completed under the old access control
*            parameters.
*/
static int _ExecUSER(FTPS_CONTEXT * pContext) {
  FTPS_ACCESS_CONTROL * pAccess;
  char acUser[32] = {0};

  pAccess = pContext->pApplication->pAccess;
  _GetLine(&pContext->InBufferDesc, acUser, sizeof(acUser));
  _EatLine(&pContext->InBufferDesc);
  pContext->UserId = pAccess->pfFindUser(acUser);
  _SendFTPString(&pContext->CtrlOut, 331, "Password required.");
  return pContext->UserId;
}


/*********************************************************************
*
*       _ParseInput
*
*  Function description
*/
static int _ParseInput(FTPS_CONTEXT * pContext) {
  IN_BUFFER_DESC * pBufferDesc;

  pBufferDesc = &pContext->InBufferDesc;

  if (_CompareCmd(pBufferDesc, "CDUP")) {
    _EatLine(&pContext->InBufferDesc);
    _ExecCDUP(pContext);
    return 0;
  } else if (_CompareCmd(pBufferDesc, "CWD")) {
    _EatBytes(pBufferDesc, 4);
    return _ExecCWD(pContext);
  } else if (_CompareCmd(pBufferDesc, "DELE")) {
    _EatBytes(pBufferDesc, 5);
    return _ExecDELE(pContext);
  } else if (_CompareCmd(pBufferDesc, "LIST")) {
    _EatBytes(pBufferDesc, 4);
    return _ExecLIST(pContext);
  } else if (_CompareCmd(pBufferDesc, "MKD")) {
    _EatBytes(pBufferDesc, 4);
    return _ExecMKD(pContext);
  } else if (_CompareCmd(pBufferDesc, "NLST")) {
    _EatBytes(pBufferDesc, 4);
    return _ExecNLST(pContext);
  } else if (_CompareCmd(pBufferDesc, "NOOP")) {
    _EatLine(&pContext->InBufferDesc);
    return _SendFTPString(&pContext->CtrlOut, 200, "Command okay.");
  } else if (_CompareCmd(pBufferDesc, "PASS")) {
    _EatBytes(pBufferDesc, 5);
    return _ExecPASS(pContext);
  } else if (_CompareCmd(pBufferDesc, "PASV")) {
    _EatBytes(pBufferDesc, 4);
    return _ExecPASV(pContext);
  } else if (_CompareCmd(pBufferDesc, "PORT")) {
    _EatBytes(pBufferDesc, 5);
    return _ExecPORT(pContext);
  } else if (_CompareCmd(pBufferDesc, "PWD")) {
    _EatLine(&pContext->InBufferDesc);
    return _ExecPWD(pContext);
  } else if (_CompareCmd(pBufferDesc, "RETR")) {
    _EatBytes(pBufferDesc, 5);
    return _ExecRETR(pContext);
  } else if (_CompareCmd(pBufferDesc, "RMD")) {
    _EatBytes(pBufferDesc, 4);
    return _ExecRMD(pContext);
  } else if (_CompareCmd(pBufferDesc, "SIZE")) {
    _EatBytes(pBufferDesc, 5);
    return _ExecSIZE(pContext);
  } else if (_CompareCmd(pBufferDesc, "STOR")) {
    _EatBytes(pBufferDesc, 5);
    return _ExecSTOR(pContext);
  } else if (_CompareCmd(pBufferDesc, "SYST")) {
    _EatLine(&pContext->InBufferDesc);
    return _SendFTPString(&pContext->CtrlOut, 215, "UNIX Type: L8");
  } else if (_CompareCmd(pBufferDesc, "TYPE")) {
    _EatBytes(pBufferDesc, 5);
    return _ExecTYPE(pContext);
  } else if (_CompareCmd(pBufferDesc, "USER")) {
    _EatBytes(pBufferDesc, 5);
    _ExecUSER(pContext);
    return 0;
  //
  // DIRECTORY ORIENTED FTP COMMANDS (Added for compatibility)
  // Refer to [RFC775] - http://tools.ietf.org/html/rfc775
  //
  } else if (_CompareCmd(pBufferDesc, "XPWD")) {
    _EatLine(&pContext->InBufferDesc);
    return _ExecPWD(pContext);
  } else if (_CompareCmd(pBufferDesc, "XMKD")) {
    _EatBytes(pBufferDesc, 5);
    return _ExecMKD(pContext);
  } else if (_CompareCmd(pBufferDesc, "XRMD")) {
    _EatBytes(pBufferDesc, 5);
    return _ExecRMD(pContext);
  } else if (_CompareCmd(pBufferDesc, "XCUP")) {
    _EatLine(&pContext->InBufferDesc);
    return _ExecCDUP(pContext);
  } else {
    _SendFTPString(&pContext->CtrlOut, 502, "Command not implemented.");
    _EatLine(&pContext->InBufferDesc);
    return 1;     // Error, unknown command
  }
}

/*********************************************************************
*
*       _Process
*
*  This is the main loop of the FTP server
*/
static void _Process(FTPS_CONTEXT * pContext) {
  int i;

  _SendFTPString(&pContext->CtrlOut, 220, FTPS_SIGN_ON_MSG);
  do {
    i = _ReadLine(&pContext->InBufferDesc);
    if (i <= 0) {
      return;     // Error, close connection
    }
    i = _ParseInput(pContext);
    if (i < 0) {
      return;     // Error, close connection
    }
    _EatLine(&pContext->InBufferDesc);
  } while (1);
}

/*********************************************************************
*
*       Public code
*
**********************************************************************
*/

/*********************************************************************
*
*       IP_FTPS_Process
*
*  Function description
*    Thread functionality of the FTP server.
*    Returns when the connection is closed or a fatal error occurs.
*/
int  IP_FTPS_Process (const IP_FTPS_API * pIP_API, void* CtrlSock, const IP_FS_API * pFS_API, const FTPS_APPLICATION * pApplication) {
  U8 acIn[FTPS_BUFFER_SIZE];
  U8 acOut[FTPS_BUFFER_SIZE];
  FTPS_CONTEXT Context;

  memset(&Context, 0, sizeof(Context));

  Context.pFS_API                  = pFS_API;
  Context.pApplication             = pApplication;

  Context.InBufferDesc.pIP_API     = pIP_API;
  Context.InBufferDesc.Sock        = CtrlSock;
  Context.InBufferDesc.pBuffer     = acIn;
  Context.InBufferDesc.Size        = sizeof(acIn);

  Context.CtrlOut.pIP_API        = pIP_API;
  Context.CtrlOut.Sock           = CtrlSock;
  Context.CtrlOut.pBuffer        = acOut;
  Context.CtrlOut.BufferSize     = sizeof(acOut);

  Context.DataOut.pIP_API        =  pIP_API;
  Context.DataOut.pBuffer        = acOut;
  Context.DataOut.BufferSize     = sizeof(acOut);

  strcpy(Context.acCurDir, "/");
  _Process(&Context);
  return 0;
}

/*********************************************************************
*
*       IP_FTPS_OnConnectionLimit
*
*  Function description
*    Thread functionality of the FTP server.
*    Returns when the connection is closed or a fatal error occurs.
*/
void IP_FTPS_OnConnectionLimit(const IP_FTPS_API * pIP_API, void* CtrlSock) {
  U8 acOut[FTPS_ERR_BUFFER_SIZE];
  OUT_BUFFER_CONTEXT OutContext;

  memset(&OutContext, 0, sizeof(OutContext));
  //
  // Save parameters
  //
  OutContext.pIP_API       = pIP_API;
  OutContext.Sock          = CtrlSock;

  OutContext.pBuffer       = acOut;
  OutContext.BufferSize    = sizeof(acOut);
  _SendFTPString(&OutContext, 421, "Connection limit reached");
}

/*************************** end of file ****************************/
