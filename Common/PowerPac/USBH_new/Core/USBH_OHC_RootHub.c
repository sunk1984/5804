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
*       ROOT HUB registers
*
**********************************************************************
*/

// Root Hub Descriptor A register
#define ROOT_HUB_MAX_DOWNSTREAM_PORTS 15
#define ROOT_HUB_NDP                  0x0ff       // Number of ports mask
#define ROOT_HUB_PSM                  0x00000100UL // Power switch mode
#define ROOT_HUB_NPS                  0x00000200UL // NoPowerSwitching, ports always on!!
#define ROOT_HUB_DT                   0x00000400UL // Compound device always zero on root hub
#define ROOT_HUB_OCPM                 0x00000800UL // Overcurrent per port
#define ROOT_HUB_NOCP                 0x00001000UL // No overcurrent protection
#define ROOT_HUB_POTPGT_BIT           24          // Power on to power good time in units of 2 ms max.0xff

// Root Hub Descriptor B register
#define ROOT_HUB_DR_MASK(Port)   (1        <<(Port)) // Device is removable
#define ROOT_HUB_PPCM_MASK(Port) (0x10000UL <<(Port)) // Port powered control mask, Port is one indexed!

// Root Hub status register
#define ROOT_HUB_LPS  0x00000001UL // LocalPowerStatus Clear global power
#define ROOT_HUB_OCI  0x00000002UL // OverCurrentIndicator If not per port poover current protection is active
#define ROOT_HUB_DRWE 0x00008000UL // This bit enables a ConnectStatusChange bit as a resume event, causing a USBSUSPEND to USBRESUME
// state transition and setting the ResumeDetected interrupt. Must be used if the host is in
// suspend state and must be wake up if an device is connected or removed in this state. The resume
// signal detection is in suspend state always enabled.
#define ROOT_HUB_LPSC 0x00010000UL       // LocalPowerStatusChange SetGlobalPower
#define ROOT_HUB_OCIC 0x00020000UL       // OverCurrentIndicatorChange.
#define ROOT_HUB_CRWE 0x80000000UL       // ClearRemoteWakeupEnable. Ends the remote wakeup signaling. only write enabled
#define ROOT_HUB_CHANGE_MASK (ROOT_HUB_LPSC | ROOT_HUB_OCIC)

#define RH_PORT_STATUS_CCS  0x00000001UL // CurrentCOnnectStatus. ClearPortEnable Always 1 if the device is nonremovablee
#define RH_PORT_STATUS_PES  0x00000002UL // PortEnableStatus, SetPortEnable
#define RH_PORT_STATUS_PSS  0x00000004UL // PortSuspendStatus, pfSetPortSuspend
#define RH_PORT_STATUS_POCI 0x00000008UL // PortOverCurrentIndicator, ClearSuspendStatus
#define RH_PORT_STATUS_PRS  0x00000010UL // PortResetStatus, SetPortReset
#define RH_PORT_STATUS_PPS  0x00000100UL // PortPowerStatus (regardless of type of power switching mode) SetPortPower
#define RH_PORT_STATUS_LSDA 0x00000200UL // LowSpeedDeviceAttached, ClearPortPower

// Root Hub Port status change bits
#define RH_PORT_CH_BIT_MASK        0x001F0000UL // Change status bits
#define RH_PORT_STATUS_CSC         0x00010000UL // ConnectStatusChange Request + clear
#define RH_PORT_STATUS_PESC        0x00020000UL // PortEnableStatusChnage + Clear
#define RH_PORT_STATUS_PSSC        0x00040000UL // PortSuspendStatusChnage (full resume sequence has been completed) + clear
#define RH_PORT_STATUS_OCIC        0x00080000UL // PortOverCurrentIndicatorChange + clear
#define RH_PORT_STATUS_PRSC        0x00100000UL // Port Reset Status change + clear (end of 10ms port reset signal)
#define HUB_PORT_STATUS_HIGH_SPEED 0x00000400UL
#define HUB_PORT_STATUS_TEST_MODE  0x00000800UL
#define HUB_PORT_STATUS_INDICATOR  0x00001000UL

