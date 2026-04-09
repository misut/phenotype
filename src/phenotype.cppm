module;
#include <cmath>
#include <cstring>
#include <functional>
#include <string>
#include <vector>
#include <map>
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

constexpr unsigned int BUF_SIZE = 65536; // 64KB

extern "C" {
    alignas(4) unsigned char phenotype_cmd_buf[BUF_SIZE];
    unsigned int phenotype_cmd_len = 0;

    __attribute__((export_name("phenotype_get_cmd_buf")))
    unsigned char* phenotype_get_cmd_buf(void) { return phenotype_cmd_buf; }

    __attribute__((export_name("phenotype_get_cmd_len")))
    unsigned int phenotype_get_cmd_len(void) { return phenotype_cmd_len; }
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

// g_app forward-declared — defined after LayoutNode and Arena

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

    // Hover styles (alpha=0 means no hover override)
    Color hover_background = {0, 0, 0, 0};
    Color hover_text_color = {0, 0, 0, 0};

    // Input field
    bool is_input = false;
    std::string placeholder;
    void* input_state = nullptr; // Trait<std::string>*, resolved at runtime

    // Computed layout
    float x = 0, y = 0, width = 0, height = 0;

    // Cached text layout (filled during layout pass, reused in paint)
    std::vector<std::string> text_lines;

    // Tree
    LayoutNode* parent = nullptr;
    std::vector<LayoutNode*> children;
};

// ============================================================
// Arena allocator
// ============================================================

struct Arena {
    static constexpr unsigned int STORAGE_SIZE = 512 * 1024; // 512KB
    static constexpr unsigned int MAX_NODES = 4096;

    unsigned char* storage = nullptr;
    unsigned int offset = 0;
    unsigned int node_count = 0;
    LayoutNode** nodes = nullptr;

    void ensure_init() {
        if (!storage) {
            storage = new unsigned char[STORAGE_SIZE];
            nodes = new LayoutNode*[MAX_NODES];
        }
    }

    void* alloc(unsigned int size) {
        ensure_init();
        size = (size + 15) & ~15u;
        auto* p = &storage[offset];
        offset += size;
        return p;
    }

    LayoutNode* alloc_node() {
        auto* mem = alloc(sizeof(LayoutNode));
        auto* node = new (mem) LayoutNode();
        nodes[node_count++] = node;
        return node;
    }

    void reset() {
        for (unsigned int i = 0; i < node_count; ++i) nodes[i]->~LayoutNode();
        node_count = 0;
        offset = 0;
    }
};

// ============================================================
// AppState — consolidated global state
// ============================================================

struct StateSlot {
    void* ptr;
    void (*deleter)(void*);

    StateSlot(void* p, void (*d)(void*)) : ptr(p), deleter(d) {}
    StateSlot(StateSlot&& o) noexcept : ptr(o.ptr), deleter(o.deleter) {
        o.ptr = nullptr;
        o.deleter = nullptr;
    }
    StateSlot& operator=(StateSlot&& o) noexcept {
        if (this != &o) {
            if (deleter) deleter(ptr);
            ptr = o.ptr; deleter = o.deleter;
            o.ptr = nullptr; o.deleter = nullptr;
        }
        return *this;
    }
    ~StateSlot() { if (deleter) deleter(ptr); }

    StateSlot(StateSlot const&) = delete;
    StateSlot& operator=(StateSlot const&) = delete;
};

struct AppState {
    Theme theme;
    Arena arena;
    LayoutNode* root = nullptr;
    float scroll_y = 0;
    unsigned int hovered_id = 0xFFFFFFFF;
    unsigned int focused_id = 0xFFFFFFFF;
    unsigned int caret_pos = 0;
    bool caret_visible = true;
    std::vector<std::function<void()>> callbacks;
    std::vector<StateSlot> states;
    std::vector<unsigned int> focusable_ids;
    std::map<unsigned int, LayoutNode*> input_nodes; // callback_id → input node
};

namespace detail {
    AppState g_app;
    LayoutNode* alloc_node() { return g_app.arena.alloc_node(); }
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

