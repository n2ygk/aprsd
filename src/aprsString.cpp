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

#include <pthread.h>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <string>
#include <ctime>
#include <stdexcept>
#include <strstream>

#include "aprsString.h"
#include "aprsd.h"
#include "constant.h"
#include "utils.h"
#include "mic_e.h"
#include "crc.h"
#include "aprsdexception.h"

using namespace std;
using namespace aprsd;

pthread_mutex_t* aprsString::mutex = NULL;  // mutex semaphore pointer common to all instances of aprsString
pthread_mutexattr_t* aprsString::mutexAttr = NULL;

//unsigned int* aprsString::NN = new unsigned int(1);
//unsigned long* aprsString::objCount = new unsigned long(1);
unsigned int aprsString::NN = 0;
unsigned long aprsString::objCount = 0;

int ttlDefault = 35;

#define pathDelm ",>:\r\n"
#define cmdDelim ",)\r\n"


aprsString::aprsString(const char* cp, int s, int e, const char* szPeer, const char* userCall) : string(cp)
{
    constructorSetUp(cp,s,e);
    peer = szPeer;
    call = userCall;
    srcHeader = "!" + peer + ":" + call + "!";    //Build the source ip header
}

aprsString::aprsString(const char* cp, int s, int e) : string(cp)
{
    peer = "";
    user = "";
    call = "";
    srcHeader = "!:!";
    constructorSetUp(cp,s,e);
}

aprsString::aprsString(const char* cp) : string(cp)
{
    peer = "";
    user = "";
    call = "";
    srcHeader = "!:!";
    constructorSetUp(cp,0,0);
}


aprsString::aprsString(string& cp) : string(cp)
{
    peer = "";
    user = "";
    call = "";
    srcHeader = "!:!";
    constructorSetUp(cp.c_str(),0,0);
}


//Copy constructor

aprsString::aprsString(aprsString& as)
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
    icall = as.icall;
    pgmName = as.pgmName;
    pgmVers = as.pgmVers;
    peer = as.peer;



    raw = as.raw;
    srcHeader = as.srcHeader;

    for (int i = 0; i < MAXPATH; i++)
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
    nogate = as.nogate;
    reformatted = as.reformatted;
    normalized = as.normalized;
    cci = as.cci;

    valid_ax25 = as.valid_ax25;
    IjpIdx = as.IjpIdx;

    pthread_mutex_lock(mutex);  // Lock the counters !
    //++(*NN);                              // Increment counters
    //++(*objCount);
    NN++;
    objCount++;
    ID = objCount;                     //new serial number
    pthread_mutex_unlock(mutex);  // Unlock counters

    timestamp = time(NULL);  //User current time instead of time in original
    instances = 0;
}





aprsString::~aprsString(void) throw()
{
    pthread_mutex_lock(mutex);
    /*
    //if (--(*refCount) == 0) {
    if (--(*NN) <= 0) {
        pthread_mutex_unlock(mutex);
        pthread_mutex_destroy(mutex);
        pthread_mutexattr_destroy(mutexAttr);
        try {
            delete mutex; mutex = NULL;
            delete mutexAttr; mutexAttr = NULL;
            //delete refCount; refCount = NULL;
            //delete object; object = NULL;
        } catch (...) { cerr << "Error in aprsString destructor; nn = " << (*NN) << endl; }
    } else {
        --(*NN);
        pthread_mutex_unlock(mutex);
    }
    */
    //pthread_mutex_lock(pmtxCounters);
    //Lock countLock(pmtxCounters);
    NN--;
    pthread_mutex_unlock(mutex);
}