// Relevant status bits in the root hub port status register.
// This includes: Connect-,Enable-,Suspend-,Overcurrent-,Reset-,Portpower- and Speedstatus.
#define HUB_PORT_STATUS_MASK       0x0000031FUL
#define HUB_PORT_CHANGE_MASK       ( RH_PORT_STATUS_CSC  \
                                   | RH_PORT_STATUS_PESC \
                                   | RH_PORT_STATUS_PSSC \
                                   | RH_PORT_STATUS_OCIC \
                                   | RH_PORT_STATUS_PRSC )


// Root Hub macros
// Port number is an one based index of the port
#define RhReadStatus(pDevice)                        OhHalReadReg((pDevice)->pRegBase, OH_REG_RHSTATUS          ) // Reading and writing HcRhStatus Register
#define RhWriteStatus(pDevice, Status)               OhHalWriteReg((pDevice)->pRegBase,OH_REG_RHSTATUS, (Status))
#define RhReadPort(pDevice, Port)                  OhHalReadReg((pDevice)->pRegBase, GET_PORTSTATUS_REG(Port) ) // Reading and writing hub port status register
#define RhWritePort(pDevice, Port,StatusMask)      OhHalWriteReg((pDevice)->pRegBase,GET_PORTSTATUS_REG(Port  ), (StatusMask))

#define RhSetGlobalPower(pDevice)                  RhWriteStatus((pDevice),        ROOT_HUB_LPSC)                   // switch on global power pins
#define RhClearGlobalPower(pDevice)                RhWriteStatus((pDevice),        ROOT_HUB_LPS)                   // switch off all global ports power
#define RhResetPort(pDevice, Port)                 RhWritePort(  (pDevice),(Port), RH_PORT_STATUS_PRS)             // Start an port based USB reset signal
#define RhSetPortPower(pDevice, Port)              RhWritePort(  (pDevice),(Port), RH_PORT_STATUS_PPS)             // Switch on port power on an port
#define RhClearPortPower(pDevice, Port)            RhWritePort(  (pDevice),(Port), RH_PORT_STATUS_LSDA)            // Switch odd port power on an port
#define RhSetPortSuspend(pDevice, Port)            RhWritePort(  (pDevice),(Port), RH_PORT_STATUS_PSS)
#define RhStartPortResume(pDevice, Port)           RhWritePort(  (pDevice),(Port), RH_PORT_STATUS_POCI)            // clear suspend status starts resume signaling
#define RhClearPortResumeChange(pDevice, Port)     RhWritePort(  (pDevice),(Port), RH_PORT_STATUS_PSSC)
#define RhSetPortReset(pDevice, Port)              RhWritePort(  (pDevice),(Port), RH_PORT_STATUS_PRS)
#define RhDisablePort(pDevice, Port)               RhWritePort(  (pDevice),(Port), RH_PORT_STATUS_CCS)

#define RhIsPortConnected(PortStatus)                 ((PortStatus) & RH_PORT_STATUS_CCS      )
#define RhIsPortResetActive(PortStatus)               ((PortStatus) & RH_PORT_STATUS_PRS      )
#define RhIsPortResetChangeActive(PortStatus)         ((PortStatus) & RH_PORT_STATUS_PRSC     )
#define RhIsPortEnabled(PortStatus)                   ((PortStatus) & RHP_ENABLE_STATUS_CHANGE)
#define RhIsPortConnectChanged(PortStatus)            ((PortStatus) & RH_PORT_STATUS_PESC     )
#define RhIsLowSpeed(PortStatus)                      ((PortStatus) & RH_PORT_STATUS_LSDA ? 1 : 0)
#define UmhHalIsSuspended(PortStatus)                 ((PortStatus) & RH_PORT_STATUS_PSS  ? 1 : 0)



/*********************************************************************
*
*       _UpdateHubStatusChangeBits
*
*  Function description
*    Updates the pRootHub Status change bits
*/
static U32 _UpdateHubStatusChangeBits(USBH_OHCI_ROOT_HUB * pRootHub, U32 NewStatus) {
  U16 hubStatus, change;
  // 1. Generate change bits form old and new Status
  // 2. Add change bits from the new Status
  // 3. Add generated change bits
  // 4. Return updated OHCI root pRootHub Status
  hubStatus   = (U16)(NewStatus & 0x0ffff);
  change      = (U16)(hubStatus ^ pRootHub->Status);
  pRootHub->Change |= (U16)(NewStatus >> 16);
  pRootHub->Change |= change;
  pRootHub->Status  = hubStatus;
  // Returns the new updated Status see also OHCI spec.
  return pRootHub->Status | (((U32)pRootHub->Change) << 16);
}

