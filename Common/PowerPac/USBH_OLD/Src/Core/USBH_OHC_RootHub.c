/*********************************************************************
*  
*   IAR PowerPac
*
*   (c) Copyright IAR Systems 2010.  All rights reserved.
*
**********************************************************************
----------------------------------------------------------------------
File        : USBH_OHC_RootHub.c
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
*       OhRhUpdateHubStatusChangeBits
*
*  Function description
*    Updates the hub status change bits
*/
static U32 OhRhUpdateHubStatusChangeBits(HC_ROOT_HUB * Rh, U32 NewStatus) {
  U16 hubStatus, change;
  // 1. Generate change bits form old and new status
  // 2. Add change bits from the new status
  // 3. Add generated change bits
  // 4. Return updated OHCI root hub status
  hubStatus   = (U16)(NewStatus & 0x0ffff);
  change      = (U16)(hubStatus ^ Rh->Status);
  Rh->Change |= (U16)(NewStatus >> 16);
  Rh->Change |= change;
  Rh->Status  = hubStatus;
  // Returns the new updated status see also OHCI spec.
  return Rh->Status | (((U32)Rh->Change) << 16);
}

/*********************************************************************
*
*       OhRhUpdatePortStatusChangeBits
*
*  Function description
*    Updates the port status change bits
*/
static U32 OhRhUpdatePortStatusChangeBits(HC_HUB_PORT * RhPort, U32 NewStatus) {
  U16 portStatus, change;
  // 1. Generate change bits form old and new status
  // 2. Or change bits from the new status
  // 3. Or generated change bits
  // 4. Return updated OHCI port status
  portStatus      = (U16)(NewStatus & 0x0ffff);
  change          = (U16)(portStatus ^ RhPort->Status);
  RhPort->Change |= (U16)(NewStatus >> 16);
  RhPort->Change |= change;
  RhPort->Status  = portStatus;
  return RhPort->Status | (((U32)RhPort->Change) << 16);
}

#if !HC_ROOTHUB_PER_PORT_POWERED

/*********************************************************************
*
*       OhRhUpdatePortStatusChangeBits
*
*  Function description
*    Returns number of powered ports on the root hub
*/
static int OhRhGetPoweredPorts(HC_ROOT_HUB * Rh) {
  int i, ct;
  for (i = 0, ct = 0; i < Rh->PortCount; i++) {
    HC_HUB_PORT * pPort;

    pPort = Rh->apHcdPort + i;
    if (pPort->Power) {
      ct++;                     // Count powered on ports
    }
  }
  USBH_LOG((USBH_MTYPE_HUB, "Roothub: OhRhGetPoweredPorts: number of powered ports: %d",ct));
  return ct;
}
#endif

/*********************************************************************
*
*       OhRhSetPortPower
*
*  Function description
*/
static void OhRhSetPortPower(HC_ROOT_HUB * Rh, U8 Port, U8 PowerFlag) {
  HC_HUB_PORT * port;

  T_ASSERT(Rh->PortCount >= Port); // Port start with 0
  port = Rh->apHcdPort + (Port - 1);
#if HC_ROOTHUB_PER_PORT_POWERED
  if (PowerFlag) {         // Port powered on/off
    if (!port->Power) {    // No power on port
      USBH_LOG((USBH_MTYPE_HUB, "Roothub: OhRhSetPortPower: set port power on port: %u",Port));
      RhSetPortPower(Rh->Dev, Port);
    }
  } else {
    if (port->Power) {     // Power on board
      USBH_LOG((USBH_MTYPE_HUB, "Roothub: OhRhSetPortPower: clear port power on port: %u",Port));
      RhClearPortPower(Rh->Dev, Port);
    }
  }
#else
  // Global on/off
  USBH_LOG((USBH_MTYPE_HUB, "Roothub: OhRhSetPortPower: global powered,Portnumber: %u power on/off:%u ",Port,PowerFlag));
  {
    int powerCt;
    powerCt = OhRhGetPoweredPorts(Rh);
    if (PowerFlag) {
      if (powerCt == 0) {  // Alls ports are power off
        USBH_LOG((USBH_MTYPE_HUB, "Roothub: OhRhSetPortPower: global power on!"));
        USBH_WriteReg32(((((Rh->Dev))->RegBase) + (0x050)), ((0x00010000L)));
        RhSetGlobalPower(Rh->Dev);
      }
    } else {
      if (powerCt != 0) {  // Any port is power on
        USBH_LOG((USBH_MTYPE_HUB, "Roothub: OhRhSetPortPower: global power off!"));
        RhClearGlobalPower(Rh->Dev);
      // Check if must reset status bits of all ports that uses the global port power
      }
    }
  }
#endif
  port->Power = PowerFlag; // Save the new power state
}

