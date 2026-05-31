// AppKit desktop shell driver. Owns the NSApplication/NSWindow event loop
// while the shared native shell owns focus, callbacks, repaint, and rendering.

module;

#if defined(__APPLE__) && !defined(__wasi__) && !defined(__ANDROID__)
#include <algorithm>
#include <charconv>
#include <cmath>
#include <chrono>
#include <concepts>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <format>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include <CoreGraphics/CoreGraphics.h>
#include <Carbon/Carbon.h>
#include <objc/message.h>
#include <objc/runtime.h>
#endif

export module phenotype.native.shell.macos;

#if defined(__APPLE__) && !defined(__wasi__) && !defined(__ANDROID__)
export import phenotype.native.shell;
import phenotype.native.macos.objc;
import phenotype.native.platform;
import phenotype.state;

export namespace phenotype::native {
namespace detail {

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
    host.surface_descriptor = &surface;
    sync_host_render_surface(host, changed);

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

inline bool appkit_function_modifier(unsigned long flags) noexcept {
    constexpr unsigned long function = 1ul << 23;
    return (flags & function) != 0;
}

inline Key appkit_key(unsigned short key_code) {
    switch (key_code) {
        case 0: return Key::A;
        case 2: return Key::D;
        case 3: return Key::F;
        case 36: return Key::Enter;
        case 48: return Key::Tab;
        case 49: return Key::Space;
        case 51: return Key::Backspace;
        case 53: return Key::Escape;
        case 45: return Key::N;
        case 76: return Key::KpEnter;
        case 111: return Key::F12;
        case 115: return Key::Home;
        case 116: return Key::PageUp;
        case 117: return Key::Delete;
        case 119: return Key::End;
        case 121: return Key::PageDown;
        case 123: return Key::Left;
        case 124: return Key::Right;
        case 125: return Key::Down;
        case 126: return Key::Up;
        default: return Key::Other;
    }
}

inline Key appkit_key_from_code_or_function_codepoint(
        unsigned short key_code,
        std::optional<unsigned int> function_codepoint = std::nullopt) {
    auto key = appkit_key(key_code);
    if (key != Key::Other)
        return key;
    if (function_codepoint.has_value() && *function_codepoint == 0xF70Fu)
        return Key::F12;
    return Key::Other;
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

inline std::optional<unsigned int> appkit_characters_ignoring_modifiers_codepoint(
        id event) {
    if (!event || !objc_responds_to(event, sel("charactersIgnoringModifiers")))
        return std::nullopt;
    id chars = objc_send<id>(event, sel("charactersIgnoringModifiers"));
    char const* utf8 = chars
        ? objc_send<char const*>(chars, sel("UTF8String"))
        : nullptr;
    return first_utf8_codepoint(utf8);
}

inline Key appkit_event_key(id event) {
    if (!event)
        return Key::Other;
    return appkit_key_from_code_or_function_codepoint(
        objc_send<unsigned short>(event, sel("keyCode")),
        appkit_characters_ignoring_modifiers_codepoint(event));
}

inline bool appkit_event_matches_debug_panel_shortcut(
        unsigned short key_code,
        std::optional<unsigned int> function_codepoint,
        unsigned long flags) {
#ifndef NDEBUG
    return appkit_key_from_code_or_function_codepoint(
            key_code,
            function_codepoint) == Key::F12
        && normalized_key_command_modifiers(appkit_modifiers(flags))
            == debug_panel_shortcut_modifiers();
#else
    (void)key_code;
    (void)function_codepoint;
    (void)flags;
    return false;
#endif
}

inline bool appkit_event_matches_debug_panel_shortcut(id event) {
    if (!event)
        return false;
    return appkit_event_matches_debug_panel_shortcut(
        objc_send<unsigned short>(event, sel("keyCode")),
        appkit_characters_ignoring_modifiers_codepoint(event),
        objc_send<unsigned long>(event, sel("modifierFlags")));
}

inline bool appkit_system_defined_aux_key_matches_debug_panel_shortcut(
        long data1,
        unsigned long flags) {
#ifndef NDEBUG
    // When macOS maps the top row to media keys, the physical F12 key
    // can still arrive as Sound Up with the Function modifier set.
    // Accept that Fn+Cmd path, but reject plain Cmd+SoundUp so the
    // volume key alone does not toggle the debug panel.
    constexpr int nx_keytype_sound_up = 0;
    constexpr int aux_key_down_state = 0x0A;
    auto const key_type = static_cast<int>((data1 >> 16) & 0xFFFF);
    auto const key_state = static_cast<int>((data1 >> 8) & 0xFF);
    return key_type == nx_keytype_sound_up
        && key_state == aux_key_down_state
        && appkit_function_modifier(flags)
        && normalized_key_command_modifiers(appkit_modifiers(flags))
            == debug_panel_shortcut_modifiers();
#else
    (void)data1;
    (void)flags;
    return false;
#endif
}

inline bool appkit_event_matches_system_debug_panel_shortcut(id event) {
    if (!event || !objc_responds_to(event, sel("subtype"))
        || !objc_responds_to(event, sel("data1")))
        return false;
    constexpr long nx_subtype_aux_control_buttons = 8;
    if (objc_send<long>(event, sel("subtype"))
        != nx_subtype_aux_control_buttons)
        return false;
    return appkit_system_defined_aux_key_matches_debug_panel_shortcut(
        objc_send<long>(event, sel("data1")),
        objc_send<unsigned long>(event, sel("modifierFlags")));
}

inline bool appkit_dispatch_debug_panel_shortcut_from_event(id event) {
    if (!event)
        return false;
    auto const type = objc_send<unsigned long>(event, sel("type"));
    if (type == 10 && appkit_event_matches_debug_panel_shortcut(event)) {
        return dispatch_debug_panel_shortcut(
            Key::F12,
            KeyAction::Press,
            appkit_modifiers(objc_send<unsigned long>(event, sel("modifierFlags"))),
            "f12");
    }
    if (type == 14 && appkit_event_matches_system_debug_panel_shortcut(event)) {
        return dispatch_debug_panel_shortcut(
            Key::F12,
            KeyAction::Press,
            appkit_modifiers(objc_send<unsigned long>(event, sel("modifierFlags"))),
            "f12");
    }
    return false;
}

inline CGPoint appkit_location(id event) {
    return objc_send<CGPoint>(event, sel("locationInWindow"));
}

inline float appkit_y(NativeSurfaceDescriptor const& surface, double y) {
    return static_cast<float>(surface.logical_height) - static_cast<float>(y);
}

inline bool appkit_event_hits_native_titlebar_control_reserve(
        id event,
        NativeSurfaceDescriptor const& surface) {
    if (!event || surface.window_chrome != WindowChromeStyle::IntegratedTitlebar)
        return false;
    auto const type = objc_send<unsigned long>(event, sel("type"));
    if (type != 1 && type != 2 && type != 5 && type != 6 && type != 7)
        return false;
    auto const point = appkit_location(event);
    float const top_y = appkit_y(surface, point.y);
    float const titlebar_height = std::max(
        36.0f,
        surface.integrated_titlebar.height);
    float const leading_controls = std::max(
        80.0f,
        surface.integrated_titlebar.leading_control_reserved_width);
    return point.x >= 0.0
        && point.x <= leading_controls
        && top_y >= 0.0f
        && top_y <= titlebar_height;
}

inline bool appkit_event_hits_close_button_fallback(
        id event,
        NativeSurfaceDescriptor const& surface) {
    if (!event || surface.window_chrome != WindowChromeStyle::IntegratedTitlebar)
        return false;
    auto const type = objc_send<unsigned long>(event, sel("type"));
    if (type != 1 && type != 2 && type != 5 && type != 6 && type != 7)
        return false;
    auto const point = appkit_location(event);
    float const top_y = appkit_y(surface, point.y);
    float const titlebar_height = std::max(
        36.0f,
        surface.integrated_titlebar.height);
    return point.x >= 0.0
        && point.x <= 52.0
        && top_y >= 0.0f
        && top_y <= std::min(44.0f, titlebar_height);
}

inline bool appkit_handle_standard_key_equivalent(id event);

inline signed char appkit_window_perform_key_equivalent(id self, SEL, id event) {
    if (appkit_handle_standard_key_equivalent(event))
        return static_cast<signed char>(1);

    objc_super super_info{};
    super_info.receiver = self;
    super_info.super_class = class_getSuperclass(object_getClass(self));
    using SuperSend = signed char (*)(objc_super*, SEL, id);
    return reinterpret_cast<SuperSend>(objc_msgSendSuper)(
        &super_info,
        sel("performKeyEquivalent:"),
        event);
}

inline Class appkit_window_class() {
    static Class window_class = nullptr;
    if (!window_class) {
        Class base = reinterpret_cast<Class>(class_id("NSWindow"));
        window_class = objc_allocateClassPair(
            base,
            "PhenotypeAppKitWindow",
            0);
        if (window_class) {
            class_addMethod(
                window_class,
                sel("performKeyEquivalent:"),
                reinterpret_cast<IMP>(appkit_window_perform_key_equivalent),
                "c@:@");
            objc_registerClassPair(window_class);
        }
    }
    return window_class ? window_class : reinterpret_cast<Class>(class_id("NSWindow"));
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
            if (appkit_handle_standard_key_equivalent(event))
                break;
            auto const flags = objc_send<unsigned long>(event, sel("modifierFlags"));
            auto action = objc_send<signed char>(event, sel("isARepeat"))
                ? KeyAction::Repeat
                : KeyAction::Press;
            auto key = appkit_event_key(event);
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
                appkit_event_key(event),
                KeyAction::Release,
                appkit_modifiers(objc_send<unsigned long>(event, sel("modifierFlags"))));
            break;
        case 14:
            (void)appkit_dispatch_debug_panel_shortcut_from_event(event);
            break;
        case 22:
            dispatch_scroll_xy(
                objc_send<double>(event, sel("scrollingDeltaX")),
                objc_send<double>(event, sel("scrollingDeltaY")),
                objc_send<signed char>(
                    event,
                    sel("hasPreciseScrollingDeltas")) != 0,
                static_cast<float>(surface.logical_width),
                static_cast<float>(surface.logical_height));
            break;
        default:
            break;
    }
}

inline bool appkit_event_targets_window(id event, id window) {
    if (!event || !window)
        return false;
    id event_window = objc_send<id>(event, sel("window"));
    return !event_window || event_window == window;
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
        objc_send<id>(reinterpret_cast<id>(appkit_window_class()), sel("alloc")),
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
inline bool g_appkit_keep_running_after_last_window_closed = false;
inline bool g_appkit_should_terminate = false;
inline bool g_appkit_mouse_tracking_mode = false;
inline bool g_appkit_window_user_closed = false;
inline bool g_appkit_close_button_candidate = false;
inline int g_appkit_front_request_attempts = 0;
inline void (*g_appkit_settings_menu_handler)() = nullptr;
inline std::string g_appkit_application_name = "Phenotype";

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

#ifndef NDEBUG
inline EventHotKeyRef g_appkit_debug_hot_key_ref = nullptr;
inline EventHandlerRef g_appkit_debug_hot_key_handler_ref = nullptr;
inline EventHandlerUPP g_appkit_debug_hot_key_handler_upp = nullptr;

inline constexpr unsigned int appkit_fourcc(char a,
                                            char b,
                                            char c,
                                            char d) noexcept {
    return (static_cast<unsigned int>(static_cast<unsigned char>(a)) << 24u)
        | (static_cast<unsigned int>(static_cast<unsigned char>(b)) << 16u)
        | (static_cast<unsigned int>(static_cast<unsigned char>(c)) << 8u)
        | static_cast<unsigned int>(static_cast<unsigned char>(d));
}

inline constexpr unsigned int appkit_debug_hot_key_signature() noexcept {
    return appkit_fourcc('p', 'h', 'd', 'b');
}

inline constexpr unsigned int appkit_debug_hot_key_identifier() noexcept {
    return 12u;
}

inline bool appkit_debug_hot_key_matches(unsigned int signature,
                                         unsigned int identifier) noexcept {
    return signature == appkit_debug_hot_key_signature()
        && identifier == appkit_debug_hot_key_identifier();
}

inline OSStatus appkit_debug_hot_key_handler(EventHandlerCallRef,
                                             EventRef event,
                                             void*) {
    EventHotKeyID hot_key{};
    auto status = GetEventParameter(
        event,
        kEventParamDirectObject,
        typeEventHotKeyID,
        nullptr,
        sizeof(hot_key),
        nullptr,
        &hot_key);
    if (status != noErr
        || !appkit_debug_hot_key_matches(hot_key.signature, hot_key.id))
        return eventNotHandledErr;

    id app = objc_send<id>(class_id("NSApplication"), sel("sharedApplication"));
    if (!appkit_app_is_active(app))
        return eventNotHandledErr;

    return dispatch_debug_panel_shortcut(
        Key::F12,
        KeyAction::Press,
        debug_panel_shortcut_modifiers(),
        "carbon-hotkey-f12")
        ? noErr
        : eventNotHandledErr;
}

inline void install_appkit_debug_hot_key() {
    if (g_appkit_debug_hot_key_ref)
        return;

    auto target = GetApplicationEventTarget();
    if (!target)
        return;

    if (!g_appkit_debug_hot_key_handler_ref) {
        EventTypeSpec spec{kEventClassKeyboard, kEventHotKeyPressed};
        g_appkit_debug_hot_key_handler_upp =
            NewEventHandlerUPP(appkit_debug_hot_key_handler);
        auto const status = InstallEventHandler(
            target,
            g_appkit_debug_hot_key_handler_upp,
            1,
            &spec,
            nullptr,
            &g_appkit_debug_hot_key_handler_ref);
        if (status != noErr) {
            std::fprintf(
                stderr,
                "[phenotype-native] debug hot key handler install failed: %d\n",
                static_cast<int>(status));
            if (g_appkit_debug_hot_key_handler_upp) {
                DisposeEventHandlerUPP(g_appkit_debug_hot_key_handler_upp);
                g_appkit_debug_hot_key_handler_upp = nullptr;
            }
            return;
        }
    }

    EventHotKeyID hot_key{
        static_cast<OSType>(appkit_debug_hot_key_signature()),
        appkit_debug_hot_key_identifier(),
    };
    auto const status = RegisterEventHotKey(
        kVK_F12,
        cmdKey,
        hot_key,
        target,
        kEventHotKeyNoOptions,
        &g_appkit_debug_hot_key_ref);
    if (status != noErr) {
        std::fprintf(
            stderr,
            "[phenotype-native] debug hot key registration failed: %d\n",
            static_cast<int>(status));
        g_appkit_debug_hot_key_ref = nullptr;
    }
}

inline void uninstall_appkit_debug_hot_key() {
    if (g_appkit_debug_hot_key_ref) {
        UnregisterEventHotKey(g_appkit_debug_hot_key_ref);
        g_appkit_debug_hot_key_ref = nullptr;
    }
    if (g_appkit_debug_hot_key_handler_ref) {
        RemoveEventHandler(g_appkit_debug_hot_key_handler_ref);
        g_appkit_debug_hot_key_handler_ref = nullptr;
    }
    if (g_appkit_debug_hot_key_handler_upp) {
        DisposeEventHandlerUPP(g_appkit_debug_hot_key_handler_upp);
        g_appkit_debug_hot_key_handler_upp = nullptr;
    }
}
#else
inline void install_appkit_debug_hot_key() {}
inline void uninstall_appkit_debug_hot_key() {}
#endif

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

struct AppKitPreferencesChoiceState {
    std::string label;
    std::string value;
    bool selected = false;
};

struct AppKitPreferencesSectionState {
    std::string title;
    std::string subtitle;
    std::vector<AppKitPreferencesChoiceState> choices;
    void (*on_select)(char const* value, void* user_data) = nullptr;
    int control_tag = 0;
    id title_label = nullptr;
    id segmented_control = nullptr;
    id subtitle_label = nullptr;
};

struct AppKitPreferencesWindowState {
    std::string identifier;
    std::string title;
    std::string appearance = "system";
    int width = 420;
    int height = 300;
    id window = nullptr;
    id content_view = nullptr;
    bool visible = false;
    std::vector<AppKitPreferencesSectionState> sections;
    void* user_data = nullptr;
    void (*on_close)(void* user_data) = nullptr;
    id title_label = nullptr;
    unsigned long long content_rebuild_count = 0;
};

inline std::vector<AppKitPreferencesWindowState> g_appkit_preferences_windows;
inline id g_appkit_preferences_delegate = nullptr;
inline int g_appkit_preferences_next_tag = 7000;

inline std::string safe_preferences_text(char const* value,
                                         char const* fallback = "") {
    return value && value[0] != '\0' ? std::string{value} : std::string{fallback};
}

inline void appkit_set_hover_cursor(bool pointing);

struct AppKitSceneWindowOptions {
    char const* identifier = "scene-window";
    char const* title = "Window";
    int width = 640;
    int height = 420;
    char const* scene_id = nullptr;
    char const* surface_id = nullptr;
    ::phenotype::SceneRole scene_role = ::phenotype::SceneRole::Window;
    ::phenotype::RenderSurfaceRole surface_role =
        ::phenotype::RenderSurfaceRole::Window;
    WindowOptions window_options = {};
    bool order_front = true;
    void* user_data = nullptr;
    void (*on_close)(void* user_data) = nullptr;
};

inline AppKitSceneWindowOptions appkit_scene_window_options(
        NativeSceneWindowOptions const& options) {
    return AppKitSceneWindowOptions{
        .identifier = options.identifier,
        .title = options.title,
        .width = options.width,
        .height = options.height,
        .scene_id = options.scene_id,
        .surface_id = options.surface_id,
        .scene_role = options.scene_role,
        .surface_role = options.surface_role,
        .window_options = options.window_options,
        .order_front = options.order_front,
        .user_data = options.user_data,
        .on_close = options.on_close,
    };
}

struct AppKitSceneWindowState {
    std::string identifier;
    std::string title;
    std::string scene_id;
    std::string surface_id;
    int width = 640;
    int height = 420;
    ::phenotype::SceneRole scene_role = ::phenotype::SceneRole::Window;
    ::phenotype::RenderSurfaceRole surface_role =
        ::phenotype::RenderSurfaceRole::Window;
    WindowOptions window_options = {};
    void* user_data = nullptr;
    void (*on_close)(void* user_data) = nullptr;
    id window = nullptr;
    id content_view = nullptr;
    NativeSurfaceDescriptor surface{};
    native_host host{};
    bool visible = false;
    bool runner_installed = false;
    std::chrono::steady_clock::time_point last_animation_tick =
        std::chrono::steady_clock::now();
};

inline std::vector<std::unique_ptr<AppKitSceneWindowState>>
    g_appkit_scene_windows;
inline id g_appkit_scene_window_delegate = nullptr;

inline std::string scene_window_identifier_or_default(char const* value) {
    auto identifier = safe_preferences_text(value, "scene-window");
    return identifier.empty() ? std::string{"scene-window"} : identifier;
}

inline AppKitSceneWindowState* find_appkit_scene_window(
        std::string_view identifier) {
    for (auto& state : g_appkit_scene_windows) {
        if (state && state->identifier == identifier)
            return state.get();
    }
    return nullptr;
}

inline AppKitSceneWindowState* find_appkit_scene_window_by_window(id window) {
    if (!window)
        return nullptr;
    for (auto& state : g_appkit_scene_windows) {
        if (state && state->window == window)
            return state.get();
    }
    return nullptr;
}

inline std::string default_scene_id_for_window(std::string_view identifier) {
    return "appkit:" + std::string{identifier};
}

inline std::string default_surface_id_for_window(std::string_view identifier) {
    return "appkit:" + std::string{identifier} + ":surface";
}

inline void mark_appkit_scene_window_visibility(AppKitSceneWindowState& state,
                                                bool visible) {
    state.visible = visible;
    state.host.render_surface_visible = visible;
    {
        ScopedHostActivation activate(state.host);
        sync_host_render_surface(state.host, true);
    }
}

inline void refresh_appkit_scene_window_visibility(
        AppKitSceneWindowState& state) {
    mark_appkit_scene_window_visibility(
        state,
        appkit_window_is_visible(state.window));
}

inline void appkit_scene_window_will_close(id, SEL, id notification) {
    id window = notification
        ? objc_send<id>(notification, sel("object"))
        : nullptr;
    if (auto* state = find_appkit_scene_window_by_window(window)) {
        bool const was_visible = state->visible;
        mark_appkit_scene_window_visibility(*state, false);
        if (was_visible && state->on_close)
            state->on_close(state->user_data);
        ::phenotype::detail::trigger_rebuild();
    }
}

inline id create_appkit_scene_window_delegate() {
    if (g_appkit_scene_window_delegate)
        return g_appkit_scene_window_delegate;
    static Class delegate_class = nullptr;
    if (!delegate_class) {
        Class base = reinterpret_cast<Class>(class_id("NSObject"));
        delegate_class = objc_allocateClassPair(
            base,
            "PhenotypeAppKitSceneWindowDelegate",
            0);
        if (!delegate_class)
            return nullptr;
        class_addMethod(
            delegate_class,
            sel("windowWillClose:"),
            reinterpret_cast<IMP>(appkit_scene_window_will_close),
            "v@:@");
        objc_registerClassPair(delegate_class);
    }
    g_appkit_scene_window_delegate = objc_send<id>(
        objc_send<id>(reinterpret_cast<id>(delegate_class), sel("alloc")),
        sel("init"));
    return g_appkit_scene_window_delegate;
}

inline void configure_appkit_scene_window_host(
        AppKitSceneWindowState& state,
        platform_api const& platform) {
    state.content_view = objc_send<id>(state.window, sel("contentView"));
    state.surface.kind = NativeSurfaceKind::MacOSWindow;
    state.surface.window = state.window;
    state.surface.view = state.content_view;
    state.surface.window_chrome = state.window_options.chrome;
    state.surface.integrated_titlebar = state.window_options.integrated_titlebar;
    state.surface.native_backdrop_material =
        state.window_options.native_backdrop_material;
    state.surface.native_backdrop_opacity =
        state.window_options.native_backdrop_opacity;
    state.surface.window_options_valid = true;

    state.host.window = &state.surface;
    state.host.surface_descriptor = &state.surface;
    state.host.platform = &platform;
    state.host.set_hover_cursor = &appkit_set_hover_cursor;
    state.host.render_surface_id = state.surface_id;
    state.host.render_scene_id = state.scene_id;
    state.host.render_surface_title = state.title;
    state.host.render_surface_role = state.surface_role;
    state.host.render_surface_visible = state.visible;
    sync_appkit_surface(state.host, state.surface, state.window, false);
}

inline void sync_appkit_scene_window_options(
        AppKitSceneWindowState& state,
        AppKitSceneWindowOptions const& options,
        platform_api const& platform) {
    state.title = safe_preferences_text(options.title, "Window");
    state.width = options.width > 0 ? options.width : 640;
    state.height = options.height > 0 ? options.height : 420;
    state.scene_id = options.scene_id && options.scene_id[0] != '\0'
        ? std::string{options.scene_id}
        : default_scene_id_for_window(state.identifier);
    state.surface_id = options.surface_id && options.surface_id[0] != '\0'
        ? std::string{options.surface_id}
        : default_surface_id_for_window(state.identifier);
    state.scene_role = options.scene_role;
    state.surface_role = options.surface_role;
    state.window_options = options.window_options;
    state.user_data = options.user_data;
    state.on_close = options.on_close;
    if (state.window) {
        objc_send<void>(state.window, sel("setTitle:"), ns_string(state.title.c_str()));
        CGSize content_size{
            static_cast<double>(state.width),
            static_cast<double>(state.height),
        };
        objc_send<void>(state.window, sel("setContentSize:"), content_size);
    }
    configure_appkit_scene_window_host(state, platform);
}

inline AppKitSceneWindowState* ensure_appkit_scene_window(
        AppKitSceneWindowOptions const& options,
        platform_api const& platform) {
    auto identifier = scene_window_identifier_or_default(options.identifier);
    auto* state = find_appkit_scene_window(identifier);
    if (!state) {
        g_appkit_scene_windows.push_back(
            std::make_unique<AppKitSceneWindowState>());
        state = g_appkit_scene_windows.back().get();
        state->identifier = identifier;
    }

    state->title = safe_preferences_text(options.title, "Window");
    state->width = options.width > 0 ? options.width : 640;
    state->height = options.height > 0 ? options.height : 420;
    state->window_options = options.window_options;
    if (!state->window) {
        state->window = create_appkit_window(
            state->width,
            state->height,
            state->title.c_str(),
            state->window_options);
        if (!state->window)
            return nullptr;
        objc_send<void>(
            state->window,
            sel("setDelegate:"),
            create_appkit_scene_window_delegate());
    }
    sync_appkit_scene_window_options(*state, options, platform);
    return state;
}

template<typename State, typename Msg, typename View, typename Update>
    requires std::invocable<View, State const&>
          && std::invocable<Update, State&, Msg>
bool show_appkit_scene_window(platform_api const& platform,
                              AppKitSceneWindowOptions const& options,
                              View view,
                              Update update) {
    auto* state = ensure_appkit_scene_window(options, platform);
    if (!state)
        return false;

    auto scene = ::phenotype::runtime::ensure_scene(::phenotype::SceneDescriptor{
        .id = state->scene_id,
        .title = state->title,
        .role = state->scene_role,
        .visible = true,
    });
    if (!state->runner_installed) {
        ScopedHostActivation activate(state->host);
        run_host_scene<State, Msg>(
            state->host,
            scene,
            std::move(view),
            std::move(update));
        state->runner_installed = true;
    } else {
        ScopedHostActivation activate(state->host);
        ::phenotype::runtime::trigger_scene_rebuild(scene);
    }

    if (options.order_front) {
        id app = objc_send<id>(class_id("NSApplication"), sel("sharedApplication"));
        activate_current_running_application();
        if (app) {
            objc_send<void>(
                app,
                sel("activateIgnoringOtherApps:"),
                static_cast<signed char>(1));
        }
        objc_send<void>(state->window, sel("makeKeyAndOrderFront:"), nullptr);
        objc_send<void>(state->window, sel("orderFrontRegardless"));
    }
    refresh_appkit_scene_window_visibility(*state);
    return true;
}

template<typename State, typename Msg, typename View, typename Update>
    requires std::invocable<View, State const&>
          && std::invocable<Update, State&, Msg>
bool show_appkit_scene_window_with_state(platform_api const& platform,
                                         AppKitSceneWindowOptions const& options,
                                         State& app_state,
                                         View view,
                                         Update update) {
    auto* state = ensure_appkit_scene_window(options, platform);
    if (!state)
        return false;

    auto scene = ::phenotype::runtime::ensure_scene(::phenotype::SceneDescriptor{
        .id = state->scene_id,
        .title = state->title,
        .role = state->scene_role,
        .visible = true,
    });
    if (!state->runner_installed) {
        ScopedHostActivation activate(state->host);
        run_host_scene_with_state<State, Msg>(
            state->host,
            scene,
            app_state,
            std::move(view),
            std::move(update));
        state->runner_installed = true;
    } else {
        ScopedHostActivation activate(state->host);
        ::phenotype::runtime::trigger_scene_rebuild(scene);
    }

    if (options.order_front) {
        id app = objc_send<id>(class_id("NSApplication"), sel("sharedApplication"));
        activate_current_running_application();
        if (app) {
            objc_send<void>(
                app,
                sel("activateIgnoringOtherApps:"),
                static_cast<signed char>(1));
        }
        objc_send<void>(state->window, sel("makeKeyAndOrderFront:"), nullptr);
        objc_send<void>(state->window, sel("orderFrontRegardless"));
    }
    refresh_appkit_scene_window_visibility(*state);
    return true;
}

inline bool is_appkit_scene_window_visible(char const* identifier) {
    auto* state = find_appkit_scene_window(
        scene_window_identifier_or_default(identifier));
    return state && state->visible && appkit_window_is_visible(state->window);
}

inline void close_appkit_scene_window(char const* identifier) {
    auto* state = find_appkit_scene_window(
        scene_window_identifier_or_default(identifier));
    if (!state || !state->window)
        return;
    mark_appkit_scene_window_visibility(*state, false);
    objc_send<void>(state->window, sel("close"));
}

inline void close_all_appkit_scene_windows() {
    for (auto& state : g_appkit_scene_windows) {
        if (!state || !state->window)
            continue;
        mark_appkit_scene_window_visibility(*state, false);
        objc_send<void>(state->window, sel("close"));
    }
}

inline std::size_t appkit_scene_window_count() {
    return g_appkit_scene_windows.size();
}

inline void reset_appkit_scene_windows_for_tests() {
    close_all_appkit_scene_windows();
    for (auto& state : g_appkit_scene_windows) {
        if (state)
            shutdown_host(state->host);
    }
    g_appkit_scene_windows.clear();
}

inline bool visible_appkit_scene_window_is_front() {
    for (auto& state : g_appkit_scene_windows) {
        if (!state || !state->visible
            || !appkit_window_is_visible(state->window)) {
            continue;
        }
        if (appkit_window_is_key(state->window)
            || appkit_window_is_main(state->window))
            return true;
    }
    return false;
}

inline void raise_visible_appkit_scene_windows() {
    for (auto& state : g_appkit_scene_windows) {
        if (!state || !state->visible
            || !appkit_window_is_visible(state->window)) {
            continue;
        }
        if (appkit_window_is_key(state->window)
            || appkit_window_is_main(state->window))
            continue;
        objc_send<void>(state->window, sel("makeKeyAndOrderFront:"), nullptr);
        objc_send<void>(state->window, sel("orderFrontRegardless"));
    }
}

inline AppKitPreferencesWindowState* find_appkit_preferences_window(
        std::string_view identifier) {
    for (auto& state : g_appkit_preferences_windows) {
        if (state.identifier == identifier)
            return &state;
    }
    return nullptr;
}

inline AppKitPreferencesSectionState* find_appkit_preferences_section_by_tag(
        int tag,
        AppKitPreferencesWindowState** owner = nullptr) {
    for (auto& window : g_appkit_preferences_windows) {
        for (auto& section : window.sections) {
            if (section.control_tag == tag) {
                if (owner)
                    *owner = &window;
                return &section;
            }
        }
    }
    return nullptr;
}

inline void copy_appkit_preferences_options(
        AppKitPreferencesWindowState& state,
        NativePreferencesWindowOptions const& options) {
    state.title = safe_preferences_text(options.title, "Settings");
    state.width = options.width > 0 ? options.width : 420;
    state.height = options.height > 0 ? options.height : 300;
    state.appearance = safe_preferences_text(options.appearance, "system");
    state.user_data = options.user_data;
    state.on_close = options.on_close;
    state.sections.clear();
    state.title_label = nullptr;
    for (std::size_t i = 0; i < options.section_count; ++i) {
        auto const& input = options.sections[i];
        AppKitPreferencesSectionState section{};
        section.title = safe_preferences_text(input.title);
        section.subtitle = safe_preferences_text(input.subtitle);
        section.on_select = input.on_select;
        section.control_tag = g_appkit_preferences_next_tag++;
        for (std::size_t c = 0; c < input.choice_count; ++c) {
            auto const& choice = input.choices[c];
            section.choices.push_back(AppKitPreferencesChoiceState{
                .label = safe_preferences_text(choice.label),
                .value = safe_preferences_text(choice.value),
                .selected = choice.selected,
            });
        }
        state.sections.push_back(std::move(section));
    }
}

inline bool appkit_preferences_structure_matches(
        AppKitPreferencesWindowState const& state,
        NativePreferencesWindowOptions const& options) {
    int const width = options.width > 0 ? options.width : 420;
    int const height = options.height > 0 ? options.height : 300;
    if (state.width != width || state.height != height)
        return false;
    if (state.sections.size() != options.section_count)
        return false;
    if (options.section_count > 0 && !options.sections)
        return false;
    for (std::size_t i = 0; i < options.section_count; ++i) {
        auto const& input = options.sections[i];
        auto const& section = state.sections[i];
        if (section.title != safe_preferences_text(input.title)
            || section.subtitle != safe_preferences_text(input.subtitle)
            || section.choices.size() != input.choice_count) {
            return false;
        }
        if (input.choice_count > 0 && !input.choices)
            return false;
        for (std::size_t c = 0; c < input.choice_count; ++c) {
            auto const& choice = input.choices[c];
            auto const& current = section.choices[c];
            if (current.label != safe_preferences_text(choice.label)
                || current.value != safe_preferences_text(choice.value)) {
                return false;
            }
        }
    }
    return true;
}

inline void sync_appkit_preferences_options(
        AppKitPreferencesWindowState& state,
        NativePreferencesWindowOptions const& options) {
    state.title = safe_preferences_text(options.title, "Settings");
    state.appearance = safe_preferences_text(options.appearance, "system");
    state.user_data = options.user_data;
    state.on_close = options.on_close;
    for (std::size_t i = 0; i < state.sections.size(); ++i) {
        auto const& input = options.sections[i];
        auto& section = state.sections[i];
        section.on_select = input.on_select;
        for (std::size_t c = 0; c < section.choices.size(); ++c)
            section.choices[c].selected = input.choices[c].selected;
    }
}

inline bool appkit_preferences_options_well_formed(
        NativePreferencesWindowOptions const& options) {
    if (options.section_count > 0 && !options.sections)
        return false;
    for (std::size_t i = 0; i < options.section_count; ++i) {
        auto const& section = options.sections[i];
        if (section.choice_count > 0 && !section.choices)
            return false;
    }
    return true;
}

inline int selected_appkit_preferences_choice(
        AppKitPreferencesSectionState const& section) {
    for (std::size_t i = 0; i < section.choices.size(); ++i) {
        if (section.choices[i].selected)
            return static_cast<int>(i);
    }
    return section.choices.empty() ? -1 : 0;
}

inline id appkit_preferences_label(std::string_view text,
                                   CGRect frame,
                                   bool heading) {
    id label = objc_send<id>(
        class_id("NSTextField"),
        sel("labelWithString:"),
        ns_string(std::string{text}.c_str()));
    if (!label)
        return nullptr;
    objc_send<void>(label, sel("setFrame:"), frame);
    id font = objc_send<id>(
        class_id("NSFont"),
        heading ? sel("boldSystemFontOfSize:") : sel("systemFontOfSize:"),
        heading ? 13.0 : 12.0);
    if (font)
        objc_send<void>(label, sel("setFont:"), font);
    if (!heading) {
        id color = objc_send<id>(class_id("NSColor"), sel("secondaryLabelColor"));
        if (color)
            objc_send<void>(label, sel("setTextColor:"), color);
    }
    return label;
}

inline void remove_all_appkit_preferences_subviews(id content) {
    if (!content)
        return;
    while (true) {
        id subviews = objc_send<id>(content, sel("subviews"));
        auto const count = subviews
            ? objc_send<unsigned long>(subviews, sel("count"))
            : 0ul;
        if (count == 0)
            break;
        id view = objc_send<id>(subviews, sel("objectAtIndex:"), count - 1ul);
        if (!view)
            break;
        objc_send<void>(view, sel("removeFromSuperview"));
    }
}

inline id appkit_preferences_appearance(std::string_view appearance) {
    char const* name = nullptr;
    if (appearance == "dark")
        name = "NSAppearanceNameDarkAqua";
    else if (appearance == "light")
        name = "NSAppearanceNameAqua";
    if (!name)
        return nullptr;
    return objc_send<id>(
        class_id("NSAppearance"),
        sel("appearanceNamed:"),
        ns_string(name));
}

inline void apply_appkit_preferences_appearance(
        AppKitPreferencesWindowState const& state) {
    if (!state.window)
        return;
    id appearance = appkit_preferences_appearance(state.appearance);
    objc_send<void>(
        state.window,
        sel("setAppearance:"),
        appearance ? appearance : static_cast<id>(nullptr));
}

inline id create_appkit_preferences_window(
        AppKitPreferencesWindowState const& state) {
    CGRect frame{};
    frame.size.width = state.width;
    frame.size.height = state.height;
    constexpr unsigned long titled = 1ul << 0;
    constexpr unsigned long closable = 1ul << 1;
    constexpr unsigned long miniaturizable = 1ul << 2;
    id window = objc_send<id>(
        objc_send<id>(class_id("NSWindow"), sel("alloc")),
        sel("initWithContentRect:styleMask:backing:defer:"),
        frame,
        titled | closable | miniaturizable,
        static_cast<unsigned long>(2),
        static_cast<signed char>(0));
    if (!window)
        return nullptr;
    objc_send<void>(window, sel("setReleasedWhenClosed:"), static_cast<signed char>(0));
    objc_send<void>(window, sel("setTitle:"), ns_string(state.title.c_str()));
    id background = objc_send<id>(class_id("NSColor"), sel("windowBackgroundColor"));
    if (background)
        objc_send<void>(window, sel("setBackgroundColor:"), background);
    return window;
}

inline id create_appkit_preferences_delegate();

inline void rebuild_appkit_preferences_content(AppKitPreferencesWindowState& state) {
    if (!state.window)
        return;
    ++state.content_rebuild_count;
    objc_send<void>(state.window, sel("setTitle:"), ns_string(state.title.c_str()));
    CGSize content_size{
        static_cast<double>(state.width),
        static_cast<double>(state.height),
    };
    objc_send<void>(state.window, sel("setContentSize:"), content_size);
    apply_appkit_preferences_appearance(state);
    state.content_view = objc_send<id>(state.window, sel("contentView"));
    state.title_label = nullptr;
    for (auto& section : state.sections) {
        section.title_label = nullptr;
        section.segmented_control = nullptr;
        section.subtitle_label = nullptr;
    }
    remove_all_appkit_preferences_subviews(state.content_view);

    double const pad = 24.0;
    double y = static_cast<double>(state.height) - pad - 28.0;
    CGRect title_frame{{pad, y}, {state.width - pad * 2.0, 24.0}};
    id title = appkit_preferences_label(state.title, title_frame, true);
    if (title) {
        state.title_label = title;
        objc_send<void>(state.content_view, sel("addSubview:"), title);
    }
    y -= 42.0;

    for (auto& section : state.sections) {
        CGRect label_frame{{pad, y + 5.0}, {120.0, 20.0}};
        id section_label =
            appkit_preferences_label(section.title, label_frame, true);
        if (section_label) {
            section.title_label = section_label;
            objc_send<void>(state.content_view, sel("addSubview:"), section_label);
        }

        CGRect segment_frame{{154.0, y}, {state.width - 178.0, 28.0}};
        id segmented = objc_send<id>(
            objc_send<id>(class_id("NSSegmentedControl"), sel("alloc")),
            sel("initWithFrame:"),
            segment_frame);
        if (segmented) {
            objc_send<void>(
                segmented,
                sel("setSegmentCount:"),
                static_cast<long>(section.choices.size()));
            for (std::size_t i = 0; i < section.choices.size(); ++i) {
                objc_send<void>(
                    segmented,
                    sel("setLabel:forSegment:"),
                    ns_string(section.choices[i].label.c_str()),
                    static_cast<long>(i));
            }
            objc_send<void>(
                segmented,
                sel("setSelectedSegment:"),
                static_cast<long>(selected_appkit_preferences_choice(section)));
            objc_send<void>(segmented, sel("setTag:"), static_cast<long>(section.control_tag));
            objc_send<void>(
                segmented,
                sel("setTarget:"),
                create_appkit_preferences_delegate());
            objc_send<void>(
                segmented,
                sel("setAction:"),
                sel("phenotypePreferencesSegmentChanged:"));
            section.segmented_control = segmented;
            objc_send<void>(state.content_view, sel("addSubview:"), segmented);
        }

        if (!section.subtitle.empty()) {
            CGRect subtitle_frame{{154.0, y - 22.0}, {state.width - 178.0, 18.0}};
            id subtitle =
                appkit_preferences_label(section.subtitle, subtitle_frame, false);
            if (subtitle) {
                section.subtitle_label = subtitle;
                objc_send<void>(state.content_view, sel("addSubview:"), subtitle);
            }
            y -= 76.0;
        } else {
            y -= 58.0;
        }
    }
}

inline void sync_appkit_preferences_content(AppKitPreferencesWindowState& state) {
    if (!state.window)
        return;
    objc_send<void>(state.window, sel("setTitle:"), ns_string(state.title.c_str()));
    apply_appkit_preferences_appearance(state);
    if (state.title_label) {
        objc_send<void>(
            state.title_label,
            sel("setStringValue:"),
            ns_string(state.title.c_str()));
    }
    for (auto& section : state.sections) {
        if (!section.segmented_control)
            continue;
        objc_send<void>(
            section.segmented_control,
            sel("setSelectedSegment:"),
            static_cast<long>(selected_appkit_preferences_choice(section)));
    }
}

inline void appkit_preferences_segment_changed(id, SEL, id sender) {
    if (!sender)
        return;
    auto const tag = static_cast<int>(objc_send<long>(sender, sel("tag")));
    auto const selected =
        static_cast<int>(objc_send<long>(sender, sel("selectedSegment")));
    AppKitPreferencesWindowState* owner = nullptr;
    auto* section = find_appkit_preferences_section_by_tag(tag, &owner);
    if (!section || !owner || selected < 0
        || static_cast<std::size_t>(selected) >= section->choices.size()) {
        return;
    }
    for (std::size_t i = 0; i < section->choices.size(); ++i)
        section->choices[i].selected = static_cast<int>(i) == selected;
    if (section->on_select)
        section->on_select(section->choices[selected].value.c_str(),
                           owner->user_data);
    ::phenotype::detail::trigger_rebuild();
}

inline void appkit_preferences_window_will_close(id, SEL, id notification) {
    id window = notification
        ? objc_send<id>(notification, sel("object"))
        : nullptr;
    for (auto& state : g_appkit_preferences_windows) {
        if (state.window != window)
            continue;
        bool const was_visible = state.visible;
        state.visible = false;
        if (was_visible && state.on_close)
            state.on_close(state.user_data);
        ::phenotype::detail::trigger_rebuild();
        break;
    }
}

inline id create_appkit_preferences_delegate() {
    if (g_appkit_preferences_delegate)
        return g_appkit_preferences_delegate;
    static Class delegate_class = nullptr;
    if (!delegate_class) {
        Class base = reinterpret_cast<Class>(class_id("NSObject"));
        delegate_class = objc_allocateClassPair(
            base,
            "PhenotypeAppKitPreferencesWindowDelegate",
            0);
        if (!delegate_class)
            return nullptr;
        class_addMethod(
            delegate_class,
            sel("phenotypePreferencesSegmentChanged:"),
            reinterpret_cast<IMP>(appkit_preferences_segment_changed),
            "v@:@");
        class_addMethod(
            delegate_class,
            sel("windowWillClose:"),
            reinterpret_cast<IMP>(appkit_preferences_window_will_close),
            "v@:@");
        objc_registerClassPair(delegate_class);
    }
    g_appkit_preferences_delegate = objc_send<id>(
        objc_send<id>(reinterpret_cast<id>(delegate_class), sel("alloc")),
        sel("init"));
    return g_appkit_preferences_delegate;
}

inline bool apply_appkit_preferences_window_options(
        NativePreferencesWindowOptions const& options,
        bool order_front) {
    if (!appkit_preferences_options_well_formed(options))
        return false;
    auto identifier = safe_preferences_text(options.identifier, "preferences");
    if (identifier.empty())
        identifier = "preferences";
    auto* state = find_appkit_preferences_window(identifier);
    if (!state) {
        g_appkit_preferences_windows.push_back(AppKitPreferencesWindowState{});
        state = &g_appkit_preferences_windows.back();
        state->identifier = identifier;
    }
    bool const reuse_content = state->window
        && appkit_preferences_structure_matches(*state, options);
    if (reuse_content)
        sync_appkit_preferences_options(*state, options);
    else
        copy_appkit_preferences_options(*state, options);
    if (!state->window) {
        state->window = create_appkit_preferences_window(*state);
        if (!state->window)
            return false;
        objc_send<void>(
            state->window,
            sel("setDelegate:"),
            create_appkit_preferences_delegate());
        objc_send<void>(state->window, sel("center"));
    }
    if (reuse_content)
        sync_appkit_preferences_content(*state);
    else
        rebuild_appkit_preferences_content(*state);
    state->visible = true;

    if (order_front) {
        id app = objc_send<id>(class_id("NSApplication"), sel("sharedApplication"));
        activate_current_running_application();
        if (app) {
            objc_send<void>(
                app,
                sel("activateIgnoringOtherApps:"),
                static_cast<signed char>(1));
        }
        objc_send<void>(state->window, sel("makeKeyAndOrderFront:"), nullptr);
        objc_send<void>(state->window, sel("orderFrontRegardless"));
    }
    return true;
}

inline bool show_appkit_preferences_window(
        NativePreferencesWindowOptions const& options) {
    return apply_appkit_preferences_window_options(options, true);
}

inline bool sync_appkit_preferences_window(
        NativePreferencesWindowOptions const& options) {
    if (!appkit_preferences_options_well_formed(options))
        return false;
    auto identifier = safe_preferences_text(options.identifier, "preferences");
    if (identifier.empty())
        identifier = "preferences";
    auto* state = find_appkit_preferences_window(identifier);
    if (!state || !state->window || !state->visible
        || !appkit_window_is_visible(state->window)) {
        return false;
    }
    bool const reuse_content = appkit_preferences_structure_matches(*state, options);
    if (reuse_content)
        sync_appkit_preferences_options(*state, options);
    else
        copy_appkit_preferences_options(*state, options);
    if (reuse_content)
        sync_appkit_preferences_content(*state);
    else
        rebuild_appkit_preferences_content(*state);
    state->visible = true;
    return true;
}

inline bool is_appkit_preferences_window_visible(char const* identifier) {
    auto* state = find_appkit_preferences_window(
        safe_preferences_text(identifier, "preferences"));
    return state && state->visible && appkit_window_is_visible(state->window);
}

inline void close_appkit_preferences_window(char const* identifier) {
    auto* state = find_appkit_preferences_window(
        safe_preferences_text(identifier, "preferences"));
    if (!state || !state->window)
        return;
    state->visible = false;
    objc_send<void>(state->window, sel("close"));
}

inline std::size_t appkit_preferences_window_count() {
    return g_appkit_preferences_windows.size();
}

inline unsigned long long appkit_preferences_content_rebuild_count(
        char const* identifier) {
    auto* state = find_appkit_preferences_window(
        safe_preferences_text(identifier, "preferences"));
    return state ? state->content_rebuild_count : 0ull;
}

inline bool visible_appkit_preferences_window_is_front() {
    for (auto& state : g_appkit_preferences_windows) {
        if (state.visible
            && appkit_window_is_visible(state.window)
            && (appkit_window_is_key(state.window)
                || appkit_window_is_main(state.window))) {
            return true;
        }
    }
    return false;
}

inline void raise_visible_appkit_preferences_windows() {
    for (auto& state : g_appkit_preferences_windows) {
        if (!state.visible || !appkit_window_is_visible(state.window))
            continue;
        if (appkit_window_is_key(state.window)
            || appkit_window_is_main(state.window))
            continue;
        objc_send<void>(state.window, sel("makeKeyAndOrderFront:"), nullptr);
        objc_send<void>(state.window, sel("orderFrontRegardless"));
    }
}

inline void request_appkit_window_front(id app, id window) {
    if (!window)
        return;

    g_appkit_window_user_closed = false;
    // Activation is asynchronous for command-line AppKit apps. Request app
    // focus and window key/main status as a single idempotent publish step so
    // the startup path and retry loop cannot drift apart.
    activate_current_running_application();
    if (app) {
        objc_send<void>(app, sel("unhide:"), nullptr);
        objc_send<void>(app, sel("activate"));
        objc_send<void>(
            app,
            sel("activateIgnoringOtherApps:"),
            static_cast<signed char>(1));
    }
    objc_send<void>(window, sel("makeKeyAndOrderFront:"), nullptr);
    objc_send<void>(window, sel("orderFrontRegardless"));
    raise_visible_appkit_preferences_windows();
    raise_visible_appkit_scene_windows();
}

inline void schedule_appkit_window_front_request(id app,
                                                 id window,
                                                 int attempts = 18) {
    if (!window)
        return;
    if (attempts > g_appkit_front_request_attempts)
        g_appkit_front_request_attempts = attempts;
    request_appkit_window_front(app, window);
}

inline signed char appkit_should_terminate_after_last_window_closed(
        id,
        SEL,
        id) {
    return static_cast<signed char>(
        g_appkit_keep_running_after_last_window_closed ? 0 : 1);
}

inline long appkit_should_terminate(id, SEL, id) {
    g_appkit_should_terminate = true;
    return 1; // NSTerminateNow
}

inline signed char appkit_window_should_close(id, SEL, id) {
    g_appkit_window_user_closed = true;
    return static_cast<signed char>(1);
}

inline void appkit_window_will_close(id, SEL, id) {
    g_appkit_window_user_closed = true;
}

inline signed char appkit_should_handle_reopen(id, SEL, id app, signed char) {
    g_appkit_window_user_closed = false;
    schedule_appkit_window_front_request(app, g_active_appkit_window);
    if (app)
        objc_send<void>(app, sel("stop:"), nullptr);
    return static_cast<signed char>(1);
}

inline void appkit_did_become_active(id, SEL, id) {
    if (g_appkit_window_user_closed)
        return;
    id app = objc_send<id>(class_id("NSApplication"), sel("sharedApplication"));
    schedule_appkit_window_front_request(app, g_active_appkit_window, 8);
}

inline void appkit_open_settings(id, SEL, id) {
    id app = objc_send<id>(class_id("NSApplication"), sel("sharedApplication"));
    g_appkit_window_user_closed = false;
    schedule_appkit_window_front_request(app, g_active_appkit_window);
    if (g_appkit_settings_menu_handler) {
        g_appkit_settings_menu_handler();
        ::phenotype::detail::trigger_rebuild();
    }
}

inline void appkit_toggle_debug_panel(id, SEL, id) {
    (void)dispatch_debug_panel_shortcut(
        Key::F12,
        KeyAction::Press,
        debug_panel_shortcut_modifiers(),
        "f12");
}

inline bool appkit_handle_standard_key_equivalent(id event) {
    if (!event)
        return false;
    auto const flags = objc_send<unsigned long>(event, sel("modifierFlags"));
    if (appkit_dispatch_debug_panel_shortcut_from_event(event))
        return true;
    if (!g_appkit_settings_menu_handler)
        return false;
    constexpr unsigned long command_modifier = 1ul << 20;
    if ((flags & command_modifier) == 0)
        return false;
    if (objc_send<unsigned short>(event, sel("keyCode")) != 43)
        return false;
    appkit_open_settings(nullptr, nullptr, nullptr);
    return true;
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
            sel("applicationShouldTerminateAfterLastWindowClosed:"),
            reinterpret_cast<IMP>(appkit_should_terminate_after_last_window_closed),
            "c@:@");
        class_addMethod(
            delegate_class,
            sel("applicationShouldTerminate:"),
            reinterpret_cast<IMP>(appkit_should_terminate),
            "q@:@");
        class_addMethod(
            delegate_class,
            sel("windowShouldClose:"),
            reinterpret_cast<IMP>(appkit_window_should_close),
            "c@:@");
        class_addMethod(
            delegate_class,
            sel("windowWillClose:"),
            reinterpret_cast<IMP>(appkit_window_will_close),
            "v@:@");
        class_addMethod(
            delegate_class,
            sel("applicationShouldHandleReopen:hasVisibleWindows:"),
            reinterpret_cast<IMP>(appkit_should_handle_reopen),
            "c@:@c");
        class_addMethod(
            delegate_class,
            sel("applicationDidBecomeActive:"),
            reinterpret_cast<IMP>(appkit_did_become_active),
            "v@:@");
        class_addMethod(
            delegate_class,
            sel("phenotypeOpenSettings:"),
            reinterpret_cast<IMP>(appkit_open_settings),
            "v@:@");
        class_addMethod(
            delegate_class,
            sel("phenotypeToggleDebugPanel:"),
            reinterpret_cast<IMP>(appkit_toggle_debug_panel),
            "v@:@");
        objc_registerClassPair(delegate_class);
    }
    return objc_send<id>(
        objc_send<id>(reinterpret_cast<id>(delegate_class), sel("alloc")),
        sel("init"));
}

