module;
#if !defined(__wasi__) && !defined(__ANDROID__)
#include <algorithm>
#include <array>
#include <cmath>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <filesystem>
#include <limits>
#include <map>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <thread>
#include <variant>
#include <vector>

#ifdef __APPLE__
#define NS_PRIVATE_IMPLEMENTATION
#define MTL_PRIVATE_IMPLEMENTATION
#define CA_PRIVATE_IMPLEMENTATION
#include <Foundation/Foundation.hpp>
#include <Metal/Metal.hpp>
#include <QuartzCore/QuartzCore.hpp>

#include <CoreText/CoreText.h>
#include <CoreGraphics/CoreGraphics.h>
#include <objc/message.h>
#include <objc/runtime.h>
#endif
#endif

export module phenotype.native.macos;

#if !defined(__wasi__) && !defined(__ANDROID__)
import cppx.os;
import cppx.os.system;
import cppx.unicode;
import json;
import phenotype;
import phenotype.commands;
import phenotype.diag;
import phenotype.material;
import phenotype.state;
import phenotype.native.platform;
import phenotype.native.shell;
import phenotype.native.macos.objc;
import phenotype.native.macos.text;
import phenotype.native.macos.image;
import phenotype.native.macos.render;
import phenotype.native.macos.material;
import phenotype.native.macos.commands;
import phenotype.native.macos.shaders;
import phenotype.types;

