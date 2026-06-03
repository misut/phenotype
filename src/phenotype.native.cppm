module;

#if defined(__APPLE__)
#include <CoreGraphics/CGGeometry.h>
#include <CoreFoundation/CoreFoundation.h>
#include <CoreText/CoreText.h>
#include <objc/message.h>
#include <objc/objc.h>
#include <objc/runtime.h>

extern "C" void objc_msgSend(void);
extern "C" void objc_msgSendSuper(void);
#endif

export module phenotype.native;

import std;
export import phenotype;

export namespace phenotype::native {

enum class WindowBackdrop {
    opaque,
    glass,
};

enum class WindowGlassMaterial {
    under_window_background,
    sidebar,
};

struct WindowGlassEffect {
    WindowGlassMaterial material = WindowGlassMaterial::under_window_background;
    float opacity = 1.0f;
};

struct MaterialSymbolStyle {
    float weight = 200.0f;
    float grade = 0.0f;
    float optical_size = 24.0f;
    bool fill = false;
    float size = 24.0f;
    float accessory_size = 14.0f;

    static auto toolbar() -> MaterialSymbolStyle {
        return {};
    }

    static auto navigation() -> MaterialSymbolStyle {
        auto style = MaterialSymbolStyle::toolbar();
        style.size = 26.0f;
        return style;
    }
};

enum class ToolbarViewMode {
    icons,
    list,
    columns,
    gallery,
};

enum class ToolbarSortMode {
    none,
    name,
    size,
};

struct ToolbarMenuItem {
    std::string label;
    std::vector<ToolbarMenuItem> submenu_items;
    bool selected = false;
    bool enabled = true;
    bool separator_before = false;
    bool selectable = true;

    static auto option(std::string label) -> ToolbarMenuItem {
        return ToolbarMenuItem{.label = std::move(label)};
    }

    static auto selected_option(std::string label) -> ToolbarMenuItem {
        return ToolbarMenuItem{
            .label = std::move(label),
            .selected = true,
        };
    }

    static auto separated_option(std::string label) -> ToolbarMenuItem {
        return ToolbarMenuItem{
            .label = std::move(label),
            .separator_before = true,
        };
    }

    static auto command(std::string label) -> ToolbarMenuItem {
        return ToolbarMenuItem{
            .label = std::move(label),
            .selectable = false,
        };
    }

    static auto submenu(std::string label, std::vector<ToolbarMenuItem> items)
        -> ToolbarMenuItem {
        return ToolbarMenuItem{
            .label = std::move(label),
            .submenu_items = std::move(items),
            .selectable = false,
        };
    }

    static auto submenu(std::string label,
                        std::initializer_list<ToolbarMenuItem> items)
        -> ToolbarMenuItem {
        return ToolbarMenuItem::submenu(
            std::move(label),
            std::vector<ToolbarMenuItem>{items});
    }
};

struct ToolbarIconButton {
    std::string icon;
    std::string label;
    std::string accessory_icon;
    std::vector<ToolbarMenuItem> menu_items;
    bool selected = false;
    bool enabled = true;
    bool opens_menu = false;
    bool opens_overflow_menu = false;
    bool opens_search = false;
    std::optional<MaterialSymbolStyle> symbol;

    static auto make_icon(std::string icon, std::string label)
        -> ToolbarIconButton {
        auto button = ToolbarIconButton{};
        button.icon = std::move(icon);
        button.label = std::move(label);
        return button;
    }

    static auto make_selected_icon(std::string icon, std::string label)
        -> ToolbarIconButton {
        auto button =
            ToolbarIconButton::make_icon(std::move(icon), std::move(label));
        button.selected = true;
        return button;
    }

    static auto make_disabled_icon(std::string icon, std::string label)
        -> ToolbarIconButton {
        auto button =
            ToolbarIconButton::make_icon(std::move(icon), std::move(label));
        button.enabled = false;
        return button;
    }

    static auto make_navigation_icon(std::string icon, std::string label)
        -> ToolbarIconButton {
        auto button = ToolbarIconButton::make_disabled_icon(
            std::move(icon),
            std::move(label));
        button.symbol = MaterialSymbolStyle::navigation();
        return button;
    }

    static auto make_menu_icon(std::string icon,
                               std::string label,
                               std::string accessory = "keyboard_arrow_down")
        -> ToolbarIconButton {
        auto button =
            ToolbarIconButton::make_icon(std::move(icon), std::move(label));
        button.accessory_icon = std::move(accessory);
        return button;
    }

    static auto make_menu_icon(
        std::string icon,
        std::string label,
        std::vector<ToolbarMenuItem> menu_items,
        std::string accessory = "keyboard_arrow_down") -> ToolbarIconButton {
        auto button = ToolbarIconButton::make_menu_icon(
            std::move(icon),
            std::move(label),
            std::move(accessory));
        button.menu_items = std::move(menu_items);
        button.opens_menu = true;
        return button;
    }

    static auto make_menu_icon(
        std::string icon,
        std::string label,
        std::initializer_list<ToolbarMenuItem> menu_items,
        std::string accessory = "keyboard_arrow_down") -> ToolbarIconButton {
        return ToolbarIconButton::make_menu_icon(
            std::move(icon),
            std::move(label),
            std::vector<ToolbarMenuItem>{menu_items},
            std::move(accessory));
    }

    static auto make_icon_menu(
        std::string icon,
        std::string label,
        std::vector<ToolbarMenuItem> menu_items)
        -> ToolbarIconButton {
        auto button =
            ToolbarIconButton::make_icon(std::move(icon), std::move(label));
        button.menu_items = std::move(menu_items);
        button.opens_menu = true;
        return button;
    }

    static auto make_icon_menu(
        std::string icon,
        std::string label,
        std::initializer_list<ToolbarMenuItem> menu_items)
        -> ToolbarIconButton {
        return ToolbarIconButton::make_icon_menu(
            std::move(icon),
            std::move(label),
            std::vector<ToolbarMenuItem>{menu_items});
    }

    static auto make_overflow_icon(std::string icon,
                                   std::string label) -> ToolbarIconButton {
        auto button =
            ToolbarIconButton::make_icon(std::move(icon), std::move(label));
        button.opens_menu = true;
        button.opens_overflow_menu = true;
        return button;
    }

    static auto make_search_icon(std::string icon = "search",
                                 std::string label = "Search")
        -> ToolbarIconButton {
        auto button =
            ToolbarIconButton::make_icon(std::move(icon), std::move(label));
        button.opens_search = true;
        return button;
    }
};

struct ToolbarGroupCompactSegment {
    std::vector<ToolbarIconButton> buttons;
    bool separators = true;

    static auto of(std::initializer_list<ToolbarIconButton> buttons)
        -> ToolbarGroupCompactSegment {
        return ToolbarGroupCompactSegment{
            .buttons = std::vector<ToolbarIconButton>{buttons},
        };
    }

    static auto joined(std::initializer_list<ToolbarIconButton> buttons)
        -> ToolbarGroupCompactSegment {
        return ToolbarGroupCompactSegment{
            .buttons = std::vector<ToolbarIconButton>{buttons},
            .separators = false,
        };
    }
};

struct ToolbarGroupCompactStage {
    int priority = 0;
    std::vector<ToolbarGroupCompactSegment> segments;
    bool overflow = false;

    static auto of(int priority,
                   std::initializer_list<ToolbarIconButton> buttons)
        -> ToolbarGroupCompactStage {
        return ToolbarGroupCompactStage{
            .priority = priority,
            .segments = {ToolbarGroupCompactSegment::of(buttons)},
        };
    }

    static auto joined(int priority,
                       std::initializer_list<ToolbarIconButton> buttons)
        -> ToolbarGroupCompactStage {
        return ToolbarGroupCompactStage{
            .priority = priority,
            .segments = {ToolbarGroupCompactSegment::joined(buttons)},
        };
    }

    static auto split(
        int priority,
        std::initializer_list<ToolbarGroupCompactSegment> segments)
        -> ToolbarGroupCompactStage {
        return ToolbarGroupCompactStage{
            .priority = priority,
            .segments = std::vector<ToolbarGroupCompactSegment>{segments},
        };
    }

    static auto overflow_joined(
        int priority,
        std::initializer_list<ToolbarIconButton> buttons)
        -> ToolbarGroupCompactStage {
        return ToolbarGroupCompactStage{
            .priority = priority,
            .segments = {ToolbarGroupCompactSegment::joined(buttons)},
            .overflow = true,
        };
    }

    static auto hidden(int priority) -> ToolbarGroupCompactStage {
        return ToolbarGroupCompactStage{
            .priority = priority,
            .overflow = true,
        };
    }
};

struct ToolbarGroup {
    std::vector<ToolbarIconButton> buttons;
    bool separators = true;
    std::optional<ToolbarIconButton> compact_button;
    std::optional<int> compact_priority;
    std::vector<ToolbarGroupCompactStage> compact_stages;

    static auto of(std::initializer_list<ToolbarIconButton> buttons)
        -> ToolbarGroup {
        return ToolbarGroup{std::vector<ToolbarIconButton>{buttons}};
    }

    static auto joined(std::initializer_list<ToolbarIconButton> buttons)
        -> ToolbarGroup {
        return ToolbarGroup{
            .buttons = std::vector<ToolbarIconButton>{buttons},
            .separators = false,
        };
    }

    static auto compacting(
        std::initializer_list<ToolbarIconButton> buttons,
        std::initializer_list<ToolbarGroupCompactStage> stages)
        -> ToolbarGroup {
        return ToolbarGroup{
            .buttons = std::vector<ToolbarIconButton>{buttons},
            .compact_stages = std::vector<ToolbarGroupCompactStage>{stages},
        };
    }

    static auto collapsible(std::initializer_list<ToolbarIconButton> buttons,
                            ToolbarIconButton compact_button,
                            int priority = 0,
                            std::initializer_list<ToolbarGroupCompactStage>
                                stages = {}) -> ToolbarGroup {
        return ToolbarGroup{
            .buttons = std::vector<ToolbarIconButton>{buttons},
            .separators = true,
            .compact_button = std::move(compact_button),
            .compact_priority = priority,
            .compact_stages = std::vector<ToolbarGroupCompactStage>{stages},
        };
    }

    static auto joined_collapsible(
        std::initializer_list<ToolbarIconButton> buttons,
        std::initializer_list<ToolbarGroupCompactStage> stages)
        -> ToolbarGroup {
        return ToolbarGroup{
            .buttons = std::vector<ToolbarIconButton>{buttons},
            .separators = false,
            .compact_stages = std::vector<ToolbarGroupCompactStage>{stages},
        };
    }

    static auto navigation() -> ToolbarGroup {
        return ToolbarGroup::of({
            ToolbarIconButton::make_navigation_icon("chevron_left", "Back"),
            ToolbarIconButton::make_navigation_icon("chevron_right", "Forward"),
        });
    }

    static auto view_modes(ToolbarViewMode selected = ToolbarViewMode::icons)
        -> ToolbarGroup {
        auto const make_button = [selected](ToolbarViewMode mode) {
            if (mode == selected) {
                return ToolbarIconButton::make_selected_icon(
                    toolbar_view_mode_icon(mode),
                    toolbar_view_mode_label(mode));
            }
            return ToolbarIconButton::make_icon(
                toolbar_view_mode_icon(mode),
                toolbar_view_mode_label(mode));
        };

        return ToolbarGroup::collapsible(
            {
                make_button(ToolbarViewMode::icons),
                make_button(ToolbarViewMode::list),
                make_button(ToolbarViewMode::columns),
                make_button(ToolbarViewMode::gallery),
            },
            ToolbarIconButton::make_menu_icon(
                toolbar_view_mode_icon(selected),
                "View",
                toolbar_view_mode_menu_items(selected),
                "unfold_more"),
            10,
            {
                ToolbarGroupCompactStage::hidden(50),
            });
    }

    static auto sort_menu(ToolbarSortMode selected = ToolbarSortMode::none)
        -> ToolbarGroup {
        return ToolbarGroup::compacting(
            {
                ToolbarIconButton::make_menu_icon(
                    "swap_vert",
                    "Sort",
                    toolbar_sort_menu_items(selected)),
            },
            {
                ToolbarGroupCompactStage::hidden(40),
            });
    }

    static auto item_actions(
        std::initializer_list<ToolbarMenuItem> more_menu_items = {})
        -> ToolbarGroup {
        auto more_items = std::vector<ToolbarMenuItem>{more_menu_items};
        if (more_items.empty())
            more_items = default_item_action_menu_items();
        return ToolbarGroup::item_actions(std::move(more_items));
    }

    static auto item_actions(std::vector<ToolbarMenuItem> more_menu_items)
        -> ToolbarGroup {
        auto tag_and_more_items = std::vector<ToolbarMenuItem>{
            ToolbarMenuItem::command("Tag"),
            ToolbarMenuItem::submenu("More", more_menu_items),
        };

        return ToolbarGroup::joined_collapsible(
            {
                ToolbarIconButton::make_icon("ios_share", "Share"),
                ToolbarIconButton::make_icon("sell", "Tag"),
                ToolbarIconButton::make_icon_menu(
                    "more_horiz",
                    "More",
                    more_menu_items),
            },
            {
                ToolbarGroupCompactStage::split(
                    20,
                    {
                        ToolbarGroupCompactSegment::of({
                            ToolbarIconButton::make_icon("ios_share", "Share"),
                        }),
                        ToolbarGroupCompactSegment::of({
                            ToolbarIconButton::make_icon_menu(
                                "keyboard_double_arrow_right",
                                "Tags and More",
                                tag_and_more_items),
                        }),
                    }),
                ToolbarGroupCompactStage::overflow_joined(
                    30,
                    {
                        ToolbarIconButton::make_overflow_icon(
                            "keyboard_double_arrow_right",
                            "Actions"),
                    }),
            });
    }

    static auto search() -> ToolbarGroup {
        return ToolbarGroup::of({ToolbarIconButton::make_search_icon()});
    }

    static auto toolbar_view_mode_icon(ToolbarViewMode mode) -> std::string {
        switch (mode) {
        case ToolbarViewMode::icons:
            return "grid_view";
        case ToolbarViewMode::list:
            return "view_list";
        case ToolbarViewMode::columns:
            return "view_column";
        case ToolbarViewMode::gallery:
            return "view_carousel";
        }
        return "grid_view";
    }

    static auto toolbar_view_mode_label(ToolbarViewMode mode) -> std::string {
        switch (mode) {
        case ToolbarViewMode::icons:
            return "as Icons";
        case ToolbarViewMode::list:
            return "as List";
        case ToolbarViewMode::columns:
            return "as Columns";
        case ToolbarViewMode::gallery:
            return "as Gallery";
        }
        return "as Icons";
    }

