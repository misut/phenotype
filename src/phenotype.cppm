module;
#include <cmath>
#include <cstring>
#include <functional>
#include <string>
#include <vector>
export module phenotype;

// ============================================================
// WASM imports — JS shim implements these
// ============================================================

extern "C" __attribute__((import_module("phenotype"), import_name("flush")))
void phenotype_flush(void);

extern "C" __attribute__((import_module("phenotype"), import_name("measure_text")))
float phenotype_measure_text(float font_size, unsigned int mono,
                             char const* text, unsigned int len);

extern "C" __attribute__((import_module("phenotype"), import_name("get_canvas_width")))
float phenotype_get_canvas_width(void);

extern "C" __attribute__((import_module("phenotype"), import_name("get_canvas_height")))
float phenotype_get_canvas_height(void);

extern "C" __attribute__((import_module("phenotype"), import_name("open_url")))
void phenotype_open_url(char const* url, unsigned int len);

// ============================================================
// Command buffer — shared memory between C++ and JS
// ============================================================

constexpr unsigned int BUF_SIZE = 262144; // 256KB

extern "C" {
    alignas(4) unsigned char phenotype_cmd_buf[BUF_SIZE];
    unsigned int phenotype_cmd_len = 0;

    __attribute__((export_name("phenotype_get_cmd_buf")))
    unsigned char* phenotype_get_cmd_buf(void) { return phenotype_cmd_buf; }

    __attribute__((export_name("phenotype_get_cmd_len")))
    unsigned int phenotype_get_cmd_len(void) { return phenotype_cmd_len; }
}

// Callback registry for event handling (survives _start() return)
namespace phenotype_detail {
    std::vector<std::function<void()>> g_callbacks;
}

extern "C" {
    __attribute__((export_name("phenotype_handle_event")))
    void phenotype_handle_event(unsigned int callback_id) {
        if (callback_id < phenotype_detail::g_callbacks.size())
            phenotype_detail::g_callbacks[callback_id]();
    }
}

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
    Color background   = {250, 250, 250, 255}; // #fafafa
    Color foreground   = { 26,  26,  26, 255}; // #1a1a1a
    Color accent       = { 37,  99, 235, 255}; // #2563eb
    Color muted        = {107, 114, 128, 255}; // #6b7280
    Color border       = {229, 231, 235, 255}; // #e5e7eb
    Color code_bg      = {243, 244, 246, 255}; // #f3f4f6
    Color hero_bg      = { 30,  41,  59, 255}; // #1e293b
    Color hero_fg      = {248, 250, 252, 255}; // #f8fafc
    Color hero_muted   = {148, 163, 184, 255}; // #94a3b8
    Color transparent  = {  0,   0,   0,   0};

    float body_font_size     = 16.0f;
    float heading_font_size  = 22.4f;  // 1.4rem
    float hero_title_size    = 40.0f;  // 2.5rem
    float hero_subtitle_size = 18.4f;  // 1.15rem
    float code_font_size     = 14.4f;  // 0.9em
    float small_font_size    = 14.4f;  // 0.9rem
    float line_height_ratio  = 1.6f;
    float max_content_width  = 720.0f;
};

namespace detail {
    Theme g_theme;
}

Theme const& current_theme() { return detail::g_theme; }

// ============================================================
// Draw commands — 2D primitives, self-contained
// ============================================================

enum class Cmd : unsigned int {
    Clear      = 1,
    FillRect   = 2,
    StrokeRect = 3,
    RoundRect  = 4,
    DrawText   = 5,
    DrawLine   = 6,
    HitRegion  = 7,
};

namespace detail {

void ensure(unsigned int needed) {
    if (phenotype_cmd_len + needed > BUF_SIZE) {
        phenotype_flush();
        phenotype_cmd_len = 0;
    }
}

void write_u32(unsigned int value) {
    auto* p = reinterpret_cast<unsigned int*>(&phenotype_cmd_buf[phenotype_cmd_len]);
    *p = value;
    phenotype_cmd_len += 4;
}

void write_f32(float value) {
    unsigned int bits;
    std::memcpy(&bits, &value, 4);
    write_u32(bits);
}

void write_bytes(char const* data, unsigned int len) {
    for (unsigned int i = 0; i < len; ++i)
        phenotype_cmd_buf[phenotype_cmd_len++] = static_cast<unsigned char>(data[i]);
    while (phenotype_cmd_len % 4 != 0)
        phenotype_cmd_buf[phenotype_cmd_len++] = 0;
}

unsigned int padded(unsigned int len) {
    return (len + 3) & ~3u;
}

} // namespace detail

