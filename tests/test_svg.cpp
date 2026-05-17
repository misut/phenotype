#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string_view>

import phenotype.icons;
import phenotype.svg;
import phenotype.types;

using namespace phenotype;

namespace {

struct CapturePainter final : Painter {
    unsigned int fills = 0;
    unsigned int strokes = 0;
    unsigned int last_verb_count = 0;
    float last_thickness = 0.0f;
    Color last_color = {};
    float first_move_x = 0.0f;
    float first_move_y = 0.0f;
    float first_line_x = 0.0f;
    float first_line_y = 0.0f;

    void line(float, float, float, float, float, Color) override {}
    void text(float, float, char const*, unsigned int, float, Color,
              FontSpec, float) override {}
    float measure_text(char const*, unsigned int, float, FontSpec) const override {
        return 0.0f;
    }
    FontMetrics font_metrics(float, FontSpec) const override {
        return {};
    }
    void arc(float, float, float, float, float, float, Color) override {}
    void stroke_path(PathBuilder const& path, float thickness, Color color) override {
        ++strokes;
        last_verb_count = path.verb_count;
        capture_first_segment(path);
        last_thickness = thickness;
        last_color = color;
    }
    void fill_path(PathBuilder const& path, Color color) override {
        ++fills;
        last_verb_count = path.verb_count;
        capture_first_segment(path);
        last_color = color;
    }

private:
    static float read_f32(PathBuilder const& path, unsigned int index) {
        float value = 0.0f;
        unsigned int bits = path.verbs[index];
        std::memcpy(&value, &bits, 4);
        return value;
    }

