module;
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cmath>
#include <span>
#include <string_view>
#include <vector>

export module phenotype.material.types;

import phenotype.types;
import phenotype.theme_contract;

export namespace phenotype {

inline constexpr std::uint32_t material_plan_contract_version = 86;
inline constexpr unsigned int material_max_execution_stages = 4;
inline constexpr unsigned int material_max_paint_layers = 4;
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
        case MaterialSurfaceRole::Control:
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
    char const* pointer_lens_model = "none";
    float pointer_lens_anchor_x = 0.5f;
    float pointer_lens_anchor_y = 0.5f;
    float pointer_lens_radius = 0.0f;
    float pointer_lens_strength = 0.0f;
    char const* control_morph_model = "none";
    float control_morph_scale_delta = 0.0f;
    float control_morph_depth = 0.0f;
    float control_morph_edge = 0.0f;
    float control_morph_shadow = 0.0f;
    char const* prominent_glass_model = "none";
    float prominent_glass_intensity = 0.0f;
    float prominent_glass_tint_weight = 0.0f;
    float prominent_glass_edge_lift = 0.0f;
    float prominent_glass_lensing_gain = 1.0f;
    char const* refraction_model = "none";
    float refraction_strength = 0.0f;
    float refraction_edge_bias = 0.0f;
    float refraction_offset_pixels = 0.0f;
    float refraction_edge_caustic_intensity = 0.0f;
    char const* edge_optics_model = "none";
    float edge_bevel_width = 0.0f;
    float edge_inner_highlight = 0.0f;
    float edge_outer_shadow = 0.0f;
    float edge_chromatic_fringe = 0.0f;
    char const* spectral_tint_model = "none";
    float spectral_tint_warmth = 0.0f;
    float spectral_tint_coolness = 0.0f;
    float spectral_dispersion = 0.0f;
    float spectral_rim_tint = 0.0f;
    char const* dynamic_lighting_model = "none";
    float dynamic_light_direction_x = 0.0f;
    float dynamic_light_direction_y = 0.0f;
    float dynamic_light_highlight = 0.0f;
    float dynamic_light_shadow = 0.0f;
    char const* glass_caustic_flow_model = "none";
    float glass_caustic_flow_strength = 0.0f;
    float glass_caustic_chroma_shear = 0.0f;
    float glass_caustic_highlight_drift = 0.0f;
    float glass_caustic_focus = 0.0f;
    char const* glass_meniscus_model = "none";
    float glass_meniscus_rim_width = 0.0f;
    float glass_meniscus_edge_pull = 0.0f;
    float glass_meniscus_highlight_gain = 1.0f;
    float glass_meniscus_refraction_gain = 1.0f;
    char const* glass_transmission_model = "none";
    float glass_internal_transmission = 0.0f;
    float glass_subsurface_scatter = 0.0f;
    float glass_volume_absorption = 0.0f;
    float glass_interlayer_refraction = 0.0f;
    char const* glass_surface_cohesion_model = "none";
    float glass_surface_response = 0.0f;
    float glass_edge_adhesion = 0.0f;
    float glass_shape_coalescence = 0.0f;
    float glass_luma_stability = 0.0f;
    char const* glass_substrate_adhesion_model = "none";
    float glass_substrate_contact = 0.0f;
    float glass_substrate_settle = 0.0f;
    float glass_substrate_shadow = 0.0f;
    float glass_substrate_refraction = 0.0f;
    char const* glass_ambient_reflection_model = "none";
    float glass_ambient_reflection_gain = 0.0f;
    float glass_ambient_color_bleed = 0.0f;
    float glass_ambient_luma_polarization = 0.0f;
    float glass_ambient_sheen_coherence = 0.0f;
    char const* glass_thickness_model = "none";
    float glass_thickness = 0.0f;
    float glass_lensing_gain = 1.0f;
    float glass_shadow_gain = 1.0f;
    float glass_scattering_gain = 1.0f;
    char const* glass_dispersion_model = "none";
    float glass_dispersion_axial_offset = 0.0f;
    float glass_dispersion_tangential_offset = 0.0f;
    float glass_dispersion_prismatic_gain = 1.0f;
    float glass_dispersion_caustic_spread = 0.0f;
    char const* glass_stabilization_model = "none";
    float glass_stabilization_strength = 0.0f;
    float glass_stabilization_damping = 0.0f;
    float glass_stabilization_shimmer_reduction = 0.0f;
    float glass_stabilization_transmission_bias = 0.0f;
    char const* glass_environment_model = "none";
    float glass_environment_reflection_strength = 0.0f;
    float glass_environment_color_pickup = 0.0f;
    float glass_environment_luminance_balance = 0.0f;
    float glass_environment_transmission_balance = 0.0f;
    char const* glass_depth_model = "none";
    float glass_depth_separation = 0.0f;
    float glass_depth_inner_shadow = 0.0f;
    float glass_depth_surface_lift = 0.0f;
    float glass_depth_parallax_gain = 0.0f;
    char const* scroll_edge_model = "none";
    float scroll_edge_extent = 0.0f;
    float scroll_edge_dissolve = 0.0f;
    float scroll_edge_dimming = 0.0f;
    float scroll_edge_hard_style = 0.0f;
    char const* clear_glass_legibility_model = "none";
    float clear_glass_dimming = 0.0f;
    float clear_glass_contrast = 0.0f;
    float clear_glass_brightness_response = 0.0f;
    float clear_glass_detail_response = 0.0f;
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
    char const* background_effect = "none";
    float x_offset = 0.0f;
    float y_offset = 0.0f;
    float inflate = 0.0f;
    float radius_delta = 0.0f;
    float stroke_width = 0.0f;
    Color color = {0, 0, 0, 0};
    float opacity = 0.0f;
    float soft_edge_radius = 0.0f;
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
    float edge_caustic_intensity = 0.0f;
};

