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


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <fstream>
#include <iostream>
#include <strstream>

#include "constant.h"
#include "cpqueue.h"
#include "aprsString.h"

using namespace std;

/*-------------------------------------------------------*/

//Build a Queue object

cpQueue::cpQueue(int n, bool d)
{
    overrun = 0;
    size = n;
    dyn = d;
    base_p = new queueData[size];
    write_p = 0;
    read_p = 0;
    itemsQueued = 0;
    HWItemsQueued = 0;
    pmtxQ = new pthread_mutex_t;
    pthread_mutex_init(pmtxQ,NULL);
    lock = 0;
    inRead = 0;
    inWrite = 0;

    for (int i=0; i < size; i++) {
        base_p[i].qcp = NULL;           // NULL all the pointers
        base_p[i].qcmd = 0;
        base_p[i].rdy = false;          // Clear the ready flags
    }
}

//Destroy Queue object
//
cpQueue::~cpQueue(void)
{
    lock = 1;

    while (inRead);
    while (inWrite);

    if (dyn) {
        while (read_p != write_p)
            if (base_p[read_p].qcp != NULL)
                delete (TAprsString*)base_p[read_p++].qcp ;
    }
    pthread_mutex_destroy(pmtxQ);
    delete pmtxQ;
    pmtxQ = NULL;
    delete base_p;
    base_p = NULL;
}


// Place a pointer and an Integer in the queue
// returns 0 on success;
//
int cpQueue::write(char *cp, int n)
{
    int rc=0;

    if (lock)
        return -2;                      // Lock is only set true in the destructor

    if(pthread_mutex_lock(pmtxQ) != 0)
    	cerr << "Unable to lock pmtxQ - cpQueue:Write-char *cp.\n" << flush;
    inWrite = 1;
    int idx = write_p;

    if (base_p[idx].rdy == false) {     // Be sure not to overwrite old stuff
        base_p[idx].qcp = (void*)cp;    // put char* on queue
        base_p[idx].qcmd = n;           // put int (cmd) on queue
        base_p[idx].rdy = TRUE;         // Set the ready flag
        idx++;
        itemsQueued++;
	if (itemsQueued > HWItemsQueued)
	    HWItemsQueued = itemsQueued;

        if (idx >= size)
            idx = 0;

        write_p = idx;
    } else {
        overrun++ ;

        if (dyn)
            delete[] cp;
            cp = NULL;

        rc = -1;
    }

    inWrite = 0;
    if(pthread_mutex_unlock(pmtxQ) != 0)
    	cerr << "Unable to unlock pmtxQ - cpQueue:Write - char *cp.\n" << flush;
    return(rc);
}

int cpQueue::write(unsigned char *cp, int n)
{
    return(write((char*)cp, n));
}

/*
int cpQueue::write(string& cs, int n)
{
    int rc = 0;

    if (lock)
        return -2;

    inWrite = 1;
    if(pthread_mutex_lock(pmtxQ) != 0)
    	cerr << "Unable to lock pmtxQ - cpQueue:Write - string.\n" << flush;
    int idx = write_p;
    if (base_p[idx].rdy == false) {     // Be sure not to overwrite old stuff
        base_p[idx].qcp = (char *)cs;	// put String on queue
        base_p[idx].qcmd = n;           // put int (cmd) on queue
        base_p[idx].rdy = TRUE;         // Set the ready flag
        itemsQueued++;
        idx++;

        if (idx >= size)
            idx = 0;

        write_p = idx;
    } else {
        overrun++ ;

        if (dyn)
            cs = NULL;                  // Delete the object that couldn't be put in the queue

        rc = -1;
    }

    inWrite = 0;
    if(pthread_mutex_unlock(pmtxQ) != 0)
    	cerr << "Unable to unlock pmtxQ - cpQueue:Write - string.\n" << flush;
    return(rc);
}

*/

