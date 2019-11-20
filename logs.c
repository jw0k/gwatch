#include "logs.h"

#include <stdarg.h>
#include <stdio.h>
#include <time.h>

void plog(const char* str)
{
    time_t rawtime;
    struct tm* timeinfo;

    time(&rawtime);
    timeinfo = localtime(&rawtime);

    char buf[50];
    strftime(buf, sizeof(buf), "%Y-%m-%d %X", timeinfo);
    printf("%s: %s\n", buf, str);
}

void pflog(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    char buf[1024];
    vsnprintf(buf, sizeof(buf), fmt, args);
    plog(buf);

    va_end(args);
}
