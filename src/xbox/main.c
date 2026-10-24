/* main.c - XIBoot, lanceur d'images ISO pour Xbox 360 modifiee (RGH/JTAG)
 *
 * Principe :
 *   1. au demarrage      -> purge COMPLETE du dossier de cache
 *   2. l'utilisateur choisit une image .iso dans la liste
 *   3. l'ISO est lue en streaming (XDVDFS), le jeu est extrait dans
 *      hdd:\XIBoot\cache\<nom_du_jeu>\ (arborescence "XEX decompressee")
 *   4. default.xex est lance via XLaunchNewImage
 *   5. le jeu demarre ; a la prochaine ouverture de XIBoot (ou au retour
 *      dans l'appli en cas d'echec) le cache est a nouveau purge
 *
 * Aucun fichier extrait ne survit donc a une session, meme si la console
 * est eteinte en cours de jeu.
 */
#include "platform.h"
#include "ui.h"

#include "../core/cache.h"
#include "../core/fs.h"
#include "../core/pathutil.h"
#include "../core/session.h"

#define MAX_GAMES 128
#define MAX_LOG 6

typedef struct {
    char path[XIB_MAX_PATH];
    char label[96];
    uint64_t size;
} game_entry;

typedef enum {
    SCREEN_LIST = 0,
    SCREEN_BUSY,
    SCREEN_MESSAGE
} screen_t;

static game_entry g_games[MAX_GAMES];
static int g_game_count;
static int g_selected;
static int g_scroll;

static char g_cache_root[XIB_MAX_PATH];
static screen_t g_screen = SCREEN_LIST;

static char g_stage_text[256] = "";
static char g_log[MAX_LOG][192];
static int g_log_count;
static int g_progress = -1;
static char g_current_file[192] = "";
static char g_message[512] = "";
static unsigned g_message_color = UI_WHITE;

/* ---------------------------------------------------------------- */
/* Cache                                                            */
/* ---------------------------------------------------------------- */

/* Choisit le volume avec le plus d'espace libre parmi ceux presents. */
static void pick_cache_root(void)
{
    static const char *roots[] = { "hdd:\\XIBoot\\cache", "usb0:\\XIBoot\\cache",
                                   "usb1:\\XIBoot\\cache" };
    unsigned best = 0;
    uint64_t best_free = 0;
    unsigned i;

    strcpy(g_cache_root, roots[0]);
    for (i = 0; i < sizeof(roots) / sizeof(roots[0]); i++) {
        char base[16];
        uint64_t freeb;
        const char *colon = strchr(roots[i], ':');
        size_t n = colon ? (size_t)(colon - roots[i]) + 1 : 0;

        if (n == 0 || n + 2 >= sizeof(base))
            continue;
        memcpy(base, roots[i], n);
        base[n] = '\\';
        base[n + 1] = '\0';

        if (!fs_isdir(base))
            continue;
        freeb = fs_free_space(base);
        if (freeb > best_free) {
            best_free = freeb;
            best = i;
        }
    }
    strcpy(g_cache_root, roots[best]);
}

/* Purge complete : appelee au demarrage, avant chaque lancement et apres
 * un echec. */
static void purge_cache_now(void)
{
    cache_t c;
#ifdef XIB_KEEP_CACHE
    /* mode debug : on garde le cache pour inspection */
    return;
#endif
    if (cache_init(&c, g_cache_root, 0) == 0)
        cache_purge(&c);
}

/* ---------------------------------------------------------------- */
/* Lecture des images                                               */
/* ---------------------------------------------------------------- */

static void add_game(const char *path, uint64_t size)
{
    if (g_game_count >= MAX_GAMES)
        return;

    strncpy(g_games[g_game_count].path, path, XIB_MAX_PATH - 1);
    g_games[g_game_count].path[XIB_MAX_PATH - 1] = '\0';
    pu_sanitize(pu_basename(path), g_games[g_game_count].label,
                sizeof(g_games[g_game_count].label));
    g_games[g_game_count].size = size;
    g_game_count++;
}

static void scan_dir(const char *dir, int depth)
{
    fs_dir *d;
    char name[XIB_MAX_PATH];
    char full[XIB_MAX_PATH];
    int is_dir;

    if (depth > 2)
        return;

    d = fs_opendir(dir);
    if (!d)
        return;

    while (fs_readdir(d, name, sizeof(name), &is_dir) == 1) {
        if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
            continue;
        if (pu_join(full, sizeof(full), dir, name) != 0)
            continue;

        if (is_dir) {
            /* on descend, mais jamais dans le cache */
            if (strstr(full, "XIBoot\\cache"))
                continue;
            scan_dir(full, depth + 1);
        } else if (pu_has_ext(name, ".iso")) {
            add_game(full, 0);
        }
    }
    fs_closedir(d);
}

