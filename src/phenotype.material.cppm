module;
#include <algorithm>
#include <cstdint>

export module phenotype.material;

import phenotype.types;

export namespace phenotype {

struct MaterialGeometry {
    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;
    float radius = 0.0f;
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
};

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

struct MaterialVerifierExpectation {
    bool require_backdrop_source = false;
    bool require_edge_highlight = false;
    float min_luma_delta = 0.0f;
    int min_unique_colors = 1;
    char const* region_name = "material";
    char const* likely_layer = "material-fallback";
};

struct MaterialPassExpectation {
    char const* name = "none";
    bool active = false;
    bool requires_backdrop = false;
    unsigned int sample_taps = 0;
    char const* likely_layer = "material-fallback";
};

struct MaterialResourceBudget {
    float max_blur_radius = 0.0f;
    unsigned int max_sample_taps = 0;
    unsigned int max_pass_count = 1;
    std::int64_t max_backdrop_pixels = 0;
    bool bounded_texture_copy = true;
    bool deterministic_fallback = true;
};

struct MaterialPlan {
    MaterialKind kind = MaterialKind::None;
    MaterialGeometry geometry{};
    float opacity = 0.0f;
    float blur_radius = 0.0f;
    Color tint = {0, 0, 0, 0};
    float saturation = 1.0f;
    float luminance_floor = 0.0f;
    float luminance_gain = 1.0f;
    float edge_highlight = 0.0f;
    float edge_width = 1.0f;
    float noise_opacity = 0.0f;
    float shadow_alpha = 0.0f;
    float shadow_radius = 0.0f;
    bool backdrop_sampling = false;
    MaterialFallbackPath fallback_path = MaterialFallbackPath::None;
    char const* fallback_reason = "";
    char const* contrast_intent = "standard";
    char const* plan_id = "material.none";
    std::uint32_t debug_seed = 0;
    unsigned int sample_taps = 0;
    MaterialQualityPolicy quality_policy{};
    MaterialPassExpectation primary_pass{};
    MaterialResourceBudget resource_budget{};
    MaterialVerifierExpectation verifier{};

