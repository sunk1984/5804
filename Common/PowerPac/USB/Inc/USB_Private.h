/*********************************************************************
*  
*   IAR PowerPac
*
*   (c) Copyright IAR Systems 2010.  All rights reserved.
*
**********************************************************************
----------------------------------------------------------------------
File    : USB_Private.h
Purpose : Private include file.
          Do not modify to allow easy updates !
--------  END-OF-HEADER  ---------------------------------------------
*/


#ifndef USB_INTERN_H
#define USB_INTERN_H

#include "USB_Conf.h"
#include "USB.h"

/*********************************************************************
*
*       Config defaults
*
**********************************************************************
*/
#ifndef USB_DEBUGOUT
  #define USB_DEBUGOUT(para)
#endif

#ifndef   USB_NUM_EPS
  #define USB_NUM_EPS 8
#endif

#ifndef   USB_MAX_NUM_IF
  #define USB_MAX_NUM_IF 4
#endif

#ifndef   USB_SUPPORT_TRANSFER_INT
  #define USB_SUPPORT_TRANSFER_INT 1
#endif

#ifndef   USB_SUPPORT_TRANSFER_ISO
  #define USB_SUPPORT_TRANSFER_ISO 1
#endif

#ifndef   USB_MAX_NUM_IAD
  #define USB_MAX_NUM_IAD 1
#endif

#ifndef   USB_MAX_NUM_COMPONENTS
  #define USB_MAX_NUM_COMPONENTS 2
#endif

#ifndef   USB_MEMCPY
  #include <string.h>
  #define USB_MEMCPY(pD, pS, NumBytes)  memcpy(pD, pS, NumBytes)
#endif

#ifndef   USB_MEMSET
  #include <string.h>
  #define USB_MEMSET(p, c, NumBytes)  memset(p, c, NumBytes)
#endif

#ifndef   USB_MEMCMP
  #include <string.h>
  #define USB_MEMCMP(p1, p2, NumBytes)  memcmp(p1, p2, NumBytes)
#endif


/* In order to avoid warnings for undefined parameters */
#ifndef USB_USE_PARA
  #if defined (__BORLANDC__) || defined(NC30) || defined(NC308)
    #define USB_USE_PARA(para)
  #else
    #define USB_USE_PARA(para) para=para;
  #endif
#endif

/*********************************************************************
*
*       Defines, fixed
*
**********************************************************************
*/
#define USB_ERROR_RX_OVERFLOW                1
#define USB_ERROR_ILLEGAL_MAX_PACKET_SIZE    2  // typically a configuration error: There is no valid packet size
#define USB_ERROR_ILLEGAL_EPADDR             3  // Illegal end point accessed
#define USB_ERROR_IBUFFER_SIZE_TOO_SMALL     4  // Info buffer size too small
#define USB_ERROR_DRIVER_ERROR               5  // general driver error
#define USB_ERROR_IAD_DESCRIPTORS_EXCEED     6
#define USB_ERROR_INVALID_INTERFACE_NO       7
#define USB_ERROR_TOO_MANY_CALLBACKS         8

/*********************************************************************
*
*       Function-like macros
*
**********************************************************************
*/
#define COUNTOF(a)          (sizeof(a)/sizeof(a[0]))
#define MIN(a,b)            (((a) < (b)) ? (a) : (b))
#define MAX(a,b)            (((a) > (b)) ? (a) : (b))


