dnl ------------------------------------------------------------------------
dnl Find a file (or one of more files in a list of dirs)
dnl ------------------------------------------------------------------------
dnl
AC_DEFUN(AC_FIND_FILE,
[
$3=NO
for i in $2;
do
  for j in $1;
  do
    if test -r "$i/$j"; then
      $3=$i
      break 2
    fi
  done
done
])

AC_DEFUN(APRSD_FIND_PATH,
[
  AC_MSG_CHECKING([for $1])
  if test -n "$$2"; then
    aprsd_cv_path="$$2";
  else
    aprsd_cache=`echo $1 | sed 'y%./+-%__p_%'`

    AC_CACHE_VAL(aprsd_cv_path$aprsd_cache,
    [
    aprsd_cv_path="NONE"
    dirs="$3"
    aprsd_save_IFS=$IFS
    IFS=':'
    for dir in $PATH; do
      dirs="$dirs $dir"
    done
    IFS=$aprsd_save_IFS

    for dir in $dirs; do
      if test -x "$dir/$1"; then
        if test -n "$5"
        then
          evalstr="$dir/$1 $5 2>&1 "
          if eval $evalstr; then
            aprsd_cv_path="$dir/$1"
            break
          fi
        else
          aprsd_cv_path="$dir/$1"
          break
        fi
      fi
    done

    eval "aprsd_cv_path_$aprsd_cache=$aprsd_cv_path"

    ])
    eval "aprsd_cv_path=\"`echo '$aprsd_cv_path_'$aprsd_cache`\""

  fi

if test
  -z "$aprsd_cv_path" || test "aprsd_cv_path" = NONE;
then AC_MSG_RESULT(not found)
  $4
else
  AC_MSG_RESULT($aprsd_cv_path)
  $2=$aprsd_cv_path
fi
])

AC_DEFUN(APRSD_MISC_TESTS,
[
  AC_LANG_C
  dnl Checks for libraries.
  AC_CHECK_LIB(compat, main, [LIBCOMPAT="-lcompat"]) dnl for FreeBSD
  AC_SUBST(LIBCOMPAT)
  aprsd_have_crypt=
  AC_CHECK_LIB(crypt, crypt, [LIBCRYPT="-lcrypt"; aprsd_have_crypt=yes],
    AC_CHECK_LIB(c, crypt, [aprsd_have_crypt=yes], [
      AC_MSG_WARN([you have no crypt in either libcrypt or libc.  You should install libcrypt from another source or configure with PAM support])
      aprsd_have_crypt=no
    ]))
  AC_SUBST(LIBCRYPT)
  if test $aprsd_have_crypt = yes; then
    AC_DEFINE_UNQUOTED(HAVE_CRYPT, 1, [Defines if your system has the crypt function])
    LIBS="$LIBS $LIBCRYPT"
  fi
  AC_CHECK_KSIZE_T
  AC_LANG_C
  AC_CHECK_LIB(dnet, dnet_ntoa, [EXTRA_LIBS="$X_EXTRA_LIBS -ldnet"])
  if test $ac_cv_lib_dnet_dnet_ntoa = no; then
    AC_CHECK_LIB(dnet_stub, dnet_ntoa,
      [EXTRA_LIBS="$EXTRA_LIBS -ldnet_stub"])
  fi
  AC_CHECK_FUNC(connect)
  if test $ac_cv_func_connect = no; then
    AC_CHECK_LIB(socket, connect, EXTRA_LIBS="-lsocket $EXTRA_LIBS", ,
      $X_EXTRA_LIBS)
  fi
  AC_CHECK_FUNC(remove)
  if test $ac_cv_func_remove = no; then
    AC_CHECK_LIB(posix, remove, EXTRA_LIBS="$EXTRA_LIBS -lposix")
  fi

  LIBSOCKET="$EXTRA_LIBS"
  AC_SUBST(LIBSOCKET)
  AC_SUBST(EXTRA_LIBS)
  AC_CHECK_LIB(ucb, killpg, [LIBUCB="-lucb"]) dnl for Solaris2.4
  AC_SUBST(LIBUCB)

  case $host in dnl this *is* LynxOS specif
  *-*-lynxos* )
    AC_MSG_CHECKING([LynxOS header file wrappers])
    [CFLAGS="$CFLAGS -D__NO_INCLUDE_WARN__"]
    AC_MSG_RESULT(disabled)
    AC_CHECK_LIB(bsd, gethostbyname, [LIBSOCKET="-lbsd"]) dnl for LynxOS
    ;;
  esac
  LIBS="$LIBS $EXTRA_LIBS"
  APRSD_CHECK_TYPES
  APRSD_CHECK_LIBDL
  AC_CHECK_BOOL
])

