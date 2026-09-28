#include <window_system/win32_window.h>
#if defined(WIN32)
#include <stdlib.h>
#include <stdbool.h>
#include <io/log.h>
#include <misc/wchar_funcs.h>
#include <window_system/os_window.h>
#define NOMINMAX
#include <Windows.h>
#include <windowsx.h>
#include <glad/glad.h>
#include "wglext.h"

#pragma comment(lib, "opengl32.lib") // for wgl functions

typedef struct te_win32_window {
    HWND hwnd;
    HDC hdc;
    HGLRC hglrc;

    float cached_cursor_x;
    float cached_cursor_y;

    te_keyboard_modifiers keyboard_mods;
    bool capture_mouse;
    bool is_cached_cursor_pos_valid;
} te_win32_window;

static PFNWGLCHOOSEPIXELFORMATARBPROC wglChoosePixelFormatARB = NULL;
static PFNWGLCREATECONTEXTATTRIBSARBPROC wglCreateContextAttribsARB = NULL;
static PFNWGLSWAPINTERVALEXTPROC wglSwapIntervalEXT = NULL;
static const wchar_t* window_class = L"Win32WindowClass";
static HMODULE opengl32dll = NULL;

static void
process_wm_char(te_os_window* os_window, wchar_t ch) {
    if (ch == L'\b' || ch == L'\r' || ch == L'\t' || ch == L'\x1B') {
        return;
    }

    static char utf8[8];
    int n = WideCharToMultiByte(CP_UTF8, 0, &ch, 1, utf8, sizeof(utf8), NULL, NULL);
    prv_os_window_get_callbacks(os_window)->on_text_input(os_window, &utf8[0]);
}

