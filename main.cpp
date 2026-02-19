
// main.cpp
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <windows.h>
#include <windowsx.h>
#include "glad.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>

#include "Application.h"

// ADD: renderer headers
#include "StatefulVectorRenderer.h"
#include "RenderContext.h"

static Application g_app;

// ADD: global renderer instance
static StatefulVectorRenderer g_renderer;

// ------------------------------------------------------------
// OpenGL init
// ------------------------------------------------------------
static void InitOpenGL(HWND hwnd)
{
    HDC hdc = GetDC(hwnd);

    PIXELFORMATDESCRIPTOR pfd{};
    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cDepthBits = 24;
    pfd.cStencilBits = 8;
    pfd.iLayerType = PFD_MAIN_PLANE;

    int pf = ChoosePixelFormat(hdc, &pfd);
    SetPixelFormat(hdc, pf, &pfd);

    HGLRC rc = wglCreateContext(hdc);
    wglMakeCurrent(hdc, rc);

    if (!gladLoadGL())
    {
        MessageBoxA(hwnd, "Failed to load OpenGL via glad.", "Error", MB_OK | MB_ICONERROR);
        std::exit(1);
    }

    // ADD: init renderer once GL is ready
    g_renderer.Init();

    ReleaseDC(hwnd, hdc);
}

// ------------------------------------------------------------
// Resize hook
// ------------------------------------------------------------
static void OnResize(int w, int h)
{
    g_app.OnResize(w, h);
    glViewport(0, 0, w, h);
}

