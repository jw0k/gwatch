#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>
#include <string.h>

#include <git2.h>
#include <uv.h>

const char* prog_name = NULL;
const char* repo_path = ".";
int timeout = 30; // s
uv_loop_t loop;
uv_timer_t low_pass_timer;

#ifdef WIN32
uv_fs_event_t fs_event_req;
#else
uv_fs_event_t* fs_event_reqs = NULL;
unsigned int num_fs_events = 0;
unsigned int fs_events_capacity = 0;
#endif

int files_added = 0;

void lp_cb(uv_timer_t* handle);
void fs_cb(uv_fs_event_t* handle, const char* filename, int events, int status);
void plog(const char* str);
void pflog(const char* fmt, ...);

#ifndef WIN32
void add_fs_event_req(uv_fs_event_t* fs_event)
{
    if (fs_event_reqs == NULL)
    {
        fs_events_capacity = 16;
        fs_event_reqs = (uv_fs_event_t*)malloc(fs_events_capacity
                * sizeof(uv_fs_event_t));
    }
    if (num_fs_events >= fs_events_capacity)
    {
        fs_events_capacity *= 2;
        void* new_buffer = realloc(fs_event_reqs, fs_events_capacity
                * sizeof(uv_fs_event_t));
        if (new_buffer == NULL)
        {
            plog("Cannot realloc fs events buffer");
            exit(1);
        }
        fs_event_reqs = (uv_fs_event_t*)new_buffer;
    }
    memcpy(fs_event_reqs + num_fs_events, fs_event, sizeof(uv_fs_event_t));
    ++num_fs_events;
}

void clear_fs_event_reqs()
{
    if (fs_event_reqs)
    {
        free(fs_event_reqs);
        fs_event_reqs = NULL;
        num_fs_events = 0;
    }
}
#endif

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
    uv_timer_init(&loop, &low_pass_timer);
}

#ifndef WIN32
#include <sys/types.h>
#include <dirent.h>
void listen_dirs_recursively(const char *name)
{
    DIR* dir;
    struct dirent *entry;

    if (!(dir = opendir(name)))
        return;

    uv_fs_event_t fs_event_r;
    uv_fs_event_init(&loop, &fs_event_r);
    add_fs_event_req(&fs_event_r);
    uv_fs_event_start(fs_event_reqs + num_fs_events - 1, fs_cb, name, 0);

    while ((entry = readdir(dir)) != NULL)
    {
        if (entry->d_type == DT_DIR)
        {
            char path[2048];
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
                continue;
            snprintf(path, sizeof(path), "%s/%s", name, entry->d_name);
            listen_dirs_recursively(path);
        }
    }

    closedir(dir);
}
#endif

void start_fs_listener()
{
#ifdef WIN32
    uv_fs_event_init(&loop, &fs_event_req);
    uv_fs_event_start(&fs_event_req, fs_cb, repo_path, UV_FS_EVENT_RECURSIVE);
#else
    listen_dirs_recursively(repo_path);
#endif
}

void stop_fs_listener(uv_fs_event_t* handle)
{
#ifdef WIN32
    uv_fs_event_stop(handle);
#else
    (void)handle;
    unsigned int i;
    for (i = 0; i < num_fs_events; ++i)
        uv_fs_event_stop(fs_event_reqs + i);
    clear_fs_event_reqs();
#endif
}

void start_lp_timer()
{
    uv_timer_start(&low_pass_timer, lp_cb, (uint64_t)timeout*1000, 0);
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

    if (strcmp(path, prog_name) != 0)
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

    if (check_error(git_repository_open(&repo, repo_path)))
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

int main(int argc, char* argv[])
{
    if (!parse_args(argc, argv))
        return -1;

    printf("Starting gwatch\n");
    printf("Watched repository: %s\n", repo_path);
    printf("Timeout: %ds\n", timeout);

    git_libgit2_init();

    git_repository* repo = NULL;

    if (check_error(git_repository_open(&repo, repo_path)))
    {
        pflog("The path %s does not represent a valid Git "
                "repository. You may want to create one using "
                "`git init`", repo_path);
    }
    else
    {
        git_repository_free(repo);
    }

    init_io_devices();

    start_fs_listener();
    uv_run(&loop, UV_RUN_DEFAULT);

    uv_loop_close(&loop);
    git_libgit2_shutdown();

    return 0;
}