int cpQueue::write(TAprsString* cs, int n)
{
    int rc = 0;

    if (lock)
        return -2;

    if(pthread_mutex_lock(pmtxQ) != 0)
    	cerr << "Unable to lock pmtxQ - cpQueue:write - TAprsString.\n" << flush;
    inWrite = 1;
    int idx = write_p;
    if (base_p[idx].rdy == false) {     // Be sure not to overwrite old stuff
        base_p[idx].qcp = (void*)cs;	// put String on queue
        base_p[idx].qcmd = n;           // put int (cmd) on queue
        base_p[idx].rdy = TRUE;         // Set the ready flag
        itemsQueued++;
	if (itemsQueued > HWItemsQueued)
	    HWItemsQueued = itemsQueued;
        idx++;

        if (idx >= size)
            idx = 0;

        write_p = idx;
    } else {
        overrun++ ;

        if (dyn) {
            delete cs;                  // Delete the object that couldn't be put in the queue
            cs = NULL;
        }

        rc = -1;
    }

    inWrite = 0;
    if(pthread_mutex_unlock(pmtxQ) != 0)
    	cerr << "Unable to unlock pmtxQ - cpQueue:write - TAprsString.\n" << flush;
    return(rc);
}


int cpQueue::write(TAprsString* cs)
{
    return(write(cs, cs->sourceSock));
}

//Read a pointer and Integer from the Queue
//
void* cpQueue::read(int *ip)
{


    if(pthread_mutex_lock(pmtxQ) != 0)
    	cerr << "Unable to lock pmtxQ - cpQueue:read - int.\n" << flush;
    inRead = 1;

    void* cp = base_p[read_p].qcp ;     // Read the TAprsString*

    if(ip)
        *ip = base_p[read_p].qcmd ;     // read the optional integer command

    base_p[read_p].qcp = NULL;          // Set the data pointer to NULL
    base_p[read_p].rdy = false;         // Clear ready flag
    read_p++;
    itemsQueued--;

    if (read_p >= size)
        read_p = 0;

    inRead = 0;

    if(pthread_mutex_unlock(pmtxQ) != 0)
    	cerr << "Unable to unlock pmtxQ - cpQueue:read - int.\n" << flush;
    return(cp);
}

//returns TRUE if data available
//
int cpQueue::ready(void)
{
    int rc=false;
    if(pthread_mutex_lock(pmtxQ) != 0)
    	cerr << "Unable to lock pmtxQ - cpQueue:ready.\n" << flush;
    //if ((read_p != write_p) || wrap) rc = true ;

    if(base_p[read_p].rdy)
        rc = true;

    if(pthread_mutex_unlock(pmtxQ) != 0)
    	cerr << "Unable to unlock pmtxQ - cpQueue:ready.\n" << flush;
    return(rc);

}

int cpQueue::getWritePtr(void)
{
    return(write_p);
}

int cpQueue::getReadPtr(void)
{
    return(read_p);
}

int cpQueue::getItemsQueued(void)
{
    int inq;
    if(pthread_mutex_lock(pmtxQ) != 0)
    	cerr << "Unable to lock pmtxQ - cpQueue:getItemsQueued.\n" << flush;
    inq = itemsQueued;
    if(pthread_mutex_unlock(pmtxQ) != 0)
    	cerr << "Unable to unlock pmtxQ - cpQueue:getItemsQueued.\n" << flush;
    return(inq);
}

int cpQueue::getHWItemsQueued(void)
{
    int HWinq;
    if(pthread_mutex_lock(pmtxQ) != 0)
    	cerr << "Unable to lock pmtxQ - cpQueue:getItemsQueued.\n" << flush;
    HWinq = HWItemsQueued;
    if(pthread_mutex_unlock(pmtxQ) != 0)
    	cerr << "Unable to unlock pmtxQ - cpQueue:getItemsQueued.\n" << flush;
    return(HWinq);
}

int cpQueue::getQueueSize(void)
{
    int Qsize;
    if(pthread_mutex_lock(pmtxQ) != 0)
    	cerr << "Unable to lock pmtxQ - cpQueue:getItemsQueued.\n" << flush;
    Qsize = size;
    if(pthread_mutex_unlock(pmtxQ) != 0)
    	cerr << "Unable to unlock pmtxQ - cpQueue:getItemsQueued.\n" << flush;
    return(Qsize);
}



