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

#include "regeximpl.hpp"
#include "aprsdassert.hpp"

#include <string>

namespace aprsd
{
    using std::string;

    RegexImpl::RegexImpl(const string& pattern, int flags)
    {
        int errCode = regcomp(&reg, pattern.c_str(),
                              flags | REG_EXTENDED);
        if (errCode)
        {
            string message = getErrMsg(errCode);
            regfree(&reg);
            throw RegexException(message);
        }
    }

    string RegexImpl::getErrMsg(int errCode) const
    {
        size_t errBufSize;
        errBufSize = regerror(errCode, &reg, NULL, 0);
        //char errBuf[errBufSize];
        char* errBuf = new char(errBufSize);
        regerror(errCode, &reg, errBuf, errBufSize);
        string retval = errBuf;
        delete [] errBuf;
        return retval;
    }

    RegexImpl::~RegexImpl() throw()
    {
        regfree(&reg);
    }

    RegexResult RegexImpl::match(const string& text,
                                 string::size_type offset,
                                 int flags) const
    {
        Assert(offset >= 0 && offset <= text.size(),
               AssertException("Regex::match: String index out of range"));

        return match(text.c_str() + offset, flags);
    }

    RegexResult RegexImpl::match(const char* text, int flags) const
    {
        int numSubs = reg.re_nsub + 1;
        regmatch_t* regMatches = new regmatch_t[numSubs];
        int result = regexec(&reg, text, numSubs, regMatches, 0);
        switch (result)
        {
        case 0:
            return RegexResult( true, numSubs, regMatches);

        case REG_NOMATCH:
            return RegexResult( false, numSubs, regMatches);

        default:
            throw RegexException(getErrMsg(result));
        }
    }

    int RegexImpl::getSubExprCount() const
    {
        return reg.re_nsub;
    }
}
