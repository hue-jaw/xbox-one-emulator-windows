/* launch_xbox.c - demarrage d'un XEX extrait depuis l'image ISO
 *
 * Deux methodes selon le SDK disponible :
 *
 *  1. XLaunchNewImage(path, NULL)  (XDK / xkelib)
 *     La console decharge le XEX courant et boote celui du jeu. C'est la
 *     methode utilisee par les launchers (Dashlaunch, Aurora, FSD...).
 *     En cas de succes cette fonction ne rend JAMAIS la main.
 *
 *  2. XLaunchXex / XeLoadImage + XLaunchNewImage (selon l'en-tete dispo)
 *
 * Consequence importante pour le cache : comme on ne revient pas, le
 * nettoyage ne peut pas se faire "apres" le jeu. C'est la purge au
 * demarrage de l'appli (xib_purge dans main) qui garantit qu'aucun
 * dossier d'extraction ne survit a une session, meme si la console a ete
 * eteinte brutalement.
 */
#include "platform.h"

#include <string.h>

#if defined(_XBOX) || defined(XBOX)
#define XIB_HAVE_XDK 1
#endif

int xboot_launch_xex(const char *exe, const char *cwd)
{
    (void)cwd;

    if (!exe || !exe[0]) {
        /* Pas de cible : on demande au dash de reprendre la main
         * (equivalent d'une sortie propre du titre). */
#ifdef XIB_HAVE_XDK
        XLaunchNewImage(NULL, NULL);
#endif
        return -1;
    }

#ifdef XIB_HAVE_XDK
    /* Certains jeux utilisent des chemins relatifs : on se place dans le
     * dossier extrait avant de lancer. */
    if (cwd && cwd[0])
        SetCurrentDirectoryA(cwd);

#if defined(XIB_USE_XLAUNCHXEX)
    /* Variante exposee par certains SDK/xkelib. */
    return XLaunchXex(exe) ? 0 : -1;
#else
    /* Methode standard : ne revient pas si le lancement reussit. */
    XLaunchNewImage(exe, NULL);
    return -1; /* atteint seulement en cas d'echec */
#endif
#else
    /* Build hote (stub) : on simule un lancement reussi. */
    printf("[stub] XLaunchNewImage(\"%s\")\n", exe);
    return 0;
#endif
}
