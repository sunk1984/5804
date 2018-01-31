/*********************************************************************
*  
*   IAR PowerPac
*
*   (c) Copyright IAR Systems 2010.  All rights reserved.
*
**********************************************************************
----------------------------------------------------------------------
File        : USBH_Log.c
Purpose     : USB host stack log routines
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
#include "USBH_Int.h"

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
  int    BufferSize;
  int    Cnt;
} BUFFER_DESC;

/*********************************************************************
*
*       Static data
*
**********************************************************************
*/

static U32 _LogFilter  = USBH_MTYPE_INIT;
static U32 _WarnFilter = 0xFFFFFFFF;    // Per default all warning enabled

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
*
*  Function description:
*/
static void _StoreChar(BUFFER_DESC * p, char c) {
  int   Cnt;
  Cnt = p->Cnt;
  if ((Cnt + 1) < p->BufferSize) {
    *(p->pBuffer + Cnt) = c;
    p->Cnt              = Cnt + 1;
  }
}

/*********************************************************************
*
*       _PrintUnsigned
*
*  Function description:
*/
static void _PrintUnsigned(BUFFER_DESC * pBufferDesc, U32 v, unsigned Base, int NumDigits) {
  static const char _aV2C[16] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' };
  unsigned          Div;
  U32               Digit = 1;
  while (((v / Digit) >= Base) | (NumDigits-- > 1)) { // Count how many digits are required
    Digit *= Base;
  }
  do {                                                // Output digits
    Div    = v     / Digit;
    v     -= Div   * Digit;
    _StoreChar(pBufferDesc, _aV2C[Div]);
    Digit /= Base;
  } while (Digit);
}

/*********************************************************************
*
*       _PrintInt
*
*  Function description:
*/
static void _PrintInt(BUFFER_DESC * pBufferDesc, I32 v, unsigned Base, unsigned NumDigits) {
  if (v < 0) { // Handle sign
    v = -v;
    _StoreChar(pBufferDesc, '-');
  }
  _PrintUnsigned(pBufferDesc, v, Base, NumDigits);
}

/*********************************************************************
*
*       _PrintHWAddr
*
*  Function description:
*/
static void _PrintHWAddr(BUFFER_DESC * pBufferDesc, int v) {
  int   i;
  U8  * p;
  p   = (U8 *)v;
  i   = 0;
  do {
    _PrintUnsigned(pBufferDesc, *p++, 16, 2);
    if (++i == 6) {
      break;
    }
    _StoreChar    (pBufferDesc, ':');
  } while (1);
}

