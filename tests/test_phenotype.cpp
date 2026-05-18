#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <string_view>
#include <variant>
#include <vector>
import phenotype;
import phenotype.svg;

using namespace phenotype;

#if !defined(__wasi__) && !defined(__ANDROID__)
static null_host host;
#define RUN_APP(S, M, V, U)              run<S, M>(host, V, U)
#define LAYOUT_NODE(h, w)                detail::layout_node(host, h, w)
#define PAINT_NODE(h, ox, oy, sy, vh)    detail::paint_node(host, host, h, ox, oy, 0.0f, sy, 800.0f, vh)
#define CMD_BUF                          host.buf()
#define CMD_LEN                          host.buf_len()
#else
extern "C" {
    extern unsigned char phenotype_cmd_buf[];
    extern unsigned int phenotype_cmd_len;
    // Stubs for WASM host imports — wasmtime has no JS shim.
    void phenotype_flush() {}
    float phenotype_measure_text(float fs, unsigned int /*flags*/,
                                  char const* /*family*/, unsigned int /*family_len*/,
                                  char const* /*text*/, unsigned int len) {
        return static_cast<float>(len) * fs * 0.6f;
    }
    float phenotype_get_canvas_width() { return 800.0f; }
    float phenotype_get_canvas_height() { return 600.0f; }
    void phenotype_open_url(char const*, unsigned int) {}
}
#define RUN_APP(S, M, V, U)              run<S, M>(V, U)
#define LAYOUT_NODE(h, w)                detail::layout_node(h, w)
#define PAINT_NODE(h, ox, oy, sy, vh)    detail::wasi_paint_node(h, ox, oy, 0.0f, sy, 800.0f, vh)
#define CMD_BUF                          phenotype_cmd_buf
#define CMD_LEN                          phenotype_cmd_len
#endif

// ============================================================
// Layout tests
// ============================================================

void test_column_layout() {
    detail::g_app.arena.reset();
    auto root_h = detail::alloc_node();
    auto& root = detail::node_at(root_h);
    root.style.flex_direction = FlexDirection::Column;
    root.style.gap = 10;
    root.style.padding[0] = 5;
    root.style.padding[2] = 5;

    auto a_h = detail::alloc_node();
    auto& a = detail::node_at(a_h);
    a.text = "hello";
    a.font_size = 16.0f;
    detail::node_at(root_h).children.push_back(a_h);

    auto b_h = detail::alloc_node();
    auto& b = detail::node_at(b_h);
    b.text = "world";
    b.font_size = 16.0f;
    detail::node_at(root_h).children.push_back(b_h);

    LAYOUT_NODE(root_h, 400.0f);

    assert(root.width == 400.0f);
    assert(a.y == 5.0f);
    assert(b.y > a.y + a.height);
    assert(a.height > 0);
    assert(b.height > 0);
    std::puts("PASS: column layout");
}

void test_row_intrinsic_width() {
    detail::g_app.arena.reset();
    auto row_h = detail::alloc_node();
    auto& row = detail::node_at(row_h);
    row.style.flex_direction = FlexDirection::Row;
    row.style.gap = 8;

    auto bullet_h = detail::alloc_node();
    auto& bullet = detail::node_at(bullet_h);
    bullet.text = "*";
    bullet.font_size = 16.0f;
    detail::node_at(row_h).children.push_back(bullet_h);

    auto text_h = detail::alloc_node();
    auto& text = detail::node_at(text_h);
    text.text = "List item text here";
    text.font_size = 16.0f;
    detail::node_at(row_h).children.push_back(text_h);

    LAYOUT_NODE(row_h, 400.0f);

    assert(bullet.width < 50.0f);
    assert(text.width > bullet.width);
    assert(text.x > bullet.x + bullet.width);
    std::puts("PASS: row intrinsic width");
}

void test_row_wraps_last_text_leaf() {
    detail::g_app.arena.reset();
    auto row_h = detail::alloc_node();
    auto& row = detail::node_at(row_h);
    row.style.flex_direction = FlexDirection::Row;
    row.style.gap = 8;

    auto bullet_h = detail::alloc_node();
    auto& bullet = detail::node_at(bullet_h);
    bullet.text = "*";
    bullet.font_size = 16.0f;
    detail::node_at(row_h).children.push_back(bullet_h);

    auto text_h = detail::alloc_node();
    auto& text = detail::node_at(text_h);
    text.text = "List item text that should wrap inside a narrow card row";
    text.font_size = 16.0f;
    detail::node_at(row_h).children.push_back(text_h);

    LAYOUT_NODE(row_h, 120.0f);

    assert(text.text_lines.size() > 1);
    assert(text.x > bullet.x + bullet.width);
    assert(text.width <= 120.0f - bullet.width - row.style.gap + 0.01f);
    std::puts("PASS: row wraps last text leaf");
}

void test_containment_invariant() {
    detail::g_app.arena.reset();
    auto root_h = detail::alloc_node();
    auto& root = detail::node_at(root_h);
    root.style.flex_direction = FlexDirection::Column;
    root.style.padding[0] = 10;
    root.style.padding[1] = 10;
    root.style.padding[2] = 10;
    root.style.padding[3] = 10;

    for (int i = 0; i < 20; ++i) {
        auto child_h = detail::alloc_node();
        auto& child = detail::node_at(child_h);
        child.text = "test item " + std::to_string(i);
        child.font_size = 16.0f;
        detail::node_at(root_h).children.push_back(child_h);
    }

    LAYOUT_NODE(root_h, 300.0f);

    for (auto child_h : detail::node_at(root_h).children) {
        auto& child = detail::node_at(child_h);
        assert(child.x >= root.style.padding[3]);
        assert(child.x + child.width <= root.width - root.style.padding[1]);
        assert(child.height > 0);
    }
    std::puts("PASS: containment invariant");
}

void test_alignment_center() {
    detail::g_app.arena.reset();
    auto col_h = detail::alloc_node();
    auto& col = detail::node_at(col_h);
    col.style.flex_direction = FlexDirection::Column;
    col.style.cross_align = CrossAxisAlignment::Center;

    auto inner_h = detail::alloc_node();
    auto& inner = detail::node_at(inner_h);
    inner.style.max_width = 100;
    inner.style.fixed_height = 20;
    detail::node_at(col_h).children.push_back(inner_h);

    LAYOUT_NODE(col_h, 400.0f);

    assert(inner.width <= 100.0f);
    assert(inner.x > 0);
    std::puts("PASS: cross-axis center alignment");
}

void test_max_width_centering() {
    detail::g_app.arena.reset();
    auto root_h = detail::alloc_node();
    auto& root = detail::node_at(root_h);
    root.style.flex_direction = FlexDirection::Column;

    auto child_h = detail::alloc_node();
    auto& child = detail::node_at(child_h);
    child.style.max_width = 200;
    child.style.fixed_height = 30;
    detail::node_at(root_h).children.push_back(child_h);

    LAYOUT_NODE(root_h, 800.0f);

    assert(child.width <= 200.0f);
    assert(child.x >= 0);
    (void)root;
    std::puts("PASS: max-width centering");
}

// ============================================================
// Text layout tests
// ============================================================

void test_word_wrap() {
    detail::g_app.arena.reset();
    auto node_h = detail::alloc_node();
    auto& node = detail::node_at(node_h);
    node.text = "hello world this is a longer text that should wrap";
    node.font_size = 16.0f;

    LAYOUT_NODE(node_h, 100.0f);

    assert(!node.text_lines.empty());
    assert(node.text_lines.size() > 1);
    assert(node.height > 0);
    std::puts("PASS: word wrap");
}

void test_newline_handling() {
    detail::g_app.arena.reset();
    auto node_h = detail::alloc_node();
    auto& node = detail::node_at(node_h);
    node.text = "line1\nline2\nline3";
    node.font_size = 16.0f;

    LAYOUT_NODE(node_h, 800.0f);

    assert(node.text_lines.size() == 3);
    assert(node.text_lines[0] == "line1");
    assert(node.text_lines[1] == "line2");
    assert(node.text_lines[2] == "line3");
    std::puts("PASS: newline handling");
}

void test_measure_text_cache_dedup() {
    detail::g_app.arena.reset();
    metrics::reset_all();
    detail::clear_measure_cache();

    auto root_h = detail::alloc_node();
    auto& root = detail::node_at(root_h);
    root.style.flex_direction = FlexDirection::Column;

    for (int i = 0; i < 5; ++i) {
        auto h = detail::alloc_node();
        auto& n = detail::node_at(h);
        n.text = "hello world hello";
        n.font_size = 16.0f;
        detail::node_at(root_h).children.push_back(h);
    }

    LAYOUT_NODE(root_h, 800.0f);

    auto host_calls_first = metrics::inst::measure_text_calls.total();
    auto cache_hits_first = metrics::inst::measure_text_cache_hits.total();
    assert(host_calls_first > 0);
    assert(host_calls_first < 10);
    assert(cache_hits_first > host_calls_first);

    metrics::inst::measure_text_calls.reset();
    metrics::inst::measure_text_cache_hits.reset();
    LAYOUT_NODE(root_h, 800.0f);
    assert(metrics::inst::measure_text_calls.total() == 0);
    assert(metrics::inst::measure_text_cache_hits.total() > 0);

    std::puts("PASS: measure_text cache deduplicates across rebuilds");
}

void test_set_theme_updates_and_invalidates_cache() {
    detail::g_app.arena.reset();
    metrics::reset_all();
    detail::clear_measure_cache();

    auto root_h = detail::alloc_node();
    auto& root = detail::node_at(root_h);
    root.style.flex_direction = FlexDirection::Column;
    auto leaf_h = detail::alloc_node();
    auto& leaf = detail::node_at(leaf_h);
    leaf.text = "theme demo";
    leaf.font_size = current_theme().body_font_size;
    detail::node_at(root_h).children.push_back(leaf_h);
    LAYOUT_NODE(root_h, 400.0f);
    assert(metrics::inst::measure_text_cache_hits.total()
           + metrics::inst::measure_text_calls.total() > 0);

    Theme dark{};
    dark.background = {10,  10,  10, 255};
    dark.foreground = {240, 240, 240, 255};
    dark.body_font_size = 18.0f;
    set_theme(dark);

    assert(current_theme().body_font_size == 18.0f);
    assert(current_theme().background.r == 10);
    assert(current_theme().foreground.r == 240);

    metrics::inst::measure_text_calls.reset();
    metrics::inst::measure_text_cache_hits.reset();
    LAYOUT_NODE(root_h, 400.0f);
    assert(metrics::inst::measure_text_calls.total() > 0);

    set_theme(Theme{});

    std::puts("PASS: set_theme swaps theme and invalidates measure cache");
}

void test_default_theme_glass_contract() {
    Theme theme{};
    assert(theme_contract::theme_contract_version == 1);
    assert(theme_contract::glass_surface_roles().size() == 7);
    assert(default_theme_profile_name() == "apple-glass-light");
    assert(default_theme_reference().find("Apple HIG Materials")
           != std::string_view::npos);
    assert(default_theme_font_policy().find("Pretendard")
           != std::string_view::npos);
    assert(default_theme_material_policy().find("pure material planner")
           != std::string_view::npos);
    assert(default_theme_iconography_policy().find("macos_finder")
           != std::string_view::npos);
    assert(default_theme_iconography_policy().find("sf_symbols_aligned")
           != std::string_view::npos);
    assert(default_theme_icon_asset_policy().find("without_embedded_apple_artwork")
           != std::string_view::npos);
    assert(default_theme_usage_policy().find("not_content_fill")
           != std::string_view::npos);
    assert(default_theme_container_policy().find("explicit_container_spacing")
           != std::string_view::npos);
    assert(default_theme_performance_policy().find("bounded_glass_surfaces")
           != std::string_view::npos);
    assert(default_theme_accessibility_policy().find("reduced_transparency")
           != std::string_view::npos);
    assert(default_theme_fallback_policy().find("unsupported_backends")
           != std::string_view::npos);
    assert(theme_matches_default_glass_contract(theme));

    theme.default_font_family = "System";
    assert(!theme_matches_default_glass_contract(theme));

    std::puts("PASS: default theme exposes Apple glass contract metadata");
}

void test_sized_box_in_row() {
    detail::g_app.arena.reset();

    auto root_h = detail::alloc_node();
    auto& root = detail::node_at(root_h);
    root.style.flex_direction = FlexDirection::Column;

    Scope scope(root_h);
    Scope::set_current(&scope);
    layout::row([&] {
        layout::sized_box(200.0f, [&] {
            widget::text("left");
        });
        layout::sized_box(300.0f, [&] {
            widget::text("right");
        });
    });
    Scope::set_current(nullptr);

    LAYOUT_NODE(root_h, 800.0f);

    assert(root.children.size() == 1);
    auto& row = detail::node_at(root.children[0]);
    assert(row.children.size() == 2);

    auto& left = detail::node_at(row.children[0]);
    auto& right = detail::node_at(row.children[1]);

    assert(left.width == 200.0f);
    assert(right.width == 300.0f);
    assert(right.x >= left.x + left.width);

    std::puts("PASS: sized_box honors max_width inside row layout");
}

void test_image_widget_layout_and_emit() {
    detail::g_app.arena.reset();

    auto root_h = detail::alloc_node();
    auto& root = detail::node_at(root_h);
    root.style.flex_direction = FlexDirection::Row;

    Scope scope(root_h);
    Scope::set_current(&scope);
    widget::image({"logo.png", 8}, 64.0f, 48.0f);
    Scope::set_current(nullptr);

    LAYOUT_NODE(root_h, 200.0f);

    assert(root.children.size() == 1);
    auto& img = detail::node_at(root.children[0]);
    assert(img.image_url == std::string("logo.png"));
    assert(img.width == 64.0f);
    assert(img.height == 48.0f);

    CMD_LEN = 0;
    PAINT_NODE(root_h, 0, 0, 0, 600.0f);

    bool found_opcode = false;
    bool found_url = false;
    for (unsigned int i = 0; i + 4 <= CMD_LEN; i += 4) {
        unsigned int word;
        std::memcpy(&word, &CMD_BUF[i], 4);
        if (word == static_cast<unsigned int>(Cmd::DrawImage)) {
            found_opcode = true;
        }
    }
    char const* needle = "logo.png";
    for (unsigned int i = 0; i + 8 <= CMD_LEN; ++i) {
        if (std::memcmp(&CMD_BUF[i], needle, 8) == 0) {
            found_url = true;
            break;
        }
    }
    assert(found_opcode);
    assert(found_url);

    std::puts("PASS: widget::image lays out and emits DrawImage");
}

void test_svg_image_widget_uses_stable_paint_token() {
    detail::g_app.arena.reset();

    auto root_h = detail::alloc_node();
    auto& root = detail::node_at(root_h);
    root.style.flex_direction = FlexDirection::Row;

    constexpr std::string_view source =
        R"SVG(<svg viewBox="0 0 24 24" fill="none" stroke="currentColor"><path d="M4 12 L20 12"/></svg>)SVG";
    auto const color = Color{0, 122, 255, 255};
    auto expected = svg::paint_token(
        svg::parse(source),
        32.0f,
        32.0f,
        color);

    Scope scope(root_h);
    Scope::set_current(&scope);
    widget::svg_image(
        {source.data(), static_cast<unsigned int>(source.size())},
        32.0f,
        32.0f,
        color);
    Scope::set_current(nullptr);

    LAYOUT_NODE(root_h, 200.0f);

    assert(root.children.size() == 1);
    auto& svg_node = detail::node_at(root.children[0]);
    assert(svg_node.width == 32.0f);
    assert(svg_node.height == 32.0f);
    assert(static_cast<bool>(svg_node.paint_fn));
    assert(svg_node.debug_semantic_role == "image");
    assert(svg_node.paint_token == expected);
    assert(svg_node.paint_token != 0);

    std::puts("PASS: widget::svg_image uses a stable paint token");
}

void test_svg_image_widget_options_contract() {
    detail::g_app.arena.reset();

    auto root_h = detail::alloc_node();
    auto& root = detail::node_at(root_h);
    root.style.flex_direction = FlexDirection::Row;

    constexpr std::string_view source =
        R"SVG(<svg viewBox="0 0 10 20" fill="none" stroke="currentColor"><path d="M0 0 L10 20"/></svg>)SVG";
    auto const color = Color{255, 45, 85, 255};
    auto options = SvgImageOptions{
        .has_current_color = true,
        .current_color = color,
        .preserve_aspect_ratio = false,
        .semantic_label = "runtime-svg-asset",
    };
    auto expected = svg::paint_token(
        svg::parse(source),
        40.0f,
        20.0f,
        color,
        false);

    Scope scope(root_h);
    Scope::set_current(&scope);
    widget::svg_image(
        {source.data(), static_cast<unsigned int>(source.size())},
        40.0f,
        20.0f,
        options);
    Scope::set_current(nullptr);

    LAYOUT_NODE(root_h, 200.0f);

    assert(root.children.size() == 1);
    auto& svg_node = detail::node_at(root.children[0]);
    assert(svg_node.width == 40.0f);
    assert(svg_node.height == 20.0f);
    assert(static_cast<bool>(svg_node.paint_fn));
    assert(svg_node.debug_semantic_role == "image");
    assert(svg_node.debug_semantic_label == "runtime-svg-asset");
    assert(svg_node.paint_token == expected);
    assert(svg_node.paint_token != svg::paint_token(
        svg::parse(source),
        40.0f,
        20.0f,
        color,
        true));

    std::puts("PASS: widget::svg_image options are renderer-visible");
}

void test_grid_cell_text_is_vertically_centered() {
    detail::g_app.arena.reset();

    auto root_h = detail::alloc_node();
    detail::node_at(root_h).style.flex_direction = FlexDirection::Column;

    Scope scope(root_h);
    Scope::set_current(&scope);
    layout::grid({80.0f}, 40.0f, [] {
        widget::cell("A", true, 80.0f, 40.0f);
    }, 0.0f);
    Scope::set_current(nullptr);

    LAYOUT_NODE(root_h, 200.0f);
    auto const& grid = detail::node_at(detail::node_at(root_h).children[0]);
    auto const& cell = detail::node_at(grid.children[0]);
    assert(cell.height == 40.0f);

    CMD_LEN = 0;
    PAINT_NODE(root_h, 0, 0, 0, 600.0f);

    bool found_text = false;
    float text_y = 0.0f;
    for (unsigned int i = 0; i + 28 <= CMD_LEN;) {
        unsigned int op;
        std::memcpy(&op, &CMD_BUF[i], 4);
        if (op == static_cast<unsigned int>(Cmd::DrawText)) {
            std::memcpy(&text_y, &CMD_BUF[i + 8], 4);
            found_text = true;
            break;
        }
        i += 4;
    }
    assert(found_text);

    auto line_height = cell.font_size * detail::g_app.theme.line_height_ratio;
    auto expected_y = (cell.height - line_height) / 2.0f;
    assert(text_y > 0.0f);
    assert(std::fabs(text_y - expected_y) < 0.01f);

    std::puts("PASS: grid cell text is vertically centered");
}

void test_canvas_widget_invokes_painter() {
    detail::g_app.arena.reset();

    auto root_h = detail::alloc_node();
    auto& root = detail::node_at(root_h);
    root.style.flex_direction = FlexDirection::Column;

    Scope scope(root_h);
    Scope::set_current(&scope);
    int paint_calls = 0;
    char const* sample_text = "cad++";
    widget::canvas(200.0f, 100.0f, [&paint_calls, sample_text](Painter& p) {
        ++paint_calls;
        p.line(10.0f, 20.0f, 60.0f, 70.0f, 1.0f, Color{255, 0, 0, 255});
        p.text(80.0f, 30.0f, sample_text, 5u, 14.0f, Color{0, 0, 0, 255});
    });
    Scope::set_current(nullptr);

    LAYOUT_NODE(root_h, 400.0f);

    assert(root.children.size() == 1);
    auto& cv = detail::node_at(root.children[0]);
    assert(cv.width == 200.0f);
    assert(cv.height == 100.0f);
    assert(cv.paint_fn);

    CMD_LEN = 0;
    PAINT_NODE(root_h, 0, 0, 0, 600.0f);
    assert(paint_calls == 1);

    bool found_line = false;
    bool found_text = false;
    for (unsigned int i = 0; i + 4 <= CMD_LEN; i += 4) {
        unsigned int word;
        std::memcpy(&word, &CMD_BUF[i], 4);
        if (word == static_cast<unsigned int>(Cmd::DrawLine)) found_line = true;
        if (word == static_cast<unsigned int>(Cmd::DrawText)) found_text = true;
    }
    assert(found_line);
    assert(found_text);

    bool found_text_payload = false;
    for (unsigned int i = 0; i + 5 <= CMD_LEN; ++i) {
        if (std::memcmp(&CMD_BUF[i], "cad++", 5) == 0) {
            found_text_payload = true;
            break;
        }
    }
    assert(found_text_payload);

    std::puts("PASS: widget::canvas invokes painter and emits DrawLine + DrawText");
}

void test_canvas_linear_gradient_rect_emits_command() {
    detail::g_app.arena.reset();

    auto root_h = detail::alloc_node();
    auto& root = detail::node_at(root_h);
    root.style.flex_direction = FlexDirection::Column;

    Scope scope(root_h);
    Scope::set_current(&scope);
    widget::canvas(120.0f, 80.0f, [](Painter& p) {
        p.linear_gradient_rect(
            8.0f,
            10.0f,
            80.0f,
            24.0f,
            Color{10, 20, 30, 255},
            Color{110, 120, 130, 128},
            GradientAxis::Horizontal,
            4);
    });
    Scope::set_current(nullptr);

    LAYOUT_NODE(root_h, 240.0f);

    CMD_LEN = 0;
    PAINT_NODE(root_h, 0, 0, 0, 240.0f);

    auto cmds = parse_commands(CMD_BUF, CMD_LEN);
    LinearGradientRectCmd const* gradient = nullptr;
    for (auto const& cmd : cmds) {
        if (auto const* parsed = std::get_if<LinearGradientRectCmd>(&cmd)) {
            gradient = parsed;
            break;
        }
    }

    assert(gradient != nullptr);
    auto const start_color = Color{10, 20, 30, 255};
    auto const end_color = Color{110, 120, 130, 128};
    assert(gradient->x == 8.0f);
    assert(gradient->y == 10.0f);
    assert(gradient->w == 80.0f);
    assert(gradient->h == 24.0f);
    assert(gradient->from == start_color);
    assert(gradient->to == end_color);
    assert(gradient->axis == GradientAxis::Horizontal);
    assert(gradient->steps == 4);

    std::puts("PASS: Painter::linear_gradient_rect emits LinearGradientRect");
}