inline id g_appkit_shell_delegate = nullptr;

inline id appkit_menu_item(char const* title,
                           SEL action,
                           char const* key_equivalent,
                           id target = nullptr) {
    id item = objc_send<id>(
        objc_send<id>(class_id("NSMenuItem"), sel("alloc")),
        sel("initWithTitle:action:keyEquivalent:"),
        ns_string(title),
        action,
        ns_string(key_equivalent ? key_equivalent : ""));
    if (item && target)
        objc_send<void>(item, sel("setTarget:"), target);
    if (item && key_equivalent && key_equivalent[0] != '\0') {
        constexpr unsigned long command_modifier = 1ul << 20;
        objc_send<void>(
            item,
            sel("setKeyEquivalentModifierMask:"),
            command_modifier);
    }
    return item;
}

inline void install_standard_appkit_menu(id app,
                                         WindowOptions const& options) {
    if (!app || !options.install_standard_app_menu)
        return;

    char const* app_name = options.application_name;
    if (!app_name || app_name[0] == '\0')
        app_name = g_appkit_application_name.c_str();

    id menubar = objc_send<id>(
        objc_send<id>(class_id("NSMenu"), sel("alloc")),
        sel("initWithTitle:"),
        ns_string(""));
    id app_item = appkit_menu_item("", nullptr, "");
    if (!menubar || !app_item)
        return;
    objc_send<void>(menubar, sel("addItem:"), app_item);
    objc_send<void>(app, sel("setMainMenu:"), menubar);

    id app_menu = objc_send<id>(
        objc_send<id>(class_id("NSMenu"), sel("alloc")),
        sel("initWithTitle:"),
        ns_string(app_name));
    if (!app_menu)
        return;
    objc_send<void>(menubar, sel("setSubmenu:forItem:"), app_menu, app_item);

#ifndef NDEBUG
    id debug = appkit_menu_item(
        "Toggle Debug Panel",
        sel("phenotypeToggleDebugPanel:"),
        "\xEF\x9C\x8F",
        g_appkit_shell_delegate);
    if (debug)
        objc_send<void>(app_menu, sel("addItem:"), debug);
#endif

    if (options.on_settings_menu) {
        id settings = appkit_menu_item(
            options.settings_menu_title ? options.settings_menu_title : "Settings...",
            sel("phenotypeOpenSettings:"),
            options.settings_menu_key_equivalent
                ? options.settings_menu_key_equivalent
                : ",",
            g_appkit_shell_delegate);
        if (settings)
            objc_send<void>(app_menu, sel("addItem:"), settings);
        id separator = objc_send<id>(class_id("NSMenuItem"), sel("separatorItem"));
        if (separator)
            objc_send<void>(app_menu, sel("addItem:"), separator);
    }

    auto quit_title = std::string{"Quit "} + app_name;
    id quit = appkit_menu_item(
        quit_title.c_str(),
        sel("terminate:"),
        "q",
        app);
    if (quit)
        objc_send<void>(app_menu, sel("addItem:"), quit);
}

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

