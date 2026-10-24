/* fs.h - couche systeme de fichiers abstraite
 *
 * Deux implementations :
 *   src/host/fs_posix.c  -> opendir/readdir/unlink (tests + CLI PC)
 *   src/xbox/fs_xbox.c   -> FindFirstFileA / DeleteFileA (XDK / xkelib)
 */
#ifndef XIB_FS_H
#define XIB_FS_H

#include <stdint.h>
#include <stddef.h>

typedef struct fs_dir fs_dir;

int  fs_mkdir(const char *path);
int  fs_remove(const char *path);        /* fichier */
int  fs_rmdir(const char *path);         /* dossier vide */
int  fs_exists(const char *path);
int  fs_isdir(const char *path);
int  fs_rename(const char *from, const char *to);

fs_dir *fs_opendir(const char *path);
/* Retourne 1 si une entree a ete lue, 0 si fin, -1 si erreur. */
int  fs_readdir(fs_dir *d, char *name, size_t namesz, int *is_dir);
void fs_closedir(fs_dir *d);

/* Espace libre en octets sur le volume contenant path (0 si inconnu). */
uint64_t fs_free_space(const char *path);

#endif /* XIB_FS_H */
