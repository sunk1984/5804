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
File        : IP.h
Purpose     : API of the TCP/IP stack
---------------------------END-OF-HEADER------------------------------
*/
/* Additional Copyrights: */
/* Copyright  2000 By InterNiche Technologies Inc. All rights reserved */
/* Portions Copyright 1990,1993 by NetPort Software. */
/* Portions Copyright 1986 by Carnegie Mellon */
/* Portions Copyright 1983 by the Massachusetts Institute of Technology */

#ifndef _IP_H_
#define _IP_H_

#include "SEGGER.h"
#include "IP_ConfDefaults.h"
#include "IP_socket.h"

#if defined(__cplusplus)
extern "C" {     /* Make sure we have C-declarations in C++ programs */
#endif

#define IP_VERSION   16203   // Format: Mmmrr. Example: 10201 is 1.02a

/*********************************************************************
*
*       IP_MTYPE
*
*  Ids to distinguish different message types
*/
#define IP_MTYPE_INIT         (1UL << 0)
#define IP_MTYPE_CORE         (1UL << 1)
#define IP_MTYPE_ALLOC        (1UL << 2)
#define IP_MTYPE_DRIVER       (1UL << 3)
#define IP_MTYPE_ARP          (1UL << 4)
#define IP_MTYPE_IP           (1UL << 5)

#define IP_MTYPE_TCP_CLOSE    (1UL << 6)
#define IP_MTYPE_TCP_OPEN     (1UL << 7)
#define IP_MTYPE_TCP_IN       (1UL << 8)
#define IP_MTYPE_TCP_OUT      (1UL << 9)
#define IP_MTYPE_TCP_RTT      (1UL << 10)
#define IP_MTYPE_TCP_RXWIN    (1UL << 11)
#define IP_MTYPE_TCP          (IP_MTYPE_TCP_OPEN | IP_MTYPE_TCP_CLOSE | IP_MTYPE_TCP_IN | IP_MTYPE_TCP_OUT | IP_MTYPE_TCP_RTT)

#define IP_MTYPE_UDP_IN       (1UL << 12)
#define IP_MTYPE_UDP_OUT      (1UL << 13)
#define IP_MTYPE_UDP          (IP_MTYPE_UDP_IN | IP_MTYPE_UDP_OUT)

#define IP_MTYPE_LINK_CHANGE  (1UL << 14)
#define IP_MTYPE_DHCP         (1UL << 17)
#define IP_MTYPE_DHCP_EXT     (1UL << 18)

#define IP_MTYPE_APPLICATION  (1UL << 19)


#define IP_MTYPE_ICMP         (1UL << 20)
#define IP_MTYPE_NET_IN       (1UL << 21)
#define IP_MTYPE_NET_OUT      (1UL << 22)

#define IP_MTYPE_DNS          (1UL << 24)

#define IP_MTYPE_SOCKET_STATE (1UL << 26)
#define IP_MTYPE_SOCKET_READ  (1UL << 27)
#define IP_MTYPE_SOCKET_WRITE (1UL << 28)
#define IP_MTYPE_SOCKET       (IP_MTYPE_SOCKET_STATE | IP_MTYPE_SOCKET_READ | IP_MTYPE_SOCKET_WRITE)
#define IP_MTYPE_DNSC         (1UL << 29)
#define IP_MTYPE_ACD          (1UL << 30)

void IP_Logf_Application(const char * sFormat, ...);
void IP_Warnf_Application(const char * sFormat, ...);

/*********************************************************************
*
*       IP_ERR_
*
*  Ids to distinguish different message types
*
* Stack generic error codes: generally full success is 0,
* definite errors are negative numbers, and indeterminate conditions
* are positive numbers. These may be changed if they conflict with
* defines in the target system.
* If you have to change
* these values, be sure to recompile ALL NetPort sources.
*/

/* programming errors */
#define IP_ERR_PARAM      -10 /* bad parameter */
#define IP_ERR_LOGIC      -11 /* sequence of events that shouldn't happen */
#define IP_ERR_NOCIPHER   -12 /* No corresponding cipher found for the cipher id */

/* system errors */
#define IP_ERR_NOMEM      -20 /* malloc or calloc failed */
#define IP_ERR_NOBUFFER   -21 /* ran out of free packets */
#define IP_ERR_RESOURCE   -22 /* ran out of other queue-able resource */
#define SEND_DROPPED IP_ERR_RESOURCE /* full queue or similar lack of resource */
#define IP_ERR_BAD_STATE  -23 /* TCP layer error */
#define IP_ERR_TIMEOUT    -24 /* TCP layer error */

#define IP_ERR_NOFILE     -25 /* expected file was missing */
#define IP_ERR_FILEIO     -26 /* file IO error */

/* net errors */
#define IP_ERR_SENDERR    -30 /* send to net failed at low layer */
#define IP_ERR_NOARPREP   -31 /* no ARP for a given host */
#define IP_ERR_BAD_HEADER -32 /* bad header at upper layer (for upcalls) */
#define IP_ERR_NO_ROUTE   -33 /* can't find a reasonable next IP hop */
#define IP_ERR_NO_IFACE   -34 /* can't find a reasonable interface */
#define IP_ERR_HARDWARE   -35 /* detected hardware failure */

/* conditions that are not really fatal OR success: */
#define IP_ERR_SEND_PENDING 1 /* packet queued pending an ARP reply */
#define IP_ERR_NOT_MINE     2 /* packet was not of interest (upcall reply) */
#define IP_ERR_ACD          3 // Address conflict detected

/*********************************************************************
*
*  Convert little/big endian - these should be efficient,
*  inline code or MACROs
*/
#if IP_IS_BIG_ENDIAN
  #define htonl(l) (l)
  #define htons(s) (s)
  #define IP_HTONL_FAST(l) (l)
#else
  #define htonl(l) IP_SwapU32(l)
  #define htons(s) ((U16)((U16)(s) >> 8) | (U16)((U16)(s) << 8))   /* Amazingly, some compilers really need all these U16 casts: */
//  #define htons(s) (((s) >> 8) | (U16)((s) << 8))
  #define IP_HTONL_FAST(v) (                      \
      (((U32)((v) << 0)  >> 24) << 0) | \
      (((U32)((v) << 8)  >> 24) << 8) | \
      (((U32)((v) << 16) >> 24) << 16) | \
      (((U32)((v) << 24) >> 24) << 24))
