#include <window_system/x11_window.h>
#if defined(__linux__) && !defined(__ANDROID__)

#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <io/log.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <dirent.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <linux/input.h>
#include <window_system/os_window.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>
#include <X11/cursorfont.h>
#include <X11/extensions/Xrandr.h>
#include <X11/extensions/Xfixes.h>
#include <sys/inotify.h>
#include <glad/gl.h>
#include <glad/glx.h>

/* GLX extension function pointers */
typedef GLXContext (*PFNGLXCREATECONTEXTATTRIBSARBPROC)(
    Display*, GLXFBConfig, GLXContext, Bool, const int*);
typedef GLXFBConfig* (*PFNGLXCHOOSEFBCONFIGPROC)(Display*, int, const int*, int*);
typedef int (*PFNGLXGETFBCONFIGATTRIBPROC)(Display*, GLXFBConfig, int, int*);
typedef void (*PFNGLXSWAPINTERVALEXTPROC)(Display*, GLXDrawable, int);

static PFNGLXCREATECONTEXTATTRIBSARBPROC glXCreateContextAttribsARB_ptr = NULL;
static PFNGLXCHOOSEFBCONFIGPROC glXChooseFBConfig_ptr = NULL;
static PFNGLXGETFBCONFIGATTRIBPROC glXGetFBConfigAttrib_ptr = NULL;
static PFNGLXSWAPINTERVALEXTPROC glXSwapIntervalEXT_ptr = NULL;

/* evdev gamepad constants */
#define MAX_GAMEPAD_AXES 8
#define MAX_GAMEPAD_BUTTONS 14 /* number of te_gamepad_button entries */

typedef struct te_x11_gamepad {
    int fd;

    int axes[MAX_GAMEPAD_AXES];
    int prev_axes[MAX_GAMEPAD_AXES];

    int buttons[MAX_GAMEPAD_BUTTONS];
    unsigned char prev_buttons[MAX_GAMEPAD_BUTTONS];

    /* axis ranges (min/max from ioctl) */
    struct input_absinfo abs_info[MAX_GAMEPAD_AXES];

    bool connected;
} te_x11_gamepad;

typedef struct te_x11_window {
    Display* display;
    Window window;
    Colormap colormap;
    GLXContext gl_context;
    Atom wm_delete_window;
    Atom wm_protocols;

    int screen;
    int width;
    int height;

    int inotify_fd;
    int inotify_wd;

    /* cursor state */
    int cursor_x;
    int cursor_y;
    bool capture_mouse;

    te_keyboard_modifiers keyboard_mods;
    te_x11_gamepad gamepad;

    /* atoms for fullscreen */
    Atom wm_state;
    Atom wm_state_fullscreen;
} te_x11_window;

static void x11_gamepad_poll(te_os_window* os_window, te_x11_window* x11_window);
static void x11_gamepad_close(te_x11_gamepad* gamepad);
static bool x11_gamepad_find_and_open(te_x11_gamepad* gamepad);
static void x11_handle_key_event(
    te_os_window* os_window, te_x11_window* x11_window, XKeyEvent* key_event, bool is_press,
    bool is_repeat);

static int
x11_error_handler(Display* display, XErrorEvent* event) {
    char error_text[256];
    XGetErrorText(display, event->error_code, error_text, sizeof(error_text));
    log_error_fmt(
        __FILE__, __LINE__, "X11 error: %s (request code: %d)", error_text,
        event->request_code);
    return 0;
}

static void
x11_load_glx_extensions(Display* display, int screen) {
    const char* glx_extensions;

    if (gladLoaderLoadGLX(display, screen) == 0) {
        log_error(__FILE__, __LINE__, "failed to initialize GLX loader");
        abort();
    }

    glx_extensions = glXQueryExtensionsString(display, screen);
    if (glx_extensions == NULL) {
        log_error(__FILE__, __LINE__, "failed to query GLX extensions");
        abort();
    }

    if (strstr(glx_extensions, "GLX_ARB_create_context") != NULL) {
        glXCreateContextAttribsARB_ptr = (PFNGLXCREATECONTEXTATTRIBSARBPROC)glXGetProcAddress(
            (const GLubyte*)"glXCreateContextAttribsARB");
    }

    if (strstr(glx_extensions, "GLX_EXT_swap_control") != NULL) {
        glXSwapIntervalEXT_ptr =
            (PFNGLXSWAPINTERVALEXTPROC)glXGetProcAddress((const GLubyte*)"glXSwapIntervalEXT");
    }

    if (strstr(glx_extensions, "GLX_ARB_create_context_profile") == NULL) {
        log_warn(__FILE__, __LINE__, "GLX_ARB_create_context_profile not supported");
    }

    if (strstr(glx_extensions, "GLX_ARB_create_context_profile") == NULL) {
        log_warn(__FILE__, __LINE__, "GLX_ARB_create_context_profile not supported");
    }

    glXChooseFBConfig_ptr =
        (PFNGLXCHOOSEFBCONFIGPROC)glXGetProcAddress((const GLubyte*)"glXChooseFBConfig");
    glXGetFBConfigAttrib_ptr =
        (PFNGLXGETFBCONFIGATTRIBPROC)glXGetProcAddress((const GLubyte*)"glXGetFBConfigAttrib");
}

