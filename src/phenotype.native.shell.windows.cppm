// Win32 desktop shell driver. Owns HWND creation, the message loop, and
// Win32-to-phenotype input translation while the shared shell owns repaint,
// focus, callbacks, and rendering.

module;

#if defined(_WIN32) && !defined(__wasi__) && !defined(__ANDROID__)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <dwmapi.h>

#include <chrono>
#include <cmath>
#include <concepts>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <string>
#include <utility>

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "dwmapi.lib")

#ifdef DrawText
#undef DrawText
#endif
#endif

export module phenotype.native.shell.windows;

#if defined(_WIN32) && !defined(__wasi__) && !defined(__ANDROID__)
export import phenotype.native.shell;
import phenotype.native.platform;
import phenotype.state;

export namespace phenotype::native {
namespace detail {

struct Win32ShellState {
    HWND hwnd = nullptr;
    NativeSurfaceDescriptor* surface = nullptr;
    native_host* host = nullptr;
    int min_w = 0;
    int min_h = 0;
    int max_w = 0;
    int max_h = 0;
    int aspect_w = 0;
    int aspect_h = 0;
    HCURSOR arrow_cursor = nullptr;
    HCURSOR hand_cursor = nullptr;
    HCURSOR active_cursor = nullptr;
    unsigned int pending_high_surrogate = 0;
    IntegratedTitlebarOptions integrated_titlebar = {};
    bool integrated_chrome = false;
    bool running = false;
};

inline Win32ShellState* g_active_win32_shell = nullptr;
constexpr unsigned int win32_cursor_arrow_id = 32512;
constexpr unsigned int win32_cursor_hand_id = 32649;
constexpr unsigned int win32_icon_application_id = 32512;

inline HCURSOR load_system_cursor(unsigned int id) {
    return LoadCursorW(nullptr, MAKEINTRESOURCEW(id));
}

inline HICON load_system_icon(unsigned int id) {
    return LoadIconW(nullptr, MAKEINTRESOURCEW(id));
}

inline std::wstring utf8_to_wide(char const* text) {
    if (!text || text[0] == '\0')
        return L"";
    int needed = MultiByteToWideChar(CP_UTF8, 0, text, -1, nullptr, 0);
    if (needed <= 0)
        return L"phenotype";
    std::wstring out(static_cast<std::size_t>(needed), L'\0');
    int written = MultiByteToWideChar(CP_UTF8, 0, text, -1, out.data(), needed);
    if (written <= 0)
        return L"phenotype";
    if (!out.empty() && out.back() == L'\0')
        out.pop_back();
    return out;
}

inline float dpi_scale_for_window(HWND hwnd) {
    if (!hwnd)
        return 1.0f;
    UINT dpi = 96;
    if (auto* user32 = GetModuleHandleW(L"user32.dll")) {
        using GetDpiForWindowFn = UINT(WINAPI*)(HWND);
        auto* get_dpi = reinterpret_cast<GetDpiForWindowFn>(
            GetProcAddress(user32, "GetDpiForWindow"));
        if (get_dpi)
            dpi = get_dpi(hwnd);
    }
    float scale = static_cast<float>(dpi) / 96.0f;
    return (scale > 0.0f && std::isfinite(scale)) ? scale : 1.0f;
}

inline int system_metric_for_window(HWND hwnd, int index) {
    UINT dpi = static_cast<UINT>(
        std::lround(static_cast<double>(dpi_scale_for_window(hwnd)) * 96.0));
    if (dpi == 0)
        dpi = 96;
    if (auto* user32 = GetModuleHandleW(L"user32.dll")) {
        using GetSystemMetricsForDpiFn = int(WINAPI*)(int, UINT);
        auto* get_metric = reinterpret_cast<GetSystemMetricsForDpiFn>(
            GetProcAddress(user32, "GetSystemMetricsForDpi"));
        if (get_metric)
            return get_metric(index, dpi);
    }
    return GetSystemMetrics(index);
}

inline int logical_to_device_px(HWND hwnd, float logical, int minimum = 0) {
    int px = static_cast<int>(std::ceil(
        static_cast<double>(logical) * static_cast<double>(dpi_scale_for_window(hwnd))));
    return (px < minimum) ? minimum : px;
}

inline int x_from_lparam(LPARAM lparam) {
    return static_cast<int>(static_cast<short>(LOWORD(lparam)));
}

inline int y_from_lparam(LPARAM lparam) {
    return static_cast<int>(static_cast<short>(HIWORD(lparam)));
}

inline void sync_win32_surface(native_host& host,
                               NativeSurfaceDescriptor& surface,
                               HWND hwnd,
                               bool notify) {
    RECT rect{};
    if (hwnd)
        GetClientRect(hwnd, &rect);
    int width = static_cast<int>(rect.right - rect.left);
    int height = static_cast<int>(rect.bottom - rect.top);
    if (width <= 0)
        width = host.cached_width_px;
    if (height <= 0)
        height = host.cached_height_px;
    float scale = dpi_scale_for_window(hwnd);

    bool changed =
        surface.logical_width != width
        || surface.logical_height != height
        || surface.framebuffer_width != width
        || surface.framebuffer_height != height
        || std::fabs(surface.content_scale - scale) > 0.001f;

    surface.logical_width = width;
    surface.logical_height = height;
    surface.framebuffer_width = width;
    surface.framebuffer_height = height;
    surface.content_scale = scale;
    host.cached_width_px = width;
    host.cached_height_px = height;
    host.cached_content_scale = scale;

    if (changed && notify) {
        notify_viewport_changed(&host, width, height, scale);
        repaint_current();
    }
}

inline int win32_modifiers() {
    int mods = 0;
    if ((GetKeyState(VK_SHIFT) & 0x8000) != 0)
        mods |= static_cast<int>(Modifier::Shift);
    if ((GetKeyState(VK_CONTROL) & 0x8000) != 0)
        mods |= static_cast<int>(Modifier::Control);
    if ((GetKeyState(VK_MENU) & 0x8000) != 0)
        mods |= static_cast<int>(Modifier::Alt);
    if ((GetKeyState(VK_LWIN) & 0x8000) != 0
        || (GetKeyState(VK_RWIN) & 0x8000) != 0)
        mods |= static_cast<int>(Modifier::Super);
    return mods;
}

inline Key win32_key(WPARAM key) {
    switch (key) {
        case VK_TAB: return Key::Tab;
        case VK_BACK: return Key::Backspace;
        case VK_RETURN: return Key::Enter;
        case VK_SPACE: return Key::Space;
        case VK_LEFT: return Key::Left;
        case VK_RIGHT: return Key::Right;
        case VK_UP: return Key::Up;
        case VK_DOWN: return Key::Down;
        case VK_PRIOR: return Key::PageUp;
        case VK_NEXT: return Key::PageDown;
        case VK_HOME: return Key::Home;
        case VK_END: return Key::End;
        case VK_ESCAPE: return Key::Escape;
        case 'A': return Key::A;
        default: return Key::Other;
    }
}

inline MouseButton win32_button(UINT message) {
    switch (message) {
        case WM_RBUTTONDOWN:
        case WM_RBUTTONUP:
            return MouseButton::Right;
        case WM_MBUTTONDOWN:
        case WM_MBUTTONUP:
            return MouseButton::Middle;
        default:
            return MouseButton::Left;
    }
}

inline KeyAction win32_button_action(UINT message) {
    switch (message) {
        case WM_LBUTTONUP:
        case WM_RBUTTONUP:
        case WM_MBUTTONUP:
            return KeyAction::Release;
        default:
            return KeyAction::Press;
    }
}

inline void dispatch_win32_char(Win32ShellState& state, wchar_t unit) {
    auto value = static_cast<unsigned int>(unit);
    if (value >= 0xD800u && value <= 0xDBFFu) {
        state.pending_high_surrogate = value;
        return;
    }
    if (value >= 0xDC00u && value <= 0xDFFFu
        && state.pending_high_surrogate != 0) {
        auto high = state.pending_high_surrogate;
        state.pending_high_surrogate = 0;
        auto codepoint = 0x10000u
            + (((high - 0xD800u) << 10u) | (value - 0xDC00u));
        dispatch_char(codepoint);
        return;
    }
    state.pending_high_surrogate = 0;
    if (value >= 0x20u || value == L'\t')
        dispatch_char(value);
}

inline void apply_window_cursor(Win32ShellState const& state) {
    SetCursor(state.active_cursor ? state.active_cursor : state.arrow_cursor);
}

inline int resize_border_px(HWND hwnd) {
    int frame = system_metric_for_window(hwnd, SM_CXSIZEFRAME);
    int padding = system_metric_for_window(hwnd, SM_CXPADDEDBORDER);
    int border = frame + padding;
    return border > 0 ? border : 8;
}

inline bool win32_sizing_edge_has_top(unsigned int edge) {
    return edge == WMSZ_TOP
        || edge == WMSZ_TOPLEFT
        || edge == WMSZ_TOPRIGHT;
}

inline bool win32_sizing_edge_has_left(unsigned int edge) {
    return edge == WMSZ_LEFT
        || edge == WMSZ_TOPLEFT
        || edge == WMSZ_BOTTOMLEFT;
}

inline bool win32_sizing_edge_uses_width(unsigned int edge) {
    switch (edge) {
        case WMSZ_LEFT:
        case WMSZ_RIGHT:
        case WMSZ_TOPLEFT:
        case WMSZ_TOPRIGHT:
        case WMSZ_BOTTOMLEFT:
        case WMSZ_BOTTOMRIGHT:
            return true;
        case WMSZ_TOP:
        case WMSZ_BOTTOM:
            return false;
        default:
            return true;
    }
}

inline bool adjust_win32_aspect_ratio_rect(RECT& rect,
                                           unsigned int edge,
                                           int aspect_w,
                                           int aspect_h,
                                           int frame_dx,
                                           int frame_dy) {
    if (aspect_w <= 0 || aspect_h <= 0)
        return false;
    int const outer_w = static_cast<int>(rect.right - rect.left);
    int const outer_h = static_cast<int>(rect.bottom - rect.top);
    if (outer_w <= 0 || outer_h <= 0)
        return false;
    if (frame_dx < 0)
        frame_dx = 0;
    if (frame_dy < 0)
        frame_dy = 0;

    if (win32_sizing_edge_uses_width(edge)) {
        int client_w = outer_w - frame_dx;
        if (client_w < 1)
            client_w = 1;
        int client_h = static_cast<int>(std::lround(
            static_cast<double>(client_w) * static_cast<double>(aspect_h)
            / static_cast<double>(aspect_w)));
        if (client_h < 1)
            client_h = 1;
        int const adjusted_outer_h = client_h + frame_dy;
        if (win32_sizing_edge_has_top(edge))
            rect.top = rect.bottom - adjusted_outer_h;
        else
            rect.bottom = rect.top + adjusted_outer_h;
        return true;
    }

    int client_h = outer_h - frame_dy;
    if (client_h < 1)
        client_h = 1;
    int client_w = static_cast<int>(std::lround(
        static_cast<double>(client_h) * static_cast<double>(aspect_w)
        / static_cast<double>(aspect_h)));
    if (client_w < 1)
        client_w = 1;
    int const adjusted_outer_w = client_w + frame_dx;
    if (win32_sizing_edge_has_left(edge))
        rect.left = rect.right - adjusted_outer_w;
    else
        rect.right = rect.left + adjusted_outer_w;
    return true;
}

inline void win32_frame_deltas(HWND hwnd,
                               bool integrated_chrome,
                               int& frame_dx,
                               int& frame_dy) {
    frame_dx = 0;
    frame_dy = 0;
    if (!hwnd || integrated_chrome)
        return;

    auto const style = static_cast<DWORD>(GetWindowLongPtrW(hwnd, GWL_STYLE));
    auto const ex_style = static_cast<DWORD>(GetWindowLongPtrW(hwnd, GWL_EXSTYLE));
    RECT rect{0, 0, 100, 100};
    BOOL adjusted = FALSE;
    if (auto* user32 = GetModuleHandleW(L"user32.dll")) {
        using AdjustForDpiFn = BOOL(WINAPI*)(LPRECT, DWORD, BOOL, DWORD, UINT);
        auto* adjust_for_dpi = reinterpret_cast<AdjustForDpiFn>(
            GetProcAddress(user32, "AdjustWindowRectExForDpi"));
        if (adjust_for_dpi) {
            UINT dpi = static_cast<UINT>(
                std::lround(static_cast<double>(dpi_scale_for_window(hwnd)) * 96.0));
            if (dpi == 0)
                dpi = 96;
            adjusted = adjust_for_dpi(&rect, style, FALSE, ex_style, dpi);
        }
    }
    if (!adjusted)
        adjusted = AdjustWindowRectEx(&rect, style, FALSE, ex_style);
    if (!adjusted)
        return;

    frame_dx = static_cast<int>((rect.right - rect.left) - 100);
    frame_dy = static_cast<int>((rect.bottom - rect.top) - 100);
    if (frame_dx < 0)
        frame_dx = 0;
    if (frame_dy < 0)
        frame_dy = 0;
}

inline bool phenotype_hit_region_at(float x, float y) {
    auto hit = hit_test(x, y, g_app_state.scroll_x, g_app_state.scroll_y);
    return hit.has_value();
}

inline LRESULT hit_test_integrated_chrome(Win32ShellState const& state,
                                          HWND hwnd,
                                          LPARAM lparam) {
    RECT window_rect{};
    if (!GetWindowRect(hwnd, &window_rect))
        return HTCLIENT;

    int const screen_x = x_from_lparam(lparam);
    int const screen_y = y_from_lparam(lparam);
    int const x = screen_x - window_rect.left;
    int const y = screen_y - window_rect.top;
    int const width = window_rect.right - window_rect.left;
    int const height = window_rect.bottom - window_rect.top;
    int const border = resize_border_px(hwnd);

    bool const left = x >= 0 && x < border;
    bool const right = x < width && x >= width - border;
    bool const top = y >= 0 && y < border;
    bool const bottom = y < height && y >= height - border;
    if (top && left) return HTTOPLEFT;
    if (top && right) return HTTOPRIGHT;
    if (bottom && left) return HTBOTTOMLEFT;
    if (bottom && right) return HTBOTTOMRIGHT;
    if (top) return HTTOP;
    if (bottom) return HTBOTTOM;
    if (left) return HTLEFT;
    if (right) return HTRIGHT;

    POINT client_point{screen_x, screen_y};
    ScreenToClient(hwnd, &client_point);
    int const drag_height = logical_to_device_px(
        hwnd, state.integrated_titlebar.drag_region_height, 1);
    if (client_point.y >= 0 && client_point.y < drag_height) {
        int const reserved = logical_to_device_px(
            hwnd, state.integrated_titlebar.trailing_control_reserved_width, 0);
        int const client_width = state.surface
            ? state.surface->logical_width
            : width;
        if (reserved > 0 && client_point.x >= client_width - reserved)
            return HTCLIENT;
        if (phenotype_hit_region_at(
                static_cast<float>(client_point.x),
                static_cast<float>(client_point.y)))
            return HTCLIENT;
        return HTCAPTION;
    }

    return HTCLIENT;
}

inline LRESULT CALLBACK win32_shell_wndproc(HWND hwnd,
                                            UINT message,
                                            WPARAM wparam,
                                            LPARAM lparam) {
    auto* state = reinterpret_cast<Win32ShellState*>(
        GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        auto* create = reinterpret_cast<CREATESTRUCTW*>(lparam);
        state = static_cast<Win32ShellState*>(create->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
    }

    switch (message) {
        case WM_NCCALCSIZE:
            if (state && state->integrated_chrome && wparam != 0)
                return 0;
            break;
        case WM_NCHITTEST:
            if (state && state->integrated_chrome) {
                LRESULT dwm_result = 0;
                if (DwmDefWindowProc(hwnd, message, wparam, lparam, &dwm_result))
                    return dwm_result;
                return hit_test_integrated_chrome(*state, hwnd, lparam);
            }
            break;
        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;
        case WM_DESTROY:
            if (state)
                state->running = false;
            PostQuitMessage(0);
            return 0;
        case WM_GETMINMAXINFO:
            if (state) {
                auto* info = reinterpret_cast<MINMAXINFO*>(lparam);
                if (state->min_w > 0)
                    info->ptMinTrackSize.x = state->min_w;
                if (state->min_h > 0)
                    info->ptMinTrackSize.y = state->min_h;
                if (state->max_w > 0)
                    info->ptMaxTrackSize.x = state->max_w;
                if (state->max_h > 0)
                    info->ptMaxTrackSize.y = state->max_h;
                return 0;
            }
            break;
        case WM_SIZING:
            if (state && state->aspect_w > 0 && state->aspect_h > 0
                && lparam != 0) {
                int frame_dx = 0;
                int frame_dy = 0;
                win32_frame_deltas(
                    hwnd, state->integrated_chrome, frame_dx, frame_dy);
                auto* rect = reinterpret_cast<RECT*>(lparam);
                if (adjust_win32_aspect_ratio_rect(
                        *rect,
                        static_cast<unsigned int>(wparam),
                        state->aspect_w,
                        state->aspect_h,
                        frame_dx,
                        frame_dy)) {
                    return TRUE;
                }
            }
            break;
        case WM_SIZE:
        case WM_DPICHANGED:
            if (state && state->host && state->surface) {
                if (message == WM_DPICHANGED && lparam != 0) {
                    auto* suggested = reinterpret_cast<RECT const*>(lparam);
                    SetWindowPos(hwnd, nullptr,
                                 suggested->left,
                                 suggested->top,
                                 suggested->right - suggested->left,
                                 suggested->bottom - suggested->top,
                                 SWP_NOZORDER | SWP_NOACTIVATE);
                }
                sync_win32_surface(*state->host, *state->surface, hwnd, true);
                return 0;
            }
            break;
        case WM_PAINT:
            if (state && state->host) {
                PAINTSTRUCT ps{};
                BeginPaint(hwnd, &ps);
                repaint_current();
                EndPaint(hwnd, &ps);
                return 0;
            }
            break;
        case WM_SETCURSOR:
            if (state && LOWORD(lparam) == HTCLIENT) {
                apply_window_cursor(*state);
                return TRUE;
            }
            break;
        case WM_MOUSEMOVE:
            if (state) {
                dispatch_cursor_pos(
                    static_cast<float>(x_from_lparam(lparam)),
                    static_cast<float>(y_from_lparam(lparam)));
                return 0;
            }
            break;
        case WM_LBUTTONDOWN:
        case WM_RBUTTONDOWN:
        case WM_MBUTTONDOWN:
        case WM_LBUTTONUP:
        case WM_RBUTTONUP:
        case WM_MBUTTONUP:
            if (state) {
                auto action = win32_button_action(message);
                if (action == KeyAction::Press)
                    SetCapture(hwnd);
                else
                    ReleaseCapture();
                dispatch_mouse_button(
                    static_cast<float>(x_from_lparam(lparam)),
                    static_cast<float>(y_from_lparam(lparam)),
                    win32_button(message),
                    action,
                    win32_modifiers());
                return 0;
            }
            break;
        case WM_MOUSEWHEEL:
        case WM_MOUSEHWHEEL:
            if (state && state->surface) {
                POINT point{
                    static_cast<LONG>(x_from_lparam(lparam)),
                    static_cast<LONG>(y_from_lparam(lparam)),
                };
                ScreenToClient(hwnd, &point);
                dispatch_cursor_pos(
                    static_cast<float>(point.x),
                    static_cast<float>(point.y));
                double delta =
                    static_cast<double>(GET_WHEEL_DELTA_WPARAM(wparam))
                    / static_cast<double>(WHEEL_DELTA);
                double dx = (message == WM_MOUSEHWHEEL) ? delta : 0.0;
                double dy = (message == WM_MOUSEWHEEL) ? delta : 0.0;
                dispatch_scroll_xy(
                    dx,
                    dy,
                    static_cast<float>(state->surface->logical_width),
                    static_cast<float>(state->surface->logical_height));
                return 0;
            }
            break;
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
        case WM_KEYUP:
        case WM_SYSKEYUP:
            if (state) {
                bool const is_up = message == WM_KEYUP || message == WM_SYSKEYUP;
                bool const repeat = !is_up && ((lparam & (1l << 30)) != 0);
                dispatch_key(
                    win32_key(wparam),
                    is_up ? KeyAction::Release
                          : (repeat ? KeyAction::Repeat : KeyAction::Press),
                    win32_modifiers());
                return 0;
            }
            break;
        case WM_CHAR:
            if (state) {
                dispatch_win32_char(*state, static_cast<wchar_t>(wparam));
                return 0;
            }
            break;
        default:
            break;
    }
    return DefWindowProcW(hwnd, message, wparam, lparam);
}

inline void enable_per_monitor_dpi_awareness() {
#ifdef DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2
    if (auto* user32 = GetModuleHandleW(L"user32.dll")) {
        using SetDpiContextFn = BOOL(WINAPI*)(DPI_AWARENESS_CONTEXT);
        auto* set_context = reinterpret_cast<SetDpiContextFn>(
            GetProcAddress(user32, "SetProcessDpiAwarenessContext"));
        if (set_context)
            (void)set_context(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    }
#endif
}

inline bool register_window_class(HINSTANCE instance) {
    constexpr wchar_t class_name[] = L"PhenotypeNativeWindow";
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = win32_shell_wndproc;
    wc.hInstance = instance;
    wc.hCursor = load_system_cursor(win32_cursor_arrow_id);
    wc.hIcon = load_system_icon(win32_icon_application_id);
    wc.hbrBackground = nullptr;
    wc.lpszClassName = class_name;
    if (RegisterClassExW(&wc) != 0)
        return true;
    return GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
}

inline HWND create_win32_window(int width,
                                int height,
                                char const* title,
                                Win32ShellState& state) {
    enable_per_monitor_dpi_awareness();
    auto instance = GetModuleHandleW(nullptr);
    if (!register_window_class(instance))
        return nullptr;

    DWORD style = WS_OVERLAPPEDWINDOW;
    DWORD ex_style = 0;
    RECT rect{0, 0, width > 0 ? width : 800, height > 0 ? height : 600};
    if (!state.integrated_chrome)
        AdjustWindowRectEx(&rect, style, FALSE, ex_style);
    auto wide_title = utf8_to_wide(title);
    return CreateWindowExW(
        ex_style,
        L"PhenotypeNativeWindow",
        wide_title.c_str(),
        style,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        rect.right - rect.left,
        rect.bottom - rect.top,
        nullptr,
        nullptr,
        instance,
        &state);
}

inline void win32_set_hover_cursor(bool pointing) {
    auto* state = g_active_win32_shell;
    if (!state)
        return;
    state->active_cursor = pointing ? state->hand_cursor : state->arrow_cursor;
    apply_window_cursor(*state);
}

inline bool env_flag_enabled(char const* name) {
    auto const* value = std::getenv(name);
    return value && value[0] != '\0' && value[0] != '0';
}

inline bool write_startup_artifact_bundle(platform_api const& platform,
                                          char const* directory) {
    if (!directory || directory[0] == '\0')
        return true;
    if (!platform.debug.write_artifact_bundle)
        return false;
    auto const* reason = std::getenv("PHENOTYPE_ARTIFACT_REASON");
    if (!reason || reason[0] == '\0')
        reason = "startup-frame";
    auto bundle = platform.debug.write_artifact_bundle(directory, reason);
    if (!bundle.ok) {
        std::fprintf(stderr,
            "[phenotype-native] artifact bundle failed: %s\n",
            bundle.error.empty() ? "unknown error" : bundle.error.c_str());
        return false;
    }
    std::fprintf(stderr,
        "[phenotype-native] artifact bundle written: %s\n",
        bundle.directory.c_str());
    if (!bundle.snapshot_json_path.empty())
        std::fprintf(stderr,
            "[phenotype-native] snapshot: %s\n",
            bundle.snapshot_json_path.c_str());
    if (!bundle.frame_image_path.empty())
        std::fprintf(stderr,
            "[phenotype-native] frame: %s\n",
            bundle.frame_image_path.c_str());
    return true;
}

template<typename State, typename Msg, typename View, typename Update>
    requires std::invocable<View, State const&>
          && std::invocable<Update, State&, Msg>
int run_app_with_windows_platform(platform_api const& platform,
                                  int width,
                                  int height,
                                  char const* title,
                                  WindowOptions options,
                                  View view,
                                  Update update,
                                  std::function<void(int, int, float)> on_viewport = {}) {
    if (!platform.enabled) {
        std::fprintf(stderr, "Platform backend '%s' is disabled\n", platform.name);
        return 1;
    }
    if (platform.startup_message)
        std::fprintf(stderr, "%s\n", platform.startup_message);

    Win32ShellState shell{};
    shell.arrow_cursor = load_system_cursor(win32_cursor_arrow_id);
    shell.hand_cursor = load_system_cursor(win32_cursor_hand_id);
    shell.active_cursor = shell.arrow_cursor;
    shell.integrated_chrome = options.chrome == WindowChromeStyle::IntegratedTitlebar;
    shell.integrated_titlebar = options.integrated_titlebar;
    shell.running = true;

    HWND hwnd = create_win32_window(width, height, title, shell);
    if (!hwnd) {
        std::fprintf(stderr, "[win32] failed to create HWND\n");
        return 1;
    }
    shell.hwnd = hwnd;

    native_host host;
    NativeSurfaceDescriptor surface{
        .kind = NativeSurfaceKind::Win32Window,
        .window = hwnd,
    };
    surface.window_chrome = options.chrome;
    surface.integrated_titlebar = options.integrated_titlebar;
    surface.window_options_valid = true;
    host.window = &surface;
    host.platform = &platform;
    host.set_hover_cursor = &win32_set_hover_cursor;
    host.on_viewport_changed = std::move(on_viewport);
    shell.surface = &surface;
    shell.host = &host;
    g_active_win32_shell = &shell;
    sync_win32_surface(host, surface, hwnd, false);

    if (platform.window.configure)
        platform.window.configure(&surface, &options);

    bind_host(host, 0.0f);
    notify_viewport_changed(
        &host,
        surface.logical_width,
        surface.logical_height,
        surface.content_scale);
    run_host<State, Msg>(host, std::move(view), std::move(update));

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    auto const* artifact_dir = std::getenv("PHENOTYPE_ARTIFACT_DIR");
    bool const artifact_requested = artifact_dir && artifact_dir[0] != '\0';
    if (artifact_requested && platform.debug.capabilities
        && platform.debug.capabilities().material_backdrop_blur) {
        ::phenotype::detail::g_app.last_paint_hash = 0;
        repaint_current();
    }
    bool const artifact_ok = artifact_requested
        ? write_startup_artifact_bundle(platform, artifact_dir)
        : true;
    if (artifact_requested && env_flag_enabled("PHENOTYPE_ARTIFACT_EXIT")) {
        shutdown_host(host);
        DestroyWindow(hwnd);
        g_active_win32_shell = nullptr;
        return artifact_ok ? 0 : 1;
    }

    auto last_animation_tick = std::chrono::steady_clock::now();
    while (shell.running) {
        MSG msg{};
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                shell.running = false;
                break;
            }
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        if (!shell.running)
            break;
        service_host_tick(last_animation_tick);
        MsgWaitForMultipleObjectsEx(
            0,
            nullptr,
            16,
            QS_ALLINPUT,
            MWMO_INPUTAVAILABLE);
    }

    shutdown_host(host);
    if (IsWindow(hwnd))
        DestroyWindow(hwnd);
    g_active_win32_shell = nullptr;
    return 0;
}

} // namespace detail

inline constexpr int window_unbounded = -1;

inline void set_window_size_limits(int min_w, int min_h,
                                   int max_w, int max_h) {
    auto* state = detail::g_active_win32_shell;
    if (!state)
        return;
    state->min_w = min_w > 0 ? min_w : 0;
    state->min_h = min_h > 0 ? min_h : 0;
    state->max_w = max_w > 0 ? max_w : 0;
    state->max_h = max_h > 0 ? max_h : 0;
    if (state->hwnd) {
        SetWindowPos(state->hwnd, nullptr, 0, 0, 0, 0,
                     SWP_FRAMECHANGED
                     | SWP_NOMOVE
                     | SWP_NOSIZE
                     | SWP_NOOWNERZORDER
                     | SWP_NOZORDER);
    }
}

inline void set_window_aspect_ratio(int numerator, int denominator) {
    auto* state = detail::g_active_win32_shell;
    if (!state || numerator <= 0 || denominator <= 0)
        return;
    state->aspect_w = numerator;
    state->aspect_h = denominator;
}

inline void clear_window_aspect_ratio() {
    auto* state = detail::g_active_win32_shell;
    if (!state)
        return;
    state->aspect_w = 0;
    state->aspect_h = 0;
}

inline float content_scale() {
    auto* host = detail::active_host();
    return host ? host->cached_content_scale : 1.0f;
}

} // namespace phenotype::native
#endif
