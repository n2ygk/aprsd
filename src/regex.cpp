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

#include "regex.hpp"
#include "string.hpp"

namespace aprsd
{
    using std::string;

    Regex::Regex() throw(exception) { }

    Regex::Regex(const string& pattern, int flags) :
        impl(new RegexImpl(pattern, flags)) { }

    Regex::~Regex() throw() { }

    RegexResult Regex::match(const string& text,
                             string::size_type offset,
                             int flags) const
    {
        return impl->match(text, offset, flags);
    }

    RegexResult Regex::match(const char* text, int flags) const
    {
        return impl->match(text, flags);
    }

    int Regex::getSubExprCount() const
    {
        return impl->getSubExprCount();
    }

    Regex::operator bool() const throw()
    {
        return impl;
    }

    Strings split(const string& text, const Regex& sep)
    {
        Strings vec;

        string::size_type pos = 0;
        for (RegexResult match; (match = sep.match(text, pos)).matched();
             pos += match.start() + match.length())
            vec.push_back(text.substr(pos, match.start()));
        vec.push_back(text.substr(pos));

        return vec;
    }
}
