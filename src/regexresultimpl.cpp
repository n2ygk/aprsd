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

#include "regexresultimpl.hpp"
#include "aprsdassert.hpp"

#include <string>

namespace aprsd
{
    using std::string;

    RegexResultImpl::RegexResultImpl(bool arg_exprMatched, int arg_numSubs,
                                     regmatch_t* arg_regMatches) :
        exprMatched(arg_exprMatched), numSubs(arg_numSubs), regMatches(arg_regMatches) { }

    RegexResultImpl::~RegexResultImpl() throw()
    {
        delete[] regMatches;
    }

    bool RegexResultImpl::matched() const throw(AssertException, exception)
    {
        return exprMatched;
    }

    int RegexResultImpl::start() const throw(AssertException, exception)
    {
        if (exprMatched)
        {
            return regMatches[0].rm_so;
        }
        else
        {
            return 0;
        }
    }

    int RegexResultImpl::length() const throw(AssertException, exception)
    {
        if (exprMatched)
        {
            return regMatches[0].rm_eo - regMatches[0].rm_so;
        }
        else
        {
            return 0;
        }
    }

    Strings RegexResultImpl::getGroups( const string &str ) const
    {
        Strings strs;

        for( int i = 1; i <= getSubExprCount(); i++ ) {
            RegexSubResult res = (*this)[ i ];

            strs.push_back( str.substr( res.start(), res.length()));
        }
        return strs;
    }

    RegexSubResult RegexResultImpl::operator [](int index) const
        throw(AssertException, exception){
        Assert(index > 0 && index < numSubs,
               AssertException("RegexSubResult index out of range"));

        regmatch_t* regMatch = &regMatches[index];

        if (regMatch->rm_so == -1)
        {
            return RegexSubResult(false, 0, 0);
        }
        else
        {
            return RegexSubResult(true, regMatch->rm_so,
                                  regMatch->rm_eo - regMatch->rm_so);
        }
    }

    int RegexResultImpl::getSubExprCount() const throw(AssertException, exception)
    {
        return numSubs - 1;
    }
}
