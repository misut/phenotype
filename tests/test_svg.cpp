#include <cassert>
#include <cmath>
#include <cstdio>
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
        last_thickness = thickness;
        last_color = color;
    }
    void fill_path(PathBuilder const& path, Color color) override {
        ++fills;
        last_verb_count = path.verb_count;
        last_color = color;
    }
};

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

void test_svg_unsupported_path_diagnostic() {
    auto doc = svg::parse(
        R"SVG(<svg viewBox="0 0 24 24"><path d="M4 12 A8 8 0 0 1 20 12"/></svg>)SVG");
    assert(doc.unsupported_count == 1);
    assert(doc.has_diagnostics());

    std::puts("PASS: SVG unsupported path commands are diagnostic");
}

void test_builtin_icons_parse() {
    constexpr icons::Symbol symbols[] = {
        icons::Symbol::Back,
        icons::Symbol::Forward,
        icons::Symbol::Search,
        icons::Symbol::Share,
        icons::Symbol::Tag,
        icons::Symbol::More,
        icons::Symbol::Grid,
        icons::Symbol::List,
        icons::Symbol::Columns,
        icons::Symbol::Gallery,
        icons::Symbol::Folder,
        icons::Symbol::Trash,
        icons::Symbol::Document,
        icons::Symbol::Image,
        icons::Symbol::Movie,
        icons::Symbol::Plus,
        icons::Symbol::XMark,
        icons::Symbol::ChevronDown,
        icons::Symbol::Home,
        icons::Symbol::Cloud,
        icons::Symbol::AirDrop,
        icons::Symbol::Recents,
        icons::Symbol::Sidebar,
        icons::Symbol::NewFolder,
    };

    for (auto symbol : symbols) {
        auto src = icons::source(symbol);
        auto doc = icons::document(symbol);
        assert(!icons::name(symbol).empty());
        assert(!src.empty());
        if (symbol != icons::Symbol::More) {
            assert(src.find(R"(stroke-linecap="round")") != std::string_view::npos);
            assert(src.find(R"(stroke-linejoin="round")") != std::string_view::npos);
        }
        assert(!doc.empty());
        assert(!doc.has_diagnostics());

        CapturePainter painter;
        icons::paint_symbol(painter, symbol, 0.0f, 0.0f, 24.0f,
                            Color{28, 28, 30, 255});
        assert(painter.fills + painter.strokes > 0);
    }

    std::puts("PASS: built-in phenotype icons are valid SVG documents");
}

} // namespace

int main() {
    test_svg_path_subset();
    test_svg_basic_shapes();
    test_svg_unsupported_path_diagnostic();
    test_builtin_icons_parse();
    std::puts("\nAll SVG/icon tests passed.");
    return 0;
}
