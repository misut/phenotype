#include <algorithm>
#include <string>
#include <utility>
#include <vector>

import phenotype;

namespace ui = phenotype::ui;

namespace {

ui::Text muted(std::string text) {
    return ui::Text(std::move(text)).color(phenotype::TextColor::Muted);
}

ui::Text caption(std::string text) {
    return ui::Text(std::move(text))
        .font(ui::Font::caption)
        .color(phenotype::TextColor::Muted);
}

float content_width(ui::Context const& cx) {
    auto const viewport = cx.viewport_width();
    if (viewport <= 1.0f)
        return 1120.0f;

    auto const margin = viewport < 720.0f ? 32.0f : 64.0f;
    return std::max(280.0f, std::min(1120.0f, viewport - margin));
}

float panel_height(ui::Context const& cx, bool compact) {
    auto const viewport = cx.viewport_height();
    if (viewport <= 1.0f)
        return compact ? 480.0f : 560.0f;

    auto const reserved = compact ? 300.0f : 320.0f;
    return std::max(360.0f, viewport - reserved);
}

template <typename Child>
ui::View centered(float width, Child child) {
    auto view = ui::as_view(std::move(child));
    return ui::View{[width, view = std::move(view)] {
        phenotype::layout::row(
            [&] {
                phenotype::layout::sized_box(width, [&] {
                    view.render();
                });
            },
            phenotype::SpaceToken::Md,
            phenotype::CrossAxisAlignment::Start,
            phenotype::MainAxisAlignment::Center);
    }};
}

ui::View responsive_pair(bool compact, ui::View leading, ui::View trailing) {
    if (compact) {
        return ui::VStack(
            ui::StackOptions{.spacing = phenotype::SpaceToken::Lg},
            std::move(leading),
            std::move(trailing));
    }

    return ui::HStack(
        ui::StackOptions{
            .spacing = phenotype::SpaceToken::Xl,
            .cross = ui::AxisAlignment::start},
        ui::Weighted(1.05f, std::move(leading)),
        ui::Weighted(0.95f, std::move(trailing)));
}

ui::View responsive_stat_pair(bool compact, ui::View first, ui::View second) {
    if (compact) {
        return ui::VStack(
            ui::StackOptions{.spacing = phenotype::SpaceToken::Md},
            std::move(first),
            std::move(second));
    }

    return ui::HStack(
        ui::StackOptions{
            .spacing = phenotype::SpaceToken::Md,
            .cross = ui::AxisAlignment::start},
        ui::Weighted(1.0f, std::move(first)),
        ui::Weighted(1.0f, std::move(second)));
}

ui::View section_header(std::string title, std::string summary) {
    return ui::VStack(
        ui::StackOptions{.spacing = phenotype::SpaceToken::Sm},
        ui::Text(std::move(title)).font(ui::Font::title),
        muted(std::move(summary)));
}

ui::View stat_card(std::string value, std::string label, std::string detail) {
    return ui::Card(
        ui::VStack(
            ui::StackOptions{.spacing = phenotype::SpaceToken::Sm},
            ui::Text(std::move(value)).font(ui::Font::title),
            ui::Text(std::move(label)),
            caption(std::move(detail))));
}

ui::View pipeline_diagram(bool compact) {
    auto const width = compact ? 280.0f : 400.0f;
    auto const height = compact ? 168.0f : 188.0f;

    return ui::View{[width, height, compact] {
        phenotype::widget::semantic_canvas(
            width,
            height,
            "Phenotype rendering pipeline",
            [width, height, compact](phenotype::Painter& p) {
                using phenotype::Color;
                auto const sx = width / 560.0f;
                auto const sy = height / 220.0f;
                auto x = [sx](float value) { return value * sx; };
                auto y = [sy](float value) { return value * sy; };
                auto fs = [compact](float value) {
                    return compact ? value * 0.76f : value * 0.88f;
                };

                phenotype::PaintRect panels[] = {
                    {x(18.0f), y(28.0f), x(134.0f), y(74.0f),
                     Color{236, 247, 241, 255}},
                    {x(214.0f), y(28.0f), x(134.0f), y(74.0f),
                     Color{239, 244, 255, 255}},
                    {x(410.0f), y(28.0f), x(132.0f), y(74.0f),
                     Color{255, 246, 226, 255}},
                    {x(118.0f), y(132.0f), x(324.0f), y(56.0f),
                     Color{245, 246, 248, 255}},
                };
                p.fill_rects(panels, 4);

                p.line(x(152.0f), y(65.0f), x(214.0f), y(65.0f), 2.0f,
                       Color{46, 122, 92, 255});
                p.line(x(348.0f), y(65.0f), x(410.0f), y(65.0f), 2.0f,
                       Color{46, 92, 160, 255});
                p.line(x(280.0f), y(102.0f), x(280.0f), y(132.0f), 2.0f,
                       Color{132, 96, 30, 255});

                auto const view_label = compact ? "Views" : "View tree";
                auto const artifact_label = compact
                    ? "semantic artifacts"
                    : "diagnostics and semantic artifacts";

                p.text(x(38.0f), y(50.0f), "State", 5u, fs(16.0f),
                       Color{26, 70, 52, 255});
                p.text(x(232.0f), y(50.0f), view_label,
                       compact ? 5u : 9u, fs(16.0f),
                       Color{35, 63, 120, 255});
                p.text(x(430.0f), y(50.0f), "Renderer", 8u, fs(16.0f),
                       Color{106, 72, 18, 255});
                p.text(x(152.0f), y(152.0f), artifact_label,
                       compact ? 18u : 34u, fs(14.0f),
                       Color{72, 74, 82, 255});
            },
            {},
            0xD0C50001u,
            "image");
    }};
}

ui::View docs_hero(bool compact,
                   float width,
                   ui::State<std::size_t> section) {
    auto const title_font = compact ? ui::Font::title : ui::Font::hero_title;
    auto const subtitle_font = compact
        ? ui::Font::body
        : ui::Font::hero_subtitle;
    auto const subtitle = compact
        ? "C++23 UI for native and WASI."
        : "Native and WASI interfaces from one modern C++ tree.";
    auto const summary = compact
        ? "Compose state, controls, rendering, and verification in one tree."
        : "Compose state locally, bind inputs directly, render material "
          "surfaces, and verify the result with artifacts.";

    auto actions = compact
        ? ui::VStack(
            ui::StackOptions{.spacing = phenotype::SpaceToken::Sm},
            ui::Button("Start tutorial")
                .role(ui::ButtonRole::primary)
                .width(210.0f)
                .on_click([section] {
                    section.set(2);
                }))
        : ui::HStack(
            ui::Button("Start tutorial")
                .role(ui::ButtonRole::primary)
                .on_click([section] {
                    section.set(2);
                }),
            ui::Link("Material 3 reference", "https://m3.material.io/"));

    return centered(
        width,
        ui::VStack(
            ui::StackOptions{.spacing = phenotype::SpaceToken::Md},
            caption("C++23 declarative UI"),
            ui::Text("Phenotype").font(title_font),
            ui::Text(subtitle).font(subtitle_font),
            muted(summary),
            std::move(actions)));
}

ui::Button nav_button(ui::State<std::size_t> section,
                      std::size_t index,
                      std::string label,
                      bool compact) {
    return ui::Button(std::move(label))
        .width(compact ? 82.0f : 150.0f)
        .on_click([section, index] {
            section.set(index);
        });
}

ui::View docs_nav(ui::State<std::size_t> section,
                  float width,
                  bool compact) {
    return centered(
        width,
        ui::HStack(
            ui::StackOptions{
                .spacing = phenotype::SpaceToken::Sm,
                .cross = ui::AxisAlignment::center,
                .main = ui::AxisAlignment::center},
            nav_button(section, 0, "Start", compact),
            nav_button(section, 1, "Features", compact),
            nav_button(section, 2, "Tutorial", compact),
            nav_button(section, 3, "Verify", compact)));
}

ui::View overview_content(ui::Context& cx, bool compact) {
    auto count = cx.state<int>("preview.count", 2);
    auto project = cx.state<std::string>("preview.project", "Orbit");
    auto diagnostics = cx.state<bool>("preview.diagnostics", true);
    auto accepted = cx.state<bool>("preview.accepted", false);
    auto target = cx.state<std::size_t>("preview.target", 0);

    auto const progress = diagnostics.get() ? 0.78f : 0.38f;

    auto preview = ui::Card(
        ui::VStack(
            ui::StackOptions{.spacing = phenotype::SpaceToken::Md},
            ui::Text("Live component preview").font(ui::Font::title),
            muted("This panel is rendered by the same public API shown in "
                  "the tutorial."),
            ui::Text("Counter frame " + std::to_string(count.get())),
            ui::HStack(
                ui::Button("-").on_click([count] {
                    count.mutate([](int& value) { --value; });
                }),
                ui::Button("+")
                    .role(ui::ButtonRole::primary)
                    .on_click([count] {
                        count.mutate([](int& value) { ++value; });
                    })),
            ui::TextField("Project", project.binding())
                .semantic_label("Preview project name"),
            ui::Tabs(std::vector<std::string>{"WASI", "Native", "Artifacts"},
                     target.get())
                .on_select([target](std::size_t index) {
                    target.set(index);
                }),
            ui::Checkbox("Accept artifact contract", accepted.get())
                .on_toggle([accepted] {
                    accepted.mutate([](bool& value) { value = !value; });
                }),
            ui::Switch("Live diagnostics", diagnostics.get())
                .on_toggle([diagnostics] {
                    diagnostics.mutate([](bool& value) { value = !value; });
                }),
            ui::Progress(progress).width(compact ? 240.0f : 320.0f)));

    auto pipeline = ui::Card(
        ui::VStack(
            ui::StackOptions{.spacing = phenotype::SpaceToken::Md},
            ui::Text("Rendering pipeline").font(ui::Font::title),
            muted("State, view construction, and the renderer stay observable "
                  "for native and WASI hosts."),
            pipeline_diagram(compact),
            responsive_stat_pair(
                compact,
                stat_card("C++23", "language surface",
                          "modules, lambdas, RAII"),
                stat_card("WASI", "browser target",
                          "single canvas host"))));

    auto code = ui::Card(
        ui::VStack(
            ui::StackOptions{.spacing = phenotype::SpaceToken::Md},
            ui::Text("The code stays local").font(ui::Font::title),
            muted("A view reads state, describes UI, and passes callbacks. "
                  "There is no extra object graph to synchronize."),
            ui::Code(
                "auto project = cx.state<std::string>(\"project\", {});\n"
                "auto target = cx.state<std::size_t>(\"target\", 0);\n"
                "\n"
                "return ui::VStack(\n"
                "    ui::TextField(\"Project\", project.binding()),\n"
                "    ui::Tabs({\"WASI\", \"Native\"}, target.get())\n"
                "        .on_select([target](std::size_t index) {\n"
                "            target.set(index);\n"
                "        }));")));

    return ui::VStack(
        ui::StackOptions{.spacing = phenotype::SpaceToken::Lg},
        section_header(
            "Start with a living component",
            "Phenotype favors the same directness seen in current declarative "
            "frameworks: describe the view, keep state nearby, and verify the "
            "result as an artifact."),
        responsive_pair(compact, std::move(preview), std::move(pipeline)),
        std::move(code));
}

ui::View features_content(bool compact) {
    return ui::VStack(
        ui::StackOptions{.spacing = phenotype::SpaceToken::Lg},
        section_header(
            "Feature map",
            "A purpose-first catalog keeps the API easy to scan and easy to "
            "extend."),
        ui::Card(
            ui::VStack(
                ui::StackOptions{.spacing = phenotype::SpaceToken::Sm},
                ui::Code(
                    "Action: callbacks and commands\n"
                    "Containment: cards, surfaces, overlays\n"
                    "Navigation: tabs and keyed children\n"
                    "Selection: checkbox, radio, switch, progress\n"
                    "Text input: state bindings\n"
                    "Verification: diagnostics and screenshots"))));
}

ui::View tutorial_content() {
    return ui::VStack(
        ui::StackOptions{.spacing = phenotype::SpaceToken::Lg},
        section_header(
            "Quick tutorial",
            "Start from a tiny component, add state, bind controls, then "
            "verify the same tree locally and in CI."),
        ui::Card(
            ui::VStack(
                ui::StackOptions{.spacing = phenotype::SpaceToken::Sm},
                caption("Flow"),
                muted("1. Define a component. 2. Add local state. "
                      "3. Bind controls. 4. Verify artifacts."),
                ui::Code(
                    "auto count = cx.state<int>(\"count\", 0);\n"
                    "ui::Button(\"Increment\").on_click([count] {\n"
                    "    count.set(count.get() + 1);\n"
                    "});"))));
}

ui::View verification_content(bool compact) {
    return ui::VStack(
        ui::StackOptions{.spacing = phenotype::SpaceToken::Lg},
        section_header(
            "Verification and references",
            "The page is built as a local WASI artifact and checked against "
            "official framework references as part of publishing."),
        ui::Card(
            ui::VStack(
                ui::StackOptions{.spacing = phenotype::SpaceToken::Md},
                ui::Text("Runtime contract").font(ui::Font::title),
                muted("The browser host provides canvas size, text "
                      "measurement, URL opening, and a WebGPU command sink."),
                caption("Checks"),
                muted("Native tests, WASI build, desktop screenshot, mobile "
                      "screenshot, and console errors."),
                ui::Divider(),
                ui::Text("References").font(ui::Font::title),
                ui::Link("Material 3 components",
                         "https://m3.material.io/components"),
                ui::Link("Material 3 in Compose",
                         "https://developer.android.com/develop/ui/compose/designsystems/material3"),
                ui::Link("React learn", "https://react.dev/learn"),
                ui::Link("GitHub", "https://github.com/misut/phenotype"))));
}

ui::View selected_content(std::size_t section,
                          ui::Context& cx,
                          bool compact) {
    switch (section) {
        case 1:
            return features_content(compact);
        case 2:
            return tutorial_content();
        case 3:
            return verification_content(compact);
        case 0:
        default:
            return overview_content(cx, compact);
    }
}

} // namespace

struct DocsApp {
    ui::View body(ui::Context& cx) const {
        bool const compact = cx.compact_width(860.0f);
        auto const width = content_width(cx);
        auto const height = panel_height(cx, compact);
        auto section = cx.state<std::size_t>("docs.section", 0);

        return ui::VStack(
            ui::StackOptions{.spacing = phenotype::SpaceToken::Lg},
            docs_hero(compact, width, section),
            docs_nav(section, width, compact),
            centered(
                width,
                ui::ScrollView(
                    height,
                    selected_content(section.get(), cx, compact))));
    }
};

int main() {
    ui::run<DocsApp>();
    return 0;
}
