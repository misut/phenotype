module;
#include <algorithm>
#include <cstdint>
#include <cmath>
#include <span>
#include <string_view>

export module phenotype.material;

import phenotype.types;

export namespace phenotype {

inline constexpr std::uint32_t material_plan_contract_version = 21;
inline constexpr unsigned int material_max_execution_stages = 4;

struct MaterialGeometry {
    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;
    float radius = 0.0f;
};

struct MaterialShapeAnalysis {
    bool valid = false;
    bool rounded = false;
    bool radius_clamped = false;
    float surface_area = 0.0f;
    float min_extent = 0.0f;
    float max_extent = 0.0f;
    float radius_limit = 0.0f;
    float effective_radius = 0.0f;
    float normalized_radius = 0.0f;
};

struct MaterialCapabilityInput {
    bool material_surfaces = true;
    bool material_backdrop_blur = false;
    bool shader_blur = false;
    bool frame_history = false;
    bool reduce_transparency = false;
    bool increase_contrast = false;
    bool reduce_motion = false;
};

struct MaterialBackdropDescriptor {
    bool available = false;
    bool stable = false;
    bool excludes_foreground_text = false;
    float luma_min = 0.0f;
    float luma_max = 1.0f;
    float luma_mean = 0.5f;
    char const* source = "none";
};

struct MaterialRenderTargetMetadata {
    int width = 0;
    int height = 0;
    float scale = 1.0f;
    char const* pixel_format = "unknown";
};

struct MaterialQualityPolicy {
    bool allow_backdrop_sampling = true;
    bool allow_noise = true;
    bool allow_shadow = true;
    float max_blur_radius = 36.0f;
    unsigned int max_sample_taps = 25;
    std::int64_t max_backdrop_pixels = 4'000'000;
};

inline constexpr MaterialQualityPolicy default_material_quality_policy() noexcept {
    return MaterialQualityPolicy{};
}

inline MaterialQualityPolicy sanitize_material_quality_policy(
        MaterialQualityPolicy policy) noexcept {
    policy.max_blur_radius = std::max(0.0f, policy.max_blur_radius);
    policy.max_sample_taps = std::min(policy.max_sample_taps, 25u);
    policy.max_backdrop_pixels =
        std::max(std::int64_t{0}, policy.max_backdrop_pixels);
    return policy;
}

struct MaterialDebugSeed {
    std::uint32_t frame = 0;
    std::uint32_t node = 0;
    std::uint32_t salt = 0x4d41544cu;
};

struct MaterialRequest {
    MaterialStyle style{};
    MaterialGeometry geometry{};
};

enum class MaterialFallbackPath {
    None,
    NoMaterial,
    InvalidGeometry,
    UnsupportedBackend,
    NoBackdropSource,
    ReducedTransparency,
    QualityPolicy,
};

inline char const* material_fallback_path_name(MaterialFallbackPath path) noexcept {
    switch (path) {
        case MaterialFallbackPath::None: return "none";
        case MaterialFallbackPath::NoMaterial: return "no-material";
        case MaterialFallbackPath::InvalidGeometry: return "invalid-geometry";
        case MaterialFallbackPath::UnsupportedBackend: return "unsupported-backend";
        case MaterialFallbackPath::NoBackdropSource: return "no-backdrop-source";
        case MaterialFallbackPath::ReducedTransparency: return "reduced-transparency";
        case MaterialFallbackPath::QualityPolicy: return "quality-policy";
    }
    return "unsupported-backend";
}

inline bool material_role_allows_liquid_glass(
        MaterialSurfaceRole role) noexcept {
    switch (role) {
        case MaterialSurfaceRole::Content:
            return false;
        case MaterialSurfaceRole::Surface:
        case MaterialSurfaceRole::Toolbar:
        case MaterialSurfaceRole::Sidebar:
        case MaterialSurfaceRole::StatusBar:
        case MaterialSurfaceRole::Navigation:
        case MaterialSurfaceRole::Overlay:
            return true;
    }
    return true;
}

inline bool material_role_uses_standard_content_layer(
        MaterialSurfaceRole role) noexcept {
    return !material_role_allows_liquid_glass(role);
}

struct MaterialVerifierExpectation {
    bool require_backdrop_source = false;
    bool require_edge_highlight = false;
    bool require_container_identity = false;
    bool require_container_morph_contract = false;
    float min_luma_delta = 0.0f;
    int min_unique_colors = 1;
    char const* region_name = "material";
    char const* likely_layer = "material-fallback";
    char const* likely_pass = "none";
};

struct MaterialObservationContract {
    std::uint32_t schema_version = material_plan_contract_version;
    bool semantic_material_required = false;
    bool runtime_plan_required = false;
    bool fallback_expected = false;
    bool backdrop_sampling_expected = false;
    bool stable_backdrop_required = false;
    bool shared_frame_capture_required = false;
    bool next_frame_capture_required = false;
    bool backdrop_capture_excludes_foreground_text = false;
    bool bounded_texture_copy_required = true;
    bool deterministic_fallback_required = true;
    char const* backdrop_capture_scope = "none";
    char const* backdrop_capture_reason = "not-required";
    char const* fallback_path = "none";
    char const* fallback_reason = "";
    char const* primary_pass = "none";
    char const* primary_executor = "none";
    std::uint32_t expected_runtime_passes = 0;
    std::uint32_t expected_execution_stages = 0;
    std::uint32_t expected_active_execution_stages = 0;
    std::uint32_t expected_backdrop_execution_stages = 0;
    std::uint32_t max_frame_capture_count = 0;
    std::int64_t max_frame_capture_pixels = 0;
    std::int64_t max_surface_sample_pixels = 0;
    std::int64_t max_texture_copy_pixels = 0;
    char const* region_name = "material";
    char const* likely_layer = "material-fallback";
    char const* likely_pass = "none";
};

struct MaterialPassExpectation {
    char const* name = "none";
    bool active = false;
    bool requires_backdrop = false;
    unsigned int sample_taps = 0;
    char const* likely_layer = "material-fallback";
    char const* executor = "none";
    std::int64_t max_texture_copy_pixels = 0;
};

struct MaterialSamplingKernel {
    char const* name = "none";
    unsigned int radius = 0;
    unsigned int sample_taps = 0;
    float blur_step_scale = 0.0f;
    char const* weight_profile = "none";
    bool requires_backdrop = false;
    bool bounded = true;
};

struct MaterialLuminanceCurve {
    char const* name = "fallback-flat";
    float floor = 0.0f;
    float gain = 1.0f;
    float gamma = 1.0f;
    float midpoint = 0.5f;
    float contrast = 1.0f;
    float edge_lift = 0.0f;
    bool backdrop_driven = false;
    bool bounded = true;
};

struct MaterialResourceBudget {
    float max_blur_radius = 0.0f;
    unsigned int max_sample_taps = 0;
    unsigned int max_sampling_kernel_radius = 0;
    unsigned int max_pass_count = 1;
    unsigned int max_execution_stages = material_max_execution_stages;
    std::int64_t max_backdrop_pixels = 0;
    std::uint32_t max_frame_capture_count = 0;
    std::int64_t max_frame_capture_pixels = 0;
    std::int64_t max_surface_sample_pixels = 0;
    float max_container_spacing = 0.0f;
    bool bounded_texture_copy = true;
    bool deterministic_fallback = true;
};

struct MaterialExecutionStage {
    char const* name = "none";
    bool active = false;
    bool requires_backdrop = false;
    unsigned int sample_taps = 0;
    char const* likely_layer = "material-fallback-pass";
    char const* executor = "none";
    std::int64_t max_texture_copy_pixels = 0;
    bool bounded = true;
};

struct MaterialBackdropAnalysis {
    bool available = false;
    bool stable = false;
    bool excludes_foreground_text = false;
    float luma_min = 0.0f;
    float luma_max = 1.0f;
    float luma_mean = 0.5f;
    float luma_span = 1.0f;
    char const* source = "none";
    char const* luminance_response = "not-sampled";
    float luminance_floor_delta = 0.0f;
    float luminance_gain_delta = 0.0f;
    float edge_highlight_delta = 0.0f;
};

struct MaterialBackdropAccess {
    bool required = false;
    bool stable_required = false;
    bool frame_history_required = false;
    bool shared_frame_capture = false;
    bool next_frame_capture_required = false;
    bool excludes_foreground_text = false;
    char const* source = "none";
    char const* capture_scope = "none";
    char const* capture_reason = "not-required";
    std::uint32_t max_frame_capture_count = 0;
    std::int64_t max_frame_capture_pixels = 0;
    std::int64_t max_surface_sample_pixels = 0;
    bool bounded = true;
};

struct MaterialForegroundRecommendation {
    Color primary = {0, 0, 0, 255};
    Color secondary = {0, 0, 0, 255};
    Color accent = {0, 0, 0, 255};
    char const* scheme = "standard";
    char const* source = "theme";
    float background_luma = 1.0f;
    float primary_contrast_ratio = 1.0f;
    float secondary_contrast_ratio = 1.0f;
    float accent_contrast_ratio = 1.0f;
    float minimum_contrast_ratio = 4.5f;
    bool backdrop_driven = false;
    bool high_contrast = false;
    bool uses_vibrancy = false;
    bool deterministic = true;
};

struct MaterialThemeSnapshot {
    Color foreground = {0, 0, 0, 255};
    Color secondary_foreground = {0, 0, 0, 255};
    Color accent_foreground = {0, 0, 0, 255};
    Color strong_accent_foreground = {0, 0, 0, 255};
    Color tint = {0, 0, 0, 0};
    Color border = {0, 0, 0, 0};
    char const* source = "material-style";
    char const* profile_name = "custom";
    char const* token_policy = "explicit-material-style-tokens";
    bool foreground_matches_theme = false;
    bool accent_matches_theme = false;
    bool tint_matches_surface = false;
    bool border_matches_theme = false;
    bool default_glass_tokens = false;
};

struct MaterialRenderTargetAnalysis {
    int width = 0;
    int height = 0;
    float scale = 1.0f;
    char const* pixel_format = "unknown";
    std::int64_t pixel_count = 0;
    bool ready = false;
    bool within_backdrop_budget = true;
};

struct MaterialContainerAnalysis {
    std::uint32_t container_id = 0;
    std::uint32_t union_id = 0;
    float spacing = 0.0f;
    bool interactive = false;
    bool morph_transitions = false;
    bool participates = false;
    MaterialContainerMode mode = MaterialContainerMode::Isolated;
    char const* mode_name = "isolated";
    bool shared_backdrop_scope = false;
    bool shape_union_expected = false;
};

struct MaterialReferenceModel {
    char const* technology = "liquid-glass";
    char const* layer = "functional-layer";
    char const* material_policy = "liquid-glass-functional-layer";
    char const* variant = "none";
    char const* shape = "none";
    char const* shape_scope = "view-bounds";
    char const* blending_scope = "none";
    char const* semantic_thickness = "none";
    char const* accessibility_response = "standard";
    char const* performance_response = "inactive";
    bool view_bounds_anchored = false;
    bool shape_matches_geometry = false;
    bool tint_applied = false;
    bool interactive_response = false;
    bool container_grouped = false;
    bool container_union = false;
    bool container_morphing = false;
    bool legibility_preserved = true;
    bool vibrancy_expected = false;
    bool deterministic_degradation = true;
};

struct MaterialDecisionTrace {
    bool has_geometry = false;
    bool has_material = false;
    bool role_allows_liquid_glass = true;
    bool content_layer_standard_material = false;
    bool liquid_glass_backdrop_candidate = false;
    bool target_ready = false;
    bool quality_switches_allow_backdrop = false;
    bool backdrop_pixels_within_budget = false;
    bool quality_allows_backdrop = false;
    bool capability_material_surfaces = false;
    bool capability_material_backdrop_blur = false;
    bool capability_shader_blur = false;
    bool capability_frame_history = false;
    bool backend_supports_backdrop = false;
    bool backdrop_available = false;
    bool backdrop_stable = false;
    bool backdrop_source_ready = false;
    bool next_frame_capture_required = false;
    bool reduced_transparency = false;
    bool increase_contrast = false;
    bool reduce_motion = false;
    bool can_sample_backdrop = false;
    char const* first_blocker = "none";
};

struct MaterialPlan {
    std::uint32_t contract_version = material_plan_contract_version;
    MaterialKind kind = MaterialKind::None;
    MaterialSurfaceRole role = MaterialSurfaceRole::Surface;
    MaterialCommandDescriptor command_descriptor{};
    MaterialGeometry geometry{};
    MaterialShapeAnalysis shape{};
    MaterialRenderTargetAnalysis render_target{};
    MaterialContainerAnalysis container{};
    MaterialReferenceModel reference_model{};
    MaterialDecisionTrace decision_trace{};
    float opacity = 0.0f;
    float blur_radius = 0.0f;
    Color tint = {0, 0, 0, 0};
    float saturation = 1.0f;
    float luminance_floor = 0.0f;
    float luminance_gain = 1.0f;
    MaterialLuminanceCurve luminance_curve{};
    float edge_highlight = 0.0f;
    float edge_width = 1.0f;
    float noise_opacity = 0.0f;
    float shadow_alpha = 0.0f;
    float shadow_radius = 0.0f;
    bool backdrop_sampling = false;
    MaterialBackdropAnalysis backdrop{};
    MaterialBackdropAccess backdrop_access{};
    MaterialThemeSnapshot theme{};
    MaterialForegroundRecommendation foreground{};
    MaterialFallbackPath fallback_path = MaterialFallbackPath::None;
    char const* fallback_reason = "";
    char const* contrast_intent = "standard";
    char const* plan_id = "material.none";
    std::uint32_t debug_seed = 0;
    unsigned int sample_taps = 0;
    MaterialSamplingKernel sampling_kernel{};
    MaterialQualityPolicy quality_policy{};
    MaterialPassExpectation primary_pass{};
    MaterialResourceBudget resource_budget{};
    unsigned int execution_stage_capacity = material_max_execution_stages;
    unsigned int execution_stage_count = 0;
    unsigned int dropped_execution_stage_count = 0;
    MaterialExecutionStage
        execution_stages[material_max_execution_stages]{};
    MaterialVerifierExpectation verifier{};
    MaterialObservationContract observation_contract{};