#ifdef __APPLE__
namespace phenotype::native::detail {

inline constexpr long long insertion_indicator_display_mode_automatic = 0;
inline constexpr long long insertion_indicator_display_mode_hidden = 1;
inline constexpr long long insertion_indicator_display_mode_visible = 2;
inline constexpr long long insertion_indicator_automatic_mode_show_effects = 1 << 0;
inline constexpr long long insertion_indicator_automatic_mode_show_while_tracking = 1 << 1;
inline constexpr unsigned long long ns_event_mask_scroll_wheel = 1ull << 22;
// AppKit's `NSEventMaskMagnify`. Stage-8 canvas gestures install a
// parallel local-event monitor so trackpad pinch (`magnifyWithEvent:`)
// shows up here even when the focused content view does not directly
// override the selector.
inline constexpr unsigned long long ns_event_mask_magnify     = 1ull << 30;
// `NSEventModifierFlagCommand` for Cmd+scroll-wheel zoom routing.
inline constexpr unsigned long long ns_event_modifier_command = 1ull << 20;
inline constexpr unsigned long long ns_event_phase_none = 0;
inline constexpr unsigned long long ns_event_phase_began = 1ull << 0;
inline constexpr unsigned long long ns_event_phase_stationary = 1ull << 1;
inline constexpr unsigned long long ns_event_phase_changed = 1ull << 2;
inline constexpr unsigned long long ns_event_phase_ended = 1ull << 3;
inline constexpr unsigned long long ns_event_phase_cancelled = 1ull << 4;
inline constexpr unsigned long long ns_event_phase_may_begin = 1ull << 5;

inline bool g_force_disable_system_caret_for_tests = false;

struct MacOSScrollRuntimeEvent {
    bool available = false;
    char const* source = "none";
    bool has_precise_scrolling_deltas = false;
    double raw_delta_x = 0.0;
    double raw_delta_y = 0.0;
    float line_height = 0.0f;
    float app_vertical_multiplier = 1.0f;
    float app_horizontal_multiplier = 1.0f;
    float normalized_delta_x = 0.0f;
    float normalized_delta_y = 0.0f;
    float viewport_width = 0.0f;
    float viewport_height = 0.0f;
    unsigned long long phase = 0;
    unsigned long long momentum_phase = 0;
    bool scroll_tracking_changed = false;
    bool handled_x = false;
    bool handled_y = false;
};

struct ImeState {
    id ns_window = nullptr;
    id content_view = nullptr;
    id editor_view = nullptr;
    id caret_host_view = nullptr;
    id insertion_indicator = nullptr;
    id scroll_monitor = nullptr;
    id magnify_monitor = nullptr;  // Stage-8 trackpad-pinch gesture monitor
    Class editor_class = Nil;
    Class caret_host_class = Nil;
    void (*request_repaint)() = nullptr;
    bool composition_active = false;
    std::string marked_text;
    CocoaRange marked_selection = {0, 0};
    std::size_t composition_anchor = 0;
    std::size_t replacement_start = 0;
    std::size_t replacement_end = 0;
    bool scroll_tracking_active = false;
    unsigned int focused_callback_id = ::phenotype::native::invalid_callback_id;
    CGRect last_host_frame = CGRectNull;
    CGRect last_indicator_frame = CGRectNull;
    CGRect last_character_rect_draw = CGRectNull;
    CGRect last_character_rect_host = CGRectNull;
    long long last_indicator_display_mode = -1;
    CGRect last_character_rect_screen = CGRectNull;
    MacOSScrollRuntimeEvent last_scroll_event{};
};

inline ImeState g_ime;

// Convert a batch's logical-pixel scissor rect into a Metal scissor in
// physical pixels, clamped to the drawable bounds. Returns a zero-sized
// rect when the intersection is empty so the caller can skip draws.
// Sentinel (w==0 && h==0) means "full drawable" — emitted by
// emit_scissor_reset and used for the initial pre-Scissor state.
inline MTL::ScissorRect compute_metal_scissor(ScissorBatch const& b,
                                              float scale,
                                              std::uint32_t drawable_w,
                                              std::uint32_t drawable_h) {
    if (b.w == 0.0f && b.h == 0.0f)
        return MTL::ScissorRect{0, 0, drawable_w, drawable_h};
    float fx = std::floor(b.x * scale);
    float fy = std::floor(b.y * scale);
    float fr = std::ceil((b.x + b.w) * scale);
    float fb = std::ceil((b.y + b.h) * scale);
    long long x = std::max<long long>(0, static_cast<long long>(fx));
    long long y = std::max<long long>(0, static_cast<long long>(fy));
    long long r = std::min<long long>(static_cast<long long>(drawable_w),
                                       static_cast<long long>(fr));
    long long btm = std::min<long long>(static_cast<long long>(drawable_h),
                                         static_cast<long long>(fb));
    if (r <= x || btm <= y)
        return MTL::ScissorRect{0, 0, 0, 0};
    return MTL::ScissorRect{
        NS::UInteger(x), NS::UInteger(y),
        NS::UInteger(r - x), NS::UInteger(btm - y)};
}

inline int quantize_metric(float value) noexcept {
    return static_cast<int>(std::lround(value * 1000.0f));
}

inline std::vector<metrics::Attribute> native_attrs(char const* phase) {
    return {{"platform", "macos"}, {"phase", phase}};
}

inline std::vector<metrics::Attribute> native_platform_attrs() {
    return {{"platform", "macos"}};
}

inline std::vector<metrics::Attribute> native_buffer_attrs(char const* buffer) {
    return {{"platform", "macos"}, {"buffer", buffer}};
}

inline float measure_utf8_line_offset(float font_size,
                                      bool mono,
                                      std::string const& text,
                                      std::size_t bytes) {
    bytes = ::phenotype::detail::clamp_utf8_boundary(text, bytes);
    if (bytes == 0)
        return 0.0f;

    auto font = copy_text_font(font_size, mono);
    if (!font)
        return measure_utf8_prefix(font_size, mono, text, bytes);

    auto line = create_text_line(
        font,
        text.data(),
        static_cast<unsigned int>(text.size()));
    if (!line)
        return measure_utf8_prefix(font_size, mono, text, bytes);

    auto utf16_offset = static_cast<CFIndex>(
        utf16_length(std::string_view(text.data(), bytes)));
    CGFloat secondary = 0.0;
    auto primary = CTLineGetOffsetForStringIndex(line, utf16_offset, &secondary);
    if (!std::isfinite(primary))
        return measure_utf8_prefix(font_size, mono, text, bytes);
    if (std::isfinite(secondary) && secondary > primary)
        primary = secondary;
    return static_cast<float>(primary);
}

inline void reset_text_cache(TextAtlasCache& cache) {
    bool had_entries = !cache.entries.empty();
    cache.entries.clear();
    cache.cursor_x = 0;
    cache.cursor_y = 0;
    cache.row_height = 0;
    if (cache.pixels.size()
        != static_cast<std::size_t>(TextAtlasCache::atlas_size * TextAtlasCache::atlas_size)) {
        cache.pixels.assign(
            static_cast<std::size_t>(TextAtlasCache::atlas_size * TextAtlasCache::atlas_size), 0);
    } else {
        std::fill(cache.pixels.begin(), cache.pixels.end(), 0);
    }
    cache.dirty = had_entries;
    cache.dirty_min_x = had_entries ? 0 : TextAtlasCache::atlas_size;
    cache.dirty_min_y = had_entries ? 0 : TextAtlasCache::atlas_size;
    cache.dirty_max_x = had_entries ? TextAtlasCache::atlas_size : 0;
    cache.dirty_max_y = had_entries ? TextAtlasCache::atlas_size : 0;
}

inline void mark_text_cache_dirty(TextAtlasCache& cache,
                                  int x, int y, int w, int h) {
    cache.dirty = true;
    if (x < cache.dirty_min_x) cache.dirty_min_x = x;
    if (y < cache.dirty_min_y) cache.dirty_min_y = y;
    if (x + w > cache.dirty_max_x) cache.dirty_max_x = x + w;
    if (y + h > cache.dirty_max_y) cache.dirty_max_y = y + h;
}

inline bool reserve_text_slot(TextAtlasCache& cache, int width, int height,
                              int& out_x, int& out_y) {
    if (width <= 0 || height <= 0
        || width > TextAtlasCache::atlas_size
        || height > TextAtlasCache::atlas_size)
        return false;

    if (cache.cursor_x + width > TextAtlasCache::atlas_size) {
        cache.cursor_x = 0;
        cache.cursor_y += cache.row_height;
        cache.row_height = 0;
    }
    if (cache.cursor_y + height > TextAtlasCache::atlas_size)
        return false;

    out_x = cache.cursor_x;
    out_y = cache.cursor_y;
    cache.cursor_x += width;
    if (height > cache.row_height) cache.row_height = height;
    return true;
}

inline bool font_keys_equal(FontCacheKey const& a, FontCacheKey const& b) noexcept {
    return a.family == b.family && a.weight == b.weight
        && a.style  == b.style  && a.mono   == b.mono;
}

inline bool text_cache_matches(TextCacheEntry const& entry,
                               ParsedTextRun const& run,
                               int font_size_key,
                               int line_height_key,
                               int scale_key,
                               int width_factor_key) {
    return entry.font_size_key == font_size_key
        && entry.line_height_key == line_height_key
        && entry.scale_key == scale_key
        && entry.width_factor_key == width_factor_key
        && entry.mono == run.mono
        && font_keys_equal(entry.font_key, run.font_key)
        && entry.text.size() == run.len
        && std::memcmp(entry.text.data(), run.text, run.len) == 0;
}

inline TextCacheEntry* find_text_cache_entry(TextAtlasCache& cache,
                                             ParsedTextRun const& run,
                                             int font_size_key,
                                             int line_height_key,
                                             int scale_key,
                                             int width_factor_key) {
    for (auto& entry : cache.entries) {
        if (text_cache_matches(entry, run, font_size_key, line_height_key,
                               scale_key, width_factor_key))
            return &entry;
    }
    return nullptr;
}

inline bool rasterize_text_run(char const* text_ptr, unsigned int len,
                               float font_size, FontCacheKey const& font_key,
                               float line_height, float scale,
                               float width_factor,
                               RasterizedTextRun& out) {
    out = {};
    if (!g_text.initialized || len == 0)
        return false;

    auto font = copy_text_font(font_size, font_key, width_factor);
    if (!font)
        return false;
    auto line = create_text_line(font, text_ptr, len);
    if (!line)
        return false;

    TextLineMetrics line_metrics;
    if (!describe_text_line(line, line_metrics))
        return false;

    int padding = static_cast<int>(std::ceil(scale)) + 1;
    if (padding < 2) padding = 2;

    auto box = make_line_box(
        line_metrics.logical_width,
        line_height > 0.0f ? line_height : font_size * 1.6f,
        line_metrics.ascent,
        line_metrics.descent,
        line_metrics.leading,
        scale,
        padding);
    int line_origin_x = padding;
    expand_line_box_for_ink(
        box,
        line_metrics.glyph_bounds,
        line_metrics.logical_width,
        scale,
        padding,
        line_origin_x);

    std::vector<std::uint8_t> slot_alpha;
    int ink_min_x = box.slot_width;
    int ink_max_x = -1;
    if (!rasterize_line_alpha(
            line,
            box.slot_width,
            box.slot_height,
            line_origin_x,
            box.baseline_y,
            scale,
            slot_alpha,
            ink_min_x,
            ink_max_x)
        || ink_max_x < ink_min_x) {
        return false;
    }

    out.has_ink = true;
    auto span = compute_line_render_span(
        ink_min_x,
        ink_max_x,
        line_origin_x,
        line_metrics.logical_width,
        scale);
    out.pixel_width = span.end_x - span.start_x;
    out.pixel_height = box.render_height;
    out.x_offset = static_cast<float>(span.start_x - span.line_origin_x) / scale;
    out.width = static_cast<float>(out.pixel_width) / scale;
    out.height = static_cast<float>(out.pixel_height) / scale;
    out.alpha.resize(static_cast<std::size_t>(out.pixel_width * out.pixel_height), 0);

    for (int row = 0; row < out.pixel_height; ++row) {
        auto src = &slot_alpha[static_cast<std::size_t>(
            (box.render_top + row) * box.slot_width + span.start_x)];
        auto* dst = out.alpha.data()
            + static_cast<std::size_t>(row * out.pixel_width);
        std::memcpy(dst, src, static_cast<std::size_t>(out.pixel_width));
    }

    return true;
}

struct MacOSAccessibilityDisplayOptions {
    bool reduce_transparency = false;
    bool increase_contrast = false;
    bool reduce_motion = false;
    char const* source = "system";
};

inline bool env_token_enabled(std::string_view config,
                              std::string_view token) noexcept {
    if (token.empty())
        return false;
    std::size_t start = 0;
    while (start < config.size()) {
        while (start < config.size()
               && (config[start] == ',' || config[start] == ';'
                   || config[start] == ' ' || config[start] == '+')) {
            ++start;
        }
        auto end = start;
        while (end < config.size()
               && config[end] != ',' && config[end] != ';'
               && config[end] != ' ' && config[end] != '+') {
            ++end;
        }
        if (config.substr(start, end - start) == token)
            return true;
        start = end;
    }
    return false;
}

inline bool workspace_bool_property(id workspace, SEL selector) {
    using ObjcBool = signed char;
    if (!workspace)
        return false;
    if (!objc_send<ObjcBool>(workspace, sel_responds_to_selector(), selector))
        return false;
    return objc_send<ObjcBool>(workspace, selector) != 0;
}

inline MacOSAccessibilityDisplayOptions system_accessibility_display_options() {
    MacOSAccessibilityDisplayOptions options{};
    auto workspace_class = static_cast<Class>(objc_getClass("NSWorkspace"));
    if (!workspace_class)
        return options;
    auto workspace = objc_send<id>(class_as_id(workspace_class), sel_shared_workspace());
    options.reduce_transparency = workspace_bool_property(
        workspace,
        sel_accessibility_display_should_reduce_transparency());
    options.increase_contrast = workspace_bool_property(
        workspace,
        sel_accessibility_display_should_increase_contrast());
    options.reduce_motion = workspace_bool_property(
        workspace,
        sel_accessibility_display_should_reduce_motion());
    return options;
}

inline MacOSAccessibilityDisplayOptions accessibility_display_options() {
    auto const* raw = std::getenv("PHENOTYPE_ACCESSIBILITY_DISPLAY");
    if (!raw || raw[0] == '\0')
        return system_accessibility_display_options();

    std::string_view config{raw};
    if (config == "system")
        return system_accessibility_display_options();
    if (config == "standard" || config == "off" || config == "none") {
        MacOSAccessibilityDisplayOptions options{};
        options.source = "env-standard";
        return options;
    }

    MacOSAccessibilityDisplayOptions options{};
    options.source = "env-override";
    options.reduce_transparency =
        env_token_enabled(config, "reduce-transparency");
    options.increase_contrast =
        env_token_enabled(config, "increase-contrast");
    options.reduce_motion = env_token_enabled(config, "reduce-motion");
    return options;
}

inline float bounded_system_setting(
        float value,
        float fallback,
        float minimum,
        float maximum) noexcept {
    if (!(value > 0.0f) || !std::isfinite(value))
        return fallback;
    if (value < minimum)
        return minimum;
    if (value > maximum)
        return maximum;
    return value;
}

inline std::string scroller_style_name(long style) {
    if (style == 0)
        return "legacy";
    if (style == 1)
        return "overlay";
    return "unknown";
}

inline std::optional<float> appkit_class_float_noarg(char const* class_name,
                                                     SEL selector) {
    auto type = static_cast<Class>(objc_getClass(class_name));
    if (!type || !objc_responds_to(class_as_id(type), selector))
        return std::nullopt;
    double const value = objc_send<double>(class_as_id(type), selector);
    if (!(value > 0.0) || !std::isfinite(value))
        return std::nullopt;
    return static_cast<float>(value);
}

inline std::optional<float> appkit_scroller_width(long scroller_style) {
    auto scroller_class = static_cast<Class>(objc_getClass("NSScroller"));
    if (!scroller_class
        || !objc_responds_to(
            class_as_id(scroller_class),
            sel_scroller_width_for_control_size_scroller_style())) {
        return std::nullopt;
    }
    constexpr long ns_regular_control_size = 0;
    double const width = objc_send<double>(
        class_as_id(scroller_class),
        sel_scroller_width_for_control_size_scroller_style(),
        ns_regular_control_size,
        scroller_style);
    if (!(width >= 0.0) || !std::isfinite(width))
        return std::nullopt;
    return static_cast<float>(width);
}

inline std::string objc_string_to_utf8(id value) {
    if (!value || !objc_responds_to(value, sel_utf8_string()))
        return {};
    char const* raw = objc_send<char const*>(value, sel_utf8_string());
    return raw ? std::string{raw} : std::string{};
}

inline std::string macos_effective_appearance_name() {
    id appearance = nullptr;
    auto app_class = static_cast<Class>(objc_getClass("NSApplication"));
    if (app_class) {
        auto app = objc_send<id>(class_as_id(app_class), sel_shared_application());
        if (app && objc_responds_to(app, sel_effective_appearance())) {
            appearance = objc_send<id>(app, sel_effective_appearance());
        }
    }
    if (!appearance) {
        auto appearance_class =
            static_cast<Class>(objc_getClass("NSAppearance"));
        if (appearance_class
            && objc_responds_to(
                class_as_id(appearance_class),
                sel_current_appearance())) {
            appearance = objc_send<id>(
                class_as_id(appearance_class),
                sel_current_appearance());
        }
    }
    if (!appearance || !objc_responds_to(appearance, sel_name()))
        return {};
    return objc_string_to_utf8(objc_send<id>(appearance, sel_name()));
}

inline void apply_appearance_to_system_snapshot(
        ::phenotype::PlatformSystemSettingsSnapshot& snapshot,
        bool increase_contrast) {
    auto name = macos_effective_appearance_name();
    snapshot.appearance_name = name;
    snapshot.color_scheme_source = name.empty()
        ? "fallback"
        : "NSApplication.effectiveAppearance.name";
    bool const dark = name.find("Dark") != std::string::npos
        || name.find("dark") != std::string::npos;
    if (increase_contrast)
        snapshot.color_scheme = dark
            ? "high-contrast-dark"
            : "high-contrast-light";
    else
        snapshot.color_scheme = dark ? "dark" : "light";
}

inline unsigned char srgb_byte(double value) {
    if (!std::isfinite(value))
        return 0;
    if (value < 0.0)
        value = 0.0;
    if (value > 1.0)
        value = 1.0;
    return static_cast<unsigned char>(value * 255.0 + 0.5);
}

inline std::optional<::phenotype::Color> nscolor_to_srgb_color(id color) {
    auto color_space_class = static_cast<Class>(objc_getClass("NSColorSpace"));
    if (!color || !color_space_class)
        return std::nullopt;
    auto rgb_space = objc_send<id>(
        class_as_id(color_space_class),
        sel_generic_rgb_color_space());
    if (!rgb_space)
        return std::nullopt;
    auto rgb_color = objc_send<id>(
        color,
        sel_color_using_color_space(),
        rgb_space);
    if (!rgb_color
        || !objc_responds_to(rgb_color, sel_get_red_green_blue_alpha())) {
        return std::nullopt;
    }

    double r = 0.0;
    double g = 0.0;
    double b = 0.0;
    double a = 1.0;
    objc_send<void>(
        rgb_color,
        sel_get_red_green_blue_alpha(),
        &r,
        &g,
        &b,
        &a);
    return ::phenotype::Color{
        srgb_byte(r),
        srgb_byte(g),
        srgb_byte(b),
        srgb_byte(a),
    };
}

inline ::phenotype::PlatformSystemSettingsSnapshot
macos_system_settings_snapshot() {
    ::phenotype::PlatformSystemSettingsSnapshot snapshot{};
    snapshot.source = "macos-appkit-coretext";
    snapshot.font_family_source =
        "CTFontCopyFamilyName(kCTFontUIFontSystem)";
    snapshot.text_size_source =
        "NSFont.systemFontSize/smallSystemFontSize";
    snapshot.scroll_source =
        "NSEvent.scrollingDelta/hasPreciseScrollingDeltas with NSScroller.preferredScrollerStyle";
    snapshot.input_timing_source =
        "NSEvent.doubleClickInterval/keyRepeatDelay/keyRepeatInterval";
    snapshot.preferred_locale_source = "NSLocale.preferredLanguages";
    snapshot.line_height_ratio = 1.6f;
    snapshot.scroll_delta_multiplier = 1.0f;
    snapshot.scroll_horizontal_delta_multiplier = 1.0f;
    auto accessibility = accessibility_display_options();
    snapshot.reduce_transparency = accessibility.reduce_transparency;
    snapshot.increase_contrast = accessibility.increase_contrast;
    snapshot.reduce_motion = accessibility.reduce_motion;
    snapshot.accessibility_source = accessibility.source;
    apply_appearance_to_system_snapshot(
        snapshot,
        accessibility.increase_contrast);

    auto system_font_size = appkit_class_float_noarg(
        "NSFont",
        sel_system_font_size());
    auto small_system_font_size = appkit_class_float_noarg(
        "NSFont",
        sel_small_system_font_size());

    CTFontRef raw_font = CTFontCreateUIFontForLanguage(
        kCTFontUIFontSystem,
        system_font_size.value_or(0.0f),
        nullptr);
    if (!raw_font)
        raw_font = CTFontCreateUIFontForLanguage(
            kCTFontUIFontSystem,
            system_font_size.value_or(13.0f),
            nullptr);
    auto font = CFGuard<CTFontRef>(raw_font);
    if (font) {
        snapshot.body_font_size = bounded_system_setting(
            system_font_size.value_or(
                static_cast<float>(CTFontGetSize(font.ref))),
            13.0f,
            8.0f,
            40.0f);
        auto family = CFGuard<CFStringRef>(CTFontCopyFamilyName(font.ref));
        snapshot.font_family = cf_string_to_utf8(family.ref);
    } else {
        snapshot.body_font_size = 13.0f;
        snapshot.font_family = ".AppleSystemUIFont";
    }
    snapshot.font_scale = bounded_system_setting(
        snapshot.body_font_size / 13.0f,
        1.0f,
        0.75f,
        1.8f);
    snapshot.heading_font_size = snapshot.body_font_size * 1.2857143f;
    snapshot.small_font_size = bounded_system_setting(
        small_system_font_size.value_or(snapshot.body_font_size * 0.8571429f),
        snapshot.body_font_size * 0.8571429f,
        8.0f,
        32.0f);
    snapshot.scroll_line_height =
        snapshot.body_font_size * snapshot.line_height_ratio;

    if (auto seconds = appkit_class_float_noarg(
            "NSEvent",
            sel_double_click_interval())) {
        snapshot.double_click_interval_ms = bounded_system_setting(
            *seconds * 1000.0f,
            500.0f,
            50.0f,
            5000.0f);
    }
    if (auto seconds = appkit_class_float_noarg(
            "NSEvent",
            sel_key_repeat_delay())) {
        snapshot.key_repeat_delay_ms = bounded_system_setting(
            *seconds * 1000.0f,
            500.0f,
            50.0f,
            5000.0f);
    }
    if (auto seconds = appkit_class_float_noarg(
            "NSEvent",
            sel_key_repeat_interval())) {
        snapshot.key_repeat_interval_ms = bounded_system_setting(
            *seconds * 1000.0f,
            50.0f,
            10.0f,
            1000.0f);
    }
    snapshot.caret_blink_interval_ms = 530.0f;

    long scroller_style = -1;
    auto scroller_class = static_cast<Class>(objc_getClass("NSScroller"));
    if (scroller_class
        && objc_responds_to(
            class_as_id(scroller_class),
            sel_preferred_scroller_style())) {
        scroller_style = objc_send<long>(
            class_as_id(scroller_class),
            sel_preferred_scroller_style());
    }
    snapshot.preferred_scroller_style = scroller_style_name(scroller_style);
    snapshot.overlay_scrollbars = scroller_style == 1;
    snapshot.scroll_wheel_lines = 1.0f;
    snapshot.scroll_page_mode = false;
    snapshot.scroll_vertical_factor = snapshot.scroll_line_height;
    snapshot.scroll_horizontal_factor = snapshot.scroll_line_height;
    if (scroller_style >= 0) {
        if (auto width = appkit_scroller_width(scroller_style))
            snapshot.scroll_bar_size = *width;
    }
    auto color_class = static_cast<Class>(objc_getClass("NSColor"));
    if (color_class
        && objc_responds_to(
            class_as_id(color_class),
            sel_control_accent_color())) {
        auto accent = objc_send<id>(
            class_as_id(color_class),
            sel_control_accent_color());
        if (auto color = nscolor_to_srgb_color(accent)) {
            snapshot.accent_color_available = true;
            snapshot.accent_color = *color;
            snapshot.accent_color_source = "NSColor.controlAccentColor";
            snapshot.accent_color_opaque = color->a == 255;
        }
    }
    if (auto locale = macos_preferred_locale(); !locale.empty())
        snapshot.preferred_locale = std::move(locale);
    return snapshot;
}

struct RendererState {
    MTL::Device* device = nullptr;
    MTL::CommandQueue* queue = nullptr;
    CA::MetalLayer* layer = nullptr;
    MTL::RenderPipelineState* tri_pipeline = nullptr;
    MTL::RenderPipelineState* color_pipeline = nullptr;
    MTL::RenderPipelineState* material_pipeline = nullptr;
    MTL::RenderPipelineState* arc_pipeline = nullptr;
    MTL::RenderPipelineState* image_pipeline = nullptr;
    MTL::RenderPipelineState* text_pipeline = nullptr;
    MTL::Buffer* uniform_buf = nullptr;
    MTL::Buffer* tri_vertices_buf = nullptr;
    std::size_t tri_vertices_capacity = 0;
    MTL::Buffer* color_instances_buf = nullptr;
    std::size_t color_instances_capacity = 0;
    MTL::Buffer* material_instances_buf = nullptr;
    std::size_t material_instances_capacity = 0;
    MTL::Buffer* arc_instances_buf = nullptr;
    std::size_t arc_instances_capacity = 0;
    MTL::Buffer* overlay_color_instances_buf = nullptr;
    std::size_t overlay_color_instances_capacity = 0;
    MTL::Buffer* image_instances_buf = nullptr;
    std::size_t image_instances_capacity = 0;
    MTL::Buffer* text_instances_buf = nullptr;
    std::size_t text_instances_capacity = 0;
    MTL::Texture* debug_capture_texture = nullptr;
    int debug_capture_width = 0;
    int debug_capture_height = 0;
    MTL::Texture* material_backdrop_texture = nullptr;
    int material_backdrop_width = 0;
    int material_backdrop_height = 0;
    MTL::Buffer* material_backdrop_luma_sample_buf = nullptr;
    std::size_t material_backdrop_luma_sample_capacity = 0;
    MTL::CommandBuffer* material_backdrop_luma_pending_command_buffer = nullptr;
    std::uint32_t material_backdrop_luma_pending_sample_count = 0;
    std::uint32_t material_backdrop_luma_pending_grid_width = 0;
    std::uint32_t material_backdrop_luma_pending_grid_height = 0;
    std::uint32_t material_backdrop_luma_pending_frame = 0;
    std::uint32_t material_backdrop_luma_skipped_sample_count = 0;
    MTL::Buffer* frame_readback_buf = nullptr;
    std::size_t frame_readback_capacity = 0;
    MTL::Texture* image_atlas_texture = nullptr;
    MTL::Texture* text_atlas_texture = nullptr;
    MTL::SamplerState* sampler = nullptr;
    std::vector<HitRegionCmd> hit_regions;
    FrameScratch scratch;
    TextAtlasCache text_cache;
    NativeSurfaceDescriptor* surface = nullptr;
    id ns_window = nullptr;
    id content_view = nullptr;
    int drawable_width = 0;
    int drawable_height = 0;
    int last_render_width = 0;
    int last_render_height = 0;
    double last_clear_alpha = 1.0;
    bool last_clear_alpha_for_transparent_window = false;
    std::uint32_t last_full_frame_opaque_fill_count = 0;
    bool last_transparent_window_has_opaque_frame_fill = false;
    std::uint32_t material_frame_sequence = 0;
    MacOSAccessibilityDisplayOptions accessibility_options{};
    MaterialExecutorSummary material_executor_summary{};
    bool last_frame_available = false;
    bool last_material_backdrop_available = false;
    bool last_material_backdrop_excludes_foreground_text = false;
    bool last_material_backdrop_color_available = false;
    Color last_material_backdrop_color_mean = {255, 255, 255, 255};
    bool last_material_backdrop_luma_available = false;
    float last_material_backdrop_luma_min = 0.0f;
    float last_material_backdrop_luma_max = 1.0f;
    float last_material_backdrop_luma_mean = 0.5f;
    std::uint32_t last_material_backdrop_luma_sample_count = 0;
    std::uint32_t last_material_backdrop_luma_grid_width = 0;
    std::uint32_t last_material_backdrop_luma_grid_height = 0;
    std::uint32_t last_material_backdrop_luma_frame = 0;
    char const* last_material_backdrop_luma_status = "not-sampled";
    bool initialized = false;
};

inline RendererState g_renderer;

inline NativeSurfaceDescriptor* desktop_surface(native_surface_handle handle) {
    return static_cast<NativeSurfaceDescriptor*>(handle);
}

inline id surface_ns_window(NativeSurfaceDescriptor const* surface) {
    if (!surface)
        return nullptr;
    if (surface->kind == NativeSurfaceKind::MacOSWindow)
        return static_cast<id>(surface->window);
    return nullptr;
}

inline id surface_content_view(NativeSurfaceDescriptor const* surface) {
    if (!surface)
        return nullptr;
    if (surface->view)
        return static_cast<id>(surface->view);
    if (auto ns_window = surface_ns_window(surface))
        return objc_send<id>(ns_window, sel_content_view());
    return nullptr;
}

inline float surface_content_scale(NativeSurfaceDescriptor const* surface) {
    if (!surface)
        return 1.0f;
    float const scale = surface->content_scale;
    return (scale > 0.0f && std::isfinite(scale)) ? scale : 1.0f;
}

inline void surface_framebuffer_size(NativeSurfaceDescriptor const* surface,
                                     int& width,
                                     int& height) {
    width = 0;
    height = 0;
    if (!surface)
        return;
    width = surface->framebuffer_width;
    height = surface->framebuffer_height;
}

inline void surface_logical_size(NativeSurfaceDescriptor const* surface,
                                 int& width,
                                 int& height) {
    width = 0;
    height = 0;
    if (!surface)
        return;
    width = surface->logical_width;
    height = surface->logical_height;
}

inline MTL::RenderPipelineState* create_pipeline(
        MTL::Device* device, MTL::Library* lib,
        char const* vs_name, char const* fs_name) {
    auto* vs_fn = lib->newFunction(NS::String::string(vs_name, NS::UTF8StringEncoding));
    auto* fs_fn = lib->newFunction(NS::String::string(fs_name, NS::UTF8StringEncoding));
    if (!vs_fn || !fs_fn) {
        if (vs_fn) vs_fn->release();
        if (fs_fn) fs_fn->release();
        return nullptr;
    }

    auto* desc = MTL::RenderPipelineDescriptor::alloc()->init();
    desc->setVertexFunction(vs_fn);
    desc->setFragmentFunction(fs_fn);

    auto* attachment = desc->colorAttachments()->object(0);
    attachment->setPixelFormat(MTL::PixelFormatBGRA8Unorm);
    attachment->setBlendingEnabled(true);
    attachment->setSourceRGBBlendFactor(MTL::BlendFactorSourceAlpha);
    attachment->setDestinationRGBBlendFactor(MTL::BlendFactorOneMinusSourceAlpha);
    attachment->setRgbBlendOperation(MTL::BlendOperationAdd);
    attachment->setSourceAlphaBlendFactor(MTL::BlendFactorOne);
    attachment->setDestinationAlphaBlendFactor(MTL::BlendFactorOneMinusSourceAlpha);
    attachment->setAlphaBlendOperation(MTL::BlendOperationAdd);

    NS::Error* err = nullptr;
    auto* pipeline = device->newRenderPipelineState(desc, &err);
    if (err) {
        std::fprintf(stderr, "[metal] pipeline error: %s\n",
                     err->localizedDescription()->utf8String());
    }
    desc->release();
    vs_fn->release();
    fs_fn->release();
    return pipeline;
}

inline bool ensure_instance_buffer(MTL::Buffer*& buffer,
                                   std::size_t& capacity,
                                   std::size_t required,
                                   char const* name) {
    if (required == 0)
        return true;
    if (required <= capacity && buffer)
        return true;

    std::size_t new_capacity = (capacity > 0) ? capacity : 4096;
    while (new_capacity < required)
        new_capacity *= 2;

    auto* replacement = g_renderer.device->newBuffer(
        NS::UInteger(new_capacity),
        MTL::ResourceStorageModeShared);
    if (!replacement)
        return false;

    if (buffer)
        buffer->release();
    buffer = replacement;
    capacity = new_capacity;
    metrics::inst::native_buffer_reallocations.add(1, native_buffer_attrs(name));
    return true;
}

inline bool ensure_text_atlas_texture() {
    if (g_renderer.text_atlas_texture)
        return true;

    auto* tex_desc = MTL::TextureDescriptor::texture2DDescriptor(
        MTL::PixelFormatR8Unorm,
        NS::UInteger(TextAtlasCache::atlas_size),
        NS::UInteger(TextAtlasCache::atlas_size),
        false);
    tex_desc->setUsage(MTL::TextureUsageShaderRead);
    g_renderer.text_atlas_texture = g_renderer.device->newTexture(tex_desc);
    return g_renderer.text_atlas_texture != nullptr;
}

inline bool ensure_image_atlas_texture() {
    if (g_renderer.image_atlas_texture)
        return true;

    auto* tex_desc = MTL::TextureDescriptor::texture2DDescriptor(
        MTL::PixelFormatRGBA8Unorm,
        NS::UInteger(ImageAtlasCache::atlas_size),
        NS::UInteger(ImageAtlasCache::atlas_size),
        false);
    tex_desc->setUsage(MTL::TextureUsageShaderRead);
    g_renderer.image_atlas_texture = g_renderer.device->newTexture(tex_desc);
    return g_renderer.image_atlas_texture != nullptr;
}

inline bool ensure_debug_capture_texture(int width, int height) {
    if (width <= 0 || height <= 0)
        return false;
    if (g_renderer.debug_capture_texture
        && g_renderer.debug_capture_width == width
        && g_renderer.debug_capture_height == height) {
        return true;
    }

    if (g_renderer.debug_capture_texture) {
        g_renderer.debug_capture_texture->release();
        g_renderer.debug_capture_texture = nullptr;
        g_renderer.debug_capture_width = 0;
        g_renderer.debug_capture_height = 0;
    }

    auto* tex_desc = MTL::TextureDescriptor::texture2DDescriptor(
        MTL::PixelFormatBGRA8Unorm,
        NS::UInteger(width),
        NS::UInteger(height),
        false);
    tex_desc->setUsage(MTL::TextureUsageShaderRead | MTL::TextureUsageRenderTarget);
    g_renderer.debug_capture_texture = g_renderer.device->newTexture(tex_desc);
    if (g_renderer.debug_capture_texture) {
        g_renderer.debug_capture_width = width;
        g_renderer.debug_capture_height = height;
    }
    return g_renderer.debug_capture_texture != nullptr;
}

inline void release_material_backdrop_luma_pending_command_buffer();

inline constexpr std::int64_t k_macos_material_max_backdrop_pixels =
    16'777'216;
inline constexpr std::int64_t k_macos_material_default_texture_dimension_2d =
    16'384;
inline constexpr std::int64_t k_macos_material_legacy_texture_dimension_2d =
    8'192;

inline std::int64_t macos_material_max_texture_dimension_2d(
        MTL::Device* device) noexcept {
    if (!device)
        return 0;
    if (device->supportsFamily(MTL::GPUFamilyApple7)
        || device->supportsFamily(MTL::GPUFamilyApple8)
        || device->supportsFamily(MTL::GPUFamilyApple9)
        || device->supportsFamily(MTL::GPUFamilyMac2)
        || device->supportsFamily(MTL::GPUFamilyCommon3)
        || device->supportsFamily(MTL::GPUFamilyMetal3)) {
        return k_macos_material_default_texture_dimension_2d;
    }
    return k_macos_material_legacy_texture_dimension_2d;
}

inline MaterialCapabilityInput macos_material_capability_input(
        MTL::Device* device,
        bool material_pipeline_ready,
        bool material_frame_history,
        MacOSAccessibilityDisplayOptions accessibility) noexcept {
    MaterialCapabilityInput capabilities{};
    capabilities.material_surfaces = true;
    capabilities.material_backdrop_blur = material_pipeline_ready;
    capabilities.shader_blur = material_pipeline_ready;
    capabilities.frame_history = material_frame_history;
    capabilities.reduce_transparency = accessibility.reduce_transparency;
    capabilities.increase_contrast = accessibility.increase_contrast;
    capabilities.reduce_motion = accessibility.reduce_motion;
    capabilities.max_shader_sample_taps = material_pipeline_ready
        ? material_max_sample_taps
        : 0u;
    capabilities.max_texture_dimension_2d =
        macos_material_max_texture_dimension_2d(device);
    capabilities.max_backdrop_pixels = material_pipeline_ready
        ? k_macos_material_max_backdrop_pixels
        : 0;
    capabilities.profile = material_pipeline_ready
        ? "macos-metal-liquid-glass"
        : "macos-metal-unavailable";
    capabilities.source = "MTLDevice.supportsFamily";
    return capabilities;
}

inline MaterialQualityPolicy macos_material_quality_policy(
        MaterialCapabilityInput capabilities) noexcept {
    auto policy = default_material_quality_policy();
    if (capabilities.max_backdrop_pixels > 0)
        policy.max_backdrop_pixels = capabilities.max_backdrop_pixels;
    if (capabilities.max_shader_sample_taps > 0)
        policy.max_sample_taps = capabilities.max_shader_sample_taps;
    return policy;
}

inline bool ensure_material_backdrop_texture(int width, int height) {
    if (width <= 0 || height <= 0)
        return false;
    if (g_renderer.material_backdrop_texture
        && g_renderer.material_backdrop_width == width
        && g_renderer.material_backdrop_height == height) {
        return true;
    }

    if (g_renderer.material_backdrop_texture) {
        release_material_backdrop_luma_pending_command_buffer();
        g_renderer.material_backdrop_texture->release();
        g_renderer.material_backdrop_texture = nullptr;
        g_renderer.material_backdrop_width = 0;
        g_renderer.material_backdrop_height = 0;
        g_renderer.last_material_backdrop_available = false;
        g_renderer.last_material_backdrop_excludes_foreground_text = false;
        g_renderer.last_material_backdrop_color_available = false;
        g_renderer.last_material_backdrop_color_mean = {255, 255, 255, 255};
        g_renderer.last_material_backdrop_luma_available = false;
        g_renderer.last_material_backdrop_luma_sample_count = 0;
        g_renderer.last_material_backdrop_luma_grid_width = 0;
        g_renderer.last_material_backdrop_luma_grid_height = 0;
        g_renderer.last_material_backdrop_luma_frame = 0;
        g_renderer.last_material_backdrop_luma_status = "not-sampled";
    }

    auto* tex_desc = MTL::TextureDescriptor::texture2DDescriptor(
        MTL::PixelFormatBGRA8Unorm,
        NS::UInteger(width),
        NS::UInteger(height),
        false);
    tex_desc->setUsage(MTL::TextureUsageShaderRead | MTL::TextureUsageRenderTarget);
    g_renderer.material_backdrop_texture =
        g_renderer.device->newTexture(tex_desc);
    if (g_renderer.material_backdrop_texture) {
        g_renderer.material_backdrop_width = width;
        g_renderer.material_backdrop_height = height;
    }
    return g_renderer.material_backdrop_texture != nullptr;
}

inline constexpr std::uint32_t k_material_backdrop_luma_grid_width = 5;
inline constexpr std::uint32_t k_material_backdrop_luma_grid_height = 5;
inline constexpr std::size_t k_material_backdrop_luma_row_stride = 256;

inline std::size_t material_backdrop_luma_sample_required_bytes(
        std::uint32_t sample_count) noexcept {
    return static_cast<std::size_t>(sample_count)
         * k_material_backdrop_luma_row_stride;
}

inline bool ensure_material_backdrop_luma_sample_buffer(
        std::uint32_t sample_count) {
    auto const required =
        material_backdrop_luma_sample_required_bytes(sample_count);
    if (required == 0)
        return false;
    if (required <= g_renderer.material_backdrop_luma_sample_capacity
        && g_renderer.material_backdrop_luma_sample_buf) {
        return true;
    }

    std::size_t new_capacity =
        g_renderer.material_backdrop_luma_sample_capacity > 0
            ? g_renderer.material_backdrop_luma_sample_capacity
            : k_material_backdrop_luma_row_stride * 32u;
    while (new_capacity < required)
        new_capacity *= 2u;

    auto* replacement = g_renderer.device->newBuffer(
        NS::UInteger(new_capacity),
        MTL::ResourceStorageModeShared);
    if (!replacement)
        return false;

    if (g_renderer.material_backdrop_luma_sample_buf)
        g_renderer.material_backdrop_luma_sample_buf->release();
    g_renderer.material_backdrop_luma_sample_buf = replacement;
    g_renderer.material_backdrop_luma_sample_capacity = new_capacity;
    return true;
}

inline float material_backdrop_sample_luma(std::uint8_t b,
                                           std::uint8_t g,
                                           std::uint8_t r) noexcept {
    return (0.2126f * static_cast<float>(r)
            + 0.7152f * static_cast<float>(g)
            + 0.0722f * static_cast<float>(b)) / 255.0f;
}

inline void release_material_backdrop_luma_pending_command_buffer() {
    if (g_renderer.material_backdrop_luma_pending_command_buffer) {
        g_renderer.material_backdrop_luma_pending_command_buffer->release();
        g_renderer.material_backdrop_luma_pending_command_buffer = nullptr;
    }
    g_renderer.material_backdrop_luma_pending_sample_count = 0;
    g_renderer.material_backdrop_luma_pending_grid_width = 0;
    g_renderer.material_backdrop_luma_pending_grid_height = 0;
    g_renderer.material_backdrop_luma_pending_frame = 0;
}

inline void process_completed_material_backdrop_luma_sample() {
    auto* command_buffer =
        g_renderer.material_backdrop_luma_pending_command_buffer;
    if (!command_buffer)
        return;

    auto const status = command_buffer->status();
    if (status != MTL::CommandBufferStatusCompleted
        && status != MTL::CommandBufferStatusError) {
        return;
    }

    if (status == MTL::CommandBufferStatusError
        || !g_renderer.material_backdrop_luma_sample_buf
        || g_renderer.material_backdrop_luma_pending_sample_count == 0) {
        g_renderer.last_material_backdrop_color_available = false;
        g_renderer.last_material_backdrop_luma_available = false;
        g_renderer.last_material_backdrop_luma_sample_count = 0;
        g_renderer.last_material_backdrop_luma_status =
            status == MTL::CommandBufferStatusError
                ? "sample-command-buffer-error"
                : "sample-buffer-unavailable";
        release_material_backdrop_luma_pending_command_buffer();
        return;
    }

    auto const* mapped = static_cast<std::uint8_t const*>(
        g_renderer.material_backdrop_luma_sample_buf->contents());
    if (!mapped) {
        g_renderer.last_material_backdrop_color_available = false;
        g_renderer.last_material_backdrop_luma_available = false;
        g_renderer.last_material_backdrop_luma_sample_count = 0;
        g_renderer.last_material_backdrop_luma_status =
            "sample-buffer-unmapped";
        release_material_backdrop_luma_pending_command_buffer();
        return;
    }

    float luma_min = 1.0f;
    float luma_max = 0.0f;
    float luma_sum = 0.0f;
    std::uint64_t red_sum = 0;
    std::uint64_t green_sum = 0;
    std::uint64_t blue_sum = 0;
    std::uint64_t alpha_sum = 0;
    std::uint32_t sampled = 0;
    for (std::uint32_t i = 0;
         i < g_renderer.material_backdrop_luma_pending_sample_count;
         ++i) {
        auto const* pixel =
            mapped + static_cast<std::size_t>(i)
                   * k_material_backdrop_luma_row_stride;
        auto const luma =
            material_backdrop_sample_luma(pixel[0], pixel[1], pixel[2]);
        luma_min = std::min(luma_min, luma);
        luma_max = std::max(luma_max, luma);
        luma_sum += luma;
        blue_sum += pixel[0];
        green_sum += pixel[1];
        red_sum += pixel[2];
        alpha_sum += pixel[3];
        ++sampled;
    }

    if (sampled > 0) {
        auto mean_channel = [sampled](std::uint64_t sum) {
            return static_cast<unsigned char>(
                std::min<std::uint64_t>(
                    255,
                    (sum + static_cast<std::uint64_t>(sampled / 2u))
                        / static_cast<std::uint64_t>(sampled)));
        };
        g_renderer.last_material_backdrop_color_available = true;
        g_renderer.last_material_backdrop_color_mean = Color{
            mean_channel(red_sum),
            mean_channel(green_sum),
            mean_channel(blue_sum),
            mean_channel(alpha_sum),
        };
        g_renderer.last_material_backdrop_luma_available = true;
        g_renderer.last_material_backdrop_luma_min = luma_min;
        g_renderer.last_material_backdrop_luma_max = luma_max;
        g_renderer.last_material_backdrop_luma_mean =
            luma_sum / static_cast<float>(sampled);
        g_renderer.last_material_backdrop_luma_sample_count = sampled;
        g_renderer.last_material_backdrop_luma_grid_width =
            g_renderer.material_backdrop_luma_pending_grid_width;
        g_renderer.last_material_backdrop_luma_grid_height =
            g_renderer.material_backdrop_luma_pending_grid_height;
        g_renderer.last_material_backdrop_luma_frame =
            g_renderer.material_backdrop_luma_pending_frame;
        g_renderer.last_material_backdrop_luma_status =
            "sampled-async-grid";
    }
    release_material_backdrop_luma_pending_command_buffer();
}

inline bool schedule_material_backdrop_luma_sample(
        MTL::BlitCommandEncoder* blit,
        MTL::CommandBuffer* command_buffer,
        MTL::Texture* source,
        int width,
        int height,
        std::uint32_t frame_index,
        MaterialExecutorSummary& summary) {
    if (!blit || !command_buffer || !source || width <= 0 || height <= 0) {
        ++summary.backdrop_luma_sampling_skipped_count;
        summary.backdrop_luma_sampling_skip_reason =
            "sample-source-unavailable";
        return false;
    }
    if (g_renderer.material_backdrop_luma_pending_command_buffer) {
        ++summary.backdrop_luma_sampling_skipped_count;
        summary.backdrop_luma_sampling_skip_reason =
            "previous-sample-pending";
        ++g_renderer.material_backdrop_luma_skipped_sample_count;
        return false;
    }

    constexpr std::uint32_t grid_w = k_material_backdrop_luma_grid_width;
    constexpr std::uint32_t grid_h = k_material_backdrop_luma_grid_height;
    constexpr std::uint32_t sample_count = grid_w * grid_h;
    if (!ensure_material_backdrop_luma_sample_buffer(sample_count)) {
        ++summary.backdrop_luma_sampling_skipped_count;
        summary.backdrop_luma_sampling_skip_reason =
            "sample-buffer-unavailable";
        return false;
    }

    for (std::uint32_t gy = 0; gy < grid_h; ++gy) {
        for (std::uint32_t gx = 0; gx < grid_w; ++gx) {
            auto const index = gy * grid_w + gx;
            auto const x = static_cast<std::uint32_t>(
                (static_cast<std::uint64_t>(gx) + 1u)
                * static_cast<std::uint64_t>(width)
                / static_cast<std::uint64_t>(grid_w + 1u));
            auto const y = static_cast<std::uint32_t>(
                (static_cast<std::uint64_t>(gy) + 1u)
                * static_cast<std::uint64_t>(height)
                / static_cast<std::uint64_t>(grid_h + 1u));
            MTL::Origin origin{
                NS::UInteger(std::min<std::uint32_t>(
                    x, static_cast<std::uint32_t>(width - 1))),
                NS::UInteger(std::min<std::uint32_t>(
                    y, static_cast<std::uint32_t>(height - 1))),
                NS::UInteger(0),
            };
            MTL::Size size{NS::UInteger(1), NS::UInteger(1), NS::UInteger(1)};
            blit->copyFromTexture(
                source,
                NS::UInteger(0),
                NS::UInteger(0),
                origin,
                size,
                g_renderer.material_backdrop_luma_sample_buf,
                NS::UInteger(
                    static_cast<std::size_t>(index)
                    * k_material_backdrop_luma_row_stride),
                NS::UInteger(k_material_backdrop_luma_row_stride),
                NS::UInteger(k_material_backdrop_luma_row_stride));
        }
    }

    g_renderer.material_backdrop_luma_pending_sample_count = sample_count;
    g_renderer.material_backdrop_luma_pending_grid_width = grid_w;
    g_renderer.material_backdrop_luma_pending_grid_height = grid_h;
    g_renderer.material_backdrop_luma_pending_frame = frame_index;
    command_buffer->retain();
    g_renderer.material_backdrop_luma_pending_command_buffer = command_buffer;
    return true;
}

inline bool ensure_frame_readback_buffer(std::size_t required) {
    if (required == 0)
        return false;
    if (required <= g_renderer.frame_readback_capacity && g_renderer.frame_readback_buf)
        return true;

    std::size_t new_capacity = (g_renderer.frame_readback_capacity > 0)
        ? g_renderer.frame_readback_capacity
        : 4096;
    while (new_capacity < required)
        new_capacity *= 2;

    auto* replacement = g_renderer.device->newBuffer(
        NS::UInteger(new_capacity),
        MTL::ResourceStorageModeShared);
    if (!replacement)
        return false;

    if (g_renderer.frame_readback_buf)
        g_renderer.frame_readback_buf->release();
    g_renderer.frame_readback_buf = replacement;
    g_renderer.frame_readback_capacity = new_capacity;
    return true;
}

inline void sync_drawable_size(int fbw, int fbh) {
    if (fbw == g_renderer.drawable_width && fbh == g_renderer.drawable_height)
        return;
    CGSize drawable_size{
        .width = static_cast<CGFloat>(fbw),
        .height = static_cast<CGFloat>(fbh),
    };
    g_renderer.layer->setDrawableSize(drawable_size);
    g_renderer.drawable_width = fbw;
    g_renderer.drawable_height = fbh;
}

inline bool upload_text_cache() {
    auto& cache = g_renderer.text_cache;
    if (!cache.dirty)
        return true;
    if (!ensure_text_atlas_texture())
        return false;

    auto width = cache.dirty_max_x - cache.dirty_min_x;
    auto height = cache.dirty_max_y - cache.dirty_min_y;
    if (width <= 0 || height <= 0) {
        cache.dirty = false;
        cache.dirty_min_x = TextAtlasCache::atlas_size;
        cache.dirty_min_y = TextAtlasCache::atlas_size;
        cache.dirty_max_x = 0;
        cache.dirty_max_y = 0;
        return true;
    }

    MTL::Region region = MTL::Region::Make2D(
        cache.dirty_min_x,
        cache.dirty_min_y,
        width,
        height);
    auto const* src = cache.pixels.data()
        + static_cast<std::size_t>(
            cache.dirty_min_y * TextAtlasCache::atlas_size + cache.dirty_min_x);
    g_renderer.text_atlas_texture->replaceRegion(
        region,
        0,
        src,
        TextAtlasCache::atlas_size);
    metrics::inst::native_texture_upload_bytes.add(
        static_cast<std::uint64_t>(width * height),
        native_platform_attrs());

    cache.dirty = false;
    cache.dirty_min_x = TextAtlasCache::atlas_size;
    cache.dirty_min_y = TextAtlasCache::atlas_size;
    cache.dirty_max_x = 0;
    cache.dirty_max_y = 0;
    return true;
}

inline bool upload_image_cache() {
    auto& cache = g_images;
    if (!cache.dirty)
        return true;
    if (!ensure_image_atlas_texture())
        return false;

    auto width = cache.dirty_max_x - cache.dirty_min_x;
    auto height = cache.dirty_max_y - cache.dirty_min_y;
    if (width <= 0 || height <= 0) {
        cache.dirty = false;
        cache.dirty_min_x = ImageAtlasCache::atlas_size;
        cache.dirty_min_y = ImageAtlasCache::atlas_size;
        cache.dirty_max_x = 0;
        cache.dirty_max_y = 0;
        return true;
    }

    MTL::Region region = MTL::Region::Make2D(
        cache.dirty_min_x,
        cache.dirty_min_y,
        width,
        height);
    auto const* src = cache.pixels.data()
        + static_cast<std::size_t>(
            (cache.dirty_min_y * ImageAtlasCache::atlas_size + cache.dirty_min_x) * 4);
    g_renderer.image_atlas_texture->replaceRegion(
        region,
        0,
        src,
        ImageAtlasCache::atlas_size * 4);
    metrics::inst::native_texture_upload_bytes.add(
        static_cast<std::uint64_t>(width * height * 4),
        native_platform_attrs());

    cache.dirty = false;
    cache.dirty_min_x = ImageAtlasCache::atlas_size;
    cache.dirty_min_y = ImageAtlasCache::atlas_size;
    cache.dirty_max_x = 0;
    cache.dirty_max_y = 0;
    return true;
}

inline bool prepare_text_instances(float scale) {
    auto& scratch = g_renderer.scratch;
    for (auto& batch : scratch.batches)
        batch.texts.clear();
    scratch.overlay_text_instances.clear();
    if (scratch.text_runs.empty())
        return true;

    auto& cache = g_renderer.text_cache;
    int scale_key = quantize_metric(scale);
    if (cache.active_scale_key != scale_key) {
        reset_text_cache(cache);
        cache.active_scale_key = scale_key;
    } else if (cache.pixels.empty()) {
        reset_text_cache(cache);
        cache.active_scale_key = scale_key;
    }

    bool retried_after_reset = false;
    while (true) {
        bool restart = false;
        for (auto& batch : scratch.batches)
            batch.texts.clear();
        scratch.overlay_text_instances.clear();

        for (auto const& run : scratch.text_runs) {
            if (!run.text || run.len == 0)
                continue;

            int font_size_key = quantize_metric(run.font_size);
            int line_height_key = quantize_metric(run.line_height);
            int width_factor_key = quantize_metric(run.width_factor);
            auto* entry = find_text_cache_entry(
                cache, run, font_size_key, line_height_key, scale_key,
                width_factor_key);

            if (entry) {
                metrics::inst::native_text_cache_hits.add(1, native_platform_attrs());
            } else {
                RasterizedTextRun rasterized;
                if (!rasterize_text_run(
                        run.text, run.len, run.font_size, run.font_key,
                        run.line_height, scale, run.width_factor, rasterized)) {
                    metrics::inst::native_text_cache_misses.add(1, native_platform_attrs());
                    continue;
                }

                int slot_x = 0;
                int slot_y = 0;
                if (!reserve_text_slot(cache, rasterized.pixel_width,
                                       rasterized.pixel_height, slot_x, slot_y)) {
                    if (retried_after_reset) {
                        log::warn("phenotype.native.macos",
                                  "text cache atlas exhausted at scale {}", scale);
                        return true;
                    }
                    reset_text_cache(cache);
                    cache.active_scale_key = scale_key;
                    retried_after_reset = true;
                    restart = true;
                    break;
                }

                for (int row = 0; row < rasterized.pixel_height; ++row) {
                    auto* dst = cache.pixels.data()
                        + static_cast<std::size_t>(
                            (slot_y + row) * TextAtlasCache::atlas_size + slot_x);
                    auto const* src = rasterized.alpha.data()
                        + static_cast<std::size_t>(row * rasterized.pixel_width);
                    std::memcpy(
                        dst,
                        src,
                        static_cast<std::size_t>(rasterized.pixel_width));
                }
                mark_text_cache_dirty(
                    cache, slot_x, slot_y, rasterized.pixel_width, rasterized.pixel_height);

                TextCacheEntry new_entry{};
                new_entry.text.assign(run.text, run.len);
                new_entry.font_size_key = font_size_key;
                new_entry.line_height_key = line_height_key;
                new_entry.scale_key = scale_key;
                new_entry.width_factor_key = width_factor_key;
                new_entry.mono = run.mono;
                new_entry.x_offset = rasterized.x_offset;
                new_entry.width = rasterized.width;
                new_entry.height = rasterized.height;
                new_entry.u = static_cast<float>(slot_x) / TextAtlasCache::atlas_size;
                new_entry.v = static_cast<float>(slot_y) / TextAtlasCache::atlas_size;
                new_entry.uw = static_cast<float>(rasterized.pixel_width) / TextAtlasCache::atlas_size;
                new_entry.vh = static_cast<float>(rasterized.pixel_height) / TextAtlasCache::atlas_size;
                new_entry.has_ink = rasterized.has_ink;
                new_entry.font_key = run.font_key;
                cache.entries.push_back(std::move(new_entry));
                entry = &cache.entries.back();
                metrics::inst::native_text_cache_misses.add(1, native_platform_attrs());
            }

            if (!entry || !entry->has_ink)
                continue;

            // Overlay runs (IME composition / generic caret) carry the
            // sentinel batch_idx == UINT32_MAX and render Z-above scene
            // content with full-viewport scissor.
            auto& dst = (run.batch_idx == std::numeric_limits<std::uint32_t>::max())
                ? scratch.overlay_text_instances
                : scratch.batches[run.batch_idx].texts;
            // Pivot for rigid rotation is the run's anchor; each
            // glyph instance carries it so the shader can rotate
            // every quad by the same angle around the same point.
            // Skip pixel-snapping the pivot — snapping introduces
            // sub-pixel mismatch between glyph offsets that breaks
            // the rigid-body invariant under rotation.
            float const pivot_x = run.rotation == 0.0f
                ? snap_to_pixel_grid(run.x, scale)
                : run.x;
            float const pivot_y = run.rotation == 0.0f
                ? snap_to_pixel_grid(run.y, scale)
                : run.y;
            float const cos_t = run.rotation == 0.0f
                ? 1.0f : std::cos(run.rotation);
            float const sin_t = run.rotation == 0.0f
                ? 0.0f : std::sin(run.rotation);
            append_text_instance(
                dst,
                pivot_x + entry->x_offset,
                pivot_y,
                entry->width,
                entry->height,
                entry->u,
                entry->v,
                entry->uw,
                entry->vh,
                run.r,
                run.g,
                run.b,
                run.a,
                pivot_x,
                pivot_y,
                cos_t,
                sin_t);
        }

        if (!restart)
            return true;
    }
}

inline void prepare_image_instances(float scale) {
    auto& scratch = g_renderer.scratch;
    for (auto& batch : scratch.batches)
        batch.images.clear();

    for (auto const& image : scratch.images) {
        auto& batch = scratch.batches[image.batch_idx];
        auto const* entry = ensure_image_cache_entry(image.url);
        if (entry && entry->state == ImageEntryState::ready) {
            append_image_instance(
                batch.images,
                snap_to_pixel_grid(image.x, scale),
                snap_to_pixel_grid(image.y, scale),
                image.w,
                image.h,
                entry->u,
                entry->v,
                entry->uw,
                entry->vh);
            continue;
        }

        bool failed = entry && entry->state == ImageEntryState::failed;
        float fill = failed ? 0.90f : 0.94f;
        float edge = failed ? 0.78f : 0.82f;
        // Placeholder colors render in the SAME scissor batch as the
        // original DrawImage so they inherit canvas/dirty-root clipping.
        append_color_instance(
            batch.colors,
            image.x, image.y, image.w, image.h,
            fill, fill, fill, 1.0f,
            6.0f, 0.0f, 2.0f);
        append_color_instance(
            batch.colors,
            image.x, image.y, image.w, image.h,
            edge, edge, edge, 1.0f,
            0.0f, 1.0f, 1.0f);
    }
}

inline void request_window_repaint() {
    if (g_ime.request_repaint)
        g_ime.request_repaint();
}

struct ByteRange {
    std::size_t start = 0;
    std::size_t end = 0;
};

inline std::size_t utf16_offset_to_utf8(std::string const& text,
                                        unsigned long units) {
    return cppx::unicode::utf16_offset_to_utf8(text, units);
}

inline ByteRange utf16_range_to_utf8_range(std::string const& text,
                                           CocoaRange range) {
    auto bytes = cppx::unicode::utf16_range_to_utf8(
        text,
        range.location,
        range.length);
    return {bytes.start, bytes.end};
}

inline CocoaRange clamp_cocoa_range(CocoaRange range, unsigned long total_units) {
    if (range.location == cocoa_not_found)
        return {total_units, 0};
    if (range.location > total_units)
        range.location = total_units;
    auto max_len = total_units - range.location;
    if (range.length > max_len)
        range.length = max_len;
    return range;
}

inline std::string substring_for_utf16_range(std::string const& text,
                                             CocoaRange range) {
    auto bytes = utf16_range_to_utf8_range(text, range);
    return text.substr(bytes.start, bytes.end - bytes.start);
}

inline ByteRange caret_byte_range(::phenotype::FocusedInputSnapshot const& snapshot) {
    auto caret = clamp_snapshot_caret(snapshot);
    return {caret, caret};
}

inline ByteRange requested_document_range(
        ::phenotype::FocusedInputSnapshot const& snapshot,
        CocoaRange replacement_range) {
    if (replacement_range.location == cocoa_not_found)
        return caret_byte_range(snapshot);
    return utf16_range_to_utf8_range(snapshot.value, replacement_range);
}

inline ByteRange effective_replacement_range(
        ::phenotype::FocusedInputSnapshot const& snapshot,
        CocoaRange replacement_range) {
    if (replacement_range.location != cocoa_not_found)
        return utf16_range_to_utf8_range(snapshot.value, replacement_range);
    if (g_ime.composition_active)
        return {g_ime.replacement_start, g_ime.replacement_end};
    return {
        static_cast<std::size_t>(snapshot.selection_start),
        static_cast<std::size_t>(snapshot.selection_end),
    };
}

inline std::size_t composition_cursor_bytes() {
    auto total_units = utf16_length(g_ime.marked_text);
    auto selected = clamp_cocoa_range(g_ime.marked_selection, total_units);
    return utf16_offset_to_utf8(g_ime.marked_text, selected.location);
}

inline void sync_input_debug_composition_state() {
    ::phenotype::detail::set_input_composition_state(
        g_ime.composition_active && !g_ime.marked_text.empty(),
        g_ime.marked_text,
        static_cast<unsigned int>(composition_cursor_bytes()));
}

inline void clear_ime_state() {
    g_ime.composition_active = false;
    g_ime.marked_text.clear();
    g_ime.marked_selection = {0, 0};
    g_ime.composition_anchor = 0;
    g_ime.replacement_start = 0;
    g_ime.replacement_end = 0;
    ::phenotype::detail::clear_input_composition_state();
}

struct CompositionVisualState {
    bool valid = false;
    ::phenotype::FocusedInputSnapshot snapshot{};
    std::string erase_text;
    std::string visible_text;
    std::size_t caret_bytes = 0;
    std::size_t marked_start = 0;
    std::size_t marked_end = 0;
    float draw_y = 0.0f;
    float text_y = 0.0f;
    float base_x = 0.0f;
    float caret_x = 0.0f;
    float underline_x = 0.0f;
    float underline_width = 0.0f;
};

struct CaretVisualState {
    bool valid = false;
    ::phenotype::FocusedInputSnapshot snapshot{};
    ::phenotype::FocusedInputCaretLayout layout{};
    float text_y = 0.0f;
    float caret_x = 0.0f;
};

inline bool system_caret_supported() {
    return !g_force_disable_system_caret_for_tests
        && objc_lookUpClass("NSTextInsertionIndicator") != Nil;
}

inline bool macos_uses_shared_caret_blink() {
    return !system_caret_supported();
}

inline int macos_caret_blink_interval_ms() {
    return 530;
}

inline bool use_custom_caret_overlay() {
    return macos_uses_shared_caret_blink();
}

struct CaretOverlayColor {
    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
    float a = 1.0f;
};

inline CaretOverlayColor current_caret_overlay_color(
        ::phenotype::FocusedInputSnapshot const& snapshot) {
    CaretOverlayColor color{
        snapshot.accent.r / 255.0f,
        snapshot.accent.g / 255.0f,
        snapshot.accent.b / 255.0f,
        snapshot.accent.a / 255.0f,
    };

    auto color_class = static_cast<Class>(objc_getClass("NSColor"));
    auto color_space_class = static_cast<Class>(objc_getClass("NSColorSpace"));
    if (!color_class || !color_space_class)
        return color;

    auto insertion_point_color = objc_send<id>(
        class_as_id(color_class),
        sel_text_insertion_point_color());
    auto rgb_space = objc_send<id>(
        class_as_id(color_space_class),
        sel_generic_rgb_color_space());
    if (!insertion_point_color || !rgb_space)
        return color;

    auto rgb_color = objc_send<id>(
        insertion_point_color,
        sel_color_using_color_space(),
        rgb_space);
    if (!rgb_color || !objc_responds_to(rgb_color, sel_get_red_green_blue_alpha()))
        return color;

    double r = color.r;
    double g = color.g;
    double b = color.b;
    double a = color.a;
    objc_send<void>(
        rgb_color,
        sel_get_red_green_blue_alpha(),
        &r,
        &g,
        &b,
        &a);
    color.r = static_cast<float>(r);
    color.g = static_cast<float>(g);
    color.b = static_cast<float>(b);
    color.a = static_cast<float>(a);
    return color;
}

inline CGRect make_content_caret_rect(float x, float y, float height) {
    CGRect rect{};
    rect.origin.x = x;
    rect.origin.y = y;
    rect.size.width = 1.5;
    rect.size.height = height;
    return rect;
}

inline ::phenotype::diag::RectSnapshot rect_snapshot(CGRect rect) {
    if (CGRectIsNull(rect))
        return {};
    return {
        true,
        static_cast<float>(rect.origin.x),
        static_cast<float>(rect.origin.y),
        static_cast<float>(rect.size.width),
        static_cast<float>(rect.size.height),
    };
}

inline CGRect current_content_view_bounds() {
    if (!g_ime.content_view)
        return CGRectNull;
    return objc_send<CGRect>(g_ime.content_view, sel_bounds());
}

inline CGRect current_caret_host_bounds() {
    auto host_view = g_ime.editor_view ? g_ime.editor_view : g_ime.caret_host_view;
    if (!host_view)
        return CGRectNull;
    return objc_send<CGRect>(host_view, sel_bounds());
}

inline bool caret_host_view_is_flipped() {
    auto host_view = g_ime.editor_view ? g_ime.editor_view : g_ime.caret_host_view;
    return host_view && objc_send<bool>(host_view, sel_is_flipped());
}

inline CGRect draw_rect_to_host_view(CGRect draw_rect) {
    auto host_view = g_ime.editor_view ? g_ime.editor_view : g_ime.caret_host_view;
    if (CGRectIsNull(draw_rect) || !host_view)
        return CGRectNull;
    return draw_rect;
}

inline CGRect view_rect_to_screen(id view, CGRect view_rect) {
    if (CGRectIsNull(view_rect) || !view || !g_ime.ns_window)
        return CGRectNull;
    auto window_rect = objc_send<CGRect>(
        view,
        sel_convert_rect_to_view(),
        view_rect,
        static_cast<id>(nullptr));
    return objc_send<CGRect>(g_ime.ns_window, sel_convert_rect_to_screen(), window_rect);
}

inline CGRect host_view_rect_to_screen(CGRect host_rect) {
    if (CGRectIsNull(host_rect))
        return CGRectNull;
    auto host_view = g_ime.editor_view ? g_ime.editor_view : g_ime.caret_host_view;
    return view_rect_to_screen(host_view, host_rect);
}

inline CGRect draw_rect_to_screen(CGRect draw_rect) {
    if (CGRectIsNull(draw_rect))
        return CGRectNull;
    return host_view_rect_to_screen(draw_rect_to_host_view(draw_rect));
}

inline void ensure_editor_view();
inline void ensure_caret_host_view();

inline bool rect_changed(CGRect previous, CGRect current) {
    if (CGRectIsNull(previous) && CGRectIsNull(current))
        return false;
    if (CGRectIsNull(previous) || CGRectIsNull(current))
        return true;
    return !CGRectEqualToRect(previous, current);
}

inline CaretVisualState current_text_caret_visual_state(bool require_visible = false) {
    CaretVisualState visual{};
    visual.snapshot = ::phenotype::detail::focused_input_snapshot();
    if (!visual.snapshot.valid)
        return visual;
    if (require_visible && !visual.snapshot.caret_visible)
        return visual;

    visual.layout = ::phenotype::detail::compute_single_line_caret_layout(
        visual.snapshot,
        ::phenotype::detail::get_scroll_y(),
        false,
        [](auto const& input, std::size_t bytes) {
            return measure_utf8_line_offset(
                input.font_size,
                input.mono,
                input.value,
                bytes);
        });
    if (!visual.layout.valid)
        return visual;

    visual.text_y = visual.layout.draw_y;
    visual.caret_x = visual.layout.draw_x;
    visual.valid = true;
    return visual;
}

inline CompositionVisualState current_composition_visual_state() {
    CompositionVisualState visual{};
    visual.snapshot = ::phenotype::detail::focused_input_snapshot();
    if (!visual.snapshot.valid || !g_ime.composition_active || g_ime.marked_text.empty())
        return visual;

    auto start = ::phenotype::detail::clamp_utf8_boundary(
        visual.snapshot.value,
        g_ime.replacement_start);
    auto end = ::phenotype::detail::clamp_utf8_boundary(
        visual.snapshot.value,
        g_ime.replacement_end);
    if (end < start)
        end = start;

    auto prefix = visual.snapshot.value.substr(0, start);
    auto suffix = visual.snapshot.value.substr(end);
    visual.valid = true;
    visual.erase_text = visual.snapshot.value.empty()
        ? visual.snapshot.placeholder
        : visual.snapshot.value;
    visual.visible_text = prefix + g_ime.marked_text + suffix;
    visual.marked_start = prefix.size();
    visual.marked_end = visual.marked_start + g_ime.marked_text.size();
    visual.caret_bytes = std::min(
        visual.visible_text.size(),
        visual.marked_start + composition_cursor_bytes());

    auto scroll_y = ::phenotype::detail::get_scroll_y();
    visual.draw_y = visual.snapshot.y - scroll_y;
    visual.text_y = visual.draw_y + visual.snapshot.padding[0];
    visual.base_x = visual.snapshot.x + visual.snapshot.padding[3];
    visual.underline_x = visual.base_x + measure_utf8_line_offset(
        visual.snapshot.font_size,
        visual.snapshot.mono,
        visual.visible_text,
        visual.marked_start);
    auto underline_end = visual.base_x + measure_utf8_line_offset(
        visual.snapshot.font_size,
        visual.snapshot.mono,
        visual.visible_text,
        visual.marked_end);
    visual.underline_width = underline_end - visual.underline_x;
    visual.caret_x = visual.base_x + measure_utf8_line_offset(
        visual.snapshot.font_size,
        visual.snapshot.mono,
        visual.visible_text,
        visual.caret_bytes);
    return visual;
}

inline CGRect current_caret_rect_screen() {
    auto composition = current_composition_visual_state();
    auto caret = current_text_caret_visual_state();
    bool use_composition = composition.valid;
    if ((!use_composition && !caret.valid) || !g_ime.content_view || !g_ime.ns_window)
        return CGRectNull;

    CGRect draw_rect{};
    draw_rect.origin.x = use_composition ? composition.caret_x : caret.layout.draw_x;
    draw_rect.origin.y = use_composition ? composition.text_y : caret.layout.draw_y;
    draw_rect.size.width = 1.5;
    draw_rect.size.height = use_composition
        ? composition.snapshot.line_height
        : caret.layout.height;
    return draw_rect_to_screen(draw_rect);
}

inline CocoaRange current_marked_range() {
    auto snapshot = ::phenotype::detail::focused_input_snapshot();
    if (!snapshot.valid || !g_ime.composition_active || g_ime.marked_text.empty())
        return {cocoa_not_found, 0};

    auto start = ::phenotype::detail::clamp_utf8_boundary(snapshot.value, g_ime.replacement_start);
    return {
        utf16_length(std::string_view(snapshot.value.data(), start)),
        utf16_length(g_ime.marked_text),
    };
}

inline CocoaRange current_selected_range() {
    auto snapshot = ::phenotype::detail::focused_input_snapshot();
    if (!snapshot.valid)
        return {cocoa_not_found, 0};

    if (!g_ime.composition_active || g_ime.marked_text.empty()) {
        auto start = static_cast<std::size_t>(snapshot.selection_start);
        auto end = static_cast<std::size_t>(snapshot.selection_end);
        return {
            utf16_length(std::string_view(snapshot.value.data(), start)),
            utf16_length(std::string_view(snapshot.value.data() + start, end - start)),
        };
    }

    auto marked = current_marked_range();
    auto selected = clamp_cocoa_range(g_ime.marked_selection, utf16_length(g_ime.marked_text));
    return {
        marked.location + selected.location,
        selected.length,
    };
}

inline id autorelease_object(id value) {
    return value ? objc_send<id>(value, sel_autorelease()) : value;
}

inline id make_nsstring(std::string_view utf8) {
    auto* text = CFStringCreateWithBytes(
        kCFAllocatorDefault,
        reinterpret_cast<UInt8 const*>(utf8.empty() ? "" : utf8.data()),
        static_cast<CFIndex>(utf8.size()),
        kCFStringEncodingUTF8,
        false);
    return autorelease_object(cf_as_id(text));
}

inline id make_attributed_string(std::string_view utf8) {
    auto string = make_nsstring(utf8);
    if (!string)
        return nil;
    auto* attr = CFAttributedStringCreate(
        kCFAllocatorDefault,
        id_as_cf<CFStringRef>(string),
        nullptr);
    return autorelease_object(cf_as_id(attr));
}

inline void append_overlay_text(FrameScratch& scratch,
                                float x,
                                float y,
                                float font_size,
                                bool mono,
                                ::phenotype::Color const& color,
                                std::string text,
                                float line_height) {
    if (text.empty())
        return;
    scratch.overlay_text_storage.push_back(std::move(text));
    auto const& stored = scratch.overlay_text_storage.back();
    // Overlay (IME composition) runs are always axis-aligned and
    // unstretched — pass `rotation = 0.0f` and `width_factor = 1.0f`
    // after `font_size` to match the ParsedTextRun layout introduced
    // for canvas-side rotated / stretched text.
    scratch.text_runs.push_back({
        x,
        y,
        font_size,
        0.0f,                                            // rotation
        1.0f,                                            // width_factor
        mono,
        color.r / 255.0f,
        color.g / 255.0f,
        color.b / 255.0f,
        color.a / 255.0f,
        line_height,
        stored.c_str(),
        static_cast<unsigned int>(stored.size()),
        std::numeric_limits<std::uint32_t>::max(),  // overlay sentinel
    });
}

inline void append_ime_overlay(FrameScratch& scratch) {
    auto visual = current_composition_visual_state();
    if (!visual.valid)
        return;

    append_overlay_text(
        scratch,
        visual.base_x,
        visual.text_y,
        visual.snapshot.font_size,
        visual.snapshot.mono,
        visual.snapshot.background,
        visual.erase_text,
        visual.snapshot.line_height);

    append_overlay_text(
        scratch,
        visual.base_x,
        visual.text_y,
        visual.snapshot.font_size,
        visual.snapshot.mono,
        visual.snapshot.foreground,
        visual.visible_text,
        visual.snapshot.line_height);

    if (visual.underline_width > 0.0f) {
        append_color_instance(
            scratch.overlay_color_instances,
            visual.underline_x,
            visual.text_y + visual.snapshot.line_height - 2.0f,
            visual.underline_width,
            1.5f,
            visual.snapshot.accent.r / 255.0f,
            visual.snapshot.accent.g / 255.0f,
            visual.snapshot.accent.b / 255.0f,
            1.0f);
    }

    append_color_instance(
        scratch.overlay_color_instances,
        visual.caret_x,
        visual.text_y + 2.0f,
        1.5f,
        (visual.snapshot.line_height > 4.0f)
            ? (visual.snapshot.line_height - 4.0f)
            : visual.snapshot.line_height,
        visual.snapshot.accent.r / 255.0f,
        visual.snapshot.accent.g / 255.0f,
        visual.snapshot.accent.b / 255.0f,
        1.0f);
}

inline void append_generic_caret_overlay(FrameScratch& scratch) {
    if (!use_custom_caret_overlay())
        return;
    if (g_ime.composition_active && !g_ime.marked_text.empty())
        return;

    auto visual = current_text_caret_visual_state(/*require_visible=*/true);
    if (!visual.valid)
        return;
    auto color = current_caret_overlay_color(visual.snapshot);

    append_color_instance(
        scratch.overlay_color_instances,
        visual.layout.draw_x,
        visual.layout.draw_y,
        1.5f,
        visual.layout.height,
        color.r,
        color.g,
        color.b,
        color.a);
}

inline void ensure_system_insertion_indicator() {
    if (!system_caret_supported() || !g_ime.content_view)
        return;
    ensure_editor_view();
    auto host_view = g_ime.editor_view ? g_ime.editor_view : g_ime.caret_host_view;
    if (!host_view)
        return;
    if (g_ime.insertion_indicator)
        return;

    auto indicator_class = objc_lookUpClass("NSTextInsertionIndicator");
    if (!indicator_class)
        return;

    auto indicator = objc_send<id>(class_as_id(indicator_class), sel_alloc());
    g_ime.insertion_indicator = objc_send<id>(indicator, sel_init_with_frame(), CGRect{});
    if (!g_ime.insertion_indicator)
        return;

    objc_send<void>(host_view, sel_add_subview(), g_ime.insertion_indicator);
    objc_send<void>(
        g_ime.insertion_indicator,
        sel_set_automatic_mode_options(),
        insertion_indicator_automatic_mode_show_effects
            | insertion_indicator_automatic_mode_show_while_tracking);
    objc_send<void>(
        g_ime.insertion_indicator,
        sel_set_display_mode(),
        insertion_indicator_display_mode_hidden);
    g_ime.last_indicator_frame = CGRectNull;
    g_ime.last_indicator_display_mode = insertion_indicator_display_mode_hidden;
}

inline void set_system_insertion_indicator_mode(long long mode) {
    if (!g_ime.insertion_indicator)
        return;
    if (g_ime.last_indicator_display_mode == mode)
        return;
    objc_send<void>(g_ime.insertion_indicator, sel_set_display_mode(), mode);
    g_ime.last_indicator_display_mode = mode;
}

inline void set_system_insertion_indicator_frame(CGRect host_rect) {
    if (!g_ime.insertion_indicator)
        return;
    if (!rect_changed(g_ime.last_indicator_frame, host_rect))
        return;
    objc_send<void>(g_ime.insertion_indicator, sel_set_frame(), host_rect);
    g_ime.last_indicator_frame = host_rect;
}

inline bool sync_caret_presentation(::phenotype::FocusedInputSnapshot const& snapshot) {
    auto previous_draw_rect = g_ime.last_character_rect_draw;
    auto previous_host_rect = g_ime.last_character_rect_host;
    auto previous_screen_rect = g_ime.last_character_rect_screen;
    auto previous_renderer = ::phenotype::diag::input_debug_snapshot().caret_renderer;
    bool previous_valid = ::phenotype::diag::input_debug_snapshot().caret_rect.valid;
    bool presentation_changed = false;

    if (!snapshot.valid) {
        if (g_ime.content_view)
            ensure_editor_view();
        if (system_caret_supported())
            ensure_system_insertion_indicator();
        set_system_insertion_indicator_mode(insertion_indicator_display_mode_hidden);
        g_ime.last_character_rect_draw = CGRectNull;
        g_ime.last_character_rect_host = CGRectNull;
        g_ime.last_character_rect_screen = CGRectNull;
        ::phenotype::detail::clear_input_debug_caret_presentation();
        if (rect_changed(previous_draw_rect, CGRectNull)
            || rect_changed(previous_host_rect, CGRectNull)) {
            presentation_changed = true;
        }
        return rect_changed(previous_screen_rect, CGRectNull)
            || previous_renderer != "hidden"
            || previous_valid
            || presentation_changed;
    }

    ensure_editor_view();
    auto composition_active = g_ime.composition_active && !g_ime.marked_text.empty();
    auto caret = current_text_caret_visual_state(/*require_visible=*/false);
    CGRect caret_draw_rect = CGRectNull;
    CGRect caret_host_rect = CGRectNull;
    CGRect caret_screen_rect = CGRectNull;
    auto host_bounds = current_caret_host_bounds();
    if (caret.valid) {
        caret_draw_rect = make_content_caret_rect(
            caret.layout.draw_x,
            caret.layout.draw_y,
            caret.layout.height);
        caret_host_rect = draw_rect_to_host_view(caret_draw_rect);
        caret_screen_rect = draw_rect_to_screen(caret_draw_rect);
    }

    auto draw_snapshot = rect_snapshot(caret_draw_rect);
    auto host_snapshot = rect_snapshot(caret_host_rect);
    auto screen_snapshot = rect_snapshot(caret_screen_rect);
    auto host_bounds_snapshot = rect_snapshot(host_bounds);
    auto host_flipped = caret_host_view_is_flipped();

    if (!use_custom_caret_overlay()) {
        ensure_system_insertion_indicator();
        if (caret.valid && !composition_active && !g_ime.scroll_tracking_active) {
            set_system_insertion_indicator_frame(caret_host_rect);
            set_system_insertion_indicator_mode(insertion_indicator_display_mode_automatic);
            ::phenotype::detail::set_input_debug_caret_geometry(
                "system",
                draw_snapshot,
                host_snapshot,
                screen_snapshot,
                host_bounds_snapshot,
                host_flipped,
                "draw");
        } else {
            set_system_insertion_indicator_mode(insertion_indicator_display_mode_hidden);
            ::phenotype::detail::clear_input_debug_caret_presentation();
        }
    } else {
        set_system_insertion_indicator_mode(insertion_indicator_display_mode_hidden);
        if (caret.valid
            && (snapshot.caret_visible || g_ime.scroll_tracking_active)
            && !composition_active) {
            ::phenotype::detail::set_input_debug_caret_geometry(
                "custom",
                draw_snapshot,
                host_snapshot,
                screen_snapshot,
                host_bounds_snapshot,
                host_flipped,
                "draw");
        } else {
            ::phenotype::detail::clear_input_debug_caret_presentation();
        }
    }

    auto current_renderer = ::phenotype::diag::input_debug_snapshot().caret_renderer;
    bool current_valid = ::phenotype::diag::input_debug_snapshot().caret_rect.valid;
    if (rect_changed(previous_draw_rect, caret_draw_rect)
        || rect_changed(previous_host_rect, caret_host_rect)) {
        presentation_changed = true;
    }
    if (rect_changed(previous_screen_rect, caret_screen_rect))
        presentation_changed = true;
    if (current_renderer != previous_renderer || current_valid != previous_valid)
        presentation_changed = true;
    g_ime.last_character_rect_draw = caret_draw_rect;
    g_ime.last_character_rect_host = caret_host_rect;
    g_ime.last_character_rect_screen = caret_screen_rect;
    return presentation_changed;
}

inline void discard_marked_text_from_system() {
    if (!g_ime.editor_view)
        return;
    auto input_context = objc_send<id>(g_ime.editor_view, sel_input_context());
    if (input_context && objc_responds_to(input_context, sel_discard_marked_text()))
        objc_send<void>(input_context, sel_discard_marked_text());
}

inline void sync_input_context_geometry() {
    if (!g_ime.editor_view)
        return;
    auto input_context = objc_send<id>(g_ime.editor_view, sel_input_context());
    if (input_context
        && objc_responds_to(input_context, sel_invalidate_character_coordinates())) {
        objc_send<void>(input_context, sel_invalidate_character_coordinates());
    }
}

inline bool editor_is_first_responder() {
    return g_ime.ns_window
        && objc_send<id>(g_ime.ns_window, sel_first_responder()) == g_ime.editor_view;
}

inline void restore_content_view_first_responder() {
    if (g_ime.ns_window && g_ime.content_view)
        objc_send<bool>(g_ime.ns_window, sel_make_first_responder(), g_ime.content_view);
}

inline void focus_editor_view() {
    if (g_ime.ns_window && g_ime.editor_view)
        objc_send<bool>(g_ime.ns_window, sel_make_first_responder(), g_ime.editor_view);
}

inline bool input_dismiss_transient();
inline void editor_do_command_by_selector(id, SEL, SEL command);

inline bool is_line_break_text(std::string_view text) {
    return text == "\n" || text == "\r" || text == "\r\n";
}

inline bool editor_handle_key_equivalent(
        unsigned long long modifier_flags,
        unsigned short key_code,
        std::string_view characters_ignoring_modifiers) {
    constexpr unsigned long long command_modifier = 1ull << 20;
    if ((modifier_flags & command_modifier) == 0)
        return false;
    bool is_select_all = key_code == 0;
    if (!is_select_all && characters_ignoring_modifiers.size() == 1) {
        auto ch = characters_ignoring_modifiers.front();
        is_select_all = ch == 'a' || ch == 'A';
    }
    if (is_select_all) {
        editor_do_command_by_selector(
            static_cast<id>(nullptr),
            nullptr,
            sel_select_all());
        return true;
    }
    return false;
}

inline void editor_key_down(id self, SEL, id event) {
    auto events = objc_send<id>(
        class_as_id(objc_getClass("NSArray")),
        sel_array_with_object(),
        event);
    objc_send<void>(self, sel_interpret_key_events(), events);
}

inline bool editor_perform_key_equivalent(id, SEL, id event) {
    if (!event)
        return false;
    auto modifier_flags = objc_send<unsigned long long>(event, sel_modifier_flags());
    auto key_code = objc_send<unsigned short>(event, sel_key_code());
    auto characters = objc_send<id>(event, sel_characters_ignoring_modifiers());
    return editor_handle_key_equivalent(
        modifier_flags,
        key_code,
        text_object_to_utf8(characters));
}

inline bool editor_accepts_first_responder(id, SEL) {
    return true;
}

inline bool editor_is_flipped(id, SEL) {
    return true;
}

inline id editor_hit_test(id, SEL, CGPoint) {
    return static_cast<id>(nullptr);
}

inline long long editor_conversation_identifier(id self, SEL) {
    return reinterpret_cast<long long>(self);
}

inline bool editor_has_marked_text(id, SEL) {
    return g_ime.composition_active && !g_ime.marked_text.empty();
}

inline CocoaRange editor_marked_range(id, SEL) {
    return current_marked_range();
}

inline CocoaRange editor_selected_range(id, SEL) {
    return current_selected_range();
}

inline void editor_set_marked_text(id, SEL, id value,
                                   CocoaRange selected_range,
                                   CocoaRange replacement_range) {
    auto snapshot = ::phenotype::detail::focused_input_snapshot();
    if (!snapshot.valid)
        return;

    auto bytes = effective_replacement_range(snapshot, replacement_range);
    bytes.start = ::phenotype::detail::clamp_utf8_boundary(snapshot.value, bytes.start);
    bytes.end = ::phenotype::detail::clamp_utf8_boundary(snapshot.value, bytes.end);
    if (bytes.end < bytes.start)
        bytes.end = bytes.start;

    g_ime.marked_text = text_object_to_utf8(value);
    g_ime.composition_active = !g_ime.marked_text.empty();
    g_ime.composition_anchor = bytes.start;
    g_ime.replacement_start = bytes.start;
    g_ime.replacement_end = bytes.end;
    g_ime.marked_selection = clamp_cocoa_range(
        selected_range,
        utf16_length(g_ime.marked_text));
    ::phenotype::detail::set_focused_input_selection(bytes.start, bytes.start);
    sync_input_debug_composition_state();
    sync_input_context_geometry();
    request_window_repaint();
}

inline void editor_unmark_text(id, SEL) {
    auto had_marked = g_ime.composition_active || !g_ime.marked_text.empty();
    clear_ime_state();
    if (had_marked) {
        sync_input_context_geometry();
        request_window_repaint();
    }
}

inline void editor_insert_text(id, SEL, id value, CocoaRange replacement_range) {
    auto snapshot = ::phenotype::detail::focused_input_snapshot();
    if (!snapshot.valid)
        return;

    auto replacement = effective_replacement_range(snapshot, replacement_range);
    replacement.start = ::phenotype::detail::clamp_utf8_boundary(snapshot.value, replacement.start);
    replacement.end = ::phenotype::detail::clamp_utf8_boundary(snapshot.value, replacement.end);
    if (replacement.end < replacement.start)
        replacement.end = replacement.start;

    auto text = text_object_to_utf8(value);
    if (!is_line_break_text(text)) {
        ::phenotype::detail::replace_focused_input_text(
            replacement.start,
            replacement.end,
            text);
    }

    clear_ime_state();
    sync_input_context_geometry();
    request_window_repaint();
}

inline CGRect editor_first_rect_for_character_range(id,
                                                    SEL,
                                                    CocoaRange range,
                                                    CocoaRange* actual_range) {
    auto visual = current_composition_visual_state();
    auto snapshot = visual.valid
        ? visual.snapshot
        : ::phenotype::detail::focused_input_snapshot();
    if (!snapshot.valid)
        return CGRectNull;

    auto const& document = visual.valid ? visual.visible_text : snapshot.value;
    auto clamped = clamp_cocoa_range(range, utf16_length(document));
    if (actual_range)
        *actual_range = clamped;

    auto bytes = utf16_range_to_utf8_range(document, clamped);
    auto scroll_y = ::phenotype::detail::get_scroll_y();
    float base_x = snapshot.x + snapshot.padding[3];
    float draw_y = snapshot.y - scroll_y + snapshot.padding[0];
    float start_x = base_x + measure_utf8_line_offset(
        snapshot.font_size,
        snapshot.mono,
        document,
        bytes.start);
    float end_x = base_x + measure_utf8_line_offset(
        snapshot.font_size,
        snapshot.mono,
        document,
        bytes.end);

    CGRect draw_rect{};
    draw_rect.origin.x = start_x;
    draw_rect.origin.y = draw_y;
    draw_rect.size.width = std::max(1.5f, end_x - start_x);
    draw_rect.size.height = snapshot.line_height;

    auto rect = draw_rect_to_screen(draw_rect);
    if (!CGRectIsNull(rect))
        return rect;
    if (!g_ime.content_view || !g_ime.ns_window)
        return CGRectNull;

    auto window_rect = objc_send<CGRect>(
        g_ime.content_view,
        sel_convert_rect_to_view(),
        draw_rect,
        static_cast<id>(nullptr));
    return objc_send<CGRect>(g_ime.ns_window, sel_convert_rect_to_screen(), window_rect);
}

inline unsigned long editor_character_index_for_point(id, SEL, CGPoint) {
    auto selected = current_selected_range();
    return (selected.location == cocoa_not_found) ? 0 : selected.location;
}

inline id editor_attributed_substring_for_range(id,
                                                SEL,
                                                CocoaRange range,
                                                CocoaRange* actual_range) {
    auto visual = current_composition_visual_state();
    auto snapshot = visual.valid
        ? visual.snapshot
        : ::phenotype::detail::focused_input_snapshot();
    if (!snapshot.valid)
        return nil;

    auto const& document = visual.valid ? visual.visible_text : snapshot.value;
    auto total_units = utf16_length(document);
    auto clamped = clamp_cocoa_range(range, total_units);
    if (actual_range)
        *actual_range = clamped;
    return make_attributed_string(substring_for_utf16_range(document, clamped));
}

inline id editor_valid_attributes_for_marked_text(id, SEL) {
    return objc_send<id>(class_as_id(objc_getClass("NSArray")), sel_array());
}

inline void editor_do_command_by_selector(id,
                                          SEL,
                                          SEL command) {
    bool handled = false;
    if (command == sel_delete_backward()) {
        handled = ::phenotype::detail::handle_key(
            1, 0, false, "macos-ime", "deleteBackward:");
    } else if (command == sel_move_left()) {
        handled = ::phenotype::detail::handle_key(
            2, 0, false, "macos-ime", "moveLeft:");
    } else if (command == sel_move_left_and_modify_selection()) {
        handled = ::phenotype::detail::handle_key(
            2, 0, true, "macos-ime", "moveLeftAndModifySelection:");
    } else if (command == sel_move_right()) {
        handled = ::phenotype::detail::handle_key(
            3, 0, false, "macos-ime", "moveRight:");
    } else if (command == sel_move_right_and_modify_selection()) {
        handled = ::phenotype::detail::handle_key(
            3, 0, true, "macos-ime", "moveRightAndModifySelection:");
    } else if (command == sel_move_to_beginning()) {
        handled = ::phenotype::detail::handle_key(
            4, 0, false, "macos-ime", "moveToBeginningOfLine:");
    } else if (command == sel_move_to_beginning_and_modify_selection()) {
        handled = ::phenotype::detail::handle_key(
            4, 0, true, "macos-ime", "moveToBeginningOfLineAndModifySelection:");
    } else if (command == sel_move_to_end()) {
        handled = ::phenotype::detail::handle_key(
            5, 0, false, "macos-ime", "moveToEndOfLine:");
    } else if (command == sel_move_to_end_and_modify_selection()) {
        handled = ::phenotype::detail::handle_key(
            5, 0, true, "macos-ime", "moveToEndOfLineAndModifySelection:");
    } else if (command == sel_select_all()) {
        handled = ::phenotype::detail::select_all_focused_input();
        if (handled) {
            ::phenotype::detail::note_input_event(
                "key",
                "macos-ime",
                "selectAll:",
                "handled",
                ::phenotype::detail::get_focused_id());
        }
    } else if (command == sel_insert_tab()) {
        handled = ::phenotype::detail::handle_tab(0, "macos-ime", "insertTab:");
    } else if (command == sel_insert_backtab()) {
        handled = ::phenotype::detail::handle_tab(1, "macos-ime", "insertBacktab:");
    } else if (command == sel_cancel_operation()) {
        bool dismissed = input_dismiss_transient();
        bool cleared = false;
        if (::phenotype::detail::get_focused_id() != ::phenotype::native::invalid_callback_id)
            cleared = ::phenotype::detail::set_focus_id(
                ::phenotype::native::invalid_callback_id,
                "macos-ime",
                "cancelOperation:");
        if (cleared)
            restore_content_view_first_responder();
        handled = dismissed || cleared;
    } else if (command == sel_insert_newline()) {
        handled = g_ime.composition_active;
    }

    if (handled)
        request_window_repaint();
}

inline bool caret_host_view_is_flipped_method(id, SEL) {
    return true;
}

inline id caret_host_view_hit_test_method(id, SEL, CGPoint) {
    return static_cast<id>(nullptr);
}

inline Class ensure_caret_host_view_class() {
    if (g_ime.caret_host_class)
        return g_ime.caret_host_class;

    static char const* class_name = "PhenotypeCaretHostView";
    if (auto existing = objc_lookUpClass(class_name)) {
        g_ime.caret_host_class = existing;
        return existing;
    }

    auto base_class = static_cast<Class>(objc_getClass("NSView"));
    auto subclass = objc_allocateClassPair(base_class, class_name, 0);
    class_addMethod(
        subclass,
        sel_is_flipped(),
        reinterpret_cast<IMP>(caret_host_view_is_flipped_method),
        "B@:");
    class_addMethod(
        subclass,
        sel_hit_test(),
        reinterpret_cast<IMP>(caret_host_view_hit_test_method),
        "@@:{CGPoint=dd}");
    objc_registerClassPair(subclass);
    g_ime.caret_host_class = subclass;
    return subclass;
}

inline Class ensure_editor_view_class() {
    if (g_ime.editor_class)
        return g_ime.editor_class;

    static char const* class_name = "PhenotypeIMEFieldEditorView";
    if (auto existing = objc_lookUpClass(class_name)) {
        g_ime.editor_class = existing;
        return existing;
    }

    auto base_class = static_cast<Class>(objc_getClass("NSView"));
    auto subclass = objc_allocateClassPair(base_class, class_name, 0);
    if (auto protocol = objc_getProtocol("NSTextInputClient"))
        class_addProtocol(subclass, protocol);
    class_addMethod(
        subclass,
        sel_accepts_first_responder(),
        reinterpret_cast<IMP>(editor_accepts_first_responder),
        "B@:");
    class_addMethod(
        subclass,
        sel_is_flipped(),
        reinterpret_cast<IMP>(editor_is_flipped),
        "B@:");
    class_addMethod(
        subclass,
        sel_hit_test(),
        reinterpret_cast<IMP>(editor_hit_test),
        "@@:{CGPoint=dd}");
    class_addMethod(
        subclass,
        sel_conversation_identifier(),
        reinterpret_cast<IMP>(editor_conversation_identifier),
        "q@:");
    class_addMethod(
        subclass,
        sel_key_down(),
        reinterpret_cast<IMP>(editor_key_down),
        "v@:@");
    class_addMethod(
        subclass,
        sel_perform_key_equivalent(),
        reinterpret_cast<IMP>(editor_perform_key_equivalent),
        "B@:@");
    class_addMethod(
        subclass,
        sel_has_marked_text(),
        reinterpret_cast<IMP>(editor_has_marked_text),
        "B@:");
    class_addMethod(
        subclass,
        sel_marked_range(),
        reinterpret_cast<IMP>(editor_marked_range),
        "{_NSRange=QQ}@:");
    class_addMethod(
        subclass,
        sel_selected_range(),
        reinterpret_cast<IMP>(editor_selected_range),
        "{_NSRange=QQ}@:");
    class_addMethod(
        subclass,
        sel_set_marked_text(),
        reinterpret_cast<IMP>(editor_set_marked_text),
        "v@:@@{_NSRange=QQ}{_NSRange=QQ}");
    class_addMethod(
        subclass,
        sel_unmark_text(),
        reinterpret_cast<IMP>(editor_unmark_text),
        "v@:");
    class_addMethod(
        subclass,
        sel_insert_text_with_range(),
        reinterpret_cast<IMP>(editor_insert_text),
        "v@:@@{_NSRange=QQ}");
    class_addMethod(
        subclass,
        sel_first_rect_for_character_range(),
        reinterpret_cast<IMP>(editor_first_rect_for_character_range),
        "{CGRect={CGPoint=dd}{CGSize=dd}}@:{_NSRange=QQ}^{_NSRange=QQ}");
    class_addMethod(
        subclass,
        sel_character_index_for_point(),
        reinterpret_cast<IMP>(editor_character_index_for_point),
        "Q@:{CGPoint=dd}");
    class_addMethod(
        subclass,
        sel_attributed_substring_for_range(),
        reinterpret_cast<IMP>(editor_attributed_substring_for_range),
        "@@:{_NSRange=QQ}^{_NSRange=QQ}");
    class_addMethod(
        subclass,
        sel_valid_attributes_for_marked_text(),
        reinterpret_cast<IMP>(editor_valid_attributes_for_marked_text),
        "@@:");
    class_addMethod(
        subclass,
        sel_do_command_by_selector(),
        reinterpret_cast<IMP>(editor_do_command_by_selector),
        "v@::");
    objc_registerClassPair(subclass);
    g_ime.editor_class = subclass;
    return subclass;
}

inline void sync_caret_host_view_frame() {
    if (!g_ime.caret_host_view)
        return;
    auto bounds = current_content_view_bounds();
    if (CGRectIsNull(bounds))
        return;
    if (rect_changed(g_ime.last_host_frame, bounds)) {
        objc_send<void>(g_ime.caret_host_view, sel_set_frame(), bounds);
        g_ime.last_host_frame = bounds;
    }
}

inline void ensure_caret_host_view() {
    if (!g_ime.content_view)
        return;
    if (!g_ime.caret_host_view) {
        auto host_class = ensure_caret_host_view_class();
        auto bounds = current_content_view_bounds();
        auto view = objc_send<id>(class_as_id(host_class), sel_alloc());
        g_ime.caret_host_view = objc_send<id>(view, sel_init_with_frame(), bounds);
        objc_send<void>(g_ime.content_view, sel_add_subview(), g_ime.caret_host_view);
        g_ime.last_host_frame = CGRectNull;
    }
    sync_caret_host_view_frame();
}

inline void ensure_editor_view() {
    if (!g_ime.content_view)
        return;
    auto bounds = current_content_view_bounds();
    if (!g_ime.editor_view) {
        auto editor_class = ensure_editor_view_class();
        auto view = objc_send<id>(class_as_id(editor_class), sel_alloc());
        g_ime.editor_view = objc_send<id>(view, sel_init_with_frame(), bounds);
        objc_send<void>(g_ime.content_view, sel_add_subview(), g_ime.editor_view);
    }
    if (!CGRectIsNull(bounds))
        objc_send<void>(g_ime.editor_view, sel_set_frame(), bounds);
}

inline float macos_normalize_scroll_delta(double scrolling_delta_y,
                                          bool has_precise_scrolling_deltas,
                                          float line_height) {
    if (!std::isfinite(scrolling_delta_y) || scrolling_delta_y == 0.0)
        return 0.0f;
    if (has_precise_scrolling_deltas)
        return static_cast<float>(scrolling_delta_y);
    if (line_height <= 0.0f)
        line_height = 1.0f;
    return static_cast<float>(scrolling_delta_y) * line_height;
}

inline float macos_scroll_delta_multiplier(bool horizontal) {
    auto const& theme = ::phenotype::current_theme();
    float multiplier = horizontal
        ? theme.scroll_horizontal_delta_multiplier
        : theme.scroll_delta_multiplier;
    if (!(multiplier > 0.0f) || !std::isfinite(multiplier))
        return 1.0f;
    return multiplier;
}

inline bool scroll_phase_active(unsigned long long phase) {
    return phase != ns_event_phase_none;
}

inline bool scroll_phase_in_progress(unsigned long long phase) {
    if (phase == ns_event_phase_none)
        return false;
    auto terminal_mask = ns_event_phase_ended | ns_event_phase_cancelled;
    return (phase & terminal_mask) == 0;
}

inline bool sync_scroll_tracking_state(unsigned long long phase,
                                       unsigned long long momentum_phase) {
    bool next_active = scroll_phase_in_progress(phase)
        || scroll_phase_in_progress(momentum_phase);
    if (g_ime.scroll_tracking_active == next_active)
        return false;
    g_ime.scroll_tracking_active = next_active;
    return true;
}

inline void record_macos_scroll_runtime_event(
        double raw_delta_x,
        double raw_delta_y,
        bool has_precise_scrolling_deltas,
        float line_height,
        float app_vertical_multiplier,
        float app_horizontal_multiplier,
        float normalized_delta_x,
        float normalized_delta_y,
        float viewport_width,
        float viewport_height,
        unsigned long long phase,
        unsigned long long momentum_phase,
        bool scroll_tracking_changed,
        bool handled_x,
        bool handled_y) {
    g_ime.last_scroll_event = MacOSScrollRuntimeEvent{
        .available = true,
        .source = has_precise_scrolling_deltas
            ? "NSEvent.scrollingDelta precise"
            : "NSEvent.scrollingDelta line",
        .has_precise_scrolling_deltas = has_precise_scrolling_deltas,
        .raw_delta_x = raw_delta_x,
        .raw_delta_y = raw_delta_y,
        .line_height = line_height,
        .app_vertical_multiplier = app_vertical_multiplier,
        .app_horizontal_multiplier = app_horizontal_multiplier,
        .normalized_delta_x = normalized_delta_x,
        .normalized_delta_y = normalized_delta_y,
        .viewport_width = viewport_width,
        .viewport_height = viewport_height,
        .phase = phase,
        .momentum_phase = momentum_phase,
        .scroll_tracking_changed = scroll_tracking_changed,
        .handled_x = handled_x,
        .handled_y = handled_y,
    };
}

inline float current_scroll_viewport_height() {
    return viewport_height();
}

inline float current_scroll_viewport_width() {
    return viewport_width();
}

inline bool event_cursor_position(id event, float& out_x, float& out_y) {
    if (!event || !g_ime.content_view)
        return false;
    auto point = objc_send<CGPoint>(event, sel_location_in_window());
    auto converted = objc_send<CGPoint>(
        g_ime.content_view,
        sel_convert_point_from_view(),
        point,
        static_cast<id>(nullptr));
    auto bounds = objc_send<CGRect>(g_ime.content_view, sel_bounds());
    out_x = static_cast<float>(converted.x);
    out_y = static_cast<float>(bounds.size.height - converted.y);
    return true;
}

inline bool handle_local_scroll_event(id event) {
    if (!event || !g_ime.ns_window)
        return false;

    auto event_window = objc_send<id>(event, sel_window());
    if (event_window != g_ime.ns_window)
        return false;

    bool has_precise_scrolling_deltas =
        objc_send<bool>(event, sel_has_precise_scrolling_deltas());
    double scrolling_delta_y = objc_send<double>(event, sel_scrolling_delta_y());
    double scrolling_delta_x = objc_send<double>(event, sel_scrolling_delta_x());

    // Stage-8 canvas-gesture path: when an on-screen `widget::canvas`
    // has registered an `on_gesture` handler and the cursor sits
    // inside its bounds, a precise (trackpad) two-finger drag becomes
    // a Pan gesture, and Cmd+scroll becomes a ScrollZoom. Falls
    // through to the layout-tree scroll path below otherwise.
    if (has_precise_scrolling_deltas
        && (scrolling_delta_x != 0.0 || scrolling_delta_y != 0.0)
        && ::phenotype::detail::g_app.gesture_target_id != 0xFFFFFFFFu) {
        float cursor_x = 0.0f, cursor_y = 0.0f;
        if (event_cursor_position(event, cursor_x, cursor_y)) {
            auto modifier_flags =
                objc_send<unsigned long long>(event, sel_modifier_flags());
            bool cmd_held = (modifier_flags & ns_event_modifier_command) != 0;
            ::phenotype::GestureEvent gev{};
            gev.focus_x = cursor_x;
            gev.focus_y = cursor_y;
            if (cmd_held) {
                gev.kind = ::phenotype::GestureKind::ScrollZoom;
                gev.dy   = static_cast<float>(scrolling_delta_y);
                gev.pinch_scale = std::exp(
                    static_cast<float>(scrolling_delta_y) * 0.005f);
            } else {
                gev.kind = ::phenotype::GestureKind::Pan;
                gev.dx   = static_cast<float>(scrolling_delta_x);
                gev.dy   = static_cast<float>(scrolling_delta_y);
            }
            if (::phenotype::native::detail::dispatch_gesture(gev))
                return true;
        }
    }
    auto phase = objc_send<unsigned long long>(event, sel_phase());
    auto momentum_phase = objc_send<unsigned long long>(event, sel_momentum_phase());
    bool scroll_tracking_changed = sync_scroll_tracking_state(phase, momentum_phase);
    float line_height = scroll_line_height();
    float normalized_delta = macos_normalize_scroll_delta(
        scrolling_delta_y,
        has_precise_scrolling_deltas,
        line_height);
    float normalized_delta_x = macos_normalize_scroll_delta(
        scrolling_delta_x,
        has_precise_scrolling_deltas,
        line_height);
    float const vertical_multiplier = macos_scroll_delta_multiplier(false);
    float const horizontal_multiplier = macos_scroll_delta_multiplier(true);
    normalized_delta *= vertical_multiplier;
    normalized_delta_x *= horizontal_multiplier;
    float viewport_height_value = current_scroll_viewport_height();
    float viewport_width_value = current_scroll_viewport_width();
    bool handled = false;
    bool handled_y = false;
    bool handled_x = false;
    if (normalized_delta != 0.0f) {
        handled_y = dispatch_scroll_pixels(
            normalized_delta,
            viewport_height_value,
            has_precise_scrolling_deltas ? "wheel-precise" : "wheel-line");
        handled = handled_y;
    }
    if (normalized_delta_x != 0.0f) {
        handled_x = dispatch_scroll_pixels_x(
            normalized_delta_x,
            viewport_width_value,
            has_precise_scrolling_deltas ? "wheel-precise-x" : "wheel-line-x");
        handled = handled || handled_x;
    }
    record_macos_scroll_runtime_event(
        scrolling_delta_x,
        scrolling_delta_y,
        has_precise_scrolling_deltas,
        line_height,
        vertical_multiplier,
        horizontal_multiplier,
        normalized_delta_x,
        normalized_delta,
        viewport_width_value,
        viewport_height_value,
        phase,
        momentum_phase,
        scroll_tracking_changed,
        handled_x,
        handled_y);
    if (scroll_tracking_changed
        && normalized_delta == 0.0f
        && normalized_delta_x == 0.0f
        && g_ime.request_repaint)
        g_ime.request_repaint();
    return handled
        || normalized_delta != 0.0f
        || normalized_delta_x != 0.0f
        || scroll_tracking_changed
        || scroll_phase_active(phase)
        || scroll_phase_active(momentum_phase);
}

// Translate macOS trackpad pinch into a `Pinch` gesture on whichever
// canvas registered an `on_gesture` handler this frame. NSEvent's
// `magnification` is the additive delta (∼0.0 + per-event change), so
// `1.0 + magnification` becomes the multiplicative scale factor we
// expose in `GestureEvent::pinch_scale`.
inline bool handle_local_magnify_event(id event) {
    if (!event || !g_ime.ns_window)
        return false;

    auto event_window = objc_send<id>(event, sel_window());
    if (event_window != g_ime.ns_window)
        return false;

    double magnification = objc_send<double>(event, sel_magnification());
    if (magnification == 0.0)
        return false;

    float cursor_x = 0.0f, cursor_y = 0.0f;
    if (!event_cursor_position(event, cursor_x, cursor_y))
        return false;

    ::phenotype::GestureEvent ev{};
    ev.kind        = ::phenotype::GestureKind::Pinch;
    ev.pinch_scale = 1.0f + static_cast<float>(magnification);
    ev.focus_x     = cursor_x;
    ev.focus_y     = cursor_y;
    return ::phenotype::native::detail::dispatch_gesture(ev);
}

inline void install_local_magnify_monitor() {
    if (g_ime.magnify_monitor)
        return;

    auto event_class = static_cast<Class>(objc_getClass("NSEvent"));
    if (!event_class)
        return;

    g_ime.magnify_monitor = objc_send<id>(
        class_as_id(event_class),
        sel_add_local_monitor_for_events_matching_mask_handler(),
        ns_event_mask_magnify,
        ^id(id event) {
            return handle_local_magnify_event(event)
                ? static_cast<id>(nullptr)
                : event;
        });
}

inline void remove_local_magnify_monitor() {
    if (!g_ime.magnify_monitor)
        return;

    if (auto event_class = static_cast<Class>(objc_getClass("NSEvent")))
        objc_send<void>(
            class_as_id(event_class),
            sel_remove_monitor(),
            g_ime.magnify_monitor);
    g_ime.magnify_monitor = nullptr;
}

inline void install_local_scroll_monitor() {
    if (g_ime.scroll_monitor)
        return;

    auto event_class = static_cast<Class>(objc_getClass("NSEvent"));
    if (!event_class)
        return;

    // Intercept local scroll events directly so macOS can preserve AppKit's
    // precise-vs-line semantics.
    g_ime.scroll_monitor = objc_send<id>(
        class_as_id(event_class),
        sel_add_local_monitor_for_events_matching_mask_handler(),
        ns_event_mask_scroll_wheel,
        ^id(id event) {
            return handle_local_scroll_event(event)
                ? static_cast<id>(nullptr)
                : event;
        });
}

inline void remove_local_scroll_monitor() {
    if (!g_ime.scroll_monitor)
        return;

    if (auto event_class = static_cast<Class>(objc_getClass("NSEvent")))
        objc_send<void>(
            class_as_id(event_class),
            sel_remove_monitor(),
            g_ime.scroll_monitor);
    g_ime.scroll_monitor = nullptr;
}

inline void input_attach(native_surface_handle handle, void (*request_repaint)()) {
    auto* surface = desktop_surface(handle);
    g_images.request_repaint = request_repaint;
    g_ime.request_repaint = request_repaint;
    g_ime.ns_window = surface_ns_window(surface);
    g_ime.content_view = surface_content_view(surface);
    g_ime.focused_callback_id = ::phenotype::native::invalid_callback_id;
    g_ime.scroll_tracking_active = false;
    g_ime.last_host_frame = CGRectNull;
    g_ime.last_indicator_frame = CGRectNull;
    g_ime.last_indicator_display_mode = -1;
    g_ime.last_character_rect_draw = CGRectNull;
    g_ime.last_character_rect_host = CGRectNull;
    g_ime.last_character_rect_screen = CGRectNull;
    clear_ime_state();
    ensure_editor_view();
    ensure_system_insertion_indicator();
    install_local_scroll_monitor();
    install_local_magnify_monitor();
    ::phenotype::detail::clear_input_debug_caret_presentation();
    restore_content_view_first_responder();
}

inline void input_detach() {
    remove_local_magnify_monitor();
    remove_local_scroll_monitor();
    discard_marked_text_from_system();
    clear_ime_state();
    if (g_ime.insertion_indicator) {
        objc_send<void>(g_ime.insertion_indicator, sel_remove_from_superview());
        objc_send<void>(g_ime.insertion_indicator, sel_release());
    }
    if (g_ime.caret_host_view) {
        objc_send<void>(g_ime.caret_host_view, sel_remove_from_superview());
        objc_send<void>(g_ime.caret_host_view, sel_release());
    }
    if (g_ime.editor_view) {
        objc_send<void>(g_ime.editor_view, sel_remove_from_superview());
        objc_send<void>(g_ime.editor_view, sel_release());
    }
    g_images.request_repaint = nullptr;
    g_ime.ns_window = nullptr;
    g_ime.content_view = nullptr;
    g_ime.editor_view = nullptr;
    g_ime.caret_host_view = nullptr;
    g_ime.insertion_indicator = nullptr;
    g_ime.request_repaint = nullptr;
    g_ime.focused_callback_id = ::phenotype::native::invalid_callback_id;
    g_ime.scroll_tracking_active = false;
    g_ime.last_host_frame = CGRectNull;
    g_ime.last_indicator_frame = CGRectNull;
    g_ime.last_indicator_display_mode = -1;
    g_ime.last_character_rect_draw = CGRectNull;
    g_ime.last_character_rect_host = CGRectNull;
    g_ime.last_character_rect_screen = CGRectNull;
    ::phenotype::detail::clear_input_debug_caret_presentation();
}

inline void input_sync() {
    bool needs_repaint = process_completed_images() && g_images.request_repaint;
    auto snapshot = ::phenotype::detail::focused_input_snapshot();

    if (!snapshot.valid) {
        if (g_ime.composition_active || !g_ime.marked_text.empty()) {
            discard_marked_text_from_system();
            clear_ime_state();
            needs_repaint = true;
        }
        g_ime.focused_callback_id = ::phenotype::native::invalid_callback_id;
        auto presentation_changed = sync_caret_presentation(snapshot);
        if (editor_is_first_responder())
            restore_content_view_first_responder();
        if (presentation_changed)
            sync_input_context_geometry();
        if (needs_repaint && g_images.request_repaint)
            g_images.request_repaint();
        return;
    }

    ensure_editor_view();
    ensure_system_insertion_indicator();
    auto focus_changed = snapshot.callback_id != g_ime.focused_callback_id;
    if (focus_changed && (g_ime.composition_active || !g_ime.marked_text.empty())) {
        discard_marked_text_from_system();
        clear_ime_state();
        needs_repaint = true;
    }

    g_ime.focused_callback_id = snapshot.callback_id;
    if (!editor_is_first_responder())
        focus_editor_view();
    auto presentation_changed = sync_caret_presentation(snapshot);
    if (focus_changed || g_ime.composition_active || presentation_changed)
        sync_input_context_geometry();

    if (needs_repaint && g_images.request_repaint)
        g_images.request_repaint();
}

inline bool input_dismiss_transient() {
    bool had_transient = g_ime.composition_active || !g_ime.marked_text.empty();
    if (!had_transient)
        return false;
    discard_marked_text_from_system();
    clear_ime_state();
    request_window_repaint();
    return true;
}

inline bool is_visual_effect_view(id view) {
    auto cls = static_cast<Class>(objc_getClass("NSVisualEffectView"));
    if (!view || !cls || !objc_responds_to(view, sel_registerName("isKindOfClass:")))
        return false;
    return objc_send<signed char>(
        view,
        sel_registerName("isKindOfClass:"),
        cls) != 0;
}

inline id find_visual_effect_subview(id view) {
    if (!view || !objc_responds_to(view, sel_registerName("subviews")))
        return nullptr;
    id subviews = objc_send<id>(view, sel_registerName("subviews"));
    if (!subviews || !objc_responds_to(subviews, sel_registerName("count")))
        return nullptr;
    auto const count = objc_send<unsigned long>(
        subviews,
        sel_registerName("count"));
    if (!objc_responds_to(subviews, sel_registerName("objectAtIndex:")))
        return nullptr;
    for (unsigned long index = 0; index < count; ++index) {
        id child = objc_send<id>(
            subviews,
            sel_registerName("objectAtIndex:"),
            index);
        if (is_visual_effect_view(child))
            return child;
    }
    return nullptr;
}

inline long ns_visual_effect_material_value(
        NativeBackdropMaterial material) noexcept {
    constexpr long visual_effect_material_sidebar = 7;
    constexpr long visual_effect_material_under_window_background = 21;
    switch (material) {
        case NativeBackdropMaterial::Sidebar:
            return visual_effect_material_sidebar;
        case NativeBackdropMaterial::UnderWindowBackground:
        default:
            return visual_effect_material_under_window_background;
    }
}

inline void configure_visual_effect_backdrop(
        id effect_view,
        NativeBackdropMaterial material,
        float opacity) {
    if (!effect_view)
        return;
    constexpr long visual_effect_blending_behind_window = 0;
    constexpr long visual_effect_state_active = 1;
    objc_send<void>(
        effect_view,
        sel_registerName("setMaterial:"),
        ns_visual_effect_material_value(material));
    objc_send<void>(
        effect_view,
        sel_registerName("setAlphaValue:"),
        static_cast<double>(sanitize_native_backdrop_opacity(opacity)));
    objc_send<void>(
        effect_view,
        sel_registerName("setBlendingMode:"),
        visual_effect_blending_behind_window);
    objc_send<void>(
        effect_view,
        sel_registerName("setState:"),
        visual_effect_state_active);
    if (objc_responds_to(effect_view, sel_registerName("setEmphasized:"))) {
        objc_send<void>(
            effect_view,
            sel_registerName("setEmphasized:"),
            static_cast<signed char>(1));
    }
}

inline bool install_integrated_titlebar_backdrop_underlay(
        NativeSurfaceDescriptor* surface,
        id ns_window,
        NativeBackdropMaterial material,
        float opacity) {
    if (!surface || !ns_window)
        return false;

    id content_view = surface_content_view(surface);
    if (!content_view)
        return false;

    id window_content = objc_send<id>(ns_window, sel_content_view());
    id content_superview = objc_responds_to(content_view, sel_superview())
        ? objc_send<id>(content_view, sel_superview())
        : nullptr;
    if (is_visual_effect_view(window_content)
        && content_superview == window_content) {
        configure_visual_effect_backdrop(
            window_content,
            material,
            opacity);
        return true;
    }
    if (content_superview && content_superview == window_content) {
        id existing_backdrop = find_visual_effect_subview(content_superview);
        if (existing_backdrop) {
            configure_visual_effect_backdrop(
                existing_backdrop,
                material,
                opacity);
            return true;
        }
    }
    if (window_content != content_view)
        return false;

    auto view_class = static_cast<Class>(objc_getClass("NSView"));
    auto effect_class = static_cast<Class>(objc_getClass("NSVisualEffectView"));
    if (!view_class || !effect_class)
        return false;

    CGRect bounds = objc_responds_to(content_view, sel_bounds())
        ? objc_send<CGRect>(content_view, sel_bounds())
        : CGRect{};
    id container_view = objc_send<id>(
        objc_send<id>(class_as_id(view_class), sel_alloc()),
        sel_init_with_frame(),
        bounds);
    id effect_view = objc_send<id>(
        objc_send<id>(class_as_id(effect_class), sel_alloc()),
        sel_init_with_frame(),
        bounds);
    if (!container_view || !effect_view) {
        if (container_view)
            objc_send<void>(container_view, sel_release());
        if (effect_view)
            objc_send<void>(effect_view, sel_release());
        return false;
    }

    constexpr unsigned long view_width_sizable = 1ul << 1;
    constexpr unsigned long view_height_sizable = 1ul << 4;
    auto const autoresizing = view_width_sizable | view_height_sizable;
    objc_send<void>(
        container_view,
        sel_registerName("setAutoresizingMask:"),
        autoresizing);
    objc_send<void>(
        effect_view,
        sel_registerName("setAutoresizingMask:"),
        autoresizing);
    configure_visual_effect_backdrop(effect_view, material, opacity);

    objc_send<id>(content_view, sel_retain());
    objc_send<void>(
        ns_window,
        sel_registerName("setContentView:"),
        container_view);
    objc_send<void>(effect_view, sel_set_frame(), bounds);
    objc_send<void>(container_view, sel_add_subview(), effect_view);
    objc_send<void>(content_view, sel_set_frame(), bounds);
    objc_send<void>(
        content_view,
        sel_registerName("setAutoresizingMask:"),
        autoresizing);
    objc_send<void>(container_view, sel_add_subview(), content_view);
    objc_send<void>(content_view, sel_release());
    objc_send<void>(effect_view, sel_release());
    objc_send<void>(container_view, sel_release());
    return true;
}

inline void configure_window(native_surface_handle handle,
                             WindowOptions const* options) {
    if (!handle || !options
        || options->chrome != WindowChromeStyle::IntegratedTitlebar)
        return;

    auto surface = desktop_surface(handle);
    auto ns_window = surface_ns_window(surface);
    if (!ns_window)
        return;

    constexpr unsigned long full_size_content_view_mask = 1ul << 15;
    constexpr long hidden_title = 1;
    using ObjcBool = signed char;

    auto style = objc_send<unsigned long>(ns_window, sel_style_mask());
    objc_send<void>(
        ns_window,
        sel_set_style_mask(),
        style | full_size_content_view_mask);
    objc_send<void>(
        ns_window,
        sel_set_titlebar_appears_transparent(),
        static_cast<ObjcBool>(1));
    objc_send<void>(
        ns_window,
        sel_set_title_visibility(),
        hidden_title);
    objc_send<void>(
        ns_window,
        sel_set_movable_by_window_background(),
        static_cast<ObjcBool>(1));
    objc_send<void>(
        ns_window,
        sel_set_opaque(),
        static_cast<ObjcBool>(0));
    auto color_class = static_cast<Class>(objc_getClass("NSColor"));
    if (color_class) {
        auto clear_color = objc_send<id>(
            class_as_id(color_class),
            sel_clear_color());
        if (clear_color) {
            objc_send<void>(
                ns_window,
                sel_set_background_color(),
                clear_color);
        }
    }
    (void)install_integrated_titlebar_backdrop_underlay(
        surface,
        ns_window,
        options->native_backdrop_material,
        options->native_backdrop_opacity);
}

inline void renderer_init(native_surface_handle handle) {
    if (g_renderer.initialized) return;
    auto* surface = desktop_surface(handle);
    g_renderer.surface = surface;
    g_renderer.ns_window = surface_ns_window(surface);
    g_renderer.content_view = surface_content_view(surface);
    if (!g_renderer.ns_window || !g_renderer.content_view) {
        std::fprintf(stderr, "[metal] no NSWindow/NSView surface\n");
        return;
    }

    g_renderer.device = MTL::CreateSystemDefaultDevice();
    if (!g_renderer.device) {
        std::fprintf(stderr, "[metal] no device\n");
        return;
    }
    g_renderer.queue = g_renderer.device->newCommandQueue();

    auto sel_wl = sel_registerName("setWantsLayer:");
    auto sel_sl = sel_registerName("setLayer:");
    void* view = g_renderer.content_view;
    reinterpret_cast<void(*)(void*, SEL, bool)>(objc_msgSend)(view, sel_wl, true);

    g_renderer.layer = CA::MetalLayer::layer()->retain();
    g_renderer.layer->setDevice(g_renderer.device);
    g_renderer.layer->setPixelFormat(MTL::PixelFormatBGRA8Unorm);
    g_renderer.layer->setFramebufferOnly(false);
    if (surface->window_options_valid
        && surface->window_chrome == WindowChromeStyle::IntegratedTitlebar) {
        using ObjcBool = signed char;
        objc_send<void>(
            reinterpret_cast<id>(g_renderer.layer),
            sel_set_opaque(),
            static_cast<ObjcBool>(0));
    }

    int fbw = 0;
    int fbh = 0;
    surface_framebuffer_size(surface, fbw, fbh);
    sync_drawable_size(fbw, fbh);

    // Prime the host's cached content scale even when the host bypassed
    // refresh_cached_canvas_size (e.g. test harnesses that wire the
    // host directly instead of going through run_app_with_platform).
    if (auto* host = ::phenotype::native::detail::active_host())
        host->cached_content_scale = surface_content_scale(surface);

    reinterpret_cast<void(*)(void*, SEL, void*)>(objc_msgSend)(
        view, sel_sl, static_cast<void*>(g_renderer.layer));

    NS::Error* err = nullptr;
    auto* lib = g_renderer.device->newLibrary(
        NS::String::string(MSL_SHADERS, NS::UTF8StringEncoding), nullptr, &err);
    if (!lib) {
        std::fprintf(stderr, "[metal] shader compile: %s\n",
                     err ? err->localizedDescription()->utf8String() : "unknown");
        return;
    }

    g_renderer.tri_pipeline   = create_pipeline(g_renderer.device, lib, "vs_tri",   "fs_tri");
    g_renderer.color_pipeline = create_pipeline(g_renderer.device, lib, "vs_color", "fs_color");
    g_renderer.material_pipeline = create_pipeline(g_renderer.device, lib, "vs_material", "fs_material");
    g_renderer.arc_pipeline   = create_pipeline(g_renderer.device, lib, "vs_arc",   "fs_arc");
    g_renderer.image_pipeline = create_pipeline(g_renderer.device, lib, "vs_image", "fs_image");
    g_renderer.text_pipeline = create_pipeline(g_renderer.device, lib, "vs_text", "fs_text");
    lib->release();
    if (!g_renderer.tri_pipeline || !g_renderer.color_pipeline
        || !g_renderer.material_pipeline
        || !g_renderer.arc_pipeline
        || !g_renderer.image_pipeline || !g_renderer.text_pipeline) {
        std::fprintf(stderr, "[metal] failed to create render pipelines\n");
        return;
    }

    g_renderer.uniform_buf = g_renderer.device->newBuffer(16, MTL::ResourceStorageModeShared);

    auto* sampler_desc = MTL::SamplerDescriptor::alloc()->init();
    sampler_desc->setMinFilter(MTL::SamplerMinMagFilterLinear);
    sampler_desc->setMagFilter(MTL::SamplerMinMagFilterLinear);
    sampler_desc->setSAddressMode(MTL::SamplerAddressModeClampToEdge);
    sampler_desc->setTAddressMode(MTL::SamplerAddressModeClampToEdge);
    g_renderer.sampler = g_renderer.device->newSamplerState(sampler_desc);
    sampler_desc->release();

    g_renderer.initialized = true;
    g_renderer.text_cache.active_scale_key = 0;

    int winw = 0;
    int winh = 0;
    surface_logical_size(surface, winw, winh);
    std::printf("[phenotype-native] Metal initialized (%dx%d)\n", winw, winh);
}

inline void renderer_flush(unsigned char const* buf, unsigned int len) {
    if (len == 0 || !g_renderer.initialized) return;

    NS::AutoreleasePool* pool = NS::AutoreleasePool::alloc()->init();
    auto const flush_started = metrics::detail::now_ns();
    g_renderer.material_executor_summary = MaterialExecutorSummary{};
    // Read the host-cached HiDPI scale. The shell keeps cached_content_scale
    // in sync with native viewport and content-scale changes.
    float frame_scale = 1.0f;
    if (auto* host = ::phenotype::native::detail::active_host())
        frame_scale = host->cached_content_scale;
    if (!(frame_scale > 0.0f))
        frame_scale = surface_content_scale(g_renderer.surface);
    float text_scale = frame_scale;
    float line_height_ratio = ::phenotype::detail::g_app.theme.line_height_ratio;

    double cr = 0.98;
    double cg = 0.98;
    double cb = 0.98;
    bool const transparent_window_surface =
        g_renderer.surface
        && g_renderer.surface->window_options_valid
        && g_renderer.surface->window_chrome
            == WindowChromeStyle::IntegratedTitlebar;
    double ca = transparent_window_surface ? 0.0 : 1.0;
    (void)process_completed_images();
    process_completed_material_backdrop_luma_sample();
    int fbw = 0;
    int fbh = 0;
    surface_framebuffer_size(g_renderer.surface, fbw, fbh);
    if (fbw == 0 || fbh == 0) {
        pool->release();
        return;
    }

    sync_drawable_size(fbw, fbh);
    bool const backdrop_ready =
        g_renderer.material_pipeline
        && g_renderer.material_backdrop_texture
        && g_renderer.last_material_backdrop_available
        && g_renderer.material_backdrop_width == fbw
        && g_renderer.material_backdrop_height == fbh;
    MaterialEnvironment material_env{};
    auto const accessibility = accessibility_display_options();
    g_renderer.accessibility_options = accessibility;
    material_env.capabilities = macos_material_capability_input(
        g_renderer.device,
        g_renderer.material_pipeline != nullptr,
        backdrop_ready,
        accessibility);
    material_env.backdrop.available = backdrop_ready;
    material_env.backdrop.stable = backdrop_ready;
    material_env.backdrop.excludes_foreground_text =
        backdrop_ready
        && g_renderer.last_material_backdrop_excludes_foreground_text;
    material_env.backdrop.source = backdrop_ready
        ? "previous-presented-frame"
        : "none";
    if (backdrop_ready && g_renderer.last_material_backdrop_luma_available) {
        if (g_renderer.last_material_backdrop_color_available) {
            material_env.backdrop.color_mean =
                g_renderer.last_material_backdrop_color_mean;
            material_env.backdrop.color_sample_count =
                g_renderer.last_material_backdrop_luma_sample_count;
            material_env.backdrop.color_sample_status =
                g_renderer.last_material_backdrop_luma_status;
        }
        material_env.backdrop.luma_min =
            g_renderer.last_material_backdrop_luma_min;
        material_env.backdrop.luma_max =
            g_renderer.last_material_backdrop_luma_max;
        material_env.backdrop.luma_mean =
            g_renderer.last_material_backdrop_luma_mean;
        material_env.backdrop.luma_sample_count =
            g_renderer.last_material_backdrop_luma_sample_count;
        material_env.backdrop.luma_sample_grid_width =
            g_renderer.last_material_backdrop_luma_grid_width;
        material_env.backdrop.luma_sample_grid_height =
            g_renderer.last_material_backdrop_luma_grid_height;
        material_env.backdrop.luma_sample_frame =
            g_renderer.last_material_backdrop_luma_frame;
        material_env.backdrop.luma_sample_status =
            g_renderer.last_material_backdrop_luma_status;
        material_env.backdrop.source =
            "previous-presented-frame-sampled-grid";
    }
    material_env.render_target.width = fbw;
    material_env.render_target.height = fbh;
    material_env.render_target.scale = frame_scale;
    material_env.render_target.pixel_format = "bgra8unorm";
    material_env.debug_seed.frame = ++g_renderer.material_frame_sequence;
    material_env.quality =
        macos_material_quality_policy(material_env.capabilities);
    auto decode_started = metrics::detail::now_ns();
    if (!decode_frame_commands(
            buf, len, line_height_ratio,
            material_env,
            g_renderer.scratch, g_renderer.hit_regions,
            cr, cg, cb, ca)) {
        pool->release();
        return;
    }
    if (transparent_window_surface)
        ca = 0.0;
    g_renderer.last_clear_alpha = ca;
    g_renderer.last_clear_alpha_for_transparent_window =
        transparent_window_surface && ca == 0.0;
    g_renderer.last_full_frame_opaque_fill_count =
        g_renderer.scratch.full_frame_opaque_fill_count;
    g_renderer.last_transparent_window_has_opaque_frame_fill =
        transparent_window_surface
        && g_renderer.last_full_frame_opaque_fill_count > 0;
    auto const decode_ns = metrics::detail::now_ns() - decode_started;
    metrics::inst::native_phase_duration.record(
        decode_ns,
        native_attrs("command_decode"));

    auto image_started = metrics::detail::now_ns();
    prepare_image_instances(text_scale);
    metrics::inst::native_phase_duration.record(
        metrics::detail::now_ns() - image_started,
        native_attrs("image_prepare"));

    append_ime_overlay(g_renderer.scratch);
    append_generic_caret_overlay(g_renderer.scratch);

    auto text_started = metrics::detail::now_ns();
    if (!prepare_text_instances(text_scale)) {
        pool->release();
        return;
    }
    metrics::inst::native_phase_duration.record(
        metrics::detail::now_ns() - text_started,
        native_attrs("text_prepare"));

    finalize_batches(g_renderer.scratch);
    apply_material_container_execution_descriptors(g_renderer.scratch);

    MaterialExecutorSummary material_summary;
    material_summary.cpu_decode_ns = decode_ns;
    material_summary.material_pipeline_ready =
        material_env.capabilities.shader_blur;
    material_summary.material_backdrop_source_ready =
        material_env.backdrop.available;
    material_summary.foreground_text_candidate_count =
        g_renderer.scratch.foreground_text_candidate_count;
    material_summary.foreground_text_remap_count =
        g_renderer.scratch.foreground_text_remap_count;
    set_material_executor_backdrop_descriptor_summary(
        material_summary,
        material_env.backdrop);
    for (auto const& record : g_renderer.scratch.material_records) {
        accumulate_material_executor_plan_summary(
            material_summary,
            record.plan);
    }
    finalize_material_executor_summary(
        material_summary,
        g_renderer.scratch.material_records);
    material_summary.material_shader_content_scale = frame_scale;
    for (auto const& record : g_renderer.scratch.material_records) {
        auto const& plan = record.plan;
        if (!material_plan_uses_sampled_backdrop_executor(plan))
            continue;
        auto const step_pixels = std::max(
            1.0f,
            plan.blur_radius
                * plan.sampling_kernel.blur_step_scale
                * frame_scale);
        material_summary.material_max_shader_blur_step_pixels = std::max(
            material_summary.material_max_shader_blur_step_pixels,
            step_pixels);
    }

    int winw = 0;
    int winh = 0;
    surface_logical_size(g_renderer.surface, winw, winh);
    float uniforms[4] = {
        static_cast<float>(winw),
        static_cast<float>(winh),
        frame_scale,
        0,
    };
    std::memcpy(g_renderer.uniform_buf->contents(), uniforms, 16);

    auto image_upload_started = metrics::detail::now_ns();
    if (!upload_image_cache()) {
        pool->release();
        return;
    }
    metrics::inst::native_phase_duration.record(
        metrics::detail::now_ns() - image_upload_started,
        native_attrs("image_upload"));

    auto upload_started = metrics::detail::now_ns();
    if (!upload_text_cache()) {
        pool->release();
        return;
    }
    metrics::inst::native_phase_duration.record(
        metrics::detail::now_ns() - upload_started,
        native_attrs("text_upload"));

    auto* drawable = g_renderer.layer->nextDrawable();
    if (!drawable) {
        pool->release();
        return;
    }

    auto* pass = MTL::RenderPassDescriptor::renderPassDescriptor();
    pass->colorAttachments()->object(0)->setTexture(drawable->texture());
    pass->colorAttachments()->object(0)->setLoadAction(MTL::LoadActionClear);
    pass->colorAttachments()->object(0)->setClearColor(MTL::ClearColor::Make(cr, cg, cb, ca));
    pass->colorAttachments()->object(0)->setStoreAction(MTL::StoreActionStore);

    auto* command_buffer = g_renderer.queue->commandBuffer();
    auto* encoder = command_buffer->renderCommandEncoder(pass);

    auto& scratch = g_renderer.scratch;
    auto const tri_bytes = scratch.tri_vertices.size() * sizeof(TriVertexGPU);
    bool tri_uploaded = false;
    if (!scratch.tri_vertices.empty()) {
        if (!ensure_instance_buffer(
                g_renderer.tri_vertices_buf,
                g_renderer.tri_vertices_capacity,
                tri_bytes,
                "tri_vertices")) {
            encoder->endEncoding();
            pool->release();
            return;
        }
        std::memcpy(
            g_renderer.tri_vertices_buf->contents(),
            scratch.tri_vertices.data(),
            tri_bytes);
        tri_uploaded = true;
    }

    auto const color_bytes = scratch.color_instances.size() * sizeof(ColorInstanceGPU);
    bool color_uploaded = false;
    if (!scratch.color_instances.empty()) {
        if (!ensure_instance_buffer(
                g_renderer.color_instances_buf,
                g_renderer.color_instances_capacity,
                color_bytes,
                "color_instances")) {
            encoder->endEncoding();
            pool->release();
            return;
        }
        std::memcpy(
            g_renderer.color_instances_buf->contents(),
            scratch.color_instances.data(),
            color_bytes);
        color_uploaded = true;
    }

    auto const material_bytes =
        scratch.material_instances.size() * sizeof(MaterialInstanceGPU);
    bool material_uploaded = false;
    auto material_upload_started = metrics::detail::now_ns();
    if (!scratch.material_instances.empty()) {
        auto const old_material_capacity = g_renderer.material_instances_capacity;
        if (!ensure_instance_buffer(
                g_renderer.material_instances_buf,
                g_renderer.material_instances_capacity,
                material_bytes,
                "material_instances")) {
            encoder->endEncoding();
            pool->release();
            return;
        }
        std::memcpy(
            g_renderer.material_instances_buf->contents(),
            scratch.material_instances.data(),
            material_bytes);
        material_uploaded = true;
        material_summary.material_upload_bytes =
            static_cast<std::int64_t>(material_bytes);
        material_summary.material_buffer_capacity_bytes =
            static_cast<std::int64_t>(g_renderer.material_instances_capacity);
        if (g_renderer.material_instances_capacity != old_material_capacity)
            ++material_summary.material_buffer_reallocations;
    }
    material_summary.cpu_material_upload_ns =
        metrics::detail::now_ns() - material_upload_started;

    auto const arc_bytes = scratch.arc_instances.size() * sizeof(ArcInstanceGPU);
    bool arc_uploaded = false;
    if (!scratch.arc_instances.empty()) {
        if (!ensure_instance_buffer(
                g_renderer.arc_instances_buf,
                g_renderer.arc_instances_capacity,
                arc_bytes,
                "arc_instances")) {
            encoder->endEncoding();
            pool->release();
            return;
        }
        std::memcpy(
            g_renderer.arc_instances_buf->contents(),
            scratch.arc_instances.data(),
            arc_bytes);
        arc_uploaded = true;
    }

    auto const image_bytes = scratch.image_instances.size() * sizeof(ImageInstanceGPU);
    bool image_uploaded = false;
    if (!scratch.image_instances.empty() && g_renderer.image_atlas_texture) {
        if (!ensure_instance_buffer(
                g_renderer.image_instances_buf,
                g_renderer.image_instances_capacity,
                image_bytes,
                "image_instances")) {
            encoder->endEncoding();
            pool->release();
            return;
        }
        std::memcpy(
            g_renderer.image_instances_buf->contents(),
            scratch.image_instances.data(),
            image_bytes);
        image_uploaded = true;
    }

    auto const text_bytes = scratch.text_instances.size() * sizeof(TextInstanceGPU);
    bool text_uploaded = false;
    if (!scratch.text_instances.empty() && g_renderer.text_atlas_texture) {
        if (!ensure_instance_buffer(
                g_renderer.text_instances_buf,
                g_renderer.text_instances_capacity,
                text_bytes,
                "text_instances")) {
            encoder->endEncoding();
            pool->release();
            return;
        }
        std::memcpy(
            g_renderer.text_instances_buf->contents(),
            scratch.text_instances.data(),
            text_bytes);
        text_uploaded = true;
    }

    // Per-batch scissor + draws. Each batch's instance ranges live
    // contiguously in the flat *_instances vectors thanks to
    // finalize_batches; we use baseInstance to point at them.
    float const scissor_scale = frame_scale;
    bool const defer_foreground_pass =
        text_uploaded || !scratch.overlay_color_instances.empty();
    for (auto const& batch : scratch.batches) {
        auto const batch_tri_count =
            static_cast<std::uint32_t>(batch.tri_vertices.size());
        auto const batch_color_count =
            static_cast<std::uint32_t>(batch.colors.size());
        auto const batch_material_count =
            static_cast<std::uint32_t>(batch.materials.size());
        auto const batch_arc_count =
            static_cast<std::uint32_t>(batch.arcs.size());
        auto const batch_image_count =
            static_cast<std::uint32_t>(batch.images.size());
        auto const batch_text_count =
            static_cast<std::uint32_t>(batch.texts.size());
        if (batch_tri_count + batch_color_count + batch_material_count + batch_arc_count
            + batch_image_count + batch_text_count == 0)
            continue;
        auto const sr = compute_metal_scissor(
            batch, scissor_scale,
            static_cast<std::uint32_t>(g_renderer.drawable_width),
            static_cast<std::uint32_t>(g_renderer.drawable_height));
        if (sr.width == 0 || sr.height == 0)
            continue;
        encoder->setScissorRect(sr);

        // Triangles render first within the batch so filled regions
        // (HATCH solids) sit beneath any FillRect / DrawLine quads
        // emitted later in the same canvas frame.
        //
        // Bind the vertex buffer with a per-batch byte offset so the
        // shader's [[vertex_id]] starts at 0 for verts[0] = this
        // batch's first vertex. We deliberately do NOT pass tri_first
        // as drawPrimitives' vertexStart — that path is ambiguous
        // across Metal versions about whether [[vertex_id]] is
        // absolute or relative to vertexStart, so non-zero start
        // batches would silently read the wrong slice on platforms
        // that report vertex_id relative to 0.
        if (tri_uploaded && batch_tri_count > 0) {
            encoder->setRenderPipelineState(g_renderer.tri_pipeline);
            encoder->setVertexBuffer(g_renderer.uniform_buf, 0, 0);
            encoder->setVertexBuffer(
                g_renderer.tri_vertices_buf,
                NS::UInteger(batch.tri_first * sizeof(TriVertexGPU)),
                1);
            encoder->drawPrimitives(
                MTL::PrimitiveTypeTriangle,
                NS::UInteger(0),
                NS::UInteger(batch_tri_count));
        }

        if (color_uploaded && batch_color_count > 0) {
            encoder->setRenderPipelineState(g_renderer.color_pipeline);
            encoder->setVertexBuffer(g_renderer.uniform_buf, 0, 0);
            encoder->setVertexBuffer(g_renderer.color_instances_buf, 0, 1);
            encoder->drawPrimitives(
                MTL::PrimitiveTypeTriangle,
                NS::UInteger(0), 6,
                NS::UInteger(batch_color_count),
                NS::UInteger(batch.color_first));
        }
        if (material_uploaded && batch_material_count > 0
            && g_renderer.material_backdrop_texture
            && g_renderer.last_material_backdrop_available) {
            encoder->setRenderPipelineState(g_renderer.material_pipeline);
            encoder->setVertexBuffer(g_renderer.uniform_buf, 0, 0);
            encoder->setVertexBuffer(g_renderer.material_instances_buf, 0, 1);
            encoder->setFragmentTexture(g_renderer.material_backdrop_texture, 0);
            encoder->setFragmentSamplerState(g_renderer.sampler, 0);
            encoder->drawPrimitives(
                MTL::PrimitiveTypeTriangle,
                NS::UInteger(0), 6,
                NS::UInteger(batch_material_count),
                NS::UInteger(batch.material_first));
            ++material_summary.material_draw_calls;
        }
        if (arc_uploaded && batch_arc_count > 0) {
            encoder->setRenderPipelineState(g_renderer.arc_pipeline);
            encoder->setVertexBuffer(g_renderer.uniform_buf, 0, 0);
            encoder->setVertexBuffer(g_renderer.arc_instances_buf, 0, 1);
            encoder->drawPrimitives(
                MTL::PrimitiveTypeTriangle,
                NS::UInteger(0), 6,
                NS::UInteger(batch_arc_count),
                NS::UInteger(batch.arc_first));
        }
        if (image_uploaded && batch_image_count > 0) {
            encoder->setRenderPipelineState(g_renderer.image_pipeline);
            encoder->setVertexBuffer(g_renderer.uniform_buf, 0, 0);
            encoder->setVertexBuffer(g_renderer.image_instances_buf, 0, 1);
            encoder->setFragmentTexture(g_renderer.image_atlas_texture, 0);
            encoder->setFragmentSamplerState(g_renderer.sampler, 0);
            encoder->drawPrimitives(
                MTL::PrimitiveTypeTriangle,
                NS::UInteger(0), 6,
                NS::UInteger(batch_image_count),
                NS::UInteger(batch.image_first));
        }
        if (!defer_foreground_pass && text_uploaded && batch_text_count > 0) {
            encoder->setRenderPipelineState(g_renderer.text_pipeline);
            encoder->setVertexBuffer(g_renderer.uniform_buf, 0, 0);
            encoder->setVertexBuffer(g_renderer.text_instances_buf, 0, 1);
            encoder->setFragmentTexture(g_renderer.text_atlas_texture, 0);
            encoder->setFragmentSamplerState(g_renderer.sampler, 0);
            encoder->drawPrimitives(
                MTL::PrimitiveTypeTriangle,
                NS::UInteger(0), 6,
                NS::UInteger(batch_text_count),
                NS::UInteger(batch.text_first));
        }
    }

    // Overlays render Z-above the batched scene with full-drawable
    // scissor so the last batch's clip rect doesn't leak into them.
    encoder->setScissorRect(MTL::ScissorRect{
        0, 0,
        static_cast<NS::UInteger>(g_renderer.drawable_width),
        static_cast<NS::UInteger>(g_renderer.drawable_height)});

    auto const overlay_color_bytes =
        scratch.overlay_color_instances.size() * sizeof(ColorInstanceGPU);
    uint32_t overlay_color_count =
        static_cast<uint32_t>(scratch.overlay_color_instances.size());
    if (!defer_foreground_pass && overlay_color_count > 0) {
        if (!ensure_instance_buffer(
                g_renderer.overlay_color_instances_buf,
                g_renderer.overlay_color_instances_capacity,
                overlay_color_bytes,
                "overlay_color_instances")) {
            encoder->endEncoding();
            pool->release();
            return;
        }
        std::memcpy(
            g_renderer.overlay_color_instances_buf->contents(),
            scratch.overlay_color_instances.data(),
            overlay_color_bytes);
        encoder->setRenderPipelineState(g_renderer.color_pipeline);
        encoder->setVertexBuffer(g_renderer.uniform_buf, 0, 0);
        encoder->setVertexBuffer(g_renderer.overlay_color_instances_buf, 0, 1);
        encoder->drawPrimitives(
            MTL::PrimitiveTypeTriangle,
            NS::UInteger(0), 6, NS::UInteger(overlay_color_count));
    }

    if (!defer_foreground_pass && text_uploaded && scratch.overlay_text_count > 0) {
        encoder->setRenderPipelineState(g_renderer.text_pipeline);
        encoder->setVertexBuffer(g_renderer.uniform_buf, 0, 0);
        encoder->setVertexBuffer(g_renderer.text_instances_buf, 0, 1);
        encoder->setFragmentTexture(g_renderer.text_atlas_texture, 0);
        encoder->setFragmentSamplerState(g_renderer.sampler, 0);
        encoder->drawPrimitives(
            MTL::PrimitiveTypeTriangle,
            NS::UInteger(0), 6,
            NS::UInteger(scratch.overlay_text_count),
            NS::UInteger(scratch.overlay_text_first));
    }

    encoder->endEncoding();
    g_renderer.last_render_width = fbw;
    g_renderer.last_render_height = fbh;
    g_renderer.last_material_backdrop_available = false;
    g_renderer.last_material_backdrop_excludes_foreground_text = false;
    bool const capture_frame_history =
        material_executor_requires_frame_capture(material_summary);
    if (capture_frame_history && ensure_material_backdrop_texture(fbw, fbh)) {
        if (auto* blit = command_buffer->blitCommandEncoder()) {
            MTL::Origin origin{0, 0, 0};
            MTL::Size size{
                NS::UInteger(fbw),
                NS::UInteger(fbh),
                NS::UInteger(1),
            };
            blit->copyFromTexture(
                drawable->texture(),
                NS::UInteger(0),
                NS::UInteger(0),
                origin,
                size,
                g_renderer.material_backdrop_texture,
                NS::UInteger(0),
                NS::UInteger(0),
                origin);
            (void)schedule_material_backdrop_luma_sample(
                blit,
                command_buffer,
                drawable->texture(),
                fbw,
                fbh,
                material_env.debug_seed.frame,
                material_summary);
            blit->endEncoding();
            g_renderer.last_material_backdrop_available = true;
            g_renderer.last_material_backdrop_excludes_foreground_text = true;
            material_summary.backdrop_copy_excludes_foreground_text = true;
            material_summary.backdrop_copy_count = 1;
            material_summary.backdrop_copy_pixels =
                static_cast<std::int64_t>(fbw)
                * static_cast<std::int64_t>(fbh);
        } else {
            material_summary.backdrop_copy_skipped_count = 1;
            material_summary.backdrop_copy_skip_reason =
                "metal-blit-encoder-unavailable";
        }
    } else if (capture_frame_history) {
        material_summary.backdrop_copy_skipped_count = 1;
        material_summary.backdrop_copy_skip_reason =
            "material-backdrop-texture-unavailable";
    } else {
        material_summary.backdrop_copy_skipped_count = 1;
        material_summary.backdrop_copy_skip_reason =
            "no-material-plan-frame-capture";
    }

    if (defer_foreground_pass) {
        auto* foreground_pass = MTL::RenderPassDescriptor::renderPassDescriptor();
        foreground_pass->colorAttachments()->object(0)->setTexture(
            drawable->texture());
        foreground_pass->colorAttachments()->object(0)->setLoadAction(
            MTL::LoadActionLoad);
        foreground_pass->colorAttachments()->object(0)->setStoreAction(
            MTL::StoreActionStore);
        auto* foreground_encoder =
            command_buffer->renderCommandEncoder(foreground_pass);
        if (!foreground_encoder) {
            release_material_backdrop_luma_pending_command_buffer();
            pool->release();
            return;
        }

        for (auto const& batch : scratch.batches) {
            auto const batch_text_count =
                static_cast<std::uint32_t>(batch.texts.size());
            if (!text_uploaded || batch_text_count == 0)
                continue;
            auto const sr = compute_metal_scissor(
                batch, scissor_scale,
                static_cast<std::uint32_t>(g_renderer.drawable_width),
                static_cast<std::uint32_t>(g_renderer.drawable_height));
            if (sr.width == 0 || sr.height == 0)
                continue;
            foreground_encoder->setScissorRect(sr);
            foreground_encoder->setRenderPipelineState(g_renderer.text_pipeline);
            foreground_encoder->setVertexBuffer(g_renderer.uniform_buf, 0, 0);
            foreground_encoder->setVertexBuffer(g_renderer.text_instances_buf, 0, 1);
            foreground_encoder->setFragmentTexture(g_renderer.text_atlas_texture, 0);
            foreground_encoder->setFragmentSamplerState(g_renderer.sampler, 0);
            foreground_encoder->drawPrimitives(
                MTL::PrimitiveTypeTriangle,
                NS::UInteger(0), 6,
                NS::UInteger(batch_text_count),
                NS::UInteger(batch.text_first));
        }

        foreground_encoder->setScissorRect(MTL::ScissorRect{
            0, 0,
            static_cast<NS::UInteger>(g_renderer.drawable_width),
            static_cast<NS::UInteger>(g_renderer.drawable_height)});

        if (overlay_color_count > 0) {
            if (!ensure_instance_buffer(
                    g_renderer.overlay_color_instances_buf,
                    g_renderer.overlay_color_instances_capacity,
                    overlay_color_bytes,
                    "overlay_color_instances")) {
                foreground_encoder->endEncoding();
                release_material_backdrop_luma_pending_command_buffer();
                pool->release();
                return;
            }
            std::memcpy(
                g_renderer.overlay_color_instances_buf->contents(),
                scratch.overlay_color_instances.data(),
                overlay_color_bytes);
            foreground_encoder->setRenderPipelineState(g_renderer.color_pipeline);
            foreground_encoder->setVertexBuffer(g_renderer.uniform_buf, 0, 0);
            foreground_encoder->setVertexBuffer(
                g_renderer.overlay_color_instances_buf, 0, 1);
            foreground_encoder->drawPrimitives(
                MTL::PrimitiveTypeTriangle,
                NS::UInteger(0), 6, NS::UInteger(overlay_color_count));
        }

        if (text_uploaded && scratch.overlay_text_count > 0) {
            foreground_encoder->setRenderPipelineState(g_renderer.text_pipeline);
            foreground_encoder->setVertexBuffer(g_renderer.uniform_buf, 0, 0);
            foreground_encoder->setVertexBuffer(g_renderer.text_instances_buf, 0, 1);
            foreground_encoder->setFragmentTexture(g_renderer.text_atlas_texture, 0);
            foreground_encoder->setFragmentSamplerState(g_renderer.sampler, 0);
            foreground_encoder->drawPrimitives(
                MTL::PrimitiveTypeTriangle,
                NS::UInteger(0), 6,
                NS::UInteger(scratch.overlay_text_count),
                NS::UInteger(scratch.overlay_text_first));
        }

        foreground_encoder->endEncoding();
        material_summary.foreground_pass_after_backdrop_copy = true;
    }

    g_renderer.last_frame_available = false;
    if (ensure_debug_capture_texture(fbw, fbh)) {
        if (auto* blit = command_buffer->blitCommandEncoder()) {
            MTL::Origin origin{0, 0, 0};
            MTL::Size size{
                NS::UInteger(fbw),
                NS::UInteger(fbh),
                NS::UInteger(1),
            };
            blit->copyFromTexture(
                drawable->texture(),
                NS::UInteger(0),
                NS::UInteger(0),
                origin,
                size,
                g_renderer.debug_capture_texture,
                NS::UInteger(0),
                NS::UInteger(0),
                origin);
            blit->endEncoding();
            g_renderer.last_frame_available = true;
        }
    }
    command_buffer->presentDrawable(drawable);
    command_buffer->commit();
    material_summary.cpu_total_ns =
        metrics::detail::now_ns() - flush_started;
    finalize_material_executor_execution_status(material_summary);
    g_renderer.material_executor_summary = material_summary;

    pool->release();
}

inline void renderer_shutdown() {
    release_material_backdrop_luma_pending_command_buffer();
    if (g_renderer.sampler) { g_renderer.sampler->release(); g_renderer.sampler = nullptr; }
    if (g_renderer.debug_capture_texture) { g_renderer.debug_capture_texture->release(); g_renderer.debug_capture_texture = nullptr; }
    if (g_renderer.material_backdrop_texture) { g_renderer.material_backdrop_texture->release(); g_renderer.material_backdrop_texture = nullptr; }
    if (g_renderer.material_backdrop_luma_sample_buf) { g_renderer.material_backdrop_luma_sample_buf->release(); g_renderer.material_backdrop_luma_sample_buf = nullptr; }
    g_renderer.last_frame_available = false;
    g_renderer.last_material_backdrop_available = false;
    g_renderer.last_material_backdrop_excludes_foreground_text = false;
    g_renderer.last_material_backdrop_color_available = false;
    g_renderer.last_material_backdrop_color_mean = {255, 255, 255, 255};
    g_renderer.last_material_backdrop_luma_available = false;
    if (g_renderer.frame_readback_buf) { g_renderer.frame_readback_buf->release(); g_renderer.frame_readback_buf = nullptr; }
    if (g_renderer.image_atlas_texture) { g_renderer.image_atlas_texture->release(); g_renderer.image_atlas_texture = nullptr; }
    if (g_renderer.text_atlas_texture) { g_renderer.text_atlas_texture->release(); g_renderer.text_atlas_texture = nullptr; }
    if (g_renderer.image_instances_buf) { g_renderer.image_instances_buf->release(); g_renderer.image_instances_buf = nullptr; }
    if (g_renderer.text_instances_buf) { g_renderer.text_instances_buf->release(); g_renderer.text_instances_buf = nullptr; }
    if (g_renderer.color_instances_buf) { g_renderer.color_instances_buf->release(); g_renderer.color_instances_buf = nullptr; }
    if (g_renderer.material_instances_buf) { g_renderer.material_instances_buf->release(); g_renderer.material_instances_buf = nullptr; }
    if (g_renderer.tri_vertices_buf) { g_renderer.tri_vertices_buf->release(); g_renderer.tri_vertices_buf = nullptr; }
    if (g_renderer.arc_instances_buf) { g_renderer.arc_instances_buf->release(); g_renderer.arc_instances_buf = nullptr; }
    if (g_renderer.overlay_color_instances_buf) { g_renderer.overlay_color_instances_buf->release(); g_renderer.overlay_color_instances_buf = nullptr; }
    if (g_renderer.uniform_buf) { g_renderer.uniform_buf->release(); g_renderer.uniform_buf = nullptr; }
    if (g_renderer.image_pipeline) { g_renderer.image_pipeline->release(); g_renderer.image_pipeline = nullptr; }
    if (g_renderer.text_pipeline) { g_renderer.text_pipeline->release(); g_renderer.text_pipeline = nullptr; }
    if (g_renderer.arc_pipeline) { g_renderer.arc_pipeline->release(); g_renderer.arc_pipeline = nullptr; }
    if (g_renderer.material_pipeline) { g_renderer.material_pipeline->release(); g_renderer.material_pipeline = nullptr; }
    if (g_renderer.color_pipeline) { g_renderer.color_pipeline->release(); g_renderer.color_pipeline = nullptr; }
    if (g_renderer.tri_pipeline) { g_renderer.tri_pipeline->release(); g_renderer.tri_pipeline = nullptr; }
    if (g_renderer.layer) { g_renderer.layer->release(); g_renderer.layer = nullptr; }
    if (g_renderer.queue) { g_renderer.queue->release(); g_renderer.queue = nullptr; }
    if (g_renderer.device) { g_renderer.device->release(); g_renderer.device = nullptr; }
    g_renderer.hit_regions.clear();
    g_renderer.scratch.clear();
    reset_image_cache();
    g_renderer.text_cache.entries.clear();
    g_renderer.text_cache.pixels.clear();
    g_renderer.text_cache.cursor_x = 0;
    g_renderer.text_cache.cursor_y = 0;
    g_renderer.text_cache.row_height = 0;
    g_renderer.text_cache.dirty = false;
    g_renderer.text_cache.dirty_min_x = TextAtlasCache::atlas_size;
    g_renderer.text_cache.dirty_min_y = TextAtlasCache::atlas_size;
    g_renderer.text_cache.dirty_max_x = 0;
    g_renderer.text_cache.dirty_max_y = 0;
    g_renderer.text_cache.active_scale_key = 0;
    g_renderer.tri_vertices_capacity = 0;
    g_renderer.color_instances_capacity = 0;
    g_renderer.material_instances_capacity = 0;
    g_renderer.overlay_color_instances_capacity = 0;
    g_renderer.image_instances_capacity = 0;
    g_renderer.text_instances_capacity = 0;
    g_renderer.frame_readback_capacity = 0;
    g_renderer.material_backdrop_luma_sample_capacity = 0;
    g_renderer.drawable_width = 0;
    g_renderer.drawable_height = 0;
    g_renderer.debug_capture_width = 0;
    g_renderer.debug_capture_height = 0;
    g_renderer.material_backdrop_width = 0;
    g_renderer.material_backdrop_height = 0;
    g_renderer.last_render_width = 0;
    g_renderer.last_render_height = 0;
    g_renderer.last_clear_alpha = 1.0;
    g_renderer.last_clear_alpha_for_transparent_window = false;
    g_renderer.last_full_frame_opaque_fill_count = 0;
    g_renderer.last_transparent_window_has_opaque_frame_fill = false;
    g_renderer.material_frame_sequence = 0;
    g_renderer.material_backdrop_luma_skipped_sample_count = 0;
    g_renderer.last_material_backdrop_luma_sample_count = 0;
    g_renderer.last_material_backdrop_luma_grid_width = 0;
    g_renderer.last_material_backdrop_luma_grid_height = 0;
    g_renderer.last_material_backdrop_luma_frame = 0;
    g_renderer.last_material_backdrop_luma_status = "not-sampled";
    g_renderer.material_executor_summary = MaterialExecutorSummary{};
    g_renderer.last_frame_available = false;
    g_renderer.surface = nullptr;
    g_renderer.ns_window = nullptr;
    g_renderer.content_view = nullptr;
    g_renderer.initialized = false;
}

inline std::optional<unsigned int> renderer_hit_test(float x, float y,
                                                     float scroll_x,
                                                     float scroll_y) {
    float wx = x + scroll_x;
    float wy = y + scroll_y;
    for (int i = static_cast<int>(g_renderer.hit_regions.size()) - 1; i >= 0; --i) {
        auto const& hr = g_renderer.hit_regions[static_cast<std::size_t>(i)];
        if (wx >= hr.x && wx <= hr.x + hr.w && wy >= hr.y && wy <= hr.y + hr.h)
            return hr.callback_id;
    }
    return std::nullopt;
}

} // namespace phenotype::native::detail
#endif

