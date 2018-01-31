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
File    : IP_DHCPc.c
Purpose : DHCP client implementation
--------  END-OF-HEADER  ---------------------------------------------

RFC 1533: DHCP Options and BOOTP Vendor Extensions
RFC 2131: Dynamic Host Configuration Protocol         March 1997

 --------                               -------
|        | +-------------------------->|       |<-------------------+
| INIT-  | |     +-------------------->| INIT  |                    |
| REBOOT |DHCPNAK/         +---------->|       |<---+               |
|        |Restart|         |            -------     |               |
 --------  |  DHCPNAK/     |               |                        |
    |      Discard offer   |      -/Send DHCPDISCOVER               |
-/Send DHCPREQUEST         |               |                        |
    |      |     |      DHCPACK            v        |               |
 -----------     |   (not accept.)/   -----------   |               |
|           |    |  Send DHCPDECLINE |           |                  |
| REBOOTING |    |         |         | SELECTING |<----+            |
|           |    |        /          |           |     |DHCPOFFER/  |
 -----------     |       /            -----------   |  |Collect     |
    |            |      /                  |   |       |  replies   |
DHCPACK/         |     /  +----------------+   +-------+            |
Record lease, set|    |   v   Select offer/                         |
timers T1, T2   ------------  send DHCPREQUEST      |               |
    |   +----->|            |             DHCPNAK, Lease expired/   |
    |   |      | REQUESTING |                  Halt network         |
    DHCPOFFER/ |            |                       |               |
    Discard     ------------                        |               |
    |   |        |        |                   -----------           |
    |   +--------+     DHCPACK/              |           |          |
    |              Record lease, set    -----| REBINDING |          |
    |                timers T1, T2     /     |           |          |
    |                     |        DHCPACK/   -----------           |
    |                     v     Record lease, set   ^               |
    +----------------> -------      /timers T1,T2   |               |
               +----->|       |<---+                |               |
               |      | BOUND |<---+                |               |
  DHCPOFFER, DHCPACK, |       |    |            T2 expires/   DHCPNAK/
   DHCPNAK/Discard     -------     |             Broadcast  Halt network
               |       | |         |            DHCPREQUEST         |
               +-------+ |        DHCPACK/          |               |
                    T1 expires/   Record lease, set |               |
                 Send DHCPREQUEST timers T1, T2     |               |
                 to leasing server |                |               |
                         |   ----------             |               |
                         |  |          |------------+               |
                         +->| RENEWING |                            |
                            |          |----------------------------+
                             ----------
          Figure 5:  State-transition diagram for DHCP clients


3.1 Client-server interaction - allocating a network address
5. The client receives the DHCPACK message with configuration
     parameters.  The client SHOULD perform a final check on the
     parameters (e.g., ARP for allocated network address), and notes the
     duration of the lease specified in the DHCPACK message.  At this
     point, the client is configured.  If the client detects that the
     address is already in use (e.g., through the use of ARP), the
     client MUST send a DHCPDECLINE message to the server and restarts
     the configuration process.  The client SHOULD wait a minimum of ten
     seconds before restarting the configuration process to avoid
     excessive network traffic in case of looping.

     If the client receives a DHCPNAK message, the client restarts the
     configuration process.

RFC 2131, 4.1 Constructing and sending DHCP messages, p.23
   ...
   DHCP clients are responsible for all message retransmission.  The
   client MUST adopt a retransmission strategy that incorporates a
   randomized exponential backoff algorithm to determine the delay
   between retransmissions.  The delay between retransmissions SHOULD be
   chosen to allow sufficient time for replies from the server to be
   delivered based on the characteristics of the internetwork between
   the client and the server.

*/


#include "IP_Int.h"

/*********************************************************************
*
*       Defines, configurable
*
**********************************************************************
*/
#define DHCP_REQLIST               0  // If enabled: Use "Parameter Request List" acc. to RFC1533, 9.6 Parameter Request List to specify requested parameters
#define DHCP_MAXDNSRVS             2

#define DHCP_MAX_TRIES             4  // Number of retires to tbe done
#define DHCP_RETRY_TMO          4000  // Timeout for retries (ms)
#define CHECK_IP_BEFORE_BOUND      1  // If enabled, send ARP packet to verify IP addr is not in use

//#define  IP_FILTER        0x0401a8c0  // Optional when one DHCP server needs to be selected, usually for tests. Undefine for production code.

/*********************************************************************
*
*       #define constants
*
**********************************************************************
*/
#define BOOTP_SERVER_PORT            67
#define BOOTP_CLIENT_PORT            68

#define DHCPC_BROADCAST_FLAG     0x8000
#define DHCPC_INFINITY       0xFFFFFFFF

//
// DHCP packet types
//
#define  DHCP_INVALIDOP 99
#define  DHCP_DISCOVER  1
#define  DHCP_OFFER     2
#define  DHCP_REQUEST   3
#define  DHCP_DECLINE   4
#define  DHCP_ACK       5
#define  DHCP_NAK       6
#define  DHCP_RELEASE   7

//
// DHCP options
//
#define  DHCP_OPTION_PAD              0    // Padding - 0
#define  DHCP_OPTION_SNMASK           1    // Client's subnet mask
#define  DHCP_OPTION_ROUTER           3    // Set default router
#define  DHCP_OPTION_DNSRV            6    // IP address of domain name server
#define  DHCP_OPTION_NAME            12    // Name
#define  DHCP_OPTION_DOMAIN          15    // Domain name
#define  DHCP_OPTION_CADDR           50    // Client requested IP address
#define  DHCP_OPTION_LEASE           51    // Lease time
#define  DHCP_OPTION_TYPE            53    // DHCP type of DHCP packet, see DHCP packet types
#define  DHCP_OPTION_SERVER          54    // Server ID (IP address)
#define  DHCP_OPTION_REQLIST         55    // Client's parameter request list: RFC 1533, 9.6. Parameter Request List
#define  DHCP_OPTION_RENEWAL         58    // Renewal time (T1)
#define  DHCP_OPTION_REBINDING       59    // Rebinding time (T2)
#define  DHCP_OPTION_CLIENT          61    // Client ID (i.e. hardware address)
#define  DHCP_OPTION_END            255    // Marks end of valid options

//
// Network masks
//
#define  AMASK    0x80000000
#define  AADDR    0x00000000
#define  BMASK    0xC0000000
#define  BADDR    0x80000000
#define  CMASK    0xE0000000
#define  CADDR    0xC0000000

//
// DHCP client states
//
#define  DHCPC_STATE_UNUSED       0  /* no discovery attempted */
#define  DHCPC_STATE_INIT         1  /* Ready to send a DISCOVER packet */
#define  DHCPC_STATE_INITREBOOT   2  /* Have IP, ready to send REQUEST(skip DISCOVER)*/
#define  DHCPC_STATE_REBOOTING    3  /* rebooting/reclaiming address */
#define  DHCPC_STATE_SELECTING    4  /* discovery sent, but no offer yet */
#define  DHCPC_STATE_REQUESTING   5  /* sent request; waiting for ACK|NAK */
#define  DHCPC_STATE_BOUND        6  /* got a ACK we liked */
#define  DHCPC_STATE_RENEWING     7  /* Renewing the lease */
#define  DHCPC_STATE_REBINDING    8  /* rebinding to new server */
#define  DHCPC_STATE_RESTARTING   9  /* Temp. state. Only to inform callback() */
#define  DHCPC_STATE_CHECK_IP    10  // Not part of the diagram: This state is entered before "BOUND", when waiting for answer to ARP-packet to make sure IP-addr is not in use
#define  DHCPC_STATE_WAIT_INIT   11  // Not part of the diagram: This state is entered if CHECK_IP fails. RFC2131, 3.1.5 says: The client SHOULD wait a minimum of ten seconds before restarting the configuration process to avoid excessive network traffic in case of looping.

//
// Packet op codes
//
#define  BOOTP_REQUEST            1
#define  BOOTP_REPLY              2

//
// RFC1048 options for the 'BOOTP Vendor Information' field
//
#define  RFC1084_MAGIC_COOKIE htonl(0x63825363)
#define  RFC1084_PAD          ((U8) 0)
#define  RFC1084_SUBNET_MASK  ((U8) 1)
#define  RFC1084_GATEWAY      ((U8) 3)
#define  RFC1084_END          ((U8) 255)

#define  BOOTP_OPTSIZE      64   // Older value
#define  DHCP_OPTSIZE      312   // Newer value

/*********************************************************************
*
*       Local data types
*
**********************************************************************
*/

//
// The structure of a BOOTP/DHCP packet. This is the
// UDP data area of a BOOTP or DHCP packet.
//
typedef struct bootp {
//
//    Field                   | Offset | Description (RFC951, section 3. Packet Format)
// -------------------------------------------------------------------------------------------------------------------------
  U8  op;                     //  0x00 - Packet op code / message type. 1 = BOOTREQUEST, 2 = BOOTREPLY
  U8  htype;                  //       - Hardware address type, see ARP section in "Assigned Numbers" RFC. '1' = 10mb ethernet
  U8  hlen;                   //       - Hardware address length (eg '6' for 10mb ethernet).
  U8  hops;                   //       - Client sets to zero, optionally used by gateways in cross-gateway booting.
  U32 xid;                    //  0x04 - Transaction ID, a random number, used to match this boot request with the responses it generates.
  U16 secs;                   //  0x08 - Filled in by client, seconds elapsed since client started trying to boot.
  U16 flags;                  //  0x0A - Unused
  U32 ciaddr;                 //  0x0C - Client IP address; filled in by client in bootrequest if known.
  U32 yiaddr;                 //  0x10 - 'your' (client) IP address; filled by server if client doesn't know its own address (ciaddr was 0).
  U32 siaddr;                 //  0x14 - Server IP address; returned in bootreply by server.
  U32 giaddr;                 //  0x18 - Gateway IP address, used in optional cross-gateway booting.
  U8  chaddr[16];             //  0x1C - Client hardware address, filled in by client.
  U8  sname[64];              //  0x2C - Optional server host name, null terminated string.
  U8  file[128];              //  0x6C - Boot file name, null terminated string; 'generic' name or null in bootrequest, fully qualified directory-path name in bootreply.
  U8  options[BOOTP_OPTSIZE]; //  0xEC - Optional vendor-specific area, e.g. could be hardware type/serial on request, or 'capability' / remote file system handle on reply. This info may be set aside for use by a third phase bootstrap or kernel.
} BOOTP;