/*********************************************************************
*
*       _UpdatePortStatusChangeBits
*
*  Function description
*    Updates the pPort Status change bits
*/
static U32 _UpdatePortStatusChangeBits(USBH_OHCI_HUB_PORT * pRootHub, U32 NewStatus) {
  U16 portStatus, change;
  // 1. Generate change bits form old and new Status
  // 2. Or change bits from the new Status
  // 3. Or generated change bits
  // 4. Return updated OHCI pPort Status
  portStatus      = (U16)(NewStatus & 0x0ffff);
  change          = (U16)(portStatus ^ pRootHub->Status);
  pRootHub->Change |= (U16)(NewStatus >> 16);
  pRootHub->Change |= change;
  pRootHub->Status  = portStatus;
  return pRootHub->Status | (((U32)pRootHub->Change) << 16);
}

/*********************************************************************
*
*       _UpdatePortStatusChangeBits
*
*  Function description
*    Returns number of powered Ports on the root pRootHub
*/
static int _GetPoweredPorts(USBH_OHCI_ROOT_HUB * pRootHub) {
  int i, ct;
  for (i = 0, ct = 0; i < pRootHub->PortCount; i++) {
    USBH_OHCI_HUB_PORT * pPort;

    pPort = pRootHub->apHcdPort + i;
    if (pPort->Power) {
      ct++;                     // Count powered on Ports
    }
  }
  USBH_LOG((USBH_MTYPE_HUB, "Roothub: _GetPoweredPorts: number of powered Ports: %d",ct));
  return ct;
}

/*********************************************************************
*
*       _SetPortPower
*
*  Function description
*/
static void _SetPortPower(USBH_OHCI_ROOT_HUB * pRootHub, U8 Port, U8 PowerFlag) {
  USBH_OHCI_HUB_PORT * port;

  USBH_ASSERT(pRootHub->PortCount >= Port); // Port start with 0
  port = pRootHub->apHcdPort + (Port - 1);
  if (USBH_Global.Config.RootHubPerPortPowered) {
    if (PowerFlag) {         // Port powered on/off
      if (!port->Power) {    // No power on pPort
        USBH_LOG((USBH_MTYPE_HUB, "Roothub: _SetPortPower: set pPort power on pPort: %u",Port));
        RhSetPortPower(pRootHub->pDev, Port);
      }
    } else {
      if (port->Power) {     // Power on board
        USBH_LOG((USBH_MTYPE_HUB, "Roothub: _SetPortPower: clear pPort power on pPort: %u",Port));
        RhClearPortPower(pRootHub->pDev, Port);
      }
    }
  } else {
    int PowerCt;
    // Global on/off
    USBH_LOG((USBH_MTYPE_HUB, "Roothub: _SetPortPower: global powered,Portnumber: %u power on/off:%u ",Port,PowerFlag));
    PowerCt = _GetPoweredPorts(pRootHub);
    if (PowerFlag) {
      if (PowerCt == 0) {  // Alls Ports are power off
        USBH_LOG((USBH_MTYPE_HUB, "Roothub: _SetPortPower: global power on!"));
        USBH_WriteReg32(((((pRootHub->pDev))->pRegBase) + (0x050)), ((0x00010000L)));
        RhSetGlobalPower(pRootHub->pDev);
      }
    } else {
      if (PowerCt != 0) {  // Any pPort is power on
        USBH_LOG((USBH_MTYPE_HUB, "Roothub: _SetPortPower: global power off!"));
        RhClearGlobalPower(pRootHub->pDev);
      // Check if must reset Status bits of all Ports that uses the global pPort power
      }
    }
  }
  port->Power = PowerFlag; // Save the new power state
}

