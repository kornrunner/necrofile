#ifndef PHP_NECROFILE_H
#define PHP_NECROFILE_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "ext/standard/info.h"
#include <pthread.h>
#include <stdatomic.h>

#define PHP_NECROFILE_VERSION "1.0.0"

#if defined(__GNUC__) || defined(__clang__)
#define ATOMIC_INCREMENT(ptr) __atomic_add_fetch(ptr, 1, __ATOMIC_SEQ_CST)
#define ATOMIC_LOAD(ptr) __atomic_load_n(ptr, __ATOMIC_SEQ_CST)
#define ATOMIC_STORE(ptr, val) __atomic_store_n(ptr, val, __ATOMIC_SEQ_CST)
#else
#error "Atomic operations are not supported on this platform"
#endif

extern zend_module_entry necrofile_module_entry;
#define phpext_necrofile_ptr &necrofile_module_entry

ZEND_BEGIN_MODULE_GLOBALS(necrofile)
    char *redis_host;
    zend_long redis_port;
    char *server_address;
    zend_long tcp_port;
    char *exclude_patterns;
    char *trim_patterns;
ZEND_END_MODULE_GLOBALS(necrofile)

#ifdef ZTS
#define NECROFILE_G(v) TSRMG(necrofile_globals_id, zend_necrofile_globals *, v)
extern int necrofile_globals_id;
#else
#define NECROFILE_G(v) (necrofile_globals.v)
extern ZEND_DECLARE_MODULE_GLOBALS(necrofile)
#endif

// Function declarations
PHP_MINIT_FUNCTION(necrofile);
PHP_MSHUTDOWN_FUNCTION(necrofile);
PHP_RINIT_FUNCTION(necrofile);
PHP_RSHUTDOWN_FUNCTION(necrofile);
PHP_MINFO_FUNCTION(necrofile);
PHP_FUNCTION(necrofiles);

#endif /* PHP_NECROFILE_H */