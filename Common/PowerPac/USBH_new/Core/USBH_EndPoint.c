/*********************************************************************
*  
*   IAR PowerPac
*
*   (c) Copyright IAR Systems 2010.  All rights reserved.
*
**********************************************************************
----------------------------------------------------------------------
File        : USBH_EndPoint.c
Purpose     : USB host implementation
---------------------------END-OF-HEADER------------------------------
*/

/*********************************************************************
*
*       #include Section
*
**********************************************************************
*/

#include <stdlib.h>
#include "USBH_Int.h"

/*********************************************************************
*
*       USBH_BD_EpUrbCompletion
*
*  Function description
*    TBD
*/
void USBH_BD_EpUrbCompletion(USBH_URB * Urb) {
  USB_ENDPOINT * UsbEndpoint = (USB_ENDPOINT *)Urb->Header.pInternalContext;
  USBH_ASSERT_MAGIC(UsbEndpoint, USB_ENDPOINT);
  UsbEndpoint->UrbCount--;
  USBH_LOG((USBH_MTYPE_URB, "URB: USBH_BD_EpUrbCompletion: urbcount: %u",UsbEndpoint->UrbCount));
  if (Urb->Header.pfOnCompletion != NULL) {
    Urb->Header.pfOnCompletion(Urb);
  }
  DEC_REF(UsbEndpoint->pUsbInterface->pDevice);
}

/*********************************************************************
*
*       USBH_BD_EpSubmitUrb
*
*  Function description
*/
USBH_STATUS USBH_BD_EpSubmitUrb(USB_ENDPOINT * pUsbEndpoint, USBH_URB * pUrb) {
  USBH_STATUS       Status;
  USB_DEVICE      * pDevice;
  USBH_HOST_CONTROLLER * pHostController;

  if (pUsbEndpoint == NULL) {
    return USBH_STATUS_INVALID_PARAM;
  }
  pDevice         = pUsbEndpoint->pUsbInterface->pDevice;
  pHostController = pDevice->pHostController;
  pUsbEndpoint->UrbCount++;
  INC_REF(pDevice);
  Status = pHostController->pDriver->pfSubmitRequest(pUsbEndpoint->hEP, pUrb);
  if (Status != USBH_STATUS_PENDING) {          // Completion routine is never called in this case
    USBH_WARN((USBH_MTYPE_URB, "URB: USBH_BD_EpSubmitUrb: %08x!",Status));
    pUrb->Header.Status = Status;
    pUsbEndpoint->UrbCount--;
    USBH_LOG((USBH_MTYPE_URB, "URB: USBH_BD_EpSubmitUrb: urbcount: %u",pUsbEndpoint->UrbCount));
    DEC_REF(pDevice);
  }
  return Status;
}

