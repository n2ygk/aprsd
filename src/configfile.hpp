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


#ifndef CONFIGFILE_HPP
#define CONFIGFILE_HPP

#include "aprsdexception.hpp"
#include "refcount.hpp"
#include "regex.hpp"

#include <iostream>
#include <map>
#include <string>

namespace aprsd
{
    using std::string;
    using std::multimap;
    using std::istream;
    using std::ostream;

    /**
     * Represents a section of a ConfigFile.  Each element in a ConfigSection represents a
     * key-value pair in the corresponding section of the file.
     *
     * This class is reference-counted.  It is not thread-safe; use a Mutex to synchronize
     * concurrent access.
     *
     * @see ConfigFile
     */
    class ConfigSection : public multimap<string, string>
    {
    public:
        ConfigSection() throw(AssertException, exception);
        ~ConfigSection() throw();

        /**
         * Searches for an element having the specified key.  Returns the value of the
         * first matching element found.  If not found, inserts an empty element and
         * returns it.
         */
        string& operator[](const string& key) throw(AssertException, exception);

        /**
         * Searches for an element having the specified key.  Returns the value of the
         * first matching element found.
         *
         * @exception NoSuchElementException if no matching element is found.
         */
        const string& operator[](const string& key) const
            throw(NoSuchElementException, AssertException, exception);

    private:
        bool hasNonEmptyElements() const throw(AssertException, exception);
        ostream& save(ostream& os) const throw(AssertException, exception);

        friend class ConfigFile;
    };

    /**
     * Reads, parses, modifies, and saves configuration files.  The file format used is as
     * follows:
     *
     * A config file consists of a series of sections.  Each section begins with a line
     * containing the section's name in brackets, and ends with the beginning of the next
     * section or the end of the file.  A section contains a series of key-value pairs,
     * each on a separate line.  Valid characters in section names and keys are
     * alphanumeric characters, whitespace, and '.'.
     *
     * A key and its value are separated by an '=' sign.  Whitespace before and after the
     * '=' sign is ignored.  If a value contains characters other than alphanumeric
     * characters and '.', it must be surrounded in double quotes.
     *
     * A '#' on any line begins a comment, which continues until the end of the line;
     * comments are ignored.  Blank lines are ignored.  Names of keys and sections can
     * contain spaces, but whitespace surrounding names is ignored.  There can be multiple
     * sections with the same name, and multiple keys with the same name within a section.
     * For example:
     *
     * <pre>
     * # Comment
     *
     * [foo]
     * bar = baz
     *
     * [foo2 bar]            # Comment
     * bar bar = bazbaz      # Comment
     * foo.bar = "baz baz"   # Comment
     *
     * [foo]
     * bar = foo
     * </pre>
     *
     * When a file is loaded, each section is represented by a ConfigSection object.
     *
     * This class is reference-counted.  It is not thread-safe; use a Mutex to synchronize
     * concurrent access.
     */
    class ConfigFile : public multimap<string, ConfigSection>
    {
    public:
        /**
         * Constructs an empty ConfigFile object.
         */
        ConfigFile() throw(AssertException, exception);

        /**
         * Loads a config file from an input stream.
         *
         * @exception ParseException if a syntax error is found in the file.
         */
        ConfigFile(istream& is) throw(ParseException, AssertException, exception);

        /**
         * Loads a config file from the filesystem.
         *
         * @param filename the name of the file to be loaded.
         * @exception IOException if an I/O error occurs while reading the file.
         * @exception ParseException if a syntax error is found in the file.
         */
        ConfigFile(const string& filename)
            throw(IOException, ParseException, AssertException, exception);

        ~ConfigFile() throw();

        /**
            Assign values from another ConfigFile object.
        */
        ConfigFile &operator=( const ConfigFile &cf );

        /**
         * Returns the first ConfigSection found that has the specified name.
         * Inserts an empty element if not found.
         */
        ConfigSection& operator[](const string& name)
            throw(AssertException, exception);

        /**
         * Returns the first ConfigSection found that has the specified name.
         *
         * @exception NoSuchElementException if no matching section is found.
         */
        const ConfigSection& operator[](const string& name) const
            throw(NoSuchElementException, AssertException, exception);

        /**
         * Clears the contents of this ConfigFile object.
         */
        void clear() throw(AssertException, exception);

        /**
         * Returns the first ConfigSection found that has the specified name, and contains
         * an element with the specified name and value.  If no such section is found, it
         * is created and returned.
         */
        ConfigSection& findFirstMatch(const string& section,
                                      const string& name,
                                      const string& value)
            throw(AssertException, exception);

        /**
         * Returns the first ConfigSection found that has the specified name, and contains
         * an element with the specified name and value.
         *
         * @exception NoSuchElementException if no matching section is found.
         */
        const ConfigSection& findFirstMatch(const string& section,
                                            const string& name,
                                            const string& value) const
            throw(NoSuchElementException, AssertException, exception);

        /**
         * Saves the config file.
         *
         * @param filename the name of the file to be saved.
         * @exception IOException if an I/O error occurs while saving the file.
         */
        void save(const string& filename) const
            throw(IOException, AssertException, exception);

        /**
         * Loads a config file from the filesystem.
         *
         * @param filename the name of the file to be loaded.
         * @exception IOException if an I/O error occurs while loading the file.
         * @exception ParseException if a syntax error is found in the file.
         */
        void load(const string& filename)
            throw(IOException, ParseException, AssertException, exception);

        /**
         * Saves the config file to an output stream.
         */
        ostream& save(ostream& os) const throw(AssertException, exception);

        /**
         * Reads a config file from an input stream.
         *
         * @exception ParseException if a syntax error is found in the file.
         */
        istream& load(istream& is) throw(ParseException, AssertException, exception);

    private:
        static const char* const SECTION_PATTERN;
        static const char* const VALUE_PATTERN;
        static const char* const COMMENT_PATTERN;
        const Regex section;
        const Regex value;
        const Regex comment;

        void throwParseException(int lineNum)
            throw(ParseException, AssertException, exception);
    };
}

#endif // CONFIGFILE_HPP
