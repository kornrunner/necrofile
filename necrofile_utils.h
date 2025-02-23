#ifndef NECROFILE_UTILS_H
#define NECROFILE_UTILS_H

#include "SAPI.h"

zend_bool is_cli_sapi(void);
char *trim_path(const char *path);
void exclude_patterns(const char *path);

#endif /* NECROFILE_UTILS_H */