/*********************************************************************
*                SEGGER MICROCONTROLLER SYSTEME GmbH                 *
*        Solutions for real time microcontroller applications        *
**********************************************************************
*                                                                    *
*        (c) 2002         SEGGER Microcontroller Systeme GmbH        *
*                                                                    *
*        Internet: www.segger.com    Support:  support@segger.com    *
*                                                                    *
**********************************************************************
----------------------------------------------------------------------
File        : IP_SMTPC.c
Purpose     : SMTP client

Note:
  - Refer to RFC2821 "Simple Mail Transfer Protocol"
    http://tools.ietf.org/html/rfc2821
  - Refer to RFC5322 "Internet Message Format"
    http://tools.ietf.org/html/rfc5322

  SMTP reply codes in numeric order (RFC2821, section 4.2.3)
    211 System status, or system help reply
    214 Help message (Information on how to use the receiver or the meaning of a particular non-standard command; this reply is useful only to the human user)
    220 <domain> Service ready
    221 <domain> Service closing transmission channel
    250 Requested mail action okay, completed
    251 User not local; will forward to <forward-path>
    252 Cannot VRFY user, but will accept message and attempt delivery
    354 Start mail input; end with <CRLF>.<CRLF>
    421 <domain> Service not available, closing transmission channel (This may be a reply to any command if the service knows it must shut down)
    450 Requested mail action not taken: mailbox unavailable (e.g., mailbox busy)
    451 Requested action aborted: local error in processing
    452 Requested action not taken: insufficient system storage
    500 Syntax error, command unrecognized (This may include errors such as command line too long)
    501 Syntax error in parameters or arguments
    502 Command not implemented (see section 4.2.4)
    503 Bad sequence of commands
    504 Command parameter not implemented
    550 Requested action not taken: mailbox unavailable (e.g., mailbox not found, no access, or command rejected for policy reasons)
    551 User not local; please try <forward-path>
    552 Requested mail action aborted: exceeded storage allocation
    553 Requested action not taken: mailbox name not allowed (e.g., mailbox syntax incorrect)
    554 Transaction failed  (Or, in the case of a connection-opening response, "No SMTP service here")

---------------------------END-OF-HEADER------------------------------
*/
#include "IP_Util.h"
#include "IP_SMTPC.h"
#include "SMTPC_Conf.h"

#include <stdio.h>
#include <string.h>

/*********************************************************************
*
*       Defines
*
**********************************************************************
*/
#ifndef   SMTPC_WARN
  #define SMTPC_WARN(p)
#endif

#ifndef   SMTPC_LOG
  #define SMTPC_LOG(p)
#endif

#ifndef   SMTPC_MSGID_DOMAIN
  #define SMTPC_MSGID_DOMAIN     "@sample.com"
#endif

/*********************************************************************
*
*       Types
*
**********************************************************************
*/
typedef struct {
  void * Socket;
  char   acInBuffer[SMTPC_IN_BUFFER_SIZE];
  int    Cnt;
} CONNECTION_CONTEXT;

typedef struct {
  const IP_SMTPC_API * pIP_API;
  CONNECTION_CONTEXT * pConnection;
  char * pBuffer;
  int BufferSize;
  int Cnt;
  int Encoding;
} OUT_BUFFER_CONTEXT;


typedef struct {
  char * pMailHost;
  char acEncodedUser[SMTPC_AUTH_USER_BUFFER_SIZE];
  char acEncodedPass[SMTPC_AUTH_PASS_BUFFER_SIZE];
} MTA_CONTEXT;

typedef struct {
  const IP_SMTPC_API * pIP_API;
  CONNECTION_CONTEXT Connection;
  MTA_CONTEXT MTAContext;
  const IP_SMTPC_APPLICATION * pApplication;
  OUT_BUFFER_CONTEXT OutBuffer;
  IP_SMTPC_MAIL_ADDR * paMailAddr;
  int NumMailAddr;
  IP_SMTPC_MESSAGE * pMessage;
} SMTP_CONTEXT;

/*********************************************************************
*
*       Static data
*
**********************************************************************
*/
static const char * _asMonth[12] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
static int _MsgId = 1000;

/*********************************************************************
*
*       Static code
*
**********************************************************************
*/

