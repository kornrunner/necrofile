PHP_ARG_ENABLE(necrofile, whether to enable necrofile extension,
[  --enable-necrofile       Enable necrofile extension], yes)

if test "$PHP_NECROFILE" = "yes"; then
    PHP_NEW_EXTENSION(necrofile,
        necrofile.c \
        necrofile_shm.c \
        necrofile_tcp.c \
        necrofile_utils.c,
        $ext_shared)
fi