void aprsString::constructorSetUp(const char* cp, int s, int e)
{
    try{
        //refCount = new unsigned int(1);



        aprsType = APRSUNKNOWN;     //Default to unknown aprs packet type
        cmdType = CMDNULL;
        dest = 0;
        instances = 0;
        reformatted = false;
        normalized = false;
        allowdup = false;
        msgType = 0;
        sourceSock = s;
        EchoMask = e;
        nogate = false;
        last = NULL;
        next = NULL;
        ttl = ttlDefault;
        timeRF = 0;
        AEA = false;
        acknum = "";
        query = "";
        icall = "";

        valid_ax25 = false;
        cci = false;
        pIdx = 0;
        gtIdx = 0;
        dataIdx = 0;
        IjpIdx = 0;

        //if(pmtxCounters == NULL){              //Create mutex semaphore to protect counters if it doesn't exist...
        //    pmtxCounters = new pthread_mutex_t; //...This semaphore is common to all instances of aprsString.
        //    pthread_mutex_init(pmtxCounters,NULL);
        //}
        if (mutex == NULL) {
            mutexAttr = new pthread_mutexattr_t;
            mutex = new pthread_mutex_t;
            pthread_mutexattr_init(mutexAttr);
            pthread_mutex_init(mutex, mutexAttr);
        }

        pthread_mutex_lock(mutex);
        objCount++;
        ID = objCount;         //set unique ID number for this new object
        NN++;
        pthread_mutex_unlock(mutex);

        timestamp = time(NULL);
        ax25Source = "";
        ax25Dest = "";
        stsmDest = "";
        raw = string(cp);
        pathSize = 0;

        if ((length() <= 0)
                || (find("cmd:",0,4) == 0)
                || (find("EH?",0,3) == 0)) {  //new in 2.1.5, reject EH? and cmd:

            aprsType = APRSREJECT;
            return;
        }

        if (cp[0] == '#') {
            aprsType = COMMENT;
            // print();
            return;
        }

        gtIdx = find(">");       //Find the first ">"
        pIdx = find(":", gtIdx);  //Find the first ":" after the ">"

        if ((gtIdx != npos)       // ">" exists
                && (pIdx != npos)     // ":" exists after the ">"
                &&(gtIdx > 0)         // ">" is not the first character
                && (gtIdx <= 10)       // ">" is not at character position 10 or lower
                && (pIdx > (gtIdx + 1))){  // there is at least 1 char between ">" and ":"
                                 //This could be a valid ax25 packet
            parseAPRS();    // process further
            return;
        }

        data = substr(0,MAXPKT); //Default the data and path to be the whole packet.
        path = data;

        if ((find("user ",0,5) == 0 || find("USER ",0,5) == 0)){    //Must be a logon string
            parseLogon();                 //Process it.
            return;
        }

        opIdx = find("(");
        cpIdx = find(")");

        if ((opIdx != npos) && (cpIdx != npos)) {      //Command string of the form " command(arg1,arg2...) "
            parseCommand();
        }

        return;

    } catch(exception& e) {
        char *errormsg;
        errormsg = new char[501];
        memset(errormsg,0,501);
        std::ostrstream msg(errormsg,500);

        msg << "Caught exception in aprsString: "
            << e.what() << endl
            << " [" << peer << "] " << raw.c_str()
            << endl
            << ends ;

        WriteLog(errormsg,ERRORLOG);
        cerr << errormsg;
        delete errormsg;
        aprsType = APRSREJECT;
        return;
    }
}

