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

#ifndef REGEX_RESULT_IMPL_HPP
#define REGEX_RESULT_IMPL_HPP

#include "osdep.hpp"
#include <cstddef>
#include <string>

extern "C"
{
#include <regex.h>
}

#include "refcount.hpp"
#include "aprsdexception.hpp"
#include "regexsubresult.hpp"
#include "string.hpp"

namespace aprsd
{
    using std::string;

    class RegexResultImpl : public RefCounted
    {
    public:
        RegexResultImpl( bool exprMatched, int numSubs, regmatch_t* regMatches);
        ~RegexResultImpl() throw();

        bool matched() const throw(AssertException, exception);

        int start() const throw(AssertException, exception);

        int length() const throw(AssertException, exception);

        Strings getGroups( const string &str ) const;

        RegexSubResult operator [](int index) const throw(AssertException, exception);

        int getSubExprCount() const throw(AssertException, exception);

    private:
        bool exprMatched;
        int numSubs;
        regmatch_t* regMatches;
    };

    typedef RefHandle<RegexResultImpl> RegexResultHandle;
}

#endif // REGEX_RESULT_IMPL_HPP
