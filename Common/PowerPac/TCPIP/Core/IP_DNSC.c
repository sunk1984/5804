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
File    : IP_DNSC.c
Purpose : DNS Client code
---------------------------END-OF-HEADER------------------------------
*/

#include "IP_Int.h"
#include "IP_Q.h"





/* set defaults of DNS variables, may override in ipport.h */
#ifndef   MAXDNSNAME
  #define MAXDNSNAME     256   /* max length of name including domain */
#endif

#ifndef   MAXDNSUDP
  #define MAXDNSUDP      512   /* MAX allowable UDP size */
#endif

#ifndef MAXDNSADDRS
#define  MAXDNSADDRS    10    /* MAX IP addresses to return via gethostbyname() */
#endif

#ifndef MAXDNSALIAS
#define  MAXDNSALIAS    2     /* MAX number of alias names */
#endif

#define  DNS_QUERY       0    /* Op code for query */
#define  DNS_UPDT        5    /* Op code for UPDATE request  */

/* DNS Record types */
#define  DNS_TYPE_QUERY    0     /* Type value for question record */
#define  DNS_TYPE_IPADDR   1     /* Type value for IPv4 address */
#define  DNS_TYPE_AUTHNS   2     /* Authoritative name server */
#define  DNS_TYPE_ALIAS    5     /* Alias for queried name */
#define  DNS_TYPE_AAAA     0x1c  /* IPv6 address ("AAAA" record) */


/* basic internal structure of a client DNS entry. This one structure does double
 * duty as a request manager and database entry.
 */

struct dns_querys {
   struct dns_querys * next;
   U32      send_time;     /* ctick when last request was sent/received */
   U32      expire_time;   // Expiration in Ticks
   U16      tries;         /* retry count */
   U16      lport;         /* local (client) UDP port, for verification */
   U16      id;            /* ID of request, 0 == unused entry. */
   int      replies;       /* number of replys to current request */
   int      ipaddrs;       /* count of entries in ipaddr_list[] */
   ip_addr  ipaddr_list[MAXDNSADDRS];  /* IP addresses (net endian) */
   char *   addrptrs[MAXDNSADDRS];  /* pointers, for hostent.h_addr_list */
   int      err;                    /* last IP_ERR_ error if, if any */
   int      rcode;                  /* last response code if replys > 1 */
   char     dns_names[MAXDNSNAME];  /* buffer of names (usually w/domain) */
   ip_addr  auths_ip;               /* IPv4 addresses of 1st auth server */
   char *   alist[MAXDNSALIAS+1];   /* alias list, points into dns_names */

   /* Most DNS queries need a hostent structure to return the data to
    * the calling application; so we embed the hostent structure inside
    * the query structure - one less alloced buffer to keep track of.
    */
   struct hostent he;               /* for return from gethostbyname() */
   char     type;                   /* type of original query */
};


#ifdef   DNS_CLIENT_UPDT
extern   char     soa_mname[MAXDNSNAME];
#endif    /* DNS_CLIENT_UPDT */

/* header format of a DNS packet over UDP */
struct dns_hdr {
   U16  id;         /* 16 bit unique query ID */
   U16  flags;      /* various bit fields, see below */
   U16  qdcount;    /* entries in the question field */
   U16  ancount;    /* resource records in the answer field */
   U16  nscount;    /* name server resource records */
   U16  arcount;    /* resource records in the additional records */
};

#define  DNS_PORT    53    /* DNS reserved port on UDP */

/* DNS header flags field defines */
#define  DNSF_QR     0x8000   /* query (0), or response (1) */
#define  DNSF_OPMASK 0x7800   /* 4 bit opcode kinds of query, 0==standard */
#define  DNSF_AA     0x0400   /* set if Authoritive Answers */
#define  DNSF_TC     0x0200   /* set if truncated message */
#define  DNSF_RD     0x0100   /* Recursion Desired bit */
#define  DNSF_RA     0x0080   /* Recursion Allowed bit */
#define  DNSF_Z      0x0070   /* 3 reserved bits, must be zero */
#define  DNSF_RCMASK 0x000F   /* Response Code mask */

/* Reponse Code values: */
#define  DNSRC_OK       0     /* good response */
#define  DNSRC_EFORMAT  1     /* Format error */
#define  DNSRC_ESERVER  2     /* Server Error */
#define  DNSRC_ENAME    3     /* Name error */
#define  DNSRC_EIMP     4     /* Not Implemented on server */
#define  DNSRC_EREFUSE  5     /* Server refused operation */
#define  DNSRC_UNSET    0xFF  /* No reponse yet (used only in dns_querys struct) */

#ifdef DNS_CLIENT_UPDT

/* Error codes used within Dynamic DNS Update operation */
#define  DNSRC_EDOMAIN  6     /* Some name that ought not to exist, exists */
#define  DNSRC_ERRSET   7     /* Some RRset that ought not to exist, exists*/
#define  DNSRC_ENRRSET  8     /* Some RRset that ought to exist, does not */
#define  DNSRC_NOTAUTH  9     /* Server is not authoritative for the zone */
                              /* named in the zone section */
#define  DNSRC_NOTZONE  10    /* A name used within the Update section */
                              /* is not within the zone denoted        */

#endif /* DNS_CLIENT_UPDT */

/* DNS client external entry points: */
int   dns_query(char * name, ip_addr * ip);     /* start a DNS query */
int   dns_lookup(ip_addr * ip, char * name);    /* check query status */
void  dnsc_check(void); /* spin once a second to drive retrys & timeouts */

/* flags for in_reshost(); */
#define  RH_VERBOSE     0x01  /* do informational printfs   */
#define  RH_BLOCK       0x02  /* block  */
int   in_reshost(char * host, ip_addr * address, int flags);

/*********************************************************************
*
*       Static data
*
**********************************************************************
*/
static UDPCONN _hConnection;
static U32     _TTLMax = 600;


struct dns_querys * dns_qs;      /* List for querie/database records. */

U16  dnsids   =  0x1234;  /* seed for unique request IDs */

/* DNS client statistics: */
U32      dnsc_errors = 0;      /* protocol/implementation runtime errors */
U32      dnsc_requests = 0;    /* requests sent */
static U32 _ReplyCnt = 0;     /* replys received */
U32      dnsc_good = 0;        /* usable replys received */
U32      dnsc_tmos = 0;        /* timeouts */
U32      dnsc_retry = 0;       /* total retries */

#ifdef DNS_CLIENT_UPDT
U32      dnsc_updates = 0;     /* Dynamic DNS Updates sent */
#endif /* DNS_CLIENT_UPDT */

