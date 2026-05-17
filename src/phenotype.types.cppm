module;
#include <cstdint>
#include <cstring>
#include <functional>
#include <string>
#include <string_view>
#include <vector>
export module phenotype.types;

import phenotype.theme_contract;

export namespace phenotype {

// ============================================================
// str — compile-time strlen for string literals, runtime for std::string
// ============================================================

struct str {
    char const* data;
    unsigned int len;

    template<unsigned int N>
    consteval str(char const (&s)[N]) : data(s), len(N - 1) {}
    str(char const* s, unsigned int l) : data(s), len(l) {}
    str(std::string const& s) : data(s.c_str()), len(static_cast<unsigned int>(s.size())) {}
};

// ============================================================
// Color
// ============================================================

struct Color {
    unsigned char r, g, b, a;

    constexpr unsigned int packed() const {
        return (static_cast<unsigned int>(r) << 24) |
               (static_cast<unsigned int>(g) << 16) |
               (static_cast<unsigned int>(b) << 8)  |
               static_cast<unsigned int>(a);
    }

    constexpr bool operator==(Color const&) const = default;
};

// First-class material contract. Backends may render these with native
// backdrop sampling; unsupported backends consume the same material
// command as a translucent rounded-rect fallback.
enum class MaterialKind {
    None,
    Clear,
    Thin,
    Regular,
    Thick,
};

inline char const* material_kind_name(MaterialKind kind) noexcept {
    switch (kind) {
        case MaterialKind::None:    return "none";
        case MaterialKind::Clear:   return "clear";
        case MaterialKind::Thin:    return "thin";
        case MaterialKind::Regular: return "regular";
        case MaterialKind::Thick:   return "thick";
    }
    return "none";
}

// Functional role for a material surface. This is semantic information, not
// backend policy: pure planning and artifacts keep it so Liquid Glass usage can
// be audited as app chrome, navigation, content, or overlay structure.
enum class MaterialSurfaceRole {
    Surface,
    Toolbar,
    Sidebar,
    StatusBar,
    Navigation,
    Content,
    Overlay,
};

inline char const* material_surface_role_name(MaterialSurfaceRole role) noexcept {
    switch (role) {
        case MaterialSurfaceRole::Surface:    return "surface";
        case MaterialSurfaceRole::Toolbar:    return "toolbar";
        case MaterialSurfaceRole::Sidebar:    return "sidebar";
        case MaterialSurfaceRole::StatusBar:  return "status_bar";
        case MaterialSurfaceRole::Navigation: return "navigation";
        case MaterialSurfaceRole::Content:    return "content";
        case MaterialSurfaceRole::Overlay:    return "overlay";
    }
    return "surface";
}

inline MaterialSurfaceRole material_surface_role_from_wire(
        unsigned int raw) noexcept {
    switch (static_cast<MaterialSurfaceRole>(raw)) {
        case MaterialSurfaceRole::Surface:
        case MaterialSurfaceRole::Toolbar:
        case MaterialSurfaceRole::Sidebar:
        case MaterialSurfaceRole::StatusBar:
        case MaterialSurfaceRole::Navigation:
        case MaterialSurfaceRole::Content:
        case MaterialSurfaceRole::Overlay:
            return static_cast<MaterialSurfaceRole>(raw);
    }
    return MaterialSurfaceRole::Surface;
}

// Semantic grouping for material surfaces. This mirrors the platform guidance
// that multiple glass shapes can be coordinated by a container and, when
// appropriate, unioned into a shared optical shape. It is metadata for pure
// planning and artifacts; backend rendering still consumes the resolved plan.
enum class MaterialContainerMode {
    Isolated,
    Container,
    Union,
};

inline char const* material_container_mode_name(
        MaterialContainerMode mode) noexcept {
    switch (mode) {
        case MaterialContainerMode::Isolated:  return "isolated";
        case MaterialContainerMode::Container: return "container";
        case MaterialContainerMode::Union:     return "union";
    }
    return "isolated";
}

struct MaterialContainerDescriptor {
    std::uint32_t container_id = 0;
    std::uint32_t union_id = 0;
    float spacing = 0.0f;
    bool interactive = false;
    bool morph_transitions = false;

    constexpr bool participates() const noexcept {
        return container_id != 0u;
    }

    constexpr MaterialContainerMode mode() const noexcept {
        if (container_id == 0u)
            return MaterialContainerMode::Isolated;
        return union_id != 0u
            ? MaterialContainerMode::Union
            : MaterialContainerMode::Container;
    }

