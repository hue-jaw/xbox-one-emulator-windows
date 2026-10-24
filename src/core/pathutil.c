#include "pathutil.h"

#include <string.h>

#if defined(_WIN32) || defined(_XBOX)
#define XIB_SEP '\\'
#else
#define XIB_SEP '/'
#endif

static int is_sep(char c) { return c == '/' || c == '\\'; }

int pu_next_component(const char **cursor, char *out, size_t outsz)
{
    const char *p = *cursor;
    size_t n = 0;

    if (!p || !*p)
        return -1;

    while (*p && is_sep(*p))
        p++;
    if (!*p) {
        *cursor = p;
        return -1;
    }

    while (*p && !is_sep(*p)) {
        if (n + 1 < outsz)
            out[n++] = *p;
        p++;
    }
    out[n] = '\0';

    while (*p && is_sep(*p))
        p++;
    *cursor = p;
    return 0;
}

size_t pu_sanitize(const char *in, char *out, size_t outsz)
{
    size_t n = 0;
    size_t i;

    if (!in || !out || outsz == 0)
        return 0;

    for (i = 0; in[i] && n + 1 < outsz && n < 42; i++) {
        unsigned char c = (unsigned char)in[i];
        if (c < 0x20 || c == '"' || c == '*' || c == '/' || c == '\\' ||
            c == ':' || c == '<' || c == '>' || c == '?' || c == '|')
            c = '_';
        out[n++] = (char)c;
    }

    /* pas de point / espace final (FATX n'aime pas) */
    while (n > 0 && (out[n - 1] == '.' || out[n - 1] == ' '))
        n--;
    out[n] = '\0';

    if (n == 0 || strcmp(out, ".") == 0 || strcmp(out, "..") == 0) {
        const char *fallback = "_";
        if (outsz < 2)
            return 0;
        strcpy(out, fallback);
        return 1;
    }
    return n;
}

int pu_join(char *out, size_t outsz, const char *a, const char *b)
{
    size_t la, lb;
    int need_sep;

    if (!out || outsz == 0)
        return -1;
    if (!a)
        a = "";
    if (!b)
        b = "";

    /* Chemin absolu : b gagne (mais "hdd:\\" -> on garde le prefixe device) */
    if (is_sep(b[0]) && a[0] && !is_sep(a[0]) && !a[1]) {
        /* cas "C:" + "\\x" : on colle sans separateur */
        la = strlen(a);
        lb = strlen(b);
        if (la + lb + 1 > outsz)
            return -1;
        memcpy(out, a, la);
        memcpy(out + la, b, lb + 1);
        return 0;
    }

    la = strlen(a);
    lb = strlen(b);
    need_sep = la > 0 && !is_sep(a[la - 1]);
    if (la + (need_sep ? 1u : 0u) + lb + 1 > outsz)
        return -1;
    memcpy(out, a, la);
    if (need_sep)
        out[la++] = XIB_SEP;
    memcpy(out + la, b, lb + 1);
    return 0;
}

void pu_to_backslash(char *s)
{
    for (; s && *s; s++)
        if (*s == '/')
            *s = '\\';
}

void pu_native_separators(char *s)
{
#if defined(_WIN32) || defined(_XBOX)
    pu_to_backslash(s);
#else
    for (; s && *s; s++)
        if (*s == '\\')
            *s = '/';
#endif
}

void pu_dirname(char *path)
{
    size_t i;
    if (!path)
        return;
    i = strlen(path);
    while (i > 0 && !is_sep(path[i - 1]))
        i--;
    if (i == 0) {
        path[0] = '\0';
        return;
    }
    /* on garde un seul separateur final, sauf pour "hdd:\\" */
    while (i > 1 && is_sep(path[i - 2]))
        i--;
    path[i] = '\0';
}

const char *pu_basename(const char *path)
{
    const char *base = path;
    if (!path)
        return "";
    for (; *path; path++)
        if (is_sep(*path))
            base = path + 1;
    return base;
}

int pu_has_ext(const char *path, const char *ext)
{
    size_t lp, le, i;
    if (!path || !ext)
        return 0;
    lp = strlen(path);
    le = strlen(ext);
    if (le > lp)
        return 0;
    for (i = 0; i < le; i++) {
        char a = path[lp - le + i];
        char b = ext[i];
        if (a >= 'A' && a <= 'Z')
            a = (char)(a - 'A' + 'a');
        if (b >= 'A' && b <= 'Z')
            b = (char)(b - 'A' + 'a');
        if (a != b)
            return 0;
    }
    return 1;
}
