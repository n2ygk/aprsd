#ifndef EXCEPTION_H
#define EXCEPTION_H

#include <exception>
#include <string>

namespace aprsd
{
    using std::string;
    using std::exception;
    /**
    * A base class for exceptions.
    */
    class Exception {
    public:
        /**
        * Constructs an Exception with an optional detail message.
        *
        * param message a detail message indicating the reason for the
        * exception.
        */
        explicit Exception(const string& message = "") throw(exception);

        /**
        * Destructor.
        */
        virtual ~Exception() throw();

        /**
        * return the exception's class name.
        */
        virtual string getName() const throw(exception);

        /**
        * @return a detail message indicating the reason for the
        * exception.
        */
        virtual string getMessage() const throw(exception);

        /**
        * A concatenation of the exception's class name and its detail
        * message, if available.
        */
        string toString() const throw(exception);

    private:
        string message;
    };


    /**
    * Indicates that an assertion failed.
    */
    class AssertException : public Exception
    {
    public:
        explicit AssertException(const string& message = "") throw(exception);

        virtual string getName() const throw(exception);
    };


    /**
    * Indicates that an unexpected exception was thrown.
    *
    * see ExceptionGuard
    */
    class UnexpectedException : public AssertException
    {
    public:
        explicit UnexpectedException(const string& message = "") throw(exception);

        virtual string getName() const throw(exception);
    };


    /**
    * Indicates that an I/O error has occurred.
    */
    class IOException : public Exception
    {
    public:
        explicit IOException(const string& message = "") throw(exception);

        virtual string getName() const throw(exception);
    };


    /**
    * Indicates that the IP address of a host could not be resolved.
    */
    class UnknownHostException : public IOException
    {
    public:
        explicit UnknownHostException(const string& message = "") throw(exception);

        virtual string getName() const throw(exception);
    };


    /**
    * Indicates that an operation has timed out.
    */
    class TimeoutException : public IOException
    {
    public:
        explicit TimeoutException(const string& message = "") throw(exception);

        virtual string getName() const throw(exception);
    };


    /**
    * Indicates that an error occurred while performing an operation
    * on a socket.
    */
    class SocketException : public IOException
    {
    public:
        explicit SocketException(const string& message = "") throw(exception);

        virtual string getName() const throw(exception);
    };


    /**
    * Indicates that an error occurred while attempting to bind a
    * socket to a local address and port.
    */
    class BindException : public SocketException
    {
    public:
        explicit BindException(const string& message = "") throw(exception);

        virtual string getName() const throw(exception);
    };


    /**
    * Indicates that an error occurred while attempting to connect a
    * socket to a remote address and port.
    */
    class ConnectException : public SocketException
    {
    public:
        explicit ConnectException(const string& message = "") throw(exception);

        virtual string getName() const throw(exception);
    };


    /**
    * Indicates that the regular expression engine returned an error.
    */
    class RegexException : public Exception
    {
    public:
        explicit RegexException(const string& message = "") throw(exception);

        virtual string getName() const throw(exception);
    };

    /**
    * Indicates that the element does not exist.
    */
    class NoSuchElementException : public Exception
    {
    public:
        explicit NoSuchElementException(const string& message = "") throw(exception);

        virtual string getName() const throw(exception);
    };

    /**
    * Indicates that the operation is not supported by the library/OS.
    */
    class UnsupportedOperationException : public Exception
    {
    public:
        explicit UnsupportedOperationException(const string& message = "") throw(exception);

        virtual string getName() const throw(exception);
    };


    /**
    * Indicates that an error was found while parsing text.
    */
    class ParseException : public Exception
    {
    public:
        explicit ParseException(const string& message = "") throw(exception);

        virtual string getName() const throw(exception);
    };


    /**
    * Indicates that an error occurred while trying to control
    * a thread or process.
    */
    class ThreadControlException : public Exception
    {
    public:
        explicit ThreadControlException(const string& message = "") throw(exception);

        virtual string getName() const throw(exception);
    };
}
#endif //_APRSD_EXCEPTION
