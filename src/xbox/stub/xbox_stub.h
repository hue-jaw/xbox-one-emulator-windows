/* xbox_stub.h - en-tetes minimalistes pour verifier la compilation du code
 * console sur PC (make check-xbox -DXBOX_STUB).
 *
 * Ce fichier NE FAIT PAS partie du build console : il ne remplace pas le
 * XDK, il permet juste au compilateur de valider les types et les appels.
 */
#ifndef XIB_XBOX_STUB_H
#define XIB_XBOX_STUB_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef int BOOL;
typedef unsigned long DWORD;
typedef unsigned int UINT;
typedef long HRESULT;
typedef void *HANDLE;
typedef intptr_t LONG_PTR;
typedef long long LONGLONG;
typedef unsigned long ULONG;

#define TRUE 1
#define FALSE 0
#define SUCCEEDED(hr) ((hr) >= 0)
#define FAILED(hr) ((hr) < 0)
#define S_OK 0L

typedef union _LARGE_INTEGER {
    struct {
        DWORD LowPart;
        long HighPart;
    };
    long long QuadPart;
} LARGE_INTEGER, *PLARGE_INTEGER;

typedef union _ULARGE_INTEGER {
    struct {
        DWORD LowPart;
        DWORD HighPart;
    };
    unsigned long long QuadPart;
} ULARGE_INTEGER;

#define INVALID_HANDLE_VALUE ((HANDLE)(intptr_t)-1)

#define FILE_ATTRIBUTE_DIRECTORY 0x00000010u
#define FILE_ATTRIBUTE_NORMAL    0x00000080u
#define GENERIC_READ             0x80000000u
#define FILE_SHARE_READ          0x00000001u
#define OPEN_EXISTING            3u
#define FILE_BEGIN               0u
#define ERROR_SUCCESS            0u

#define D3D_SDK_VERSION 32
#define D3DADAPTER_DEFAULT 0
#define D3DDEVTYPE_HAL 1
#define D3DCREATE_HARDWARE_VERTEXPROCESSING 0x00000040u
#define D3DFMT_A8R8G8B8 21
#define D3DMULTISAMPLE_NONE 0
#define D3DSWAPEFFECT_DISCARD 1
#define D3DPRESENT_INTERVAL_ONE 0x00000001u
#define D3DCLEAR_TARGET 0x00000001u
#define D3DCOLOR unsigned int

#define FW_BOLD 700
#define FW_NORMAL 400
#define DEFAULT_CHARSET 1
#define OUT_TT_ONLY_PRECIS 7
#define ANTIALIASED_QUALITY 4
#define FF_DONTCARE 0
#define DT_LEFT 0x00000000u
#define DT_NOCLIP 0x00000100u

#define XINPUT_GAMEPAD_DPAD_UP 0x0001
#define XINPUT_GAMEPAD_DPAD_DOWN 0x0002
#define XINPUT_GAMEPAD_DPAD_LEFT 0x0004
#define XINPUT_GAMEPAD_DPAD_RIGHT 0x0008
#define XINPUT_GAMEPAD_START 0x0010
#define XINPUT_GAMEPAD_BACK 0x0020
#define XINPUT_GAMEPAD_A 0x1000
#define XINPUT_GAMEPAD_B 0x2000
#define XINPUT_GAMEPAD_X 0x4000
#define XINPUT_GAMEPAD_Y 0x8000

#define PM_REMOVE 1

typedef struct tagRECT {
    long left, top, right, bottom;
} RECT;

typedef struct tagTEXTMETRICA {
    long tmHeight;
    long tmAscent;
} TEXTMETRICA;

typedef struct tagMSG {
    void *hwnd;
    UINT message;
    unsigned long long wParam, lParam;
    DWORD time;
    long pt_x, pt_y;
} MSG;

typedef struct tagWIN32_FIND_DATAA {
    DWORD dwFileAttributes;
    char cFileName[260];
    char cAlternateFileName[14];
} WIN32_FIND_DATAA;

typedef struct {
    DWORD dwSize;
    DWORD dwFlags;
    DWORD dwButtonFlags;
    unsigned char bLeftTrigger;
    unsigned char bRightTrigger;
    short sThumbLX, sThumbLY, sThumbRX, sThumbRY;
    unsigned short wButtons;
} XINPUT_GAMEPAD;

typedef struct {
    DWORD dwPacketNumber;
    XINPUT_GAMEPAD Gamepad;
} XINPUT_STATE;

