#include "necrofile_shm.h"
#include "necrofile_utils.h"
#include "php_necrofile.h"
#include <sys/mman.h>
#include <fcntl.h>

char *shm_ptr = NULL;
pthread_rwlock_t shm_rwlock = PTHREAD_RWLOCK_INITIALIZER;
pthread_mutex_t necrofile_mutex = PTHREAD_MUTEX_INITIALIZER;

int init_shared_memory(void)
{
    if (UNEXPECTED(is_cli_sapi())) {
        return SUCCESS;
    }

    zend_long size = NECROFILE_G(shm_size);
    if (UNEXPECTED(size < MIN_SHM_SIZE || size > SIZE_MAX)) {
        php_error_docref(NULL, E_WARNING, "necrofile.shm_size must be at least 4MB and less than %zu", SIZE_MAX);
        return FAILURE;
    }

    if (shm_unlink(SHM_NAME) == -1 && errno != ENOENT) {
        php_error_docref(NULL, E_WARNING, "Failed to unlink shared memory: %s", strerror(errno));
        return FAILURE;
    }

    int shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0600);
    if (UNEXPECTED(shm_fd == -1)) {
        php_error_docref(NULL, E_WARNING, "Failed to create shared memory: %s", strerror(errno));
        return FAILURE;
    }

    if (ftruncate(shm_fd, size) == -1) {
        php_error_docref(NULL, E_WARNING, "Failed to resize shared memory: %s", strerror(errno));
        close(shm_fd);
        return FAILURE;
    }

    shm_ptr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_HUGETLB, shm_fd, 0);
    if (shm_ptr == MAP_FAILED) {
        shm_ptr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
        if (shm_ptr == MAP_FAILED) {
            php_error_docref(NULL, E_WARNING, "Failed to map shared memory: %s", strerror(errno));
            close(shm_fd);
            return FAILURE;
        }
    }

    shm_header *header = (shm_header *)shm_ptr;

    int expected = 0;
    if (atomic_compare_exchange_strong(&header->initialized, &expected, 1)) {
        pthread_mutexattr_t mutex_attr;
        pthread_mutexattr_init(&mutex_attr);
        pthread_mutexattr_setpshared(&mutex_attr, PTHREAD_PROCESS_SHARED);
        if (pthread_mutex_init(&header->shm_mutex, &mutex_attr) != 0) {
            php_error_docref(NULL, E_WARNING, "Failed to initialize shared mutex");
            return FAILURE;
        }
        pthread_mutexattr_destroy(&mutex_attr);

        pthread_rwlockattr_t rwlock_attr;
        pthread_rwlockattr_init(&rwlock_attr);
        pthread_rwlockattr_setpshared(&rwlock_attr, PTHREAD_PROCESS_SHARED);
        if (pthread_rwlock_init(&header->shm_rwlock, &rwlock_attr) != 0) {
            php_error_docref(NULL, E_WARNING, "Failed to initialize shared rwlock");
            return FAILURE;
        }
        pthread_rwlockattr_destroy(&rwlock_attr);
    }

    ATOMIC_STORE(&header->count, 0);
    ATOMIC_STORE(&header->data_offset, sizeof(shm_header));

    volatile char *touch = shm_ptr;
    for (size_t i = 0; i < size; i += 4096) {
        touch[i] = 0;
    }

    close(shm_fd);
    return SUCCESS;
}