#endif

#define ntohl(l) htonl(l)
#define ntohs(s) htons(s)


U32 IP_SwapU32(U32 v);


/*********************************************************************
*
*  IP_OS_
*/
void IP_OS_Delay(unsigned ms);
void IP_OS_DisableInterrupt(void);
void IP_OS_EnableInterrupt(void);
void IP_OS_Init(void);
void IP_OS_Unlock(void);
void IP_OS_AssertLock(void);
void IP_OS_Lock  (void);
U32  IP_OS_GetTime32(void);
// Wait and signal for Net task
void IP_OS_WaitNetEvent  (unsigned ms);
void IP_OS_SignalNetEvent(void);
// Wait and signal for the optional Rx task
void IP_OS_WaitRxEvent  (void);
void IP_OS_SignalRxEvent(void);
// Wait and signal for application tasks
void IP_OS_WaitItem      (void * pWaitItem);
void IP_OS_WaitItemTimed (void * pWaitItem, unsigned Timeout);
void IP_OS_SignalItem(void * pWaitItem);
void IP_OS_AddTickHook(void (* pfHook)(void));


/*********************************************************************
*
*       IP_PACKET
*
* INCOMING: Incoming packets are always front-aligned in the
* pBuffer field. The pData pointer is set to pBuffer by the
* receiver and advanced by each layer of the stack before
* upcalling the next; ie the ethernet driver bumps the prot field
* by 14 and decrements plen by 14. PACKETs are pk_alloc()ed by
* the receiving net layer and pk_free()ed by the transport layer
* or application when it's finished with them. OUTGOING:
* Protocols install data into pBuffer with a front pad big enough
* to accomadate the biggest likely protocol headers, ususally
* about 62 bytes (14 ether + 24 IP + 24 TCP, where IP & TCP each
* have option fields.) prot plen are set for this data, and the
* protocol headers are prepended as the packet goes down the
* stack. pBuffer is not used in this case except for overflow
* checks. PACKETs are pk_alloc()ed by the sending protocol and
* freed by the lower layer level that dispatches them, usually
* net link layer driver. They can be held by ARP for several
* seconds while awaiting arp replys on initial sends to a new IP
* host, and the ARP code will free them when a reply comes in or
* times out.
*/
typedef   U32 ip_addr;