// Draw command emitters

void emit_clear(Color c) {
    detail::ensure(8);
    detail::write_u32(static_cast<unsigned int>(Cmd::Clear));
    detail::write_u32(c.packed());
}

void emit_fill_rect(float x, float y, float w, float h, Color c) {
    detail::ensure(24);
    detail::write_u32(static_cast<unsigned int>(Cmd::FillRect));
    detail::write_f32(x);
    detail::write_f32(y);
    detail::write_f32(w);
    detail::write_f32(h);
    detail::write_u32(c.packed());
}

void emit_stroke_rect(float x, float y, float w, float h, float line_width, Color c) {
    detail::ensure(28);
    detail::write_u32(static_cast<unsigned int>(Cmd::StrokeRect));
    detail::write_f32(x);
    detail::write_f32(y);
    detail::write_f32(w);
    detail::write_f32(h);
    detail::write_f32(line_width);
    detail::write_u32(c.packed());
}

void emit_round_rect(float x, float y, float w, float h, float radius, Color c) {
    detail::ensure(28);
    detail::write_u32(static_cast<unsigned int>(Cmd::RoundRect));
    detail::write_f32(x);
    detail::write_f32(y);
    detail::write_f32(w);
    detail::write_f32(h);
    detail::write_f32(radius);
    detail::write_u32(c.packed());
}

void emit_draw_text(float x, float y, float font_size, unsigned int mono,
                    Color c, char const* text, unsigned int len) {
    detail::ensure(28 + detail::padded(len));
    detail::write_u32(static_cast<unsigned int>(Cmd::DrawText));
    detail::write_f32(x);
    detail::write_f32(y);
    detail::write_f32(font_size);
    detail::write_u32(mono);
    detail::write_u32(c.packed());
    detail::write_u32(len);
    detail::write_bytes(text, len);
}

void emit_draw_line(float x1, float y1, float x2, float y2, float thickness, Color c) {
    detail::ensure(28);
    detail::write_u32(static_cast<unsigned int>(Cmd::DrawLine));
    detail::write_f32(x1);
    detail::write_f32(y1);
    detail::write_f32(x2);
    detail::write_f32(y2);
    detail::write_f32(thickness);
    detail::write_u32(c.packed());
}

void emit_hit_region(float x, float y, float w, float h,
                     unsigned int callback_id, unsigned int cursor_type) {
    detail::ensure(28);
    detail::write_u32(static_cast<unsigned int>(Cmd::HitRegion));
    detail::write_f32(x);
    detail::write_f32(y);
    detail::write_f32(w);
    detail::write_f32(h);
    detail::write_u32(callback_id);
    detail::write_u32(cursor_type);
}

void flush() {
    if (phenotype_cmd_len == 0) return;
    phenotype_flush();
    phenotype_cmd_len = 0;
}

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

    // Content
    std::string text;
    std::string url;
    unsigned int callback_id = 0xFFFFFFFF;
    unsigned int cursor_type = 0; // 0=default, 1=pointer

    // Computed layout
    float x = 0, y = 0, width = 0, height = 0;

    // Tree
    LayoutNode* parent = nullptr;
    std::vector<LayoutNode*> children;
};

// ============================================================
// Arena allocator — reset per frame
// ============================================================

namespace detail {

constexpr unsigned int ARENA_SIZE = 1024 * 1024; // 1MB
alignas(16) unsigned char g_arena[ARENA_SIZE];
unsigned int g_arena_offset = 0;

void* arena_alloc(unsigned int size) {
    size = (size + 15) & ~15u;
    auto* p = &g_arena[g_arena_offset];
    g_arena_offset += size;
    return p;
}

void arena_reset() {
    // Destruct all LayoutNodes in arena
    // Since we use placement new, we need to track them
    g_arena_offset = 0;
}

// We need a separate list to track nodes for destruction
std::vector<LayoutNode*> g_nodes;

LayoutNode* alloc_node() {
    auto* mem = arena_alloc(sizeof(LayoutNode));
    auto* node = new (mem) LayoutNode();
    g_nodes.push_back(node);
    return node;
}

void reset_nodes() {
    for (auto* n : g_nodes) n->~LayoutNode();
    g_nodes.clear();
    arena_reset();
}

} // namespace detail

