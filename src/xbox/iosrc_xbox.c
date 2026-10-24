/* iosrc_xbox.c - lecture d'images ISO sur la console (offsets 64 bits) */
#include "../core/iosrc.h"
#include "platform.h"

struct iosrc {
    HANDLE h;
    uint64_t size;
    const uint8_t *mem;
};

iosrc *iosrc_open(const char *path)
{
    HANDLE h;
    LARGE_INTEGER sz;
    iosrc *s;

    if (!path)
        return NULL;

    /* FILE_SHARE_READ : l'image peut aussi etre ouverte par le dash en
     * lecture ; on ne verrouille donc pas le fichier. */
    h = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                    FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE)
        return NULL;

    sz.QuadPart = 0;
    if (!GetFileSizeEx(h, &sz) || sz.QuadPart <= 0) {
        CloseHandle(h);
        return NULL;
    }

    s = (iosrc *)calloc(1, sizeof(*s));
    if (!s) {
        CloseHandle(h);
        return NULL;
    }
    s->h = h;
    s->size = (uint64_t)sz.QuadPart;
    s->mem = NULL;
    return s;
}

iosrc *iosrc_from_memory(const void *data, uint64_t size)
{
    iosrc *s = (iosrc *)calloc(1, sizeof(*s));
    if (!s)
        return NULL;
    s->h = INVALID_HANDLE_VALUE;
    s->mem = (const uint8_t *)data;
    s->size = size;
    return s;
}

int iosrc_read_at(iosrc *s, uint64_t off, void *buf, size_t len)
{
    if (!s || !buf)
        return -1;

    if (off > s->size || len > s->size - off) {
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

    {
        LARGE_INTEGER pos;
        DWORD done = 0;
        pos.QuadPart = (LONGLONG)off;
        if (!SetFilePointerEx(s->h, pos, NULL, FILE_BEGIN))
            return -1;
        if (!ReadFile(s->h, buf, (DWORD)len, &done, NULL))
            return -1;
        return done == (DWORD)len ? 0 : -1;
    }
}

uint64_t iosrc_size(iosrc *s)
{
    return s ? s->size : 0;
}

void iosrc_close(iosrc *s)
{
    if (!s)
        return;
    if (s->h != INVALID_HANDLE_VALUE)
        CloseHandle(s->h);
    free(s);
}