typedef struct IP_PACKET {
  struct IP_PACKET * pNext;
  struct net       * pNet;          // The interface (net) it came in on
  char             * pBuffer;       // Beginning of raw buffer
  char             * pData;         // Beginning of protocol/data. This is always >= pBuffer.
  ip_addr          fhost;           // IP address asociated with packet
  U16              NumBytes;        // Number of bytes in buffer
  U16              BufferSize;      // Length of raw buffer */
  U16              UseCnt;          // Use count, for cloning buffer
} IP_PACKET;


/*********************************************************************
*
*       Ethernet PHY
*/
typedef struct IP_PHY_CONTEXT IP_PHY_CONTEXT;

typedef struct {
  unsigned (*pfRead)         (IP_PHY_CONTEXT* pContext, unsigned RegIndex);
  void     (*pfWrite)        (IP_PHY_CONTEXT* pContext, unsigned RegIndex, unsigned  val);
} IP_PHY_ACCESS;

#define IP_PHY_MODE_10_HALF   (1 << 5)
#define IP_PHY_MODE_10_FULL   (1 << 6)
#define IP_PHY_MODE_100_HALF  (1 << 7)
#define IP_PHY_MODE_100_FULL  (1 << 8)

struct IP_PHY_CONTEXT {
  const IP_PHY_ACCESS * pAccess;
  void * pContext;   // Context needed for low level functions
  U8   Addr;
  U8   UseRMII;      // 0: MII, 1: RMII
  U16  SupportedModes;
  U16  Anar;         // Value written to ANAR (Auto-negotiation Advertisement register)
  U16  Bmcr;         // Value written to BMCR (basic mode control register)
};

typedef struct {
  int   (*pfInit)         (IP_PHY_CONTEXT * pContext);
  void  (*pfGetLinkState) (IP_PHY_CONTEXT * pContext, U32 * pDuplex, U32 * pSpeed);
} IP_PHY_DRIVER;

extern const IP_PHY_DRIVER IP_PHY_Generic;

/*********************************************************************
*
*       Ethernet HW driver
*/
typedef struct {
  int   (*pfInit)         (unsigned Unit);
  int   (*pfSendPacket)   (unsigned Unit);
  int   (*pfGetPacketSize)(unsigned Unit);                                   // Return the number of bytes in next packet, <= 0 if there is no more packet.
  int   (*pfReadPacket)   (unsigned Unit, U8 * pDest, unsigned NumBytes);    // Read (if pDest is valid) and discard packet.
  void  (*pfTimer)        (unsigned Unit);                                   // Routine is called periodically
  int   (*pfControl)      (unsigned Unit, int Cmd, void * p);                // Various control functions
} IP_HW_DRIVER;


typedef struct {
  unsigned NumAddr;
  const U8 * pHWAddr;                // Hardware addresses
} IP_NI_CMD_SET_FILTER_DATA;



