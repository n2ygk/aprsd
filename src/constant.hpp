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


#ifndef CONSTANT_H
#define CONSTANT_H

#include <string>
#include "config.h"




// If you really must convert Mic-E packets to traditional APRS
// change this to TRUE and add "ConvertMicE yes" to your aprsd.conf file.
const bool CONVERT_MIC_E = false;

/* Define this to write 3rd party pkts to debug.log */
// #define DEBUGTHIRDPARTY


//If this is defined the qAc,CallSign construct is added to path
#define INET_TRACE

#define ULONG unsigned long
#define LONG long
#define USHORT unsigned short int
#define INT16 unsigned short int
#define APIRET int
#define INT32 int
#define echomask_t unsigned long
#define LF 0x0a
#define CR 0x0d
#define RXwhite " \t\n\r"

//#define LINK_SERVER_PORT 1313

#define CMD_END_THREAD -1L
#define SRC_TNC	-2L
#define SRC_USER	-3L
#define SRC_INTERNAL -4L
#define SRC_UDP -5L
#define SRC_IGATE -6L


#define MAXCLIENTS 30
#define MAXLOAD 200000

//This sets the Internet line input buffer size.
//Lines longer than this, including CR,LF,NULL, will be truncated.
//Actually, the data will be truncated but a CR,LF,NULL will always be on the end.
#define BUFSIZE 255

//Defines duplicate detection window time in seconds
#define DUPWINDOW 20

#define RUNTSIZE 0
#define MAXRETRYS 7

#define HOSTIPSIZE 33

#define destINET 1
#define destTNC  2


/* These are for Linux user/pass logons.  They define the group used by
   /etc/group .   You must have these groups and users defined in /etc/group.

   The "tnc" group defines which users are allowed direct access to control the TNC.
   The "aprs"  group defines users which log on with a Linux user/password
   instead of the Mac/WinAPRS automatic password. These users are allowed to
   insert data into the data stream.

   To add a group logon as root and edit the /etc/group file by adding
	a line such as: tnc::102:root,wa4dsy,bozo   */

#define APRSGROUP "aprs"
#define TNCGROUP "tnc"


/* this is not used in production code*/
//#define TEST "WA4DSY>APRS,WIDE:!3405.31N/08422.46WyWA4DSY APRS Internet Server running on Linux.\r\n"


#endif      // CONSTANT_H