static void rescan(void)
{
    static const char *roots[] = { "hdd:\\Games", "hdd:\\RGH", "usb0:\\Games",
                                   "usb1:\\Games", "dvd:\\" };
    unsigned i;

    g_game_count = 0;
    for (i = 0; i < sizeof(roots) / sizeof(roots[0]); i++) {
        if (fs_isdir(roots[i]))
            scan_dir(roots[i], 0);
    }

    /* tri alphabetique simple */
    {
        int a, b;
        for (a = 0; a < g_game_count; a++)
            for (b = a + 1; b < g_game_count; b++)
                if (strcmp(g_games[a].label, g_games[b].label) > 0) {
                    game_entry tmp = g_games[a];
                    g_games[a] = g_games[b];
                    g_games[b] = tmp;
                }
    }

    if (g_selected >= g_game_count)
        g_selected = g_game_count > 0 ? g_game_count - 1 : 0;
}

/* ---------------------------------------------------------------- */
/* Callbacks du pipeline (core/session.h)                           */
/* ---------------------------------------------------------------- */

static void draw_busy(void);

static void cb_stage(void *user, xib_stage stage, const char *text)
{
    (void)user;
    (void)stage;
    if (text) {
        strncpy(g_stage_text, text, sizeof(g_stage_text) - 1);
        g_stage_text[sizeof(g_stage_text) - 1] = '\0';
    }
    if (g_screen == SCREEN_BUSY)
        draw_busy();
}

static void log_push(const char *line)
{
    if (g_log_count < MAX_LOG) {
        strncpy(g_log[g_log_count], line, sizeof(g_log[0]) - 1);
        g_log[g_log_count][sizeof(g_log[0]) - 1] = '\0';
        g_log_count++;
    } else {
        int i;
        for (i = 1; i < MAX_LOG; i++)
            memcpy(g_log[i - 1], g_log[i], sizeof(g_log[0]));
        strncpy(g_log[MAX_LOG - 1], line, sizeof(g_log[0]) - 1);
        g_log[MAX_LOG - 1][sizeof(g_log[0]) - 1] = '\0';
    }
}

static void cb_log(void *user, const char *line)
{
    (void)user;
    log_push(line);
}

static void cb_progress(void *user, int permille, const char *current)
{
    (void)user;
    g_progress = permille;
    if (current) {
        strncpy(g_current_file, current, sizeof(g_current_file) - 1);
        g_current_file[sizeof(g_current_file) - 1] = '\0';
    }
    if (g_screen == SCREEN_BUSY)
        draw_busy();
}

static int cb_launch(void *user, const char *exe, const char *cwd)
{
    (void)user;
    /* En cas de succes, on ne revient jamais ici : le XEX du jeu remplace
     * celui de l'appli. */
    return xboot_launch_xex(exe, cwd);
}

/* ---------------------------------------------------------------- */
/* Rendu                                                            */
/* ---------------------------------------------------------------- */

static void draw_header(const char *subtitle)
{
    ui_text(40, 24, UI_ACCENT, "XIBoot");
    ui_text(140, 30, UI_GREY, "ISO -> XEX launcher  |  Xbox 360 RGH/JTAG");
    if (subtitle)
        ui_text(40, 62, UI_WHITE, "%s", subtitle);
    ui_rect(40, 96, 1200, 2, UI_ACCENT);
}

static void human_size(uint64_t bytes, char *out, size_t outsz)
{
    if (bytes >= (1ull << 30))
        snprintf(out, outsz, "%.1f Gio", (double)bytes / (double)(1ull << 30));
    else
        snprintf(out, outsz, "%.0f Mio", (double)bytes / (double)(1ull << 20));
}

static void draw_list(void)
{
    char line[256];
    char sz[32];
    int i;
    uint64_t freeb = fs_free_space(g_cache_root);

    ui_begin();
    draw_header("Choisis un jeu, puis A pour lancer");

    if (g_game_count == 0) {
        ui_text(60, 160, UI_WARN, "Aucune image .iso trouvee.");
        ui_text(60, 200, UI_GREY, "Depose tes jeux dans hdd:\\Games (ou usb0:\\Games).");
        ui_text(60, 236, UI_GREY, "Formats acceptes : ISO XGD2, XGD3 et XISO trimee.");
        ui_text(60, 300, UI_GREY, "X : relire les disques    Y : vider le cache    B : quitter");
        ui_present();
        return;
    }

    /* fenetre de 12 lignes */
    if (g_selected < g_scroll)
        g_scroll = g_selected;
    if (g_selected >= g_scroll + 12)
        g_scroll = g_selected - 11;

    for (i = g_scroll; i < g_game_count && i < g_scroll + 12; i++) {
        int y = 130 + (i - g_scroll) * 42;
        if (i == g_selected) {
            ui_rect(40, y - 6, 1200, 38, 0xFF1E2A38u);
            ui_rect(40, y - 6, 4, 38, UI_ACCENT);
        }
        snprintf(line, sizeof(line), "%s", g_games[i].label);
        ui_text(60, y, i == g_selected ? UI_WHITE : UI_GREY, "%s", line);
        ui_text(1000, y + 2, UI_GREY, "%s", g_games[i].path);
    }

    if (freeb) {
        human_size(freeb, sz, sizeof(sz));
        ui_text(40, 640, UI_GREY, "Espace libre sur le volume de cache : %s", sz);
    }
    ui_text(40, 672, UI_GREY,
            "A : lancer    X : relire    Y : vider le cache    B : quitter");
    ui_present();
}

