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

#include "exceptionguard.hpp"

namespace aprsd
{
    void assertUnexpected() throw(UnexpectedException)
    {
        try {
            throw;
        }
        catch (Exception& e) {
            throw UnexpectedException(e.toString());
        }
        catch (exception& e) {
            throw UnexpectedException(string("Unexpected system exception: ") + e.what());
        }
        catch (...) {
            throw UnexpectedException("Exception thrown of unknown type");
        }
    }

    ExceptionGuard::ExceptionGuard() throw()
    {
        old = std::set_unexpected(&assertUnexpected);
    }

    ExceptionGuard::~ExceptionGuard() throw()
    {
        std::set_unexpected(old);
    }
}
