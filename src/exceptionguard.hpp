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

#ifndef EXCEPTIONGUARD_H
#define EXCEPTIONGUARD_H

#include "aprsdexception.hpp"

namespace aprsd
{
    using std::unexpected_handler;

    extern void assertUnexpected() throw(UnexpectedException);

    /**
     * Catches unexpected exceptions in a program, and rethrows them as
     * UnexpectedException objects.  UnexpectedException is derived from AssertException,
     * which passes to user code.  If the unexpected exception is derived from
     * Exception or std::exception, the UnexpectedException object's detail
     * message will contain a description of it.
     *
     * To use this feature, just instantiate an ExceptionGuard as a local variable at the
     * beginning of your program's main() method.
     *
     * This class is not reference-counted.
     *
     */
    class ExceptionGuard
    {
        std::unexpected_handler old;

    public:
        /**
         * Sets an unexpected_handler that rethrows exceptions as UnexpectedException
         * objects.
         */
        ExceptionGuard() throw();

        /**
         * Restores the original unexpected_handler.
         */
        ~ExceptionGuard() throw();
    };
}

#endif      // EXCEPTIONGUARD_H