/*********************************************************************
*
*       OhRhSetPortPower
*
*  Function description
*    Sets up the root hub register OH_REG_RHDESCRIPTORA and OH_REG_RHDESCRIPTORB!
*    Initializes the root hub object and the root hub register in the host controller
*/
USBH_STATUS OhRhInit(struct T_HC_DEVICE * Dev, USBH_RootHubNotification * UbdRootHubNotification, void * RootHubNotificationContext) {
  USBH_STATUS     status;
  HC_ROOT_HUB   * hub;
  HC_HUB_PORT   * port;
  U32             ports, hubDesc;
  unsigned int    i;
  USBH_LOG((USBH_MTYPE_HUB, "Roothub: OhRhInit"));
  OH_DEV_VALID(Dev);
  hub = &Dev->RootHub;
  // Clears the root hub object
  ZERO_MEMORY(hub, sizeof(HC_ROOT_HUB));

  hub->UbdRootHubNotification      = UbdRootHubNotification;
  hub->RootHubNotificationContext  = RootHubNotificationContext;
  status                           = USBH_STATUS_SUCCESS;
  hub->Dev                         = Dev;
  ports                            = OhHalReadReg(Dev->RegBase, OH_REG_RHDESCRIPTORA);
  hub->PortCount                   = (U16)(ports &ROOT_HUB_NDP);
  hub->PowerOnToPowerGoodTime      = POWERON_TO_POWERGOOD_TIME;
  hub->apHcdPort                   = (HC_HUB_PORT *)USBH_MallocZeroed(hub->PortCount * sizeof(HC_HUB_PORT));
//   if (hub->PortCount              != HC_ROOTHUB_PORT_NUMBERS) {
//     USBH_WARN((USBH_MTYPE_HUB, "Roothub: OhRhInit: Ports from OHCI differs from HC_ROOTHUB_PORT_NUMBERS! numbers: %hu!", hub->PortCount));
//     //return USBH_STATUS_INVALID_PARAM;
//   }
  // Setup hub descriptor A
  OhHalWriteReg(Dev->RegBase, OH_REG_RHDESCRIPTORA, INIT_ROOT_HUB_DESC_A);
  OhHalWriteReg(Dev->RegBase, OH_REG_RHDESCRIPTORB, 0);
  // Init all ports and clear all port status change bits
  port    = hub->apHcdPort;
  hubDesc = 0;
  for (i = 0; i < hub->PortCount; i++, port++) {
    port->Port = (U8)(i + 1);
#if HC_ROOTHUB_PORTS_ALWAYS_POWERED
    // This is a host controller where the port power can not switched or the user
    // want that the port power on all ports on after initializing the driver
    port->Power = TRUE;
#endif
#if HC_ROOTHUB_PER_PORT_POWERED
    hubDesc |= ROOT_HUB_PPCM_MASK(port->Port);
#endif
  }
  // Setup hub descriptor B
  hubDesc |= INIT_ROOT_HUB_DESC_B;
  OhHalWriteReg(Dev->RegBase, OH_REG_RHDESCRIPTORB, hubDesc);
  return status;
}

