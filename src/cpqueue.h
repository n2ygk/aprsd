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


#ifndef __CPQUEUE_H
#define __CPQUEUE_H

#include "aprsString.h"


class  queueData
{
public:
    void* qcp;                          // Pointer to dynamically allocated TAprsString or char string.
    int	qcmd;                           // Optional interger data command or info
    bool rdy;
};


class cpQueue
{
public:
    int overrun;
    int itemsQueued;
    cpQueue(int n, bool d);             // fifo Queue constructor
    ~cpQueue(void);                     // destructor

    int write(TAprsString* cs, int cmd);
    int write(TAprsString* cs);
    int write(char* cs, int cmd);
    int write(unsigned char* cs, int cmd);

    void* read(int *cmd);
    int ready(void);                    // return non-zero when queue data is available

    int getWritePtr(void);              // For debugging
    int getReadPtr(void);
    int getItemsQueued(void);

private:
    queueData *base_p;
    int  write_p;
    int  read_p;
    int size, lock, inRead, inWrite;
    pthread_mutex_t* Q_mutex;
    bool dyn;
};

#endif