unsigned   dnsc_active;      /* pending requests, can be used as a flag to spin dnsc_check() task */

/* retry info, can be overwritten by application code */
unsigned dns_firsttry = 4; /* time to first retry, in seconds */
unsigned dns_trys = 5;     /* max number of retrys */

#ifdef  DNS_CLIENT_UPDT
char    soa_mname[MAXDNSNAME];

/* internal routines */
static struct hostent* getsoa(char *);
#endif  /* DNS_CLIENT_UPDT */



static QUEUE _FreeQ;

/*********************************************************************
*
*       _TryAlloc
*
*  Function description
*    Allocate storage space.
*    It is either fetched from the free queue or allocated.
*/
static struct dns_querys * _TryAlloc(void) {
  return (struct dns_querys *)IP_TryAllocWithQ(&_FreeQ, sizeof(struct dns_querys));
}

/*********************************************************************
*
*       _Free
*
*  Function description
*    Frees storage space.
*/
static void _Free(struct dns_querys * p) {
  IP_Q_Add(&_FreeQ, p);
}

/*********************************************************************
*
*       _FindDNSServer
*/
static U32 _FindDNSServer(void) {
  return IP_aIFace[0].aDNSServer[0];
}

/*********************************************************************
*
*       _ParseIPAddr
*
* Looks for an IP address in
* string_in buffer, makes an IP address (in big endian) in long_out.
*
* ipout      pointer to IP address to set
* sbits      default subnet bit number
* stringin  buffer with ascii to parse
*/
static int _ParseIPAddr(ip_addr * ipout,  unsigned *  sbits, char *   stringin) {
  char *   cp;
  int   dots  =  0; /* periods imbedded in input string */
  int   number;
  union
  {
    U8    c[4];
    U32   l;
  } retval;

  cp = stringin;
  while (*cp) {
   if (*cp > '9' || *cp < '.' || *cp == '/') {
     return 1;     // return("all chars must be digits (0-9) or dots (.)");
   }
   if (*cp == '.') {
     dots++;
   }
   cp++;
  }

  if ( dots < 1 || dots > 3 ) {
    return 1;   // return("string must contain 1 - 3 dots (.)");
  }

  cp = stringin;
  if ((number = atoi(cp)) > 255) {  /* set net number */
    return 1;   // Error
  }

  retval.c[0] = (U8)number;

  while (*cp != '.') {
   cp++; /* find dot (end of number) */
  }
  cp++;             /* point past dot */

  if (dots == 1 || dots == 2) {
    retval.c[1] = 0;
  } else {
    number = atoi(cp);
    while (*cp != '.') {
      cp++; /* find dot (end of number) */
    }
    cp++;             /* point past dot */
    if (number > 255) {
      return 1;    // (toobig);
    }
    retval.c[1] = (U8)number;
  }

  if (dots == 1) {
    retval.c[2] = 0;
  } else {
    number = atoi(cp);
    while (*cp != '.')cp++; /* find dot (end of number) */
       cp++;             /* point past dot */
       if (number > 255) {
         return 1;
       }
       retval.c[2] = (U8)number;
  }

  if ((number = atoi(cp)) > 255) {
    return 1;
  }
  retval.c[3] = (U8)number;

  if (retval.c[0] < 128) {
    *sbits = 8;
  } else if (retval.c[0] < 192) {
    *sbits = 16;
  } else {
    *sbits = 24;
  }
  *ipout = retval.l;      /* everything went OK, return number */
  return 0;               /* return OK code (no error string) */
}




/*********************************************************************
*
*       dnc_lookup()
*
* Check cache for a pending or completed DNS client
* request. Passed entry index is filled in if DNS query was resolved
* without error, else this returns non-zero IP_ERR_ error code.
*
*
* PARAM1: char * name
* PARAM2: int * cacheindex
*
* RETURNS: IP_ERR_SEND_PENDING if request is still on the net, or IP_ERR_TIMEOUT
* id request timed out (or was never made), or 0 (SUCCESS) if passed
* name found in cache, or IP_ERR_PARAM if
*/
static int dnc_lookup(char * name, struct dns_querys ** cacheentry) {
  struct dns_querys * dns_entry;
  int      alias;

  /* look through the cache for the passed name */
  for (dns_entry = dns_qs; dns_entry; dns_entry = dns_entry->next) {
    if (strcmp(dns_entry->dns_names, name) == 0) {
      break;
    }

    if ((dns_entry->he.h_name) && (strcmp(dns_entry->he.h_name, name) == 0)) {
      break;
    }
    for (alias = 0; alias < MAXDNSALIAS; alias++) {
      if ((dns_entry->alist[alias]) && (strcmp(dns_entry->alist[alias], name) == 0)) {
        goto found_name;
      }
    }
  }

found_name:
  /* if not found, return error */
  if (dns_entry == NULL) {
    return IP_ERR_PARAM;
  }

  /* else, prepare to return entry index */
  if (cacheentry != NULL) {
    *cacheentry = dns_entry;
  }

  /* if completed request, return success */
  if (dns_entry->rcode != DNSRC_UNSET) {
    return 0;
  }

  /* incomplete request -- return pending or timed-out status */
  if (dns_entry->tries < dns_trys) { /* still trying? */
    return IP_ERR_SEND_PENDING;
  }
  return IP_ERR_TIMEOUT;
}

/*********************************************************************
*
*       dnc_copyout()
*
* Copy a domain name from user "dot" text format to
* DNS header format. dest buffer is assumed to be large enough,
* which means at least as big as src string.
*
* RETURNS:  Returns length of string copied.
*/
static int dnc_copyout(char * dest, char * src) {
   int   namelen; /* length of name segment */
   char *   cp;   /* pointer to dest string we're building */
   char *   fld, *   next; /* pointers into src */

   cp = dest;
   next = src; /* will be assigned to "this" on first pass */

   while (next) {   /* 1 loop per field in dot-delimited string */
      fld = next;
      next = strchr(fld, '.');   /* find next dot in string */
      if (next) {  /* if not last field... */
         namelen = next - fld;   /* length of this field */
         next++;  /* bump next pointer past dot */
      } else {  /* no dot means end of string */
         namelen = strlen(fld);
      }

      *cp++ = (char)(namelen & 0xFF);  /* put length in dest buffer */
      IP_MEMCPY(cp, fld, namelen);     /* follow with string data */
      cp += namelen;    /* point past name to next length byte */
   }
   *cp++ = 0;  /* null terminate string */
   return(cp - dest);
}