dnl Slightly changed version of AC_CHECK_FUNC(setenv)
dnl checks for unsetenv too
AC_DEFUN(AC_CHECK_SETENV,
[AC_MSG_CHECKING([for setenv])
AC_CACHE_VAL(ac_cv_func_setenv,
[AC_LANG_C
AC_TRY_LINK(
dnl Don't include <ctype.h> because OSF/1 3.0 it includes <sys/types.h>
dnl which includes <sys/select.h> which contains a prototype for
dnl select.  Similarly for bzero.
[#include <assert.h>
]ifelse(AC_LANG, CPLUSPLUS, [#ifdef __cplusplus
extern "C"
#endif
])dnl
[/* We use char because int might match the return type of a gcc2
    builtin and then its argument prototype would still apply. */
#include <stdlib.h>
], [
/* The GNU C library defines this for functions which it implements
   to always fail with ENOSYS.  Some functions are actually named
   something starting with __ and the normal name is an alias. */
#if defined (__stub_$1) || defined (__stub___$1)
choke me
#else
setenv("TEST", "alle", 1);
#endif
], eval "ac_cv_func_setenv=yes", eval "ac_cv_func_setenv=no")])

if test "$ac_cv_func_setenv" = "yes"; then
  AC_MSG_RESULT(yes)
  AC_DEFINE_UNQUOTED(HAVE_FUNC_SETENV, 1, [Define if you have setenv])
else
  AC_MSG_RESULT(no)
fi

AC_MSG_CHECKING([for unsetenv])
AC_CACHE_VAL(ac_cv_unsetenv,
[AC_LANG_C
AC_TRY_LINK(
dnl Don't include <ctype.h> because on OSF/1 3.0 it includes <sys/types.h>
dnl which includes <sys/select.h> which contains a prototype for
dnl select.  Similarly for bzero.
[#include <assert.h>
]ifelse(AC_LANG, CPLUSPLUS, [#ifdef __cplusplus
extern "C"
#endif
])dnl
[/* The GNU C library defines this for functions which it implements
    to always fail with ENOSYS.  Some functions are actually named
    something starting with __ and the normal name is an alias. */
#if defined (__stub_$1) || defined (___stub___$1)
choke me
#else
unsetenv("TEST");
#endif
], eval "ac_cv_func_unsetenv=yes", eval "ac_cv_func_unsetenv=no")])

if test "$ac_cv_func_unsetenv" = "yes"; then
  AC_MSG_RESULT(yes)
  AC_DEFINE_UNQUOTED(HAVE_FUNC_UNSETENV, 1, [Define if you have unsetenv])
else
  AC_MSG_RESULT(no)
fi
])

AC_DEFUN(AC_CHECK_GETDOMAINNAME,
[
AC_MSG_CHECKING(for getdomainname)
AC_CAHCE_VAL(ac_cv_func_getdomainname,
[
AC_LANG_SAVE
AC_LANG_CPLUSPLUS
save_CXXFLAGS="$CXXFLAGS"
if test "$GCC" = "yes"; then
CXXFLAGS="$CXXFLAGS -pedantic-errors"
fi
AC_TRY_COMPILE([
#include <stdlib.h>
#include <unistd.h>
],
[
char buffer[200];
getdomainname(buffer, 200);
],
ac_cv_func_getdomainname=yes,
ac_cv_func_getdomainname=no)
CXXFLAGS="$save_CXXFLAGS"
AC_LANG_RESTORE
])
AC_MSG_RESULT($ac_cv_func_getdomainname)
if eval "test \"`echo $ac_cv_func_getdomainname`\" = yes"; then
  AC_DEFINE(HAVE_GETDOMAINNAME, 1, [Define if you have getdomainame])
fi
])

AC_DEFUN(AC_CHECK_GETHOSTNAME,
[
AC_MSG_CHECKING([for gethostname])
AC_CACHE_VAL(ac_cv_func_gethostname,
[
AC_LANG_SAVE
AC_LANG_CPLUSPLUS
save_CXXFLAGS="$CXXFLAGS"
if test "$GCC" = "yes"; then
CXXFLAGS="$CXXFLAGS -pendantic-errors"
fi
AC_TRY_COMPILE([
#include <stdlib.h>
#include <unistd.h>
],
[
char buffer[200];
gethostname(buffer, 200);
],
ac_cv_func_gethostname=yes,
ac_cv_func_gethostname=no)
CXXFLAGS="$save_CXXFLAGS"
AC_LANG_RESTORE
])
AC_MSG_RESULT($ac_cv_func_gethostname)
if eval "test \"`echo $ac_cv_gethostname`\" = yes"; then
  AC_DEFINE(HAVE_GETHOSTNAME, 1, [Define if you have gethosname])
fi
])

