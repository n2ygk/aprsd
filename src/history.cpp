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


/*This code maintains a linked list of aprsString objects called the
  "History List".  It is used to detect and remove duplicate packets
  from the APRS stream, provide a 30 minute history of activity to
  new users when they log on and determine if the destination of a
  3rd party station to station message is "local".
  */



#include <cstdio>
//#include <unistd.h>
#include <cstdlib>
#include <string>
#include <ctime>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <pthread.h>

#include <fstream>
#include <iostream>
#include <strstream>
#include <iomanip>
#include <cerrno>
#include <netinet/in.h>
#include <cassert>


#include "history.hpp"
#include "aprsd.hpp"
#include "utils.hpp"
#include "aprsString.hpp"
#include "servers.hpp"
#include "mutex.hpp"
#include "constant.hpp"

using namespace std;
using namespace aprsd;

struct histRec {                  //This is for the History Array
    int count;
    time_t timestamp;
    echomask_t EchoMask;
    int type;
    bool reformatted;
    char *data;
};

class HistoryRecord
{
public:
    HistoryRecord() throw();
    ~HistoryRecord() throw();
    int count;
    time_t timestamp;
    echomask_t EchoMask;
    int type;
    bool reformatted;
    char* data;
};

aprsString *pHead, *pTail;

int ItemCount;
Mutex histMutex;
Mutex dupMutex;

int dumpAborts = 0;



//---------------------------------------------------------------------
void CreateHistoryList()
{
    pHead = NULL;
    pTail = NULL;
    ItemCount = 0;
}

//---------------------------------------------------------------------
//Adds aprs packet to history list
//Returns true on success, false on failure

bool AddHistoryItem(aprsString *hp)
{
    Lock histLock(histMutex);

    if (hp == NULL)
        return false;

    if (pTail == NULL) {        // Starting from empty list
        pTail = hp;
        pHead = hp;
        hp->next = NULL;
        hp->last = NULL;
        ItemCount++;
    } else {                    // List has at least one item in it.
        DeleteItem(hp);         // Delete any previously stored items with same call and type
        if (ItemCount == 0) {   // List is empty, put in first item
            pTail = hp;
            pHead = hp;
            hp->next = NULL;
            hp->last = NULL;
        } else  {               // list not empty...
            pTail->next = hp;   // link the last item to us
            hp->last = pTail;   // link us to last item
            hp->next = NULL;    // link us to next item which is NULL
            pTail = hp;         // link pTail to us
        }

        ItemCount++;
    }
    return true;
}


//---------------------------------------------------------------------
//Don't call this from anywhere except DeleteOldItems().
void DeleteHistoryItem(aprsString *hp)
{
    if (hp == NULL)
        return;

    aprsString *li = hp->last;
    aprsString *ni = hp->next;

    if (hp != NULL) {
        if (li != NULL)
            li->next = ni;

        if (ni != NULL)
            ni->last = li;

        if (hp == pHead)
            pHead = ni;

        if (hp == pTail)
            pTail  = li;

        delete hp;          // Delete the aprsString object
        ItemCount--;        // Reduce ItemCount by 1
    }
    return;
}

//-----------------------------------------------------------------------
//Call this once per 5 minutes
//This reduces the ttl field by x then deletes items when the ttl field is zero or less.
int DeleteOldItems(int x)
{
    int delCount = 0;
    Lock histLock(histMutex);

    if ((pHead == NULL) || (pHead == pTail))
        return 0;  //empty list

    aprsString *hp = pHead;
    aprsString *pNext;

    while (hp) {
        hp->ttl = hp->ttl - x; //update time to live
        pNext = hp->next;

        if (hp->ttl <= 0 ) {
            DeleteHistoryItem(hp);
            delCount++;
        }
        hp = pNext;
    }
    return delCount;
}

