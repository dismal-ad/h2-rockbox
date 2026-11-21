/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
 * KIND, either express or implied.
 *
 ****************************************************************************/

/* lwIP configuration for Rockbox: one Wi-Fi station interface, IPv4 with
 * DHCP and DNS, TCP for HTTP(S) streams, UDP for DNS and SNTP. The core
 * runs in its own thread (tcpip); applications use the sockets API. */

#ifndef LWIPOPTS_H
#define LWIPOPTS_H

/* --- OS --- */
#define NO_SYS                      0
#define SYS_LIGHTWEIGHT_PROT        1
#define LWIP_TCPIP_CORE_LOCKING     1
#define LWIP_TCPIP_CORE_LOCKING_INPUT 0
#define TCPIP_THREAD_NAME           "tcpip"
#define TCPIP_THREAD_STACKSIZE      (12 * 1024)
#define TCPIP_THREAD_PRIO           0   /* sys_thread_new picks the priority */
#define TCPIP_MBOX_SIZE             64
#define DEFAULT_THREAD_STACKSIZE    (8 * 1024)
#define DEFAULT_RAW_RECVMBOX_SIZE   8
#define DEFAULT_UDP_RECVMBOX_SIZE   16
#define DEFAULT_TCP_RECVMBOX_SIZE   64
#define DEFAULT_ACCEPTMBOX_SIZE     4

/* --- memory: everything from the network heap (TLSF) --- */
#define MEM_LIBC_MALLOC             0
#define MEM_CUSTOM_ALLOCATOR        1
#define MEMP_MEM_MALLOC             1
#define MEM_ALIGNMENT               4
#define MEM_SIZE                    (256 * 1024) /* unused: custom allocator */
void *net_malloc(unsigned long size);
void *net_calloc(unsigned long n, unsigned long size);
void  net_free(void *p);
#define MEM_CUSTOM_MALLOC(s)        net_malloc(s)
#define MEM_CUSTOM_CALLOC(n, s)     net_calloc(n, s)
#define MEM_CUSTOM_FREE(p)          net_free(p)
#define PBUF_POOL_SIZE              48
#define PBUF_POOL_BUFSIZE           1600
#define MEMP_NUM_PBUF               64
#define MEMP_NUM_TCP_PCB            8
#define MEMP_NUM_TCP_PCB_LISTEN     2
#define MEMP_NUM_TCP_SEG            64
#define MEMP_NUM_UDP_PCB            6
#define MEMP_NUM_NETBUF             32
#define MEMP_NUM_NETCONN            10
#define MEMP_NUM_SYS_TIMEOUT        16

/* --- protocols --- */
#define LWIP_IPV4                   1
#define LWIP_IPV6                   0
#define LWIP_ARP                    1
#define LWIP_ETHERNET               1
#define ETH_PAD_SIZE                0
#define ARP_TABLE_SIZE              8
#define ARP_QUEUEING                1
#define LWIP_ICMP                   1
#define LWIP_RAW                    0
#define LWIP_DHCP                   1
#define LWIP_DHCP_DOES_ACD_CHECK    0
#define LWIP_ACD                    0
#define LWIP_AUTOIP                 0
#define LWIP_IGMP                   0
#define LWIP_DNS                    1
#define DNS_MAX_SERVERS             2
#define DNS_TABLE_SIZE              8
#define LWIP_DHCP_GET_NTP_SRV       0
#define LWIP_UDP                    1
#define LWIP_TCP                    1
#define IP_REASSEMBLY               1
#define IP_FRAG                     0
#define IP_REASS_MAX_PBUFS          16

/* --- TCP: a fast receive window for streams --- */
#define TCP_MSS                     1460
#define TCP_WND                     (22 * TCP_MSS)
#define TCP_SND_BUF                 (6 * TCP_MSS)
#define TCP_SND_QUEUELEN            ((4 * (TCP_SND_BUF) + (TCP_MSS - 1)) / (TCP_MSS))
#define TCP_QUEUE_OOSEQ             1
#define TCP_OOSEQ_MAX_PBUFS         16
#define LWIP_TCP_SACK_OUT           0
#define LWIP_TCP_KEEPALIVE          1
#define LWIP_TCP_TIMESTAMPS         0
#define TCP_LISTEN_BACKLOG          0
#define LWIP_WND_SCALE              0

/* --- netif --- */
#define LWIP_NETIF_HOSTNAME         1
#define LWIP_NETIF_STATUS_CALLBACK  1
#define LWIP_NETIF_LINK_CALLBACK    1
#define LWIP_NETIF_API              1
#define LWIP_HAVE_LOOPIF            0
#define LWIP_NETIF_LOOPBACK         0
#define LWIP_SINGLE_NETIF           1
#define LWIP_CHECKSUM_ON_COPY       0

/* --- API --- */
#define LWIP_NETCONN                1
#define LWIP_SOCKET                 1
#define LWIP_COMPAT_SOCKETS         0
#define LWIP_POSIX_SOCKETS_IO_NAMES 0
#define LWIP_SOCKET_SELECT          0
#define LWIP_SOCKET_POLL            0
#define LWIP_SO_RCVTIMEO            1
#define LWIP_SO_SNDTIMEO            1
#define LWIP_SO_RCVBUF              0
#define LWIP_SO_LINGER              0
#define SO_REUSE                    0
#define LWIP_TIMEVAL_PRIVATE        1
#define LWIP_ERRNO_STDINCLUDE       1
#define LWIP_DNS_API_DECLARE_H_ERRNO 1
#define LWIP_FIONREAD_LINUXMODE     0
#define LWIP_NETCONN_SEM_PER_THREAD 0
#define LWIP_MPU_COMPATIBLE         0

/* --- SNTP (time for certificate checks) --- */
#define SNTP_SERVER_DNS             1
#define SNTP_MAX_SERVERS            2
#define SNTP_UPDATE_DELAY           (3600 * 1000)
#define SNTP_STARTUP_DELAY          0
#define SNTP_CHECK_RESPONSE         1
void net_sntp_set_time(unsigned long sec, unsigned long us);
#define SNTP_SET_SYSTEM_TIME_US(sec, us) net_sntp_set_time((sec), (us))
#define sntp_format_time(sec)       ""

/* --- random --- */
unsigned long net_random(void);
#define LWIP_RAND()                 ((u32_t)net_random())

/* --- debugging and statistics: off --- */
#define LWIP_STATS                  0
#define LWIP_STATS_DISPLAY          0
#define LWIP_DEBUG                  0
#define LWIP_PERF                   0

#endif /* LWIPOPTS_H */
