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



/*

FUNCTIONS PERFORMED:

This program gets data from a TNC connected to a serial port
and sends it to all clients who have connected via tcpip to
ports defined in the /home/aprsd2/aprsd.conf file.

Clients can use telnet to watch raw TNC data or
other APRS specfic clients such as JavAPRS, Mac/WinAPRS
or Xastir to view the data plotted on a map.

A history of TNC data going back 30 minutes is kept in
memory and delivered to each user when he connects.  This
history data is filtered to remove duplicates and certain
other unwanted information.  The data is saved to history.txt
every 10 minutes and on exit with ctrl-C or "q" .



REMOTE CONTROL of the TNC
The server system operator can access the TNC remotely via
telnet.  Simply telnet to the server and, after the server
finishes sending the history file, hit the <Esc> key .
The user will be prompted for a user name and password.  If these
are valid and the user is a member of the "tnc" group the user
will be able to type commands to the TNC.	Type <Esc> to quit
remote mode or ctrl-d to disconnect.  Be sure to give the
TNC a "K" command to put it back on line before you exit.

To logon in remote TNC control mode you must have a
tnc group defined in /etc/group and your Linux login
name must appear in the entry.  Here is an example.

tnc::102:root,wa4dsy,bozo

Note: Once you have entered remote control mode the TNC
is off-line and no TNC data will go out to users.

See the aprsddoc.html file for more instructions.




--------------------------------------------------------
   Here's some off the air data for reference
//
//$GPRMC,013426,A,3405.3127,N,08422.4680,W,1.7,322.5,280796,003.5,W*73

KD4GVX>APRS,WB4VNT-2,N4NEQ-2*,WIDE:@071425/Steve in Athens, Ga.
N4NEQ-9>APRS,RELAY*,WIDE:/211121z3354.00N/08418.04W/GGA/NUL/APRS/good GGA FIX/A=
000708
N4NEQ-9>APRS,RELAY,WIDE*:/211121z3354.00N/08418.04W/GGA/NUL/APRS/good GGA FIX/A=
000708
KE4FNU>APRS,KD4DLT-7,N4NEQ-2*,N4KMJ-7,WIDE:@311659/South Atlanta via v7.5a@Stock
bridge@DavesWorld
KC4ELV-2>APRS,KD4DLT-7,N4NEQ-2*,WIDE:@262026/APRS 7.4 on line.
KC4ELV-2>APRS,KD4DLT-7,N4NEQ-2,WIDE*:@262026/APRS 7.4 on line.
N4QEA>APRS,SEV,WIDE,WIDE*:@251654/John in Knoxville, TN.  U-2000
WD4JEM>APRS,N4NEQ-2,WIDE*:@170830/APRS 7.4 on line.
KD4DLT>APRS,KD4DLT-7,N4NEQ-2*,WIDE:@201632/APRS 7.6f on line.
N4NEQ-3>BEACON,WIDE,WIDE:!3515.46N/08347.70W#PHG4970/WIDE-RELAY Up High!!!
N4NEQ-3>BEACON,WIDE*,WIDE:!3515.46N/08347.70W#PHG4970/WIDE-RELAY Up High!!!
KD4DKW>APRS:@151615/APRS 7.6f on line.
KE4KQB>APRS,KD4DLT-7,WIDE*:@111950/APRS 7.6 on line.
WB4VNT>APRS,WB4VNT-2,N4NEQ-2*,WIDE:@272238/7.5a on line UGA rptr 147.000+
N4YTR>APRS,AB4KN-2*,WIDE:@111443zEd - ARES DEC National Weather Service, GA
N4YTR>APRS,AB4KN-2,WIDE*:@111443zEd - ARES DEC National Weather Service, GA
W6PNC>APRS,N4NEQ-2,WIDE:@272145/3358.60N/08417.84WyJohn in Dunwoody, GA
WA4DSY-9>APRS,WIDE:$GPRMC,014441,A,3405.3251,N,08422.5074,W,0.0,359.3,280796,003
.5,W*77
N6OAA>APRS,GATE,WIDE*:@280144z4425.56N/08513.11W/ "Mitch", Lake City, MI



-------------------------------------------------------------------------------------

*/

#define DEBUG

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

extern "C" {
#include <termios.h>
#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>
#include <signal.h>                     // signal
#include <fstream.h>
#include <iostream.h>
#include <strstream.h>
#include <iomanip.h>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>                      // gethostbyname2_r
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <errno.h>

#include <sys/time.h>

#include <stdio.h>
#include <fcntl.h>                      // umask
}

#include <string>


#include "dupCheck.h"
#include "cpqueue.h"
#include "utils.h"
#include "constant.h"
#include "history.h"
#include "rf.h"
#include "aprsString.h"
#include "validate.h"
#include "queryResp.h"
#include "except.hpp"

using namespace std;

//--------------------------------------------------
#define BASE 0
#define USER 1
#define PASS 2
#define REMOTE 3
#define MAX 256
#define UMSG_SIZE MAX+BUFSIZE+3


//---------------------------------------------------
extern int dumpAborts;                  // Number of history dumps aborted
extern int ItemCount;	                // number of items in History list
cpQueue sendQueue(1024, true);          // Internet transmit queue
cpQueue tncQueue(64, true);             // TNC RF transmit queue
cpQueue charQueue(1024, false);         // A queue of single characters
cpQueue conQueue(256, true);            // data going to the console from various threads

string MyCall;
string MyLocation;
string MyEmail;
string NetBeacon;
string TncBeacon;
int TncBeaconInterval, NetBeaconInterval;
long tncPktSpacing;
bool igateMyCall;                       // Set TRUE if server will gate packets to inet with "MyCall"
                                        // in the ax25 source field.
bool logAllRF;
bool ConvertMicE;                       // Set true causes Mic-E pkts to be converted to classic APRS pkts.
extern int ttlDefault;
extern bool TncSysopMode;
bool APRS_PASS_ALLOW;

int ackRepeats,ackRepeatTime;           // Used by the ACK repeat thread

bool sendOnRF(TAprsString& atemp,  const char* szPeer, const char* userCall, const int src);
int WriteLog(const char *cp, char *LogFile);
void *HTTPServerThread(void* p);


//-------------------------------------------------------------------
int SendFiletoClient(int session, char *szName);
void PrintStats(ostream &os) ;
void dequeueTNC(void);

void (*p_func)(void*);

char *szAprsPath;                       // APRS packet path
string szServerCall;                    // This servers "AX25 Source Call" (user defined)
char *szAPRSDPATH;                      // Servers "ax25 dest address + path" eg: >APD213,TCPIP*:

const char szServerID[] = "APRS Server: ";
const char szJAVAMSG[] = ">JAVA::javaMSG  :";

const char szUSERLISTMSG[] = ":USERLIST :";
const char szUNVERIFIED[] = "Unverified ";
const char szVERIFIED[] = "Verified ";
const char szRM1[] = "You are an unverified user, Internet access only.\r\n";
const char szRM2[] = "Send Access Denied. Access is Read-Only.\r\n";
const char szRM3[] = "RF access denied, Internet access only.\r\n";
const char szACCESSDENIED[] = "Access Denied ";

pthread_mutex_t *pmtxSendFile;
pthread_mutex_t *pmtxSend;              // protect socket send()
pthread_mutex_t *pmtxAddDelSess;        // protects AddSession() and DeleteSession()
pthread_mutex_t *pmtxCount;             // Protect counters
pthread_mutex_t *pmtxDNS;               // Protect buggy gethostbyname2_r()

bool ShutDownServer, configComplete;
int ConnectedClients;
int msg_cnt;
int posit_cnt;
ULONG error_cnt, frame_cnt;
ULONG WatchDog, tickcount, TotalConnects, TotalTncChars, TotalLines;
int  MaxConnects;
ULONG TotalIgateChars, TotalUserChars, bytesSent, webCounter;
time_t serverStartTime;
ULONG TotalTNCtxChars;
int msgsn;
char *szComPort;
extern int queryCounter;

bool respondToIgateQueries;
bool respondToAprsdQueries;
bool broadcastJavaInfo;
bool logBadPackets;

//----------------------------

struct ConnectParams {
    int RemoteSocket;
    char *RemoteName;
    int EchoMask;
    char *user;
    char *pass;
    char *remoteIgateInfo;
    long bytesIn;
    long bytesOut;
    time_t starttime;
    time_t lastActive;
    bool  connected;
    bool hub;
    pthread_t tid;
    pid_t pid;
};

struct ServerParams {
    int ServerPort;
    int ServerSocket;
    int EchoMask;
    pthread_t tid;
    pid_t pid;
};

#define SZPEERSIZE 16
#define USERCALLSIZE 10
#define PGMVERSSIZE 24

struct SessionParams {
    int Socket;
    int EchoMask;
    int ServerPort;
    int overruns;
    pid_t pid;
    time_t starttime;
    time_t lastActive;
    long bytesIn;
    long bytesOut;
    bool vrfy;
    bool dead;
    char *szPeer;
    char *userCall;
    char *pgmVers;
};

struct UdpParams {
    int ServerPort;
    pthread_t tid;
    pid_t pid;
};

struct pidList {
    pid_t main;
    pid_t SerialInp;
    pid_t TncQueue;
    pid_t InetQueue;
};


//  Constants for EchoMask.  Each aprsSting object has this variable.
//  These allow each Internet listen port to filter
//  data it needs to send.
extern const int srcTNC = 1;            // data from TNC
extern const int srcUSER = 2;           // data from any logged on internet user
extern const int srcUSERVALID = 4;      // data from validated internet user
extern const int srcIGATE = 8;          // data from another IGATE
extern const int srcSYSTEM = 16;        // data from this server program
extern const int srcUDP = 32;           // data from UDP port
extern const int srcHISTORY = 64;       // data fetched from history list
extern const int src3RDPARTY = 128;     // data is a 3rd party station to station message
extern const int srcSTATS = 0x100;      // data is server statistics

extern const int wantSRCHEADER = 0x0800;// User wants Source info header prepended to data
extern const int wantHTML = 0x1000;     // User wants html server statistics (added in version 2.1.2)
extern const int wantRAW = 0x2000;      // User wants raw data only
extern const int sendDUPS = 0x4000;     // if set then don't filter duplicates
extern const int sendHISTORY = 0x8000;  // if set then history list is sent on connect

//------------------------------

SessionParams *sessions;                // points to array of active socket descriptors

int MaxClients;                         // defines how many can log on at once

ULONG ulNScnt = 0;
bool tncPresent;                        // TRUE if TNC com port has been specified
bool tncMute;                           // TRUE stops messages from going to the TNC

//int  nObjects = 0;                      // number of TAprsString objects that exist now.

ServerParams spMainServer, spMainServer_NH, spLinkServer, spLocalServer, spMsgServer, spHTTPServer;
ServerParams spRawTNCServer, spIPWatchServer;
UdpParams upUdpServer;

dupCheck dupFilter;                     // Create a dupCheck class named dupFilter.
                                        // This identifies duplicate packets.

extern const int maxIGATES = 100;       // Defines max number of IGATES you can connect to
int nIGATES = 0;                        // Actual number of IGATES you have defined

//------------------------------------------------------------------------------------------
//  Array to hold list of stations
//  allowed to full time gate from
//  Internet to RF
#define MAXRFCALL 65                    // allow 64 of 'em including a NULL for last entry.
string *rfcall[MAXRFCALL];              // list of stations gated to RF full time (all packets)
int rfcall_idx;

string *posit_rfcall[MAXRFCALL];        // Stations whose posits are always gated to RF
int posit_rfcall_idx;

string *stsmDest_rfcall[MAXRFCALL];     // Station to station messages with these
                                        // DESTINATION call signs are always gated to RF
int stsmDest_rfcall_idx;

int aprsStreamRate, serverLoad;         // Server statistics
int tncStreamRate;
double upTime;

bool RF_ALLOW = false;                  // TRUE to allow Internet to RF message passing.

//--------------------------------------------------------------------------------------------
ConnectParams cpIGATE[maxIGATES];

//Stuff for trusted UDP source IPs
struct sTrusted {
    in_addr sin_addr;                   // ip address
    in_addr sin_mask;                   // subnet mask
};

const int maxTRUSTED = 20;              // Max number of trusted UDP users
sTrusted Trusted[maxTRUSTED];           // Array to store their IP addresses
int nTrusted = 0;                       // Number of trusted UDP users defined

//Debug stuff
pidList pidlist;
string DBstring;                        // for debugging
TAprsString* lastPacket;                 // for debugging
FILE* fdump;

//-------------------------------------------------------------------

void initSessionParams(SessionParams* sp, int s, int echo)
{
    sp->Socket = s;
    sp->EchoMask = echo;

    sp->overruns = 0;
    sp->bytesIn = 0;
    sp->bytesOut = 0;
    sp->vrfy = false;
    sp->dead = false;
    sp->starttime = time(NULL);
    sp->lastActive = sp->starttime;
    memset((void*)sp->szPeer, NULLCHR, SZPEERSIZE);
    memset((void*)sp->userCall, NULLCHR, USERCALLSIZE);
}


//---------------------------------------------------------------------
//  Add a new user to the list of active sessions
//  Includes outbound Igate connections too!
//  Returns NULL if it can't find an available session
SessionParams* AddSession(int s, int echo)
{
    SessionParams *rv = NULL;
    int i;

    try {
        if(pthread_mutex_lock(pmtxAddDelSess) != 0)
            cerr << "Unable to lock pmtxAddDelSess - SessionParams.\n" << flush;

        if(pthread_mutex_lock(pmtxSend) != 0)
            cerr << "Unable to lock pmtxSend - SessionParams.\n" << flush;

        DBstring = "SessionParams - top of loop to find available session";
        for (i = 0; i < MaxClients; i++)
            if (sessions[i].Socket == -1)
                break;                      // Find available unused session

        if (i < MaxClients) {
            rv = &sessions[i];
            initSessionParams(rv, s, echo);
            if(pthread_mutex_lock(pmtxCount) != 0)
                cerr << "Unable to lock pmtxCount - SessionParams.\n" << flush;

            ConnectedClients++;
            if(pthread_mutex_unlock(pmtxCount) != 0)
                cerr << "Unable to unlock pmtxCount - SessionParams.\n" << flush;
        } else
            rv = NULL;
            DBstring = "SessionParams - completed add";
            if(pthread_mutex_unlock(pmtxSend) != 0)
                cerr << "Unable to unlock pmtxSend - SessionParams.\n" << flush;

            if(pthread_mutex_unlock(pmtxAddDelSess) != 0)
                cerr << "Unable to unlock pmtxAddDelSess - SessionParams.\n" << flush;

            return(rv);
    }
    catch (TAprsdException except) {
        cout << except.what() << endl;
        WriteLog(except.what(), ERRORLOG);
        return(rv);
    }
}


//--------------------------------------------------------------------
//  Remove a user
bool DeleteSession(int s)
{
    int i = 0;
    bool rv = false;

    if (s == -1)
        return false;
    try {
        DBstring = "DeleteSession - top of loop";
        if(pthread_mutex_lock(pmtxAddDelSess) != 0)
            cerr << "Unable to lock pmtxAddDelSess - DeleteSession.\n" << flush;

        for (i=0; i<MaxClients; i++) {
            if (sessions[i].Socket == s ) {
                sessions[i].Socket = -1;
                sessions[i].EchoMask = 0;
                sessions[i].pid = 0;
                sessions[i].dead = true;
                if(pthread_mutex_lock(pmtxCount) != 0)
                    cerr << "Unable to lock pmtxCount - DeleteSession.\n" << flush;

                ConnectedClients--;
                if ( ConnectedClients < 0)
                    ConnectedClients = 0;

                if(pthread_mutex_unlock(pmtxCount) != 0)
                    cerr << "Unable to unlock pmtxCount - DeleteSession.\n" << flush;

                rv = true;
            }
        }
        DBstring = "DeleteSession - out of loop";
        if(pthread_mutex_unlock(pmtxAddDelSess) != 0)
            cerr << "Unable to unlock pmtxAddDelSess - DeleteSession.\n" << flush;

        return(rv);
    }
    catch (TAprsdException except) {
        cout << except.what() << endl;
        WriteLog(except.what(), ERRORLOG);
        return(false);
    }
}

//-------------------------------------------------
bool AddSessionInfo(int s, const char* userCall, const char* szPeer, int port, const char* pgmVers)
{
    int i = 0;
    bool rv = false;

    try {
        if(pthread_mutex_lock(pmtxAddDelSess) != 0)
            cerr << "Unable to lock pmtxAddDelSess - AddSessionInfo.\n" << flush;

        DBstring = "AddSessionInfo - top of loop";
        for (i = 0; i<MaxClients; i++) {
            if (sessions[i].Socket == s ) {
                strncpy(sessions[i].szPeer, szPeer, SZPEERSIZE-1);
                strncpy(sessions[i].userCall, userCall, USERCALLSIZE-1);
                sessions[i].ServerPort = port;
                strncpy(sessions[i].pgmVers, pgmVers, PGMVERSSIZE-1);
                sessions[i].pid = getpid();
                rv = true;
            }
        }
        DBstring = "AddSession - out of loop";

        if(pthread_mutex_unlock(pmtxAddDelSess) != 0)
            cerr << "Unable to unlock pmtxAddDelSess - AddSessionInfo.\n" << flush;
        return(rv);
    }
    catch (TAprsdException except) {
        cout << except.what() << endl;
        WriteLog(except.what(), ERRORLOG);
        return(false);
    }
}

//---------------------------------------------------------------------
void CloseAllSessions(void)
{
    for (int i=0;i<MaxClients;i++) {
        if (sessions[i].Socket != -1 ) {
            shutdown(sessions[i].Socket,2);
            close(sessions[i].Socket);
            sessions[i].Socket = -1;
            sessions[i].EchoMask = 0;
        }
    }
}


//---------------------------------------------------------------------
//  Send data at "p" to all logged on clients listed in the sessions array.


