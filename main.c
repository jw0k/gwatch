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
 */

uv_loop_t loop;
uv_timer_t low_pass_timer;
uv_fs_event_t fs_event_req;
int files_added = 0;

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
    (void)status_flags;

    git_index* index = (git_index*)payload;

    //TODO: do not hardcode the program name
    if (strcmp(path, "gwatch") != 0)
    {
        if ((status_flags & GIT_STATUS_WT_NEW) ||
            (status_flags & GIT_STATUS_WT_MODIFIED) ||
            (status_flags & GIT_STATUS_WT_TYPECHANGE) ||
            (status_flags & GIT_STATUS_WT_RENAMED))
        {
            if (check_error(git_index_add_bypath(index, path)))
            {
                pflog("Cannot add %s to index", path);
                return -1;
            }
        }
        else if (status_flags & GIT_STATUS_WT_DELETED)
        {
            if (check_error(git_index_remove_bypath(index, path)))
            {
                pflog("Cannot remove %s from index", path);
                return -1;
            }
        }

        ++files_added;
    }

    return 0;
}

void lp_cb(uv_timer_t* handle)
{
    (void)handle;
    plog("Trying to commit...");

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

    git_index* index = NULL;
    if (check_error(git_repository_index(&index, repo)))
    {
        plog("Cannot open the index file");
        git_repository_free(repo);
        start_fs_listener();
        return;
    }

    files_added = 0;
    int status_result = git_status_foreach(repo, status_cb, index);
    if (check_error(status_result))
    {
        plog("Cannot add files to index");
        git_index_free(index);
        git_repository_free(repo);
        start_fs_listener();
        return;
    }
    if (files_added == 0)
    {
        plog("No changes - will not commit");
        git_index_free(index);
        git_repository_free(repo);
        start_fs_listener();
        return;
    }
    if (check_error(git_index_write(index)))
    {
        plog("Cannot write index to disk");
        git_index_free(index);
        git_repository_free(repo);
        start_fs_listener();
        return;
    }

    git_oid tree_id;
    if (check_error(git_index_write_tree(&tree_id, index)))
    {
        plog("Cannot write index as a tree");
        git_index_free(index);
        git_repository_free(repo);
        start_fs_listener();
        return;
    }

    git_tree* tree = NULL;
    if (check_error(git_tree_lookup(&tree, repo, &tree_id)))
    {
        plog("Cannot find the index tree object");
        git_index_free(index);
        git_repository_free(repo);
        start_fs_listener();
        return;
    }

    git_signature* gwatch_sig = NULL;
    if (check_error(git_signature_now(&gwatch_sig,
                    "gwatch", "gwatch@example.com")))
    {
        plog("Cannot create the signature");
        git_tree_free(tree);
        git_index_free(index);
        git_repository_free(repo);
        start_fs_listener();
        return;
    }

    git_oid commit_id;
    if (!unborn)
    {
        git_oid parent_id;
        if (check_error(git_reference_name_to_id(&parent_id, repo, "HEAD")))
        {
            plog("Cannot find HEAD id");
            git_tree_free(tree);
            git_index_free(index);
            git_repository_free(repo);
            start_fs_listener();
            return;
        }
        git_commit* parent = NULL;
        if (check_error(git_commit_lookup(&parent, repo, &parent_id)))
        {
            plog("Cannot find HEAD commit");
            git_tree_free(tree);
            git_index_free(index);
            git_repository_free(repo);
            start_fs_listener();
            return;
        }

        if (check_error(git_commit_create_v(
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
        )))
        {
            plog("Cannot create a commit");
            git_commit_free(parent);
            git_tree_free(tree);
            git_index_free(index);
            git_repository_free(repo);
            start_fs_listener();
            return;
        }

        git_commit_free(parent);
    }
    else
    {
        if (check_error(git_commit_create_v(
            &commit_id,
            repo,
            "HEAD",
            gwatch_sig,
            gwatch_sig,
            NULL,
            "gwatch auto-commit",
            tree,
            0
        )))
        {
            plog("Cannot create initial commit");
            git_tree_free(tree);
            git_index_free(index);
            git_repository_free(repo);
            start_fs_listener();
            return;
        }
    }

    git_signature_free(gwatch_sig);
    git_tree_free(tree);
    git_index_free(index);
    git_repository_free(repo);
    start_fs_listener();

    plog("Successfully created a new commit");
}

void fs_cb(uv_fs_event_t* handle, const char* filename, int events, int status)
{
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
