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
#include <ctype.h>
#include <pthread.h>
#include <stdio.h>
#include <fstream>
#include <iostream>
#include <strstream>
#include <iomanip>
#include <string>
#include <ctime>
#include <string>
#include <stdexcept>

#include "constant.h"
#include "dupCheck.h"
#include "mutex.h"

#define TABLESIZE  0x10000    /* 64K hash table containing time_t values */

using namespace std;
using namespace aprsd;


dupCheck::dupCheck()
{
    hashtime = new time_t[TABLESIZE];
    hashhash = new INT16[TABLESIZE];
    clear();
    time_t t = time(NULL);  //Make sure no two aprsd servers user the same seed in the hash function
    seed = (unsigned long)t;
    if ((hashtime == NULL) || (hashhash == NULL))
        cerr << "Dup Filter failed to initialize" << endl;
}


dupCheck::~dupCheck()
{
    try {
        if (hashtime != NULL) {
            delete hashtime;
            hashtime = NULL;
        }

        if (hashhash != NULL) {
            delete hashhash;
            hashhash = NULL;
        }
    } catch (...) { }
}



bool dupCheck::check(aprsString* s, int t)
{
    bool dup = false;
    time_t dt = 0;
    static int processed = 0;
    static int dupcount = 0;
    Lock locker(mutex);

    if ((hashtime == NULL) || (hashhash == NULL) || s->allowdup)
        return false;

    INT32 hash = s->gethash(seed);  //Generate hash value from initial seed based on pgm start time.
    INT16 hash_lo = hash & 0xffff;  //be sure we stay inside the table!
    INT16 hash_hi = hash >> 16;     //upper 16 bits of hash
    hash_hi &= 0xffff;

    dt =  s->timestamp - hashtime[hash_lo];

    if (( dt <= t )  // See if time difference is less than t seconds
            && (hash_hi == hashhash[hash_lo])) {    // and hash_hi value is identical

        dup = true;
    }

    hashtime[hash_lo] = s->timestamp;   // put this new data in the tables
    hashhash[hash_lo] = hash_hi;

    if (dup)
        dupcount++;

    processed++;

    return dup;
}


void dupCheck::clear()
{
    for (int i = 0; i < TABLESIZE; i++){
        if (hashtime)
            hashtime[i] = 0;  //Initialize tables

        if (hashhash)
            hashhash[i] = 0;
    }
}