    constexpr bool operator==(MaterialContainerDescriptor const&) const = default;
};

inline unsigned int material_container_flags(
        MaterialContainerDescriptor descriptor) noexcept {
    return (descriptor.interactive ? 1u : 0u)
        | (descriptor.morph_transitions ? 2u : 0u);
}

inline MaterialContainerDescriptor material_container_descriptor_from_wire(
        unsigned int container_id,
        unsigned int union_id,
        float spacing,
        unsigned int flags) noexcept {
    return MaterialContainerDescriptor{
        container_id,
        union_id,
        spacing,
        (flags & 1u) != 0u,
        (flags & 2u) != 0u};
}

struct MaterialStyle {
    MaterialKind kind = MaterialKind::None;
    MaterialSurfaceRole role = MaterialSurfaceRole::Surface;
    MaterialContainerDescriptor container{};
    float opacity = 0.0f;
    float blur_radius = 0.0f;
    Color tint = {0, 0, 0, 0};
    Color border = {0, 0, 0, 0};
    Color foreground = {0, 0, 0, 255};
    Color secondary_foreground = {0, 0, 0, 255};
    Color accent_foreground = {0, 0, 0, 255};
    Color strong_accent_foreground = {0, 0, 0, 255};
    float saturation = 1.0f;
    float luminance_floor = 0.0f;
    float luminance_gain = 1.0f;
    float edge_highlight = 0.0f;
    float edge_width = 1.0f;
    float noise_opacity = 0.0f;
    float shadow_alpha = 0.0f;
    float shadow_radius = 0.0f;
    bool fallback = false;
    char const* fallback_reason = "";
    char const* contrast_intent = "standard";
    char const* plan_id = "material.none";
    char const* verifier_profile = "none";
};

struct MaterialCommandDescriptor {
    MaterialKind kind = MaterialKind::None;
    MaterialSurfaceRole role = MaterialSurfaceRole::Surface;
    MaterialContainerDescriptor container{};
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
};

// A solid convex quadrilateral in canvas-local coordinates. Intended
// for CAD / chart workloads that need to emit thousands of already-
// tessellated fills without paying PathBuilder + FillPath decode cost
// per primitive.
struct PaintQuad {
    float x0, y0;
    float x1, y1;
    float x2, y2;
    float x3, y3;
    Color color;
};

struct PaintRect {
    float x, y;
    float w, h;
    Color color;
};

enum class GradientAxis {
    Horizontal,
    Vertical,
};

inline constexpr unsigned int max_linear_gradient_steps = 64;

inline unsigned int linear_gradient_step_count(unsigned int steps) noexcept {
    if (steps == 0)
        return 1;
    return steps > max_linear_gradient_steps
        ? max_linear_gradient_steps
        : steps;
}

inline GradientAxis gradient_axis_from_wire(unsigned int raw) noexcept {
    switch (static_cast<GradientAxis>(raw)) {
        case GradientAxis::Horizontal:
            return GradientAxis::Horizontal;
        case GradientAxis::Vertical:
        default:
            return GradientAxis::Vertical;
    }
}

inline Color lerp_color(Color from, Color to, float t) noexcept {
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    auto lerp_channel = [t](unsigned char a, unsigned char b) {
        auto const af = static_cast<float>(a);
        auto const bf = static_cast<float>(b);
        return static_cast<unsigned char>(af + (bf - af) * t + 0.5f);
    };
    return {
        lerp_channel(from.r, to.r),
        lerp_channel(from.g, to.g),
        lerp_channel(from.b, to.b),
        lerp_channel(from.a, to.a),
    };
}

// ============================================================
// Theme — design tokens (matches exon docs CSS variables)
// ============================================================

struct Theme {
    // Color tokens
    Color background    = {242, 242, 247, 255}; // #f2f2f7
    Color foreground    = { 28,  28,  30, 255}; // #1c1c1e
    Color accent        = {  0, 122, 255, 255}; // #007aff
    Color accent_strong = {  0,  90, 190, 255}; // #005abe
    Color muted         = { 99,  99, 102, 255}; // #636366
    Color border        = {209, 209, 214, 255}; // #d1d1d6
    Color surface       = {255, 255, 255, 238}; // #ffffffee
    Color code_bg       = {229, 229, 234, 255}; // #e5e5ea
    Color hero_bg       = {242, 242, 247, 255}; // #f2f2f7
    Color hero_fg       = { 28,  28,  30, 255}; // #1c1c1e
    Color hero_muted    = { 99,  99, 102, 255}; // #636366
    Color transparent   = {  0,   0,   0,   0};

    // Typography
    std::string default_font_family = "Pretendard";
    float body_font_size     = 16.0f;
    float heading_font_size  = 22.4f;  // 1.4rem
    float hero_title_size    = 40.0f;  // 2.5rem
    float hero_subtitle_size = 18.4f;  // 1.15rem
    float code_font_size     = 14.4f;  // 0.9em
    float small_font_size    = 14.4f;  // 0.9rem
    float line_height_ratio  = 1.6f;

    // Layout
    float max_content_width  = 720.0f;

    // Box visual size for widget::checkbox / widget::radio. 16 dp default
    // matches the desktop look; touch-first hosts (Android) bump this to
    // 44 dp at app startup via phenotype::set_theme.
    float toggle_box_size    = 16.0f;

    // Radius scale for Apple-like glass chrome.
    float radius_xs   = 6.0f;
    float radius_sm   = 10.0f;
    float radius_md   = 14.0f;
    float radius_lg   = 22.0f;
    float radius_full = 9999.0f;

    // Spacing scale
    float space_xs  = 4.0f;
    float space_sm  = 8.0f;
    float space_md  = 12.0f;
    float space_lg  = 16.0f;
    float space_xl  = 24.0f;
    float space_2xl = 32.0f;
    float space_3xl = 48.0f;

    // State tokens
    Color state_hover_bg          = {229, 229, 234, 255};
    Color state_hover_fg          = { 28,  28,  30, 255}; // = foreground
    Color state_active_bg         = {  0,  90, 190, 255}; // = accent_strong
    Color state_active_fg         = {255, 255, 255, 255};
    Color state_disabled_bg       = {242, 242, 247, 255};
    Color state_disabled_fg       = {142, 142, 147, 255};
    Color state_disabled_border   = {209, 209, 214, 255}; // = border
    Color state_error_bg          = {254, 242, 242, 255};
    Color state_error_fg          = {185,  28,  28, 255};
    Color state_error_border      = {220,  38,  38, 255};
    Color state_focus_ring        = {  0, 122, 255, 255}; // = accent
    float state_focus_ring_width  = 2.0f;