void SendToAllClients(TAprsString* p)
{
    int ccount = 0;
    int rc, n, nraw, nsh;

    if (p == NULL)
        return;
    DBstring = "Top of Error Check"; 
    if ((p->aprsType == APRSERROR) || (p->length() < 3)  ) {
        if (logBadPackets) {
            if (!((p->find("Sent") <= p->length())
                    || (p->find("has connected") <= p->length())
                    || (p->find("Connection attempt failed") <= p->length())
                    || (p->find("has disconnected") <= p->length())
                    || (p->find("Connected to") <= p->length()))) {
                if (p->aprsType == APRSERROR) {
                    char *fubarmsg;
                    fubarmsg = new char[2049];
                    memset(fubarmsg, 0, 2049);
                    ostrstream msg(fubarmsg, 2048);

                    msg << "FUBARPKT " << p->srcHeader.c_str()
                        << " " << p->c_str()
                        << endl
                        << ends;

                    DBstring = "Write bad packet log";

                    WriteLog(fubarmsg, FUBARLOG);
                    delete[] fubarmsg;
                }
            }
        }
        return;                         // Reject runts and error pkts
    }
    try {
        DBstring = "Lock mutexes before send";
        if(pthread_mutex_lock(pmtxAddDelSess) != 0)
            cerr << "Unable to lock pmtxAddDelSess - SendToAllClients.\n" << flush;

        if(pthread_mutex_lock(pmtxSend) != 0)
            cerr << "Unable to lock pmtxSend - SendToAllClients.\n" << flush;

        DBstring = "Mutexes locked for send";
        n = p->length();
        nraw = p->raw.length();
        nsh = p->srcHeader.length();
        DBstring = "SendToAllClients: p length check";
        for (int i = 0; i < MaxClients; i++) {
            DBstring = "SendToAllClients: top of for loop";
            if (sessions[i].Socket != -1) {
                bool dup, wantdups, wantsrcheader;
                dup = wantdups = wantsrcheader = false ;

                if (p->EchoMask & sendDUPS)
                    dup = true;             // This packet is a duplicate

                if (sessions[i].EchoMask & sendDUPS)
                    wantdups = true;        // User wants duplicates

                if (sessions[i].EchoMask & wantSRCHEADER)
                    wantsrcheader = true;   // User wants IP source header

                int Em = p->EchoMask & 0x1ff;   // Mask off non-source determining bits

                if ((sessions[i].EchoMask & Em) // Echo inet data if source mask bits match
                        && (p->sourceSock != sessions[i].Socket) // no echo to original sender
                        && (ShutDownServer == false)
                        && ((dup == false) || (wantdups))) { //dups filtered (or not)

                    rc = 0;
                    if (sessions[i].EchoMask & wantRAW) {  //User wants raw data?
                        rc = send(sessions[i].Socket,p->raw.c_str(),nraw,0); //Raw data to clients
                    } else {
                        if ((p->reformatted == false)  // No 3rd party reformatted packets allowed
                                && (wantsrcheader == false) // This guy doesn't want the IP source header prepended
                                && (dup == false)) {        // No duplicates
                            DBstring = "SendToAllClient: at send";
                            rc = send(sessions[i].Socket,p->c_str(),n,0); // Cooked data to clients (normal mode)
                        }

                        if (wantsrcheader) {// Append source header to aprs packets, duplicates ok.
                                            // Mostly for debugging the network
                            rc = send(sessions[i].Socket,p->srcHeader.c_str(),nsh,0);
                            if (rc != -1)
                                rc = send(sessions[i].Socket,p->c_str(),n,0);

                        }
                    }

                    // Disconnect user if socket error or he failed
                    // to accept 10 consecutive packets due to
                    // resource temporarally unavailable errors
                    if (rc == -1) {
                        if (errno == EAGAIN) {
                            sessions[i].overruns++;
                            cerr << "Session overrun (" << sessions[i].userCall << ")" << ends << endl;
                        }
                        if ((errno != EAGAIN) || (sessions[i].overruns >= 10)) {
                            sessions[i].EchoMask = 0;   // No more data for you!
                            sessions[i].dead = true;    // Mark connection as dead for later removal...
                                                        // ...by thread that owns it.
                        }
                    } else {
                        if(pthread_mutex_lock(pmtxCount) != 0)
                            cerr << "Unable to lock pmtxCount - SessionOverrun.\n" << flush;

                        sessions[i].overruns = 0;       // Clear users overrun counter if he accepted packet
                        sessions[i].bytesOut += rc;     // Add these bytes to his bytesOut total
                        if(pthread_mutex_unlock(pmtxCount) != 0)
                            cerr << "Unable to unlock pmtxCount - SessionOverrun.\n" << flush;
                    }
                    if(pthread_mutex_lock(pmtxCount) != 0)
                        cerr << "Unable to lock pmtxCount - SessionOverrun2.\n" << flush;

                    ccount++;
                    if(pthread_mutex_unlock(pmtxCount) != 0)
                        cerr << "Unable to unlock pmtxCount - SessionOverrun2.\n" << flush;
                }
            }
        }
        DBstring = "SendToAllClients: out of loop";
        if(pthread_mutex_unlock(pmtxSend) != 0)
            cerr << "Unable to unlock pmtxSend - SendToAllClients.\n" << flush;

        if(pthread_mutex_unlock(pmtxAddDelSess) != 0)
            cerr << "Unable to unlock pmtxAddDelSess - SendToAllClients.\n" << flush;

        DBstring = "Unlock AddDelSess and Send Mutexes";

        /*if ((ccount > 0) && ((p->EchoMask & srcSTATS) == 0)) {
            char *cp = new char[257];
            memset(cp,0,257);
            ostrstream msg(cp,256);

            //msg << "Sent " << setw(4) << n << " bytes to " << setw(3) << ccount << " clients"
            //    << endl
            //    << ends ;

            if (cp != NULL)
                conQueue.write(cp,0);           // cp deleted by queue reader
            else
                DBstring = "SendToAllClients: cp is NULL!";
        }*/

        DBstring = "Lock pmtxCount for bytesSent increment";
        if(pthread_mutex_lock(pmtxCount) != 0)
            cerr << "Unable to lock pmtxCount - SendToAlClients.\n" << flush;

        bytesSent += (n * ccount);
        if (pthread_mutex_unlock(pmtxCount) != 0)
            cerr << "Unable to unlock pmtxCount - SendToAllClients.\n" << flush;

        DBstring = "Unlock pmtxCount after bytesSent increment";

        /*
            gettimeofday(&tv,&tz);  //Get time of day in microseconds to tv.tv_usec
            t1 = tv.tv_usec + (tv.tv_sec * 1000000);
            cout << "t= " << t1-t0 << endl;
        */
        return;
    }

    catch (TAprsdException except) {
        cout << except.what() << endl;
        WriteLog(except.what(), ERRORLOG);
        return;
    }
}


//---------------------------------------------------------------------
// Pushes a character string into the server send queue.
void BroadcastString(char *cp)
{
    DBstring = "BroadcastString - function entrance";
    TAprsString *msgbuf = new TAprsString(cp,SRC_INTERNAL,srcSYSTEM);
    sendQueue.write(msgbuf);            // DeQueue() deletes the *msgbuf
    DBstring = "BroadcastString - function exit";
    return ;
}

//---------------------------------------------------------------------
// This is a thread.  It removes items from the send queue and transmits
// them to all the clients.  Also handles checking for items in the TNC queue
// and calling the tnc dequeue routine.
void *DeQueue(void *)
{
    bool noHist,dup;
    int cmd;
    TAprsString* abuff;
    int MaxAge,MaxCount;
    struct timeval tv;
    struct timezone tz;
    long usLastTime, usNow;
    long t0;
    //const char *dcp;
    //string(dcp);

    usLastTime = usNow = 0;

    pidlist.InetQueue = getpid();

    //nice(-10);                        // Increase priority of this thread by
                                        // 10 (only works if run as root)
                                        // n5VFF - let's try it without this..

    while (!ShutDownServer) {
        DBstring = "Top of DeQueue thread main loop";
        gettimeofday(&tv,&tz);          // Get time of day in microseconds to tv.tv_usec
        usNow = tv.tv_usec + (tv.tv_sec * 1000000);

        if (usNow < usLastTime)
            usLastTime = usNow;

        t0 = usNow;

        if (tncPresent) {
            if ((usNow - usLastTime) >= tncPktSpacing) {  // Once every 1.5 second or user defined
                usLastTime = usNow;
                if (tncQueue.ready())
                    dequeueTNC();           // Check the TNC queue
            }
        }

        while (sendQueue.ready() == false) {    // Loop here till somethings in the send queue
            DBstring = "Top of !sendQueue.ready while loop";
            gettimeofday(&tv,&tz);      // Get time of day in microseconds to tv.tv_usec
            usNow = tv.tv_usec + (tv.tv_sec * 1000000);
            if (usNow < usLastTime)
                usLastTime = usNow;

            t0 = usNow;

            if (tncPresent) {           // only run this if a TNC is actually connected
                if ((usNow - usLastTime) >= tncPktSpacing) {  //Once every 1.5 second or user defined
                    usLastTime = usNow;
                    if (tncQueue.ready())
                        dequeueTNC();       // Check the TNC queue
                }
            }
            reliable_usleep(1000);      // 1ms  ZPO - try .1ms

            if (ShutDownServer)
                pthread_exit(0);

            DBstring = "Out of 1ms sleep - bottom of !sendQueue.ready while loop";

        }
        DBstring = "Get TAprsString off sendQueue - before";
        abuff = (TAprsString*)sendQueue.read(&cmd);  // Read an TAprsString pointer from the queue to abuff
        DBstring = "Get TAprsString off sendQueue - after";

        //lastPacket = abuff;             // debug thing
        //dcp = " ";                    // another one
        DBstring = "Check abuff != NULL";

        if (abuff != NULL) {
            DBstring = "Set abuff->dest";
            abuff->dest = destINET;
            DBstring = "Entrance to dup check";
            dup = false;
            if (!((abuff->EchoMask) & (srcSTATS | srcSYSTEM)))
                dup  = dupFilter.check(abuff,15);   // Check for duplicates within 15 second window

            if (((abuff->EchoMask & src3RDPARTY)&&((abuff->aprsType == APRSPOS)) // No Posits fetched from history list
                    || (abuff->aprsType == COMMENT)               // No comment packets in the history buffer
                    || (abuff->aprsType == APRSERROR)             // No packets that crashed the parser
                    || (abuff->aprsType == APRSUNKNOWN)           // No Unknown packets
                    || (abuff->aprsType == APRSID)                // No ID packets
                    || (abuff->EchoMask & (srcSTATS | srcSYSTEM)) // No internally generated packets
                    || (dup)                                      // No duplicates
                    || (abuff->reformatted))) {                   // No 3rd party reformatted pkts
                noHist = true;    //None of the above allowed in history list
            } else {
                DBstring = "Execute GetMaxAgeAndCount - these are constants???";
                GetMaxAgeAndCount(&MaxAge,&MaxCount);   // Set max ttl and count values
                abuff->ttl = MaxAge;
                DBstring = "Add item to history list - before";

                if(pthread_mutex_lock(pmtxCount) != 0)
                    cerr << "Unable to lock pmtxCount - DeQueue-AddHistoryItem.\n" << flush;

                AddHistoryItem(abuff);  // Put item in history list.
                if(pthread_mutex_unlock(pmtxCount) != 0)
                    cerr << "Unable to unlock pmtxCount - DeQueue-AddHistoryItem.\n" << flush;

                DBstring = "Add item to history list - after";

                noHist = false;
            }

            //if(abuff->EchoMask & sendDUPS) printf("EchoMask sendDUPS bit is set\n");

            if (dup)
                abuff->EchoMask |= sendDUPS;    // If it's a duplicate mark it.

            DBstring = "Execute SendToAllClients";
            SendToAllClients(abuff);    // Send item out on internet
            DBstring = "Back in main DeQueue flow";
            if (noHist) {
                delete abuff;           // delete it now if it didn't go to the history list.
                abuff = NULL;
            }
        } else
            cerr << "Error in DeQueue: abuff is NULL" << endl << flush;
    }
    return NULL;                       // Should never get here
}

//----------------------------------------------------------------------
//  This thread is started by code in  dequeueTNC when an ACK packet
//  is detected being sent to the TNC.  A pointer to the ack packet is passed
//  to this thread.  This thread puts additional identical ack packets into the
//  TNC queue.  The allowdup attribute is set so the dup detector won't kill 'em.
//
void *ACKrepeaterThread(void *p)
{
    TAprsString *paprs;
    paprs = (TAprsString*)p;
    TAprsString *abuff = new TAprsString(*paprs);
    abuff->allowdup = true;             // Bypass the dup filter!
    paprs->ttl = 0;                     // Flag tells caller we're done with it.

    for (int i=0 ;i<ackRepeats;i++) {
        sleep(ackRepeatTime);
        TAprsString *ack =  new TAprsString(*abuff);

        tncQueue.write(ack);            // ack TAprsString  will be deleted by TNC queue reader
    }
    delete abuff;
    pthread_exit(0);
}

//----------------------------------------------------------------------
// Pulls TAprsString object pointers off the tncQueue
// and sends the data to the TNC.
//
void dequeueTNC(void)
{
    bool dup;
    char *rfbuf = NULL;
    TAprsString* abuff = NULL;
    char szUserMsg[UMSG_SIZE];

    pthread_t tid;

    abuff = (TAprsString*)tncQueue.read(NULL);   // Get pointer to a packet off the queue

    if (abuff == NULL)
        return;

    if ((RF_ALLOW == false)             // See if sysop allows Internet to RF traffic
            && ((abuff->EchoMask & srcUDP) == false)){  // UDP packets excepted
        delete abuff;                   // No RF permitted, delete it and return.
        return;
    }

    //abuff->print(cout); //debug

    abuff->ttl = 0;
    abuff->dest = destTNC;
    dup = dupFilter.check(abuff,15);    // Check for duplicates during past 15 seconds

    if (dup) {
        delete abuff;                   // kill the duplicate here
        return;
    }

    rfbuf = new char[300];
    memset(rfbuf, 0, 300);

    if (rfbuf != NULL) {
        if (tncPresent ) {
            if ((abuff->allowdup == false)              // Prevent infinite loop!
                    && (abuff->msgType == APRSMSGACK)   // Only ack packets
                    && (ackRepeats > 0) ) {             // Only if repeats greater than zero
                abuff->ttl = 1;                         // Mark it as unprocessed (ttl serves double duty here)
                int rc = pthread_create(&tid, NULL, ACKrepeaterThread, abuff);    // Create ack repeater thread
                if (rc != 0) {          // Make sure it starts
                    cerr << "Error: ACKrepeaterThread failed to start\n";
                    abuff->ttl = 0;
                } else
                    pthread_detach(tid);                // Run detached to free resources.
            }

            strncpy(rfbuf,abuff->data.c_str(),256); // copy only data portion to rf buffer
                                                    // and truncate to 256 bytes
            RemoveCtlCodes(rfbuf);      // remove control codes and set 8th bit to zero.
            rfbuf[256] = NULLCHR;          // Make sure there's a null on the end
            strcat(rfbuf,"\r");         // append a CR to the end
            char* cp = new char[300];   // Will be deleted by conQueue reader.
            memset(cp, 0, 300);
            ostrstream msg(cp,299);

            msg << "Sending to TNC: " << rfbuf << endl << ends; //debug only
            conQueue.write(cp, 0);

            if(pthread_mutex_lock(pmtxCount) != 0)
                cerr << "Unable to lock pmtxCount - SendToTNC.\n" << flush;

            TotalTNCtxChars += strlen(rfbuf);
            if(pthread_mutex_unlock(pmtxCount) != 0)
                cerr << "Unable to unlock pmtxCount - SendToTNC.\n" << flush;

            if (!tncMute) {
                if(abuff->reformatted) {
                    if(pthread_mutex_lock(pmtxCount) != 0)
                        cerr << "Unable to lock pmtxCount - SendToTNC - Msg.\n" << flush;

                    msg_cnt++;
                    if(pthread_mutex_unlock(pmtxCount) != 0)
                        cerr << "Unable to unlock pmtxCount - SendToTNC - Msg.\n" << flush;

                    memset(szUserMsg,0,MAX);
                    ostrstream umsg(szUserMsg,MAX-1);
                    umsg << abuff->peer << " " << abuff->user
                        << ": "
                        << abuff->getChar()
                        << ends;

                    //Save the station-to-station message in the log
                    WriteLog(szUserMsg,STSMLOG);
                }
                rfWrite(rfbuf);         // Send string out on RF via TNC
            }
        }
    }

    if (abuff) {
        while (abuff->ttl > 0)
            reliable_usleep(10);                 // wait 'till it's safe to delete this...

        delete abuff;                   // ...ack repeater thread will set ttl to zero
    }                                   // ...Perhaps the ack repeater should delete this?

    if (rfbuf)
        delete[] rfbuf;

    return ;
}


//-----------------------------------------------------------------------
int SendSessionStr(int session, const char *s)
{
    int rc, retrys;

    if(pthread_mutex_lock(pmtxSend) != 0)
        cerr << "Unable to lock pmtxSend - SendSessionStr.\n" << flush;

    retrys = 0;

    do {
        rc = send(session, s, strlen(s), 0);
        if (rc < 0) {
            reliable_usleep(50000);              // try again 50ms later
            retrys++;
        }
    } while((rc < 0) && (errno == EAGAIN) && (retrys <= MAXRETRYS));

    if(pthread_mutex_unlock(pmtxSend) != 0)
        cerr << "Unable to unlock pmtxSend - SendSessionStr.\n" << flush;

    return(rc);
}


//-----------------------------------------------------------------------
void endSession(int session, char* szPeer, char* userCall, time_t starttime)
{
    char szLog[MAX],infomsg[MAX];

    if (ShutDownServer)
        pthread_exit(0);

    if(pthread_mutex_lock(pmtxSend) != 0)
        cerr << "Unable to lock pmtxSend - endSession.\n" << flush;

    DeleteSession(session);             // remove it  from list
    shutdown(session, 2);
    close(session);                     // Close socket
    if(pthread_mutex_unlock(pmtxSend) != 0)
        cerr << "Unable to unlock pmtxSend - endSession.\n" << flush;

    {
        char* cp = new char[128];
        memset(cp, 0, 128);
        ostrstream msg(cp,127);
        msg << szPeer << " " << userCall
            << " has disconnected\n"
            << ends;

        conQueue.write(cp, 0);
    }

    strncpy(szLog, szPeer, MAX - 1);
    strcat(szLog, " ");
    strcat(szLog, userCall);
    strcat(szLog, " disconnected ");
    time_t endtime = time(NULL);
    double dConnecttime = difftime(endtime , starttime);
    int iMinute = (int)(dConnecttime / 60);
    iMinute = iMinute % 60;
    int iHour = (int)dConnecttime / 3600;
    int iSecond = (int)dConnecttime % 60;
    char timeStr[32];
    sprintf(timeStr, "%3d:%02d:%02d", iHour, iMinute, iSecond);
    strcat(szLog, timeStr);

    WriteLog(szLog, MAINLOG);

    {   memset(infomsg,0,MAX);
        ostrstream msg(infomsg, MAX-1);

        msg << szServerCall
            << szJAVAMSG
            << MyLocation << " "
            << szServerID
            << szPeer
            << " " << userCall
            << " disconnected. "
            << ConnectedClients
            << " users online.\r\n"
            << ends;
    }
    if (broadcastJavaInfo)
        BroadcastString(infomsg);           // Say IP address of disconected client

    if (strlen(userCall) > 0) {
        if(pthread_mutex_lock(pmtxCount) != 0)
            cerr << "Unable to lock pmtxCount - EndSession.\n" << flush;

        memset(infomsg,0,MAX);
        ostrstream msg(infomsg, MAX-1);
        msg << szServerCall
            << szAPRSDPATH
            << szUSERLISTMSG
            << MyLocation
            << ": Disconnected from "
            << userCall
            << ". "
            << ConnectedClients << " users"
            << "\r\n"
            << ends;

        if(pthread_mutex_unlock(pmtxCount) != 0)
            cerr << "Unable to unlock pmtxCount - EndSession.\n" << flush;

        BroadcastString(infomsg);       // Say call sign of disconnected client
    }

    pthread_exit(0);
}


