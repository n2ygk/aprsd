/*
 * $Id$
 *
 * aprsd, Automatic Packet Reporting System Daemon
 * Copyright (C) 1997,2002 Dale A. Heatherington, WA4DSY
 * Copyright (C) 2001-2003 aprsd Dev Team
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


#include <termios.h>
#include <unistd.h>
#include <csignal>
#include <cassert>
#include <ctime>
#include <sys/timeb.h>
#include <sys/time.h>
#include <cstdio>
#include <ctype.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <string>
#include <sstream>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <strstream>
#include <iomanip>

#ifdef linux
#include <linux/kernel.h>
#include <linux/sys.h>
#endif
#include <sys/resource.h>

//tcpip header files

#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <cerrno>


#include "aprsd.h"
#include "servers.h"

#include "dupCheck.h"
#include "cpqueue.h"
#include "utils.h"
#include "history.h"
#include "serial.h"
#include "aprsString.h"
#include "validate.h"
#include "queryResp.h"
#include "mutex.h"
#include "rf.h"

using namespace std;
using namespace aprsd;

int nIGATES;      //Actual number of IGATES you have defined

SessionParams *sessions;    //points to array of active socket descriptors

int MaxClients;      //Limits how many users can log on at once
int MaxLoad;         //Limits maximum server load in Kbytes/sec

int serverLoad_s, bytesSent_s ; // Used in short term load calculation

//ULONG ulNScnt = 0;
bool tncPresent;     //TRUE if TNC com port has been specified
bool tncMute;        //TRUE stops messages from going to the TNC
bool traceMode;      //TRUE causes our Server Call to be appended to all packets in the path


ServerParams spMainServer, spMainServer_NH, spLinkServer;
ServerParams spLocalServer, spMsgServer, spHTTPServer, spSysopServer;
ServerParams spRawTNCServer, spIPWatchServer, spErrorServer,spOmniServer;
UdpParams upUdpServer;

dupCheck dupFilter;    //Create a dupCheck class named dupFilter.  This identifies duplicate packets.


const int maxIGATES = 100;   //Defines max number of IGATES/Servers you can connect to

#define BASE 0
#define USER 1
#define PASS 2
#define REMOTE 3
#define MAX 256
#define UMSG_SIZE MAX+BUFSIZE+3

//---------------------------------------------------
cpQueue sendQueue(256,true);        // Internet transmit queue
cpQueue tncQueue(64,true);          // TNC RF transmit queue
cpQueue charQueue(256,false);       // A queue of single characters
cpQueue conQueue(256,true);         // data going to the console from various threads

struct sLogon logon;
char *passDefault = "-1";
//char *MyCall;
string MyCall;
//char *MyLocation;
string MyLocation;
//char *MyEmail;
string MyEmail;
//char *NetBeacon;
string NetBeacon;
//char *TncBeacon;
string TncBeacon;
char *TncBaud;
int TncBeaconInterval, NetBeaconInterval;
long tncPktSpacing;
bool igateMyCall;       //Set TRUE if server will gate packets to inet with "MyCall" in the ax25 source field.
bool logAllRF;

bool APRS_PASS_ALLOW;      //Allow APRS style passcodes: T/F
bool History_ALLOW;        //Allow history list dumps: T/F
int defaultPktTTL;         //Holds default packet Time to live (hops) value.

int ackRepeats,ackRepeatTime;    //Used by the ACK repeat thread

bool sendOnRF(aprsString& atemp,  string szPeer, char* userCall, const int src);

//-------------------------------------------------------------------

//char* szServerCall;             //  This servers "AX25 Source Call" (user defined)
string szServerCall;
//char* szAPRSDPATH;              //  Servers "ax25 dest address + path" eg: >APD213,TCPIP*:
string szAPRSDPATH;


char szServerID[] = "APRS Server: ";
//const char szJAVAMSG[] = ">JAVA::javaMSG  :";
string szJAVAMSG = string(">JAVA::javaMSG  :");

const char szUSERLISTMSG[]  = ":USERLIST :";
const char szUNVERIFIED[]   = "Unverified ";
const char szVERIFIED[]     = "Verified ";
const char szRM1[]          = "You are an unverified user, Internet access only.\r\n";
const char szRM2[]          = "Send Access Denied. Access is Read-Only.\r\n";
const char szRM3[]          = "RF access denied, Internet access only.\r\n";
const char szACCESSDENIED[] = "Access Denied ";

Mutex pmtxSendFile;
Mutex pmtxSend;                         // socket send
Mutex pmtxAddDelSess;                   // add/del sessions
Mutex pmtxCount;                        // counters
Mutex pmtxDNS;                          // buggy gethostbyname2_r ?

bool ShutDownServer, configComplete;

int msg_cnt;
int posit_cnt;
ULONG WatchDog, tickcount, TotalConnects, TotalTncChars, TotalLines;
int  MaxConnects;
ULONG TotalServerChars, TotalUserChars,TotalMsgChars, TotalUdpChars, bytesSent, webCounter;
ULONG TotalFullStreamChars;
ULONG countREJECTED, countLOOP, countOVERSIZE, countNOGATE, countIGATED;
time_t serverStartTime;
ULONG TotalTNCtxChars;

//------------------------------------------------------------------------------------------
//Array to hold list of stations
//allowed to full time gate from
//Internet to RF

string* rfcall[MAXRFCALL];                 //list of stations gated to RF full time (all packets)
int rfcall_idx;

string* posit_rfcall[MAXRFCALL];           //Stations whose posits are always gated to RF
int posit_rfcall_idx;

string* stsmDest_rfcall[MAXRFCALL];       //Station to station messages with these
                                          // DESTINATION call signs are always gated to RF

int stsmDest_rfcall_idx;

int fullStreamRate, serverLoad;
int serverStreamRate, tncStreamRate, userStreamRate, msgStreamRate, udpStreamRate;   //Server statistics
//double upTime;
int upTime;

bool RF_ALLOW = false;                    //TRUE to allow Internet to RF message passing.
bool NOGATE_FILTER = true;                //TRUE enables the NOGATE and RFONLY packet reject filter

//--------------------------------------------------------------------------------------------
ConnectParams cpIGATE[maxIGATES];

const int maxTRUSTED = 20;        //Max number of trusted UDP users
sTrusted Trusted[maxTRUSTED];     //Array to store their IP addresses
int nTrusted = 0;                 //Number of trusted UDP users defined

//Debug stuff
pidList pidlist;
char* DBstring;      //for debugging
aprsString* lastPacket; //for debugging
FILE* fdump;


//-------------------------------------------------------------------------------
int initDefaults(void)
{
    int i;
    logon.user = NULL;
    logon.pass = NULL;
    TotalConnects = TotalLines = MaxConnects = TotalServerChars = 0;
    TotalTncChars = 0;
    TotalUserChars = 0;
    TotalMsgChars = 0;
    TotalServerChars = 0;
    TotalFullStreamChars = 0;
    TotalUdpChars = 0;
    countNOGATE = 0;
    countREJECTED = 0;
    countOVERSIZE = 0;
    countLOOP = 0;
    countIGATED = 0;
    defaultPktTTL = 5;      //Set default Time-To-Live
    bytesSent = 0;
    serverLoad_s = 0;
    bytesSent_s  = 0;
    TotalTNCtxChars = 0;
    msg_cnt = 0;
    posit_cnt = 0;
    MyCall = "N0CALL";
    MyLocation = "NoWhere";
    MyEmail = "nobody@NoWhere.net";
    TncBeacon = string("");
    TncBaud = "9600";
    NetBeacon = string("");
    TncBeaconInterval = 0;
    NetBeaconInterval = 0;
    tncPktSpacing = 1500000;  //1.5 second default

    igateMyCall = true;        //To be compatible with previous versions set it TRUE
    tncPresent = false;
    traceMode = false;
    logAllRF = false;
    ConvertMicE = false;
    tncMute = false;
    MaxClients = MAXCLIENTS;  //Set default. aprsd.conf file will override this
    MaxLoad = MAXLOAD ; //Set default server load limit in bytes/sec

    ackRepeats = 2;            //Default extra acks to TNC
    ackRepeatTime = 5;         //Default time between extra acks to TNC in seconds.

    APRS_PASS_ALLOW = true;    //Default: allow aprs style user passcodes
    History_ALLOW = true;      //Default: allow history dumps.
    webCounter = 0;
    queryCounter = 0;

    for (i = 0; i < maxIGATES; i++) {           //Clear the array of outbound server connections
        cpIGATE[i].RemoteSocket = -1;
        cpIGATE[i].RemoteName = string("");
    }

    szServerCall = "aprsd";    //default server FROM CALL used in system generated pkts.

    //szAPRSDPATH = new char[64];     //Don't put more than 63 chars in here
    //memset(szAPRSDPATH, 0, 64);
    //strcpy(szAPRSDPATH,">");
    //strcat(szAPRSDPATH,PGVERS);
    //strcat(szAPRSDPATH,",TCPIP*:");  // ">APD215,TCPIP*:"
    szAPRSDPATH = string(">");
    szAPRSDPATH += PGVERS;
    szAPRSDPATH += ",TCPIP*:";
    
    spOmniServer.ServerPort = 0;
    spLinkServer.ServerPort = 0;      //Ports are set in aprsd.conf file
    spMainServer.ServerPort = 0;
    spMainServer_NH.ServerPort = 0;
    spLocalServer.ServerPort = 0;
    spRawTNCServer.ServerPort = 0;
    spMsgServer.ServerPort = 0;
    upUdpServer.ServerPort = 0;

    spSysopServer.ServerPort = 14500;    //Sysop Port for telnetting to the TNC directly
    spSysopServer.EchoMask = 0;

    spHTTPServer.ServerPort = 14501;    //HTTP monitor port default
    spHTTPServer.EchoMask = wantHTML;

    spIPWatchServer.ServerPort = 14502;    //IP Watch port default
    spIPWatchServer.EchoMask = srcUSERVALID
                              +  srcUSER
                              +  srcIGATE
                              +  srcTNC
                              +  wantSRCHEADER;

    spErrorServer.ServerPort = 14503;
    spErrorServer.EchoMask = wantREJECTED + wantSRCHEADER;


    ShutDownServer = false;

    return 0;
}

//----------------------------------------------------------------
//This runs after the aprsd.conf file is read.
void serverInit(void)
{
    int i,rc;

    sessions = new SessionParams[MaxClients];

    if (sessions == NULL) {
        cerr << "Can't create sessions pointer\n" ;
        exit(-1);
    }

    for (i = 0; i < MaxClients; i++) {
        sessions[i].Socket = -1;
        sessions[i].ServerPort = -1;
        sessions[i].EchoMask = 0;
        sessions[i].szPeer = new char[SZPEERSIZE];
        sessions[i].userCall = new char[USERCALLSIZE];
        sessions[i].pgmVers = new char[PGMVERSSIZE];
        memset((void*)sessions[i].szPeer, 0, SZPEERSIZE);    //Fill strings with nulls
        memset((void*)sessions[i].userCall, 0, USERCALLSIZE);
        memset((void*)sessions[i].pgmVers, 0, PGMVERSSIZE);
    }

    // Create the server threads

    //Create Main Server thread. (Provides Local data, Logged on users and IGATE data)
    if (spMainServer.ServerPort > 0) {
        if ((rc = pthread_create(&spMainServer.tid, NULL,TCPServerThread,&spMainServer)) != 0) {
            cerr << "Error: Main TCPServerThread failed to start" << endl;
            exit(-1);
        }
    }

    //Create another Main Server thread .
    // (Provides Local data, Logged on users and IGATE data but doesn't dump the 30 min. history)
    if (spMainServer_NH.ServerPort > 0) {
        if ((rc = pthread_create(&spMainServer_NH.tid, NULL,TCPServerThread,&spMainServer_NH)) != 0) {
            cerr << "Error: Main-NH TCPServerThread failed to start" << endl;
            exit(-1);
        }
    }

    // Create Main Server thread. (Provides Local data, Logged on users and IGATE data)
    if (spOmniServer.ServerPort > 0) {
        if ((rc = pthread_create(&spOmniServer.tid, NULL,TCPServerThread,&spOmniServer)) != 0) {
            cerr << "Error: Omni TCPServerThread failed to start" << endl;
            exit(-1);
        }
    }

    //Create Link Server thread.  (Provides local TNC data plus logged on users data)
    if (spLinkServer.ServerPort > 0) {
        if ((rc = pthread_create(&spLinkServer.tid, NULL,TCPServerThread,&spLinkServer)) != 0) {
            cerr << "Error: Link TCPServerThread failed to start" << endl;
            exit(-1);
        }
    }

    //Create Local Server thread  (Provides only local TNC data).
    if (spLocalServer.ServerPort > 0) {
        if ((rc = pthread_create(&spLocalServer.tid, NULL,TCPServerThread,&spLocalServer)) != 0) {
            cerr << "Error: Local TCPServerThread failed to start" << endl;
            exit(-1);
        }
    }

    // Create Sysop Port thread  (Provides no data, only for talking to the TNC directly).
    if (spSysopServer.ServerPort > 0) {
        if ((rc = pthread_create(&spSysopServer.tid, NULL,TCPServerThread,&spSysopServer)) != 0) {
            cerr << "Error: Sysop Port TCPServerThread failed to start" << endl;
            exit(-1);
        }
    }

    // Create Local Server thread  (Provides only local TNC data).
    if (spRawTNCServer.ServerPort > 0) {
        if ((rc = pthread_create(&spRawTNCServer.tid, NULL,TCPServerThread,&spRawTNCServer)) != 0) {
            cerr << "Error: RAW TNC TCPServerThread failed to start" << endl;
            exit(-1);
        }
    }

    //Create message Server thread  (Provides only 3rd party message data).
    if (spMsgServer.ServerPort > 0) {
        if ((rc = pthread_create(&spMsgServer.tid, NULL,TCPServerThread,&spMsgServer)) != 0) {
            cerr << "Error: 3rd party message TCPServerThread failed to start" << endl;
            exit(-1);
        }
    }

    if (upUdpServer.ServerPort > 0) {
        if ((rc = pthread_create(&upUdpServer.tid, NULL,UDPServerThread,&upUdpServer)) != 0) {
            cerr << "Error: UDP Server thread failed to start" << endl;
            exit(-1);
        }
    }

    // Create HTTP Server thread. (Provides server status in HTML format)
    if (spHTTPServer.ServerPort > 0) {
        if ((rc = pthread_create(&spHTTPServer.tid, NULL,TCPServerThread,&spHTTPServer)) != 0) {
            cerr << "Error: HTTP server thread failed to start" << endl;
            exit(-1);
        }
    }

    // Create IPWatch Server thread. (Provides prepended header with IP and User Call
    // on aprs packets)
    if (spIPWatchServer.ServerPort > 0) {
        if ((rc = pthread_create(&spIPWatchServer.tid, NULL,TCPServerThread,&spIPWatchServer)) != 0) {
            cerr << "Error: IPWatch server thread failed to start" << endl;
            exit(-1);
        }
    }

    // Create ERROR Server thread. (Provides prepended header with IP and User Call on
    // REJECTED aprs packets)
    if (spErrorServer.ServerPort > 0) {
        if ((rc = pthread_create(&spErrorServer.tid, NULL,TCPServerThread,&spErrorServer)) != 0) {
            cerr << "Error: Rejected aprs packet server thread failed to start" << endl;
            exit(-1);
        }
    }

    pthread_t  tidDeQueuethread;
    if ((rc = pthread_create(&tidDeQueuethread, NULL, DeQueue, NULL)) != 0) {
        cerr << "Error: DeQueue thread failed to start" << endl;
        exit(-1);
    }

    cerr << "nIGATES=" << nIGATES << endl;
    cerr << "igate0=" << cpIGATE[0].RemoteName << endl;

    bool firstHub = false;

    if (nIGATES > 0)
        cout << "Connecting to IGATEs and Hubs now..." << endl;

    for (i = 0; i < nIGATES; i++) {
        if (cpIGATE[i].RemoteSocket == 0) {
            string warn = string(cpIGATE[i].RemoteName) + ": Error: No socket number specified for Server/Hub\n";
            cerr << warn ;
            WriteLog(warn, string(MAINLOG));
        } else {
            if (!firstHub) {
                if ((rc = pthread_create(&cpIGATE[i].tid, NULL,TCPConnectThread,&cpIGATE[i])) == 0)
                    pthread_detach(cpIGATE[i].tid);

                if (cpIGATE[i].hub)
                    firstHub = true;
            } else {
                if (!cpIGATE[i].hub) {
                    if ((rc = pthread_create(&cpIGATE[i].tid, NULL,TCPConnectThread,&cpIGATE[i])) == 0)
                        pthread_detach(cpIGATE[i].tid);
                }
            }
        }
    }
}



//-----------------------------------------------------------------
void initSessionParams(SessionParams* sp, int s, int echo, bool svr)
{
    sp->Socket = s;
    sp->EchoMask = echo;

    sp->overruns = 0;
    sp->bytesIn = 0;
    sp->bytesOut = 0;
    sp->vrfy = false;
    sp->dead = false;
    sp->svrcon = svr;    //outbound server connection, T/F
    sp->starttime = time(NULL);
    sp->lastActive = sp->starttime;
    memset((void*)sp->szPeer, 0, SZPEERSIZE);    //Fill strings with nulls
    memset((void*)sp->userCall, 0, USERCALLSIZE);
    memset((void*)sp->pgmVers, 0, PGMVERSSIZE);
}


//--------------------------------------------------------------------
//Returns the number of connected clients including IGATE and HUB connections
int getConnectedClients(void)
{
    //int i;
    int count = 0;
    Lock addDelLock(pmtxAddDelSess);

    for (int i = 0; i < MaxClients; i++)
        if ((sessions[i].Socket > -1))
            count++;

    for (int i = 0; i < nIGATES; i++)
        if (cpIGATE[i].connected != true)
            count--;


   return count;
}


//---------------------------------------------------------------------
//Add a new user to the list of active sessions
//Includes outbound Igate connections too!
//Returns NULL if it can't find an available session
// or server load may exceed MaxLoad with the added connection.
SessionParams* AddSession(int s, int echo, bool svr)
{
    SessionParams *rv = NULL;
    int i, softlimit;
    Lock addDelLock(pmtxAddDelSess);

    addDelLock.get();
    if (fullStreamRate > 0)
        softlimit = MaxLoad - fullStreamRate;     // MaxLoad less 1 full aprs data stream
    else
        softlimit = MaxLoad - (MaxLoad / 10);     // MaxLoad less 10%

    for (i = 0; i < MaxClients; i++)
        if (sessions[i].Socket == -1)
            break;  //Find available unused session

    // If less than max clients and short term load is well below maxload...
    if ((i < MaxClients) && (serverLoad_s <= softlimit)) {
        rv = &sessions[i];
        initSessionParams(rv, s, echo, svr);
    } else
        rv = NULL;

    addDelLock.release();
    return rv;
}


//--------------------------------------------------------------------
//Remove a user
bool DeleteSession(int s)
{
    bool rv = false;

    if (s == -1)
        return false;

    Lock addDelLock(pmtxAddDelSess);
    
    for (int i = 0; i < MaxClients; i++) {
        if (sessions[i].Socket == s ) {
            sessions[i].Socket = -1;
            sessions[i].EchoMask = 0;
            sessions[i].pid = 0;
            sessions[i].dead = true;
            rv = true;
        }
    }
    return rv;
}


//-------------------------------------------------
bool AddSessionInfo(int s, const char* userCall, const char* szPeer, int port, const char* pgmVers)
{
    bool retval = false;
    Lock addDelLock(pmtxAddDelSess);

    for (int i = 0; i < MaxClients; i++) {
        if (sessions[i].Socket == s ) {
            strncpy(sessions[i].szPeer, szPeer, SZPEERSIZE-1);
            strncpy(sessions[i].userCall, userCall, USERCALLSIZE-1);
            sessions[i].ServerPort = port;
            strncpy(sessions[i].pgmVers,pgmVers, PGMVERSSIZE-1);
            sessions[i].pid = getpid();
            retval = true;
        }
    }
    return retval;
}

//---------------------------------------------------------------------
void CloseAllSessions(void)
{
    for (int i = 0; i < MaxClients; i++) {
        if (sessions[i].Socket != -1 ) {
            shutdown(sessions[i].Socket,2);
            close(sessions[i].Socket);
            sessions[i].Socket = -1;
            sessions[i].EchoMask = 0;
        }
    }
}
//---------------------------------------------------------------------

//-------------------------------------------------------------------


// Send data at "p" to all logged on clients listed in the sessions array
// subject to some packet filtering.


void SendToAllClients(aprsString* p)
{
    int rc, n, nraw, nsh;
    bool dup = false;
    Lock addDelLock(pmtxAddDelSess, false); // don't get these locks quite yet...
    Lock sendLock(pmtxSend, false);
    Lock countLock(pmtxCount, false);

    if (p == NULL)
        return;

    if (p->length() < 3)
        return;         //Silently reject runts

    if ((p->aprsType == CONTROL) || (p->msgType == APRSMSGSERVER))
        return;         //Silently reject user control commands

    if (p->EchoMask & sendDUPS)
        dup = true;     //This packet is a duplicate

    if (NOGATE_FILTER && p->nogate) {   //Don't igate NOGATE and RFONLY packets
        countNOGATE++;                  //Count the rejected NOGATE packets
        p->aprsType = APRSREJECT;       //Flag as reject
    }

    bool bad_ax25 = ((((p->EchoMask & srcSTATS) == 0) //Don't flag system status text stream
                    && (p->valid_ax25 == false))   //invalid ax25
                    || (p->aprsType == APRSREJECT)   // reject
                    || (p->EchoMask == 0));         //EchoMask is zero

    if (bad_ax25) {
        countREJECTED++;   //Count rejected AX25 packets
        p->EchoMask |= wantREJECTED ;  //For those who want to see them
    }

    if ((bad_ax25 == false) && (p->EchoMask & srcSTATS) == 0) {
        countLock.get();
        countIGATED++;     //Count good packets

        if (p->aprsType == APRSMSG)
            TotalMsgChars += p->length(); //Count message characters

        countLock.release();
    }

    if ((p->aprsType == APRSREJECT) && (dup == false))
        WriteLog(p->raw, string(REJECTLOG));     // Log rejected packets

    sendLock.get();
    addDelLock.get();

    // save some cpu time by putting these values in integers now.
    n = p->length();
    nraw = p->raw.length();
    nsh = p->srcHeader.length();

    for (int i = 0; i < MaxClients; i++) {
        if (sessions[i].Socket != -1) {
            bool wantdups, wantsrcheader,wantrejected,wantecho,echo,raw,wantrej;
            wantdups = wantsrcheader = wantrejected = wantecho = echo = raw = wantrej = false ;
            int tc = 0;
            int Em;

            if (sessions[i].EchoMask & sendDUPS)
                wantdups = true; //User wants duplicates

            if (sessions[i].EchoMask & wantREJECTED)
                wantrej = true;

            if (sessions[i].EchoMask & wantSRCHEADER)
                wantsrcheader = true;   //User wants IP source header

            if (sessions[i].EchoMask & wantRAW)
                raw = true;         //User wants raw TNC data

            if (sessions[i].EchoMask & wantECHO)
                wantecho = true;   //User wants to see his own packets

            if (p->sourceSock == sessions[i].Socket)
                echo = true;   //This packet came from the this user

            // Mask off non-source determining bits.
            Em = p->EchoMask & 0x7ff;

            if ((sessions[i].EchoMask & Em)   //Echo inet data if source mask bits match session mask
                    && (ShutDownServer == false)
                    && (sessions[i].dead == false)) {//Send NO data to a dead socket (added vers. 215a6)

                rc = tc = 0;
                // User wants raw tnc data
                if(raw && !echo && (nraw > 0)) {
                    rc = send(sessions[i].Socket,p->raw.c_str(),nraw,0);
                    goto done;
                }

                //Normal dup and error filtered data stream
                if ((!wantsrcheader)      /* No IP source header prepended */
                        && (!dup)          /* No duplicates */
                        && (!bad_ax25)     /* No invalid ax25 packets or rejects */
                        && (!echo)){       /* No echo of users own data */

                    rc = send(sessions[i].Socket,p->c_str(),n,0);
                    goto done;
                }

                // User wants source header prepended to all aprs packets.
                if (wantsrcheader && !echo && !bad_ax25){  //Append source header to aprs packets, duplicates ok.
                                                                 // Mostly for debugging the network
                    rc = send(sessions[i].Socket,p->srcHeader.c_str(),nsh,0);
                    tc = rc;

                    if (rc != -1)
                        rc = send(sessions[i].Socket, p->c_str(), n, 0);
                        goto done;
                }

                //User wants his own data echoed back,
                // no dup check or validation on his own data.
                if (echo && wantecho){
                    rc = send(sessions[i].Socket, p->c_str(), n, 0);
                    goto done;
                }

                // Error Port 14503: User wants source header plus Rejected packets ONLY.
                if (wantrej  && bad_ax25
                        && (echo == false)
                        && (dup == false)
                        && ((p->EchoMask & srcSTATS) == 0) ){

                    rc = send(sessions[i].Socket,p->srcHeader.c_str(),nsh,0);
                    tc = rc;
                    if (rc != -1)
                        rc = send(sessions[i].Socket,p->c_str(),n,0);
                }

                if (wantdups && dup && !echo ) {     //Send only the dups to this guy
                    rc = send(sessions[i].Socket,p->c_str(),n,0);
                    goto done;
                }

                /*
                    Disconnect user if socket error or he failed
                    to accept 10 consecutive packets due to
                    resource temporarally unavailable errors
                */

                done:
                    if (rc == -1) {
                        if (errno == EAGAIN)
                            sessions[i].overruns++;

                        if ((errno != EAGAIN) || (sessions[i].overruns >= 10)){
                            sessions[i].EchoMask = 0;          //No more data for you!
                            sessions[i].dead = true;           //Mark connection as dead for later removal...
                                                           //...by thread that owns it.
                        }
                    } else {
                        sessions[i].overruns = 0;   //Clear users overrun counter if he accepted packet
                        sessions[i].bytesOut += (tc + rc); //Add these bytes to his bytesOut total
                        bytesSent += (tc + rc);      //Add these to the grand total for server load calc.
                        bytesSent_s += (tc + rc);    //Also add to short term load
                    }
            }
        }
    }
    addDelLock.release();
    sendLock.release();
    return;
}


