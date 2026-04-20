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
struct ToggleAgreed {};
struct SetChoice { int value; };

using Msg = std::variant<
    Increment,
    Decrement,
    NameChanged,
    ImeChanged,
    ToggleAgreed,
    SetChoice>;

// ---- State ----

struct State {
    int count = 0;
    std::string name;
    std::string ime_sample;
    bool agreed = false;
    int choice = 0;
};

// ---- Update ----

void update(State& state, Msg msg) {
    std::visit([&](auto const& m) {
        using T = std::decay_t<decltype(m)>;
        if constexpr (std::same_as<T, Increment>)      state.count += 1;
        else if constexpr (std::same_as<T, Decrement>)  state.count -= 1;
        else if constexpr (std::same_as<T, NameChanged>) state.name = m.text;
        else if constexpr (std::same_as<T, ImeChanged>) state.ime_sample = m.text;
        else if constexpr (std::same_as<T, ToggleAgreed>) state.agreed = !state.agreed;
        else if constexpr (std::same_as<T, SetChoice>)  state.choice = m.value;
    }, msg);
}

// ---- View ----

static Msg on_name_changed(std::string s) { return NameChanged{std::move(s)}; }
static Msg on_ime_changed(std::string s) { return ImeChanged{std::move(s)}; }

static constexpr char kLocalImageAsset[] = "showcase-local.bmp";
static constexpr char kRemoteImageAsset[] = "showcase.bmp";

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
    block += "\nhovered_id: ";
    block += debug_id_text(debug.hovered_id);
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
            widget::checkbox<Msg>("I agree to the terms", state.agreed, ToggleAgreed{});
            layout::spacer(6);
            widget::radio<Msg>("Option A", state.choice == 0, SetChoice{0});
            widget::radio<Msg>("Option B", state.choice == 1, SetChoice{1});
            widget::radio<Msg>("Option C", state.choice == 2, SetChoice{2});
            layout::spacer(10);
            widget::text(std::string("Agreement: ") + (state.agreed ? "enabled" : "disabled"));
            layout::spacer(4);
            widget::text(std::string("Selected plan: ") + selected_choice_label(state.choice));
        });

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

// ---- Theme ----

static phenotype::Theme showcase_theme() {
    return phenotype::Theme{
        .background = {244, 241, 234, 255},
        .foreground = {25, 32, 45, 255},
        .accent = {16, 110, 115, 255},
        .muted = {96, 104, 118, 255},
        .border = {212, 217, 223, 255},
        .code_bg = {233, 238, 240, 255},
        .hero_bg = {26, 66, 78, 255},
        .hero_fg = {244, 247, 244, 255},
        .hero_muted = {181, 202, 198, 255},
        .transparent = {0, 0, 0, 0},
        .body_font_size = 16.0f,
        .heading_font_size = 23.0f,
        .hero_title_size = 40.0f,
        .hero_subtitle_size = 18.0f,
        .code_font_size = 14.0f,
        .small_font_size = 14.0f,
        .line_height_ratio = 1.6f,
        .max_content_width = 760.0f,
    };
}

// ---- Entry point ----

int main() {
    phenotype::set_theme(showcase_theme());
    return phenotype::native::run_app<State, Msg>(480, 720, "phenotype", view, update);
}