    void capture_first_segment(PathBuilder const& path) {
        if (path.verbs.size() >= 6
            && static_cast<PathVerb>(path.verbs[0]) == PathVerb::MoveTo
            && static_cast<PathVerb>(path.verbs[3]) == PathVerb::LineTo) {
            first_move_x = read_f32(path, 1);
            first_move_y = read_f32(path, 2);
            first_line_x = read_f32(path, 4);
            first_line_y = read_f32(path, 5);
        }
    }
};

unsigned int count_path_verb(PathBuilder const& path, PathVerb needle) {
    unsigned int count = 0;
    unsigned int i = 0;
    auto skip = [&](unsigned int words) {
        if (i + words <= path.verbs.size()) {
            i += words;
            return true;
        }
        i = static_cast<unsigned int>(path.verbs.size());
        return false;
    };
    while (i < path.verbs.size()) {
        auto const tag = static_cast<PathVerb>(path.verbs[i++]);
        if (tag == needle)
            ++count;
        switch (tag) {
        case PathVerb::MoveTo:
        case PathVerb::LineTo:
            if (!skip(2)) return count;
            break;
        case PathVerb::QuadTo:
            if (!skip(4)) return count;
            break;
        case PathVerb::CubicTo:
            if (!skip(6)) return count;
            break;
        case PathVerb::ArcTo:
            if (!skip(5)) return count;
            break;
        case PathVerb::Close:
            break;
        default:
            return count;
        }
    }
    return count;
}

void test_svg_path_subset() {
    auto doc = svg::parse(R"SVG(
        <svg viewBox="0 0 24 24">
          <g fill="none" stroke="currentColor" stroke-width="2">
            <path d="M4 4 L20 4 H21 V8 Q18 12 21 16 C18 20 8 20 4 16 Z"/>
          </g>
        </svg>
    )SVG");
    assert(doc.view_width == 24.0f);
    assert(doc.view_height == 24.0f);
    assert(doc.shapes.size() == 1);
    assert(!doc.has_diagnostics());

    CapturePainter painter;
    svg::paint(painter, doc, 0.0f, 0.0f, 24.0f, 24.0f,
               svg::RenderOptions{{0, 122, 255, 255}, true});
    assert(painter.fills == 0);
    assert(painter.strokes == 1);
    assert(painter.last_verb_count == 7);
    assert(std::abs(painter.last_thickness - 2.0f) < 0.001f);
    assert(painter.last_color.r == 0);
    assert(painter.last_color.g == 122);
    assert(painter.last_color.b == 255);

    std::puts("PASS: SVG path subset parses and paints currentColor");
}

void test_svg_basic_shapes() {
    auto doc = svg::parse(R"SVG(
        <svg viewBox="0 0 32 32" fill="none" stroke="#1c1c1e">
          <rect x="2" y="2" width="8" height="8" rx="2"/>
          <circle cx="18" cy="6" r="4"/>
          <ellipse cx="18" cy="18" rx="5" ry="3"/>
          <line x1="2" y1="18" x2="10" y2="26"/>
          <polygon points="14 24 20 24 17 29"/>
        </svg>
    )SVG");
    assert(doc.shapes.size() == 5);
    assert(!doc.has_diagnostics());

    CapturePainter painter;
    svg::paint(painter, doc, 0.0f, 0.0f, 64.0f, 64.0f);
    assert(painter.fills == 0);
    assert(painter.strokes == 5);

    std::puts("PASS: SVG rect/circle/ellipse/line/polygon parse");
}

void test_svg_circle_uses_arc_path() {
    auto circle = svg::parse(
        R"SVG(<svg viewBox="0 0 24 24"><circle cx="12" cy="12" r="8"/></svg>)SVG");
    assert(circle.shapes.size() == 1);
    assert(!circle.has_diagnostics());
    assert(count_path_verb(circle.shapes[0].path, PathVerb::ArcTo) == 4);
    assert(count_path_verb(circle.shapes[0].path, PathVerb::CubicTo) == 0);

    auto ellipse = svg::parse(
        R"SVG(<svg viewBox="0 0 24 24"><ellipse cx="12" cy="12" rx="8" ry="5"/></svg>)SVG");
    assert(ellipse.shapes.size() == 1);
    assert(!ellipse.has_diagnostics());
    assert(count_path_verb(ellipse.shapes[0].path, PathVerb::ArcTo) == 0);
    assert(count_path_verb(ellipse.shapes[0].path, PathVerb::CubicTo) == 4);

    std::puts("PASS: SVG circles lower to native arc paths");
}

void test_svg_transform_subset() {
    auto doc = svg::parse(R"SVG(
        <svg viewBox="0 0 24 24" fill="none" stroke="currentColor">
          <g transform="translate(2 3)">
            <path transform="scale(2)" d="M1 1 L3 1"/>
          </g>
        </svg>
    )SVG");
    assert(doc.shapes.size() == 1);
    assert(!doc.has_diagnostics());

    CapturePainter painter;
    svg::paint(painter, doc, 0.0f, 0.0f, 24.0f, 24.0f);
    assert(painter.strokes == 1);
    assert(std::abs(painter.first_move_x - 4.0f) < 0.001f);
    assert(std::abs(painter.first_move_y - 5.0f) < 0.001f);
    assert(std::abs(painter.first_line_x - 8.0f) < 0.001f);
    assert(std::abs(painter.first_line_y - 5.0f) < 0.001f);
    assert(std::abs(painter.last_thickness - 2.0f) < 0.001f);

    std::puts("PASS: SVG transform subset composes group and shape transforms");
}

void test_svg_unsupported_path_diagnostic() {
    auto doc = svg::parse(
        R"SVG(<svg viewBox="0 0 24 24"><path d="M4 12 A8 8 0 0 1 20 12"/></svg>)SVG");
    assert(doc.unsupported_count == 1);
    assert(doc.has_diagnostics());

    std::puts("PASS: SVG unsupported path commands are diagnostic");
}

void test_builtin_icons_parse() {
    assert(icons::style_name() == "macos_rounded_outline_svg");
    assert(icons::style_reference().find("Apple HIG") != std::string_view::npos);
    assert(icons::asset_policy().find("no Apple") != std::string_view::npos);
    assert(icons::all_symbol_count == 31);
    assert(icons::sidebar_symbol_count == 11);
    assert(icons::toolbar_symbol_count == 15);
    assert(icons::outline_symbol_count == 30);
    assert(icons::filled_symbol_count == 1);
    assert(icons::hierarchical_symbol_count == 20);

    unsigned int outline_count = 0;
    unsigned int filled_count = 0;
    unsigned int hierarchical_count = 0;
    for (unsigned int i = 0; i < icons::all_symbol_count; ++i) {
        auto symbol = icons::symbol_at(i);
        auto src = icons::source(symbol);
        auto doc = icons::document(symbol);
        auto descriptor = icons::descriptor(symbol);
        assert(!icons::name(symbol).empty());
        assert(!src.empty());
        assert(descriptor.name == icons::name(symbol));
        assert(descriptor.style == icons::style_name());
        assert(descriptor.grid_size == 24.0f);
        assert(descriptor.layer_count == doc.shapes.size());
        assert(descriptor.text_weight_aligned);
        assert(descriptor.default_scale == icons::SymbolScale::Medium);
        assert(icons::symbol_scale_name(descriptor.default_scale) == "medium");
        assert(descriptor.phenotype_owned);
        assert(!descriptor.uses_sf_symbols_asset);
        assert(descriptor.uses_current_color);
        if (symbol != icons::Symbol::More) {
            ++outline_count;
            assert(descriptor.variant == icons::SymbolVariant::Outline);
            assert(icons::symbol_variant_name(descriptor.variant) == "outline");
            assert(src.find(R"(stroke-linecap="round")") != std::string_view::npos);
            assert(src.find(R"(stroke-linejoin="round")") != std::string_view::npos);
            assert(descriptor.round_stroke);
            assert(!descriptor.filled);
        } else {
            ++filled_count;
            assert(descriptor.variant == icons::SymbolVariant::Fill);
            assert(icons::symbol_variant_name(descriptor.variant) == "fill");
            assert(descriptor.filled);
            assert(!descriptor.round_stroke);
        }
        if (descriptor.supports_hierarchical_opacity) {
            ++hierarchical_count;
            assert(descriptor.preferred_rendering
                   == icons::SymbolRenderingMode::Hierarchical);
            assert(icons::symbol_rendering_mode_name(descriptor.preferred_rendering)
                   == "hierarchical");
            assert(std::abs(descriptor.secondary_opacity - 0.66f) < 0.001f);
            assert(src.find("opacity") != std::string_view::npos);
        } else {
            assert(descriptor.preferred_rendering
                   == icons::SymbolRenderingMode::Monochrome);
            assert(icons::symbol_rendering_mode_name(descriptor.preferred_rendering)
                   == "monochrome");
            assert(descriptor.secondary_opacity == 1.0f);
        }
        assert(!doc.empty());
        assert(!doc.has_diagnostics());

        CapturePainter painter;
        icons::paint_symbol(painter, symbol, 0.0f, 0.0f, 24.0f,
                            Color{28, 28, 30, 255});
        assert(painter.fills + painter.strokes > 0);
    }
    assert(outline_count == icons::outline_symbol_count);
    assert(filled_count == icons::filled_symbol_count);
    assert(hierarchical_count == icons::hierarchical_symbol_count);

    auto shared = icons::descriptor(icons::Symbol::Shared);
    assert(shared.role == icons::SymbolRole::Sidebar);
    assert(icons::symbol_role_name(shared.role) == "sidebar");
    assert(shared.supports_hierarchical_opacity);
    assert(icons::sidebar_symbol_at(1) == icons::Symbol::Shared);
    assert(icons::toolbar_symbol_at(10) == icons::Symbol::Search);

    std::puts("PASS: built-in phenotype icon catalog is valid SVG");
}

} // namespace

int main() {
    test_svg_path_subset();
    test_svg_basic_shapes();
    test_svg_circle_uses_arc_path();
    test_svg_transform_subset();
    test_svg_unsupported_path_diagnostic();
    test_builtin_icons_parse();
    std::puts("\nAll SVG/icon tests passed.");
    return 0;
}