//---------------------------------------------------------------------
// Pushes a character string into the server send queue.
void BroadcastString(const char* sp)
{
    aprsString *msgbuf = new aprsString(sp, SRC_INTERNAL, srcSYSTEM);
    msgbuf->addIIPPE('Z');   //Make it a zero hop packet
    msgbuf->addPath(szServerCall);
    sendQueue.write(msgbuf); //DeQueue() deletes the *msgbuf
    return;
}

//---------------------------------------------------------------------
//This is a thread.  It removes items from the send queue and transmits
//them to all the clients.  Also handles checking for items in the TNC queue
//and calling the tnc dequeue routine.
void *DeQueue(void* vp)
{
    bool Hist,dup;
    aprsString* abuff;
    int MaxAge,MaxCount;
    struct timeval tv;
    struct timezone tz;
    long usLastTime, usNow;
    long t0;
    //string serverCall = szServerCall; //convert out Server call to a string
    timespec ts;
    char* dcp;
    Lock countLock(pmtxCount, false);   // don't get this lock quite yet...

    usLastTime = usNow = 0;

    pidlist.InetQueue = getpid();

    nice(-10);   //Increase priority of this thread by 10 (only works if run as root)

    while (!ShutDownServer) {
        gettimeofday(&tv,&tz);  //Get time of day in microseconds to tv.tv_usec
        usNow = tv.tv_usec + (tv.tv_sec * 1000000);
        if (usNow < usLastTime)
            usLastTime = usNow;

        t0 = usNow;
        if ((usNow - usLastTime) >= tncPktSpacing) {  //Once every 1.5 second or user defined
            usLastTime = usNow;
            if (tncQueue.ready())
                dequeueTNC(); //Check the TNC queue
        }

        while (!sendQueue.ready()) {    //Loop here till somethings in the send queue
            gettimeofday(&tv,&tz);  //Get time of day in microseconds to tv.tv_usec
            usNow = tv.tv_usec + (tv.tv_sec * 1000000);

            if (usNow < usLastTime)
                usLastTime = usNow;

            t0 = usNow;

            if ((usNow - usLastTime) >= tncPktSpacing) {  //Once every 1.5 second or user defined
                usLastTime = usNow;
                if (tncQueue.ready())
                    dequeueTNC(); //Check the TNC queue
            }
            ts.tv_sec = 0;
            ts.tv_nsec = 1000000; //1 ms timeout for nanosleep()
            nanosleep(&ts,NULL);  //1ms

            if (ShutDownServer)
                pthread_exit(0);
        }

        abuff = (aprsString*)sendQueue.read(NULL);  // <--Read an aprsString pointer from the queue to abuff

        lastPacket = abuff;  //debug thing
        dcp = " ";           // another one

        if (abuff != NULL) {
            abuff->dest = destINET;
#ifdef DEBUGTHIRDPARTY
            if (abuff->aprsType == APRS3RDPARTY){
                string logEntry = abuff->call + ": " + *abuff;
                WriteLog(logEntry, DEBUGLOG);  // DEBUG CODE
            }
#endif
            if ((abuff->hasBeenIgated()) && (abuff->aprsType == APRS3RDPARTY))
                abuff->aprsType = APRSREJECT;

            if (abuff->aprsType == APRS3RDPARTY){ // Convert  3rd party packets not previously
                                                  // igated to normal packets
                abuff->thirdPartyToNormal();
                //cerr << "Converted a 3rd party packet\n" ;
            }

            //Debug: Prints hash value, packet and marks duplicates with X to console
            // char f;
            // if(dup) f = 'X'; else f = ' ';
            // printf("%c %08X %s",f,abuff->hash,abuff->c_str());
            //if(dup) cerr << "DUP: " << abuff->c_str();

            // This option enables the adding of callsign of the sender for
            // so we can see where it entered the internet system.
#ifdef INET_TRACE
            // Now insert the injecting callsign and IIPPE (q string) info into the path
            if ((abuff->valid_ax25) && ((abuff->EchoMask & srcSTATS) == 0)){
                string src_call = MyCall;  //Source call defaults to our own Igate call.

                if (abuff->IjpIdx == 0) {   // If q construct not in packet
                                            // we need to convert old packet to new format
                                            // and add qAc and source callsign.
                    // Convert UI-View "CALLSIGN,I" style packets
                    string Icall = abuff->removeI();  //If present remove I or I* and preceeding call sign
                                                     //Also saves callsign in abuff->icall
                    if (abuff->cci == true)
                        src_call = Icall; //Use preceeding call as source call

                    // Add the q string path elemement
                    abuff->addIIPPE();

                    // At this point we have a packet with IIPPE (q string)  in it.

                    char ps = abuff->getPktSource();   // ps is set to C,X,S,R,r,U,I or Z .

                    // add source callsign after the qAc
                    switch (ps) {    //Insert the packet source callsign into path
                        case 'X':                                   //non-validated client or
                        case 'C':                                   //Validated client or Server...
                        case 'S':
                            abuff->addPath(abuff->call); //Add peer ip hex alias or user login call
                            break;

                        case 'U':
                            abuff->addPath(szServerCall);     // our UDP port
                            break;

                        case 'r':
                        case 'R':
                            abuff->addPath(src_call); // From a TNC, ours or a users.
                            break;

                        case 'I':
                            abuff->addPath(MyCall);   //From Internal use our IGATE call
                            break;

                        case 'Z' :
                            abuff->aprsType = APRSREJECT;   // Zero Hops, REJECT.
                            break;

                    } ; //End switch
                } //End if " no Internet Injection Point Path Element."
            }     //End of valid_ax25 test
#endif
            // DUP CHECK HERE
            dup = false;

            if (((abuff->EchoMask) & (srcSTATS | srcSYSTEM)) == 0 )
                dup  = dupFilter.check(abuff,DUPWINDOW);  //Check for duplicates within N second window

            if (dup == false){                           //Packet is NOT a duplicate
                //pthread_mutex_lock(pmtxCount);
                countLock.get();
                TotalFullStreamChars += abuff->length();  //Tally all non-duplicate bytes
                //pthread_mutex_unlock(pmtxCount);
                countLock.release();


                if (abuff->IjpIdx > 0){   //Packet has q construct in it
                    char ps = abuff->ax25Path[abuff->IjpIdx][2]; //Get packet source character from q string

                    if(abuff->EchoMask & (srcUSER | srcUSERVALID | srcIGATE)){  //Came from server/hub or user igate
                        if (ps =='Z') {
                            if (abuff->ax25Source.compare(szServerCall) != 0)
                                abuff->aprsType = APRSREJECT;   // Zero Hops, REJECT.
                        } // end Z test

                        // LOOP DETECTORS.

                        // Loop detector #1, Reject if server call or igate call seen after the qA
                        if ((abuff->queryPath(szServerCall,abuff->IjpIdx + 1) != string::npos)
                                || (abuff->queryPath(MyCall,abuff->IjpIdx) != string::npos)){

                            abuff->aprsType = APRSREJECT;    //Looped packet, REJECT
                            string log_str = abuff->srcHeader + *abuff;
                            WriteLog(log_str, LOOPLOG);   //Write offending packet to loop.log
                            countLOOP++;
                        }  // End loop detect #1

                        // Loop detector #2, Reject if user login call seen after qA but not last path element
                        unsigned rc = abuff->queryPath(abuff->call,abuff->IjpIdx + 1);
                        if (( rc != string::npos)
                                && (abuff->aprsType != APRSREJECT)){

                            if (rc != (unsigned)(abuff->pathSize - 1)){
                                abuff->aprsType = APRSREJECT;    //Looped packet, REJECT
                                string log_str = abuff->srcHeader + *abuff;
                                WriteLog(log_str, LOOPLOG);   //Write offending packet to loop.log
                                countLOOP++;
                            }
                        } // End loop detect #2
                    } // End pkt echomask source check, user/server/hub

                    // Add TRACE path element if required
                    if (((ps == 'I') || traceMode)            //Beacon packets always get full trace
                            && (abuff->aprsType != APRSREJECT)
                            && (abuff->sourceSock != SRC_INTERNAL)){

                        if (abuff->EchoMask & srcUSERVALID){  //From validated connection?
                            unsigned rc = abuff->queryPath(abuff->call,abuff->IjpIdx + 1);
                            if(rc == string::npos)
                                abuff->addPath(abuff->call); //Add user login call if not present in path.
                        }
                        abuff->addPath(szServerCall);    //Add our server call
                    }
                } // end  if(abuff->IjpIdx > 0){ // pkt has q construct

            } // end if(dup == false)

            // Packet size limiter
            if (abuff->EchoMask & ~srcSTATS){  //Check all except srcSTATS for size
                if ((abuff->length() >= BUFSIZE - 5)      //Limit total packet length (250 bytes)
                        || (abuff->pathSize > MAXISPATH)){ //Limit path size

                    countOVERSIZE++;
                    abuff->aprsType = APRSREJECT;
                }
            }

            // The following code filters input to the History List
            if (((abuff->EchoMask & src3RDPARTY)&&((abuff->aprsType == APRSPOS)) /* No Posits fetched from history list */
                    || (abuff->aprsType == COMMENT)               /* No comment packets in the history buffer*/
                    || (abuff->aprsType == CONTROL)               /* No user commands */
                    || (abuff->msgType == APRSMSGSERVER)          /* No user Server control messages */
                    || (abuff->aprsType == APRSREJECT)            /* No rejected packets  */
                    || (abuff->aprsType == APRSUNKNOWN)           /* No Unknown packets */
                    || (abuff->aprsType == APRSID)                /* No ID packets */
                    || (abuff->aprsType == APRSQUERY)             /* No Querys */
                    || (abuff->EchoMask & (srcSTATS | srcSYSTEM)) /* No internally generated packets */
                    || (dup)                                      /* No duplicates */
                    || (NOGATE_FILTER && abuff->nogate)           /* No NOGATE flagged packets, new in v2.1.5 */
                    || (abuff->valid_ax25 == false)               /* No invalid ax25 packets */
                    || (abuff->reformatted))){                    /* No 3rd party reformatted pkts */

                Hist = false;    //None of the above allowed in history list
            } else {
                GetMaxAgeAndCount(&MaxAge,&MaxCount);	//Set max ttl and count values
                abuff->ttl = MaxAge;
                Hist = AddHistoryItem(abuff);   	//Put item in history list.
            }

            // if(abuff->EchoMask & sendDUPS) printf("EchoMask sendDUPS bit is set\n");

            if (dup)
                abuff->EchoMask |= sendDUPS; //If it's a duplicate mark it.

            if ((abuff->aprsType == APRSQUERY) && (dup == false)){
                if (abuff->EchoMask & srcUSERVALID)
                    queryResp(abuff->sourceSock,abuff);

                if (abuff->EchoMask & srcIGATE)
                    queryResp(SRC_IGATE, abuff);

                if (abuff->EchoMask & srcTNC) {
                    queryResp(SRC_TNC,abuff);
                    abuff->aprsType = APRSREJECT; //Do not pass along any queries from TNC.
                }
            }

            SendToAllClients(abuff);	// Send item out on internet subject to additional filtering

            if (!Hist) {
                if (abuff)
                    delete abuff;   //delete it now if it didn't go to the history list.

                abuff = NULL;
            }
        } else
            cerr << "Error in DeQueue: abuff is NULL" << endl;
    }
    return NULL;       //Should never return

}