// ============================================================
// Text measurement and word wrapping
// ============================================================

namespace detail {

struct TextLayout {
    std::vector<std::string> lines;
    float width;
    float height;
};

float measure(str text, float font_size, bool mono) {
    return phenotype_measure_text(font_size, mono ? 1 : 0, text.data, text.len);
}

TextLayout layout_text(std::string const& text, float font_size, bool mono,
                       float max_width, float line_height) {
    TextLayout result;
    result.width = 0;

    // Split by newlines first
    std::vector<std::string> paragraphs;
    {
        unsigned int start = 0;
        for (unsigned int i = 0; i <= text.size(); ++i) {
            if (i == text.size() || text[i] == '\n') {
                paragraphs.push_back(text.substr(start, i - start));
                start = i + 1;
            }
        }
    }

    for (auto const& para : paragraphs) {
        if (para.empty()) {
            result.lines.push_back("");
            continue;
        }

        // Word wrap
        std::string current_line;
        float current_width = 0;
        unsigned int i = 0;

        while (i < para.size()) {
            // Find next word
            unsigned int word_start = i;
            while (i < para.size() && para[i] != ' ') ++i;
            std::string word = para.substr(word_start, i - word_start);
            // Skip spaces
            while (i < para.size() && para[i] == ' ') ++i;

            float space_width = current_line.empty() ? 0
                : phenotype_measure_text(font_size, mono ? 1 : 0, " ", 1);
            float word_width = phenotype_measure_text(
                font_size, mono ? 1 : 0, word.c_str(),
                static_cast<unsigned int>(word.size()));

            if (!current_line.empty() && current_width + space_width + word_width > max_width) {
                // Wrap
                result.lines.push_back(current_line);
                if (current_width > result.width) result.width = current_width;
                current_line = word;
                current_width = word_width;
            } else {
                if (!current_line.empty()) {
                    current_line += ' ';
                    current_width += space_width;
                }
                current_line += word;
                current_width += word_width;
            }
        }

        result.lines.push_back(current_line);
        if (current_width > result.width) result.width = current_width;
    }

    result.height = static_cast<float>(result.lines.size()) * line_height;
    return result;
}

} // namespace detail

// ============================================================
// Layout algorithm — flexbox subset
// ============================================================

