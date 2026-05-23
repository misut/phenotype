#include <filesystem>
#include <string>
#include <variant>

import phenotype;
import phenotype.native;

#ifndef PHENOTYPE_EXAMPLE_ASSET_ROOT
#define PHENOTYPE_EXAMPLE_ASSET_ROOT "."
#endif

// ---- Messages ----

struct Increment {};
struct Decrement {};
struct NameChanged { std::string text; };
struct ImeChanged { std::string text; };
struct ValidationChanged { std::string text; };
struct ToggleAgreed {};
struct ToggleNotifications {};
struct ToggleDialog {};
struct SetChoice { int value; };
struct SelectTab { std::size_t value; };
struct Resized { int width; int height; float scale; };

using Msg = std::variant<
    Increment,
    Decrement,
    NameChanged,
    ImeChanged,
    ValidationChanged,
    ToggleAgreed,
    ToggleNotifications,
    ToggleDialog,
    SetChoice,
    SelectTab,
    Resized>;

// ---- State ----

struct State {
    int count = 0;
    std::string name;
    std::string ime_sample;
    std::string validation_sample = "Needs review";
    bool agreed = false;
    bool notifications = false;
    bool dialog_open = false;
    int choice = 0;
    std::size_t selected_tab = 0;
    int viewport_width = 0;
    int viewport_height = 0;
    float viewport_scale = 1.0f;
    bool size_limits_applied = false;
};

// ---- Update ----

void update(State& state, Msg msg) {
    std::visit([&](auto const& m) {
        using T = std::decay_t<decltype(m)>;
        if constexpr (std::same_as<T, Increment>)      state.count += 1;
        else if constexpr (std::same_as<T, Decrement>)  state.count -= 1;
        else if constexpr (std::same_as<T, NameChanged>) state.name = m.text;
        else if constexpr (std::same_as<T, ImeChanged>) state.ime_sample = m.text;
        else if constexpr (std::same_as<T, ValidationChanged>) state.validation_sample = m.text;
        else if constexpr (std::same_as<T, ToggleAgreed>) state.agreed = !state.agreed;
        else if constexpr (std::same_as<T, ToggleNotifications>) state.notifications = !state.notifications;
        else if constexpr (std::same_as<T, ToggleDialog>) state.dialog_open = !state.dialog_open;
        else if constexpr (std::same_as<T, SetChoice>)  state.choice = m.value;
        else if constexpr (std::same_as<T, SelectTab>)  state.selected_tab = m.value;
        else if constexpr (std::same_as<T, Resized>) {
            state.viewport_width = m.width;
            state.viewport_height = m.height;
            state.viewport_scale = m.scale;
            // Apply window-size constraints once the native window is up;
            // the first Resized message is the earliest safe moment.
            if (!state.size_limits_applied) {
                phenotype::native::set_window_size_limits(
                    320, 480,
                    phenotype::native::window_unbounded,
                    phenotype::native::window_unbounded);
                state.size_limits_applied = true;
            }
        }
    }, msg);
}

// ---- View ----

static Msg on_name_changed(std::string s) { return NameChanged{std::move(s)}; }
static Msg on_ime_changed(std::string s) { return ImeChanged{std::move(s)}; }
static Msg on_validation_changed(std::string s) { return ValidationChanged{std::move(s)}; }

static constexpr char kLocalImageAsset[] = "showcase-local.bmp";
static constexpr char kRemoteImageAsset[] = "showcase.bmp";
static constexpr char kInlineSvgImage[] =
    R"SVG(<svg viewBox="0 0 64 64" fill="none" stroke="currentColor" stroke-width="4" stroke-linecap="round" stroke-linejoin="round">
<rect x="10" y="18" width="44" height="32" rx="8"/>
<path d="M14 24 H28 L32 30 H50"/>
<path d="M18 36 H46"/>
<path d="M24 44 H40"/>
</svg>)SVG";

static std::string local_image_url() {
    namespace fs = std::filesystem;

    auto root = fs::path(PHENOTYPE_EXAMPLE_ASSET_ROOT);
    auto direct = root / kLocalImageAsset;
    if (fs::exists(direct))
        return direct.string();

    auto nested = root / "assets" / kLocalImageAsset;
    if (fs::exists(nested))
        return nested.string();

    return nested.string();
}

static std::string remote_image_url() {
    return std::string(
        "https://raw.githubusercontent.com/misut/phenotype/main/examples/native/assets/")
        + kRemoteImageAsset;
}