/* DHCP client per-interface state: */
typedef struct DHC_STATE {
  unsigned state;
  int      tries;         /* retry count of current state */
  U32      xid;           /* last xid sent */
  U16      secs;          /* seconds since client came up */
  U32      last_tick;     /* time of last DHCP packet */
  U32      lease;         /* lease; only valid if state == DHCPC_STATE_BOUND */
  U32      t1 ;           /* lease related - renew timer */
  U32      t2 ;           /* lease related - rebind timer */
  U32      lease_start;   /* time  in cticks   of when  lease started */
  /* Configuration  options  of outstanding request */
  ip_addr  IpAddr;        /* IP address */
  ip_addr  snmask;        /* subnet mask */
  ip_addr  defgw;         /* default gateway (router) */
  ip_addr  rly_ipaddr;    /* IP addr of our RELAY agent (if any) */
  /* IP Address of the DHCP Server.
  * Needed   to send  unicast  when  renewing IP Addr
  */
  ip_addr  srv_ipaddr;
  ip_addr  dnsrv[DHCP_MAXDNSRVS];   /* domain name server addresses */
  int      (*callback)(int IFIndex, int state);  /* callback when IPaddress set */
  int      (*pfCheck) (DHCPC_CHECK_PARAS * pParas);
  U16      UseDHCP;       // Flag to decide if the stack should send plain BOOTP or DHCP requests.
#if (IP_DEBUG >= 1)
  char      sService[6];      // Flag to decide if the stack should send plain BOOTP or DHCP requests.
#endif
} DHC_STATE;

#define USE_BOOTP 0xFF00
#define USE_DHCP  0xFF01

/*********************************************************************
*
*       static const
*
**********************************************************************
*/

#if IP_DEBUG > 0
static const char * dhc_state_str[] = {
  "unused",
  "init",
  "init-reboot",
  "rebooting",
  "selecting",
  "requesting",
  "bound",
  "renewing",
  "rebinding",
  "restarting",
};
#endif

/*********************************************************************
*
*       Static data
*
**********************************************************************
*/
static const char * _sDomain;
static const char * _sVendor;
static const char * _sHost;        // Host name option. See. RFC1533, 3.14, option 12
static const U8   * _pClientId;    // Client Id. See. RFC1533, 9.12, option 61
static unsigned     _ClientIdLen;

static UDPCONN _hConnection;

static U32      _xids       = 0x22334455;    /* seed for unique request IDs */
static U32      _MaxTries   = DHCP_MAX_TRIES;
static U32      _Timeout    = DHCP_RETRY_TMO;
static U8       _ExpTimeout = 1;

/* DHCP client statistics: */
typedef struct {
  U32 errors;         /* protocol/implementation runtime errors */
  U32 discovers;      /* discovers sent */
  U32 offers;         /* offers recieved */
  U32 requests;       /* requests sent */
  U32 acks;           /* acks received */
  U32 bpreplys;       /* plain bootp replys received */
  U32 declines;       /* declines sent */
  U32 releases;       /* releases sent */
  U32 naks;           /* naks received */
  U32 renew;          /* unicast requests sent to renew lease */
  U32 rebind;         /* broadcast requests sent to renew lease */
  U32 rlyerrs;        /* runtime errors due to DHCP Relay agents */
}  IP_DHCPC_STAT;

IP_DHCPC_STAT IP_DHCPC_Stat;

static U32 _sysuptime;

/* reqlist contains the list of options to be requested in a DISCOVER
 * pkt. reqlist is used only if DHCP_REQLIST is enabled.
 */
#ifdef DHCP_REQLIST
static U8  reqlist[]   = { DHCP_OPTION_SNMASK, DHCP_OPTION_ROUTER, DHCP_OPTION_DNSRV, DHCP_OPTION_DOMAIN };
static int reqlist_len = sizeof(reqlist)/sizeof(U8);
#endif

static DHC_STATE _aState[IP_MAX_IFACES]; // DHCP client state of each net.



/*********************************************************************
*
*       static code
*
**********************************************************************
*/

/*********************************************************************
*
*       _PutOptU32
*/
static void _PutOptU32(U8 ** ppDest, U8 Code, U32 Data) {
  U8 * pDest;

  pDest    = *ppDest;
  *pDest++ = Code;           // Type
  *pDest++ = 4;              // Num data bytes
  IP_StoreU32BE(pDest, Data);
  pDest   += 4;
  *ppDest  = pDest;
}

/*********************************************************************
*
*       _PutOpt
*/
static void _PutOpt(U8 ** ppDest, U8 Code, const U8 * p, int NumBytes) {
  U8 * pDest;

  if (p) {
    pDest    = *ppDest;
    *pDest++ = Code;           // Type
    *pDest++ = NumBytes;       // Num data bytes
    IP_MEMCPY(pDest, p, NumBytes);
    pDest   += NumBytes;
    *ppDest  = pDest;
  }
}

/*********************************************************************
*
*       _PutOptString
*/
static void _PutOptString(U8 ** ppDest, U8 Code, const char * s) {
  int NumBytes;

  if (s) {
    NumBytes = strlen(s);
    _PutOpt(ppDest, Code, (const U8*)s, NumBytes);
  }
}

/*********************************************************************
*
*       _ExtractOpts()
*
*  Function description
*    Extract certain interesting options from the
*    options string passed. These options are filled in to the fields
*    passed.
*
*  Return value:
*    0   If the parsing of the option string was sucessful.
*    !=0 In case of a parsing error.
*
*  Note:
*    A return value of 0 does not mean that all (or even any) of the
*    options passed were filled in with good values.
*/
static int _ExtractOpts(int IFace, U8 * pOpt) {
  U8 * end   =  pOpt  +  DHCP_OPTSIZE;  /* limit scope of search */
  U8   optlen;
  U8   Type;
  U32 v;
#if defined(DHCP_MAXDNSRVS) && (DHCP_MAXDNSRVS > 0)
  int   i;
#endif

  //
  // Clear the options
  //
  _aState[IFace].snmask = 0;
  _aState[IFace].defgw  = 0;
  _aState[IFace].lease  = DHCPC_INFINITY;
#if defined(DHCP_MAXDNSRVS) && (DHCP_MAXDNSRVS > 0)
  IP_MEMSET(_aState[IFace].dnsrv, 0, sizeof(_aState[IFace].dnsrv));
#endif
  //
  //
  //
  while (pOpt <= end) {
    Type = *pOpt++;
    if (Type == DHCP_OPTION_END) {
      return 0;
    }
    if (Type != DHCP_OPTION_PAD) {
      optlen = *pOpt++;
      switch (Type) {
      case DHCP_OPTION_SNMASK:
        _aState[IFace].snmask = IP_LoadU32TE(pOpt);
        break;
      case DHCP_OPTION_ROUTER:
        if (optlen >= 4) {
          _aState[IFace].defgw = IP_LoadU32TE(pOpt);
        }
        break;
      case DHCP_OPTION_LEASE:
        v = IP_LoadU32BE(pOpt);
        IP_LOG((IP_MTYPE_DHCP_EXT, "%s: Lease time: %d min.", _aState[IFace].sService, v/60));
        _aState[IFace].lease = v;
        break;
      case DHCP_OPTION_DNSRV:
#if defined(DHCP_MAXDNSRVS) && (DHCP_MAXDNSRVS > 0)
        i = 0;
        while ((optlen >= 4) && (i < DHCP_MAXDNSRVS)) {
          U32 IpAddr;
          IpAddr  = IP_LoadU32TE(pOpt);
          _aState[IFace].dnsrv[i] = IpAddr;
          IP_LOG((IP_MTYPE_DHCP_EXT, "%s: DNS Server[%u]: %i.", _aState[IFace].sService, i, IpAddr));
          optlen -= 4;
          pOpt   += 4;
          i++;
        }
#endif
        break;
      case DHCP_OPTION_RENEWAL:     //   58: renewal (T1) time
        v = IP_LoadU32BE(pOpt);
        IP_LOG((IP_MTYPE_DHCP_EXT, "%s: Renewal time: %d min.", _aState[IFace].sService, v/60));
        break;
      case DHCP_OPTION_REBINDING:   //   59: rebinding (T2) time
        v = IP_LoadU32BE(pOpt);
        IP_LOG((IP_MTYPE_DHCP_EXT, "%s: Rebinding time: %d min.", _aState[IFace].sService, v/60));
        break;
      case DHCP_OPTION_SERVER:
         break;                     // Well known option, ignored here.
      case DHCP_OPTION_TYPE:
         break;                     // Well known option, ignored here.
      case DHCP_OPTION_NAME:
        IP_LOG((IP_MTYPE_DHCP_EXT, "%s: Host name received.", _aState[IFace].sService));
        break;
      default:
         IP_LOG((IP_MTYPE_DHCP_EXT, "%s: unsupported option %d", _aState[IFace].sService, Type));
         break;
      }
      pOpt += optlen;
    }
  }
  IP_WARN((IP_MTYPE_DHCP, "%s: Illegal options received", _aState[IFace].sService));
  return -1;
}


