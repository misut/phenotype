#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
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

static int g_application_runtime_open_url_calls = 0;
static std::string g_application_runtime_open_url_value;
static int g_application_runtime_settings_menu_calls = 0;

static void application_runtime_open_url_probe(
        char const* url,
        unsigned int len) {
    ++g_application_runtime_open_url_calls;
    g_application_runtime_open_url_value.assign(url, url + len);
}

static void application_runtime_settings_menu_probe() {
    ++g_application_runtime_settings_menu_calls;
}

// ============================================================
// Layout tests
// ============================================================

void test_column_layout() {
    detail::g_app().arena.reset();
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
    detail::g_app().arena.reset();
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
    detail::g_app().arena.reset();
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
    detail::g_app().arena.reset();
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
    detail::g_app().arena.reset();
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
    detail::g_app().arena.reset();
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
    detail::g_app().arena.reset();
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
    detail::g_app().arena.reset();
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
    detail::g_app().arena.reset();
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
    detail::g_app().arena.reset();
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
    assert(theme_contract::theme_contract_version == 2);
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

void test_scene_runtime_isolates_app_state_and_messages() {
    auto main_scene = runtime::main_scene();
    {
        auto activate_main = runtime::activate_scene(main_scene);
        detail::g_app().hovered_id = 11u;
        detail::g_app().focused_id = 12u;
        runtime::clear_messages();
        runtime::post<int>(7);
    }

    auto settings_scene = runtime::ensure_scene(SceneDescriptor{
        .id = "settings",
        .title = "Settings",
        .role = SceneRole::Settings,
        .visible = true,
    });
    {
        auto activate_settings = runtime::activate_scene(settings_scene);
        assert(detail::g_app().hovered_id == 0xFFFFFFFFu);
        assert(detail::g_app().focused_id == 0xFFFFFFFFu);
        detail::g_app().hovered_id = 21u;
        detail::g_app().focused_id = 22u;
        runtime::post<int>(42);
        auto settings_snapshot = runtime::active_scene();
        assert(settings_snapshot.id == "settings");
        assert(settings_snapshot.role == SceneRole::Settings);
        assert(settings_snapshot.hovered_id == 21u);
        assert(settings_snapshot.focused_id == 22u);
        assert(settings_snapshot.queued_messages == 1u);
        detail::g_app().has_active_animations = true;
        detail::g_app().debug_panel_refresh_active = true;
        auto settings_schedule = runtime::active_scene_schedule();
        assert(settings_schedule.has_active_animations);
        assert(settings_schedule.debug_panel_refresh_active);
        assert(detail::active_scene_needs_scheduled_tick());
        auto settings_messages = runtime::drain<int>();
        assert(settings_messages.size() == 1);
        assert(settings_messages[0] == 42);
    }

    {
        auto activate_main = runtime::activate_scene(main_scene);
        assert(runtime::active_scene().id == "main");
        assert(detail::g_app().hovered_id == 11u);
        assert(detail::g_app().focused_id == 12u);
        assert(!runtime::active_scene_schedule().has_active_animations);
        assert(!runtime::active_scene_schedule().debug_panel_refresh_active);
        assert(!detail::active_scene_needs_scheduled_tick());
        auto main_messages = runtime::drain<int>();
        assert(main_messages.size() == 1);
        assert(main_messages[0] == 7);
    }

    runtime::post_to_scene<int>(settings_scene, 99);
    assert(runtime::scene(settings_scene).queued_messages == 1u);
    auto settings_messages = runtime::drain_scene<int>(settings_scene);
    assert(settings_messages.size() == 1);
    assert(settings_messages[0] == 99);
    assert(runtime::active_scene().id == "main");

    auto snapshots = runtime::scenes();
    bool saw_main = false;
    bool saw_settings = false;
    for (auto const& snapshot : snapshots) {
        saw_main = saw_main || (snapshot.id == "main"
                                && snapshot.role == SceneRole::Main
                                && !snapshot.schedule.has_active_animations);
        saw_settings = saw_settings || (snapshot.id == "settings"
                                        && snapshot.role == SceneRole::Settings
                                        && snapshot.schedule.has_active_animations
                                        && snapshot.schedule.debug_panel_refresh_active);
    }
    assert(saw_main);
    assert(saw_settings);

    std::puts("PASS: scene runtime isolates app state and messages");
}

void test_message_queue_is_derived_from_active_scene() {
    auto main_scene = runtime::main_scene();
    auto tools_scene = runtime::ensure_scene(SceneDescriptor{
        .id = "message-queue-tools",
        .title = "Message Queue Tools",
        .role = SceneRole::Custom,
        .visible = true,
    });

    {
        auto activate_main = runtime::activate_scene(main_scene);
        runtime::clear_messages();
        runtime::post<int>(101);

        auto* main_queue = &detail::msg_queue();
        assert(main_queue == &detail::active_scene_runtime().messages);

        {
            auto activate_tools = runtime::activate_scene(tools_scene);
            runtime::clear_messages();
            auto* tools_queue = &detail::msg_queue();
            assert(tools_queue == &detail::active_scene_runtime().messages);
            assert(tools_queue != main_queue);
            assert(runtime::active_scene().queued_messages == 0u);

            runtime::post<int>(202);
            assert(tools_queue->size() == 1u);
        }

        assert(runtime::active_scene().id == "main");
        assert(&detail::msg_queue() == main_queue);
        assert(runtime::active_scene().queued_messages == 1u);

        auto main_messages = runtime::drain<int>();
        assert(main_messages.size() == 1u);
        assert(main_messages[0] == 101);
    }

    {
        auto activate_tools = runtime::activate_scene(tools_scene);
        auto tools_messages = runtime::drain<int>();
        assert(tools_messages.size() == 1u);
        assert(tools_messages[0] == 202);
    }

    std::puts("PASS: message queue is derived from active scene");
}

void test_app_state_is_derived_from_active_scene() {
    auto main_scene = runtime::main_scene();
    auto inspector_scene = runtime::ensure_scene(SceneDescriptor{
        .id = "app-state-inspector",
        .title = "App State Inspector",
        .role = SceneRole::Custom,
        .visible = true,
    });

    {
        auto activate_main = runtime::activate_scene(main_scene);
        auto* main_app = &detail::g_app();
        assert(main_app == &detail::active_scene_runtime().app_state());
        detail::g_app().focused_id = 301u;

        {
            auto activate_inspector = runtime::activate_scene(inspector_scene);
            auto* inspector_app = &detail::g_app();
            assert(inspector_app == &detail::active_scene_runtime().app_state());
            assert(inspector_app != main_app);
            assert(detail::g_app().focused_id == 0xFFFFFFFFu);
            detail::g_app().focused_id = 302u;
        }

        assert(runtime::active_scene().id == "main");
        assert(&detail::g_app() == main_app);
        assert(detail::g_app().focused_id == 301u);
    }

    {
        auto activate_inspector = runtime::activate_scene(inspector_scene);
        assert(detail::g_app().focused_id == 302u);
    }

    std::puts("PASS: app state is derived from active scene");
}

void test_scene_scheduler_clock_is_scene_local() {
    auto main_scene = runtime::main_scene();
    auto debug_scene = runtime::ensure_scene(SceneDescriptor{
        .id = "scheduler-debug-scene",
        .title = "Scheduler Debug Scene",
        .role = SceneRole::Debug,
        .visible = true,
    });

    {
        auto activate_main = runtime::activate_scene(main_scene);
        detail::g_app().has_active_animations = false;
        detail::g_app().scrollbar_animation_active = false;
        detail::g_app().debug_panel_refresh_active = false;
        detail::reset_active_scene_schedule_clock();
    }

    {
        auto activate_debug = runtime::activate_scene(debug_scene);
        detail::g_app().has_active_animations = false;
        detail::g_app().scrollbar_animation_active = false;
        detail::g_app().debug_panel_refresh_active = true;
        detail::reset_active_scene_schedule_clock();

        assert(detail::active_scene_needs_scheduled_tick());
        assert(detail::active_scene_debug_panel_only_refresh());
        assert(detail::active_scene_scheduled_tick_due(1'000));

        detail::note_active_scene_scheduled_tick(1'000);
        auto debug_schedule = runtime::active_scene_schedule();
        assert(debug_schedule.needs_scheduled_tick);
        assert(debug_schedule.debug_panel_only_refresh);
        assert(debug_schedule.scheduled_tick_interval_ns
               == k_scene_debug_tick_interval_ns);
        assert(debug_schedule.last_scheduled_tick_ns == 1'000);
        assert(debug_schedule.next_scheduled_tick_ns
               == 1'000 + k_scene_debug_tick_interval_ns);
        assert(debug_schedule.scheduled_tick_count == 1);
        assert(!detail::active_scene_scheduled_tick_due(1'000 + 10'000'000));
        assert(detail::active_scene_scheduled_tick_due(
            1'000 + k_scene_debug_tick_interval_ns));

        detail::g_app().has_active_animations = true;
        auto animation_schedule = runtime::active_scene_schedule();
        assert(animation_schedule.needs_scheduled_tick);
        assert(!animation_schedule.debug_panel_only_refresh);
        assert(animation_schedule.scheduled_tick_interval_ns
               == k_scene_animation_tick_interval_ns);
        assert(!detail::active_scene_scheduled_tick_due(1'000 + 10'000'000));
        assert(detail::active_scene_scheduled_tick_due(
            1'000 + k_scene_animation_tick_interval_ns));
    }

    {
        auto activate_main = runtime::activate_scene(main_scene);
        auto main_schedule = runtime::active_scene_schedule();
        assert(!main_schedule.needs_scheduled_tick);
        assert(main_schedule.last_scheduled_tick_ns == 0);
        assert(main_schedule.scheduled_tick_count == 0);

        auto debug_schedule = runtime::scene_schedule(debug_scene);
        assert(debug_schedule.scheduled_tick_count == 1);
        assert(runtime::active_scene().id == "main");
    }

    std::puts("PASS: scene scheduler clock is scene-local");
}

void test_render_surface_runtime_binds_scene_and_tracks_frames() {
    auto main_surface = runtime::main_render_surface();
    {
        auto activate_main = runtime::activate_render_surface(main_surface);
        runtime::configure_active_render_surface(RenderSurfaceDescriptor{
            .id = "main",
            .title = "Main",
            .scene_id = "main",
            .role = RenderSurfaceRole::MainWindow,
            .visible = true,
            .logical_width = 1024,
            .logical_height = 768,
            .framebuffer_width = 2048,
            .framebuffer_height = 1536,
            .content_scale = 2.0f,
        });
        auto snapshot = runtime::active_render_surface();
        assert(snapshot.id == "main");
        assert(snapshot.scene_id == "main");
        assert(snapshot.role == RenderSurfaceRole::MainWindow);
        assert(snapshot.active);
        assert(snapshot.logical_width == 1024);
        assert(snapshot.framebuffer_width == 2048);
        assert(runtime::active_scene().id == "main");
    }

    auto settings_surface =
        runtime::ensure_render_surface(RenderSurfaceDescriptor{
            .id = "settings-window-surface",
            .title = "Settings Surface",
            .scene_id = "surface-settings-scene",
            .role = RenderSurfaceRole::Settings,
            .visible = true,
            .logical_width = 420,
            .logical_height = 320,
            .framebuffer_width = 840,
            .framebuffer_height = 640,
            .content_scale = 2.0f,
        });
    assert(runtime::render_surface_exists(settings_surface));
    assert(runtime::render_surface(settings_surface).logical_width == 420);

    {
        auto activate_settings =
            runtime::activate_render_surface(settings_surface);
        assert(runtime::active_scene().id == "surface-settings-scene");
        assert(runtime::active_scene().role == SceneRole::Settings);
        detail::g_app().hovered_id = 314u;
        runtime::mark_active_render_surface_damaged();
        runtime::note_active_render_surface_frame();
        runtime::set_active_render_surface_paint_hash(12345u);
        runtime::note_active_render_surface_paint_flush();

        auto snapshot = runtime::active_render_surface();
        assert(snapshot.id == "settings-window-surface");
        assert(snapshot.scene_id == "surface-settings-scene");
        assert(snapshot.role == RenderSurfaceRole::Settings);
        assert(snapshot.active);
        assert(snapshot.logical_width == 420);
        assert(snapshot.frame_sequence == 1);
        assert(snapshot.damage_generation == 1);
        assert(snapshot.last_paint_hash == 12345u);
        assert(snapshot.paint_flush_count == 1u);
        assert(runtime::active_scene().hovered_id == 314u);
    }

    {
        auto activate_main = runtime::activate_render_surface(main_surface);
        assert(runtime::active_scene().id == "main");
        assert(runtime::active_scene().hovered_id != 314u);
        assert(runtime::active_render_surface().id == "main");
        assert(runtime::active_render_surface().last_paint_hash != 12345u);
        detail::g_app().last_paint_hash = 67890u;
        runtime::invalidate_active_render_surface_paint_cache();
        assert(runtime::active_render_surface().last_paint_hash == 0u);
        assert(detail::g_app().last_paint_hash == 0u);
    }

    auto surfaces = runtime::render_surfaces();
    bool saw_main = false;
    bool saw_settings = false;
    for (auto const& surface : surfaces) {
        saw_main = saw_main || (surface.id == "main"
                                && surface.role == RenderSurfaceRole::MainWindow);
        saw_settings = saw_settings
            || (surface.id == "settings-window-surface"
                && surface.scene_id == "surface-settings-scene"
                && surface.frame_sequence == 1
                && surface.damage_generation == 1
                && surface.last_paint_hash == 12345u
                && surface.paint_flush_count == 1u);
    }
    assert(saw_main);
    assert(saw_settings);

    std::puts("PASS: render surface runtime binds scenes and tracks frames");
}

void test_render_surface_visibility_updates_bound_scene() {
    auto scene = runtime::ensure_scene(SceneDescriptor{
        .id = "surface-lifecycle-scene",
        .title = "Surface Lifecycle Scene",
        .role = SceneRole::Settings,
        .visible = true,
    });
    auto surface = runtime::ensure_render_surface(RenderSurfaceDescriptor{
        .id = "surface-lifecycle-surface",
        .title = "Surface Lifecycle Surface",
        .scene_id = "surface-lifecycle-scene",
        .role = RenderSurfaceRole::Settings,
        .visible = true,
        .logical_width = 320,
        .logical_height = 240,
        .framebuffer_width = 640,
        .framebuffer_height = 480,
        .content_scale = 2.0f,
    });
    assert(runtime::scene(scene).visible);
    assert(runtime::render_surface(surface).visible);

    runtime::ensure_render_surface(RenderSurfaceDescriptor{
        .id = "surface-lifecycle-surface",
        .title = "Surface Lifecycle Surface",
        .scene_id = "surface-lifecycle-scene",
        .role = RenderSurfaceRole::Settings,
        .visible = false,
        .logical_width = 320,
        .logical_height = 240,
        .framebuffer_width = 640,
        .framebuffer_height = 480,
        .content_scale = 2.0f,
    });
    assert(!runtime::render_surface(surface).visible);
    assert(!runtime::scene(scene).visible);

    runtime::set_scene_visible(scene, true);
    assert(runtime::scene(scene).visible);
    assert(!runtime::render_surface(surface).visible);

    runtime::set_render_surface_visible(surface, true);
    assert(runtime::render_surface(surface).visible);
    assert(runtime::scene(scene).visible);

    runtime::set_render_surface_visible("surface-lifecycle-surface", false);
    assert(!runtime::render_surface(surface).visible);
    assert(!runtime::scene(scene).visible);

    std::puts("PASS: render surface visibility updates bound scene");
}

void test_application_runtime_snapshot_tracks_process_owners() {
    detail::set_application_open_url_handler(nullptr);
    detail::set_application_settings_menu_handler(nullptr);
    g_application_runtime_open_url_calls = 0;
    g_application_runtime_settings_menu_calls = 0;

    auto active_scene_before = runtime::active_scene_handle();
    auto active_surface_before = runtime::active_render_surface_handle();
    auto before = runtime::application_runtime();
    assert(before.scene_runtime_owner == "ApplicationSceneRuntimeStore");
    assert(before.scene_count >= 1u);
    assert(before.render_surface_count >= 1u);
    assert(!before.active_scene_id.empty());
    assert(!before.active_render_surface_id.empty());
    assert(!before.open_url_handler_installed);
    assert(!before.settings_menu_handler_installed);

    detail::set_application_open_url_handler(
        application_runtime_open_url_probe);
    auto with_opener = runtime::application_runtime();
    assert(with_opener.open_url_handler_installed);
    std::string_view const runtime_url =
        "https://example.com/runtime-open-url";
    assert(detail::open_application_url(
        runtime_url.data(),
        static_cast<unsigned int>(runtime_url.size())));
    assert(g_application_runtime_open_url_calls == 1);
    assert(g_application_runtime_open_url_value == runtime_url);
    detail::set_application_open_url_handler(nullptr);
    auto after_opener_reset = runtime::application_runtime();
    assert(!after_opener_reset.open_url_handler_installed);

    detail::set_application_settings_menu_handler(
        application_runtime_settings_menu_probe);
    auto with_settings_menu = runtime::application_runtime();
    assert(with_settings_menu.settings_menu_handler_installed);
    assert(detail::invoke_application_settings_menu());
    assert(g_application_runtime_settings_menu_calls == 1);
    detail::set_application_settings_menu_handler(nullptr);
    auto after_settings_menu_reset = runtime::application_runtime();
    assert(!after_settings_menu_reset.settings_menu_handler_installed);

    auto scene = runtime::ensure_scene(SceneDescriptor{
        .id = "application-runtime-settings-scene",
        .title = "Application Runtime Settings",
        .role = SceneRole::Settings,
        .visible = true,
    });
    auto with_scene = runtime::application_runtime();
    assert(with_scene.scene_count == before.scene_count + 1u);
    assert(with_scene.visible_scene_count == before.visible_scene_count + 1u);
    assert(with_scene.active_scene_id == active_scene_before.id);
    assert(with_scene.active_render_surface_id == active_surface_before.id);

    auto surface = runtime::ensure_render_surface(RenderSurfaceDescriptor{
        .id = "application-runtime-settings-surface",
        .title = "Application Runtime Settings Surface",
        .scene_id = "application-runtime-settings-scene",
        .role = RenderSurfaceRole::Settings,
        .visible = false,
        .logical_width = 320,
        .logical_height = 240,
        .framebuffer_width = 640,
        .framebuffer_height = 480,
        .content_scale = 2.0f,
    });
    auto with_surface = runtime::application_runtime();
    assert(with_surface.scene_count == before.scene_count + 1u);
    assert(with_surface.visible_scene_count == before.visible_scene_count);
    assert(with_surface.render_surface_count == before.render_surface_count + 1u);
    assert(with_surface.visible_render_surface_count
           == before.visible_render_surface_count);
    assert(with_surface.active_scene_id == active_scene_before.id);
    assert(with_surface.active_render_surface_id == active_surface_before.id);

    {
        auto activate = runtime::activate_render_surface(surface);
        runtime::mark_active_render_surface_damaged();
        auto active = runtime::application_runtime();
        assert(active.active_scene_id == scene.id);
        assert(active.active_scene_role == SceneRole::Settings);
        assert(!active.active_scene_visible);
        assert(active.active_render_surface_id == surface.id);
        assert(active.active_render_surface_scene_id == scene.id);
        assert(active.active_render_surface_role == RenderSurfaceRole::Settings);
        assert(!active.active_render_surface_visible);
        assert(active.damaged_render_surface_count
               == with_surface.damaged_render_surface_count + 1u);
    }

    assert(runtime::active_scene_handle().id == active_scene_before.id);
    assert(runtime::active_render_surface_handle().id == active_surface_before.id);

    std::puts("PASS: application runtime snapshot tracks process owners");
}

void test_active_runtime_binding_restores_scene_and_surface() {
    auto active_scene_before = runtime::active_scene_handle();
    auto active_surface_before = runtime::active_render_surface_handle();
    auto scene_a = runtime::ensure_scene(SceneDescriptor{
        .id = "active-binding-scene-a",
        .title = "Active Binding Scene A",
        .role = SceneRole::Settings,
        .visible = true,
    });
    auto scene_b = runtime::ensure_scene(SceneDescriptor{
        .id = "active-binding-scene-b",
        .title = "Active Binding Scene B",
        .role = SceneRole::Debug,
        .visible = true,
    });
    auto surface_a = runtime::ensure_render_surface(RenderSurfaceDescriptor{
        .id = "active-binding-surface-a",
        .title = "Active Binding Surface A",
        .scene_id = "active-binding-scene-a",
        .role = RenderSurfaceRole::Settings,
        .visible = true,
        .logical_width = 320,
        .logical_height = 240,
        .framebuffer_width = 640,
        .framebuffer_height = 480,
        .content_scale = 2.0f,
    });
    auto surface_b = runtime::ensure_render_surface(RenderSurfaceDescriptor{
        .id = "active-binding-surface-b",
        .title = "Active Binding Surface B",
        .scene_id = "active-binding-scene-b",
        .role = RenderSurfaceRole::Debug,
        .visible = true,
        .logical_width = 480,
        .logical_height = 320,
        .framebuffer_width = 960,
        .framebuffer_height = 640,
        .content_scale = 2.0f,
    });

    {
        auto activate_surface_a = runtime::activate_render_surface(surface_a);
        assert(runtime::active_scene_handle().id == scene_a.id);
        assert(runtime::active_render_surface_handle().id == surface_a.id);
        assert(runtime::scene(scene_a).active);
        assert(runtime::render_surface(surface_a).active);

        {
            auto activate_scene_b = runtime::activate_scene(scene_b);
            assert(runtime::active_scene_handle().id == scene_b.id);
            assert(runtime::active_render_surface_handle().id == surface_a.id);
            assert(runtime::scene(scene_b).active);
            assert(runtime::render_surface(surface_a).active);

            {
                auto activate_surface_b =
                    runtime::activate_render_surface(surface_b);
                assert(runtime::active_scene_handle().id == scene_b.id);
                assert(runtime::active_render_surface_handle().id
                       == surface_b.id);
                assert(runtime::scene(scene_b).active);
                assert(runtime::render_surface(surface_b).active);
            }

            assert(runtime::active_scene_handle().id == scene_b.id);
            assert(runtime::active_render_surface_handle().id == surface_a.id);
            assert(runtime::scene(scene_b).active);
            assert(runtime::render_surface(surface_a).active);
        }

        assert(runtime::active_scene_handle().id == scene_a.id);
        assert(runtime::active_render_surface_handle().id == surface_a.id);
        assert(runtime::scene(scene_a).active);
        assert(runtime::render_surface(surface_a).active);
    }

    assert(runtime::active_scene_handle().id == active_scene_before.id);
    assert(runtime::active_render_surface_handle().id == active_surface_before.id);
    std::puts("PASS: active runtime binding restores scene and surface");
}

void test_app_runner_uses_context_pointer() {
    struct RunnerContext {
        int rebuilds = 0;
        int payload = 0;
    };

    auto& scene = detail::active_scene_runtime();
    auto* previous_runner = scene.runner;
    auto* previous_context = scene.runner_context;
    auto previous_owner = scene.runner_context_owner;

    RunnerContext first{.payload = 7};
    RunnerContext second{.payload = 11};
    detail::install_app_runner([](void* raw) {
        auto& context = *static_cast<RunnerContext*>(raw);
        context.rebuilds += context.payload;
    }, &first);
    detail::trigger_rebuild();
    assert(first.rebuilds == 7);
    assert(second.rebuilds == 0);

    detail::install_app_runner([](void* raw) {
        auto& context = *static_cast<RunnerContext*>(raw);
        context.rebuilds += context.payload;
    }, &second);
    detail::trigger_rebuild();
    assert(first.rebuilds == 7);
    assert(second.rebuilds == 11);

    detail::install_app_runner(previous_runner, previous_context, previous_owner);
    std::puts("PASS: app runner uses context pointer");
}

void test_app_runner_can_own_context_pointer() {
    struct RunnerContext {
        int rebuilds = 0;
    };

    auto& scene = detail::active_scene_runtime();
    auto* previous_runner = scene.runner;
    auto* previous_context = scene.runner_context;
    auto previous_owner = scene.runner_context_owner;

    std::weak_ptr<RunnerContext> weak_context;
    {
        auto context = std::make_shared<RunnerContext>();
        weak_context = context;
        auto* raw_context = context.get();
        detail::install_app_runner([](void* raw) {
            auto& context = *static_cast<RunnerContext*>(raw);
            context.rebuilds += 1;
        }, raw_context, std::move(context));
    }

    assert(!weak_context.expired());
    detail::trigger_rebuild();
    {
        auto locked = weak_context.lock();
        assert(locked);
        assert(locked->rebuilds == 1);
    }

    detail::install_app_runner(nullptr, nullptr);
    assert(weak_context.expired());
    detail::install_app_runner(previous_runner, previous_context, previous_owner);

    std::puts("PASS: app runner can own context pointer");
}

static int legacy_runner_scene_a_rebuilds = 0;
static int legacy_runner_scene_b_rebuilds = 0;

void legacy_runner_scene_a() {
    ++legacy_runner_scene_a_rebuilds;
}

void legacy_runner_scene_b() {
    ++legacy_runner_scene_b_rebuilds;
}

void test_legacy_app_runner_context_is_scene_local() {
    auto active_before = runtime::active_scene_handle();
    auto scene_a = runtime::ensure_scene(SceneDescriptor{
        .id = "legacy-runner-scene-a",
        .title = "Legacy Runner Scene A",
        .role = SceneRole::Settings,
        .visible = true,
    });
    auto scene_b = runtime::ensure_scene(SceneDescriptor{
        .id = "legacy-runner-scene-b",
        .title = "Legacy Runner Scene B",
        .role = SceneRole::Debug,
        .visible = true,
    });

    {
        auto activate = runtime::activate_scene(scene_a);
        detail::install_app_runner(legacy_runner_scene_a);
    }
    {
        auto activate = runtime::activate_scene(scene_b);
        detail::install_app_runner(legacy_runner_scene_b);
    }

    legacy_runner_scene_a_rebuilds = 0;
    legacy_runner_scene_b_rebuilds = 0;
    runtime::trigger_scene_rebuild(scene_a);
    assert(legacy_runner_scene_a_rebuilds == 1);
    assert(legacy_runner_scene_b_rebuilds == 0);

    runtime::trigger_scene_rebuild(scene_b);
    assert(legacy_runner_scene_a_rebuilds == 1);
    assert(legacy_runner_scene_b_rebuilds == 1);

    runtime::clear_scene_runner(scene_a);
    runtime::trigger_scene_rebuild(scene_a);
    runtime::trigger_scene_rebuild(scene_b);
    assert(legacy_runner_scene_a_rebuilds == 1);
    assert(legacy_runner_scene_b_rebuilds == 2);

    runtime::clear_scene_runner(scene_b);
    assert(runtime::active_scene_handle().id == active_before.id);

    std::puts("PASS: legacy app runner context is scene-local");
}

void test_material_build_context_is_scene_local() {
    auto active_before = runtime::active_scene_handle();
    auto scene_a = runtime::ensure_scene(SceneDescriptor{
        .id = "material-build-context-scene-a",
        .title = "Material Build Context Scene A",
        .role = SceneRole::Settings,
        .visible = true,
    });
    auto scene_b = runtime::ensure_scene(SceneDescriptor{
        .id = "material-build-context-scene-b",
        .title = "Material Build Context Scene B",
        .role = SceneRole::Debug,
        .visible = true,
    });
    auto container_union = [] {
        return layout::current_material_container().union_id;
    };
    auto transition_kind = [] {
        return layout::current_material_transition().kind;
    };
    auto transition_progress = [] {
        return layout::current_material_transition().progress;
    };
    auto identity_namespace = [] {
        return layout::current_material_glass_identity().namespace_id;
    };
    auto identity_effect = [] {
        return layout::current_material_glass_identity().effect_id;
    };
    void* scene_a_context_owner = nullptr;
    void* scene_b_context_owner = nullptr;

    {
        auto activate_a = runtime::activate_scene(scene_a);
        assert(!detail::active_scene_runtime().material_build_context_owner);
        layout::glass_effect_union(101u, [&] {
            scene_a_context_owner =
                detail::active_scene_runtime().material_build_context_owner.get();
            assert(scene_a_context_owner);
            assert(container_union() == 101u);
            layout::glass_effect_transition(
                layout::glass_matched_geometry_transition(0.25f),
                [&] {
                    assert(transition_kind()
                           == MaterialGlassTransitionKind::MatchedGeometry);
                    assert(transition_progress() == 0.25f);
                    layout::glass_effect_id(201u, 301u, [&] {
                        assert(identity_namespace() == 201u);
                        assert(identity_effect() == 301u);

                        {
                            auto activate_b = runtime::activate_scene(scene_b);
                            assert(!detail::active_scene_runtime()
                                        .material_build_context_owner);
                            assert(container_union() == 0u);
                            assert(transition_kind()
                                   == MaterialGlassTransitionKind::Identity);
                            assert(identity_effect() == 0u);
                            layout::glass_effect_union(202u, [&] {
                                scene_b_context_owner =
                                    detail::active_scene_runtime()
                                        .material_build_context_owner.get();
                                assert(scene_b_context_owner);
                                assert(scene_b_context_owner
                                       != scene_a_context_owner);
                                assert(container_union() == 202u);
                            });
                            assert(container_union() == 0u);
                        }

                        assert(runtime::active_scene_handle().id == scene_a.id);
                        assert(detail::active_scene_runtime()
                                   .material_build_context_owner.get()
                               == scene_a_context_owner);
                        assert(container_union() == 101u);
                        assert(transition_kind()
                               == MaterialGlassTransitionKind::MatchedGeometry);
                        assert(identity_effect() == 301u);
                    });
                });
        });
        assert(container_union() == 0u);
        assert(transition_kind() == MaterialGlassTransitionKind::Identity);
        assert(identity_effect() == 0u);
    }

    assert(runtime::active_scene_handle().id == active_before.id);
    assert(scene_a_context_owner);
    assert(scene_b_context_owner);

    std::puts("PASS: material build context is scene-local");
}

void test_view_build_scope_is_scene_local() {
    auto active_before = runtime::active_scene_handle();
    auto scene_a = runtime::ensure_scene(SceneDescriptor{
        .id = "view-build-scope-scene-a",
        .title = "View Build Scope Scene A",
        .role = SceneRole::Settings,
        .visible = true,
    });
    auto scene_b = runtime::ensure_scene(SceneDescriptor{
        .id = "view-build-scope-scene-b",
        .title = "View Build Scope Scene B",
        .role = SceneRole::Debug,
        .visible = true,
    });

    {
        auto activate_a = runtime::activate_scene(scene_a);
        detail::g_app().arena.reset();
        auto root_a = detail::alloc_node();
        Scope scope_a(root_a, 111u);
        Scope::set_current(&scope_a);
        assert(Scope::current() == &scope_a);

        keyed(101u, [&] {
            assert(detail::pending_child_key() == 101u);
            {
                auto activate_b = runtime::activate_scene(scene_b);
                assert(Scope::current() == nullptr);
                assert(detail::pending_child_key() == LayoutNode::unkeyed_key);

                detail::g_app().arena.reset();
                auto root_b = detail::alloc_node();
                Scope scope_b(root_b, 222u);
                Scope::set_current(&scope_b);
                assert(Scope::current() == &scope_b);

                keyed(202u, [&] {
                    assert(detail::pending_child_key() == 202u);
                });
                assert(detail::pending_child_key() == LayoutNode::unkeyed_key);
                Scope::set_current(nullptr);
            }

            assert(runtime::active_scene_handle().id == scene_a.id);
            assert(Scope::current() == &scope_a);
            assert(detail::pending_child_key() == 101u);
        });

        assert(detail::pending_child_key() == LayoutNode::unkeyed_key);
        Scope::set_current(nullptr);
    }

    assert(runtime::active_scene_handle().id == active_before.id);
    std::puts("PASS: view build scope is scene-local");
}

void test_runtime_run_scene_keeps_same_types_scene_local() {
    struct SceneRunState {
        int value = 0;
    };

    struct ViewProbe {
        int* last_seen = nullptr;

        void operator()(SceneRunState const& state) const {
            if (last_seen)
                *last_seen = state.value;
            widget::text("scene value=" + std::to_string(state.value));
        }
    };

    struct UpdateProbe {
        void operator()(SceneRunState& state, int msg) const {
            state.value += msg;
        }
    };

    auto scene_a = runtime::ensure_scene(SceneDescriptor{
        .id = "run-scene-a",
        .title = "Run Scene A",
        .role = SceneRole::Settings,
        .visible = true,
    });
    auto scene_b = runtime::ensure_scene(SceneDescriptor{
        .id = "run-scene-b",
        .title = "Run Scene B",
        .role = SceneRole::Debug,
        .visible = true,
    });
    auto active_before = runtime::active_scene_handle();
    int last_a = -1;
    int last_b = -1;

#if !defined(__wasi__) && !defined(__ANDROID__)
    null_host host_a;
    null_host host_b;
    runtime::run_scene<SceneRunState, int>(
        scene_a,
        host_a,
        ViewProbe{&last_a},
        UpdateProbe{});
    runtime::run_scene<SceneRunState, int>(
        scene_b,
        host_b,
        ViewProbe{&last_b},
        UpdateProbe{});
#elif defined(__wasi__)
    runtime::run_scene<SceneRunState, int>(
        scene_a,
        ViewProbe{&last_a},
        UpdateProbe{});
    runtime::run_scene<SceneRunState, int>(
        scene_b,
        ViewProbe{&last_b},
        UpdateProbe{});
#else
    std::puts("SKIP: runtime run_scene scene-local contexts");
    return;
#endif

    assert(runtime::active_scene_handle().id == active_before.id);
    assert(last_a == 0);
    assert(last_b == 0);

    runtime::post_to_scene<int>(scene_a, 3);
    runtime::trigger_scene_rebuild(scene_a);
    assert(last_a == 3);
    assert(last_b == 0);
    assert(runtime::active_scene_handle().id == active_before.id);

    runtime::post_to_scene<int>(scene_b, 5);
    runtime::trigger_scene_rebuild(scene_b);
    assert(last_a == 3);
    assert(last_b == 5);

    runtime::post_to_scene<int>(scene_a, 2);
    runtime::trigger_scene_rebuild(scene_a);
    assert(last_a == 5);
    assert(last_b == 5);

    runtime::clear_scene_runner(scene_a);
    runtime::clear_scene_runner(scene_b);

    std::puts("PASS: runtime run_scene keeps same types scene-local");
}

void test_runtime_run_scene_can_share_external_state() {
    struct SharedState {
        int value = 7;
    };

    struct ViewProbe {
        int* last_seen = nullptr;

        void operator()(SharedState const& state) const {
            if (last_seen)
                *last_seen = state.value;
            widget::text("shared value=" + std::to_string(state.value));
        }
    };

    struct UpdateProbe {
        void operator()(SharedState& state, int msg) const {
            state.value += msg;
        }
    };

    auto scene = runtime::ensure_scene(SceneDescriptor{
        .id = "run-scene-shared-state",
        .title = "Run Scene Shared State",
        .role = SceneRole::Settings,
        .visible = true,
    });
    auto active_before = runtime::active_scene_handle();
    SharedState state{};
    int last_seen = -1;

#if !defined(__wasi__) && !defined(__ANDROID__)
    null_host host;
    runtime::run_scene_with_state<SharedState, int>(
        scene,
        host,
        state,
        ViewProbe{&last_seen},
        UpdateProbe{});
#elif defined(__wasi__)
    runtime::run_scene_with_state<SharedState, int>(
        scene,
        state,
        ViewProbe{&last_seen},
        UpdateProbe{});
#else
    std::puts("SKIP: runtime run_scene shared external state");
    return;
#endif

    assert(runtime::active_scene_handle().id == active_before.id);
    assert(state.value == 7);
    assert(last_seen == 7);

    runtime::post_to_scene<int>(scene, 5);
    runtime::trigger_scene_rebuild(scene);
    assert(state.value == 12);
    assert(last_seen == 12);
    assert(runtime::active_scene_handle().id == active_before.id);

    state.value = 40;
    runtime::trigger_scene_rebuild(scene);
    assert(last_seen == 40);

    runtime::clear_scene_runner(scene);

    std::puts("PASS: runtime run_scene can share external state");
}

void test_runtime_scene_runner_targets_scene() {
    struct RunnerContext {
        int rebuilds = 0;
        int payload = 0;
    };

    auto main_scene = runtime::main_scene();
    auto settings_scene = runtime::ensure_scene(SceneDescriptor{
        .id = "runner-settings-scene",
        .title = "Runner Settings",
        .role = SceneRole::Settings,
        .visible = true,
    });
    auto active_before = runtime::active_scene_handle();

    RunnerContext main_context{.payload = 3};
    RunnerContext settings_context{.payload = 5};
    auto runner = [](void* raw) {
        auto& context = *static_cast<RunnerContext*>(raw);
        context.rebuilds += context.payload;
    };

    runtime::install_scene_runner(main_scene, runner, &main_context);
    runtime::install_scene_runner(settings_scene, runner, &settings_context);
    assert(runtime::scene_has_runner(main_scene));
    assert(runtime::scene_has_runner(settings_scene));

    runtime::trigger_scene_rebuild(settings_scene);
    assert(main_context.rebuilds == 0);
    assert(settings_context.rebuilds == 5);
    assert(runtime::active_scene_handle().id == active_before.id);

    runtime::trigger_scene_rebuild(main_scene);
    assert(main_context.rebuilds == 3);
    assert(settings_context.rebuilds == 5);
    assert(runtime::active_scene_handle().id == active_before.id);

    runtime::clear_scene_runner(settings_scene);
    assert(!runtime::scene_has_runner(settings_scene));
    assert(runtime::scene_has_runner(main_scene));
    runtime::clear_scene_runner(main_scene);
    assert(!runtime::scene_has_runner(main_scene));

    std::puts("PASS: runtime scene runner targets scenes");
}

void test_scene_runner_survives_app_state_swap() {
    struct RunnerContext {
        int rebuilds = 0;
    };

    auto scene = runtime::ensure_scene(SceneDescriptor{
        .id = "runner-app-swap-scene",
        .title = "Runner App Swap",
        .role = SceneRole::Settings,
        .visible = true,
    });

    RunnerContext context{};
    auto runner = [](void* raw) {
        auto& context = *static_cast<RunnerContext*>(raw);
        context.rebuilds += 1;
    };

    {
        runtime::SceneActivation activate{scene};
        detail::install_app_runner(runner, &context);
        auto& runtime_scene = detail::active_scene_runtime();
        runtime_scene.owned_app = std::make_unique<AppState>();
        runtime_scene.app = runtime_scene.owned_app.get();
    }

    assert(runtime::scene_has_runner(scene));
    runtime::trigger_scene_rebuild(scene);
    assert(context.rebuilds == 1);
    runtime::clear_scene_runner(scene);

    std::puts("PASS: scene runner survives app state swap");
}

void test_system_theme_preferences_are_pure_overlays() {
    Theme theme{};
    theme.default_font_family = "Pretendard";
    theme.body_font_size = 14.0f;
    theme.heading_font_size = 18.0f;
    theme.small_font_size = 12.0f;
    theme.scroll_delta_multiplier = 1.0f;

    PlatformSystemSettingsSnapshot system{};
    system.source = "test-system";
    system.font_family = "System UI";
    system.font_scale = 1.25f;
    system.line_height_ratio = 1.6f;
    system.text_size_source = "test-text-size";
    system.scroll_delta_multiplier = 1.25f;
    system.scroll_horizontal_delta_multiplier = 0.5f;
    system.scroll_source = "test-scroll";
    system.color_scheme = "dark";
    system.color_scheme_source = "test-appearance";
    system.accent_color_available = true;
    system.accent_color = Color{88, 86, 214, 255};
    system.accent_color_source = "test-accent";
    system.reduce_motion = true;
    system.accessibility_source = "test-accessibility";

    ThemePreferenceOverrides overrides{};
    overrides.font_scale = 1.2f;
    overrides.scroll_delta_multiplier = 1.5f;
    overrides.scroll_horizontal_delta_multiplier = 2.0f;
    overrides.scroll_bar_visibility = "always";

    auto applied = apply_system_theme_preferences(theme, system, overrides);
    auto resolved = resolve_system_theme_preferences(
        theme,
        system,
        overrides,
        "test-preferences");
    auto pure_resolved = theme_contract::resolve_theme_preferences(
        theme_contract_preference_base(theme),
        theme_contract_system_snapshot(system),
        overrides,
        "test-preferences");
    assert(resolved.source == "test-preferences");
    assert(pure_resolved.source == resolved.source);
    assert(resolved.theme.default_font_family == applied.default_font_family);
    assert(std::fabs(resolved.effective_body_font_size - 21.0f) < 0.001f);
    assert(std::fabs(resolved.effective_heading_font_size - 27.0f) < 0.001f);
    assert(std::fabs(resolved.effective_small_font_size - 18.0f) < 0.001f);
    assert(std::fabs(resolved.effective_line_height_ratio - 1.6f) < 0.001f);
    assert(resolved.effective_font_family == "Pretendard");
    assert(resolved.effective_color_scheme == "light");
    assert(!resolved.used_system_font_family);
    assert(!resolved.used_system_color_scheme);
    assert(!resolved.used_system_font_metrics);
    assert(resolved.used_system_font_scale);
    assert(resolved.used_user_font_scale);
    assert(resolved.used_system_line_height);
    assert(resolved.used_system_scroll_metrics);
    assert(resolved.used_user_scroll_scale);
    assert(resolved.used_user_scroll_bar_visibility);
    assert(resolved.used_system_reduce_motion);
    assert(!resolved.used_user_motion_scale);
    assert(pure_resolved.used_system_scroll_metrics
           == resolved.used_system_scroll_metrics);
    assert(pure_resolved.used_user_scroll_scale
           == resolved.used_user_scroll_scale);
    assert(pure_resolved.used_user_scroll_bar_visibility
           == resolved.used_user_scroll_bar_visibility);
    assert(pure_resolved.used_system_reduce_motion
           == resolved.used_system_reduce_motion);
    assert(std::fabs(pure_resolved.effective_body_font_size
                     - resolved.effective_body_font_size)
           < 0.001f);
    assert(std::fabs(pure_resolved.effective_motion_duration_multiplier
                     - resolved.effective_motion_duration_multiplier)
           < 0.001f);
    assert(applied.default_font_family == "Pretendard");
    assert(std::fabs(applied.body_font_size - 21.0f) < 0.001f);
    assert(std::fabs(applied.heading_font_size - 27.0f) < 0.001f);
    assert(std::fabs(applied.small_font_size - 18.0f) < 0.001f);
    assert(std::fabs(applied.scroll_delta_multiplier - 1.875f) < 0.001f);
    assert(std::fabs(applied.scroll_horizontal_delta_multiplier - 1.5f)
           < 0.001f);
    assert(applied.scroll_bar_visibility == "always");
    assert(std::fabs(applied.motion_duration_multiplier - 0.0f) < 0.001f);
    assert(applied.accent == theme.accent);

    PlatformSystemSettingsSnapshot fallback_system{};
    fallback_system.color_scheme = "dark";
    fallback_system.font_scale = 1.4f;
    fallback_system.line_height_ratio = 1.8f;
    fallback_system.scroll_delta_multiplier = 2.0f;
    ThemePreferenceOverrides fallback_overrides{};
    fallback_overrides.prefer_system_color_scheme = true;
    auto fallback_resolved = resolve_system_theme_preferences(
        theme,
        fallback_system,
        fallback_overrides,
        "fallback-source");
    assert(fallback_resolved.effective_color_scheme == "light");
    assert(!fallback_resolved.used_system_color_scheme);
    assert(!fallback_resolved.used_system_font_scale);
    assert(!fallback_resolved.used_system_line_height);
    assert(!fallback_resolved.used_system_scroll_metrics);
    assert(!fallback_resolved.used_system_reduce_motion);
    assert(std::fabs(fallback_resolved.theme.body_font_size - 14.0f) < 0.001f);
    assert(std::fabs(fallback_resolved.theme.line_height_ratio - 1.6f)
           < 0.001f);
    assert(std::fabs(fallback_resolved.theme.scroll_delta_multiplier - 1.0f)
           < 0.001f);
    assert(fallback_resolved.theme.scroll_bar_visibility == "auto");
    assert(std::fabs(fallback_resolved.theme.motion_duration_multiplier - 1.0f)
           < 0.001f);

    ThemePreferenceOverrides motion_overrides{};
    motion_overrides.apply_system_reduce_motion = false;
    motion_overrides.motion_duration_multiplier = 0.5f;
    auto motion_resolved = resolve_system_theme_preferences(
        theme,
        system,
        motion_overrides,
        "motion-source");
    assert(!motion_resolved.used_system_reduce_motion);
    assert(motion_resolved.used_user_motion_scale);
    assert(std::fabs(motion_resolved.theme.motion_duration_multiplier - 0.5f)
           < 0.001f);

    overrides.apply_system_scroll_metrics = false;
    applied = apply_system_theme_preferences(theme, system, overrides);
    assert(std::fabs(applied.scroll_delta_multiplier - 1.5f) < 0.001f);
    assert(std::fabs(applied.scroll_horizontal_delta_multiplier - 3.0f)
           < 0.001f);
    assert(applied.scroll_bar_visibility == "always");
    overrides.apply_system_scroll_metrics = true;

    system.body_font_size = 13.0f;
    system.heading_font_size = 17.0f;
    system.small_font_size = 11.0f;
    overrides = ThemePreferenceOverrides{};
    applied = apply_system_theme_preferences(theme, system, overrides);
    resolved = resolve_system_theme_preferences(theme, system, overrides);
    assert(resolved.used_system_font_metrics);
    assert(!resolved.used_system_font_scale);
    assert(!resolved.used_user_font_scale);
    assert(std::fabs(applied.body_font_size - 13.0f) < 0.001f);
    assert(std::fabs(applied.heading_font_size - 17.0f) < 0.001f);
    assert(std::fabs(applied.small_font_size - 11.0f) < 0.001f);
    overrides.apply_system_font_metrics = false;
    applied = apply_system_theme_preferences(theme, system, overrides);
    assert(std::fabs(applied.body_font_size - 17.5f) < 0.001f);

    overrides.font_family = "UserFont";
    overrides.body_font_size = 19.0f;
    overrides.small_font_size = 13.0f;
    overrides.apply_system_accent_color = true;
    applied = apply_system_theme_preferences(theme, system, overrides);
    resolved = resolve_system_theme_preferences(theme, system, overrides);
    assert(resolved.used_user_font_family);
    assert(resolved.used_user_font_size);
    assert(resolved.used_system_accent_color);
    assert(applied.default_font_family == "UserFont");
    assert(applied.body_font_size == 19.0f);
    assert(applied.small_font_size == 13.0f);
    assert((applied.accent == Color{88, 86, 214, 255}));
    assert(applied.state_focus_ring == applied.accent);
    assert(applied.state_active_bg == applied.accent_strong);

    overrides = ThemePreferenceOverrides{};
    overrides.prefer_system_color_scheme = true;
    applied = apply_system_theme_preferences(theme, system, overrides);
    resolved = resolve_system_theme_preferences(theme, system, overrides);
    assert(resolved.used_system_color_scheme);
    assert(resolved.effective_color_scheme == "dark");
    assert((applied.background == Color{28, 28, 30, 255}));
    assert((applied.foreground == Color{242, 242, 247, 255}));
    assert((applied.surface == Color{44, 44, 46, 238}));

    overrides.prefer_system_color_scheme = false;
    overrides.color_scheme = "light";
    applied = apply_system_theme_preferences(theme, system, overrides);
    assert(applied.background == theme.background);
    overrides.color_scheme = "high-contrast-dark";
    applied = apply_system_theme_preferences(theme, system, overrides);
    resolved = resolve_system_theme_preferences(theme, system, overrides);
    assert(resolved.used_user_color_scheme);
    assert(resolved.effective_color_scheme == "high-contrast-dark");
    assert((applied.background == Color{28, 28, 30, 255}));

    std::puts("PASS: system theme preferences are pure overlays");
}

void test_sized_box_in_row() {
    detail::g_app().arena.reset();

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
    detail::g_app().arena.reset();

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
    detail::g_app().arena.reset();

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
    detail::g_app().arena.reset();

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
    detail::g_app().arena.reset();

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

    auto line_height = cell.font_size * detail::g_app().theme.line_height_ratio;
    auto expected_y = (cell.height - line_height) / 2.0f;
    assert(text_y > 0.0f);
    assert(std::fabs(text_y - expected_y) < 0.01f);

    std::puts("PASS: grid cell text is vertically centered");
}

void test_canvas_widget_invokes_painter() {
    detail::g_app().arena.reset();

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

void test_semantic_canvas_exposes_debug_label() {
    detail::g_app().arena.reset();

    auto root_h = detail::alloc_node();
    auto& root = detail::node_at(root_h);
    root.style.flex_direction = FlexDirection::Column;

    Scope scope(root_h);
    Scope::set_current(&scope);
    int paint_calls = 0;
    widget::semantic_canvas(
        180.0f,
        20.0f,
        "Operation: file_create ok - Action Note.txt",
        [&paint_calls](Painter& p) {
            ++paint_calls;
            p.text(0.0f,
                   0.0f,
                   "Operation: file...",
                   18u,
                   14.0f,
                   Color{99, 99, 102, 255});
        },
        {},
        0x5101u);
    Scope::set_current(nullptr);

    LAYOUT_NODE(root_h, 400.0f);

    assert(root.children.size() == 1);
    auto& canvas = detail::node_at(root.children[0]);
    assert(canvas.width == 180.0f);
    assert(canvas.height == 20.0f);
    assert(canvas.paint_fn);
    assert(canvas.paint_token == 0x5101u);
    assert(canvas.text.empty());
    assert(canvas.debug_semantic_role == "text");
    assert(canvas.debug_semantic_label
           == "Operation: file_create ok - Action Note.txt");
    assert(canvas.debug_semantic_hidden == false);

    CMD_LEN = 0;
    PAINT_NODE(root_h, 0, 0, 0, 600.0f);
    assert(paint_calls == 1);

    std::puts("PASS: widget::semantic_canvas preserves debug label");
}

void test_canvas_linear_gradient_rect_emits_command() {
    detail::g_app().arena.reset();

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
            std::memcpy(detail::g_app().prev_cmd_buf, CMD_BUF, CMD_LEN);
            detail::g_app().prev_cmd_len = CMD_LEN;
        }
        return root_h;
    };

    detail::g_app().arena.reset();
    detail::g_app().prev_arena.reset();
    detail::g_app().callbacks.clear();
    detail::g_app().prev_cmd_len = 0;

    int old_calls = 0;
    auto old_root = make_canvas_tree(old_calls, Color{255, 0, 0, 255}, true);
    assert(old_calls == 1);
    assert(detail::node_at(old_root).paint_dynamic);
    assert(!detail::node_at(old_root).paint_valid);

    detail::g_app().prev_root = old_root;
    std::swap(detail::g_app().arena, detail::g_app().prev_arena);
    detail::g_app().arena.reset();
    detail::g_app().callbacks.clear();

    int new_calls = 0;
    auto new_root = make_canvas_tree(new_calls, Color{0, 0, 255, 255}, false);
    auto matched = detail::diff_and_copy_layout(
        detail::g_app().prev_root,
        new_root,
        detail::g_app().prev_arena,
        detail::g_app().arena);
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
            std::memcpy(detail::g_app().prev_cmd_buf, CMD_BUF, CMD_LEN);
            detail::g_app().prev_cmd_len = CMD_LEN;
        }
        return root_h;
    };

    detail::g_app().arena.reset();
    detail::g_app().prev_arena.reset();
    detail::g_app().callbacks.clear();
    detail::g_app().prev_cmd_len = 0;
    detail::g_app().paint_invalidation_mask = 0;
    detail::g_app().prev_scroll_x = 0;
    detail::g_app().prev_scroll_y = 0;
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

    detail::g_app().prev_root = old_root;
    std::swap(detail::g_app().arena, detail::g_app().prev_arena);
    detail::g_app().arena.reset();
    detail::g_app().callbacks.clear();

    int new_calls = 0;
    // Same token, but a different colour the painter would emit if
    // re-invoked. The blit must reuse prev_cmd_buf bytes (the red
    // line), so new_calls must stay at 0 and the bytes must match.
    auto new_root = make_canvas_tree(new_calls,
        Color{0, 0, 255, 255}, kToken, false);
    auto matched = detail::diff_and_copy_layout(
        detail::g_app().prev_root, new_root,
        detail::g_app().prev_arena, detail::g_app().arena);
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

void test_theme_change_invalidates_token_stable_canvas_paint() {
    auto make_canvas_tree = [](int& paint_calls,
                               std::uint64_t token,
                               bool paint) {
        auto root_h = detail::alloc_node();
        detail::node_at(root_h).style.flex_direction = FlexDirection::Column;

        Scope scope(root_h);
        Scope::set_current(&scope);
        widget::canvas(200.0f, 100.0f,
            [&paint_calls](Painter& p) {
                ++paint_calls;
                auto ink = current_theme().foreground;
                p.line(10.0f, 20.0f, 60.0f, 70.0f, 1.0f, ink);
            },
            {},
            token);
        Scope::set_current(nullptr);

        if (paint) {
            LAYOUT_NODE(root_h, 400.0f);
            CMD_LEN = 0;
            PAINT_NODE(root_h, 0, 0, 0, 600.0f);
            std::memcpy(detail::g_app().prev_cmd_buf, CMD_BUF, CMD_LEN);
            detail::g_app().prev_cmd_len = CMD_LEN;
        }
        return root_h;
    };

    set_theme(Theme{});
    detail::g_app().arena.reset();
    detail::g_app().prev_arena.reset();
    detail::g_app().callbacks.clear();
    detail::g_app().prev_cmd_len = 0;
    detail::g_app().paint_invalidation_mask = 0;
    detail::g_app().prev_scroll_x = 0;
    detail::g_app().prev_scroll_y = 0;
    metrics::reset_all();

    constexpr std::uint64_t kToken = 0x1234'ABCD'5678'EF90ULL;

    int old_calls = 0;
    auto old_root = make_canvas_tree(old_calls, kToken, true);
    assert(old_calls == 1);
    auto const old_generation = detail::g_app().theme_generation;
    auto& old_canvas = detail::node_at(detail::node_at(old_root).children[0]);
    assert(old_canvas.paint_theme_generation == old_generation);
    assert(old_canvas.paint_valid);

    detail::g_app().prev_root = old_root;
    std::swap(detail::g_app().arena, detail::g_app().prev_arena);
    detail::g_app().arena.reset();
    detail::g_app().callbacks.clear();

    Theme dark = apply_dark_color_scheme(Theme{});
    set_theme(dark);
    assert(detail::g_app().theme_generation != old_generation);

    int new_calls = 0;
    auto new_root = make_canvas_tree(new_calls, kToken, false);
    auto matched = detail::diff_and_copy_layout(
        detail::g_app().prev_root, new_root,
        detail::g_app().prev_arena, detail::g_app().arena);
    assert(matched);

    auto& new_canvas = detail::node_at(detail::node_at(new_root).children[0]);
    assert(new_canvas.paint_token == kToken);
    assert(new_canvas.paint_token_prev == kToken);
    assert(new_canvas.paint_theme_generation == old_generation);

    auto blits_before = metrics::inst::paint_subtrees_blitted.total();
    LAYOUT_NODE(new_root, 400.0f);
    CMD_LEN = 0;
    PAINT_NODE(new_root, 0, 0, 0, 600.0f);
    auto blits_during =
        metrics::inst::paint_subtrees_blitted.total() - blits_before;

    assert(new_calls == 1);
    assert(blits_during == 0);
    assert(new_canvas.paint_theme_generation
           == detail::g_app().theme_generation);

    set_theme(Theme{});

    std::puts("PASS: theme changes invalidate token-stable canvas paint");
}

void test_material_foreground_palette_invalidates_diff_cache() {
    auto make_tree = [](Color foreground) {
        auto root_h = detail::alloc_node();
        auto& root = detail::node_at(root_h);
        root.style.flex_direction = FlexDirection::Column;

        auto child_h = detail::alloc_node();
        auto& child = detail::node_at(child_h);
        child.style.fixed_height = 48.0f;
        child.material.kind = MaterialKind::Regular;
        child.material.foreground = foreground;
        child.material.secondary_foreground = foreground;
        child.material.accent_foreground = foreground;
        child.material.strong_accent_foreground = foreground;
        root.children.push_back(child_h);
        return root_h;
    };

    detail::g_app().arena.reset();
    detail::g_app().prev_arena.reset();
    detail::g_app().prev_cmd_len = 0;

    auto old_root = make_tree(Color{20, 20, 20, 255});
    LAYOUT_NODE(old_root, 400.0f);
    detail::g_app().prev_root = old_root;
    std::swap(detail::g_app().arena, detail::g_app().prev_arena);
    detail::g_app().arena.reset();

    auto new_root = make_tree(Color{240, 240, 245, 255});
    auto matched = detail::diff_and_copy_layout(
        detail::g_app().prev_root, new_root,
        detail::g_app().prev_arena, detail::g_app().arena);

    assert(!matched);
    assert(!detail::node_at(new_root).layout_valid);

    std::puts("PASS: material foreground palette invalidates diff cache");
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
            std::memcpy(detail::g_app().prev_cmd_buf, CMD_BUF, CMD_LEN);
            detail::g_app().prev_cmd_len = CMD_LEN;
        }
        return root_h;
    };

    detail::g_app().arena.reset();
    detail::g_app().prev_arena.reset();
    detail::g_app().callbacks.clear();
    detail::g_app().prev_cmd_len = 0;
    detail::g_app().paint_invalidation_mask = 0;
    detail::g_app().prev_scroll_x = 0;
    detail::g_app().prev_scroll_y = 0;

    int old_calls = 0;
    auto old_root = make_canvas_tree(old_calls,
        Color{255, 0, 0, 255}, 0xAAAA'AAAA'AAAA'AAAAULL, true);
    assert(old_calls == 1);

    detail::g_app().prev_root = old_root;
    std::swap(detail::g_app().arena, detail::g_app().prev_arena);
    detail::g_app().arena.reset();
    detail::g_app().callbacks.clear();

    int new_calls = 0;
    // Different token — represents an upstream input change.
    auto new_root = make_canvas_tree(new_calls,
        Color{0, 0, 255, 255}, 0xBBBB'BBBB'BBBB'BBBBULL, false);
    auto matched = detail::diff_and_copy_layout(
        detail::g_app().prev_root, new_root,
        detail::g_app().prev_arena, detail::g_app().arena);
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
            std::memcpy(detail::g_app().prev_cmd_buf, CMD_BUF, CMD_LEN);
            detail::g_app().prev_cmd_len = CMD_LEN;
        }
        return root_h;
    };

    detail::g_app().arena.reset();
    detail::g_app().prev_arena.reset();
    detail::g_app().callbacks.clear();
    detail::g_app().prev_cmd_len = 0;
    detail::g_app().paint_invalidation_mask = 0;
    detail::g_app().prev_scroll_x = 0;
    detail::g_app().prev_scroll_y = 0;
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

    detail::g_app().prev_root = old_root;
    std::swap(detail::g_app().arena, detail::g_app().prev_arena);
    detail::g_app().arena.reset();
    detail::g_app().callbacks.clear();

    int new_calls = 0;
    auto new_root = make_tree(new_calls,
        Color{0, 0, 255, 255}, kToken, false);
    auto matched = detail::diff_and_copy_layout(
        detail::g_app().prev_root, new_root,
        detail::g_app().prev_arena, detail::g_app().arena);
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

void test_static_parent_self_paint_survives_child_hover_walk() {
    constexpr unsigned int kChildCallback = 17u;

    auto build_tree = []() {
        auto root_h = detail::alloc_node();
        detail::node_at(root_h).style.flex_direction = FlexDirection::Column;

        auto parent_h = detail::alloc_node();
        auto& parent = detail::node_at(parent_h);
        parent.style.flex_direction = FlexDirection::Column;
        parent.style.padding[0] = 20.0f;
        parent.style.padding[1] = 20.0f;
        parent.style.padding[2] = 20.0f;
        parent.style.padding[3] = 20.0f;
        parent.style.max_width = 360.0f;
        parent.style.fixed_height = 160.0f;
        parent.border_radius = 18.0f;
        parent.material = layout::plain_material_style(
            Color{255, 255, 255, 255},
            Color{210, 210, 216, 255},
            MaterialSurfaceRole::Content,
            "test-static-parent",
            "test-static-parent");

        auto child_h = detail::alloc_node();
        auto& child = detail::node_at(child_h);
        child.callback_id = kChildCallback;
        child.cursor_type = 1;
        child.text = "Applications";
        child.font_size = 16.0f;
        child.text_color = Color{28, 28, 30, 255};
        child.hover_background = Color{224, 224, 228, 255};
        child.style.fixed_height = 36.0f;
        child.style.padding[0] = 6.0f;
        child.style.padding[1] = 10.0f;
        child.style.padding[2] = 6.0f;
        child.style.padding[3] = 10.0f;
        child.border_radius = 8.0f;

        parent.children.push_back(child_h);
        detail::node_at(root_h).children.push_back(parent_h);
        return root_h;
    };

    auto mirror_cmd_to_prev = []() {
        auto len = CMD_LEN;
        if (len > AppState::PAINT_CACHE_BUF_SIZE)
            len = AppState::PAINT_CACHE_BUF_SIZE;
        std::memcpy(detail::g_app().prev_cmd_buf, CMD_BUF, len);
        detail::g_app().prev_cmd_len = len;
    };

    detail::g_app().arena.reset();
    detail::g_app().prev_arena.reset();
    detail::g_app().callbacks.clear();
    detail::g_app().callback_roles.clear();
    detail::g_app().hovered_id = 0xFFFFFFFFu;
    detail::g_app().prev_hovered_id = 0xFFFFFFFFu;
    detail::g_app().focused_id = 0xFFFFFFFFu;
    detail::g_app().prev_focused_id = 0xFFFFFFFFu;
    detail::g_app().focus_visible = false;
    detail::g_app().prev_focus_visible = false;
    detail::g_app().pressed_id = 0xFFFFFFFFu;
    detail::g_app().prev_pressed_id = 0xFFFFFFFFu;
    detail::g_app().paint_invalidation_mask = 0;
    detail::g_app().prev_scroll_x = 0.0f;
    detail::g_app().prev_scroll_y = 0.0f;
    detail::clear_pointer_position();
    metrics::reset_all();

    auto root_0 = build_tree();
    LAYOUT_NODE(root_0, 480.0f);
    CMD_LEN = 0;
    PAINT_NODE(root_0, 0, 0, 0, 320.0f);
    auto const parent_0_h = detail::node_at(root_0).children[0];
    assert(detail::node_at(parent_0_h).self_paint_valid);
    mirror_cmd_to_prev();
    detail::g_app().prev_root = root_0;
    detail::persist_paint_inputs(detail::g_app());

    std::swap(detail::g_app().arena, detail::g_app().prev_arena);
    detail::g_app().arena.reset();
    detail::g_app().callbacks.clear();
    detail::g_app().callback_roles.clear();

    auto root_1 = build_tree();
    auto matched = detail::diff_and_copy_layout(
        detail::g_app().prev_root,
        root_1,
        detail::g_app().prev_arena,
        detail::g_app().arena);
    assert(matched);

    detail::g_app().hovered_id = kChildCallback;
    detail::g_app().paint_invalidation_mask =
        detail::compute_paint_invalidation_mask(detail::g_app());
    auto const self_blits_before =
        metrics::inst::paint_self_prefixes_blitted.total();
    LAYOUT_NODE(root_1, 480.0f);
    CMD_LEN = 0;
    PAINT_NODE(root_1, 0, 0, 0, 320.0f);
    auto const self_blits_during =
        metrics::inst::paint_self_prefixes_blitted.total()
        - self_blits_before;

    auto const parent_1_h = detail::node_at(root_1).children[0];
    assert(detail::node_at(parent_1_h).layout_valid);
    assert(detail::node_at(parent_1_h).paint_valid);
    assert(detail::node_at(parent_1_h).self_paint_valid);
    assert(self_blits_during >= 1);

    detail::g_app().hovered_id = 0xFFFFFFFFu;
    detail::persist_paint_inputs(detail::g_app());

    std::puts("PASS: static parent self-paint survives child hover walk");
}

// Regression: after diff_and_copy_layout marks a subtree layout_valid,
// re-running layout with a different available_width must NOT short-
// circuit on the cached width. Otherwise window resize leaves the tree
// at the previous-frame width and content collapses to one corner.
void test_layout_relayout_when_available_width_changes() {
    auto build_tree = [](float canvas_w) {
        detail::g_app().arena.reset();
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

    detail::g_app().arena.reset();
    detail::g_app().prev_arena.reset();
    detail::g_app().callbacks.clear();

    // Frame 1 at narrow width.
    auto old_root = build_tree(400.0f);
    assert(detail::node_at(old_root).width == 400.0f);

    // Hand the tree off as the previous frame.
    detail::g_app().prev_root = old_root;
    std::swap(detail::g_app().arena, detail::g_app().prev_arena);
    detail::g_app().arena.reset();
    detail::g_app().callbacks.clear();

    // Frame 2 at wider width — diff/copy will mark layout_valid.
    auto new_root = build_tree(400.0f);  // builds a structurally-identical tree
    auto matched = detail::diff_and_copy_layout(
        detail::g_app().prev_root, new_root,
        detail::g_app().prev_arena, detail::g_app().arena);
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

    detail::g_app().arena.reset();
    detail::g_app().callbacks.clear();
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
            assert(box.background.r == detail::g_app().theme.accent.r);
            assert(box.background.g == detail::g_app().theme.accent.g);
            assert(box.background.b == detail::g_app().theme.accent.b);
            assert(box.background.a == 255);
            assert(box.decoration == expected_decoration);
        } else {
            assert(box.background.r == 255);
            assert(box.background.g == 255);
            assert(box.background.b == 255);
            assert(box.border_color.r == detail::g_app().theme.border.r);
            assert(box.decoration == Decoration::None);
        }

        auto& lbl = detail::node_at(row.children[1]);
        assert(!lbl.text.empty());
        assert(lbl.callback_id == 0xFFFFFFFF);
        assert(lbl.cursor_type == 0);
        assert(lbl.focusable == false);
    };

    check_widget(root.children[0], false, detail::g_app().theme.radius_xs,
                 Decoration::Check, InteractionRole::Checkbox);
    check_widget(root.children[1], true,  detail::g_app().theme.radius_xs,
                 Decoration::Check, InteractionRole::Checkbox);
    check_widget(root.children[2], false, detail::g_app().theme.radius_lg,
                 Decoration::Dot,   InteractionRole::Radio);
    check_widget(root.children[3], true,  detail::g_app().theme.radius_lg,
                 Decoration::Dot,   InteractionRole::Radio);

    auto cb_id_a = detail::node_at(root.children[0]).callback_id;
    detail::g_app().callbacks[cb_id_a]();
    auto msgs = detail::drain<Msg>();
    assert(msgs.size() == 1);
    assert(std::holds_alternative<ToggleA>(msgs[0]));

    auto cb_id_b = detail::node_at(root.children[3]).callback_id;
    detail::g_app().callbacks[cb_id_b]();
    auto msgs2 = detail::drain<Msg>();
    assert(msgs2.size() == 1);
    assert(std::holds_alternative<PickB>(msgs2[0]));
    assert(std::get<PickB>(msgs2[0]).idx == 1);

    CMD_LEN = 0;
    detail::g_app().focusable_ids.clear();
    detail::collect_focusable_ids(root_h);
    PAINT_NODE(root_h, 0, 0, 0, 600.0f);
    int hit_regions = 0;
    for (unsigned int i = 0; i + 4 <= CMD_LEN; i += 4) {
        unsigned int word;
        std::memcpy(&word, &CMD_BUF[i], 4);
        if (word == static_cast<unsigned int>(Cmd::HitRegion)) ++hit_regions;
    }
    assert(hit_regions == 4);
    assert(detail::g_app().focusable_ids.size() == 4);

    // theme.toggle_box_size drives the box visual size. Override to a
    // touch-friendly 44 dp and confirm the laid-out box picks it up.
    float saved_size = detail::g_app().theme.toggle_box_size;
    detail::g_app().theme.toggle_box_size = 44.0f;
    detail::g_app().arena.reset();
    detail::g_app().callbacks.clear();
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
    detail::g_app().theme.toggle_box_size = saved_size;

    std::puts("PASS: checkbox + radio widgets");
}

void test_frame_skip_on_identical_cmd_buffer() {
    detail::g_app().arena.reset();
    detail::g_app().prev_arena.reset();
    metrics::reset_all();
    detail::clear_measure_cache();
    detail::invalidate_active_render_surface_paint_cache();
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

    detail::g_app().arena.reset();
    detail::g_app().prev_arena.reset();
    detail::g_app().callbacks.clear();
    CMD_LEN = 0;
    auto old_root = make_radio_tree(true, true);

    detail::g_app().prev_root = old_root;
    std::swap(detail::g_app().arena, detail::g_app().prev_arena);
    detail::g_app().arena.reset();
    detail::g_app().callbacks.clear();
    CMD_LEN = 0;
    auto new_root = make_radio_tree(false, false);

    auto matched = detail::diff_and_copy_layout(
        detail::g_app().prev_root,
        new_root,
        detail::g_app().prev_arena,
        detail::g_app().arena);

    assert(!matched);
    auto const& radio_row = detail::node_at(detail::node_at(new_root).children[0]);
    auto const& radio_box = detail::node_at(radio_row.children[0]);
    assert(radio_box.decoration == Decoration::None);
    assert(!radio_box.layout_valid);
    assert(!radio_box.paint_valid);

    std::puts("PASS: paint-only prop changes invalidate diff/paint cache");
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
        std::memcpy(detail::g_app().prev_cmd_buf, CMD_BUF, len);
        detail::g_app().prev_cmd_len = len;
    };

    auto next_frame = []() {
        std::swap(detail::g_app().arena, detail::g_app().prev_arena);
        detail::g_app().arena.reset();
        detail::g_app().callbacks.clear();
        detail::msg_queue().clear();
    };

    detail::g_app().arena.reset();
    detail::g_app().prev_arena.reset();
    detail::g_app().callbacks.clear();
    detail::msg_queue().clear();
    detail::g_app().paint_invalidation_mask = 0;
    detail::g_app().prev_scroll_x = 0;
    detail::g_app().prev_scroll_y = 0;
    metrics::reset_all();

    // Frame 0: A selected, initial walk seeds the cache.
    auto root_0 = build_radios(true, false, false);
    LAYOUT_NODE(root_0, 400.0f);
    CMD_LEN = 0;
    PAINT_NODE(root_0, 0, 0, 0, 600.0f);
    mirror_cmd_to_prev();
    detail::g_app().prev_root = root_0;

    // Frame 1: click B. A and B flip; C unchanged so C.row blits as a
    // chunk and C's descendant offsets stay at frame-0 positions.
    next_frame();
    auto root_1 = build_radios(false, true, false);
    detail::diff_and_copy_layout(detail::g_app().prev_root, root_1,
                                 detail::g_app().prev_arena, detail::g_app().arena);
    LAYOUT_NODE(root_1, 400.0f);
    CMD_LEN = 0;
    PAINT_NODE(root_1, 0, 0, 0, 600.0f);
    mirror_cmd_to_prev();
    detail::g_app().prev_root = root_1;

    // Frame 2: click A. C.row blits again. prev_cmd_buf is rewritten
    // each frame, so C.label's frame-0 offset now points to whatever
    // bytes happen to occupy that position in frame 2's emit.
    next_frame();
    auto root_2 = build_radios(true, false, false);
    detail::diff_and_copy_layout(detail::g_app().prev_root, root_2,
                                 detail::g_app().prev_arena, detail::g_app().arena);
    LAYOUT_NODE(root_2, 400.0f);
    CMD_LEN = 0;
    PAINT_NODE(root_2, 0, 0, 0, 600.0f);
    mirror_cmd_to_prev();
    detail::g_app().prev_root = root_2;

    // Frame 3: click C. C.box decoration None→Dot fails diff and
    // cascades C.row to walk. C.label.diff still succeeds (text
    // unchanged) — its blit guard would fire with the stale frame-0
    // offset without the descendant invalidation.
    next_frame();
    auto root_3 = build_radios(false, false, true);
    detail::diff_and_copy_layout(detail::g_app().prev_root, root_3,
                                 detail::g_app().prev_arena, detail::g_app().arena);
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
    detail::g_app().arena.reset();
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
    original.scroll_bar_visibility = "always";

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
    assert(parsed->scroll_bar_visibility == "always");
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
    assert(parsed->scroll_bar_visibility == defaults.scroll_bar_visibility);
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

void test_column_props_default_and_override() {
    auto const& t = detail::g_app().theme;

    // Default builder overload — gap=Md, cross=Start, main=Start.
    {
        detail::g_app().arena.reset();
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
        detail::g_app().arena.reset();
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
    auto const& t = detail::g_app().theme;

    // Default builder overload — gap=Sm, cross=Center, main=Start.
    {
        detail::g_app().arena.reset();
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
        detail::g_app().arena.reset();
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
    detail::g_app().arena.reset();

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
    assert(phenotype::icon_catalog::style_name() == "google_material_symbols_outlined_svg");
    assert(phenotype::icon_catalog::all_symbol_count == icons::all_symbol_count);
    assert(phenotype::icon_catalog::file_type_symbol_count
           == icons::file_type_symbol_count);
    assert(phenotype::icon_catalog::svg_subset_policy()
           == "bounded_material_symbols_svg_subset");
    assert(phenotype::icon_catalog::svg_supported_path_commands().find("C S Z")
           != std::string_view::npos);
    assert(phenotype::icon_catalog::svg_supported_style_attributes()
               .find("stroke-linecap")
           != std::string_view::npos);
    assert(phenotype::icon_catalog::stroke_geometry_policy()
           == "material_symbols_filled_path_geometry");
    assert(phenotype::icon_catalog::interface_metaphor_policy()
           == "familiar_simplified_macos_symbol_metaphors");
    assert(phenotype::icon_catalog::toolbar_symbol_chrome_policy()
           == "borderless_toolbar_symbols_inside_grouped_controls");
    assert(phenotype::icon_catalog::symbol_control_chrome_policy()
           == "macos_finder_symbol_state_chrome");
    assert(phenotype::icon_catalog::default_weight_policy()
           == "regular_material_symbols_weight_aligned");
    assert(phenotype::icon_catalog::rendering_capability_policy().find(
               "material_symbols_monochrome")
           != std::string_view::npos);
    assert(phenotype::icon_catalog::monochrome_symbol_count
           == phenotype::icon_catalog::all_symbol_count);
    assert(phenotype::icon_catalog::regular_weight_symbol_count
           == phenotype::icon_catalog::all_symbol_count);
    assert(phenotype::icon_catalog::palette_symbol_count == 0);
    assert(phenotype::icon_catalog::multicolor_symbol_count == 0);
    assert(phenotype::icon_catalog::phenotype_owned_symbol_count == 0);
    assert(phenotype::icon_catalog::permissive_source_symbol_count
           == phenotype::icon_catalog::all_symbol_count);
    assert(phenotype::icon_catalog::material_symbols_source_symbol_count
           == phenotype::icon_catalog::all_symbol_count);
    assert(phenotype::icon_catalog::material_symbols_unique_source_icon_count == 39);
    assert(phenotype::icon_catalog::material_symbols_style_count == 3);
    assert(phenotype::icon_catalog::default_material_symbols_style()
           == phenotype::icon_catalog::MaterialSymbolsStyle::Outlined);
    assert(phenotype::icon_catalog::style_name(
               phenotype::icon_catalog::MaterialSymbolsStyle::Rounded)
           == "google_material_symbols_rounded_svg");
    assert(phenotype::icon_catalog::apple_asset_symbol_count == 0);
    assert(phenotype::icon_catalog::svg_path_arc_symbol_count == 0);
    assert(phenotype::icon_catalog::round_stroke_symbol_count == 0);
    assert(phenotype::icon_catalog::semantic_reference_name(
               phenotype::icon_catalog::Symbol::AirDrop)
           == "airdrop");
    assert(!phenotype::icon_catalog::uses_svg_path_arcs(
               phenotype::icon_catalog::Symbol::AirDrop));
    assert(phenotype::icon_catalog::uses_material_symbols_source(
               phenotype::icon_catalog::Symbol::Folder));
    assert(phenotype::icon_catalog::source_attribution(
               phenotype::icon_catalog::Symbol::Folder)
               .license
           == "Apache-2.0");
    assert(phenotype::icon_catalog::source_attribution(
               phenotype::icon_catalog::Symbol::Search)
               .license
           == "Apache-2.0");
    assert(phenotype::icon_catalog::source_attribution(
               phenotype::icon_catalog::Symbol::Search,
               phenotype::icon_catalog::MaterialSymbolsStyle::Sharp)
               .style
           == "sharp");
    assert(phenotype::icon_catalog::source_attribution(
               phenotype::icon_catalog::Symbol::Search)
               .source_url
               .find(phenotype::icon_catalog::material_symbols_source_revision())
           != std::string_view::npos);
    auto const capabilities = phenotype::icon_catalog::rendering_capabilities(
        phenotype::icon_catalog::Symbol::Recents);
    assert(capabilities.monochrome);
    assert(!capabilities.hierarchical);
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
    assert(phenotype::icon_catalog::file_type_symbol_at(10)
           == phenotype::icon_catalog::Symbol::PresentationDocument);
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
    assert(sidebar_chrome.background_color.a == 150);
    assert(sidebar_chrome.hover_background_color.a == 176);

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
    test_scene_runtime_isolates_app_state_and_messages();
    test_message_queue_is_derived_from_active_scene();
    test_app_state_is_derived_from_active_scene();
    test_scene_scheduler_clock_is_scene_local();
    test_render_surface_runtime_binds_scene_and_tracks_frames();
    test_render_surface_visibility_updates_bound_scene();
    test_application_runtime_snapshot_tracks_process_owners();
    test_active_runtime_binding_restores_scene_and_surface();
    test_app_runner_uses_context_pointer();
    test_app_runner_can_own_context_pointer();
    test_legacy_app_runner_context_is_scene_local();
    test_material_build_context_is_scene_local();
    test_view_build_scope_is_scene_local();
    test_runtime_run_scene_keeps_same_types_scene_local();
    test_runtime_run_scene_can_share_external_state();
    test_runtime_scene_runner_targets_scene();
    test_scene_runner_survives_app_state_swap();
    test_system_theme_preferences_are_pure_overlays();
    test_sized_box_in_row();
    test_image_widget_layout_and_emit();
    test_svg_image_widget_uses_stable_paint_token();
    test_svg_image_widget_options_contract();
    test_grid_cell_text_is_vertically_centered();
    test_canvas_widget_invokes_painter();
    test_semantic_canvas_exposes_debug_label();
    test_canvas_linear_gradient_rect_emits_command();
    test_canvas_bypasses_paint_cache_after_diff();
    test_canvas_paint_token_hit_skips_paint_fn();
    test_theme_change_invalidates_token_stable_canvas_paint();
    test_material_foreground_palette_invalidates_diff_cache();
    test_canvas_paint_token_miss_invokes_paint_fn();
    test_canvas_paint_token_lets_ancestor_blit();
    test_static_parent_self_paint_survives_child_hover_walk();
    test_layout_relayout_when_available_width_changes();
    test_checkbox_and_radio_widgets();
    test_frame_skip_on_identical_cmd_buffer();
    test_paint_only_props_invalidate_diff_cache();
    test_radio_paint_cache_stale_descendant_after_subtree_blit();
    test_row_cross_align_center_default();
    test_theme_json_roundtrip();
    test_theme_json_partial_keeps_defaults();
    test_theme_json_color_string_forms();
    test_theme_json_color_object_back_compat();
    test_theme_json_mixed_overlay();
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
