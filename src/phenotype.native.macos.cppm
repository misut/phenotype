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
#include <memory>
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
import phenotype.native.macos.input;
import phenotype.types;

#ifdef __APPLE__
namespace phenotype::native::detail {

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
    if (!text_runtime_initialized() || len == 0)
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

inline bool macos_env_flag_enabled(char const* name) {
    auto const* value = std::getenv(name);
    return value && value[0] != '\0' && value[0] != '0';
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
    std::uint64_t image_atlas_uploaded_generation = 0;
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
    double layer_contents_scale = 0.0;
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
    bool debug_capture_next_frame = false;
    bool debug_capture_always = false;
    std::uint64_t debug_capture_copy_count = 0;
    bool last_material_backdrop_available = false;
    std::uint64_t material_backdrop_theme_generation = 0;
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

inline RendererState g_default_renderer;

struct ActiveRendererBinding {
    RendererState* state = &g_default_renderer;
};

inline ActiveRendererBinding& active_renderer_binding() {
    static ActiveRendererBinding& binding =
        *new ActiveRendererBinding{&g_default_renderer};
    return binding;
}

inline ActiveRendererBinding capture_active_renderer_binding() {
    return active_renderer_binding();
}

inline std::vector<std::unique_ptr<RendererState>>& renderer_registry() {
    static std::vector<std::unique_ptr<RendererState>>& states =
        *new std::vector<std::unique_ptr<RendererState>>();
    return states;
}

inline RendererState& renderer_state() {
    auto& binding = active_renderer_binding();
    if (!binding.state)
        binding.state = &g_default_renderer;
    return *binding.state;
}

inline RendererState* find_renderer_state(NativeSurfaceDescriptor* surface) {
    if (!surface)
        return &g_default_renderer;
    if (g_default_renderer.surface == surface)
        return &g_default_renderer;
    for (auto& state : renderer_registry()) {
        if (state && state->surface == surface)
            return state.get();
    }
    return nullptr;
}

inline RendererState& ensure_renderer_state(NativeSurfaceDescriptor* surface) {
    if (!surface)
        return g_default_renderer;
    if (!g_default_renderer.initialized && !g_default_renderer.surface) {
        g_default_renderer.surface = surface;
        return g_default_renderer;
    }
    if (auto* existing = find_renderer_state(surface))
        return *existing;
    auto state = std::make_unique<RendererState>();
    state->surface = surface;
    auto* ptr = state.get();
    renderer_registry().push_back(std::move(state));
    return *ptr;
}

inline void bind_renderer_state(RendererState& state) {
    active_renderer_binding().state = &state;
}

inline void activate_renderer_state(NativeSurfaceDescriptor* surface) {
    bind_renderer_state(ensure_renderer_state(surface));
}

inline bool all_renderer_surfaces_uploaded_image_generation(
        std::uint64_t generation) {
    auto state_is_current = [generation](RendererState const& state) {
        if (!state.surface && !state.initialized)
            return true;
        return state.image_atlas_uploaded_generation == generation;
    };
    if (!state_is_current(g_default_renderer))
        return false;
    for (auto const& state : renderer_registry()) {
        if (state && !state_is_current(*state))
            return false;
    }
    return true;
}

inline void restore_active_renderer_binding(
        ActiveRendererBinding const& binding) {
    active_renderer_binding() = binding;
    if (!active_renderer_binding().state)
        active_renderer_binding().state = &g_default_renderer;
}

struct ScopedRendererActivation {
    ActiveRendererBinding previous{};

    explicit ScopedRendererActivation(NativeSurfaceDescriptor* surface)
        : previous(capture_active_renderer_binding()) {
        activate_renderer_state(surface);
    }

    ~ScopedRendererActivation() {
        restore_active_renderer_binding(previous);
    }
};

inline void reset_active_renderer_state() {
    bind_renderer_state(g_default_renderer);
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

    auto* replacement = renderer_state().device->newBuffer(
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
    if (renderer_state().text_atlas_texture)
        return true;

    auto* tex_desc = MTL::TextureDescriptor::texture2DDescriptor(
        MTL::PixelFormatR8Unorm,
        NS::UInteger(TextAtlasCache::atlas_size),
        NS::UInteger(TextAtlasCache::atlas_size),
        false);
    tex_desc->setUsage(MTL::TextureUsageShaderRead);
    renderer_state().text_atlas_texture = renderer_state().device->newTexture(tex_desc);
    return renderer_state().text_atlas_texture != nullptr;
}

inline bool ensure_image_atlas_texture() {
    if (renderer_state().image_atlas_texture)
        return true;

    auto* tex_desc = MTL::TextureDescriptor::texture2DDescriptor(
        MTL::PixelFormatRGBA8Unorm,
        NS::UInteger(ImageAtlasCache::atlas_size),
        NS::UInteger(ImageAtlasCache::atlas_size),
        false);
    tex_desc->setUsage(MTL::TextureUsageShaderRead);
    renderer_state().image_atlas_texture = renderer_state().device->newTexture(tex_desc);
    return renderer_state().image_atlas_texture != nullptr;
}

inline bool ensure_debug_capture_texture(int width, int height) {
    if (width <= 0 || height <= 0)
        return false;
    if (renderer_state().debug_capture_texture
        && renderer_state().debug_capture_width == width
        && renderer_state().debug_capture_height == height) {
        return true;
    }

    if (renderer_state().debug_capture_texture) {
        renderer_state().debug_capture_texture->release();
        renderer_state().debug_capture_texture = nullptr;
        renderer_state().debug_capture_width = 0;
        renderer_state().debug_capture_height = 0;
    }

    auto* tex_desc = MTL::TextureDescriptor::texture2DDescriptor(
        MTL::PixelFormatBGRA8Unorm,
        NS::UInteger(width),
        NS::UInteger(height),
        false);
    tex_desc->setUsage(MTL::TextureUsageShaderRead | MTL::TextureUsageRenderTarget);
    renderer_state().debug_capture_texture = renderer_state().device->newTexture(tex_desc);
    if (renderer_state().debug_capture_texture) {
        renderer_state().debug_capture_width = width;
        renderer_state().debug_capture_height = height;
    }
    return renderer_state().debug_capture_texture != nullptr;
}

inline void request_debug_capture_next_frame() {
    renderer_state().debug_capture_next_frame = true;
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
    bool const debug_panel_needs_backdrop =
        ::phenotype::detail::g_app().debug_panel_open;
    if (!debug_panel_needs_backdrop
        && (::phenotype::detail::g_app().has_active_animations
         || ::phenotype::detail::g_app().has_active_input_motion)
        && !macos_env_flag_enabled(
            "PHENOTYPE_MATERIAL_DISABLE_ACTIVE_QUALITY_THROTTLE")) {
        policy.allow_backdrop_sampling = false;
        policy.allow_noise = false;
        policy.max_blur_radius = 0.0f;
        policy.max_sample_taps = 0;
    }
    return policy;
}

inline bool ensure_material_backdrop_texture(int width, int height) {
    if (width <= 0 || height <= 0)
        return false;
    if (renderer_state().material_backdrop_texture
        && renderer_state().material_backdrop_width == width
        && renderer_state().material_backdrop_height == height) {
        return true;
    }

    if (renderer_state().material_backdrop_texture) {
        release_material_backdrop_luma_pending_command_buffer();
        renderer_state().material_backdrop_texture->release();
        renderer_state().material_backdrop_texture = nullptr;
        renderer_state().material_backdrop_width = 0;
        renderer_state().material_backdrop_height = 0;
        renderer_state().last_material_backdrop_available = false;
        renderer_state().last_material_backdrop_excludes_foreground_text = false;
        renderer_state().last_material_backdrop_color_available = false;
        renderer_state().last_material_backdrop_color_mean = {255, 255, 255, 255};
        renderer_state().last_material_backdrop_luma_available = false;
        renderer_state().last_material_backdrop_luma_sample_count = 0;
        renderer_state().last_material_backdrop_luma_grid_width = 0;
        renderer_state().last_material_backdrop_luma_grid_height = 0;
        renderer_state().last_material_backdrop_luma_frame = 0;
        renderer_state().last_material_backdrop_luma_status = "not-sampled";
    }

    auto* tex_desc = MTL::TextureDescriptor::texture2DDescriptor(
        MTL::PixelFormatBGRA8Unorm,
        NS::UInteger(width),
        NS::UInteger(height),
        false);
    tex_desc->setUsage(MTL::TextureUsageShaderRead | MTL::TextureUsageRenderTarget);
    renderer_state().material_backdrop_texture =
        renderer_state().device->newTexture(tex_desc);
    if (renderer_state().material_backdrop_texture) {
        renderer_state().material_backdrop_width = width;
        renderer_state().material_backdrop_height = height;
    }
    return renderer_state().material_backdrop_texture != nullptr;
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
    if (required <= renderer_state().material_backdrop_luma_sample_capacity
        && renderer_state().material_backdrop_luma_sample_buf) {
        return true;
    }

    std::size_t new_capacity =
        renderer_state().material_backdrop_luma_sample_capacity > 0
            ? renderer_state().material_backdrop_luma_sample_capacity
            : k_material_backdrop_luma_row_stride * 32u;
    while (new_capacity < required)
        new_capacity *= 2u;

    auto* replacement = renderer_state().device->newBuffer(
        NS::UInteger(new_capacity),
        MTL::ResourceStorageModeShared);
    if (!replacement)
        return false;

    if (renderer_state().material_backdrop_luma_sample_buf)
        renderer_state().material_backdrop_luma_sample_buf->release();
    renderer_state().material_backdrop_luma_sample_buf = replacement;
    renderer_state().material_backdrop_luma_sample_capacity = new_capacity;
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
    if (renderer_state().material_backdrop_luma_pending_command_buffer) {
        renderer_state().material_backdrop_luma_pending_command_buffer->release();
        renderer_state().material_backdrop_luma_pending_command_buffer = nullptr;
    }
    renderer_state().material_backdrop_luma_pending_sample_count = 0;
    renderer_state().material_backdrop_luma_pending_grid_width = 0;
    renderer_state().material_backdrop_luma_pending_grid_height = 0;
    renderer_state().material_backdrop_luma_pending_frame = 0;
}

inline void process_completed_material_backdrop_luma_sample() {
    auto* command_buffer =
        renderer_state().material_backdrop_luma_pending_command_buffer;
    if (!command_buffer)
        return;

    auto const status = command_buffer->status();
    if (status != MTL::CommandBufferStatusCompleted
        && status != MTL::CommandBufferStatusError) {
        return;
    }

    if (status == MTL::CommandBufferStatusError
        || !renderer_state().material_backdrop_luma_sample_buf
        || renderer_state().material_backdrop_luma_pending_sample_count == 0) {
        renderer_state().last_material_backdrop_color_available = false;
        renderer_state().last_material_backdrop_luma_available = false;
        renderer_state().last_material_backdrop_luma_sample_count = 0;
        renderer_state().last_material_backdrop_luma_status =
            status == MTL::CommandBufferStatusError
                ? "sample-command-buffer-error"
                : "sample-buffer-unavailable";
        release_material_backdrop_luma_pending_command_buffer();
        return;
    }

    auto const* mapped = static_cast<std::uint8_t const*>(
        renderer_state().material_backdrop_luma_sample_buf->contents());
    if (!mapped) {
        renderer_state().last_material_backdrop_color_available = false;
        renderer_state().last_material_backdrop_luma_available = false;
        renderer_state().last_material_backdrop_luma_sample_count = 0;
        renderer_state().last_material_backdrop_luma_status =
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
         i < renderer_state().material_backdrop_luma_pending_sample_count;
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
        renderer_state().last_material_backdrop_color_available = true;
        renderer_state().last_material_backdrop_color_mean = Color{
            mean_channel(red_sum),
            mean_channel(green_sum),
            mean_channel(blue_sum),
            mean_channel(alpha_sum),
        };
        renderer_state().last_material_backdrop_luma_available = true;
        renderer_state().last_material_backdrop_luma_min = luma_min;
        renderer_state().last_material_backdrop_luma_max = luma_max;
        renderer_state().last_material_backdrop_luma_mean =
            luma_sum / static_cast<float>(sampled);
        renderer_state().last_material_backdrop_luma_sample_count = sampled;
        renderer_state().last_material_backdrop_luma_grid_width =
            renderer_state().material_backdrop_luma_pending_grid_width;
        renderer_state().last_material_backdrop_luma_grid_height =
            renderer_state().material_backdrop_luma_pending_grid_height;
        renderer_state().last_material_backdrop_luma_frame =
            renderer_state().material_backdrop_luma_pending_frame;
        renderer_state().last_material_backdrop_luma_status =
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
    if (renderer_state().material_backdrop_luma_pending_command_buffer) {
        ++summary.backdrop_luma_sampling_skipped_count;
        summary.backdrop_luma_sampling_skip_reason =
            "previous-sample-pending";
        ++renderer_state().material_backdrop_luma_skipped_sample_count;
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
                renderer_state().material_backdrop_luma_sample_buf,
                NS::UInteger(
                    static_cast<std::size_t>(index)
                    * k_material_backdrop_luma_row_stride),
                NS::UInteger(k_material_backdrop_luma_row_stride),
                NS::UInteger(k_material_backdrop_luma_row_stride));
        }
    }

    renderer_state().material_backdrop_luma_pending_sample_count = sample_count;
    renderer_state().material_backdrop_luma_pending_grid_width = grid_w;
    renderer_state().material_backdrop_luma_pending_grid_height = grid_h;
    renderer_state().material_backdrop_luma_pending_frame = frame_index;
    command_buffer->retain();
    renderer_state().material_backdrop_luma_pending_command_buffer = command_buffer;
    return true;
}

inline bool ensure_frame_readback_buffer(std::size_t required) {
    if (required == 0)
        return false;
    if (required <= renderer_state().frame_readback_capacity && renderer_state().frame_readback_buf)
        return true;

    std::size_t new_capacity = (renderer_state().frame_readback_capacity > 0)
        ? renderer_state().frame_readback_capacity
        : 4096;
    while (new_capacity < required)
        new_capacity *= 2;

    auto* replacement = renderer_state().device->newBuffer(
        NS::UInteger(new_capacity),
        MTL::ResourceStorageModeShared);
    if (!replacement)
        return false;

    if (renderer_state().frame_readback_buf)
        renderer_state().frame_readback_buf->release();
    renderer_state().frame_readback_buf = replacement;
    renderer_state().frame_readback_capacity = new_capacity;
    return true;
}

inline void sync_layer_backing_geometry(int fbw, int fbh, float content_scale) {
    if (!renderer_state().layer)
        return;
    double const scale =
        content_scale > 0.0f && std::isfinite(content_scale)
            ? static_cast<double>(content_scale)
            : 1.0;
    if (std::fabs(renderer_state().layer_contents_scale - scale) > 0.001) {
        reinterpret_cast<void(*)(void*, SEL, double)>(objc_msgSend)(
            reinterpret_cast<void*>(renderer_state().layer),
            sel_registerName("setContentsScale:"),
            scale);
        renderer_state().layer_contents_scale = scale;
    }
    if (fbw == renderer_state().drawable_width && fbh == renderer_state().drawable_height)
        return;
    CGSize drawable_size{
        .width = static_cast<CGFloat>(fbw),
        .height = static_cast<CGFloat>(fbh),
    };
    renderer_state().layer->setDrawableSize(drawable_size);
    renderer_state().drawable_width = fbw;
    renderer_state().drawable_height = fbh;
}

inline bool upload_text_cache() {
    auto& cache = renderer_state().text_cache;
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
    renderer_state().text_atlas_texture->replaceRegion(
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
    if (cache.generation == 0
        || renderer_state().image_atlas_uploaded_generation == cache.generation) {
        return true;
    }
    if (!ensure_image_atlas_texture())
        return false;

    bool const can_upload_dirty_region =
        cache.dirty
        && renderer_state().image_atlas_uploaded_generation
            == cache.dirty_base_generation;
    int x = 0;
    int y = 0;
    int width = ImageAtlasCache::atlas_size;
    int height = ImageAtlasCache::atlas_size;
    if (can_upload_dirty_region) {
        width = cache.dirty_max_x - cache.dirty_min_x;
        height = cache.dirty_max_y - cache.dirty_min_y;
        if (width <= 0 || height <= 0) {
            renderer_state().image_atlas_uploaded_generation = cache.generation;
            if (all_renderer_surfaces_uploaded_image_generation(cache.generation)) {
                cache.dirty = false;
                cache.dirty_base_generation = cache.generation;
                cache.dirty_min_x = ImageAtlasCache::atlas_size;
                cache.dirty_min_y = ImageAtlasCache::atlas_size;
                cache.dirty_max_x = 0;
                cache.dirty_max_y = 0;
            }
            return true;
        }
        x = cache.dirty_min_x;
        y = cache.dirty_min_y;
    }

    MTL::Region region = MTL::Region::Make2D(x, y, width, height);
    auto const* src = cache.pixels.data()
        + static_cast<std::size_t>(
            (y * ImageAtlasCache::atlas_size + x) * 4);
    renderer_state().image_atlas_texture->replaceRegion(
        region,
        0,
        src,
        ImageAtlasCache::atlas_size * 4);
    metrics::inst::native_texture_upload_bytes.add(
        static_cast<std::uint64_t>(width * height * 4),
        native_platform_attrs());

    renderer_state().image_atlas_uploaded_generation = cache.generation;
    if (all_renderer_surfaces_uploaded_image_generation(cache.generation)) {
        cache.dirty = false;
        cache.dirty_base_generation = cache.generation;
        cache.dirty_min_x = ImageAtlasCache::atlas_size;
        cache.dirty_min_y = ImageAtlasCache::atlas_size;
        cache.dirty_max_x = 0;
        cache.dirty_max_y = 0;
    }
    return true;
}

inline bool prepare_text_instances(float scale) {
    auto& scratch = renderer_state().scratch;
    for (auto& batch : scratch.batches) {
        batch.texts.clear();
        batch.text_command_indices.clear();
    }
    scratch.overlay_text_instances.clear();
    if (scratch.text_runs.empty())
        return true;

    auto& cache = renderer_state().text_cache;
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
        for (auto& batch : scratch.batches) {
            batch.texts.clear();
            batch.text_command_indices.clear();
        }
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
            bool const overlay_run =
                run.batch_idx == std::numeric_limits<std::uint32_t>::max();
            auto& dst = overlay_run
                ? scratch.overlay_text_instances
                : scratch.batches[run.batch_idx].texts;
            auto* command_indices = overlay_run
                ? nullptr
                : &scratch.batches[run.batch_idx].text_command_indices;
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
            if (command_indices)
                command_indices->push_back(run.command_index);
        }

        if (!restart)
            return true;
    }
}

inline void prepare_image_instances(float scale) {
    auto& scratch = renderer_state().scratch;
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

inline bool rects_overlap(float ax,
                          float ay,
                          float aw,
                          float ah,
                          float bx,
                          float by,
                          float bw,
                          float bh) noexcept {
    return aw > 0.0f && ah > 0.0f && bw > 0.0f && bh > 0.0f
        && ax < bx + bw && ax + aw > bx
        && ay < by + bh && ay + ah > by;
}

inline bool foreground_text_occluded_by_later_material(
        TextInstanceGPU const& text,
        std::uint32_t text_command_index,
        std::vector<MaterialRuntimeRecord> const& records) noexcept {
    for (auto const& record : records) {
        auto const& plan = record.plan;
        if (record.command_index <= text_command_index)
            continue;
        if (plan.kind == MaterialKind::None)
            continue;
        auto const& geometry = plan.geometry;
        if (rects_overlap(
                text.rect[0],
                text.rect[1],
                text.rect[2],
                text.rect[3],
                geometry.x,
                geometry.y,
                geometry.w,
                geometry.h)) {
            return true;
        }
    }
    return false;
}

inline void draw_deferred_text_range(MTL::RenderCommandEncoder* encoder,
                                     bool text_uploaded,
                                     std::uint32_t base_instance,
                                     std::uint32_t count) {
    if (!text_uploaded || count == 0)
        return;
    encoder->setRenderPipelineState(renderer_state().text_pipeline);
    encoder->setVertexBuffer(renderer_state().uniform_buf, 0, 0);
    encoder->setVertexBuffer(renderer_state().text_instances_buf, 0, 1);
    encoder->setFragmentTexture(renderer_state().text_atlas_texture, 0);
    encoder->setFragmentSamplerState(renderer_state().sampler, 0);
    encoder->drawPrimitives(
        MTL::PrimitiveTypeTriangle,
        NS::UInteger(0), 6,
        NS::UInteger(count),
        NS::UInteger(base_instance));
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

inline void flush_appkit_backdrop_view(id view) {
    if (!view)
        return;
    using ObjcBool = signed char;
    if (objc_responds_to(view, sel_registerName("setNeedsLayout:"))) {
        objc_send<void>(
            view,
            sel_registerName("setNeedsLayout:"),
            static_cast<ObjcBool>(1));
    }
    if (objc_responds_to(view, sel_registerName("layoutSubtreeIfNeeded"))) {
        objc_send<void>(
            view,
            sel_registerName("layoutSubtreeIfNeeded"));
    }
    if (objc_responds_to(view, sel_registerName("setNeedsDisplay:"))) {
        objc_send<void>(
            view,
            sel_registerName("setNeedsDisplay:"),
            static_cast<ObjcBool>(1));
    }
    if (objc_responds_to(view, sel_registerName("displayIfNeeded"))) {
        objc_send<void>(
            view,
            sel_registerName("displayIfNeeded"));
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
        flush_appkit_backdrop_view(window_content);
        flush_appkit_backdrop_view(content_view);
        return true;
    }
    if (content_superview && content_superview == window_content) {
        id existing_backdrop = find_visual_effect_subview(content_superview);
        if (existing_backdrop) {
            configure_visual_effect_backdrop(
                existing_backdrop,
                material,
                opacity);
            flush_appkit_backdrop_view(existing_backdrop);
            flush_appkit_backdrop_view(content_superview);
            flush_appkit_backdrop_view(content_view);
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
    flush_appkit_backdrop_view(effect_view);
    flush_appkit_backdrop_view(content_view);
    flush_appkit_backdrop_view(container_view);
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
    auto* surface = desktop_surface(handle);
    activate_renderer_state(surface);
    if (renderer_state().initialized) return;
    renderer_state().surface = surface;
    renderer_state().ns_window = surface_ns_window(surface);
    renderer_state().content_view = surface_content_view(surface);
    if (!renderer_state().ns_window || !renderer_state().content_view) {
        std::fprintf(stderr, "[metal] no NSWindow/NSView surface\n");
        return;
    }

    renderer_state().device = MTL::CreateSystemDefaultDevice();
    if (!renderer_state().device) {
        std::fprintf(stderr, "[metal] no device\n");
        return;
    }
    renderer_state().queue = renderer_state().device->newCommandQueue();

    auto sel_wl = sel_registerName("setWantsLayer:");
    auto sel_sl = sel_registerName("setLayer:");
    void* view = renderer_state().content_view;
    reinterpret_cast<void(*)(void*, SEL, bool)>(objc_msgSend)(view, sel_wl, true);

    renderer_state().layer = CA::MetalLayer::layer()->retain();
    renderer_state().layer->setDevice(renderer_state().device);
    renderer_state().layer->setPixelFormat(MTL::PixelFormatBGRA8Unorm);
    renderer_state().layer->setFramebufferOnly(false);
    using ObjcBool = signed char;
    if (surface->window_options_valid
        && surface->window_chrome == WindowChromeStyle::IntegratedTitlebar) {
        objc_send<void>(
            reinterpret_cast<id>(renderer_state().layer),
            sel_set_opaque(),
            static_cast<ObjcBool>(0));
    } else {
        objc_send<void>(
            reinterpret_cast<id>(renderer_state().layer),
            sel_set_opaque(),
            static_cast<ObjcBool>(1));
    }

    int fbw = 0;
    int fbh = 0;
    surface_framebuffer_size(surface, fbw, fbh);
    sync_layer_backing_geometry(fbw, fbh, surface_content_scale(surface));

    // Prime the host's cached content scale even when the host bypassed
    // refresh_cached_canvas_size (e.g. test harnesses that wire the
    // host directly instead of going through run_app_with_platform).
    if (auto* host = ::phenotype::native::detail::active_host())
        host->cached_content_scale = surface_content_scale(surface);

    reinterpret_cast<void(*)(void*, SEL, void*)>(objc_msgSend)(
        view, sel_sl, static_cast<void*>(renderer_state().layer));

    NS::Error* err = nullptr;
    auto* lib = renderer_state().device->newLibrary(
        NS::String::string(MSL_SHADERS, NS::UTF8StringEncoding), nullptr, &err);
    if (!lib) {
        std::fprintf(stderr, "[metal] shader compile: %s\n",
                     err ? err->localizedDescription()->utf8String() : "unknown");
        return;
    }

    renderer_state().tri_pipeline   = create_pipeline(renderer_state().device, lib, "vs_tri",   "fs_tri");
    renderer_state().color_pipeline = create_pipeline(renderer_state().device, lib, "vs_color", "fs_color");
    renderer_state().material_pipeline = create_pipeline(renderer_state().device, lib, "vs_material", "fs_material");
    renderer_state().arc_pipeline   = create_pipeline(renderer_state().device, lib, "vs_arc",   "fs_arc");
    renderer_state().image_pipeline = create_pipeline(renderer_state().device, lib, "vs_image", "fs_image");
    renderer_state().text_pipeline = create_pipeline(renderer_state().device, lib, "vs_text", "fs_text");
    lib->release();
    if (!renderer_state().tri_pipeline || !renderer_state().color_pipeline
        || !renderer_state().material_pipeline
        || !renderer_state().arc_pipeline
        || !renderer_state().image_pipeline || !renderer_state().text_pipeline) {
        std::fprintf(stderr, "[metal] failed to create render pipelines\n");
        return;
    }

    renderer_state().uniform_buf = renderer_state().device->newBuffer(16, MTL::ResourceStorageModeShared);

    auto* sampler_desc = MTL::SamplerDescriptor::alloc()->init();
    sampler_desc->setMinFilter(MTL::SamplerMinMagFilterLinear);
    sampler_desc->setMagFilter(MTL::SamplerMinMagFilterLinear);
    sampler_desc->setSAddressMode(MTL::SamplerAddressModeClampToEdge);
    sampler_desc->setTAddressMode(MTL::SamplerAddressModeClampToEdge);
    renderer_state().sampler = renderer_state().device->newSamplerState(sampler_desc);
    sampler_desc->release();

    renderer_state().initialized = true;
    renderer_state().text_cache.active_scale_key = 0;
    renderer_state().debug_capture_always =
        macos_env_flag_enabled("PHENOTYPE_DEBUG_CAPTURE_EACH_FRAME");

    int winw = 0;
    int winh = 0;
    surface_logical_size(surface, winw, winh);
    std::printf("[phenotype-native] Metal initialized (%dx%d)\n", winw, winh);
}

inline void renderer_activate(native_surface_handle handle) {
    activate_renderer_state(desktop_surface(handle));
}

inline void renderer_flush(unsigned char const* buf, unsigned int len) {
    if (len == 0 || !renderer_state().initialized) return;

    NS::AutoreleasePool* pool = NS::AutoreleasePool::alloc()->init();
    auto const flush_started = metrics::detail::now_ns();
    renderer_state().material_executor_summary = MaterialExecutorSummary{};
    // Read the host-cached HiDPI scale. The shell keeps cached_content_scale
    // in sync with native viewport and content-scale changes.
    float frame_scale = 1.0f;
    if (auto* host = ::phenotype::native::detail::active_host())
        frame_scale = host->cached_content_scale;
    if (!(frame_scale > 0.0f))
        frame_scale = surface_content_scale(renderer_state().surface);
    float text_scale = frame_scale;
    float line_height_ratio = ::phenotype::detail::g_app().theme.line_height_ratio;

    double cr = 0.98;
    double cg = 0.98;
    double cb = 0.98;
    bool const transparent_window_surface =
        renderer_state().surface
        && renderer_state().surface->window_options_valid
        && renderer_state().surface->window_chrome
            == WindowChromeStyle::IntegratedTitlebar;
    double ca = transparent_window_surface ? 0.0 : 1.0;
    (void)process_completed_images();
    process_completed_material_backdrop_luma_sample();
    int fbw = 0;
    int fbh = 0;
    surface_framebuffer_size(renderer_state().surface, fbw, fbh);
    if (fbw == 0 || fbh == 0) {
        pool->release();
        return;
    }

    sync_layer_backing_geometry(fbw, fbh, frame_scale);
    auto const current_theme_generation =
        ::phenotype::detail::g_app().theme_generation;
    if (renderer_state().material_backdrop_theme_generation
        != current_theme_generation) {
        renderer_state().last_material_backdrop_available = false;
        renderer_state().last_material_backdrop_excludes_foreground_text = false;
        renderer_state().last_material_backdrop_color_available = false;
        renderer_state().last_material_backdrop_luma_available = false;
    }
    bool const backdrop_ready =
        renderer_state().material_pipeline
        && renderer_state().material_backdrop_texture
        && renderer_state().last_material_backdrop_available
        && renderer_state().material_backdrop_theme_generation
            == current_theme_generation
        && renderer_state().material_backdrop_width == fbw
        && renderer_state().material_backdrop_height == fbh;
    MaterialEnvironment material_env{};
    auto const accessibility = accessibility_display_options();
    renderer_state().accessibility_options = accessibility;
    material_env.capabilities = macos_material_capability_input(
        renderer_state().device,
        renderer_state().material_pipeline != nullptr,
        backdrop_ready,
        accessibility);
    material_env.backdrop.available = backdrop_ready;
    material_env.backdrop.stable = backdrop_ready;
    material_env.backdrop.excludes_foreground_text =
        backdrop_ready
        && renderer_state().last_material_backdrop_excludes_foreground_text;
    material_env.backdrop.source = backdrop_ready
        ? "previous-presented-frame"
        : "none";
    if (backdrop_ready && renderer_state().last_material_backdrop_luma_available) {
        if (renderer_state().last_material_backdrop_color_available) {
            material_env.backdrop.color_mean =
                renderer_state().last_material_backdrop_color_mean;
            material_env.backdrop.color_sample_count =
                renderer_state().last_material_backdrop_luma_sample_count;
            material_env.backdrop.color_sample_status =
                renderer_state().last_material_backdrop_luma_status;
        }
        material_env.backdrop.luma_min =
            renderer_state().last_material_backdrop_luma_min;
        material_env.backdrop.luma_max =
            renderer_state().last_material_backdrop_luma_max;
        material_env.backdrop.luma_mean =
            renderer_state().last_material_backdrop_luma_mean;
        material_env.backdrop.luma_sample_count =
            renderer_state().last_material_backdrop_luma_sample_count;
        material_env.backdrop.luma_sample_grid_width =
            renderer_state().last_material_backdrop_luma_grid_width;
        material_env.backdrop.luma_sample_grid_height =
            renderer_state().last_material_backdrop_luma_grid_height;
        material_env.backdrop.luma_sample_frame =
            renderer_state().last_material_backdrop_luma_frame;
        material_env.backdrop.luma_sample_status =
            renderer_state().last_material_backdrop_luma_status;
        material_env.backdrop.source =
            "previous-presented-frame-sampled-grid";
    }
    material_env.render_target.width = fbw;
    material_env.render_target.height = fbh;
    material_env.render_target.scale = frame_scale;
    material_env.render_target.pixel_format = "bgra8unorm";
    material_env.debug_seed.frame = ++renderer_state().material_frame_sequence;
    material_env.quality =
        macos_material_quality_policy(material_env.capabilities);
    auto decode_started = metrics::detail::now_ns();
    if (!decode_frame_commands(
            buf, len, line_height_ratio,
            material_env,
            renderer_state().scratch, renderer_state().hit_regions,
            cr, cg, cb, ca)) {
        pool->release();
        return;
    }
    if (transparent_window_surface)
        ca = 0.0;
    if (ca <= 0.0) {
        cr = 0.0;
        cg = 0.0;
        cb = 0.0;
    }
    renderer_state().last_clear_alpha = ca;
    renderer_state().last_clear_alpha_for_transparent_window =
        transparent_window_surface && ca == 0.0;
    renderer_state().last_full_frame_opaque_fill_count =
        renderer_state().scratch.full_frame_opaque_fill_count;
    renderer_state().last_transparent_window_has_opaque_frame_fill =
        transparent_window_surface
        && renderer_state().last_full_frame_opaque_fill_count > 0;
    auto const decode_ns = metrics::detail::now_ns() - decode_started;
    metrics::inst::native_phase_duration.record(
        decode_ns,
        native_attrs("command_decode"));

    auto image_started = metrics::detail::now_ns();
    prepare_image_instances(text_scale);
    metrics::inst::native_phase_duration.record(
        metrics::detail::now_ns() - image_started,
        native_attrs("image_prepare"));

    append_ime_overlay(renderer_state().scratch);
    append_generic_caret_overlay(renderer_state().scratch);

    auto text_started = metrics::detail::now_ns();
    if (!prepare_text_instances(text_scale)) {
        pool->release();
        return;
    }
    metrics::inst::native_phase_duration.record(
        metrics::detail::now_ns() - text_started,
        native_attrs("text_prepare"));

    finalize_batches(renderer_state().scratch);
    apply_material_container_execution_descriptors(renderer_state().scratch);

    MaterialExecutorSummary material_summary;
    material_summary.cpu_decode_ns = decode_ns;
    material_summary.material_pipeline_ready =
        material_env.capabilities.shader_blur;
    material_summary.material_backdrop_source_ready =
        material_env.backdrop.available;
    material_summary.foreground_text_candidate_count =
        renderer_state().scratch.foreground_text_candidate_count;
    material_summary.foreground_text_remap_count =
        renderer_state().scratch.foreground_text_remap_count;
    set_material_executor_backdrop_descriptor_summary(
        material_summary,
        material_env.backdrop);
    for (auto const& record : renderer_state().scratch.material_records) {
        accumulate_material_executor_plan_summary(
            material_summary,
            record.plan);
    }
    finalize_material_executor_summary(
        material_summary,
        renderer_state().scratch.material_records);
    material_summary.material_shader_content_scale = frame_scale;
    for (auto const& record : renderer_state().scratch.material_records) {
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
    bool const capture_frame_history =
        material_executor_requires_frame_capture(material_summary);

    int winw = 0;
    int winh = 0;
    surface_logical_size(renderer_state().surface, winw, winh);
    float uniforms[4] = {
        static_cast<float>(winw),
        static_cast<float>(winh),
        frame_scale,
        0,
    };
    std::memcpy(renderer_state().uniform_buf->contents(), uniforms, 16);

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

    auto* command_buffer = renderer_state().queue->commandBuffer();
    if (!command_buffer) {
        pool->release();
        return;
    }

    auto& scratch = renderer_state().scratch;
    auto const tri_bytes = scratch.tri_vertices.size() * sizeof(TriVertexGPU);
    bool tri_uploaded = false;
    if (!scratch.tri_vertices.empty()) {
        if (!ensure_instance_buffer(
                renderer_state().tri_vertices_buf,
                renderer_state().tri_vertices_capacity,
                tri_bytes,
                "tri_vertices")) {
            pool->release();
            return;
        }
        std::memcpy(
            renderer_state().tri_vertices_buf->contents(),
            scratch.tri_vertices.data(),
            tri_bytes);
        tri_uploaded = true;
    }

    auto const color_bytes = scratch.color_instances.size() * sizeof(ColorInstanceGPU);
    bool color_uploaded = false;
    if (!scratch.color_instances.empty()) {
        if (!ensure_instance_buffer(
                renderer_state().color_instances_buf,
                renderer_state().color_instances_capacity,
                color_bytes,
                "color_instances")) {
            pool->release();
            return;
        }
        std::memcpy(
            renderer_state().color_instances_buf->contents(),
            scratch.color_instances.data(),
            color_bytes);
        color_uploaded = true;
    }

    auto const material_bytes =
        scratch.material_instances.size() * sizeof(MaterialInstanceGPU);
    bool material_uploaded = false;
    auto material_upload_started = metrics::detail::now_ns();
    if (!scratch.material_instances.empty()) {
        auto const old_material_capacity = renderer_state().material_instances_capacity;
        if (!ensure_instance_buffer(
                renderer_state().material_instances_buf,
                renderer_state().material_instances_capacity,
                material_bytes,
                "material_instances")) {
            pool->release();
            return;
        }
        std::memcpy(
            renderer_state().material_instances_buf->contents(),
            scratch.material_instances.data(),
            material_bytes);
        material_uploaded = true;
        material_summary.material_upload_bytes =
            static_cast<std::int64_t>(material_bytes);
        material_summary.material_buffer_capacity_bytes =
            static_cast<std::int64_t>(renderer_state().material_instances_capacity);
        if (renderer_state().material_instances_capacity != old_material_capacity)
            ++material_summary.material_buffer_reallocations;
    }
    material_summary.cpu_material_upload_ns =
        metrics::detail::now_ns() - material_upload_started;

    auto const arc_bytes = scratch.arc_instances.size() * sizeof(ArcInstanceGPU);
    bool arc_uploaded = false;
    if (!scratch.arc_instances.empty()) {
        if (!ensure_instance_buffer(
                renderer_state().arc_instances_buf,
                renderer_state().arc_instances_capacity,
                arc_bytes,
                "arc_instances")) {
            pool->release();
            return;
        }
        std::memcpy(
            renderer_state().arc_instances_buf->contents(),
            scratch.arc_instances.data(),
            arc_bytes);
        arc_uploaded = true;
    }

    auto const image_bytes = scratch.image_instances.size() * sizeof(ImageInstanceGPU);
    bool image_uploaded = false;
    if (!scratch.image_instances.empty() && renderer_state().image_atlas_texture) {
        if (!ensure_instance_buffer(
                renderer_state().image_instances_buf,
                renderer_state().image_instances_capacity,
                image_bytes,
                "image_instances")) {
            pool->release();
            return;
        }
        std::memcpy(
            renderer_state().image_instances_buf->contents(),
            scratch.image_instances.data(),
            image_bytes);
        image_uploaded = true;
    }

    auto const text_bytes = scratch.text_instances.size() * sizeof(TextInstanceGPU);
    bool text_uploaded = false;
    if (!scratch.text_instances.empty() && renderer_state().text_atlas_texture) {
        if (!ensure_instance_buffer(
                renderer_state().text_instances_buf,
                renderer_state().text_instances_capacity,
                text_bytes,
                "text_instances")) {
            pool->release();
            return;
        }
        std::memcpy(
            renderer_state().text_instances_buf->contents(),
            scratch.text_instances.data(),
            text_bytes);
        text_uploaded = true;
    }

    // Per-batch scissor + draws. Each batch's instance ranges live
    // contiguously in the flat *_instances vectors thanks to
    // finalize_batches; we use baseInstance to point at them.
    float const scissor_scale = frame_scale;
    // The backdrop source is now rendered into a separate underlay texture
    // before the main drawable pass, with foreground text intentionally
    // omitted from that underlay. Text therefore no longer needs to be
    // deferred until after all materials draw. Keeping text in its original
    // scissor batch preserves command-order z semantics: main content text is
    // covered by later modal surfaces, while overlay text still draws above
    // its own surface.
    bool constexpr defer_foreground_pass = false;

    auto first_overlay_material_command =
        std::optional<std::uint32_t>{};
    for (auto const& record : scratch.material_records) {
        if (record.plan.role != MaterialSurfaceRole::Overlay)
            continue;
        if (!first_overlay_material_command
            || record.command_index < *first_overlay_material_command) {
            first_overlay_material_command = record.command_index;
        }
    }
    auto material_paint_layer_excluded_from_backdrop =
        [&](MaterialPaintLayerRange const& range) noexcept {
            return first_overlay_material_command
                && range.command_index >= *first_overlay_material_command;
        };
    auto draw_color_segment =
        [&](MTL::RenderCommandEncoder* target_encoder,
            std::size_t first,
            std::size_t count) {
            if (!color_uploaded || count == 0)
                return;
            target_encoder->drawPrimitives(
                MTL::PrimitiveTypeTriangle,
                NS::UInteger(0), 6,
                NS::UInteger(count),
                NS::UInteger(first));
        };
    auto draw_backdrop_underlay_colors =
        [&](MTL::RenderCommandEncoder* target_encoder,
            std::size_t batch_index,
            ScissorBatch const& batch) {
            auto const batch_color_count = batch.colors.size();
            if (!color_uploaded || batch_color_count == 0)
                return;
            std::vector<std::pair<std::size_t, std::size_t>> excluded;
            for (auto const& range : scratch.material_paint_layer_ranges) {
                if (range.batch_index != batch_index
                    || !material_paint_layer_excluded_from_backdrop(range)) {
                    continue;
                }
                auto const first = std::min(range.first, batch_color_count);
                auto const last = std::min(
                    first + range.count,
                    batch_color_count);
                if (first < last)
                    excluded.emplace_back(first, last);
            }
            if (excluded.empty()) {
                draw_color_segment(
                    target_encoder,
                    batch.color_first,
                    batch_color_count);
                return;
            }
            std::sort(excluded.begin(), excluded.end());
            auto cursor = std::size_t{0};
            for (auto const& [first, last] : excluded) {
                if (first > cursor) {
                    draw_color_segment(
                        target_encoder,
                        batch.color_first + cursor,
                        first - cursor);
                }
                cursor = std::max(cursor, last);
            }
            if (cursor < batch_color_count) {
                draw_color_segment(
                    target_encoder,
                    batch.color_first + cursor,
                    batch_color_count - cursor);
            }
        };

    auto draw_backdrop_underlay_batch =
        [&](MTL::RenderCommandEncoder* target_encoder,
            std::size_t batch_index,
            ScissorBatch const& batch) {
            auto const batch_tri_count =
                static_cast<std::uint32_t>(batch.tri_vertices.size());
            auto const batch_color_count =
                static_cast<std::uint32_t>(batch.colors.size());
            auto const batch_arc_count =
                static_cast<std::uint32_t>(batch.arcs.size());
            auto const batch_image_count =
                static_cast<std::uint32_t>(batch.images.size());
            if (batch_tri_count + batch_color_count + batch_arc_count
                    + batch_image_count == 0) {
                return;
            }
            auto const sr = compute_metal_scissor(
                batch, scissor_scale,
                static_cast<std::uint32_t>(renderer_state().drawable_width),
                static_cast<std::uint32_t>(renderer_state().drawable_height));
            if (sr.width == 0 || sr.height == 0)
                return;
            target_encoder->setScissorRect(sr);

            if (tri_uploaded && batch_tri_count > 0) {
                target_encoder->setRenderPipelineState(renderer_state().tri_pipeline);
                target_encoder->setVertexBuffer(renderer_state().uniform_buf, 0, 0);
                target_encoder->setVertexBuffer(
                    renderer_state().tri_vertices_buf,
                    NS::UInteger(batch.tri_first * sizeof(TriVertexGPU)),
                    1);
                target_encoder->drawPrimitives(
                    MTL::PrimitiveTypeTriangle,
                    NS::UInteger(0),
                    NS::UInteger(batch_tri_count));
            }

            if (color_uploaded && batch_color_count > 0) {
                target_encoder->setRenderPipelineState(renderer_state().color_pipeline);
                target_encoder->setVertexBuffer(renderer_state().uniform_buf, 0, 0);
                target_encoder->setVertexBuffer(
                    renderer_state().color_instances_buf, 0, 1);
                draw_backdrop_underlay_colors(
                    target_encoder,
                    batch_index,
                    batch);
            }

            if (arc_uploaded && batch_arc_count > 0) {
                target_encoder->setRenderPipelineState(renderer_state().arc_pipeline);
                target_encoder->setVertexBuffer(renderer_state().uniform_buf, 0, 0);
                target_encoder->setVertexBuffer(renderer_state().arc_instances_buf, 0, 1);
                target_encoder->drawPrimitives(
                    MTL::PrimitiveTypeTriangle,
                    NS::UInteger(0), 6,
                    NS::UInteger(batch_arc_count),
                    NS::UInteger(batch.arc_first));
            }

            if (image_uploaded && batch_image_count > 0) {
                target_encoder->setRenderPipelineState(renderer_state().image_pipeline);
                target_encoder->setVertexBuffer(renderer_state().uniform_buf, 0, 0);
                target_encoder->setVertexBuffer(
                    renderer_state().image_instances_buf, 0, 1);
                target_encoder->setFragmentTexture(
                    renderer_state().image_atlas_texture, 0);
                target_encoder->setFragmentSamplerState(renderer_state().sampler, 0);
                target_encoder->drawPrimitives(
                    MTL::PrimitiveTypeTriangle,
                    NS::UInteger(0), 6,
                    NS::UInteger(batch_image_count),
                    NS::UInteger(batch.image_first));
            }
        };

    renderer_state().last_material_backdrop_available = false;
    renderer_state().last_material_backdrop_excludes_foreground_text = false;
    if (capture_frame_history && ensure_material_backdrop_texture(fbw, fbh)) {
        auto* backdrop_pass = MTL::RenderPassDescriptor::renderPassDescriptor();
        backdrop_pass->colorAttachments()->object(0)->setTexture(
            renderer_state().material_backdrop_texture);
        backdrop_pass->colorAttachments()->object(0)->setLoadAction(
            MTL::LoadActionClear);
        backdrop_pass->colorAttachments()->object(0)->setClearColor(
            MTL::ClearColor::Make(cr, cg, cb, ca));
        backdrop_pass->colorAttachments()->object(0)->setStoreAction(
            MTL::StoreActionStore);
        if (auto* backdrop_encoder =
                command_buffer->renderCommandEncoder(backdrop_pass)) {
            for (std::size_t batch_index = 0;
                 batch_index < scratch.batches.size();
                 ++batch_index) {
                draw_backdrop_underlay_batch(
                    backdrop_encoder,
                    batch_index,
                    scratch.batches[batch_index]);
            }
            backdrop_encoder->endEncoding();
            if (auto* blit = command_buffer->blitCommandEncoder()) {
                (void)schedule_material_backdrop_luma_sample(
                    blit,
                    command_buffer,
                    renderer_state().material_backdrop_texture,
                    fbw,
                    fbh,
                    material_env.debug_seed.frame,
                    material_summary);
                blit->endEncoding();
            }
            renderer_state().last_material_backdrop_available = true;
            renderer_state().material_backdrop_theme_generation =
                current_theme_generation;
            renderer_state().last_material_backdrop_excludes_foreground_text = true;
            material_summary.backdrop_copy_excludes_foreground_text = true;
            material_summary.backdrop_copy_count = 1;
            material_summary.backdrop_copy_pixels =
                static_cast<std::int64_t>(fbw)
                * static_cast<std::int64_t>(fbh);
        } else {
            material_summary.backdrop_copy_skipped_count = 1;
            material_summary.backdrop_copy_skip_reason =
                "metal-backdrop-render-encoder-unavailable";
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

    auto* drawable = renderer_state().layer->nextDrawable();
    if (!drawable) {
        release_material_backdrop_luma_pending_command_buffer();
        pool->release();
        return;
    }

    auto* pass = MTL::RenderPassDescriptor::renderPassDescriptor();
    pass->colorAttachments()->object(0)->setTexture(drawable->texture());
    pass->colorAttachments()->object(0)->setLoadAction(MTL::LoadActionClear);
    pass->colorAttachments()->object(0)->setClearColor(
        MTL::ClearColor::Make(cr, cg, cb, ca));
    pass->colorAttachments()->object(0)->setStoreAction(MTL::StoreActionStore);

    auto* encoder = command_buffer->renderCommandEncoder(pass);
    if (!encoder) {
        release_material_backdrop_luma_pending_command_buffer();
        pool->release();
        return;
    }

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
            static_cast<std::uint32_t>(renderer_state().drawable_width),
            static_cast<std::uint32_t>(renderer_state().drawable_height));
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
            encoder->setRenderPipelineState(renderer_state().tri_pipeline);
            encoder->setVertexBuffer(renderer_state().uniform_buf, 0, 0);
            encoder->setVertexBuffer(
                renderer_state().tri_vertices_buf,
                NS::UInteger(batch.tri_first * sizeof(TriVertexGPU)),
                1);
            encoder->drawPrimitives(
                MTL::PrimitiveTypeTriangle,
                NS::UInteger(0),
                NS::UInteger(batch_tri_count));
        }

        if (color_uploaded && batch_color_count > 0) {
            encoder->setRenderPipelineState(renderer_state().color_pipeline);
            encoder->setVertexBuffer(renderer_state().uniform_buf, 0, 0);
            encoder->setVertexBuffer(renderer_state().color_instances_buf, 0, 1);
            encoder->drawPrimitives(
                MTL::PrimitiveTypeTriangle,
                NS::UInteger(0), 6,
                NS::UInteger(batch_color_count),
                NS::UInteger(batch.color_first));
        }
        if (material_uploaded && batch_material_count > 0
            && renderer_state().material_backdrop_texture
            && renderer_state().last_material_backdrop_available) {
            encoder->setRenderPipelineState(renderer_state().material_pipeline);
            encoder->setVertexBuffer(renderer_state().uniform_buf, 0, 0);
            encoder->setVertexBuffer(renderer_state().material_instances_buf, 0, 1);
            encoder->setFragmentTexture(renderer_state().material_backdrop_texture, 0);
            encoder->setFragmentSamplerState(renderer_state().sampler, 0);
            encoder->drawPrimitives(
                MTL::PrimitiveTypeTriangle,
                NS::UInteger(0), 6,
                NS::UInteger(batch_material_count),
                NS::UInteger(batch.material_first));
            ++material_summary.material_draw_calls;
        }
        if (arc_uploaded && batch_arc_count > 0) {
            encoder->setRenderPipelineState(renderer_state().arc_pipeline);
            encoder->setVertexBuffer(renderer_state().uniform_buf, 0, 0);
            encoder->setVertexBuffer(renderer_state().arc_instances_buf, 0, 1);
            encoder->drawPrimitives(
                MTL::PrimitiveTypeTriangle,
                NS::UInteger(0), 6,
                NS::UInteger(batch_arc_count),
                NS::UInteger(batch.arc_first));
        }
        if (image_uploaded && batch_image_count > 0) {
            encoder->setRenderPipelineState(renderer_state().image_pipeline);
            encoder->setVertexBuffer(renderer_state().uniform_buf, 0, 0);
            encoder->setVertexBuffer(renderer_state().image_instances_buf, 0, 1);
            encoder->setFragmentTexture(renderer_state().image_atlas_texture, 0);
            encoder->setFragmentSamplerState(renderer_state().sampler, 0);
            encoder->drawPrimitives(
                MTL::PrimitiveTypeTriangle,
                NS::UInteger(0), 6,
                NS::UInteger(batch_image_count),
                NS::UInteger(batch.image_first));
        }
        if (!defer_foreground_pass && text_uploaded && batch_text_count > 0) {
            encoder->setRenderPipelineState(renderer_state().text_pipeline);
            encoder->setVertexBuffer(renderer_state().uniform_buf, 0, 0);
            encoder->setVertexBuffer(renderer_state().text_instances_buf, 0, 1);
            encoder->setFragmentTexture(renderer_state().text_atlas_texture, 0);
            encoder->setFragmentSamplerState(renderer_state().sampler, 0);
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
        static_cast<NS::UInteger>(renderer_state().drawable_width),
        static_cast<NS::UInteger>(renderer_state().drawable_height)});

    auto const overlay_color_bytes =
        scratch.overlay_color_instances.size() * sizeof(ColorInstanceGPU);
    uint32_t overlay_color_count =
        static_cast<uint32_t>(scratch.overlay_color_instances.size());
    if (!defer_foreground_pass && overlay_color_count > 0) {
        if (!ensure_instance_buffer(
                renderer_state().overlay_color_instances_buf,
                renderer_state().overlay_color_instances_capacity,
                overlay_color_bytes,
                "overlay_color_instances")) {
            encoder->endEncoding();
            pool->release();
            return;
        }
        std::memcpy(
            renderer_state().overlay_color_instances_buf->contents(),
            scratch.overlay_color_instances.data(),
            overlay_color_bytes);
        encoder->setRenderPipelineState(renderer_state().color_pipeline);
        encoder->setVertexBuffer(renderer_state().uniform_buf, 0, 0);
        encoder->setVertexBuffer(renderer_state().overlay_color_instances_buf, 0, 1);
        encoder->drawPrimitives(
            MTL::PrimitiveTypeTriangle,
            NS::UInteger(0), 6, NS::UInteger(overlay_color_count));
    }

    if (!defer_foreground_pass && text_uploaded && scratch.overlay_text_count > 0) {
        encoder->setRenderPipelineState(renderer_state().text_pipeline);
        encoder->setVertexBuffer(renderer_state().uniform_buf, 0, 0);
        encoder->setVertexBuffer(renderer_state().text_instances_buf, 0, 1);
        encoder->setFragmentTexture(renderer_state().text_atlas_texture, 0);
        encoder->setFragmentSamplerState(renderer_state().sampler, 0);
        encoder->drawPrimitives(
            MTL::PrimitiveTypeTriangle,
            NS::UInteger(0), 6,
            NS::UInteger(scratch.overlay_text_count),
            NS::UInteger(scratch.overlay_text_first));
    }

    encoder->endEncoding();
    renderer_state().last_render_width = fbw;
    renderer_state().last_render_height = fbh;

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
                static_cast<std::uint32_t>(renderer_state().drawable_width),
                static_cast<std::uint32_t>(renderer_state().drawable_height));
            if (sr.width == 0 || sr.height == 0)
                continue;
            foreground_encoder->setScissorRect(sr);
            if (batch.text_command_indices.size() != batch.texts.size()) {
                draw_deferred_text_range(
                    foreground_encoder,
                    text_uploaded,
                    batch.text_first,
                    batch_text_count);
                continue;
            }

            std::uint32_t range_start = 0;
            std::uint32_t range_count = 0;
            auto flush_range = [&] {
                if (range_count == 0)
                    return;
                draw_deferred_text_range(
                    foreground_encoder,
                    text_uploaded,
                    batch.text_first + range_start,
                    range_count);
                range_start = 0;
                range_count = 0;
            };
            for (std::uint32_t i = 0; i < batch_text_count; ++i) {
                bool const occluded =
                    foreground_text_occluded_by_later_material(
                        batch.texts[i],
                        batch.text_command_indices[i],
                        scratch.material_records);
                if (occluded) {
                    flush_range();
                    continue;
                }
                if (range_count == 0)
                    range_start = i;
                ++range_count;
            }
            flush_range();
        }

        foreground_encoder->setScissorRect(MTL::ScissorRect{
            0, 0,
            static_cast<NS::UInteger>(renderer_state().drawable_width),
            static_cast<NS::UInteger>(renderer_state().drawable_height)});

        if (overlay_color_count > 0) {
            if (!ensure_instance_buffer(
                    renderer_state().overlay_color_instances_buf,
                    renderer_state().overlay_color_instances_capacity,
                    overlay_color_bytes,
                    "overlay_color_instances")) {
                foreground_encoder->endEncoding();
                release_material_backdrop_luma_pending_command_buffer();
                pool->release();
                return;
            }
            std::memcpy(
                renderer_state().overlay_color_instances_buf->contents(),
                scratch.overlay_color_instances.data(),
                overlay_color_bytes);
            foreground_encoder->setRenderPipelineState(renderer_state().color_pipeline);
            foreground_encoder->setVertexBuffer(renderer_state().uniform_buf, 0, 0);
            foreground_encoder->setVertexBuffer(
                renderer_state().overlay_color_instances_buf, 0, 1);
            foreground_encoder->drawPrimitives(
                MTL::PrimitiveTypeTriangle,
                NS::UInteger(0), 6, NS::UInteger(overlay_color_count));
        }

        if (text_uploaded && scratch.overlay_text_count > 0) {
            foreground_encoder->setRenderPipelineState(renderer_state().text_pipeline);
            foreground_encoder->setVertexBuffer(renderer_state().uniform_buf, 0, 0);
            foreground_encoder->setVertexBuffer(renderer_state().text_instances_buf, 0, 1);
            foreground_encoder->setFragmentTexture(renderer_state().text_atlas_texture, 0);
            foreground_encoder->setFragmentSamplerState(renderer_state().sampler, 0);
            foreground_encoder->drawPrimitives(
                MTL::PrimitiveTypeTriangle,
                NS::UInteger(0), 6,
                NS::UInteger(scratch.overlay_text_count),
                NS::UInteger(scratch.overlay_text_first));
        }

        foreground_encoder->endEncoding();
        material_summary.foreground_pass_after_backdrop_copy = true;
    }

    bool const capture_debug_frame =
        renderer_state().debug_capture_always || renderer_state().debug_capture_next_frame;
    renderer_state().last_frame_available = false;
    if (capture_debug_frame && ensure_debug_capture_texture(fbw, fbh)) {
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
                renderer_state().debug_capture_texture,
                NS::UInteger(0),
                NS::UInteger(0),
                origin);
            blit->endEncoding();
            renderer_state().last_frame_available = true;
            ++renderer_state().debug_capture_copy_count;
        }
    }
    renderer_state().debug_capture_next_frame = false;
    command_buffer->presentDrawable(drawable);
    command_buffer->commit();
    if (auto* host = ::phenotype::native::detail::active_host())
        ::phenotype::native::detail::note_host_render_surface_frame(*host);
    material_summary.cpu_total_ns =
        metrics::detail::now_ns() - flush_started;
    finalize_material_executor_execution_status(material_summary);
    renderer_state().material_executor_summary = material_summary;

