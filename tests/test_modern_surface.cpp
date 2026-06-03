#include <cassert>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

import phenotype;
import phenotype.native;

namespace ui = phenotype::ui;
namespace native = phenotype::native;

extern "C" {
extern unsigned int phenotype_cmd_len;
void phenotype_set_focus(unsigned int callback_id);
void phenotype_handle_event(unsigned int callback_id);
}

namespace {

struct CounterApp {
    ui::View body(ui::Context& cx) const {
        auto count = cx.state<int>("count", 0);

        return ui::VStack(
            ui::Text("Counter").font(ui::Font::title),
            ui::Text("count=" + std::to_string(count.get())),
            ui::Button("+")
                .role(ui::ButtonRole::primary)
                .on_click([count] {
                    count.set(count.get() + 1);
                }));
    }
};

struct InputApp {
    ui::View body(ui::Context& cx) const {
        auto query = cx.state<std::string>("query", {});
        auto section = cx.state<std::size_t>("section", 0);
        auto accepted = cx.state<bool>("accepted", false);

        return ui::VStack(
            ui::TextField("Search", query.binding()),
            ui::Tabs(std::vector<std::string>{"Intro", "API"}, section.get())
                .on_select([section](std::size_t index) {
                    section.set(index);
                }),
            ui::Checkbox("Accepted", accepted.get())
                .on_toggle([accepted] {
                    accepted.set(!accepted.get());
                }),
            ui::Text(
                "query=" + query.get()
                + " section=" + std::to_string(section.get())
                + " accepted=" + std::string(accepted.get() ? "true" : "false")));
    }
};

struct LayoutApp {
    ui::View body(ui::Context& cx) const {
        bool const compact = cx.compact_width(1200.0f);

        return ui::VStack(
            ui::Text(compact ? "compact=true" : "compact=false"),
            ui::Card(
                ui::HStack(
                    ui::Weighted(1.0f, ui::Text("left")),
                    ui::Weighted(1.0f, ui::Text("right")))),
            ui::Progress(0.65f).width(180.0f));
    }
};

struct GlassApp {
    ui::View body(ui::Context& cx) const {
        auto query = cx.state<std::string>("glass.query", {});
        auto tab = cx.state<std::size_t>("glass.tab", 0);

        auto material = phenotype::GlassMaterial::regular()
            .role_as(phenotype::GlassMaterialRole::surface)
            .radius(22.0f)
            .grouped(1001u, 36.0f);

        return ui::VStack(
            ui::Glass(
                material,
                ui::VStack(
                    ui::StackOptions{.spacing = phenotype::SpaceToken::Md},
                    ui::Text("Glass surface").font(ui::Font::title),
                    ui::Text("A material container can own controls."),
                    ui::Button("Glass action")
                        .role(ui::ButtonRole::primary)
                        .glass(),
                    ui::TextField("Glass input", query.binding())
                        .glass(phenotype::GlassMaterial::thin().radius(14.0f)),
                    ui::Tabs(std::vector<std::string>{"One", "Two"}, tab.get())
                        .glass(phenotype::GlassMaterial::clear().capsule())
                        .on_select([tab](std::size_t index) {
                            tab.set(index);
                        }))));
    }
};

void test_state_and_callbacks() {
    ui::run<CounterApp>();

    assert(phenotype::testing::command_buffer_contains("count=0"));
    assert(phenotype::testing::callback_count() == 1u);

    phenotype::testing::invoke_callback(0);

    assert(phenotype::testing::command_buffer_contains("count=1"));
    std::puts("PASS: state and callbacks");
}

void test_input_and_selection_controls() {
    auto standalone_state = ui::State<std::string>{std::make_shared<std::string>("")};
    auto standalone_binding = ui::Binding<std::string>{standalone_state};
    standalone_binding.set("shared");
    assert(standalone_state.get() == "shared");

    ui::run<InputApp>();

    assert(phenotype::testing::input_count() == 1u);
    phenotype_set_focus(0);
    phenotype::testing::commit_focused_input("phenotype");
    assert(phenotype::testing::command_buffer_contains("query=phenotype"));

    phenotype_handle_event(2);
    assert(phenotype::testing::command_buffer_contains("section=1"));

    phenotype_handle_event(3);
    assert(phenotype::testing::command_buffer_contains("accepted=true"));
    std::puts("PASS: input and selection controls");
}

void test_layout_render_contract() {
    ui::run<LayoutApp>();

    assert(phenotype::testing::command_buffer_contains("compact=true"));
    assert(phenotype::testing::command_buffer_contains("left"));
    assert(phenotype::testing::command_buffer_contains("right"));
    assert(phenotype_cmd_len > 64u);
    assert(phenotype::testing::total_height() > 0.0f);
    std::puts("PASS: layout render contract");
}

void test_glass_material_contract() {
    ui::run<GlassApp>();

    assert(phenotype::testing::command_buffer_contains("Glass surface"));
    assert(phenotype::testing::command_buffer_contains("Glass action"));
    assert(phenotype::testing::round_rect_count() >= 10u);
    assert(phenotype::testing::input_count() == 1u);
    std::puts("PASS: glass material contract");
}

void test_native_toolbar_presets() {
    auto toolbar = native::WindowToolbar{
        .visible = true,
        .title = "Files",
        .leading_groups = {
            native::ToolbarGroup::navigation(),
        },
        .trailing_groups = {
            native::ToolbarGroup::view_modes(
                native::ToolbarViewMode::columns),
            native::ToolbarGroup::sort_menu(native::ToolbarSortMode::size),
            native::ToolbarGroup::item_actions({
                native::ToolbarMenuItem::command("Duplicate"),
            }),
            native::ToolbarGroup::search(),
        },
    };

    assert(toolbar.metrics.group_gap == 14.0f);
    assert(toolbar.metrics.trailing_inset == 10.0f);

    assert(toolbar.leading_groups.size() == 1u);
    assert(toolbar.leading_groups.front().buttons.size() == 2u);
    assert(!toolbar.leading_groups.front().buttons.front().enabled);

    auto const& view_group = toolbar.trailing_groups.at(0);
    assert(view_group.buttons.size() == 4u);
    assert(view_group.buttons.at(2).selected);
    assert(view_group.compact_button.has_value());
    assert(view_group.compact_button->icon == "view_column");
    assert(view_group.compact_button->opens_menu);
    assert(view_group.compact_button->menu_items.size() == 4u);
    assert(view_group.compact_button->menu_items.at(2).selected);

    auto const& sort_group = toolbar.trailing_groups.at(1);
    assert(sort_group.buttons.size() == 1u);
    assert(sort_group.buttons.front().opens_menu);
    assert(sort_group.buttons.front().menu_items.at(1).separator_before);
    assert(sort_group.buttons.front().menu_items.at(2).selected);
    assert(sort_group.compact_stages.size() == 1u);
    assert(sort_group.compact_stages.front().overflow);

    auto const& actions_group = toolbar.trailing_groups.at(2);
    assert(!actions_group.separators);
    assert(actions_group.buttons.size() == 3u);
    assert(actions_group.buttons.at(2).menu_items.at(0).label
           == "Duplicate");
    assert(actions_group.compact_stages.size() == 2u);
    assert(actions_group.compact_stages.at(0).segments.size() == 2u);
    assert(actions_group.compact_stages.at(1).overflow);

    auto const& search_group = toolbar.trailing_groups.at(3);
    assert(search_group.buttons.size() == 1u);
    assert(search_group.buttons.front().opens_search);

    auto padding = native::WindowPadding::macos_toolbar();
    assert(padding.left == 20.0f);
    assert(padding.top == 10.0f);
    assert(padding.right == 0.0f);
    assert(padding.bottom == 18.0f);
    std::puts("PASS: native toolbar presets");
}

} // namespace

int main() {
    test_state_and_callbacks();
    test_input_and_selection_controls();
    test_layout_render_contract();
    test_glass_material_contract();
    test_native_toolbar_presets();
    std::puts("\nAll modern surface tests passed.");
}