/*********************************************************************
*
*       _FindOption()
*
*  Function description
*    Search an options string for a particular option code.
*
*  Return value:
*    Pointer to the searched opcode.
*    NULL if not found.
*/
static U8 * _FindOption(U8 opcode, U8 * pOptions) {
  U8 * pEnd;

  pEnd = pOptions + DHCP_OPTSIZE;
  while (pOptions < pEnd)    {
    if (*pOptions == opcode) {
      return pOptions;
    }
    if (*pOptions == DHCP_OPTION_END) {
      return NULL;
    }
    if (*pOptions == DHCP_OPTION_PAD) { /* PAD has only 1 byte */
      pOptions++;
    } else {
      pOptions += (*(pOptions+1)) + 2;  // Skip length field
    }
  }
  return NULL;
}

/*********************************************************************
*
*       _GetServerIP()
*
*  Function description
*    Extracts the server IP from the DCHP options.
*
*  Return value:
*    OK:  Server IP
*    Bad: 0
*/
static ip_addr _GetServerIP(U8 * pOpts) {
  IP_ADDR SrvIP;
  U8 * pOptions;
  U8   NumBytes;

  SrvIP = 0;
  if ((pOptions = _FindOption(DHCP_OPTION_SERVER, pOpts)) != NULL) {
    pOptions++;
    NumBytes  = *pOptions;
    pOptions++;
    SrvIP     = IP_LoadU32TE(pOptions);
    pOptions += NumBytes;
  }
  return SrvIP;
}

/*********************************************************************
*
*       _SetState()
*
*  Function description
*    Sets the new state for the interface and calls the callback
*    routine if available.
*/
static void _SetState(int IFIndex, int state) {
  _aState[IFIndex].state = state;
  _aState[IFIndex].tries = 0;
  if (_aState[IFIndex].callback) {
    _aState[IFIndex].callback(IFIndex,state);
  }
}

/*********************************************************************
*
*       _BuildHeader()
*
*  Function description
*    Build BOOTP request header.
*/
static void _BuildHeader(int IFIndex, BOOTP * pOutBOOTP) {
  NET * pIFace;
  int AddrLen;
  int i;

  pIFace  = &IP_aIFace[IFIndex];
  AddrLen = sizeof(pIFace->abHWAddr);
  IP_MEMSET(pOutBOOTP, 0, sizeof(struct bootp));
  //
  // Fill the BOOTP header
  //
  pOutBOOTP->op    = BOOTP_REQUEST;
  pOutBOOTP->htype = 1;             // See ARP section of RFC1700
  pOutBOOTP->hlen  = AddrLen;       // Ethernet MAC: 6 bytes
  pOutBOOTP->hops  = 0;
  if(_aState[IFIndex].state == DHCPC_STATE_RENEWING) {
    pOutBOOTP->flags = 0;           // Renewing needs unicast
  } else {
    pOutBOOTP->flags = htons(DHCPC_BROADCAST_FLAG); // Otherwise broadcast
  }
  pOutBOOTP->xid   = _aState[IFIndex].xid;
  pOutBOOTP->secs  = ntohl(_aState[IFIndex].secs);
  for (i = 0; i < AddrLen; i++) {
    if (pIFace->abHWAddr[i]) {
      goto Done;
    }
  }
  IP_PANIC("Hardware addr. not set!");
Done:
  IP_MEMCPY(pOutBOOTP->chaddr, pIFace->abHWAddr, AddrLen);
}

/********************************************************************
*
*       _FixupSubnetMask
*
* Function description
*   If not subnet mask has been set, use the default based on the IP address.
*/
static void _FixupSubnetMask(NET * pIFace) {
  U32 smask;
  U32 IpAddr;
#if IP_DEBUG > 1
  U8  IFIndex;

  IFIndex = 0;
#endif
  if (pIFace->snmask) {  // If subnet mask is already set, do not bother
    return;
  }
  IpAddr = htonl(pIFace->n_ipaddr);
  //
  // Select matching subnet mask.
  //
  if ((IpAddr & AMASK) == AADDR) {
    smask = 0xFF000000L;
  } else if((IpAddr & BMASK) == BADDR) {
    smask = 0xFFFF0000L;
  } else if((IpAddr & CMASK) == CADDR) {
    smask = 0xFFFFFF00L;
  } else {
    IP_WARN((IP_MTYPE_DHCP, "%s: Illegal subnet mask", _aState[IFIndex].sService));
    smask = 0xFFFFFF00L;
  }
  pIFace->snmask = htonl(smask);
}

/*********************************************************************
*
*       _SetIP()
*
* Function description
*   Sets the IP address (& other paras) for the interface
*   It is called when we have received an ACK. That is, we have the IP
*   address and other information needed for this interface. If the
*   subnet mask has not been specified by the DHCP Server, then we
*   will calculate one from the IP Address using fixup_subnet_mask()
*
*/
static void _SetIP(int IFIndex) {
  NET     * pIFace;
  DHC_STATE * pState;
  int         MaxDNSServers;

  pIFace          = &IP_aIFace[IFIndex];
  pState          = &_aState[IFIndex];
  IP_LOG((IP_MTYPE_DHCP, "%s: IFace %d: Using IP: %i, Mask: %i, GW: %i.", _aState[IFIndex].sService, IFIndex, pState->IpAddr, pState->snmask, pState->defgw));
  pIFace->n_ipaddr  = pState->IpAddr;
  pIFace->snmask  = pState->snmask;
  pIFace->n_defgw = pState->defgw;
  //
  // Check if a subnet mask has been set.
  //
  if ( pIFace->snmask == 0 ) {
    _FixupSubnetMask(pIFace);
    pState->snmask = pIFace->snmask;
  }
  //
  // Update DNS Server of interface
  //
  MaxDNSServers = IP_MIN(DHCP_MAXDNSRVS, IP_MAX_DNS_SERVERS);
  IP_MEMCPY(IP_aIFace[IFIndex].aDNSServer, _aState[IFIndex].dnsrv, 4 * MaxDNSServers);
}

/*********************************************************************
*
*       _SendDeclinePacket()
*
* _SendDeclinePacket() - send a decline packet to server. This is usually
* because he didn't send us an IP address.
*
* RETURNS: Returns 0 if ok, else non-zero IP_ERR_ error.
*/
static int _SendDeclinePacket(int IFIndex, BOOTP * pBOOTP, unsigned bplen) {
  BOOTP * pOutBOOTP;
  IP_PACKET * pPacket;
  U8 * pOptions;

  //
  // Alloc packet
  //
  pPacket = (IP_PACKET *)IP_UDP_Alloc(bplen);
  if (!pPacket) {
    IP_WARN((IP_MTYPE_DHCP, "%s: Can not send decline!", _aState[IFIndex].sService));
    return IP_ERR_NOMEM;
  }
  pPacket->NumBytes = bplen;
  //
  // Fill the packet with the BOOTP header data
  //
  pOutBOOTP     = (BOOTP *)pPacket->pData;
  IP_MEMCPY(pOutBOOTP, pBOOTP, bplen);
  pOutBOOTP->op = BOOTP_REQUEST;
  //
  // Find DHCP TYPE option to overwrite it
  //
  pOptions  = _FindOption(DHCP_OPTION_TYPE, &pOutBOOTP->options[4]);
  pOptions += 2;              // Point to the actual opcode
  *pOptions = DHCP_DECLINE;   // Overwrite it
  //
  // Send DHCP decline as broadcast.
  //
  IP_UDP_SendAndFree(IFIndex, 0xFFFFFFFF, BOOTP_SERVER_PORT, BOOTP_CLIENT_PORT, pPacket);
  IP_STAT_INC(IP_DHCPC_Stat.declines);
  return 0;
}

/*********************************************************************
*
*       _SendARPRequest()
*
*  Function description
*    Sends ARP request in order to find out if the IP addr. is already in use.
*/
static void _SendARPRequest(int IFIndex) {
  DHC_STATE * pState;

  pState = &_aState[IFIndex];
  IP_ARP_SendRequest(pState->IpAddr);
}

