/*
 * $Id$
 *
 * aprsd, Automatic Packet Reporting System Daemon
 * Copyright (C) 1997,2001 Dale A. Heatherington, WA4DSY
 * Copyright (C) 2001 aprsd Dev Team
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * Look at the README for more information on the program.
 */

/* Based on serial.cpp
   Implements the same interface as serial.cpp (as specified
   in serial.h), but uses Linux network sockets.
*/

#include "config.h"

#ifdef HAVE_LIBAX25                     // if no AX25, do nothing



#include <cstdio>
#include <cstdlib>
#include <sys/poll.h>                   // poll, et el
#include <net/if.h>
#include <netinet/if_ether.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/types.h>                      // isprint
#include <netax25/axlib.h>
#include <netax25/axconfig.h>

#include <string>
#include <iostream>

#include "osdep.h"
#include "ax25socket.h"
#include "constant.h"
#include "rf.h"

using std::cerr;
using std::endl;
using std::cout;

//---------------------------------------------------------------------
// AX.25 constants

#define ALEN            6
#define AXLEN           7

/* SSID mask and various flags that are stuffed into the SSID byte */
#define SSID            0x1E
#define HDLCAEB         0x01
#define REPEATED        0x80

#define UI              0x03       // unnumbered information (unproto)
#define PID_NO_L3       0xF0       // no level-3 (text)

//---------------------------------------------------------------------
// AX.25 variables

char* ax25port;                    // Name of port to use
char* ax25dev;                     // Associated device name
int rx_socket, tx_socket;          // RX and TX socket descriptors
int proto = ETH_P_ALL;             // Protocol to use for receive

struct full_sockaddr_ax25 tx_dest; // TX destination address
struct full_sockaddr_ax25 tx_src;  // TX source address
int tx_src_len, tx_dest_len;       // Length of addresses

//---------------------------------------------------------------------
// AX.25 function prototypes

void fmt (const unsigned char*, int, unsigned char**);
char* pax25 (char*, const unsigned char*);

//---------------------------------------------------------------------
// Open the AX.25 sockets

int SocketOpen(const string& rfport, const char *destcall)
{
    char* portcall;

    if (destcall == NULL) {
        cerr << "aprsd: no APRSPATH specified, required for sockets" << endl;
        return 1;
    }

    // Open network sockets here
    ax25port = strdup(rfport.c_str());

    if (ax25_config_load_ports() == 0)
        cerr << "aprsd: no AX.25 port data configured" << endl;

    if ((ax25dev = ax25_config_get_dev(ax25port)) == NULL) {
        cerr << "aprsd: invalid port name - " << ax25port << endl;
        return 1;
    }

    // Create the receive socket
    if ((rx_socket = socket(PF_PACKET, SOCK_PACKET, htons(proto))) == -1) {
        perror("socket");
        return 1;
    }

    // Create the transmit socket
    if ((tx_socket = socket(PF_AX25, SOCK_DGRAM, 0)) == -1) {
        perror("socket");
        return 1;
    }

    // Get the port's callsign
    if ((portcall = ax25_config_get_addr(ax25port)) == NULL) {
        cerr << "invalid AX.25 port setting - " << ax25port << endl;
        return 1;
    }

    // Convert to AX.25 addresses
    if ((tx_src_len = ax25_aton(portcall, &tx_src)) == -1) {
        cerr << "unable to convert callsign '" << portcall << "'" << endl;
        return 1;
    }

    if ((tx_dest_len = ax25_aton(destcall, &tx_dest)) == -1) {
        cerr << "unable to convert callsign '" << destcall << "'" << endl;
        return 1;
    }

    // Bind the socket
    if (bind(tx_socket, (struct sockaddr *)&tx_src, tx_src_len) == -1) {
        perror("bind");
        return 1;
    }

    return 0;

}

//--------------------------------------------------------------------
int SocketClose (void)
{

    // nothing to do here
    return 0;

}

//--------------------------------------------------------------------
// This is the socket read thread.
// It polls the socket, and returns any data received
// (after reformatting it to raw TNC format).