namespace detail {

void layout_node(LayoutNode* node, float available_width) {
    auto const& s = node->style;
    float content_width = available_width;

    // Apply max_width
    if (s.max_width > 0 && content_width > s.max_width)
        content_width = s.max_width;

    node->width = content_width;

    float inner_width = content_width - s.padding[1] - s.padding[3]; // right, left

    // Text leaf
    if (!node->text.empty() && node->children.empty()) {
        float line_height = node->font_size * g_theme.line_height_ratio;
        auto tl = layout_text(node->text, node->font_size, node->mono, inner_width, line_height);
        node->height = tl.height + s.padding[0] + s.padding[2];
        return;
    }

    // Fixed height (Spacer, Divider)
    if (s.fixed_height >= 0 && node->children.empty()) {
        node->height = s.fixed_height + s.padding[0] + s.padding[2];
        return;
    }

    // Auto-center nodes with max_width
    if (s.max_width > 0 && content_width < available_width) {
        node->x += (available_width - content_width) / 2;
    }

    // Container layout
    if (s.flex_direction == FlexDirection::Column) {
        // First pass: layout children to compute total height
        float total_children_h = 0;
        unsigned int nc = static_cast<unsigned int>(node->children.size());
        for (unsigned int i = 0; i < nc; ++i) {
            auto* child = node->children[i];
            if (s.cross_align == CrossAxisAlignment::Stretch)
                layout_node(child, inner_width);
            else
                layout_node(child, inner_width);
            total_children_h += child->height;
            if (i + 1 < nc) total_children_h += s.gap;
        }

        // Main axis (vertical) alignment
        float y = s.padding[0];
        float effective_gap = s.gap;
        if (s.main_align == MainAxisAlignment::SpaceBetween && nc > 1) {
            float avail_h = node->height > 0
                ? node->height - s.padding[0] - s.padding[2]
                : total_children_h;
            float children_only = total_children_h - s.gap * static_cast<float>(nc - 1);
            effective_gap = (avail_h - children_only) / static_cast<float>(nc - 1);
        }
        // (Center/End only meaningful if node has a fixed or known height)

        for (unsigned int i = 0; i < nc; ++i) {
            auto* child = node->children[i];
            // Cross axis (horizontal) alignment
            switch (s.cross_align) {
                case CrossAxisAlignment::Center:
                    child->x = s.padding[3] + (inner_width - child->width) / 2;
                    break;
                case CrossAxisAlignment::End:
                    child->x = s.padding[3] + inner_width - child->width;
                    break;
                default: // Start, Stretch
                    child->x = s.padding[3];
                    break;
            }
            child->y = y;
            y += child->height;
            if (i + 1 < nc) y += effective_gap;
        }
        node->height = y + s.padding[2];
    } else {
        // Row: intrinsic-width children get their measured size,
        // remaining space goes to the last flexible child.
        unsigned int n = static_cast<unsigned int>(node->children.size());
        float total_gap = (n > 1) ? s.gap * static_cast<float>(n - 1) : 0;

        // First pass: measure intrinsic widths for text leaves
        float used = total_gap;
        int flex_index = -1;
        for (unsigned int i = 0; i < n; ++i) {
            auto* child = node->children[i];
            bool is_text_leaf = !child->text.empty() && child->children.empty();
            if (is_text_leaf) {
                float measured = phenotype_measure_text(
                    child->font_size, child->mono ? 1 : 0,
                    child->text.c_str(), static_cast<unsigned int>(child->text.size()));
                float w = measured + child->style.padding[1] + child->style.padding[3];
                child->width = w;
                used += w;
            } else {
                flex_index = static_cast<int>(i);
            }
        }
        if (flex_index < 0) flex_index = static_cast<int>(n) - 1;

        float remaining = inner_width - used;
        if (remaining < 0) remaining = 0;

        // Second pass: layout with computed widths
        float total_used_w = 0;
        float max_h = 0;
        for (unsigned int i = 0; i < n; ++i) {
            auto* child = node->children[i];
            float cw = (static_cast<int>(i) == flex_index)
                ? remaining + child->width
                : child->width;
            layout_node(child, cw);
            total_used_w += child->width;
            if (child->height > max_h) max_h = child->height;
        }

        // Main axis (horizontal) alignment
        float x = s.padding[3];
        float effective_gap = s.gap;
        float total_w = total_used_w + total_gap;
        if (s.main_align == MainAxisAlignment::Center) {
            x = s.padding[3] + (inner_width - total_w) / 2;
        } else if (s.main_align == MainAxisAlignment::End) {
            x = s.padding[3] + inner_width - total_w;
        } else if (s.main_align == MainAxisAlignment::SpaceBetween && n > 1) {
            effective_gap = (inner_width - total_used_w) / static_cast<float>(n - 1);
        }

        for (unsigned int i = 0; i < n; ++i) {
            auto* child = node->children[i];
            child->x = x;
            // Cross axis (vertical) alignment
            switch (s.cross_align) {
                case CrossAxisAlignment::Center:
                    child->y = s.padding[0] + (max_h - child->height) / 2;
                    break;
                case CrossAxisAlignment::End:
                    child->y = s.padding[0] + max_h - child->height;
                    break;
                default: // Start, Stretch
                    child->y = s.padding[0];
                    break;
            }
            x += child->width;
            if (i + 1 < n) x += effective_gap;
        }
        node->height = max_h + s.padding[0] + s.padding[2];
    }
}

} // namespace detail

// ============================================================
// Paint — walk layout tree, emit draw commands
// ============================================================