    // Semantic colors reserved for downstream status widgets.
    Color semantic_success_bg     = {220, 252, 231, 255};
    Color semantic_success_fg     = { 22, 101,  52, 255};
    Color semantic_success_border = { 34, 197,  94, 255};
    Color semantic_warning_bg     = {254, 243, 199, 255};
    Color semantic_warning_fg     = {146,  64,  14, 255};
    Color semantic_warning_border = {245, 158,  11, 255};
    Color semantic_info_bg        = {219, 234, 254, 255};
    Color semantic_info_fg        = { 30,  64, 175, 255};
    Color semantic_info_border    = { 59, 130, 246, 255};
    Color semantic_error_bg       = {254, 242, 242, 255}; // = state_error_bg
    Color semantic_error_fg       = {185,  28,  28, 255}; // = state_error_fg
    Color semantic_error_border   = {220,  38,  38, 255}; // = state_error_border
};

inline auto default_theme_profile_name() noexcept -> std::string_view {
    return theme_contract::default_theme_profile_name();
}

inline auto default_theme_reference() noexcept -> std::string_view {
    return theme_contract::default_theme_reference();
}

inline auto default_theme_font_policy() noexcept -> std::string_view {
    return theme_contract::default_theme_font_policy();
}

inline auto default_theme_material_policy() noexcept -> std::string_view {
    return theme_contract::default_theme_material_policy();
}

inline auto default_theme_iconography_policy() noexcept -> std::string_view {
    return theme_contract::default_theme_iconography_policy();
}

inline auto default_theme_icon_asset_policy() noexcept -> std::string_view {
    return theme_contract::default_theme_icon_asset_policy();
}

inline auto default_theme_usage_policy() noexcept -> std::string_view {
    return theme_contract::default_theme_usage_policy();
}

inline auto default_theme_container_policy() noexcept -> std::string_view {
    return theme_contract::default_theme_container_policy();
}

inline auto default_theme_performance_policy() noexcept -> std::string_view {
    return theme_contract::default_theme_performance_policy();
}

inline auto default_theme_accessibility_policy() noexcept -> std::string_view {
    return theme_contract::default_theme_accessibility_policy();
}

inline auto default_theme_fallback_policy() noexcept -> std::string_view {
    return theme_contract::default_theme_fallback_policy();
}

inline bool theme_matches_default_glass_contract(Theme const& theme) {
    auto const contract = theme_contract::default_glass_theme_contract();
    return theme.default_font_family == "Pretendard"
        && theme.background == Color{contract.background.r,
                                     contract.background.g,
                                     contract.background.b,
                                     contract.background.a}
        && theme.foreground == Color{contract.foreground.r,
                                     contract.foreground.g,
                                     contract.foreground.b,
                                     contract.foreground.a}
        && theme.accent == Color{contract.accent.r,
                                 contract.accent.g,
                                 contract.accent.b,
                                 contract.accent.a}
        && theme.surface == Color{contract.surface.r,
                                  contract.surface.g,
                                  contract.surface.b,
                                  contract.surface.a}
        && theme.radius_sm == contract.radius.sm
        && theme.radius_md == contract.radius.md
        && theme.radius_lg == contract.radius.lg
        && theme.body_font_size == contract.typography.body_font_size
        && theme.small_font_size == contract.typography.small_font_size
        && theme.line_height_ratio == contract.typography.line_height_ratio
        && theme.state_focus_ring == theme.accent;
}

// ============================================================
// Draw command opcodes — shared by paint emitters and JS shim
// ============================================================

enum class Cmd : unsigned int {
    Clear      = 1,
    FillRect   = 2,
    StrokeRect = 3,
    RoundRect  = 4,
    DrawText   = 5,
    DrawLine   = 6,
    HitRegion  = 7,
    DrawImage  = 8,
    // Scissor restricts subsequent draw commands to the given rect
    // (backend-native primitive: Metal setScissorRect, D3D12
    // RSSetScissorRects, Vulkan vkCmdSetScissor, WebGPU
    // setScissorRect). Emitted around a "dirty root" subtree — one
    // whose own layout_valid is false but whose parent already
    // blitted cached bytes — so the GPU rasterises only the part of
    // the viewport that actually changed this frame. A zero-sized
    // payload (w == 0 && h == 0) resets to the full viewport. Nesting
    // is not supported across the four target APIs; emit Scissor
    // resets, not overlapping Scissor regions.
    Scissor    = 9,
    // Stroked arc segment. Drawn pixel-perfectly via SDF in the
    // backend (no parse-time chord tessellation by the consumer).
    // `start_angle` / `end_angle` are radians, CCW per the AutoCAD
    // / mathematical convention; pass `start_angle = 0`,
    // `end_angle = 2π` for a full circle.
    DrawArc    = 10,
    // Variable-length stroked path — a sequence of `PathVerb`s
    // (MoveTo, LineTo, QuadTo, CubicTo, ArcTo, Close). Curves are
    // CPU-flattened by the backend and re-dispatched onto the
    // existing line / arc instance buffers (no new pipeline). Layout:
    // opcode + thickness (f32) + packed RGBA + verb_count (u32) +
    // verb_count × (verb_tag + N × f32 inline).
    Path       = 11,
    // Variable-length filled path — same verb stream, no thickness.
    // Tessellated to a triangle list on the CPU and dispatched onto
    // the fill_tri pipeline. Layout: opcode + packed RGBA + verb_count
    // + inline verbs.
    FillPath   = 12,
    // Batched solid convex quadrilaterals. Layout: opcode +
    // quad_count + quad_count × (packed RGBA + 8×f32 points).
    // Backends expand each quad directly to two triangles or their
    // platform's equivalent fill primitive, bypassing Path / FillPath
    // flattening and ear clipping.
    FillQuads  = 13,
    // Batched axis-aligned rectangles. Layout: opcode + rect_count +
    // rect_count × (x, y, w, h, packed RGBA). Backends may route this
    // to an instanced rectangle path or expand it to triangles when
    // that is needed to preserve fill ordering.
    FillRects  = 14,
    // Material surface. Layout: opcode + x/y/w/h/radius f32 +
    // kind u32 + role u32 + opacity f32 + blur_radius f32 + packed
    // RGBA tint + saturation/luminance_floor/luminance_gain/
    // edge_highlight/edge_width/noise_opacity/shadow_alpha/
    // shadow_radius f32 + container_id/union_id u32 +
    // container_spacing f32 + container_flags u32.
    // Backends with backdrop sampling render a glass/material effect;
    // backends without it draw the tint as a rounded-rect fallback.
    MaterialRect = 15,
    // Bounded axis-aligned linear gradient. Layout: opcode +
    // x/y/w/h f32 + packed RGBA from/to + axis u32 + steps u32.
    // Backends may use a native gradient path later; today every
    // renderer lowers through the same bounded strip contract.
    LinearGradientRect = 16,
};

// Verb tags inside a Cmd::Path / Cmd::FillPath payload. Numeric values
// are part of the wire format — never reorder existing entries; append
// only.
enum class PathVerb : unsigned int {
    MoveTo  = 0,  // 2 f32: x, y
    LineTo  = 1,  // 2 f32: x, y
    QuadTo  = 2,  // 4 f32: cx, cy, x, y     (quadratic Bézier control + endpoint)
    CubicTo = 3,  // 6 f32: c1x, c1y, c2x, c2y, x, y
    ArcTo   = 4,  // 5 f32: cx, cy, radius, start_angle, end_angle (CCW math/AutoCAD)
    Close   = 5,  // 0 f32 — closes the current subpath with a straight segment.
};

// PathBuilder — accumulates verbs into a packed buffer (tag + inline
// f32-as-u32 words) so emit can memcpy the buffer directly into the
// command stream. Lives next to `Painter` because `Painter::stroke_path`
// takes it by reference.
//
// Coordinate convention: canvas-local pixels for points, CCW math /
// AutoCAD radians for `arc_to` angles. `close()` synthesises a
// straight segment to the current subpath's start when the backend
// strokes / fills it.
struct PathBuilder {
    // Packed verb stream: each verb is `[tag u32]` followed by N
    // f32-as-u32 words (N depends on the tag). One word per push,
    // appended in order — the same byte stream a backend will see in
    // `Cmd::Path` / `Cmd::FillPath` payload.
    std::vector<unsigned int> verbs;
    unsigned int verb_count = 0;