/*********************************************************************
*
*       Types
*
**********************************************************************
*/
#if defined(__cplusplus)
extern "C" {     /* Make sure we have C-declarations in C++ programs */
#endif

enum {
  STRING_INDEX_LANGUAGE = 0,  // Language index. MUST BE 0 acc. to spec.
  STRING_INDEX_MANUFACTURER,  // iManufacturer:      Index of String Desc (Manuf)    (variable, but needs to be unique)
  STRING_INDEX_PRODUCT,       // iProduct:           Index of String Desc (Product)  (variable, but needs to be unique)
  STRING_INDEX_SN             // iSerialNumber:      Index of String Desc (Serial #) (variable, but needs to be unique)
};

struct USB_INFO_BUFFER {
  unsigned Cnt;
  unsigned Sizeof;
  U8 * pBuffer;
};

typedef struct {
  U8       * pData;
  unsigned   Size;
  unsigned   NumBytesIn;
  unsigned   RdPos;
} BUFFER;

struct EP_STAT {
  U16             MaxPacketSize;
  U16             Interval;
  U8              EPType;
  U8              EPAddr;       // b[6:0]: EPAddr b7: Direction, 1: Device to Host (IN)
  U8              AllowShortPacket;
  volatile U8     IsHalted;
  U8           *  pData;
  volatile U32    NumBytesRem;
  union {
    struct {
      volatile U8     TxIsPending;
      volatile U8     NextTransferPrepared;
      U8              Send0PacketIfRequired;
    } TxInfo;
    USB_ON_RX_FUNC *  pfOnRx;
  } Dir;
  BUFFER          Buffer;
};

typedef struct {
  void (*pfAdd)(U8 FirstInterFaceNo, U8 NumInterfaces, U8 ClassNo, U8 SubClassNo, U8 ProtocolNo);
  void (*pfAddIadDesc)(int InterFaceNo, USB_INFO_BUFFER * pInfoBuffer);
} USB_IAD_API;

typedef struct {
  U16                    EPs;
  U8                     IFClass   ;    // Interface Class
  U8                     IFSubClass;    // Interface Subclass
  U8                     IFProtocol;    // Interface Protocol
  USB_ADD_FUNC_DESC    * pfAddFuncDesc;
  USB_ON_CLASS_REQUEST * pfOnClassRequest;
  USB_ON_CLASS_REQUEST * pfOnVendorRequest;
  USB_ON_SETUP         * pfOnSetup;
} INTERFACE;

typedef struct {
  U8                     NumEPs;
  U8                     NumIFs;
  U8                     Class   ;    // Interface Class
  U8                     SubClass;    // Interface Subclass
  U8                     Protocol;    // Interface Protocol
  U8                     OnTxBehavior;
  U8                     SetAddressBehavior;
  volatile U8            State;       // Global USB state, similar to 9.1.1. Bitwise combination of USB_STAT_ATTACHED, USB_STAT_READY, USB_STAT_ADDRESSED, USB_STAT_CONFIGURED, USB_STAT_SUSPENDED
  volatile U8            AllowRemoteWakeup;
  volatile U8            Addr;
  volatile U8            IsSelfPowered;
  INTERFACE              aIF[USB_MAX_NUM_IF];
  U8                     NumOnRxEP0Callbacks;
  USB_ON_RX_FUNC       * apfOnRxEP0[USB_MAX_NUM_COMPONENTS];
  const USB_HW_DRIVER  * pDriver;
  USB_IAD_API          * pIadAPI;
} GLOBAL;

/*********************************************************************
*
*       Global data
*
**********************************************************************
*/

#ifdef USB_MAIN_C
  #define EXTERN
#else
  #define EXTERN extern
#endif

EXTERN EP_STAT USB_aEPStat[USB_NUM_EPS];
EXTERN GLOBAL  USB_Global;
#undef EXTERN

/*********************************************************************
*
*       USB descriptor defines (refer to USB spec for details)
*
*/
#define USB_DESC_TYPE_DEVICE           1
#define USB_DESC_TYPE_CONFIG           2
#define USB_DESC_TYPE_STRING           3
#define USB_DESC_TYPE_INTERFACE        4
#define USB_DESC_TYPE_ENDPOINT         5
#define USB_DESC_TYPE_QUALIFIER        6
#define USB_DESC_TYPE_SPEED_CONFIG     7
#define USB_DESC_TYPE_INTERFACE_POWER  8
#define USB_DESC_TYPE_IAD             11

#define USB_STORE_U16(u16) ((u16) & 255), ((u16) / 256)

/*********************************************************************
*
*       USB__
*
**********************************************************************
*/
unsigned USB__CalcMaxPacketSize (unsigned MaxPacketSize, U8 TransferType, U8 IsHighSpeedMode);
U8       USB__EPAddr2Index      (unsigned EPAddr);
U8       USB__EPIndex2Addr      (U8 EPIndex);
void*    USB__GetpDest          (U8 EPIndex,    unsigned NumBytes);
U16      USB__GetU16BE          (U8 * p);
U16      USB__GetU16LE          (U8 * p);
U32      USB__GetU32BE          (U8 * p);
U32      USB__GetU32LE          (U8 * p);
void     USB__StoreU16BE        (U8 * p, unsigned v);
void     USB__StoreU16LE        (U8 * p, unsigned v);
void     USB__StoreU32LE        (U8 * p, U32 v);
void     USB__StoreU32BE        (U8 * p, U32 v);
U32      USB__SwapU32           (U32 v);
void     USB__HandleSetup       (const USB_SETUP_PACKET * pSetupPacket);
void     USB__OnBusReset        (void);
void     USB__OnResume          (void);
void     USB__OnRx              (U8 EPIndex, const U8 * pData, unsigned Len);
void     USB__OnRxZeroCopy      (U8 EpIndex, unsigned NumBytes);
void     USB__OnSetupCancel     (void);
void     USB__OnStatusChange    (U8 State);
void     USB__OnSuspend         (void);
void     USB__OnTx              (U8 EPIndex);
void     USB__OnTx0Done         (void);
void     USB__Send              (U8 EPIndex);
void     USB__UpdateEPHW        (void);
void     USB__WriteEP0FromISR   (const void* pData, unsigned NumBytes, char Send0PacketIfRequired);
int      USB__IsHighSpeedCapable(void);
int      USB__IsHighSpeedMode   (void);
U8       USB__AllocIF           (void);
void     USB__InvalidateEP      (U8 EPIndex);
void     USB__StallEP0          (void);
void     USB__ResetDataToggleEP (U8 EPIndex);

const U8 * USB__BuildConfigDesc(void);
const U8 * USB__BuildDeviceDesc(void);


/*********************************************************************
*
*       InfoBuffer routines
*
**********************************************************************
*/
void USB_IB_Init  (USB_INFO_BUFFER * pInfoBuffer, U8 * pBuffer, unsigned SizeofBuffer);
void USB_IB_AddU8 (USB_INFO_BUFFER * pInfoBuffer, U8  Data);
void USB_IB_AddU16(USB_INFO_BUFFER * pInfoBuffer, U16 Data);
void USB_IB_AddU32(USB_INFO_BUFFER * pInfoBuffer, U32 Data);

/*********************************************************************
*
*       Buffer routines
*
**********************************************************************
*/
unsigned BUFFER_Read (BUFFER * pBuffer,       U8 * pData, unsigned NumBytesReq);
void     BUFFER_Write(BUFFER * pBuffer, const U8 * pData, unsigned NumBytes);

#if defined(__cplusplus)
  }              /* Make sure we have C-declarations in C++ programs */
#endif

#endif

/*************************** End of file ****************************/
