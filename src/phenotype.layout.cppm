module;
#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <tuple>
#include <vector>
#ifdef __wasi__
#include "phenotype_host.h"
#endif
export module phenotype.layout;

import phenotype.types;
import phenotype.state;
import phenotype.diag;
import phenotype.host;

namespace phenotype::detail {

// ============================================================
// measure_text cache
// ============================================================
//
// Most measurements repeat across rebuilds — the same button label,
// the same heading, the same code-block strings — so a
// (font_size, mono, text) → width cache amortizes them to one host
// call per distinct tuple per process.
//
// The cache is unbounded by design: a typical phenotype app has on
// the order of a few hundred distinct text snippets. If a future
// caller produces unbounded text strings (e.g. echoing user input
// into a measured leaf), revisit with an LRU.

// Cache key is just a (font_size, mono, text) tuple. std::map is used
// instead of std::unordered_map because clang 22's libc++ unordered_map
// references __hash_memory, which wasn't in the macOS system libc++
// the test binaries link against. std::map keeps lookup O(log n) but
// the cache has on the order of a few hundred entries — log n is
// dwarfed by the host call cost we're avoiding.
using MeasureKey = std::tuple<float, unsigned int, std::string>;

struct MeasureKeyLess {
    bool operator()(MeasureKey const& lhs, MeasureKey const& rhs) const noexcept {
        if (std::get<0>(lhs) < std::get<0>(rhs)) return true;
        if (std::get<0>(rhs) < std::get<0>(lhs)) return false;
        if (std::get<1>(lhs) < std::get<1>(rhs)) return true;
        if (std::get<1>(rhs) < std::get<1>(lhs)) return false;
        return std::get<2>(lhs) < std::get<2>(rhs);
    }
};

// Heap-bound reference for the same reason the diag instruments use
// the pattern: wasi-sdk's crt1-command.o runs __cxa_finalize after
// _start() returns, and we still want the cache alive for every
// JS-driven rebuild after that.
inline std::map<MeasureKey, float, MeasureKeyLess>& measure_cache() {
    static std::map<MeasureKey, float, MeasureKeyLess>& m
        = *new std::map<MeasureKey, float, MeasureKeyLess>();
    return m;
}

// Non-template cache helpers — compiled once in this TU to avoid
// wasi-sdk's cross-module operator new ambiguity with std::map.
inline float* cache_find(float fs, unsigned int mono,
                         char const* text, unsigned int len) {
    auto& cache = measure_cache();
    MeasureKey key{fs, mono, std::string(text, len)};
    auto it = cache.find(key);
    return it != cache.end() ? &it->second : nullptr;
}

inline void cache_insert(float fs, unsigned int mono,
                         char const* text, unsigned int len, float w) {
    measure_cache().insert({MeasureKey{fs, mono, std::string(text, len)}, w});
}

} // namespace phenotype::detail

