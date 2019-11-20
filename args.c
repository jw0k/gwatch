#include "args.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char* prog_name = NULL;
const char* repo_path = ".";
int timeout = 30; // s

void print_usage()
{
    printf("Usage: %s [-r path/to/git/repo] [-t timeout_in_s]\n", prog_name);
}

bool parse_pair(char* argv[], int offset)
{
    if (strcmp(argv[offset], "-r") == 0)
    {
        repo_path = argv[offset+1];
        return true;
    }
    else if (strcmp(argv[offset], "-t") == 0)
    {
        long int t = strtol(argv[offset+1], NULL, 10);
        if (t >= 1 && t <= 100000)
        {
            timeout = (int)t;
        }
        else
        {
            printf("Timeout value must be between 1s and 100000s\n");
            return false;
        }
        return true;
    }

    return false;
}

bool parse_args(int argc, char* argv[])
{
    prog_name = argv[0];
    char* last_slash = NULL;
#ifdef WIN32
    last_slash = strrchr(prog_name, '\\');
#else
    last_slash = strrchr(prog_name, '/');
#endif
    if (last_slash)
    {
        prog_name = last_slash + 1;
    }

    if (argc == 2 || argc == 4 || argc > 5)
    {
        print_usage();
        return false;
    }
    if (argc == 3)
    {
        return parse_pair(argv, 1);
    }
    if (argc == 5)
    {
        if (!parse_pair(argv, 1))
        {
            print_usage();
            return false;
        }
        if (!parse_pair(argv, 3))
        {
            print_usage();
            return false;
        }
    }

    return true;
}

const char* get_prog_name()
{
    return prog_name;
}

const char* get_repo_path()
{
    return repo_path;
}

int get_timeout()
{
    return timeout;
}