//-------------------------------------------------------------------------
//Deletes an aprsString object matching the ax25Source call sign and packet type
// of the "ref" aprsString.  The destination (destTNC or destINET) must also match.
// pmtxHistory mutex must be locked when calling this.
int DeleteItem(aprsString* ref)
{
    int delCount = 0;

    if ((pHead == NULL) || (pHead == pTail) || (ref == NULL))
        return 0;

    aprsString *hp = pHead;
    aprsString *pNext;

    while (hp != NULL) {
        pNext = hp->next;
        if ((hp->aprsType == ref->aprsType ) && (hp->dest == ref->dest)) {
            if (hp->ax25Source.compare(ref->ax25Source) == 0) {
                //cerr << "deleteing " << hp->getChar() << flush;
                DeleteHistoryItem(hp);
                delCount++ ;
            }
        }
        hp = pNext;
    }
    return delCount;
}

//-------------------------------------------------------------------------
/* Check history list for a packet whose data matches that of "*ref"
   that was transmitted less or equal to "t" seconds ago.  Returns true
   if duplicate exists.  Scanning stops when a packet older than "t" seconds
   is found.
*/
/*
bool DupCheck(aprsString* ref, time_t t)   // Now obsolete and not used in version 2.1.2, June 2000
{	int x = -1;
	bool z;
   time_t tNow,age;

   if (ref->allowdup) return false;

	if ((pTail == NULL) || (pHead == NULL)) return false;  //Empty list


   pthread_mutex_lock(pmtxDupCheck);        //Grab semaphore

   time(&tNow);                             //get current time

	aprsString *hp = pTail;       //Point hp to last item in list
	age = tNow - hp->timestamp;



   while((hp != NULL) && (x != 0) && (age <= t))
		{

        age = tNow - hp->timestamp;
        if((ref->ax25Source.compare(hp->ax25Source) == 0)
             && (ref->dest == hp->dest))  //check for matching call signs  and destination (TNC or Internet)
            {
              if(ref->data.compare(hp->data) == 0) x = 0;  //and matching data
            }

			hp = hp->last;									//Go back in time through list
                                              //Usually less than 10 entrys need checking for 30 sec.
		}

  if (x == 0) z = true; else z = false;
  pthread_mutex_unlock(pmtxDupCheck);
  return z;
 }
 */

//--------------------------------------------------------------------------

//Scan history list for source callsign which exactly matches *cp .
//If callsign is present and GATE* is not in the path and hops are 3 or less then return true
//The code to scan for "GATE* and hops have been moved to aprsString.queryLocal() .

bool StationLocal(const string& sp, int em)
{
    bool retval = false;
    Lock histLock(histMutex);

    if ((pTail == NULL) || (pHead == NULL))
        return retval;                           // Empty list

    if (ItemCount == 0)                         // if no data then...
        return retval;                           // ...return false

    aprsString *hp = pTail;                     // point to end of history list

    while ((!retval) && (hp != NULL)) {      // Loop while no match and not at beginning of list
        if (hp->EchoMask & em) {
            if (hp->ax25Source.compare(sp) == 0)    // Find the source call sign
                retval = hp->queryLocal();           // then see if it's local
	}
        hp = hp->last;
    }
    return retval;                                   // return true or false
}


//------------------------------------------------------------------------
/* Returns number of stations in history list defined as "Local" */

int localCount()
{
    int localCounter = 0;
    Lock histLock(histMutex);

    if ((pTail == NULL) || (pHead == NULL))
        return 0;  //Empty list

    if (ItemCount == 0)                     // if no data then...
        return 0;                           // ...return zero

    aprsString *hp = pTail;                 // point to end of history list

    while (hp != NULL) {                    // Go through the whole list
        if ( hp->queryLocal())
            localCounter++;                 // Increment conter if local

        hp = hp->last;
    }
    return localCounter;                    // return number of local staitons
}