/*********************************************************************
*
*       _SetPortPower
*
*  Function description
*    Sets up the root pRootHub register OH_REG_RHDESCRIPTORA and OH_REG_RHDESCRIPTORB!
*    Initializes the root pRootHub object and the root pRootHub register in the host controller
*/
USBH_STATUS USBH_OHCI_ROOTHUB_Init(USBH_OHCI_DEVICE * pDev, USBH_ROOT_HUB_NOTIFICATION_FUNC * pfUbdRootHubNotification, void * pRootHubNotificationContext) {
  USBH_STATUS     Status;
  USBH_OHCI_ROOT_HUB   * pRootHub;
  USBH_OHCI_HUB_PORT   * pPort;
  U32             Ports, HubDesc;
  U32             RhDescA;
  U32             RhDescB = 0;
  unsigned int    i;


  if (USBH_Global.Config.RootHubSupportOvercurrent) {
    RhDescA = 0;
  } else {
    RhDescA = ROOT_HUB_NOCP;
  }
  if (USBH_Global.Config.RootHubPortsAlwaysPowered) {
    RhDescA |= (((POWERON_TO_POWERGOOD_TIME / 2) << ROOT_HUB_POTPGT_BIT) | ROOT_HUB_NPS);
  } else {
    if (USBH_Global.Config.RootHubPerPortPowered) {
      RhDescA |= (((POWERON_TO_POWERGOOD_TIME / 2) << ROOT_HUB_POTPGT_BIT) | ROOT_HUB_OCPM | ROOT_HUB_PSM);
    } else {
      RhDescA |= (((POWERON_TO_POWERGOOD_TIME / 2) << ROOT_HUB_POTPGT_BIT));
    }
  }
  USBH_LOG((USBH_MTYPE_HUB, "Roothub: USBH_OHCI_ROOTHUB_Init"));
  USBH_OCHI_IS_DEV_VALID(pDev);
  pRootHub = &pDev->RootHub;
  // Clears the root pRootHub object
  USBH_ZERO_MEMORY(pRootHub, sizeof(USBH_OHCI_ROOT_HUB));

  pRootHub->pfUbdRootHubNotification    = pfUbdRootHubNotification;
  pRootHub->pRootHubNotificationContext = pRootHubNotificationContext;
  Status                                = USBH_STATUS_SUCCESS;
  pRootHub->pDev                        = pDev;
  Ports                                 = OhHalReadReg(pDev->pRegBase, OH_REG_RHDESCRIPTORA);
  pRootHub->PortCount                   = (U16)(Ports &ROOT_HUB_NDP);
  pRootHub->PowerOnToPowerGoodTime      = POWERON_TO_POWERGOOD_TIME;
  pRootHub->apHcdPort                   = (USBH_OHCI_HUB_PORT *)USBH_MallocZeroed(pRootHub->PortCount * sizeof(USBH_OHCI_HUB_PORT));
  // Setup pRootHub descriptor A
  OhHalWriteReg(pDev->pRegBase, OH_REG_RHDESCRIPTORA, RhDescA);
  OhHalWriteReg(pDev->pRegBase, OH_REG_RHDESCRIPTORB, RhDescB);
  // Init all Ports and clear all pPort Status change bits
  pPort    = pRootHub->apHcdPort;
  HubDesc = 0;
  for (i = 0; i < pRootHub->PortCount; i++, pPort++) {
    pPort->Port = (U8)(i + 1);
    if (USBH_Global.Config.RootHubPortsAlwaysPowered) {
      // This is a host controller where the pPort power can not switched or the user
      // want that the pPort power on all Ports on after initializing the driver
      pPort->Power = TRUE;
    }
    if (USBH_Global.Config.RootHubPerPortPowered) {
      HubDesc |= ROOT_HUB_PPCM_MASK(pPort->Port);
    }
  }
  // Setup pRootHub descriptor B
  HubDesc |= RhDescB;
  OhHalWriteReg(pDev->pRegBase, OH_REG_RHDESCRIPTORB, HubDesc);
  return Status;
}