    void clear() { verbs.clear(); verb_count = 0; }

    void move_to(float x, float y) {
        push_verb(PathVerb::MoveTo);
        push_f32(x); push_f32(y);
    }
    void line_to(float x, float y) {
        push_verb(PathVerb::LineTo);
        push_f32(x); push_f32(y);
    }
    void quad_to(float cx, float cy, float x, float y) {
        push_verb(PathVerb::QuadTo);
        push_f32(cx); push_f32(cy);
        push_f32(x); push_f32(y);
    }
    void cubic_to(float c1x, float c1y, float c2x, float c2y,
                  float x, float y) {
        push_verb(PathVerb::CubicTo);
        push_f32(c1x); push_f32(c1y);
        push_f32(c2x); push_f32(c2y);
        push_f32(x); push_f32(y);
    }
    // Arc segment in centre form. `start_angle` to `end_angle` are
    // radians, CCW per the math/AutoCAD convention (matches Painter::arc
    // and Cmd::DrawArc).
    void arc_to(float cx, float cy, float radius,
                float start_angle, float end_angle) {
        push_verb(PathVerb::ArcTo);
        push_f32(cx); push_f32(cy);
        push_f32(radius);
        push_f32(start_angle); push_f32(end_angle);
    }
    void close() {
        push_verb(PathVerb::Close);
    }

    bool empty() const { return verb_count == 0; }

    // Returns a copy with every coordinate word offset by `(dx, dy)`.
    // Magnitude / angle words (radius, start_angle, end_angle on
    // ArcTo) pass through unchanged. Used by `Painter::stroke_path`
    // adapters to convert canvas-local verb streams into surface-local
    // ones at emit time, mirroring how `line()` / `arc()` translate
    // their absolute coords with `origin_x / origin_y`. An unknown
    // verb tag terminates the walk and the rest of the buffer is
    // copied through verbatim.
    PathBuilder translated(float dx, float dy) const {
        PathBuilder out;
        out.verb_count = verb_count;
        out.verbs.reserve(verbs.size());

        unsigned int i = 0;
        auto translate_xy = [&]() {
            float x, y;
            unsigned int xb = verbs[i], yb = verbs[i + 1];
            std::memcpy(&x, &xb, 4);
            std::memcpy(&y, &yb, 4);
            x += dx; y += dy;
            std::memcpy(&xb, &x, 4);
            std::memcpy(&yb, &y, 4);
            out.verbs.push_back(xb);
            out.verbs.push_back(yb);
            i += 2;
        };
        auto passthrough = [&](unsigned int n) {
            for (unsigned int k = 0; k < n; ++k) {
                out.verbs.push_back(verbs[i]);
                ++i;
            }
        };

        while (i < verbs.size()) {
            unsigned int tag = verbs[i++];
            out.verbs.push_back(tag);
            switch (static_cast<PathVerb>(tag)) {
            case PathVerb::MoveTo:
            case PathVerb::LineTo:
                translate_xy();
                break;
            case PathVerb::QuadTo:
                translate_xy();
                translate_xy();
                break;
            case PathVerb::CubicTo:
                translate_xy();
                translate_xy();
                translate_xy();
                break;
            case PathVerb::ArcTo:
                translate_xy();
                passthrough(3);  // radius, start_angle, end_angle
                break;
            case PathVerb::Close:
                break;
            default:
                while (i < verbs.size())
                    out.verbs.push_back(verbs[i++]);
                return out;
            }
        }
        return out;
    }

private:
    void push_verb(PathVerb v) {
        verbs.push_back(static_cast<unsigned int>(v));
        ++verb_count;
    }
    void push_f32(float v) {
        unsigned int bits;
        std::memcpy(&bits, &v, 4);
        verbs.push_back(bits);
    }
};

// ============================================================
// NodeHandle — generational handle to a LayoutNode in the Arena
// ============================================================
//
// Replaces raw `LayoutNode*` everywhere a node identity outlives the
// alloc-and-build pass. Each handle stores (index, generation); the
// arena bumps `current_gen` on every reset(), so handles from a prior
// rebuild fail validation at deref time instead of returning a
// freshly-recycled but unrelated node. Pattern from Rust's slotmap /
// id_arena / generational_arena crates.

struct NodeHandle {
    std::uint32_t index = 0xFFFFFFFFu;
    std::uint32_t generation = 0xFFFFFFFFu;

