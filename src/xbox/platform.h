/* platform.h - point d'entree unique vers l'API Xbox 360
 *
 * Deux modes :
 *   - build console : le XDK (ou xkelib) fournit <xtl.h>, d3d9.h, d3dx9.h
 *   - build "stub"  : -DXBOX_STUB, utilise les en-tetes minimalistes de
 *     src/xbox/stub/ pour verifier la compilation sur PC (make check-xbox)
 */
#ifndef XIB_PLATFORM_H
#define XIB_PLATFORM_H

#ifdef XBOX_STUB
#include "stub/xbox_stub.h"
#else
#include <xtl.h>
#include <d3d9.h>
#include <d3dx9.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Prefixes de volumes FATX utilises par l'appli.
 * "hdd:" = disque dur interne, "usb0:"/"usb1:" = cle USB formatee Xbox,
 * "dvd:" = lecteur (utile pour lancer une image depuis un disque). */
#define XIB_HDD      "hdd:"
#define XIB_USB0     "usb0:"
#define XIB_USB1     "usb1:"
#define XIB_DVD      "dvd:"

/* Dossier de travail de l'appli sur le disque dur. */
#define XIB_APP_DIR      "hdd:\\XIBoot"
#define XIB_CACHE_DIR    "hdd:\\XIBoot\\cache"
#define XIB_GAMES_DIR    "hdd:\\Games"

/* Lance un XEX. Ne revient pas en cas de succes : le titre appelant est
 * remplace en memoire par le jeu. Retourne -1 en cas d'echec. */
int xboot_launch_xex(const char *exe, const char *cwd);

#endif /* XIB_PLATFORM_H */
