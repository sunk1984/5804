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
File        : IP_ACD.c
Purpose     : Address conflict module.

Literature  :
              [1] Internet Draft - IPv4 Address Conflict Detection - 14th February 2008
                  http://files.stuartcheshire.org/draft-cheshire-ipv4-acd.txt
---------------------------END-OF-HEADER------------------------------
*/
#include <stdlib.h>
#include "IP_Int.h"

typedef enum {
  ACD_STATE_RUN,
  ACD_STATE_RESTART
} ACD_STATE;

//
// ACD interface state
//
typedef struct ACD_STATE  {
  unsigned State;
  unsigned ProbeNum;
  U8       aConflictingHWAddr[6];
  unsigned DefendInterval;
  U32      IPAddr;
  U32      tLastACD;
  U32      (*pfRenewIPAddr)(int IFace);  // Used to renew the IP address if a conflict has been detected during startup.
  int      (*pfDefend)     (int IFace);  // Used to defend the IP address against a conflicting host on the network.
  int      (*pfRestart)    (int IFace);  // Used to restart the address conflict detection.
} ACD_CONTEXT;


/*********************************************************************
*
*       defines, configurable
*
**********************************************************************
*/
#define PROBE_MAX                100  // Maximum number of probes
#define PROBE_DEFAULT              5  // Default number of probes
#define PROBE_MIN                  2  // Minimum number of probes
#define DEFEND_PERIOD_MAX     300000  // 5 minutes
#define DEFEND_PERIOD_DEFAULT   5000  // 5 seconds, default
#define DEFEND_PERIOD_MIN       1000  // 1 second

/*********************************************************************
*
*       Static data
*
**********************************************************************
*/
static ACD_CONTEXT _ACDContext;

/*********************************************************************
*
*       Static code
*
**********************************************************************
*/

/*********************************************************************
*
*       _StoreHWAddr()
*
*  Function description
*    Stores the hardware address of conflicting host
*/
static void _StoreHWAddr(IP_PACKET * pPacket) {
  char * pData;

  pData          = pPacket->pBuffer + 16;   // Ethernet header (14 bytes header + 2 bytes padding)
  pPacket->pData = pData;
  IP_MEMCPY(_ACDContext.aConflictingHWAddr, pData + 0x08, 6);
}

/*********************************************************************
*
*       Public code
*
**********************************************************************
*/

/*********************************************************************
*
*       _HandleAddrConflict()
*
*  Function description
*    Called from the ARP module if a conflict has been detected.
*    Defends the used IP address as described in [1] 2.4.c
*    if no user callback function is defined to change the default
*    defend strategy
*/
static void _HandleAddrConflict(int IFace, IP_PACKET * pPacket) {
  int v;
  int t;
  int Period;
  char * pData;

  IP_LOG((IP_MTYPE_ACD, "ACD: IP conflict detected!"));
  //
  // Check if a user implemented defend strategy is available
  //
  if (_ACDContext.pfDefend) {
    _ACDContext.pfDefend(0);
    return;
  }
  //
  // Defends the IP address as described in [1] 2.4.c
  //
  if (_ACDContext.tLastACD == 0) { // First conflict ?
    //
    // Store tick count to compute intervall between two conflicts.
    // [1] says in section  2.4.c:
    //   "If the host has not seen any other conflicting ARP packets recently within the last
    //   DEFEND_INTERVAL seconds then it MUST record the time that the conflicting ARP packet
    //   was received, and then broadcast one single ARP announcement, giving its own IP and
    //   hardware addresses."
    //
Defend:
    _ACDContext.tLastACD = IP_OS_GetTime32();
    _StoreHWAddr(pPacket);
    IP_WARN((IP_MTYPE_ACD, "ACD: Conflicting host: %h uses IP: %i", _ACDContext.aConflictingHWAddr, _ACDContext.IPAddr));
    IP_LOG((IP_MTYPE_ACD, "ACD: Send gratuitous ARP to defend the %i.", _ACDContext.IPAddr));
    IP_ARP_SendRequest(_ACDContext.IPAddr);
    return;
  } else {
    //
    // Check if it is the first conflict with this host.
    //
    pData = pPacket->pBuffer + 16;   // Ethernet header (14 bytes header + 2 bytes padding)
    v = IP_MEMCMP(_ACDContext.aConflictingHWAddr, pData + 0x08, 6);
    if (v == 0) {                    // Conflict with a known host ?
      //
      // Compute delay between both conflicts
      //
      t      = IP_OS_GetTime32();
      Period = t - _ACDContext.tLastACD;
      if (Period < _ACDContext.DefendInterval) {
        _ACDContext.tLastACD = 0;
        IP_MEMSET(_ACDContext.aConflictingHWAddr, 0, 6);
        //
        // Conflict is in the configured defend intervall.
        // [1] says in section  2.4.c:
        //   "However, if this is not the first conflicting ARP packet the host has seen,
        //   and the time recorded for the previous conflicting ARP packet is within
        //   DEFEND_INTERVAL seconds then the host MUST NOT send another defensive ARP
        //   announcement.  This is necessary to ensure that two misconfigured hosts do
        //   not get stuck in an endless loop flooding the network with broadcast traffic
        //   while they both try to defend the same address."
        //
        // _ACDContext.pfRestart() can be implemented to break these rule and use IP address in any case.
        // A return value of 1 means that the target should keep the IP address.
        // A return value of 0 means that the target should try to get a new IP address.
        //
        v = _ACDContext.pfRestart(0);
        if (v) {  // Keep IP address anyhow and try to defend it.
          //
          // Reset the relevant parts of the ACD context and defend the address
          //
          goto Defend;
        }
      } else {
        goto Defend;
      }
    }
  }
}

