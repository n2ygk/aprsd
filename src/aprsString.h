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


#ifndef __APRSSTRING_H
#define __APRSSTRING_H

#define MAXPATH 12
#define MAXPKT 256

#include <string>

#include "constant.h"
#include "mutex.h"

using namespace std;
using namespace aprsd;


static const int APRSMSGACK = 31;       // These three are for the msgType variable
static const int APRSMSGTEXT = 32;      // and indicate what kind of message it is.
static const int APRSMSGQUERY = 33;

static const int APRSMIC_E = 20;
static const int APRSOBJECT = 9;
static const int COMMENT = 10;
static const int APRSID = 11;           // Station ID packet eg: WA4DSY>ID......
static const int NMEA = 100;
static const int APRSREFORMATTED = 8;
static const int APRSQUERY = 7;
static const int APRSSTATUS = 6;
static const int APRSTCPXX = 5;
static const int APRSLOGON = 4;
static const int APRSMSG = 3;
static const int APRSWX = 2;
static const int APRSPOS = 1;
static const int APRSUNKNOWN = 0;
static const int APRSERROR = -1;

static const int MAXWORDS = 8;


class TAprsString: public std::string
{
public:
    static int NN;                      // Tells how many objects exist
    static long objCount;               // Total of objects created (used as ID)

    long ID;                            // Unique ID number for this object
    int instances ;                     // Number of pointers to this object that exist in user queues
    int tcpxx;                          // TRUE = TCPXX or TCPXX* was in the path
    int reformatted;                    // TRUE = packet has been reformatted into 3rd party format
    time_t timestamp;                   // time it was created
    int ttl;                            // time to live (minutes)
    INT32 hash;                         // 32 bit hash code of this packet
    time_t timeRF;                      // time this was sent on RF (0 = never)
    TAprsString* next;                  // Only used if this is part of a linked list
    TAprsString* last;                  // ditto..
    int localSource;                    // TRUE = source station is local
    int aprsType;
    int sourceSock;                     // tcpip socket this came from
    int pathSize;
    int EchoMask;                       // Set a bit to indicate where this object came from
    int dest;                           // Where it's going, destTNC or destINET
    size_type dataIdx;                  // index of data part of this packet

    int allowdup;                       // TRUE or FALSE, object is a duplicate (for redundent ACKs)
    int msgType;                        // Indicates if message is an ACK or just text
    time_t lastPositTx;                 // Time of last transmission by POSIT2RF

    string ax25Source;                  // ax25 source
    string ax25Dest ;                   // ax25 destination
    string stsmDest;                    // Station to Station Message destination
    string path;                        // Raw ax25 path  (up to but not including the colon)
    string data;                        // Raw ax25 data (all following first colon)
    string msgdata;
    string msgid;
    string ax25Path[MAXPATH];
    string words[MAXWORDS];
    string user;
    string pass;
    string pgmName;
    string pgmVers;
    string peer;
    string call;
    string query;                       // APRS Query string
    string acknum;                      // The "{nnn" at the end of messages
    string raw;                         // copy of complete raw packet preserved here
    string srcHeader;                   // Source IP and user call header. format example: #192.168.1.1:N0CALL#
    bool AEA;

    TAprsString(const char *cp, int s, int e);
    TAprsString(string& as, int s, int e);
    TAprsString(const char *cp, int s, int e, const char* szPeer, const char *userCall);
    TAprsString(const char *cp);
    TAprsString(string& as);
    TAprsString(TAprsString& as);
    ~TAprsString(void);

    void print(ostream& os);
    string getAX25Source(void);
    string getAX25Dest(void);
    const char* getChar(void);
    int getEchoMask(void);
    void setEchoMask(int m);
    bool queryPath(char* cp);           // Tells if char string cp is in the ax25 path
    bool changePath(const char* newPath, const char* oldPath); //Change one path element
    bool queryLocal(void);              // returns true if source call is local
    void stsmReformat(string& MyCall);
    void mic_e_Reformat(TAprsString** posit, TAprsString** telemetry);
    void printN(void);
    void addInstance(void);
    void removeInstance(void);
    INT32 gethash(void);

    static int getObjCount(void);
private:
    void constructorSetUp(const char *cp, int s, int e);
    void AEAtoTAPR(string& s, string& rs);
    //static pthread_mutex_t pmtxCounters;
    static Mutex mutex;
};

#endif  // __APRSSTRING_H
