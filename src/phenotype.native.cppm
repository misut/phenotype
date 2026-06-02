module;

#if !defined(__wasi__)
#include <concepts>
#include <cstdio>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#endif

export module phenotype.native;

#if !defined(__wasi__)
export import phenotype;
export import phenotype.native.platform;
export import phenotype.native.shell;
import phenotype.native.stub;

#if defined(__ANDROID__)
import phenotype.native.android;
#else
import phenotype.native.macos;
import phenotype.native.windows;
#  if defined(__APPLE__)
export import phenotype.native.shell.macos;
#  elif defined(_WIN32)
export import phenotype.native.shell.windows;
#  endif
#endif

namespace phenotype::native::detail {

inline platform_api const& select_platform() {
#ifdef __APPLE__
    return macos_platform();
#elif defined(_WIN32)
    return windows_platform();
#elif defined(__ANDROID__)
    return android_platform();
#else
    return desktop_stub_platform();
#endif
}

inline platform_api const& require_platform(platform_api const* platform) {
    return platform ? *platform : select_platform();
}

template<typename App>
int run_desktop_app_with_platform(platform_api const& platform,
                                  int width,
                                  int height,
                                  char const* title,
                                  WindowOptions options,
                                  App app,
                                  std::function<void(int, int, float)> on_viewport = {}) {
#if defined(__APPLE__)
    return run_app_with_macos_platform(
        platform,
        width,
        height,
        title,
        options,
        std::move(app),
        std::move(on_viewport));
#elif defined(_WIN32)
    return run_app_with_windows_platform(
        platform,
        width,
        height,
        title,
        options,
        std::move(app),
        std::move(on_viewport));
#else
    (void)platform;
    (void)width;
    (void)height;
    (void)title;
    (void)options;
    (void)app;
    (void)on_viewport;
    std::fprintf(stderr, "phenotype.native run_app is not available on this desktop target\n");
    return 1;
#endif
}

} // namespace phenotype::native::detail