/*********************************************************************
*
*       OhRhProcessInterrupt
*
*  Function description
*    If an hub or status change bit is set then an Bit an Mask is set
*    and the driver hub and port object is updated. The change bit in
*    the hub register is cleared. If the mask is unequal zero the
*    USBH_RootHubNotification function is called!
*/
void OhRhProcessInterrupt(HC_ROOT_HUB * Hub) {
  HC_DEVICE        * dev;
  HC_HUB_PORT      * hcPort;
  U8                 port;
  U32                status;
  U32                mask, notificationMask;
  dev              = Hub->Dev;
  OH_DEV_VALID(dev);
  notificationMask = 0; // Notify the USB bus driver of status changes in the root hub Bit 0=1 -> root hub changes other bits for port changes
  status           = RhReadStatus(dev);
  if (0 != (status &ROOT_HUB_CHANGE_MASK)) {
    RhWriteStatus(dev, status & ROOT_HUB_CHANGE_MASK);       // This clears the hub interrupt
    dev->RootHub.Change |= (U16)(status >> 16);              // Update the status change bits with the the physical not self calculated change bits
    notificationMask    |= 1;                                // Root hub status chnaged bit 0
  }
  // Check all hub ports
  mask   = 0x02;                                             // Port notification bits starts with bit 1
  hcPort = Hub->apHcdPort;
  for (port = 1; port <= Hub->PortCount; port++, mask <<= 1, hcPort++) {
    status = RhReadPort(dev, port);
    if (0 != (status &HUB_PORT_CHANGE_MASK)) {
      RhWritePort(dev, port, status & HUB_PORT_CHANGE_MASK); // Change bit on, clear the change bit
      hcPort->Change   |= (U16)(status >> 16);               // Set the change bits in the hub context
      notificationMask |= mask;
    }
  }
  if (notificationMask) {
    USBH_LOG((USBH_MTYPE_HUB, "Roothub: UBD notification: RootHub=Bit 0 Ports=Bit 1..31: 0x%lx ", notificationMask));
    Hub->UbdRootHubNotification(Hub->RootHubNotificationContext, notificationMask);
  }
}


/*********************************************************************
*
*       Device driver hub interface
*
**********************************************************************
*/

/*********************************************************************
*
*       OhRh_GetPortCount
*
*  Function description
*    Returns the number of root hub ports
*/
unsigned int OhRh_GetPortCount(USBH_HC_HANDLE HcHandle) {
  HC_DEVICE    * dev;
  unsigned int   count;
  OH_DEV_FROM_HANDLE(dev, HcHandle);
  OH_DEV_VALID(dev);
  // Return the devices shadow counter
  count = dev->RootHub.PortCount;
  USBH_LOG((USBH_MTYPE_HUB, "Roothub: OhRh_GetPortCount: count:%u!",count));
  return count;
}

/*********************************************************************
*
*       OhRh_GetPowerGoodTime
*
*  Function description
*    Returns the power on to power good time in ms
*/
unsigned int OhRh_GetPowerGoodTime(USBH_HC_HANDLE HcHandle) {
  HC_DEVICE    * dev;
  unsigned int   time;
  OH_DEV_FROM_HANDLE(dev, HcHandle);
  OH_DEV_VALID(dev);
  time = dev->RootHub.PowerOnToPowerGoodTime;
  USBH_LOG((USBH_MTYPE_HUB, "Roothub: OhRh_GetPowerGoodTime: time: %u!", time));
  return time;
}

/*********************************************************************
*
*       OhRh_GetHubStatus
*
*  Function description
*    Returns the HUB status as defined in the USB specification 11.24.2.6
*    The status bits are returned in bits 0..Bits 15 and the change bits
*    in bits 16..31.
 */
U32 OhRh_GetHubStatus(USBH_HC_HANDLE HcHandle) {
  HC_DEVICE * dev;
  U32         status;
  OH_DEV_FROM_HANDLE(dev, HcHandle);
  OH_DEV_VALID      (dev);
  status = RhReadStatus(dev);
  USBH_LOG((USBH_MTYPE_HUB, "Roothub: OhRh_GetHubStatus: Hub status:  0x%x", status));
  status = OhRhUpdateHubStatusChangeBits(&dev->RootHub, status);
  USBH_LOG((USBH_MTYPE_HUB, "Roothub: OhRh_GetHubStatus: Calc.status: 0x%x", status));
  return status;
}

