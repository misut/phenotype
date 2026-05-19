module;
#include <string>
#include <string_view>

export module phenotype.wasm;

#ifdef __wasi__
import json;
import phenotype.diag;
import phenotype.types;
#endif

export namespace phenotype::wasi::detail {

#ifdef __wasi__
inline ::phenotype::diag::PlatformCapabilitiesSnapshot debug_capabilities() {
    ::phenotype::diag::PlatformCapabilitiesSnapshot snapshot{
        "wasi",
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
    snapshot.system_settings = ::phenotype::PlatformSystemSettingsSnapshot{
        .source = "wasi-fallback",
        .font_family = "Pretendard",
        .body_font_size = 16.0f,
        .heading_font_size = 22.4f,
        .small_font_size = 14.4f,
        .line_height_ratio = 1.6f,
        .font_scale = 1.0f,
        .text_size_source = "fallback",
        .preferred_scroller_style = "none",
        .overlay_scrollbars = false,
        .scroll_line_height = 25.6f,
        .scroll_wheel_lines = 3.0f,
        .scroll_page_mode = false,
        .scroll_delta_multiplier = 1.0f,
        .scroll_source = "fallback",
        .preferred_locale = "en",
        .preferred_locale_source = "wasi-fallback",
        .color_scheme = "light",
        .color_scheme_source = "wasi-fallback",
        .accessibility_source = "wasi-fallback",
    };
    return snapshot;
}

inline json::Value platform_runtime_details_json_with_reason(
        std::string_view artifact_reason) {
    json::Object runtime;
    runtime.emplace("host_model", json::Value{"wasi"});
    runtime.emplace("frame_capture_supported", json::Value{false});
    runtime.emplace("artifact_bundle_support", json::Value{"snapshot-only"});
    runtime.emplace(
        "system_settings",
        ::phenotype::diag::system_settings_to_json(
            debug_capabilities().system_settings));
    runtime.emplace(
        "renderer",
        json::Value{
            ::phenotype::diag::detail::empty_material_renderer_contract(
                "snapshot-only-semantic-material-fallback")});
    if (!artifact_reason.empty()) {
        runtime.emplace(
            "artifact_reason",
            json::Value{std::string(artifact_reason)});
    }
    return json::Value{std::move(runtime)};
}

inline json::Value platform_runtime_details_json() {
    return platform_runtime_details_json_with_reason({});
}

inline ::phenotype::diag::detail::ArtifactBundleResult write_artifact_bundle(
        std::string_view directory,
        std::string_view snapshot_json,
        std::string_view reason = {}) {
    auto runtime_json = json::emit(
        platform_runtime_details_json_with_reason(reason));
    return ::phenotype::diag::detail::write_debug_artifact_bundle(
        directory,
        "wasi",
        snapshot_json,
        runtime_json,
        nullptr);
}
#endif

} // namespace phenotype::wasi::detail
