/**
 * \file Assert.hpp
 *
 * Contains the DEBUG macro and the Assert() function.
 */


#ifndef ASSERT_H
#define ASSERT_H

#include <string>

#ifndef DEBUG
#ifdef NDEBUG
#define DEBUG(x)        /* Debug assertion */
#else
#define DEBUG(x)        x
#endif
#endif

/**
 * \def DEBUG(x)
 *
 * Removes x from the source code if NDEBUG is defined.
 */

/**
 * Throws an object as an exception if an assertion is not met.  Can be used with the
 * DEBUG macro to selectively remove assertions from production code.  For example:
 *
 * <pre>
 * Assert(foo == bar, AssertException("failed!"));
 *
 * DEBUG(Assert(foo == bar, AssertException("failed!")));
 * </pre>
 *
 * The second assertion will be removed if NDEBUG is defined.
 */
namespace aprsd
{
template<class E> inline void Assert(bool assertion, E e)
{
    if (!assertion) throw e;
}
}

#endif // ASSERT_H