    constexpr bool fallback() const noexcept {
        return fallback_path != MaterialFallbackPath::None;
    }
};

struct MaterialRuntimeRecord {
    MaterialPlan plan{};
    std::uint32_t command_index = 0;
};

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
    style.border = tint;
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

inline std::uint32_t material_debug_seed(MaterialDebugSeed seed,
                                         MaterialKind kind) noexcept {
    auto value = seed.salt ^ (seed.frame * 0x45d9f3bu);
    value ^= seed.node + 0x9e3779b9u + (value << 6u) + (value >> 2u);
    value ^= static_cast<std::uint32_t>(kind) * 0x27d4eb2du;
    return value;
}

inline char const* material_plan_id(MaterialKind kind,
                                    bool backdrop_sampling) noexcept {
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

inline MaterialPlan plan_material_surface(MaterialRequest request,
                                          MaterialEnvironment environment) noexcept {
    MaterialPlan plan{};
    auto const& style = request.style;
    MaterialQualityPolicy resolved_quality = environment.quality;
    resolved_quality.max_blur_radius = std::max(
        0.0f,
        resolved_quality.max_blur_radius);
    resolved_quality.max_sample_taps = std::min(
        resolved_quality.max_sample_taps,
        25u);
    auto const max_blur_radius = std::max(
        0.0f,
        resolved_quality.max_blur_radius);
    plan.kind = style.kind;
    plan.geometry = request.geometry;
    plan.quality_policy = resolved_quality;
    plan.opacity = std::clamp(style.opacity, 0.0f, 1.0f);
    plan.blur_radius = std::clamp(
        style.blur_radius, 0.0f, max_blur_radius);
    plan.tint = style.tint;
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
    plan.sample_taps = resolved_quality.max_sample_taps;
    auto const target_pixels =
        environment.render_target.width > 0
        && environment.render_target.height > 0
            ? static_cast<std::int64_t>(environment.render_target.width)
                * static_cast<std::int64_t>(environment.render_target.height)
            : std::int64_t{0};
    plan.resource_budget.max_blur_radius = max_blur_radius;
    plan.resource_budget.max_sample_taps = plan.sample_taps;
    plan.resource_budget.max_pass_count = 1;
    plan.resource_budget.max_backdrop_pixels = target_pixels;
    plan.resource_budget.bounded_texture_copy = true;
    plan.resource_budget.deterministic_fallback = true;

    bool const has_geometry = request.geometry.w > 0.0f
        && request.geometry.h > 0.0f;
    bool const has_material = style.kind != MaterialKind::None
        && style.tint.a > 0
        && plan.opacity > 0.0f
        && has_geometry;
    bool const target_ready = environment.render_target.width > 0
        && environment.render_target.height > 0;
    bool const quality_allows_backdrop =
        resolved_quality.allow_backdrop_sampling
        && max_blur_radius > 0.0f
        && plan.sample_taps > 0u;
    bool const can_sample_backdrop =
        has_material
        && target_ready
        && quality_allows_backdrop
        && environment.capabilities.material_surfaces
        && environment.capabilities.material_backdrop_blur
        && environment.capabilities.shader_blur
        && environment.capabilities.frame_history
        && environment.backdrop.available
        && environment.backdrop.stable
        && !environment.capabilities.reduce_transparency;

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
    } else if (environment.capabilities.reduce_transparency) {
        plan.fallback_path = MaterialFallbackPath::ReducedTransparency;
        plan.fallback_reason = "reduced transparency disables backdrop sampling";
        plan.blur_radius = 0.0f;
        plan.saturation = 1.0f;
        plan.noise_opacity = 0.0f;
    } else if (!quality_allows_backdrop) {
        plan.fallback_path = MaterialFallbackPath::QualityPolicy;
        plan.fallback_reason = "quality policy disables material backdrop sampling";
        plan.blur_radius = 0.0f;
        plan.saturation = 1.0f;
        plan.noise_opacity = 0.0f;
    } else if (!environment.capabilities.material_surfaces
               || !environment.capabilities.material_backdrop_blur
               || !environment.capabilities.shader_blur) {
        plan.fallback_path = MaterialFallbackPath::UnsupportedBackend;
        plan.fallback_reason = "backend reports no material backdrop blur support";
        plan.blur_radius = 0.0f;
        plan.saturation = 1.0f;
        plan.noise_opacity = 0.0f;
    } else if (!target_ready || !environment.capabilities.frame_history
               || !environment.backdrop.available
               || !environment.backdrop.stable) {
        plan.fallback_path = MaterialFallbackPath::NoBackdropSource;
        plan.fallback_reason = "stable backdrop source is not available for this frame";
        plan.blur_radius = 0.0f;
        plan.saturation = 1.0f;
        plan.noise_opacity = 0.0f;
    }

    plan.backdrop_sampling = can_sample_backdrop;
    if (!plan.backdrop_sampling) {
        plan.sample_taps = 0u;
    }
    if (environment.capabilities.increase_contrast) {
        plan.opacity = std::clamp(plan.opacity + 0.12f, 0.0f, 1.0f);
        plan.luminance_floor = std::clamp(plan.luminance_floor + 0.05f, 0.0f, 1.0f);
        plan.saturation = std::min(plan.saturation, 1.0f);
    }

    plan.plan_id = material_plan_id(style.kind, plan.backdrop_sampling);
    if (has_material && plan.backdrop_sampling && !plan.fallback()) {
        plan.primary_pass = MaterialPassExpectation{
            "backdrop-sample-blur",
            true,
            true,
            plan.sample_taps,
            "material-blur-pass",
        };
    } else if (has_material && plan.fallback_path != MaterialFallbackPath::InvalidGeometry) {
        plan.primary_pass = MaterialPassExpectation{
            "translucent-rounded-rect",
            true,
            false,
            0u,
            "material-fallback-pass",
        };
    } else {
        plan.primary_pass = MaterialPassExpectation{
            "none",
            false,
            false,
            0u,
            "material-fallback-pass",
        };
    }
    plan.verifier.require_backdrop_source = plan.backdrop_sampling;
    plan.verifier.require_edge_highlight = plan.edge_highlight > 0.0f
        && !plan.fallback();
    plan.verifier.min_luma_delta = plan.backdrop_sampling ? 8.0f : 3.0f;
    plan.verifier.min_unique_colors = plan.backdrop_sampling ? 4 : 2;
    plan.verifier.region_name = style.verifier_profile;
    plan.verifier.likely_layer = plan.backdrop_sampling
        ? "material-blur-pass"
        : "material-fallback-pass";
    return plan;
}

} // namespace phenotype
