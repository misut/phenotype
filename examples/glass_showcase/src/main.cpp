#include <concepts>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

import phenotype;
import phenotype.native;

struct ToggleBackdrop {};
struct ToggleInspector {};
struct SelectDensity { std::size_t value; };
struct NoteChanged { std::string text; };
struct Resized { int width; int height; float scale; };

using Msg = std::variant<
    ToggleBackdrop,
    ToggleInspector,
    SelectDensity,
    NoteChanged,
    Resized>;

struct State {
    bool high_contrast_backdrop = false;
    bool inspector_open = true;
    std::size_t selected_density = 1;
    std::string note = "Artifact-ready material contract";
    int viewport_width = 0;
    int viewport_height = 0;
    float viewport_scale = 1.0f;
};

static void update(State& state, Msg msg) {
    std::visit([&](auto const& m) {
        using T = std::decay_t<decltype(m)>;
        if constexpr (std::same_as<T, ToggleBackdrop>) {
            state.high_contrast_backdrop = !state.high_contrast_backdrop;
        } else if constexpr (std::same_as<T, ToggleInspector>) {
            state.inspector_open = !state.inspector_open;
        } else if constexpr (std::same_as<T, SelectDensity>) {
            state.selected_density = m.value;
        } else if constexpr (std::same_as<T, NoteChanged>) {
            state.note = m.text;
        } else if constexpr (std::same_as<T, Resized>) {
            state.viewport_width = m.width;
            state.viewport_height = m.height;
            state.viewport_scale = m.scale;
        }
    }, msg);
}

static Msg on_note_changed(std::string text) {
    return NoteChanged{std::move(text)};
}

static std::string density_label(std::size_t value) {
    switch (value) {
    case 0: return "Comfortable";
    case 2: return "Dense";
    default: return "Balanced";
    }
}

static std::string material_contract_text(
    phenotype::MaterialKind kind,
    char const* expected_contrast) {
    std::string text = "kind: ";
    text += phenotype::material_kind_name(kind);
    text += "\nfallback: translucent rounded rect available";
    text += "\nbackdrop blur: macOS sampled backdrop when available";
    text += "\ncontrast intent: ";
    text += expected_contrast;
    return text;
}

static void paint_backdrop(phenotype::Painter& painter, bool high_contrast) {
    using namespace phenotype;
    Color ink = high_contrast
        ? Color{15, 23, 42, 255}
        : Color{30, 41, 59, 255};
    Color muted = high_contrast
        ? Color{71, 85, 105, 255}
        : Color{100, 116, 139, 255};

    PaintRect base[] = {
        {0.0f, 0.0f, 360.0f, 52.0f, Color{239, 246, 255, 255}},
        {0.0f, 52.0f, 360.0f, 54.0f, Color{236, 253, 245, 255}},
        {0.0f, 106.0f, 360.0f, 54.0f, Color{255, 247, 237, 255}},
        {0.0f, 160.0f, 360.0f, 40.0f, Color{250, 245, 255, 255}},
    };
    painter.fill_rects(base, 4);

    PaintQuad planes[] = {
        {24.0f, 24.0f, 158.0f, 8.0f, 122.0f, 88.0f, 10.0f, 96.0f,
         Color{59, 130, 246, 210}},
        {184.0f, 14.0f, 334.0f, 42.0f, 296.0f, 110.0f, 154.0f, 78.0f,
         Color{20, 184, 166, 205}},
        {72.0f, 118.0f, 190.0f, 100.0f, 226.0f, 178.0f, 58.0f, 188.0f,
         Color{245, 158, 11, 205}},
        {224.0f, 122.0f, 340.0f, 116.0f, 330.0f, 188.0f, 202.0f, 170.0f,
         Color{244, 63, 94, 190}},
    };
    painter.fill_quads(planes, 4);

    for (int i = 0; i < 8; ++i) {
        float x = 18.0f + static_cast<float>(i) * 42.0f;
        painter.line(x, 0.0f, x + 34.0f, 200.0f, 1.0f,
                     high_contrast
                         ? Color{15, 23, 42, 90}
                         : Color{255, 255, 255, 115});
    }

    painter.push_clip(16.0f, 150.0f, 328.0f, 34.0f);
    for (int i = 0; i < 9; ++i) {
        float x = 20.0f + static_cast<float>(i) * 38.0f;
        painter.line(x, 148.0f, x + 22.0f, 190.0f, 2.0f, Color{255, 255, 255, 150});
    }
    painter.pop_clip();

    auto label = high_contrast
        ? std::string("high-contrast backdrop")
        : std::string("deterministic backdrop");
    painter.text(18.0f, 18.0f, label.c_str(),
                 static_cast<unsigned int>(label.size()), 16.0f, ink);
    std::string probe_text =
        "future pixel probes should target these stable regions";
    painter.text(18.0f, 44.0f,
                 probe_text.c_str(),
                 static_cast<unsigned int>(probe_text.size()),
                 12.0f, muted);
}

