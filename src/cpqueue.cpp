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



#include <unistd.h>
#include <pthread.h>
#include <stdio.h>
#include <fstream>
#include <iostream>
#include <strstream>
#include <iomanip>

#include "constant.hpp"
#include "cpqueue.hpp"
#include "aprsString.hpp"

using namespace std;
using namespace aprsd;

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
    lock = 0;
    inRead = 0;
    inWrite = 0;

    for (int i = 0; i < size; i++) {
        base_p[i].qcp = NULL;           // NULL all the pointers
        base_p[i].qcmd = 0;
        base_p[i].rdy = false;          // Clear the ready flags
    }
}

//Destroy Queue object

cpQueue::~cpQueue(void)
{
    lock = 1;

    while(inRead);
    while(inWrite);

    if (dyn) {
        while (read_p != write_p)
            if (base_p[read_p].qcp != NULL)
                delete (aprsString*)base_p[read_p++].qcp ;
    }
    delete base_p;
}


//Place a pointer and an Integer in the queue
//returns 0 on success;
int cpQueue::write(char *cp, int n)
{
    int rc = 0;
    Lock qlock(Q_mutex, false);

    if (lock)
        return -2;  //Lock is only set true in the destructor

    inWrite = 1;
    qlock.get();
    int idx = write_p;

    if (base_p[idx].rdy == false) {         // Be sure not to overwrite old stuff
        base_p[idx].qcp = (void*)cp;        // put char* on queue
        base_p[idx].qcmd = n;               // put int (cmd) on queue
        base_p[idx].rdy = true;             // Set the ready flag
        idx++;
        itemsQueued++;
        if (idx >= size)
            idx = 0;

        write_p = idx;
    } else {
        overrun++ ;
        if (dyn) {
            delete[] cp;      //Added [] brackets 14-July-2001 wa4dsy
            cp = NULL;        //Added set to NULL 14-July-2001 wa4dsy
        }
        rc = -1;
    }

    qlock.release();
    inWrite = 0;
    return rc;
}

int cpQueue::write(unsigned char *cp, int n)
{
    return write((char*)cp, n);
}

int cpQueue::write(const char* cp, int n)
{
    return write((char*)cp, n);
}


int cpQueue::write(aprsString* cs, int n)
{
    int rc = 0;
    Lock qlock(Q_mutex, false);

    if (lock)
        return -2;

    inWrite = 1;
    qlock.get();
    int idx = write_p;

    if (base_p[idx].rdy == false) {         // Be sure not to overwrite old stuff
        base_p[idx].qcp = (void*)cs;        // put String on queue
        base_p[idx].qcmd = n;               // put int (cmd) on queue
        base_p[idx].rdy = true;             // Set the ready flag
        itemsQueued++;
        idx++;

        if (idx >= size)
            idx = 0;

        write_p = idx;
    }else {
        overrun++ ;
        if (dyn){
            delete cs;                      // Delete the object that couldn't be put in the queue
            cs = NULL;
        }
        rc = -1;
    }

    qlock.release();
    inWrite = 0;
    return rc;
}


int cpQueue::write(aprsString* cs)
{
    return write(cs, cs->sourceSock);
}

//Read a pointer and Integer from the Queue

void* cpQueue::read(int *ip)
{
    Lock qlock(Q_mutex, false);

    inRead = 1;

    qlock.get();
    void* cp = base_p[read_p].qcp ;     // Read the aprsString*

    if (ip)
        *ip = base_p[read_p].qcmd ;     // read the optional integer command

    base_p[read_p].qcp = NULL;          // Set the data pointer to NULL
    base_p[read_p].rdy = false;         // Clear ready flag
    read_p++;
    itemsQueued--;
    if(read_p >= size)
        read_p = 0;

    inRead = 0;

    qlock.release();
    return cp;
}


int cpQueue::ready(void)            //returns true if data available
{
    int rc=false;
    Lock qlock(Q_mutex);

    //if ((read_p != write_p) || wrap) rc = true ;
    if(base_p[read_p].rdy)
        rc = true;

    //pthread_mutex_unlock(Q_mutex);
    return rc;
}

int cpQueue::getWritePtr(void)
{
    return write_p;
}

int cpQueue::getReadPtr(void)
{
    return read_p;
}

int cpQueue::getItemsQueued(void)
{
    return itemsQueued;
}