static void draw_busy(void)
{
    char sz1[32], sz2[32];
    int i;

    ui_begin();
    draw_header(g_stage_text[0] ? g_stage_text : "Preparation...");

    ui_text(60, 140, UI_WHITE, "Jeu : %s", g_games[g_selected].label);

    if (g_progress >= 0) {
        ui_progress(60, 190, 1160, 30, g_progress, UI_ACCENT);
        ui_text(60, 234, UI_GREY, "%d.%d %%", g_progress / 10, g_progress % 10);
    } else {
        ui_text(60, 190, UI_GREY, "Lecture de l'image...");
    }

    if (g_current_file[0])
        ui_text(60, 268, UI_GREY, "Fichier : %.120s", g_current_file);

    (void)sz1;
    (void)sz2;

    for (i = 0; i < g_log_count; i++)
        ui_text(60, 330 + i * 26, UI_GREY, "%s", g_log[i]);

    ui_present();
}

static void draw_message(void)
{
    ui_begin();
    draw_header("Information");
    ui_text(60, 180, g_message_color, "%s", g_message);
    ui_text(60, 620, UI_GREY, "A : continuer");
    ui_present();
}

/* ---------------------------------------------------------------- */
/* Actions                                                          */
/* ---------------------------------------------------------------- */

static void show_message(const char *text, unsigned color)
{
    strncpy(g_message, text, sizeof(g_message) - 1);
    g_message[sizeof(g_message) - 1] = '\0';
    g_message_color = color;
    g_screen = SCREEN_MESSAGE;
    draw_message();
    ui_wait_pad(0);
    g_screen = SCREEN_LIST;
}

static void launch_selected(void)
{
    xib_options opt;
    xib_ui ui;
    xib_result res;
    int rc;

    if (g_selected < 0 || g_selected >= g_game_count)
        return;

    /* Purge de securite avant toute nouvelle extraction. */
    purge_cache_now();

    memset(&ui, 0, sizeof(ui));
    ui.on_stage = cb_stage;
    ui.on_log = cb_log;
    ui.on_progress = cb_progress;
    ui.launch = cb_launch;

    memset(&opt, 0, sizeof(opt));
    opt.iso_path = g_games[g_selected].path;
    opt.cache_root = g_cache_root;
#ifdef XIB_KEEP_CACHE
    opt.keep_cache = 1;
#endif

    g_screen = SCREEN_BUSY;
    g_progress = -1;
    g_log_count = 0;
    g_current_file[0] = '\0';
    g_stage_text[0] = '\0';
    draw_busy();

    rc = xib_run(&opt, &ui, &res);

    /* On n'arrive ici que si le jeu n'a pas demarre (ou si la plateforme
     * a rendu la main) : on nettoie puis on affiche l'erreur. */
    purge_cache_now();
    g_screen = SCREEN_LIST;

    if (rc != XIB_OK) {
        char msg[512];
        snprintf(msg, sizeof(msg), "%s\n%s", xib_strerror(rc),
                 res.cache_dir[0] ? res.cache_dir : "");
        show_message(msg, UI_ERR);
    } else {
        show_message("Le jeu s'est ferme. Le cache a ete supprime.", UI_OK);
    }
}

/* ---------------------------------------------------------------- */

int main(void)
{
    /* 1. Ecran */
    if (ui_init() != 0) {
        /* Pas d'ecran : on reste silencieux, rien d'utile a faire. */
        return 1;
    }

    /* 2. Choix du volume de cache puis purge COMPLETE du cache.
     *    C'est ici que sont effaces les restes d'une session precedente,
     *    y compris apres une coupure de courant. */
    pick_cache_root();
    purge_cache_now();

    /* 3. Inventaire des images */
    rescan();

    /* 4. Boucle principale */
    for (;;) {
        MSG msg;
        unsigned pad;

        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
            DispatchMessage(&msg);

        draw_list();
        pad = ui_poll_pad();

        if (pad & UI_PAD_UP) {
            if (g_selected > 0)
                g_selected--;
        }
        if (pad & UI_PAD_DOWN) {
            if (g_selected + 1 < g_game_count)
                g_selected++;
        }
        if (pad & UI_PAD_X) {
            rescan();
        }
        if (pad & UI_PAD_Y) {
            cache_t c;
            purge_cache_now();
            if (cache_init(&c, g_cache_root, 0) == 0)
                snprintf(g_message, sizeof(g_message),
                         "Cache vide (%llu fichiers supprimes).",
                         (unsigned long long)c.purged_files);
            show_message(g_message, UI_OK);
        }
        if (pad & UI_PAD_A) {
            launch_selected();
        }
        if (pad & UI_PAD_B) {
            purge_cache_now();
            ui_shutdown();
            /* Retour au dashboard : sur 360, relancer le shell equivaut a
             * quitter proprement le titre. */
            xboot_launch_xex(NULL, NULL);
            return 0;
        }

        Sleep(16);
    }
}