export namespace phenotype::detail {

template <text_measurer M>
float measure_text_cached(M const& measurer, float font_size,
                          unsigned int mono, char const* text,
                          unsigned int len) {
    if (auto* cached = cache_find(font_size, mono, text, len)) {
        metrics::inst::measure_text_cache_hits.add();
        return *cached;
    }
    metrics::inst::measure_text_calls.add();
    float w = measurer.measure_text(font_size, mono, text, len);
    cache_insert(font_size, mono, text, len, w);
    return w;
}

inline void clear_measure_cache() noexcept {
    measure_cache().clear();
}

struct TextLayout {
    std::vector<std::string> lines;
    float width;
    float height;
};

template <text_measurer M>
float measure(M const& measurer, str text, float font_size, bool mono) {
    return measure_text_cached(measurer, font_size, mono ? 1 : 0, text.data, text.len);
}

template <text_measurer M>
TextLayout layout_text(M const& measurer, std::string const& text,
                       float font_size, bool mono, float max_width,
                       float line_height) {
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
                : measure_text_cached(measurer, font_size, mono ? 1 : 0, " ", 1);
            float word_width = measure_text_cached(
                measurer, font_size, mono ? 1 : 0, word.c_str(),
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

// Compare the fields that affect layout or cached paint output. If all
// match AND child count is the same, the subtree can safely reuse the
// previous frame's layout and paint byte range.
inline bool color_equal(Color const& a, Color const& b) noexcept {
    return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

inline bool layout_props_equal(LayoutNode const& a, LayoutNode const& b) {
    return a.text == b.text
        && a.font_size == b.font_size
        && a.mono == b.mono
        && a.image_url == b.image_url
        && color_equal(a.background, b.background)
        && color_equal(a.text_color, b.text_color)
        && a.border_radius == b.border_radius
        && color_equal(a.border_color, b.border_color)
        && a.border_width == b.border_width
        && a.decoration == b.decoration
        && a.cursor_type == b.cursor_type
        && a.focusable == b.focusable
        && a.interaction_role == b.interaction_role
        && color_equal(a.hover_background, b.hover_background)
        && color_equal(a.hover_text_color, b.hover_text_color)
        && a.is_input == b.is_input
        && a.placeholder == b.placeholder
        && a.style.flex_direction == b.style.flex_direction
        && a.style.main_align == b.style.main_align
        && a.style.cross_align == b.style.cross_align
        && a.style.text_align == b.style.text_align
        && a.style.gap == b.style.gap
        && a.style.padding[0] == b.style.padding[0]
        && a.style.padding[1] == b.style.padding[1]
        && a.style.padding[2] == b.style.padding[2]
        && a.style.padding[3] == b.style.padding[3]
        && a.style.max_width == b.style.max_width
        && a.style.fixed_height == b.style.fixed_height
        && a.is_grid_container == b.is_grid_container
        && a.grid_row_height == b.grid_row_height
        && a.grid_columns == b.grid_columns;
}

inline bool diff_and_copy_layout(NodeHandle old_h, NodeHandle new_h,
                                 Arena& old_a, Arena& new_a);

// Keyed-list salvage: after the structural diff has returned false for
// a parent, look for any new child whose key matches a previous old
// child anywhere in the sibling list. Recurse into matched pairs so
// the child (and its entire subtree) can still set its own
// layout_valid + paint_* state even though positional diff failed.
//
// Only runs when `new_n->children_keyed` is set. Children that already
// matched structurally (layout_valid == true) are skipped — the map
// lookup must not overwrite their already-validated state with a
// different old node. Duplicate keys among siblings: first-seen wins
// (later duplicates skip the lookup and miss the salvage). Using
// std::map keyed by uint32_t avoids the wasi-sdk std::unordered_map
// constraint called out above at measure_cache.
inline void keyed_salvage(LayoutNode const& old_n, LayoutNode const& new_n,
                          Arena& old_a, Arena& new_a) {
    if (!new_n.children_keyed) return;
    std::map<std::uint32_t, NodeHandle> old_by_key;
    for (auto old_child_h : old_n.children) {
        auto const* old_child = old_a.get(old_child_h);
        if (!old_child || old_child->key == LayoutNode::unkeyed_key) continue;
        old_by_key.emplace(old_child->key, old_child_h);
    }
    if (old_by_key.empty()) return;
    metrics::inst::keyed_lists_matched.add();
    for (auto new_child_h : new_n.children) {
        auto* new_child = new_a.get(new_child_h);
        if (!new_child) continue;
        if (new_child->layout_valid) continue;       // already matched structurally
        if (new_child->key == LayoutNode::unkeyed_key) continue;
        auto it = old_by_key.find(new_child->key);
        if (it == old_by_key.end()) continue;
        if (diff_and_copy_layout(it->second, new_child_h, old_a, new_a))
            metrics::inst::keyed_children_matched.add();
    }
}

// Post-order subtree diff: returns true if the ENTIRE subtree matches.
// When true, copies computed layout from old → new and sets
// layout_valid = true so layout_node() skips the subtree.
//
// Siblings are diffed independently — a failure in one child does not
// short-circuit the loop, so later siblings still get a chance to set
// their own layout_valid / paint cache state. The parent's return value
// still reflects whole-subtree match (required for paint-cache blit
// correctness at this level), but children below a non-matching
// ancestor can still independently rescue layout + paint bytes from the
// previous frame.
//
// For parents whose children carry keys (children_keyed == true), a
// keyed salvage pass runs after any structural failure so that
// reordering / insertion / deletion in a keyed list preserves per-item
// layout_valid + paint_* state without requiring positional alignment.
inline bool diff_and_copy_layout(NodeHandle old_h, NodeHandle new_h,
                                 Arena& old_a, Arena& new_a) {
    auto* old_n = old_a.get(old_h);
    auto* new_n = new_a.get(new_h);
    if (!old_n || !new_n) return false;

    if (!layout_props_equal(*old_n, *new_n)) {
        keyed_salvage(*old_n, *new_n, old_a, new_a);
        return false;
    }
    if (old_n->children.size() != new_n->children.size()) {
        keyed_salvage(*old_n, *new_n, old_a, new_a);
        return false;
    }

    bool all_matched = true;
    for (std::size_t i = 0; i < new_n->children.size(); ++i) {
        if (!diff_and_copy_layout(old_n->children[i], new_n->children[i],
                                  old_a, new_a))
            all_matched = false;
    }
    if (!all_matched) {
        keyed_salvage(*old_n, *new_n, old_a, new_a);
        return false;
    }

    new_n->x = old_n->x;
    new_n->y = old_n->y;
    new_n->width = old_n->width;
    new_n->height = old_n->height;
    new_n->text_lines = std::move(old_n->text_lines);
    new_n->paint_offset = old_n->paint_offset;
    new_n->paint_length = old_n->paint_length;
    new_n->paint_ax = old_n->paint_ax;
    new_n->paint_ay = old_n->paint_ay;
    new_n->paint_callback_mask = old_n->paint_callback_mask;
    new_n->paint_dynamic = old_n->paint_dynamic || static_cast<bool>(new_n->paint_fn);
    new_n->paint_valid = old_n->paint_valid;
    new_n->layout_valid = true;
    return true;
}

template <text_measurer M>
void layout_node(M const& measurer, NodeHandle node_h, float available_width) {
    auto& node = node_at(node_h);
    auto const& s = node.style;
    float content_width = available_width;

    if (s.max_width > 0 && content_width > s.max_width)
        content_width = s.max_width;

    // Cache shortcut: a node copied verbatim from the previous frame
    // (diff_and_copy_layout) has layout_valid==true with its previously
    // computed width. Reusing it is only safe when the available width
    // (capped by max_width) hasn't changed — otherwise the cached width
    // is stale, e.g. after a window resize where the parent now passes
    // a different content area to the same subtree.
    if (node.layout_valid && node.width == content_width) {
        metrics::inst::layout_nodes_skipped.add();
        return;
    }
    node.layout_valid = false;
    metrics::inst::layout_nodes_computed.add();

    node.width = content_width;

    float inner_width = content_width - s.padding[1] - s.padding[3];

    // Text leaf
    if (!node.text.empty() && node.children.empty()) {
        float line_height = node.font_size * g_app.theme.line_height_ratio;
        auto tl = layout_text(measurer, node.text, node.font_size, node.mono,
                              inner_width, line_height);
        node.text_lines = std::move(tl.lines);
        node.height = tl.height + s.padding[0] + s.padding[2];
        return;
    }

    // Fixed height (Spacer, Divider)
    if (s.fixed_height >= 0 && node.children.empty()) {
        node.height = s.fixed_height + s.padding[0] + s.padding[2];
        return;
    }

    // Auto-center nodes with max_width
    if (s.max_width > 0 && content_width < available_width) {
        node.x += (available_width - content_width) / 2;
    }

    // Grid container — rigid track layout (no flex distribution).
    // Children are placed row-major into a fixed `grid_columns × n_rows`
    // matrix with `style.gap` between tracks in both directions.
    if (node.is_grid_container) {
        unsigned int n_cols = static_cast<unsigned int>(node.grid_columns.size());
        unsigned int n_children = static_cast<unsigned int>(node.children.size());
        if (n_cols == 0 || n_children == 0) {
            node.height = s.padding[0] + s.padding[2];
            return;
        }
        unsigned int n_rows = (n_children + n_cols - 1) / n_cols;
        float gap = s.gap;

        // Per-column x offsets (within inner content area).
        std::vector<float> col_x;
        col_x.reserve(n_cols);
        {
            float x = s.padding[3];
            for (unsigned int c = 0; c < n_cols; ++c) {
                col_x.push_back(x);
                x += node.grid_columns[c] + gap;
            }
        }

        // First pass: lay out each child with its track width, then
        // collect per-row heights (either grid_row_height when set, or
        // the tallest measured child in that row).
        std::vector<float> row_heights(n_rows,
            node.grid_row_height > 0 ? node.grid_row_height : 0);
        for (unsigned int i = 0; i < n_children; ++i) {
            unsigned int col = i % n_cols;
            unsigned int row = i / n_cols;
            layout_node(measurer, node.children[i], node.grid_columns[col]);
            auto& child = node_at(node.children[i]);
            child.width = node.grid_columns[col];
            if (node.grid_row_height > 0) {
                child.height = node.grid_row_height;
            } else if (child.height > row_heights[row]) {
                row_heights[row] = child.height;
            }
        }

        // Second pass: compute y per row and position each child.
        std::vector<float> row_y;
        row_y.reserve(n_rows);
        {
            float y = s.padding[0];
            for (unsigned int r = 0; r < n_rows; ++r) {
                row_y.push_back(y);
                y += row_heights[r] + gap;
            }
        }
        for (unsigned int i = 0; i < n_children; ++i) {
            unsigned int col = i % n_cols;
            unsigned int row = i / n_cols;
            auto& child = node_at(node.children[i]);
            child.x = col_x[col];
            child.y = row_y[row];
            if (node.grid_row_height <= 0) {
                // Auto-row mode: align all cells in the row to its tallest.
                child.height = row_heights[row];
            }
        }

        // Container height = sum of row heights + gaps + vertical padding.
        float total_h = 0;
        for (float h : row_heights) total_h += h;
        if (n_rows > 1) total_h += static_cast<float>(n_rows - 1) * gap;
        node.height = total_h + s.padding[0] + s.padding[2];
        return;
    }

    // Container layout
    if (s.flex_direction == FlexDirection::Column) {
        float total_children_h = 0;
        unsigned int nc = static_cast<unsigned int>(node.children.size());
        for (unsigned int i = 0; i < nc; ++i) {
            auto child_h = node.children[i];
            layout_node(measurer, child_h, inner_width);
            auto& child = node_at(child_h);
            total_children_h += child.height;
            if (i + 1 < nc) total_children_h += s.gap;
        }

        float y = s.padding[0];
        float effective_gap = s.gap;
        if (s.main_align == MainAxisAlignment::SpaceBetween && nc > 1) {
            float avail_h = node.height > 0
                ? node.height - s.padding[0] - s.padding[2]
                : total_children_h;
            float children_only = total_children_h - s.gap * static_cast<float>(nc - 1);
            effective_gap = (avail_h - children_only) / static_cast<float>(nc - 1);
        }

        for (unsigned int i = 0; i < nc; ++i) {
            auto& child = node_at(node.children[i]);
            switch (s.cross_align) {
                case CrossAxisAlignment::Center:
                    child.x = s.padding[3] + (inner_width - child.width) / 2;
                    break;
                case CrossAxisAlignment::End:
                    child.x = s.padding[3] + inner_width - child.width;
                    break;
                default:
                    child.x = s.padding[3];
                    break;
            }
            child.y = y;
            y += child.height;
            if (i + 1 < nc) y += effective_gap;
        }
        node.height = y + s.padding[2];
    } else {
        // Row
        unsigned int n = static_cast<unsigned int>(node.children.size());
        float total_gap = (n > 1) ? s.gap * static_cast<float>(n - 1) : 0;

        float used = total_gap;
        int flex_index = -1;
        int wrap_text_index = -1;
        float wrap_text_width = 0;
        for (unsigned int i = 0; i < n; ++i) {
            auto& child = node_at(node.children[i]);
            bool is_text_leaf = !child.text.empty() && child.children.empty();
            bool has_max_width = child.style.max_width > 0;
            if (is_text_leaf) {
                float measured = measure_text_cached(
                    measurer, child.font_size, child.mono ? 1 : 0,
                    child.text.c_str(), static_cast<unsigned int>(child.text.size()));
                float w = measured + child.style.padding[1] + child.style.padding[3];
                child.width = w;
                used += w;
                wrap_text_index = static_cast<int>(i);
                wrap_text_width = w;
            } else if (has_max_width) {
                child.width = child.style.max_width;
                used += child.width;
            } else {
                flex_index = static_cast<int>(i);
            }
        }
        bool flex_is_wrap_text = false;
        if (flex_index < 0 && wrap_text_index >= 0) {
            flex_index = wrap_text_index;
            used -= wrap_text_width;
            flex_is_wrap_text = true;
        }
        if (flex_index < 0) flex_index = static_cast<int>(n) - 1;

        float remaining = inner_width - used;
        if (remaining < 0) remaining = 0;

        float total_used_w = 0;
        float max_h = 0;
        for (unsigned int i = 0; i < n; ++i) {
            auto child_h = node.children[i];
            auto& child = node_at(child_h);
            float cw = child.width;
            if (static_cast<int>(i) == flex_index) {
                if (flex_is_wrap_text) {
                    cw = remaining;
                    if (child.width < cw) cw = child.width;
                } else {
                    cw = remaining + child.width;
                }
            }
            layout_node(measurer, child_h, cw);
            auto& child2 = node_at(child_h);
            total_used_w += child2.width;
            if (child2.height > max_h) max_h = child2.height;
        }

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
            auto& child = node_at(node.children[i]);
            child.x = x;
            switch (s.cross_align) {
                case CrossAxisAlignment::Center:
                    child.y = s.padding[0] + (max_h - child.height) / 2;
                    break;
                case CrossAxisAlignment::End:
                    child.y = s.padding[0] + max_h - child.height;
                    break;
                default:
                    child.y = s.padding[0];
                    break;
            }
            x += child.width;
            if (i + 1 < n) x += effective_gap;
        }
        node.height = max_h + s.padding[0] + s.padding[2];
    }
}

} // namespace phenotype::detail

// ---- WASM non-template wrappers ----
// Instantiates templates within this TU using phenotype_host.h to avoid
// a wasi-sdk clang 22 crash on cross-module template instantiation.
#ifdef __wasi__
namespace phenotype::detail {
    struct wasi_measurer {
        float measure_text(float fs, unsigned int m,
                           char const* t, unsigned int l) const {
            return phenotype_measure_text(fs, m, t, l);
        }
    };
    inline wasi_measurer g_wasi_measurer;
}

export namespace phenotype::detail {

inline float measure(str text, float font_size, bool mono) {
    return measure_text_cached(g_wasi_measurer, font_size, mono ? 1 : 0,
                               text.data, text.len);
}

inline void layout_node(NodeHandle h, float w) {
    layout_node(g_wasi_measurer, h, w);
}

} // namespace phenotype::detail
#endif
