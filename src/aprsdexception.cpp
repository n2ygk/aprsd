/*
 * $Id$
 *
 * aprsd, Automatic Packet Reporting System Daemon
 * Copyright (C) 1997,2002 Dale A. Heatherington, WA4DSY
 * Copyright (C) 2001-2002 aprsd Dev Team
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

#include "aprsdexception.hpp"

namespace aprsd
{
    Exception::Exception(const string& arg_message) : m_message(arg_message) { }

    Exception::~Exception() throw() { }

    string Exception::getMessage() const
    {
        return m_message;
    }

    string Exception::toString() const
    {
        if (getMessage().empty())
            return getName();
        else
            return getName() + ": " + getMessage();
    }

    const char *Exception::what( void ) const throw()
    {
        return m_message.c_str();
    }

    string Exception::getName() const
    {
        return "aprsd::Exception";
    }

    AssertException::AssertException(const string& message) :
        Exception(message) { }

    string AssertException::getName() const
    {
        return "aprsd::AssertException";
    }

    UnexpectedException::UnexpectedException(const string& message) :
        AssertException(message) { }

    string UnexpectedException::getName() const
    {
        return "aprsd::UnexpectedException";
    }

    IOException::IOException(const string& message) throw() :
        Exception(message) { m_errno = 0; }

    IOException::IOException( int ERRNUM ) throw() {
        m_message = strerror_r(ERRNUM);
        m_errno = ERRNUM;
    }

    string IOException::getName() const
    {
        return "aprsd::IOException";
    }

    UnknownHostException::UnknownHostException(const string& message)
        : IOException(message) { }

    string UnknownHostException::getName() const
    {
        return "aprsd::UnknownHostException";
    }

    TimeoutException::TimeoutException(const string& message) :
        IOException(message) { }

    string TimeoutException::getName() const
    {
        return "aprsd::TimeoutException";
    }

    SocketException::SocketException(const string& message) :
        IOException(message) { }


    string SocketException::getName() const
    {
        return "aprsd::SocketException";
    }

    BindException::BindException(const string& message) :
        SocketException(message) { }

    string BindException::getName() const
    {
        return "aprsd::BindException";
    }

    ConnectException::ConnectException(const string& message) :
        SocketException(message) { }

    string ConnectException::getName() const
    {
        return "aprsd::ConnectException";
    }

    RegexException::RegexException(const string& message) :
        Exception(message) { }

    string RegexException::getName() const
    {
        return "aprsd::RegexException";
    }

    NoSuchElementException::NoSuchElementException(const string& message) :
        Exception(message) { }

    string NoSuchElementException::getName() const
    {
        return "aprsd::NoSuchElementException";
    }

    UnsupportedOperationException::UnsupportedOperationException(const string& message)
        : Exception(message) { }

    string UnsupportedOperationException::getName() const
    {
        return "aprsd::UnsupportedOperationException";
    }

    ParseException::ParseException(const string& message) :
        Exception(message) { }

    string ParseException::getName() const
    {
        return "aprsd::ParseException";
    }

    ThreadControlException::ThreadControlException( const string& message ) :
        Exception(message) { }

    string ThreadControlException::getName() const
    {
        return "aprsd::ThreadControlException";
    }

    RefCountException::RefCountException( const string& message ) 
        : Exception( message ) {}

    string RefCountException::getName() const
    {
        return "aprsd::RefCountException";
    }
}

