/*********************************************************************
*               SEGGER MICROCONTROLLER SYSTEME GmbH                  *
*       Solutions for real time microcontroller applications         *
**********************************************************************
*                                                                    *
*       (C) 2006   SEGGER Microcontroller Systeme GmbH               *
*                                                                    *
*       www.segger.com     Support: support@segger.com               *
*                                                                    *
**********************************************************************

----------------------------------------------------------------------
File    : OS_IP_TFTP.c
Purpose : TFTP routines for embOS & TCP/IP

Literature:
  [1] RFC 1350 - THE TFTP PROTOCOL (REVISION 2)
      http://tools.ietf.org/html/rfc1350

TBD:
  - Handle resends
  - Handle netascii mode
--------- END-OF-HEADER --------------------------------------------*/

#include <string.h>  // Required for memset
#include "IP_TFTP.h"
#include "IP_Int.h"

/*********************************************************************
*
*       Local defines, configurable
*
**********************************************************************
*/
#define TFTP_ERR_STR_NOT_DEFINED               "Not defined, see error message (if any)."
#define TFTP_ERR_STR_FILE_NOT_FOUND            "File not found."
#define TFTP_ERR_STR_ACCESS_VIOLATION          "Access violation."
#define TFTP_ERR_STR_DISK_FULL                 "Disk full or allocation exceeded."
#define TFTP_ERR_STR_ILLEGAL_OP                "Illegal TFTP operation."
#define TFTP_ERR_STR_UNKNOWN_TRANSFER_ID       "Unknown transfer ID."
#define TFTP_ERR_STR_FILE_ALREADY_EXISTS       "File already exists."
#define TFTP_ERR_STR_NO_SUCH_USER              "No such user."

/*********************************************************************
*
*       defines, non-configurable
*
**********************************************************************
*/
#define TFTP_ERR_CODE_NOT_DEFINED              0
#define TFTP_ERR_CODE_FILE_NOT_FOUND           1
#define TFTP_ERR_CODE_ACCESS_VIOLATION         2
#define TFTP_ERR_CODE_DISK_FULL                3
#define TFTP_ERR_CODE_ILLEGAL_OP               4
#define TFTP_ERR_CODE_UNKNOWN_TRANSFER_ID      5
#define TFTP_ERR_CODE_FILE_ALREADY_EXISTS      6
#define TFTP_ERR_CODE_NO_SUCH_USER             7

/*********************************************************************
*
*       static data
*
**********************************************************************
*/
//NONE

/*********************************************************************
*
*       static code
*
**********************************************************************
*/

/*********************************************************************
*
*       _SendAck
*
*  Function description
*    Sends an ACK packet to the client.
*
*  Notes:
*    2 bytes     2 bytes
*    ---------------------
*   | Opcode |   Block #  |
*    ---------------------
*/
static int _SendAck(TFTP_CONTEXT * pContext, U32 IncBlockCnt) {
  U16 * pBuffer;

  pBuffer = (U16*)pContext->pBuffer;
  //
  // Fill packet.
  //
  *pBuffer     = ntohs(TFTP_ACK);
  *(pBuffer+1) = ntohs(pContext->BlockCnt);
  send(pContext->Sock, (char*)pBuffer , 4, 0);
  if (IncBlockCnt) {
    pContext->BlockCnt++;
  }
  return 0;
}

/*********************************************************************
*
*       _SendErr
*
*  Function description:
*    Sends a TFTP error packet.
*
*  Notes:
*    Error packet
*
*       2 bytes     2 bytes      string    1 byte
*       -----------------------------------------
*      | Opcode |  ErrorCode |   ErrMsg   |   0  |
*       -----------------------------------------
*
*      Opcode:
*        5     Error (ERROR)
*
*      Error Codes (Value / Meaning):
*        0     Not defined, see error message (if any).
*        1     File not found.
*        2     Access violation.
*        3     Disk full or allocation exceeded.
*        4     Illegal TFTP operation.
*        5     Unknown transfer ID.
*        6     File already exists.
*        7     No such user.
*/
static int _SendErr(TFTP_CONTEXT * pContext, U8 ErrCode, const char * sErr) {
  unsigned NumBytes;
  unsigned Len;
  U16 * pHeader;
  char *  pBuffer;
  int r;

  IP_LOG((IP_MTYPE_APPLICATION, "TFTP: Send an errer msg: Error no. %d .", ErrCode));
  pBuffer = pContext->pBuffer;
  pHeader = (U16*)pContext->pBuffer;
  //
  // Build the error packet.
  //
  *pHeader     = ntohs(TFTP_ERR);
  *(pHeader+1) = ntohs(ErrCode);
  NumBytes = 4;                  // Skip Opcode and ErrorCode
  Len = strlen(sErr);
  IP_MEMCPY(pBuffer + NumBytes, sErr, Len);
  NumBytes += Len;
  *(pBuffer + NumBytes) = '\0';
  NumBytes++;
  //
  // Send error packet.
  //
  r = send(pContext->Sock, pContext->pBuffer, NumBytes, 0);
  if (r ==  SOCKET_ERROR) {
    IP_LOG((IP_MTYPE_APPLICATION, "TFTP: Could not send error msg. "));
    return r;
  }
  return ErrCode;
}

