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


#ifndef __UTILS_H
#define __UTILS_H

#include <vector.h>
#include "constant.h"
#include "aprsString.h"
#include "cpqueue.h"

#define BADUSER -1
#define BADGROUP -2
#define BADPASSWD -3

int WriteLog(const char *cp, const char *LogFile);
char* strupr(char *cp);
void printhex(char *cp, int n);
bool CmpDest(const char *line, const char *ref);
bool CmpPath(const char *line, const char *ref);
bool callsign(char *s, const char *call);
bool CompareSourceCalls(char *s1, char *s2);
void GetMaxAgeAndCount( int *MaxAge, int *MaxCount);
int  checkpass(const char *szUser, const char *szGroup, const char *szPass);
void RemoveCtlCodes(char *cp);
void makePrintable(char *cp);
void removeHTML(string& sp);
char* StripPath(char* cp);
bool getMsgDestCall(char *data, char* cp, int n);
bool getMsgSourceCall(char* data, char* cp, int n);
char checkUserDeny(string& user);

int stricmp(const char* szX, const char* szY);


int split(  string& s, string sa[],   int saSize,  const char* delim);
int freq(string& s, char c);
void upcase(string& s);
void reformatAndSendMicE(TAprsString* inetpacket, cpQueue& sendQueue);
bool find_rfcall(const string& s, string* rfcall[]);
bool find_rfcall(const string& s, vector<string*>& rfcall);
bool matchCallsign(const string& s1, const string& s2);

void strElapsedTime(time_t starttime,  char* timeStr);
extern void reliable_usleep (int usecs);

#endif


