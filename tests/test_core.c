/* test_core.c - tests du coeur (parseur XDVDFS, extraction, cache, session) */
#include "src/core/cache.h"
#include "src/core/extract.h"
#include "src/core/fs.h"
#include "src/core/iosrc.h"
#include "src/core/pathutil.h"
#include "src/core/session.h"
#include "src/core/xiso.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_fail;
static int g_ok;

#define CHECK(cond)                                                           \
    do {                                                                      \
        if (cond) {                                                           \
            g_ok++;                                                           \
        } else {                                                              \
            g_fail++;                                                         \
            printf("  ECHEC %s:%d  %s\n", __FILE__, __LINE__, #cond);          \
        }                                                                     \
    } while (0)

#define CHECK_STR(a, b)                                                       \
    do {                                                                      \
        const char *x_ = (a);                                                 \
        const char *y_ = (b);                                                 \
        if (x_ && y_ && strcmp(x_, y_) == 0) {                                \
            g_ok++;                                                           \
        } else {                                                              \
            g_fail++;                                                         \
            printf("  ECHEC %s:%d  \"%s\" != \"%s\"\n", __FILE__, __LINE__,   \
                   x_ ? x_ : "(null)", y_ ? y_ : "(null)");                   \
        }                                                                     \
    } while (0)

/* ---------------------------------------------------------------- */
static void test_pathutil(void)
{
    char out[64];
    const char *cur;

    printf("- pathutil\n");

    pu_sanitize("normal.bin", out, sizeof(out));
    CHECK_STR(out, "normal.bin");

    pu_sanitize("bad:name*with?chars|.bin", out, sizeof(out));
    CHECK_STR(out, "bad_name_with_chars_.bin");

    pu_sanitize("...", out, sizeof(out));
    CHECK_STR(out, "_");

    pu_sanitize("..", out, sizeof(out));
    CHECK_STR(out, "_");

    {
        char longname[80];
        memset(longname, 'a', sizeof(longname) - 1);
        longname[sizeof(longname) - 1] = '\0';
        pu_sanitize(longname, out, sizeof(out));
        CHECK(strlen(out) == 42);
    }

    CHECK(pu_has_ext("DEFAULT.XEX", ".xex") == 1);
    CHECK(pu_has_ext("game.iso", ".iso") == 1);
    CHECK(pu_has_ext("game.iso", ".xex") == 0);

    cur = "a/b/c";
    CHECK(pu_next_component(&cur, out, sizeof(out)) == 0);
    CHECK_STR(out, "a");
    CHECK(pu_next_component(&cur, out, sizeof(out)) == 0);
    CHECK_STR(out, "b");
    CHECK(pu_next_component(&cur, out, sizeof(out)) == 0);
    CHECK_STR(out, "c");
    CHECK(pu_next_component(&cur, out, sizeof(out)) == -1);

    CHECK_STR(pu_basename("hdd:\\Games\\x.iso"), "x.iso");
    CHECK_STR(pu_basename("/tmp/x.iso"), "x.iso");
}

static void test_extract_map(void)
{
    char out[256];

    printf("- mapping des chemins\n");

    CHECK(extract_map_relpath("media/textures/a.dds", out, sizeof(out)) == 0);
    CHECK_STR(out, "media/textures/a.dds");

    CHECK(extract_map_relpath("data/weird:name*.bin", out, sizeof(out)) == 0);
    CHECK_STR(out, "data/weird_name_.bin");

    CHECK(extract_map_relpath("../../../etc/passwd", out, sizeof(out)) == 0);
    CHECK(strstr(out, "..") == NULL);
}

static void test_iso(const char *iso_path)
{
    iosrc *src;
    xiso_volume vol;
    xiso_stats st;
    char launcher[256];

    printf("- ISO %s\n", iso_path);

    src = iosrc_open(iso_path);
    CHECK(src != NULL);
    if (!src)
        return;

    CHECK(xiso_probe(src, &vol) == 0);
    CHECK(xiso_stat(src, &vol, &st) == 0);
    printf("    base=0x%llx fichiers=%u dossiers=%u octets=%llu\n",
           (unsigned long long)vol.base, st.files, st.dirs,
           (unsigned long long)st.bytes);
    CHECK(st.files == 7);
    CHECK(st.dirs == 4); /* data, media, textures, deep */
    CHECK(st.bytes > 0);

    CHECK(extract_find_launcher(src, &vol, launcher, sizeof(launcher)) == 0);
    CHECK_STR(launcher, "default.xex");

    {
        xiso_entry e;
        unsigned char buf[512];
        CHECK(xiso_find(src, &vol, "default.xex", &e) == 0);
        CHECK(e.size == 8 + 16 * 64);
        CHECK(xiso_read_file(src, &vol, &e, 0, buf, 4) == 0);
        CHECK(memcmp(buf, "XEX2", 4) == 0);

        CHECK(xiso_find(src, &vol, "media/textures/deep/tex.dat", &e) == 0);
        CHECK(e.size == 5003);
        CHECK(xiso_read_file(src, &vol, &e, 0, buf, 3) == 0);
        CHECK(memcmp(buf, "TEX", 3) == 0);

        CHECK(xiso_find(src, &vol, "nope.bin", &e) != 0);
    }

    iosrc_close(src);
}

static int count_cb(void *user, const xiso_entry *e)
{
    (void)e;
    (*(int *)user)++;
    return 0;
}

static void test_iso_offsets(const char *iso_path)
{
    /* Verifie que le scan generique retrouve une partition placee a un
     * offset inconnu (image bricolee). */
    FILE *f;
    long size;
    unsigned char *data;
    iosrc *src;
    xiso_volume vol;
    int n = 0;

    printf("- offsets non standard\n");

    f = fopen(iso_path, "rb");
    CHECK(f != NULL);
    if (!f)
        return;
    fseek(f, 0, SEEK_END);
    size = ftell(f);
    fseek(f, 0, SEEK_SET);
    data = (unsigned char *)malloc((size_t)size);
    CHECK(data != NULL);
    if (!data) {
        fclose(f);
        return;
    }
    CHECK(fread(data, 1, (size_t)size, f) == (size_t)size);
    fclose(f);

    /* fabrique une image a offset arbitraire (0x1234000) */
    {
        const long off = 0x1234000;
        unsigned char *img = (unsigned char *)calloc((size_t)size + off + 4096, 1);
        CHECK(img != NULL);
        if (!img) {
            free(data);
            return;
        }
        memcpy(img + off, data, (size_t)size);
        src = iosrc_from_memory(img, (uint64_t)(size + off + 4096));
        CHECK(xiso_probe(src, &vol) == 0);
        CHECK(vol.base == (uint64_t)off);
        CHECK(xiso_walk(src, &vol, count_cb, &n) == 0);
        CHECK(n == 11);
        iosrc_close(src);
        free(img);
    }

    free(data);
}

static int count_tree(const char *dir)
{
    fs_dir *d;
    char name[XIB_MAX_PATH];
    char child[XIB_MAX_PATH];
    int is_dir;
    int total = 0;

    d = fs_opendir(dir);
    if (!d)
        return 0;
    while (fs_readdir(d, name, sizeof(name), &is_dir) == 1) {
        if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
            continue;
        total++;
        if (is_dir) {
            pu_join(child, sizeof(child), dir, name);
            total += count_tree(child);
        }
    }
    fs_closedir(d);
    return total;
}

static void test_extract_and_purge(const char *iso_path)
{
    const char *cache_root = "build/tmp/cache";
    iosrc *src;
    xiso_volume vol;
    extract_job job;
    int rc;

    printf("- extraction + purge\n");

    src = iosrc_open(iso_path);
    CHECK(src != NULL);
    if (!src)
        return;
    CHECK(xiso_probe(src, &vol) == 0);

    CHECK(extract_prepare(&job, src, &vol, cache_root, NULL, NULL) == 0);
    CHECK(job.files_total == 7);
    CHECK(extract_all(&job) == 0);
    CHECK(job.errors == 0);

    CHECK(fs_exists("build/tmp/cache/default.xex"));
    CHECK(fs_exists("build/tmp/cache/media/textures/deep/tex.dat"));
    CHECK(fs_exists("build/tmp/cache/data/config.ini"));
    /* noms assainis */
    CHECK(fs_exists("build/tmp/cache/data/weird_name_with_bad_chars.bin"));
    printf("    arborescence extraite : %d entrees\n",
           count_tree("build/tmp/cache"));

    /* contenu identique a l'ISO */
    {
        xiso_entry e;
        FILE *f;
        unsigned char a[512], b[512];
        CHECK(xiso_find(src, &vol, "media/game.bin", &e) == 0);
        f = fopen("build/tmp/cache/media/game.bin", "rb");
        CHECK(f != NULL);
        if (f) {
            CHECK(xiso_read_file(src, &vol, &e, 0, a, sizeof(a)) == 0);
            CHECK(fread(b, 1, sizeof(b), f) == sizeof(b));
            CHECK(memcmp(a, b, sizeof(a)) == 0);
            fclose(f);
        }
    }

    /* purge */
    {
        cache_t c;
        CHECK(cache_init(&c, cache_root, 0) == 0);
        CHECK(cache_purge(&c) == 0);
        CHECK(count_tree(cache_root) == 0);
    }

    iosrc_close(src);
    rc = fs_isdir(cache_root);
    CHECK(rc == 1); /* le dossier racine survit, son contenu non */
}

static int launch_calls;
static int fake_launch(void *user, const char *exe, const char *cwd)
{
    (void)user;
    printf("    lancement simule : %s (cwd=%s)\n", exe, cwd);
    CHECK(fs_exists(exe));
    launch_calls++;
    return 0;
}

static void test_session(const char *iso_path)
{
    const char *cache_root = "build/tmp/session-cache";
    xib_options opt;
    xib_ui ui;
    xib_result res;
    int rc;

    printf("- session complete (purge -> extraction -> lancement -> purge)\n");

    /* on laisse un faux reste de session precedente */
    cache_ensure_dir("build/tmp/session-cache/vieux-jeu");
    {
        FILE *f = fopen("build/tmp/session-cache/vieux-jeu/reste.bin", "wb");
        if (f) {
            fwrite("x", 1, 1, f);
            fclose(f);
        }
    }
    CHECK(fs_exists("build/tmp/session-cache/vieux-jeu/reste.bin"));

    memset(&opt, 0, sizeof(opt));
    opt.iso_path = iso_path;
    opt.cache_root = cache_root;

    memset(&ui, 0, sizeof(ui));
    ui.launch = fake_launch;

    launch_calls = 0;
    rc = xib_run(&opt, &ui, &res);
    CHECK(rc == XIB_OK);
    CHECK(launch_calls == 1);
    CHECK(strstr(res.cache_dir, "game-trim") != NULL ||
          strstr(res.cache_dir, "game-xgd2") != NULL);
    CHECK(res.purge_ok == 1);
    /* le cache doit etre vide apres la session */
    CHECK(count_tree(cache_root) == 0);
    /* le reste de la session precedente a bien ete supprime avant */
    CHECK(!fs_exists("build/tmp/session-cache/vieux-jeu"));

    /* mode "extract only" : le cache doit survivre */
    opt.extract_only = 1;
    rc = xib_run(&opt, &ui, &res);
    CHECK(rc == XIB_OK);
    CHECK(res.exe_path[0] != '\0');
    CHECK(fs_exists(res.exe_path));
    printf("    cache conserve (mode test) : %s\n", res.cache_dir);
}

static void test_session_marker(void)
{
    const char *cache_root = "build/tmp/marker-cache";
    cache_t c;

    printf("- marqueur de session (simulation de coupure de courant)\n");

    CHECK(cache_init(&c, cache_root, 0) == 0);
    CHECK(cache_mark_session(&c, "hdd:\\Games\\gros-jeu.iso") == 0);
    CHECK(cache_has_session_marker(&c) == 1);

    /* un dossier d'extraction abandonne (console eteinte en plein jeu) */
    cache_ensure_dir("build/tmp/marker-cache/gros-jeu/media");
    {
        FILE *f = fopen("build/tmp/marker-cache/gros-jeu/media/big.bin", "wb");
        if (f) {
            fwrite("donnees", 1, 7, f);
            fclose(f);
        }
    }
    CHECK(fs_exists("build/tmp/marker-cache/gros-jeu/media/big.bin"));

    /* "redemarrage de l'appli" : la purge doit tout enlever */
    {
        xib_ui ui;
        memset(&ui, 0, sizeof(ui));
        CHECK(xib_purge(cache_root, &ui) == XIB_OK);
    }
    CHECK(cache_has_session_marker(&c) == 0);
    CHECK(!fs_exists("build/tmp/marker-cache/gros-jeu"));
    CHECK(count_tree(cache_root) == 0);
    CHECK(fs_isdir(cache_root) == 1);
}

static void test_errors(void)
{
    xib_options opt;
    xib_ui ui;
    xib_result res;

    printf("- cas d'erreur\n");

    memset(&opt, 0, sizeof(opt));
    memset(&ui, 0, sizeof(ui));
    opt.cache_root = "build/tmp/err-cache";

    opt.iso_path = "build/fixtures/inexistant.iso";
    CHECK(xib_run(&opt, &ui, &res) == XIB_ERR_OPEN_ISO);

    /* fichier qui n'est pas une ISO */
    {
        FILE *f;
        cache_ensure_dir("build/tmp");
        f = fopen("build/tmp/pas-une.iso", "wb");
        CHECK(f != NULL);
        if (f) {
            fwrite("hello world", 1, 11, f);
            fclose(f);
        }
        opt.iso_path = "build/tmp/pas-une.iso";
        CHECK(xib_run(&opt, &ui, &res) == XIB_ERR_NOT_XISO);
    }

    /* arguments manquants */
    memset(&opt, 0, sizeof(opt));
    CHECK(xib_run(&opt, &ui, &res) == XIB_ERR_BAD_ARGS);
}

int main(int argc, char **argv)
{
    const char *trim = "build/fixtures/game-trim.iso";
    const char *xgd2 = "build/fixtures/game-xgd2.iso";

    printf("== tests xiboot core ==\n");

    test_pathutil();
    test_extract_map();

    if (!fs_exists(trim) || !fs_exists(xgd2)) {
        printf("fixtures manquantes : lance 'make fixtures' d'abord\n");
        return 2;
    }

    test_iso(trim);
    test_iso(xgd2);
    test_iso_offsets(trim);
    test_extract_and_purge(trim);
    test_session(xgd2);
    test_session_marker();
    test_errors();

    printf("\n%d verifications OK, %d echecs\n", g_ok, g_fail);
    if (argc > 1 && strcmp(argv[1], "-v") == 0)
        printf("mode verbeux\n");
    return g_fail ? 1 : 0;
}
