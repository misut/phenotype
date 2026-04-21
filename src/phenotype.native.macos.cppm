module;
#if !defined(__wasi__) && !defined(__ANDROID__)
#include <algorithm>
#include <array>
#include <cmath>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <deque>
#include <filesystem>
#include <limits>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <variant>
#include <vector>

#include <GLFW/glfw3.h>

#ifdef __APPLE__
#define GLFW_EXPOSE_NATIVE_COCOA
#include <GLFW/glfw3native.h>

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
import cppx.http;
import cppx.http.system;
import cppx.os;
import cppx.os.system;
import cppx.resource;
import cppx.unicode;
import json;
import phenotype;
import phenotype.commands;
import phenotype.diag;
import phenotype.state;
import phenotype.native.platform;
import phenotype.native.shell;
import phenotype.types;

#ifdef __APPLE__
namespace phenotype::native::detail {

template<typename T>
struct CFGuard {
    T ref;
    explicit CFGuard(T r) : ref(r) {}
    ~CFGuard() { if (ref) CFRelease(ref); }
    CFGuard(CFGuard const&) = delete;
    CFGuard& operator=(CFGuard const&) = delete;
    operator T() const { return ref; }
    explicit operator bool() const { return ref != nullptr; }
};

struct TextState {
    CTFontRef sans = nullptr;
    CTFontRef mono = nullptr;
    bool initialized = false;
};

inline TextState g_text;

struct CocoaRange {
    unsigned long location = 0;
    unsigned long length = 0;
};

inline constexpr unsigned long cocoa_not_found =
    std::numeric_limits<unsigned long>::max();

template<typename Ret, typename... Args>
inline Ret objc_send(id obj, SEL sel, Args... args) {
    return reinterpret_cast<Ret (*)(id, SEL, Args...)>(objc_msgSend)(
        obj, sel, args...);
}

inline id class_as_id(Class type) {
    return reinterpret_cast<id>(type);
}

template<typename T>
inline id cf_as_id(T value) {
    return reinterpret_cast<id>(const_cast<void*>(reinterpret_cast<void const*>(value)));
}

template<typename T>
inline T id_as_cf(id value) {
    return reinterpret_cast<T>(value);
}

template<typename Ret, typename... Args>
inline Ret objc_send_super(id obj, Class super_class, SEL sel, Args... args) {
    objc_super super{
        .receiver = obj,
        .super_class = super_class,
    };
    return reinterpret_cast<Ret (*)(objc_super*, SEL, Args...)>(objc_msgSendSuper)(
        &super, sel, args...);
}

inline SEL sel_accepts_first_responder() {
    static auto sel = sel_registerName("acceptsFirstResponder");
    return sel;
}

inline SEL sel_add_subview() {
    static auto sel = sel_registerName("addSubview:");
    return sel;
}

inline SEL sel_alloc() {
    static auto sel = sel_registerName("alloc");
    return sel;
}

inline SEL sel_array() {
    static auto sel = sel_registerName("array");
    return sel;
}

inline SEL sel_array_with_object() {
    static auto sel = sel_registerName("arrayWithObject:");
    return sel;
}

inline SEL sel_autorelease() {
    static auto sel = sel_registerName("autorelease");
    return sel;
}

inline SEL sel_bounds() {
    static auto sel = sel_registerName("bounds");
    return sel;
}

inline SEL sel_add_local_monitor_for_events_matching_mask_handler() {
    static auto sel = sel_registerName("addLocalMonitorForEventsMatchingMask:handler:");
    return sel;
}

inline SEL sel_cancel_operation() {
    static auto sel = sel_registerName("cancelOperation:");
    return sel;
}

inline SEL sel_character_index_for_point() {
    static auto sel = sel_registerName("characterIndexForPoint:");
    return sel;
}

inline SEL sel_convert_rect_to_screen() {
    static auto sel = sel_registerName("convertRectToScreen:");
    return sel;
}

inline SEL sel_color_using_color_space() {
    static auto sel = sel_registerName("colorUsingColorSpace:");
    return sel;
}

inline SEL sel_convert_rect_to_view() {
    static auto sel = sel_registerName("convertRect:toView:");
    return sel;
}

inline SEL sel_content_view() {
    static auto sel = sel_registerName("contentView");
    return sel;
}

inline SEL sel_conversation_identifier() {
    static auto sel = sel_registerName("conversationIdentifier");
    return sel;
}

inline SEL sel_discard_marked_text() {
    static auto sel = sel_registerName("discardMarkedText");
    return sel;
}

inline SEL sel_do_command_by_selector() {
    static auto sel = sel_registerName("doCommandBySelector:");
    return sel;
}

inline SEL sel_first_rect_for_character_range() {
    static auto sel = sel_registerName("firstRectForCharacterRange:actualRange:");
    return sel;
}

inline SEL sel_first_responder() {
    static auto sel = sel_registerName("firstResponder");
    return sel;
}

inline SEL sel_has_marked_text() {
    static auto sel = sel_registerName("hasMarkedText");
    return sel;
}

inline SEL sel_has_precise_scrolling_deltas() {
    static auto sel = sel_registerName("hasPreciseScrollingDeltas");
    return sel;
}

inline SEL sel_hit_test() {
    static auto sel = sel_registerName("hitTest:");
    return sel;
}

inline SEL sel_handle_event() {
    static auto sel = sel_registerName("handleEvent:");
    return sel;
}

inline SEL sel_input_context() {
    static auto sel = sel_registerName("inputContext");
    return sel;
}

inline SEL sel_init_with_frame() {
    static auto sel = sel_registerName("initWithFrame:");
    return sel;
}

inline SEL sel_insert_newline() {
    static auto sel = sel_registerName("insertNewline:");
    return sel;
}

inline SEL sel_insert_tab() {
    static auto sel = sel_registerName("insertTab:");
    return sel;
}

inline SEL sel_insert_backtab() {
    static auto sel = sel_registerName("insertBacktab:");
    return sel;
}

inline SEL sel_interpret_key_events() {
    static auto sel = sel_registerName("interpretKeyEvents:");
    return sel;
}

inline SEL sel_invalidate_character_coordinates() {
    static auto sel = sel_registerName("invalidateCharacterCoordinates");
    return sel;
}

inline SEL sel_is_flipped() {
    static auto sel = sel_registerName("isFlipped");
    return sel;
}

inline SEL sel_key_code() {
    static auto sel = sel_registerName("keyCode");
    return sel;
}

inline SEL sel_key_down() {
    static auto sel = sel_registerName("keyDown:");
    return sel;
}

inline SEL sel_length() {
    static auto sel = sel_registerName("length");
    return sel;
}

inline SEL sel_make_first_responder() {
    static auto sel = sel_registerName("makeFirstResponder:");
    return sel;
}

inline SEL sel_modifier_flags() {
    static auto sel = sel_registerName("modifierFlags");
    return sel;
}

inline SEL sel_marked_range() {
    static auto sel = sel_registerName("markedRange");
    return sel;
}

inline SEL sel_momentum_phase() {
    static auto sel = sel_registerName("momentumPhase");
    return sel;
}

inline SEL sel_move_left() {
    static auto sel = sel_registerName("moveLeft:");
    return sel;
}

inline SEL sel_move_left_and_modify_selection() {
    static auto sel = sel_registerName("moveLeftAndModifySelection:");
    return sel;
}

inline SEL sel_move_right() {
    static auto sel = sel_registerName("moveRight:");
    return sel;
}

inline SEL sel_move_right_and_modify_selection() {
    static auto sel = sel_registerName("moveRightAndModifySelection:");
    return sel;
}

inline SEL sel_move_to_beginning() {
    static auto sel = sel_registerName("moveToBeginningOfLine:");
    return sel;
}

inline SEL sel_move_to_beginning_and_modify_selection() {
    static auto sel = sel_registerName("moveToBeginningOfLineAndModifySelection:");
    return sel;
}

inline SEL sel_move_to_end() {
    static auto sel = sel_registerName("moveToEndOfLine:");
    return sel;
}

inline SEL sel_move_to_end_and_modify_selection() {
    static auto sel = sel_registerName("moveToEndOfLineAndModifySelection:");
    return sel;
}

inline SEL sel_move_up() {
    static auto sel = sel_registerName("moveUp:");
    return sel;
}

inline SEL sel_move_down() {
    static auto sel = sel_registerName("moveDown:");
    return sel;
}

inline SEL sel_delete_backward() {
    static auto sel = sel_registerName("deleteBackward:");
    return sel;
}

inline SEL sel_display_mode() {
    static auto sel = sel_registerName("displayMode");
    return sel;
}

inline SEL sel_set_display_mode() {
    static auto sel = sel_registerName("setDisplayMode:");
    return sel;
}

inline SEL sel_automatic_mode_options() {
    static auto sel = sel_registerName("automaticModeOptions");
    return sel;
}

inline SEL sel_set_automatic_mode_options() {
    static auto sel = sel_registerName("setAutomaticModeOptions:");
    return sel;
}

inline SEL sel_responds_to_selector() {
    static auto sel = sel_registerName("respondsToSelector:");
    return sel;
}

inline SEL sel_release() {
    static auto sel = sel_registerName("release");
    return sel;
}

inline SEL sel_perform_key_equivalent() {
    static auto sel = sel_registerName("performKeyEquivalent:");
    return sel;
}

inline SEL sel_remove_monitor() {
    static auto sel = sel_registerName("removeMonitor:");
    return sel;
}

inline SEL sel_remove_from_superview() {
    static auto sel = sel_registerName("removeFromSuperview");
    return sel;
}

inline SEL sel_selected_range() {
    static auto sel = sel_registerName("selectedRange");
    return sel;
}

inline SEL sel_select_all() {
    static auto sel = sel_registerName("selectAll:");
    return sel;
}

inline SEL sel_scrolling_delta_y() {
    static auto sel = sel_registerName("scrollingDeltaY");
    return sel;
}

inline SEL sel_set_marked_text() {
    static auto sel = sel_registerName("setMarkedText:selectedRange:replacementRange:");
    return sel;
}

inline SEL sel_phase() {
    static auto sel = sel_registerName("phase");
    return sel;
}

inline SEL sel_insert_text_with_range() {
    static auto sel = sel_registerName("insertText:replacementRange:");
    return sel;
}

inline SEL sel_string() {
    static auto sel = sel_registerName("string");
    return sel;
}

inline SEL sel_substring_with_range() {
    static auto sel = sel_registerName("substringWithRange:");
    return sel;
}

inline SEL sel_set_frame() {
    static auto sel = sel_registerName("setFrame:");
    return sel;
}

inline SEL sel_frame() {
    static auto sel = sel_registerName("frame");
    return sel;
}

inline SEL sel_characters_ignoring_modifiers() {
    static auto sel = sel_registerName("charactersIgnoringModifiers");
    return sel;
}

inline SEL sel_generic_rgb_color_space() {
    static auto sel = sel_registerName("genericRGBColorSpace");
    return sel;
}

inline SEL sel_get_red_green_blue_alpha() {
    static auto sel = sel_registerName("getRed:green:blue:alpha:");
    return sel;
}

inline SEL sel_text_insertion_point_color() {
    static auto sel = sel_registerName("textInsertionPointColor");
    return sel;
}

inline SEL sel_unmark_text() {
    static auto sel = sel_registerName("unmarkText");
    return sel;
}

inline SEL sel_utf8_string() {
    static auto sel = sel_registerName("UTF8String");
    return sel;
}

inline SEL sel_valid_attributes_for_marked_text() {
    static auto sel = sel_registerName("validAttributesForMarkedText");
    return sel;
}

inline SEL sel_window() {
    static auto sel = sel_registerName("window");
    return sel;
}

inline SEL sel_attributed_substring_for_range() {
    static auto sel =
        sel_registerName("attributedSubstringForProposedRange:actualRange:");
    return sel;
}

inline bool objc_responds_to(id obj, SEL sel) {
    return obj && objc_send<bool>(obj, sel_responds_to_selector(), sel);
}

inline float text_measure(float font_size, bool mono,
                          char const* text_ptr, unsigned int len);

inline float sanitize_scale(float scale) {
    return (scale > 0.0f && std::isfinite(scale)) ? scale : 1.0f;
}

inline int logical_pixels(float logical, float scale, int minimum = 1) {
    int px = static_cast<int>(std::ceil(logical * scale));
    return (px < minimum) ? minimum : px;
}

inline float snap_to_pixel_grid(float value, float scale) {
    float s = sanitize_scale(scale);
    return std::round(value * s) / s;
}

inline std::size_t clamp_snapshot_caret(
        ::phenotype::FocusedInputSnapshot const& snapshot) {
    if (snapshot.caret_pos == ::phenotype::native::invalid_callback_id)
        return snapshot.value.size();
    return ::phenotype::detail::clamp_utf8_boundary(snapshot.value, snapshot.caret_pos);
}

inline float measure_utf8_prefix(float font_size,
                                 bool mono,
                                 std::string const& text,
                                 std::size_t bytes) {
    bytes = ::phenotype::detail::clamp_utf8_boundary(text, bytes);
    if (bytes == 0)
        return 0.0f;
    return text_measure(
        font_size,
        mono,
        text.data(),
        static_cast<unsigned int>(bytes));
}

struct LineBoxMetrics {
    int slot_width = 0;
    int slot_height = 0;
    int render_top = 0;
    int render_height = 0;
    float baseline_y = 0;
};

inline LineBoxMetrics make_line_box(float logical_width,
                                    float logical_line_height,
                                    float ascent, float descent, float leading,
                                    float scale, int padding) {
    LineBoxMetrics box;
    float natural_line_height = (ascent + descent + leading) * sanitize_scale(scale);
    int text_width_px = logical_pixels(logical_width, scale, 0);
    int line_height_px = logical_pixels(logical_line_height, scale);
    int natural_height_px = static_cast<int>(std::ceil(natural_line_height));
    if (natural_height_px < 1) natural_height_px = 1;

    box.slot_width = text_width_px + padding * 2;
    if (box.slot_width < padding * 2 + 1)
        box.slot_width = padding * 2 + 1;

    box.render_top = padding;
    box.render_height = (line_height_px > natural_height_px)
        ? line_height_px : natural_height_px;
    box.slot_height = box.render_height + padding * 2;

    float extra_leading = static_cast<float>(box.render_height) - natural_line_height;
    if (extra_leading < 0.0f) extra_leading = 0.0f;
    box.baseline_y = static_cast<float>(box.render_top)
        + extra_leading * 0.5f + ascent * sanitize_scale(scale);
    return box;
}

inline void text_init() {
    if (g_text.initialized) return;
    g_text.sans = CTFontCreateUIFontForLanguage(kCTFontUIFontSystem, 16.0, nullptr);
    g_text.mono = CTFontCreateUIFontForLanguage(kCTFontUIFontUserFixedPitch, 16.0, nullptr);
    if (!g_text.sans) {
        std::fprintf(stderr, "[text] failed to create system font\n");
        return;
    }
    if (!g_text.mono)
        g_text.mono = static_cast<CTFontRef>(CFRetain(g_text.sans));
    g_text.initialized = true;
}

inline void text_shutdown() {
    if (g_text.sans) { CFRelease(g_text.sans); g_text.sans = nullptr; }
    if (g_text.mono) { CFRelease(g_text.mono); g_text.mono = nullptr; }
    g_text.initialized = false;
}

struct TextLineMetrics {
    float logical_width = 0.0f;
    float ascent = 0.0f;
    float descent = 0.0f;
    float leading = 0.0f;
    CGRect glyph_bounds = CGRectNull;
};

inline CFGuard<CTFontRef> copy_text_font(float font_size, bool mono) {
    CTFontRef base = mono ? g_text.mono : g_text.sans;
    return CFGuard<CTFontRef>(
        base ? CTFontCreateCopyWithAttributes(base, font_size, nullptr, nullptr) : nullptr);
}

inline CFGuard<CFStringRef> create_text_cf_string(char const* text_ptr,
                                                  unsigned int len) {
    return CFGuard<CFStringRef>(
        (text_ptr && len > 0)
            ? CFStringCreateWithBytes(
                kCFAllocatorDefault,
                reinterpret_cast<UInt8 const*>(text_ptr),
                static_cast<CFIndex>(len),
                kCFStringEncodingUTF8,
                false)
            : nullptr);
}

inline CFGuard<CTLineRef> create_text_line(CTFontRef font,
                                           char const* text_ptr,
                                           unsigned int len) {
    if (!font || !text_ptr || len == 0)
        return CFGuard<CTLineRef>(nullptr);

    auto text = create_text_cf_string(text_ptr, len);
    if (!text)
        return CFGuard<CTLineRef>(nullptr);

    CFTypeRef keys[] = {kCTFontAttributeName};
    CFTypeRef values[] = {font};
    auto attrs = CFGuard<CFDictionaryRef>(CFDictionaryCreate(
        kCFAllocatorDefault,
        reinterpret_cast<void const**>(keys),
        reinterpret_cast<void const**>(values),
        1,
        &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks));
    if (!attrs)
        return CFGuard<CTLineRef>(nullptr);

    auto attr_text = CFGuard<CFAttributedStringRef>(
        CFAttributedStringCreate(kCFAllocatorDefault, text, attrs));
    if (!attr_text)
        return CFGuard<CTLineRef>(nullptr);

    return CFGuard<CTLineRef>(CTLineCreateWithAttributedString(attr_text));
}

inline bool describe_text_line(CTLineRef line, TextLineMetrics& out) {
    if (!line)
        return false;

    CGFloat ascent = 0.0;
    CGFloat descent = 0.0;
    CGFloat leading = 0.0;
    double width = CTLineGetTypographicBounds(line, &ascent, &descent, &leading);
    if (!std::isfinite(width))
        return false;

    out.logical_width = static_cast<float>(width);
    out.ascent = static_cast<float>(ascent);
    out.descent = static_cast<float>(descent);
    out.leading = static_cast<float>(leading);
    out.glyph_bounds = CTLineGetBoundsWithOptions(
        line,
        static_cast<CTLineBoundsOptions>(kCTLineBoundsUseGlyphPathBounds));
    return true;
}

inline void expand_line_box_for_ink(LineBoxMetrics& box,
                                    CGRect const& glyph_bounds,
                                    float logical_width,
                                    float scale,
                                    int padding,
                                    int& line_origin_x) {
    line_origin_x = padding;
    if (CGRectIsNull(glyph_bounds)
        || !std::isfinite(glyph_bounds.origin.x)
        || !std::isfinite(glyph_bounds.size.width))
        return;

    float typographic_width_px = logical_width * sanitize_scale(scale);
    int left_overhang = 0;
    float glyph_origin_x_px = static_cast<float>(glyph_bounds.origin.x)
        * sanitize_scale(scale);
    if (glyph_origin_x_px < 0.0f)
        left_overhang = static_cast<int>(std::ceil(-glyph_origin_x_px));

    float bounds_max_x = static_cast<float>(CGRectGetMaxX(glyph_bounds))
        * sanitize_scale(scale);
    int right_overhang = 0;
    if (std::isfinite(bounds_max_x) && bounds_max_x > typographic_width_px) {
        right_overhang = static_cast<int>(
            std::ceil(bounds_max_x - typographic_width_px));
    }

    box.slot_width += left_overhang + right_overhang;
    if (box.slot_width < padding * 2 + 1)
        box.slot_width = padding * 2 + 1;
    line_origin_x = padding + left_overhang;
}

struct LineRenderSpan {
    int start_x = 0;
    int end_x = 0;
    int line_origin_x = 0;
};

inline LineRenderSpan compute_line_render_span(int ink_min_x,
                                               int ink_max_x,
                                               int line_origin_x,
                                               float logical_width,
                                               float scale) {
    int logical_width_px = static_cast<int>(
        std::ceil(logical_width * sanitize_scale(scale)));
    if (logical_width_px < 1)
        logical_width_px = 1;

    int render_start_x = line_origin_x;
    int render_end_x = line_origin_x + logical_width_px;
    if (ink_max_x >= ink_min_x) {
        if (ink_min_x < render_start_x)
            render_start_x = ink_min_x;
        if (ink_max_x + 1 > render_end_x)
            render_end_x = ink_max_x + 1;
    }
    if (render_end_x <= render_start_x)
        render_end_x = render_start_x + 1;
    return {render_start_x, render_end_x, line_origin_x};
}

inline bool rasterize_line_alpha(CTLineRef line,
                                 int slot_width,
                                 int slot_height,
                                 int line_origin_x,
                                 float baseline_y,
                                 float scale,
                                 std::vector<std::uint8_t>& slot_alpha,
                                 int& ink_min_x,
                                 int& ink_max_x) {
    slot_alpha.assign(static_cast<std::size_t>(slot_width * slot_height), 0);
    ink_min_x = slot_width;
    ink_max_x = -1;

    auto* ctx = CGBitmapContextCreate(
        slot_alpha.data(),
        slot_width,
        slot_height,
        8,
        slot_width,
        nullptr,
        kCGImageAlphaOnly);
    if (!ctx)
        return false;

    CGContextSetTextMatrix(ctx, CGAffineTransformIdentity);
    CGContextSetGrayFillColor(ctx, 1.0, 1.0);
    CGContextSetShouldAntialias(ctx, true);
    CGContextScaleCTM(
        ctx,
        static_cast<CGFloat>(sanitize_scale(scale)),
        static_cast<CGFloat>(sanitize_scale(scale)));
    auto baseline_from_bottom = static_cast<CGFloat>(slot_height) - baseline_y;
    CGContextSetTextPosition(
        ctx,
        static_cast<CGFloat>(line_origin_x) / sanitize_scale(scale),
        baseline_from_bottom / sanitize_scale(scale));
    CTLineDraw(line, ctx);
    CGContextRelease(ctx);

    bool has_ink = false;
    for (int row = 0; row < slot_height; ++row) {
        for (int col = 0; col < slot_width; ++col) {
            auto alpha = slot_alpha[static_cast<std::size_t>(row * slot_width + col)];
            if (alpha == 0)
                continue;
            has_ink = true;
            if (col < ink_min_x)
                ink_min_x = col;
            if (col > ink_max_x)
                ink_max_x = col;
        }
    }

    return has_ink;
}

inline float text_measure(float font_size, bool mono,
                          char const* text_ptr, unsigned int len) {
    if (!g_text.initialized || len == 0)
        return 0.0f;

    auto font = copy_text_font(font_size, mono);
    if (!font)
        return 0.0f;

    auto line = create_text_line(font, text_ptr, len);
    if (!line)
        return 0.0f;

    double width = CTLineGetTypographicBounds(line, nullptr, nullptr, nullptr);
    return std::isfinite(width) ? static_cast<float>(width) : 0.0f;
}

inline unsigned long utf16_length(std::string_view utf8) {
    return static_cast<unsigned long>(cppx::unicode::utf16_length(utf8));
}

inline std::string text_object_to_utf8(id value) {
    if (!value)
        return {};
    if (objc_responds_to(value, sel_string()))
        value = objc_send<id>(value, sel_string());
    if (!value)
        return {};
    auto utf8 = objc_send<char const*>(value, sel_utf8_string());
    return utf8 ? std::string(utf8) : std::string{};
}

inline unsigned long text_object_length_utf16(id value) {
    if (!value)
        return 0;
    if (objc_responds_to(value, sel_string()))
        value = objc_send<id>(value, sel_string());
    if (!value)
        return 0;
    return objc_send<unsigned long>(value, sel_length());
}

inline std::string text_object_prefix_utf8(id value, unsigned long length) {
    if (!value)
        return {};
    if (objc_responds_to(value, sel_string()))
        value = objc_send<id>(value, sel_string());
    if (!value)
        return {};
    auto total = objc_send<unsigned long>(value, sel_length());
    if (length > total)
        length = total;
    CocoaRange range{0, length};
    auto prefix = objc_send<id>(value, sel_substring_with_range(), range);
    return text_object_to_utf8(prefix);
}

inline TextAtlas text_build_atlas(std::vector<TextEntry> const& entries,
                                  float backing_scale) {
    if (entries.empty()) return {};

    float scale = sanitize_scale(backing_scale);
    static constexpr int ATLAS_SIZE = 2048;
    TextAtlas atlas;
    atlas.width = ATLAS_SIZE;
    atlas.height = ATLAS_SIZE;
    atlas.pixels.resize(static_cast<std::size_t>(ATLAS_SIZE * ATLAS_SIZE * 4), 0);

    int ax = 0;
    int ay = 0;
    int row_height = 0;
    int padding = static_cast<int>(std::ceil(scale)) + 1;
    if (padding < 2) padding = 2;

    for (auto const& entry : entries) {
        if (entry.text.empty()) continue;

        auto font = copy_text_font(entry.font_size, entry.mono);
        if (!font) continue;
        auto line = create_text_line(
            font, entry.text.c_str(), static_cast<unsigned int>(entry.text.size()));
        if (!line) continue;

        TextLineMetrics line_metrics;
        if (!describe_text_line(line, line_metrics))
            continue;

        float logical_line_height = entry.line_height > 0
            ? entry.line_height
            : entry.font_size * 1.6f;
        float snapped_x = snap_to_pixel_grid(entry.x, scale);
        float snapped_y = snap_to_pixel_grid(entry.y, scale);

        auto box = make_line_box(
            line_metrics.logical_width,
            logical_line_height,
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

        if (ax + box.slot_width > ATLAS_SIZE) {
            ax = 0;
            ay += row_height;
            row_height = 0;
        }
        if (ay + box.slot_height > ATLAS_SIZE) break;

        std::vector<std::uint8_t> slot_alpha;
        int ink_min_x = box.slot_width;
        int ink_max_x = -1;
        bool has_ink = rasterize_line_alpha(
            line,
            box.slot_width,
            box.slot_height,
            line_origin_x,
            box.baseline_y,
            scale,
            slot_alpha,
            ink_min_x,
            ink_max_x);

        if (has_ink) {
            for (int row = 0; row < box.slot_height; ++row) {
                for (int col = 0; col < box.slot_width; ++col) {
                    auto alpha = slot_alpha[static_cast<std::size_t>(
                        row * box.slot_width + col)];
                    if (alpha == 0)
                        continue;

                    int px = ax + col;
                    int py = ay + row;
                    if (px < 0 || px >= ATLAS_SIZE || py < 0 || py >= ATLAS_SIZE)
                        continue;

                    auto idx = static_cast<std::size_t>((py * ATLAS_SIZE + px) * 4);
                    atlas.pixels[idx + 0] = static_cast<std::uint8_t>(
                        entry.r * 255.0f * alpha / 255.0f);
                    atlas.pixels[idx + 1] = static_cast<std::uint8_t>(
                        entry.g * 255.0f * alpha / 255.0f);
                    atlas.pixels[idx + 2] = static_cast<std::uint8_t>(
                        entry.b * 255.0f * alpha / 255.0f);
                    atlas.pixels[idx + 3] = alpha;
                }
            }

            auto span = compute_line_render_span(
                ink_min_x,
                ink_max_x,
                line_origin_x,
                line_metrics.logical_width,
                scale);
            int render_w = span.end_x - span.start_x;
            atlas.quads.push_back({
                snapped_x + static_cast<float>(span.start_x - span.line_origin_x) / scale,
                snapped_y,
                static_cast<float>(render_w) / scale,
                static_cast<float>(box.render_height) / scale,
                static_cast<float>(ax + span.start_x) / ATLAS_SIZE,
                static_cast<float>(ay + box.render_top) / ATLAS_SIZE,
                static_cast<float>(render_w) / ATLAS_SIZE,
                static_cast<float>(box.render_height) / ATLAS_SIZE,
            });
        }

        ax += box.slot_width;
        if (box.slot_height > row_height) row_height = box.slot_height;
    }

    return atlas;
}

struct ColorInstanceGPU {
    float rect[4]{};
    float color[4]{};
    float params[4]{};
};

struct TextInstanceGPU {
    float rect[4]{};
    float uv_rect[4]{};
    float color[4]{};
};

struct ParsedTextRun {
    float x = 0.0f;
    float y = 0.0f;
    float font_size = 0.0f;
    bool mono = false;
    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
    float a = 1.0f;
    float line_height = 0.0f;
    char const* text = nullptr;
    unsigned int len = 0;
};

struct RasterizedTextRun {
    std::vector<std::uint8_t> alpha;
    float x_offset = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
    int pixel_width = 0;
    int pixel_height = 0;
    bool has_ink = false;
};

struct TextCacheEntry {
    std::string text;
    int font_key = 0;
    int line_height_key = 0;
    int scale_key = 0;
    bool mono = false;
    float x_offset = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
    float u = 0.0f;
    float v = 0.0f;
    float uw = 0.0f;
    float vh = 0.0f;
    bool has_ink = false;
};

struct TextAtlasCache {
    static constexpr int atlas_size = 4096;

    std::vector<TextCacheEntry> entries;
    std::vector<std::uint8_t> pixels;
    int cursor_x = 0;
    int cursor_y = 0;
    int row_height = 0;
    bool dirty = false;
    int dirty_min_x = atlas_size;
    int dirty_min_y = atlas_size;
    int dirty_max_x = 0;
    int dirty_max_y = 0;
    int active_scale_key = 0;
};

struct PendingImageCmd {
    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;
    std::string url;
};

struct ImageInstanceGPU {
    float rect[4]{};
    float uv_rect[4]{};
    float color[4]{};
};

struct DecodedImage {
    std::string url;
    std::vector<std::uint8_t> pixels;
    int width = 0;
    int height = 0;
    bool failed = false;
    std::string error_detail;
};

enum class ImageEntryState {
    pending,
    ready,
    failed,
};

inline char const* image_entry_state_name(ImageEntryState state) {
    switch (state) {
    case ImageEntryState::pending:
        return "pending";
    case ImageEntryState::ready:
        return "ready";
    case ImageEntryState::failed:
        return "failed";
    default:
        return "unknown";
    }
}

struct ImageCacheEntry {
    ImageEntryState state = ImageEntryState::pending;
    float u = 0.0f;
    float v = 0.0f;
    float uw = 0.0f;
    float vh = 0.0f;
    std::string failure_reason;
};

struct ImageAtlasCache {
    static constexpr int atlas_size = 2048;

    std::vector<std::uint8_t> pixels;
    std::map<std::string, ImageCacheEntry> cache;
    std::deque<std::string> pending_jobs;
    std::deque<DecodedImage> completed_jobs;
    std::mutex mutex;
    std::condition_variable cv;
    std::thread worker;
    void (*request_repaint)() = nullptr;
    bool worker_started = false;
    bool queue_only_for_tests = false;
    bool stop_worker = false;
    int cursor_x = 0;
    int cursor_y = 0;
    int row_height = 0;
    bool dirty = false;
    int dirty_min_x = atlas_size;
    int dirty_min_y = atlas_size;
    int dirty_max_x = 0;
    int dirty_max_y = 0;
};

inline ImageAtlasCache g_images;

inline constexpr long long insertion_indicator_display_mode_automatic = 0;
inline constexpr long long insertion_indicator_display_mode_hidden = 1;
inline constexpr long long insertion_indicator_display_mode_visible = 2;
inline constexpr long long insertion_indicator_automatic_mode_show_effects = 1 << 0;
inline constexpr long long insertion_indicator_automatic_mode_show_while_tracking = 1 << 1;
inline constexpr unsigned long long ns_event_mask_scroll_wheel = 1ull << 22;
inline constexpr unsigned long long ns_event_phase_none = 0;
inline constexpr unsigned long long ns_event_phase_began = 1ull << 0;
inline constexpr unsigned long long ns_event_phase_stationary = 1ull << 1;
inline constexpr unsigned long long ns_event_phase_changed = 1ull << 2;
inline constexpr unsigned long long ns_event_phase_ended = 1ull << 3;
inline constexpr unsigned long long ns_event_phase_cancelled = 1ull << 4;
inline constexpr unsigned long long ns_event_phase_may_begin = 1ull << 5;

inline bool g_force_disable_system_caret_for_tests = false;

struct ImeState {
    GLFWwindow* window = nullptr;
    id ns_window = nullptr;
    id content_view = nullptr;
    id editor_view = nullptr;
    id caret_host_view = nullptr;
    id insertion_indicator = nullptr;
    id scroll_monitor = nullptr;
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
};

inline ImeState g_ime;

struct FrameScratch {
    std::vector<ColorInstanceGPU> color_instances;
    std::vector<ColorInstanceGPU> overlay_color_instances;
    std::vector<ParsedTextRun> text_runs;
    std::vector<PendingImageCmd> images;
    std::vector<ImageInstanceGPU> image_instances;
    std::vector<TextInstanceGPU> text_instances;
    std::vector<std::string> overlay_text_storage;

    void clear() {
        color_instances.clear();
        overlay_color_instances.clear();
        text_runs.clear();
        images.clear();
        image_instances.clear();
        text_instances.clear();
        overlay_text_storage.clear();
    }
};

inline Color unpack_color(unsigned int packed) noexcept {
    return {
        static_cast<unsigned char>((packed >> 24) & 0xFF),
        static_cast<unsigned char>((packed >> 16) & 0xFF),
        static_cast<unsigned char>((packed >>  8) & 0xFF),
        static_cast<unsigned char>( packed        & 0xFF),
    };
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

inline void append_color_instance(std::vector<ColorInstanceGPU>& out,
                                  float x, float y, float w, float h,
                                  float r, float g, float b, float a,
                                  float p0 = 0.0f, float p1 = 0.0f,
                                  float p2 = 0.0f, float p3 = 0.0f) {
    ColorInstanceGPU inst{};
    inst.rect[0] = x;
    inst.rect[1] = y;
    inst.rect[2] = w;
    inst.rect[3] = h;
    inst.color[0] = r;
    inst.color[1] = g;
    inst.color[2] = b;
    inst.color[3] = a;
    inst.params[0] = p0;
    inst.params[1] = p1;
    inst.params[2] = p2;
    inst.params[3] = p3;
    out.push_back(inst);
}

inline void append_text_instance(std::vector<TextInstanceGPU>& out,
                                 float x, float y, float w, float h,
                                 float u, float v, float uw, float vh,
                                 float r, float g, float b, float a) {
    TextInstanceGPU inst{};
    inst.rect[0] = x;
    inst.rect[1] = y;
    inst.rect[2] = w;
    inst.rect[3] = h;
    inst.uv_rect[0] = u;
    inst.uv_rect[1] = v;
    inst.uv_rect[2] = uw;
    inst.uv_rect[3] = vh;
    inst.color[0] = r;
    inst.color[1] = g;
    inst.color[2] = b;
    inst.color[3] = a;
    out.push_back(inst);
}

inline void append_image_instance(std::vector<ImageInstanceGPU>& out,
                                  float x, float y, float w, float h,
                                  float u, float v, float uw, float vh,
                                  float r = 1.0f, float g = 1.0f,
                                  float b = 1.0f, float a = 1.0f) {
    ImageInstanceGPU inst{};
    inst.rect[0] = x;
    inst.rect[1] = y;
    inst.rect[2] = w;
    inst.rect[3] = h;
    inst.uv_rect[0] = u;
    inst.uv_rect[1] = v;
    inst.uv_rect[2] = uw;
    inst.uv_rect[3] = vh;
    inst.color[0] = r;
    inst.color[1] = g;
    inst.color[2] = b;
    inst.color[3] = a;
    out.push_back(inst);
}

struct CommandReader {
    unsigned char const* cur = nullptr;
    unsigned char const* end = nullptr;

    bool can_read(unsigned int bytes) const noexcept {
        return cur + bytes <= end;
    }

    bool read_u32(unsigned int& out) noexcept {
        if (!can_read(4)) return false;
        std::memcpy(&out, cur, 4);
        cur += 4;
        return true;
    }

    bool read_f32(float& out) noexcept {
        unsigned int bits = 0;
        if (!read_u32(bits)) return false;
        std::memcpy(&out, &bits, 4);
        return true;
    }

    bool read_text(char const*& data, unsigned int len) noexcept {
        if (!can_read(len)) return false;
        data = reinterpret_cast<char const*>(cur);
        cur += len;
        auto const padded_len = (len + 3u) & ~3u;
        if (padded_len > len) {
            if (!can_read(padded_len - len)) return false;
            cur += padded_len - len;
        }
        return true;
    }
};

inline bool is_http_url(std::string const& url) {
    return cppx::resource::is_remote(url);
}

inline std::filesystem::path resolve_image_path(std::string const& url) {
    return cppx::resource::resolve_path(
        std::filesystem::current_path(),
        std::string_view{url});
}

inline bool decode_frame_commands(unsigned char const* buf, unsigned int len,
                                  float line_height_ratio,
                                  FrameScratch& scratch,
                                  std::vector<HitRegionCmd>& hit_regions,
                                  double& cr, double& cg,
                                  double& cb, double& ca) {
    scratch.clear();
    hit_regions.clear();
    cr = 0.98;
    cg = 0.98;
    cb = 0.98;
    ca = 1.0;

    CommandReader reader{buf, buf + len};
    while (reader.cur < reader.end) {
        unsigned int raw_cmd = 0;
        if (!reader.read_u32(raw_cmd))
            return false;

        switch (static_cast<Cmd>(raw_cmd)) {
            case Cmd::Clear: {
                unsigned int packed = 0;
                if (!reader.read_u32(packed))
                    return false;
                auto color = unpack_color(packed);
                cr = color.r / 255.0;
                cg = color.g / 255.0;
                cb = color.b / 255.0;
                ca = color.a / 255.0;
                break;
            }
            case Cmd::FillRect: {
                float x = 0.0f, y = 0.0f, w = 0.0f, h = 0.0f;
                unsigned int packed = 0;
                if (!reader.read_f32(x) || !reader.read_f32(y)
                    || !reader.read_f32(w) || !reader.read_f32(h)
                    || !reader.read_u32(packed))
                    return false;
                auto color = unpack_color(packed);
                append_color_instance(
                    scratch.color_instances,
                    x, y, w, h,
                    color.r / 255.0f, color.g / 255.0f,
                    color.b / 255.0f, color.a / 255.0f);
                break;
            }
            case Cmd::StrokeRect: {
                float x = 0.0f, y = 0.0f, w = 0.0f, h = 0.0f, line_width = 0.0f;
                unsigned int packed = 0;
                if (!reader.read_f32(x) || !reader.read_f32(y)
                    || !reader.read_f32(w) || !reader.read_f32(h)
                    || !reader.read_f32(line_width)
                    || !reader.read_u32(packed))
                    return false;
                auto color = unpack_color(packed);
                append_color_instance(
                    scratch.color_instances,
                    x, y, w, h,
                    color.r / 255.0f, color.g / 255.0f,
                    color.b / 255.0f, color.a / 255.0f,
                    0.0f, line_width, 1.0f);
                break;
            }
            case Cmd::RoundRect: {
                float x = 0.0f, y = 0.0f, w = 0.0f, h = 0.0f, radius = 0.0f;
                unsigned int packed = 0;
                if (!reader.read_f32(x) || !reader.read_f32(y)
                    || !reader.read_f32(w) || !reader.read_f32(h)
                    || !reader.read_f32(radius)
                    || !reader.read_u32(packed))
                    return false;
                auto color = unpack_color(packed);
                append_color_instance(
                    scratch.color_instances,
                    x, y, w, h,
                    color.r / 255.0f, color.g / 255.0f,
                    color.b / 255.0f, color.a / 255.0f,
                    radius, 0.0f, 2.0f);
                break;
            }
            case Cmd::DrawText: {
                float x = 0.0f, y = 0.0f, font_size = 0.0f;
                unsigned int mono = 0;
                unsigned int packed = 0;
                unsigned int text_len = 0;
                char const* text = nullptr;
                if (!reader.read_f32(x) || !reader.read_f32(y)
                    || !reader.read_f32(font_size) || !reader.read_u32(mono)
                    || !reader.read_u32(packed) || !reader.read_u32(text_len)
                    || !reader.read_text(text, text_len))
                    return false;
                auto color = unpack_color(packed);
                scratch.text_runs.push_back({
                    x,
                    y,
                    font_size,
                    mono != 0,
                    color.r / 255.0f,
                    color.g / 255.0f,
                    color.b / 255.0f,
                    color.a / 255.0f,
                    font_size * line_height_ratio,
                    text,
                    text_len,
                });
                break;
            }
            case Cmd::DrawLine: {
                float x1 = 0.0f, y1 = 0.0f, x2 = 0.0f, y2 = 0.0f, thickness = 0.0f;
                unsigned int packed = 0;
                if (!reader.read_f32(x1) || !reader.read_f32(y1)
                    || !reader.read_f32(x2) || !reader.read_f32(y2)
                    || !reader.read_f32(thickness)
                    || !reader.read_u32(packed))
                    return false;
                auto color = unpack_color(packed);
                float dx = x2 - x1;
                float dy = y2 - y1;
                float line_len = std::sqrt(dx * dx + dy * dy);
                float w = (dy == 0.0f) ? line_len : thickness;
                float h = (dx == 0.0f) ? line_len : thickness;
                float x = (dx == 0.0f)
                    ? x1 - thickness / 2.0f
                    : (x1 < x2 ? x1 : x2);
                float y = (dy == 0.0f)
                    ? y1 - thickness / 2.0f
                    : (y1 < y2 ? y1 : y2);
                append_color_instance(
                    scratch.color_instances,
                    x, y, w, h,
                    color.r / 255.0f, color.g / 255.0f,
                    color.b / 255.0f, color.a / 255.0f,
                    0.0f, 0.0f, 3.0f);
                break;
            }
            case Cmd::HitRegion: {
                HitRegionCmd hit{};
                unsigned int cursor_type = 0;
                if (!reader.read_f32(hit.x) || !reader.read_f32(hit.y)
                    || !reader.read_f32(hit.w) || !reader.read_f32(hit.h)
                    || !reader.read_u32(hit.callback_id)
                    || !reader.read_u32(cursor_type))
                    return false;
                hit.cursor_type = cursor_type;
                hit_regions.push_back(hit);
                break;
            }
            case Cmd::DrawImage: {
                PendingImageCmd image{};
                unsigned int url_len = 0;
                char const* url = nullptr;
                if (!reader.read_f32(image.x) || !reader.read_f32(image.y)
                    || !reader.read_f32(image.w) || !reader.read_f32(image.h)
                    || !reader.read_u32(url_len)
                    || !reader.read_text(url, url_len))
                    return false;
                image.url.assign(url ? url : "", url_len);
                scratch.images.push_back(std::move(image));
                break;
            }
            default:
                return false;
        }
    }
    return true;
}

inline void mark_image_cache_dirty(ImageAtlasCache& cache,
                                   int x, int y, int w, int h) {
    cache.dirty = true;
    if (x < cache.dirty_min_x) cache.dirty_min_x = x;
    if (y < cache.dirty_min_y) cache.dirty_min_y = y;
    if (x + w > cache.dirty_max_x) cache.dirty_max_x = x + w;
    if (y + h > cache.dirty_max_y) cache.dirty_max_y = y + h;
}

inline bool reserve_image_slot(ImageAtlasCache& cache, int width, int height,
                               int& out_x, int& out_y) {
    if (width <= 0 || height <= 0
        || width > ImageAtlasCache::atlas_size
        || height > ImageAtlasCache::atlas_size) {
        return false;
    }

    if (cache.pixels.size()
        != static_cast<std::size_t>(ImageAtlasCache::atlas_size * ImageAtlasCache::atlas_size * 4)) {
        cache.pixels.assign(
            static_cast<std::size_t>(ImageAtlasCache::atlas_size * ImageAtlasCache::atlas_size * 4),
            0);
    }

    if (cache.cursor_x + width > ImageAtlasCache::atlas_size) {
        cache.cursor_x = 0;
        cache.cursor_y += cache.row_height;
        cache.row_height = 0;
    }
    if (cache.cursor_y + height > ImageAtlasCache::atlas_size)
        return false;

    out_x = cache.cursor_x;
    out_y = cache.cursor_y;
    cache.cursor_x += width;
    if (height > cache.row_height) cache.row_height = height;
    return true;
}

inline void clear_image_entry(ImageCacheEntry& entry) {
    entry.u = 0.0f;
    entry.v = 0.0f;
    entry.uw = 0.0f;
    entry.vh = 0.0f;
    entry.failure_reason.clear();
}

inline void mark_image_entry_failed(ImageCacheEntry& entry,
                                    std::string_view detail) {
    entry.state = ImageEntryState::failed;
    clear_image_entry(entry);
    entry.failure_reason = std::string(detail);
}

inline std::uint16_t read_le16(std::uint8_t const* ptr) noexcept {
    return static_cast<std::uint16_t>(ptr[0])
        | (static_cast<std::uint16_t>(ptr[1]) << 8);
}

inline std::uint32_t read_le32(std::uint8_t const* ptr) noexcept {
    return static_cast<std::uint32_t>(ptr[0])
        | (static_cast<std::uint32_t>(ptr[1]) << 8)
        | (static_cast<std::uint32_t>(ptr[2]) << 16)
        | (static_cast<std::uint32_t>(ptr[3]) << 24);
}

inline bool decode_bmp_memory(std::vector<std::uint8_t> const& bytes,
                              DecodedImage& out) {
    if (bytes.size() < 54)
        return false;
    if (bytes[0] != 'B' || bytes[1] != 'M')
        return false;

    auto const dib_size = read_le32(bytes.data() + 14);
    if (dib_size < 40)
        return false;

    auto const data_offset = read_le32(bytes.data() + 10);
    auto const raw_width = static_cast<std::int32_t>(read_le32(bytes.data() + 18));
    auto const raw_height = static_cast<std::int32_t>(read_le32(bytes.data() + 22));
    auto const planes = read_le16(bytes.data() + 26);
    auto const bits_per_pixel = read_le16(bytes.data() + 28);
    auto const compression = read_le32(bytes.data() + 30);

    if (planes != 1 || raw_width <= 0 || raw_height == 0 || compression != 0)
        return false;
    if (bits_per_pixel != 24 && bits_per_pixel != 32)
        return false;

    auto const top_down = raw_height < 0;
    auto const height = top_down ? -raw_height : raw_height;
    auto const width = raw_width;
    auto const bytes_per_pixel = bits_per_pixel / 8;
    auto const row_stride = static_cast<std::size_t>(((width * bits_per_pixel + 31) / 32) * 4);
    auto const required = static_cast<std::size_t>(data_offset)
        + row_stride * static_cast<std::size_t>(height);
    if (required > bytes.size())
        return false;

    out.width = width;
    out.height = height;
    out.failed = false;
    out.pixels.assign(static_cast<std::size_t>(width * height * 4), 0);

    for (int row = 0; row < height; ++row) {
        int const src_row = top_down ? row : (height - 1 - row);
        auto const* src = bytes.data()
            + static_cast<std::size_t>(data_offset)
            + static_cast<std::size_t>(src_row) * row_stride;
        auto* dst = out.pixels.data()
            + static_cast<std::size_t>(row * width * 4);

        for (int col = 0; col < width; ++col) {
            auto const pixel_index = static_cast<std::size_t>(col * bytes_per_pixel);
            dst[col * 4 + 0] = src[pixel_index + 2];
            dst[col * 4 + 1] = src[pixel_index + 1];
            dst[col * 4 + 2] = src[pixel_index + 0];
            dst[col * 4 + 3] = (bytes_per_pixel == 4)
                ? src[pixel_index + 3]
                : static_cast<std::uint8_t>(255);
        }
    }

    return true;
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

inline bool decode_bmp_file(std::filesystem::path const& path, DecodedImage& out) {
    auto file = std::fopen(path.string().c_str(), "rb");
    if (!file)
        return false;

    std::vector<std::uint8_t> bytes;
    std::array<std::uint8_t, 8192> chunk{};
    while (true) {
        auto const read = std::fread(chunk.data(), 1, chunk.size(), file);
        if (read > 0) {
            bytes.insert(bytes.end(), chunk.begin(), chunk.begin() + static_cast<std::ptrdiff_t>(read));
        }
        if (read < chunk.size()) {
            if (std::ferror(file)) {
                std::fclose(file);
                return false;
            }
            break;
        }
    }
    std::fclose(file);
    return decode_bmp_memory(bytes, out);
}

inline void ensure_image_worker();
inline bool store_decoded_image(DecodedImage decoded);

inline void queue_image_load(std::string const& url) {
    ensure_image_worker();
    {
        std::lock_guard lock(g_images.mutex);
        g_images.pending_jobs.push_back(url);
    }
    g_images.cv.notify_one();
}

inline ImageCacheEntry const* ensure_image_cache_entry(std::string const& url) {
    auto [it, inserted] = g_images.cache.try_emplace(url, ImageCacheEntry{});
    if (!inserted)
        return &it->second;

    if (is_http_url(url)) {
        queue_image_load(url);
        return &it->second;
    }

    DecodedImage decoded;
    decoded.url = url;
    decoded.failed = !decode_bmp_file(resolve_image_path(url), decoded);
    if (decoded.failed)
        decoded.error_detail = "Failed to decode local BMP image";
    (void)store_decoded_image(std::move(decoded));

    it = g_images.cache.find(url);
    return it != g_images.cache.end() ? &it->second : nullptr;
}

inline bool store_decoded_image(DecodedImage decoded) {
    auto [it, inserted] = g_images.cache.try_emplace(decoded.url, ImageCacheEntry{});
    if (!inserted && it->second.state == ImageEntryState::ready)
        return false;

    clear_image_entry(it->second);

    if (decoded.failed || decoded.pixels.empty()
        || decoded.width <= 0 || decoded.height <= 0) {
        bool changed = it->second.state != ImageEntryState::failed;
        mark_image_entry_failed(
            it->second,
            decoded.error_detail.empty()
                ? "Image decode failed"
                : decoded.error_detail);
        return changed;
    }

    int slot_x = 0;
    int slot_y = 0;
    if (!reserve_image_slot(g_images, decoded.width, decoded.height, slot_x, slot_y)) {
        bool changed = it->second.state != ImageEntryState::failed;
        mark_image_entry_failed(it->second, "Image atlas is full");
        return changed;
    }

    for (int row = 0; row < decoded.height; ++row) {
        auto* dst = g_images.pixels.data()
            + static_cast<std::size_t>(
                ((slot_y + row) * ImageAtlasCache::atlas_size + slot_x) * 4);
        auto const* src = decoded.pixels.data()
            + static_cast<std::size_t>(row * decoded.width * 4);
        std::memcpy(dst, src, static_cast<std::size_t>(decoded.width * 4));
    }
    mark_image_cache_dirty(g_images, slot_x, slot_y, decoded.width, decoded.height);

    it->second.state = ImageEntryState::ready;
    it->second.u = static_cast<float>(slot_x) / ImageAtlasCache::atlas_size;
    it->second.v = static_cast<float>(slot_y) / ImageAtlasCache::atlas_size;
    it->second.uw = static_cast<float>(decoded.width) / ImageAtlasCache::atlas_size;
    it->second.vh = static_cast<float>(decoded.height) / ImageAtlasCache::atlas_size;
    return true;
}

inline bool process_completed_images() {
    std::deque<DecodedImage> completed;
    {
        std::lock_guard lock(g_images.mutex);
        if (g_images.completed_jobs.empty())
            return false;
        completed.swap(g_images.completed_jobs);
    }

    bool changed = false;
    while (!completed.empty()) {
        changed = store_decoded_image(std::move(completed.front())) || changed;
        completed.pop_front();
    }
    return changed;
}

inline void image_worker_main() {
    for (;;) {
        std::string url;
        {
            std::unique_lock lock(g_images.mutex);
            g_images.cv.wait(lock, [] {
                return g_images.stop_worker || !g_images.pending_jobs.empty();
            });
            if (g_images.stop_worker && g_images.pending_jobs.empty())
                break;
            url = std::move(g_images.pending_jobs.front());
            g_images.pending_jobs.pop_front();
        }

        DecodedImage decoded;
        decoded.url = url;
        if (auto response = cppx::http::system::get(url);
            response && response->stat.ok()) {
            std::vector<std::uint8_t> body;
            body.reserve(response->body.size());
            auto const* body_data = response->body.data();
            for (std::size_t i = 0, n = response->body.size(); i < n; ++i)
                body.push_back(static_cast<std::uint8_t>(body_data[i]));
            decoded.failed = !decode_bmp_memory(body, decoded);
            if (decoded.failed)
                decoded.error_detail = "Remote BMP decode failed";
        } else {
            decoded.failed = true;
            if (!response) {
                decoded.error_detail = std::string(cppx::http::to_string(response.error()));
            } else {
                decoded.error_detail = "HTTP status " + std::to_string(response->stat.code);
            }
        }

        {
            std::lock_guard lock(g_images.mutex);
            g_images.completed_jobs.push_back(std::move(decoded));
        }
    }
}

inline void ensure_image_worker() {
    if (g_images.worker_started || g_images.queue_only_for_tests)
        return;
    g_images.stop_worker = false;
    g_images.worker = std::thread(image_worker_main);
    g_images.worker_started = true;
}

inline void shutdown_image_worker() {
    {
        std::lock_guard lock(g_images.mutex);
        g_images.stop_worker = true;
        g_images.pending_jobs.clear();
    }
    g_images.cv.notify_all();
    if (g_images.worker.joinable())
        g_images.worker.join();
    g_images.worker_started = false;
}

inline void reset_image_cache(bool preserve_request_repaint = false) {
    shutdown_image_worker();
    {
        std::lock_guard lock(g_images.mutex);
        g_images.pending_jobs.clear();
        g_images.completed_jobs.clear();
    }
    g_images.cache.clear();
    g_images.pixels.clear();
    g_images.cursor_x = 0;
    g_images.cursor_y = 0;
    g_images.row_height = 0;
    g_images.dirty = false;
    g_images.dirty_min_x = ImageAtlasCache::atlas_size;
    g_images.dirty_min_y = ImageAtlasCache::atlas_size;
    g_images.dirty_max_x = 0;
    g_images.dirty_max_y = 0;
    g_images.queue_only_for_tests = false;
    g_images.stop_worker = false;
    if (!preserve_request_repaint)
        g_images.request_repaint = nullptr;
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

inline bool text_cache_matches(TextCacheEntry const& entry,
                               ParsedTextRun const& run,
                               int font_key,
                               int line_height_key,
                               int scale_key) {
    return entry.font_key == font_key
        && entry.line_height_key == line_height_key
        && entry.scale_key == scale_key
        && entry.mono == run.mono
        && entry.text.size() == run.len
        && std::memcmp(entry.text.data(), run.text, run.len) == 0;
}

inline TextCacheEntry* find_text_cache_entry(TextAtlasCache& cache,
                                             ParsedTextRun const& run,
                                             int font_key,
                                             int line_height_key,
                                             int scale_key) {
    for (auto& entry : cache.entries) {
        if (text_cache_matches(entry, run, font_key, line_height_key, scale_key))
            return &entry;
    }
    return nullptr;
}

inline bool rasterize_text_run(char const* text_ptr, unsigned int len,
                               float font_size, bool mono,
                               float line_height, float scale,
                               RasterizedTextRun& out) {
    out = {};
    if (!g_text.initialized || len == 0)
        return false;

    auto font = copy_text_font(font_size, mono);
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

inline constexpr char MSL_SHADERS[] = R"(
#include <metal_stdlib>
using namespace metal;

struct Uniforms { float2 viewport; float2 _pad; };

struct ColorVsOut {
    float4 pos       [[position]];
    float4 color;
    float2 local_pos;
    float2 rect_size;
    float4 params;
};

struct ColorInstance {
    float4 rect;
    float4 color;
    float4 params;
};

vertex ColorVsOut vs_color(
    uint vi [[vertex_id]],
    uint ii [[instance_id]],
    constant Uniforms& u [[buffer(0)]],
    const device ColorInstance* instances [[buffer(1)]]
) {
    constexpr float2 corners[] = {
        float2(0,0), float2(1,0), float2(0,1),
        float2(1,0), float2(1,1), float2(0,1),
    };
    float2 c = corners[vi];
    ColorInstance inst = instances[ii];
    float px = inst.rect.x + c.x * inst.rect.z;
    float py = inst.rect.y + c.y * inst.rect.w;
    float cx = (px / u.viewport.x) * 2.0 - 1.0;
    float cy = 1.0 - (py / u.viewport.y) * 2.0;
    ColorVsOut out;
    out.pos = float4(cx, cy, 0, 1);
    out.color = inst.color;
    out.local_pos = c * inst.rect.zw;
    out.rect_size = inst.rect.zw;
    out.params = inst.params;
    return out;
}

fragment float4 fs_color(ColorVsOut in [[stage_in]]) {
    uint draw_type = uint(in.params.z);
    float radius = in.params.x;
    float border_w = in.params.y;
    if (draw_type == 2u && radius > 0.0) {
        float2 half_size = in.rect_size * 0.5;
        float2 p = abs(in.local_pos - half_size) - half_size + float2(radius);
        float d = length(max(p, float2(0.0))) - radius;
        if (d > 0.5) discard_fragment();
        float alpha = in.color.a * clamp(0.5 - d, 0.0, 1.0);
        return float4(in.color.rgb * alpha, alpha);
    }
    if (draw_type == 1u) {
        float2 lp = in.local_pos;
        float2 sz = in.rect_size;
        if (lp.x > border_w && lp.x < sz.x - border_w &&
            lp.y > border_w && lp.y < sz.y - border_w) discard_fragment();
        return in.color;
    }
    return in.color;
}

struct TextVsOut {
    float4 pos [[position]];
    float2 uv;
    float4 color;
};

struct TextInstance {
    float4 rect;
    float4 uv_rect;
    float4 color;
};

inline TextVsOut make_textured_vs(
    uint vi,
    uint ii,
    constant Uniforms& u,
    const device TextInstance* instances
) {
    constexpr float2 corners[] = {
        float2(0,0), float2(1,0), float2(0,1),
        float2(1,0), float2(1,1), float2(0,1),
    };
    float2 c = corners[vi];
    TextInstance inst = instances[ii];
    float px = inst.rect.x + c.x * inst.rect.z;
    float py = inst.rect.y + c.y * inst.rect.w;
    float cx = (px / u.viewport.x) * 2.0 - 1.0;
    float cy = 1.0 - (py / u.viewport.y) * 2.0;
    TextVsOut out;
    out.pos = float4(cx, cy, 0, 1);
    out.uv = inst.uv_rect.xy + c * inst.uv_rect.zw;
    out.color = inst.color;
    return out;
}

vertex TextVsOut vs_text(
    uint vi [[vertex_id]],
    uint ii [[instance_id]],
    constant Uniforms& u [[buffer(0)]],
    const device TextInstance* instances [[buffer(1)]]
) {
    return make_textured_vs(vi, ii, u, instances);
}

fragment float4 fs_text(
    TextVsOut in [[stage_in]],
    texture2d<float> atlas [[texture(0)]],
    sampler samp [[sampler(0)]]
) {
    float coverage = atlas.sample(samp, in.uv).r;
    if (coverage < 0.01) discard_fragment();
    return float4(in.color.rgb, in.color.a * coverage);
}

vertex TextVsOut vs_image(
    uint vi [[vertex_id]],
    uint ii [[instance_id]],
    constant Uniforms& u [[buffer(0)]],
    const device TextInstance* instances [[buffer(1)]]
) {
    return make_textured_vs(vi, ii, u, instances);
}

fragment float4 fs_image(
    TextVsOut in [[stage_in]],
    texture2d<float> atlas [[texture(0)]],
    sampler samp [[sampler(0)]]
) {
    float4 sample = atlas.sample(samp, in.uv);
    if (sample.a < 0.01) discard_fragment();
    return sample * in.color;
}
)";

struct RendererState {
    MTL::Device* device = nullptr;
    MTL::CommandQueue* queue = nullptr;
    CA::MetalLayer* layer = nullptr;
    MTL::RenderPipelineState* color_pipeline = nullptr;
    MTL::RenderPipelineState* image_pipeline = nullptr;
    MTL::RenderPipelineState* text_pipeline = nullptr;
    MTL::Buffer* uniform_buf = nullptr;
    MTL::Buffer* color_instances_buf = nullptr;
    std::size_t color_instances_capacity = 0;
    MTL::Buffer* overlay_color_instances_buf = nullptr;
    std::size_t overlay_color_instances_capacity = 0;
    MTL::Buffer* image_instances_buf = nullptr;
    std::size_t image_instances_capacity = 0;
    MTL::Buffer* text_instances_buf = nullptr;
    std::size_t text_instances_capacity = 0;
    MTL::Texture* debug_capture_texture = nullptr;
    int debug_capture_width = 0;
    int debug_capture_height = 0;
    MTL::Buffer* frame_readback_buf = nullptr;
    std::size_t frame_readback_capacity = 0;
    MTL::Texture* image_atlas_texture = nullptr;
    MTL::Texture* text_atlas_texture = nullptr;
    MTL::SamplerState* sampler = nullptr;
    std::vector<HitRegionCmd> hit_regions;
    FrameScratch scratch;
    TextAtlasCache text_cache;
    GLFWwindow* window = nullptr;
    int drawable_width = 0;
    int drawable_height = 0;
    int last_render_width = 0;
    int last_render_height = 0;
    bool last_frame_available = false;
    bool initialized = false;
};

inline RendererState g_renderer;

inline float current_backing_scale(GLFWwindow* window) {
    if (!window) return 1.0f;
    int fbw = 0;
    int fbh = 0;
    int winw = 0;
    int winh = 0;
    glfwGetFramebufferSize(window, &fbw, &fbh);
    glfwGetWindowSize(window, &winw, &winh);
    float sx = (winw > 0) ? static_cast<float>(fbw) / static_cast<float>(winw) : 1.0f;
    float sy = (winh > 0) ? static_cast<float>(fbh) / static_cast<float>(winh) : 1.0f;
    float scale = (sx > sy) ? sx : sy;
    return (scale > 0.0f && std::isfinite(scale)) ? scale : 1.0f;
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
    scratch.text_instances.clear();
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
        scratch.text_instances.clear();

        for (auto const& run : scratch.text_runs) {
            if (!run.text || run.len == 0)
                continue;

            int font_key = quantize_metric(run.font_size);
            int line_height_key = quantize_metric(run.line_height);
            auto* entry = find_text_cache_entry(
                cache, run, font_key, line_height_key, scale_key);

            if (entry) {
                metrics::inst::native_text_cache_hits.add(1, native_platform_attrs());
            } else {
                RasterizedTextRun rasterized;
                if (!rasterize_text_run(
                        run.text, run.len, run.font_size, run.mono,
                        run.line_height, scale, rasterized)) {
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

                cache.entries.push_back({
                    std::string(run.text, run.len),
                    font_key,
                    line_height_key,
                    scale_key,
                    run.mono,
                    rasterized.x_offset,
                    rasterized.width,
                    rasterized.height,
                    static_cast<float>(slot_x) / TextAtlasCache::atlas_size,
                    static_cast<float>(slot_y) / TextAtlasCache::atlas_size,
                    static_cast<float>(rasterized.pixel_width) / TextAtlasCache::atlas_size,
                    static_cast<float>(rasterized.pixel_height) / TextAtlasCache::atlas_size,
                    rasterized.has_ink,
                });
                entry = &cache.entries.back();
                metrics::inst::native_text_cache_misses.add(1, native_platform_attrs());
            }

            if (!entry || !entry->has_ink)
                continue;

            append_text_instance(
                scratch.text_instances,
                snap_to_pixel_grid(run.x, scale) + entry->x_offset,
                snap_to_pixel_grid(run.y, scale),
                entry->width,
                entry->height,
                entry->u,
                entry->v,
                entry->uw,
                entry->vh,
                run.r,
                run.g,
                run.b,
                run.a);
        }

        if (!restart)
            return true;
    }
}

inline void prepare_image_instances(float scale) {
    auto& scratch = g_renderer.scratch;
    scratch.image_instances.clear();

    for (auto const& image : scratch.images) {
        auto const* entry = ensure_image_cache_entry(image.url);
        if (entry && entry->state == ImageEntryState::ready) {
            append_image_instance(
                scratch.image_instances,
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
        append_color_instance(
            scratch.color_instances,
            image.x, image.y, image.w, image.h,
            fill, fill, fill, 1.0f,
            6.0f, 0.0f, 2.0f);
        append_color_instance(
            scratch.color_instances,
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
    scratch.text_runs.push_back({
        x,
        y,
        font_size,
        mono,
        color.r / 255.0f,
        color.g / 255.0f,
        color.b / 255.0f,
        color.a / 255.0f,
        line_height,
        stored.c_str(),
        static_cast<unsigned int>(stored.size()),
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

inline float current_scroll_viewport_height() {
    // shell.cppm's viewport_height() reads from the active host's cached
    // size, which is kept up to date by the GLFW driver's resize callback.
    return viewport_height();
}

inline bool handle_local_scroll_event(id event) {
    if (!event || !g_ime.window || !g_ime.ns_window)
        return false;

    auto event_window = objc_send<id>(event, sel_window());
    if (event_window != g_ime.ns_window)
        return false;

    bool has_precise_scrolling_deltas =
        objc_send<bool>(event, sel_has_precise_scrolling_deltas());
    double scrolling_delta_y = objc_send<double>(event, sel_scrolling_delta_y());
    auto phase = objc_send<unsigned long long>(event, sel_phase());
    auto momentum_phase = objc_send<unsigned long long>(event, sel_momentum_phase());
    bool scroll_tracking_changed = sync_scroll_tracking_state(phase, momentum_phase);
    float normalized_delta = macos_normalize_scroll_delta(
        scrolling_delta_y,
        has_precise_scrolling_deltas,
        scroll_line_height());
    float viewport_height_value = current_scroll_viewport_height();
    bool handled = false;
    if (normalized_delta != 0.0f) {
        handled = has_precise_scrolling_deltas
            ? dispatch_scroll_pixels(normalized_delta,
                                     viewport_height_value,
                                     "wheel-precise")
            : dispatch_scroll_lines(scrolling_delta_y,
                                    viewport_height_value,
                                    "wheel-line");
    }
    if (scroll_tracking_changed && normalized_delta == 0.0f && g_ime.request_repaint)
        g_ime.request_repaint();
    return handled
        || normalized_delta != 0.0f
        || scroll_tracking_changed
        || scroll_phase_active(phase)
        || scroll_phase_active(momentum_phase);
}

inline void install_local_scroll_monitor() {
    if (g_ime.scroll_monitor)
        return;

    auto event_class = static_cast<Class>(objc_getClass("NSEvent"));
    if (!event_class)
        return;

    // Intercept local scroll events before GLFW normalizes them so macOS can
    // preserve AppKit's precise-vs-line semantics.
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

inline void input_attach(GLFWwindow* window, void (*request_repaint)()) {
    g_images.request_repaint = request_repaint;
    g_ime.window = window;
    g_ime.request_repaint = request_repaint;
    g_ime.ns_window = window
        ? static_cast<id>(glfwGetCocoaWindow(window))
        : nullptr;
    g_ime.content_view = g_ime.ns_window
        ? objc_send<id>(g_ime.ns_window, sel_content_view())
        : nullptr;
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
    ::phenotype::detail::clear_input_debug_caret_presentation();
    restore_content_view_first_responder();
}

inline void input_detach() {
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
    g_ime.window = nullptr;
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

inline void renderer_init(GLFWwindow* window) {
    if (g_renderer.initialized) return;
    g_renderer.window = window;

    g_renderer.device = MTL::CreateSystemDefaultDevice();
    if (!g_renderer.device) {
        std::fprintf(stderr, "[metal] no device\n");
        return;
    }
    g_renderer.queue = g_renderer.device->newCommandQueue();

    void* nswin = glfwGetCocoaWindow(window);
    auto sel_cv = sel_registerName("contentView");
    auto sel_wl = sel_registerName("setWantsLayer:");
    auto sel_sl = sel_registerName("setLayer:");
    void* view = reinterpret_cast<void*(*)(void*, SEL)>(objc_msgSend)(nswin, sel_cv);
    reinterpret_cast<void(*)(void*, SEL, bool)>(objc_msgSend)(view, sel_wl, true);

    g_renderer.layer = CA::MetalLayer::layer()->retain();
    g_renderer.layer->setDevice(g_renderer.device);
    g_renderer.layer->setPixelFormat(MTL::PixelFormatBGRA8Unorm);
    g_renderer.layer->setFramebufferOnly(false);

    int fbw = 0;
    int fbh = 0;
    glfwGetFramebufferSize(window, &fbw, &fbh);
    sync_drawable_size(fbw, fbh);

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

    g_renderer.color_pipeline = create_pipeline(g_renderer.device, lib, "vs_color", "fs_color");
    g_renderer.image_pipeline = create_pipeline(g_renderer.device, lib, "vs_image", "fs_image");
    g_renderer.text_pipeline = create_pipeline(g_renderer.device, lib, "vs_text", "fs_text");
    lib->release();
    if (!g_renderer.color_pipeline || !g_renderer.image_pipeline || !g_renderer.text_pipeline) {
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
    glfwGetWindowSize(window, &winw, &winh);
    std::printf("[phenotype-native] Metal initialized (%dx%d)\n", winw, winh);
}

inline void renderer_flush(unsigned char const* buf, unsigned int len) {
    if (len == 0 || !g_renderer.initialized) return;

    NS::AutoreleasePool* pool = NS::AutoreleasePool::alloc()->init();
    float text_scale = current_backing_scale(g_renderer.window);
    float line_height_ratio = ::phenotype::detail::g_app.theme.line_height_ratio;

    double cr = 0.98;
    double cg = 0.98;
    double cb = 0.98;
    double ca = 1.0;
    (void)process_completed_images();
    auto decode_started = metrics::detail::now_ns();
    if (!decode_frame_commands(
            buf, len, line_height_ratio,
            g_renderer.scratch, g_renderer.hit_regions,
            cr, cg, cb, ca)) {
        pool->release();
        return;
    }
    metrics::inst::native_phase_duration.record(
        metrics::detail::now_ns() - decode_started,
        native_attrs("command_decode"));

    int fbw = 0;
    int fbh = 0;
    glfwGetFramebufferSize(g_renderer.window, &fbw, &fbh);
    if (fbw == 0 || fbh == 0) {
        pool->release();
        return;
    }

    sync_drawable_size(fbw, fbh);

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

    int winw = 0;
    int winh = 0;
    glfwGetWindowSize(g_renderer.window, &winw, &winh);
    float uniforms[4] = {
        static_cast<float>(winw),
        static_cast<float>(winh),
        0,
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
    auto const color_bytes = scratch.color_instances.size() * sizeof(ColorInstanceGPU);
    uint32_t color_count = static_cast<uint32_t>(scratch.color_instances.size());
    if (color_count > 0) {
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
        encoder->setRenderPipelineState(g_renderer.color_pipeline);
        encoder->setVertexBuffer(g_renderer.uniform_buf, 0, 0);
        encoder->setVertexBuffer(g_renderer.color_instances_buf, 0, 1);
        encoder->drawPrimitives(MTL::PrimitiveTypeTriangle,
                                NS::UInteger(0), 6, NS::UInteger(color_count));
    }

    auto const image_bytes = scratch.image_instances.size() * sizeof(ImageInstanceGPU);
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
        encoder->setRenderPipelineState(g_renderer.image_pipeline);
        encoder->setVertexBuffer(g_renderer.uniform_buf, 0, 0);
        encoder->setVertexBuffer(g_renderer.image_instances_buf, 0, 1);
        encoder->setFragmentTexture(g_renderer.image_atlas_texture, 0);
        encoder->setFragmentSamplerState(g_renderer.sampler, 0);
        encoder->drawPrimitives(
            MTL::PrimitiveTypeTriangle,
            NS::UInteger(0), 6, NS::UInteger(scratch.image_instances.size()));
    }

    auto const text_bytes = scratch.text_instances.size() * sizeof(TextInstanceGPU);
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

        encoder->setRenderPipelineState(g_renderer.text_pipeline);
        encoder->setVertexBuffer(g_renderer.uniform_buf, 0, 0);
        encoder->setVertexBuffer(g_renderer.text_instances_buf, 0, 1);
        encoder->setFragmentTexture(g_renderer.text_atlas_texture, 0);
        encoder->setFragmentSamplerState(g_renderer.sampler, 0);
        encoder->drawPrimitives(
            MTL::PrimitiveTypeTriangle,
            NS::UInteger(0), 6, NS::UInteger(scratch.text_instances.size()));
    }

    auto const overlay_color_bytes =
        scratch.overlay_color_instances.size() * sizeof(ColorInstanceGPU);
    uint32_t overlay_color_count =
        static_cast<uint32_t>(scratch.overlay_color_instances.size());
    if (overlay_color_count > 0) {
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

    encoder->endEncoding();
    g_renderer.last_render_width = fbw;
    g_renderer.last_render_height = fbh;
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

    pool->release();
}

inline void renderer_shutdown() {
    if (g_renderer.sampler) { g_renderer.sampler->release(); g_renderer.sampler = nullptr; }
    if (g_renderer.debug_capture_texture) { g_renderer.debug_capture_texture->release(); g_renderer.debug_capture_texture = nullptr; }
    if (g_renderer.frame_readback_buf) { g_renderer.frame_readback_buf->release(); g_renderer.frame_readback_buf = nullptr; }
    if (g_renderer.image_atlas_texture) { g_renderer.image_atlas_texture->release(); g_renderer.image_atlas_texture = nullptr; }
    if (g_renderer.text_atlas_texture) { g_renderer.text_atlas_texture->release(); g_renderer.text_atlas_texture = nullptr; }
    if (g_renderer.image_instances_buf) { g_renderer.image_instances_buf->release(); g_renderer.image_instances_buf = nullptr; }
    if (g_renderer.text_instances_buf) { g_renderer.text_instances_buf->release(); g_renderer.text_instances_buf = nullptr; }
    if (g_renderer.color_instances_buf) { g_renderer.color_instances_buf->release(); g_renderer.color_instances_buf = nullptr; }
    if (g_renderer.overlay_color_instances_buf) { g_renderer.overlay_color_instances_buf->release(); g_renderer.overlay_color_instances_buf = nullptr; }
    if (g_renderer.uniform_buf) { g_renderer.uniform_buf->release(); g_renderer.uniform_buf = nullptr; }
    if (g_renderer.image_pipeline) { g_renderer.image_pipeline->release(); g_renderer.image_pipeline = nullptr; }
    if (g_renderer.text_pipeline) { g_renderer.text_pipeline->release(); g_renderer.text_pipeline = nullptr; }
    if (g_renderer.color_pipeline) { g_renderer.color_pipeline->release(); g_renderer.color_pipeline = nullptr; }
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
    g_renderer.color_instances_capacity = 0;
    g_renderer.overlay_color_instances_capacity = 0;
    g_renderer.image_instances_capacity = 0;
    g_renderer.text_instances_capacity = 0;
    g_renderer.frame_readback_capacity = 0;
    g_renderer.drawable_width = 0;
    g_renderer.drawable_height = 0;
    g_renderer.debug_capture_width = 0;
    g_renderer.debug_capture_height = 0;
    g_renderer.last_render_width = 0;
    g_renderer.last_render_height = 0;
    g_renderer.last_frame_available = false;
    g_renderer.window = nullptr;
    g_renderer.initialized = false;
}

inline std::optional<unsigned int> renderer_hit_test(float x, float y,
                                                     float scroll_y) {
    float wy = y + scroll_y;
    for (int i = static_cast<int>(g_renderer.hit_regions.size()) - 1; i >= 0; --i) {
        auto const& hr = g_renderer.hit_regions[static_cast<std::size_t>(i)];
        if (x >= hr.x && x <= hr.x + hr.w && wy >= hr.y && wy <= hr.y + hr.h)
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
    if (!detail::rasterize_text_run(
            text.data(),
            static_cast<unsigned int>(text.size()),
            font_size,
            mono,
            line_height,
            scale,
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
    return {
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
    };
}

inline json::Object macos_renderer_runtime_json() {
    json::Object renderer;
    renderer.emplace("initialized", json::Value{g_renderer.initialized});
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
        "readback_texture_ready",
        json::Value{g_renderer.debug_capture_texture != nullptr});
    renderer.emplace(
        "readback_buffer_ready",
        json::Value{g_renderer.frame_readback_buf != nullptr});
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

inline json::Value macos_platform_runtime_details_json_with_reason(
        std::string_view artifact_reason) {
#ifdef __APPLE__
    json::Object runtime;
    runtime.emplace("renderer", json::Value{macos_renderer_runtime_json()});
    runtime.emplace("images", json::Value{macos_images_runtime_json()});
    runtime.emplace("text_input", json::Value{macos_text_input_runtime_json()});
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
            detail::text_measure,
            detail::text_build_atlas,
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
            nullptr,
            nullptr,
            detail::input_dismiss_transient,
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
    };
    return api;
#else
    static platform_api api{};
    return api;
#endif
}

} // namespace phenotype::native
#endif
