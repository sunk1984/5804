/*********************************************************************
*               SEGGER MICROCONTROLLER SYSTEME GmbH                  *
*       Solutions for real time microcontroller applications         *
**********************************************************************
*                                                                    *
*       (C) 2003 - 2007   SEGGER Microcontroller Systeme GmbH        *
*                                                                    *
*       www.segger.com     Support: support@segger.com               *
*                                                                    *
**********************************************************************
*                                                                    *
*       TCP/IP stack for embedded applications                       *
*                                                                    *
**********************************************************************
----------------------------------------------------------------------
File        : IP_Log.c
Purpose     : TCP/IP system log routines
---------------------------END-OF-HEADER------------------------------
*/

/*********************************************************************
*
*       #include Section
*
**********************************************************************
*/

#include <stdlib.h>
#include <stdarg.h>

#include "IP_Int.h"

/*********************************************************************
*
*       #define constants
*
**********************************************************************
*/

/*********************************************************************
*
*       Local data types
*
**********************************************************************
*/

typedef struct {
  char * pBuffer;
  int BufferSize;
  int Cnt;
} BUFFER_DESC;

/*********************************************************************
*
*       Static data
*
**********************************************************************
*/
static U32  _LogFilter  = IP_MTYPE_INIT | IP_MTYPE_DHCP | IP_MTYPE_LINK_CHANGE;
static U32  _WarnFilter = 0xFFFFFFFF;    // Per default all warning enabled

/*********************************************************************
*
*       Static code
*
**********************************************************************
*/
/*********************************************************************
*
*       Public code
*
**********************************************************************
*/

/*********************************************************************
*
*       _StoreChar
*/
static void _StoreChar(BUFFER_DESC * p, char c) {
  int Cnt;

  Cnt = p->Cnt;
  if ((Cnt + 1) < p->BufferSize) {
    *(p->pBuffer + Cnt) = c;
    p->Cnt = Cnt + 1;
  }
}

/*********************************************************************
*
*       _PrintUnsigned
*/
static void _PrintUnsigned(BUFFER_DESC * pBufferDesc, U32 v, unsigned Base, int NumDigits) {
  static const char _aV2C[16] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' };
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
    v -= Div * Digit;
    _StoreChar(pBufferDesc, _aV2C[Div]);
    Digit /= Base;
  } while (Digit);
}


/*********************************************************************
*
*       _PrintInt
*/
static void _PrintInt(BUFFER_DESC * pBufferDesc, I32 v, unsigned Base, unsigned NumDigits) {
  //
  // Handle sign
  //
  if (v < 0) {
    v = -v;
    _StoreChar(pBufferDesc, '-');
  }
  _PrintUnsigned(pBufferDesc, v, Base, NumDigits);
}

/*********************************************************************
*
*       _PrintDecByte
*/
static void _PrintDecByte(BUFFER_DESC * pBufferDesc, unsigned v) {
  _PrintInt(pBufferDesc, v, 10, 0);
}

/*********************************************************************
*
*       _PrintIPAddr
*
*  Function description
*    Writes the IP address in human readable form into the Buffer
*
*  Note:
*    IPAddr is given in network order
*    Example: 192.168.0.1 is 0xC0A80001 on Big endian systems
*                            0x0100A8C0 on little endian systems
*/
static void _PrintIPAddr(BUFFER_DESC * pBufferDesc, U32 v) {
  v = htonl(v);
  _PrintDecByte(pBufferDesc, (U8)(v >> 24));
  _StoreChar   (pBufferDesc, '.');
  _PrintDecByte(pBufferDesc, (U8)(v >> 16));
  _StoreChar   (pBufferDesc, '.');
  _PrintDecByte(pBufferDesc, (U8)(v >>  8));
  _StoreChar   (pBufferDesc, '.');
  _PrintDecByte(pBufferDesc, (U8)v);
}

/*********************************************************************
*
*       _PrintHWAddr
*/
static void _PrintHWAddr(BUFFER_DESC * pBufferDesc, int v) {
  int i;
  U8 * p;
  p = (U8*)v;

  i = 0;
  do {
    _PrintUnsigned(pBufferDesc, *p++, 16, 2);
    if (++i == 6) {
      break;
    }
    _StoreChar   (pBufferDesc, ':');
  } while (1);
}