export namespace phenotype::native::macos_test {
#ifdef __APPLE__
struct Utf8Range {
    std::size_t start = 0;
    std::size_t end = 0;
};

struct CompositionVisualDebug {
    std::string visible_text;
    std::size_t caret_bytes = 0;
    std::size_t marked_start = 0;
    std::size_t marked_end = 0;
};

struct SelectedRangeDebug {
    unsigned long location_utf16 = 0;
    unsigned long length_utf16 = 0;
};

struct SystemCaretDebug {
    bool supported = false;
    bool attached = false;
    long long display_mode = -1;
    long long automatic_mode_options = 0;
    bool scroll_tracking_active = false;
    bool host_flipped = false;
    ::phenotype::diag::RectSnapshot frame{};
    ::phenotype::diag::RectSnapshot draw_rect{};
    ::phenotype::diag::RectSnapshot host_rect{};
    ::phenotype::diag::RectSnapshot screen_rect{};
    ::phenotype::diag::RectSnapshot host_bounds{};
    ::phenotype::diag::RectSnapshot first_rect_screen{};
};

struct RemoteImageDebug {
    bool entry_exists = false;
    int entry_state = -1;
    std::string failure_reason;
    std::size_t pending_jobs = 0;
    std::size_t completed_jobs = 0;
    bool worker_started = false;
};

struct RasterizedTextRunDebug {
    bool has_ink = false;
    float logical_width = 0.0f;
    float x_offset = 0.0f;
    float width = 0.0f;
    float draw_right = 0.0f;
    float right_gap = 0.0f;
    int first_ink_row = -1;
    int last_ink_row = -1;
    int pixel_height = 0;
    std::uint64_t top_alpha = 0;
    std::uint64_t bottom_alpha = 0;
};

struct ScissorBatchDebug {
    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;
    bool pending_text_runs = false;
    std::size_t text_count = 0;
};

struct ScissorDecodeDebug {
    bool ok = false;
    std::vector<ScissorBatchDebug> batches;
    std::vector<std::uint32_t> text_run_batches;
};

inline Utf8Range utf16_range_to_utf8(std::string const& text,
                                     unsigned long location,
                                     unsigned long length) {
    auto range = detail::utf16_range_to_utf8_range(text, {location, length});
    return {range.start, range.end};
}

inline unsigned long utf8_prefix_to_utf16(std::string const& text,
                                          std::size_t bytes) {
    bytes = ::phenotype::detail::clamp_utf8_boundary(text, bytes);
    return detail::utf16_length(std::string_view(text.data(), bytes));
}

inline float normalize_scroll_delta(double scrolling_delta_y,
                                    bool has_precise_scrolling_deltas,
                                    float line_height) {
    return detail::macos_normalize_scroll_delta(
        scrolling_delta_y,
        has_precise_scrolling_deltas,
        line_height);
}

inline float scroll_delta_multiplier(bool horizontal) {
    return detail::macos_scroll_delta_multiplier(horizontal);
}

inline void record_scroll_runtime_event_for_tests(
        double raw_delta_x,
        double raw_delta_y,
        bool has_precise_scrolling_deltas,
        float line_height,
        float app_vertical_multiplier,
        float app_horizontal_multiplier,
        float normalized_delta_x,
        float normalized_delta_y,
        bool handled_x,
        bool handled_y) {
    detail::record_macos_scroll_runtime_event(
        raw_delta_x,
        raw_delta_y,
        has_precise_scrolling_deltas,
        line_height,
        app_vertical_multiplier,
        app_horizontal_multiplier,
        normalized_delta_x,
        normalized_delta_y,
        640.0f,
        480.0f,
        detail::ns_event_phase_changed,
        detail::ns_event_phase_none,
        false,
        handled_x,
        handled_y);
}

inline bool last_scroll_event_available_for_tests() {
    return detail::g_ime.last_scroll_event.available;
}

inline std::string last_scroll_event_source_for_tests() {
    return detail::g_ime.last_scroll_event.source;
}

inline float last_scroll_event_normalized_y_for_tests() {
    return detail::g_ime.last_scroll_event.normalized_delta_y;
}

inline float last_scroll_event_vertical_multiplier_for_tests() {
    return detail::g_ime.last_scroll_event.app_vertical_multiplier;
}

inline bool last_scroll_event_handled_y_for_tests() {
    return detail::g_ime.last_scroll_event.handled_y;
}

inline CompositionVisualDebug build_visual_text(std::string const& committed,
                                                std::size_t replacement_start,
                                                std::size_t replacement_end,
                                                std::string const& marked,
                                                unsigned long selected_location_utf16) {
    replacement_start = ::phenotype::detail::clamp_utf8_boundary(committed, replacement_start);
    replacement_end = ::phenotype::detail::clamp_utf8_boundary(committed, replacement_end);
    if (replacement_end < replacement_start)
        replacement_end = replacement_start;

    auto prefix = committed.substr(0, replacement_start);
    auto suffix = committed.substr(replacement_end);
    auto marked_cursor = detail::utf16_offset_to_utf8(marked, selected_location_utf16);
    return {
        prefix + marked + suffix,
        std::min(prefix.size() + marked_cursor, prefix.size() + marked.size() + suffix.size()),
        prefix.size(),
        prefix.size() + marked.size(),
    };
}

inline void force_disable_system_caret(bool disabled) {
    detail::g_force_disable_system_caret_for_tests = disabled;
}

inline void set_scroll_tracking_for_tests(bool active) {
    detail::g_ime.scroll_tracking_active = active;
}

inline void reset_image_cache_for_tests() {
    detail::reset_image_cache(true);
}

inline void set_image_queue_only_for_tests(bool enabled) {
    detail::g_images.queue_only_for_tests = enabled;
    if (enabled)
        detail::shutdown_image_worker();
}

inline RemoteImageDebug remote_image_debug(std::string const& url) {
    RemoteImageDebug debug{};
    if (auto it = detail::g_images.cache.find(url); it != detail::g_images.cache.end()) {
        debug.entry_exists = true;
        debug.entry_state = static_cast<int>(it->second.state);
        debug.failure_reason = it->second.failure_reason;
    }
    {
        std::lock_guard lock(detail::g_images.mutex);
        debug.pending_jobs = detail::g_images.pending_jobs.size();
        debug.completed_jobs = detail::g_images.completed_jobs.size();
        debug.worker_started = detail::g_images.worker_started;
    }
    return debug;
}

inline RasterizedTextRunDebug rasterized_text_run_debug(std::string const& text,
                                                        float font_size,
                                                        bool mono,
                                                        float line_height,
                                                        float scale) {
    RasterizedTextRunDebug debug{};
    debug.logical_width = detail::text_measure(
        font_size,
        mono,
        text.data(),
        static_cast<unsigned int>(text.size()));

    detail::RasterizedTextRun rasterized;
    detail::FontCacheKey key{};
    key.mono = mono;
    if (!detail::rasterize_text_run(
            text.data(),
            static_cast<unsigned int>(text.size()),
            font_size,
            key,
            line_height,
            scale,
            /*width_factor=*/1.0f,
            rasterized)) {
        return debug;
    }

    debug.has_ink = rasterized.has_ink;
    debug.x_offset = rasterized.x_offset;
    debug.width = rasterized.width;
    debug.draw_right = rasterized.x_offset + rasterized.width;
    debug.right_gap = debug.logical_width - debug.draw_right;
    debug.pixel_height = rasterized.pixel_height;

    int split = rasterized.pixel_height / 2;
    for (int row = 0; row < rasterized.pixel_height; ++row) {
        for (int col = 0; col < rasterized.pixel_width; ++col) {
            auto alpha = rasterized.alpha[static_cast<std::size_t>(
                row * rasterized.pixel_width + col)];
            if (alpha != 0) {
                if (debug.first_ink_row < 0)
                    debug.first_ink_row = row;
                debug.last_ink_row = row;
            }
            if (row < split) {
                debug.top_alpha += alpha;
            } else {
                debug.bottom_alpha += alpha;
            }
        }
    }
    return debug;
}

inline ScissorDecodeDebug decode_scissor_batches_debug(
        std::vector<unsigned char> const& commands) {
    ScissorDecodeDebug debug{};
    detail::FrameScratch scratch;
    std::vector<HitRegionCmd> hit_regions;
    double cr = 0.0;
    double cg = 0.0;
    double cb = 0.0;
    double ca = 0.0;
    MaterialEnvironment material_env{};
    debug.ok = detail::decode_frame_commands(
        commands.data(),
        static_cast<unsigned int>(commands.size()),
        1.6f,
        material_env,
        scratch,
        hit_regions,
        cr,
        cg,
        cb,
        ca);
    if (!debug.ok)
        return debug;
    for (auto const& batch : scratch.batches) {
        debug.batches.push_back(ScissorBatchDebug{
            batch.x,
            batch.y,
            batch.w,
            batch.h,
            batch.pending_text_runs,
            batch.texts.size(),
        });
    }
    for (auto const& run : scratch.text_runs)
        debug.text_run_batches.push_back(run.batch_idx);
    return debug;
}

inline SelectedRangeDebug selected_range_debug() {
    auto range = detail::current_selected_range();
    return {
        range.location == detail::cocoa_not_found ? 0u : range.location,
        range.location == detail::cocoa_not_found ? 0u : range.length,
    };
}

inline ::phenotype::diag::RectSnapshot first_rect_for_range_debug(
        unsigned long location_utf16,
        unsigned long length_utf16) {
    auto rect = detail::editor_first_rect_for_character_range(
        static_cast<id>(nullptr),
        nullptr,
        {location_utf16, length_utf16},
        nullptr);
    return detail::rect_snapshot(rect);
}

inline void invoke_command_for_tests(char const* selector_name) {
    if (!selector_name)
        return;
    detail::editor_do_command_by_selector(
        static_cast<id>(nullptr),
        nullptr,
        sel_registerName(selector_name));
}

inline bool perform_key_equivalent_for_tests(
        unsigned long long modifier_flags,
        char const* characters_ignoring_modifiers,
        unsigned short key_code = 0) {
    return detail::editor_handle_key_equivalent(
        modifier_flags,
        key_code,
        std::string_view{
            characters_ignoring_modifiers ? characters_ignoring_modifiers : "",
            characters_ignoring_modifiers ? std::strlen(characters_ignoring_modifiers) : 0u});
}

inline SystemCaretDebug system_caret_debug() {
    SystemCaretDebug debug{};
    debug.supported = detail::system_caret_supported();
    debug.attached = detail::g_ime.insertion_indicator != nullptr;
    debug.scroll_tracking_active = detail::g_ime.scroll_tracking_active;
    debug.host_flipped = detail::caret_host_view_is_flipped();
    debug.draw_rect = detail::rect_snapshot(detail::g_ime.last_character_rect_draw);
    debug.host_rect = detail::rect_snapshot(detail::g_ime.last_character_rect_host);
    debug.host_bounds = detail::rect_snapshot(detail::current_caret_host_bounds());
    if (detail::g_ime.insertion_indicator) {
        auto frame = detail::objc_send<CGRect>(
            detail::g_ime.insertion_indicator,
            detail::sel_frame());
        debug.display_mode = detail::objc_send<long long>(
            detail::g_ime.insertion_indicator,
            detail::sel_display_mode());
        debug.automatic_mode_options = detail::objc_send<long long>(
            detail::g_ime.insertion_indicator,
            detail::sel_automatic_mode_options());
        if (!CGRectIsNull(frame)) {
            debug.frame = {
                true,
                static_cast<float>(frame.origin.x),
                static_cast<float>(frame.origin.y),
                static_cast<float>(frame.size.width),
                static_cast<float>(frame.size.height),
            };
            auto screen = detail::host_view_rect_to_screen(frame);
            if (!CGRectIsNull(screen)) {
                debug.screen_rect = {
                    true,
                    static_cast<float>(screen.origin.x),
                    static_cast<float>(screen.origin.y),
                    static_cast<float>(screen.size.width),
                    static_cast<float>(screen.size.height),
                };
            }
        }
    }

    auto selected = detail::current_selected_range();
    auto first_rect = detail::editor_first_rect_for_character_range(
        static_cast<id>(nullptr),
        nullptr,
        selected,
        nullptr);
    if (!CGRectIsNull(first_rect)) {
        debug.first_rect_screen = {
            true,
            static_cast<float>(first_rect.origin.x),
            static_cast<float>(first_rect.origin.y),
            static_cast<float>(first_rect.size.width),
            static_cast<float>(first_rect.size.height),
        };
    }
    return debug;
}

inline void set_composition_for_tests(std::string marked_text,
                                      std::size_t replacement_start,
                                      std::size_t replacement_end,
                                      unsigned long selected_location_utf16) {
    auto snapshot = ::phenotype::detail::focused_input_snapshot();
    if (!snapshot.valid)
        return;
    replacement_start = ::phenotype::detail::clamp_utf8_boundary(snapshot.value, replacement_start);
    replacement_end = ::phenotype::detail::clamp_utf8_boundary(snapshot.value, replacement_end);
    if (replacement_end < replacement_start)
        replacement_end = replacement_start;
    detail::g_ime.marked_text = std::move(marked_text);
    detail::g_ime.composition_active = !detail::g_ime.marked_text.empty();
    detail::g_ime.composition_anchor = replacement_start;
    detail::g_ime.replacement_start = replacement_start;
    detail::g_ime.replacement_end = replacement_end;
    detail::g_ime.marked_selection = {
        selected_location_utf16,
        0,
    };
    ::phenotype::detail::set_input_composition_state(
        detail::g_ime.composition_active,
        detail::g_ime.marked_text,
        static_cast<unsigned int>(
            detail::utf16_offset_to_utf8(detail::g_ime.marked_text, selected_location_utf16)));
}

inline void clear_composition_for_tests() {
    detail::clear_ime_state();
}
#else
struct Utf8Range {
    std::size_t start = 0;
    std::size_t end = 0;
};

struct CompositionVisualDebug {
    std::string visible_text;
    std::size_t caret_bytes = 0;
    std::size_t marked_start = 0;
    std::size_t marked_end = 0;
};

struct SelectedRangeDebug {
    unsigned long location_utf16 = 0;
    unsigned long length_utf16 = 0;
};

struct SystemCaretDebug {
    bool supported = false;
    bool attached = false;
    long long display_mode = -1;
    long long automatic_mode_options = 0;
    bool scroll_tracking_active = false;
    bool host_flipped = false;
    ::phenotype::diag::RectSnapshot frame{};
    ::phenotype::diag::RectSnapshot draw_rect{};
    ::phenotype::diag::RectSnapshot host_rect{};
    ::phenotype::diag::RectSnapshot screen_rect{};
    ::phenotype::diag::RectSnapshot host_bounds{};
    ::phenotype::diag::RectSnapshot first_rect_screen{};
};

struct RemoteImageDebug {
    bool entry_exists = false;
    int entry_state = -1;
    std::string failure_reason;
    std::size_t pending_jobs = 0;
    std::size_t completed_jobs = 0;
    bool worker_started = false;
};

struct RasterizedTextRunDebug {
    bool has_ink = false;
    float logical_width = 0.0f;
    float x_offset = 0.0f;
    float width = 0.0f;
    float draw_right = 0.0f;
    float right_gap = 0.0f;
    int first_ink_row = -1;
    int last_ink_row = -1;
    int pixel_height = 0;
    std::uint64_t top_alpha = 0;
    std::uint64_t bottom_alpha = 0;
};

struct ScissorBatchDebug {
    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;
    bool pending_text_runs = false;
    std::size_t text_count = 0;
};

struct ScissorDecodeDebug {
    bool ok = false;
    std::vector<ScissorBatchDebug> batches;
    std::vector<std::uint32_t> text_run_batches;
};

inline Utf8Range utf16_range_to_utf8(std::string const&,
                                     unsigned long,
                                     unsigned long) {
    return {};
}

inline unsigned long utf8_prefix_to_utf16(std::string const&, std::size_t) {
    return 0;
}

inline float normalize_scroll_delta(double, bool, float) {
    return 0.0f;
}

inline CompositionVisualDebug build_visual_text(std::string const&,
                                                std::size_t,
                                                std::size_t,
                                                std::string const&,
                                                unsigned long) {
    return {};
}

inline void force_disable_system_caret(bool) {}
inline void set_scroll_tracking_for_tests(bool) {}
inline void reset_image_cache_for_tests() {}
inline void set_image_queue_only_for_tests(bool) {}

inline RemoteImageDebug remote_image_debug(std::string const&) {
    return {};
}

inline RasterizedTextRunDebug rasterized_text_run_debug(
        std::string const&,
        float,
        bool,
        float,
        float) {
    return {};
}

inline ScissorDecodeDebug decode_scissor_batches_debug(
        std::vector<unsigned char> const&) {
    return {};
}

inline SelectedRangeDebug selected_range_debug() {
    return {};
}

inline ::phenotype::diag::RectSnapshot first_rect_for_range_debug(
        unsigned long,
        unsigned long) {
    return {};
}

inline void invoke_command_for_tests(char const*) {}

inline bool perform_key_equivalent_for_tests(unsigned long long, char const*, unsigned short = 0) {
    return false;
}

inline SystemCaretDebug system_caret_debug() {
    return {};
}

inline void set_composition_for_tests(std::string,
                                      std::size_t,
                                      std::size_t,
                                      unsigned long) {}

inline void clear_composition_for_tests() {}
#endif
} // namespace phenotype::native::macos_test

