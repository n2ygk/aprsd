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

#include <sys/ioctl.h>
#include <sys/types.h>          // send()
#include <sys/socket.h>         // send()
#include <unistd.h>             // close()
#include <cassert>
#include <iostream>

#include <string>
#include <list>
#include <iomanip>

#include "aprsd.hpp"
#include "osdep.hpp"
#include "servers.hpp"
#include "mutex.hpp"
#include "dupCheck.hpp"
#include "cpqueue.hpp"
#include "utils.hpp"
#include "constant.hpp"
#include "history.hpp"
#include "serial.hpp"
#include "aprsString.hpp"
#include "validate.hpp"
#include "queryResp.hpp"
#include "aprsdexception.hpp"

using namespace aprsd;

typedef std::list<std::string> StringList;

void buildPage(StringList& htmlpage);

string getCall(const string& sp)
{
    string retval;
    int i;

    if ((i = sp.find("-")) < static_cast<int>(sp.length()))
        retval = sp.substr(0, i);
    else
        retval = sp;

    retval += "*";
    return retval;
}

string fixEmail(const string& sp)
{
    string retval;
    int i;

    if ((i = sp.find("@")) < static_cast<int>(sp.length())) {
        retval = sp.substr(0, i);
        retval += " at ";
        retval += sp.substr(i+1, sp.length());
    } else
        retval = sp;

    return retval;
}


bool isAPRSD(const string& sp)
{
    if (static_cast<unsigned int>(sp.compare("aprsd")) < sp.length())
        return true;
    else
        return false;
}

bool isJAVAAprsSrv(const string& sp)
{
    if (static_cast<unsigned int>(sp.compare("javAPRS")) < sp.length())
        return true;
    else
        return false;
}




