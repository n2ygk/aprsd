
#ifndef EXCEPTION_GUARD
#define EXCEPTION_GUARD

#include "exception.h"

namespace aprsd
{
    using std::unexpected_handler;

    extern void assertUnexpected() throw(UnexpectedException);

    /**
     * Catches unexpected exceptions in a program, and rethrows them as
     * UnexpectedException objects.  UnexpectedException is derived from AssertException,
     * which passes to user code.  If the unexpected exception is derived from
     * Exception or std::exception, the UnexpectedException object's detail
     * message will contain a description of it.
     *
     * To use this feature, just instantiate an ExceptionGuard as a local variable at the
     * beginning of your program's main() method.
     *
     * This class is not reference-counted.
     *
     */
    class ExceptionGuard
    {
        std::unexpected_handler old;

    public:
        /**
         * Sets an unexpected_handler that rethrows exceptions as UnexpectedException
         * objects.
         */
        ExceptionGuard() throw();

        /**
         * Restores the original unexpected_handler.
         */
        ~ExceptionGuard() throw();
    };
}

#endif /* EXCEPTION_GUARD */