//-----------------------------------------------------------------------
//An instance of this thread is created for each user who connects.
//
void *TCPSessionThread(void *p)
{
    char buf[BUFSIZE];
    string pgm_vers;

    SessionParams *psp = (SessionParams*)p;
    int session = psp->Socket;
    int EchoMask = psp->EchoMask;
    int serverport = psp->ServerPort;

    delete psp;

    int BytesRead, i;
    struct sockaddr_in peer_adr;
    char szPeer[MAX], szError[MAX], szLog[MAX], infomsg[MAX], logmsg[MAX];

    const char *szUserStatus;
    unsigned char c;
    unsigned adr_size = sizeof(peer_adr);
    int n, rc,data,verified=false, loggedon=false;
    ULONG State = BASE;
    char userCall[10];
    char* tc;
    char checkdeny = '+';               // Default to no restrictions
    const char *szRestriction;
    int dummy;

    // These deal with Telnet protocol option negotiation suppression
    int iac,sbEsc;
    const int IAC = 255, SB = 250, SE = 240;

    char szUser[16], szPass[16];
    //char* szServerPort[10];

    if (session < 0)
        return NULL;

    DBstring = "TCPSessionThread - basic initialization complete";

    if(pthread_mutex_lock(pmtxCount) != 0)
        cerr << "Unable to lock pmtxCount - TCPSessionThread - TotalConnects.\n" << flush;

    TotalConnects++;
    if(pthread_mutex_unlock(pmtxCount) != 0)
        cerr << "Unable to unlock pmtxCount - TCPSessionThread - Total Connects.\n" << flush;

    time_t  starttime = time(NULL);

    szPeer[0] = NULLCHR;
    userCall[0] = NULLCHR;

    if (getpeername(session, (struct sockaddr *)&peer_adr, &adr_size) == 0)
        strncpy(szPeer,inet_ntoa(peer_adr.sin_addr),32);

    {   memset(szError,0,MAX);
        ostrstream msg(szError, MAX-1);  // Build an error message in szError

        msg << szServerCall
            << szJAVAMSG
            << "Limit of "
            << MaxClients
            << " users exceeded.  Try again later. Disconnecting...\r\n"
            << ends;
    }

    {
        char *cp = new char[256];
        memset(cp, 0, 256);
        ostrstream msg(cp, 255);
        msg << szPeer << " has connected to port " << serverport << endl << ends;
        conQueue.write(cp, 0);           // queue reader deletes cp
    }

    {   memset(szLog,0,MAX);
        ostrstream msg(szLog, MAX-1);
        msg << szPeer
            << " connected on "
            << serverport
            << ends;

        WriteLog(szLog, MAINLOG);
    }

    data = 1;                           // Set socket for non-blocking

    DBstring = "TCPSessionThread - entering ioctl call";
    ioctl(session,FIONBIO,(char*)&data,sizeof(int));
    DBstring = "TCPSessionThread - exiting ioctl call";

    rc = SendSessionStr(session,SIGNON);

    if (rc < 0)
        endSession(session, szPeer, userCall, starttime);

    if (!NetBeacon.empty())
        rc = SendSessionStr(session, NetBeacon.c_str());

    if (rc < 0)
        endSession(session, szPeer, userCall, starttime);

    if (EchoMask & sendHISTORY) {
        DBstring = "TCPSessionThread - call to SendHistory";
        n = SendHistory(session,(EchoMask & ~(srcSYSTEM | srcSTATS)));  // Send out the previous N minutes of APRS activity
                                                                        // except messages generated by this system.
        DBstring = "TCPSessionThread - return from SendHistory";
        if (n < 0) {
            memset(szLog,0,MAX);
            ostrstream msg(szLog, MAX-1);
            msg << szPeer
                << " aborted during history dump on "
                << serverport
                << ends;

            WriteLog(szLog,MAINLOG);
            endSession(session,szPeer,userCall,starttime);
        }

        {
            char *cp = new char[256];
            memset(cp, 0, 256);
            ostrstream msg(cp,255);
            msg << "Sent " << n << " history items to " << szPeer << endl << ends;
            conQueue.write(cp,0);       // queue reader deletes cp
        }
    }
    char *pWelcome = new char[strlen(CONFPATH) + strlen(WELCOME) + 1];
    strcpy(pWelcome, CONFPATH);
    strcat(pWelcome, WELCOME);

    rc = SendFiletoClient(session, pWelcome);    // Read Welcome message from file

    if (rc < 0) {
        memset(szLog,0,MAX);
        ostrstream msg(szLog,MAX-1);
        msg << szPeer
            << " aborted welcome msg on "
            << serverport
            << ends;

        WriteLog(szLog, MAINLOG);
        delete[] pWelcome;
        endSession(session, szPeer, userCall, starttime);
    }

    if (pWelcome != NULL)
        delete[] pWelcome;

    DBstring = "TCPSessionThread - get session pointer via AddSession";
    SessionParams* sp =  AddSession(session, EchoMask);
    DBstring = "TCPSessionThread - return with session pointer via AddSession";



    if (sp == NULL) {
        rc = SendSessionStr(session,szError);
        if (rc == -1)
            perror("AddSession");

        WriteLog("Error, too many users",MAINLOG);
        cerr << "Can't find free session.\n" << flush;		// debug stuff
        endSession(session,szPeer,userCall,starttime);
        char *cp = new char[256];
        memset(cp, 0, 256);
        ostrstream msg(cp, 255);
        msg <<  "Can't add client to session list, too many users - closing connection.\n"
            << ends;

        conQueue.write(cp,0);
    }

    AddSessionInfo(session,"*",szPeer,serverport, "*");

    {
        memset(infomsg,0,MAX);
        ostrstream msg(infomsg, MAX-1);

        msg << szServerCall
            << szJAVAMSG
            << MyLocation << " "
            << szServerID
            << szPeer
            << " connected "
            << ConnectedClients
            << " users online.\r\n"
            << ends;

        if (broadcastJavaInfo)
            BroadcastString(infomsg);
    }

    if(pthread_mutex_lock(pmtxCount) != 0)
        cerr << "Unable to lock pmtxCount - TCPSessionThread - disco1.\n" << flush;

    if (ConnectedClients > MaxConnects)
        MaxConnects = ConnectedClients;

    if(pthread_mutex_unlock(pmtxCount) != 0)
        cerr << "Unable to unlock pmtxCount - TCPSessionThread - disco1.\n" << flush;

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

        BytesRead = 0;                  // initialize byte counter

        do {
            if ((charQueue.ready()) && (State == REMOTE) ) {
                tc = (char*) charQueue.read(&dummy);
                send(session,&tc,1,0);  // send a tnc character to sysop
                //printhex(&tc,1);
            }

            do {
                c = 0x7F;
                i = recv(session,&c,1,0);   // get 1 byte into c
            } while(c == 0x00);

            if (ShutDownServer)
                raise(SIGTERM);         // Terminate this process

            //if (State == REMOTE) printhex(&c,1); debug
            if (i == -1) {
                if (errno != EWOULDBLOCK) {
                    BytesRead = 0;      // exit on errors other than EWOULDBLOCK
                    i = 0;
                    endSession(session, szPeer, userCall, starttime);
                }

                //cerr << "i=" << i << "  chTimer=" << chTimer << "   c=" << c << endl;
                if (State != REMOTE)
                    sleep(1);           // Don't hog cpu while in loop awaiting data
            }
            if (sp->dead) {             // force disconnect if connection is dead
                BytesRead = 0;
                i = 0;
                endSession(session, szPeer, userCall, starttime);
            }

            if (i != -1) {              // Got a real character from the net
                if (loggedon == false) {
                    if ((c == IAC) && (sbEsc == false))
                        iac = 3;        // This is a Telnet IAC byte.  Ignore the next 2 chars

                    if ((c == SB) && (iac == 2))
                        sbEsc = true;   // SB is suboption begin.
                }                       // Ignore everything until SE, suboption end.

                // printhex(&c,1);  //debug
                //if ( !((lastch == 0x0d) && (c == 0x0a)) && (c != 0) && (iac == 0) )	  //reject LF after CR and NULL

                // This logic discards CR or LF if it's the first character on the line.
                bool cLFCR =  (( c == LF) || ( c == CR));
                bool rejectCH = (((BytesRead == 0) && cLFCR) || (c == 0)) ;  //also discard NULLs

                if ((rejectCH == false) && (iac == 0)) {
                    if (BytesRead < BUFSIZE-3)
                        buf[BytesRead] = c;

                    BytesRead += i;

                    // Only enable control char interpreter if user is NOT logged on in client mode
                    if (loggedon == false) {
                        switch (c) {
                            case 0x04:  // Control-D = disconnect
                                i = 0;
                                BytesRead = 0;
                                break;

                            case 0x1b:  // ESC = TNC control
                                if ((State == BASE) && tncPresent) {
                                    sp->EchoMask = 0;   // Stop echoing the aprs data stream.
                                    rc = SendSessionStr(session,"\r\n220 Remote TNC control mode. <ESC> to quit.\r\n503 User:");

                                    if (rc < 0)
                                        endSession(session,szPeer,userCall,starttime);

                                    State = USER;       // Set for remote control of TNC
                                    BytesRead = 0;
                                    break;
                                }

                                //<ESC> = exit TNC control
                                if ((State != BASE) && tncPresent) {
                                    if (State == REMOTE) {
                                        memset(szLog,0,MAX);
                                        ostrstream log(szLog, MAX-1);

                                        log << szPeer << " "
                                            << szUser << " "
                                            << " Exited TNC remote sysop mode."
                                            << endl
                                            << ends;

                                        WriteLog(szLog,MAINLOG);
                                    }

                                    tncMute = false;
                                    TncSysopMode = false;
                                    State = BASE;   // <ESC>Turn off remote
                                    rc = SendSessionStr(session,"\r\n200 Exit remote mode successfull\r\n");

                                    if (rc < 0)
                                        endSession(session,szPeer,userCall,starttime);

                                    sp->EchoMask = EchoMask;    // Restore aprs data stream.
                                    BytesRead = 0;  //Reset buffer
                                }

                                //i = 0;
                                break;
                        }; // switch
                    } //end if (loggedon==false)

                    if ((State == REMOTE) && (c != 0x1b) && (c != 0x0) && (c != 0x0a)) {
                        char chbuf[2];
                        chbuf[0] = c;
                        chbuf[1] = NULLCHR;
                        rfWrite(chbuf);     // Send chars to TNC in real time if REMOTE
                    }
                }
            } else
                c = 0;

            if (loggedon == false) {
                if (c == SE) {
                    sbEsc = false;
                    iac = 0;
                }  // End of telnet suboption string

                if (sbEsc == false)
                    if(iac-- <= 0)
                        iac = 0;        // Count the bytes in the Telnet command string
            }

            // Terminate loop when we see a CR or LF if it's not the first char on the line.
        } while (((i != 0) && (c != 0x0d) && (c != 0x0a)) || ((BytesRead == 0) && (i != 0)));

        if ((BytesRead > 0) && ( i > 0)  ) {    // 1 or more bytes needed
            i = BytesRead - 1;
            RemoveCtlCodes(buf);

            buf[i++] = 0x0d;            // Put a CR-LF on the end of the buffer
            buf[i++] = 0x0a;
            buf[i++] = 0;
            
            if(pthread_mutex_lock(pmtxCount) != 0)
                cerr << "Unable to lock pmtxCount - TCPSessionThread - Character/Frame Counters.\n" << flush;

            TotalUserChars += i;
            frame_cnt++;

            if(pthread_mutex_unlock(pmtxCount) != 0)
                cerr << "Unable to unlock pmtxCount - TCPSessionThread - Character/Frame Counters.\n" << flush;

            if (sp) {
                sp->bytesIn += i;
                sp->lastActive = time(NULL);
            }

            // cout << szPeer << ": " << buf << flush;		 //Debug code
            //printhex(buf,i);

            //cout << endl;
            //printhex(buf,strlen(buf));

            if (State == REMOTE) {
                buf[i-2] = NULLCHR;        // No line feed required for TNC
                // printhex(buf,strlen(buf)); //debug code
            }

            if (State == BASE) {        // Internet to RF messaging handler
                bool sentOnRF=false;

                TAprsString atemp(buf, session, srcUSER, szPeer, userCall);

                if (atemp.aprsType == APRSQUERY){   // non-directed query ?
                    queryResp(session,&atemp);      // yes, send our response
                }

                //cout << atemp << endl;
                //cout << atemp.stsmDest << "|" << szServerCall << "|" << atemp.aprsType << endl;

                if ((atemp.aprsType == APRSMSG) && (atemp.msgType == APRSMSGQUERY)) {
                    // is this a directed query message?
                    //if ((stricmp(szServerCall.c_str(), atemp.stsmDest.c_str()) == 0)
                    //        || (stricmp("aprsd",atemp.stsmDest.c_str()) == 0)
                    //        || (stricmp("igate",atemp.stsmDest.c_str()) == 0)) {    // Is query for us?
                    //
                    //    queryResp(session,&atemp);  // Yes, respond.
                    //}
                    if ((respondToIgateQueries) && (stricmp(szServerCall.c_str(), atemp.stsmDest.c_str()) == 0)
                            && (stricmp("igate", atemp.stsmDest.c_str()) == 0))  {
                        queryResp(session, &atemp);  // Yes, respond.
                    } else {
                        cerr << "Ignored IGATE query from " << atemp.ax25Source << ends << endl;
                    }

                    if ((respondToAprsdQueries) && (stricmp(szServerCall.c_str(), atemp.stsmDest.c_str()) == 0)
                            && (stricmp("aprsd", atemp.stsmDest.c_str()) == 0))  {
                        queryResp(session, &atemp);  // Yes, respond.
                    } else {
                        cerr << "Ignored APRSD query from " << atemp.ax25Source << ends << endl;
                    }
                }

                string vd;
                unsigned idxInvalid=0;

                if (atemp.aprsType == APRSLOGON) {
                    loggedon = true;

                    verified = false;

                    vd = atemp.user + atemp.pass ;

                    // 2.0.7b Security bug fix - don't allow ;!@#$%~^&*():="\<>[]  in user or pass!
                    if (((idxInvalid = vd.find_first_of(";!@#$%~^&*():=\"\\<>[]",0,20)) == string::npos)
                            && (atemp.user.length() <= 15)      // Limit length to 15 or less
                            && (atemp.pass.length() <= 15)) {

                        if (validate(atemp.user.c_str(), atemp.pass.c_str(),APRSGROUP, APRS_PASS_ALLOW) == 0)
                            verified = true;
                    } else {
                        if (idxInvalid != string::npos) {
                            char *cp = new char[256];
                            memset(cp, 0, 256);
                            ostrstream msg(cp, 255);

                            msg << szPeer
                                << " Invalid character \""
                                << vd[idxInvalid]
                                << "\" in APRS logon"
                                << endl
                                << ends ;

                            conQueue.write(cp,0);       // cp deleted by queue reader
                            WriteLog(cp,MAINLOG);
                        }
                    }

                    checkdeny = toupper(checkUserDeny(atemp.user)); // returns + , L or R
                                                                    // + = no restriction
                                                                    // L = No login
                                                                    // R = No RF
                    if (verified) {
                        szUserStatus = szVERIFIED ;
                        if (sp)
                            sp->vrfy = true;
                    } else
                        szUserStatus = szUNVERIFIED;

                    switch (checkdeny) {
                        case 'L':       // Read only access
                            szUserStatus = szACCESSDENIED;
                            szRestriction = szRM2;
                            verified = false;
                            break;

                        case 'R':       // No send on RF access
                            szRestriction = szRM3;
                            break;

                        default:
                            szRestriction = szRM1;
                    } // switch

                    if (checkdeny != 'L') {
                        memset(infomsg,0,MAX);
                        ostrstream msg(infomsg, MAX-1);

                        msg << szServerCall
                            << szAPRSDPATH
                            << szUSERLISTMSG
                            << MyLocation << ": "
                            << szUserStatus << " "
                            << atemp.user
                            << " using "
                            << atemp.pgmName << " "
                            << atemp.pgmVers
                            << ". "
                            << ConnectedClients << " users"
                            << "\r\n"   // Don't want acks from this!
                            << ends;

                        BroadcastString(infomsg);   // send users logon status to everyone
                    }

                    {
                        memset(logmsg,0,MAX);
                        ostrstream msg(logmsg, MAX-1);
                        msg << szPeer
                            << " " << atemp.user
                            << " " << atemp.pgmName
                            << " " << atemp.pgmVers
                            << " " << szUserStatus
                            << endl
                            << ends;
                    }

                    WriteLog(logmsg,MAINLOG);
                    strncpy(userCall,atemp.user.c_str(),9);     // save users call sign
                    pgm_vers = atemp.pgmName + " " + atemp.pgmVers;
                    AddSessionInfo(session,userCall,szPeer,serverport,pgm_vers.c_str());  // put it here so HTTP monitor can see it

                    if ((!verified) || (checkdeny == 'R')) {
                        char call_pad[] = "         ";  // 9 spaces
                        int len = strlen(atemp.user.c_str());
                        if (len > 9)
                            len = 9;

                        memmove(call_pad,atemp.user.c_str(),len);

                        {
                            memset(infomsg,0,MAX);
                            ostrstream msg(infomsg, MAX-1);  // Message to user...

                            msg << szServerCall
                                << szAPRSDPATH
                                << ':'
                                << call_pad
                                << ":" << szRestriction
                                << ends ;
                        }

                        rc = SendSessionStr(session,infomsg);
                        if (rc < 0)
                            endSession(session,szPeer,userCall,starttime);

                        if (checkdeny == '+') {
                            memset(infomsg,0,MAX);
                            ostrstream msg(infomsg, MAX-1);  // messsage to user
                            msg << szServerCall
                                << szAPRSDPATH
                                << ':'
                                << call_pad
                                << ":Contact program author for registration number.\r\n"
                                << ends ;

                            rc = SendSessionStr(session,infomsg);

                            if (rc < 0)
                                endSession(session,szPeer,userCall,starttime);
                        }
                    }

                    if (verified && (atemp.pgmName.compare("monitor") == 0)) {
                        if (sp) {
                            sp->EchoMask = srcSTATS;
                            char prompt[] = "#Entered Monitor mode\n\r";
                            TAprsString *amsg = new TAprsString(prompt,SRC_USER,srcSTATS);
                            sendQueue.write(amsg);
                            AddSessionInfo(session,userCall,szPeer,serverport,"Monitor");
                        }
                    }
                }

                // One of the stations in the gate2rf list?
                bool RFalways = find_rfcall(atemp.ax25Source,rfcall);

                if ( verified  && (!RFalways) && (atemp.aprsType == APRSMSG) && (checkdeny == '+')) {
                    sentOnRF = false;
                    atemp.changePath("TCPIP*","TCPIP");

                    sentOnRF = sendOnRF(atemp,szPeer,userCall,srcUSERVALID);    // Send on RF if dest local

                    if (sentOnRF) {     //Now find the posit for this guy in the history list
                                        // and send it too.
                        TAprsString* posit = getPosit(atemp.ax25Source,srcIGATE | srcUSERVALID);

                        if (posit != NULL) {
                            time_t Time = time(NULL);       // get current time

                            if ((Time - posit->timeRF) >= 60*10) {  // every 10 minutes only
                                timestamp(posit->ID,Time);          // Time stamp the original in hist. list
                                posit->stsmReformat(MyCall);        // Reformat it for RF delivery
                                tncQueue.write(posit);              // posit will be deleted elsewhere
                            } else
                                delete posit;
                        } /*else cout << "Can't find matching posit for "
                                                << atemp.ax25Source
                                                << endl
                                                << flush;        //Debug only
                                                */
                    }
                }
                if (atemp.aprsType == APRSERROR) {
                    if(pthread_mutex_lock(pmtxCount) != 0)
                        cerr << "Unable to lock pmtxCount - TCPSessionThread - error counter.\n" << flush;

                    error_cnt++;
                    if(pthread_mutex_unlock(pmtxCount) != 0)
                        cerr << "Unable to unlock pmtxCount - TCPSessionThread - error counter.\n" << flush;
                }

                // Filter out COMMENT type packets, eg: # Tickle
                if ( verified && (atemp.aprsType != COMMENT) && (atemp.aprsType != APRSLOGON) ) {
                    TAprsString* inetpacket = new TAprsString(buf,session,srcUSERVALID,szPeer,userCall);
                    inetpacket->changePath("TCPIP*","TCPIP") ;

                    if (inetpacket->aprsType == APRSMIC_E) {    // Reformat Mic-E packets
                        reformatAndSendMicE(inetpacket,sendQueue);
                    } else
                        sendQueue.write(inetpacket);    // note: inetpacket is deleted in DeQueue
                }

                if (!verified && (atemp.aprsType != COMMENT)
                        && (atemp.aprsType != APRSLOGON)
                        && (checkdeny != 'L') ) {

                    TAprsString* inetpacket = new TAprsString(buf,session,srcUSER,szPeer,userCall);

                    if (inetpacket->ax25Source.compare(userCall) != 0)
                        inetpacket->EchoMask = 0;       // No tcpip echo if not from user

                    inetpacket->changePath("TCPIP*","TCPIP") ;

                    if (inetpacket->changePath("TCPXX*","TCPIP*") == false)
                        inetpacket->EchoMask = 0;       // No tcpip echo if no TCPXX* in path;

                    //inetpacket->print(cout);  //debug
                    sendQueue.write(inetpacket);        // note: inetpacket is deleted in DeQueue
                }

                if ((atemp.aprsType == APRSMSG) && (RFalways == false) ) {
                    TAprsString* posit = getPosit(atemp.ax25Source,srcIGATE | srcUSERVALID | srcTNC);
                    if (posit != NULL) {
                        posit->EchoMask = src3RDPARTY;
                        sendQueue.write(posit);         // send matching posit only to msg port
                    }
                }

                // Here's where the priviledged get their posits sent to RF full time.
                if (configComplete
                        && verified
                        && RFalways
                        && (StationLocal(atemp.ax25Source.c_str(),srcTNC) == false)
                        && (atemp.tcpxx == false)
                        && (checkdeny == '+')) {

                    TAprsString* RFpacket = new TAprsString(buf,session,srcUSER,szPeer,userCall);
                    RFpacket->changePath("TCPIP*","TCPIP");

                    if (RFpacket->aprsType == APRSMIC_E) {      // Reformat Mic-E packets
                        if (ConvertMicE) {
                            TAprsString* posit = NULL;
                            TAprsString* telemetry = NULL;
                            RFpacket->mic_e_Reformat(&posit,&telemetry);

                            if (posit) {
                                posit->stsmReformat(MyCall);    // Reformat it for RF delivery
                                tncQueue.write(posit);          // Send reformatted packet on RF
                            }

                            if (telemetry) {
                                telemetry->stsmReformat(MyCall);    // Reformat it for RF delivery
                                tncQueue.write(telemetry);          // Send packet on RF
                            }

                            delete RFpacket;
                        } else {
                            RFpacket->stsmReformat(MyCall);
                            tncQueue.write(RFpacket);   // Raw MIC-E data to RF
                        }
                    } else {
                        RFpacket->stsmReformat(MyCall);
                        tncQueue.write(RFpacket);   // send data to RF
                    }                   // Note: RFpacket is deleted elsewhere
                }
            }

            int j = i-3;

            if ((State == PASS) && (BytesRead > 1)) {
                strncpy(szPass,buf,15);

                if (j<16)
                    szPass[j] = NULLCHR;
                else
                    szPass[15] = NULLCHR;

                bool verified_tnc = false;
                unsigned idxInvalid=0;

                int valid = -1;

                string vd = string(szUser) + string(szPass) ;

                // 2.0.7b Security bug fix - don't allow ;!@#$%~^&*():="\<>[]  in szUser or szPass!
                // Probably not needed in 2.0.9 because validate is not an external pgm anymore!
                if (((idxInvalid = vd.find_first_of(";!@#$%~^&*():=\"\\<>[]",0,20)) == string::npos)
                        && (strlen(szUser) <= 16)       // Limit length to 16 or less
                        && (strlen(szPass) <= 16)) {

                    valid = validate(szUser,szPass,TNCGROUP,APRS_PASS_ALLOW);   // Check user/password
                } else {
                    if (idxInvalid != string::npos) {
                        char *cp = new char[256];
                        memset(cp, 0, 256);
                        ostrstream msg(cp, 255);

                        msg << szPeer
                            << " Invalid character \""
                            << vd[idxInvalid]
                            << "\" in TNC logon"
                            << endl
                            << ends ;

                        conQueue.write(cp,0);       // cp deleted by queue reader
                        WriteLog(cp,MAINLOG);
                    }
                }

                if (valid == 0)
                    verified_tnc = true;

                if (verified_tnc) {
                    if (TncSysopMode == false) {
                        TncSysopMode = true;

                        State = REMOTE;
                        tncMute = true;
                        rc = SendSessionStr(session,"\r\n230 Login successful. <ESC> to exit remote mode.\r\n");

                        if (rc < 0)
                            endSession(session,szPeer,userCall,starttime);

                        memset(szLog,0,MAX);
                        ostrstream log(szLog, MAX-1);
                        log << szPeer << " "
                            << szUser
                            << " Entered TNC remote sysop mode."
                            << endl
                            << ends;

                        WriteLog(szLog,MAINLOG);
                    } else {
                        rc = SendSessionStr(session,"\r\n550 Login failed, TNC is busy\r\n");

                        if (rc < 0)
                            endSession(session,szPeer,userCall,starttime);

                        memset(szLog,0,MAX);
                        ostrstream log(szLog, MAX-1);
                        log << szPeer << " "
                            << szUser
                            << " Login failed: TNC busy."
                            << endl
                            << ends;

                        WriteLog(szLog,MAINLOG);
                        State = BASE;

                        if (sp) {
                            sp->EchoMask = EchoMask;    // Restore original echomask
                            AddSessionInfo(session,userCall,szPeer,serverport,pgm_vers.c_str());
                        } else {
                            // failed to get a session
                        }
                    }
                } else {
                    rc = SendSessionStr(session,"\r\n550 Login failed, invalid user or password\r\n");

                    if (rc < 0)
                        endSession(session,szPeer,userCall,starttime);

                    memset(szLog,0,MAX);
                    ostrstream log(szLog, MAX-1);

                    log << szPeer << " "
                        << szUser  << " "
                        << szPass
                        << " Login failed: Invalid user or password."
                        << endl
                        << ends;

                    WriteLog(szLog,MAINLOG);
                    State = BASE;

                    if (sp) {
                        sp->EchoMask = EchoMask;
                        AddSessionInfo(session,userCall,szPeer,serverport,pgm_vers.c_str());
                    } else {
                        // failed to get a session
                    }
                }
            }

            if ((State == USER) && (BytesRead > 1)) {
                strncpy(szUser,buf,15);
                if (j < 16)
                    szUser[j] = NULLCHR;
                else
                    szUser[15]=NULLCHR;

                State = PASS;
                rc = SendSessionStr(session,"\r\n331 Pass:");

                if (rc < 0)
                    endSession(session,szPeer,userCall,starttime);
            }
        }
    } while (BytesRead != 0);   // Loop back and get another line from remote user.

    if (State == REMOTE) {
        tncMute = false;
        TncSysopMode = false;
    }

    endSession(session,szPeer,userCall,starttime);

    pthread_exit(0);  //Actually thread exits from endSession above.
}