    // Text leaf — compute and cache word-wrapped lines
    if (!node->text.empty() && node->children.empty()) {
        float line_height = node->font_size * g_app.theme.line_height_ratio;
        auto tl = layout_text(node->text, node->font_size, node->mono, inner_width, line_height);
        node->text_lines = std::move(tl.lines);
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

    // Hover/focus state
    bool is_hovered = (node->callback_id != 0xFFFFFFFF &&
                       node->callback_id == g_app.hovered_id);
    bool is_focused = (node->callback_id != 0xFFFFFFFF &&
                       node->callback_id == g_app.focused_id);

    // Background (with hover override)
    Color bg = (is_hovered && node->hover_background.a > 0)
        ? node->hover_background : node->background;
    if (bg.a > 0) {
        if (node->border_radius > 0)
            emit_round_rect(ax, draw_y, node->width, node->height,
                            node->border_radius, bg);
        else
            emit_fill_rect(ax, draw_y, node->width, node->height, bg);
    }

    // Border (accent color when focused)
    Color bc = is_focused ? g_app.theme.accent : node->border_color;
    float bw = is_focused ? 2.0f : node->border_width;
    if (bw > 0 && bc.a > 0) {
        emit_stroke_rect(ax, draw_y, node->width, node->height, bw, bc);
    }

    // Text — use cached text_lines from layout pass
    Color tc = (is_hovered && node->hover_text_color.a > 0)
        ? node->hover_text_color : node->text_color;
    if (!node->text_lines.empty()) {
        float line_height = node->font_size * g_app.theme.line_height_ratio;
        float inner_width = node->width - node->style.padding[1] - node->style.padding[3];
        float ty = draw_y + node->style.padding[0];
        for (auto const& line : node->text_lines) {
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
                               tc, line.c_str(),
                               static_cast<unsigned int>(line.size()));
            }
            ty += line_height;
        }
    }

    // Caret for focused input — use displayed text to compute position
    if (is_focused && node->is_input && g_app.caret_visible) {
        float caret_x = ax + node->style.padding[3];
        // Measure displayed text width (text_lines[0] if not placeholder)
        if (!node->text_lines.empty() && !node->text_lines[0].empty() &&
            tc.r != g_app.theme.muted.r) { // not placeholder
            caret_x += phenotype_measure_text(
                node->font_size, 0, node->text_lines[0].c_str(),
                static_cast<unsigned int>(node->text_lines[0].size()));
        }
        float caret_y = draw_y + node->style.padding[0];
        float caret_h = node->font_size * g_app.theme.line_height_ratio;
        emit_draw_line(caret_x, caret_y, caret_x, caret_y + caret_h, 1.5f, g_app.theme.accent);
    }

    // Collect focusable IDs (for Tab navigation)
    if (node->callback_id != 0xFFFFFFFF)
        g_app.focusable_ids.push_back(node->callback_id);

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

template<typename T>
Trait<T>* encode(T initial) {
    auto* p = new Trait<T>(std::move(initial));
    detail::g_app.states.push_back({p, [](void* ptr) { delete static_cast<Trait<T>*>(ptr); }});
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
    if (detail::g_app.root) {
        float cw = phenotype_get_canvas_width();
        detail::layout_node(detail::g_app.root, cw);
        emit_clear(detail::g_app.theme.background);
        float vh = phenotype_get_canvas_height();
        detail::paint_node(detail::g_app.root, 0, 0, detail::g_app.scroll_y, vh);
        flush();
    }
}

// ============================================================
// DSL — style context for Scaffold sections
// ============================================================

// ============================================================
// DSL Components
// ============================================================

// Text
void Text(str content) {
    auto* node = detail::alloc_node();
    node->text = std::string(content.data, content.len);
    node->font_size = detail::g_app.theme.body_font_size;
    node->text_color = detail::g_app.theme.foreground;

    if (Scope::current()) {
        node->style.text_align = Scope::current()->node->style.text_align;
        Scope::current()->node->children.push_back(node);
    }
}

// Button
void Button(str label, std::function<void()> on_click = {}) {
    auto* node = detail::alloc_node();
    node->text = std::string(label.data, label.len);
    node->font_size = detail::g_app.theme.body_font_size;
    node->text_color = detail::g_app.theme.foreground;
    node->background = detail::g_app.theme.code_bg;
    node->hover_background = detail::g_app.theme.border; // slightly darker on hover
    node->border_radius = 4;
    node->style.padding[0] = 6; node->style.padding[1] = 12;
    node->style.padding[2] = 6; node->style.padding[3] = 12;
    node->cursor_type = 1; // pointer

    if (on_click) {
        auto id = static_cast<unsigned int>(detail::g_app.callbacks.size());
        detail::g_app.callbacks.push_back(std::move(on_click));
        node->callback_id = id;
    }

    if (Scope::current())
        Scope::current()->node->children.push_back(node);
}

