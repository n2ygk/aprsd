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



#include "mutex.hpp"
#include "osdep.hpp"

namespace aprsd
{
    string hstrerror_r(int ERRNUM)
    {
        static Mutex mutex;

        Lock lock (mutex);
#ifdef HAVE_HSTRERROR
        return ::hstrerror (ERRNUM);
#else /* !HAVE_HSTRERROR */
        switch (ERRNUM)
        {
        case 0:
            return "Success.";
        case HOST_NOT_FOUND:
            return "The specified host is unknown.";
        case NO_ADDRESS:
#if NO_ADDRESS != NO_DATA
        case NO_DATA:
#endif // NO_ADDRESS != NO_DATA
            return "The requested name is valid but does not have an IP address.";
        case NO_RECOVERY:
            return "A non-recoverable name server error occurred.";
        case TRY_AGAIN:
            return "A temporary error occurred on an authoritative name server. "
                "Try again later.";
        }

        return "Unknown error number at hstrerror_r.";
#endif/* !HAVE_HSTRERROR */
    }

    void reliable_usleep(int usecs)
    {
        timeval now, end;

        gettimeofday (&now, NULL);
        end = now;
        end.tv_sec  += usecs / 1000000;
        end.tv_usec += usecs % 1000000;

        while ((now.tv_sec < end.tv_sec) || ((now.tv_sec == end.tv_sec) && (now.tv_usec < end.tv_usec))) {
            timeval tv;
            tv.tv_sec = end.tv_sec - now.tv_sec;
            if (end.tv_usec >= now.tv_usec)
                tv.tv_usec = end.tv_usec - now.tv_usec;
            else {
                tv.tv_sec--;
                tv.tv_usec = 1000000 + end.tv_usec - now.tv_usec;
            }
            select (0, NULL, NULL, NULL, &tv);
            gettimeofday (&now, NULL);
        }
    }
}
