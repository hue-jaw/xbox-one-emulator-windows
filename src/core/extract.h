/* extract.h - extraction d'un volume XDVDFS vers un dossier (format decompresse)
 *
 * Le resultat est l'arborescence du jeu, exactement ce que contient l'ISO
 * une fois "extrait en XEX" : default.xex + media/ + ... C'est ce dossier
 * qui est lance par l'appli, puis supprime.
 */
#ifndef XIB_EXTRACT_H
#define XIB_EXTRACT_H

#include <stdint.h>

#include "pathutil.h"
#include "iosrc.h"
#include "xiso.h"

typedef struct {
    uint64_t bytes_done;
    uint64_t bytes_total;
    uint32_t files_done;
    uint32_t files_total;
    const char *current;   /* fichier en cours (relatif) */
} extract_progress;

/* Retourner 0 pour continuer, !=0 pour annuler l'extraction. */
typedef int (*extract_progress_fn)(void *user, const extract_progress *p);

typedef struct {
    iosrc *src;
    const xiso_volume *vol;
    char root[XIB_MAX_PATH];

    uint64_t bytes_total;
    uint32_t files_total;

    uint32_t files_done;
    uint64_t bytes_done;
    const char *current;

    extract_progress_fn on_progress;
    void *user;
    int cancel;

    int skip_existing;   /* 1 = ne reecrit pas un fichier deja present */
    uint32_t errors;     /* fichiers qui n'ont pas pu etre ecrits */
} extract_job;

/* Calcule la taille totale, verifie l'espace libre, cree la racine.
 * Retourne 0 si OK, -1 sinon. */
int extract_prepare(extract_job *j, iosrc *src, const xiso_volume *vol,
                    const char *root, extract_progress_fn cb, void *user);

/* Extrait tout. Retourne 0 si l'arborescence est complete. */
int extract_all(extract_job *j);

/* Chemin de destination (assaini, relatif a root) pour un chemin XDVDFS. */
int extract_map_relpath(const char *rel, char *out, size_t outsz);

/* Cherche le binaire a lancer. Essaie default.xex puis le premier *.xex
 * a la racine. Retourne 0 et remplit rel si trouve. */
int extract_find_launcher(iosrc *src, const xiso_volume *vol, char *rel,
                          size_t relsz);

#endif /* XIB_EXTRACT_H */