//-------------------------------------------------------------
void aprsString::parseAPRS(void)
{
    valid_ax25 = true;
    tcpxx = false;

    path = substr(0,pIdx);  //extract all chars up to the ":" into path
    dataIdx = pIdx+1;       //remember where data starts


    size_type gt2Idx = path.find_last_of(">"); // find last ">" in path

    /* Filter out packets from misconfigured TNCs that put <UI> and [time] in the path */
    if((path.find("<") != npos)
            || (path.find("[") != npos)
            || (path.find(" ") != npos)){

        aprsType = APRSREJECT;
        valid_ax25 = false;
        return;               //Bad things found in path - REJECT
    }


    if ((gtIdx != gt2Idx)&&(gtIdx != npos))  { //This is in AEA TNC format because it has more than 1 ">"
        //cout << "AEA " << *this << endl << flush;
        size_type savepIdx = pIdx;
        string rs;
        bool rv = AEAtoTAPR(*this,rs);    //Replace AEA path with TAPR path

        if (rv == false) {               //New in 2.1.5
            aprsType = APRSREJECT;
            valid_ax25 = false;
            return;               //Conversion failed
        }

        pIdx = rs.find(":");
        string rsPath = rs.substr(0,pIdx);
        replace(0,savepIdx,rsPath);
        path = rsPath;

        AEA = true;
    }

    if ((pIdx+1) < length())
        data = substr(pIdx+1,MAXPKT);  //The data portion of the packet

    for (int i = 0; i < MAXPATH; i++)
        ax25Path[i] = "";

    pathSize = split(path ,ax25Path,MAXPATH,pathDelm);

    if (pathSize >= 2)
        ax25Dest = ax25Path[1];             //AX25 destination

    if (pathSize >= 1)
        ax25Source = ax25Path[0];           //AX25 Source

    findInjectionPoint();      //Set IjpIdx and IjpOffset  (Points to IIPPE )
                              //Also sets tcpxx if reqired

    if (data.length() == 0)
        return;

    size_type idx,qidx,aidx;

    if(ax25Dest.compare("ID") == 0){      //ax25 ID packet
        aprsType = APRSID;
        return;
    }

    switch (data[0]) {
        case ':' :    /* station to station message, new 1998 format*/
                  /* example of string in "data" :N0CLU    :ack1   */
            stsmDest = data.substr(1,MAXPKT);
            idx = stsmDest.find(":");
            if (idx == npos)
                break;

            stsmDest = stsmDest.substr(0,idx);
            if (stsmDest.length() != 9)
                break;

            idx = stsmDest.find_first_of(RXwhite);
            if (idx != npos)
                stsmDest = stsmDest.substr(0,idx);
                        
            aprsType = APRSMSG;
            EchoMask |= src3RDPARTY;
            msgType = APRSMSGTEXT;
            if (stsmDest.compare("SERVER") == 0) 
                msgType = APRSMSGSERVER;
            
            aidx = data.find_last_of('{');
            if (aidx != npos) 
                acknum = data.substr(aidx+1);

            if (data.length() >= 15)
                if (data.substr(10,4) == string(":ack")) 
                    msgType = APRSMSGACK ;
                          
            if (data.substr(10,2) == string(":?")){
                qidx = data.find('?',12);
                if (qidx != string::npos) {
                    query = data.substr(12,qidx-12);
                    msgType = APRSMSGQUERY;
                    qidx = data.find('{',qidx);
                    if (qidx != string::npos) {
                        acknum = data.substr(qidx);
                    }
                    //cout << "Query=" << query << " qidx=" << qidx << endl;
                }
            }
            break;
        
        case '_' :           
            aprsType = APRSWX;
            break;/* weather */
   
        case '@' :                    /* APRS mobile station */
        case '=' :                    /* APRS fixed station */
        case '!' :                    /* APRS not runing, fixed short format */
        case '/' :
            aprsType = APRSPOS;  /* APRS not running, fade to gray in 2 hrs */
            break;
        
        case '>' :
            aprsType = APRSSTATUS;
            break;
            
        case '?' :
            qidx = data.find('?',1);
            if (qidx != string::npos) {
                query = data.substr(1,qidx-1);
                aprsType = APRSQUERY;
            }
            break;
            
        case ';' :
            aprsType = APRSOBJECT;
            break;

        case 0x60:          //These indicate it's a Mic-E packet
        case 0x27:
        case 0x1c:
        case 0x1d:
            aprsType = APRSMIC_E;
            break;
            
        case '}' :           
            {  
                aprsType = APRS3RDPARTY;
                //Extract the 3rd party path data
                string tp_path =  data.substr(1,data.find(":",1));
                thirdPartyPathSize = split(tp_path , thirdPartyPath,MAXPATH,pathDelm);
                //reformatted = true;
                //string temp = data.substr(1,MAXPKT);
                // aprsString reparse(temp);
                // aprsType = reparse.aprsType;
                //print(cout);
                break;
            }
        
        case '$' :
            aprsType = NMEA;
            break;

        /* check for messages in the old format */
        default:
            if (data.length() >= 10) {  
                if ((data.at(9) == ':') && isalnum(data.at(0))) {
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
                            //cout << "Query=" << query << " qidx=" << qidx << endl;
                        }
                    }
                }
                if ((data.at(9) == '*') && isalnum(data.at(0))) { 
                    aprsType = APRSOBJECT;
                }
            }
            break;
    }; /* end switch */

    /*
        mark packets for certain path elements (TCPXX, RFONLY, NOGATE) between the
        destination call and the Internet Injection Point (q string).
    */
    int stop,start;
    start = 2;
    if (IjpIdx > 0) 
        stop = IjpIdx-1; 
    else 
        stop = pathSize-1;

    if (queryPath("TCPXX",start,stop,5) != npos)
        tcpxx = true;        //If TCPXX or TCPXX* don't allow Inet->RF gating
                           // qAX is checked elsewhere in findInjectionPoint()

    nogate = false;
    if (queryPath("NOGATE",start,stop) != npos)
        nogate = true;   //If NOGATE or RFONLY don't allow RF->Inet gating
   
    if (queryPath("RFONLY",start,stop) != npos) 
        nogate = true;
}

//-------------------------------------------------------------
void aprsString::parseLogon(void)
{

    for (int i = 0; i < MAXWORDS; i++) 
        words[i] = "";

    int n = split(*this,words,MAXWORDS,RXwhite);

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
   
    EchoMask = 0;      //Don't echo any logon strings
    aprsType = APRSLOGON;
}

//-------------------------------------------------------------
//Puts the message text into msg.
void aprsString::getMsgText(string& msg)
{
    if (aprsType == APRSMSG) {
        msg = data.substr(11);
        size_type ackidx = msg.find_last_of('{');
        if(ackidx != npos) 
            msg.erase(ackidx);  //Remove {001 ack number
    } else 
        msg = "*";
}

//-------------------------------------------------------------

// Packets of the form "command(arg1,arg2,arg3,...)" are processed here.

bool aprsString::parseCommand(void)
{ 
    bool rv = false;

    string cmd = substr(0,opIdx); //cmd is everything up to first "("

    upcase(cmd);
 
    if ((cmd.compare("PORTFILTER") == 0 ) || cmd.compare("PF") == 0) 
        rv = parsePortFilter();

    if (cmd.compare("LOCK") == 0 ){ 
        rv = true; 
        cmdType = CMDLOCK; 
    }

    aprsType = CONTROL;
    return rv;
}
//------------------------------------------------------------