inline void run_appkit_slice(id app, double timeout_seconds) {
    if (!app)
        return;
    objc_send<void>(
        app,
        sel("performSelector:withObject:afterDelay:"),
        sel("stop:"),
        nullptr,
        timeout_seconds);
    objc_send<void>(app, sel("run"));
}

inline bool should_run_appkit_activation_slice(bool visible,
                                               bool user_closed,
                                               bool app_active) noexcept {
    return visible && !user_closed && !app_active;
}

inline bool should_request_appkit_window_front(bool visible,
                                               bool user_closed,
                                               bool app_active,
                                               bool key_window,
                                               bool main_window,
                                               bool auxiliary_front_window = false) noexcept {
    return visible
        && !user_closed
        && app_active
        && !auxiliary_front_window
        && (!key_window || !main_window);
}

inline bool appkit_window_front_ready(id app, id window);

inline bool appkit_window_or_auxiliary_front_ready(id app, id window) {
    return appkit_window_front_ready(app, window)
        || (appkit_app_is_active(app)
            && appkit_window_is_visible(window)
            && (visible_appkit_preferences_window_is_front()
                || visible_appkit_scene_window_is_front()));
}

inline void service_appkit_activation_reopen(id app, id window, bool visible) {
    if (g_appkit_front_request_attempts > 0 && visible
        && !g_appkit_window_user_closed) {
        request_appkit_window_front(app, window);
        run_appkit_slice(app, 0.016);
        if (appkit_window_or_auxiliary_front_ready(app, window))
            g_appkit_front_request_attempts = 0;
        else
            --g_appkit_front_request_attempts;
        return;
    }

    if (!should_run_appkit_activation_slice(
            visible,
            g_appkit_window_user_closed,
            appkit_app_is_active(app))) {
        if (should_request_appkit_window_front(
                visible,
                g_appkit_window_user_closed,
                appkit_app_is_active(app),
                appkit_window_is_key(window),
                appkit_window_is_main(window),
                visible_appkit_preferences_window_is_front()
                    || visible_appkit_scene_window_is_front())) {
            request_appkit_window_front(app, window);
        }
        return;
    }

    run_appkit_slice(app, 0.016);
    if (should_request_appkit_window_front(
            appkit_window_is_visible(window),
            g_appkit_window_user_closed,
            appkit_app_is_active(app),
            appkit_window_is_key(window),
            appkit_window_is_main(window),
            visible_appkit_preferences_window_is_front()
                || visible_appkit_scene_window_is_front())) {
        request_appkit_window_front(app, window);
    }
}