/*********************************************************************
*
*       _Send()
*
*  Function description
*    Called when
*    1. we have received a DHCP offer (via dhc_rx_offer() ).
*    2. we have to renew a lease with a DHCP server. (via _SendReclaim()).
*    3. we have to rebind to a different DHCP server.
*    4. we are in INIT-REBOOT state (via _SendReclaim() ).
*    This should format & send a request. Values are picked from
*    _aState[]. It handles the specific nuances of a REQUEST
*    depending on the state of DHCP Client. It implements the following
*    table of RFC2131 (sec 4.3.6).
*    ---------------------------------------------------------------------
*    |              |INIT-REBOOT |SELECTING |RENEWING   |REBINDING |
*    ---------------------------------------------------------------------
*    |broad/unicast |broadcast   |broadcast |unicast    |broadcast |
*    |server-ip     |MUST NOT    |MUST      |MUST NOT   |MUST NOT  |
*    |requested-ip  |MUST        |MUST      |MUST NOT   |MUST NOT  |
*    |ciaddr        |zero        |zero      |IP address |IP address|
*    ---------------------------------------------------------------------
*    In a DISCOVER->OFFER->REQUEST scenario, we need to use the same
*    XID, SECS field as in OFFER packet. This is controlled via
*    XIDFlag. If XIDFlag is TRUE, then use a new XID. Else use the
*    old one.
*/
static int _Send(int IFIndex, int XIDFlag, int Type) {
  struct  bootp * pOutBOOTP;
  IP_PACKET *     pPacket;
  U8  *           pOptions;
  unsigned        State;
  IP_ADDR         FHost;

  //
  // Alloc packet
  //
  pPacket = IP_UDP_Alloc(sizeof(struct bootp) + DHCP_OPTSIZE - BOOTP_OPTSIZE);
  if (!pPacket) {
    IP_WARN((IP_MTYPE_DHCP, "%s: Can not send packet: %i!", _aState[IFIndex].sService, Type));
    return IP_ERR_NOMEM;
  }
  pPacket->NumBytes = sizeof(struct bootp) - BOOTP_OPTSIZE;
  //
  // In a DISCOVER->OFFER->REQUEST scenario, the XID, SECS field of the OFFER packet needs to be used.
  //
  if (XIDFlag) {
    _aState[IFIndex].xid  = _xids++;
    _aState[IFIndex].secs = 0;
  }
  //
  // Build BOOTP request header
  //
  pOutBOOTP = (struct bootp *)pPacket->pData;
  _BuildHeader(IFIndex, pOutBOOTP);
  //
  // Add boot vendor specific information field. The 'vend' field can be filled in by
  // the client with vendor-specific strings or structures. RFC951 says that the operation
  // of the BOOTP server should not DEPEND on this information existing. To enhance the
  // compatibility with not fully RFC-compliant implemented servers, we add always the
  // magic cookie (see RFC951 and RFC1048).
  //
  *(U32*)(&pOutBOOTP->options) = RFC1084_MAGIC_COOKIE;
  pOptions    = &pOutBOOTP->options[4];
  if (_aState[IFIndex].UseDHCP & 0x00FF) {
    //
    // Change the BOOTP packet to a DHCP REQUEST packet
    //
    *pOptions++ = DHCP_OPTION_TYPE;   // Set DHCP type option.
    *pOptions++ = 1;                  // Length of the type option field.
    *pOptions++ = Type;               // Set the type of the packet.
    //
    // Put host name and client Id to the DHCP packet options.
    //
    _PutOptString(&pOptions, DHCP_OPTION_NAME,   _sHost);
    _PutOpt(&pOptions, DHCP_OPTION_CLIENT, _pClientId, _ClientIdLen);
    //
    // Append the options that we want to request.
    //
    State = _aState[IFIndex].state;
    //
    // Add client IP address
    //
    if ((State == DHCPC_STATE_SELECTING) || (State == DHCPC_STATE_CHECK_IP) || (State == DHCPC_STATE_REQUESTING) || (State == DHCPC_STATE_REBOOTING) || (State == DHCPC_STATE_INITREBOOT)) {
      _PutOptU32(&pOptions, DHCP_OPTION_CADDR, ntohl(_aState[IFIndex].IpAddr));
    }
    //
    // Add client subnet mask.
    //
    if (_aState[IFIndex].snmask) {
      _PutOptU32(&pOptions, DHCP_OPTION_SNMASK, ntohl(_aState[IFIndex].snmask));
    }
    //
    // Add clients default gateway address.
    //
    if (_aState[IFIndex].defgw) {
      _PutOptU32(&pOptions, DHCP_OPTION_ROUTER, ntohl(_aState[IFIndex].defgw));
    }
#if defined(DHCP_MAXDNSRVS) && (DHCP_MAXDNSRVS > 0)
  {
    int   i;
    U8 * pOptLen;
    IP_ADDR DNSSrv;

    pOptLen = NULL;
    for (i = 0; i < DHCP_MAXDNSRVS; i++) {
      if (_aState[IFIndex].dnsrv[i]) {
        if (i == 0) {
          *pOptions++ = DHCP_OPTION_DNSRV;
          pOptLen  = pOptions;
          *pOptions++ = 0;
        }
        //
        // Copy the IP address in network byte order.
        //
        DNSSrv    = _aState[IFIndex].dnsrv[i];
        IP_MEMCPY((char *)pOptions, (char *)&DNSSrv, 4);
        pOptions += 4;
        *pOptLen += 4;
      }
    }
  }
#endif
    //
    // Add lease time.
    //
    if (_aState[IFIndex].lease) {
      _PutOptU32(&pOptions, DHCP_OPTION_LEASE, _aState[IFIndex].lease);
    }
#ifdef DHCP_REQLIST
    //
    // If there is a list of options to be requested from server, include it
    //
    if ( reqlist_len > 0 ) {
      int   i;

      *pOptions++ = DHCP_OPTION_REQLIST ;
      *pOptions++ = (U8)reqlist_len ;
      for (i=0 ; i < reqlist_len ; i++ ) {
        *pOptions++ = reqlist[i];
      }
    }
#endif
    //
    // RFC 2131, section 4.3.5 ref to server-ip is option 54 0x36, not
    // siaddr: p. 16 number 3: "The client broadcasts a DHCPREQUEST
    // message that MUST include the 'server identifier' option to
    // indicate which server it has selected. . . ." p. 31 bullet 1:
    // "Client inserts the address of the selected server in 'server
    // identifier'. . . ." RFC 951, p. 4 definition of 'siaddr' is
    // "server IP address; returned in bootreply by server."
    //

    //
    // Add the server identifier option if a server should be selected.
    //
    if ((_aState[IFIndex].state == DHCPC_STATE_SELECTING) || (_aState[IFIndex].state == DHCPC_STATE_REQUESTING)) {
      _PutOptU32(&pOptions, DHCP_OPTION_SERVER, ntohl(_aState[IFIndex].srv_ipaddr));
    }
    _PutOptString(&pOptions, 81, _sDomain);  // Add domain name
    _PutOptString(&pOptions, 60, _sVendor);  // Add vendor class identifier
  }
  //
  // Add the end of options marker.
  //
  *pOptions++ = DHCP_OPTION_END;
  //
  // Add the used client IP address when renewing or rebinding
  //
  if ((_aState[IFIndex].state == DHCPC_STATE_RENEWING) || (_aState[IFIndex].state == DHCPC_STATE_REBINDING)) {
    pOutBOOTP->ciaddr = IP_aIFace[IFIndex].n_ipaddr;
  }
  //
  // Select if request has to be send via broadcast or unicast.
  //
  FHost = 0xFFFFFFFF;
  if (_aState[IFIndex].state == DHCPC_STATE_RENEWING) {
    FHost = _aState[IFIndex].srv_ipaddr;
  }
  //
  // Compute the packet length and send the request.
  //
  pPacket->NumBytes = (char *)pOptions - (char *)pOutBOOTP;
  IP_UDP_SendAndFree(IFIndex, FHost, BOOTP_SERVER_PORT, BOOTP_CLIENT_PORT, pPacket);
  IP_STAT_INC(IP_DHCPC_Stat.requests);
  _aState[IFIndex].last_tick = IP_OS_GetTime32();
  _aState[IFIndex].tries++;
  return 0;
}

/* FUNCTION: _SendRequest()
 *
 * _SendRequest() - called when
 * 1. we have received a DHCP offer (via dhc_rx_offer() ).
 * 2. we have to renew a lease with a DHCP server. ( via _SendReclaim() ).
 * 3. we have to rebind to a different DHCP server.
 * 4. we are in INIT-REBOOT state (via _SendReclaim() ).
 * This should format & send a request. Values are picked from
 * _aState[]. It handles the specific nuances of a REQUEST
 * depending on the state of DHCP Client. It implements the following
 * table of RFC2131 (sec 4.3.6).
 * ---------------------------------------------------------------------
 * |              |INIT-REBOOT |SELECTING |RENEWING   |REBINDING |
 * ---------------------------------------------------------------------
 * |broad/unicast |broadcast   |broadcast |unicast    |broadcast |
 * |server-ip     |MUST NOT    |MUST      |MUST NOT   |MUST NOT  |
 * |requested-ip  |MUST        |MUST      |MUST NOT   |MUST NOT  |
 * |ciaddr        |zero        |zero      |IP address |IP address|
 * ---------------------------------------------------------------------
 * In a DISCOVER->OFFER->REQUEST scenario, we need to use the same
 * XID, SECS field as in OFFER packet. This is controlled via
 * XIDFlag. If XIDFlag is TRUE, then use a new XID. Else use the
 * old one.
 *
 * PARAM1: int IFIndex
 * PARAM2: int XIDFlag
 *
 * RETURNS:  Returns 0 if ok, else non-zero IP_ERR_ error.
 */
static int _SendRequest(int IFIndex, int XIDFlag) {
  IP_LOG((IP_MTYPE_DHCP, "%s: Sending Request.",  _aState[IFIndex].sService));
  return _Send(IFIndex, XIDFlag, DHCP_REQUEST);
}


/*********************************************************************
*
*       _SendDecline
*/
static int _SendDecline(int IFIndex, int XIDFlag) {
  IP_LOG((IP_MTYPE_DHCP, "%s: Sending Decline.", _aState[IFIndex].sService));
  return _Send(IFIndex, XIDFlag, DHCP_DECLINE);
}