static std::string selected_choice_label(int choice) {
    switch (choice) {
    case 1: return "Option B";
    case 2: return "Option C";
    default: return "Option A";
    }
}

static std::string debug_id_text(unsigned int id) {
    if (id == phenotype::native::invalid_callback_id)
        return "none";
    return std::to_string(id);
}

static std::string debug_caret_text(unsigned int caret_pos) {
    if (caret_pos == phenotype::native::invalid_callback_id)
        return "none";
    return std::to_string(caret_pos);
}

static std::string debug_rect_text(phenotype::diag::RectSnapshot const& rect) {
    if (!rect.valid)
        return "(invalid)";
    std::string text = "(";
    text += std::to_string(rect.x);
    text += ", ";
    text += std::to_string(rect.y);
    text += ", ";
    text += std::to_string(rect.w);
    text += ", ";
    text += std::to_string(rect.h);
    text += ")";
    return text;
}

static std::string input_debug_block(phenotype::diag::InputDebugSnapshot const& debug) {
    auto platform_name = phenotype::native::current_platform().name;
    std::string block;
    block += "platform: ";
    block += platform_name ? platform_name : "unknown";
    block += "\nlast_event: ";
    block += debug.event.empty() ? "none" : debug.event;
    block += "\nsource: ";
    block += debug.source.empty() ? "none" : debug.source;
    block += "\ndetail: ";
    block += debug.detail.empty() ? "none" : debug.detail;
    block += "\nresult: ";
    block += debug.result.empty() ? "none" : debug.result;
    block += "\ncallback_id: ";
    block += debug_id_text(debug.callback_id);
    block += "\nrole: ";
    block += debug.role;
    block += "\nfocused_id: ";
    block += debug_id_text(debug.focused_id);
    block += "\nfocused_role: ";
    block += debug.focused_role;
    block += "\nfocus_visible: ";
    block += debug.focus_visible ? "true" : "false";
    block += "\nhovered_id: ";
    block += debug_id_text(debug.hovered_id);
    block += "\npressed_id: ";
    block += debug_id_text(debug.pressed_id);
    block += "\nscroll_y: ";
    block += std::to_string(debug.scroll_y);
    block += "\ntext caret: ";
    block += debug_caret_text(debug.caret_pos);
    block += "\nselection active: ";
    block += debug.selection_active ? "true" : "false";
    block += "\nselection start: ";
    block += debug_caret_text(debug.selection_start);
    block += "\nselection end: ";
    block += debug_caret_text(debug.selection_end);
    block += "\ncaret visible: ";
    block += debug.caret_visible ? "true" : "false";
    block += "\ncaret renderer: ";
    block += debug.caret_renderer.empty() ? "hidden" : debug.caret_renderer;
    block += "\ncaret geometry source: ";
    block += debug.caret_geometry_source.empty() ? "draw" : debug.caret_geometry_source;
    block += "\ncaret rect: ";
    block += debug_rect_text(debug.caret_rect);
    block += "\ncaret draw rect: ";
    block += debug_rect_text(debug.caret_draw_rect);
    block += "\ncaret host rect: ";
    block += debug_rect_text(debug.caret_host_rect);
    block += "\ncaret screen rect: ";
    block += debug_rect_text(debug.caret_screen_rect);
    block += "\ncaret host bounds: ";
    block += debug_rect_text(debug.caret_host_bounds);
    block += "\ncaret host flipped: ";
    block += debug.caret_host_flipped ? "true" : "false";
    block += "\ncomposition active: ";
    block += debug.composition_active ? "true" : "false";
    block += "\ncomposition cursor: ";
    block += std::to_string(debug.composition_cursor);
    block += "\ncomposition text: ";
    block += debug.composition_text.empty() ? "(empty)" : debug.composition_text;
    return block;
}

static void render_expectation_card(phenotype::str title, phenotype::str body) {
    using namespace phenotype;
    layout::card([&] {
        widget::text(title);
        layout::spacer(6);
        widget::text(body);
    });
}