inline id next_appkit_event(id app, double default_timeout_seconds) {
    if (!app)
        return nullptr;

    return objc_send<id>(
        app,
        sel("nextEventMatchingMask:untilDate:inMode:dequeue:"),
        ~0ul,
        ns_date_after(default_timeout_seconds),
        ns_string(g_appkit_mouse_tracking_mode
            ? "NSEventTrackingRunLoopMode"
            : "NSDefaultRunLoopMode"),
        static_cast<signed char>(1));
}

inline void update_appkit_mouse_tracking_mode_before_send(id event) {
    if (!event)
        return;
    auto const type = objc_send<unsigned long>(event, sel("type"));
    if (type == 1 || type == 3)
        g_appkit_mouse_tracking_mode = true;
}

inline void update_appkit_mouse_tracking_mode_after_send(id event) {
    if (!event)
        return;
    auto const type = objc_send<unsigned long>(event, sel("type"));
    if (type == 2 || type == 4)
        g_appkit_mouse_tracking_mode = false;
}

inline void update_appkit_close_button_candidate_before_send(
        id event,
        NativeSurfaceDescriptor const& surface) {
    if (!event)
        return;
    auto const type = objc_send<unsigned long>(event, sel("type"));
    if (type == 1)
        g_appkit_close_button_candidate =
            appkit_event_hits_close_button_fallback(event, surface);
    else if (type == 3)
        g_appkit_close_button_candidate = false;
}

