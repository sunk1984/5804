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
File    : IP_UTIL_BASE64.c
Purpose : Base64 encoding
---------------------------END-OF-HEADER------------------------------
*/

#include <stdio.h>
#include <stdlib.h>
#include "IP_UTIL.h"
#include "SEGGER.h"

/*********************************************************************
*
*       defines, configurable
*
**********************************************************************
*/

/*********************************************************************
*
*       defines & enums, fixed
*
**********************************************************************
*/


/*********************************************************************
*
*       Types
*
**********************************************************************
*/
/*********************************************************************
*
*       static const
*
**********************************************************************
*/
static const char _aBase64[64] = {
  'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P', \
  'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f', \
  'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', \
  'w', 'x', 'y', 'z', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '+', '/'
};

/*********************************************************************
*
*       static Code
*
**********************************************************************
*/

/*********************************************************************
*
*       _Encode
*/
static U8 _Encode(U32 DataIn) {
  DataIn &= 0x3f;
  return _aBase64[DataIn];
}

/*********************************************************************
*
*       _Decode
*/
static U32 _Decode(U32 DataIn) {
  DataIn &= 0xff;
  if ((DataIn >= 'A') && (DataIn <= 'Z')) {
    return DataIn - 'A';
  }
  if ((DataIn >= 'a') && (DataIn <= 'z')) {
    return DataIn - 'a' + 26;
  }
  if ((DataIn >= '0') && (DataIn <= '9')) {
    return DataIn - '0' + 52;
  }
  if (DataIn == '+') {
    return 62;
  }
  if (DataIn == '/') {
    return 63;
  }
  if (DataIn == '-') {
    return 62;
  }
  if (DataIn == '_') {
    return 63;
  }
  return 255;
}

/*********************************************************************
*
*       Public Code
*
**********************************************************************
*/








/*********************************************************************
*
*       IP_UTIL_BASE64_Encode
*
*  Function description
*    Performs BASE-64 encoding according to RFC3548
*    For more info, check out http://tools.ietf.org/html/rfc3548
*      and 
*    http://tools.ietf.org/html/rfc2440
*
*  Parameters
*    pSrc     IN      Pointer to data to encode
*    SrcLen           Number of bytes to encode
*    pDest    IN      Pointer to destination buffer
*    pDestlen IN      Ptr to destination buffer size
*             OUT     Ptr to Number of bytes used in destination buffer
*
*  Return value
*    < 0      Error
*    > 0      Number of source bytes encoded, further call required
*    0        All bytes encoded
*/
int IP_UTIL_BASE64_Encode(const U8 * pSrc, int SrcLen, U8 *pDest, int * pDestLen) {
  int DestLen;
  int DestOff;
  int SrcOff;
  int Shift;
  int i;
  U32 DataIn;
  U32 DataOut;
  int NumBytesAtOnce;

  DestOff = 0;
  DestLen = *pDestLen;
  SrcOff  = 0;
  if (DestLen < 4) {
    return -1;    // Buffer too small
  }
  //
  // First step: compress as long as there is enough space in dest buffer
  //
  while (1) {
    //
    // Check if there is more to do
    //
    NumBytesAtOnce = SrcLen - SrcOff;
    if (NumBytesAtOnce > 3) {
      NumBytesAtOnce = 3;
    }
    if ((DestOff > DestLen +4) || (NumBytesAtOnce <= 0)) {    // Enough space in dest buffer and anything in Source ?
      *pDestLen = DestOff;
      return SrcOff;
    }
    //
    // Load up to 3 bytes
    //
    i = NumBytesAtOnce;
    DataIn = 0;
    SrcOff += i;
    Shift = 16;
    do {
      DataIn |= (U32)(*pSrc++) << Shift;
      Shift -= 8;
    } while (--i);
    //
    // Compute data output
    //
    DataOut  = _Encode(DataIn >> 18) << 24;
    DataOut |= _Encode(DataIn >> 12) << 16;
    if (NumBytesAtOnce > 1) {
      DataOut |= _Encode(DataIn >>  6) << 8;
    } else {
      DataOut |= '=' << 8;
    }
    if (NumBytesAtOnce > 2) {
      DataOut |= _Encode(DataIn);
    } else {
      DataOut |= '=';
    }
    //
    // Store in output buffer
    //
    *pDest++ = (U8)(DataOut >> 24);
    *pDest++ = (U8)(DataOut >> 16);
    *pDest++ = (U8)(DataOut >>  8);
    *pDest++ = (U8)DataOut;
    DestOff += 4;
  }
}

/*********************************************************************
*
*       IP_UTIL_BASE64_Decode
*
*  Function description
*    Performs BASE-64 decoding according to RFC3548
*    For more info, check out http://tools.ietf.org/html/rfc3548
*      and 
*    http://tools.ietf.org/html/rfc2440
*
*  Parameters
*    pSrc     IN      Pointer to data to encode
*    SrcLen           Number of bytes to encode
*    pDest    IN      Pointer to destination buffer
*    pDestlen IN      Ptr to destination buffer size
*             OUT     Ptr to Number of bytes used in destination buffer
*
*  Return value
*    < 0      Error
*    > 0      Number of source bytes encoded, further call required
*    0        All bytes encoded
*/
int IP_UTIL_BASE64_Decode(const U8 * pSrc, int SrcLen, U8 *pDest, int * pDestLen) {
  int DestLen;
  int DestOff;
  int SrcOff;
  int i;
  U32 DataOut;
  int NumBytesAtOnce;

  DestOff = 0;
  DestLen = *pDestLen;
  SrcOff  = 0;
  if (DestLen < 3) {
    return -1;    // Buffer too small
  }
  //
  // First step: compress as long as there is enough space in dest buffer
  //
  while (1) {
    //
    // Check if there is more to do
    //
    NumBytesAtOnce = SrcLen - SrcOff;
    if (NumBytesAtOnce > 4) {
      NumBytesAtOnce = 4;
    }
    if ((DestOff > DestLen +3) || (NumBytesAtOnce <= 0)) {    // Enough space in dest buffer and anything in Source ?
      *pDestLen = DestOff;
      return SrcOff;
    }
    i = NumBytesAtOnce;
    SrcOff += i;
    //
    // Load 4 bytes
    //
    DataOut  = _Decode(*pSrc++)     << 18;
    DataOut |= _Decode(*pSrc++)     << 12;
    DataOut |= _Decode(*pSrc    )   <<  6;
    DataOut |= _Decode(*(pSrc + 1));

    *pDest++ = (U8)(DataOut >> 16);
    if (*pSrc == '=') {
      *pDestLen = DestOff + 1;
      return SrcOff;
    }
    *pDest++ = (U8)(DataOut >> 8);
    if (*(pSrc + 1) == '=') {
      *pDestLen = DestOff + 2;
      return SrcOff;
    }
    *pDest++ = (U8)DataOut;
    pSrc += 2;
    DestOff += 3;
  }
}




/*************************** end of file ****************************/