void buildPage(StringList& htmlpage)
{
    //StringList htmlpage;
    time_t localtime;
    int  i;
    char szTime[64];
    struct tm *gmt = NULL;
    string xmode ;
    Lock countLock(pmtxCount, false);
    Lock addDelSessLock(pmtxAddDelSess, false);
    ostringstream stats;
    ostringstream igateheader;
    ostringstream userheader;
    ostringstream endpage;  // streams to build page

    countLock.get();
    webCounter++;

    gmt = new tm;
    time(&localtime);
    gmt = gmtime_r(&localtime,gmt);
    strftime(szTime,64,"%a, %d %b %Y %H:%M:%S GMT",gmt);    // Date in RFC 1123 format
    delete gmt;

    char *szHdumps, *szMicE, *szTrace;

    if (History_ALLOW)
        szHdumps = "YES";
    else
        szHdumps = "NO";

    if (ConvertMicE)
        szMicE = "YES";
    else
        szMicE = "NO";

    if (traceMode)
        szTrace = "YES";
    else
        szTrace = "NO";

    stats << setiosflags(ios::showpoint | ios::fixed)
        << setprecision(1)
        << "HTTP/1.0 200 OK\n"
        << "Date: " << szTime << "\n"
        << "Server: aprsd\n"
        << "MIME-version: 1.0\n"
        << "Content-type: text/html\n"
        << "Expires: " << szTime << "\n"
        << "Refresh: 300\n"             //uncomment to activate 5 minute refresh time
        << "\n"                         // Blank line terminates headers
        << "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01 Transitional//EN\">\n"
        << "<HTML>\n"
        << "<HEAD>\n"
        << "<TITLE>" << szServerCall << " Server Status Report</TITLE>\n"
        << "<style type=\"text/css\">\n"
        << "<!-- \n"
        << "a { text-decoration: none; }\n"
        << "a:hover { text-decoration: underline; }\n"
        << "h1 { font-family: arial, helvetica, sans-serif; font-size: 18pt; font-weight: bold;}\n"
        << "h2 { font-family: arial, helvetica, sans-serif; font-size: 14pt; font-weight: bold;}\n"
        << "body, td { font-family: arial, helvetica, sans-serif; font-size: 10pt; }\n"
        << "th { font-family: arial, helvetica, sans-serif; font-size: 11pt; font-weight: bold; }\n"
        << "//--></style>\n"
        << "<meta http-equiv=\"Content-Type\" content=\"text/html; charset=iso-8859-1\">\n"
        << "<meta name=\"robots\" content=\"noindex,nofollow\"></HEAD>\n"

        << "<BODY>\n"
        << "<TABLE BORDER=\"0\" CELLPADDING=\"3\" CELLSPACING=\"1\" BGCOLOR=\"#000000\" ALIGN=\"CENTER\">\n"
        << "<TR ALIGN=\"CENTER\" BGCOLOR=\"#9999CC\">"
        << "<TH COLSPAN=\"2\">" << "Server: " << szServerCall << " " << MyLocation
        //<< "<BR />" << "Igate: " << MyCall << "</TH>\n"
        << "</TH>"
        << "</TR>\n"
        << "<TR VALIGN=\"BASELINE\" BGCOLOR=\"#CCCCCC\"><TD ALIGN=\"CENTER\" BGCOLOR=\"#CCCCFF\" COLSPAN=2>" << szTime << "</TD></TR>\n"
        << "<TR VALIGN=\"BASELINE\" BGCOLOR=\"#CCCCCC\"><TD BGCOLOR=\"#CCCCFF\">Server Up-Time</TD><TD>" << convertUpTime(upTime) << "</TD></TR>\n"
        << "<TR VALIGN=\"BASELINE\" BGCOLOR=\"#CCCCCC\"><TD BGCOLOR=\"#CCCCFF\">Connections</TD><TD>" << getConnectedClients() << "</TD></TR>\n"
        << "<TR VALIGN=\"BASELINE\" BGCOLOR=\"#CCCCCC\"><TD BGCOLOR=\"#CCCCFF\">Peak Connection Count</TD><TD>" << MaxConnects << "</TD></TR>\n"
        << "<TR VALIGN=\"BASELINE\" BGCOLOR=\"#CCCCCC\"><TD BGCOLOR=\"#CCCCFF\">Connection Limit</TD><TD>" << MaxClients << "</TD></TR>\n"
        //<< "<TR VALIGN=\"BASELINE\" BGCOLOR=\"#CCCCCC\"><TD BGCOLOR=\"#CCCCFF\">Max Server Load (Bps)</TD><TD>" << MaxLoad << "</TD></TR>\n"
        << "<TR VALIGN=\"BASELINE\" BGCOLOR=\"#CCCCCC\"><TD BGCOLOR=\"#CCCCFF\">Connection Count</TD><TD>" << TotalConnects << "</TD></TR>\n";
    if (tncPresent) {
        stats << "<TR VALIGN=\"BASELINE\" BGCOLOR=\"#CCCCCC\"><TD BGCOLOR=\"#CCCCFF\">TNC Packets</TD><TD>" << TotalLines << "</TD></TR>\n"
            << "<TR VALIGN=\"BASELINE\" BGCOLOR=\"#CCCCCC\"><TD BGCOLOR=\"#CCCCFF\">Pkts gated to RF</TD><TD>" << msg_cnt << "</TD></TR>\n";
    }
        //<< "<TR VALIGN=\"BASELINE\" BGCOLOR=\"#CCCCCC\"><TD BGCOLOR=\"#CCCCFF\">Local Count</TD><TD>" << localCount() << "</TD></TR>\n"
    stats << "<TR VALIGN=\"BASELINE\" BGCOLOR=\"#CCCCCC\"><TD BGCOLOR=\"#CCCCFF\">Internet Packet Count</TD><TD>" << countIGATED << "</TD></TR>\n"
        //<< "<TR VALIGN=\"BASELINE\" BGCOLOR=\"#CCCCCC\"><TD BGCOLOR=\"#CCCCFF\">NOGATE Pkts</TD><TD>" << countNOGATE <<  "</TD></TR>\n"
        //<< "<TR VALIGN=\"BASELINE\" BGCOLOR=\"#CCCCCC\"><TD BGCOLOR=\"#CCCCFF\">LOOPED Pkts</TD><TD>" << countLOOP <<  "</TD></TR>\n"
        //<< "<TR VALIGN=\"BASELINE\" BGCOLOR=\"#CCCCCC\"><TD BGCOLOR=\"#CCCCFF\">OVERSIZE Pkts</TD><TD>" << countOVERSIZE <<  "</TD></TR>\n"
        << "<TR VALIGN=\"BASELINE\" BGCOLOR=\"#CCCCCC\"><TD BGCOLOR=\"#CCCCFF\">Dropped Packets</TD><TD>" << countREJECTED <<  "</TD></TR>\n"
        << "<TR VALIGN=\"BASELINE\" BGCOLOR=\"#CCCCCC\"><TD BGCOLOR=\"#CCCCFF\">UDP Stream Rate</TD><TD>" << convertRate(getStreamRate(STREAM_UDP)) << convertScale(getStreamRate(STREAM_UDP)) <<  "</TD></TR>\n"
        << "<TR VALIGN=\"BASELINE\" BGCOLOR=\"#CCCCCC\"><TD BGCOLOR=\"#CCCCFF\">Message Stream Rate</TD><TD>" << convertRate(getStreamRate(STREAM_MSG)) << convertScale(getStreamRate(STREAM_MSG)) <<  "</TD></TR>\n";

    if (tncPresent) {
        stats << "<TR VALIGN=\"BASELINE\" BGCOLOR=\"#CCCCCC\"><TD BGCOLOR=\"#CCCCFF\">TNC stream Rate</TD><TD>" << getStreamRate(STREAM_TNC) << convertScale(getStreamRate(STREAM_TNC)) << "</TD></TR>\n";
     }

     stats << "<TR VALIGN=\"BASELINE\" BGCOLOR=\"#CCCCCC\"><TD BGCOLOR=\"#CCCCFF\">User Stream Rate</TD><TD>" << convertRate(getStreamRate(STREAM_USER)) << convertScale(getStreamRate(STREAM_USER)) <<  "</TD></TR>\n"
        << "<TR VALIGN=\"BASELINE\" BGCOLOR=\"#CCCCCC\"><TD BGCOLOR=\"#CCCCFF\">Server Stream Rate</TD><TD>" << convertRate(getStreamRate(STREAM_SERVER)) << convertScale(getStreamRate(STREAM_SERVER)) <<  "</TD></TR>\n"
        << "<TR VALIGN=\"BASELINE\" BGCOLOR=\"#CCCCCC\"><TD BGCOLOR=\"#CCCCFF\">Full Stream Rate -dups</TD><TD>" << convertRate(getStreamRate(STREAM_FULL)) << convertScale(getStreamRate(STREAM_FULL)) << "</TD></TR>\n"
        << "<TR VALIGN=\"BASELINE\" BGCOLOR=\"#CCCCCC\"><TD BGCOLOR=\"#CCCCFF\">Server Load</TD><TD>" << convertRate(serverLoad) << convertScale(serverLoad) << "</TD></TR>\n"
        << "<TR VALIGN=\"BASELINE\" BGCOLOR=\"#CCCCCC\"><TD BGCOLOR=\"#CCCCFF\">AprsString Objects</TD><TD>" << aprsString::getObjCount() << "</TD></TR>\n"
        << "<TR VALIGN=\"BASELINE\" BGCOLOR=\"#CCCCCC\"><TD BGCOLOR=\"#CCCCFF\">History Items</TD><TD>" << ItemCount  << "</TD></TR>\n"
        //<< "<TR VALIGN=\"BASELINE\" BGCOLOR=\"#CCCCCC\"><TD BGCOLOR=\"#CCCCFF\">History Time Limit</TD><TD>" << ttlDefault << " min.</TD></TR>\n"
        //<< "<TR VALIGN=\"BASELINE\" BGCOLOR=\"#CCCCCC\"><TD BGCOLOR=\"#CCCCFF\">History dump aborts</TD><TD>" << dumpAborts << "</TD></TR>\n"
        //<< "<TR VALIGN=\"BASELINE\" BGCOLOR=\"#CCCCCC\"><TD BGCOLOR=\"#CCCCFF\">History dumps allowed</TD><TD>" << szHdumps  << "</TD></TR>\n"
        //<< "<TR VALIGN=\"BASELINE\" BGCOLOR=\"#CCCCCC\"><TD BGCOLOR=\"#CCCCFF\">Mic-E conversions </TD><TD>" << szMicE  << "</TD></TR>\n"
        //<< "<TR VALIGN=\"BASELINE\" BGCOLOR=\"#CCCCCC\"><TD BGCOLOR=\"#CCCCFF\">Trace Internet Path </TD><TD>" << szTrace << "</TD></TR>\n"
        << "<TR VALIGN=\"BASELINE\" BGCOLOR=\"#CCCCCC\"><TD BGCOLOR=\"#CCCCFF\">Items in Queue</TD><TD>" << sendQueue.getItemsQueued() << "</TD></TR>\n"
        //<< "<TR VALIGN=\"BASELINE\" BGCOLOR=\"#CCCCCC\"><TD BGCOLOR=\"#CCCCFF\">Queue Overflows</TD><TD>" << sendQueue.overrun << "</TD></TR>\n"
        //<< "<TR VALIGN=\"BASELINE\" BGCOLOR=\"#CCCCCC\"><TD BGCOLOR=\"#CCCCFF\">TncQ overflows</TD><TD>" << tncQueue.overrun << "</TD></TR>\n"
        //<< "<TR VALIGN=\"BASELINE\" BGCOLOR=\"#CCCCCC\"><TD BGCOLOR=\"#CCCCFF\">ConQ overflows</TD><TD>" << conQueue.overrun << "</TD></TR>\n"
        //<< "<TR VALIGN=\"BASELINE\" BGCOLOR=\"#CCCCCC\"><TD BGCOLOR=\"#CCCCFF\">charQ overflows</TD><TD>" << charQueue.overrun << "</TD></TR>\n"
        << "<TR VALIGN=\"BASELINE\" BGCOLOR=\"#CCCCCC\"><TD BGCOLOR=\"#CCCCFF\">HTTP Access Counter</TD><TD>" << webCounter << "</TD></TR>\n"
        << "<TR VALIGN=\"BASELINE\" BGCOLOR=\"#CCCCCC\"><TD BGCOLOR=\"#CCCCFF\">IGATE Queries</TD><TD>" << queryCounter << "</TD></TR>\n"
        << "<TR VALIGN=\"BASELINE\" BGCOLOR=\"#CCCCCC\"><TD BGCOLOR=\"#CCCCFF\">Server Version</TD><TD>" << VERS << "</TD></TR>\n"
        << "<TR VALIGN=\"BASELINE\" BGCOLOR=\"#CCCCCC\"><TD BGCOLOR=\"#CCCCFF\">Sysop email</TD><TD><A HREF=\"mailto:" << MyEmail << "\">" << fixEmail(MyEmail) << "</A></TD></TR>\n";
        
        if (httpStats)
            stats << "<tr valign=\"baseline\" bgcolor=\"#CCCCCC\"><td bgcolor=\"#CCCCFF\" colspan=2 align=center><a href=\"" << httpStatsLink << "\">Server Stats</a></td></tr>\n";
        
        stats << "</TABLE><BR />\n";


    countLock.release();

    htmlpage.push_back(stats.str());

    igateheader << "<TABLE BORDER=\"0\" CELLPADDING=\"3\" CELLSPACING=\"1\" BGCOLOR=\"#000000\" ALIGN=\"CENTER\"\n>"
        << "<TR VALIGN=\"BASELINE\" BGCOLOR=\"#9999CC\">\n"
        << "<TH COLSPAN=11 ALIGN=\"CENTER\">Server Connections</TH></TR>\n"
        << "<TR BGCOLOR=\"#CCCCFF\"><TH>Domain Name</TH><TH>Hex IP<br>Alias</TH><TH>Port</TH><TH>Type</TH><TH>Status</TH><TH>Igate Pgm</TH>\n"
        << "<TH>Last active<BR>H:M:S</TH><TH>Bytes<BR> In</TH><TH>Bytes<BR> Out</TH><TH>Time<BR> H:M:S</TH></TR>\n";

    htmlpage.push_back(igateheader.str());

    countLock.get();

    for (i = 0; i < nIGATES; i++) {
        if (cpIGATE[i].RemoteSocket != -1) {
            string timeStr;
            strElapsedTime(cpIGATE[i].starttime, timeStr);           // Compute elapsed time of connection
            string lastActiveTime;
            strElapsedTime(cpIGATE[i].lastActive, lastActiveTime);   // Compute time since last input char

            ostringstream igateinfo;
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
                    bgcolor = "\"#909090\"";
                }
                conType = "SERVER";
            }

            xmode = "";
            if (cpIGATE[i].mode == MODE_RECV_ONLY)
                xmode = "-RO";

            if (cpIGATE[i].mode == MODE_RECV_SEND)
                xmode = "-SR";

            string infoTokens[3];
            infoTokens[1] = "*";
            infoTokens[2] = "";
            int ntok = 0;

            if (cpIGATE[i].remoteIgateInfo.length() > 0) {
                string rii(cpIGATE[i].remoteIgateInfo);

                if (rii[0] == '#') {
                    ntok = split(rii, infoTokens, 3, RXwhite);      // Parse into tokens if first char was "#".
                }                       // Token 1 is remote igate program name and token 2 is the version number.
            }

            igateinfo << "<TR VALIGN=\"CENTER\" BGCOLOR=" << bgcolor << "><TD>";

            // See if this client/server supports a status page
            if (isAPRSD(infoTokens[1]) || isJAVAAprsSrv(infoTokens[1]))
                igateinfo << "<A HREF=\"http://"
                << cpIGATE[i].RemoteName
                << ":14501/\">" << cpIGATE[i].RemoteName
                <<  "</A></TD>";
            else
                igateinfo << cpIGATE[i].RemoteName << "</TD>";

            igateinfo << "<TD>" << cpIGATE[i].alias << "</TD>"
                << "<TD>" << cpIGATE[i].RemoteSocket << "</TD>"
                << "<TD>" << conType << xmode << "</TD>"
                << "<TD>" << status << "</TD>"
                << "<TD>" << infoTokens[1] << " " << infoTokens[2] << "</TD>"
                << "<TD>" << lastActiveTime << "</TD>"
                << "<TD>"  << cpIGATE[i].bytesIn / 1024 << " K</TD>"
                << "<TD>" << cpIGATE[i].bytesOut / 1024 << " K</TD>"
                << "<TD>" << timeStr << "</TD>"
                //<< "<TD>" << cpIGATE[i].pid << "</TD>"
                << "</TR>\n";
            htmlpage.push_back(igateinfo.str());
        }
    }

    countLock.release();

    userheader << "</TABLE><BR />\n"
        << "<TABLE BORDER=\"0\" CELLPADDING=\"3\" CELLSPACING=\"1\" BGCOLOR=\"#000000\" ALIGN=\"CENTER\"\n>"       // Start of user list table
        << "<TR  VALIGN=\"BASELINE\" BGCOLOR=\"#9999CC\">\n"
        << "<TH COLSPAN=10>Client Connections</TH></TR>\n"
        << "<TR ALIGN=\"CENTER\" BGCOLOR=\"#CCCCFF\"><TH>IP Address</TH><TH>Port</TH><TH>Call</TH><TH>Vrfy</TH>"
        << "<TH>Program Vers</TH><TH>Last Active<BR>H:M:S</TH><TH>Bytes<BR> In</TH><TH>Bytes <BR>Out</TH><TH>Time<BR> H:M:S</TH></TR>\n";

    htmlpage.push_back(userheader.str());

    addDelSessLock.get();
    countLock.get();

    string TszPeer;
    string TserverPort;
    string TuserCall;
    string TpgmVers;
    string TlastActive;
    string TtimeStr;

    int bytesout = 0;
    int bytesin = 0;
    int npid = -1;

    for (i = 0; i < MaxClients; i++) {        // Create html table with user information
        if ((sessions[i].Socket != -1) && (sessions[i].ServerPort != -1)){
            //char timeStr[32];
            string timeStr;
            strElapsedTime(sessions[i].starttime, timeStr);      // Compute elapsed time
            //char lastActiveTime[32];
            string lastActiveTime;
            strElapsedTime(sessions[i].lastActive, lastActiveTime);  // Compute elapsed time from last input char

            string szVrfy;
            TszPeer = sessions[i].sPeer;
            TuserCall = sessions[i].sUserCall;
            TpgmVers = sessions[i].sPgmVers;
            TlastActive = lastActiveTime;
            TtimeStr = timeStr;
            //removeHTML(TszPeer);
            //removeHTML(TuserCall);
            //removeHTML(TpgmVers);
            //removeHTML(TlastActive);
            //removeHTML(TtimeStr);

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

            ostringstream userinfo;

            userinfo << "<TR VALIGN=\"CENTER\" BGCOLOR=\"#C0C0C0\"> <TD>";

            if (isAPRSD(TpgmVers) || isJAVAAprsSrv(TpgmVers))
                userinfo << "<A HREF=\"http://"
                << TszPeer << ":14501/\">" << TszPeer <<  "</A></TD>";
            else
                userinfo << TszPeer << "</TD>";

                //<< "<TD><A target=\"PortInfoFrame\" HREF=\"portinfo." << i << "\">" << sessions[i].ServerPort << "</A></TD>"
            userinfo << "<TD>" << sessions[i].ServerPort << "</TD>"
                << "<TD>" << "<a href=\"http://map.findu.com/" << getCall(TuserCall) << "\">" << TuserCall << "</a>" << "</TD>"
                << "<TD>"  << szVrfy << "</TD>"
                << "<TD>" << TpgmVers  << "</TD>"
                << "<TD>" << TlastActive  << "</TD>"
                << "<TD>" << bytesin << " K</TD>"
                << "<TD>" << bytesout << " K</TD>"
                << "<TD>" << TtimeStr  << "</TD>"
                //<< "<TD>" << npid << "</TD>"
                << "</TR>" << endl;

            htmlpage.push_back(userinfo.str());

        }   //End of if()

    }   //End of for()

    countLock.release();
    addDelSessLock.release();

    endpage << "</TABLE>\n</BODY>\n</HTML>";
    htmlpage.push_back(endpage.str());
    //return htmlpage;
}


