/* ui.h - interface de l'ecran (liste des jeux + progression)
 *
 * Implementation : src/xbox/ui_d3d.c (Direct3D 9 + D3DXFont/D3DXLine).
 */
#ifndef XIB_UI_H
#define XIB_UI_H

#ifdef __cplusplus
extern "C" {
#endif

/* Couleurs ARGB */
#define UI_WHITE   0xFFFFFFFFu
#define UI_GREY    0xFF9AA0A6u
#define UI_ACCENT  0xFF00A2FFu
#define UI_BG      0xFF101215u
#define UI_WARN    0xFFFFB300u
#define UI_ERR     0xFFFF5252u
#define UI_OK      0xFF69F0AEu

/* Boutons manettes (masque, aligne sur XINPUT_GAMEPAD_*) */
#define UI_PAD_UP    0x0001u
#define UI_PAD_DOWN  0x0002u
#define UI_PAD_LEFT  0x0004u
#define UI_PAD_RIGHT 0x0008u
#define UI_PAD_START 0x0010u
#define UI_PAD_BACK  0x0020u
#define UI_PAD_A     0x1000u
#define UI_PAD_B     0x2000u
#define UI_PAD_X     0x4000u
#define UI_PAD_Y     0x8000u

int  ui_init(void);
void ui_shutdown(void);

/* Debut / fin de frame */
void ui_begin(void);
void ui_present(void);

void ui_text(int x, int y, unsigned color, const char *fmt, ...);
void ui_rect(int x, int y, int w, int h, unsigned color);
void ui_frame_rect(int x, int y, int w, int h, unsigned color);
void ui_progress(int x, int y, int w, int h, int permille, unsigned color);

/* Lecture des manettes : boutons venant d'etre enfonces (front montant). */
unsigned ui_poll_pad(void);
/* Attente bloquante d'un appui (utilisee entre deux ecrans). */
unsigned ui_wait_pad(unsigned timeout_ms);

#ifdef __cplusplus
}
#endif

#endif /* XIB_UI_H */