namespace detail {

void paint_node(LayoutNode* node, float ox, float oy, float scroll_y,
                float vp_height) {
    float ax = ox + node->x;
    float ay = oy + node->y;

    // Viewport culling (leaf nodes only — containers delegate to children)
    if (node->children.empty()) {
        if (ay + node->height < scroll_y || ay > scroll_y + vp_height) return;
    }

    float draw_y = ay - scroll_y;

    // Background
    if (node->background.a > 0) {
        if (node->border_radius > 0)
            emit_round_rect(ax, draw_y, node->width, node->height,
                            node->border_radius, node->background);
        else
            emit_fill_rect(ax, draw_y, node->width, node->height, node->background);
    }

    // Border
    if (node->border_width > 0 && node->border_color.a > 0) {
        emit_stroke_rect(ax, draw_y, node->width, node->height,
                         node->border_width, node->border_color);
    }

    // Text
    if (!node->text.empty() && node->children.empty()) {
        float line_height = node->font_size * g_theme.line_height_ratio;
        float inner_width = node->width - node->style.padding[1] - node->style.padding[3];
        auto tl = layout_text(node->text, node->font_size, node->mono,
                               inner_width, line_height);
        float ty = draw_y + node->style.padding[0];
        for (auto const& line : tl.lines) {
            if (!line.empty()) {
                float line_w = phenotype_measure_text(
                    node->font_size, node->mono ? 1 : 0,
                    line.c_str(), static_cast<unsigned int>(line.size()));
                float tx = ax + node->style.padding[3];
                if (node->style.text_align == TextAlign::Center)
                    tx += (inner_width - line_w) / 2;
                else if (node->style.text_align == TextAlign::End)
                    tx += inner_width - line_w;
                emit_draw_text(tx, ty, node->font_size, node->mono ? 1 : 0,
                               node->text_color, line.c_str(),
                               static_cast<unsigned int>(line.size()));
            }
            ty += line_height;
        }
    }

    // Hit region
    if (node->callback_id != 0xFFFFFFFF) {
        emit_hit_region(ax, draw_y, node->width, node->height,
                        node->callback_id, node->cursor_type);
    }

    // Children
    for (auto* child : node->children)
        paint_node(child, ax, ay, scroll_y, vp_height);
}

} // namespace detail

// ============================================================
// Scope — implicit parent tracking for declarative DSL
// ============================================================

struct Scope {
    LayoutNode* node;
    std::function<void()> builder;

    Scope(LayoutNode* n) : node(n) {}
    Scope(LayoutNode* n, std::function<void()> b) : node(n), builder(std::move(b)) {}

    void mutate();

    static Scope*& current() {
        static Scope* s = nullptr;
        return s;
    }

    static void set_current(Scope* s) { current() = s; }
};

// ============================================================
// Trait<T> — reactive state with partial mutation
// ============================================================

template<typename T>
class Trait {
    T value_;
    std::vector<Scope*> subscribers_;

public:
    explicit Trait(T initial) : value_(std::move(initial)) {}

    T const& value() {
        if (Scope::current()) {
            auto* s = Scope::current();
            bool found = false;
            for (auto* sub : subscribers_)
                if (sub == s) { found = true; break; }
            if (!found)
                subscribers_.push_back(s);
        }
        return value_;
    }

    void set(T new_val) {
        if (value_ == new_val) return;
        value_ = std::move(new_val);
        for (auto* scope : subscribers_)
            scope->mutate();
    }
};

// Global state storage (heap-allocated, survives _start())
namespace detail {
    struct StateSlot {
        void* ptr;
        void (*deleter)(void*);
        ~StateSlot() { if (deleter) deleter(ptr); }
    };
    std::vector<StateSlot> g_states;

    // Root node for relayout/repaint on mutate
    LayoutNode* g_root = nullptr;
    float g_scroll_y = 0;
}

template<typename T>
Trait<T>* encode(T initial) {
    auto* p = new Trait<T>(std::move(initial));
    detail::g_states.push_back({p, [](void* ptr) { delete static_cast<Trait<T>*>(ptr); }});
    return p;
}

// Scope::mutate needs access to relayout/repaint
void Scope::mutate() {
    if (!builder) return;
    node->children.clear();
    auto* prev = current();
    set_current(this);
    builder();
    set_current(prev);
    // Relayout and repaint entire tree
    if (detail::g_root) {
        float cw = phenotype_get_canvas_width();
        detail::layout_node(detail::g_root, cw);
        emit_clear(detail::g_theme.background);
        float vh = phenotype_get_canvas_height();
        detail::paint_node(detail::g_root, 0, 0, detail::g_scroll_y, vh);
        flush();
    }
}

