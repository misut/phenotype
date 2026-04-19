module;
#ifndef __wasi__
#include <concepts>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

struct GLFWwindow;
#endif

export module phenotype.native;

#ifndef __wasi__
export import phenotype;
export import phenotype.native.platform;
export import phenotype.native.shell;

import phenotype.native.macos;
import phenotype.native.stub;
import phenotype.native.windows;

namespace phenotype::native::detail {

inline platform_api const& select_platform() {
#ifdef __APPLE__
    return macos_platform();
#elif defined(_WIN32)
    return windows_platform();
#else
    return desktop_stub_platform();
#endif
}

inline platform_api const& require_platform(platform_api const* platform) {
    return platform ? *platform : select_platform();
}

} // namespace phenotype::native::detail

export namespace phenotype::native {

inline platform_api const& current_platform() {
    return detail::select_platform();
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

inline float measure(float font_size, bool mono,
                     char const* text, unsigned int len) {
    auto const& platform = current_platform();
    if (!platform.text.measure) return 0.0f;
    return platform.text.measure(font_size, mono, text, len);
}

inline TextAtlas build_atlas(std::vector<TextEntry> const& entries,
                             float backing_scale = 1.0f) {
    auto const& platform = current_platform();
    if (!platform.text.build_atlas) return {};
    return platform.text.build_atlas(entries, backing_scale);
}

} // namespace text

namespace renderer {

inline void init(GLFWwindow* window) {
    auto const& platform = current_platform();
    if (platform.renderer.init)
        platform.renderer.init(window);
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

inline std::optional<unsigned int> hit_test(float x, float y, float scroll_y) {
    auto const& platform = current_platform();
    if (!platform.renderer.hit_test) return std::nullopt;
    return platform.renderer.hit_test(x, y, scroll_y);
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

inline void shutdown() {
    renderer::shutdown();
    text::shutdown();
    ::phenotype::detail::g_open_url = nullptr;
}

template<typename State, typename Msg, typename View, typename Update>
    requires std::invocable<View, State const&>
          && std::invocable<Update, State&, Msg>
void run(native_host& host, View view, Update update) {
    host.platform = &detail::require_platform(host.platform);
    detail::run_host<State, Msg>(host, std::move(view), std::move(update));
}

template<typename State, typename Msg, typename View, typename Update>
    requires std::invocable<View, State const&>
          && std::invocable<Update, State&, Msg>
int run_app(int width, int height, char const* title, View view, Update update) {
    return detail::run_app_with_platform<State, Msg>(
        current_platform(), width, height, title,
        std::move(view), std::move(update));
}

} // namespace phenotype::native
#endif
