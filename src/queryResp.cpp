/*
 * $Id$
 *
 * aprsd, Automatic Packet Reporting System Daemon
 * Copyright (C) 1997,2002 Dale A. Heatherington, WA4DSY
 * Copyright (C) 2001-2002 aprsd Dev Team
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


#include <unistd.h>
#include <string>
#include <ctime>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <netdb.h>
#include <pthread.h>

#include <fstream>
#include <iostream>
#include <strstream>
#include <iomanip>
#include <cerrno>

#include <netinet/in.h>
#include <arpa/inet.h>

#include "constant.hpp"
#include "utils.hpp"
#include "aprsString.hpp"
#include "queryResp.hpp"
#include "history.hpp"
#include "servers.hpp"
#include "mutex.hpp"

using namespace std;
using namespace aprsd;

char szHostIP[HOSTIPSIZE];
int queryCounter;



void queryResp(int source, const aprsString* pkt)
{
    int rc;
    struct hostent *h = NULL;
    struct hostent hostinfo_d;
    char h_buf[512];
    int h_err;
    Lock countLock(pmtxCount, false);

    aprsString *rfpacket, *inetpkt;
    char* hostname = new char[80];
    unsigned char hip[5];
    char* cp = new char[256];
    char* cpAck = new char[256];
    memset(cp,0,256);
    memset(cpAck,0,256);
    ostrstream reply(cp,255);
    ostrstream ack(cpAck,255);


    for (int i=0;i<4;i++)
        hip[i] = 0;

    /* Get hostname and host IP */
    if ((rc = gethostname(hostname,80)) != 0)
        strcpy(hostname, "Host_Unknown");
    else {

        //Thread-Safe verison of gethostbyname()
        h = NULL;
        rc = gethostbyname_r(hostname,
                                &hostinfo_d,
                                h_buf,
                                512,
                                &h,
                                &h_err);



        if ((rc == 0) && (h!= NULL)) {
            strncpy(hostname,h->h_name,80);            //Copy Full host name
            hostname[79] = '\0';                      //Be sure it's terminated
            strncpy((char*)hip,h->h_addr_list[0],4);   //Copy Host IP
        }
    }


    /*debug*/
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

    /* Now build the query specfic packet.
      Only "IGATE" supported at this time. */

    if (pkt->query.compare("IGATE") == 0) {
        //pthread_mutex_lock(pmtxCount) ;
        countLock.get();
        queryCounter++;                   //Count this query
        //pthread_mutex_unlock(pmtxCount);
        countLock.release();

        reply << szServerCall << ">" << PGVERS << ",TCPIP*:"
            << "<IGATE,"
            << "MSG_CNT="
            << msg_cnt << ","
            << "LOC_CNT="
            << localCount() << ","    /* <--  scan history lists for local items */
            << "CALL="
            << MyCall << ","
            << "LOCATION="
            << MyLocation << ","
            << "HOST="
            << hostname << ","
            << "IP="
            << (int)hip[0] << "."
            << (int)hip[1] << "."
            << (int)hip[2] << "."
            << (int)hip[3] << ","
            << "EMAIL="
            << MyEmail << ","
            << "VERS="
            << VERS
            << "\r\n"
            << ends;

        switch (source) {
            case SRC_IGATE:
                inetpkt = new aprsString(cp, SRC_INTERNAL, srcBEACON);
                inetpkt->addIIPPE('I');   //Make it an Internal packet
                inetpkt->addPath(MyCall,'*');
                sendQueue.write(inetpkt); //DeQueue() deletes the *msgbuf
                break;

            case SRC_TNC:
                rfpacket = new aprsString(cp);   // Query reply
                //cout << rfpacket << endl;      //debug
                tncQueue.write(rfpacket);        // queue read deletes rfpacket
                break;

            default:
                if (source >= 0) {
                    inetpkt = new aprsString(cp, SRC_INTERNAL, srcBEACON);
                    inetpkt->addIIPPE('I');   //Make it an Internal packet
                    inetpkt->addPath(MyCall,'*');
                    rc = send(source,(const void*)inetpkt->c_str(),strlen(inetpkt->c_str()),0);  //Only to one user
                    delete inetpkt;
                }
        }

    }
    delete cp;
    delete cpAck;
    delete hostname;
}

