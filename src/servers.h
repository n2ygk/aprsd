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


#ifndef SERVERS_H
#define SERVERS_H

#include <string>
#include <sstream>

#include "aprsd.h"
#include "mutex.h"

using namespace std;
using namespace aprsd;

#define SZPEERSIZE 16
#define USERCALLSIZE 10
#define PGMVERSSIZE 40
#define MAXRFCALL 65    //allow 64 of 'em including a NULL for last entry.
#define BASE 0
#define USER 1
#define PASS 2
#define REMOTE 3
#define MAX 256
#define UMSG_SIZE MAX+BUFSIZE+3

#define MODE_RECV_ONLY 1
#define MODE_RECV_SEND 2

/*constants for the getStreamRate() function.*/
#define STREAM_TNC 1
#define STREAM_USER 2
#define STREAM_SERVER 3
#define STREAM_MSG 4
#define STREAM_FULL 5
#define STREAM_UDP 6



struct ConnectParams {
    int RemoteSocket;
    //char* RemoteName;
    string RemoteName;
    //char* alias;
    string alias;
    echomask_t EchoMask;
    unsigned long FilterMask;
    //char* user;
    string user;
    //char* pass;
    string pass;
    //char* remoteIgateInfo;
    string remoteIgateInfo;
    int   nCmds;
    char* serverCmd[3];
    //string serverCmd;
    long  bytesIn;
    long  bytesOut;
    time_t starttime;
    time_t lastActive;
    bool connected;
    bool hub;
    pthread_t tid;
    pid_t pid;
    int mode;
};

struct ServerParams {
    int ServerPort;
    int ServerSocket;
    echomask_t EchoMask;
    unsigned long FilterMask;
    bool omni;
    pthread_t tid;
    pid_t pid;
};



struct SessionParams {
    int   Socket;
    echomask_t  EchoMask;
    int   ServerPort;
    int   overruns;
    pid_t pid;
    time_t starttime;
    time_t lastActive;
    long  bytesIn;
    long  bytesOut;
    bool  vrfy;
    bool  dead;
    bool  svrcon;
    char *szPeer;
    char *userCall;
    char *pgmVers;
};


struct UdpParams {
    int ServerPort;
    pthread_t tid;
    pid_t pid;
};



struct pidList{
    pid_t main;
    pid_t SerialInp;
    pid_t TncQueue;
    pid_t InetQueue;
};


//Stuff for trusted UDP source IPs
struct sTrusted {
    in_addr sin_addr;   //ip address
    in_addr sin_mask;   //subnet mask
};


struct sLogon{ 
    char *user;
    char* pass;
};




//------------------------------

extern sLogon logon;

extern SessionParams *sessions;    //points to array of active socket descriptors

extern int MaxClients;      //Limits how many users can log on at once
extern int MaxLoad;         //Limits maximum server load in Kbytes/sec
extern bool tncPresent;     //TRUE if TNC com port has been specified
extern bool tncMute;        //TRUE stops messages from going to the TNC
extern bool traceMode;      //TRUE causes our Server Call to be appended to all packets in the path



extern ServerParams spMainServer, spMainServer_NH, spLinkServer;
extern ServerParams spLocalServer, spMsgServer, spHTTPServer, spSysopServer;
extern ServerParams spRawTNCServer, spIPWatchServer, spErrorServer,spOmniServer;
extern UdpParams upUdpServer;

extern const int maxIGATES;         //Defines max number of IGATES you can connect to


//extern char* szServerCall;    //  This servers "AX25 Source Call" (user defined)
extern std::string szServerCall;
//extern const char szJAVAMSG[] ;
extern std::string szJAVAMSG;
//extern char*  szAPRSDPATH;  //  Servers "ax25 dest address + path" eg: >APD213,TCPIP*:
extern std::string szAPRSDPATH;
extern char *passDefault;   //Default passcode is "-1"
//extern char *MyCall;
extern std::string MyCall;
//extern char *MyLocation;
extern std::string MyLocation;
//extern char *MyEmail;
extern std::string MyEmail;
//extern char *NetBeacon;
extern std::string NetBeacon;
//extern char *TncBeacon;
extern std::string TncBeacon;
extern char *TncBaud;
extern char szServerID[];