// ============================================================
// DSL — style context for Scaffold sections
// ============================================================

namespace detail {

enum class Section { Normal, Hero, Footer };
Section g_section = Section::Normal;
unsigned int g_section_child_index = 0;

} // namespace detail

// ============================================================
// DSL Components
// ============================================================

// Text
void Text(str content) {
    auto* node = detail::alloc_node();
    node->text = std::string(content.data, content.len);

    auto const& t = detail::g_theme;
    if (detail::g_section == detail::Section::Hero) {
        if (detail::g_section_child_index == 0) {
            // Hero title
            node->font_size = t.hero_title_size;
            node->text_color = t.hero_fg;
        } else {
            // Hero subtitle
            node->font_size = t.hero_subtitle_size;
            node->text_color = t.hero_muted;
        }
        detail::g_section_child_index++;
    } else if (detail::g_section == detail::Section::Footer) {
        node->font_size = t.small_font_size;
        node->text_color = t.muted;
    } else {
        node->font_size = t.body_font_size;
        node->text_color = t.foreground;
    }

    if (Scope::current()) {
        // Inherit text_align from parent
        node->style.text_align = Scope::current()->node->style.text_align;
        Scope::current()->node->children.push_back(node);
    }
}

// Button
void Button(str label, std::function<void()> on_click = {}) {
    auto* node = detail::alloc_node();
    node->text = std::string(label.data, label.len);
    node->font_size = detail::g_theme.body_font_size;
    node->text_color = detail::g_theme.foreground;
    node->background = detail::g_theme.code_bg;
    node->border_radius = 4;
    node->style.padding[0] = 6; node->style.padding[1] = 12;
    node->style.padding[2] = 6; node->style.padding[3] = 12;
    node->cursor_type = 1; // pointer

    if (on_click) {
        auto id = static_cast<unsigned int>(phenotype_detail::g_callbacks.size());
        phenotype_detail::g_callbacks.push_back(std::move(on_click));
        node->callback_id = id;
    }

    if (Scope::current())
        Scope::current()->node->children.push_back(node);
}

// Link
void Link(str label, str href) {
    auto* node = detail::alloc_node();
    node->text = std::string(label.data, label.len);
    node->font_size = detail::g_theme.small_font_size;
    node->text_color = detail::g_theme.accent;
    node->url = std::string(href.data, href.len);
    node->cursor_type = 1; // pointer

    if (detail::g_section == detail::Section::Footer) {
        node->font_size = detail::g_theme.small_font_size;
    }

    // Register click callback to open URL
    auto url_copy = node->url;
    auto id = static_cast<unsigned int>(phenotype_detail::g_callbacks.size());
    phenotype_detail::g_callbacks.push_back([url_copy] {
        phenotype_open_url(url_copy.c_str(), static_cast<unsigned int>(url_copy.size()));
    });
    node->callback_id = id;

    if (Scope::current())
        Scope::current()->node->children.push_back(node);
}

// Code block
void Code(str content) {
    auto* node = detail::alloc_node();
    node->text = std::string(content.data, content.len);
    node->font_size = detail::g_theme.code_font_size;
    node->text_color = detail::g_theme.foreground;
    node->mono = true;
    node->background = detail::g_theme.code_bg;
    node->border_color = detail::g_theme.border;
    node->border_width = 1;
    node->border_radius = 6;
    node->style.padding[0] = 16; node->style.padding[1] = 16;
    node->style.padding[2] = 16; node->style.padding[3] = 16;

    if (Scope::current())
        Scope::current()->node->children.push_back(node);
}

// Divider
void Divider() {
    auto* node = detail::alloc_node();
    node->style.fixed_height = 1;
    node->background = detail::g_theme.border;

    if (Scope::current())
        Scope::current()->node->children.push_back(node);
}

// Spacer
void Spacer(unsigned int height_px) {
    auto* node = detail::alloc_node();
    node->style.fixed_height = static_cast<float>(height_px);

    if (Scope::current())
        Scope::current()->node->children.push_back(node);
}

// ============================================================
// Container helpers
// ============================================================