AC_DEFUN(AC_CHECK_USLEEP,
[
AC_MSG_CHECKING([for usleep])
AC_CACHE_VAL(ac_cv_func_usleep,
[
AC_LANG_SAVE
AC_LANG_CPLUSPLUS
ac_libs_safe="$LIBS"
LIBS="$LIBS $LIBUCB"
AC_TRY_LINK([
#include <stdlib.h>
#include <unistd.h>
],
[
usleep(200);
],
ac_cv_func_usleep=yes,
ac_cv_func_usleep=no)
LIBS="$ac_libs_safe"
AC_LANG_RESTORE
])
AC_MSG_RESULT($ac_cv_func_usleep)
if eval "test \"`echo $ac_cv_func_usleep`\" = yes"; then
  AC_DEFINE(HAVE_USLEEP, 1, [Define if you have the usleep function])
fi
])

AC_DEFUN(AC_CHECK_RANDOM,
[
AC_MSG_CHECKING([for random])
AC_CACHE_VAL(ac_cv_func_random,
[
AC_LANG_SAVE
AC_LANG_CPLUSPLUS
ac_libs_safe="$LIBS"
LIBS="$LIBS $LIBSUCB"
AC_TRY_LINK([
#include <stdlib.h>
],
[random();
],
ac_cv_func_random=yes,
ac_cv_func_random=no)
LIBS="$ac_libs_safe"
AC_LANG_RESTORE
])
AC_MSG_RESULT($ac_cv_func_random)
if eval "test \"`echo $ac_cv_func_random`\" = yes"; then
  AC_DEFINE(HAVE_RANDOM, 1, [Define if you have random])
fi
])


dnl Check for the type of the third argument of getsockname
AC_DEFUN(AC_CHECK_KSIZE_T,
[AC_MSG_CHECKING(for the third argument of getsockname)
AC_CACHE_VAL(ac_cv_ksize_t,
AC_LANG_SAVE
AC_LANG_CPLUSPLUS
[AC_TRY_COMPILE([
#include <sys/types.h>
#include <sys/socket.h>
],[
socklen_t a=0;
getsockname(0,(struct sockaddr*)0, &a);
],
ac_cv_ksize_t=socklen_t,
ac_cv_ksize_t=)
if test -z "$ac_cv_ksize_t"; then
ac_safe_cxxflags="$CXXFLAGS"
if test "$GCC" = "yes"; then
  CXXFLAGS="-Werror $CXXFLAGS"
fi
AC_TRY_COMPILE([
#include <sys/types.h>
#include <sys/socket.h>
],[
int a=0;
getsockname(0,(struct sockaddr*)0, &a);
],
ac_cv_ksize_t=int,
ac_cv_ksize_t=size_t)
CXXFLAGS="$ac_safe_cxxflags"
fi
AC_LANG_RESTORE
])

if test -z "$ac_cv_ksize_t"; then
  ac_cv_ksize_t=int
fi

AC_MSG_RESULT($ac_cv_ksize_t)
AC_DEFINE_UNQUOTED(ksize_t, $ac_cv_ksize_t,
      [Define the type of the third argument for getsockname]
)

])


AC_DEFUN(AC_CHECK_BOOL,
[
	AC_MSG_CHECKING([for bool])
        AC_CACHE_VAL(ac_cv_have_bool,
        [
                AC_LANG_SAVE
		AC_LANG_CPLUSPLUS
          	AC_TRY_COMPILE([],
                 [bool aBool = true;],
                 [ac_cv_have_bool="yes"],
                 [ac_cv_have_bool="no"])
                AC_LANG_RESTORE
        ]) dnl end AC_CHECK_VAL
        AC_MSG_RESULT($ac_cv_have_bool)
        if test "$ac_cv_have_bool" = "yes"; then
        	AC_DEFINE(HAVE_BOOL, 1, [Define if the C++ compiler supports BOOL])
        fi

])

AC_DEFUN(APRSD_CHECK_TYPES,
[  AC_CHECK_SIZEOF(int, 4)dnl
  AC_CHECK_SIZEOF(long, 4)dnl
  AC_CHECK_SIZEOF(char *, 4)dnl
])dnl

AC_DEFUN(APRSD_CHECK_LIBDL,
[
AC_CHECK_LIB(dl, dlopen, [
LIBDL="-ldl"
ac_cv_have_dlfcn=yes
])

AC_SUBST(LIBDL)
])





AC_DEFUN(AM_INIT_AUTOMAKE,
[AC_REQUIRE([AC_PROG_INSTALL])
PACKAGE=[$1]
AC_SUBST(PACKAGE)
VERSION=[$2]
AC_SUBST(VERSION)
if test "`cd $srcdir && pwd`" != "`pwd`" && test -f $srcdir/config.status; then
  AC_MSG_ERROR([source directory already configured; run "make distclean" there first])
fi
ifelse([$3],,
AC_DEFINE_UNQUOTED(PACKAGE, "$PACKAGE", [Name of package])
AC_DEFINE_UNQUOTED(VERSION, "$VERSION", [Version number of package]))
AC_REQUIRE([AM_SANITY_CHECK])
AC_REQUIRE([AC_ARG_PROGRAM])
dnl FixMe this is really gross
missing_dir=`cd $ac_aux_dir && pwd`
AM_MISSING_PROG(ACLOCAL, aclocal, $missing_dir)
AM_MISSING_PROG(AUTOCONF, autoconf, $missing_dir)
AM_MISSING_PROG(AUTOMAKE, automake, $missing_dir)
AM_MISSING_PROG(AUTOHEADER, autoheader, $missing_dir)
AC_REQUIRE([AC_PROG_MAKE_SET])])

