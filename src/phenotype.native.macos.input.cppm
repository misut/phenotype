module;
#if !defined(__wasi__) && !defined(__ANDROID__) && defined(__APPLE__)
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <Foundation/Foundation.hpp>
#include <CoreText/CoreText.h>
#include <CoreGraphics/CoreGraphics.h>
#include <objc/message.h>
#include <objc/runtime.h>
#endif

export module phenotype.native.macos.input;

#if !defined(__wasi__) && !defined(__ANDROID__) && defined(__APPLE__)
import cppx.unicode;
import phenotype;
import phenotype.diag;
import phenotype.native.platform;
import phenotype.native.shell;
import phenotype.native.macos.objc;
import phenotype.native.macos.text;
import phenotype.native.macos.image;
import phenotype.native.macos.render;
import phenotype.native.macos.commands;
import phenotype.types;

export namespace phenotype::native::detail {

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
    NativeSurfaceDescriptor* surface = nullptr;
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
        && ::phenotype::detail::g_app().gesture_target_id != 0xFFFFFFFFu) {
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
    float cursor_x = shell_state().last_mouse_x;
    float cursor_y = shell_state().last_mouse_y;
    bool const has_cursor = event_cursor_position(event, cursor_x, cursor_y);
    if (has_cursor) {
        shell_state().last_mouse_x = cursor_x;
        shell_state().last_mouse_y = cursor_y;
    }
    if (normalized_delta != 0.0f) {
        handled_y = dispatch_scroll_pixels_at_cursor(
            normalized_delta,
            viewport_height_value,
            has_precise_scrolling_deltas ? "wheel-precise" : "wheel-line",
            cursor_x,
            cursor_y);
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
    register_image_repaint_target(surface, request_repaint);
    g_ime.surface = surface;
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
    unregister_image_repaint_target(g_ime.surface);
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
    g_ime.surface = nullptr;
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
    bool image_repaint_needed = process_completed_images();
    bool input_repaint_needed = false;
    auto snapshot = ::phenotype::detail::focused_input_snapshot();

    if (!snapshot.valid) {
        if (g_ime.composition_active || !g_ime.marked_text.empty()) {
            discard_marked_text_from_system();
            clear_ime_state();
            input_repaint_needed = true;
        }
        g_ime.focused_callback_id = ::phenotype::native::invalid_callback_id;
        auto presentation_changed = sync_caret_presentation(snapshot);
        if (editor_is_first_responder())
            restore_content_view_first_responder();
        if (presentation_changed)
            sync_input_context_geometry();
        if (image_repaint_needed)
            request_image_repaint_for_all_targets();
        if (input_repaint_needed && !image_repaint_needed)
            request_window_repaint();
        return;
    }

    ensure_editor_view();
    ensure_system_insertion_indicator();
    auto focus_changed = snapshot.callback_id != g_ime.focused_callback_id;
    if (focus_changed && (g_ime.composition_active || !g_ime.marked_text.empty())) {
        discard_marked_text_from_system();
        clear_ime_state();
        input_repaint_needed = true;
    }

    g_ime.focused_callback_id = snapshot.callback_id;
    if (!editor_is_first_responder())
        focus_editor_view();
    auto presentation_changed = sync_caret_presentation(snapshot);
    if (focus_changed || g_ime.composition_active || presentation_changed)
        sync_input_context_geometry();

    if (image_repaint_needed)
        request_image_repaint_for_all_targets();
    if (input_repaint_needed && !image_repaint_needed)
        request_window_repaint();
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

} // namespace phenotype::native::detail
#endif