struct MaterialSpecularProfile {
    char const* model = "none";
    char const* source = "none";
    bool active = false;
    bool ambient = false;
    bool interaction_driven = false;
    bool bounded = true;
    float anchor_x = 0.5f;
    float anchor_y = 0.5f;
    float radius = 0.0f;
    float intensity = 0.0f;
};

struct MaterialEdgeOpticsProfile {
    char const* model = "none";
    char const* source = "none";
    bool active = false;
    bool backdrop_driven = false;
    bool caustic_driven = false;
    bool bounded = true;
    float bevel_width = 0.0f;
    float inner_highlight = 0.0f;
    float outer_shadow = 0.0f;
    float chromatic_fringe = 0.0f;
};

struct MaterialProminentGlassProfile {
    char const* model = "none";
    char const* source = "none";
    bool active = false;
    bool control_driven = false;
    bool tint_driven = false;
    bool backdrop_driven = false;
    bool interaction_driven = false;
    bool reduced_motion_suppressed = false;
    bool bounded = true;
    float intensity = 0.0f;
    float tint_weight = 0.0f;
    float edge_lift = 0.0f;
    float lensing_gain = 1.0f;
};

struct MaterialSpectralTintProfile {
    char const* model = "none";
    char const* source = "none";
    bool active = false;
    bool backdrop_driven = false;
    bool color_driven = false;
    bool tint_driven = false;
    bool caustic_driven = false;
    bool bounded = true;
    float warmth = 0.0f;
    float coolness = 0.0f;
    float dispersion = 0.0f;
    float rim_tint = 0.0f;
    float balance = 0.5f;
    float tint_influence = 0.0f;
};

struct MaterialDynamicLightingProfile {
    char const* model = "none";
    char const* source = "none";
    bool active = false;
    bool backdrop_driven = false;
    bool color_driven = false;
    bool caustic_driven = false;
    bool interaction_driven = false;
    bool reduced_motion_suppressed = false;
    bool bounded = true;
    float direction_x = 0.0f;
    float direction_y = 0.0f;
    float highlight_strength = 0.0f;
    float shadow_strength = 0.0f;
};

struct MaterialGlassCausticFlowProfile {
    char const* model = "none";
    char const* source = "none";
    bool active = false;
    bool backdrop_driven = false;
    bool lighting_driven = false;
    bool dispersion_driven = false;
    bool depth_driven = false;
    bool interaction_driven = false;
    bool reduced_motion_suppressed = false;
    bool bounded = true;
    float flow_strength = 0.0f;
    float chroma_shear = 0.0f;
    float highlight_drift = 0.0f;
    float caustic_focus = 0.0f;
};

struct MaterialGlassMeniscusProfile {
    char const* model = "none";
    char const* source = "none";
    bool active = false;
    bool edge_driven = false;
    bool depth_driven = false;
    bool caustic_driven = false;
    bool environment_driven = false;
    bool interaction_driven = false;
    bool reduced_motion_suppressed = false;
    bool bounded = true;
    float rim_width = 0.0f;
    float edge_pull = 0.0f;
    float highlight_gain = 1.0f;
    float refraction_gain = 1.0f;
};