/*********************************************************************
*
*       USBH_PrintfSafe
*
*  Function description:
*/
void USBH_PrintfSafe(char * pBuffer, const char * sFormat, int BufferSize, va_list * pParamList) {
  char                    c;
  BUFFER_DESC             BufferDesc;
  I32                     v;
  unsigned                NumDigits;
  BufferDesc.pBuffer    = pBuffer;
  BufferDesc.BufferSize = BufferSize;
  BufferDesc.Cnt        = 0;
  do {
    c = * sFormat++;
    if (c == 0) {
      break;
    }
    if (c == '%') { // Filter out width specifier (number of digits)
      NumDigits = 0;
      do {
        c = * sFormat++;
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
          c = * sFormat++;
          continue;
        }
        break;
      } while (1);
      //
      // Handle different types
      //
      v = va_arg(* pParamList, int);
      switch (c) {
      case 'h':
        _PrintHWAddr  (&BufferDesc, v);
        break;
      case 'd':
        _PrintInt     (&BufferDesc, v, 10, NumDigits);
        break;
      case 'u':
        _PrintUnsigned(&BufferDesc, v, 10, NumDigits);
        break;
      case 'c':
      case 'C':
        _StoreChar    (&BufferDesc, (char)v);
        break;
      case 'X':
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
        USBH_PANIC("Illegal format string");
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
*       USBH_Logf
*
*  Function description:
*/
void USBH_Logf(U32 Type, const char * sFormat, ...) {
  va_list ParamList;
  char    acBuffer[100];
  //
  // Filter message. If logging for this type of message is not enabled, do  nothing.
  //
  //by roger
  if ((Type & _LogFilter) == 0) {
    return;
  }
  //
  // Replace place holders (%d, %x etc) by values and call output routine.
  //
  va_start       (ParamList, sFormat);
  USBH_PrintfSafe(acBuffer,  sFormat, sizeof(acBuffer), &ParamList);
  USBH_Log       (acBuffer);
}

/*********************************************************************
*
*       USBH_Warnf
*
*  Function description:
*/
void USBH_Warnf(U32 Type, const char * sFormat, ...) {
  va_list ParamList;
  char    acBuffer[100];
  //
  // Filter message. If logging for this type of message is not enabled, do  nothing.
  //
  if ((Type & _WarnFilter) == 0) {
    return;
  }
  //
  // Replace place holders (%d, %x etc) by values and call output routine.
  //
  va_start       (ParamList, sFormat);
  USBH_PrintfSafe(acBuffer,  sFormat, sizeof(acBuffer), &ParamList);
  USBH_Warn      (acBuffer);
}

/*********************************************************************
*
*       USBH_SetLogFilter
*
*  Function description:
*/
void USBH_SetLogFilter(U32 FilterMask) {
  _LogFilter = FilterMask;
}

/*********************************************************************
*
*       USBH_AddLogFilter
*
*  Function description:
*/
void USBH_AddLogFilter(U32 FilterMask) {
  _LogFilter |= FilterMask;
}

/*********************************************************************
*
*       USBH_SetWarnFilter
*
*  Function description:
*/
void USBH_SetWarnFilter(U32 FilterMask) {
  _WarnFilter = FilterMask;
}

/*********************************************************************
*
*       USBH_AddLogFilter
*
*  Function description:
*/
void USBH_AddWarnFilter(U32 FilterMask) {
  _WarnFilter |= FilterMask;
}

/*********************************************************************
*
*       USBH_Logf_Application
*
*  Function description:
*/
void USBH_Logf_Application(const char * sFormat, ...) {
  va_list ParamList;
  char    acBuffer[100];
  //
  // Filter message. If logging for this type of message is not enabled, do  nothing.
  //
  if ((USBH_MTYPE_APPLICATION & _LogFilter) == 0) {
    return;
  }
  //
  // Replace place holders (%d, %x etc) by values and call output routine.
  //
  va_start       (ParamList, sFormat);
  USBH_PrintfSafe(acBuffer,  sFormat, sizeof(acBuffer), &ParamList);
  USBH_Log       (acBuffer);
}

/*********************************************************************
*
*       USBH_Warnf_Application
*
*  Function description:
*/
void USBH_Warnf_Application(const char * sFormat, ...) {
  va_list ParamList;
  char    acBuffer[100];
  //
  // Filter message. If logging for this type of message is not enabled, do  nothing.
  //
  if ((USBH_MTYPE_APPLICATION & _WarnFilter) == 0) {
    return;
  }
  //
  // Replace place holders (%d, %x etc) by values and call output routine.
  //
  va_start       (ParamList, sFormat);
  USBH_PrintfSafe(acBuffer,  sFormat, sizeof(acBuffer), &ParamList);
  USBH_Warn      (acBuffer);
}


/*********************************************************************
*
*       USBH_sprintf_Application
*
*  Function description:
*/
void USBH_sprintf_Application(char * pBuffer, unsigned BufferSize, const char * sFormat, ...) {
  va_list ParamList;
  //
  // Replace place holders (%d, %x etc) by values and call output routine.
  //
  va_start       (ParamList, sFormat);
  USBH_PrintfSafe(pBuffer,   sFormat, BufferSize, &ParamList);
}


/******************************* EOF ********************************/
