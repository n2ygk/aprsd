# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.57)
AC_INIT(aprsd, 2.2.5-4, cbyam@virginia.edu)
AC_CONFIG_AUX_DIR(support)
AC_CONFIG_SRCDIR([src/aprsd.cpp])
AM_CONFIG_HEADER([config.h])
AM_INIT_AUTOMAKE

AC_DEFINE(TOCALL, "APD225", [Defines the AX25 tocall constant])

# Checks for programs.
AC_PROG_CXX
AC_PROG_CC

if test "$GCC" = "yes"; then
	CXXFLAGS="$CXXFLAGS -Wall -pipe -pthread"
fi


# Checks for libraries.
AC_CHECK_LIB([pthread], [pthread_mutexattr_init])
AC_CHECK_LIB([crypt], [crypt])

# Checks for header files.
AC_HEADER_STDC
AC_HEADER_SYS_WAIT
AC_CHECK_HEADERS([arpa/inet.h fcntl.h netdb.h netinet/in.h shadow.h stdlib.h string.h sys/ioctl.h sys/socket.h sys/time.h sys/timeb.h termios.h unistd.h])
AC_CHECK_HEADERS([pthread.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDC
AC_HEADER_SYS_WAIT
AC_CHECK_HEADERS([arpa/inet.h fcntl.h netdb.h netinet/in.h shadow.h stdlib.h string.h sys/ioctl.h sys/socket.h sys/time.h sys/timeb.h termios.h unistd.h])

AC_DEFINE(_GNU_SOURCE, 1, [This is a GNU compliant source, use GNU extensions.])
AC_DEFINE(_REENTRANT, 1, Use reentrant code if available.)



# Checks for library functions.
AC_FUNC_FORK
AC_PROG_GCC_TRADITIONAL
AC_FUNC_MALLOC
AC_FUNC_MEMCMP
AC_FUNC_SELECT_ARGTYPES
AC_TYPE_SIGNAL
AC_FUNC_STAT
AC_FUNC_STRERROR_R
AC_CHECK_FUNCS([gethostbyname gethostname getspnam gettimeofday inet_ntoa memchr memmove memset select socket strchr strdup strerror strpbrk strstr])



AC_CONFIG_FILES([Makefile \
	src/Makefile \
	support/Makefile \
	doc/Makefile \
	admin/Makefile])
AC_OUTPUT