    constexpr bool valid() const noexcept { return index != 0xFFFFFFFFu; }
    constexpr bool operator==(NodeHandle const&) const noexcept = default;

    static constexpr NodeHandle null() noexcept { return {}; }
};

// ============================================================
// Layout tree
// ============================================================

enum class FlexDirection { Column, Row };

enum class MainAxisAlignment {
    Start,
    Center,
    End,
    SpaceBetween,
};

enum class CrossAxisAlignment {
    Start,
    Center,
    End,
    Stretch,
};

enum class TextAlign {
    Start,
    Center,
    End,
};

// Decoration — small visual glyph drawn on top of a node's background.
// Used by widget::checkbox and widget::radio to render the active state
// (a white "V" checkmark or a small white inner circle, respectively)
// without needing a font glyph or a new draw-command opcode.
enum class Decoration {
    None,
    Check,
    Dot,
};

enum class InteractionRole {
    None,
    Link,
    Button,
    Checkbox,
    Radio,
    TextField,
    Command,
};

// Button visual variant.
//   Default — code_bg / surface chrome with hover_bg fallback.
//   Primary — accent-filled with white text; hover darkens to accent_strong.
enum class ButtonVariant {
    Default,
    Primary,
};

struct ButtonStyleOptions {
    ButtonVariant variant = ButtonVariant::Default;
    bool disabled = false;
    bool has_background = false;
    Color background = {};
    bool has_hover_background = false;
    Color hover_background = {};
    bool has_border_color = false;
    Color border_color = {};
    bool has_text_color = false;
    Color text_color = {};
    float border_width = -1.0f;
    float border_radius = -1.0f;
    float font_size = 0.0f;
    float padding_top = -1.0f;
    float padding_right = -1.0f;
    float padding_bottom = -1.0f;
    float padding_left = -1.0f;
    float max_width = 0.0f;
    float fixed_height = -1.0f;
    TextAlign text_align = TextAlign::Start;
};

struct KeyCommandOptions {
    bool disabled = false;
    bool allow_when_input_focused = false;
    std::string debug_label;
};

// Text size variant.
// Picks one of the typography scalars on Theme. `Code` additionally
// applies the code-block chrome (mono font, code_bg background,
// 1px border, radius_md, space_lg padding).
enum class TextSize {
    Body,
    Heading,
    Small,
    Code,
    HeroTitle,
    HeroSubtitle,
};

// Text color variant.
// Picks one of the foreground / muted / accent / hero color tokens.
enum class TextColor {
    Default,
    Muted,
    Accent,
    HeroFg,
    HeroMuted,
};

// Spacing-scale token resolved against the active Theme by
// layout::space_value().
enum class SpaceToken {
    Xs,
    Sm,
    Md,
    Lg,
    Xl,
    Xl2,
};

inline constexpr char const* interaction_role_name(InteractionRole role) noexcept {
    switch (role) {
        case InteractionRole::Link:      return "link";
        case InteractionRole::Button:    return "button";
        case InteractionRole::Checkbox:  return "checkbox";
        case InteractionRole::Radio:     return "radio";
        case InteractionRole::TextField: return "text_field";
        case InteractionRole::Command:   return "command";
        default:                         return "none";
    }
}

struct Style {
    FlexDirection flex_direction = FlexDirection::Column;
    MainAxisAlignment main_align = MainAxisAlignment::Start;
    CrossAxisAlignment cross_align = CrossAxisAlignment::Start;
    TextAlign text_align = TextAlign::Start;
    float gap = 0;
    float padding[4] = {}; // top, right, bottom, left
    float max_width = 0;   // 0 = no limit
    float fixed_height = -1;
    // Proportional share of remaining main-axis space inside a flex
    // parent. 0 (the default) opts out — the child takes its intrinsic
    // / max_width size and the parent's existing implicit "last
    // unspecified child fills the rest" rule still applies. When any
    // sibling has flex_grow > 0, those children split the remaining
    // space in proportion to their values and the implicit fallback is
    // suppressed. Currently honoured by row layout only; column flex
    // distribution waits on a bounded-height story.
    float flex_grow = 0;
};

// GestureEvent — pan / pinch / scroll-zoom dispatched to a
// `widget::canvas` whose builder passed an `on_gesture` callback.
// Platform backends translate native input (macOS magnifyWithEvent +
// scrollWheel, Android multi-pointer MotionEvents) to this neutral
// shape; the shell delivers it in canvas-local coordinates so app
// code never has to know the canvas's outer (x, y) on screen.
//
//   * Pan        — `dx` / `dy` are pixel deltas since the previous
//                  event. `pinch_scale` stays 1.0.
//   * Pinch      — `pinch_scale` is the multiplicative zoom delta
//                  since the previous event (1.0 ≡ no change).
//                  `focus_x` / `focus_y` are canvas-local coords of
//                  the midpoint between fingers.
//   * ScrollZoom — modifier-augmented scroll (macOS Cmd+scroll).
//                  `pinch_scale` is the platform-derived multiplier
//                  so simple consumers can treat Pinch + ScrollZoom
//                  uniformly. `dy` carries the raw wheel delta for
//                  consumers that want to roll their own response
//                  curve. `focus_x` / `focus_y` mark the cursor.
//
// A gesture whose focal point falls outside the canvas bounds is
// dropped by the shell — apps don't see fragmented streams.
enum class GestureKind : int {
    Pan        = 0,
    Pinch      = 1,
    ScrollZoom = 2,
};

struct GestureEvent {
    GestureKind kind = GestureKind::Pan;
    float       dx = 0.0f;
    float       dy = 0.0f;
    float       pinch_scale = 1.0f;
    float       focus_x = 0.0f;
    float       focus_y = 0.0f;
};

// Font specification for `Painter::text` and `Painter::measure_text`.
// `family` is a string view: empty (or "system") means the backend's
// platform default font; non-empty is a TTF/OTF family name (e.g.
// "Arial", "Helvetica Neue"). Backends that cannot resolve the name
// fall back to their default family silently — matching the platform
// font stack (DWrite IDWriteFontFallback, Android Typeface.create,
// Core Text best-match). `mono` selects a monospace default family
// when `family` is empty (Consolas / SF Mono / Typeface.MONOSPACE).
//
// The default-constructed `FontSpec{}` reproduces the pre-FontSpec
// behaviour: system sans, Regular, Upright — so existing callers that
// pass `Painter::text(...)` with no font argument keep rendering the
// same glyphs they always did.
enum class FontWeight : unsigned char { Regular = 0, Bold = 1 };
enum class FontStyle  : unsigned char { Upright = 0, Italic = 1 };

struct FontSpec {
    std::string_view family{};
    FontWeight       weight = FontWeight::Regular;
    FontStyle        style  = FontStyle::Upright;
    bool             mono   = false;
    // Horizontal glyph width multiplier. 1.0 = the font's native
    // advance; values > 1 stretch each glyph along the run's local
    // X axis (so a single character is visibly wider AND the run
    // takes more space), values < 1 condense them. AutoCAD's
    // STYLE/TEXT/ATTRIB DXF-41 width factor is the immediate
    // motivating use — `blocks_and_tables_-_imperial.dwg` saves an
    // ATTRIB stretching "ADDA" 6.54× across the title-block column.
    // The macOS backend applies it via the Core Text font matrix
    // so glyphs draw stretched in place; Windows / Android consume
    // the wire field but render axis-natural until their respective
    // backend slabs land. `measure_text` multiplies the host-
    // measured width by this factor, so layout cursors and bounding
    // boxes stay consistent with the rendered glyphs.
    float            width_factor = 1.0f;
};

// Vertical metrics for a `FontSpec` at a given `font_size`, in canvas
// pixels. `ascent` is the distance from the baseline up to the top of
// the font's design ascender; `descent` is the distance down from the
// baseline to the bottom of the descender (positive value); `leading`
// is the platform-recommended extra space between consecutive lines
// (often 0 on CoreText). `cap_height` is the distance from the baseline
// up to the visible top of the font's capital glyphs (typically smaller
// than `ascent`, which also covers diacritic clearance and the design
// ascender area above the cap). Sum `ascent + descent + leading` to
// obtain the canonical natural line height for the face at this size.
//
// CAD viewers and custom-layout widgets need this to anchor text by
// baseline / cap-top rather than by the Painter::text's font-box top
// (which is `(x, y)` but corresponds to ascent above the baseline, so
// the visible glyph cap-top sits at `y + (ascent - cap_height)` in
// practice). Painter consumers that mix bundled / system fonts of
// differing ascender heights (e.g. cad++'s Architects Daughter vs
// AutoCAD-shipped `cityb___.ttf`) read these metrics to keep glyph
// baselines on the entity's geometric anchor regardless of which face
// resolved. `cap_height` additionally lets CAD-style consumers
// cap-height-lock the rendered glyph size so a host font_size derived
// from `autocad_height / (cap_height / font_size)` produces a visible
// capital that matches AutoCAD's `STYLE.height` regardless of the
// substitute face's design proportions.
//
// `width_factor` does not affect any of these values — they are pure
// vertical metrics. All four values are 0 when the backend cannot
// resolve the face (or has not implemented the metric query yet, e.g.
// the Windows / Android stubs as of this PR) — the caller should fall
// back to a font-size-based heuristic in that case.
struct FontMetrics {
    float ascent     = 0.0f;
    float descent    = 0.0f;
    float leading    = 0.0f;
    float cap_height = 0.0f;
};

// Painter — immediate-mode drawing surface handed to a `widget::canvas`
// callback during the paint pass. Coordinates are local to the canvas
// (origin at the canvas's top-left, x extends right, y extends down).
// Used by apps that need to render arbitrary 2D geometry — CAD plans,
// charts, custom widgets — that doesn't fit the layout-tree model.
class Painter {
public:
    virtual ~Painter() = default;
    virtual void line(float x1, float y1, float x2, float y2,
                      float thickness, Color color) = 0;
    // Text — `(x, y)` is the top-left of the text's font-size box, in
    // the canvas's local coordinate system. Mirrors the existing text
    // path used by widget::text. `font_size` is in pixels. `font`
    // selects family / weight / italic / mono; default-constructed is
    // the backend's platform sans Regular Upright. `rotation` is in
    // radians, CCW about the canvas's +Z axis (y-down screen, so a
    // positive angle rotates text counter-clockwise on screen). The
    // pivot is `(x, y)` — the top-left of the font-size box — so
    // each glyph's quad rotates around the run's anchor as a rigid
    // body. Default 0.0f preserves the pre-rotation behaviour. Only
    // the macOS backend renders rotation today; Windows / Android
    // round-trip the wire field but draw axis-aligned until their
    // respective backend slabs land.
    virtual void text(float x, float y,
                      char const* str, unsigned int len,
                      float font_size, Color color,
                      FontSpec font = {},
                      float rotation = 0.0f) = 0;
    // Pixel width of `[str, str+len)` rendered at `font_size` with
    // `font`. Independent of `rotation` — callers needing a rotated
    // bbox compose their own corners after rotation. Returns 0 when
    // the canvas's host has no measurement capability.
    virtual float measure_text(char const* str, unsigned int len,
                               float font_size,
                               FontSpec font = {}) const = 0;
    // Vertical metrics for `font` at `font_size`. Returns a zero
    // `FontMetrics{}` when the backend cannot resolve the face — the
    // caller should fall back to a `font_size`-based heuristic in that
    // case. See `FontMetrics` for the per-field semantics. Hosts that
    // can't measure (some embedded surfaces, the stub backend) return
    // zero; callers are expected to handle that the same way they
    // already handle `measure_text` returning 0.
    virtual FontMetrics font_metrics(float font_size,
                                     FontSpec font = {}) const = 0;
    // Arc — stroked circular arc centred at `(cx, cy)` with the given
    // `radius`, swept from `start_angle` to `end_angle` (radians, CCW
    // per the math / AutoCAD convention; positive y goes down on the
    // canvas, but the angles still measure mathematically). For a
    // full circle pass `start_angle = 0, end_angle = 2π`. Backends
    // rasterise via a fragment-shader SDF so zoom-in stays smooth at
    // any scale — no parse-time chord tessellation required by the
    // consumer.
    virtual void arc(float cx, float cy, float radius,
                     float start_angle, float end_angle,
                     float thickness, Color color) = 0;
    // Stroked path — a sequence of `PathVerb`s expressed via a
    // `PathBuilder`. The path's coordinates are canvas-local; the
    // adapter translates them to surface-local before emitting the
    // wire-format `Cmd::Path` payload. Backends CPU-flatten curve
    // segments into the existing line / arc instance buffers so no
    // new pipeline is required for stroke rendering.
    virtual void stroke_path(PathBuilder const& path,
                             float thickness, Color color) = 0;
    // Filled path — same verb stream, no thickness. The adapter
    // emits a `Cmd::FillPath` payload; backends flatten the verbs
    // into a polygon, ear-clip it into a triangle list, then
    // rasterise each triangle into horizontal axis-aligned strips
    // dispatched onto the existing colour pipeline (no new shader,
    // no new pipeline). Single closed loop only — self-intersection
    // and multi-loop / hole semantics are out of scope for now.
    virtual void fill_path(PathBuilder const& path, Color color) = 0;
    // Batched convex quad fills. Default no-op preserves source
    // compatibility for external Painter derivations that only support
    // the older primitive set.
    virtual void fill_quads(PaintQuad const* quads, unsigned int count) {
        (void)quads; (void)count;
    }
    virtual void fill_rects(PaintRect const* rects, unsigned int count) {
        (void)rects; (void)count;
    }
    // Bounded linear-gradient helper. External Painter derivations get
    // a deterministic `fill_rects` fallback; phenotype's own canvas
    // adapter overrides this with the first-class wire command.
    virtual void linear_gradient_rect(float x, float y, float w, float h,
                                      Color from, Color to,
                                      GradientAxis axis = GradientAxis::Vertical,
                                      unsigned int steps = 24) {
        if (w == 0.0f || h == 0.0f)
            return;
        if (w < 0.0f) { x += w; w = -w; }
        if (h < 0.0f) { y += h; h = -h; }

        unsigned int const count = linear_gradient_step_count(steps);

        PaintRect rects[max_linear_gradient_steps]{};
        for (unsigned int i = 0; i < count; ++i) {
            float const t = count == 1
                ? 0.0f
                : static_cast<float>(i) / static_cast<float>(count - 1);
            auto const color = lerp_color(from, to, t);
            if (axis == GradientAxis::Horizontal) {
                auto const x0 = x + w * static_cast<float>(i)
                    / static_cast<float>(count);
                auto const x1 = x + w * static_cast<float>(i + 1)
                    / static_cast<float>(count);
                rects[i] = PaintRect{x0, y, x1 - x0, h, color};
            } else {
                auto const y0 = y + h * static_cast<float>(i)
                    / static_cast<float>(count);
                auto const y1 = y + h * static_cast<float>(i + 1)
                    / static_cast<float>(count);
                rects[i] = PaintRect{x, y0, w, y1 - y0, color};
            }
        }
        fill_rects(rects, count);
    }
    // Push / pop a rectangular clip region. The rect is canvas-local
    // (same coordinate system as `line` / `arc` / `stroke_path`); the
    // adapter intersects it with the current clip and applies the
    // result to both the CPU-side line / text cull path and the
    // backend's `Cmd::Scissor` opcode. Pop restores the previous clip
    // (the canvas's outer rect at the bottom of the stack). Default
    // implementation is no-op for derivations that do not care about
    // sub-canvas clipping; the widget::canvas adapter's `PainterImpl`
    // overrides both. Used by cad++ to clip per-VIEWPORT model-space
    // walks to their paper-space rect.
    virtual void push_clip(float x, float y, float w, float h) {
        (void)x; (void)y; (void)w; (void)h;
    }
    virtual void pop_clip() {}
};

// ScrollState — `layout::scroll_view`'s persistent state, kept in the
// framework_local store keyed by the scroll_view's call site. Wheel
// dispatch writes `offset_y` (clamped to [0, content_height -
// container_height]); paint reads it to translate children. Storing
// pointers from the framework_local store directly on LayoutNode is
// safe within one frame — the heap-allocated payload outlives prune
// for any widget visited that frame, and view re-runs on every frame
// so the ptr is freshly resolved before paint reads it.
struct ScrollState {
    float offset_x = 0;
    float offset_y = 0;
};

struct LayoutNode {
    // Style
    Style style;
    Color background = {0, 0, 0, 0};
    Color text_color = {26, 26, 26, 255};
    float font_size = 16.0f;
    bool mono = false;
    float border_radius = 0;
    Color border_color = {0, 0, 0, 0};
    float border_width = 0;
    Decoration decoration = Decoration::None; // checkmark / inner dot glyph
    MaterialStyle material;

