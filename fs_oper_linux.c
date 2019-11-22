#include "fs_oper.h"

#include <dirent.h>
#include <stdbool.h>
#include <stdlib.h>

bool dir_exists(const char* path)
{
    DIR* dir = NULL;
    dir = opendir(path);
    bool result = (dir != NULL);
    if (dir)
    {
        closedir(dir);
    }
    return result;
}