AC_DEFUN(AM_SANITY_CHECK,
[AC_MSG_CHECKING([whether build environmnet is sane])
# Just in case
sleep 1
echo timestamp > conftestfile
#
# Do 'set' in a subshell so we don't clobber the current shell's
# arguments.  Must try -L first in case configure is actually a
# symlink; some systems play weird games with the mod time of symlinks
# (eg, FreeBSD returns the mod time of the symlink's containing
# directory).
if (
   set X `ls -Lt $srcdir/configure conftestfile 2> /dev/null`
   if test "[$]*" = "X"; then
      # -L didn't work
      set X `ls -t $srcdir/configure conftestfile`
   fi 
   if test "[$]*" != "X $srcdir/configure conftestfile" \
      && test "[$]*" != "X conftestfile $srcdir/configure"; then

      # if neither matched, then we have a broken ls.  This can happen
      # if, for instance, CONFIG_SHELL is bash and it inherits a 
      # broken ls alias from the environment.  This has actually 
      # happened.  Such a system could not be considered "sane".
      AC_MSG_ERROR([ls -t appears to fail.  Make sure there is not a broken alias in your environment])
   fi

   test "[$]2" = conftestfile
   )
then
   # OK
   :
else
   AC_MSG_ERROR([newly created file is older than distributed files! Check your system clock])
fi
rm -f conftest*
AC_MSG_RESULT(yes)])

dnl AM_MISSING_PROG(NAME, PROGRAM, DIRECTORY)
AC_DEFUN(AM_MISSING_PROG,
[AC_MSG_CHECKING(for working $2)
# Run test in a subshell; some versions of sh will print an error if
# an executable is not found, even if stderr is redirected.
# Redirect stdin to placate older versions of autoconf. Sigh.
if ($2 --version) < /dev/null > /dev/null 2>&1; then
   $1=$2
   AC_MSG_RESULT(found)
else
   $1="$3/missing $2"
   AC_MSG_RESULT(missing)
fi
AC_SUBST($1)])

AC_DEFUN(AM_CONFIG_HEADER,
[AC_PREREQ([2.12])
AC_CONFIG_HEADER([$1])
dnl When config.status generates a header, we must update the stamp-h file.
dnl This file resides in the same directory as the config header
dnl that is generated.  We must strip everything past the first ":",
dnl and everything past the last "/".
AC_OUTPUT_COMMANDS(changequote(<<,>>)dnl
ifelse(patsubst(<<$1>>, <<[^ ]>>, <<>>), <<>>,
<<test -z "<<$>>CONFIG_HEADERS" || echo timestamp > patsubst(<<$1>>, <<^\([^:]*/\)?.*>>, <<\1>>)stamp-h<<>>dnl>>,
<<am_indx=1
for am_file in <<$1>>; do
  case " <<$>>CONFIG_HEADERS " in
  *" <<$>>am_file "*<<)>>
    echo timestamp > `echo <<$>>am_file | sed -e 's%:.*%%' -e 's%[^/]*$%%'`stamp-h$am_indx
    ;;
  esac
  am_indx=`expr "<<$>>am_indx" + 1`
done<<>>dnl>>)
changequote([,]))])

AC_DEFUN(APRSD_CHECK_LIBPTHREAD,
[
AC_CHECK_LIB(pthread, pthread_create, [LIBPTHREAD="-lpthread"] )
AC_SUBST(LIBPTHREAD)
LIBS="$LIBS $LIBPTHREAD"
])

