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


#include "configfile.hpp"
#include "aprsdassert.hpp"
#include "aprsdexception.hpp"

#include <fstream>

#if __GNUG__ < 3
#   include <strstream>
#   define ostringstream ostrstream
#else
#   include <sstream>
#endif

namespace aprsd
{
    using std::ifstream;
    using std::ofstream;
    using std::ostringstream;

    ConfigSection::ConfigSection() throw(AssertException, exception) { }

    ConfigSection::~ConfigSection() throw() { }

    string& ConfigSection::operator[](const string& name)
        throw(AssertException, exception)
    {
        iterator i = find(name);
        if (i == end())
            i = insert(value_type(name, ""));
        return (*i).second;
    }

    const string& ConfigSection::operator[](const string& name) const
        throw(NoSuchElementException, AssertException, exception)
    {
        const_iterator i = find(name);
        if (i == end())
            throw NoSuchElementException("ConfigSection::operator[] const: "
                                         "element not found");
        return (*i).second;
    }

    bool ConfigSection::hasNonEmptyElements() const throw(AssertException, exception)
    {
        for (const_iterator i = begin(); i != end(); i++)
            if (!(*i).second.empty())
                return true;
        return false;
    }

    ostream& ConfigSection::save(ostream& os) const throw(AssertException, exception)
    {
        for (const_iterator i = begin(); i != end(); i++)
            if (!(*i).second.empty())
            {
                os << (*i).first << " = \"" << (*i).second << '"' << std::endl;
            }

        return os;
    }

    const char* const ConfigFile::SECTION_PATTERN =
        "^[[:space:]]*"                                // leading space
        "\\["                                          // open bracket
        "[[:space:]]*"                                 // space
        "([[:alnum:]._]+([[:space:]]+[[:alnum:]._]+)*)"  // words
        "[[:space:]]*"                                 // space
        "\\]"                                          // close bracket
        "[[:space:]]*"                                 // space
        "(#.*)?$";                                     // comment

    const char* const ConfigFile::VALUE_PATTERN =
        "^[[:space:]]*"                                // leading space
        "([[:alnum:]._]+([[:space:]]+[[:alnum:]._]+)*)"  // words
        "[[:space:]]*"                                 // space
        "="                                            // equals sign
        "[[:space:]]*"                                 // space
        "(([[:alnum:].]*)|(\"([^\"]*)\"))"             // quoted or non-quoted value
        "[[:space:]]*"                                 // space
        "(#.*)?$";                                     // comment

    const char* const ConfigFile::COMMENT_PATTERN =
        "^[[:space:]]*"                                // leading space
        "(#.*)?$";                                     // comment

    ConfigFile::ConfigFile() throw(AssertException, exception) :
        section(SECTION_PATTERN), value(VALUE_PATTERN), comment(COMMENT_PATTERN) { }

    ConfigFile::ConfigFile(istream& is) throw(ParseException, AssertException, exception) :
        section(SECTION_PATTERN), value(VALUE_PATTERN), comment(COMMENT_PATTERN)
    {
        load(is);
    }

    ConfigFile::ConfigFile(const string& file)
        throw(IOException, ParseException, AssertException, exception) :
        section(SECTION_PATTERN), value(VALUE_PATTERN), comment(COMMENT_PATTERN)
    {
        load(file);
    }

    ConfigFile::~ConfigFile() throw() { }

    ConfigFile &ConfigFile::operator=( const ConfigFile &cf )
    {
        static_cast<multimap<string, ConfigSection>&>( *this ) = cf;
        return *this;
    }

    ConfigSection& ConfigFile::operator[](const string& name)
        throw(AssertException, exception)
    {
        iterator i = find(name);
        if (i == end())
            i = insert(value_type(name, ConfigSection()));
        return (*i).second;
    }

    const ConfigSection& ConfigFile::operator[](const string& name) const
        throw(NoSuchElementException, AssertException, exception)
    {
        const_iterator i = find(name);
        if (i == end())
            throw NoSuchElementException("ConfigFile::operator[] const: "
                                         "element not found");
        return (*i).second;
    }