//------------------------------------------------------------------------
//Returns a new aprsString of the posit packet whose ax25 source call
//matches the "call" arg and echomask bit matches a bit in "em".
//Memory allocated for the returned aprsString  must be deleted by the caller.
aprsString* getPosit(const string& call, int em)
{
    Lock histLock(histMutex);   // create a semaphore object

    if ((pTail == NULL) || (pHead == NULL))
        return NULL ;                       // Empty list

    aprsString* posit = NULL;

    if (ItemCount == 0)                     // if no data then...
        return NULL;                        // ...return NULL

    aprsString *hp = pTail;                 // point to end of history list

    while ((posit == NULL) && (hp != NULL)) {   // Loop while no match and not at beginning of list
        if (hp->EchoMask & em) {
            if ((hp->ax25Source.compare(call) == 0) // Find the source call sign
                    && ((hp->aprsType == APRSPOS)
                    || hp->aprsType == NMEA)) {     // of a posit packet
                posit = new aprsString(*hp);        // make a copy of the packet
            }
        }
        hp = hp->last;
    }
    return posit;
}


//-------------------------------------------------------------------------

/* timestamp the timeRF variable in an aprsString object in the history list
   given the objects serial number.  Return true if object exists. */
bool timestamp(unsigned long sn, time_t t)
{
    bool x = false;
    Lock histLock(histMutex);

    if ((pTail == NULL) || (pHead == NULL))
        return false ;  //Empty list

    if (ItemCount == 0)                        //if no data then...
        return false;                        // ...return false

    aprsString *hp = pTail;                      //point to end of history list

    while ((x == false) && (hp != NULL)) {   //Loop while no match and not at beginning of list
        if (hp->ID == sn) {
            hp->timeRF = t;       //time stamp it.
            x = true;             //indicate we did it.
        }
        hp = hp->last;
    }
    return x;
}

//--------------------------------------------------------------------------
/* Deletes the history array and it's data created above */

void deleteHistoryArray(histRec* hr)
{
    int i;

    if (hr) {
        int arraySize = hr[0].count;
        for (i = 0;i<arraySize;i++) {
            if(hr[i].data != NULL)
                delete hr[i].data;
        }
        delete hr;
    }
}


//-------------------------------------------------------------------------
/* This reads the history list into an array to facilitate sending
the data to logged on clients without locking the History linked list
for long periods of time.

Note: Must be called with pmtxHistory locked !
*/

histRec* createHistoryArray(aprsString* hp)
{
    int i;

    if (hp == NULL)
        return NULL;

    histRec* hr = new histRec[ItemCount];    //allocate memory for array

    if (hr == NULL)
        return NULL;

    for (i = 0; i < ItemCount; i++)
        hr[i].data = NULL;

    for (i = 0; i < ItemCount; i++) {
        hr[i].count = ItemCount - i;
        hr[i].timestamp = hp->timestamp;
        hr[i].EchoMask = hp->EchoMask;
        hr[i].type = hp->aprsType;
        hr[i].reformatted = hp->reformatted;
        hr[i].data = strdup(hp->getChar());    //Allocate memory and copy data

        if (hr[i].data == NULL) {
            deleteHistoryArray(hr);   //Abort if malloc fails
            return NULL;
        }

        if (hp->next == NULL)
            break;

        hp = hp->next;
    }
    return hr;
}