    // Content
    std::string text;
    std::string url;
    std::string image_url;            // non-empty → image leaf
    unsigned int callback_id = 0xFFFFFFFF;
    unsigned int cursor_type = 0; // 0=default, 1=pointer
    bool focusable = true;        // false = clickable but skipped by Tab + no focus ring
    InteractionRole interaction_role = InteractionRole::None;
    std::string debug_semantic_role;
    std::string debug_semantic_label;
    unsigned int debug_semantic_callback_id = 0xFFFFFFFFu;
    bool debug_semantic_hidden = false;
    bool debug_semantic_focusable = false;
    bool debug_semantic_enabled = true;

    // Hover styles (alpha=0 means no hover override)
    Color hover_background = {0, 0, 0, 0};
    Color hover_text_color = {0, 0, 0, 0};

    // Input field
    bool is_input = false;
    std::string placeholder;

    // Grid container — set by layout::grid. When `is_grid_container` is
    // true, the layout pass walks `children` in row-major order and
    // assigns each child a fixed cell from the `grid_columns` track list
    // and `grid_row_height` (0 → derive height from content per row).
    // `style.gap` doubles as both column-gap and row-gap.
    bool is_grid_container = false;
    std::vector<float> grid_columns;
    float grid_row_height = 0;

    // Immediate-mode paint hook — set by widget::canvas. When non-null,
    // the paint pass invokes paint_fn with a Painter bound to the
    // node's resolved (x, y) and the active scroll. layout-tree
    // children are still painted normally afterwards.
    std::function<void(Painter&)> paint_fn;