static void material_panel(
    phenotype::MaterialKind kind,
    phenotype::str title,
    phenotype::str description,
    char const* contrast_intent) {
    using namespace phenotype;
    layout::material_surface(kind, [&] {
        widget::text(title);
        layout::spacer(4);
        widget::text(description);
        layout::spacer(8);
        widget::code(material_contract_text(kind, contrast_intent));
    });
}

static void view(State const& state) {
    using namespace phenotype;
    auto input_debug = phenotype::diag::input_debug_snapshot();

    layout::scaffold(
        [] {
            widget::text("Glass Material Showcase");
            widget::text("A deterministic native scene for material semantics, artifact capture, and pixel-region checks.");
        },
        [&] {
            layout::card([&] {
                widget::text("Backdrop Test Pattern");
                layout::spacer(6);
                widget::text("The canvas below is intentionally stable so later glass blur checks can probe exact frame regions instead of relying on visual guessing.");
                layout::spacer(10);
                widget::canvas(360.0f, 200.0f,
                    [high_contrast = state.high_contrast_backdrop](Painter& painter) {
                        paint_backdrop(painter, high_contrast);
                    },
                    {},
                    state.high_contrast_backdrop ? 2u : 1u);
                layout::spacer(10);
                auto backdrop_label = state.high_contrast_backdrop
                    ? std::string("Use standard backdrop")
                    : std::string("Use high-contrast backdrop");
                auto inspector_label = state.inspector_open
                    ? std::string("Hide inspector")
                    : std::string("Show inspector");
                layout::row([&] {
                    widget::button<Msg>(
                        backdrop_label,
                        ToggleBackdrop{},
                        ButtonVariant::Primary);
                    widget::button<Msg>(inspector_label, ToggleInspector{});
                });
            });

            layout::spacer(12);
            layout::card([&] {
                widget::text("Material Ramp");
                layout::spacer(6);
                widget::text("Every public MaterialKind is present as a semantic node in this scene. macOS native samples the captured backdrop; other backends keep the documented translucent fallback.");
                layout::spacer(10);
                material_panel(
                    MaterialKind::Clear,
                    "Clear Material",
                    "Lowest opacity surface for contextual controls over rich content.",
                    "context");
                layout::spacer(8);
                material_panel(
                    MaterialKind::Thin,
                    "Thin Material",
                    "Balanced surface for floating navigation and secondary controls.",
                    "balanced");
                layout::spacer(8);
                material_panel(
                    MaterialKind::Regular,
                    "Regular Material",
                    "Default legible glass layer for primary controls and inspectors.",
                    "legible");
                layout::spacer(8);
                material_panel(
                    MaterialKind::Thick,
                    "Thick Material",
                    "High-contrast surface for dense text over active content.",
                    "high-contrast");
            });

            layout::spacer(12);
            layout::material_surface(MaterialKind::Regular, [&] {
                widget::text("Control Layer");
                layout::spacer(6);
                widget::text("This panel combines material semantics with ordinary controls so input, focus, and artifact labels stay debuggable.");
                layout::spacer(8);
                {
                    std::vector<phenotype::str> tabs;
                    tabs.emplace_back("Comfortable", 11u);
                    tabs.emplace_back("Balanced", 8u);
                    tabs.emplace_back("Dense", 5u);
                    widget::tabs<Msg>(tabs, state.selected_density,
                        [](std::size_t index) -> Msg {
                            return SelectDensity{index};
                        });
                }
                layout::spacer(8);
                widget::text(std::string("Density: ") + density_label(state.selected_density));
                layout::spacer(8);
                widget::text_field<Msg>("Debug note", state.note, on_note_changed);
                layout::spacer(8);
                widget::progress(state.high_contrast_backdrop ? 0.85f : 0.55f, 300.0f);
            });

            if (state.inspector_open) {
                layout::spacer(12);
                layout::material_surface(MaterialKind::Thick, [&] {
                    widget::text("Debug Contract");
                    layout::spacer(6);
                    widget::text("Artifact checks should require material roles, all four material kinds, frame output, and stable semantic labels from this scene.");
                    layout::spacer(8);
                    std::string info = "viewport: ";
                    info += std::to_string(state.viewport_width);
                    info += " x ";
                    info += std::to_string(state.viewport_height);
                    info += "\nscale: ";
                    info += std::to_string(state.viewport_scale);
                    info += "\nlast input: ";
                    info += input_debug.event.empty() ? "none" : input_debug.event;
                    info += "\ninput result: ";
                    info += input_debug.result.empty() ? "none" : input_debug.result;
                    info += "\nmaterial_backdrop_blur: see snapshot capabilities";
                    widget::code(info);
                });
            }
        },
        [] {
            widget::text("Artifact target");
            widget::text("Build, launch with PHENOTYPE_ARTIFACT_DIR, and verify clear/thin/regular/thick material semantics before using the frame for visual debugging.");
        });
}

int main() {
    return phenotype::native::run_app<State, Msg>(
        520, 760, "phenotype glass showcase",
        view, update,
        [](int width, int height, float scale) -> Msg {
            return Resized{width, height, scale};
        });
}
