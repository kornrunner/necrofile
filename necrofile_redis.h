#ifndef NECROFILE_REDIS_H
#define NECROFILE_REDIS_H

#include <hiredis/hiredis.h>

#define REDIS_SET_KEY "necrofile:included_files"

extern redisContext *redis_context;
redisContext* connect_to_redis(void);

#endif /* NECROFILE_REDIS_H */