//------------------------------------------------------------------------
//  One instance of this thread is created for each port definition in aprsd.conf.
//  Each instance listens on the a user defined port number for clients
//  wanting to connect.  Each connect request is assigned a
//  new socket and a new instance of TCPSessionThread() is created.
//
void *TCPServerThread(void *p)
{
    int s;
    int rc = 0;
    unsigned i;
    SessionParams* session;
    pthread_t SessionThread;
    struct sockaddr_in server,client;
    int optval;
    ServerParams *sp = (ServerParams*)p;
    
    sp->pid = getpid();
    s = socket(PF_INET,SOCK_STREAM,0);

    sp->ServerSocket = s;

    optval = 1;                                 // Allow address reuse
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char*)&optval, sizeof(int));
    setsockopt(s, SOL_SOCKET, SO_KEEPALIVE, (char*)&optval, sizeof(int));

    if (s == 0) {
        perror("TCPServerThread socket error");
        ShutDownServer = true;
        return NULL;
    }

    memset(&server, NULLCHR, sizeof(server));
    memset(&client, NULLCHR, sizeof(client));

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(sp->ServerPort);

    if (bind(s, (struct sockaddr *)&server, sizeof(server)) <  0) {
        perror("TCPServerThread bind error");
        ShutDownServer = true;
        return NULL;
    }

    cout << "TCP Server listening on port " << sp->ServerPort << endl << flush;

    while (!configComplete)
        reliable_usleep(100000);                // Wait till everything else is running.

    listen(s, 2);

    for(;;) {
        i = sizeof(client);
        session = new SessionParams;
        session->Socket = accept(s, (struct sockaddr *)&client, &i);
        session->EchoMask = sp->EchoMask;
        session->ServerPort = sp->ServerPort;

        if (ShutDownServer) {
            close(s);
            if (session->Socket >= 0)
                close(session->Socket);

            cerr << "Ending TCP server thread\n" << flush;
            delete session;

            if (ShutDownServer)
                raise(SIGTERM);                 // Terminate this process
        }

        if (session->Socket < 0) {
            perror( "Error in accepting a connection");
            delete session;
        } else {
            if (session->EchoMask & wantHTML) {
                rc = pthread_create(&SessionThread, NULL, HTTPServerThread, session);  //Added in 2.1.2
            } else {
                rc = pthread_create(&SessionThread, NULL, TCPSessionThread, session);
            }
        }
        if (rc != 0) {
            cerr << "Error creating new client thread.\n" << flush;
            shutdown(session->Socket,2);
            rc = close(session->Socket);        // Close it if thread didn't start
            delete session;

            if (rc < 0)
                perror("Session Thread close()");
        } else                                  // session will be deleted in TCPSession Thread
            pthread_detach(SessionThread);      // run session thread DETACHED!

        memset(&client, NULLCHR, sizeof(client));
    }
    return(0);
}


//----------------------------------------------------------------------
// This thread listens to a UDP port and sends all packets heard to all
// logged on clients unless the destination call is "TNC" it sends it
// out to RF.
//
void *UDPServerThread(void *p)
{
#define UDPSIZE 256
    int s,i;
    unsigned client_address_size;
    struct sockaddr_in client, server;
    char buf[UDPSIZE+3],szLog[UDPSIZE+50];
    UdpParams* upp = (UdpParams*)p;
    int UDP_Port = upp->ServerPort;     // UDP port set in aprsd.conf
    const char *CRLF = "\r\n";

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
    server.sin_family = AF_INET;        // Server is in Internet Domain
    server.sin_port = htons(UDP_Port);  // 0 = Use any available port
    server.sin_addr.s_addr = INADDR_ANY;    // Server's Internet Address

    if (bind(s, (struct sockaddr *)&server, sizeof(server)) < 0) {
        perror("Datagram socket bind error");
        ShutDownServer = true;
        return NULL;
    }

    cout << "UDP Server listening on port " << UDP_Port << endl << flush;

    for (;;) {                          // Loop forever
        client_address_size = sizeof(client);
        i = recvfrom(s, buf, UDPSIZE, 0, (struct sockaddr *) &client, &client_address_size) ; //Get client udp data

        bool sourceOK = false;
        int n=0;
        do {                            // look for clients IP address in list of trusted addresses.
            long maskedTrusted = Trusted[n].sin_addr.s_addr & Trusted[n].sin_mask.s_addr;
            long maskedClient = client.sin_addr.s_addr & Trusted[n].sin_mask.s_addr;
            if(maskedClient == maskedTrusted) sourceOK = true;
            n++;
        } while ((n < nTrusted) && (sourceOK == false)) ;

        if (sourceOK && configComplete && (i > 0)) {
            if (buf[i-1] != '\n')
                strcat(buf,CRLF);       // Add a CR/LF if not present

            memset(szLog,0,UDPSIZE+50);
            ostrstream log(szLog, UDPSIZE+49);
            log << inet_ntoa(client.sin_addr)
                << ": " << buf
                << ends;

            WriteLog(szLog,UDPLOG);

            TAprsString* abuff = new TAprsString(buf,SRC_UDP,srcUDP,inet_ntoa(client.sin_addr),"UDP");

            //printhex(abuff->c_str(),strlen(abuff->c_str())); //debug

            if (abuff->ax25Dest.compare("TNC") == 0  ) {    // See if it's  data for the TNC
                tncQueue.write(abuff,SRC_UDP);              // Send remaining data from ourself to the TNC
            } else {
                sendQueue.write(abuff,0);                   // else send data to all users.
            }                                               // Note that abuff is deleted in History list expire func.
        }

        if (ShutDownServer)
            raise(SIGTERM);             // Terminate this process

    }
    return NULL;
 }

//----------------------------------------------------------------------

// Receive a line of ASCII from "sock" . Nulls, line feeds and carrage returns are
// removed.  End of line is determined by CR or LF or CR-LF or LF-CR .
// CR-LF sequences appended before the string is returned.  If no data is received in "timeoutMax" seconds
// it returns int Zero else it returns the number of characters in the received
// string.
//
// Returns ZERO if timeout and -1 if socket error.
//
int recvline(int sock, char *buf, int n, int *err,int timeoutMax)
{
    int c;
    int i,BytesRead ,timeout;

    *err = 0;
    BytesRead = 0;
    bool abort;

    timeout = timeoutMax;
    abort = false;

    do {
        c = -1;

        i = recv(sock, &c, 1, 0);       // get 1 byte into c

        if (i == 0)
            abort = true;               // recv returns ZERO when remote host disconnects

        if (i == -1) {
            *err = errno;

            if ((*err != EWOULDBLOCK) || (ShutDownServer == true)) {
                BytesRead = 0;
                i = -2;
                abort = true;           // exit on errors other than EWOULDBLOCK
            }

            sleep(1);                   // Wait 1 sec. Don't hog cpu while in loop awaiting data

            if (timeout-- <= 0) {
                i = 0;
                abort = true;           // Force exit if timeout
            }

            //cout << timeout << " Waiting...  abort= " << abort << "\n";  //debug code
        }

        if (i == 1) {
            c &= 0x7f ;

            bool cLFCR =  (( c == LF) || ( c == CR));   // TRUE if c is a LF or CR
            bool rejectCH = (((BytesRead == 0) && cLFCR ) || (c == 0)) ;

            if ((BytesRead < (n - 3)) && (rejectCH == false)) {
                // reject if LF or CR is first on line or it's a NULL
                if ((c >= 0x20) || (c <= 0x7e)) {
                    buf[BytesRead] = (char)c;   // and discard data that runs off the end of the buffer
                    BytesRead++;            // We have to allow 3 extra bytes for CR,LF,NULL
                    timeout = timeoutMax;
                }
            }
        }

    } while ((c != CR) && (c != LF) && (abort == false));   // Loop while c is not CR or LF
                                                            // And no socket errors or timeouts

    //cerr << "Bytes received=" << BytesRead << " abort=" << abort << endl;   //debug code
    
    if ((BytesRead > 0) && (abort == false) ) {     // 1 or more bytes needed
        i = BytesRead -1 ;
        buf[i++] = (char)CR;            // make end-of-line CR-LF
        buf[i++] = (char)LF;
        buf[i++] = NULLCHR;                // Stick a NULL on the end.
        return(i-1);                    // Return number of bytes received.
    }

    //if(i == -2) cerr << "errorno= " << *err << endl;    //debug

    return(i);                          // Return 0 if timeout or
                                        // Return  -2 if socket error
}


//---------------------------------------------------------------------------------------------

ConnectParams* getNextHub(ConnectParams* pcp)
{
    int i = 0;

    if (!pcp->hub)
        return(pcp);

    while ((i < nIGATES) && (pcp != &cpIGATE[i]))  // Find current hub
        i++;

    //cerr << "Previous hub = " << cpIGATE[i].RemoteName << endl; //debug
    i++;

    while ((i < nIGATES) && (!cpIGATE[i].hub))      // Find next hub
        i++;

    if (i == nIGATES) {
        i = 0;                          // Wrap around to start again

        while ((i < nIGATES) && (!cpIGATE[i].hub))
            i++;
    }

    //cerr << "Next hub = " << cpIGATE[i].RemoteName << endl;   //debug

    if (cpIGATE[i].hub) {
        cpIGATE[i].pid = getpid();
        cpIGATE[i].connected = false;
        cpIGATE[i].bytesIn = 0;
        cpIGATE[i].bytesOut = 0;
        cpIGATE[i].starttime = time(NULL);
        cpIGATE[i].lastActive = time(NULL);
        return(&cpIGATE[i]);            // Return pointer to next hub
    }
    return(pcp);
}



