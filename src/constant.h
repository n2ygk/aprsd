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

#ifndef __CONSTANT_H
#define __CONSTANT_H

#define DEBUG

/* update the next 3 lines with each version change */
//#define SIGNON "# " PACKAGE " " VERSION " (c) 2001, aprsd Dev Team http://aprsd.sourceforge.net \r\n"
const char SIGNON[] = "# " PACKAGE " " VERSION " (c) 2001, aprsd Dev Team http://sourceforge.net/projects/aprsd/ \r\n";
#define VERS PACKAGE " " VERSION
#define PGVERS APRSDTOCALL
/*--------------------------------------------------*/

#ifndef TRUE
#define TRUE (!FALSE)
#endif

#ifndef FALSE
#define FALSE 0
#endif

#ifndef HAVE_BOOL
typedef bool int;
#endif

#ifndef ULONG
typedef unsigned long ULONG;            // 4 bytes, unsigned
#endif

#ifndef LONG
typedef long LONG;
#endif

#ifndef USHORT
typedef unsigned short int USHORT;
#endif

#ifndef INT16
typedef unsigned short int INT16;
#endif

#define APIRET int

#ifndef INT32
typedef int INT32;
#endif

#define LF 0x0a
#define CR 0x0d
#define RXwhite " \t\n\r"
#define NULLCHR '\0'

//#define LINK_SERVER_PORT 1313

#define CMD_END_THREAD -1L
#define SRC_TNC -2L
#define SRC_USER -3L
#define SRC_INTERNAL -4L
#define SRC_UDP -5L
#define SRC_IGATE -6L

#define MAXCLIENTS 30

//This sets the Internet line input buffer size.
//Lines longer than this, including CR,LF,NULL, will be truncated.
//Actually, the data will be truncated but a CR,LF,NULL will always be on the end.
#define BUFSIZE 255


#define RUNTSIZE 0
#define MAXRETRYS 7

#define HOSTIPSIZE 33

#define destINET 1
#define destTNC  2


/* To run aprsd from another directory change the next line */
#ifdef USE_FHS
#define HOMEDIR "/var/log/aprsd/"
#define CONFPATH "/etc/aprsd/"
#define LOGPATH "/var/log/aprsd/"
#define VARPATH "/var/lib/aprsd/"
#else
#define HOMEDIR "/home/aprsd2"
#define CONFPATH ""
#define LOGPATH ""
#define VARPATH ""
#endif

#define CONFFILE "aprsd.conf"
#define MAINLOG "aprsd.log"
#define STSMLOG "stsm.log"
#define RFLOG "rf.log"
#define UDPLOG "udp.log"
#define ERRORLOG "error.log"
#define WELCOME "welcome.txt"
#define TNC_INIT  "INIT.TNC"
#define TNC_RESTORE "RESTORE.TNC"
#define APRSD_INIT "INIT.APRSD"
#define SAVE_HISTORY "history.txt"
#define USER_DENY "user.deny"
#define FUBARLOG "badpacket.log"
//  #define BPLOG		// Uncomment this for logging of filtered packets 

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
#define TEST "WA4DSY>APRS,WIDE:!3405.31N/08422.46WyWA4DSY APRS Internet Server running on Linux.\r\n"

#endif // __CONSTANT_H