    pool->release();
}

inline void renderer_shutdown_active_state() {
    release_material_backdrop_luma_pending_command_buffer();
    if (renderer_state().sampler) { renderer_state().sampler->release(); renderer_state().sampler = nullptr; }
    if (renderer_state().debug_capture_texture) { renderer_state().debug_capture_texture->release(); renderer_state().debug_capture_texture = nullptr; }
    if (renderer_state().material_backdrop_texture) { renderer_state().material_backdrop_texture->release(); renderer_state().material_backdrop_texture = nullptr; }
    if (renderer_state().material_backdrop_luma_sample_buf) { renderer_state().material_backdrop_luma_sample_buf->release(); renderer_state().material_backdrop_luma_sample_buf = nullptr; }
    renderer_state().last_frame_available = false;
    renderer_state().debug_capture_next_frame = false;
    renderer_state().debug_capture_always = false;
    renderer_state().last_material_backdrop_available = false;
    renderer_state().material_backdrop_theme_generation = 0;
    renderer_state().last_material_backdrop_excludes_foreground_text = false;
    renderer_state().last_material_backdrop_color_available = false;
    renderer_state().last_material_backdrop_color_mean = {255, 255, 255, 255};
    renderer_state().last_material_backdrop_luma_available = false;
    if (renderer_state().frame_readback_buf) { renderer_state().frame_readback_buf->release(); renderer_state().frame_readback_buf = nullptr; }
    if (renderer_state().image_atlas_texture) { renderer_state().image_atlas_texture->release(); renderer_state().image_atlas_texture = nullptr; }
    if (renderer_state().text_atlas_texture) { renderer_state().text_atlas_texture->release(); renderer_state().text_atlas_texture = nullptr; }
    if (renderer_state().image_instances_buf) { renderer_state().image_instances_buf->release(); renderer_state().image_instances_buf = nullptr; }
    if (renderer_state().text_instances_buf) { renderer_state().text_instances_buf->release(); renderer_state().text_instances_buf = nullptr; }
    if (renderer_state().color_instances_buf) { renderer_state().color_instances_buf->release(); renderer_state().color_instances_buf = nullptr; }
    if (renderer_state().material_instances_buf) { renderer_state().material_instances_buf->release(); renderer_state().material_instances_buf = nullptr; }
    if (renderer_state().tri_vertices_buf) { renderer_state().tri_vertices_buf->release(); renderer_state().tri_vertices_buf = nullptr; }
    if (renderer_state().arc_instances_buf) { renderer_state().arc_instances_buf->release(); renderer_state().arc_instances_buf = nullptr; }
    if (renderer_state().overlay_color_instances_buf) { renderer_state().overlay_color_instances_buf->release(); renderer_state().overlay_color_instances_buf = nullptr; }
    if (renderer_state().uniform_buf) { renderer_state().uniform_buf->release(); renderer_state().uniform_buf = nullptr; }
    if (renderer_state().image_pipeline) { renderer_state().image_pipeline->release(); renderer_state().image_pipeline = nullptr; }
    if (renderer_state().text_pipeline) { renderer_state().text_pipeline->release(); renderer_state().text_pipeline = nullptr; }
    if (renderer_state().arc_pipeline) { renderer_state().arc_pipeline->release(); renderer_state().arc_pipeline = nullptr; }
    if (renderer_state().material_pipeline) { renderer_state().material_pipeline->release(); renderer_state().material_pipeline = nullptr; }
    if (renderer_state().color_pipeline) { renderer_state().color_pipeline->release(); renderer_state().color_pipeline = nullptr; }
    if (renderer_state().tri_pipeline) { renderer_state().tri_pipeline->release(); renderer_state().tri_pipeline = nullptr; }
    if (renderer_state().layer) { renderer_state().layer->release(); renderer_state().layer = nullptr; }
    if (renderer_state().queue) { renderer_state().queue->release(); renderer_state().queue = nullptr; }
    if (renderer_state().device) { renderer_state().device->release(); renderer_state().device = nullptr; }
    renderer_state().hit_regions.clear();
    renderer_state().scratch.clear();
    reset_image_cache();
    renderer_state().text_cache.entries.clear();
    renderer_state().text_cache.pixels.clear();
    renderer_state().text_cache.cursor_x = 0;
    renderer_state().text_cache.cursor_y = 0;
    renderer_state().text_cache.row_height = 0;
    renderer_state().text_cache.dirty = false;
    renderer_state().text_cache.dirty_min_x = TextAtlasCache::atlas_size;
    renderer_state().text_cache.dirty_min_y = TextAtlasCache::atlas_size;
    renderer_state().text_cache.dirty_max_x = 0;
    renderer_state().text_cache.dirty_max_y = 0;
    renderer_state().text_cache.active_scale_key = 0;
    renderer_state().tri_vertices_capacity = 0;
    renderer_state().color_instances_capacity = 0;
    renderer_state().material_instances_capacity = 0;
    renderer_state().overlay_color_instances_capacity = 0;
    renderer_state().image_instances_capacity = 0;
    renderer_state().text_instances_capacity = 0;
    renderer_state().image_atlas_uploaded_generation = 0;
    renderer_state().frame_readback_capacity = 0;
    renderer_state().material_backdrop_luma_sample_capacity = 0;
    renderer_state().drawable_width = 0;
    renderer_state().drawable_height = 0;
    renderer_state().debug_capture_width = 0;
    renderer_state().debug_capture_height = 0;
    renderer_state().debug_capture_copy_count = 0;
    renderer_state().material_backdrop_width = 0;
    renderer_state().material_backdrop_height = 0;
    renderer_state().layer_contents_scale = 0.0;
    renderer_state().last_render_width = 0;
    renderer_state().last_render_height = 0;
    renderer_state().last_clear_alpha = 1.0;
    renderer_state().last_clear_alpha_for_transparent_window = false;
    renderer_state().last_full_frame_opaque_fill_count = 0;
    renderer_state().last_transparent_window_has_opaque_frame_fill = false;
    renderer_state().material_frame_sequence = 0;
    renderer_state().material_backdrop_luma_skipped_sample_count = 0;
    renderer_state().last_material_backdrop_luma_sample_count = 0;
    renderer_state().last_material_backdrop_luma_grid_width = 0;
    renderer_state().last_material_backdrop_luma_grid_height = 0;
    renderer_state().last_material_backdrop_luma_frame = 0;
    renderer_state().last_material_backdrop_luma_status = "not-sampled";
    renderer_state().material_executor_summary = MaterialExecutorSummary{};
    renderer_state().last_frame_available = false;
    renderer_state().surface = nullptr;
    renderer_state().ns_window = nullptr;
    renderer_state().content_view = nullptr;
    renderer_state().initialized = false;
}

