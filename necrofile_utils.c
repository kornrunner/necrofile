#include "SAPI.h"
#include "php_necrofile.h"

zend_bool is_cli_sapi(void)
{
    static int is_cli = -1;
    if (UNEXPECTED(is_cli == -1)) {
        is_cli = (strcmp(sapi_module.name, "cli") == 0 ||
                 strcmp(sapi_module.name, "phpdbg") == 0);
    }
    return is_cli;
}

void exclude_patterns(const char *path)
{
    if (!path || !*path) {
        return;
    }

    if (NECROFILE_G(exclude_patterns) && *NECROFILE_G(exclude_patterns)) {
        char exclude_patterns_buf[1024];
        char *exclude_patterns = NULL;
        char *saveptr;

        size_t pattern_len = strlen(NECROFILE_G(exclude_patterns));
        if (pattern_len < sizeof(exclude_patterns_buf)) {
            exclude_patterns = exclude_patterns_buf;
            memcpy(exclude_patterns, NECROFILE_G(exclude_patterns), pattern_len + 1);
        } else {
            exclude_patterns = estrdup(NECROFILE_G(exclude_patterns));
        }

        char *pattern = strtok_r(exclude_patterns, ",", &saveptr);
        size_t path_len = strlen(path);

        while (pattern) {
            char *match = strstr(path, pattern);
            if (match) {
                if (exclude_patterns != exclude_patterns_buf) {
                    efree(exclude_patterns);
                }
                return;
            }
            pattern = strtok_r(NULL, ",", &saveptr);
        }

        if (exclude_patterns != exclude_patterns_buf) {
            efree(exclude_patterns);
        }
    }

    return;
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
        memcpy(trim_patterns, NECROFILE_G(trim_patterns), pattern_len + 1);
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
            strcpy(result, match + offset);
            break;
        }
        pattern = strtok_r(NULL, ",", &saveptr);
    }

    if (trim_patterns != trim_patterns_buf) {
        efree(trim_patterns);
    }

    return result ? result : estrdup(path);
}