/*********************************************************************
*
*       Drivers commands
*/
#define IP_NI_CMD_SET_FILTER                   0   // Set filter. Can handle multiple MAC-addresses.
#define IP_NI_CMD_CLR_BPRESSURE                1   // Clear back-pressure
#define IP_NI_CMD_SET_BPRESSURE                2   // Set back-pressure, to avoid receiving more data until the current data is handled
#define IP_NI_CMD_GET_CAPS                     3   // Retrieves the capabilites, which are a logical-or combination of the IP_NI_CAPS below
#define IP_NI_CMD_SET_PHY_ADDR                 4   // Allows settings the PHY address
#define IP_NI_CMD_SET_PHY_MODE                 5   // Allows settings the PHY in a specific mode (duplex, speed)
#define IP_NI_CMD_POLL                         6   // Poll MAC (typically once per ms) in cases where MAC does not trigger an interrupt.
#define IP_NI_CMD_GET_MAC_ADDR                 7   // Retrieve the MAC address from the MAC. This is used for hardware which stores the MAC addr. in an attached EEPROM.
#define IP_NI_CMD_DISABLE                      8   // Disable the network interface (MAC unit + PHY)
#define IP_NI_CMD_ENABLE                       9   // Enable the network interface (MAC unit + PHY)
#define IP_NI_CMD_SET_TX_BUFFER_SIZE          10   // Allows setting the size of the Tx buffer.
#define IP_NI_CMD_SET_SUPPORTED_DUPLEX_MODES  11   // Allows setting the supported duplex modes.
#define IP_NI_CMD_CFG_POLL                    12   // Configure the target to run in polling mode.



/*********************************************************************
*
*       Drivers capabilities
*/
#define IP_NI_CAPS_WRITE_IP_CHKSUM     (1 << 0)    // Driver capable of inserting the IP-checksum into an outgoing packet ?
#define IP_NI_CAPS_WRITE_UDP_CHKSUM    (1 << 1)    // Driver capable of inserting the UDP-checksum into an outgoing packet ?
#define IP_NI_CAPS_WRITE_TCP_CHKSUM    (1 << 2)    // Driver capable of inserting the TCP-checksum into an outgoing packet ?
#define IP_NI_CAPS_WRITE_ICMP_CHKSUM   (1 << 3)    // Driver capable of inserting the ICMP-checksum into an outgoing packet ?
#define IP_NI_CAPS_CHECK_IP_CHKSUM     (1 << 4)    // Driver capable of computing and comparing the IP-checksum of an incoming packet ?
#define IP_NI_CAPS_CHECK_UDP_CHKSUM    (1 << 5)    // Driver capable of computing and comparing the UDP-checksum of an incoming packet ?
#define IP_NI_CAPS_CHECK_TCP_CHKSUM    (1 << 6)    // Driver capable of computing and comparing the TCP-checksum of an incoming packet ?
#define IP_NI_CAPS_CHECK_ICMP_CHKSUM   (1 << 7)    // Driver capable of computing and comparing the ICMP-checksum of an incoming packet ?


void IP_NI_ClrBPressure  (unsigned Unit);
void IP_NI_SetBPressure  (unsigned Unit);

/*********************************************************************
*
*       PHY configuration
*/
#define IP_PHY_MODE_MII  0
#define IP_PHY_MODE_RMII 1

#define IP_PHY_ADDR_ANY       0xFF                          // IP_PHY_ADDR_ANY is used as PHY addr to initiate automatic scan for PHY
#define IP_PHY_ADDR_INTERNAL  0xFE                          // IP_PHY_ADDR_INTERNAL is used as PHY addr to select internal PHY

void IP_NI_ConfigPHYAddr       (unsigned Unit, U8 Addr);    // Configure PHY Addr (5-bit)
void IP_NI_ConfigPHYMode       (unsigned Unit, U8 Mode);    // Configure PHY Mode: 0: MII, 1: RMII
void IP_NI_ConfigPoll          (unsigned Unit);
void IP_NI_SetError            (unsigned Unit);
int  IP_NI_SetTxBufferSize     (unsigned Unit, unsigned NumBytes);
int  IP_SetSupportedDuplexModes(unsigned Unit, unsigned DuplexMode);


/*********************************************************************
*
*       IP stack tasks
*/
void IP_Task(void);
void IP_RxTask(void);
void IP_ShellServer(void);


typedef int (IP_RX_HOOK)(IP_PACKET * pPacket);

