module;
#include <cstddef>
#include <map>
#include <string>
#include <tuple>
#include <vector>
export module phenotype.layout;

import phenotype.types;
import phenotype.state;
import phenotype.diag;

// Host imports — declared per-module so each TU resolves them at link time.
extern "C" __attribute__((import_module("phenotype"), import_name("measure_text")))
float phenotype_measure_text(float font_size, unsigned int mono,
                             char const* text, unsigned int len);

namespace phenotype::detail {

// ============================================================
// measure_text cache
// ============================================================
//
// The host measure_text import is on the JS↔WASM trampoline, the most
// expensive call type byte-for-byte phenotype makes. Most measurements
// repeat across rebuilds — the same button label, the same heading,
// the same code-block strings — so a (font_size, mono, text) → width
// cache amortizes them to one host call per distinct tuple per wasm
// instance.
//
// The cache is unbounded by design: a typical phenotype app has on
// the order of a few hundred distinct text snippets. If a future
// caller produces unbounded text strings (e.g. echoing user input
// into a measured leaf), revisit with an LRU. For now simplicity
// wins — see `phenotype.host.measure_text_calls` (real misses) and
// `phenotype.host.measure_text_cache_hits` in the diag snapshot to
// confirm the cache is doing its job in production.
//
// Theme changes will need to call `clear_measure_cache()` once the
// custom theming roadmap item lands; until then theme is const so
// the cache is effectively process-lifetime.

// Cache key is just a (font_size, mono, text) tuple. std::map is used
// instead of std::unordered_map because clang 22's libc++ unordered_map
// references __hash_memory, which wasn't in the macOS system libc++
// the test binaries link against. std::map keeps lookup O(log n) but
// the cache has on the order of a few hundred entries — log n is
// dwarfed by the JS↔WASM trampoline cost we're avoiding.
using MeasureKey = std::tuple<float, unsigned int, std::string>;

// Heap-bound reference for the same reason the diag instruments use
// the pattern: wasi-sdk's crt1-command.o runs __cxa_finalize after
// _start() returns, and we still want the cache alive for every
// JS-driven rebuild after that. See phenotype_diag.cppm:262 for the
// precedent.
inline std::map<MeasureKey, float>& measure_cache() {
    static std::map<MeasureKey, float>& m = *new std::map<MeasureKey, float>();
    return m;
}

// Cached wrapper around the host measure_text import. On hit the
// width is returned without crossing the trampoline and the
// `phenotype.host.measure_text_cache_hits` counter is bumped instead
// of `measure_text_calls`. The latter therefore once again reflects
// real JS↔WASM cost — exactly the metric the diag baseline in PR #26
// was supposed to optimize.
inline float measure_text_cached(float font_size, unsigned int mono,
                                 char const* text, unsigned int len) {
    auto& cache = measure_cache();
    MeasureKey key{font_size, mono, std::string(text, len)};
    if (auto it = cache.find(key); it != cache.end()) {
        metrics::inst::measure_text_cache_hits.add();
        return it->second;
    }
    metrics::inst::measure_text_calls.add();
    float w = phenotype_measure_text(font_size, mono, text, len);
    cache.emplace(std::move(key), w);
    return w;
}

} // namespace phenotype::detail

