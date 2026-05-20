#include <string>
#include <vector>

import glass_showcase_shared;
import phenotype;
import phenotype.native;

namespace glass = glass_showcase_demo;

using State = glass::State;
using Msg = glass::Msg;

static State const* g_debug_state = nullptr;

static auto glass_showcase_application_debug_payload() {
    if (!g_debug_state)
        return glass::glass_showcase_application_debug_json(State{});
    return glass::glass_showcase_application_debug_json(*g_debug_state);
}

static phenotype::MaterialKind material_kind(
        glass::GlassProbeMaterialKind kind) {
    switch (kind) {
        case glass::GlassProbeMaterialKind::Clear:
            return phenotype::MaterialKind::Clear;
        case glass::GlassProbeMaterialKind::Thin:
            return phenotype::MaterialKind::Thin;
        case glass::GlassProbeMaterialKind::Regular:
            return phenotype::MaterialKind::Regular;
        case glass::GlassProbeMaterialKind::Thick:
            return phenotype::MaterialKind::Thick;
    }
    return phenotype::MaterialKind::Regular;
}

static std::string material_contract_text(
        glass::GlassMaterialProbe const& probe) {
    std::string text = "kind: ";
    text += glass::glass_probe_material_kind_name(probe.kind);
    text += "\npass: ";
    text += probe.expected_pass;
    text += "\nexecutor: ";
    text += probe.expected_executor;
    text += "\nluminance: ";
    text += probe.expected_luminance_curve;
    text += "\nfallback: ";
    text += probe.fallback_path;
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

    painter.linear_gradient_rect(
        0.0f, 0.0f, 360.0f, 200.0f,
        Color{239, 246, 255, 255},
        Color{250, 245, 255, 255},
        GradientAxis::Vertical,
        18);

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
        glass::GlassMaterialProbe const& probe) {
    using namespace phenotype;
    layout::material_surface(material_kind(probe.kind), [&] {
        widget::text(phenotype::str{probe.label.data(),
                                    static_cast<unsigned int>(
                                        probe.label.size())});
        layout::spacer(4);
        widget::text(phenotype::str{probe.description.data(),
                                    static_cast<unsigned int>(
                                        probe.description.size())});
        layout::spacer(8);
        widget::code(material_contract_text(probe));
    });
}