/*********************************************************************
*
*       Core functions
*/
void IP_AddBuffers                (int NumBuffers, int BytesPerBuffer);
void IP_AddEtherInterface         (const IP_HW_DRIVER *pDriver);
void IP_AllowBackPressure         (char v);
void IP_AssignMemory              (U32 * pMem, U32 NumBytes);
void IP_ConfTCPSpace              (unsigned SendSpace, unsigned RecvSpace);  // Set window sizes
void IP_Exec                      (void);
void IP_GetAddrMask               (U8 IFace, U32 * pAddr, U32 * pMask);
int  IP_GetCurrentLinkSpeed       (void);
U32  IP_GetGWAddr                 (U8 IFace);
void IP_GetHWAddr                 (U8 IFace, U8 * pDest, unsigned Len);
U32  IP_GetIPAddr                 (U8 IFace);
const char * IP_GetIPPacketInfo   (IP_PACKET * pPacket);
int  IP_GetVersion                (void);                   // Format: Mmmrr. Sample 10201 is 1.02a
void IP_ICMP_SetRxHook            (IP_RX_HOOK * pfRxHook);
int  IP_IFaceIsReady              (void);
void IP_Init                      (void);
int  IP_NI_GetCaps                (unsigned Unit);
int  IP_NI_LoadHWAddr             (unsigned Unit);
void IP_Panic                     (const char * sError);
void IP_SetAddrMask               (U32 Addr, U32 Mask);
int  IP_SetCurrentLinkState       (U32 Duplex, U32 Speed);  // Called from driver
void IP_SetDefaultTTL             (int v);
void IP_SetGWAddr                 (U8 IFace, U32 GWAddr);
void IP_SetHWAddr                 (const U8 * pHWAddr);
void IP_SetMTU                    (U8 IFace, U32 Mtu);
int  IP_SendPacket                (unsigned IFace, void * pData, int NumBytes);
int  IP_SendPing                  (ip_addr, char* pData, unsigned NumBytes, U16 SeqNo);
void IP_SetRxHook                 (IP_RX_HOOK * pfRxHook);
void IP_SOCKET_SetDefaultOptions  (U16 v);
void IP_SOCKET_SetLimit           (unsigned Limit);
void IP_TCP_Set2MSLDelay          (unsigned v);
void IP_TCP_SetConnKeepaliveOpt   (U32 Init, U32 Idle, U32 Period, U32 Cnt);
void IP_TCP_SetRetransDelayRange  (unsigned RetransDelayMin, unsigned RetransDelayMax);
void IP_X_Config                  (void);

/*********************************************************************
*
*       Log/Warn functions
*/
void IP_Log           (const char * s);
void IP_Warn          (const char * s);
void IP_SetLogFilter  (U32 FilterMask);
void IP_SetWarnFilter (U32 FilterMask);
void IP_AddLogFilter  (U32 FilterMask);
void IP_AddWarnFilter (U32 FilterMask);


/*********************************************************************
*
*       DNS (Domain name system)
*
*  Name resolution
*/
// Description of data base entry for a single host.
struct hostent {
  char *  h_name;        // Official name of host.
  char ** h_aliases;     // Alias list.
  int     h_addrtype;    // Host address type.
  int     h_length;      // Length of address.
  char ** h_addr_list;   // List of addresses from name server.
#define h_addr h_addr_list[0] /* Address, for backward compatibility.  */
#ifdef DNS_CLIENT_UPDT
  // Extra variables passed in to Dynamic DNS updates.
  char *  h_z_name;      // IN- zone name for UPDATE packet.
  ip_addr h_add_ipaddr;  // IN- add this ip address for host name in zone.
  U32     h_ttl;         // IN- time-to-live field for UPDATE packet.
#endif
};


struct hostent * gethostbyname (char * name);
int              IP_ResolveHost(char * host, ip_addr *   address,  int   flags);
void             IP_DNS_SetServer (U32 DNSServerAddr);
U32              IP_DNS_GetServer (void);
int              IP_DNS_SetServerEx (U8 IFace, U8 DNSServer, const U8 * pDNSAddr, int AddrLen);
void             IP_DNS_GetServerEx (U8 IFace, U8 DNSServer, U8 * pDNSAddr, int * pAddrLen);
char           * IP_ParseIPAddr(ip_addr * ipout,  unsigned *  sbits, char *   stringin);
void             IP_DNSC_SetMaxTTL(U32 TTL);


