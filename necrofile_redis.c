#include <php_necrofile.h>
#include <hiredis/hiredis.h>

redisContext *redis_context = NULL;

redisContext* connect_to_redis(void)
{
    struct timeval timeout = {1, 500000}; // 1.5 seconds
    redisContext *context = redisConnectWithTimeout(
        NECROFILE_G(redis_host),
        NECROFILE_G(redis_port),
        timeout
    );

    if (context == NULL) {
        php_error_docref(NULL, E_WARNING, "Redis connection error: can't allocate redis context");
        return NULL;
    }

    if (context->err) {
        php_error_docref(NULL, E_WARNING, "Redis connection error: %s", context->errstr);
        redisFree(context);
        return NULL;
    }

    return context;
}