    static auto toolbar_view_mode_menu_items(ToolbarViewMode selected)
        -> std::vector<ToolbarMenuItem> {
        auto make_item = [selected](ToolbarViewMode mode) {
            if (mode == selected)
                return ToolbarMenuItem::selected_option(
                    toolbar_view_mode_label(mode));
            return ToolbarMenuItem::option(toolbar_view_mode_label(mode));
        };
        return {
            make_item(ToolbarViewMode::icons),
            make_item(ToolbarViewMode::list),
            make_item(ToolbarViewMode::columns),
            make_item(ToolbarViewMode::gallery),
        };
    }

    static auto toolbar_sort_menu_items(ToolbarSortMode selected)
        -> std::vector<ToolbarMenuItem> {
        auto none = selected == ToolbarSortMode::none
            ? ToolbarMenuItem::selected_option("None")
            : ToolbarMenuItem::option("None");
        auto name = selected == ToolbarSortMode::name
            ? ToolbarMenuItem::selected_option("Name")
            : ToolbarMenuItem::option("Name");
        auto size = selected == ToolbarSortMode::size
            ? ToolbarMenuItem::selected_option("Size")
            : ToolbarMenuItem::option("Size");
        name.separator_before = true;
        return {none, name, size};
    }

    static auto default_item_action_menu_items()
        -> std::vector<ToolbarMenuItem> {
        return {
            ToolbarMenuItem::command("Get Info"),
            ToolbarMenuItem::command("Rename"),
            ToolbarMenuItem::command("Move to Trash"),
        };
    }
};

struct ToolbarMetrics {
    float height = 54.0f;
    float group_height = 38.0f;
    float button_width = 32.0f;
    float button_height = 32.0f;
    float accessory_button_width = 44.0f;
    float button_spacing = 2.0f;
    float group_padding_x = 4.0f;
    float group_gap = 14.0f;
    float leading_reserved_width = 108.0f;
    float trailing_inset = 10.0f;
    float icon_extra_width = 6.0f;
    float icon_extra_height = 4.0f;
    float accessory_icon_extra_width = 2.0f;
    float accessory_icon_extra_height = 4.0f;
};

struct WindowToolbar {
    bool visible = false;
    std::string title;
    std::string icon_directory = "assets/material-symbols/rounded";
    std::string symbol_font =
        "assets/material-symbols/"
        "MaterialSymbolsRounded[FILL,GRAD,opsz,wght].ttf";
    std::string symbol_codepoints =
        "assets/material-symbols/"
        "MaterialSymbolsRounded[FILL,GRAD,opsz,wght].codepoints";
    MaterialSymbolStyle symbol = MaterialSymbolStyle::toolbar();
    ToolbarMetrics metrics = {};
    std::vector<ToolbarGroup> leading_groups;
    std::vector<ToolbarGroup> trailing_groups;
};

struct WindowPadding {
    float left = 0.0f;
    float top = 0.0f;
    float right = 0.0f;
    float bottom = 0.0f;

    static auto macos_toolbar() -> WindowPadding {
        return WindowPadding{
            .left = 20.0f,
            .top = 10.0f,
            .right = 0.0f,
            .bottom = 18.0f,
        };
    }
};

struct WindowMinimumSize {
    float width = 0.0f;
    float height = 0.0f;
};

struct WindowOptions {
    int width = 960;
    int height = 720;
    std::string title = "Phenotype";
    WindowBackdrop backdrop = WindowBackdrop::opaque;
    WindowGlassEffect glass = {};
    WindowToolbar toolbar = {};
    WindowPadding padding = {};
    WindowMinimumSize minimum_size = {};
};

namespace detail {

#if defined(__APPLE__)

using NSUInteger = unsigned long;
using NSInteger = long;
using ObjcBool = signed char;

enum class AppKitWindowStyle : NSUInteger {
    titled = 1u << 0u,
    closable = 1u << 1u,
    miniaturizable = 1u << 2u,
    resizable = 1u << 3u,
    full_size_content_view = 1u << 15u,
};

enum class AppKitBackingStore : NSUInteger {
    buffered = 2u,
};

enum class AppKitAutoresizingMask : NSUInteger {
    min_x_margin = 1u << 0u,
    width_sizable = 1u << 1u,
    max_x_margin = 1u << 2u,
    min_y_margin = 1u << 3u,
    height_sizable = 1u << 4u,
    max_y_margin = 1u << 5u,
};

enum class VisualEffectBlendingMode : NSInteger {
    behind_window = 0,
};

enum class VisualEffectState : NSInteger {
    active = 1,
};

enum class VisualEffectMaterial : NSInteger {
    sidebar = 7,
    under_window_background = 21,
};

enum class ImageScaling : NSInteger {
    proportionally_up_or_down = 3,
};

enum class TextAlignment : NSInteger {
    center = 1,
};

constexpr NSUInteger tracking_mouse_entered_and_exited = 0x01;
constexpr NSUInteger tracking_active_in_active_app = 0x40;
constexpr NSUInteger tracking_in_visible_rect = 0x200;

constexpr NSInteger toolbar_button_selected_tag = 1;
constexpr NSUInteger event_modifier_flag_command = 1ull << 20u;
constexpr CGFloat toolbar_title_width = 320.0;
constexpr CGFloat toolbar_title_height = 28.0;
constexpr CGFloat toolbar_title_spacing = 8.0;
constexpr CGFloat toolbar_title_trailing_gap = 12.0;
constexpr CGFloat toolbar_search_width = 220.0;
constexpr CGFloat toolbar_search_field_y_offset = -1.5;

auto toolbar_tracking_area_key() -> void const* {
    static char key;
    return &key;
}

auto toolbar_leading_separator_key() -> void const* {
    static char key;
    return &key;
}

auto toolbar_trailing_separator_key() -> void const* {
    static char key;
    return &key;
}

auto toolbar_button_toolbar_view_key() -> void const* {
    static char key;
    return &key;
}

auto toolbar_button_group_side_key() -> void const* {
    static char key;
    return &key;
}

auto toolbar_button_group_index_key() -> void const* {
    static char key;
    return &key;
}

auto toolbar_button_item_index_key() -> void const* {
    static char key;
    return &key;
}

auto toolbar_button_opens_menu_key() -> void const* {
    static char key;
    return &key;
}

auto toolbar_button_opens_overflow_menu_key() -> void const* {
    static char key;
    return &key;
}

auto toolbar_button_opens_search_key() -> void const* {
    static char key;
    return &key;
}

auto toolbar_search_field_key() -> void const* {
    static char key;
    return &key;
}

auto toolbar_button_menu_item_index_key() -> void const* {
    static char key;
    return &key;
}

auto toolbar_frame_observer_key() -> void const* {
    static char key;
    return &key;
}

auto traffic_light_resize_observer_key() -> void const* {
    static char key;
    return &key;
}

auto toolbar_separator_leading_button_key() -> void const* {
    static char key;
    return &key;
}

auto toolbar_separator_trailing_button_key() -> void const* {
    static char key;
    return &key;
}

struct ToolbarLayoutState {
    id view = nil;
    id window = nil;
    WindowPadding padding = {};
    WindowToolbar toolbar = {};
    std::vector<std::size_t> trailing_compact_stages;
    CGSize minimum_size = {};
    bool search_expanded = false;
    bool layouting = false;
    bool layout_pending = false;
    std::string search_text;
    CGFloat group_y = 0.0;
    CGFloat title_y = 0.0;
};

struct TrafficLightButtonLayoutState {
    bool initialized = false;
    CGPoint base_origin = {};
    CGPoint applied_origin = {};
};

struct TrafficLightLayoutState {
    id window = nil;
    WindowPadding padding = {};
    std::array<TrafficLightButtonLayoutState, 3> buttons = {};
};

enum class ToolbarGroupSide : unsigned long long {
    leading = 0,
    trailing = 1,
};

struct ToolbarGroupBinding {
    ToolbarGroupSide side = ToolbarGroupSide::leading;
    std::size_t group_index = 0;
    bool selectable = false;
    std::optional<std::size_t> compact_item_index;
};

struct ToolbarButtonMenuState {
    id button = nil;
    std::vector<ToolbarMenuItem> menu_items;
};

struct ToolbarButtonBindingState {
    id toolbar_view = nil;
    ToolbarGroupSide side = ToolbarGroupSide::leading;
    std::size_t group_index = 0;
    std::size_t item_index = 0;
};

auto toolbar_layout_states() -> std::vector<ToolbarLayoutState>& {
    static auto states = std::vector<ToolbarLayoutState>{};
    return states;
}

auto toolbar_button_menu_states() -> std::vector<ToolbarButtonMenuState>& {
    static auto states = std::vector<ToolbarButtonMenuState>{};
    return states;
}

auto traffic_light_layout_states() -> std::vector<TrafficLightLayoutState>& {
    static auto states = std::vector<TrafficLightLayoutState>{};
    return states;
}

auto toolbar_layout_state(id view) -> ToolbarLayoutState* {
    auto& states = toolbar_layout_states();
    auto found = std::ranges::find_if(
        states,
        [&](ToolbarLayoutState const& state) {
            return state.view == view;
        });
    return found == states.end() ? nullptr : std::addressof(*found);
}

void set_toolbar_layout_state(id view, ToolbarLayoutState state) {
    auto& states = toolbar_layout_states();
    std::erase_if(
        states,
        [&](ToolbarLayoutState const& existing) {
            return existing.view == view;
        });
    state.view = view;
    states.push_back(std::move(state));
}

void set_toolbar_button_menu_items(
    id button,
    std::vector<ToolbarMenuItem> const& menu_items) {
    auto& states = toolbar_button_menu_states();
    std::erase_if(
        states,
        [&](ToolbarButtonMenuState const& existing) {
            return existing.button == button;
        });
    if (button && !menu_items.empty()) {
        states.push_back(ToolbarButtonMenuState{
            .button = button,
            .menu_items = menu_items,
        });
    }
}

auto toolbar_button_menu_items(id button)
    -> std::vector<ToolbarMenuItem> const* {
    auto& states = toolbar_button_menu_states();
    auto found = std::ranges::find_if(
        states,
        [&](ToolbarButtonMenuState const& state) {
            return state.button == button;
        });
    return found == states.end()
        ? nullptr
        : std::addressof(found->menu_items);
}

auto traffic_light_layout_state(id window) -> TrafficLightLayoutState& {
    auto& states = traffic_light_layout_states();
    auto found = std::ranges::find_if(
        states,
        [&](TrafficLightLayoutState const& state) {
            return state.window == window;
        });
    if (found == states.end()) {
        states.push_back(TrafficLightLayoutState{.window = window});
        return states.back();
    }
    return *found;
}

auto nearly_equal(CGPoint lhs, CGPoint rhs) -> bool {
    constexpr CGFloat epsilon = 0.5;
    return std::abs(lhs.x - rhs.x) < epsilon
        && std::abs(lhs.y - rhs.y) < epsilon;
}

void apply_traffic_light_padding(id window, WindowPadding const& padding);
void layout_toolbar_view(id toolbar_view);

constexpr auto operator|(AppKitWindowStyle lhs,
                         AppKitWindowStyle rhs) -> NSUInteger {
    return static_cast<NSUInteger>(lhs) | static_cast<NSUInteger>(rhs);
}

constexpr auto operator|(NSUInteger lhs,
                         AppKitWindowStyle rhs) -> NSUInteger {
    return lhs | static_cast<NSUInteger>(rhs);
}

constexpr auto operator|=(NSUInteger& lhs,
                          AppKitWindowStyle rhs) -> NSUInteger& {
    lhs = lhs | rhs;
    return lhs;
}

constexpr auto operator|(AppKitAutoresizingMask lhs,
                         AppKitAutoresizingMask rhs) -> NSUInteger {
    return static_cast<NSUInteger>(lhs) | static_cast<NSUInteger>(rhs);
}

constexpr auto operator|(NSUInteger lhs,
                         AppKitAutoresizingMask rhs) -> NSUInteger {
    return lhs | static_cast<NSUInteger>(rhs);
}

template <typename Ret, typename... Args>
auto send(id receiver, SEL selector, Args... args) -> Ret {
    using Fn = Ret (*)(id, SEL, Args...);
    return reinterpret_cast<Fn>(objc_msgSend)(receiver, selector, args...);
}

template <typename Ret, typename... Args>
auto send_super(id receiver, Class superclass, SEL selector, Args... args)
    -> Ret {
    auto super = objc_super{
        .receiver = receiver,
        .super_class = superclass,
    };
    using Fn = Ret (*)(objc_super*, SEL, Args...);
    return reinterpret_cast<Fn>(objc_msgSendSuper)(&super, selector, args...);
}

auto cls(char const* name) -> id {
    return reinterpret_cast<id>(objc_getClass(name));
}

auto sel(char const* name) -> SEL {
    return sel_registerName(name);
}

auto ns_string(std::string const& value) -> id {
    return send<id>(
        cls("NSString"),
        sel("stringWithUTF8String:"),
        value.c_str());
}

auto string_value(id value) -> std::string {
    if (!value)
        return {};
    auto text = send<char const*>(value, sel("UTF8String"));
    return text ? std::string{text} : std::string{};
}

auto objc_bool(bool value) -> ObjcBool {
    return static_cast<ObjcBool>(value ? 1 : 0);
}

auto ns_number(unsigned long long value) -> id {
    return send<id>(
        cls("NSNumber"),
        sel("numberWithUnsignedLongLong:"),
        value);
}

auto ns_number(bool value) -> id {
    return send<id>(
        cls("NSNumber"),
        sel("numberWithBool:"),
        objc_bool(value));
}

auto number_unsigned_value(id number) -> unsigned long long {
    return number
        ? send<unsigned long long>(number, sel("unsignedLongLongValue"))
        : 0ull;
}

auto number_bool_value(id number) -> bool {
    return number
        && send<ObjcBool>(number, sel("boolValue")) != 0;
}

class OwnedObjcObject {
public:
    OwnedObjcObject() = default;

    explicit OwnedObjcObject(id object) : object_{object} {}

    OwnedObjcObject(OwnedObjcObject const&) = delete;
    auto operator=(OwnedObjcObject const&) -> OwnedObjcObject& = delete;

    OwnedObjcObject(OwnedObjcObject&& other) noexcept
        : object_{other.object_} {
        other.object_ = nil;
    }

    auto operator=(OwnedObjcObject&& other) noexcept -> OwnedObjcObject& {
        if (this == &other)
            return *this;
        reset();
        object_ = other.object_;
        other.object_ = nil;
        return *this;
    }

    ~OwnedObjcObject() {
        reset();
    }

    explicit operator bool() const {
        return object_ != nil;
    }

    auto get() const -> id {
        return object_;
    }

private:
    void reset() {
        if (object_)
            send<void>(object_, sel("release"));
        object_ = nil;
    }

    id object_ = nil;
};

template <typename T>
class OwnedCfObject {
public:
    OwnedCfObject() = default;

