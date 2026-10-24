/* cli.c - version hote (PC) de xiboot : meme pipeline que la console.
 *
 * Sert a tester le parseur XDVDFS, l'extraction et le cache sans console :
 *   ./xiboot-host info  game.iso
 *   ./xiboot-host iso   game.iso --cache /tmp/xib --keep
 *   ./xiboot-host purge /tmp/xib
 */
#include "../core/cache.h"
#include "../core/extract.h"
#include "../core/fs.h"
#include "../core/iosrc.h"
#include "../core/session.h"
#include "../core/xiso.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void on_stage(void *user, xib_stage stage, const char *text)
{
    (void)user;
    printf("[etape %2d] %s\n", (int)stage, text ? text : "");
}

static void on_progress(void *user, int permille, const char *current)
{
    (void)user;
    printf("\r  %3d.%d%%  %-50.50s", permille / 10, permille % 10,
           current ? current : "");
    fflush(stdout);
    if (permille >= 1000)
        printf("\n");
}

static void on_log(void *user, const char *line)
{
    (void)user;
    printf("  . %s\n", line);
}

/* Lanceur factice : sur console, c'est XLaunchNewImage / XLaunchXex. */
static int on_launch(void *user, const char *exe, const char *cwd)
{
    (void)user;
    printf("  >> LANCEMENT de %s (cwd=%s)\n", exe, cwd);
    printf("  >> (simulation : le jeu est considere comme ferme)\n");
    return 0;
}

static int list_cb(void *user, const xiso_entry *e)
{
    (void)user;
    printf("  %-5s %10u  %s\n", xiso_attr_str(e->attr), e->size, e->rel);
    return 0;
}

static int cmd_info(const char *iso)
{
    iosrc *src;
    xiso_volume vol;
    xiso_stats st;
    char launcher[256];

    src = iosrc_open(iso);
    if (!src) {
        fprintf(stderr, "erreur : impossible d'ouvrir %s\n", iso);
        return 1;
    }
    printf("image : %s (%llu octets)\n", iso, (unsigned long long)iosrc_size(src));
    if (xiso_probe(src, &vol) != 0) {
        fprintf(stderr, "erreur : volume XDVDFS introuvable\n");
        iosrc_close(src);
        return 1;
    }
    printf("partition : base=0x%llX root_sector=%u root_size=%u offsets=%s\n",
           (unsigned long long)vol.base, vol.root_sector, vol.root_size,
           vol.entry_rel ? "relatifs a l'entree" : "relatifs a la table");
    if (xiso_stat(src, &vol, &st) != 0) {
        fprintf(stderr, "erreur : arborescence illisible\n");
        iosrc_close(src);
        return 1;
    }
    printf("contenu   : %u fichiers, %u dossiers, %.2f Mio\n", st.files, st.dirs,
           (double)st.bytes / (1024.0 * 1024.0));
    if (extract_find_launcher(src, &vol, launcher, sizeof(launcher)) == 0)
        printf("lanceur   : %s\n", launcher);
    else
        printf("lanceur   : AUCUN .xex a la racine\n");

    /* listing complet */
    xiso_walk(src, &vol, list_cb, NULL);
    iosrc_close(src);
    return 0;
}

static int cmd_purge(const char *root)
{
    xib_ui ui;
    memset(&ui, 0, sizeof(ui));
    ui.on_stage = on_stage;
    ui.on_log = on_log;
    printf("purge de %s\n", root);
    return xib_purge(root, &ui) == 0 ? 0 : 1;
}

static int cmd_run(const char *iso, const char *cache_root, int keep,
                   int extract_only)
{
    xib_options opt;
    xib_ui ui;
    xib_result res;
    int rc;

    memset(&opt, 0, sizeof(opt));
    opt.iso_path = iso;
    opt.cache_root = cache_root;
    opt.keep_cache = keep;
    opt.extract_only = extract_only;

    memset(&ui, 0, sizeof(ui));
    ui.on_stage = on_stage;
    ui.on_progress = on_progress;
    ui.on_log = on_log;
    ui.launch = on_launch;

    rc = xib_run(&opt, &ui, &res);
    printf("\nresultat  : %d (%s)\n", rc, xib_strerror(rc));
    printf("cache     : %s\n", res.cache_dir);
    printf("exe       : %s\n", res.exe_path);
    printf("purge fin : %s\n", res.purge_ok ? "oui" : "non");
    if (fs_isdir(cache_root))
        printf("cache vide: %s\n", fs_exists(res.cache_dir) ? "NON" : "oui");
    return rc == 0 ? 0 : 1;
}

static void usage(const char *argv0)
{
    printf("usage:\n"
           "  %s info  <image.iso>\n"
           "  %s iso   <image.iso> [--cache <dir>] [--keep] [--extract-only]\n"
           "  %s purge <dir>\n",
           argv0, argv0, argv0);
}

int main(int argc, char **argv)
{
    const char *cache = "/tmp/xiboot-cache";
    int keep = 0, extract_only = 0;
    int i;

    if (argc < 2) {
        usage(argv[0]);
        return 2;
    }

    for (i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--cache") == 0 && i + 1 < argc)
            cache = argv[++i];
        else if (strcmp(argv[i], "--keep") == 0)
            keep = 1;
        else if (strcmp(argv[i], "--extract-only") == 0)
            extract_only = 1;
    }

    if (strcmp(argv[1], "info") == 0 && argc >= 3)
        return cmd_info(argv[2]);
    if (strcmp(argv[1], "iso") == 0 && argc >= 3)
        return cmd_run(argv[2], cache, keep, extract_only);
    if (strcmp(argv[1], "purge") == 0 && argc >= 3)
        return cmd_purge(argv[2]);

    usage(argv[0]);
    return 2;
}
