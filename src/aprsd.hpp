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


#ifndef APRSD_H
#define APRSD_H

#include <string>

#include "constant.hpp"
#include "cpqueue.hpp"

/* update the next 3 lines with each version change */
#define SIGNON "# " PACKAGE_STRING " 2002-2004 aprsd group\r\n"


#define VERS PACKAGE_STRING
//The next char string is put into the AX25 "TO" field.  Keep length at 6 chars.
#define PGVERS TOCALL
/*--------------------------------------------------*/


// To run aprsd from another directory change the next line
extern const std::string HOMEDIR;

extern const std::string CONFPATH;
extern const std::string CONFFILE;
extern const std::string MAINLOG;
extern const std::string STSMLOG;
extern const std::string RFLOG;
extern const std::string UDPLOG;
extern const std::string ERRORLOG;
extern const std::string DEBUGLOG;
extern const std::string REJECTLOG;
extern const std::string LOOPLOG;
extern const std::string WELCOME;
extern const std::string TNC_INIT;
extern const std::string TNC_RESTORE;
extern const std::string APRSD_INIT;
extern const std::string SAVE_HISTORY;
extern const std::string USER_DENY;

extern std::string logDir;



#endif      // APRSD_H
