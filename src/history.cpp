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


// July 1998.  Completely revised this code to deal with aprsSting objects
//instead of History structures.

/*
    This code maintains a linked list of TAprsString objects called the
    "History List".  It is used to detect and remove duplicate packets
    from the APRS stream, provide a 30 minute history of activity to
    new users when they log on and determine if the destination of a
    3rd party station to station message is "local".
*/


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <pthread.h>
#include <errno.h>
#include <unistd.h>
#include <assert.h>

#include <fstream>
#include <iostream>
#include <strstream>
#include <iomanip>
#include <cstdlib>

#include "osdep.h"
#include "constant.h"
#include "utils.h"
#include "history.h"
#include "mutex.h"

using namespace std;
using namespace aprsd;

struct histRec {                        // This is for the History Array
    int count;
    time_t timestamp;
    int EchoMask;
    int type;
    int reformatted;
    char *data;
};

Mutex mtxHistory;
Mutex mtxDupCheck;

TAprsString *pHead, *pTail;

int ItemCount;

extern const int srcTNC;
extern int ttlDefault;

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
bool AddHistoryItem(TAprsString *hp)
{
    Lock historyLock(mtxHistory, false);

    if (hp == NULL)
        return(1);

    historyLock.get();

    if (pTail == NULL) {                // Starting from empty list
        pTail = hp;
        pHead = hp;
        hp->next = NULL;
        hp->last = NULL;
        ItemCount++;
    } else {                            // List has at least one item in it.
        DeleteItem(hp);                 // Delete any previously stored items with same call and type

        if (ItemCount == 0) {           // List is empty, put in first item
            pTail = hp;
            pHead = hp;
            hp->next = NULL;
            hp->last = NULL;
        } else {                        // list not empty...
            pTail->next = hp;           // link the last item to us
            hp->last = pTail;           // link us to last item
            hp->next = NULL;            // link us to next item which is NULL
            pTail = hp;                 // link pTail to us
        }
        ItemCount++;
    }
    historyLock.release();
    return(0);
}

//---------------------------------------------------------------------
//Don't call this from anywhere except DeleteOldItems().
void DeleteHistoryItem(TAprsString *hp)
{
    if (hp == NULL)
        return;

    TAprsString *li = hp->last;
    TAprsString *ni = hp->next;

    if (hp != NULL) {
        if (li != NULL)
            li->next = ni;

        if (ni != NULL)
            ni->last = li;

        if (hp == pHead)
            pHead = ni;

        if (hp == pTail)
            pTail  = li;

        delete hp;                      // Delete the TAprsString object
        hp = NULL;
        ItemCount--;                    // Reduce ItemCount by 1
    }
    return;
}


//-----------------------------------------------------------------------
//Call this once per 5 minutes
//This reduces the ttl field by x then deletes items when the ttl field is zero or less.
int DeleteOldItems(int x)
{
    int delCount = 0;
    Lock historyLock(mtxHistory, false);

    if ((pHead == NULL) || (pHead == pTail))
        return(0);                      // empty list

    historyLock.get();

    TAprsString *hp = pHead;
    TAprsString *pNext;

    while (hp) {
        hp->ttl = hp->ttl - x;          // update time to live
        pNext = hp->next;

        if (hp->ttl <= 0 ) {
            DeleteHistoryItem(hp);
            delCount++;
        }

        hp = pNext;
    }

    historyLock.release();

    return(delCount);
}


//-------------------------------------------------------------------------
//  Deletes an TAprsString object matching the ax25Source call sign and packet type
//  of the "ref" TAprsString.  The destination (destTNC or destINET) must also match.
//  pmtxHistory mutex must be locked when calling this.
int DeleteItem(TAprsString* ref)
{
    int delCount = 0;

    if ((pHead == NULL) || (pHead == pTail) || (ref == NULL))
        return(0);

    TAprsString *hp = pHead;
    TAprsString *pNext;

    while (hp != NULL) {
        pNext = hp->next;
        if ((hp->aprsType == ref->aprsType )
                && (hp->dest == ref->dest)) {

            if(hp->ax25Source.compare(ref->ax25Source) == 0) {
                //cerr << "deleteing " << hp->getChar() << flush;
                DeleteHistoryItem(hp);
                delCount++ ;
            }
        }

        hp = pNext;
    }
    return(delCount);
}

//-------------------------------------------------------------------------
// Finds the last time a position was sent by the POSIT2RF handling (lastPositTx variable).

time_t GetLastPositTx(TAprsString* ref) {   // is this not called anywhere???
    Lock historyLock(mtxHistory, false);

    if ((pHead == NULL) || (pHead == pTail) || (ref == NULL))
        return 0;

    historyLock.get();

    TAprsString *hp = pHead;
    TAprsString *pNext;

    while(hp != NULL) {
        pNext = hp->next;
        if ((hp->aprsType == ref->aprsType) && (hp->dest == ref->dest)) {
            if(hp->ax25Source.compare(ref->ax25Source) == 0) {
                historyLock.release();
                return hp->lastPositTx;
            }
        }

        hp = pNext;
    }
    historyLock.release();
    return 0;
}

