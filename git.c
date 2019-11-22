#include "git.h"
#include "logs.h"
#include "args.h"

#include <git2.h>

#include <stdbool.h>
#include <string.h>

int files_added = 0;

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

bool check_if_valid_git_repo()
{
    git_repository* repo = NULL;

    if (check_error(git_repository_open(&repo, get_repo_path())))
    {
        pflog("The path %s does not represent a valid Git "
                "repository. You may want to create one using "
                "`git init`", get_repo_path());
        return false;
    }
    else
    {
        git_repository_free(repo);
        return true;
    }
}

int status_cb(const char* path, unsigned int status_flags, void* payload)
{
    git_index* index = (git_index*)payload;

    if (strcmp(path, get_prog_name()) != 0)
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

void commit_impl(git_repository** repo, git_index** index, git_tree** tree,
        git_signature** gwatch_sig, git_commit** parent)
{
    if (check_error(git_repository_open(repo, get_repo_path())))
    {
        plog("Cannot open the git repository");
        return;
    }

    int unborn = git_repository_head_unborn(*repo);
    if (check_error(unborn))
    {
        plog("Cannot check if HEAD is unborn");
        return;
    }
    if (unborn)
    {
        plog("HEAD is unborn - creating initial commit...");
    }

    int detached = git_repository_head_detached(*repo);
    if (check_error(detached))
    {
        plog("Cannot check if HEAD is detached");
        return;
    }
    if (detached)
    {
        plog("HEAD is detached - will not commit");
        return;
    }

    if (check_error(git_repository_index(index, *repo)))
    {
        plog("Cannot open the index file");
        return;
    }

    files_added = 0;
    int status_result = git_status_foreach(*repo, status_cb, index);
    if (check_error(status_result))
    {
        plog("Cannot add files to index");
        return;
    }
    if (files_added == 0)
    {
        // let's avoid too many log messages
        // plog("No changes - will not commit");
        return;
    }
    if (check_error(git_index_write(*index)))
    {
        plog("Cannot write index to disk");
        return;
    }

    git_oid tree_id;
    if (check_error(git_index_write_tree(&tree_id, *index)))
    {
        plog("Cannot write index as a tree");
        return;
    }

    if (check_error(git_tree_lookup(tree, *repo, &tree_id)))
    {
        plog("Cannot find the index tree object");
        return;
    }

    if (check_error(git_signature_now(gwatch_sig,
                    "gwatch", "gwatch@example.com")))
    {
        plog("Cannot create the signature");
        return;
    }

    git_oid commit_id;
    if (!unborn)
    {
        git_oid parent_id;
        if (check_error(git_reference_name_to_id(&parent_id, *repo, "HEAD")))
        {
            plog("Cannot find HEAD id");
            return;
        }
        if (check_error(git_commit_lookup(parent, *repo, &parent_id)))
        {
            plog("Cannot find HEAD commit");
            return;
        }

        if (check_error(git_commit_create_v(
            &commit_id,
            *repo,
            "HEAD",
            *gwatch_sig, // author
            *gwatch_sig, // committer
            NULL, // utf-8 encoding
            "gwatch auto-commit",
            *tree,
            1,
            *parent
        )))
        {
            plog("Cannot create a commit");
            return;
        }
    }
    else
    {
        if (check_error(git_commit_create_v(
            &commit_id,
            *repo,
            "HEAD",
            *gwatch_sig,
            *gwatch_sig,
            NULL,
            "gwatch auto-commit",
            *tree,
            0
        )))
        {
            plog("Cannot create initial commit");
            return;
        }
    }

    plog("Successfully created a new commit");
}

void commit()
{
    git_repository* repo = NULL;
    git_index* index = NULL;
    git_tree* tree = NULL;
    git_signature* gwatch_sig = NULL;
    git_commit* parent = NULL;

    commit_impl(&repo, &index, &tree, &gwatch_sig, &parent);

    git_commit_free(parent);
    git_signature_free(gwatch_sig);
    git_tree_free(tree);
    git_index_free(index);
    git_repository_free(repo);
}
