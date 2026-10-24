/* ui_d3d.c - ecran Xbox 360 : Direct3D 9 + D3DXFont/D3DXLine
 *
 * Resolution de rendu : 1280x720 (upscale par la console). Le XDK fournit
 * l'ecran sans fenetre : CreateDevice(NULL, ...) est la norme sur 360.
 */
#include "platform.h"

extern "C" {
#include "ui.h"
}

#include <stdarg.h>

#ifndef XBOX_STUB

static IDirect3DDevice9 *g_dev;
static ID3DXFont *g_font;
static ID3DXFont *g_font_small;
static ID3DXLine *g_line;
static unsigned g_held;

#define SCR_W 1280
#define SCR_H 720

extern "C" int ui_init(void)
{
    IDirect3D9 *d3d;
    D3DPRESENT_PARAMETERS pp;
    HRESULT hr;

    d3d = Direct3DCreate9(D3D_SDK_VERSION);
    if (!d3d)
        return -1;

    memset(&pp, 0, sizeof(pp));
    pp.BackBufferWidth = SCR_W;
    pp.BackBufferHeight = SCR_H;
    pp.BackBufferFormat = D3DFMT_A8R8G8B8;
    pp.BackBufferCount = 1;
    pp.MultiSampleType = D3DMULTISAMPLE_NONE;
    pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    pp.EnableAutoDepthStencil = FALSE;
    pp.PresentationInterval = D3DPRESENT_INTERVAL_ONE;
    pp.FullScreen_RefreshRateInHz = 60;

    hr = d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, NULL,
                           D3DCREATE_HARDWARE_VERTEXPROCESSING, &pp, &g_dev);
    d3d->Release();
    if (FAILED(hr))
        return -1;

    if (FAILED(D3DXCreateFontA(g_dev, 26, 0, FW_BOLD, 1, FALSE, DEFAULT_CHARSET,
                               OUT_TT_ONLY_PRECIS, ANTIALIASED_QUALITY,
                               FF_DONTCARE, "Arial", &g_font)))
        return -1;
    if (FAILED(D3DXCreateFontA(g_dev, 18, 0, FW_NORMAL, 1, FALSE, DEFAULT_CHARSET,
                               OUT_TT_ONLY_PRECIS, ANTIALIASED_QUALITY,
                               FF_DONTCARE, "Arial", &g_font_small)))
        return -1;
    if (FAILED(D3DXCreateLine(g_dev, &g_line)))
        return -1;

    g_held = 0;
    return 0;
}

extern "C" void ui_shutdown(void)
{
    if (g_line) {
        g_line->Release();
        g_line = NULL;
    }
    if (g_font_small) {
        g_font_small->Release();
        g_font_small = NULL;
    }
    if (g_font) {
        g_font->Release();
        g_font = NULL;
    }
    if (g_dev) {
        g_dev->Release();
        g_dev = NULL;
    }
}

extern "C" void ui_begin(void)
{
    if (!g_dev)
        return;
    g_dev->Clear(0, NULL, D3DCLEAR_TARGET, UI_BG, 1.0f, 0);
    g_dev->BeginScene();
}

extern "C" void ui_present(void)
{
    if (!g_dev)
        return;
    g_dev->EndScene();
    g_dev->Present(NULL, NULL, NULL, NULL);
}

static unsigned measure_height(ID3DXFont *f)
{
    if (!f)
        return 20;
    {
        TEXTMETRICA tm;
        if (FAILED(f->GetTextMetricsA(&tm)))
            return 20;
        return tm.tmHeight;
    }
}

extern "C" void ui_text(int x, int y, unsigned color, const char *fmt, ...)
{
    char buf[1024];
    RECT rc;
    va_list ap;

    if (!g_dev || !g_font)
        return;

    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    rc.left = x;
    rc.top = y;
    rc.right = SCR_W;
    rc.bottom = y + measure_height(g_font) + 4;
    g_font->DrawTextA(NULL, buf, -1, &rc, DT_LEFT | DT_NOCLIP, (D3DCOLOR)color);
}

extern "C" void ui_rect(int x, int y, int w, int h, unsigned color)
{
    if (!g_dev || !g_line || w <= 0 || h <= 0)
        return;

    g_line->SetWidth((float)h);
    g_line->SetAntialias(FALSE);
    g_line->Begin();
    {
        D3DXVECTOR2 pts[2];
        pts[0].x = (float)(x + 1);
        pts[0].y = (float)(y + h / 2);
        pts[1].x = (float)(x + w - 1);
        pts[1].y = (float)(y + h / 2);
        g_line->Draw(pts, 2, (D3DCOLOR)color);
    }
    g_line->End();
}