    // Index into `g_app.gesture_callbacks` — set by widget::canvas when
    // the consumer passes an `on_gesture` lambda. 0xFFFFFFFFu = no
    // gesture handler on this node. Platform input backends look up
    // the active canvas's bounds via this id at dispatch time.
    unsigned int gesture_callback_id = 0xFFFFFFFFu;

    // Scroll container — set by `layout::scroll_view`. Children lay out
    // at the container's inner width (no shrink-to-fit) but their total
    // height is recorded in `content_height` rather than expanding the
    // node, so `style.fixed_height` defines a viewport that clips the
    // overflow. Paint translates children by `scroll_offset_y` and
    // emits a Scissor for the viewport rect; wheel dispatch routes
    // deltas into `scroll_state` (a pointer into framework_local) and
    // clamps to [0, content_height - height].
    bool         is_scroll_container = false;
    float        content_height      = 0.0f;
    float        scroll_offset_y     = 0.0f;
    ScrollState* scroll_state        = nullptr;

    // Computed layout
    float x = 0, y = 0, width = 0, height = 0;
    bool layout_valid = false; // true = copied from prev frame by diff, layout_node skips

    // Cached text layout (filled during layout pass, reused in paint)
    std::vector<std::string> text_lines;

    // Paint-cache state (subtree byte range in the previous cmd buffer).
    // Populated by paint_node when it emits this subtree; copied by
    // diff_and_copy_layout on match. When layout_valid and ambient paint
    // inputs (absolute position, scroll_y, hover/focus) are unchanged,
    // paint_node blits [paint_offset..paint_offset+paint_length) from
    // g_app.prev_cmd_buf instead of re-walking this subtree.
    //
    // paint_callback_mask is a 64-bit bloom over the subtree's
    // callback_ids (1ULL << (callback_id & 63)) so the blit guard can
    // ask "does this subtree contain the hovered/focused id?" in O(1)
    // without storing a full child-id vector per node.
    //
    // paint_dynamic marks a subtree containing widget::canvas paint_fn.
    // Those callbacks are opaque app code and may capture changing state
    // without changing layout props, so the subtree must be repainted —
    // unless the canvas opts in via paint_token (see below) and its
    // token matches the value recorded last frame.
    std::uint32_t paint_offset = 0;
    std::uint32_t paint_length = 0;
    float         paint_ax = 0.0f;
    float         paint_ay = 0.0f;
    std::uint64_t paint_callback_mask = 0;
    bool          paint_dynamic = false;
    bool          paint_valid = false;

