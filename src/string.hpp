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



//
//   string helper definitions and functions.
//

#ifndef STRING_H__
#define STRING_H__

#include <string>
#include <vector>

#include "osdep.hpp"

namespace aprsd {

    typedef std::vector<std::string> Strings;

    /**
        Trim the string both left and right for whitespaces.

        @param str the string to trim
        @return the newly trimed string
    */
    std::string trim(const std::string& str);

    /**
        Trim all whitespace on the left of the string.

        @param str the string to trim
        @return the newly trimed string
    */
    std::string ltrim(const std::string& str);

    /**
        Trim all whitespace on the right of the string.

        @param str the string to trim
        @return the newly trimed string
    */
    std::string rtrim(const std::string& str);

    /**
        Splits a string into substrings, using the specified character as a delimiter.

        @param text the string to be split.
        @param delim the character to be used as a delimiter.
        @return a Strings containing the substrings.
    */
    Strings split(const std::string& text, char delim);

    /**
        Splits a string into substrings, using the specified string as a delimiter.

        @param text the string to be split.
        @param delim the string to be used as a delimiter.
        @return a Strings containing the substrings.
    */
    Strings split(const std::string& text, const std::string& delim);
}

#endif  // String_H__