//----------------------------------------------------------------------
/* This thread is started by code in  dequeueTNC when an ACK packet
is detected being sent to the TNC.  A pointer to the ack packet is passed
to this thread.  This thread puts additional identical ack packets into the
TNC queue.  The allowdup attribute is set so the dup detector won't kill 'em.
*/
void *ACKrepeaterThread(void *p)
{
    timespec ts;
    ts.tv_sec = ackRepeatTime;
    ts.tv_nsec = 0;

    aprsString *paprs;
    paprs = (aprsString*)p;
    aprsString *abuff = new aprsString(*paprs);
    abuff->allowdup = true; //Bypass the dup filter!
    paprs->ttl = 0;         //Flag tells caller we're done with it.

    for (int i = 0 ; i < ackRepeats; i++) {
        nanosleep(&ts,NULL);  //Delay ackRepeatTime
        aprsString *ack =  new aprsString(*abuff);
        tncQueue.write(ack);   //ack aprsString  will be deleted by TNC queue reader
    }
    delete abuff;
    pthread_exit(0);
}

//----------------------------------------------------------------------

// Pulls aprsString object pointers off the tncQueue
// and sends the data to the TNC.
#define RFBUFSIZE 252
void dequeueTNC(void)
{
    bool dup;
    char *rfbuf = NULL;
    aprsString* abuff = NULL;
    char szUserMsg[UMSG_SIZE];
    timespec ts;

    pthread_t tid;

    abuff = (aprsString*)tncQueue.read(NULL);  //Get pointer to a packet off the queue

    if(abuff == NULL) 
        return;

    if ((RF_ALLOW == false) //See if sysop allows Internet to RF traffic
            && ((abuff->EchoMask & srcUDP) == false)){  //UDP packets excepted

        delete abuff;  //No RF permitted, delete it and return.
        return;
    }

    //abuff->print(cout); //debug

    abuff->ttl = 0;
    abuff->dest = destTNC;
    dup = dupFilter.check(abuff, DUPWINDOW);       //Check for duplicates during past 20 seconds

    if (dup) {
        delete abuff; //kill the duplicate here
        return;
    }

    rfbuf = new char[RFBUFSIZE] ;

    if (rfbuf != NULL) {
        if (tncPresent ) {
            if ((abuff->allowdup == false)          //Prevent infinite loop!
                    && (abuff->msgType == APRSMSGACK) //Only ack packets
                    && (ackRepeats > 0) ) {           //Only if repeats greater than zero

                abuff->ttl = 1;                      //Mark it as unprocessed (ttl serves double duty here)
                int rc = pthread_create(&tid, NULL,ACKrepeaterThread,abuff); //Create ack repeater thread

                if (rc != 0) {                         //Make sure it starts
                    cerr << "Error: ACKrepeaterThread failed to start" << endl;
                    abuff->ttl = 0;
                } else 
                    pthread_detach(tid);  //Run detached to free resources.
            }
            
            memset(rfbuf, 0, RFBUFSIZE);               //Clear the RF buffer
            if (abuff->valid_ax25)
                strncpy(rfbuf,abuff->data.c_str(), RFBUFSIZE-2); //copy only data portion to rf buffer
                                                                     //and truncate to 250 bytes
            else
                strncpy(rfbuf,abuff->c_str(), RFBUFSIZE-2); //If it had no ax25 header copy the whole thing.

            RemoveCtlCodes(rfbuf);     //remove control codes.
            strcat(rfbuf,"\r");         //append a CR to the end

            char* cp = new char[301];  //Will be deleted by conQueue reader.
            memset(cp, 0, 301);
            ostrstream msg(cp,300);
            msg << "Sending to TNC: " << rfbuf << endl << ends; //debug only
            conQueue.write(cp,0);

            TotalTNCtxChars += strlen(rfbuf);

            if (!tncMute){
                if (abuff->reformatted) {
                    msg_cnt++;
                    memset(szUserMsg, 0, UMSG_SIZE);
                    ostrstream umsg(szUserMsg,UMSG_SIZE-1);
                    umsg  << abuff->peer << " " << abuff->user
                            << ": "
                            << abuff->getChar()
                            << ends;
                    //ostringstream szUserMsg;
                    //szUserMsg << abuff->peer << " " << abuff->user
                    //    << ": "
                    //    << abuff->getChar();
                    // Save the station-to-station message in the log
                    WriteLog(szUserMsg, STSMLOG);
                }
                //WriteCom(rfbuf);    // <- Send string out on RF via TNC
                rfWrite(rfbuf);
            }
        }
    }
    ts.tv_sec = 0;
    ts.tv_nsec = 10000; // 10 uS timeout for nanosleep()

    if (abuff){
        while (abuff->ttl > 0)
            nanosleep(&ts, NULL) ;  //wait 'till it's safe to delete this...

        delete abuff;                      // ...ack repeater thread will set ttl to zero
    }                                      //...Perhaps the ack repeater should delete this?

    if (rfbuf)
        delete rfbuf;

    return ;
}


