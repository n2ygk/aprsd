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


#ifndef MIC_E_H
#define MIC_E_H

#ifndef u_char
#define u_char unsigned char
#endif

extern bool ConvertMicE;

unsigned int hex2i(u_char,u_char);

int fmt_mic_e(const u_char *t,      // tocall
    const u_char *i,                // info
    const int l,                    // length of info
    u_char *buf1,                   // output buffer
    int *l1,                        // length of output buffer
    u_char *buf2,                   // 2nd output buffer
    int *l2,                        // length of 2nd output buffer
    time_t tick);                   // received timestamp of the packet

int fmt_x1j4(const u_char *t,       // tocall
    const u_char *i,                // info
    const int l,                    // length of info
    u_char *buf1,                   // output buffer
    int *l1,                        // length of output buffer
    u_char *buf2,                   // 2nd output buffer
    int *l2,                        // length of 2nd output buffer
    time_t tick);                   // received timestamp of the packet

#endif      // MIC_E_H
