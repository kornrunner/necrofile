#include "php_necrofile.h"
#include "necrofile_shm.h"
#include "necrofile_tcp.h"
#include "necrofile_utils.h"
#include "zend_smart_str.h"
#include <sys/mman.h>

ZEND_DECLARE_MODULE_GLOBALS(necrofile)

static zend_op_array *(*original_zend_compile_file)(zend_file_handle *file_handle, int type);

PHP_INI_BEGIN()
    STD_PHP_INI_ENTRY("necrofile.shm_size", "67108864", PHP_INI_SYSTEM, OnUpdateLong, shm_size, zend_necrofile_globals, necrofile_globals)
    STD_PHP_INI_ENTRY("necrofile.tcp_port", "8282", PHP_INI_SYSTEM, OnUpdateLong, tcp_port, zend_necrofile_globals, necrofile_globals)
    STD_PHP_INI_ENTRY("necrofile.server_address", "127.0.0.1", PHP_INI_SYSTEM, OnUpdateString, server_address, zend_necrofile_globals, necrofile_globals)
    STD_PHP_INI_ENTRY("necrofile.exclude_patterns", "", PHP_INI_SYSTEM, OnUpdateString, exclude_patterns, zend_necrofile_globals, necrofile_globals)
    STD_PHP_INI_ENTRY("necrofile.trim_patterns", "", PHP_INI_SYSTEM, OnUpdateString, trim_patterns, zend_necrofile_globals, necrofile_globals)
PHP_INI_END()

ZEND_BEGIN_ARG_INFO_EX(arginfo_necrofiles, 0, 0, 0)
ZEND_END_ARG_INFO()

static void php_necrofile_init_globals(zend_necrofile_globals *ng)
{
    ng->shm_size = 67108864;
    ng->tcp_port = 8282;
    ng->server_address = "127.0.0.1";
    ng->exclude_patterns = "";
    ng->trim_patterns = "";
}

static zend_op_array *necrofile_compile_file(zend_file_handle *file_handle, int type)
{
    if (!is_cli_sapi() && file_handle->filename) {
        necrofile_included_file(ZSTR_VAL(file_handle->filename));
    }
    return original_zend_compile_file(file_handle, type);
}

PHP_FUNCTION(necrofiles)
{
    if (is_cli_sapi()) {
        RETURN_STRING("[]");
    }

    smart_str json = {0};
    pthread_mutex_lock(&necrofile_mutex);

    pthread_rwlock_rdlock(&shm_rwlock);
    if (shm_ptr) {
        shm_header *header = (shm_header *)shm_ptr;
        pthread_mutex_lock(&header->shm_mutex);
        pthread_rwlock_rdlock(&header->shm_rwlock);

        smart_str_appendc(&json, '[');

        for (size_t i = 0; i < header->count; i++) {
            if (i > 0) {
                smart_str_appendl(&json, ",\n", 2);
            }

            smart_str_appendc(&json, '"');
            smart_str_appendl(&json, shm_ptr + header->paths_offset[i], header->paths_length[i]);
            smart_str_appendc(&json, '"');
        }

        pthread_rwlock_unlock(&header->shm_rwlock);
        pthread_mutex_unlock(&header->shm_mutex);
        smart_str_appendc(&json, ']');
    } else {
        smart_str_appendl(&json, "[]", 2);
    }
    pthread_rwlock_unlock(&shm_rwlock);

    smart_str_0(&json);
    pthread_mutex_unlock(&necrofile_mutex);

    RETURN_STR(json.s);
}

PHP_MINIT_FUNCTION(necrofile)
{
    ZEND_INIT_MODULE_GLOBALS(necrofile, php_necrofile_init_globals, NULL);
    REGISTER_INI_ENTRIES();

    if (!is_cli_sapi()) {
        ATOMIC_STORE(&tcp_server_running, 1);
        shm_unlink(SHM_NAME);

        if (init_shared_memory() == FAILURE) {
            return FAILURE;
        }
        original_zend_compile_file = zend_compile_file;
        zend_compile_file = necrofile_compile_file;

        if (pthread_create(&tcp_server_thread, NULL, tcp_server, NULL) != 0) {
            php_error_docref(NULL, E_WARNING, "Failed to start TCP server thread");
            ATOMIC_STORE(&tcp_server_running, 0);
            pthread_join(tcp_server_thread, NULL);
            return FAILURE;
        }
    }
    return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(necrofile)
{
    UNREGISTER_INI_ENTRIES();
    if (!is_cli_sapi()) {
        ATOMIC_STORE(&tcp_server_running, 0);
        pthread_join(tcp_server_thread, NULL);

        pthread_rwlock_destroy(&shm_rwlock);
        pthread_mutex_destroy(&necrofile_mutex);
        if (shm_ptr) {
            shm_header *header = (shm_header *)shm_ptr;
            pthread_mutex_destroy(&header->shm_mutex);
            pthread_rwlock_destroy(&header->shm_rwlock);
            munmap(shm_ptr, NECROFILE_G(shm_size));
            shm_ptr = NULL;
            shm_unlink(SHM_NAME);
        }

        if (tcp_server_fd != -1) {
            close(tcp_server_fd);
            tcp_server_fd = -1;
        }
    }
    return SUCCESS;
}

PHP_RINIT_FUNCTION(necrofile)
{
    return SUCCESS;
}

PHP_RSHUTDOWN_FUNCTION(necrofile)
{
    return SUCCESS;
}

PHP_MINFO_FUNCTION(necrofile)
{
    char buf[32];
    php_info_print_table_start();
    php_info_print_table_header(2, "necrofile support", "enabled");
    snprintf(buf, sizeof(buf), "%ld bytes", NECROFILE_G(shm_size));
    php_info_print_table_row(2, "Shared Memory Size", buf);
    snprintf(buf, sizeof(buf), "%ld", NECROFILE_G(tcp_port));
    php_info_print_table_row(2, "TCP Server Port", buf);
    php_info_print_table_row(2, "Exclude Patterns", NECROFILE_G(exclude_patterns));
    php_info_print_table_row(2, "Trim Patterns", NECROFILE_G(trim_patterns));
    php_info_print_table_end();
    DISPLAY_INI_ENTRIES();
}

static const zend_function_entry necrofile_functions[] = {
    PHP_FE(necrofiles, arginfo_necrofiles)
    PHP_FE_END
};

zend_module_entry necrofile_module_entry = {
    STANDARD_MODULE_HEADER,
    "necrofile",
    necrofile_functions,
    PHP_MINIT(necrofile),
    PHP_MSHUTDOWN(necrofile),
    PHP_RINIT(necrofile),
    PHP_RSHUTDOWN(necrofile),
    PHP_MINFO(necrofile),
    PHP_NECROFILE_VERSION,
    STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_NECROFILE
ZEND_GET_MODULE(necrofile)
#endif