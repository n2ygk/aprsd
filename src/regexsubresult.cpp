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

#include "regexsubresult.hpp"

#include <string>

namespace aprsd
{
    using std::string;

    RegexSubResult::RegexSubResult(bool arg_exprMatched,
                                   int arg_startIndex, int arg_len)
        throw(AssertException, exception) :
        exprMatched(arg_exprMatched), startIndex(arg_startIndex),
        len(arg_len) { }

    RegexSubResult::~RegexSubResult() throw() { }

    bool RegexSubResult::matched() const throw(AssertException, exception)
    {
        return exprMatched;
    }

    int RegexSubResult::start() const throw(AssertException, exception)
    {
        return startIndex;
    }

    int RegexSubResult::length() const throw(AssertException, exception)
    {
        return len;
    }

    string RegexSubResult::substr(const string& text,
                                  string::size_type offset) const
        throw(AssertException, exception)
    {
        return matched()? text.substr(offset + start(), offset + length()) : "";
    }
}