static void*
glad_load_proc(const char* name) {
    void* func = (void*)glXGetProcAddress((const GLubyte*)name);
    if (func == NULL) {
        /* try dlsym as fallback */
        func = (void*)glXGetProcAddress((const GLubyte*)name);
    }
    return func;
}

static bool
x11_gamepad_is_gamepad_device(const char* path) {
    unsigned long evbit[((EV_MAX + 1) / (sizeof(unsigned long) * 8)) + 1] = {0};
    unsigned long keybit[((KEY_MAX + 1) / (sizeof(unsigned long) * 8)) + 1] = {0};
    bool has_gamepad_buttons = false;

    int fd = open(path, O_RDONLY | O_NONBLOCK);
    if (fd < 0) {
        return false;
    }

    /* check if device has gamepad capabilities */
    if (ioctl(fd, EVIOCGBIT(0, sizeof(evbit)), evbit) < 0) {
        close(fd);
        return false;
    }

    /* check for axes */
    if (!(evbit[EV_ABS / (sizeof(unsigned long) * 8)]
          & (1UL << (EV_ABS % (sizeof(unsigned long) * 8))))) {
        close(fd);
        return false;
    }

    /* check for buttons */
    if (ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(keybit)), keybit) < 0) {
        close(fd);
        return false;
    }

    if (keybit[BTN_GAMEPAD / (sizeof(unsigned long) * 8)]
        & (1UL << (BTN_GAMEPAD % (sizeof(unsigned long) * 8)))) {
        has_gamepad_buttons = true;
    }

    close(fd);
    return has_gamepad_buttons;
}

static bool
x11_gamepad_find_and_open(te_x11_gamepad* gamepad) {
    static char path[512];
    struct dirent* entry;
    bool found = false;
    DIR* dir = opendir("/dev/input");
    if (dir == NULL) {
        log_warn(__FILE__, __LINE__, "failed to open /dev/input directory");
        return false;
    }

    while ((entry = readdir(dir)) != NULL) {
        if (strncmp(entry->d_name, "event", 5) != 0) {
            continue;
        }

        snprintf(path, sizeof(path), "/dev/input/%s", entry->d_name);

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-conversion"
        if (x11_gamepad_is_gamepad_device(path)) {
            int fd = open(path, O_RDONLY | O_NONBLOCK);
            if (fd < 0) {
                continue;
            }

            char name[128] = "Unknown Gamepad";
            if (ioctl(fd, EVIOCGNAME(sizeof(name)), name) < 0) {
                strncpy(name, "Unknown Gamepad", sizeof(name) - 1);
            }

            memset(gamepad, 0, sizeof(te_x11_gamepad));
            gamepad->fd = fd;
            gamepad->connected = true;

            /* get axis info */
            for (int i = 0; i < MAX_GAMEPAD_AXES; i++) {
                struct input_absinfo abs_info;
                if (ioctl(fd, EVIOCGABS(i), &abs_info) == 0) {
                    gamepad->abs_info[i] = abs_info;
                } else {
                    gamepad->abs_info[i].minimum = -32767;
                    gamepad->abs_info[i].maximum = 32767;
                    gamepad->abs_info[i].flat = 0;
                }
            }

            log_info(__FILE__, __LINE__, "gamepad connected");
            found = true;
            break;
        }
#pragma GCC diagnostic pop
    }

    closedir(dir);
    return found;
}

static void
x11_gamepad_close(te_x11_gamepad* gamepad) {
    if (gamepad->fd >= 0) {
        close(gamepad->fd);
        gamepad->fd = -1;
    }
    gamepad->connected = false;
}

static bool
x11_gamepad_is_trigger_axis(int axis) {
    return axis == ABS_Z || axis == ABS_RZ || axis == ABS_GAS || axis == ABS_BRAKE;
}

