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

 
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <unistd.h>                     // gethostname
#include <sys/socket.h>                 // send
#include <netdb.h>                      // gethostbyname2_r
#include <strstream>

#include "osdep.h"
#include "constant.h"
#include "utils.h"
#include "aprsString.h"
#include "queryResp.h"

using namespace std;

extern void BroadcastString(const char *cp);

extern char* szAPRSDPATH ;
extern string szServerCall;
extern string MyLocation;
extern string MyCall;
extern string MyEmail;

extern int msgsn;
extern cpQueue tncQueue;
extern RecursiveMutex mtxCount;
extern RecursiveMutex mtxDNS;

char szHostIP[HOSTIPSIZE];
int queryCounter;


void queryResp(int session, const TAprsString* pkt)
{
    int rc;
    struct hostent *hostinfo = NULL;
    struct hostent hostinfo_d;
    char h_buf[512];
    int h_err;
    Lock dnsLock(mtxDNS, false);
    Lock countLock(mtxCount, false);

    TAprsString *rfpacket, *ackRFpacket;
    char* hostname = new char[180];
    unsigned char hip[5];
    char* cp = new char[256];
    char* cpAck = new char[256];
    memset(cp,0,256);
    memset(cpAck,0,256);
    ostrstream reply(cp, 255);
    ostrstream ack(cpAck, 255);
    bool wantAck = false;

    for (int i = 0; i < 4; i++)
        hip[i] = 0;

    // Get hostname and host IP
    if ((rc = gethostname(hostname, 179)) < 0 )
        strcpy(hostname,"Host_Unknown");
    else {
        dnsLock.get();
        // Thread-Safe verison of gethostbyname2() ?  Actually it's not so lock after all
        if ((rc = gethostbyname_r(hostname, &hostinfo_d, h_buf, sizeof(h_buf), &hostinfo, &h_err)) < 0 ){
            if (hostinfo != NULL) {
                strncpy(hostname, hostinfo->h_name, 80);             // Full host name
                strncpy((char*)hip, hostinfo->h_addr_list[0], 4);    // Host IP
            } else
                return;     // gethost failed so bail out.
        }
    }

    // debug
   /*
   cout  << "Hostname: " << hostname << endl
         << "IP: "
         << (int)hip[0] << "."
         << (int)hip[1] << "."
         << (int)hip[2] << "."
         << (int)hip[3]
         <<endl;
    */

    //cout << pkt << endl;

    // Send ack only if there was a line number on the end of the query.
    if (pkt->acknum.length() == 0)
        wantAck = false;
    else
        wantAck = true;

    char sourceCall[] = "         ";    // 9 blanks
    int i = 0;

    while ((i <9) && (pkt->ax25Source.c_str()[i] != '\0')) {
        sourceCall[i] = pkt->ax25Source.c_str()[i];
        i++;
    }

    if (wantAck) {                      // construct an ack packet
        ack << pkt->stsmDest << szAPRSDPATH << ":"
            << sourceCall << ":ack"
            << pkt->acknum << endl;    // use senders sequence number
    }

    // Now build the query specfic packet(s)
    if (pkt->query.compare("IGATE") == 0) {
        countLock.get();
        queryCounter++;                 // Count this query
        countLock.release();
        reply << szServerCall << szAPRSDPATH << ":"
            << sourceCall << ":"
            << MyCall << " "
            << MyLocation << " "
            << hostname << " "
            << (int)hip[0] << "."
            << (int)hip[1] << "."
            << (int)hip[2] << "."
            << (int)hip[3] << " "
            << MyEmail << " "
            << VERS
            << "\r\n"                   // Don't send a sequence number!
            << ends;

        switch (session) {
            case SRC_IGATE:
                if (wantAck)            // To the whole aprs network
                    BroadcastString(cpAck);

                BroadcastString(cp);
                break;

            case SRC_TNC:
                if (wantAck) {
                    ackRFpacket = new TAprsString(cpAck);  // Ack reply
                    ackRFpacket->stsmReformat(MyCall);
                    tncQueue.write(ackRFpacket);
                }

                rfpacket = new TAprsString(cp);     // Query reply
                rfpacket->stsmReformat(MyCall);     // Reformat it for RF delivery
                //cout << rfpacket << endl;         //debug
                tncQueue.write(rfpacket);           // queue read deletes rfpacket

                break;


            default:
                if (session > 0) {
                    if (wantAck)
                        rc = send(session, (const void*)cpAck, strlen(cpAck), 0);  //Only to one user

                    rc = send(session, (const void*)cp, strlen(cp), 0);
                }
        }
    }

    delete[] cp;
    cp = NULL;
    delete[] cpAck;
    cpAck = NULL;
    delete[] hostname;
    hostname = NULL;
}

// eof: queryResp.cpp