export namespace phenotype::native {

inline platform_api const& current_platform() {
    return detail::select_platform();
}

namespace preferences {

inline bool show_window(NativePreferencesWindowOptions const& options) {
#if defined(__APPLE__)
    return detail::show_appkit_preferences_window(options);
#else
    (void)options;
    return false;
#endif
}

inline bool sync_window(NativePreferencesWindowOptions const& options) {
#if defined(__APPLE__)
    return detail::sync_appkit_preferences_window(options);
#else
    (void)options;
    return false;
#endif
}

inline bool is_window_visible(char const* identifier = "preferences") {
#if defined(__APPLE__)
    return detail::is_appkit_preferences_window_visible(identifier);
#else
    (void)identifier;
    return false;
#endif
}

inline void close_window(char const* identifier = "preferences") {
#if defined(__APPLE__)
    detail::close_appkit_preferences_window(identifier);
#else
    (void)identifier;
#endif
}

} // namespace preferences

namespace scene_window {

template<typename App>
inline bool show(NativeSceneWindowOptions const& options, App app) {
#if defined(__APPLE__)
    return detail::show_appkit_scene_window(
        current_platform(),
        detail::appkit_scene_window_options(options),
        std::move(app));
#else
    (void)options;
    (void)app;
    return false;
#endif
}

inline bool is_window_visible(char const* identifier = "scene-window") {
#if defined(__APPLE__)
    return detail::is_appkit_scene_window_visible(identifier);
#else
    (void)identifier;
    return false;
#endif
}

inline void close_window(char const* identifier = "scene-window") {
#if defined(__APPLE__)
    detail::close_appkit_scene_window(identifier);
#else
    (void)identifier;
#endif
}

inline DebugArtifactBundleResult write_artifact_bundle(
        char const* identifier,
        std::string_view directory,
        std::string_view reason = {}) {
#if defined(__APPLE__)
    auto* state = detail::find_appkit_scene_window(
        detail::scene_window_identifier_or_default(identifier));
    if (!state) {
        return {
            false,
            std::string(directory),
            {},
            {},
            {},
            "Scene window is not registered",
        };
    }
    auto const& platform = current_platform();
    if (!platform.debug.write_artifact_bundle) {
        return {
            false,
            std::string(directory),
            {},
            {},
            {},
            "Current platform does not expose write_artifact_bundle()",
        };
    }
    detail::ScopedHostActivation activate(state->host);
    auto directory_copy = std::string(directory);
    auto reason_copy = std::string(reason);
    return platform.debug.write_artifact_bundle(
        directory_copy.empty() ? nullptr : directory_copy.c_str(),
        reason_copy.empty() ? nullptr : reason_copy.c_str());
#else
    (void)identifier;
    return {
        false,
        std::string(directory),
        {},
        {},
        {},
        "Scene window artifact capture is only available on Apple targets",
    };
#endif
}

} // namespace scene_window

inline PlatformSystemSettingsSnapshot system_settings() {
    auto const& platform = current_platform();
    if (platform.system.settings)
        return platform.system.settings();
    if (platform.debug.capabilities)
        return platform.debug.capabilities().system_settings;
    return {};
}

inline ResolvedThemePreferences resolve_native_theme_preferences(
        Theme theme,
        ThemePreferenceOverrides const& overrides = {},
        std::string_view source = "native-system-settings") {
    return ::phenotype::resolve_system_theme_preferences(
        theme,
        system_settings(),
        overrides,
        source);
}

inline ResolvedThemePreferences set_native_theme_preferences(
        Theme theme,
        ThemePreferenceOverrides const& overrides = {},
        std::string_view source = "native-system-settings") {
    auto resolved = resolve_native_theme_preferences(theme, overrides, source);
    ::phenotype::set_theme(resolved.theme);
    return resolved;
}

namespace text {

using ::phenotype::native::TextAtlas;
using ::phenotype::native::TextEntry;
using ::phenotype::native::TextQuad;

inline void init() {
    auto const& platform = current_platform();
    if (platform.text.init)
        platform.text.init();
}

inline void shutdown() {
    auto const& platform = current_platform();
    if (platform.text.shutdown)
        platform.text.shutdown();
}

inline float measure(float font_size, unsigned int flags,
                     char const* font_family, unsigned int family_len,
                     char const* text, unsigned int len) {
    auto const& platform = current_platform();
    if (!platform.text.measure) return 0.0f;
    return platform.text.measure(font_size, flags,
                                 font_family, family_len, text, len);
}

// Convenience overload for the common widget::text path that does not
// carry a font family — selects mono-default when `mono` is true.
inline float measure(float font_size, bool mono,
                     char const* text, unsigned int len) {
    auto const& platform = current_platform();
    if (!platform.text.measure) return 0.0f;
    return platform.text.measure(font_size, mono ? 1u : 0u,
                                 nullptr, 0u, text, len);
}

inline TextAtlas build_atlas(std::vector<TextEntry> const& entries,
                             float backing_scale = 1.0f) {
    auto const& platform = current_platform();
    if (!platform.text.build_atlas) return {};
    return platform.text.build_atlas(entries, backing_scale);
}

// Register a TTF / OTF / TTC font file with the active platform's font
// manager and bind it to `family_alias`. Subsequent FontSpec lookups
// for the same alias resolve to the registered face. Returns false
// when the file is missing, unsupported, or the platform backend has
// no runtime registration entry point — callers should treat that as
// "the alias is unknown" and fall back to their default-font path
// (the existing measure / atlas paths already do that for unknown
// families). Idempotent: re-registering an alias replaces the prior
// binding.
//
// Backend status: macOS uses CTFontManagerRegisterFontsForURL with
// process scope. Windows / Android / stub backends currently return
// false (file-loading registration is tracked as a follow-up).
inline bool register_font_file(std::string_view family_alias,
                               std::string_view path) {
    auto const& platform = current_platform();
    if (!platform.text.register_font_file) return false;
    if (family_alias.empty() || path.empty()) return false;
    return platform.text.register_font_file(
        family_alias.data(),
        static_cast<unsigned int>(family_alias.size()),
        path.data(),
        static_cast<unsigned int>(path.size()));
}

} // namespace text

namespace renderer {

inline void init(native_surface_handle surface) {
    auto const& platform = current_platform();
    if (platform.renderer.init)
        platform.renderer.init(surface);
}

inline void flush(unsigned char const* buf, unsigned int len) {
    auto const& platform = current_platform();
    if (platform.renderer.flush)
        platform.renderer.flush(buf, len);
}

inline void shutdown() {
    auto const& platform = current_platform();
    if (platform.renderer.shutdown)
        platform.renderer.shutdown();
}

inline std::optional<unsigned int> hit_test(float x, float y,
                                            float scroll_x, float scroll_y) {
    auto const& platform = current_platform();
    if (!platform.renderer.hit_test) return std::nullopt;
    return platform.renderer.hit_test(x, y, scroll_x, scroll_y);
}

} // namespace renderer

namespace debug {

inline ::phenotype::diag::PlatformCapabilitiesSnapshot capabilities() {
    auto const& platform = current_platform();
    if (platform.debug.capabilities)
        return platform.debug.capabilities();
    return {};
}

inline std::string snapshot_json() {
    auto const& platform = current_platform();
    if (platform.debug.snapshot_json)
        return platform.debug.snapshot_json();
    return ::phenotype::diag::serialize_snapshot();
}

inline std::optional<DebugFrameCapture> capture_frame_rgba() {
    auto const& platform = current_platform();
    if (!platform.debug.capture_frame_rgba)
        return std::nullopt;
    return platform.debug.capture_frame_rgba();
}

inline DebugArtifactBundleResult write_artifact_bundle(
        std::string_view directory,
        std::string_view reason) {
    auto const& platform = current_platform();
    if (!platform.debug.write_artifact_bundle) {
        return {
            false,
            std::string(directory),
            {},
            {},
            {},
            "Current platform does not expose write_artifact_bundle()",
        };
    }
    auto directory_copy = std::string(directory);
    auto reason_copy = std::string(reason);
    return platform.debug.write_artifact_bundle(
        directory_copy.empty() ? nullptr : directory_copy.c_str(),
        reason_copy.empty() ? nullptr : reason_copy.c_str());
}

} // namespace debug

namespace dialog {

// Thin wrapper over `platform_api::dialog.open_file`. Backends that
// have not implemented file picking yet (Windows, Android stub) leave
// the function pointer null — in that case we synthesise a "cancel"
// outcome so callers don't have to special-case unsupported platforms.
inline void open_file(char const* filter_extensions,
                      void (*callback)(char const* path)) {
    auto const& platform = current_platform();
    if (platform.dialog.open_file) {
        platform.dialog.open_file(filter_extensions, callback);
    } else if (callback) {
        callback(nullptr);
    }
}

} // namespace dialog

inline void shutdown() {
    renderer::shutdown();
    text::shutdown();
    ::phenotype::detail::set_application_open_url_handler(nullptr);
    ::phenotype::detail::set_application_settings_menu_handler(nullptr);
}

template<typename App>
void run(native_host& host, App app) {
    host.platform = &detail::require_platform(host.platform);
    detail::run_host(host, std::move(app));
}

template<typename App>
void run_scene(SceneHandle const& scene,
               native_host& host,
               App app) {
    host.platform = &detail::require_platform(host.platform);
    detail::run_host_scene(
        host,
        scene,
        std::move(app));
}

#if !defined(__ANDROID__)
template <typename App>
int run_app(int width, int height, char const* title, App app = App{}) {
    return detail::run_desktop_app_with_platform(
            current_platform(),
            width,
            height,
            title,
            WindowOptions{},
            std::move(app));
}

template <typename App>
int run_app(int width,
            int height,
            char const* title,
            WindowOptions options,
            App app = App{}) {
    return detail::run_desktop_app_with_platform(
            current_platform(),
            width,
            height,
            title,
            options,
            std::move(app));
}

template <typename App, typename ViewportChanged>
    requires std::invocable<ViewportChanged&, int, int, float>
int run_app(int width,
            int height,
            char const* title,
            App app,
            ViewportChanged on_viewport) {
    return detail::run_desktop_app_with_platform(
            current_platform(),
            width,
            height,
            title,
            WindowOptions{},
            std::move(app),
            std::function<void(int, int, float)>{std::move(on_viewport)});
}

template <typename App, typename ViewportChanged>
    requires std::invocable<ViewportChanged&, int, int, float>
int run_app(int width,
            int height,
            char const* title,
            WindowOptions options,
            App app,
            ViewportChanged on_viewport) {
    return detail::run_desktop_app_with_platform(
            current_platform(),
            width,
            height,
            title,
            options,
            std::move(app),
            std::function<void(int, int, float)>{std::move(on_viewport)});
}
#endif

} // namespace phenotype::native
#endif
