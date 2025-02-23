dnl config.m4 for extension necrofile

PHP_ARG_ENABLE(necrofile, whether to enable necrofile support,
[  --enable-necrofile      Enable necrofile support])

PHP_ARG_WITH(hiredis, for hiredis support,
[  --with-hiredis[=DIR]    Include hiredis support. DIR is the hiredis install prefix], yes)

if test "$PHP_NECROFILE" != "no"; then
  dnl Check for hiredis
  if test "$PHP_HIREDIS" != "no"; then
    if test -r "$PHP_HIREDIS/include/hiredis/hiredis.h"; then
      HIREDIS_DIR="$PHP_HIREDIS"
    else
      AC_MSG_CHECKING(for hiredis in default path)
      for i in /usr/local /usr; do
        if test -r "$i/include/hiredis/hiredis.h"; then
          HIREDIS_DIR="$i"
          AC_MSG_RESULT(found in $i)
          break
        fi
      done
    fi

    if test -z "$HIREDIS_DIR"; then
      AC_MSG_RESULT(not found)
      AC_MSG_ERROR(Please install hiredis)
    fi

    PHP_ADD_INCLUDE($HIREDIS_DIR/include)
    PHP_ADD_LIBRARY_WITH_PATH(hiredis, $HIREDIS_DIR/$PHP_LIBDIR, NECROFILE_SHARED_LIBADD)
    PHP_SUBST(NECROFILE_SHARED_LIBADD)
  fi

  dnl Check for required headers
  AC_CHECK_HEADERS([pthread.h sys/epoll.h arpa/inet.h fcntl.h])

  dnl Check for required libraries
  PHP_CHECK_LIBRARY(pthread, pthread_create,
  [
    PHP_ADD_LIBRARY(pthread,, NECROFILE_SHARED_LIBADD)
  ],[
    AC_MSG_ERROR(pthread library not found)
  ])

  PHP_NEW_EXTENSION(necrofile,
    necrofile.c \
    necrofile_redis.c \
    necrofile_tcp.c \
    necrofile_utils.c,
    $ext_shared)
fi