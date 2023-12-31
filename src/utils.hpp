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


#ifndef UTILS_H
#define UTILS_H

#include <string>
#include <vector>

#include "constant.hpp"
#include "aprsString.hpp"
#include "cpqueue.hpp"



#define BADUSER -1
#define BADGROUP -2
#define BADPASSWD -3

using namespace aprsd;

int WriteLog(const std::string& sp, const std::string& LogFile);
char* strupr(char *cp);
void printhex(char *cp, int n);
bool CmpDest(const char *line, const char *ref);
bool CmpPath(const char *line, const char *ref);
bool callsign(char *s, const char *call);
bool CompareSourceCalls(char *s1, char *s2);
void GetMaxAgeAndCount( int *MaxAge, int *MaxCount);
int  checkpass(const char *szUser, const char *szGroup, const char *szPass);
void RemoveCtlCodes(char *cp);
char* StripPath(char* cp);
bool getMsgDestCall(char *data, char* cp, int n);
bool getMsgSourceCall(char* data, char* cp, int n);
char checkUserDeny(std::string& user);

int stricmp(const char* szX, const char* szY);
void removeHTML(std::string& sp);


int split(std::string& s, std::string sa[],   int saSize,  const char* delim);
int freq(const std::string& s, char c);
void upcase(std::string& s);
void reformatAndSendMicE(aprsString* inetpacket, cpQueue& sendQueue);
bool find_rfcall(const std::string& s, std::string* rfcall[]);
bool find_rfcall(const std::string& s, std::vector<string>& rfcall);
bool find_rfcall(const std::string& s, std::string rfcall[]);

//void strElapsedTime(time_t starttime,  char* timeStr);
void strElapsedTime(const time_t starttime, std::string& timeStr);

unsigned int string_hash(const std::string& s);
void crc_byte(const char data, unsigned int *crc16);
void makeAlias(std::string& s);

double convertRate(int rate);
string convertScale(int rate);



#endif      // UTILS_H
