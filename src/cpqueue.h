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


#ifndef CPQUEUE_H
#define CPQUEUE_H

#include "aprsString.h"
#include "mutex.h"

using namespace aprsd;

class  queueData
{
public:
    void* qcp;      //Pointer to dynamically allocated aprsString or char string.
    int	qcmd;     //Optional interger data command or info
    bool rdy;
};


class cpQueue
{
private:
    queueData *base_p;
    int write_p;
    int read_p;
    int size, lock, inRead, inWrite;
    Mutex Q_mutex;
    bool dyn;

public:
    int overrun;
    int itemsQueued;
    cpQueue(int n, bool d);     // fifo Queue constructor
    ~cpQueue(void);             // destructor

    int write(aprsString* cs, int cmd);
    int write(aprsString* cs);
    int write(char* cs, int cmd);
    int write(unsigned char* cs, int cmd);
    int write(const char* cs, int cmd);

    void* read(int *cmd);
    int ready(void);            //return non-zero when queue data is available

    int getWritePtr(void);      //For debugging
    int getReadPtr(void);
    int getItemsQueued(void);
};

#endif      // CPQUEUE_H