static LRESULT CALLBACK
wnd_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    te_os_window* os_window = (te_os_window*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    if (os_window != NULL) {
        switch (msg) {
            case WM_INPUT: {
                UINT size = 0;
                GetRawInputData(
                    (HRAWINPUT)lparam, RID_INPUT, NULL, &size, sizeof(RAWINPUTHEADER));
                if (size == 0) {
                    return 0;
                }
                static BYTE buffer[sizeof(RAWINPUT)];
                if (size > sizeof(buffer)) {
                    return 0;
                }
                if (GetRawInputData(
                        (HRAWINPUT)lparam, RID_INPUT, buffer, &size, sizeof(RAWINPUTHEADER))
                    != size) {
                    return 0;
                }

                RAWINPUT* raw = (RAWINPUT*)buffer;
                if (raw->header.dwType == RIM_TYPEMOUSE) {
                    RAWMOUSE* mouse = &raw->data.mouse;
                    te_win32_window* win32_window =
                        (te_win32_window*)prv_os_window_get_impl(os_window);

                    te_os_window_callbacks* callbacks = prv_os_window_get_callbacks(os_window);

                    if ((mouse->usFlags & MOUSE_MOVE_ABSOLUTE) == 0) {
                        win32_window->is_cached_cursor_pos_valid = false;
                        callbacks->on_mouse_move(
                            os_window, (float)mouse->lLastX, (float)mouse->lLastY);
                    }

                    if (mouse->usButtonFlags & RI_MOUSE_LEFT_BUTTON_DOWN) {
                        callbacks->on_mouse_button_pressed(os_window, TE_MB_LEFT);
                    }
                    if (mouse->usButtonFlags & RI_MOUSE_LEFT_BUTTON_UP) {
                        callbacks->on_mouse_button_released(os_window, TE_MB_LEFT);
                    }
                    if (mouse->usButtonFlags & RI_MOUSE_RIGHT_BUTTON_DOWN) {
                        callbacks->on_mouse_button_pressed(os_window, TE_MB_RIGHT);
                    }
                    if (mouse->usButtonFlags & RI_MOUSE_RIGHT_BUTTON_UP) {
                        callbacks->on_mouse_button_released(os_window, TE_MB_RIGHT);
                    }
                    if (mouse->usButtonFlags & RI_MOUSE_MIDDLE_BUTTON_DOWN) {
                        callbacks->on_mouse_button_pressed(os_window, TE_MB_MIDDLE);
                    }
                    if (mouse->usButtonFlags & RI_MOUSE_MIDDLE_BUTTON_UP) {
                        callbacks->on_mouse_button_released(os_window, TE_MB_MIDDLE);
                    }
                    if (mouse->usButtonFlags & RI_MOUSE_BUTTON_4_DOWN) {
                        callbacks->on_mouse_button_pressed(os_window, TE_MB_X1);
                    }
                    if (mouse->usButtonFlags & RI_MOUSE_BUTTON_4_UP) {
                        callbacks->on_mouse_button_released(os_window, TE_MB_X1);
                    }
                    if (mouse->usButtonFlags & RI_MOUSE_BUTTON_5_DOWN) {
                        callbacks->on_mouse_button_pressed(os_window, TE_MB_X2);
                    }
                    if (mouse->usButtonFlags & RI_MOUSE_BUTTON_5_UP) {
                        callbacks->on_mouse_button_released(os_window, TE_MB_X2);
                    }

                    if (mouse->usButtonFlags & RI_MOUSE_WHEEL) {
                        callbacks->on_mouse_scroll_moved(
                            os_window, (float)mouse->usButtonData);
                    }

                    return 0;
                }
            }
            case WM_CHAR: {
                process_wm_char(os_window, (wchar_t)wparam);
                break;
            }
            case WM_KEYDOWN: {
                UINT scancode = (lparam >> 16) & 0xFF;
                bool is_extended = (lparam & (1 << 24)) != 0;
                UINT code = is_extended ? (0xE000u | scancode) : scancode;

                bool is_repeat = (lparam & (1 << 30)) != 0;

                // update keyboard modifiers
                te_win32_window* win32_window =
                    (te_win32_window*)prv_os_window_get_impl(os_window);
                enum te_keyboard_button button = (enum te_keyboard_button)(code);
                if (button == TE_KB_LEFT_ALT) {
                    win32_window->keyboard_mods.bitmask |= 0b1;
                } else if (button == TE_KB_LEFT_CONTROL) {
                    win32_window->keyboard_mods.bitmask |= 0b10;
                } else if (button == TE_KB_LEFT_SHIFT) {
                    win32_window->keyboard_mods.bitmask |= 0b100;
                }

                prv_os_window_get_callbacks(os_window)->on_keyboard_button_pressed(
                    os_window, button, is_repeat, win32_window->keyboard_mods);
                break;
            }
            case WM_KEYUP: {
                UINT scancode = (lparam >> 16) & 0xFF;
                bool is_extended = (lparam & (1 << 24)) != 0;
                UINT code = is_extended ? (0xE000u | scancode) : scancode;

                // update keyboard modifiers
                te_win32_window* win32_window =
                    (te_win32_window*)prv_os_window_get_impl(os_window);
                enum te_keyboard_button button = (enum te_keyboard_button)(code);
                if (button == TE_KB_LEFT_ALT) {
                    win32_window->keyboard_mods.bitmask &= ~0b1;
                } else if (button == TE_KB_LEFT_CONTROL) {
                    win32_window->keyboard_mods.bitmask &= ~0b10;
                } else if (button == TE_KB_LEFT_SHIFT) {
                    win32_window->keyboard_mods.bitmask &= ~0b100;
                }

                prv_os_window_get_callbacks(os_window)->on_keyboard_button_released(
                    os_window, button, win32_window->keyboard_mods);
                break;
            }
            case WM_SETFOCUS: {
                prv_os_window_get_callbacks(os_window)->on_received_focus(os_window);
                break;
            }
            case WM_KILLFOCUS: {
                prv_os_window_get_callbacks(os_window)->on_lost_focus(os_window);
                break;
            }
            case WM_SIZE: {
                unsigned int width = (unsigned int)LOWORD(lparam);
                unsigned int height = (unsigned int)HIWORD(lparam);
                prv_os_window_on_size_changed(os_window, width, height);
                prv_os_window_get_callbacks(os_window)->on_resized(os_window, width, height);
                break;
            }
            case WM_CLOSE: {
                os_window_close(os_window);
                PostQuitMessage(0);
                // note: will call on_closed callback outside
                return 0;
            }
        }
    }

    return DefWindowProc(hwnd, msg, wparam, lparam);
}

