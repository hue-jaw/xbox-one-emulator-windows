/* fs_xbox.c - fs.h au dessus de l'API fichiers Xbox 360 (xtl.h / xkelib) */
#include "../core/fs.h"
#include "platform.h"

struct fs_dir {
    HANDLE h;
    WIN32_FIND_DATAA data;
    int first;
    int done;
};

int fs_mkdir(const char *path)
{
    if (CreateDirectoryA(path, NULL))
        return 0;
    return fs_isdir(path) ? 0 : -1;
}

int fs_remove(const char *path)
{
    return DeleteFileA(path) ? 0 : -1;
}

int fs_rmdir(const char *path)
{
    return RemoveDirectoryA(path) ? 0 : -1;
}

int fs_exists(const char *path)
{
    DWORD attr = GetFileAttributesA(path);
    return attr != 0xFFFFFFFF ? 1 : 0;
}

int fs_isdir(const char *path)
{
    DWORD attr = GetFileAttributesA(path);
    if (attr == 0xFFFFFFFF)
        return 0;
    return (attr & FILE_ATTRIBUTE_DIRECTORY) ? 1 : 0;
}

int fs_rename(const char *from, const char *to)
{
    return MoveFileA(from, to) ? 0 : -1;
}

fs_dir *fs_opendir(const char *path)
{
    char pattern[520];
    fs_dir *d;

    if (!path)
        return NULL;
    /* "hdd:\\Games" -> "hdd:\\Games\\*" */
    if (path[0] && path[strlen(path) - 1] == '\\')
        snprintf(pattern, sizeof(pattern), "%s*", path);
    else
        snprintf(pattern, sizeof(pattern), "%s\\*", path);

    d = (fs_dir *)calloc(1, sizeof(*d));
    if (!d)
        return NULL;

    d->h = FindFirstFileA(pattern, &d->data);
    if (d->h == INVALID_HANDLE_VALUE) {
        free(d);
        return NULL;
    }
    d->first = 1;
    d->done = 0;
    return d;
}

int fs_readdir(fs_dir *d, char *name, size_t namesz, int *is_dir)
{
    if (!d || d->done)
        return 0;

    if (!d->first) {
        if (!FindNextFileA(d->h, &d->data)) {
            d->done = 1;
            return 0;
        }
    } else {
        d->first = 0;
    }

    strncpy(name, d->data.cFileName, namesz - 1);
    name[namesz - 1] = '\0';
    if (is_dir)
        *is_dir = (d->data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? 1 : 0;
    return 1;
}

void fs_closedir(fs_dir *d)
{
    if (!d)
        return;
    if (d->h && d->h != INVALID_HANDLE_VALUE)
        FindClose(d->h);
    free(d);
}

uint64_t fs_free_space(const char *path)
{
    ULARGE_INTEGER avail, total, freeb;

    avail.QuadPart = total.QuadPart = freeb.QuadPart = 0;
    if (!GetDiskFreeSpaceExA(path, &avail, &total, &freeb))
        return 0;
    return (uint64_t)avail.QuadPart;
}
