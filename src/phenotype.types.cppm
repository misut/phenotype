module;
#include <cstdint>
#include <functional>
#include <string>
#include <vector>
export module phenotype.types;

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
};

// ============================================================
// Theme — design tokens (matches exon docs CSS variables)
// ============================================================

struct Theme {
    // Color tokens
    Color background    = {250, 250, 250, 255}; // #fafafa
    Color foreground    = { 26,  26,  26, 255}; // #1a1a1a
    Color accent        = { 10, 186, 181, 255}; // #0abab5 (Tiffany)
    Color accent_strong = {  8, 128, 124, 255}; // #08807c
    Color muted         = {107, 114, 128, 255}; // #6b7280
    Color border        = {229, 231, 235, 255}; // #e5e7eb
    Color surface       = {255, 255, 255, 255}; // #ffffff
    Color code_bg       = {243, 244, 246, 255}; // #f3f4f6
    Color hero_bg       = {244, 244, 245, 255}; // #f4f4f5
    Color hero_fg       = { 24,  24,  27, 255}; // #18181b
    Color hero_muted    = {113, 113, 122, 255}; // #71717a
    Color transparent   = {  0,   0,   0,   0};

    // Typography
    float body_font_size     = 16.0f;
    float heading_font_size  = 22.4f;  // 1.4rem
    float hero_title_size    = 40.0f;  // 2.5rem
    float hero_subtitle_size = 18.4f;  // 1.15rem
    float code_font_size     = 14.4f;  // 0.9em
    float small_font_size    = 14.4f;  // 0.9rem
    float line_height_ratio  = 1.6f;

    // Layout
    float max_content_width  = 720.0f;

    // Radius scale
    float radius_xs   = 3.0f;
    float radius_sm   = 4.0f;
    float radius_md   = 6.0f;
    float radius_lg   = 8.0f;
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
    Color state_hover_bg          = {229, 231, 235, 255}; // = border
    Color state_hover_fg          = { 26,  26,  26, 255}; // = foreground
    Color state_active_bg         = {  8, 128, 124, 255}; // = accent_strong
    Color state_active_fg         = {255, 255, 255, 255};
    Color state_disabled_bg       = {243, 244, 246, 255};
    Color state_disabled_fg       = {156, 163, 175, 255};
    Color state_disabled_border   = {229, 231, 235, 255}; // = border
    Color state_error_bg          = {254, 242, 242, 255};
    Color state_error_fg          = {185,  28,  28, 255};
    Color state_error_border      = {220,  38,  38, 255};
    Color state_focus_ring        = { 10, 186, 181, 255}; // = accent (Tiffany)
    float state_focus_ring_width  = 2.0f;

    // Semantic colors (consumers TBD; mirror phenotype-web defaults)
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
};

inline constexpr char const* interaction_role_name(InteractionRole role) noexcept {
    switch (role) {
        case InteractionRole::Link:      return "link";
        case InteractionRole::Button:    return "button";
        case InteractionRole::Checkbox:  return "checkbox";
        case InteractionRole::Radio:     return "radio";
        case InteractionRole::TextField: return "text_field";
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

    // Hover styles (alpha=0 means no hover override)
    Color hover_background = {0, 0, 0, 0};
    Color hover_text_color = {0, 0, 0, 0};

    // Input field
    bool is_input = false;
    std::string placeholder;

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
    std::uint32_t paint_offset = 0;
    std::uint32_t paint_length = 0;
    float         paint_ax = 0.0f;
    float         paint_ay = 0.0f;
    std::uint64_t paint_callback_mask = 0;
    bool          paint_valid = false;

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