static void
load_wgl_extensions(void) {
    WNDCLASSW wc = {0};
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = GetModuleHandleA(NULL);
    wc.lpszClassName = L"DummyWindowClass";
    if (!RegisterClassW(&wc)) {
        log_error("failed to register a dummy window class");
        abort();
    }

    HWND dummy = CreateWindowExW(
        0, L"DummyWindowClass", L"", WS_OVERLAPPEDWINDOW, 0, 0, 1, 1, NULL, NULL, wc.hInstance,
        NULL);
    if (!dummy) {
        log_error("failed to create a dummy window");
        abort();
    }
    HDC dummy_dc = GetDC(dummy);

    PIXELFORMATDESCRIPTOR pfd = {
        .nSize = sizeof(pfd),
        .nVersion = 1,
        .dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
        .iPixelType = PFD_TYPE_RGBA,
        .cColorBits = 32,
        .cDepthBits = 24,
        .cStencilBits = 8,
        .iLayerType = PFD_MAIN_PLANE,
    };
    int pf = ChoosePixelFormat(dummy_dc, &pfd);
    SetPixelFormat(dummy_dc, pf, &pfd);

    HGLRC dummy_rc = wglCreateContext(dummy_dc);
    wglMakeCurrent(dummy_dc, dummy_rc);

    wglChoosePixelFormatARB =
        (PFNWGLCHOOSEPIXELFORMATARBPROC)wglGetProcAddress("wglChoosePixelFormatARB");
    if (wglChoosePixelFormatARB == NULL) {
        log_error("failed to load wgl extension wglChoosePixelFormatARB");
        abort();
    }

    wglCreateContextAttribsARB =
        (PFNWGLCREATECONTEXTATTRIBSARBPROC)wglGetProcAddress("wglCreateContextAttribsARB");
    if (wglCreateContextAttribsARB == NULL) {
        log_error("failed to load wgl extension wglCreateContextAttribsARB");
        abort();
    }

    wglSwapIntervalEXT = (PFNWGLSWAPINTERVALEXTPROC)wglGetProcAddress("wglSwapIntervalEXT");
    if (wglSwapIntervalEXT == NULL) {
        log_warn("wglSwapIntervalEXT not supported");
    }

    wglMakeCurrent(NULL, NULL);

    wglDeleteContext(dummy_rc);
    ReleaseDC(dummy, dummy_dc);
    DestroyWindow(dummy);
    UnregisterClassA("DummyWindowClass", wc.hInstance);
}

static void*
gladloadproc(const char* name) {
    void* func = (void*)wglGetProcAddress(name);
    if (func == NULL || (func == (void*)0x1) || (func == (void*)0x2) || (func == (void*)0x3)
        || (func == (void*)-1)) {
        if (opengl32dll == NULL) {
            opengl32dll = LoadLibraryA("opengl32.dll");
        }
        func = (void*)GetProcAddress(opengl32dll, name);
        if (func == NULL) {
            log_error_fmt("failed to find GL function %s", name);
            abort();
        }
    }

    return func;
}