static float
x11_gamepad_normalize_axis(te_x11_gamepad* gamepad, int axis, int value) {
    struct input_absinfo* info = &gamepad->abs_info[axis];

    if (x11_gamepad_is_trigger_axis(axis)) {
        float min = (float)info->minimum;
        float max = (float)info->maximum;
        if (max == min) {
            return 0.0f;
        }
        float normalized = ((float)value - min) / (max - min);
        if (normalized < 0.0f)
            normalized = 0.0f;
        if (normalized > 1.0f)
            normalized = 1.0f;
        return normalized;
    }

    float center = (float)(info->maximum + info->minimum) / 2.0f;
    float range = (float)(info->maximum - info->minimum) / 2.0f;

    if (range == 0.0f) {
        return 0.0f;
    }

    float normalized = ((float)value - center) / range;

    if (normalized < -1.0f)
        normalized = -1.0f;
    if (normalized > 1.0f)
        normalized = 1.0f;

    return normalized;
}

static bool
x11_gamepad_axis_in_deadzone(te_x11_gamepad* gamepad, int axis, int value) {
    struct input_absinfo* info = &gamepad->abs_info[axis];

    if (x11_gamepad_is_trigger_axis(axis)) {
        return false;
    }

    float center = (float)(info->maximum + info->minimum) / 2.0f;
    float range = (float)(info->maximum - info->minimum) / 2.0f;

    if (range == 0.0f) {
        return true;
    }

    float normalized = ((float)value - center) / range;
    return fabsf(normalized) < TE_OS_WINDOW_GAMEPAD_AXIS_DEADZONE;
}

static enum te_gamepad_button
x11_gamepad_button_to_engine(int evdev_button) {
    switch (evdev_button) {
        case BTN_SOUTH: return TE_GB_DOWN; /* A / Cross */
        case BTN_EAST: return TE_GB_RIGHT; /* B / Circle */
        case BTN_NORTH: return TE_GB_UP;   /* Y / Triangle */
        case BTN_WEST: return TE_GB_LEFT;  /* X / Square */
        case BTN_START: return TE_GB_START;
        case BTN_SELECT: return TE_GB_BACK;
        case BTN_MODE: return TE_GB_START;
        case BTN_THUMBL: return TE_GB_LEFT_STICK;
        case BTN_THUMBR: return TE_GB_RIGHT_STICK;
        case BTN_TL: return TE_GB_LEFT_SHOULDER;
        case BTN_TR: return TE_GB_RIGHT_SHOULDER;
        case BTN_DPAD_UP: return TE_GB_DPAD_UP;
        case BTN_DPAD_DOWN: return TE_GB_DPAD_DOWN;
        case BTN_DPAD_LEFT: return TE_GB_DPAD_LEFT;
        case BTN_DPAD_RIGHT: return TE_GB_DPAD_RIGHT;
        default: return TE_GB_LEFT;
    }
}

static enum te_gamepad_axis
x11_gamepad_axis_to_engine(int evdev_axis) {
    switch (evdev_axis) {
        case ABS_X: return TE_GA_LEFT_STICK_X;
        case ABS_Y: return TE_GA_LEFT_STICK_Y;
        case ABS_RX: return TE_GA_RIGHT_STICK_X;
        case ABS_RY: return TE_GA_RIGHT_STICK_Y;
        case ABS_Z: return TE_GA_LEFT_TRIGGER;
        case ABS_RZ: return TE_GA_RIGHT_TRIGGER;
        case ABS_GAS: return TE_GA_RIGHT_TRIGGER;
        case ABS_BRAKE: return TE_GA_LEFT_TRIGGER;
        default: return TE_GA_RIGHT_TRIGGER;
    }
}

static void
x11_gamepad_process_event(
    te_os_window* os_window, te_x11_window* x11_window, struct input_event* ev) {
    te_x11_gamepad* gamepad = &x11_window->gamepad;
    te_os_window_callbacks* callbacks = prv_os_window_get_callbacks(os_window);

    if (ev->type == EV_KEY) {
        enum te_gamepad_button engine_button = x11_gamepad_button_to_engine(ev->code);

        bool was_pressed = gamepad->prev_buttons[engine_button] != 0;
        bool now_pressed = ev->value != 0;

        if (!was_pressed && now_pressed) {
            callbacks->on_gamepad_button_pressed(os_window, engine_button);
        } else if (was_pressed && !now_pressed) {
            callbacks->on_gamepad_button_released(os_window, engine_button);
        }

        gamepad->prev_buttons[engine_button] = (unsigned char)(ev->value != 0);
    } else if (ev->type == EV_ABS) {
        int axis = ev->code;
        enum te_gamepad_axis engine_axis = x11_gamepad_axis_to_engine(axis);

        if (engine_axis < 0 || axis >= MAX_GAMEPAD_AXES) {
            return;
        }

        int prev_value = gamepad->prev_axes[axis];
        int new_value = ev->value;
        gamepad->prev_axes[axis] = new_value;

        bool prev_in_deadzone = x11_gamepad_axis_in_deadzone(gamepad, axis, prev_value);
        bool new_in_deadzone = x11_gamepad_axis_in_deadzone(gamepad, axis, new_value);

        if (prev_in_deadzone && new_in_deadzone) {
            return;
        }

        float normalized = x11_gamepad_normalize_axis(gamepad, axis, new_value);

        if (!x11_gamepad_is_trigger_axis(axis)) {
            if (!new_in_deadzone) {
                float deadzone = TE_OS_WINDOW_GAMEPAD_AXIS_DEADZONE;
                if (normalized >= 0.0f) {
                    normalized = (normalized - deadzone) / (1.0f - deadzone);
                    if (normalized < 0.0f)
                        normalized = 0.0f;
                } else {
                    normalized = (normalized + deadzone) / (1.0f - deadzone);
                    if (normalized > 0.0f)
                        normalized = 0.0f;
                }
            } else {
                normalized = 0.0f;
            }
        }

        callbacks->on_gamepad_axis_moved(os_window, engine_axis, normalized);
    }
}