static void render_paint_command_showcase() {
    using namespace phenotype;

    layout::card([&] {
        widget::text("Paint Command Showcase");
        layout::spacer(6);
        widget::text("This canvas intentionally touches every advanced Painter command so backend regressions can be spotted from one runnable scene.");
        layout::spacer(8);
        widget::canvas(320.0f, 210.0f, [](Painter& painter) {
            constexpr float pi = 3.1415926535f;
            Color ink{15, 23, 42, 255};
            Color blue{59, 130, 246, 220};
            Color cyan{6, 182, 212, 190};
            Color green{34, 197, 94, 210};
            Color amber{245, 158, 11, 215};
            Color rose{244, 63, 94, 190};
            Color purple{147, 51, 234, 190};
            Color grid{203, 213, 225, 130};

            painter.linear_gradient_rect(
                12.0f, 14.0f, 156.0f, 28.0f,
                Color{219, 234, 254, 255},
                Color{220, 252, 231, 255},
                GradientAxis::Horizontal,
                12);

            PaintQuad quads[] = {
                {18.0f, 64.0f, 72.0f, 54.0f, 84.0f, 98.0f, 26.0f, 108.0f, blue},
                {92.0f, 58.0f, 142.0f, 70.0f, 132.0f, 112.0f, 84.0f, 96.0f, cyan},
            };
            painter.fill_quads(quads, 2);

            painter.line(12.0f, 132.0f, 168.0f, 132.0f, 2.0f, ink);
            painter.arc(54.0f, 168.0f, 26.0f, 0.15f * pi, 1.75f * pi, 4.0f, amber);

            PathBuilder stroke_path;
            stroke_path.move_to(184.0f, 38.0f);
            stroke_path.cubic_to(220.0f, 2.0f, 270.0f, 78.0f, 304.0f, 38.0f);
            painter.stroke_path(stroke_path, 3.0f, rose);

            PathBuilder fill_path;
            fill_path.move_to(202.0f, 78.0f);
            fill_path.line_to(288.0f, 78.0f);
            fill_path.quad_to(306.0f, 122.0f, 244.0f, 134.0f);
            fill_path.line_to(190.0f, 112.0f);
            fill_path.close();
            painter.fill_path(fill_path, purple);

            painter.push_clip(188.0f, 150.0f, 104.0f, 40.0f);
            for (int i = 0; i < 7; ++i) {
                auto x = 170.0f + static_cast<float>(i) * 20.0f;
                painter.line(x, 148.0f, x + 56.0f, 194.0f, 3.0f, grid);
            }
            painter.text(198.0f, 162.0f, "clipped", 7, 14.0f, ink);
            painter.pop_clip();

            std::string footer = "Gradient / FillQuads / Line / Arc / Path / FillPath / Clip / Text";
            painter.text(12.0f, 188.0f,
                         footer.c_str(), static_cast<unsigned int>(footer.size()),
                         11.0f, ink);
        });
    });
}