//----------------------------------------------------------------------
void buildPortInfoPage(list<string>& htmlpage, const string& arg)
{
    time_t localtime;
    unsigned  idx,i;
    char szTime[64];
    struct tm *gmt = NULL;
    long sNum = 0;
    char *endptr;
    gmt = new tm;
    Lock addDelSessLock(pmtxAddDelSess, false);
    ostringstream stats, endpage;

    time(&localtime);
    gmt = gmtime_r(&localtime,gmt);
    strftime(szTime,64,"%a, %d %b %Y %H:%M:%S GMT",gmt);    // Date in RFC 1123 format
    delete gmt;

    idx = arg.find_last_of(".");
    if (idx != string::npos){
        string snum = arg.substr(idx+1);  //Get session number in ascii form.
        sNum = strtol(snum.c_str(),&endptr,10);
        if ((sNum == 0) && (endptr == snum.c_str()))
            return;

        if ((sNum >= MaxClients) || (sNum < 0))
            return;
    }

    addDelSessLock.get();

    if ((sessions[sNum].Socket == -1) || (sessions[sNum].ServerPort == -1)) {//No such session?
        addDelSessLock.release();
        return;
    }

    unsigned nBits = (sizeof(echomask_t) * 8);      //Number of bits in the EchoMask

    //char emb[nBits + 1];
    char* emb = new char(nBits + 1);

    for (i = 0; i < nBits; i++) {
        if (sessions[sNum].EchoMask & (1 << i))
            emb[i] = '1';
        else
            emb[i] = '0';
    }

    emb[i] = '\0';

    //char* name = new char(nBits);
    std::vector<string> name(nBits);

    name[0] = "Local TNC";
    name[1] = "User";
    name[2] = "Validated User";
    name[3] = "Server/Hub";
    name[4] = "System Msgs";
    name[5] = "UDP Port";
    name[6] = "History List";
    name[7] = "All Messages";
    name[8] = "Server Stats";
    name[9] = "Server Beacon";
    name[10]= "Rejected Pkts";
    name[11]= "Source Header";
    name[12]= "HTML";
    name[13]= "Raw";
    name[14]= "Duplicates";
    name[15]= "History";
    name[16]= "Echo";
    name[17]= ""; // NULL;

    string TszServerCall = szServerCall;
    removeHTML(TszServerCall);
    string TuserCall = sessions[sNum].sUserCall;
    removeHTML(TuserCall);

    i = 0;
    string emStr = "";
    string color;

    while (i < name.size()) {
        if (emb[i] == '1')
            color = "<TR BGCOLOR=\"30C030\">";
        else
            color = "<TR BGCOLOR=\"909090\">";

        emStr = emStr + color + "<TD>" + name[i] + "</TD><TD ALIGN=\"CENTER\" WIDTH=\"20%\">" + emb[i] + "</TD></TR>\n";
        i++;
    }

    delete [] emb;

    stats << setiosflags(ios::showpoint | ios::fixed)
        << setprecision(1)
        << "HTTP/1.0 200 OK\n"
        << "Date: " << szTime << "\n"
        << "Server: aprsd\n"
        << "MIME-version: 1.0\n"
        << "Content-type: text/html\n"
        << "Expires: " << szTime << "\n"
        << "\n"                           //Blank line terminates headers
        << "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01 Transitional//EN\">\n"
        << "<HTML>"
        << "<HEAD><TITLE>" << TszServerCall << " User Stream Filter Info</TITLE>\n"
        << "<meta http-equiv=\"Content-Type\" content=\"text/html; charset=iso-8859-1\">\n"
        << "<meta name=\"robots\" content=\"noindex,nofollow\"></HEAD>\n"
        << "<BODY LINK=\"#0000FF\" VLINK=\"#800080\" ALINK=\"#FF0000\" BGCOLOR=\"#606060\"><CENTER>"
        << "<P><TABLE WIDTH=\"150\" BORDER=2 BGCOLOR=\"#D0D0D0\">"
        << "<TR BGCOLOR=\"#FFD700\">"
        << "<TH COLSPAN=2>Port Filter</TH></TR>\n"
        << "<TR BGCOLOR=\"#FFD700\">"
        << "<TH COLSPAN=2>User: " << TuserCall
        << "<BR>Port: " << sessions[sNum].ServerPort << "</TH></TR>\n"
        //<< "<TR><TD colspan=2>" << emb << "</TD></TR>"
        << emStr;

    addDelSessLock.release();
    htmlpage.push_back(stats.str());

    endpage << "</TABLE></CENTER></BODY></HTML>";
    htmlpage.push_back(endpage.str());

    return;
}