static void
x11_gamepad_poll(te_os_window* os_window, te_x11_window* x11_window) {
    te_x11_gamepad* gamepad = &x11_window->gamepad;
    te_os_window_callbacks* callbacks = prv_os_window_get_callbacks(os_window);

    /* check if was disconnected */
    if (gamepad->connected && gamepad->fd < 0) {
        gamepad->connected = false;
        log_info(__FILE__, __LINE__, "gamepad disconnected");
        callbacks->on_gamepad_disconnected(os_window);
        return;
    }

    /* try to connect */
    if (x11_window->inotify_fd >= 0) {
        static char buf[64];
        ssize_t n = read(x11_window->inotify_fd, buf, sizeof(buf));
        if (n > 0 && !gamepad->connected) {
            if (x11_gamepad_find_and_open(gamepad)) {
                callbacks->on_gamepad_connected(os_window);
            }
        }
    }

    if (!gamepad->connected || gamepad->fd < 0) {
        return;
    }

    /* read events from gamepad */
    struct input_event ev;
    ssize_t bytes_read;
    bool read_error = false;

    while ((bytes_read = read(gamepad->fd, &ev, sizeof(ev))) == sizeof(ev)) {
        if (ev.type == EV_SYN) {
            continue;
        }
        x11_gamepad_process_event(os_window, x11_window, &ev);
    }

    if (bytes_read < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
        read_error = true;
    }

    /* check if disconnected */
    if (read_error) {
        log_info(__FILE__, __LINE__, "gamepad disconnected");
        x11_gamepad_close(gamepad);
        callbacks->on_gamepad_disconnected(os_window);
    }
}

static void
x11_handle_key_event(
    te_os_window* os_window, te_x11_window* x11_window, XKeyEvent* key_event, bool is_press,
    bool is_repeat) {
    enum te_keyboard_button button = (enum te_keyboard_button)(key_event->keycode - 8);

    /* update keyboard modifiers */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-conversion"
    if (button == TE_KB_LEFT_ALT) {
        if (is_press) {
            x11_window->keyboard_mods.bitmask |= 1;
        } else {
            x11_window->keyboard_mods.bitmask &= ~1;
        }
    } else if (button == TE_KB_LEFT_CONTROL) {
        if (is_press) {
            x11_window->keyboard_mods.bitmask |= 2;
        } else {
            x11_window->keyboard_mods.bitmask &= ~2;
        }
    } else if (button == TE_KB_LEFT_SHIFT) {
        if (is_press) {
            x11_window->keyboard_mods.bitmask |= 4;
        } else {
            x11_window->keyboard_mods.bitmask &= ~4;
        }
    }
#pragma GCC diagnostic pop

    te_os_window_callbacks* callbacks = prv_os_window_get_callbacks(os_window);

    if (is_press) {
        callbacks->on_keyboard_button_pressed(
            os_window, button, is_repeat, x11_window->keyboard_mods);
    } else {
        callbacks->on_keyboard_button_released(os_window, button, x11_window->keyboard_mods);
    }
}

