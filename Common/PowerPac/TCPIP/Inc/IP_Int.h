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
File        : IP_Int.h
Purpose     : Internals used accross different layers of the TCP/IP stack
---------------------------END-OF-HEADER------------------------------
*/
/* Additional Copyrights: */
/* Copyright  2000 By InterNiche Technologies Inc. All rights reserved */
/* Portions Copyright 1990,1993 by NetPort Software. */
/* Portions Copyright 1986 by Carnegie Mellon */
/* Portions Copyright 1983 by the Massachusetts Institute of Technology */

#ifndef _IP_INT_H_              // Avoid multiple/recursive inclusion
#define _IP_INT_H_

#include <stdlib.h>   /* for atoi(), exit() */
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

#include "IP.h"
#include "IP_ConfDefaults.h"
#include "IP_Q.h"

#if defined(__cplusplus)
extern "C" {     /* Make sure we have C-declarations in C++ programs */
#endif

#ifdef IPCORE_C
  #define EXTERN
#else
  #define EXTERN extern
#endif

#ifdef IPDEBUG_C
  #define EXTERN_D
#else
  #define EXTERN_D extern
#endif

#if IP_SUPPORT_LOG
  #define IP_LOG(p) IP_Logf p
#else
  #define IP_LOG(p)
#endif

#if IP_SUPPORT_WARN
  #define IP_WARN(p) IP_Warnf p
#else
  #define IP_WARN(p)
#endif

#if IP_DEBUG >= 3
  #define IP_WARN_INTERNAL(p) IP_Warnf p
#else
  #define IP_WARN_INTERNAL(p)
#endif


/* usefull macros: */
#define  IP_MIN(x,y)    ((x)  <  (y)   ?  (x)   :  (y))
#define  IP_MAX(x,y)    ((x)  >  (y)   ?  (x)   :  (y))
#define  IP_COUNTOF(a)  (sizeof(a)/sizeof(a[0]))

// TBD: Replace in code by IP_MIN/MAX
#ifndef MIN
#define  MIN(a,b)    (a>b?b:a)
#define  MAX(a,b)    (a>b?a:b)
#define  COUNTOF(a)  (sizeof(a)/sizeof(a[0]))
#endif

/* ethernet packet header */

struct ethhdr {
U8   e_dst[6];
U8   e_src[6];
U16  e_type;
};

typedef struct ethhdr ETHHDR;

#define  ETHHDR_SIZE 16
#define  ETHHDR_BIAS 2


/* minimum & maximun length legal ethernet packet sizes */
#define     ET_MINLEN   60
#define     ET_MAXLEN   1514
#define     MINTU       60
#define     MTU         1514


typedef struct {
  U16 SrcPort;
  U16 DestPort;
  U16 NumBytes;
  U16 CheckSum;
} IP_UDP_HEADER;

typedef struct {
  U8  VerLen;
  U8  TOS;
  U16 NumBytes;
  U16 Id;
  U16 FlagsOff;
  U8  TTL;
  U8  Proto;
  U16 HeaderCheckSum;
  U32 SrcAddr;
  U32 DestAddr;
} IP_IP_HEADER;


/*
 * save socket options to be passed via the PACKET.
 */
struct ip_socopts {
  U8 ip_tos;     /* TOS */
  U8 ip_tll;     /* TLL */
};


/* The following structure is the iface MIB from rfc1213. Most fields are
 * simple counters and are used as such by the InterNiche networking code.
 * Two exceptions are ifAdminStatus and ifOperStatus. We have tried to
 * keep their use consistent with the RFC.
 *    When an interface (NET) is created, it is marked down (i.e. the
 * ifAdminStatus is set to the "down" position) so that it can safely
 * be configured (a separate step), and then marked "up" once
 * configuration is complete. A NET with ifAdminStatus==DOWN will
 * not be used to send by the inet code.
 *    ifOperStatus should indicate the actual state of the link. For
 * ethernet this probably means UP almost all the time. For PPP it means
 * UP when connected, DOWN other wise; thus a modem in AUTOANSWER would
 * have the same ifAdminStatus and ifOperStatus (UP, DOWN) as a
 * disconnected modem.
 */

//
// Note (RS): Some elements are required by the stack, some are statistics only.
typedef struct IfMib {
  U32   ifIndex;          /* 1 - entry in nets[] */
  U8 * ifDescr;          /* 2 - string describing interface */
  U32   ifMtu;            /* 4 - Max Transmission Unit */
  U32   ifSpeed;          /* 5 - Theoretical max in bits/sec */
  U32   ifLastChange;     /* 9 - time of current ifOperState */
  U32   ifInOctets;       /* 10 - bytes received */
  U32   ifInUcastPkts;    /* 11 - DELIVERED unicast packets */
  U32   ifInNUcastPkts;   /* 12 - non-unicast delivered packets */
  U32   ifInDiscards;     /* 13 - dropped, ie no buffers */
  U32   ifInErrors;       /* 14 - bad received packets */
  U32   ifInUnknownProtos;/* 15 -  unknown  protocols */
  U32   ifOutOctets;      /* 16 - bytes sent */
  U32   ifOutUcastPkts;   /* 17 - unicasts sent */
  U32   ifOutNUcastPkts;  /* 18 - non-unicast packets sent */
  U32   ifOutDiscards;    /* 19 - send dropped - non-error */
  U32   ifOutErrors;      /* 20 - send dropped - errors */
  U32   ifOutQLen;        /* 21 - packets queued for send */
} IFMIB;


/* The NET struct has all the actual interface characteristics which
 * are visible at the internet level and has pointers to the
 * interface handling routines.
 */

typedef struct net {
  const IP_HW_DRIVER * pDriver;
  int   n_lnh;            /* net's local net header  size */
  int   n_mtu;            /* net's largest legal buffer size size */
  ip_addr  n_ipaddr;      /* interface's internet address */
  ip_addr  snmask;        /* interface's subnet mask */
  ip_addr  n_defgw;       /* the default gateway for this net */
  ip_addr  n_netbr;       /* our network broadcast address */
  U8    n_flags;          // Multicast / broadcast capabilities
  U8       n_hal;         // Hardware address length. Typically 6 for Ethernet
  U8    Caps;             // Capabilities of the driver. Tells us if the driver is capable of computing IP, TCP checksums etc.
  U8    ifType;           /* 3 - Ether, token, etc */
  U8    abHWAddr[6];      /* 6 - node address */
  U8    ifAdminStatus;    /* 7 - up=1, down=2, testing=3 */
  U8    ifOperStatus;     /* 8 - up=1, down=2, testing=3 */
  U8    HasError;
#if IP_INCLUDE_STAT
  struct IfMib mib;       /* the actual iface MIB image */
#endif
#if IP_MAX_DNS_SERVERS
  U32   aDNSServer[IP_MAX_DNS_SERVERS];
#endif
} NET;




/* bits for pkt->flags field */
#define  PKF_BCAST    0x01
#define  PKF_MCAST    0x02
#define  PKF_IPOPTS   0x04     /* ip_socopts present */
#define  PKF_IPV6_GBL_PING  0x20  /* global ping, if MC scope == 0xE */



/*********************************************************************
*
*       IP_PACKET_
*/
void        IP_PACKET_Q_Add                (QUEUE * pQ, IP_PACKET * pPacket);    // Add item
IP_PACKET * IP_PACKET_Q_GetRemoveFirst     (QUEUE * pQ);                         // Get and remove first item
IP_PACKET * IP_PACKET_Q_TryGetRemoveFirst  (QUEUE * pQ);                         // Get and remove first item if any

#if IP_DEBUG
  #define IP_PACKET_Q_ADD(pQ, pPacket)          IP_PACKET_Q_Add(pQ, pPacket)
  #define IP_PACKET_Q_GET_REMOVE_FIRST(pQ)      IP_PACKET_Q_GetRemoveFirst(pQ)
  #define IP_PACKET_Q_TRY_GET_REMOVE_FIRST(pQ)  IP_PACKET_Q_TryGetRemoveFirst(pQ)
#else
  #define IP_PACKET_Q_ADD(pQ, pPacket)          IP_Q_Add(pQ, pPacket)
  #define IP_PACKET_Q_GET_REMOVE_FIRST(pQ)      (IP_PACKET*)IP_Q_GetRemoveFirst(pQ)
  #define IP_PACKET_Q_TRY_GET_REMOVE_FIRST(pQ)  (IP_PACKET*)IP_Q_TryGetRemoveFirst(pQ)
#endif


IP_PACKET *  IP_PACKET_Alloc(unsigned size);      /* allocate a packet for sending */
void         pk_free(IP_PACKET *);
int          pk_init(void);    /* call at init time to set up for IP_PACKET_Allocs */


/*********************************************************************
*
*       IP_PROT_
*
*  Defines protocol types
*/
#define IP_PROT_ICMP 1        // ICMP Protocol number on IP
#define IP_PROT_TCP  6        // TCP protocol type byte in IP header
#define IP_PROT_UDP 17




#define  IP_SPEED_UNKNOWN                               0
#define  IP_SPEED_10MHZ                                 1
#define  IP_SPEED_100MHZ                                2
#define  IP_SPEED_1GHZ                                  3


#define  IP_DUPLEX_UNKNOWN                        0       /* Duplex uknown or auto-neg incomplete        */
#define  IP_DUPLEX_HALF                           1       /* Duplex = Half Duplex                        */
#define  IP_DUPLEX_FULL                           2       /* Duplex = Full Duplex                        */

#define LOCK_NET()    IP_OS_Lock()
#define UNLOCK_NET()  IP_OS_Unlock()

#if IP_DEBUG
  #define ASSERT_LOCK() IP_OS_AssertLock()
#else
  #define ASSERT_LOCK()
#endif

#define ENTER_CRIT_SECTION()  IP_OS_DisableInterrupt()
#define EXIT_CRIT_SECTION()   IP_OS_EnableInterrupt()

void * IP_AllocPacketBuffer(int NumBytes);
void   IP_OnRx(void);
void   IP_TCP_OnRx(IP_PACKET *);
int    IP__SendPacket(IP_PACKET * pPacket);


/* A portable macro to check whether a ptr is within a certain range.
 * In an environment where selectors are important, this macro will
 * be altered to reflect that.
 */
#define IN_RANGE(p1, len1, p2)  (((p1) <= (p2)) && (((p1) + (len1)) > (p2)))

#define INETNET     0   /* index into nets[] for internet iface */
#define LOCALNET    1   /* index into nets[] for local net iface */


/* ARP holding packet while awaiting a response from fhost */
#define ARP_WAITING   IP_ERR_SEND_PENDING


struct mbuf;

void   dns_check(void);



/* For now, include IPv4 if it's not specificly excluded */
#define IP_V4  1

/* Internet status. Keeps track of packets gone through and errors.
 */
typedef struct IpMib   {
  U32   ipForwarding;     /* 1= we are a gateway; 2 = host */
  U32   ipDefaultTTL;     /* IP time to live */
  U32   ipInReceives;     /* total received datagrams (bad too) */
  U32   ipInHdrErrors;    /* Header Err (xsum, ver, ttl, etc) */
  U32   ipInAddrErrors;   /* nonsense IP addresses */
  U32   ipForwDatagrams;  /* routed packets */
  U32   ipUnknownProtos;  /* unknown protocol types */
  U32   ipInDiscards;     /* dropped (ie no buffer space) */
  U32   ipInDelivers;     /* delivered receive packets */
  U32   ipOutRequests;    /* sends (not includeing routed) */
  U32   ipOutDiscards;    /* sends dropped (no buffer) */
  U32   ipOutNoRoutes;    /* dropped, can't route */
  U32   ipReasmTimeout;   /* fragment reassembly timeouts */
  U32   ipReasmReqds;     /* frags received */
  U32   ipReasmOKs;       /* packets successfully reassembled */
  U32   ipReasmFails;     /* packets reassemblys failed */
  U32   ipFragOKs;        /* packets we fragmented for send */
  U32   ipFragFails;      /* packets we wanted to frag and couldnt */
  U32   ipFragCreates;    /* fragments we made */
  U32   ipRoutingDiscards;
} IPMIB;

EXTERN IPMIB ip_mib;           /* IP stats for snmp */

struct ip   {
   U8       ip_ver_ihl;    /* 4 bit version, 4 bit hdr len in 32bit words */
   U8       ip_tos;        /* Type of Service */
   U16      ip_len;        /* Total packet length including header */
   U16      ip_id;         /* ID for fragmentation */
   U16      ip_flgs_foff;  /* mask in flags as needed */
   U8       ip_time;       /* Time to live (secs) */
   U8       ip_prot;       /* protocol */
   U16      ip_chksum;     /* Header checksum */
   ip_addr  ip_src;        /* Source name */
   ip_addr  ip_dest;       /* Destination name */
};


/* Some useful definitions */

#define  IPHSIZ   sizeof(struct  ip)   /* internet header size */
#define  IP_VER   4     /* internet version */
#define  IP_TSRV  0     /* default type of service */
#define  IP_ID    0     /* kernel fills in IN id */
#define  IP_FLGS  0     /* no fragmentation yet */

/* fragmentation flag bits, for masking into 16bit flags/offset word */
#define  IP_FLG_DF   0x4000   /* Don't   Fragment (DF) bit */
#define  IP_FLG_MF   0x2000   /* More Fragments (MF) bit */
#define  IP_FLG_MASK 0xe000   /* for masking out all flags from word */

#define  IP_FOFF  0     /* " " " */

/* ethernet-ish packet types in NET endian: */
#define  IP_TYPE     htons(0x0800)
#define  ARP_TYPE    htons(0x0806)

/* Some macros for finding IP offsets in incoming packets */
#define  ip_head(ppkt)     (struct  ip *)(ppkt->pData)
#define  ip_hlen(pip)      (((pip)->ip_ver_ihl  &  0x0f) << 2)
#define  ip_data(pip)      ((char *)(pip) +  ip_hlen(pip))
#define  ip_optlen(pip)    (ip_hlen(pip)  -  20)


/* prototype IP routines */
int      IP_Write(U8 prot, IP_PACKET *);
int      IP_DispatchPacket(IP_PACKET *);      /* low level process of incomming IP */
ip_addr  ip_mymach(ip_addr);
NET *    iproute(ip_addr, ip_addr*);
int      del_route(ip_addr dest, ip_addr mask, int iface);

/* struct ipraw_ep - IP's endpoint for raw IP access
 */
struct ipraw_ep {
   struct ipraw_ep * ipr_next;
   ip_addr ipr_laddr;               /* local host IP address binding */
   ip_addr ipr_faddr;               /* remote host IP address binding
                                     * (connection) */
   int (*ipr_rcv)(IP_PACKET *, void*);   /* incoming packet handler */
   void * ipr_data;                 /* user 'per-connection' token */
   U8   ipr_prot;                   /* IP protocol ID binding */
};

/* ipraw_eps - pointer to list of raw IP endpoints
 */
extern struct ipraw_ep * ipraw_eps;

/* prototypes for raw IP API functions
 */
struct ipraw_ep * ip_raw_open(U8 prot, ip_addr laddr, ip_addr faddr, int (*handler)(IP_PACKET *, void *), void * data);
void ip_raw_close(struct ipraw_ep * ep);
int ip_raw_write(IP_PACKET * p);
IP_PACKET * ip_raw_alloc(int datalen, int hdrincl);
void ip_raw_free(IP_PACKET * p);
int ip_raw_maxalloc(int hdrincl);
void dhc_setup(void);


/*
 * Socket options for use with [gs]etsockopt at the IP level.
 * First word of comment is data type; bool is stored in int.
 */
#define  IP_OPTIONS        1  /* buf/ip_opts; set/get IP options */
#define  IP_HDRINCL        2  /* int; header is included with data */
#define  IP_TOS            3  /* int; IP type of service and preced. */
#define  IP_TTL_OPT        4  /* int; IP time to live */
#define  IP_RECVOPTS       5  /* bool; receive all IP opts w/dgram */
#define  IP_RECVRETOPTS    6  /* bool; receive IP opts for response */
#define  IP_RECVDSTADDR    7  /* bool; receive IP dst addr w/dgram */
#define  IP_RETOPTS        8  /* ip_opts; set/get IP options */
#define  IP_MULTICAST_IF   9  /* u_char; set/get IP multicast i/f  */
#define  IP_MULTICAST_TTL  10 /* u_char; set/get IP multicast ttl */
#define  IP_MULTICAST_LOOP 11 /* u_char; set/get IP multicast loopback */
#define  IP_ADD_MEMBERSHIP 12 /* ip_mreq; add an IP group membership */
#define  IP_DROP_MEMBERSHIP 13 /* ip_mreq; drop an IP group membership */

IP_PACKET * ip_copypkt(IP_PACKET * p);        /* copy packet into new buffer */

/* ethernet & token ring protocol values in local endian: */
#define     IP_TYPE_ARP    ntohs(0x0806)     /* ARP type */
#define     IP_TYPE_IP     ntohs(0x0800)     /* IP type */
#define     TCPTP    0x06     /* TCP type on IP (8 bit, no swap) */

void * IP_Alloc   (U32 NumBytes);
void * IP_TryAlloc(U32 NumBytes);
void * IP_AllocZeroed(U32 NumBytesReq);
void   IP_ReadPackets(void);
void   IP_AddTimer(void (* pfHandler)(void), int Period);
void   IP_ARP_Timer(void);
void   IP_ARP_SendRequest(ip_addr dest_ip);
int    IP_ARP_HasEntry(ip_addr dest_ip);

int   in_broadcast(U32 ipaddr);     /* TRUE if ipaddr is broadcast */

typedef struct socket SOCKET;

void     IP_TCP_DataUpcall(SOCKET *);   /* internal (invoke upcall) */

#define     MAXOPTLEN      256   /* size of alloc for TCP options */

#define     PR_SLOWHZ   (1000 / IP_TCP_SLOW_PERIOD)   // Slow TCP ticks per second. Old default: 2



#define     IPPORT_RESERVED      1024
#define     IPPORT_USERRESERVED     5000  /* more BSDness */
#define     INADDR_BROADCAST     0xffffffffL /* must be masked */


int     ip_output(struct mbuf *); /* ip send routine for TCP layers */
U16     in_cksum (struct mbuf *, int);   /* mbuf checksumming routine */


#if IP_DEBUG > 0
  #define  SETTP(tp,action)  tp=(action)
#else
  #define  SETTP(tp,action)  (action)
#endif

extern   U16  select_wait;

void     IP_MBUF_Init(void);
int      udp_usrreq(SOCKET * so, struct mbuf * m, struct mbuf * nam);
int      rawip_usrreq(SOCKET * so, struct mbuf * m, struct mbuf * nam);
int      ip_raw_input(IP_PACKET * p);

void     IP_GetNextOutPacket    (void ** ppData, unsigned * pNumBytes);    // Obsolete, but still available (Use ...Fast-Version)
unsigned IP_GetNextOutPacketFast(void ** ppData);
void IP_RemoveOutPacket(void);
IP_OPTIMIZE
unsigned IP_cksum(void * ptr, unsigned NumHWords);
unsigned IP_CalcChecksum_Byte(const void * pData, unsigned NumBytes);

/*********************************************************************
*
*       IP_SOCKET
*/
SOCKET *        IP_SOCKET_Alloc(void);
void            IP_SOCKET_Free(struct socket * pSock);
struct socket * IP_SOCKET_h2p(long hSock);
long            IP_SOCKET_p2h(struct socket * s);

void     IP_FreeSockOpt(struct ip_socopts * p);

/*********************************************************************
*
*       IP_GLOBAL
*/
typedef struct {
  U8           ConfigCompleted;
  U8           InitCompleted;
  U8           UDP_RxChecksumEnable;
  U8           UDP_TxChecksumEnable;
  U16          PacketId;
  U8           TTL;
  U16          Padding;
  IP_RX_HOOK * pfOnRx;
  U32          TCP_TxWindowSize;
  U32          TCP_RxWindowSize;
  unsigned     aBufferConfigNum[2];     // Number of buffers for each pool
  unsigned     aBufferConfigSize[2];    // Size of buffers for each pool
  U32 *        pMem;
  U32          NumBytesRem;             // Number of bytes remaining on memory area used as heap
  QUEUE        aFreeBufferQ[2];         // Free buffers. Big: FTP etc.. Small: ARPs, TCP acks, pings etc.
  QUEUE        SocketInUseQ;            // A socket queue
  QUEUE        SocketFreeQ;
  QUEUE        MBufInUseQ;              // In-use mbufs
  QUEUE        MBufFreeQ;               // Free mbufs
  QUEUE        RxPacketQ;
  QUEUE        TxPacketQ;
  U32          NextSocketHandle;
} IP_GLOBAL;

EXTERN U32     IP_LinkSpeed;     // Current Link Speed (either 10 or 100 MHz)
EXTERN U32     IP_LinkDuplex;    // Current duplex state

/*********************************************************************
*
*       Debug variables
*/
typedef struct {
  //
  //  Support for an option that allows us to deliberatly loose or slow down packets
  //  Variables are used in this module only, but also only in debug builds.
  //  They could be static, but this would lead to warnings in release builds so we leave them public.
  //
  int TxDropCnt;  // Packets since last loss
  int TxDropRate; // Number of packets to punt (3 is "1 in 3")
  int RxDropCnt;  // Packets since last loss
  int RxDropRate; // Number of packets to punt (3 is "1 in 3")
  int PacketDelay;
} IP_DEBUG_VARS;

EXTERN_D IP_DEBUG_VARS  IP_Debug;
EXTERN_D IP_GLOBAL IP_Global;
EXTERN NET       IP_aIFace[IP_MAX_IFACES];  // Actual net structs static memory for interface structs and interface mib data */

void  IP_IP_OnRx(IP_PACKET *);        /* arp received packet upcall */
int   IP_ETH_SendBroadcast(IP_PACKET * pkt);
int   IP_ETH_SendMulticast(IP_PACKET * pkt, ip_addr dest_ip);

/* arp function prototypes */
int   IP_ARP_OnRx(IP_PACKET *);        /* arp received packet upcall */
void  IP_ARP_SetAgeout(U32 Ageout);
int   IP_ARP_Send (IP_PACKET * pkt, ip_addr dest_ip);

void IP_Logf (U32 Type, const char * sFormat, ...);
void IP_Warnf(U32 Type, const char * sFormat, ...);
void IP_PrintfSafe(char * pBuffer, const char * sFormat, int BufferSize, va_list * pParamList);
char IP_IsExpired(I32 Time);

int IP_IsEtherBroadcast(char * address);
void * IP_AllocWithQ(QUEUE * pQueue, int NumBytes);
void * IP_TryAllocWithQ(QUEUE * pQueue, int NumBytes);

/* TCP code options - these need to preceed the nptcp.h include */
#define  UDP_SOCKETS    1  /* support UDP (DGRAM) under sockets API */

struct TcpMib {
  long     tcpRtoAlgorithm;
  long     tcpRtoMin;
  long     tcpRtoMax;
  long     tcpMaxConn;
  U32      tcpActiveOpens;
  U32      tcpPassiveOpens;
  U32      tcpAttemptFails;
  U32      tcpEstabResets;
  U32      tcpCurrEstab;
  U32      tcpInSegs;
  U32      tcpOutSegs;
  U32      tcpRetransSegs;
  void *   tcpConnTable;  /*32 bit ptr */
  U32      tcpInErrs;
  U32      tcpOutRsts;
};
extern   struct TcpMib  tcpmib;

#define  LONG2MBUF(ln)  ((struct mbuf *)ln)
#define  MBUF2LONG(mb)  ((long)mb)

/* some SNMP-derived net interface types, per rfc 1156, pg 14 */
/* ...these values now assigned by RFC-1700 */
#define  ETHERNET       6
#define  SLIP           28       /* SLIP, per rfc1213 */
#define  PPP            23       /* PPP, per rfc1213 */


/* bit definitions for net.n_flags */
#define  NF_BCAST    0x0001   /* device supports broadcasts */
#define  NF_MCAST    0x0002   /* device supports multicast */

/* parameters to ni_set_state - match RFC-1213 ifXxxxStatus */
#define  NI_UP     1
#define  NI_DOWN   2


#define  IP_TCP_HEADER_SIZE     (68)     // Typically the worst-case header.  40 IP/TCP, 12 RTTM, 14+2 Ether

/* define a way to convert NET pointers to index numbers */
int      if_netnumber(NET *);   /* get index into nets list for NET */

unsigned long   getipadd(char *);   /* resolve hostname to IP address */


#ifdef USE_PPP
  #define IS_BROADCAST(ifc, address) (FALSE)  /* PPP has no broadcast */
#else
  #define IS_BROADCAST(ifc, pAddr) IP_IsEtherBroadcast(pAddr)
#endif

/*********************************************************************
*
*       UDP
*/
/* UDP Header structure */
struct udp {
   U16  ud_srcp;    /* source port */
   U16  ud_dstp;    /* dest port */
   U16  ud_len;     /* length of UDP packet */
   U16  ud_cksum;   /* UDP checksum */
};

/* Flags for use in u_flags: */
#define UDPCF_V4     0x0001   /* flag: tied to IPv4 */
#define UDPCF_V6     0x0002   /* flag: tied to IPv6 */

/* UDP stats (MIB information), see RFC 1156 */
typedef struct UdpMib {
   U32   udpInDatagrams;   /* total delivered Datagrams */
   U32   udpNoPorts;       /* undelivered datagrams: unused port */
   U32   udpInErrors;      /* undelivered datagrams: other reasons */
   U32   udpOutDatagrams;  /* successfully sent datagrams */
} UDPMIB;

EXTERN UDPMIB udp_mib;    /* udp stats block */


/* The UDP Connection structure */
struct udp_conn {
   struct udp_conn * u_next;
   U16  u_flags;    /* flags for this connection */
   U16  u_lport;    /* local port (host byte order) */
   U16  u_fport;    /* foreign port (host byte order) */
#ifdef IP_V4
   ip_addr  u_lhost;    /* local host IP address (network byte order) */
   ip_addr  u_fhost;    /* foreign host IP address (network byte order) */
#endif
   int      (*u_rcv)(IP_PACKET *, void*);   /* incoming packet handler */
   void *   u_data;     /* user 'per-connection' token */
   int      (*u_durcv)(void);  /* incoming dest. unreach. handler */
};

EXTERN   UDPCONN  firstudp;

int      IP_UDP_OnRx(IP_PACKET *);
int      udp_maxalloc(void);

#undef EXTERN

#if defined(__cplusplus)
  }              // Make sure we have C-declarations in C++ programs
#endif

#endif                // Avoid multiple/recursive inclusion

/*************************** End of file ****************************/