/*********************************************************************
*
*       OhRh_ClearHubStatus
*
*  Function description
*    This request is identical to an hub class ClearHubFeature request
*    with the restriction that only hub change bits can be cleared.
*    For all other hub features other root hub functions must be used.
*/
void OhRh_ClearHubStatus(USBH_HC_HANDLE HcHandle, U16 FeatureSelector) {
  HC_DEVICE * dev;
  unsigned    change;
  USBH_LOG((USBH_MTYPE_HUB, "Roothub: OhRh_ClearHubStatus"));
  OH_DEV_FROM_HANDLE(dev, HcHandle);
  OH_DEV_VALID      (dev);
  change               = (U16)(HDC_GET_SELECTOR_MASK(FeatureSelector) & 0xffff);
  dev->RootHub.Change &= (U16)~change;  // Clear root hub change bits
}

/*********************************************************************
*
*       OhRh_GetPortStatus
*
*  Function description
*    Returns the port status as defined in the USB specification 11.24.2.7
*    The status bits are returned in bits 0..Bits 15 and the change bits
*    in bits 16..31.
*
*  Parameters:
*    Port: One based index of the port
*/
U32 OhRh_GetPortStatus(USBH_HC_HANDLE HcHandle, U8 Port) {
  HC_DEVICE   * dev;
  HC_HUB_PORT * hcPort;
  U32           status;
  OH_DEV_FROM_HANDLE   (dev, HcHandle);
  OH_DEV_VALID         (dev);
  OH_ASSERT_PORT_NUMBER(dev, Port);
  T_ASSERT((Port - 1) < dev->RootHub.PortCount);
  hcPort = dev->RootHub.apHcdPort + (Port - 1);
  status = RhReadPort(dev, Port);
  USBH_LOG((USBH_MTYPE_HUB, "Roothub: OhRh_GetPortStatus: Port:%d status: 0x%08x",Port,status  ));
  status = OhRhUpdatePortStatusChangeBits(hcPort, status);
  USBH_LOG((USBH_MTYPE_HUB, "Roothub: OhRh_GetPortStatus: status with calculated change bits:0x%08x!",status  ));
  return status;
}

/*********************************************************************
*
*       OhRh_ClearPortStatus
*
*  Function description
*    This request is identical to an hub class ClearPortFeature request
*    with the restriction that only port change bits can be cleared.
*    For all other port features other root hub functions must be used.
*
*  Parameters:
*    Port: One based index of the port
*/
void OhRh_ClearPortStatus(USBH_HC_HANDLE HcHandle, U8 Port, U16 FeatureSelector) {
  HC_DEVICE * dev;
  unsigned    change;
  USBH_LOG((USBH_MTYPE_HUB, "Roothub: OhRh_ClearPortStatus: Port: %u!", Port));
  OH_DEV_FROM_HANDLE   (dev, HcHandle);
  OH_DEV_VALID         (dev);
  OH_ASSERT_PORT_NUMBER(dev, Port);
  // Clear the change bits in the device driver, on the host controller these
  // bits are cleared in the ISR , Port status change bits starts with bit 15.
  // The mask bits are identical to the port status bits.
  change = (U16)((HDC_GET_SELECTOR_MASK(FeatureSelector) >> 16) & 0x0ffff);
  T_ASSERT((Port - 1) < dev->RootHub.PortCount);
  (dev->RootHub.apHcdPort + Port - 1)->Change &= (U16)~change;
}

/*********************************************************************
*
*       OhRh_SetPortPower
*
*  Function description
*    Set the power state of a port. If the HC can not handle the power
*    switching for individual ports, it must turn on all ports if at
*    least one port requires power. It turns off the power if no port
*    requires power
*
*  Parameters:
*    Port:    One based index of the port
*    PowerOn: 1 to turn the power on or 0 for off
*/
void OhRh_SetPortPower(USBH_HC_HANDLE HcHandle, U8 Port, U8 PowerOn) {
  HC_DEVICE   * dev;
  HC_ROOT_HUB * hub;
  USBH_LOG((USBH_MTYPE_HUB, "Roothub: OhRh_SetPortPower: port: %u",Port));
  OH_DEV_FROM_HANDLE(dev, HcHandle);
  OH_DEV_VALID(dev);
  T_ASSERT(Port != 0);
  T_ASSERT(Port <= dev->RootHub.PortCount);
  hub = &dev->RootHub;
  OhRhSetPortPower(hub, Port, PowerOn);
}

