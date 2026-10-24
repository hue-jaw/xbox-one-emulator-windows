/* pathutil.h - manipulation de chemins + assainissement FATX
 *
 * Portable (C99). Utilise par le coeur, le CLI hote et l'appli console.
 */
#ifndef XIB_PATHUTIL_H
#define XIB_PATHUTIL_H

#include <stddef.h>

#define XIB_MAX_PATH 260

/* Separe un chemin en composants, quel que soit le separateur ('/' ou '\\').
 * Retourne 0 si le composant a ete extrait, -1 en fin de chaine. */
int pu_next_component(const char **cursor, char *out, size_t outsz);

/* Assainit un nom de fichier/dossier pour FATX (Xbox 360).
 * - remplace les caracteres interdits " * / \ : < > ? | et les caracteres
 *   de controle par '_'
 * - tronque a 42 caracteres (limite FATX)
 * - supprime les points/espace finaux
 * - refuse "." et ".." (retourne "_")
 * Retourne la longueur du resultat. */
size_t pu_sanitize(const char *in, char *out, size_t outsz);

/* Concatene a + b avec le separateur de la plateforme.
 * Si b est absolu (commence par '\\' ou '/'), b remplace a.
 * Retourne 0 si OK, -1 si le buffer est trop petit. */
int pu_join(char *out, size_t outsz, const char *a, const char *b);

/* Normalise les separateurs en '\\' (FATX/Windows) dans une chaine en place. */
void pu_to_backslash(char *s);

/* Convertit les separateurs vers ceux de la plateforme courante ('\\' sur
 * Windows/Xbox, '/' ailleurs). A appeler sur les chemins construits a partir
 * de chemins XDVDFS, qui utilisent toujours '/'. */
void pu_native_separators(char *s);

/* Retire le dernier composant d'un chemin (garde le separateur final).
 * "hdd:\\Games\\a.iso" -> "hdd:\\Games\\" */
void pu_dirname(char *path);

/* Retourne un pointeur sur le dernier composant (nom de fichier). */
const char *pu_basename(const char *path);

/* Compare des extensions (insensible a la casse). ext commence par '.'. */
int pu_has_ext(const char *path, const char *ext);

#endif /* XIB_PATHUTIL_H */
