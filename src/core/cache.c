#include "cache.h"
#include "fs.h"

#include <stdio.h>
#include <string.h>

#define SESSION_FILE ".xiboot-session"

static int purge_recursive(cache_t *c, const char *path, int depth)
{
    fs_dir *d;
    char name[XIB_MAX_PATH];
    char child[XIB_MAX_PATH];
    int is_dir;
    int rc = 0;

    if (depth > 32)
        return -1;

    d = fs_opendir(path);
    if (!d)
        return -1;

    while (fs_readdir(d, name, sizeof(name), &is_dir) == 1) {
        if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
            continue;
        if (pu_join(child, sizeof(child), path, name) != 0)
            continue;

        if (is_dir) {
            purge_recursive(c, child, depth + 1);
            if (fs_rmdir(child) == 0)
                c->purged_dirs++;
        } else {
            if (fs_remove(child) == 0)
                c->purged_files++;
        }
    }
    fs_closedir(d);
    return rc;
}

int cache_ensure_dir(const char *path)
{
    char tmp[XIB_MAX_PATH];
    size_t i, n;

    if (!path || !*path)
        return -1;
    if (fs_isdir(path))
        return 0;

    n = strlen(path);
    if (n >= sizeof(tmp))
        return -1;
    memcpy(tmp, path, n + 1);

    /* on saute le prefixe device ("hdd:\\", "usb0:\\", "C:\\") */
    i = 0;
    if (n >= 2 && tmp[1] == ':')
        i = 2;
    for (; i < n; i++) {
        if (tmp[i] == '\\' || tmp[i] == '/') {
            char save = tmp[i];
            tmp[i] = '\0';
            if (tmp[0] && !(strlen(tmp) == 2 && tmp[1] == ':'))
                fs_mkdir(tmp);
            tmp[i] = save;
        }
    }
    if (fs_mkdir(tmp) == 0 || fs_isdir(tmp))
        return 0;
    return -1;
}

int cache_init(cache_t *c, const char *root, int keep)
{
    if (!c || !root || strlen(root) >= sizeof(c->root))
        return -1;

    memset(c, 0, sizeof(*c));
    strcpy(c->root, root);
    c->keep = keep;

    if (cache_ensure_dir(c->root) != 0 && !fs_isdir(c->root))
        return -1;
    return 0;
}

int cache_purge(cache_t *c)
{
    if (!c)
        return -1;
    if (c->keep)
        return 0;
    if (!fs_isdir(c->root))
        return cache_ensure_dir(c->root) == 0 ? 0 : -1;
    purge_recursive(c, c->root, 0);
    return 0;
}

int cache_game_dir(cache_t *c, const char *iso_path, char *out, size_t outsz)
{
    char base[XIB_MAX_PATH];
    char safe[64];

    if (!c || !iso_path || !out)
        return -1;

    strncpy(base, pu_basename(iso_path), sizeof(base) - 1);
    base[sizeof(base) - 1] = '\0';

    /* On retire l'extension pour un nom de dossier plus lisible. */
    {
        char *dot = strrchr(base, '.');
        if (dot && dot != base)
            *dot = '\0';
    }
    pu_sanitize(base, safe, sizeof(safe));

    if (pu_join(out, outsz, c->root, safe) != 0)
        return -1;
    return cache_ensure_dir(out);
}

int cache_has_space(const cache_t *c, uint64_t needed)
{
    uint64_t freeb;

    if (!c)
        return 0;
    freeb = fs_free_space(c->root);
    if (freeb == 0)
        return 1; /* inconnu : on laisse l'ecriture echouer proprement */
    /* marge de securite de 64 Mio */
    return freeb > needed + (64ull * 1024ull * 1024ull);
}

static int marker_path(const cache_t *c, char *out, size_t outsz)
{
    return pu_join(out, outsz, c->root, SESSION_FILE);
}

int cache_mark_session(const cache_t *c, const char *iso_path)
{
    char p[XIB_MAX_PATH];
    FILE *f;

    if (marker_path(c, p, sizeof(p)) != 0)
        return -1;
    f = fopen(p, "wb");
    if (!f)
        return -1;
    if (iso_path)
        fwrite(iso_path, 1, strlen(iso_path), f);
    fclose(f);
    return 0;
}

int cache_has_session_marker(const cache_t *c)
{
    char p[XIB_MAX_PATH];
    FILE *f;

    if (marker_path(c, p, sizeof(p)) != 0)
        return 0;
    f = fopen(p, "rb");
    if (!f)
        return 0;
    fclose(f);
    return 1;
}

int cache_clear_session(const cache_t *c)
{
    char p[XIB_MAX_PATH];

    if (marker_path(c, p, sizeof(p)) != 0)
        return -1;
    return fs_remove(p);
}