/*********************************************************************
*
*       Utility functions
*
* RS: Maybe we should move them into a UTIL module some time ? (We can keep macros here for compatibility)
*/
I32  IP_BringInBounds(I32 v, I32 Min, I32 Max);
U32  IP_LoadU32BE(const U8 * pData);
U32  IP_LoadU32LE(const U8 * pData);
U32  IP_LoadU32TE(const U8 * pData);
U32  IP_LoadU16BE(const U8 * pData);
U32  IP_LoadU16LE(const U8 * pData);
void IP_StoreU32BE(U8 * p, U32 v);
void IP_StoreU32LE(U8 * p, U32 v);

char IP_tolower(char c);
char IP_isalpha(char c);
char IP_isalnum(char c);
int  IP_PrintIPAddr(char * pDest, U32 IPAddr, int BufferSize);



/*********************************************************************
*
*       UDP
*/
typedef  struct udp_conn * UDPCONN;
typedef  U32 IP_ADDR;

#define IP_RX_ERROR        -1
#define IP_OK               0
#define IP_OK_KEEP_PACKET   1

typedef UDPCONN IP_UDP_CONN;

IP_UDP_CONN IP_UDP_Open(IP_ADDR IPAddr, U16 fport, U16 lport, int(*)(IP_PACKET *, void * pContext) , void * pContext);
void        IP_UDP_Close(IP_UDP_CONN);
IP_PACKET * IP_UDP_Alloc(int NumBytes);
int         IP_UDP_Send       (int IFace, IP_ADDR FHost, U16 fport, U16 lport, IP_PACKET * pPacket);
int         IP_UDP_SendAndFree(int IFace, IP_ADDR FHost, U16 fport, U16 lport, IP_PACKET * pPacket);
void        IP_UDP_Free       (IP_PACKET * pPacket);
U16         IP_UDP_FindFreePort(void);
U16         IP_UDP_GetLPort  (const IP_PACKET *pPacket);
void *      IP_UDP_GetDataPtr(const IP_PACKET *pPacket);
void        IP_UDP_GetSrcAddr(const IP_PACKET *pPacket, void * pSrcAddr, int AddrLen);

void        IP_UDP_EnableRxChecksum(void);
void        IP_UDP_DisableRxChecksum(void);
void        IP_UDP_EnableTxChecksum(void);
void        IP_UDP_DisableTxChecksum(void);


/*********************************************************************
*
*       TCP Zero copy
*/
IP_PACKET * IP_TCP_Alloc      (int datasize);
void        IP_TCP_Free       (        IP_PACKET * pPacket);
int         IP_TCP_Send       (long s, IP_PACKET * pPacket);
int         IP_TCP_SendAndFree(long s, IP_PACKET * pPacket);

/*********************************************************************
*
*       IP_DHCPC_...
*
*  DHCP (Dynamic host configuration protocol) functions.
*/
//
// Structure used to pass parameters to the optional address check function
// in the application.
//
typedef struct DHCPC_CHECK_PARAS {
  U32 IpAddr;
  U32 NetMask;
} DHCPC_CHECK_PARAS;

void     IP_DHCPC_Activate   (int IFIndex, const char *sHost, const char *sDomain, const char *sVendor);
unsigned IP_DHCPC_GetState   (int IFIndex);
void     IP_DHCPC_Halt       (int IFIndex);
void     IP_DHCPC_SetCallback(int IFIndex, int (*routine)(int,int) );
void     IP_DHCPC_SetClientId(int IFIndex, const U8 *pClientId, unsigned ClientIdLen);
void     IP_DHCPC_SetTimeout (int IFIndex, U32 Timeout, U32 MaxTries, unsigned Exponential);
U32      IP_DHCPC_GetServer  (int IFIndex);
void     IP_DHCPC_CheckAddrIsValid(int IFIndex, int (*pfCheck) (DHCPC_CHECK_PARAS * pParas));



/*********************************************************************
*
*       IP_BOOTPC_...
*
*  BBOOTP - bootstrap Protocol
*/
void    IP_BOOTPC_Activate (int IFIndex);
//
// We do not need extra functions, since we have the compatible functions with IP_DHCPC_ prefix
//
#define IP_BOOTPC_Halt(IFIndex)                                     IP_DHCPC_Halt(IFIndex)
#define IP_BOOTPC_SetTimeout(IFIndex,Timeout,MaxTries,Exponential)  IP_DHCPC_SetTimeout(IFIndex,Timeout,MaxTries,1)



