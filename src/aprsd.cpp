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



#include <termios.h>
#include <unistd.h>
#include <signal.h>
#include <cassert>

#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

#include <cstdio>
#include <cctype>

#include <sys/stat.h>
#include <fcntl.h>

#include <string>

#include <cmath>
#include <cstdlib>

#include <fstream>
#include <iostream>
#include <strstream>
#include <iomanip>
#include <sstream>

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
#include <errno.h>

#include "dupCheck.h"
#include "cpqueue.h"
#include "utils.h"
#include "constant.h"
#include "history.h"
#include "serial.h"
#include "aprsString.h"
#include "validate.h"
#include "queryResp.h"
#include "rf.h"

#include "aprsd.h"
#include "servers.h"
#include "exceptionguard.h"

using namespace aprsd;
using namespace std;

struct termios initial_settings, new_settings;
//char *szComPort;
string szComPort;
int msgsn;

const string HOMEDIR("/home/aprsd2");
const string CONFPATH("");
const string CONFFILE("aprsd.conf");
const string MAINLOG("aprsd.log");
const string STSMLOG("thirdparty.log");
const string RFLOG("rf.log");
const string UDPLOG("udp.log");
const string ERRORLOG("error.log");
const string DEBUGLOG("debug.log");
const string REJECTLOG("reject.log");
const string LOOPLOG("loop.log");
const string WELCOME("welcome.txt");
const string TNC_INIT("INIT.TNC");
const string TNC_RESTORE("RESTORE.TNC");
const string APRSD_INIT("INIT.APRSD");
const string SAVE_HISTORY("history.txt");
const string USER_DENY("user.deny");

//----------------------------------------------------------------------

void serverQuit(void)      /* Invoked by console 'q' quit or SIGINT (killall -INT aprsd) */
{

    timespec ts;

    cout << "\nBeginning shutdown..." << endl;
    WriteLog(string("Server Shutdown"), MAINLOG);
    tcsetattr(fileno(stdin),TCSANOW,&initial_settings); //restore terminal mode

    string outFile = CONFPATH;
    outFile += SAVE_HISTORY;
    int n = SaveHistory(outFile);

    cout << "Saved "
        << n
        << " history items in "
        << outFile
        << endl;

    string ShutDown = szServerCall;
    ShutDown += szJAVAMSG;
    ShutDown += MyLocation;
    ShutDown += " ";
    ShutDown += szServerID;
    ShutDown += " shutting down... Bye.\r\n";

    aprsString* abuff = new aprsString(ShutDown.c_str(), SRC_INTERNAL, srcSYSTEM);

    sendQueue.write(abuff);

    ts.tv_sec = 1;
    ts.tv_nsec = 0;
    nanosleep(&ts,NULL);

    if (tncPresent) {
        //char *pRestore = new char[CONFPATH.length() + TNC_RESTORE.length() + 1];
        //strcpy(pRestore, CONFPATH.c_str());
        //strcat(pRestore, TNC_RESTORE.c_str());

        string pRestore = CONFPATH;
        pRestore += TNC_RESTORE;

        SendFiletoTNC(pRestore.c_str());

        AsyncClose() ;
        //delete pRestore;
    }

    ShutDownServer = true;

    kill(pidlist.main, SIGTERM);  //Kill main thread
}

//---------------------------------------------------------------------

/* Signal handler.  Note: Signals are received by ALL threads at once.
   To ensure the handler only reacts once, the PID is checked and only
   signals to the main thread are accepted.
*/
void sig_handler(int sig)
{
    if (getpid() != pidlist.main)
        return;  //Only allow signals from main thread.

    if (sig == SIGINT)
        serverQuit(); //Shut ourself down.
}