//User selects his port filter parameters here.

bool aprsString::parsePortFilter(void)
{ 
    int i,n;
    bool rv = false;
    string arg;

    string args = substr(opIdx+1) ;
    int nw = split(args,words,MAXWORDS,cmdDelim);
    cmdType = CMDPORTFILTER;

    n = 0;
    EchoMask = 0;
    for (i = 0; i < nw; i++) {    //Go through the arg list
        arg = words[i];       //Grab an argument from the list
        upcase(arg);          //Convert to uppercase
                           //See if it's anything we recognize below...

        if ((arg.compare("ALL") == 0)
                || (arg.compare("FULL") == 0)
                || (arg.compare("23") == 0)
                ||  (arg.compare("10152") == 0)){
        
            EchoMask |= srcTNC | srcIGATE | srcUDP
                   | srcUSER | srcUSERVALID | srcSYSTEM | srcBEACON;
            n++;
            continue;
        }

        if ((arg.compare("1313") == 0) || (arg.compare("LINK") == 0)) {
            EchoMask |= srcTNC | srcUSER | srcUSERVALID | srcUDP | srcBEACON;
            n++;
            continue;
        }

        if ((arg.compare("14579") == 0) || (arg.compare("LOCAL") == 0)) {
            EchoMask |=  srcTNC | srcUDP | srcSYSTEM | srcBEACON;
            n++;
            continue;
        }

        if ((arg.compare("1314") == 0) || (arg.compare("MSG") == 0)) {
            EchoMask |=  src3RDPARTY;
            n++;
            continue;
        }

        if ((arg.compare("-HUB") == 0) 
                || (arg.compare("-SERVER") == 0)
                || (arg.compare("-IGATE") == 0)) {

            EchoMask &= ~srcIGATE;
            n++;
            continue;
        }

        if (arg.compare("-TNC") == 0) {    
            EchoMask &= ~srcTNC; 
            n++; 
            continue;
        }
     
        if (arg.compare("-USER") == 0) {   
            EchoMask &= ~(srcUSER | srcUSERVALID); 
            n++ ; 
            continue;  
        }

        if (arg.compare("-SYSTEM") == 0) { 
            EchoMask &= ~srcSYSTEM; 
            n++ ;
            continue;
        }
     
        if (arg.compare("-BEACON") == 0) { 
            EchoMask &= ~srcBEACON; 
            n++;
            continue;
        }

        if (arg.compare("ECHO") == 0) {
            EchoMask |= wantECHO; 
            n++;
            continue;
        }
     
        if ((arg.compare("REJECT") == 0) || (arg.compare("14503") == 0))  {  
            EchoMask |= wantREJECTED; 
            n++ ;
            continue;
        }

        if ((arg.compare("HEADER") == 0) || (arg.compare("14502") == 0)) {
            EchoMask |= wantSRCHEADER | srcUSER | srcUSERVALID | srcTNC | srcUDP | srcIGATE;
            n++ ;
            continue;
        }

        if (arg.compare("RAW") == 0) {     
            EchoMask |= wantRAW | srcTNC ; 
            n++ ;
            continue;
        }
     
        if (arg.compare("DUP") == 0) {     
            EchoMask |= sendDUPS ; 
            n++ ;
            continue;
        }
     
        if (arg.compare("TNC") == 0) {     
            EchoMask |=  srcTNC; 
            n++;
            continue;
        }
     
        if (arg.compare("UDP") == 0) {     
            EchoMask |= srcUDP;  
            n++;
            continue;
        }

        if (arg.compare("USER") == 0){     
            EchoMask |= srcUSERVALID | srcUSER ;
            n++;
            continue;
        }
     
        if (arg.compare("SERVER") == 0){   
            EchoMask |= srcIGATE; 
            n++;
            continue;
        }
     
        if (arg.compare("HUB") == 0){      
            EchoMask |= srcIGATE;  
            n++;
            continue;
        }
     
        if (arg.compare("IGATE") == 0){    
            EchoMask |= srcIGATE;  
            n++;
            continue;
        }
     
        if (arg.compare("SYSTEM")== 0){    
            EchoMask |= srcSYSTEM | srcBEACON;
            n++; 
            continue; 
        }
     
        if (arg.compare("STATS") == 0){    
            EchoMask |= srcSTATS;  
            n++;
            continue;
        }
     
        if (arg.compare("HISTORY") == 0){  
            EchoMask |= sendHISTORY;  
            n++;
            continue;
        }

        if (arg.compare("CLEAR") == 0){ 
            EchoMask = 0; 
            n++; 
        }
    }

    rv = (n == nw);        //Check for error
    
    if (!rv) 
        EchoMask = 0;  //if error then clear echomask.
  
    return rv;
}