//---------------------------------------------------------------------------------------------
// ***** TCPConnectThread *****
//
//  This thread connects to another aprsd, IGATE or APRServe machine
//  as defined in aprsd.conf with the "igate" and "hub commands.
//
//  One instance of this thread is created for each and every  igate connection.
//
//  Only a one hub thread is created regardless of the number of "hub"
//  connectons defined.  Each hub will be tried until an active one is found.
//
void *TCPConnectThread(void *p)
{
    int rc,length,state;
    int clientSocket = 0;
    int data;

    SessionParams *sp = NULL;

    struct hostent *hostinfo = NULL;
    struct hostent hostinfo_d;
    struct sockaddr_in host;
    char h_buf[1024];
    int h_err;
    char buf[BUFSIZE];
    char logonBuf[MAX];
    char remoteIgateInfo[MAX];
    char szLog[MAX];
    int retryTimer;
    ConnectParams *pcp = (ConnectParams*)p;
    int err;
    bool gotID = false;
    time_t connectTime = 0;
    bool hubConn = pcp->hub;            // Mark this as an IGATE or HUB connection

    pcp->pid = getpid();
    pcp->connected = false;
    pcp->bytesIn = 0;
    pcp->bytesOut = 0;
    pcp->starttime = time(NULL);
    pcp->lastActive = time(NULL);

    retryTimer = 60;                    // time between connection attempts in seconds

    memset(remoteIgateInfo, NULLCHR, 256);

    do {
        state = 0;

        if(pthread_mutex_lock(pmtxDNS) != 0)
            cerr << "Unable to lock pmtxDNS - TCPConnectThread.\n" << flush;

        // Thread-Safe version of gethostbyname2() ?  Actually it's still buggy so needs mutex locks!
        rc = gethostbyname2_r(pcp->RemoteName, AF_INET,
                                 &hostinfo_d,
                                 h_buf,
                                 1024,
                                 &hostinfo,
                                 &h_err);

        if(pthread_mutex_unlock(pmtxDNS) != 0)
            cerr << "Unable to unlock pmtxDNS - TCPConnectThread.\n" << flush;

        if (!hostinfo) {
            char* cp = new char[256];
            memset(cp, 0, 256);
            ostrstream msg(cp, 255);
            msg << "Can't resolve igate host name: "  << pcp->RemoteName << endl << ends;
            WriteLog(cp, MAINLOG);
            conQueue.write(cp,0);       // cp deleted by conQueue
        } else
            state = 1;

        if (state == 1) {
            clientSocket = socket(AF_INET, SOCK_STREAM, 0);
            host.sin_family = AF_INET;
            host.sin_port = htons(pcp->RemoteSocket);
            host.sin_addr = *(struct in_addr *)*hostinfo->h_addr_list;
            length = sizeof(host);

            rc = connect(clientSocket,(struct sockaddr *)&host, length);

            if (rc == -1) {
                close(clientSocket);
                memset(szLog,0,UDPSIZE+50);
                ostrstream os(szLog, UDPSIZE+49);
                os << "Connection attempt failed " << pcp->RemoteName
                    << " " << pcp->RemoteSocket << ends;

                WriteLog(szLog, MAINLOG);

                {
                    char* cp = new char[256];
                    memset(cp, 0, 256);
                    ostrstream msg(cp, 255);
                    msg <<  szLog << endl << ends;
                    conQueue.write(cp, 0);      // cp deleted by conQueue
                }

                gotID = false;
                state = 0;
            } else {

                state++;
                pcp->connected = true;
                pcp->starttime = time(NULL);
                pcp->bytesIn = 0;
                pcp->bytesOut = 0;

                memset(szLog,0,MAX);
                ostrstream os(szLog, MAX-1);
                os << "Connected to " << pcp->RemoteName
                    << " " << pcp->RemoteSocket << ends;

                WriteLog(szLog, MAINLOG);

                char* cp = new char[256];
                memset(cp, 0, 256);
                ostrstream msg(cp, 255);
                msg <<  szLog << endl << ends;
                conQueue.write(cp, 0);               // cp deleted in queue reader

            }
        }

        if (state == 2) {
            data = 1;                   // Set socket for non-blocking
            ioctl(clientSocket, FIONBIO, (char*)&data, sizeof(int));

            int optval = 1;             // Enable keepalive option
            setsockopt(clientSocket, SOL_SOCKET, SO_KEEPALIVE, (char*)&optval, sizeof(int));

            /*
                If user and password are supplied we will send our Internet user and TNC
                data to the IGATE, otherwise we just read data from the IGATE.
                NEW IN 2.1.2: If only the user name is supplied without a password
                we send a password of "-1" and do not send any data.
            */

            if (pcp->user) {
                cout << "IGATE Login: "
                    << pcp->RemoteName
                    << " " << pcp->user
                    << " "
                    << pcp->pass
                    << endl
                    << flush;

                memset(logonBuf,0,MAX);
                ostrstream logon(logonBuf, MAX-1);     // Build logon string
                logon << "user "
                    << pcp->user
                    << " pass "
                    << pcp->pass
                    << " vers "
                    << VERS
                    << "\r\n"
                    << ends;

                rc = send(clientSocket, logonBuf, strlen(logonBuf), 0); // Send logon string to IGATE or Hub

                if (pcp->EchoMask) {    // If any bits are set in EchoMask then this add to sessions list.
                    if (sp == NULL) {   // Grab an output session now. Note: This takes away 1 avalable user connection
                        sp = AddSession(clientSocket, pcp->EchoMask);    // Add this to list of sockets to send on
                    } else {            // else already had an output session

                        if(pthread_mutex_lock(pmtxAddDelSess) != 0) //need locking as initSessionParams has none
                            cerr << "Unable to lock pmtxAddDelSess - SessionParams.\n" << flush;

                        if(pthread_mutex_lock(pmtxSend) != 0)
                            cerr << "Unable to lock pmtxSend - SessionParams.\n" << flush;

                        initSessionParams(sp, clientSocket, pcp->EchoMask);   // Restore output session for sending

                        if(pthread_mutex_unlock(pmtxSend) != 0)
                            cerr << "Unable to unlock pmtxSend - SessionParams.\n" << flush;

                        if(pthread_mutex_unlock(pmtxAddDelSess) != 0)
                            cerr << "Unable to unlock pmtxAddDelSess - SessionParams.\n" << flush;


                    }

                    if (sp == NULL) {
                        cerr << "Can't add IGATE to session list .";
                        WriteLog("Failed to add IGATE to session list", MAINLOG);
                    } else {
                        AddSessionInfo(clientSocket, "*", "To IGATE", -1, "*");
                    }
                }
            }

            do {
                // Reset the retry timer to 60 only if previous connection lasted more than 30 secs.
                // If less than 30 secs the retrys will increase from 60 to 120 to 240 to 480 and finally 960 secs.
                if (connectTime > 30)
                    retryTimer = 60;        // Retry connection in 60 seconds if it breaks

                rc = recvline(clientSocket, buf, BUFSIZE, &err, 900);  // 900 sec (15 min) timeout value

                if (sp) {
                    if (sp->dead)
                        rc = -1;        // Force disconnect if OUTgoing connection is dead

                    pcp->bytesOut = sp->bytesOut;
                }

                if (rc > 0) { // rc: = chars recvd,   0 = timeout,  -2 = socket error
                    if (!gotID) {
                        if (buf[0] == '#') {    // First line starting with '#' should be the program name and version
                            strncpy(remoteIgateInfo, buf, 255);   // This gets used in the html status web page function
                            gotID = true;
                            pcp->remoteIgateInfo = remoteIgateInfo;
                        }
                        cerr << pcp->RemoteName << ":" << remoteIgateInfo ; //Debug
                    }

                    pcp->bytesIn += rc;             // Count incoming bytes
                    pcp->lastActive = time(NULL);   // record time of this input

                    bool sentOnRF = false;
                    TAprsString atemp(buf, clientSocket, srcIGATE, pcp->RemoteName, "IGATE");

                    if (atemp.aprsType == APRSQUERY) {  // non-directed query ?
                        queryResp(SRC_IGATE, &atemp);    // yes, send our response
                    }

                    //cout << atemp << endl;
                    //cout << atemp.stsmDest << "|" << szServerCall << "|" << atemp.aprsType << endl;

                    if ((atemp.aprsType == APRSMSG) && (atemp.msgType == APRSMSGQUERY)){
                        // is this a directed query message?

                        if ((stricmp(szServerCall.c_str(), atemp.stsmDest.c_str()) == 0)
                                || (stricmp("aprsd",atemp.stsmDest.c_str()) == 0)
                                || (stricmp("igate",atemp.stsmDest.c_str()) == 0)) { // Is query for us?

                            queryResp(SRC_IGATE, &atemp);   //Yes, respond.
                        }
                    }

                    // One of the stations in the gate2rf list?
                    bool RFalways = find_rfcall(atemp.ax25Source, rfcall);

                    if(pthread_mutex_lock(pmtxCount) != 0)
                        cerr << "Unable to lock pmtxCount - TCPSessionThread - IgateChars.\n" << flush;

                    TotalIgateChars += rc;

                    if(pthread_mutex_unlock(pmtxCount) != 0)
                        cerr << "Unable to unlock pmtxCount - TCPSessionThread - IgateChars.\n" << flush;

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

                        sentOnRF = sendOnRF(atemp,pcp->RemoteName, "IGATE", srcIGATE);   // Try to send on RF

                        if (sentOnRF) {     // Now find the posit for this guy in the history list
                                            // and send it too.
                            TAprsString* posit = getPosit(atemp.ax25Source, srcIGATE | srcUSERVALID);

                            if (posit != NULL) {
                                time_t Time = time(NULL);       // get current time

                                if ((Time - posit->timeRF) >= 60*10) {  // posit every 10 minutes only
                                    timestamp(posit->ID, Time);      // Time stamp the original in hist. list
                                    posit->stsmReformat(MyCall);    // Reformat it for RF delivery
                                    tncQueue.write(posit);          // posit will be deleted elsewhere?
                                } else
                                    delete posit;
                            } /*else  cout << "Can't find matching posit for "
                                               << atemp.ax25Source
                                               << endl
                                               << flush;     //Debug only
                                               */
                        }
                    }

                    // Send it on TCPIP if it's NOT a 3rd party msg
                    // OR TCPXX is in path .

                    if ((configComplete) && (atemp.aprsType != COMMENT)) {
                        // Send everything except COMMENT pkts back out to tcpip users
                        TAprsString* inetpacket = new TAprsString(buf, clientSocket, srcIGATE,pcp->RemoteName, "IGATE");
                        inetpacket->changePath("TCPIP*", "TCPIP");

                        if (inetpacket->aprsType == APRSMIC_E) {    // Reformat Mic-E packets
                            reformatAndSendMicE(inetpacket, sendQueue);
                        } else
                            sendQueue.write(inetpacket,0);          // send data to users.
                    }

                    if (configComplete && (atemp.aprsType == APRSMSG)) {    //find matching posit for 3rd party msg
                        TAprsString* posit = getPosit(atemp.ax25Source, srcIGATE | srcUSERVALID | srcTNC);
                        if (posit != NULL) {
                            posit->EchoMask = src3RDPARTY;
                            sendQueue.write(posit);         // send matching posit only to msg port

                        }
                    }

                    //Here's where the priviledged get their posits sent to RF full time.

                    if(configComplete
                            && RFalways
                            && (StationLocal(atemp.ax25Source.c_str(),srcTNC) == false)
                            && (atemp.tcpxx == false)) {

                        TAprsString* RFpacket = new TAprsString(buf,clientSocket,srcIGATE,pcp->RemoteName,"IGATE");
                        RFpacket->changePath("TCPIP*", "TCPIP");

                        if (RFpacket->aprsType == APRSMIC_E) {      // Reformat Mic-E packets
                            if (ConvertMicE) {
                                TAprsString* posit = NULL;
                                TAprsString* telemetry = NULL;
                                RFpacket->mic_e_Reformat(&posit,&telemetry);

                                if (posit) {
                                    posit->stsmReformat(MyCall);    // Reformat it for RF delivery
                                    tncQueue.write(posit);          // Send reformatted packet on RF
                                }

                                if (telemetry) {
                                    telemetry->stsmReformat(MyCall);    // Reformat it for RF delivery
                                    tncQueue.write(telemetry);          // Send packet on RF
                                }

                                delete RFpacket;
                            } else {
                                RFpacket->stsmReformat(MyCall);
                                tncQueue.write(RFpacket);   // send raw MIC-E data to RF
                            }
                        } else {
                            RFpacket->stsmReformat(MyCall);
                            tncQueue.write(RFpacket);       // send data to RF
                        }                                   // Note: RFpacket is deleted elsewhere
                    }
                }
            } while (rc > 0);           // Loop while rc is greater than zero else disconnect

            if(pthread_mutex_lock(pmtxAddDelSess) != 0)
                cerr << "Unable to lock pmtxAddDelSess - TCPConnectThread.\n" << flush;

            if(pthread_mutex_lock(pmtxSend) != 0)
                cerr << "Unable to lock pmtxSend - TCPConnectThread.\n" << flush;

            if (sp)
                sp->EchoMask = 0;       // Turn off the session output data stream if it's enabled

            shutdown(clientSocket,2);
            close(clientSocket);


            pcp->connected = false;     // set status to unconnected
            connectTime = time(NULL) - pcp->starttime ;     // Save how long the connection stayed up
            pcp->starttime = time(NULL);    // reset elapsed timer
            gotID = false;              // Force new aquisition of ID string next time we connect

            memset(szLog,0,MAX);
            ostrstream os(szLog, MAX-1);
            os << "Disconnected " << pcp->RemoteName
                << " " << pcp->RemoteSocket
                << ends;

            WriteLog(szLog, MAINLOG);

            {
                char* cp = new char[300];
                memset(cp, 0, 300);
                ostrstream msg(cp,299);
                msg <<  szLog << endl << ends;
                conQueue.write(cp,0);

            if(pthread_mutex_unlock(pmtxSend) != 0)
                cerr << "Unable to unlock pmtxSend - TCPConnectThread.\n" << flush;

            if(pthread_mutex_unlock(pmtxAddDelSess) != 0)
                cerr << "Unable to unlock pmtxAddDelSess - TCPConnectThread.\n" << flush;

            }
        }

        //cerr << pcp->RemoteName << " retryTimer= " <<  retryTimer << endl;

        gotID = false;
        sleep(retryTimer);
        retryTimer *= 2;                // Double retry time delay if next try is unsuccessful

        if (retryTimer >= (16 * 60))
            retryTimer = 16 * 60;       // Limit max to 16 minutes

        if (hubConn) {
            pcp->remoteIgateInfo = NULL;
            pcp = getNextHub(pcp);
            retryTimer = 60;            // Try next hub in 60 sec
        }
    } while(ShutDownServer == false);

    pthread_exit(0);
}

//----------------------------------------------------------------------

bool sendOnRF(TAprsString& atemp,  const char* szPeer, const char* userCall, const int src)
{
    bool sentOnRF = false;
    bool stsmRFalways =  find_rfcall(atemp.stsmDest, stsmDest_rfcall); //Always send on RF ?

    if ((atemp.tcpxx == false) && (atemp.aprsType == APRSMSG)) {
        if (checkUserDeny(atemp.ax25Source) != '+')
            return false;               // Reject if no RF or login permitted

        //Destination station active on VHFand source not?
        if (((StationLocal(atemp.stsmDest.c_str(), srcTNC) == true) || stsmRFalways)
                && (StationLocal(atemp.ax25Source.c_str(), srcTNC) == true)) {

            TAprsString* rfpacket = new TAprsString(atemp.getChar(), atemp.sourceSock, src, szPeer, userCall);
            //ofstream debug("rfdump.txt");
            //debug << rfpacket->getChar << endl ;  //Debug
            //debug.close();
            rfpacket->stsmReformat(MyCall);  // Reformat it for RF delivery
            tncQueue.write(rfpacket);        // queue read deletes rfpacket
            sentOnRF = true;
        }
    }
    return(sentOnRF);
}

//----------------------------------------------------------------------
int SendFiletoClient(int session, char *szName)
{
    char Line[256];
    APIRET rc = 0;
    int n,retrys;
    int throttle;
    
    if(pthread_mutex_lock(pmtxSendFile) != 0)
        cerr << "Unable to lock pmtxSendFile - SendFilteToClient.\n" << flush;

    ifstream file(szName);

    if (!file) {
        cerr << "Can't open " << szName << endl << flush;

        if(pthread_mutex_unlock(pmtxSendFile) != 0)
            cerr << "Unable to unlock pmtxSendFile - SendFileToClient.\n" << flush;

        return(-1);
    }
    if(pthread_mutex_lock(pmtxSend) != 0)
        cerr << "Unable to lock pmtxSend - SendFiletoClient.\n" << flush;
    do {
        file.getline(Line, 256);         // Read each line in file and send to client session
        if (!file.good())
            break;

        if (strlen(Line) > 0) {
            strncat(Line, "\r\n", 256);
            n = strlen(Line);
            retrys = 0;

            do {
                rc = send(session, Line, n, 0);
                throttle = n * 150;
                reliable_usleep(throttle);       // Limit max rate to about 50kbaud

                if (rc < 0) {
                    reliable_usleep(100000);     // 0.1 sec between retrys
                    retrys++;
                }
            } while((rc < 0) && (errno == EAGAIN) && (retrys <= MAXRETRYS));

            //if (rc == -1) {
            if (rc < 0) {
                perror("SendFileToClient()");
                shutdown(session,2);
                close(session);         // close the socket if error happened
            }

        }

    } while (file.good() && (rc >= 0));

    if(pthread_mutex_unlock(pmtxSend) != 0)
        cerr << "Unable to unlock pmtxSend - SendFiletoClient.\n" << flush;

    file.close();

    if(pthread_mutex_unlock(pmtxSendFile) != 0)
        cerr << "Unable to unlock pmtxSendFile - SendFileToClient.\n" << flush;

    return(rc);
}



//----------------------------------------------------------------------
//
//
char* getStats()
{
    time_t time_now;

    static time_t last_time = 0;
    static ULONG last_chars = 0;
    static ULONG last_tnc_chars=0;
    double serverRate = 0;
    double inetRate = 0;
    string inetRateX, serverRateX;
    char *cbuf = new char[1024];

    time(&time_now);
    upTime = ((double)(time_now - serverStartTime) / 3600);

    if(pthread_mutex_lock(pmtxCount) != 0)
        cerr << "Unable to lock pmtxCount - Calculate text stats.\n" << flush;

    aprsStreamRate =  ((TotalTncChars + TotalIgateChars + TotalUserChars - last_chars) / (time_now - last_time));
    tncStreamRate = ((TotalTncChars - last_tnc_chars) / (time_now - last_time));
    serverLoad =  (bytesSent / (time_now - last_time));

    if (aprsStreamRate > 1024) {
        inetRate = ((double)aprsStreamRate / 1024);
        inetRateX = "Kbps";
        if (inetRate > 1000) {
            inetRate = (inetRate / 1000);
            inetRateX = "Mbps";
        }
    } else {
        inetRate = (double)aprsStreamRate;
        inetRateX = "Bps";
    }

    if (serverLoad > 1024) {
        serverRate = ((double)serverLoad / 1024);
        serverRateX = "Kbps";
        if (serverRate > 1000) {
            serverRate = (serverRate / 1000);
            serverRateX = "Mbps";
        }
    } else {
        serverRate = (double)serverLoad;
        serverRateX = "Bps";
    }

    memset(cbuf, 0, 1024);
    ostrstream os(cbuf, 1023);
    os << setiosflags(ios::showpoint | ios::fixed)
        << setprecision(1)
        << "#\r\n"
        << "Server Up Time    = " << upTime << " hours" << "\r\n"
        << "Total TNC packets = " << TotalLines << "\r\n"
        << "TNC stream rate   = " << tncStreamRate << " bytes/sec" << "\r\n"
        << "Msgs gated to RF  = " << msg_cnt << "\r\n"
        << "Connect count     = " << TotalConnects << "\r\n"
        << "Users             = " << ConnectedClients << "\r\n"
        << "Peak Users        = " << MaxConnects << "\r\n"
        << "APRS Stream rate  = " << inetRate << " " << " " << inetRateX << "\r\n"
        << "Server load       = " << serverRate << " " << " " << serverRateX << "\r\n"
        << "History Items     = " << ItemCount << "\r\n"
        << "TAprsString Objs  = " << TAprsString::getObjCount() << "\r\n"
        << "Items in InetQ    = " << sendQueue.getItemsQueued() << "\r\n"
        << "InetQ overflows   = " << sendQueue.overrun << "\r\n"
        << "TncQ overflows    = " << tncQueue.overrun << "\r\n"
        << "conQ overflows    = " << conQueue.overrun << "\r\n"
        << "charQ overflow    = " << charQueue.overrun << "\r\n"
        << "Hist. dump aborts = " << dumpAborts << "\r\n"
        << ends;

    last_time = time_now;
    last_chars = TotalTncChars + TotalIgateChars + TotalUserChars;
    last_tnc_chars = TotalTncChars;

    bytesSent = 0;                      // Reset this

    if(pthread_mutex_unlock(pmtxCount) != 0)
        cerr << "Unable to unlock pmtxCount - Calculate text stats.\n" << flush;

    return(cbuf);  // cbuf deleted by calling function... or should be :)
}


