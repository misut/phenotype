import phenotype.native;

namespace ui = phenotype::ui;
namespace native = phenotype::native;

struct FilesApp {
    ui::View body(ui::Context&) const {
        return {};
    }
};

int main() {
    return native::run_app(
        native::WindowOptions{
            .width = 980,
            .height = 680,
            .title = "Files",
            .backdrop = native::WindowBackdrop::glass,
            .glass = {
                .material =
                    native::WindowGlassMaterial::under_window_background,
                .opacity = 1.0f,
            },
            .toolbar = {
                .visible = true,
                .title = "examples",
                .leading_groups = {
                    native::ToolbarGroup::of({
                        native::ToolbarIconButton::make_navigation_icon(
                            "chevron_left",
                            "Back"),
                        native::ToolbarIconButton::make_navigation_icon(
                            "chevron_right",
                            "Forward"),
                    }),
                },
                .trailing_groups = {
                    native::ToolbarGroup::of({
                        native::ToolbarIconButton::make_selected_icon(
                            "grid_view",
                            "Icon View"),
                        native::ToolbarIconButton::make_icon(
                            "view_list",
                            "List View"),
                        native::ToolbarIconButton::make_icon(
                            "view_column",
                            "Column View"),
                        native::ToolbarIconButton::make_icon(
                            "view_carousel",
                            "Gallery View"),
                    }),
                    native::ToolbarGroup::of({
                        native::ToolbarIconButton::make_menu_icon(
                            "swap_vert",
                            "Sort"),
                    }),
                    native::ToolbarGroup::of({
                        native::ToolbarIconButton::make_icon(
                            "ios_share",
                            "Share"),
                        native::ToolbarIconButton::make_icon("sell", "Tags"),
                        native::ToolbarIconButton::make_icon(
                            "more_horiz",
                            "More"),
                    }),
                    native::ToolbarGroup::of({
                        native::ToolbarIconButton::make_icon("search", "Search"),
                    }),
                },
            },
            .padding = {
                .left = 20.0f,
                .top = 10.0f,
                .right = 18.0f,
                .bottom = 18.0f,
            },
        },
        FilesApp{});
}