//-------------------------------------------------------------
bool aprsString::AEAtoTAPR(string& s, string& rs)
{  
    bool rv;
    string pathElem[MAXPATH+2];
    string pathPart, dataPart;

    try {
        size_type pIdx = find(":");

        dataPart = s.substr(pIdx+1,MAXPKT);
        pathPart = s.substr(0,pIdx+1);
        int n = split(pathPart,pathElem,MAXPATH+2,pathDelm);
        rs  = pathElem[0] + '>' + pathElem[n-1];

        for (int i = 1; i < n-1; ++i) 
            rs = rs + ',' + pathElem[i];
            
        rs = rs + ':' + dataPart;
        rv = true;
    }catch(exception& error) {
        rv = false;
        WriteLog("AEA to TAPR conversion error",ERRORLOG);
        WriteLog(s.c_str(),ERRORLOG);
    }
    return rv;
}


//--------------------------------------------------------------

/* This is for debugging only.
   It dumps important variables to the console */

void aprsString::print(ostream& os)
{
    string s_ref = "FALSE";
    string s_igated = "FALSE";
    string s_digipeated = "FALSE";

    if (reformatted) 
        s_ref = "TRUE";
   
    if (hasBeenIgated()) 
        s_igated = "TRUE";

    os << *this << endl
        << "Serial No. = " << ID << endl
        << "TCPIP source socket = " << sourceSock << endl
        << "Packet type = " << aprsType << endl
        << "Reformatted = " << s_ref << endl
        << "Has Been Igated = " << s_igated << endl
        << "EchoMask = " << EchoMask << endl
        << "Peer= " << peer << endl
        << "User Call= " << call << endl;

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
    os << endl << ends;
}

//----------------------------------------------------------------
/*returns the string as a char*
*/

const char* aprsString::getChar(void)
{
    return c_str();
}

echomask_t aprsString::getEchoMask(void)
{
    return EchoMask;
}

void aprsString::setEchoMask(echomask_t m)
{
    EchoMask = m;
}

string aprsString::getAX25Source(void)
{
    return ax25Path[0];
}

string aprsString::getAX25Dest(void)
{
    return ax25Path[1];
}



//---------------------------------------------------------------------------
bool aprsString::queryLocal(void)
{
    bool localSource = false;

    if (valid_ax25 == false) 
        return false;

    if (valid_ax25 == false) 
        return false;
   
    if ((EchoMask & srcTNC)
            && (path.find("GATE*") == npos )
            && (freq(path,'*') < 3))
        localSource = true;
    
    return localSource;
}

//-------------------------------------------------------------------------------

//Search 1st n chars of ax25path elements for match with string s
// starting at path element "start" and ending at "stop"
//If n not present compare complete string.
//Returns index of path element if match found
// or npos is not found.

unsigned aprsString::queryPath(const string& s, int start, int stop  , unsigned n)
{
    unsigned  rc = npos;
  
    if (valid_ax25 == false) 
        return rc;
  
    if (stop == -1) 
        stop = pathSize-1;
  
    if (start >= pathSize) 
        return rc;

    if (stop < start) 
        return rc;

    for (int i = start; i <= stop; i++){
#if (__GNUC__ >= 3) || (STLport)
        if (s.compare(0,n,ax25Path[i]) == 0){
        rc = i;
        break;
    }
#else
        if (s.compare(ax25Path[i],0,n) == 0){
            rc = i;
            break;
        }
#endif
    }
    return rc;
}


//-------------------------------------------------------
//Add character string "s" + "c" to the ax25 path
bool aprsString::addPath(string s, char c)
{
    if (aprsType == APRSREJECT) 
        return false;

    if (pathSize == MAXPATH) 
        return false;
  
    if (valid_ax25 == false) 
        return false;

    try {
        if (c != ' ') 
            s = s + c;
        
        path = path + "," + s;   //Add string to path part
        replace(0,dataIdx-1,path);        //Replace path portion of complete packet
        dataIdx = path.length() + 1;      //adjust dataIdx to point to moved data beginning
        ax25Path[pathSize++] = s ; //Update path array and pathSize
    } catch(exception& error){ 
        return false;
    }
    return true;
}

//-----------------------------------------------------
bool aprsString::addPath(const char *cp, char c)
{
    string s = cp;
    return addPath(s,c);
}


//-----------------------------------------------------
//Return max hops
int aprsString::getMaxHops(void)
{
    return MAXISHOPS;
}


//------------------------------------------------------

//Return 0 if packet does not have any injection point
//or  else return the path index where it's located.
//Injection point is indicated by a path element with "q" as first char.