/*********************************************************************
*
*       _recv
*/
static void _recv(SMTP_CONTEXT * pContext, char * s, int Len) {
  Len = pContext->pIP_API->pfReceive(s, Len, pContext->Connection.Socket);
  if (Len > 0) {
    *(s + Len) = 0;
    SMTPC_LOG(("    << %s", s));
    pContext->Connection.Cnt += Len;
  } else {
    SMTPC_LOG(("Socket error" ));
  }
}

/**************************************************************************************************************************************************************
*
*       Output related code
*/

/*********************************************************************
*
*       _Send
*/
static void _Send(SMTP_CONTEXT * pContext, const char * s) {
  int NumBytes;

  NumBytes = pContext->pIP_API->pfSend(s, strlen(s),pContext->Connection.Socket);
  if (NumBytes != -1) {
    SMTPC_LOG(("    >> %s", s ));
  }
}

/*********************************************************************
*
*       _Flush
*/
static void _Flush(OUT_BUFFER_CONTEXT * pOutContext) {
  int Len;

  Len = pOutContext->Cnt;
  if (Len) {
    pOutContext->pIP_API->pfSend(pOutContext->pBuffer, Len, pOutContext->pConnection->Socket);
    pOutContext->Cnt = 0;
  }
}

/*********************************************************************
*
*       _WriteChar
*/
static void _WriteChar(OUT_BUFFER_CONTEXT * pOutContext, char c) {
  int Len;

  Len = pOutContext->Cnt;
  *(pOutContext->pBuffer + Len) = c;
  Len++;
  pOutContext->Cnt = Len;
  if (Len == pOutContext->BufferSize) {
    _Flush(pOutContext);
  }
}

/*********************************************************************
*
*       _WriteString
*/
static void _WriteString(OUT_BUFFER_CONTEXT * pOutContext, const char *s) {
  char c;

  do {
    c = *s++;
    if (c == 0) {
      break;
    }
    _WriteChar(pOutContext, c);
  } while (1);
}

/*********************************************************************
*
*       _WriteUnsigned
*
*  Function description
*    Writes an unsigned numerical value into the out context.
*
*  Parameters
*    pOutContext   Out context containing buffer information and output functions.
*    v             Value to output
*    Base          Numerical base. Typically 2, 10 or 16.
*    NumDigits     Minimum number of digits to output.
*
*  Examples
*    _WriteUnsigned(p, 100, 10, 0)   ->   "100"
*    _WriteUnsigned(p, 100, 10, 4)   ->   "0100"
*    _WriteUnsigned(p, 100, 16, 0)   ->   "64"
*    _WriteUnsigned(p, 100,  2, 0)   ->   "1100100"
*/
static void _WriteUnsigned(OUT_BUFFER_CONTEXT * pOutContext, U32 v, unsigned Base, int NumDigits) {
  unsigned Div;
  U32 Digit = 1;
  static const char _aV2C[16] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' };
  //
  // Count how many digits are required
  //
  while (((v / Digit) >= Base) | (NumDigits-- > 1)) {
    Digit *= Base;
  }
  //
  // Output digits
  //
  do {
    Div = v / Digit;
    v  -= Div * Digit;
    _WriteChar(pOutContext, _aV2C[Div]);
    Digit /= Base;
  } while (Digit);
}

/*********************************************************************
*
*       _BufferContainsLine
*
*  Function Description
*    Checks if a complete line is stored in the buffer.
*/
static int _BufferContainsLine(SMTP_CONTEXT * pContext) {
  int i;
  for (i = 0; i < pContext->Connection.Cnt; i++) {
    if (pContext->Connection.acInBuffer[i] == '\n') {
      return i + 1;
    }
  }
  return 0;
}

/*********************************************************************
*
*       _ClearBuffer
*
*  Function Description
*    Clears the buffer.
*/
static void _ClearBuffer(SMTP_CONTEXT * pContext) {
  pContext->Connection.Cnt = 0;
}

