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

extern "C" {
#include <unistd.h>
#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <sys/timeb.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>

#include <fstream.h>
#include <iostream.h>
#include <strstream.h>
#include <iomanip.h>
}

#include "rf.h"
#include "serial.h"
#include "sockets.h"

#include "constant.h"
#include "utils.h"
#include "cpqueue.h"
#include "history.h"
#include "queryResp.h"

using namespace std;

struct pidList {
    pid_t main;
    pid_t SerialInp;
    pid_t TncQueue;
    pid_t InetQueue;
};

extern ULONG WatchDog, tickcount, TotalConnects, TotalTncChars, TotalLines;
extern ULONG MaxConnects;
extern bool ShutDownServer;
extern cpQueue sendQueue;
extern cpQueue charQueue;
extern const int srcTNC;
extern bool configComplete;
extern bool igateMyCall;
extern bool logAllRF;
extern pidList pidlist;

extern const int srcTNC;
extern const int srcUSER;
extern const int srcUSERVALID;
extern const int srcIGATE;
extern const int srcSYSTEM;
extern const int srcUDP;
extern const int srcHISTORY;
extern const int src3RDPARTY;
extern const int sendHISTORY;

extern char *szServerCall;

pthread_t tidReadCom;
extern string MyCall;
pthread_mutex_t *pmtxWriteTNC;

char tx_buffer[260];
int txrdy;

int CloseAsync, threadAck;
bool TncSysopMode;                      // Set true when sysop wants direct TNC access

int AsyncPort;
char* AprsPath;

//--------------------------------------------------------------------
// Set APRS path (before the port is opened)
//
void rfSetPath (char* path)
{
    AprsPath = strdup(path);
}


//--------------------------------------------------------------------
// Open RF ports (TNC or sockets)

int rfOpen (char *szPort)
{
    int result;
    AsyncPort = (szPort[0] == '/');
    TncSysopMode = false;
    txrdy = 0;
    APIRET rc;

    if (AsyncPort)
        result = AsyncOpen(szPort);
#ifdef WITH_AX25
    else
        result = SocketOpen(szPort, AprsPath);
#else
    else {
        cerr << "AX.25 sockets are not supported by this executable" << endl;
        result = 1;
    }
#endif

    if (result != 0)
        return(result);

    pmtxWriteTNC = new pthread_mutex_t;
    pthread_mutex_init (pmtxWriteTNC, NULL);

    CloseAsync = false;
    threadAck = false;

    // Now start the serial port reader thread

    rc = pthread_create (&tidReadCom, NULL, rfReadCom, NULL);
    if (rc) {
        cerr << "Error: ReadCom thread creation failed. Error code = " << rc
            << endl;

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
    while (threadAck == false)
        reliable_usleep (1000);                  // wait till it does

    pthread_mutex_destroy(pmtxWriteTNC);
    delete pmtxWriteTNC;

    if (AsyncPort)
        return(AsyncClose());
#ifdef WITH_AX25
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
int rfWrite (char *cp)
{
    int rc;

    rc = pthread_mutex_lock(pmtxWriteTNC);
    strncpy(tx_buffer, cp, 256);

    txrdy = 1;
    while (txrdy)
        reliable_usleep(10000);                  // The rfReadCom thread will clear txrdy when it takes data

    rc = pthread_mutex_unlock(pmtxWriteTNC);
    return(rc);
}

//--------------------------------------------------------------------
// This is the RF port read thread.
// It also handles buffered writes to the port.
//
void* rfReadCom (void *vp)
{
    USHORT i;
    // APIRET rc;
    char buf[BUFSIZE];
    // unsigned char c;
    // FILE *rxc = (FILE *) vp;
    // size_t BytesRead;
    bool lineTimeout = false;
    TAprsString *abuff;

    i = 0;

    pidlist.SerialInp = getpid ();

    while (!CloseAsync) {

        if (AsyncPort)
            lineTimeout = AsyncReadWrite(buf);
#ifdef WITH_AX25
        else
            lineTimeout = SocketReadWrite(buf);
#endif

        WatchDog++;
        tickcount = 0;

        i = strlen((char*)buf);

        if ((i > 0) && ((buf[0] != 0x0d) && (buf[0] != 0x0a))) {
            if (lineTimeout == false) {
                TotalLines++;
                buf[i - 1] = 0x0d;
                buf[i++] = 0x0a;
            }

            buf[i++] = '\0';

            if ((i > 0) && (!TncSysopMode) && (configComplete)) {
                abuff = new TAprsString ((char *) buf, SRC_TNC, srcTNC, "TNC", "*");

                if (abuff != NULL) {        //don't let a null pointer get past here!

                    if (abuff->aprsType == APRSQUERY) {     // non-directed query ?
                        queryResp(SRC_TNC, abuff);         // yes, send our response
                    }

                    if ((abuff->aprsType == APRSMSG)
                        && (abuff->msgType == APRSMSGQUERY)) {  // is this a directed query message?

                        if ((stricmp(szServerCall, abuff->stsmDest.c_str()) == 0)
                            || (stricmp("aprsd", abuff->stsmDest.c_str()) == 0)
                            || (stricmp("igate", abuff->stsmDest.c_str()) == 0)) {    // Is query for us?
                            queryResp (SRC_TNC, abuff);         // Yes, respond.
                        }

                    }

                    if (logAllRF || abuff->ax25Source.compare(MyCall) == 0)
                        WriteLog (abuff->c_str (), RFLOG);        //Log our own packets that were digipeated

                    if ((abuff->reformatted)
                        || ((abuff->ax25Source.compare(MyCall) == 0)
                            && (igateMyCall == false))) {
                        delete abuff;        //don't igate packets which have been igated to RF...
                        abuff = NULL;        // ... and/or originated from our own TNC

                    } else {  // Not reformatted and not from our own TNC

                        if (abuff->aprsType == APRSMSG)        //find matching posit for 3rd party msg
                        {
                            TAprsString *posit =
                                getPosit(abuff->ax25Source,
                                          srcIGATE | srcUSERVALID | srcTNC);

                            if (posit != NULL) {
                                posit->EchoMask = src3RDPARTY;
                                sendQueue.write(posit);        //send matching posit only to msg port

                            }
                        }


                        if (abuff->aprsType == APRSMIC_E)        //Reformat if it's a Mic-E packet
                        {
                            reformatAndSendMicE(abuff, sendQueue);
                        } else
                            sendQueue.write(abuff, 0);        // Now put it in the Internet send queue.
                        // *** abuff must be freed by Queue reader ***.

                    }
                }
            }
        }

        i = 0;                        //Reset buffer pointer

    }                                // Loop until server is shut down

    cerr << "Exiting Async com thread\n" << endl << flush;
    threadAck = true;
    pthread_exit(0);

    return NULL;

}


//---------------------------------------------------------------------
// Send a text file to the TNC for configuration

int rfSendFiletoTNC (char *szName)
{
    if (AsyncPort)
        return(AsyncSendFiletoTNC(szName));
#ifdef WITH_AX25
    else
        return(0); // Not applicable for sockets
#else
    else
        return(0);                      // quite compiler
#endif
}

