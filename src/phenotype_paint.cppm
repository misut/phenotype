module;
#include <cstring>
#include <string>
export module phenotype.paint;

import phenotype.types;
import phenotype.state;

// Host imports
extern "C" __attribute__((import_module("phenotype"), import_name("flush")))
void phenotype_flush(void);

extern "C" __attribute__((import_module("phenotype"), import_name("measure_text")))
float phenotype_measure_text(float font_size, unsigned int mono,
                             char const* text, unsigned int len);

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

namespace phenotype::detail {

inline void ensure(unsigned int needed) {
    if (phenotype_cmd_len + needed > BUF_SIZE) {
        phenotype_flush();
        phenotype_cmd_len = 0;
    }
}

inline void write_u32(unsigned int value) {
    auto* p = reinterpret_cast<unsigned int*>(&phenotype_cmd_buf[phenotype_cmd_len]);
    *p = value;
    phenotype_cmd_len += 4;
}

inline void write_f32(float value) {
    unsigned int bits;
    std::memcpy(&bits, &value, 4);
    write_u32(bits);
}

inline void write_bytes(char const* data, unsigned int len) {
    for (unsigned int i = 0; i < len; ++i)
        phenotype_cmd_buf[phenotype_cmd_len++] = static_cast<unsigned char>(data[i]);
    while (phenotype_cmd_len % 4 != 0)
        phenotype_cmd_buf[phenotype_cmd_len++] = 0;
}

inline unsigned int padded(unsigned int len) {
    return (len + 3) & ~3u;
}

} // namespace phenotype::detail

export namespace phenotype {

// Draw command emitters

inline void emit_clear(Color c) {
    detail::ensure(8);
    detail::write_u32(static_cast<unsigned int>(Cmd::Clear));
    detail::write_u32(c.packed());
}

inline void emit_fill_rect(float x, float y, float w, float h, Color c) {
    detail::ensure(24);
    detail::write_u32(static_cast<unsigned int>(Cmd::FillRect));
    detail::write_f32(x);
    detail::write_f32(y);
    detail::write_f32(w);
    detail::write_f32(h);
    detail::write_u32(c.packed());
}

inline void emit_stroke_rect(float x, float y, float w, float h, float line_width, Color c) {
    detail::ensure(28);
    detail::write_u32(static_cast<unsigned int>(Cmd::StrokeRect));
    detail::write_f32(x);
    detail::write_f32(y);
    detail::write_f32(w);
    detail::write_f32(h);
    detail::write_f32(line_width);
    detail::write_u32(c.packed());
}

inline void emit_round_rect(float x, float y, float w, float h, float radius, Color c) {
    detail::ensure(28);
    detail::write_u32(static_cast<unsigned int>(Cmd::RoundRect));
    detail::write_f32(x);
    detail::write_f32(y);
    detail::write_f32(w);
    detail::write_f32(h);
    detail::write_f32(radius);
    detail::write_u32(c.packed());
}

inline void emit_draw_text(float x, float y, float font_size, unsigned int mono,
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

inline void emit_draw_line(float x1, float y1, float x2, float y2, float thickness, Color c) {
    detail::ensure(28);
    detail::write_u32(static_cast<unsigned int>(Cmd::DrawLine));
    detail::write_f32(x1);
    detail::write_f32(y1);
    detail::write_f32(x2);
    detail::write_f32(y2);
    detail::write_f32(thickness);
    detail::write_u32(c.packed());
}

inline void emit_hit_region(float x, float y, float w, float h,
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

inline void flush() {
    if (phenotype_cmd_len == 0) return;
    phenotype_flush();
    phenotype_cmd_len = 0;
}

} // namespace phenotype

// ============================================================
// paint_node — walk layout tree, emit draw commands
// ============================================================

export namespace phenotype::detail {

inline void paint_node(LayoutNode* node, float ox, float oy, float scroll_y,
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

} // namespace phenotype::detail
