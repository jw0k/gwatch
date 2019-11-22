#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>

#include <git2.h>

#include "args.h"
#include "fs_listener.h"
#include "git.h"

int main(int argc, char* argv[])
{
    uv_loop_t loop;

    if (!parse_args(argc, argv))
        return -1;

    printf("Starting gwatch\n");
    printf("Watched repository: %s\n", get_repo_path());
    printf("Timeout: %ds\n", get_timeout());

    git_libgit2_init();

    if (check_if_valid_git_repo())
        commit();

    uv_loop_init(&loop);

    fs_listener_start(&loop, commit);

    uv_run(&loop, UV_RUN_DEFAULT);

    uv_loop_close(&loop);
    git_libgit2_shutdown();

    return 0;
}
