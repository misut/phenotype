module;
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cmath>
#include <span>
#include <string_view>

export module phenotype.material;

import phenotype.types;
import phenotype.theme_contract;

export namespace phenotype {

inline constexpr std::uint32_t material_plan_contract_version = 44;
inline constexpr unsigned int material_max_execution_stages = 4;
inline constexpr unsigned int material_max_paint_layers = 3;
inline constexpr float material_max_blur_radius = 36.0f;
inline constexpr unsigned int material_max_sample_taps = 25;
inline constexpr std::int64_t material_default_max_backdrop_pixels = 4'000'000;

struct MaterialGeometry {
    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;
    float radius = 0.0f;
};

enum class MaterialShapeKind {
    Invalid,
    Rectangle,
    RoundedRectangle,
    Capsule,
};

inline char const* material_shape_kind_name(MaterialShapeKind kind) noexcept {
    switch (kind) {
        case MaterialShapeKind::Invalid: return "invalid";
        case MaterialShapeKind::Rectangle: return "rectangle";
        case MaterialShapeKind::RoundedRectangle: return "rounded-rectangle";
        case MaterialShapeKind::Capsule: return "capsule";
    }
    return "invalid";
}

struct MaterialShapeAnalysis {
    bool valid = false;
    MaterialShapeKind kind = MaterialShapeKind::Invalid;
    bool rounded = false;
    bool capsule = false;
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
    unsigned int max_shader_sample_taps = material_max_sample_taps;
    std::int64_t max_texture_dimension_2d = 0;
    std::int64_t max_backdrop_pixels = 0;
    char const* profile = "generic";
    char const* source = "default";
};

struct MaterialBackdropDescriptor {
    bool available = false;
    bool stable = false;
    bool excludes_foreground_text = false;
    Color color_mean = {255, 255, 255, 255};
    std::uint32_t color_sample_count = 0;
    char const* color_sample_status = "not-sampled";
    float luma_min = 0.0f;
    float luma_max = 1.0f;
    float luma_mean = 0.5f;
    std::uint32_t luma_sample_count = 0;
    std::uint32_t luma_sample_grid_width = 0;
    std::uint32_t luma_sample_grid_height = 0;
    std::uint64_t luma_sample_frame = 0;
    char const* luma_sample_status = "not-sampled";
    char const* source = "none";
};

struct MaterialRenderTargetMetadata {
    int width = 0;
    int height = 0;
    float scale = 1.0f;
    char const* pixel_format = "unknown";
};

struct MaterialCapabilityAnalysis {
    bool material_surfaces = true;
    bool material_backdrop_blur = false;
    bool shader_blur = false;
    bool frame_history = false;
    bool reduce_transparency = false;
    bool increase_contrast = false;
    bool reduce_motion = false;
    unsigned int max_shader_sample_taps = material_max_sample_taps;
    std::int64_t max_texture_dimension_2d = 0;
    std::int64_t max_backdrop_pixels = 0;
    bool texture_limits_known = false;
    bool backdrop_budget_known = false;
    bool target_within_texture_limits = true;
    bool target_within_backdrop_budget = true;
    char const* profile = "generic";
    char const* source = "default";
};

inline MaterialCapabilityInput sanitize_material_capability_input(
        MaterialCapabilityInput input) noexcept {
    input.max_shader_sample_taps =
        std::min(input.max_shader_sample_taps, material_max_sample_taps);
    input.max_texture_dimension_2d =
        std::max(std::int64_t{0}, input.max_texture_dimension_2d);
    input.max_backdrop_pixels =
        std::max(std::int64_t{0}, input.max_backdrop_pixels);
    if (input.profile == nullptr)
        input.profile = "generic";
    if (input.source == nullptr)
        input.source = "default";
    return input;
}

inline std::int64_t material_render_target_pixel_count(
        MaterialRenderTargetMetadata const& target) noexcept {
    if (target.width <= 0 || target.height <= 0)
        return 0;
    return static_cast<std::int64_t>(target.width)
         * static_cast<std::int64_t>(target.height);
}

inline MaterialCapabilityAnalysis analyze_material_capabilities(
        MaterialCapabilityInput input,
        MaterialRenderTargetMetadata target) noexcept {
    input = sanitize_material_capability_input(input);
    MaterialCapabilityAnalysis analysis{};
    analysis.material_surfaces = input.material_surfaces;
    analysis.material_backdrop_blur = input.material_backdrop_blur;
    analysis.shader_blur = input.shader_blur;
    analysis.frame_history = input.frame_history;
    analysis.reduce_transparency = input.reduce_transparency;
    analysis.increase_contrast = input.increase_contrast;
    analysis.reduce_motion = input.reduce_motion;
    analysis.max_shader_sample_taps = input.max_shader_sample_taps;
    analysis.max_texture_dimension_2d = input.max_texture_dimension_2d;
    analysis.max_backdrop_pixels = input.max_backdrop_pixels;
    analysis.texture_limits_known = input.max_texture_dimension_2d > 0;
    analysis.backdrop_budget_known = input.max_backdrop_pixels > 0;
    analysis.profile = input.profile;
    analysis.source = input.source;

    if (analysis.texture_limits_known && target.width > 0 && target.height > 0) {
        analysis.target_within_texture_limits =
            static_cast<std::int64_t>(target.width)
                <= analysis.max_texture_dimension_2d
            && static_cast<std::int64_t>(target.height)
                <= analysis.max_texture_dimension_2d;
    }
    if (analysis.backdrop_budget_known) {
        auto const pixels = material_render_target_pixel_count(target);
        analysis.target_within_backdrop_budget =
            pixels <= analysis.max_backdrop_pixels;
    }
    return analysis;
}

struct MaterialQualityPolicy {
    bool allow_backdrop_sampling = true;
    bool allow_noise = true;
    bool allow_shadow = true;
    float max_blur_radius = material_max_blur_radius;
    unsigned int max_sample_taps = material_max_sample_taps;
    std::int64_t max_backdrop_pixels = material_default_max_backdrop_pixels;
};

inline constexpr MaterialQualityPolicy default_material_quality_policy() noexcept {
    return MaterialQualityPolicy{};
}

inline MaterialQualityPolicy sanitize_material_quality_policy(
        MaterialQualityPolicy policy) noexcept {
    policy.max_blur_radius = std::isfinite(policy.max_blur_radius)
        ? std::clamp(
            policy.max_blur_radius,
            0.0f,
            material_max_blur_radius)
        : 0.0f;
    policy.max_sample_taps = std::min(
        policy.max_sample_taps,
        material_max_sample_taps);
    policy.max_backdrop_pixels =
        std::max(std::int64_t{0}, policy.max_backdrop_pixels);
    return policy;
}

inline MaterialQualityPolicy resolve_material_quality_policy(
        MaterialQualityPolicy policy,
        MaterialCapabilityAnalysis capabilities) noexcept {
    policy = sanitize_material_quality_policy(policy);
    if (capabilities.max_shader_sample_taps > 0) {
        policy.max_sample_taps = std::min(
            policy.max_sample_taps,
            capabilities.max_shader_sample_taps);
    }
    if (capabilities.max_backdrop_pixels > 0) {
        policy.max_backdrop_pixels = std::min(
            policy.max_backdrop_pixels,
            capabilities.max_backdrop_pixels);
    }
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
    char const* expected_stage_order = "none";
    std::uint32_t expected_runtime_passes = 0;
    std::uint32_t expected_active_runtime_passes = 0;
    std::uint32_t expected_backdrop_runtime_passes = 0;
    std::uint32_t expected_execution_stages = 0;
    std::uint32_t expected_active_execution_stages = 0;
    std::uint32_t expected_backdrop_execution_stages = 0;
    std::uint32_t expected_paint_layers = 0;
    std::uint32_t expected_active_paint_layers = 0;
    std::uint32_t expected_shadow_paint_layers = 0;
    std::uint32_t expected_fill_paint_layers = 0;
    std::uint32_t expected_edge_paint_layers = 0;
    std::uint32_t max_frame_capture_count = 0;
    std::int64_t max_frame_capture_pixels = 0;
    std::int64_t max_surface_sample_pixels = 0;
    std::int64_t max_texture_copy_pixels = 0;
    char const* region_name = "material";
    char const* likely_layer = "material-fallback";
    char const* likely_pass = "none";
};

struct MaterialExecutionAudit {
    std::uint32_t schema_version = material_plan_contract_version;
    std::uint32_t actual_runtime_passes = 0;
    std::uint32_t actual_active_runtime_passes = 0;
    std::uint32_t actual_backdrop_runtime_passes = 0;
    std::uint32_t actual_execution_stages = 0;
    std::uint32_t actual_active_execution_stages = 0;
    std::uint32_t actual_backdrop_execution_stages = 0;
    std::uint32_t actual_paint_layers = 0;
    std::uint32_t actual_active_paint_layers = 0;
    std::uint32_t actual_shadow_paint_layers = 0;
    std::uint32_t actual_fill_paint_layers = 0;
    std::uint32_t actual_edge_paint_layers = 0;
    bool runtime_passes_match = true;
    bool active_runtime_passes_match = true;
    bool backdrop_runtime_passes_match = true;
    bool execution_stages_match = true;
    bool active_execution_stages_match = true;
    bool backdrop_execution_stages_match = true;
    bool paint_layers_match = true;
    bool active_paint_layers_match = true;
    bool shadow_paint_layers_match = true;
    bool fill_paint_layers_match = true;
    bool edge_paint_layers_match = true;
    bool stage_order_match = true;
    bool bounded_texture_copy = true;
    bool deterministic_fallback = true;
    bool contract_satisfied = true;
    std::uint32_t mismatch_count = 0;
    char const* first_mismatch = "none";
    char const* expected_stage_order = "none";
    char const* actual_stage_order = "none";
    char const* likely_layer = "material-execution-contract";
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
    unsigned int max_paint_layers = material_max_paint_layers;
    std::int64_t max_backdrop_pixels = 0;
    std::uint32_t max_frame_capture_count = 0;
    std::int64_t max_frame_capture_pixels = 0;
    std::int64_t max_surface_sample_pixels = 0;
    float max_refraction_offset_pixels = 0.0f;
    float max_container_spacing = 0.0f;
    float max_paint_layer_inflate = 0.0f;
    bool bounded_texture_copy = true;
    bool deterministic_fallback = true;
};

struct MaterialStageOptics {
    char const* channel = "none";
    float opacity = 0.0f;
    float blur_radius = 0.0f;
    float tint_alpha = 0.0f;
    float saturation = 1.0f;
    float luminance_floor = 0.0f;
    float luminance_gain = 1.0f;
    float edge_highlight = 0.0f;
    float edge_width = 0.0f;
    float noise_opacity = 0.0f;
    float shadow_alpha = 0.0f;
    float shadow_radius = 0.0f;
    char const* specular_model = "none";
    float specular_anchor_x = 0.5f;
    float specular_anchor_y = 0.5f;
    float specular_radius = 0.0f;
    float specular_intensity = 0.0f;
    char const* refraction_model = "none";
    float refraction_strength = 0.0f;
    float refraction_edge_bias = 0.0f;
    float refraction_offset_pixels = 0.0f;
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
    MaterialStageOptics optics{};
};

struct MaterialPaintLayer {
    char const* name = "none";
    bool active = false;
    char const* executor = "none";
    float x_offset = 0.0f;
    float y_offset = 0.0f;
    float inflate = 0.0f;
    float radius_delta = 0.0f;
    float stroke_width = 0.0f;
    Color color = {0, 0, 0, 0};
    float opacity = 0.0f;
    bool bounded = true;
};

struct MaterialBackdropAnalysis {
    bool available = false;
    bool stable = false;
    bool excludes_foreground_text = false;
    Color color_mean = {255, 255, 255, 255};
    std::uint32_t color_sample_count = 0;
    char const* color_sample_status = "not-sampled";
    float luma_min = 0.0f;
    float luma_max = 1.0f;
    float luma_mean = 0.5f;
    float luma_span = 1.0f;
    std::uint32_t luma_sample_count = 0;
    std::uint32_t luma_sample_grid_width = 0;
    std::uint32_t luma_sample_grid_height = 0;
    std::uint64_t luma_sample_frame = 0;
    char const* luma_sample_status = "not-sampled";
    char const* source = "none";
    char const* luminance_response = "not-sampled";
    char const* frosting_response = "not-sampled";
    char const* color_response = "not-sampled";
    char const* tint_response = "not-sampled";
    char const* saturation_response = "not-sampled";
    char const* depth_response = "not-sampled";
    float luminance_floor_delta = 0.0f;
    float luminance_gain_delta = 0.0f;
    float edge_highlight_delta = 0.0f;
    float opacity_delta = 0.0f;
    float tint_color_delta = 0.0f;
    float tint_alpha_delta = 0.0f;
    float saturation_delta = 0.0f;
    float shadow_alpha_delta = 0.0f;
    float shadow_radius_delta = 0.0f;
    float response_strength = 0.0f;
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
    char const* contrast_policy = "standard-contrast";
    char const* remap_policy = "theme-role-remap-if-needed";
    float background_luma = 1.0f;
    float primary_contrast_ratio = 1.0f;
    float secondary_contrast_ratio = 1.0f;
    float accent_contrast_ratio = 1.0f;
    float minimum_contrast_ratio = 4.5f;
    float primary_contrast_margin = 0.0f;
    float secondary_contrast_margin = 0.0f;
    float accent_contrast_margin = 0.0f;
    bool backdrop_driven = false;
    bool high_contrast = false;
    bool uses_vibrancy = false;
    bool deterministic = true;
};

struct MaterialRefractionProfile {
    char const* model = "none";
    char const* source = "none";
    bool active = false;
    bool backdrop_driven = false;
    bool interaction_driven = false;
    bool reduced_motion_suppressed = false;
    bool bounded = true;
    float strength = 0.0f;
    float edge_bias = 0.0f;
    float max_offset_pixels = 0.0f;
};

struct MaterialInteractionResponse {
    bool enabled = false;
    bool active = false;
    bool hovered = false;
    bool pressed = false;
    bool focused = false;
    bool pointer_inside = false;
    bool reduce_motion = false;
    float pointer_x = 0.5f;
    float pointer_y = 0.5f;
    float response_strength = 0.0f;
    float opacity_delta = 0.0f;
    float blur_radius_delta = 0.0f;
    float saturation_delta = 0.0f;
    float edge_highlight_delta = 0.0f;
    float shadow_alpha_delta = 0.0f;
    float shadow_radius_delta = 0.0f;
    char const* state = "inactive";
    char const* enablement_reason = "inactive-material";
    char const* response_model = "none";
    char const* motion_policy = "static";
    char const* specular_model = "none";
    bool specular_highlight_active = false;
    float specular_anchor_x = 0.5f;
    float specular_anchor_y = 0.5f;
    float specular_radius = 0.0f;
    float specular_intensity = 0.0f;
    bool deterministic = true;
};

struct MaterialOpticalResponseContract {
    char const* response_model = "inactive";
    char const* blur_strategy = "none";
    char const* color_strategy = "none";
    char const* depth_strategy = "none";
    bool backdrop_driven = false;
    bool blur_active = false;
    bool frosting_active = false;
    bool tint_active = false;
    bool saturation_active = false;
    bool luminance_preservation_active = false;
    bool edge_highlight_active = false;
    bool depth_shadow_active = false;
    bool noise_dither_active = false;
    bool refraction_active = false;
    bool foreground_vibrancy_active = false;
    bool interaction_active = false;
    bool interaction_modulates_optics = false;
    bool deterministic_fallback = true;
};

struct MaterialOpticalComposition {
    std::uint32_t schema_version = material_plan_contract_version;
    char const* model = "inactive";
    char const* blur_source = "none";
    char const* frosting_source = "none";
    char const* tint_source = "none";
    char const* luminance_source = "none";
    char const* depth_source = "none";
    char const* refraction_source = "none";
    char const* interaction_source = "none";
    char const* fallback_source = "none";
    char const* stage_order = "none";
    char const* backdrop_capture_policy = "no-capture";
    char const* foreground_sampling_policy = "not-applicable";
    bool backdrop_sampled = false;
    bool blur_required = false;
    bool frosting_required = false;
    bool tint_required = false;
    bool saturation_required = false;
    bool luminance_required = false;
    bool edge_required = false;
    bool shadow_required = false;
    bool noise_required = false;
    bool refraction_required = false;
    bool interaction_required = false;
    bool fallback_required = false;
    bool backdrop_capture_required = false;
    bool foreground_excluded_from_backdrop = false;
    bool stage_order_stable = true;
    bool bounded = true;
    bool deterministic = true;
    float opacity = 0.0f;
    float blur_radius = 0.0f;
    float tint_alpha = 0.0f;
    float saturation = 1.0f;
    float luminance_floor = 0.0f;
    float luminance_gain = 1.0f;
    float edge_highlight = 0.0f;
    float edge_width = 0.0f;
    float noise_opacity = 0.0f;
    float shadow_alpha = 0.0f;
    float shadow_radius = 0.0f;
    float refraction_strength = 0.0f;
    float refraction_edge_bias = 0.0f;
    float refraction_offset_pixels = 0.0f;
    float interaction_response_strength = 0.0f;
    unsigned int sample_taps = 0;
    std::int64_t max_texture_copy_pixels = 0;
    std::int64_t max_surface_sample_pixels = 0;
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
    float blend_distance = 0.0f;
    bool interactive = false;
    bool morph_transitions = false;
    bool requested_morph_transitions = false;
    bool participates = false;
    MaterialContainerMode mode = MaterialContainerMode::Isolated;
    char const* mode_name = "isolated";
    bool shared_backdrop_scope = false;
    bool shape_union_expected = false;
    bool shape_blending_expected = false;
    bool reduced_motion_suppressed_morph = false;
    bool spacing_clamped = false;
    char const* blend_policy = "isolated";
    char const* morph_policy = "isolated";
    char const* performance_policy = "single-surface";
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
    bool capability_texture_limits_known = false;
    bool capability_backdrop_budget_known = false;
    bool capability_target_within_texture_limits = true;
    bool capability_target_within_backdrop_budget = true;
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
    MaterialCapabilityAnalysis capability_snapshot{};
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
    MaterialRefractionProfile refraction{};
    MaterialInteractionResponse interaction{};
    MaterialOpticalComposition optical_composition{};
    MaterialOpticalResponseContract optical_response{};
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
    unsigned int paint_layer_capacity = material_max_paint_layers;
    unsigned int paint_layer_count = 0;
    unsigned int dropped_paint_layer_count = 0;
    MaterialPaintLayer paint_layers[material_max_paint_layers]{};
    MaterialVerifierExpectation verifier{};
    MaterialObservationContract observation_contract{};
    MaterialExecutionAudit execution_audit{};

