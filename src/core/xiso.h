/* xiso.h - lecteur d'images Xbox 360 (XGD2 / XGD3 / ISO "trim" XDVDFS)
 *
 * Format : la partition jeu est un volume XDVDFS (ISO9660 simplifie).
 *   - descripteur de volume a partition_base + 0x10000 (secteur 32)
 *       "MICROSOFT*XBOX*MEDIA" (20 octets)
 *       +0x14 u32 : secteur du repertoire racine
 *       +0x18 u32 : taille du repertoire racine
 *   - entrees de repertoire : voir xib_dir_entry ci-dessous, arbre
 *     binaire (left / right) dont les offsets sont en mots de 4 octets,
 *     relatifs au debut de la table du repertoire.
 *
 * Ce module ne fait aucune hypothese sur la taille de l'image : tout est
 * lu en streaming via iosrc_read_at().
 */
#ifndef XIB_XISO_H
#define XIB_XISO_H

#include <stdint.h>
#include <stddef.h>

#include "iosrc.h"

#define XIB_SECTOR_SIZE      2048u
#define XIB_VD_OFFSET        0x10000u          /* 32 * 2048 */
#define XIB_XDVDFS_MAGIC     "MICROSOFT*XBOX*MEDIA"
#define XIB_XDVDFS_MAGIC_LEN 20

/* Attributs XDVDFS */
#define XIB_ATTR_READONLY  0x01
#define XIB_ATTR_HIDDEN    0x02
#define XIB_ATTR_SYSTEM    0x04
#define XIB_ATTR_DIRECTORY 0x10
#define XIB_ATTR_ARCHIVE   0x20
#define XIB_ATTR_NORMAL    0x80

#define XIB_MAX_RELPATH 512

typedef struct {
    uint64_t base;        /* offset de la partition jeu dans le fichier ISO */
    uint32_t root_sector; /* secteur du repertoire racine */
    uint32_t root_size;   /* taille en octets */
    int      entry_rel;   /* 1 = offsets relatifs a l'entree, 0 = a la table */
} xiso_volume;

typedef struct {
    char     rel[512];    /* chemin relatif, separateur '/' */
    char     name[256];   /* nom seul (non assaini) */
    uint32_t sector;      /* secteur de depart (relatif a la partition) */
    uint32_t size;        /* taille en octets */
    uint8_t  attr;
    int      depth;
} xiso_entry;

/* Callback de parcours. Retourner 0 pour continuer, !=0 pour arreter. */
typedef int (*xiso_visit_fn)(void *user, const xiso_entry *e);

typedef struct {
    uint64_t bytes;   /* taille totale des fichiers */
    uint32_t files;
    uint32_t dirs;
    uint64_t base;
} xiso_stats;

/* Cherche la partition jeu : essaie les offsets connus (0, XGD2, XGD3, ...)
 * puis scanne l'image par pas de 2 Kio si necessaire.
 * Retourne 0 si trouve. */
int xiso_probe(iosrc *src, xiso_volume *vol);

/* Variante : force un offset de partition (utile pour les tests / images
 * non standard). Retourne 0 si un XDVDFS valide est present a cet offset. */
int xiso_open_at(iosrc *src, uint64_t base, xiso_volume *vol);

/* Parcourt tout l'arbre. Le callback est appele une fois par entree
 * (fichiers ET dossiers), parents avant enfants. */
int xiso_walk(iosrc *src, const xiso_volume *vol, xiso_visit_fn cb, void *user);

/* Statistiques (parcours complet, sans ecriture). */
int xiso_stat(iosrc *src, const xiso_volume *vol, xiso_stats *st);

/* Recherche un fichier par chemin relatif ('/' ou '\\').
 * Retourne 0 si trouve. */
int xiso_find(iosrc *src, const xiso_volume *vol, const char *relpath,
              xiso_entry *out);

/* Lit `len` octets du fichier `e` a l'offset `off`. */
int xiso_read_file(iosrc *src, const xiso_volume *vol, const xiso_entry *e,
                   uint64_t off, void *buf, size_t len);

const char *xiso_attr_str(uint8_t attr);

#endif /* XIB_XISO_H */