/*********************************************************************
*
*       IP_ACD_Config()
*
*  Function description
*    Configures the address conflict detection
*/
void IP_ACD_Config(int IFace, unsigned ProbeNum, unsigned DefendInterval, const ACD_FUNC * pACDContext) {
  IP_USE_PARA(IFace);
  //
  // Check if values are valid
  //
  if (ProbeNum > PROBE_MAX) {
    ProbeNum = PROBE_MAX;
  }
  if (DefendInterval < PROBE_MIN) {
    DefendInterval = PROBE_MIN;
  }
  if (DefendInterval > DEFEND_PERIOD_MAX) {
    DefendInterval = DEFEND_PERIOD_MAX;
  }
  if (DefendInterval < DEFEND_PERIOD_MIN) {
    DefendInterval = DEFEND_PERIOD_MIN;
  }
  _ACDContext.ProbeNum       = ProbeNum;
  _ACDContext.DefendInterval = DefendInterval;
  _ACDContext.pfRenewIPAddr  = pACDContext->pfRenewIPAddr;
  _ACDContext.pfDefend       = pACDContext->pfDefend;
  _ACDContext.pfRestart      = pACDContext->pfRestart;
}


/*********************************************************************
*
*       _CopyIPAddr()
*
*  Function description
*    Copies MAC address into buffer.
*/
static int _CopyIPAddr(int IFace) {
  if (IP_aIFace[IFace].n_ipaddr == 0) {
    IP_LOG((IP_MTYPE_ACD, "ACD: No IP addr. set."));
    return 1;
  } else {
    //
    // Target has an IP address. Store and test before using it
    //
    _ACDContext.IPAddr        = IP_aIFace[IFace].n_ipaddr;
    IP_aIFace[IFace].n_ipaddr = 0;
    return _ACDContext.IPAddr;
  }
}

/*********************************************************************
*
*       IP_ACD_Start()
*
*  Function description
*    Starts the address conflict detection (ACD).
*/
int IP_ACD_Start(int IFace) {
  int ReqCnt;
  int Entry;
  int v;

  //
  // Copy if available targets IP address.
  //
  v = _CopyIPAddr(IFace);
  if (v == 0) {
    return 1;    // Error, no IP address set.
  }
  //
  // Check if we have a link before we start probing
  //
  while (IP_GetCurrentLinkSpeed() == 0) {
    IP_OS_Delay(50);
  }
Start:
  ReqCnt = 0;
  do {
    //
    // Send ARP request to check if address is present in the network.
    //
    IP_ARP_SendRequest(_ACDContext.IPAddr);
    ReqCnt++;
    IP_OS_WaitNetEvent(500);
    //
    // Ckeck if we got an answer
    //
    Entry = IP_ARP_HasEntry(_ACDContext.IPAddr);
    if (Entry != 0) {
      //
      // IP address is in use. Call user callback to
      //
      IP_LOG((IP_MTYPE_ACD, "ACD: IP addr. declined: %i already in use.", _ACDContext.IPAddr));
      if (_ACDContext.pfRenewIPAddr) {
        _ACDContext.IPAddr = _ACDContext.pfRenewIPAddr(IFace);
      }
      IP_LOG((IP_MTYPE_ACD, "ACD: Start testing %i.", _ACDContext.IPAddr));
      goto Start;
    } else {
      if (ReqCnt == _ACDContext.ProbeNum) {
        //
        // If we have no reply for our requests, use the IP address and send an ARP announcement
        // to clean the cache of the other host on the network.
        //
        IP_aIFace[0].n_ipaddr = _ACDContext.IPAddr;
        //
        // Send the ARP announcement (gratious ARP)
        // An ARP announcement is usually an ARP request containing the sender's protocol address (SPA)
        // in the target field (TPA = SPA)
        //
        IP_ARP_SendRequest(IP_aIFace[IFace].n_ipaddr);
        //
        // Add the address conflict handler to the ARP module.
        //
        IP_ARP_AddACDHandler(_HandleAddrConflict);
        IP_LOG((IP_MTYPE_ACD, "ACD: IP addr. checked, no conflicts. Using %i", _ACDContext.IPAddr));
      }
    }
  } while (ReqCnt < _ACDContext.ProbeNum);
  return 0;
}

/*************************** End of file ****************************/