void test_canvas_bypasses_paint_cache_after_diff() {
    auto make_canvas_tree = [](int& paint_calls, Color color, bool paint) {
        auto root_h = detail::alloc_node();
        detail::node_at(root_h).style.flex_direction = FlexDirection::Column;

        Scope scope(root_h);
        Scope::set_current(&scope);
        widget::canvas(200.0f, 100.0f, [&paint_calls, color](Painter& p) {
            ++paint_calls;
            p.line(10.0f, 20.0f, 60.0f, 70.0f, 1.0f, color);
        });
        Scope::set_current(nullptr);

        if (paint) {
            LAYOUT_NODE(root_h, 400.0f);
            CMD_LEN = 0;
            PAINT_NODE(root_h, 0, 0, 0, 600.0f);
            std::memcpy(detail::g_app.prev_cmd_buf, CMD_BUF, CMD_LEN);
            detail::g_app.prev_cmd_len = CMD_LEN;
        }
        return root_h;
    };

    detail::g_app.arena.reset();
    detail::g_app.prev_arena.reset();
    detail::g_app.callbacks.clear();
    detail::g_app.prev_cmd_len = 0;

    int old_calls = 0;
    auto old_root = make_canvas_tree(old_calls, Color{255, 0, 0, 255}, true);
    assert(old_calls == 1);
    assert(detail::node_at(old_root).paint_dynamic);
    assert(!detail::node_at(old_root).paint_valid);

    detail::g_app.prev_root = old_root;
    std::swap(detail::g_app.arena, detail::g_app.prev_arena);
    detail::g_app.arena.reset();
    detail::g_app.callbacks.clear();

    int new_calls = 0;
    auto new_root = make_canvas_tree(new_calls, Color{0, 0, 255, 255}, false);
    auto matched = detail::diff_and_copy_layout(
        detail::g_app.prev_root,
        new_root,
        detail::g_app.prev_arena,
        detail::g_app.arena);
    assert(matched);

    LAYOUT_NODE(new_root, 400.0f);
    CMD_LEN = 0;
    PAINT_NODE(new_root, 0, 0, 0, 600.0f);
    assert(new_calls == 1);
    assert(detail::node_at(new_root).paint_dynamic);
    assert(!detail::node_at(new_root).paint_valid);

    std::puts("PASS: widget::canvas bypasses paint cache after diff");
}

// Opt-in dirty-token contract: when the caller passes a non-zero
// paint_token to widget::canvas and the same token is observed across
// two consecutive frames, paint_node blits the prev_cmd_buf byte
// range and skips paint_fn entirely.
void test_canvas_paint_token_hit_skips_paint_fn() {
    auto make_canvas_tree = [](int& paint_calls, Color color,
                               std::uint64_t token, bool paint) {
        auto root_h = detail::alloc_node();
        detail::node_at(root_h).style.flex_direction = FlexDirection::Column;

        Scope scope(root_h);
        Scope::set_current(&scope);
        widget::canvas(200.0f, 100.0f,
            [&paint_calls, color](Painter& p) {
                ++paint_calls;
                p.line(10.0f, 20.0f, 60.0f, 70.0f, 1.0f, color);
            },
            {},
            token);
        Scope::set_current(nullptr);

        if (paint) {
            LAYOUT_NODE(root_h, 400.0f);
            CMD_LEN = 0;
            PAINT_NODE(root_h, 0, 0, 0, 600.0f);
            std::memcpy(detail::g_app.prev_cmd_buf, CMD_BUF, CMD_LEN);
            detail::g_app.prev_cmd_len = CMD_LEN;
        }
        return root_h;
    };

    detail::g_app.arena.reset();
    detail::g_app.prev_arena.reset();
    detail::g_app.callbacks.clear();
    detail::g_app.prev_cmd_len = 0;
    detail::g_app.paint_invalidation_mask = 0;
    detail::g_app.prev_scroll_x = 0;
    detail::g_app.prev_scroll_y = 0;
    metrics::reset_all();

    constexpr std::uint64_t kToken = 0xABCD'1234'ABCD'1234ULL;

    int old_calls = 0;
    auto old_root = make_canvas_tree(old_calls,
        Color{255, 0, 0, 255}, kToken, true);
    assert(old_calls == 1);
    auto& old_canvas = detail::node_at(detail::node_at(old_root).children[0]);
    // Token opt-in flips the canvas to non-dynamic at recording so
    // next frame's blit guard is allowed to fire.
    assert(!old_canvas.paint_dynamic);
    assert(old_canvas.paint_valid);
    assert(old_canvas.paint_token == kToken);

    detail::g_app.prev_root = old_root;
    std::swap(detail::g_app.arena, detail::g_app.prev_arena);
    detail::g_app.arena.reset();
    detail::g_app.callbacks.clear();

    int new_calls = 0;
    // Same token, but a different colour the painter would emit if
    // re-invoked. The blit must reuse prev_cmd_buf bytes (the red
    // line), so new_calls must stay at 0 and the bytes must match.
    auto new_root = make_canvas_tree(new_calls,
        Color{0, 0, 255, 255}, kToken, false);
    auto matched = detail::diff_and_copy_layout(
        detail::g_app.prev_root, new_root,
        detail::g_app.prev_arena, detail::g_app.arena);
    assert(matched);

    auto& new_canvas = detail::node_at(detail::node_at(new_root).children[0]);
    assert(new_canvas.paint_token == kToken);
    assert(new_canvas.paint_token_prev == kToken);
    assert(!new_canvas.paint_dynamic);
    assert(new_canvas.paint_valid);

    auto blits_before = metrics::inst::paint_subtrees_blitted.total();
    LAYOUT_NODE(new_root, 400.0f);
    CMD_LEN = 0;
    PAINT_NODE(new_root, 0, 0, 0, 600.0f);
    auto blits_during = metrics::inst::paint_subtrees_blitted.total() - blits_before;

    // paint_fn never fires for the cached canvas this frame.
    assert(new_calls == 0);
    // At least one blit (could be 1 = parent column blits whole
    // subtree, or 2 = parent walks + canvas blits separately, both
    // are correct for this assertion's purpose).
    assert(blits_during >= 1);

    std::puts("PASS: widget::canvas with stable paint_token blits and skips paint_fn");
}