//--------------------------------------------------------------------------
//  Scan history list for source callsign which exactly matches *cp .
//  If callsign is present and GATE* is not in the path and hops are 3 or less then return TRUE
//  The code to scan for "GATE* and hops have been moved to TAprsString.queryLocal() .
//
bool StationLocal(const char *cp, int em)
{
    bool z = false;
    Lock historyLock(mtxHistory, false);

    if ((pTail == NULL) || (pHead == NULL))
        return(z);                      // Empty list

    historyLock.get();
    //cout << cp << " " << em << endl;  // debug
    if (ItemCount == 0) {               // if no data then...
        historyLock.release();
        return(z);                      // ...return false
    }

    TAprsString *hp = pTail;             // point to end of history list

    while ((z == false) && (hp != NULL)) {  // Loop while no match and not at beginning of list
        if (hp->EchoMask & em) {
            if (hp->ax25Source.compare(cp) == 0)    // Find the source call sign
                z = hp->queryLocal();   // then see if it's local
        }

        hp = hp->last;
    }
    historyLock.release();
    return(z);                          // return TRUE or false
}


//------------------------------------------------------------------------
//  Returns a new TAprsString of the posit packet whose ax25 source call
//  matches the "call" arg and echomask bit matches a bit in "em".
//  Memory allocated for the returned TAprsString  must be deleted by the caller.
TAprsString* getPosit(const string& call, int em)
{
    Lock historyLock(mtxHistory, false);

    if ((pTail == NULL) || (pHead == NULL))
        return NULL ;                   // Empty list

    TAprsString* posit = NULL;

    historyLock.get();

    if (ItemCount == 0) {               // if no data then...
        historyLock.release();
        return NULL;                    // ...return NULL
    }

    TAprsString *hp = pTail;             // point to end of history list

    while ((posit == NULL) && (hp != NULL)) {   // Loop while no match and not at beginning of list
        if (hp->EchoMask & em) {
            if ((hp->ax25Source.compare(call) == 0)                             // Find the source call sign
                    && ((hp->aprsType == APRSPOS) || hp->aprsType == NMEA)) {   // of a posit packet

                posit = new TAprsString(*hp);        // make a copy of the packet
            }
        }

        hp = hp->last;
    }

    historyLock.release();
    return(posit);
}

//------------------------------------------------------------------------
//  Returns a new aprsString of the posit packet whose ax25 source call
//  matches the "call" arg and echomask bit matches a bit in "em".
//  Memory allocated for the returned aprsString  must be deleted by the caller.
//  It will return a position from the history list which was last transmitted
//  before earliestTime, by checking the lastPositTx field. Then it will update
//  the lastPositTx field.
TAprsString* getPositAndUpdate(const string& call, int em, time_t earliestTime, time_t newTime)
{
    Lock historyLock(mtxHistory, false);

    if ((pTail == NULL) || (pHead == NULL))
        return NULL ;  //Empty list

    TAprsString* posit = NULL;

    historyLock.get();
    if (ItemCount == 0) {                      //if no data then...
        historyLock.release();
        return NULL;                        // ...return NULL
    }

    TAprsString *hp = pTail;                      //point to end of history list

    while ((posit == NULL) && (hp != NULL)) {    //Loop while no match and not at beginning of list
        if(hp->EchoMask & em) {
            if((matchCallsign(hp->ax25Source, call))    //Find the source call sign
                && ((hp->aprsType == APRSPOS) || hp->aprsType == NMEA)   //of a posit packet
                && (hp->lastPositTx < earliestTime)) {

                posit = new TAprsString(*hp);    //make a copy of the packet
                hp->lastPositTx = newTime;      //update last TX time

            }
        }

        hp = hp->last;

    }
    historyLock.release();
    return posit;

}


//-------------------------------------------------------------------------
//  timestamp the timeRF variable in an TAprsString object in the history list
//  given the objects serial number.  Return TRUE if object exists.
//
bool timestamp(long sn, time_t t)
{
    bool x = false;
    Lock historyLock(mtxHistory, false);

    if ((pTail == NULL) || (pHead == NULL))
        return false;                   // Empty list

    historyLock.get();
    if (ItemCount == 0) {               // if no data then...
        historyLock.release();
        return false;                       // ...return false
    }

    TAprsString *hp = pTail;             // point to end of history list

    while ((x == false) && (hp != NULL)) {      // Loop while no match and not at beginning of list
        if (hp->ID == sn) {
            hp->timeRF = t;             // time stamp it.
            x = true;                   // indicate we did it.
        }
        hp = hp->last;
    }
    historyLock.release();
    return(x);
}


//--------------------------------------------------------------------------
//  Deletes the history array and it's data created above
//
void deleteHistoryArray(histRec* hr)
{
    int i;

    if (hr) {
        int arraySize = hr[0].count;

        for (i = 0;i<arraySize;i++) {
            if (hr[i].data != NULL) {
                free(hr[i].data);
                hr[i].data = NULL;
            }
        }
        delete[] hr;
        hr = NULL;
    }
}