//----------------------------------------------------------------------
//
void resetCounters()
{
    if(pthread_mutex_lock(pmtxCount) != 0)
        cerr << "Unable to lock pmtxCount - ResetCounters.\n" << flush;

    dumpAborts = 0;
    sendQueue.overrun = 0 ;
    tncQueue.overrun = 0 ;
    conQueue.overrun = 0 ;
    charQueue.overrun = 0;
    TotalLines = 0;
    msg_cnt = 0;
    TotalConnects = 0;
    MaxConnects = ConnectedClients;

    if(pthread_mutex_unlock(pmtxCount) != 0)
        cerr << "Unable to unlock pmtxCount - ResetCounters.\n" << flush;
}


//----------------------------------------------------------------------
//  Invoked by console 'q' quit
//
void serverQuit(termios* initial_settings)
{
    cout << endl << "Beginning shutdown...\n";
    WriteLog("Server Shutdown", MAINLOG);
    tcsetattr(fileno(stdin), TCSANOW, initial_settings); //restore terminal mode

    char *pSaveHistory = new char[strlen(VARPATH) + strlen(SAVE_HISTORY)+1];
    strcpy(pSaveHistory,VARPATH);
    strcat(pSaveHistory,SAVE_HISTORY);
    int n = SaveHistory(pSaveHistory);

    cout << "Saved "
        << n
        << " history items in "
        << pSaveHistory
        << endl << flush ;

    delete[] pSaveHistory;

    if (broadcastJavaInfo) {
        //char *ShutDown = new char[255];
        //strcpy(ShutDown,szServerCall);
        //strcat(ShutDown,szJAVAMSG);
        //strcat(ShutDown,MyLocation);
        //strcat(ShutDown," ");
        //strcat(ShutDown,szServerID);
        //strcat(ShutDown," shutting down.  Bye.\r\n");
        string ShutDown;
        ShutDown = szServerCall;
        ShutDown.append(szJAVAMSG);
        ShutDown.append(MyLocation);
        ShutDown.append(" ");
        ShutDown.append(szServerID);
        ShutDown.append(" shutting down. Bye.\r\n");

        TAprsString* abuff = new TAprsString(ShutDown, SRC_INTERNAL, srcTNC);
        //cout << abuff->c_str() << endl;
        sendQueue.write(abuff,0);
        //delete ShutDown;
    }
    sleep(1);

    if (tncPresent) {
        char *pRestore = new char[strlen(CONFPATH) + strlen(TNC_RESTORE) + 1];
        strcpy(pRestore,CONFPATH);
        strcat(pRestore,TNC_RESTORE);

        rfSendFiletoTNC(pRestore);
        delete[] pRestore;
        rfClose() ;
    }

    ShutDownServer = true;

    return ;
}

//---------------------------------------------------------------------
//
int serverConfig(const string& cf)
{
    const int maxToken=32;
    int nTokens ;
    char Line[256];
    string cmd;
    int n, m = 0;

    rfcall_idx = 0;
    posit_rfcall_idx = 0;
    stsmDest_rfcall_idx = 0;

    for (int i = 0; i < MAXRFCALL; i++) {
        rfcall[i] = NULL;               // clear the rfcall arrays
        posit_rfcall[i] = NULL;
        stsmDest_rfcall[i] = NULL;
    }

    cout << "Reading " << cf << endl << flush;

    ifstream file(cf.c_str());

    if (!file) {
        cerr << "Can't open " << cf << endl << flush;
        return(-1)  ;
    }

    do  {
        file.getline(Line,256);         // Read each line in file
        if (!file.good())
            break;

        n = 0;
        if (strlen(Line) > 0) {
            if (Line[0] != '#')  {      // Ignore comments
                string sLine(Line);
                string token[maxToken];
                nTokens = split(sLine, token, maxToken, RXwhite);   // Parse into tokens

                for (int i = 0 ; i < nTokens; i++)
                    cout << token[i] << " " ;
                    cout << endl << flush;
                    upcase(token[0]);
                    cmd = token[0];

                    if ((cmd.compare("TNCPORT") == 0) && (nTokens >= 2)) {
                        szComPort = strdup(token[1].c_str());
                        n = 1;
                    }

                    if ((cmd.compare("UDPPORT") == 0) && (nTokens >= 2)) {
                        upUdpServer.ServerPort = atoi(token[1].c_str());
                        n = 1;
                    }

                    if ((cmd.compare("TRUST") == 0) && (nTokens >= 2) && (nTrusted < maxTRUSTED)) {
                        int rc = inet_aton(token[1].c_str(), &Trusted[nTrusted].sin_addr);

                        if(nTokens >= 3)
                            inet_aton(token[2].c_str(), &Trusted[nTrusted].sin_mask);
                        else
                            Trusted[nTrusted].sin_mask.s_addr = 0xffffffff;

                        if (rc )
                            nTrusted++;
                        else
                            Trusted[nTrusted].sin_addr.s_addr = 0;

                        n = 1;
                    }

                    if ((cmd.compare("IGATE") == 0) || (cmd.compare("HUB") == 0)) {
                        cpIGATE[m].hub = (cmd.compare("HUB") == 0) ? true : false ;

                    cpIGATE[m].EchoMask = 0;    // default is to not send any data to other igates
                    cpIGATE[m].user = strdup(MyCall.c_str());   // default user is MyCall
                    cpIGATE[m].pass = (char*)"-1";     // default pass is -1
                    cpIGATE[m].RemoteSocket = 1313;     // Default remote port is 1313
                    cpIGATE[m].starttime = -1;
                    cpIGATE[m].lastActive = -1;
                    cpIGATE[m].pid = 0;
                    cpIGATE[m].remoteIgateInfo = NULL;

                    if (nTokens >= 2)
                        cpIGATE[m].RemoteName = strdup(token[1].c_str());   // remote domain name

                    if (nTokens >= 3)
                        cpIGATE[m].RemoteSocket = atoi(token[2].c_str());   // remote port number

                    if (nTokens >= 4)
                        cpIGATE[m].user = strdup(token[3].c_str());         // User name (call)

                    if (nTokens >= 5) {
                        cpIGATE[m].pass = strdup(token[4].c_str());         // Passcode
                        cpIGATE[m].EchoMask =  srcUSERVALID                 // If passcode present then we send out data
                                               + srcUSER                    // Same data as igate port 1313
                                               + srcTNC
                                               + srcUDP
                                               + srcSYSTEM;
                    }

                    if (m < maxIGATES)
                        m++;

                    nIGATES = m;
                    n = 1;
                }

                if (cmd.compare("LOCALPORT") == 0) {    // provides local TNC data only
                    spLocalServer.ServerPort = atoi(token[1].c_str());      // set server port number
                    spLocalServer.EchoMask =   srcUDP
                                                + srcTNC    // Set data sources to echo
                                                + srcSYSTEM
                                                + sendHISTORY;
                    n = 1;
                }

                if (cmd.compare("RAWTNCPORT") == 0) {       // provides local TNC data only
                    spRawTNCServer.ServerPort = atoi(token[1].c_str());     // set server port number
                    spRawTNCServer.EchoMask = srcTNC            // Set data sources to echo
                                                + sendDUPS      // No dup filtering
                                                + wantRAW;      // RAW data
                    n = 1;
                }

                if(cmd.compare("HTTPPORT") == 0) {      // provides server status in HTML format
                    spHTTPServer.ServerPort = atoi(token[1].c_str());   // set server port number
                    spHTTPServer.EchoMask = wantHTML;
                    n = 1;
                }

                if (cmd.compare("MAINPORT") == 0) {     // provides all data
                    spMainServer.ServerPort = atoi(token[1].c_str());   // set server port number
                    spMainServer.EchoMask = srcUSERVALID
                                                + srcUSER
                                                + srcIGATE
                                                + srcUDP
                                                + srcTNC
                                                + srcSYSTEM
                                                + sendHISTORY;
                    n = 1;
                }

                if (cmd.compare("MAINPORT-NH") == 0) {      // provides all data but no history dump
                    spMainServer_NH.ServerPort = atoi(token[1].c_str());    // set server port number
                    spMainServer_NH.EchoMask = srcUSERVALID
                                                + srcUSER
                                                + srcIGATE
                                                + srcUDP
                                                + srcTNC
                                                + srcSYSTEM;
                    n = 1;
                }

                if (cmd.compare("LINKPORT") == 0) {         // provides local TNC data + logged on users
                    spLinkServer.ServerPort = atoi(token[1].c_str());
                    spLinkServer.EchoMask = srcUSERVALID
                                                + srcUSER
                                                + srcTNC
                                                + srcUDP
                                                + srcSYSTEM;
                    n = 1;
                }

                if (cmd.compare("MSGPORT") == 0) {
                    spMsgServer.ServerPort  = atoi(token[1].c_str());
                    spMsgServer.EchoMask = src3RDPARTY + srcSYSTEM;
                    n = 1;
                }

                if (cmd.compare("IPWATCHPORT") == 0) {
                    spIPWatchServer.ServerPort = atoi(token[1].c_str());
                    spIPWatchServer.EchoMask = srcUSERVALID
                                                +  srcUSER
                                                +  srcIGATE
                                                +  srcTNC
                                                +  wantSRCHEADER;
                    n = 1;
                }

                if (cmd.compare("MYCALL") == 0) {   // This will be over-written by the MYCALL in INIT.TNC...
                                                    // ...if a TNC is being used.
                    MyCall = strdup(token[1].c_str());
                    if (/*strlen(MyCall)*/MyCall.length() > 9)
                        MyCall[9] = NULLCHR;   // Truncate to 9 chars.

                    n = 1;
                }

                if (cmd.compare("MYEMAIL") == 0) {
                    MyEmail = strdup(token[1].c_str());
                    n = 1;
                }

                if (cmd.compare("SERVERCALL") == 0) {
                    szServerCall = strdup(token[1].c_str());
                    if (szServerCall.length() > 9)
                        szServerCall[9] = NULLCHR;     // Truncate to 9 chars.

                    n = 1;
                }

                if (cmd.compare("APRSPATH") == 0) {
                    szAprsPath = (char*)malloc(BUFSIZE);
                    szAprsPath[0] = NULLCHR;

                    for (n = 1; n < nTokens; n++) {
                        strcat(szAprsPath, token[n].c_str());
                        strcat(szAprsPath, " ");
                    }

                    n = 1;
                }

                if (cmd.compare("MYLOCATION") == 0) {
                    MyLocation = strdup(token[1].c_str());
                    n = 1;
                }

                if (cmd.compare("MAXUSERS") == 0) {     // Set max users of server.
                    int mc = atoi(token[1].c_str());
                    if (mc > 0)
                        MaxClients = mc;

                    n = 1;
                }

                if (cmd.compare("EXPIRE") == 0) {       // Set time to live for history items (minutes)
                    int ttl = atoi(token[1].c_str());

                    if (ttl > 0)
                        ttlDefault = ttl;

                    n = 1;
                }

                if (cmd.compare("RF-ALLOW") == 0) {     // Allow internet to RF message passing.
                    upcase(token[1]);
                    if (token[1].compare("YES") == 0 )
                        RF_ALLOW = true;
                    else
                        RF_ALLOW = false;

                    n = 1;
                }

                if (cmd.compare("IGATEMYCALL") == 0) {  // Allow igating packets from "MyCall"
                    upcase(token[1]);
                    if (token[1].compare("YES") == 0 )
                        igateMyCall = true;
                    else
                        igateMyCall = false;

                    n = 1;
                }

                if (cmd.compare("LOGALLRF") == 0) {     // If "YES" then all packets heard are logged to rf.log"
                    upcase(token[1]);
                    if (token[1].compare("YES") == 0 )
                        logAllRF = true;
                    else
                        logAllRF = false;

                    n = 1;
                }

                if (cmd.compare("CONVERTMICE") == 0) {  // If "YES" then all MIC-E packets converted to classic APRS"
                    upcase(token[1]);
                    if (token[1].compare("YES") == 0 )
                        ConvertMicE = true;
                    else
                        ConvertMicE = false;

                    n = 1;
                }

                if (cmd.compare("GATE2RF") == 0) {      // Call signs of users always gated to RF
                    for (int i=1; i < nTokens;i++) {
                        string* s = new string(token[i]);
                        if (rfcall_idx < MAXRFCALL)
                            rfcall[rfcall_idx++] = s;   // add it to the list
                    }
                    n = 1;

                }

                if (cmd.compare("POSIT2RF") == 0) {     // Call sign posits gated to RF every 16 minutes
                    for (int i=1; i < nTokens;i++) {
                        string* s = new string(token[i]);
                        if (posit_rfcall_idx < MAXRFCALL)
                            posit_rfcall[posit_rfcall_idx++] = s;   // add it to the list
                    }
                    n = 1;

                }

                if (cmd.compare("MSGDEST2RF") == 0) {   // Destination call signs
                                                        // of station to station messages
                    for (int i=1; i < nTokens;i++) {    // always gated to RF
                        string* s = new string(token[i]);
                        if (stsmDest_rfcall_idx < MAXRFCALL)
                            stsmDest_rfcall[stsmDest_rfcall_idx++] = s;     // add it to the list
                    }
                    n = 1;

                }

                if (cmd.compare("ACKREPEATS") == 0) {   // How many extra ACKs to send to tnc
                    int mc = atoi(token[1].c_str());
                    if (mc < 0) {
                        mc = 0;
                        cout << "ACKREPEATS set to ZERO\n";
                    }

                    if (mc > 9) {
                        mc = 9;
                        cout << "ACKREPEATS set to 9\n";
                    }
                    ackRepeats = mc;
                    n = 1;
                }

                if (cmd.compare("ACKREPEATTIME") == 0) {    // Time in secs between extra ACKs
                    int mc = atoi(token[1].c_str());
                    if (mc < 1) {
                        mc = 1;
                        cout << "ACKREPEATTIME set to 1 sec.\n";
                    }

                    if (mc > 30) {
                        mc = 30;
                        cout << "ACKREPEATTIME set to 30 sec.\n";
                    }
                    ackRepeatTime = mc;
                    n = 1;
                }

                if (cmd.compare("NETBEACON") == 0) {        // Internet Beacon text
                    if (nTokens > 1) {
                        NetBeaconInterval = atoi(token[1].c_str()); // Set Beacon Interval in minutes

                        if (nTokens > 2) {
                            string s = token[2];

                            for (int i = 3 ; i<nTokens; i++)
                                s = s + " " + token[i] ;

                            NetBeacon = strdup(s.c_str());
                        }
                    }
                    n = 1;
                }

                if (cmd.compare("TNCBEACON") == 0) {    // TNC Beacon text
                    if (nTokens > 1) {
                        TncBeaconInterval = atoi(token[1].c_str());     // Set Beacon Interval in minutes

                        if (nTokens > 2) {
                            string s = token[2];

                            for (int i = 3 ; i<nTokens; i++)
                                s = s + " " + token[i] ;

                            TncBeacon = strdup(s.c_str());
                        }
                    }
                    n = 1;
                }

                if (cmd.compare("TNCPKTSPACING") == 0) {    // Set tnc packet spacing in ms
                    if (nTokens > 1)
                        tncPktSpacing = 1000 * atoi(token[1].c_str());  // ms to microsecond conversion

                    n = 1;
                }

                if (cmd.compare("APRSPASS") == 0) {         // Allow aprs style user passcodes for validation?
                    upcase(token[1]);

                    if (token[1].compare("YES") == 0 )
                        APRS_PASS_ALLOW = true;
                    else
                        APRS_PASS_ALLOW = false;

                    n = 1;
                }

                if (cmd.compare("RESPONDTOIGATEQUERIES") == 0) {
                    upcase(token[1]);

                    if (token[1].compare("YES") == 0)
                        respondToIgateQueries = true;
                    else
                        respondToIgateQueries = false;

                    n = 1;
                }

                if (cmd.compare("RESPONDTOAPRSDQUERIES") == 0) {
                    upcase(token[1]);

                    if (token[1].compare("YES") == 0)
                        respondToAprsdQueries = true;
                    else
                        respondToAprsdQueries = false;

                    n = 1;
                }

                if (cmd.compare("BROADCASTJAVAINFO") == 0) {
                    upcase(token[1]);

                    if (token[1].compare("YES") == 0)
                        broadcastJavaInfo = true;
                    else
                        broadcastJavaInfo = false;

                    n = 1;
                }

                if (cmd.compare("LOGBADPACKETS") == 0) {
                    upcase(token[1]);

                    if (token[1].compare("YES") == 0)
                        logBadPackets = true;
                    else
                        logBadPackets = false;

                    n = 1;
                }
                if (n == 0)
                    cout << "Unknown command: " << Line << endl << flush;
            }
        }
    } while(file.good());

    file.close();

    return(0);
}


