
AC_DEFUN(APRSD_CHECK_TYPE, [
AC_CACHE_CHECK(for $1, uck_cv_type_$1, [
AC_EGREP_HEADER([(^|[^a-zA-Z_0-9])$1[^a-zA-Z_0-9]], $3,
		uck_cv_type_$1=yes, uck_cv_type_$1=no)
])
if test $uck_cv_type_$1 = no; then
AC_DEFINE($1, $2, Define to \`$2' if $3 doesn't define.)
fi
])


AC_DEFUN(APRSD_CHECK_FHS,
[
  AC_ARG_ENABLE(fhs,
     [  --enable-fhs            use FHS-compliant paths for data files
  --disable-fhs           use standard APRSD paths],
     [
       AC_DEFINE_UNQUOTED(USE_FHS, 1, [Define to use FHS-compliant file locations])
       AC_MSG_RESULT(["Using FHS-compliant file locations"])
     ],
     [
       AC_MSG_RESULT(["Using standard APRSD file locations"])
   ])
])