//-------------------------------------------------------------------------
//  This reads the history list into an array to facilitate sending
//  the data to logged on clients without locking the History linked list
//  for long periods of time.
//
//  Note: Must be called with pmtxHistory locked !
//

histRec* createHistoryArray(TAprsString* hp)
{
    int i;

    if (hp == NULL)
        return NULL;

    histRec* hr = new histRec[ItemCount];   // allocate memory for array

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
        hr[i].data = strdup(hp->getChar());     // Allocate memory and copy data

        if (hr[i].data == NULL) {
            deleteHistoryArray(hr);     // Abort if malloc fails
            return NULL;
        }

        if (hp->next == NULL)
            break;

        hp = hp->next;
    }
    return(hr);
}

//--------------------------------------------------------------------------
//  Send the history items to the user when he connects
//  returns number of history items sent or -1 if an error in sending occured
//
int SendHistory(int session, int em)
{
    int rc,count,bytesSent, i=0;
    int retrys, lastretry;
    Lock historyLock(mtxHistory, false);

    if (pHead == NULL)
        return(0);                      // Nothing to send

    historyLock.get();
    TAprsString *hp = pHead;            // Start at the beginning of list
    histRec* hr = createHistoryArray(hp);   // copy it to an array
    historyLock.release();

    if (hr == NULL)
        return 0;

    int n = hr[0].count;                // n has total number of items
    count = 0;
    bytesSent = 0;
    float throttle = 150;                // Initial rate  about 50kbaud
    for (i = 0; i < n; i++) {
        if ((hr[i].EchoMask & em) && (hr[i].reformatted == false)) {
            // filter data intended for this users port
            count++;                    // Count how many items we actually send
            size_t dlen  = strlen(hr[i].data);
            retrys = lastretry = 0;

            do {
                rc = send(session, (const void*)hr[i].data, dlen, 0);  // Send history list item to client
                usleep(((int)throttle * dlen) * 10);       // pace ourself
                bytesSent += dlen;              // used for average size calculations
                if (rc < 0) {
                    if (errno == EAGAIN)        // only sleep on overrun
                        usleep(1000000);        // Pause output 1 second if resource unavailable

                    if (retrys > lastretry) {   // original version would only throttle down once.
                        lastretry = retrys;
                        throttle = (throttle * 2); //cut our speed in half
                    }

                    if (throttle > 3300) {
                        throttle = 3300;    // Don't go slower than 2400bps line speed (abt 1800bps payload)
                        cerr << "SendHistory: Throttled at minimum." << endl;
                    }
                    retrys++;
                } else
                    if (throttle > 30) {   // max speed of about 250kbaud
                        throttle = throttle * 0.98;     // Speed up 2%
                    }

            } while((errno == EAGAIN) && (rc < 0) && ( retrys <= 90));  //Keep trying for 1.5 minutes

            if ((rc < 0) || (retrys >= 90)) {
                cerr <<  "send() error in SendHistory() errno= " << errno << " retrys= " << retrys
                    << " \n[" << strerror(errno) <<  "]" << endl;

                historyLock.get();
                deleteHistoryArray(hr);
                dumpAborts++;
                historyLock.release();
                return(-1);
            }
        }
    }
    historyLock.get();
    deleteHistoryArray(hr);
    historyLock.release();

    return(count);
}

//---------------------------------------------------------------------
//  Save history list to disk
//
int SaveHistory(char *name)
{
    int icount = 0;
    Lock historyLock(mtxHistory, false);

    if (pHead == NULL)
        return 0;

    historyLock.get();
    ofstream hf(name);                  // Open the output file

    if (hf.good()) {                    // if no open errors go ahead and write data
        TAprsString *hp = pHead;

        for (;;) {
            if (hp->EchoMask) {         // Save only items which have a bit set in EchoMask
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

            hp = hp->next;              // go to next item in list
        }
        hf.close();
    }
    historyLock.release();
    return icount;
}


//---------------------------------------------------------------------
int ReadHistory(char *name)
{
    int icount = 0;
    int expiredCount = 0;
    int ttl,EchoMask,aprsType;
    char *data = new char[256];
    time_t now, timestamp;

    ifstream hf(name);                  // Create ifstream instance "hf" and open the input file

    if (hf.good()) {                    // if no open errors go ahead and read data
        now = time(NULL);
        while (hf.good()) {
            hf >> ttl  >> timestamp >> EchoMask >> aprsType;
            hf.get();                   // skip 1 space
            hf.get(data, 252, '\r');      // read the rest of the line as a char string
            int i = strlen(data);

            data[i++] = '\r';
            data[i++] = '\n';
            data[i++] = '\0';

            TAprsString *hist = new TAprsString(data);

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
                hist = NULL;
                expiredCount++;
            }
        }

        hf.close();
    }

    delete[] data;
    cout << "Read " << icount+expiredCount << "  items from " << name  << endl;

    if (expiredCount)
        cout << "Ignored " << expiredCount
            << " expired items in "
            << name << endl;

    return(icount);
}

// eof: history.cpp