/*********************************************************************
*
*       _OnRx
*
*  Function descrition
*    DHCP client UDP callback. Called from stack whenever we get a
*    bootp (or DHCP) reply.
*
*  Return value
*    IP_RX_ERROR - If packet is invalid for some reason
*    IP_OK       - If packet is valid
*/
static int _OnRx(IP_PACKET * pPacket, void * pContext) {
  BOOTP *     pBOOTP;
  DHC_STATE * pState;
  U8 *        pOptions;
  int         NumBytes;
  int         DHCPPacketType;
  int         e;
  int         IFIndex;
  int         State;
  U32         ServerIP;

  (void)pContext;   // Avoid "parameter not used" warning.
  NumBytes       = pPacket->NumBytes;
  DHCPPacketType = 0;
  IFIndex        = 0;
  pState         = &_aState[IFIndex];
  pBOOTP         = (BOOTP *)pPacket->pData;
  ServerIP       = pBOOTP->siaddr;
#ifdef IP_FILTER    // For tests with multiple DHCP servers
  if (ServerIP != IP_FILTER) {
    return IP_RX_ERROR;
  }
#endif
  IP_LOG((IP_MTYPE_DHCP_EXT, "%s: Received packet from %i", _aState[IFIndex].sService, ServerIP));
  ServerIP = _GetServerIP(&pBOOTP->options[4]);  // RS: This should not be necessary, since the server addr. we have from the fixed field should be o.k.
  if ((NumBytes < (int)(sizeof(struct bootp) - BOOTP_OPTSIZE)) || (pBOOTP->op != BOOTP_REPLY) || (*(U32*)(&pBOOTP->options) != RFC1084_MAGIC_COOKIE)) {
    IP_WARN((IP_MTYPE_DHCP, "%s: Invalid packet. Ignored.", _aState[IFIndex].sService));
    IP_STAT_INC(IP_DHCPC_Stat.errors);
    return IP_RX_ERROR;
  }
  //
  // Refuse DHCP offers or replys which are not for us
  //
  if(IP_MEMCMP(pBOOTP->chaddr, pPacket->pNet->abHWAddr, pPacket->pNet->n_hal)) {
    IP_LOG((IP_MTYPE_DHCP_EXT, "%s: Not for us (wrong MAC Addr %h). Ignored.", _aState[IFIndex].sService, pBOOTP->chaddr));
    return IP_RX_ERROR;    /* not an error, just ignore it */
  }
  //
  // Search the DHCP type option to check if it is a full DHCP or a plain BOOTP packet.
  //
  pOptions = _FindOption(DHCP_OPTION_TYPE, &pBOOTP->options[4]);
  if ((pOptions == NULL) && (_aState[IFIndex].state != DHCPC_STATE_BOUND)) {
    IP_LOG((IP_MTYPE_DHCP, "%s: BOOTP reply received.", _aState[IFIndex].sService));
    IP_STAT_INC(IP_DHCPC_Stat.bpreplys);
    _ExtractOpts(IFIndex, &pBOOTP->options[4]);
    _aState[IFIndex].IpAddr = pBOOTP->yiaddr;
    _SetIP(IFIndex);
    //
    // Set values so that DHCP state machine works as expected
    //
    _SetState(IFIndex, DHCPC_STATE_BOUND);
    _aState[IFIndex].t1 = DHCPC_INFINITY ;
    return IP_OK;
  }
  //
  // It is a full DHCP packet.
  // There are 8 types according to RFC 1533.
  // We filter out the ones that we do not react to.
  //
  DHCPPacketType = *(pOptions+2);
  switch (DHCPPacketType) {
  case DHCP_DISCOVER:
  case DHCP_REQUEST:
  case DHCP_DECLINE:
  case DHCP_RELEASE:
    IP_LOG((IP_MTYPE_DHCP, "%s: Received packet of wrong type %d. Ignored.", _aState[IFIndex].sService, DHCPPacketType));
    IP_STAT_INC(IP_DHCPC_Stat.errors);     /* these should only be upcalled to a server */
    return IP_RX_ERROR;
  case DHCP_ACK:
    IP_LOG((IP_MTYPE_DHCP_EXT, "%s: Packet type is ACK.", _aState[IFIndex].sService));
    break;
  case DHCP_NAK:
    IP_LOG((IP_MTYPE_DHCP_EXT, "%s: Packet type is NACK.", _aState[IFIndex].sService));
    break;
  case DHCP_OFFER:
    IP_LOG((IP_MTYPE_DHCP_EXT, "%s: Packet type is OFFER.", _aState[IFIndex].sService));
    break;
  default:
    ;
  }
  State = pState->state;
  switch (State) {
  case DHCPC_STATE_INIT:
  case DHCPC_STATE_INITREBOOT:
  case DHCPC_STATE_BOUND:
    IP_STAT_INC(IP_DHCPC_Stat.errors);     /* these should only be upcalled to a server */
    IP_LOG((IP_MTYPE_DHCP_EXT, "%s: IFace already configured. Ignored.", _aState[IFIndex].sService));
    return IP_RX_ERROR;
  case DHCPC_STATE_SELECTING:
    //
    // We respond to the first offer packet that we receive
    //
    if (DHCPPacketType == DHCP_OFFER ) {
      IP_STAT_INC(IP_DHCPC_Stat.offers);
      _aState[IFIndex].srv_ipaddr  = ServerIP;
      if (_aState[IFIndex].srv_ipaddr == 0) {
        IP_WARN((IP_MTYPE_DHCP, "%s: Did not receive server IP-addr. Ignored.", _aState[IFIndex].sService));
        IP_STAT_INC(IP_DHCPC_Stat.errors);
        _aState[IFIndex].srv_ipaddr = pPacket->fhost;   /* Try using fhost */
      }
      if (pBOOTP->hops) {
        //
        // If the number of hops is not 0. The packet has been received via DHCP Relay Agent.
        // We store the IP address of DHCP Relay Agent, so that packets from other DHCP Relay Agents can be discarded.
        //
        _aState[IFIndex].rly_ipaddr = pPacket->fhost;
      } else {
        _aState[IFIndex].rly_ipaddr = 0;
      }
      //
      // In a DISCOVER->OFFER->REQUEST scenario, the XID must be the same.
      //
      if (_aState[IFIndex].xid != pBOOTP->xid) {
        goto Error;
      }
      //
      // Examine the options after the DHCP magic cookie
      //
      pOptions = &pBOOTP->options[4];
      e = _ExtractOpts(IFIndex,pOptions);
      if (e) {
        goto Error;
      }
      //
      // The packet must contain an IP address for the client.
      //
      if (!pBOOTP->yiaddr){
        _SendDeclinePacket(IFIndex, pBOOTP, pPacket->NumBytes);
        goto Error;
      }
      _aState[IFIndex].IpAddr = pBOOTP->yiaddr;
      //
      // Check if the offered IP address is vaild.
      // If the callback function is set, the user application can decide if the address is valid.
      //
      if (_aState[IFIndex].pfCheck) {
        DHCPC_CHECK_PARAS DHCPCParas;

        DHCPCParas.IpAddr  = ntohl(pState->IpAddr);
        DHCPCParas.NetMask = ntohl(pState->snmask);
        e = _aState[IFIndex].pfCheck(&DHCPCParas);
        if (e) {
          _SendDeclinePacket(IFIndex, pBOOTP, pPacket->NumBytes);
          _SetState(IFIndex, DHCPC_STATE_WAIT_INIT);
          IP_LOG((IP_MTYPE_DHCP, "%s: IFace %d: Offer declined. IP: %i, Mask: %i, GW: %i", _aState[IFIndex].sService, IFIndex, pState->IpAddr, pState->snmask, pState->defgw));
          IP_LOG((IP_MTYPE_DHCP, "%s: 10 seconds idle to avoid looping.", _aState[IFIndex].sService));
          IP_STAT_INC(IP_DHCPC_Stat.errors);
          return IP_RX_ERROR;
        }
      }
#if CHECK_IP_BEFORE_BOUND
      _SendARPRequest(IFIndex);
      _SetState(IFIndex, DHCPC_STATE_CHECK_IP);
      IP_LOG((IP_MTYPE_DHCP, "%s: IFace %d: Offer: IP: %i, Mask: %i, GW: %i.", _aState[IFIndex].sService, IFIndex, pState->IpAddr, pState->snmask, pState->defgw));
#else
      //
      // Offer is OK. Send a DHCP request.
      //
      _SendRequest(IFIndex, 0);
      _SetState(IFIndex, DHCPC_STATE_REQUESTING);
#endif
    } else {
      //
      // We can not receive any other packet in this state.
      // Report an error and remain in SELECTING state, so that
      // an OFFER packet from another DHCP server can be
      // accepted.
      //
      IP_STAT_INC(IP_DHCPC_Stat.errors);
      if (DHCPPacketType == DHCP_NAK) {
        IP_STAT_INC(IP_DHCPC_Stat.naks);
        IP_WARN((IP_MTYPE_DHCP, "%s: NACK received.", _aState[IFIndex].sService));
      }
      return IP_RX_ERROR;
    }
    break;
#if CHECK_IP_BEFORE_BOUND
  case DHCPC_STATE_CHECK_IP:              // Ignore further packets in this state
    return IP_RX_ERROR;
#endif
  case DHCPC_STATE_REQUESTING:
  case DHCPC_STATE_REBINDING:
  case DHCPC_STATE_RENEWING:
    //
    // Check if the ACK/NAK is from the same server which sent the offer packet.
    //
    if ( _aState[IFIndex].srv_ipaddr != ServerIP ) {
      IP_STAT_INC(IP_DHCPC_Stat.errors);
      return IP_RX_ERROR;
    }
    if (_aState[IFIndex].rly_ipaddr && (_aState[IFIndex].rly_ipaddr != pPacket->fhost)) {
      IP_STAT_INC(IP_DHCPC_Stat.rlyerrs);
      IP_STAT_INC(IP_DHCPC_Stat.errors);
      return IP_RX_ERROR;
    }
    //
    // The rest of the requesting, rebinding and renewing state is identical to the
    // rebooting state, therefore we do not add a break here...
    //
  case DHCPC_STATE_REBOOTING:
    if ( DHCPPacketType == DHCP_ACK ) {
      IP_STAT_INC(IP_DHCPC_Stat.acks);
      _ExtractOpts(IFIndex,&pBOOTP->options[4]);
      if (_aState[IFIndex].lease == DHCPC_INFINITY ) {
        _aState[IFIndex].t1 = DHCPC_INFINITY;
        _aState[IFIndex].t2 = DHCPC_INFINITY;
      } else {
        _aState[IFIndex].t1 = _aState[IFIndex].lease / 2;
        _aState[IFIndex].t2 = (_aState[IFIndex].lease / 8) * 7;
      }
      _aState[IFIndex].lease_start = IP_OS_GetTime32();
      _aState[IFIndex].srv_ipaddr  = ServerIP;
      if (_aState[IFIndex].srv_ipaddr == 0) {
        IP_WARN((IP_MTYPE_DHCP, "%s: Did not receive server IP-addr. Ignored.", _aState[IFIndex].sService));
        IP_STAT_INC(IP_DHCPC_Stat.errors);
        _aState[IFIndex].srv_ipaddr = pPacket->fhost;   /* Try using fhost */
      }
      if (pBOOTP->hops) {
        //
        // If the number of hops is not 0. The packet has been received via DHCP Relay Agent.
        // We store the IP address of DHCP Relay Agent, so that packets from other DHCP Relay Agents can be discarded.
        //
        _aState[IFIndex].rly_ipaddr = pPacket->fhost;
      } else {
        _aState[IFIndex].rly_ipaddr = 0;
      }
      _SetIP(IFIndex);
      _SetState(IFIndex, DHCPC_STATE_BOUND);
    } else if ( DHCPPacketType == DHCP_NAK ) {
      //
      // DHCP server does not accept the request.
      // We go back to the init state.
      //
      _SetState(IFIndex,DHCPC_STATE_INIT);
      IP_STAT_INC(IP_DHCPC_Stat.naks);
    } else  {
      // Valid packet types:
      //  - In renewing, rebinding, or rebooting state: ACK or NAK
      //  - In requesting state: ACK, NAK, or OFFER.
      //    It is possible that we receive a retransmitted offer
      //    in the requesting state. We dicard the OFFER, but since it is not an error, so we count it for the stats.
      //
      if ((_aState[IFIndex].state == DHCPC_STATE_REQUESTING) && (DHCPPacketType == DHCP_OFFER)) {
        IP_STAT_INC(IP_DHCPC_Stat.offers);
        return IP_RX_ERROR;
      }
      IP_STAT_INC(IP_DHCPC_Stat.errors);
      return IP_RX_ERROR;
    }
    break;
  case DHCPC_STATE_WAIT_INIT:
    break;
  default:
     IP_WARN((IP_MTYPE_DHCP, "%s: Bad state.", _aState[IFIndex].sService));
     _SetState(IFIndex,DHCPC_STATE_INIT);
     IP_STAT_INC(IP_DHCPC_Stat.errors);
     return IP_RX_ERROR;
  }
  return IP_OK;
Error:
  IP_STAT_INC(IP_DHCPC_Stat.errors);
  _SetState(IFIndex,DHCPC_STATE_INIT);
  IP_WARN((IP_MTYPE_DHCP, "%s: Error in packet.", _aState[IFIndex].sService));
  return IP_RX_ERROR;
}