struct MaterialGlassTransmissionProfile {
    char const* model = "none";
    char const* source = "none";
    bool active = false;
    bool thickness_driven = false;
    bool depth_driven = false;
    bool environment_driven = false;
    bool caustic_driven = false;
    bool stabilization_driven = false;
    bool interaction_driven = false;
    bool reduced_motion_suppressed = false;
    bool bounded = true;
    float internal_transmission = 0.0f;
    float subsurface_scatter = 0.0f;
    float volume_absorption = 0.0f;
    float interlayer_refraction = 0.0f;
};

struct MaterialGlassSurfaceCohesionProfile {
    char const* model = "none";
    char const* source = "none";
    bool active = false;
    bool thickness_driven = false;
    bool transmission_driven = false;
    bool meniscus_driven = false;
    bool caustic_driven = false;
    bool interaction_driven = false;
    bool reduced_motion_suppressed = false;
    bool bounded = true;
    float surface_response = 0.0f;
    float edge_adhesion = 0.0f;
    float shape_coalescence = 0.0f;
    float luma_stability = 0.0f;
};

struct MaterialGlassSubstrateAdhesionProfile {
    char const* model = "none";
    char const* source = "none";
    bool active = false;
    bool surface_driven = false;
    bool transmission_driven = false;
    bool depth_driven = false;
    bool environment_driven = false;
    bool interaction_driven = false;
    bool reduced_motion_suppressed = false;
    bool bounded = true;
    float contact_strength = 0.0f;
    float settle_depth = 0.0f;
    float contact_shadow = 0.0f;
    float refraction_crawl = 0.0f;
};

struct MaterialGlassAmbientReflectionProfile {
    char const* model = "none";
    char const* source = "none";
    bool active = false;
    bool environment_driven = false;
    bool transmission_driven = false;
    bool substrate_driven = false;
    bool surface_driven = false;
    bool lighting_driven = false;
    bool interaction_driven = false;
    bool reduced_motion_suppressed = false;
    bool bounded = true;
    float reflection_gain = 0.0f;
    float color_bleed = 0.0f;
    float luma_polarization = 0.0f;
    float sheen_coherence = 0.0f;
};

struct MaterialGlassEnvironmentProfile {
    char const* model = "none";
    char const* source = "none";
    bool active = false;
    bool backdrop_driven = false;
    bool color_driven = false;
    bool luminance_driven = false;
    bool reflection_driven = false;
    bool stabilization_driven = false;
    bool container_driven = false;
    bool bounded = true;
    float reflection_strength = 0.0f;
    float color_pickup = 0.0f;
    float luminance_balance = 0.0f;
    float transmission_balance = 0.0f;
};

struct MaterialGlassDepthResponseProfile {
    char const* model = "none";
    char const* source = "none";
    bool active = false;
    bool thickness_driven = false;
    bool environment_driven = false;
    bool geometry_driven = false;
    bool luminance_driven = false;
    bool stabilization_driven = false;
    bool bounded = true;
    float depth_separation = 0.0f;
    float inner_shadow = 0.0f;
    float surface_lift = 0.0f;
    float parallax_gain = 0.0f;
};

struct MaterialGlassThicknessProfile {
    char const* model = "none";
    char const* source = "none";
    bool active = false;
    bool size_driven = false;
    bool transition_driven = false;
    bool interaction_driven = false;
    bool reduced_motion_suppressed = false;
    bool bounded = true;
    float thickness = 0.0f;
    float lensing_gain = 1.0f;
    float shadow_gain = 1.0f;
    float scattering_gain = 1.0f;
};

struct MaterialGlassDispersionProfile {
    char const* model = "none";
    char const* source = "none";
    bool active = false;
    bool spectral_driven = false;
    bool thickness_driven = false;
    bool transition_driven = false;
    bool lighting_driven = false;
    bool interaction_driven = false;
    bool reduced_motion_suppressed = false;
    bool bounded = true;
    float axial_offset_pixels = 0.0f;
    float tangential_offset_pixels = 0.0f;
    float prismatic_gain = 1.0f;
    float caustic_spread = 0.0f;
};