AC_DEFUN(APRSD_CHECK_PTHREAD_OPTION,
[
    AC_ARG_ENABLE(kernel-threads, [  --enable-kernel-threads Enable the use of the LinuxThreads port on FreeBSD/i386 only.],
	aprsd_use_kernthreads=$enableval, aprsd_use_kernthreads=no)

    if test "$aprsd_use_kernthreads" = "yes"; then
      ac_save_CXXFLAGS="$CXXFLAGS"
      ac_save_CFLAGS="$CXXFLAGS"
      CXXFLAGS="-I/usr/local/include/pthread/linuxthreads $CXXFLAGS"
      CFLAGS="-I/usr/local/include/pthread/linuxthreads $CFLAGS"
      AC_CHECK_HEADERS(pthread/linuxthreads/pthread.h)
      CXXFLAGS="$ac_save_CXXFLAGS"
      CFLAGS="$ac_save_CFLAGS"
      if test "$ac_cv_header_pthread_linuxthreads_pthread_h" = "no"; then
        aprsd_use_kernthreads=no
      else
        dnl Add proper -I and -l statements
        AC_CHECK_LIB(lthread, pthread_join, [LIBPTHREAD="-llthread -llgcc_r"]) dnl for FreeBSD
        if test "x$LIBPTHREAD" = "x"; then
          aprsd_use_kernthreads=no
        else
          USE_THREADS="-D_THREAD_SAFE -I/usr/local/include/pthread/linuxthreads"
        fi
      fi
    else
      USE_THREADS=""
      if test -z "$LIBPTHREAD"; then
        APRSD_CHECK_COMPILER_FLAG(pthread, [USE_THREADS="-pthread"] )
      fi
    fi

    case $host_os in
 	solaris*)
		APRSD_CHECK_COMPILER_FLAG(mt, [USE_THREADS="-mt"])
                CPPFLAGS="$CPPFLAGS -D_REENTRANT -D_POSIX_PTHREAD_SEMANTICS -DUSE_SOLARIS"
                echo "Setting Solaris pthread compilation options"
    		;;
        freebsd*)
                CPPFLAGS="$CPPFLAGS -D_THREAD_SAFE"
                echo "Setting FreeBSD pthread compilation options"
                ;;
        aix*)
                CPPFLAGS="$CPPFLAGS -D_THREAD_SAFE"
                LIBPTHREAD="$LIBPTHREAD -lc_r"
                echo "Setting AIX pthread compilation options"
                ;;
        linux*) CPPFLAGS="$CPPFLAGS -D_REENTRANT -D_PTHREADS"
                USE_THREADS="$USE_THREADS -DPIC -fPIC"
                echo "Setting Linux pthread compilation options"
                ;;
	*)
		;;
    esac
    AC_SUBST(USE_THREADS)
    AC_SUBST(LIBPTHREAD)
])

AC_DEFUN(APRSD_CHECK_THREADING,
[
  AC_REQUIRE([APRSD_CHECK_LIBPTHREAD])
  AC_REQUIRE([APRSD_CHECK_PTHREAD_OPTION])
  dnl default is yes if libpthread is found and no if no libpthread is available
  if test -z "$LIBPTHREAD"; then
    aprsd_check_threading_default=no
  else
    aprsd_check_threading_default=yes
  fi
  AC_ARG_ENABLE(threading, [  --disable-threading     disables threading even if libpthread found ],
   aprsd_use_threading=$enableval, aprsd_use_threading=$aprsd_check_threading_default)

  if test "x$aprsd_use_threading" = "xyes"; then
    AC_DEFINE(HAVE_LIBPTHREAD, 1, [Define if you have a working libpthread (will enable threaded code)])
  fi
])


AC_DEFUN(AC_CHECK_COMPILERS,
[
  dnl this is somehow a fat lie, but prevents other macros from double checking
  AC_PROVIDE([AC_PROG_CC])
  AC_PROVIDE([AC_PROG_CPP])
  AC_PROVIDE([AC_PROG_CXX])
  AC_PROVIDE([AC_PROG_CXXCPP])

  AC_ARG_ENABLE(debug,[  --enable-debug          creates debugging code [default=no]],
  [
   if test $enableval = "no"; dnl
     then
       aprsd_use_debug_code="no"
       aprsd_use_debug_define=yes
     else
       aprsd_use_debug_code="yes"
       aprsd_use_debug_define=no
   fi
  ], [aprsd_use_debug_code="no"
      aprsd_use_debug_define=no
    ])

  AC_ARG_ENABLE(strict,[  --enable-strict         compiles with strict compiler options (may not work!)],
   [
    if test $enableval = "no"; then
         aprsd_use_strict_options="no"
       else
         aprsd_use_strict_options="yes"
    fi
   ], [aprsd_use_strict_options="no"])

  AC_ARG_ENABLE(profile,[  --enable-profile        creates profiling infos [default=no]],
     [aprsd_use_profiling=$enableval],
     [aprsd_use_profiling="no"]
  )

dnl this was AC_PROG_CC. I had to include it manualy, since I had to patch it
  AC_MSG_CHECKING(for a C-Compiler)
  dnl if there is one, print out. if not, don't matter
  AC_MSG_RESULT($CC)

  if test -z "$CC"; then AC_CHECK_PROG(CC, gcc, gcc) fi
  if test -z "$CC"; then AC_CHECK_PROG(CC, cc, cc, , , /usr/ucb/cc) fi
  if test -z "$CC"; then AC_CHECK_PROG(CC, xlc, xlc) fi
  test -z "$CC" && AC_MSG_ERROR([no acceptable cc found in \$PATH])

  AC_PROG_CC_WORKS
  AC_PROG_CC_GNU

  if test $ac_cv_prog_gcc = yes; then
    GCC=yes
  else
    GCC=
  fi

  USER_CFLAGS=$CFLAGS
  CFLAGS=

  if test -z "$CFLAGS"; then
    if test "$aprsd_use_debug_code" = "yes"; then
      AC_PROG_CC_G
      if test $ac_cv_prog_cc_g = yes; then
	CFLAGS="-g"
	case $host in
   	*-*-linux-gnu)
           CFLAGS="$CFLAGS -ansi -W -Wall -pedantic -Wshadow -Wpointer-arith -Wmissing-prototypes -Wwrite-strings -D_XOPEN_SOURCE -D_BSD_SOURCE"
         ;;
        esac
      fi
    else
      if test "$GCC" = "yes"; then
        CFLAGS="-O2"
      else
        CFLAGS=""
      fi
      if test "$aprsd_use_debug_define" = "yes"; then
         CFLAGS="$CFLAGS -DNDEBUG"
      fi
    fi

    if test "$aprsd_use_profiling" = yes; then
      APRSD_PROG_CC_PG
      if test "$aprsd_cv_prog_cc_pg" = yes; then
        CFLAGS="$CFLAGS -pg"
      fi
    fi

    if test "$GCC" = "yes"; then
     CFLAGS="$CFLAGS"

     if test "$aprsd_use_strict_options" = "yes"; then
	CFLAGS="$CFLAGS -W -Wall -ansi -pedantic -Wshadow -Wpointer-arith -Wmissing-prototypes -Wwrite-strings"
     fi
    fi

  fi

  case "$host" in
  *-*-sysv4.2uw*) CFLAGS="$CFLAGS -D_UNIXWARE";;
  esac

  if test -n "$USER_CFLAGS"; then
    CFLAGS="$CFLAGS $USER_CFLAGS"
  fi

  if test -z "$LDFLAGS" && test "$aprsd_use_debug_code" = "no" && test "$GCC" = "yes"; then
     LDFLAGS=""
  fi


