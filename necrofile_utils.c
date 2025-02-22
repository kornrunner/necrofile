#include "SAPI.h"

zend_bool is_cli_sapi(void)
{
    static int is_cli = -1;
    if (UNEXPECTED(is_cli == -1)) {
        is_cli = (strcmp(sapi_module.name, "cli") == 0 ||
                 strcmp(sapi_module.name, "phpdbg") == 0);
    }
    return is_cli;
}