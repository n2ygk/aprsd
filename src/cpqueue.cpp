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

extern "C"
{
#include <stdio.h>
}

#include "osdep.h"
#include "constant.h"
#include "cpqueue.h"
#include "aprsString.h"
#include "assert.h"

/*-------------------------------------------------------*/

//Build a Queue object
namespace aprsd
{
    using std::queue;
    using std::string;

    cpQueue::cpQueue(int n, bool d) throw(AssertException, exception)
    {
        overrun = 0;
        maxQueueSize = n;
        dyn = d;
        itemsQueued = 0;
        HWItemsQueued = 0;
        lock = 0;
    }

    //Destroy Queue object
    //
    cpQueue::~cpQueue(void) throw()
    {
        lock = 1;
        while (!ls.empty())
            ls.pop();
    }


    // Place a pointer and an Integer in the queue
    // returns 0 on success;
    //
    bool cpQueue::write(const char *cp, int n) throw(AssertException, exception)
    {
        int rc = true;
        Lock lock(mutex);

        lock.get();
        if ((int)ls.size() < maxQueueSize) {
            ls.push((void*)cp);
            itemsQueued = ls.size();

            if (itemsQueued > HWItemsQueued)
                HWItemsQueued = itemsQueued;
        } else {
            overrun++ ;
            rc = false;
            delete[] cp;
            cp = NULL;
        }

        lock.release();
        return(rc);
    }

    bool cpQueue::write(char *cp, int n) throw(AssertException, exception)
    {
        return(write((const char*)cp, n));
    }

    bool cpQueue::write(unsigned char *cp, int n) throw(AssertException, exception)
    {
        return(write((const char*)cp, n));
    }


    bool cpQueue::write(TAprsString* cs, int n) throw(AssertException, exception)
    {
        int rc = true;
        Lock lock(mutex);

        lock.get();
        if ((int)ls.size() < maxQueueSize) {     // limit size of queue
            ls.push((void*)cs);             // push new item onto queue
            itemsQueued = ls.size();
            if (itemsQueued > HWItemsQueued)
                HWItemsQueued = itemsQueued;
        } else {
            overrun++ ;
            delete cs;                      // Delete the object that couldn't be put in the queue
            cs = NULL;
            rc = false;
        }
        lock.release();
        return(rc);
    }


    bool cpQueue::write(TAprsString* cs) throw(AssertException, exception)
    {
        return(write(cs, cs->sourceSock));
    }

    //Read a pointer and Integer from the Queue
    //
    void* cpQueue::read(int *ip) throw(AssertException, exception)
    {
        Lock lock(mutex);

        lock.get();
        if (!ls.empty()) {
            void* cp = ls.front();              // Read from the front of queue
            ls.pop();                           // pop the node (del)
            itemsQueued = ls.size();
            lock.release();
            return(cp);
        }
        lock.release();
        return(NULL);
    }

    //returns TRUE if data available
    //
    bool cpQueue::ready(void) throw(AssertException, exception)
    {
        int rc=true;
        Lock lock(mutex);

        lock.get();
        if (ls.empty())
            rc = false;

        lock.release();
        return(rc);
    }

    int cpQueue::getItemsQueued(void) throw(AssertException, exception)
    {
        int inq;
        Lock lock(mutex);

        lock.get();
        inq = itemsQueued;
        lock.release();

        return(inq);
    }

    int cpQueue::getHWItemsQueued(void) throw(AssertException, exception)
    {
        int HWinq;
        Lock lock(mutex);

        lock.get();
        HWinq = HWItemsQueued;
        lock.release();

        return(HWinq);
    }


    int cpQueue::getQueueSize(void) throw(AssertException, exception)
    {
        int Qsize;
        Lock lock(mutex);

        lock.get();
        Qsize = maxQueueSize;
        lock.release();

        return(Qsize);
    }
}
