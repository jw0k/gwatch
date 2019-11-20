#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>
#include <string.h>

#include <git2.h>
#include <uv.h>

#include "logs.h"
#include "args.h"
#include "fs_listener.h"
#include "git.h"

/* TODO:
 * - possibly make commit upon start
 */

uv_loop_t loop;

void commit_and_restart_listener()
{
    commit();
    fs_listener_start(&loop, commit_and_restart_listener);
}

int main(int argc, char* argv[])
{
    if (!parse_args(argc, argv))
        return -1;

    printf("Starting gwatch\n");
    printf("Watched repository: %s\n", get_repo_path());
    printf("Timeout: %ds\n", get_timeout());

    git_libgit2_init();

    warn_if_non_git_repo();

    uv_loop_init(&loop);

    fs_listener_start(&loop, commit_and_restart_listener);

    uv_run(&loop, UV_RUN_DEFAULT);

    uv_loop_close(&loop);
    git_libgit2_shutdown();

    return 0;
}
