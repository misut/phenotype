module;

#if defined(__APPLE__)
#include <CoreGraphics/CGGeometry.h>
#include <CoreFoundation/CoreFoundation.h>
#include <CoreText/CoreText.h>
#include <objc/objc.h>
#include <objc/runtime.h>

extern "C" void objc_msgSend(void);
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
    float weight = 360.0f;
    float grade = 0.0f;
    float optical_size = 24.0f;
    bool fill = false;
    float size = 22.0f;
    float accessory_size = 13.0f;

    static auto toolbar() -> MaterialSymbolStyle {
        return {};
    }

    static auto navigation() -> MaterialSymbolStyle {
        auto style = MaterialSymbolStyle::toolbar();
        style.size = 24.0f;
        return style;
    }
};

struct ToolbarIconButton {
    std::string icon;
    std::string label;
    std::string accessory_icon;
    bool selected = false;
    bool enabled = true;
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
};

struct ToolbarGroup {
    std::vector<ToolbarIconButton> buttons;

    static auto of(std::initializer_list<ToolbarIconButton> buttons)
        -> ToolbarGroup {
        return ToolbarGroup{std::vector<ToolbarIconButton>{buttons}};
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
    float group_gap = 16.0f;
    float leading_reserved_width = 108.0f;
    float trailing_inset = 26.0f;
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
};

struct WindowOptions {
    int width = 960;
    int height = 720;
    std::string title = "Phenotype";
    WindowBackdrop backdrop = WindowBackdrop::opaque;
    WindowGlassEffect glass = {};
    WindowToolbar toolbar = {};
    WindowPadding padding = {};
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

auto objc_bool(bool value) -> ObjcBool {
    return static_cast<ObjcBool>(value ? 1 : 0);
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

auto color_with_alpha(id color, double alpha) -> id {
    return color == nil
        ? nil
        : send<id>(color, sel("colorWithAlphaComponent:"), alpha);
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
        send<id>(class_object("NSButton"), sel("alloc")),
        sel("initWithFrame:"),
        frame)};
    if (!button)
        return button;

