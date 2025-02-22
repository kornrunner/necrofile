#ifndef NECROFILE_SHM_H
#define NECROFILE_SHM_H

#include "php_necrofile.h"

typedef struct {
    atomic_int initialized;
    pthread_mutex_t shm_mutex;
    pthread_rwlock_t shm_rwlock;
    size_t count;
    size_t data_offset;
    size_t paths_length[MAX_PATHS];
    size_t paths_offset[MAX_PATHS];
} shm_header;

int init_shared_memory(void);
void necrofile_included_file(char *filename);
zend_bool path_exists(const char *path);
char *trim_path(const char *path);

extern char *shm_ptr;
extern pthread_rwlock_t shm_rwlock;
extern pthread_mutex_t necrofile_mutex;

#endif /* NECROFILE_SHM_H */