/*********************************************************************
*
*       USBH_OHCI_ROOTHUB_ProcessInterrupt
*
*  Function description
*    If a RootHub or Status change bit is set then a Bit, a Mask is set
*    and the driver pRootHub and pPort object is updated. The change bit in
*    the pRootHub register is cleared. If the Mask is unequal zero the
*    USBH_RootHubNotification function is called!
*/
void USBH_OHCI_ROOTHUB_ProcessInterrupt(USBH_OHCI_ROOT_HUB * pHub) {
  USBH_OHCI_DEVICE   * pDev;
  USBH_OHCI_HUB_PORT      * pHubPort;
  U8                 Port;
  U32                Status;
  U32                Mask, NotificationMask;

  pDev              = pHub->pDev;
  USBH_OCHI_IS_DEV_VALID(pDev);
  NotificationMask = 0; // Notify the USB bus driver of Status changes in the root pRootHub Bit 0=1 -> root pRootHub changes other bits for pPort changes
  Status           = RhReadStatus(pDev);
  if (0 != (Status &ROOT_HUB_CHANGE_MASK)) {
    RhWriteStatus(pDev, Status & ROOT_HUB_CHANGE_MASK);       // This clears the pRootHub interrupt
    pDev->RootHub.Change |= (U16)(Status >> 16);              // Update the Status change bits with the the physical not self calculated change bits
    NotificationMask    |= 1;                                // Root pRootHub Status changed bit 0
  }
  // Check all pRootHub Ports
  Mask   = 0x02;                                             // Port notification bits starts with bit 1
  pHubPort = pHub->apHcdPort;
  for (Port = 1; Port <= pHub->PortCount; Port++, Mask <<= 1, pHubPort++) {
    Status = RhReadPort(pDev, Port);
    if (0 != (Status & HUB_PORT_CHANGE_MASK)) {
      RhWritePort(pDev, Port, Status & HUB_PORT_CHANGE_MASK); // Change bit on, clear the change bit
      pHubPort->Change   |= (U16)(Status >> 16);               // Set the change bits in the pRootHub context
      NotificationMask |= Mask;
    }
  }
  if (NotificationMask) {
    USBH_LOG((USBH_MTYPE_HUB, "Roothub: UBD notification: RootHub=Bit 0 Ports=Bit 1..31: 0x%lx ", NotificationMask));
    pHub->pfUbdRootHubNotification(pHub->pRootHubNotificationContext, NotificationMask);
  }
}


/*********************************************************************
*
*       Device driver pRootHub interface
*
**********************************************************************
*/

/*********************************************************************
*
*       USBH_OHCI_ROOTHUB_GetPortCount
*
*  Function description
*    Returns the number of root pRootHub Ports
*/
unsigned int USBH_OHCI_ROOTHUB_GetPortCount(USBH_HC_HANDLE hHostController) {
  USBH_OHCI_DEVICE * pDev;
  unsigned int     Count;
  USBH_OHCI_HANDLE_TO_PTR(pDev, hHostController);
  USBH_OCHI_IS_DEV_VALID(pDev);
  // Return the devices shadow counter
  Count = pDev->RootHub.PortCount;
  USBH_LOG((USBH_MTYPE_HUB, "Roothub: USBH_OHCI_ROOTHUB_GetPortCount: Count:%u!",Count));
  return Count;
}

/*********************************************************************
*
*       USBH_OHCI_ROOTHUB_GetPowerGoodTime
*
*  Function description
*    Returns the power on to power good Time in ms
*/
unsigned int USBH_OHCI_ROOTHUB_GetPowerGoodTime(USBH_HC_HANDLE hHostController) {
  USBH_OHCI_DEVICE * pDev;
  unsigned int     Time;
  USBH_OHCI_HANDLE_TO_PTR(pDev, hHostController);
  USBH_OCHI_IS_DEV_VALID(pDev);
  Time = pDev->RootHub.PowerOnToPowerGoodTime;
  USBH_LOG((USBH_MTYPE_HUB, "Roothub: USBH_OHCI_ROOTHUB_GetPowerGoodTime: Time: %u!", Time));
  return Time;
}

/*********************************************************************
*
*       USBH_OHCI_ROOTHUB_GetHubStatus
*
*  Function description
*    Returns the HUB Status as defined in the USB specification 11.24.2.6
*    The Status bits are returned in bits 0..Bits 15 and the change bits
*    in bits 16..31.
 */