inline void renderer_shutdown() {
    reset_active_renderer_state();
    renderer_shutdown_active_state();
    for (auto& state : renderer_registry()) {
        if (!state)
            continue;
        bind_renderer_state(*state);
        renderer_shutdown_active_state();
    }
    renderer_registry().clear();
    reset_active_renderer_state();
}

inline std::optional<unsigned int> renderer_hit_test(float x, float y,
                                                     float scroll_x,
                                                     float scroll_y) {
    float wx = x + scroll_x;
    float wy = y + scroll_y;
    for (int i = static_cast<int>(renderer_state().hit_regions.size()) - 1; i >= 0; --i) {
        auto const& hr = renderer_state().hit_regions[static_cast<std::size_t>(i)];
        if (wx >= hr.x && wx <= hr.x + hr.w && wy >= hr.y && wy <= hr.y + hr.h)
            return hr.callback_id;
    }
    return std::nullopt;
}

} // namespace phenotype::native::detail
#endif

export namespace phenotype::native::macos_test {
#ifdef __APPLE__
inline void reset_renderer_surface_states() {
    detail::renderer_shutdown();
}

inline void activate_renderer_surface_state(NativeSurfaceDescriptor* surface) {
    detail::activate_renderer_state(surface);
}

inline std::size_t renderer_surface_state_count() {
    return 1 + detail::renderer_registry().size();
}

inline bool active_renderer_surface_is(NativeSurfaceDescriptor* surface) {
    return detail::renderer_state().surface == surface;
}

template<typename F>
inline void with_renderer_surface_state(NativeSurfaceDescriptor* surface, F&& fn) {
    detail::ScopedRendererActivation activate(surface);
    fn();
}

inline void* active_renderer_state_identity() {
    return &detail::renderer_state();
}

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
    return detail::active_ime().last_scroll_event.available;
}

