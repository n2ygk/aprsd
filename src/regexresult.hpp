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

#ifndef REGEX_RESULT_HPP
#define REGEX_RESULT_HPP

#include "regexresultimpl.hpp"
#include "string.hpp"

namespace aprsd
{
    /**
     * Represents the result of a Regex search.  This class is
     * reference-counted.  Instances of this class are constructed by
     * Regex.
     *
     * @see Regex
     * @see RegexSubResult
     */
    class RegexResult
    {
    public:
        RegexResult(bool matched, int numSubs, regmatch_t* regMatches)
            throw(AssertException, exception);

        ~RegexResult() throw();

        /**
         * @return true if a match was found.
         */
        bool matched() const throw(AssertException, exception);

        /**
         * @return the starting index of the string that matched the
         * pattern, or 0 if no match was found.
         */
        int start() const throw(AssertException, exception);

        /**
         * @return the length of the string that matched the pattern, or 0
         * if no match was found.
         */
        int length() const throw(AssertException, exception);

        /**
           Return a string vector of all the results. This is the easy but also
           somewhat more expensive version, as it allocates both a vector an some
           new substrings. But its nice and easy :-)

           @param the newly mached string
           @return vector of result strings
         */
        Strings getGroups( const string &str ) const;

        /**
         * @return a RegexSubResult representing the location of text
         * matching a subexpression.  The RegexSubResult objects with
         * indices 1..n give the locations of substrings that matched
         * subexpressions 1..n.
         *
         * @param index the 1-based index of the subexpression.
         * @exception AssertException if the index is out of range.
         */
        RegexSubResult operator [](int index) const
            throw(AssertException, exception);

        /**
         * @param text the string searched.
         * @param offset the index in the string at which the search started.
         * @return the substring that matched the
         * subexpression, or an empty string if there was no match
         * for the subexpression.
         */
        string substr(const string& text, string::size_type offset = 0) const
            throw(AssertException, exception);

        /**
         * Constructs a null RegexResult.
         */
        RegexResult() throw(exception);

        /**
         * @return true if this object is non-null.
         */
        operator bool() const throw();

        /**
         * @return the number of subexpressions in this result.
         */
        int getSubExprCount() const throw(AssertException, exception);

    private:
        RegexResultHandle impl;
    };
}

#endif // REGEX_RESULT_HPP