/* FUNCTION: dnc_sendreq()
 *
 * dnc_sendreq() - Sends out a DNS request packet based on the query
 * info in dns_entry. This is intended to be called from
 * dns_query() or dns_check().
 *
 *
 * PARAM1: int entry
 *
 * RETURNS: Returns 0 if OK, or one of the ENP error codes.
 */

static int dnc_sendreq(struct dns_querys * dns_entry) {
   IP_PACKET * pkt;
   struct dns_hdr *  dns;  /* outgoing dns header */
   char *   cp;   /* scratch pointer for building question field */
   int   server;  /* index of server to use */


   _FindDNSServer();
   /* figure out which server to try */
   for (server = 0; server < IP_MAX_DNS_SERVERS; server++) {
     if (IP_aIFace[0].aDNSServer[server] == 0L) {
       break;
     }
   }
   if (server == 0) { /* no servers set? */
     IP_WARN((IP_MTYPE_DNS, "DNS: No name server."));
      return IP_ERR_LOGIC;
   }
   server = dns_entry->tries % server;

   /* allocate packet for DNS request */
   if ((pkt = IP_UDP_Alloc(MAXDNSUDP)) == NULL) {
      return IP_ERR_RESOURCE;
   }

   /* fill in DNS header. Most fields are cleared to 0s */
   dns = (struct dns_hdr *)pkt->pData;
   IP_MEMSET(dns, 0, sizeof(struct dns_hdr));
   dns->id = dns_entry->id;

#ifdef DNS_CLIENT_UPDT
   /* If this is an UPDATE packet, format the DNS packet differently */

   if (dns_entry->type == DNS_UPDT) {
      dns->qdcount = htons(1);      /* Set zone count to 1 */
      dns->nscount = htons(1);      /* Set update count to 1 */
      dns->flags = htons(0x2800);   /* Set opcode field to 5 */

      /* format zone name into UPDATE packet */
      cp = (char*)(dns+1);      /* point at next byte past header */
      cp += dnc_copyout(cp, dns_entry->he.h_z_name);

      /* finish off zone section. We write these two 16 bit words a
       * byte at a time since cp may be on an odd address and some
       * machines
       */
      *cp++ = 0;  /* high byte of type */
      *cp++ = 6;  /* type 6 = soa */
      *cp++ = 0;  /* high byte of class */
      *cp++ = 1;  /* class 1 == internet */
      cp += dnc_copyout(cp, dns_entry->dns_names);

      /* Put in NAME and TYPE */
      *cp++ = 0;  /* high byte of type */

      /* If ttl value is 0, this is a delete operation. Set type to ANY */
      if (dns_entry->he.h_ttl == 0)
         *cp++ = (unsigned char)255;
      else
         *cp++ = 1;  /* type 1 == host address, type 6 = soa */
      *cp++ = 0;     /* high byte of class */
      *cp++ = 1;     /* class 1 == internet */

      /* Put in TTL value */
      *cp++ = (unsigned char)(dns_entry->he.h_ttl >> 24);
      *cp++ = (unsigned char)(dns_entry->he.h_ttl >> 16);
      *cp++ = (unsigned char)(dns_entry->he.h_ttl >> 8);
      *cp++ = (unsigned char)dns_entry->he.h_ttl;

      /* Put in RDLENGTH which is length of ip address ie 4 */
      *cp++ = 0;  /* low byte of length */

      /* If ttl value is 0, this is a delete operation. Set RDLENGTH to 0 */
      if (dns_entry->he.h_ttl == 0)
         *cp++ = 0;
      else
         *cp++ = 4;
      /* Put in IP address */
      MEMCPY(cp, &dns_entry->he.h_add_ipaddr, 4);
      cp += 4;
      dnsc_updates++;
   } else {
#endif  /* DNS_CLIENT_UPDT */


      dns->qdcount = htons(1);      /* 1 question */
      dns->flags = htons(DNSF_RD);  /* Recursion Desired */

      /* format name into question field after header */
      cp = (char*)(dns + 1);  /* point at next byte past header */
      cp += dnc_copyout(cp, dns_entry->dns_names);

      /* finish off question field. We write these two 16 bit words a
       * byte at a time since cp may be on an odd address and some
       * machines
       */
      *cp++ = 0;  /* high byte of type */
      *cp++ = dns_entry->type;  /* type 1 == host address, type 6 = soa */
      *cp++ = 0;  /* high byte of class */
      *cp++ = 1;  /* class 1 == internet */

#ifdef DNS_CLIENT_UPDT
   }
#endif /* DNS_CLIENT_UPDT */
   pkt->NumBytes = cp - (char*)dns;     /* length of packet */
   dns_entry->send_time = IP_OS_GetTime32();   /* note time we send packet */
   return IP_UDP_SendAndFree(0, IP_aIFace[0].aDNSServer[server], DNS_PORT, dns_entry->lport, pkt);
}



/*********************************************************************
*
*       dnc_new
*
* Tries to make an empty entry use in the cache.
*/
static struct dns_querys * dnc_new(void) {
  struct dns_querys * dns_entry;

  dns_entry = _TryAlloc();  /* get new entry */
  if (dns_entry) {
    /* set respose code to an illegal value. Response will overwrite */
    dns_entry->rcode = DNSRC_UNSET;
    /* set legacy hostent alias pointer to dns_entry alias list */
    dns_entry->he.h_aliases = &dns_entry->alist[0];
    /* Put new entry in the DNS database/request list */
    LOCK_NET();
    dns_entry->next = dns_qs;
    dns_qs = dns_entry;
    UNLOCK_NET();
  }
  return dns_entry;
}




/* FUNCTION: _dns_query_type()
 *
 * New (for v2.0) version of dns_query()
 * This creates the control structures and calls dnc_sendreq() to
 * send the DNS query packet. Caller should
 * poll dns_lookup for results.
 *
 *
 * PARAM1: char * name - IN - host name to look up
 * PARAM2: char type - IN - type of query (V4, V6, SERVER, etc.)
 * PARAM3: dns_querys ** dnsptr - OUT - dns entry
 *
 * RETURNS: Returns 0 if IP address was filled in
 * from cache, IP_ERR_SEND_PENDING if query was launched OK, or one of
 * the ENP error codes.
 */