inline void complete_appkit_close_button_after_event(
        id event,
        NativeSurfaceDescriptor const& surface,
        id window) {
    if (!event || !window)
        return;
    auto const type = objc_send<unsigned long>(event, sel("type"));
    if (type != 2)
        return;

    bool const should_close = g_appkit_close_button_candidate
        && appkit_event_hits_close_button_fallback(event, surface);
    g_appkit_close_button_candidate = false;
    if (!should_close)
        return;

    g_appkit_window_user_closed = true;
    if (g_appkit_keep_running_after_last_window_closed) {
        objc_send<void>(window, sel("orderOut:"), nullptr);
        id app = objc_send<id>(
            class_id("NSApplication"),
            sel("sharedApplication"));
        if (app)
            objc_send<void>(app, sel("hide:"), nullptr);
    } else {
        objc_send<void>(window, sel("close"));
    }
    if (!g_appkit_keep_running_after_last_window_closed)
        g_appkit_should_terminate = true;
}

inline void sync_appkit_host_surface(native_host& host,
                                     NativeSurfaceDescriptor& surface,
                                     id window,
                                     bool notify) {
    ScopedHostActivation activate(host);
    sync_appkit_surface(host, surface, window, notify);
}

inline void service_appkit_unhandled_event(id app, id event) {
    if (!event)
        return;
    auto const type = objc_send<unsigned long>(event, sel("type"));
    if (type == 2 || type == 3)
        g_appkit_close_button_candidate = false;
    update_appkit_mouse_tracking_mode_before_send(event);
    objc_send<void>(app, sel("sendEvent:"), event);
    update_appkit_mouse_tracking_mode_after_send(event);
}

