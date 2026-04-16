#include <array>
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
            widget::text("phenotype native acceptance");
            widget::text("Windows DirectWrite + Direct3D 12 showcase for text, IME, images, scroll, and interaction.");
        },
        [&] {
        layout::card([&] {
            widget::text("What To Verify");
            layout::spacer(8);
            layout::list_items([&] {
                layout::item("Buttons, cards, code blocks, and links should render sharply on the first frame.");
                layout::item("The IME field should show composition text and candidate UI anchored to the focused input.");
                layout::item("The local image should load immediately and the remote image should replace its placeholder after download.");
                layout::item("Scroll, resize, minimize/restore, and alt-tab should not leave stale overlays or black frames.");
            });
            layout::spacer(10);
            widget::code("Tip: set PHENOTYPE_DX12_WARP=1 if you want a software-rendered smoke path on Windows.");
        });

        layout::spacer(12);
        layout::card([&] {
            widget::text("Counter");
            layout::spacer(6);
            widget::text(std::string("Count: ") + std::to_string(state.count));
            layout::spacer(6);
            layout::row(
                [&] { widget::button<Msg>("-", Decrement{}); },
                [&] { widget::button<Msg>("+", Increment{}); }
            );
            layout::spacer(8);
            widget::text("Use this card to verify button hover, focus, click dispatch, and text updates.");
        });

        layout::spacer(12);

        layout::card([&] {
            widget::text("Text Input + IME");
            layout::spacer(6);
            widget::text("The first field is a simple Latin input. The second one is for Korean/Japanese/Chinese IME composition.");
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
            widget::code("Expected behavior: preedit text, caret, and candidate overlay stay attached to the focused field while scrolling and resizing.");
        });

        layout::spacer(12);

        layout::card([&] {
            widget::text("Native DrawImage");
            layout::spacer(6);
            widget::text("The first image is loaded from the local repository. The second image uses the Windows HTTP path through cppx.");
            layout::spacer(10);
            widget::text("Local file");
            layout::spacer(4);
            widget::image(local_image_url(), 320.0f, 180.0f);
            layout::spacer(10);
            widget::text("Remote HTTP");
            layout::spacer(4);
            widget::image(remote_image_url(), 320.0f, 180.0f);
            layout::spacer(8);
            widget::text("A soft placeholder is expected until the remote image finishes downloading.");
        });

        layout::spacer(12);

        layout::card([&] {
            widget::text("Interaction");
            layout::spacer(6);
            widget::checkbox<Msg>("I agree to the terms", state.agreed, ToggleAgreed{});
            layout::spacer(6);
            widget::radio<Msg>("Option A", state.choice == 0, SetChoice{0});
            widget::radio<Msg>("Option B", state.choice == 1, SetChoice{1});
            widget::radio<Msg>("Option C", state.choice == 2, SetChoice{2});
            layout::spacer(10);
            widget::link("Open phenotype on GitHub", "https://github.com/misut/phenotype");
        });

        layout::spacer(12);

        layout::card([&] {
            widget::text("Render Notes");
            layout::spacer(6);
            widget::code(
                "import phenotype;\n"
                "import phenotype.native;\n\n"
                "// This example is the Windows native acceptance scene.\n"
                "// It exercises text, IME, images, scroll, link opening, and resize."
            );
        });

        layout::spacer(12);

        render_expectation_card(
            "Resize + DPI",
            "Resize the window a few times. Cards, text, and images should keep their proportions without flicker or stale candidate UI.");
        layout::spacer(12);
        render_expectation_card(
            "Scroll",
            "Scroll through the page while the IME field is focused. The composition underline and candidate panel should continue to track the input.");
        layout::spacer(12);
        render_expectation_card(
            "Image Fallbacks",
            "If the machine is offline, the remote image should stay on a neutral placeholder without affecting the rest of the frame.");
    },
        [] {
            widget::text("Windows acceptance checklist");
            widget::text("Tab through controls, type with IME, wait for the remote image, then resize and scroll.");
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