/*  Thread to process http request for server status data */
void *HTTPServerThread(void *p)
{
    unsigned idx, i;
    char buf[BUFSIZE];
    SessionParams *psp = (SessionParams*)p;
    int sock = psp->Socket;
    delete psp;
    int page_select;
    char  szError[MAX];
    int n, rc,data;
    int err,nTokens;

    if (sock < 0)
        pthread_exit(0);

    data = 1;                           // Set socket for non-blocking
    ioctl(sock,FIONBIO, (char*)&data, sizeof(int));

    rc = recvline(sock, buf, BUFSIZE, &err, 10);  //10 sec timeout value

    if (rc<=0) {
        close(sock);
        pthread_exit(0);
    }

    string sLine(buf);
    string token[4];
    nTokens = split(sLine, token, 4, RXwhite);      // Parse http request into tokens

    char buf2[127];

    do {
        n = recvline(sock,buf2,126,&err, 1);        // Discard everything else
    } while (n > 0);

    if (n == -2) {
        close(sock);
        pthread_exit(0);
    }

    page_select = 0;
    idx = 0;

    if ((token[0].compare("GET") == 0) && (token[1].find("/portinfo.") != string::npos))
        page_select = 2;

    if ((token[0].compare("GET") == 0) && (token[1].compare("/") == 0))
        page_select = 1;

    list<string> html2send;

    /* Assemble page based on value of page_select variable */
    switch (page_select) {
        case 1:
            buildPage(html2send);
            idx = html2send.size();
            break;

        case 2:
            buildPortInfoPage(html2send, token[1]);
            idx = html2send.size();
            break;
    }

    //Now read fully assembled web page from html2send and send to the tcpip socket.
    if (idx > 0) {
        data = 0;                           // Set socket for blocking
        ioctl(sock, FIONBIO, (char*)&data, sizeof(int));
        rc = 1;
        i = 0;

        list<string>::iterator it = html2send.begin();
        while ((rc > 0) && (it != html2send.end())) {
            //cout << *it << endl;
            string as = *it;
            rc = send(sock, as.c_str(), as.size(), 0);
            it++;
        }
    } else {   //Error if page failed to assemble
        strcpy(szError,"HTTP/1.0 404 Not Found\nContent-Type: text/html\n\n<HTML><BODY>Unable to process request</BODY></HTML>\n");
        send(sock, szError, strlen(szError), 0);
    }

    close(sock);
    pthread_exit(0);
    return NULL;  //To keep g++ and RH-7.x happy.
}
