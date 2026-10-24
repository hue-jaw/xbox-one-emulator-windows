/* iosrc_stdio.c - source d'octets via stdio (hôte POSIX/Windows) */
#define _FILE_OFFSET_BITS 64
#define _POSIX_C_SOURCE 200809L
#include "../core/iosrc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct iosrc {
    FILE *f;
    uint64_t size;
    const uint8_t *mem;
};

iosrc *iosrc_open(const char *path)
{
    FILE *f;
    long long size;
    iosrc *s;

    if (!path)
        return NULL;
    f = fopen(path, "rb");
    if (!f)
        return NULL;

    if (fseeko(f, 0, SEEK_END) != 0) {
        fclose(f);
        return NULL;
    }
    size = ftello(f);
    if (size < 0) {
        fclose(f);
        return NULL;
    }
    rewind(f);

    s = (iosrc *)calloc(1, sizeof(*s));
    if (!s) {
        fclose(f);
        return NULL;
    }
    s->f = f;
    s->size = (uint64_t)size;
    s->mem = NULL;
    return s;
}

iosrc *iosrc_from_memory(const void *data, uint64_t size)
{
    iosrc *s = (iosrc *)calloc(1, sizeof(*s));
    if (!s)
        return NULL;
    s->f = NULL;
    s->mem = (const uint8_t *)data;
    s->size = size;
    return s;
}

int iosrc_read_at(iosrc *s, uint64_t off, void *buf, size_t len)
{
    if (!s || !buf)
        return -1;
    if (off > s->size || len > s->size - off) {
        /* Lecture partielle tolérée : on lit ce qui existe. */
        if (off >= s->size) {
            memset(buf, 0, len);
            return 0;
        }
        len = (size_t)(s->size - off);
    }
    if (s->mem) {
        memcpy(buf, s->mem + off, len);
        return 0;
    }
    if (fseeko(s->f, (off_t)off, SEEK_SET) != 0)
        return -1;
    return fread(buf, 1, len, s->f) == len ? 0 : -1;
}

uint64_t iosrc_size(iosrc *s) { return s ? s->size : 0; }

void iosrc_close(iosrc *s)
{
    if (!s)
        return;
    if (s->f)
        fclose(s->f);
    free(s);
}