/*********************************************************************
*
*       _SendReq
*
*  Function description:
*    Sends a read / write request.
*
*  Notes:
*
*    Write request (WRQ) / Read request (RRQ):
*       2 bytes     string    1 byte     string   1 byte
*       ------------------------------------------------
*      | Opcode |  Filename  |   0  |    Mode    |   0  |
*       ------------------------------------------------
*
*    Opcode (Opcode / Operation):
*      1     Read request (RRQ)
*      2     Write request (WRQ)
*      3     Data (DATA)
*      4     Acknowledgment (ACK)
*      5     Error (ERROR)
*
*    Filename:
*        zero-terminated string.
*
*    Mode:
*      The mode field contains the string "netascii", "octet", or "mail"
*      (or any combination of upper and lower case, such as "NETASCII",
*      NetAscii", etc.) in netascii indicating the three modes defined in
*      the protocol.
*/
static int _SendReq(TFTP_CONTEXT * pContext, U32 ServerIP, U16 ServerPort, int ReqType, const char * sName, int Mode) {
  U16 * pHeader;
  char * pBuffer;
  unsigned Len;
  unsigned NumBytes;
  char * sAscii  = "netascii";
  char * sBinary = "octet";
  int r;
  struct sockaddr_in FAddr;

  //
  // Create a socket and connect to the socket to the server port
  //
  FAddr.sin_family      = AF_INET;
  FAddr.sin_port        = htons(ServerPort);
  FAddr.sin_addr.s_addr = htonl(ServerIP);
  NumBytes = 0;
  pBuffer = pContext->pBuffer;
  pHeader = (U16*)pBuffer;
  //
  // Fill packet.
  //
  *pHeader  = ntohs(ReqType);                   // Add Opcode
  NumBytes += 2;
  Len       = strlen(sName);                    // Add filename
  IP_MEMCPY(pBuffer + NumBytes, sName, Len);
  NumBytes += Len;
  *(pBuffer + NumBytes) = '\0';
  NumBytes++;
  IP_LOG((IP_MTYPE_APPLICATION, "TFTP: Send a request."));
  switch (Mode) {
  case 0:  // netascii
    IP_LOG((IP_MTYPE_APPLICATION, "TFTP: Using netascii mode."));
    IP_MEMCPY(pBuffer + NumBytes, sAscii, 8);
    NumBytes += 8;
    *(pBuffer + NumBytes) = '\0';
    NumBytes++;
    break;
  case 1:  // octet, binary
    IP_LOG((IP_MTYPE_APPLICATION, "TFTP: Using octet (binary) mode."));
    IP_MEMCPY(pBuffer + NumBytes, sBinary, 5);
    NumBytes += 5;
    *(pBuffer + NumBytes) = '\0';
    NumBytes++;
    break;
  }
  r = sendto(pContext->Sock, pBuffer, NumBytes, 0, (struct sockaddr*)&FAddr, sizeof(struct sockaddr_in));
  if (r == SOCKET_ERROR) {
    IP_LOG((IP_MTYPE_APPLICATION, "TFTP: Could not send request."));
    return r;
  }
  return 0;
}

