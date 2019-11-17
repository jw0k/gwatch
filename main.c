#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>
#include <string.h>

#include <git2.h>
#include <git2/errors.h>
#include <git2/index.h>
#include <git2/types.h>
#include <uv.h>

/*
 * TODO:
 * - check if repo is valid upon start
 * - parse command-line args (timer, repo path)
 * - handle all possible errors from libgit2
 * - handle "touch file" properly (do not create empty commits)
 */

uv_loop_t loop;
uv_timer_t low_pass_timer;
uv_fs_event_t fs_event_req;

void lp_cb(uv_timer_t* handle);
void fs_cb(uv_fs_event_t* handle, const char* filename, int events, int status);

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

void init_io_devices()
{
    uv_loop_init(&loop);
    uv_fs_event_init(&loop, &fs_event_req);
    uv_timer_init(&loop, &low_pass_timer);
}

void start_fs_listener()
{
    //TODO: make path settable via a command-line arg
    uv_fs_event_start(&fs_event_req, fs_cb, ".", UV_FS_EVENT_RECURSIVE);
}

void stop_fs_listener(uv_fs_event_t* handle)
{
    uv_fs_event_stop(handle);
}

void start_lp_timer()
{
    //TODO: make timeout settable via a command-line arg
    uv_timer_start(&low_pass_timer, lp_cb, 3000, 0);
}

bool check_error(int error)
{
    if (error < 0)
    {
        const git_error* ge = giterr_last();
        pflog("Git related error, %s", ge->message);
        return true;
    }
    return false;
}

int status_cb(const char* path, unsigned int status_flags, void* payload)
{
    (void)path;
    (void)status_flags;

    git_index* index = (git_index*)payload;

    /*qInfo() << "stat" << status_flags << path;*/

    pflog("status %u %s", status_flags, path);

    if (strcmp(path, "gwatch") != 0)
    {
        int error = git_index_add_bypath(index, path);
        if (check_error(error))
        {
            plog("niedobrze");
        }
    }

    return 0;
}

void lp_cb(uv_timer_t* handle)
{
    (void)handle;
    plog("Committing...");

    git_repository* repo = NULL;
    if (check_error(git_repository_open(&repo, ".")))
    {
        plog("Cannot open the git repository");
        start_fs_listener();
        return;
    }

    int unborn = git_repository_head_unborn(repo);
    if (check_error(unborn))
    {
        plog("Cannot check if HEAD is unborn");
        git_repository_free(repo);
        start_fs_listener();
        return;
    }
    if (unborn)
    {
        plog("HEAD is unborn - creating initial commit...");
    }

    int detached = git_repository_head_detached(repo);
    if (check_error(detached))
    {
        plog("Cannot check if HEAD is detached");
        git_repository_free(repo);
        start_fs_listener();
        return;
    }
    if (detached)
    {
        plog("HEAD is detached - will not commit");
        git_repository_free(repo);
        start_fs_listener();
        return;
    }

    git_index* index;
    git_repository_index(&index, repo);

    int error2 = git_status_foreach(repo, status_cb, index);
    (void)error2;
    git_index_write(index);

    git_oid tree_id;
    git_index_write_tree(&tree_id, index);

    git_tree* tree;
    git_tree_lookup(&tree, repo, &tree_id);

    git_signature* gwatch_sig;
    git_signature_now(&gwatch_sig, "gwatch", "gwatch@example.com");

    git_oid commit_id;
    if (!unborn)
    {
        git_oid parent_id;
        git_reference_name_to_id(&parent_id, repo, "HEAD");
        git_commit* parent;
        git_commit_lookup(&parent, repo, &parent_id);

        git_commit_create_v(
            &commit_id,
            repo,
            "HEAD",
            gwatch_sig, // author
            gwatch_sig, // committer
            NULL, // utf-8 encoding
            "gwatch auto-commit",
            tree,
            1,
            parent
        );

        git_commit_free(parent);
    }
    else
    {
        git_commit_create_v(
            &commit_id,
            repo,
            "HEAD",
            gwatch_sig,
            gwatch_sig,
            NULL,
            "gwatch auto-commit",
            tree,
            0
        );
    }

    git_signature_free(gwatch_sig);
    git_tree_free(tree);
    git_index_free(index);
    git_repository_free(repo);

    plog("done");

    start_fs_listener();
}

void fs_cb(uv_fs_event_t* handle, const char* filename, int events, int status)
{
    (void)handle;
    (void)filename;
    (void)status;

    if (events & UV_CHANGE)
        pflog("File changed - %s, starting timer", filename);

    if (events & UV_RENAME)
        pflog("File (re)moved - %s, starting timer", filename);

    stop_fs_listener(handle);
    start_lp_timer();
}

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    git_libgit2_init();
    init_io_devices();

    start_fs_listener();
    uv_run(&loop, UV_RUN_DEFAULT);

    uv_loop_close(&loop);
    git_libgit2_shutdown();
    return 0;
}