int aprsString::findInjectionPoint(void)
{
    if ((aprsType == APRSREJECT)
            || (pathSize == MAXPATH)
            || (valid_ax25 == false))
        return -1;

    bool done = false;
    int i = 2;
    
    while(( i < pathSize) && !done) {
        if (ax25Path[i][0] == 'q') 
            done = true;
      
        if (!done) 
            i++;
    }

    if ((i < pathSize) && (i >= 2)){
        IjpIdx = i;
        IjpOffset = find(ax25Path[i],0);

        if ((ax25Path[IjpIdx][2] == 'X')       //qAX - unvalidated user
                || (ax25Path[IjpIdx][2] == 'Z'))   //qAZ - Zero hops allowed
            tcpxx = true;                      //Mark for NO RF gating

        return i;
    } else 
        return 0;
}

//------------------------------------------------------
/* If IIPPE (q string) is present change it to new string in qs */
int aprsString::changeIIPPE(const char* qs)
{
    if (IjpIdx == 0) 
        return 0;   //No q string to change

    cerr << "qs=" << qs << "   at IjpIdx=" << ax25Path[IjpIdx] << endl;
    string oldq = ax25Path[IjpIdx];
    return changePath(qs, oldq.c_str());
}


//-------------------------------------------------------
//Convert a packet to 3rd party format if it's not
// already in 3rd party format.  Return true on success.

bool aprsString::thirdPartyReformat(const char *mycall)
{ 
    char *co;
    char out[BUFSIZE+1];

    if (valid_ax25 == false) 
        return false;      //If it isn't AX25 do not attempt conversion
  
    if (aprsType == APRS3RDPARTY) 
        return false; //Refuse to convert existing 3rd party packets

    memset(out, 0, BUFSIZE+1);

    std::ostrstream os(out,BUFSIZE);
    
    co = ":";

    if ((aprsType == APRSMSG) && (data.at(0) != ':')) 
        co = "::"; //convert to new msg format if old

    os <<  "}" << ax25Source
        <<  ">" << ax25Dest
        << ",TCPIP*," <<  mycall << "*"
        << co << data << ends;

    data = out;
    reformatted = true;
    normalized = false;
    aprsType = APRS3RDPARTY;
    return true;
}

//----------------------------------------------------------

//Convert 3rd party format packet to normal format (inverse of thirdPartyReformat above)
bool aprsString::thirdPartyToNormal(void)
{
    if (valid_ax25 == false) 
        return false;
   
    if (aprsType != APRS3RDPARTY) 
        return false;

    string Icall = removeI();   //If there was a CALLSIGN,I in path remove it an keep CALLSIGN in Icall

    string d = data.substr(1,data.length());   //Keep only the data after ":}"

    //Use data to create a new aprsString
    aprsString* temp =  new aprsString(d.c_str(),sourceSock,EchoMask,peer.c_str(),call.c_str());

    if (temp->aprsType == APRSREJECT){           //Check for validity
        aprsType = APRSREJECT;                   //Flag as error if defective

        return false;                            //Do not copy if defective
    }

    temp->reformatted = false;  //Indicate it's not a 3rd party packet
    temp->normalized = true;    //Indicate it's been converted to normal format

    *this = *temp;      //Copy data into this aprsString object
    delete temp;        //Delete the local temporary object

    if (Icall.length() > 0){  //Re-insert the CALLSIGN,I construct
        if (Icall[Icall.length()-1] == '*')
            addPath(Icall);
        else
            addPath(Icall,'*');  //Reinsert CALLSIGN and * if not present

        addPath("I");      //Add the "I" path element
    }
    return true;
}



//---------------------------------------------------------

//checks for string s = any of:  TCPIP*,  TCPXX*,  I* , I.
bool aprsString::igated(const string& s)
{
    if (s.compare("TCPIP*") == 0) 
        return true;

    if (s.compare("TCPIP") == 0) 
        return true;
    
    if (s.compare("I") == 0) 
        return true;
    
    if (s.compare("I*") == 0) 
        return true;
    
    if (s.compare("TCPXX*") == 0) 
        return true;
    
    if (s.compare("TCPXX") == 0) 
        return true;
    
    if (s[0] == 'q') 
        return true;

    return false;
}

//---------------------------------------------------------

//Returns true if 3rd party path indicates this has been igated to RF from Internet.

bool aprsString::hasBeenIgated(void)
{
    int i;
    bool rv = false;
    
    if (valid_ax25 == false) 
        return false;

    if (aprsType == APRS3RDPARTY){      //Scan the 3rd party header for igated flags
        i = thirdPartyPathSize - 1;
        while ((i > 0) && (rv == false)){
            rv = igated(thirdPartyPath[i--]);
        }
    }
    return rv;
}