namespace detail {

void open_container(LayoutNode* container, std::function<void()> builder) {
    if (Scope::current())
        Scope::current()->node->children.push_back(container);
    auto* scope = new Scope(container, std::move(builder));
    auto* prev = Scope::current();
    Scope::set_current(scope);
    scope->builder();
    Scope::set_current(prev);
}

template<typename Arg>
void render_one(Arg&& arg) {
    if constexpr (std::is_invocable_v<Arg>) {
        arg();
    }
}

} // namespace detail

// Column
template<typename F>
    requires std::is_invocable_v<F>
void Column(F&& builder) {
    auto* node = detail::alloc_node();
    node->style.flex_direction = FlexDirection::Column;
    node->style.gap = 12;
    detail::open_container(node, std::forward<F>(builder));
}

template<typename A, typename B, typename... Rest>
void Column(A&& a, B&& b, Rest&&... rest) {
    auto* node = detail::alloc_node();
    node->style.flex_direction = FlexDirection::Column;
    node->style.gap = 12;
    if (Scope::current())
        Scope::current()->node->children.push_back(node);
    Scope child_scope(node);
    auto* prev = Scope::current();
    Scope::set_current(&child_scope);
    (detail::render_one(std::forward<A>(a)),
     detail::render_one(std::forward<B>(b)),
     (detail::render_one(std::forward<Rest>(rest)), ...));
    Scope::set_current(prev);
}

// Row
template<typename F>
    requires std::is_invocable_v<F>
void Row(F&& builder) {
    auto* node = detail::alloc_node();
    node->style.flex_direction = FlexDirection::Row;
    node->style.gap = 8;
    detail::open_container(node, std::forward<F>(builder));
}

template<typename A, typename B, typename... Rest>
void Row(A&& a, B&& b, Rest&&... rest) {
    auto* node = detail::alloc_node();
    node->style.flex_direction = FlexDirection::Row;
    node->style.gap = 8;
    if (Scope::current())
        Scope::current()->node->children.push_back(node);
    Scope child_scope(node);
    auto* prev = Scope::current();
    Scope::set_current(&child_scope);
    (detail::render_one(std::forward<A>(a)),
     detail::render_one(std::forward<B>(b)),
     (detail::render_one(std::forward<Rest>(rest)), ...));
    Scope::set_current(prev);
}

// Box
template<typename F>
    requires std::is_invocable_v<F>
void Box(F&& builder) {
    auto* node = detail::alloc_node();
    detail::open_container(node, std::forward<F>(builder));
}

template<typename A, typename B, typename... Rest>
void Box(A&& a, B&& b, Rest&&... rest) {
    auto* node = detail::alloc_node();
    if (Scope::current())
        Scope::current()->node->children.push_back(node);
    Scope child_scope(node);
    auto* prev = Scope::current();
    Scope::set_current(&child_scope);
    (detail::render_one(std::forward<A>(a)),
     detail::render_one(std::forward<B>(b)),
     (detail::render_one(std::forward<Rest>(rest)), ...));
    Scope::set_current(prev);
}

// ============================================================
// Compose-style higher-level components
// ============================================================

// Scaffold — page layout with topBar, content, bottomBar
void Scaffold(std::function<void()> top_bar,
              std::function<void()> content,
              std::function<void()> bottom_bar = {}) {
    if (top_bar) {
        auto* header = detail::alloc_node();
        header->style.flex_direction = FlexDirection::Column;
        header->style.gap = 8;
        header->style.cross_align = CrossAxisAlignment::Center;
        header->style.text_align = TextAlign::Center;
        header->background = detail::g_theme.hero_bg;
        header->style.padding[0] = 48; header->style.padding[1] = 24;
        header->style.padding[2] = 48; header->style.padding[3] = 24;

        auto saved_section = detail::g_section;
        auto saved_index = detail::g_section_child_index;
        detail::g_section = detail::Section::Hero;
        detail::g_section_child_index = 0;
        detail::open_container(header, std::move(top_bar));
        detail::g_section = saved_section;
        detail::g_section_child_index = saved_index;
    }

    if (content) {
        auto* main = detail::alloc_node();
        main->style.flex_direction = FlexDirection::Column;
        main->style.gap = 32;
        main->style.max_width = detail::g_theme.max_content_width;
        main->style.padding[0] = 32; main->style.padding[1] = 24;
        main->style.padding[2] = 32; main->style.padding[3] = 24;

        detail::open_container(main, std::move(content));
    }

    if (bottom_bar) {
        auto* footer = detail::alloc_node();
        footer->style.flex_direction = FlexDirection::Row;
        footer->style.gap = 0;
        footer->style.main_align = MainAxisAlignment::Center;
        footer->style.cross_align = CrossAxisAlignment::Center;
        footer->style.padding[0] = 32; footer->style.padding[1] = 24;
        footer->style.padding[2] = 32; footer->style.padding[3] = 24;
        footer->border_color = detail::g_theme.border;
        footer->border_width = 1;

        auto saved_section = detail::g_section;
        detail::g_section = detail::Section::Footer;
        detail::open_container(footer, std::move(bottom_bar));
        detail::g_section = saved_section;
    }
}

