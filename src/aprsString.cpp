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



/* TAprsString class implimentation*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

extern "C" {
#include <pthread.h>
#include <stdio.h>
}

#include <string>
#include <stdexcept>
#include <strstream>

#include "osdep.h"
#include "constant.h"
#include "aprsString.h"
#include "utils.h"
#include "mic_e.h"
#include "crc.h"
#include "mutex.h"

using namespace std;
using namespace aprsd;

Mutex TAprsString::mutex;

int TAprsString::NN = 0;
long TAprsString::objCount = 0;

#define pathDelm ">,:"
extern const int srcTNC, src3RDPARTY;
extern int ttlDefault;
extern bool ConvertMicE;
extern bool checknogate;

TAprsString::TAprsString(const char* cp, int s, int e, const char* szPeer, const char* userCall) : string(cp)
{
    constructorSetUp(cp, s, e);
    peer = szPeer;
    call = userCall;
    srcHeader = "!" + peer + ":" + call + "!";  // Build the source ip header
}

TAprsString::TAprsString(const char* cp, int s, int e) : string(cp)
{
    peer = "";
    user = "";
    call = "";
    srcHeader = "!:!";
    constructorSetUp(cp, s, e);
}

TAprsString::TAprsString(string& cp, int s, int e) : string(cp)
{
    peer = "";
    user = "";
    call = "";
    srcHeader = "!:!";
    constructorSetUp(cp.c_str(), s, e);
}

TAprsString::TAprsString(const char* cp) : string(cp)
{
    peer = "";
    user = "";
    call = "";
    srcHeader = "!:!";
    constructorSetUp(cp, 0, 0);
}


TAprsString::TAprsString(string& cp) : string(cp)
{
    peer = "";
    user = "";
    call = "";
    srcHeader = "!:!";
    constructorSetUp(cp.c_str(), 0, 0);
}

//
// Copy constructor
//
TAprsString::TAprsString(TAprsString& as) : string(as)
{
    *this = as;
    ax25Source = as.ax25Source;
    ax25Dest = as.ax25Dest;
    stsmDest = as.stsmDest;
    query = as.query;
    acknum = as.acknum;
    dest = as.dest;
    path = as.path;
    data = as.data;
    user = as.user;
    pass = as.pass;
    call = as.call;
    pgmName = as.pgmName;
    pgmVers = as.pgmVers;
    peer = as.peer;
    raw = as.raw;
    srcHeader = as.srcHeader;
    msgdata = "";
    Lock lock(mutex);

    for (int i=0;i<MAXWORDS;i++)
        ax25Path[i] = as.ax25Path[i];

    ttl = as.ttl;
    next = NULL;
    last = NULL;
    aprsType = as.aprsType;
    msgType = as.msgType;
    allowdup = as.allowdup;
    sourceSock = as.sourceSock;
    pathSize = as.pathSize;
    EchoMask = as.EchoMask;
    lastPositTx = as.lastPositTx;

    lock.get();
    NN++;                               // Increment counters
    objCount++;
    ID = objCount;                      // new serial number
    timestamp = time(NULL);             // User current time instead of time in original
    instances = 0;

    lock.release();
}


TAprsString::~TAprsString(void)
{
    Lock lock(mutex);
    lock.get();
    NN--;
    lock.release();
}




void TAprsString::constructorSetUp(const char* cp, int s, int e)
{

    try {
        aprsType = APRSUNKNOWN;
        dest = 0;
        instances = 0;
        reformatted = false;
        allowdup = false;
        msgType = 0;
        sourceSock = s;
        EchoMask = e;
        last = NULL;
        next = NULL;
        ttl = ttlDefault;
        timeRF = 0;
        AEA = false;
        acknum = "";
        query = "";
        lastPositTx = 0;
        msgdata = "";
        Lock lock(mutex);

        lock.get();
        objCount++;
        ID = objCount;                  // set unique ID number for this new object
        NN++;

        lock.release();
        timestamp = time(NULL);

        for (int i = 0; i<MAXPATH; i++)
            ax25Path[i] = "";

        for (int i = 0; i< MAXWORDS; i++)
            words[i] = "";

        ax25Source = "";
        ax25Dest   = "";
        stsmDest   = "";
        raw = string(cp);

        pathSize = 0;

        if (length() <= 0) {
            cerr << "Zero or neg string Length" << endl;
            return;
        }

        if (cp[0] == '#') {
            aprsType = COMMENT;
            //print(cout);
            return;
        }

        if ((find("user ") == 0 || find("USER ") == 0)) {   // Must be a logon string
            int n = split(*this, words, MAXWORDS, RXwhite);
            if (n > 1)
                user = words[1];
            else
                user = "Telnet";

            if (n > 3)
                pass = words[3];
            else
                pass = "-1";

            if (n > 5)
                pgmName = words[5];
            else
                pgmName = "Telnet";

            if (n > 6)
                pgmVers = words[6];

            EchoMask = 0;               // Don't echo any logon strings
            aprsType = APRSLOGON;

            return;
        }                               // else it's a aprs posit packet or something

        if (find(":") > 0) {
            size_type pIdx = find(":"); // Find location of first ":"
            path = substr(0,pIdx);      // extract all chars up to the ":" into path
            dataIdx = pIdx+1;           // remember where data starts

            size_type gtIdx = path.find(">");  // find first ">" in path
            size_type gt2Idx = path.find_last_of(">"); // find last ">" in path

            if ((gtIdx != gt2Idx) && (gtIdx != npos)) {
                //This is in AEA TNC format because it has more than 1 ">"

                //cout << "AEA " << *this << endl << flush;
                size_type savepIdx = pIdx;
                string rs;
                AEAtoTAPR(*this, rs);


                // Replace AEA path with TAPR path
                pIdx = rs.find(":");
                string rsPath = rs.substr(0, pIdx);
                replace(0, savepIdx, rsPath);
                path = rsPath;

                AEA = true;
            }


            if ((sourceSock != SRC_INTERNAL) && (path.find(">") == npos)) {
                // If there isn't a ">" in the packet
                // and it didn't come from me...
                aprsType = APRSERROR;         // then it's bogus
                return;
            }

            if ((pIdx+1) < length())
                data = substr(pIdx+1, MAXPKT);  //The data portion of the packet

            pathSize = split(path, ax25Path, MAXPATH, pathDelm);

            if (pathSize >= 2)
                ax25Dest = ax25Path[1]; // AX25 destination

            if (pathSize >= 1) {
                ax25Source = ax25Path[0];           //AX25 Source

                if ((ax25Source.length() > 9) || (ax25Source.length() < 3)) {
                    // 10 v 9 to allow for src*
                    // discard runts
                    aprsType = APRSERROR;
                    return;
                }
                int pathlen = ax25Source.length();
                char c;
                for (int i = 0; i < pathlen; i++) {
                    c = ax25Source[i];
                    if (!((c >= 'a' && c <= 'z') 
                            || (c >= 'A' && c <= 'Z')
                            || (c >= '0' && c <= '9')
                            || (c == '-'))) {
                        aprsType = APRSERROR;
                        return;
                    }
                }

                if (checknogate) {
                    if ((path.find("NOGATE") != npos) || (path.find("RFONLY") != npos)) {
                        aprsType = APRSERROR;
                        return;
                    }
                }


            }
            if ((data.length() < 4) || (data.length() > 253)) {
                // need some data to be of some use...
                // then again too much of a good thing is bad as well
                // min 4 so we will allow "?WX?" and other querries - do we want to?????
                aprsType = APRSERROR;
                return;
            }

            if ((data.find('\r') > data.length()) || (data.find('\n') > data.length())) {
                //cerr << "added CRLF to packet " << data << endl;
                append("\n\r");
                //data.append('\r');
            }

            size_type idx,qidx;

            if (ax25Dest.compare("ID") == 0) {      // ax25 ID packet
                aprsType = APRSID;
                return;
            }
            int mlen = data.length();
            switch (data[0]) {
                case ':' :
                    // Message Packet
                    // Spec:
                    // :Addressee (9bytes):Message Text (69 bytes)[{msg ID (1-5bytes)] <-optional
                    // Message Text may contain any printable data except "|~{"
                    //
                    //msgdata = "";

                    if (mlen > MAXPKT-5) {
                        aprsType = APRSERROR;
                        break;
                    }

                    stsmDest = data.substr(1, MAXPKT);
                    idx = stsmDest.find(":");
                    if (idx == npos)
                        break;

                    // Get the adressee
                    stsmDest = stsmDest.substr(0, idx);
                    if (stsmDest.length() != 9)
                        break;

                    // Get the data portion
                    msgdata = data.substr(11, data.length());

                    // Get the msg id; if available
                    // Ensure everthing is within spec
                    unsigned int x;
                    x = msgdata.find_last_of('{');
                    if (x <= msgdata.length()) {
                        msgid = msgdata.substr(x, msgdata.length());
                        msgdata = msgdata.substr(0, x);
                        if (msgdata.length() > 69)
                            msgdata.resize(69);
                    } else {
                        msgid = "";
                        if (msgdata.length() > 69)
                            msgdata.resize(69);
                    }

                    if (msgid.length() > 6)
                        msgid.resize(6);

                    // put it all back together again
                    data = ":" + stsmDest + ":" + msgdata;
                    data += msgid;
                    aprsType = APRSMSG;
                    EchoMask |= src3RDPARTY;
                    msgType = APRSMSGTEXT;
                    if (data.length() >= 15)
                        if (data.substr(10, 4) == string(":ack"))
                            msgType = APRSMSGACK ;

                    if (data.substr(10, 2) == string(":?")) {
                        qidx = data.find('?', 12);
                        if (qidx != string::npos) {
                            query = data.substr(12, qidx - 12);
                            msgType = APRSMSGQUERY;
                            qidx = data.find('{', qidx);

                            if (qidx != string::npos) {
                                acknum = data.substr(qidx);
                            }

                            //cout << "Query=" << query << " qidx=" << qidx << endl;  // debug
                        }
                    }

                    // Strip trailing white spaces from stsmDest. If this is done
                    // above, invalid messages are generated on RF (dest call must be
                    // padded)
                    idx = stsmDest.find_first_of(RXwhite);
                    if (idx != npos)
                        stsmDest = stsmDest.substr(0, idx);

                    break;

                case '_' :              // positionless weather
                                        // _ (1) Time MDHM (8) data(36) aprs software (1) wx unit (2-4)
                    if (mlen > MAXPKT-5) {
                        aprsType = APRSERROR;
                        //cerr << "Bad weather packet length = " << data.length() << endl << flush;
                        //cerr << data << endl << flush;
                    }
                    else
                        aprsType = APRSWX ;

                    break;

                case '@' :              // APRS mobile station (with messaging)
                                        // @ (1), Time DHM/HMS (7), Lat (8), Sym ID (1), Lon (9), Sym Code (1), ;
                                        //      Speed/Crs/PHG/DF (8), /BRG/NRQ (8), Comment (0-28)
                                        //
                                        // Compressed
                                        // @ (1), Time DHM/HMS (7), Sym ID (1), Lat (4), Lon (4), Sym Code (1), ;
                                        //      Speed/Crs/PHG/DF (2), Comp Type (1), Comment (0-40)
                    if (mlen > MAXPKT-5) {
                        aprsType = APRSERROR;
                        //cerr << "Bad @ packet; length = " << data.length() << endl << flush;
                        //cerr << data << endl << flush;
                    } else
                        aprsType = APRSPOS;

                    break;

                case '=' :              // APRS fixed station
                    if (mlen > MAXPKT-5) {
                        aprsType = APRSERROR;
                        //cerr << "Bad = packet; length = " << data.length() << endl << flush;
                        //cerr << data << endl << flush;
                    } else
                        aprsType = APRSPOS;

                    break;

                case '!' :              // APRS not runing, fixed short format
                    if (mlen > MAXPKT-5) {
                        aprsType = APRSERROR;
                        //cerr << "Bad ! packet; length = " << data.length() << endl << flush;
                        //cerr << data << endl << flush;
                    } else
                        aprsType = APRSPOS;

                    break;

                case '/' :              // APRS not running, fade to gray in 2 hrs
                    if (mlen > MAXPKT-5) {
                        aprsType = APRSERROR;
                        //cerr << "Bad / packet; length = " << data.length() << endl << flush;
                        //cerr << data << endl << flush;
                    } else
                        aprsType = APRSPOS;

                    break;

                case '>' :
                    if (mlen > MAXPKT-5) {
                        aprsType = APRSERROR;
                        //cerr << "Bad Status packet; length = " << data.length() << endl << flush;
                        //cerr << data << endl << flush;
                    } else
                        aprsType = APRSSTATUS;

                    break;

                case '?' :
                    if (mlen > MAXPKT-5) {
                        aprsType = APRSERROR;
                    } else {
                        qidx = data.find('?',1);
                        if (qidx != string::npos) {
                            query = data.substr(1,qidx-1);
                            aprsType = APRSQUERY;
                        }
                    }
                    break;

                case ';' :
                    if (mlen > MAXPKT-5) {
                        aprsType = APRSERROR;
                        //cerr << "Bad Object packet; length = " << data.length() << endl << flush;
                        //cerr << data << endl << flush;
                    } else
                        aprsType = APRSOBJECT;

                    break;

                case 0x60:              // These indicate it's a Mic-E packet

                case 0x27:

                case 0x1c:

                case 0x1d:
                    aprsType = APRSMIC_E;
                    break;

                case '}' :
                    {
                        reformatted = true;
                        string temp = data.substr(1,MAXPKT);
                        TAprsString reparse(temp);
                        aprsType = reparse.aprsType;
                        //print(cout);    // debug
                        break;
                    }

                case '$' :
                    if (mlen > MAXPKT-5) {
                        aprsType = APRSERROR;
                        //cerr << "Bad NMEA sentence packet; length = " << data.length() << endl << flush;
                        //cerr << data << endl << flush;
                    } else
                        aprsType = NMEA;

                    break;

                default:                // check for messages in the old format
                    if (mlen >= 10) {
                        if((data.at(9) == ':') && isalnum(data.at(0))) {
                            idx = data.find(":");
                            stsmDest = data.substr(0,idx);  //Old format
                            aprsType = APRSMSG;
                            EchoMask |= src3RDPARTY;
                            idx = stsmDest.find_first_of(RXwhite);

                            if (idx != npos)
                                stsmDest = stsmDest.substr(0,idx);

                            if (data.substr(9,4) == string(":ack"))
                                msgType = APRSMSGACK ;

                            if (data.substr(9,2) == string(":?")) {
                                qidx = data.find('?',11);

                                if (qidx != string::npos) {
                                    query = data.substr(11,qidx-11);
                                    msgType = APRSMSGQUERY;
                                    qidx = data.find('{',qidx);

                                    if (qidx != string::npos) {
                                        acknum = data.substr(qidx);
                                    }

                                    //cout << "Query=" << query << " qidx=" << qidx << endl; // debug
                                }
                            }
                        }

                        if ((data.at(9) == '*') && isalnum(data.at(0))) {
                            aprsType = APRSOBJECT;
                        }
                    }
                    break;
            }; // end switch

            if (path.find("TCPXX") != npos)
                tcpxx = true;
            else
                tcpxx = false;
        }

        return;

    } //end try

    catch (exception& rEx) {
        char *errormsg;
        errormsg = new char[500];

        memset(errormsg,0, 500);
        ostrstream msg(errormsg, 499);

        msg << "Caught exception in TAprsString: "
            << rEx.what()
            << endl
            << " [" << peer << "] " << raw.c_str()
            << endl
            << ends ;

        WriteLog(errormsg, ERRORLOG);
        cerr << errormsg << flush;
        delete[] errormsg;
        errormsg = NULL;
        aprsType = APRSERROR;
        return;
    }
}


//-------------------------------------------------------------
void TAprsString::AEAtoTAPR(string& s, string& rs)
{
    string pathElem[MAXPATH+2];
    string pathPart, dataPart;
    size_type pIdx = find(":");
    dataPart = s.substr(pIdx+1,MAXPKT);
    pathPart = s.substr(0,pIdx+1);
    int n = split(pathPart, pathElem, MAXPATH+2, pathDelm);
    rs  = pathElem[0] + '>' + pathElem[n-1];

    for (int i = 1; i < n-1; ++i)
        rs = rs + ',' + pathElem[i];    // corrected version

    rs = rs + ':' + dataPart;
}

//--------------------------------------------------------------
//  This is for debugging only.
//  It dumps important variables to the console
void TAprsString::print(ostream& os)
{
    os << *this << endl
        << "Serial No. = " << ID << endl
        << "TCPIP source socket = " << sourceSock << endl
        << "Packet type = " << aprsType << endl
        << "EchoMask = " << EchoMask << endl;

    if (aprsType == APRSLOGON) {
        os  << "User: " << user << endl
            << "Pass: " << pass << endl
            << "Program: " << pgmName << endl
            << "Vers: " << pgmVers << endl;
    } else {
        os  << "Source =" << getAX25Source() << endl
            << "Destination = " << getAX25Dest() << endl;

        for (int i = 0; i< pathSize; i++) {
            os << "Path " << i << " " << ax25Path[i] << endl;
        }
    }
}


//  returns the string as a char*
const char* TAprsString::getChar(void)
{
    return(c_str());
}


int TAprsString::getEchoMask(void)
{
    return(EchoMask);
}


void TAprsString::setEchoMask(int m)
{
    EchoMask = m;
}


string TAprsString::getAX25Source(void)
{
    return ax25Path[0];
}

string TAprsString::getAX25Dest(void)
{
    return(ax25Path[1]);
}


//---------------------------------------------------------------------------
bool TAprsString::queryLocal(void)
{
    bool localSource = false;

    if ((EchoMask & srcTNC) && (path.find("GATE*") == npos ) && (freq(path,'*') < 3))
        localSource = true;

    return(localSource);
}

//-------------------------------------------------------------------------------
// Search ax25path for match with *cp
bool TAprsString::queryPath(char* cp)
{
   bool rc = false;

    for (int i=0;i<pathSize;i++) {
        if (ax25Path[i].compare(cp) == 0) {
            rc = true;
            break;
        }
    }
    return(rc);
}



void TAprsString::stsmReformat(string& MyCall)
{
    //char *co;
    string(co);
    char out[BUFSIZE];
    memset(out, 0, BUFSIZE);
    ostrstream os(out, BUFSIZE-1);

    co = ":";

    if ((aprsType == APRSMSG) && (data.at(0) != ':'))
        co = "::";                      // convert to new msg format if old

    os <<  "}" << ax25Source
        <<  ">" << ax25Dest
        << ",TCPIP*," <<  MyCall
        << "*" << co << data << ends;

    data = out;
    reformatted = true;
}



//--------------------------------------------------------
//  Converts mic-e packets to 1 or 2 normal APRS packets
//  Returns pointers to newly allocated TAprsStrings
//  Pointers remain NULL if conversion fails.

void TAprsString::mic_e_Reformat(TAprsString** posit, TAprsString** telemetry)
{
    unsigned char mic1[512], mic2[512];
    int l1, l2;
    time_t now = time(NULL);

    //cout << data;
    //printhex((char*)ax25Dest.c_str(),strlen(ax25Dest.c_str()));
    //printhex((char*)data.c_str(),strlen(data.c_str()));

    //cout << "data len=" << data.find("\r") << endl;

    int i = fmt_mic_e((unsigned char*)ax25Dest.c_str(),
                     (unsigned char*)data.c_str(),
                     data.find("\r"),
                     mic1,&l1,
                     mic2,&l2,
                     now);

    if (i) {
        mic1[l1] = mic2[l2] = '\0';

        if (l1) {
            char *buf1 = new char[512];
            memset(buf1,0,512);
            ostrstream pbuf(buf1, 511);
            pbuf <<  path << ':' << mic1 << "\r\n" << ends;
            TAprsString* Posit = new TAprsString(buf1,sourceSock,EchoMask,peer.c_str(),call.c_str());
            Posit->raw = string(raw);   // Save a copy of the raw mic_e packet
            Posit->changePath(APRSDTOCALL,ax25Dest.c_str());
            delete[] buf1;
            buf1 = NULL;
            *posit = Posit;
        }

        if (l2) {
            char *buf2 = new char[512];
            memset(buf2,0,512);
            ostrstream tbuf(buf2, 511);
            tbuf <<  path << ':' << mic2 << "\r\n" << ends;
            TAprsString* Telemetry = new TAprsString(buf2,sourceSock,EchoMask,peer.c_str(),call.c_str());
            Telemetry->raw = string(raw);   // Save a copy of the raw mic_e packet
            Telemetry->changePath(APRSDTOCALL,ax25Dest.c_str());
            delete[] buf2;
            buf2 = NULL;
            *telemetry = Telemetry;
        }
    }
}

//---------------------------------------------------------
//  Find path element oldPath and replace with newPath
//  Returns TRUE if success.
bool TAprsString::changePath(const char* newPath, const char* oldPath)
{
    bool rc = false;
    int i;

    for (i = 0;i<pathSize;i++) {
        if (ax25Path[i].compare(oldPath) == 0 ) {
            ax25Path[i] = newPath;
            size_type idx = find(oldPath);
            replace(idx,strlen(oldPath),newPath);
            rc = true;

            if (i==1)
                ax25Dest = newPath;

            if(i==0)
                ax25Source = newPath;

            break;
        }
    }
    return rc;
}

void TAprsString::printN(void)
{
    cout << "N=" << NN << endl;
}


//-----------------------------------------------------------------
//  This is a hash function used for duplicate packet detection

INT32 TAprsString::gethash()
{
    int i;
    INT32 crc32 = 0;

    int datalen = data.length();        // Length of ax25 information field

    if (datalen >= 2)
        datalen -= 2;                   // Don't include CR-LF in length

    if (datalen > 0) {
        while ((data[datalen-1] == ' ') && (datalen > 0))
            datalen--;                  // Strip off trailing spaces
    }

    int len = length();
    int slen = ax25Source.length();
    int destlen = ax25Dest.length();

    if (aprsType == COMMENT) {          // hash the whole packet, else...
        for (i=0;i<len;i++)
            crc32 = updateCRC32(c_str()[i],crc32);
    } else {                  //hash only ax25Source and data. Ignore path.
        if (slen > 0)
            for (i=0;i < slen;i++)
                crc32 = updateCRC32(ax25Source.c_str()[i],crc32);

        //If it's a Mic-E also hash the ax25 Destination (really Longitude).
        if ((aprsType == APRSMIC_E) && (ConvertMicE == false))
            for (i=0;i < destlen;i++)
                crc32 = updateCRC32(ax25Dest.c_str()[i],crc32);

        if (datalen > 0)
            for (i=0;i < datalen;i++)
                crc32 = updateCRC32(data.c_str()[i],crc32);

        crc32 ^= dest ;                 // "dest" value is 1 or 2 depending on if destTNC or destINET.
    }                                   // This is done so the same packet can be sent to both the TNC and Internet
                                        // queues and be dup checked without a false positive.

    return(crc32) ;                     //Return 32 bit hash value
}


//-------------------------------------------
//  Returns the number of TAprsString objects that currently exist.
int TAprsString::getObjCount()
{
    int n;
    Lock lock(mutex);
    
    lock.get();
    n = NN;
    lock.release();
    return(n);
}

// eof - aprsString.cpp
