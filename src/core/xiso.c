#include "xiso.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_DEPTH 32
#define MAX_DIR_TABLE (16u * 1024u * 1024u)
#define SCAN_LIMIT (64u * 1024u * 1024u)

static uint32_t rd32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static uint16_t rd16(const uint8_t *p)
{
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

const char *xiso_attr_str(uint8_t attr)
{
    return (attr & XIB_ATTR_DIRECTORY) ? "dir" : "file";
}

/* ------------------------------------------------------------------ */
/* Ouverture / detection de partition                                  */
/* ------------------------------------------------------------------ */

static int read_dir_table(iosrc *src, const xiso_volume *vol, uint32_t sector,
                          uint32_t size, uint8_t **out)
{
    uint8_t *buf;
    uint64_t off;

    if (size == 0 || size > MAX_DIR_TABLE)
        return -1;

    buf = (uint8_t *)malloc(size);
    if (!buf)
        return -1;

    off = vol->base + (uint64_t)sector * XIB_SECTOR_SIZE;
    if (iosrc_read_at(src, off, buf, size) != 0) {
        free(buf);
        return -1;
    }
    *out = buf;
    return 0;
}

/* Verifie qu'une table de repertoire est coherente avec le mode d'offset
 * donne (0 = relatif a la table, 1 = relatif a l'entree). */
static int table_is_valid(iosrc *src, const xiso_volume *vol, uint32_t sector,
                          uint32_t size, int entry_rel)
{
    uint8_t *buf = NULL;
    int ok = 0;
    int pos = 0, nb = 0;
    int stack[512];
    uint8_t seen[4096];

    if (read_dir_table(src, vol, sector, size, &buf) != 0)
        return 0;

    memset(seen, 0, sizeof(seen));
    stack[0] = 0;
    pos = 1;

    while (pos > 0 && nb < 4096) {
        int off = stack[--pos];
        const uint8_t *e;
        uint32_t lo, ro;
        uint8_t nlen;

        if (off < 0 || (uint32_t)off + 14 > size)
            goto done;
        if ((off & 3) != 0)
            goto done;
        if (seen[off >> 3] & (1u << (off & 7)))
            goto done;
        seen[off >> 3] |= (uint8_t)(1u << (off & 7));

        e = buf + off;
        lo = rd16(e + 0);
        ro = rd16(e + 2);
        nlen = e[13];
        if (nlen == 0 || nlen > 200 || (uint32_t)off + 14u + nlen > size)
            goto done;

        nb++;
        if (lo) {
            int no = entry_rel ? off + (int)(lo * 4) : (int)(lo * 4);
            if (pos < (int)(sizeof(stack) / sizeof(stack[0])))
                stack[pos++] = no;
            else
                goto done;
        }
        if (ro) {
            int no = entry_rel ? off + (int)(ro * 4) : (int)(ro * 4);
            if (pos < (int)(sizeof(stack) / sizeof(stack[0])))
                stack[pos++] = no;
            else
                goto done;
        }
    }
    ok = (nb > 0);

done:
    free(buf);
    return ok;
}

int xiso_open_at(iosrc *src, uint64_t base, xiso_volume *vol)
{
    uint8_t vd[32];
    uint64_t img;

    if (!src || !vol)
        return -1;

    img = iosrc_size(src);
    if (base + XIB_VD_OFFSET + 32 > img)
        return -1;

    if (iosrc_read_at(src, base + XIB_VD_OFFSET, vd, sizeof(vd)) != 0)
        return -1;
    if (memcmp(vd, XIB_XDVDFS_MAGIC, XIB_XDVDFS_MAGIC_LEN) != 0)
        return -1;

    vol->base = base;
    vol->root_sector = rd32(vd + 0x14);
    vol->root_size = rd32(vd + 0x18);
    vol->entry_rel = 0;

    if (vol->root_size == 0 || vol->root_size > MAX_DIR_TABLE)
        return -1;
    if (base + (uint64_t)vol->root_sector * XIB_SECTOR_SIZE +
            vol->root_size > img + XIB_SECTOR_SIZE)
        return -1;

    /* Auto-detection de la semantique des offsets left/right. */
    if (table_is_valid(src, vol, vol->root_sector, vol->root_size, 0)) {
        vol->entry_rel = 0;
    } else if (table_is_valid(src, vol, vol->root_sector, vol->root_size, 1)) {
        vol->entry_rel = 1;
    } else {
        return -1;
    }
    return 0;
}

int xiso_probe(iosrc *src, xiso_volume *vol)
{
    /* Offsets connus des partitions jeu (en octets, depuis le debut du
     * fichier) :
     *   0x00000000  image "trimee" (extract-xiso, Xbox Image Browser)
     *   0x002080000 XGD3
     *   0x0FD90000  XGD2
     *   0x18300000  XGD1
     */
    static const uint64_t known[] = {
        0x0ull, 0x2080000ull, 0xFD90000ull, 0x18300000ull,
        0x1FB20000ull, 0x18300000ull + 0x8000000ull
    };
    size_t i;
    uint64_t img = iosrc_size(src);

    for (i = 0; i < sizeof(known) / sizeof(known[0]); i++) {
        if (known[i] + XIB_VD_OFFSET + 32 > img)
            continue;
        if (xiso_open_at(src, known[i], vol) == 0)
            return 0;
    }

    /* Scan generique : on cherche la signature du descripteur de volume
     * par blocs de 1 Mio, secteur par secteur. */
    {
        const size_t chunk = 1024 * 1024;
        uint8_t *buf = (uint8_t *)malloc(chunk + XIB_XDVDFS_MAGIC_LEN);
        uint64_t limit = img < SCAN_LIMIT ? img : SCAN_LIMIT;
        uint64_t off;
        int found = -1;

        if (!buf)
            return -1;

        for (off = 0; off + XIB_XDVDFS_MAGIC_LEN < limit; off += chunk) {
            size_t want = (size_t)((limit - off) < chunk ? (limit - off) : chunk);
            uint64_t i2;

            if (iosrc_read_at(src, off, buf, want) != 0)
                break;
            for (i2 = 0; i2 + XIB_XDVDFS_MAGIC_LEN <= want; i2 += XIB_SECTOR_SIZE) {
                if (memcmp(buf + i2, XIB_XDVDFS_MAGIC, XIB_XDVDFS_MAGIC_LEN) == 0) {
                    uint64_t magic_abs = off + i2;
                    uint64_t base = magic_abs >= XIB_VD_OFFSET
                                        ? magic_abs - XIB_VD_OFFSET
                                        : 0;
                    if (xiso_open_at(src, base, vol) == 0) {
                        found = 0;
                        goto scan_done;
                    }
                }
            }
        }
scan_done:
        free(buf);
        return found;
    }
}

/* ------------------------------------------------------------------ */
/* Parcours                                                            */
/* ------------------------------------------------------------------ */

typedef struct {
    iosrc *src;
    const xiso_volume *vol;
    xiso_visit_fn cb;
    void *user;
    xiso_stats *stats;
    int entries;      /* compteur pour detecter les boucles */
    int stop;
} walk_ctx;

/* Certaines images (graveurs "linked list") chainent les entrees en
 * utilisant le pointeur gauche et terminent un secteur par 0xFFFF ; il faut
 * alors reprendre au secteur suivant de la table. */
#define PAD_SHORT 0xFFFFu

static int child_off(const xiso_volume *vol, int off, uint16_t raw, uint32_t size)
{
    if (!raw)
        return -1;
    if (raw == PAD_SHORT) {
        int next = ((off / (int)XIB_SECTOR_SIZE) + 1) * (int)XIB_SECTOR_SIZE;
        if ((uint32_t)next + 14 > size)
            return -1;
        return next;
    }
    return vol->entry_rel ? off + (int)raw * 4 : (int)raw * 4;
}

static int walk_table(walk_ctx *w, uint32_t sector, uint32_t size, int off,
                      char *rel, size_t relsz, int depth);

static int walk_node(walk_ctx *w, const uint8_t *table, uint32_t size, int off,
                     char *rel, size_t relsz, int depth)
{
    const uint8_t *e;
    uint16_t left, right;
    uint8_t nlen, attr;
    uint32_t sector, fsize;
    char name[256];
    size_t i;

    if (w->stop)
        return 0;
    if (off < 0 || (uint32_t)off + 14 > size)
        return -1;
    if (++w->entries > 200000)
        return -1;

    e = table + off;
    left = rd16(e + 0);
    right = rd16(e + 2);
    sector = rd32(e + 4);
    fsize = rd32(e + 8);
    attr = e[12];
    nlen = e[13];

    if (nlen == 0 || (uint32_t)off + 14u + nlen > size)
        return -1;

    for (i = 0; i < nlen && i + 1 < sizeof(name); i++)
        name[i] = (char)e[14 + i];
    name[i] = '\0';

    /* chemin complet de l'entree courante */
    {
        xiso_entry ent;
        size_t plen = strlen(rel);

        memset(&ent, 0, sizeof(ent));
        if (plen)
            snprintf(ent.rel, sizeof(ent.rel), "%s/%s", rel, name);
        else
            snprintf(ent.rel, sizeof(ent.rel), "%s", name);

        ent.sector = sector;
        ent.size = fsize;
        ent.attr = attr;
        ent.depth = depth;
        memcpy(ent.name, name, sizeof(ent.name));
        ent.name[sizeof(ent.name) - 1] = '\0';

        if (w->stats) {
            if (attr & XIB_ATTR_DIRECTORY)
                w->stats->dirs++;
            else {
                w->stats->files++;
                w->stats->bytes += fsize;
            }
        }
        if (w->cb && w->cb(w->user, &ent) != 0) {
            w->stop = 1;
            return 0;
        }

        if ((attr & XIB_ATTR_DIRECTORY) && depth + 1 < MAX_DEPTH) {
            if (walk_table(w, sector, fsize, 0, ent.rel, sizeof(ent.rel),
                           depth + 1) != 0)
                return -1;
        }
    }

    if (left) {
        int no = child_off(w->vol, off, left, size);
        if (no >= 0 && no != off && (uint32_t)no + 14 <= size) {
            if (walk_node(w, table, size, no, rel, relsz, depth) != 0)
                return -1;
        }
    }
    if (right) {
        int no = child_off(w->vol, off, right, size);
        if (no >= 0 && no != off && (uint32_t)no + 14 <= size) {
            if (walk_node(w, table, size, no, rel, relsz, depth) != 0)
                return -1;
        }
    }
    return 0;
}

static int walk_table(walk_ctx *w, uint32_t sector, uint32_t size, int off,
                      char *rel, size_t relsz, int depth)
{
    uint8_t *table = NULL;
    int rc;

    if (read_dir_table(w->src, w->vol, sector, size, &table) != 0)
        return -1;
    rc = walk_node(w, table, size, off, rel, relsz, depth);
    free(table);
    return rc;
}

int xiso_walk(iosrc *src, const xiso_volume *vol, xiso_visit_fn cb, void *user)
{
    walk_ctx w;
    char rel[512];
    int rc;

    if (!src || !vol)
        return -1;

    memset(&w, 0, sizeof(w));
    w.src = src;
    w.vol = vol;
    w.cb = cb;
    w.user = user;
    w.entries = 0;
    w.stop = 0;
    rel[0] = '\0';

    rc = walk_table(&w, vol->root_sector, vol->root_size, 0, rel, sizeof(rel), 0);
    return rc == 0 ? 0 : -1;
}

int xiso_stat(iosrc *src, const xiso_volume *vol, xiso_stats *st)
{
    walk_ctx w;
    char rel[512];
    int rc;

    if (!src || !vol || !st)
        return -1;

    memset(&w, 0, sizeof(w));
    memset(st, 0, sizeof(*st));
    w.src = src;
    w.vol = vol;
    w.cb = NULL;
    w.user = NULL;
    w.stats = st;
    rel[0] = '\0';

    rc = walk_table(&w, vol->root_sector, vol->root_size, 0, rel, sizeof(rel), 0);
    st->base = vol->base;
    return rc == 0 ? 0 : -1;
}

/* ------------------------------------------------------------------ */
/* Recherche / lecture                                                 */
/* ------------------------------------------------------------------ */

typedef struct {
    const char *want;
    xiso_entry *out;
    int found;
} find_ctx;

static int find_cb(void *user, const xiso_entry *e)
{
    find_ctx *f = (find_ctx *)user;
    if (strcmp(e->rel, f->want) == 0) {
        *f->out = *e;
        f->found = 1;
        return 1;
    }
    return 0;
}

static void normalize_rel(const char *in, char *out, size_t outsz)
{
    size_t n = 0;
    size_t i;
    while (*in == '/' || *in == '\\')
        in++;
    for (i = 0; in[i] && n + 1 < outsz; i++) {
        char c = in[i];
        if (c == '\\')
            c = '/';
        if (c == '/' && n > 0 && out[n - 1] == '/')
            continue;
        out[n++] = c;
    }
    out[n] = '\0';
}

int xiso_find(iosrc *src, const xiso_volume *vol, const char *relpath,
              xiso_entry *out)
{
    find_ctx f;
    char norm[XIB_MAX_RELPATH];

    if (!src || !vol || !relpath || !out)
        return -1;

    normalize_rel(relpath, norm, sizeof(norm));
    f.want = norm;
    f.out = out;
    f.found = 0;
    memset(out, 0, sizeof(*out));
    xiso_walk(src, vol, find_cb, &f);
    return f.found ? 0 : -1;
}

int xiso_read_file(iosrc *src, const xiso_volume *vol, const xiso_entry *e,
                   uint64_t off, void *buf, size_t len)
{
    uint64_t pos;

    if (!src || !vol || !e)
        return -1;
    if (off >= e->size)
        return 0;
    if (off + len > e->size)
        len = (size_t)(e->size - off);

    pos = vol->base + (uint64_t)e->sector * XIB_SECTOR_SIZE + off;
    return iosrc_read_at(src, pos, buf, len);
}
