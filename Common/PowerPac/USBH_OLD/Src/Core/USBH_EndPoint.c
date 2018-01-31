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
*       UbdEpUrbCompletion
*
*  Function description
*    TBD
*/
void UbdEpUrbCompletion(URB * Urb) {
  USB_ENDPOINT * UsbEndpoint = (USB_ENDPOINT *)Urb->Header.InternalContext;
  T_ASSERT_MAGIC(UsbEndpoint, USB_ENDPOINT);
  UsbEndpoint->UrbCount--;
  USBH_LOG((USBH_MTYPE_URB, "URB: UbdEpUrbCompletion: urbcount: %u",UsbEndpoint->UrbCount));
  if (Urb->Header.Completion != NULL) {
    Urb->Header.Completion(Urb);
  }
  DEC_REF(UsbEndpoint->UsbInterface->Device);
}

/*********************************************************************
*
*       UbdEpSubmitUrb
*
*  Function description
*/
USBH_STATUS UbdEpSubmitUrb(USB_ENDPOINT * UsbEndpoint, URB * Urb) {
  USBH_STATUS       Status;
  USB_DEVICE      * Device;
  HOST_CONTROLLER * HostController;

  if (UsbEndpoint == NULL) {
    return USBH_STATUS_INVALID_PARAM;
  }
  Device         = UsbEndpoint->UsbInterface->Device;
  HostController = Device->HostController;
  UsbEndpoint->UrbCount++;
  INC_REF(Device);
  Status = HostController->HostEntry.SubmitRequest(UsbEndpoint->EpHandle, Urb);
  if (Status != USBH_STATUS_PENDING) {          // Completion routine is never called in this case
    USBH_WARN((USBH_MTYPE_URB, "URB: UbdEpSubmitUrb: %08x!",Status));
    Urb->Header.Status = Status;
    UsbEndpoint->UrbCount--;
    USBH_LOG((USBH_MTYPE_URB, "URB: UbdEpSubmitUrb: urbcount: %u",UsbEndpoint->UrbCount));
    DEC_REF(Device);
  }
  return Status;
}

/*********************************************************************
*
*       UbdNewEndpoint
*
*  Function description
*    Allocates an new endpoint object, clears the object sets the interface pointer
*    and initializes the interfaces list. The endpoint handle is invalid!
*/
USB_ENDPOINT * UbdNewEndpoint(USB_INTERFACE * UsbInterface, U8 * EndpointDescriptor) {
  USB_ENDPOINT    * ep;
  USB_DEVICE      * Device;
  HOST_CONTROLLER * HostController;
  U16               IntervalTime;
  //DBGOUT(DBG_EP,DbgPrint(DBGPFX"NewEndpoint: Device USB addr: %u bInterfaceNumber: %u!",
  //                       UsbInterface->Device->UsbAddress,
  //                       UsbInterface->InterfaceDescriptor[2]));
  Device          = UsbInterface->Device;
  HostController  = Device->HostController;
  ep              = USBH_Malloc(sizeof(USB_ENDPOINT));
  if (!ep) {
    USBH_WARN((USBH_MTYPE_URB, "URB: NewEndpoint: USBH_malloc!"));
    return NULL;
  }
  ZERO_MEMORY(ep, sizeof(USB_ENDPOINT));
  ep->UsbInterface       = UsbInterface;

#if (USBH_DEBUG > 1)
  ep->Magic              = USB_ENDPOINT_MAGIC;
#endif

  ep->EndpointDescriptor = EndpointDescriptor;
  IntervalTime           = UbdGetUcharFromDesc(EndpointDescriptor, 6);

  switch (Device->DeviceSpeed) {
    case USBH_LOW_SPEED:
      break;
    case USBH_FULL_SPEED:
      if (Device->DeviceDescriptor.bcdUSB < 0x0200) {                                                  // 1.0 or 1.1 device
      }
      else {
        if ((EndpointDescriptor[USB_EP_DESC_ATTRIB_OFS]&USB_EP_DESC_ATTRIB_MASK) == USB_EP_TYPE_ISO) { // ISO
          IntervalTime = (U16)(0x0001 << (IntervalTime - 1));
        }
      }
      break;
    case USBH_HIGH_SPEED:
      if ((EndpointDescriptor[USB_EP_DESC_ATTRIB_OFS]&USB_EP_DESC_ATTRIB_MASK) == USB_EP_TYPE_BULK) {  // Bulk endpoint
      }
      else {
        IntervalTime = (U16)(0x0001 << (IntervalTime - 1));
      }
      break;
    default:
      T_ASSERT0;
  }
  ep->EpHandle = HostController->HostEntry.AddEndpoint(HostController->HostEntry.HcHandle,   // USBH_HC_HANDLE HcHandle,
    (U8)(EndpointDescriptor[USB_EP_DESC_ATTRIB_OFS] & 0x3),                                  // U8 EndpointType,
    UsbInterface->Device->UsbAddress,                                                        // U8 DeviceAddress,
    EndpointDescriptor[USB_EP_DESC_ADDRESS_OFS],                                             // U8 EndpointAddress,
    UbdGetUshortFromDesc(EndpointDescriptor, USB_EP_DESC_PACKET_SIZE_OFS),                   // U16 MaxFifoSize,
    IntervalTime,                                                                            // U16 IntervalTime,
    UsbInterface->Device->DeviceSpeed                                                        // USBH_SPEED Speed,
  );
  if (ep->EpHandle == NULL) {
    USBH_WARN((USBH_MTYPE_URB, "URB: NewEndpoint: AddEndpoint dev: %d ep: 0x%x failed", UsbInterface->Device->UsbAddress, EndpointDescriptor[USB_EP_DESC_ADDRESS_OFS]));
    USBH_Free(ep);
    ep = NULL;
  }
  SET_EP_FOR_ADDRESS(UsbInterface, EndpointDescriptor[USB_EP_DESC_ADDRESS_OFS], ep);
  return ep;
}

/*********************************************************************
*
*       UbdDeleteEndpoint
*
*  Function description
*/
void UbdDeleteEndpoint(USB_ENDPOINT * UsbEndpoint) {
  HOST_CONTROLLER * HostController = UsbEndpoint->UsbInterface->Device->HostController;
  //DBGOUT(DBG_EP,DbgPrint(DBGPFX"UbdDeleteEndpoint addr: %u bInterfaceNumber: %u!", UsbEndpoint->UsbInterface->Device->UsbAddress,
  //                                                                                 UsbEndpoint->UsbInterface->InterfaceDescriptor[2]));
  T_ASSERT(UsbEndpoint->EpHandle != NULL);  // The EP must have a handle to the physical endpoint
  T_ASSERT(0 == UsbEndpoint->UrbCount);     // A URB must have a reference and the device must not be deleted if the URB has the reference 
  SET_EP_FOR_ADDRESS(UsbEndpoint->UsbInterface, UsbEndpoint->EndpointDescriptor[2], NULL);
  HC_INC_REF(HostController);
  HostController->HostEntry.ReleaseEndpoint(UsbEndpoint->EpHandle, UbdDefaultReleaseEpCompletion, HostController);
  USBH_Free(UsbEndpoint);
}

/********************************* EOF ******************************/