/*********************************************************************
*
*       _SendData
*
*  Function description:
*    Sends data to a client. (Read request from a client)
*/
static int _SendData(TFTP_CONTEXT * pContext, void * hFile) {
  U16 *  pHeader;
  char * pBuffer;
  int    NumBytesAtOnce;
  U16    BlockCnt;
  int    FileLen;
  int    FilePos;
  int    r;
  int    SendNullBytePacket;

  SendNullBytePacket = 0;
  FileLen = pContext->pFS_API->pfGetLen(hFile);
  if ((FileLen % 512) == 0) {
    SendNullBytePacket = 1;
  }
  FilePos = 0;
  pContext->BlockCnt = 1;  // First data packet has always the block number '1'
  while (FileLen > 0) {
    //
    // Fill in TFTP header
    //
    pHeader      = (U16*)pContext->pBuffer;
    *pHeader     = ntohs(TFTP_DATA);
    *(pHeader+1) = ntohs(pContext->BlockCnt);
    //
    // Add payload
    //
    pBuffer        = pContext->pBuffer + 4;  // Data is stored after the 4 header bytes
    NumBytesAtOnce = 512;                    // 512 bytes can be transmitted in one TFTP package
    if (NumBytesAtOnce > FileLen) {
      NumBytesAtOnce = FileLen;
    }
    pContext->pFS_API->pfReadAt(hFile, pBuffer, FilePos, NumBytesAtOnce);
    FilePos += NumBytesAtOnce;
    FileLen -= NumBytesAtOnce;
    pBuffer  = pContext->pBuffer;
    r = send(pContext->Sock, pBuffer, NumBytesAtOnce + 4, 0);
    if (r == SOCKET_ERROR) {
      return SOCKET_ERROR;
    }
    //
    // Check if we receive an ACK
    //
    r = recv(pContext->Sock, pBuffer, pContext->BufferSize, 0);
    if (r == -1) {
      return SOCKET_ERROR;
    }
    if (*(pBuffer+1) == 0x04) {              // Is an ACK ?
      BlockCnt = *((U16*)(pBuffer+2));
      BlockCnt = ntohs(BlockCnt);
      if (BlockCnt == pContext->BlockCnt) {  // Is block number valid ?
        pContext->BlockCnt++;
      } else {
        IP_LOG((IP_MTYPE_APPLICATION, "TFTP: Invalid block number. Expected: %d. Received: %d.", pContext->BlockCnt, BlockCnt));
        //
        // TBD: Resend?
        //
      }
    }
  }
  if (SendNullBytePacket) {
    pHeader      = (U16*)pContext->pBuffer;
    *pHeader     = ntohs(TFTP_DATA);
    *(pHeader+1) = ntohs(pContext->BlockCnt);
    r = send(pContext->Sock, pBuffer, 4, 0);
  }
  return 0;
}

/*********************************************************************
*
*       _HandleReadReq
*
*  Function Description:
*    Handles a read request.
*
*  Notes:
*     2 bytes    string    1 byte    string    1 byte
*    ------------------------------------------------
*   | Opcode |  Filename  |   0  |    Mode    |   0  |
*    ------------------------------------------------
*/
static int _HandleReadReq(TFTP_CONTEXT * pContext) {
  char * sFileName;
  void * hFile;
  int v;

  //
  // Get file name
  //
  sFileName = pContext->pBuffer + 2;
  hFile = pContext->pFS_API->pfOpenFile(sFileName);
  if (hFile) {
    //
    // Send the data to the client.
    //
    v = _SendData(pContext, hFile);
    pContext->pFS_API->pfCloseFile(hFile);
    return v;
  } else {
    _SendErr(pContext, 1, TFTP_ERR_STR_FILE_NOT_FOUND);
    return -1;
  }
}

/*********************************************************************
*
*       _CheckHeader
*
*  Function description:
*    Checks if the UDP packet contains a TFTP data packet.
*/
static int _CheckHeader(TFTP_CONTEXT * pContext) {
  U16 BlockCnt;

  if (*(pContext->pBuffer+1) == 0x03) {  // Data packet ?
    //
    // Check if the block count is as expected.
    //
    BlockCnt = *((U16*)(pContext->pBuffer+2));
    BlockCnt = ntohs(BlockCnt);
    if (BlockCnt < pContext->BlockCnt) {
      IP_LOG((IP_MTYPE_APPLICATION, "TFTP: Invalid block number. Expected: %d. Received: %d.", pContext->BlockCnt, BlockCnt));
      goto Error; // TBD: Resend ?
    }
  } else {
Error:
    return -1;
  }
  return 0;
}

/*********************************************************************
*
*       _StoreData
*
*  Function description:
*    Checks if the UDP packet contains a TFTP data packet.
*/
static int _StoreData(TFTP_CONTEXT * pContext, void * hFile, int FilePos, int NumBytes) {
  char * pBuffer;

  pBuffer = pContext->pBuffer;
  //
  // Skip TFTP header
  //
  pBuffer  += 4;
  NumBytes -= 4;
  //
  // Store data
  //
  pContext->pFS_API->pfWriteAt(hFile, pBuffer, FilePos, NumBytes);
  FilePos += NumBytes;
  return FilePos;
}

