/* session.h - orchestration : purge -> lecture ISO -> extraction -> lancement
 *
 * Ce fichier est la piece centrale, partagee par :
 *   - l'appli console (src/xbox/main.c) : UI + lancement reel du XEX
 *   - le CLI de test hote (src/host/cli.c) : meme pipeline, sans console
 */
#ifndef XIB_SESSION_H
#define XIB_SESSION_H

#include <stdint.h>

#include "pathutil.h"

typedef enum {
    XIB_STAGE_PURGE = 0,
    XIB_STAGE_OPEN_ISO,
    XIB_STAGE_PROBE,
    XIB_STAGE_SCAN,
    XIB_STAGE_SPACE,
    XIB_STAGE_EXTRACT,
    XIB_STAGE_LAUNCH,
    XIB_STAGE_CLEANUP,
    XIB_STAGE_DONE,
    XIB_STAGE_ERROR
} xib_stage;

typedef enum {
    XIB_OK = 0,
    XIB_ERR_BAD_ARGS = -1,
    XIB_ERR_OPEN_ISO = -2,      /* ISO absente / illisible */
    XIB_ERR_NOT_XISO = -3,      /* pas de volume XDVDFS reconnu */
    XIB_ERR_NO_XEX = -4,        /* aucun .xex a la racine */
    XIB_ERR_NO_SPACE = -5,      /* pas assez d'espace libre */
    XIB_ERR_EXTRACT = -6,       /* extraction incomplete */
    XIB_ERR_LAUNCH = -7,        /* le jeu n'a pas pu demarrer */
    XIB_ERR_CACHE = -8          /* dossier de cache inutilisable */
} xib_error;

/* Interface de presentation. Tous les pointeurs peuvent etre NULL. */
typedef struct {
    void (*on_stage)(void *user, xib_stage stage, const char *text);
    /* bytes_done/bytes_total pour une barre de progression, 0..1000 (promille) */
    void (*on_progress)(void *user, int permille, const char *current);
    void (*on_log)(void *user, const char *line);
    /* Lance exe dans cwd et bloque jusqu'a la fermeture du jeu.
     * Retourne 0 si le jeu s'est termine normalement. */
    int (*launch)(void *user, const char *exe, const char *cwd);
    void *user;
} xib_ui;

typedef struct {
    const char *iso_path;    /* image a lancer */
    const char *cache_root;  /* ex "hdd:\\XIBootCache" */
    int keep_cache;          /* 1 = ne pas purger (debug) */
    int extract_only;        /* 1 = ne pas lancer le jeu */
    int purge_only;          /* 1 = purger et sortir */
} xib_options;

typedef struct {
    char cache_dir[XIB_MAX_PATH];
    char exe_path[XIB_MAX_PATH];
    char launcher_rel[XIB_MAX_PATH];
    uint64_t payload_bytes;
    uint32_t payload_files;
    int      launched;
    int      launch_rc;
    int      purge_ok;
} xib_result;

/* Execute le pipeline complet. Retourne un xib_error. */
int xib_run(const xib_options *opt, const xib_ui *ui, xib_result *res);

/* Purge seule (utilisable au demarrage avant toute UI). */
int xib_purge(const char *cache_root, const xib_ui *ui);

const char *xib_strerror(int code);

#endif /* XIB_SESSION_H */