//-----------------------------------------------------------------------
int SendSessionStr(int session, const char *s)
{
    int rc, retrys;
    timespec ts;
    Lock sendLock(pmtxSend);

    ts.tv_sec = 0;
    ts.tv_nsec = 50000000;  //50mS timeout for nanosleep()
    retrys = 0;

    do {
        if ((rc = send(session, s, strlen(s), 0)) < 0) {
            nanosleep(&ts,NULL) ;
            retrys++;
        }   //try again 50ms later
    } while ((rc < 0) && (errno == EAGAIN) && (retrys <= MAXRETRYS));
    return rc;
}

//-----------------------------------------------------------------------
void endSession(int session, char* szPeer, char* userCall, time_t starttime)
{
    char infomsg[MAX];
    Lock sendLock(pmtxSend, false); // don't get this lock quite yet...

    if (ShutDownServer)
        pthread_exit(0);

    sendLock.get();                     // Be sure not to close during a send() opteration
    DeleteSession(session);             // remove it  from list
    shutdown(session,2);
    close(session);                     // Close socket
    sendLock.release();

    {
        char* cp = new char[128];
        memset(cp, 0, 128);
        ostrstream msg(cp, 127);
        //ostringstream msg;
        msg << szPeer << " " << userCall
            << " has disconnected" << endl
            << ends;

        conQueue.write(cp, 0);
    }

    //strncpy(szLog, szPeer, 16);
    //strcat(szLog, " ");
    //strcat(szLog, userCall);
    //strcat(szLog, " disconnected ");
    ostringstream sLog;
    sLog << szPeer << " " << userCall << " disconnected";
    time_t endtime = time(NULL);
    double  dConnecttime = difftime(endtime , starttime);
    int iMinute = (int)(dConnecttime / 60);
    iMinute = iMinute % 60;
    int iHour = (int)dConnecttime / 3600;
    int iSecond = (int)dConnecttime % 60;
    char timeStr[32];
    sprintf(timeStr, "%3d:%02d:%02d", iHour, iMinute, iSecond);
    //strcat(szLog, timeStr);
    sLog << timeStr;        // time already adds CFLF

    WriteLog(sLog.str(), MAINLOG);

    /*{
        memset(infomsg, 0, MAX);
        ostrstream msg(infomsg,MAX-1);

        msg << szServerCall
            << szJAVAMSG
            << MyLocation << " "
            << szServerID
            << szPeer
            << " " << userCall
            << " disconnected. "
            << getConnectedClients()
            << " users online.\r\n"
            << ends;
    }

    BroadcastString(infomsg); //Say IP address of disconected client
    */

    if (strlen(userCall) > 0) {
        memset(infomsg, 0, MAX);
        ostrstream msg(infomsg,MAX-1);
        //ostringstream msg;

        msg  << szServerCall
            << szAPRSDPATH
            << szUSERLISTMSG
            << MyLocation
            << ": Disconnected from "
            << userCall
            << ". " << getConnectedClients() << " users"
            << "\r\n"
            << ends;

        BroadcastString(infomsg); //Say call sign of disconnected client
        //BroadcastString(msg.str());
    }

    pthread_exit(0);
}
//----------------------------------------------------------------------
//Sends an aprs unnumbered message.  No acknowledgement expected.
void sendMsg(int session, const string& from, const string& to, const string& text)
{
    //char buff[256];
    string dest;
    //ostrstream msg(buff,255);
    dest = to;
    ostringstream msg;

    while (dest.length() < 9)
        dest += " ";  //Pad destination with spaces

    msg << from
        << ">"
        << PGVERS
        << "::"
        << dest
        << ":"
        << text
        << "\r\n";
        //<< ends;

    SendSessionStr(session, msg.str().c_str()); //buff);//Send ack to user
}

//-----------------------------------------------------------------------
//Sends an aprs ack message
void sendAck(int session, const string& from, const string& to, const string& acknum)
{
    // char buff[64];
    string dest;
    //ostrstream msg(buff,63);
    dest = to;
    ostringstream msg;

    while (dest.length() < 9)
        dest += " ";  //Pad destination with spaces

    msg << from
        << ">"
        << PGVERS
        << "::"
        << dest
        << ":ack"
        << acknum
        << "\r\n";
        //<< ends;

    SendSessionStr(session, msg.str().c_str()); //buff);//Send ack to user
     // cerr << "ACK: " << buff;    //DEBUG
}