inline std::string last_scroll_event_source_for_tests() {
    return detail::active_ime().last_scroll_event.source;
}

inline float last_scroll_event_normalized_y_for_tests() {
    return detail::active_ime().last_scroll_event.normalized_delta_y;
}

inline float last_scroll_event_vertical_multiplier_for_tests() {
    return detail::active_ime().last_scroll_event.app_vertical_multiplier;
}

inline bool last_scroll_event_handled_y_for_tests() {
    return detail::active_ime().last_scroll_event.handled_y;
}

inline double renderer_layer_contents_scale_for_tests() {
    return detail::renderer_state().layer_contents_scale;
}

inline double renderer_layer_reported_contents_scale_for_tests() {
    if (!detail::renderer_state().layer)
        return 0.0;
    return reinterpret_cast<double(*)(void*, SEL)>(objc_msgSend)(
        reinterpret_cast<void*>(detail::renderer_state().layer),
        sel_registerName("contentsScale"));
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

inline std::size_t input_surface_state_count_for_tests() {
    return detail::ime_registry().live_count();
}

inline bool input_surface_registered_for_tests(NativeSurfaceDescriptor* surface) {
    return detail::ime_registry().find(surface) != nullptr;
}

inline bool active_input_surface_is_for_tests(NativeSurfaceDescriptor* surface) {
    return detail::active_ime().surface == surface;
}

inline void set_scroll_tracking_for_tests(bool active) {
    detail::active_ime().scroll_tracking_active = active;
}

inline void reset_image_cache_for_tests() {
    detail::reset_image_cache(true);
}

inline void reset_image_repaint_targets_for_tests() {
    detail::clear_image_repaint_targets();
}

inline void register_image_repaint_target_for_tests(
        void* surface,
        void (*request_repaint)()) {
    detail::register_image_repaint_target(surface, request_repaint);
}

inline void unregister_image_repaint_target_for_tests(void* surface) {
    detail::unregister_image_repaint_target(surface);
}

inline std::size_t image_repaint_target_count_for_tests() {
    return detail::image_repaint_target_count();
}

inline bool image_repaint_target_registered_for_tests(void* surface) {
    return detail::image_repaint_target_registered(surface);
}

inline void request_image_repaint_targets_for_tests() {
    detail::request_image_repaint_for_all_targets();
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

inline std::uint64_t image_cache_generation() {
    return detail::g_images.generation;
}

inline std::uint64_t active_renderer_image_upload_generation() {
    return detail::renderer_state().image_atlas_uploaded_generation;
}

inline bool active_renderer_needs_image_atlas_upload() {
    auto const generation = detail::g_images.generation;
    return generation != 0
        && detail::renderer_state().image_atlas_uploaded_generation
            != generation;
}

inline void bump_image_cache_generation_for_tests() {
    ++detail::g_images.generation;
}

inline void mark_active_renderer_image_atlas_uploaded_for_tests() {
    detail::renderer_state().image_atlas_uploaded_generation =
        detail::g_images.generation;
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
    debug.attached = detail::active_ime().insertion_indicator != nullptr;
    debug.scroll_tracking_active = detail::active_ime().scroll_tracking_active;
    debug.host_flipped = detail::caret_host_view_is_flipped();
    debug.draw_rect = detail::rect_snapshot(detail::active_ime().last_character_rect_draw);
    debug.host_rect = detail::rect_snapshot(detail::active_ime().last_character_rect_host);
    debug.host_bounds = detail::rect_snapshot(detail::current_caret_host_bounds());
    if (detail::active_ime().insertion_indicator) {
        auto frame = detail::objc_send<CGRect>(
            detail::active_ime().insertion_indicator,
            detail::sel_frame());
        debug.display_mode = detail::objc_send<long long>(
            detail::active_ime().insertion_indicator,
            detail::sel_display_mode());
        debug.automatic_mode_options = detail::objc_send<long long>(
            detail::active_ime().insertion_indicator,
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
    detail::active_ime().marked_text = std::move(marked_text);
    detail::active_ime().composition_active = !detail::active_ime().marked_text.empty();
    detail::active_ime().composition_anchor = replacement_start;
    detail::active_ime().replacement_start = replacement_start;
    detail::active_ime().replacement_end = replacement_end;
    detail::active_ime().marked_selection = {
        selected_location_utf16,
        0,
    };
    ::phenotype::detail::set_input_composition_state(
        detail::active_ime().composition_active,
        detail::active_ime().marked_text,
        static_cast<unsigned int>(
            detail::utf16_offset_to_utf8(detail::active_ime().marked_text, selected_location_utf16)));
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
    bool const material_pipeline_ready = renderer_state().material_pipeline != nullptr;
    bool const material_frame_history =
        renderer_state().material_backdrop_texture != nullptr
        && renderer_state().last_material_backdrop_available;
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
        renderer_state().device,
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
        renderer_state().scratch.material_records);
}

inline json::Object macos_metal_capabilities_json() {
    json::Object metal;
    auto* device = renderer_state().device;
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
    renderer.emplace("initialized", json::Value{renderer_state().initialized});
    renderer.emplace(
        "metal_capabilities",
        json::Value{macos_metal_capabilities_json()});
    renderer.emplace(
        "drawable_width",
        json::Value{static_cast<std::int64_t>(renderer_state().drawable_width)});
    renderer.emplace(
        "drawable_height",
        json::Value{static_cast<std::int64_t>(renderer_state().drawable_height)});
    renderer.emplace(
        "last_render_width",
        json::Value{static_cast<std::int64_t>(renderer_state().last_render_width)});
    renderer.emplace(
        "last_render_height",
        json::Value{static_cast<std::int64_t>(renderer_state().last_render_height)});
    renderer.emplace(
        "last_frame_available",
        json::Value{renderer_state().last_frame_available});
    renderer.emplace(
        "debug_capture_policy",
        json::Value{
            renderer_state().debug_capture_always
                ? "every-frame"
                : "on-demand"});
    renderer.emplace(
        "debug_capture_pending",
        json::Value{renderer_state().debug_capture_next_frame});
    renderer.emplace(
        "debug_capture_copy_count",
        json::Value{static_cast<std::int64_t>(
            renderer_state().debug_capture_copy_count)});
    renderer.emplace(
        "clear_alpha",
        json::Value{renderer_state().last_clear_alpha});
    renderer.emplace(
        "clear_alpha_for_transparent_window",
        json::Value{renderer_state().last_clear_alpha_for_transparent_window});
    renderer.emplace(
        "full_frame_opaque_fill_count",
        json::Value{
            static_cast<std::int64_t>(
                renderer_state().last_full_frame_opaque_fill_count)});
    renderer.emplace(
        "transparent_window_has_opaque_frame_fill",
        json::Value{
            renderer_state().last_transparent_window_has_opaque_frame_fill});
    renderer.emplace(
        "readback_texture_ready",
        json::Value{renderer_state().debug_capture_texture != nullptr});
    renderer.emplace(
        "material_pipeline_ready",
        json::Value{renderer_state().material_pipeline != nullptr});
    renderer.emplace(
        "material_backdrop_source_ready",
        json::Value{
            renderer_state().material_backdrop_texture != nullptr
            && renderer_state().last_material_backdrop_available});
    renderer.emplace(
        "material_backdrop_excludes_foreground_text",
        json::Value{
            renderer_state().material_backdrop_texture != nullptr
            && renderer_state().last_material_backdrop_available
            && renderer_state().last_material_backdrop_excludes_foreground_text});
    json::Object luma_descriptor;
    luma_descriptor.emplace(
        "available",
        json::Value{renderer_state().last_material_backdrop_luma_available});
    luma_descriptor.emplace(
        "color_available",
        json::Value{renderer_state().last_material_backdrop_color_available});
    json::Object color_mean;
    color_mean.emplace(
        "r",
        json::Value{static_cast<std::int64_t>(
            renderer_state().last_material_backdrop_color_mean.r)});
    color_mean.emplace(
        "g",
        json::Value{static_cast<std::int64_t>(
            renderer_state().last_material_backdrop_color_mean.g)});
    color_mean.emplace(
        "b",
        json::Value{static_cast<std::int64_t>(
            renderer_state().last_material_backdrop_color_mean.b)});
    color_mean.emplace(
        "a",
        json::Value{static_cast<std::int64_t>(
            renderer_state().last_material_backdrop_color_mean.a)});
    luma_descriptor.emplace(
        "color_mean",
        json::Value{std::move(color_mean)});
    luma_descriptor.emplace(
        "luma_min",
        json::Value{renderer_state().last_material_backdrop_luma_min});
    luma_descriptor.emplace(
        "luma_max",
        json::Value{renderer_state().last_material_backdrop_luma_max});
    luma_descriptor.emplace(
        "luma_mean",
        json::Value{renderer_state().last_material_backdrop_luma_mean});
    luma_descriptor.emplace(
        "sample_count",
        json::Value{static_cast<std::int64_t>(
            renderer_state().last_material_backdrop_luma_sample_count)});
    luma_descriptor.emplace(
        "sample_grid_width",
        json::Value{static_cast<std::int64_t>(
            renderer_state().last_material_backdrop_luma_grid_width)});
    luma_descriptor.emplace(
        "sample_grid_height",
        json::Value{static_cast<std::int64_t>(
            renderer_state().last_material_backdrop_luma_grid_height)});
    luma_descriptor.emplace(
        "sample_frame",
        json::Value{static_cast<std::int64_t>(
            renderer_state().last_material_backdrop_luma_frame)});
    luma_descriptor.emplace(
        "status",
        json::Value{renderer_state().last_material_backdrop_luma_status
                        ? renderer_state().last_material_backdrop_luma_status
                        : "not-sampled"});
    luma_descriptor.emplace(
        "pending",
        json::Value{
            renderer_state().material_backdrop_luma_pending_command_buffer != nullptr});
    luma_descriptor.emplace(
        "skipped_sample_count",
        json::Value{static_cast<std::int64_t>(
            renderer_state().material_backdrop_luma_skipped_sample_count)});
    renderer.emplace(
        "material_backdrop_luma_descriptor",
        json::Value{std::move(luma_descriptor)});
    renderer.emplace(
        "readback_buffer_ready",
        json::Value{renderer_state().frame_readback_buf != nullptr});
    json::Object accessibility;
    accessibility.emplace(
        "source",
        json::Value{renderer_state().accessibility_options.source});
    accessibility.emplace(
        "reduce_transparency",
        json::Value{renderer_state().accessibility_options.reduce_transparency});
    accessibility.emplace(
        "increase_contrast",
        json::Value{renderer_state().accessibility_options.increase_contrast});
    accessibility.emplace(
        "reduce_motion",
        json::Value{renderer_state().accessibility_options.reduce_motion});
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
                renderer_state().scratch.material_records.size())});
    renderer.emplace(
        "material_plans",
        json::Value{material_plans_runtime_json()});
    renderer.emplace(
        "material_container_groups",
        json::Value{
            ::phenotype::diag::detail::material_container_group_details_json(
                renderer_state().scratch.material_records)});
    renderer.emplace(
        "material_runtime_summary",
        ::phenotype::diag::detail::material_runtime_summary_json(
            renderer_state().scratch.material_records));
    renderer.emplace(
        "material_executor_summary",
        ::phenotype::diag::detail::material_executor_summary_json(
            renderer_state().material_executor_summary));
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
        "atlas_generation",
        json::Value{static_cast<std::int64_t>(g_images.generation)});
    images.emplace(
        "active_surface_uploaded_generation",
        json::Value{
            static_cast<std::int64_t>(
                renderer_state().image_atlas_uploaded_generation)});
    images.emplace(
        "repaint_target_count",
        json::Value{
            static_cast<std::int64_t>(image_repaint_target_count())});
    images.emplace(
        "remote_entry_count",
        json::Value{static_cast<std::int64_t>(remote_entries.size())});
    images.emplace("remote_entries", json::Value{std::move(remote_entries)});
    return images;
}

