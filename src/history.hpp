/*
 * $Id$
 *
 * aprsd, Automatic Packet Reporting System Daemon
 * Copyright (C) 1997,2002 Dale A. Heatherington, WA4DSY
 * Copyright (C) 2001-2004 aprsd Dev Team
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


#ifndef HISTORY_H
#define HISTORY_H

#include "aprsString.hpp"

extern int dumpAborts;  //Number of history dumps aborted
extern int ItemCount;   //number of items in History list

void CreateHistoryList();
bool AddHistoryItem(aprsd::aprsString *hp);
void DeleteHistoryItem(aprsd::aprsString *hp);
int DeleteOldItems(int x);
int DeleteItem(aprsd::aprsString* ref);
bool DupCheck(aprsd::aprsString* ref, time_t t);
int SendHistory(int session,int em);
int SaveHistory(const std::string& name);
int ReadHistory(const std::string& name);
bool StationLocal(const std::string& sp, int em);
aprsd::aprsString* getPosit(const std::string& call, int em);
bool timestamp(unsigned long sn, time_t t);
int localCount();


#endif      // HISTORY_H