/*********************************************************************
*
*       _RecvData
*
*  Function description:
*    Receives data from a client. (Write request from a client)
*/
static int _RecvData(TFTP_CONTEXT * pContext, void * hFile) {
  char * pBuffer;
  int  NumBytesAtOnce;
  int  FilePos;
  int  r;
  int  v;

  pBuffer = pContext->pBuffer;
  FilePos = 0;
  NumBytesAtOnce = pContext->BufferSize;
  while (1) {
    pBuffer = pContext->pBuffer;
    v = recv(pContext->Sock, pBuffer, NumBytesAtOnce, 0);
    if (v == -1) {
      return SOCKET_ERROR;
    }
    r = _CheckHeader(pContext);
    if (r == -1) {
      _SendErr(pContext, 0, TFTP_ERR_STR_NOT_DEFINED);
      return r;
    }
    FilePos = _StoreData(pContext, hFile, FilePos, v);
    if (v < 512) {
      _SendAck(pContext, 0);
      return FilePos;
    }
    _SendAck(pContext, 1);
  }
}

/*********************************************************************
*
*       _HandleWriteReq
*
*  Function description:
*    Handles a write request. -> Client writes to the server.
*
*  Notes:
*     2 bytes    string    1 byte    string    1 byte
*    ------------------------------------------------
*   | Opcode |  Filename  |   0  |    Mode    |   0  |
*    ------------------------------------------------
*/
static int _HandleWriteReq(TFTP_CONTEXT * pContext) {
  char * sFileName;
  void * hFile;
  int v;

  //
  // Get file name
  //
  sFileName = pContext->pBuffer + 2;
  hFile = pContext->pFS_API->pfCreate(sFileName);
  if (hFile) {
    //
    // Send first ACK and receive data
    //
    _SendAck(pContext, 1);
    v = _RecvData(pContext, hFile);
    pContext->pFS_API->pfCloseFile(hFile);
    return v;
  }
  _SendErr(pContext, 3, TFTP_ERR_STR_DISK_FULL);
  return -1;
}

/*********************************************************************
*
*       _CreateDataConn
*
*  Function description
*    Creates a data connection to exchange data with the client.
*/
static int _CreateDataConn(TFTP_CONTEXT * pContext, struct sockaddr * pFAddr) {
  //
  // Create socket for data transfer.
  //
  pContext->Sock = socket(AF_INET, SOCK_DGRAM, 0);
  if (pContext->Sock == SOCKET_ERROR) {
     IP_LOG((IP_MTYPE_APPLICATION, "TFTP: Could not create socket for data transmission."));
     return -1;
  }
  return connect(pContext->Sock, pFAddr, sizeof(struct sockaddr));
}

/*********************************************************************
*
*       _CloseDataConn
*
*  Function description
*    Closes the socket used for the data connection and resets the
*    connection related parts of the TFTP context.
*/
static void  _CloseDataConn(TFTP_CONTEXT * pContext) {
  closesocket(pContext->Sock);
  pContext->BlockCnt = 0;
  pContext->Sock     = 0;
}

/*********************************************************************
*
*       _IsACK()
*
*  Function description:
*    Checks if it is a valid ACK packet.
*
*  Return value:
*     0: Okay, packet valid.
*    -1: Error, packet is not an ACK.
*    -2: ACK packet valid, block number invalid
*/
static int _IsACK(TFTP_CONTEXT * pContext) {
  int BlockCnt;
  char * pBuffer;

  pBuffer = pContext->pBuffer;
  if (*(pBuffer+1) == 0x04) {              // Is an ACK ?
    BlockCnt = *((U16*)(pBuffer+2));
    BlockCnt = ntohs(BlockCnt);
    if (BlockCnt == pContext->BlockCnt) {  // Is block number valid ?
      pContext->BlockCnt++;
      return 0;
    } else {
      IP_LOG((IP_MTYPE_APPLICATION, "TFTP: Invalid block number. Expected: %d. Received: %d.", pContext->BlockCnt, BlockCnt));
      return -2;
    }
  }
  IP_LOG((IP_MTYPE_APPLICATION, "TFTP: Error. Received packet is not an ACK."));
  return -1;
}

/*********************************************************************
*
*       Public code
*
**********************************************************************
*/

