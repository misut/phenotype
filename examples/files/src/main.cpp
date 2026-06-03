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
                    native::ToolbarGroup::navigation(),
                },
                .trailing_groups = {
                    native::ToolbarGroup::view_modes(),
                    native::ToolbarGroup::sort_menu(),
                    native::ToolbarGroup::item_actions(),
                    native::ToolbarGroup::search(),
                },
            },
            .padding = native::WindowPadding::macos_toolbar(),
        },
        FilesApp{});
}