/*********************************************************************
*
*       _SendDiscover
*
*  Function descrition
*    Sends a DHCP discovery packet.
*
*  Return value
*    IP_ERR_NOMEM - If packet could not be allocated.
*    IP_OK        - If packet is valid.
*/
static int _SendDiscover(int IFIndex) {
  BOOTP *     pOutBOOTP;
  IP_PACKET * pPacket;
  U8 *        pOptions;
  I32         LeaseTime;

  //
  // Alloc packet
  //
  pPacket = IP_UDP_Alloc(sizeof(struct bootp));
  if (!pPacket) {
    IP_WARN((IP_MTYPE_DHCP, "%s: Can not send discover!", _aState[IFIndex].sService));
    return IP_ERR_NOMEM;
  }
  //
  // Initialize variables to start a new DHCP transaction.
  // XID needs to be unique for each DHCP transaction.
  //
  _aState[IFIndex].xid  = _xids++;
  _aState[IFIndex].secs = (U16)_sysuptime;
  //
  // Build the BOOTP header
  //
  pOutBOOTP = (BOOTP *)pPacket->pData;     /* overlay bootp struct on buffer */
  _BuildHeader(IFIndex,pOutBOOTP);
  //
  // Add boot vendor specific information. The 'vend' field can be filled in by
  // the client with vendor-specific strings or structures. RFC951 says that the operation
  // of the BOOTP server should not DEPEND on this information existing. To advance the
  // compatiblity with not fully RFC-compliant implemented servers, we add always the
  // magic number as described in RFC1048.
  //
  *(U32*)(&pOutBOOTP->options) = RFC1084_MAGIC_COOKIE;
  pOptions    = &pOutBOOTP->options[4];
  if (_aState[IFIndex].UseDHCP & 0x00FF) {
    //
    // Change the BOOTP packet to a DHCP Discover packet.
    //
    *pOptions++ = DHCP_OPTION_TYPE;   // Set DHCP type option.
    *pOptions++ = 1;                  // Length of the type option field.
    *pOptions++ = DHCP_DISCOVER;      // Set the type of the packet.
    //
    // Put host name and client Id to the DHCP packet options.
    //
    _PutOptString(&pOptions, DHCP_OPTION_NAME,   _sHost);
    _PutOpt(&pOptions, DHCP_OPTION_CLIENT, _pClientId, _ClientIdLen);
    _PutOptString(&pOptions, 81, _sDomain);  // Add domain name
    _PutOptString(&pOptions, 60, _sVendor);  // Add vendor class identifier

#if 1
    //
    // Ask for an infinite lease
    //
    LeaseTime = -1L ;
    _PutOptU32(&pOptions, DHCP_OPTION_LEASE, LeaseTime);
#endif
    //
    // If we already have an IP address, try to get it from the server
    //
    if (IP_aIFace[IFIndex].n_ipaddr != 0) {
      IP_ADDR IpAddr = htonl(IP_aIFace[IFIndex].n_ipaddr);
      _PutOptU32(&pOptions, DHCP_OPTION_CADDR, IpAddr);
    }
#ifdef DHCP_REQLIST
    //
    // If there is a list of options to be requested from server, include it
    //
    if ( reqlist_len > 0 ) {
      int   i;

      *pOptions++ = DHCP_OPTION_REQLIST ;
      *pOptions++ = (U8)reqlist_len ;
      for (i=0 ; i < reqlist_len ; i++ ) {
        *pOptions++ = reqlist[i];
      }
    }
#endif
  }
  //
  // Add the end of options marker.
  //
  *pOptions++ = DHCP_OPTION_END;
  //
  // Save the current system time to prevend the DHCP client timer
  // to prevent unnecessary retries.
  //
  _aState[IFIndex].last_tick = IP_OS_GetTime32();
  //
  // The DHCP state needs to be changed before sending to avoid a race condition with the expected reply.
  //
  if (_aState[IFIndex].state != DHCPC_STATE_SELECTING) {
   _SetState(IFIndex, DHCPC_STATE_SELECTING);
  }
  IP_LOG((IP_MTYPE_DHCP, "%s: Sending discover!", _aState[IFIndex].sService));
  IP_UDP_SendAndFree(IFIndex, 0xFFFFFFFF, BOOTP_SERVER_PORT, BOOTP_CLIENT_PORT, pPacket);
  IP_STAT_INC(IP_DHCPC_Stat.discovers);
  _aState[IFIndex].last_tick = IP_OS_GetTime32();
  _aState[IFIndex].tries++;
  return 0;
}

/*********************************************************************
*
*       _SendReclaim
*
*  Function descrition
*    Sends a DHCP request packet.
*    Called in the init reboot and the bound state.
*    Init reboot scenario: A target with an IP address which has been
*    received via DHCP and stored in non volatile memory, can try to
*    reclaim the IP address.
*    Bound scenario: Target has an IP address, but the lease has been
*    expired.
*
*  Return value
*    IP_ERR_NOMEM - If packet could not be allocated.
*    IP_OK        - If packet is valid.
*/
static int _SendReclaim(int IFIndex) {
  if (IP_aIFace[IFIndex].n_ipaddr == 0L) {
    IP_WARN((IP_MTYPE_DHCP, "%s: IP addr. not set!", _aState[IFIndex].sService));
    return IP_ERR_LOGIC;
  }
  _aState[IFIndex].IpAddr = IP_aIFace[IFIndex].n_ipaddr;
  _aState[IFIndex].snmask = IP_aIFace[IFIndex].snmask;
  _aState[IFIndex].defgw  = IP_aIFace[IFIndex].n_defgw;
  return _SendRequest(IFIndex, 1);
}

