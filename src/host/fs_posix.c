/* fs_posix.c - implémentation fs.h pour hôte POSIX (tests + CLI PC) */
#if !defined(_WIN32)

#include "../core/fs.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <unistd.h>

struct fs_dir {
    DIR *d;
    char path[4096];
};

int fs_mkdir(const char *path)
{
    if (mkdir(path, 0777) == 0)
        return 0;
    return fs_isdir(path) ? 0 : -1;
}

int fs_remove(const char *path) { return remove(path) == 0 ? 0 : -1; }

int fs_rmdir(const char *path) { return rmdir(path) == 0 ? 0 : -1; }

int fs_exists(const char *path)
{
    struct stat st;
    return stat(path, &st) == 0;
}

int fs_isdir(const char *path)
{
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

int fs_rename(const char *from, const char *to)
{
    return rename(from, to) == 0 ? 0 : -1;
}

fs_dir *fs_opendir(const char *path)
{
    DIR *d = opendir(path);
    fs_dir *out;
    if (!d)
        return NULL;
    out = (fs_dir *)calloc(1, sizeof(*out));
    if (!out) {
        closedir(d);
        return NULL;
    }
    out->d = d;
    strncpy(out->path, path, sizeof(out->path) - 1);
    return out;
}

int fs_readdir(fs_dir *d, char *name, size_t namesz, int *is_dir)
{
    struct dirent *e;
    struct stat st;
    char full[8300];

    if (!d)
        return -1;
    e = readdir(d->d);
    if (!e)
        return 0;
    strncpy(name, e->d_name, namesz - 1);
    name[namesz - 1] = '\0';
    if (is_dir) {
        snprintf(full, sizeof(full), "%s/%s", d->path, e->d_name);
        *is_dir = (stat(full, &st) == 0 && S_ISDIR(st.st_mode)) ? 1 : 0;
    }
    return 1;
}

void fs_closedir(fs_dir *d)
{
    if (!d)
        return;
    closedir(d->d);
    free(d);
}

uint64_t fs_free_space(const char *path)
{
    struct statvfs sv;
    if (statvfs(path, &sv) != 0)
        return 0;
    return (uint64_t)sv.f_bavail * (uint64_t)sv.f_frsize;
}

#endif /* !_WIN32 */