//--------------------------------------------------------------------------
//Send the history items to the user when he connects
//returns number of history items sent or -1 if an error in sending occured
int SendHistory(int session, int em)
{
    int rc,count, i=0;
    int retrys;
    timespec ts;
    Lock histLock(histMutex, false);

    if (pHead == NULL)
        return 0;       //Nothing to send

    histLock.get();

    aprsString *hp = pHead;             //Start at the beginning of list
    histRec* hr = createHistoryArray(hp); //copy it to an array

    histLock.release();

    if (hr == NULL)
        return 0;

    int n = hr[0].count;                // n has total number of items
    count = 0;

    //float throttle = 32;    //Initial rate  about 250kbaud (1/baud * 8 * 1e6)

    /* New smart rate throttle for history dump */
    float maxspeed = 1e6/(MaxLoad - serverLoad);  //This much BW remaining...
    if (maxspeed > 833)
        maxspeed = 833; // No slower than 9600 baud

    if (maxspeed < 32)
        maxspeed = 32; // No faster than 250k baud.

    float throttle = maxspeed;      //Initial rate is MaxLoad - serverLoad bytes/sec

    for (i = 0; i < n; i++) {
        if ((hr[i].EchoMask & em) && (hr[i].reformatted == false)) {
            //filter data intended for this users port
            count++;                //Count how many items we actually send
            size_t dlen  = strlen(hr[i].data);
            retrys = 0;

            do {
                rc = send(session, (const void*)hr[i].data, dlen, 0); //Send history list item to client
                ts.tv_sec = 0;
                ts.tv_nsec = (int)throttle * dlen * 1000;  //Delay time in nano seconds
                nanosleep(&ts, NULL);   //pace ourself

                if (rc < 0) {
                    ts.tv_sec = 1;
                    ts.tv_nsec = 0;
                    nanosleep(&ts, NULL);     //Pause output 1 second if resource unavailable
                    if (retrys == 0) {
                        throttle = throttle * 2;
                    } //cut our speed in half

                    if (throttle > 833)
                        throttle = 833;        //Don't go slower than 9600 baud

                    retrys++;
                } else
                    if (throttle > maxspeed)
                        throttle = throttle * 0.98; //Speed up 2%
            } while ((errno == EAGAIN) && (rc < 0) && ( retrys <= 180));  //Keep trying for 3 minutes

            if (rc < 0) {
                cerr <<  "send() error in SendHistory() errno= " << errno << " retrys= " << retrys
                    << " \n[" << strerror(errno) <<  "]" << endl;
                deleteHistoryArray(hr);

                histLock.get();
                dumpAborts++;
                histLock.release();

                return -1;
            }
        }
    }
    deleteHistoryArray(hr);
    return count;
}
//---------------------------------------------------------------------
//Save history list to disk
int SaveHistory(const string& name)
{
    int icount = 0;
    Lock histLock(histMutex);

    if (pHead == NULL)
        return icount;

    ofstream hf(name.c_str());      // Open the output file

    if (hf.good()) {        //if no open errors go ahead and write data
        aprsString *hp = pHead;
        for (;;) {
            if (hp->EchoMask){           //Save only items which have a bit set in EchoMask
                hf << hp->ttl  << " "
                    << hp->timestamp << " "
                    << hp->EchoMask << " "
                    << hp->aprsType << " "
                    << hp->getChar() ;  //write to file

                if (!hf.good()) {
                    cerr << "Error writing " << SAVE_HISTORY << endl;
                    break;
                }
                icount++;
            }
            if (hp->next == NULL)
                break;

            hp = hp->next;  //go to next item in list
        }
        hf.close();
    }

    return icount;
}
//---------------------------------------------------------------------

int ReadHistory(const string& name)
{
    int icount = 0;
    int expiredCount = 0;
    int ttl,EchoMask,aprsType;
    char *data = new char[256];
    time_t now,timestamp;

    ifstream hf(name.c_str());          // Create ifstream instance "hf" and open the input file

    if (hf.good()) {            //if no open errors go ahead and read data
        now = time(NULL);
        while (hf.good() ) {
            hf >> ttl  >> timestamp >> EchoMask >> aprsType;
            hf.get();           // skip 1 space
            hf.get(data, 252, '\r');  // read the rest of the line as a char string
            int i = strlen(data);

            data[i++] = '\r';
            data[i++] = '\n';
            data[i++] = '\0';

            aprsString *hist = new aprsString(data);

            hist->EchoMask = EchoMask;
            hist->timestamp = timestamp;

            ttl = ttl - ((now - timestamp) / 60); //Adjust timep-to-live field
            if (ttl < 0)
                ttl = 0;

            hist->ttl = ttl;
            hist->aprsType = aprsType;

            //Now add to list only items which have NOT expired
            if (ttl > 0) {
                AddHistoryItem(hist);
                icount++;
            } else {
                delete hist;
                expiredCount++;
            }
        }
        hf.close();
    }
    delete data;
    cout << "Read " << icount+expiredCount << "  items from "
         << name  << endl;

    if (expiredCount)
        cout  << "Ignored " << expiredCount
            << " expired items in "
            << name << endl;

    return icount;
}

//-------------------------------------------------------------------------------------
