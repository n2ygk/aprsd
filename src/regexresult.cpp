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


#include "regexresult.hpp"

namespace aprsd
{
    RegexResult::RegexResult( bool exprMatched, int numSubs, regmatch_t*
                             regMatches) throw(AssertException, exception) :
        impl(new RegexResultImpl( exprMatched, numSubs, regMatches)) { }

    RegexResult::RegexResult() throw(exception) { }

    RegexResult::~RegexResult() throw() { }

    bool RegexResult::matched() const throw(AssertException, exception)
    {
        return impl->matched();
    }

    int RegexResult::start() const throw(AssertException, exception)
    {
        return impl->start();
    }

    int RegexResult::length() const throw(AssertException, exception)
    {
        return impl->length();
    }

    Strings RegexResult::getGroups( const string &str ) const
    {
        return impl->getGroups( str );
    }

    RegexSubResult RegexResult::operator [](int index) const
        throw(AssertException, exception)
    {
        return (*impl)[index];
    }

    string RegexResult::substr(const string& text, string::size_type offset) const
        throw(AssertException, exception)
    {
        return matched() ? text.substr(offset + start(), offset + length()) : "";
    }

    RegexResult::operator bool() const throw()
    {
        return impl;
    }

    int RegexResult::getSubExprCount() const throw(AssertException, exception)
    {
        return impl->getSubExprCount();
    }
}