/*********************************************************************
*
*       _WriteHeaderMsgID
*
*  Function Description
*    Writes the header field "Message-ID".
*    "Message-ID" field should look like:
*      Message-ID: <1234.5678@DOMAIN>
*    The message-id has to be unique for all messages.
*/
static void _WriteHeaderMsgID(SMTP_CONTEXT * pContext) {
  _WriteString(&pContext->OutBuffer, "Message-ID: <");
  _WriteUnsigned(&pContext->OutBuffer, _MsgId, 10, 0);
  _WriteString(&pContext->OutBuffer, ".");
  _WriteUnsigned(&pContext->OutBuffer, _MsgId + 1234, 10, 0);
  if (pContext->pApplication->sDomain) {     // Check if domain is set, if it is not set use the value of SMTPC_MSGID_DOMAIN as in previous versions.
    _WriteChar(&pContext->OutBuffer, '@');
    _WriteString(&pContext->OutBuffer, pContext->pApplication->sDomain);
  } else {
    _WriteString(&pContext->OutBuffer, SMTPC_MSGID_DOMAIN);
  }
  _WriteString(&pContext->OutBuffer, ">\r\n");
  _MsgId++;  // Increment Message-ID since message-id has to be unique.
}

/*********************************************************************
*
*       _WriteHeaderFrom
*
*  Function description
*    Writes the header field "FROM".
*    "FROM" field should look like:
*      From: "Foo Bar" <foo@bar.com>
*/
static void _WriteHeaderFrom(SMTP_CONTEXT * pContext) {
  _WriteString(&pContext->OutBuffer, "From: \"");
  _WriteString(&pContext->OutBuffer, pContext->paMailAddr[0].sName);
  _WriteString(&pContext->OutBuffer, "\"<");
  _WriteString(&pContext->OutBuffer, pContext->paMailAddr[0].sAddr);
  _WriteString(&pContext->OutBuffer, ">\r\n");
}

/*********************************************************************
*
*       _WriteHeaderRecipients
*
*  Function description
*    Writes the header fields "TO", "CC", "BCC".
*    "TO" field should look like:
*      To: "Webmaster" <webmaster@xyz.com>
*    "CC" field should look like:
*      CC: "Admin1" <admin1@yxz.com>
*    "BCC" field should look like:
*      BCC: "Admin2" <admin2@yxz.com>
*/
static void _WriteHeaderRecipients(SMTP_CONTEXT * pContext, int Type) {
  int i;
  int Cnt;

  i   = 0;
  Cnt = 0;
  while (i < pContext->NumMailAddr) {
    if (pContext->paMailAddr[i].Type == Type) {
      Cnt++;
      if (Cnt == 1) {
        switch(Type) {
        case SMTPC_REC_TYPE_TO:
          _WriteString(&pContext->OutBuffer, "TO: ");
          break;
        case SMTPC_REC_TYPE_CC:
          _WriteString(&pContext->OutBuffer, "CC: ");
          break;
        case SMTPC_REC_TYPE_BCC:
          _WriteString(&pContext->OutBuffer, "BCC: ");
          break;
        }
        _WriteString(&pContext->OutBuffer, pContext->paMailAddr[i].sName);
        _WriteString(&pContext->OutBuffer, " <");
        _WriteString(&pContext->OutBuffer, pContext->paMailAddr[i].sAddr);
        _WriteChar(&pContext->OutBuffer, '>');
        i++;
        continue;
      }
      _WriteString(&pContext->OutBuffer, ", ");
      _WriteString(&pContext->OutBuffer, pContext->paMailAddr[i].sName);
        _WriteString(&pContext->OutBuffer, " <");
      _WriteString(&pContext->OutBuffer, pContext->paMailAddr[i].sAddr);
      _WriteChar(&pContext->OutBuffer, '>');
    }
    i++;
  }
  if (Cnt > 0) {
    _WriteString(&pContext->OutBuffer, "\r\n");
    _Flush(&pContext->OutBuffer);
  }
}

/*********************************************************************
*
*       _WriteHeaderTo
*
*  Function description
*    Writes the header field "TO".
*    "TO" field should look like:
*      To: "Webmaster" <webmaster@xyz.com>
*/
static void _WriteHeaderTo(SMTP_CONTEXT * pContext) {
  _WriteHeaderRecipients(pContext, SMTPC_REC_TYPE_TO);
}

/*********************************************************************
*
*       _WriteHeaderCC
*
*  Function description
*    Writes the header field "CC".
*    "CC" field should look like:
*      CC: "Admin1" <admin1@yxz.com>
*/
static void _WriteHeaderCC(SMTP_CONTEXT * pContext) {
  _WriteHeaderRecipients(pContext, SMTPC_REC_TYPE_CC);
}