U32 USBH_OHCI_ROOTHUB_GetHubStatus(USBH_HC_HANDLE hHostController) {
  USBH_OHCI_DEVICE * pDev;
  U32              Status;

  USBH_OHCI_HANDLE_TO_PTR(pDev, hHostController);
  USBH_OCHI_IS_DEV_VALID      (pDev);
  Status = RhReadStatus(pDev);
  USBH_LOG((USBH_MTYPE_HUB, "Roothub: USBH_OHCI_ROOTHUB_GetHubStatus: pHub Status:  0x%x", Status));
  Status = _UpdateHubStatusChangeBits(&pDev->RootHub, Status);
  USBH_LOG((USBH_MTYPE_HUB, "Roothub: USBH_OHCI_ROOTHUB_GetHubStatus: Calc.Status: 0x%x", Status));
  return Status;
}

/*********************************************************************
*
*       USBH_OHCI_ROOTHUB_ClearHubStatus
*
*  Function description
*    This request is identical to an pRootHub class ClearHubFeature request
*    with the restriction that only pRootHub change bits can be cleared.
*    For all other pRootHub features other root pRootHub functions must be used.
*/
void USBH_OHCI_ROOTHUB_ClearHubStatus(USBH_HC_HANDLE hHostController, U16 FeatureSelector) {
  USBH_OHCI_DEVICE * pDev;
  unsigned         Change;

  USBH_LOG((USBH_MTYPE_HUB, "Roothub: USBH_OHCI_ROOTHUB_ClearHubStatus"));
  USBH_OHCI_HANDLE_TO_PTR(pDev, hHostController);
  USBH_OCHI_IS_DEV_VALID      (pDev);
  Change                = (U16)(HDC_GET_SELECTOR_MASK(FeatureSelector) & 0xffff);
  pDev->RootHub.Change &= (U16)~Change;  // Clear root pRootHub change bits
}

/*********************************************************************
*
*       USBH_OHCI_ROOTHUB_GetPortStatus
*
*  Function description
*    Returns the pPort Status as defined in the USB specification 11.24.2.7
*    The Status bits are returned in bits 0..Bits 15 and the change bits
*    in bits 16..31.
*
*  Parameters:
*    Port: One based index of the pPort
*/
U32 USBH_OHCI_ROOTHUB_GetPortStatus(USBH_HC_HANDLE hHostController, U8 Port) {
  USBH_OHCI_DEVICE * pDev;
  USBH_OHCI_HUB_PORT    * hcPort;
  U32              Status;

  USBH_OHCI_HANDLE_TO_PTR   (pDev, hHostController);
  USBH_OCHI_IS_DEV_VALID         (pDev);
  OH_ASSERT_PORT_NUMBER(pDev, Port);
  USBH_ASSERT((Port - 1) < pDev->RootHub.PortCount);
  hcPort = pDev->RootHub.apHcdPort + (Port - 1);
  Status = RhReadPort(pDev, Port);
  USBH_LOG((USBH_MTYPE_HUB, "Roothub: USBH_OHCI_ROOTHUB_GetPortStatus: Port:%d Status: 0x%08x",Port,Status  ));
  Status = _UpdatePortStatusChangeBits(hcPort, Status);
  USBH_LOG((USBH_MTYPE_HUB, "Roothub: USBH_OHCI_ROOTHUB_GetPortStatus: Status with calculated change bits:0x%08x!",Status  ));
  return Status;
}

/*********************************************************************
*
*       USBH_OHCI_ROOTHUB_ClearPortStatus
*
*  Function description
*    This request is identical to an pRootHub class ClearPortFeature request
*    with the restriction that only pPort change bits can be cleared.
*    For all other pPort features other root pRootHub functions must be used.
*
*  Parameters:
*    Port: One based index of the pPort
*/
void USBH_OHCI_ROOTHUB_ClearPortStatus(USBH_HC_HANDLE hHostController, U8 Port, U16 FeatureSelector) {
  USBH_OHCI_DEVICE * pDev;
  unsigned         Change;

  USBH_LOG((USBH_MTYPE_HUB, "Roothub: USBH_OHCI_ROOTHUB_ClearPortStatus: Port: %u!", Port));
  USBH_OHCI_HANDLE_TO_PTR   (pDev, hHostController);
  USBH_OCHI_IS_DEV_VALID         (pDev);
  OH_ASSERT_PORT_NUMBER(pDev, Port);
  // Clear the change bits in the device driver, on the host controller these
  // bits are cleared in the ISR , Port Status change bits starts with bit 15.
  // The Mask bits are identical to the pPort Status bits.
  Change = (U16)((HDC_GET_SELECTOR_MASK(FeatureSelector) >> 16) & 0x0ffff);
  USBH_ASSERT((Port - 1) < pDev->RootHub.PortCount);
  (pDev->RootHub.apHcdPort + Port - 1)->Change &= (U16)~Change;
}

