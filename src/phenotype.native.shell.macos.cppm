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
    (void)options;

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
    return window;
}

inline id g_active_appkit_window = nullptr;
inline NativeSurfaceDescriptor* g_active_appkit_surface = nullptr;

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
    host.window = &surface;
    host.platform = &platform;
    host.set_hover_cursor = &appkit_set_hover_cursor;
    host.on_viewport_changed = std::move(on_viewport);
    sync_appkit_surface(host, surface, window, false);
    g_active_appkit_window = window;
    g_active_appkit_surface = &surface;

    if (platform.window.configure)
        platform.window.configure(&surface, &options);

    bind_host(host, 0.0f);
    notify_viewport_changed(
        &host,
        surface.logical_width,
        surface.logical_height,
        surface.content_scale);
    run_host<State, Msg>(host, std::move(view), std::move(update));

    objc_send<void>(window, sel("makeKeyAndOrderFront:"), nullptr);
    objc_send<void>(app, sel("activateIgnoringOtherApps:"), static_cast<signed char>(1));

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