inline json::Object macos_text_input_runtime_json() {
    auto caret = ::phenotype::native::macos_test::system_caret_debug();
    auto input_debug = ::phenotype::diag::input_debug_snapshot();
    auto composition_active = active_ime().composition_active && !active_ime().marked_text.empty();

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
        json::Value{static_cast<std::int64_t>(active_ime().composition_anchor)});
    composition.emplace(
        "replacement_start",
        json::Value{static_cast<std::int64_t>(active_ime().replacement_start)});
    composition.emplace(
        "replacement_end",
        json::Value{static_cast<std::int64_t>(active_ime().replacement_end)});
    composition.emplace(
        "marked_selection_location_utf16",
        json::Value{static_cast<std::int64_t>(active_ime().marked_selection.location)});
    composition.emplace(
        "marked_selection_length_utf16",
        json::Value{static_cast<std::int64_t>(active_ime().marked_selection.length)});

    json::Object text_input;
    text_input.emplace(
        "surface_state_count",
        json::Value{static_cast<std::int64_t>(ime_registry().live_count())});
    text_input.emplace(
        "active_surface_attached",
        json::Value{active_ime().surface != nullptr});
    text_input.emplace(
        "scroll_tracking_active",
        json::Value{active_ime().scroll_tracking_active});
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
    auto const& scroll = active_ime().last_scroll_event;
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
    auto const* surface = renderer_state().surface;
    auto ns_window = renderer_state().ns_window;
    auto ns_app = objc_send<id>(
        class_as_id(objc_getClass("NSApplication")),
        sel_shared_application());
    id content_view = renderer_state().content_view
        ? renderer_state().content_view
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
    bool const metal_layer_opaque = renderer_state().layer
        && objc_send<bool>(
            reinterpret_cast<id>(renderer_state().layer),
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
        renderer_state().last_clear_alpha == 0.0;
    bool const renderer_clear_for_transparent_window =
        renderer_state().last_clear_alpha_for_transparent_window;
    bool const renderer_has_full_frame_opaque_fill =
        renderer_state().last_full_frame_opaque_fill_count != 0
        || renderer_state().last_transparent_window_has_opaque_frame_fill;
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
    if (renderer_state().initialized
        && !renderer_state().last_frame_available
        && ::phenotype::native::detail::active_host()) {
        request_debug_capture_next_frame();
        ::phenotype::detail::invalidate_active_render_surface_paint_cache();
        ::phenotype::native::detail::repaint_current();
    }
    if (!renderer_state().initialized
        || !renderer_state().last_frame_available
        || !renderer_state().debug_capture_texture
        || !renderer_state().device
        || !renderer_state().queue) {
        return std::nullopt;
    }

    auto width = renderer_state().last_render_width;
    auto height = renderer_state().last_render_height;
    if (width <= 0 || height <= 0)
        return std::nullopt;

    auto const bytes_per_row =
        static_cast<std::size_t>(width) * sizeof(std::uint32_t);
    auto const total_size = bytes_per_row * static_cast<std::size_t>(height);
    if (!ensure_frame_readback_buffer(total_size))
        return std::nullopt;

    NS::AutoreleasePool* pool = NS::AutoreleasePool::alloc()->init();
    auto* command_buffer = renderer_state().queue->commandBuffer();
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
        renderer_state().debug_capture_texture,
        NS::UInteger(0),
        NS::UInteger(0),
        origin,
        size,
        renderer_state().frame_readback_buf,
        NS::UInteger(0),
        NS::UInteger(bytes_per_row),
        NS::UInteger(total_size));
    blit->endEncoding();

    command_buffer->commit();
    command_buffer->waitUntilCompleted();

    auto const* mapped =
        static_cast<std::uint8_t const*>(renderer_state().frame_readback_buf->contents());
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
    if (renderer_state().initialized && ::phenotype::native::detail::active_host()) {
        request_debug_capture_next_frame();
        ::phenotype::detail::invalidate_active_render_surface_paint_cache();
        ::phenotype::native::detail::repaint_current();
    }
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

#ifdef __APPLE__
namespace detail {

inline float macos_shell_scroll_delta_y(
        double scrolling_delta_y,
        bool has_precise_scrolling_deltas,
        float line_height,
        float) {
    return macos_normalize_scroll_delta(
        scrolling_delta_y,
        has_precise_scrolling_deltas,
        line_height);
}

inline float macos_shell_scroll_delta_x(
        double scrolling_delta_x,
        bool has_precise_scrolling_deltas,
        float line_height,
        float) {
    return macos_normalize_scroll_delta(
        scrolling_delta_x,
        has_precise_scrolling_deltas,
        line_height);
}

} // namespace detail
#endif

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
            detail::renderer_activate,
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
            detail::macos_shell_scroll_delta_y,
            detail::macos_shell_scroll_delta_x,
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