/*********************************************************************
*
*       _WriteHeaderSubject
*
*  Function description
*    Writes the header field "Subject".
*    "Subject" field should look like:
*      Subject: My status
*/
static void _WriteHeaderSubject(SMTP_CONTEXT * pContext) {
  _WriteString(&pContext->OutBuffer, "Subject: ");
  _WriteString(&pContext->OutBuffer, pContext->pMessage->sSubject);
  _WriteString(&pContext->OutBuffer, "\r\n");
}

/*********************************************************************
*
*       _WriteHeaderDate
*
*  Function description
*    Writes the header field "Date".
*    "Date" field should look like:
*      Date: 1 Jan 2008 00:00 +0100
*/
static void _WriteHeaderDate(SMTP_CONTEXT * pContext) {
  int SysTime;
  int Day;
  int Month;
  int Year;
  int Hour;
  int Minute;
  //
  // Get time and date and build header field.
  //
  SysTime = pContext->pApplication->pfGetTimeDate();
  Day     = (SysTime >> 16) & 0x1F;
  Month   = (SysTime >> 21) & 0x0F;
  Year    = (SysTime >> 25) & 0x7F;
  Hour    = (SysTime >> 11) & 0x1F;
  Minute  = (SysTime >>  5) & 0x3F;
  _WriteString(&pContext->OutBuffer, "Date: ");
  _WriteUnsigned(&pContext->OutBuffer, Day, 10, 0);
  _WriteString(&pContext->OutBuffer, " ");
  _WriteString(&pContext->OutBuffer, _asMonth[Month-1]);
  _WriteString(&pContext->OutBuffer, " ");
  _WriteUnsigned(&pContext->OutBuffer, Year + 1980, 10, 0);
  _WriteString(&pContext->OutBuffer, " ");
  _WriteUnsigned(&pContext->OutBuffer, Hour, 10, 2);
  _WriteString(&pContext->OutBuffer, ":");
  _WriteUnsigned(&pContext->OutBuffer, Minute, 10, 2);
  //
  // Check if time zone offset is set.
  // By default, we use "-0000"
  //
  // RFC5322 says:
  // "Though "-0000" also indicates Universal Time, it is used to indicate
  //  that the time was generated on a system that may be in a local time zone
  //  other than Universal Time and that the date-time contains no information
  //  about the local time zone."
  //
  if (pContext->pApplication->sTimezone) {
    _WriteString(&pContext->OutBuffer, " ");
    _WriteString(&pContext->OutBuffer, pContext->pApplication->sTimezone);
  } else {
    _WriteString(&pContext->OutBuffer, " -0000");
  }
  _WriteString(&pContext->OutBuffer, "\r\n\r\n");
}

/*********************************************************************
*
*       _WriteHeader
*
*  Function description
*    Writes the message header.
*    The header should be similar to:
*    Message header should look similar to:
*      Message-ID: <A3F.F11@xyz.com>
*      From: "Info" <info@xyz.com>
*      To: "Webmaster" <webmaster@xyz.com>
*      CC: "Admin1" <admin1@yxz.com>
*      Subject: My message
*      Date: 1 Jan 2008 00:00 +0100
*/
static U32 _WriteHeader(SMTP_CONTEXT * pContext) {
  _WriteHeaderMsgID(pContext);
  _WriteHeaderFrom(pContext);
  _WriteHeaderTo(pContext);
  _WriteHeaderCC(pContext);
  _WriteHeaderSubject(pContext);
  _WriteHeaderDate(pContext);
  _Flush(&pContext->OutBuffer);
  return 0;
}


/*********************************************************************
*
*       _WaitForStatus
*
*  Function description
*    Receives the status message of the server and compares the first
*    3 digits with the expected status message code.
*/
static int _WaitForStatus(SMTP_CONTEXT * pContext, const char * sStatus) {
  _recv(pContext, pContext->Connection.acInBuffer, sizeof(pContext->Connection.acInBuffer));
  if (_BufferContainsLine(pContext) == 0) {
    return 1;
  }
  if (strncmp(pContext->Connection.acInBuffer, sStatus, 3)) {
    SMTPC_LOG(("Error: Expected %s reply, got %s", sStatus, pContext->Connection.acInBuffer));
    return 1;
  }
  _ClearBuffer(pContext);
  return 0;
}