struct MaterialGlassStabilizationProfile {
    char const* model = "none";
    char const* source = "none";
    bool active = false;
    bool backdrop_driven = false;
    bool motion_driven = false;
    bool interaction_driven = false;
    bool transition_driven = false;
    bool container_driven = false;
    bool reduced_motion_suppressed = false;
    bool bounded = true;
    float strength = 0.0f;
    float damping = 0.0f;
    float shimmer_reduction = 0.0f;
    float transmission_bias = 0.0f;
};

struct MaterialScrollEdgeProfile {
    char const* model = "none";
    char const* source = "none";
    bool active = false;
    bool role_driven = false;
    bool backdrop_driven = false;
    bool contrast_driven = false;
    bool hard_style = false;
    bool legibility_driven = false;
    bool bounded = true;
    float response_strength = 0.0f;
    float fade_extent_pixels = 0.0f;
    float dissolve_strength = 0.0f;
    float dimming_strength = 0.0f;
    float hard_style_strength = 0.0f;
    float opacity_delta = 0.0f;
    float tint_alpha_delta = 0.0f;
    float luminance_floor_delta = 0.0f;
    float edge_highlight_delta = 0.0f;
};

struct MaterialClearGlassLegibilityProfile {
    char const* model = "none";
    char const* source = "none";
    bool active = false;
    bool backdrop_driven = false;
    bool brightness_driven = false;
    bool detail_driven = false;
    bool contrast_driven = false;
    bool accessibility_driven = false;
    bool bounded = true;
    float dimming_strength = 0.0f;
    float contrast_lift = 0.0f;
    float brightness_response = 0.0f;
    float detail_response = 0.0f;
};