void necrofile_included_file(char *filename)
{
    if (is_cli_sapi()) {
        return;
    }

    char *real_path = realpath(filename, NULL);
    if (!real_path) {
        return;
    }

    char *trimmed_path = trim_path(real_path);
    if (!trimmed_path || !*trimmed_path) {
        free(real_path);
        if (trimmed_path) efree(trimmed_path);
        return;
    }

    shm_header *header = (shm_header *)shm_ptr;
    pthread_mutex_lock(&header->shm_mutex);
    if (shm_ptr && !path_exists(trimmed_path)) {
        size_t path_len = strlen(trimmed_path);
        size_t required_space = path_len + 1;

        if (required_space > NECROFILE_G(shm_size) - sizeof(shm_header)) {
            pthread_mutex_unlock(&header->shm_mutex);
            free(real_path);
            efree(trimmed_path);
            return;
        }

        pthread_rwlock_wrlock(&header->shm_rwlock);

        if (header->count < MAX_PATHS &&
            (header->data_offset + required_space) < NECROFILE_G(shm_size)) {
            char *dest = shm_ptr + header->data_offset;
            size_t copied_len = strlcpy(dest, trimmed_path, NECROFILE_G(shm_size) - header->data_offset);

            if (copied_len < (NECROFILE_G(shm_size) - header->data_offset) && *dest) {
                header->paths_offset[header->count] = header->data_offset;
                header->paths_length[header->count] = path_len;
                header->data_offset += required_space;
                header->count++;
            }
        }

        pthread_rwlock_unlock(&header->shm_rwlock);
    }
    pthread_mutex_unlock(&header->shm_mutex);

    free(real_path);
    efree(trimmed_path);
}

zend_bool path_exists(const char *path)
{
    if (!path || !*path) {
        return 1;
    }

    if (NECROFILE_G(exclude_patterns) && *NECROFILE_G(exclude_patterns)) {
        char exclude_patterns_buf[1024];
        char *exclude_patterns = NULL;
        char *saveptr;

        size_t pattern_len = strlen(NECROFILE_G(exclude_patterns));
        if (pattern_len < sizeof(exclude_patterns_buf)) {
            exclude_patterns = exclude_patterns_buf;
            strlcpy(exclude_patterns, NECROFILE_G(exclude_patterns), sizeof(exclude_patterns_buf));
        } else {
            exclude_patterns = estrdup(NECROFILE_G(exclude_patterns));
        }

        char *pattern = strtok_r(exclude_patterns, ",", &saveptr);

        while (pattern) {
            char *match = strstr(path, pattern);
            if (match) {
                if (exclude_patterns != exclude_patterns_buf) {
                    efree(exclude_patterns);
                }
                return 1;
            }
            pattern = strtok_r(NULL, ",", &saveptr);
        }

        if (exclude_patterns != exclude_patterns_buf) {
            efree(exclude_patterns);
        }
    }

    shm_header *header = (shm_header *)shm_ptr;
    pthread_rwlock_rdlock(&header->shm_rwlock);

    zend_bool exists = 0;
    size_t count = ATOMIC_LOAD(&header->count);
    for (size_t i = 0; i < count; i++) {
        const char *stored_path = shm_ptr + header->paths_offset[i];
        if (stored_path && strcmp(stored_path, path) == 0) {
            exists = 1;
            break;
        }
    }

    pthread_rwlock_unlock(&header->shm_rwlock);
    return exists;
}

char *trim_path(const char *path)
{
    if (!path || !NECROFILE_G(trim_patterns) || !*NECROFILE_G(trim_patterns)) {
        return path ? estrdup(path) : NULL;
    }

    char trim_patterns_buf[1024];
    char *trim_patterns = NULL;
    char *result = NULL;
    char *saveptr;

    size_t pattern_len = strlen(NECROFILE_G(trim_patterns));
    if (pattern_len < sizeof(trim_patterns_buf)) {
        trim_patterns = trim_patterns_buf;
        strlcpy(trim_patterns, NECROFILE_G(trim_patterns), sizeof(trim_patterns_buf));
    } else {
        trim_patterns = estrdup(NECROFILE_G(trim_patterns));
    }

    char *pattern = strtok_r(trim_patterns, ",", &saveptr);
    size_t path_len = strlen(path);

    while (pattern) {
        char *match = strstr(path, pattern);
        if (match) {
            size_t offset = strlen(pattern);
            result = emalloc(path_len - offset + 1);
            strlcpy(result, match + offset, path_len - offset + 1);
            break;
        }
        pattern = strtok_r(NULL, ",", &saveptr);
    }

    if (trim_patterns != trim_patterns_buf) {
        efree(trim_patterns);
    }

    return result ? result : estrdup(path);
}