//-----------------------------------------------------------------------
//An instance of this thread is created for each USER who connects.
void *TCPSessionThread(void *p)
{
    char buf[BUFSIZE];
    string pgm_vers;
    bool omniPort = false;
    SessionParams *psp = (SessionParams*)p;
    int session = psp->Socket;
    echomask_t EchoMask = psp->EchoMask;
    int serverport = psp->ServerPort;
    timespec ts;
    bool cmd_lock = true;  //When set TRUE user commands locked out.
    Lock countLock(pmtxCount, false);

    if(EchoMask & omniPortBit) {
        omniPort = true;   //User defines streams he wants
        cmd_lock = false;  //Allow commands
    }

    delete psp;

    int BytesRead, i;
    struct sockaddr_in peer_adr;
    char szPeer[MAX], szErrorUsers[MAX], szErrorLoad[MAX], szLog[MAX], infomsg[MAX], logmsg[MAX];

    const char *szUserStatus;
    unsigned char c;
    char star = '*';

    unsigned adr_size = sizeof(peer_adr);
    int n, rc,data;
    bool verified = false, loggedon = false;
    ULONG State = BASE;
    char userCall[10];
    char tc, checkdeny;
    const char *szRestriction;
    //int dummy;

    //These deal with Telnet protocol option negotiation suppression
    int iac,sbEsc;

#define IAC 255
#define SB 250
#define SE 240
#define DO 253
#define DONT 254
#define WILL 251
#define WONT 252
#define Echo 1
#define SGA 3
#define OPT_LINEMODE 34

    //This string puts a telnet client into character at a time mode
    unsigned char character_at_a_time[] = {IAC, WILL, SGA, IAC, WILL, Echo, '\0'};

    //Puts telnet client in lin-at-a-time mode
    //unsigned char line_at_a_time[] = {IAC,WONT,SGA,IAC,WONT,Echo,'\0'};

    char szUser[16], szPass[16];

    if (session < 0)
        return NULL;

    memset(szPeer, 0, MAX);
    memset(szLog, 0, MAX);
    memset(infomsg, 0, MAX);
    memset(logmsg, 0, MAX);

    //pthread_mutex_lock(pmtxCount);
    countLock.get();
    TotalConnects++;
    //pthread_mutex_unlock(pmtxCount);
    countLock.release();

    time_t  starttime = time(NULL);

    szPeer[0] = '\0';
    userCall[0] = '\0';

    if (getpeername(session, (struct sockaddr *)&peer_adr, &adr_size) == 0)
        strncpy(szPeer, inet_ntoa(peer_adr.sin_addr), 32);

    {
        memset(szErrorUsers, 0, MAX);
        memset(szErrorLoad, 0, MAX);
        ostrstream msg1(szErrorUsers, MAX-1); //Build an error message in szErrorUsers
        ostrstream msg2(szErrorLoad, MAX-1); //Build an error message in szErrorLoad

        msg1 << szServerCall
            << szJAVAMSG
            << "Limit of "
            << MaxClients
            << " users exceeded.  Try again later. Disconnecting...\r\n"
            << ends;

        msg2 << szServerCall
            << szJAVAMSG
            << "Server load limit of "
            << MaxLoad
            << " bytes/sec exceeded.  Try again later. Disconnecting...\r\n"
            << ends;
    }

    {
        char* cp = new char[256];
        memset(cp, 0, 256);
        ostrstream msg(cp, 255);
        msg << szPeer << " has connected to port " << serverport << endl << ends;
        conQueue.write(cp,0);  //queue reader deletes cp
    }

    {
        ostringstream msg;
        msg << szPeer
            << " connected on "
            << serverport;
            /* << "   " << aprsString::getObjCount() << "/" << ItemCount */  //Debug
            //<< ends;

        WriteLog(msg.str(), MAINLOG);
    }

    data = 1;  //Set socket for non-blocking
    ioctl(session, FIONBIO, (char*)&data, sizeof(int));

    if (EchoMask == 0)
        rc = SendSessionStr(session, (char*)character_at_a_time);  //Sysop mode. Put Telnet client in char mode

    rc = SendSessionStr(session, SIGNON);

    if (rc < 0)
        endSession(session, szPeer, userCall, starttime);

    if (NetBeacon.length() != 0)
        rc = SendSessionStr(session, NetBeacon.c_str());

    if (rc < 0)
        endSession(session, szPeer, userCall, starttime);

    if ((EchoMask & sendHISTORY) && (History_ALLOW)){
        n = SendHistory(session, (EchoMask & ~(srcSYSTEM | srcSTATS)));   //Send out the previous N minutes of APRS activity
                                                                           //except messages generated by this system.
        if (n < 0) {
            ostringstream msg;
            msg << szPeer
                << " aborted during history dump on "
                << serverport;
                //<< ends;

            WriteLog(msg.str(), MAINLOG);
            endSession(session, szPeer, userCall, starttime);
        }

        {
            char* cp = new char[256];
            memset(cp,0,256);
            ostrstream msg(cp, 255);
            msg << "Sent " << n << " history items to " << szPeer << endl << ends;
            conQueue.write(cp, 0);  //queue reader deletes cp
        }
    }

    if (!omniPort){
        char *pWelcome = new char[CONFPATH.length() + WELCOME.length() + 1];
        strcpy(pWelcome, CONFPATH.c_str());
        strcat(pWelcome, WELCOME.c_str());
        rc = SendFiletoClient(session, pWelcome);		 //Read Welcome message from file
        if (rc < 0) {
            ostringstream msg;
            msg << szPeer
                << " aborted welcome msg on "
                << serverport;

            WriteLog(msg.str(), MAINLOG);
            delete pWelcome;
            endSession(session, szPeer, userCall, starttime);
        }
        delete pWelcome;
    }

    SessionParams* sp =  AddSession(session,EchoMask,0);
    if (sp == NULL) {
        if (serverLoad_s < MaxLoad){
            rc = SendSessionStr(session,szErrorUsers);
            WriteLog(string("Error, too many users "), MAINLOG);
        } else {
            rc = SendSessionStr(session,szErrorLoad);
            WriteLog(string("Error, max server load exceeded"), MAINLOG);
        }

        if (rc == -1)
            perror("AddSession");

        endSession(session, szPeer, userCall, starttime);
        char* cp = new char[256];
        memset(cp,0,256);
        ostrstream msg(cp,255);
        msg <<  "Can't add client to session list, too many users or max load exceeded - closing connection.\n"
            << ends;

        conQueue.write(cp,0);
    }

    AddSessionInfo(session,"*",szPeer,serverport, "*");

    if(EchoMask == 0)   //Empty echomask means this is a Sysop TNC session
        rc = SendSessionStr(session,"\r\nHit Esc key to login for TNC access, ctrl D to disconnect.\r\n");


    {
        memset(infomsg, 0, MAX);
        ostrstream msg(infomsg, MAX-1);

        msg << szServerCall
            << szJAVAMSG
            << MyLocation << " "
            << szServerID
            << szPeer
            << " connected "
            << getConnectedClients()
            << " users online.\r\n"
            << ends;

        aprsString logon_msg(infomsg, SRC_INTERNAL, srcSYSTEM);
        BroadcastString(infomsg);

        if ((EchoMask & srcSYSTEM) == 0) {         //If user can't see SYSTEM messages
            logon_msg.addIIPPE('Z');
            logon_msg.addPath(szServerCall);
            SendSessionStr(session,logon_msg.c_str());//Send a copy directly to him
        }
    }

    int cc = getConnectedClients();

    if (cc > MaxConnects)
        MaxConnects = cc;

    iac = 0;
    sbEsc = false;

    do {
        /*
            The logic here is that 1 char is fetched
            on each loop and tested for a CR or LF	before
            the buffer is processed.  If "i" is -1 then
            any socket error other that "would block" causes
            BytesRead and i to be set to zero which causes
            the socket to be closed and the thread terminated.
            If "i" is greater than 0 the character in "c" is
            put in the buffer.
        */

        BytesRead = 0;  //initialize byte counter

        do {
            if ((charQueue.ready()) && (State == REMOTE) ) {
                int ic;
                charQueue.read(&ic);   //RH7 FIX   11-1-00
                tc = (char)ic;
                send(session,&tc,1,0);   //send a tnc character to sysop
                //printhex(&tc,1);
            }

            do {
                c = 0xff;
                i = recv(session,&c,1,0); //get 1 byte into c
            } while (c == 0x00);

            if ((State == USER) && (i != -1)) 
                send(session,&c,1,0);   //Echo chars back to telnet client if TNC USER logon
               
            if ((State == PASS) && (i != -1) && (c != 0x0a) && (c != 0x0d)) 
                send(session,&star,1,0); //Echo "*" if TNC password is being entered.

            if (ShutDownServer) 
                raise(SIGTERM);  //Terminate this process

            //if (State == REMOTE) printhex(&c,1); debug
            if (i == -1) {
                if (errno != EWOULDBLOCK) {
                    BytesRead = 0;  //exit on errors other than EWOULDBLOCK
                    i = 0;
                }

                //cerr << "i=" << i << "  chTimer=" << chTimer << "   c=" << c << endl; 
                if (State != REMOTE) {
                    ts.tv_sec = 1;
                    ts.tv_nsec = 0;
                    nanosleep(&ts,NULL);  // Don't hog cpu while in loop awaiting data
                }
            }

            if (sp->dead) {      //force disconnect if connection is dead
                BytesRead = 0;
                i = 0;
            }

            if (i != -1) {  //Got a real character from the net
                if (loggedon == false) {
                    if ((c == IAC) && (sbEsc == false))
                        iac = 3;  //This is a Telnet IAC byte.  Ignore the next 2 chars

                    if ((c == SB) && (iac == 2))
                        sbEsc = true;    //SB is suboption begin.
                }  //Ignore everything until SE, suboption end.

                // printhex(&c,1);  //debug
                //if ( !((lastch == 0x0d) && (c == 0x0a)) && (c != 0) && (iac == 0) )	  //reject LF after CR and NULL

                // This logic discards CR or LF if it's the first character on the line.
                bool cLFCR =  (( c == LF) || ( c == CR));
                bool rejectCH = (((BytesRead == 0) && cLFCR) || (c == 0)) ;  //also discard NULLs

                if ((rejectCH == false) && (iac == 0)) {
                    if (BytesRead < BUFSIZE-3) 
                        buf[BytesRead] = c;

                    BytesRead += i;
                    //Only enable control char interpreter if user is NOT logged on in client mode
                    if (loggedon == false){
                        switch (c) {
                            case 0x04:
                                i = 0;  //Control-D = disconnect
                                BytesRead = 0;
                                break;

                            case 0x1b:
                                if ((State == BASE) && tncPresent && (EchoMask == 0)) {  //ESC = TNC control
                                    sp->EchoMask = 0;  //Stop echoing the aprs data stream.
                                    rc = SendSessionStr(session,"\r\n220 Remote TNC control mode. <ESC> to quit.\r\n503 User:");
                                    if (rc < 0)
                                        endSession(session,szPeer,userCall,starttime);

                                    State = USER;    //   Set for remote control of TNC
                                    BytesRead = 0;
                                    break;
                                }

                                //<ESC> = exit TNC control
                                if ((State != BASE) && tncPresent) {
                                    if (State == REMOTE){
                                        ostringstream log;
                                        log << szPeer << " "
                                            << szUser << " "
                                            << " Exited TNC remote sysop mode."
                                            << endl;

                                        WriteLog(log.str(), MAINLOG);
                                    }
                                    tncMute = false;
                                    TncSysopMode = false;
                                    State = BASE;  // <ESC>Turn off remote
                                    rc = SendSessionStr(session,"\r\n200 Exit remote mode successfull\r\n");
                                    if (rc < 0)
                                        endSession(session,szPeer,userCall,starttime);

                                    sp->EchoMask = EchoMask;  //Restore aprs data stream.
                                    BytesRead = 0;  //Reset buffer
                                }
                                //i = 0;
                                break;
                        };
                    }//end if (loggedon==false)

                    if ((State == REMOTE) && (c != 0x1b) && (c != 0x0) && (c != 0x0a)) {
                        char chbuf[2];
                        chbuf[0] = c;
                        chbuf[1] = '\0';
                        //WriteCom(chbuf); //Send chars to TNC in real time if REMOTE
                        rfWrite(chbuf);
                        send(session, chbuf, 1, 0);  //Echo chars back to telnet client
                        //printhex(chbuf,1);  //Debug
                    }
                }
            }else
                c = 0;

            if (loggedon == false){
                if (c == SE) { 
                    sbEsc = false; 
                    iac = 0;
                }  //End of telnet suboption string
                
                if (sbEsc == false) 
                    if(iac-- <= 0) 
                        iac = 0;  //Count the bytes in the Telnet command string
            }

            //Terminate loop when we see a CR or LF if it's not the first char on the line.
        } while (((i != 0) && (c != 0x0d) && (c != 0x0a))||((BytesRead == 0) && (i != 0)));

        if ((BytesRead > 0) && ( i > 0)) {  // 1 or more bytes needed
            i = BytesRead - 1;
            buf[i++] = 0x0d;        //Put a CR-LF on the end of the buffer
            buf[i++] = 0x0a;
            buf[i++] = 0;

            //pthread_mutex_lock(pmtxCount);
            countLock.get();

            if (sp) {
                TotalUserChars += i - 1;     //Keep a tally of input bytes on this connection.
                sp->bytesIn += i - 1;
                sp->lastActive = time(NULL);
            }

            //pthread_mutex_unlock(pmtxCount);
            countLock.release();

            if (State == REMOTE) {
                buf[i-2] = '\0';    //No line feed required for TNC
                //printhex(buf,strlen(buf));    //debug code
            }

            if (State == BASE) {          //Internet to RF messaging handler
                bool sentOnRF=false;
                aprsString atemp(buf,session,srcUSER,szPeer,userCall);

                /*
                     if(atemp.aprsType == APRSQUERY){ // non-directed query ?
                        queryResp(session,&atemp);   // yes, send our response
                     }
                */

                /*
                    if(atemp.aprsType == APRSMSG) {    //DEBUG
                       cerr << atemp.c_str()
                           << "*" << atemp.ax25Source << "*"
                           << " msgType=" << atemp.msgType
                          << "  userCall=*" << userCall <<  "*"
                          << "  omniPort=" << omniPort
                          << "  cmd_lock=" << cmd_lock
                          << "  acknum=" << atemp.acknum
                          << endl;
                    }
                */

                // User stream filter request commands processed here
                if((atemp.aprsType == APRSMSG)     //Server command in a message
                        && (atemp.msgType == APRSMSGSERVER)
                        && (omniPort == true)          //This only works on omniPort
                        && (cmd_lock == false)) {       //Commands not locked out

                    if (atemp.ax25Source.compare(userCall) == 0){ // From logged on user ONLY
                        string s;
                        atemp.getMsgText(s);   //Extract message text into string s.

                        aprsString cmd(s.c_str(),session,srcUSER,szPeer,userCall); //Make s into aprsString

                        if (cmd.cmdType == CMDPORTFILTER){
                            EchoMask = cmd.EchoMask;  //Set user requested EchoMask
                            sp->EchoMask = cmd.EchoMask;
                            atemp.EchoMask = 0;
                            printf("Stream Filter EchoMask= %08lX\n",EchoMask);  //Debug

                            if (atemp.acknum.length() > 0)                      //If ack number present...
                                sendAck(session,"SERVER",userCall,atemp.acknum);  // Then send ACK

                            if (EchoMask & sendHISTORY)
                                SendHistory(session,(EchoMask & ~(srcSYSTEM | srcSTATS)));
                        }
                    }
                }

                if((atemp.aprsType == CONTROL)   //Naked control command
                        && (omniPort == true)         //To omniport only
                        && (cmd_lock == false)        //Commands not locked out
                        && (loggedon == false)){      //Execute only prior to logon!

                    if(atemp.cmdType == CMDPORTFILTER){
                        EchoMask = atemp.EchoMask;  //Set user requested EchoMask
                        sp->EchoMask = atemp.EchoMask;
                        // printf("Stream Filter EchoMask= %08lX\n",EchoMask);  //Debug
                        if (EchoMask & sendHISTORY)
                            SendHistory(session,(EchoMask & ~(srcSYSTEM | srcSTATS)));
                    }
                    if (atemp.cmdType == CMDLOCK)
                        cmd_lock = true; //Prevent future command execution.
                }

                string vd;
                unsigned idxInvalid=0;
                if (atemp.aprsType == APRSLOGON) {
                    loggedon = true;
                    verified = false;
                    vd = atemp.user + atemp.pass ;

                    /*
                        2.0.7b Security bug fix - don't allow ;!@#$%~^&*():="\<>[]  in user or pass!
                    */
                    if (((idxInvalid = vd.find_first_of(";!@#$%~^&*():=\"\\<>[]",0,20)) == string::npos)
                            && (atemp.user.length() <= 15)   /*Limit length to 15 or less*/
                            && (atemp.pass.length() <= 15)){

                        if (validate(atemp.user.c_str(), atemp.pass.c_str(),APRSGROUP, APRS_PASS_ALLOW) == 0)
                            verified = true;
                    } else {
                        if (idxInvalid != string::npos){
                            char *cp = new char[256];
                            memset(cp,0,256);
                            ostrstream msg(cp,255);

                            msg << szPeer
                                << " Invalid character \""
                                << vd[idxInvalid]
                                << "\" in APRS logon"
                                << endl
                                << ends ;

                            conQueue.write(cp,0);    //cp deleted by queue reader
                            WriteLog(cp,MAINLOG);
                        }
                    }

                    checkdeny = toupper(checkUserDeny(atemp.user));  // returns + , L or R
                                                                                // + = no restriction
                                                                                // L = No login
                                                                                // R = No RF
                    if (verified) {
                        szUserStatus = szVERIFIED ;
                        if (sp)
                            sp->vrfy = true;
                    } else
                        szUserStatus = szUNVERIFIED;

                    switch (checkdeny){
                        case 'L':
                            szUserStatus = szACCESSDENIED; //Read only access
                            szRestriction = szRM2;
                            verified = false;
                            break;

                        case 'R':
                            szRestriction = szRM3;  //No send on RF access
                            break;

                        default:
                            szRestriction = szRM1;
                    }

                    if (checkdeny != 'L'){
                        memset(infomsg,0,MAX);
                        ostrstream msg(infomsg,MAX-1);
                        msg << szServerCall
                            << szAPRSDPATH
                            << szUSERLISTMSG
                            << MyLocation << ": "
                            << szUserStatus << "user "
                            << atemp.user
                            << " logged on using "
                            << atemp.pgmName << " "
                            << atemp.pgmVers
                            << "."
                            << "\r\n"  /* Don't want acks from this! */
                            << ends;

                        aprsString logon_msg(infomsg, SRC_INTERNAL, srcSYSTEM);
                        BroadcastString(infomsg);   //send users logon status to everyone

                        if ((EchoMask & srcSYSTEM) == 0){ //If user can't see system messages
                            logon_msg.addIIPPE('Z');
                            logon_msg.addPath(szServerCall);
                            SendSessionStr(session,logon_msg.c_str()); //send him  his own private copy
                        }                                           //so he knows the logon worked

                    }

                    {
                        ostringstream msg;
                        msg << szPeer
                            << " " << atemp.user
                            << " " << atemp.pgmName
                            << " " << atemp.pgmVers
                            << " " << szUserStatus;
                        
                        WriteLog(msg.str(), MAINLOG);
                    }


                    strncpy(userCall, atemp.user.c_str(), 9);  //save users call sign
                    pgm_vers = atemp.pgmName + " " + atemp.pgmVers;
                    AddSessionInfo(session, userCall, szPeer, serverport, pgm_vers.c_str()); //put it here so HTTP monitor can see it

                    if((!verified) || (checkdeny == 'R')) {
                        char call_pad[] = "         "; //9 spaces
                        int len = 0;
                        if ((len = atemp.user.length()) > 9)
                            len = 9;

                        memmove(call_pad, atemp.user.c_str(), len);

                        {
                            memset(infomsg, 0, MAX);
                            ostrstream msg(infomsg, MAX-1);  //Message to user...
                            msg << szServerCall
                                << szAPRSDPATH
                                << ':'
                                << call_pad
                                << ":" << szRestriction
                                << ends ;
                        }

                        if ((rc = SendSessionStr(session, infomsg)) < 0)
                            endSession(session, szPeer, userCall, starttime);

                        if (checkdeny == '+'){
                            memset(infomsg,0,MAX);
                            ostrstream msg(infomsg,MAX-1); //messsage to user
                            msg << szServerCall
                                << szAPRSDPATH
                                << ':'
                                << call_pad
                                << ":Contact program author for registration number.\r\n"
                                << ends ;

                            if ((rc = SendSessionStr(session, infomsg)) < 0)
                                endSession(session, szPeer, userCall, starttime);
                        }
                    }

                    if (verified && (atemp.pgmName.compare("monitor") == 0)) {
                        if (sp){
                            sp->EchoMask = srcSTATS;
                            char prompt[] = "#Entered Monitor mode\n\r";
                            aprsString *amsg = new aprsString(prompt, SRC_USER, srcSTATS);
                            sendQueue.write(amsg);
                            AddSessionInfo(session, userCall, szPeer, serverport, "Monitor");
                        }
                    }
                }

                // One of the stations in the gate2rf list?
                bool RFalways = find_rfcall(atemp.ax25Source,rfcall);

                if (verified  && (!RFalways) && (atemp.aprsType == APRSMSG) && (checkdeny == '+')) {
                    sentOnRF = false;
                    atemp.changePath("TCPIP*","TCPIP");
                    sentOnRF = sendOnRF(atemp, szPeer, userCall, srcUSERVALID);   // Send on RF if dest local

                    if (sentOnRF) { //Now find the posit for this guy in the history list
                                    // and send it too.
                        aprsString* posit = getPosit(atemp.ax25Source,srcIGATE | srcUSERVALID);
                        if (posit != NULL) {
                            time_t Time = time(NULL);           //get current time
                            if ((Time - posit->timeRF) >= 60*10) { //every 10 minutes only
                                timestamp(posit->ID, Time); //Time stamp the original in hist. list
                                if (posit->thirdPartyReformat(MyCall.c_str())) // Reformat it for RF delivery
                                    tncQueue.write(posit);    //posit will be deleted elsewhere
                                else
                                    delete posit;  //Delete if conversion failed
                            } else
                                delete posit;
                        } /*else cout << "Can't find matching posit for "
                                                << atemp.ax25Source
                                                << endl
                                                << flush;        //Debug only
                                                */
                    }
                }

                // Filter out COMMENT type packets, eg: # Tickle
                if ( verified && (atemp.aprsType != COMMENT)
                        && (atemp.aprsType != APRSLOGON)  //No logon strings
                        && (atemp.msgType != APRSMSGSERVER)  //No Server control messages
                        && (atemp.aprsType != CONTROL)) {     //No naked control strings
                    
                    aprsString* inetpacket = new aprsString(buf, session, srcUSERVALID, szPeer, userCall);
                    inetpacket->changePath("TCPIP*", "TCPIP") ;

                    if (inetpacket->aprsType == APRSMIC_E)   //Reformat Mic-E packets
                        reformatAndSendMicE(inetpacket,sendQueue);
                    else
                        sendQueue.write(inetpacket); //note: inetpacket is deleted in DeQueue
                }

                /*
                    Here we allow users who logged on with a CALL but used -1 as a password to
                    send data to the internet but restrict them to there OWN packets and
                    we add TCPXX* to the path
                */

                if (!verified && (atemp.aprsType != COMMENT) //Not a comment
                        && (atemp.aprsType != APRSLOGON)         //Not a logon
                        && (atemp.valid_ax25)                    //Valid AX25 format
                        && (checkdeny != 'L')) {                  //Not denied

                    aprsString* inetpacket = new aprsString(buf,session,srcUSER,szPeer,userCall);
                    bool fromUser = false;

                    // compare users logon call with packet source call (case sensitive)
                    if (inetpacket->ax25Source.compare(userCall) == 0)
                        fromUser = true;

                    if (fromUser){ //Ok, packet has users call in the FROM field  (case sensitive match)
                        inetpacket->changePath("TCPIP*", "TCPIP") ;

                    if (inetpacket->changePath("TCPXX*", "TCPIP*")) //Change to TCPXX*
                        inetpacket->changeIIPPE("qAX");            //Change qA? to qAX if present
                    else
                        inetpacket->aprsType = APRSREJECT;  //Reject if no TCPIP in path
                } else {  //Not from user
                    inetpacket->aprsType = APRSREJECT;  //Unverified users not allowed to relay from others
                }

                sendQueue.write(inetpacket); //note: inetpacket is deleted in DeQueue
            }

            if ((atemp.aprsType == APRSMSG) && (RFalways == false) ) {
                aprsString* posit = getPosit(atemp.ax25Source, srcIGATE | srcUSERVALID | srcTNC);
                if (posit != NULL) {
                    posit->EchoMask = src3RDPARTY;
                    sendQueue.write(posit);  //send matching posit only to msg port
                }
            }

            // Here's where the priviledged get their posits sent to RF full time.
            if (configComplete
                    && verified
                    && RFalways
                    && (StationLocal(atemp.ax25Source, srcTNC) == false)
                    && (atemp.tcpxx == false)
                    && (checkdeny == '+')) {
                     
                aprsString* RFpacket = new aprsString(buf, session, srcUSER, szPeer, userCall);
                RFpacket->changePath("TCPIP*","TCPIP");

                if (RFpacket->aprsType == APRSMIC_E) {  //Reformat Mic-E packets
                    if (ConvertMicE){
                        aprsString* posit = NULL;
                        aprsString* telemetry = NULL;
                        RFpacket->mic_e_Reformat(&posit,&telemetry);
                               
                        if (posit){
                            posit->thirdPartyReformat(MyCall.c_str());  // Reformat it for RF delivery
                            tncQueue.write(posit);       //Send reformatted packet on RF
                        }
                               
                        if (telemetry) {
                            telemetry->thirdPartyReformat(MyCall.c_str()); // Reformat it for RF delivery
                            tncQueue.write(telemetry);      //Send packet on RF
                        }

                        delete RFpacket;
                    } else {
                        if (RFpacket->thirdPartyReformat(MyCall.c_str()))
                            tncQueue.write(RFpacket);  // Raw MIC-E data to RF
                        else 
                            delete RFpacket;  //Kill it if 3rd party conversion fails.
                    }
                } else {
                    if (RFpacket->thirdPartyReformat(MyCall.c_str()))
                        tncQueue.write(RFpacket);  // send data to RF
                    else 
                        delete RFpacket;   //Kill it if 3rd party conversion fails.
                }
            }
        }
        int j = i-3;

        if ((State == PASS) && (BytesRead > 1)) {
            strncpy(szPass,buf,15);
                if (j<16)
                    szPass[j] = '\0';
                else 
                    szPass[15] = '\0';

                bool verified_tnc = false;
                unsigned idxInvalid=0;

                int valid = -1;

                string vd = string(szUser) + string(szPass) ;

                // 2.0.7b Security bug fix - don't allow ;!@#$%~^&*():="\<>[]  in szUser or szPass!
                //Probably not needed in 2.0.9 because validate is not an external pgm anymore!

                if (((idxInvalid = vd.find_first_of(";!@#$%~^&*():=\"\\<>[]",0,20)) == string::npos)
                        && (strlen(szUser) <= 16)   //Limit length to 16 or less
                        && (strlen(szPass) <= 16)){

                    valid = validate(szUser,szPass,TNCGROUP,APRS_PASS_ALLOW);  //Check user/password
                } else {
                    if (idxInvalid != string::npos){
                        char *cp = new char[256];
                        memset(cp,0,256);
                        ostrstream msg(cp,255);

                        msg << szPeer
                            << " Invalid character \""
                            << vd[idxInvalid]
                            << "\" in TNC logon"
                            << endl
                            << ends ;

                        conQueue.write(cp,0);    //cp deleted by queue reader
                        WriteLog(cp,MAINLOG);
                    }
                }

                if (valid == 0)
                    verified_tnc = true;

                if (verified_tnc) { 
                    if(TncSysopMode == false) { 
                        TncSysopMode = true;
                        State = REMOTE;
                        tncMute = true;
                        if ((rc = SendSessionStr(session,"\r\n230 Login successful. <ESC> to exit remote mode.\r\n")) < 0)
                            endSession(session,szPeer,userCall,starttime);
 
                        ostringstream log;
                        log << szPeer << " "
                            << szUser
                            << " Entered TNC remote sysop mode."
                            << endl;

                        WriteLog(log.str(), MAINLOG);
                    } else {
                        if ((rc = SendSessionStr(session,"\r\n550 Login failed, TNC is busy\r\n")) < 0)
                            endSession(session,szPeer,userCall,starttime);

                        ostringstream log;
                        log << szPeer << " "
                            << szUser
                            << " Login failed: TNC busy."
                            << endl;

                        WriteLog(log.str(), MAINLOG);

                        State = BASE;

                        if (sp){
                            sp->EchoMask = EchoMask;  //Restore original echomask
                            AddSessionInfo(session, userCall, szPeer, serverport, pgm_vers.c_str());
                        } else {
                            /* failed to get a session */
                        }
                    }
                } else {
                    if ((rc = SendSessionStr(session,"\r\n550 Login failed, invalid user or password\r\n")) < 0)
                        endSession(session, szPeer, userCall, starttime);

                    ostringstream log;
                    log << szPeer << " "
                        << szUser  << " "
                        << szPass
                        << " Login failed: Invalid user or password."
                        << endl;

                    WriteLog(log.str(), MAINLOG);
                    State = BASE;

                    if (sp) {
                        sp->EchoMask = EchoMask;
                        AddSessionInfo(session, userCall, szPeer, serverport, pgm_vers.c_str());
                    } else {
                        /* failed to get a session */
                    }
                }
            }

            if ((State == USER) && (BytesRead > 1)) {
                strncpy(szUser,buf,15);
                if (j < 16)
                    szUser[j] = '\0';
                else
                    szUser[15]='\0';

                State = PASS;
                if ((rc = SendSessionStr(session, "\r\n331 Pass:")) < 0)
                    endSession(session, szPeer, userCall, starttime);
            }
        }
    } while (BytesRead != 0);// Loop back and get another line from remote user.

    if (State == REMOTE){
        tncMute = false;
        TncSysopMode = false;
    }

    endSession(session, szPeer, userCall, starttime);

    pthread_exit(0);  //Actually thread exits from endSession above.
}