void
win32_window_create(te_os_window* os_window, const char* title) {
    SetProcessDPIAware();
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    load_wgl_extensions();

    te_win32_window* win32_window = malloc(sizeof(te_win32_window));
    memset(win32_window, 0, sizeof(te_win32_window));
    prv_os_window_set_impl(os_window, win32_window);

    HINSTANCE hinstance = GetModuleHandle(NULL);

    WNDCLASSW wc = {0};
    wc.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = wnd_proc;
    wc.hInstance = hinstance;
    wc.hCursor = NULL;
    wc.lpszClassName = window_class;
    if (!RegisterClassW(&wc)) {
        log_error("failed to register window class");
        abort();
    }

    int screen_w = GetSystemMetrics(SM_CXSCREEN);
    int screen_h = GetSystemMetrics(SM_CYSCREEN);

    wchar_t* titlew = wchar_from_char(title, NULL);

    // fullscreen borderless window
    win32_window->hwnd = CreateWindowExW(
        WS_EX_APPWINDOW, window_class, titlew, WS_POPUP | WS_VISIBLE | WS_MAXIMIZE, 0, 0,
        screen_w, screen_h, NULL, NULL, hinstance, NULL);
    if (!win32_window->hwnd) {
        log_error("failed to create window");
        abort();
    }
    free(titlew);

    SetWindowLongPtr(win32_window->hwnd, GWLP_USERDATA, (LONG_PTR)os_window);
    SetWindowPos(
        win32_window->hwnd, HWND_TOP, 0, 0, screen_w, screen_h,
        SWP_FRAMECHANGED | SWP_NOACTIVATE);
    ShowWindow(win32_window->hwnd, SW_SHOWMAXIMIZED);
    SetForegroundWindow(win32_window->hwnd);

    win32_window->hdc = GetDC(win32_window->hwnd);

    const int pixel_attribs[] = {
        WGL_DRAW_TO_WINDOW_ARB,
        GL_TRUE,
        WGL_SUPPORT_OPENGL_ARB,
        GL_TRUE,
        WGL_DOUBLE_BUFFER_ARB,
        GL_TRUE,
        WGL_PIXEL_TYPE_ARB,
        WGL_TYPE_RGBA_ARB,
        WGL_COLOR_BITS_ARB,
        32,
        WGL_DEPTH_BITS_ARB,
        TE_OS_WINDOW_DEPTH_BITS,
        WGL_STENCIL_BITS_ARB,
        TE_OS_WINDOW_STENCIL_BITS,
        TE_OS_WINDOW_MSAA > 1 ? WGL_SAMPLE_BUFFERS_ARB : 0,
        TE_OS_WINDOW_MSAA > 1 ? GL_TRUE : 0,
        TE_OS_WINDOW_MSAA > 1 ? WGL_SAMPLES_ARB : 0,
        TE_OS_WINDOW_MSAA > 1 ? TE_OS_WINDOW_MSAA : 0,
        0};

    int pixel_format;
    UINT num_formats;
    if (!wglChoosePixelFormatARB(
            win32_window->hdc, pixel_attribs, NULL, 1, &pixel_format, &num_formats)
        || num_formats == 0) {
        log_error("the system failed to meet required pixel format");
        abort();
    }

    PIXELFORMATDESCRIPTOR pfd;
    DescribePixelFormat(win32_window->hdc, pixel_format, sizeof(pfd), &pfd);
    if (!SetPixelFormat(win32_window->hdc, pixel_format, &pfd)) {
        log_error("failed to set pixel format");
        abort();
    }

    const int context_attribs[] = {
        WGL_CONTEXT_MAJOR_VERSION_ARB,
        TE_OS_WINDOW_GL_MAJOR_VERSION,
        WGL_CONTEXT_MINOR_VERSION_ARB,
        TE_OS_WINDOW_GL_MINOR_VERSION,
        WGL_CONTEXT_PROFILE_MASK_ARB,
#if !defined(ENGINE_GLES)
        WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
#else
        WGL_CONTEXT_ES2_PROFILE_BIT_EXT,
#endif
        0};

    win32_window->hglrc = wglCreateContextAttribsARB(win32_window->hdc, NULL, context_attribs);
    if (win32_window->hglrc == NULL) {
#if defined(WIN32)
        MessageBoxA(NULL, "Error", "failed to create OpenGL context", MB_OK);
#endif
        log_error("failed to create OpenGL context");
        abort();
    }

    wglMakeCurrent(win32_window->hdc, win32_window->hglrc);

#if defined(ENGINE_GLES)
    if (gladLoadGLES2Loader((GLADloadproc)gladloadproc) == 0) {
#else
    if (gladLoadGLLoader((GLADloadproc)gladloadproc) == 0) {
#endif
#if defined(WIN32)
        MessageBoxA(NULL, "Error", "failed to initialize OpenGL", MB_OK);
#endif
        log_error("failed to initialize OpenGL");
        abort();
    }

    // disable vsync
    if (wglSwapIntervalEXT != NULL) {
        wglSwapIntervalEXT(0);
    }

    RAWINPUTDEVICE rid = {0};
    rid.usUsagePage = 0x01;
    rid.usUsage = 0x02;           // mouse
    rid.dwFlags = RIDEV_NOLEGACY; // suppress WM_MOUSEMOVE etc.
    rid.hwndTarget = win32_window->hwnd;
    RegisterRawInputDevices(&rid, 1, sizeof(rid));

    win32_window->capture_mouse = false;
    win32_window->is_cached_cursor_pos_valid = false;
    win32_window->keyboard_mods.bitmask = 0;

    // should return the actual pixel count
    // not affected by the DPI because we set DPI awareness
    RECT rect;
    GetClientRect(win32_window->hwnd, &rect);
    int width = rect.right - rect.left;
    int height = rect.bottom - rect.top;
    // set initial size
    prv_os_window_on_size_changed(os_window, (unsigned int)width, (unsigned int)height);

    SetCursor(LoadCursor(NULL, IDC_ARROW));
}

void
win32_window_destroy(te_os_window* os_window) {
    te_win32_window* wnd = prv_os_window_get_impl(os_window);
    SetWindowLongPtr(wnd->hwnd, GWLP_USERDATA, (LONG_PTR)NULL);

    wglMakeCurrent(NULL, NULL);
    wglDeleteContext(wnd->hglrc);

    ReleaseDC(wnd->hwnd, wnd->hdc);

    DestroyWindow(wnd->hwnd);
    UnregisterClassW(window_class, GetModuleHandle(NULL));

    free(wnd);
    prv_os_window_set_impl(os_window, NULL);

    if (opengl32dll != NULL) {
        FreeLibrary(opengl32dll);
        opengl32dll = NULL;
    }
}

void
win32_window_poll_event(te_os_window* os_window) {
    te_win32_window* win32_window = prv_os_window_get_impl(os_window);
    win32_window->is_cached_cursor_pos_valid = false;

    MSG msg;
    while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) {
            os_window_should_close(os_window);
            break;
        }
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
}

