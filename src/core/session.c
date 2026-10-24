#include "session.h"
#include "cache.h"
#include "extract.h"
#include "fs.h"
#include "iosrc.h"
#include "xiso.h"

#include <stdio.h>
#include <string.h>

static void stage(const xib_ui *ui, xib_stage st, const char *text)
{
    if (ui && ui->on_stage)
        ui->on_stage(ui->user, st, text);
}

static void log_msg(const xib_ui *ui, const char *line)
{
    if (ui && ui->on_log && line)
        ui->on_log(ui->user, line);
}

const char *xib_strerror(int code)
{
    switch (code) {
    case XIB_OK:
        return "OK";
    case XIB_ERR_BAD_ARGS:
        return "arguments invalides";
    case XIB_ERR_OPEN_ISO:
        return "impossible d'ouvrir l'image ISO";
    case XIB_ERR_NOT_XISO:
        return "image non reconnue (pas de volume XDVDFS / ISO non trimee)";
    case XIB_ERR_NO_XEX:
        return "aucun default.xex trouve dans l'image";
    case XIB_ERR_NO_SPACE:
        return "espace libre insuffisant dans le cache";
    case XIB_ERR_EXTRACT:
        return "extraction incomplete";
    case XIB_ERR_LAUNCH:
        return "le jeu n'a pas pu etre lance";
    case XIB_ERR_CACHE:
        return "dossier de cache inutilisable";
    default:
        return "erreur inconnue";
    }
}

int xib_purge(const char *cache_root, const xib_ui *ui)
{
    cache_t c;

    if (!cache_root)
        return XIB_ERR_BAD_ARGS;
    if (cache_init(&c, cache_root, 0) != 0)
        return XIB_ERR_CACHE;

    stage(ui, XIB_STAGE_PURGE, "Nettoyage du cache");
    cache_purge(&c);
    log_msg(ui, "cache purge");
    return XIB_OK;
}

/* Etat partage entre les callbacks d'extraction et l'UI. */
typedef struct {
    const xib_ui *ui;
    int last_permille;
} progress_ctx;

static int progress_cb(void *user, const extract_progress *p)
{
    progress_ctx *pc = (progress_ctx *)user;
    int permille = 0;

    if (!pc || !pc->ui || !pc->ui->on_progress)
        return 0;

    if (p->bytes_total)
        permille = (int)((p->bytes_done * 1000ull) / p->bytes_total);
    if (permille != pc->last_permille) {
        pc->last_permille = permille;
        pc->ui->on_progress(pc->ui->user, permille, p->current);
    }
    return 0;
}