/*********************************************************************
*
*       USBH_BD_NewEndpoint
*
*  Function description
*    Allocates an new endpoint object, clears the object sets the interface pointer
*    and initializes the interfaces list. The endpoint handle is invalid!
*/
USB_ENDPOINT * USBH_BD_NewEndpoint(USB_INTERFACE * pUsbInterface, U8 * pEndpointDescriptor) {
  USB_ENDPOINT         * ep;
  USB_DEVICE           * Device;
  USBH_HOST_CONTROLLER * HostController;
  U16                    IntervalTime;
  U8                     EPType;
  U8                     DevAddr;
  U8                     EPAddr;
  U16                    MaxPacketSize;
  USBH_SPEED             DevSpeed;

  //DBGOUT(DBG_EP,DbgPrint(DBGPFX"NewEndpoint: Device USB addr: %u bInterfaceNumber: %u!",
  //                       pUsbInterface->Device->UsbAddress,
  //                       pUsbInterface->InterfaceDescriptor[2]));
  Device          = pUsbInterface->pDevice;
  HostController  = Device->pHostController;
  ep              = (USB_ENDPOINT *)USBH_Malloc(sizeof(USB_ENDPOINT));
  if (!ep) {
    USBH_WARN((USBH_MTYPE_URB, "URB: NewEndpoint: USBH_malloc!"));
    return NULL;
  }
  USBH_ZERO_MEMORY(ep, sizeof(USB_ENDPOINT));
  ep->pUsbInterface       = pUsbInterface;
  IFDBG(ep->Magic        = USB_ENDPOINT_MAGIC);
  ep->pEndpointDescriptor = pEndpointDescriptor;
  IntervalTime           = USBH_GetUcharFromDesc(pEndpointDescriptor, 6);
  switch (Device->DeviceSpeed) {
  case USBH_LOW_SPEED:
    break;
  case USBH_FULL_SPEED:
    if (Device->DeviceDescriptor.bcdUSB < 0x0200) {                                                  // 1.0 or 1.1 device
    } else {
      if ((pEndpointDescriptor[USB_EP_DESC_ATTRIB_OFS]&USB_EP_DESC_ATTRIB_MASK) == USB_EP_TYPE_ISO) { // ISO
        IntervalTime = (U16)(0x0001 << (IntervalTime - 1));
      }
    }
    break;
  case USBH_HIGH_SPEED:
    if ((pEndpointDescriptor[USB_EP_DESC_ATTRIB_OFS]&USB_EP_DESC_ATTRIB_MASK) == USB_EP_TYPE_BULK) {  // Bulk endpoint
    }
    else {
      IntervalTime = (U16)(0x0001 << (IntervalTime - 1));
    }
    break;
  default:
    USBH_ASSERT0;
  }
  EPType = pEndpointDescriptor[USB_EP_DESC_ATTRIB_OFS] & 0x3;
  DevAddr = pUsbInterface->pDevice->UsbAddress;
  EPAddr = pEndpointDescriptor[USB_EP_DESC_ADDRESS_OFS];
  MaxPacketSize = USBH_GetUshortFromDesc(pEndpointDescriptor, USB_EP_DESC_PACKET_SIZE_OFS);
  DevSpeed = pUsbInterface->pDevice->DeviceSpeed;
  ep->hEP = HostController->pDriver->pfAddEndpoint(HostController->hHostController, EPType, DevAddr, EPAddr, MaxPacketSize, IntervalTime, DevSpeed);
  if (ep->hEP == NULL) {
    USBH_WARN((USBH_MTYPE_URB, "URB: NewEndpoint: pfAddEndpoint dev: %d ep: 0x%x failed", pUsbInterface->pDevice->UsbAddress, pEndpointDescriptor[USB_EP_DESC_ADDRESS_OFS]));
    USBH_Free(ep);
    ep = NULL;
  }
  SET_EP_FOR_ADDRESS(pUsbInterface, pEndpointDescriptor[USB_EP_DESC_ADDRESS_OFS], ep);
  return ep;
}

/*********************************************************************
*
*       USBH_BD_DeleteEndpoint
*
*  Function description
*/
void USBH_BD_DeleteEndpoint(USB_ENDPOINT * UsbEndpoint) {
  USBH_HOST_CONTROLLER * HostController = UsbEndpoint->pUsbInterface->pDevice->pHostController;
  //DBGOUT(DBG_EP,DbgPrint(DBGPFX"USBH_BD_DeleteEndpoint addr: %u bInterfaceNumber: %u!", UsbEndpoint->pUsbInterface->pDevice->UsbAddress,
  //                                                                                 UsbEndpoint->pUsbInterface->InterfaceDescriptor[2]));
  USBH_ASSERT(UsbEndpoint->hEP != NULL);  // The EP must have a handle to the physical endpoint
  USBH_ASSERT(0 == UsbEndpoint->UrbCount);     // A URB must have a reference and the device must not be deleted if the URB has the reference
  SET_EP_FOR_ADDRESS(UsbEndpoint->pUsbInterface, UsbEndpoint->pEndpointDescriptor[2], NULL);
  HC_INC_REF(HostController);
  HostController->pDriver->pfReleaseEndpoint(UsbEndpoint->hEP, USBH_DefaultReleaseEpCompletion, HostController);
  USBH_Free(UsbEndpoint);
}

/********************************* EOF ******************************/
