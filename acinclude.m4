AC_DEFUN(APRSD_CHECK_TYPE, [
AC_CACHE_CHECK(for $1, uck_cv_type_$1, [
AC_EGREP_HEADER([(^|[^a-zA-Z_0-9])$1[^a-zA-Z_0-9]], $3,
		uck_cv_type_$1=yes, uck_cv_type_$1=no)
])
if test $uck_cv_type_$1 = no; then
AC_DEFINE($1, $2, Define to \`$2' if $3 doesn't define.)
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