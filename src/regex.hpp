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

#ifndef REGEX_HPP
#define REGEX_HPP

#include "regeximpl.hpp"

namespace aprsd
{
    using namespace std;
    /**
     * Represents a POSIX extended regular expression.  A Regex object
     * does not save the results of searches; instead, it returns a
     * RegexResult object for each search.  One Regex object can therefore
     * be used by multiple threads.  This class is reference-counted.
     *
     * @see RegexResult
     * @see RegexSubResult
     */
    class Regex
    {
    public:
        /**
         * Compiles a regular expression.
         *
         * @param pattern the pattern to compile.
         * @param flags the bitwise OR of any of the following:
         * REG_ICASE (ignore case), REG_NEWLINE (match-any-character operators
         * don't match a newline; '^' and '$' match at newlines).
         *
         * @exception RegexException if there is a compilation error.
         */
        Regex(const string& pattern, int flags = 0);

        ~Regex() throw();

        /**
         * Looks for a match in a string.
         *
         * @param text the string to search in.
         * @param offset the index in the string at which to start the search.
         * @param flags the bitwise OR of any of the following: REG_NOTBOL (the beginning
         * of the string will not match '^'), REG_NOTEOL (the end of the string will not
         * match '$').
         *
         * @exception RegexException if the regular expression engine runs out of memory.
         * @exception AssertException if the offset is out of bounds.
         */
        RegexResult match(const string& text,
                          string::size_type offset = 0,
                          int flags = 0) const;

        /**
         * Looks for a match in a C-style string.
         *
         * @param text the string to search in.
         * @param flags the bitwise OR of any of the following:
         * REG_NOTBOL (the beginning of the string will not match '^'),
         * REG_NOTEOL (the end of the string will not match '$').
         *
         * @exception RegexException RegexException if the regular expression engine runs
         * out of memory.
         */
        RegexResult match(const char* text, int flags = 0) const;

        /**
         * Constructs a null Regex.
         */
        Regex() throw(exception);

        /**
         * @return true if this object is non-null.
         */
        operator bool() const throw();

        /**
         * @return the number of subexpressions in the pattern.
         */
        int getSubExprCount() const;

    private:
        RefHandle<RegexImpl> impl;
    };

    /**
       Splits a string into substrings, using delimiters that match the specified
       regular expression.

       @param text the string to be split.
       @param delim the regular expression to be used for matching delimiters.
       @return a Strings containing the substrings.
       @exception RegexException if the regular expression engine runs out of memory.
     */
    Strings split( const string& text, const Regex &delim );
}

#endif // REGEX_HPP