static void
x11_process_event(te_os_window* os_window, te_x11_window* x11_window, XEvent* event) {
    te_os_window_callbacks* callbacks = prv_os_window_get_callbacks(os_window);

    switch (event->type) {
        case ClientMessage: {
            if ((Atom)event->xclient.data.l[0] == x11_window->wm_delete_window) {
                os_window_close(os_window);
            }
            break;
        }
        case ConfigureNotify: {
            int new_width = event->xconfigure.width;
            int new_height = event->xconfigure.height;
            if (new_width != x11_window->width || new_height != x11_window->height) {
                x11_window->width = new_width;
                x11_window->height = new_height;
                prv_os_window_on_size_changed(
                    os_window, (unsigned int)new_width, (unsigned int)new_height);
                callbacks->on_resized(
                    os_window, (unsigned int)new_width, (unsigned int)new_height);
            }
            break;
        }
        case FocusIn: {
            callbacks->on_received_focus(os_window);
            break;
        }
        case FocusOut: {
            callbacks->on_lost_focus(os_window);
            break;
        }
        case KeyPress: {
            /* check if this is a repeat by peeking at the next event */
            bool is_repeat = false;
            if (XEventsQueued(x11_window->display, QueuedAfterReading) > 0) {
                XEvent next_event;
                XPeekEvent(x11_window->display, &next_event);
                if (next_event.xkey.keycode == event->xkey.keycode
                    && next_event.xkey.time - event->xkey.time < 10) {
                    is_repeat = true;
                }
            }

            /* send text input */
            char buf[8];
            KeySym keysym;
            int len = XLookupString(&event->xkey, buf, sizeof(buf), &keysym, NULL);
            if (len > 0 && keysym != XK_BackSpace && keysym != XK_Return && keysym != XK_Tab
                && keysym != XK_Escape) {
                callbacks->on_text_input(os_window, buf);
            }

            x11_handle_key_event(os_window, x11_window, &event->xkey, true, is_repeat);
            break;
        }
        case KeyRelease: {
            x11_handle_key_event(os_window, x11_window, &event->xkey, false, false);
            break;
        }
        case ButtonPress: {
            switch (event->xbutton.button) {
                case Button1: callbacks->on_mouse_button_pressed(os_window, TE_MB_LEFT); break;
                case Button2:
                    callbacks->on_mouse_button_pressed(os_window, TE_MB_MIDDLE);
                    break;
                case Button3:
                    callbacks->on_mouse_button_pressed(os_window, TE_MB_RIGHT);
                    break;
                case Button4: callbacks->on_mouse_scroll_moved(os_window, 1.0f); break;
                case Button5: callbacks->on_mouse_scroll_moved(os_window, -1.0f); break;
                case 8: callbacks->on_mouse_button_pressed(os_window, TE_MB_X1); break;
                case 9: callbacks->on_mouse_button_pressed(os_window, TE_MB_X2); break;
            }
            break;
        }
        case ButtonRelease: {
            switch (event->xbutton.button) {
                case Button1:
                    callbacks->on_mouse_button_released(os_window, TE_MB_LEFT);
                    break;
                case Button2:
                    callbacks->on_mouse_button_released(os_window, TE_MB_MIDDLE);
                    break;
                case Button3:
                    callbacks->on_mouse_button_released(os_window, TE_MB_RIGHT);
                    break;
                case 8: callbacks->on_mouse_button_released(os_window, TE_MB_X1); break;
                case 9: callbacks->on_mouse_button_released(os_window, TE_MB_X2); break;
            }
            break;
        }
        case MotionNotify: {
            int x_diff = event->xmotion.x - x11_window->cursor_x;
            int y_diff = event->xmotion.y - x11_window->cursor_y;
            if (x_diff == 0 && y_diff == 0) {
                break;
            }

            if (x11_window->capture_mouse) {
                callbacks->on_mouse_move(os_window, (float)x_diff, (float)y_diff);

                /* teleport cursor back */
                XWarpPointer(
                    x11_window->display, None, x11_window->window, 0, 0, 0, 0,
                    x11_window->cursor_x, x11_window->cursor_y);
            } else {
                x11_window->cursor_x = event->xmotion.x;
                x11_window->cursor_y = event->xmotion.y;

                callbacks->on_mouse_move(os_window, (float)x_diff, (float)y_diff);
            }
            break;
        }
        default: {
            break;
        }
    }
}

