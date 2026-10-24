#include "extract.h"
#include "cache.h"
#include "fs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define COPY_CHUNK (256u * 1024u)
#define MAX_PAYLOAD (4ull * 1024ull * 1024ull * 1024ull) /* 4 Gio */

static int stats_cb(void *user, const xiso_entry *e)
{
    extract_job *j = (extract_job *)user;
    if (!(e->attr & XIB_ATTR_DIRECTORY)) {
        j->files_total++;
        j->bytes_total += e->size;
    }
    return 0;
}

int extract_prepare(extract_job *j, iosrc *src, const xiso_volume *vol,
                    const char *root, extract_progress_fn cb, void *user)
{
    xiso_stats st;

    if (!j || !src || !vol || !root)
        return -1;

    memset(j, 0, sizeof(*j));
    j->src = src;
    j->vol = vol;
    j->on_progress = cb;
    j->user = user;

    if (strlen(root) >= sizeof(j->root))
        return -1;
    strcpy(j->root, root);

    if (xiso_walk(src, vol, stats_cb, j) != 0)
        return -1;

    if (j->bytes_total > MAX_PAYLOAD)
        return -1;

    if (cache_ensure_dir(j->root) != 0 && !fs_isdir(j->root))
        return -1;

    memset(&st, 0, sizeof(st));
    st.bytes = j->bytes_total;
    (void)st;
    return 0;
}

int extract_map_relpath(const char *rel, char *out, size_t outsz)
{
    char comp[256];
    char safe[64];
    const char *cur = rel;
    size_t n = 0;

    if (!rel || !out || outsz == 0)
        return -1;
    out[0] = '\0';

    while (pu_next_component(&cur, comp, sizeof(comp)) == 0) {
        pu_sanitize(comp, safe, sizeof(safe));
        if (n + strlen(safe) + 2 > outsz)
            return -1;
        if (n)
            out[n++] = '/';
        strcpy(out + n, safe);
        n += strlen(safe);
    }
    return out[0] ? 0 : -1;
}

static int write_file(extract_job *j, const xiso_entry *e, const char *dst)
{
    uint8_t *buf;
    uint64_t off = 0;
    FILE *f;

    buf = (uint8_t *)malloc(COPY_CHUNK);
    if (!buf)
        return -1;

    f = fopen(dst, "wb");
    if (!f) {
        free(buf);
        return -1;
    }

    while (off < e->size) {
        size_t want = (size_t)((e->size - off) < COPY_CHUNK ? (e->size - off)
                                                           : COPY_CHUNK);
        if (xiso_read_file(j->src, j->vol, e, off, buf, want) != 0)
            break;
        if (fwrite(buf, 1, want, f) != want)
            break;
        off += want;
        j->bytes_done += want;
        j->current = e->rel;
        if (j->on_progress) {
            extract_progress p;
            p.bytes_done = j->bytes_done;
            p.bytes_total = j->bytes_total;
            p.files_done = j->files_done;
            p.files_total = j->files_total;
            p.current = j->current;
            if (j->on_progress(j->user, &p) != 0) {
                j->cancel = 1;
                break;
            }
        }
    }

    fclose(f);
    free(buf);

    if (j->cancel || off != e->size) {
        fs_remove(dst);
        return -1;
    }
    j->files_done++;
    return 0;
}

static int extract_cb(void *user, const xiso_entry *e)
{
    extract_job *j = (extract_job *)user;
    char rel[XIB_MAX_PATH];
    char dst[XIB_MAX_PATH];

    if (j->cancel)
        return 1;

    if (extract_map_relpath(e->rel, rel, sizeof(rel)) != 0)
        return 0;

    if (pu_join(dst, sizeof(dst), j->root, rel) != 0)
        return 0;
    pu_native_separators(dst);

    if (e->attr & XIB_ATTR_DIRECTORY) {
        cache_ensure_dir(dst);
        return 0;
    }

    /* dossier parent */
    {
        char parent[XIB_MAX_PATH];
        strcpy(parent, dst);
        pu_dirname(parent);
        if (parent[0])
            cache_ensure_dir(parent);
    }

    if (j->skip_existing && fs_exists(dst)) {
        j->bytes_done += e->size;
        j->files_done++;
        return 0;
    }

    if (write_file(j, e, dst) != 0)
        j->errors++;

    return 0;
}

int extract_all(extract_job *j)
{
    if (!j)
        return -1;
    if (xiso_walk(j->src, j->vol, extract_cb, j) != 0)
        return -1;
    if (j->cancel)
        return -1;
    return j->errors == 0 ? 0 : -1;
}

typedef struct {
    char *rel;
    size_t relsz;
    int found;
} launch_ctx;

static int launch_cb(void *user, const xiso_entry *e)
{
    launch_ctx *l = (launch_ctx *)user;

    if (e->attr & XIB_ATTR_DIRECTORY)
        return 0;

    /* On ne garde que la racine du volume. */
    if (strchr(e->rel, '/'))
        return 0;

    if (pu_has_ext(e->name, ".xex")) {
        if (!l->found || pu_has_ext(e->name, "default.xex")) {
            strncpy(l->rel, e->rel, l->relsz - 1);
            l->rel[l->relsz - 1] = '\0';
            l->found = 1;
            if (pu_has_ext(e->name, "default.xex"))
                return 1; /* priorite maximale */
        }
    }
    return 0;
}

int extract_find_launcher(iosrc *src, const xiso_volume *vol, char *rel,
                          size_t relsz)
{
    launch_ctx l;

    if (!src || !vol || !rel || relsz == 0)
        return -1;

    l.rel = rel;
    l.relsz = relsz;
    l.found = 0;
    rel[0] = '\0';
    xiso_walk(src, vol, launch_cb, &l);
    return l.found ? 0 : -1;
}
