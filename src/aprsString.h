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


#ifndef APRSSTRING_H
#define APRSSTRING_H

#include <string>
#include <exception>

#include "constant.h"



#define MAXPKT 256
#define MAXPATH 22    /* longest path we can parse */
#define MAXISPATH 18  /* Largest Internet System path allowed (CALL + DEST + 8 digi + qAc + N other things :) */
#define MAXRFPATH 10 
#define MAXISHOPS (MAXISPATH-MAXRFPATH-1)



//-----------------------------------

//Constants for EchoMask.  Each aprsSting object has this variable.
//These allow each Internet listen port to filter
//data it needs to send.

 static const echomask_t srcTNC = 1;            //data from TNC
 static const echomask_t srcUSER = 2;           //data from any logged on internet user
 static const echomask_t srcUSERVALID = 4;      //data from validated internet user
 static const echomask_t srcIGATE = 8;          //data from another IGATE
 static const echomask_t srcSYSTEM = 16;        //data from this server program
 static const echomask_t srcUDP = 32;           //data from UDP port
 static const echomask_t srcHISTORY = 64;       //data fetched from history list
 static const echomask_t src3RDPARTY = 128;     //data is a 3rd party station to station message
 static const echomask_t srcSTATS = 0x100;      //data is server statistics
 static const echomask_t srcBEACON = 0x200;     //data is from internal beacon for ID
//
 static const echomask_t wantREJECTED = 0x400;   //user wants rejected packets
 static const echomask_t wantSRCHEADER = 0x0800;//User wants Source info header prepended to data
 static const echomask_t wantHTML = 0x1000;     //User wants html server statistics (added in version 2.1.2)
 static const echomask_t wantRAW = 0x2000;      //User wants raw data only
 static const echomask_t sendDUPS = 0x4000;     //if set then don't filter duplicates
 static const echomask_t sendHISTORY = 0x8000;  //if set then history list is sent on connect
 static const echomask_t wantECHO = 0x10000;    //Echo users data back to him
 static const echomask_t omniPortBit = 0x80000000;  //User defines his own echo mask
 //
 static const int APRSMSGACK = 31;        //These three are for the msgType variable
 static const int APRSMSGTEXT = 32;       // and indicate what kind of message it is.
 static const int APRSMSGQUERY = 33; 
 static const int APRSMSGSERVER = 34;    //SERVER command embedded in a message
//-------------------------------------

 //Packet types
 static const int APRSMIC_E = 20;
 static const int APRSOBJECT = 9;
 static const int COMMENT = 10;
 static const int APRSID = 11;  //Station ID packet eg: WA4DSY>ID......
 static const int CONTROL = 12; //User control command to configure his port
 static const int NMEA = 100;
 static const int APRS3RDPARTY = 8;
 static const int APRSREFORMATTED = 8;
 static const int APRSQUERY = 7;
 static const int APRSSTATUS = 6;
 static const int APRSTCPXX = 5;
 static const int APRSLOGON = 4;
 static const int APRSMSG = 3;
 static const int APRSWX = 2;
 static const int APRSPOS = 1;
 static const int APRSUNKNOWN = 0;
 static const int APRSREJECT = -1;

 //Command Types

 static const int CMDNULL = 0;
 static const int CMDPORTFILTER = 1;
 static const int CMDLOCK = 2;
 



       
 static const int MAXWORDS = 8;

 static const unsigned int ttlSize = 5; //String length of a TTL path element
 //Return codes from decrementTTL()
 static const int ttlExpired = -1;
 static const int ttlNotPresent = 1;
 static const int ttlSuccess = 0;
 static const int TTLSIZE = 5;      //Length of TTL or Internet Injection point string

 extern int ttlDefault;    //Make ttlDefault available to other modules
namespace aprsd {
using namespace std;

class aprsString: public string
{  
public:

   static unsigned int NN;             //Tells how many objects exist
   static unsigned long objCount;      //Total of objects created (used as ID)
   //static unsigned int* refCount;
   static pthread_mutexattr_t* mutexAttr;
   static pthread_mutex_t* mutex;