//------------------------------------------------------------------------

// One instance of this thread is created for each port definition in aprsd.conf.
// Each instance listens on the a user defined port number for clients
// wanting to connect.  Each connect request is assigned a
// new socket and a new instance of TCPSessionThread() is created.
void *TCPServerThread(void *p)
{
    int s = 0, rc = 0;
    unsigned i;
    SessionParams* session;
    pthread_t SessionThread;
    int backlog = 5;            // Backlog of pending connections
    
    struct sockaddr_in server,client;
    timespec ts;

    int optval;
    ServerParams *sp = (ServerParams*)p;

    sp->pid = getpid();
    s = socket(PF_INET, SOCK_STREAM, 0);
    sp->ServerSocket = s;
    optval = 1; //Allow address reuse
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char*)&optval, sizeof(int));
    setsockopt(s, SOL_SOCKET, SO_KEEPALIVE, (char*)&optval, sizeof(int));

    if (s == 0) {
        perror("TCPServerThread socket error");
        ShutDownServer = true;
        return NULL;
    }

    memset(&server, 0, sizeof(server));
    memset(&client, 0, sizeof(client));

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(sp->ServerPort);

    if (bind(s, (struct sockaddr *)&server, sizeof(server)) <  0) {
        perror("TCPServerThread bind error");
        ShutDownServer = true;
        return NULL;
    }

    ts.tv_sec = 0;
    ts.tv_nsec = 100000000;  //100mS timeout for nanosleep()

    while (!configComplete)
        nanosleep(&ts, NULL);  //Wait till everything else is running.

    listen(s, backlog);

    for(;;) {
        i = sizeof(client);
        session = new SessionParams;
        session->Socket = accept(s, (struct sockaddr *)&client, &i);
        session->EchoMask = sp->EchoMask;
        session->ServerPort = sp->ServerPort;
        if (ShutDownServer) {
            close(s);
            if (session->Socket >= 0) close(session->Socket);
                cerr << "Ending TCP server thread" << endl;

            delete session;
            if (ShutDownServer)
                raise(SIGTERM);  //Terminate this process
        }
        if (session->Socket < 0) {
            perror( "Error in accepting a connection");
            delete session;
        } else
            if (session->EchoMask & wantHTML) {
                rc = pthread_create(&SessionThread, NULL, HTTPServerThread, session);  //Added in 2.1.2
            } else
                rc = pthread_create(&SessionThread, NULL, TCPSessionThread, session);

        if (rc  != 0) {
            cerr << "Error creating new client thread." << endl;
            shutdown(session->Socket,2);
            rc = close(session->Socket);      // Close it if thread didn't start
            delete session;
            if (rc < 0)
                perror("Session Thread close()");
        } else   //session will be deleted in TCPSession Thread
            pthread_detach(SessionThread);	 //run session thread DETACHED!

        memset(&client, 0, sizeof(client));
    }
   return 0;
}



//----------------------------------------------------------------------
//This thread listens to a UDP port and sends all packets heard to all
//logged on clients unless the destination call is "TNC" it sends it
//out to RF.