void
x11_window_create(te_os_window* os_window, const char* title) {
    XSetErrorHandler(x11_error_handler);

    te_x11_window* x11_window = malloc(sizeof(te_x11_window));
    memset(x11_window, 0, sizeof(te_x11_window));
    prv_os_window_set_impl(os_window, x11_window);

    x11_window->display = XOpenDisplay(NULL);
    if (x11_window->display == NULL) {
        log_error(__FILE__, __LINE__, "failed to open X display");
        abort();
    }

    x11_window->screen = DefaultScreen(x11_window->display);
    Window root = RootWindow(x11_window->display, x11_window->screen);

    x11_load_glx_extensions(x11_window->display, x11_window->screen);

    int visual_attribs[] = {
        GLX_X_RENDERABLE,
        True,
        GLX_DRAWABLE_TYPE,
        GLX_WINDOW_BIT,
        GLX_RENDER_TYPE,
        GLX_RGBA_BIT,
        GLX_X_VISUAL_TYPE,
        GLX_TRUE_COLOR,
        GLX_RED_SIZE,
        8,
        GLX_GREEN_SIZE,
        8,
        GLX_BLUE_SIZE,
        8,
        GLX_ALPHA_SIZE,
        8,
        GLX_DEPTH_SIZE,
        TE_OS_WINDOW_DEPTH_BITS,
        GLX_STENCIL_SIZE,
        TE_OS_WINDOW_STENCIL_BITS,
        GLX_DOUBLEBUFFER,
        True,
        TE_OS_WINDOW_MSAA > 1 ? GLX_SAMPLE_BUFFERS : None,
        TE_OS_WINDOW_MSAA > 1 ? 1 : None,
        TE_OS_WINDOW_MSAA > 1 ? GLX_SAMPLES : None,
        TE_OS_WINDOW_MSAA > 1 ? TE_OS_WINDOW_MSAA : None,
        None};

    int fb_count = 0;
    GLXFBConfig* fb_configs =
        glXChooseFBConfig(x11_window->display, x11_window->screen, visual_attribs, &fb_count);

    if (fb_configs == NULL || fb_count == 0) {
        log_error(__FILE__, __LINE__, "the system failed to meet required pixel format");
        abort();
    }

    /* pick the best FBConfig */
    GLXFBConfig fb_config = fb_configs[0];
    if (TE_OS_WINDOW_MSAA > 1) {
        /* find the FBConfig with exactly required samples (or closest) */
        int best_diff = 0x7FFFFFFF;
        for (int i = 0; i < fb_count; i++) {
            int samples = 0;
            glXGetFBConfigAttrib_ptr(
                x11_window->display, fb_configs[i], GLX_SAMPLES, &samples);
            int diff = abs(samples - TE_OS_WINDOW_MSAA);
            if (diff < best_diff) {
                best_diff = diff;
                fb_config = fb_configs[i];
                if (diff == 0) {
                    break; /* exact match, done */
                }
            }
        }
    } else {
        /* MSAA disabled: pick an FBConfig with no multisampling */
        for (int i = 0; i < fb_count; i++) {
            int samples = 0;
            glXGetFBConfigAttrib_ptr(
                x11_window->display, fb_configs[i], GLX_SAMPLES, &samples);
            if (samples == 0) {
                fb_config = fb_configs[i];
                break;
            }
        }
    }
    XFree(fb_configs);

    XVisualInfo* visual_info = glXGetVisualFromFBConfig(x11_window->display, fb_config);
    if (visual_info == NULL) {
        log_error(__FILE__, __LINE__, "failed to get XVisualInfo from FBConfig");
        abort();
    }

    x11_window->colormap =
        XCreateColormap(x11_window->display, root, visual_info->visual, AllocNone);

    int screen_w = DisplayWidth(x11_window->display, x11_window->screen);
    int screen_h = DisplayHeight(x11_window->display, x11_window->screen);

    /* create window */
    XSetWindowAttributes window_attribs;
    memset(&window_attribs, 0, sizeof(window_attribs));
    window_attribs.colormap = x11_window->colormap;
    window_attribs.background_pixmap = None;
    window_attribs.border_pixel = 0;
    window_attribs.event_mask = ExposureMask | KeyPressMask | KeyReleaseMask | ButtonPressMask
                                | ButtonReleaseMask | PointerMotionMask | StructureNotifyMask
                                | FocusChangeMask | EnterWindowMask | LeaveWindowMask;
    x11_window->window = XCreateWindow(
        x11_window->display, root, 0, 0, (unsigned int)screen_w, (unsigned int)screen_h, 0,
        visual_info->depth, InputOutput, visual_info->visual,
        CWBorderPixel | CWColormap | CWEventMask, &window_attribs);
    if (x11_window->window == 0) {
        log_error(__FILE__, __LINE__, "failed to create X11 window");
        abort();
    }

    XFree(visual_info);

    /* set window title */
    XStoreName(x11_window->display, x11_window->window, title);
    XClassHint* class_hint = XAllocClassHint();
    if (class_hint != NULL) {
        class_hint->res_name = (char*)title;
        class_hint->res_class = (char*)"X11WindowClass";
        XSetClassHint(x11_window->display, x11_window->window, class_hint);
        XFree(class_hint);
    }

    /* set WM protocols for close button */
    x11_window->wm_delete_window = XInternAtom(x11_window->display, "WM_DELETE_WINDOW", False);
    x11_window->wm_protocols = XInternAtom(x11_window->display, "WM_PROTOCOLS", False);
    XSetWMProtocols(x11_window->display, x11_window->window, &x11_window->wm_delete_window, 1);

    /* set fullscreen atoms */
    x11_window->wm_state = XInternAtom(x11_window->display, "_NET_WM_STATE", False);
    x11_window->wm_state_fullscreen =
        XInternAtom(x11_window->display, "_NET_WM_STATE_FULLSCREEN", False);

    /* try to set fullscreen via EWMH */
    if (x11_window->wm_state != None && x11_window->wm_state_fullscreen != None) {
        Atom fullscreen_atoms[] = {x11_window->wm_state_fullscreen};
        XChangeProperty(
            x11_window->display, x11_window->window, x11_window->wm_state, XA_ATOM, 32,
            PropModeReplace, (unsigned char*)fullscreen_atoms, 1);
    }

    XMapWindow(x11_window->display, x11_window->window);
    XFlush(x11_window->display);

    /* create opengl context */
    const int context_attribs[] = {
        GLX_CONTEXT_MAJOR_VERSION_ARB,
        TE_OS_WINDOW_GL_MAJOR_VERSION,
        GLX_CONTEXT_MINOR_VERSION_ARB,
        TE_OS_WINDOW_GL_MINOR_VERSION,
        GLX_CONTEXT_PROFILE_MASK_ARB,
#if !defined(ENGINE_GLES)
        GLX_CONTEXT_CORE_PROFILE_BIT_ARB,
#else
        GLX_CONTEXT_ES2_PROFILE_BIT_EXT,
#endif
#if defined(DEBUG)
        GLX_CONTEXT_FLAGS_ARB,
        GLX_CONTEXT_DEBUG_BIT_ARB,
#endif
        None};

    if (glXCreateContextAttribsARB_ptr != NULL) {
        x11_window->gl_context = glXCreateContextAttribsARB_ptr(
            x11_window->display, fb_config, NULL, True, context_attribs);
    } else {
        /* fallback to legacy context creation */
        x11_window->gl_context =
            glXCreateNewContext(x11_window->display, fb_config, GLX_RGBA_TYPE, NULL, 1);
    }

    if (x11_window->gl_context == NULL) {
        log_error(__FILE__, __LINE__, "failed to create OpenGL context");
        abort();
    }

    if (!glXMakeCurrent(x11_window->display, x11_window->window, x11_window->gl_context)) {
        log_error(__FILE__, __LINE__, "failed to make OpenGL context current");
        abort();
    }

    /* init GLAD */
#if defined(ENGINE_GLES)
    if (gladLoadGLES2(glad_load_proc) == 0) {
#else
    if (gladLoadGL(glad_load_proc) == 0) {
#endif
        log_error(__FILE__, __LINE__, "failed to initialize OpenGL");
        abort();
    }

    /* disable vsync */
    if (glXSwapIntervalEXT_ptr != NULL) {
        glXSwapIntervalEXT_ptr(x11_window->display, x11_window->window, 0);
    }

    /* initialize window state */
    x11_window->width = screen_w;
    x11_window->height = screen_h;
    x11_window->capture_mouse = false;
    x11_window->keyboard_mods.bitmask = 0;

    /* save current cursor pos */
    {
        Window root_return;
        Window child_return;
        int root_x, root_y;
        int win_x, win_y;
        unsigned int mask;

        if (XQueryPointer(
                x11_window->display, x11_window->window, &root_return, &child_return, &root_x,
                &root_y, &win_x, &win_y, &mask)) {
            x11_window->cursor_x = win_x;
            x11_window->cursor_y = win_y;
        }
    }

    /* set initial size */
    prv_os_window_on_size_changed(os_window, (unsigned int)screen_w, (unsigned int)screen_h);

    /* check gamepad */
    x11_window->inotify_fd = -1;
    x11_window->inotify_wd = -1;
    x11_window->inotify_fd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
    if (x11_window->inotify_fd >= 0) {
        x11_window->inotify_wd = inotify_add_watch(
            x11_window->inotify_fd, "/dev/input", IN_CREATE | IN_DELETE | IN_ATTRIB);
    }
    te_x11_gamepad* gamepad = &x11_window->gamepad;
    memset(gamepad, 0, sizeof(te_x11_gamepad));
    gamepad->fd = -1;
    gamepad->connected = false;
    x11_gamepad_find_and_open(gamepad);
}

void
x11_window_destroy(te_os_window* os_window) {
    te_x11_window* x11_window = prv_os_window_get_impl(os_window);

    if (x11_window->inotify_wd >= 0 && x11_window->inotify_fd >= 0) {
        inotify_rm_watch(x11_window->inotify_fd, x11_window->inotify_wd);
        x11_window->inotify_wd = -1;
    }
    if (x11_window->inotify_fd >= 0) {
        close(x11_window->inotify_fd);
        x11_window->inotify_fd = -1;
    }
    x11_gamepad_close(&x11_window->gamepad);

    if (x11_window->display != NULL) {
        glXMakeCurrent(x11_window->display, None, NULL);
    }

    if (x11_window->gl_context != NULL && x11_window->display != NULL) {
        glXDestroyContext(x11_window->display, x11_window->gl_context);
    }

    if (x11_window->display != NULL) {
        if (x11_window->window != 0) {
            XDestroyWindow(x11_window->display, x11_window->window);
        }
        if (x11_window->colormap != 0) {
            XFreeColormap(x11_window->display, x11_window->colormap);
        }
        XCloseDisplay(x11_window->display);
    }

    free(x11_window);
    prv_os_window_set_impl(os_window, NULL);
}

void
x11_window_poll_event(te_os_window* os_window) {
    te_x11_window* x11_window = prv_os_window_get_impl(os_window);

    while (XPending(x11_window->display) > 0) {
        XEvent event;
        XNextEvent(x11_window->display, &event);
        x11_process_event(os_window, x11_window, &event);
    }

    x11_gamepad_poll(os_window, x11_window);
}

void
x11_window_swap_buffers(te_os_window* os_window) {
    te_x11_window* x11_window = prv_os_window_get_impl(os_window);
    glXSwapBuffers(x11_window->display, x11_window->window);
}

void
x11_window_set_cursor_position(te_os_window* os_window, float x, float y) {
    te_x11_window* x11_window = prv_os_window_get_impl(os_window);

    XWarpPointer(x11_window->display, None, x11_window->window, 0, 0, 0, 0, (int)x, (int)y);
    XFlush(x11_window->display);
}

void
x11_window_get_cursor_position(te_os_window* os_window, float* x, float* y) {
    te_x11_window* x11_window = prv_os_window_get_impl(os_window);

    *x = (float)x11_window->cursor_x;
    *y = (float)x11_window->cursor_y;
}

unsigned int
x11_window_get_refresh_rate(te_os_window* os_window) {
    te_x11_window* x11_window = prv_os_window_get_impl(os_window);

    Window root = RootWindow(x11_window->display, x11_window->screen);

    XRRScreenResources* res = XRRGetScreenResourcesCurrent(x11_window->display, root);
    if (res == NULL) {
        return 60;
    }

    unsigned int refresh_rate = 0;

    for (int i = 0; i < res->ncrtc; i++) {
        XRRCrtcInfo* crtc_info = XRRGetCrtcInfo(x11_window->display, res, res->crtcs[i]);
        if (crtc_info == NULL) {
            continue;
        }

        if (crtc_info->mode != None) {
            for (int j = 0; j < res->nmode; j++) {
                if (res->modes[j].id == crtc_info->mode) {
                    XRRModeInfo* mode = &res->modes[j];

                    double v_total = (double)mode->vTotal;

                    if (mode->modeFlags & RR_DoubleScan) {
                        v_total *= 2.0;
                    }
                    if (mode->modeFlags & RR_Interlace) {
                        v_total /= 2.0;
                    }

                    if (mode->hTotal > 0 && v_total > 0) {
                        double rate =
                            (double)mode->dotClock / ((double)mode->hTotal * v_total);
                        refresh_rate = (unsigned int)(rate + 0.5); /* round to nearest */
                    }
                    break;
                }
            }
        }
        XRRFreeCrtcInfo(crtc_info);

        if (refresh_rate > 0) {
            break;
        }
    }

    XRRFreeScreenResources(res);

    if (refresh_rate < 1) {
        refresh_rate = 60;
    }
    return refresh_rate;
}

void
x11_window_capture_mouse_cursor(te_os_window* os_window, bool capture) {
    te_x11_window* x11_window = prv_os_window_get_impl(os_window);

    if (x11_window->capture_mouse == capture) {
        return;
    }
    x11_window->capture_mouse = capture;

    if (capture) {
        XFixesHideCursor(x11_window->display, x11_window->window);
    } else {
        XFixesShowCursor(x11_window->display, x11_window->window);
    }

    XFlush(x11_window->display);
}

bool
x11_window_is_gamepad_connected(te_os_window* os_window) {
    te_x11_window* x11_window = prv_os_window_get_impl(os_window);
    return x11_window->gamepad.connected;
}

#endif