//--------------------------------------------------------
//Returns true if string s is  a path element that's been used.
bool aprsString::pathUsed(const string& s)
{
    int sz = s.size();

    if (s[sz-1] == '*') 
        return true; //Last char is an '*' ?

    if(sz == 7){        //Is it WIDEn-n format?
#if (__GNUC__ >= 3) || (STLport)
        if ((s.compare(0,4,"WIDE"))  //1st 4 chars are "WIDE"
                && (s[5] == '-')        // 6th char is "-"
                && (s[4] >= '0')        // 5th char is'0' or higher
                && (s[4] <= '7')){      // 5th char is 7 or lower
            if (s[4] > s[6]) 
                return true;   //if 5th char greater than 7th char ret true
        }
#else
        if((s.compare("WIDE",0,4))  //1st 4 chars are "WIDE"
                && (s[5] == '-')        // 6th char is "-"
                && (s[4] >= '0')        // 5th char is'0' or higher
                && (s[4] <= '7')){      // 5th char is 7 or lower
            if (s[4] > s[6]) 
                return true;   //if 5th char greater than 7th char ret true
        }
#endif
    }
    return false;
}
//--------------------------------------------------------

//Starting at the last ax25 path element work backward
//removing elements until a used element is found.

void aprsString::cutUnusedPath(void)
{
    int i = pathSize -1;
    
    if (valid_ax25 == false) 
        return;

    while ((i > 1) && (pathUsed(ax25Path[i]) == false)) 
        i--;

    pathSize = i + 1;  //New path size
    path = ax25Path[0] + ">";

    for (i = 1; i < pathSize; i++){    //Now rebuild the path
        path = path + ax25Path[i];
        if (i < (pathSize-1))
            path = path + ',';
    }

    replace(0,dataIdx-1,path);        //Replace path portion of complete packet
    dataIdx = path.length() + 1;      //adjust dataIdx to point to moved data beginning
}

//-----------------------------------------------------------
//Remove the last n path elements
void aprsString::trimPath(int n)
{
    int i;
    pathSize = pathSize - n;         //Reduce path size by n

    if (pathSize < 2) 
        pathSize = 2;   // Do not trim the destination field!
      
    path = ax25Path[0] + ">";        //Start path rebuild

    for (i = 1; i < pathSize; i++){    //Now rebuild the rest of the path
        path = path + ax25Path[i];
        if (i < (pathSize-1)) 
            path = path + ',';
    }

    replace(0,dataIdx-1,path);        //Replace path portion of complete packet
    dataIdx = path.length() + 1;      //adjust dataIdx to point to moved data beginning
}
//------------------------------------------------------------


//If the last path element it "I" or "I*" remove it and the preceeding path element.
//Return the callsign preceeding the "I".

string aprsString::removeI(void)
{
    if (pathSize < 4) 
        return "";

    string sI = ax25Path[pathSize-1];  //sI is last path element

    if (sI.size() <= 2){
        if ((sI.compare("I") == 0) || (sI.compare("I*") == 0)){
            icall = ax25Path[pathSize-2];   //Get the callsign before th "I"
            trimPath(2);
            cci = true;         //Set "Converted Callsign I" flag true
        }
    }

    return icall;   //Return the callsign  or empty string
}
//----------------------------------------------------------
char aprsString::getPktSource(void)
{
    if(EchoMask & srcUSERVALID) {
        if (cci){                           //Converted from callsign,I construct
            if(call.compare(icall) == 0)
                return 'R';                      //call preceeding I == login so return R
            else
                return 'r';                      //Else return r
        }
                                        //No callsign,i in packet
        if (ax25Source.compare(call) == 0)
            return 'C';                      //return C if login call matches ax25 FROM call
        else
            return 'S';                      //Else return S for all others

    }

    if (EchoMask & srcIGATE){            //From an aprs Server or Hub
        if (cci)
            return 'r';                      //I conversion to qAr
        else 
            return 'S';                    //No I so return S

    }

    if (EchoMask & srcTNC) 
        return 'R' ;     //From RF (TNC)

    if (EchoMask & srcUSER) 
        return 'X';     //From non-validated client
   
    if (EchoMask & srcUDP) 
        return 'U';      //From UDP port
   
    if (EchoMask & srcBEACON) 
        return 'I';   //From system Internal beacon
   
    if (EchoMask & srcSYSTEM) 
        return 'Z';   //Zero hop for system status messages

    return 'Z';  //Zero hop        DEFAULT
}

//-----------------------------------------------------------
//Create  Internet Injection Point Path Element
//If c parameter present it replaces char  from getPktSource().
void aprsString::createIIPPE(char* out, int size, char c )
{
    if (c == ' ') 
        c = getPktSource();
    
    ostrstream os(out,size);
    os << "qA" << c  << ends; //Create IIPPE
}

//------------------------------------------------------------

//Add IIPPE to ax25 path.  If the c parameter
//is present it replaces the character from getPacketSource().

void aprsString::addIIPPE(char c )
{
    char IIPPE[12];           //Where IIPPE string will be
    createIIPPE(IIPPE,11,c);  //Create IIPPE string
    addPath(IIPPE);           //Add it to end of path
    findInjectionPoint();     //Update indexs and offsets
}