   unsigned long ID;                   //Unique ID number for this object
   int instances ;             //Number of pointers to this object that exist in user queues
   int tcpxx;                 //true = TCPXX or TCPXX* was in the path
   int reformatted;           //true = packet has been reformatted into 3rd party format
   int normalized;            //true = packet has been converted to normal from 3rd party
   bool cci;                  //true = converted from CallSign,I to qAR.
   time_t timestamp;          // time it was created
   int ttl;                   // time to live (minutes) in history list
   INT32 hash;                // 32 bit hash code of this packet
   time_t timeRF;             // time this was sent on RF (0 = never)
   aprsString* next;          //Only used if this is part of a linked list
   aprsString* last;          // ditto..
   int localSource;           //true = source station is local
   int nogate;                //true = do not igate this packet
   int aprsType;              //Type of packet (comment, 3rdparty etc.)
   int cmdType;               //User command code
   bool valid_ax25;           //Validated as a good ax25 packet with PATH and DATA parts
   int sourceSock;            //tcpip socket this came from
   int pathSize;              //Length of the path part in bytes
   int thirdPartyPathSize;    //Length of the 3rd party path in bytes
   echomask_t EchoMask;       //Set a bit to indicate where this object came from
   long FilterMask;           //Mask for USER selected regional data streams
   int dest;                  //Where it's going, destTNC or destINET

   string::size_type gtIdx;           //character position of the ">" symbol
   string::size_type pIdx;            //Character position of the ":" symbol
   string::size_type opIdx;           //Character position of the "(" symbol
   string::size_type cpIdx;           //Character position of the ")" symbol
   string::size_type dataIdx;         //index of data part of this packet, usually pIdx + 1
   int IjpIdx;                //index to path element that has IS Injection point ( qAc )
   int IjpOffset;             //Offset in characters from start of packet of the Injection point
   
  
   int allowdup;              //true or false, object is a duplicate (for redundent ACKs)
   int msgType;               //Indicates if message is an ACK or just text
   //bool Expired;              //True indicates this packet has expired (time-to-Live is zero)
                
   bool AEA;

   string ax25Source;         //ax25 source
   string ax25Dest ;          //ax25 destination
   string stsmDest;           //Station to Station Message destination
   string path;               //Raw ax25 path  (up to but not including the colon)
   string data;               //Raw ax25 data (all following first colon)
   string ax25Path[MAXPATH];
   string thirdPartyPath[MAXPATH];
   string words[MAXWORDS];
   string user;
   string pass;
   string pgmName;
   string pgmVers;
   string peer;
   string call;
   string icall;              //Callsign precceding the "I" in old uiview packets
   string query;              //APRS Query string
   string acknum;             //The "{nnn" at the end of messages
   string raw;                //copy of complete raw packet preserved here
   string srcHeader;          //Source IP and user call header. format example: !192.168.1.1:N0CALL!

   aprsString(const char *cp, int s,int e);
   aprsString(const char *cp, int s,int e, const char* szPeer, const char *userCall);
   aprsString(const char *cp);
   aprsString(string& as);
   aprsString(aprsString& as);
   ~aprsString(void) throw();

   void parseAPRS(void);
   void parseLogon(void);
   bool parseCommand(void);
   bool parsePortFilter(void);
   void aprsString::getMsgText(string& msg);
   void print(ostream& os);
   string getAX25Source(void);
   string getAX25Dest(void);
   const char* getChar(void);
   echomask_t getEchoMask(void);
   void setEchoMask(echomask_t m);

   //Tells if char string cp is in the ax25 path
   unsigned queryPath(const string& s, int start = 0, int stop = -1, unsigned n = npos);

   bool changePath(const char* newPath, const char* oldPath); //Change one path element
   bool addPath(const char* cp, char c = ' ');
   bool addPath(string s, char c = ' ');
   void trimPath(int n);         //Remove last n path elements
   bool queryLocal(void);        //returns true if source call is local
   bool igated(void);            //Returns true if packet has been igated to RF
   bool thirdPartyReformat(const char *call);   //Convert to 3rd party format
   bool thirdPartyToNormal(void);   //Convert 3rd party to normal
   void mic_e_Reformat(aprsString** posit, aprsString** telemetry);
   void printN(void);
   void addInstance(void);
   void removeInstance(void);
   INT32 gethash(INT32 seed = 0);
   bool hasBeenDigipeated(const string& s);
   bool hasBeenIgated(void);
   bool igated(const string& s) ;
   bool pathUsed(const string& s);
   void cutUnusedPath(void) ;
   int decrementTTL(void);
   char getPktSource(void);            //get char code for packet source ( C I R U X Z )
   void addIIPPE( char c = ' ');       //add IIPPE  to path             
   
   string removeI(void);
   void createIIPPE(char* out, int size, char c = ' ') ; // Create Internet Injection Point Path Element
   int getIsHops(void);
   int getMaxHops(void) ;
   int changeIIPPE(const char* qs); 
   int findInjectionPoint(void);
   bool loopCheck(const string& ucall);   //true if ucall is in IS path. * is ignored
    
   static int getObjCount(void);

private:
    void constructorSetUp(const char *cp, int s, int e);
    bool AEAtoTAPR(string& s, string& rs);
} ;
}
#endif      // APESSTRING_H