    // Optional dirty-token cache for widget::canvas paint_fn nodes.
    // Caller-supplied uint64; sentinel 0 = always-dirty (preserves the
    // pre-token behaviour for every existing call site). When
    // `paint_token != 0 && paint_token == paint_token_prev` after the
    // layout diff, paint_node treats the canvas subtree as cache-eligible
    // exactly like a static subtree: blits the prev_cmd_buf byte range
    // and skips the paint_fn callback entirely. paint_token_prev is
    // populated by diff_and_copy_layout from the matched old node's
    // recorded paint_token.
    std::uint64_t paint_token      = 0;
    std::uint64_t paint_token_prev = 0;

    // Keyed-list reconciliation — see phenotype::keyed in the DSL.
    // `key` is set on a child by wrapping its builder in keyed(id, [&]{...}).
    // `children_keyed` auto-flips when any direct child has a key; diff
    // then runs a key-based salvage pass after structural diff so that
    // reorder / insert / delete inside a list preserves individual
    // children's layout_valid + paint_* state instead of invalidating
    // every item past the first positional mismatch. 0xFFFFFFFFu = unkeyed.
    static constexpr std::uint32_t unkeyed_key = 0xFFFFFFFFu;
    std::uint32_t key = unkeyed_key;
    bool          children_keyed = false;

    // Tree — generational handles, validated via Arena::get/must_get
    NodeHandle parent = NodeHandle::null();
    std::vector<NodeHandle> children;
};

} // namespace phenotype