static int _dns_query_type(char * name, char type, struct dns_querys ** dns_ptr) {
  struct dns_querys *  dns_entry;
  int   e;

  /* see if we already have an entry for this name in the cache */
  e = dnc_lookup(name, dns_ptr);
  if (e == 0) {  /* no error */
    return 0;
  } else if (e == IP_ERR_SEND_PENDING) {  /* waiting for previous query */
    return e;
  }
  /* else find a free entry so we can start a query */
  dns_entry = dnc_new();
  if (dns_entry == NULL) {
    return IP_ERR_RESOURCE;
  }

  /* prepare entry for use and copy in name. The name for the query will
  * be replaced by the reply, and other strings (alias, server name) may
  * be added to the buffer if room permits.
  */
  strncpy(dns_entry->dns_names, name, MAXDNSNAME-1);  /* copy in name */

  dns_entry->tries = 0;      /* no retries yet */
  dns_entry->id = dnsids++;  /* set ID for this transaction */
  dns_entry->ipaddr_list[0] = 0L;  /* non-zero when it succeeds */
  if (dnsids == 0) {              /* don't allow ID of 0 */
    dnsids++;
  }
  /* get UDP port for packet, keep for ID */
  dns_entry->lport = IP_UDP_FindFreePort();
  dnsc_active++;       /* maintain global dns pending count */
  dnsc_requests++;     /* count this request */

  dns_entry->type = type;    /* set type of DNS query */
  *dns_ptr = dns_entry;      /* return DNS entry */

  e = dnc_sendreq(dns_entry);
  if (e == 0) {/* first packet sent OK */
    return IP_ERR_SEND_PENDING;
  }
  return e;
}


/*********************************************************************
*
*       dns_query()
*
* Starts the process of sending out a DNS request for
* a domain name. This is hardwired for the basic question "what is
* the IP address of named host?". It creates the control structures
* and calls dnc_sendreq() to send the actual packet. Caller should
* poll dns_lookup for results.
*
* Old version, for backward API compatibility. This is implemented
* as a wrapper for the new function
*
*
* RETURNS: Returns 0 if IP address was filled in
* from cache, IP_ERR_SEND_PENDING if query was launched OK, or one of
* the ENP error codes.
*/
int dns_query(char * name, ip_addr * ip_ptr) {
  int      err;
  struct dns_querys * dns_entry;

  /* Do query for an "A" record (DNS_TYPE_IPADDR) */
  err = _dns_query_type(name, DNS_TYPE_IPADDR, &dns_entry);
  if (!err) {
    *ip_ptr = dns_entry->ipaddr_list[0];
  }

  return err;
}



/*********************************************************************
*
*       dnc_copyin()
*
* dnc_copyin() - the inverse of copyout above - it copies a dns
* format domain name to "dot" formatting.
*
*
* RETURNS: Returns length of string copied, 0 if error.
*/
static int dnc_copyin(char * dest, char * src, struct dns_hdr * dns) {
  U16   namelen; /* length of name segment */
  char *   cp;   /* pointer to dest string we're building */
  char *   fld, *   next; /* pointers into src */
  int   donelen;    /* number of bytes moved */
  U16 offset;

  cp = dest;
  next = src; /* will be assigned to "this" on first pass */
  donelen = 0;

  while (next) {  /* 1 loop per field in dot-delimited string */
    fld = next;
    namelen = *fld++;
    if (namelen == 0) {
      break;   /* done */
    }
    if ((namelen & 0xC0) == 0xC0) {
       fld--;
       offset = (U16)*fld; /* get first byte of offset code */
       fld++;
       offset &= 0x3f;             /* mask our high two bits */
       offset <<= 8;               /* make it high byte of word */
       offset += (U16)*fld; /* add low byte of word */
       fld = offset + (char *)dns;  /* ptr into domain name */
       namelen = *fld++;
    }
    if (namelen + donelen > MAXDNSNAME) /* check for overflow */
       return 0;   /* error */
    IP_MEMCPY(cp, fld, namelen);
    donelen += (namelen+1); /* allow for dot/len byte */
    cp += namelen;
    *cp++ = '.';
    next = fld + namelen;
  }
  *(cp-1) = 0;   /* null terminate string (over last dot) */
  return donelen + 1;  /* include null in count */
}



/*********************************************************************
*
*       dnc_del()
*
* Delete a DNS entry
*
*/
static void dnc_del(struct dns_querys * entry) {
   struct dns_querys * tmp;
   struct dns_querys * last;

   /* find passed dns entrery in list */
   last = NULL;
   for(tmp = dns_qs; tmp; tmp = tmp->next) {
     if(tmp == entry) { /* found entry in list */
       if (last) {         /* unlink */
         last->next = tmp->next;
       } else {
         dns_qs = tmp->next;
       }
       break;
     }
     last = tmp;
   }
   _Free(entry);  /* free the memory */
   dnsc_active--;    /* one less active entry */
}

/*********************************************************************
*
*       dnc_ageout()
*
* See if we can age out any of the DNS entries.
*/
static void dnc_ageout(void) {
  I32 Timeout;
  struct dns_querys * dns_entry;

  LOCK_NET();

  /* See if we can expire any old entries */
  for(dns_entry = dns_qs; dns_entry; dns_entry = dns_entry->next) {
    /* don't use entries that haven't resolved yet */
   if (dns_entry->ipaddrs == 0) {
     continue;
   }

    /* If entry has expired then delete it. */
   Timeout = dns_entry->expire_time - IP_OS_GetTime32();
   if (Timeout <= 0) {
      dnc_del(dns_entry);
    }
  }
  UNLOCK_NET();
}



/*********************************************************************
*
*       _IP_DNSC_Timer
*
*  Function description
*    Perform regular maintenance. Typically called once per second.
*/
static void _IP_DNSC_Timer(void) {
  struct dns_querys * dns_entry;
  int   trysec;  /* seconds to wait for next try */

  dnc_ageout();     /* age out expired entries */

  for(dns_entry = dns_qs; dns_entry; dns_entry = dns_entry->next) {
    if (dns_entry->id == 0) /* skip non-active request entrys */
       continue;

    /* If we already have a reply we like, don't send */
    if(dns_entry->rcode == DNSRC_OK)
       continue;

    /* If it's a name error then punt the request */
    if (dns_entry->rcode == DNSRC_ENAME) {
       /* only if it's over 10 seconds old */
      if(((IP_OS_GetTime32() - dns_entry->send_time)/TPS) > 10) {
        goto timeout;
      }
    }
    /* active request, see if it's time to retry */
    trysec = dns_firsttry << dns_entry->tries;
    if ((dns_entry->send_time + (TPS*(unsigned long)trysec)) < IP_OS_GetTime32())  {
      if (dns_entry->tries >= dns_trys) {  /* retried out */
timeout:
          dnc_del(dns_entry);
          dnsc_tmos++;      /* count timeouts */
          /* After a timeout we return because the list is now altered.
           * We'll process the rest of it on the next time tick.
           */
          return;
       }
       dnsc_retry++;           /* count total retries */
       dns_entry->tries++;  /* count entry retries */
       dnc_sendreq(dns_entry);
    }
  }
}