/*********************************************************************
*
*       IP_Show_...
*
*  Text output functions informing about the state of various components of the software
*/
int IP_ShowARP       (void (*pfSendf)(void * pContext, const char * sFormat, ...), void * pContext);
int IP_ShowICMP      (void (*pfSendf)(void * pContext, const char * sFormat, ...), void * pContext);
int IP_ShowTCP       (void (*pfSendf)(void * pContext, const char * sFormat, ...), void * pContext);
int IP_ShowBSDConn   (void (*pfSendf)(void * pContext, const char * sFormat, ...), void * pContext);
int IP_ShowBSDSend   (void (*pfSendf)(void * pContext, const char * sFormat, ...), void * pContext);
int IP_ShowBSDRcv    (void (*pfSendf)(void * pContext, const char * sFormat, ...), void * pContext);
int IP_ShowMBuf      (void (*pfSendf)(void * pContext, const char * sFormat, ...), void * pContext);
int IP_ShowMBufList  (void (*pfSendf)(void * pContext, const char * sFormat, ...), void * pContext);
int IP_ShowSocketList(void (*pfSendf)(void * pContext, const char * sFormat, ...), void * pContext);
int IP_ShowStat      (void (*pfSendf)(void * pContext, const char * sFormat, ...), void * pContext);
int IP_ShowUDP       (void (*pfSendf)(void * pContext, const char * sFormat, ...), void * pContext);
int IP_ShowUDPSockets(void (*pfSendf)(void * pContext, const char * sFormat, ...), void * pContext);
int IP_ShowDHCPClient(void (*pfSendf)(void * pContext, const char * sFormat, ...), void * pContext);
int IP_ShowDNS       (void (*pfSendf)(void * pContext, const char * sFormat, ...), void * pContext);
int IP_ShowDNS1      (void (*pfSendf)(void * pContext, const char * sFormat, ...), void * pContext);


typedef struct {
  long Socket;
  char * pBuffer;
  int BufferSize;
} IP_SENDF_CONTEXT;

void IP_Sendf(void * pContext, const char * sFormat, ...);

/*********************************************************************
*
*       IP_CONNECTION information
*
*/

#define IP_CONNECTION_TYPE_TCP 1

typedef void * IP_CONNECTION_HANDLE;
typedef struct {
  void * pSock;
  long hSock;
  U32  ForeignAddr;
  U32  LocalAddr;
  U16  ForeignPort;
  U16  LocalPort;
  U8   Type;
  U8   TcpState;
  U16  TcpMtu;
  U16  TcpMss;
  U32  TcpRetransDelay;
  U32  TcpIdleTime;
  U32  RxWindowCur;
  U32  RxWindowMax;
  U32  TxWindow;
} IP_CONNECTION;

int IP_INFO_GetConnectionList(IP_CONNECTION_HANDLE *pDest, int MaxItems);
int IP_INFO_GetConnectionInfo(IP_CONNECTION_HANDLE h, IP_CONNECTION * p);
const char * IP_INFO_ConnectionType2String(U8 Type);
const char * IP_INFO_ConnectionState2String(U8 State);

/*********************************************************************
*
*       Address conflict detection (ACD)
*
*/
typedef struct ACD_FUNC {
  U32      (*pfRenewIPAddr)(int IFace);  // Used to renew the IP address if a conflict has been detected during startup.
  int      (*pfDefend)     (int IFace);  // Used to defend the IP address against a conflicting host on the network.
  int      (*pfRestart)    (int IFace);  // Used to restart the address conflict detection.
} ACD_FUNC;

int IP_ACD_Start(int IFace);
void IP_ACD_Config(int IFace, unsigned ProbeNum, unsigned DefendInterval, const ACD_FUNC * pACDContext);

void IP_ARP_AddACDHandler(void (*pf)(int, IP_PACKET*));


#if defined(__cplusplus)
  }              // Make sure we have C-declarations in C++ programs
#endif

#endif   // Avoid multiple inclusion