    ConfigSection& ConfigFile::findFirstMatch(const string& sSection,
                                              const string& sName,
                                              const string& sValue)
        throw(AssertException, exception)
    {
        std::pair<iterator,iterator> range = equal_range(sSection);

        for (; range.first != range.second; range.first++)
        {
            std::pair<ConfigSection::iterator,ConfigSection::iterator>
                sectrange = (*range.first).second.equal_range( sName );

            for (; sectrange.first != sectrange.second; sectrange.first++)
            {
                if ((*sectrange.first).second == sValue)
                {
                    return (*range.first).second;
                }
            }
        }

        ConfigSection sect;
        sect[ sName ] = value;
        return (*insert(value_type(sSection, sect ))).second;
    }

    const ConfigSection& ConfigFile::findFirstMatch(const string& sSection,
                                                    const string& sName,
                                                    const string& sValue) const
        throw(NoSuchElementException, AssertException, exception)
    {
        std::pair<const_iterator,const_iterator> range = equal_range(sSection);

        for (; range.first != range.second; range.first++)
        {
            std::pair<ConfigSection::const_iterator,ConfigSection::const_iterator>
                sectrange = (*range.first).second.equal_range( sName );

            for (; sectrange.first != sectrange.second; sectrange.first++)
            {
                if ((*sectrange.first).second == sValue)
                {
                    return (*range.first).second;
                }
            }
        }

        throw NoSuchElementException("ConfigFile::findFirstMatch(): element not found");
    }

    void ConfigFile::clear() throw(AssertException, exception)
    {
        erase(begin(), end());
    }

    void ConfigFile::save(const string& file) const
        throw(IOException, AssertException, exception)
    {
        ofstream outputFile(file.c_str());

        if (!outputFile)
        {
            throw IOException("Can't open " + file + " for output");
        }
        else if (!save(outputFile))
        {
            throw IOException("Error saving to " + file);
        }
    }

    void ConfigFile::load(const string& file)
        throw(IOException, ParseException, AssertException, exception)
    {
        ifstream inputFile(file.c_str());

        if (!inputFile)
        {
            throw IOException("Can't open " + file + " for input");
        }
        else if (!load(inputFile) && !inputFile.eof())
        {
            throw IOException("Error loading " + file);
        }
    }

    ostream& ConfigFile::save(ostream& os) const throw(AssertException, exception)
    {
        for (const_iterator i = begin(); i != end(); i++)
        {
            if ((*i).second.hasNonEmptyElements())
            {
                os << "[" << (*i).first << "]" << std::endl;
                (*i).second.save(os);
            }
        }

        return os;
    }

    void ConfigFile::throwParseException(int lineNum)
        throw(ParseException, AssertException, exception)
    {
        ostringstream msg;
        msg << "Syntax error on line " << lineNum << " of config file";
        throw ParseException(msg.str());
    }

    istream& ConfigFile::load(istream& is)
        throw(ParseException, AssertException, exception)
    {
        iterator currentSection = end();
        string line;
        for (int lineNum = 1; getline (is, line); ++lineNum)
        {
            try
            {
                RegexResult match = section.match(line);

                if (match.matched ()) {
                    RegexSubResult sectMatch = match [1];

                    currentSection = insert (std::pair<string,ConfigSection >(sectMatch.substr(line), ConfigSection()));
                } else if ((match = value.match(line)).matched ()) {
                    RegexSubResult nameMatch = match [1];

                    // Look for an unquoted value.
                    RegexSubResult valueMatch = match [4];

                    // Look for a quoted value.
                    if (!valueMatch.matched())
                        valueMatch = match [6];

                    if (!valueMatch.matched()) {
                        throwParseException(lineNum);
                    }

                    if (currentSection != end()) {
                        (*currentSection).second.insert(std::pair<string,string>(
                            nameMatch.substr(line),
                            valueMatch.substr(line)));
                    } else {
                        throwParseException(lineNum);
                    }
                } else if (!(match = comment.match(line)).matched()) {
                    throwParseException(lineNum);
                }
            }
            catch (RegexException& e)
            {
                throwParseException(lineNum);
            }
        }

        return is;
    }
}