//------------------------------------------------------------------------
//Server configuration parser.
//Non-comment lines from aprsd.conf pass through this.
//Action is taken based on the command and args.
int serverConfig(const string& cf)
{
    const int maxToken=32;
    int nTokens ;
    char Line[256];
    string cmd, errmsg;
    int n, m = 0;

    rfcall_idx = 0;
    posit_rfcall_idx = 0;
    stsmDest_rfcall_idx = 0;
  
    for (int i = 0; i < MAXRFCALL; i++) {
        rfcall[i] = NULL;         //clear the rfcall arrays
        posit_rfcall[i] = NULL;
        stsmDest_rfcall[i] = NULL;
    }

    cout << "Reading " << cf << endl;

    ifstream file(cf.c_str());    //delete pSaveHistory;
  
    if (!file) {
        cerr << "Can't open " << cf << endl;
        return -1;
    }

    do {
        errmsg = "";
        file.getline(Line,256);     //Read each line in file
        
        if (!file.good())
            break;

        n = 0;
        int i;
        
        if (strlen(Line) > 0) { 
            if (Line[0] != '#') {   //Ignore comments
                string sLine(Line);
                string token[maxToken];
                nTokens = split(sLine, token, maxToken, RXwhite);  //Parse into tokens

                for (i = 0 ; i < nTokens; i++) 
                    cout << token[i] << " " ;

                upcase(token[0]);
                cmd = token[0];

                if ((cmd.compare("USER") == 0) && (nTokens >= 2)) {  
                    //upcase(token[1]);
                    logon.user = strdup(token[1].c_str());
                    n = 1;
                }

                if ((cmd.compare("PASS") == 0) && (nTokens >= 2)) {
                    logon.pass = strdup(token[1].c_str());
                    n = 1;
                }

                if ((cmd.compare("TNCPORT") == 0) && (nTokens >= 2)) {
                    szComPort = token[1];
                    n = 1;
                }

                if ((cmd.compare("UDPPORT") == 0) && (nTokens >= 2)) {
                    upUdpServer.ServerPort = atoi(token[1].c_str());
                    n = 1;
                }

                if ((cmd.compare("TRUST") == 0) && (nTokens >= 2)
                        && (nTrusted < maxTRUSTED)) {

                    int rc = inet_aton(token[1].c_str(), &Trusted[nTrusted].sin_addr);

                    if (nTokens >= 3)
                        inet_aton(token[2].c_str(), &Trusted[nTrusted].sin_mask);
                    else
                        Trusted[nTrusted].sin_mask.s_addr = 0xffffffff;

                    if (rc )
                        nTrusted++;
                    else
                        Trusted[nTrusted].sin_addr.s_addr = 0;

                    n = 1;
                }

                if ((cmd.compare("SERVER") == 0) //New server command in vers 2.1.5
                        && (nTokens >= 2)) {

                    cpIGATE[m].EchoMask = 0;   //default is to not send any data to other igates
                    cpIGATE[m].user = MyCall;
                    cpIGATE[m].pass = passDefault;
                    cpIGATE[m].RemoteSocket = 1313; //Default remote port is 1313
                    cpIGATE[m].starttime = -1;
                    cpIGATE[m].lastActive = -1;
                    cpIGATE[m].pid = 0;
                    cpIGATE[m].remoteIgateInfo = string("");
                    cpIGATE[m].hub = false;
                    cpIGATE[m].mode = MODE_RECV_ONLY;
                    cpIGATE[m].serverCmd[0] = NULL;
                    cpIGATE[m].serverCmd[1] = NULL;
                    cpIGATE[m].serverCmd[2] = NULL;
                    cpIGATE[m].nCmds = 0;
                    cpIGATE[m].alias = string("*");
                    cpIGATE[m].RemoteName = "UNDEFINED";

                    if (nTokens >= 2)
                        cpIGATE[m].RemoteName = strdup(token[1].c_str());  //remote domain name

                    if (nTokens >= 3)
                        cpIGATE[m].RemoteSocket = atoi(token[2].c_str());  //remote port number

                    if (nTokens >= 4) {
                        upcase(token[3]);        //parse RO-HUB or SR-HUB or RO or SR
                        if(token[3].find("HUB") != string::npos)
                            cpIGATE[m].hub = true;

                        if(token[3].find("SR") != string::npos)
                            cpIGATE[m].mode = MODE_RECV_SEND;
                    }

                    if (logon.user)
                        cpIGATE[m].user = logon.user;
                    else {
                        cpIGATE[m].user = MyCall;
                        cpIGATE[m].pass = passDefault;
                    }

                    if (logon.pass)
                        cpIGATE[m].pass = logon.pass;
                    else
                        cpIGATE[m].pass = passDefault;

                    //The following strings are sent to remote server as messages after logon
                    if (nTokens >= 5) {
                        cpIGATE[m].serverCmd[0] = strdup(token[4].c_str());
                        cpIGATE[m].nCmds++;
                    }

                    if (nTokens >= 6) {
                        cpIGATE[m].serverCmd[1] = strdup(token[5].c_str());
                        cpIGATE[m].nCmds++;
                    }

                    if (nTokens >= 7) {
                        cpIGATE[m].serverCmd[2] = strdup(token[6].c_str());
                        cpIGATE[m].nCmds++;
                    }

                    //Locally validate the passcode to see if we are allowed to send data

                    if ((validate(cpIGATE[m].user, cpIGATE[m].pass,APRSGROUP, APRS_PASS_ALLOW) == 0)
                            && (cpIGATE[m].mode == MODE_RECV_SEND)){

                        cpIGATE[m].EchoMask =  srcUSERVALID //If valid passcode present then we send out data
                                                + srcUSER      //Same data as igate port 1313
                                                + srcTNC
                                                + srcBEACON
                                                + srcUDP;      //Changed in 2.1.5: Internal status packets NOT sent
                    } else {
                        if (cpIGATE[m].mode == MODE_RECV_SEND)
                            errmsg = " <--Invalid APRS passcode";

                        cpIGATE[m].mode = MODE_RECV_ONLY;
                        cpIGATE[m].EchoMask = 0;
                    }

                    if (m < maxIGATES)
                        m++;

                    nIGATES = m;
                    n = 1;
                }

                if ((cmd.compare("IGATE") == 0)   //IGATE still recognized so not to break old .conf files.
                        || (cmd.compare("HUB") == 0)) {  //HUB is assumed to have all APRS data

                    cpIGATE[m].hub = (cmd.compare("HUB") == 0) ? true : false ;
                    cpIGATE[m].EchoMask = 0;   //default is to not send any data to other igates
                    cpIGATE[m].user = MyCall;
                    cpIGATE[m].pass = passDefault;
                    cpIGATE[m].RemoteSocket = 1313; //Default remote port is 1313
                    cpIGATE[m].starttime = -1;
                    cpIGATE[m].lastActive = -1;
                    cpIGATE[m].pid = 0;
                    cpIGATE[m].remoteIgateInfo = string("");
                    cpIGATE[m].mode = MODE_RECV_ONLY;
                    cpIGATE[m].serverCmd[0] = NULL;
                    cpIGATE[m].serverCmd[1] = NULL;
                    cpIGATE[m].serverCmd[2] = NULL;
                    cpIGATE[m].nCmds = 0;
                    cpIGATE[m].alias = string("");

                    if (nTokens >= 2)
                        cpIGATE[m].RemoteName = strdup(token[1].c_str());  //remote domain name

                    if (nTokens >= 3)
                        cpIGATE[m].RemoteSocket = atoi(token[2].c_str());  //remote port number

                    if (nTokens >= 4)
                        cpIGATE[m].user = strdup(token[3].c_str()); //User name (call)

                    if (nTokens >= 5)
                        cpIGATE[m].pass = strdup(token[4].c_str()); //Get Passcode

                    /* If passcode is valid allow the this server to send data up stream */
                    if ((validate(cpIGATE[m].user, cpIGATE[m].pass,APRSGROUP, APRS_PASS_ALLOW) == 0)){
                        cpIGATE[m].mode = MODE_RECV_SEND;
                        cpIGATE[m].EchoMask =  srcUSERVALID //If valid passcode present then we send out data
                                                + srcUSER      //Same data as igate port 1313
                                                + srcTNC
                                                + srcBEACON
                                                + srcUDP;      //Changed in 2.1.5: Internal status packets NOT sent
                    } else {
                        if(cpIGATE[m].pass[0] != '-')
                            errmsg = " <--Invalid APRS passcode";

                        cpIGATE[m].EchoMask =  0;
                        cpIGATE[m].mode = MODE_RECV_ONLY;
                    }

                    if (m < maxIGATES)
                        m++;

                    nIGATES = m;
                    n = 1;
                }

                if (cmd.compare("LOCALPORT") == 0) {   //provides local TNC data only
                    spLocalServer.ServerPort = atoi(token[1].c_str());  //set server port number
                    spLocalServer.EchoMask = srcUDP
                                            + srcTNC   /*Set data sources to echo*/
                                            + srcSYSTEM
                                            + srcBEACON
                                            + sendHISTORY;
                    n = 1;
                }

                if(cmd.compare("RAWTNCPORT") == 0) {   //provides local TNC data only
                    spRawTNCServer.ServerPort = atoi(token[1].c_str());  //set server port number
                    spRawTNCServer.EchoMask = srcTNC   /*Set data sources to echo*/
                                                + wantRAW;  //RAW data
                    spRawTNCServer.FilterMask = false;
                    spRawTNCServer.omni = false;
                    n = 1;
                }

                if (cmd.compare("SYSOPPORT") == 0) {    // provides no igated data
                                                        // This is for telnetting directly to the TNC
                    spSysopServer.ServerPort = atoi(token[1].c_str());  //set server port number
                    spSysopServer.EchoMask =   0;
                    spSysopServer.FilterMask = 0;
                    spSysopServer.omni = false;

                    n = 1;
                }

                if (cmd.compare("ERRORPORT") == 0) {    // provides only rejected packets
                                                        // This is for telneting directly to the TNC
                    spErrorServer.ServerPort = atoi(token[1].c_str());  //set server port number
                    spErrorServer.EchoMask =   wantREJECTED ;

                    spErrorServer.FilterMask = 0;
                    spErrorServer.omni = false;

                    n = 1;
                }

                if (cmd.compare("HTTPPORT") == 0) {       //provides server status in HTML format
                    spHTTPServer.ServerPort = atoi(token[1].c_str());  //set server port number
                    spHTTPServer.EchoMask = wantHTML;
                    spHTTPServer.FilterMask = 0;
                    spHTTPServer.omni = false;

                    n = 1;
                }

                if(cmd.compare("OMNIPORT") == 0) {      //provides any data stream the user requests
                    spOmniServer.ServerPort = atoi(token[1].c_str());  //set server port number
                    spOmniServer.EchoMask = omniPortBit;
                    spOmniServer.FilterMask = 0;
                    spOmniServer.omni = true;

                    n = 1;
                }

                if (cmd.compare("MAINPORT") == 0) {      //provides all data
                    spMainServer.ServerPort = atoi(token[1].c_str());  //set server port number
                    spMainServer.EchoMask = srcUSERVALID
                                            + srcUSER
                                            + srcIGATE
                                            + srcUDP
                                            + srcTNC
                                            + srcSYSTEM
                                            + srcBEACON
                                            + sendHISTORY;

                    spMainServer.FilterMask = 0;
                    spMainServer.omni = false;
                    n = 1;
                }

                if (cmd.compare("MAINPORT-NH") == 0) {      //provides all data but no history dump
                    spMainServer_NH.ServerPort = atoi(token[1].c_str());  //set server port number
                    spMainServer_NH.EchoMask = srcUSERVALID
                                                + srcUSER
                                                + srcIGATE
                                                + srcUDP
                                                + srcTNC
                                                + srcBEACON
                                                + srcSYSTEM;

                    spMainServer_NH.FilterMask = 0;
                    spMainServer.omni = false;
                    n = 1;
                }

                if (cmd.compare("LINKPORT") == 0) {      //provides local TNC data + logged on users
                    spLinkServer.ServerPort = atoi(token[1].c_str());
                    spLinkServer.EchoMask = srcUSERVALID
                                            + srcUSER
                                            + srcTNC
                                            + srcUDP
                                            + srcBEACON ; //System messages removed from link port v.215
                    spLinkServer.FilterMask = 0;
                    spLinkServer.omni = false;
                    n = 1;
                }

                if(cmd.compare("MSGPORT") == 0) {    //System messages removed from msgport port v.215
                    spMsgServer.ServerPort  = atoi(token[1].c_str());
                    spMsgServer.EchoMask = src3RDPARTY ;
                    spMsgServer.FilterMask = 0;
                    spMsgServer.omni = false;
                    n = 1;
                }

                if (cmd.compare("IPWATCHPORT") == 0){
                     spIPWatchServer.ServerPort = atoi(token[1].c_str());
                     spIPWatchServer.EchoMask = srcUSERVALID
                                             +  srcUSER
                                             +  srcIGATE
                                             +  srcTNC
                                             +  srcUDP
                                             +  srcBEACON
                                             +  wantSRCHEADER;
                     spIPWatchServer.FilterMask = 0;
                     spIPWatchServer.omni = false;
                     n = 1;
                }

                if (cmd.compare("TNCBAUD") == 0) { // Set TNC baud rate 1200,2400,4800,9600,19200
                    TncBaud = strdup(token[1].c_str());
                    n = 1;
                }

                if (cmd.compare("MYCALL") == 0) {   // This will be over-written by the MYCALL in INIT.TNC...
                                                    // ...if a TNC is being used.
                    MyCall = token[1];
                    if (MyCall.length() > 9)
                        MyCall.resize(9);  //Truncate to 9 chars.
                        
                    n = 1;
                }

                if (cmd.compare("MYEMAIL") == 0) {
                    MyEmail = token[1];
                    n = 1;
                }

                if (cmd.compare("SERVERCALL") == 0) {
                    szServerCall = token[1];
                    if (szServerCall.length() > 9)
                        //szServerCall[9] = '\0';  //Truncate to 9 chars.
                        szServerCall.resize(9);     // truncate to 9 chars
                        
                    //cout << "Config server call: " << szServerCall << endl;

                    n = 1;
                }

                if (cmd.compare("MYLOCATION") == 0) {
                    MyLocation = strdup(token[1].c_str());
                    n = 1;
                }
                
                if (cmd.compare("MAXUSERS") == 0) {  //Set max users of server.
                    int mc = atoi(token[1].c_str());
                    if (mc > 0)
                        MaxClients = mc;

                    n = 1;
                }

                if (cmd.compare("MAXLOAD") == 0) {  //Set max server output load in bytes/sec.
                    int ml = atoi(token[1].c_str());
                    if (ml > 0)
                        MaxLoad = ml ;

                    n = 1;
                }

                if (cmd.compare("EXPIRE") == 0) {    //Set time to live for history items (minutes)
                    int ttl = atoi(token[1].c_str());
                    if (ttl > 0)
                        ttlDefault = ttl;

                    n = 1;
                }

                /*
                    if (cmd.compare("TIMETOLIVE") == 0)     //Set time to live for packets on Internet (Hops)
                    {  int t = atoi(token[1].c_str());
                        if (t > 0) defaultPktTTL = t;
                    n = 1;
                    ]
                */

                if (cmd.compare("FILTERNOGATE") == 0) { //Set YES to enable reject filtering of NOGATE and RFONLY
                    upcase(token[1]);
                    if (token[1].compare("YES") == 0)
                        NOGATE_FILTER = true;
                    else
                        NOGATE_FILTER = false;

                    n = 1;
                }

                if (cmd.compare("HISTORY-ALLOW") == 0) { //Allow history list dumps.
                    upcase(token[1]);
                    if (token[1].compare("YES") == 0 )
                        History_ALLOW = true;
                    else
                        History_ALLOW = false;
                    
                    n = 1;
                }

                if (cmd.compare("RF-ALLOW") == 0) { //Allow internet to RF message passing.
                    upcase(token[1]);
                    if (token[1].compare("YES") == 0 )
                        RF_ALLOW = true;
                    else
                        RF_ALLOW = false;

                    n = 1;
                }

                if (cmd.compare("TRACE") == 0) { //Enable full internet path tracing.
                    upcase(token[1]);
                    if (token[1].compare("YES") == 0 )
                        traceMode = true;
                    else
                        traceMode = false;
                    
                    n = 1;
                }

                if (cmd.compare("IGATEMYCALL") == 0) {//Allow igating packets from "MyCall"
                    upcase(token[1]);
                    if (token[1].compare("YES") == 0 )
                        igateMyCall = true;
                    else
                        igateMyCall = false;

                    n = 1;
                }

                if (cmd.compare("LOGALLRF") == 0) { //If "YES" then all packets heard are logged to rf.log"
                    upcase(token[1]);
                    if (token[1].compare("YES") == 0 )
                        logAllRF = true;
                    else
                        logAllRF = false;
                    
                    n = 1;
                }

                if (cmd.compare("CONVERTMICE") == 0) { //If "YES" then all MIC-E packets converted to classic APRS"
                    upcase(token[1]);               //Note that compile time option CONVERT_MIC_E in constant.h
                                                    // must also be set true.
                    if ((token[1].compare("YES") == 0 ) && (CONVERT_MIC_E == true))
                        ConvertMicE = true;
                    else
                        ConvertMicE = false;

                    n = 1;
                }

                if (cmd.compare("GATE2RF") == 0) {  //Call signs of users always gated to RF
                    for (int i = 1; i < nTokens; i++) {
                        string* s = new string(token[i]);
                        if (rfcall_idx < MAXRFCALL)
                            rfcall[rfcall_idx++] = s; //add it to the list
                    }
                    n = 1;
                }

                if (cmd.compare("POSIT2RF") == 0) {  //Call sign posits gated to RF every 16 minutes
                    for (int i = 1; i < nTokens; i++) {
                        string* s = new string(token[i]);
                        if (posit_rfcall_idx < MAXRFCALL)
                            posit_rfcall[posit_rfcall_idx++] = s; //add it to the list
                    }
                    n = 1;
                }

                if (cmd.compare("MSGDEST2RF") == 0) {  //Destination call signs
                                                     // of station to station messages
                    for (int i = 1; i < nTokens; i++) {  //always gated to RF
                        string* s = new string(token[i]);
                        if (stsmDest_rfcall_idx < MAXRFCALL)
                            stsmDest_rfcall[stsmDest_rfcall_idx++] = s; //add it to the list
                    }
                    n = 1;
                }
                
                if (cmd.compare("ACKREPEATS") == 0) {  //How many extra ACKs to send to tnc
                    int mc = atoi(token[1].c_str());
                    if (mc < 0) {
                        mc = 0;
                        cout << "ACKREPEATS set to ZERO" << endl;
                    }
                    if (mc > 9) {
                        mc = 9;
                        cout << "ACKREPEATS set to 9" << endl;
                    }
                    ackRepeats = mc;
                    n = 1;
                }

                if (cmd.compare("ACKREPEATTIME") == 0) {  //Time in secs between extra ACKs
                    int mc = atoi(token[1].c_str());
                    if (mc < 1) {
                        mc = 1;
                        cout << "ACKREPEATTIME set to 1 sec." << endl;
                    }
                    if (mc > 30) {
                        mc = 30;
                        cout << "ACKREPEATTIME set to 30 sec." << endl;
                    }
                    ackRepeatTime = mc;
                    n = 1;
                }

                if (cmd.compare("NETBEACON") == 0) {  //Internet Beacon text
                    if (nTokens > 1) {
                        NetBeaconInterval = atoi(token[1].c_str());//Set Beacon Interval in minutes
                        if (nTokens > 2) {
                            string s = token[2];
                            for (int i = 3 ; i < nTokens; i++)
                                s = s + " " + token[i];

                            NetBeacon = s; //strdup(s);
                        }
                    }
                    n = 1;
                }

                if (cmd.compare("TNCBEACON") == 0) {  //TNC Beacon text
                    if (nTokens > 1){
                        TncBeaconInterval = atoi(token[1].c_str()); //Set Beacon Interval in minutes
                        if (nTokens > 2){
                            string s = token[2];
                            for (int i = 3 ; i < nTokens; i++)
                                s = s + " " + token[i] ;

                            TncBeacon = strdup(s.c_str());
                        }
                    }
                    n = 1;
                }

                if (cmd.compare("TNCPKTSPACING") == 0) {  //Set tnc packet spacing in ms
                    if (nTokens > 1)
                        tncPktSpacing = 1000 * atoi(token[1].c_str());// ms to microsecond conversion

                    n = 1;
                }

                if (cmd.compare("APRSPASS") == 0) {  //Allow aprs style user passcodes for validation?
                    upcase(token[1]);
                    if (token[1].compare("YES") == 0 )
                        APRS_PASS_ALLOW = true;
                    else
                        APRS_PASS_ALLOW = false;

                    n = 1;
                }

                cout << errmsg << endl;

                if (n == 0)
                    cout << "Unknown command: " << Line << endl;
            }
        }
    } while(file.good());

    file.close();

    return 0;
}