/*********************************************************************
*
*       badreply()
*
* Per-port handler for less than ideal DNS replys.
*
* RETURNS: Returns ENP code if problem should kill transaction, else 0
*/
static int badreply(struct dns_hdr * dns, char * text) {
   dnsc_errors++;
//   dprintf("DNS error: %s; flags %x\n", text, htons(dns->flags));
   /* don't kill request, let it retry with another server */
   return 0;
}

static char * getshort(char * cp, U16 * val) {
   *val = (U16)(*cp++ << 8);
   *val += (U16)(*cp++);
   return (cp);
}

static char * getoffset(char * cp, char * dns, U16 * offsetp) {
   U16  offset;

   /* bump past name field in answer, keeping offset to text */
   if ((*cp & 0xC0) == 0xC0)  /* is it an offset? */
   {
      offset = (U16)(*cp++); /* get first byte of offset code */
      offset &= 0x3f;   /* mask our high two bits */
      offset <<= 8;     /* make it high byte of word */
      offset += (U16)(*cp++);   /* add low byte of word */
   }
   else  /* text for name is right here */
   {
      offset = (U16)(cp - dns);   /* save offset */
      while (*cp++ != 0);  /* scan past name */
   }
   *offsetp = offset;
   return cp;
}

static void dnc_setaddr(struct dns_querys * dns_entry, U16 type, char * cp) {
   int   addrx;      /* index into address lists */

   /* save reply IP addresses in array of IP addresses so long
    * as there is room for them.
    */
#ifdef IP_V4
   if(type == DNS_TYPE_IPADDR)
   {
      if (dns_entry->ipaddrs < MAXDNSADDRS)
      {
         addrx = dns_entry->ipaddrs++;      /* set index */
         IP_MEMCPY(&dns_entry->ipaddr_list[addrx], cp, 4);
      }
      return;
   }
#endif   /* IP_V4 */


}


/* FUNCTION: dnc_savename()
 *
 * Save a passed name in the passed dns_query structure. name is given
 * via an offset into the dns packet.
 *
 *
 * PARAM1: dns_query structure to add name to
 * PARAM2: pointer to dns header
 * PARAM3: offset into dns header for name info
 * PARAM3: TRUE if name is an alias
 *
 * RETURNS: void
 */

static void dnc_savename(struct dns_querys *  dns_entry, struct dns_hdr * dns, int offset, int aliasflag) {
  char * cp;     /* pointer to dns_names[] name buffer */

  /* find next available space in name buffer */
  cp = dns_entry->dns_names;
  while (*cp) {
    if (*cp) {
     cp += (strlen(cp) + 1); /* end of name (if any) */
    }

    /* check for buffer overflow */
    if (cp >= &dns_entry->dns_names[MAXDNSNAME])  {
      IP_WARN((IP_MTYPE_DNS, "DNS: Out of buffer space for names"));
      return;
    }
    if (*cp == 0) {
      break;
    }
  }

  dnc_copyin(cp, offset + (char*)(dns), dns);      /* copy dns-format name into buffer as text */

  /* Set pointer(s) in dns structures to new name */
  if(aliasflag) {
    int alias;     /* alias name index */
    /* Set the next alias pointer. First we have to find out
     * how many aliases are already in dns_entry.
     */
    for (alias = 0; alias < MAXDNSALIAS; alias++) {
      if (dns_entry->alist[alias] == NULL) {
        break;
      }
    }
    if (alias >= MAXDNSALIAS) {
      return;
    }
    /* set alias pointer to alias name in dns_names[] buffer */
    dns_entry->alist[alias] = cp;
  } else {  /* set the name pointer(s) */
    dns_entry->he.h_name = cp;  /* The hostent name field always points to dns_name */
  }
  return;
}

