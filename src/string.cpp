/*
 * $Id$
 *
 * aprsd, Automatic Packet Reporting System Daemon
 * Copyright (C) 1997,2002 Dale A. Heatherington, WA4DSY
 * Copyright (C) 2001-2003 aprsd Dev Team
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

#include "string.hpp"

#include <cctype>

namespace aprsd {
    using namespace std;

    string trim(const string& str) 
    {
        return rtrim(ltrim(str));
    }

    string rtrim(const string& str)
    {
        string::const_reverse_iterator iter = str.rbegin();

        while (iter != str.rend() && isspace(*iter))
            iter++;

        return str.substr(0, str.length() - (iter - str.rbegin()));
    }

    string ltrim(const string& str)
    {
        string::const_iterator iter = str.begin();
        while (iter != str.end() && isspace(*iter ))
            iter++;

        return string(iter, str.end());
    }

    Strings split(const string& text, char sep)
    {
        Strings vec;

        string::size_type pos = 0;
        for (string::size_type newp; (newp = text.find(sep, pos)) != string::npos;
            pos = newp + 1)
            vec.push_back(text.substr(pos, newp - pos));
        vec.push_back(text.substr(pos));

        return vec;
    }

    Strings split(const string& text, const string& sep)
    {
        Strings vec;

        string::size_type pos = 0;
        for (string::size_type newp; (newp = text.find(sep, pos)) != string::npos;
            pos = newp + sep.length())
            vec.push_back(text.substr(pos, newp - pos));
        vec.push_back(text.substr(pos));

        return vec;
    }

};