dnl this is AC_PROG_CPP. I had to include it here, since autoconf checks
dnl dependecies between AC_PROG_CPP and AC_PROG_CC (or is it automake?)

  AC_MSG_CHECKING(how to run the C preprocessor)
  # On Suns, sometimes $CPP names a directory.
  if test -n "$CPP" && test -d "$CPP"; then
    CPP=
  fi
  if test -z "$CPP"; then
  AC_CACHE_VAL(ac_cv_prog_CPP,
  [  # This must be in double quotes, not single quotes, because CPP may get
    # substituted into the Makefile and "${CC-cc}" will confuse make.
    CPP="${CC-cc} -E"
    # On the NeXT, cc -E runs the code through the compiler's parser,
    # not just through cpp.
    dnl Use a header file that comes with gcc, so configuring glibc
    dnl with a fresh cross-compiler works.
    AC_TRY_CPP([#include <assert.h>
    Syntax Error], ,
    CPP="${CC-cc} -E -traditional-cpp"
    AC_TRY_CPP([#include <assert.h>
    Syntax Error], , CPP=/lib/cpp))
    ac_cv_prog_CPP="$CPP"])dnl
    CPP="$ac_cv_prog_CPP"
  else
    ac_cv_prog_CPP="$CPP"
  fi
  AC_MSG_RESULT($CPP)
  AC_SUBST(CPP)dnl


  AC_MSG_CHECKING(for a C++-Compiler)
  dnl if there is one, print out. if not, don't matter
  AC_MSG_RESULT($CXX)

  if test -z "$CXX"; then AC_CHECK_PROG(CXX, g++, g++) fi
  if test -z "$CXX"; then AC_CHECK_PROG(CXX, CC, CC) fi
  if test -z "$CXX"; then AC_CHECK_PROG(CXX, xlC, xlC) fi
  if test -z "$CXX"; then AC_CHECK_PROG(CXX, DCC, DCC) fi
  test -z "$CXX" && AC_MSG_ERROR([no acceptable C++-compiler found in \$PATH])

  AC_PROG_CXX_WORKS
  AC_PROG_CXX_GNU

  if test $ac_cv_prog_gxx = yes; then
    GXX=yes
  fi

  USER_CXXFLAGS=$CXXFLAGS
  CXXFLAGS=""

  if test -z "$CXXFLAGS"; then
    if test "$aprsd_use_debug_code" = "yes"; then
      AC_PROG_CXX_G
      if test $ac_cv_prog_cxx_g = yes; then
        CXXFLAGS="-g"
	case $host in  dnl
   	*-*-linux-gnu)
           CXXFLAGS="$CXXFLAGS -ansi -D_XOPEN_SOURCE -D_BSD_SOURCE -Wbad-function-cast -Wcast-align -Wundef -Wconversion"
         ;;
        esac
      fi
    else
      if test "$GXX" = "yes"; then
         CXXFLAGS="-O2"
      fi
      if test "$aprsd_use_debug_define" = "yes"; then
         CXXFLAGS="$CXXFLAGS -DNDEBUG"
      fi
    fi

    if test "$aprsd_use_profiling" = yes; then
      APRSD_PROG_CXX_PG
      if test "$aprsd_cv_prog_cxx_pg" = yes; then
        CXXFLAGS="$CXXFLAGS -pg"
      fi
    fi

    APRSD_CHECK_COMPILER_FLAG(fexceptions,
    [
      CXXFLAGS="$CXXFLAGS -fexceptions"
    ])