/*********************************************************************
*
*       dns_upcall()
*
* dns_upcall() - called from the UDP layer whenever we receive a DNS
* packet (one addressed to port 53). p_nb_prot points to UDP data,
* which should be DNS header. Return 0 if OK, or ENP code.
*
*
* PARAM1: IP_PACKET * pkt
* PARAM2: U16 lport
*
* RETURNS:
*/
static int _upcall(IP_PACKET * pkt, U16 lport) {
   int      i;          /* record index */
   char *   cp;         /* scratch pointer for building question field */
   U16      flags;      /* dns->flags in local endian */
   U16      rcode;      /* response code */
   U16      queries;    /* number of question records in reply */
   U16      answers;    /* number of answer records in reply */
   U16      records;    /* total records in reply */
   U16      type;       /* various fields from the reply */
   U16      netclass;   /* class of net (1 == inet) */
   U32      ttl;        /* records time to live */
   U16      rdlength;   /* length of record data */
   U16      offset;     /* scratch offset to domain name text */
   U16      nameoffset = 0;   /* offset to name in query */
   U16      authoffset = 0;   /* offset to first auth server name */
   struct dns_hdr *  dns;     /* incoming dns header */
   struct dns_querys *  dns_entry;

   _ReplyCnt++;
   dns = (struct dns_hdr *)pkt->pData;

   dns_entry = dns_qs;
   do {
     if (dns_entry == NULL) {
       return IP_ERR_NOT_MINE;         /* do not free pkt here */
     }
     if ((dns_entry->id == dns->id) && (dns_entry->lport == lport)) {
       break;
     }
     dns_entry = dns_entry->next;
   } while (1);

   dns_entry->replies++;           /* count total replies */

   /* If we already have a reply we liked then punt this one */
   if (dns_entry->rcode == DNSRC_OK) {
     return 0;
   }

   flags = htons(dns->flags);      /* extract data fromheader */
   rcode = flags & DNSF_RCMASK;    /* Response code */
   queries = htons(dns->qdcount);    /* number of questions */
   answers = htons(dns->ancount);  /* number of answers */
   records = queries + answers + htons(dns->nscount) + htons(dns->arcount);

   /* check DNS flags for good response value */
   if (!(flags & DNSF_QR)) {
     return (badreply(dns, "not reply"));
   }

   /* looks like we got an answer to the query */
   dns_entry->rcode = rcode;

#ifdef DNS_CLIENT_UPDT
 {
   int opcode = (flags & DNSF_OPMASK) >> 11;   /* Op code */

   /* If UPDATE opcode set the ip address field to non zero and return */
   if (opcode == DNS_UPDT) {
      dns_entry->ipaddr_list[0] = 0xff;
      return 0;
   }
 }
#endif /* DNS_CLIENT_UPDT */

  if (rcode != DNSRC_OK) {
    return(badreply(dns, "bad response code"));
  }
  if (answers < 1) {
    return(badreply(dns, "no answers"));
  }
  cp = (char*)(dns+1);    /* start after DNS entry */

  /* Since we're going to store new DNS info, clean out the old info */
  IP_MEMSET(dns_entry->dns_names, 0, MAXDNSNAME);
  IP_MEMSET(dns_entry->alist, 0, sizeof(dns_entry->alist) );
  dns_entry->ipaddrs = 0;

  /* loop through remaining records - answers, auth, and RRs */
  for (i = 0; i < records; i++) {
    /* Get offset to record's name */
    cp = getoffset(cp, (char *)dns, &offset);

    /* get records type and class from packet bytes */
    cp = getshort(cp, &type);
    cp = getshort(cp, &netclass);
    if (netclass != 1)
       return badreply(dns, "class not internet");

    if(i < queries)      /* just skip of echos question record */
       continue;

    /* Get the Time and data-length */
    ttl = IP_LoadU32BE((U8 *)cp);       /* 4 byte time to live field */
    if (ttl > _TTLMax) {
      ttl = _TTLMax;
    }
    cp += 4;
    cp = getshort(cp, &rdlength); /* length of record data */

    switch(type) {
    case DNS_TYPE_IPADDR:   /* IPv4 address for a name */
      if ((type == DNS_TYPE_IPADDR) && (rdlength != 4)) {
        return(badreply(dns, "IPADDR len not 4"));
      }

      dnsc_good++;
      if (i < (queries + answers)) {  /* If record is an answer... */
        if (nameoffset == 0) {
           nameoffset = offset;                       /* save first answer name over request name */
           dnc_savename(dns_entry, dns, offset, 0);   /* save the name in the local DNS structure */
        }

        dnc_setaddr(dns_entry, type, cp);    /* save address */
        dns_entry->expire_time = IP_OS_GetTime32() + ttl * 1000;  /* save ttl */
      } else {  /* auth record or additional record */
         if (offset == nameoffset) {   /* another addr for query? */
           dnc_setaddr(dns_entry, type, cp);
         } else if(offset == authoffset) {/* auth server IP address */
           if (type == DNS_TYPE_IPADDR) {
              IP_MEMCPY(&dns_entry->auths_ip, cp, 4);
           }
         }
       }
       break;

    case DNS_TYPE_AUTHNS: /* authoritative name server */
       /* What we really want for the name server is it's IP address,
        * however this record only contains a name, ttl, etc. We save
        * the offset to the name, hoping that one of the additional
        * records will have the IP address matching this name.
        */
      if (authoffset == 0) {  /* only save first one */
        authoffset = cp - (char*)dns;
      }
      break;

    case DNS_TYPE_ALIAS:  /* alias */
       /* save name in dns rec as an alias */
       dnc_savename(dns_entry, dns, offset, 1);
       break;

    default:       /* unhandled record type, ignore it. */
       break;
    }
    cp += rdlength;   /* bump past trailing data to next record */
  }
  IP_OS_SignalItem(&_hConnection);    // Wakeup all tasks waiting for a DNS reply
  return 0;
}



/*********************************************************************
*
*       _OnRx
*
*  Function descrition
*    DNS client UDP callback. Called from stack
*    whenever we get a DNS reply. Returns 0 or error code.
*
* Return value
*  IP_RX_ERROR  if packet is invalid for some reason
*  IP_OK        if packet is valid
*/
static int _OnRx(IP_PACKET * pPacket, void * pContext) {
  U16 LPort;

  LPort = IP_UDP_GetLPort(pPacket);
  IP_LOG((IP_MTYPE_DNS, "DNS: Received packet."));
  _upcall(pPacket, LPort);
  return IP_OK;
}




/* FUNCTION: dns_lookup()
 *
 * dns_lookup() - check state of a DNS client request previously
 * launched with dns_query(). Passed IP address is filled in (in net
 * endian) if DNS query was resolved without error, else this clears
 * passed Ip address and returns non-zero IP_ERR_ code.
 *
 *
 * PARAM1: ip_addr * ip
 * PARAM2: char * name
 *
 * RETURNS: IP_ERR_SEND_PENDING if request is still on the net, or IP_ERR_TIMEOUT
 * if request timed out (or was never made), or 0 (SUCCESS) if passed IP
 * is filled in and everything is peachy, or other ENP errors if
 * detected.
 */

int dns_lookup(ip_addr * ip, char * name) {
   struct dns_querys * dns_entry;
   int      err;

   *ip = 0L;
   err = dnc_lookup(name, &dns_entry);
   if (err)
      return err;
   *ip = dns_entry->ipaddr_list[0];    /* return IP address */
   return 0;
}







/* static hostent structure for gethostbyname to return when it's
 * been passed a "name" which is just a dot notation IP address.
 */
static struct hostent dnc_hostent;
static ip_addr dnc_addr;         /* actual output in hostent */
static char *  dnc_addrptrs[2];  /* pointer array for hostent */

/*********************************************************************
*
*       gethostbyname()
*
* "standard" Unixey version of gethostbyname() returns pointer to a
* hostent structure if
* successful, NULL if not successful.
*
* PARAM1: char *name
*
* RETURNS:  pointer to a hostent structure if successful, NULL if not.
*/
struct hostent * gethostbyname(char * name) {
  int      err;
  int      i;
  unsigned long Timeout;
  unsigned int snbits;
  struct dns_querys * dns_entry;
  struct hostent * ho;

  //
  // Check if addr. is as parsable dot notation, such as 192.168.1.1
  //
  i = _ParseIPAddr(&dnc_addr, &snbits, name);

  /* if name looks like an dot notation just return the address */
  if (i == 0) {
    /* fill in the static hostent structure and return it */
    dnc_hostent.h_name = name;
    dnc_hostent.h_aliases = NULL;  /* we don't do the aliases */
    dnc_hostent.h_addrtype = AF_INET; /* AF_INET */
    dnc_hostent.h_length = 4;   /* length of IP address */
    dnc_hostent.h_addr_list = &dnc_addrptrs[0];
    dnc_addrptrs[0] = (char*)&dnc_addr;
    dnc_addrptrs[1] = NULL;
    return &dnc_hostent;
  }

