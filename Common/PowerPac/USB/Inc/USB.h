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
*       USB device stack for embedded applications                   *
*                                                                    *
**********************************************************************
----------------------------------------------------------------------
File    : USB.h
Purpose : USB stack API
          Do not modify to allow easy updates !
--------  END-OF-HEADER  ---------------------------------------------
*/

#ifndef USB_H     /* Avoid multiple inclusion */
#define USB_H

#include "SEGGER.h"
#include "USB_Conf.h"

#if defined(__cplusplus)
extern "C" {     /* Make sure we have C-declarations in C++ programs */
#endif

/*********************************************************************
*
*       Defines, fixed
*
**********************************************************************
*/

/* USB system version */
#define USB_VERSION                23201UL

/*********************************************************************
*
*       Default values
*
*/
#ifndef   USB_SUPPORT_HIGH_SPEED
  #define USB_SUPPORT_HIGH_SPEED 1
#endif

#ifndef   USB_MAX_PACKET_SIZE
  #if USB_SUPPORT_HIGH_SPEED
    #define USB_MAX_PACKET_SIZE   512
  #else
    #define USB_MAX_PACKET_SIZE    64
  #endif
#endif

#ifndef   USB_DEBUG_LEVEL
  #define USB_DEBUG_LEVEL  0
#endif

#ifndef    USB_DRIVER_OPTIMIZE
  #define  USB_DRIVER_OPTIMIZE
#endif

/*********************************************************************
*
*       Defines, fixed
*
**********************************************************************
*/
#define USB_Driver_NEC_70F377x    USB_Driver_NEC_70F376x

/*********************************************************************
*
*       Transfer types
*/
#define USB_TRANSFER_TYPE_CONTROL   0     // Refer to 9.6.6, table 9-13
#define USB_TRANSFER_TYPE_ISO       1
#define USB_TRANSFER_TYPE_BULK      2
#define USB_TRANSFER_TYPE_INT       3

/*********************************************************************
*
*       Endpoint direction
*/
#define USB_DIR_IN  1
#define USB_DIR_OUT 0

/*********************************************************************
*
*       Status flags
*/
#define USB_STAT_ATTACHED   (1 << 4)
#define USB_STAT_READY      (1 << 3)      // Set by any bus reset. Is required to go from "Powered" to "Addressed"
#define USB_STAT_ADDRESSED  (1 << 2)
#define USB_STAT_CONFIGURED (1 << 1)
#define USB_STAT_SUSPENDED  (1 << 0)

/*********************************************************************
*
*       Driver commands
*/
#define USB_DRIVER_CMD_SET_CONFIGURATION        0
#define USB_DRIVER_CMD_GET_TX_BEHAVIOR          1
#define USB_DRIVER_CMD_GET_SETADDRESS_BEHAVIOR  2
#define USB_DRIVER_CMD_REMOTE_WAKEUP            3


/*********************************************************************
*
*       Types / structures
*/
typedef struct USB_INFO_BUFFER USB_INFO_BUFFER;
typedef struct EP_STAT EP_STAT;

typedef struct {
  U8 bmRequestType;
  U8 bRequest;
  U8 wValueLow;
  U8 wValueHigh;
  U8 wIndexLow;
  U8 wIndexHigh;
  U8 wLengthLow;
  U8 wLengthHigh;
} USB_SETUP_PACKET;

typedef struct {
  U8 EPIn;
  U8 EPOut;
} USB_BULK_INIT_DATA;

typedef struct USB_HOOK {
  struct USB_HOOK * pNext;
  void    (*cb) (void* pContext, U8 NewState);
  void     * pContext;
} USB_HOOK;

typedef struct USB_HW_DRIVER {
  void     (*pfInit)                (void);
  U8       (*pfAllocEP)             (U8  InDir, U8 TransferType);
  void     (*pfUpdateEP)            (EP_STAT * pEPStat);
  void     (*pfEnable)              (void);
  void     (*pfAttach)              (void);
  unsigned (*pfGetMaxPacketSize)    (U8  EPIndex);
  int      (*pfIsInHighSpeedMode)   (void);
  void     (*pfSetAddress)          (U8  Addr);
  void     (*pfSetClrStallEP)       (U8  EPIndex, int OnOnff);
  void     (*pfStallEP0)            (void);
  void     (*pfDisableRxInterruptEP)(U8  EPIndex);
  void     (*pfEnableRxInterruptEP) (U8  EPIndex);
  void     (*pfStartTx)             (U8  EPIndex);
  void     (*pfSendEP)              (U8  EPIndex, const U8 * p, unsigned NumBytes);
  void     (*pfDisableTx)           (U8  EPIndex);
  void     (*pfResetEP)             (U8  EPIndex);
  int      (*pfControl)             (int Cmd, void * p);
} USB_HW_DRIVER;

typedef int  USB_ON_CLASS_REQUEST(const USB_SETUP_PACKET * pSetupPacket);
typedef int  USB_ON_SETUP        (const USB_SETUP_PACKET * pSetupPacket);
typedef void USB_ADD_FUNC_DESC   (USB_INFO_BUFFER * pInfoBuffer);
typedef void USB_ON_RX_FUNC      (const U8 * pData, unsigned NumBytes);
typedef void USB_ISR_HANDLER     (void);


/*********************************************************************
*
*       Public API functions
*
*/
unsigned USB_AddEP                   (U8 InDir, U8 TransferType, U16 Interval, U8 * pBuffer, unsigned BufferSize);
void     USB_EnableIAD               (void);
int      USB_GetState                (void);
void     USB_Init                    (void);
char     USB_IsConfigured            (void);
void     USB_Start                   (void);
void     USB_SetMaxPower             (unsigned MaxPower);
int      USB_RegisterSCHook          (USB_HOOK * pHook, void (*cb) (void* pContext, U8 NewState), void * pContext);
int      USB_UnregisterSCHook        (USB_HOOK * pHook);

void     USB_CancelIO                (U8 EPIndex);
unsigned USB_GetNumBytesInBuffer     (U8 EPIndex);
int      USB_ReadEP                  (U8 EPOut, void * pData, unsigned NumBytesReq);
int      USB_ReadEPOverlapped        (U8 EPOut, void * pData, unsigned NumBytesReq);
int      USB_ReadEPTimed             (U8 EPOut, void * pData, unsigned NumBytesReq, unsigned ms);
int      USB_ReceiveEP               (U8 EPOut, void * pData, unsigned NumBytesReq);
int      USB_ReceiveEPTimed          (U8 EPOut, void* pData, unsigned NumBytesReq, unsigned ms);
void     USB_SetOnRXHookEP           (U8 EPIndex, USB_ON_RX_FUNC * pfOnRx);
void     USB_SetClrStallEP           (U8 EPIndex, int OnOff);
void     USB_StallEP                 (U8 EPIndex);
void     USB_WaitForEndOfTransfer    (U8 EPIndex);
int      USB_WriteEP                 (U8 EPIndex, const void* pData, unsigned NumBytes, char Send0PacketIfRequired);
int      USB_WriteEPOverlapped       (U8 EPIndex, const void* pData, unsigned NumBytes, char Send0PacketIfRequired);
int      USB_WriteEPTimed            (U8 EPIndex, const void* pData, unsigned NumBytes, char Send0PacketIfRequired, unsigned ms);

void     USB_SetAddFuncDesc          (USB_ADD_FUNC_DESC    * pfAddDescFunc);
void     USB_SetClassRequestHook     (unsigned InterfaceNum, USB_ON_CLASS_REQUEST * pfOnClassRequest);
void     USB_SetVendorRequestHook    (unsigned InterfaceNum, USB_ON_CLASS_REQUEST * pfOnVendorRequest);
void     USB_SetOnSetupHook          (unsigned InterfaceNum, USB_ON_SETUP         * pfOnSetup);
void     USB_SetOnRxEP0              (USB_ON_RX_FUNC       * pfOnRx);

void     USB_DoRemoteWakeup          (void);
void     USB_SetIsSelfPowered        (U8 IsSelfPowered);
void     USB_SetAllowRemoteWakeUp    (U8 AllowRemoteWakeup);

/*********************************************************************
*
*       Public BULK API functions
*
*/
void     USB_BULK_Add                 (const USB_BULK_INIT_DATA * pInitData);
void     USB_BULK_CancelRead          (void);
void     USB_BULK_CancelWrite         (void);
unsigned USB_BULK_GetNumBytesInBuffer (void);
unsigned USB_BULK_GetNumBytesRemToRead(void);
unsigned USB_BULK_GetNumBytesToWrite  (void);
int      USB_BULK_Read                (      void * pData, unsigned NumBytes);
int      USB_BULK_ReadOverlapped      (      void * pData, unsigned NumBytes);
int      USB_BULK_ReadTimed           (      void * pData, unsigned NumBytes, unsigned ms);
int      USB_BULK_Receive             (      void * pData, unsigned NumBytes);
int      USB_BULK_ReceiveTimed        (      void * pData, unsigned NumBytes, unsigned ms);
void     USB_BULK_SetOnRXHook         (USB_ON_RX_FUNC * pfOnRx);
void     USB_BULK_WaitForRX           (void);
void     USB_BULK_WaitForTX           (void);
int      USB_BULK_Write               (const void * pData, unsigned NumBytes);
int      USB_BULK_WriteEx             (const void * pData, unsigned NumBytes, char Send0PacketIfRequired);
int      USB_BULK_WriteExTimed        (const void * pData, unsigned NumBytes, char Send0PacketIfRequired, unsigned ms);
int      USB_BULK_WriteOverlapped     (const void * pData, unsigned NumBytes);
int      USB_BULK_WriteOverlappedEx   (const void * pData, unsigned NumBytes, char Send0PacketIfRequired);
int      USB_BULK_WriteTimed          (const void * pData, unsigned NumBytes, unsigned ms);
void     USB_BULK_WriteNULLPacket     (void);

/*********************************************************************
*
*       Kernel interface routines (also for polled mode without kernel
*
*/
void     USB_OS_Init                   (void);
void     USB_OS_Delay                  (int ms);
void     USB_OS_DecRI                  (void);
U32      USB_OS_GetTickCnt             (void);
void     USB_OS_IncDI                  (void);
void     USB_OS_Panic                  (unsigned ErrCode);
void     USB_OS_Signal                 (unsigned EPIndex);
void     USB_OS_Wait                   (unsigned EPIndex);
int      USB_OS_WaitTimed              (unsigned EPIndex, unsigned ms);

/*********************************************************************
*
*       Get String functions during enumeration
*
*/
const char * USB_GetVendorName  (void);
const char * USB_GetProductName (void);
const char * USB_GetSerialNumber(void);

/*********************************************************************
*
*       Get Vendor/Product Ids during enumeration
*
*/
U16  USB_GetVendorId(void);
U16  USB_GetProductId(void);


/*********************************************************************
*
*       Setting USB target driver
*
*/
void USB_AddDriver(const USB_HW_DRIVER * pDriver);


/*********************************************************************
*
*       Function that has to be supplied by the customer
*
*/
void USB_X_AddDriver(void);
void USB_X_HWAttach(void);
void USB_X_EnableISR(USB_ISR_HANDLER * pfISRHandler);

/*********************************************************************
*
*       Available target USB drivers
*
*/
extern const USB_HW_DRIVER USB_Driver_AtmelCAP9;
extern const USB_HW_DRIVER USB_Driver_AtmelSAM3U;
extern const USB_HW_DRIVER USB_Driver_AtmelRM9200;
extern const USB_HW_DRIVER USB_Driver_AtmelSAM7A3;
extern const USB_HW_DRIVER USB_Driver_AtmelSAM7S;
extern const USB_HW_DRIVER USB_Driver_AtmelSAM7SE;
extern const USB_HW_DRIVER USB_Driver_AtmelSAM7X;
extern const USB_HW_DRIVER USB_Driver_AtmelSAM9260;
extern const USB_HW_DRIVER USB_Driver_AtmelSAM9261;
extern const USB_HW_DRIVER USB_Driver_AtmelSAM9263;
extern const USB_HW_DRIVER USB_Driver_AtmelSAM9G45;
extern const USB_HW_DRIVER USB_Driver_AtmelSAM9G20;
extern const USB_HW_DRIVER USB_Driver_AtmelSAM9Rx64;
extern const USB_HW_DRIVER USB_Driver_AtmelSAM9XE;
extern const USB_HW_DRIVER USB_Driver_Freescale_iMX25;
extern const USB_HW_DRIVER USB_Driver_Freescale_MCF227x;
extern const USB_HW_DRIVER USB_Driver_Freescale_MCF225x;
extern const USB_HW_DRIVER USB_Driver_Freescale_MCF51JMx;
extern const USB_HW_DRIVER USB_Driver_NEC_70F376x;
extern const USB_HW_DRIVER USB_Driver_NEC_uPD720150;
extern const USB_HW_DRIVER USB_Driver_NEC_78F102x;
extern const USB_HW_DRIVER USB_Driver_NXPLPC13xx;
extern const USB_HW_DRIVER USB_Driver_NXPLPC17xx;
extern const USB_HW_DRIVER USB_Driver_NXPLPC214x;
extern const USB_HW_DRIVER USB_Driver_NXPLPC23xx;
extern const USB_HW_DRIVER USB_Driver_NXPLPC24xx;
extern const USB_HW_DRIVER USB_Driver_NXPLPC318x;
extern const USB_HW_DRIVER USB_Driver_NXPLPC313x;
extern const USB_HW_DRIVER USB_Driver_OKI69Q62;
extern const USB_HW_DRIVER USB_Driver_SharpLH79524;
extern const USB_HW_DRIVER USB_Driver_SharpLH7A40x;
extern const USB_HW_DRIVER USB_Driver_STSTM32;
extern const USB_HW_DRIVER USB_Driver_STSTM32F107;
extern const USB_HW_DRIVER USB_Driver_STSTR71x;
extern const USB_HW_DRIVER USB_Driver_STSTR750;
extern const USB_HW_DRIVER USB_Driver_STSTR91x;
extern const USB_HW_DRIVER USB_Driver_H8SX1668R;
extern const USB_HW_DRIVER USB_Driver_H8S2472;
extern const USB_HW_DRIVER USB_Driver_TMPA910;
extern const USB_HW_DRIVER USB_Driver_TMPA900;
extern const USB_HW_DRIVER USB_Driver_RX62N;
extern const USB_HW_DRIVER USB_Driver_SH7203;
extern const USB_HW_DRIVER USB_Driver_SH7216;
extern const USB_HW_DRIVER USB_Driver_SH7286;
extern const USB_HW_DRIVER USB_Driver_TI_MSP430;
extern const USB_HW_DRIVER USB_Driver_TI_LM3S9B9x;

#if defined(__cplusplus)
  }              /* Make sure we have C-declarations in C++ programs */
#endif

#endif