#ifdef __APPLE__
namespace phenotype::native::detail {

inline ::phenotype::diag::PlatformCapabilitiesSnapshot macos_debug_capabilities() {
    auto accessibility = accessibility_display_options();
    bool const material_pipeline_ready = g_renderer.material_pipeline != nullptr;
    bool const material_frame_history =
        g_renderer.material_backdrop_texture != nullptr
        && g_renderer.last_material_backdrop_available;
    ::phenotype::diag::PlatformCapabilitiesSnapshot snapshot{
        "macos",
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        material_pipeline_ready,
    };
    snapshot.reduce_transparency = accessibility.reduce_transparency;
    snapshot.increase_contrast = accessibility.increase_contrast;
    snapshot.reduce_motion = accessibility.reduce_motion;
    snapshot.material_shader_blur = material_pipeline_ready;
    snapshot.material_frame_history = material_frame_history;
    auto const material_capabilities = macos_material_capability_input(
        g_renderer.device,
        material_pipeline_ready,
        material_frame_history,
        accessibility);
    snapshot.material_max_shader_sample_taps =
        material_capabilities.max_shader_sample_taps;
    snapshot.material_max_texture_dimension_2d =
        material_capabilities.max_texture_dimension_2d;
    snapshot.material_max_backdrop_pixels =
        material_capabilities.max_backdrop_pixels;
    snapshot.material_capability_profile = material_capabilities.profile;
    snapshot.material_capability_source = material_capabilities.source;
    snapshot.system_settings = macos_system_settings_snapshot();
    return snapshot;
}

inline json::Array material_plans_runtime_json() {
    return ::phenotype::diag::detail::material_plans_runtime_json(
        g_renderer.scratch.material_records);
}

inline json::Object macos_metal_capabilities_json() {
    json::Object metal;
    auto* device = g_renderer.device;
    metal.emplace("source", json::Value{"MTLDevice.supportsFamily"});
    metal.emplace(
        "reference",
        json::Value{"https://developer.apple.com/metal/capabilities/"});
    metal.emplace("device_present", json::Value{device != nullptr});
    metal.emplace(
        "supports_texture_sample_count_1",
        json::Value{device != nullptr && device->supportsTextureSampleCount(1)});
    metal.emplace(
        "supports_texture_sample_count_4",
        json::Value{device != nullptr && device->supportsTextureSampleCount(4)});
    metal.emplace(
        "material_max_shader_sample_taps",
        json::Value{static_cast<std::int64_t>(
            device != nullptr ? material_max_sample_taps : 0u)});
    metal.emplace(
        "material_max_texture_dimension_2d",
        json::Value{macos_material_max_texture_dimension_2d(device)});
    metal.emplace(
        "material_max_backdrop_pixels",
        json::Value{device != nullptr
            ? k_macos_material_max_backdrop_pixels
            : std::int64_t{0}});
    metal.emplace(
        "material_capability_profile",
        json::Value{device != nullptr
            ? "macos-metal-liquid-glass"
            : "macos-metal-unavailable"});
    metal.emplace(
        "supports_family_apple7",
        json::Value{device != nullptr && device->supportsFamily(MTL::GPUFamilyApple7)});
    metal.emplace(
        "supports_family_apple8",
        json::Value{device != nullptr && device->supportsFamily(MTL::GPUFamilyApple8)});
    metal.emplace(
        "supports_family_apple9",
        json::Value{device != nullptr && device->supportsFamily(MTL::GPUFamilyApple9)});
    metal.emplace(
        "supports_family_mac2",
        json::Value{device != nullptr && device->supportsFamily(MTL::GPUFamilyMac2)});
    metal.emplace(
        "supports_family_common3",
        json::Value{device != nullptr && device->supportsFamily(MTL::GPUFamilyCommon3)});
    metal.emplace(
        "supports_family_metal3",
        json::Value{device != nullptr && device->supportsFamily(MTL::GPUFamilyMetal3)});
    return metal;
}

inline json::Object macos_renderer_runtime_json() {
    json::Object renderer;
    renderer.emplace("initialized", json::Value{g_renderer.initialized});
    renderer.emplace(
        "metal_capabilities",
        json::Value{macos_metal_capabilities_json()});
    renderer.emplace(
        "drawable_width",
        json::Value{static_cast<std::int64_t>(g_renderer.drawable_width)});
    renderer.emplace(
        "drawable_height",
        json::Value{static_cast<std::int64_t>(g_renderer.drawable_height)});
    renderer.emplace(
        "last_render_width",
        json::Value{static_cast<std::int64_t>(g_renderer.last_render_width)});
    renderer.emplace(
        "last_render_height",
        json::Value{static_cast<std::int64_t>(g_renderer.last_render_height)});
    renderer.emplace(
        "last_frame_available",
        json::Value{g_renderer.last_frame_available});
    renderer.emplace(
        "clear_alpha",
        json::Value{g_renderer.last_clear_alpha});
    renderer.emplace(
        "clear_alpha_for_transparent_window",
        json::Value{g_renderer.last_clear_alpha_for_transparent_window});
    renderer.emplace(
        "full_frame_opaque_fill_count",
        json::Value{
            static_cast<std::int64_t>(
                g_renderer.last_full_frame_opaque_fill_count)});
    renderer.emplace(
        "transparent_window_has_opaque_frame_fill",
        json::Value{
            g_renderer.last_transparent_window_has_opaque_frame_fill});
    renderer.emplace(
        "readback_texture_ready",
        json::Value{g_renderer.debug_capture_texture != nullptr});
    renderer.emplace(
        "material_pipeline_ready",
        json::Value{g_renderer.material_pipeline != nullptr});
    renderer.emplace(
        "material_backdrop_source_ready",
        json::Value{
            g_renderer.material_backdrop_texture != nullptr
            && g_renderer.last_material_backdrop_available});
    renderer.emplace(
        "material_backdrop_excludes_foreground_text",
        json::Value{
            g_renderer.material_backdrop_texture != nullptr
            && g_renderer.last_material_backdrop_available
            && g_renderer.last_material_backdrop_excludes_foreground_text});
    json::Object luma_descriptor;
    luma_descriptor.emplace(
        "available",
        json::Value{g_renderer.last_material_backdrop_luma_available});
    luma_descriptor.emplace(
        "color_available",
        json::Value{g_renderer.last_material_backdrop_color_available});
    json::Object color_mean;
    color_mean.emplace(
        "r",
        json::Value{static_cast<std::int64_t>(
            g_renderer.last_material_backdrop_color_mean.r)});
    color_mean.emplace(
        "g",
        json::Value{static_cast<std::int64_t>(
            g_renderer.last_material_backdrop_color_mean.g)});
    color_mean.emplace(
        "b",
        json::Value{static_cast<std::int64_t>(
            g_renderer.last_material_backdrop_color_mean.b)});
    color_mean.emplace(
        "a",
        json::Value{static_cast<std::int64_t>(
            g_renderer.last_material_backdrop_color_mean.a)});
    luma_descriptor.emplace(
        "color_mean",
        json::Value{std::move(color_mean)});
    luma_descriptor.emplace(
        "luma_min",
        json::Value{g_renderer.last_material_backdrop_luma_min});
    luma_descriptor.emplace(
        "luma_max",
        json::Value{g_renderer.last_material_backdrop_luma_max});
    luma_descriptor.emplace(
        "luma_mean",
        json::Value{g_renderer.last_material_backdrop_luma_mean});
    luma_descriptor.emplace(
        "sample_count",
        json::Value{static_cast<std::int64_t>(
            g_renderer.last_material_backdrop_luma_sample_count)});
    luma_descriptor.emplace(
        "sample_grid_width",
        json::Value{static_cast<std::int64_t>(
            g_renderer.last_material_backdrop_luma_grid_width)});
    luma_descriptor.emplace(
        "sample_grid_height",
        json::Value{static_cast<std::int64_t>(
            g_renderer.last_material_backdrop_luma_grid_height)});
    luma_descriptor.emplace(
        "sample_frame",
        json::Value{static_cast<std::int64_t>(
            g_renderer.last_material_backdrop_luma_frame)});
    luma_descriptor.emplace(
        "status",
        json::Value{g_renderer.last_material_backdrop_luma_status
                        ? g_renderer.last_material_backdrop_luma_status
                        : "not-sampled"});
    luma_descriptor.emplace(
        "pending",
        json::Value{
            g_renderer.material_backdrop_luma_pending_command_buffer != nullptr});
    luma_descriptor.emplace(
        "skipped_sample_count",
        json::Value{static_cast<std::int64_t>(
            g_renderer.material_backdrop_luma_skipped_sample_count)});
    renderer.emplace(
        "material_backdrop_luma_descriptor",
        json::Value{std::move(luma_descriptor)});
    renderer.emplace(
        "readback_buffer_ready",
        json::Value{g_renderer.frame_readback_buf != nullptr});
    json::Object accessibility;
    accessibility.emplace(
        "source",
        json::Value{g_renderer.accessibility_options.source});
    accessibility.emplace(
        "reduce_transparency",
        json::Value{g_renderer.accessibility_options.reduce_transparency});
    accessibility.emplace(
        "increase_contrast",
        json::Value{g_renderer.accessibility_options.increase_contrast});
    accessibility.emplace(
        "reduce_motion",
        json::Value{g_renderer.accessibility_options.reduce_motion});
    renderer.emplace(
        "accessibility_display_options",
        json::Value{std::move(accessibility)});
    renderer.emplace(
        "system_settings",
        ::phenotype::diag::system_settings_to_json(
            macos_system_settings_snapshot()));
    renderer.emplace(
        "material_plan_contract_version",
        json::Value{
            static_cast<std::int64_t>(material_plan_contract_version)});
    renderer.emplace(
        "material_plan_count",
        json::Value{
            static_cast<std::int64_t>(
                g_renderer.scratch.material_records.size())});
    renderer.emplace(
        "material_plans",
        json::Value{material_plans_runtime_json()});
    renderer.emplace(
        "material_container_groups",
        json::Value{
            ::phenotype::diag::detail::material_container_group_details_json(
                g_renderer.scratch.material_records)});
    renderer.emplace(
        "material_runtime_summary",
        ::phenotype::diag::detail::material_runtime_summary_json(
            g_renderer.scratch.material_records));
    renderer.emplace(
        "material_executor_summary",
        ::phenotype::diag::detail::material_executor_summary_json(
            g_renderer.material_executor_summary));
    return renderer;
}

inline json::Object macos_images_runtime_json() {
    std::size_t pending_jobs = 0;
    std::size_t completed_jobs = 0;
    bool worker_started = false;
    {
        std::lock_guard lock(g_images.mutex);
        pending_jobs = g_images.pending_jobs.size();
        completed_jobs = g_images.completed_jobs.size();
        worker_started = g_images.worker_started;
    }

    json::Array remote_entries;
    for (auto const& record : g_images.cache) {
        if (!is_http_url(record.first))
            continue;
        json::Object entry;
        entry.emplace("url", json::Value{record.first});
        entry.emplace(
            "state",
            json::Value{image_entry_state_name(record.second.state)});
        entry.emplace(
            "failure_reason",
            json::Value{record.second.failure_reason});
        remote_entries.push_back(json::Value{std::move(entry)});
    }

    json::Object images;
    images.emplace(
        "pending_queue_count",
        json::Value{static_cast<std::int64_t>(pending_jobs)});
    images.emplace(
        "completed_queue_count",
        json::Value{static_cast<std::int64_t>(completed_jobs)});
    images.emplace("worker_started", json::Value{worker_started});
    images.emplace(
        "remote_entry_count",
        json::Value{static_cast<std::int64_t>(remote_entries.size())});
    images.emplace("remote_entries", json::Value{std::move(remote_entries)});
    return images;
}

inline json::Object macos_text_input_runtime_json() {
    auto caret = ::phenotype::native::macos_test::system_caret_debug();
    auto input_debug = ::phenotype::diag::input_debug_snapshot();
    auto composition_active = g_ime.composition_active && !g_ime.marked_text.empty();

    json::Object system_caret;
    system_caret.emplace("supported", json::Value{caret.supported});
    system_caret.emplace("attached", json::Value{caret.attached});
    system_caret.emplace(
        "display_mode",
        json::Value{static_cast<std::int64_t>(caret.display_mode)});
    system_caret.emplace(
        "automatic_mode_options",
        json::Value{static_cast<std::int64_t>(caret.automatic_mode_options)});
    system_caret.emplace(
        "scroll_tracking_active",
        json::Value{caret.scroll_tracking_active});
    system_caret.emplace("host_flipped", json::Value{caret.host_flipped});
    system_caret.emplace("frame", ::phenotype::diag::rect_to_json(caret.frame));
    system_caret.emplace("draw_rect", ::phenotype::diag::rect_to_json(caret.draw_rect));
    system_caret.emplace("host_rect", ::phenotype::diag::rect_to_json(caret.host_rect));
    system_caret.emplace(
        "screen_rect",
        ::phenotype::diag::rect_to_json(caret.screen_rect));
    system_caret.emplace(
        "host_bounds",
        ::phenotype::diag::rect_to_json(caret.host_bounds));
    system_caret.emplace(
        "first_rect_screen",
        ::phenotype::diag::rect_to_json(caret.first_rect_screen));

    json::Object composition;
    composition.emplace("active", json::Value{composition_active});
    composition.emplace("text", json::Value{input_debug.composition_text});
    composition.emplace(
        "cursor_bytes",
        json::Value{static_cast<std::int64_t>(
            composition_active ? composition_cursor_bytes() : 0)});
    composition.emplace(
        "anchor",
        json::Value{static_cast<std::int64_t>(g_ime.composition_anchor)});
    composition.emplace(
        "replacement_start",
        json::Value{static_cast<std::int64_t>(g_ime.replacement_start)});
    composition.emplace(
        "replacement_end",
        json::Value{static_cast<std::int64_t>(g_ime.replacement_end)});
    composition.emplace(
        "marked_selection_location_utf16",
        json::Value{static_cast<std::int64_t>(g_ime.marked_selection.location)});
    composition.emplace(
        "marked_selection_length_utf16",
        json::Value{static_cast<std::int64_t>(g_ime.marked_selection.length)});

    json::Object text_input;
    text_input.emplace(
        "scroll_tracking_active",
        json::Value{g_ime.scroll_tracking_active});
    text_input.emplace(
        "caret_renderer",
        json::Value{input_debug.caret_renderer});
    text_input.emplace(
        "composition_active",
        json::Value{input_debug.composition_active});
    text_input.emplace("system_caret", json::Value{std::move(system_caret)});
    text_input.emplace("composition", json::Value{std::move(composition)});
    return text_input;
}

inline json::Object macos_input_runtime_json() {
    auto const& scroll = g_ime.last_scroll_event;
    json::Object scroll_json;
    scroll_json.emplace("available", json::Value{scroll.available});
    scroll_json.emplace("source", json::Value{std::string{scroll.source}});
    scroll_json.emplace(
        "has_precise_scrolling_deltas",
        json::Value{scroll.has_precise_scrolling_deltas});
    scroll_json.emplace("raw_delta_x", json::Value{scroll.raw_delta_x});
    scroll_json.emplace("raw_delta_y", json::Value{scroll.raw_delta_y});
    scroll_json.emplace("line_height", json::Value{scroll.line_height});
    scroll_json.emplace(
        "app_vertical_multiplier",
        json::Value{scroll.app_vertical_multiplier});
    scroll_json.emplace(
        "app_horizontal_multiplier",
        json::Value{scroll.app_horizontal_multiplier});
    scroll_json.emplace(
        "normalized_delta_x",
        json::Value{scroll.normalized_delta_x});
    scroll_json.emplace(
        "normalized_delta_y",
        json::Value{scroll.normalized_delta_y});
    scroll_json.emplace("viewport_width", json::Value{scroll.viewport_width});
    scroll_json.emplace("viewport_height", json::Value{scroll.viewport_height});
    scroll_json.emplace(
        "phase",
        json::Value{static_cast<std::int64_t>(scroll.phase)});
    scroll_json.emplace(
        "momentum_phase",
        json::Value{static_cast<std::int64_t>(scroll.momentum_phase)});
    scroll_json.emplace(
        "scroll_tracking_changed",
        json::Value{scroll.scroll_tracking_changed});
    scroll_json.emplace("handled_x", json::Value{scroll.handled_x});
    scroll_json.emplace("handled_y", json::Value{scroll.handled_y});

    json::Object input;
    input.emplace(
        "scroll_contract",
        json::Value{
            "NSEvent scrollingDelta values are already OS-adjusted; phenotype "
            "applies explicit theme multipliers at the backend edge"});
    input.emplace("scroll", json::Value{std::move(scroll_json)});
    return input;
}

struct WindowServerSnapshot {
    bool valid = false;
    bool onscreen = false;
    bool frontmost_app_window = false;
    bool occluded_by_front_app_window = false;
    std::int64_t window_number = 0;
    int window_order_index = -1;
    int app_window_order_index = -1;
    double x = 0.0;
    double y = 0.0;
    double w = 0.0;
    double h = 0.0;
};

struct StandardWindowButtonSnapshot {
    std::string_view name;
    bool present = false;
    bool hidden = false;
    bool visible = false;
    bool within_leading_reserve = false;
    ::phenotype::diag::RectSnapshot frame{};
    ::phenotype::diag::RectSnapshot content_frame{};
    ::phenotype::diag::RectSnapshot top_left_content_frame{};
};

inline std::optional<std::int64_t> cf_number_to_i64(CFTypeRef value) {
    if (!value || CFGetTypeID(value) != CFNumberGetTypeID())
        return std::nullopt;
    std::int64_t out = 0;
    if (!CFNumberGetValue(static_cast<CFNumberRef>(value),
                          kCFNumberSInt64Type,
                          &out))
        return std::nullopt;
    return out;
}

inline bool window_rects_intersect(CGRect const& a, CGRect const& b) {
    return CGRectIntersectsRect(a, b) != 0;
}

inline std::optional<CGRect> window_bounds_from_info(CFDictionaryRef info) {
    if (!info || CFGetTypeID(info) != CFDictionaryGetTypeID())
        return std::nullopt;
    auto raw_bounds = CFDictionaryGetValue(info, kCGWindowBounds);
    if (!raw_bounds || CFGetTypeID(raw_bounds) != CFDictionaryGetTypeID())
        return std::nullopt;

    CGRect rect{};
    if (!CGRectMakeWithDictionaryRepresentation(
            static_cast<CFDictionaryRef>(raw_bounds),
            &rect))
        return std::nullopt;
    return rect;
}

inline CGRect rect_to_top_left_space(CGRect rect, CGRect bounds) {
    if (CGRectIsNull(rect) || CGRectIsNull(bounds))
        return CGRectNull;
    CGRect out = rect;
    out.origin.y = bounds.size.height - rect.origin.y - rect.size.height;
    return out;
}

inline bool rect_snapshot_inside_top_left_reserve(
        ::phenotype::diag::RectSnapshot const& rect,
        IntegratedTitlebarOptions const& options) {
    if (!rect.valid)
        return false;
    constexpr float tolerance = 0.5f;
    float const max_x = rect.x + rect.w;
    float const max_y = rect.y + rect.h;
    return rect.x >= -tolerance
        && rect.y >= -tolerance
        && max_x <= options.leading_control_reserved_width + tolerance
        && max_y <= options.height + tolerance;
}

inline StandardWindowButtonSnapshot standard_window_button_snapshot(
        id ns_window,
        id content_view,
        std::string_view name,
        long button_kind,
        IntegratedTitlebarOptions const& titlebar_options) {
    auto snapshot = StandardWindowButtonSnapshot{.name = name};
    if (!ns_window || !objc_responds_to(ns_window, sel_standard_window_button()))
        return snapshot;

    id button = objc_send<id>(
        ns_window,
        sel_standard_window_button(),
        button_kind);
    if (!button)
        return snapshot;

    snapshot.present = true;
    snapshot.hidden = objc_responds_to(button, sel_is_hidden())
        && objc_send<bool>(button, sel_is_hidden());
    snapshot.visible = !snapshot.hidden;

    CGRect frame = objc_responds_to(button, sel_frame())
        ? objc_send<CGRect>(button, sel_frame())
        : CGRectNull;
    snapshot.frame = rect_snapshot(frame);

    id superview = objc_responds_to(button, sel_superview())
        ? objc_send<id>(button, sel_superview())
        : nullptr;
    CGRect content_frame = CGRectNull;
    if (!CGRectIsNull(frame) && superview && content_view
        && objc_responds_to(superview, sel_convert_rect_to_view())) {
        content_frame = objc_send<CGRect>(
            superview,
            sel_convert_rect_to_view(),
            frame,
            content_view);
    } else {
        content_frame = frame;
    }
    snapshot.content_frame = rect_snapshot(content_frame);

    CGRect bounds = content_view && objc_responds_to(content_view, sel_bounds())
        ? objc_send<CGRect>(content_view, sel_bounds())
        : CGRectNull;
    snapshot.top_left_content_frame =
        rect_snapshot(rect_to_top_left_space(content_frame, bounds));
    snapshot.within_leading_reserve =
        snapshot.visible
        && rect_snapshot_inside_top_left_reserve(
            snapshot.top_left_content_frame,
            titlebar_options);
    return snapshot;
}

inline json::Value standard_window_button_json(
        StandardWindowButtonSnapshot const& button) {
    json::Object out;
    out.emplace("name", json::Value{std::string(button.name)});
    out.emplace("present", json::Value{button.present});
    out.emplace("hidden", json::Value{button.hidden});
    out.emplace("visible", json::Value{button.visible});
    out.emplace(
        "within_leading_reserve",
        json::Value{button.within_leading_reserve});
    out.emplace("frame", ::phenotype::diag::rect_to_json(button.frame));
    out.emplace(
        "content_frame",
        ::phenotype::diag::rect_to_json(button.content_frame));
    out.emplace(
        "top_left_content_frame",
        ::phenotype::diag::rect_to_json(button.top_left_content_frame));
    return json::Value{std::move(out)};
}

inline json::Value native_window_controls_runtime_json(
        id ns_window,
        id content_view,
        IntegratedTitlebarOptions const& titlebar_options,
        bool full_size_content_view,
        bool titlebar_transparent,
        bool title_hidden) {
    auto const buttons = std::array{
        standard_window_button_snapshot(
            ns_window,
            content_view,
            "close",
            0,
            titlebar_options),
        standard_window_button_snapshot(
            ns_window,
            content_view,
            "minimize",
            1,
            titlebar_options),
        standard_window_button_snapshot(
            ns_window,
            content_view,
            "zoom",
            2,
            titlebar_options),
    };

    int present_count = 0;
    int visible_count = 0;
    int hidden_count = 0;
    int within_reserve_count = 0;
    json::Array button_json;
    for (auto const& button : buttons) {
        if (button.present)
            ++present_count;
        if (button.visible)
            ++visible_count;
        if (button.hidden)
            ++hidden_count;
        if (button.within_leading_reserve)
            ++within_reserve_count;
        button_json.push_back(standard_window_button_json(button));
    }

    bool const expected_buttons_visible = present_count == 3
        && visible_count == 3
        && hidden_count == 0;
    bool const all_buttons_within_reserve = within_reserve_count == 3;
    bool const integrated = full_size_content_view
        && titlebar_transparent
        && title_hidden
        && expected_buttons_visible
        && all_buttons_within_reserve;

    json::Object controls;
    controls.emplace(
        "ownership_policy",
        json::Value{"platform_edge_standard_buttons_only"});
    controls.emplace(
        "integration_policy",
        json::Value{"standard_buttons_inside_leading_content_reserve"});
    controls.emplace("expected_count", json::Value{std::int64_t{3}});
    controls.emplace(
        "present_count",
        json::Value{static_cast<std::int64_t>(present_count)});
    controls.emplace(
        "visible_count",
        json::Value{static_cast<std::int64_t>(visible_count)});
    controls.emplace(
        "hidden_count",
        json::Value{static_cast<std::int64_t>(hidden_count)});
    controls.emplace(
        "within_leading_reserve_count",
        json::Value{static_cast<std::int64_t>(within_reserve_count)});
    controls.emplace(
        "all_buttons_within_leading_reserve",
        json::Value{all_buttons_within_reserve});
    controls.emplace(
        "integrated_in_content_area",
        json::Value{integrated});
    controls.emplace("duplicate_window_controls", json::Value{false});
    controls.emplace("window_control_single_owner", json::Value{integrated});
    controls.emplace(
        "window_control_duplication_guard",
        json::Value{
            integrated ? "native_window_controls_single_owner"
                       : "native_window_controls_not_integrated"});
    controls.emplace(
        "content_drawn_window_control_count",
        json::Value{std::int64_t{0}});
    controls.emplace(
        "artifact_drawn_window_control_count",
        json::Value{std::int64_t{0}});
    controls.emplace("buttons", json::Value{std::move(button_json)});
    return json::Value{std::move(controls)};
}

inline WindowServerSnapshot window_server_snapshot(id ns_window) {
    WindowServerSnapshot snapshot;
    if (!ns_window)
        return snapshot;

    auto number = objc_send<int>(ns_window, sel_window_number());
    if (number <= 0)
        return snapshot;
    snapshot.window_number = number;

    CFGuard<CFArrayRef> windows{
        CGWindowListCopyWindowInfo(
            kCGWindowListOptionIncludingWindow,
            static_cast<CGWindowID>(number))};
    if (!windows || CFArrayGetCount(windows) <= 0)
        return snapshot;

    auto info = static_cast<CFDictionaryRef>(CFArrayGetValueAtIndex(windows, 0));
    if (!info || CFGetTypeID(info) != CFDictionaryGetTypeID())
        return snapshot;

    if (auto raw_onscreen = CFDictionaryGetValue(info, kCGWindowIsOnscreen)) {
        if (CFGetTypeID(raw_onscreen) == CFBooleanGetTypeID())
            snapshot.onscreen = CFBooleanGetValue(static_cast<CFBooleanRef>(raw_onscreen));
    }

    auto target_bounds = window_bounds_from_info(info);
    if (!target_bounds)
        return snapshot;

    snapshot.valid = true;
    snapshot.x = target_bounds->origin.x;
    snapshot.y = target_bounds->origin.y;
    snapshot.w = target_bounds->size.width;
    snapshot.h = target_bounds->size.height;

    CFGuard<CFArrayRef> ordered_windows{
        CGWindowListCopyWindowInfo(
            kCGWindowListOptionOnScreenOnly,
            kCGNullWindowID)};
    if (!ordered_windows)
        return snapshot;

    int app_order = 0;
    for (CFIndex i = 0; i < CFArrayGetCount(ordered_windows); ++i) {
        auto ordered_info = static_cast<CFDictionaryRef>(
            CFArrayGetValueAtIndex(ordered_windows, i));
        if (!ordered_info || CFGetTypeID(ordered_info) != CFDictionaryGetTypeID())
            continue;

        auto layer = cf_number_to_i64(
            CFDictionaryGetValue(ordered_info, kCGWindowLayer));
        auto ordered_number = cf_number_to_i64(
            CFDictionaryGetValue(ordered_info, kCGWindowNumber));
        bool const app_window = layer && *layer == 0;
        bool const target = ordered_number
            && *ordered_number == snapshot.window_number;

        if (target) {
            snapshot.window_order_index = static_cast<int>(i);
            if (app_window) {
                snapshot.app_window_order_index = app_order;
                snapshot.frontmost_app_window = app_order == 0;
            }
            break;
        }

        if (app_window) {
            if (auto bounds = window_bounds_from_info(ordered_info)) {
                if (window_rects_intersect(*bounds, *target_bounds))
                    snapshot.occluded_by_front_app_window = true;
            }
            ++app_order;
        }
    }
    return snapshot;
}

inline char const* visual_effect_material_name(long value) noexcept {
    switch (value) {
    case 3: return "titlebar";
    case 4: return "selection";
    case 5: return "menu";
    case 6: return "popover";
    case 7: return "sidebar";
    case 10: return "header-view";
    case 11: return "sheet";
    case 12: return "window-background";
    case 13: return "hud-window";
    case 15: return "fullscreen-ui";
    case 17: return "tooltip";
    case 18: return "content-background";
    case 21: return "under-window-background";
    case 22: return "under-page-background";
    case 0: return "appearance-based";
    case 1: return "light";
    case 2: return "dark";
    case 8: return "medium-light";
    case 9: return "ultra-dark";
    default: return "unknown";
    }
}

inline char const* visual_effect_blending_mode_name(long value) noexcept {
    switch (value) {
    case 0: return "behind-window";
    case 1: return "within-window";
    default: return "unknown";
    }
}

inline char const* visual_effect_state_name(long value) noexcept {
    switch (value) {
    case 0: return "follows-window-active-state";
    case 1: return "active";
    case 2: return "inactive";
    default: return "unknown";
    }
}

inline json::Object macos_window_runtime_json() {
    auto const* surface = g_renderer.surface;
    auto ns_window = g_renderer.ns_window;
    auto ns_app = objc_send<id>(
        class_as_id(objc_getClass("NSApplication")),
        sel_shared_application());
    id content_view = g_renderer.content_view
        ? g_renderer.content_view
        : (ns_window ? objc_send<id>(ns_window, sel_content_view()) : nullptr);
    bool const has_options = surface && surface->window_options_valid;
    WindowChromeStyle const chrome =
        has_options ? surface->window_chrome : WindowChromeStyle::System;
    IntegratedTitlebarOptions const titlebar_options =
        has_options ? surface->integrated_titlebar : IntegratedTitlebarOptions{};
    NativeBackdropMaterial const expected_backdrop_material =
        has_options
            ? surface->native_backdrop_material
            : NativeBackdropMaterial::UnderWindowBackground;
    float const expected_backdrop_opacity =
        has_options
            ? sanitize_native_backdrop_opacity(surface->native_backdrop_opacity)
            : 1.0f;
    long const expected_native_backdrop_material =
        ns_visual_effect_material_value(expected_backdrop_material);
    constexpr unsigned long move_to_active_space = 1ul << 1;
    constexpr unsigned long full_size_content_view_mask = 1ul << 15;
    constexpr long hidden_title = 1;
    unsigned long const collection_behavior = ns_window
        ? objc_send<unsigned long>(ns_window, sel_collection_behavior())
        : 0ul;
    unsigned long const style_mask = ns_window
        ? objc_send<unsigned long>(ns_window, sel_style_mask())
        : 0ul;
    bool const full_size_content_view =
        (style_mask & full_size_content_view_mask) != 0;
    bool const titlebar_transparent = ns_window
        && objc_send<bool>(ns_window, sel_titlebar_appears_transparent());
    long const title_visibility = ns_window
        ? objc_send<long>(ns_window, sel_title_visibility())
        : 0;
    bool const background_drag_enabled = ns_window
        && objc_send<bool>(ns_window, sel_is_movable_by_window_background());
    bool const window_opaque = ns_window
        && objc_send<bool>(ns_window, sel_is_opaque());
    auto const background_color = ns_window
        ? objc_send<id>(ns_window, sel_background_color())
        : nullptr;
    auto const background_rgba = nscolor_to_srgb_color(background_color);
    std::int64_t const window_background_alpha =
        static_cast<std::int64_t>(
            background_rgba.has_value() ? background_rgba->a : 255);
    bool const window_background_clear =
        background_rgba.has_value() && background_rgba->a == 0;
    bool const metal_layer_opaque = g_renderer.layer
        && objc_send<bool>(
            reinterpret_cast<id>(g_renderer.layer),
            sel_is_opaque());
    id window_content_view = ns_window
        ? objc_send<id>(ns_window, sel_content_view())
        : nullptr;
    id content_superview = content_view
        && objc_responds_to(content_view, sel_superview())
        ? objc_send<id>(content_view, sel_superview())
        : nullptr;
    id native_backdrop_view = nullptr;
    char const* native_backdrop_underlay_placement = "none";
    if (is_visual_effect_view(window_content_view)
        && content_superview == window_content_view) {
        native_backdrop_view = window_content_view;
        native_backdrop_underlay_placement = "parent-content-view";
    } else if (content_superview && content_superview == window_content_view) {
        native_backdrop_view = find_visual_effect_subview(content_superview);
        if (native_backdrop_view)
            native_backdrop_underlay_placement = "sibling-underlay";
    }
    bool const native_backdrop_underlay_enabled = native_backdrop_view != nullptr;
    long const native_backdrop_material =
        native_backdrop_underlay_enabled
        && objc_responds_to(native_backdrop_view, sel_registerName("material"))
            ? objc_send<long>(native_backdrop_view, sel_registerName("material"))
            : -1;
    long const native_backdrop_blending_mode =
        native_backdrop_underlay_enabled
        && objc_responds_to(
            native_backdrop_view,
            sel_registerName("blendingMode"))
            ? objc_send<long>(
                native_backdrop_view,
                sel_registerName("blendingMode"))
            : -1;
    long const native_backdrop_state =
        native_backdrop_underlay_enabled
        && objc_responds_to(native_backdrop_view, sel_registerName("state"))
            ? objc_send<long>(native_backdrop_view, sel_registerName("state"))
            : -1;
    bool const native_backdrop_emphasized =
        native_backdrop_underlay_enabled
        && objc_responds_to(native_backdrop_view, sel_registerName("isEmphasized"))
        && objc_send<signed char>(
            native_backdrop_view,
            sel_registerName("isEmphasized")) != 0;
    double const native_backdrop_alpha =
        native_backdrop_underlay_enabled
        && objc_responds_to(native_backdrop_view, sel_registerName("alphaValue"))
            ? objc_send<double>(
                native_backdrop_view,
                sel_registerName("alphaValue"))
            : 1.0;
    double const native_backdrop_alpha_delta =
        native_backdrop_alpha - static_cast<double>(expected_backdrop_opacity);
    double const native_backdrop_abs_alpha_delta =
        native_backdrop_alpha_delta < 0.0
            ? -native_backdrop_alpha_delta
            : native_backdrop_alpha_delta;
    bool const native_backdrop_underlay_material_matches =
        native_backdrop_material == expected_native_backdrop_material;
    bool const native_backdrop_underlay_alpha_matches =
        native_backdrop_abs_alpha_delta <= 0.01;
    bool const native_backdrop_underlay_sibling =
        std::string_view{native_backdrop_underlay_placement}
            == "sibling-underlay";
    bool const native_backdrop_underlay_blending_behind_window =
        native_backdrop_blending_mode == 0;
    bool const native_backdrop_underlay_active =
        native_backdrop_state == 1;
    bool const renderer_clear_alpha_zero =
        g_renderer.last_clear_alpha == 0.0;
    bool const renderer_clear_for_transparent_window =
        g_renderer.last_clear_alpha_for_transparent_window;
    bool const renderer_has_full_frame_opaque_fill =
        g_renderer.last_full_frame_opaque_fill_count != 0
        || g_renderer.last_transparent_window_has_opaque_frame_fill;
    auto const backdrop_composition_plan =
        plan_native_backdrop_composition(NativeBackdropCompositionInput{
            .chrome = chrome,
            .expected_material = expected_backdrop_material,
            .expected_alpha = expected_backdrop_opacity,
            .window_opaque = window_opaque,
            .window_background_clear = window_background_clear,
            .window_background_alpha = static_cast<int>(window_background_alpha),
            .metal_layer_opaque = metal_layer_opaque,
            .underlay_enabled = native_backdrop_underlay_enabled,
            .underlay_material_matches =
                native_backdrop_underlay_material_matches,
            .underlay_alpha_matches = native_backdrop_underlay_alpha_matches,
            .underlay_sibling = native_backdrop_underlay_sibling,
            .underlay_blending_behind_window =
                native_backdrop_underlay_blending_behind_window,
            .underlay_active = native_backdrop_underlay_active,
            .renderer_clear_alpha_zero = renderer_clear_alpha_zero,
            .renderer_clear_for_transparent_window =
                renderer_clear_for_transparent_window,
            .renderer_has_full_frame_opaque_fill =
                renderer_has_full_frame_opaque_fill,
        });
    bool const app_active = ns_app
        && objc_send<bool>(ns_app, sel_is_active());
    bool const window_visible = ns_window
        && objc_send<bool>(ns_window, sel_is_visible());
    bool const window_key = ns_window
        && objc_send<bool>(ns_window, sel_is_key_window());
    bool const window_main = ns_window
        && objc_send<bool>(ns_window, sel_is_main_window());

    json::Object titlebar;
    titlebar.emplace(
        "height",
        json::Value{static_cast<double>(titlebar_options.height)});
    titlebar.emplace(
        "drag_region_height",
        json::Value{
            static_cast<double>(
                titlebar_options.drag_region_height)});
    titlebar.emplace(
        "leading_control_reserved_width",
        json::Value{
            static_cast<double>(
                titlebar_options.leading_control_reserved_width)});
    titlebar.emplace(
        "trailing_control_reserved_width",
        json::Value{
            static_cast<double>(
                titlebar_options.trailing_control_reserved_width)});

    json::Object window;
    window.emplace(
        "surface_kind",
        json::Value{
            surface ? native_surface_kind_name(surface->kind) : "unknown"});
    window.emplace("window_options_present", json::Value{has_options});
    window.emplace(
        "chrome",
        json::Value{window_chrome_style_name(chrome)});
    window.emplace(
        "integrated_titlebar",
        json::Value{std::move(titlebar)});
    window.emplace("native_controls_owned_by_os", json::Value{true});
    window.emplace("toolkit_window_shim", json::Value{false});
    window.emplace("uses_glfw", json::Value{false});
    window.emplace("style_mask", json::Value{static_cast<double>(style_mask)});
    window.emplace("titlebar_transparent", json::Value{titlebar_transparent});
    window.emplace("full_size_content_view", json::Value{full_size_content_view});
    window.emplace("title_hidden", json::Value{title_visibility == hidden_title});
    window.emplace("background_drag_enabled", json::Value{background_drag_enabled});
    window.emplace("window_opaque", json::Value{window_opaque});
    window.emplace(
        "window_background_clear",
        json::Value{window_background_clear});
    window.emplace(
        "window_background_alpha",
        json::Value{window_background_alpha});
    window.emplace("metal_layer_opaque", json::Value{metal_layer_opaque});
    window.emplace(
        "native_backdrop_underlay_enabled",
        json::Value{native_backdrop_underlay_enabled});
    window.emplace(
        "native_backdrop_underlay_kind",
        json::Value{
            native_backdrop_underlay_enabled
                ? "nsvisualeffectview"
                : "none"});
    window.emplace(
        "native_backdrop_underlay_placement",
        json::Value{native_backdrop_underlay_placement});
    window.emplace(
        "native_backdrop_underlay_material",
        json::Value{visual_effect_material_name(native_backdrop_material)});
    window.emplace(
        "native_backdrop_expected_material",
        json::Value{native_backdrop_material_name(expected_backdrop_material)});
    window.emplace(
        "native_backdrop_underlay_alpha",
        json::Value{native_backdrop_alpha});
    window.emplace(
        "native_backdrop_expected_alpha",
        json::Value{static_cast<double>(expected_backdrop_opacity)});
    window.emplace(
        "native_backdrop_underlay_blending_mode",
        json::Value{
            visual_effect_blending_mode_name(native_backdrop_blending_mode)});
    window.emplace(
        "native_backdrop_underlay_state",
        json::Value{visual_effect_state_name(native_backdrop_state)});
    window.emplace(
        "native_backdrop_underlay_emphasized",
        json::Value{native_backdrop_emphasized});
    json::Object backdrop_composition;
    backdrop_composition.emplace(
        "schema_version",
        json::Value{
            static_cast<int>(backdrop_composition_plan.schema_version)});
    backdrop_composition.emplace(
        "policy",
        json::Value{backdrop_composition_plan.policy});
    backdrop_composition.emplace(
        "native_backdrop_expected_material",
        json::Value{
            native_backdrop_material_name(
                backdrop_composition_plan.expected_material)});
    backdrop_composition.emplace(
        "native_backdrop_underlay_placement",
        json::Value{native_backdrop_underlay_placement});
    backdrop_composition.emplace(
        "native_backdrop_underlay_alpha",
        json::Value{native_backdrop_alpha});
    backdrop_composition.emplace(
        "native_backdrop_expected_alpha",
        json::Value{
            static_cast<double>(backdrop_composition_plan.expected_alpha)});
    backdrop_composition.emplace(
        "status",
        json::Value{backdrop_composition_plan.status});
    backdrop_composition.emplace(
        "ready",
        json::Value{backdrop_composition_plan.ready});
    backdrop_composition.emplace(
        "failure_reason",
        json::Value{backdrop_composition_plan.failure_reason});
    backdrop_composition.emplace(
        "likely_layer",
        json::Value{backdrop_composition_plan.likely_layer});
    backdrop_composition.emplace(
        "likely_pass",
        json::Value{backdrop_composition_plan.likely_pass});
    backdrop_composition.emplace(
        "requires_transparent_window",
        json::Value{backdrop_composition_plan.requires_transparent_window});
    backdrop_composition.emplace(
        "requires_clear_metal_layer",
        json::Value{backdrop_composition_plan.requires_clear_metal_layer});
    backdrop_composition.emplace(
        "requires_native_backdrop_underlay",
        json::Value{
            backdrop_composition_plan.requires_native_backdrop_underlay});
    backdrop_composition.emplace(
        "requires_sibling_underlay",
        json::Value{backdrop_composition_plan.requires_sibling_underlay});
    backdrop_composition.emplace(
        "requires_under_window_background_underlay",
        json::Value{
            backdrop_composition_plan
                .requires_under_window_background_underlay});
    backdrop_composition.emplace(
        "requires_alpha_zero_clear",
        json::Value{backdrop_composition_plan.requires_alpha_zero_clear});
    backdrop_composition.emplace(
        "requires_no_full_frame_opaque_fill",
        json::Value{
            backdrop_composition_plan.requires_no_full_frame_opaque_fill});
    backdrop_composition.emplace(
        "samples_external_backdrop",
        json::Value{backdrop_composition_plan.samples_external_backdrop});
    window.emplace(
        "glass_backdrop_composition",
        json::Value{std::move(backdrop_composition)});
    window.emplace(
        "native_window_controls",
        native_window_controls_runtime_json(
            ns_window,
            content_view,
            titlebar_options,
            full_size_content_view,
            titlebar_transparent,
            title_visibility == hidden_title));
    window.emplace("app_active", json::Value{app_active});
    window.emplace("window_visible", json::Value{window_visible});
    window.emplace("window_key", json::Value{window_key});
    window.emplace("window_main", json::Value{window_main});
    window.emplace(
        "collection_behavior",
        json::Value{static_cast<double>(collection_behavior)});
    window.emplace(
        "moves_to_active_space",
        json::Value{(collection_behavior & move_to_active_space) != 0});

    auto server = window_server_snapshot(ns_window);
    json::Object server_bounds;
    server_bounds.emplace("valid", json::Value{server.valid});
    server_bounds.emplace("x", json::Value{server.x});
    server_bounds.emplace("y", json::Value{server.y});
    server_bounds.emplace("w", json::Value{server.w});
    server_bounds.emplace("h", json::Value{server.h});
    window.emplace("window_server_onscreen", json::Value{server.onscreen});
    window.emplace(
        "window_server_surface_area",
        json::Value{server.w * server.h});
    window.emplace(
        "window_server_bounds",
        json::Value{std::move(server_bounds)});
    window.emplace(
        "window_server_window_number",
        json::Value{static_cast<std::int64_t>(server.window_number)});
    window.emplace(
        "window_server_order_index",
        json::Value{static_cast<std::int64_t>(server.window_order_index)});
    window.emplace(
        "window_server_app_order_index",
        json::Value{static_cast<std::int64_t>(server.app_window_order_index)});
    window.emplace(
        "window_server_frontmost_app_window",
        json::Value{server.frontmost_app_window});
    window.emplace(
        "window_server_occluded_by_front_app_window",
        json::Value{server.occluded_by_front_app_window});
    bool const ready_for_user_interaction =
        ns_window
        && window_visible
        && window_key
        && window_main
        && app_active
        && server.valid
        && server.onscreen
        && server.frontmost_app_window
        && !server.occluded_by_front_app_window;
    auto const* visibility_state = "ready";
    if (!ns_window) {
        visibility_state = "missing-window";
    } else if (!window_visible) {
        visibility_state = "window-not-visible";
    } else if (!server.valid) {
        visibility_state = "window-server-bounds-missing";
    } else if (!server.onscreen) {
        visibility_state = "window-server-offscreen";
    } else if (!app_active) {
        visibility_state = "app-not-active";
    } else if (!window_key) {
        visibility_state = "window-not-key";
    } else if (!window_main) {
        visibility_state = "window-not-main";
    } else if (server.occluded_by_front_app_window) {
        visibility_state = "occluded-by-front-app-window";
    } else if (!server.frontmost_app_window) {
        visibility_state = "not-frontmost-app-window";
    }
    window.emplace(
        "ready_for_user_interaction",
        json::Value{ready_for_user_interaction});
    window.emplace("visibility_state", json::Value{visibility_state});
    bool const artifact_capture_ready =
        ns_window
        && window_visible
        && server.valid
        && server.onscreen
        && !server.occluded_by_front_app_window;
    auto const* artifact_visibility_state = "ready";
    if (!ns_window) {
        artifact_visibility_state = "missing-window";
    } else if (!window_visible) {
        artifact_visibility_state = "window-not-visible";
    } else if (!server.valid) {
        artifact_visibility_state = "window-server-bounds-missing";
    } else if (!server.onscreen) {
        artifact_visibility_state = "window-server-offscreen";
    } else if (server.occluded_by_front_app_window) {
        artifact_visibility_state = "occluded-by-front-app-window";
    }
    window.emplace(
        "artifact_capture_ready",
        json::Value{artifact_capture_ready});
    window.emplace(
        "artifact_capture_visibility_state",
        json::Value{artifact_visibility_state});
    window.emplace(
        "artifact_capture_contract",
        json::Value{"visible-onscreen-native-window"});
    window.emplace(
        "launch_visibility_contract",
        json::Value{"visible-key-main-frontmost-native-window"});
    return window;
}

inline json::Value macos_platform_runtime_details_json_with_reason(
        std::string_view artifact_reason) {
#ifdef __APPLE__
    json::Object runtime;
    runtime.emplace("renderer", json::Value{macos_renderer_runtime_json()});
    runtime.emplace("images", json::Value{macos_images_runtime_json()});
    runtime.emplace("text_input", json::Value{macos_text_input_runtime_json()});
    runtime.emplace("input", json::Value{macos_input_runtime_json()});
    runtime.emplace("window", json::Value{macos_window_runtime_json()});
    if (!artifact_reason.empty()) {
        runtime.emplace(
            "artifact_reason",
            json::Value{std::string(artifact_reason)});
    }
    return json::Value{std::move(runtime)};
#else
    (void)artifact_reason;
    return json::Value{json::Object{}};
#endif
}

inline json::Value macos_platform_runtime_details_json() {
    return macos_platform_runtime_details_json_with_reason({});
}

inline std::string macos_snapshot_json() {
    return ::phenotype::detail::serialize_diag_snapshot_with_debug(
        macos_debug_capabilities(),
        macos_platform_runtime_details_json());
}

inline std::optional<DebugFrameCapture> macos_capture_frame_rgba() {
#ifdef __APPLE__
    if (!g_renderer.initialized
        || !g_renderer.last_frame_available
        || !g_renderer.debug_capture_texture
        || !g_renderer.device
        || !g_renderer.queue) {
        return std::nullopt;
    }

    auto width = g_renderer.last_render_width;
    auto height = g_renderer.last_render_height;
    if (width <= 0 || height <= 0)
        return std::nullopt;

    auto const bytes_per_row =
        static_cast<std::size_t>(width) * sizeof(std::uint32_t);
    auto const total_size = bytes_per_row * static_cast<std::size_t>(height);
    if (!ensure_frame_readback_buffer(total_size))
        return std::nullopt;

    NS::AutoreleasePool* pool = NS::AutoreleasePool::alloc()->init();
    auto* command_buffer = g_renderer.queue->commandBuffer();
    if (!command_buffer) {
        pool->release();
        return std::nullopt;
    }
    auto* blit = command_buffer->blitCommandEncoder();
    if (!blit) {
        pool->release();
        return std::nullopt;
    }

    MTL::Origin origin{0, 0, 0};
    MTL::Size size{
        NS::UInteger(width),
        NS::UInteger(height),
        NS::UInteger(1),
    };
    blit->copyFromTexture(
        g_renderer.debug_capture_texture,
        NS::UInteger(0),
        NS::UInteger(0),
        origin,
        size,
        g_renderer.frame_readback_buf,
        NS::UInteger(0),
        NS::UInteger(bytes_per_row),
        NS::UInteger(total_size));
    blit->endEncoding();

    command_buffer->commit();
    command_buffer->waitUntilCompleted();

    auto const* mapped =
        static_cast<std::uint8_t const*>(g_renderer.frame_readback_buf->contents());
    if (!mapped) {
        pool->release();
        return std::nullopt;
    }

    DebugFrameCapture capture{};
    capture.width = static_cast<unsigned int>(width);
    capture.height = static_cast<unsigned int>(height);
    capture.rgba.resize(total_size);

    for (int y = 0; y < height; ++y) {
        auto const* src_row = mapped + static_cast<std::size_t>(y) * bytes_per_row;
        auto* dst_row =
            capture.rgba.data()
            + static_cast<std::size_t>(y) * bytes_per_row;
        for (int x = 0; x < width; ++x) {
            auto const pixel_offset = static_cast<std::size_t>(x) * 4u;
            dst_row[pixel_offset + 0] = src_row[pixel_offset + 2];
            dst_row[pixel_offset + 1] = src_row[pixel_offset + 1];
            dst_row[pixel_offset + 2] = src_row[pixel_offset + 0];
            dst_row[pixel_offset + 3] = src_row[pixel_offset + 3];
        }
    }

    pool->release();
    return capture;
#else
    return std::nullopt;
#endif
}

inline DebugArtifactBundleResult macos_write_artifact_bundle(
        char const* directory,
        char const* reason) {
    auto snapshot = macos_snapshot_json();
    auto runtime_json = json::emit(
        macos_platform_runtime_details_json_with_reason(
            reason ? std::string_view(reason) : std::string_view{}));
    auto frame = macos_capture_frame_rgba();
    return ::phenotype::diag::detail::write_debug_artifact_bundle(
        directory ? std::string_view(directory) : std::string_view{},
        "macos",
        snapshot,
        runtime_json,
        frame ? &*frame : nullptr);
}

inline void install_macos_debug_providers() {
    ::phenotype::diag::detail::set_platform_capabilities_provider(
        macos_debug_capabilities);
    ::phenotype::diag::detail::set_platform_runtime_details_provider(
        macos_platform_runtime_details_json);
}

// ----- file-open dialog --------------------------------------------------
//
// Selectors for `[NSOpenPanel openPanel]` and friends. Inlined here
// rather than next to the alphabetised selector block above so the
// dialog implementation stays in one contiguous diff — they're only
// referenced from `macos_dialog_open_file` below.

inline SEL sel_open_panel() {
    static auto sel = sel_registerName("openPanel");
    return sel;
}
inline SEL sel_set_can_choose_files() {
    static auto sel = sel_registerName("setCanChooseFiles:");
    return sel;
}
inline SEL sel_set_can_choose_directories() {
    static auto sel = sel_registerName("setCanChooseDirectories:");
    return sel;
}
inline SEL sel_set_allows_multiple_selection() {
    static auto sel = sel_registerName("setAllowsMultipleSelection:");
    return sel;
}
inline SEL sel_set_allowed_file_types() {
    static auto sel = sel_registerName("setAllowedFileTypes:");
    return sel;
}
inline SEL sel_run_modal() {
    static auto sel = sel_registerName("runModal");
    return sel;
}
inline SEL sel_url() {
    static auto sel = sel_registerName("URL");
    return sel;
}
inline SEL sel_path() {
    static auto sel = sel_registerName("path");
    return sel;
}
inline SEL sel_string_with_utf8_string() {
    static auto sel = sel_registerName("stringWithUTF8String:");
    return sel;
}
inline SEL sel_add_object() {
    static auto sel = sel_registerName("addObject:");
    return sel;
}

// Forward declare the autorelease-pool entry points from the Objective-C
// runtime — `<objc/runtime.h>` doesn't expose them via the headers shipped
// with the intron toolchain, so we link to the dyld export directly.
extern "C" void* objc_autoreleasePoolPush(void);
extern "C" void  objc_autoreleasePoolPop(void* pool);

// `phenotype::native::dialog::open_file` macOS backend. Modally runs
// NSOpenPanel and synthesises the `(char const*)` callback contract:
// the panel returns control with NSModalResponseOK / Cancel; we hand
// the picked file's POSIX path to the caller, or null on cancel.
//
// `setAllowedFileTypes:` is technically deprecated in favour of
// `setAllowedContentTypes:` (UTType-based) on macOS 12+, but the older
// API is still functional and avoids dragging UniformTypeIdentifiers
// into phenotype's link surface for what is (today) the only consumer.
inline void macos_dialog_open_file(char const* filter_extensions,
                                   void (*callback)(char const* path)) {
    if (!callback)
        return;

    void* pool = objc_autoreleasePoolPush();
    char const* result_path = nullptr;
    std::string path_storage;

    auto open_panel_class =
        static_cast<Class>(objc_getClass("NSOpenPanel"));
    if (open_panel_class) {
        id panel = objc_send<id>(class_as_id(open_panel_class), sel_open_panel());
        if (panel) {
            objc_send<void>(panel, sel_set_can_choose_files(),
                            static_cast<bool>(true));
            objc_send<void>(panel, sel_set_can_choose_directories(),
                            static_cast<bool>(false));
            objc_send<void>(panel, sel_set_allows_multiple_selection(),
                            static_cast<bool>(false));

            if (filter_extensions != nullptr && filter_extensions[0] != '\0') {
                auto ns_string_class =
                    static_cast<Class>(objc_getClass("NSString"));
                auto mutable_array_class =
                    static_cast<Class>(objc_getClass("NSMutableArray"));
                if (ns_string_class && mutable_array_class) {
                    id ext_arr = objc_send<id>(
                        class_as_id(mutable_array_class), sel_array());
                    if (ext_arr) {
                        std::string ext_buf;
                        for (char const* p = filter_extensions; ; ++p) {
                            if (*p == ';' || *p == '\0') {
                                if (!ext_buf.empty()) {
                                    id ext_str = objc_send<id>(
                                        class_as_id(ns_string_class),
                                        sel_string_with_utf8_string(),
                                        ext_buf.c_str());
                                    if (ext_str)
                                        objc_send<void>(ext_arr,
                                                        sel_add_object(),
                                                        ext_str);
                                    ext_buf.clear();
                                }
                                if (*p == '\0') break;
                            } else {
                                ext_buf.push_back(*p);
                            }
                        }
                        objc_send<void>(panel, sel_set_allowed_file_types(),
                                        ext_arr);
                    }
                }
            }

            // NSModalResponseOK == 1.
            long modal_response = objc_send<long>(panel, sel_run_modal());
            if (modal_response == 1) {
                id url = objc_send<id>(panel, sel_url());
                if (url) {
                    id path_str = objc_send<id>(url, sel_path());
                    if (path_str) {
                        char const* utf8 = objc_send<char const*>(
                            path_str, sel_utf8_string());
                        if (utf8) {
                            path_storage = utf8;
                            result_path = path_storage.c_str();
                        }
                    }
                }
            }
        }
    }

    callback(result_path);
    objc_autoreleasePoolPop(pool);
}

} // namespace phenotype::native::detail
#endif