// Surface
template<typename F>
    requires std::is_invocable_v<F>
void Surface(F&& builder) {
    auto* node = detail::alloc_node();
    detail::open_container(node, std::forward<F>(builder));
}

// Card
template<typename F>
    requires std::is_invocable_v<F>
void Card(F&& builder) {
    auto* node = detail::alloc_node();
    node->border_radius = 8;
    node->background = {255, 255, 255, 255};
    node->style.padding[0] = 16; node->style.padding[1] = 16;
    node->style.padding[2] = 16; node->style.padding[3] = 16;
    detail::open_container(node, std::forward<F>(builder));
}

// ListItems
template<typename F>
    requires std::is_invocable_v<F>
void ListItems(F&& builder) {
    auto* node = detail::alloc_node();
    node->style.flex_direction = FlexDirection::Column;
    node->style.gap = 6;
    node->style.padding[3] = 24; // left indent for bullets
    detail::open_container(node, std::forward<F>(builder));
}

// Item (list item with bullet)
void Item(str content) {
    auto* row = detail::alloc_node();
    row->style.flex_direction = FlexDirection::Row;
    row->style.gap = 8;

    // Bullet
    auto* bullet = detail::alloc_node();
    bullet->text = "\xe2\x80\xa2"; // bullet character "•"
    bullet->font_size = detail::g_theme.body_font_size;
    bullet->text_color = detail::g_theme.foreground;
    row->children.push_back(bullet);

    // Text
    auto* text = detail::alloc_node();
    text->text = std::string(content.data, content.len);
    text->font_size = detail::g_theme.body_font_size;
    text->text_color = detail::g_theme.foreground;
    row->children.push_back(text);

    if (Scope::current())
        Scope::current()->node->children.push_back(row);
}

// ============================================================
// express() — application entry point
// ============================================================

template<typename F>
void express(F&& app_fn) {
    detail::reset_nodes();
    phenotype_detail::g_callbacks.clear();

    auto* root = detail::alloc_node();
    root->style.flex_direction = FlexDirection::Column;
    root->background = detail::g_theme.background;
    detail::g_root = root;

    static Scope root_scope(root);
    root_scope.node = root;
    Scope::set_current(&root_scope);

    app_fn(); // Build pass

    float cw = phenotype_get_canvas_width();
    detail::layout_node(root, cw); // Layout pass

    float vh = phenotype_get_canvas_height();
    emit_clear(detail::g_theme.background);
    detail::paint_node(root, 0, 0, detail::g_scroll_y, vh); // Paint pass
    flush();

    Scope::set_current(nullptr);
}

// ============================================================
// Repaint — called from JS on scroll/resize
// ============================================================

} // namespace phenotype

extern "C" {
    __attribute__((export_name("phenotype_repaint")))
    void phenotype_repaint(float scroll_y) {
        phenotype::detail::g_scroll_y = scroll_y;
        if (phenotype::detail::g_root) {
            float cw = phenotype_get_canvas_width();
            phenotype::detail::layout_node(phenotype::detail::g_root, cw);
            phenotype::emit_clear(phenotype::detail::g_theme.background);
            float vh = phenotype_get_canvas_height();
            phenotype::detail::paint_node(phenotype::detail::g_root, 0, 0, scroll_y, vh);
            phenotype::flush();
        }
    }

    __attribute__((export_name("phenotype_get_total_height")))
    float phenotype_get_total_height(void) {
        if (phenotype::detail::g_root)
            return phenotype::detail::g_root->height;
        return 0;
    }
}
