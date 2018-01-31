/*********************************************************************
*  
*   IAR PowerPac
*
*   (c) Copyright IAR Systems 2010.  All rights reserved.
*
**********************************************************************
----------------------------------------------------------------------
File    : USB_CDC.h
Purpose : Public header of the communication device class
--------  END-OF-HEADER  ---------------------------------------------
*/

#ifndef CDC_H          /* Avoid multiple inclusion */
#define CDC_H

#include "SEGGER.h"

#if defined(__cplusplus)
extern "C" {     /* Make sure we have C-declarations in C++ programs */
#endif


/*********************************************************************
*
*       Config defaults
*
**********************************************************************
*/

#ifndef   CDC_DEBUG_LEVEL
  #define CDC_DEBUG_LEVEL 0
#endif

#ifndef   CDC_USE_PARA
  #define CDC_USE_PARA(para) para = para
#endif

#define CDC_USB_CLASS     2         // 2: Communication Device Class
#define CDC_USB_SUBCLASS  0x00      //
#define CDC_USB_PROTOCOL  0x00      //


/*********************************************************************
*
*       Types
*
**********************************************************************
*/


typedef struct {
  U32 DTERate;
  U8  CharFormat;
  U8  ParityType;
  U8  DataBits;
} USB_CDC_LINE_CODING;

typedef struct {
  U8 DCD;           // CDC spec: bRxCarrier
  U8 DSR;           // CDC spec: bTxCarrier
  U8 Break;         // CDC spec: bBreak
  U8 Ring;          // CDC spec: bRingSignal
  U8 FramingError;  // CDC spec: bFraming
  U8 ParityError;   // CDC spec: bParity
  U8 OverRunError;  // CDC spec: bOverRun
  U8 CTS;           // CDC spec: not specified
} USB_CDC_SERIAL_STATE;

typedef struct USB_CDC_CONTROL_LINE_STATE {
  U8 DTR;
  U8 RTS;
} USB_CDC_CONTROL_LINE_STATE;

typedef void USB_CDC_ON_SET_LINE_CODING(USB_CDC_LINE_CODING * pLineCoding);
typedef void USB_CDC_ON_SET_CONTROL_LINE_STATE(USB_CDC_CONTROL_LINE_STATE * pLineState);

typedef int USB_CDC_HANDLE;

/*********************************************************************
*
*       Communication interface
*/
typedef struct {
  U8 EPIn;
  U8 EPOut;
  U8 EPInt;
} USB_CDC_INIT_DATA;

/*********************************************************************
*
*       API functions
*
**********************************************************************
*/
USB_CDC_HANDLE USB_CDC_Add                  (const USB_CDC_INIT_DATA * pInitData);
void USB_CDC_CancelRead     (void);
void USB_CDC_CancelWrite    (void);
int  USB_CDC_Read           (      void * pData, unsigned NumBytes);
int  USB_CDC_ReadOverlapped (      void * pData, unsigned NumBytes);
int  USB_CDC_ReadTimed      (      void * pData, unsigned NumBytes, unsigned ms);
int  USB_CDC_Receive        (      void * pData, unsigned NumBytes);
int  USB_CDC_ReceiveTimed   (      void * pData, unsigned NumBytes, unsigned ms);
void USB_CDC_Write          (const void * pData, unsigned NumBytes);
int            USB_CDC_WriteOverlapped      (const void * pData, unsigned NumBytes);
int            USB_CDC_WriteTimed           (const void * pData, unsigned NumBytes, unsigned ms);
void           USB_CDC_SetOnLineCoding      (USB_CDC_ON_SET_LINE_CODING * pf);
void USB_CDC_SetOnControlLineState(USB_CDC_ON_SET_CONTROL_LINE_STATE * pf);
void USB_CDC_WaitForTX      (void);
void USB_CDC_WaitForRX      (void);
void           USB_CDC_WriteSerialState     (void);
void           USB_CDC_UpdateSerialState    (USB_CDC_SERIAL_STATE * pSerialState);

void           USB_CDC_CancelReadEx           (USB_CDC_HANDLE hInst);
void           USB_CDC_CancelWriteEx          (USB_CDC_HANDLE hInst);
int            USB_CDC_ReadEx                 (USB_CDC_HANDLE hInst,       void * pData, unsigned NumBytes);
int            USB_CDC_ReadOverlappedEx       (USB_CDC_HANDLE hInst,       void * pData, unsigned NumBytes);
int            USB_CDC_ReadTimedEx            (USB_CDC_HANDLE hInst,       void * pData, unsigned NumBytes, unsigned ms);
int            USB_CDC_ReceiveEx              (USB_CDC_HANDLE hInst,       void * pData, unsigned NumBytes);
int            USB_CDC_ReceiveTimedEx         (USB_CDC_HANDLE hInst,       void * pData, unsigned NumBytes, unsigned ms);
void           USB_CDC_WriteEx                (USB_CDC_HANDLE hInst, const void * pData, unsigned NumBytes);
int            USB_CDC_WriteOverlappedEx      (USB_CDC_HANDLE hInst, const void * pData, unsigned NumBytes);
int            USB_CDC_WriteTimedEx           (USB_CDC_HANDLE hInst, const void * pData, unsigned NumBytes, unsigned ms);
void           USB_CDC_SetOnLineCodingEx      (USB_CDC_HANDLE hInst, USB_CDC_ON_SET_LINE_CODING * pf);
void           USB_CDC_SetOnControlLineStateEx(USB_CDC_HANDLE hInst, USB_CDC_ON_SET_CONTROL_LINE_STATE * pf);
void           USB_CDC_WaitForTXEx            (USB_CDC_HANDLE hInst);
void           USB_CDC_WaitForRXEx            (USB_CDC_HANDLE hInst);
void           USB_CDC_WriteSerialStateEx     (USB_CDC_HANDLE hInst);
void           USB_CDC_UpdateSerialStateEx    (USB_CDC_HANDLE hInst, USB_CDC_SERIAL_STATE * pSerialState);


#if defined(__cplusplus)
  }              /* Make sure we have C-declarations in C++ programs */
#endif

#endif                 /* Avoid multiple inclusion */

/**************************** end of file ***************************/