struct MaterialLargeSurfaceLegibilityProfile {
    char const* model = "none";
    char const* source = "none";
    bool active = false;
    bool role_driven = false;
    bool size_driven = false;
    bool backdrop_driven = false;
    bool contrast_driven = false;
    bool brightness_driven = false;
    bool bounded = true;
    float response_strength = 0.0f;
    float opacity_delta = 0.0f;
    float tint_alpha_delta = 0.0f;
    float luminance_floor_delta = 0.0f;
    float edge_highlight_delta = 0.0f;
    float shadow_alpha_delta = 0.0f;
    float shadow_radius_delta = 0.0f;
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
    char const* pointer_lens_model = "none";
    bool pointer_lens_active = false;
    float pointer_lens_anchor_x = 0.5f;
    float pointer_lens_anchor_y = 0.5f;
    float pointer_lens_radius = 0.0f;
    float pointer_lens_strength = 0.0f;
    char const* control_morph_model = "none";
    bool control_morph_active = false;
    bool control_morph_reduced_motion_suppressed = false;
    float control_morph_scale_delta = 0.0f;
    float control_morph_depth = 0.0f;
    float control_morph_edge = 0.0f;
    float control_morph_shadow = 0.0f;
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
    bool spectral_tint_active = false;
    bool dynamic_lighting_active = false;
    bool glass_caustic_flow_active = false;
    bool glass_meniscus_active = false;
    bool glass_transmission_active = false;
    bool glass_surface_cohesion_active = false;
    bool glass_substrate_adhesion_active = false;
    bool glass_ambient_reflection_active = false;
    bool glass_thickness_active = false;
    bool glass_dispersion_active = false;
    bool glass_stabilization_active = false;
    bool glass_environment_active = false;
    bool glass_depth_active = false;
    bool scroll_edge_active = false;
    bool prominent_glass_active = false;
    bool clear_glass_legibility_active = false;
    bool large_surface_legibility_active = false;
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
    char const* spectral_tint_source = "none";
    char const* dynamic_lighting_source = "none";
    char const* glass_caustic_flow_source = "none";
    char const* glass_meniscus_source = "none";
    char const* glass_transmission_source = "none";
    char const* glass_surface_cohesion_source = "none";
    char const* glass_substrate_adhesion_source = "none";
    char const* glass_ambient_reflection_source = "none";
    char const* glass_thickness_source = "none";
    char const* glass_dispersion_source = "none";
    char const* glass_stabilization_source = "none";
    char const* glass_environment_source = "none";
    char const* glass_depth_source = "none";
    char const* scroll_edge_source = "none";
    char const* prominent_glass_source = "none";
    char const* clear_glass_legibility_source = "none";
    char const* large_surface_legibility_source = "none";
    char const* interaction_source = "none";
    char const* transition_source = "identity";
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
    bool spectral_tint_required = false;
    bool dynamic_lighting_required = false;
    bool glass_caustic_flow_required = false;
    bool glass_meniscus_required = false;
    bool glass_transmission_required = false;
    bool glass_surface_cohesion_required = false;
    bool glass_substrate_adhesion_required = false;
    bool glass_ambient_reflection_required = false;
    bool glass_thickness_required = false;
    bool glass_dispersion_required = false;
    bool glass_stabilization_required = false;
    bool glass_environment_required = false;
    bool glass_depth_required = false;
    bool scroll_edge_required = false;
    bool prominent_glass_required = false;
    bool clear_glass_legibility_required = false;
    bool large_surface_legibility_required = false;
    bool interaction_required = false;
    bool transition_required = false;
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
    float refraction_edge_caustic_intensity = 0.0f;
    float edge_bevel_width = 0.0f;
    float edge_inner_highlight = 0.0f;
    float edge_outer_shadow = 0.0f;
    float edge_chromatic_fringe = 0.0f;
    float spectral_tint_warmth = 0.0f;
    float spectral_tint_coolness = 0.0f;
    float spectral_dispersion = 0.0f;
    float spectral_rim_tint = 0.0f;
    float spectral_tint_balance = 0.5f;
    float spectral_tint_influence = 0.0f;
    float dynamic_light_direction_x = 0.0f;
    float dynamic_light_direction_y = 0.0f;
    float dynamic_light_highlight = 0.0f;
    float dynamic_light_shadow = 0.0f;
    float glass_caustic_flow_strength = 0.0f;
    float glass_caustic_chroma_shear = 0.0f;
    float glass_caustic_highlight_drift = 0.0f;
    float glass_caustic_focus = 0.0f;
    float glass_meniscus_rim_width = 0.0f;
    float glass_meniscus_edge_pull = 0.0f;
    float glass_meniscus_highlight_gain = 1.0f;
    float glass_meniscus_refraction_gain = 1.0f;
    float glass_internal_transmission = 0.0f;
    float glass_subsurface_scatter = 0.0f;
    float glass_volume_absorption = 0.0f;
    float glass_interlayer_refraction = 0.0f;
    float glass_surface_response = 0.0f;
    float glass_edge_adhesion = 0.0f;
    float glass_shape_coalescence = 0.0f;
    float glass_luma_stability = 0.0f;
    float glass_substrate_contact = 0.0f;
    float glass_substrate_settle = 0.0f;
    float glass_substrate_shadow = 0.0f;
    float glass_substrate_refraction = 0.0f;
    float glass_ambient_reflection_gain = 0.0f;
    float glass_ambient_color_bleed = 0.0f;
    float glass_ambient_luma_polarization = 0.0f;
    float glass_ambient_sheen_coherence = 0.0f;
    float glass_thickness = 0.0f;
    float glass_lensing_gain = 1.0f;
    float glass_shadow_gain = 1.0f;
    float glass_scattering_gain = 1.0f;
    float glass_dispersion_axial_offset = 0.0f;
    float glass_dispersion_tangential_offset = 0.0f;
    float glass_dispersion_prismatic_gain = 1.0f;
    float glass_dispersion_caustic_spread = 0.0f;
    float glass_stabilization_strength = 0.0f;
    float glass_stabilization_damping = 0.0f;
    float glass_stabilization_shimmer_reduction = 0.0f;
    float glass_stabilization_transmission_bias = 0.0f;
    float glass_environment_reflection_strength = 0.0f;
    float glass_environment_color_pickup = 0.0f;
    float glass_environment_luminance_balance = 0.0f;
    float glass_environment_transmission_balance = 0.0f;
    float glass_depth_separation = 0.0f;
    float glass_depth_inner_shadow = 0.0f;
    float glass_depth_surface_lift = 0.0f;
    float glass_depth_parallax_gain = 0.0f;
    float scroll_edge_response = 0.0f;
    float scroll_edge_extent = 0.0f;
    float scroll_edge_dissolve = 0.0f;
    float scroll_edge_dimming = 0.0f;
    float scroll_edge_hard_style = 0.0f;
    float scroll_edge_opacity_delta = 0.0f;
    float scroll_edge_tint_alpha_delta = 0.0f;
    float scroll_edge_luminance_floor_delta = 0.0f;
    float scroll_edge_edge_highlight_delta = 0.0f;
    float prominent_glass_intensity = 0.0f;
    float prominent_glass_tint_weight = 0.0f;
    float prominent_glass_edge_lift = 0.0f;
    float prominent_glass_lensing_gain = 1.0f;
    float clear_glass_dimming = 0.0f;
    float clear_glass_contrast = 0.0f;
    float clear_glass_brightness_response = 0.0f;
    float clear_glass_detail_response = 0.0f;
    float large_surface_legibility_response = 0.0f;
    float large_surface_opacity_delta = 0.0f;
    float large_surface_tint_alpha_delta = 0.0f;
    float large_surface_luminance_floor_delta = 0.0f;
    float large_surface_edge_highlight_delta = 0.0f;
    float interaction_response_strength = 0.0f;
    float interaction_control_morph_scale_delta = 0.0f;
    float interaction_control_morph_depth = 0.0f;
    float interaction_control_morph_edge = 0.0f;
    float interaction_control_morph_shadow = 0.0f;
    float transition_progress = 1.0f;
    float transition_opacity_gain = 1.0f;
    float transition_optical_gain = 1.0f;
    float transition_refraction_gain = 1.0f;
    float transition_materialize_wave_strength = 0.0f;
    float transition_materialize_edge_lift = 0.0f;
    float transition_materialize_lensing_gain = 1.0f;
    float transition_materialize_rim_position = 0.0f;
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

struct MaterialTransitionAnalysis {
    MaterialGlassTransitionKind kind = MaterialGlassTransitionKind::Identity;
    char const* kind_name = "identity";
    bool active = false;
    bool materialize = false;
    bool matched_geometry = false;
    bool appearing = true;
    bool reduced_motion_suppressed = false;
    bool bounded = true;
    float progress = 1.0f;
    float opacity_gain = 1.0f;
    float optical_gain = 1.0f;
    float shadow_gain = 1.0f;
    float refraction_gain = 1.0f;
    char const* materialize_optics_model = "none";
    bool materialize_optics_active = false;
    float materialize_wave_strength = 0.0f;
    float materialize_edge_lift = 0.0f;
    float materialize_lensing_gain = 1.0f;
    float materialize_rim_position = 0.0f;
    char const* policy = "identity";
};

struct MaterialGlassIdentityAnalysis {
    std::uint32_t namespace_id = 0;
    std::uint32_t effect_id = 0;
    bool namespace_scoped = false;
    bool effect_identified = false;
    bool participates = false;
    bool container_scoped = false;
    bool matched_geometry_candidate = false;
    bool bounded = true;
    char const* source = "none";
    char const* policy = "unidentified";
};

struct MaterialGlassBackgroundAnalysis {
    bool active = false;
    MaterialGlassBackgroundEffectKind kind =
        MaterialGlassBackgroundEffectKind::None;
    char const* kind_name = "none";
    bool automatic = false;
    bool plate = false;
    bool feathered = false;
    float feather_padding = 0.0f;
    float soft_edge_radius = 0.0f;
    char const* optical_profile = "none";
    char const* edge_policy = "none";
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
    bool glass_effect_identified = false;
    bool glass_effect_matched_geometry = false;
    char const* glass_background_effect = "none";
    bool glass_background_plate = false;
    bool glass_background_feathered = false;
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
    MaterialTransitionAnalysis transition{};
    MaterialGlassIdentityAnalysis glass_identity{};
    MaterialGlassBackgroundAnalysis glass_background{};
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
    MaterialSpecularProfile specular{};
    MaterialEdgeOpticsProfile edge_optics{};
    MaterialSpectralTintProfile spectral_tint{};
    MaterialDynamicLightingProfile dynamic_lighting{};
    MaterialGlassCausticFlowProfile glass_caustic_flow{};
    MaterialGlassMeniscusProfile glass_meniscus{};
    MaterialGlassTransmissionProfile glass_transmission{};
    MaterialGlassSurfaceCohesionProfile glass_surface_cohesion{};
    MaterialGlassSubstrateAdhesionProfile glass_substrate_adhesion{};
    MaterialGlassAmbientReflectionProfile glass_ambient_reflection{};
    MaterialGlassThicknessProfile glass_thickness{};
    MaterialGlassDispersionProfile glass_dispersion{};
    MaterialGlassStabilizationProfile glass_stabilization{};
    MaterialGlassEnvironmentProfile glass_environment{};
    MaterialGlassDepthResponseProfile glass_depth{};
    MaterialScrollEdgeProfile scroll_edge{};
    MaterialProminentGlassProfile prominent_glass{};
    MaterialClearGlassLegibilityProfile clear_glass_legibility{};
    MaterialLargeSurfaceLegibilityProfile large_surface_legibility{};
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

} // namespace phenotype