/*********************************************************************
*
*       _SendRelayed
*
*  Function description
*    Connects to a relay server and sends data to the relay server. The
     relay server forwards the message to the recipients.
*
*  Sample session:
*    Scenario: Client connects to a relay server (sample.com)and logs
*    in a user account with user name and password. The mail should
*    be sent to User1. User2, User3, and User4 are on carbon copy (CC).
*    User5 is on blind carbon copy (BCC).
*    'S' stands for SMTP server
*    'C' stands for SMTP client
*    ----------------------------------------------------------------
*    S:  220 srv.sample.com ESMTP
*    C:   HELO
*    S:  250 srv.sample.com
*    C:   AUTH LOGIN
*    S:  334 VXNlcm5hbWU6
*    C:   c3BzZXk29IulbkY29tZcZXIbtZ
*    S:  334 UGFzc3dvcmQ6
*    C:   UlblhFz7ZlblsZlZQ==
*    S:  235 go ahead
*    C:   Mail from:<user0@sample.com>
*    S:  250 ok
*    C:   Rcpt to:<user1@sample.com>
*    S:  250 ok
*    C:   Rcpt to:<user2@sample.com>
*    S:  250 ok
*    C:   Rcpt to:<user3@sample.com>
*    S:  250 ok
*    C:   Rcpt to:<user4@sample.com>
*    S:  250 ok
*    C:   Rcpt to:<user5@sample.com>
*    S:  250 ok
*    C:   DATA
*    S:  354 go ahead
*    C:   Message-ID: <1000.2234@sample.com>
*    C:   From: "User0" <User0@sample.com>
*    C:   TO: "User1" <User1@sample.com>
*    C:   CC: "User2" <User2@sample.com>, "User3" <User3@sample.com>, "User4" <User4@sample.com>
*    C:   Subject: Testmail
*    C:   Date: 1 Jan 2008 00:00 +0100
*    C:
*    C:   This is a test!
*    C:
*    C:   .
*    S:  250 ok 1231221612 qp 3364
*    C:   quit
*    S:  221 srv.sample.com
*    ----------------------------------------------------------------
*/
static void _SendRelayed(SMTP_CONTEXT * pContext) {
  int r;
  int i;

  i = 1;
  r = _WaitForStatus(pContext, "220");   // Wait for 220 (Service ready)
  if (r == 1) {
    goto End;
  }
  if (pContext->pApplication->sDomain == NULL) {
    _Send(pContext, "HELO\r\n");
  } else {
    _WriteString(&pContext->OutBuffer, "HELO ");
    _WriteString(&pContext->OutBuffer, pContext->pApplication->sDomain);
    _WriteString(&pContext->OutBuffer, "\r\n");
    _Flush(&pContext->OutBuffer);
  }
  r = _WaitForStatus(pContext, "250");   // Wait for 250 (Requested mail action okay, completed)
  if (r == 1) {
    goto End;
  }
  //
  // If an user account is required to send mails, transmit
  // login data to the SMTP server.
  //
  if (strlen(pContext->MTAContext.acEncodedUser)) {
    _Send(pContext, "AUTH LOGIN\r\n");
    r = _WaitForStatus(pContext, "334"); // Wait for 334 VXNlcm5hbWU6 (username)
    if (r == 1) {
      goto End;
    }
    _WriteString(&pContext->OutBuffer, pContext->MTAContext.acEncodedUser);
    _WriteString(&pContext->OutBuffer, "\r\n");
    _Flush(&pContext->OutBuffer);
    r = _WaitForStatus(pContext, "334"); // Wait for UGFzc3dvcmQ6 (password)
    if (r == 1) {
      goto End;
    }
    _WriteString(&pContext->OutBuffer, pContext->MTAContext.acEncodedPass);
    _WriteString(&pContext->OutBuffer, "\r\n");
    _Flush(&pContext->OutBuffer);
    r = _WaitForStatus(pContext, "235"); // Wait for 235
    if (r == 1) {
      goto End;
    }
  }
  _WriteString(&pContext->OutBuffer, "Mail from:");
  _WriteChar(&pContext->OutBuffer, '<');
  _WriteString(&pContext->OutBuffer, pContext->paMailAddr[0].sAddr);
  _WriteString(&pContext->OutBuffer, ">\r\n");
  _Flush(&pContext->OutBuffer);
  r = _WaitForStatus(pContext, "250"); // Wait for 250
  if (r == 1) {
    goto End;
  }
  do {
    _WriteString(&pContext->OutBuffer, "Rcpt to:");
    _WriteChar(&pContext->OutBuffer, '<');
    _WriteString(&pContext->OutBuffer, pContext->paMailAddr[i].sAddr);
    _WriteString(&pContext->OutBuffer, ">\r\n");
    _Flush(&pContext->OutBuffer);
    r = _WaitForStatus(pContext, "250"); // Wait for 250
    if (r == 1) {
      goto End;
    }
    i++;
  } while (i < pContext->NumMailAddr);
  _Send(pContext, "DATA\r\n");
  r = _WaitForStatus(pContext, "354"); // Wait for 354 (Start mail input; end with <CRLF>.<CRLF>)
  if (r == 1) {
    goto End;
  }
  //
  // Build header and send it
  //
  _WriteHeader(pContext);
  //
  // Send mail body
  //
  _Send(pContext, pContext->pMessage->sBody);
  _Send(pContext, "\r\n.\r\n");        // End of message
  r = _WaitForStatus(pContext, "250"); // Wait for 250
  if (r == 1) {
    goto End;
  }
End:
  _Send(pContext, "quit\r\n");         // Log off from server
  _WaitForStatus(pContext, "221");     // Wait for 221 (Service closing transmission channel)
}

