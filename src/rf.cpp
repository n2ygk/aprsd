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

#include "config.h"

#include <unistd.h>                     // getpid
#include <iostream>

#include "rf.hpp"
#include "aprsd.hpp"
#include "servers.hpp"
#include "serial.hpp"
#include "ax25socket.hpp"
#include "aprsString.hpp"
#include "osdep.hpp"
#include "constant.hpp"
#include "utils.hpp"
#include "cpqueue.hpp"
#include "history.hpp"
#include "queryResp.hpp"

using namespace std;
using namespace aprsd;

pthread_t tidReadCom;
Mutex mtxWriteTNC;

char tx_buffer[260];
bool txrdy;

int CloseAsync, threadAck;
bool TncSysopMode;                      // Set true when sysop wants direct TNC access

//int AsyncPort;
bool AsyncPort;
string AprsPath;
char* ComBaud;

//--------------------------------------------------------------------
// Set APRS path (before the port is opened)
//
void rfSetPath(const string& path)
{
    AprsPath = path;
}

//--------------------------------------------------------------------
// Set serial port speed (before the port is opened)
//
void rfSetBaud(const char* baud)
{
    ComBaud = strdup(baud);
}


//--------------------------------------------------------------------
// Open RF ports (TNC or sockets)

int rfOpen(const string& szPort, const string& baudrate)
{
    int result;
    AsyncPort = (szPort[0] == '/');
    TncSysopMode = false;
    txrdy = false;
    APIRET rc;

    //cout << "Debug: szPort == " << szPort << endl;
    
    if (AsyncPort) {
        //cout << "AsyncPort is true" << endl;
        result = AsyncOpen(szPort, baudrate);
    }
#ifdef HAVE_LIBAX25
    else {
        //cout << "AsyncPort is false, using AX25" << endl;
        result = SocketOpen(szPort, AprsPath);
    }
#else
    else {
        cerr << "AX.25 sockets are not supported by this executable" << endl;
        result = 1;
    }
#endif

    if (result != 0)
        return(result);

    CloseAsync = false;
    threadAck = false;

    // Now start the serial port reader thread

    if ((rc = pthread_create(&tidReadCom, NULL, rfReadCom, NULL)) < 0) {
        cerr << "Error: ReadCom thread creation failed. Error code = " << rc << endl;
        CloseAsync = true;
    }
    return(0);
}

//--------------------------------------------------------------------
// Close the RF ports
//
int rfClose(void)
{
    CloseAsync = true;                  // Tell the read thread to quit
    
    while (!threadAck)
        reliable_usleep (1000);                  // wait till it does

     if (AsyncPort)
        return(AsyncClose());
#ifdef HAVE_LIBAX25
    else
        return(SocketClose());
#else
    else
        return(0);      // quite compiler
#endif
}

//-------------------------------------------------------------------
// Write NULL terminated string to serial port */
//
int rfWrite(const char *cp)
{
    int rc = 0;
    Lock writeTNCLock(mtxWriteTNC);

    strncpy(tx_buffer, cp, 256);
    txrdy = true;
    while (txrdy)
        reliable_usleep(10000);                  // The rfReadCom thread will clear txrdy when it takes data

    writeTNCLock.release();
    return(rc);
}

//--------------------------------------------------------------------
// This is the RF port read thread.
// It also handles buffered writes to the port.
//
void* rfReadCom(void *vp)
{
    unsigned short i;
    // APIRET rc;
    char buf[BUFSIZE];
    // unsigned char c;
    // FILE *rxc = (FILE *) vp;
    // size_t BytesRead;
    bool lineTimeout = false;
    aprsString *abuff;

    i = 0;

    pidlist.SerialInp = getpid ();
    //cerr << "Async Comm thread started." << endl;
    //cerr << "Debug: AsyncPort is " << (AsyncPort ? "true":"false") << endl;
    while (!CloseAsync) {
        if (AsyncPort)
            lineTimeout = AsyncReadWrite(buf);
#ifdef HAVE_LIBAX25
        else {
            //cout << "DEBUG: Using SocketReadWrite" << endl;
            lineTimeout = SocketReadWrite(buf);
        }
#endif
        WatchDog++;
        tickcount = 0;

        i = strlen((char*)buf);
        //cout << "DEBUG: buf length is: " << i << endl;

        if ((i > 0) && ((buf[0] != 0x0d) && (buf[0] != 0x0a))) {
            if (!lineTimeout) {
                TotalLines++;
                buf[i - 1] = 0x0d;
                buf[i++] = 0x0a;
            }

            buf[i++] = '\0';

            if ((i > 0) && (!TncSysopMode) && (configComplete)) {
                abuff = new aprsString ((char *) buf, SRC_TNC, srcTNC, "TNC", "*");

                if (abuff != NULL) {        //don't let a null pointer get past here!

                    if (abuff->aprsType == APRSQUERY) {     // non-directed query ?
                        queryResp(SRC_TNC, abuff);         // yes, send our response
                    }

                    if ((abuff->aprsType == APRSMSG)
                        && (abuff->msgType == APRSMSGQUERY)) {  // is this a directed query message?

                        if ((stricmp(szServerCall.c_str(), abuff->stsmDest.c_str()) == 0)
                            || (stricmp("aprsd", abuff->stsmDest.c_str()) == 0)
                            || (stricmp("igate", abuff->stsmDest.c_str()) == 0)) {    // Is query for us?
                            queryResp(SRC_TNC, abuff);         // Yes, respond.
                        }
                    }

                    if (logAllRF || abuff->ax25Source.compare(MyCall) == 0)
                        WriteLog(abuff->c_str(), RFLOG);        //Log our own packets that were digipeated

                    if ((abuff->reformatted)
                        || ((abuff->ax25Source.compare(MyCall) == 0)
                            && (igateMyCall == false))) {
                        delete abuff;        //don't igate packets which have been igated to RF...
                        abuff = NULL;        // ... and/or originated from our own TNC

                    } else {  // Not reformatted and not from our own TNC
                        if (abuff->aprsType == APRSMSG) {       //find matching posit for 3rd party msg
                            aprsString *posit =
                                getPosit(abuff->ax25Source,
                                          srcIGATE | srcUSERVALID | srcTNC);

                            if (posit != NULL) {
                                posit->EchoMask = src3RDPARTY;
                                sendQueue.write(posit);        //send matching posit only to msg port

                            }
                        }

                        if (abuff->aprsType == APRSMIC_E) {       //Reformat if it's a Mic-E packet
                            reformatAndSendMicE(abuff, sendQueue);
                        } else {
                            sendQueue.write(abuff, 0);        // Now put it in the Internet send queue.
                            // *** abuff must be freed by Queue reader ***.
                            //cout << "DEBUG: sent to Inet " << endl;
                        }
                    }
                }
            }
        }
        i = 0;                        //Reset buffer pointer
    }                                // Loop until server is shut down

    cerr << "Exiting Async com thread\n" << endl;
    threadAck = true;
    pthread_exit(0);

    return NULL;

}


//---------------------------------------------------------------------
// Send a text file to the TNC for configuration
int rfSendFiletoTNC(const std::string& szName)
{
    if (AsyncPort) {
        return(SendFiletoTNC(szName));
#ifdef HAVE_LIBAX25
    } else {
	//cout << "DEBUG: rfSendFiletoTNC(): AsyncPort == " << (AsyncPort ? "true":"false") << " using libAX25" << endl;
        return(0); // Not applicable for sockets
    }
#else
    } else {
        return(0);                      // quite compiler
    }
#endif
}

