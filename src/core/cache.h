/* cache.h - gestion du dossier de cache (extraction temporaire)
 *
 * Cycle de vie :
 *  1. au demarrage de l'appli  -> cache_purge() (efface tout reste d'une
 *     session precedente, y compris apres une coupure de courant)
 *  2. a l'extraction           -> cache_game_dir() + cache_ensure_dir()
 *  3. au retour du jeu lance   -> cache_purge_game() puis cache_purge()
 *  4. a l'extinction brutale   -> le point 1 s'en occupe au prochain boot
 */
#ifndef XIB_CACHE_H
#define XIB_CACHE_H

#include <stdint.h>
#include <stddef.h>

#include "pathutil.h"

typedef struct {
    char root[XIB_MAX_PATH];
    int  keep;              /* 1 = ne jamais purger (debug) */
    uint64_t purged_files;
    uint64_t purged_dirs;
    uint64_t purged_bytes;
} cache_t;

/* Cree le dossier racine si besoin. Retourne 0 si OK. */
int cache_init(cache_t *c, const char *root, int keep);

/* Supprime tout le contenu du dossier racine (le dossier lui-meme reste). */
int cache_purge(cache_t *c);

/* Cree (recursivement) root/<nom_assaini> et renvoie le chemin. */
int cache_game_dir(cache_t *c, const char *iso_path, char *out, size_t outsz);

/* mkdir -p */
int cache_ensure_dir(const char *path);

/* Vrai si l'espace libre permet d'ecrire `needed` octets. */
int cache_has_space(const cache_t *c, uint64_t needed);

/* Pose/supprime un marqueur de session (detection d'arret brutal). */
int cache_mark_session(const cache_t *c, const char *iso_path);
int cache_has_session_marker(const cache_t *c);
int cache_clear_session(const cache_t *c);

#endif /* XIB_CACHE_H */