  //
  // On the first call, open UDP connection to receive incoming DNS replys
  //
  if (_hConnection == 0) {
    _hConnection = IP_UDP_Open(0L /* any foreign host */,  DNS_PORT, 0,  _OnRx, 0);
    if (!_hConnection) {
      IP_PANIC("Can not open UDP connection for DHCP");
      return NULL;
    }
    IP_AddTimer(_IP_DNSC_Timer, 1000);
  }

  //
  // Periodically check if either an response has been received or the timeout expires
  //
  Timeout = IP_OS_GetTime32() + 5000;
  for (;;) {
    int RemTime;
    err = _dns_query_type(name, DNS_TYPE_IPADDR, &dns_entry);
    if (err == 0) {
      break;
    }
    if (err != IP_ERR_SEND_PENDING) {
      return NULL;
    }
    RemTime = Timeout - IP_OS_GetTime32();
    if (RemTime < 0) {
      return NULL;
    }
    IP_OS_Delay(10);
  };

  //
  // Return address in proper format.
  //
  ho = &dns_entry->he;
  ho->h_addrtype = AF_INET;
  ho->h_length = 4;   /* length of IP address */

  /* fill in the address pointer list in hostent structure */
  for (i = 0; i < dns_entry->ipaddrs; i++) {
    dns_entry->addrptrs[i] = (char*)&dns_entry->ipaddr_list[i];
  }

  dns_entry->addrptrs[i] = NULL;      /* last one gets a NULL */
  ho->h_addr_list = &dns_entry->addrptrs[0];
  return &dns_entry->he;
}

#ifdef DNS_CLIENT_UPDT

/*********************************************************************
*
*       dns_update()
*
* Sends a DNS UPDATE packet to the authoritative server
* of the specified domain name.  This routine uses the getsoa call
* to get the authoritative server of the domain name.  It then sends
* the UPDATE packet to the authoritative server.
*
* RETURNS: 0 if successful
*          negative ENP error if internal error occurs (eg timeout)
*          else one of the DNSRC_ errors (all positive).
*/
int dns_update(char * soa_mname, char * dname, ip_addr ipaddr,  unsigned long ttl) {
  int      err;
  I32      Timeout;
  struct dns_querys * dns_entry;
  unsigned char * ucp;
  ip_addr save_dns_srvr;

  /* get authoritative name server for specified domain. We will
  * call _dns_query_type() until it either succeeds, fails or
  * we time out trying.
  */
  Timeout = IP_OS_GetTime32() + 5000;
  err = _dns_query_type(dname, DNS_TYPE_AUTHNS, &dns_entry);	
  do {	
    if ((err = dnc_lookup(dname, &dns_entry)) == 0) {
      break;
    }
    IP_OS_Delay(10);  // TBD: Use Sleep and wait for Event
  } while  ((Timeout - IP_OS_GetTime32()) > 0);
  if (err) {
      return err;
  }

   /* Get here if we received a hostent structure. Send the update packet
    * to the received IP address (first in list) by swapping the address
    * into the list of DNS servers.
    */

   if (dns_entry->he.h_addr_list) {
      ucp = (unsigned char *) *(dns_entry->he.h_addr_list);
      MEMCPY(&save_dns_srvr, dns_servers, 4);   /* save 1st DNS server */
      MEMCPY(dns_servers, ucp, 4);              /* swap in AUTHS address */
//      IP_LOG((IP_MTYPE_DNS, "Sending update packet to %i\n", ucp));
   }

   /* load up input parameters for DNS UPDATE request */
   dns_entry->he.h_z_name = soa_mname;
   dns_entry->he.h_add_ipaddr = ipaddr;
   dns_entry->he.h_ttl = ttl;
   Timeout = IP_OS_GetTime32() + (5 * TPS);
   err = _dns_query_type(dname, DNS_TYPE_ALIAS, &dns_entry);
   while (IP_OS_GetTime32() < Timeout) {
     if ((err = dnc_lookup(dname, &dns_entry)) == 0) {
            break;
     }
     tk_yield();
   }

   MEMCPY(&dns_servers[0], &save_dns_srvr, 4);
   if (err) {
      return err;
   }
   return dns_entry->rcode; /* Return response code */
}

#endif  /* DNS_CLIENT_UPDT */





/*********************************************************************
*
*       IP_ShowDNS1
*/
int IP_ShowDNS1    (void (*pfSendf)(void * pContext, const char * sFormat, ...), void * pContext) {
#if IP_DEBUG > 0
   struct dns_querys * dns_entry;
   struct hostent *  p;
   char ** cpp;
   int   i;

   if(dns_qs == NULL) {
      pfSendf(pContext, "No DNS entries.\n");
      return 0;
   }

   /* look through the cache for the passed name */
   for(dns_entry = dns_qs; dns_entry; dns_entry = dns_entry->next) {
      if(dns_entry->rcode != DNSRC_OK) {
         pfSendf(pContext, "Query for %s: ", dns_entry->dns_names);
         if(dns_entry->rcode == DNSRC_UNSET) {
           pfSendf(pContext, "no reply\n");
         } else {
           pfSendf(pContext, "bad reply code was %d.\n", dns_entry->rcode);
         }
         continue;
      }
      if(dns_entry->he.h_name) {
         pfSendf(pContext, "name: %s, ", dns_entry->he.h_name);
      }
      p= &dns_entry->he;
      if(*p->h_aliases) {
         pfSendf(pContext, "\n  Aliases: ");
         for(cpp = p->h_aliases; *cpp; cpp++) {
           pfSendf(pContext, "%s, ", *cpp);
         }
      }
#ifdef IP_V4
      if (dns_entry->ipaddrs) {
        pfSendf(pContext, "\n  IPv4 addrs: ");
        for (i = 0; i < dns_entry->ipaddrs; i++) {
         pfSendf(pContext, "%i, ", dns_entry->ipaddr_list[i]);
        }
      }
#endif   /* IP_V4 */
      pfSendf(pContext, "\n  Age (in seconds): %lu, Expires in %lu seconds",  (IP_OS_GetTime32() - dns_entry->send_time)/TPS, (dns_entry->expire_time - IP_OS_GetTime32())/TPS );
      pfSendf(pContext, "\n");
   }
#else
  pfSendf(pContext,"DNS infos not available in release build.");
#endif
   return 0;
}

