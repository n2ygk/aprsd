
#include "exceptionguard.h"

namespace aprsd
{
    void assertUnexpected() throw(UnexpectedException)
    {
        try {
            throw;
        }
        catch (Exception& e) {
            throw UnexpectedException(e.toString());
        }
        catch (exception& e) {
            throw UnexpectedException(string("Unexpected system exception: ") + e.what());
        }
        catch (...) {
            throw UnexpectedException("Exception thrown of unknown type");
        }
    }

    ExceptionGuard::ExceptionGuard() throw()
    {
        old = std::set_unexpected(&assertUnexpected);
    }

    ExceptionGuard::~ExceptionGuard() throw()
    {
        std::set_unexpected(old);
    }
}