inline bool service_appkit_host_event(id app,
                                      id event,
                                      native_host& host,
                                      NativeSurfaceDescriptor const& surface,
                                      id window,
                                      bool visible) {
    if (!event || !appkit_event_targets_window(event, window))
        return false;

    ScopedHostActivation activate(host);
    update_appkit_mouse_tracking_mode_before_send(event);
    update_appkit_close_button_candidate_before_send(event, surface);
    bool const close_button_fallback_event =
        visible
        && (g_appkit_close_button_candidate
            || appkit_event_hits_close_button_fallback(event, surface));
    if (close_button_fallback_event)
        g_appkit_mouse_tracking_mode = false;
    bool const native_titlebar_control_event =
        visible
        && appkit_event_hits_native_titlebar_control_reserve(event, surface);
    if (visible
        && !close_button_fallback_event
        && !native_titlebar_control_event)
        dispatch_appkit_event(event, surface);
    if (!close_button_fallback_event)
        objc_send<void>(app, sel("sendEvent:"), event);
    update_appkit_mouse_tracking_mode_after_send(event);
    complete_appkit_close_button_after_event(event, surface, window);
    return true;
}

inline bool service_appkit_scene_window_event(id app, id event) {
    if (!event)
        return false;
    for (auto& state : g_appkit_scene_windows) {
        if (!state || !state->visible
            || !appkit_window_is_visible(state->window)) {
            continue;
        }
        if (service_appkit_host_event(
                app,
                event,
                state->host,
                state->surface,
                state->window,
                true)) {
            return true;
        }
    }
    return false;
}