void view(State const& state) {
    using namespace phenotype;
    auto input_debug = phenotype::diag::input_debug_snapshot();

    layout::scaffold(
        [] {
            widget::text("Native Widget Showcase");
            widget::text("A backend-neutral native demo for validating the shared UI contract across Metal, DirectX 12, and future Vulkan paths.");
        },
        [&] {
        layout::card([&] {
            widget::text("Overview");
            layout::spacer(8);
            widget::text("This scene keeps the content focused on shared widgets, layout, input, and image behavior instead of any one renderer implementation.");
            layout::spacer(8);
            layout::list_items([&] {
                layout::item("Shared contract: text, cards, buttons, links, focus, scrolling, resizing, and async image states should stay coherent.");
                layout::item("Platform extras can differ by backend, but the same scene should remain useful for manual walkthroughs everywhere.");
                layout::item("Known gaps should read as backend status notes, not as a reason to redesign the content around a single graphics API.");
            });
            layout::spacer(10);
            widget::code(
                "Shared goal:\n"
                "- One widget scene\n"
                "- Multiple native backends\n"
                "- Stable manual verification flow"
            );
        });

        layout::spacer(12);
        layout::card([&] {
            widget::text("Input Debug Panel");
            layout::spacer(6);
            widget::text("Use this panel to distinguish whether the shared shell received an input, ignored it, handled it, or let a platform-specific hook consume it.");
            layout::spacer(8);
            widget::code(input_debug_block(input_debug));
            layout::spacer(8);
            widget::text("Manual check: hover a control, click it, drag-select inside a field, try Cmd/Ctrl+A, type to replace the selection, then scroll with both wheel and keyboard and confirm the panel changes immediately.");
        });

        layout::spacer(12);
        layout::card([&] {
            widget::text("Core Widgets");
            layout::spacer(6);
            widget::text("This section covers first-frame text rendering, button interaction, inline code styling, link behavior, and state updates.");
            layout::spacer(6);
            widget::text(std::string("Count: ") + std::to_string(state.count));
            layout::spacer(6);
            layout::row(
                [&] { widget::button<Msg>("-", Decrement{}); },
                [&] { widget::button<Msg>("+", Increment{}); }
            );
            layout::spacer(8);
            widget::text("Use the buttons to confirm hover, focus, click dispatch, and immediate text refresh.");
            layout::spacer(8);
            widget::code(
                "widget::button<Msg>(\"-\", Decrement{});\n"
                "widget::button<Msg>(\"+\", Increment{});\n"
                "widget::link(\"Project page\", \"https://github.com/misut/phenotype\");"
            );
            layout::spacer(8);
            widget::link("Open phenotype on GitHub", "https://github.com/misut/phenotype");
        });

        layout::spacer(12);

        layout::card([&] {
            widget::text("Text Input and IME");
            layout::spacer(6);
            widget::text("The first field is a basic text input. The second one is reserved for native IME composition and platform-specific text system parity, including shared drag selection and select-all behavior.");
            layout::spacer(8);
            widget::text_field<Msg>("Enter your name...", state.name, on_name_changed);
            if (!state.name.empty()) {
                layout::spacer(6);
                widget::text(std::string("Hello, ") + state.name + "!");
            }
            layout::spacer(10);
            widget::text_field<Msg>("Try native IME composition here...", state.ime_sample, on_ime_changed);
            layout::spacer(6);
            widget::text(std::string("Committed text: ")
                + (state.ime_sample.empty() ? std::string("(empty)") : state.ime_sample));
            layout::spacer(8);
            widget::text("Platform-specific status: caret, live marked text, and candidate overlays should feel attached to the focused field where that backend already supports native IME parity.");
            layout::spacer(8);
            widget::code("Walkthrough: tab into each field, drag a partial selection, try Cmd/Ctrl+A, type plain text to replace the selection, then try native IME composition and keep the field focused while scrolling.");
        });

        layout::spacer(12);

        layout::card([&] {
            widget::text("Selection Controls");
            layout::spacer(6);
            widget::text("Checkboxes and radios help verify focus order, pointer interaction, and persistent selection state.");
            layout::spacer(8);
            auto indicator_glass = layout::glass_clear()
                .tint(Color{64, 156, 255, 90})
                .effect_union("native.showcase", "indicator.controls", 10.0f)
                .matched_geometry();
            widget::glass_checkbox<Msg>(
                "I agree to the terms",
                state.agreed,
                ToggleAgreed{},
                indicator_glass.effect_id("native.showcase", "agree"));
            layout::spacer(6);
            widget::glass_switch<Msg>(
                "Email notifications",
                state.notifications,
                ToggleNotifications{},
                indicator_glass.effect_id("native.showcase", "notifications"));
            layout::spacer(6);
            widget::glass_radio<Msg>(
                "Option A",
                state.choice == 0,
                SetChoice{0},
                indicator_glass.effect_id("native.showcase", "option-a"));
            widget::glass_radio<Msg>(
                "Option B",
                state.choice == 1,
                SetChoice{1},
                indicator_glass.effect_id("native.showcase", "option-b"));
            widget::glass_radio<Msg>(
                "Option C",
                state.choice == 2,
                SetChoice{2},
                indicator_glass.effect_id("native.showcase", "option-c"));
            layout::spacer(10);
            widget::text(std::string("Agreement: ") + (state.agreed ? "enabled" : "disabled"));
            layout::spacer(4);
            widget::text(std::string("Notifications: ") + (state.notifications ? "on" : "off"));
            layout::spacer(4);
            widget::text(std::string("Selected plan: ") + selected_choice_label(state.choice));
            layout::spacer(12);
            widget::text("Tabs (sliding indicator)");
            layout::spacer(6);
            {
                std::vector<phenotype::str> tab_items;
                tab_items.emplace_back("Overview", 8u);
                tab_items.emplace_back("Settings", 8u);
                tab_items.emplace_back("Activity", 8u);
                auto tab_options = TabsStyleOptions{};
                tab_options.indicator_glass_identity =
                    layout::glass_effect_identity(
                        "native.showcase",
                        "tabs.selection");
                widget::tabs<Msg>(
                    tab_items,
                    state.selected_tab,
                    [](std::size_t i) -> Msg { return SelectTab{i}; },
                    layout::glass_regular()
                        .tint(Color{64, 156, 255, 70})
                        .border(Color{64, 156, 255, 128})
                        .effect_union(
                            "native.showcase",
                            "tabs.segmented",
                            18.0f,
                            true,
                            true)
                        .effect_id("native.showcase", "tabs.pill")
                        .matched_geometry(),
                    tab_options);
            }
            layout::spacer(12);
            widget::text("Indeterminate progress");
            layout::spacer(6);
            widget::progress_indeterminate(280.0f);
        });

        layout::spacer(12);

        layout::card([&] {
            widget::text("Control States");
            layout::spacer(6);
            widget::text("Disabled, validation, and determinate progress states are included here so the compact native showcase covers more than the happy path.");
            layout::spacer(8);
            layout::row([&] {
                widget::button<Msg>("Primary action", Increment{}, ButtonVariant::Primary);
                widget::button<Msg>("Disabled action", Increment{}, ButtonVariant::Default, true);
            });
            layout::spacer(10);
            widget::text_field<Msg>("Validation error sample", state.validation_sample, on_validation_changed, true);
            layout::spacer(8);
            widget::text_field<Msg>("Disabled field", std::string("Locked value"), on_name_changed, false, true);
            layout::spacer(10);
            widget::text("Determinate progress");
            layout::spacer(6);
            widget::progress(state.agreed ? 0.72f : 0.35f, 280.0f);
            layout::spacer(6);
            widget::text(std::string("Progress source: ")
                + (state.agreed ? "agreement enabled" : "agreement disabled"));
        });

        layout::spacer(12);

        auto glass_group = layout::GlassEffectContainerOptions{};
        glass_group.container_id = 1201u;
        glass_group.spacing = 16.0f;
        glass_group.interactive = true;
        layout::glass_effect_container(glass_group, [&] {
            auto glass = layout::glass_effect_options(
                layout::glass_regular()
                    .tint(Color{64, 156, 255, 96})
                    .interactive()
                    .effect_id("native.showcase", "surface")
                    .matched_geometry());
            glass.semantic_label = "Glass Effect Surface";
            layout::glass_effect(glass, [&] {
                widget::text("Glass Effect Surface");
                layout::spacer(6);
                widget::text("Regular Liquid Glass now defaults to a capsule shape and can participate in a morphing effect container.");
                layout::spacer(8);
                layout::row([&] {
                    auto action_union = layout::glass_clear()
                        .tint(Color{64, 156, 255, 110})
                        .effect_union(
                            "native.showcase",
                            "action.row",
                            12.0f);
                    widget::glass_prominent_button<Msg>(
                        "Glass action",
                        Increment{},
                        action_union
                            .effect_id("native.showcase", "primary-action")
                            .matched_geometry());
                    widget::glass_button<Msg>(
                        "Secondary",
                        Decrement{},
                        action_union
                            .effect_id("native.showcase", "secondary-action")
                            .matched_geometry());
                    widget::glass_button<Msg>(
                        "More",
                        Increment{},
                        action_union
                            .effect_id("native.showcase", "more-action")
                            .matched_geometry());
                });
                layout::spacer(8);
                widget::glass_text_field<Msg>(
                    "Liquid search field",
                    state.name,
                    on_name_changed,
                    layout::glass_regular()
                        .tint(Color{64, 156, 255, 72})
                        .effect_union("native.showcase", "field.row", 18.0f)
                        .effect_id("native.showcase", "search-field")
                        .matched_geometry(),
                    GlassTextFieldStyleOptions{
                        .role = MaterialSurfaceRole::Toolbar,
                        .width = 280.0f,
                        .height = 34.0f,
                        .semantic_label = "Glass Search Field",
                    });
                layout::spacer(8);
                widget::code("layout::glass_effect_container(options, builder);");
            });
            layout::spacer(8);
            auto chip_glass = layout::glass_clear()
                .tint(Color{64, 156, 255, 88})
                .effect_union("native.showcase", "chip.pair", 16.0f);
            layout::row([&] {
                layout::glass_effect(chip_glass, [] {
                    widget::text("Unified");
                });
                layout::glass_effect(chip_glass, [] {
                    widget::text("Capsules");
                });
            });
        });

        layout::spacer(12);

        layout::card([&] {
            widget::text("Layout Primitives");
            layout::spacer(6);
            widget::text("This section exercises layout helpers that previously existed only in tests or larger product examples.");
            layout::spacer(10);
            widget::text("Weighted row and box");
            layout::spacer(6);
            layout::row([&] {
                layout::weighted(2.0f, [&] {
                    layout::box([&] {
                        widget::code("Weight 2\nexpanded track");
                    });
                });
                layout::weighted(1.0f, [&] {
                    layout::box([&] {
                        widget::code("Weight 1\nside track");
                    });
                });
            });
            layout::spacer(12);
            widget::text("Padded sized box");
            layout::spacer(6);
            layout::padded(SpaceToken::Sm, [&] {
                layout::sized_box(260.0f, [&] {
                    layout::box([&] {
                        widget::code("layout::padded -> sized_box -> box");
                    });
                });
            });
            layout::spacer(12);
            widget::text("Nested scroll view");
            layout::spacer(6);
            layout::scroll_view(112.0f, [&] {
                for (int i = 0; i < 8; ++i) {
                    widget::text(std::string("Scrollable row ") + std::to_string(i + 1));
                    layout::spacer(4);
                }
            }, SpaceToken::Sm);
            layout::spacer(12);
            layout::accordion("Accordion details", [&] {
                widget::text("This body is emitted only when the accordion is expanded.");
                layout::spacer(6);
                widget::code("layout::accordion(\"Accordion details\", builder);");
            });
            layout::spacer(12);
            auto dialog_label = state.dialog_open
                ? std::string("Close dialog overlay")
                : std::string("Open dialog overlay");
            widget::button<Msg>(dialog_label, ToggleDialog{});
        });

        if (state.dialog_open) {
            layout::dialog([&] {
                widget::text("Dialog Overlay");
                widget::text("This modal is rendered through layout::dialog on top of the main tree.");
                widget::button<Msg>("Close", ToggleDialog{}, ButtonVariant::Primary);
            }, 360.0f, 72);
        }

        layout::spacer(12);

        render_paint_command_showcase();

        layout::spacer(12);

        layout::card([&] {
            widget::text("Images and Backend Status");
            layout::spacer(6);
            widget::text("The first image is a dedicated local asset used for manual acceptance. The second image fetches a separate remote asset so the two paths stay independently testable.");
            layout::spacer(10);
            widget::text("Local file");
            layout::spacer(4);
            widget::image(local_image_url(), 320.0f, 180.0f);
            layout::spacer(10);
            widget::text("Remote HTTP");
            layout::spacer(4);
            widget::image(remote_image_url(), 320.0f, 180.0f);
            layout::spacer(10);
            widget::text("Inline SVG vector");
            layout::spacer(4);
            widget::svg_image(
                {kInlineSvgImage,
                 static_cast<unsigned int>(sizeof(kInlineSvgImage) - 1)},
                96.0f,
                96.0f,
                Color{0, 122, 255, 255});
            layout::spacer(8);
            widget::text("Expected state: show a placeholder first, replace it when the remote image succeeds, and keep the placeholder if the request never finishes.");
        });

        layout::spacer(12);

        layout::card([&] {
            widget::text("Layout, Scroll, and Resize");
            layout::spacer(6);
            widget::text("This page is intentionally longer than the initial window so scrolling, section spacing, and viewport updates are always part of the walkthrough.");
            layout::spacer(8);
            layout::list_items([&] {
                layout::item("Resize the window a few times and make sure cards, text blocks, and images keep their proportions.");
                layout::item("Scroll with an input focused and confirm that the page stays readable while interactive state remains intact.");
                layout::item("Treat backend differences as status notes, but keep the same content structure for every native renderer.");
            });
            layout::spacer(8);
            widget::code(
                "import phenotype;\n"
                "import phenotype.native;\n\n"
                "// This example is a native widget showcase.\n"
                "// It exercises shared widgets, layout, IME paths, images,\n"
                "// scrolling, and manual backend walkthroughs."
            );
        });

        layout::spacer(12);

        render_expectation_card(
            "Resize and DPI",
            "Cards, text, and images should adapt cleanly as the window changes size. Use this to spot layout drift, stale overlays, or scale mismatches.");
        layout::spacer(12);
        render_expectation_card(
            "Scroll Behavior",
            "Scroll from top to bottom and back again. The content should remain readable and interactive states should stay attached to the right controls.");
        layout::spacer(12);
        render_expectation_card(
            "Platform Status Notes",
            "Backend-specific notes can still describe IME differences, but every native renderer should keep the same showcase structure, including local and remote image slots.");
    },
        [] {
            widget::text("Manual Walkthrough");
            widget::text("Tab through the controls, type in both fields, drag a partial selection, try Cmd/Ctrl+A, verify replacement and backspace over the selection, inspect both image slots, then scroll and resize the window.");
        }
    );
}

// ---- Entry point ----

int main() {
    return phenotype::native::run_app<State, Msg>(
        480, 720, "phenotype",
        view, update,
        [](int w, int h, float scale) -> Msg {
            return Resized{w, h, scale};
        });
}