// ------------------------------------------------------------
// Render
// ------------------------------------------------------------
static void RenderFrame(HWND hwnd)
{
    glClearColor(0.07f, 0.07f, 0.08f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // ADD: draw EntityBook through renderer each frame
    RenderContext ctx;
    ctx.projection = g_app.GetProjectionMatrix();
    ctx.view = g_app.GetViewMatrix();
    ctx.model = g_app.GetModelMatrix();

    g_renderer.Redraw(ctx);

    // FIX: don't leak DC handles; GetDC must be released
    HDC hdc = GetDC(hwnd);
    SwapBuffers(hdc);
    ReleaseDC(hwnd, hdc);
}

// ------------------------------------------------------------
// Win32 Proc
// ------------------------------------------------------------

// ------------------------------------------------------------
// Cursor hide/show for client area (robust)
// ------------------------------------------------------------
static bool g_cursorHidden = false;
static bool g_trackingMouseLeave = false;

static void HideSystemCursor()
{
    if (g_cursorHidden)
        return;

    while (ShowCursor(FALSE) >= 0) {}
    g_cursorHidden = true;
}

static void ShowSystemCursor()
{
    if (!g_cursorHidden)
        return;

    while (ShowCursor(TRUE) < 0) {}
    g_cursorHidden = false;
}

static void BeginTrackMouseLeave(HWND hwnd)
{
    if (g_trackingMouseLeave)
        return;

    TRACKMOUSEEVENT tme{};
    tme.cbSize = sizeof(tme);
    tme.dwFlags = TME_LEAVE;
    tme.hwndTrack = hwnd;

    if (TrackMouseEvent(&tme))
        g_trackingMouseLeave = true;
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CREATE:
        InitOpenGL(hwnd);
        return 0;

    case WM_SIZE:
        OnResize((int)LOWORD(lParam), (int)HIWORD(lParam));
        return 0;

    case WM_SETCURSOR:
    {
        if (LOWORD(lParam) == HTCLIENT)
        {
            HideSystemCursor();
            SetCursor(NULL);
            return TRUE;
        }
        else
        {
            ShowSystemCursor();
        }
        break;
    }

    case WM_MOUSEMOVE:
    {
        HideSystemCursor();
        BeginTrackMouseLeave(hwnd);

        const int x = GET_X_LPARAM(lParam);
        const int y = GET_Y_LPARAM(lParam);
        g_app.SetMouseClient(x, y);
        if (g_app.IsMousePanning())
            g_app.UpdateMousePan(x, y);
        if (g_app.IsMarqueeSelecting())
            g_app.UpdateMarqueeDrag(x, y);
        return 0;
    }

    case WM_MOUSELEAVE:
        g_trackingMouseLeave = false;
        ShowSystemCursor();
        return 0;

    case WM_NCMOUSEMOVE:
        g_trackingMouseLeave = false;
        ShowSystemCursor();
        break;

    case WM_KILLFOCUS:
    case WM_ACTIVATEAPP:
        if (msg == WM_KILLFOCUS || wParam == FALSE)
        {
            g_trackingMouseLeave = false;
            ShowSystemCursor();
        }
        break;

    case WM_MOUSEWHEEL:
    {
        POINT p;
        p.x = GET_X_LPARAM(lParam);
        p.y = GET_Y_LPARAM(lParam);
        ScreenToClient(hwnd, &p);

        const int wheelDelta = GET_WHEEL_DELTA_WPARAM(wParam);
        const float step = 1.10f;
        const float zoomFactor = (wheelDelta > 0) ? step : (1.0f / step);

        g_app.ZoomAtClient(p.x, p.y, zoomFactor);
        return 0;
    }

    case WM_LBUTTONDOWN:
    {
        const int x = GET_X_LPARAM(lParam);
        const int y = GET_Y_LPARAM(lParam);
        SetCapture(hwnd);
        g_app.SetMouseClient(x, y);
        g_app.OnLeftDown(hwnd);
        return 0;
    }

    case WM_LBUTTONUP:
        ReleaseCapture();
        g_app.OnLeftUp(hwnd);
        return 0;

    case WM_RBUTTONDOWN:
    {
        const int x = GET_X_LPARAM(lParam);
        const int y = GET_Y_LPARAM(lParam);
        SetCapture(hwnd);
        g_app.BeginMousePan(x, y);
        return 0;
    }

    case WM_RBUTTONUP:
        ReleaseCapture();
        g_app.EndMousePan();
        return 0;

    case WM_KEYDOWN:
    {
        switch (wParam)
        {
        case 'S': g_app.ToggleSelectionMode(); return 0;
        case 'G': g_app.ToggleGrid(); return 0;
        case VK_LEFT:  g_app.PanByPixels(-40, 0); return 0;
        case VK_RIGHT: g_app.PanByPixels(40, 0); return 0;
        case VK_UP:    g_app.PanByPixels(0, -40); return 0;
        case VK_DOWN:  g_app.PanByPixels(0, 40); return 0;
        }
        break;
    }

    case WM_DESTROY:
        ShowSystemCursor();
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

// ------------------------------------------------------------
// Entry (ANSI)
// ------------------------------------------------------------
int WINAPI WinMain(
    _In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE,
    _In_ LPSTR,
    _In_ int nCmdShow)
{
    // Ensure a console is visible even when running as
    // a Windows subsystem app (useful for debug prints).
#if _DEBUG
    AllocConsole();
    freopen("CONOUT$", "w", stdout);
    freopen("CONOUT$", "w", stderr);
#endif

    const char* className = "VDrawDemo13WndClass";

    WNDCLASSEXA wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_OWNDC;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = className;
    RegisterClassExA(&wc);

    const int width = 1280;
    const int height = 720;

    HWND hwnd = CreateWindowExA(
        0,
        className,
        "VectorKernel-Starter",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT,
        width, height,
        nullptr,
        nullptr,
        hInstance,
        nullptr);

    if (!hwnd)
        return 0;

    g_app.Init(width, height);

    // ADD: connect renderer to app's EntityBook after app is initialized
    g_renderer.SetEntityBook(&g_app.GetEntityBook());

    auto last = std::chrono::high_resolution_clock::now();

    MSG msg{};
    while (true)
    {
        while (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT)
                return 0;
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }

        auto now = std::chrono::high_resolution_clock::now();
        std::chrono::duration<float> dt = now - last;
        last = now;

        g_app.Update(dt.count());
        RenderFrame(hwnd);
    }
}





