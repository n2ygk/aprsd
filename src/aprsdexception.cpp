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

#include "aprsdexception.h"

namespace aprsd
{
    Exception::Exception(const string& arg_message) throw(exception) :
        message(arg_message) { }

    Exception::~Exception() throw() { }

    string Exception::getMessage() const throw(exception)
    {
        return message;
    }

    string Exception::toString() const throw(exception)
    {
        if (getMessage().empty())
        {
            return getName();
        } else {
            return getName() + ": " + getMessage();
        }
    }

    string Exception::getName() const throw(exception)
    {
        return "aprsd::Exception";
    }

    AssertException::AssertException(const string& message) throw(exception) :
        Exception(message) { }

    string AssertException::getName() const throw(exception)
    {
        return "aprsd::AssertException";
    }

    UnexpectedException::UnexpectedException(const string& message) throw(exception) :
        AssertException(message) { }

    string UnexpectedException::getName() const throw(exception)
    {
        return "aprsd::UnexpectedException";
    }

    IOException::IOException(const string& message) throw(exception) :
        Exception(message) { }

    string IOException::getName() const throw(exception)
    {
        return "aprsd::IOException";
    }

    UnknownHostException::UnknownHostException(const string& message)
        throw(exception) : IOException(message) { }

    string UnknownHostException::getName() const throw(exception)
    {
        return "aprsd::UnknownHostException";
    }

    TimeoutException::TimeoutException(const string& message) throw(exception) :
        IOException(message) { }

    string TimeoutException::getName() const throw(exception)
    {
        return "aprsd::TimeoutException";
    }

    SocketException::SocketException(const string& message) throw(exception) :
        IOException(message) { }


    string SocketException::getName() const throw(exception)
    {
        return "aprsd::SocketException";
    }

    BindException::BindException(const string& message) throw(exception) :
        SocketException(message) { }

    string BindException::getName() const throw(exception)
    {
        return "aprsd::BindException";
    }

    ConnectException::ConnectException(const string& message) throw(exception) :
        SocketException(message) { }

    string ConnectException::getName() const throw(exception)
    {
        return "aprsd::ConnectException";
    }

    RegexException::RegexException(const string& message) throw(exception) :
        Exception(message) { }

    string RegexException::getName() const throw(exception)
    {
        return "aprsd::RegexException";
    }

    NoSuchElementException::NoSuchElementException(const string& message) throw(exception) :
        Exception(message) { }

    string NoSuchElementException::getName() const throw(exception)
    {
        return "aprsd::NoSuchElementException";
    }

    UnsupportedOperationException::UnsupportedOperationException(const string& message)
        throw(exception) : Exception(message) { }

    string UnsupportedOperationException::getName() const throw(exception)
    {
        return "aprsd::UnsupportedOperationException";
    }

    ParseException::ParseException(const string& message) throw(exception) :
        Exception(message) { }

    string ParseException::getName() const throw(exception)
    {
        return "aprsd::ParseException";
    }

    ThreadControlException::ThreadControlException(const string& message) throw(exception) :
        Exception(message) { }

    string ThreadControlException::getName() const throw(exception)
    {
        return "aprsd::ThreadControlException";
    }
}