/*********************************************************************
*
*       IP_PrintfSafe
*/
void IP_PrintfSafe(char * pBuffer, const char * sFormat, int BufferSize, va_list * pParamList) {
  char c;
  BUFFER_DESC BufferDesc;
  I32 v;
  unsigned NumDigits;

  BufferDesc.pBuffer    = pBuffer;
  BufferDesc.BufferSize = BufferSize;
  BufferDesc.Cnt        = 0;
  do {
    c = *sFormat++;
    if (c == 0) {
      break;
    }
    if (c == '%') {
      //
      // Filter out width specifier (number of digits)
      //
      NumDigits = 0;
      do {
        c = *sFormat++;
        if (c < '0' || c > '9') {
          break;
        }
        NumDigits = NumDigits * 10 + (c - '0');
      } while (1);
      //
      // Handle different qualifiers
      //
      do {
        if (c == 'l') {
          c = *sFormat++;
          continue;
        }
        break;
      } while (1);
      //
      // handle different types
      //
      v = va_arg(*pParamList, int);
      switch (c) {
      case 'i':
        _PrintIPAddr(&BufferDesc, v);
        break;
      case 'h':
        _PrintHWAddr(&BufferDesc, v);
        break;
      case 'd':
        _PrintInt(&BufferDesc, v, 10, NumDigits);
        break;
      case 'u':
        _PrintUnsigned(&BufferDesc, v, 10, NumDigits);
        break;
      case 'x':
        _PrintUnsigned(&BufferDesc, v, 16, NumDigits);
        break;
      case 's':
        {
          const char * s = (const char *)v;
          do {
            c = *s++;
            if (c == 0) {
              break;
            }
           _StoreChar(&BufferDesc, c);
          } while (1);
        }
        break;
      case 'p':
        _PrintUnsigned(&BufferDesc, v, 16, 8);
        break;
      default:
        IP_PANIC("Illegal format string");
      }
    } else {
      _StoreChar(&BufferDesc, c);
    }
  } while (1);
  //
  // Add trailing 0 to string
  //
  *(pBuffer + BufferDesc.Cnt) = 0;
}

/*********************************************************************
*
*       IP_Logf
*/
void IP_Logf(U32 Type, const char * sFormat, ...) {
  va_list ParamList;
  char acBuffer[100];

  //
  // Filter message. If logging for this type of message is not enabled, do  nothing.
  //
  if ((Type & _LogFilter) == 0) {
    return;
  }
  //
  // Replace place holders (%d, %x etc) by values and call output routine.
  //
  va_start(ParamList, sFormat);
  IP_PrintfSafe(acBuffer, sFormat, sizeof(acBuffer), &ParamList);
  IP_Log(acBuffer);
}

/*********************************************************************
*
*       IP_Warnf
*/
void IP_Warnf(U32 Type, const char * sFormat, ...) {
  va_list ParamList;
  char acBuffer[100];

  //
  // Filter message. If logging for this type of message is not enabled, do  nothing.
  //
  if ((Type & _WarnFilter) == 0) {
    return;
  }
  //
  // Replace place holders (%d, %x etc) by values and call output routine.
  //
  va_start(ParamList, sFormat);
  IP_PrintfSafe(acBuffer, sFormat, sizeof(acBuffer), &ParamList);
  IP_Warn(acBuffer);
}

/*********************************************************************
*
*       IP_SetLogFilter
*/
void IP_SetLogFilter(U32 FilterMask) {
  _LogFilter = FilterMask;
}

/*********************************************************************
*
*       IP_AddLogFilter
*/
void IP_AddLogFilter(U32 FilterMask) {
  _LogFilter |= FilterMask;
}

/*********************************************************************
*
*       IP_SetWarnFilter
*/
void IP_SetWarnFilter(U32 FilterMask) {
  _WarnFilter = FilterMask;
}

/*********************************************************************
*
*       IP_AddLogFilter
*/
void IP_AddWarnFilter(U32 FilterMask) {
  _WarnFilter |= FilterMask;
}

/*********************************************************************
*
*       IP_PrintIPAddr
*
*  Function description
*    Writes the IP address in human readable form into a string.
*    The string is 0-terminated.
*
*  Note:
*    IPAddr is given in network order (Big endian)
*    Example: 192.168.0.1 is 0xC0A80001
*/
int IP_PrintIPAddr(char * pDest, U32 IPAddr, int BufferSize) {
  BUFFER_DESC BufferDesc;

  if (BufferSize < 16) {
    *pDest = 0;
    return 0;
  }
  BufferDesc.pBuffer    = pDest;
  BufferDesc.BufferSize = BufferSize;
  BufferDesc.Cnt        = 0;
  _PrintIPAddr(&BufferDesc, htonl(IPAddr));
  *(pDest + BufferDesc.Cnt) = 0;
  return BufferDesc.Cnt;
}

/*********************************************************************
*
*       IP_Logf_Application
*/
void IP_Logf_Application(const char * sFormat, ...) {
  va_list ParamList;
  char acBuffer[100];

  //
  // Filter message. If logging for this type of message is not enabled, do  nothing.
  //
  if ((IP_MTYPE_APPLICATION & _LogFilter) == 0) {
    return;
  }
  //
  // Replace place holders (%d, %x etc) by values and call output routine.
  //
  va_start(ParamList, sFormat);
  IP_PrintfSafe(acBuffer, sFormat, sizeof(acBuffer), &ParamList);
  IP_Log(acBuffer);
}

/*********************************************************************
*
*       IP_Warnf_Application
*/
void IP_Warnf_Application(const char * sFormat, ...) {
  va_list ParamList;
  char acBuffer[100];

  //
  // Filter message. If logging for this type of message is not enabled, do  nothing.
  //
  if ((IP_MTYPE_APPLICATION & _WarnFilter) == 0) {
    return;
  }
  //
  // Replace place holders (%d, %x etc) by values and call output routine.
  //
  va_start(ParamList, sFormat);
  IP_PrintfSafe(acBuffer, sFormat, sizeof(acBuffer), &ParamList);
  IP_Warn(acBuffer);
}


/*************************** End of file ****************************/