//---------------------------------------------------------------------
// FOR DEBUGGING ONLY */
//
#ifdef DEBUG
void segvHandler(int signum)  //For debugging seg. faults
{
    pid_t pid; //,ppid;
    string /*char**/ err;

    pid = getpid();

    if (pid == pidlist.main)
        err = "aprsd main";

    if (pid == spMainServer.pid)
        err = "spMainServer";

    if (pid == spMainServer_NH.pid)
        err = "spMainServer_NH";

    if (pid == spLocalServer.pid)
        err = "spLocalServer";

    if (pid == spLinkServer.pid)
        err = "spLinkServer" ;

    if (pid == spMsgServer.pid)
        err = "spMsgServer";

    if (pid == upUdpServer.pid)
        err = "upUdpServer";

    if (pid == pidlist.SerialInp)
        err = "Serial input thread";

    if (pid == pidlist.TncQueue)
        err = "TNC Dequeue";

    if (pid == pidlist.InetQueue)
        err = "Internet Dequeue";

    char buf[256];
    memset(buf,0,256);
    ostrstream sout(buf,255);
    sout << "A segment violation (" << signum << ") has occurred in process id "
         << pid
         << " Thread: "
         << err
         << endl ;

    char buf2[256];
    memset(buf2,0,256);
    ostrstream sout2(buf2,255);

    sout2 << "Died in "
         << DBstring
         << " Last packet:  " << lastPacket->c_str()
         << " Packet length = " << lastPacket->length()
         << endl
         << ends;

    cout << buf << buf2;
    WriteLog(buf,"segfault.log");
    WriteLog(buf2,"segfault.log");

    exit(-1);
}
#endif
//----------------------------------------------------------------------
/* Thread to process http request for server status data */
 void *HTTPServerThread(void *p)
{
    char buf[BUFSIZE];
    SessionParams *psp = (SessionParams*)p;
    int sock = psp->Socket;
    delete psp;
    int  i;
    char  szError[MAX];
    int n, rc,data;
    int err,nTokens;
    char *htmlbuf = NULL;
    time_t localtime;
    char szTime[64];
    struct tm *gmt = NULL;
    double serverRate = 0;
    double inetRate = 0;
    string inetRateX, serverRateX;

    if (sock < 0)
        pthread_exit(0);

    if(pthread_mutex_lock(pmtxCount) != 0)
        cerr << "Unable to lock pmtxCount - HTTPStats1.\n" << flush;

    webCounter++ ;

    if (aprsStreamRate > 1024) {
        inetRate = ((double)aprsStreamRate / 1024);
        inetRateX = "Kbps";
        if (inetRate > 1000) {
            inetRate = (inetRate / 1000);
            inetRateX = "Mbps";
        }
    } else {
        inetRate = (double)aprsStreamRate;
        inetRateX = "Bps";
    }

    if (serverLoad > 1024) {
        serverRate = ((double)serverLoad / 1024);
        serverRateX = "Kbps";
        if (serverRate > 1000) {
            serverRate = (serverRate / 1000);
            serverRateX = "Mbps";
        }
    } else {
        serverRate = (double)serverLoad;
        serverRateX = "Bps";
    }
    if(pthread_mutex_unlock(pmtxCount) != 0)
        cerr << "Unable to unlock pmtxCount - HTTPStats1.\n" << flush;

    gmt = new tm;
    time(&localtime);
    gmt = gmtime_r(&localtime,gmt);
    strftime(szTime,64,"%a, %d %b %Y %H:%M:%S GMT",gmt);    // Date in RFC 1123 format
    delete gmt;

    data = 1;                           // Set socket for non-blocking
    ioctl(sock,FIONBIO,(char*)&data,sizeof(int));

    rc = recvline(sock,buf,BUFSIZE,&err, 10);  //10 sec timeout value

    if (rc<=0) {
        close(sock);
        pthread_exit(0);
    }

    string sLine(buf);
    string token[4];
    nTokens = split(sLine, token, 4, RXwhite);      // Parse http request into tokens

    //for(int i = 0 ;i< 4 ;i++) cout << token[i] << " " ;  //debug
    // cout << endl << flush;

    char buf2[127];

    do {
        n = recvline(sock,buf2,126,&err, 1);        // Discard everything else
    } while (n > 0);

    if (n == -2) {
        close(sock);
        pthread_exit(0);
    }

    if ((token[0].compare("GET") != 0) || (token[1].compare("/") != 0)) {
        strcpy(szError,"HTTP/1.0 404 Not Found\nContent-Type: text/html\n\n<HTML><BODY>File not found</BODY></HTML>\n");
        send(sock,szError,strlen(szError),0);
        close(sock);
        pthread_exit(0);
    }

#define HTMLSIZE 5000
    if(pthread_mutex_lock(pmtxCount) != 0)
        cerr << "Unable to lock pmtxCount - HTTPStats2.\n" << flush;

    htmlbuf = new char[HTMLSIZE];
    htmlbuf[HTMLSIZE-1] = '\0';   //Set last byte in buffer to null
    ostrstream stats(htmlbuf,HTMLSIZE-1);

    stats << setiosflags(ios::showpoint | ios::fixed)
        << setprecision(1)
        << "HTTP/1.0 200 OK\n"
        << "Date: " << szTime << "\n"
        << "Server: aprsd\n"
        << "MIME-version: 1.0\n"
        << "Content-type: text/html\n"
        << "Expires: " << szTime << "\n"
        << "Refresh: 300\n"             // uncomment to activate 5 minute refresh time
        << "\n<HTML>"
        << "<HEAD><TITLE>" << szServerCall << " Server Status Report</TITLE></HEAD>"
        << "<BODY ALINK=#0000FF VLINK=#800080 ALINK=#FF0000 BGCOLOR=\"#606060\"><CENTER>"
        << "<TABLE BORDER=2 BGCOLOR=\"#D0D0D0\">"
        << "<TR BGCOLOR=\"#FFD700\">"
        << "<TH COLSPAN=2>" << szServerCall << " " << MyLocation << "</TH>"
        << "</TR>"
        << "<TR ALIGN=center><TD COLSPAN=2>" << szTime << "</TD></TR>\n"
        << "<TR><TD>Server up time</TD><TD>" << upTime << " hours</TD></TR>\n"
        << "<TR><TD>Users</TD><TD>" << ConnectedClients << "</TD></TR>\n"
        << "<TR><TD>Peak Users</TD><TD>" << MaxConnects << "</TD></TR>\n"
        << "<TR><TD>Max User Limit</TD><TD>" << MaxClients << "</TD></TR>\n"
        << "<TR><TD>Connect count</TD><TD>" << TotalConnects << "</TD></TR>\n"
        << "<TR><TD>TNC Packets</TD><TD>" << TotalLines << "</TD></TR>\n"
        << "<TR><TD>TNC Stream Rate</TD><TD>" << tncStreamRate << " bytes/sec</TD></TR>\n"
        << "<TR><TD>Msgs gated to RF</TD><TD>" << msg_cnt << "</TD></TR>\n"
        << "<TR><TD>APRS stream rate</TD><TD>" << inetRate << " " << inetRateX << "</TD></TR>\n"
        << "<TR><TD>Server Load</TD><TD>" << serverRate << " " << serverRateX << "</TD></TR>\n"
        << "<TR><TD>History Time Limit</TD><TD>" << ttlDefault << " min.</TD></TR>\n"
        << "<TR><TD>History items</TD><TD>" << ItemCount  << "</TD></TR>\n"
        << "<TR><TD>TAprsString Objects</TD><TD>" << TAprsString::getObjCount() << "</TD></TR>\n"
        << "<TR><TD>Frame Count</TD><TD>" << frame_cnt << "</TD></TR>\n"
        << (logBadPackets ? "<TR><TD><a href=\"http://first.aprs.net/aprsd/badpacket.log\">Frame Errors</a></TD><TD>" :
                "<TR><TD>Frame Errors</TD><TD>") << error_cnt << "</TD></TR>\n"
        << "<TR><TD>Items in InetQ</TD><TD>" << sendQueue.getItemsQueued() << "</TD></TR>\n"
        << "<TR><TD>InetQ overflows</TD><TD>" << sendQueue.overrun << "</TD></TR>\n"
        << "<TR><TD>TncQ overflows</TD><TD>" << tncQueue.overrun << "</TD></TR>\n"
        << "<TR><TD>ConQ overflows</TD><TD>" << conQueue.overrun << "</TD></TR>\n"
        << "<TR><TD>charQ overflows</TD><TD>" << charQueue.overrun << "</TD></TR>\n"
        << "<TR><TD>History dump aborts</TD><TD>" << dumpAborts << "</TD></TR>\n"
        << "<TR><TD>HTTP Access Counter</TD><TD>" << webCounter << "</TD></TR>\n"
        << "<TR><TD>?IGATE? Querys</TD><TD>" << queryCounter << "</TD></TR>\n"
        << "<TR><TD>Server Version</TD><TD>" << VERS << "</TD></TR>\n"
        << "<TR><TD>Sysop email</TD><TD><A HREF=\"mailto:" << MyEmail << "\">" << MyEmail << "</A></TD></TR>\n"
        << "</TABLE><P>"
        << ends;

    

    if(pthread_mutex_unlock(pmtxCount) != 0)
        cerr << "Unable to unlock pmtxCount - HTTPStats2.\n" << flush;

    rc = send(sock,htmlbuf,strlen(htmlbuf),0);  //Send this part of the page now
    

    // Now send the Igate connection report.

    string igateheader =

               "</TABLE><P><TABLE BORDER=2 BGCOLOR=\"#C0C0C0\"><TR BGCOLOR=\"#FFD700\">"
               "<TH COLSPAN=10>Igate Connections</TH></TR>"
               "<TR><TH>Domain Name</TH><TH>Port</TH><TH>Type</TH><TH>Status</TH><TH>Igate Pgm</TH>"
               "<TH>Last active<BR>H:M:S</TH><TH>Bytes<BR> In</TH><TH>Bytes<BR> Out</TH><TH>Time<BR> H:M:S</TH><TH>PID</TH></TR>" ;

    rc = send(sock,igateheader.c_str(), igateheader.length(), 0);

    if(pthread_mutex_lock(pmtxCount) != 0)
        cerr << "Unable to lock pmtxCount - HTTPStats3.\n" << flush;

    for (i=0; i< nIGATES;i++) {
        if (cpIGATE[i].RemoteSocket != -1) {
            char timeStr[32];
            strElapsedTime(cpIGATE[i].starttime,timeStr);           // Compute elapsed time of connection
            char lastActiveTime[32];
            strElapsedTime(cpIGATE[i].lastActive,lastActiveTime);   // Compute time since last input char


            ostrstream igateinfo(htmlbuf, HTMLSIZE-1);
            //char *status, *bgcolor, *conType;
            string status, bgcolor, conType;

            if (cpIGATE[i].hub) {
                if (cpIGATE[i].connected) {
                    status = "UP";
                    bgcolor = "\"#C0C0C0\"";
                } else {
                    status = "N/C" ;
                    bgcolor = "\"#909090\"";
                }
                conType = "HUB";
            } else {
                if (cpIGATE[i].connected) {
                    status = "UP";
                    bgcolor = "\"#C0C0C0\"";
                } else {
                    status = "DOWN" ;
                    bgcolor = "\"#F07070\"";
                }
                conType = "IGATE";
            }

            string infoTokens[3];
            infoTokens[1] = "*";
            infoTokens[2] = "";
            int ntok = 0;

            if (cpIGATE[i].remoteIgateInfo) {
                string rii(cpIGATE[i].remoteIgateInfo);

                if (rii[0] == '#') {
                    ntok = split(rii, infoTokens, 3, RXwhite);      // Parse into tokens if first char was "#".
                }                       // Token 1 is remote igate program name and token 2 is the version number.
            }

            igateinfo << "<TR ALIGN=center BGCOLOR=" << bgcolor << "><TD>"
                <<  "<A HREF=\"http://"
                << cpIGATE[i].RemoteName
                << ":14501/\">" << cpIGATE[i].RemoteName
                <<  "</A></TD>"
                << "<TD>" << cpIGATE[i].RemoteSocket << "</TD>"
                << "<TD>" << conType << "</TD>"
                << "<TD>" << status << "</TD>"
                << "<TD>" << infoTokens[1] << " " << infoTokens[2] << "</TD>"
                << "<TD>" << lastActiveTime << "</TD>"
                << "<TD>"  << cpIGATE[i].bytesIn / 1024 << " K</TD>"
                << "<TD>" << cpIGATE[i].bytesOut / 1024 << " K</TD>"
                << "<TD>" << timeStr << "</TD>"
                << "<TD>" << cpIGATE[i].pid << "</TD>"
                << "</TR>\n"
                << ends;

            rc = send(sock,htmlbuf,strlen(htmlbuf),0);
        }
    }
    if(pthread_mutex_unlock(pmtxCount) != 0)
        cerr << "Unable to unlock pmtxCount - HTTPStats3.\n" << flush;

    string userheader =
        "</TABLE><P><TABLE  BORDER=2 BGCOLOR=\"#C0C0C0\">"       // Start of user list table
        "<TR BGCOLOR=\"#FFD700\"><TH COLSPAN=10>Users</TH></TR>\n"
        "<TR><TH>IP Address</TH><TH>Port</TH><TH>Call</TH><TH>Vrfy</TH>"
        "<TH>Program Vers</TH><TH>Last Active<BR>H:M:S</TH><TH>Bytes<BR> In</TH><TH>Bytes <BR>Out</TH><TH>Time<BR> H:M:S</TH><TH>PID</TH></TR>\n" ;

    rc = send(sock, userheader.c_str(), userheader.length(), 0);

    if(pthread_mutex_lock(pmtxAddDelSess) != 0)		// comment this out to allow viewing if mutex is locked
        cerr << "Unable to lock pmtxAddDelSess HTTPStats4- .\n" << flush;

    if(pthread_mutex_lock(pmtxCount) != 0)
        cerr << "Unable to lock pmtxCount - HTTPStats4.\n" << flush;

    string TszPeer;
    string TserverPort;
    string TuserCall;
    string TpgmVers;
    string TlastActive;
    string TtimeStr;
    //string Tpid;
    int bytesout = 0;
    int bytesin = 0;
    int npid = -1;
    for (i=0;i<MaxClients;i++) {        // Create a table with user information
        if ((sessions[i].Socket != -1) && (sessions[i].ServerPort != -1)) {
            char timeStr[32];
            strElapsedTime(sessions[i].starttime, timeStr);      // Compute elapsed time
            char lastActiveTime[32];
            strElapsedTime(sessions[i].lastActive, lastActiveTime);  // Compute elapsed time from last input char
            //char* szVrfy;
            string szVrfy;
            TszPeer = sessions[i].szPeer;
            //TserverPort = sessions[i].ServerPort;
            TuserCall = sessions[i].userCall;
            TpgmVers = sessions[i].pgmVers;
            TlastActive = lastActiveTime;
            TtimeStr = timeStr;

            removeHTML(TszPeer);
            //removeHTML(TserverPort);
            removeHTML(TuserCall);
            removeHTML(TpgmVers);
            removeHTML(TlastActive);
            removeHTML(TtimeStr);

            if (sessions[i].vrfy)
                szVrfy = "YES";
            else
                szVrfy = "NO";

            if (sessions[i].bytesOut > 0)
                bytesout = (sessions[i].bytesOut / 1024);

            if (sessions[i].bytesIn > 0)
                bytesin = (sessions[i].bytesIn / 1024);

            if (sessions[i].pid > 0)
                npid = sessions[i].pid;

            ostrstream userinfo(htmlbuf,HTMLSIZE-1);

            userinfo << "<TR ALIGN=center><TD>"
                     << "<A HREF=\"http://"
                     << TszPeer.c_str() /*sessions[i].szPeer*/
                     << ":14501/\">" << TszPeer.c_str() /*sessions[i].szPeer*/
                     << "</A></TD>"
                     << "<TD>" << sessions[i].ServerPort << "</TD>"
                     << "<TD>" << TuserCall.c_str() /*sessions[i].userCall*/ << "</TD>"
                     << "<TD>"  << szVrfy << "</TD>"
                     << "<TD>" << TpgmVers.c_str() /*sessions[i].pgmVers*/ << "</TD>"
                     << "<TD>" << TlastActive.c_str() /*lastActiveTime*/ << "</TD>"
                     << "<TD>" << bytesin << " K</TD>"
                     << "<TD>" << bytesout << " K</TD>"
                     << "<TD>" << TtimeStr.c_str() /*timeStr*/ << "</TD>"
                     << "<TD>" << npid << "</TD>"
                     << "</TR>" << endl
                     << ends ;
            rc = send(sock, htmlbuf, strlen(htmlbuf), 0);
        }
    }
    if(pthread_mutex_unlock(pmtxCount) != 0)
        cerr << "Unable to unlock pmtxCount - HTTPStats4.\n" << flush;

    if(pthread_mutex_unlock(pmtxAddDelSess) != 0)
        cerr << "Unable to unlock pmtxAddDelSess - HTTPStats4.\n" << flush;

    string endpage = "</TABLE></CENTER></BODY></HTML>";
    rc = send(sock, endpage.c_str(), endpage.length(), 0);

    close(sock);

    if (htmlbuf != NULL)
        delete[] htmlbuf;

    pthread_exit(0);
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
    TAprsString* abuff;

    if (difftime(t,last_t) < 14)
        return;                         // return if not time yet (14 seconds)

    last_t = t;

    if (posit_rfcall[ptr] != NULL) {

        // Get a position which is older than 15 minutes and update it to the current time
        abuff = getPositAndUpdate(*posit_rfcall[ptr] , srcIGATE | srcUSERVALID | srcUSER, t - (15 * 60), t);

        if (abuff) {
            cout << "Found position ready for tx: " << abuff << endl;
            abuff->stsmReformat(MyCall);    // Convert to 3rd party format
            tncQueue.write(abuff);          // Send to TNC

        } else
            ptr++;                        // Next time try the next callsign entry

    } else
        ptr++;                              // point to next call sign

    if (ptr >= posit_rfcall_idx)
        ptr = 0;                        // wrap around if past end of array

}

//----------------------------------------------------------------------


int daemonInit(void)
{
    pid_t pid;
    char pid_file[] = "/var/run/aprsd.pid";
    char s[25];
    pid_t xx;
    FILE *fp, *f;

   // Check for pre-existing aprsd
   f = fopen(pid_file, "r");
   if (f != NULL) {
      fgets(s, 10, f);
      xx = atoi(s);
      kill(xx, SIGCHLD);                // Meaningless kill to determine if pid is used
      if (errno != ESRCH) {
         cout << "aprsd already running" << endl;
         cout << "PID: " << pid_file << endl;
         exit(1);
      }
   }
   if ((pid = fork()) < 0)
        return -1 ;
    else if (pid != 0)
        exit(0);                        // Parent goes away

    // child continues
    unlink(pid_file);
    xx = getpid();
    fp = fopen(pid_file, "w");
    printf("pid %i", xx);
    if (fp != NULL) {
        fprintf(fp, "%u\n", xx);
        if (fflush(fp)) {
           // Let aprsd live since this doesn't appear to be a chkaprsd
           printf("Warning, could not write %s file!", pid_file);
           fclose(fp);
           unlink(pid_file);
        } else
          fclose(fp);
    }

    setpgid(0,0);
    freopen("/dev/null", "r", stdin);   // redirect output
    freopen("/dev/null", "w", stdout);
    freopen("/dev/null", "w", stderr);

    setsid();                           // Become session leader
    chdir(HOMEDIR);                     // change to the aprsd2 directory
    umask(0);                           // Clear our file mode selection mask

    return(0);
}



