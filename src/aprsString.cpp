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
// Note:    This needs to be redone as this* is going away
//          in GCC 3.x
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

// Class destructor
TAprsString::~TAprsString(void)
{
    Lock lock(mutex);
    
    lock.get();
    NN--;
    lock.release();
}



void TAprsString::constructorSetUp(const char* cp, int s, int e)
{

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
    string as;
    size_t pSourceIdx;
    size_t pPathIdx;

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
    as = raw;

    pathSize = 0;

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

    if ((pSourceIdx = as.find('>')) > 0) {
        ax25Source = as.substr(0, pSourceIdx);
    }

    if (ax25Source[0] == '#') {
        aprsType = COMMENT;
        return;
    }

    if (!ValidSource(ax25Source)) {
        aprsType = APRSERROR;
        cerr << "Bad Source: [" << ax25Source << "]" << endl;
        return;
    }

    if ((pPathIdx = as.find(':')) > 0) {
        path = as.substr(pSourceIdx+1, pPathIdx-(pSourceIdx+1));
        ax25Dest = path;
        data = as.substr(pPathIdx+1, as.length());
    }
    if (!ValidPath(path)) {
        aprsType = APRSERROR;
        cerr << "Bad Path: [" << path << "]" << endl;
        return;
    }

    if (!ValidData(data)) {
        aprsType = APRSERROR;
        cerr << "Bad Data: [" << data << "]" << endl;
        return;
    } else {
        aprsType = GetMsgType(data);
    }

    return;


}


//-------------------------------------------------------------
// Converts an AEA packet to TAPR2
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


//  returns the string as a const char*
const char* TAprsString::getChar(void)
{
    return(c_str());
}

// returns the EchoMask
int TAprsString::getEchoMask(void)
{
    return(EchoMask);
}

// sets the EchoMask to "m"
void TAprsString::setEchoMask(int m)
{
    EchoMask = m;
}

// Returns the AX25Source
string TAprsString::getAX25Source(void)
{
    return ax25Path[0];
}

// Returns the AX25Dest
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


// This routine inspects the station-to-station message
// packet and converts it the "new" style if it isn't
// already
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
//
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


// Validates packet source data
//
// This simply looks for a valid source header.  The spec says upercase with 
// a 0-15 SSID, but we look for any alphanumeric plus "-".
bool TAprsString::ValidSource(string& as)
{
    bool retval = false;
    size_t idx;

    if ((idx = as.find("*"))  <= as.length())
        as = as.substr(0, idx);

    if ((as.length() < 3) || (as.length() > 9)) {
        cerr << "Source length error..." << endl;
        retval = false;
    }

    int len = as.length();
    char c;
    for (int i = 0; i < len; i++) {
       c = as[i];
       if ((c >= 'a' && c <= 'z')
            || (c >= 'A' && c <= 'Z')
            || (c >= '0' && c <= '9')
            || (c == '-')) {
           //cerr << c;
           retval = true;
       } else {
           retval = false;
           break;
       }

    }
    return retval;
}

// Validates packet destination
//  This simply looks for a vaild destination path.  We look for
// alphanumeric plus "-,*/".  Actually, according to spec "/" shouldn't
// be there but some TNC's will insert this to indicated which
// port it was heard on.
bool TAprsString::ValidPath(string& as)
{
    bool retval = false;



    int len = as.length();
    char c;
    for (int i = 0; i < len; i++) {
       c = as[i];
       if ((c >= 'a' && c <= 'z')
            || (c >= 'A' && c <= 'Z')
            || (c >= '0' && c <= '9')
            || (c == '-')
            || (c == ',')
            || (c == '*')
            || (c == '/') ) { // This one needed for lame TNC's port stuff
           retval = true;
       } else {
           retval = false;
           //cerr << "Invalid data in path" << endl;
           return retval;
       }
    }

    // was this packet from a validated TCP user?
    if (as.find("TCPXX") < as.length())
                tcpxx = true;
            else
                tcpxx = false;

    if (!ValidToCall(as)) {
        //cerr << "Invalid ToCall: " << toCall << endl;
        retval = false;
    }
    return retval;
}

