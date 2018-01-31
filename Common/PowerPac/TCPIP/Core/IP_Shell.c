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
File        : IP_Shell.c
Purpose     : TCP/IP system core routines
---------------------------END-OF-HEADER------------------------------
*/

/*********************************************************************
*
*       #include Section
*
**********************************************************************
*/

#include <stdlib.h>
#include <stdarg.h>

#include "IP_Int.h"

/*********************************************************************
*
*       #define constants
*
**********************************************************************
*/

/*********************************************************************
*
*       Local data types
*
**********************************************************************
*/

/*********************************************************************
*
*       Static code
*
**********************************************************************
*/


/*********************************************************************
*
*       _toupper
*/
static char _toupper(char c) {
  if ((c >= 'a') && (c <= 'z')) {
    c -= 32;
  }
  return c;
}

/*********************************************************************
*
*       _SendString
*/
static void _SendString(long Socket, const char* s) {
  int Len;

  if (s) {
    Len = strlen(s);
    send(Socket, (char *)s, Len, 0);
  }
}

/*********************************************************************
*
*       IP_Sendf
*/
void IP_Sendf(void * pContext, const char * sFormat, ...) {
  va_list ParamList;
  IP_SENDF_CONTEXT * p;

  p = (IP_SENDF_CONTEXT*)pContext;
  if (sFormat) {
    va_start(ParamList, sFormat);
    IP_PrintfSafe(p->pBuffer, sFormat, p->BufferSize, &ParamList);
  }
  _SendString(p->Socket, p->pBuffer);
}

/*********************************************************************
*
*       _SendfLineEnd
*/
static void _SendfLineEnd(void * pContext, const char * sFormat, ...) {
  int NumBytes;
  va_list ParamList;
  IP_SENDF_CONTEXT * p;

  p = (IP_SENDF_CONTEXT*)pContext;
  if (sFormat) {
    va_start(ParamList, sFormat);
//    vsprintf(p->pBuffer, sFormat, ParamList);
    IP_PrintfSafe(p->pBuffer, sFormat, p->BufferSize, &ParamList);
  }
  NumBytes = strlen(p->pBuffer);
  _SendString(p->Socket, p->pBuffer);
//  send(p->Socket, p->pBuffer, NumBytes, 0);
  //
  // Add "\r" to make windows telnet happy
  //
  if (*(p->pBuffer + NumBytes -1) == '\n') {
  _SendString(p->Socket, "\r");
  //    send(p->Socket, "\r", 1, 0);
  }
}

static   char acOut[1024];

/*********************************************************************
*
*       Types
*
**********************************************************************
*/
typedef struct {
  int (*pfShow)       (void (*pfSendf)(void * pContext, const char * sFormat, ...), void * pContext);
  const char *  sLongName;
  const char *  sShortName;
  const char * sDescription;
} COMMAND;

/*********************************************************************
*
*       _CompareCmd
*
*  Return value
*    0    Equal
*    1    Not equal
*/
static char _CompareCmd(const char ** ps, const char * sCmd) {
  const char *s;
  s = *ps;
  do {
    char c;
    char c1;

    c  = _toupper(*sCmd++);
    c1 = _toupper(*s);
    if (c == 0) {
      if ((c1 == 0x20) || (c1 == 0)) {
        *ps = s;
        return 0;       // Command found
      }
      return 1;         // Command not found
    }
    if (c != c1) {
      return 1;         // Command not found
    }
    s++;
  } while (1);
}

/*********************************************************************
*
*       Command list
*/
static const COMMAND _aCmd[] = {
  { IP_ShowARP,        "arp",      NULL, "Show arp status"         },
  { IP_ShowICMP,       "icmp",     NULL, "Show ICMP statistics"    },
  { IP_ShowTCP,        "tcp",      NULL, "Show TCP statistics"     },
  { IP_ShowSocketList, "sock",     NULL, "Show socket list"        },
  { IP_ShowBSDConn,    "bsd",      NULL, ""},
  { IP_ShowBSDSend,    "bsdsend",  NULL, ""},
  { IP_ShowBSDRcv,     "bsdrcv",   NULL, ""},
  { IP_ShowMBuf,       "mbuf",     NULL, "Show memory buffer statistics"},
  { IP_ShowMBufList,   "mbl",      NULL, "Show memory buffer list"},
  { IP_ShowStat,       "stat",     NULL, "Show MIB statistics"},
  { IP_ShowUDP,        "udp",      NULL, ""},
  { IP_ShowUDPSockets, "udpsock",  NULL, "List UDP sockets"},
  { IP_ShowDHCPClient, "dhcp",     NULL, "Show status of DHCP client"},
  { IP_ShowDNS,        "dns",      NULL, "Show status of DNS"},
  { IP_ShowDNS1,       "dns1",     NULL, "Show status of DNS"},
};