export namespace phenotype::native {

inline platform_api const& macos_platform() {
#ifdef __APPLE__
    detail::install_macos_debug_providers();
    static platform_api api{
        "macos",
        true,
        {
            detail::text_init,
            detail::text_shutdown,
            detail::text_measure_api,
            detail::text_metrics_api,
            detail::text_build_atlas,
            detail::text_register_font_file,
        },
        {
            detail::renderer_init,
            detail::renderer_flush,
            detail::renderer_shutdown,
            detail::renderer_hit_test,
        },
        {
            detail::input_attach,
            detail::input_detach,
            detail::input_sync,
            detail::macos_uses_shared_caret_blink,
            detail::macos_caret_blink_interval_ms,
            nullptr,
            nullptr,
            detail::input_dismiss_transient,
            nullptr,
            nullptr,
        },
        {
            detail::macos_debug_capabilities,
            detail::macos_snapshot_json,
            detail::macos_capture_frame_rgba,
            detail::macos_write_artifact_bundle,
        },
        [](char const* url, unsigned int len) {
            auto opened = cppx::os::system::open_url(std::string_view(url, len));
            if (!opened) {
                std::fprintf(
                    stderr,
                    "[macos] failed to open url: %.*s (%.*s)\n",
                    static_cast<int>(len),
                    url,
                    static_cast<int>(cppx::os::to_string(opened.error()).size()),
                    cppx::os::to_string(opened.error()).data());
            }
        },
        nullptr,
        {
            detail::macos_dialog_open_file,
        },
        {
            detail::configure_window,
        },
    };
    api.system.settings = detail::macos_system_settings_snapshot;
    return api;
#else
    static platform_api api{};
    return api;
#endif
}

} // namespace phenotype::native
#endif
