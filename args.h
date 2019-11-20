#pragma once

#include <stdbool.h>

bool parse_args(int argc, char* argv[]);
const char* get_prog_name();
const char* get_repo_path();
int get_timeout();
