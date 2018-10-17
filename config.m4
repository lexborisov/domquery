PHP_ARG_ENABLE(modest,
    [Whether to enable the "modest" extension],
    [  --enable-modest         Enable "modest" extension support])

if test $PHP_MODEST != "no"; then
    AC_PROG_CC
    AC_PROG_CPP
    PHP_SUBST(MODEST_SHARED_LIBADD)
    PHP_ADD_LIBRARY(modest_static, 1, MODEST_SHARED_LIBADD)
    PHP_NEW_EXTENSION(modest, php_modest.c, $ext_shared)
fi