void
win32_window_set_cursor_position(te_os_window* os_window, float x, float y) {
    te_win32_window* win32_window = prv_os_window_get_impl(os_window);

    POINT p = {(long)x, (long)y};
    ClientToScreen(win32_window->hwnd, &p);
    SetCursorPos(p.x, p.y);
}

void
win32_window_get_cursor_position(te_os_window* os_window, float* x, float* y) {
    te_win32_window* win32_window = prv_os_window_get_impl(os_window);

    if (win32_window->is_cached_cursor_pos_valid) {
        (*x) = win32_window->cached_cursor_x;
        (*y) = win32_window->cached_cursor_y;
    } else {
        POINT p;
        GetCursorPos(&p);
        ScreenToClient(win32_window->hwnd, &p);
        (*x) = (float)p.x;
        (*y) = (float)p.y;

        win32_window->cached_cursor_x = (*x);
        win32_window->cached_cursor_y = (*x);
        win32_window->is_cached_cursor_pos_valid = true;
    }
}

unsigned int
win32_window_get_refresh_rate(te_os_window* os_window) {
    te_win32_window* win32_window = prv_os_window_get_impl(os_window);

    int refresh = GetDeviceCaps(win32_window->hdc, VREFRESH);
    if (refresh < 60) {
        refresh = 60;
    }

    return (unsigned int)refresh;
}

void
win32_window_capture_mouse_cursor(te_os_window* os_window, bool capture) {
    te_win32_window* win32_window = prv_os_window_get_impl(os_window);

    if (win32_window->capture_mouse == capture) {
        return;
    }

    win32_window->capture_mouse = capture;

    static POINT pos_before_lock;

    if (capture) {
        GetCursorPos(&pos_before_lock);

        ClipCursor(&(RECT){0, 0, 1, 1});
        ShowCursor(FALSE);
    } else {
        ClipCursor(NULL);
        ShowCursor(TRUE);

        SetCursorPos(pos_before_lock.x, pos_before_lock.y);
    }
}

void
win32_window_swap_buffers(struct te_os_window* os_window) {
    te_win32_window* win32_window = prv_os_window_get_impl(os_window);

    SwapBuffers(win32_window->hdc);
}

#endif