/*********************************************************************
*
*       IP_ShowDNS
*/
int IP_ShowDNS       (void (*pfSendf)(void * pContext, const char * sFormat, ...), void * pContext) {
#if IP_DEBUG > 0
  struct dns_querys * dns_entry;
  int   i;

  pfSendf(pContext,"DNS servers:");
  for (i = 0; i < IP_MAX_DNS_SERVERS; i++) {
   pfSendf(pContext,"%i ", IP_aIFace[0].aDNSServer[i]);
  }
  pfSendf(pContext,"\nDNS cache:\n");
  for(dns_entry = dns_qs; dns_entry; dns_entry = dns_entry->next) {
    pfSendf(pContext,"name: %s, IP: %i, ",          dns_entry->he.h_name, dns_entry->ipaddr_list[0] );
    pfSendf(pContext,"retry:%d, ID:%d, rcode:%d, err:%d\n",  dns_entry->tries, dns_entry->id, dns_entry->rcode, dns_entry->err);
  }
  pfSendf(pContext,"protocol/implementation runtime errors:%lu\n", dnsc_errors);
  pfSendf(pContext,"requests sent:%lu\n", dnsc_requests);
#ifdef DNS_CLIENT_UPDT
  pfSendf(pContext, "Updates sent:%lu\n", dnsc_updates);
#endif /* DNS_CLIENT_UPDT */
  pfSendf(pContext,"replies received:%lu\n", _ReplyCnt);
  pfSendf(pContext,"usable replies:%lu\n", dnsc_good);
  pfSendf(pContext,"total retries:%lu\n", dnsc_retry);
  pfSendf(pContext,"timeouts:%lu\n", dnsc_tmos);
#else
  pfSendf(pContext,"DNS infos not available in release build.");
#endif
  return 0;
}

/*********************************************************************
*
*       IP_DNS_SetServer
*/
void IP_DNS_SetServer (U32 DNSServerAddr) {
  DNSServerAddr = htonl(DNSServerAddr);
  IP_aIFace[0].aDNSServer[0] = DNSServerAddr;
}

/*********************************************************************
*
*       IP_DNS_GetServer
*/
U32 IP_DNS_GetServer (void) {
  return ntohl(IP_aIFace[0].aDNSServer[0]);
}

/*********************************************************************
*
*       IP_DNS_SetServerEx
*
*  Function description
*    Sets the IP address of the available DNS servers for an interface.
*
*  Parameters
*    IFace:    [IN] Zero-based interface index.
*    DNSIndex: [IN] Zero-based index of DNS servers.
*    pDNSAddr: [IN] IP address of the DNS server.
*    AddrLen:  [IN] Length of the DNS server address.
*
*  Return value
*    0  - OK.
*    -1 - Error.
*/
int IP_DNS_SetServerEx (U8 IFace, U8 DNSIndex, const U8 * pDNSAddr, int AddrLen) {
  U32 IPAddr;

  if ((DNSIndex >= IP_MAX_DNS_SERVERS) || (AddrLen != 4)) { // IPv4 address are always 4-bytes long.
    return -1;  // Error.
  }
  IPAddr = *(U32*)pDNSAddr;
  IP_aIFace[0].aDNSServer[DNSIndex] = htonl(IPAddr);
  return 0;
}

/*********************************************************************
*
*       IP_DNS_GetServerEx
*
*  Function description
*    Returns the IP address of the requested DNS server.
*
*  Parameters
*    IFace:    [IN] Zero-based interface index.
*    DNSIndex: [IN] Zero-based index of DNS servers.
*    pDNSAddr: [IN] Pointer to a buffer to store the address of the requested DNS server.
*    AddrLen:  [IN] Size of the buffer.
*              [OUT] Length of the address stored in the buffer.
*/
void IP_DNS_GetServerEx (U8 IFace, U8 DNSIndex, U8 * pAddr, int * pAddrLen) {
  void * p;

  if ((DNSIndex >= IP_MAX_DNS_SERVERS) || (*pAddrLen < 4)) {
    return;  // Error.
  }
  p = &IP_aIFace[0].aDNSServer[DNSIndex];
  *pAddrLen = 4;   // IPv4 address are always 4-bytes long.
  IP_MEMCPY(pAddr, p, *pAddrLen);
}

/*********************************************************************
*
*       IP_ResolveHost()
*
*  Function description
*    Resolve an IP address text string into an actual IP address.
*    Calls DNS if
* supported. flags word is a bit mask of the RH_ values in dns.h
* Returns 0 if address was set, else one of the IP_ERR_ error codes
*
*
* PARAM1: char * host     - IN - textual IP address or host name
* PARAM2: ip_addr * address - OUT - address if successful
* PARAM3: int    flags    - IN - RH_VERBOSE, RH_BLOCK
*
*/
int IP_ResolveHost(char * host, ip_addr *   address,  int   flags) {
  unsigned snbits;  // for pass to parse_ipad() */
  int      e;          /* Error code */
  I32      Timeout;     /* timeout for blocking calls */
  int      blocking =  flags &  RH_BLOCK;

  //
  // Check if addr. is as parsable dot notation, such as 192.168.1.1
  //
  e = _ParseIPAddr(address, &snbits, host);
  if (e == 0) {
    return 0;
  }

  Timeout = IP_OS_GetTime32() + (5 * TPS);  /* set timeout value */

  if (_FindDNSServer()) { /* dont bother if no servers are set */
//    DPRINTF("trying DNS lookup...\n");
    e = dns_query(host, address);
    if (e == IP_ERR_SEND_PENDING) {
      if (blocking) {
        while ((Timeout - (I32)IP_OS_GetTime32()) > 0) {
//          tk_yield();
          e = dns_query(host, address);
          if (e == 0) {
            goto rh_got_dns;
          }
        }
      }
//      DPRINTF("DNS inquiry sent\n");
      return 0;
    } else if(e == 0) {
rh_got_dns:
      /* DNS resolution worked */
//      DPRINTF(("active host found via DNS (%i)\n",  *address));
      return 0;
    } else if(e == IP_ERR_TIMEOUT) {  /* timeout? */
//      DPRINTF("DNS timeout");
    } else {
//      DPRINTF(("DNS error %d", e));
    }
//    DPRINTF(", host not set\n");
    return e;
  }
  IP_LOG((IP_MTYPE_DNSC, "DNSC: IP_ResolveHost() failed.\n"));
  return IP_ERR_PARAM;
}






/*********************************************************************
*
*       IP_DNSC_SetMaxTTL()
*
*  Function description
*    Sets the max. TTL of a DNS entry in seconds.
*    The real TTL is the minimum of this value and the TTL specified by the DNS server for the entry.

*/
void IP_DNSC_SetMaxTTL(U32 TTL) {
  _TTLMax = TTL;
}





/*************************** End of file ****************************/