// This attempts to ascertain a reasonable To Call.
// We don't check for the validity of the many tocalls
// available.  We only make sure that RELAY and WIDE are
// not inserted before a tocall.
bool TAprsString::ValidToCall(string& as)
{
    bool retval = false;
    size_t pIdx;

    if ((pIdx = as.find(',')) > 0)
        toCall = as.substr(0, pIdx);

    if (toCall.substr(0,5) == "RELAY")
        retval = false;
    else if (toCall.substr(0,4) == "WIDE")
        retval = false;
    else if (toCall.substr(0, 6) == "BEACON") {
        aprsType = APRSBEACON;
        retval = true;
    } else if (toCall.substr(0,2) == "ID") {
        aprsType = APRSID;
        retval = true;
    } else if (toCall.substr(0,4) == "MAIL") {
        aprsType = APRSMSG;
        retval = true;
    } else if (toCall.substr(0,2) == "DX") {
        aprsType = APRSMSG;
        retval = true;
    } else
        retval = true;

    return retval;

}

// Validates packet data
//  
// This method looks for a resonably valid payload.
// Whay consitutes a valid payload has been a bone of contention 
// for some time, so we only look at overall length and whether
// a CRLF has been added to the end of the packet.
//
bool TAprsString::ValidData(string& as)
{
    bool retval = true;
    size_t pIdx;

    if (as.length() > 250) {
        retval = false;
        cerr << "Data Error: Data too long..." << endl;
    } else if (as.length() < 3) {
        retval = false;
        cerr << "Data Error: No valid data..." << endl;
    }

    switch (as[0]) {
        case 0x60:                      // These indicate it's a Mic-E packet
            break;
        case 0x27:
            break;
        case 0x1c:
            break;
        case 0x1d:
            break;
        default:
            char c;
            for (int i; i = 0; i++) {
                c = as[i];
                if ((c < 0x20) || (c == 0x127)) {
                    cerr << "Non-printable char in data" << endl;
                    retval = false;
                    return retval;
                }
            }
    }

    if ((pIdx = as.find('\r')) <= as.length()) {
        if ((as[pIdx+1] != '\n') || (as[pIdx-1] != '\n'))
            as.append("\n");
    } else
        as.append("\r\n");

    return retval;
}

// Simply determins the type of APRS packet.
unsigned int TAprsString::GetMsgType(string& as)
{
    unsigned int retval = 0;
    size_t idx;

    switch (as[0]) {
        case '!':                       // Fixed short format
            if (as[1] == '!')
                retval = APRSWX;        // Ultimeter 2000
            else
                retval = APRSPOS;
            break;

        case '@':                       // APRS mobile with messaging
            retval = APRSPOS;
            break;

        case '_':                       // Positionless weather
            retval = APRSWX;
            break;
            
        case '*':                       // Peet Bros WX
            retval = APRSWX;
            break;

        case ':':                       // Message packet
            stsmDest = as.substr(1, MAXPKT);
            idx = stsmDest.find(":");
            if (idx == npos)
                break;

            // Get the adressee
            stsmDest = stsmDest.substr(0, idx);
            if (stsmDest.length() != 9)
                break;

            idx = stsmDest.find_first_of(RXwhite);
            if (idx != npos)
                stsmDest = stsmDest.substr(0, idx);

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
            retval = APRSMSG;
            EchoMask |= src3RDPARTY;
            msgType = APRSMSGTEXT;
            if (data.length() >= 15)
                if (data.substr(10, 4) == string(":ack"))
                    msgType = APRSMSGACK ;

            if (data.substr(10, 2) == string(":?")) {
                idx = data.find('?', 12);
                if (idx != string::npos) {
                    query = data.substr(12, idx - 12);
                    msgType = APRSMSGQUERY;
                    idx = data.find('{', idx);

                    if (idx != string::npos) {
                        acknum = data.substr(idx);
                    }

                    //cout << "Query=" << query << " qidx=" << qidx << endl;  // debug
                }
            }
            retval = APRSMSG;
            break;

        case '=':                       // Fixed station
            retval = APRSPOS;
            break;

        case '>':                       // APRS Status
            retval = APRSSTATUS;
            break;

        case '?':                       // APRS Query
            retval = APRSQUERY;
            break;

        case ';':                       // APRS Object
            retval = APRSOBJECT;
            break;

        case '/':                       // APRS not running, fade to grey in 2hrs
            retval = APRSPOS;
            break;

        case 0x60:                      // These indicate it's a Mic-E packet

        case 0x27:

        case 0x1c:

        case 0x1d:
            retval = APRSMIC_E;
            break;
            
        case 'T':
            retval = APRSTELEMETRY;
            break;

        case '$':
            if (as[1] == 'U')
                retval = APRSWX;        // Ultimeter 2000
            else
                retval = NMEA;
            break;

        default:
            retval = APRSUNKNOWN;
    }
    return retval;
}



// eof - aprsString.cpp