dnl WABA: Nothing wrong with RTTI, keep it on.
dnl    APRSD_CHECK_COMPILER_FLAG(fno-rtti,
dnl	[
dnl	  CXXFLAGS="$CXXFLAGS -fno-rtti"
dnl	])

    APRSD_CHECK_COMPILER_FLAG(fno-check-new,
	[
	  CXXFLAGS="$CXXFLAGS -fno-check-new"
	])

    if test "$GXX" = "yes"; then
       CXXFLAGS="$CXXFLAGS"

       if test true || test "$aprsd_use_debug_code" = "yes"; then
	 CXXFLAGS="$CXXFLAGS -Wall -pedantic -W -Wpointer-arith -Wmissing-prototypes -Wwrite-strings"

         APRSD_CHECK_COMPILER_FLAG(Wno-long-long,
	 [
	   CXXFLAGS="$CXXFLAGS -Wno-long-long"
	 ])
         APRSD_CHECK_COMPILER_FLAG(fno-builtin,
         [
           CXXFLAGS="$CXXFLAGS -fno-builtin"
         ])

       fi

       if test "$aprsd_use_strict_options" = "yes"; then
	CXXFLAGS="$CXXFLAGS -Wcast-qual -Wbad-function-cast -Wshadow -Wcast-align"
       fi

       if test "$aprsd_very_strict" = "yes"; then
         CXXFLAGS="$CXXFLAGS -Wold-style-cast -Wredundant-decls -Wconversion"
       fi
    fi
  fi

    APRSD_CHECK_COMPILER_FLAG(fexceptions,
	[
	  USE_EXCEPTIONS="-fexceptions"
	],
	  USE_EXCEPTIONS=
	)
    AC_SUBST(USE_EXCEPTIONS)

    APRSD_CHECK_COMPILER_FLAG(frtti,
	[
	  USE_RTTI="-frtti"
	],
	  USE_RTTI=
	)
    AC_SUBST(USE_RTTI)

    case "$host" in
      *-*-irix*)  test "$GXX" = yes && CXXFLAGS="$CXXFLAGS -D_LANGUAGE_C_PLUS_PLUS -D__LANGUAGE_C_PLUS_PLUS" ;;
      *-*-sysv4.2uw*) CXXFLAGS="$CXXFLAGS -D_UNIXWARE";;
    esac

    if test -n "$USER_CXXFLAGS"; then
       CXXFLAGS="$CXXFLAGS $USER_CXXFLAGS"
    fi

    AC_VALIDIFY_CXXFLAGS

    AC_MSG_CHECKING(how to run the C++ preprocessor)
    if test -z "$CXXCPP"; then
      AC_CACHE_VAL(ac_cv_prog_CXXCPP,
      [
         AC_LANG_SAVE[]dnl
         AC_LANG_CPLUSPLUS[]dnl
         CXXCPP="${CXX-g++} -E"
         AC_TRY_CPP([#include <stdlib.h>], , CXXCPP=/lib/cpp)
         ac_cv_prog_CXXCPP="$CXXCPP"
         AC_LANG_RESTORE[]dnl
     ])dnl
     CXXCPP="$ac_cv_prog_CXXCPP"
    fi
    AC_MSG_RESULT($CXXCPP)
    AC_SUBST(CXXCPP)dnl

    # the following is to allow programs, that are known to
    # have problems when compiled with -O2
    if test -n "$CXXFLAGS"; then
      aprsd_safe_IFS=$IFS
      IFS=" "
      NOOPT_CXXFLAGS=""
      for i in $CXXFLAGS; do
        case $i in
          -O*)
                ;;
          *)
                NOOPT_CXXFLAGS="$NOOPT_CXXFLAGS $i"
                ;;
        esac
      done
      IFS=$aprsd_safe_IFS
    fi
    AC_SUBST(NOOPT_CXXFLAGS)

    APRSD_CXXFLAGS=
    AC_SUBST(APRSD_CXXFLAGS)
])