static void view(State const& state) {
    using namespace phenotype;
    g_debug_state = &state;
    auto input_debug = phenotype::diag::input_debug_snapshot();
    auto const probe_contract = glass::glass_probe_contract(state);

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
                        glass::ToggleBackdrop{},
                        ButtonVariant::Primary);
                    widget::button<Msg>(inspector_label, glass::ToggleInspector{});
                });
            });

            layout::spacer(12);
            layout::card([&] {
                widget::text("Material Ramp");
                layout::spacer(6);
                widget::text("Every public MaterialKind is present as a semantic node in this scene. The probe represents functional layers; unsupported backends keep the documented translucent fallback.");
                layout::spacer(10);
                layout::material_container(
                    layout::MaterialContainerOptions{
                        .container_id = 1001u,
                        .spacing = 20.0f,
                        .morph_transitions = true,
                    },
                    [] {
                        material_panel(glass::glass_material_probe_at(0));
                        layout::spacer(8);
                        material_panel(glass::glass_material_probe_at(1));
                        layout::spacer(8);
                        material_panel(glass::glass_material_probe_at(2));
                        layout::spacer(8);
                        material_panel(glass::glass_material_probe_at(3));
                    });
            });

            layout::spacer(12);
            layout::material_container(
                layout::MaterialContainerOptions{
                    .container_id = 1003u,
                    .spacing = 12.0f,
                    .interactive = true,
                    .morph_transitions = false,
                },
                [&] {
                    auto const control_probe =
                        glass::glass_material_probe_at(4);
                    layout::material_surface(
                        layout::MaterialSurfaceOptions{
                            .kind = material_kind(control_probe.kind),
                            .interaction =
                                MaterialInteractionDescriptor{
                                    .hovered = true,
                                    .focused = true,
                                    .pointer_inside = true,
                                    .pointer_x = 0.62f,
                                    .pointer_y = 0.38f,
                                },
                        },
                        [&] {
                        widget::text(phenotype::str{
                            control_probe.label.data(),
                            static_cast<unsigned int>(
                                control_probe.label.size())});
                        layout::spacer(6);
                        widget::text(phenotype::str{
                            control_probe.description.data(),
                            static_cast<unsigned int>(
                                control_probe.description.size())});
                        layout::spacer(8);
                        {
                            std::vector<phenotype::str> tabs;
                            tabs.emplace_back("Comfortable", 11u);
                            tabs.emplace_back("Balanced", 8u);
                            tabs.emplace_back("Dense", 5u);
                            widget::tabs<Msg>(tabs, state.selected_density,
                                [](std::size_t index) -> Msg {
                                    return glass::select_density(index);
                                });
                        }
                        layout::spacer(8);
                        widget::text(std::string("Density: ") + glass::density_label(state.selected_density));
                        layout::spacer(8);
                        widget::text_field<Msg>("Debug note", state.note, glass::note_changed);
                        layout::spacer(8);
                        widget::progress(glass::progress_value(state), 300.0f);
                        });
                });

            if (state.inspector_open) {
                layout::spacer(12);
                auto const debug_probe = glass::glass_material_probe_at(6);
                layout::material_surface(material_kind(debug_probe.kind), [&] {
                    widget::text(phenotype::str{
                        debug_probe.label.data(),
                        static_cast<unsigned int>(debug_probe.label.size())});
                    layout::spacer(6);
                    widget::text(phenotype::str{
                        debug_probe.description.data(),
                        static_cast<unsigned int>(
                            debug_probe.description.size())});
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
                    info += "\nprobe contract: ";
                    info += probe_contract.contract_name;
                    info += "\nactive probes: ";
                    info += std::to_string(probe_contract.active_material_probe_count);
                    info += "\nexpected stages: ";
                    info += std::to_string(
                        probe_contract.total_expected_execution_stages);
                    widget::code(info);
                });
            }
        },
        [] {
            widget::text("Artifact target");
            widget::text("Build, launch with PHENOTYPE_ARTIFACT_DIR, and verify clear/thin/regular/thick material semantics before using the frame for visual debugging.");
        });

    layout::overlay([&] {
        layout::spacer(430);
        layout::row([&] {
            layout::sized_box(280.0f, [&] {
                layout::material_container(
                    layout::MaterialContainerOptions{
                        .container_id = 1002u,
                        .union_id = 1u,
                        .spacing = 28.0f,
                        .morph_transitions = true,
                    },
                    [] {
                        auto const blur_probe =
                            glass::glass_material_probe_at(5);
                        layout::material_surface(
                            material_kind(blur_probe.kind),
                            [blur_probe] {
                                widget::text(phenotype::str{
                                    blur_probe.label.data(),
                                    static_cast<unsigned int>(
                                        blur_probe.label.size())});
                                layout::spacer(4);
                                widget::text(phenotype::str{
                                    blur_probe.description.data(),
                                    static_cast<unsigned int>(
                                        blur_probe.description.size())});
                            },
                            SpaceToken::Sm,
                            SpaceToken::Xs);
                    });
            });
        }, SpaceToken::Sm, CrossAxisAlignment::Center, MainAxisAlignment::Center);
    });
}

int main() {
    phenotype::diag::set_application_debug_provider(
        glass_showcase_application_debug_payload);
    return phenotype::native::run_app<State, Msg>(
        glass::k_default_viewport_width,
        glass::k_default_viewport_height,
        "phenotype glass showcase",
        view, glass::update,
        [](int width, int height, float scale) -> Msg {
            return glass::resized(width, height, scale);
        });
}
