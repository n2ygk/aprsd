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

#ifndef REGEX_IMPL_HPP
#define REGEX_IMPL_HPP

#include "regexresult.hpp"

namespace aprsd
{
    using std::string;

    class RegexImpl : public RefCounted
    {
    public:
        RegexImpl(const string& pattern, int flags);

        ~RegexImpl() throw();

        RegexResult match(const string& text,
                          string::size_type offset,
                          int flags) const;

        RegexResult match(const char* text, int flags) const;

        int getSubExprCount() const;

    private:
        string getErrMsg(int errCode) const;
        regex_t reg;
    };
}

#endif // REGEX_IMPL_HPP
