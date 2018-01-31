/*********************************************************************
*
*   IAR PowerPac
*
*   (c) Copyright IAR Systems 2010.  All rights reserved.
*
**********************************************************************
----------------------------------------------------------------------
File        : IP_Config_AT91SAM9260.c
Purpose     : Configuration file for TCP/IP with ATMEL AT91SAM9260
---------------------------END-OF-HEADER------------------------------
*/

#include <stdio.h>
#include "IP.h"
#include "IP_NI_AT91SAM9260.h"

/*********************************************************************
*
*       Defines, configurable
*
**********************************************************************
*/
#define ALLOC_SIZE                 0x20000      // Size of memory dedicated to the stack in bytes

/*********************************************************************
*
*       Static data
*
**********************************************************************
*/
static U32 _aPool[ALLOC_SIZE / 4];                    // This is the memory area used by the stack.

/*********************************************************************
*
*       Public code
*
**********************************************************************
*/

/*********************************************************************
*
*       IP_X_Config
*
*  Function description
*    This function is called by the IP stack during IP_Init().
*
*  Typical memory/buffer configurations:
*    Microcontroller system, size optimized
*      #define ALLOC_SIZE 0x3000                    // 12KBytes RAM
*      mtu = 576;                                   // 576 is minimum acc. to RFC, 1500 is max. for Ethernet
*      IP_SetMTU(0, mtu);                           // Maximum Transmission Unit is 1500 for ethernet by default
*      IP_AddBuffers(8, 256);                       // Small buffers.
*      IP_AddBuffers(4, mtu + 16);                  // Big buffers. Size should be mtu + 16 byte for ethernet header (2 bytes type, 2*6 bytes MAC, 2 bytes padding)
*      IP_ConfTCPSpace(1 * (mtu-40), 1 * (mtu-40)); // Define the TCP Tx and Rx window size
*
*    Microcontroller system, speed optimized or multiple connections
*      #define ALLOC_SIZE 0x6000                    // 24 KBytes RAM
*      mtu = 1500;                                  // 576 is minimum acc. to RFC, 1500 is max. for Ethernet
*      IP_SetMTU(0, mtu);                           // Maximum Transmission Unit is 1500 for ethernet by default
*      IP_AddBuffers(12, 256);                      // Small buffers.
*      IP_AddBuffers(6, mtu + 16);                  // Big buffers. Size should be mtu + 16 byte for ethernet header (2 bytes type, 2*6 bytes MAC, 2 bytes padding)
*      IP_ConfTCPSpace(3 * (mtu-40), 3 * (mtu-40)); // Define the TCP Tx and Rx window size
*
*    System with lots of RAM
*      #define ALLOC_SIZE 0x20000                   // 128 KBytes RAM
*      mtu = 1500;                                  // 576 is minimum acc. to RFC, 1500 is max. for Ethernet
*      IP_SetMTU(0, mtu);                           // Maximum Transmission Unit is 1500 for ethernet by default
*      IP_AddBuffers(50, 256);                      // Small buffers.
*      IP_AddBuffers(50, mtu + 16);                 // Big buffers. Size should be mtu + 16 byte for ethernet header (2 bytes type, 2*6 bytes MAC, 2 bytes padding)
*      IP_ConfTCPSpace(5 * (mtu-40), 5 * (mtu-40)); // Define the TCP Tx and Rx window size
*/
void IP_X_Config(void) {
  int mtu;

  IP_AssignMemory(_aPool, sizeof(_aPool));      // Assigning memory should be the first thing
  IP_AddEtherInterface(&IP_Driver_SAM9260);     // Add ethernet driver for your hardware
  IP_SetHWAddr("\x00\x1F\x55\x01\x02\x03");     // MAC addr: Needs to be unique for production units
  //
  // Use DHCP client or define IP address, subnet mask,
  // gateway address and DNS server according to the
  // requirements of your application.
  //
  //IP_DHCPC_Activate(0, "TARGET", NULL, NULL);
  IP_SetAddrMask(0xC0A80164, 0xFFFFFF00);       // Assign IP addr. (e.g. 192.168.1.100) and subnet mask (e.g. 255.255.255.0)
  IP_SetGWAddr(0, 0xC0A80101);                  // Set gateway address, for example 192.168.1.1
  IP_DNS_SetServer(0xCA60D105);                 // Set DNS server address, for example 202.96.209.5
  //
  // Run-time configure buffers.
  // The default setup will do for most cases.
  //
  mtu = 1500;
  IP_SetMTU(0, mtu);                            // Maximum Transmission Unit is 1500 for ethernet by default
  IP_AddBuffers(50, 256);                       // Small buffers.
  IP_AddBuffers(50, mtu + 16);                  // Big buffers. Size should be mtu + 16 byte for ethernet header (2 bytes type, 2*6 bytes MAC, 2 bytes padding)
  IP_ConfTCPSpace(5 * (mtu-40), 5 * (mtu-40));  // Define the TCP Tx and Rx window size
  IP_SOCKET_SetDefaultOptions(0
//                              | SO_TIMESTAMP  // Send TCP timestamp to optimize the round trip time measurement. Normally not used in LAN.
                             );
  //
  // Define log and warn filter
  // Note: The terminal I/O emulation affects the timing
  // of your communication, since the debugger stops the target
  // for every terminal I/O output unless you use DCC!
  //
  IP_SetWarnFilter(0xFFFFFFFF);                 // 0xFFFFFFFF: Do not filter: Output all warnings.
  IP_SetLogFilter(IP_MTYPE_INIT               // Output all messages from init
                | IP_MTYPE_LINK_CHANGE        // Output a message if link status changes
//                | IP_MTYPE_DHCP               // Output general DHCP status messages
//                | IP_MTYPE_DHCP_EXT           // Output additional DHCP messages
//                | IP_MTYPE_CORE               // Output log messages from core module
//                | IP_MTYPE_ALLOC              // Output log messages for memory allocation
//                | IP_MTYPE_DRIVER             // Output log messages from driver
//                | IP_MTYPE_ARP                // Output log messages from ARP layer
//                | IP_MTYPE_IP                 // Output log messages from IP layer
//                | IP_MTYPE_TCP_CLOSE          // Output a log messages if a TCP connection has been closed
//                | IP_MTYPE_TCP_OPEN           // Output a log messages if a TCP connection has been opened
//                | IP_MTYPE_TCP_IN             // Output TCP input logs
//                | IP_MTYPE_TCP_OUT            // Output TCP output logs
//                | IP_MTYPE_TCP_RTT            // Output TCP round trip time (RTT) logs
//                | IP_MTYPE_TCP_RXWIN          // Output TCP RX window related log messages
//                | IP_MTYPE_TCP                // Output all TCP related log messages
//                | IP_MTYPE_UDP_IN             // Output UDP input logs
//                | IP_MTYPE_UDP_OUT            // Output UDP output logs
//                | IP_MTYPE_UDP                // Output all UDP related messages
//                | IP_MTYPE_ICMP               // Output ICMP related log messages
//                | IP_MTYPE_NET_IN             // Output network input related messages
//                | IP_MTYPE_NET_OUT            // Output network output related messages
//                | IP_MTYPE_DNS                // Output all DNS related messages
//                | IP_MTYPE_SOCKET_STATE       // Output socket status messages
//                | IP_MTYPE_SOCKET_READ        // Output socket read related messages
//                | IP_MTYPE_SOCKET_WRITE       // Output socket write related messages
//                | IP_MTYPE_SOCKET             // Output all socket related messages
                | IP_MTYPE_APPLICATION
                );
}

/*************************** End of file ****************************/