/*********************************************************************
*
*       OhRh_ResetPort
*
*  Function description
*    Reset the port (USB Reset) and send a port change notification if
*    ready. If reset was successful the port is enabled after reset and
*    the speed is detected
*
*  Parameters:
*    Port:    One based index of the port
*/
void OhRh_ResetPort(USBH_HC_HANDLE HcHandle, U8 Port) {
  HC_DEVICE * dev;
  USBH_LOG((USBH_MTYPE_HUB, "Roothub: OhRh_ResetPort: port: %u",Port));
  OH_DEV_FROM_HANDLE(dev, HcHandle);
  OH_DEV_VALID(dev);
  T_ASSERT(Port != 0);
  T_ASSERT(Port <= dev->RootHub.PortCount);
  {
    U32 status;
    status = RhReadPort(dev, Port);
    if (!RhIsPortConnected(status)) {
      USBH_LOG((USBH_MTYPE_HUB, "Roothub: FATAL OhRh_ResetPort: No device!" ));
      return;
    }
    if (UmhHalIsSuspended(status)) {
      USBH_LOG((USBH_MTYPE_HUB, "Roothub: FATAL OhRh_ResetPort: Port in suspend!" ));
      return;
    }
    if (RhIsPortResetActive(status)) {
      USBH_LOG((USBH_MTYPE_HUB, "Roothub: FATAL OhRh_ResetPort: Port reset already active!" ));
      return;
    }
    if (RhIsPortResetChangeActive(status)) {
      USBH_WARN((USBH_MTYPE_HUB, "Roothub: OhRh_ResetPort: Port reset change bit is on!" ));
      return;
    }
  }
  RhSetPortReset(dev, Port);  // Start port reset
}

/*********************************************************************
*
*       OhRh_ResetPort
*
*  Function description
*    Disable the port, no requests and SOF's are issued on this port
*
*  Parameters:
*    Port:    One based index of the port
*/
void OhRh_DisablePort(USBH_HC_HANDLE HcHandle, U8 Port) {
  HC_DEVICE * dev;
  USBH_LOG((USBH_MTYPE_HUB, "Roothub: OhRh_DisablePort: port: %u",Port));
  OH_DEV_FROM_HANDLE   (dev, HcHandle);
  OH_DEV_VALID         (dev);
  OH_ASSERT_PORT_NUMBER(dev, Port);
  RhDisablePort        (dev, Port);
}

/*********************************************************************
*
*       OhRh_ResetPort
*
*  Function description
*    Switch the port power between running and suspend
*
*  Parameters:
*    Port:    One based index of the port
*/
void OhRh_SetPortSuspend(USBH_HC_HANDLE HcHandle, U8 Port, USBH_PortPowerState State) {
  HC_DEVICE * dev;
  U32         portStatus;
  USBH_LOG((USBH_MTYPE_HUB, "Roothub: OhRh_SetPortSuspend: Port: %u!",Port ));
  OH_DEV_FROM_HANDLE     (dev, HcHandle);
  OH_DEV_VALID           (dev);
  OH_ASSERT_PORT_NUMBER  (dev, Port);
  portStatus = RhReadPort(dev, Port);
  if (!RhIsPortConnected(portStatus)) {
    USBH_WARN((USBH_MTYPE_HUB, "Roothub: FATAL OhRh_SetPortSuspend: No device!" ));
    return;
  }
  switch (State) {
    case USBH_PortPowerRunning:
      USBH_LOG((USBH_MTYPE_HUB, "Roothub: OhRh_SetPortSuspend: set port: %u in running mode",Port));
      if (!UmhHalIsSuspended(portStatus)) {
        USBH_WARN((USBH_MTYPE_HUB, "Roothub: OhRh_SetPortSuspend: port not in suspend state!"));
      } else {
        RhStartPortResume(dev, Port); // Clears the suspend state
      }
      break;
    case USBH_PortPowerSuspend:
      USBH_LOG((USBH_MTYPE_HUB, "Roothub: OhRh_SetPortSuspend: set port: %u in suspend mode",Port));
      RhSetPortSuspend(dev, Port);
      break;
    default:
      break;
  }
}

/******************************* EOF ********************************/
