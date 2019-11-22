#include "fs_oper.h"
#include "args.h"

uv_fs_event_t fs_event_req;

bool dir_exists(const char* path)
{
    DWORD dwAttrib = GetFileAttributes(path);
    return (dwAttrib != INVALID_FILE_ATTRIBUTES &&
        (dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
}

void fs_listener_start_impl(uv_loop_t* loop, void(*fs_cb)(
        uv_fs_event_t* handle, const char* filename, int events, int status))
{
    uv_fs_event_init(loop, &fs_event_req);
    uv_fs_event_start(&fs_event_req, fs_cb, get_repo_path(),
        UV_FS_EVENT_RECURSIVE);
}

void fs_listener_stop_impl(uv_fs_event_t* handle)
{
    uv_fs_event_stop(handle);
}