/*********************************************************************
*
*       _ResetIP()
*
*  Function description
*    Resets the IP address, gateway and network mask for the interface.
*    It is called when we didn't receive a ACK/NAK before we go to the
*    restarting state.
*/
static void _ResetIP(int IFace) {
  NET * pIFace;

  pIFace = &IP_aIFace[IFace];
  pIFace->n_ipaddr  = 0;
  pIFace->snmask  = 0;
  pIFace->n_defgw = 0;
}

/*********************************************************************
*
*       _DHCPC_Timer
*
*  Function description
*    DHCP client timer. Called every second for retries, lease renewal, etc.
*/
static void _DHCPC_Timer(void) {
  DHC_STATE * pState;
  int         IFace;
  int         NumTries;
  U32         half_time;
  I32         t;
  int         e;

  _sysuptime++;
  for (IFace = 0; IFace < IP_MAX_IFACES; IFace++) {
    if (IP_GetCurrentLinkSpeed() == 0) { // ToDo: Replace with a function that checks the current link speed of the selected interface.
      return; // No link yet, we have to wait for it
    }
    pState = &_aState[IFace];
    switch (pState->state) {
    case DHCPC_STATE_INIT:
      if (_SendDiscover(IFace)) {     // Error while sending a discover packet ?
        return;
      }
      _SetState(IFace, DHCPC_STATE_SELECTING);
      break;
    case DHCPC_STATE_INITREBOOT:
      e = _SendReclaim(IFace);
      if (e) {
        IP_WARN((IP_MTYPE_DHCP, "%s: Could not send packet.", _aState[IFace].sService));
        return;
       }
       _SetState(IFace, DHCPC_STATE_REBOOTING);
       break;
    case DHCPC_STATE_SELECTING:
    case DHCPC_STATE_REBOOTING:
    case DHCPC_STATE_REQUESTING:
      NumTries = pState->tries;
      //
      // Check if timed out while waiting for an offer, ACK, NAK.
      //
      if ((unsigned)NumTries >= _MaxTries) {
        NumTries = _MaxTries;
      }
      t = _Timeout;
      if (_ExpTimeout) {
        t <<= NumTries;
      }
      t += pState->last_tick;
      t -= IP_OS_GetTime32();
      if (t < 0)  {
        //
        // If timed out -> retransmit
        //
        switch(pState->state) {
        case DHCPC_STATE_SELECTING:
          _SendDiscover(IFace);
          break;
        case DHCPC_STATE_REQUESTING:
          _SendRequest(IFace, 0);
          break;
        case DHCPC_STATE_REBOOTING:
          _SendReclaim(IFace);
          break;
        default:
          IP_WARN((IP_MTYPE_DHCP, "%s: illegal state", _aState[IFace].sService));
          break;
        }
      }
      if (((unsigned)NumTries == _MaxTries) && (pState->state !=DHCPC_STATE_SELECTING)) {
        //
        // Restart at init state.
        //
        _SetState(IFace,DHCPC_STATE_RESTARTING);
        _ResetIP(IFace);
        _SetState(IFace,DHCPC_STATE_INIT);
      }
      break;
    case DHCPC_STATE_REBINDING:
      //
      // Check if we need to retransmit the request.
      //
      if ((pState->lease * 1000 + pState->lease_start) > IP_OS_GetTime32())  {
        //
        // If half the time between last transmit and the lease time
        // has been elapsed, we need a retransmit.
        //
        half_time = (pState->lease_start + pState->lease * 1000 - pState->last_tick) / 2;
        //
        // The minimum retransmission interval is 60 seconds.
        //
        if (half_time < 60000uL) {
          half_time = 60000uL;
        }
        if ( pState->last_tick + half_time < IP_OS_GetTime32()) {
          _SendRequest(IFace, 0);
        }
      } else {
        //
        // We did not receive a ACK/NAK and the lease has expired. -> Restart...
        //
        _SetState(IFace,DHCPC_STATE_RESTARTING);
        _ResetIP(IFace);
        _SetState(IFace,DHCPC_STATE_INIT);
      }
      break;
    case DHCPC_STATE_BOUND:
      //
      // Test for lease expiry. The RENEW timer.
      //
      if (pState->t1 != DHCPC_INFINITY) {
        I32 tExp;

        tExp = pState->lease_start + pState->t1 * 1000;
        if (IP_IsExpired(tExp)) {
          //
          // Time to renew the IP address.
          //
          _SetState(IFace,DHCPC_STATE_RENEWING);
          e = _SendReclaim(IFace);
          if (e) {
            IP_WARN((IP_MTYPE_DHCP, "%s: Could not send packet.", _aState[IFace].sService));
            return;
          }
          IP_STAT_INC(IP_DHCPC_Stat.renew);
        }
      }
      break;
    case DHCPC_STATE_RENEWING:
      {
        I32 tExp;
        tExp = pState->lease_start + pState->t2 * 1000;
        /* Test for lease expiry. The REBIND timer. */
        if (IP_IsExpired(tExp)) {
          /* See if we need to retransmit. If we have waiting for
           * half the time between last transmit and t2, then we
           * need to retransmit. Also the minimum retransmit
           * interval is 60 secs.
           */
          half_time = (tExp - pState->last_tick) / 2;

          //
          // The minimum retransmission interval is 60 seconds.
          //
          if ( half_time < 60000uL) {
            half_time = 60000uL;
          }
          if (IP_IsExpired(pState->last_tick + half_time)) {
            _SendRequest(IFace, 0);
          }
        } else {
          /* No Response has come from the Server that assigned our
           * IP. Hence send a broadcast packet to see if we can
           * lease this IP from some other server
           */
          _SetState(IFace, DHCPC_STATE_REBINDING);
          e = _SendRequest(IFace, 1);
          if (e) {
            IP_WARN((IP_MTYPE_DHCP, "%s: Could not send packet.", _aState[IFace].sService));
            return;
          }
          IP_STAT_INC(IP_DHCPC_Stat.rebind);
        }
      }
      break;
    case DHCPC_STATE_CHECK_IP:
      t  = pState->last_tick + 500;   // Give other systems 500 ms reaction time, which should be more than enough
      t -= IP_OS_GetTime32();
      if (t < 0)  {
        t = IP_ARP_HasEntry(pState->IpAddr);
        if (t == 0) {
          IP_LOG((IP_MTYPE_DHCP, "%s: IP addr. checked, no conflicts", _aState[IFace].sService));
          _SendRequest(IFace, 0);
          _SetState(IFace, DHCPC_STATE_REQUESTING);
        } else {
          IP_LOG((IP_MTYPE_DHCP, "%s: IP addr. declined: Already in use.", _aState[IFace].sService));
          _SendDecline(IFace, 0);
          _SetState(IFace, DHCPC_STATE_WAIT_INIT);
          IP_LOG((IP_MTYPE_DHCP, "%s: 10 seconds idle to avoid looping.", _aState[IFace].sService));
        }
      }
      break;
    case DHCPC_STATE_WAIT_INIT:
      t  = pState->last_tick + 10000;   // RFC2131, 3.1.5 says: The client SHOULD wait a minimum of ten seconds before restarting the configuration process to avoid excessive network traffic in case of looping.
      t -= IP_OS_GetTime32();
      if (t < 0)  {
        IP_LOG((IP_MTYPE_DHCP, "%s: Looping-avoindance-delay expired, back to init state", _aState[IFace].sService));
        _SetState(IFace, DHCPC_STATE_INIT);
      }
      break;
    case DHCPC_STATE_UNUSED:
    default:
       continue;
    }
  }
}

/*********************************************************************
*
*       _Activate
*
*  Function description
*    Activates the DHCP client for the specified interface.
*
*  Parameters:
*    IFIndex    Zero based interface index. 0 for first, 1 for second interface.
*    sHost      Pointer to host name to use in negotiation. May be NULL.
*    sDomain    Pointer to domain name to use in negotiation. May be NULL.
*    sVendor    Pointer to vendor to use in negotiation. May be NULL.
*
*  Notes
*    (1) Names
*        All names are optional (can be NULL).
*        If not NULL, must point to a memory area which remains valid after the call since the string is not copied.
*        Examples:
*          Good:
*             IP_DHCPC_Activate(0, "Target", NULL, NULL);
*          Bad:
*             char ac;
*             sprintf(ac, "Target%d, Index);
*             IP_DHCPC_Activate(0, ac, NULL, NULL);
*          Good:
*             static char ac;
*             sprintf(ac, "Target%d, Index);
*             IP_DHCPC_Activate(0, ac, NULL, NULL);
*/
static void _Activate(int IFIndex, const char *sHost, const char *sDomain, const char *sVendor) {
  int State;

  _sHost   = sHost;
  _sDomain = sDomain;
  _sVendor = sVendor;
  if (IP_aIFace[IFIndex].n_ipaddr) {
    State = DHCPC_STATE_INITREBOOT;
  } else {
    State = DHCPC_STATE_INIT;
  }
  _SetState(IFIndex, State);
  if (_aState[IFIndex].UseDHCP == 0) {
    //
    // Open UDP connection to receive incoming DHCP replys
    //
    _hConnection = IP_UDP_Open(0L /* any foreign host */,  BOOTP_SERVER_PORT, BOOTP_CLIENT_PORT,  _OnRx, NULL);
    if (!_hConnection) {
      IP_PANIC("Can not open UDP connection for DHCP");
    }
    //_aState[IFIndex].state = DHCPC_STATE_INIT;  // ??
    IP_AddTimer(_DHCPC_Timer, 1000);
  }
}

/*********************************************************************
*
*       Public code
*
**********************************************************************
*/