//---------------------------------------------------------------------

/* FOR DEBUGGING ONLY */
/*
void segvHandler(int signum)  //For debugging seg. faults
{
   pid_t pid,ppid;
   char* err;


   pid = getpid();

   if(pid == pidlist.main) err = "aprsd main";
   if(pid == spMainServer.pid) err = "spMainServer";
   if(pid == spMainServer_NH.pid) err = "spMainServer_NH";
   if(pid == spLocalServer.pid) err = "spLocalServer";
   if(pid == spLinkServer.pid) err = "spLinkServer" ;
   if(pid == spMsgServer.pid) err = "spMsgServer";
   if(pid == upUdpServer.pid) err = "upUdpServer";
   if(pid == pidlist.SerialInp) err = "Serial input thread";
   if(pid == pidlist.TncQueue) err = "TNC Dequeue";
   if(pid == pidlist.InetQueue) err = "Internet Dequeue";

   char buf[256];
   memset(buf,0,256);
   ostrstream sout(buf,255);
   sout  << "A segment violation has occured in process id "
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
*/
//----------------------------------------------------------------------
//If aprsd is invoked with the -d option...
int daemonInit(void)
{
    pid_t pid;
    pid_t xx;
    string pid_file = "/var/run/aprsd.pid";
    char s[25];

    // Check for pre-existing aprsd
    fstream f(pid_file.c_str(), ios::in);
    if (f.is_open()) {
        f.read(s, 10);
        xx = atoi(s);
        kill(xx, SIGCHLD);      // Meaningless kill to determine if pid is used
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
    unlink(pid_file.c_str());
    xx = getpid();
    fstream fp(pid_file.c_str(), ios::out);
    cout << "pid " << xx << endl;
    if (fp.is_open()) {
        fp << xx;
        fp.flush();
        if (fp.fail()) {
           // Let aprsd live since this doesn't appear to be a chkaprsd
           cout << "Warning: Could not write " << pid_file << " file!" << endl;
           fp.close();
           unlink(pid_file.c_str());
        } else
          fp.close();
    }

    setpgid(0, 0);
    freopen("/dev/null", "r", stdin);   // redirect output
    freopen("/dev/null", "w", stdout);
    freopen("/dev/null", "w", stderr);

    setsid();                           // Become session leader
    chdir(HOMEDIR.c_str());             // change to the aprsd2 directory
    umask(0);                           // Clear our file mode selection mask
    return(0);
}



//----------------------------------------------------------------------
int main(int argc, char *argv[])
{
    int rc, di;
    //char *szConfFile;
    string sConfFile;
    timespec ts;
    string stats;
    time_t lastSec,tNow,tLast,tLastDel, tPstats;
    time_t LastNetBeacon , LastTncBeacon;
    time_t Time = time(NULL);
    time_t tCalcLoad = Time;
    serverStartTime = Time;
    ExceptionGuard exceptionguard;

    try {
        msgsn = 0;                 //Clear message serial number

        initDefaults();             //Initialize some variables

        if (argc > 1)
            if (strcmp("-d",argv[1]) == 0)
                daemonInit();  //option -d means run as daemon

        pidlist.main = getpid();     //Save our own PID for future use

        signal(SIGPIPE, SIG_IGN);
        signal(SIGXCPU, SIG_IGN);
        signal(SIGTSTP, SIG_IGN);
        signal(SIGSTOP, SIG_IGN);

        signal(SIGINT, sig_handler);  //SIGINT is handled by sig_handler() .

        LastNetBeacon = 0;
        LastTncBeacon = 0;

        /*
        memset(&sa,0,sizeof(sa));
        sa.sa_handler = segvHandler;
        if(sigaction(SIGSEGV,&sa,NULL)) perror("sigaction");
        */

        configComplete = false;
        szComPort = string("");               //null string for TNC com port

        CreateHistoryList();            //Make the history linked list structure

        cout << SIGNON << endl;

        /*   //DEBUG & TEST CODE
    //aprsString mic_e("K4HG-9>RU4U9T,WIDE,WIDE,WIDE:`l'q#R>/\r\n");
    aprsString mic_e("K4HG-9>RU4W5S,WIDE,WIDE,WIDE:`l(XnUU>/Steve's RX-7/Mic-E\r\n");
    aprsString* posit = NULL;
    aprsString* telemetry = NULL;
    if(mic_e.aprsType == APRSMIC_E) mic_e.mic_e_Reformat(&posit,&telemetry);
    if(posit){ posit->print(cout); delete posit;}
    if(telemetry) { telemetry->print(cout); delete telemetry;}

    sleep(5);
    */

    //fdump = fopen("dump.txt","w+");  //debug

        //pSaveHistory = new char[CONFPATH.length() + SAVE_HISTORY.length() +1];
        //strcpy(pSaveHistory, CONFPATH.c_str());
        //strcat(pSaveHistory, SAVE_HISTORY.c_str());

        //ReadHistory(pSaveHistory);
        string histFile = CONFPATH;
        histFile += SAVE_HISTORY;
        ReadHistory(histFile);
        
        string sConfFile = CONFPATH;
        sConfFile += CONFFILE;              //default server configuration file

        if (argc > 1) {
            if (strcmp("-d",argv[argc-1]) != 0){
                //szConfFile = new char[CONFPATH.length() + CONFFILE.length() + 1];
                //szConfFile = new char[MAX];
                sConfFile = "";
                sConfFile = argv[argc-1];
                //szConfFile = argv[argc-1];	//get optional 1st or 2nd arg which is configuration file name
            }
        }

        if (serverConfig(sConfFile) != 0)
            exit(-1);     //Read configuration file (aprsd.conf)

        //Now add a ax25 path to the Internet beacon text string

        ostringstream osnetbc;
        osnetbc << szServerCall
                << szAPRSDPATH
                << NetBeacon
                << "\r\n";

        NetBeacon = osnetbc.str();   //Internet beacon complete with ax25 path
        cout << "NetBeacon is: " << osnetbc.str() << endl;

        if (TncBeacon.length() > 0){
            ostringstream ostncbc;
            ostncbc << TncBeacon
                    << "\r\n";

            TncBeacon = ostncbc.str();  //TNC beacon (no ax25 path)
        }

        /*Initialize TNC Com port if specified in config file */
        if (szComPort.length() > 0) {
            cout  << "Opening serial port device " << szComPort << endl;
            //if ((rc = AsyncOpen(szComPort, TncBaud)) != 0) {  
            if ((rc = rfOpen(szComPort, TncBaud)) != 0) {
                ts.tv_sec = 2;
                ts.tv_nsec = 0;
                nanosleep(&ts,NULL);
                return -1;
            }

            cout << "Setting up TNC" << endl;

            string pInitTNC = CONFPATH;
            pInitTNC += TNC_INIT;
            cerr << "AsyncSendFiletoTNC..." <<  "filename: " << pInitTNC << endl;
            SendFiletoTNC(pInitTNC);    //Setup TNC from initialization file
            tncPresent = true;
        } else
            cout << "TNC com port not defined." << endl;

        if (RF_ALLOW)
            cout << "Internet to RF data flow is ENABLED" << endl;
        else
            cout << "Internet to RF data flow is DISABLED" << endl;

        serverInit();       //Initialize Server - start threads

        WriteLog(string("Server Start"), MAINLOG);
        cout << "Server Started" << endl;

        Time = time(NULL);
        tNow = Time;
        tLast = Time;
        tLastDel = Time ;
        tPstats = Time;

        configComplete = true;

        if (TncBeacon.length() > 0)
            cout << "TncBeacon every " << TncBeaconInterval << " minutes : " << TncBeacon << endl;

        if (NetBeacon.length() > 0)
            cout << "NetBeacon every " << NetBeaconInterval << " minutes : " << NetBeacon << endl;

        cout << "MYCALL set to: " << MyCall << endl;

        tcgetattr(fileno(stdin), &initial_settings);
        new_settings = initial_settings;
        new_settings.c_lflag &= ~ICANON;
        new_settings.c_lflag &= ~ISIG;
        new_settings.c_cc[VMIN] = 0;
        new_settings.c_cc[VTIME] = 1;       //.1 second timeout for input chars

        tcsetattr(fileno(stdin), TCSANOW, &new_settings);

        /*
    char *xx = "WA4DSY-2>APRS,WIDE:}WA4DSY-5>APX,WIDE2-1,TCPIP,WIDE7-7:TESTING 12345\n";
    aprsString* testpkt = new aprsString(xx,SRC_TNC,srcTNC,"192.168.0.1","WA4DSY-15");
    testpkt->sourceSock = 333;
    testpkt->EchoMask = 4;
    testpkt->print(cerr);

    testpkt->thirdPartyToNormal();
    testpkt->print(cerr);

    testpkt->addPath(MyCall);
    testpkt->print(cerr);

    string ssss = "MOREPATH";
    testpkt->addPath(ssss);
    testpkt->print(cerr);


    testpkt->cutUnusedPath();
    testpkt->print(cerr);

    exit(0);
    */
    /*
    sigAct.sa_handler = sig_handler;   //Allow shutdown by SIGINT
    sigemptyset(&sigAct.sa_mask);
    sigAct.sa_flags = 0;
    sigaction(SIGINT,&sigAct,0);
    */

        do {
            ts.tv_sec = 0;
            ts.tv_nsec = 1000000;
            nanosleep(&ts, NULL);   //Sleep 1ms

            if (msgsn > 9999)
                msgsn = 0;

           while (conQueue.ready()) {        //Data for Console?
                char *cp = (char*)conQueue.read(NULL);    //Yes, read and print it.
                if (cp) {
                    printf("%s",cp);
                    strcat(cp, "\r");
                    aprsString* monStats = new aprsString(cp, SRC_INTERNAL, srcSTATS);
                    sendQueue.write(monStats);

                    delete cp;
                }
            }

            char ch = fgetc(stdin);  //stalls for 0.1 sec.

            switch (ch) {
                case  'r':
                    resetCounters();
                    break;
                case 0x03:
                case 'q' :
                    serverQuit();
                    raise(SIGTERM);;
            }
            lastSec = Time;
            Time = time(NULL);

            if (difftime(Time,lastSec) > 0) 
                schedule_posit2RF(Time);  //Once per second

            if (difftime(Time, LastNetBeacon) >= NetBeaconInterval * 60) {  //Send Internet Beacon text
                LastNetBeacon = Time;

                if ((NetBeacon.length() > 0) && (NetBeaconInterval > 0)){
                    aprsString* netbc = new aprsString(NetBeacon.c_str(), SRC_INTERNAL, srcBEACON);

                    netbc->addIIPPE() ;        //qAI
                    netbc->addPath(szServerCall);    //qAI,servercall
                    sendQueue.write(netbc);
                }
            }

            if (difftime(Time, LastTncBeacon) >= TncBeaconInterval * 60){  //Send TNC Beacon text
                LastTncBeacon = Time;
                if ((TncBeacon.length() > 0) && (TncBeaconInterval > 0)){
                    aprsString* tncbc = new aprsString(TncBeacon.c_str(), SRC_INTERNAL, srcBEACON);
                    tncQueue.write(tncbc);
                }
            }

            if ((Time - tCalcLoad) >= 15){  // Every 15 Seconds, Compute short term server load
                computeShortTermLoad(15);
                tCalcLoad = Time;
            }
            if ((Time - tPstats) > 60) {    // 1 minute
                if (WatchDog < 2)
                    cerr << "** No data from TNC during previous 2 minutes **" << endl;

                tPstats = Time;
                
                WatchDog = 0;   //Serial port reader increments this with each rx line.
                
                stats = getStats();
                cout << stats;
                aprsString* monStats = new aprsString(stats.c_str(), SRC_INTERNAL, srcSTATS);
                sendQueue.write(monStats);
                
                enforceMaxLoad();    //Check server load and shed users if required
            }
            
            if ((Time - tLastDel) > 300 ) {     //do this every 5 minutes.
                                                //Remove old entrys from history list
                if ((di = DeleteOldItems(5)) > 0)
                    cout << " Deleted " << di << " expired items from history list" << endl;

                tLastDel = Time;
            }
    /*
        if ((Time - tLast) > 900)		//Save history list every 15 minutes
            {
            SaveHistory(pSaveHistory);
            tLast = Time;
            }

        */
        } while (1==1); // ctrl-C to quit
        
    }
    catch (Exception& e) {
        cerr << e.toString() << endl;
        return 1;
    }
    catch (exception& e) {
        cerr << e.what() << endl;
        return 1;
    }
    return 0;
}