static int xib_run_inner(const xib_options *opt, const xib_ui *ui,
                         xib_result *res)
{
    cache_t cache;
    iosrc *src = NULL;
    xiso_volume vol;
    xiso_stats st;
    extract_job job;
    progress_ctx pc;
    int rc = XIB_OK;
    char msg[512];

    if (!opt || !opt->iso_path || !opt->cache_root || !res)
        return XIB_ERR_BAD_ARGS;

    memset(res, 0, sizeof(*res));

    /* 1. Purge systematique a chaque lancement de l'appli. */
    rc = xib_purge(opt->cache_root, ui);
    if (rc != XIB_OK)
        return rc;
    if (opt->purge_only)
        return XIB_OK;

    /* 2. Cache utilisable ? */
    if (cache_init(&cache, opt->cache_root, 0) != 0)
        return XIB_ERR_CACHE;

    /* 3. Ouverture de l'image. */
    stage(ui, XIB_STAGE_OPEN_ISO, "Ouverture de l'image");
    src = iosrc_open(opt->iso_path);
    if (!src) {
        stage(ui, XIB_STAGE_ERROR, xib_strerror(XIB_ERR_OPEN_ISO));
        return XIB_ERR_OPEN_ISO;
    }
    snprintf(msg, sizeof(msg), "image : %.380s (%llu Mio)", opt->iso_path,
             (unsigned long long)(iosrc_size(src) / (1024ull * 1024ull)));
    log_msg(ui, msg);

    /* 4. Detection de la partition jeu (XGD2/XGD3/trimee). */
    stage(ui, XIB_STAGE_PROBE, "Analyse de la partition jeu");
    if (xiso_probe(src, &vol) != 0) {
        iosrc_close(src);
        stage(ui, XIB_STAGE_ERROR, xib_strerror(XIB_ERR_NOT_XISO));
        return XIB_ERR_NOT_XISO;
    }
    snprintf(msg, sizeof(msg), "partition jeu a 0x%llx (root sector %u)",
             (unsigned long long)vol.base, vol.root_sector);
    log_msg(ui, msg);

    /* 5. Inventaire. */
    stage(ui, XIB_STAGE_SCAN, "Inventaire du disque");
    if (xiso_stat(src, &vol, &st) != 0) {
        iosrc_close(src);
        return XIB_ERR_NOT_XISO;
    }

    /* 6. Binaire a lancer. */
    if (extract_find_launcher(src, &vol, res->launcher_rel,
                              sizeof(res->launcher_rel)) != 0) {
        iosrc_close(src);
        stage(ui, XIB_STAGE_ERROR, xib_strerror(XIB_ERR_NO_XEX));
        return XIB_ERR_NO_XEX;
    }
    snprintf(msg, sizeof(msg), "lanceur : %s", res->launcher_rel);
    log_msg(ui, msg);
    snprintf(msg, sizeof(msg), "%u fichiers, %.1f Mio a extraire",
             (unsigned)st.files, (double)st.bytes / (1024.0 * 1024.0));
    log_msg(ui, msg);

    /* 7. Espace libre. */
    stage(ui, XIB_STAGE_SPACE, "Verification de l'espace libre");
    if (!cache_has_space(&cache, st.bytes)) {
        iosrc_close(src);
        stage(ui, XIB_STAGE_ERROR, xib_strerror(XIB_ERR_NO_SPACE));
        return XIB_ERR_NO_SPACE;
    }

    /* 8. Extraction dans le dossier de cache. */
    stage(ui, XIB_STAGE_EXTRACT, "Extraction du jeu");
    {
        char dir[XIB_MAX_PATH];
        if (cache_game_dir(&cache, opt->iso_path, dir, sizeof(dir)) != 0) {
            iosrc_close(src);
            return XIB_ERR_CACHE;
        }
        strcpy(res->cache_dir, dir);

        memset(&pc, 0, sizeof(pc));
        pc.ui = ui;
        pc.last_permille = -1;

        if (extract_prepare(&job, src, &vol, dir, progress_cb, &pc) != 0) {
            iosrc_close(src);
            return XIB_ERR_EXTRACT;
        }
        res->payload_bytes = job.bytes_total;
        res->payload_files = job.files_total;
        if (extract_all(&job) != 0) {
            iosrc_close(src);
            stage(ui, XIB_STAGE_ERROR, xib_strerror(XIB_ERR_EXTRACT));
            if (ui && ui->on_progress)
                ui->on_progress(ui->user, 1000, NULL);
            return XIB_ERR_EXTRACT;
        }
        if (ui && ui->on_progress)
            ui->on_progress(ui->user, 1000, NULL);

        {
            xiso_entry e;
            memset(&e, 0, sizeof(e));
            if (xiso_find(src, &vol, res->launcher_rel, &e) == 0) {
                char rel[XIB_MAX_PATH];
                if (extract_map_relpath(e.rel, rel, sizeof(rel)) == 0 &&
                    pu_join(res->exe_path, sizeof(res->exe_path), dir, rel) == 0)
                    pu_native_separators(res->exe_path);
            }
        }
    }
    iosrc_close(src);
    src = NULL;

    if (res->exe_path[0] == '\0') {
        char rel[XIB_MAX_PATH];
        if (extract_map_relpath(res->launcher_rel, rel, sizeof(rel)) == 0 &&
            pu_join(res->exe_path, sizeof(res->exe_path), res->cache_dir, rel) == 0)
            pu_native_separators(res->exe_path);
    }

    if (opt->extract_only) {
        stage(ui, XIB_STAGE_DONE, "Extraction terminee (mode test)");
        return XIB_OK;
    }

    /* 9. Lancement. Un marqueur de session permet de detecter un arret
     *    brutal (coupure de courant) au prochain demarrage. */
    stage(ui, XIB_STAGE_LAUNCH, "Lancement du jeu");
    cache_mark_session(&cache, opt->iso_path);

    if (!ui || !ui->launch) {
        log_msg(ui, "aucun lanceur fourni par la plateforme");
        rc = XIB_ERR_LAUNCH;
    } else {
        int lrc = ui->launch(ui->user, res->exe_path, res->cache_dir);
        res->launched = 1;
        res->launch_rc = lrc;
        /*
         * NOTE : quand le jeu tourne, le XEX appelant est en general
         * remplace en memoire ; on ne revient ici que si le lancement a
         * echoue ou si la plateforme a rendu la main. Dans tous les cas
         * le nettoyage ci-dessous + la purge au demarrage couvrent le cas
         * "console eteinte brutalement".
         */
        if (lrc != 0)
            rc = XIB_ERR_LAUNCH;
    }
    cache_clear_session(&cache);

    /* 10. Nettoyage final : le cache ne survit pas a la session. */
    stage(ui, XIB_STAGE_CLEANUP, "Suppression du cache");
    if (opt->keep_cache) {
        log_msg(ui, "cache conserve (option debug)");
        res->purge_ok = 0;
    } else {
        cache_purge(&cache);
        res->purge_ok = 1;
    }

    stage(ui, rc == XIB_OK ? XIB_STAGE_DONE : XIB_STAGE_ERROR,
          rc == XIB_OK ? "Termine" : xib_strerror(rc));
    return rc;
}

int xib_run(const xib_options *opt, const xib_ui *ui, xib_result *res)
{
    int rc = xib_run_inner(opt, ui, res);

    /* Quelle que soit l'erreur (ISO illisible, place insuffisante,
     * extraction interrompue...), aucun fichier extrait ne doit rester. */
    if (rc != XIB_OK && opt && opt->cache_root && !opt->keep_cache)
        xib_purge(opt->cache_root, NULL);

    return rc;
}