// Token mismatch: a non-zero token that differs from prev frame's
// recorded value falls through to the miss path and paint_fn fires.
void test_canvas_paint_token_miss_invokes_paint_fn() {
    auto make_canvas_tree = [](int& paint_calls, Color color,
                               std::uint64_t token, bool paint) {
        auto root_h = detail::alloc_node();
        detail::node_at(root_h).style.flex_direction = FlexDirection::Column;

        Scope scope(root_h);
        Scope::set_current(&scope);
        widget::canvas(200.0f, 100.0f,
            [&paint_calls, color](Painter& p) {
                ++paint_calls;
                p.line(10.0f, 20.0f, 60.0f, 70.0f, 1.0f, color);
            },
            {},
            token);
        Scope::set_current(nullptr);

        if (paint) {
            LAYOUT_NODE(root_h, 400.0f);
            CMD_LEN = 0;
            PAINT_NODE(root_h, 0, 0, 0, 600.0f);
            std::memcpy(detail::g_app.prev_cmd_buf, CMD_BUF, CMD_LEN);
            detail::g_app.prev_cmd_len = CMD_LEN;
        }
        return root_h;
    };

    detail::g_app.arena.reset();
    detail::g_app.prev_arena.reset();
    detail::g_app.callbacks.clear();
    detail::g_app.prev_cmd_len = 0;
    detail::g_app.paint_invalidation_mask = 0;
    detail::g_app.prev_scroll_x = 0;
    detail::g_app.prev_scroll_y = 0;

    int old_calls = 0;
    auto old_root = make_canvas_tree(old_calls,
        Color{255, 0, 0, 255}, 0xAAAA'AAAA'AAAA'AAAAULL, true);
    assert(old_calls == 1);

    detail::g_app.prev_root = old_root;
    std::swap(detail::g_app.arena, detail::g_app.prev_arena);
    detail::g_app.arena.reset();
    detail::g_app.callbacks.clear();

    int new_calls = 0;
    // Different token — represents an upstream input change.
    auto new_root = make_canvas_tree(new_calls,
        Color{0, 0, 255, 255}, 0xBBBB'BBBB'BBBB'BBBBULL, false);
    auto matched = detail::diff_and_copy_layout(
        detail::g_app.prev_root, new_root,
        detail::g_app.prev_arena, detail::g_app.arena);
    assert(matched);

    LAYOUT_NODE(new_root, 400.0f);
    CMD_LEN = 0;
    PAINT_NODE(new_root, 0, 0, 0, 600.0f);

    // Token differs → blit guard fails → paint_fn re-invoked.
    assert(new_calls == 1);

    std::puts("PASS: widget::canvas with mismatched paint_token re-invokes paint_fn");
}

// Ancestor of a token-stable canvas regains cache eligibility:
// without the opt-in, a canvas poisons every ancestor with
// paint_dynamic=true. With opt-in, the canvas's parent column blits
// the entire subtree as one byte range and the canvas's paint_fn
// never runs.
void test_canvas_paint_token_lets_ancestor_blit() {
    auto make_tree = [](int& paint_calls, Color color,
                        std::uint64_t token, bool paint) {
        auto root_h = detail::alloc_node();
        detail::node_at(root_h).style.flex_direction = FlexDirection::Column;

        Scope scope(root_h);
        Scope::set_current(&scope);
        layout::column([&] {
            widget::canvas(200.0f, 100.0f,
                [&paint_calls, color](Painter& p) {
                    ++paint_calls;
                    p.line(0.0f, 0.0f, 100.0f, 100.0f, 1.0f, color);
                },
                {},
                token);
        });
        Scope::set_current(nullptr);

        if (paint) {
            LAYOUT_NODE(root_h, 400.0f);
            CMD_LEN = 0;
            PAINT_NODE(root_h, 0, 0, 0, 600.0f);
            std::memcpy(detail::g_app.prev_cmd_buf, CMD_BUF, CMD_LEN);
            detail::g_app.prev_cmd_len = CMD_LEN;
        }
        return root_h;
    };

    detail::g_app.arena.reset();
    detail::g_app.prev_arena.reset();
    detail::g_app.callbacks.clear();
    detail::g_app.prev_cmd_len = 0;
    detail::g_app.paint_invalidation_mask = 0;
    detail::g_app.prev_scroll_x = 0;
    detail::g_app.prev_scroll_y = 0;
    metrics::reset_all();

    constexpr std::uint64_t kToken = 0x1234'5678'9ABC'DEF0ULL;

    int old_calls = 0;
    auto old_root = make_tree(old_calls,
        Color{255, 0, 0, 255}, kToken, true);
    assert(old_calls == 1);
    // The intermediate column wrapper around the canvas should also
    // be non-dynamic now because the only dynamic descendant has
    // opted in.
    auto& old_root_node = detail::node_at(old_root);
    assert(old_root_node.children.size() == 1);
    auto& old_inner_col = detail::node_at(old_root_node.children[0]);
    assert(!old_inner_col.paint_dynamic);
    assert(old_inner_col.paint_valid);

    detail::g_app.prev_root = old_root;
    std::swap(detail::g_app.arena, detail::g_app.prev_arena);
    detail::g_app.arena.reset();
    detail::g_app.callbacks.clear();

    int new_calls = 0;
    auto new_root = make_tree(new_calls,
        Color{0, 0, 255, 255}, kToken, false);
    auto matched = detail::diff_and_copy_layout(
        detail::g_app.prev_root, new_root,
        detail::g_app.prev_arena, detail::g_app.arena);
    assert(matched);

    auto blits_before = metrics::inst::paint_subtrees_blitted.total();
    LAYOUT_NODE(new_root, 400.0f);
    CMD_LEN = 0;
    PAINT_NODE(new_root, 0, 0, 0, 600.0f);
    auto blits_during = metrics::inst::paint_subtrees_blitted.total() - blits_before;

    // paint_fn never runs (would have run for the blue line otherwise).
    assert(new_calls == 0);
    // Exactly one blit: the outermost cache-eligible subtree blits
    // its whole byte range. Without the dynamic-poison fix, this
    // would be 0 because every ancestor would carry paint_dynamic=true.
    assert(blits_during >= 1);

    std::puts("PASS: token-stable canvas lets ancestors take the blit path");
}

// Regression: after diff_and_copy_layout marks a subtree layout_valid,
// re-running layout with a different available_width must NOT short-
// circuit on the cached width. Otherwise window resize leaves the tree
// at the previous-frame width and content collapses to one corner.
void test_layout_relayout_when_available_width_changes() {
    auto build_tree = [](float canvas_w) {
        detail::g_app.arena.reset();
        auto root_h = detail::alloc_node();
        auto& root = detail::node_at(root_h);
        root.style.flex_direction = FlexDirection::Column;

        auto child_h = detail::alloc_node();
        auto& child = detail::node_at(child_h);
        child.style.flex_direction = FlexDirection::Column;
        child.style.cross_align = CrossAxisAlignment::Center;
        child.text = "";
        root.children.push_back(child_h);

        auto leaf_h = detail::alloc_node();
        auto& leaf = detail::node_at(leaf_h);
        leaf.text = "hi";
        leaf.font_size = 16.0f;
        leaf.style.text_align = TextAlign::Center;
        child.children.push_back(leaf_h);

        LAYOUT_NODE(root_h, canvas_w);
        return root_h;
    };

    detail::g_app.arena.reset();
    detail::g_app.prev_arena.reset();
    detail::g_app.callbacks.clear();

    // Frame 1 at narrow width.
    auto old_root = build_tree(400.0f);
    assert(detail::node_at(old_root).width == 400.0f);

    // Hand the tree off as the previous frame.
    detail::g_app.prev_root = old_root;
    std::swap(detail::g_app.arena, detail::g_app.prev_arena);
    detail::g_app.arena.reset();
    detail::g_app.callbacks.clear();

    // Frame 2 at wider width — diff/copy will mark layout_valid.
    auto new_root = build_tree(400.0f);  // builds a structurally-identical tree
    auto matched = detail::diff_and_copy_layout(
        detail::g_app.prev_root, new_root,
        detail::g_app.prev_arena, detail::g_app.arena);
    assert(matched);
    assert(detail::node_at(new_root).layout_valid);

    // Re-layout at a wider canvas — this is what the runner does on
    // window resize. Without the fix, layout_node sees layout_valid==
    // true and returns immediately with width=400.
    LAYOUT_NODE(new_root, 1500.0f);
    assert(detail::node_at(new_root).width == 1500.0f);

    auto const& root = detail::node_at(new_root);
    assert(root.children.size() == 1);
    auto const& middle = detail::node_at(root.children[0]);
    assert(middle.width == 1500.0f);
    assert(middle.children.size() == 1);
    auto const& leaf = detail::node_at(middle.children[0]);
    assert(leaf.width == 1500.0f);

    std::puts("PASS: relayout invalidates cached width on viewport resize");
}

void test_checkbox_and_radio_widgets() {
    struct ToggleA {};
    struct PickB { int idx; };
    using Msg = std::variant<ToggleA, PickB>;

    detail::g_app.arena.reset();
    detail::g_app.callbacks.clear();
    detail::msg_queue().clear();

    auto root_h = detail::alloc_node();
    detail::node_at(root_h).style.flex_direction = FlexDirection::Column;

    Scope scope(root_h);
    Scope::set_current(&scope);
    widget::checkbox<Msg>("Subscribe",  false, ToggleA{});
    widget::checkbox<Msg>("Subscribe!", true,  ToggleA{});
    widget::radio<Msg>   ("Option A",   false, PickB{0});
    widget::radio<Msg>   ("Option B",   true,  PickB{1});
    Scope::set_current(nullptr);

    LAYOUT_NODE(root_h, 400.0f);

    auto& root = detail::node_at(root_h);
    assert(root.children.size() == 4);

    auto check_widget = [](NodeHandle row_h, bool active, float radius,
                           Decoration expected_decoration,
                           InteractionRole expected_role) {
        auto& row = detail::node_at(row_h);
        assert(row.style.flex_direction == FlexDirection::Row);
        assert(row.callback_id != 0xFFFFFFFF);
        assert(row.cursor_type == 1);
        assert(row.focusable == true);
        assert(row.interaction_role == expected_role);
        assert(row.children.size() == 2);

        auto& box = detail::node_at(row.children[0]);
        assert(box.style.max_width == 16.0f);
        assert(box.style.fixed_height == 16.0f);
        assert(box.border_radius == radius);
        assert(box.callback_id == 0xFFFFFFFF);
        assert(box.cursor_type == 0);
        assert(box.interaction_role == InteractionRole::None);
        if (active) {
            assert(box.background.r == detail::g_app.theme.accent.r);
            assert(box.background.g == detail::g_app.theme.accent.g);
            assert(box.background.b == detail::g_app.theme.accent.b);
            assert(box.background.a == 255);
            assert(box.decoration == expected_decoration);
        } else {
            assert(box.background.r == 255);
            assert(box.background.g == 255);
            assert(box.background.b == 255);
            assert(box.border_color.r == detail::g_app.theme.border.r);
            assert(box.decoration == Decoration::None);
        }

        auto& lbl = detail::node_at(row.children[1]);
        assert(!lbl.text.empty());
        assert(lbl.callback_id == 0xFFFFFFFF);
        assert(lbl.cursor_type == 0);
        assert(lbl.focusable == false);
    };

    check_widget(root.children[0], false, detail::g_app.theme.radius_xs,
                 Decoration::Check, InteractionRole::Checkbox);
    check_widget(root.children[1], true,  detail::g_app.theme.radius_xs,
                 Decoration::Check, InteractionRole::Checkbox);
    check_widget(root.children[2], false, detail::g_app.theme.radius_lg,
                 Decoration::Dot,   InteractionRole::Radio);
    check_widget(root.children[3], true,  detail::g_app.theme.radius_lg,
                 Decoration::Dot,   InteractionRole::Radio);

    auto cb_id_a = detail::node_at(root.children[0]).callback_id;
    detail::g_app.callbacks[cb_id_a]();
    auto msgs = detail::drain<Msg>();
    assert(msgs.size() == 1);
    assert(std::holds_alternative<ToggleA>(msgs[0]));

    auto cb_id_b = detail::node_at(root.children[3]).callback_id;
    detail::g_app.callbacks[cb_id_b]();
    auto msgs2 = detail::drain<Msg>();
    assert(msgs2.size() == 1);
    assert(std::holds_alternative<PickB>(msgs2[0]));
    assert(std::get<PickB>(msgs2[0]).idx == 1);

    CMD_LEN = 0;
    detail::g_app.focusable_ids.clear();
    detail::collect_focusable_ids(root_h);
    PAINT_NODE(root_h, 0, 0, 0, 600.0f);
    int hit_regions = 0;
    for (unsigned int i = 0; i + 4 <= CMD_LEN; i += 4) {
        unsigned int word;
        std::memcpy(&word, &CMD_BUF[i], 4);
        if (word == static_cast<unsigned int>(Cmd::HitRegion)) ++hit_regions;
    }
    assert(hit_regions == 4);
    assert(detail::g_app.focusable_ids.size() == 4);

    // theme.toggle_box_size drives the box visual size. Override to a
    // touch-friendly 44 dp and confirm the laid-out box picks it up.
    float saved_size = detail::g_app.theme.toggle_box_size;
    detail::g_app.theme.toggle_box_size = 44.0f;
    detail::g_app.arena.reset();
    detail::g_app.callbacks.clear();
    detail::msg_queue().clear();
    auto root2_h = detail::alloc_node();
    detail::node_at(root2_h).style.flex_direction = FlexDirection::Column;
    {
        Scope scope2(root2_h);
        Scope::set_current(&scope2);
        widget::checkbox<Msg>("Touch", false, ToggleA{});
        Scope::set_current(nullptr);
    }
    LAYOUT_NODE(root2_h, 400.0f);
    auto& touch_row = detail::node_at(detail::node_at(root2_h).children[0]);
    auto& touch_box = detail::node_at(touch_row.children[0]);
    assert(touch_box.style.max_width == 44.0f);
    assert(touch_box.style.fixed_height == 44.0f);
    detail::g_app.theme.toggle_box_size = saved_size;

    std::puts("PASS: checkbox + radio widgets");
}

void test_frame_skip_on_identical_cmd_buffer() {
    detail::g_app.arena.reset();
    detail::g_app.prev_arena.reset();
    metrics::reset_all();
    detail::clear_measure_cache();
    detail::g_app.last_paint_hash = 0;
    CMD_LEN = 0;

    RUN_APP(int, int,
        [](int const&) { widget::text("hello frame skip"); },
        [](int& s, int m) { s = m; });

    auto initial_flushes = metrics::inst::flush_calls.total();
    auto initial_skips   = metrics::inst::frames_skipped.total();
    assert(initial_flushes >= 1);
    assert(initial_skips == 0);

    detail::trigger_rebuild();
    assert(metrics::inst::flush_calls.total() == initial_flushes);
    assert(metrics::inst::frames_skipped.total() == initial_skips + 1);

    detail::trigger_rebuild();
    assert(metrics::inst::flush_calls.total() == initial_flushes);
    assert(metrics::inst::frames_skipped.total() == initial_skips + 2);

    std::puts("PASS: frame skip when cmd buffer is identical");
}

void test_paint_only_props_invalidate_diff_cache() {
    auto make_radio_tree = [](bool selected, bool paint) {
        auto root_h = detail::alloc_node();
        detail::node_at(root_h).style.flex_direction = FlexDirection::Column;
        Scope scope(root_h);
        Scope::set_current(&scope);
        widget::radio<int>("Base", selected, 1);
        Scope::set_current(nullptr);
        if (paint) {
            LAYOUT_NODE(root_h, 400.0f);
            PAINT_NODE(root_h, 0, 0, 0, 600.0f);
        }
        return root_h;
    };

    detail::g_app.arena.reset();
    detail::g_app.prev_arena.reset();
    detail::g_app.callbacks.clear();
    CMD_LEN = 0;
    auto old_root = make_radio_tree(true, true);

    detail::g_app.prev_root = old_root;
    std::swap(detail::g_app.arena, detail::g_app.prev_arena);
    detail::g_app.arena.reset();
    detail::g_app.callbacks.clear();
    CMD_LEN = 0;
    auto new_root = make_radio_tree(false, false);

    auto matched = detail::diff_and_copy_layout(
        detail::g_app.prev_root,
        new_root,
        detail::g_app.prev_arena,
        detail::g_app.arena);

    assert(!matched);
    auto const& radio_row = detail::node_at(detail::node_at(new_root).children[0]);
    auto const& radio_box = detail::node_at(radio_row.children[0]);
    assert(radio_box.decoration == Decoration::None);
    assert(!radio_box.layout_valid);
    assert(!radio_box.paint_valid);

    std::puts("PASS: paint-only prop changes invalidate diff/paint cache");
}

void test_material_props_invalidate_diff_cache() {
    auto make_tree = [](float blur_radius) {
        auto root_h = detail::alloc_node();
        auto material_h = detail::alloc_node();
        auto& material = detail::node_at(material_h);
        material.material = layout::material_style(MaterialKind::Regular);
        material.material.blur_radius = blur_radius;
        material.background = Color{255, 255, 255, 128};
        material.border_color = Color{229, 231, 235, 190};
        material.border_width = 1.0f;
        material.border_radius = 4.0f;
        material.style.fixed_height = 40.0f;
        detail::node_at(root_h).children.push_back(material_h);
        return root_h;
    };

    detail::g_app.arena.reset();
    detail::g_app.prev_arena.reset();
    auto old_root = make_tree(16.0f);
    detail::g_app.prev_root = old_root;
    std::swap(detail::g_app.arena, detail::g_app.prev_arena);
    detail::g_app.arena.reset();

    auto new_root = make_tree(30.0f);
    auto matched = detail::diff_and_copy_layout(
        detail::g_app.prev_root,
        new_root,
        detail::g_app.prev_arena,
        detail::g_app.arena);

    assert(!matched);
    auto const& material = detail::node_at(detail::node_at(new_root).children[0]);
    assert(!material.layout_valid);
    assert(!material.paint_valid);

    std::puts("PASS: material prop changes invalidate diff/paint cache");
}

void test_material_planner_backdrop_and_fallback_paths() {
    Theme theme{};
    assert(material_resolve_sample_taps(0) == 0);
    assert(material_resolve_sample_taps(4) == 1);
    assert(material_resolve_sample_taps(7) == 5);
    assert(material_resolve_sample_taps(10) == 9);
    assert(material_resolve_sample_taps(24) == 13);
    assert(material_resolve_sample_taps(25) == 25);

    auto default_quality = default_material_quality_policy();
    assert(default_quality.allow_backdrop_sampling);
    assert(default_quality.allow_noise);
    assert(default_quality.allow_shadow);
    assert(default_quality.max_blur_radius == 36.0f);
    assert(default_quality.max_sample_taps == 25);
    assert(default_quality.max_backdrop_pixels == 4'000'000);

    MaterialQualityPolicy raw_quality{};
    raw_quality.max_blur_radius = -4.0f;
    raw_quality.max_sample_taps = 99;
    raw_quality.max_backdrop_pixels = -8;
    auto sanitized_quality = sanitize_material_quality_policy(raw_quality);
    assert(sanitized_quality.max_blur_radius == 0.0f);
    assert(sanitized_quality.max_sample_taps == 25);
    assert(sanitized_quality.max_backdrop_pixels == 0);

    auto style = material_style_for_kind(MaterialKind::Regular, theme);
    MaterialRequest request{
        style,
        MaterialGeometry{12.0f, 20.0f, 240.0f, 96.0f, 10.0f},
    };

    MaterialEnvironment fallback_env{};
    fallback_env.capabilities.material_surfaces = true;
    fallback_env.render_target.width = 520;
    fallback_env.render_target.height = 760;
    fallback_env.render_target.scale = 2.0f;
    auto fallback_plan = plan_material_surface(request, fallback_env);
    assert(fallback_plan.contract_version == material_plan_contract_version);
    assert(fallback_plan.kind == MaterialKind::Regular);
    assert(fallback_plan.role == MaterialSurfaceRole::Surface);
    assert(fallback_plan.theme.default_glass_tokens);
    assert(std::string(fallback_plan.theme.profile_name) == "apple-glass-light");
    assert(std::string(fallback_plan.theme.source) == "material-style");
    assert(fallback_plan.theme.foreground_matches_theme);
    assert(fallback_plan.theme.accent_matches_theme);
    assert(fallback_plan.theme.tint_matches_surface);
    assert(fallback_plan.theme.border_matches_theme);
    assert(fallback_plan.command_descriptor.role == MaterialSurfaceRole::Surface);
    assert(fallback_plan.container.mode == MaterialContainerMode::Isolated);
    assert(std::string(fallback_plan.container.mode_name) == "isolated");
    assert(!fallback_plan.container.participates);
    assert(fallback_plan.shape.valid);
    assert(fallback_plan.shape.rounded);
    assert(!fallback_plan.shape.radius_clamped);
    assert(fallback_plan.shape.surface_area == 240.0f * 96.0f);
    assert(fallback_plan.shape.min_extent == 96.0f);
    assert(fallback_plan.shape.max_extent == 240.0f);
    assert(fallback_plan.shape.radius_limit == 48.0f);
    assert(fallback_plan.shape.effective_radius == 10.0f);
    assert(std::fabs(fallback_plan.shape.normalized_radius
                     - (10.0f / 48.0f)) < 0.0001f);
    assert(fallback_plan.fallback());
    assert(!fallback_plan.backdrop_sampling);
    assert(fallback_plan.render_target.width == 520);
    assert(fallback_plan.render_target.height == 760);
    assert(fallback_plan.render_target.pixel_count == 395'200);
    assert(fallback_plan.render_target.ready);
    assert(fallback_plan.render_target.within_backdrop_budget);
    assert(std::string(fallback_plan.render_target.pixel_format)
           == "unknown");
    assert(fallback_plan.decision_trace.has_material);
    assert(fallback_plan.decision_trace.role_allows_liquid_glass);
    assert(!fallback_plan.decision_trace.content_layer_standard_material);
    assert(fallback_plan.decision_trace.liquid_glass_backdrop_candidate);
    assert(fallback_plan.decision_trace.target_ready);
    assert(!fallback_plan.decision_trace.backend_supports_backdrop);
    assert(!fallback_plan.decision_trace.can_sample_backdrop);
    assert(!fallback_plan.decision_trace.next_frame_capture_required);
    assert(std::string(fallback_plan.decision_trace.first_blocker)
           == "unsupported-backend");
    assert(fallback_plan.fallback_path == MaterialFallbackPath::UnsupportedBackend);
    assert(fallback_plan.blur_radius == 0.0f);
    assert(std::string(fallback_plan.primary_pass.name)
           == "translucent-rounded-rect");
    assert(fallback_plan.primary_pass.active);
    assert(!fallback_plan.primary_pass.requires_backdrop);
    assert(fallback_plan.sample_taps == 0);
    assert(fallback_plan.primary_pass.sample_taps == 0);
    assert(std::string(fallback_plan.primary_pass.executor)
           == "fallback-fill");
    assert(fallback_plan.primary_pass.max_texture_copy_pixels == 0);
    assert(fallback_plan.resource_budget.max_sample_taps == 25);
    assert(fallback_plan.resource_budget.max_sampling_kernel_radius == 0);
    assert(fallback_plan.resource_budget.deterministic_fallback);
    assert(!fallback_plan.backdrop_access.required);
    assert(!fallback_plan.backdrop_access.shared_frame_capture);
    assert(!fallback_plan.backdrop_access.next_frame_capture_required);
    assert(!fallback_plan.backdrop_access.excludes_foreground_text);
    assert(std::string(fallback_plan.backdrop_access.capture_scope) == "none");
    assert(std::string(fallback_plan.backdrop_access.capture_reason)
           == "not-required");
    assert(std::string(fallback_plan.sampling_kernel.name) == "none");
    assert(fallback_plan.sampling_kernel.radius == 0);
    assert(fallback_plan.sampling_kernel.sample_taps == 0);
    assert(fallback_plan.sampling_kernel.blur_step_scale == 0.0f);
    assert(std::string(fallback_plan.sampling_kernel.weight_profile)
           == "none");
    assert(!fallback_plan.sampling_kernel.requires_backdrop);
    assert(fallback_plan.sampling_kernel.bounded);
    assert(std::string(fallback_plan.luminance_curve.name)
           == "fallback-flat");
    assert(std::fabs(fallback_plan.luminance_curve.floor
                     - fallback_plan.luminance_floor) < 0.0001f);
    assert(std::fabs(fallback_plan.luminance_curve.gain
                     - fallback_plan.luminance_gain) < 0.0001f);
    assert(std::fabs(fallback_plan.luminance_curve.gamma - 1.0f)
           < 0.0001f);
    assert(std::fabs(fallback_plan.luminance_curve.midpoint - 0.5f)
           < 0.0001f);
    assert(std::fabs(fallback_plan.luminance_curve.contrast - 1.0f)
           < 0.0001f);
    assert(std::fabs(fallback_plan.luminance_curve.edge_lift
                     - fallback_plan.edge_highlight) < 0.0001f);
    assert(!fallback_plan.luminance_curve.backdrop_driven);
    assert(fallback_plan.luminance_curve.bounded);
    assert(std::string(fallback_plan.backdrop.source) == "none");
    assert(std::string(fallback_plan.backdrop.luminance_response)
           == "not-sampled");
    assert(std::string(fallback_plan.foreground.scheme)
           == "solid-fallback");
    assert(std::string(fallback_plan.foreground.source)
           == "fallback-material");
    assert(!fallback_plan.foreground.backdrop_driven);
    assert(!fallback_plan.foreground.uses_vibrancy);
    assert(fallback_plan.foreground.deterministic);
    assert(fallback_plan.foreground.primary_contrast_ratio
           >= fallback_plan.foreground.minimum_contrast_ratio);
    assert(fallback_plan.foreground.secondary_contrast_ratio
           >= fallback_plan.foreground.minimum_contrast_ratio);
    assert(fallback_plan.foreground.accent_contrast_ratio
           >= fallback_plan.foreground.minimum_contrast_ratio);
    assert(std::string(fallback_plan.reference_model.technology)
           == "liquid-glass");
    assert(std::string(fallback_plan.reference_model.layer)
           == "functional-layer");
    assert(std::string(fallback_plan.reference_model.material_policy)
           == "liquid-glass-functional-layer");
    assert(std::string(fallback_plan.reference_model.variant) == "regular");
    assert(std::string(fallback_plan.reference_model.shape)
           == "rounded-rectangle");
    assert(std::string(fallback_plan.reference_model.shape_scope)
           == "view-bounds");
    assert(std::string(fallback_plan.reference_model.blending_scope)
           == "deterministic-fallback");
    assert(std::string(fallback_plan.reference_model.accessibility_response)
           == "standard");
    assert(std::string(fallback_plan.reference_model.performance_response)
           == "deterministic-fallback");
    assert(fallback_plan.reference_model.view_bounds_anchored);
    assert(fallback_plan.reference_model.shape_matches_geometry);
    assert(fallback_plan.reference_model.tint_applied);
    assert(!fallback_plan.reference_model.interactive_response);
    assert(!fallback_plan.reference_model.container_grouped);
    assert(!fallback_plan.reference_model.container_union);
    assert(!fallback_plan.reference_model.container_morphing);
    assert(fallback_plan.reference_model.legibility_preserved);
    assert(!fallback_plan.reference_model.vibrancy_expected);
    assert(fallback_plan.reference_model.deterministic_degradation);
    assert(std::string(fallback_plan.verifier.likely_layer)
           == "material-fallback-pass");
    assert(std::string(fallback_plan.verifier.likely_pass)
           == "translucent-rounded-rect");
    assert(fallback_plan.observation_contract.schema_version
           == material_plan_contract_version);
    assert(fallback_plan.observation_contract.semantic_material_required);
    assert(fallback_plan.observation_contract.runtime_plan_required);
    assert(fallback_plan.observation_contract.fallback_expected);
    assert(!fallback_plan.observation_contract.backdrop_sampling_expected);
    assert(!fallback_plan.observation_contract.stable_backdrop_required);
    assert(!fallback_plan.observation_contract.shared_frame_capture_required);
    assert(!fallback_plan.observation_contract.next_frame_capture_required);
    assert(!fallback_plan.observation_contract
                .backdrop_capture_excludes_foreground_text);
    assert(fallback_plan.observation_contract.bounded_texture_copy_required);
    assert(fallback_plan.observation_contract.deterministic_fallback_required);
    assert(std::string(fallback_plan.observation_contract.fallback_path)
           == "unsupported-backend");
    assert(std::string(fallback_plan.observation_contract.fallback_reason)
           == fallback_plan.fallback_reason);
    assert(std::string(fallback_plan.observation_contract.backdrop_capture_scope)
           == "none");
    assert(std::string(fallback_plan.observation_contract.backdrop_capture_reason)
           == "not-required");
    assert(std::string(fallback_plan.observation_contract.primary_pass)
           == "translucent-rounded-rect");
    assert(std::string(fallback_plan.observation_contract.primary_executor)
           == "fallback-fill");
    assert(fallback_plan.observation_contract.expected_runtime_passes == 1);
    assert(fallback_plan.observation_contract.expected_execution_stages
           == fallback_plan.execution_stage_count);
    assert(fallback_plan.observation_contract.expected_backdrop_execution_stages
           == 0);
    assert(fallback_plan.observation_contract.max_texture_copy_pixels == 0);
    assert(std::string(fallback_plan.observation_contract.region_name)
           == fallback_plan.verifier.region_name);
    assert(std::string(fallback_plan.observation_contract.likely_layer)
           == fallback_plan.verifier.likely_layer);
    assert(std::string(fallback_plan.observation_contract.likely_pass)
           == fallback_plan.verifier.likely_pass);

    MaterialEnvironment unsupported_large_env = fallback_env;
    unsupported_large_env.render_target.width = 2400;
    unsupported_large_env.render_target.height = 2400;
    auto unsupported_large_plan =
        plan_material_surface(request, unsupported_large_env);
    assert(unsupported_large_plan.fallback());
    assert(unsupported_large_plan.fallback_path
           == MaterialFallbackPath::UnsupportedBackend);
    assert(!unsupported_large_plan.render_target.within_backdrop_budget);

    MaterialEnvironment warmup_env = fallback_env;
    warmup_env.capabilities.material_backdrop_blur = true;
    warmup_env.capabilities.shader_blur = true;
    warmup_env.capabilities.frame_history = false;
    warmup_env.backdrop.available = false;
    warmup_env.backdrop.stable = false;
    warmup_env.backdrop.source = "none";
    auto warmup_plan = plan_material_surface(request, warmup_env);
    assert(warmup_plan.fallback());
    assert(warmup_plan.fallback_path == MaterialFallbackPath::NoBackdropSource);
    assert(warmup_plan.decision_trace.backend_supports_backdrop);
    assert(!warmup_plan.decision_trace.backdrop_source_ready);
    assert(warmup_plan.decision_trace.next_frame_capture_required);
    assert(!warmup_plan.backdrop_access.required);
    assert(!warmup_plan.backdrop_access.stable_required);
    assert(!warmup_plan.backdrop_access.frame_history_required);
    assert(warmup_plan.backdrop_access.shared_frame_capture);
    assert(warmup_plan.backdrop_access.next_frame_capture_required);
    assert(warmup_plan.backdrop_access.excludes_foreground_text);
    assert(std::string(warmup_plan.backdrop_access.capture_scope)
           == "shared-frame");
    assert(std::string(warmup_plan.backdrop_access.capture_reason)
           == "warmup-next-frame");
    assert(warmup_plan.backdrop_access.max_frame_capture_count == 1);
    assert(warmup_plan.backdrop_access.max_frame_capture_pixels
           == warmup_plan.render_target.pixel_count);
    assert(warmup_plan.backdrop_access.max_surface_sample_pixels == 0);
    assert(warmup_plan.resource_budget.max_frame_capture_count == 1);
    assert(warmup_plan.resource_budget.max_frame_capture_pixels
           == warmup_plan.render_target.pixel_count);
    assert(warmup_plan.resource_budget.max_surface_sample_pixels == 0);
    assert(warmup_plan.observation_contract.shared_frame_capture_required);
    assert(warmup_plan.observation_contract.next_frame_capture_required);
    assert(warmup_plan.observation_contract
                .backdrop_capture_excludes_foreground_text);
    assert(std::string(warmup_plan.reference_model.performance_response)
           == "warmup-capture");
    assert(std::string(warmup_plan.observation_contract.backdrop_capture_scope)
           == "shared-frame");
    assert(std::string(warmup_plan.observation_contract.backdrop_capture_reason)
           == "warmup-next-frame");
    assert(std::string(unsupported_large_plan.decision_trace.first_blocker)
           == "unsupported-backend");

    MaterialEnvironment glass_env = fallback_env;
    glass_env.capabilities.material_backdrop_blur = true;
    glass_env.capabilities.shader_blur = true;
    glass_env.capabilities.frame_history = true;
    glass_env.backdrop.available = true;
    glass_env.backdrop.stable = true;
    glass_env.backdrop.excludes_foreground_text = true;
    glass_env.backdrop.source = "previous-presented-frame";
    auto glass_plan = plan_material_surface(request, glass_env);
    assert(glass_plan.contract_version == material_plan_contract_version);
    assert(glass_plan.role == MaterialSurfaceRole::Surface);
    assert(glass_plan.container.mode == MaterialContainerMode::Isolated);
    assert(!glass_plan.fallback());
    assert(glass_plan.backdrop_sampling);
    assert(glass_plan.decision_trace.backend_supports_backdrop);
    assert(glass_plan.decision_trace.role_allows_liquid_glass);
    assert(!glass_plan.decision_trace.content_layer_standard_material);
    assert(glass_plan.decision_trace.liquid_glass_backdrop_candidate);
    assert(glass_plan.decision_trace.backdrop_source_ready);
    assert(glass_plan.decision_trace.can_sample_backdrop);
    assert(glass_plan.decision_trace.next_frame_capture_required);
    assert(std::string(glass_plan.decision_trace.first_blocker) == "none");
    assert(glass_plan.render_target.ready);
    assert(glass_plan.render_target.within_backdrop_budget);
    assert(glass_plan.blur_radius >= 20.0f);
    assert(glass_plan.saturation > 1.0f);
    assert(glass_plan.edge_highlight > 0.0f);
    assert(glass_plan.shadow_alpha > 0.0f);
    assert(std::string(glass_plan.primary_pass.name)
           == "backdrop-sample-blur");
    assert(glass_plan.primary_pass.active);
    assert(glass_plan.primary_pass.requires_backdrop);
    assert(glass_plan.primary_pass.sample_taps == glass_plan.sample_taps);
    assert(std::string(glass_plan.primary_pass.executor)
           == "backdrop-filter");
    assert(glass_plan.primary_pass.max_texture_copy_pixels
           == glass_plan.render_target.pixel_count);
    assert(glass_plan.resource_budget.max_execution_stages == 4);
    assert(glass_plan.execution_stage_capacity == 4);
    assert(glass_plan.execution_stage_count == 4);
    assert(glass_plan.dropped_execution_stage_count == 0);
    assert(std::string(glass_plan.execution_stages[0].name)
           == "shape-shadow");
    assert(!glass_plan.execution_stages[0].requires_backdrop);
    assert(std::string(glass_plan.execution_stages[1].name)
           == "backdrop-sample-blur");
    assert(glass_plan.execution_stages[1].requires_backdrop);
    assert(glass_plan.execution_stages[1].sample_taps
           == glass_plan.sample_taps);
    assert(glass_plan.execution_stages[1].max_texture_copy_pixels
           == glass_plan.render_target.pixel_count);
    assert(std::string(glass_plan.execution_stages[2].name)
           == "edge-highlight");
    assert(std::string(glass_plan.execution_stages[3].name)
           == "noise-dither");
    for (unsigned int i = 0; i < glass_plan.execution_stage_count; ++i) {
        assert(glass_plan.execution_stages[i].active);
        assert(glass_plan.execution_stages[i].bounded);
    }
    assert(std::string(glass_plan.sampling_kernel.name)
           == "weighted-5x5-manhattan");
    assert(glass_plan.sampling_kernel.radius == 2);
    assert(glass_plan.sampling_kernel.sample_taps == glass_plan.sample_taps);
    assert(std::fabs(glass_plan.sampling_kernel.blur_step_scale - 0.35f)
           < 0.0001f);
    assert(std::string(glass_plan.sampling_kernel.weight_profile)
           == "center4-cardinal2-diagonal1");
    assert(glass_plan.sampling_kernel.requires_backdrop);
    assert(glass_plan.sampling_kernel.bounded);
    assert(std::string(glass_plan.luminance_curve.name)
           == "adaptive-backdrop-luma");
    assert(std::fabs(glass_plan.luminance_curve.floor
                     - glass_plan.luminance_floor) < 0.0001f);
    assert(std::fabs(glass_plan.luminance_curve.gain
                     - glass_plan.luminance_gain) < 0.0001f);
    assert(std::fabs(glass_plan.luminance_curve.gamma - 1.0f)
           < 0.0001f);
    assert(std::fabs(glass_plan.luminance_curve.midpoint - 0.5f)
           < 0.0001f);
    assert(std::fabs(glass_plan.luminance_curve.contrast - 1.0f)
           < 0.0001f);
    assert(std::fabs(glass_plan.luminance_curve.edge_lift
                     - glass_plan.edge_highlight) < 0.0001f);
    assert(glass_plan.luminance_curve.backdrop_driven);
    assert(glass_plan.luminance_curve.bounded);
    assert(glass_plan.resource_budget.max_sampling_kernel_radius == 2);
    assert(glass_plan.backdrop_access.required);
    assert(glass_plan.backdrop_access.shared_frame_capture);
    assert(glass_plan.backdrop_access.next_frame_capture_required);
    assert(glass_plan.backdrop_access.excludes_foreground_text);
    assert(std::string(glass_plan.backdrop_access.capture_scope)
           == "shared-frame");
    assert(std::string(glass_plan.backdrop_access.capture_reason)
           == "sample-current-frame");
    assert(glass_plan.backdrop.available);
    assert(glass_plan.backdrop.stable);
    assert(glass_plan.backdrop.excludes_foreground_text);
    assert(std::string(glass_plan.backdrop.source)
           == "previous-presented-frame");
    assert(std::string(glass_plan.backdrop.luminance_response)
           == "neutral");
    assert(std::fabs(glass_plan.backdrop.luminance_floor_delta) < 0.0001f);
    assert(std::fabs(glass_plan.backdrop.luminance_gain_delta) < 0.0001f);
    assert(std::fabs(glass_plan.backdrop.edge_highlight_delta) < 0.0001f);
    assert(std::string(glass_plan.foreground.scheme)
           == "vibrant-balanced");
    assert(std::string(glass_plan.foreground.source)
           == "backdrop-analysis");
    assert(glass_plan.foreground.backdrop_driven);
    assert(glass_plan.foreground.uses_vibrancy);
    assert(!glass_plan.foreground.high_contrast);
    assert(glass_plan.foreground.deterministic);
    assert(glass_plan.foreground.background_luma > 0.60f);
    assert(glass_plan.foreground.background_luma < 0.75f);
    assert(glass_plan.foreground.primary_contrast_ratio
           >= glass_plan.foreground.minimum_contrast_ratio);
    assert(glass_plan.foreground.secondary_contrast_ratio
           >= glass_plan.foreground.minimum_contrast_ratio);
    assert(glass_plan.foreground.accent_contrast_ratio
           >= glass_plan.foreground.minimum_contrast_ratio);
    assert(std::string(glass_plan.reference_model.blending_scope)
           == "sampled-backdrop");
    assert(std::string(glass_plan.reference_model.technology)
           == "liquid-glass");
    assert(std::string(glass_plan.reference_model.layer)
           == "functional-layer");
    assert(std::string(glass_plan.reference_model.material_policy)
           == "liquid-glass-functional-layer");
    assert(std::string(glass_plan.reference_model.semantic_thickness)
           == "regular");
    assert(std::string(glass_plan.reference_model.accessibility_response)
           == "standard");
    assert(std::string(glass_plan.reference_model.performance_response)
           == "standard");
    assert(glass_plan.reference_model.view_bounds_anchored);
    assert(glass_plan.reference_model.shape_matches_geometry);
    assert(glass_plan.reference_model.tint_applied);
    assert(!glass_plan.reference_model.interactive_response);
    assert(!glass_plan.reference_model.container_grouped);
    assert(!glass_plan.reference_model.container_union);
    assert(!glass_plan.reference_model.container_morphing);
    assert(glass_plan.reference_model.legibility_preserved);
    assert(glass_plan.reference_model.vibrancy_expected);
    assert(glass_plan.reference_model.deterministic_degradation);
    assert(std::string(glass_plan.verifier.likely_layer)
           == "material-blur-pass");
    assert(std::string(glass_plan.verifier.likely_pass)
           == "backdrop-sample-blur");
    assert(glass_plan.observation_contract.schema_version
           == material_plan_contract_version);
    assert(glass_plan.observation_contract.semantic_material_required);
    assert(glass_plan.observation_contract.runtime_plan_required);
    assert(!glass_plan.observation_contract.fallback_expected);
    assert(glass_plan.observation_contract.backdrop_sampling_expected);
    assert(glass_plan.observation_contract.stable_backdrop_required);
    assert(glass_plan.observation_contract.shared_frame_capture_required);
    assert(glass_plan.observation_contract.next_frame_capture_required);
    assert(glass_plan.observation_contract
                .backdrop_capture_excludes_foreground_text);
    assert(std::string(glass_plan.observation_contract.backdrop_capture_scope)
           == "shared-frame");
    assert(std::string(glass_plan.observation_contract.backdrop_capture_reason)
           == "sample-current-frame");
    assert(glass_plan.observation_contract.expected_runtime_passes == 1);
    assert(glass_plan.observation_contract.expected_execution_stages == 4);
    assert(glass_plan.observation_contract.expected_active_execution_stages == 4);
    assert(glass_plan.observation_contract.expected_backdrop_execution_stages == 1);
    assert(glass_plan.observation_contract.max_texture_copy_pixels
           == glass_plan.render_target.pixel_count);
    assert(std::string(glass_plan.observation_contract.primary_pass)
           == "backdrop-sample-blur");
    assert(std::string(glass_plan.observation_contract.primary_executor)
           == "backdrop-filter");

    MaterialRequest container_request = request;
    container_request.style.container = MaterialContainerDescriptor{
        41u,
        7u,
        24.0f,
        true,
        true};
    auto container_plan = plan_material_surface(container_request, glass_env);
    assert(!container_plan.fallback());
    assert(container_plan.container.mode == MaterialContainerMode::Union);
    assert(std::string(container_plan.container.mode_name) == "union");
    assert(container_plan.container.container_id == 41u);
    assert(container_plan.container.union_id == 7u);
    assert(container_plan.container.spacing == 24.0f);
    assert(container_plan.container.participates);
    assert(container_plan.container.shared_backdrop_scope);
    assert(container_plan.container.shape_union_expected);
    assert(container_plan.container.interactive);
    assert(container_plan.container.morph_transitions);
    assert(container_plan.command_descriptor.container.container_id == 41u);
    assert(container_plan.resource_budget.max_container_spacing == 24.0f);
    assert(container_plan.verifier.require_container_identity);
    assert(container_plan.verifier.require_container_morph_contract);
    assert(container_plan.reference_model.interactive_response);
    assert(container_plan.reference_model.container_grouped);
    assert(container_plan.reference_model.container_union);
    assert(container_plan.reference_model.container_morphing);

    MaterialRequest clamped_shape_request = request;
    clamped_shape_request.geometry.radius = 200.0f;
    auto clamped_shape_plan =
        plan_material_surface(clamped_shape_request, glass_env);
    assert(!clamped_shape_plan.fallback());
    assert(clamped_shape_plan.shape.valid);
    assert(clamped_shape_plan.shape.rounded);
    assert(clamped_shape_plan.shape.radius_clamped);
    assert(clamped_shape_plan.shape.radius_limit == 48.0f);
    assert(clamped_shape_plan.shape.effective_radius == 48.0f);
    assert(clamped_shape_plan.shape.normalized_radius == 1.0f);

    MaterialEnvironment reduced_motion_container_env = glass_env;
    reduced_motion_container_env.capabilities.reduce_motion = true;
    auto reduced_motion_container_plan =
        plan_material_surface(container_request, reduced_motion_container_env);
    assert(reduced_motion_container_plan.container.participates);
    assert(reduced_motion_container_plan.container.interactive);
    assert(!reduced_motion_container_plan.container.morph_transitions);
    assert(!reduced_motion_container_plan.verifier.require_container_morph_contract);
    assert(std::string(
               reduced_motion_container_plan.reference_model
                   .accessibility_response)
           == "reduced-motion");

    MaterialEnvironment dark_backdrop_env = glass_env;
    dark_backdrop_env.backdrop.luma_min = 0.02f;
    dark_backdrop_env.backdrop.luma_max = 0.28f;
    dark_backdrop_env.backdrop.luma_mean = 0.12f;
    auto dark_backdrop_plan =
        plan_material_surface(request, dark_backdrop_env);
    assert(dark_backdrop_plan.backdrop_sampling);
    assert(dark_backdrop_plan.luminance_floor
           > glass_plan.luminance_floor);
    assert(dark_backdrop_plan.luminance_gain
           > glass_plan.luminance_gain);
    assert(dark_backdrop_plan.edge_highlight
           > glass_plan.edge_highlight);
    assert(std::string(dark_backdrop_plan.backdrop.luminance_response)
           == "dark");
    assert(dark_backdrop_plan.backdrop.luminance_floor_delta > 0.0f);
    assert(dark_backdrop_plan.backdrop.luminance_gain_delta > 0.0f);
    assert(dark_backdrop_plan.backdrop.edge_highlight_delta > 0.0f);
    assert(dark_backdrop_plan.luminance_curve.gamma < 1.0f);
    assert(dark_backdrop_plan.luminance_curve.midpoint < 0.35f);
    assert(std::string(dark_backdrop_plan.foreground.scheme)
           == "vibrant-light");
    assert(dark_backdrop_plan.foreground.primary_contrast_ratio
           >= dark_backdrop_plan.foreground.minimum_contrast_ratio);

    MaterialEnvironment bright_backdrop_env = glass_env;
    bright_backdrop_env.backdrop.luma_min = 0.84f;
    bright_backdrop_env.backdrop.luma_max = 0.97f;
    bright_backdrop_env.backdrop.luma_mean = 0.90f;
    auto bright_backdrop_plan =
        plan_material_surface(request, bright_backdrop_env);
    assert(bright_backdrop_plan.backdrop_sampling);
    assert(bright_backdrop_plan.luminance_gain
           < glass_plan.luminance_gain);
    assert(bright_backdrop_plan.edge_highlight
           > glass_plan.edge_highlight);
    assert(std::string(bright_backdrop_plan.backdrop.luminance_response)
           == "bright");
    assert(bright_backdrop_plan.backdrop.luminance_gain_delta < 0.0f);
    assert(bright_backdrop_plan.backdrop.edge_highlight_delta > 0.0f);
    assert(bright_backdrop_plan.luminance_curve.gamma > 1.0f);
    assert(bright_backdrop_plan.luminance_curve.midpoint > 0.70f);
    assert(std::string(bright_backdrop_plan.foreground.scheme)
           == "vibrant-dark");
    assert(bright_backdrop_plan.foreground.primary_contrast_ratio
           >= bright_backdrop_plan.foreground.minimum_contrast_ratio);

    MaterialEnvironment flat_backdrop_env = glass_env;
    flat_backdrop_env.backdrop.luma_min = 0.46f;
    flat_backdrop_env.backdrop.luma_max = 0.50f;
    flat_backdrop_env.backdrop.luma_mean = 0.48f;
    auto flat_backdrop_plan =
        plan_material_surface(request, flat_backdrop_env);
    assert(flat_backdrop_plan.backdrop_sampling);
    assert(flat_backdrop_plan.backdrop.luma_span < 0.05f);
    assert(std::string(flat_backdrop_plan.backdrop.luminance_response)
           == "flat");
    assert(std::fabs(flat_backdrop_plan.backdrop.luminance_floor_delta)
           < 0.0001f);
    assert(std::fabs(flat_backdrop_plan.backdrop.luminance_gain_delta)
           < 0.0001f);
    assert(flat_backdrop_plan.backdrop.edge_highlight_delta > 0.0f);
    assert(flat_backdrop_plan.luminance_curve.contrast > 1.0f);

    MaterialEnvironment contrast_motion_env = glass_env;
    contrast_motion_env.capabilities.increase_contrast = true;
    contrast_motion_env.capabilities.reduce_motion = true;
    auto contrast_motion_plan =
        plan_material_surface(request, contrast_motion_env);
    assert(contrast_motion_plan.backdrop_sampling);
    assert(contrast_motion_plan.decision_trace.increase_contrast);
    assert(contrast_motion_plan.decision_trace.reduce_motion);
    assert(contrast_motion_plan.opacity > glass_plan.opacity);
    assert(contrast_motion_plan.luminance_floor > glass_plan.luminance_floor);
    assert(contrast_motion_plan.saturation <= 1.0f);
    assert(contrast_motion_plan.noise_opacity == 0.0f);
    assert(contrast_motion_plan.sample_taps < glass_plan.sample_taps);
    assert(contrast_motion_plan.primary_pass.sample_taps
           == contrast_motion_plan.sample_taps);
    assert(std::string(contrast_motion_plan.foreground.scheme)
           == "high-contrast");
    assert(std::string(contrast_motion_plan.foreground.source)
           == "accessibility");
    assert(contrast_motion_plan.foreground.high_contrast);
    assert(std::string(contrast_motion_plan.reference_model
                           .accessibility_response)
           == "combined-accessibility");
    assert(std::string(contrast_motion_plan.reference_model
                           .performance_response)
           == "budgeted-effects");

    glass_env.capabilities.reduce_transparency = true;
    auto reduced_plan = plan_material_surface(request, glass_env);
    assert(reduced_plan.fallback());
    assert(reduced_plan.fallback_path == MaterialFallbackPath::ReducedTransparency);
    assert(!reduced_plan.backdrop_sampling);
    assert(reduced_plan.sample_taps == 0);
    assert(reduced_plan.primary_pass.sample_taps == 0);
    assert(reduced_plan.noise_opacity == 0.0f);
    assert(std::string(reduced_plan.primary_pass.name)
           == "translucent-rounded-rect");
    assert(reduced_plan.execution_stage_count == 3);
    assert(std::string(reduced_plan.execution_stages[1].name)
           == "translucent-rounded-rect");
    assert(!reduced_plan.execution_stages[1].requires_backdrop);
    assert(std::string(reduced_plan.reference_model.accessibility_response)
           == "reduced-transparency");
    assert(std::string(reduced_plan.reference_model.performance_response)
           == "deterministic-fallback");

    glass_env.capabilities.reduce_transparency = false;
    MaterialEnvironment disabled_quality_env = glass_env;
    disabled_quality_env.quality.allow_backdrop_sampling = false;
    auto disabled_quality_plan =
        plan_material_surface(request, disabled_quality_env);
    assert(disabled_quality_plan.fallback());
    assert(disabled_quality_plan.fallback_path
           == MaterialFallbackPath::QualityPolicy);
    assert(!disabled_quality_plan.quality_policy.allow_backdrop_sampling);
    assert(!disabled_quality_plan.backdrop_sampling);
    assert(disabled_quality_plan.blur_radius == 0.0f);
    assert(disabled_quality_plan.sample_taps == 0);
    assert(disabled_quality_plan.primary_pass.sample_taps == 0);
    assert(std::string(disabled_quality_plan.fallback_reason)
           == "quality policy disables material backdrop sampling");
    assert(std::string(disabled_quality_plan.primary_pass.name)
           == "translucent-rounded-rect");

    MaterialEnvironment zero_tap_env = glass_env;
    zero_tap_env.quality.max_sample_taps = 0;
    auto zero_tap_plan = plan_material_surface(request, zero_tap_env);
    assert(zero_tap_plan.fallback());
    assert(zero_tap_plan.fallback_path == MaterialFallbackPath::QualityPolicy);
    assert(zero_tap_plan.quality_policy.max_sample_taps == 0);
    assert(zero_tap_plan.sample_taps == 0);
    assert(zero_tap_plan.primary_pass.sample_taps == 0);

    MaterialEnvironment budget_env = glass_env;
    budget_env.capabilities.reduce_transparency = false;
    budget_env.quality.max_blur_radius = 12.0f;
    budget_env.quality.max_sample_taps = 7;
    budget_env.quality.max_backdrop_pixels = 600'000;
    budget_env.quality.allow_noise = false;
    budget_env.quality.allow_shadow = false;
    auto budget_plan = plan_material_surface(request, budget_env);
    assert(budget_plan.blur_radius == 12.0f);
    assert(budget_plan.sample_taps == 5);
    assert(budget_plan.primary_pass.sample_taps == 5);
    assert(budget_plan.noise_opacity == 0.0f);
    assert(budget_plan.shadow_alpha == 0.0f);
    assert(budget_plan.shadow_radius == 0.0f);
    assert(budget_plan.quality_policy.max_blur_radius == 12.0f);
    assert(budget_plan.quality_policy.max_sample_taps == 7);
    assert(budget_plan.quality_policy.max_backdrop_pixels == 600'000);
    assert(!budget_plan.quality_policy.allow_noise);
    assert(!budget_plan.quality_policy.allow_shadow);
    assert(budget_plan.resource_budget.max_blur_radius == 12.0f);
    assert(budget_plan.resource_budget.max_sample_taps == 5);
    assert(budget_plan.resource_budget.max_sampling_kernel_radius == 2);
    assert(budget_plan.resource_budget.max_pass_count == 1);
    assert(budget_plan.resource_budget.max_execution_stages == 4);
    assert(budget_plan.execution_stage_capacity == 4);
    assert(budget_plan.resource_budget.max_backdrop_pixels == 600'000);
    assert(budget_plan.resource_budget.bounded_texture_copy);
    assert(budget_plan.resource_budget.deterministic_fallback);
    assert(budget_plan.execution_stage_count == 2);
    assert(budget_plan.dropped_execution_stage_count == 0);
    assert(std::string(budget_plan.reference_model.performance_response)
           == "budgeted-effects");
    assert(std::string(budget_plan.execution_stages[0].name)
           == "backdrop-sample-blur");
    assert(std::string(budget_plan.execution_stages[1].name)
           == "edge-highlight");

    MaterialEnvironment oversize_env = glass_env;
    oversize_env.quality.max_backdrop_pixels = 100;
    auto oversize_plan = plan_material_surface(request, oversize_env);
    assert(oversize_plan.fallback());
    assert(oversize_plan.fallback_path == MaterialFallbackPath::QualityPolicy);
    assert(oversize_plan.render_target.pixel_count == 395'200);
    assert(!oversize_plan.render_target.within_backdrop_budget);
    assert(!oversize_plan.decision_trace.backdrop_pixels_within_budget);
    assert(std::string(oversize_plan.decision_trace.first_blocker)
           == "quality-policy");
    assert(oversize_plan.quality_policy.max_backdrop_pixels == 100);
    assert(oversize_plan.resource_budget.max_backdrop_pixels == 100);
    assert(!oversize_plan.backdrop_sampling);
    assert(oversize_plan.sample_taps == 0);
    assert(oversize_plan.sampling_kernel.radius == 0);
    assert(oversize_plan.resource_budget.max_sampling_kernel_radius == 0);
    assert(oversize_plan.primary_pass.sample_taps == 0);
    assert(std::string(oversize_plan.fallback_reason)
           == "quality policy backdrop pixel budget exceeded");

    MaterialRequest invalid_request = request;
    invalid_request.geometry.w = 0.0f;
    auto invalid_plan = plan_material_surface(invalid_request, glass_env);
    assert(invalid_plan.fallback());
    assert(invalid_plan.fallback_path == MaterialFallbackPath::InvalidGeometry);
    assert(!invalid_plan.primary_pass.active);
    assert(std::string(invalid_plan.primary_pass.executor) == "none");
    assert(invalid_plan.primary_pass.max_texture_copy_pixels == 0);
    assert(std::string(invalid_plan.verifier.likely_pass) == "none");
    assert(invalid_plan.execution_stage_count == 0);
    assert(invalid_plan.dropped_execution_stage_count == 0);
    assert(invalid_plan.observation_contract.fallback_expected);
    assert(!invalid_plan.observation_contract.backdrop_sampling_expected);
    assert(std::string(invalid_plan.observation_contract.fallback_path)
           == "invalid-geometry");
    assert(std::string(invalid_plan.observation_contract.primary_pass)
           == "none");
    assert(invalid_plan.observation_contract.expected_execution_stages == 0);
    assert(!invalid_plan.shape.valid);
    assert(!invalid_plan.shape.rounded);
    assert(invalid_plan.shape.effective_radius == 0.0f);
    assert(std::string(invalid_plan.reference_model.shape) == "invalid");
    assert(!invalid_plan.reference_model.view_bounds_anchored);
    assert(!invalid_plan.reference_model.shape_matches_geometry);
    assert(!invalid_plan.reference_model.tint_applied);
    assert(invalid_plan.reference_model.deterministic_degradation);
    assert(!invalid_plan.decision_trace.has_geometry);
    assert(!invalid_plan.decision_trace.has_material);
    assert(std::string(invalid_plan.decision_trace.first_blocker)
           == "invalid-geometry");

    MaterialPlan capacity_plan;
    capacity_plan.execution_stage_capacity = 1;
    MaterialExecutionStage active_stage{
        .name = "shape-shadow",
        .active = true,
        .requires_backdrop = false,
        .sample_taps = 0,
        .likely_layer = "material-shadow-pass",
        .executor = "shape-shadow",
        .max_texture_copy_pixels = 0,
        .bounded = true,
    };
    append_material_execution_stage(capacity_plan, active_stage);
    append_material_execution_stage(capacity_plan, active_stage);
    assert(capacity_plan.execution_stage_count == 1);
    assert(capacity_plan.dropped_execution_stage_count == 1);

    std::puts("PASS: material planner resolves backdrop and fallback paths");
}

void test_material_text_foreground_resolution() {
    Theme theme{};
    auto style = material_style_for_kind(MaterialKind::Regular, theme);
    MaterialRequest request{
        style,
        MaterialGeometry{12.0f, 20.0f, 240.0f, 96.0f, 10.0f},
    };

    MaterialEnvironment env{};
    env.capabilities.material_surfaces = true;
    env.capabilities.material_backdrop_blur = true;
    env.capabilities.shader_blur = true;
    env.capabilities.frame_history = true;
    env.backdrop.available = true;
    env.backdrop.stable = true;
    env.backdrop.excludes_foreground_text = true;
    env.backdrop.source = "previous-presented-frame";
    env.backdrop.luma_min = 0.84f;
    env.backdrop.luma_max = 0.97f;
    env.backdrop.luma_mean = 0.90f;
    env.render_target.width = 520;
    env.render_target.height = 760;
    env.render_target.scale = 2.0f;

    auto plan = plan_material_surface(request, env);
    assert(plan.backdrop_sampling);
    assert(plan.foreground.backdrop_driven);
    std::vector<MaterialRuntimeRecord> records{
        MaterialRuntimeRecord{plan, 4u},
    };

    auto primary = material_resolve_text_foreground(
        records,
        5u,
        24.0f,
        36.0f,
        Color{theme.foreground.r, theme.foreground.g, theme.foreground.b, 96},
        theme);
    assert(primary.has_material);
    assert(primary.remapped);
    assert(std::string(primary.role) == "primary");
    assert(primary.material_command_index == 4u);
    assert(material_color_rgb_equal(primary.color, plan.foreground.primary));
    assert(primary.color.a == 96);

    auto secondary = material_resolve_text_foreground(
        records,
        5u,
        24.0f,
        36.0f,
        theme.muted,
        theme);
    assert(secondary.has_material);
    assert(secondary.remapped);
    assert(std::string(secondary.role) == "secondary");
    assert(material_color_rgb_equal(secondary.color, plan.foreground.secondary));

    auto accent = material_resolve_text_foreground(
        records,
        5u,
        24.0f,
        36.0f,
        theme.accent,
        theme);
    assert(accent.has_material);
    assert(accent.remapped);
    assert(std::string(accent.role) == "accent");
    assert(material_color_rgb_equal(accent.color, plan.foreground.accent));

    Color custom{12, 34, 56, 200};
    auto custom_result = material_resolve_text_foreground(
        records,
        5u,
        24.0f,
        36.0f,
        custom,
        theme);
    assert(custom_result.has_material);
    assert(!custom_result.remapped);
    assert(material_color_rgb_equal(custom_result.color, custom));
    assert(custom_result.color.a == custom.a);

    auto outside = material_resolve_text_foreground(
        records,
        5u,
        400.0f,
        36.0f,
        theme.foreground,
        theme);
    assert(!outside.has_material);
    assert(!outside.remapped);

    auto before_material = material_resolve_text_foreground(
        records,
        3u,
        24.0f,
        36.0f,
        theme.foreground,
        theme);
    assert(!before_material.has_material);
    assert(!before_material.remapped);

    std::puts("PASS: material text foreground resolution");
}

void test_material_surface_emits_material_rect_command() {
    detail::g_app.arena.reset();
    detail::g_app.prev_arena.reset();
    detail::g_app.callbacks.clear();
    CMD_LEN = 0;

    auto root_h = detail::alloc_node();
    detail::node_at(root_h).style.flex_direction = FlexDirection::Column;
    Scope scope(root_h);
    Scope::set_current(&scope);
    layout::material_surface(MaterialKind::Regular, [] {
        widget::text("Glass command");
    });
    Scope::set_current(nullptr);

    LAYOUT_NODE(root_h, 320.0f);
    PAINT_NODE(root_h, 0, 0, 0, 600.0f);

    auto cmds = parse_commands(CMD_BUF, CMD_LEN);
    auto const* material = static_cast<MaterialRectCmd const*>(nullptr);
    for (auto const& cmd : cmds) {
        if (auto const* m = std::get_if<MaterialRectCmd>(&cmd)) {
            material = m;
            break;
        }
    }

    assert(material != nullptr);
    auto const& descriptor = material->material;
    assert(descriptor.kind == MaterialKind::Regular);
    assert(descriptor.role == MaterialSurfaceRole::Surface);
    assert(descriptor.container.container_id == 0u);
    assert(descriptor.container.union_id == 0u);
    assert(!descriptor.container.interactive);
    assert(descriptor.opacity > 0.5f);
    assert(descriptor.blur_radius >= 20.0f);
    assert(descriptor.tint.a > 0);
    assert(descriptor.saturation > 1.0f);
    assert(descriptor.luminance_floor > 0.0f);
    assert(descriptor.luminance_gain > 1.0f);
    assert(descriptor.edge_highlight > 0.0f);
    assert(descriptor.edge_width >= 1.0f);
    assert(descriptor.noise_opacity > 0.0f);
    assert(descriptor.shadow_alpha > 0.0f);
    assert(descriptor.shadow_radius > 0.0f);

    std::puts("PASS: material surface emits MaterialRect command");
}

void test_material_surface_shape_overrides() {
    detail::g_app.arena.reset();
    detail::g_app.prev_arena.reset();
    detail::g_app.callbacks.clear();
    CMD_LEN = 0;

    auto root_h = detail::alloc_node();
    detail::node_at(root_h).style.flex_direction = FlexDirection::Column;
    Scope scope(root_h);
    Scope::set_current(&scope);
    layout::material_surface(
        layout::MaterialSurfaceOptions{
            .kind = MaterialKind::Regular,
            .role = MaterialSurfaceRole::Content,
            .fixed_height = 40.0f,
            .border_radius = 7.5f,
            .border_width = 0.0f,
            .semantic_label = "Shaped Material",
        },
        [] {
            widget::text("Override radius");
        });
    Scope::set_current(nullptr);

    LAYOUT_NODE(root_h, 320.0f);
    PAINT_NODE(root_h, 0, 0, 0, 600.0f);

    auto cmds = parse_commands(CMD_BUF, CMD_LEN);
    auto const* material = static_cast<MaterialRectCmd const*>(nullptr);
    bool saw_stroke = false;
    for (auto const& cmd : cmds) {
        if (auto const* m = std::get_if<MaterialRectCmd>(&cmd))
            material = m;
        if (std::holds_alternative<StrokeRectCmd>(cmd))
            saw_stroke = true;
    }

    assert(material != nullptr);
    assert(std::fabs(material->radius - 7.5f) < 0.0001f);
    assert(material->material.role == MaterialSurfaceRole::Content);
    assert(!saw_stroke);

    std::puts("PASS: material surface shape overrides");
}

void test_glass_surface_presets_emit_material_contract() {
    auto toolbar = layout::glass_surface_options(
        layout::GlassSurfacePreset::Toolbar);
    assert(toolbar.kind == MaterialKind::Clear);
    assert(toolbar.role == MaterialSurfaceRole::Toolbar);
    assert(toolbar.direction == FlexDirection::Row);
    assert(toolbar.cross_align == CrossAxisAlignment::Center);
    assert(toolbar.border_radius == 0.0f);
    assert(std::string_view{toolbar.semantic_label} == "Toolbar");
    assert(std::string_view{
        layout::glass_surface_preset_name(
            layout::GlassSurfacePreset::ToolbarGroup)}
        == "toolbar_group");

    auto content = layout::glass_surface_options(
        layout::GlassSurfacePreset::Content,
        "Files");
    assert(content.kind == MaterialKind::Regular);
    assert(content.role == MaterialSurfaceRole::Content);
    assert(content.padding == SpaceToken::Xl);
    assert(std::string_view{content.semantic_label} == "Files");

    detail::g_app.arena.reset();
    detail::g_app.prev_arena.reset();
    detail::g_app.callbacks.clear();
    CMD_LEN = 0;

    auto root_h = detail::alloc_node();
    detail::node_at(root_h).style.flex_direction = FlexDirection::Column;
    Scope scope(root_h);
    Scope::set_current(&scope);
    layout::glass_surface(
        layout::GlassSurfacePreset::ToolbarGroup,
        [] {
            widget::text("Preset command");
        },
        "Preset Toolbar Group");
    Scope::set_current(nullptr);

    auto const& root = detail::node_at(root_h);
    assert(root.children.size() == 1);
    auto const& surface = detail::node_at(root.children[0]);
    assert(surface.material.kind == MaterialKind::Thick);
    assert(surface.material.role == MaterialSurfaceRole::Toolbar);
    assert(surface.border_width == 0.0f);
    assert(surface.border_radius == detail::g_app.theme.radius_lg);
    assert(std::string_view{surface.debug_semantic_label}
           == "Preset Toolbar Group");

    LAYOUT_NODE(root_h, 320.0f);
    PAINT_NODE(root_h, 0, 0, 0, 600.0f);

    auto cmds = parse_commands(CMD_BUF, CMD_LEN);
    auto const* material = static_cast<MaterialRectCmd const*>(nullptr);
    for (auto const& cmd : cmds) {
        if (auto const* m = std::get_if<MaterialRectCmd>(&cmd)) {
            material = m;
            break;
        }
    }
    assert(material != nullptr);
    assert(material->material.kind == MaterialKind::Thick);
    assert(material->material.role == MaterialSurfaceRole::Toolbar);

    std::puts("PASS: glass surface presets emit material contract");
}

void test_material_container_scope_emits_command_context() {
    detail::g_app.arena.reset();
    detail::g_app.prev_arena.reset();
    detail::g_app.callbacks.clear();
    CMD_LEN = 0;

    auto root_h = detail::alloc_node();
    detail::node_at(root_h).style.flex_direction = FlexDirection::Column;
    Scope scope(root_h);
    Scope::set_current(&scope);
    layout::material_container(
        layout::MaterialContainerOptions{
            .container_id = 501u,
            .union_id = 9u,
            .spacing = 22.0f,
            .interactive = true,
            .morph_transitions = true,
        },
        [] {
            layout::material_surface(MaterialKind::Thin, [] {
                widget::text("Container glass");
            });
        });
    Scope::set_current(nullptr);

    LAYOUT_NODE(root_h, 320.0f);
    PAINT_NODE(root_h, 0, 0, 0, 600.0f);

    auto cmds = parse_commands(CMD_BUF, CMD_LEN);
    auto const* material = static_cast<MaterialRectCmd const*>(nullptr);
    for (auto const& cmd : cmds) {
        if (auto const* m = std::get_if<MaterialRectCmd>(&cmd)) {
            material = m;
            break;
        }
    }

    assert(material != nullptr);
    auto const& container = material->material.container;
    assert(container.container_id == 501u);
    assert(container.union_id == 9u);
    assert(std::fabs(container.spacing - 22.0f) < 0.0001f);
    assert(container.interactive);
    assert(container.morph_transitions);

    std::puts("PASS: material container scope emits command context");
}

void test_material_command_preserves_style_optics() {
    detail::g_app.arena.reset();
    detail::g_app.prev_arena.reset();
    detail::g_app.callbacks.clear();
    CMD_LEN = 0;

    auto root_h = detail::alloc_node();
    auto material_h = detail::alloc_node();
    auto& root = detail::node_at(root_h);
    root.style.flex_direction = FlexDirection::Column;
    root.children.push_back(material_h);

    auto& material = detail::node_at(material_h);
    material.material = layout::material_style(MaterialKind::Regular);
    material.material.role = MaterialSurfaceRole::Content;
    material.material.container = MaterialContainerDescriptor{
        88u,
        12u,
        18.0f,
        true,
        true};
    material.material.opacity = 0.63f;
    material.material.blur_radius = 18.0f;
    material.material.tint = Color{32, 64, 96, 144};
    material.material.saturation = 0.73f;
    material.material.luminance_floor = 0.19f;
    material.material.luminance_gain = 1.31f;
    material.material.edge_highlight = 0.57f;
    material.material.edge_width = 2.25f;
    material.material.noise_opacity = 0.031f;
    material.material.shadow_alpha = 0.22f;
    material.material.shadow_radius = 17.0f;
    material.background = material.material.tint;
    material.border_color = material.material.border;
    material.border_width = 1.0f;
    material.border_radius = 6.0f;
    material.style.fixed_height = 40.0f;

    LAYOUT_NODE(root_h, 320.0f);
    PAINT_NODE(root_h, 0, 0, 0, 600.0f);

    auto cmds = parse_commands(CMD_BUF, CMD_LEN);
    auto const* cmd = static_cast<MaterialRectCmd const*>(nullptr);
    for (auto const& parsed : cmds) {
        if (auto const* material_cmd = std::get_if<MaterialRectCmd>(&parsed)) {
            cmd = material_cmd;
            break;
        }
    }

    assert(cmd != nullptr);
    auto const& descriptor = cmd->material;
    assert(descriptor.kind == MaterialKind::Regular);
    assert(descriptor.role == MaterialSurfaceRole::Content);
    assert(descriptor.container.container_id == 88u);
    assert(descriptor.container.union_id == 12u);
    assert(std::fabs(descriptor.container.spacing - 18.0f) < 0.0001f);
    assert(descriptor.container.interactive);
    assert(descriptor.container.morph_transitions);
    assert(std::fabs(descriptor.opacity - 0.63f) < 0.0001f);
    assert(std::fabs(descriptor.blur_radius - 18.0f) < 0.0001f);
    assert(descriptor.tint.r == 32 && descriptor.tint.g == 64
           && descriptor.tint.b == 96);
    assert(descriptor.tint.a == 144);
    assert(std::fabs(descriptor.saturation - 0.73f) < 0.0001f);
    assert(std::fabs(descriptor.luminance_floor - 0.19f) < 0.0001f);
    assert(std::fabs(descriptor.luminance_gain - 1.31f) < 0.0001f);
    assert(std::fabs(descriptor.edge_highlight - 0.57f) < 0.0001f);
    assert(std::fabs(descriptor.edge_width - 2.25f) < 0.0001f);
    assert(std::fabs(descriptor.noise_opacity - 0.031f) < 0.0001f);
    assert(std::fabs(descriptor.shadow_alpha - 0.22f) < 0.0001f);
    assert(std::fabs(descriptor.shadow_radius - 17.0f) < 0.0001f);

    MaterialEnvironment env{};
    env.capabilities.material_surfaces = true;
    env.capabilities.material_backdrop_blur = true;
    env.capabilities.shader_blur = true;
    env.capabilities.frame_history = true;
    env.backdrop.available = true;
    env.backdrop.stable = true;
    env.render_target.width = 320;
    env.render_target.height = 40;
    auto plan = plan_material_surface(
        material_request_for_command(
            descriptor,
            MaterialGeometry{cmd->x, cmd->y, cmd->w, cmd->h, cmd->radius},
            detail::g_app.theme),
        env);
    assert(!plan.fallback());
    assert(plan.role == MaterialSurfaceRole::Content);
    assert(plan.command_descriptor.role == MaterialSurfaceRole::Content);
    assert(!plan.decision_trace.role_allows_liquid_glass);
    assert(plan.decision_trace.content_layer_standard_material);
    assert(!plan.decision_trace.liquid_glass_backdrop_candidate);
    assert(!plan.decision_trace.can_sample_backdrop);
    assert(!plan.decision_trace.next_frame_capture_required);
    assert(!plan.backdrop_sampling);
    assert(!plan.backdrop_access.shared_frame_capture);
    assert(std::string(plan.plan_id)
           == "material.regular.standard-material");
    assert(std::string(plan.reference_model.technology)
           == "standard-material");
    assert(std::string(plan.reference_model.layer) == "content-layer");
    assert(std::string(plan.reference_model.material_policy)
           == "standard-material-content-layer");
    assert(std::string(plan.reference_model.blending_scope)
           == "standard-fill");
    assert(std::string(plan.primary_pass.name)
           == "standard-material-fill");
    assert(std::string(plan.primary_pass.executor) == "standard-fill");
    assert(std::string(plan.primary_pass.likely_layer)
           == "material-standard-pass");
    assert(plan.container.mode == MaterialContainerMode::Union);
    assert(plan.container.container_id == 88u);
    assert(plan.container.union_id == 12u);
    assert(plan.container.interactive);
    assert(plan.container.morph_transitions);
    assert(plan.blur_radius == 0.0f);
    assert(plan.sample_taps == 0u);
    assert(plan.saturation == 1.0f);
    assert(std::fabs(plan.luminance_floor - 0.19f) < 0.0001f);
    assert(std::fabs(plan.luminance_gain - 1.31f) < 0.0001f);
    assert(std::fabs(plan.edge_highlight - 0.57f) < 0.0001f);
    assert(std::fabs(plan.edge_width - 2.25f) < 0.0001f);
    assert(plan.noise_opacity == 0.0f);
    assert(std::fabs(plan.shadow_alpha - 0.22f) < 0.0001f);
    assert(std::fabs(plan.shadow_radius - 17.0f) < 0.0001f);

    std::puts("PASS: material command preserves style optics");
}

// Regression: when a subtree (here widget::radio's row) keeps blitting
// as a chunk frame after frame, descendants' paint_offset values stay
// frozen at whatever frame last walked them. Pre-fix, the moment a
// sibling-only diff failure forces the row to walk while a leaf inside
// still matches structurally, that leaf's blit guard fires with a
// paint_offset pointing into long-stale prev_cmd_buf bytes and memcpys
// garbage into the live cmd buffer.
//
// Repro mirrors the cad++ view-selector that surfaced this in PR #21:
// a 3-radio group cycled A → B → A → C. C.row blits across frames 1+2;
// in frame 3 C.box's diff fails (decoration None→Dot) cascading C.row
// to walk, but C.label.diff still succeeds because its text is
// unchanged. C.label must NOT take the blit path in that frame.
void test_radio_paint_cache_stale_descendant_after_subtree_blit() {
    using Msg = int;

    auto build_radios = [](bool a, bool b, bool c) {
        auto root_h = detail::alloc_node();
        detail::node_at(root_h).style.flex_direction = FlexDirection::Column;
        Scope scope(root_h);
        Scope::set_current(&scope);
        widget::radio<Msg>("Option A", a, 0);
        widget::radio<Msg>("Option B", b, 1);
        widget::radio<Msg>("Option C", c, 2);
        Scope::set_current(nullptr);
        return root_h;
    };

    auto mirror_cmd_to_prev = []() {
        auto len = CMD_LEN;
        if (len > AppState::PAINT_CACHE_BUF_SIZE) len = AppState::PAINT_CACHE_BUF_SIZE;
        std::memcpy(detail::g_app.prev_cmd_buf, CMD_BUF, len);
        detail::g_app.prev_cmd_len = len;
    };

    auto next_frame = []() {
        std::swap(detail::g_app.arena, detail::g_app.prev_arena);
        detail::g_app.arena.reset();
        detail::g_app.callbacks.clear();
        detail::msg_queue().clear();
    };

    detail::g_app.arena.reset();
    detail::g_app.prev_arena.reset();
    detail::g_app.callbacks.clear();
    detail::msg_queue().clear();
    detail::g_app.paint_invalidation_mask = 0;
    detail::g_app.prev_scroll_x = 0;
    detail::g_app.prev_scroll_y = 0;
    metrics::reset_all();

    // Frame 0: A selected, initial walk seeds the cache.
    auto root_0 = build_radios(true, false, false);
    LAYOUT_NODE(root_0, 400.0f);
    CMD_LEN = 0;
    PAINT_NODE(root_0, 0, 0, 0, 600.0f);
    mirror_cmd_to_prev();
    detail::g_app.prev_root = root_0;

    // Frame 1: click B. A and B flip; C unchanged so C.row blits as a
    // chunk and C's descendant offsets stay at frame-0 positions.
    next_frame();
    auto root_1 = build_radios(false, true, false);
    detail::diff_and_copy_layout(detail::g_app.prev_root, root_1,
                                 detail::g_app.prev_arena, detail::g_app.arena);
    LAYOUT_NODE(root_1, 400.0f);
    CMD_LEN = 0;
    PAINT_NODE(root_1, 0, 0, 0, 600.0f);
    mirror_cmd_to_prev();
    detail::g_app.prev_root = root_1;

    // Frame 2: click A. C.row blits again. prev_cmd_buf is rewritten
    // each frame, so C.label's frame-0 offset now points to whatever
    // bytes happen to occupy that position in frame 2's emit.
    next_frame();
    auto root_2 = build_radios(true, false, false);
    detail::diff_and_copy_layout(detail::g_app.prev_root, root_2,
                                 detail::g_app.prev_arena, detail::g_app.arena);
    LAYOUT_NODE(root_2, 400.0f);
    CMD_LEN = 0;
    PAINT_NODE(root_2, 0, 0, 0, 600.0f);
    mirror_cmd_to_prev();
    detail::g_app.prev_root = root_2;

    // Frame 3: click C. C.box decoration None→Dot fails diff and
    // cascades C.row to walk. C.label.diff still succeeds (text
    // unchanged) — its blit guard would fire with the stale frame-0
    // offset without the descendant invalidation.
    next_frame();
    auto root_3 = build_radios(false, false, true);
    detail::diff_and_copy_layout(detail::g_app.prev_root, root_3,
                                 detail::g_app.prev_arena, detail::g_app.arena);
    LAYOUT_NODE(root_3, 400.0f);

    auto blits_before = metrics::inst::paint_subtrees_blitted.total();
    CMD_LEN = 0;
    PAINT_NODE(root_3, 0, 0, 0, 600.0f);
    auto blits_during = metrics::inst::paint_subtrees_blitted.total() - blits_before;

    // Frame 3 legitimate blits: A.label (text unchanged across f2→f3,
    // offset stayed fresh because A.row was walked every frame and
    // therefore recursed into A.label) and B.row (B fully unchanged
    // f2→f3, so the whole row blits). C.label MUST take the miss-path
    // walk: its inherited paint_offset is from a long-defunct buffer
    // position because C.row chunk-blitted across frames 1+2 without
    // ever recursing into C.label to refresh it. Pre-fix: 3 blits
    // (extra stale C.label blit corrupts the cmd buffer). Post-fix: 2.
    assert(blits_during == 2);

    // Sanity: C.box walked and emitted active visuals.
    auto& column = detail::node_at(root_3);
    assert(column.children.size() == 3);
    auto& c_row = detail::node_at(column.children[2]);
    assert(c_row.children.size() == 2);
    auto& c_box = detail::node_at(c_row.children[0]);
    assert(c_box.decoration == Decoration::Dot);
    assert(!c_box.layout_valid);
    assert(c_box.paint_valid);

    std::puts("PASS: radio paint cache survives stale-descendant blit propagation");
}

void test_row_cross_align_center_default() {
    detail::g_app.arena.reset();
    auto root_h = detail::alloc_node();
    auto& root = detail::node_at(root_h);
    root.style.flex_direction = FlexDirection::Column;

    Scope scope(root_h);
    Scope::set_current(&scope);
    layout::row([&] {
        widget::text("plain");
        widget::code("code");
    });
    Scope::set_current(nullptr);

    assert(root.children.size() == 1);
    auto& row = detail::node_at(root.children[0]);
    assert(row.style.cross_align == CrossAxisAlignment::Center);
    assert(row.children.size() == 2);

    LAYOUT_NODE(root_h, 800.0f);

    auto& tnode = detail::node_at(row.children[0]);
    auto& cnode = detail::node_at(row.children[1]);

    assert(cnode.height > tnode.height + 1.0f);
    assert(tnode.y > 0.5f);

    float t_center = tnode.y + tnode.height / 2.0f;
    float c_center = cnode.y + cnode.height / 2.0f;
    float diff = t_center - c_center;
    if (diff < 0) diff = -diff;
    assert(diff < 0.5f);
    std::puts("PASS: row cross-align center default");
}

void test_theme_json_roundtrip() {
    Theme original{};
    original.accent = {255, 0, 0, 255};

    auto json_str = theme_to_json(original);
    auto parsed = theme_from_json(json_str);
    assert(parsed.has_value());
    assert(parsed->accent.r == 255);
    assert(parsed->accent.g == 0);
    assert(parsed->accent.b == 0);
    assert(parsed->accent.a == 255);
    assert(parsed->background.r == original.background.r);
    assert(parsed->background.g == original.background.g);
    assert(parsed->foreground.r == original.foreground.r);
    assert(parsed->default_font_family == original.default_font_family);
    assert(parsed->body_font_size == original.body_font_size);
    assert(parsed->line_height_ratio == original.line_height_ratio);
    assert(parsed->max_content_width == original.max_content_width);

    std::puts("PASS: theme JSON roundtrip");
}

void test_theme_json_partial_keeps_defaults() {
    Theme defaults{};
    auto parsed = theme_from_json(R"({"accent":{"r":10,"g":186,"b":181,"a":255}})");
    assert(parsed.has_value());

    // Overridden field reflects the JSON.
    assert(parsed->accent.r == 10);
    assert(parsed->accent.g == 186);
    assert(parsed->accent.b == 181);
    assert(parsed->accent.a == 255);

    // Every other field falls back to the C++ default.
    assert(parsed->background.r == defaults.background.r);
    assert(parsed->foreground.r == defaults.foreground.r);
    assert(parsed->muted.r       == defaults.muted.r);
    assert(parsed->border.r      == defaults.border.r);
    assert(parsed->code_bg.r     == defaults.code_bg.r);
    assert(parsed->hero_bg.r     == defaults.hero_bg.r);
    assert(parsed->default_font_family == defaults.default_font_family);
    assert(parsed->body_font_size    == defaults.body_font_size);
    assert(parsed->heading_font_size == defaults.heading_font_size);
    assert(parsed->line_height_ratio == defaults.line_height_ratio);
    assert(parsed->max_content_width == defaults.max_content_width);

    // An empty overlay yields the unmodified Theme defaults.
    auto empty_overlay = theme_from_json("{}");
    assert(empty_overlay.has_value());
    assert(empty_overlay->accent.r == defaults.accent.r);
    assert(empty_overlay->background.r == defaults.background.r);
    assert(empty_overlay->default_font_family == defaults.default_font_family);
    assert(empty_overlay->body_font_size == defaults.body_font_size);

    std::puts("PASS: theme JSON partial keeps defaults");
}

void test_theme_json_color_string_forms() {
    // 6-digit hex (Tiffany).
    {
        auto parsed = theme_from_json(R"({"accent":"#0abab5"})");
        assert(parsed.has_value());
        assert(parsed->accent.r == 0x0a);
        assert(parsed->accent.g == 0xba);
        assert(parsed->accent.b == 0xb5);
        assert(parsed->accent.a == 0xff);
    }
    // 3-digit hex shorthand (#fff -> 0xff 0xff 0xff).
    {
        auto parsed = theme_from_json(R"({"background":"#fff"})");
        assert(parsed.has_value());
        assert(parsed->background.r == 0xff);
        assert(parsed->background.g == 0xff);
        assert(parsed->background.b == 0xff);
        assert(parsed->background.a == 0xff);
    }
    // 4-digit hex shorthand with alpha (#0a0f -> r=0,g=0xaa,b=0,a=0xff).
    {
        auto parsed = theme_from_json(R"({"foreground":"#0a0f"})");
        assert(parsed.has_value());
        assert(parsed->foreground.r == 0x00);
        assert(parsed->foreground.g == 0xaa);
        assert(parsed->foreground.b == 0x00);
        assert(parsed->foreground.a == 0xff);
    }
    // 8-digit hex with alpha.
    {
        auto parsed = theme_from_json(R"({"foreground":"#11223344"})");
        assert(parsed.has_value());
        assert(parsed->foreground.r == 0x11);
        assert(parsed->foreground.g == 0x22);
        assert(parsed->foreground.b == 0x33);
        assert(parsed->foreground.a == 0x44);
    }
    // Uppercase hex digits.
    {
        auto parsed = theme_from_json(R"({"accent":"#ABCDEF"})");
        assert(parsed.has_value());
        assert(parsed->accent.r == 0xab);
        assert(parsed->accent.g == 0xcd);
        assert(parsed->accent.b == 0xef);
    }
    // CSS rgb() function.
    {
        auto parsed = theme_from_json(R"json({"accent":"rgb(10, 186, 181)"})json");
        assert(parsed.has_value());
        assert(parsed->accent.r == 10);
        assert(parsed->accent.g == 186);
        assert(parsed->accent.b == 181);
        assert(parsed->accent.a == 255);
    }
    // CSS rgba() function with fractional alpha.
    {
        auto parsed = theme_from_json(R"json({"accent":"rgba(255, 0, 0, 0.5)"})json");
        assert(parsed.has_value());
        assert(parsed->accent.r == 255);
        assert(parsed->accent.g == 0);
        assert(parsed->accent.b == 0);
        // 0.5 * 255 = 127.5 -> 128 (round-to-nearest).
        assert(parsed->accent.a == 128);
    }
    // "transparent" keyword.
    {
        auto parsed = theme_from_json(R"({"accent":"transparent"})");
        assert(parsed.has_value());
        assert(parsed->accent.r == 0);
        assert(parsed->accent.g == 0);
        assert(parsed->accent.b == 0);
        assert(parsed->accent.a == 0);
    }
    // Invalid color string -> error reports the offending value and field.
    {
        auto parsed = theme_from_json(R"({"accent":"not-a-color"})");
        assert(!parsed.has_value());
        assert(parsed.error().find("accent") != std::string::npos);
        assert(parsed.error().find("not-a-color") != std::string::npos);
    }
    // Out-of-range rgb component -> error.
    {
        auto parsed = theme_from_json(R"json({"accent":"rgb(999, 0, 0)"})json");
        assert(!parsed.has_value());
    }
    // Hex with non-hex digits -> error.
    {
        auto parsed = theme_from_json(R"({"accent":"#zzzzzz"})");
        assert(!parsed.has_value());
    }
    std::puts("PASS: theme JSON color string forms");
}

void test_theme_json_color_object_back_compat() {
    auto parsed = theme_from_json(
        R"({"accent":{"r":10,"g":186,"b":181,"a":255}})");
    assert(parsed.has_value());
    assert(parsed->accent.r == 10);
    assert(parsed->accent.g == 186);
    assert(parsed->accent.b == 181);
    assert(parsed->accent.a == 255);

    // theme_to_json emits the object form, and that output round-trips
    // through theme_from_json — covering JSON files that were written
    // by an older phenotype build before the string forms existed.
    Theme original{};
    original.accent = {12, 34, 56, 78};
    auto reparsed = theme_from_json(theme_to_json(original));
    assert(reparsed.has_value());
    assert(reparsed->accent.r == 12);
    assert(reparsed->accent.g == 34);
    assert(reparsed->accent.b == 56);
    assert(reparsed->accent.a == 78);

    // A Color object with a wrong field type still errors.
    auto bad = theme_from_json(
        R"({"accent":{"r":"oops","g":0,"b":0,"a":0}})");
    assert(!bad.has_value());

    std::puts("PASS: theme JSON Color object back-compat");
}

void test_theme_json_mixed_overlay() {
    Theme defaults{};
    auto parsed = theme_from_json(R"({
        "accent": "#0abab5",
        "background": {"r":244,"g":244,"b":245,"a":255},
        "default_font_family": "Pretendard",
        "body_font_size": 18.0,
        "max_content_width": 960
    })");
    assert(parsed.has_value());

    // String form for accent.
    assert(parsed->accent.r == 0x0a);
    assert(parsed->accent.g == 0xba);
    assert(parsed->accent.b == 0xb5);
    assert(parsed->accent.a == 0xff);
    // Object form for background.
    assert(parsed->background.r == 244);
    assert(parsed->background.g == 244);
    assert(parsed->background.b == 245);
    assert(parsed->background.a == 255);
    // Float overrides.
    assert(parsed->default_font_family == "Pretendard");
    assert(parsed->body_font_size == 18.0f);
    assert(parsed->max_content_width == 960.0f);
    // Untouched fields still hold defaults.
    assert(parsed->foreground.r == defaults.foreground.r);
    assert(parsed->heading_font_size == defaults.heading_font_size);

    std::puts("PASS: theme JSON mixed overlay");
}

namespace button_test {
struct Click {};
using ButtonMsg = std::variant<Click>;

inline NodeHandle build_button(ButtonVariant variant, bool disabled,
                               unsigned int hovered_id = 0xFFFFFFFFu,
                               unsigned int focused_id = 0xFFFFFFFFu,
                               unsigned int pressed_id = 0xFFFFFFFFu,
                               bool focus_visible = false) {
    detail::g_app.arena.reset();
    detail::g_app.callbacks.clear();
    detail::msg_queue().clear();
    // Wipe per-call-site animation state so the first `animate_color` /
    // `animate_float` inside `widget::button` snaps to its target
    // instead of inheriting the previous test's interpolation.
    detail::local_store().clear();
    detail::bump_local_gen();
    detail::g_app.hovered_id = hovered_id;
    detail::g_app.focused_id = focused_id;
    detail::g_app.pressed_id = pressed_id;
    detail::g_app.focus_visible = focus_visible;

    auto root_h = detail::alloc_node();
    detail::node_at(root_h).style.flex_direction = FlexDirection::Column;

    Scope scope(root_h);
    Scope::set_current(&scope);
    widget::button<ButtonMsg>("Click", Click{}, variant, disabled);
    Scope::set_current(nullptr);

    auto& root = detail::node_at(root_h);
    assert(root.children.size() == 1);
    return root.children[0];
}

inline NodeHandle build_button_with_options(
        ButtonStyleOptions options,
        unsigned int hovered_id = 0xFFFFFFFFu,
        unsigned int focused_id = 0xFFFFFFFFu,
        unsigned int pressed_id = 0xFFFFFFFFu,
        bool focus_visible = false) {
    detail::g_app.arena.reset();
    detail::g_app.callbacks.clear();
    detail::msg_queue().clear();
    detail::local_store().clear();
    detail::bump_local_gen();
    detail::g_app.hovered_id = hovered_id;
    detail::g_app.focused_id = focused_id;
    detail::g_app.pressed_id = pressed_id;
    detail::g_app.focus_visible = focus_visible;

    auto root_h = detail::alloc_node();
    detail::node_at(root_h).style.flex_direction = FlexDirection::Column;

    Scope scope(root_h);
    Scope::set_current(&scope);
    widget::button<ButtonMsg>("Click", Click{}, options);
    Scope::set_current(nullptr);

    auto& root = detail::node_at(root_h);
    assert(root.children.size() == 1);
    return root.children[0];
}

inline NodeHandle build_canvas_button_with_options(
        ButtonStyleOptions options,
        std::uint64_t paint_token = 0xCAFEu,
        unsigned int hovered_id = 0xFFFFFFFFu,
        unsigned int focused_id = 0xFFFFFFFFu,
        unsigned int pressed_id = 0xFFFFFFFFu,
        bool focus_visible = false) {
    detail::g_app.arena.reset();
    detail::g_app.callbacks.clear();
    detail::g_app.callback_roles.clear();
    detail::msg_queue().clear();
    detail::local_store().clear();
    detail::bump_local_gen();
    detail::g_app.hovered_id = hovered_id;
    detail::g_app.focused_id = focused_id;
    detail::g_app.pressed_id = pressed_id;
    detail::g_app.focus_visible = focus_visible;

    auto root_h = detail::alloc_node();
    detail::node_at(root_h).style.flex_direction = FlexDirection::Column;

    Scope scope(root_h);
    Scope::set_current(&scope);
    widget::canvas_button<ButtonMsg>(
        "Grid View",
        44.0f,
        36.0f,
        [](Painter& p) {
            p.line(12.0f, 12.0f, 32.0f, 24.0f,
                   2.0f, Color{20, 20, 20, 255});
        },
        Click{},
        options,
        paint_token);
    Scope::set_current(nullptr);

    auto& root = detail::node_at(root_h);
    assert(root.children.size() == 1);
    return root.children[0];
}

inline NodeHandle build_symbol_button_with_options(
        icons::SymbolButtonOptions options,
        unsigned int hovered_id = 0xFFFFFFFFu,
        unsigned int focused_id = 0xFFFFFFFFu,
        unsigned int pressed_id = 0xFFFFFFFFu,
        bool focus_visible = false) {
    detail::g_app.arena.reset();
    detail::g_app.callbacks.clear();
    detail::g_app.callback_roles.clear();
    detail::msg_queue().clear();
    detail::local_store().clear();
    detail::bump_local_gen();
    detail::g_app.hovered_id = hovered_id;
    detail::g_app.focused_id = focused_id;
    detail::g_app.pressed_id = pressed_id;
    detail::g_app.focus_visible = focus_visible;

    auto root_h = detail::alloc_node();
    detail::node_at(root_h).style.flex_direction = FlexDirection::Column;

    Scope scope(root_h);
    Scope::set_current(&scope);
    widget::symbol_button<ButtonMsg>(
        "Grid View",
        icons::Symbol::Grid,
        Click{},
        options);
    Scope::set_current(nullptr);

    auto& root = detail::node_at(root_h);
    assert(root.children.size() == 1);
    return root.children[0];
}
} // namespace button_test

void test_button_default_variant() {
    auto btn_h = button_test::build_button(ButtonVariant::Default, /*disabled=*/false);
    auto& btn = detail::node_at(btn_h);
    auto const& t = detail::g_app.theme;
    assert(btn.background.r == t.surface.r &&
           btn.background.g == t.surface.g &&
           btn.background.b == t.surface.b);
    assert(btn.text_color.r == t.foreground.r);
    // Hover is now driven by view-time animate_color; the static
    // `hover_background` field is no longer set so paint's fallback
    // branch falls through to the animated `background`.
    assert(btn.hover_background.a == 0);
    assert(btn.border_color.r == t.border.r);
    assert(btn.border_width == 1);
    assert(btn.border_radius == t.radius_sm);
    assert(btn.cursor_type == 1);
    assert(btn.focusable == true);
    assert(btn.callback_id != 0xFFFFFFFFu);
    assert(btn.interaction_role == InteractionRole::Button);
    std::puts("PASS: button default variant");
}

void test_button_primary_variant() {
    auto btn_h = button_test::build_button(ButtonVariant::Primary, /*disabled=*/false);
    auto& btn = detail::node_at(btn_h);
    auto const& t = detail::g_app.theme;
    assert(btn.background.r == t.accent.r &&
           btn.background.g == t.accent.g &&
           btn.background.b == t.accent.b);
    assert(btn.text_color.r == t.state_active_fg.r);
    assert(btn.hover_background.a == 0);
    assert(btn.border_color.r == t.accent.r);
    assert(btn.border_width == 1);
    assert(btn.cursor_type == 1);
    assert(btn.focusable == true);
    assert(btn.callback_id != 0xFFFFFFFFu);
    std::puts("PASS: button primary variant");
}

// Hover smoke test: build a button with `g_app.hovered_id` pre-set to
// the id we know the button will land on. animate_color's first call
// snaps to its target (initialised=false), so the resulting node
// background must equal the variant's hover colour exactly. This wires
// up the view-time check without needing to advance time.
void test_button_default_hovered_snaps_to_hover_bg() {
    auto btn_h = button_test::build_button(
        ButtonVariant::Default, /*disabled=*/false, /*hovered_id=*/0u);
    auto& btn = detail::node_at(btn_h);
    auto const& t = detail::g_app.theme;
    assert(btn.callback_id == 0u);
    assert(btn.background.r == t.state_hover_bg.r &&
           btn.background.g == t.state_hover_bg.g &&
           btn.background.b == t.state_hover_bg.b);
    detail::g_app.hovered_id = 0xFFFFFFFFu;
    std::puts("PASS: button default snaps to hover bg when hovered");
}

void test_button_pressed_snaps_to_pressed_bg() {
    ButtonStyleOptions options;
    options.has_background = true;
    options.background = Color{1, 2, 3, 255};
    options.has_hover_background = true;
    options.hover_background = Color{5, 6, 7, 255};
    options.has_pressed_background = true;
    options.pressed_background = Color{9, 10, 11, 255};

    auto btn_h = button_test::build_button_with_options(
        options,
        /*hovered_id=*/0u,
        /*focused_id=*/0xFFFFFFFFu,
        /*pressed_id=*/0u);
    auto& btn = detail::node_at(btn_h);
    assert(btn.callback_id == 0u);
    assert(btn.background.r == 9 && btn.background.g == 10
           && btn.background.b == 11 && btn.background.a == 255);
    detail::g_app.hovered_id = 0xFFFFFFFFu;
    detail::g_app.pressed_id = 0xFFFFFFFFu;
    std::puts("PASS: button snaps to pressed bg when pressed");
}

void test_button_primary_hovered_snaps_to_hover_bg() {
    auto btn_h = button_test::build_button(
        ButtonVariant::Primary, /*disabled=*/false, /*hovered_id=*/0u);
    auto& btn = detail::node_at(btn_h);
    auto const& t = detail::g_app.theme;
    assert(btn.callback_id == 0u);
    assert(btn.background.r == t.accent_strong.r &&
           btn.background.g == t.accent_strong.g &&
           btn.background.b == t.accent_strong.b);
    detail::g_app.hovered_id = 0xFFFFFFFFu;
    std::puts("PASS: button primary snaps to accent_strong when hovered");
}

// Keyboard focus ring smoke test: pre-set `g_app.focused_id` and
// `g_app.focus_visible` to the id the button is about to claim, then
// assert the first frame snaps the animated border_width to
// `state_focus_ring_width`. Pointer focus keeps the id for event routing
// but leaves `focus_visible=false`, so it should not draw this ring.
void test_button_focused_snaps_to_focus_ring_width() {
    auto btn_h = button_test::build_button(
        ButtonVariant::Default, /*disabled=*/false,
        /*hovered_id=*/0xFFFFFFFFu, /*focused_id=*/0u,
        /*pressed_id=*/0xFFFFFFFFu, /*focus_visible=*/true);
    auto& btn = detail::node_at(btn_h);
    auto const& t = detail::g_app.theme;
    assert(btn.callback_id == 0u);
    assert(btn.border_width == t.state_focus_ring_width);
    detail::g_app.focused_id = 0xFFFFFFFFu;
    detail::g_app.focus_visible = false;
    std::puts("PASS: button border_width snaps to focus ring width when focused");
}

void test_button_pointer_focused_hides_focus_ring() {
    auto btn_h = button_test::build_button(
        ButtonVariant::Default, /*disabled=*/false,
        /*hovered_id=*/0xFFFFFFFFu, /*focused_id=*/0u,
        /*pressed_id=*/0xFFFFFFFFu, /*focus_visible=*/false);
    auto& btn = detail::node_at(btn_h);
    auto const& t = detail::g_app.theme;
    assert(btn.callback_id == 0u);
    assert(btn.border_width == 1.0f);
    assert(btn.border_color.r == t.border.r);
    detail::g_app.focused_id = 0xFFFFFFFFu;
    detail::g_app.focus_visible = false;
    std::puts("PASS: pointer-focused button hides focus ring");
}

void test_focus_visible_tracks_keyboard_modality() {
    detail::g_app.focusable_ids.clear();
    detail::g_app.focusable_ids.push_back(7u);
    detail::g_app.focusable_ids.push_back(11u);
    detail::g_app.focused_id = 0xFFFFFFFFu;
    detail::g_app.focus_visible = false;

    bool const tabbed =
        detail::handle_tab(0u, "test", "tab");
    assert(tabbed);
    assert(detail::g_app.focused_id == 7u);
    assert(detail::g_app.focus_visible == true);
    auto debug = diag::input_debug_snapshot();
    assert(debug.focused_id == 7u);
    assert(debug.focus_visible == true);

    bool const pointer_focus =
        detail::set_focus_id(7u, "test", "pointer-focus");
    assert(pointer_focus);
    assert(detail::g_app.focused_id == 7u);
    assert(detail::g_app.focus_visible == false);
    debug = diag::input_debug_snapshot();
    assert(debug.focused_id == 7u);
    assert(debug.focus_visible == false);

    detail::g_app.focusable_ids.clear();
    detail::g_app.focused_id = 0xFFFFFFFFu;
    detail::g_app.focus_visible = false;
    std::puts("PASS: focus-visible tracks keyboard modality");
}

// Defocused (resting) border_width is the variant's natural 1px — not
// the focus-ring upgrade. Pairs with the focused test above so a
// regression that pins the value to either side stands out.
void test_button_defocused_resting_border_width() {
    auto btn_h = button_test::build_button(
        ButtonVariant::Default, /*disabled=*/false);
    auto& btn = detail::node_at(btn_h);
    assert(btn.border_width == 1);
    std::puts("PASS: button defocused border_width snaps to resting 1px");
}

void test_button_disabled() {
    for (auto variant : {ButtonVariant::Default, ButtonVariant::Primary}) {
        auto btn_h = button_test::build_button(variant, /*disabled=*/true);
        auto& btn = detail::node_at(btn_h);
        auto const& t = detail::g_app.theme;
        assert(btn.background.r == t.state_disabled_bg.r);
        assert(btn.text_color.r == t.state_disabled_fg.r);
        assert(btn.border_color.r == t.state_disabled_border.r);
        assert(btn.border_width == 1);
        assert(btn.cursor_type == 0);
        assert(btn.focusable == false);
        assert(btn.callback_id == 0xFFFFFFFFu);
        // hover_background was never assigned, stays transparent so paint
        // skips the hover overlay.
        assert(btn.hover_background.a == 0);
        // Disabled buttons keep the Button role for accessibility.
        assert(btn.interaction_role == InteractionRole::Button);
    }
    std::puts("PASS: button disabled (both variants)");
}

void test_button_disabled_custom_chrome() {
    ButtonStyleOptions options;
    options.disabled = true;
    options.has_background = true;
    options.background = Color{1, 2, 3, 4};
    options.has_border_color = true;
    options.border_color = Color{5, 6, 7, 8};
    options.has_text_color = true;
    options.text_color = Color{9, 10, 11, 12};
    options.border_width = 0.0f;

    auto btn_h = button_test::build_button_with_options(options);
    auto& btn = detail::node_at(btn_h);
    assert(btn.background.r == 1 && btn.background.a == 4);
    assert(btn.border_color.r == 5 && btn.border_color.a == 8);
    assert(btn.text_color.r == 9 && btn.text_color.a == 12);
    assert(btn.border_width == 0.0f);
    assert(btn.cursor_type == 0);
    assert(btn.focusable == false);
    assert(btn.callback_id == 0xFFFFFFFFu);
    assert(btn.debug_semantic_enabled == false);
    assert(detail::g_app.callbacks.empty());

    std::puts("PASS: button disabled custom chrome");
}

void test_button_style_options_custom_chrome() {
    ButtonStyleOptions options;
    options.has_background = true;
    options.background = Color{1, 2, 3, 4};
    options.has_hover_background = true;
    options.hover_background = Color{5, 6, 7, 8};
    options.has_pressed_background = true;
    options.pressed_background = Color{17, 18, 19, 20};
    options.has_border_color = true;
    options.border_color = Color{9, 10, 11, 12};
    options.has_text_color = true;
    options.text_color = Color{13, 14, 15, 16};
    options.border_width = 0.0f;
    options.border_radius = 11.0f;
    options.font_size = 17.0f;
    options.padding_top = 1.0f;
    options.padding_right = 2.0f;
    options.padding_bottom = 3.0f;
    options.padding_left = 4.0f;
    options.max_width = 120.0f;
    options.fixed_height = 32.0f;
    options.text_align = TextAlign::Center;

    auto btn_h = button_test::build_button_with_options(options);
    auto& btn = detail::node_at(btn_h);
    assert(btn.background.r == 1 && btn.background.g == 2
           && btn.background.b == 3 && btn.background.a == 4);
    assert(btn.text_color.r == 13 && btn.text_color.a == 16);
    assert(btn.border_color.r == 9 && btn.border_color.a == 12);
    assert(btn.border_width == 0.0f);
    assert(btn.border_radius == 11.0f);
    assert(btn.font_size == 17.0f);
    assert(btn.style.padding[0] == 1.0f);
    assert(btn.style.padding[1] == 2.0f);
    assert(btn.style.padding[2] == 3.0f);
    assert(btn.style.padding[3] == 4.0f);
    assert(btn.style.max_width == 120.0f);
    assert(btn.style.fixed_height == 32.0f);
    assert(btn.min_hit_width == minimum_button_activation_size);
    assert(btn.min_hit_height == minimum_button_activation_size);
    assert(btn.style.text_align == TextAlign::Center);
    assert(btn.callback_id != 0xFFFFFFFFu);

    auto hovered_h = button_test::build_button_with_options(
        options, /*hovered_id=*/0u);
    auto& hovered = detail::node_at(hovered_h);
    assert(hovered.background.r == 5 && hovered.background.g == 6
           && hovered.background.b == 7 && hovered.background.a == 8);

    auto pressed_h = button_test::build_button_with_options(
        options,
        /*hovered_id=*/0u,
        /*focused_id=*/0xFFFFFFFFu,
        /*pressed_id=*/0u);
    auto& pressed = detail::node_at(pressed_h);
    assert(pressed.background.r == 17 && pressed.background.g == 18
           && pressed.background.b == 19 && pressed.background.a == 20);

    std::puts("PASS: button style options custom chrome");
}

void test_canvas_button_semantic_and_layout_contract() {
    ButtonStyleOptions options;
    options.has_background = true;
    options.background = Color{255, 255, 255, 0};
    options.has_hover_background = true;
    options.hover_background = Color{230, 230, 230, 255};
    options.has_border_color = true;
    options.border_color = Color{0, 0, 0, 0};
    options.border_width = 0.0f;
    options.border_radius = 18.0f;
    options.max_width = 44.0f;
    options.fixed_height = 36.0f;

    auto btn_h = button_test::build_canvas_button_with_options(
        options, 0xF00Du);
    LAYOUT_NODE(btn_h, 100.0f);

    auto& btn = detail::node_at(btn_h);
    assert(btn.width == 44.0f);
    assert(btn.height == 36.0f);
    assert(btn.min_hit_width == minimum_button_activation_size);
    assert(btn.min_hit_height == minimum_button_activation_size);
    assert(btn.interaction_role == InteractionRole::Button);
    assert(btn.debug_semantic_label == "Grid View");
    assert(btn.callback_id == 0u);
    assert(btn.focusable == true);
    assert(btn.cursor_type == 1);
    assert(detail::g_app.callbacks.size() == 1);
    assert(detail::g_app.callback_roles.size() == 1);
    assert(detail::g_app.callback_roles[0] == InteractionRole::Button);
    assert(btn.children.size() == 1);

    auto& canvas = detail::node_at(btn.children[0]);
    assert(canvas.width == 44.0f);
    assert(canvas.height == 36.0f);
    assert(canvas.paint_fn);
    assert(canvas.paint_token == 0xF00Du);
    assert(canvas.debug_semantic_hidden == true);

    std::puts("PASS: canvas_button semantic + layout contract");
}

void test_canvas_button_minimum_hit_region_contract() {
    ButtonStyleOptions options;
    options.max_width = 36.0f;
    options.fixed_height = 36.0f;
    options.has_background = true;
    options.background = Color{255, 255, 255, 0};
    options.has_border_color = true;
    options.border_color = Color{0, 0, 0, 0};
    options.border_width = 0.0f;

    auto btn_h = button_test::build_canvas_button_with_options(
        options, 0xBEEFu);
    LAYOUT_NODE(btn_h, 100.0f);
    auto const& btn = detail::node_at(btn_h);
    float const expected_slop_x =
        (minimum_button_activation_size - btn.width) * 0.5f;
    float const expected_slop_y =
        (minimum_button_activation_size - btn.height) * 0.5f;
    CMD_LEN = 0;
    PAINT_NODE(btn_h, 0, 0, 0, 100.0f);

    bool found = false;
    for (unsigned int i = 0; i + 28 <= CMD_LEN; i += 4) {
        unsigned int op;
        std::memcpy(&op, &CMD_BUF[i], 4);
        if (op != static_cast<unsigned int>(Cmd::HitRegion))
            continue;
        float hx = 0.0f;
        float hy = 0.0f;
        float hw = 0.0f;
        float hh = 0.0f;
        std::memcpy(&hx, &CMD_BUF[i + 4], 4);
        std::memcpy(&hy, &CMD_BUF[i + 8], 4);
        std::memcpy(&hw, &CMD_BUF[i + 12], 4);
        std::memcpy(&hh, &CMD_BUF[i + 16], 4);
        assert(hx == btn.x - expected_slop_x);
        assert(hy == btn.y - expected_slop_y);
        assert(hw == minimum_button_activation_size);
        assert(hh == minimum_button_activation_size);
        found = true;
        break;
    }
    assert(found);
    std::puts("PASS: canvas_button minimum hit region contract");
}

void test_canvas_button_focus_visible_contract() {
    ButtonStyleOptions options;
    options.max_width = 44.0f;
    options.fixed_height = 36.0f;

    auto keyboard_h = button_test::build_canvas_button_with_options(
        options,
        0xCAFEu,
        /*hovered_id=*/0xFFFFFFFFu,
        /*focused_id=*/0u,
        /*pressed_id=*/0xFFFFFFFFu,
        /*focus_visible=*/true);
    auto& keyboard = detail::node_at(keyboard_h);
    auto const& t = detail::g_app.theme;
    assert(keyboard.callback_id == 0u);
    assert(keyboard.border_width == t.state_focus_ring_width);
    assert(keyboard.border_color.r == t.state_focus_ring.r);

    auto pointer_h = button_test::build_canvas_button_with_options(
        options,
        0xCAFEu,
        /*hovered_id=*/0xFFFFFFFFu,
        /*focused_id=*/0u,
        /*pressed_id=*/0xFFFFFFFFu,
        /*focus_visible=*/false);
    auto& pointer = detail::node_at(pointer_h);
    assert(pointer.callback_id == 0u);
    assert(pointer.border_width == 1.0f);
    assert(pointer.border_color.r == t.border.r);
    detail::g_app.focused_id = 0xFFFFFFFFu;
    detail::g_app.focus_visible = false;

    std::puts("PASS: canvas_button focus-visible contract");
}

void test_canvas_button_visual_state_contract() {
    ButtonStyleOptions options;
    options.max_width = 44.0f;
    options.fixed_height = 36.0f;
    options.has_pressed_background = true;
    options.pressed_background = Color{9, 10, 11, 255};

    detail::g_app.arena.reset();
    detail::g_app.callbacks.clear();
    detail::g_app.callback_roles.clear();
    detail::msg_queue().clear();
    detail::local_store().clear();
    detail::bump_local_gen();
    detail::g_app.hovered_id = 0u;
    detail::g_app.focused_id = 0u;
    detail::g_app.pressed_id = 0u;

    auto root_h = detail::alloc_node();
    detail::node_at(root_h).style.flex_direction = FlexDirection::Column;
    bool observed_pressed = false;
    bool observed_hovered = false;
    Scope scope(root_h);
    Scope::set_current(&scope);
    widget::canvas_button<button_test::ButtonMsg>(
        "Stateful",
        44.0f,
        36.0f,
        [&observed_pressed, &observed_hovered](
                Painter& p,
                ButtonVisualState state) {
            observed_pressed = state.pressed;
            observed_hovered = state.hovered;
            p.line(2.0f, 2.0f, state.pressed ? 24.0f : 12.0f, 12.0f,
                   1.0f, Color{20, 20, 20, 255});
        },
        button_test::Click{},
        options,
        0xCAFEu);
    Scope::set_current(nullptr);

    auto& root = detail::node_at(root_h);
    assert(root.children.size() == 1);
    auto& btn = detail::node_at(root.children[0]);
    assert(btn.callback_id == 0u);
    assert(btn.children.size() == 1);
    auto& canvas = detail::node_at(btn.children[0]);
    assert(canvas.paint_token != 0xCAFEu);

    LAYOUT_NODE(root_h, 100.0f);
    CMD_LEN = 0;
    PAINT_NODE(root_h, 0, 0, 0, 100.0f);
    assert(observed_pressed);
    assert(observed_hovered);
    detail::g_app.hovered_id = 0xFFFFFFFFu;
    detail::g_app.focused_id = 0xFFFFFFFFu;
    detail::g_app.pressed_id = 0xFFFFFFFFu;

    std::puts("PASS: canvas_button visual state contract");
}

void test_canvas_button_disabled_contract() {
    ButtonStyleOptions options;
    options.disabled = true;
    options.max_width = 44.0f;
    options.fixed_height = 36.0f;

    auto btn_h = button_test::build_canvas_button_with_options(options);
    auto& btn = detail::node_at(btn_h);
    assert(btn.interaction_role == InteractionRole::Button);
    assert(btn.debug_semantic_label == "Grid View");
    assert(btn.debug_semantic_enabled == false);
    assert(btn.callback_id == 0xFFFFFFFFu);
    assert(btn.focusable == false);
    assert(detail::g_app.callbacks.empty());
    assert(btn.children.size() == 1);
    assert(detail::node_at(btn.children[0]).paint_fn);

    std::puts("PASS: canvas_button disabled contract");
}

void test_canvas_button_disabled_custom_chrome() {
    ButtonStyleOptions options;
    options.disabled = true;
    options.has_background = true;
    options.background = Color{1, 2, 3, 0};
    options.has_border_color = true;
    options.border_color = Color{4, 5, 6, 0};
    options.border_width = 0.0f;
    options.max_width = 44.0f;
    options.fixed_height = 36.0f;

    auto btn_h = button_test::build_canvas_button_with_options(options);
    auto& btn = detail::node_at(btn_h);
    assert(btn.background.r == 1 && btn.background.a == 0);
    assert(btn.border_color.r == 4 && btn.border_color.a == 0);
    assert(btn.border_width == 0.0f);
    assert(btn.debug_semantic_enabled == false);
    assert(btn.callback_id == 0xFFFFFFFFu);
    assert(btn.focusable == false);
    assert(detail::g_app.callbacks.empty());
    assert(btn.children.size() == 1);

    std::puts("PASS: canvas_button disabled custom chrome");
}

void test_symbol_button_macos_contract() {
    icons::SymbolButtonOptions options;
    options.role = icons::SymbolPresentationRole::Toolbar;
    options.selected = true;
    options.token_salt = 0xA11CEu;

    auto btn_h = button_test::build_symbol_button_with_options(options);
    LAYOUT_NODE(btn_h, 100.0f);

    auto& btn = detail::node_at(btn_h);
    assert(btn.width == 36.0f);
    assert(btn.height == 36.0f);
    assert(btn.min_hit_width == minimum_button_activation_size);
    assert(btn.min_hit_height == minimum_button_activation_size);
    assert(btn.interaction_role == InteractionRole::Button);
    assert(btn.debug_semantic_label == "Grid View");
    assert(btn.callback_id == 0u);
    assert(btn.background.a == 150);
    assert(btn.border_radius == 15.0f);
    assert(btn.children.size() == 1);

    auto& canvas = detail::node_at(btn.children[0]);
    assert(canvas.width == 36.0f);
    assert(canvas.height == 36.0f);
    assert(canvas.debug_semantic_hidden == true);
    assert(canvas.paint_token
           == icons::symbol_button_paint_token(icons::Symbol::Grid, options));

    std::puts("PASS: symbol_button macOS chrome contract");
}

void test_symbol_button_minimum_hit_region_contract() {
    icons::SymbolButtonOptions options;
    options.role = icons::SymbolPresentationRole::Toolbar;

    auto btn_h = button_test::build_symbol_button_with_options(options);
    LAYOUT_NODE(btn_h, 100.0f);
    auto const& btn = detail::node_at(btn_h);
    float const expected_slop_x =
        (icons::activation_hit_target_size(options.role) - btn.width) * 0.5f;
    float const expected_slop_y =
        (icons::activation_hit_target_size(options.role) - btn.height) * 0.5f;
    CMD_LEN = 0;
    PAINT_NODE(btn_h, 0, 0, 0, 100.0f);

    bool found = false;
    for (unsigned int i = 0; i + 28 <= CMD_LEN; i += 4) {
        unsigned int op;
        std::memcpy(&op, &CMD_BUF[i], 4);
        if (op != static_cast<unsigned int>(Cmd::HitRegion))
            continue;
        float hx = 0.0f;
        float hy = 0.0f;
        float hw = 0.0f;
        float hh = 0.0f;
        std::memcpy(&hx, &CMD_BUF[i + 4], 4);
        std::memcpy(&hy, &CMD_BUF[i + 8], 4);
        std::memcpy(&hw, &CMD_BUF[i + 12], 4);
        std::memcpy(&hh, &CMD_BUF[i + 16], 4);
        assert(hx == btn.x - expected_slop_x);
        assert(hy == btn.y - expected_slop_y);
        assert(hw == icons::activation_hit_target_size(options.role));
        assert(hh == icons::activation_hit_target_size(options.role));
        found = true;
        break;
    }
    assert(found);
    std::puts("PASS: symbol_button minimum hit region contract");
}

void test_symbol_button_visual_state_token_contract() {
    icons::SymbolButtonOptions options;
    options.role = icons::SymbolPresentationRole::Toolbar;
    options.token_salt = 0x51A7Eu;

    auto normal_h = button_test::build_symbol_button_with_options(options);
    auto const normal_token =
        detail::node_at(detail::node_at(normal_h).children[0]).paint_token;

    auto pressed_h = button_test::build_symbol_button_with_options(
        options,
        /*hovered_id=*/0u,
        /*focused_id=*/0xFFFFFFFFu,
        /*pressed_id=*/0u);
    auto& pressed_btn = detail::node_at(pressed_h);
    auto const pressed_token =
        detail::node_at(pressed_btn.children[0]).paint_token;
    assert(pressed_btn.background.a == 150);
    assert(pressed_token != normal_token);

    detail::g_app.hovered_id = 0xFFFFFFFFu;
    detail::g_app.pressed_id = 0xFFFFFFFFu;
    std::puts("PASS: symbol_button visual-state paint token contract");
}

void test_symbol_button_disabled_contract() {
    icons::SymbolButtonOptions options;
    options.role = icons::SymbolPresentationRole::Navigation;
    options.disabled = true;

    auto btn_h = button_test::build_symbol_button_with_options(options);
    auto& btn = detail::node_at(btn_h);
    assert(btn.style.max_width == 36.0f);
    assert(btn.style.fixed_height == 36.0f);
    assert(btn.interaction_role == InteractionRole::Button);
    assert(btn.debug_semantic_label == "Grid View");
    assert(btn.debug_semantic_enabled == false);
    assert(btn.callback_id == 0xFFFFFFFFu);
    assert(btn.focusable == false);
    assert(detail::g_app.callbacks.empty());
    assert(btn.children.size() == 1);

    auto& canvas = detail::node_at(btn.children[0]);
    assert(canvas.paint_token != 0);
    std::puts("PASS: symbol_button disabled contract");
}

namespace text_field_test {
struct Changed { std::string text; };
using TfMsg = std::variant<Changed>;
inline TfMsg make_changed(std::string s) { return Changed{std::move(s)}; }

inline NodeHandle build_text_field(std::string const& current,
                                   bool error, bool disabled) {
    detail::g_app.arena.reset();
    detail::g_app.callbacks.clear();
    detail::g_app.input_handlers.clear();
    detail::g_app.input_nodes.clear();
    detail::msg_queue().clear();
    // Wipe per-call-site animation state so the first `animate_float`
    // / `animate_color` inside `widget::text_field` snaps to its
    // target instead of inheriting the previous test's interpolation
    // (default → error swap would otherwise return mid-fade colours).
    detail::local_store().clear();
    detail::bump_local_gen();
    detail::g_app.focused_id = 0xFFFFFFFFu;

    auto root_h = detail::alloc_node();
    detail::node_at(root_h).style.flex_direction = FlexDirection::Column;

    Scope scope(root_h);
    Scope::set_current(&scope);
    widget::text_field<TfMsg>("Name", current, &make_changed, error, disabled);
    Scope::set_current(nullptr);

    auto& root = detail::node_at(root_h);
    assert(root.children.size() == 1);
    return root.children[0];
}
} // namespace text_field_test

void test_text_field_default() {
    auto h = text_field_test::build_text_field("hello", /*error=*/false, /*disabled=*/false);
    auto& f = detail::node_at(h);
    auto const& t = detail::g_app.theme;
    assert(f.background.r == t.surface.r);
    assert(f.text_color.r == t.foreground.r); // current non-empty -> foreground
    assert(f.border_color.r == t.border.r);
    assert(f.border_width == 1);
    assert(f.border_radius == t.radius_sm);
    assert(f.cursor_type == 1);
    assert(f.focusable == true);
    assert(f.is_input == true);
    assert(f.callback_id != 0xFFFFFFFFu);
    assert(f.interaction_role == InteractionRole::TextField);
    assert(detail::g_app.input_handlers.size() == 1);
    std::puts("PASS: text_field default state");
}

void test_text_field_default_placeholder() {
    auto h = text_field_test::build_text_field("", /*error=*/false, /*disabled=*/false);
    auto& f = detail::node_at(h);
    auto const& t = detail::g_app.theme;
    assert(f.text_color.r == t.muted.r); // empty -> muted placeholder color
    assert(f.text == f.placeholder);
    std::puts("PASS: text_field default placeholder color");
}

void test_text_field_error() {
    auto h = text_field_test::build_text_field("oops", /*error=*/true, /*disabled=*/false);
    auto& f = detail::node_at(h);
    auto const& t = detail::g_app.theme;
    assert(f.background.r == t.state_error_bg.r);
    assert(f.text_color.r == t.state_error_fg.r);
    assert(f.border_color.r == t.state_error_border.r);
    assert(f.is_input == true);     // error stays interactive
    assert(f.focusable == true);
    assert(f.cursor_type == 1);
    assert(f.callback_id != 0xFFFFFFFFu);
    std::puts("PASS: text_field error state");
}

void test_text_field_disabled() {
    auto h = text_field_test::build_text_field("anything", /*error=*/false, /*disabled=*/true);
    auto& f = detail::node_at(h);
    auto const& t = detail::g_app.theme;
    assert(f.background.r == t.state_disabled_bg.r);
    assert(f.text_color.r == t.state_disabled_fg.r);
    assert(f.border_color.r == t.state_disabled_border.r);
    assert(f.is_input == false);
    assert(f.focusable == false);
    assert(f.cursor_type == 0);
    assert(f.callback_id == 0xFFFFFFFFu);
    // Disabled fields skip the input-handler registration entirely.
    assert(detail::g_app.input_handlers.empty());
    std::puts("PASS: text_field disabled state");
}

namespace text_variant_test {
inline NodeHandle build_text(str content, TextSize size, TextColor color) {
    detail::g_app.arena.reset();
    detail::g_app.callbacks.clear();

    auto root_h = detail::alloc_node();
    detail::node_at(root_h).style.flex_direction = FlexDirection::Column;

    Scope scope(root_h);
    Scope::set_current(&scope);
    widget::text(content, size, color);
    Scope::set_current(nullptr);

    auto& root = detail::node_at(root_h);
    assert(root.children.size() == 1);
    return root.children[0];
}
} // namespace text_variant_test

void test_text_size_variants() {
    auto const& t = detail::g_app.theme;
    auto& body = detail::node_at(text_variant_test::build_text(
        "body", TextSize::Body, TextColor::Default));
    assert(body.font_size == t.body_font_size);
    assert(body.mono == false);

    auto& heading = detail::node_at(text_variant_test::build_text(
        "heading", TextSize::Heading, TextColor::Default));
    assert(heading.font_size == t.heading_font_size);

    auto& small = detail::node_at(text_variant_test::build_text(
        "small", TextSize::Small, TextColor::Default));
    assert(small.font_size == t.small_font_size);

    auto& hero_title = detail::node_at(text_variant_test::build_text(
        "title", TextSize::HeroTitle, TextColor::Default));
    assert(hero_title.font_size == t.hero_title_size);

    auto& hero_sub = detail::node_at(text_variant_test::build_text(
        "subtitle", TextSize::HeroSubtitle, TextColor::Default));
    assert(hero_sub.font_size == t.hero_subtitle_size);

    std::puts("PASS: text size variants pick the right typography token");
}

void test_text_color_variants() {
    auto const& t = detail::g_app.theme;
    auto& def = detail::node_at(text_variant_test::build_text(
        "x", TextSize::Body, TextColor::Default));
    assert(def.text_color.r == t.foreground.r);

    auto& muted = detail::node_at(text_variant_test::build_text(
        "x", TextSize::Body, TextColor::Muted));
    assert(muted.text_color.r == t.muted.r);

    auto& accent = detail::node_at(text_variant_test::build_text(
        "x", TextSize::Body, TextColor::Accent));
    assert(accent.text_color.r == t.accent.r);

    auto& hero_fg = detail::node_at(text_variant_test::build_text(
        "x", TextSize::Body, TextColor::HeroFg));
    assert(hero_fg.text_color.r == t.hero_fg.r);

    auto& hero_muted = detail::node_at(text_variant_test::build_text(
        "x", TextSize::Body, TextColor::HeroMuted));
    assert(hero_muted.text_color.r == t.hero_muted.r);

    std::puts("PASS: text color variants pick the right color token");
}

void test_text_inline_code_chrome() {
    auto const& t = detail::g_app.theme;
    auto& code = detail::node_at(text_variant_test::build_text(
        "fn()", TextSize::Code, TextColor::Default));
    assert(code.font_size == t.code_font_size);
    assert(code.mono == true);
    assert(code.background.r == t.code_bg.r);
    assert(code.border_color.r == t.border.r);
    assert(code.border_width == 1);
    assert(code.border_radius == t.radius_md);
    assert(code.style.padding[0] == t.space_xs);
    assert(code.style.padding[1] == t.space_sm);
    assert(code.style.padding[2] == t.space_xs);
    assert(code.style.padding[3] == t.space_sm);
    std::puts("PASS: text(Code) inline chrome (mono + border + xs/sm padding)");
}

void test_space_value_resolves_each_token() {
    auto const& t = detail::g_app.theme;
    assert(layout::space_value(SpaceToken::Xs)  == t.space_xs);
    assert(layout::space_value(SpaceToken::Sm)  == t.space_sm);
    assert(layout::space_value(SpaceToken::Md)  == t.space_md);
    assert(layout::space_value(SpaceToken::Lg)  == t.space_lg);
    assert(layout::space_value(SpaceToken::Xl)  == t.space_xl);
    assert(layout::space_value(SpaceToken::Xl2) == t.space_2xl);
    std::puts("PASS: space_value resolves every SpaceToken rung");
}

void test_column_props_default_and_override() {
    auto const& t = detail::g_app.theme;

    // Default builder overload — gap=Md, cross=Start, main=Start.
    {
        detail::g_app.arena.reset();
        auto root_h = detail::alloc_node();
        Scope scope(root_h);
        Scope::set_current(&scope);
        layout::column([&]{});
        Scope::set_current(nullptr);

        auto& root = detail::node_at(root_h);
        assert(root.children.size() == 1);
        auto& col = detail::node_at(root.children[0]);
        assert(col.style.flex_direction == FlexDirection::Column);
        assert(col.style.gap == t.space_md);
        assert(col.style.cross_align == CrossAxisAlignment::Start);
        assert(col.style.main_align == MainAxisAlignment::Start);
    }
    // Explicit overrides.
    {
        detail::g_app.arena.reset();
        auto root_h = detail::alloc_node();
        Scope scope(root_h);
        Scope::set_current(&scope);
        layout::column([&]{}, SpaceToken::Lg, CrossAxisAlignment::Center,
                       MainAxisAlignment::SpaceBetween);
        Scope::set_current(nullptr);

        auto& root = detail::node_at(root_h);
        auto& col = detail::node_at(root.children[0]);
        assert(col.style.gap == t.space_lg);
        assert(col.style.cross_align == CrossAxisAlignment::Center);
        assert(col.style.main_align == MainAxisAlignment::SpaceBetween);
    }
    std::puts("PASS: column props (default + override)");
}

void test_row_props_default_and_override() {
    auto const& t = detail::g_app.theme;

    // Default builder overload — gap=Sm, cross=Center, main=Start.
    {
        detail::g_app.arena.reset();
        auto root_h = detail::alloc_node();
        Scope scope(root_h);
        Scope::set_current(&scope);
        layout::row([&]{});
        Scope::set_current(nullptr);

        auto& root = detail::node_at(root_h);
        auto& r = detail::node_at(root.children[0]);
        assert(r.style.flex_direction == FlexDirection::Row);
        assert(r.style.gap == t.space_sm);
        assert(r.style.cross_align == CrossAxisAlignment::Center);
        assert(r.style.main_align == MainAxisAlignment::Start);
    }
    // Explicit overrides.
    {
        detail::g_app.arena.reset();
        auto root_h = detail::alloc_node();
        Scope scope(root_h);
        Scope::set_current(&scope);
        layout::row([&]{}, SpaceToken::Xl, CrossAxisAlignment::Stretch,
                    MainAxisAlignment::End);
        Scope::set_current(nullptr);

        auto& root = detail::node_at(root_h);
        auto& r = detail::node_at(root.children[0]);
        assert(r.style.gap == t.space_xl);
        assert(r.style.cross_align == CrossAxisAlignment::Stretch);
        assert(r.style.main_align == MainAxisAlignment::End);
    }
    std::puts("PASS: row props (default + override)");
}

// ============================================================
// FontSpec wire-format round-trip
// ============================================================

void test_draw_text_roundtrip_with_fontspec() {
    // null_host carries a 64 KB cmd buffer — even one instance on the
    // stack overflows the default 64 KB wasm32-wasi stack. Allocate on
    // the heap so the same test runs on both native and wasm targets.
    auto host_owned = std::make_unique<null_host>();
    auto& h = *host_owned;
    h.flush();

    // Three calls: bare default, mono+bold+italic with named family,
    // and an empty-family Bold-only run. The new `rotation` slot
    // (radians, CCW about pivot `(x, y)`) goes between `font_size`
    // and `flags` — pass 0.0f everywhere here to keep the FontSpec
    // assertions below unchanged; rotation round-trip is exercised
    // by the dedicated test further down.
    emit_draw_text(h, 12.5f, 24.0f, 18.0f, /*rotation=*/0.0f,
                   /*width_factor=*/1.0f,
                   /*flags=*/0u,
                   Color{10, 20, 30, 255},
                   std::string_view{}, "Hi", 2u);
    emit_draw_text(h, 100.0f, 200.0f, 24.0f, /*rotation=*/0.0f,
                   /*width_factor=*/1.0f,
                   /*flags=*/0b111u,                        // mono+bold+italic
                   Color{200, 100, 50, 128},
                   std::string_view{"Arial Black"},
                   "World", 5u);
    emit_draw_text(h, 0.0f, 0.0f, 16.0f, /*rotation=*/0.0f,
                   /*width_factor=*/1.0f,
                   /*flags=*/0b010u,                        // bold only
                   Color{255, 255, 255, 255},
                   std::string_view{},
                   "Bold", 4u);

    auto cmds = parse_commands(h.buf(), h.buf_len());
    assert(cmds.size() == 3);

    auto const* a = std::get_if<DrawTextCmd>(&cmds[0]);
    assert(a);
    assert(a->x == 12.5f && a->y == 24.0f && a->font_size == 18.0f);
    assert(!a->mono);
    assert(a->weight == FontWeight::Regular);
    assert(a->style  == FontStyle::Upright);
    assert(a->family.empty());
    assert(a->text == "Hi");
    assert(a->color.r == 10 && a->color.g == 20 && a->color.b == 30 && a->color.a == 255);

    auto const* b = std::get_if<DrawTextCmd>(&cmds[1]);
    assert(b);
    assert(b->x == 100.0f && b->y == 200.0f && b->font_size == 24.0f);
    assert(b->mono);
    assert(b->weight == FontWeight::Bold);
    assert(b->style  == FontStyle::Italic);
    assert(b->family == "Arial Black");
    assert(b->text == "World");
    assert(b->color.a == 128);

    auto const* c = std::get_if<DrawTextCmd>(&cmds[2]);
    assert(c);
    assert(!c->mono);
    assert(c->weight == FontWeight::Bold);
    assert(c->style  == FontStyle::Upright);
    assert(c->family.empty());
    assert(c->text == "Bold");

    // Cache key separates by FontSpec — same text + size at Regular vs
    // Bold must miss each other.
    detail::clear_measure_cache();
    auto base = metrics::inst::measure_text_calls.total();
    FontSpec const reg{};
    FontSpec const bold{ {}, FontWeight::Bold, FontStyle::Upright, false };
    detail::measure_text_cached(h, 16.0f, reg,  "abc", 3u);
    detail::measure_text_cached(h, 16.0f, bold, "abc", 3u);
    detail::measure_text_cached(h, 16.0f, reg,  "abc", 3u); // cache hit
    auto delta = metrics::inst::measure_text_calls.total() - base;
    assert(delta == 2);  // bold + first reg miss; second reg is a hit

    std::puts("PASS: DrawText round-trips with FontSpec");
}

void test_widget_text_uses_theme_default_font_family() {
    set_theme(Theme{});
    detail::g_app.arena.reset();

    auto root_h = detail::alloc_node();
    detail::node_at(root_h).style.flex_direction = FlexDirection::Column;

    Scope scope(root_h);
    Scope::set_current(&scope);
    widget::text("Pretendard default");
    Scope::set_current(nullptr);

    LAYOUT_NODE(root_h, 400.0f);
    CMD_LEN = 0;
    PAINT_NODE(root_h, 0, 0, 0, 600.0f);

    auto cmds = parse_commands(CMD_BUF, CMD_LEN);
    bool found = false;
    for (auto const& cmd : cmds) {
        if (auto const* text = std::get_if<DrawTextCmd>(&cmd)) {
            if (text->text == "Pretendard default") {
                found = true;
                assert(!text->mono);
                assert(text->family == "Pretendard");
            }
        }
    }
    assert(found);
    std::puts("PASS: widget text uses Theme::default_font_family");
}

ResourceCatalog make_test_resource_catalog() {
    ResourceCatalog catalog;
    catalog.application = {
        .id = "com.misut.phenotype.examples.file-explorer-desktop",
        .display_name = "Phenotype File Explorer",
        .version = "0.1.0",
        .entry = "file_explorer_desktop",
        .platforms = {"macos", "windows"},
    };
    catalog.default_locale = "en";
    catalog.default_font_family = "Pretendard";
    catalog.assets = {
        {
            .name = "app.icon",
            .source = "assets/file-explorer-icon.svg",
            .content_type = "image/svg+xml",
            .preload = true,
            .runtime_visible = false,
        },
    };
    catalog.locales = {
        {
            .tag = "en",
            .source = "locales/en.toml",
            .strings = {
                {.key = "window.title", .value = "Recents"},
                {.key = "sidebar.recents", .value = "Recents"},
                {.key = "action.delete", .value = "Delete"},
            },
        },
        {
            .tag = "ko",
            .source = "locales/ko.toml",
            .fallback = {"en"},
            .strings = {
                {.key = "window.title", .value = "최근 항목"},
                {.key = "sidebar.recents", .value = "최근 항목"},
            },
        },
    };
    catalog.fonts = {
        {
            .family = "Pretendard",
            .source = "fonts/pretendard.alias.toml",
            .register_font = false,
            .fallback = {"system-ui", "Apple SD Gothic Neo", "Segoe UI", "Noto Sans CJK"},
        },
    };
    catalog.debug = {
        .artifact_manifest = "artifact_manifest.json",
        .probe_scene = "finder-style-startup",
        .verifier = "phenotype artifact verify-file-explorer",
    };
    return catalog;
}

void test_resource_catalog_lookup_and_locale_fallback() {
    auto catalog = make_test_resource_catalog();

    auto required = std::array<std::string_view, 3>{
        "window.title",
        "sidebar.recents",
        "action.delete",
    };
    auto diagnostics = validate_resource_catalog(catalog, required);
    assert(diagnostics.empty());
    assert(resource_catalog_ok(catalog));

    auto asset = find_asset(catalog, "app.icon");
    assert(asset);
    assert(asset->get().source == "assets/file-explorer-icon.svg");
    assert(resource_asset_declares_svg(asset->get()));

    auto contract = resource_catalog_contract(catalog, required);
    assert(contract.svg_asset_count == 1);
    assert(contract.preload_svg_asset_count == 1);
    assert(contract.runtime_visible_svg_asset_count == 0);
    assert(svg_asset_contract_policy()
           == "package_svg_assets_must_declare_image_svg_xml_and_svg_source_suffix");

    auto font = find_font(catalog, "Pretendard");
    assert(font);
    assert(font->get().fallback.size() == 4);

    auto ko_delete = localized_string(catalog, "action.delete", "ko");
    assert(ko_delete);
    assert(ko_delete->tag == "en");
    assert(ko_delete->value == "Delete");
    assert(ko_delete->fallback);

    auto chain = locale_fallback_chain(catalog, "ko");
    assert(chain.size() == 2);
    assert(chain[0] == "ko");
    assert(chain[1] == "en");

    std::puts("PASS: resource catalog lookup and locale fallback");
}

void test_resource_catalog_diagnostics_are_actionable() {
    auto catalog = make_test_resource_catalog();
    catalog.application.id.clear();
    catalog.assets.push_back({
        .name = "app.icon",
        .source = "assets/duplicate-icon.txt",
        .content_type = "text/plain",
    });
    catalog.debug.artifact_manifest.clear();

    auto required = std::array<std::string_view, 1>{"action.archive"};
    auto diagnostics = validate_resource_catalog(catalog, required);

    auto has_kind = [&](ResourceDiagnosticKind kind,
                        std::string_view path) {
        return std::ranges::any_of(
            diagnostics,
            [&](ResourceDiagnostic const& diagnostic) {
                return diagnostic.kind == kind
                    && diagnostic.path == path
                    && diagnostic.severity == ResourceDiagnosticSeverity::Error
                    && !diagnostic.message.empty();
            });
    };

    assert(has_kind(ResourceDiagnosticKind::MissingApplicationId,
                    "application.id"));
    assert(has_kind(ResourceDiagnosticKind::DuplicateAssetName,
                    "assets[1].name"));
    assert(has_kind(ResourceDiagnosticKind::MissingArtifactManifest,
                    "debug.artifact_manifest"));
    assert(has_kind(ResourceDiagnosticKind::MissingLocaleKey,
                    "locales.en.action.archive"));
    assert(has_kind(ResourceDiagnosticKind::MissingLocaleKey,
                    "locales.ko.action.archive"));
    assert(!resource_catalog_ok(catalog));

    assert(std::string{
        resource_diagnostic_kind_name(
            ResourceDiagnosticKind::DuplicateAssetName)}
        == "duplicate_asset_name");
    assert(std::string{
        resource_diagnostic_severity_name(
            ResourceDiagnosticSeverity::Error)}
        == "error");

    std::puts("PASS: resource catalog diagnostics are actionable");
}

void test_resource_catalog_theme_defaults() {
    auto catalog = make_test_resource_catalog();
    catalog.default_font_family = "Pretendard";

    Theme theme{};
    theme.default_font_family = "System";
    auto themed = theme_with_resource_defaults(theme, catalog);
    assert(themed.default_font_family == "Pretendard");
    assert(theme.default_font_family == "System");

    catalog.default_font_family.clear();
    auto unchanged = theme_with_resource_defaults(theme, catalog);
    assert(unchanged.default_font_family == "System");

    std::puts("PASS: resource catalog theme defaults");
}

void test_icon_catalog_umbrella_export() {
    assert(phenotype::icon_catalog::style_name() == "macos_rounded_outline_svg");
    assert(phenotype::icon_catalog::all_symbol_count == icons::all_symbol_count);
    assert(phenotype::icon_catalog::file_type_symbol_count
           == icons::file_type_symbol_count);
    assert(phenotype::icon_catalog::svg_subset_policy()
           == "bounded_svg_icon_subset");
    assert(phenotype::icon_catalog::svg_supported_path_commands().find("A Z")
           != std::string_view::npos);
    assert(phenotype::icon_catalog::svg_supported_style_attributes()
               .find("stroke-linecap")
           != std::string_view::npos);
    assert(phenotype::icon_catalog::stroke_geometry_policy()
           == "round_cap_round_join_svg_strokes");
    assert(phenotype::icon_catalog::interface_metaphor_policy()
           == "familiar_simplified_macos_symbol_metaphors");
    assert(phenotype::icon_catalog::toolbar_symbol_chrome_policy()
           == "borderless_toolbar_symbols_inside_grouped_controls");
    assert(phenotype::icon_catalog::symbol_control_chrome_policy()
           == "macos_finder_symbol_state_chrome");
    assert(phenotype::icon_catalog::default_weight_policy()
           == "regular_text_weight_aligned");
    assert(phenotype::icon_catalog::rendering_capability_policy().find(
               "sf_symbols_mode_names")
           != std::string_view::npos);
    assert(phenotype::icon_catalog::monochrome_symbol_count
           == phenotype::icon_catalog::all_symbol_count);
    assert(phenotype::icon_catalog::regular_weight_symbol_count
           == phenotype::icon_catalog::all_symbol_count);
    assert(phenotype::icon_catalog::palette_symbol_count == 0);
    assert(phenotype::icon_catalog::multicolor_symbol_count == 0);
    assert(phenotype::icon_catalog::svg_path_arc_symbol_count == 1);
    assert(phenotype::icon_catalog::round_stroke_symbol_count
           == phenotype::icon_catalog::outline_symbol_count);
    assert(phenotype::icon_catalog::semantic_reference_name(
               phenotype::icon_catalog::Symbol::AirDrop)
           == "airdrop");
    assert(phenotype::icon_catalog::uses_svg_path_arcs(
               phenotype::icon_catalog::Symbol::AirDrop));
    auto const capabilities = phenotype::icon_catalog::rendering_capabilities(
        phenotype::icon_catalog::Symbol::AirDrop);
    assert(capabilities.monochrome);
    assert(capabilities.hierarchical);
    assert(!capabilities.palette);
    assert(!capabilities.multicolor);
    assert(capabilities.policy
           == phenotype::icon_catalog::rendering_capability_policy());
    assert(phenotype::icon_catalog::svg_source(
               phenotype::icon_catalog::Symbol::Applications)
               .find("currentColor")
           != std::string_view::npos);
    assert(phenotype::icon_catalog::file_type_color_policy()
           == "macos_finder_file_type_tints");
    assert(phenotype::icon_catalog::file_type_symbol_at(2)
           == phenotype::icon_catalog::Symbol::PdfDocument);
    assert(phenotype::icon_catalog::file_type_symbol_at(6)
           == phenotype::icon_catalog::Symbol::Archive);
    assert(phenotype::icon_catalog::interaction_tone_policy()
           == "macos_finder_interaction_tones");
    assert(phenotype::icon_catalog::metrics_policy()
           == "macos_finder_role_metrics_with_explicit_hit_targets");
    assert(phenotype::icon_catalog::hit_target_size(
               phenotype::icon_catalog::SymbolPresentationRole::Toolbar)
           == 36.0f);
    assert(phenotype::icon_catalog::hit_target_size(
               phenotype::icon_catalog::SymbolPresentationRole::Sidebar)
           == 38.0f);
    assert(phenotype::icon_catalog::activation_hit_target_size(
               phenotype::icon_catalog::SymbolPresentationRole::Toolbar)
           == 44.0f);
    assert(phenotype::icon_catalog::activation_hit_target_size(
               phenotype::icon_catalog::SymbolPresentationRole::Sidebar)
           == 44.0f);
    assert(phenotype::icon_catalog::activation_hit_target_size(
               phenotype::icon_catalog::SymbolPresentationRole::FileType)
           == 64.0f);
    assert(phenotype::icon_catalog::symbol_from_name("recents").has_value());
    assert(*phenotype::icon_catalog::symbol_from_name("recents")
           == phenotype::icon_catalog::Symbol::Recents);
    assert(phenotype::icon_catalog::symbol_from_semantic_reference_name(
               "magnifyingglass")
               .has_value());
    assert(*phenotype::icon_catalog::symbol_from_semantic_reference_name(
               "magnifyingglass")
           == phenotype::icon_catalog::Symbol::Search);
    assert(phenotype::icon_catalog::macos_interaction_tone(
               phenotype::icon_catalog::SymbolPresentationRole::Sidebar,
               phenotype::icon_catalog::SymbolInteractionState{true, true})
           == phenotype::icon_catalog::SymbolTone::Accent);
    auto const toolbar_chrome = phenotype::icon_catalog::macos_control_chrome(
        phenotype::icon_catalog::SymbolPresentationRole::Toolbar,
        phenotype::icon_catalog::SymbolInteractionState{false, true});
    assert(toolbar_chrome.background_color.a == 0);
    assert(toolbar_chrome.hover_background_color.a == 120);
    auto const sidebar_chrome = phenotype::icon_catalog::macos_control_chrome(
        phenotype::icon_catalog::SymbolPresentationRole::Sidebar,
        phenotype::icon_catalog::SymbolInteractionState{true, true});
    assert(sidebar_chrome.background_color.a == 238);
    assert(sidebar_chrome.hover_background_color.a == 248);

    std::puts("PASS: umbrella module exports icon catalog contract");
}

void test_io_contract_value_types() {
    auto frame = phenotype::io::InputFrame{
        .frame_index = 2,
        .timestamp_ms = 32,
        .deterministic = true,
    };
    frame.events.push_back(phenotype::io::InputEvent{
        .sequence = 1,
        .payload = phenotype::io::InputViewportEvent{
            .width = 900,
            .height = 620,
            .scale = 2.0f,
        },
    });
    frame.events.push_back(phenotype::io::InputEvent{
        .sequence = 2,
        .payload = phenotype::io::InputScrollEvent{
            .delta_x = 0.0f,
            .delta_y = -24.0f,
            .precise = true,
        },
    });

    auto script = phenotype::io::InputScript{
        .source_name = "test-script",
        .deterministic = true,
        .frames = {frame},
    };

    assert(phenotype::io::io_contract_version == 1);
    assert(phenotype::io::input_event_kinds.size() == 7);
    assert(phenotype::io::output_observation_kinds.size() == 6);
    assert(phenotype::io::input_event_kind_name(
               phenotype::io::InputEventKind::Pointer)
           == "pointer");
    assert(phenotype::io::output_observation_kind_name(
               phenotype::io::OutputObservationKind::PixelRegion)
           == "pixel_region");
    assert(phenotype::io::input_event_kind(frame.events[0])
           == phenotype::io::InputEventKind::Viewport);
    assert(phenotype::io::input_script_event_count(script) == 2);
    assert(phenotype::io::input_script_is_replayable(script));

    auto observation = phenotype::io::OutputObservation{
        .semantic_tree_present = true,
        .command_stream_present = true,
        .material_plans_present = true,
        .runtime_summary_present = true,
        .machine_readable_failure_shape = true,
        .semantic_node_count = 3,
        .command_stream = {.command_count = 4,
                           .path_count = 1,
                           .material_count = 1,
                           .text_count = 1,
                           .image_count = 0,
                           .bounded = true},
        .material = {.plan_count = 1,
                     .fallback_count = 0,
                     .runtime_pass_count = 2,
                     .semantic_runtime_match = true},
        .likely_layers = {"material_plan"},
        .likely_passes = {"blur_pass"},
    };

    auto bundle = phenotype::io::ArtifactBundleDescriptor{
        .snapshot_json = true,
        .frame_image = true,
        .platform_runtime_details = true,
        .observation = observation,
    };
    assert(phenotype::io::output_observation_is_llm_debuggable(observation));
    assert(phenotype::io::artifact_bundle_is_llm_debuggable(bundle));
    assert(phenotype::io::edge_effect_policy().find("filesystem")
           != std::string_view::npos);
    assert(phenotype::io::production_bypass_policy().find("release_adapters")
           != std::string_view::npos);

    std::puts("PASS: pure IO contract value types");
}

// ============================================================
// Runner
// ============================================================

int main() {
    test_column_layout();
    test_row_intrinsic_width();
    test_row_wraps_last_text_leaf();
    test_containment_invariant();
    test_alignment_center();
    test_max_width_centering();
    test_word_wrap();
    test_newline_handling();
    test_measure_text_cache_dedup();
    test_set_theme_updates_and_invalidates_cache();
    test_default_theme_glass_contract();
    test_sized_box_in_row();
    test_image_widget_layout_and_emit();
    test_svg_image_widget_uses_stable_paint_token();
    test_svg_image_widget_options_contract();
    test_grid_cell_text_is_vertically_centered();
    test_canvas_widget_invokes_painter();
    test_canvas_linear_gradient_rect_emits_command();
    test_canvas_bypasses_paint_cache_after_diff();
    test_canvas_paint_token_hit_skips_paint_fn();
    test_canvas_paint_token_miss_invokes_paint_fn();
    test_canvas_paint_token_lets_ancestor_blit();
    test_layout_relayout_when_available_width_changes();
    test_checkbox_and_radio_widgets();
    test_frame_skip_on_identical_cmd_buffer();
    test_paint_only_props_invalidate_diff_cache();
    test_material_props_invalidate_diff_cache();
    test_material_planner_backdrop_and_fallback_paths();
    test_material_text_foreground_resolution();
    test_material_surface_emits_material_rect_command();
    test_material_surface_shape_overrides();
    test_glass_surface_presets_emit_material_contract();
    test_material_container_scope_emits_command_context();
    test_material_command_preserves_style_optics();
    test_radio_paint_cache_stale_descendant_after_subtree_blit();
    test_row_cross_align_center_default();
    test_theme_json_roundtrip();
    test_theme_json_partial_keeps_defaults();
    test_theme_json_color_string_forms();
    test_theme_json_color_object_back_compat();
    test_theme_json_mixed_overlay();
    test_button_default_variant();
    test_button_primary_variant();
    test_button_default_hovered_snaps_to_hover_bg();
    test_button_primary_hovered_snaps_to_hover_bg();
    test_button_pressed_snaps_to_pressed_bg();
    test_button_focused_snaps_to_focus_ring_width();
    test_button_pointer_focused_hides_focus_ring();
    test_focus_visible_tracks_keyboard_modality();
    test_button_defocused_resting_border_width();
    test_button_disabled();
    test_button_disabled_custom_chrome();
    test_button_style_options_custom_chrome();
    test_canvas_button_semantic_and_layout_contract();
    test_canvas_button_minimum_hit_region_contract();
    test_canvas_button_focus_visible_contract();
    test_canvas_button_visual_state_contract();
    test_canvas_button_disabled_contract();
    test_canvas_button_disabled_custom_chrome();
    test_symbol_button_macos_contract();
    test_symbol_button_minimum_hit_region_contract();
    test_symbol_button_visual_state_token_contract();
    test_symbol_button_disabled_contract();
    test_text_field_default();
    test_text_field_default_placeholder();
    test_text_field_error();
    test_text_field_disabled();
    test_text_size_variants();
    test_text_color_variants();
    test_text_inline_code_chrome();
    test_space_value_resolves_each_token();
    test_column_props_default_and_override();
    test_row_props_default_and_override();
    test_draw_text_roundtrip_with_fontspec();
    test_widget_text_uses_theme_default_font_family();
    test_resource_catalog_lookup_and_locale_fallback();
    test_resource_catalog_diagnostics_are_actionable();
    test_resource_catalog_theme_defaults();
    test_icon_catalog_umbrella_export();
    test_io_contract_value_types();
    std::puts("\nAll tests passed.");
    return 0;
}