//----------------------------------------------------------------------
int main(int argc, char *argv[])
{
    int i,rc;
    char *pSaveHistory /* ,*szConfFile*/;
    string szConfFile;
    time_t lastSec,tNow,tLast,tLastDel, tPstats;
    time_t LastNetBeacon , LastTncBeacon;
    time_t Time = time(NULL);
    serverStartTime = Time;

    TotalConnects = TotalTncChars = TotalLines = MaxConnects = TotalIgateChars = 0;
    TotalUserChars = 0;
    bytesSent = 0;
    TotalTNCtxChars = 0;
    msg_cnt = 0;
    posit_cnt = 0;
    MyCall = "N0CALL";
    MyLocation = "NoWhere";
    MyEmail = "nobody@NoWhere.net";
    TncBeacon = NULLCHR;
    NetBeacon = NULLCHR;
    TncBeaconInterval = 0;
    NetBeaconInterval = 0;
    tncPktSpacing = 1500000;            // 1.5 second default
    LastNetBeacon = 0;
    LastTncBeacon = 0;
    igateMyCall = true;                 // To be compatible with previous versions set it TRUE
    tncPresent = false;
    logAllRF = false;
    ConvertMicE = false;
    tncMute = false;
    MaxClients = MAXCLIENTS;            // Set default aprsd.conf file will override this
    respondToIgateQueries = false;
    respondToAprsdQueries = true;
    broadcastJavaInfo = false;
    logBadPackets = false;

    ackRepeats = 2;                     // Default extra acks to TNC
    ackRepeatTime = 5;                  // Default time between extra acks to TNC in seconds.
    msgsn = 0;                          // Clear message serial number
    APRS_PASS_ALLOW = true;             // Default allow aprs style user passcodes
    webCounter = 0;
    queryCounter = 0;
    error_cnt = 0;
    frame_cnt = 0;

    spLinkServer.ServerPort = 0;        // Ports are set in aprsd.conf file
    spMainServer.ServerPort = 0;
    spMainServer_NH.ServerPort = 0;
    spLocalServer.ServerPort = 0;
    spRawTNCServer.ServerPort = 0;
    spMsgServer.ServerPort = 0;
    upUdpServer.ServerPort = 0;

    spHTTPServer.ServerPort = 14501;    // HTTP monitor port default
    spHTTPServer.EchoMask = wantHTML;

    spIPWatchServer.ServerPort = 14502; // IP Watch port default
    spIPWatchServer.EchoMask = srcUSERVALID
                              +  srcUSER
                              +  srcIGATE
                              +  srcTNC
                              +  wantSRCHEADER;
    struct sigaction sa;


    if (argc > 1)
        if (strcmp("-d",argv[1]) == 0)
            daemonInit();               // option -d means run as daemon

    signal(SIGPIPE,SIG_IGN);
    signal(SIGXCPU,SIG_IGN);
    signal(SIGTSTP,SIG_IGN);
    signal(SIGSTOP,SIG_IGN);

    pidlist.main = getpid();

#ifdef DEBUG
    memset(&sa, NULLCHR, sizeof(sa));
    sa.sa_handler = segvHandler;
    if (sigaction(SIGSEGV, &sa, NULL))
        perror("sigaction");
#endif

    configComplete = false;
    szComPort = NULL;                   // null string for TNC com port
    szAprsPath = NULL;

    szServerCall = "aprsd";             // default server FROM CALL used in system generated pkts.

    szAPRSDPATH = new char[64];
    memset(szAPRSDPATH, NULLCHR, 64);
    strcpy(szAPRSDPATH,">");
    strncat(szAPRSDPATH,PGVERS,64);
    strncat(szAPRSDPATH,",TCPIP*:",64); // ">APD215,TCPIP*:"

    ShutDownServer = false;

    szConfFile = CONFPATH;
    szConfFile += CONFFILE;             // default server configuration file

    CreateHistoryList();                // Make the history linked list structure

    cout << SIGNON << endl << flush;

    /*   //DEBUG & TEST CODE
   //TAprsString mic_e("K4HG-9>RU4U9T,WIDE,WIDE,WIDE:`l'q#R>/\r\n");
   TAprsString mic_e("K4HG-9>RU4W5S,WIDE,WIDE,WIDE:`l(XnUU>/Steve's RX-7/Mic-E\r\n");
   TAprsString* posit = NULL;
   TAprsString* telemetry = NULL;
   if(mic_e.aprsType == APRSMIC_E) mic_e.mic_e_Reformat(&posit,&telemetry);
   if(posit){ posit->print(cout); delete posit;}
   if(telemetry) { telemetry->print(cout); delete telemetry;}

   sleep(5);
   */

   //fdump = fopen("dump.txt","w+");  //debug

    pSaveHistory = new char[strlen(VARPATH) + strlen(SAVE_HISTORY)+1];
    strcpy(pSaveHistory,VARPATH);
    strcat(pSaveHistory,SAVE_HISTORY);

    ReadHistory(pSaveHistory);

    if (argc > 1) {
        if (strcmp("-d",argv[argc-1]) != 0) {
            szConfFile = new char[MAX];
            szConfFile = argv[argc-1];  // get optional 1st or 2nd arg which is configuration file name
        }
    }

    for (i=0;i<maxIGATES;i++) {
        cpIGATE[i].RemoteSocket = -1;
        cpIGATE[i].RemoteName = NULL;
    }

    if (serverConfig(szConfFile) != 0)
        exit(-1);                       // Read configuration file (aprsd.conf)

    //Now add a ax25 path to the Internet beacon text string

    if (!NetBeacon.empty()) {
        string netbc(szServerCall);
        netbc.reserve(256);
        netbc.append(szAPRSDPATH);
        netbc.append(NetBeacon);
        netbc.append("\r\n");
        NetBeacon = netbc;              // Internet beacon complete with ax25 path

    }

    if (!TncBeacon.empty()) {
        string tncbc = TncBeacon;
        tncbc.reserve(256);
        tncbc.append("\r\n");
        TncBeacon = tncbc;              // TNC beacon (no ax25 path)

    }

    // Make the semaphores
    pmtxSendFile = new pthread_mutex_t;
    pmtxSend = new pthread_mutex_t;
    pmtxAddDelSess = new pthread_mutex_t;
    pmtxCount = new pthread_mutex_t;
    pmtxDNS = new pthread_mutex_t;

    if ((rc = pthread_mutex_init(pmtxSendFile, NULL)) == -1) {
        cerr << "pthread_mutex_init error: SendFile\n";
        exit(2);
    }

    if ((rc = pthread_mutex_init(pmtxSend, NULL)) == -1) {
        cerr << "pthread_mutex_init error: Send\n";
        exit(2);
    }

    if ((rc = pthread_mutex_init(pmtxAddDelSess, NULL)) == -1) {
        cerr << "pthread_mutex_init error: AddDelSess\n";
        exit(2);
    }

    if ((rc = pthread_mutex_init(pmtxCount, NULL)) == -1) {
        cerr << "pthread_mutex_init error: Count\n";
        exit(2);
    }

    if ((rc = pthread_mutex_init(pmtxDNS, NULL)) == -1) {
        cerr << "pthread_mutex_init error: DNS\n";
        exit(2);
    }

    /*
    rc = pthread_mutex_init(pmtxSendFile,NULL);
    rc = pthread_mutex_init(pmtxSend,NULL);
    rc = pthread_mutex_init(pmtxAddDelSess,NULL);
    rc = pthread_mutex_init(pmtxCount,NULL);
    rc = pthread_mutex_init(pmtxDNS,NULL);
    */

    sessions = new SessionParams[MaxClients];

    if (sessions == NULL) {
        cerr << "Can't create sessions pointer\n" ;
        return(-1);
    }

    for (i = 0; i < MaxClients; i++) {
        sessions[i].Socket = -1;
        sessions[i].EchoMask = 0;
        sessions[i].szPeer = new char[SZPEERSIZE];
        sessions[i].userCall = new char[USERCALLSIZE];
        sessions[i].pgmVers = new char[PGMVERSSIZE];

        memset((void*)sessions[i].szPeer, NULLCHR, SZPEERSIZE);
        memset((void*)sessions[i].userCall, NULLCHR, USERCALLSIZE);
        memset((void*)sessions[i].pgmVers, NULLCHR, PGMVERSSIZE);
    }

    ConnectedClients = 0;

    if (spMainServer.ServerPort > 0) {
        //Create Main Server thread. (Provides Local data, Logged on users and IGATE data)
        if (pthread_create(&spMainServer.tid, NULL,TCPServerThread,&spMainServer) == -1) {
            cerr << "Error: Main TCPServerThread failed to start\n";
            exit(2);
        }
    }

    if (spMainServer_NH.ServerPort > 0) {
        // Create another Main Server thread .
        // (Provides Local data, Logged on users and IGATE data but doesn't dump the 30 min. history)
        if (pthread_create(&spMainServer_NH.tid, NULL,TCPServerThread,&spMainServer_NH) == -1) {
            cerr << "Error: Main-NH TCPServerThread failed to start\n";
            exit(2);
        }
    }

    if (spLinkServer.ServerPort > 0) {
        // Create Link Server thread.  (Provides local TNC data plus logged on users data)
        if (pthread_create(&spLinkServer.tid, NULL,TCPServerThread,&spLinkServer) == 1) {
            cerr << "Error: Link TCPServerThread failed to start\n";
            exit(2);
        }
    }

    if (spLocalServer.ServerPort > 0) {
        // Create Local Server thread  (Provides only local TNC data).
        if (pthread_create(&spLocalServer.tid, NULL,TCPServerThread,&spLocalServer) == -1) {
            cerr << "Error: Local TCPServerThread failed to start\n";
            exit(2);
        }
    }

    if (spRawTNCServer.ServerPort > 0) {
        // Create Local Server thread  (Provides only local TNC data).
        if (pthread_create(&spRawTNCServer.tid, NULL,TCPServerThread,&spRawTNCServer) == -1) {
            cerr << "Error: RAW TNC TCPServerThread failed to start\n";
            exit(2);
        }
    }

    if (spMsgServer.ServerPort > 0) {
        // Create message Server thread  (Provides only 3rd party message data).
        if (pthread_create(&spMsgServer.tid, NULL,TCPServerThread,&spMsgServer) == -1) {
            cerr << "Error: 3rd party message TCPServerThread failed to start\n";
            exit(2);
        }
    }

    if (upUdpServer.ServerPort > 0) {
        if (pthread_create(&upUdpServer.tid, NULL,UDPServerThread,&upUdpServer) == -1) {
            cerr << "Error: UDP Server thread failed to start\n";
            exit(2);
        }
    }

    if (spHTTPServer.ServerPort > 0) {
        // Create HTTP Server thread. (Provides server status in HTML format)
        if (pthread_create(&spHTTPServer.tid, NULL,TCPServerThread,&spHTTPServer) == -1) {
            cerr << "Error: HTTP server thread failed to start\n";
            exit(2);
        }
    }

    if (spIPWatchServer.ServerPort > 0) {
        // Create IPWatch Server thread. (Provides prepended header with IP and User Call on aprs packets)
        if (pthread_create(&spIPWatchServer.tid, NULL,TCPServerThread,&spIPWatchServer) == -1) {
            cerr << "Error: IPWatch server thread failed to start\n";
            exit(2);
        }
    }

    pthread_t  tidDeQueuethread;
    if (pthread_create(&tidDeQueuethread, NULL,DeQueue,NULL) == -1) {
        cerr << "Error: DeQueue thread failed to start\n";
        exit(2);
    }

    if (szAprsPath) {
        cout << "APRS packet path = " << szAprsPath << endl;
        rfSetPath(szAprsPath);
    }

    if (szComPort != NULL) {            // Initialize TNC Com port if specified in config file
        cout  << "Opening device "
            << szComPort
            << endl
            << flush;

        if ((rc = rfOpen(szComPort)) != 0) {
            sleep(2);
            return(-1);
        }

        cout << "Setting up TNC\n" << flush;

        char *pInitTNC = new char[strlen(CONFPATH) + strlen(TNC_INIT) +1];
        strcpy(pInitTNC,CONFPATH);
        strcat(pInitTNC,TNC_INIT);

        rfSendFiletoTNC(pInitTNC);      // Setup TNC from initialization file
        tncPresent = true;
        delete (char*)pInitTNC;
    } else
        cout << "TNC com port not defined.\n" << flush;

    if (RF_ALLOW)
        cout << "Internet to RF data flow is ENABLED\n" ;
    else
        cout << "Internet to RF data flow is DISABLED\n";

    WriteLog("Server Start",MAINLOG);
    cout << "Server Started\n" << flush;

    bool firstHub = false;

    if (nIGATES > 0)
        cout << "Connecting to IGATEs and Hubs now..." << endl << flush;

    for (i=0;i<nIGATES;i++) {
        if (!firstHub) {
            //rc = pthread_create(&cpIGATE[i].tid, NULL,TCPConnectThread,&cpIGATE[i]);
            //if (rc == 0)
            //    pthread_detach(cpIGATE[i].tid);

            //if (cpIGATE[i].hub)
            //    firstHub = true;

            if (pthread_create(&cpIGATE[i].tid, NULL, TCPConnectThread, &cpIGATE[i]) == 0)
                pthread_detach(cpIGATE[i].tid);

            if (cpIGATE[i].hub)
                firstHub = true;

        } else {
            if (!cpIGATE[i].hub) {
                //rc = pthread_create(&cpIGATE[i].tid, NULL,TCPConnectThread,&cpIGATE[i]);

                //if (rc == 0)
                //    pthread_detach(cpIGATE[i].tid);
                if (pthread_create(&cpIGATE[i].tid, NULL, TCPConnectThread, &cpIGATE[i]) == 0)
                    pthread_detach(cpIGATE[i].tid);
            }
        }
    }

    Time = time(NULL);
    tNow = Time;
    tLast = Time;
    tLastDel = Time ;
    tPstats = Time;

    configComplete = true;

    if (!TncBeacon.empty())
        cout << "TncBeacon every " << TncBeaconInterval << " minutes: " << TncBeacon << endl;

    if (!NetBeacon.empty())
        cout << "NetBeacon every " << NetBeaconInterval << " minutes: " << NetBeacon << endl;

    cout << "MYCALL set to: " << MyCall << endl;

    struct termios initial_settings, new_settings;
    tcgetattr(fileno(stdin),&initial_settings);
    new_settings = initial_settings;
    new_settings.c_lflag &= ~ICANON;
    new_settings.c_lflag &= ~ISIG;
    new_settings.c_cc[VMIN] = 0;
    new_settings.c_cc[VTIME] = 1;		 //.1 second timeout for input chars

    tcsetattr(fileno(stdin),TCSANOW,&new_settings);

    do {
        reliable_usleep(1000);

        if (msgsn > 9999)
            msgsn = 0;

        while (conQueue.ready()) {      // Data for Console?
            char *cp = (char*)conQueue.read(NULL);    //Yes, read and print it.

            if (cp) {
                printf("%s",cp);
                strcat(cp,"\r");
                TAprsString* monStats = new TAprsString(cp,SRC_INTERNAL,srcSTATS);
                sendQueue.write(monStats);

                delete cp;
            }
        }

        char ch = fgetc(stdin);         // stalls for 0.1 sec.

        switch (ch) {
            case  'r':
                resetCounters();
                break;

            case 0x03:

            case 'q' :
                serverQuit(&initial_settings);
                raise(SIGTERM);;
        }

        lastSec = Time;
        Time = time(NULL);

        if (difftime(Time,lastSec) > 0)
            schedule_posit2RF(Time);    // Once per second

        if (difftime(Time,LastNetBeacon) >= NetBeaconInterval * 60) {   //S end Internet Beacon text
            LastNetBeacon = Time;

            if ((!NetBeacon.empty()) && (NetBeaconInterval > 0)){
                TAprsString* netbc = new TAprsString(NetBeacon,SRC_INTERNAL,srcSYSTEM);
                sendQueue.write(netbc);
            }
        }

        if (difftime(Time,LastTncBeacon) >= TncBeaconInterval * 60) {   // Send TNC Beacon text
            LastTncBeacon = Time;

            if ((!TncBeacon.empty()) && (TncBeaconInterval > 0)) {
                TAprsString* tncbc = new TAprsString(TncBeacon,SRC_INTERNAL,srcSYSTEM);
                tncQueue.write(tncbc);
            }
        }

        /*debug*/
   /*
    if (Time != lastSec)	  //send the test message for debugging
     {
          char *test = "W4ZZ>APRS,TCPIP:!BOGUS PACKET ";
            char testbuf[256];
            ostrstream os(testbuf,255);
            os << test << Time << "\r\n" << ends;
            TAprsString* inetpacket = new TAprsString(testbuf,0,srcTNC);
            inetpacket->changePath("TCPIP*","TCPIP") ;
            tncQueue.write(inetpacket); //note: inetpacket is deleted in DeQueue

     }
    */

        if ((Time - tPstats) > 60) {    // 1 minute
            if (WatchDog < 2)
                cerr << "** No data from TNC during previous 2 minutes **\n" << flush;

            /*       //# Tickle  has been commented out.
            if(aprsStreamRate == 0){    //Send  tickle if nothing else is being sent.
                TAprsString* tickle = new TAprsString("# Tickle\r\n",SRC_INTERNAL,srcSYSTEM);
                sendQueue.write(tickle);
            } */

            tPstats = Time;
            WatchDog = 0;               // Serial port reader increments this with each rx line.
            char *stats = getStats();
            cout << stats << flush;
            //getProcStats();
            TAprsString* monStats = new TAprsString(stats, SRC_INTERNAL, srcSTATS);
            sendQueue.write(monStats);
            delete stats;

          /*
            for(int i=0;i<MaxClients;i++)
             cout << setw(4) << sessions[i].Socket  ;

          cout << endl;
          */
        }

        if ((Time - tLastDel) > 300 ) { // do this every 5 minutes.
                                        // Remove old entrys from history list

            if(pthread_mutex_lock(pmtxCount) != 0)
                cerr << "Unable to lock pmtxCount - main-DeleteOldItems.\n" << flush;

            int di = DeleteOldItems(5);
            if(pthread_mutex_unlock(pmtxCount) != 0)
                cerr << "Unable to unlock pmtxCount - main-DeleteOldItems.\n" << flush;

            if (di > 0)
                cout << " Deleted " << di << " expired items from history list" << endl << flush;

            tLastDel = Time;
        }

    /*    					// N5VFF This was commented out -- why??
    if ((Time - tLast) > 900)		//Save history list every 15 minutes
    {
      SaveHistory(pSaveHistory);
      tLast = Time;
    }
    */

    } while (1 == 1);                     // ctrl-C to quit

    // Compiler burps on this (RH7.1)
/*
    pthread_mutex_destroy(pmtxSendFile);    // destroy/delete the mutex's before exit
    delete pmtxSendFile;
    pthread_mutex_destroy(pmtxSend);
    delete pmtxSend;
    pthread_mutex_destroy(pmtxAddDelSess);
    delete pmtxAddDelSess;
    pthread_mutex_destroy(pmtxCount);
    delete pmtxCount;
    pthread_mutex_destroy(pmtxDNS);
    delete pmtxDNS;
    return(0);
*/
}

// eof aprsd.cpp