    explicit OwnedCfObject(T object) : object_{object} {}

    OwnedCfObject(OwnedCfObject const&) = delete;
    auto operator=(OwnedCfObject const&) -> OwnedCfObject& = delete;

    OwnedCfObject(OwnedCfObject&& other) noexcept
        : object_{other.object_} {
        other.object_ = nullptr;
    }

    auto operator=(OwnedCfObject&& other) noexcept -> OwnedCfObject& {
        if (this == &other)
            return *this;
        reset();
        object_ = other.object_;
        other.object_ = nullptr;
        return *this;
    }

    ~OwnedCfObject() {
        reset();
    }

    explicit operator bool() const {
        return object_ != nullptr;
    }

    auto get() const -> T {
        return object_;
    }

    void reset(T object = nullptr) {
        if (object_)
            CFRelease(object_);
        object_ = object;
    }

private:
    T object_ = nullptr;
};

auto responds_to(id receiver, char const* selector) -> bool {
    return receiver
        && send<ObjcBool>(
            receiver,
            sel("respondsToSelector:"),
            sel(selector)) != 0;
}

auto window_for(id receiver) -> id {
    if (!receiver)
        return nil;
    if (responds_to(receiver, "window"))
        return send<id>(receiver, sel("window"));
    return receiver;
}

void resign_first_responder(id receiver) {
    auto window = window_for(receiver);
    if (window && responds_to(window, "makeFirstResponder:"))
        send<void>(window, sel("makeFirstResponder:"), nil);
}

void collapse_empty_toolbar_search(id receiver);

auto class_object(char const* name) -> id {
    return reinterpret_cast<id>(objc_getClass(name));
}

auto color_with_white(double white, double alpha) -> id {
    return send<id>(
        class_object("NSColor"),
        sel("colorWithCalibratedWhite:alpha:"),
        white,
        alpha);
}

auto color_with_rgba(double red,
                     double green,
                     double blue,
                     double alpha) -> id {
    return send<id>(
        class_object("NSColor"),
        sel("colorWithCalibratedRed:green:blue:alpha:"),
        red,
        green,
        blue,
        alpha);
}

auto label_color() -> id {
    return send<id>(class_object("NSColor"), sel("labelColor"));
}

auto control_background_color() -> id {
    return send<id>(class_object("NSColor"), sel("controlBackgroundColor"));
}

auto color_with_alpha(id color, double alpha) -> id {
    return color == nil
        ? nil
        : send<id>(color, sel("colorWithAlphaComponent:"), alpha);
}

auto toolbar_separator_color() -> id {
    return color_with_alpha(label_color(), 0.08);
}

void configure_layer(id view,
                     double corner_radius,
                     id background,
                     id border,
                     double border_width = 1.0) {
    if (!view)
        return;
    send<void>(view, sel("setWantsLayer:"), objc_bool(true));
    auto layer = send<id>(view, sel("layer"));
    if (!layer)
        return;
    send<void>(layer, sel("setCornerRadius:"), corner_radius);
    if (responds_to(layer, "setMasksToBounds:"))
        send<void>(layer, sel("setMasksToBounds:"), objc_bool(true));
    if (background) {
        auto cg_color = send<id>(background, sel("CGColor"));
        send<void>(layer, sel("setBackgroundColor:"), cg_color);
    }
    if (border) {
        auto cg_color = send<id>(border, sel("CGColor"));
        send<void>(layer, sel("setBorderColor:"), cg_color);
        send<void>(layer, sel("setBorderWidth:"), border_width);
    }
}

void set_layer_background(id view, id color) {
    if (!view)
        return;
    auto layer = send<id>(view, sel("layer"));
    if (!layer)
        return;
    send<void>(
        layer,
        sel("setBackgroundColor:"),
        color ? send<id>(color, sel("CGColor")) : nil);
}

void configure_shadow(id view,
                      double opacity,
                      double radius,
                      CGSize offset) {
    if (!view)
        return;
    send<void>(view, sel("setWantsLayer:"), objc_bool(true));
    auto layer = send<id>(view, sel("layer"));
    if (!layer)
        return;
    if (responds_to(layer, "setMasksToBounds:"))
        send<void>(layer, sel("setMasksToBounds:"), objc_bool(false));
    send<void>(
        layer,
        sel("setShadowColor:"),
        send<id>(color_with_white(0.0, 1.0), sel("CGColor")));
    send<void>(
        layer,
        sel("setShadowOpacity:"),
        static_cast<float>(opacity));
    send<void>(layer, sel("setShadowRadius:"), radius);
    send<void>(layer, sel("setShadowOffset:"), offset);
}

auto toolbar_button_background(bool selected, bool hovering) -> id {
    if (hovering)
        return color_with_white(0.0, selected ? 0.22 : 0.08);
    return selected ? color_with_white(0.0, 0.16) : nil;
}

auto toolbar_group_selected_index(ToolbarGroup const& group)
    -> std::optional<std::size_t> {
    for (auto index = std::size_t{0}; index < group.buttons.size(); ++index) {
        if (group.buttons[index].selected)
            return index;
    }
    return std::nullopt;
}

auto toolbar_group_selectable(ToolbarGroup const& group) -> bool {
    return toolbar_group_selected_index(group).has_value();
}

auto toolbar_source_group(ToolbarLayoutState& state,
                          ToolbarGroupSide side,
                          std::size_t group_index) -> ToolbarGroup* {
    auto& groups = side == ToolbarGroupSide::trailing
        ? state.toolbar.trailing_groups
        : state.toolbar.leading_groups;
    return group_index < groups.size() ? std::addressof(groups[group_index])
                                       : nullptr;
}

void bind_toolbar_button(id button,
                         id toolbar_view,
                         ToolbarGroupBinding const& binding,
                         std::size_t item_index) {
    if (!button)
        return;
    objc_setAssociatedObject(
        button,
        toolbar_button_toolbar_view_key(),
        toolbar_view,
        OBJC_ASSOCIATION_ASSIGN);
    objc_setAssociatedObject(
        button,
        toolbar_button_group_side_key(),
        ns_number(static_cast<unsigned long long>(binding.side)),
        OBJC_ASSOCIATION_RETAIN_NONATOMIC);
    objc_setAssociatedObject(
        button,
        toolbar_button_group_index_key(),
        ns_number(static_cast<unsigned long long>(binding.group_index)),
        OBJC_ASSOCIATION_RETAIN_NONATOMIC);
    objc_setAssociatedObject(
        button,
        toolbar_button_item_index_key(),
        ns_number(static_cast<unsigned long long>(item_index)),
        OBJC_ASSOCIATION_RETAIN_NONATOMIC);
}

auto toolbar_button_binding(id button)
    -> std::optional<ToolbarButtonBindingState> {
    auto toolbar_view = static_cast<id>(
        objc_getAssociatedObject(button, toolbar_button_toolbar_view_key()));
    if (!toolbar_view)
        return std::nullopt;

    return ToolbarButtonBindingState{
        .toolbar_view = toolbar_view,
        .side = static_cast<ToolbarGroupSide>(
            number_unsigned_value(
                objc_getAssociatedObject(
                    button,
                    toolbar_button_group_side_key()))),
        .group_index = static_cast<std::size_t>(
            number_unsigned_value(
                objc_getAssociatedObject(
                    button,
                    toolbar_button_group_index_key()))),
        .item_index = static_cast<std::size_t>(
            number_unsigned_value(
                objc_getAssociatedObject(
                    button,
                    toolbar_button_item_index_key()))),
    };
}

auto toolbar_button_matches_binding(
    id candidate,
    ToolbarButtonBindingState const& binding) -> bool {
    auto candidate_binding = toolbar_button_binding(candidate);
    return candidate_binding
        && candidate_binding->toolbar_view == binding.toolbar_view
        && candidate_binding->side == binding.side
        && candidate_binding->group_index == binding.group_index
        && candidate_binding->item_index == binding.item_index;
}

auto find_toolbar_button(id container,
                         ToolbarButtonBindingState const& binding) -> id {
    if (!container)
        return nil;
    if (toolbar_button_matches_binding(container, binding))
        return container;
    if (!responds_to(container, "subviews"))
        return nil;

    auto subviews = send<id>(container, sel("subviews"));
    auto const count = subviews ? send<NSUInteger>(subviews, sel("count")) : 0u;
    for (auto index = NSUInteger{0}; index < count; ++index) {
        auto subview = send<id>(subviews, sel("objectAtIndex:"), index);
        if (auto found = find_toolbar_button(subview, binding))
            return found;
    }
    return nil;
}

auto collapse_empty_toolbar_search_for_button(id button) -> id {
    auto binding = toolbar_button_binding(button);
    collapse_empty_toolbar_search(button);
    if (!binding)
        return button;
    if (auto current = find_toolbar_button(binding->toolbar_view, *binding))
        return current;
    return button;
}

auto toolbar_button_opens_menu(id button) -> bool {
    return number_bool_value(
        objc_getAssociatedObject(button, toolbar_button_opens_menu_key()));
}

auto toolbar_button_opens_overflow_menu(id button) -> bool {
    return number_bool_value(
        objc_getAssociatedObject(
            button,
            toolbar_button_opens_overflow_menu_key()));
}

auto toolbar_button_opens_search(id button) -> bool {
    return number_bool_value(
        objc_getAssociatedObject(button, toolbar_button_opens_search_key()));
}

void expand_toolbar_search(id button) {
    auto toolbar_view = static_cast<id>(
        objc_getAssociatedObject(button, toolbar_button_toolbar_view_key()));
    auto state = toolbar_layout_state(toolbar_view);
    if (!state)
        return;

    state->search_expanded = true;
    layout_toolbar_view(toolbar_view);
}

void select_toolbar_item(id toolbar_view,
                         ToolbarGroupSide side,
                         std::size_t group_index,
                         std::size_t item_index) {
    auto state = toolbar_layout_state(toolbar_view);
    if (!state)
        return;

    auto group = toolbar_source_group(*state, side, group_index);
    if (!group || !toolbar_group_selectable(*group)
        || item_index >= group->buttons.size()) {
        return;
    }

    for (auto& item : group->buttons)
        item.selected = false;
    group->buttons[item_index].selected = true;
    layout_toolbar_view(toolbar_view);
}

auto toolbar_source_button(ToolbarLayoutState& state,
                           ToolbarGroupSide side,
                           std::size_t group_index,
                           std::size_t item_index) -> ToolbarIconButton* {
    auto group = toolbar_source_group(state, side, group_index);
    if (!group || item_index >= group->buttons.size())
        return nullptr;
    return std::addressof(group->buttons[item_index]);
}

void bind_toolbar_menu_item(id item,
                            id button,
                            ToolbarGroupSide side,
                            std::size_t group_index,
                            std::size_t item_index) {
    objc_setAssociatedObject(
        item,
        toolbar_button_group_side_key(),
        ns_number(static_cast<unsigned long long>(side)),
        OBJC_ASSOCIATION_RETAIN_NONATOMIC);
    objc_setAssociatedObject(
        item,
        toolbar_button_group_index_key(),
        ns_number(static_cast<unsigned long long>(group_index)),
        OBJC_ASSOCIATION_RETAIN_NONATOMIC);
    objc_setAssociatedObject(
        item,
        toolbar_button_item_index_key(),
        ns_number(static_cast<unsigned long long>(item_index)),
        OBJC_ASSOCIATION_RETAIN_NONATOMIC);
    send<void>(item, sel("setTarget:"), button);
    send<void>(item, sel("setAction:"), sel("toolbarMenuItemPressed:"));
}

void bind_toolbar_custom_menu_item(id item,
                                   id button,
                                   ToolbarGroupSide side,
                                   std::size_t group_index,
                                   std::size_t button_index,
                                   std::size_t menu_item_index) {
    bind_toolbar_menu_item(item, button, side, group_index, button_index);
    objc_setAssociatedObject(
        item,
        toolbar_button_menu_item_index_key(),
        ns_number(static_cast<unsigned long long>(menu_item_index)),
        OBJC_ASSOCIATION_RETAIN_NONATOMIC);
}

void select_toolbar_custom_menu_item(id button, id item) {
    auto toolbar_view = static_cast<id>(
        objc_getAssociatedObject(button, toolbar_button_toolbar_view_key()));
    auto state = toolbar_layout_state(toolbar_view);
    if (!state)
        return;

    auto const side = static_cast<ToolbarGroupSide>(
        number_unsigned_value(
            objc_getAssociatedObject(item, toolbar_button_group_side_key())));
    auto const group_index = static_cast<std::size_t>(
        number_unsigned_value(
            objc_getAssociatedObject(item, toolbar_button_group_index_key())));
    auto const button_index = static_cast<std::size_t>(
        number_unsigned_value(
            objc_getAssociatedObject(item, toolbar_button_item_index_key())));
    auto const menu_item_index = static_cast<std::size_t>(
        number_unsigned_value(
            objc_getAssociatedObject(
                item,
                toolbar_button_menu_item_index_key())));
    auto source_button =
        toolbar_source_button(*state, side, group_index, button_index);
    if (!source_button || menu_item_index >= source_button->menu_items.size())
        return;
    if (!source_button->menu_items[menu_item_index].selectable)
        return;

    for (auto& menu_item : source_button->menu_items)
        menu_item.selected = false;
    source_button->menu_items[menu_item_index].selected = true;
    layout_toolbar_view(toolbar_view);
}

void select_toolbar_menu_item(id button, id item) {
    if (objc_getAssociatedObject(item, toolbar_button_menu_item_index_key())) {
        select_toolbar_custom_menu_item(button, item);
        return;
    }

    auto toolbar_view = static_cast<id>(
        objc_getAssociatedObject(button, toolbar_button_toolbar_view_key()));
    auto const side = static_cast<ToolbarGroupSide>(
        number_unsigned_value(
            objc_getAssociatedObject(item, toolbar_button_group_side_key())));
    auto const group_index = static_cast<std::size_t>(
        number_unsigned_value(
            objc_getAssociatedObject(item, toolbar_button_group_index_key())));
    auto const item_index = static_cast<std::size_t>(
        number_unsigned_value(
            objc_getAssociatedObject(item, toolbar_button_item_index_key())));
    select_toolbar_item(toolbar_view, side, group_index, item_index);
}

void add_menu_separator(id menu) {
    if (!menu)
        return;
    auto separator =
        send<id>(class_object("NSMenuItem"), sel("separatorItem"));
    send<void>(menu, sel("addItem:"), separator);
}

auto make_menu_item(std::string const& title,
                    char const* action,
                    std::string const& key_equivalent)
    -> OwnedObjcObject {
    auto selector = action ? sel(action) : static_cast<SEL>(nullptr);
    auto item = OwnedObjcObject{send<id>(
        send<id>(class_object("NSMenuItem"), sel("alloc")),
        sel("initWithTitle:action:keyEquivalent:"),
        ns_string(title),
        selector,
        ns_string(key_equivalent))};
    if (item && !key_equivalent.empty()
        && responds_to(item.get(), "setKeyEquivalentModifierMask:")) {
        send<void>(
            item.get(),
            sel("setKeyEquivalentModifierMask:"),
            event_modifier_flag_command);
    }
    return item;
}

void add_standard_menu_item(id menu,
                            std::string const& title,
                            char const* action,
                            std::string const& key_equivalent) {
    auto item = make_menu_item(title, action, key_equivalent);
    if (item)
        send<void>(menu, sel("addItem:"), item.get());
}

void install_standard_main_menu(id app, std::string const& app_name) {
    auto main_menu = OwnedObjcObject{send<id>(
        send<id>(class_object("NSMenu"), sel("alloc")),
        sel("initWithTitle:"),
        ns_string(""))};
    if (!main_menu)
        return;

    auto app_menu = OwnedObjcObject{send<id>(
        send<id>(class_object("NSMenu"), sel("alloc")),
        sel("initWithTitle:"),
        ns_string(app_name))};
    auto app_menu_item = make_menu_item(app_name, nullptr, "");
    if (app_menu && app_menu_item) {
        add_standard_menu_item(
            app_menu.get(),
            "Quit " + app_name,
            "terminate:",
            "q");
        send<void>(main_menu.get(), sel("addItem:"), app_menu_item.get());
        send<void>(
            main_menu.get(),
            sel("setSubmenu:forItem:"),
            app_menu.get(),
            app_menu_item.get());
    }

    auto edit_menu = OwnedObjcObject{send<id>(
        send<id>(class_object("NSMenu"), sel("alloc")),
        sel("initWithTitle:"),
        ns_string("Edit"))};
    auto edit_menu_item = make_menu_item("Edit", nullptr, "");
    if (edit_menu && edit_menu_item) {
        add_standard_menu_item(edit_menu.get(), "Cut", "cut:", "x");
        add_standard_menu_item(edit_menu.get(), "Copy", "copy:", "c");
        add_standard_menu_item(edit_menu.get(), "Paste", "paste:", "v");
        add_menu_separator(edit_menu.get());
        add_standard_menu_item(
            edit_menu.get(),
            "Select All",
            "selectAll:",
            "a");
        send<void>(main_menu.get(), sel("addItem:"), edit_menu_item.get());
        send<void>(
            main_menu.get(),
            sel("setSubmenu:forItem:"),
            edit_menu.get(),
            edit_menu_item.get());
    }

    send<void>(app, sel("setMainMenu:"), main_menu.get());
}

auto make_toolbar_menu(std::string const& title) -> OwnedObjcObject;
void add_toolbar_submenu(id menu,
                         std::string const& title,
                         id submenu);

void add_toolbar_command_menu_items(id menu,
                                    std::span<ToolbarMenuItem const> items) {
    for (auto const& source_item : items) {
        if (source_item.separator_before)
            add_menu_separator(menu);

        if (!source_item.submenu_items.empty()) {
            auto submenu = make_toolbar_menu(source_item.label);
            if (!submenu)
                continue;
            add_toolbar_command_menu_items(
                submenu.get(),
                source_item.submenu_items);
            add_toolbar_submenu(menu, source_item.label, submenu.get());
            continue;
        }

        auto menu_item = OwnedObjcObject{send<id>(
            send<id>(class_object("NSMenuItem"), sel("alloc")),
            sel("initWithTitle:action:keyEquivalent:"),
            ns_string(source_item.label),
            nil,
            ns_string(""))};
        if (!menu_item)
            continue;
        send<void>(
            menu_item.get(),
            sel("setEnabled:"),
            objc_bool(source_item.enabled));
        send<void>(menu, sel("addItem:"), menu_item.get());
    }
}

void add_toolbar_button_menu_items(id menu,
                                   id button,
                                   ToolbarGroupSide side,
                                   std::size_t group_index,
                                   std::size_t button_index,
                                   ToolbarIconButton const& source_button) {
    for (auto index = std::size_t{0};
         index < source_button.menu_items.size();
         ++index) {
        auto const& source_item = source_button.menu_items[index];
        if (source_item.separator_before)
            add_menu_separator(menu);

        if (!source_item.submenu_items.empty()) {
            auto submenu = make_toolbar_menu(source_item.label);
            if (!submenu)
                continue;
            add_toolbar_command_menu_items(
                submenu.get(),
                source_item.submenu_items);
            add_toolbar_submenu(menu, source_item.label, submenu.get());
            continue;
        }

        auto menu_item = OwnedObjcObject{send<id>(
            send<id>(class_object("NSMenuItem"), sel("alloc")),
            sel("initWithTitle:action:keyEquivalent:"),
            ns_string(source_item.label),
            sel("toolbarMenuItemPressed:"),
            ns_string(""))};
        if (!menu_item)
            continue;
        bind_toolbar_custom_menu_item(
            menu_item.get(),
            button,
            side,
            group_index,
            button_index,
            index);
        send<void>(
            menu_item.get(),
            sel("setState:"),
            static_cast<NSInteger>(
                source_item.selectable && source_item.selected ? 1 : 0));
        send<void>(
            menu_item.get(),
            sel("setEnabled:"),
            objc_bool(source_item.enabled));
        send<void>(menu, sel("addItem:"), menu_item.get());
    }
}

auto make_toolbar_menu(std::string const& title = "") -> OwnedObjcObject {
    auto menu = OwnedObjcObject{send<id>(
        send<id>(class_object("NSMenu"), sel("alloc")),
        sel("initWithTitle:"),
        ns_string(title))};
    if (menu && responds_to(menu.get(), "setAutoenablesItems:")) {
        send<void>(
            menu.get(),
            sel("setAutoenablesItems:"),
            objc_bool(false));
    }
    return menu;
}

void add_toolbar_submenu(id menu,
                         std::string const& title,
                         id submenu) {
    auto menu_item = OwnedObjcObject{send<id>(
        send<id>(class_object("NSMenuItem"), sel("alloc")),
        sel("initWithTitle:action:keyEquivalent:"),
        ns_string(title),
        nil,
        ns_string(""))};
    if (!menu_item)
        return;
    send<void>(menu, sel("addItem:"), menu_item.get());
    send<void>(
        menu,
        sel("setSubmenu:forItem:"),
        submenu,
        menu_item.get());
}

void add_toolbar_group_menu_items(id menu,
                                  id button,
                                  ToolbarGroupSide side,
                                  std::size_t group_index,
                                  ToolbarGroup const& group,
                                  bool selectable) {
    if (selectable) {
        for (auto index = std::size_t{0}; index < group.buttons.size(); ++index) {
            auto const& source_item = group.buttons[index];
            auto menu_item = OwnedObjcObject{send<id>(
                send<id>(class_object("NSMenuItem"), sel("alloc")),
                sel("initWithTitle:action:keyEquivalent:"),
                ns_string(source_item.label),
                sel("toolbarMenuItemPressed:"),
                ns_string(""))};
            if (!menu_item)
                continue;
            bind_toolbar_menu_item(
                menu_item.get(),
                button,
                side,
                group_index,
                index);
            send<void>(
                menu_item.get(),
                sel("setState:"),
                static_cast<NSInteger>(source_item.selected ? 1 : 0));
            send<void>(
                menu_item.get(),
                sel("setEnabled:"),
                objc_bool(source_item.enabled));
            send<void>(menu, sel("addItem:"), menu_item.get());
        }
        return;
    }

    for (auto index = std::size_t{0}; index < group.buttons.size(); ++index) {
        auto const& source_button = group.buttons[index];
        if (!source_button.menu_items.empty()) {
            add_toolbar_button_menu_items(
                menu,
                button,
                side,
                group_index,
                index,
                source_button);
            continue;
        }

        auto menu_item = OwnedObjcObject{send<id>(
            send<id>(class_object("NSMenuItem"), sel("alloc")),
            sel("initWithTitle:action:keyEquivalent:"),
            ns_string(source_button.label),
            nil,
            ns_string(""))};
        if (!menu_item)
            continue;
        send<void>(
            menu_item.get(),
            sel("setEnabled:"),
            objc_bool(source_button.enabled));
        send<void>(menu, sel("addItem:"), menu_item.get());
    }
}

auto toolbar_group_stage_overflows(ToolbarGroup const& group,
                                   std::size_t compact_stage) -> bool {
    if (compact_stage == 0)
        return false;
    if (group.compact_button && compact_stage == 1)
        return false;

    auto const static_stage_index =
        compact_stage - (group.compact_button ? 2 : 1);
    return static_stage_index < group.compact_stages.size()
        && group.compact_stages[static_stage_index].overflow;
}

auto toolbar_overflow_group_label(ToolbarGroup const& group) -> std::string {
    if (group.compact_button && !group.compact_button->label.empty())
        return group.compact_button->label;
    if (group.buttons.size() == 1)
        return group.buttons.front().label;
    return {};
}

void add_toolbar_overflow_group(id menu,
                                id button,
                                ToolbarGroupSide side,
                                std::size_t group_index,
                                ToolbarGroup const& group) {
    if (toolbar_group_selectable(group)) {
        auto submenu = make_toolbar_menu(toolbar_overflow_group_label(group));
        if (!submenu)
            return;
        add_toolbar_group_menu_items(
            submenu.get(),
            button,
            side,
            group_index,
            group,
            true);
        add_toolbar_submenu(
            menu,
            toolbar_overflow_group_label(group),
            submenu.get());
        return;
    }

    for (auto index = std::size_t{0}; index < group.buttons.size(); ++index) {
        auto const& source_button = group.buttons[index];
        if (!source_button.menu_items.empty()) {
            auto submenu = make_toolbar_menu(source_button.label);
            if (!submenu)
                continue;
            add_toolbar_button_menu_items(
                submenu.get(),
                button,
                side,
                group_index,
                index,
                source_button);
            add_toolbar_submenu(menu, source_button.label, submenu.get());
            continue;
        }

        auto menu_item = OwnedObjcObject{send<id>(
            send<id>(class_object("NSMenuItem"), sel("alloc")),
            sel("initWithTitle:action:keyEquivalent:"),
            ns_string(source_button.label),
            nil,
            ns_string(""))};
        if (!menu_item)
            continue;
        send<void>(
            menu_item.get(),
            sel("setEnabled:"),
            objc_bool(source_button.enabled));
        send<void>(menu, sel("addItem:"), menu_item.get());
    }
}

void popup_toolbar_menu(id menu, id button) {
    auto bounds = send<CGRect>(button, sel("bounds"));
    send<ObjcBool>(
        menu,
        sel("popUpMenuPositioningItem:atLocation:inView:"),
        nil,
        CGPoint{0.0, bounds.size.height},
        button);
}

void show_toolbar_overflow_menu(id button) {
    auto toolbar_view = static_cast<id>(
        objc_getAssociatedObject(button, toolbar_button_toolbar_view_key()));
    auto state = toolbar_layout_state(toolbar_view);
    if (!state)
        return;

    auto menu = make_toolbar_menu();
    if (!menu)
        return;

    auto added_group = false;
    for (auto group_index = std::size_t{0};
         group_index < state->toolbar.trailing_groups.size()
         && group_index < state->trailing_compact_stages.size();
         ++group_index) {
        auto const& group = state->toolbar.trailing_groups[group_index];
        auto const compact_stage = state->trailing_compact_stages[group_index];
        if (!toolbar_group_stage_overflows(group, compact_stage))
            continue;

        add_toolbar_overflow_group(
            menu.get(),
            button,
            ToolbarGroupSide::trailing,
            group_index,
            group);
        added_group = true;
    }

    if (added_group)
        popup_toolbar_menu(menu.get(), button);
}

void show_toolbar_button_menu(id button) {
    if (toolbar_button_opens_overflow_menu(button)) {
        show_toolbar_overflow_menu(button);
        return;
    }

    auto toolbar_view = static_cast<id>(
        objc_getAssociatedObject(button, toolbar_button_toolbar_view_key()));
    auto state = toolbar_layout_state(toolbar_view);
    if (!state)
        return;

    auto const side = static_cast<ToolbarGroupSide>(
        number_unsigned_value(
            objc_getAssociatedObject(button, toolbar_button_group_side_key())));
    auto const group_index = static_cast<std::size_t>(
        number_unsigned_value(
            objc_getAssociatedObject(button, toolbar_button_group_index_key())));
    auto const button_index = static_cast<std::size_t>(
        number_unsigned_value(
            objc_getAssociatedObject(button, toolbar_button_item_index_key())));
    auto group = toolbar_source_group(*state, side, group_index);
    auto source_button =
        toolbar_source_button(*state, side, group_index, button_index);
    auto rendered_menu_items = toolbar_button_menu_items(button);
    if (rendered_menu_items
        && (!source_button || source_button->menu_items.empty())) {
        auto menu = make_toolbar_menu();
        if (!menu)
            return;
        add_toolbar_command_menu_items(menu.get(), *rendered_menu_items);
        popup_toolbar_menu(menu.get(), button);
        return;
    }
    if (!group || (!source_button || source_button->menu_items.empty())
        && !toolbar_group_selectable(*group)) {
        return;
    }

    auto menu = make_toolbar_menu();
    if (!menu)
        return;

    if (source_button && !source_button->menu_items.empty()) {
        add_toolbar_button_menu_items(
            menu.get(),
            button,
            side,
            group_index,
            button_index,
            *source_button);
    } else {
        add_toolbar_group_menu_items(
            menu.get(),
            button,
            side,
            group_index,
            *group,
            toolbar_group_selectable(*group));
    }

    popup_toolbar_menu(menu.get(), button);
}

void select_toolbar_button(id button) {
    if (!button || send<ObjcBool>(button, sel("isEnabled")) == 0)
        return;
    resign_first_responder(button);
    if (toolbar_button_opens_search(button)) {
        expand_toolbar_search(button);
        return;
    }
    if (toolbar_button_opens_menu(button)) {
        show_toolbar_button_menu(collapse_empty_toolbar_search_for_button(button));
        return;
    }

    auto toolbar_view = static_cast<id>(
        objc_getAssociatedObject(button, toolbar_button_toolbar_view_key()));
    auto const side = static_cast<ToolbarGroupSide>(
        number_unsigned_value(
            objc_getAssociatedObject(button, toolbar_button_group_side_key())));
    auto const group_index = static_cast<std::size_t>(
        number_unsigned_value(
            objc_getAssociatedObject(button, toolbar_button_group_index_key())));
    auto const item_index = static_cast<std::size_t>(
        number_unsigned_value(
            objc_getAssociatedObject(button, toolbar_button_item_index_key())));
    select_toolbar_item(toolbar_view, side, group_index, item_index);
    collapse_empty_toolbar_search(button);
}

auto toolbar_button_selected(id button) -> bool {
    return button
        && send<NSInteger>(button, sel("tag")) == toolbar_button_selected_tag;
}

void update_toolbar_button_background(id button, bool hovering) {
    set_layer_background(
        button,
        toolbar_button_background(toolbar_button_selected(button), hovering));
}

auto toolbar_separator_hidden_by_selection(id separator) -> bool {
    auto leading_button = static_cast<id>(
        objc_getAssociatedObject(
            separator,
            toolbar_separator_leading_button_key()));
    auto trailing_button = static_cast<id>(
        objc_getAssociatedObject(
            separator,
            toolbar_separator_trailing_button_key()));
    return toolbar_button_selected(leading_button)
        || toolbar_button_selected(trailing_button);
}

void set_toolbar_separator_hidden(id separator, bool hidden) {
    if (!separator)
        return;
    send<void>(
        separator,
        sel("setHidden:"),
        objc_bool(hidden || toolbar_separator_hidden_by_selection(separator)));
}

void set_toolbar_button_separators_hidden(id button, bool hidden) {
    for (auto key : {
             toolbar_leading_separator_key(),
             toolbar_trailing_separator_key(),
         }) {
        auto separator = static_cast<id>(objc_getAssociatedObject(button, key));
        set_toolbar_separator_hidden(separator, hidden);
    }
}

auto toolbar_button_class() -> Class {
    static Class button_class = [] {
        auto superclass = static_cast<Class>(objc_getClass("NSButton"));
        auto created =
            objc_allocateClassPair(superclass, "PhenotypeToolbarButton", 0);
        class_addMethod(
            created,
            sel("mouseDown:"),
            reinterpret_cast<IMP>(
                +[](id button, SEL, id event) {
                    resign_first_responder(button);
                    send_super<void>(
                        button,
                        class_getSuperclass(object_getClass(button)),
                        sel("mouseDown:"),
                        event);
                }),
            "v@:@");
        class_addMethod(
            created,
            sel("updateTrackingAreas"),
            reinterpret_cast<IMP>(
                +[](id button, SEL) {
                    auto tracking_area = static_cast<id>(
                        objc_getAssociatedObject(
                            button,
                            toolbar_tracking_area_key()));
                    if (tracking_area)
                        send<void>(
                            button,
                            sel("removeTrackingArea:"),
                            tracking_area);

                    auto bounds = send<CGRect>(button, sel("bounds"));
                    auto const options =
                        tracking_mouse_entered_and_exited
                        | tracking_active_in_active_app
                        | tracking_in_visible_rect;
                    auto area = send<id>(
                        send<id>(
                            class_object("NSTrackingArea"),
                            sel("alloc")),
                        sel("initWithRect:options:owner:userInfo:"),
                        bounds,
                        options,
                        button,
                        nil);
                    if (!area)
                        return;

                    send<void>(button, sel("addTrackingArea:"), area);
                    objc_setAssociatedObject(
                        button,
                        toolbar_tracking_area_key(),
                        area,
                        OBJC_ASSOCIATION_RETAIN_NONATOMIC);
                    send<void>(area, sel("release"));
                }),
            "v@:");
        class_addMethod(
            created,
            sel("mouseEntered:"),
            reinterpret_cast<IMP>(
                +[](id button, SEL, id) {
                    if (send<ObjcBool>(button, sel("isEnabled")) == 0)
                        return;
                    update_toolbar_button_background(button, true);
                    set_toolbar_button_separators_hidden(button, true);
                }),
            "v@:@");
        class_addMethod(
            created,
            sel("mouseExited:"),
            reinterpret_cast<IMP>(
                +[](id button, SEL, id) {
                    update_toolbar_button_background(button, false);
                    set_toolbar_button_separators_hidden(button, false);
                }),
            "v@:@");
        class_addMethod(
            created,
            sel("toolbarButtonPressed:"),
            reinterpret_cast<IMP>(
                +[](id button, SEL, id) {
                    select_toolbar_button(button);
                }),
            "v@:@");
        class_addMethod(
            created,
            sel("toolbarMenuItemPressed:"),
            reinterpret_cast<IMP>(
                +[](id button, SEL, id item) {
                    select_toolbar_menu_item(button, item);
                }),
            "v@:@");
        objc_registerClassPair(created);
        return created;
    }();
    return button_class;
}

auto resource_path(std::filesystem::path const& relative) -> std::string {
    std::error_code error;
    if (std::filesystem::exists(relative, error))
        return relative.lexically_normal().string();

    auto const cwd = std::filesystem::current_path();
    for (auto base = cwd; !base.empty(); base = base.parent_path()) {
        auto const candidate = base / relative;
        error.clear();
        if (std::filesystem::exists(candidate, error))
            return candidate.lexically_normal().string();
        if (base == base.parent_path())
            break;
    }

    return relative.string();
}

auto icon_path(WindowToolbar const& toolbar,
               std::string const& icon) -> std::string {
    auto const filename = icon + ".svg";
    return resource_path(
        std::filesystem::path{toolbar.icon_directory} / filename);
}

auto symbol_font_path(WindowToolbar const& toolbar) -> std::string {
    return resource_path(std::filesystem::path{toolbar.symbol_font});
}

auto symbol_codepoints_path(WindowToolbar const& toolbar) -> std::string {
    return resource_path(std::filesystem::path{toolbar.symbol_codepoints});
}

constexpr auto font_axis_tag(char a,
                             char b,
                             char c,
                             char d) -> std::uint32_t {
    return (static_cast<std::uint32_t>(a) << 24u)
        | (static_cast<std::uint32_t>(b) << 16u)
        | (static_cast<std::uint32_t>(c) << 8u)
        | static_cast<std::uint32_t>(d);
}

auto cf_axis_tag(std::uint32_t tag) -> OwnedCfObject<CFNumberRef> {
    return OwnedCfObject<CFNumberRef>{CFNumberCreate(
        kCFAllocatorDefault,
        kCFNumberSInt32Type,
        &tag)};
}

void apply_font_axis(OwnedCfObject<CTFontDescriptorRef>& descriptor,
                     std::uint32_t tag,
                     CGFloat value) {
    if (!descriptor)
        return;
    auto identifier = cf_axis_tag(tag);
    if (!identifier)
        return;
    descriptor.reset(CTFontDescriptorCreateCopyWithVariation(
        descriptor.get(),
        identifier.get(),
        value));
}

auto clamp_symbol_style(MaterialSymbolStyle style) -> MaterialSymbolStyle {
    style.weight = std::clamp(style.weight, 100.0f, 700.0f);
    style.grade = std::clamp(style.grade, -50.0f, 200.0f);
    style.optical_size = std::clamp(style.optical_size, 20.0f, 48.0f);
    style.size = std::max(style.size, 1.0f);
    style.accessory_size = std::max(style.accessory_size, 1.0f);
    return style;
}

auto symbol_style(WindowToolbar const& toolbar,
                  ToolbarIconButton const& item) -> MaterialSymbolStyle {
    return clamp_symbol_style(item.symbol.value_or(toolbar.symbol));
}

struct SymbolCodepointEntry {
    std::string name;
    std::string text;
};

struct SymbolCodepointCache {
    std::string path;
    std::vector<SymbolCodepointEntry> entries;
};

auto utf8_encode(std::uint32_t codepoint) -> std::string {
    auto out = std::string{};
    if (codepoint <= 0x7fu) {
        out.push_back(static_cast<char>(codepoint));
    } else if (codepoint <= 0x7ffu) {
        out.push_back(static_cast<char>(0xc0u | (codepoint >> 6u)));
        out.push_back(static_cast<char>(0x80u | (codepoint & 0x3fu)));
    } else if (codepoint <= 0xffffu) {
        out.push_back(static_cast<char>(0xe0u | (codepoint >> 12u)));
        out.push_back(
            static_cast<char>(0x80u | ((codepoint >> 6u) & 0x3fu)));
        out.push_back(static_cast<char>(0x80u | (codepoint & 0x3fu)));
    } else {
        out.push_back(static_cast<char>(0xf0u | (codepoint >> 18u)));
        out.push_back(
            static_cast<char>(0x80u | ((codepoint >> 12u) & 0x3fu)));
        out.push_back(
            static_cast<char>(0x80u | ((codepoint >> 6u) & 0x3fu)));
        out.push_back(static_cast<char>(0x80u | (codepoint & 0x3fu)));
    }
    return out;
}

auto load_symbol_codepoints(std::string const& path)
    -> std::vector<SymbolCodepointEntry> {
    auto input = std::ifstream{path};
    auto entries = std::vector<SymbolCodepointEntry>{};
    auto name = std::string{};
    auto hex = std::string{};
    while (input >> name >> hex) {
        auto codepoint = std::uint32_t{};
        auto const result = std::from_chars(
            hex.data(),
            hex.data() + hex.size(),
            codepoint,
            16);
        if (result.ec == std::errc{})
            entries.push_back({name, utf8_encode(codepoint)});
    }
    return entries;
}

auto symbol_codepoint_entries(std::string const& path)
    -> std::vector<SymbolCodepointEntry> const& {
    static auto caches = std::vector<SymbolCodepointCache>{};
    for (auto const& cache : caches) {
        if (cache.path == path)
            return cache.entries;
    }
    auto& cache = caches.emplace_back();
    cache.path = path;
    cache.entries = load_symbol_codepoints(path);
    return cache.entries;
}

auto symbol_text(WindowToolbar const& toolbar,
                 std::string const& icon) -> std::string {
    auto const path = symbol_codepoints_path(toolbar);
    for (auto const& entry : symbol_codepoint_entries(path)) {
        if (entry.name == icon)
            return entry.text;
    }
    return icon;
}

auto register_symbol_font(WindowToolbar const& toolbar) -> bool {
    auto const path = symbol_font_path(toolbar);
    std::error_code error;
    if (!std::filesystem::exists(path, error))
        return false;

    static auto registered_paths = std::vector<std::string>{};
    if (std::ranges::find(registered_paths, path) != registered_paths.end())
        return true;

    auto url = send<id>(
        class_object("NSURL"),
        sel("fileURLWithPath:"),
        ns_string(path));
    if (!url)
        return false;

    if (!CTFontManagerIsSupportedFont(reinterpret_cast<CFURLRef>(url)))
        return false;

    auto const registered = CTFontManagerRegisterFontsForURL(
        reinterpret_cast<CFURLRef>(url),
        kCTFontManagerScopeProcess,
        nullptr);
    registered_paths.push_back(path);
    return registered != 0;
}

auto material_symbol_font(WindowToolbar const& toolbar,
                          MaterialSymbolStyle style) -> OwnedCfObject<CTFontRef> {
    if (!register_symbol_font(toolbar))
        return {};

    auto descriptor = OwnedCfObject<CTFontDescriptorRef>{
        CTFontDescriptorCreateWithNameAndSize(
            CFSTR("Material Symbols Rounded"),
            static_cast<CGFloat>(style.size))};
    apply_font_axis(
        descriptor,
        font_axis_tag('F', 'I', 'L', 'L'),
        style.fill ? 1.0 : 0.0);
    apply_font_axis(
        descriptor,
        font_axis_tag('G', 'R', 'A', 'D'),
        static_cast<CGFloat>(style.grade));
    apply_font_axis(
        descriptor,
        font_axis_tag('o', 'p', 's', 'z'),
        static_cast<CGFloat>(style.optical_size));
    apply_font_axis(
        descriptor,
        font_axis_tag('w', 'g', 'h', 't'),
        static_cast<CGFloat>(style.weight));
    if (!descriptor)
        return {};
    return OwnedCfObject<CTFontRef>{CTFontCreateWithFontDescriptor(
        descriptor.get(),
        static_cast<CGFloat>(style.size),
        nullptr)};
}

auto image_for_icon(WindowToolbar const& toolbar,
                    std::string const& icon) -> OwnedObjcObject {
    auto image = OwnedObjcObject{send<id>(
        send<id>(class_object("NSImage"), sel("alloc")),
        sel("initWithContentsOfFile:"),
        ns_string(icon_path(toolbar, icon)))};
    if (image && responds_to(image.get(), "setTemplate:"))
        send<void>(image.get(), sel("setTemplate:"), objc_bool(true));
    return image;
}

auto visual_effect_material(WindowGlassMaterial material) -> NSInteger {
    switch (material) {
    case WindowGlassMaterial::sidebar:
        return static_cast<NSInteger>(VisualEffectMaterial::sidebar);
    case WindowGlassMaterial::under_window_background:
    default:
        return static_cast<NSInteger>(
            VisualEffectMaterial::under_window_background);
    }
}

auto clamp_opacity(float opacity) -> double {
    if (opacity < 0.0f)
        return 0.0;
    if (opacity > 1.0f)
        return 1.0;
    return static_cast<double>(opacity);
}

void configure_visual_effect_view(id view, WindowGlassEffect const& effect) {
    send<void>(
        view,
        sel("setMaterial:"),
        visual_effect_material(effect.material));
    send<void>(
        view,
        sel("setBlendingMode:"),
        static_cast<NSInteger>(VisualEffectBlendingMode::behind_window));
    send<void>(
        view,
        sel("setState:"),
        static_cast<NSInteger>(VisualEffectState::active));
    send<void>(view, sel("setAlphaValue:"), clamp_opacity(effect.opacity));
    if (responds_to(view, "setEmphasized:"))
        send<void>(view, sel("setEmphasized:"), objc_bool(true));
}

auto make_view(CGRect frame) -> OwnedObjcObject {
    return OwnedObjcObject{send<id>(
        send<id>(cls("NSView"), sel("alloc")),
        sel("initWithFrame:"),
        frame)};
}

auto make_visual_effect_view(CGRect frame) -> OwnedObjcObject {
    return OwnedObjcObject{send<id>(
        send<id>(cls("NSVisualEffectView"), sel("alloc")),
        sel("initWithFrame:"),
        frame)};
}

auto focus_clearing_view_class() -> Class {
    static Class view_class = [] {
        auto superclass = static_cast<Class>(objc_getClass("NSView"));
        auto created =
            objc_allocateClassPair(superclass, "PhenotypeFocusClearingView", 0);
        class_addMethod(
            created,
            sel("acceptsFirstResponder"),
            reinterpret_cast<IMP>(
                +[](id, SEL) -> ObjcBool {
                    return objc_bool(true);
                }),
            "c@:");
        class_addMethod(
            created,
            sel("mouseDown:"),
            reinterpret_cast<IMP>(
                +[](id view, SEL, id) {
                    resign_first_responder(view);
                    auto window = window_for(view);
                    if (window)
                        send<void>(
                            window,
                            sel("makeFirstResponder:"),
                            view);
                    collapse_empty_toolbar_search(view);
                }),
            "v@:@");
        objc_registerClassPair(created);
        return created;
    }();
    return view_class;
}

auto make_focus_clearing_view(CGRect frame) -> OwnedObjcObject {
    return OwnedObjcObject{send<id>(
        send<id>(reinterpret_cast<id>(focus_clearing_view_class()), sel("alloc")),
        sel("initWithFrame:"),
        frame)};
}

auto make_text_field(CGRect frame,
                     std::string const& text,
                     double font_size,
                     bool bold = false) -> OwnedObjcObject {
    auto field = OwnedObjcObject{send<id>(
        send<id>(class_object("NSTextField"), sel("alloc")),
        sel("initWithFrame:"),
        frame)};
    if (!field)
        return field;
    send<void>(field.get(), sel("setStringValue:"), ns_string(text));
    send<void>(field.get(), sel("setBezeled:"), objc_bool(false));
    send<void>(field.get(), sel("setDrawsBackground:"), objc_bool(false));
    send<void>(field.get(), sel("setEditable:"), objc_bool(false));
    send<void>(field.get(), sel("setSelectable:"), objc_bool(false));
    send<void>(field.get(), sel("setTextColor:"), color_with_white(0.94, 1.0));
    auto font = bold
        ? send<id>(class_object("NSFont"), sel("boldSystemFontOfSize:"), font_size)
        : send<id>(class_object("NSFont"), sel("systemFontOfSize:"), font_size);
    send<void>(field.get(), sel("setFont:"), font);
    return field;
}

auto make_search_text_field(CGRect frame,
                            std::string const& placeholder,
                            std::string const& text)
    -> OwnedObjcObject {
    auto field = OwnedObjcObject{send<id>(
        send<id>(class_object("NSTextField"), sel("alloc")),
        sel("initWithFrame:"),
        frame)};
    if (!field)
        return field;

    send<void>(field.get(), sel("setStringValue:"), ns_string(text));
    if (responds_to(field.get(), "setPlaceholderString:")) {
        send<void>(
            field.get(),
            sel("setPlaceholderString:"),
            ns_string(placeholder));
    }
    send<void>(field.get(), sel("setBezeled:"), objc_bool(false));
    send<void>(field.get(), sel("setDrawsBackground:"), objc_bool(false));
    send<void>(field.get(), sel("setEditable:"), objc_bool(true));
    send<void>(field.get(), sel("setSelectable:"), objc_bool(true));
    if (responds_to(field.get(), "setFocusRingType:"))
        send<void>(field.get(), sel("setFocusRingType:"), 1ul);
    send<void>(field.get(), sel("setTextColor:"), label_color());
    send<void>(
        field.get(),
        sel("setFont:"),
        send<id>(class_object("NSFont"), sel("systemFontOfSize:"), 15.0));
    return field;
}

auto make_svg_icon_view(CGRect frame,
                        WindowToolbar const& toolbar,
                        std::string const& icon,
                        id tint) -> OwnedObjcObject {
    auto view = OwnedObjcObject{send<id>(
        send<id>(class_object("NSImageView"), sel("alloc")),
        sel("initWithFrame:"),
        frame)};
    if (!view)
        return view;
    if (auto image = image_for_icon(toolbar, icon))
        send<void>(view.get(), sel("setImage:"), image.get());
    if (responds_to(view.get(), "setImageScaling:"))
        send<void>(
            view.get(),
            sel("setImageScaling:"),
            static_cast<NSInteger>(ImageScaling::proportionally_up_or_down));
    if (tint && responds_to(view.get(), "setContentTintColor:"))
        send<void>(view.get(), sel("setContentTintColor:"), tint);
    return view;
}

auto make_symbol_icon_view(CGRect frame,
                           WindowToolbar const& toolbar,
                           std::string const& icon,
                           MaterialSymbolStyle style,
                           id tint) -> OwnedObjcObject {
    auto font = material_symbol_font(toolbar, style);
    if (!font)
        return {};

    auto field = OwnedObjcObject{send<id>(
        send<id>(class_object("NSTextField"), sel("alloc")),
        sel("initWithFrame:"),
        frame)};
    if (!field)
        return field;

    send<void>(
        field.get(),
        sel("setStringValue:"),
        ns_string(symbol_text(toolbar, icon)));
    send<void>(field.get(), sel("setBezeled:"), objc_bool(false));
    send<void>(field.get(), sel("setDrawsBackground:"), objc_bool(false));
    send<void>(field.get(), sel("setEditable:"), objc_bool(false));
    send<void>(field.get(), sel("setSelectable:"), objc_bool(false));
    send<void>(
        field.get(),
        sel("setAlignment:"),
        static_cast<NSInteger>(TextAlignment::center));
    if (tint)
        send<void>(field.get(), sel("setTextColor:"), tint);
    send<void>(
        field.get(),
        sel("setFont:"),
        reinterpret_cast<id>(
            const_cast<void*>(
                reinterpret_cast<void const*>(font.get()))));
    return field;
}

auto make_icon_view(CGRect frame,
                    WindowToolbar const& toolbar,
                    std::string const& icon,
                    MaterialSymbolStyle style,
                    id tint) -> OwnedObjcObject {
    if (auto font_icon = make_symbol_icon_view(frame, toolbar, icon, style, tint))
        return font_icon;
    return make_svg_icon_view(frame, toolbar, icon, tint);
}

auto toolbar_button(CGRect frame,
                    WindowToolbar const& toolbar,
                    ToolbarIconButton const& item) -> OwnedObjcObject {
    auto button = OwnedObjcObject{send<id>(
        send<id>(reinterpret_cast<id>(toolbar_button_class()), sel("alloc")),
        sel("initWithFrame:"),
        frame)};
    if (!button)
        return button;

    send<void>(button.get(), sel("setTitle:"), ns_string(""));
    send<void>(button.get(), sel("setBordered:"), objc_bool(false));
    send<void>(button.get(), sel("setTransparent:"), objc_bool(false));
    send<void>(button.get(), sel("setEnabled:"), objc_bool(item.enabled));
    send<void>(button.get(), sel("setToolTip:"), ns_string(item.label));
    send<void>(button.get(), sel("setTarget:"), button.get());
    send<void>(
        button.get(),
        sel("setAction:"),
        sel("toolbarButtonPressed:"));
    send<void>(
        button.get(),
        sel("setTag:"),
        item.selected ? toolbar_button_selected_tag : 0);
    configure_layer(
        button.get(),
        toolbar.metrics.button_height * 0.5,
        toolbar_button_background(item.selected, false),
        nil,
        0.0);
    send<void>(button.get(), sel("updateTrackingAreas"));
    auto const style = symbol_style(toolbar, item);
    auto tint = item.enabled
        ? label_color()
        : color_with_alpha(label_color(), 0.34);

    auto add_icon = [&](std::string const& icon,
                        CGRect icon_frame,
                        MaterialSymbolStyle icon_style) {
        auto image_view = make_icon_view(
            icon_frame,
            toolbar,
            icon,
            icon_style,
            tint);
        if (image_view) {
            send<void>(image_view.get(), sel("setAutoresizingMask:"), 0ul);
            send<void>(button.get(), sel("addSubview:"), image_view.get());
        }
    };

    if (item.accessory_icon.empty()) {
        auto const icon_size = static_cast<CGFloat>(style.size);
        auto const icon_width =
            icon_size + static_cast<CGFloat>(toolbar.metrics.icon_extra_width);
        auto const icon_height =
            icon_size + static_cast<CGFloat>(toolbar.metrics.icon_extra_height);
        auto const icon_x = (frame.size.width - icon_width) * 0.5;
        auto const icon_y = (frame.size.height - icon_height) * 0.5;
        add_icon(
            item.icon,
            CGRect{{icon_x, icon_y}, {icon_width, icon_height}},
            style);
    } else {
        auto const icon_size = static_cast<CGFloat>(style.size);
        auto const accessory_size =
            static_cast<CGFloat>(style.accessory_size);
        auto const icon_width = icon_size
            + static_cast<CGFloat>(
                toolbar.metrics.accessory_icon_extra_width);
        auto const icon_height = icon_size
            + static_cast<CGFloat>(
                toolbar.metrics.accessory_icon_extra_height);
        auto const accessory_width = accessory_size
            + static_cast<CGFloat>(
                toolbar.metrics.accessory_icon_extra_width);
        auto const accessory_height = accessory_size
            + static_cast<CGFloat>(
                toolbar.metrics.accessory_icon_extra_height);
        constexpr CGFloat gap = 0.0;
        auto const total_width = icon_width + gap + accessory_width;
        auto const icon_x = (frame.size.width - total_width) * 0.5;
        auto const icon_y = (frame.size.height - icon_height) * 0.5;
        auto const accessory_x = icon_x + icon_width + gap;
        auto const accessory_y =
            (frame.size.height - accessory_height) * 0.5;
        auto accessory_style = style;
        accessory_style.size = style.accessory_size;
        accessory_style.optical_size =
            std::max(20.0f, style.optical_size - 4.0f);
        add_icon(
            item.icon,
            CGRect{{icon_x, icon_y}, {icon_width, icon_height}},
            style);
        add_icon(
            item.accessory_icon,
            CGRect{
                {accessory_x, accessory_y},
                {accessory_width, accessory_height}},
            accessory_style);
    }
    return button;
}

auto toolbar_button_width(WindowToolbar const& toolbar,
                          ToolbarIconButton const& item) -> CGFloat {
    if (item.accessory_icon.empty())
        return static_cast<CGFloat>(toolbar.metrics.button_width);

    return static_cast<CGFloat>(toolbar.metrics.accessory_button_width);
}

auto toolbar_group_is_search(ToolbarGroup const& group) -> bool {
    return group.buttons.size() == 1 && group.buttons.front().opens_search;
}

auto toolbar_group_width(WindowToolbar const& toolbar,
                         ToolbarGroup const& group,
                         bool search_expanded = false) -> CGFloat {
    if (search_expanded && toolbar_group_is_search(group))
        return toolbar_search_width;

    if (group.buttons.empty())
        return 40.0;

    auto width = static_cast<CGFloat>(toolbar.metrics.group_padding_x * 2.0f);
    auto first = true;
    for (auto const& item : group.buttons) {
        if (!first)
            width += static_cast<CGFloat>(toolbar.metrics.button_spacing);
        width += toolbar_button_width(toolbar, item);
        first = false;
    }
    return width;
}

auto toolbar_group_from_compact_segment(
    ToolbarGroupCompactSegment const& segment) -> ToolbarGroup {
    return ToolbarGroup{
        .buttons = segment.buttons,
        .separators = segment.separators,
    };
}

auto toolbar_layout_groups(ToolbarGroup const& group,
                           std::size_t compact_stage)
    -> std::vector<ToolbarGroup> {
    if (compact_stage == 0)
        return {group};

    if (group.compact_button && compact_stage == 1) {
        auto compact_group = ToolbarGroup{};
        auto compact_button = *group.compact_button;
        if (auto selected_index = toolbar_group_selected_index(group)) {
            compact_button = group.buttons[*selected_index];
            compact_button.accessory_icon =
                group.compact_button->accessory_icon;
        }
        compact_button.selected = false;
        compact_button.opens_menu = true;
        compact_group.buttons.push_back(std::move(compact_button));
        compact_group.separators = false;
        return {std::move(compact_group)};
    }

    auto const static_stage_index =
        compact_stage - (group.compact_button ? 2 : 1);
    if (static_stage_index < group.compact_stages.size()) {
        auto const& stage = group.compact_stages[static_stage_index];
        auto groups = std::vector<ToolbarGroup>{};
        groups.reserve(stage.segments.size());
        for (auto const& segment : stage.segments)
            groups.push_back(toolbar_group_from_compact_segment(segment));
        return groups;
    }

    return {group};
}

auto toolbar_group_compact_stage_count(ToolbarGroup const& group)
    -> std::size_t {
    return (group.compact_button ? std::size_t{1} : std::size_t{0})
        + group.compact_stages.size();
}

auto toolbar_group_next_compact_priority(ToolbarGroup const& group,
                                         std::size_t compact_stage)
    -> std::optional<int> {
    if (compact_stage >= toolbar_group_compact_stage_count(group))
        return std::nullopt;
    if (group.compact_button && compact_stage == 0)
        return group.compact_priority.value_or(0);

    auto const static_stage_index =
        compact_stage - (group.compact_button ? 1 : 0);
    if (static_stage_index < group.compact_stages.size())
        return group.compact_stages[static_stage_index].priority;
    return std::nullopt;
}

auto toolbar_trailing_groups_width(WindowToolbar const& toolbar,
                                   std::span<std::size_t const> compact_stages,
                                   bool search_expanded = false)
    -> CGFloat {
    auto width = CGFloat{0.0};
    auto first = true;
    for (auto index = std::size_t{0};
         index < toolbar.trailing_groups.size();
         ++index) {
        auto const& source_group = toolbar.trailing_groups[index];
        auto groups =
            toolbar_layout_groups(source_group, compact_stages[index]);
        for (auto const& group : groups) {
            if (!first)
                width += static_cast<CGFloat>(toolbar.metrics.group_gap);
            width += toolbar_group_width(
                toolbar,
                group,
                search_expanded && toolbar_group_is_search(source_group));
            first = false;
        }
    }
    return width;
}

auto toolbar_trailing_layout_stages(WindowToolbar const& toolbar,
                                    WindowPadding const& padding,
                                    CGFloat width,
                                    CGFloat protected_left,
                                    bool search_expanded = false)
    -> std::vector<std::size_t> {
    auto compact_stages =
        std::vector<std::size_t>(
            toolbar.trailing_groups.size(),
            std::size_t{0});
    auto const right_edge = width
        - static_cast<CGFloat>(toolbar.metrics.trailing_inset)
        - static_cast<CGFloat>(std::max(0.0f, padding.right));

    auto fits = [&] {
        auto const left =
            right_edge
            - toolbar_trailing_groups_width(
                toolbar,
                compact_stages,
                search_expanded);
        return left >= protected_left;
    };

    while (!fits()) {
        auto best_index = std::optional<std::size_t>{};
        auto best_priority = std::optional<int>{};
        for (auto index = std::size_t{0};
             index < toolbar.trailing_groups.size();
             ++index) {
            auto priority = toolbar_group_next_compact_priority(
                toolbar.trailing_groups[index],
                compact_stages[index]);
            if (!priority)
                continue;
            if (!best_priority || *priority < *best_priority) {
                best_index = index;
                best_priority = *priority;
            }
        }
        if (!best_index)
            break;
        ++compact_stages[*best_index];
    }

    return compact_stages;
}

auto toolbar_max_compact_stages(WindowToolbar const& toolbar)
    -> std::vector<std::size_t> {
    auto compact_stages = std::vector<std::size_t>{};
    compact_stages.reserve(toolbar.trailing_groups.size());
    for (auto const& group : toolbar.trailing_groups)
        compact_stages.push_back(toolbar_group_compact_stage_count(group));
    return compact_stages;
}

auto toolbar_leading_protected_width(WindowToolbar const& toolbar,
                                     WindowPadding const& padding)
    -> CGFloat {
    auto protected_width =
        static_cast<CGFloat>(toolbar.metrics.leading_reserved_width)
        + static_cast<CGFloat>(std::max(0.0f, padding.left));
    for (auto const& group : toolbar.leading_groups) {
        protected_width += toolbar_group_width(toolbar, group)
            + static_cast<CGFloat>(toolbar.metrics.group_gap);
    }
    if (!toolbar.title.empty()) {
        protected_width += toolbar_title_spacing
            + toolbar_title_width
            + toolbar_title_trailing_gap;
    }
    return protected_width;
}

auto toolbar_minimum_content_width(WindowToolbar const& toolbar,
                                   WindowPadding const& padding,
                                   bool search_expanded = false) -> CGFloat {
    if (!toolbar.visible)
        return 0.0;
    auto const compact_stages = toolbar_max_compact_stages(toolbar);
    auto const trailing_width =
        toolbar_trailing_groups_width(
            toolbar,
            compact_stages,
            search_expanded);
    return toolbar_leading_protected_width(toolbar, padding)
        + trailing_width
        + static_cast<CGFloat>(toolbar.metrics.trailing_inset)
        + static_cast<CGFloat>(std::max(0.0f, padding.right));
}

auto toolbar_minimum_content_height(WindowToolbar const& toolbar,
                                    WindowPadding const& padding) -> CGFloat {
    if (!toolbar.visible)
        return 0.0;
    return static_cast<CGFloat>(toolbar.metrics.height)
        + static_cast<CGFloat>(std::max(0.0f, padding.top))
        + static_cast<CGFloat>(std::max(0.0f, padding.bottom));
}

auto minimum_content_size(WindowOptions const& options) -> CGSize {
    auto width =
        static_cast<CGFloat>(std::max(0.0f, options.minimum_size.width));
    auto height =
        static_cast<CGFloat>(std::max(0.0f, options.minimum_size.height));
    width = std::max(
        width,
        toolbar_minimum_content_width(options.toolbar, options.padding));
    height = std::max(
        height,
        toolbar_minimum_content_height(options.toolbar, options.padding));
    return CGSize{width, height};
}

auto toolbar_content_minimum_size(ToolbarLayoutState const& state) -> CGSize {
    auto width = state.minimum_size.width;
    auto height = state.minimum_size.height;
    width = std::max(
        width,
        toolbar_minimum_content_width(
            state.toolbar,
            state.padding,
            state.search_expanded));
    height = std::max(
        height,
        toolbar_minimum_content_height(state.toolbar, state.padding));
    return CGSize{width, height};
}

void apply_toolbar_minimum_size(ToolbarLayoutState const& state) {
    if (!state.window)
        return;

    auto const minimum_size = toolbar_content_minimum_size(state);
    if (minimum_size.width > 0.0 || minimum_size.height > 0.0)
        send<void>(state.window, sel("setContentMinSize:"), minimum_size);

    auto content_view = send<id>(state.window, sel("contentView"));
    if (!content_view || !responds_to(content_view, "bounds"))
        return;
    auto const bounds = send<CGRect>(content_view, sel("bounds"));
    auto const content_size = CGSize{
        std::max(bounds.size.width, minimum_size.width),
        std::max(bounds.size.height, minimum_size.height),
    };
    if (content_size.width > bounds.size.width
        || content_size.height > bounds.size.height) {
        send<void>(state.window, sel("setContentSize:"), content_size);
    }
}

auto add_toolbar_separator(id container, CGFloat center_x, CGFloat group_height)
    -> id {
    constexpr CGFloat separator_width = 1.0;
    constexpr CGFloat separator_height = 18.0;
    auto separator = make_view(CGRect{
        {center_x - separator_width * 0.5,
         (group_height - separator_height) * 0.5},
        {separator_width, separator_height}});
    if (!separator)
        return nil;
    send<void>(separator.get(), sel("setAutoresizingMask:"), 0ul);
    configure_layer(
        separator.get(),
        separator_width * 0.5,
        toolbar_separator_color(),
        nil,
        0.0);
    send<void>(container, sel("addSubview:"), separator.get());
    return separator.get();
}

void attach_toolbar_separator(id leading_button,
                              id trailing_button,
                              id separator) {
    if (!leading_button || !trailing_button || !separator)
        return;
    objc_setAssociatedObject(
        leading_button,
        toolbar_trailing_separator_key(),
        separator,
        OBJC_ASSOCIATION_RETAIN_NONATOMIC);
    objc_setAssociatedObject(
        trailing_button,
        toolbar_leading_separator_key(),
        separator,
        OBJC_ASSOCIATION_RETAIN_NONATOMIC);
    objc_setAssociatedObject(
        separator,
        toolbar_separator_leading_button_key(),
        leading_button,
        OBJC_ASSOCIATION_ASSIGN);
    objc_setAssociatedObject(
        separator,
        toolbar_separator_trailing_button_key(),
        trailing_button,
        OBJC_ASSOCIATION_ASSIGN);
    set_toolbar_separator_hidden(separator, false);
}

void add_toolbar_search_group(id toolbar_view,
                              CGRect frame,
                              WindowToolbar const& toolbar,
                              ToolbarGroup const& group,
                              NSUInteger autoresizing,
                              std::string const& search_text) {
    auto container = make_view(frame);
    if (!container)
        return;
    send<void>(container.get(), sel("setAutoresizingMask:"), autoresizing);
    configure_layer(
        container.get(),
        frame.size.height * 0.5,
        control_background_color(),
        nil,
        0.0);
    configure_shadow(container.get(), 0.12, 4.0, CGSize{0.0, -1.0});

    auto const& item = group.buttons.front();
    auto style = symbol_style(toolbar, item);
    auto const icon_size = static_cast<CGFloat>(style.size);
    auto const icon_width =
        icon_size + static_cast<CGFloat>(toolbar.metrics.icon_extra_width);
    auto const icon_height =
        icon_size + static_cast<CGFloat>(toolbar.metrics.icon_extra_height);
    constexpr CGFloat icon_x = 12.0;
    auto const icon_y = (frame.size.height - icon_height) * 0.5;
    auto icon = make_icon_view(
        CGRect{{icon_x, icon_y}, {icon_width, icon_height}},
        toolbar,
        item.icon,
        style,
        label_color());
    if (icon) {
        send<void>(icon.get(), sel("setAutoresizingMask:"), 0ul);
        send<void>(container.get(), sel("addSubview:"), icon.get());
    }

    auto const field_x = icon_x + icon_width + 4.0;
    auto const field_height = CGFloat{24.0};
    auto const field_y = (frame.size.height - field_height) * 0.5
        + toolbar_search_field_y_offset;
    auto field = make_search_text_field(
        CGRect{
            {field_x, field_y},
            {std::max(CGFloat{24.0}, frame.size.width - field_x - 12.0),
             field_height}},
        item.label,
        search_text);
    if (field) {
        send<void>(
            field.get(),
            sel("setAutoresizingMask:"),
            static_cast<NSUInteger>(AppKitAutoresizingMask::width_sizable));
        send<void>(container.get(), sel("addSubview:"), field.get());
    }

    send<void>(toolbar_view, sel("addSubview:"), container.get());
    auto window = send<id>(toolbar_view, sel("window"));
    if (window && field)
        send<void>(window, sel("makeFirstResponder:"), field.get());
    if (field) {
        objc_setAssociatedObject(
            toolbar_view,
            toolbar_search_field_key(),
            field.get(),
            OBJC_ASSOCIATION_ASSIGN);
    }
}

void add_toolbar_group(id toolbar_view,
                       CGRect frame,
                       WindowToolbar const& toolbar,
                       ToolbarGroup const& group,
                       NSUInteger autoresizing,
                       std::optional<ToolbarGroupBinding> binding = std::nullopt,
                       bool search_expanded = false,
                       std::string const& search_text = {}) {
    if (search_expanded && toolbar_group_is_search(group)) {
        add_toolbar_search_group(
            toolbar_view,
            frame,
            toolbar,
            group,
            autoresizing,
            search_text);
        return;
    }

    auto container = make_view(frame);
    if (!container)
        return;
    send<void>(container.get(), sel("setAutoresizingMask:"), autoresizing);
    configure_layer(
        container.get(),
        frame.size.height * 0.5,
        control_background_color(),
        nil,
        0.0);
    configure_shadow(container.get(), 0.12, 4.0, CGSize{0.0, -1.0});

    auto button_x = static_cast<CGFloat>(toolbar.metrics.group_padding_x);
    auto const button_y =
        (frame.size.height
         - static_cast<CGFloat>(toolbar.metrics.button_height)) * 0.5;
    auto previous_button = id{};
    for (auto index = std::size_t{0}; index < group.buttons.size(); ++index) {
        auto const& item = group.buttons[index];
        auto const button_width = toolbar_button_width(toolbar, item);
        auto button_frame = CGRect{
            {button_x, button_y},
            {button_width, static_cast<CGFloat>(toolbar.metrics.button_height)},
        };
        auto button = toolbar_button(button_frame, toolbar, item);
        auto current_button = id{};
        if (button) {
            current_button = button.get();
            if (binding) {
                bind_toolbar_button(
                    button.get(),
                    toolbar_view,
                    *binding,
                    binding->compact_item_index.value_or(index));
            }
            if (item.opens_menu || !item.menu_items.empty()) {
                objc_setAssociatedObject(
                    button.get(),
                    toolbar_button_opens_menu_key(),
                    ns_number(true),
                    OBJC_ASSOCIATION_RETAIN_NONATOMIC);
            }
            set_toolbar_button_menu_items(button.get(), item.menu_items);
            if (item.opens_overflow_menu) {
                objc_setAssociatedObject(
                    button.get(),
                    toolbar_button_opens_overflow_menu_key(),
                    ns_number(true),
                    OBJC_ASSOCIATION_RETAIN_NONATOMIC);
            }
            if (item.opens_search) {
                objc_setAssociatedObject(
                    button.get(),
                    toolbar_button_opens_search_key(),
                    ns_number(true),
                    OBJC_ASSOCIATION_RETAIN_NONATOMIC);
            }
            send<void>(button.get(), sel("setAutoresizingMask:"), 0ul);
            send<void>(container.get(), sel("addSubview:"), button.get());
        }
        if (group.separators && previous_button && current_button) {
            auto separator = add_toolbar_separator(
                container.get(),
                button_x
                    - static_cast<CGFloat>(toolbar.metrics.button_spacing)
                        * 0.5,
                frame.size.height);
            attach_toolbar_separator(previous_button, current_button, separator);
        }
        if (current_button)
            previous_button = current_button;
        button_x += button_width
            + static_cast<CGFloat>(toolbar.metrics.button_spacing);
    }

    send<void>(toolbar_view, sel("addSubview:"), container.get());
}

void remove_toolbar_subviews(id toolbar_view) {
    if (!toolbar_view)
        return;
    auto subviews = send<id>(toolbar_view, sel("subviews"));
    while (subviews && send<NSUInteger>(subviews, sel("count")) > 0u) {
        auto const index = send<NSUInteger>(subviews, sel("count")) - 1u;
        auto subview = send<id>(subviews, sel("objectAtIndex:"), index);
        if (subview)
            send<void>(subview, sel("removeFromSuperview"));
        subviews = send<id>(toolbar_view, sel("subviews"));
    }
}

void sync_toolbar_search_text(id toolbar_view,
                              ToolbarLayoutState& state,
                              bool clear_field = false) {
    auto field = static_cast<id>(
        objc_getAssociatedObject(toolbar_view, toolbar_search_field_key()));
    if (field) {
        state.search_text =
            string_value(send<id>(field, sel("stringValue")));
    }
    if (clear_field) {
        objc_setAssociatedObject(
            toolbar_view,
            toolbar_search_field_key(),
            nil,
            OBJC_ASSOCIATION_ASSIGN);
    }
}

void collapse_empty_toolbar_search(id receiver) {
    auto window = window_for(receiver);
    if (!window)
        return;

    auto view_to_layout = id{};
    for (auto& state : toolbar_layout_states()) {
        if (state.window != window || !state.search_expanded)
            continue;

        sync_toolbar_search_text(state.view, state);
        if (!state.search_text.empty())
            continue;

        state.search_expanded = false;
        view_to_layout = state.view;
        break;
    }

    if (view_to_layout)
        layout_toolbar_view(view_to_layout);
}

void layout_toolbar_view(id toolbar_view) {
    auto state = toolbar_layout_state(toolbar_view);
    if (!state)
        return;
    if (state->layouting) {
        state->layout_pending = true;
        return;
    }

    state->layouting = true;
    do {
        state->layout_pending = false;

        sync_toolbar_search_text(toolbar_view, *state, true);
        remove_toolbar_subviews(toolbar_view);
        apply_toolbar_minimum_size(*state);

        auto const& toolbar = state->toolbar;
        auto bounds = send<CGRect>(toolbar_view, sel("bounds"));
        auto const width = bounds.size.width;
        auto const group_height =
            static_cast<CGFloat>(toolbar.metrics.group_height);
        auto const left_padding =
            static_cast<CGFloat>(std::max(0.0f, state->padding.left));
        auto const right_padding =
            static_cast<CGFloat>(std::max(0.0f, state->padding.right));

        auto leading_x =
            static_cast<CGFloat>(toolbar.metrics.leading_reserved_width)
            + left_padding;
        for (auto group_index = std::size_t{0};
             group_index < toolbar.leading_groups.size();
             ++group_index) {
            auto const& group = toolbar.leading_groups[group_index];
            auto group_width = toolbar_group_width(toolbar, group);
            add_toolbar_group(
                toolbar_view,
                CGRect{{leading_x, state->group_y}, {group_width, group_height}},
                toolbar,
                group,
                static_cast<NSUInteger>(AppKitAutoresizingMask::max_x_margin),
                ToolbarGroupBinding{
                    .side = ToolbarGroupSide::leading,
                    .group_index = group_index,
                    .selectable = toolbar_group_selectable(group),
                });
            leading_x += group_width
                + static_cast<CGFloat>(toolbar.metrics.group_gap);
        }

        auto protected_left = leading_x;
        if (!toolbar.title.empty()) {
            auto title_x = leading_x + toolbar_title_spacing;
            auto title = make_text_field(
                CGRect{
                    {title_x, state->title_y},
                    {toolbar_title_width, toolbar_title_height}},
                toolbar.title,
                20.0,
                true);
            if (title) {
                send<void>(
                    title.get(),
                    sel("setAutoresizingMask:"),
                    static_cast<NSUInteger>(
                        AppKitAutoresizingMask::max_x_margin));
                send<void>(toolbar_view, sel("addSubview:"), title.get());
            }
            protected_left =
                title_x + toolbar_title_width + toolbar_title_trailing_gap;
        }

        state->trailing_compact_stages = toolbar_trailing_layout_stages(
            toolbar,
            state->padding,
            width,
            protected_left,
            state->search_expanded);
        auto trailing_x = width
            - static_cast<CGFloat>(toolbar.metrics.trailing_inset)
            - right_padding;
        for (auto reverse_index = std::size_t{0};
             reverse_index < toolbar.trailing_groups.size();
             ++reverse_index) {
            auto const group_index =
                toolbar.trailing_groups.size() - reverse_index - 1;
            auto const& source_group = toolbar.trailing_groups[group_index];
            auto const compact_stage =
                state->trailing_compact_stages[group_index];
            auto groups = toolbar_layout_groups(source_group, compact_stage);
            for (auto visual_reverse_index = std::size_t{0};
                 visual_reverse_index < groups.size();
                 ++visual_reverse_index) {
                auto const visual_index =
                    groups.size() - visual_reverse_index - 1;
                auto const& group = groups[visual_index];
                auto const search_expanded =
                    state->search_expanded
                    && toolbar_group_is_search(source_group);
                auto group_width =
                    toolbar_group_width(toolbar, group, search_expanded);
                trailing_x -= group_width;
                auto selected_index = toolbar_group_selected_index(source_group);
                auto compact_item_index =
                    compact_stage == 1 && source_group.compact_button
                        ? selected_index
                        : std::optional<std::size_t>{};
                add_toolbar_group(
                    toolbar_view,
                    CGRect{
                        {trailing_x, state->group_y},
                        {group_width, group_height}},
                    toolbar,
                    group,
                    static_cast<NSUInteger>(
                        AppKitAutoresizingMask::min_x_margin),
                    ToolbarGroupBinding{
                        .side = ToolbarGroupSide::trailing,
                        .group_index = group_index,
                        .selectable = toolbar_group_selectable(source_group),
                        .compact_item_index = compact_item_index,
                    },
                    search_expanded,
                    state->search_text);
                trailing_x -= static_cast<CGFloat>(toolbar.metrics.group_gap);
            }
        }

        apply_traffic_light_padding(state->window, state->padding);
    } while (state->layout_pending);
    state->layouting = false;
}

auto toolbar_frame_observer_class() -> Class {
    static Class observer_class = [] {
        auto superclass = static_cast<Class>(objc_getClass("NSObject"));
        auto created = objc_allocateClassPair(
            superclass,
            "PhenotypeToolbarFrameObserver",
            0);
        class_addMethod(
            created,
            sel("toolbarFrameDidChange:"),
            reinterpret_cast<IMP>(
                +[](id, SEL, id notification) {
                    auto toolbar_view =
                        send<id>(notification, sel("object"));
                    layout_toolbar_view(toolbar_view);
                }),
            "v@:@");
        objc_registerClassPair(created);
        return created;
    }();
    return observer_class;
}

void install_toolbar_frame_observer(id toolbar_view) {
    if (!toolbar_view)
        return;
    send<void>(
        toolbar_view,
        sel("setPostsFrameChangedNotifications:"),
        objc_bool(true));
    auto observer = OwnedObjcObject{send<id>(
        send<id>(
            reinterpret_cast<id>(toolbar_frame_observer_class()),
            sel("alloc")),
        sel("init"))};
    if (!observer)
        return;

    auto notification_center =
        send<id>(class_object("NSNotificationCenter"), sel("defaultCenter"));
    send<void>(
        notification_center,
        sel("addObserver:selector:name:object:"),
        observer.get(),
        sel("toolbarFrameDidChange:"),
        ns_string("NSViewFrameDidChangeNotification"),
        toolbar_view);
    objc_setAssociatedObject(
        toolbar_view,
        toolbar_frame_observer_key(),
        observer.get(),
        OBJC_ASSOCIATION_RETAIN_NONATOMIC);
}

auto traffic_light_resize_observer_class() -> Class {
    static Class observer_class = [] {
        auto superclass = static_cast<Class>(objc_getClass("NSObject"));
        auto created = objc_allocateClassPair(
            superclass,
            "PhenotypeTrafficLightResizeObserver",
            0);
        class_addMethod(
            created,
            sel("windowDidResize:"),
            reinterpret_cast<IMP>(
                +[](id, SEL, id notification) {
                    auto window = send<id>(notification, sel("object"));
                    auto& state = traffic_light_layout_state(window);
                    apply_traffic_light_padding(window, state.padding);
                }),
            "v@:@");
        objc_registerClassPair(created);
        return created;
    }();
    return observer_class;
}

void install_traffic_light_resize_observer(id window,
                                           WindowPadding const& padding) {
    if (!window)
        return;

    auto& state = traffic_light_layout_state(window);
    state.padding = padding;
    if (objc_getAssociatedObject(window, traffic_light_resize_observer_key()))
        return;

    auto observer = OwnedObjcObject{send<id>(
        send<id>(
            reinterpret_cast<id>(traffic_light_resize_observer_class()),
            sel("alloc")),
        sel("init"))};
    if (!observer)
        return;

    auto notification_center =
        send<id>(class_object("NSNotificationCenter"), sel("defaultCenter"));
    send<void>(
        notification_center,
        sel("addObserver:selector:name:object:"),
        observer.get(),
        sel("windowDidResize:"),
        ns_string("NSWindowDidResizeNotification"),
        window);
    objc_setAssociatedObject(
        window,
        traffic_light_resize_observer_key(),
        observer.get(),
        OBJC_ASSOCIATION_RETAIN_NONATOMIC);
}

auto traffic_light_center_y(id window, id content_view, CGRect bounds) -> CGFloat {
    constexpr NSInteger close_button = 0;
    if (!window || !content_view
        || !responds_to(window, "standardWindowButton:")) {
        return bounds.size.height - 22.0;
    }

    auto button = send<id>(
        window,
        sel("standardWindowButton:"),
        close_button);
    if (!button || !responds_to(button, "frame")
        || !responds_to(button, "superview")
        || !responds_to(content_view, "convertRect:fromView:")) {
        return bounds.size.height - 22.0;
    }

    auto button_frame = send<CGRect>(button, sel("frame"));
    auto superview = send<id>(button, sel("superview"));
    auto content_frame = send<CGRect>(
        content_view,
        sel("convertRect:fromView:"),
        button_frame,
        superview);
    return content_frame.origin.y + content_frame.size.height * 0.5;
}

void apply_traffic_light_padding(id window, WindowPadding const& padding) {
    if (!window || !responds_to(window, "standardWindowButton:"))
        return;

    auto const horizontal = static_cast<CGFloat>(std::max(0.0f, padding.left));
    auto const vertical = static_cast<CGFloat>(std::max(0.0f, padding.top));
    auto& state = traffic_light_layout_state(window);
    state.padding = padding;
    for (NSInteger kind : {0, 1, 2}) {
        auto button = send<id>(
            window,
            sel("standardWindowButton:"),
            kind);
        if (!button || !responds_to(button, "frame")
            || !responds_to(button, "setFrameOrigin:"))
            continue;

        auto frame = send<CGRect>(button, sel("frame"));
        auto& button_state =
            state.buttons[static_cast<std::size_t>(kind)];
        if (!button_state.initialized) {
            button_state.base_origin = frame.origin;
            button_state.initialized = true;
        } else if (!nearly_equal(frame.origin, button_state.applied_origin)) {
            button_state.base_origin = frame.origin;
        }

        auto origin = CGPoint{
            button_state.base_origin.x + horizontal,
            button_state.base_origin.y - vertical,
        };
        button_state.applied_origin = origin;
        send<void>(button, sel("setFrameOrigin:"), origin);
    }
}

void install_toolbar(id content_view,
                     id window,
                     CGRect bounds,
                     WindowPadding const& padding,
                     CGSize minimum_size,
                     WindowToolbar const& toolbar) {
    if (!content_view || !toolbar.visible)
        return;

    auto const toolbar_height = static_cast<CGFloat>(toolbar.metrics.height);
    auto const group_height = static_cast<CGFloat>(toolbar.metrics.group_height);
    constexpr CGFloat title_height = 28.0;
    auto const width = bounds.size.width;
    auto const y = bounds.size.height - toolbar_height;
    auto const center_y = traffic_light_center_y(window, content_view, bounds);
    auto const center_in_toolbar = std::clamp(
        center_y - y,
        group_height * 0.5,
        toolbar_height - group_height * 0.5);
    auto const group_y = center_in_toolbar - group_height * 0.5;
    auto const title_y = center_in_toolbar - title_height * 0.5;
    auto toolbar_frame = CGRect{{0.0, y}, {width, toolbar_height}};
    auto toolbar_view = make_focus_clearing_view(toolbar_frame);
    if (!toolbar_view)
        return;
    send<void>(
        toolbar_view.get(),
        sel("setAutoresizingMask:"),
        AppKitAutoresizingMask::width_sizable
            | AppKitAutoresizingMask::min_y_margin);

    set_toolbar_layout_state(
        toolbar_view.get(),
        ToolbarLayoutState{
            .window = window,
            .padding = padding,
            .toolbar = toolbar,
            .minimum_size = minimum_size,
            .group_y = group_y,
            .title_y = title_y,
        });
    install_toolbar_frame_observer(toolbar_view.get());
    layout_toolbar_view(toolbar_view.get());
    send<void>(content_view, sel("addSubview:"), toolbar_view.get());
}

auto install_glass_backdrop(id window,
                            CGRect bounds,
                            WindowGlassEffect const& effect) -> id {
    auto container = OwnedObjcObject{send<id>(
        send<id>(cls("NSView"), sel("alloc")),
        sel("initWithFrame:"),
        bounds)};
    auto visual = OwnedObjcObject{send<id>(
        send<id>(cls("NSVisualEffectView"), sel("alloc")),
        sel("initWithFrame:"),
        bounds)};
    auto content = make_focus_clearing_view(bounds);
    if (!container || !visual || !content)
        return nil;

    auto const autoresizing =
        AppKitAutoresizingMask::width_sizable
        | AppKitAutoresizingMask::height_sizable;
    send<void>(container.get(), sel("setAutoresizingMask:"), autoresizing);
    send<void>(visual.get(), sel("setAutoresizingMask:"), autoresizing);
    send<void>(content.get(), sel("setAutoresizingMask:"), autoresizing);
    configure_visual_effect_view(visual.get(), effect);

    send<void>(window, sel("setContentView:"), container.get());
    send<void>(container.get(), sel("addSubview:"), visual.get());
    send<void>(container.get(), sel("addSubview:"), content.get());
    return content.get();
}

void configure_glass_window(id window,
                            CGRect bounds,
                            WindowOptions const& options) {
    constexpr NSInteger hidden_title = 1;

    send<void>(window, sel("setOpaque:"), objc_bool(false));
    send<void>(
        window,
        sel("setBackgroundColor:"),
        send<id>(cls("NSColor"), sel("clearColor")));
    send<void>(
        window,
        sel("setTitlebarAppearsTransparent:"),
        objc_bool(true));
    send<void>(window, sel("setTitleVisibility:"), hidden_title);
    send<void>(
        window,
        sel("setMovableByWindowBackground:"),
        objc_bool(true));
    apply_traffic_light_padding(window, options.padding);
    install_traffic_light_resize_observer(window, options.padding);
    auto content_view = install_glass_backdrop(window, bounds, options.glass);
    auto const minimum_size = minimum_content_size(options);
    install_toolbar(
        content_view,
        window,
        bounds,
        options.padding,
        minimum_size,
        options.toolbar);
}

auto app_delegate_class() -> Class {
    static Class delegate_class = [] {
        auto superclass = static_cast<Class>(objc_getClass("NSObject"));
        auto created = objc_allocateClassPair(
            superclass,
            "PhenotypeNativeAppDelegate",
            0);
        class_addMethod(
            created,
            sel("applicationShouldTerminateAfterLastWindowClosed:"),
            reinterpret_cast<IMP>(
                +[](id, SEL, id) -> ObjcBool {
                    return objc_bool(true);
                }),
            "c@:@");
        objc_registerClassPair(created);
        return created;
    }();
    return delegate_class;
}

auto run_macos_window(WindowOptions const& options) -> int {
    constexpr NSInteger activation_policy_regular = 0;

    auto app = send<id>(cls("NSApplication"), sel("sharedApplication"));
    send<void>(app,
               sel("setActivationPolicy:"),
               activation_policy_regular);
    install_standard_main_menu(app, options.title.empty() ? "Phenotype"
                                                          : options.title);

    auto delegate = send<id>(
        send<id>(reinterpret_cast<id>(app_delegate_class()), sel("alloc")),
        sel("init"));
    send<void>(app, sel("setDelegate:"), delegate);

    auto const minimum_size = minimum_content_size(options);
    auto width = std::max(
        static_cast<CGFloat>(std::max(1, options.width)),
        minimum_size.width);
    auto height = std::max(
        static_cast<CGFloat>(std::max(1, options.height)),
        minimum_size.height);
    auto frame = CGRect{{0.0, 0.0}, {width, height}};
    auto style = AppKitWindowStyle::titled
        | AppKitWindowStyle::closable
        | AppKitWindowStyle::miniaturizable
        | AppKitWindowStyle::resizable;
    if (options.backdrop == WindowBackdrop::glass)
        style |= AppKitWindowStyle::full_size_content_view;

    auto window = send<id>(
        send<id>(cls("NSWindow"), sel("alloc")),
        sel("initWithContentRect:styleMask:backing:defer:"),
        frame,
        style,
        static_cast<NSUInteger>(AppKitBackingStore::buffered),
        objc_bool(false));
    send<void>(window, sel("setTitle:"), ns_string(options.title));
    if (minimum_size.width > 0.0 || minimum_size.height > 0.0)
        send<void>(window, sel("setContentMinSize:"), minimum_size);

    if (options.backdrop == WindowBackdrop::glass)
        configure_glass_window(window, frame, options);

    send<void>(window, sel("center"));
    send<void>(window, sel("makeKeyAndOrderFront:"), nil);
    send<void>(app, sel("activateIgnoringOtherApps:"), objc_bool(true));
    send<void>(app, sel("run"));
    return 0;
}

#else

auto run_macos_window(WindowOptions const&) -> int {
    return 0;
}

#endif

} // namespace detail

template <typename App>
int run_app(WindowOptions options,
            App app,
            std::function<void(int, int, float)> on_viewport = {}) {
    if (on_viewport)
        on_viewport(options.width, options.height, 1.0f);
    ui::run<App>(std::move(app));
#if defined(__APPLE__)
    return detail::run_macos_window(options);
#else
    return 0;
#endif
}

template <typename App>
int run_app(int width,
            int height,
            char const* title,
            App app,
            std::function<void(int, int, float)> on_viewport = {}) {
    auto options = WindowOptions{};
    options.width = width;
    options.height = height;
    options.title = title == nullptr ? "Phenotype" : title;
    return run_app(std::move(options), std::move(app), std::move(on_viewport));
}

} // namespace phenotype::native
