module;
#include <string>
#include <string_view>

export module phenotype.wasm;

import json;
import phenotype.diag;

export namespace phenotype::wasi::detail {

inline ::phenotype::diag::PlatformCapabilitiesSnapshot debug_capabilities() {
    return {
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
}

inline json::Value platform_runtime_details_json_with_reason(
        std::string_view artifact_reason) {
    json::Object runtime;
    runtime.emplace("host_model", json::Value{"wasi"});
    runtime.emplace("frame_capture_supported", json::Value{false});
    runtime.emplace("artifact_bundle_support", json::Value{"snapshot-only"});
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

} // namespace phenotype::wasi::detail
