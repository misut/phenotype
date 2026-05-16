// AppKit desktop shell driver. Owns the NSApplication/NSWindow event loop
// while the shared native shell owns focus, callbacks, repaint, and rendering.

module;

#if defined(__APPLE__) && !defined(__wasi__) && !defined(__ANDROID__)
#include <cmath>
#include <chrono>
#include <concepts>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include <CoreGraphics/CoreGraphics.h>
#include <objc/message.h>
#include <objc/runtime.h>
#endif

export module phenotype.native.shell.macos;

#if defined(__APPLE__) && !defined(__wasi__) && !defined(__ANDROID__)
export import phenotype.native.shell;
import phenotype.native.platform;
import phenotype.state;

export namespace phenotype::native {
namespace detail {

template<typename R, typename... Args>
inline R objc_send(id object, SEL selector, Args... args) {
    return reinterpret_cast<R (*)(id, SEL, Args...)>(objc_msgSend)(
        object,
        selector,
        args...);
}

inline id class_id(char const* name) {
    return reinterpret_cast<id>(objc_getClass(name));
}

inline SEL sel(char const* name) {
    return sel_registerName(name);
}

inline id ns_string(char const* text) {
    return objc_send<id>(class_id("NSString"),
                         sel("stringWithUTF8String:"),
                         text ? text : "");
}

inline id ns_date_after(double seconds) {
    return objc_send<id>(class_id("NSDate"),
                         sel("dateWithTimeIntervalSinceNow:"),
                         seconds);
}

inline float sanitize_scale(double scale) {
    return (scale > 0.0 && std::isfinite(scale))
        ? static_cast<float>(scale)
        : 1.0f;
}

inline void apply_appkit_window_chrome(id window,
                                       WindowOptions const& options) {
    if (!window || options.chrome != WindowChromeStyle::IntegratedTitlebar)
        return;

    constexpr unsigned long full_size_content_view_mask = 1ul << 15;
    constexpr long hidden_title = 1;
    auto style = objc_send<unsigned long>(window, sel("styleMask"));
    objc_send<void>(
        window,
        sel("setStyleMask:"),
        style | full_size_content_view_mask);
    objc_send<void>(
        window,
        sel("setTitlebarAppearsTransparent:"),
        static_cast<signed char>(1));
    objc_send<void>(
        window,
        sel("setTitleVisibility:"),
        hidden_title);
    objc_send<void>(
        window,
        sel("setMovableByWindowBackground:"),
        static_cast<signed char>(1));
}

inline void sync_appkit_surface(native_host& host,
                                NativeSurfaceDescriptor& surface,
                                id window,
                                bool notify) {
    id view = static_cast<id>(surface.view);
    CGRect bounds{};
    if (view)
        bounds = objc_send<CGRect>(view, sel("bounds"));
    double const scale_value = window
        ? objc_send<double>(window, sel("backingScaleFactor"))
        : 1.0;
    float const scale = sanitize_scale(scale_value);
    int logical_width = static_cast<int>(std::lround(bounds.size.width));
    int logical_height = static_cast<int>(std::lround(bounds.size.height));
    if (logical_width <= 0)
        logical_width = host.cached_width_px;
    if (logical_height <= 0)
        logical_height = host.cached_height_px;
    int framebuffer_width = static_cast<int>(
        std::lround(static_cast<double>(logical_width) * scale));
    int framebuffer_height = static_cast<int>(
        std::lround(static_cast<double>(logical_height) * scale));
    if (framebuffer_width <= 0)
        framebuffer_width = logical_width;
    if (framebuffer_height <= 0)
        framebuffer_height = logical_height;

    bool const changed =
        surface.logical_width != logical_width
        || surface.logical_height != logical_height
        || surface.framebuffer_width != framebuffer_width
        || surface.framebuffer_height != framebuffer_height
        || std::fabs(surface.content_scale - scale) > 0.001f;

    surface.logical_width = logical_width;
    surface.logical_height = logical_height;
    surface.framebuffer_width = framebuffer_width;
    surface.framebuffer_height = framebuffer_height;
    surface.content_scale = scale;
    host.cached_width_px = logical_width;
    host.cached_height_px = logical_height;
    host.cached_content_scale = scale;

    if (changed && notify) {
        notify_viewport_changed(&host, logical_width, logical_height, scale);
        repaint_current();
    }
}

inline int appkit_modifiers(unsigned long flags) {
    int mods = 0;
    constexpr unsigned long shift = 1ul << 17;
    constexpr unsigned long control = 1ul << 18;
    constexpr unsigned long option = 1ul << 19;
    constexpr unsigned long command = 1ul << 20;
    if ((flags & shift) != 0)
        mods |= static_cast<int>(Modifier::Shift);
    if ((flags & control) != 0)
        mods |= static_cast<int>(Modifier::Control);
    if ((flags & option) != 0)
        mods |= static_cast<int>(Modifier::Alt);
    if ((flags & command) != 0)
        mods |= static_cast<int>(Modifier::Super);
    return mods;
}

inline Key appkit_key(unsigned short key_code) {
    switch (key_code) {
        case 0: return Key::A;
        case 36: return Key::Enter;
        case 48: return Key::Tab;
        case 49: return Key::Space;
        case 51: return Key::Backspace;
        case 53: return Key::Escape;
        case 76: return Key::KpEnter;
        case 115: return Key::Home;
        case 116: return Key::PageUp;
        case 119: return Key::End;
        case 121: return Key::PageDown;
        case 123: return Key::Left;
        case 124: return Key::Right;
        case 125: return Key::Down;
        case 126: return Key::Up;
        default: return Key::Other;
    }
}

inline std::optional<unsigned int> first_utf8_codepoint(char const* text) {
    if (!text || text[0] == '\0')
        return std::nullopt;
    auto const b0 = static_cast<unsigned char>(text[0]);
    if (b0 < 0x80)
        return b0;
    if ((b0 & 0xE0u) == 0xC0u) {
        auto const b1 = static_cast<unsigned char>(text[1]);
        if ((b1 & 0xC0u) == 0x80u)
            return ((b0 & 0x1Fu) << 6u) | (b1 & 0x3Fu);
    }
    if ((b0 & 0xF0u) == 0xE0u) {
        auto const b1 = static_cast<unsigned char>(text[1]);
        auto const b2 = static_cast<unsigned char>(text[2]);
        if ((b1 & 0xC0u) == 0x80u && (b2 & 0xC0u) == 0x80u) {
            return ((b0 & 0x0Fu) << 12u)
                | ((b1 & 0x3Fu) << 6u)
                | (b2 & 0x3Fu);
        }
    }
    if ((b0 & 0xF8u) == 0xF0u) {
        auto const b1 = static_cast<unsigned char>(text[1]);
        auto const b2 = static_cast<unsigned char>(text[2]);
        auto const b3 = static_cast<unsigned char>(text[3]);
        if ((b1 & 0xC0u) == 0x80u
            && (b2 & 0xC0u) == 0x80u
            && (b3 & 0xC0u) == 0x80u) {
            return ((b0 & 0x07u) << 18u)
                | ((b1 & 0x3Fu) << 12u)
                | ((b2 & 0x3Fu) << 6u)
                | (b3 & 0x3Fu);
        }
    }
    return std::nullopt;
}

inline CGPoint appkit_location(id event) {
    return objc_send<CGPoint>(event, sel("locationInWindow"));
}

inline float appkit_y(NativeSurfaceDescriptor const& surface, double y) {
    return static_cast<float>(surface.logical_height) - static_cast<float>(y);
}

inline void dispatch_appkit_event(id event, NativeSurfaceDescriptor const& surface) {
    if (!event)
        return;
    auto const type = objc_send<unsigned long>(event, sel("type"));
    switch (type) {
        case 1: {
            auto point = appkit_location(event);
            dispatch_mouse_button(
                static_cast<float>(point.x),
                appkit_y(surface, point.y),
                MouseButton::Left,
                KeyAction::Press,
                appkit_modifiers(objc_send<unsigned long>(event, sel("modifierFlags"))));
            break;
        }
        case 2: {
            auto point = appkit_location(event);
            dispatch_mouse_button(
                static_cast<float>(point.x),
                appkit_y(surface, point.y),
                MouseButton::Left,
                KeyAction::Release,
                appkit_modifiers(objc_send<unsigned long>(event, sel("modifierFlags"))));
            break;
        }
        case 3: {
            auto point = appkit_location(event);
            dispatch_mouse_button(
                static_cast<float>(point.x),
                appkit_y(surface, point.y),
                MouseButton::Right,
                KeyAction::Press,
                appkit_modifiers(objc_send<unsigned long>(event, sel("modifierFlags"))));
            break;
        }
        case 4: {
            auto point = appkit_location(event);
            dispatch_mouse_button(
                static_cast<float>(point.x),
                appkit_y(surface, point.y),
                MouseButton::Right,
                KeyAction::Release,
                appkit_modifiers(objc_send<unsigned long>(event, sel("modifierFlags"))));
            break;
        }
        case 5:
        case 6:
        case 7: {
            auto point = appkit_location(event);
            dispatch_cursor_pos(
                static_cast<float>(point.x),
                appkit_y(surface, point.y));
            break;
        }
        case 10: {
            auto const flags = objc_send<unsigned long>(event, sel("modifierFlags"));
            auto action = objc_send<signed char>(event, sel("isARepeat"))
                ? KeyAction::Repeat
                : KeyAction::Press;
            auto key = appkit_key(objc_send<unsigned short>(event, sel("keyCode")));
            dispatch_key(key, action, appkit_modifiers(flags));
            if ((flags & ((1ul << 18) | (1ul << 20))) == 0) {
                id chars = objc_send<id>(event, sel("characters"));
                char const* utf8 = chars
                    ? objc_send<char const*>(chars, sel("UTF8String"))
                    : nullptr;
                if (auto cp = first_utf8_codepoint(utf8))
                    dispatch_char(*cp);
            }
            break;
        }
        case 11:
            dispatch_key(
                appkit_key(objc_send<unsigned short>(event, sel("keyCode"))),
                KeyAction::Release,
                appkit_modifiers(objc_send<unsigned long>(event, sel("modifierFlags"))));
            break;
        case 22:
            dispatch_scroll_xy(
                objc_send<double>(event, sel("scrollingDeltaX")),
                objc_send<double>(event, sel("scrollingDeltaY")),
                static_cast<float>(surface.logical_width),
                static_cast<float>(surface.logical_height));
            break;
        default:
            break;
    }
}

inline id create_appkit_window(int width,
                               int height,
                               char const* title,
                               WindowOptions const& options) {
    CGRect frame{};
    frame.origin.x = 100.0;
    frame.origin.y = 100.0;
    frame.size.width = width > 0 ? width : 800;
    frame.size.height = height > 0 ? height : 600;

    constexpr unsigned long titled = 1ul << 0;
    constexpr unsigned long closable = 1ul << 1;
    constexpr unsigned long miniaturizable = 1ul << 2;
    constexpr unsigned long resizable = 1ul << 3;
    unsigned long style = titled | closable | miniaturizable | resizable;

    id window = objc_send<id>(
        objc_send<id>(class_id("NSWindow"), sel("alloc")),
        sel("initWithContentRect:styleMask:backing:defer:"),
        frame,
        style,
        static_cast<unsigned long>(2),
        static_cast<signed char>(0));
    if (!window)
        return nullptr;
    objc_send<void>(window, sel("setReleasedWhenClosed:"), static_cast<signed char>(0));
    objc_send<void>(window, sel("setTitle:"), ns_string(title));
    objc_send<void>(window, sel("setAcceptsMouseMovedEvents:"), static_cast<signed char>(1));
    apply_appkit_window_chrome(window, options);
    constexpr unsigned long move_to_active_space = 1ul << 1;
    objc_send<void>(window, sel("setCollectionBehavior:"), move_to_active_space);
    objc_send<void>(window, sel("center"));
    return window;
}

inline id g_active_appkit_window = nullptr;
inline NativeSurfaceDescriptor* g_active_appkit_surface = nullptr;

inline bool appkit_app_is_active(id app) {
    return app && objc_send<signed char>(app, sel("isActive")) != 0;
}

inline bool appkit_window_is_visible(id window) {
    return window && objc_send<signed char>(window, sel("isVisible")) != 0;
}

inline bool appkit_window_is_key(id window) {
    return window && objc_send<signed char>(window, sel("isKeyWindow")) != 0;
}

inline bool appkit_window_is_main(id window) {
    return window && objc_send<signed char>(window, sel("isMainWindow")) != 0;
}

inline bool activate_current_running_application() {
    id running_app = objc_send<id>(
        class_id("NSRunningApplication"),
        sel("currentApplication"));
    if (!running_app)
        return false;

    constexpr unsigned long activate_all_windows = 1ul << 0;
    constexpr unsigned long activate_ignoring_other_apps = 1ul << 1;
    return objc_send<signed char>(
        running_app,
        sel("activateWithOptions:"),
        activate_all_windows | activate_ignoring_other_apps) != 0;
}

inline void request_appkit_window_front(id app, id window) {
    if (!window)
        return;

    // Activation is asynchronous for command-line AppKit apps. Request app
    // focus and window key/main status as a single idempotent publish step so
    // the startup path and retry loop cannot drift apart.
    activate_current_running_application();
    if (app) {
        objc_send<void>(app, sel("activate"));
        objc_send<void>(
            app,
            sel("activateIgnoringOtherApps:"),
            static_cast<signed char>(1));
    }
    objc_send<void>(window, sel("makeKeyAndOrderFront:"), nullptr);
    objc_send<void>(window, sel("orderFrontRegardless"));
}

inline signed char appkit_should_handle_reopen(id, SEL, id app, signed char) {
    request_appkit_window_front(app, g_active_appkit_window);
    return static_cast<signed char>(1);
}

inline void appkit_did_become_active(id, SEL, id) {
    id app = objc_send<id>(class_id("NSApplication"), sel("sharedApplication"));
    request_appkit_window_front(app, g_active_appkit_window);
}

inline id create_appkit_shell_delegate() {
    static Class delegate_class = nullptr;
    if (!delegate_class) {
        Class base = reinterpret_cast<Class>(class_id("NSObject"));
        delegate_class = objc_allocateClassPair(
            base,
            "PhenotypeAppKitShellDelegate",
            0);
        if (!delegate_class)
            return nullptr;
        class_addMethod(
            delegate_class,
            sel("applicationShouldHandleReopen:hasVisibleWindows:"),
            reinterpret_cast<IMP>(appkit_should_handle_reopen),
            "c@:@@c");
        class_addMethod(
            delegate_class,
            sel("applicationDidBecomeActive:"),
            reinterpret_cast<IMP>(appkit_did_become_active),
            "v@:@");
        objc_registerClassPair(delegate_class);
    }
    return objc_send<id>(
        objc_send<id>(reinterpret_cast<id>(delegate_class), sel("alloc")),
        sel("init"));
}

inline id g_appkit_shell_delegate = nullptr;

inline void prime_appkit_window_ordering(id app) {
    if (!app)
        return;
    // AppKit does additional WindowServer ordering work inside NSApplication::run.
    // Prime it once, then return to phenotype's deterministic event pump.
    objc_send<void>(
        app,
        sel("performSelector:withObject:afterDelay:"),
        sel("stop:"),
        nullptr,
        0.10);
    objc_send<void>(app, sel("run"));
}

struct AppKitWindowServerSurface {
    bool valid = false;
    bool onscreen = false;
    double width = 0.0;
    double height = 0.0;
};

inline AppKitWindowServerSurface appkit_window_server_surface(id window) {
    AppKitWindowServerSurface surface;
    if (!window)
        return surface;

    auto number = objc_send<int>(window, sel("windowNumber"));
    if (number <= 0)
        return surface;

    CFArrayRef raw_windows = CGWindowListCopyWindowInfo(
        kCGWindowListOptionIncludingWindow,
        static_cast<CGWindowID>(number));
    if (!raw_windows)
        return surface;

    struct CFArrayGuard {
        CFArrayRef value = nullptr;
        ~CFArrayGuard() {
            if (value)
                CFRelease(value);
        }
    } windows{raw_windows};

    if (CFArrayGetCount(windows.value) <= 0)
        return surface;
    auto info = static_cast<CFDictionaryRef>(
        CFArrayGetValueAtIndex(windows.value, 0));
    if (!info || CFGetTypeID(info) != CFDictionaryGetTypeID())
        return surface;

    if (auto raw_onscreen = CFDictionaryGetValue(info, kCGWindowIsOnscreen)) {
        if (CFGetTypeID(raw_onscreen) == CFBooleanGetTypeID()) {
            surface.onscreen = CFBooleanGetValue(
                static_cast<CFBooleanRef>(raw_onscreen));
        }
    }

    auto raw_bounds = CFDictionaryGetValue(info, kCGWindowBounds);
    if (!raw_bounds || CFGetTypeID(raw_bounds) != CFDictionaryGetTypeID())
        return surface;

    CGRect rect{};
    if (!CGRectMakeWithDictionaryRepresentation(
            static_cast<CFDictionaryRef>(raw_bounds),
            &rect))
        return surface;

    surface.valid = rect.size.width > 0.0 && rect.size.height > 0.0;
    surface.width = rect.size.width;
    surface.height = rect.size.height;
    return surface;
}

inline bool appkit_window_server_surface_ready(id window) {
    auto surface = appkit_window_server_surface(window);
    return surface.valid && surface.onscreen;
}

inline char const* appkit_window_front_state(id app, id window) {
    if (!window)
        return "missing-window";
    if (!appkit_window_is_visible(window))
        return "window-not-visible";
    auto surface = appkit_window_server_surface(window);
    if (!surface.valid)
        return "window-server-bounds-missing";
    if (!surface.onscreen)
        return "window-server-offscreen";
    if (!appkit_app_is_active(app))
        return "app-not-active";
    if (!appkit_window_is_key(window))
        return "window-not-key";
    if (!appkit_window_is_main(window))
        return "window-not-main";
    return "ready";
}

inline bool appkit_window_front_ready(id app, id window) {
    return std::string_view{appkit_window_front_state(app, window)} == "ready";
}

inline void pump_appkit_ordering_once(id app, double timeout_seconds) {
    if (!app)
        return;
    objc_send<void>(app, sel("updateWindows"));
    id event = objc_send<id>(
        app,
        sel("nextEventMatchingMask:untilDate:inMode:dequeue:"),
        ~0ul,
        ns_date_after(timeout_seconds),
        ns_string("NSDefaultRunLoopMode"),
        static_cast<signed char>(1));
    if (event)
        objc_send<void>(app, sel("sendEvent:"), event);
    objc_send<void>(app, sel("updateWindows"));
}

inline void wait_for_appkit_window_server_surface(id app, id window) {
    if (!app || !window)
        return;
    auto const deadline =
        std::chrono::steady_clock::now() + std::chrono::milliseconds(1000);
    while (!appkit_window_server_surface_ready(window)) {
        if (std::chrono::steady_clock::now() >= deadline)
            return;
        pump_appkit_ordering_once(app, 0.016);
    }
}

inline void wait_for_appkit_window_front(id app, id window) {
    if (!app || !window)
        return;
    auto const deadline =
        std::chrono::steady_clock::now() + std::chrono::milliseconds(1500);
    while (!appkit_window_front_ready(app, window)) {
        if (std::chrono::steady_clock::now() >= deadline)
            return;
        request_appkit_window_front(app, window);
        pump_appkit_ordering_once(app, 0.016);
    }
}

inline void warn_if_appkit_window_not_front(id app, id window) {
    auto const* state = appkit_window_front_state(app, window);
    if (std::string_view{state} == "ready")
        return;
    std::fprintf(
        stderr,
        "[appkit] window fronting incomplete: state=%s visible=%d active=%d key=%d main=%d\n",
        state,
        appkit_window_is_visible(window) ? 1 : 0,
        appkit_app_is_active(app) ? 1 : 0,
        appkit_window_is_key(window) ? 1 : 0,
        appkit_window_is_main(window) ? 1 : 0);
}

inline void appkit_set_hover_cursor(bool pointing) {
    id cursor = objc_send<id>(
        class_id("NSCursor"),
        pointing ? sel("pointingHandCursor") : sel("arrowCursor"));
    if (cursor)
        objc_send<void>(cursor, sel("set"));
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
int run_app_with_macos_platform(platform_api const& platform,
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

    id pool = objc_send<id>(objc_send<id>(class_id("NSAutoreleasePool"), sel("alloc")), sel("init"));
    id app = objc_send<id>(class_id("NSApplication"), sel("sharedApplication"));
    objc_send<void>(app, sel("setActivationPolicy:"), static_cast<long>(0));
    if (!g_appkit_shell_delegate)
        g_appkit_shell_delegate = create_appkit_shell_delegate();
    if (g_appkit_shell_delegate)
        objc_send<void>(app, sel("setDelegate:"), g_appkit_shell_delegate);
    objc_send<void>(app, sel("finishLaunching"));

    id window = create_appkit_window(width, height, title, options);
    if (!window) {
        std::fprintf(stderr, "[appkit] failed to create NSWindow\n");
        if (pool)
            objc_send<void>(pool, sel("drain"));
        return 1;
    }
    id view_obj = objc_send<id>(window, sel("contentView"));

    native_host host;
    NativeSurfaceDescriptor surface{
        .kind = NativeSurfaceKind::MacOSWindow,
        .window = window,
        .view = view_obj,
    };
    surface.window_chrome = options.chrome;
    surface.integrated_titlebar = options.integrated_titlebar;
    surface.window_options_valid = true;
    host.window = &surface;
    host.platform = &platform;
    host.set_hover_cursor = &appkit_set_hover_cursor;
    host.on_viewport_changed = std::move(on_viewport);
    sync_appkit_surface(host, surface, window, false);
    g_active_appkit_window = window;
    g_active_appkit_surface = &surface;

    if (platform.window.configure)
        platform.window.configure(&surface, &options);
    sync_appkit_surface(host, surface, window, false);

    request_appkit_window_front(app, window);

    bind_host(host, 0.0f);
    notify_viewport_changed(
        &host,
        surface.logical_width,
        surface.logical_height,
        surface.content_scale);
    run_host<State, Msg>(host, std::move(view), std::move(update));

    request_appkit_window_front(app, window);
    prime_appkit_window_ordering(app);
    wait_for_appkit_window_front(app, window);
    warn_if_appkit_window_not_front(app, window);
    repaint_current_after_surface_presented();

    auto const* artifact_dir = std::getenv("PHENOTYPE_ARTIFACT_DIR");
    bool const artifact_requested = artifact_dir && artifact_dir[0] != '\0';
    if (artifact_requested && platform.debug.capabilities
        && platform.debug.capabilities().material_backdrop_blur) {
        ::phenotype::detail::g_app.last_paint_hash = 0;
        repaint_current();
        wait_for_appkit_window_front(app, window);
    }
    bool const artifact_ok = artifact_requested
        ? write_startup_artifact_bundle(platform, artifact_dir)
        : true;
    if (artifact_requested && env_flag_enabled("PHENOTYPE_ARTIFACT_EXIT")) {
        shutdown_host(host);
        objc_send<void>(window, sel("close"));
        if (pool)
            objc_send<void>(pool, sel("drain"));
        g_active_appkit_window = nullptr;
        g_active_appkit_surface = nullptr;
        return artifact_ok ? 0 : 1;
    }

    auto last_animation_tick = std::chrono::steady_clock::now();
    while (objc_send<signed char>(window, sel("isVisible"))) {
        sync_appkit_surface(host, surface, window, true);
        id event = objc_send<id>(
            app,
            sel("nextEventMatchingMask:untilDate:inMode:dequeue:"),
            ~0ul,
            ns_date_after(0.016),
            ns_string("NSDefaultRunLoopMode"),
            static_cast<signed char>(1));
        if (event) {
            dispatch_appkit_event(event, surface);
            objc_send<void>(app, sel("sendEvent:"), event);
        }
        objc_send<void>(app, sel("updateWindows"));
        service_host_tick(last_animation_tick);
    }

    shutdown_host(host);
    g_active_appkit_window = nullptr;
    g_active_appkit_surface = nullptr;
    if (pool)
        objc_send<void>(pool, sel("drain"));
    return 0;
}

} // namespace detail

inline constexpr int window_unbounded = -1;

inline void set_window_size_limits(int min_w, int min_h,
                                   int max_w, int max_h) {
    if (!detail::g_active_appkit_window)
        return;
    CGSize min_size{
        static_cast<double>(min_w > 0 ? min_w : 0),
        static_cast<double>(min_h > 0 ? min_h : 0),
    };
    detail::objc_send<void>(
        detail::g_active_appkit_window,
        detail::sel("setContentMinSize:"),
        min_size);
    if (max_w > 0 || max_h > 0) {
        CGSize max_size{
            static_cast<double>(max_w > 0 ? max_w : 1000000),
            static_cast<double>(max_h > 0 ? max_h : 1000000),
        };
        detail::objc_send<void>(
            detail::g_active_appkit_window,
            detail::sel("setContentMaxSize:"),
            max_size);
    }
}

inline void set_window_aspect_ratio(int numerator, int denominator) {
    if (!detail::g_active_appkit_window || numerator <= 0 || denominator <= 0)
        return;
    CGSize ratio{
        static_cast<double>(numerator),
        static_cast<double>(denominator),
    };
    detail::objc_send<void>(
        detail::g_active_appkit_window,
        detail::sel("setAspectRatio:"),
        ratio);
}

inline void clear_window_aspect_ratio() {
    if (!detail::g_active_appkit_window)
        return;
    CGSize ratio{0.0, 0.0};
    detail::objc_send<void>(
        detail::g_active_appkit_window,
        detail::sel("setAspectRatio:"),
        ratio);
}

inline float content_scale() {
    auto* host = detail::active_host();
    return host ? host->cached_content_scale : 1.0f;
}

} // namespace phenotype::native
#endif
