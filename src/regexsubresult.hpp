/*
 * $Id$
 *
 * aprsd, Automatic Packet Reporting System Daemon
 * Copyright (C) 1997,2002 Dale A. Heatherington, WA4DSY
 * Copyright (C) 2001-200 aprsd Dev Team
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

#ifndef REGEX_SUB_RESULT_HPP
#define REGEX_SUB_RESULT_HPP

#include "osdep.hpp"
#include "aprsdexception.hpp"

#include <string>

namespace aprsd
{
    using std::string;

    /**
     * Represents a substring matched by a subexpression in a Regex.  This
     * class is not reference-counted, but is inexpensive to pass by
     * value.
     *
     * @see Regex
     * @see RegexResult
     */
    class RegexSubResult
    {
    public:
        RegexSubResult(bool exprMatched, int startIndex, int len)
            throw(AssertException, exception);

        ~RegexSubResult() throw();

        /**
         * @return true if there was a match for the subexpression.
         */
        bool matched() const throw(AssertException, exception);

        /**
         * @return the starting index of the substring that matched the
         * subexpression, or 0 if there was no match for the
         * subexpression.
         */
        int start() const throw(AssertException, exception);

        /**
         * @return the length of the substring that matched the
         * subexpression, or 0 if there was no match for the
         * subexpression.
         */
        int length() const throw(AssertException, exception);

        /**
         * @param text the string searched.
         * @param offset the index in the string at which the search started.
         * @return the substring that matched the
         * subexpression, or an empty string if there was no match
         * for the subexpression.
         */
        string substr(const string& text, string::size_type offset = 0) const
            throw(AssertException, exception);

    private:
        bool exprMatched;
        int startIndex;
        int len;
    };
}

#endif // REGEX_SUB_RESULT_HPP
