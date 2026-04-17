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

static std::string local_image_url() {
    return std::string(PHENOTYPE_EXAMPLE_ASSET_ROOT) + "/showcase.bmp";
}

static std::string remote_image_url() {
    return "https://raw.githubusercontent.com/misut/phenotype/main/examples/native/assets/showcase.bmp";
}

static std::string selected_choice_label(int choice) {
    switch (choice) {
    case 1: return "Option B";
    case 2: return "Option C";
    default: return "Option A";
    }
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
            widget::text("The first field is a basic text input. The second one is reserved for native IME composition and platform-specific text system parity.");
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
            widget::text("Platform-specific status: caret, preedit text, and candidate overlays should feel attached to the focused field where that backend already supports native IME parity.");
            layout::spacer(8);
            widget::code("Walkthrough: tab into each field, type plain text, then try native IME composition and keep the field focused while scrolling.");
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
            widget::text("Images and Async Loading");
            layout::spacer(6);
            widget::text("The first image is a local asset. The second image exercises an async remote load and should remain visually safe even when the network path is unavailable.");
            layout::spacer(10);
            widget::text("Local file");
            layout::spacer(4);
            widget::image(local_image_url(), 320.0f, 180.0f);
            layout::spacer(10);
            widget::text("Remote HTTP");
            layout::spacer(4);
            widget::image(remote_image_url(), 320.0f, 180.0f);
            layout::spacer(8);
            widget::text("Expected state: show a placeholder first, replace it when the remote image succeeds, and keep the frame stable if it never arrives.");
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
            "If a backend is missing IME parity or remote image support, keep the note neutral and preserve the same showcase structure for future backend work.");
    },
        [] {
            widget::text("Manual Walkthrough");
            widget::text("Tab through the controls, type in both fields, verify the selection state, inspect both image slots, then scroll and resize the window.");
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