inline void service_appkit_scene_windows() {
    for (auto& state : g_appkit_scene_windows) {
        if (!state || !state->visible)
            continue;
        bool const visible = appkit_window_is_visible(state->window);
        if (!visible) {
            mark_appkit_scene_window_visibility(*state, false);
            continue;
        }
        sync_appkit_host_surface(state->host, state->surface, state->window, true);
        service_host_tick(state->host, state->last_animation_tick);
    }
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
    while (!appkit_window_or_auxiliary_front_ready(app, window)) {
        if (std::chrono::steady_clock::now() >= deadline)
            return;
        request_appkit_window_front(app, window);
        pump_appkit_ordering_once(app, 0.016);
    }
}

inline void warn_if_appkit_window_not_front(id app, id window) {
    if (appkit_window_or_auxiliary_front_ready(app, window))
        return;
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

inline std::optional<int> env_positive_int(char const* name, int max_value) {
    auto const* value = std::getenv(name);
    if (!value || value[0] == '\0')
        return std::nullopt;
    int parsed = 0;
    auto const* first = value;
    auto const* last = value + std::char_traits<char>::length(value);
    auto [ptr, ec] = std::from_chars(first, last, parsed);
    if (ec != std::errc{} || ptr != last || parsed <= 0
        || parsed > max_value) {
        std::fprintf(
            stderr,
            "[phenotype-native] ignoring invalid %s=%s\n",
            name,
            value);
        return std::nullopt;
    }
    return parsed;
}

struct PerformanceProbeStats {
    std::string mode;
    int frames = 0;
    int handled_frames = 0;
    int long_frames = 0;
    bool debug_panel = false;
    std::uint64_t total_ns = 0;
    std::uint64_t min_ns = 0;
    std::uint64_t max_ns = 0;
    std::uint64_t p50_ns = 0;
    std::uint64_t p95_ns = 0;
    int pace_hz = 0;
    double average_ns = 0.0;
    double average_fps_capacity = 0.0;
    double p50_fps_capacity = 0.0;
    double p95_fps_capacity = 0.0;
    bool idle_240_required = false;
    bool active_60_required = false;
    bool action_60_required = false;
    bool idle_240_ok = true;
    bool active_60_ok = true;
    bool action_60_ok = true;
};

inline std::uint64_t percentile_from_sorted(
        std::vector<std::uint64_t> const& sorted,
        double percentile) {
    if (sorted.empty())
        return 0;
    auto const index = static_cast<std::size_t>(
        std::lround(percentile * static_cast<double>(sorted.size() - 1)));
    return sorted[std::min(index, sorted.size() - 1)];
}

inline double fps_capacity(std::uint64_t ns) {
    if (ns == 0)
        return 0.0;
    return 1'000'000'000.0 / static_cast<double>(ns);
}

inline std::string performance_probe_json(PerformanceProbeStats const& s) {
    return std::format(
        "{{\n"
        "  \"schema_version\": 1,\n"
        "  \"kind\": \"phenotype.performance_probe\",\n"
        "  \"mode\": \"{}\",\n"
        "  \"frames\": {},\n"
        "  \"handled_frames\": {},\n"
        "  \"long_frames\": {},\n"
        "  \"debug_panel\": {},\n"
        "  \"pace_hz\": {},\n"
        "  \"timing\": {{\n"
        "    \"total_ns\": {},\n"
        "    \"average_ns\": {:.3f},\n"
        "    \"min_ns\": {},\n"
        "    \"max_ns\": {},\n"
        "    \"p50_ns\": {},\n"
        "    \"p95_ns\": {},\n"
        "    \"average_fps_capacity\": {:.3f},\n"
        "    \"p50_fps_capacity\": {:.3f},\n"
        "    \"p95_fps_capacity\": {:.3f}\n"
        "  }},\n"
        "  \"budgets\": {{\n"
        "    \"idle_240_frame_budget_ns\": 4166667,\n"
        "    \"active_60_frame_budget_ns\": 16666667,\n"
        "    \"action_60_frame_budget_ns\": 16666667,\n"
        "    \"idle_240_required\": {},\n"
        "    \"active_60_required\": {},\n"
        "    \"action_60_required\": {},\n"
        "    \"idle_240_ok\": {},\n"
        "    \"active_60_ok\": {},\n"
        "    \"action_60_ok\": {}\n"
        "  }}\n"
        "}}\n",
        s.mode,
        s.frames,
        s.handled_frames,
        s.long_frames,
        s.debug_panel ? "true" : "false",
        s.pace_hz,
        s.total_ns,
        s.average_ns,
        s.min_ns,
        s.max_ns,
        s.p50_ns,
        s.p95_ns,
        s.average_fps_capacity,
        s.p50_fps_capacity,
        s.p95_fps_capacity,
        s.idle_240_required ? "true" : "false",
        s.active_60_required ? "true" : "false",
        s.action_60_required ? "true" : "false",
        s.idle_240_ok ? "true" : "false",
        s.active_60_ok ? "true" : "false",
        s.action_60_ok ? "true" : "false");
}

inline bool write_performance_probe_report(std::string_view report) {
    auto const* out_path = std::getenv("PHENOTYPE_PERF_OUT");
    if (!out_path || out_path[0] == '\0') {
        std::fprintf(stderr, "%.*s", static_cast<int>(report.size()), report.data());
        return true;
    }
    auto file = std::ofstream{out_path, std::ios::binary};
    if (!file) {
        std::fprintf(
            stderr,
            "[phenotype-native] performance report failed: %s\n",
            out_path);
        return false;
    }
    file << report;
    return static_cast<bool>(file);
}

inline bool run_performance_probe_if_requested() {
    auto const frames = env_positive_int("PHENOTYPE_PERF_FRAMES", 1'000'000);
    if (!frames)
        return true;

    auto const* mode_env = std::getenv("PHENOTYPE_PERF_MODE");
    auto mode = std::string{
        mode_env && mode_env[0] != '\0' ? mode_env : "idle"};
    bool const action_mode =
        mode == "hover" || mode == "scroll" || mode == "mixed-input";
    if (mode != "idle" && mode != "rebuild" && mode != "force-flush"
        && !action_mode) {
        std::fprintf(
            stderr,
            "[phenotype-native] unknown PHENOTYPE_PERF_MODE=%s\n",
            mode.c_str());
        return false;
    }
    int pace_hz = 0;
    if (auto configured = env_positive_int("PHENOTYPE_PERF_PACE_HZ", 1000)) {
        pace_hz = *configured;
    } else if (mode == "force-flush" || action_mode) {
        pace_hz = 60;
    }
    auto const pace_duration = pace_hz > 0
        ? std::chrono::nanoseconds{
            static_cast<long long>(1'000'000'000ll / pace_hz)}
        : std::chrono::nanoseconds{0};
    bool const debug_panel = env_flag_enabled("PHENOTYPE_PERF_DEBUG_PANEL");
    if (debug_panel && !::phenotype::detail::g_app().debug_panel_open) {
        ::phenotype::detail::g_app().debug_panel_open = true;
        ::phenotype::detail::trigger_rebuild();
    }

    auto samples = std::vector<std::uint64_t>{};
    samples.reserve(static_cast<std::size_t>(*frames));
    bool const previous_active_animations =
        ::phenotype::detail::g_app().has_active_animations;
    if (mode == "force-flush")
        ::phenotype::detail::g_app().has_active_animations = true;
    int stats_long_frames = 0;
    int stats_handled_frames = 0;
    if (mode == "scroll" || mode == "mixed-input") {
        float const w = shell_state().host ? shell_state().host->canvas_width() : 800.0f;
        float const h = shell_state().host ? shell_state().host->canvas_height() : 600.0f;
        shell_state().last_mouse_x = w * 0.55f;
        shell_state().last_mouse_y = h * 0.55f;
    }

    for (int i = 0; i < *frames; ++i) {
        auto const start = std::chrono::steady_clock::now();
        bool handled = true;
        if (mode == "rebuild") {
            ::phenotype::detail::trigger_rebuild();
        } else if (mode == "hover") {
            float const w = shell_state().host ? shell_state().host->canvas_width() : 800.0f;
            float const h = shell_state().host ? shell_state().host->canvas_height() : 600.0f;
            float const usable_w = std::max(1.0f, w - 96.0f);
            float const usable_h = std::max(1.0f, h - 96.0f);
            float const x = 48.0f + std::fmod(static_cast<float>(i * 37), usable_w);
            float const y = 48.0f + std::fmod(static_cast<float>(i * 29), usable_h);
            handled = dispatch_cursor_pos(x, y);
        } else if (mode == "scroll") {
            double const dy = ((i / 18) % 2 == 0) ? -12.0 : 12.0;
            float const w = shell_state().host ? shell_state().host->canvas_width() : 800.0f;
            float const h = shell_state().host ? shell_state().host->canvas_height() : 600.0f;
            shell_state().last_mouse_x = w * 0.55f;
            shell_state().last_mouse_y = h * 0.55f;
            handled = dispatch_scroll_xy(0.0, dy, false, w, h);
            if (!handled) {
                bool const previous_input_motion =
                    ::phenotype::detail::g_app().has_active_input_motion;
                ::phenotype::detail::g_app().has_active_input_motion = true;
                ::phenotype::detail::g_app().last_paint_hash = 0;
                repaint_current();
                ::phenotype::detail::g_app().has_active_input_motion =
                    previous_input_motion;
            }
        } else if (mode == "mixed-input") {
            float const w = shell_state().host ? shell_state().host->canvas_width() : 800.0f;
            float const h = shell_state().host ? shell_state().host->canvas_height() : 600.0f;
            switch (i % 4) {
                case 0:
                    handled = dispatch_cursor_pos(
                        48.0f + std::fmod(static_cast<float>(i * 41), std::max(1.0f, w - 96.0f)),
                        48.0f + std::fmod(static_cast<float>(i * 31), std::max(1.0f, h - 96.0f)));
                    break;
                case 1:
                    shell_state().last_mouse_x = w * 0.55f;
                    shell_state().last_mouse_y = h * 0.55f;
                    handled = dispatch_scroll_xy(
                        0.0,
                        ((i / 16) % 2 == 0) ? -12.0 : 12.0,
                        false,
                        w,
                        h);
                    if (!handled) {
                        bool const previous_input_motion =
                            ::phenotype::detail::g_app().has_active_input_motion;
                        ::phenotype::detail::g_app().has_active_input_motion = true;
                        ::phenotype::detail::g_app().last_paint_hash = 0;
                        repaint_current();
                        ::phenotype::detail::g_app().has_active_input_motion =
                            previous_input_motion;
                    }
                    break;
                case 2:
                    handled = dispatch_key(Key::Down, KeyAction::Press, 0);
                    break;
                default:
                    handled = dispatch_key(Key::Up, KeyAction::Press, 0);
                    break;
            }
        } else {
            if (mode == "force-flush")
                ::phenotype::detail::g_app().last_paint_hash = 0;
            repaint_current();
        }
        auto const end = std::chrono::steady_clock::now();
        auto const elapsed =
            std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
        samples.push_back(
            static_cast<std::uint64_t>(elapsed.count()));
        if (handled)
            ++stats_handled_frames;
        if (static_cast<std::uint64_t>(elapsed.count()) > 16'666'667ull)
            ++stats_long_frames;
        if (pace_duration.count() > 0 && elapsed < pace_duration)
            std::this_thread::sleep_for(pace_duration - elapsed);
    }
    ::phenotype::detail::g_app().has_active_animations =
        previous_active_animations;

    auto sorted = samples;
    std::ranges::sort(sorted);
    auto stats = PerformanceProbeStats{
        .mode = mode,
        .frames = *frames,
        .handled_frames = stats_handled_frames,
        .long_frames = stats_long_frames,
        .debug_panel = debug_panel,
        .min_ns = sorted.empty() ? 0 : sorted.front(),
        .max_ns = sorted.empty() ? 0 : sorted.back(),
        .p50_ns = percentile_from_sorted(sorted, 0.50),
        .p95_ns = percentile_from_sorted(sorted, 0.95),
        .pace_hz = pace_hz,
        .idle_240_required =
            env_flag_enabled("PHENOTYPE_PERF_REQUIRE_IDLE_240"),
        .active_60_required =
            env_flag_enabled("PHENOTYPE_PERF_REQUIRE_ACTIVE_60"),
        .action_60_required =
            env_flag_enabled("PHENOTYPE_PERF_REQUIRE_ACTION_60"),
    };
    for (auto ns : samples)
        stats.total_ns += ns;
    stats.average_ns = stats.frames > 0
        ? static_cast<double>(stats.total_ns) / static_cast<double>(stats.frames)
        : 0.0;
    stats.average_fps_capacity =
        stats.average_ns > 0.0 ? 1'000'000'000.0 / stats.average_ns : 0.0;
    stats.p50_fps_capacity = fps_capacity(stats.p50_ns);
    stats.p95_fps_capacity = fps_capacity(stats.p95_ns);
    stats.idle_240_ok =
        !stats.idle_240_required || stats.p95_ns <= 4'166'667ull;
    stats.active_60_ok =
        !stats.active_60_required || stats.p95_ns <= 16'666'667ull;
    stats.action_60_ok =
        !stats.action_60_required || stats.p95_ns <= 16'666'667ull;

    auto const report = performance_probe_json(stats);
    auto const wrote = write_performance_probe_report(report);
    return wrote && stats.idle_240_ok && stats.active_60_ok
        && stats.action_60_ok;
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
    g_appkit_keep_running_after_last_window_closed =
        options.keep_running_after_last_window_closed;
    g_appkit_should_terminate = false;
    g_appkit_mouse_tracking_mode = false;
    g_appkit_window_user_closed = false;
    g_appkit_close_button_candidate = false;
    g_appkit_front_request_attempts = 0;
    g_appkit_settings_menu_handler = options.on_settings_menu;
    g_appkit_application_name = options.application_name
        && options.application_name[0] != '\0'
            ? options.application_name
            : title;
    if (!g_appkit_shell_delegate)
        g_appkit_shell_delegate = create_appkit_shell_delegate();
    if (g_appkit_shell_delegate)
        objc_send<void>(app, sel("setDelegate:"), g_appkit_shell_delegate);
    install_standard_appkit_menu(app, options);
    install_appkit_debug_hot_key();
    objc_send<void>(app, sel("finishLaunching"));

    id window = create_appkit_window(width, height, title, options);
    if (!window) {
        std::fprintf(stderr, "[appkit] failed to create NSWindow\n");
        uninstall_appkit_debug_hot_key();
        if (pool)
            objc_send<void>(pool, sel("drain"));
        return 1;
    }
    if (g_appkit_shell_delegate)
        objc_send<void>(window, sel("setDelegate:"), g_appkit_shell_delegate);
    id view_obj = objc_send<id>(window, sel("contentView"));

    native_host host;
    NativeSurfaceDescriptor surface{
        .kind = NativeSurfaceKind::MacOSWindow,
        .window = window,
        .view = view_obj,
    };
    surface.window_chrome = options.chrome;
    surface.integrated_titlebar = options.integrated_titlebar;
    surface.native_backdrop_material = options.native_backdrop_material;
    surface.native_backdrop_opacity = options.native_backdrop_opacity;
    surface.window_options_valid = true;
    host.window = &surface;
    host.surface_descriptor = &surface;
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
    // AppKit can finalize the full-size content view and backing scale while
    // the window is first ordered. Synchronize after that native commit so the
    // first frame uses the same surface geometry that later mouse events see.
    sync_appkit_surface(host, surface, window, true);
    repaint_current_after_surface_presented();

    auto const* artifact_dir = std::getenv("PHENOTYPE_ARTIFACT_DIR");
    bool const artifact_requested = artifact_dir && artifact_dir[0] != '\0';
    if (artifact_requested && platform.debug.capabilities
        && platform.debug.capabilities().material_backdrop_blur) {
        ::phenotype::detail::g_app().last_paint_hash = 0;
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

    bool const perf_ok = run_performance_probe_if_requested();
    if (env_flag_enabled("PHENOTYPE_PERF_EXIT")) {
        shutdown_host(host);
        objc_send<void>(window, sel("close"));
        if (pool)
            objc_send<void>(pool, sel("drain"));
        g_active_appkit_window = nullptr;
        g_active_appkit_surface = nullptr;
        return perf_ok ? 0 : 1;
    }

    auto last_animation_tick = std::chrono::steady_clock::now();
    while (!g_appkit_should_terminate
           && (!g_appkit_keep_running_after_last_window_closed
               || window)) {
        bool const visible = appkit_window_is_visible(window);
        if (!g_appkit_keep_running_after_last_window_closed && !visible)
            break;
        if (g_appkit_window_user_closed && !visible) {
            objc_send<void>(app, sel("run"));
            last_animation_tick = std::chrono::steady_clock::now();
            continue;
        }
        if (visible)
            sync_appkit_host_surface(host, surface, window, true);
        service_appkit_activation_reopen(app, window, visible);
        id event = next_appkit_event(app, 0.016);
        if (event
            && !service_appkit_host_event(
                app,
                event,
                host,
                surface,
                window,
                visible)
            && !service_appkit_scene_window_event(app, event))
            service_appkit_unhandled_event(app, event);
        objc_send<void>(app, sel("updateWindows"));
        service_appkit_scene_windows();
        service_host_tick(host, last_animation_tick);
    }

    close_all_appkit_scene_windows();
    for (auto& state : g_appkit_scene_windows) {
        if (state)
            shutdown_host(state->host);
    }
    g_appkit_scene_windows.clear();
    shutdown_host(host);
    g_active_appkit_window = nullptr;
    g_active_appkit_surface = nullptr;
    g_appkit_settings_menu_handler = nullptr;
    g_appkit_keep_running_after_last_window_closed = false;
    g_appkit_mouse_tracking_mode = false;
    g_appkit_window_user_closed = false;
    g_appkit_close_button_candidate = false;
    g_appkit_front_request_attempts = 0;
    uninstall_appkit_debug_hot_key();
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