extern cpQueue sendQueue;       // Internet transmit queue
extern cpQueue tncQueue;        // TNC RF transmit queue
extern cpQueue charQueue;       // A queue of single characters
extern cpQueue conQueue;        // data going to the console from various threads
extern pidList pidlist;
extern bool ShutDownServer, configComplete;
extern int rfcall_idx;
extern int posit_rfcall_idx;
extern int stsmDest_rfcall_idx;
extern string* posit_rfcall[];
extern string* stsmDest_rfcall[];
extern string* rfcall[];   //list of stations gated to RF full time (all packets)

extern int TncBeaconInterval, NetBeaconInterval;
extern long tncPktSpacing;
extern bool igateMyCall;       //Set TRUE if server will gate packets to inet with "MyCall" in the ax25 source field.
extern bool logAllRF;
extern bool ConvertMicE;       //Set true causes Mic-E pkts to be converted to classic APRS pkts.

extern bool APRS_PASS_ALLOW;
extern bool History_ALLOW;       //History dumps allowed: true/false
extern int defaultPktTTL;             //Holds default packet Time to live (hops) value.
extern const int maxTRUSTED;          //Max number of trusted UDP users
extern sTrusted Trusted[];  //Array to store their IP addresses        
extern int nTrusted ;                 //Number of trusted UDP users defined
extern ConnectParams cpIGATE[];
extern int nIGATES;

extern bool RF_ALLOW, NOGATE_FILTER;  
extern int ackRepeats,ackRepeatTime;    //Used by the ACK repeat thread
extern time_t serverStartTime;
extern ULONG WatchDog;
extern int  MaxConnects, serverLoad;
extern int fullStreamRate, userStreamRate,tncStreamRate,serverStreamRate, msgStreamRate;   //Server statistics
extern int udpStreamRate;
extern ULONG tickcount, TotalConnects,  TotalLines;
extern ULONG TotalTncChars, TotalMsgChars, TotalServerChars, TotalUserChars, bytesSent, webCounter;
extern ULONG TotalUdpChars;
extern ULONG TotalFullStreamChars;
extern ULONG countREJECTED, countLOOP, countOVERSIZE, countNOGATE, countIGATED;
extern int   msg_cnt;
//extern double upTime;
extern int upTime;

extern Mutex pmtxSendFile;
extern Mutex pmtxSend;
extern Mutex pmtxAddDelSess;
extern Mutex pmtxCount;
extern Mutex pmtxDNS;


int initDefaults(void);
void serverInit(void);
void initSessionParams(SessionParams* sp, int s, int echo);
int getConnectedClients(void);
SessionParams* AddSession(int s, int echo);
bool DeleteSession(int s);
bool AddSessionInfo(int s, const char* userCall, const char* szPeer, int port, const char* pgmVers);
void CloseAllSessions(void);
void BroadcastString(const char *cp);
int SendFiletoClient(int session, char *szName);
void PrintStats(ostream &os) ;
void dequeueTNC(void);

void *HTTPServerThread(void *p);
void *TCPConnectThread(void *p);
void *UDPServerThread(void *p);
void *TCPServerThread(void *p);
void *DeQueue(void* vp);
void schedule_posit2RF(time_t t);
string getStats();
void resetCounters();
int getStreamRate(int stream);
int recvline(int sock, char *buf, int n, int *err,int timeoutMax);
void *HTTPServerThread(void* p);
int computeShortTermLoad(int t);
void enforceMaxLoad(void);

string convertUpTime(int dTime);


#endif // SERVERS_H
