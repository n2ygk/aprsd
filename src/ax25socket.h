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

#ifndef AX25SOCKETS_H
#define AX25SOCKETS_H

#include "constant.h"

using namespace aprsd;

// TCP Wrappers
int inet_ptons(int family, const char *strptr, void *addrptr);

int SocketOpen (const string& rfport, const char *destcall);
int SocketClose (void);
int SocketWrite (const char *cp);
int SocketWrite (void);
bool SocketReadWrite (char buf[]);

#endif      // AX25SOCKETS_H