//-----------------------------------------------------------

//Converts mic-e packets to 1 or 2 normal APRS packets
//Returns pointers to newly allocated aprsStrings
//Pointers remain NULL if conversion fails.

void aprsString::mic_e_Reformat(aprsString** posit, aprsString** telemetry)
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
            std::ostrstream pbuf(buf1,511);
            pbuf <<  path << ':' << mic1 << "\r\n" << ends;
            aprsString* Posit = new aprsString(buf1,sourceSock,EchoMask,peer.c_str(),call.c_str());
            Posit->raw = string(raw);  // Save a copy of the raw mic_e packet
            Posit->changePath(PGVERS,ax25Dest.c_str());
            delete buf1;
            *posit = Posit;
        }
        
        if (l2){
            char *buf2 = new char[512];
            memset(buf2,0,512);
            ostrstream tbuf(buf2,511);
            tbuf <<  path << ':' << mic2 << "\r\n" << ends;
            aprsString* Telemetry = new aprsString(buf2,sourceSock,EchoMask,peer.c_str(),call.c_str());
            Telemetry->raw = string(raw);  // Save a copy of the raw mic_e packet
            Telemetry->changePath(PGVERS,ax25Dest.c_str());
            delete buf2;
            *telemetry = Telemetry;
        }
    }
}

//---------------------------------------------------------

//Find path element oldPath and replace with newPath
//Returns true if success.

bool aprsString::changePath(const char* newPath, const char* oldPath)
{
    bool rc = false;
    int i;

    if (valid_ax25 == false) 
        return false;

    try {
        for (i = 0; i < pathSize; i++)
            if (ax25Path[i].compare(oldPath) == 0 ) {
                ax25Path[i] = newPath;
                size_type idx = find(oldPath);
                replace(idx,strlen(oldPath),newPath);
                path.replace(idx,strlen(oldPath),newPath);
                dataIdx = find(":") + 1;    //Update offset to data part
                rc = true;
                if (i==1) 
                    ax25Dest = newPath;
               
                if (i==0) 
                    ax25Source = newPath;

                break;
            }
    } catch(exception& error) {
        rc = false;
        WriteLog("aprsString changePath error",ERRORLOG);
        WriteLog(oldPath,ERRORLOG);
    }
    return rc;
}

//-----------------------------------------------------------

void aprsString::printN(void)
{
    cout << "N=" << NN << endl;
}


//-----------------------------------------------------------------
//This is a hash function used for duplicate packet detection

INT32 aprsString::gethash(INT32 seed)
{
    int i;
    INT32 crc32 = seed;

    int datalen = data.length();   //Length of ax25 information field
    
    if (datalen >= 2) 
        datalen -= 2; //Don't include CR-LF in length

    if (datalen > 0){
        while ((data[datalen-1] == ' ') && (datalen > 0)) 
            datalen--;  //Strip off trailing spaces
    }

    int len = length();
    int slen = ax25Source.length();
    int destlen = ax25Dest.length();

    if ((aprsType == COMMENT) || (valid_ax25 == false)){     //hash the whole packet, else...
        for (i = 0; i < len; i++) 
            crc32 = updateCRC32(c_str()[i],crc32);
    } else {                  //hash only ax25Source and data. Ignore path.
        if (slen > 0)      
            for (i = 0; i < slen; i++) 
                crc32 = updateCRC32(ax25Source.c_str()[i],crc32);

        //If it's a Mic-E also hash the ax25 Destination (really Longitude).
        if ((aprsType == APRSMIC_E) && (ConvertMicE == false))
            for (i = 0; i < destlen; i++) 
                crc32 = updateCRC32(ax25Dest.c_str()[i],crc32);

        if (datalen > 0) {   
            for(i = 0; i < datalen; i++){
                char c = data.c_str()[i];
                if ((c == 0) || (c == 0x0d) || (c == 0x0a)){
                    i = datalen;      //Ignore NULLs, Cr,LF in data
                } else {
                    crc32 = updateCRC32(c , crc32);
                }
            }   //end for()
        }

        crc32 ^= dest ;   // "dest" value is 1 or 2 depending on if destTNC or destINET.
    }                       // This is done so the same packet can be sent to both the TNC and Internet
                           // queues and be dup checked without a false positive.
    hash = crc32;
    return crc32 ;   //Return 32 bit hash value
}

//-------------------------------------------
//Returns the number of aprsString objects that currently exist.
int aprsString::getObjCount()
{  
    int n = 0;
    //Lock countCount(pmtxCounters);
    pthread_mutex_lock(mutex);
    n = NN;
    pthread_mutex_unlock(mutex);
    //cerr << "getObjCount: n = " << n << endl;
    //cerr << "getObjCount: nn = " << NN << endl;
    return n;
}

//----------------