void *UDPServerThread(void *p)
{
#define UDPSIZE 256
    int s,i;
    unsigned client_address_size;
    struct sockaddr_in client, server;
    char buf[UDPSIZE+3];
    UdpParams* upp = (UdpParams*)p;
    int UDP_Port = upp->ServerPort;   //UDP port set in aprsd.conf
    char *CRLF = "\r\n";
    Lock countLock(pmtxCount, false);

    upp->pid = getpid();

   /*
    * Create a datagram socket in the internet domain and use the
    * default protocol (UDP).
    */
    if ((s = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Failed to create datagram socket");
        ShutDownServer = true;
        return NULL;
    }

    /*
     *
     * Bind my name to this socket so that clients on the network can
     * send me messages. (This allows the operating system to demultiplex
     * messages and get them to the correct server)
     *
     * Set up the server name. The internet address is specified as the
     * wildcard INADDR_ANY so that the server can get messages from any
     * of the physical internet connections on this host. (Otherwise we
     * would limit the server to messages from only one network interface)
    */
    server.sin_family = AF_INET;  /* Server is in Internet Domain */
    server.sin_port = htons(UDP_Port) ;/* 0 = Use any available port       */
    server.sin_addr.s_addr = INADDR_ANY;	/* Server's Internet Address    */

    if (bind(s, (struct sockaddr *)&server, sizeof(server)) < 0) {
        perror("Datagram socket bind error");
        ShutDownServer = true;
        return NULL;
    }

    cout << "UDP Server listening on port " << UDP_Port << endl;

    for (;;) { //Loop forever
        client_address_size = sizeof(client);
        i = recvfrom(s, buf, UDPSIZE, 0, (struct sockaddr *) &client, &client_address_size) ; //Get client udp data
        bool sourceOK = false;
        int n=0;
        do {                  //look for clients IP address in list of trusted addresses.
            long maskedTrusted = Trusted[n].sin_addr.s_addr & Trusted[n].sin_mask.s_addr;
            long maskedClient = client.sin_addr.s_addr & Trusted[n].sin_mask.s_addr;
            if(maskedClient == maskedTrusted)
                sourceOK = true;

            n++;
        } while((n < nTrusted) && (sourceOK == false)) ;

        if (sourceOK && configComplete && (i > 0)){
            int j = strlen(buf);
            if (j < i)
                i = j;       //Remove trailing NULLs

            if (buf[i-1] != '\n')
                strcat(buf,CRLF);  //Add a CR/LF if not present

            ostringstream log;
            log << inet_ntoa(client.sin_addr)
                << ": " << buf;

            WriteLog(log.str(), UDPLOG);

            aprsString* abuff = new aprsString(buf, SRC_UDP, srcUDP, inet_ntoa(client.sin_addr), "UDP");

            countLock.get();
            TotalUdpChars += abuff->length();
            countLock.release();

            //printhex(abuff->c_str(),strlen(abuff->c_str())); //debug

            if (abuff->ax25Dest.compare("TNC") == 0  ) {  //See if it's  data for the TNC
                tncQueue.write(abuff,SRC_UDP);       //Send remaining data from ourself to the TNC
            } else {
                sendQueue.write(abuff,0);  //else send data to all users.
            }                             //Note that abuff is deleted in History list expire func.

        }
        if (ShutDownServer)
            raise(SIGTERM);  //Terminate this process
    }
    return NULL;
}

//----------------------------------------------------------------------

/* Receive a line of ASCII from "sock" . Nulls, line feeds and carrage returns are
removed.  End of line is determined by CR or LF or CR-LF or LF-CR .
CR-LF sequences appended before the string is returned.  If no data is received in "timeoutMax" seconds
it returns int Zero else it returns the number of characters in the received
string.
Returns ZERO if timeout and -2 if socket error.
*/

int recvline(int sock, char *buf, int n, int *err,int timeoutMax)
{
    int c;
    int i,BytesRead ,timeout;
    timespec ts;

    *err = 0;
    BytesRead = 0;
    bool abort ;
    timeout = timeoutMax;
    abort = false;

    do {
        c = -1;
        i = recv(sock,&c,1,0); //get 1 byte into c
        if (i == 0)
            abort = true;   //recv returns ZERO when remote host disconnects

        if (i == -1) {
            *err = errno;
            if ((*err != EWOULDBLOCK) || (ShutDownServer == true)) {
                BytesRead = 0;
                i = -2;
                abort = true;  //exit on errors other than EWOULDBLOCK
            }
            ts.tv_sec = 1;
            ts.tv_nsec = 0;
            nanosleep(&ts, NULL);  // Wait 1 sec. Don't hog cpu while in loop awaiting data

            if (timeout-- <= 0) {
                i = 0;
                abort = true;  //Force exit if timeout
            }

            //cout << timeout << " Waiting...  abort= " << abort << "\n";  //debug code
        }

        if (i == 1) {
            c &= 0xff ;    //  <--- Changed to support 8 bit wide character sets in version 2.1.5

            bool cLFCR =  (( c == LF) || ( c == CR));     //true if c is a LF or CR
            bool rejectCH = (((BytesRead == 0) && cLFCR ) || (c == 0)) ;

            if ((BytesRead < (n - 3)) && (rejectCH == false)) {
                //reject if LF or CR is first on line or it's a NULL
                buf[BytesRead] = (char)c;   //and discard data that runs off the end of the buffer
                BytesRead++;                // We have to allow 3 extra bytes for CR,LF,NULL
                timeout = timeoutMax;
            }
        }
    } while ((c != CR) && (c != LF) && (abort == false)); /* Loop while c is not CR or LF */
                                                       /* And no socket errors or timeouts */

    //cerr << "Bytes received=" << BytesRead << " abort=" << abort << endl;   //debug code

    if ((BytesRead > 0) && (abort == false) ) { // 1 or more bytes needed
        i = BytesRead -1 ;
        buf[i++] = (char)CR;  //make end-of-line CR-LF
        buf[i++] = (char)LF;
        buf[i++] = 0;         //Stick a NULL on the end.
        return i-1;           //Return number of bytes received.
    }
    //if(i == -2) cerr << "errorno= " << *err << endl;    //debug
    return i;  //Return 0 if timeout or
               // Return  -2 if socket error
}


//---------------------------------------------------------------------------------------------

ConnectParams* getNextHub(ConnectParams* pcp)
{
    int i = 0;

    if (!pcp->hub)
        return pcp;

    while((i < nIGATES) && (pcp != &cpIGATE[i])) //Find current hub
        i++;

    //cerr << "Previous hub = " << cpIGATE[i].RemoteName << endl; //debug
    i++;
    while ((i < nIGATES) && (!cpIGATE[i].hub))
        i++; //Find next hub

    if(i == nIGATES){
        i = 0;          //Wrap around to start again

        while ((i < nIGATES) && (!cpIGATE[i].hub))
            i++;
    }

    //cerr << "Next hub = " << cpIGATE[i].RemoteName << endl;   //debug

    if (cpIGATE[i].hub){
        cpIGATE[i].pid = getpid();
        cpIGATE[i].connected = false;
        cpIGATE[i].bytesIn = 0;
        cpIGATE[i].bytesOut = 0;
        cpIGATE[i].starttime = time(NULL);
        cpIGATE[i].lastActive = time(NULL);
        return &cpIGATE[i];   //Return pointer to next hub
    }
    return pcp ;
}



//---------------------------------------------------------------------------------------------

/* ***** TCPConnectThread *****

   This thread connects to another aprsd or other APRS Server machine
   as defined in aprsd.conf with the "server", "igate" and "hub commands.

   One instance of this thread is created for each and every connection.

   Only a one hub thread is created regardless of the number of "hub"
   connectons defined.  Each hub will be tried until an active one is found.
*/

 void *TCPConnectThread(void *p)
{
    int rc = 0, length, state;
    int clientSocket;
    int data;
    ConnectParams *pcp;
    int err;
    int retryTimer;
    timespec ts;
    bool gotID = false;
    time_t connectTime = 0;
    bool hubConn;
    SessionParams *sp = NULL;
    //char ip_hex_alias[32];

    Lock countLock(pmtxCount, false);
    Lock sendLock(pmtxSend, false);

    int h_err;
    char h_buf[1024];
    struct hostent hostinfo_d;

    //struct hostent *hinfo = NULL;
    struct hostent *hostinfo = NULL;
    struct sockaddr_in host;
    char buf[BUFSIZE+1];
    char remoteIgateInfo[256];
    char szLog[MAX];

    buf[BUFSIZE] = 0x55;  //Debug buffer overflow

    pcp = (ConnectParams*)p;
    pcp->pid = getpid();
    pcp->connected = false;
    pcp->bytesIn = 0;
    pcp->bytesOut = 0;
    pcp->starttime = time(NULL);
    pcp->lastActive = time(NULL);

    hubConn = pcp->hub;  //Mark this as an IGATE or HUB connection

    retryTimer = 60;      //time between connection attempts in seconds

    memset(remoteIgateInfo, 0, 256);

    do {
        state = 0;
        hostinfo = NULL;
        ostringstream os_iphex;
        //Thread-Safe version of gethostbyname()
        rc = gethostbyname_r(pcp->RemoteName,
                                 &hostinfo_d,
                                 h_buf,
                                 1024,
                                 &hostinfo,
                                 &h_err);


        if (rc || (hostinfo == NULL)){
            char* cp = new char[256];
            memset(cp, 0, 256);
            ostrstream msg(cp, 255);
            msg  << "Can't resolve igate host name: "  << pcp->RemoteName << endl << ends;
            WriteLog(cp, MAINLOG);
            conQueue.write(cp, 0);
        } else
            state = 1;

        if (state == 1) {
            //cerr << pcp->RemoteName << " " << pcp->RemoteSocket << " Connecting.\n";
            clientSocket = socket(AF_INET,SOCK_STREAM, 0);
            host.sin_family = AF_INET;
            host.sin_port = htons(pcp->RemoteSocket);
            host.sin_addr = *(struct in_addr *)*hostinfo->h_addr_list;
            length = sizeof(host);

            /* Create HEX format IP address of distant server to use as alias */
            os_iphex.flags(ios::hex | ios::uppercase);
            os_iphex.width(8);
            os_iphex.fill('0');

            os_iphex <<  ntohl(host.sin_addr.s_addr);   //Build hex ip string
                     //<<  ends;

            //cerr << "HEX Ip= " << ip_hex_alias << endl;   //DEBUG

            //pcp->alias = ip_hex_alias;
            pcp->alias = os_iphex.str();

            if ((rc = connect(clientSocket,(struct sockaddr *)&host, length)) == -1) {
                close(clientSocket);
                ostringstream os;
                os << "Connection attempt failed " << pcp->RemoteName
                    << " " << pcp->RemoteSocket;

                WriteLog(os.str(), MAINLOG);

                {
                    char* cp = new char[256];
                    memset(cp, 0, 256);
                    ostrstream msg(cp, 255);
                    msg  <<  szLog << endl << ends;
                    conQueue.write(cp, 0);
                }
                gotID = false;
                state = 0;
            } else {
                state++;
                pcp->connected = true;
                pcp->starttime = time(NULL);
                pcp->bytesIn = 0;
                pcp->bytesOut = 0;

                ostringstream os;
                os << "Connected to " << pcp->RemoteName
                    << " " << pcp->RemoteSocket;

                WriteLog(os.str(), MAINLOG);

                char* cp = new char[256];
                memset(cp, 0, 256);
                ostrstream msg(cp, 255);
                msg  <<  os.str() << endl << ends;
                conQueue.write(cp,0);           //cp deleted in queue reader

            }
        }

        if (state == 2) {
            data = 1;  //Set socket for non-blocking
            ioctl(clientSocket, FIONBIO, (char*)&data, sizeof(int));

            int optval = 1;         //Enable keepalive option
            setsockopt(clientSocket, SOL_SOCKET, SO_KEEPALIVE, (char*)&optval, sizeof(int));

            /*
                If user and password are supplied we will send our Internet user and TNC
                data to the IGATE, otherwise we just read data from the IGATE.
                NEW IN 2.1.2: If only the user name is supplied without a password
                we send a password of "-1" and do not send any data.
            */

            if (pcp->user.length() != 0) {
                cout  << "IGATE Login: "
                    << pcp->RemoteName
                    << " " << pcp->user
                    << " "
                    << pcp->pass
                    << endl;

                ostringstream logon;
                logon << "user "
                    << pcp->user
                    << " pass "
                    << pcp->pass
                    << " vers "
                    << VERS
                    << "\r\n";

                //Send logon string to IGATE or Hub
                rc = send(clientSocket, logon.str().c_str(), logon.str().length(), 0);

                //Optionally send server control command if omniPort connection.
                for (int i = 0; i < pcp->nCmds; i++) {
                    ts.tv_sec = 1;
                    ts.tv_nsec = 0;
                    nanosleep(&ts,NULL);    //Sleep for 1 second
                    string s_user = pcp->user;
                    string s_cmd  = pcp->serverCmd[i];
                    if (pcp->serverCmd[i])
                        sendMsg(clientSocket, s_user, "SERVER", s_cmd);
                }

                if ((pcp->EchoMask) && (pcp->mode == MODE_RECV_SEND)) {
                    //If any bits are set in EchoMask then this add to sessions list.
                    if (sp == NULL){
                        //Grab an output session now. Note: This takes away 1 avalable user connection
                        sp = AddSession(clientSocket, pcp->EchoMask, true);  //Add this to list of sockets to send on
                    } else { // else already had an output session
                        initSessionParams(sp, clientSocket, pcp->EchoMask, true); //Restore output session for sending
                    }

                    if (sp == NULL){
                        cerr << "Can't add Server or Hub to session list .";
                        WriteLog("Failed to add Server or Hub to session list", MAINLOG);
                    } else {
                        AddSessionInfo(clientSocket, "*", "To SERVER", -1, "*");
                    }
                }
            }

            do {
                // Reset the retry timer to 60 only if previous connection lasted more than 30 secs.
                // If less than 30 secs the retrys will increase from 60 to 120 to 240 to 480 and finally 960 secs.
                if (connectTime > 30)
                    retryTimer = 60;     //Retry connection in 60 seconds if it breaks

                buf[0] = 0;
                rc = recvline(clientSocket, buf, BUFSIZE, &err, 900);  //900 sec (15 min) timeout value

                if (sp) {
                    if (sp->dead) {
                        rc = -1;  // Force disconnect if OUTgoing connection is dead
                    }
                    pcp->bytesOut = sp->bytesOut;
                }

                if (rc > 1) {  // rc: = chars recvd,   0 = timeout,  -2 = socket error
                    if (!gotID){
                        if (buf[0] == '#') {                 //First line starting with '#' should be the program name and version
                            strncpy(remoteIgateInfo, buf, 250); //This gets used in the html status web page function
                            gotID = true;
                            pcp->remoteIgateInfo = remoteIgateInfo;
                        }
                        //cerr << pcp->RemoteName << ":" << remoteIgateInfo ; //Debug
                    }

                    countLock.get();
                    TotalServerChars += rc;       //Tally up the bytes from the distant server.
                    pcp->bytesIn += rc;
                    countLock.release();

                    pcp->lastActive = time(NULL); //record time of this input

                    bool sentOnRF=false;
                    aprsString atemp(buf, clientSocket, srcIGATE, pcp->RemoteName.c_str(), "IGATE");
                    atemp.call = os_iphex.str(); //string(ip_hex_alias);   //Tag packet with alias of this hub or server

                    //One of the stations in the gate2rf list?
                    bool RFalways = find_rfcall(atemp.ax25Source, rfcall);

                    /*
                        Send it on RF if it's 3rd party msg AND TCPXX is not in the path.
                        The sendOnRF() function determines if the "to station" is local
                        and the "from station" is not.  It also reformats the packet in
                        station to station 3rd party format before sending.
                    */

                    if ((atemp.aprsType == APRSMSG)
                            && (atemp.tcpxx == false)
                            && configComplete
                            && (!RFalways)) {

                        sentOnRF = sendOnRF(atemp, pcp->RemoteName, "IGATE", srcIGATE);   // Try to send on RF

                        if (sentOnRF) {     //Now find the posit for this guy in the history list
                                            // and send it too.
                            aprsString* posit = getPosit(atemp.ax25Source,srcIGATE | srcUSERVALID);
                            if (posit != NULL) {
                                time_t Time = time(NULL);           //get current time
                                if ((Time - posit->timeRF) >= 60*10) { //posit every 10 minutes only
                                    timestamp(posit->ID, Time);  //Time stamp the original in hist. list
                                    if (posit->thirdPartyReformat(MyCall.c_str()))// Reformat it for RF delivery
                                        tncQueue.write(posit);     //posit will be deleted elsewhere
                                    else
                                        delete posit;  //Kill if conversion failed
                                }else
                                    delete posit;
                            } /*else  cout << "Can't find matching posit for "
                                               << atemp.ax25Source
                                               << endl
                                               << flush;     //Debug only
                                               */
                        }
                    }

                    /*
                        Send it on TCPIP if it's NOT a 3rd party msg
                        OR TCPXX is in path .
                    */

                    if((configComplete) && (atemp.aprsType != COMMENT)) { /* Send everything except COMMENT pkts back out to tcpip users */
                        aprsString* inetpacket = new aprsString(buf, clientSocket, srcIGATE, pcp->RemoteName.c_str(), "IGATE");
                        inetpacket->changePath("TCPIP*","TCPIP");
                        inetpacket->call = os_iphex.str(); //string(ip_hex_alias);  //tag pkt with alias of this hub or server

                        if(inetpacket->aprsType == APRSMIC_E)   //Reformat Mic-E packets
                            reformatAndSendMicE(inetpacket,sendQueue);
                        else
                            sendQueue.write(inetpacket);    // send data to users.
                                                            // Note: inetpacket is deleted in DeQueue
                    }

                    if (configComplete && (atemp.aprsType == APRSMSG)) { //find matching posit for 3rd party msg
                        aprsString* posit = getPosit(atemp.ax25Source,srcIGATE | srcUSERVALID | srcTNC);
                        if (posit != NULL) {
                            posit->EchoMask = src3RDPARTY;
                            sendQueue.write(posit);  //send matching posit only to msg port
                        }
                    }

                    //Here's where the priviledged get their posits sent to RF full time.
                    if (configComplete
                            && RFalways
                            && (StationLocal(atemp.ax25Source, srcTNC) == false)
                            && (atemp.tcpxx == false)) {
                        aprsString* RFpacket = new aprsString(buf, clientSocket, srcIGATE, pcp->RemoteName.c_str(), "IGATE");
                        RFpacket->changePath("TCPIP*","TCPIP");

                        if (RFpacket->aprsType == APRSMIC_E) {  //Reformat Mic-E packets
                            if (ConvertMicE){
                                aprsString* posit = NULL;
                                aprsString* telemetry = NULL;
                                RFpacket->mic_e_Reformat(&posit,&telemetry);
                                if (posit){
                                    posit->thirdPartyReformat(MyCall.c_str());  // Reformat it for RF delivery
                                    tncQueue.write(posit);       //Send reformatted packet on RF
                                }
                                if (telemetry) {
                                    telemetry->thirdPartyReformat(MyCall.c_str()); // Reformat it for RF delivery
                                    tncQueue.write(telemetry);      //Send packet on RF
                                }
                                delete RFpacket;
                            } else {
                                if (RFpacket->thirdPartyReformat(MyCall.c_str()))
                                    tncQueue.write(RFpacket);  // send raw MIC-E data to RF
                                else
                                    delete RFpacket;
                            }
                        } else {
                            if (RFpacket->thirdPartyReformat(MyCall.c_str()))
                                tncQueue.write(RFpacket);  // send data to RF if conversion successful
                            else
                                delete RFpacket;
                        }
                    }
                }
            } while (rc > 0);  //Loop while rc is greater than zero else disconnect

            sendLock.get();
            if(sp)
                sp->EchoMask = 0;   //Turn off the session output data stream if it's enabled

            shutdown(clientSocket,2);
            close(clientSocket);

            sendLock.release();

            pcp->connected = false;       //set status to unconnected
            connectTime = time(NULL) - pcp->starttime ;   //Save how long the connection stayed up
            pcp->starttime = time(NULL);  //reset elapsed timer
            gotID = false;                //Force new aquisition of ID string next time we connect
            pcp->alias = string("");        // reset this alias
            os_iphex.str(string(""));
            memset(szLog,0,MAX);
            ostrstream os(szLog,MAX-1);
            os << "Disconnected " << pcp->RemoteName
                << " " << pcp->RemoteSocket
                << ends;

            WriteLog(szLog, MAINLOG);

            {
                char* cp = new char[256];
                memset(cp,0,256);
                ostrstream msg(cp,255);
                msg  <<  szLog << endl << ends;
                conQueue.write(cp,0);
            }
        }

        //cerr << pcp->RemoteName << " retryTimer= " <<  retryTimer << endl;

        gotID = false;
        ts.tv_sec = retryTimer;
        ts.tv_nsec = 0;
        nanosleep(&ts,NULL);   //Sleep for retryTimer seconds
        retryTimer *= 2;               //Double retry time delay if next try is unsuccessful

        if (retryTimer >= (16 * 60))
            retryTimer = 16 * 60;     //Limit max to 16 minutes

        if (hubConn) {
            //string salias("");
            pcp->remoteIgateInfo = string("");
            pcp->alias = string("*");
            pcp = getNextHub(pcp);
            retryTimer = 60;      //Try next hub in 60 sec
        }
    } while (ShutDownServer == false);

    pthread_exit(0);
}

//----------------------------------------------------------------------

bool sendOnRF(aprsString& atemp,  string szPeer, char* userCall, const int src)
{
    bool sentOnRF = false;
    bool stsmRFalways =  find_rfcall(atemp.stsmDest, stsmDest_rfcall); //Always send on RF ?

    if ((atemp.tcpxx == false) && (atemp.aprsType == APRSMSG)) {
        if (checkUserDeny(atemp.ax25Source) != '+')
            return false; //Reject if no RF or login permitted

        //Destination station active on VHFand source not?
        if (((StationLocal(atemp.stsmDest, srcTNC) == true) || stsmRFalways)
                && (StationLocal(atemp.ax25Source, srcTNC) == false)) {
            aprsString* rfpacket = new aprsString(atemp.getChar(),atemp.sourceSock, src, szPeer.c_str(), userCall);
            //ofstream debug("rfdump.txt");
            //debug << rfpacket->getChar << endl ;  //Debug
            //debug.close();
            if (rfpacket->thirdPartyReformat(MyCall.c_str())){  // Reformat it for RF delivery
                tncQueue.write(rfpacket);  // queue read deletes rfpacket
                sentOnRF = true;
            } else
                delete rfpacket;     //Kill it if 3rd party conversion failed
        }
    }
    return sentOnRF;
}

//----------------------------------------------------------------------
int SendFiletoClient(int session, char *szName)
{
    char Line[256];
    APIRET rc = 0;
    int n,retrys;
    int throttle;
    timespec ts;
    Lock sendFileLock(pmtxSendFile, false);
    Lock sendLock(pmtxSend, false);

    ts.tv_sec = 0;
    sendFileLock.get();

    ifstream file(szName);
    if (!file) {
        cerr << "Can't open " << szName << endl;
        sendFileLock.release();
        return -1;
    }

    do {
        file.getline(Line,253); //Read each line (up to 253 bytes) in file and send to client session
        if (!file.good())
            break;

        if (strlen(Line) > 0) {
            strcat(Line,"\r\n");
            n = strlen(Line);
            sendLock.get();
            retrys = -0;
            do {
                rc = send(session,Line,n,0);
                throttle = n * 150000;
                ts.tv_nsec = throttle;
                nanosleep(&ts,NULL);  // Limit max rate to about 50kbaud

                if (rc < 0) {
                    ts.tv_nsec = 100000000;
                    nanosleep(&ts,NULL);
                    retrys++;
                } //0.1 sec between retrys
            }while((rc < 0) && (errno == EAGAIN) && (retrys <= MAXRETRYS));

            if (rc == -1) {
                perror("SendFileToClient()");
                shutdown(session,2);
                close(session); //close the socket if error happened
            }
            sendLock.release();
        }

    }while (file.good() && (rc >= 0));

    file.close();
    sendFileLock.release();

    return rc;
}

//----------------------------------------------------------------------
// Send the posits of selected users (in posit_rfcall array) to
// RF every 14.9 minutes.  Call this every second.
// Stations are defined in the aprsd.conf file with the posit2rf command.
// Posits are read from the history list.
void schedule_posit2RF(time_t t)
{

    static int ptr=0;
    static time_t last_t = 0;
    aprsString* abuff;

    if (difftime(t,last_t) < 14)
        return;  //return if not time yet (14 seconds)

    last_t = t;

    if(posit_rfcall[ptr] != NULL) {
        abuff = getPosit(*posit_rfcall[ptr] , srcIGATE | srcUSERVALID | srcUSER);
        if (abuff){
            if (abuff->thirdPartyReformat(MyCall.c_str()))  //Convert to 3rd party format
                tncQueue.write(abuff);     //Send to TNC
            else
                delete abuff;   //Kill it if conversion failed
        }
    }

    ptr++;                         //point to next call sign
    if (ptr >= (MAXRFCALL-1))
        ptr = 0;  //wrap around if past end of array
}

//---------------------------------------------------------------------
int computeShortTermLoad(int t)
{
    serverLoad_s = bytesSent_s / t;
    bytesSent_s = 0;
    return serverLoad_s;
}

//----------------------------------------------------------------------
void computeStreamRates(void)
{
    time_t time_now,sampleTime;
    Lock countLock(pmtxCount, false);

    static time_t last_time = 0;
    static ULONG last_chars = 0;
    static ULONG last_tnc_chars=0;
    static ULONG last_user_chars=0;
    static ULONG last_server_chars=0;
    static ULONG last_msg_chars=0;
    static ULONG last_UDP_chars=0;
    static ULONG last_fullstream_chars=0;

    time(&time_now);

    sampleTime = time_now - last_time;

    countLock.get();
    /*
        fullStreamRate =  (TotalTncChars + TotalServerChars
                         + TotalUserChars - last_chars) / sampleTime;
                         */
    fullStreamRate = (TotalFullStreamChars - last_fullstream_chars) / sampleTime;
    tncStreamRate = (TotalTncChars - last_tnc_chars) / sampleTime ;
    userStreamRate = (TotalUserChars - last_user_chars) /  sampleTime;
    msgStreamRate = (TotalMsgChars - last_msg_chars) / sampleTime;
    udpStreamRate = (TotalUdpChars - last_UDP_chars) / sampleTime;
    serverStreamRate = (TotalServerChars - last_server_chars) / sampleTime;
    serverLoad =  bytesSent / sampleTime;

    last_time = time_now;
    last_chars = TotalTncChars + TotalServerChars + TotalUserChars + TotalUdpChars;
    last_tnc_chars = TotalTncChars;
    last_user_chars = TotalUserChars;
    last_server_chars = TotalServerChars;
    last_msg_chars = TotalMsgChars;
    last_UDP_chars = TotalUdpChars;
    last_fullstream_chars = TotalFullStreamChars;
    bytesSent = 0;
    countLock.release();
    return;
}


//----------------------------------------------------------------------
int getStreamRate(int stream)
{
    int rv;

    switch (stream) {
        case STREAM_TNC:
            rv = tncStreamRate;
            break;
        case STREAM_USER:
            rv = userStreamRate;
            break;
        case STREAM_SERVER:
            rv = serverStreamRate;
            break;
        case STREAM_MSG:
            rv = msgStreamRate;
            break;
        case STREAM_FULL:
            rv = fullStreamRate;
            break;
        case STREAM_UDP:
            rv = udpStreamRate;
            break;
        default:
            rv = 0;
    }
    return (rv*8);  // convert to bits
}

//----------------------------------------------------------------------


string getStats()
{
    std::ostringstream os;
    //Lock countLock(pmtxCount);
    time_t time_now;
    time(&time_now);
    //upTime = (double) (time_now - serverStartTime) / 3600;

    upTime = (time_now - serverStartTime);

    //countLock.get();  
      
    computeStreamRates() ;

    os << setiosflags(ios::showpoint | ios::fixed)
        << setprecision(1)
        << "#\r\n"
        << "Server Up Time    = " << convertUpTime(upTime) << "  " << upTime << "\r\n" 
        << "Total TNC packets = " << TotalLines << "\r\n"
        << "UDP stream rate   = " << getStreamRate(STREAM_UDP) <<  " bits/sec" << "\r\n"
        << "Msg stream rate   = " << getStreamRate(STREAM_MSG) <<  " bits/sec" << "\r\n"
        << "TNC stream rate   = " << getStreamRate(STREAM_TNC) << " bits/sec" << "\r\n"
        << "User stream rate  = " << getStreamRate(STREAM_USER) << " bits/sec" << "\r\n"
        << "Hub stream rate   = " << getStreamRate(STREAM_SERVER) << " bits/sec" << "\r\n"
        << "Full stream rate  = " << getStreamRate(STREAM_FULL) << " bits/sec" << "\r\n"
        << "Msgs gated to RF  = " << msg_cnt << "\r\n"
        << "Connect count     = " << TotalConnects << "\r\n"
        << "Users             = " << getConnectedClients() << "\r\n"
        << "Peak Users        = " << MaxConnects << "\r\n"
        << "Server load       = " << serverLoad << " bits/sec" << "\r\n"
        << "History Items     = " << ItemCount << "\r\n"
        << "aprsString Objs   = " << aprsString::getObjCount() << "\r\n"
        << "Items in InetQ    = " << sendQueue.getItemsQueued() << "\r\n"
        << "InetQ overflows   = " << sendQueue.overrun << "\r\n"
        << "TncQ overflows    = " << tncQueue.overrun << "\r\n"
        << "conQ overflows    = " << conQueue.overrun << "\r\n"
        << "charQ overflow    = " << charQueue.overrun << "\r\n"
        << "Hist. dump aborts = " << dumpAborts << "\r\n";

    //countLock.release();
    return os.str();
}


//----------------------------------------------------------------------
//Here we check server load against MaxLoad and drop the last user
//to logon if load is excessive.  This executes once a minute.
void enforceMaxLoad(void)
{
    int i = 0, k = 0 ;
    time_t t;
    Lock addDelSessLock(pmtxAddDelSess, false);

    if(serverLoad <= MaxLoad)
        return;

    t = 0;

    addDelSessLock.get();
    
    for (i = 0; i < MaxClients; i++) {
        if ((sessions[i].starttime > t)
                && (sessions[i].Socket != -1)
                && (sessions[i].EchoMask & srcIGATE)
                && (sessions[i].svrcon == false)){  //Check all established connections
                                                 //receiving the full stream first.
            t = sessions[i].starttime;     //Find most receint start time
            k = i;
        }
    }

    if (t > 0) {
        sessions[k].dead = true;   //Kill off high bandwidth users first.
    } else {
        t = 0;
        for (i = 0; i < MaxClients; i++) {
            if ((sessions[i].starttime > t)
                    && (sessions[i].Socket != -1)
                    && (!(sessions[i].EchoMask & srcIGATE))
                    && (sessions[i].svrcon == false)){  //Check all established connections
                                                // not receiving the full stream next.
                t = sessions[i].starttime;     //Find most receint start time
                k = i;
            }
        }
        sessions[k].dead = true;   //Kill off low bandwidth user now.
    }

    addDelSessLock.release();
}


//----------------------------------------------------------------------
void resetCounters()
{
    dumpAborts = 0;
    sendQueue.overrun = 0 ;
    tncQueue.overrun = 0 ;
    conQueue.overrun = 0 ;
    charQueue.overrun = 0;
    TotalLines = 0;
    msg_cnt = 0;

    MaxConnects = getConnectedClients();
    TotalConnects = MaxConnects;
    countREJECTED = 0;
    countLOOP = 0;
    countOVERSIZE = 0;
    countNOGATE = 0;
    countIGATED = 0;
}



//----------------------------------------------------------------------

inline string convertUpTime(int dTime)
{
    std::ostringstream ostr;
    int x;
    int y;

    if (dTime > 86400) {                                // greater than a day
        x = (dTime / 86400);
        y = (dTime % 86400)/3600;
        ostr << x << ((x == 1) ? " Day " : " Days ") << y << ((y <= 1) ? " hour" : " hours");
    } else if ((dTime >= 3600) && (dTime <= 86400)) {   // 1 hr to 1 day
        x = (dTime / 3600);
        y = (dTime % 3600)/60;
        ostr << x << ((x == 1) ? " hour " : " hours ") << y << ((y <= 1) ? " minute" : " minutes");
    } else {                                            // hmm, must've just started...
        x = (dTime / 60);
        ostr << x << ((x <= 1) ? " minute" : " minutes");
    }

    return ostr.str();
}