/*********************************************************************
*
*       IP_TFTP_SendFile()
*
*  Function description:
*    Sends a data file to a TFTP server.
*/
int IP_TFTP_SendFile(TFTP_CONTEXT * pContext, U32 IPAddr, U16 Port, const char * sFileName, int Mode) {
  struct sockaddr_in FAddr;
  char * pBuffer;
  int    NumBytesAtOnce;
  void * hFile;
  int r;
  int Len;
  int NumBytesReceived;

  hFile = pContext->pFS_API->pfOpenFile(sFileName);
  if (hFile) {
    //
    // Create socket for data transfer.
    //
    pContext->Sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (pContext->Sock == SOCKET_ERROR) {  // In case of an error, close the file handle and return
      r = -1;
      goto Done;
    }
    r = _SendReq(pContext, IPAddr, Port, TFTP_WRQ, sFileName, Mode);
    if (r) {  // In case of an error, close the file handle and return
      goto Done;
    }
    pBuffer        = pContext->pBuffer;
    NumBytesAtOnce = pContext->BufferSize;
    //
    // Receive the first ACK packet and connect the socket to the server port.
    //
    FAddr.sin_family      = AF_INET;
    FAddr.sin_port        = htons(0xFFFF);
    FAddr.sin_addr.s_addr = htonl(0xFFFFFFFF);
    Len                   = sizeof(FAddr);
    NumBytesReceived = recvfrom(pContext->Sock, pBuffer, NumBytesAtOnce, 0, (struct sockaddr*)&FAddr, &Len);
    if (NumBytesReceived == SOCKET_ERROR) {
      goto Done;
    }
    r = connect(pContext->Sock, (struct sockaddr*)&FAddr, sizeof(struct sockaddr));
    if (r == SOCKET_ERROR) {
      goto Done;
    }
    r = _IsACK(pContext);
    if (r) {
      goto Done;
    }
    r = _SendData(pContext, hFile);
Done:
    pContext->pFS_API->pfCloseFile(hFile);
    _CloseDataConn(pContext);
    return r;
  } else {
    _SendErr(pContext, 1, TFTP_ERR_STR_FILE_NOT_FOUND);
    return -1;
  }
}

/*********************************************************************
*
*       IP_TFTP_RecvFile()
*
*  Function description:
*    Receives a data file from a TFTP server.
*/
int IP_TFTP_RecvFile(TFTP_CONTEXT * pContext, U32 IPAddr, U16 Port, const char * sFileName, int Mode) {
  struct sockaddr_in FAddr;
  void * hFile;
  char * pBuffer;
  int    NumBytesAtOnce;
  int    NumBytesReceived;
  int    r;
  int    Len;
  int    FilePos;

  //
  // Create socket for data transfer.
  //
  pContext->Sock = socket(AF_INET, SOCK_DGRAM, 0);
  if (pContext->Sock == SOCKET_ERROR) {  // In case of an error, close the file handle and return
    return -1;
  }
  r = _SendReq(pContext, IPAddr, Port, TFTP_RRQ, sFileName, Mode);
  pContext->BlockCnt++;  // Increment the initial block count, since first ACK after a request is send with block number 1.
  if (r) {  // In case of an error, close the file handle and return
    goto Done;
  }
  hFile          = pContext->pFS_API->pfCreate(sFileName);
  pBuffer        = pContext->pBuffer;
  NumBytesAtOnce = pContext->BufferSize;
  //
  // Receive the first data packet and connect the socket to the server port.
  //
  FAddr.sin_family      = AF_INET;
  FAddr.sin_port        = htons(0xFFFF);
  FAddr.sin_addr.s_addr = htonl(0xFFFFFFFF);
  Len                   = sizeof(FAddr);
  NumBytesReceived = recvfrom(pContext->Sock, pBuffer, NumBytesAtOnce, 0, (struct sockaddr*)&FAddr, &Len);
  if (NumBytesReceived == SOCKET_ERROR) {
    goto Done;
  }
  r = connect(pContext->Sock, (struct sockaddr*)&FAddr, sizeof(struct sockaddr));
  if (r == SOCKET_ERROR) {
    goto Done;
  }
  FilePos = 0;
  while (1) {
    r = _CheckHeader(pContext);
    if (r == -1) {
      _SendErr(pContext, 0, TFTP_ERR_STR_NOT_DEFINED);
      FilePos = -1;
      goto Done;
    }
    FilePos = _StoreData(pContext, hFile, FilePos, NumBytesReceived);
    if (NumBytesReceived < 516) {
      _SendAck(pContext, 0);
      goto Done;
    }
    _SendAck(pContext, 1);
    pBuffer = pContext->pBuffer;
    NumBytesReceived = recv(pContext->Sock, pBuffer, NumBytesAtOnce, 0);
    if (NumBytesReceived == -1) {
      FilePos = -1;
      goto Done;
    }
  }
Done:
  pContext->pFS_API->pfCloseFile(hFile);
  _CloseDataConn(pContext);
  return FilePos;
}

