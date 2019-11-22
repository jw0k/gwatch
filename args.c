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
    static bool timeout_set = false;
    static bool repo_set = false;

    if (!repo_set && strcmp(argv[offset], "-r") == 0)
    {
        repo_path = argv[offset+1];
        repo_set = true;
        return true;
    }
    else if (!timeout_set && strcmp(argv[offset], "-t") == 0)
    {
        long int t = strtol(argv[offset+1], NULL, 10);
        if (t >= 1 && t <= 100000)
        {
            timeout = (int)t;
            timeout_set = true;
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

bool parse_args_impl(int argc, char* argv[])
{
    prog_name = argv[0];
    char* last_slash = NULL;

#ifdef WIN32
    int delimiter = '\\';
#else
    int delimiter = '/';
#endif

    last_slash = strrchr(prog_name, delimiter);

    if (last_slash)
        prog_name = last_slash + 1;

    if (argc == 2 || argc == 4 || argc > 5)
        return false;

    if (argc == 3)
        return parse_pair(argv, 1);

    if (argc == 5)
    {
        if (!parse_pair(argv, 1))
        {
            return false;
        }

        return parse_pair(argv, 3);
    }

    return true;
}

bool parse_args(int argc, char* argv[])
{
    if (!parse_args_impl(argc, argv))
    {
        print_usage();
        return false;
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