    send<void>(button.get(), sel("setTitle:"), ns_string(""));
    send<void>(button.get(), sel("setBordered:"), objc_bool(false));
    send<void>(button.get(), sel("setTransparent:"), objc_bool(false));
    send<void>(button.get(), sel("setEnabled:"), objc_bool(item.enabled));
    send<void>(button.get(), sel("setToolTip:"), ns_string(item.label));
    auto const style = symbol_style(toolbar, item);
    auto tint = item.enabled
        ? label_color()
        : color_with_alpha(label_color(), 0.34);
    if (item.selected) {
        configure_layer(
            button.get(),
            toolbar.metrics.button_height * 0.5,
            color_with_white(1.0, 0.16),
            nil,
            0.0);
    }

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

auto toolbar_group_width(WindowToolbar const& toolbar,
                         ToolbarGroup const& group) -> CGFloat {
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

void add_toolbar_group(id toolbar_view,
                       CGRect frame,
                       WindowToolbar const& toolbar,
                       ToolbarGroup const& group,
                       NSUInteger autoresizing) {
    auto container = make_visual_effect_view(frame);
    if (!container)
        return;
    send<void>(container.get(), sel("setAutoresizingMask:"), autoresizing);
    configure_visual_effect_view(
        container.get(),
        WindowGlassEffect{
            .material = WindowGlassMaterial::under_window_background,
            .opacity = 0.72f,
        });
    configure_layer(
        container.get(),
        frame.size.height * 0.5,
        color_with_white(1.0, 0.08),
        color_with_white(1.0, 0.13));

    auto button_x = static_cast<CGFloat>(toolbar.metrics.group_padding_x);
    auto const button_y =
        (frame.size.height
         - static_cast<CGFloat>(toolbar.metrics.button_height)) * 0.5;
    for (auto const& item : group.buttons) {
        auto const button_width = toolbar_button_width(toolbar, item);
        auto button_frame = CGRect{
            {button_x, button_y},
            {button_width, static_cast<CGFloat>(toolbar.metrics.button_height)},
        };
        auto button = toolbar_button(button_frame, toolbar, item);
        if (button) {
            send<void>(button.get(), sel("setAutoresizingMask:"), 0ul);
            send<void>(container.get(), sel("addSubview:"), button.get());
        }
        button_x += button_width
            + static_cast<CGFloat>(toolbar.metrics.button_spacing);
    }

    send<void>(toolbar_view, sel("addSubview:"), container.get());
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
    for (NSInteger kind : {0, 1, 2}) {
        auto button = send<id>(
            window,
            sel("standardWindowButton:"),
            kind);
        if (!button || !responds_to(button, "frame")
            || !responds_to(button, "setFrameOrigin:"))
            continue;

        auto frame = send<CGRect>(button, sel("frame"));
        auto origin = CGPoint{
            frame.origin.x + horizontal,
            frame.origin.y - vertical,
        };
        send<void>(button, sel("setFrameOrigin:"), origin);
    }
}

void install_toolbar(id content_view,
                     id window,
                     CGRect bounds,
                     WindowPadding const& padding,
                     WindowToolbar const& toolbar) {
    if (!content_view || !toolbar.visible)
        return;

    auto const toolbar_height = static_cast<CGFloat>(toolbar.metrics.height);
    auto const group_height = static_cast<CGFloat>(toolbar.metrics.group_height);
    constexpr CGFloat title_height = 28.0;
    auto const width = bounds.size.width;
    auto const left_padding = static_cast<CGFloat>(std::max(0.0f, padding.left));
    auto const right_padding = static_cast<CGFloat>(std::max(0.0f, padding.right));
    auto const y = bounds.size.height - toolbar_height;
    auto const center_y = traffic_light_center_y(window, content_view, bounds);
    auto const center_in_toolbar = std::clamp(
        center_y - y,
        group_height * 0.5,
        toolbar_height - group_height * 0.5);
    auto const group_y = center_in_toolbar - group_height * 0.5;
    auto const title_y = center_in_toolbar - title_height * 0.5;
    auto toolbar_frame = CGRect{{0.0, y}, {width, toolbar_height}};
    auto toolbar_view = make_view(toolbar_frame);
    if (!toolbar_view)
        return;
    send<void>(
        toolbar_view.get(),
        sel("setAutoresizingMask:"),
        AppKitAutoresizingMask::width_sizable
            | AppKitAutoresizingMask::min_y_margin);

    auto leading_x =
        static_cast<CGFloat>(toolbar.metrics.leading_reserved_width)
        + left_padding;
    for (auto const& group : toolbar.leading_groups) {
        auto group_width = toolbar_group_width(toolbar, group);
        add_toolbar_group(
            toolbar_view.get(),
            CGRect{{leading_x, group_y}, {group_width, group_height}},
            toolbar,
            group,
            static_cast<NSUInteger>(AppKitAutoresizingMask::max_x_margin));
        leading_x += group_width
            + static_cast<CGFloat>(toolbar.metrics.group_gap);
    }

    if (!toolbar.title.empty()) {
        auto title = make_text_field(
            CGRect{{leading_x + 8.0, title_y}, {320.0, title_height}},
            toolbar.title,
            20.0,
            true);
        if (title) {
            send<void>(
                title.get(),
                sel("setAutoresizingMask:"),
                static_cast<NSUInteger>(AppKitAutoresizingMask::max_x_margin));
            send<void>(toolbar_view.get(), sel("addSubview:"), title.get());
        }
    }

    auto trailing_x = width
        - static_cast<CGFloat>(toolbar.metrics.trailing_inset)
        - right_padding;
    for (auto it = toolbar.trailing_groups.rbegin();
         it != toolbar.trailing_groups.rend();
         ++it) {
        auto group_width = toolbar_group_width(toolbar, *it);
        trailing_x -= group_width;
        add_toolbar_group(
            toolbar_view.get(),
            CGRect{{trailing_x, group_y}, {group_width, group_height}},
            toolbar,
            *it,
            static_cast<NSUInteger>(AppKitAutoresizingMask::min_x_margin));
        trailing_x -= static_cast<CGFloat>(toolbar.metrics.group_gap);
    }

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
    auto content = OwnedObjcObject{send<id>(
        send<id>(cls("NSView"), sel("alloc")),
        sel("initWithFrame:"),
        bounds)};
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
    auto content_view = install_glass_backdrop(window, bounds, options.glass);
    install_toolbar(
        content_view,
        window,
        bounds,
        options.padding,
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

    auto delegate = send<id>(
        send<id>(reinterpret_cast<id>(app_delegate_class()), sel("alloc")),
        sel("init"));
    send<void>(app, sel("setDelegate:"), delegate);

    auto width = static_cast<CGFloat>(std::max(1, options.width));
    auto height = static_cast<CGFloat>(std::max(1, options.height));
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