/*********************************************************************
*
*       _ExecCommandLine
*/
static void _ExecCommandLine(const char* s, long Socket) {
  int i;

/* Pointer version */
  const COMMAND * pCommandLst;

  pCommandLst = &_aCmd[0];

  for (i = 0; i < COUNTOF(_aCmd); i++) {
    if (_CompareCmd(&s, pCommandLst->sLongName) == 0) {
      IP_SENDF_CONTEXT Context;
Found:
      Context.Socket  = Socket;
      Context.pBuffer = acOut;
      Context.BufferSize =  sizeof(acOut);
      pCommandLst->pfShow(_SendfLineEnd, &Context);
      return;
    }
    if (pCommandLst->sShortName) {
      if (_CompareCmd(&s, pCommandLst->sShortName) == 0) {
        goto Found;
      }
    }
    pCommandLst++;
  }
  //
  // Output list of commands
  //
  _SendString(Socket, "List of supported commands\r\n");
  pCommandLst = &_aCmd[0];
  for (i = 0; i < COUNTOF(_aCmd); i++) {
    const char * s;
    s = pCommandLst->sLongName;
    if (s == NULL) {
      s = pCommandLst->sShortName;
    }
    _SendString(Socket, s);
    _SendString(Socket, " --> ");
    _SendString(Socket, pCommandLst->sDescription);
    _SendString(Socket, "\n\r");
    pCommandLst++;
  }
}

/*********************************************************************
*
*       _Process
*/
static void _Process(long Socket) {
  U8 c;
  U8 acCommandBuffer[50] = {0};
  const char * sHello = "IP-Shell.\n\rEnter command (? for help)\n\r";
  int NumBytes;
  int i = 0;
  _SendString(Socket, sHello);
  do {
    NumBytes = recv(Socket, (char *) &c, 1, 0);
    if (NumBytes <= 0) {
      return;
    }
    if (c == '\r') {
      acCommandBuffer[i] = 0;
      if (i) {
        _ExecCommandLine((const char *) &acCommandBuffer, Socket);
        i = 0;
      } else {
        _SendString(Socket, "(? for help)\n\r");
      }
    } else if (c >= 32) {
      if (i < sizeof(acCommandBuffer) -1) {
        acCommandBuffer[i++] = c;
      }
    }
  } while (1);
}

/*********************************************************************
*
*       _ListenAtTcpAddr
*
* Starts listening at the given TCP port.
*/
static long _ListenAtTcpAddr(void) {
  long sock;
  struct sockaddr_in addr;

  sock = socket(AF_INET, SOCK_STREAM, 0);
  IP_MEMSET(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(23);
  addr.sin_addr.s_addr = INADDR_ANY;
  bind(sock, (struct sockaddr *)&addr, sizeof(addr));
  listen(sock, 5);

  return sock;
}

/*********************************************************************
*
*       Public code
*
**********************************************************************
*/

/*********************************************************************
*
*       IP_ServerThread
*
* Purpose
*   The server thread.
*/
void IP_ShellServer(void) {
  long s, sock;
  struct sockaddr addr;

  do {
    s = _ListenAtTcpAddr();
    if (s != SOCKET_ERROR) {
      break;
    }
    IP_OS_Delay(10);    // Try again
  } while (1);

  while (1) {
    // Wait for an incoming connection
    int addrlen = sizeof(addr);
    if ((sock = accept(s, &addr, &addrlen)) == SOCKET_ERROR) {
      continue;    // Error
    }
    _Process(sock);    // Then process this client
    closesocket(sock);
  }
}


/*************************** End of file ****************************/
