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


#ifndef APRSD_EXCEPTION_H
#define APRSD_EXCEPTION_H

#include "osdep.hpp"
#include <exception>
#include <string>

namespace aprsd
{
    using std::string;
    using std::exception;
    /**
    * A base class for exceptions.
    */
    class Exception : public std::exception 
    {
    public:
        /**
        * Constructs an Exception with an optional detail message.
        *
        * param message a detail message indicating the reason for the
        * exception.
        */
        explicit Exception(const string& message = "");

        /**
        * Destructor.
        */
        virtual ~Exception() throw();

        /**
        * return the exception's class name.
        */
        virtual string getName() const;

        /**
        * @return a detail message indicating the reason for the
        * exception.
        */
        virtual string getMessage() const;

        /**
        * A concatenation of the exception's class name and its detail
        * message, if available.
        */
        string toString(void) const;
        
        /**
            Return only the message, but following the normal C++ notation !

            @return a ziro terminated const char string 
        */
        const char *what( void ) const throw();

    protected:
        string m_message;
    };


    /**
    * Indicates that an assertion failed.
    */
    class AssertException : public Exception
    {
    public:
        explicit AssertException(const string& message = "");

        virtual string getName() const;
    };


    /**
    * Indicates that an unexpected exception was thrown.
    *
    * see ExceptionGuard
    */
    class UnexpectedException : public AssertException
    {
    public:
        explicit UnexpectedException(const string& message = "");

        virtual string getName() const;
    };


    /**
    * Indicates that an I/O error has occurred.
    */
    class IOException : public Exception
    {
    public:
        explicit IOException( const string& message = "" ) throw();
        explicit IOException( int ERRNUM ) throw();
        ~IOException() throw(){}

        virtual string getName() const;

        int getErrno( void ) const { return m_errno;}
    private:
        int m_errno;
    };


    /**
    * Indicates that the IP address of a host could not be resolved.
    */
    class UnknownHostException : public IOException
    {
    public:
        explicit UnknownHostException( const string& message = "" );
        explicit UnknownHostException( int ERRNUM ) throw() : IOException( ERRNUM ) {}

        virtual string getName() const;
    };


    /**
    * Indicates that an operation has timed out.
    */
    class TimeoutException : public IOException
    {
    public:
        explicit TimeoutException( const string& message = "" );
        explicit TimeoutException( int ERRNUM ) throw() : IOException( ERRNUM ) {}

        virtual string getName() const;
    };


    /**
    * Indicates that an error occurred while performing an operation
    * on a socket.
    */
    class SocketException : public IOException
    {
    public:
        explicit SocketException( const string& message = "" );
		explicit SocketException( int ERRNUM ) throw() : IOException( ERRNUM ) {}

        virtual string getName() const;
    };


    /**
    * Indicates that an error occurred while attempting to bind a
    * socket to a local address and port.
    */
    class BindException : public SocketException
    {
    public:
        explicit BindException( const string& message = "" );
		explicit BindException( int ERRNUM ) throw() : SocketException( ERRNUM ) {}

        virtual string getName() const;
    };


    /**
    * Indicates that an error occurred while attempting to connect a
    * socket to a remote address and port.
    */
    class ConnectException : public SocketException
    {
    public:
       explicit ConnectException(const string& message = "");
		explicit ConnectException( int ERRNUM ) throw() : SocketException( ERRNUM ) {}

        virtual string getName() const;
    };


    /**
    * Indicates that the regular expression engine returned an error.
    */
    class RegexException : public Exception
    {
    public:
        explicit RegexException(const string& message = "");

        virtual string getName() const;
    };

    /**
    * Indicates that the element does not exist.
    */
    class NoSuchElementException : public Exception
    {
    public:
        explicit NoSuchElementException(const string& message = "");

        virtual string getName() const;
    };

    /**
    * Indicates that the operation is not supported by the library/OS.
    */
    class UnsupportedOperationException : public Exception
    {
    public:
        explicit UnsupportedOperationException(const string& message = "");

        virtual string getName() const;
    };


    /**
    * Indicates that an error was found while parsing text.
    */
    class ParseException : public Exception
    {
    public:
        explicit ParseException(const string& message = "");

        virtual string getName() const;
    };


    /**
    * Indicates that an error occurred while trying to control
    * a thread or process.
    */
    class ThreadControlException : public Exception
    {
    public:
        explicit ThreadControlException(const string& message = "");

        virtual string getName() const;
    };
    
    /**
        This indicate an error occurred while using refcounting.
    */
    class RefCountException : public Exception
    {
    public:
        explicit RefCountException(const string& message = "");

        virtual string getName() const;
    };
}
#endif // APRSD_EXCEPTION_H