/*********************************************************************
*
*       IP_DHCPC_Activate
*
*  Function description
*    Activates the DHCP client for the specified interface.
*
*  Parameters:
*    IFIndex    Zero based interface index. 0 for first, 1 for second interface.
*    sHost      Pointer to host name to use in negotiation. May be NULL.
*    sDomain    Pointer to domain name to use in negotiation. May be NULL.
*    sVendor    Pointer to vendor to use in negotiation. May be NULL.
*
*  Notes
*    (1) Names
*        All names are optional (can be NULL).
*        If not NULL, must point to a memory area which remains valid after the call since the string is not copied.
*        Examples:
*          Good:
*             IP_DHCPC_Activate(0, "Target", NULL, NULL);
*          Bad:
*             char ac;
*             sprintf(ac, "Target%d, Index);
*             IP_DHCPC_Activate(0, ac, NULL, NULL);
*          Good:
*             static char ac;
*             sprintf(ac, "Target%d, Index);
*             IP_DHCPC_Activate(0, ac, NULL, NULL);
*/
void IP_DHCPC_Activate(int IFIndex, const char * sHost, const char * sDomain, const char * sVendor) {
#if (IP_DEBUG >= 1)
  strcpy(_aState[IFIndex].sService, "DHCPc");
#endif
  _Activate(IFIndex, sHost, sDomain, sVendor);
  _aState[IFIndex].UseDHCP  = USE_DHCP;
}

/*********************************************************************
*
*       IP_BOOTPC_Activate
*
*  Function description
*    Activates the BOOTP client for the specified interface.
*/
void IP_BOOTPC_Activate(int IFIndex) {
#if (IP_DEBUG >= 1)
  strcpy(_aState[IFIndex].sService, "BOOTP");
#endif
  _Activate(IFIndex, NULL, NULL, NULL);
  _aState[IFIndex].UseDHCP  = USE_BOOTP;
}

/*********************************************************************
*
*       IP_DHCPC_SetClientId
*
*  Function description
*    Sets the DHCP client id for the specified interface.
*    Should be called prior to IP_DHCPC_Activate()
*
*  Parameters:
*    IFIndex      Zero based interface index. 0 for first, 1 for second interface.
*    pClientId    Pointer to ClientId to use in negotiation. May be NULL.
*    ClientIdLen  Size of client Id in bytes
*
*  Notes
*    (1) DHCP option
*        Client Id is transmitted using option 61. See. RFC1533, 9.12
*    (2) Names
*        If not NULL, must point to a memory area which remains valid after the call since the string is not copied.
*        Examples:
*          Good:
*             IP_DHCPC_Activate(0, "Target", NULL, NULL);
*          Bad:
*             char ac;
*             sprintf(ac, "Target%d, Index);
*             IP_DHCPC_Activate(0, ac, NULL, NULL);
*          Good:
*             static char ac;
*             sprintf(ac, "Target%d, Index);
*             IP_DHCPC_Activate(0, ac, NULL, NULL);
*/
void IP_DHCPC_SetClientId(int IFIndex, const U8 * pClientId, unsigned ClientIdLen) {
  IP_USE_PARA(IFIndex);
  _pClientId   = pClientId;
  _ClientIdLen = ClientIdLen;
}

/*********************************************************************
*
*       IP_DHCPC_Halt
*
*  Function description
*    Stop DHCP client activity for the passed interface.
*/
void IP_DHCPC_Halt(int IFace) {
  int (*pfCallback)(int, int);
  int (*pfCheck)(DHCPC_CHECK_PARAS *);

  pfCallback = NULL;
  pfCheck    = NULL;
  if ((unsigned)IFace > IP_MAX_IFACES) {
    IP_PANIC("Illegal interface.");
  }
  IP_UDP_Close(_hConnection);
  //
  // Keep the callback function for the interface.
  //
  if (_aState[IFace].callback) {
    pfCallback = _aState[IFace].callback;
  }
  //
  // Keep the callback function for the interface.
  //
  if (_aState[IFace].pfCheck) {
    pfCheck = _aState[IFace].pfCheck;
  }
  //
  // Clear the _aState entry
  //
  IP_MEMSET(&_aState[IFace], 0, sizeof(struct DHC_STATE));
  //
  // Initialize the DHCP state entry with valid values.
  //
  _aState[IFace].callback = pfCallback;
  _aState[IFace].pfCheck  = pfCheck;
  _aState[IFace].state    = DHCPC_STATE_UNUSED;
}

/*********************************************************************
*
*       IP_DHCPC_SetCallback
*
*  Function description
*    This function allows the caller to set a callback for an interface.
*    The callback is called with every status change.
*    This mechanism is provided so that the caller can do some processing
*    when the interface is up (like doing initializations or blinking
*    LEDs, etc.).
*/
void IP_DHCPC_SetCallback(int IFIndex, int (*routine)(int,int) ) {
  _aState[IFIndex].callback = routine;
}

/*********************************************************************
*
*       IP_DHCPC_GetState
*
*  Function description
*    This function returns the state of the DHCP client.
*
*  Return value
*    0    Not in use
*    >0   In use, check DHCPC_STATE_ macros for detailed info
*/
unsigned IP_DHCPC_GetState(int IFIndex) {
  return _aState[IFIndex].state;
}

/*********************************************************************
*
*       IP_DHCPC_SetTimeout
*
*  Function description
*    Sets timeout parameters for DHCP requests.
*    RFC2131 demands exponential retransmission times (doubling retransmission time with each retry),
*    but in practice it may make more sense to work with a fixed, non-exponential timeout.
*/
void IP_DHCPC_SetTimeout(int IFIndex, U32 Timeout, U32 MaxTries, unsigned Exponential) {
  (void)IFIndex;
  _Timeout    = Timeout;
  _MaxTries   = MaxTries;
  _ExpTimeout = Exponential;
}

/*********************************************************************
*
*       IP_DHCPC_GetServer
*
*  Function description
*    Returns the address of the DHCP server for the given interface
*/
U32 IP_DHCPC_GetServer(int IFIndex) {
  return ntohl(_aState[IFIndex].srv_ipaddr);
}

/*********************************************************************
*
*       IP_DHCPC_CheckAddrIsValid
*
*  Function description
*    Returns the address of the DHCP server for the given interface
*/
void IP_DHCPC_CheckAddrIsValid(int IFIndex, int (*pfCheck) (DHCPC_CHECK_PARAS * pParas)) {
  _aState[IFIndex].pfCheck = pfCheck;
}


/*********************************************************************
*
*       IP_ShowDHCPClient
*
*  Function description
*    Print dhcp client statistics.
*    The statistical information is available at elevated debug levels only.
*/
int IP_ShowDHCPClient(void (*pfSendf)(void * pContext, const char * sFormat, ...), void * pContext) {
#if IP_DEBUG > 0
  int   i;
  pfSendf(pContext ,"dhcp client stats:\n");
  pfSendf(pContext ,"all errors:      %lu\n", IP_DHCPC_Stat.errors);
  pfSendf(pContext ,"discover sent:   %lu\n", IP_DHCPC_Stat.discovers);
  pfSendf(pContext ,"offers rcvd:     %lu\n", IP_DHCPC_Stat.offers);
  pfSendf(pContext ,"requests sent:   %lu\n", IP_DHCPC_Stat.requests);
  pfSendf(pContext ,"acks received:   %lu\n", IP_DHCPC_Stat.acks);
  pfSendf(pContext ,"bootp replys:    %lu\n", IP_DHCPC_Stat.bpreplys);
  pfSendf(pContext ,"declines sent:   %lu\n", IP_DHCPC_Stat.declines);
  pfSendf(pContext ,"releases sent:   %lu\n", IP_DHCPC_Stat.releases);
  pfSendf(pContext ,"naks received:   %lu\n", IP_DHCPC_Stat.naks);
  pfSendf(pContext ,"renew req sent:  %lu\n", IP_DHCPC_Stat.renew);
  pfSendf(pContext ,"rebind req sent: %lu\n", IP_DHCPC_Stat.rebind);
  pfSendf(pContext ,"relay agent errs:%lu\n", IP_DHCPC_Stat.rlyerrs);

  for ( i=0 ; i < IP_MAX_IFACES ; i++ ) {
    pfSendf(pContext ,"Interface %d state = %s\n",i+1, dhc_state_str[ _aState[i].state ] );
    if ( _aState[i].state == DHCPC_STATE_UNUSED ) {
      continue;
    } else {
       pfSendf(pContext ,"   tries=%d, xid=%lu, secs=%d\n",  _aState[i].tries,_aState[i].xid,_aState[i].secs);
       pfSendf(pContext ,"   lease=%lu, t1=%lu, t2=%lu\n",   _aState[i].lease,_aState[i].t1,_aState[i].t2);
       pfSendf(pContext ,"   ip=%i, snmask=%i, gw=%i\n",     _aState[i].IpAddr, _aState[i].snmask, _aState[i].defgw);
       pfSendf(pContext , "   serverip=%i\n",                _aState[i].srv_ipaddr);
  #if defined(DHCP_MAXDNSRVS) && (DHCP_MAXDNSRVS > 0)
       {
          int   dnsindex;
          for (dnsindex=0; dnsindex < DHCP_MAXDNSRVS ; dnsindex++ ) {
             pfSendf(pContext , "   DNS%d=%i\n", dnsindex+1,  _aState[i].dnsrv[dnsindex]);
          }
       }
  #endif
    }
  }
#else
  pfSendf(pContext,"DHCPC statistics not available in release build.");
#endif
  return 0;
}


/*************************** End of file ****************************/


