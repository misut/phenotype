module;
#ifndef __wasi__
#endif

export module phenotype.native.stub;

#ifndef __wasi__
import std;
import phenotype;
import phenotype.commands;
import phenotype.native.platform;
import json;

namespace phenotype::native::detail {

inline std::vector<HitRegionCmd>& stub_hit_regions() {
    static std::vector<HitRegionCmd>& regions = *new std::vector<HitRegionCmd>();
    return regions;
}

inline void stub_text_init() {}
inline void stub_text_shutdown() {}

inline float stub_measure(float font_size, bool, char const*, unsigned int len) {
    return static_cast<float>(len) * font_size * 0.6f;
}

inline TextAtlas stub_build_atlas(std::vector<TextEntry> const&, float) {
    return {};
}

inline void stub_renderer_init(GLFWwindow*) {}

inline void stub_input_attach(GLFWwindow*, void (*)()) {}

inline void stub_input_detach() {}

inline bool stub_uses_shared_caret_blink() {
    return true;
}

inline void stub_input_sync() {
    auto snapshot = ::phenotype::detail::focused_input_snapshot();
    if (!snapshot.valid || !snapshot.caret_visible) {
        ::phenotype::detail::clear_input_debug_caret_presentation();
        return;
    }

    auto layout = ::phenotype::detail::compute_single_line_caret_layout(
        snapshot,
        ::phenotype::detail::get_scroll_y(),
        ::phenotype::diag::input_debug_snapshot().composition_active,
        [](auto const& input, std::size_t bytes) {
            bytes = ::phenotype::detail::clamp_utf8_boundary(input.value, bytes);
            if (bytes == 0)
                return 0.0f;
            return stub_measure(
                input.font_size,
                input.mono,
                input.value.data(),
                static_cast<unsigned int>(bytes));
        });
    ::phenotype::detail::set_input_debug_caret_presentation(
        "custom",
        layout.draw_x,
        layout.draw_y,
        1.5f,
        layout.height);
}

inline void stub_renderer_flush(unsigned char const* buf, unsigned int len) {
    auto& regions = stub_hit_regions();
    regions.clear();
    if (len == 0) return;

    auto cmds = parse_commands(buf, len);
    for (auto const& cmd : cmds) {
        if (auto const* hr = std::get_if<HitRegionCmd>(&cmd))
            regions.push_back(*hr);
    }
}

inline void stub_renderer_shutdown() {
    stub_hit_regions().clear();
}

inline std::optional<unsigned int> stub_renderer_hit_test(
        float x, float y, float scroll_y) {
    float wy = y + scroll_y;
    auto& regions = stub_hit_regions();
    for (int i = static_cast<int>(regions.size()) - 1; i >= 0; --i) {
        auto const& hr = regions[static_cast<std::size_t>(i)];
        if (x >= hr.x && x <= hr.x + hr.w && wy >= hr.y && wy <= hr.y + hr.h)
            return hr.callback_id;
    }
    return std::nullopt;
}

inline void stub_open_url(char const*, unsigned int) {}

inline ::phenotype::diag::PlatformCapabilitiesSnapshot stub_debug_capabilities() {
    return {
        "desktop-stub",
        true,
        true,
        false,
        true,
        true,
        true,
        true,
        false,
        false,
    };
}

inline json::Value stub_platform_runtime_details_json() {
    return json::Value{json::Object{}};
}

inline std::string stub_snapshot_json() {
    return ::phenotype::detail::serialize_diag_snapshot_with_debug(
        stub_debug_capabilities(),
        stub_platform_runtime_details_json());
}

inline std::optional<DebugFrameCapture> stub_capture_frame_rgba() {
    return std::nullopt;
}

inline DebugArtifactBundleResult stub_write_artifact_bundle(
        char const* directory,
        char const*) {
    DebugArtifactBundleResult result{};
    result.directory = directory ? directory : "";

    auto directory_path = std::filesystem::path{result.directory};
    if (!::phenotype::native::detail::ensure_directory(directory_path, result.error))
        return result;

    auto snapshot_path = directory_path / "snapshot.json";
    auto snapshot = stub_snapshot_json();
    if (!::phenotype::native::detail::write_text_file(
            snapshot_path,
            snapshot,
            result.error)) {
        return result;
    }

    result.ok = true;
    result.snapshot_json_path = snapshot_path.string();
    return result;
}

inline void install_stub_debug_providers() {
    ::phenotype::diag::detail::set_platform_capabilities_provider(
        stub_debug_capabilities);
    ::phenotype::diag::detail::set_platform_runtime_details_provider(
        stub_platform_runtime_details_json);
}

} // namespace phenotype::native::detail

export namespace phenotype::native {

inline platform_api make_stub_platform(char const* name,
                                       char const* startup_message) {
    platform_api api{};
    api.name = name;
    api.enabled = true;
    api.text = {
        detail::stub_text_init,
        detail::stub_text_shutdown,
        detail::stub_measure,
        detail::stub_build_atlas,
    };
    api.renderer = {
        detail::stub_renderer_init,
        detail::stub_renderer_flush,
        detail::stub_renderer_shutdown,
        detail::stub_renderer_hit_test,
    };
    api.input = {
        detail::stub_input_attach,
        detail::stub_input_detach,
        detail::stub_input_sync,
        detail::stub_uses_shared_caret_blink,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
    };
    api.debug = {
        detail::stub_debug_capabilities,
        detail::stub_snapshot_json,
        detail::stub_capture_frame_rgba,
        detail::stub_write_artifact_bundle,
    };
    api.open_url = detail::stub_open_url;
    api.startup_message = startup_message;
    return api;
}

inline platform_api const& desktop_stub_platform() {
    detail::install_stub_debug_providers();
    static platform_api api = make_stub_platform(
        "desktop-stub",
        "[phenotype-native] using stub desktop backend; native renderer/text are not implemented on this platform");
    return api;
}

} // namespace phenotype::native
#endif