/*********************************************************************
*
*       USBH_OHCI_ROOTHUB_SetPortPower
*
*  Function description
*    Set the power state of a pPort. If the HC can not handle the power
*    switching for individual Ports, it must turn on all Ports if at
*    least one pPort requires power. It turns off the power if no pPort
*    requires power
*
*  Parameters:
*    Port:    One based index of the pPort
*    PowerOn: 1 to turn the power on or 0 for off
*/
void USBH_OHCI_ROOTHUB_SetPortPower(USBH_HC_HANDLE hHostController, U8 Port, U8 PowerOn) {
  USBH_OHCI_DEVICE * pDev;
  USBH_OHCI_ROOT_HUB    * pRootHub;

  USBH_LOG((USBH_MTYPE_HUB, "Roothub: USBH_OHCI_ROOTHUB_SetPortPower: pPort: %u",Port));
  USBH_OHCI_HANDLE_TO_PTR(pDev, hHostController);
  USBH_OCHI_IS_DEV_VALID(pDev);
  USBH_ASSERT(Port != 0);
  USBH_ASSERT(Port <= pDev->RootHub.PortCount);
  pRootHub = &pDev->RootHub;
  _SetPortPower(pRootHub, Port, PowerOn);
}

/*********************************************************************
*
*       USBH_OHCI_ROOTHUB_ResetPort
*
*  Function description
*    Reset the pPort (USB Reset) and send a pPort change notification if
*    ready. If reset was successful the pPort is enabled after reset and
*    the speed is detected
*
*  Parameters:
*    Port:    One based index of the pPort
*/
void USBH_OHCI_ROOTHUB_ResetPort(USBH_HC_HANDLE hHostController, U8 Port) {
  USBH_OHCI_DEVICE * pDev;
  U32              Status;

  USBH_LOG((USBH_MTYPE_HUB, "Roothub: USBH_OHCI_ROOTHUB_ResetPort: pPort: %u",Port));
  USBH_OHCI_HANDLE_TO_PTR(pDev, hHostController);
  USBH_OCHI_IS_DEV_VALID(pDev);
  USBH_ASSERT(Port != 0);
  USBH_ASSERT(Port <= pDev->RootHub.PortCount);
  Status = RhReadPort(pDev, Port);
  if (!RhIsPortConnected(Status)) {
    USBH_LOG((USBH_MTYPE_HUB, "Roothub: FATAL USBH_OHCI_ROOTHUB_ResetPort: No device!" ));
    return;
  }
  if (UmhHalIsSuspended(Status)) {
    USBH_LOG((USBH_MTYPE_HUB, "Roothub: FATAL USBH_OHCI_ROOTHUB_ResetPort: Port in suspend!" ));
    return;
  }
  if (RhIsPortResetActive(Status)) {
    USBH_LOG((USBH_MTYPE_HUB, "Roothub: FATAL USBH_OHCI_ROOTHUB_ResetPort: Port reset already active!" ));
    return;
  }
  if (RhIsPortResetChangeActive(Status)) {
    USBH_WARN((USBH_MTYPE_HUB, "Roothub: USBH_OHCI_ROOTHUB_ResetPort: Port reset change bit is on!" ));
    return;
  }
  OhHalWriteReg(pDev->pRegBase, OH_REG_INTERRUPTDISABLE, HC_INT_MIE); // disable the interrupt
#if 0
{
#define	PORT_RESET_MSEC		50

/* this timer value might be vendor-specific ... */
#define	PORT_RESET_HW_MSEC	10
#define tick_before(t1,t2) ((I16)(((I16)(t1))-((I16)(t2))) < 0)

	U32	temp;
	U16	now = OhHalReadReg(pDev->pRegBase, OH_REG_FMNUMBER);
	U16	reset_done = now + PORT_RESET_MSEC;

	/* build a "continuous enough" reset signal, with up to
	 * 3msec gap between pulses.  scheduler HZ==100 must work;
	 * this might need to be deadline-scheduled.
	 */
	do {
		/* spin until any current reset finishes */
		for (;;) {
			temp = RhReadPort(pDev, Port);
			if (!(temp & RH_PORT_STATUS_PRS))
				break;
			USBH_OS_Delay(1);
		} 

		if (!(temp & RH_PORT_STATUS_CCS))
			break;
		if (temp & RH_PORT_STATUS_PRSC)
			RhWritePort(pDev, Port, RH_PORT_STATUS_PRSC);

		/* start the next reset, sleep till it's probably done */
		RhWritePort(pDev, Port, RH_PORT_STATUS_PRS);
		USBH_OS_Delay(PORT_RESET_HW_MSEC);
		now = OhHalReadReg(pDev->pRegBase, OH_REG_FMNUMBER);
	} while (tick_before(now, reset_done));
	/* caller synchronizes using PRSC */
}
#else
  RhSetPortReset(pDev, Port);  // Start pPort reset
#endif
  OhHalWriteReg(pDev->pRegBase, OH_REG_INTERRUPTENABLE, HC_INT_MIE); // Now the ISR detects the next interrupt
}

