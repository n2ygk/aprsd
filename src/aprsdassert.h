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


#ifndef ASSERT_H
#define ASSERT_H

#include "osdep.h"
#include <string>

#ifndef DEBUG
#ifdef NDEBUG
#define DEBUG(x)        /* Debug assertion */
#else
#define DEBUG(x)        x
#endif
#endif

/**
 * \def DEBUG(x)
 *
 * Removes x from the source code if NDEBUG is defined.
 */

/**
 * Throws an object as an exception if an assertion is not met.  Can be used with the
 * DEBUG macro to selectively remove assertions from production code.  For example:
 *
 * <pre>
 * Assert(foo == bar, AssertException("failed!"));
 *
 * DEBUG(Assert(foo == bar, AssertException("failed!")));
 * </pre>
 *
 * The second assertion will be removed if NDEBUG is defined.
 */

namespace aprsd {

    template<class E> inline void Assert(bool assertion, E e)
    {
        if (!assertion) throw e;
    }

}
#endif // ASSERT_H