    constexpr bool fallback() const noexcept {
        return fallback_path != MaterialFallbackPath::None;
    }
};

struct MaterialRuntimeRecord {
    MaterialPlan plan{};
    std::uint32_t command_index = 0;
};

inline float material_alpha_fraction(Color color) noexcept {
    return static_cast<float>(color.a) / 255.0f;
}

inline MaterialStageOptics material_primary_stage_optics(
        MaterialPlan const& plan) noexcept {
    auto const& composition = plan.optical_composition;
    MaterialStageOptics optics{};
    optics.channel = plan.primary_pass.executor;
    optics.opacity = composition.opacity;
    optics.blur_radius = composition.blur_radius;
    optics.tint_alpha = composition.tint_alpha;
    optics.saturation = composition.saturation;
    optics.luminance_floor = composition.luminance_floor;
    optics.luminance_gain = composition.luminance_gain;
    optics.refraction_model = plan.refraction.model;
    optics.refraction_strength = plan.refraction.strength;
    optics.refraction_edge_bias = plan.refraction.edge_bias;
    optics.refraction_offset_pixels = plan.refraction.max_offset_pixels;
    return optics;
}

inline MaterialStageOptics material_shadow_stage_optics(
        MaterialPlan const& plan) noexcept {
    auto const& composition = plan.optical_composition;
    MaterialStageOptics optics{};
    optics.channel = "shape-shadow";
    optics.edge_width = composition.edge_width;
    optics.shadow_alpha = composition.shadow_alpha;
    optics.shadow_radius = composition.shadow_radius;
    return optics;
}

inline MaterialStageOptics material_edge_stage_optics(
        MaterialPlan const& plan) noexcept {
    auto const& composition = plan.optical_composition;
    MaterialStageOptics optics{};
    optics.channel = "edge-highlight";
    optics.edge_highlight = composition.edge_highlight;
    optics.edge_width = composition.edge_width;
    optics.specular_model = plan.interaction.specular_model;
    optics.specular_anchor_x = plan.interaction.specular_anchor_x;
    optics.specular_anchor_y = plan.interaction.specular_anchor_y;
    optics.specular_radius = plan.interaction.specular_radius;
    optics.specular_intensity = plan.interaction.specular_intensity;
    return optics;
}

inline MaterialStageOptics material_noise_stage_optics(
        MaterialPlan const& plan) noexcept {
    auto const& composition = plan.optical_composition;
    MaterialStageOptics optics{};
    optics.channel = "noise-dither";
    optics.noise_opacity = composition.noise_opacity;
    return optics;
}

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

inline bool material_paint_layer_matches(
        char const* actual,
        std::string_view expected) noexcept {
    return actual && std::string_view{actual} == expected;
}

inline void append_material_paint_layer(
        MaterialPlan& plan,
        MaterialPaintLayer layer) noexcept {
    if (!layer.active)
        return;
    auto const capacity =
        std::min(plan.paint_layer_capacity, material_max_paint_layers);
    if (plan.paint_layer_count >= capacity) {
        ++plan.dropped_paint_layer_count;
        return;
    }
    layer.inflate = std::max(0.0f, layer.inflate);
    layer.stroke_width = std::max(0.0f, layer.stroke_width);
    layer.opacity = std::clamp(layer.opacity, 0.0f, 1.0f);
    plan.resource_budget.max_paint_layer_inflate = std::max(
        plan.resource_budget.max_paint_layer_inflate,
        layer.inflate);
    plan.paint_layers[plan.paint_layer_count++] = layer;
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
    contract.expected_stage_order = plan.optical_composition.stage_order;
    contract.expected_runtime_passes = 1;
    contract.expected_active_runtime_passes =
        plan.primary_pass.active ? 1u : 0u;
    contract.expected_backdrop_runtime_passes =
        plan.primary_pass.requires_backdrop ? 1u : 0u;
    contract.expected_execution_stages = plan.execution_stage_count;
    contract.expected_paint_layers = plan.paint_layer_count;
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
    for (unsigned int i = 0; i < plan.paint_layer_count; ++i) {
        auto const& layer = plan.paint_layers[i];
        if (layer.active)
            ++contract.expected_active_paint_layers;
        if (material_paint_layer_matches(layer.executor, "rounded-shadow"))
            ++contract.expected_shadow_paint_layers;
        if (material_paint_layer_matches(layer.executor, "rounded-fill"))
            ++contract.expected_fill_paint_layers;
        if (material_paint_layer_matches(layer.executor, "rounded-edge"))
            ++contract.expected_edge_paint_layers;
    }
    return contract;
}

inline void material_record_execution_mismatch(
        MaterialExecutionAudit& audit,
        bool condition,
        char const* mismatch) noexcept {
    if (condition)
        return;
    audit.contract_satisfied = false;
    ++audit.mismatch_count;
    if (audit.first_mismatch == nullptr
        || std::string_view{audit.first_mismatch} == "none") {
        audit.first_mismatch =
            mismatch != nullptr && mismatch[0] ? mismatch : "unknown";
    }
}

inline char const* material_execution_stage_order_name(
        MaterialPlan const& plan) noexcept;

inline MaterialExecutionAudit material_execution_audit(
        MaterialPlan const& plan) noexcept {
    auto const& observation = plan.observation_contract;
    MaterialExecutionAudit audit{};
    audit.schema_version = plan.contract_version;
    audit.actual_runtime_passes = 1;
    audit.actual_active_runtime_passes =
        plan.primary_pass.active ? 1u : 0u;
    audit.actual_backdrop_runtime_passes =
        plan.primary_pass.requires_backdrop ? 1u : 0u;
    audit.actual_execution_stages = plan.execution_stage_count;
    audit.actual_paint_layers = plan.paint_layer_count;
    audit.expected_stage_order = observation.expected_stage_order;
    audit.actual_stage_order = material_execution_stage_order_name(plan);
    audit.bounded_texture_copy = plan.resource_budget.bounded_texture_copy;
    audit.deterministic_fallback =
        plan.resource_budget.deterministic_fallback;
    audit.likely_layer =
        observation.likely_layer != nullptr && observation.likely_layer[0]
            ? observation.likely_layer
            : "material-execution-contract";
    audit.likely_pass =
        observation.likely_pass != nullptr && observation.likely_pass[0]
            ? observation.likely_pass
            : "none";

    for (unsigned int i = 0; i < plan.execution_stage_count; ++i) {
        auto const& stage = plan.execution_stages[i];
        if (stage.active)
            ++audit.actual_active_execution_stages;
        if (stage.requires_backdrop)
            ++audit.actual_backdrop_execution_stages;
    }
    for (unsigned int i = 0; i < plan.paint_layer_count; ++i) {
        auto const& layer = plan.paint_layers[i];
        if (layer.active)
            ++audit.actual_active_paint_layers;
        if (material_paint_layer_matches(layer.executor, "rounded-shadow"))
            ++audit.actual_shadow_paint_layers;
        if (material_paint_layer_matches(layer.executor, "rounded-fill"))
            ++audit.actual_fill_paint_layers;
        if (material_paint_layer_matches(layer.executor, "rounded-edge"))
            ++audit.actual_edge_paint_layers;
    }

    audit.runtime_passes_match =
        observation.expected_runtime_passes == audit.actual_runtime_passes;
    audit.active_runtime_passes_match =
        observation.expected_active_runtime_passes
            == audit.actual_active_runtime_passes;
    audit.backdrop_runtime_passes_match =
        observation.expected_backdrop_runtime_passes
            == audit.actual_backdrop_runtime_passes;
    audit.execution_stages_match =
        observation.expected_execution_stages == audit.actual_execution_stages;
    audit.active_execution_stages_match =
        observation.expected_active_execution_stages
            == audit.actual_active_execution_stages;
    audit.backdrop_execution_stages_match =
        observation.expected_backdrop_execution_stages
            == audit.actual_backdrop_execution_stages;
    audit.paint_layers_match =
        observation.expected_paint_layers == audit.actual_paint_layers;
    audit.active_paint_layers_match =
        observation.expected_active_paint_layers
            == audit.actual_active_paint_layers;
    audit.shadow_paint_layers_match =
        observation.expected_shadow_paint_layers
            == audit.actual_shadow_paint_layers;
    audit.fill_paint_layers_match =
        observation.expected_fill_paint_layers
            == audit.actual_fill_paint_layers;
    audit.edge_paint_layers_match =
        observation.expected_edge_paint_layers
            == audit.actual_edge_paint_layers;
    audit.stage_order_match =
        std::string_view{audit.expected_stage_order
            ? audit.expected_stage_order
            : "none"}
        == std::string_view{audit.actual_stage_order
            ? audit.actual_stage_order
            : "none"};

    material_record_execution_mismatch(
        audit, audit.runtime_passes_match, "runtime-pass-count");
    material_record_execution_mismatch(
        audit, audit.active_runtime_passes_match, "active-runtime-pass-count");
    material_record_execution_mismatch(
        audit, audit.backdrop_runtime_passes_match, "backdrop-runtime-pass-count");
    material_record_execution_mismatch(
        audit, audit.execution_stages_match, "execution-stage-count");
    material_record_execution_mismatch(
        audit, audit.active_execution_stages_match, "active-execution-stage-count");
    material_record_execution_mismatch(
        audit, audit.backdrop_execution_stages_match, "backdrop-execution-stage-count");
    material_record_execution_mismatch(
        audit, audit.paint_layers_match, "paint-layer-count");
    material_record_execution_mismatch(
        audit, audit.active_paint_layers_match, "active-paint-layer-count");
    material_record_execution_mismatch(
        audit, audit.shadow_paint_layers_match, "shadow-paint-layer-count");
    material_record_execution_mismatch(
        audit, audit.fill_paint_layers_match, "fill-paint-layer-count");
    material_record_execution_mismatch(
        audit, audit.edge_paint_layers_match, "edge-paint-layer-count");
    material_record_execution_mismatch(
        audit, audit.stage_order_match, "stage-order");
    material_record_execution_mismatch(
        audit,
        !observation.bounded_texture_copy_required
            || audit.bounded_texture_copy,
        "bounded-texture-copy");
    material_record_execution_mismatch(
        audit,
        !observation.deterministic_fallback_required
            || audit.deterministic_fallback,
        "deterministic-fallback");
    return audit;
}

struct MaterialForegroundTextResolution {
    Color color{};
    char const* role = "none";
    std::uint32_t material_command_index = 0;
    bool has_material = false;
    bool remapped = false;
};

struct MaterialContainerGroupRuntimeSummary {
    std::uint32_t group_count = 0;
    std::uint32_t multi_surface_group_count = 0;
    std::uint32_t union_group_count = 0;
    std::uint32_t morph_group_count = 0;
    std::uint32_t interactive_group_count = 0;
    std::uint32_t shared_backdrop_scope_group_count = 0;
    std::uint32_t shared_capture_surface_count = 0;
    std::uint32_t shared_capture_saved_surface_count = 0;
    std::uint32_t max_shared_capture_group_surfaces = 0;
    std::uint32_t shape_blend_execution_group_count = 0;
    std::uint32_t shape_blend_execution_surface_count = 0;
    std::uint32_t fallback_mixed_group_count = 0;
    std::uint32_t max_group_size = 0;
    std::uint32_t max_active_surfaces = 0;
    std::uint32_t max_sampled_backdrop_surfaces = 0;
    std::uint32_t max_fallback_surfaces = 0;
    std::uint32_t total_shape_pair_count = 0;
    std::uint32_t blend_candidate_pair_count = 0;
    std::uint32_t union_candidate_pair_count = 0;
    std::uint32_t morph_candidate_pair_count = 0;
    std::uint32_t separated_pair_count = 0;
    float min_shape_gap = 0.0f;
    float max_shape_gap = 0.0f;
    float max_blend_distance = 0.0f;
    float max_group_bounds_width = 0.0f;
    float max_group_bounds_height = 0.0f;
    float max_group_bounds_area = 0.0f;
    float max_shape_blend_strength = 0.0f;
};

struct MaterialContainerExecutionDescriptor {
    std::uint32_t command_index = 0;
    std::uint32_t container_id = 0;
    std::uint32_t surface_count = 0;
    bool active = false;
    bool group_bounds_valid = false;
    bool shared_backdrop_scope = false;
    bool shape_blend_execution = false;
    bool union_execution = false;
    bool morph_execution = false;
    char const* execution_policy = "isolated";
    float group_x = 0.0f;
    float group_y = 0.0f;
    float group_w = 0.0f;
    float group_h = 0.0f;
    float shape_blend_strength = 0.0f;
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
    std::uint32_t total_paint_layers = 0;
    std::uint32_t active_paint_layers = 0;
    std::uint32_t dropped_paint_layers = 0;
    std::uint32_t shadow_paint_layers = 0;
    std::uint32_t fill_paint_layers = 0;
    std::uint32_t edge_paint_layers = 0;
    unsigned int max_paint_layer_count = 0;
    unsigned int max_paint_layers = 0;
    unsigned int max_paint_layer_capacity = 0;
    float max_paint_layer_inflate = 0.0f;
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
    std::uint32_t capsule_shape_count = 0;
    std::uint32_t radius_clamped_count = 0;
    std::uint32_t foreground_backdrop_driven_count = 0;
    std::uint32_t foreground_high_contrast_count = 0;
    std::uint32_t foreground_vibrant_count = 0;
    std::uint32_t interaction_enabled_count = 0;
    std::uint32_t interaction_active_count = 0;
    std::uint32_t interaction_reduce_motion_count = 0;
    std::uint32_t interaction_specular_highlight_count = 0;
    std::uint32_t refraction_active_count = 0;
    std::uint32_t theme_default_glass_token_count = 0;
    std::uint32_t theme_custom_token_count = 0;
    MaterialContainerGroupRuntimeSummary container_groups{};
    float max_surface_area = 0.0f;
    float max_effective_radius = 0.0f;
    float max_radius_limit = 0.0f;
    float max_normalized_radius = 0.0f;
    float max_saturation = 0.0f;
    float max_edge_highlight = 0.0f;
    float max_edge_width = 0.0f;
    float max_noise_opacity = 0.0f;
    float max_shadow_alpha = 0.0f;
    float max_shadow_radius = 0.0f;
    float max_interaction_response_strength = 0.0f;
    float max_interaction_specular_radius = 0.0f;
    float max_interaction_specular_intensity = 0.0f;
    float max_refraction_strength = 0.0f;
    float max_refraction_edge_bias = 0.0f;
    float max_refraction_offset_pixels = 0.0f;
    float min_foreground_contrast_ratio = 0.0f;
    std::uint32_t unbounded_texture_copy = 0;
    std::uint32_t non_deterministic_fallback = 0;
    std::uint32_t execution_contract_satisfied_count = 0;
    std::uint32_t execution_contract_mismatch_count = 0;
    std::uint32_t execution_contract_mismatch_total = 0;
    std::uint32_t stage_order_match_count = 0;
    std::uint32_t stage_order_mismatch_count = 0;
    char const* first_execution_contract_mismatch = "none";
    char const* first_stage_order_mismatch = "none";
};

struct MaterialExecutorSummary {
    std::uint32_t plan_count = 0;
    std::uint32_t material_instance_count = 0;
    std::uint32_t fallback_instance_count = 0;
    std::uint32_t sampled_backdrop_instance_count = 0;
    std::uint32_t standard_fill_instance_count = 0;
    std::uint32_t deterministic_fallback_instance_count = 0;
    std::uint32_t material_draw_calls = 0;
    std::uint32_t backdrop_copy_count = 0;
    std::uint32_t backdrop_copy_skipped_count = 0;
    std::uint32_t execution_stage_count = 0;
    std::uint32_t active_execution_stage_count = 0;
    std::uint32_t backdrop_execution_stage_count = 0;
    std::uint32_t primary_execution_stage_count = 0;
    std::uint32_t dropped_execution_stage_count = 0;
    std::uint32_t paint_layer_count = 0;
    std::uint32_t active_paint_layer_count = 0;
    std::uint32_t dropped_paint_layer_count = 0;
    std::uint32_t shadow_paint_layer_count = 0;
    std::uint32_t fill_paint_layer_count = 0;
    std::uint32_t edge_paint_layer_count = 0;
    float max_paint_layer_inflate = 0.0f;
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
    std::uint32_t execution_contract_satisfied_count = 0;
    std::uint32_t execution_contract_mismatch_count = 0;
    std::uint32_t execution_contract_mismatch_total = 0;
    std::uint32_t stage_order_match_count = 0;
    std::uint32_t stage_order_mismatch_count = 0;
    char const* first_execution_contract_mismatch = "none";
    char const* first_stage_order_mismatch = "none";
    std::uint32_t foreground_text_candidate_count = 0;
    std::uint32_t foreground_text_remap_count = 0;
    std::uint32_t interaction_enabled_count = 0;
    std::uint32_t interaction_active_count = 0;
    std::uint32_t interaction_specular_highlight_count = 0;
    std::uint32_t refraction_active_count = 0;
    float max_interaction_response_strength = 0.0f;
    float max_interaction_specular_radius = 0.0f;
    float max_interaction_specular_intensity = 0.0f;
    float max_refraction_strength = 0.0f;
    float max_refraction_edge_bias = 0.0f;
    float max_refraction_offset_pixels = 0.0f;
    bool backdrop_descriptor_color_available = false;
    Color backdrop_descriptor_color_mean = {255, 255, 255, 255};
    std::uint32_t backdrop_descriptor_color_sample_count = 0;
    char const* backdrop_descriptor_color_status = "not-sampled";
    bool backdrop_descriptor_luma_available = false;
    float backdrop_descriptor_luma_min = 0.0f;
    float backdrop_descriptor_luma_max = 1.0f;
    float backdrop_descriptor_luma_mean = 0.5f;
    std::uint32_t backdrop_descriptor_luma_sample_count = 0;
    std::uint32_t backdrop_descriptor_luma_grid_width = 0;
    std::uint32_t backdrop_descriptor_luma_grid_height = 0;
    std::uint64_t backdrop_descriptor_luma_frame = 0;
    char const* backdrop_descriptor_luma_status = "not-sampled";
    char const* backdrop_descriptor_source = "none";
    bool material_pipeline_ready = false;
    bool material_backdrop_source_ready = false;
    bool material_sampled_backdrop_upload_required = false;
    bool material_sampled_backdrop_draw_required = false;
    bool material_sampled_backdrop_uploaded = false;
    bool material_sampled_backdrop_drawn = false;
    char const* material_upload_status = "not-needed";
    char const* material_draw_status = "not-needed";
    std::uint32_t backdrop_luma_sampling_skipped_count = 0;
    char const* backdrop_luma_sampling_skip_reason = "none";
    char const* backdrop_copy_skip_reason = "none";
    MaterialContainerGroupRuntimeSummary container_groups{};
    float material_shader_content_scale = 1.0f;
    float material_max_shader_blur_step_pixels = 0.0f;
    std::int64_t cpu_decode_ns = 0;
    std::int64_t cpu_material_upload_ns = 0;
    std::int64_t cpu_total_ns = 0;
};

inline char const* material_backdrop_copy_policy() noexcept {
    return "copy_only_when_material_plan_requires_shared_or_next_frame_capture";
}

inline bool material_executor_requires_frame_capture(
        MaterialExecutorSummary const& summary) noexcept {
    return summary.planned_frame_capture_count > 0
        && summary.planned_frame_capture_pixels > 0;
}

inline char const* material_executor_sampled_upload_status(
        MaterialExecutorSummary const& summary) noexcept {
    if (summary.sampled_backdrop_instance_count == 0u)
        return "not-needed";
    if (summary.material_upload_bytes > 0)
        return "uploaded";
    if (!summary.material_pipeline_ready)
        return "skipped-material-pipeline-unavailable";
    return "skipped-material-buffer-upload-not-recorded";
}

inline char const* material_executor_sampled_draw_status(
        MaterialExecutorSummary const& summary) noexcept {
    if (summary.sampled_backdrop_instance_count == 0u)
        return "not-needed";
    if (summary.material_draw_calls > 0u)
        return "drawn";
    if (!summary.material_pipeline_ready)
        return "skipped-material-pipeline-unavailable";
    if (!summary.material_backdrop_source_ready)
        return "skipped-material-backdrop-source-unavailable";
    if (!summary.material_sampled_backdrop_uploaded)
        return "skipped-material-buffer-upload-not-recorded";
    return "skipped-material-draw-not-recorded";
}

inline void finalize_material_executor_execution_status(
        MaterialExecutorSummary& summary) noexcept {
    summary.material_sampled_backdrop_upload_required =
        summary.sampled_backdrop_instance_count > 0u;
    summary.material_sampled_backdrop_draw_required =
        summary.sampled_backdrop_instance_count > 0u;
    summary.material_sampled_backdrop_uploaded =
        summary.material_upload_bytes > 0;
    summary.material_sampled_backdrop_drawn =
        summary.material_draw_calls > 0u;
    summary.material_upload_status =
        material_executor_sampled_upload_status(summary);
    summary.material_draw_status =
        material_executor_sampled_draw_status(summary);
}

inline float material_safe_luma(float value, float fallback) noexcept;

inline void set_material_executor_backdrop_descriptor_summary(
        MaterialExecutorSummary& summary,
        MaterialBackdropDescriptor const& backdrop) noexcept {
    summary.backdrop_descriptor_color_available =
        backdrop.available && backdrop.color_sample_count > 0;
    summary.backdrop_descriptor_color_mean = backdrop.color_mean;
    summary.backdrop_descriptor_color_sample_count =
        backdrop.color_sample_count;
    summary.backdrop_descriptor_color_status =
        backdrop.color_sample_status && backdrop.color_sample_status[0]
            ? backdrop.color_sample_status
            : "not-sampled";
    summary.backdrop_descriptor_luma_available =
        backdrop.available && backdrop.luma_sample_count > 0;
    summary.backdrop_descriptor_luma_min =
        material_safe_luma(backdrop.luma_min, 0.0f);
    summary.backdrop_descriptor_luma_max =
        std::max(summary.backdrop_descriptor_luma_min,
                 material_safe_luma(backdrop.luma_max, 1.0f));
    summary.backdrop_descriptor_luma_mean = std::clamp(
        material_safe_luma(backdrop.luma_mean, 0.5f),
        summary.backdrop_descriptor_luma_min,
        summary.backdrop_descriptor_luma_max);
    summary.backdrop_descriptor_luma_sample_count =
        backdrop.luma_sample_count;
    summary.backdrop_descriptor_luma_grid_width =
        backdrop.luma_sample_grid_width;
    summary.backdrop_descriptor_luma_grid_height =
        backdrop.luma_sample_grid_height;
    summary.backdrop_descriptor_luma_frame = backdrop.luma_sample_frame;
    summary.backdrop_descriptor_luma_status =
        backdrop.luma_sample_status && backdrop.luma_sample_status[0]
            ? backdrop.luma_sample_status
            : "not-sampled";
    summary.backdrop_descriptor_source =
        backdrop.source && backdrop.source[0] ? backdrop.source : "none";
}

inline bool material_stage_matches(
        char const* actual,
        std::string_view expected) noexcept {
    return actual && std::string_view{actual} == expected;
}

inline char const* material_execution_stage_role(
        MaterialPlan const& plan,
        MaterialExecutionStage const& stage) noexcept {
    if (material_stage_matches(stage.name, "shape-shadow"))
        return "shadow";
    if (material_stage_matches(stage.name, "edge-highlight"))
        return "edge";
    if (material_stage_matches(stage.name, "noise-dither"))
        return "noise";
    if (plan.primary_pass.active
        && material_stage_matches(stage.name, plan.primary_pass.name)) {
        return "primary";
    }
    return "unknown";
}

inline bool material_execution_stage_role_at(
        MaterialPlan const& plan,
        unsigned int index,
        std::string_view expected) noexcept {
    if (index >= plan.execution_stage_count)
        return false;
    return std::string_view{
        material_execution_stage_role(plan, plan.execution_stages[index])
    } == expected;
}

inline char const* material_execution_stage_order_name(
        MaterialPlan const& plan) noexcept {
    switch (plan.execution_stage_count) {
        case 0:
            return "none";
        case 1:
            return material_execution_stage_role_at(plan, 0, "primary")
                ? "primary"
                : "unexpected-stage-order";
        case 2:
            if (material_execution_stage_role_at(plan, 0, "shadow")
                && material_execution_stage_role_at(plan, 1, "primary")) {
                return "shadow-primary";
            }
            if (material_execution_stage_role_at(plan, 0, "primary")
                && material_execution_stage_role_at(plan, 1, "edge")) {
                return "primary-edge";
            }
            if (material_execution_stage_role_at(plan, 0, "primary")
                && material_execution_stage_role_at(plan, 1, "noise")) {
                return "primary-noise";
            }
            return "unexpected-stage-order";
        case 3:
            if (material_execution_stage_role_at(plan, 0, "shadow")
                && material_execution_stage_role_at(plan, 1, "primary")
                && material_execution_stage_role_at(plan, 2, "edge")) {
                return "shadow-primary-edge";
            }
            if (material_execution_stage_role_at(plan, 0, "shadow")
                && material_execution_stage_role_at(plan, 1, "primary")
                && material_execution_stage_role_at(plan, 2, "noise")) {
                return "shadow-primary-noise";
            }
            if (material_execution_stage_role_at(plan, 0, "primary")
                && material_execution_stage_role_at(plan, 1, "edge")
                && material_execution_stage_role_at(plan, 2, "noise")) {
                return "primary-edge-noise";
            }
            return "unexpected-stage-order";
        case 4:
            if (material_execution_stage_role_at(plan, 0, "shadow")
                && material_execution_stage_role_at(plan, 1, "primary")
                && material_execution_stage_role_at(plan, 2, "edge")
                && material_execution_stage_role_at(plan, 3, "noise")) {
                return "shadow-primary-edge-noise";
            }
            return "unexpected-stage-order";
    }
    return "unexpected-stage-order";
}

inline bool material_plan_uses_sampled_backdrop_executor(
        MaterialPlan const& plan) noexcept {
    return plan.primary_pass.active
        && plan.primary_pass.requires_backdrop
        && material_stage_matches(plan.primary_pass.executor, "backdrop-filter");
}

inline bool material_plan_uses_standard_fill_executor(
        MaterialPlan const& plan) noexcept {
    return plan.primary_pass.active
        && !plan.primary_pass.requires_backdrop
        && material_stage_matches(plan.primary_pass.executor, "standard-fill");
}

inline bool material_plan_uses_deterministic_fallback_executor(
        MaterialPlan const& plan) noexcept {
    return plan.primary_pass.active
        && !plan.primary_pass.requires_backdrop
        && material_stage_matches(plan.primary_pass.executor, "fallback-fill");
}

inline Color material_paint_layer_color_with_alpha(
        Color color,
        float alpha) noexcept {
    color.a = static_cast<unsigned char>(
        std::clamp(std::lround(std::clamp(alpha, 0.0f, 1.0f) * 255.0f),
                   0l,
                   255l));
    return color;
}

inline float material_shadow_paint_inflate(
        MaterialPlan const& plan) noexcept {
    return std::min(std::max(plan.shadow_radius * 0.5f, 1.0f), 18.0f);
}

inline float material_shadow_paint_y_offset(
        MaterialPlan const& plan) noexcept {
    return std::min(std::max(plan.shadow_radius * 0.18f, 1.0f), 6.0f);
}

inline void resolve_material_paint_layers(MaterialPlan& plan) noexcept {
    if (!plan.primary_pass.active || plan.primary_pass.requires_backdrop)
        return;

    if (plan.shadow_alpha > 0.0f) {
        auto const inflate = material_shadow_paint_inflate(plan);
        append_material_paint_layer(
            plan,
            MaterialPaintLayer{
                "fallback-shadow",
                true,
                "rounded-shadow",
                0.0f,
                material_shadow_paint_y_offset(plan),
                inflate,
                inflate,
                0.0f,
                Color{0, 0, 0, 255},
                plan.shadow_alpha,
                true,
            });
    }

    append_material_paint_layer(
        plan,
        MaterialPaintLayer{
            plan.primary_pass.name,
            true,
            "rounded-fill",
            0.0f,
            0.0f,
            0.0f,
            0.0f,
            0.0f,
            plan.tint,
            plan.opacity,
            true,
        });

    if (plan.edge_highlight > 0.0f) {
        append_material_paint_layer(
            plan,
            MaterialPaintLayer{
                "fallback-edge-highlight",
                true,
                "rounded-edge",
                0.0f,
                0.0f,
                0.0f,
                0.0f,
                plan.edge_width,
                material_paint_layer_color_with_alpha(
                    Color{255, 255, 255, 255},
                    plan.edge_highlight),
                1.0f,
                true,
            });
    }
}

struct MaterialContainerGroupAccumulator {
    std::uint32_t container_id = 0;
    std::uint32_t surface_count = 0;
    std::uint32_t active_surfaces = 0;
    std::uint32_t sampled_backdrop_surfaces = 0;
    std::uint32_t fallback_surfaces = 0;
    std::uint32_t union_surfaces = 0;
    std::uint32_t morph_surfaces = 0;
    std::uint32_t interactive_surfaces = 0;
    std::uint32_t shared_backdrop_scope_surfaces = 0;
    std::uint32_t shape_pair_count = 0;
    std::uint32_t blend_candidate_pair_count = 0;
    std::uint32_t union_candidate_pair_count = 0;
    std::uint32_t morph_candidate_pair_count = 0;
    std::uint32_t separated_pair_count = 0;
    bool has_bounds = false;
    bool has_shape_gap = false;
    float min_x = 0.0f;
    float min_y = 0.0f;
    float max_x = 0.0f;
    float max_y = 0.0f;
    float min_shape_gap = 0.0f;
    float max_shape_gap = 0.0f;
    float max_blend_distance = 0.0f;
};

inline bool material_plan_in_container(
        MaterialPlan const& plan,
        std::uint32_t container_id) noexcept {
    return plan.container.participates
        && plan.container.container_id == container_id;
}

inline float material_rect_gap(MaterialGeometry const& a,
                               MaterialGeometry const& b) noexcept {
    auto const ax2 = a.x + std::max(0.0f, a.w);
    auto const ay2 = a.y + std::max(0.0f, a.h);
    auto const bx2 = b.x + std::max(0.0f, b.w);
    auto const by2 = b.y + std::max(0.0f, b.h);
    auto const dx = std::max(std::max(b.x - ax2, a.x - bx2), 0.0f);
    auto const dy = std::max(std::max(b.y - ay2, a.y - by2), 0.0f);
    return std::sqrt(dx * dx + dy * dy);
}

inline void accumulate_material_container_bounds(
        MaterialContainerGroupAccumulator& group,
        MaterialPlan const& plan) noexcept {
    if (!plan.shape.valid)
        return;
    group.max_blend_distance =
        std::max(group.max_blend_distance, plan.container.blend_distance);
    auto const x0 = plan.geometry.x;
    auto const y0 = plan.geometry.y;
    auto const x1 = plan.geometry.x + std::max(0.0f, plan.geometry.w);
    auto const y1 = plan.geometry.y + std::max(0.0f, plan.geometry.h);
    if (!group.has_bounds) {
        group.has_bounds = true;
        group.min_x = x0;
        group.min_y = y0;
        group.max_x = x1;
        group.max_y = y1;
        return;
    }
    group.min_x = std::min(group.min_x, x0);
    group.min_y = std::min(group.min_y, y0);
    group.max_x = std::max(group.max_x, x1);
    group.max_y = std::max(group.max_y, y1);
}

inline void accumulate_material_container_pair(
        MaterialContainerGroupAccumulator& group,
        MaterialPlan const& a,
        MaterialPlan const& b) noexcept {
    if (!a.shape.valid || !b.shape.valid)
        return;
    ++group.shape_pair_count;
    auto const gap = material_rect_gap(a.geometry, b.geometry);
    if (!group.has_shape_gap) {
        group.has_shape_gap = true;
        group.min_shape_gap = gap;
        group.max_shape_gap = gap;
    } else {
        group.min_shape_gap = std::min(group.min_shape_gap, gap);
        group.max_shape_gap = std::max(group.max_shape_gap, gap);
    }
    auto const blend_distance =
        std::min(a.container.blend_distance, b.container.blend_distance);
    group.max_blend_distance =
        std::max(group.max_blend_distance, blend_distance);
    auto const blend_candidate = gap <= blend_distance;
    if (blend_candidate)
        ++group.blend_candidate_pair_count;
    else
        ++group.separated_pair_count;
    if (blend_candidate
        && (a.container.shape_union_expected
            || b.container.shape_union_expected)) {
        ++group.union_candidate_pair_count;
    }
    if (blend_candidate
        && (a.container.morph_transitions
            || b.container.morph_transitions)) {
        ++group.morph_candidate_pair_count;
    }
}

inline bool material_container_group_shape_blend_execution_active(
        MaterialContainerGroupAccumulator const& group) noexcept {
    return group.active_surfaces > 1u
        && group.blend_candidate_pair_count > 0u
        && (group.union_surfaces > 0u
            || group.morph_surfaces > 0u
            || group.shared_backdrop_scope_surfaces > 1u
            || group.interactive_surfaces > 0u);
}

inline float material_container_group_shape_blend_strength(
        MaterialContainerGroupAccumulator const& group) noexcept {
    if (!material_container_group_shape_blend_execution_active(group)
        || !group.has_shape_gap) {
        return 0.0f;
    }
    if (group.max_blend_distance <= 0.0f)
        return group.min_shape_gap <= 0.5f ? 1.0f : 0.0f;
    auto const proximity = std::clamp(
        (group.max_blend_distance - group.min_shape_gap)
            / group.max_blend_distance,
        0.0f,
        1.0f);
    return std::clamp(std::max(0.25f, proximity), 0.0f, 1.0f);
}

inline char const* material_container_execution_policy_name(
        MaterialContainerGroupAccumulator const& group) noexcept {
    if (material_container_group_shape_blend_execution_active(group))
        return "group-edge-continuity";
    if (group.surface_count > 0u)
        return "group-isolated";
    return "isolated";
}

inline MaterialContainerGroupAccumulator accumulate_material_container_group(
        std::span<MaterialRuntimeRecord const> records,
        std::uint32_t container_id) noexcept {
    MaterialContainerGroupAccumulator group{
        .container_id = container_id,
    };
    if (container_id == 0u)
        return group;
    for (auto const& candidate_record : records) {
        auto const& candidate = candidate_record.plan;
        if (!candidate.container.participates
            || candidate.container.container_id != container_id) {
            continue;
        }
        ++group.surface_count;
        accumulate_material_container_bounds(group, candidate);
        if (candidate.primary_pass.active)
            ++group.active_surfaces;
        if (candidate.backdrop_sampling)
            ++group.sampled_backdrop_surfaces;
        if (candidate.fallback())
            ++group.fallback_surfaces;
        if (candidate.container.shape_union_expected)
            ++group.union_surfaces;
        if (candidate.container.morph_transitions)
            ++group.morph_surfaces;
        if (candidate.container.interactive)
            ++group.interactive_surfaces;
        if (candidate.container.shared_backdrop_scope)
            ++group.shared_backdrop_scope_surfaces;
    }
    for (std::size_t left = 0; left < records.size(); ++left) {
        auto const& a = records[left].plan;
        if (!material_plan_in_container(a, container_id))
            continue;
        for (std::size_t right = left + 1; right < records.size(); ++right) {
            auto const& b = records[right].plan;
            if (!material_plan_in_container(b, container_id))
                continue;
            accumulate_material_container_pair(group, a, b);
        }
    }
    return group;
}

inline MaterialContainerExecutionDescriptor
material_container_execution_descriptor_from_group(
        MaterialRuntimeRecord const& record,
        MaterialContainerGroupAccumulator const& group) noexcept {
    MaterialContainerExecutionDescriptor descriptor{};
    descriptor.command_index = record.command_index;
    auto const& plan = record.plan;
    if (!plan.container.participates
        || plan.container.container_id == 0u
        || plan.container.container_id != group.container_id) {
        return descriptor;
    }

    descriptor.container_id = group.container_id;
    descriptor.surface_count = group.surface_count;
    descriptor.active = plan.primary_pass.active && group.surface_count > 0u;
    descriptor.group_bounds_valid = group.has_bounds;
    descriptor.shared_backdrop_scope = plan.container.shared_backdrop_scope
        && group.shared_backdrop_scope_surfaces > 1u;
    descriptor.shape_blend_execution =
        descriptor.active
        && material_container_group_shape_blend_execution_active(group);
    descriptor.union_execution =
        descriptor.shape_blend_execution && group.union_surfaces > 0u;
    descriptor.morph_execution =
        descriptor.shape_blend_execution && group.morph_surfaces > 0u;
    descriptor.execution_policy =
        material_container_execution_policy_name(group);
    descriptor.shape_blend_strength =
        material_container_group_shape_blend_strength(group);
    if (group.has_bounds) {
        descriptor.group_x = group.min_x;
        descriptor.group_y = group.min_y;
        descriptor.group_w = std::max(0.0f, group.max_x - group.min_x);
        descriptor.group_h = std::max(0.0f, group.max_y - group.min_y);
    }
    return descriptor;
}

inline MaterialContainerExecutionDescriptor
material_container_execution_descriptor(
        MaterialRuntimeRecord const& record,
        std::span<MaterialRuntimeRecord const> records) {
    MaterialContainerExecutionDescriptor descriptor{};
    descriptor.command_index = record.command_index;
    auto const& plan = record.plan;
    if (!plan.container.participates || plan.container.container_id == 0u)
        return descriptor;

    auto const group = accumulate_material_container_group(
        records,
        plan.container.container_id);
    return material_container_execution_descriptor_from_group(record, group);
}

inline MaterialContainerGroupRuntimeSummary summarize_material_container_groups(
        std::span<MaterialRuntimeRecord const> records) {
    MaterialContainerGroupRuntimeSummary summary{};
    for (std::size_t index = 0; index < records.size(); ++index) {
        auto const& plan = records[index].plan;
        if (!plan.container.participates || plan.container.container_id == 0u)
            continue;
        auto const container_id = plan.container.container_id;
        auto seen = false;
        for (std::size_t prior = 0; prior < index; ++prior) {
            auto const& prior_plan = records[prior].plan;
            if (prior_plan.container.participates
                && prior_plan.container.container_id == container_id) {
                seen = true;
                break;
            }
        }
        if (seen)
            continue;

        MaterialContainerGroupAccumulator group{
            .container_id = container_id,
        };
        for (auto const& candidate_record : records) {
            auto const& candidate = candidate_record.plan;
            if (!candidate.container.participates
                || candidate.container.container_id != container_id) {
                continue;
            }
            ++group.surface_count;
            accumulate_material_container_bounds(group, candidate);
            if (candidate.primary_pass.active)
                ++group.active_surfaces;
            if (candidate.backdrop_sampling)
                ++group.sampled_backdrop_surfaces;
            if (candidate.fallback())
                ++group.fallback_surfaces;
            if (candidate.container.shape_union_expected)
                ++group.union_surfaces;
            if (candidate.container.morph_transitions)
                ++group.morph_surfaces;
            if (candidate.container.interactive)
                ++group.interactive_surfaces;
            if (candidate.container.shared_backdrop_scope)
                ++group.shared_backdrop_scope_surfaces;
        }
        for (std::size_t left = 0; left < records.size(); ++left) {
            auto const& a = records[left].plan;
            if (!material_plan_in_container(a, container_id))
                continue;
            for (std::size_t right = left + 1; right < records.size(); ++right) {
                auto const& b = records[right].plan;
                if (!material_plan_in_container(b, container_id))
                    continue;
                accumulate_material_container_pair(group, a, b);
            }
        }
        ++summary.group_count;
        summary.max_group_size =
            std::max(summary.max_group_size, group.surface_count);
        summary.max_active_surfaces =
            std::max(summary.max_active_surfaces, group.active_surfaces);
        summary.max_sampled_backdrop_surfaces = std::max(
            summary.max_sampled_backdrop_surfaces,
            group.sampled_backdrop_surfaces);
        summary.max_fallback_surfaces =
            std::max(summary.max_fallback_surfaces, group.fallback_surfaces);
        if (group.surface_count > 1u)
            ++summary.multi_surface_group_count;
        if (group.union_surfaces > 0u)
            ++summary.union_group_count;
        if (group.morph_surfaces > 0u)
            ++summary.morph_group_count;
        if (group.interactive_surfaces > 0u)
            ++summary.interactive_group_count;
        if (group.shared_backdrop_scope_surfaces > 0u)
            ++summary.shared_backdrop_scope_group_count;
        if (group.shared_backdrop_scope_surfaces > 0u) {
            summary.shared_capture_surface_count +=
                group.shared_backdrop_scope_surfaces;
            summary.shared_capture_saved_surface_count +=
                group.shared_backdrop_scope_surfaces > 1u
                    ? group.shared_backdrop_scope_surfaces - 1u
                    : 0u;
            summary.max_shared_capture_group_surfaces = std::max(
                summary.max_shared_capture_group_surfaces,
                group.shared_backdrop_scope_surfaces);
        }
        if (material_container_group_shape_blend_execution_active(group)) {
            ++summary.shape_blend_execution_group_count;
            summary.shape_blend_execution_surface_count +=
                group.active_surfaces;
            summary.max_shape_blend_strength = std::max(
                summary.max_shape_blend_strength,
                material_container_group_shape_blend_strength(group));
        }
        if (group.fallback_surfaces > 0u
            && group.active_surfaces > group.fallback_surfaces) {
            ++summary.fallback_mixed_group_count;
        }
        summary.total_shape_pair_count += group.shape_pair_count;
        summary.blend_candidate_pair_count += group.blend_candidate_pair_count;
        summary.union_candidate_pair_count += group.union_candidate_pair_count;
        summary.morph_candidate_pair_count += group.morph_candidate_pair_count;
        summary.separated_pair_count += group.separated_pair_count;
        if (group.has_shape_gap) {
            summary.min_shape_gap =
                summary.total_shape_pair_count == group.shape_pair_count
                    ? group.min_shape_gap
                    : std::min(summary.min_shape_gap, group.min_shape_gap);
            summary.max_shape_gap =
                std::max(summary.max_shape_gap, group.max_shape_gap);
        }
        summary.max_blend_distance =
            std::max(summary.max_blend_distance, group.max_blend_distance);
        if (group.has_bounds) {
            auto const bounds_width =
                std::max(0.0f, group.max_x - group.min_x);
            auto const bounds_height =
                std::max(0.0f, group.max_y - group.min_y);
            summary.max_group_bounds_width = std::max(
                summary.max_group_bounds_width,
                bounds_width);
            summary.max_group_bounds_height = std::max(
                summary.max_group_bounds_height,
                bounds_height);
            summary.max_group_bounds_area = std::max(
                summary.max_group_bounds_area,
                bounds_width * bounds_height);
        }
    }
    return summary;
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
    if (material_plan_uses_sampled_backdrop_executor(plan))
        ++summary.sampled_backdrop_instance_count;
    if (material_plan_uses_standard_fill_executor(plan))
        ++summary.standard_fill_instance_count;
    if (material_plan_uses_deterministic_fallback_executor(plan))
        ++summary.deterministic_fallback_instance_count;
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
    summary.paint_layer_count += plan.paint_layer_count;
    summary.dropped_paint_layer_count += plan.dropped_paint_layer_count;
    for (unsigned int i = 0; i < plan.paint_layer_count; ++i) {
        auto const& layer = plan.paint_layers[i];
        if (layer.active)
            ++summary.active_paint_layer_count;
        if (material_paint_layer_matches(layer.executor, "rounded-shadow"))
            ++summary.shadow_paint_layer_count;
        if (material_paint_layer_matches(layer.executor, "rounded-fill"))
            ++summary.fill_paint_layer_count;
        if (material_paint_layer_matches(layer.executor, "rounded-edge"))
            ++summary.edge_paint_layer_count;
        summary.max_paint_layer_inflate = std::max(
            summary.max_paint_layer_inflate,
            layer.inflate);
    }
    if (plan.backdrop_access.required)
        ++summary.backdrop_access_plan_count;
    if (plan.backdrop_access.next_frame_capture_required)
        ++summary.next_frame_capture_plan_count;
    if (plan.interaction.enabled)
        ++summary.interaction_enabled_count;
    if (plan.interaction.active)
        ++summary.interaction_active_count;
    if (plan.interaction.specular_highlight_active)
        ++summary.interaction_specular_highlight_count;
    if (plan.refraction.active)
        ++summary.refraction_active_count;
    summary.max_interaction_response_strength = std::max(
        summary.max_interaction_response_strength,
        plan.interaction.response_strength);
    summary.max_interaction_specular_radius = std::max(
        summary.max_interaction_specular_radius,
        plan.interaction.specular_radius);
    summary.max_interaction_specular_intensity = std::max(
        summary.max_interaction_specular_intensity,
        plan.interaction.specular_intensity);
    summary.max_refraction_strength = std::max(
        summary.max_refraction_strength,
        plan.refraction.strength);
    summary.max_refraction_edge_bias = std::max(
        summary.max_refraction_edge_bias,
        plan.refraction.edge_bias);
    summary.max_refraction_offset_pixels = std::max(
        summary.max_refraction_offset_pixels,
        plan.refraction.max_offset_pixels);
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
    if (plan.execution_audit.contract_satisfied) {
        ++summary.execution_contract_satisfied_count;
    } else {
        ++summary.execution_contract_mismatch_count;
        if (std::string_view{summary.first_execution_contract_mismatch}
                == "none") {
            summary.first_execution_contract_mismatch =
                plan.execution_audit.first_mismatch;
        }
    }
    summary.execution_contract_mismatch_total +=
        plan.execution_audit.mismatch_count;
    if (plan.execution_audit.stage_order_match) {
        ++summary.stage_order_match_count;
    } else {
        ++summary.stage_order_mismatch_count;
        if (std::string_view{summary.first_stage_order_mismatch} == "none") {
            summary.first_stage_order_mismatch =
                plan.execution_audit.actual_stage_order;
        }
    }
}

inline void finalize_material_executor_summary(
        MaterialExecutorSummary& summary,
        std::span<MaterialRuntimeRecord const> records) {
    summary.container_groups = summarize_material_container_groups(records);
    finalize_material_executor_execution_status(summary);
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
    summary.total_paint_layers += plan.paint_layer_count;
    summary.dropped_paint_layers += plan.dropped_paint_layer_count;
    summary.max_execution_stage_count =
        std::max(summary.max_execution_stage_count, plan.execution_stage_count);
    summary.max_execution_stages =
        std::max(summary.max_execution_stages,
                 plan.resource_budget.max_execution_stages);
    summary.max_paint_layers =
        std::max(summary.max_paint_layers,
                 plan.resource_budget.max_paint_layers);
    summary.max_execution_stage_capacity =
        std::max(summary.max_execution_stage_capacity,
                 plan.execution_stage_capacity);
    summary.max_paint_layer_count =
        std::max(summary.max_paint_layer_count, plan.paint_layer_count);
    summary.max_paint_layer_capacity =
        std::max(summary.max_paint_layer_capacity, plan.paint_layer_capacity);
    summary.max_paint_layer_inflate = std::max(
        summary.max_paint_layer_inflate,
        plan.resource_budget.max_paint_layer_inflate);
    for (unsigned int i = 0; i < plan.execution_stage_count; ++i) {
        auto const& stage = plan.execution_stages[i];
        if (stage.active)
            ++summary.active_execution_stages;
        if (stage.requires_backdrop)
            ++summary.backdrop_execution_stages;
    }
    for (unsigned int i = 0; i < plan.paint_layer_count; ++i) {
        auto const& layer = plan.paint_layers[i];
        if (layer.active)
            ++summary.active_paint_layers;
        if (material_paint_layer_matches(layer.executor, "rounded-shadow"))
            ++summary.shadow_paint_layers;
        if (material_paint_layer_matches(layer.executor, "rounded-fill"))
            ++summary.fill_paint_layers;
        if (material_paint_layer_matches(layer.executor, "rounded-edge"))
            ++summary.edge_paint_layers;
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
    summary.max_refraction_offset_pixels = std::max(
        summary.max_refraction_offset_pixels,
        plan.resource_budget.max_refraction_offset_pixels);
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
    if (plan.shape.capsule)
        ++summary.capsule_shape_count;
    if (plan.shape.radius_clamped)
        ++summary.radius_clamped_count;
    if (plan.foreground.backdrop_driven)
        ++summary.foreground_backdrop_driven_count;
    if (plan.foreground.high_contrast)
        ++summary.foreground_high_contrast_count;
    if (plan.foreground.uses_vibrancy)
        ++summary.foreground_vibrant_count;
    if (plan.interaction.enabled)
        ++summary.interaction_enabled_count;
    if (plan.interaction.active)
        ++summary.interaction_active_count;
    if (plan.interaction.reduce_motion)
        ++summary.interaction_reduce_motion_count;
    if (plan.interaction.specular_highlight_active)
        ++summary.interaction_specular_highlight_count;
    if (plan.refraction.active)
        ++summary.refraction_active_count;
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
    summary.max_saturation =
        std::max(summary.max_saturation, plan.saturation);
    summary.max_edge_highlight =
        std::max(summary.max_edge_highlight, plan.edge_highlight);
    summary.max_edge_width =
        std::max(summary.max_edge_width, plan.edge_width);
    summary.max_noise_opacity =
        std::max(summary.max_noise_opacity, plan.noise_opacity);
    summary.max_shadow_alpha =
        std::max(summary.max_shadow_alpha, plan.shadow_alpha);
    summary.max_shadow_radius =
        std::max(summary.max_shadow_radius, plan.shadow_radius);
    summary.max_interaction_response_strength = std::max(
        summary.max_interaction_response_strength,
        plan.interaction.response_strength);
    summary.max_interaction_specular_radius = std::max(
        summary.max_interaction_specular_radius,
        plan.interaction.specular_radius);
    summary.max_interaction_specular_intensity = std::max(
        summary.max_interaction_specular_intensity,
        plan.interaction.specular_intensity);
    summary.max_refraction_strength = std::max(
        summary.max_refraction_strength,
        plan.refraction.strength);
    summary.max_refraction_edge_bias = std::max(
        summary.max_refraction_edge_bias,
        plan.refraction.edge_bias);
    summary.max_refraction_offset_pixels = std::max(
        summary.max_refraction_offset_pixels,
        plan.refraction.max_offset_pixels);
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
    if (plan.execution_audit.contract_satisfied) {
        ++summary.execution_contract_satisfied_count;
    } else {
        ++summary.execution_contract_mismatch_count;
        if (std::string_view{summary.first_execution_contract_mismatch}
                == "none") {
            summary.first_execution_contract_mismatch =
                plan.execution_audit.first_mismatch;
        }
    }
    summary.execution_contract_mismatch_total +=
        plan.execution_audit.mismatch_count;
    if (plan.execution_audit.stage_order_match) {
        ++summary.stage_order_match_count;
    } else {
        ++summary.stage_order_mismatch_count;
        if (std::string_view{summary.first_stage_order_mismatch} == "none") {
            summary.first_stage_order_mismatch =
                plan.execution_audit.actual_stage_order;
        }
    }
}

inline void finalize_material_runtime_summary(
        MaterialRuntimeSummary& summary,
        std::span<MaterialRuntimeRecord const> records) {
    summary.container_groups = summarize_material_container_groups(records);
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
        style.shadow_radius,
        style.interaction};
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
    style.interaction = descriptor.interaction;
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
    auto const bounded = std::min(max_sample_taps, material_max_sample_taps);
    if (bounded == 0u)
        return 0u;
    if (bounded < 5u)
        return 1u;
    if (bounded < 9u)
        return 5u;
    if (bounded < 13u)
        return 9u;
    if (bounded < material_max_sample_taps)
        return 13u;
    return material_max_sample_taps;
}

inline MaterialSamplingKernel material_resolve_sampling_kernel(
        bool backdrop_sampling,
        unsigned int sample_taps) noexcept {
    if (!backdrop_sampling || sample_taps == 0u)
        return {};
    if (sample_taps <= 1u)
        return MaterialSamplingKernel{
            "weighted-center",
            0u,
            sample_taps,
            0.0f,
            "center4",
            true,
            true,
        };
    if (sample_taps <= 5u)
        return MaterialSamplingKernel{
            "weighted-cross-5",
            1u,
            sample_taps,
            0.35f,
            "gaussian-cross-5-separable",
            true,
            true,
        };
    if (sample_taps <= 9u)
        return MaterialSamplingKernel{
            "weighted-3x3-grid",
            1u,
            sample_taps,
            0.35f,
            "gaussian-3x3-separable",
            true,
            true,
        };
    return MaterialSamplingKernel{
        "gaussian-5x5",
        2u,
        sample_taps,
        0.35f,
        "gaussian-5x5-separable",
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
    auto const absolute_dark = Color{0, 0, 0, 255};
    Color const candidates[3] = {dark, light, absolute_dark};
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
    foreground.minimum_contrast_ratio =
        capabilities.increase_contrast ? 7.0f : 4.5f;
    foreground.scheme = material_foreground_scheme_name(
        plan,
        foreground.background_luma,
        capabilities);
    foreground.source = material_foreground_source_name(plan, capabilities);
    foreground.contrast_policy = capabilities.increase_contrast
        ? "enhanced-contrast"
        : "standard-contrast";
    foreground.remap_policy = plan.kind == MaterialKind::None
        ? "none"
        : capabilities.increase_contrast
            ? "strict-theme-role-remap"
            : "theme-role-remap-if-needed";
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
    foreground.primary_contrast_margin =
        foreground.primary_contrast_ratio - foreground.minimum_contrast_ratio;
    foreground.secondary_contrast_margin =
        foreground.secondary_contrast_ratio - foreground.minimum_contrast_ratio;
    foreground.accent_contrast_margin =
        foreground.accent_contrast_ratio - foreground.minimum_contrast_ratio;
    return foreground;
}

inline bool material_color_rgb_equal(Color a, Color b) noexcept {
    return a.r == b.r && a.g == b.g && a.b == b.b;
}

inline Color material_color_from_contract_token(
        theme_contract::ColorToken color) noexcept {
    return Color{color.r, color.g, color.b, color.a};
}

inline MaterialThemeSnapshot material_resolve_theme_snapshot(
        MaterialStyle const& style) noexcept {
    MaterialThemeSnapshot snapshot{};
    auto const contract = theme_contract::default_glass_theme_contract();
    auto const default_foreground =
        material_color_from_contract_token(contract.foreground);
    auto const default_muted =
        material_color_from_contract_token(contract.muted);
    auto const default_accent =
        material_color_from_contract_token(contract.accent);
    auto const default_accent_strong =
        material_color_from_contract_token(contract.accent_strong);
    auto const default_surface =
        material_color_from_contract_token(contract.surface);
    auto const default_border =
        material_color_from_contract_token(contract.border);
    snapshot.foreground = style.foreground;
    snapshot.secondary_foreground = style.secondary_foreground;
    snapshot.accent_foreground = style.accent_foreground;
    snapshot.strong_accent_foreground = style.strong_accent_foreground;
    snapshot.tint = style.tint;
    snapshot.border = style.border;
    snapshot.foreground_matches_theme =
        style.foreground == default_foreground
        && style.secondary_foreground == default_muted;
    snapshot.accent_matches_theme =
        style.accent_foreground == default_accent
        && style.strong_accent_foreground == default_accent_strong;
    snapshot.tint_matches_surface =
        style.kind == MaterialKind::None
        || material_color_rgb_equal(style.tint, default_surface);
    snapshot.border_matches_theme =
        style.kind == MaterialKind::None
        || material_color_rgb_equal(style.border, default_border);
    snapshot.default_glass_tokens =
        snapshot.foreground_matches_theme
        && snapshot.accent_matches_theme
        && snapshot.tint_matches_surface
        && snapshot.border_matches_theme
        && contract.profile_name == "apple-glass-light"
        && contract.typography.default_font_family == "Pretendard";
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
    analysis.color_mean = backdrop.color_mean;
    analysis.color_sample_count = backdrop.color_sample_count;
    analysis.color_sample_status =
        backdrop.color_sample_status && backdrop.color_sample_status[0]
            ? backdrop.color_sample_status
            : "not-sampled";
    analysis.luma_min = material_safe_luma(backdrop.luma_min, 0.0f);
    analysis.luma_max = std::max(
        analysis.luma_min,
        material_safe_luma(backdrop.luma_max, 1.0f));
    analysis.luma_mean = std::clamp(
        material_safe_luma(backdrop.luma_mean, 0.5f),
        analysis.luma_min,
        analysis.luma_max);
    analysis.luma_span = analysis.luma_max - analysis.luma_min;
    analysis.luma_sample_count = backdrop.luma_sample_count;
    analysis.luma_sample_grid_width = backdrop.luma_sample_grid_width;
    analysis.luma_sample_grid_height = backdrop.luma_sample_grid_height;
    analysis.luma_sample_frame = backdrop.luma_sample_frame;
    analysis.luma_sample_status =
        backdrop.luma_sample_status && backdrop.luma_sample_status[0]
            ? backdrop.luma_sample_status
            : "not-sampled";
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
    analysis.capsule = analysis.rounded
        && analysis.normalized_radius >= 0.999f;
    analysis.kind = analysis.capsule
        ? MaterialShapeKind::Capsule
        : analysis.rounded
            ? MaterialShapeKind::RoundedRectangle
            : MaterialShapeKind::Rectangle;
    return analysis;
}

inline float material_sanitize_container_spacing(float spacing) noexcept {
    return std::isfinite(spacing) ? std::max(0.0f, spacing) : 0.0f;
}

inline char const* material_container_blend_policy_name(
        MaterialContainerMode mode,
        float spacing) noexcept {
    if (mode == MaterialContainerMode::Isolated)
        return "isolated";
    if (spacing <= 0.0f)
        return "touching-only";
    if (mode == MaterialContainerMode::Union)
        return "union-shape-proximity";
    return "container-shape-proximity";
}

inline char const* material_container_morph_policy_name(
        MaterialContainerMode mode,
        bool morph_transitions,
        bool reduced_motion_suppressed_morph) noexcept {
    if (mode == MaterialContainerMode::Isolated)
        return "isolated";
    if (reduced_motion_suppressed_morph)
        return "reduced-motion-static";
    if (!morph_transitions)
        return "static-container";
    if (mode == MaterialContainerMode::Union)
        return "union-morph";
    return "container-morph";
}

inline char const* material_container_performance_policy_name(
        MaterialContainerMode mode) noexcept {
    if (mode == MaterialContainerMode::Union)
        return "shared-union-capture";
    if (mode == MaterialContainerMode::Container)
        return "shared-container-capture";
    return "single-surface";
}

inline MaterialContainerAnalysis analyze_material_container(
        MaterialContainerDescriptor descriptor,
        bool reduce_motion) noexcept {
    MaterialContainerAnalysis analysis{};
    analysis.container_id = descriptor.container_id;
    analysis.union_id = descriptor.union_id;
    analysis.spacing = material_sanitize_container_spacing(descriptor.spacing);
    analysis.blend_distance = analysis.spacing;
    analysis.interactive = descriptor.interactive;
    analysis.participates = descriptor.participates();
    analysis.mode = descriptor.mode();
    analysis.mode_name = material_container_mode_name(analysis.mode);
    analysis.shared_backdrop_scope = analysis.participates;
    analysis.shape_union_expected =
        analysis.mode == MaterialContainerMode::Union;
    analysis.requested_morph_transitions =
        descriptor.morph_transitions && analysis.participates;
    analysis.reduced_motion_suppressed_morph =
        analysis.requested_morph_transitions && reduce_motion;
    analysis.morph_transitions =
        analysis.requested_morph_transitions
        && !analysis.reduced_motion_suppressed_morph;
    analysis.shape_blending_expected =
        analysis.participates && analysis.blend_distance > 0.0f;
    analysis.spacing_clamped =
        !std::isfinite(descriptor.spacing)
        || std::fabs(analysis.spacing - descriptor.spacing) > 0.0001f;
    analysis.blend_policy = material_container_blend_policy_name(
        analysis.mode,
        analysis.blend_distance);
    analysis.morph_policy = material_container_morph_policy_name(
        analysis.mode,
        analysis.morph_transitions,
        analysis.reduced_motion_suppressed_morph);
    analysis.performance_policy =
        material_container_performance_policy_name(analysis.mode);
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

inline char const* material_frosting_response_name(
        bool dark,
        bool bright,
        bool flat) noexcept {
    if (dark)
        return "dark-frost-lift";
    if (bright)
        return "bright-frost-thin";
    if (flat)
        return "flat-edge-frost";
    return "balanced";
}

inline char const* material_tint_response_name(
        bool dark,
        bool bright,
        bool flat) noexcept {
    if (dark)
        return "lift-dark-backdrop";
    if (bright)
        return "thin-bright-backdrop";
    if (flat)
        return "stabilize-flat-backdrop";
    return "preserve";
}

inline float material_color_channel_fraction(unsigned char value) noexcept {
    return static_cast<float>(value) / 255.0f;
}

inline float material_rgb_distance(Color a, Color b) noexcept {
    auto const red = std::fabs(
        material_color_channel_fraction(a.r)
        - material_color_channel_fraction(b.r));
    auto const green = std::fabs(
        material_color_channel_fraction(a.g)
        - material_color_channel_fraction(b.g));
    auto const blue = std::fabs(
        material_color_channel_fraction(a.b)
        - material_color_channel_fraction(b.b));
    return std::max(red, std::max(green, blue));
}

inline float material_colorfulness(Color color) noexcept {
    auto const red = material_color_channel_fraction(color.r);
    auto const green = material_color_channel_fraction(color.g);
    auto const blue = material_color_channel_fraction(color.b);
    auto const high = std::max(red, std::max(green, blue));
    auto const low = std::min(red, std::min(green, blue));
    return high - low;
}

inline char const* material_color_response_name(float colorfulness,
                                                float distance) noexcept {
    if (distance <= 0.025f)
        return "preserve";
    if (colorfulness <= 0.025f)
        return "neutral-backdrop-color";
    return "sampled-backdrop-color";
}

inline char const* material_saturation_response_name(
        bool dark,
        bool bright,
        bool flat) noexcept {
    if (dark)
        return "lift-dark-color";
    if (bright)
        return "compress-bright-color";
    if (flat)
        return "restore-flat-color";
    return "preserve";
}

inline char const* material_depth_response_name(
        bool dark,
        bool bright,
        bool flat) noexcept {
    if (dark)
        return "soften-dark-depth";
    if (bright)
        return "restore-bright-depth";
    if (flat)
        return "restore-flat-depth";
    return "standard";
}

inline float material_apply_scalar_delta(float& value,
                                         float delta,
                                         float min_value,
                                         float max_value) noexcept {
    if (std::fabs(delta) <= 0.000001f)
        return 0.0f;
    auto const before = value;
    auto const lower = std::min(min_value, before);
    auto const upper = std::max(max_value, before);
    value = std::clamp(value + delta, lower, upper);
    return value - before;
}

inline float material_apply_tint_alpha_delta(Color& tint,
                                             float delta) noexcept {
    auto const before = tint.a;
    auto const adjusted =
        static_cast<int>(before)
        + static_cast<int>(std::round(delta * 255.0f));
    tint.a = static_cast<unsigned char>(std::clamp(adjusted, 0, 255));
    return static_cast<float>(
        static_cast<int>(tint.a) - static_cast<int>(before)) / 255.0f;
}

inline void apply_backdrop_luminance_policy(MaterialPlan& plan) noexcept {
    auto const luma_mean = plan.backdrop.luma_mean;
    auto const luma_span = plan.backdrop.luma_span;
    auto const floor_before = plan.luminance_floor;
    auto const gain_before = plan.luminance_gain;
    auto const edge_before = plan.edge_highlight;
    auto const opacity_before = plan.opacity;
    auto const saturation_before = plan.saturation;
    auto const shadow_alpha_before = plan.shadow_alpha;
    auto const shadow_radius_before = plan.shadow_radius;
    bool dark = false;
    bool bright = false;
    bool flat = false;
    float darkness = 0.0f;
    float brightness = 0.0f;
    float flatness = 0.0f;

    if (luma_mean < 0.35f) {
        dark = true;
        darkness = std::clamp((0.35f - luma_mean) / 0.35f, 0.0f, 1.0f);
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
        brightness = std::clamp((luma_mean - 0.72f) / 0.28f, 0.0f, 1.0f);
        plan.luminance_gain = std::max(
            1.0f,
            plan.luminance_gain - 0.04f * brightness);
        plan.edge_highlight = std::min(
            1.0f,
            plan.edge_highlight + 0.04f * brightness);
    }

    if (luma_span < 0.12f) {
        flat = true;
        flatness = std::clamp((0.12f - luma_span) / 0.12f, 0.0f, 1.0f);
        plan.edge_highlight = std::min(
            1.0f,
            plan.edge_highlight + 0.04f * flatness);
    }

    plan.backdrop.opacity_delta = material_apply_scalar_delta(
        plan.opacity,
        0.035f * darkness - 0.020f * brightness + 0.015f * flatness,
        0.0f,
        1.0f);
    plan.backdrop.tint_alpha_delta = material_apply_tint_alpha_delta(
        plan.tint,
        0.045f * darkness - 0.030f * brightness + 0.015f * flatness);
    plan.backdrop.saturation_delta = material_apply_scalar_delta(
        plan.saturation,
        0.040f * darkness - 0.045f * brightness + 0.055f * flatness,
        0.0f,
        1.3f);
    plan.backdrop.shadow_alpha_delta = material_apply_scalar_delta(
        plan.shadow_alpha,
        -0.015f * darkness + 0.025f * brightness + 0.015f * flatness,
        0.0f,
        0.4f);
    plan.backdrop.shadow_radius_delta = material_apply_scalar_delta(
        plan.shadow_radius,
        -0.50f * darkness + 1.50f * brightness + 0.75f * flatness,
        0.0f,
        40.0f);

    plan.backdrop.luminance_response =
        material_luminance_response_name(dark, bright, flat);
    plan.backdrop.frosting_response =
        material_frosting_response_name(dark, bright, flat);
    plan.backdrop.tint_response =
        material_tint_response_name(dark, bright, flat);
    plan.backdrop.saturation_response =
        material_saturation_response_name(dark, bright, flat);
    plan.backdrop.depth_response =
        material_depth_response_name(dark, bright, flat);
    plan.backdrop.luminance_floor_delta =
        plan.luminance_floor - floor_before;
    plan.backdrop.luminance_gain_delta =
        plan.luminance_gain - gain_before;
    plan.backdrop.edge_highlight_delta =
        plan.edge_highlight - edge_before;
    plan.backdrop.response_strength =
        std::max(std::max(darkness, brightness), flatness);
    if (plan.backdrop.response_strength < 0.0001f) {
        plan.backdrop.opacity_delta = plan.opacity - opacity_before;
        plan.backdrop.saturation_delta = plan.saturation - saturation_before;
        plan.backdrop.shadow_alpha_delta =
            plan.shadow_alpha - shadow_alpha_before;
        plan.backdrop.shadow_radius_delta =
            plan.shadow_radius - shadow_radius_before;
    }
}

inline void apply_backdrop_color_policy(MaterialPlan& plan) noexcept {
    if (!plan.backdrop_sampling || plan.backdrop.color_sample_count == 0) {
        plan.backdrop.color_response = "not-sampled";
        return;
    }

    auto const source = plan.backdrop.color_mean;
    auto const colorfulness = material_colorfulness(source);
    auto const distance = material_rgb_distance(plan.tint, source);
    plan.backdrop.color_response =
        material_color_response_name(colorfulness, distance);
    if (std::string_view{plan.backdrop.color_response}
        != "sampled-backdrop-color") {
        return;
    }

    auto const before = plan.tint;
    auto target = source;
    target.a = before.a;
    auto const response_strength = std::clamp(
        0.06f
            + 0.10f * colorfulness
            + 0.04f * std::clamp(plan.backdrop.response_strength, 0.0f, 1.0f),
        0.0f,
        0.18f);
    plan.tint = lerp_color(before, target, response_strength);
    plan.tint.a = before.a;
    plan.backdrop.tint_color_delta =
        material_rgb_distance(before, plan.tint);
}

inline float material_clamped_pointer_coordinate(float value) noexcept {
    return std::isfinite(value) ? std::clamp(value, 0.0f, 1.0f) : 0.5f;
}

inline char const* material_interaction_state_name(
        MaterialInteractionDescriptor input) noexcept {
    if (input.pressed)
        return "pressed";
    if (input.hovered || input.pointer_inside)
        return "hovered";
    if (input.focused)
        return "focused";
    return "idle";
}

inline float material_interaction_strength(
        MaterialInteractionDescriptor input) noexcept {
    float strength = 0.0f;
    if (input.focused)
        strength = std::max(strength, 0.24f);
    if (input.hovered || input.pointer_inside)
        strength = std::max(strength, 0.48f);
    if (input.pressed)
        strength = std::max(strength, 1.0f);
    return strength;
}

inline char const* material_interaction_enablement_reason(
        MaterialPlan const& plan) noexcept {
    if (plan.kind == MaterialKind::None)
        return "inactive-material";
    if (!plan.container.interactive)
        return "noninteractive-container";
    return "interactive-container";
}

inline MaterialInteractionResponse material_resolve_interaction_response(
        MaterialPlan const& plan,
        MaterialInteractionDescriptor input,
        bool reduce_motion) noexcept {
    MaterialInteractionResponse response{};
    response.enablement_reason =
        material_interaction_enablement_reason(plan);
    response.enabled = plan.kind != MaterialKind::None
        && plan.container.interactive;
    response.hovered = input.hovered;
    response.pressed = input.pressed;
    response.focused = input.focused;
    response.pointer_inside = input.pointer_inside;
    response.reduce_motion = reduce_motion;
    response.pointer_x =
        material_clamped_pointer_coordinate(input.pointer_x);
    response.pointer_y =
        material_clamped_pointer_coordinate(input.pointer_y);
    response.deterministic = true;
    if (!response.enabled)
        return response;

    response.state = material_interaction_state_name(input);
    response.response_model = "liquid-glass-interaction";
    response.motion_policy = reduce_motion
        ? "reduced-motion-static"
        : "animated-optical-response";
    auto strength = material_interaction_strength(input);
    if (reduce_motion)
        strength = std::min(strength, 0.36f);
    response.response_strength = strength;
    response.active = strength > 0.0f;
    auto const pointer_driven =
        response.pointer_inside || response.hovered || response.pressed;
    if (pointer_driven && response.active) {
        auto const press_scale = response.pressed ? 1.0f : 0.0f;
        auto const motion_scale = reduce_motion ? 0.45f : 1.0f;
        response.specular_model = "pointer-specular";
        response.specular_highlight_active = true;
        response.specular_anchor_x = response.pointer_x;
        response.specular_anchor_y = response.pointer_y;
        response.specular_radius = std::clamp(
            0.34f - 0.10f * press_scale,
            0.20f,
            0.38f);
        response.specular_intensity = std::clamp(
            motion_scale * (0.42f + 0.18f * press_scale) * strength,
            0.0f,
            0.75f);
    }
    return response;
}

inline void apply_material_interaction_policy(MaterialPlan& plan) noexcept {
    auto response = material_resolve_interaction_response(
        plan,
        plan.command_descriptor.interaction,
        plan.decision_trace.reduce_motion);
    if (!response.enabled || !response.active) {
        plan.interaction = response;
        return;
    }

    auto const before_opacity = plan.opacity;
    auto const before_blur = plan.blur_radius;
    auto const before_saturation = plan.saturation;
    auto const before_edge = plan.edge_highlight;
    auto const before_shadow_alpha = plan.shadow_alpha;
    auto const before_shadow_radius = plan.shadow_radius;
    auto const strength = response.response_strength;
    auto const press = response.pressed ? strength : 0.0f;
    auto const hover = response.pressed
        ? 0.0f
        : ((response.hovered || response.pointer_inside) ? strength : 0.0f);
    auto const focus = (!response.pressed
        && !(response.hovered || response.pointer_inside)
        && response.focused) ? strength : 0.0f;
    auto const motion_scale = response.reduce_motion ? 0.35f : 1.0f;

    plan.opacity = std::clamp(
        plan.opacity + 0.026f * hover + 0.050f * press,
        0.0f,
        1.0f);
    if (plan.backdrop_sampling) {
        auto const blur_delta =
            motion_scale * (0.85f * hover - 1.15f * press);
        plan.blur_radius = std::clamp(
            plan.blur_radius + blur_delta,
            0.0f,
            plan.resource_budget.max_blur_radius);
        plan.saturation = std::clamp(
            plan.saturation
                + motion_scale * (0.025f * hover + 0.035f * press),
            0.0f,
            1.35f);
    }
    plan.edge_highlight = std::clamp(
        plan.edge_highlight
            + 0.055f * hover + 0.090f * press + 0.050f * focus,
        0.0f,
        1.0f);
    plan.shadow_alpha = std::clamp(
        plan.shadow_alpha + 0.012f * hover + 0.022f * press,
        0.0f,
        0.4f);
    plan.shadow_radius = std::clamp(
        plan.shadow_radius
            + motion_scale * (0.80f * hover + 1.20f * press),
        0.0f,
        40.0f);

    response.opacity_delta = plan.opacity - before_opacity;
    response.blur_radius_delta = plan.blur_radius - before_blur;
    response.saturation_delta = plan.saturation - before_saturation;
    response.edge_highlight_delta = plan.edge_highlight - before_edge;
    response.shadow_alpha_delta = plan.shadow_alpha - before_shadow_alpha;
    response.shadow_radius_delta = plan.shadow_radius - before_shadow_radius;
    plan.interaction = response;
}

inline float material_base_refraction_strength(MaterialKind kind) noexcept {
    switch (kind) {
        case MaterialKind::Clear: return 0.075f;
        case MaterialKind::Thin: return 0.095f;
        case MaterialKind::Regular: return 0.120f;
        case MaterialKind::Thick: return 0.150f;
        case MaterialKind::None: return 0.0f;
    }
    return 0.0f;
}

inline MaterialRefractionProfile material_resolve_refraction_profile(
        MaterialPlan const& plan) noexcept {
    MaterialRefractionProfile profile{};
    profile.bounded = true;
    if (!plan.backdrop_sampling || plan.fallback() || plan.kind == MaterialKind::None)
        return profile;

    auto const thickness_strength =
        material_base_refraction_strength(plan.kind);
    auto const radius_strength =
        0.020f * std::clamp(plan.shape.normalized_radius, 0.0f, 1.0f);
    auto const interaction_boost =
        plan.interaction.active
            ? 0.035f * std::clamp(
                plan.interaction.response_strength,
                0.0f,
                1.0f)
            : 0.0f;
    auto const motion_scale = plan.decision_trace.reduce_motion ? 0.48f : 1.0f;
    auto const strength = std::clamp(
        motion_scale * (thickness_strength + radius_strength + interaction_boost),
        0.0f,
        0.22f);
    if (strength <= 0.0001f)
        return profile;

    profile.active = true;
    profile.backdrop_driven = true;
    profile.interaction_driven = plan.interaction.active;
    profile.reduced_motion_suppressed =
        plan.decision_trace.reduce_motion && motion_scale < 1.0f;
    profile.model = profile.interaction_driven
        ? "interactive-edge-lens"
        : "edge-lens";
    profile.source = "sampled-backdrop-edge-refraction";
    profile.strength = strength;
    profile.edge_bias = std::clamp(
        0.32f + 0.28f * plan.shape.normalized_radius,
        0.20f,
        0.72f);
    auto const blur_offset =
        0.18f * std::clamp(plan.blur_radius, 0.0f, material_max_blur_radius);
    auto const edge_offset =
        0.65f * std::clamp(plan.edge_width, 0.5f, 8.0f);
    profile.max_offset_pixels = std::clamp(
        motion_scale * (blur_offset + edge_offset) * profile.strength,
        0.0f,
        3.50f);
    return profile;
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
    return material_shape_kind_name(shape.kind);
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
        && (plan.resource_budget.max_blur_radius < material_max_blur_radius
            || plan.resource_budget.max_sample_taps < material_max_sample_taps
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
    model.interactive_response = plan.interaction.enabled;
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

inline char const* material_optical_response_model_name(
        MaterialPlan const& plan) noexcept {
    if (plan.kind == MaterialKind::None)
        return "inactive";
    if (plan.backdrop_sampling)
        return "sampled-backdrop";
    if (material_plan_uses_standard_content_layer(plan))
        return "standard-content";
    return "deterministic-fallback";
}

inline char const* material_optical_blur_strategy_name(
        MaterialPlan const& plan) noexcept {
    if (!plan.primary_pass.active)
        return "none";
    if (plan.primary_pass.requires_backdrop)
        return "backdrop-sample-blur";
    if (material_stage_matches(plan.primary_pass.executor, "standard-fill"))
        return "standard-fill";
    if (material_stage_matches(plan.primary_pass.executor, "fallback-fill"))
        return "fallback-fill";
    return "none";
}

inline char const* material_optical_color_strategy_name(
        MaterialPlan const& plan) noexcept {
    if (plan.kind == MaterialKind::None || plan.tint.a == 0)
        return "none";
    if (plan.backdrop_sampling)
        return "adaptive-backdrop-color";
    if (material_plan_uses_standard_content_layer(plan))
        return "standard-content-color";
    return "fallback-solid-color";
}

inline char const* material_optical_depth_strategy_name(
        MaterialPlan const& plan) noexcept {
    if (!plan.primary_pass.active)
        return "none";
    bool const edge = plan.edge_highlight > 0.0f;
    bool const shadow = plan.shadow_alpha > 0.0f;
    bool const noise = plan.noise_opacity > 0.0f && plan.backdrop_sampling;
    if (plan.backdrop_sampling && shadow && edge && noise)
        return "layered-shadow-edge-noise";
    if (plan.backdrop_sampling && (shadow || edge))
        return "layered-shadow-edge";
    if (material_plan_uses_standard_content_layer(plan) && edge)
        return "standard-content-edge";
    if ((plan.fallback() || !plan.backdrop_sampling) && shadow && edge)
        return "fallback-shadow-edge";
    if ((plan.fallback() || !plan.backdrop_sampling) && edge)
        return "fallback-edge";
    return "none";
}

inline char const* material_optical_frosting_source_name(
        MaterialPlan const& plan) noexcept {
    if (plan.kind == MaterialKind::None)
        return "none";
    if (plan.backdrop_sampling)
        return "sampled-backdrop-frosting";
    if (plan.fallback())
        return "solid-fallback-frosting";
    if (material_plan_uses_standard_content_layer(plan))
        return "standard-material-fill";
    return "none";
}

inline char const* material_optical_tint_source_name(
        MaterialPlan const& plan) noexcept {
    if (plan.kind == MaterialKind::None || plan.tint.a == 0)
        return "none";
    if (plan.backdrop_sampling)
        return "adaptive-backdrop-tint";
    return "style-tint";
}

inline char const* material_optical_interaction_source_name(
        MaterialPlan const& plan) noexcept {
    if (plan.interaction.active
        && plan.interaction.response_model
        && plan.interaction.response_model[0])
        return plan.interaction.response_model;
    return "none";
}

inline char const* material_optical_refraction_source_name(
        MaterialPlan const& plan) noexcept {
    if (plan.refraction.active
        && plan.refraction.source
        && plan.refraction.source[0])
        return plan.refraction.source;
    return "none";
}

inline char const* material_optical_stage_order_name(
        MaterialPlan const& plan) noexcept {
    if (!plan.primary_pass.active)
        return "none";
    bool const shadow = plan.shadow_alpha > 0.0f;
    bool const edge = plan.edge_highlight > 0.0f;
    bool const noise = plan.backdrop_sampling && plan.noise_opacity > 0.0f;
    if (shadow && edge && noise)
        return "shadow-primary-edge-noise";
    if (shadow && edge)
        return "shadow-primary-edge";
    if (shadow && noise)
        return "shadow-primary-noise";
    if (edge && noise)
        return "primary-edge-noise";
    if (shadow)
        return "shadow-primary";
    if (edge)
        return "primary-edge";
    if (noise)
        return "primary-noise";
    return "primary";
}

inline char const* material_optical_backdrop_capture_policy_name(
        MaterialPlan const& plan) noexcept {
    if (!plan.backdrop_access.shared_frame_capture
        && !plan.backdrop_access.next_frame_capture_required
        && !plan.backdrop_access.required) {
        return "no-capture";
    }
    if (plan.backdrop_sampling)
        return "sample-current-frame";
    if (plan.backdrop_access.next_frame_capture_required)
        return "warmup-next-frame";
    return "shared-frame-capture";
}

inline char const* material_optical_foreground_sampling_policy_name(
        MaterialPlan const& plan) noexcept {
    bool const captures_backdrop =
        plan.backdrop_access.shared_frame_capture
        || plan.backdrop_access.next_frame_capture_required
        || plan.backdrop_access.required;
    if (!captures_backdrop)
        return "not-applicable";
    if (plan.backdrop_access.excludes_foreground_text
        && plan.backdrop_sampling) {
        return "foreground-excluded-from-sample";
    }
    if (plan.backdrop_access.excludes_foreground_text)
        return "foreground-excluded-from-warmup";
    return "foreground-inclusion-risk";
}

inline MaterialOpticalComposition material_resolve_optical_composition(
        MaterialPlan const& plan) noexcept {
    MaterialOpticalComposition composition{};
    composition.schema_version = plan.contract_version;
    composition.model = material_optical_response_model_name(plan);
    composition.blur_source = material_optical_blur_strategy_name(plan);
    composition.frosting_source = material_optical_frosting_source_name(plan);
    composition.tint_source = material_optical_tint_source_name(plan);
    composition.luminance_source = plan.luminance_curve.name;
    composition.depth_source = material_optical_depth_strategy_name(plan);
    composition.refraction_source =
        material_optical_refraction_source_name(plan);
    composition.interaction_source =
        material_optical_interaction_source_name(plan);
    composition.fallback_source = plan.fallback()
        ? material_fallback_path_name(plan.fallback_path)
        : "none";
    composition.stage_order = material_optical_stage_order_name(plan);
    composition.backdrop_capture_policy =
        material_optical_backdrop_capture_policy_name(plan);
    composition.foreground_sampling_policy =
        material_optical_foreground_sampling_policy_name(plan);
    composition.backdrop_sampled = plan.backdrop_sampling;
    composition.blur_required = plan.primary_pass.requires_backdrop;
    composition.frosting_required = plan.backdrop_sampling;
    composition.tint_required =
        plan.kind != MaterialKind::None && plan.tint.a > 0;
    composition.saturation_required =
        plan.backdrop_sampling && std::fabs(plan.saturation - 1.0f) > 0.0001f;
    composition.luminance_required =
        plan.kind != MaterialKind::None
        && (plan.luminance_curve.bounded
            || plan.foreground.primary_contrast_ratio
                >= plan.foreground.minimum_contrast_ratio);
    composition.edge_required =
        plan.primary_pass.active && plan.edge_highlight > 0.0f;
    composition.shadow_required =
        plan.primary_pass.active && plan.shadow_alpha > 0.0f;
    composition.noise_required =
        plan.backdrop_sampling && plan.noise_opacity > 0.0f;
    composition.refraction_required = plan.refraction.active;
    composition.interaction_required = plan.interaction.active;
    composition.fallback_required = plan.fallback();
    composition.backdrop_capture_required =
        plan.backdrop_access.shared_frame_capture
        || plan.backdrop_access.next_frame_capture_required
        || plan.backdrop_access.required;
    composition.foreground_excluded_from_backdrop =
        plan.backdrop_access.excludes_foreground_text;
    composition.stage_order_stable = true;
    composition.bounded =
        plan.resource_budget.bounded_texture_copy
        && plan.sampling_kernel.bounded
        && plan.luminance_curve.bounded
        && plan.backdrop_access.bounded
        && plan.refraction.bounded;
    composition.deterministic =
        plan.resource_budget.deterministic_fallback
        && plan.foreground.deterministic
        && plan.interaction.deterministic;
    composition.opacity = plan.opacity;
    composition.blur_radius = plan.blur_radius;
    composition.tint_alpha = material_alpha_fraction(plan.tint);
    composition.saturation = plan.saturation;
    composition.luminance_floor = plan.luminance_floor;
    composition.luminance_gain = plan.luminance_gain;
    composition.edge_highlight = plan.edge_highlight;
    composition.edge_width = plan.edge_width;
    composition.noise_opacity = plan.noise_opacity;
    composition.shadow_alpha = plan.shadow_alpha;
    composition.shadow_radius = plan.shadow_radius;
    composition.refraction_strength = plan.refraction.strength;
    composition.refraction_edge_bias = plan.refraction.edge_bias;
    composition.refraction_offset_pixels = plan.refraction.max_offset_pixels;
    composition.interaction_response_strength =
        plan.interaction.response_strength;
    composition.sample_taps = plan.sample_taps;
    composition.max_texture_copy_pixels =
        plan.primary_pass.max_texture_copy_pixels;
    composition.max_surface_sample_pixels =
        plan.backdrop_access.max_surface_sample_pixels;
    return composition;
}

inline MaterialOpticalResponseContract material_resolve_optical_response(
        MaterialPlan const& plan) noexcept {
    auto const& composition = plan.optical_composition;
    MaterialOpticalResponseContract response{};
    response.response_model = composition.model;
    response.blur_strategy = composition.blur_source;
    response.color_strategy = material_optical_color_strategy_name(plan);
    response.depth_strategy = composition.depth_source;
    response.backdrop_driven = composition.backdrop_sampled;
    response.blur_active = composition.blur_required;
    response.frosting_active = composition.frosting_required;
    response.tint_active = composition.tint_required;
    response.saturation_active = composition.saturation_required;
    response.luminance_preservation_active =
        composition.luminance_required;
    response.edge_highlight_active = composition.edge_required;
    response.depth_shadow_active = composition.shadow_required;
    response.noise_dither_active = composition.noise_required;
    response.refraction_active = composition.refraction_required;
    response.foreground_vibrancy_active = plan.foreground.uses_vibrancy;
    response.interaction_active = composition.interaction_required;
    response.interaction_modulates_optics =
        composition.interaction_required
        && (std::fabs(plan.interaction.opacity_delta) > 0.0001f
            || std::fabs(plan.interaction.blur_radius_delta) > 0.0001f
            || std::fabs(plan.interaction.saturation_delta) > 0.0001f
            || std::fabs(plan.interaction.edge_highlight_delta) > 0.0001f
            || std::fabs(plan.interaction.shadow_alpha_delta) > 0.0001f
            || std::fabs(plan.interaction.shadow_radius_delta) > 0.0001f);
    response.deterministic_fallback = composition.deterministic;
    return response;
}

inline MaterialPlan plan_material_surface(MaterialRequest request,
                                          MaterialEnvironment environment) noexcept {
    MaterialPlan plan{};
    auto const& style = request.style;
    auto const capability_snapshot = analyze_material_capabilities(
        environment.capabilities,
        environment.render_target);
    MaterialQualityPolicy resolved_quality =
        resolve_material_quality_policy(
            environment.quality,
            capability_snapshot);
    auto const max_blur_radius = std::max(
        0.0f,
        resolved_quality.max_blur_radius);
    plan.kind = style.kind;
    plan.role = style.role;
    plan.command_descriptor = material_command_descriptor(style);
    plan.geometry = request.geometry;
    plan.shape = analyze_material_shape(request.geometry);
    plan.capability_snapshot = capability_snapshot;
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
    plan.resource_budget.max_paint_layers = material_max_paint_layers;
    plan.resource_budget.max_backdrop_pixels =
        resolved_quality.max_backdrop_pixels;
    plan.resource_budget.max_frame_capture_count = 0;
    plan.resource_budget.max_frame_capture_pixels = 0;
    plan.resource_budget.max_surface_sample_pixels = 0;
    plan.resource_budget.max_container_spacing = plan.container.spacing;
    plan.resource_budget.max_paint_layer_inflate = 0.0f;
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
        plan.render_target.within_backdrop_budget
        && plan.capability_snapshot.target_within_texture_limits
        && plan.capability_snapshot.target_within_backdrop_budget;
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
    plan.decision_trace.capability_texture_limits_known =
        plan.capability_snapshot.texture_limits_known;
    plan.decision_trace.capability_backdrop_budget_known =
        plan.capability_snapshot.backdrop_budget_known;
    plan.decision_trace.capability_target_within_texture_limits =
        plan.capability_snapshot.target_within_texture_limits;
    plan.decision_trace.capability_target_within_backdrop_budget =
        plan.capability_snapshot.target_within_backdrop_budget;
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
        apply_backdrop_color_policy(plan);
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
    apply_material_interaction_policy(plan);
    plan.refraction = material_resolve_refraction_profile(plan);
    plan.resource_budget.max_refraction_offset_pixels =
        plan.refraction.max_offset_pixels;
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
    plan.optical_composition = material_resolve_optical_composition(plan);
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
                material_shadow_stage_optics(plan),
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
                material_primary_stage_optics(plan),
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
                material_edge_stage_optics(plan),
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
                material_noise_stage_optics(plan),
            });
    }
    resolve_material_paint_layers(plan);
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
    plan.optical_response = material_resolve_optical_response(plan);
    plan.observation_contract = material_observation_contract(plan);
    plan.execution_audit = material_execution_audit(plan);
    return plan;
}

} // namespace phenotype