extern "C" void ui_frame_rect(int x, int y, int w, int h, unsigned color)
{
    ui_rect(x, y, w, 2, color);
    ui_rect(x, y + h - 2, w, 2, color);
    ui_rect(x, y, 2, h, color);
    ui_rect(x + w - 2, y, 2, h, color);
}

extern "C" void ui_progress(int x, int y, int w, int h, int permille, unsigned color)
{
    int fill;

    if (permille < 0)
        permille = 0;
    if (permille > 1000)
        permille = 1000;

    ui_frame_rect(x, y, w, h, UI_GREY);
    fill = (w - 6) * permille / 1000;
    if (fill > 0)
        ui_rect(x + 3, y + 3, fill, h - 6, color);
}

/* Lit la manette 0 et renvoie les boutons qui viennent d'etre enfonces. */
extern "C" unsigned ui_poll_pad(void)
{
    XINPUT_STATE st;
    unsigned now = 0;
    unsigned edge;

    memset(&st, 0, sizeof(st));
    if (XamInputGetState(0, 0, &st) != ERROR_SUCCESS)
        return 0;

    if (st.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_UP)
        now |= UI_PAD_UP;
    if (st.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN)
        now |= UI_PAD_DOWN;
    if (st.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_LEFT)
        now |= UI_PAD_LEFT;
    if (st.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT)
        now |= UI_PAD_RIGHT;
    if (st.Gamepad.wButtons & XINPUT_GAMEPAD_START)
        now |= UI_PAD_START;
    if (st.Gamepad.wButtons & XINPUT_GAMEPAD_BACK)
        now |= UI_PAD_BACK;
    if (st.Gamepad.wButtons & XINPUT_GAMEPAD_A)
        now |= UI_PAD_A;
    if (st.Gamepad.wButtons & XINPUT_GAMEPAD_B)
        now |= UI_PAD_B;
    if (st.Gamepad.wButtons & XINPUT_GAMEPAD_X)
        now |= UI_PAD_X;
    if (st.Gamepad.wButtons & XINPUT_GAMEPAD_Y)
        now |= UI_PAD_Y;

    /* stick gauche -> haut/bas */
    if (st.Gamepad.sThumbLY > 20000)
        now |= UI_PAD_UP;
    if (st.Gamepad.sThumbLY < -20000)
        now |= UI_PAD_DOWN;

    edge = now & ~g_held;
    g_held = now;
    return edge;
}

extern "C" unsigned ui_wait_pad(unsigned timeout_ms)
{
    DWORD start = GetTickCount();
    unsigned edge;
    MSG msg;

    for (;;) {
        /* la pompe a messages est indispensable en homebrew : sans elle,
         * le dash peut considerer le titre comme bloque. */
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
            DispatchMessage(&msg);

        edge = ui_poll_pad();
        if (edge)
            return edge;
        if (timeout_ms && (GetTickCount() - start) > timeout_ms)
            return 0;
        Sleep(16);
    }
}

#else /* XBOX_STUB : rendu texte sur stdout pour les tests de compilation */

#include <stdarg.h>

extern "C" int ui_init(void)
{
    return 0;
}
void ui_shutdown(void) {}
void ui_begin(void) {}
void ui_present(void) {}
extern "C" void ui_text(int x, int y, unsigned color, const char *fmt, ...)
{
    va_list ap;
    (void)x;
    (void)y;
    (void)color;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    printf("\n");
}
extern "C" void ui_rect(int x, int y, int w, int h, unsigned color)
{
    (void)x; (void)y; (void)w; (void)h; (void)color;
}
extern "C" void ui_frame_rect(int x, int y, int w, int h, unsigned color)
{
    (void)x; (void)y; (void)w; (void)h; (void)color;
}
extern "C" void ui_progress(int x, int y, int w, int h, int permille, unsigned color)
{
    (void)x; (void)y; (void)w; (void)h; (void)permille; (void)color;
}
extern "C" unsigned ui_poll_pad(void)
{
    return 0;
}
extern "C" unsigned ui_wait_pad(unsigned timeout_ms)
{
    (void)timeout_ms;
    return UI_PAD_A;
}

#endif /* XBOX_STUB */