/*********************************************************************
*
*       IP_TFTP_InitContext()
*
*  Function description:
*    Initialize the TFTP context.
*
*  Parameter:
*    pContext   - TFTP context
*    pFS_API    - Pointer to a structure which contains mapping to the file system.
*    pBuffer    - Buffer for the TFTP data. The normal TFTP data packet is 516 bytes. 4 bytes header, 512 bytes of data.
*    BufferSize - Size of the data buffer
*    ServerPort - Port of the server. Can be 0 if the structure is used to cionnect as a client or if the default TFTP port (69) should be used.
*/
int IP_TFTP_InitContext(TFTP_CONTEXT * pContext, unsigned IFace, const IP_FS_API * pFS_API, char * pBuffer, int BufferSize, U16 ServerPort) {
  if ((BufferSize < 516) || (pBuffer == NULL)) {
    return -1;
  }
  memset(pContext, 0, sizeof (TFTP_CONTEXT));
  pContext->IFace      = IFace;
  pContext->pBuffer    = pBuffer;
  pContext->BufferSize = BufferSize;
  pContext->pFS_API    = pFS_API;
  if (ServerPort == 0) {
    pContext->ServerPort = 69;        // Set the well-known port 69 to wait for TFTP requests
  } else {
    pContext->ServerPort = ServerPort;
  }
  return 0;
}

/*********************************************************************
*
*       IP_TFTP_ServerTask()
*
*  Function description:
*    TFTP server thread.
*/
void IP_TFTP_ServerTask(void * pPara) {
  struct sockaddr_in LAddr;
  struct sockaddr_in FAddr;
  TFTP_CONTEXT * pContext;
  int Sock;
  int Len;
  int v;

  pContext = (TFTP_CONTEXT*) pPara;
  pContext->BlockCnt = 0;
  pContext->Sock     = 0;
  //
  // Create a socket to receive TFTP request...
  //
  Sock = socket(AF_INET, SOCK_DGRAM, 0);
  if (Sock == SOCKET_ERROR) {
    IP_LOG((IP_MTYPE_APPLICATION, "TFTP: Could not create socket for TFTP server."));
    return;
  }
  //
  // Bind socket to TFTP server port
  //
  memset(&LAddr, 0, sizeof(LAddr));
  LAddr.sin_family      = AF_INET;
  LAddr.sin_port        = htons(pContext->ServerPort);
  LAddr.sin_addr.s_addr = INADDR_ANY;
  bind(Sock, (struct sockaddr *)&LAddr, sizeof(LAddr));
  //
  // Wait for incoming TFTP requests
  //
  while (1) {
    //
    // Wait for UDP requests
    //
    FAddr.sin_family      = AF_INET;
    FAddr.sin_port        = htons(pContext->ServerPort);
    FAddr.sin_addr.s_addr = htonl(0xFFFFFFFF);
    Len                   = sizeof(FAddr);
    v = recvfrom(Sock, pContext->pBuffer, pContext->BufferSize, 0, (struct sockaddr*)&FAddr, &Len);
    if (v == SOCKET_ERROR) {
      continue;
    }
    //
    // Check if we have data received
    //
    if (Len > 0) {
      //
      // Handle request
      //
      if (*(pContext->pBuffer + 1) == TFTP_RRQ) {
        //
        // Handle read request
        //
        _CreateDataConn(pContext, (struct sockaddr*)&FAddr);
        _HandleReadReq(pContext);
        _CloseDataConn(pContext);
        continue;
      }
      if (*(pContext->pBuffer + 1) == TFTP_WRQ) {
        //
        // Handle write request
        //
        _CreateDataConn(pContext,(struct sockaddr*)&FAddr);
        _HandleWriteReq(pContext);
        _CloseDataConn(pContext);
        continue;
      }
      _SendErr(pContext, 4, TFTP_ERR_STR_ILLEGAL_OP);
    }
    IP_OS_Delay(100);
  }
}

/*************************** End of file ****************************/