bool SocketReadWrite(char buf[])
{
    bool lineTimeout;
    struct pollfd pfd;
    struct sockaddr sa;
    struct ifreq ifr;
    int asize;
    int result;
    int size;
    unsigned char rxbuf[1500];
    unsigned char *textbuf;

    lineTimeout = false;

    do {

        if (txrdy) {        //Check for data to Transmit
            sendto(tx_socket, tx_buffer, strlen(tx_buffer), 0,
                (struct sockaddr *)&tx_dest, tx_dest_len);
            txrdy = false;      //Indicate we sent it.
        }


        pfd.fd = rx_socket;
        pfd.events = 0x040;             // Read normal data -- should be POLLRDNORM

        result = poll(&pfd, 1, 100);    // Time-out after 100ms

        if (result == 1) {
            asize = sizeof(sa);

            if ((size = recvfrom(rx_socket, rxbuf, sizeof(rxbuf), 0, &sa, (socklen_t*)&asize)) == -1) {
                perror("recv");
                result = -1;

            } else {

                if (ax25dev != NULL && strcmp(ax25dev, sa.sa_data) != 0)
                    result = 0;

                if (proto == ETH_P_ALL) {       // promiscuous mode?
                    strcpy(ifr.ifr_name, sa.sa_data);
                    if (ioctl(rx_socket, SIOCGIFHWADDR, &ifr) < 0
                        || ifr.ifr_hwaddr.sa_family != AF_AX25)
                        result = 0;             // not AX25 so ignore this packet
                }

                if (result == 1) {
                    fmt(rxbuf, size, &textbuf);     // convert to text
                    strncpy(buf, (char*)textbuf, BUFSIZE);

                }
            }
        }

        if (result < 0)         // input error
            lineTimeout = true; // Error has occurred

    } while (result == 0);
    return lineTimeout;
}


//---------------------------------------------------------------------
// fmt converts a received packet into a TNC message string

void fmt(const unsigned char *buf, int len, unsigned char **outbuf)
{
    static unsigned char buf1[1000];
    char from[10], to[10], digis[100];
    int i, hadlast, l;
    char tmp[15];

    *buf1 = '\0';
    *outbuf = buf1;

    if ((buf[0] & 0xf) != 0)    // not a kiss data frame
        return;

    ++buf;
    --len;

    if (len < (AXLEN + ALEN + 1))   // too short
        return;

    pax25 (to, buf);                // to call
    pax25 (from, &buf[AXLEN]);      // from call

    buf += AXLEN;                   // skip over the from call now...
    len -= AXLEN;
    *digis = '\0';
    if (!(buf[ALEN] & HDLCAEB)) {   // is there a digipeater path?
        for (l = 0, buf += AXLEN, len -= AXLEN, i = 0;
                i < 6 && len >= 0; i++, len -= AXLEN, buf += AXLEN) {
            int nextrept = buf[AXLEN + ALEN] & REPEATED;
            if (buf[ALEN] & HDLCAEB)
                nextrept = 0;

            pax25 (tmp, buf);

            if (*tmp) {
                sprintf (&digis[l], ",%s%s", tmp, (buf[ALEN] & REPEATED && !nextrept) ? "*" : "");
                ++hadlast;
                l += strlen (&digis[l]);
            }
            if (buf[ALEN] & HDLCAEB)
                break;
        }
    }
    buf += AXLEN;
    len -= AXLEN;
    if (len <= 0)
        return;     // no data after callsigns

    if (*buf++ == UI && *buf++ == PID_NO_L3) {
        len -= 2;
    } else {
        return;     // must have UI text to be useful
    }

    // No rewriting for mic-e frames because aprsd does this later
    sprintf ((char*)buf1, "%s>%s%s:", from, to, digis);
    l = strlen ((char*)buf1);
    for (i = 0; i < len; i++, l++) {
        buf1[l] = (isprint (buf[i])) ? buf[i] : ' ';    // keep it clean
    }

    buf1[l++] = 0x0d;
    buf1[l++] = 0x0a;

    buf1[l] = '\0';
    return;
}

//---------------------------------------------------------------------
// pax25 formats an AX.25 callsign.

char * pax25 (char *buf, const unsigned char *data)
{
    int i, ssid;
    char *s;
    char c;

    s = buf;

    for (i = 0; i < ALEN; i++) {
        c = (data[i] >> 1) & 0x7F;

        if (c != ' ')
            *s++ = c;
    }

    if ((ssid = (data[ALEN] & SSID) >> 1) != 0)
        sprintf (s, "-%d", ssid);
    else
        *s = '\0';

    return (buf);
}

#endif // HAVE_LIBAX25