/*********************************************************************
*
*       Public code: main
*
**********************************************************************
*/

/*********************************************************************
*
*       IP_SMTPC_Send
*
*  Function description
*    Sends an email.
*
*/
int IP_SMTPC_Send(const IP_SMTPC_API * pIP_API, IP_SMTPC_MAIL_ADDR * paMailAddr, int NumMailAddr, IP_SMTPC_MESSAGE * pMessage, const IP_SMTPC_MTA * pMTA, const IP_SMTPC_APPLICATION * pApplication) {
  char acOut[SMTPC_OUT_BUFFER_SIZE];
  SMTP_CONTEXT Context;
  int Len;
  //
  //
  //
  IP_MEMSET(&Context, 0, sizeof(Context));
  Context.pIP_API               = pIP_API;
  Context.pMessage              = pMessage;
  Context.paMailAddr            = paMailAddr;
  Context.NumMailAddr           = NumMailAddr;
  Context.OutBuffer.pIP_API     = pIP_API;
  Context.OutBuffer.pConnection = &Context.Connection;
  Context.OutBuffer.pBuffer     = acOut;
  Context.OutBuffer.BufferSize  = sizeof(acOut);
  Context.pApplication          = pApplication;
  Context.MTAContext.pMailHost  = (char*)pMTA->sServer;
  //
  // Encode username and password for SMTP relay server
  //
  if (pMTA->sUser) {
    Len = sizeof(Context.MTAContext.acEncodedUser);
    IP_UTIL_BASE64_Encode((unsigned char const *)pMTA->sUser, strlen(pMTA->sUser), (unsigned char*)Context.MTAContext.acEncodedUser, &Len);
    Context.MTAContext.acEncodedUser[Len] = 0;
    SMTPC_LOG(("User: %s, Base64: %s\r\n", pMTA->sUser, Context.MTAContext.acEncodedUser));
  }
  if (pMTA->sPass) {
    Len = sizeof(Context.MTAContext.acEncodedPass);
    IP_UTIL_BASE64_Encode((unsigned char const *)pMTA->sPass, strlen(pMTA->sPass), (unsigned char*)Context.MTAContext.acEncodedPass, &Len);
    Context.MTAContext.acEncodedPass[Len] = 0;
    SMTPC_LOG(("Pass: %s, Base64: %s\r\n", pMTA->sPass, Context.MTAContext.acEncodedPass));
  }
  //
  // Connect to relay server
  //
  SMTPC_LOG(("Domain: %s\r\n", Context.MTAContext.pMailHost));
  Context.Connection.Socket = Context.pIP_API->pfConnect(Context.MTAContext.pMailHost);
  if (Context.Connection.Socket) {
    _SendRelayed(&Context);
    //
    // Cleanup
    //
    SMTPC_LOG(("Closing socket.\r\n"));
    Context.pIP_API->pfDisconnect(Context.Connection.Socket);
    return 0;
  }
  return 1;
}

/*************************** End of file ****************************/