// Link
void Link(str label, str href) {
    auto* node = detail::alloc_node();
    node->text = std::string(label.data, label.len);
    node->font_size = detail::g_app.theme.small_font_size;
    node->text_color = detail::g_app.theme.accent;
    node->hover_text_color = {29, 78, 216, 255}; // darker blue on hover
    node->url = std::string(href.data, href.len);
    node->cursor_type = 1; // pointer

    // Register click callback to open URL
    auto url_copy = node->url;
    auto id = static_cast<unsigned int>(detail::g_app.callbacks.size());
    detail::g_app.callbacks.push_back([url_copy] {
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
    node->font_size = detail::g_app.theme.code_font_size;
    node->text_color = detail::g_app.theme.foreground;
    node->mono = true;
    node->background = detail::g_app.theme.code_bg;
    node->border_color = detail::g_app.theme.border;
    node->border_width = 1;
    node->border_radius = 6;
    node->style.padding[0] = 16; node->style.padding[1] = 16;
    node->style.padding[2] = 16; node->style.padding[3] = 16;

    if (Scope::current())
        Scope::current()->node->children.push_back(node);
}

// TextField — single-line text input bound to Trait<std::string>
void TextField(Trait<std::string>* state, str placeholder = "") {
    auto* node = detail::alloc_node();
    node->is_input = true;
    node->input_state = state;
    node->placeholder = std::string(placeholder.data, placeholder.len);

    // Display current value or placeholder
    auto const& val = state->value();
    node->text = val.empty() ? node->placeholder : val;
    node->font_size = detail::g_app.theme.body_font_size;
    node->text_color = val.empty() ? detail::g_app.theme.muted : detail::g_app.theme.foreground;
    node->background = {255, 255, 255, 255};
    node->border_color = detail::g_app.theme.border;
    node->border_width = 1;
    node->border_radius = 4;
    node->style.padding[0] = 8; node->style.padding[1] = 12;
    node->style.padding[2] = 8; node->style.padding[3] = 12;
    node->cursor_type = 1; // text cursor

    // Register callback (for focus) and track as input node
    auto id = static_cast<unsigned int>(detail::g_app.callbacks.size());
    detail::g_app.callbacks.push_back([]{}); // no-op click, focus handled by phenotype_set_focus
    node->callback_id = id;
    detail::g_app.input_nodes[id] = node;

    if (Scope::current())
        Scope::current()->node->children.push_back(node);
}

// Divider
void Divider() {
    auto* node = detail::alloc_node();
    node->style.fixed_height = 1;
    node->background = detail::g_app.theme.border;

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
        header->background = detail::g_app.theme.hero_bg;
        header->style.padding[0] = 48; header->style.padding[1] = 24;
        header->style.padding[2] = 48; header->style.padding[3] = 24;

        detail::open_container(header, std::move(top_bar));
        // Post-build: apply hero styles to children
        auto const& t = detail::g_app.theme;
        if (header->children.size() >= 1) {
            header->children[0]->font_size = t.hero_title_size;
            header->children[0]->text_color = t.hero_fg;
        }
        if (header->children.size() >= 2) {
            header->children[1]->font_size = t.hero_subtitle_size;
            header->children[1]->text_color = t.hero_muted;
        }
    }

    if (content) {
        auto* main = detail::alloc_node();
        main->style.flex_direction = FlexDirection::Column;
        main->style.gap = 32;
        main->style.max_width = detail::g_app.theme.max_content_width;
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
        footer->border_color = detail::g_app.theme.border;
        footer->border_width = 1;

        detail::open_container(footer, std::move(bottom_bar));
        // Post-build: apply footer styles to children
        auto const& t = detail::g_app.theme;
        for (auto* child : footer->children) {
            child->font_size = t.small_font_size;
            child->text_color = t.muted;
        }
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
    bullet->font_size = detail::g_app.theme.body_font_size;
    bullet->text_color = detail::g_app.theme.foreground;
    row->children.push_back(bullet);

    // Text
    auto* text = detail::alloc_node();
    text->text = std::string(content.data, content.len);
    text->font_size = detail::g_app.theme.body_font_size;
    text->text_color = detail::g_app.theme.foreground;
    row->children.push_back(text);

    if (Scope::current())
        Scope::current()->node->children.push_back(row);
}

// ============================================================
// express() — application entry point
// ============================================================

template<typename F>
void express(F&& app_fn) {
    detail::g_app.arena.reset();
    detail::g_app.callbacks.clear();
    detail::g_app.input_nodes.clear();

    auto* root = detail::alloc_node();
    root->style.flex_direction = FlexDirection::Column;
    root->background = detail::g_app.theme.background;
    detail::g_app.root = root;

    static Scope root_scope(root);
    root_scope.node = root;
    Scope::set_current(&root_scope);

    app_fn(); // Build pass

    float cw = phenotype_get_canvas_width();
    detail::layout_node(root, cw); // Layout pass

    float vh = phenotype_get_canvas_height();
    detail::g_app.focusable_ids.clear();
    emit_clear(detail::g_app.theme.background);
    detail::paint_node(root, 0, 0, detail::g_app.scroll_y, vh); // Paint pass
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
        phenotype::detail::g_app.scroll_y = scroll_y;
        if (phenotype::detail::g_app.root) {
            float cw = phenotype_get_canvas_width();
            phenotype::detail::layout_node(phenotype::detail::g_app.root, cw);
            phenotype::detail::g_app.focusable_ids.clear();
            phenotype::emit_clear(phenotype::detail::g_app.theme.background);
            float vh = phenotype_get_canvas_height();
            phenotype::detail::paint_node(phenotype::detail::g_app.root, 0, 0, scroll_y, vh);
            phenotype::flush();
        }
    }

    __attribute__((export_name("phenotype_get_total_height")))
    float phenotype_get_total_height(void) {
        if (phenotype::detail::g_app.root)
            return phenotype::detail::g_app.root->height;
        return 0;
    }

    __attribute__((export_name("phenotype_handle_event")))
    void phenotype_handle_event(unsigned int callback_id) {
        if (callback_id < phenotype::detail::g_app.callbacks.size())
            phenotype::detail::g_app.callbacks[callback_id]();
    }

    __attribute__((export_name("phenotype_set_hover")))
    void phenotype_set_hover(unsigned int callback_id) {
        if (phenotype::detail::g_app.hovered_id == callback_id) return;
        phenotype::detail::g_app.hovered_id = callback_id;
        phenotype_repaint(phenotype::detail::g_app.scroll_y);
    }

    __attribute__((export_name("phenotype_set_focus")))
    void phenotype_set_focus(unsigned int callback_id) {
        phenotype::detail::g_app.focused_id = callback_id;
        phenotype::detail::g_app.caret_pos = 0xFFFFFFFF; // end of text
        phenotype::detail::g_app.caret_visible = true;
        phenotype_repaint(phenotype::detail::g_app.scroll_y);
    }

    __attribute__((export_name("phenotype_handle_tab")))
    void phenotype_handle_tab(unsigned int reverse) {
        auto& app = phenotype::detail::g_app;
        if (app.focusable_ids.empty()) return;
        int n = static_cast<int>(app.focusable_ids.size());
        // Find current index
        int idx = -1;
        for (int i = 0; i < n; ++i) {
            if (app.focusable_ids[i] == app.focused_id) { idx = i; break; }
        }
        if (reverse)
            idx = (idx <= 0) ? n - 1 : idx - 1;
        else
            idx = (idx < 0 || idx >= n - 1) ? 0 : idx + 1;
        app.focused_id = app.focusable_ids[idx];
        app.caret_visible = true;
        phenotype_repaint(app.scroll_y);
    }

    __attribute__((export_name("phenotype_toggle_caret")))
    void phenotype_toggle_caret() {
        auto& app = phenotype::detail::g_app;
        app.caret_visible = !app.caret_visible;
        phenotype_repaint(app.scroll_y);
    }

    __attribute__((export_name("phenotype_handle_key")))
    void phenotype_handle_key(unsigned int key_type, unsigned int codepoint) {
        auto& app = phenotype::detail::g_app;
        auto it = app.input_nodes.find(app.focused_id);
        if (it == app.input_nodes.end() || !it->second->input_state) return;

        auto* state = static_cast<phenotype::Trait<std::string>*>(it->second->input_state);
        auto val = state->value();

        switch (key_type) {
            case 0: { // character
                char buf[4];
                unsigned int len = 0;
                if (codepoint < 0x80) {
                    buf[0] = static_cast<char>(codepoint); len = 1;
                } else if (codepoint < 0x800) {
                    buf[0] = static_cast<char>(0xC0 | (codepoint >> 6));
                    buf[1] = static_cast<char>(0x80 | (codepoint & 0x3F)); len = 2;
                } else if (codepoint < 0x10000) {
                    buf[0] = static_cast<char>(0xE0 | (codepoint >> 12));
                    buf[1] = static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
                    buf[2] = static_cast<char>(0x80 | (codepoint & 0x3F)); len = 3;
                } else {
                    buf[0] = static_cast<char>(0xF0 | (codepoint >> 18));
                    buf[1] = static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F));
                    buf[2] = static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
                    buf[3] = static_cast<char>(0x80 | (codepoint & 0x3F)); len = 4;
                }
                val.append(buf, len);
                break;
            }
            case 1: // backspace
                if (!val.empty()) {
                    // Remove last UTF-8 character
                    auto i = val.size() - 1;
                    while (i > 0 && (static_cast<unsigned char>(val[i]) & 0xC0) == 0x80) --i;
                    val.erase(i);
                }
                break;
            default:
                return; // other keys not yet handled
        }

        app.caret_visible = true; // reset blink on input
        state->set(std::move(val));
    }
}