typedef struct {
    UINT BackBufferWidth;
    UINT BackBufferHeight;
    int BackBufferFormat;
    UINT BackBufferCount;
    int MultiSampleType;
    int SwapEffect;
    DWORD Flags;
    BOOL EnableAutoDepthStencil;
    int PresentationInterval;
    UINT FullScreen_RefreshRateInHz;
} D3DPRESENT_PARAMETERS;

typedef struct D3DXVECTOR2 {
    float x, y;
} D3DXVECTOR2;

#ifdef __cplusplus
extern "C" {
#endif

HANDLE CreateFileA(const char *path, DWORD access, DWORD share, void *sa,
                   DWORD disposition, DWORD flags, HANDLE tmpl);
BOOL GetFileSizeEx(HANDLE h, LARGE_INTEGER *size);
BOOL SetFilePointerEx(HANDLE h, LARGE_INTEGER dist, LARGE_INTEGER *newpos,
                      DWORD method);
BOOL ReadFile(HANDLE h, void *buf, DWORD len, DWORD *done, void *ov);
BOOL CloseHandle(HANDLE h);

BOOL CreateDirectoryA(const char *path, void *sa);
BOOL RemoveDirectoryA(const char *path);
BOOL DeleteFileA(const char *path);
BOOL MoveFileA(const char *from, const char *to);
DWORD GetFileAttributesA(const char *path);
HANDLE FindFirstFileA(const char *pattern, WIN32_FIND_DATAA *data);
BOOL FindNextFileA(HANDLE h, WIN32_FIND_DATAA *data);
BOOL FindClose(HANDLE h);
BOOL GetDiskFreeSpaceExA(const char *path, ULARGE_INTEGER *avail,
                         ULARGE_INTEGER *total, ULARGE_INTEGER *freeb);
BOOL SetCurrentDirectoryA(const char *path);

DWORD GetTickCount(void);
void Sleep(DWORD ms);
BOOL PeekMessage(MSG *msg, void *hwnd, UINT min, UINT max, UINT remove);
void DispatchMessage(MSG *msg);

DWORD XamInputGetState(DWORD user, DWORD flags, XINPUT_STATE *state);
DWORD XLaunchNewImage(const char *path, void *launch_data);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
/* Interface COM minimaliste (le vrai build utilise d3d9.h / d3dx9.h) */
struct IDirect3DDevice9;
struct IDirect3D9 {
    virtual HRESULT CreateDevice(UINT adapter, int type, void *focus,
                                 DWORD flags, D3DPRESENT_PARAMETERS *pp,
                                 IDirect3DDevice9 **dev) = 0;
    virtual ULONG Release(void) = 0;
};
struct IDirect3DDevice9 {
    virtual HRESULT Clear(DWORD count, const void *rects, DWORD flags,
                          D3DCOLOR color, float z, DWORD stencil) = 0;
    virtual HRESULT BeginScene(void) = 0;
    virtual HRESULT EndScene(void) = 0;
    virtual HRESULT Present(const void *src, const void *dst, void *hwnd,
                            const void *dirty) = 0;
    virtual ULONG Release(void) = 0;
};
struct ID3DXFont {
    virtual HRESULT GetTextMetricsA(TEXTMETRICA *tm) = 0;
    virtual int DrawTextA(void *sprite, const char *text, int count, RECT *rect,
                          DWORD format, D3DCOLOR color) = 0;
    virtual ULONG Release(void) = 0;
};
struct ID3DXLine {
    virtual HRESULT SetWidth(float w) = 0;
    virtual HRESULT SetAntialias(BOOL a) = 0;
    virtual HRESULT Begin(void) = 0;
    virtual HRESULT End(void) = 0;
    virtual HRESULT Draw(const D3DXVECTOR2 *pts, DWORD count, D3DCOLOR color) = 0;
    virtual ULONG Release(void) = 0;
};

IDirect3D9 *Direct3DCreate9(UINT sdk);
HRESULT D3DXCreateFontA(IDirect3DDevice9 *dev, int height, UINT width, UINT weight,
                        UINT miplevels, BOOL italic, DWORD charset,
                        DWORD precision, DWORD quality, DWORD pitch,
                        const char *face, ID3DXFont **font);
HRESULT D3DXCreateLine(IDirect3DDevice9 *dev, ID3DXLine **line);
#endif /* __cplusplus */

#endif /* XIB_XBOX_STUB_H */