AC_DEFUN(APRSD_CHECK_COMPILER_FLAG,
[
AC_REQUIRE([AC_CHECK_COMPILERS])
AC_MSG_CHECKING(whether $CXX supports -$1)
aprsd_cache=`echo $1 | sed 'y%.=/+-%___p_%'`
AC_CACHE_VAL(aprsd_cv_prog_cxx_$aprsd_cache,
[
echo 'int main() { return 0; }' >conftest.cc
eval "aprsd_cv_prog_cxx_$aprsd_cache=no"
if test -z "`$CXX -$1 -c conftest.cc 2>&1`"; then
  if test -z "`$CXX -$1 -o conftest conftest.o 2>&1`"; then
    eval "aprsd_cv_prog_cxx_$aprsd_cache=yes"
  fi
fi
rm -f conftest*
])
if eval "test \"`echo '$aprsd_cv_prog_cxx_'$aprsd_cache`\" = yes"; then
 AC_MSG_RESULT(yes)
 :
 $2
else
 AC_MSG_RESULT(no)
 :
 $3
fi
])




dnl AC_VALIDIFY_CXXFLAGS checks for forbidden flags the user may have given
AC_DEFUN(AC_VALIDIFY_CXXFLAGS,
[dnl
 AC_REMOVE_FORBIDDEN(CXX, [-fno-rtti])
 AC_REMOVE_FORBIDDEN(CXXFLAGS, [-fno-rtti])
])

dnl use: AC_REMOVE_FORBIDDEN(CC, [-forbid -bad-option whatever])
dnl it's all white-space separated
AC_DEFUN(AC_REMOVE_FORBIDDEN,
[ __val=$$1
  __forbid=" $2 "
  if test -n "$__val"; then
    __new=""
    ac_save_IFS=$IFS
    IFS=" 	"
    for i in $__val; do
      case "$__forbid" in
        *" $i "*) AC_MSG_WARN([found forbidden $i in $1, removing it]) ;;
	*) # Careful to not add spaces, where there were none, because otherwise
	   # libtool gets confused, if we change e.g. CXX
	   if test -z "$__new" ; then __new=$i ; else __new="$__new $i" ; fi ;;
      esac
    done
    IFS=$ac_save_IFS
    $1=$__new
  fi
])



AC_DEFUN(APRSD_CHECK_NAMESPACES,
[
AC_MSG_CHECKING(whether C++ compiler supports namespaces)
AC_LANG_SAVE
AC_LANG_CPLUSPLUS
AC_TRY_COMPILE([
],
[
namespace Foo {
    extern int i;
    namespace Bar {
        extern int i;
    }
}

int Foo::i = 0;
int Foo::Bar::i = 1;
],[
  AC_MSG_RESULT(yes)
  AC_DEFINE(HAVE_NAMESPACES)
], [
AC_MSG_RESULT(no)
])
AC_LANG_RESTORE
])

dnl ----------------------------------------------------------------------
dnl
AC_DEFUN(CHECK_AX25,
[
  AC_ARG_WITH(ax25,
  [ --with-ax25=DIR directory path of ax25 installation [checks
      /usr/include and /usr/local/include]
  --without-ax25 to disable ax25 support completely],
  [if test "$withval" != no ; then
    AC_MSG_CHECKING([for AX25])
  else
    AC_MSG_RESULT([No AX25 support requested])
  fi], [

  LIBS_AX25=""

  ac_new_ax25_incdirs="/usr/include/netax25 /usr/local/include/netax25"
  ac_old_ax25_incdirs="/usr/include/ax25 /usr/local/include/ax25"
  ac_ax25_libdirs="/usr/lib /usr/local/lib"

  AC_FIND_FILE(axlib.h, $ac_new_ax25_incdirs, ac_new_ax25_incdir)
  AC_FIND_FILE(libax25.so, $ac_ax25_libdirs, ac_ax25_libdir)
  if test ! "$ac_new_ax25_incdir" = "NO"; then
    #CFLAGS="$CFLAGS $ac_new_ax25_incdir"
    AC_DEFINE_UNQUOTED(WITH_AX25, 1, [Define if you have AX25])
    AC_DEFINE_UNQUOTED(WITH_NEW_AX25, 1, [Define if you have "new" AX25])
    AC_CHECK_LIB(ax25, ax25_aton_entry, LIBS_AX25="-lax25")
    AC_SUBST(LIBS_AX25)
    AC_MSG_RESULT(["New" AX25 found: libraries: $ac_ax25_libdir, headers: $ac_new_ax25_incdir])
  fi

  AC_FIND_FILE(axconfig.h, $ac_old_ax25_incdirs, ac_old_ax25_incdir)
  if test ! "$ac_old_ax25_incdir" = "NO"; then
    #CFLAGS="$CFLAGS $ac_old_ax25_incdir"
    AC_DEFINE_UNQUOTED(WITH_AX25, 1, [Define if you have AX25])
    AC_DEFINE_UNQUOTED(WITH_OLD_AX25, 1, [Define if you have AX25])
    AC_CHECK_LIB(ax25, concert_call_entry, LIBS_AX25="-lax25")
    AC_SUBST(LIBS_AX25)
    AC_MSG_RESULT(["OLD" AX25 found: libraries: , headers: $ac_old_ax25_incdir])
  fi
  LIBS="$LIBS $LIBS_AX25"
  ])
])