/*********************************************************************
*
*       USBH_OHCI_ROOTHUB_ResetPort
*
*  Function description
*    Disable the pPort, no requests and SOF's are issued on this pPort
*
*  Parameters:
*    Port:    One based index of the pPort
*/
void USBH_OHCI_ROOTHUB_DisablePort(USBH_HC_HANDLE hHostController, U8 Port) {
  USBH_OHCI_DEVICE * pDev;

  USBH_LOG((USBH_MTYPE_HUB, "Roothub: USBH_OHCI_ROOTHUB_DisablePort: pPort: %u",Port));
  USBH_OHCI_HANDLE_TO_PTR   (pDev, hHostController);
  USBH_OCHI_IS_DEV_VALID         (pDev);
  OH_ASSERT_PORT_NUMBER(pDev, Port);
  RhDisablePort        (pDev, Port);
}

/*********************************************************************
*
*       USBH_OHCI_ROOTHUB_ResetPort
*
*  Function description
*    Switch the pPort power between running and suspend
*
*  Parameters:
*    Port:    One based index of the pPort
*/
void USBH_OHCI_ROOTHUB_SetPortSuspend(USBH_HC_HANDLE hHostController, U8 Port, USBH_PORT_POWER_STATE State) {
  USBH_OHCI_DEVICE * pDev;
  U32              PortStatus;

  USBH_LOG((USBH_MTYPE_HUB, "Roothub: USBH_OHCI_ROOTHUB_SetPortSuspend: Port: %u!",Port ));
  USBH_OHCI_HANDLE_TO_PTR     (pDev, hHostController);
  USBH_OCHI_IS_DEV_VALID           (pDev);
  OH_ASSERT_PORT_NUMBER  (pDev, Port);
  PortStatus = RhReadPort(pDev, Port);
  if (!RhIsPortConnected(PortStatus)) {
    USBH_WARN((USBH_MTYPE_HUB, "Roothub: FATAL USBH_OHCI_ROOTHUB_SetPortSuspend: No device!" ));
    return;
  }
  switch (State) {
  case USBH_PORT_POWER_RUNNING:
    USBH_LOG((USBH_MTYPE_HUB, "Roothub: USBH_OHCI_ROOTHUB_SetPortSuspend: set pPort: %u in running mode",Port));
    if (!UmhHalIsSuspended(PortStatus)) {
      USBH_WARN((USBH_MTYPE_HUB, "Roothub: USBH_OHCI_ROOTHUB_SetPortSuspend: pPort not in suspend state!"));
    } else {
      RhStartPortResume(pDev, Port); // Clears the suspend state
    }
    break;
  case USBH_PORT_POWER_SUSPEND:
    USBH_LOG((USBH_MTYPE_HUB, "Roothub: USBH_OHCI_ROOTHUB_SetPortSuspend: set pPort: %u in suspend mode",Port));
    RhSetPortSuspend(pDev, Port);
    break;
  default:
    break;
  }
}

/******************************* EOF ********************************/