export namespace phenotype::detail {

// Test-only helper — exposed via the export namespace so tests
// outside the module can reset the cache between assertions. Real
// runtime callers should never need it: the cache is process-
// lifetime and only theme changes invalidate it (which the future
// custom-theming PR will hook in here).
inline void clear_measure_cache() noexcept {
    measure_cache().clear();
}

struct TextLayout {
    std::vector<std::string> lines;
    float width;
    float height;
};

inline float measure(str text, float font_size, bool mono) {
    return measure_text_cached(font_size, mono ? 1 : 0, text.data, text.len);
}

inline TextLayout layout_text(std::string const& text, float font_size, bool mono,
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
                : measure_text_cached(font_size, mono ? 1 : 0, " ", 1);
            float word_width = measure_text_cached(
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

inline void layout_node(NodeHandle node_h, float available_width) {
    auto& node = node_at(node_h);
    auto const& s = node.style;
    float content_width = available_width;

    // Apply max_width
    if (s.max_width > 0 && content_width > s.max_width)
        content_width = s.max_width;

    node.width = content_width;

    float inner_width = content_width - s.padding[1] - s.padding[3]; // right, left

    // Text leaf — compute and cache word-wrapped lines
    if (!node.text.empty() && node.children.empty()) {
        float line_height = node.font_size * g_app.theme.line_height_ratio;
        auto tl = layout_text(node.text, node.font_size, node.mono, inner_width, line_height);
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

    // Container layout
    if (s.flex_direction == FlexDirection::Column) {
        // First pass: layout children to compute total height
        float total_children_h = 0;
        unsigned int nc = static_cast<unsigned int>(node.children.size());
        for (unsigned int i = 0; i < nc; ++i) {
            auto child_h = node.children[i];
            layout_node(child_h, inner_width);
            auto& child = node_at(child_h);
            total_children_h += child.height;
            if (i + 1 < nc) total_children_h += s.gap;
        }

        // Main axis (vertical) alignment
        float y = s.padding[0];
        float effective_gap = s.gap;
        if (s.main_align == MainAxisAlignment::SpaceBetween && nc > 1) {
            float avail_h = node.height > 0
                ? node.height - s.padding[0] - s.padding[2]
                : total_children_h;
            float children_only = total_children_h - s.gap * static_cast<float>(nc - 1);
            effective_gap = (avail_h - children_only) / static_cast<float>(nc - 1);
        }
        // (Center/End only meaningful if node has a fixed or known height)

        for (unsigned int i = 0; i < nc; ++i) {
            auto& child = node_at(node.children[i]);
            // Cross axis (horizontal) alignment
            switch (s.cross_align) {
                case CrossAxisAlignment::Center:
                    child.x = s.padding[3] + (inner_width - child.width) / 2;
                    break;
                case CrossAxisAlignment::End:
                    child.x = s.padding[3] + inner_width - child.width;
                    break;
                default: // Start, Stretch
                    child.x = s.padding[3];
                    break;
            }
            child.y = y;
            y += child.height;
            if (i + 1 < nc) y += effective_gap;
        }
        node.height = y + s.padding[2];
    } else {
        // Row: intrinsic-width children get their measured size,
        // remaining space goes to the last flexible child.
        unsigned int n = static_cast<unsigned int>(node.children.size());
        float total_gap = (n > 1) ? s.gap * static_cast<float>(n - 1) : 0;

        // First pass: measure intrinsic widths for text leaves
        float used = total_gap;
        int flex_index = -1;
        for (unsigned int i = 0; i < n; ++i) {
            auto& child = node_at(node.children[i]);
            bool is_text_leaf = !child.text.empty() && child.children.empty();
            bool has_max_width = child.style.max_width > 0;
            if (is_text_leaf) {
                float measured = measure_text_cached(
                    child.font_size, child.mono ? 1 : 0,
                    child.text.c_str(), static_cast<unsigned int>(child.text.size()));
                float w = measured + child.style.padding[1] + child.style.padding[3];
                child.width = w;
                used += w;
            } else if (has_max_width) {
                // Container child with an explicit max_width is treated as
                // having a fixed intrinsic width: row layout sizes it to
                // exactly that value, leaving the flex slot for an unsized
                // sibling. Used by layout::sized_box() to build columns of
                // fixed-width children (e.g. the docs Examples section's
                // code-on-left / live-on-right pairs).
                child.width = child.style.max_width;
                used += child.width;
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
            auto child_h = node.children[i];
            auto& child = node_at(child_h);
            float cw = (static_cast<int>(i) == flex_index)
                ? remaining + child.width
                : child.width;
            layout_node(child_h, cw);
            // Re-fetch in case the recursive call invalidated nothing
            // (it doesn't — layout doesn't alloc — but be explicit).
            auto& child2 = node_at(child_h);
            total_used_w += child2.width;
            if (child2.height > max_h) max_h = child2.height;
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
            auto& child = node_at(node.children[i]);
            child.x = x;
            // Cross axis (vertical) alignment
            switch (s.cross_align) {
                case CrossAxisAlignment::Center:
                    child.y = s.padding[0] + (max_h - child.height) / 2;
                    break;
                case CrossAxisAlignment::End:
                    child.y = s.padding[0] + max_h - child.height;
                    break;
                default: // Start, Stretch
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
