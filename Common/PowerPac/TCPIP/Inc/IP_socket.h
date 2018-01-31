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
File    : IP_socket.h
Purpose :
--------  END-OF-HEADER  ---------------------------------------------
*/

/* Additional Copyrights: */
/* Copyright 1997 - 2000 By InterNiche Technologies Inc. All rights reserved */
/* Copyright (c) 1982, 1986 Regents of the University of California.
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions
* are met:
* 1. Redistributions of source code must retain the above copyright
*    notice, this list of conditions and the following disclaimer.
* 2. Redistributions in binary form must reproduce the above copyright
*    notice, this list of conditions and the following disclaimer in the
*    documentation and/or other materials provided with the distribution.
* 3. [rescinded 22 July 1999]
* 4. Neither the name of the University nor the names of its contributors
*    may be used to endorse or promote products derived from this software
*    without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
* ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
* ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
* FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
* DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
* OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
* HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
* LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
* OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
* SUCH DAMAGE.
*/



#ifndef SOCKET_H                         // Avoid multiple inclusion
#define  SOCKET_H

#if defined(__cplusplus)
extern "C" {     /* Make sure we have C-declarations in C++ programs */
#endif

/*
 * Address families, for socket() call and "domain" variables.
 */

#define  AF_INET     2     /* internetwork: UDP, TCP, etc. */
#define  AF_INET6    3     /* IPv6 */
#define  PF_INET6    AF_INET6


/* IPPROTO_... definitions, from BSD <netinet/in.h> */
#define     IPPROTO_IP     0  /* added for IP multicasting changes */
#define     IPPROTO_IGMP   2  /* added for IP multicasting changes */
#define     IPPROTO_TCP    6
#define     IPPROTO_UDP    17
#define     IPPROTO_RAW    255



/* BSD sockets errors */

#define     ENOBUFS        1
#define     ETIMEDOUT      2
#define     EISCONN        3
#define     EOPNOTSUPP     4
#define     ECONNABORTED   5
#define     EWOULDBLOCK    6
#define     ECONNREFUSED   7
#define     ECONNRESET     8
#define     ENOTCONN       9
#define     EALREADY       10
#define     EINVAL         11
#define     EMSGSIZE       12
#define     EPIPE          13
#define     EDESTADDRREQ   14
#define     ESHUTDOWN      15
#define     ENOPROTOOPT    16
#define     EHAVEOOB       17
#define     ENOMEM         18
#define     EADDRNOTAVAIL  19
#define     EADDRINUSE     20
#define     EAFNOSUPPORT   21
#define     EINPROGRESS    22
#define     ELOWER         23    /* lower layer (IP) error */
#define     ENOTSOCK       24    /* Includes sockets which closed while blocked */
#define     EIEIO 27 /* bad input/output on Old Macdonald's farm :-) */
#define     ETOOMANYREFS   28    // Multicast only
#define     EFAULT         29
#define     ENETUNREACH    30









/*
 * Types
 */
#define     SOCK_STREAM    1     /* stream socket */
#define     SOCK_DGRAM     2     /* datagram socket */
#define     SOCK_RAW       3     /* raw-protocol interface */
#define     SOCK_RDM       4     /* reliably-delivered message */
#define     SOCK_SEQPACKET 5     /* sequenced packet stream */

/*
 * Option flags per-socket.
 */
#define     SO_DEBUG       0x00001     /* turn on debugging info recording */
#define     SO_ACCEPTCONN  0x00002     /* socket has had listen() */
#define     SO_REUSEADDR   0x00004     /* allow local address reuse */
#define     SO_KEEPALIVE   0x00008     /* keep connections alive */
#define     SO_DONTROUTE   0x00010     /* just use interface addresses */
#define     SO_BROADCAST   0x00020     /* permit sending of broadcast msgs */
#define     SO_USELOOPBACK 0x00040     /* bypass hardware when possible */
#define     SO_LINGER      0x00080     /* linger on close if data present */
#define     SO_OOBINLINE   0x00100     /* leave received OOB data in line */
#define     SO_TCPSACK     0x00200     /* Allow TCP SACK (Selective acknowledgment) */
#define     SO_WINSCALE    0x00400     /* Set scaling window option */
#define     SO_TIMESTAMP   0x00800     /* Set TCP timestamp option */
#define     SO_BIGCWND     0x01000     /* Large initial TCP Congenstion window */
#define     SO_HDRINCL     0x02000     /* user access to IP hdr for SOCK_RAW */
#define     SO_NOSLOWSTART 0x04000     /* suppress slowstart on this socket */
#define     SO_FULLMSS     0x08000     /* force packets to all be MAX size */

/* for compatability with second-rate stacks: */
#define SO_EXPEDITE     SO_NOSLOWSTART
#define SO_THROUGHPUT   SO_FULLMSS

/*
 * Additional options, not kept in so_options.
 */
#define  SO_SNDBUF      0x1001      /* send buffer size */
#define  SO_RCVBUF      0x1002      /* receive buffer size */
#define  SO_SNDLOWAT    0x1003      /* send low-water mark */
#define  SO_RCVLOWAT    0x1004      /* receive low-water mark */
#define  SO_SNDTIMEO    0x1005      /* send timeout */
#define  SO_RCVTIMEO    0x1006      /* receive timeout */
#define  SO_ERROR       0x1007      /* get error status and clear */
#define  SO_TYPE        0x1008      /* get socket type */
#define  SO_HOPCNT      0x1009      /* Hop count to get to dst   */
#define  SO_MAXMSG      0x1010      /* get TCP_MSS (max segment size) */

/* ...And some netport additions to setsockopt: */
#define  SO_RXDATA      0x1011      /* get count of bytes in sb_rcv */
#define  SO_TXDATA      0x1012      /* get count of bytes in sb_snd */
#define  SO_MYADDR      0x1013      /* return my IP address */
#define  SO_NBIO        0x1014      /* set socket into NON-blocking mode */
#define  SO_BIO         0x1015      /* set socket into blocking mode */
#define  SO_NONBLOCK    0x1016      /* set/get blocking mode via optval param */
#define  SO_CALLBACK    0x1017      /* set/get zero_copy callback routine */

/*
 * TCP User-settable options (used with setsockopt).
 * TCP-specific socket options use the 0x2000 number space.
 */

#define  TCP_ACKDELAYTIME 0x2001    /* Set time for delayed acks */
#define  TCP_NOACKDELAY   0x2002    /* suppress delayed ACKs */
#define  TCP_MAXSEG       0x2003    /* set maximum segment size */
#define  TCP_NODELAY      0x2004    /* Disable Nagle Algorithm */


/*
 * Structure used for manipulating linger option.
 */
struct   linger {
   int   l_onoff;    /* option on/off */
   int   l_linger;   /* linger time */
};


/*
 * Structure used by kernel to store most
 * addresses.
 */
struct sockaddr {
   U16     sa_family;     /* address family */
   char     sa_data[14];      /* up to 14 bytes of direct address */
};


/* Berkeley style "Internet address" */

struct in_addr {
   U32  s_addr;
};

#define  INADDR_ANY     0L

/* Berkeley style "Socket address" */
struct sockaddr_in {
  U16      sin_family;
  U16      sin_port;
  struct   in_addr  sin_addr;
  char     sin_zero[8];
};




/*
 * Structure used by kernel to pass protocol
 * information in raw sockets.
 */
struct sockproto {
   U16     sp_family;     /* address family */
   U16     sp_protocol;   /* protocol */
};

/* Support for Large initial congestion window */
#ifdef TCP_BIGCWND
extern   int      use_default_cwnd;    /* Flag to use this on all sockets */
extern   U32   default_cwnd;        /* initial cwnd value to use */
#endif   /* TCP_BIGCWND */

/*
 * Protocol families, same as address families for now.
 */
#define  PF_UNSPEC   AF_UNSPEC
#define  PF_UNIX     AF_UNIX
#define  PF_INET     AF_INET
#define  PF_IMPLINK  AF_IMPLINK
#define  PF_PUP      AF_PUP
#define  PF_CHAOS    AF_CHAOS
#define  PF_NS       AF_NS
#define  PF_NBS      AF_NBS
#define  PF_ECMA     AF_ECMA
#define  PF_DATAKIT  AF_DATAKIT
#define  PF_CCITT    AF_CCITT
#define  PF_SNA      AF_SNA
#define  PF_DECnet   AF_DECnet
#define  PF_DLI      AF_DLI
#define  PF_LAT      AF_LAT
#define  PF_HYLINK   AF_HYLINK
#define  PF_APPLETALK   AF_APPLETALK


/*
 * Maximum queue length specifiable by listen.
 */
#define     SOMAXCONN   5

#define     MSG_PEEK       0x2      /* peek at incoming message */
#define     MSG_DONTROUTE  0x4      /* send without using routing tables */
#define     MSG_NEWPIPE    0x8      /* New pipe for recvfrom call   */
#define     MSG_EOR        0x10     /* data completes record */
#define     MSG_DONTWAIT   0x20     /* this message should be nonblocking */

/* utility functions defined in misclib\parseip.c */
int inet46_addr(char *str, struct sockaddr *address);

long  t_socket (int, int, int);
int   t_bind (long, struct sockaddr *, int);
int   t_listen (long, int);
long  t_accept (long, struct sockaddr *, int *);
int   t_connect (long, struct sockaddr *, int);
int   t_getpeername (long, struct sockaddr *, int * addrlen);
int   t_getsockname (long, struct sockaddr *, int * addrlen);
int   t_setsockopt (long sock, int level, int op, void * data, int dlen);
int   t_getsockopt (long sock, int level, int op, void * data, int dlen);
int   t_recv (long, char *, int, int);
int   t_send (long, const char *, int, int);
int   t_recvfrom (long s, char * buf, int len, int flags, struct sockaddr *, int*);
int   t_sendto (long s, const char * buf, int len, int flags, struct sockaddr *, int);
int   t_shutdown (long, int);
int   t_socketclose (long);
int   t_errno(long s);

char * so_perror(int);  /* return an error string for a socket error */

//
// Map plain BSD socket routine names to Interniche t_" names.
//
#define  socket(x,y,z)           t_socket(x,y,z)
#define  bind(s,a,l)             t_bind(s,a,l)
#define  connect(s,a,l)          t_connect(s,a,l)
#define  listen(s,c)             t_listen(s,c)
#define  send(s, b, l, f)        t_send(s, b, l, f)
#define  recv(s, b, l, f)        t_recv(s, b, l, f)
#define  accept(s,a,l)           t_accept(s, a, l)
#define  sendto(s,b,l,f,a,x)     t_sendto(s,b,l,f,a,x)
#define  recvfrom(s,b,l,f,a,x)   t_recvfrom(s,b,l,f,a,x)
#define  socketclose(s)          t_socketclose(s)
#define  closesocket(s)          t_socketclose(s)
#define  setsockopt(s,l,o,d,x)   t_setsockopt(s,l,o,d,x)
#define  getsockopt(s,l,o,d,x)   t_getsockopt(s,l,o,d,x)
#define  shutdown(s,how)         t_shutdown(s,how)
#define  select(i,o,e,tv)        t_select(i,o,e,tv)
#define  getpeername(s,a,al)     t_getpeername(s,a,al)
#define  getsockname(s,a,al)     t_getsockname(s,a,al)

struct hostent *  gethostbyname (char * name);



#define  SOCKTYPE    long     /* preferred generic socket type */

#define  SYS_SOCKETNULL -1    /* error return from sys_socket. */
#define  INVALID_SOCKET -1    /* WINsock-ish synonym for SYS_SOCKETNULL */
#define  SOCKET_ERROR   -1    /* error return from send(), sendto(), et.al. */
#define  SOL_SOCKET     -1    /* compatability parm for set/get sockopt */

#define  SO_SELECT      1  /* support select() call */


#ifdef SO_SELECT
/* define the size of the sockets arrays passed to select(). On UNIX
 * and winsock this is usually 64, but most embedded systems don't
 * need more than 1 or 2, and can't always afford to waste the space.
 * NOTE: These determine the size of set_fd structs, which are often
 */
#ifndef FD_SETSIZE   /* let h_h files override */
#define  FD_SETSIZE     12
#endif   /* FD_SETSIZE */
#endif   /* SO_SELECT */




/* the definitions to support the select() function. These are about
 * as UNIX-like as we can make 'em on embedded code. They are also
 * fairly compatable with WinSock's select() definitions.
 */

typedef struct IP_FD_SET   /* the select socket array manager */
{
   unsigned fd_count;               /* how many are SET? */
   long     fd_array[FD_SETSIZE];   /* an array of SOCKETs */
} IP_fd_set;

/* our select call - note the traditional "width" parameter is absent */
int t_select(IP_fd_set * in, IP_fd_set * out, IP_fd_set * ev, long tmo_seconds);

//
// Select-related functions are calls (not macros) to save space
//
void  IP_FD_CLR  (long so, IP_fd_set * set);
void  IP_FD_SET  (long so, IP_fd_set * set);
int   IP_FD_ISSET(long so, IP_fd_set * set);
// and one actual macro:
#define  IP_FD_ZERO(set)   (((IP_fd_set *)(set))->fd_count=0)

#if defined(__cplusplus)
  }
#endif

#endif   /* SOCKET_H */

/* end of file socket.h */


