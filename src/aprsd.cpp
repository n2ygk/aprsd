/*
 * $Id$
 *
 * aprsd, Automatic Packet Reporting System Daemon
 * Copyright (C) 1997,2002 Dale A. Heatherington, WA4DSY
 * Copyright (C) 2001-2004 aprsd Dev Team
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

#if __GNUG__ < 3
#   include <strstream>
#   define ostringstream ostrstream
#else
#   include <sstream>
#endif

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
#include <errno.h>

#include "dupCheck.hpp"
#include "cpqueue.hpp"
#include "utils.hpp"
#include "constant.hpp"
#include "history.hpp"
#include "serial.hpp"
#include "aprsString.hpp"
#include "validate.hpp"
#include "queryResp.hpp"
#include "rf.hpp"

#include "aprsd.hpp"
#include "servers.hpp"
#include "configfile.hpp"
#include "exceptionguard.hpp"

using namespace aprsd;
using namespace std;

//History history;


struct termios initial_settings, new_settings;
string szComPort;
string szAprsPath;
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
string logDir;
bool httpStats;
string httpStatsLink;

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
    //int n = history.saveHistoryFile(outFile);

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
        string pRestore = CONFPATH;
        pRestore += TNC_RESTORE;

        rfSendFiletoTNC(pRestore);

        AsyncClose() ;
    }

    ShutDownServer = true;

    kill(pidlist.main, SIGTERM);  //Kill main thread
}

/*
    Signal handler.  Note: Signals are received by ALL threads at once.
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

/*
    Server configuration parser.

    Note: Configuration file format has changed in 2.6.x
*/
int serverConfig(const string& configFile)
{
    bool isValidPass = false;
    string errmsg;

    rfcall_idx = 0;
    posit_rfcall_idx = 0;
    stsmDest_rfcall_idx = 0;

    try {
        cout << "Reading " << configFile << endl;
        ConfigFile cf(configFile);
        ConfigSection cs;

        // Server Section
        cs = cf["server"];

        szServerCall = trim(cs["servercall"]);
        if (szServerCall.size() > 9)
            szServerCall.resize(9);

        MyLocation = trim(cs["mylocation"]);
        cout << "Server Location: " << MyLocation << endl;

        MyEmail = trim(cs["myemail"]);
        cout << "SysOp Email: " << MyEmail << endl;

        MyCall = trim(cs["mycall"]);    // This will be overwritten by MYCALL in INIT.TNC if used
        if (MyCall.size() > 9)
            MyCall.resize(9);           // Truncate to 9 chars

        logon.pass = trim(cs["pass"]);

        if (validate(MyCall, logon.pass, "aprs", true) == 0)
            isValidPass = true;

        int maxclients = atoi(trim(cs["maxusers"]).c_str());
        if (maxclients > 0)
            MaxClients = maxclients;
        else
            MaxClients = MAXCLIENTS;    // default see constant.hpp

        cout << "Max Users: " << MaxClients << endl;

        int maxload = atoi(trim(cs["maxload"]).c_str());
        if (maxload > 0)
            MaxLoad = maxload;
        else
            MaxLoad = MAXLOAD;          // default, see constant.hpp


        NetBeaconInterval = atoi(trim(cs["netbeaconinterval"]).c_str());
        NetBeacon = trim(cs["netbeacon"]);

        cout << "Beacon Interval: " << NetBeaconInterval << endl;

        ttlDefault = atoi(trim(cs["historyexpire"]).c_str());

        cout << "History Expire: " << ttlDefault << endl;

        History_ALLOW = (trim(cs["historydump"]) == "yes" ? true : false );

        spRawTNCServer.ServerPort = atoi(trim(cs["rawtncport"]).c_str()); // local TNC data only
        spRawTNCServer.EchoMask = srcTNC + wantRAW; // set data source to echo with raw data
        spRawTNCServer.FilterMask = false;
        spRawTNCServer.omni = false;

        cout << "Raw TNC port: " << spRawTNCServer.ServerPort << endl;

        spLocalServer.ServerPort = atoi(trim(cs["localport"]).c_str()); // local TNC data only
        spLocalServer.EchoMask = srcUDP
                                + srcTNC
                                + srcSYSTEM
                                + srcBEACON
                                + sendHISTORY;

        cout << "Local port: " << spLocalServer.ServerPort << endl;

        spMainServer.ServerPort = atoi(trim(cs["mainport"]).c_str()); // all data
        spMainServer.EchoMask = srcUSERVALID
                                + srcIGATE
                                + srcUDP
                                + srcTNC
                                + srcSYSTEM
                                + srcBEACON
                                + sendHISTORY;
        spMainServer.FilterMask = 0;
        spMainServer.omni = false;

        cout << "Main Server Port: " << spMainServer.ServerPort << endl;

        spMainServer_NH.ServerPort = atoi(trim(cs["mainportnohistory"]).c_str());
        spMainServer_NH.EchoMask = srcUSERVALID
                                + srcUSER
                                + srcIGATE
                                + srcUDP
                                + srcTNC
                                + srcBEACON
                                + srcSYSTEM;
        spMainServer_NH.FilterMask = 0;
        spMainServer_NH.omni = false;

        cout << "No History Port: " << spMainServer_NH.ServerPort << endl;

        spLinkServer.ServerPort = atoi(trim(cs["linkport"]).c_str());
        spLinkServer.EchoMask = srcUSERVALID
                                + srcUSER
                                + srcTNC
                                + srcUDP
                                + srcBEACON;
        spLinkServer.FilterMask = 0;
        spLinkServer.omni = false;

        cout << "Link Port: " << spLinkServer.ServerPort << endl;

        spMsgServer.ServerPort = atoi(trim(cs["messageport"]).c_str());
        spMsgServer.EchoMask = src3RDPARTY;
        spMsgServer.FilterMask = 0;
        spMsgServer.omni = false;

        cout << "Message Port: " << spMsgServer.ServerPort << endl;

        upUdpServer.ServerPort = atoi(trim(cs["udpport"]).c_str());

        cout << "UDP Port: " << upUdpServer.ServerPort << endl;

        spSysopServer.ServerPort = atoi(trim(cs["sysopport"]).c_str());
        spSysopServer.EchoMask = 0;
        spSysopServer.FilterMask = 0;
        spSysopServer.omni = false;

        cout << "SysOp Port: " << spSysopServer.ServerPort << endl;

        spHTTPServer.ServerPort = atoi(trim(cs["httpport"]).c_str());
        spHTTPServer.EchoMask = wantHTML;
        spHTTPServer.FilterMask = 0;
        spHTTPServer.omni = false;

        cout << "HTTP Server Port: " << spHTTPServer.ServerPort << endl;

        spIPWatchServer.ServerPort = atoi(trim(cs["ipwatchport"]).c_str());
        spIPWatchServer.EchoMask = srcUSERVALID
                                + srcUSER
                                + srcIGATE
                                + srcTNC
                                + srcUDP
                                + srcBEACON
                                + wantSRCHEADER;
        spIPWatchServer.FilterMask = 0;
        spIPWatchServer.omni = false;

        cout << "IP Watch Port: " << spIPWatchServer.ServerPort << endl;

        spErrorServer.ServerPort = atoi(trim(cs["errorport"]).c_str()); // provides only rejected packets
        spErrorServer.EchoMask = wantREJECTED;
        spErrorServer.FilterMask = 0;
        spErrorServer.omni = false;

        cout << "Error Port: " << spErrorServer.ServerPort << endl;

        spOmniServer.ServerPort = atoi(trim(cs["omniport"]).c_str());
        spOmniServer.EchoMask = omniPortBit;
        spOmniServer.FilterMask = 0;
        spOmniServer.omni = true;

        cout << "Omni Port: " << spOmniServer.ServerPort << endl;

        logDir = trim(cs["logdir"]);
        if (logDir.size() == 0)
            logDir = "/usr/local/var/";

        cout << "Logging to " << logDir << endl;

        //
        // There must be an easier way to do this...
        //
        nIGATES = 0;
        ostringstream ss;
        std::vector<std::string> vec;
        for (int i = 0; i < maxIGATES; i++) {
            ss.str("");
            ss << "server" << (i + 1);
            vec = split(cs[ss.str()], ",");
            if (vec[0].size() == 0)
                break;          // No more to process

            nIGATES++;          // increment number of igates defined.
            cpIGATE[i].EchoMask = 0;
            cpIGATE[i].user = MyCall;
            cpIGATE[i].pass = passDefault;
            cpIGATE[i].RemoteSocket = atoi(trim(vec[1]).c_str());
            cpIGATE[i].starttime = 0;
            cpIGATE[i].lastActive = 0;
            cpIGATE[i].pid = 0;
            cpIGATE[i].remoteIgateInfo = "";
            cpIGATE[i].hub = false;
            cpIGATE[i].mode = MODE_RECV_ONLY;
            cpIGATE[i].serverCmd.reserve(3);
            cpIGATE[i].serverCmd.push_back(string(""));
            cpIGATE[i].serverCmd.push_back(string(""));
            cpIGATE[i].serverCmd.push_back(string(""));
            cpIGATE[i].nCmds = 0;
            cpIGATE[i].alias = string("*");
            cpIGATE[i].RemoteName = trim(vec[0]);

            if (trim(vec[2]).find("hub") != string::npos)
                cpIGATE[i].hub = true;

            if (trim(vec[2]).find("sr") != string::npos)
                cpIGATE[i].mode = MODE_RECV_SEND;

            if (logon.user.size() > 0) {
                cpIGATE[i].user = logon.user;

            } else {
                cpIGATE[i].user = MyCall;
                cpIGATE[i].pass = passDefault;
            }

            if (logon.pass.size() > 0) {
                cpIGATE[i].pass = logon.pass;
            } else {
                cpIGATE[i].pass = passDefault;
            }

            //The following strings are sent to remote server as messages after logon
            if (vec.size() > 3) {
                cpIGATE[i].serverCmd[0] = (char*)trim(vec[3]).c_str();
                cpIGATE[i].nCmds++;
            }

            if (vec.size() > 4) {
                cpIGATE[i].serverCmd[0] = (char*)trim(vec[4]).c_str();
                cpIGATE[i].nCmds++;
            }

            if (vec.size() > 5) {
                cpIGATE[i].serverCmd[0] = (char*)trim(vec[5]).c_str();
                cpIGATE[i].nCmds++;
            }

            //Locally validate the passcode to see if we are allowed to send data
            if (isValidPass && cpIGATE[0].mode == MODE_RECV_SEND) {
                cpIGATE[i].EchoMask =  srcUSERVALID //If valid passcode present then we send out data
                                    + srcUSER      //Same data as igate port 1313
                                    + srcTNC
                                    + srcBEACON
                                    + srcUDP;      //Changed in 2.1.5: Internal status packets NOT sent
            } else {
                if (cpIGATE[i].mode == MODE_RECV_SEND)
                    errmsg = " <--Invalid APRS passcode";

                cpIGATE[i].mode = MODE_RECV_ONLY;
                cpIGATE[i].EchoMask = 0;
            }
        }
        traceMode = (trim(cs["trace"]) == "yes" ? true : false );   // Full Internet path tracing
        ConvertMicE = (trim(cs["convertmice"]) == "yes" ? true : false );
        APRS_PASS_ALLOW = (trim(cs["allowaprspass"]) == "yes" ? true : false );

        httpStats = (trim(cs["serverstats"]) == "yes" ? true : false);
        if (httpStats) {
            httpStatsLink = trim(cs["serverstatslink"]);
            cout << "Using HTTP Stats Page at: " << httpStatsLink << endl;
        }


        // IGATE Section
        cs = cf["igate"];

        NOGATE_FILTER = (trim(cs["filternogate"]) == "yes" ? true : false);

        RF_ALLOW = (trim(cs["rfallow"]) == "yes" ? true : false);

        szAprsPath = trim(cs["aprspath"]);

        logAllRF = (trim(cs["logallrf"]) == "yes" ? true : false);

        TncBeacon = trim(cs["tncbeacon"]);
        TncBeaconInterval = atoi(trim(cs["tncbeaconinterval"]).c_str());
        tncPktSpacing = (1000 * atoi(trim(cs["tncpacketspacing"]).c_str()));
        szComPort = trim(cs["tncport"]);
        TncBaud = trim(cs["tncbaud"]);

        int mc;

        mc = atoi(trim(cs["ackrepeats"]).c_str());
        if (mc < 0) {
            mc = 0;
            cout << "ACKREPEATS set to Zero" << endl;
        }
        if (mc > 9) {
            mc = 9;
            cout << "ACKREPEATS set to 9" << endl;
        }
        ackRepeats = mc;

        mc = atoi(trim(cs["ackrepeattime"]).c_str());
        if (mc < 1) {
            mc = 1;
            cout << "ACKREPEATTIME set to 1 sec" << endl;
        }

        if (mc > 30) {
            mc = 30;
            cout << "ACKREPEATTIME set to 30 sec" << endl;
        }
        ackRepeatTime = mc;

        igateMyCall = (trim(cs["igatemycall"]) == "yes" ? true : false );
        vec = split(trim(cs["gatetorf"]), ",");
        if (vec[0].size() > 0) {
            vector<string>::iterator it = vec.begin();

            while (it != vec.end()) {
                if (rfcall_idx > MAXRFCALL) {
                    rfcall.push_back(trim(*it));
                    rfcall_idx++;
                }

                it++;
            }
        }

        vec = split(trim(cs["gatetorf"]), ",");
        if (vec[0].size() > 0) {
            vector<string>::iterator it = vec.begin();
            while (it != vec.end()) {
                if (rfcall_idx < MAXRFCALL) {
                    rfcall.push_back(trim(*it));
                    rfcall_idx++;
                }

                it++;
            }
        }

        vec = split(trim(cs["posittorf"]), ",");
        if (vec[0].size() > 0) {
            vector<string>::iterator it = vec.begin();
            while (it != vec.end()) {
                if (posit_rfcall_idx < MAXRFCALL) {
                    posit_rfcall.push_back(trim(*it));
                    posit_rfcall_idx++;
                }

                it++;
            }
        }

        vec = split(trim(cs["msgdesttorf"]), ",");
        if (vec[0].size() > 0) {
            vector<string>::iterator it = vec.begin();
            while (it != vec.end()) {
                if (stsmDest_rfcall_idx < MAXRFCALL) {
                    stsmDest_rfcall.push_back(trim(*it));
                    stsmDest_rfcall_idx++;
                }
                it++;
            }
        }

    } catch (Exception& e) {
        cerr << e.toString() << endl;
        return 1;

    } catch (exception& e) {
        cerr << e.what() << endl;
        return 1;

    }

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

        string histFile = CONFPATH;
        histFile += SAVE_HISTORY;
        ReadHistory(histFile);
        //history.readHistoryFile(histFile);

        string sConfFile = CONFPATH;
        sConfFile += CONFFILE;              //default server configuration file

        if (argc > 1) {
            if (strcmp("-d",argv[argc-1]) != 0){
                //szConfFile = new char[CONFPATH.length() + CONFFILE.length() + 1];
                //szConfFile = new char[MAX];
                sConfFile = "";
                sConfFile = argv[argc-1];
                //szConfFile = argv[argc-1];    //get optional 1st or 2nd arg which is configuration file name
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

        if (szAprsPath.length() > 0) {
            cout << "APRS packet path = " << szAprsPath << endl;
            rfSetPath(szAprsPath);
        }


        /*Initialize TNC Com port if specified in config file */
        if (szComPort.size() > 0) {
            cout  << "Opening serial port device " << szComPort << endl;
            if ((rc = rfOpen(szComPort, TncBaud)) != 0) {
                ts.tv_sec = 2;
                ts.tv_nsec = 0;
                nanosleep(&ts,NULL);
                return -1;
            }

            cout << "Setting up TNC" << endl;

            string pInitTNC = CONFPATH;
            pInitTNC += TNC_INIT;
            //cerr << "AsyncSendFiletoTNC..." <<  "filename: " << pInitTNC << endl;
            rfSendFiletoTNC(pInitTNC);    //Setup TNC from initialization file
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
                //if ((di = history.deleteOldItems(5)) > 0)
                    cout << " Deleted " << di << " expired items from history list" << endl;

                tLastDel = Time;
            }
    /*
        if ((Time - tLast) > 900)       //Save history list every 15 minutes
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