    constexpr bool fallback() const noexcept {
        return fallback_path != MaterialFallbackPath::None;
    }
};

struct MaterialRuntimeRecord {
    MaterialPlan plan{};
    std::uint32_t command_index = 0;
};

inline bool material_plan_uses_standard_content_layer(
        MaterialPlan const& plan) noexcept {
    return plan.kind != MaterialKind::None
        && material_role_uses_standard_content_layer(plan.role);
}

inline void append_material_execution_stage(
        MaterialPlan& plan,
        MaterialExecutionStage stage) noexcept {
    if (!stage.active)
        return;
    auto const capacity =
        std::min(plan.execution_stage_capacity, material_max_execution_stages);
    if (plan.execution_stage_count >= capacity) {
        ++plan.dropped_execution_stage_count;
        return;
    }
    plan.execution_stages[plan.execution_stage_count++] = stage;
}

inline MaterialObservationContract material_observation_contract(
        MaterialPlan const& plan) noexcept {
    MaterialObservationContract contract{};
    contract.schema_version = plan.contract_version;
    contract.semantic_material_required = plan.kind != MaterialKind::None;
    contract.runtime_plan_required = plan.kind != MaterialKind::None;
    contract.fallback_expected = plan.fallback();
    contract.backdrop_sampling_expected = plan.backdrop_sampling;
    contract.stable_backdrop_required = plan.backdrop_sampling;
    contract.shared_frame_capture_required =
        plan.backdrop_access.shared_frame_capture;
    contract.next_frame_capture_required =
        plan.backdrop_access.next_frame_capture_required;
    contract.backdrop_capture_excludes_foreground_text =
        plan.backdrop_access.excludes_foreground_text;
    contract.bounded_texture_copy_required =
        plan.resource_budget.bounded_texture_copy;
    contract.deterministic_fallback_required =
        plan.resource_budget.deterministic_fallback;
    contract.backdrop_capture_scope = plan.backdrop_access.capture_scope;
    contract.backdrop_capture_reason = plan.backdrop_access.capture_reason;
    contract.fallback_path =
        material_fallback_path_name(plan.fallback_path);
    contract.fallback_reason = plan.fallback_reason;
    contract.primary_pass = plan.primary_pass.name;
    contract.primary_executor = plan.primary_pass.executor;
    contract.expected_runtime_passes = 1;
    contract.expected_execution_stages = plan.execution_stage_count;
    contract.max_frame_capture_count =
        plan.backdrop_access.max_frame_capture_count;
    contract.max_frame_capture_pixels =
        plan.backdrop_access.max_frame_capture_pixels;
    contract.max_surface_sample_pixels =
        plan.backdrop_access.max_surface_sample_pixels;
    contract.max_texture_copy_pixels =
        plan.primary_pass.max_texture_copy_pixels;
    contract.region_name = plan.verifier.region_name;
    contract.likely_layer = plan.verifier.likely_layer;
    contract.likely_pass = plan.verifier.likely_pass;
    for (unsigned int i = 0; i < plan.execution_stage_count; ++i) {
        auto const& stage = plan.execution_stages[i];
        if (stage.active)
            ++contract.expected_active_execution_stages;
        if (stage.requires_backdrop)
            ++contract.expected_backdrop_execution_stages;
    }
    return contract;
}

struct MaterialForegroundTextResolution {
    Color color{};
    char const* role = "none";
    std::uint32_t material_command_index = 0;
    bool has_material = false;
    bool remapped = false;
};

struct MaterialRuntimeSummary {
    std::uint32_t plan_count = 0;
    std::uint32_t fallback_count = 0;
    std::uint32_t backdrop_sampling_count = 0;
    std::uint32_t total_runtime_passes = 0;
    std::uint32_t active_runtime_passes = 0;
    std::uint32_t backdrop_runtime_passes = 0;
    std::uint32_t total_execution_stages = 0;
    std::uint32_t active_execution_stages = 0;
    std::uint32_t backdrop_execution_stages = 0;
    std::uint32_t dropped_execution_stages = 0;
    unsigned int max_execution_stage_count = 0;
    unsigned int max_execution_stages = 0;
    unsigned int max_execution_stage_capacity = 0;
    std::int64_t max_pass_texture_copy_pixels = 0;
    std::int64_t total_pass_texture_copy_pixels = 0;
    std::uint32_t backdrop_access_count = 0;
    std::uint32_t shared_frame_capture_plan_count = 0;
    std::uint32_t next_frame_capture_plan_count = 0;
    std::uint32_t max_frame_capture_count = 0;
    std::int64_t max_frame_capture_pixels = 0;
    std::int64_t total_surface_sample_pixels = 0;
    std::int64_t max_surface_sample_pixels = 0;
    float max_plan_blur_radius = 0.0f;
    unsigned int max_plan_sample_taps = 0;
    std::int64_t total_plan_sample_taps = 0;
    float max_budget_blur_radius = 0.0f;
    unsigned int max_sample_taps = 0;
    unsigned int max_sampling_kernel_radius = 0;
    unsigned int max_pass_count = 0;
    std::int64_t max_backdrop_pixels = 0;
    float max_container_spacing = 0.0f;
    std::uint32_t containered_count = 0;
    std::uint32_t unioned_count = 0;
    std::uint32_t interactive_count = 0;
    std::uint32_t morph_transition_count = 0;
    std::uint32_t valid_shape_count = 0;
    std::uint32_t rounded_shape_count = 0;
    std::uint32_t radius_clamped_count = 0;
    std::uint32_t foreground_backdrop_driven_count = 0;
    std::uint32_t foreground_high_contrast_count = 0;
    std::uint32_t foreground_vibrant_count = 0;
    std::uint32_t theme_default_glass_token_count = 0;
    std::uint32_t theme_custom_token_count = 0;
    float max_surface_area = 0.0f;
    float max_effective_radius = 0.0f;
    float max_radius_limit = 0.0f;
    float max_normalized_radius = 0.0f;
    float min_foreground_contrast_ratio = 0.0f;
    std::uint32_t unbounded_texture_copy = 0;
    std::uint32_t non_deterministic_fallback = 0;
};

struct MaterialExecutorSummary {
    std::uint32_t plan_count = 0;
    std::uint32_t material_instance_count = 0;
    std::uint32_t fallback_instance_count = 0;
    std::uint32_t material_draw_calls = 0;
    std::uint32_t backdrop_copy_count = 0;
    std::uint32_t execution_stage_count = 0;
    std::uint32_t active_execution_stage_count = 0;
    std::uint32_t backdrop_execution_stage_count = 0;
    std::uint32_t primary_execution_stage_count = 0;
    std::uint32_t dropped_execution_stage_count = 0;
    std::uint32_t backdrop_filter_stage_count = 0;
    std::uint32_t fallback_fill_stage_count = 0;
    std::uint32_t shadow_stage_count = 0;
    std::uint32_t edge_highlight_stage_count = 0;
    std::uint32_t noise_dither_stage_count = 0;
    std::uint32_t backdrop_access_plan_count = 0;
    std::uint32_t next_frame_capture_plan_count = 0;
    std::uint32_t planned_frame_capture_count = 0;
    std::int64_t planned_frame_capture_pixels = 0;
    std::int64_t planned_surface_sample_pixels = 0;
    std::uint32_t material_max_sample_taps = 0;
    std::int64_t material_total_sample_taps = 0;
    std::int64_t backdrop_copy_pixels = 0;
    bool backdrop_copy_excludes_foreground_text = false;
    bool foreground_pass_after_backdrop_copy = false;
    std::int64_t material_upload_bytes = 0;
    std::int64_t material_buffer_capacity_bytes = 0;
    std::uint32_t material_buffer_reallocations = 0;
    std::uint32_t foreground_text_candidate_count = 0;
    std::uint32_t foreground_text_remap_count = 0;
    std::int64_t cpu_decode_ns = 0;
    std::int64_t cpu_material_upload_ns = 0;
    std::int64_t cpu_total_ns = 0;
};

inline bool material_stage_matches(
        char const* actual,
        std::string_view expected) noexcept {
    return actual && std::string_view{actual} == expected;
}

inline void accumulate_material_executor_plan_summary(
        MaterialExecutorSummary& summary,
        MaterialPlan const& plan) noexcept {
    ++summary.plan_count;
    if (plan.fallback()) {
        ++summary.fallback_instance_count;
    } else {
        ++summary.material_instance_count;
        summary.material_max_sample_taps = std::max(
            summary.material_max_sample_taps,
            plan.sample_taps);
        summary.material_total_sample_taps += plan.sample_taps;
    }
    for (unsigned int i = 0; i < plan.execution_stage_count; ++i) {
        auto const& stage = plan.execution_stages[i];
        ++summary.execution_stage_count;
        if (stage.active)
            ++summary.active_execution_stage_count;
        if (stage.requires_backdrop)
            ++summary.backdrop_execution_stage_count;
        if (plan.primary_pass.active) {
            auto const primary_stage =
                material_stage_matches(stage.name, plan.primary_pass.name)
                && stage.requires_backdrop == plan.primary_pass.requires_backdrop
                && stage.sample_taps == plan.primary_pass.sample_taps
                && material_stage_matches(stage.likely_layer,
                                          plan.primary_pass.likely_layer)
                && material_stage_matches(stage.executor,
                                          plan.primary_pass.executor)
                && stage.max_texture_copy_pixels
                    == plan.primary_pass.max_texture_copy_pixels;
            if (primary_stage)
                ++summary.primary_execution_stage_count;
        }
        if (material_stage_matches(stage.executor, "backdrop-filter"))
            ++summary.backdrop_filter_stage_count;
        if (material_stage_matches(stage.executor, "fallback-fill"))
            ++summary.fallback_fill_stage_count;
        if (material_stage_matches(stage.name, "shape-shadow"))
            ++summary.shadow_stage_count;
        if (material_stage_matches(stage.name, "edge-highlight"))
            ++summary.edge_highlight_stage_count;
        if (material_stage_matches(stage.name, "noise-dither"))
            ++summary.noise_dither_stage_count;
    }
    summary.dropped_execution_stage_count +=
        plan.dropped_execution_stage_count;
    if (plan.backdrop_access.required)
        ++summary.backdrop_access_plan_count;
    if (plan.backdrop_access.next_frame_capture_required)
        ++summary.next_frame_capture_plan_count;
    if (plan.backdrop_access.shared_frame_capture
        || plan.backdrop_access.next_frame_capture_required) {
        summary.planned_frame_capture_count = std::max(
            summary.planned_frame_capture_count,
            plan.backdrop_access.max_frame_capture_count);
        summary.planned_frame_capture_pixels = std::max(
            summary.planned_frame_capture_pixels,
            plan.backdrop_access.max_frame_capture_pixels);
    }
    summary.planned_surface_sample_pixels +=
        plan.backdrop_access.max_surface_sample_pixels;
}

inline void accumulate_material_runtime_summary(
        MaterialRuntimeSummary& summary,
        MaterialRuntimeRecord const& record) noexcept {
    auto const& plan = record.plan;
    ++summary.plan_count;
    if (plan.fallback())
        ++summary.fallback_count;
    if (plan.backdrop_sampling)
        ++summary.backdrop_sampling_count;

    ++summary.total_runtime_passes;
    if (plan.primary_pass.active)
        ++summary.active_runtime_passes;
    if (plan.primary_pass.requires_backdrop)
        ++summary.backdrop_runtime_passes;
    summary.total_execution_stages += plan.execution_stage_count;
    summary.dropped_execution_stages +=
        plan.dropped_execution_stage_count;
    summary.max_execution_stage_count =
        std::max(summary.max_execution_stage_count, plan.execution_stage_count);
    summary.max_execution_stages =
        std::max(summary.max_execution_stages,
                 plan.resource_budget.max_execution_stages);
    summary.max_execution_stage_capacity =
        std::max(summary.max_execution_stage_capacity,
                 plan.execution_stage_capacity);
    for (unsigned int i = 0; i < plan.execution_stage_count; ++i) {
        auto const& stage = plan.execution_stages[i];
        if (stage.active)
            ++summary.active_execution_stages;
        if (stage.requires_backdrop)
            ++summary.backdrop_execution_stages;
    }
    summary.max_pass_texture_copy_pixels = std::max(
        summary.max_pass_texture_copy_pixels,
        plan.primary_pass.max_texture_copy_pixels);
    summary.total_pass_texture_copy_pixels +=
        plan.primary_pass.max_texture_copy_pixels;
    if (plan.backdrop_access.required)
        ++summary.backdrop_access_count;
    if (plan.backdrop_access.shared_frame_capture)
        ++summary.shared_frame_capture_plan_count;
    if (plan.backdrop_access.next_frame_capture_required)
        ++summary.next_frame_capture_plan_count;
    summary.max_frame_capture_count = std::max(
        summary.max_frame_capture_count,
        plan.backdrop_access.max_frame_capture_count);
    summary.max_frame_capture_pixels = std::max(
        summary.max_frame_capture_pixels,
        plan.backdrop_access.max_frame_capture_pixels);
    summary.total_surface_sample_pixels +=
        plan.backdrop_access.max_surface_sample_pixels;
    summary.max_surface_sample_pixels = std::max(
        summary.max_surface_sample_pixels,
        plan.backdrop_access.max_surface_sample_pixels);

    summary.max_plan_blur_radius =
        std::max(summary.max_plan_blur_radius, plan.blur_radius);
    summary.max_plan_sample_taps =
        std::max(summary.max_plan_sample_taps, plan.sample_taps);
    summary.total_plan_sample_taps += plan.sample_taps;
    summary.max_budget_blur_radius = std::max(
        summary.max_budget_blur_radius,
        plan.resource_budget.max_blur_radius);
    summary.max_sample_taps = std::max(
        summary.max_sample_taps,
        plan.resource_budget.max_sample_taps);
    summary.max_sampling_kernel_radius = std::max(
        summary.max_sampling_kernel_radius,
        plan.resource_budget.max_sampling_kernel_radius);
    summary.max_pass_count =
        std::max(summary.max_pass_count, plan.resource_budget.max_pass_count);
    summary.max_backdrop_pixels = std::max(
        summary.max_backdrop_pixels,
        plan.resource_budget.max_backdrop_pixels);
    summary.max_frame_capture_count = std::max(
        summary.max_frame_capture_count,
        plan.resource_budget.max_frame_capture_count);
    summary.max_frame_capture_pixels = std::max(
        summary.max_frame_capture_pixels,
        plan.resource_budget.max_frame_capture_pixels);
    summary.max_surface_sample_pixels = std::max(
        summary.max_surface_sample_pixels,
        plan.resource_budget.max_surface_sample_pixels);
    summary.max_container_spacing = std::max(
        summary.max_container_spacing,
        plan.resource_budget.max_container_spacing);
    if (plan.container.participates)
        ++summary.containered_count;
    if (plan.container.shape_union_expected)
        ++summary.unioned_count;
    if (plan.container.interactive)
        ++summary.interactive_count;
    if (plan.container.morph_transitions)
        ++summary.morph_transition_count;
    if (plan.shape.valid)
        ++summary.valid_shape_count;
    if (plan.shape.rounded)
        ++summary.rounded_shape_count;
    if (plan.shape.radius_clamped)
        ++summary.radius_clamped_count;
    if (plan.foreground.backdrop_driven)
        ++summary.foreground_backdrop_driven_count;
    if (plan.foreground.high_contrast)
        ++summary.foreground_high_contrast_count;
    if (plan.foreground.uses_vibrancy)
        ++summary.foreground_vibrant_count;
    if (plan.theme.default_glass_tokens)
        ++summary.theme_default_glass_token_count;
    else
        ++summary.theme_custom_token_count;
    summary.max_surface_area = std::max(
        summary.max_surface_area,
        plan.shape.surface_area);
    summary.max_effective_radius = std::max(
        summary.max_effective_radius,
        plan.shape.effective_radius);
    summary.max_radius_limit = std::max(
        summary.max_radius_limit,
        plan.shape.radius_limit);
    summary.max_normalized_radius = std::max(
        summary.max_normalized_radius,
        plan.shape.normalized_radius);
    if (plan.foreground.primary_contrast_ratio > 0.0f) {
        summary.min_foreground_contrast_ratio =
            summary.min_foreground_contrast_ratio == 0.0f
                ? plan.foreground.primary_contrast_ratio
                : std::min(summary.min_foreground_contrast_ratio,
                           plan.foreground.primary_contrast_ratio);
    }
    if (!plan.resource_budget.bounded_texture_copy)
        ++summary.unbounded_texture_copy;
    if (!plan.resource_budget.deterministic_fallback)
        ++summary.non_deterministic_fallback;
}

struct MaterialEnvironment {
    MaterialCapabilityInput capabilities{};
    MaterialBackdropDescriptor backdrop{};
    MaterialRenderTargetMetadata render_target{};
    MaterialDebugSeed debug_seed{};
    MaterialQualityPolicy quality{};
};

inline MaterialKind material_kind_from_wire(unsigned int raw) noexcept {
    switch (static_cast<MaterialKind>(raw)) {
        case MaterialKind::Clear: return MaterialKind::Clear;
        case MaterialKind::Thin: return MaterialKind::Thin;
        case MaterialKind::Regular: return MaterialKind::Regular;
        case MaterialKind::Thick: return MaterialKind::Thick;
        case MaterialKind::None:
        default: return MaterialKind::None;
    }
}

inline Color material_with_alpha(Color color, unsigned char alpha) noexcept {
    color.a = alpha;
    return color;
}

inline MaterialStyle material_style_for_kind(MaterialKind kind,
                                             Theme const& theme) noexcept {
    MaterialStyle style{};
    style.kind = kind;
    style.fallback = kind != MaterialKind::None;
    style.fallback_reason = kind == MaterialKind::None
        ? ""
        : "fallback path is available when backdrop blur is unavailable";
    style.foreground = theme.foreground;
    style.secondary_foreground = theme.muted;
    style.accent_foreground = theme.accent;
    style.strong_accent_foreground = theme.accent_strong;

    switch (kind) {
        case MaterialKind::Clear:
            style.opacity = 0.28f;
            style.blur_radius = 10.0f;
            style.tint = material_with_alpha(theme.surface, 72);
            style.border = material_with_alpha(theme.border, 120);
            style.contrast_intent = "context";
            style.saturation = 1.22f;
            style.luminance_floor = 0.02f;
            style.luminance_gain = 1.02f;
            style.edge_highlight = 0.22f;
            style.noise_opacity = 0.010f;
            style.shadow_alpha = 0.08f;
            style.shadow_radius = 8.0f;
            style.plan_id = "material.clear.base";
            style.verifier_profile = "clear-over-rich-backdrop";
            break;
        case MaterialKind::Thin:
            style.opacity = 0.42f;
            style.blur_radius = 16.0f;
            style.tint = material_with_alpha(theme.surface, 108);
            style.border = material_with_alpha(theme.border, 150);
            style.contrast_intent = "balanced";
            style.saturation = 1.16f;
            style.luminance_floor = 0.04f;
            style.luminance_gain = 1.05f;
            style.edge_highlight = 0.28f;
            style.noise_opacity = 0.012f;
            style.shadow_alpha = 0.10f;
            style.shadow_radius = 10.0f;
            style.plan_id = "material.thin.base";
            style.verifier_profile = "thin-balanced-backdrop";
            break;
        case MaterialKind::Regular:
            style.opacity = 0.58f;
            style.blur_radius = 22.0f;
            style.tint = material_with_alpha(theme.surface, 148);
            style.border = material_with_alpha(theme.border, 190);
            style.contrast_intent = "legible";
            style.saturation = 1.08f;
            style.luminance_floor = 0.08f;
            style.luminance_gain = 1.08f;
            style.edge_highlight = 0.34f;
            style.noise_opacity = 0.014f;
            style.shadow_alpha = 0.14f;
            style.shadow_radius = 14.0f;
            style.plan_id = "material.regular.base";
            style.verifier_profile = "regular-legibility-backdrop";
            break;
        case MaterialKind::Thick:
            style.opacity = 0.78f;
            style.blur_radius = 30.0f;
            style.tint = material_with_alpha(theme.surface, 198);
            style.border = material_with_alpha(theme.border, 230);
            style.contrast_intent = "high-contrast";
            style.saturation = 1.00f;
            style.luminance_floor = 0.12f;
            style.luminance_gain = 1.12f;
            style.edge_highlight = 0.42f;
            style.noise_opacity = 0.016f;
            style.shadow_alpha = 0.18f;
            style.shadow_radius = 18.0f;
            style.plan_id = "material.thick.base";
            style.verifier_profile = "thick-high-contrast-backdrop";
            break;
        case MaterialKind::None:
        default:
            style.opacity = 0.0f;
            style.blur_radius = 0.0f;
            style.tint = theme.transparent;
            style.border = theme.transparent;
            style.fallback = false;
            style.contrast_intent = "standard";
            style.plan_id = "material.none";
            style.verifier_profile = "none";
            break;
    }
    return style;
}

inline MaterialStyle material_style_for_command(MaterialKind kind,
                                                float opacity,
                                                float blur_radius,
                                                Color tint,
                                                Theme const& theme) noexcept {
    auto style = material_style_for_kind(kind, theme);
    style.opacity = opacity;
    style.blur_radius = blur_radius;
    style.tint = tint;
    return style;
}

inline MaterialCommandDescriptor material_command_descriptor(
        MaterialStyle const& style) noexcept {
    return MaterialCommandDescriptor{
        style.kind,
        style.role,
        style.container,
        style.opacity,
        style.blur_radius,
        style.tint,
        style.saturation,
        style.luminance_floor,
        style.luminance_gain,
        style.edge_highlight,
        style.edge_width,
        style.noise_opacity,
        style.shadow_alpha,
        style.shadow_radius};
}

inline MaterialStyle material_style_for_command(MaterialKind kind,
                                                MaterialSurfaceRole role,
                                                float opacity,
                                                float blur_radius,
                                                Color tint,
                                                Theme const& theme) noexcept {
    auto style = material_style_for_command(
        kind,
        opacity,
        blur_radius,
        tint,
        theme);
    style.role = role;
    return style;
}

inline MaterialStyle material_style_for_command(MaterialKind kind,
                                                float opacity,
                                                float blur_radius,
                                                Color tint,
                                                float saturation,
                                                float luminance_floor,
                                                float luminance_gain,
                                                float edge_highlight,
                                                float edge_width,
                                                float noise_opacity,
                                                float shadow_alpha,
                                                float shadow_radius,
                                                Theme const& theme) noexcept {
    auto style = material_style_for_command(
        kind,
        opacity,
        blur_radius,
        tint,
        theme);
    style.saturation = saturation;
    style.luminance_floor = luminance_floor;
    style.luminance_gain = luminance_gain;
    style.edge_highlight = edge_highlight;
    style.edge_width = edge_width;
    style.noise_opacity = noise_opacity;
    style.shadow_alpha = shadow_alpha;
    style.shadow_radius = shadow_radius;
    return style;
}

inline MaterialStyle material_style_for_command(
        MaterialCommandDescriptor const& descriptor,
        Theme const& theme) noexcept {
    auto style = material_style_for_command(
        descriptor.kind,
        descriptor.opacity,
        descriptor.blur_radius,
        descriptor.tint,
        descriptor.saturation,
        descriptor.luminance_floor,
        descriptor.luminance_gain,
        descriptor.edge_highlight,
        descriptor.edge_width,
        descriptor.noise_opacity,
        descriptor.shadow_alpha,
        descriptor.shadow_radius,
        theme);
    style.role = descriptor.role;
    style.container = descriptor.container;
    return style;
}

inline MaterialRequest material_request_for_command(MaterialKind kind,
                                                    float opacity,
                                                    float blur_radius,
                                                    Color tint,
                                                    MaterialGeometry geometry,
                                                    Theme const& theme) noexcept {
    return MaterialRequest{
        material_style_for_command(kind, opacity, blur_radius, tint, theme),
        geometry,
    };
}

inline MaterialRequest material_request_for_command(
        MaterialCommandDescriptor const& descriptor,
        MaterialGeometry geometry,
        Theme const& theme) noexcept {
    return MaterialRequest{
        material_style_for_command(descriptor, theme),
        geometry,
    };
}

inline MaterialRequest material_request_for_command(MaterialKind kind,
                                                    float opacity,
                                                    float blur_radius,
                                                    Color tint,
                                                    float saturation,
                                                    float luminance_floor,
                                                    float luminance_gain,
                                                    float edge_highlight,
                                                    float edge_width,
                                                    float noise_opacity,
                                                    float shadow_alpha,
                                                    float shadow_radius,
                                                    MaterialGeometry geometry,
                                                    Theme const& theme) noexcept {
    return material_request_for_command(
        MaterialCommandDescriptor{
            kind,
            MaterialSurfaceRole::Surface,
            {},
            opacity,
            blur_radius,
            tint,
            saturation,
            luminance_floor,
            luminance_gain,
            edge_highlight,
            edge_width,
            noise_opacity,
            shadow_alpha,
            shadow_radius},
        geometry,
        theme);
}

inline std::uint32_t material_debug_seed(MaterialDebugSeed seed,
                                         MaterialKind kind) noexcept {
    auto value = seed.salt ^ (seed.frame * 0x45d9f3bu);
    value ^= seed.node + 0x9e3779b9u + (value << 6u) + (value >> 2u);
    value ^= static_cast<std::uint32_t>(kind) * 0x27d4eb2du;
    return value;
}

inline unsigned int material_resolve_sample_taps(
        unsigned int max_sample_taps) noexcept {
    auto const bounded = std::min(max_sample_taps, 25u);
    if (bounded == 0u)
        return 0u;
    if (bounded < 5u)
        return 1u;
    if (bounded < 9u)
        return 5u;
    if (bounded < 13u)
        return 9u;
    if (bounded < 25u)
        return 13u;
    return 25u;
}

inline MaterialSamplingKernel material_resolve_sampling_kernel(
        bool backdrop_sampling,
        unsigned int sample_taps) noexcept {
    if (!backdrop_sampling || sample_taps == 0u)
        return {};
    return MaterialSamplingKernel{
        "weighted-5x5-manhattan",
        2u,
        sample_taps,
        0.35f,
        "center4-cardinal2-diagonal1",
        true,
        true,
    };
}

inline float material_safe_luma(float value, float fallback) noexcept {
    return std::isfinite(value)
        ? std::clamp(value, 0.0f, 1.0f)
        : fallback;
}

inline float material_srgb_channel_luma(unsigned char channel) noexcept {
    auto const value = static_cast<float>(channel) / 255.0f;
    return value <= 0.04045f
        ? value / 12.92f
        : std::pow((value + 0.055f) / 1.055f, 2.4f);
}

inline float material_color_luma(Color color) noexcept {
    return 0.2126f * material_srgb_channel_luma(color.r)
        + 0.7152f * material_srgb_channel_luma(color.g)
        + 0.0722f * material_srgb_channel_luma(color.b);
}

inline float material_contrast_ratio(float a, float b) noexcept {
    auto const lighter = std::max(a, b);
    auto const darker = std::min(a, b);
    return (lighter + 0.05f) / (darker + 0.05f);
}

inline float material_contrast_ratio(Color foreground,
                                     float background_luma) noexcept {
    return material_contrast_ratio(
        material_color_luma(foreground),
        material_safe_luma(background_luma, 1.0f));
}

inline Color material_with_opaque_alpha(Color color) noexcept {
    color.a = 255;
    return color;
}

inline Color material_pick_readable_color(Color preferred,
                                          float background_luma,
                                          float minimum_contrast) noexcept {
    auto best = material_with_opaque_alpha(preferred);
    auto best_contrast = material_contrast_ratio(best, background_luma);
    if (best_contrast >= minimum_contrast)
        return best;
    auto const dark = Color{17, 24, 39, 255};
    auto const light = Color{255, 255, 255, 255};
    Color const candidates[2] = {dark, light};
    for (auto candidate : candidates) {
        auto const contrast = material_contrast_ratio(candidate, background_luma);
        if (contrast > best_contrast) {
            best = candidate;
            best_contrast = contrast;
        }
    }
    return best;
}

inline float material_estimated_surface_luma(MaterialPlan const& plan) noexcept {
    auto const backdrop_luma = plan.backdrop.available
        ? plan.backdrop.luma_mean
        : material_color_luma(plan.tint);
    auto const tint_luma = material_color_luma(plan.tint);
    auto const alpha = std::clamp(
        plan.opacity * (static_cast<float>(plan.tint.a) / 255.0f),
        0.0f,
        1.0f);
    auto const mixed = backdrop_luma * (1.0f - alpha) + tint_luma * alpha;
    return std::clamp(mixed + plan.luminance_floor * 0.08f, 0.0f, 1.0f);
}

inline char const* material_foreground_scheme_name(
        MaterialPlan const& plan,
        float background_luma,
        MaterialCapabilityInput capabilities) noexcept {
    if (plan.kind == MaterialKind::None)
        return "none";
    if (capabilities.increase_contrast)
        return "high-contrast";
    if (capabilities.reduce_transparency || plan.fallback())
        return "solid-fallback";
    if (!plan.backdrop_sampling)
        return "standard";
    if (background_luma < 0.48f)
        return "vibrant-light";
    if (background_luma > 0.68f)
        return "vibrant-dark";
    return "vibrant-balanced";
}

inline char const* material_foreground_source_name(
        MaterialPlan const& plan,
        MaterialCapabilityInput capabilities) noexcept {
    if (plan.kind == MaterialKind::None)
        return "none";
    if (capabilities.increase_contrast)
        return "accessibility";
    if (plan.backdrop_sampling)
        return "backdrop-analysis";
    if (plan.fallback())
        return "fallback-material";
    return "theme";
}

inline MaterialForegroundRecommendation material_resolve_foreground(
        MaterialPlan const& plan,
        MaterialStyle const& style,
        MaterialCapabilityInput capabilities) noexcept {
    MaterialForegroundRecommendation foreground{};
    foreground.background_luma = material_estimated_surface_luma(plan);
    foreground.minimum_contrast_ratio = 4.5f;
    foreground.scheme = material_foreground_scheme_name(
        plan,
        foreground.background_luma,
        capabilities);
    foreground.source = material_foreground_source_name(plan, capabilities);
    foreground.backdrop_driven = plan.backdrop_sampling;
    foreground.high_contrast = capabilities.increase_contrast;
    foreground.uses_vibrancy = plan.backdrop_sampling
        && !capabilities.reduce_transparency
        && !plan.fallback();
    foreground.deterministic = true;

    foreground.primary = material_pick_readable_color(
        style.foreground,
        foreground.background_luma,
        foreground.minimum_contrast_ratio);
    auto const primary_light =
        material_color_luma(foreground.primary) > foreground.background_luma;
    auto secondary_preferred = style.secondary_foreground;
    if (material_contrast_ratio(secondary_preferred,
                                foreground.background_luma)
            < foreground.minimum_contrast_ratio) {
        secondary_preferred = primary_light
            ? Color{226, 232, 240, 255}
            : Color{71, 85, 105, 255};
    }
    foreground.secondary = material_pick_readable_color(
        secondary_preferred,
        foreground.background_luma,
        foreground.minimum_contrast_ratio);

    auto accent_preferred = style.accent_foreground;
    if (material_contrast_ratio(accent_preferred,
                                foreground.background_luma)
            < foreground.minimum_contrast_ratio) {
        accent_preferred = style.strong_accent_foreground;
    }
    foreground.accent = material_pick_readable_color(
        accent_preferred,
        foreground.background_luma,
        foreground.minimum_contrast_ratio);

    foreground.primary_contrast_ratio =
        material_contrast_ratio(foreground.primary, foreground.background_luma);
    foreground.secondary_contrast_ratio =
        material_contrast_ratio(foreground.secondary, foreground.background_luma);
    foreground.accent_contrast_ratio =
        material_contrast_ratio(foreground.accent, foreground.background_luma);
    return foreground;
}

inline bool material_color_rgb_equal(Color a, Color b) noexcept {
    return a.r == b.r && a.g == b.g && a.b == b.b;
}

inline MaterialThemeSnapshot material_resolve_theme_snapshot(
        MaterialStyle const& style) noexcept {
    MaterialThemeSnapshot snapshot{};
    Theme const default_theme{};
    snapshot.foreground = style.foreground;
    snapshot.secondary_foreground = style.secondary_foreground;
    snapshot.accent_foreground = style.accent_foreground;
    snapshot.strong_accent_foreground = style.strong_accent_foreground;
    snapshot.tint = style.tint;
    snapshot.border = style.border;
    snapshot.foreground_matches_theme =
        style.foreground == default_theme.foreground
        && style.secondary_foreground == default_theme.muted;
    snapshot.accent_matches_theme =
        style.accent_foreground == default_theme.accent
        && style.strong_accent_foreground == default_theme.accent_strong;
    snapshot.tint_matches_surface =
        style.kind == MaterialKind::None
        || material_color_rgb_equal(style.tint, default_theme.surface);
    snapshot.border_matches_theme =
        style.kind == MaterialKind::None
        || material_color_rgb_equal(style.border, default_theme.border);
    snapshot.default_glass_tokens =
        snapshot.foreground_matches_theme
        && snapshot.accent_matches_theme
        && snapshot.tint_matches_surface
        && snapshot.border_matches_theme
        && theme_matches_default_glass_contract(default_theme);
    snapshot.profile_name = snapshot.default_glass_tokens
        ? "apple-glass-light"
        : "custom";
    return snapshot;
}

inline Color material_preserve_text_alpha(Color resolved,
                                          Color original) noexcept {
    resolved.a = original.a;
    return resolved;
}

inline bool material_point_inside_geometry(MaterialGeometry geometry,
                                           float x,
                                           float y) noexcept {
    return geometry.w > 0.0f
        && geometry.h > 0.0f
        && x >= geometry.x
        && y >= geometry.y
        && x <= geometry.x + geometry.w
        && y <= geometry.y + geometry.h;
}

inline MaterialForegroundTextResolution material_resolve_text_foreground(
        std::span<MaterialRuntimeRecord const> records,
        std::uint32_t text_command_index,
        float text_x,
        float text_y,
        Color text_color,
        Theme const& theme) noexcept {
    MaterialForegroundTextResolution result{};
    result.color = text_color;

    for (auto it = records.rbegin(); it != records.rend(); ++it) {
        auto const& record = *it;
        auto const& plan = record.plan;
        if (record.command_index >= text_command_index)
            continue;
        if (plan.kind == MaterialKind::None)
            continue;
        if (!material_point_inside_geometry(plan.geometry, text_x, text_y))
            continue;

        result.has_material = true;
        result.material_command_index = record.command_index;
        auto const style = material_style_for_kind(plan.kind, theme);

        if (material_color_rgb_equal(text_color, style.foreground)) {
            result.color = material_preserve_text_alpha(
                plan.foreground.primary,
                text_color);
            result.role = "primary";
            result.remapped = true;
            return result;
        }
        if (material_color_rgb_equal(text_color, style.secondary_foreground)) {
            result.color = material_preserve_text_alpha(
                plan.foreground.secondary,
                text_color);
            result.role = "secondary";
            result.remapped = true;
            return result;
        }
        if (material_color_rgb_equal(text_color, style.accent_foreground)
            || material_color_rgb_equal(
                text_color,
                style.strong_accent_foreground)) {
            result.color = material_preserve_text_alpha(
                plan.foreground.accent,
                text_color);
            result.role = "accent";
            result.remapped = true;
            return result;
        }
        return result;
    }

    return result;
}

inline MaterialBackdropAnalysis analyze_material_backdrop(
        MaterialBackdropDescriptor backdrop) noexcept {
    MaterialBackdropAnalysis analysis{};
    analysis.available = backdrop.available;
    analysis.stable = backdrop.stable;
    analysis.excludes_foreground_text = backdrop.excludes_foreground_text;
    analysis.luma_min = material_safe_luma(backdrop.luma_min, 0.0f);
    analysis.luma_max = std::max(
        analysis.luma_min,
        material_safe_luma(backdrop.luma_max, 1.0f));
    analysis.luma_mean = std::clamp(
        material_safe_luma(backdrop.luma_mean, 0.5f),
        analysis.luma_min,
        analysis.luma_max);
    analysis.luma_span = analysis.luma_max - analysis.luma_min;
    analysis.source = backdrop.source && backdrop.source[0]
        ? backdrop.source
        : "none";
    return analysis;
}

inline MaterialRenderTargetAnalysis analyze_material_render_target(
        MaterialRenderTargetMetadata target,
        std::int64_t max_backdrop_pixels) noexcept {
    MaterialRenderTargetAnalysis analysis{};
    analysis.width = std::max(0, target.width);
    analysis.height = std::max(0, target.height);
    analysis.scale = std::isfinite(target.scale) && target.scale > 0.0f
        ? target.scale
        : 1.0f;
    analysis.pixel_format = target.pixel_format && target.pixel_format[0]
        ? target.pixel_format
        : "unknown";
    analysis.ready = analysis.width > 0 && analysis.height > 0;
    analysis.pixel_count = analysis.ready
        ? static_cast<std::int64_t>(analysis.width)
            * static_cast<std::int64_t>(analysis.height)
        : std::int64_t{0};
    analysis.within_backdrop_budget =
        analysis.pixel_count <= max_backdrop_pixels;
    return analysis;
}

inline std::int64_t material_estimate_surface_sample_pixels(
        MaterialShapeAnalysis shape,
        MaterialRenderTargetAnalysis target) noexcept {
    if (!shape.valid || !target.ready)
        return 0;
    auto const scaled =
        static_cast<double>(shape.surface_area)
        * static_cast<double>(target.scale)
        * static_cast<double>(target.scale);
    if (!std::isfinite(scaled) || scaled <= 0.0)
        return 0;
    auto const bounded = std::min(
        scaled,
        static_cast<double>(target.pixel_count));
    return static_cast<std::int64_t>(std::ceil(bounded));
}

inline MaterialShapeAnalysis analyze_material_shape(
        MaterialGeometry geometry) noexcept {
    MaterialShapeAnalysis analysis{};
    bool const width_ready = std::isfinite(geometry.w) && geometry.w > 0.0f;
    bool const height_ready = std::isfinite(geometry.h) && geometry.h > 0.0f;
    analysis.valid = width_ready && height_ready;
    if (!analysis.valid)
        return analysis;

    auto const surface_area = geometry.w * geometry.h;
    analysis.surface_area = std::isfinite(surface_area) ? surface_area : 0.0f;
    analysis.min_extent = std::min(geometry.w, geometry.h);
    analysis.max_extent = std::max(geometry.w, geometry.h);
    analysis.radius_limit = analysis.min_extent * 0.5f;
    auto const requested_radius = std::isfinite(geometry.radius)
        ? geometry.radius
        : 0.0f;
    analysis.effective_radius =
        std::clamp(requested_radius, 0.0f, analysis.radius_limit);
    analysis.rounded = analysis.effective_radius > 0.0f;
    analysis.radius_clamped =
        std::fabs(analysis.effective_radius - requested_radius) > 0.0001f;
    analysis.normalized_radius = analysis.radius_limit > 0.0f
        ? analysis.effective_radius / analysis.radius_limit
        : 0.0f;
    return analysis;
}

inline MaterialContainerAnalysis analyze_material_container(
        MaterialContainerDescriptor descriptor,
        bool reduce_motion) noexcept {
    MaterialContainerAnalysis analysis{};
    analysis.container_id = descriptor.container_id;
    analysis.union_id = descriptor.union_id;
    analysis.spacing = std::max(0.0f, descriptor.spacing);
    analysis.interactive = descriptor.interactive;
    analysis.morph_transitions =
        descriptor.morph_transitions && !reduce_motion;
    analysis.participates = descriptor.participates();
    analysis.mode = descriptor.mode();
    analysis.mode_name = material_container_mode_name(analysis.mode);
    analysis.shared_backdrop_scope = analysis.participates;
    analysis.shape_union_expected =
        analysis.mode == MaterialContainerMode::Union;
    return analysis;
}

inline char const* material_luminance_response_name(
        bool dark,
        bool bright,
        bool flat) noexcept {
    if (dark && flat)
        return "dark-flat";
    if (bright && flat)
        return "bright-flat";
    if (dark)
        return "dark";
    if (bright)
        return "bright";
    if (flat)
        return "flat";
    return "neutral";
}

inline void apply_backdrop_luminance_policy(MaterialPlan& plan) noexcept {
    auto const luma_mean = plan.backdrop.luma_mean;
    auto const luma_span = plan.backdrop.luma_span;
    auto const floor_before = plan.luminance_floor;
    auto const gain_before = plan.luminance_gain;
    auto const edge_before = plan.edge_highlight;
    bool dark = false;
    bool bright = false;
    bool flat = false;

    if (luma_mean < 0.35f) {
        dark = true;
        auto const darkness = std::clamp((0.35f - luma_mean) / 0.35f, 0.0f, 1.0f);
        plan.luminance_floor = std::max(
            plan.luminance_floor,
            0.08f + 0.08f * darkness);
        plan.luminance_gain = std::max(
            plan.luminance_gain,
            1.08f + 0.10f * darkness);
        plan.edge_highlight = std::min(
            1.0f,
            plan.edge_highlight + 0.06f * darkness);
    } else if (luma_mean > 0.72f) {
        bright = true;
        auto const brightness = std::clamp((luma_mean - 0.72f) / 0.28f, 0.0f, 1.0f);
        plan.luminance_gain = std::max(
            1.0f,
            plan.luminance_gain - 0.04f * brightness);
        plan.edge_highlight = std::min(
            1.0f,
            plan.edge_highlight + 0.04f * brightness);
    }

    if (luma_span < 0.12f) {
        flat = true;
        auto const flatness = std::clamp((0.12f - luma_span) / 0.12f, 0.0f, 1.0f);
        plan.edge_highlight = std::min(
            1.0f,
            plan.edge_highlight + 0.04f * flatness);
    }

    plan.backdrop.luminance_response =
        material_luminance_response_name(dark, bright, flat);
    plan.backdrop.luminance_floor_delta =
        plan.luminance_floor - floor_before;
    plan.backdrop.luminance_gain_delta =
        plan.luminance_gain - gain_before;
    plan.backdrop.edge_highlight_delta =
        plan.edge_highlight - edge_before;
}

inline MaterialBackdropAccess material_resolve_backdrop_access(
        MaterialPlan const& plan) noexcept {
    MaterialBackdropAccess access{};
    if (!plan.backdrop_sampling
        && !plan.decision_trace.next_frame_capture_required)
        return access;
    access.required = plan.backdrop_sampling;
    access.stable_required = plan.backdrop_sampling;
    access.frame_history_required = plan.backdrop_sampling;
    access.shared_frame_capture = true;
    access.next_frame_capture_required = true;
    access.excludes_foreground_text = true;
    bool const has_named_backdrop_source =
        plan.backdrop.source
        && plan.backdrop.source[0]
        && std::string_view{plan.backdrop.source} != "none";
    access.source = has_named_backdrop_source
        ? plan.backdrop.source
        : "previous-presented-frame";
    access.capture_scope = "shared-frame";
    access.capture_reason = plan.backdrop_sampling
        ? "sample-current-frame"
        : "warmup-next-frame";
    access.max_frame_capture_count = 1;
    access.max_frame_capture_pixels = plan.render_target.pixel_count;
    access.max_surface_sample_pixels = plan.backdrop_sampling
        ? material_estimate_surface_sample_pixels(
            plan.shape,
            plan.render_target)
        : 0;
    access.bounded = true;
    return access;
}

inline MaterialLuminanceCurve material_resolve_luminance_curve(
        bool backdrop_sampling,
        MaterialBackdropAnalysis backdrop,
        float luminance_floor,
        float luminance_gain,
        float edge_highlight) noexcept {
    MaterialLuminanceCurve curve{};
    curve.floor = std::clamp(luminance_floor, 0.0f, 1.0f);
    curve.gain = std::max(0.0f, luminance_gain);
    curve.edge_lift = std::clamp(edge_highlight, 0.0f, 1.0f);
    if (!backdrop_sampling)
        return curve;

    auto const luma_mean = std::clamp(backdrop.luma_mean, 0.0f, 1.0f);
    auto const luma_span = std::clamp(backdrop.luma_span, 0.0f, 1.0f);
    curve.name = "adaptive-backdrop-luma";
    curve.backdrop_driven = true;
    curve.midpoint = std::clamp(luma_mean, 0.25f, 0.75f);

    if (luma_mean < 0.35f) {
        auto const darkness =
            std::clamp((0.35f - luma_mean) / 0.35f, 0.0f, 1.0f);
        curve.gamma = 0.92f - 0.06f * darkness;
    } else if (luma_mean > 0.72f) {
        auto const brightness =
            std::clamp((luma_mean - 0.72f) / 0.28f, 0.0f, 1.0f);
        curve.gamma = 1.04f + 0.06f * brightness;
    }

    if (luma_span < 0.12f) {
        auto const flatness =
            std::clamp((0.12f - luma_span) / 0.12f, 0.0f, 1.0f);
        curve.contrast = 1.0f + 0.12f * flatness;
    }
    return curve;
}

inline char const* material_plan_id(MaterialKind kind,
                                    MaterialSurfaceRole role,
                                    bool backdrop_sampling) noexcept {
    if (material_role_uses_standard_content_layer(role)) {
        switch (kind) {
            case MaterialKind::Clear:
                return "material.clear.standard-material";
            case MaterialKind::Thin:
                return "material.thin.standard-material";
            case MaterialKind::Regular:
                return "material.regular.standard-material";
            case MaterialKind::Thick:
                return "material.thick.standard-material";
            case MaterialKind::None:
            default:
                return "material.none";
        }
    }
    switch (kind) {
        case MaterialKind::Clear:
            return backdrop_sampling ? "material.clear.liquid-glass"
                                     : "material.clear.fallback";
        case MaterialKind::Thin:
            return backdrop_sampling ? "material.thin.liquid-glass"
                                     : "material.thin.fallback";
        case MaterialKind::Regular:
            return backdrop_sampling ? "material.regular.liquid-glass"
                                     : "material.regular.fallback";
        case MaterialKind::Thick:
            return backdrop_sampling ? "material.thick.liquid-glass"
                                     : "material.thick.fallback";
        case MaterialKind::None:
        default:
            return "material.none";
    }
}

inline char const* material_reference_shape_name(
        MaterialShapeAnalysis shape) noexcept {
    if (!shape.valid)
        return "invalid";
    return shape.rounded ? "rounded-rectangle" : "rectangle";
}

inline char const* material_reference_blending_scope(
        MaterialPlan const& plan) noexcept {
    if (plan.kind == MaterialKind::None)
        return "none";
    if (plan.backdrop_sampling)
        return "sampled-backdrop";
    if (plan.fallback())
        return "deterministic-fallback";
    if (material_plan_uses_standard_content_layer(plan))
        return "standard-fill";
    return "none";
}

inline char const* material_reference_technology(
        MaterialPlan const& plan) noexcept {
    if (material_plan_uses_standard_content_layer(plan))
        return "standard-material";
    return "liquid-glass";
}

inline char const* material_reference_layer(
        MaterialPlan const& plan) noexcept {
    if (plan.kind == MaterialKind::None)
        return "inactive";
    if (material_plan_uses_standard_content_layer(plan))
        return "content-layer";
    return "functional-layer";
}

inline char const* material_reference_policy(
        MaterialPlan const& plan) noexcept {
    if (plan.kind == MaterialKind::None)
        return "inactive";
    if (material_plan_uses_standard_content_layer(plan))
        return "standard-material-content-layer";
    return "liquid-glass-functional-layer";
}

inline char const* material_reference_accessibility_response(
        MaterialPlan const& plan) noexcept {
    unsigned int accessibility_adjustment_count = 0;
    if (plan.decision_trace.reduced_transparency)
        ++accessibility_adjustment_count;
    if (plan.decision_trace.increase_contrast)
        ++accessibility_adjustment_count;
    if (plan.decision_trace.reduce_motion)
        ++accessibility_adjustment_count;
    if (accessibility_adjustment_count > 1u)
        return "combined-accessibility";
    if (plan.decision_trace.reduced_transparency)
        return "reduced-transparency";
    if (plan.decision_trace.increase_contrast)
        return "increased-contrast";
    if (plan.decision_trace.reduce_motion)
        return "reduced-motion";
    return "standard";
}

inline bool material_reference_uses_budgeted_effects(
        MaterialPlan const& plan) noexcept {
    return plan.backdrop_sampling
        && (plan.resource_budget.max_blur_radius < 36.0f
            || plan.resource_budget.max_sample_taps < 25u
            || !plan.quality_policy.allow_noise
            || !plan.quality_policy.allow_shadow);
}

inline char const* material_reference_performance_response(
        MaterialPlan const& plan) noexcept {
    if (plan.kind == MaterialKind::None)
        return "inactive";
    if (plan.backdrop_access.next_frame_capture_required
        && !plan.backdrop_sampling
        && plan.fallback_path == MaterialFallbackPath::NoBackdropSource)
        return "warmup-capture";
    if (plan.fallback())
        return "deterministic-fallback";
    if (material_reference_uses_budgeted_effects(plan))
        return "budgeted-effects";
    return "standard";
}

inline MaterialReferenceModel material_resolve_reference_model(
        MaterialPlan const& plan) noexcept {
    MaterialReferenceModel model{};
    model.technology = material_reference_technology(plan);
    model.layer = material_reference_layer(plan);
    model.material_policy = material_reference_policy(plan);
    model.variant = material_kind_name(plan.kind);
    model.shape = material_reference_shape_name(plan.shape);
    model.blending_scope = material_reference_blending_scope(plan);
    model.semantic_thickness = material_kind_name(plan.kind);
    model.accessibility_response =
        material_reference_accessibility_response(plan);
    model.performance_response =
        material_reference_performance_response(plan);
    model.view_bounds_anchored = plan.kind != MaterialKind::None
        && plan.shape.valid;
    model.shape_matches_geometry = model.view_bounds_anchored
        && plan.shape.effective_radius <= plan.shape.radius_limit;
    model.tint_applied = plan.kind != MaterialKind::None
        && plan.tint.a > 0;
    model.interactive_response = plan.container.interactive;
    model.container_grouped = plan.container.participates;
    model.container_union = plan.container.shape_union_expected;
    model.container_morphing = plan.container.morph_transitions;
    model.legibility_preserved =
        plan.foreground.primary_contrast_ratio
            >= plan.foreground.minimum_contrast_ratio
        && plan.foreground.secondary_contrast_ratio
            >= plan.foreground.minimum_contrast_ratio
        && plan.foreground.accent_contrast_ratio
            >= plan.foreground.minimum_contrast_ratio;
    model.vibrancy_expected = plan.foreground.uses_vibrancy;
    model.deterministic_degradation =
        plan.resource_budget.deterministic_fallback;
    return model;
}

inline MaterialPlan plan_material_surface(MaterialRequest request,
                                          MaterialEnvironment environment) noexcept {
    MaterialPlan plan{};
    auto const& style = request.style;
    MaterialQualityPolicy resolved_quality =
        sanitize_material_quality_policy(environment.quality);
    auto const max_blur_radius = std::max(
        0.0f,
        resolved_quality.max_blur_radius);
    plan.kind = style.kind;
    plan.role = style.role;
    plan.command_descriptor = material_command_descriptor(style);
    plan.geometry = request.geometry;
    plan.shape = analyze_material_shape(request.geometry);
    plan.quality_policy = resolved_quality;
    plan.render_target = analyze_material_render_target(
        environment.render_target,
        resolved_quality.max_backdrop_pixels);
    plan.container = analyze_material_container(
        style.container,
        environment.capabilities.reduce_motion);
    plan.opacity = std::clamp(style.opacity, 0.0f, 1.0f);
    plan.blur_radius = std::clamp(
        style.blur_radius, 0.0f, max_blur_radius);
    plan.tint = style.tint;
    plan.theme = material_resolve_theme_snapshot(style);
    plan.saturation = std::max(0.0f, style.saturation);
    plan.luminance_floor = std::clamp(style.luminance_floor, 0.0f, 1.0f);
    plan.luminance_gain = std::max(0.0f, style.luminance_gain);
    plan.edge_highlight = std::clamp(style.edge_highlight, 0.0f, 1.0f);
    plan.edge_width = std::max(0.5f, style.edge_width);
    plan.noise_opacity = resolved_quality.allow_noise
        ? std::clamp(style.noise_opacity, 0.0f, 0.05f)
        : 0.0f;
    plan.shadow_alpha = resolved_quality.allow_shadow
        ? std::clamp(style.shadow_alpha, 0.0f, 0.4f)
        : 0.0f;
    plan.shadow_radius = resolved_quality.allow_shadow
        ? std::max(0.0f, style.shadow_radius)
        : 0.0f;
    plan.contrast_intent = style.contrast_intent;
    plan.debug_seed = material_debug_seed(environment.debug_seed, style.kind);
    plan.sample_taps =
        material_resolve_sample_taps(resolved_quality.max_sample_taps);
    if (environment.capabilities.reduce_motion)
        plan.sample_taps = std::min(plan.sample_taps, 9u);
    plan.resource_budget.max_blur_radius = max_blur_radius;
    plan.resource_budget.max_sample_taps = plan.sample_taps;
    plan.resource_budget.max_sampling_kernel_radius = 0u;
    plan.resource_budget.max_pass_count = 1;
    plan.resource_budget.max_execution_stages = material_max_execution_stages;
    plan.resource_budget.max_backdrop_pixels =
        resolved_quality.max_backdrop_pixels;
    plan.resource_budget.max_frame_capture_count = 0;
    plan.resource_budget.max_frame_capture_pixels = 0;
    plan.resource_budget.max_surface_sample_pixels = 0;
    plan.resource_budget.max_container_spacing = plan.container.spacing;
    plan.resource_budget.bounded_texture_copy = true;
    plan.resource_budget.deterministic_fallback = true;
    plan.backdrop = analyze_material_backdrop(environment.backdrop);

    bool const has_geometry = plan.shape.valid;
    bool const has_material = style.kind != MaterialKind::None
        && style.tint.a > 0
        && plan.opacity > 0.0f
        && has_geometry;
    bool const role_allows_liquid_glass =
        material_role_allows_liquid_glass(style.role);
    bool const content_layer_standard_material =
        has_material && !role_allows_liquid_glass;
    bool const liquid_glass_backdrop_candidate =
        has_material && role_allows_liquid_glass;
    bool const target_ready = plan.render_target.ready;
    bool const backdrop_pixels_within_budget =
        plan.render_target.within_backdrop_budget;
    bool const quality_switches_allow_backdrop =
        resolved_quality.allow_backdrop_sampling
        && max_blur_radius > 0.0f
        && plan.sample_taps > 0u;
    bool const quality_allows_backdrop =
        quality_switches_allow_backdrop
        && backdrop_pixels_within_budget;
    bool const backend_supports_backdrop =
        environment.capabilities.material_surfaces
        && environment.capabilities.material_backdrop_blur
        && environment.capabilities.shader_blur;
    bool const backdrop_source_ready =
        environment.backdrop.available
        && environment.backdrop.stable;
    bool const can_sample_backdrop =
        liquid_glass_backdrop_candidate
        && target_ready
        && quality_allows_backdrop
        && backend_supports_backdrop
        && environment.capabilities.frame_history
        && backdrop_source_ready
        && !environment.capabilities.reduce_transparency;
    bool const next_frame_capture_required =
        liquid_glass_backdrop_candidate
        && target_ready
        && quality_allows_backdrop
        && backend_supports_backdrop
        && !environment.capabilities.reduce_transparency;
    plan.decision_trace.has_geometry = has_geometry;
    plan.decision_trace.has_material = has_material;
    plan.decision_trace.role_allows_liquid_glass =
        role_allows_liquid_glass;
    plan.decision_trace.content_layer_standard_material =
        content_layer_standard_material;
    plan.decision_trace.liquid_glass_backdrop_candidate =
        liquid_glass_backdrop_candidate;
    plan.decision_trace.target_ready = target_ready;
    plan.decision_trace.quality_switches_allow_backdrop =
        quality_switches_allow_backdrop;
    plan.decision_trace.backdrop_pixels_within_budget =
        backdrop_pixels_within_budget;
    plan.decision_trace.quality_allows_backdrop = quality_allows_backdrop;
    plan.decision_trace.capability_material_surfaces =
        environment.capabilities.material_surfaces;
    plan.decision_trace.capability_material_backdrop_blur =
        environment.capabilities.material_backdrop_blur;
    plan.decision_trace.capability_shader_blur =
        environment.capabilities.shader_blur;
    plan.decision_trace.capability_frame_history =
        environment.capabilities.frame_history;
    plan.decision_trace.backend_supports_backdrop = backend_supports_backdrop;
    plan.decision_trace.backdrop_available = environment.backdrop.available;
    plan.decision_trace.backdrop_stable = environment.backdrop.stable;
    plan.decision_trace.backdrop_source_ready = backdrop_source_ready;
    plan.decision_trace.next_frame_capture_required =
        next_frame_capture_required;
    plan.decision_trace.reduced_transparency =
        environment.capabilities.reduce_transparency;
    plan.decision_trace.increase_contrast =
        environment.capabilities.increase_contrast;
    plan.decision_trace.reduce_motion =
        environment.capabilities.reduce_motion;
    plan.decision_trace.can_sample_backdrop = can_sample_backdrop;

    if (!has_material) {
        plan.fallback_path = style.kind == MaterialKind::None
            ? MaterialFallbackPath::NoMaterial
            : MaterialFallbackPath::InvalidGeometry;
        plan.fallback_reason = style.kind == MaterialKind::None
            ? "material kind is none"
            : "material geometry or opacity is invalid";
        plan.blur_radius = 0.0f;
        plan.opacity = 0.0f;
        plan.tint.a = 0;
    } else if (liquid_glass_backdrop_candidate
               && environment.capabilities.reduce_transparency) {
        plan.fallback_path = MaterialFallbackPath::ReducedTransparency;
        plan.fallback_reason = "reduced transparency disables backdrop sampling";
        plan.blur_radius = 0.0f;
        plan.saturation = 1.0f;
        plan.noise_opacity = 0.0f;
    } else if (liquid_glass_backdrop_candidate
               && !quality_switches_allow_backdrop) {
        plan.fallback_path = MaterialFallbackPath::QualityPolicy;
        plan.fallback_reason = "quality policy disables material backdrop sampling";
        plan.blur_radius = 0.0f;
        plan.saturation = 1.0f;
        plan.noise_opacity = 0.0f;
    } else if (liquid_glass_backdrop_candidate
               && (!environment.capabilities.material_surfaces
                   || !environment.capabilities.material_backdrop_blur
                   || !environment.capabilities.shader_blur)) {
        plan.fallback_path = MaterialFallbackPath::UnsupportedBackend;
        plan.fallback_reason = "backend reports no material backdrop blur support";
        plan.blur_radius = 0.0f;
        plan.saturation = 1.0f;
        plan.noise_opacity = 0.0f;
    } else if (liquid_glass_backdrop_candidate
               && !backdrop_pixels_within_budget) {
        plan.fallback_path = MaterialFallbackPath::QualityPolicy;
        plan.fallback_reason = "quality policy backdrop pixel budget exceeded";
        plan.blur_radius = 0.0f;
        plan.saturation = 1.0f;
        plan.noise_opacity = 0.0f;
    } else if (liquid_glass_backdrop_candidate
               && (!target_ready || !environment.capabilities.frame_history
                   || !environment.backdrop.available
                   || !environment.backdrop.stable)) {
        plan.fallback_path = MaterialFallbackPath::NoBackdropSource;
        plan.fallback_reason = "stable backdrop source is not available for this frame";
        plan.blur_radius = 0.0f;
        plan.saturation = 1.0f;
        plan.noise_opacity = 0.0f;
    }
    if (content_layer_standard_material) {
        plan.blur_radius = 0.0f;
        plan.saturation = 1.0f;
        plan.noise_opacity = 0.0f;
    }
    plan.decision_trace.first_blocker =
        material_fallback_path_name(plan.fallback_path);

    plan.backdrop_sampling = can_sample_backdrop;
    if (!plan.backdrop_sampling) {
        plan.sample_taps = 0u;
    } else {
        apply_backdrop_luminance_policy(plan);
    }
    plan.sampling_kernel = material_resolve_sampling_kernel(
        plan.backdrop_sampling,
        plan.sample_taps);
    plan.resource_budget.max_sampling_kernel_radius =
        plan.sampling_kernel.radius;
    plan.backdrop_access = material_resolve_backdrop_access(plan);
    plan.resource_budget.max_frame_capture_count =
        plan.backdrop_access.max_frame_capture_count;
    plan.resource_budget.max_frame_capture_pixels =
        plan.backdrop_access.max_frame_capture_pixels;
    plan.resource_budget.max_surface_sample_pixels =
        plan.backdrop_access.max_surface_sample_pixels;
    if (environment.capabilities.reduce_motion) {
        plan.noise_opacity = 0.0f;
    }
    if (environment.capabilities.increase_contrast) {
        plan.opacity = std::clamp(plan.opacity + 0.12f, 0.0f, 1.0f);
        plan.luminance_floor = std::clamp(plan.luminance_floor + 0.05f, 0.0f, 1.0f);
        plan.saturation = std::min(plan.saturation, 1.0f);
    }
    plan.luminance_curve = material_resolve_luminance_curve(
        plan.backdrop_sampling,
        plan.backdrop,
        plan.luminance_floor,
        plan.luminance_gain,
        plan.edge_highlight);
    plan.foreground = material_resolve_foreground(
        plan,
        style,
        environment.capabilities);
    plan.reference_model = material_resolve_reference_model(plan);

    plan.plan_id = material_plan_id(
        style.kind,
        style.role,
        plan.backdrop_sampling);
    char const* const non_backdrop_material_layer =
        material_plan_uses_standard_content_layer(plan)
            ? "material-standard-pass"
            : "material-fallback-pass";
    if (has_material && plan.backdrop_sampling && !plan.fallback()) {
        plan.primary_pass = MaterialPassExpectation{
            "backdrop-sample-blur",
            true,
            true,
            plan.sample_taps,
            "material-blur-pass",
            "backdrop-filter",
            plan.render_target.pixel_count,
        };
    } else if (content_layer_standard_material && !plan.fallback()) {
        plan.primary_pass = MaterialPassExpectation{
            "standard-material-fill",
            true,
            false,
            0u,
            "material-standard-pass",
            "standard-fill",
            0,
        };
    } else if (has_material && plan.fallback_path != MaterialFallbackPath::InvalidGeometry) {
        plan.primary_pass = MaterialPassExpectation{
            "translucent-rounded-rect",
            true,
            false,
            0u,
            "material-fallback-pass",
            "fallback-fill",
            0,
        };
    } else {
        plan.primary_pass = MaterialPassExpectation{
            "none",
            false,
            false,
            0u,
            "material-fallback-pass",
            "none",
            0,
        };
    }
    if (has_material && plan.shadow_alpha > 0.0f) {
        append_material_execution_stage(
            plan,
            MaterialExecutionStage{
                "shape-shadow",
                true,
                false,
                0u,
                "material-shadow-pass",
                "shape-shadow",
                0,
                true,
            });
    }
    if (plan.primary_pass.active) {
        append_material_execution_stage(
            plan,
            MaterialExecutionStage{
                plan.primary_pass.name,
                true,
                plan.primary_pass.requires_backdrop,
                plan.primary_pass.sample_taps,
                plan.primary_pass.likely_layer,
                plan.primary_pass.executor,
                plan.primary_pass.max_texture_copy_pixels,
                true,
            });
    }
    if (has_material && plan.edge_highlight > 0.0f && plan.primary_pass.active) {
        append_material_execution_stage(
            plan,
            MaterialExecutionStage{
                "edge-highlight",
                true,
                false,
                0u,
                plan.backdrop_sampling ? "material-edge-pass"
                                       : non_backdrop_material_layer,
                "edge-highlight",
                0,
                true,
            });
    }
    if (has_material && plan.noise_opacity > 0.0f && plan.backdrop_sampling) {
        append_material_execution_stage(
            plan,
            MaterialExecutionStage{
                "noise-dither",
                true,
                false,
                0u,
                "material-noise-pass",
                "ordered-dither",
                0,
                true,
            });
    }
    plan.verifier.require_backdrop_source = plan.backdrop_sampling;
    plan.verifier.require_edge_highlight = plan.edge_highlight > 0.0f
        && !plan.fallback();
    plan.verifier.require_container_identity = plan.container.participates;
    plan.verifier.require_container_morph_contract =
        plan.container.morph_transitions;
    plan.verifier.min_luma_delta = plan.backdrop_sampling ? 8.0f : 3.0f;
    plan.verifier.min_unique_colors = plan.backdrop_sampling ? 4 : 2;
    plan.verifier.region_name = style.verifier_profile;
    plan.verifier.likely_layer = plan.backdrop_sampling
        ? "material-blur-pass"
        : non_backdrop_material_layer;
    plan.verifier.likely_pass = plan.primary_pass.name;
    plan.observation_contract = material_observation_contract(plan);
    return plan;
}

} // namespace phenotype
